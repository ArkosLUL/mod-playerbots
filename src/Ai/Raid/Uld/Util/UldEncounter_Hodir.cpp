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
#include "RaidInstanceState.h"
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

// Which way the tank stands off the hold point when Hodir is already on top of it and the bearing to
// him means nothing. North-east into the open floor, so the degenerate case never puts the tank spot
// in the chamfered south-west corner.
constexpr float ULDUAR_HODIR_HOLD_FALLBACK_BEARING = 1.0845f;

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

bool IsHodirLustHeld(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);
    if (!boss || !boss->IsInCombat())
        return false;

    // Gone after 3:00, or never cast: nothing to time the hold by, so don't hold.
    Aura const* timer = boss->GetAura(SPELL_HODIR_SHATTER_CHEST_TIMER);
    if (!timer ||
        timer->GetMaxDuration() - timer->GetDuration() >= static_cast<int32>(ULDUAR_HODIR_LUST_FALLBACK_MS))
        return false;

    return !botAI->GetBot()->FindNearestCreature(NPC_TOASTY_FIRE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, true);
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
struct HodirStarlightLatch
{
    Position zone;
    Position stand;
    uint32 rejectedSince = 0;  // when the stand started failing the band, 0 while it passes
};

// The fire a bot stands in, latched the same way and for the same reason: what makes a stand usable is
// measured against a boss who moves, so a stateless sweep swaps fires whenever he drifts.
struct HodirFireLatch
{
    ObjectGuid fire;
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

// Where Hodir is held, one per instance so both tanks agree. fire is empty while the point is
// ULDUAR_HODIR_CENTRE. bearing points from where he stood when the
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
    std::unordered_map<ObjectGuid, HodirFireLatch> fireStand;
    std::unordered_map<ObjectGuid, HodirStormRally> stormRally;

    // Where each ice-breaking candidate stood when the first block of a wave went up. Cleared once no
    // block is left, so the next wave reads fresh positions.
    std::unordered_map<ObjectGuid, Position> blockRank;

    HodirHoldLatch hold;
};

static RaidInstanceState<HodirBotLatches> hodirLatches;

static HodirBotLatches& HodirLatchesFor(Player* bot) { return hodirLatches.For(bot->GetInstanceId()); }

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

// Where Hodir is held: the Toasty Fire nearest the centre that is inside ULDUAR_HODIR_HOLD_FIRE_LEASH
// of it, else the centre itself. On the fire and not near it, because the melee pack wraps round him
// (p50 1.2 yd behind his centre) rather than lining up behind, so a fire at his feet is the one that
// covers them - and it is the bots standing in a fire that proc Singed, never the fire itself.
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
    if (!hold.fire.IsEmpty())
        for (Creature* fire : found)
            if (fire && fire->IsAlive() && fire->GetGUID() == hold.fire)
            {
                pick = fire;
                break;
            }

    if (!pick)
    {
        float bestDist = 0.0f;
        for (Creature* fire : found)
        {
            if (!fire || !fire->IsAlive())
                continue;

            float const dist = fire->GetExactDist2d(&ULDUAR_HODIR_CENTRE);
            if (dist > ULDUAR_HODIR_HOLD_FIRE_LEASH)
                continue;

            if (!pick || dist < bestDist)
            {
                pick = fire;
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
        hold.point = pick ? pick->GetPosition() : ULDUAR_HODIR_CENTRE;
        hold.offSince = 0;

        // He trails the tank, so the tank stands past the point on the side away from him and he stops
        // on it. Right on top of the point there is no side, so it falls back to a fixed bearing.
        hold.bearing = hodir->GetExactDist2d(&hold.point) > ULDUAR_HODIR_HOLD_BEARING_MIN_GAP
                           ? std::atan2(hold.point.GetPositionY() - hodir->GetPositionY(),
                                        hold.point.GetPositionX() - hodir->GetPositionX())
                           : ULDUAR_HODIR_HOLD_FALLBACK_BEARING;
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

    if (RaidObs::Active())
    {
        // Which fire, and where he is being held. Two probes because they change on different
        // schedules: the fire swaps once a minute, the point also moves when the bearing is re-aimed.
        RaidObs::NoteDerived(bot, "hodir.tankhold",
                             pick ? RaidObs::DescribeAssignment(fire) : std::string("centre"));
        RaidObs::NoteDerived(bot, "hodir.hold", RaidObs::DescribeDerived(hold.point));
    }

    return &hold;
}

// Where a tank stands so Hodir stops on the hold point. The main tank goes past it on the far side
// from where he came, and he trails onto it. The off-tank stands a quarter turn off that line, so a
// Frozen Blows taunt walks him about 6 yd sideways and leaves him on the fire either way.
//
// Static floor check: the campfire gameobject a fire summons is in the dynamic tree, so the ordinary
// one clips a spot behind a fire back to the fire's near edge - which is exactly the spot this asks
// for, and how the tank ended up between Hodir and the fire he was meant to stand on.
static Position HodirTankSpot(Player* bot, HodirHoldLatch const& hold, bool offTank)
{
    float const angle = offTank ? hold.bearing - static_cast<float>(M_PI) / 2.0f : hold.bearing;
    return ValidateStaticFloorPoint(
        bot, Position(hold.point.GetPositionX() + std::cos(angle) * ULDUAR_HODIR_HOLD_TANK_OFFSET,
                      hold.point.GetPositionY() + std::sin(angle) * ULDUAR_HODIR_HOLD_TANK_OFFSET,
                      hold.point.GetPositionZ()));
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
        entry.rally = ValidateStaticFloorPoint(bot, carrier->GetPosition());
    }

    out = entry.rally;
    return true;
}

// A bot already holding a buff has something to lose by walking, so it keeps the short leash.
static float HodirBuffWalk(Player* bot)
{
    return bot->HasAura(SPELL_HODIR_STARLIGHT) || bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA)
               ? ULDUAR_HODIR_BUFF_WALK
               : ULDUAR_HODIR_BUFF_WALK_UNBUFFED;
}

// Where this bot stands if there is a Starlight zone it can use. Starlight is +50% to cast time and
// all three attack timers, the fight's biggest throughput lever, and it is worth stacking for: one
// Ice Shards hit is 41% of a health pool (p90 54%), so an icicle catching two bots in one zone costs
// two heals rather than two lives. Any number may share a zone; what each gets is a bearing of its
// own, taken from where it stands, so they spread around it instead of piling on a point.
//
// Usable means the resulting spot still reaches Hodir, is out of his melee range, and is within the
// walk this bot will make for a buff; the nearest zone wins among those.
//
// The pick is latched per bot and held until the zone expires, because the boss moves under it. A
// stand that stops qualifying is held for ULDUAR_HODIR_STARLIGHT_REJECT_DROP_MS, then dropped. Dropped
// at once, a fresh sweep while Hodir is a yard past the band hands the bot a different zone that
// rejects on the next tick and back: 1201 anchor moves a median 320ms apart. Never dropped, casters sat
// on a zone they couldn't use for a third of the pull.
static bool FindHodirStarlightStand(PlayerbotAI* botAI, Player* bot, Position& out)
{
    std::unordered_map<ObjectGuid, HodirStarlightLatch>& latched = HodirLatchesFor(bot).starlight;

    // Bounded rather than the room radius: this runs per bot per tick, and a zone further out than
    // this is well past what the bot would walk for anyway.
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
    // that was good when it was picked can stop being one. The walk gate is not in here: it decides
    // which zone is worth starting for, and a bot already on its way is past that question.
    auto standRejects = [&](Position const& stand) -> char const*
    {
        // Starlight's own inner edge and the caster band's outer one. A zone the bot cannot shoot the
        // boss from is not a throughput lever, whatever haste it carries.
        if (hodir)
        {
            float const gap = stand.GetExactDist2d(hodir);
            if (gap < ULDUAR_HODIR_STARLIGHT_MIN_BOSS_GAP || gap > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
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
            // Held through a short reject, dropped after a long one, for the reason above the function.
            if (char const* reject = standRejects(held->second.stand))
            {
                uint32 const now = getMSTime();
                if (!held->second.rejectedSince)
                    held->second.rejectedSince = now;

                if (getMSTimeDiff(held->second.rejectedSince, now) < ULDUAR_HODIR_STARLIGHT_REJECT_DROP_MS)
                {
                    // "held" so a latched stand standing down reads differently from a sweep that found
                    // nothing: the first is a bot with a zone waiting for the boss to move back, the
                    // second is a bot with no zone at all, and only the second wants more zones.
                    if (RaidObs::Active())
                        RaidObs::NoteDerived(bot, "hodir.starlight", std::string("held ") + reject);

                    return false;
                }
            }
            else
            {
                held->second.rejectedSince = 0;
                out = held->second.stand;

                if (RaidObs::Active())
                    RaidObs::NoteDerived(bot, "hodir.starlight",
                                         "stand " + RaidObs::DescribeDerived(held->second.zone));

                return true;
            }
        }

        // The zone expired, or its stand stayed out of the band too long, so the next sweep counts.
        latched.erase(held);
    }

    bool found = false;
    float bestWalk = 0.0f;
    Position bestZone;

    for (Position const& zone : zones)
    {
        float const walk = bot->GetExactDist2d(&zone);
        if (walk > HodirBuffWalk(bot))
        {
            if (!found)
                how = "far";

            continue;
        }

        if (found && walk >= bestWalk)
            continue;

        float const bearing = std::atan2(bot->GetPositionY() - zone.GetPositionY(),
                                         bot->GetPositionX() - zone.GetPositionX());

        Position const stand = ValidateStaticFloorPoint(
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

// Where this bot stands if a Toasty Fire is close enough to be worth the walk: the point nearest it
// that is inside ULDUAR_HODIR_FIRE_STAND_RADIUS of the fire, which is the bot's own spot once it is
// already in one. Second choice behind Starlight - +50% haste beats what a fire gives - but the two
// are not exclusive, and a bot that has neither is walking for Biting Cold and procs nothing.
//
// The fire Hodir is held on needs no special case: with him standing on it, nothing within 8 yd of it
// is 15 yd from him, so the caster band throws it out on its own.
static bool FindHodirFireStand(PlayerbotAI* botAI, Player* bot, Position& out)
{
    std::unordered_map<ObjectGuid, HodirFireLatch>& latched = HodirLatchesFor(bot).fireStand;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_TOASTY_FIRE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    Unit* hodir = GetHodir(botAI);

    // Both ends of the caster band: a spot the bot cannot shoot him from is worth nothing, and one
    // closer in puts it among the melee.
    auto standRejects = [&](Position const& stand)
    {
        if (!hodir)
            return false;

        float const gap = stand.GetExactDist2d(hodir);
        return gap < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP || gap > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP;
    };

    // Nearest point of this fire to the bot, capped at the stand radius so it never sits on the aura's
    // edge. A bot already inside keeps the spot it is on, which is the whole walk saved.
    auto standFor = [&](Creature* fire)
    {
        float const gap = bot->GetExactDist2d(fire);
        if (gap <= ULDUAR_HODIR_FIRE_STAND_RADIUS)
            return bot->GetPosition();

        float const bearing = std::atan2(bot->GetPositionY() - fire->GetPositionY(),
                                         bot->GetPositionX() - fire->GetPositionX());
        return ValidateStaticFloorPoint(
            bot, Position(fire->GetPositionX() + std::cos(bearing) * ULDUAR_HODIR_FIRE_STAND_RADIUS,
                          fire->GetPositionY() + std::sin(bearing) * ULDUAR_HODIR_FIRE_STAND_RADIUS,
                          fire->GetPositionZ()));
    };

    auto held = latched.find(bot->GetGUID());
    if (held != latched.end())
    {
        for (Creature* fire : found)
            if (fire && fire->IsAlive() && fire->GetGUID() == held->second.fire)
            {
                if (standRejects(held->second.stand))
                    return false;

                out = held->second.stand;
                return true;
            }

        // The fire burned out or was put out by a Flash Freeze, so the sweep below is what counts.
        latched.erase(held);
    }

    bool picked = false;
    float bestWalk = 0.0f;
    ObjectGuid bestFire;

    for (Creature* fire : found)
    {
        if (!fire || !fire->IsAlive())
            continue;

        float const walk = bot->GetExactDist2d(fire);
        if (walk > HodirBuffWalk(bot) + ULDUAR_HODIR_FIRE_STAND_RADIUS)
            continue;

        if (picked && walk >= bestWalk)
            continue;

        Position const stand = standFor(fire);
        if (bot->GetExactDist2d(&stand) > HodirBuffWalk(bot) || standRejects(stand))
            continue;

        out = stand;
        bestFire = fire->GetGUID();
        bestWalk = walk;
        picked = true;
    }

    if (picked)
        latched[bot->GetGUID()] = HodirFireLatch{bestFire, out};

    return picked;
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

static bool DeriveHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out, char const*& how)
{
    if (!bot)
        return false;

    bool const mainTank = botAI->IsMainTank(bot);
    if (mainTank || botAI->IsAssistTankOfIndex(bot, 0, true))
    {
        // Two points either side of the hold spot rather than wandering, because Hodir follows. Square
        // to the line he is held on, so neither end drags him off the fire, and 6 yd apart covers two
        // aura ticks.
        HodirHoldLatch const* hold = DeriveHodirHold(botAI, bot);
        if (!hold)
            return false;

        Position const spot = HodirTankSpot(bot, *hold, !mainTank);

        float const axis = hold->bearing + static_cast<float>(M_PI) / 2.0f;
        float const dx = std::cos(axis) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;
        float const dy = std::sin(axis) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;

        Position const legA = ValidateStaticFloorPoint(
            bot, Position(spot.GetPositionX() + dx, spot.GetPositionY() + dy, spot.GetPositionZ()));
        Position const legB = ValidateStaticFloorPoint(
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

        Position const legA = ValidateStaticFloorPoint(
            bot, Position(shelter->GetPositionX() + dx, shelter->GetPositionY() + dy, shelter->GetPositionZ()));
        Position const legB = ValidateStaticFloorPoint(
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

        Position const legA = ValidateStaticFloorPoint(
            bot, Position(zone.GetPositionX() + dx, zone.GetPositionY() + dy, zone.GetPositionZ()));
        Position const legB = ValidateStaticFloorPoint(
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

    // At the room's edge the collision check pulls every probe on that side back onto the bot's own
    // spot. That spot is clear of everything, so it can win, MoveTo refuses it as already there and
    // the bot stands still while the stacks climb.
    auto const goesSomewhere = [bot](float x, float y)
    { return bot->GetExactDist2d(x, y) >= ULDUAR_HODIR_SHUTTLE_HALF_LEG; };

    Position leg = FindNearestPositionClearOfHazards(
        bot, sweepFor(ULDUAR_HODIR_DECLUMP_RADIUS, ULDUAR_HODIR_ICE_SHARDS_CLEAR, ULDUAR_HODIR_BIG_SHARDS_CLEAR),
        ULDUAR_HODIR_DODGE_LEASH, 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG, static_cast<float>(M_PI) / 8.0f,
        preferNear, goesSomewhere);

    // Nothing clear with margin. The clears carry 2 yd over the radius that actually kills and the
    // declump only stops two bots sharing one icicle, so both are worth giving up before standing
    // still: a bot that cannot move sheds nothing.
    if (!leg.GetPositionX() && !leg.GetPositionY())
        leg = FindNearestPositionClearOfHazards(
            bot,
            sweepFor(ULDUAR_HODIR_SHUTTLE_HALF_LEG, ULDUAR_HODIR_ICE_SHARDS_RADIUS + 0.5f,
                     ULDUAR_HODIR_BIG_SHARDS_RADIUS + 0.5f),
            2.0f * ULDUAR_HODIR_DODGE_LEASH, 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG,
            static_cast<float>(M_PI) / 8.0f, preferNear, goesSomewhere);

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

    // Ranged and healers get a buff or nothing. No boss mechanic asks them to stand anywhere in
    // particular, so a home spot only ever walks them back out of the zone they just reached - which
    // is what kept them in Starlight or a fire for 23-24% of a pull while one was in reach on half of
    // their samples.
    //
    // Starlight first: +50% to every cast and swing beats what a fire gives, and the two are not
    // exclusive anyway. Neither is gated on already holding the aura - that would hand the bot a new
    // destination the moment the buff landed and start the trip again.
    bool found = false;
    char const* kind = "none";
    Position stand;
    if (FindHodirStarlightStand(botAI, bot, stand))
    {
        out = stand;
        tolerance = ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE;
        kind = "starlight";
        found = true;
    }
    else if (FindHodirFireStand(botAI, bot, stand))
    {
        out = stand;
        tolerance = ULDUAR_HODIR_FIRE_STAND_TOLERANCE;
        kind = "fire";
        found = true;
    }

    // Which buff, if any. hodir.anchor carries the point and hodir.starlight the rule that kept a bot
    // out of a zone; neither says whether the bot ended up on a fire instead.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.stand", kind);

    return found;
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

    std::unordered_map<ObjectGuid, Position>& ranks = HodirLatchesFor(bot).blockRank;
    if (blocks.empty())
    {
        ranks.clear();
        return nullptr;
    }

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

    // While a mage is still iced the wave is holding up the next fire, so it gets the ranged group.
    // After that, what is left on ice is worth a few bots and the rest belong back on the boss.
    bool const mageIced = std::any_of(blocks.begin(), blocks.end(), [](Creature* block)
                                      { return GetHodirHelperKind(block) == HodirHelperKind::Mage; });

    size_t const total = candidates.size();
    size_t budget = std::min<size_t>(
        mageIced ? ULDUAR_HODIR_HELPER_BLOCK_BREAKERS : ULDUAR_HODIR_LATE_BLOCK_BREAKERS, seats.size());
    if (total > ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE)
        budget = std::min(budget, total - ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE);
    else
        budget = std::min<size_t>(budget, 1);

    // Rank on where each bot stood when the wave went up, latched until no block is left. Live
    // distances re-shuffle the assignment every tick and bots flick between a block and the boss; a
    // guid rotation holds still but hands blocks to bots across the room, and they were spending
    // 61.6% of their time on ice walking to one. The latch is both: every bot reads the same numbers
    // for as long as the wave lasts, and they are the positions that were actually nearest.
    std::vector<Position> spots(total);
    for (size_t i = 0; i < total; ++i)
    {
        auto const known = ranks.find(candidates[i]->GetGUID());
        if (known != ranks.end())
            spots[i] = known->second;
        else
            spots[i] = ranks[candidates[i]->GetGUID()] = candidates[i]->GetPosition();
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
