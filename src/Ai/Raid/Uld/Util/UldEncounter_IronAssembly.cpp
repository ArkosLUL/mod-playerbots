/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_IronAssembly.h"
#include "RaidObs.h"

#include "Creature.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

#include <cmath>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace EncounterHelpers;

namespace
{

// Spread slots are held rather than re-derived, for the same reason Vezax holds his: ranking the raid
// by guid every tick means one death renumbers everyone behind the corpse and the whole ring shuffles
// mid-fight.
struct IronAssemblyEncounterState
{
    RaidObs::ObsGuidMap<uint8> spreadSlots{"ironassembly.slot"};
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

Position IronAssemblyStackPoint(PlayerbotAI* botAI)
{
    // Once Brundir is alone the isolation has nothing left to protect, and holding the opening point
    // would leave casters 38 yd off him. Three yards the other side of the anchor puts the stack 25
    // yd from his spot: clear of Overload, inside caster range.
    if (IronAssemblyBrundirIsLast(botAI))
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

    return (targets.steelbreaker && targets.steelbreaker->IsInCombat()) ||
           (targets.molgeim && targets.molgeim->IsInCombat()) ||
           (targets.brundir && targets.brundir->IsInCombat());
}

bool IronAssemblyBrundirIsLast(PlayerbotAI* botAI)
{
    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);
    return targets.brundir && !targets.steelbreaker && !targets.molgeim;
}

Unit* IronAssemblyFocusTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot)
        return nullptr;

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
                return marked;
            }
        }
    }

    return GetIronAssemblyNextKillTarget(botAI);
}

Unit* IronAssemblyAssignedBoss(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !botAI->IsTank(bot))
        return nullptr;

    std::vector<Player*> tanks;
    GatherIronAssemblyTanks(botAI, bot, tanks);

    // One tank cannot split three bosses, and pretending otherwise parks the only tank 28 yd from
    // the raid with the kill target loose. Below two tanks the encounter keeps its hands off.
    if (tanks.size() < 2)
        return nullptr;

    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);

    // The Overwhelming Power swap needs both partners on Steelbreaker, so his empowered phase is the
    // one case where two tanks share a member.
    if (IsSteelbreakerEmpowered(botAI) && targets.steelbreaker)
    {
        if (PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0))
            return targets.steelbreaker;

        return nullptr;
    }

    std::vector<Unit*> bosses;
    GatherIronAssemblyTankOrder(targets, bosses);
    if (bosses.empty())
        return nullptr;

    size_t index = tanks.size();
    for (size_t i = 0; i < tanks.size(); ++i)
        if (tanks[i] == bot)
            index = i;

    if (index >= tanks.size())
        return nullptr;

    // Two tanks against three members: the main tank isolates Brundir and the other holds whatever
    // the raid is killing. The third is left to the generic threat table, which is no loss - the
    // raid is ignoring it anyway, and towing it anywhere would only put it back in the stack.
    if (tanks.size() == 2 && bosses.size() > 2 && index == 1)
    {
        Unit* focus = IronAssemblyFocusTarget(botAI);
        if (focus && focus != targets.brundir)
            return focus;
    }

    if (index >= bosses.size())
        return nullptr;

    return bosses[index];
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

bool TryGetIronAssemblyRaidSpot(PlayerbotAI* botAI, Player* bot, Position& position)
{
    if (!IronAssemblyTakesRaidSpot(botAI, bot))
        return false;

    Position const stack = IronAssemblyStackPoint(botAI);

    // Static Disruption is the only reason to spread and it does not exist before Steelbreaker's
    // phase 2, which the normal kill order never reaches. Everywhere else the raid stacks, which is
    // what keeps healers in range and makes Rune of Power worth soaking.
    if (!IsSteelbreakerEmpowered(botAI))
    {
        position = stack;
        return true;
    }

    EnsureIronAssemblySpreadSlot(botAI, bot);

    IronAssemblyEncounterState const& state = ironAssemblyStates[bot->GetInstanceId()];
    auto const assignment = state.spreadSlots.find(bot->GetGUID());
    if (assignment == state.spreadSlots.end() || assignment->second >= ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS)
    {
        position = stack;
        return true;
    }

    float const bearing = 2.0f * static_cast<float>(M_PI) * static_cast<float>(assignment->second) /
                          static_cast<float>(ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS);

    position =
        Position(stack.GetPositionX() + std::cos(bearing) * ULDUAR_IRON_ASSEMBLY_SPREAD_RING_RADIUS,
                 stack.GetPositionY() + std::sin(bearing) * ULDUAR_IRON_ASSEMBLY_SPREAD_RING_RADIUS,
                 stack.GetPositionZ());
    return true;
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

bool IsIronAssemblyPositionClearOfRunes(Position const& spot, std::vector<Position> const& runes)
{
    for (Position const& rune : runes)
    {
        if (rune.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <
            ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE)
        {
            return false;
        }
    }

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

bool TryGetIronAssemblyRuneOfPowerSoakSpot(PlayerbotAI* botAI, Player* bot, Position& position)
{
    if (!IronAssemblyTakesRaidSpot(botAI, bot))
        return false;

    Unit* carrier = IronAssemblyRuneOfPowerCarrier(botAI);
    if (!carrier)
        return false;

    // The rune sits under whichever member is standing in it, and that member's tank is already
    // walking him off it - so aim at his feet and accept that the spot expires with the pull-out.
    Position const rune = carrier->GetPosition();

    // Without the cap, a rune that lands on Brundir drags the whole ranged group into Overload.
    if (bot->GetExactDist2d(rune.GetPositionX(), rune.GetPositionY()) >
        ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_SOAK_MAX_TRAVEL)
    {
        return false;
    }

    std::vector<Position> runes;
    GatherIronAssemblyRunesOfDeath(bot, runes);
    if (!IsIronAssemblyPositionClearOfRunes(rune, runes))
        return false;

    position = rune;
    return true;
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

    return !IsIronAssemblyPositionClearOfRunes(member->GetPosition(), runes);
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

bool IronAssemblyInterruptRank(PlayerbotAI* botAI, Player* bot, Unit* brundir, uint8& rank)
{
    if (!botAI || !bot || !brundir || !IronAssemblyReadyInterrupt(bot, brundir))
        return false;

    // A bot already walking out of a hazard cannot cast, so it must not hold a duty either.
    if (IronAssemblyMemberMustMove(botAI, bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
    {
        rank = 0;
        return true;
    }

    uint32 const instanceId = bot->GetInstanceId();

    uint8 ahead = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member == bot || !IronAssemblyMemberCounts(member, instanceId))
            continue;

        if (member->GetGUID() < bot->GetGUID() && IronAssemblyReadyInterrupt(member, brundir) &&
            !IronAssemblyMemberMustMove(botAI, member))
        {
            ++ahead;
        }
    }

    rank = ahead;
    return true;
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
