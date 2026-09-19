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
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "UldScripts.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace EncounterHelpers;

// Where Hodir is held when no fire qualifies. The eight helpers spawn round the middle of the room
// (boss_hodir.cpp hhd) and from here every one is inside its AttackStartCaster stand-off: priests
// 16.3/15.5 of 17, druids 21.6/5.4 of 22, shamans 14.5/9.3 of 25, mages 23.2/6.5 of 30. So none of
// them has to walk to reach him, and early on their zones land near where they spawned: Starlight a
// median 9.8 yd from a druid's spawn, fires 8.2 from a mage's. They still drift later in a pull, for
// reasons the trace can't show (friendly NPCs are not swept). The old corner hold dragged him 30-55 yd
// out to every fire and back, 913 of the 1106 yd he walked across three pulls. navprobe settles this
// point and 6/12 yd rings round it at 432.687.
const Position ULDUAR_HODIR_CENTRE = Position(1998.0f, -235.5f, 432.687f);

// Centre of the ranged ring while he is on ULDUAR_HODIR_CENTRE, 24.45 yd south-west of it, so the ring's
// 9 yd sits inside the 15-35 yd caster band.
const Position ULDUAR_HODIR_RAID_ANCHOR = Position(1986.56f, -257.11f, 432.687f);

// The ring's slot 1 bearing. Fixed so the layout never rotates and nobody's slot changes when the
// anchor moves.
constexpr float ULDUAR_HODIR_RING_BEARING = -2.1512f;

// From the ranged ring out through the centre. Tanks shuttle across it and the off-tank stands off it,
// so neither walks Hodir at the ring.
static float HodirOutwardBearing()
{
    return std::atan2(ULDUAR_HODIR_CENTRE.GetPositionY() - ULDUAR_HODIR_RAID_ANCHOR.GetPositionY(),
                      ULDUAR_HODIR_CENTRE.GetPositionX() - ULDUAR_HODIR_RAID_ANCHOR.GetPositionX());
}

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

float GetHodirShelterPark(Creature* shelter)
{
    return shelter && shelter->GetEntry() == NPC_HODIR_ICICLE_DRIFT ? ULDUAR_HODIR_BIG_SHARDS_CLEAR
                                                                    : ULDUAR_HODIR_SAFE_AREA_TOLERANCE;
}

float GetHodirShelterRelease(Creature* shelter)
{
    return GetHodirShelterPark(shelter) +
           (ULDUAR_HODIR_SAFE_AREA_RELEASE - ULDUAR_HODIR_SAFE_AREA_TOLERANCE);
}

// Both per-bot latches, keyed by instance on the way in so the inner maps are only ever touched by the
// one thread ticking that map.
//
// Not thread_local. A map is updated by one thread at a time but is never pinned to one, and
// MapUpdate.Threads is 6 here, so per-thread copies hand the same bot a fresh latch whenever the pool
// reassigns its map. Both picks below then stop holding and go back to drifting, which is the 739 anchor
// changes the Starlight comment further down was written to kill. References into an unordered_map
// survive rehashing, so the lock only has to cover the outer lookup.
struct HodirStarlightLatch
{
    Position zone;
    Position stand;
};

// One entry per carry, keyed on the carrier rather than on the bot reading it: the carrier and every
// receiver have to gather on the same point, so the first to derive it publishes and the rest read.
// applied is the aura's apply time, which is what tells two carries apart - stacks only ever fall, so
// a carry ending on one and a new one starting on one are the same number.
struct HodirStormRally
{
    time_t applied = 0;
    Position rally;
};

// Where Hodir is held, one per instance: both tanks and the ranged ring read it, so all three agree.
// fire is empty while the point is ULDUAR_HODIR_CENTRE. bearing points from where he stood when the
// point was adopted to the point, so the tank stands on the far side and he trails onto it. offSince
// is when he was last seen parked off the point, 0 while he is on it.
struct HodirHoldLatch
{
    bool set = false;
    ObjectGuid fire;
    Position point;
    float bearing = 0.0f;
    uint32 offSince = 0;
};

struct HodirBotLatches
{
    std::unordered_map<ObjectGuid, ObjectGuid> shelter;
    std::unordered_map<ObjectGuid, HodirStarlightLatch> starlight;
    std::unordered_map<ObjectGuid, HodirStormRally> stormRally;
    HodirHoldLatch hold;
};

static std::mutex hodirLatchesMutex;
static std::unordered_map<uint32 /*instanceId*/, HodirBotLatches> hodirLatches;

static HodirBotLatches& HodirLatchesFor(Player* bot)
{
    std::lock_guard<std::mutex> guard(hodirLatchesMutex);
    return hodirLatches[bot->GetInstanceId()];
}

Creature* GetHodirShelter(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !GetHodir(botAI))
        return nullptr;

    std::unordered_map<ObjectGuid, ObjectGuid>& latched = HodirLatchesFor(bot).shelter;

    std::vector<Creature*> candidates;
    std::list<Creature*> found;

    bot->GetCreatureListWithEntryInGrid(found, NPC_SNOWPACKED_ICICLE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
    for (Creature* target : found)
        if (target && target->IsAlive())
            candidates.push_back(target);

    // A drift stands at the spot its target will occupy - 0.0 yd apart across seven freezes - and it is
    // on the floor from the first tick of the 9s cast while the target only appears at 3.9s. Staging on
    // it is the difference between a 5.1s scramble over up to 35 yd and a 9s walk. Only while the cast
    // is up: at any other time a drift is something to dodge, not somewhere to stand.
    if (IsHodirFlashFreezeIncoming(botAI))
    {
        found.clear();
        bot->GetCreatureListWithEntryInGrid(found, NPC_HODIR_ICICLE_DRIFT, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
        for (Creature* drift : found)
            if (IsHodirIcicleLethal(drift))
                candidates.push_back(drift);
    }

    if (candidates.empty())
    {
        latched.erase(bot->GetGUID());

        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "hodir.shelter", "none");

        return nullptr;
    }

    Creature* best = nullptr;

    // Held until the pick leaves the floor rather than re-derived every tick. Nearest-to-self mostly
    // holds itself, because walking at a shelter keeps it the nearest one, but a sideways dodge can
    // carry a bot past the midpoint between two and re-picking there flips the destination. The drift
    // hands over for free: it stops being a candidate when it lands, the latch drops, and the sweep
    // below finds the target that just spawned where the bot is already standing.
    if (auto held = latched.find(bot->GetGUID()); held != latched.end())
        for (Creature* candidate : candidates)
            if (candidate->GetGUID() == held->second)
            {
                best = candidate;
                break;
            }

    if (!best)
    {
        // Nearest the bot. Trigger and action both call this, and one derivation is what keeps them
        // from disagreeing - but it never had to be the same answer for every bot. All three targets
        // grant the Safe Area, and ranking from the ring centre instead sent bots a median 17.7 yd
        // when their own was 9.8, walked two of them into a Flash Freeze, and left one standing 2.6 yd
        // from a shelter while it ran 31.6 yd to another.
        float bestDist = 0.0f;
        for (Creature* candidate : candidates)
        {
            float const dist = bot->GetExactDist2d(candidate);
            if (!best || dist < bestDist)
            {
                best = candidate;
                bestDist = dist;
            }
        }

        latched[bot->GetGUID()] = best->GetGUID();
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shelter", RaidObs::DescribeAssignment(best->GetGUID()));

    return best;
}

bool IsHodirInLandedShelter(PlayerbotAI* botAI, Player* bot)
{
    Creature* shelter = GetHodirShelter(botAI, bot);
    return shelter && shelter->GetEntry() == NPC_SNOWPACKED_ICICLE &&
           bot->GetExactDist2d(shelter) <= GetHodirShelterPark(shelter);
}

// Where he is held for this fire: the point of the hold box nearest it, or false when that point is
// further than ULDUAR_HODIR_HOLD_FIRE_REACH from it. The box is the leash disc cut by the backstep line,
// so the nearest point is the fire itself or the nearest in-box one of: its projection onto the line,
// its projection onto the circle, the two corners. Fires never move and neither does the box, so the
// answer for a fire never changes.
static bool HodirFireHoldPoint(Creature* fire, Position& out)
{
    float const outward = HodirOutwardBearing();
    float const ux = std::cos(outward);
    float const uy = std::sin(outward);
    float const leash = ULDUAR_HODIR_HOLD_FIRE_LEASH;
    float const back = ULDUAR_HODIR_HOLD_FIRE_BACKSTEP;

    // Relative to the centre from here on.
    float const fx = fire->GetPositionX() - ULDUAR_HODIR_CENTRE.GetPositionX();
    float const fy = fire->GetPositionY() - ULDUAR_HODIR_CENTRE.GetPositionY();

    // 1 cm of slack so a candidate built on an edge is not thrown out by float rounding.
    auto inBox = [&](float x, float y)
    { return std::hypot(x, y) <= leash + 0.01f && x * ux + y * uy >= -back - 0.01f; };

    float bestX = fx;
    float bestY = fy;
    float bestGap = inBox(fx, fy) ? 0.0f : -1.0f;
    auto consider = [&](float x, float y)
    {
        if (bestGap == 0.0f || !inBox(x, y))
            return;

        float const gap = std::hypot(fx - x, fy - y);
        if (bestGap < 0.0f || gap < bestGap)
        {
            bestX = x;
            bestY = y;
            bestGap = gap;
        }
    };

    float const along = fx * ux + fy * uy;
    consider(fx - (along + back) * ux, fy - (along + back) * uy);

    if (float const radius = std::hypot(fx, fy); radius > 0.0f)
        consider(fx * leash / radius, fy * leash / radius);

    float const half = std::sqrt(leash * leash - back * back);
    consider(-back * ux - uy * half, -back * uy + ux * half);
    consider(-back * ux + uy * half, -back * uy - ux * half);

    if (bestGap < 0.0f || bestGap > ULDUAR_HODIR_HOLD_FIRE_REACH)
        return false;

    out = Position(ULDUAR_HODIR_CENTRE.GetPositionX() + bestX, ULDUAR_HODIR_CENTRE.GetPositionY() + bestY,
                   ULDUAR_HODIR_CENTRE.GetPositionZ());
    return true;
}

// Where Hodir is held: the box point for the qualifying Toasty Fire nearest the centre, else the centre
// itself. Standing in a fire sheds Biting Cold every tick exactly as moving does, and melee spells and
// pets in one proc Singed on him, +2% magic damage taken a stack up to 25. That is why it is worth the
// short drag.
//
// Nothing but Flash Freeze puts a fire out (npc_ulduar_toasty_fire::DoAction), so the pick holds until
// the fire dies; re-picking while one burns walks him between two of them.
static HodirHoldLatch const* DeriveHodirHold(PlayerbotAI* botAI, Player* bot)
{
    Unit* hodir = bot ? GetHodir(botAI) : nullptr;
    if (!hodir)
        return nullptr;

    HodirHoldLatch& hold = HodirLatchesFor(bot).hold;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_TOASTY_FIRE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    Creature* pick = nullptr;
    Position pickPoint;
    if (!hold.fire.IsEmpty())
        for (Creature* fire : found)
            if (fire && fire->IsAlive() && fire->GetGUID() == hold.fire)
            {
                pick = fire;
                pickPoint = hold.point;
                break;
            }

    if (!pick)
    {
        float bestDist = 0.0f;
        for (Creature* fire : found)
        {
            Position point;
            if (!fire || !fire->IsAlive() || !HodirFireHoldPoint(fire, point))
                continue;

            float const dist = fire->GetExactDist2d(&ULDUAR_HODIR_CENTRE);
            if (!pick || dist < bestDist)
            {
                pick = fire;
                pickPoint = point;
                bestDist = dist;
            }
        }
    }

    ObjectGuid const fire = pick ? pick->GetGUID() : ObjectGuid::Empty;

    // Out of combat he is standing on his spawn, so the bearing is taken fresh every tick and the tank
    // pre-positions on the far side of the centre from him. It only latches once he is engaged, and a
    // wipe drops it again.
    bool const engaged = hodir->IsInCombat();
    if (!engaged || !hold.set || hold.fire != fire)
    {
        hold.set = engaged;
        hold.fire = fire;
        hold.point = pick ? pickPoint : ULDUAR_HODIR_CENTRE;
        hold.offSince = 0;

        // He trails the tank, so the tank stands past the point on the side away from him and he stops
        // on it. Right on top of the point there is no side, and the outward bearing keeps the tank
        // off the ranged ring.
        hold.bearing = hodir->GetExactDist2d(&hold.point) > ULDUAR_HODIR_HOLD_BEARING_MIN_GAP
                           ? std::atan2(hold.point.GetPositionY() - hodir->GetPositionY(),
                                        hold.point.GetPositionX() - hodir->GetPositionX())
                           : HodirOutwardBearing();
    }
    else if (!hodir->isMoving() && hodir->GetExactDist2d(&hold.point) > ULDUAR_HODIR_HOLD_REAIM_GAP)
    {
        // Parked off the point with the tank spot already in his reach, so nothing brings him back.
        // Aim again from where he stands; the tank walks round to the far side and he trails onto it.
        uint32 const now = getMSTime();
        if (!hold.offSince)
            hold.offSince = now;
        else if (getMSTimeDiff(hold.offSince, now) >= ULDUAR_HODIR_HOLD_REAIM_MS)
        {
            hold.bearing = std::atan2(hold.point.GetPositionY() - hodir->GetPositionY(),
                                      hold.point.GetPositionX() - hodir->GetPositionX());
            hold.offSince = 0;
        }
    }
    else
        hold.offSince = 0;

    // Which fire, not where: the spot is already in hodir.anchor and hodir.centre.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.tankhold",
                             pick ? RaidObs::DescribeAssignment(fire) : std::string("centre"));

    return &hold;
}

Player* GetHodirStormCloudCarrier(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !GetHodir(botAI))
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    uint32 const cloud = sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot);

    // Lowest guid wins when the boss has two carries running, so every bot picks the same one to
    // gather on rather than each heading for whichever it happened to iterate first.
    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (!member->HasAura(cloud))
            continue;

        if (!best || member->GetGUID() < best->GetGUID())
            best = member;
    }

    return best;
}

bool GetHodirStormCloudRally(PlayerbotAI* botAI, Player* bot, Player* carrier, Position& out)
{
    if (!bot || !carrier)
        return false;

    Aura* cloud = carrier->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, carrier));
    if (!cloud)
        return false;

    std::unordered_map<ObjectGuid, HodirStormRally>& latched = HodirLatchesFor(bot).stormRally;
    HodirStormRally& entry = latched[carrier->GetGUID()];

    time_t const applied = cloud->GetApplyTime();
    if (entry.applied != applied)
    {
        entry.applied = applied;

        // Seeded from where the carrier was standing when the cloud landed, so the gather point is
        // already in the middle of whichever half of the raid the carrier belongs to: a melee carrier
        // holds the melee on the boss, a ranged one holds the formation. Picking one point for the
        // whole raid instead would walk half of it across the room for six seconds of buff.
        entry.rally = ValidateFloorPoint(bot, carrier->GetPosition());
    }

    out = entry.rally;
    return true;
}

Position GetHodirRingCentre(PlayerbotAI* botAI, Player* bot)
{
    Position centre = ULDUAR_HODIR_RAID_ANCHOR;

    // Moves with the hold point and only when it does, once per fire at most. The leash and the
    // backstep keep that shift under 12 yd and never toward him.
    if (HodirHoldLatch const* hold = DeriveHodirHold(botAI, bot))
        centre = Position(ULDUAR_HODIR_RAID_ANCHOR.GetPositionX() + hold->point.GetPositionX() -
                              ULDUAR_HODIR_CENTRE.GetPositionX(),
                          ULDUAR_HODIR_RAID_ANCHOR.GetPositionY() + hold->point.GetPositionY() -
                              ULDUAR_HODIR_CENTRE.GetPositionY(),
                          ULDUAR_HODIR_RAID_ANCHOR.GetPositionZ());

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
    size_t const inner = std::min<size_t>(ULDUAR_HODIR_RAID_RING_INNER_SLOTS, total > 0 ? total - 1 : 0);

    float const baseAngle = ULDUAR_HODIR_RING_BEARING;
    float radius = 0.0f;
    float angle = baseAngle;

    if (!slot)
    {
        // Slot 0 stands on the centre itself, the only one never within 4 yd of two neighbours at once.
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
//
// The pick is latched per bot and held until the zone expires, because both of its inputs move. Zones
// are ranked against the slot and the bearing is taken from the slot, so every centre change - 3
// distinct ones in a six minute kill - rewrites all 14 slots and with them the answer here. Stateless,
// that came out as 739 anchor changes across 23 bots, a median 11 yd jump every 3.4s, and Starlight
// windows lasting 1.6s against zones that live a minute.
static bool FindHodirStarlightStand(PlayerbotAI* botAI, Player* bot, Position const& slot, Position& out)
{
    std::unordered_map<ObjectGuid, HodirStarlightLatch>& latched = HodirLatchesFor(bot).starlight;

    // Bounded rather than the room radius: this runs per bot per tick, and a zone further out than
    // this cannot be within reach of any slot the bot could be standing on anyway.
    std::vector<Position> const zones =
        GetDynamicObjectPositions(bot, ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT);
    if (zones.empty())
    {
        latched.erase(bot->GetGUID());

        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "hodir.starlight", "none");

        return false;
    }

    // The druid casts where it is standing, which is usually next to Hodir, so a good share of the
    // zones on the floor are ones no caster can use.
    Unit* hodir = GetHodir(botAI);

    // Which rule kept the bot out, not which zone. A zone passed every gate on 85% of ranged ticks
    // while they stood in one for 22%, so "was there one to have" and "was it offered" have to be
    // separable from position alone.
    char const* how = "none";

    // The rule that keeps a bot off a stand point, or nullptr when it passes. Shared so a latched
    // point is re-checked against exactly what a fresh pick faces, since the boss moves and a point
    // that was good when it was picked can stop being one.
    //
    // No fire leash. Ranged are Starlight first; holding the stand inside a fire-centred ring threw out
    // 56-91 stands a pull and kept ranged Starlight at 11-14% with a zone up 90% of the time.
    auto standRejects = [&](Position const& stand) -> char const*
    {
        // Both ends of the caster band. A zone the bot cannot shoot the boss from is not a throughput
        // lever, whatever haste it carries.
        if (hodir)
        {
            float const gap = stand.GetExactDist2d(hodir);
            if (gap < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP || gap > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
                return "noreach";
        }

        return nullptr;
    };

    auto held = latched.find(bot->GetGUID());
    if (held != latched.end())
    {
        bool stillUp = false;
        for (Position const& zone : zones)
        {
            if (zone.GetExactDist2d(&held->second.zone) <= ULDUAR_HODIR_STARLIGHT_ZONE_MATCH)
            {
                stillUp = true;
                break;
            }
        }

        if (stillUp)
        {
            // Held through a reject rather than dropped. Erasing here re-swept, and the sweep ranks by
            // walk from the slot, so Hodir drifting a yard past the caster band handed the bot a
            // different zone that rejected on the next tick and back: 1201 anchor moves a median 320ms
            // apart, ten of them per distinct point, 1137 of them with a stand in force. Falling back
            // to the ring slot for the ticks a stand does not qualify costs one walk; swapping zones
            // cost the walk every tick.
            if (char const* reject = standRejects(held->second.stand))
            {
                // "held" so a latched stand standing down reads differently from a sweep that found
                // nothing: the first is a bot with a zone waiting for the boss to move back, the
                // second is a bot with no zone at all, and only the second wants more zones.
                if (RaidObs::Active())
                    RaidObs::NoteDerived(bot, "hodir.starlight", std::string("held ") + reject);

                return false;
            }

            out = held->second.stand;

            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "hodir.starlight",
                                     "stand " + RaidObs::DescribeDerived(held->second.zone));

            return true;
        }

        // The zone expired, so the next sweep is the one that counts.
        latched.erase(held);
    }

    bool found = false;
    float bestWalk = 0.0f;
    Position bestZone;

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

        if (char const* reject = standRejects(stand))
        {
            if (!found)
                how = reject;

            continue;
        }

        out = stand;
        bestZone = zone;
        bestWalk = walk;
        found = true;
    }

    if (found)
        latched[bot->GetGUID()] = HodirStarlightLatch{bestZone, out};

    // The point itself is already hodir.anchor, which this becomes when it is found. The zone rides
    // along because the value is otherwise only the rule: a re-latch onto a different zone still read
    // "stand", NoteDerived emits on change, and so the swap above stayed invisible for a whole pull.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.starlight",
                             found ? "stand " + RaidObs::DescribeDerived(bestZone) : std::string(how));

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

std::vector<HazardCircle> CollectHodirIcicleHazards(Player* bot, float radius, float smallClear, float bigClear)
{
    std::vector<HazardCircle> hazards;

    for (auto const& entry : {std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_SMALL), smallClear),
                              std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_DRIFT), bigClear)})
    {
        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, entry.first, radius);
        for (Creature* icicle : found)
            if (IsHodirIcicleLethal(icicle))
                hazards.emplace_back(icicle->GetPosition(), entry.second);
    }

    return hazards;
}

// 2D distance from a point to the walk between two positions, endpoints included, so one test
// covers where the bot is, where it is headed, and everything it crosses on the way.
static float DistToSegment2d(Position const& point, Position const& from, Position const& to)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const lengthSq = dx * dx + dy * dy;

    float t = 0.0f;
    if (lengthSq > 0.0f)
    {
        t = ((point.GetPositionX() - from.GetPositionX()) * dx +
             (point.GetPositionY() - from.GetPositionY()) * dy) /
            lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    float const nearestX = from.GetPositionX() + dx * t;
    float const nearestY = from.GetPositionY() + dy * t;
    return std::hypot(point.GetPositionX() - nearestX, point.GetPositionY() - nearestY);
}

bool IsHodirWalkThroughLiveIcicle(Player* bot, Position const& dest, float smallRadius, float bigRadius)
{
    if (!bot)
        return false;

    Position const self = bot->GetPosition();
    for (auto const& entry : {std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_SMALL), smallRadius),
                              std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_DRIFT), bigRadius)})
    {
        std::list<Creature*> icicles;
        bot->GetCreatureListWithEntryInGrid(icicles, entry.first, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
        for (Creature* icicle : icicles)
            if (IsHodirIcicleLethal(icicle) && DistToSegment2d(icicle->GetPosition(), self, dest) < entry.second)
                return true;
    }

    return false;
}

bool GetHodirDodgePreference(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!PlayerbotAI::IsMelee(bot))
    {
        float tolerance = 0.0f;
        return GetHodirAnchor(botAI, bot, out, tolerance);
    }

    Unit* boss = GetHodir(botAI);
    if (!boss)
        return false;

    out = boss->GetPosition();
    return true;
}

static bool DeriveHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance);

static bool DeriveHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out, char const*& how)
{
    if (!bot)
        return false;

    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        // Two points either side of the hold spot rather than wandering, because Hodir follows. Across
        // the ranged ring's line, so neither end walks him at it, and 6 yd apart covers two aura ticks.
        Position spot;
        float tolerance = 0.0f;
        if (!DeriveHodirAnchor(botAI, bot, spot, tolerance))
            return false;

        float const axis = HodirOutwardBearing() + static_cast<float>(M_PI) / 2.0f;
        float const dx = std::cos(axis) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;
        float const dy = std::sin(axis) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;

        Position const legA = ValidateFloorPoint(
            bot, Position(spot.GetPositionX() + dx, spot.GetPositionY() + dy, spot.GetPositionZ()));
        Position const legB = ValidateFloorPoint(
            bot, Position(spot.GetPositionX() - dx, spot.GetPositionY() - dy, spot.GetPositionZ()));

        // Whichever end is further away, so the leg is always the full 6 yd and IsDuplicateMove
        // cannot refuse it for repeating the last destination.
        out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
        how = "tank";
        return true;
    }

    // Inside a landed shelter while Flash Freeze is cast: shed across it rather than out of it, same
    // shape as the Starlight shuttle below. Standing out the 9s cast adds two stacks, which is how a
    // warlock went 5 -> 7 and died on the landing.
    if (IsHodirFlashFreezeIncoming(botAI) && IsHodirInLandedShelter(botAI, bot))
    {
        Creature* shelter = GetHodirShelter(botAI, bot);
        float const bearing = std::atan2(bot->GetPositionY() - shelter->GetPositionY(),
                                         bot->GetPositionX() - shelter->GetPositionX());
        float const dx = std::cos(bearing) * ULDUAR_HODIR_SHELTER_SHED_RADIUS;
        float const dy = std::sin(bearing) * ULDUAR_HODIR_SHELTER_SHED_RADIUS;

        Position const legA = ValidateFloorPoint(
            bot, Position(shelter->GetPositionX() + dx, shelter->GetPositionY() + dy, shelter->GetPositionZ()));
        Position const legB = ValidateFloorPoint(
            bot, Position(shelter->GetPositionX() - dx, shelter->GetPositionY() - dy, shelter->GetPositionZ()));

        out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
        how = "shelter";
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

    // Everyone else steps to the nearest point clear of the rest of the raid, of every live icicle,
    // and - for anyone who has to shoot from range - of Hodir himself. Sweeping for allies alone put
    // three raiders on top of an icicle inside fourteen seconds and left a warlock dead at 7.7 yd from
    // the boss, and the old no-allies branch was a blind 6 yd hop on a guid-derived bearing with no
    // sweep and no floor check at all.
    //
    // distanceStep is the leg length, not a probe granularity: the helper rings outward from it, so
    // 6 yd is the shortest move it can return and a shorter one would not span two aura ticks.
    bool crowded = false;
    auto sweepFor = [&](float declump, float smallClear, float bigClear)
    {
        std::vector<HazardCircle> hazards;

        for (auto const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
        {
            Unit* ally = botAI->GetUnit(guid);
            if (ally && ally->IsAlive() && ally != bot && bot->GetExactDist2d(ally) <= ULDUAR_HODIR_DODGE_LEASH)
            {
                hazards.emplace_back(ally->GetPosition(), declump);
                crowded = true;
            }
        }

        for (HazardCircle const& icicle :
             CollectHodirIcicleHazards(bot, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, smallClear, bigClear))
            hazards.push_back(icicle);

        // Melee belong inside this, so only the bots the gap is protecting get it.
        if (!PlayerbotAI::IsMelee(bot))
            if (Unit* hodir = GetHodir(botAI))
                hazards.emplace_back(hodir->GetPosition(), ULDUAR_HODIR_RANGED_MIN_BOSS_GAP);

        return hazards;
    };

    Position preference;
    Position const* preferNear = GetHodirDodgePreference(botAI, bot, preference) ? &preference : nullptr;

    Position leg = FindNearestPositionClearOfHazards(
        bot, sweepFor(ULDUAR_HODIR_DECLUMP_RADIUS, ULDUAR_HODIR_ICE_SHARDS_CLEAR, ULDUAR_HODIR_BIG_SHARDS_CLEAR),
        ULDUAR_HODIR_DODGE_LEASH, 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG, static_cast<float>(M_PI) / 8.0f,
        preferNear);

    // Nothing clear with margin. The clears carry 2 yd over the radius that actually kills and the
    // declump only stops two bots sharing one icicle, so both are worth giving up before standing
    // still: a bot that cannot move sheds nothing.
    if (!leg.GetPositionX() && !leg.GetPositionY())
        leg = FindNearestPositionClearOfHazards(
            bot,
            sweepFor(ULDUAR_HODIR_SHUTTLE_HALF_LEG, ULDUAR_HODIR_ICE_SHARDS_RADIUS + 0.5f,
                     ULDUAR_HODIR_BIG_SHARDS_RADIUS + 0.5f),
            2.0f * ULDUAR_HODIR_DODGE_LEASH, 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG,
            static_cast<float>(M_PI) / 8.0f, preferNear);

    if (!leg.GetPositionX() && !leg.GetPositionY())
        return false;

    out = leg;
    how = crowded ? "crowd" : "solo";
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

bool IsHodirBitingColdShedArmed(Player* bot)
{
    if (!bot)
        return false;

    // A fire counts as movement on every tick, so a bot in one sheds without the shuttle ever running.
    if (bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA))
        return false;

    Aura* cold = bot->GetAura(SPELL_BITING_COLD_PLAYER_AURA);
    if (!cold)
        return false;

    // The extra stack in Starlight buys uninterrupted haste rather than an uninterrupted stand, and it
    // has to be the same number the action arms at or the two disagree about whether a shed is coming.
    uint32 const arm = bot->HasAura(SPELL_HODIR_STARLIGHT) ? ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT
                                                           : ULDUAR_HODIR_BITING_COLD_SHED_STACKS;

    return cold->GetStackAmount() >= arm;
}

// Where a tank stands so Hodir stops on the hold point. The main tank goes past it on the far side
// from where he came, and he trails onto it. The off-tank stands off to one side of it, across the
// ranged ring's line, so a Frozen Blows taunt walks him about 6 yd sideways rather than at the ring.
static Position HodirTankSpot(Player* bot, HodirHoldLatch const& hold, bool offTank)
{
    float const angle = offTank ? HodirOutwardBearing() - static_cast<float>(M_PI) / 2.0f : hold.bearing;
    return ValidateFloorPoint(
        bot, Position(hold.point.GetPositionX() + std::cos(angle) * ULDUAR_HODIR_HOLD_TANK_OFFSET,
                      hold.point.GetPositionY() + std::sin(angle) * ULDUAR_HODIR_HOLD_TANK_OFFSET,
                      hold.point.GetPositionZ()));
}

static bool DeriveHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    if (!GetHodir(botAI))
        return false;

    bool const mainTank = botAI->IsMainTank(bot);
    if (mainTank || botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        HodirHoldLatch const* hold = DeriveHodirHold(botAI, bot);
        if (!hold)
            return false;

        out = HodirTankSpot(bot, *hold, !mainTank);
        tolerance = ULDUAR_HODIR_TANK_HOLD_TOLERANCE;
        return true;
    }

    // Melee ride the boss. Pinning them would cost uptime, and he is on a fire whenever one is near
    // enough to drag him to.
    if (!botAI->IsRanged(bot))
        return false;

    // Derived once and passed down, since every call sweeps the grid for fires and writes a note.
    Position const centre = GetHodirRingCentre(botAI, bot);
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
    if (FindHodirStarlightStand(botAI, bot, out, stand))
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
        // bot standing on the boss is indistinguishable from one that never got told where to go.
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
    // never leaves the hold point, so it holds them at 100% against 82% for a retribution paladin who runs
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

// Who a helper block holds, in the order they get freed. The block is summoned by the helper inside
// it (boss_hodir.cpp), so the summoner names it. Mages first because their fire brings back both the
// Biting Cold shed and Singed; then Starlight, then Storm Cloud.
enum class HodirHelperKind : uint8
{
    Mage,
    Druid,
    Shaman,
    Priest,
    None
};

static HodirHelperKind GetHodirHelperKind(Unit* block)
{
    if (!block || block->GetEntry() != NPC_HODIR_FLASH_FREEZE_BLOCK)
        return HodirHelperKind::None;

    TempSummon* summon = block->ToTempSummon();
    Unit* helper = summon ? summon->GetSummonerUnit() : nullptr;
    switch (helper ? helper->GetEntry() : 0)
    {
        case NPC_HODIR_MAGE_ALLIANCE_10:
        case NPC_HODIR_MAGE_ALLIANCE_25:
        case NPC_HODIR_MAGE_HORDE_10:
        case NPC_HODIR_MAGE_HORDE_25:
            return HodirHelperKind::Mage;
        case NPC_HODIR_DRUID_ALLIANCE_10:
        case NPC_HODIR_DRUID_ALLIANCE_25:
        case NPC_HODIR_DRUID_HORDE_10:
        case NPC_HODIR_DRUID_HORDE_25:
            return HodirHelperKind::Druid;
        case NPC_HODIR_SHAMAN_ALLIANCE_10:
        case NPC_HODIR_SHAMAN_ALLIANCE_25:
        case NPC_HODIR_SHAMAN_HORDE_10:
        case NPC_HODIR_SHAMAN_HORDE_25:
            return HodirHelperKind::Shaman;
        case NPC_HODIR_PRIEST_ALLIANCE_10:
        case NPC_HODIR_PRIEST_ALLIANCE_25:
        case NPC_HODIR_PRIEST_HORDE_10:
        case NPC_HODIR_PRIEST_HORDE_25:
            return HodirHelperKind::Priest;
        default:
            return HodirHelperKind::None;
    }
}

char const* GetHodirHelperBlockKind(Unit* block)
{
    switch (GetHodirHelperKind(block))
    {
        case HodirHelperKind::Mage:
            return "mage";
        case HodirHelperKind::Druid:
            return "druid";
        case HodirHelperKind::Shaman:
            return "shaman";
        case HodirHelperKind::Priest:
            return "priest";
        default:
            return nullptr;
    }
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

    // Helper kind, then guid; guid, not live distance, on the bots. Distances to the bot change every
    // tick, so ranking on them re-shuffles the assignment constantly and bots drop off a block and back
    // onto the boss between one tick and the next. Sorted, both lists read the same to every bot, which
    // is what lets the greedy pass below agree across the raid without any shared state.
    std::sort(candidates.begin(), candidates.end(),
              [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });
    std::sort(blocks.begin(), blocks.end(),
              [](Creature* left, Creature* right)
              {
                  HodirHelperKind const leftKind = GetHodirHelperKind(left);
                  HodirHelperKind const rightKind = GetHodirHelperKind(right);
                  if (leftKind != rightKind)
                      return leftKind < rightKind;
                  return left->GetGUID() < right->GetGUID();
              });

    // One seat per breaker. A mage block takes several, everything else one.
    std::vector<Creature*> seats;
    for (Creature* block : blocks)
    {
        uint32 const want =
            GetHodirHelperKind(block) == HodirHelperKind::Mage ? ULDUAR_HODIR_MAGE_BLOCK_BREAKERS : 1;
        for (uint32 i = 0; i < want; ++i)
            seats.push_back(block);
    }

    size_t const total = candidates.size();
    size_t budget = std::min<size_t>(ULDUAR_HODIR_HELPER_BLOCK_BREAKERS, seats.size());
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

    // Seat order, every seat taking the nearest slot still free.
    std::vector<bool> taken(total, false);
    for (size_t i = 0; i < budget; ++i)
    {
        size_t best = total;
        float bestDist = 0.0f;
        for (size_t j = 0; j < total; ++j)
        {
            if (taken[j])
                continue;

            float const dist = seats[i]->GetExactDist2d(&spots[j]);
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
            return seats[i];
    }

    return nullptr;
}
