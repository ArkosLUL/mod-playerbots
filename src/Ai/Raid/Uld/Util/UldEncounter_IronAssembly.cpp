/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_IronAssembly.h"
#include "RaidObs.h"

#include "Creature.h"
#include "Group.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "Timer.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace EncounterHelpers;

const Position ULDUAR_IRON_ASSEMBLY_ANCHOR = Position(1587.18f, 121.02f, 427.27f);

namespace
{

// Spread slots are held rather than re-derived, for the same reason Vezax holds his: ranking the raid
// by guid every tick means one death renumbers everyone behind the corpse and the whole ring shuffles
// mid-fight.
struct IronAssemblyEncounterState
{
    RaidObs::ObsGuidMap<uint8> spreadSlots{"ironassembly.slot"};

    // Which members are up: bit 0 Steelbreaker, 1 Molgeim, 2 Brundir. Each death restores the
    // survivors to full and changes what every other node decides, and it is the only phase boundary
    // this fight has - in the snapshot stream it shows up as nothing more than a boss row that stops
    // appearing.
    RaidObs::ObsValue<uint8> membersAlive{"ironassembly.alive"};

    // Bare rather than an ObsValue: a scan timestamp is bookkeeping, not an assignment.
    uint32 hazardNoteMs = 0;
};

// One map per map-update thread, keyed by instance: a bot is only ever updated from its own map's
// thread, so this needs no lock. Trigger, action and multiplier each hold their own helper instance,
// so the state they must agree on is defined here and nowhere else.
thread_local std::unordered_map<uint32, IronAssemblyEncounterState> ironAssemblyStates;

Position IronAssemblyPositionAt(float bearing, float radius)
{
    return Position(ULDUAR_IRON_ASSEMBLY_ANCHOR.GetPositionX() + std::cos(bearing) * radius,
                    ULDUAR_IRON_ASSEMBLY_ANCHOR.GetPositionY() + std::sin(bearing) * radius,
                    ULDUAR_IRON_ASSEMBLY_ANCHOR.GetPositionZ());
}

bool IronAssemblyHasEitherAura(Unit const* unit, uint32 first, uint32 second)
{
    return unit && (unit->HasAura(first) || unit->HasAura(second));
}

// Ranged and healers take spread slots and stack points; tanks hold their own boss and melee ride
// whatever the raid is killing.
bool IronAssemblyTakesRaidSpot(PlayerbotAI* botAI, Player* member)
{
    return member && botAI && PlayerbotAI::IsRanged(member) && !botAI->IsTank(member);
}

bool IronAssemblyMemberCounts(Player* member, uint32 instanceId)
{
    return member && member->IsAlive() && member->GetMapId() == ULDUAR_MAP_ID &&
           member->GetInstanceId() == instanceId;
}

// Two radii, because they answer different questions: rad is the spell's own, clear is the line the
// strategy draws. A bot that died at 22 yd from a 20 yd blast is a different bug from one at 18.
void NoteIronAssemblyCircle(Map* map, uint32 spellId, Position const& origin, float radius, float clearance)
{
    char params[48];
    snprintf(params, sizeof(params), "\"rad\":%.1f,\"clear\":%.1f", radius, clearance);

    RaidObs::NoteHazard(map, spellId, origin, "circle", params,
                        ULDUAR_IRON_ASSEMBLY_HAZARD_NOTE_INTERVAL_MS);
}

// The phase latch, and the geometry of the three hazards nothing can sweep for. Paced per instance
// rather than per bot: NoteHazard has no change-latch of its own, so 25 bots reaching it once a tick
// would write 25 rows a tick.
void TickIronAssemblyObs(Player* bot, IronAssemblyTargets const& targets)
{
    if (!RaidObs::Active())
        return;

    IronAssemblyEncounterState& state = ironAssemblyStates[bot->GetInstanceId()];

    uint8 alive = 0;
    if (targets.steelbreaker)
        alive |= 1;
    if (targets.molgeim)
        alive |= 2;
    if (targets.brundir)
        alive |= 4;

    state.membersAlive = alive;

    if (state.hazardNoteMs &&
        GetMSTimeDiffToNow(state.hazardNoteMs) < ULDUAR_IRON_ASSEMBLY_HAZARD_NOTE_INTERVAL_MS)
    {
        return;
    }

    state.hazardNoteMs = getMSTime();

    Map* map = bot->GetMap();

    if (targets.brundir)
    {
        if (IronAssemblyOverloadActive(targets.brundir))
            NoteIronAssemblyCircle(map, SPELL_OVERLOAD_DAMAGE, targets.brundir->GetPosition(),
                                   ULDUAR_IRON_ASSEMBLY_OVERLOAD_RADIUS,
                                   ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE);

        // Read the damage id off whichever aura is actually up. Both Overload auras trigger one
        // damage spell but the two Tendrils auras do not, and a timeline row has to name one.
        uint32 tendrils = 0;
        if (targets.brundir->HasAura(SPELL_LIGHTNING_TENDRILS_25_MAN))
            tendrils = SPELL_LIGHTNING_TENDRILS_DAMAGE_25_MAN;
        else if (targets.brundir->HasAura(SPELL_LIGHTNING_TENDRILS_10_MAN))
            tendrils = SPELL_LIGHTNING_TENDRILS_DAMAGE_10_MAN;

        if (tendrils)
            NoteIronAssemblyCircle(map, tendrils, targets.brundir->GetPosition(),
                                   ULDUAR_IRON_ASSEMBLY_TENDRILS_RADIUS,
                                   ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE);
    }

    // Meltdown is centred on the carrier rather than a boss, so there is one circle per carrier and
    // usually none at all.
    Group* group = bot->GetGroup();
    if (!group)
        return;

    uint32 const instanceId = bot->GetInstanceId();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (IronAssemblyMemberCounts(member, instanceId) && IronAssemblyHasOverwhelmingPower(member))
            NoteIronAssemblyCircle(map, SPELL_MELTDOWN, member->GetPosition(),
                                   ULDUAR_IRON_ASSEMBLY_MELTDOWN_RADIUS,
                                   ULDUAR_IRON_ASSEMBLY_MELTDOWN_CLEARANCE);
    }
}

// Brundir first, then the other two. His is the only spot that prevents damage, so he keeps a tank
// at every roster size; the rest are filled in whatever order is left.
void GatherIronAssemblyTankOrder(IronAssemblyTargets const& targets, std::vector<Unit*>& bosses)
{
    if (targets.brundir)
        bosses.push_back(targets.brundir);
    if (targets.steelbreaker)
        bosses.push_back(targets.steelbreaker);
    if (targets.molgeim)
        bosses.push_back(targets.molgeim);
}

void GatherIronAssemblyTanks(PlayerbotAI* botAI, Player* bot, std::vector<Player*>& tanks)
{
    if (Player* mainTank = GetGroupMainTank(bot))
        tanks.push_back(mainTank);

    for (uint8 index = 0; index < 2; ++index)
        if (Player* assist = GetGroupAssistTank(bot, index))
            tanks.push_back(assist);
}

// Reports which of the two points it picked, so the caller does not have to ask
// IronAssemblyBrundirIsLast a second time - that is another sweep for all three members.
Position IronAssemblyStackPoint(PlayerbotAI* botAI, bool& brundirLast)
{
    // Once Brundir is alone the isolation has nothing left to protect, and holding the opening point
    // would leave casters 38 yd off him. Three yards the other side of the anchor puts the stack 25
    // yd from his spot: clear of Overload, inside caster range.
    brundirLast = IronAssemblyBrundirIsLast(botAI);
    if (brundirLast)
        return IronAssemblyPositionAt(ULDUAR_IRON_ASSEMBLY_BRUNDIR_BEARING,
                                      ULDUAR_IRON_ASSEMBLY_BRUNDIR_LAST_STACK_RADIUS);

    return IronAssemblyPositionAt(ULDUAR_IRON_ASSEMBLY_STACK_BEARING,
                                  ULDUAR_IRON_ASSEMBLY_STACK_RADIUS);
}

void EnsureIronAssemblySpreadSlot(PlayerbotAI* botAI, Player* bot)
{
    IronAssemblyEncounterState& state = ironAssemblyStates[bot->GetInstanceId()];

    Group* group = bot->GetGroup();
    if (!group)
    {
        state.spreadSlots[bot->GetGUID()] = 0;
        return;
    }

    uint32 const instanceId = bot->GetInstanceId();

    std::unordered_set<ObjectGuid> present;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (IronAssemblyMemberCounts(member, instanceId) && IronAssemblyTakesRaidSpot(botAI, member))
            present.insert(member->GetGUID());
    }

    for (auto itr = state.spreadSlots.begin(); itr != state.spreadSlots.end();)
        itr = present.count(itr->first) ? std::next(itr) : state.spreadSlots.erase(itr);

    if (state.spreadSlots.count(bot->GetGUID()))
        return;

    std::set<uint8> taken;
    for (auto const& assignment : state.spreadSlots)
        taken.insert(assignment.second);

    for (uint8 slot = 0; slot < ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS; ++slot)
    {
        if (!taken.count(slot))
        {
            state.spreadSlots[bot->GetGUID()] = slot;
            return;
        }
    }

    // More ranged than the ring holds. Overflow keeps the stack point, which is no worse than where
    // they would stand with no spread at all.
    state.spreadSlots[bot->GetGUID()] = ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS;
}

}  // namespace

uint8 IronAssemblyTargets::AliveCount() const
{
    return (steelbreaker ? 1 : 0) + (molgeim ? 1 : 0) + (brundir ? 1 : 0);
}

Unit* GetIronAssemblyMember(PlayerbotAI* botAI, uint32 entry)
{
    return GetFirstAliveUnitByEntry(botAI, entry);
}

void GatherIronAssemblyTargets(PlayerbotAI* botAI, IronAssemblyTargets& targets)
{
    targets.steelbreaker = GetIronAssemblyMember(botAI, NPC_STEELBREAKER);
    targets.molgeim = GetIronAssemblyMember(botAI, NPC_MOLGEIM);
    targets.brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
}

bool IronAssemblyEncounterActive(PlayerbotAI* botAI)
{
    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);
    return targets.AliveCount() > 0;
}

bool IronAssemblyFormationActive(PlayerbotAI* botAI)
{
    if (!botAI)
        return false;

    // Room test first: it is two float compares against a fixed point, and it keeps every bot outside
    // the hall off the grid sweep the target gather costs.
    Player* bot = botAI->GetBot();
    if (!bot)
        return false;

    if (bot->GetExactDist2d(&ULDUAR_IRON_ASSEMBLY_ANCHOR) > ULDUAR_IRON_ASSEMBLY_ARENA_RADIUS)
        return false;

    if (std::fabs(bot->GetPositionZ() - ULDUAR_IRON_ASSEMBLY_ANCHOR.GetPositionZ()) >
        ULDUAR_IRON_ASSEMBLY_ARENA_HEIGHT)
        return false;

    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);

    bool const engaged = (targets.steelbreaker && targets.steelbreaker->IsInCombat()) ||
                         (targets.molgeim && targets.molgeim->IsInCombat()) ||
                         (targets.brundir && targets.brundir->IsInCombat());

    // Driven from the gate rather than a tick of its own, the way Algalon drives his: this already
    // runs every tick for every bot in the hall and for nobody outside it, which is the population
    // the probes want.
    if (engaged)
        TickIronAssemblyObs(bot, targets);

    return engaged;
}

bool IronAssemblyBrundirIsLast(PlayerbotAI* botAI)
{
    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);
    return targets.brundir && !targets.steelbreaker && !targets.molgeim;
}

static Unit* DeriveIronAssemblyFocusTarget(PlayerbotAI* botAI, Player* bot, char const*& how)
{
    // A mark a human raid leader set outranks the built-in order. Bots do not set the skull on this
    // encounter, so anything on it came from a person, and Ulduar already takes that stance
    // elsewhere. Only a living council member counts, or a stale mark would strand the raid.
    if (Group* group = bot->GetGroup())
    {
        ObjectGuid const skull = group->GetTargetIcon(RtiTargetValue::skullIndex);
        if (skull)
        {
            Unit* marked = botAI->GetUnit(skull);
            if (marked && marked->IsAlive() &&
                (marked->GetEntry() == NPC_STEELBREAKER || marked->GetEntry() == NPC_MOLGEIM ||
                 marked->GetEntry() == NPC_BRUNDIR))
            {
                how = "skull";
                return marked;
            }
        }
    }

    how = "order";
    return GetIronAssemblyNextKillTarget(botAI);
}

Unit* IronAssemblyFocusTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;

    char const* how = "order";
    Unit* focus = DeriveIronAssemblyFocusTarget(botAI, bot, how);

    // Which member and on whose authority. The act stream says the dps-priority node ran; only this
    // says what it picked, and whether a person overrode the configured order to get it.
    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "ironassembly.focus",
                             focus ? RaidObs::DescribeAssignment(focus->GetGUID()) + " " + how : "none");
    }

    return focus;
}

static Unit* DeriveIronAssemblyAssignedBoss(PlayerbotAI* botAI, Player* bot, char const*& how)
{
    std::vector<Player*> tanks;
    GatherIronAssemblyTanks(botAI, bot, tanks);

    // One tank cannot split three bosses, and pretending otherwise parks the only tank 28 yd from
    // the raid with the kill target loose. Below two tanks the encounter keeps its hands off.
    if (tanks.size() < 2)
    {
        how = "none:onetank";
        return nullptr;
    }

    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);

    // The Overwhelming Power swap needs both partners on Steelbreaker, so his empowered phase is the
    // one case where two tanks share a member.
    if (IsSteelbreakerEmpowered(botAI) && targets.steelbreaker)
    {
        if (PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0))
        {
            how = "swap";
            return targets.steelbreaker;
        }

        how = "none:surplus";
        return nullptr;
    }

    std::vector<Unit*> bosses;
    GatherIronAssemblyTankOrder(targets, bosses);
    if (bosses.empty())
    {
        how = "none:nobosses";
        return nullptr;
    }

    size_t index = tanks.size();
    for (size_t i = 0; i < tanks.size(); ++i)
        if (tanks[i] == bot)
            index = i;

    if (index >= tanks.size())
    {
        how = "none:surplus";
        return nullptr;
    }

    // Two tanks against three members: the main tank isolates Brundir and the other holds whatever
    // the raid is killing. The third is left to the generic threat table, which is no loss - the
    // raid is ignoring it anyway, and towing it anywhere would only put it back in the stack.
    if (tanks.size() == 2 && bosses.size() > 2 && index == 1)
    {
        Unit* focus = IronAssemblyFocusTarget(botAI);
        if (focus && focus != targets.brundir)
        {
            how = "focus";
            return focus;
        }
    }

    if (index >= bosses.size())
    {
        how = "none:surplus";
        return nullptr;
    }

    Unit* boss = bosses[index];
    switch (boss->GetEntry())
    {
        case NPC_BRUNDIR:
            how = "brundir";
            break;
        case NPC_STEELBREAKER:
            how = "steelbreaker";
            break;
        case NPC_MOLGEIM:
            how = "molgeim";
            break;
        default:
            how = "member";
            break;
    }

    return boss;
}

Unit* IronAssemblyAssignedBoss(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !botAI->IsTank(bot))
        return nullptr;

    char const* how = "none:nobosses";
    Unit* boss = DeriveIronAssemblyAssignedBoss(botAI, bot, how);

    // Probed here rather than at the trigger, because the interesting answer is the nullptr: an
    // unassigned tank simply leaves the node out of the act stream, and the four branches that get
    // there are indistinguishable from outside. Tanks only - the rest would each file one
    // meaningless row.
    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "ironassembly.tank",
                             boss ? RaidObs::DescribeAssignment(boss->GetGUID()) + " " + how : how);
    }

    return boss;
}

bool TryGetIronAssemblyTankSpot(PlayerbotAI* botAI, Player* bot, Position& position)
{
    Unit* boss = IronAssemblyAssignedBoss(botAI, bot);
    if (!boss)
        return false;

    switch (boss->GetEntry())
    {
        case NPC_BRUNDIR:
            position = IronAssemblyPositionAt(ULDUAR_IRON_ASSEMBLY_BRUNDIR_BEARING,
                                              ULDUAR_IRON_ASSEMBLY_BRUNDIR_RADIUS);
            return true;
        case NPC_STEELBREAKER:
            position = IronAssemblyPositionAt(ULDUAR_IRON_ASSEMBLY_STEELBREAKER_BEARING,
                                              ULDUAR_IRON_ASSEMBLY_MELEE_BOSS_RADIUS);
            return true;
        case NPC_MOLGEIM:
            position = IronAssemblyPositionAt(ULDUAR_IRON_ASSEMBLY_MOLGEIM_BEARING,
                                              ULDUAR_IRON_ASSEMBLY_MELEE_BOSS_RADIUS);
            return true;
        default:
            return false;
    }
}

// Molgeim drops Rune of Death on a random member, so on a stacked raid it lands on the stack point
// itself. Without this the raid-position action keeps ordering everyone back into it while the escape
// action pulls them out, and the two cancel each other every tick: nobody travels, nobody parks, and
// because a moving bot holds no interrupt duty Lightning Whirl goes unanswered as well.
//
// Reads the stack point and the runes and nothing else - never the calling bot. Every bot derives
// this on its own, so an answer that depended on where the caller stood would scatter the raid
// instead of moving it.
static Position DisplaceIronAssemblyStackPointOffRunes(Player* bot, Position const& stack, bool& displaced)
{
    displaced = false;

    std::vector<Position> swept;
    GatherIronAssemblyRunesOfDeath(bot, swept);

    // Kept against the stack rather than the caller: the sweep is centred on whoever asked, so two
    // bots standing apart would otherwise weigh different runes. It cannot conjure one a distant bot
    // never swept, but with the raid stacked around this point they all sweep the same ground.
    std::vector<Position> runes;
    for (Position const& rune : swept)
        if (rune.GetExactDist2d(stack.GetPositionX(), stack.GetPositionY()) <
            ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_SEARCH_RADIUS)
            runes.push_back(rune);

    if (IsIronAssemblyPositionClearOfRunes(stack, runes, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE))
        return stack;

    // Nearest clear heading, so the raid gives up as little range on the bosses as it can. Both the
    // ring and a dropped rune hold still, so the winner does not change while the rune lasts and the
    // raid parks instead of drifting between candidates.
    Position best;
    float bestTravel = 0.0f;
    for (uint8 heading = 0; heading < ULDUAR_IRON_ASSEMBLY_RUNE_SHIFT_HEADINGS; ++heading)
    {
        float const bearing = 2.0f * static_cast<float>(M_PI) * static_cast<float>(heading) /
                              static_cast<float>(ULDUAR_IRON_ASSEMBLY_RUNE_SHIFT_HEADINGS);
        Position const candidate = IronAssemblyPositionAt(bearing, ULDUAR_IRON_ASSEMBLY_RUNE_SHIFT_RADIUS);

        if (!IsIronAssemblyPositionClearOfRunes(candidate, runes,
                                                ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE))
            continue;

        float const travel = stack.GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY());
        if (!displaced || travel < bestTravel)
        {
            best = candidate;
            bestTravel = travel;
            displaced = true;
        }
    }

    // Every heading covered: hold formation and let the escape action walk each bot out on its own.
    // Scattering the raid to chase a spot that does not exist costs more than the ticks do.
    return displaced ? best : stack;
}

static bool DeriveIronAssemblyRaidSpot(PlayerbotAI* botAI, Player* bot, Position& position, char const*& how)
{
    bool brundirLast = false;
    Position const opening = IronAssemblyStackPoint(botAI, brundirLast);

    bool displaced = false;
    Position const stack = DisplaceIronAssemblyStackPointOffRunes(bot, opening, displaced);

    // Static Disruption is the only reason to spread and it does not exist before Steelbreaker's
    // phase 2, which the normal kill order never reaches. Everywhere else the raid stacks, which is
    // what keeps healers in range and makes Rune of Power worth soaking.
    if (!IsSteelbreakerEmpowered(botAI))
    {
        if (displaced)
            how = brundirLast ? "stack-late-rune" : "stack-rune";
        else
            how = brundirLast ? "stack-late" : "stack";

        position = stack;
        return true;
    }

    EnsureIronAssemblySpreadSlot(botAI, bot);

    IronAssemblyEncounterState const& state = ironAssemblyStates[bot->GetInstanceId()];
    auto const assignment = state.spreadSlots.find(bot->GetGUID());
    if (assignment == state.spreadSlots.end() || assignment->second >= ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS)
    {
        how = displaced ? "overflow-rune" : "overflow";
        position = stack;
        return true;
    }

    float const bearing = 2.0f * static_cast<float>(M_PI) * static_cast<float>(assignment->second) /
                          static_cast<float>(ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS);

    // The ring rides the displaced centre, so a rune moves the whole formation rather than leaving
    // half the slots inside it.
    how = displaced ? "spread-rune" : "spread";
    position =
        Position(stack.GetPositionX() + std::cos(bearing) * ULDUAR_IRON_ASSEMBLY_SPREAD_RING_RADIUS,
                 stack.GetPositionY() + std::sin(bearing) * ULDUAR_IRON_ASSEMBLY_SPREAD_RING_RADIUS,
                 stack.GetPositionZ());
    return true;
}

bool TryGetIronAssemblyRaidSpot(PlayerbotAI* botAI, Player* bot, Position& position)
{
    if (!IronAssemblyTakesRaidSpot(botAI, bot))
        return false;

    char const* how = "none";
    bool const found = DeriveIronAssemblyRaidSpot(botAI, bot, position, how);

    // The branch, not the coordinate: the move record already carries where the bot was sent, and it
    // is which of the four rules produced it that a clumped or scattered raid comes down to. The
    // ring index is not repeated here - ironassembly.slot already holds it.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "ironassembly.spot", how);

    return found;
}

bool IronAssemblyOverloadActive(Unit* brundir)
{
    return IronAssemblyHasEitherAura(brundir, SPELL_OVERLOAD_10_MAN, SPELL_OVERLOAD_25_MAN);
}

bool IronAssemblyTendrilsActive(Unit* brundir)
{
    return IronAssemblyHasEitherAura(brundir, SPELL_LIGHTNING_TENDRILS_10_MAN,
                                     SPELL_LIGHTNING_TENDRILS_25_MAN);
}

bool IronAssemblyLightningWhirlActive(Unit* brundir)
{
    return IronAssemblyHasEitherAura(brundir, SPELL_LIGHTNING_WHIRL_10_MAN, SPELL_LIGHTNING_WHIRL_25_MAN);
}

bool IronAssemblyChainLightningCasting(Unit* brundir)
{
    if (!brundir || !brundir->HasUnitState(UNIT_STATE_CASTING))
        return false;

    return brundir->FindCurrentSpellBySpellId(SPELL_CHAIN_LIGHTNING_10_MAN) ||
           brundir->FindCurrentSpellBySpellId(SPELL_CHAIN_LIGHTNING_25_MAN);
}

void GatherIronAssemblyRunesOfDeath(Player* bot, std::vector<Position>& runes)
{
    if (!bot)
        return;

    for (uint32 spellId : {SPELL_RUNE_OF_DEATH_10_MAN, SPELL_RUNE_OF_DEATH_25_MAN})
    {
        std::vector<Position> const found =
            GetDynamicObjectPositions(bot, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_SEARCH_RADIUS, spellId);
        runes.insert(runes.end(), found.begin(), found.end());
    }
}

bool IsIronAssemblyPositionClearOfRunes(Position const& spot, std::vector<Position> const& runes,
                                        float radius)
{
    for (Position const& rune : runes)
        if (rune.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < radius)
            return false;

    return true;
}

Unit* IronAssemblyRuneOfPowerCarrier(PlayerbotAI* botAI)
{
    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);

    for (Unit* member : {targets.steelbreaker, targets.molgeim, targets.brundir})
        if (member && member->HasAura(SPELL_RUNE_OF_POWER))
            return member;

    return nullptr;
}

static Unit* DeriveIronAssemblyRuneOfPowerSoakSpot(PlayerbotAI* botAI, Player* bot, Position& position,
                                                   char const*& how)
{
    Unit* carrier = IronAssemblyRuneOfPowerCarrier(botAI);
    if (!carrier)
    {
        how = "none:nocarrier";
        return nullptr;
    }

    // The rune sits under whichever member is standing in it, and that member's tank is already
    // walking him off it - so aim at his feet and accept that the spot expires with the pull-out.
    Position const rune = carrier->GetPosition();

    // Without the cap, a rune that lands on Brundir drags the whole ranged group into Overload.
    if (bot->GetExactDist2d(rune.GetPositionX(), rune.GetPositionY()) >
        ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_SOAK_MAX_TRAVEL)
    {
        how = "none:far";
        return nullptr;
    }

    std::vector<Position> runes;
    GatherIronAssemblyRunesOfDeath(bot, runes);
    if (!IsIronAssemblyPositionClearOfRunes(rune, runes, ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_DANGER_RADIUS))
    {
        how = "none:rune";
        return nullptr;
    }

    position = rune;
    return carrier;
}

bool TryGetIronAssemblyRuneOfPowerSoakSpot(PlayerbotAI* botAI, Player* bot, Position& position)
{
    if (!IronAssemblyTakesRaidSpot(botAI, bot))
        return false;

    char const* how = "none:nocarrier";
    Unit* carrier = DeriveIronAssemblyRuneOfPowerSoakSpot(botAI, bot, position, how);

    // Which member's feet the bot is walking to, or which of the two caps stopped it. Both refusals
    // look the same from outside: the node's trigger comes back false and nothing is written.
    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "ironassembly.soak",
                             carrier ? RaidObs::DescribeAssignment(carrier->GetGUID()) : how);
    }

    return carrier != nullptr;
}

bool IronAssemblyMemberMustMove(PlayerbotAI* botAI, Player* member)
{
    if (!botAI || !member)
        return false;

    if (Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR))
    {
        float const distance = member->GetDistance2d(brundir);

        if (IronAssemblyTendrilsActive(brundir) && distance < ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE)
            return true;

        // Tanks hold through Overload on purpose, so they never count as committed to a walk.
        if (IronAssemblyOverloadActive(brundir) && !botAI->IsTank(member) &&
            distance < ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE)
        {
            return true;
        }
    }

    std::vector<Position> runes;
    GatherIronAssemblyRunesOfDeath(member, runes);

    return !IsIronAssemblyPositionClearOfRunes(member->GetPosition(), runes,
                                               ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_DANGER_RADIUS);
}

char const* IronAssemblyReadyInterrupt(Player* bot, Unit* target)
{
    if (!bot || !target)
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    // Silences are absent because Brundir is immune to MECHANIC_SILENCE, which rules out silencing
    // shot and spell lock. Stuns are present because he is immune to neither STUN nor INTERRUPT -
    // the encounter's own achievement is built on stunning him.
    static char const* const interrupts[] = {"kick",         "pummel",
                                             "shield bash",  "counterspell",
                                             "wind shear",   "mind freeze",
                                             "hammer of justice", "bash",
                                             "kidney shot",  "shockwave"};

    for (char const* interrupt : interrupts)
        if (botAI->CanCastSpell(interrupt, target))
            return interrupt;

    return nullptr;
}

// Nothing is stored: every bot reaches the same ranking from the same facts, which is what stops two
// of them both standing down. `how` is only meaningful once the bot has an interrupt to offer - see
// the probe in the wrapper.
static char const* DeriveIronAssemblyInterruptDuty(PlayerbotAI* botAI, Player* bot, Unit* brundir,
                                                   char const*& how)
{
    bool const whirl = IronAssemblyLightningWhirlActive(brundir);
    bool const chainLightning = !whirl && IronAssemblyChainLightningCasting(brundir);
    if (!whirl && !chainLightning)
    {
        how = nullptr;
        return nullptr;
    }

    if (!IronAssemblyReadyInterrupt(bot, brundir))
    {
        how = nullptr;
        return nullptr;
    }

    // A bot already walking out of a hazard cannot cast, so it must not hold a duty either.
    if (IronAssemblyMemberMustMove(botAI, bot))
    {
        how = "none:moving";
        return nullptr;
    }

    uint8 rank = 0;
    if (Group* group = bot->GetGroup())
    {
        uint32 const instanceId = bot->GetInstanceId();

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member == bot || !IronAssemblyMemberCounts(member, instanceId))
                continue;

            if (member->GetGUID() < bot->GetGUID() && IronAssemblyReadyInterrupt(member, brundir) &&
                !IronAssemblyMemberMustMove(botAI, member))
            {
                ++rank;
            }
        }
    }

    // Rank 0 owns Lightning Whirl, which is 100 yd and has no positional answer at all. Rank 1 takes
    // Chain Lightning, so when only one interrupt is off cooldown Chain Lightning is deliberately
    // allowed through rather than spending the cooldown that the next Whirl needs.
    if (whirl && rank == 0)
    {
        how = "whirl";
        return how;
    }

    if (chainLightning && rank == 1)
    {
        how = "chain";
        return how;
    }

    how = "standby";
    return nullptr;
}

char const* IronAssemblyInterruptDuty(PlayerbotAI* botAI, Player* bot, Unit* brundir)
{
    if (!botAI || !bot || !brundir)
        return nullptr;

    char const* how = nullptr;
    char const* duty = DeriveIronAssemblyInterruptDuty(botAI, bot, brundir, how);

    // A role, not an event, so it is latched and left: the act stream already reports every kick
    // that fired. `how` stays null for a bot with no cast to answer or no interrupt to answer it
    // with, and those are deliberately unprobed - flapping the whole raid back to idle between casts
    // would bury the handful of rows that say who was actually on the hook.
    if (how && RaidObs::Active())
        RaidObs::NoteDerived(bot, "ironassembly.interrupt", how);

    return duty;
}

bool IronAssemblyShieldOfRunesUp(Unit* molgeim)
{
    return IronAssemblyHasEitherAura(molgeim, SPELL_SHIELD_OF_RUNES_10_MAN, SPELL_SHIELD_OF_RUNES_25_MAN);
}

bool IronAssemblyHasFusionPunch(Unit* unit)
{
    return IronAssemblyHasEitherAura(unit, SPELL_FUSION_PUNCH_10_MAN, SPELL_FUSION_PUNCH_25_MAN);
}

bool IronAssemblyHasOverwhelmingPower(Unit* unit)
{
    return IronAssemblyHasEitherAura(unit, SPELL_OVERWHELMING_POWER_10_MAN,
                                     SPELL_OVERWHELMING_POWER_25_MAN);
}

bool IronAssemblyEncounterStateIsStale(PlayerbotAI* botAI)
{
    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);

    // All three up and untouched is the pull, not a mid-fight lull. Testing combat instead would wipe
    // the state every Lightning Tendrils, when Brundir spends 16s with no victim by design - the core
    // script carries the same warning about its own reset path.
    if (targets.AliveCount() < 3)
        return false;

    for (Unit* member : {targets.steelbreaker, targets.molgeim, targets.brundir})
        if (member->GetHealth() < member->GetMaxHealth())
            return false;

    return true;
}

bool IronAssemblyBotHasEncounterState(Player* bot)
{
    if (!bot)
        return false;

    auto const state = ironAssemblyStates.find(bot->GetInstanceId());
    return state != ironAssemblyStates.end() && state->second.spreadSlots.count(bot->GetGUID()) > 0;
}

void ResetIronAssemblyEncounterState(Player* bot, bool clearInstance)
{
    if (!bot)
        return;

    if (clearInstance)
    {
        ironAssemblyStates.erase(bot->GetInstanceId());
        return;
    }

    auto const state = ironAssemblyStates.find(bot->GetInstanceId());
    if (state == ironAssemblyStates.end())
        return;

    state->second.spreadSlots.erase(bot->GetGUID());
}
