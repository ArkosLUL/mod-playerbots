/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Rotface.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "PaladinHelper.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float PI_F = static_cast<float>(M_PI);
constexpr float SCAN_RANGE = 60.0f;
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_REFRESH_MS = 200;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;
constexpr float LIVE_RANGE = 80.0f;

constexpr uint8 STAND_STACK = RF_SLOT_COUNT + 1;
constexpr uint8 STAND_TANK = STAND_STACK + 1;
constexpr uint8 STAND_OOZE_SPOT = STAND_TANK + 1;
constexpr uint8 DEFAULT_OOZE_QUADRANT = 2;

constexpr float SLOT_MARGIN = 1.0f + RF_STAND_PARK;

constexpr uint32 MARK_LIFETIME_MS = 60000;
constexpr uint32 MARK_GRACE_MS = 400;
constexpr uint32 MARK_FLIGHT_CAP_MS = 12000;

constexpr float DODGE_TRIGGER = 0.5f;
constexpr float DODGE_MARGIN = 2.5f;
constexpr float DODGE_TIGHT = 0.75f;
constexpr float DODGE_SWEEP_RADIUS = 30.0f;
constexpr uint32 URGENT_MARK_MS = 2500;
constexpr int32 URGENT_WARNING_MS = 1500;
constexpr float SPRAY_MARGIN = 2.0f;
constexpr float MELEE_KEEP = 9.0f;

constexpr float KITE_GO = 15.0f;
constexpr float KITE_MIN_LEAD = 16.0f;
constexpr float KITE_OFF_RING = 3.0f;
constexpr int8 KITE_LEG_STEPS[] = {2, 3, 1, 4};
constexpr int8 WADE_LEG_STEPS[] = {4, 3, 2, 1};
static_assert(std::size(KITE_LEG_STEPS) == std::size(WADE_LEG_STEPS), "both leg orders walk the same steps");
constexpr float KITE_FLOOD_CLEARANCE = RF_FLOOD_RADIUS + 3.0f;
constexpr float KITE_STICKY_CLEARANCE = RF_STICKY_RADIUS + 2.5f;
constexpr float KITE_TANK_REACH = 15.0f;
constexpr float KITE_KEEP_OUT = RF_OUTER_RADIUS + RF_RADIATING_RADIUS + 1.0f;
constexpr float TAUNT_RANGE = 30.0f;
constexpr float OOZE_REACH = 6.0f;
constexpr float SPLASH_GUARD = 15.0f;
constexpr float BURST_CLEAR = 12.5f;
constexpr char const* KIND_BURST = "burst";

constexpr float FREEDOM_OOZE_NEAR = 20.0f;
constexpr float FREEDOM_RANGE = 30.0f;
constexpr uint64 FREEDOM_MECHANICS = (1ULL << MECHANIC_SNARE) | (1ULL << MECHANIC_ROOT);
constexpr uint32 SPELL_RF_HAND_OF_FREEDOM = 1044;
constexpr uint32 SPELL_RF_EVERY_MAN_FOR_HIMSELF = 59752;
constexpr uint32 SPELL_RF_ESCAPE_ARTIST = 20589;
constexpr uint32 SPELL_RF_ANTI_MAGIC_SHELL = 48707;
constexpr float GAS_OOZE_NEAR = 20.0f;

constexpr float CURE_WALK_REACH = 25.0f;
constexpr float CURE_WALK_STEP_DEG = 20.0f;
constexpr int32 CURE_WALK_TRIES = 7;
constexpr float LONE_HEALER_REACH = 30.0f;

constexpr float DPS_PICKUP_STANDOFF = 20.0f;
constexpr float DPS_PICKUP_REACH = 25.0f;
constexpr float DPS_SPOT_LEAD_DEG = 30.0f;
constexpr float DPS_MERGE_GO = 9.0f;

constexpr uint32 SPELL_RF_CLEANSING_TOTEM = 8170;
constexpr char const* WATER_TOTEM_SWAPS[] = {"mana spring totem", "healing stream totem"};

struct Mark
{
    Position at;
    uint32 landsMs = 0;
};

struct Wet
{
    Position at;
    bool flooding = false;
    int32 warningLeftMs = 0;
};

struct Hazard
{
    Position at;
    float radius = 0.0f;
    char const* kind = "";
    bool urgent = false;
};

struct Stand
{
    uint8 id = 0;
    Position spot;
    bool held = false;
    bool parked = false;
};

struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct HazardMemo
{
    uint32 ms = 0;
    bool set = false;
    std::vector<Hazard> hazards;
};

enum class KiteMode : uint8
{
    None,
    Spot,
    Pickup,
    Tank,
    Hold,
    Run
};

struct RfState
{
    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<ObjectGuid> bigOozes;
    std::vector<ObjectGuid> littleOozes;
    std::vector<Position> sticky;
    std::vector<Mark> marks;
    std::vector<Wet> wet;
    std::vector<ObjectGuid> gasCarriers;
    std::unordered_map<ObjectGuid, Position> burstFrom;
    bool spray = false;
    float sprayAxis = 0.0f;
    RaidObs::ObsValue<uint8> flood{"rf.flood"};
    int8 lastWetQuadrant = -1;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    RaidObs::ObsValue<ObjectGuid> kiter{"rf.kiter"};
    ObjectGuid holder;
    RaidObs::ObsGuidMap<uint8> slot{"rf.slot"};
    RaidObs::ObsValue<uint8> oozeSpot{"rf.oozespot"};
    RaidObs::ObsGuidMap<ObjectGuid> dispel{"rf.dispel"};
    std::unordered_map<ObjectGuid, ObjectGuid> cureWalk;
    RaidObs::ObsValue<ObjectGuid> freer{"rf.freedom"};
    std::unordered_set<ObjectGuid> carriers;
    bool dragging = false;
    int8 kiteDir = 1;
    bool kiteWading = false;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
    std::unordered_map<ObjectGuid, HazardMemo> hazards;
};

RaidInstanceState<RfState> rfStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool IsRfUnit(Unit* unit)
{
    uint32 const entry = unit ? unit->GetEntry() : 0;
    return entry == NPC_RF_ROTFACE || entry == NPC_RF_BIG_OOZE || entry == NPC_RF_LITTLE_OOZE;
}

bool IsHeroic(Player* bot)
{
    Difficulty const difficulty = GetRaidMapDifficulty(bot);
    return difficulty == RAID_DIFFICULTY_10MAN_HEROIC || difficulty == RAID_DIFFICULTY_25MAN_HEROIC;
}

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

void NoteDerived(Player* bot, char const* key, char const* value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, key, value);
}

std::string DescribeGuid(Unit* unit) { return unit ? RaidObs::DescribeAssignment(unit->GetGUID()) : "none"; }

std::vector<Player*> MembersOnMap(Player* bot)
{
    std::vector<Player*> members;
    Group* group = bot->GetGroup();
    if (!group)
    {
        members.push_back(bot);
        return members;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && IccSameInstance(bot, member))
            members.push_back(member);
    }

    return members;
}

Position RingPoint(float radius, float bearingDeg)
{
    float const angle = bearingDeg * PI_F / 180.0f;
    return Position(ICC_RF_CENTER.GetPositionX() + radius * std::cos(angle),
                    ICC_RF_CENTER.GetPositionY() + radius * std::sin(angle), ICC_RF_CENTER.GetPositionZ());
}

Position SlotSpot(uint8 id)
{
    if (id >= 1 && id <= RF_INNER_SLOTS)
        return RingPoint(RF_INNER_RADIUS, RF_INNER_OFFSET_DEG + (id - 1) * 360.0f / RF_INNER_SLOTS);

    if (id > RF_INNER_SLOTS && id <= RF_SLOT_COUNT)
        return RingPoint(RF_OUTER_RADIUS, RF_OUTER_OFFSET_DEG + (id - 1 - RF_INNER_SLOTS) * 360.0f / RF_OUTER_SLOTS);

    return RingPoint(RF_STACK_DISTANCE, 180.0f);
}

bool IsOuterSlot(uint8 id) { return id > RF_INNER_SLOTS && id <= RF_SLOT_COUNT; }

Position KitePoint(int32 index)
{
    int32 const wrapped = ((index % RF_KITE_POINTS) + RF_KITE_POINTS) % RF_KITE_POINTS;
    return RingPoint(RF_KITE_RADIUS, wrapped * 360.0f / RF_KITE_POINTS);
}

float BearingDeg(Position const& pos)
{
    float const angle = std::atan2(pos.GetPositionY() - ICC_RF_CENTER.GetPositionY(),
                                   pos.GetPositionX() - ICC_RF_CENTER.GetPositionX());
    float const deg = angle * 180.0f / PI_F;
    return deg < 0.0f ? deg + 360.0f : deg;
}

float SlotGapDeg(uint8 a, uint8 b)
{
    float const gap = std::fabs(BearingDeg(SlotSpot(a)) - BearingDeg(SlotSpot(b)));
    return gap > 180.0f ? 360.0f - gap : gap;
}

int8 QuadrantOf(Position const& pos)
{
    return static_cast<int8>(std::min(3, static_cast<int32>(BearingDeg(pos) / 90.0f)));
}

Position OozeSpotOf(uint8 quadrant) { return RingPoint(RF_KITE_RADIUS, 45.0f + 90.0f * quadrant); }

uint8 WantedOozeQuadrant(RfState const& st)
{
    return st.lastWetQuadrant >= 0 ? static_cast<uint8>((st.lastWetQuadrant + 2) % 4) : DEFAULT_OOZE_QUADRANT;
}

uint8 OozeQuadrant(RfState const& st)
{
    uint8 const held = st.oozeSpot;
    return held ? held - 1 : WantedOozeQuadrant(st);
}

Position OozeSpot(RfState const& st) { return OozeSpotOf(OozeQuadrant(st)); }

Position DpsKiterWait(RfState const& st)
{
    return RingPoint(RF_KITE_RADIUS, 45.0f + 90.0f * OozeQuadrant(st) + DPS_SPOT_LEAD_DEG);
}

Position TankSpot() { return RingPoint(RF_TANK_SPOT_DISTANCE, 0.0f); }

Position DragSpot(Creature* boss)
{
    float const dx = ICC_RF_CENTER.GetPositionX() - boss->GetPositionX();
    float const dy = ICC_RF_CENTER.GetPositionY() - boss->GetPositionY();
    float const len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.1f)
        return TankSpot();

    return Position(ICC_RF_CENTER.GetPositionX() + dx / len * RF_DRAG_DISTANCE,
                    ICC_RF_CENTER.GetPositionY() + dy / len * RF_DRAG_DISTANCE, ICC_RF_CENTER.GetPositionZ());
}

bool WetAt(RfState const& st, Position const& pos, float clearance)
{
    return std::any_of(st.wet.begin(), st.wet.end(),
                       [&](Wet const& wet) { return wet.at.GetExactDist2d(pos) < clearance; });
}

bool StickyAt(RfState const& st, Position const& pos, float clearance)
{
    return std::any_of(st.sticky.begin(), st.sticky.end(),
                       [&](Position const& puddle) { return puddle.GetExactDist2d(pos) < clearance; });
}

bool LiveMarks(RfState const& st, uint32 now)
{
    return std::any_of(st.marks.begin(), st.marks.end(),
                       [now](Mark const& mark) { return static_cast<int32>(mark.landsMs - now) > 0; });
}

std::vector<Creature*> Resolve(Player* bot, std::vector<ObjectGuid> const& guids)
{
    std::vector<Creature*> units;
    for (ObjectGuid const& guid : guids)
    {
        Creature* unit = bot->GetMap()->GetCreature(guid);
        if (unit && unit->IsAlive() && unit->IsVisible())
            units.push_back(unit);
    }

    return units;
}

bool Bursting(Creature* ooze)
{
    return ooze->FindCurrentSpellBySpellId(SPELL_RF_UNSTABLE_OOZE_EXPLOSION) ||
           ooze->HasAura(SPELL_RF_UNSTABLE_OOZE_SUICIDE);
}

uint8 Stacks(Creature* ooze)
{
    Aura const* aura = ooze->GetAura(SPELL_RF_UNSTABLE_OOZE);
    return aura ? aura->GetStackAmount() : 0;
}

std::vector<Creature*> MergeableOozes(Player* bot, RfState const& st)
{
    std::vector<Creature*> oozes = Resolve(bot, st.bigOozes);
    oozes.erase(std::remove_if(oozes.begin(), oozes.end(), &Bursting), oozes.end());
    return oozes;
}

Creature* Nearest(Position const& from, std::vector<Creature*> const& units)
{
    Creature* best = nullptr;
    for (Creature* unit : units)
        if (!best || unit->GetExactDist2d(from) < best->GetExactDist2d(from))
            best = unit;

    return best;
}

Player* LittleOozeVictim(Creature* ooze)
{
    Unit* victim = ooze->GetVictim();
    return victim ? victim->ToPlayer() : nullptr;
}

bool Infected(Player* member, Creature* boss)
{
    return member->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_RF_MUTATED_INFECTION, boss));
}

Creature* RotfaceUnit(Player* bot)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(ICC_BOSS_ROTFACE)) : nullptr;
}

RfState* LiveState(PlayerbotAI* botAI, Creature** boss = nullptr)
{
    Creature* rotface = GetRfBoss(botAI);
    if (boss)
        *boss = rotface;

    return rotface ? rfStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void Scan(RfState& st, Player* bot, Creature* boss, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    st.bigOozes.clear();
    st.littleOozes.clear();
    st.sticky.clear();
    st.marks.clear();
    st.wet.clear();
    st.gasCarriers.clear();

    for (Creature* ooze : IccGetCreaturesByEntries(boss, {NPC_RF_BIG_OOZE}, SCAN_RANGE))
    {
        if (!ooze->IsVisible())
            continue;

        if (ooze->FindCurrentSpellBySpellId(SPELL_RF_UNSTABLE_OOZE_EXPLOSION))
            st.burstFrom[ooze->GetGUID()] = ooze->GetPosition();

        st.bigOozes.push_back(ooze->GetGUID());
    }

    for (Creature* ooze : IccGetCreaturesByEntries(boss, {NPC_RF_LITTLE_OOZE}, SCAN_RANGE))
        st.littleOozes.push_back(ooze->GetGUID());

    for (Creature* puddle : IccGetCreaturesByEntries(boss, {NPC_RF_STICKY_OOZE}, SCAN_RANGE))
        st.sticky.push_back(puddle->GetPosition());

    for (Creature* stalker : IccGetCreaturesByEntries(boss, {NPC_RF_EXPLOSION_STALKER}, SCAN_RANGE))
    {
        TempSummon* summon = stalker->ToTempSummon();
        if (!summon)
            continue;

        uint32 const timer = summon->GetTimer();
        uint32 const age = timer < MARK_LIFETIME_MS ? MARK_LIFETIME_MS - timer : 0;
        uint32 flightMs = MARK_FLIGHT_CAP_MS;
        auto const origin = st.burstFrom.find(summon->GetSummonerGUID());
        if (origin != st.burstFrom.end())
            flightMs = static_cast<uint32>(origin->second.GetExactDist(stalker) / RF_MARK_SPEED * 1000.0f);
        else if (Creature* source = bot->GetMap()->GetCreature(summon->GetSummonerGUID()))
            flightMs = static_cast<uint32>(source->GetExactDist(stalker) / RF_MARK_SPEED * 1000.0f);

        flightMs = std::min(flightMs, MARK_FLIGHT_CAP_MS);
        if (age >= flightMs + MARK_GRACE_MS)
            continue;

        st.marks.push_back({stalker->GetPosition(), now + flightMs + MARK_GRACE_MS - age});
    }

    int8 warnedQuadrant = -1;
    int8 floodingQuadrant = -1;
    for (Creature* stalker : IccGetCreaturesByEntries(boss, {NPC_RF_PUDDLE_STALKER}, SCAN_RANGE))
    {
        Wet wet;
        wet.at = stalker->GetPosition();
        wet.flooding = stalker->HasAura(SPELL_RF_OOZE_FLOOD_ACTIVE);
        bool warned = false;
        for (uint32 spellId : SPELL_RF_OOZE_FLOOD_WARNINGS)
            if (Aura const* aura = stalker->GetAura(spellId))
            {
                warned = true;
                wet.warningLeftMs = aura->GetDuration();
            }

        if (!wet.flooding && !warned)
            continue;

        st.wet.push_back(wet);
        (warned ? warnedQuadrant : floodingQuadrant) = QuadrantOf(wet.at);
    }

    int8 const quadrant = warnedQuadrant >= 0 ? warnedQuadrant : floodingQuadrant;
    st.flood = static_cast<uint8>(quadrant + 1);
    if (quadrant >= 0)
        st.lastWetQuadrant = quadrant;

    st.spray = boss->FindCurrentSpellBySpellId(SPELL_RF_SLIME_SPRAY) != nullptr;
    if (st.spray)
    {
        Creature* aim = nullptr;
        for (Creature* stalker : IccGetCreaturesByEntries(boss, {NPC_RF_SPRAY_STALKER}, SCAN_RANGE))
        {
            TempSummon* summon = stalker->ToTempSummon();
            if (summon && (!aim || summon->GetTimer() > aim->ToTempSummon()->GetTimer()))
                aim = stalker;
        }

        st.sprayAxis = aim ? boss->GetAngle(aim) : boss->GetOrientation();
    }

    if (IsHeroic(bot))
    {
        uint32 const gas = sSpellMgr->GetSpellIdForDifficulty(SPELL_RF_VILE_GAS, boss);
        for (Player* member : MembersOnMap(bot))
            if (member->HasAura(gas))
                st.gasCarriers.push_back(member->GetGUID());
    }
}

bool CanKiteAsDps(Player* member)
{
    uint8 const cls = member->getClass();
    return IsRaidDpsBot(member) && (cls == CLASS_DEATH_KNIGHT || cls == CLASS_PALADIN);
}

uint8 KiterRank(Player* member)
{
    if (!IsTankPlayer(member))
        return member->getClass() == CLASS_DEATH_KNIGHT ? 3 : 4;

    if (PlayerbotAI::IsAssistTank(member))
        return 0;

    return PlayerbotAI::IsMainTank(member) ? 2 : 1;
}

Player* ElectKiter(RfState const& st, std::vector<Player*> const& members)
{
    ObjectGuid const current = st.kiter;
    Player* kept = nullptr;
    Player* best = nullptr;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || member->GetGUID() == st.holder || (!IsTankPlayer(member) && !CanKiteAsDps(member)))
            continue;

        if (member->GetGUID() == current)
            kept = member;

        uint8 const rank = KiterRank(member);
        if (!best || rank < KiterRank(best) || (rank == KiterRank(best) && member->GetGUID() < best->GetGUID()))
            best = member;
    }

    bool const tankFree = best && IsTankPlayer(best) && !PlayerbotAI::IsMainTank(best);
    return kept && (IsTankPlayer(kept) || !tankFree) ? kept : best;
}

bool IsKiterBot(Player* bot, RfState const& st)
{
    ObjectGuid const kiter = st.kiter;
    return !kiter.IsEmpty() && kiter == bot->GetGUID();
}

bool IsCarrierBot(Player* bot, RfState const& st) { return st.carriers.count(bot->GetGUID()) != 0; }

Player* KiterOf(Player* bot, RfState const& st)
{
    ObjectGuid const guid = st.kiter;
    Player* kiter = guid.IsEmpty() ? nullptr : ObjectAccessor::FindPlayer(guid);
    return kiter && IccSameInstance(bot, kiter) ? kiter : nullptr;
}

bool SlotUsable(RfState const& st, Player* bot, uint8 id)
{
    Position const spot = SlotSpot(id);
    if (WetAt(st, spot, RF_FLOOD_RADIUS + SLOT_MARGIN) || StickyAt(st, spot, RF_STICKY_RADIUS + SLOT_MARGIN))
        return false;

    for (Creature* ooze : Resolve(bot, st.bigOozes))
        if (ooze->GetExactDist2d(spot) < RF_RADIATING_RADIUS + SLOT_MARGIN)
            return false;

    return true;
}

void AssignSlots(RfState& st, Player* bot, std::vector<Player*> const& members)
{
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || IsTankPlayer(member) || IsKiterBot(member, st))
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
    }

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    std::sort(ranged.begin(), ranged.end(), IccGuidLess());

    Player* kiter = KiterOf(bot, st);
    Player* lone = healers.size() == 1 && kiter ? healers.front() : nullptr;

    std::unordered_set<ObjectGuid> roster;
    for (Player* member : healers)
        roster.insert(member->GetGUID());
    for (Player* member : ranged)
        roster.insert(member->GetGUID());

    for (auto itr = st.slot.begin(); itr != st.slot.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.slot.erase(itr);

    bool used[RF_SLOT_COUNT + 2] = {};
    std::vector<Player*> unplaced;
    std::vector<uint8> healerSlots;
    auto const keep = [&](Player* member)
    {
        auto const held = st.slot.find(member->GetGUID());
        uint8 const id = held != st.slot.end() ? held->second : 0;
        bool const hunterInner = member->getClass() == CLASS_HUNTER && id >= 1 && id <= RF_INNER_SLOTS;
        bool const strayed = member == lone && SlotSpot(id).GetExactDist2d(kiter) > LONE_HEALER_REACH;
        if (id >= 1 && id <= RF_SLOT_COUNT && !used[id] && !hunterInner && !strayed && SlotUsable(st, bot, id))
        {
            used[id] = true;
            if (PlayerbotAI::IsHeal(member) && !IsOuterSlot(id))
                healerSlots.push_back(id);
        }
        else
            unplaced.push_back(member);
    };
    for (Player* member : healers)
        keep(member);
    for (Player* member : ranged)
        keep(member);

    for (Player* member : unplaced)
    {
        bool const healer = PlayerbotAI::IsHeal(member);
        bool const hunter = member->getClass() == CLASS_HUNTER;
        bool const spread = healer && st.slot.find(member->GetGUID()) == st.slot.end();
        uint8 best = 0;
        float bestScore = 0.0f;
        for (uint8 id = 1; id <= RF_SLOT_COUNT; ++id)
        {
            if (used[id] || (hunter && !IsOuterSlot(id)) || !SlotUsable(st, bot, id))
                continue;

            bool const wanted = healer != IsOuterSlot(id);
            Player const* from = member == lone ? kiter : member;
            float score = from->GetExactDist2d(SlotSpot(id)) + (wanted ? 0.0f : 100000.0f);
            if (spread && wanted)
            {
                float gap = 180.0f;
                for (uint8 other : healerSlots)
                    gap = std::min(gap, SlotGapDeg(id, other));

                score -= 100.0f * std::round(gap);
            }

            if (!best || score < bestScore)
            {
                best = id;
                bestScore = score;
            }
        }

        if (best)
        {
            used[best] = true;
            st.slot[member->GetGUID()] = best;
            if (healer && !IsOuterSlot(best))
                healerSlots.push_back(best);
        }
        else if (hunter)
            st.slot.erase(member->GetGUID());
        else
            st.slot[member->GetGUID()] = STAND_STACK;
    }
}

bool MergeReady(RfState const& st, Player* carrier, Creature* boss)
{
    std::vector<Creature*> const bigs = MergeableOozes(carrier, st);
    if (!bigs.empty())
        return Nearest(carrier->GetPosition(), bigs)->GetExactDist2d(carrier) <= RF_MERGE_TOUCH;

    for (Creature* ooze : Resolve(carrier, st.littleOozes))
        if (LittleOozeVictim(ooze) != carrier && ooze->GetExactDist2d(carrier) <= RF_MERGE_TOUCH)
            return true;

    for (Player* member : MembersOnMap(carrier))
        if (member != carrier && Infected(member, boss) && member->GetExactDist2d(carrier) <= RF_MERGE_TOUCH)
            return true;

    return false;
}

bool CanPayFor(Player* bot, SpellInfo const* info)
{
    if (info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return true;

    return static_cast<int32>(bot->GetPower(Powers(info->PowerType))) >=
           info->CalcPowerCost(bot, info->GetSchoolMask());
}

char const* ReadyCure(Player* member, Player* carrier, bool inReach = true)
{
    static char const* const priest[] = {"abolish disease", "cure disease"};
    static char const* const paladin[] = {"cleanse", "purify"};
    static char const* const shaman[] = {"cleanse spirit", "cure toxins"};

    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI || !IsDutyBot(member) || !member->IsAlive() || member->HasUnitState(UNIT_STATE_LOST_CONTROL) ||
        member->HasUnitFlag(UNIT_FLAG_SILENCED) ||
        (inReach && (member->GetExactDist(carrier) > RF_DISPEL_RANGE || !member->IsWithinLOSInMap(carrier))))
        return nullptr;

    char const* const* names = nullptr;
    switch (member->getClass())
    {
        case CLASS_PRIEST:
            names = priest;
            break;
        case CLASS_PALADIN:
            names = paladin;
            break;
        case CLASS_SHAMAN:
            names = shaman;
            break;
        default:
            return nullptr;
    }

    for (size_t i = 0; i < 2; ++i)
    {
        uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", names[i])->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (info && member->HasSpell(spellId) && !member->HasSpellCooldown(spellId) && CanPayFor(member, info) &&
            info->CheckShapeshift(member->GetShapeshiftForm()) == SPELL_CAST_OK)
            return names[i];
    }

    return nullptr;
}

bool CanWalkToCure(RfState const& st, Player* member, Player* carrier)
{
    if (member->GetGUID() == st.holder || IsKiterBot(member, st) || IsCarrierBot(member, st))
        return false;

    return std::none_of(st.dispel.Raw().begin(), st.dispel.Raw().end(), [&](auto const& duty)
                        { return duty.second == member->GetGUID() && duty.first != carrier->GetGUID(); });
}

KiteMode ModeOf(Player* kiter, RfState const& st, Creature*& ooze);

bool DpsKiterTowing(Player* member, RfState const& st)
{
    Creature* ooze = nullptr;
    return IsKiterBot(member, st) && !IsTankPlayer(member) && ModeOf(member, st, ooze) == KiteMode::Run;
}

void UpdateDispel(RfState& st, std::vector<Player*> const& members, Creature* boss)
{
    std::unordered_set<ObjectGuid> ready;
    for (Player* member : members)
        if (Infected(member, boss) && (MergeReady(st, member, boss) || DpsKiterTowing(member, st) ||
                                       member->GetHealthPct() < RF_DISPEL_EMERGENCY_PCT))
            ready.insert(member->GetGUID());

    for (auto itr = st.dispel.begin(); itr != st.dispel.end();)
        itr = ready.count(itr->first) ? std::next(itr) : st.dispel.erase(itr);

    auto const nearest = [&](Player* carrier, bool inReach)
    {
        Player* best = nullptr;
        for (Player* member : members)
            if (ReadyCure(member, carrier, inReach) && (inReach || CanWalkToCure(st, member, carrier)) &&
                (!best || member->GetExactDist(carrier) < best->GetExactDist(carrier)))
                best = member;

        return best;
    };

    for (Player* carrier : members)
    {
        if (!ready.count(carrier->GetGUID()))
            continue;

        auto const held = st.dispel.find(carrier->GetGUID());
        Player* current = held != st.dispel.end() ? ObjectAccessor::FindPlayer(held->second) : nullptr;
        if (current && (!IccSameInstance(carrier, current) || !ReadyCure(current, carrier, false)))
            current = nullptr;

        if (current && ReadyCure(current, carrier))
            continue;

        Player* best = nearest(carrier, true);
        if (!best && current && CanWalkToCure(st, current, carrier))
            continue;

        if (!best)
            best = nearest(carrier, false);

        if (best)
            st.dispel[carrier->GetGUID()] = best->GetGUID();
        else
            st.dispel.erase(carrier->GetGUID());
    }

    st.cureWalk.clear();
    for (auto const& [carrierGuid, curerGuid] : st.dispel.Raw())
    {
        Player* carrier = ObjectAccessor::FindPlayer(carrierGuid);
        Player* curer = ObjectAccessor::FindPlayer(curerGuid);
        if (carrier && curer && IccSameInstance(carrier, curer) && !ReadyCure(curer, carrier))
            st.cureWalk[curerGuid] = carrierGuid;
    }
}

void UpdateOozeSpot(RfState& st)
{
    bool const needed = !st.bigOozes.empty() || !st.littleOozes.empty() || !st.carriers.empty();
    st.oozeSpot = needed ? static_cast<uint8>(WantedOozeQuadrant(st) + 1) : 0;
}

Creature* DutyOoze(Player* kiter, RfState const& st)
{
    std::vector<Creature*> oozes = Resolve(kiter, st.bigOozes);
    oozes.erase(std::remove_if(oozes.begin(), oozes.end(), &Bursting), oozes.end());
    std::vector<Creature*> loose;
    for (Creature* ooze : oozes)
        if (ooze->GetVictim() != kiter)
            loose.push_back(ooze);

    return Nearest(kiter->GetPosition(), loose.empty() ? oozes : loose);
}

KiteMode ModeOf(Player* kiter, RfState const& st, Creature*& ooze)
{
    ooze = DutyOoze(kiter, st);
    if (!ooze)
        return st.carriers.empty() && st.littleOozes.empty() ? KiteMode::None : KiteMode::Spot;

    if (ooze->GetVictim() != kiter)
        return KiteMode::Pickup;

    if (!IsTankPlayer(kiter))
        return KiteMode::Run;

    if (st.carriers.count(kiter->GetGUID()))
        return KiteMode::Hold;

    uint8 const spot = st.oozeSpot;
    bool const spotHeld = spot && !WetAt(st, OozeSpotOf(spot - 1), KITE_FLOOD_CLEARANCE) &&
                          kiter->GetExactDist2d(OozeSpotOf(spot - 1)) <= KITE_TANK_REACH;
    return Stacks(ooze) < RF_KITE_FROM_STACKS && spotHeld ? KiteMode::Tank : KiteMode::Run;
}

char const* ModeName(KiteMode mode, bool wading)
{
    switch (mode)
    {
        case KiteMode::Spot:
            return "spot";
        case KiteMode::Pickup:
            return "pickup";
        case KiteMode::Tank:
            return "tank";
        case KiteMode::Hold:
            return "hold";
        case KiteMode::Run:
            return wading ? "wade" : "kite";
        default:
            return "none";
    }
}

float KiteGo(Player* kiter, RfState const& st)
{
    if (IsTankPlayer(kiter))
        return KITE_GO;

    for (Creature* little : Resolve(kiter, st.littleOozes))
        if (LittleOozeVictim(little) == kiter)
            return DPS_MERGE_GO;

    return KITE_GO;
}

bool DpsPickupWalk(Player* kiter, Creature* ooze, Position& spot)
{
    float const distance = ooze->GetExactDist2d(kiter);
    if (distance <= DPS_PICKUP_REACH)
        return false;

    float const scale = DPS_PICKUP_STANDOFF / distance;
    spot = Position(ooze->GetPositionX() + (kiter->GetPositionX() - ooze->GetPositionX()) * scale,
                    ooze->GetPositionY() + (kiter->GetPositionY() - ooze->GetPositionY()) * scale,
                    ICC_RF_CENTER.GetPositionZ());
    return spot.GetExactDist2d(ICC_RF_CENTER) <= RF_ROOM_RADIUS;
}

bool KiterWet(RfState const& st, Player* kiter)
{
    return WetAt(st, kiter->GetPosition(), RF_FLOOD_RADIUS + DODGE_TRIGGER);
}

bool KiterFlooded(RfState const& st, Player* kiter)
{
    return std::any_of(st.wet.begin(), st.wet.end(), [kiter](Wet const& wet)
                       { return wet.flooding && wet.at.GetExactDist2d(kiter) < RF_FLOOD_RADIUS + DODGE_TRIGGER; });
}

void KiterBind(RfState const& st, Player* kiter, Creature* ooze, bool& impaired, bool& wading)
{
    impaired = ooze->GetExactDist2d(kiter) <= FREEDOM_OOZE_NEAR && !kiter->HasAura(SPELL_RF_HAND_OF_FREEDOM) &&
               kiter->HasAuraWithMechanic(FREEDOM_MECHANICS);
    wading = kiter->getClass() == CLASS_DEATH_KNIGHT && KiterFlooded(st, kiter);
}

uint32 SelfFreedom(PlayerbotAI* kiterAI, Player* kiter, bool impaired, bool wading)
{
    if (impaired)
        for (uint32 spellId : {SPELL_RF_HAND_OF_FREEDOM, SPELL_RF_EVERY_MAN_FOR_HIMSELF, SPELL_RF_ESCAPE_ARTIST})
            if (kiterAI->CanCastSpell(spellId, kiter))
                return spellId;

    return wading && kiterAI->CanCastSpell(SPELL_RF_ANTI_MAGIC_SHELL, kiter) ? SPELL_RF_ANTI_MAGIC_SHELL : 0;
}

Player* FreedomPaladin(Player* kiter, std::vector<Player*> const& members)
{
    Player* best = nullptr;
    for (Player* member : members)
    {
        bool const paladinBot = member != kiter && member->getClass() == CLASS_PALADIN && IsDutyBot(member);
        PlayerbotAI* memberAI = paladinBot ? GET_PLAYERBOT_AI(member) : nullptr;
        if (!memberAI || member->HasUnitState(UNIT_STATE_LOST_CONTROL) || member->GetExactDist(kiter) > FREEDOM_RANGE ||
            !member->IsWithinLOSInMap(kiter) || ai::paladin::HasAnyPaladinHandFromCaster(kiter, member) ||
            !memberAI->CanCastSpell(SPELL_RF_HAND_OF_FREEDOM, kiter))
            continue;

        if (!best || member->GetExactDist(kiter) < best->GetExactDist(kiter))
            best = member;
    }

    return best;
}

void UpdateFreedom(RfState& st, Player* bot, std::vector<Player*> const& members)
{
    ObjectGuid freer;
    Player* kiter = KiterOf(bot, st);
    PlayerbotAI* kiterAI = kiter ? GET_PLAYERBOT_AI(kiter) : nullptr;
    Creature* ooze = nullptr;
    if (kiterAI && ModeOf(kiter, st, ooze) == KiteMode::Run)
    {
        bool impaired = false;
        bool wading = false;
        KiterBind(st, kiter, ooze, impaired, wading);
        if (SelfFreedom(kiterAI, kiter, impaired, wading))
            freer = kiter->GetGUID();
        else if (Player* paladin = impaired ? FreedomPaladin(kiter, members) : nullptr)
            freer = paladin->GetGUID();
    }

    st.freer = freer;
}

bool GassedWithOozeNear(RfState const& st, Player* member, std::vector<Creature*> const& oozes)
{
    if (std::find(st.gasCarriers.begin(), st.gasCarriers.end(), member->GetGUID()) == st.gasCarriers.end())
        return false;

    return std::any_of(oozes.begin(), oozes.end(), [member](Creature* ooze)
                       { return ooze->GetVictim() == member && ooze->GetExactDist2d(member) <= GAS_OOZE_NEAR; });
}

void RefreshDuties(RfState& st, Player* bot, Creature* boss, uint32 now)
{
    if (st.dutiesRan && getMSTimeDiff(st.dutyMs, now) < DUTY_REFRESH_MS)
        return;

    st.dutiesRan = true;
    st.dutyMs = now;

    std::vector<Player*> const members = MembersOnMap(bot);

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    st.holder = holder && IsDutyBot(holder) && IsTankPlayer(holder) ? holder->GetGUID() : ObjectGuid::Empty;

    ObjectGuid const lastKiter = st.kiter;
    Player* kiter = ElectKiter(st, members);
    st.kiter = kiter ? kiter->GetGUID() : ObjectGuid::Empty;
    if (st.kiter.Get() != lastKiter)
        st.kiteWading = false;

    st.carriers.clear();
    for (Player* member : members)
        if (IsDutyBot(member) && Infected(member, boss))
            st.carriers.insert(member->GetGUID());
    for (Creature* ooze : Resolve(bot, st.littleOozes))
        if (Player* carrier = LittleOozeVictim(ooze))
            if (IsDutyBot(carrier))
                st.carriers.insert(carrier->GetGUID());

    float const drift = boss->GetExactDist2d(ICC_RF_CENTER);
    if (!st.dragging && drift > RF_BOSS_DRIFT)
        st.dragging = true;
    else if (st.dragging && drift <= RF_BOSS_SETTLED)
        st.dragging = false;

    AssignSlots(st, bot, members);
    UpdateOozeSpot(st);
    UpdateDispel(st, members, boss);
    UpdateFreedom(st, bot, members);
}

bool CarrierWalks(Player* bot, RfState const& st)
{
    return IsCarrierBot(bot, st) && (!IsKiterBot(bot, st) || !DutyOoze(bot, st));
}

void UpdateStand(RfState& st, Player* bot, Creature* boss)
{
    Stand& stand = st.stands[bot->GetGUID()];
    uint8 id = 0;
    Position spot;
    if (bot->IsAlive() && !IsCarrierBot(bot, st) && !st.cureWalk.count(bot->GetGUID()))
    {
        if (bot->GetGUID() == st.holder)
        {
            id = STAND_TANK;
            spot = st.dragging ? DragSpot(boss) : TankSpot();
        }
        else if (IsKiterBot(bot, st))
        {
            Creature* ooze = nullptr;
            KiteMode const mode = ModeOf(bot, st, ooze);
            if (mode == KiteMode::Spot || mode == KiteMode::Tank)
            {
                id = STAND_OOZE_SPOT;
                spot = IsTankPlayer(bot) ? OozeSpot(st) : DpsKiterWait(st);
            }
        }
        else
        {
            auto const held = st.slot.find(bot->GetGUID());
            if (held != st.slot.end())
            {
                id = held->second;
                spot = SlotSpot(id);
            }
        }
    }

    if (!id)
    {
        stand.held = false;
        return;
    }

    if (!stand.held || stand.id != id || stand.spot.GetExactDist2d(spot) > 0.5f)
    {
        stand.id = id;
        stand.spot = spot;
        stand.held = true;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= RF_STAND_UNPARK : distance <= RF_STAND_PARK;
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    auto const itr = st->stands.find(botAI->GetBot()->GetGUID());
    return itr != st->stands.end() && itr->second.held ? &itr->second : nullptr;
}

bool ClearOfSpray(RfState const& st, Creature* boss, float x, float y, float margin)
{
    if (!st.spray)
        return true;

    float const dx = x - boss->GetPositionX();
    float const dy = y - boss->GetPositionY();
    float const distance = std::sqrt(dx * dx + dy * dy);
    if (distance < RF_SPRAY_BOUNDARY + margin)
        return false;

    float off = std::atan2(dy, dx) - st.sprayAxis;
    off = std::fabs(Position::NormalizeOrientation(off));
    if (off > PI_F)
        off = 2.0f * PI_F - off;

    float const half = RF_SPRAY_ARC_DEG * PI_F / 360.0f;
    return off >= half + std::asin(std::min(1.0f, margin / distance));
}

std::vector<Hazard> const& HazardsFor(PlayerbotAI* botAI, RfState& st)
{
    Player* bot = botAI->GetBot();
    HazardMemo& memo = st.hazards[bot->GetGUID()];
    uint32 const now = getMSTime();
    if (memo.set && memo.ms == now)
        return memo.hazards;

    memo.set = true;
    memo.ms = now;
    memo.hazards.clear();

    for (Mark const& mark : st.marks)
    {
        int32 const left = static_cast<int32>(mark.landsMs - now);
        if (left > 0)
            memo.hazards.push_back({mark.at, RF_MARK_RADIUS, "mark", left < static_cast<int32>(URGENT_MARK_MS)});
    }

    for (Position const& puddle : st.sticky)
        memo.hazards.push_back({puddle, RF_STICKY_RADIUS, "sticky", true});

    bool const kiter = IsKiterBot(bot, st);
    Creature* towed = nullptr;
    if (!kiter || ModeOf(bot, st, towed) != KiteMode::Run)
        for (Wet const& wet : st.wet)
        {
            bool const urgent = wet.flooding || wet.warningLeftMs < URGENT_WARNING_MS;
            memo.hazards.push_back({wet.at, RF_FLOOD_RADIUS, "flood", urgent});
        }

    bool const gassed = std::find(st.gasCarriers.begin(), st.gasCarriers.end(), bot->GetGUID()) != st.gasCarriers.end();
    if (!gassed)
        for (ObjectGuid const& guid : st.gasCarriers)
            if (Player* carrier = ObjectAccessor::FindPlayer(guid))
                if (IccSameInstance(bot, carrier))
                    memo.hazards.push_back({carrier->GetPosition(), RF_VILE_GAS_RADIUS, "gas", true});

    bool const wantsOozes = kiter || IsCarrierBot(bot, st);
    for (Creature* ooze : Resolve(bot, st.bigOozes))
    {
        if (ooze->FindCurrentSpellBySpellId(SPELL_RF_UNSTABLE_OOZE_EXPLOSION))
            memo.hazards.push_back({ooze->GetPosition(), BURST_CLEAR, KIND_BURST, true});
        else if (!wantsOozes)
            memo.hazards.push_back({ooze->GetPosition(), RF_RADIATING_RADIUS, "ooze", false});
    }

    if (!wantsOozes)
        for (Creature* ooze : Resolve(bot, st.littleOozes))
            memo.hazards.push_back({ooze->GetPosition(), RF_RADIATING_RADIUS, "little", false});

    return memo.hazards;
}

bool IsMeleeDuty(Player* bot, RfState const& st)
{
    return !IsKiterBot(bot, st) &&
           (bot->GetGUID() == st.holder || (IsRaidDpsBot(bot) && !PlayerbotAI::IsRanged(bot)));
}

Unit* ResolveTarget(PlayerbotAI* botAI, RfState& st, Creature* boss)
{
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!tank && !IsRaidDpsBot(bot))
        return nullptr;

    Creature* ooze = IsKiterBot(bot, st) ? DutyOoze(bot, st) : nullptr;
    if (ooze)
    {
        NoteDerived(bot, "rf.target", "ooze");
        return ooze;
    }

    if (bot->GetGUID() != st.holder)
        if (Unit* skull = GetRaidHumanSkull(botAI, &IsRfUnit))
        {
            NoteDerived(bot, "rf.target", "skull");
            return skull;
        }

    NoteDerived(bot, "rf.target", "boss");
    return boss;
}

bool PlanKiteLeg(Player* kiter, RfState const& st, Creature* ooze, bool wade, Position& spot, int8& dir)
{
    float const step = 360.0f / RF_KITE_POINTS;
    int32 const from = static_cast<int32>(std::lround(BearingDeg(kiter->GetPosition()) / step));
    Position const here = kiter->GetPosition();
    bool const inReach = ooze->GetExactDist2d(here) < RF_RADIATING_RADIUS + 1.0f;
    int8 const* legs = wade ? WADE_LEG_STEPS : KITE_LEG_STEPS;

    for (int8 const way : {st.kiteDir, static_cast<int8>(-st.kiteDir)})
        for (size_t leg = 0; leg < std::size(KITE_LEG_STEPS); ++leg)
        {
            Position const point = KitePoint(from + way * legs[leg]);
            float const lead = ooze->GetExactDist2d(point);
            if (lead < KITE_MIN_LEAD || StickyAt(st, point, KITE_STICKY_CLEARANCE) ||
                (!wade && WetAt(st, point, KITE_FLOOD_CLEARANCE)))
                continue;

            if (inReach ? lead <= ooze->GetExactDist2d(here)
                        : SegmentCrossesCircle(here, point, {ooze->GetPosition(), RF_RADIATING_RADIUS + 1.0f}))
                continue;

            bool const floods = !wade && std::any_of(st.wet.begin(), st.wet.end(),
                                                     [&](Wet const& wet)
                                                     {
                                                         return wet.at.GetExactDist2d(here) >= RF_FLOOD_RADIUS &&
                                                                SegmentCrossesCircle(here, point,
                                                                                     {wet.at, RF_FLOOD_RADIUS});
                                                     });
            if (floods)
                continue;

            spot = point;
            dir = way;
            return true;
        }

    return false;
}
}

bool IsRfLive(PlayerbotAI* botAI) { return GetRfBoss(botAI) != nullptr; }

Creature* GetRfBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetExactDist2d(ICC_RF_CENTER) > LIVE_RANGE)
        return nullptr;

    Creature* boss = RotfaceUnit(bot);
    return boss && boss->IsAlive() && boss->IsInCombat() ? boss : nullptr;
}

Unit* GetRfTarget(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    RfState* st = LiveState(botAI, &boss);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st, boss);
    }

    return memo.pick;
}

Unit* GetRfPetTarget(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    RfState* st = LiveState(botAI, &boss);
    Unit* pick = st ? GetRfTarget(botAI) : nullptr;
    bool const kited = pick && pick->GetEntry() == NPC_RF_BIG_OOZE && IsKiterBot(botAI->GetBot(), *st);
    return kited ? boss : pick;
}

Unit* GetRfOozeToTaunt(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsKiterBot(bot, *st))
        return nullptr;

    Creature* ooze = DutyOoze(bot, *st);
    if (!ooze || ooze->GetVictim() == bot || bot->GetExactDist(ooze) > TAUNT_RANGE || !bot->IsWithinLOSInMap(ooze) ||
        !ClassTauntReady(botAI))
        return nullptr;

    return ooze->GetDiminishing(DIMINISHING_TAUNT) < DIMINISHING_LEVEL_TAUNT_IMMUNE ? ooze : nullptr;
}

bool GetRfKiteWalk(PlayerbotAI* botAI, Position& spot, int8& dir, bool& wade)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    wade = false;
    if (!st || !IsKiterBot(bot, *st))
        return false;

    Creature* ooze = nullptr;
    KiteMode const mode = ModeOf(bot, *st, ooze);
    if (mode == KiteMode::Pickup && !IsTankPlayer(bot))
    {
        dir = st->kiteDir;
        return DpsPickupWalk(bot, ooze, spot);
    }

    if (mode != KiteMode::Run)
        return false;

    float const offRing = std::fabs(bot->GetExactDist2d(ICC_RF_CENTER) - RF_KITE_RADIUS);
    if (!KiterWet(*st, bot) && ooze->GetExactDist2d(bot) >= KiteGo(bot, *st) && offRing <= KITE_OFF_RING)
        return false;

    if (PlanKiteLeg(bot, *st, ooze, false, spot, dir))
        return true;

    wade = PlanKiteLeg(bot, *st, ooze, true, spot, dir);
    return wade;
}

void CommitRfKiteLeg(PlayerbotAI* botAI, int8 dir, bool wade)
{
    if (RfState* st = LiveState(botAI))
    {
        st->kiteDir = dir < 0 ? -1 : 1;
        st->kiteWading = wade;
    }
}

bool IsRfKiteSpotSafe(PlayerbotAI* botAI, Position const& spot)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsKiterBot(bot, *st))
        return false;

    Creature* ooze = nullptr;
    KiteMode const mode = ModeOf(bot, *st, ooze);
    if (mode == KiteMode::Pickup && !IsTankPlayer(bot))
        return ooze->GetExactDist2d(bot) > DPS_PICKUP_REACH;

    if (mode != KiteMode::Run)
        return false;

    return (st->kiteWading || !WetAt(*st, spot, KITE_FLOOD_CLEARANCE)) && !StickyAt(*st, spot, KITE_STICKY_CLEARANCE) &&
           ooze->GetExactDist2d(spot) >= RF_RADIATING_RADIUS + 1.0f;
}

bool IsRfKiteUrgent(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsKiterBot(bot, *st))
        return false;

    Creature* ooze = nullptr;
    ModeOf(bot, *st, ooze);
    return ooze && (ooze->GetExactDist2d(bot) < RF_RADIATING_RADIUS || KiterWet(*st, bot));
}

bool IsRfKiter(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    return st && IsKiterBot(botAI->GetBot(), *st);
}

bool IsRfKiteHeld(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsKiterBot(bot, *st))
        return false;

    Creature* ooze = nullptr;
    KiteMode const mode = ModeOf(bot, *st, ooze);
    return mode == KiteMode::Run || mode == KiteMode::Hold || (mode == KiteMode::Pickup && !IsTankPlayer(bot));
}

Unit* GetRfFreedom(PlayerbotAI* botAI, uint32& spellId)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    ObjectGuid const freer = st ? st->freer.Get() : ObjectGuid::Empty;
    Player* kiter = !freer.IsEmpty() && freer == bot->GetGUID() ? KiterOf(bot, *st) : nullptr;
    if (!kiter)
        return nullptr;

    if (kiter != bot)
    {
        spellId = SPELL_RF_HAND_OF_FREEDOM;
        return !kiter->HasAura(SPELL_RF_HAND_OF_FREEDOM) && botAI->CanCastSpell(spellId, kiter) ? kiter : nullptr;
    }

    Creature* ooze = nullptr;
    if (ModeOf(bot, *st, ooze) != KiteMode::Run)
        return nullptr;

    bool impaired = false;
    bool wading = false;
    KiterBind(*st, bot, ooze, impaired, wading);
    spellId = SelfFreedom(botAI, bot, impaired, wading);
    return spellId ? bot : nullptr;
}

bool GetRfCarrierWalk(PlayerbotAI* botAI, Position& spot)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive() || !CarrierWalks(bot, *st))
        return false;

    std::vector<Creature*> const bigs = MergeableOozes(bot, *st);
    ObjectGuid const kiter = st->kiter;
    Creature* target = nullptr;
    for (Creature* ooze : bigs)
        if (!kiter.IsEmpty() && ooze->GetVictim() && ooze->GetVictim()->GetGUID() == kiter)
            target = ooze;

    if (!target)
        target = Nearest(bot->GetPosition(), bigs);

    if (!target)
    {
        std::vector<Creature*> others;
        for (Creature* ooze : Resolve(bot, st->littleOozes))
            if (LittleOozeVictim(ooze) != bot)
                others.push_back(ooze);

        target = Nearest(bot->GetPosition(), others);
    }

    if (target)
    {
        spot = target->GetPosition();
        return bot->GetExactDist2d(spot) > RF_MERGE_TOUCH - 2.0f;
    }

    spot = OozeSpot(*st);
    std::vector<Hazard> const& hazards = HazardsFor(botAI, *st);
    bool const covered = std::any_of(hazards.begin(), hazards.end(), [&spot](Hazard const& hazard)
                                     { return hazard.at.GetExactDist2d(spot) < hazard.radius + DODGE_TRIGGER; });
    return !covered && bot->GetExactDist2d(spot) > RF_STAND_UNPARK;
}

bool IsRfCarrierHeld(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    return st && CarrierWalks(bot, *st);
}

Player* GetRfDispelTarget(PlayerbotAI* botAI, char const*& spell)
{
    RfState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    Player* bot = botAI->GetBot();
    for (auto const& [carrierGuid, dispeller] : st->dispel.Raw())
    {
        if (dispeller != bot->GetGUID())
            continue;

        Player* carrier = ObjectAccessor::FindPlayer(carrierGuid);
        if (!carrier || !IccSameInstance(bot, carrier))
            continue;

        spell = ReadyCure(bot, carrier);
        if (spell)
            return carrier;
    }

    return nullptr;
}

bool GetRfCureWalk(PlayerbotAI* botAI, Position& spot)
{
    Creature* boss = nullptr;
    RfState* st = LiveState(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive())
        return false;

    auto const duty = st->cureWalk.find(bot->GetGUID());
    if (duty == st->cureWalk.end())
        return false;

    Player* carrier = ObjectAccessor::FindPlayer(duty->second);
    if (!carrier || !IccSameInstance(bot, carrier) || ReadyCure(bot, carrier) || !ReadyCure(bot, carrier, false))
        return false;

    std::vector<Hazard> const& hazards = HazardsFor(botAI, *st);
    float const base = carrier->GetAngle(bot);
    for (int32 tried = 0; tried < CURE_WALK_TRIES; ++tried)
    {
        float const swing =
            static_cast<float>((tried + 1) / 2) * (tried % 2 ? 1.0f : -1.0f) * CURE_WALK_STEP_DEG * PI_F / 180.0f;
        Position const at(carrier->GetPositionX() + CURE_WALK_REACH * std::cos(base + swing),
                          carrier->GetPositionY() + CURE_WALK_REACH * std::sin(base + swing),
                          ICC_RF_CENTER.GetPositionZ());
        if (at.GetExactDist2d(ICC_RF_CENTER) > RF_ROOM_RADIUS ||
            !ClearOfSpray(*st, boss, at.GetPositionX(), at.GetPositionY(), SPRAY_MARGIN) ||
            std::any_of(hazards.begin(), hazards.end(), [&at](Hazard const& hazard)
                        { return hazard.at.GetExactDist2d(at) < hazard.radius + DODGE_MARGIN; }))
            continue;

        spot = at;
        return true;
    }

    return false;
}

bool IsRfCureWalkHeld(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    return st && st->cureWalk.count(botAI->GetBot()->GetGUID());
}

char const* GetRfTotemSwap(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_SHAMAN || !IsDutyBot(bot) || !IsRfLive(botAI))
        return nullptr;

    ObjectGuid const guid = bot->m_SummonSlot[SUMMON_SLOT_TOTEM_WATER];
    Creature* totem = guid.IsEmpty() ? nullptr : bot->GetMap()->GetCreature(guid);
    if (!totem || totem->GetUInt32Value(UNIT_CREATED_BY_SPELL) != SPELL_RF_CLEANSING_TOTEM)
        return nullptr;

    for (char const* name : WATER_TOTEM_SWAPS)
        if (botAI->CanCastSpell(name, bot))
            return name;

    return nullptr;
}

char const* GetRfHazardKind(PlayerbotAI* botAI)
{
    Creature* boss = nullptr;
    RfState* st = LiveState(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive())
        return nullptr;

    if (!ClearOfSpray(*st, boss, bot->GetPositionX(), bot->GetPositionY(), DODGE_TRIGGER))
        return "spray";

    for (Hazard const& hazard : HazardsFor(botAI, *st))
        if (hazard.at.GetExactDist2d(bot) < hazard.radius + DODGE_TRIGGER)
            return hazard.kind;

    return nullptr;
}

bool FindRfDodgeSpot(PlayerbotAI* botAI, Position& spot, bool& interrupt, std::string& branch)
{
    Creature* boss = nullptr;
    RfState* st = LiveState(botAI, &boss);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    std::vector<Hazard> const& hazards = HazardsFor(botAI, *st);
    Position const from = bot->GetPosition();
    bool const sprayed = !ClearOfSpray(*st, boss, from.GetPositionX(), from.GetPositionY(), DODGE_TRIGGER);

    interrupt = sprayed;
    bool burst = false;
    std::vector<HazardCircle> crossing;
    for (Hazard const& hazard : hazards)
    {
        bool const inside = hazard.at.GetExactDist2d(from) < hazard.radius + DODGE_TRIGGER;
        interrupt = interrupt || (inside && hazard.urgent);
        burst = burst || (inside && hazard.kind == KIND_BURST);
        if (!inside)
            crossing.emplace_back(hazard.at, hazard.radius);
    }

    bool const melee = IsMeleeDuty(bot, *st);
    bool const kiter = IsKiterBot(bot, *st);
    Creature* towed = kiter ? DutyOoze(bot, *st) : nullptr;
    bool const keepOut = kiter && (burst || (towed && towed->GetVictim() == bot));
    Position anchor;
    Position const* preferNear = nullptr;
    if (melee)
    {
        anchor = boss->GetPosition();
        preferNear = &anchor;
    }
    else if (Stand const* stand = HeldStand(botAI))
    {
        anchor = stand->spot;
        preferNear = &anchor;
    }

    HazardSweepCache sweep;
    auto const findSpot = [&](float margin, bool keepMelee, bool checkWalk)
    {
        std::vector<HazardCircle> circles;
        circles.reserve(hazards.size() + 1);
        for (Hazard const& hazard : hazards)
            circles.emplace_back(hazard.at, hazard.radius + margin);

        if (circles.empty())
            circles.emplace_back(from, 1.0f);

        auto const accept = [&](float x, float y)
        {
            Position const to(x, y, from.GetPositionZ());
            float const out = to.GetExactDist2d(ICC_RF_CENTER);
            if (out > RF_ROOM_RADIUS || (keepOut && out < KITE_KEEP_OUT) ||
                !ClearOfSpray(*st, boss, x, y, SPRAY_MARGIN))
                return false;

            if (keepMelee && boss->GetExactDist2d(x, y) > MELEE_KEEP)
                return false;

            return !checkWalk || std::none_of(crossing.begin(), crossing.end(), [&](HazardCircle const& circle)
                                              { return SegmentCrossesCircle(from, to, circle); });
        };

        return FindNearestPositionClearOfHazards(bot, circles, DODGE_SWEEP_RADIUS, 2.0f, PI_F / 8.0f, preferNear,
                                                 accept, &sweep);
    };

    struct Attempt
    {
        float margin;
        bool keepMelee;
        bool checkWalk;
        char const* branch;
    };
    Attempt const attempts[] = {
        {DODGE_MARGIN, true, true, "move"},  {DODGE_MARGIN, false, true, "move"},
        {DODGE_TIGHT, false, true, "tight"}, {DODGE_TIGHT, false, false, "cross"},
    };

    for (Attempt const& attempt : attempts)
    {
        if (attempt.keepMelee && !melee)
            continue;

        spot = findSpot(attempt.margin, attempt.keepMelee, attempt.checkWalk);
        if (spot != Position())
        {
            branch = attempt.branch;
            return true;
        }
    }

    branch = "none";
    return false;
}

bool IsRfDodgeSpotClear(PlayerbotAI* botAI, Position const& spot)
{
    Creature* boss = nullptr;
    RfState* st = LiveState(botAI, &boss);
    if (!st || !ClearOfSpray(*st, boss, spot.GetPositionX(), spot.GetPositionY(), DODGE_TRIGGER))
        return false;

    std::vector<Hazard> const& hazards = HazardsFor(botAI, *st);
    return std::none_of(hazards.begin(), hazards.end(), [&spot](Hazard const& hazard)
                        { return hazard.at.GetExactDist2d(spot) < hazard.radius + DODGE_TIGHT; });
}

bool IsRfHazardHeld(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    return st && (st->spray || LiveMarks(*st, getMSTime()));
}

bool IsRfHoldingStand(PlayerbotAI* botAI) { return HeldStand(botAI) != nullptr; }

bool GetRfStandWalk(PlayerbotAI* botAI, Position& spot)
{
    Creature* boss = nullptr;
    RfState* st = LiveState(botAI, &boss);
    Stand const* stand = st ? HeldStand(botAI) : nullptr;
    if (!stand || stand->parked)
        return false;

    if (!ClearOfSpray(*st, boss, stand->spot.GetPositionX(), stand->spot.GetPositionY(), DODGE_TRIGGER))
        return false;

    std::vector<Hazard> const& hazards = HazardsFor(botAI, *st);
    if (std::any_of(hazards.begin(), hazards.end(), [stand](Hazard const& hazard)
                    { return hazard.at.GetExactDist2d(stand->spot) < hazard.radius + DODGE_TRIGGER; }))
        return false;

    spot = stand->spot;
    return true;
}

bool IsRfMovementOwned(PlayerbotAI* botAI)
{
    return IsRfHoldingStand(botAI) || IsRfKiteHeld(botAI) || IsRfCarrierHeld(botAI) || IsRfCureWalkHeld(botAI);
}

uint32 GetRfTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !bot->IsAlive() || !IsDutyBot(bot))
        return 0;

    bool const tank = IsTankPlayer(bot);
    bool const kiter = IsKiterBot(bot, *st);
    if (!tank && !kiter)
        return 0;

    std::vector<Creature*> const oozes = Resolve(bot, st->bigOozes);
    bool window = bot->GetHealthPct() < RF_TANK_DEFENSIVE_PCT || (kiter && GassedWithOozeNear(*st, bot, oozes));
    for (Creature* ooze : oozes)
        window = window || (ooze->GetVictim() == bot && (!tank || Stacks(ooze) >= RF_OOZE_DEFENSIVE_STACKS) &&
                            ooze->GetExactDist2d(bot) <= OOZE_REACH);

    if (!window)
    {
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    return tank ? NextTankDefensive(botAI, bot, noteKind, false) : NextPhysicalDefensive(botAI, bot, noteKind, 0);
}

bool IsRfTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsRfLive(botAI) && botAI->GetBot()->GetHealthPct() >= RF_TANK_LAST_RESORT_PCT;
}

bool IsRfAreaDamageHeld(PlayerbotAI* botAI)
{
    RfState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || IsTankPlayer(bot) || IsKiterBot(bot, *st))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    auto const oozeClose = [&](std::vector<ObjectGuid> const& guids)
    {
        for (Creature* ooze : Resolve(bot, guids))
            if (ooze->GetExactDist2d(bot) < SPLASH_GUARD || (target && ooze->GetExactDist2d(target) < SPLASH_GUARD))
                return true;

        return false;
    };

    return oozeClose(st->bigOozes) || oozeClose(st->littleOozes);
}

namespace
{
void CallHumanTank(RfState& st, Player* sender, Creature* boss, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;

    std::vector<Creature*> const oozes = Resolve(sender, st.bigOozes);
    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        bool oozeHit = false;
        bool hit = boss->GetVictim() == member;
        for (Creature* ooze : oozes)
            if (ooze->GetVictim() == member)
            {
                hit = true;
                oozeHit = oozeHit ||
                          (Stacks(ooze) >= RF_OOZE_DEFENSIVE_STACKS && ooze->GetExactDist2d(member) <= OOZE_REACH);
            }

        bool const low = hit && member->GetHealthPct() < RF_TANK_DEFENSIVE_PCT;
        bool const gassed = GassedWithOozeNear(st, member, oozes);
        if (!oozeHit && !low && !gassed)
            continue;

        char const* why = low ? "you are low" : oozeHit ? "the Big Ooze is at full strength"
                                                        : "Vile Gas has you and the Big Ooze is closing";
        uint32 const spellId = ReadyTankDefensiveSpell(member, false, 0, 0);
        if (spellId)
            CallHumanTankDefensive(sender, member, spellId, 0, why, "rf.humancall");
    }
}

void NoteDuties(RfState const& st, Player* bot)
{
    if (!RaidObs::Active())
        return;

    if (IsKiterBot(bot, st))
    {
        Creature* ooze = nullptr;
        KiteMode const mode = ModeOf(bot, st, ooze);
        char const* name = ModeName(mode, st.kiteWading && KiterWet(st, bot));
        NoteDerived(bot, "rf.bigooze", ooze ? std::string(name) + " " + DescribeGuid(ooze) : name);
    }

    auto const walk = st.cureWalk.find(bot->GetGUID());
    NoteDerived(bot, "rf.curewalk", walk != st.cureWalk.end() ? RaidObs::DescribeAssignment(walk->second) : "none");

    if (!CarrierWalks(bot, st))
    {
        NoteDerived(bot, "rf.carrier", "none");
        return;
    }

    std::vector<Creature*> const bigs = MergeableOozes(bot, st);
    if (!bigs.empty())
    {
        NoteDerived(bot, "rf.carrier", "ooze");
        return;
    }

    bool const partner = std::any_of(st.littleOozes.begin(), st.littleOozes.end(),
                                     [&](ObjectGuid const& guid)
                                     {
                                         Creature* ooze = bot->GetMap()->GetCreature(guid);
                                         return ooze && LittleOozeVictim(ooze) != bot;
                                     });
    NoteDerived(bot, "rf.carrier", partner ? "little" : "spot");
}
}

void TickRfEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    Creature* rotface = RotfaceUnit(bot);
    if (!rotface || !rotface->IsAlive() || !rotface->IsInCombat())
    {
        if (rfStates.Find(bot->GetInstanceId()))
            rfStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* boss = GetRfBoss(botAI);
    if (!boss)
        return;

    RfState& st = rfStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    Scan(st, bot, boss, now);
    RefreshDuties(st, bot, boss, now);
    CallHumanTank(st, bot, boss, now);
    NoteDuties(st, bot);
    UpdateStand(st, bot, boss);
}
