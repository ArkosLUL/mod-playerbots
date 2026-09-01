/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Hodir.h"

#include "BossAuraTriggers.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "SpellMgr.h"
#include "UldScripts.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <vector>

using namespace EncounterHelpers;

// Hodir's corner. His room runs x 1965-2041 and y -170 to -298, and he evades the moment he leaves
// that y band. Tanking him in the south-west corner collapses the helper NPCs' 17-30 yd stand-off
// arc into one place, which is what makes the Starlight zone and the Toasty Fires land somewhere the
// raid can predict. That corner is chamfered, not square - the floor bevels away from about
// (1966, -274) to (1990, -298) - so the tank spot is the deepest point with 6 yd of floor all round
// rather than the visual corner, which has three yards of nothing behind it. The off-tank sits
// further into the corner rather than toward the raid, so a taunt never walks him at the stack. The
// raid anchor is only the fallback centre for the ranged ring; normally the ring rides Starlight.
const Position ULDUAR_HODIR_MAINTANK_SPOT = Position(1974.50f, -275.50f, 432.687f);
const Position ULDUAR_HODIR_OFFTANK_SPOT = Position(1980.00f, -277.00f, 432.687f);
const Position ULDUAR_HODIR_RAID_ANCHOR = Position(1986.56f, -257.11f, 432.687f);

Unit* GetHodir(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_HODIR); }

bool IsHodirEngaged(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);
    return boss && boss->IsInCombat();
}

bool IsHodirFlashFreezeIncoming(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);

    // Every cast slot, not CURRENT_GENERIC_SPELL: which slot a scripted boss cast lands in is the
    // script's business, and guessing wrong here silently opens the window on nothing.
    return boss && boss->HasUnitState(UNIT_STATE_CASTING) &&
           boss->FindCurrentSpellBySpellId(SPELL_FLASH_FREEZE) != nullptr;
}

bool HodirFrozenBlowsActive(PlayerbotAI* botAI, Player* bot)
{
    Unit* boss = GetHodir(botAI);
    return boss && bot && boss->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_FROZEN_BLOWS, bot));
}

bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || bot->GetHealthPct() >= ULDUAR_HODIR_TAUNT_HEALTH_FLOOR)
        return false;

    if (!HodirFrozenBlowsActive(botAI, bot))
        return false;

    // Somebody else has to already be holding him. With nobody on him the taunt is the rescue and has
    // to survive whatever shape the bot is in, and a taunt at a creature victim is how the boss comes
    // off a pet.
    Unit* boss = GetHodir(botAI);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    return victim && victim != bot && victim->IsPlayer() && victim->IsAlive();
}

Creature* GetHodirSharedShelter(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !GetHodir(botAI))
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_SNOWPACKED_ICICLE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    // Before the centre, not after: a shelter only exists for about 6s of every 49s cycle, and the
    // centre costs a second grid sweep to find the fire.
    if (found.empty())
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "hodir.shelter", "none");

        return nullptr;
    }

    // Nearest the ring centre rather than nearest the bot, so the whole raid converges on one drift
    // and re-forms cleanly instead of splitting across the two or three that spawn. Trigger and
    // action both call this: two derivations of "which shelter" would disagree and oscillate.
    //
    // The centre, not ULDUAR_HODIR_RAID_ANCHOR: the ring rides a Toasty Fire now and sits a median
    // 9.5 yd off that fixed point, p90 17.6. Measuring from a spot the raid is not standing on was
    // picking a drift 25 yd away with three other candidates on the floor.
    Position const centre = GetHodirRingCentre(botAI, bot);

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* shelter : found)
    {
        if (!shelter || !shelter->IsAlive())
            continue;

        float const dist = shelter->GetExactDist2d(&centre);
        if (!best || dist < bestDist)
        {
            best = shelter;
            bestDist = dist;
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shelter",
                             best ? RaidObs::DescribeAssignment(best->GetGUID()) : "none");

    return best;
}

Creature* GetHodirRaidFire(PlayerbotAI* botAI, Player* bot)
{
    Unit* hodir = bot ? GetHodir(botAI) : nullptr;
    if (!hodir)
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_TOASTY_FIRE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* fire : found)
    {
        if (!fire || !fire->IsAlive())
            continue;

        // Measured from Hodir, not from the fixed anchor: he drifts and the anchor does not, so a fire
        // picked off the anchor put the far side of the ring 45 yd from him and the casters walked a
        // reach spell back in. A fire the boss is standing on is no good either, however close.
        float const gap = fire->GetExactDist2d(hodir);
        if (gap < ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP)
            continue;

        // The whole ring has to reach him from it, not just the centre.
        if (gap + ULDUAR_HODIR_RAID_RING_OUTER + ULDUAR_HODIR_RING_SPOT_TOLERANCE >
            ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
            continue;

        if (!best || gap < bestDist)
        {
            best = fire;
            bestDist = gap;
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.fire",
                             best ? RaidObs::DescribeAssignment(best->GetGUID()) : "none");

    return best;
}

Position GetHodirRingCentre(PlayerbotAI* botAI, Player* bot, bool* onFire)
{
    Position centre = ULDUAR_HODIR_RAID_ANCHOR;

    // Unquantised. The fire does not move, so there is nothing for a quantum to smooth out, and
    // rounding would only push the centre off the one point the whole ring is sized around.
    Creature* fire = bot ? GetHodirRaidFire(botAI, bot) : nullptr;
    if (fire)
        centre = Position(fire->GetPositionX(), fire->GetPositionY(),
                          ULDUAR_HODIR_RAID_ANCHOR.GetPositionZ());

    // Reported rather than re-derived: the fire sweep is the expensive half of this call, and a
    // centre that happens to sit near a fire is not the same as a centre that is one.
    if (onFire)
        *onFire = fire != nullptr;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.centre", RaidObs::DescribeDerived(centre));

    return centre;
}

// The ring roster, in the order every bot derives identically. The dead keep their slots: indexing by
// the living instead shifts every bot after the corpse, so one death re-seats the whole formation and
// the raid walks the layout again mid-fight - exactly when it can least afford to. A vacant slot
// costs nothing; someone zoned out is a different case, because they are not coming back to it.
static bool BuildHodirRingMembers(Player* bot, std::vector<Player*>& out)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    out.clear();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !memberAI->IsRanged(member) || memberAI->IsTank(member))
            continue;

        out.push_back(member);
    }

    if (out.empty())
        return false;

    // Ranged dps ahead of healers, then guid. Both keys are identical on every bot, so nobody has to
    // be told which slot is theirs.
    std::sort(out.begin(), out.end(), [](Player* left, Player* right)
    {
        bool const leftDps = GET_PLAYERBOT_AI(left)->IsRangedDps(left);
        bool const rightDps = GET_PLAYERBOT_AI(right)->IsRangedDps(right);
        if (leftDps != rightDps)
            return leftDps;
        return left->GetGUID() < right->GetGUID();
    });

    return true;
}

// Raw ring geometry for one slot, no ground or collision pass. Cheap enough to run for the whole
// roster, which is what deciding who owns a Starlight zone needs.
static Position HodirRingSlotPoint(Position const& centre, size_t slot, size_t total)
{
    // Bearing between two fixed points, so the layout never rotates. Deriving it from the centre
    // instead would spin every slot each time the centre moved.
    float const baseAngle = std::atan2(ULDUAR_HODIR_MAINTANK_SPOT.GetPositionY() - ULDUAR_HODIR_RAID_ANCHOR.GetPositionY(),
                                       ULDUAR_HODIR_MAINTANK_SPOT.GetPositionX() - ULDUAR_HODIR_RAID_ANCHOR.GetPositionX());

    size_t const inner = std::min<size_t>(ULDUAR_HODIR_RAID_RING_INNER_SLOTS, total > 0 ? total - 1 : 0);

    float radius = 0.0f;
    float angle = baseAngle;

    if (!slot)
    {
        // Slot 0 stands on the centre itself - the safest spot in the fire and the only one that is
        // never within 4 yd of two neighbours at once.
        radius = 0.0f;
    }
    else if (slot <= inner)
    {
        radius = ULDUAR_HODIR_RAID_RING_INNER;
        angle = baseAngle + 2.0f * static_cast<float>(M_PI) * static_cast<float>(slot - 1) / static_cast<float>(inner);
    }
    else
    {
        size_t const outerCount = total - inner - 1;
        size_t const outerSlot = slot - inner - 1;
        radius = ULDUAR_HODIR_RAID_RING_OUTER;
        angle = baseAngle +
                2.0f * static_cast<float>(M_PI) * static_cast<float>(outerSlot) / static_cast<float>(outerCount);
    }

    angle = Position::NormalizeOrientation(angle);

    return Position(centre.GetPositionX() + std::cos(angle) * radius,
                    centre.GetPositionY() + std::sin(angle) * radius, centre.GetPositionZ());
}

bool GetHodirRingSlot(PlayerbotAI* botAI, Player* bot, Position const& centre, Position& out)
{
    std::vector<Player*> ringMembers;
    if (!BuildHodirRingMembers(bot, ringMembers))
        return false;

    size_t slot = ringMembers.size();
    for (size_t i = 0; i < ringMembers.size(); ++i)
        if (ringMembers[i] == bot)
            slot = i;

    if (slot >= ringMembers.size())
        return false;

    size_t const total = ringMembers.size();
    out = ValidateFloorPoint(bot, HodirRingSlotPoint(centre, slot, total));

    // The slot index and the size of the ring it was cut from, so a formation that re-seated is
    // readable without re-deriving the sort from the roster.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.slot",
                             std::to_string(slot) + "/" + std::to_string(total) + " " +
                                 RaidObs::DescribeDerived(out));

    return true;
}

// Where this bot stands if there is a Starlight zone it can use. Starlight is +50% to cast time and
// all three attack timers, the fight's biggest throughput lever, and it is worth stacking for: one
// Ice Shards hit is 41% of a health pool (p90 54%), so an icicle catching two bots in one zone costs
// two heals rather than two lives. Any number may share a zone; what each gets is a bearing of its
// own, taken from the slot it came from, so they spread around it instead of piling on a point.
//
// Usable means the resulting spot still reaches Hodir and is still out of his reach; the nearest zone
// to the slot wins among those, so the detour stays short.
static bool FindHodirStarlightStand(PlayerbotAI* botAI, Player* bot, Position const& centre,
                                    bool onFire, Position const& slot, Position& out)
{
    // Bounded rather than the room radius: this runs per bot per tick, and a zone further out than
    // this cannot be within reach of any slot the bot could be standing on anyway.
    std::vector<Position> const zones =
        GetDynamicObjectPositions(bot, ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT);
    if (zones.empty())
        return false;

    // The druid casts where it is standing, which is usually next to Hodir, so a good share of the
    // zones on the floor are ones no caster can use.
    Unit* hodir = GetHodir(botAI);

    // Staying inside the fire is not optional while there is one: a bot outside it starts shedding
    // Biting Cold, and that node outranks this one, so it would shuttle straight back out of the zone
    // it just walked to. Off a fire there is nothing to stay inside - the bot is shedding wherever it
    // stands - and applying the leash anyway drew a 10 yd box round the anchor that rejected 89% of
    // the zones on the floor, which is why only 1.40 dps of 18 were ever standing in one.
    float const fireLeash = ULDUAR_HODIR_TOASTY_FIRE_RADIUS - ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE;

    bool found = false;
    float bestWalk = 0.0f;

    for (Position const& zone : zones)
    {
        float const walk = slot.GetExactDist2d(&zone);
        if (found && walk >= bestWalk)
            continue;

        float const bearing = std::atan2(slot.GetPositionY() - zone.GetPositionY(),
                                         slot.GetPositionX() - zone.GetPositionX());

        Position const stand = ValidateFloorPoint(
            bot, Position(zone.GetPositionX() + std::cos(bearing) * ULDUAR_HODIR_STARLIGHT_STAND_RADIUS,
                          zone.GetPositionY() + std::sin(bearing) * ULDUAR_HODIR_STARLIGHT_STAND_RADIUS,
                          zone.GetPositionZ()));

        if (onFire && centre.GetExactDist2d(&stand) > fireLeash)
            continue;

        // Both ends of the caster band. A zone the bot cannot shoot the boss from is not a throughput
        // lever, whatever haste it carries.
        if (hodir)
        {
            float const gap = stand.GetExactDist2d(hodir);
            if (gap < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP || gap > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
                continue;
        }

        out = stand;
        bestWalk = walk;
        found = true;
    }

    return found;
}

bool GetHodirStarlightZoneAt(PlayerbotAI* /*botAI*/, Player* bot, Position& out)
{
    if (!bot)
        return false;

    std::vector<Position> const zones =
        GetDynamicObjectPositions(bot, ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT);

    bool found = false;
    float bestDist = 0.0f;
    for (Position const& zone : zones)
    {
        float const dist = bot->GetExactDist2d(&zone);
        if (dist > ULDUAR_HODIR_STARLIGHT_RADIUS)
            continue;

        if (!found || dist < bestDist)
        {
            out = zone;
            bestDist = dist;
            found = true;
        }
    }

    return found;
}

bool IsHodirIcicleLethal(Creature* icicle)
{
    if (!icicle || !icicle->IsAlive())
        return false;

    TempSummon* summon = icicle->ToTempSummon();
    uint32 const remaining = summon ? summon->GetTimer() : 0;

    // Both icicles are TEMPSUMMON_TIMED_DESPAWN, so GetTimer counts the 7000ms down. Anything else
    // leaves it parked at its start value and every icicle reads live, which is the safe way to be
    // wrong.
    return !remaining || remaining > ULDUAR_HODIR_ICICLE_SPENT_MS;
}

static bool DeriveHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out, char const*& how)
{
    if (!bot)
        return false;

    bool const mainTank = botAI->IsMainTank(bot);
    if (mainTank || botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        // Tanks shuttle between two fixed points instead of wandering, because Hodir follows. The
        // axis runs parallel to the SW bevel, so neither end walks him toward the chamfer, and 6 yd
        // apart is both long enough to cover two aura ticks and short enough to keep him cornered.
        Position const& spot = mainTank ? ULDUAR_HODIR_MAINTANK_SPOT : ULDUAR_HODIR_OFFTANK_SPOT;
        float const dx = std::cos(ULDUAR_HODIR_SHUTTLE_BEARING) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;
        float const dy = std::sin(ULDUAR_HODIR_SHUTTLE_BEARING) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;

        Position const legA(spot.GetPositionX() + dx, spot.GetPositionY() + dy, spot.GetPositionZ());
        Position const legB(spot.GetPositionX() - dx, spot.GetPositionY() - dy, spot.GetPositionZ());

        // Whichever end is further away, so the leg is always the full 6 yd and IsDuplicateMove
        // cannot refuse it for repeating the last destination.
        out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
        how = "tank";
        return true;
    }

    // A bot holding Starlight sheds across the zone rather than out of it. Same shape as the tank
    // shuttle and for the same reason: two opposite points through a centre, taking whichever end is
    // further so the leg is always the full length and IsDuplicateMove cannot refuse it for repeating
    // the last destination. Both ends sit inside the zone, so the aura survives the shuttle - which is
    // the whole point, because the shed outranks the anchor and would otherwise walk the bot out of
    // the biggest throughput buff in the fight to save 800 a tick.
    Position zone;
    if (GetHodirStarlightZoneAt(botAI, bot, zone))
    {
        float const bearing = std::atan2(bot->GetPositionY() - zone.GetPositionY(),
                                         bot->GetPositionX() - zone.GetPositionX());
        float const dx = std::cos(bearing) * ULDUAR_HODIR_STARLIGHT_SHED_RADIUS;
        float const dy = std::sin(bearing) * ULDUAR_HODIR_STARLIGHT_SHED_RADIUS;

        Position const legA = ValidateFloorPoint(
            bot, Position(zone.GetPositionX() + dx, zone.GetPositionY() + dy, zone.GetPositionZ()));
        Position const legB = ValidateFloorPoint(
            bot, Position(zone.GetPositionX() - dx, zone.GetPositionY() - dy, zone.GetPositionZ()));

        out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
        how = "starlight";
        return true;
    }

    // Everyone else steps to the nearest point clear of the rest of the raid. distanceStep is the
    // leg length, not a probe granularity: the helper rings outward from it, so 6 yd is the shortest
    // move it can return and a shorter one would not span two aura ticks.
    std::vector<Position> crowd;
    for (auto const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
    {
        Unit* ally = botAI->GetUnit(guid);
        if (ally && ally->IsAlive() && ally != bot && bot->GetExactDist2d(ally) <= ULDUAR_HODIR_DODGE_LEASH)
            crowd.push_back(ally->GetPosition());
    }

    if (crowd.empty())
    {
        float const bearing = static_cast<float>(bot->GetGUID().GetCounter() % 8) * static_cast<float>(M_PI) / 4.0f;
        out = Position(bot->GetPositionX() + std::cos(bearing) * 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG,
                       bot->GetPositionY() + std::sin(bearing) * 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG,
                       bot->GetPositionZ());
        how = "solo";
        return true;
    }

    Position leg = FindNearestPositionClearOfHazards(bot, crowd, ULDUAR_HODIR_DECLUMP_RADIUS,
                                                     ULDUAR_HODIR_DODGE_LEASH,
                                                     2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG);
    if (!leg.GetPositionX() && !leg.GetPositionY())
        leg = FindNearestPositionClearOfHazards(bot, crowd, ULDUAR_HODIR_SHUTTLE_HALF_LEG,
                                                ULDUAR_HODIR_DODGE_LEASH,
                                                2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG);

    if (!leg.GetPositionX() && !leg.GetPositionY())
        return false;

    out = leg;
    how = "crowd";
    return true;
}

bool GetHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out)
{
    char const* how = "none";
    bool const found = DeriveHodirShuttleLeg(botAI, bot, out, how);

    // The rule, not the leg. The leg is already a move record with this action's name on it, and a
    // crowd-derived one moves every tick, so latching the coordinate emitted on every tick too.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shuttle", how);

    return found;
}

static bool DeriveHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    if (!GetHodir(botAI))
        return false;

    if (botAI->IsMainTank(bot))
    {
        out = ULDUAR_HODIR_MAINTANK_SPOT;
        tolerance = ULDUAR_HODIR_MAINTANK_SPOT_TOLERANCE;
        return true;
    }

    if (botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        out = ULDUAR_HODIR_OFFTANK_SPOT;
        tolerance = ULDUAR_HODIR_MAINTANK_SPOT_TOLERANCE;
        return true;
    }

    // Melee ride the boss in the corner. Pinning them would cost uptime, and they are far outside
    // both buff zones there whatever we do.
    if (!botAI->IsRanged(bot))
        return false;

    // Derived once and passed down. The slot and the Starlight step both need it, and every call
    // sweeps the grid for the fire and writes a note.
    bool onFire = false;
    Position const centre = GetHodirRingCentre(botAI, bot, &onFire);
    if (!GetHodirRingSlot(botAI, bot, centre, out))
        return false;

    tolerance = ULDUAR_HODIR_RING_SPOT_TOLERANCE;

    // The step is here rather than in the position trigger so one place decides where the bot stands.
    // A trigger that fired on "no Starlight" while the action still walked to the slot would move the
    // bot forever without ever arriving, which is what the old raid-wide constraint did.
    //
    // Deliberately not gated on already holding the aura: that would hand the bot back its ring slot
    // the moment the buff landed, walk it out of the zone, and start the whole trip again. The anchor
    // stays on the zone until the zone expires.
    Position stand;
    if (FindHodirStarlightStand(botAI, bot, centre, onFire, out, stand))
    {
        out = stand;
        tolerance = ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE;
    }

    return true;
}

bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    bool const found = DeriveHodirAnchor(botAI, bot, out, tolerance);

    if (RaidObs::Active())
    {
        // Melee have no anchor by design, and "none" is the answer that says so - without it a melee
        // bot standing in the corner is indistinguishable from one that never got told where to go.
        std::string value = "none";
        if (found)
        {
            char suffix[16];
            snprintf(suffix, sizeof(suffix), " ~%.1f", tolerance);
            value = RaidObs::DescribeDerived(out) + suffix;
        }

        RaidObs::NoteDerived(bot, "hodir.anchor", value);
    }

    return found;
}

Player* GetHodirResistancePaladin(PlayerbotAI* /*botAI*/, Player* bot)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return nullptr;

    static uint32 const ranks[] = {SPELL_FROST_RESISTANCE_AURA_RANK_5, SPELL_FROST_RESISTANCE_AURA_RANK_4,
                                   SPELL_FROST_RESISTANCE_AURA_RANK_3, SPELL_FROST_RESISTANCE_AURA_RANK_2,
                                   SPELL_FROST_RESISTANCE_AURA_RANK_1};

    // A paladin tank first, then any non-healer, then whoever is left. The aura reaches 40 yd (48945,
    // radius index 23) and the two people it has to cover are the ones eating Frozen Blows melee -
    // 63511 is 39999 base and lands a median 23691 after resists, against a 34-45k tank pool. A tank
    // never leaves the corner, so it holds them at 100% against 82% for a retribution paladin who runs
    // the dodge and the shelter; every tank killing blow in 603_3_hodir_1787590072 landed with the aura
    // off and the retribution paladin 44-60 yd away, two of them resisting nothing at all.
    //
    // The raid loses about ten points of coverage for it, which is the right trade: a resist point on
    // the tank is ~6000 off a swing that kills, and on the raid ~700 off a tick the healers cover.
    Player* tank = nullptr;
    Player* dps = nullptr;
    Player* healer = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->getClass() != CLASS_PALADIN ||
            member->GetMapId() != bot->GetMapId())
            continue;

        bool knows = false;
        for (uint32 rank : ranks)
            if (member->HasActiveSpell(rank))
            {
                knows = true;
                break;
            }

        if (!knows)
            continue;

        if (PlayerbotAI::IsTank(member))
        {
            if (!tank)
                tank = member;
        }
        else if (PlayerbotAI::IsHeal(member))
        {
            if (!healer)
                healer = member;
        }
        else if (!dps)
        {
            dps = member;
        }
    }

    if (tank)
        return tank;

    return dps ? dps : healer;
}

bool IsHodirTrappedAllyBreaker(PlayerbotAI* botAI, Player* bot, Unit* block)
{
    if (!block)
        return false;

    bool breaker = false;
    Group* group = bot->GetGroup();
    if (!group)
        breaker = true;
    else
    {
        // Healers keep healing: the raid is taking 14000 every two seconds from icicles while this block
        // is up, and it dies to a handful of hits anyway.
        std::vector<Player*> candidates;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
                continue;

            PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
            if (!memberAI || memberAI->IsHeal(member) || memberAI->IsTank(member))
                continue;

            if (member->GetExactDist2d(block) > ULDUAR_HODIR_TRAPPED_ALLY_RANGE)
                continue;

            candidates.push_back(member);
        }

        // Guid, not distance. Distances change every tick, so a distance rank re-shuffles the set
        // constantly and bots drop off the block and back onto the boss between one tick and the next.
        std::sort(candidates.begin(), candidates.end(),
                  [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });

        // Start the window at an offset derived from the block, so blocks that are up together draw
        // disjoint breaker sets. Taking the first five every time would put the same five bots on all
        // of them, and one Flash Freeze puts up a block per helper.
        size_t const total = candidates.size();
        size_t const offset = total ? block->GetGUID().GetCounter() % total : 0;

        for (size_t i = 0; i < total && i < ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS; ++i)
            if (candidates[(offset + i) % total] == bot)
            {
                breaker = true;
                break;
            }
    }

    // Deliberately unprobed. Every block in range is asked about and a bot can come back a breaker for
    // several of them in one pass, so a note per true answer just flaps between blocks - and the one it
    // actually goes for is already in hodir.dpstarget.
    return breaker;
}

Unit* GetHodirAssignedHelperBlock(PlayerbotAI* botAI, Player* bot)
{
    Unit* boss = GetHodir(botAI);
    if (!bot || !boss)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // Ranged carry this. A melee that leaves the boss for a block makes a 21 yd round trip and gives up
    // its whole uptime for it, while a ranged bot can shoot one from nearer where it already stands.
    // Falls back to the rest when the raid has no ranged at all, or nobody would be freed.
    std::vector<Player*> ranged;
    std::vector<Player*> rest;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || memberAI->IsHeal(member) || memberAI->IsTank(member))
            continue;

        if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
        else
            rest.push_back(member);
    }

    std::vector<Player*>& candidates = ranged.empty() ? rest : ranged;

    // Before the sweep, not after: the group walk above is free next to a grid pass, and on a raid with
    // ranged this drops every melee out before it costs one.
    if (std::find(candidates.begin(), candidates.end(), bot) == candidates.end())
        return nullptr;

    // Swept from the boss rather than from the bot, and only as far as the room the fight happens in.
    // A bot-centred sweep hands every bot a different block set and so a different assignment, and the
    // raid then disagrees about who owns what.
    std::list<Creature*> found;
    boss->GetCreatureListWithEntryInGrid(found, NPC_HODIR_FLASH_FREEZE_BLOCK, ULDUAR_HODIR_TRAPPED_ALLY_RANGE);

    std::vector<Creature*> blocks;
    for (Creature* candidate : found)
        if (candidate && candidate->IsAlive())
            blocks.push_back(candidate);

    if (blocks.empty())
        return nullptr;

    // Guid, not live distance, on both lists. Distances to the bot change every tick, so ranking on
    // them re-shuffles the assignment constantly and bots drop off a block and back onto the boss
    // between one tick and the next. Sorted, both lists read the same to every bot, which is what lets
    // the greedy pass below agree across the raid without any shared state.
    std::sort(candidates.begin(), candidates.end(),
              [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });
    std::sort(blocks.begin(), blocks.end(),
              [](Creature* left, Creature* right) { return left->GetGUID() < right->GetGUID(); });

    size_t const total = candidates.size();
    size_t budget = std::min<size_t>(ULDUAR_HODIR_HELPER_BLOCK_BREAKERS, blocks.size());
    if (total > ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE)
        budget = std::min(budget, total - ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE);
    else
        budget = std::min<size_t>(budget, 1);

    // Rank on the formation slot, not on where the bot happens to be. The slot comes out of the same
    // guid-sorted roster and a fixed centre, so it reads identically on every bot and holds still
    // between ticks - the stability the guid rotation was buying - while still handing each block to
    // the bot that stands nearest it. Bots were spending 61.6% of their time on ice walking to it.
    //
    // The anchor rather than the adopted centre: this is a ranking key, not a spot anyone walks to,
    // and asking for the real centre would cost a second fire sweep every tick for nothing.
    std::vector<Player*> ringMembers;
    BuildHodirRingMembers(bot, ringMembers);

    std::vector<Position> spots(total);
    for (size_t i = 0; i < total; ++i)
    {
        size_t slot = ringMembers.size();
        for (size_t r = 0; r < ringMembers.size(); ++r)
            if (ringMembers[r] == candidates[i])
                slot = r;

        // Only the melee fallback lands here, and it has no slot to rank from. Its own position is
        // still the same number on every bot that reads it, so the raid keeps agreeing.
        spots[i] = slot < ringMembers.size()
                       ? HodirRingSlotPoint(ULDUAR_HODIR_RAID_ANCHOR, slot, ringMembers.size())
                       : candidates[i]->GetPosition();
    }

    // One breaker each, in block-guid order, every block taking the nearest slot still free.
    std::vector<bool> taken(total, false);
    for (size_t i = 0; i < budget; ++i)
    {
        size_t best = total;
        float bestDist = 0.0f;
        for (size_t j = 0; j < total; ++j)
        {
            if (taken[j])
                continue;

            float const dist = blocks[i]->GetExactDist2d(&spots[j]);
            if (best == total || dist < bestDist)
            {
                best = j;
                bestDist = dist;
            }
        }

        if (best == total)
            break;

        taken[best] = true;
        if (candidates[best] == bot)
            return blocks[i];
    }

    return nullptr;
}
