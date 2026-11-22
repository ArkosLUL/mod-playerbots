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
#include "MotionMaster.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "Unit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace EncounterHelpers;

const Position ULDUAR_HODIR_CENTRE = Position(1998.0f, -235.5f, 432.687f);

constexpr float ULDUAR_HODIR_HOLD_FALLBACK_BEARING = 1.0845f;

Unit* GetHodir(PlayerbotAI* botAI) { return GetUldUnitByEntry(botAI, NPC_HODIR); }

bool IsHodirEngaged(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);
    return boss && boss->IsInCombat();
}

bool IsHodirFlashFreezeIncoming(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);

    return boss && boss->HasUnitState(UNIT_STATE_CASTING) &&
           boss->FindCurrentSpellBySpellId(SPELL_FLASH_FREEZE) != nullptr;
}

bool IsHodirLustHeld(PlayerbotAI* botAI)
{
    Unit* boss = GetHodir(botAI);
    if (!boss || !boss->IsInCombat())
        return false;

    Aura const* timer = boss->GetAura(SPELL_HODIR_SHATTER_CHEST_TIMER);
    if (!timer ||
        timer->GetMaxDuration() - timer->GetDuration() >= static_cast<int32>(ULDUAR_HODIR_LUST_FALLBACK_MS))
        return false;

    return !botAI->GetBot()->FindNearestCreature(NPC_TOASTY_FIRE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS, true);
}

static Aura const* HodirFrozenBlowsAura(PlayerbotAI* botAI, Player* bot)
{
    Unit* boss = GetHodir(botAI);
    return boss && bot ? boss->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_FROZEN_BLOWS, bot)) : nullptr;
}

bool HodirFrozenBlowsActive(PlayerbotAI* botAI, Player* bot) { return HodirFrozenBlowsAura(botAI, bot) != nullptr; }

bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || bot->GetHealthPct() >= ULDUAR_HODIR_TAUNT_HEALTH_FLOOR)
        return false;

    if (!HodirFrozenBlowsActive(botAI, bot))
        return false;

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

struct HodirStarlightLatch
{
    Position zone;
    Position stand;
    uint32 rejectedSince = 0;
};

struct HodirFireLatch
{
    ObjectGuid fire;
    Position stand;
    uint32 rejectedSince = 0;
};

struct HodirStormRally
{
    time_t applied = 0;
    Position rally;
};

struct HodirHoldLatch
{
    bool set = false;
    ObjectGuid fire;
    Position point;
    float bearing = 0.0f;
    uint32 offSince = 0;
};

struct HodirShedHopLatch
{
    Position home;
    float axis = 0.0f;
    Position to;
};

struct HodirTankDefensiveLatch
{
    uint32 victimSince = 0;
    time_t spentWindow = 0;
};

struct HodirBotLatches
{
    std::unordered_map<ObjectGuid, HodirTankDefensiveLatch> tankDefensive;
    std::unordered_map<ObjectGuid, ObjectGuid> shelter;
    std::unordered_map<ObjectGuid, HodirStarlightLatch> starlight;
    std::unordered_map<ObjectGuid, HodirFireLatch> fireStand;
    std::unordered_map<ObjectGuid, HodirStormRally> stormRally;
    std::unordered_map<ObjectGuid, HodirShedHopLatch> shedHop;

    std::unordered_map<ObjectGuid, Position> blockRank;

    HodirHoldLatch hold;
};

static RaidInstanceState<HodirBotLatches> hodirLatches;

static HodirBotLatches& HodirLatchesFor(Player* bot) { return hodirLatches.For(bot->GetInstanceId()); }

uint32 GetHodirTankDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind)
{
    if (!bot || !bot->IsAlive() || !PlayerbotAI::IsTank(bot) || !IsHodirEngaged(botAI))
        return 0;

    Aura const* blows = HodirFrozenBlowsAura(botAI, bot);
    HodirTankDefensiveLatch& latch = HodirLatchesFor(bot).tankDefensive[bot->GetGUID()];

    char const* waiting = nullptr;
    if (!blows || GetHodir(botAI)->GetVictim() != bot)
    {
        latch.victimSince = 0;
        waiting = "idle";
    }
    else if (latch.spentWindow == blows->GetApplyTime())
        waiting = "spent";
    else
    {
        uint32 const now = getMSTime();
        if (!latch.victimSince)
            latch.victimSince = now;

        if (getMSTimeDiff(latch.victimSince, now) < ULDUAR_HODIR_TANK_DEFENSIVE_HOLD_MS)
            waiting = "wait";
    }

    if (waiting)
    {
        if (noteKind && RaidObs::Active())
            RaidObs::NoteDerived(bot, noteKind, waiting);

        return 0;
    }

    return NextTankDefensive(botAI, bot, noteKind, false);
}

void LatchHodirTankDefensive(PlayerbotAI* botAI, Player* bot)
{
    if (Aura const* blows = HodirFrozenBlowsAura(botAI, bot))
        HodirLatchesFor(bot).tankDefensive[bot->GetGUID()].spentWindow = blows->GetApplyTime();
}

bool IsHodirTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsHodirEngaged(botAI) && botAI->GetBot()->GetHealthPct() >= ULDUAR_HODIR_TANK_DEFENSIVE_LAST_RESORT_PCT;
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

    if (auto held = latched.find(bot->GetGUID()); held != latched.end())
        for (Creature* candidate : candidates)
            if (candidate->GetGUID() == held->second)
            {
                best = candidate;
                break;
            }

    if (!best)
    {
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

    bool const engaged = hodir->IsInCombat();
    if (!engaged || !hold.set || hold.fire != fire)
    {
        hold.set = engaged;
        hold.fire = fire;
        hold.point = pick ? pick->GetPosition() : ULDUAR_HODIR_CENTRE;
        hold.offSince = 0;

        hold.bearing = hodir->GetExactDist2d(&hold.point) > ULDUAR_HODIR_HOLD_BEARING_MIN_GAP
                           ? std::atan2(hold.point.GetPositionY() - hodir->GetPositionY(),
                                        hold.point.GetPositionX() - hodir->GetPositionX())
                           : ULDUAR_HODIR_HOLD_FALLBACK_BEARING;
    }
    else if (!hodir->isMoving() && hodir->GetExactDist2d(&hold.point) > ULDUAR_HODIR_HOLD_REAIM_GAP)
    {
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
        RaidObs::NoteDerived(bot, "hodir.tankhold",
                             pick ? RaidObs::DescribeAssignment(fire) : std::string("centre"));
        RaidObs::NoteDerived(bot, "hodir.hold", RaidObs::DescribeDerived(hold.point));
    }

    return &hold;
}

static Position HodirTankSpot(Player* bot, Unit* hodir, HodirHoldLatch const& hold, bool offTank)
{
    float const offset = hodir->GetCombatReach() + bot->GetCombatReach() + CONTACT_DISTANCE;
    float const angle = offTank ? hold.bearing - static_cast<float>(M_PI) / 2.0f : hold.bearing;
    return ValidateStaticFloorPoint(
        bot, Position(hold.point.GetPositionX() + std::cos(angle) * offset,
                      hold.point.GetPositionY() + std::sin(angle) * offset, hold.point.GetPositionZ()));
}

Player* GetHodirStormCloudCarrier(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !GetHodir(botAI))
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    uint32 const cloud = sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot);

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

bool GetHodirStormCloudRally(Player* bot, Player* carrier, Position& out)
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

        entry.rally = ValidateStaticFloorPoint(bot, carrier->GetPosition());
    }

    out = entry.rally;
    return true;
}

bool GetHodirStormGather(PlayerbotAI* botAI, Player* bot, HodirStormGather& out)
{
    if (!bot || !GetHodir(botAI))
        return false;

    if (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsHeal(bot))
        return false;

    if (bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_POWER, bot)))
        return false;

    Player* carrier = GetHodirStormCloudCarrier(botAI, bot);
    if (!carrier || carrier == bot || !GetHodirStormCloudRally(bot, carrier, out.rally))
        return false;

    out.dist = bot->GetExactDist2d(&out.rally);
    if (out.dist > ULDUAR_HODIR_STORM_CLOUD_COLLECT_LEASH)
        return false;

    Aura const* cloud = carrier->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, carrier));
    AuraEffect const* pulse = cloud ? cloud->GetEffect(EFFECT_0) : nullptr;
    if (!pulse || pulse->GetAmplitude() <= 0)
        return false;

    int32 const period = pulse->GetAmplitude();
    out.msToPulse = (static_cast<int32>(ULDUAR_HODIR_STORM_GATHER_TICK) - static_cast<int32>(pulse->GetTickNumber()) - 1) *
                        period +
                    std::clamp(pulse->GetPeriodicTimer(), 0, period);

    float const speed = std::max(bot->GetSpeed(MOVE_RUN), 1.0f);
    float const walk = std::max(0.0f, out.dist - ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK);
    out.msToWalk = out.msToPulse - static_cast<int32>(walk / speed * IN_MILLISECONDS) - ULDUAR_HODIR_STORM_GATHER_LEAD_MS;
    return true;
}

bool IsHodirStormGatherHeld(PlayerbotAI* botAI)
{
    HodirStormGather gather;
    return IsHodirEngaged(botAI) && GetHodirStormGather(botAI, botAI->GetBot(), gather) && gather.msToWalk <= 0 &&
           gather.msToPulse > -ULDUAR_HODIR_STORM_GATHER_HOLD_MS &&
           gather.dist <= ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE;
}

int32 GetHodirStormGatherCastSlackMs(PlayerbotAI* botAI, Player* bot)
{
    HodirStormGather gather;
    if (!GetHodirStormGather(botAI, bot, gather) || gather.dist <= ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE ||
        gather.msToPulse <= -ULDUAR_HODIR_STORM_GATHER_HOLD_MS || bot->HasUnitState(UNIT_STATE_ROOT))
        return std::numeric_limits<int32>::max();

    return std::max(gather.msToWalk, 0);
}

static float HodirBuffWalk(Player* bot)
{
    return bot->HasAura(SPELL_HODIR_STARLIGHT) || bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA)
               ? ULDUAR_HODIR_BUFF_WALK
               : ULDUAR_HODIR_BUFF_WALK_UNBUFFED;
}

static bool FindHodirStarlightStand(PlayerbotAI* botAI, Player* bot, Position& out)
{
    std::unordered_map<ObjectGuid, HodirStarlightLatch>& latched = HodirLatchesFor(bot).starlight;

    std::vector<Position> const zones =
        GetDynamicObjectPositions(bot, ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT);
    if (zones.empty())
    {
        latched.erase(bot->GetGUID());

        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "hodir.starlight", "none");

        return false;
    }

    Unit* hodir = GetHodir(botAI);

    char const* how = "none";

    auto standRejects = [&](Position const& stand) -> char const*
    {
        if (hodir)
        {
            float const gap = stand.GetExactDist2d(hodir);
            if (gap < hodir->GetMeleeRange(bot) || gap > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
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
            if (char const* reject = standRejects(held->second.stand))
            {
                uint32 const now = getMSTime();
                if (!held->second.rejectedSince)
                    held->second.rejectedSince = now;

                if (getMSTimeDiff(held->second.rejectedSince, now) < ULDUAR_HODIR_STAND_REJECT_DROP_MS)
                {
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

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.starlight",
                             found ? "stand " + RaidObs::DescribeDerived(bestZone) : std::string(how));

    return found;
}

static bool FindHodirFireStand(PlayerbotAI* botAI, Player* bot, Position& out)
{
    std::unordered_map<ObjectGuid, HodirFireLatch>& latched = HodirLatchesFor(bot).fireStand;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_TOASTY_FIRE, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);

    Unit* hodir = GetHodir(botAI);

    auto standRejects = [&](Position const& stand)
    {
        if (!hodir)
            return false;

        float const gap = stand.GetExactDist2d(hodir);
        return gap < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP || gap > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP;
    };

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
        bool const burning = std::any_of(found.begin(), found.end(), [&](Creature* fire)
                                         { return fire && fire->IsAlive() && fire->GetGUID() == held->second.fire; });
        if (burning && !standRejects(held->second.stand))
        {
            held->second.rejectedSince = 0;
            out = held->second.stand;

            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "hodir.firestand", "stand " + RaidObs::DescribeAssignment(held->second.fire));

            return true;
        }

        if (burning)
        {
            uint32 const now = getMSTime();
            if (!held->second.rejectedSince)
                held->second.rejectedSince = now;

            if (getMSTimeDiff(held->second.rejectedSince, now) < ULDUAR_HODIR_STAND_REJECT_DROP_MS)
            {
                if (RaidObs::Active())
                    RaidObs::NoteDerived(bot, "hodir.firestand", "held noreach");

                return false;
            }
        }

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

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.firestand",
                             picked ? "stand " + RaidObs::DescribeAssignment(bestFire) : std::string("none"));

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

    bool const mainTank = PlayerbotAI::IsMainTank(bot);
    if (mainTank || PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
    {
        HodirHoldLatch const* hold = DeriveHodirHold(botAI, bot);
        if (!hold)
            return false;

        Position const spot = HodirTankSpot(bot, GetHodir(botAI), *hold, !mainTank);

        float const axis = mainTank ? hold->bearing + static_cast<float>(M_PI) / 2.0f : hold->bearing;
        float const dx = std::cos(axis) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;
        float const dy = std::sin(axis) * ULDUAR_HODIR_SHUTTLE_HALF_LEG;

        Position const legA = ValidateStaticFloorPoint(
            bot, Position(spot.GetPositionX() + dx, spot.GetPositionY() + dy, spot.GetPositionZ()));
        Position const legB = ValidateStaticFloorPoint(
            bot, Position(spot.GetPositionX() - dx, spot.GetPositionY() - dy, spot.GetPositionZ()));

        out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
        how = "tank";
        return true;
    }

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

    bool crowded = false;
    auto sweepFor = [&](float declump, float smallClear, float bigClear)
    {
        std::vector<HazardCircle> hazards;

        for (ObjectGuid const& guid :
             botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get())
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

        if (!PlayerbotAI::IsMelee(bot))
            if (Unit* hodir = GetHodir(botAI))
                hazards.emplace_back(hodir->GetPosition(), ULDUAR_HODIR_RANGED_MIN_BOSS_GAP);

        return hazards;
    };

    Position preference;
    Position const* preferNear = GetHodirDodgePreference(botAI, bot, preference) ? &preference : nullptr;

    auto const goesSomewhere = [bot](float x, float y)
    { return bot->GetExactDist2d(x, y) >= ULDUAR_HODIR_SHUTTLE_HALF_LEG; };

    Position leg = FindNearestPositionClearOfHazards(
        bot, sweepFor(ULDUAR_HODIR_DECLUMP_RADIUS, ULDUAR_HODIR_ICE_SHARDS_CLEAR, ULDUAR_HODIR_BIG_SHARDS_CLEAR),
        ULDUAR_HODIR_DODGE_LEASH, 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG, ULDUAR_HODIR_SWEEP_ANGLE_STEP, preferNear,
        goesSomewhere);

    if (!leg.GetPositionX() && !leg.GetPositionY())
        leg = FindNearestPositionClearOfHazards(
            bot,
            sweepFor(ULDUAR_HODIR_SHUTTLE_HALF_LEG, ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN,
                     ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN),
            2.0f * ULDUAR_HODIR_DODGE_LEASH, 2.0f * ULDUAR_HODIR_SHUTTLE_HALF_LEG, ULDUAR_HODIR_SWEEP_ANGLE_STEP,
            preferNear, goesSomewhere);

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

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shuttle", how);

    return found;
}

static float HodirShedHopAxis(PlayerbotAI* botAI, Player* bot)
{
    auto const bearingFrom = [bot](Position const& from)
    {
        return std::atan2(bot->GetPositionY() - from.GetPositionY(), bot->GetPositionX() - from.GetPositionX());
    };
    float const square = static_cast<float>(M_PI) / 2.0f;

    Unit* hodir = GetHodir(botAI);
    bool const offHodir = hodir && bot->GetExactDist2d(hodir) >= ULDUAR_HODIR_SHED_HOP_MIN_TRAVEL;
    bool const tank = PlayerbotAI::IsTank(bot);
    if (offHodir && PlayerbotAI::IsMelee(bot) && !tank)
        return bearingFrom(hodir->GetPosition());

    Position zone;
    if (!tank && GetHodirStarlightZoneAt(botAI, bot, zone) &&
        bot->GetExactDist2d(&zone) >= ULDUAR_HODIR_SHED_HOP_MIN_TRAVEL)
        return bearingFrom(zone) + square;

    if (offHodir)
        return bearingFrom(hodir->GetPosition()) + square;

    return bot->GetOrientation() + square;
}

bool GetHodirShedHop(PlayerbotAI* botAI, Player* bot, Position& out, uint32& flightMs)
{
    Aura* cold = bot ? bot->GetAura(SPELL_BITING_COLD_PLAYER_AURA) : nullptr;
    if (!cold)
        return false;

    auto& latches = HodirLatchesFor(bot).shedHop;
    auto latched = latches.find(bot->GetGUID());
    if (latched == latches.end() || bot->GetExactDist2d(&latched->second.home) > ULDUAR_HODIR_SHED_HOP_HOME_SLIP)
    {
        Position const to = latched == latches.end() ? Position() : latched->second.to;
        latched = latches
                      .insert_or_assign(bot->GetGUID(),
                                        HodirShedHopLatch{bot->GetPosition(), HodirShedHopAxis(botAI, bot), to})
                      .first;
    }

    Position const& home = latched->second.home;

    if (PlayerbotAI::IsMelee(bot) && !PlayerbotAI::IsTank(bot))
        if (Unit* victim = bot->GetVictim())
            if (victim->GetExactDist2d(&home) > bot->GetMeleeRange(victim))
                return false;

    float const dx = std::cos(latched->second.axis) * ULDUAR_HODIR_SHED_HOP_HALF;
    float const dy = std::sin(latched->second.axis) * ULDUAR_HODIR_SHED_HOP_HALF;
    std::array<Position, 2> ends = {
        ValidateStaticFloorPoint(bot, Position(home.GetPositionX() + dx, home.GetPositionY() + dy, home.GetPositionZ())),
        ValidateStaticFloorPoint(bot, Position(home.GetPositionX() - dx, home.GetPositionY() - dy, home.GetPositionZ()))};

    if (bot->GetExactDist2d(&ends[1]) > bot->GetExactDist2d(&ends[0]))
        std::swap(ends[0], ends[1]);

    bool picked = false;
    for (Position const& end : ends)
    {
        if (bot->GetExactDist2d(&end) < ULDUAR_HODIR_SHED_HOP_MIN_TRAVEL)
            continue;

        if (!PlayerbotAI::IsTank(bot) &&
            IsHodirWalkThroughLiveIcicle(bot, end, ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN,
                                         ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN))
            continue;

        out = end;
        picked = true;
        break;
    }

    if (!picked)
        return false;

    AuraEffect const* tick = cold->GetEffect(EFFECT_0);
    int32 const period = tick && tick->GetAmplitude() > 0 ? tick->GetAmplitude()
                                                          : static_cast<int32>(ULDUAR_HODIR_BITING_COLD_TICK_MS);
    int32 const next = tick ? std::clamp(tick->GetPeriodicTimer(), 0, period) : period;
    flightMs = static_cast<uint32>(next + period) + ULDUAR_HODIR_SHED_HOP_TICK_MARGIN_MS;
    return true;
}

void LatchHodirShedHop(Player* bot, Position const& to)
{
    auto& latches = HodirLatchesFor(bot).shedHop;
    auto const latched = latches.find(bot->GetGUID());
    if (latched != latches.end())
        latched->second.to = to;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.shuttle", "hop");
}

bool IsHodirShedHopInFlight(Player* bot)
{
    if (!bot || bot->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_CONTROLLED) != EFFECT_MOTION_TYPE)
        return false;

    auto const& latches = HodirLatchesFor(bot).shedHop;
    auto const latched = latches.find(bot->GetGUID());
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    return latched != latches.end() && bot->GetMotionMaster()->GetDestination(x, y, z) &&
           latched->second.to.GetExactDist2d(x, y) < 0.1f;
}

void DropHodirShedHop(Player* bot)
{
    auto& latches = HodirLatchesFor(bot).shedHop;
    auto const latched = latches.find(bot->GetGUID());
    if (latched != latches.end())
        latched->second.home = Position();
}

bool IsHodirBitingColdShedArmed(PlayerbotAI* botAI, Player* bot)
{
    if (!bot)
        return false;

    if (bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA))
        return false;

    Aura* cold = bot->GetAura(SPELL_BITING_COLD_PLAYER_AURA);
    if (!cold)
        return false;

    uint32 arm = bot->HasAura(SPELL_HODIR_STARLIGHT) ? ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT
                                                     : ULDUAR_HODIR_BITING_COLD_SHED_STACKS;
    if (HodirFrozenBlowsActive(botAI, bot))
        arm -= ULDUAR_HODIR_BITING_COLD_SHED_STACKS_FROZEN_BLOWS_DROP;

    return cold->GetStackAmount() >= arm;
}

static bool DeriveHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    if (!GetHodir(botAI))
        return false;

    bool const mainTank = PlayerbotAI::IsMainTank(bot);
    if (mainTank || PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
    {
        HodirHoldLatch const* hold = DeriveHodirHold(botAI, bot);
        if (!hold)
            return false;

        out = HodirTankSpot(bot, GetHodir(botAI), *hold, !mainTank);
        tolerance = ULDUAR_HODIR_TANK_HOLD_TOLERANCE;
        return true;
    }

    if (!PlayerbotAI::IsRanged(bot))
        return false;

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

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.stand", kind);

    return found;
}

bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    bool const found = DeriveHodirAnchor(botAI, bot, out, tolerance);

    if (RaidObs::Active())
    {
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

bool IsHodirTrappedAllyBreaker(Player* bot, Unit* block)
{
    if (!block)
        return false;

    bool breaker = false;
    Group* group = bot->GetGroup();
    if (!group)
        breaker = true;
    else
    {
        std::vector<Player*> candidates;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
                continue;

            PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
            if (!memberAI || PlayerbotAI::IsHeal(member) || PlayerbotAI::IsTank(member))
                continue;

            if (member->GetExactDist2d(block) > ULDUAR_HODIR_TRAPPED_ALLY_RANGE)
                continue;

            candidates.push_back(member);
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); });

        size_t const total = candidates.size();
        size_t const offset = total ? block->GetGUID().GetCounter() % total : 0;

        for (size_t i = 0; i < total && i < ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS; ++i)
            if (candidates[(offset + i) % total] == bot)
            {
                breaker = true;
                break;
            }
    }

    return breaker;
}

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

    if (boss->GetHealthPct() < ULDUAR_HODIR_LAST_WAVE_HEALTH_PCT)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    std::vector<Player*> ranged;
    std::vector<Player*> rest;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || PlayerbotAI::IsHeal(member) || PlayerbotAI::IsTank(member))
            continue;

        if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
        else
            rest.push_back(member);
    }

    std::vector<Player*>& candidates = ranged.empty() ? rest : ranged;

    if (std::find(candidates.begin(), candidates.end(), bot) == candidates.end())
        return nullptr;

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

    std::vector<Creature*> seats;
    for (Creature* block : blocks)
    {
        uint32 const want =
            GetHodirHelperKind(block) == HodirHelperKind::Mage ? ULDUAR_HODIR_MAGE_BLOCK_BREAKERS : 1;
        for (uint32 i = 0; i < want; ++i)
            seats.push_back(block);
    }

    bool const mageIced = std::any_of(blocks.begin(), blocks.end(), [](Creature* block)
                                      { return GetHodirHelperKind(block) == HodirHelperKind::Mage; });

    size_t const total = candidates.size();
    size_t budget = std::min<size_t>(
        mageIced ? ULDUAR_HODIR_HELPER_BLOCK_BREAKERS : ULDUAR_HODIR_LATE_BLOCK_BREAKERS, seats.size());
    if (total > ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE)
        budget = std::min(budget, total - ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE);
    else
        budget = std::min<size_t>(budget, 1);

    std::vector<Position> spots(total);
    for (size_t i = 0; i < total; ++i)
    {
        auto const known = ranks.find(candidates[i]->GetGUID());
        if (known != ranks.end())
            spots[i] = known->second;
        else
            spots[i] = ranks[candidates[i]->GetGUID()] = candidates[i]->GetPosition();
    }

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
