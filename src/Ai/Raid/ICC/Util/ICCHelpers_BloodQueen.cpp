/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_BloodQueen.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "ICCSeams.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidAntiFear.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidDirectTargeting.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidResistAura.h"
#include "RaidTankDefensive.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float LIVE_RANGE = 70.0f;
constexpr float SCAN_RANGE = 100.0f;
constexpr uint32 SCAN_MS = 200;
constexpr uint32 DUTY_MS = 250;
constexpr uint32 HUMAN_CALL_CHECK_MS = 500;

constexpr uint32 BERSERK_MS = 330000;
constexpr uint32 END_WINDOW_MS = 60000;
constexpr uint32 FIRST_BITE_MS = 20000;
constexpr uint32 FIRST_AIR_MS_10 = 124000;
constexpr uint32 FIRST_AIR_MS_25 = 127000;
constexpr uint32 NEXT_AIR_MS_10 = 120000;
constexpr uint32 NEXT_AIR_MS_25 = 100000;
constexpr uint32 CENTER_WALK_MS = 3000;
constexpr float CENTER_ARRIVED = 1.5f;
constexpr uint32 FEAR_LEAD_MS = 8000;

constexpr float BRAKE_CAP = 0.9f;
constexpr float BRAKE_RELEASE = 0.8f;

constexpr float SHADOW_RADIUS = 4.0f;
constexpr float SHADOW_TRIGGER = SHADOW_RADIUS + 0.5f;
constexpr float SHADOW_CLEAR = SHADOW_RADIUS + BQL_STAND_PARK + 1.0f;
constexpr float LANE_CLEAR = SHADOW_RADIUS + 1.5f;
constexpr float LANE_STEP = 5.0f;
constexpr float LANE_REACHED = 3.0f;
constexpr float LANE_INNER_PENALTY = 6.0f;
constexpr float LANE_RUN_WANTED = 45.0f;
constexpr float DODGE_SWEEP = 15.0f;
constexpr float MELEE_KEEP = 3.0f;

constexpr float DETOUR_SWEEP = 16.0f;
constexpr float DETOUR_MAX_EXTRA = 8.0f;
constexpr float DETOUR_REACHED = 2.0f;

constexpr float PACT_PARK = 1.0f;
constexpr float PACT_SPOT_CLEAR = SHADOW_RADIUS + 2.0f;
constexpr uint32 PACT_HUMAN_WAIT_MS = 3000;
constexpr float PACT_HUMAN_AWAY = 4.0f;
constexpr float PACT_HUMAN_PARK = 2.0f;

constexpr float HEAL_TAKE = 36.0f;
constexpr float HEAL_KEEP = 38.0f;
constexpr float RANGED_TAKE = 35.5f;
constexpr float RANGED_KEEP = 37.5f;
constexpr float HUNTER_TAKE = 40.0f;
constexpr float HUNTER_KEEP = 42.0f;

constexpr float GROUP_RADIUS = 7.5f;
constexpr float GROUP_DRIFT = 1.5f;
constexpr uint32 STILL_MS = 500;
constexpr uint8 GROUP_MAX = 3;
constexpr float GROUP_MAX_OFFSET = 2.1f;
constexpr float ROOM_RADIUS = 36.0f;

constexpr uint32 BITE_SAFE_HEALTH = 15000;
constexpr float CC_RANGE_CAP = 30.0f;
constexpr uint32 KILL_GRACE_MS = 2000;

constexpr float MASS_DISPEL_RANGE = 30.0f;
constexpr float MASS_DISPEL_RADIUS = 15.0f;
constexpr uint32 MASS_DISPEL_MAX = 10;
constexpr uint32 MASS_DISPEL_MIN = 3;
constexpr int32 MASS_DISPEL_CAST_MS = 1500;
constexpr float TAUNT_RANGE = 30.0f;
constexpr float MAIN_LEASH = 15.0f;

constexpr uint32 WHIRL_FLIGHT_MS = 850;
constexpr float WHIRL_TICKS = 3.0f;
constexpr float PRESENCE_PER_STACK = 0.05f;

constexpr uint32 LUST_LATEST_MS = 280000;
constexpr uint32 VAMPIRE_PEAK_10 = 8;
constexpr uint32 VAMPIRE_PEAK_25 = 16;

enum class TankRole : uint8
{
    None = 0,
    Main = 1,
    Mirror = 2,
    Spare = 3
};

enum class StandKind : uint8
{
    None,
    Tank,
    Mirror,
    Group,
    Node,
    Air
};

struct Stand
{
    StandKind kind = StandKind::None;
    Position spot;
    bool parked = false;
};

struct LanePlan
{
    uint8 lane = 0;
    int8 dir = 1;
    int16 next = 0;
};

struct TargetMemo
{
    uint32 ms = 0;
    bool set = false;
    Unit* pick = nullptr;
};

struct BqlState
{
    uint32 pullMs = 0;
    bool vampireSeen = false;
    uint32 vampires = 0;

    uint32 scanMs = 0;
    bool scanned = false;
    std::vector<Position> shadows;
    std::vector<ObjectGuid> shadowOwners;

    uint32 dutyMs = 0;
    bool dutiesRan = false;
    ObjectGuid mainTank;
    RaidObs::ObsGuidMap<uint8> tank{"bql.tank"};
    RaidObs::ObsGuidMap<uint8> slot{"bql.slot"};
    RaidObs::ObsGuidMap<uint8> airslot{"bql.airslot"};
    RaidObs::ObsGuidMap<uint8> group{"bql.group"};
    RaidObs::ObsGuidMap<ObjectGuid> bite{"bql.bite"};
    RaidObs::ObsGuidMap<ObjectGuid> cc{"bql.cc"};
    std::unordered_map<ObjectGuid, uint32> looseSince;
    RaidObs::ObsGuidSet kill{"bql.kill"};
    RaidObs::ObsGuidMap<ObjectGuid> massDispel{"bql.massdispel"};
    RaidObs::ObsGuidMap<uint8> curve{"bql.curve"};
    RaidObs::ObsGuidSet brake{"bql.brake"};

    Position herLatch;
    bool herLatched = false;
    Position herLast;
    uint32 herStillMs = 0;
    Position groupSpots[GROUP_MAX];
    bool groupValid[GROUP_MAX] = {};
    float groupOffset[GROUP_MAX] = {};
    bool groupOffsetHeld[GROUP_MAX] = {};

    IccBqlPhase phase = IccBqlPhase::Ground;
    uint32 nextCenterMs = 0;
    bool centerArrived = false;
    bool whirlSeen = false;
    bool whirlDone = false;

    uint8 pactSpot = 0;
    uint32 pactSinceMs = 0;
    ObjectGuid pactHuman;

    uint32 humanCallMs = 0;
    bool humanCallRan = false;

    std::unordered_map<ObjectGuid, LanePlan> lanes;
    std::unordered_map<ObjectGuid, Stand> stands;
    std::unordered_map<ObjectGuid, TargetMemo> targets;
};

RaidInstanceState<BqlState> bqlStates;

bool IsTankPlayer(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

bool IsDutyBot(Player* player) { return GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

bool Is25(Player* bot) { return IsRaid25Man(bot); }

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

bool IsMindControlled(Unit* member) { return member && member->HasAura(SPELL_BQL_UNCONTROLLABLE_FRENZY); }

bool IsSkullUnit(Unit* unit) { return unit && (unit->GetEntry() == NPC_BQL_BLOOD_QUEEN || IsMindControlled(unit)); }

Creature* QueenUnit(Player* bot)
{
    InstanceScript* instance = bot->GetMapId() == ICC_MAP_ID ? bot->GetInstanceScript() : nullptr;
    return instance ? ObjectAccessor::GetCreature(*bot, instance->GetGuidData(BQL_DATA_BLOOD_QUEEN)) : nullptr;
}

IccBqlPhase PhaseOf(Unit* boss)
{
    Creature* queen = boss ? boss->ToCreature() : nullptr;
    if (queen && queen->HasReactState(REACT_PASSIVE))
        return boss->IsLevitating() ? IccBqlPhase::Air : IccBqlPhase::Center;

    return IccBqlPhase::Ground;
}

bool IsFrenzied(Player* member)
{
    return member->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_BQL_FRENZIED_BLOODTHIRST, member));
}

bool IsVampire(Player* member)
{
    return member->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_BQL_ESSENCE, member)) ||
           member->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_BQL_ESSENCE_BITTEN, member)) ||
           IsFrenzied(member);
}

bool IsCarrier(Player* member)
{
    return member->HasAura(SPELL_BQL_SHADOWS_WARNING) || member->HasAura(SPELL_BQL_SHADOWS_TRAIL);
}

bool IsShadowWalker(Player* member) { return IsCarrier(member) && !IsFrenzied(member); }

bool IsPactWalker(Player* member)
{
    return member->HasAura(SPELL_BQL_PACT) && !IsCarrier(member) && !IsFrenzied(member);
}

bool OnSideDuty(Player* bot) { return IsFrenzied(bot) || IsCarrier(bot) || IsPactWalker(bot); }

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

bool InShadow(BqlState const& st, Position const& pos, float reach)
{
    return std::any_of(st.shadows.begin(), st.shadows.end(),
                       [&](Position const& shadow) { return shadow.GetExactDist2d(pos) < reach; });
}

Position const& NodeSpot(uint8 id) { return ICC_BQL_NODES[id - 1]; }

bool NodeClear(BqlState const& st, uint8 id)
{
    return id >= 1 && id <= BQL_NODE_COUNT && !InShadow(st, NodeSpot(id), SHADOW_CLEAR);
}

bool GroundNodeUsable(BqlState const& st, uint8 id) { return id <= BQL_GROUND_NODES && NodeClear(st, id); }

Position const* LanePoints(uint8 lane, int16& count)
{
    if (lane == 0)
    {
        count = static_cast<int16>(sizeof(ICC_BQL_LANE_OUTER) / sizeof(ICC_BQL_LANE_OUTER[0]));
        return ICC_BQL_LANE_OUTER;
    }

    count = static_cast<int16>(sizeof(ICC_BQL_LANE_INNER) / sizeof(ICC_BQL_LANE_INNER[0]));
    return ICC_BQL_LANE_INNER;
}

TankRole RoleOf(BqlState const& st, Player* bot)
{
    auto const itr = st.tank.find(bot->GetGUID());
    return itr != st.tank.end() ? static_cast<TankRole>(itr->second) : TankRole::None;
}

uint8 Held(RaidObs::ObsGuidMap<uint8> const& map, Player* bot)
{
    auto const itr = map.find(bot->GetGUID());
    return itr != map.end() ? itr->second : 0;
}

bool AirSpread(BqlState const& st) { return st.phase != IccBqlPhase::Ground && !st.whirlDone; }

bool EndWindow(BqlState const& st, uint32 now)
{
    return st.pullMs && getMSTimeDiff(st.pullMs, now) >= BERSERK_MS - END_WINDOW_MS;
}

BqlState* LiveState(PlayerbotAI* botAI, Creature** queen = nullptr)
{
    Creature* boss = GetBqlQueen(botAI);
    if (queen)
        *queen = boss;

    return boss ? bqlStates.Find(botAI->GetBot()->GetInstanceId()) : nullptr;
}

void Scan(BqlState& st, Player* bot, uint32 now)
{
    if (st.scanned && getMSTimeDiff(st.scanMs, now) < SCAN_MS)
        return;

    st.scanned = true;
    st.scanMs = now;
    st.shadows.clear();
    st.shadowOwners.clear();
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_BQL_SWARMING_SHADOWS, SCAN_RANGE);
    for (Creature* shadow : found)
    {
        if (!shadow->IsAlive())
            continue;

        TempSummon* summon = shadow->ToTempSummon();
        st.shadows.push_back(shadow->GetPosition());
        st.shadowOwners.push_back(summon ? summon->GetSummonerGUID() : ObjectGuid::Empty);
    }
}

int8 MirrorTier(Player* member)
{
    if (!IsDutyBot(member) || IsTankPlayer(member) || !IsRaidDpsBot(member) || IsMindControlled(member))
        return -1;

    if (PlayerbotAI::IsRanged(member))
        return 2;

    switch (member->getClass())
    {
        case CLASS_DEATH_KNIGHT:
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
            return 0;
        default:
            return 1;
    }
}

Player* PickMirror(BqlState const& st, std::vector<Player*> const& members)
{
    Player* best = nullptr;
    int8 bestTier = 0;
    for (Player* member : members)
    {
        int8 const tier = MirrorTier(member);
        if (tier < 0)
            continue;

        if (RoleOf(st, member) == TankRole::Mirror)
            return member;

        bool better = !best || tier < bestTier;
        if (!better && tier == bestTier)
            better = member->GetMaxHealth() != best->GetMaxHealth() ? member->GetMaxHealth() > best->GetMaxHealth()
                                                                   : member->GetGUID() < best->GetGUID();

        if (better)
        {
            best = member;
            bestTier = tier;
        }
    }

    return best;
}

void AssignTanks(BqlState& st, Player* bot, std::vector<Player*> const& members)
{
    size_t const tankPlayers = std::count_if(members.begin(), members.end(), &IsTankPlayer);
    std::vector<std::pair<uint8, Player*>> ranked;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        uint8 rank = PlayerbotAI::IsMainTank(member) ? 0 : 10;
        for (uint8 index = 0; rank == 10 && index < 4; ++index)
            if (PlayerbotAI::IsAssistTankOfIndex(member, index, true))
                rank = index + 1;

        ranked.emplace_back(rank, member);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](auto const& a, auto const& b)
              { return a.first != b.first ? a.first < b.first : a.second->GetGUID() < b.second->GetGUID(); });

    Player* groupMain = GetGroupMainTank(bot);
    bool const humanMain = groupMain && groupMain->IsAlive() && !IsDutyBot(groupMain) &&
                           IccSameInstance(bot, groupMain) && IsTankPlayer(groupMain);

    std::unordered_map<ObjectGuid, TankRole> roles;
    for (size_t i = 0; i < ranked.size(); ++i)
    {
        size_t const place = humanMain ? i + 1 : i;
        roles[ranked[i].second->GetGUID()] = place == 0   ? TankRole::Main
                                             : place == 1 ? TankRole::Mirror
                                                          : TankRole::Spare;
    }

    st.mainTank = humanMain ? groupMain->GetGUID() : (ranked.empty() ? ObjectGuid::Empty : ranked[0].second->GetGUID());
    if (tankPlayers == 1 && st.mainTank)
        if (Player* mirror = PickMirror(st, members))
            roles[mirror->GetGUID()] = TankRole::Mirror;

    std::vector<ObjectGuid> dropped;
    for (auto const& entry : st.tank)
        if (!roles.count(entry.first))
            dropped.push_back(entry.first);

    for (ObjectGuid const& guid : dropped)
        st.tank.erase(guid);

    for (auto const& [guid, role] : roles)
        st.tank[guid] = static_cast<uint8>(role);
}

void UpdatePhase(BqlState& st, Creature* queen, Player* bot, uint32 now)
{
    IccBqlPhase const phase = PhaseOf(queen);
    if (!st.pullMs)
    {
        st.pullMs = now;
        st.nextCenterMs = now + (Is25(bot) ? FIRST_AIR_MS_25 : FIRST_AIR_MS_10);
    }

    uint32 const nextAir = Is25(bot) ? NEXT_AIR_MS_25 : NEXT_AIR_MS_10;
    if (phase == IccBqlPhase::Center && st.phase != IccBqlPhase::Center)
    {
        st.centerArrived = false;
        st.nextCenterMs = now + CENTER_WALK_MS + nextAir;
    }

    if (phase == IccBqlPhase::Center && !st.centerArrived &&
        queen->GetExactDist2d(ICC_BQL_ROOM_CENTER) <= CENTER_ARRIVED)
    {
        st.centerArrived = true;
        st.nextCenterMs = now + nextAir;
    }

    bool const whirling = queen->HasAura(SPELL_BQL_BLOODBOLT_WHIRL);
    if (phase == IccBqlPhase::Ground)
    {
        st.whirlSeen = false;
        st.whirlDone = false;
    }
    else if (whirling)
        st.whirlSeen = true;
    else if (st.whirlSeen)
        st.whirlDone = true;

    st.phase = phase;
}

void UpdateHer(BqlState& st, Creature* queen, uint32 now)
{
    Position const pos = queen->GetPosition();
    if (queen->isMoving() || pos.GetExactDist2d(st.herLast) > 0.3f)
    {
        st.herStillMs = now;
        st.herLast = pos;
    }

    bool const still = getMSTimeDiff(st.herStillMs, now) >= STILL_MS;
    if (still && (!st.herLatched || st.herLatch.GetExactDist2d(pos) > GROUP_DRIFT))
    {
        st.herLatch = pos;
        st.herLatched = true;
        std::fill(std::begin(st.groupOffsetHeld), std::end(st.groupOffsetHeld), false);
    }
}

float DefaultGroupOffset(uint8 index, uint8 count)
{
    float const step = static_cast<float>(M_PI) / (count == 2 ? 4.0f : 3.0f);
    if (count == 1)
        return 0.0f;

    if (count == 2)
        return index == 0 ? step : -step;

    return index == 0 ? 0.0f : index == 1 ? step : -step;
}

void UpdateGroupSpots(BqlState& st, Creature* queen, uint8 count)
{
    std::fill(std::begin(st.groupValid), std::end(st.groupValid), false);
    Unit* victim = queen->GetVictim();
    if (st.phase != IccBqlPhase::Ground || !st.herLatched || !victim ||
        st.herLatch.GetExactDist2d(queen->GetPosition()) > GROUP_DRIFT)
        return;

    float const back = std::atan2(st.herLatch.GetPositionY() - victim->GetPositionY(),
                                  st.herLatch.GetPositionX() - victim->GetPositionX());
    float const turn = static_cast<float>(M_PI) / 6.0f;
    for (uint8 g = 0; g < count && g < GROUP_MAX; ++g)
    {
        float const base = DefaultGroupOffset(g, count);
        std::vector<float> offsets;
        if (st.groupOffsetHeld[g])
            offsets.push_back(st.groupOffset[g]);
        for (float k : {0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 3.0f, -3.0f})
            if (std::fabs(base + k * turn) <= GROUP_MAX_OFFSET)
                offsets.push_back(base + k * turn);

        for (float offset : offsets)
        {
            float const x = st.herLatch.GetPositionX() + GROUP_RADIUS * std::cos(back + offset);
            float const y = st.herLatch.GetPositionY() + GROUP_RADIUS * std::sin(back + offset);
            float const z = queen->GetMapHeight(x, y, st.herLatch.GetPositionZ() + 2.0f);
            Position const spot(x, y, z);
            bool clear = z > INVALID_HEIGHT && std::fabs(z - st.herLatch.GetPositionZ()) < 2.5f &&
                         spot.GetExactDist2d(ICC_BQL_ROOM_CENTER) <= ROOM_RADIUS && !InShadow(st, spot, SHADOW_CLEAR);
            for (uint8 other = 0; clear && other < g; ++other)
                clear = !st.groupValid[other] || st.groupSpots[other].GetExactDist2d(spot) >= BQL_NODE_SPACING;

            if (!clear)
                continue;

            st.groupSpots[g] = spot;
            st.groupValid[g] = true;
            st.groupOffset[g] = offset;
            st.groupOffsetHeld[g] = true;
            break;
        }
    }
}

float RangedReach(Player* member, bool keep)
{
    bool const hunter = member->getClass() == CLASS_HUNTER;
    return keep ? (hunter ? HUNTER_KEEP : RANGED_KEEP) : (hunter ? HUNTER_TAKE : RANGED_TAKE);
}

Player* MainTank(BqlState const& st, Player* bot)
{
    Player* main = st.mainTank ? ObjectAccessor::GetPlayer(*bot, st.mainTank) : nullptr;
    return main && main->IsAlive() && IccSameInstance(bot, main) ? main : nullptr;
}

Position TankAnchor(BqlState const& st, Player* bot)
{
    Player* main = MainTank(st, bot);
    return main ? main->GetPosition() : ICC_BQL_TANK_SPOT;
}

void AssignSlots(BqlState& st, Player* bot, std::vector<Player*> const& members, Creature* queen)
{
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || IsTankPlayer(member) || RoleOf(st, member) == TankRole::Mirror)
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
    }

    std::sort(healers.begin(), healers.end(), IccGuidLess());
    std::sort(ranged.begin(), ranged.end(), IccGuidLess());

    std::unordered_set<ObjectGuid> roster;
    for (Player* member : healers)
        roster.insert(member->GetGUID());
    for (Player* member : ranged)
        roster.insert(member->GetGUID());

    for (auto itr = st.slot.begin(); itr != st.slot.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.slot.erase(itr);

    bool const frozen = st.phase != IccBqlPhase::Ground || queen->isMoving();
    Position const tank = TankAnchor(st, bot);
    bool used[BQL_NODE_COUNT + 1] = {};

    auto const place = [&](Player* member, auto&& keeps, auto&& takes)
    {
        uint8 const id = Held(st.slot, member);
        if (GroundNodeUsable(st, id) && !used[id] && (frozen || keeps(NodeSpot(id))))
        {
            used[id] = true;
            return;
        }

        uint8 best = 0;
        float bestDistance = 0.0f;
        for (uint8 node = 1; node <= BQL_GROUND_NODES; ++node)
        {
            if (used[node] || !GroundNodeUsable(st, node) || !takes(NodeSpot(node)))
                continue;

            float const distance = member->GetExactDist2d(NodeSpot(node));
            if (!best || distance < bestDistance)
            {
                best = node;
                bestDistance = distance;
            }
        }

        if (best)
        {
            used[best] = true;
            st.slot[member->GetGUID()] = best;
        }
        else
            st.slot.erase(member->GetGUID());
    };

    for (Player* healer : healers)
        place(
            healer,
            [&](Position const& node)
            { return node.GetExactDist2d(tank) <= HEAL_KEEP && node.GetExactDist2d(queen) <= HEAL_KEEP; },
            [&](Position const& node)
            { return node.GetExactDist2d(tank) <= HEAL_TAKE && node.GetExactDist2d(queen) <= HEAL_TAKE; });

    for (Player* member : ranged)
        place(
            member, [&](Position const& node) { return node.GetExactDist2d(queen) <= RangedReach(member, true); },
            [&](Position const& node) { return node.GetExactDist2d(queen) <= RangedReach(member, false); });
}

uint8 AssignGroups(BqlState& st, std::vector<Player*> const& members)
{
    std::vector<Player*> melee;
    for (Player* member : members)
    {
        if (!IsDutyBot(member) || PlayerbotAI::IsHeal(member) || PlayerbotAI::IsRanged(member))
            continue;

        TankRole const role = RoleOf(st, member);
        if (role == TankRole::Main || role == TankRole::Mirror)
            continue;

        melee.push_back(member);
    }

    std::sort(melee.begin(), melee.end(), IccGuidLess());
    std::unordered_set<ObjectGuid> roster;
    for (Player* member : melee)
        roster.insert(member->GetGUID());

    for (auto itr = st.group.begin(); itr != st.group.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.group.erase(itr);

    uint8 const count = static_cast<uint8>(std::min<size_t>(GROUP_MAX, melee.size()));
    std::vector<size_t> const sizes = IccBalancedGroupSizes(melee.size(), count);
    for (size_t i = 0; i < melee.size(); ++i)
        st.group[melee[i]->GetGUID()] = static_cast<uint8>(IccAssignedBucketIndex(i, sizes) + 1);

    return count;
}

void AssignAirSlots(BqlState& st, std::vector<Player*> const& members)
{
    if (!AirSpread(st))
    {
        std::vector<ObjectGuid> held;
        for (auto const& entry : st.airslot)
            held.push_back(entry.first);

        for (ObjectGuid const& guid : held)
            st.airslot.erase(guid);

        return;
    }

    bool used[BQL_NODE_COUNT + 1] = {};
    for (auto const& entry : st.slot)
        if (entry.second >= 1 && entry.second <= BQL_NODE_COUNT)
            used[entry.second] = true;

    std::vector<Player*> wanted;
    for (Player* member : members)
        if (IsDutyBot(member) && RoleOf(st, member) != TankRole::Main && !st.slot.count(member->GetGUID()))
            wanted.push_back(member);

    std::sort(wanted.begin(), wanted.end(), IccGuidLess());
    std::unordered_set<ObjectGuid> roster;
    for (Player* member : wanted)
        roster.insert(member->GetGUID());

    for (auto itr = st.airslot.begin(); itr != st.airslot.end();)
        itr = roster.count(itr->first) ? std::next(itr) : st.airslot.erase(itr);

    for (Player* member : wanted)
    {
        uint8 const id = Held(st.airslot, member);
        if (NodeClear(st, id) && !used[id])
        {
            used[id] = true;
            continue;
        }

        uint8 best = 0;
        float bestDistance = 0.0f;
        for (uint8 node = 1; node <= BQL_NODE_COUNT; ++node)
        {
            if (used[node] || !NodeClear(st, node))
                continue;

            float const distance = member->GetExactDist2d(NodeSpot(node));
            if (!best || distance < bestDistance)
            {
                best = node;
                bestDistance = distance;
            }
        }

        if (best)
        {
            used[best] = true;
            st.airslot[member->GetGUID()] = best;
        }
        else
            st.airslot.erase(member->GetGUID());
    }
}

uint8 BiteTier(BqlState const& st, Player* candidate)
{
    bool const tank = IsTankPlayer(candidate) || RoleOf(st, candidate) == TankRole::Mirror;
    uint8 tier = tank ? 1 : PlayerbotAI::IsHeal(candidate) ? 2 : 0;
    if (candidate->GetHealth() < BITE_SAFE_HEALTH)
        tier += 3;

    return tier;
}

bool BiteCandidate(BqlState const& st, Player* member, Unit* victim)
{
    return member->IsAlive() && member != victim && member->GetGUID() != st.mainTank && !IsVampire(member) &&
           !IsMindControlled(member) && !IsCarrier(member);
}

void AssignBites(BqlState& st, Player* bot, std::vector<Player*> const& members, Creature* queen)
{
    Unit* victim = queen->GetVictim();
    std::vector<Player*> biters;
    std::unordered_set<ObjectGuid> biterGuids;
    for (Player* member : members)
        if (IsDutyBot(member) && IsFrenzied(member))
        {
            biters.push_back(member);
            biterGuids.insert(member->GetGUID());
        }

    std::sort(biters.begin(), biters.end(), IccGuidLess());

    std::unordered_set<ObjectGuid> claimed;
    std::vector<ObjectGuid> ended;
    for (auto const& [biter, target] : st.bite)
    {
        Player* held = ObjectAccessor::GetPlayer(*bot, target);
        if (!biterGuids.count(biter) || !held || !IccSameInstance(bot, held) || !BiteCandidate(st, held, victim))
            ended.push_back(biter);
        else
            claimed.insert(target);
    }

    for (ObjectGuid const& biter : ended)
        st.bite.erase(biter);

    for (Player* biter : biters)
    {
        if (st.bite.count(biter->GetGUID()))
            continue;

        Player* best = nullptr;
        uint8 bestTier = 0;
        float bestDistance = 0.0f;
        for (Player* member : members)
        {
            if (claimed.count(member->GetGUID()) || !BiteCandidate(st, member, victim))
                continue;

            uint8 const tier = BiteTier(st, member);
            float const distance = biter->GetExactDist2d(member);
            if (!best || tier < bestTier || (tier == bestTier && distance < bestDistance))
            {
                best = member;
                bestTier = tier;
                bestDistance = distance;
            }
        }

        if (!best)
            continue;

        claimed.insert(best->GetGUID());
        st.bite[biter->GetGUID()] = best->GetGUID();
    }
}

bool IsControlled(Unit* victim)
{
    return victim->HasAuraType(SPELL_AURA_MOD_STUN) || victim->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
           victim->HasAuraType(SPELL_AURA_MOD_FEAR) || victim->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE);
}

uint32 CcSpellFor(PlayerbotAI* ai, Unit* victim)
{
    Player* caster = ai->GetBot();
    std::vector<char const*> names;
    switch (caster->getClass())
    {
        case CLASS_MAGE:
            names = {"polymorph"};
            break;
        case CLASS_DRUID:
            names = {"cyclone"};
            break;
        case CLASS_SHAMAN:
            names = {"hex"};
            break;
        case CLASS_PALADIN:
            names = {"repentance", "hammer of justice"};
            break;
        case CLASS_ROGUE:
            names = {"blind"};
            break;
        case CLASS_WARLOCK:
            names = {"death coil"};
            break;
        case CLASS_HUNTER:
            names = {"scatter shot"};
            break;
        default:
            return 0;
    }

    for (char const* name : names)
    {
        uint32 const spellId = ai->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || caster->HasSpellCooldown(spellId) ||
            info->CheckShapeshift(caster->GetShapeshiftForm()) != SPELL_CAST_OK)
            continue;

        if (victim->GetDiminishing(GetDiminishingReturnsGroupForSpell(info, false)) >= DIMINISHING_LEVEL_IMMUNE ||
            victim->IsImmunedToSpell(info))
            continue;

        if (caster->GetDistance(victim) > std::min(info->GetMaxRange(false), CC_RANGE_CAP))
            continue;

        return spellId;
    }

    return 0;
}

void AssignCc(BqlState& st, Player* bot, std::vector<Player*> const& members, uint32 now)
{
    std::vector<Player*> victims;
    std::unordered_set<ObjectGuid> victimGuids;
    for (Player* member : members)
        if (IsMindControlled(member))
        {
            victims.push_back(member);
            victimGuids.insert(member->GetGUID());
        }

    std::vector<ObjectGuid> gone;
    for (ObjectGuid const& victim : st.kill)
        if (!victimGuids.count(victim))
            gone.push_back(victim);

    for (ObjectGuid const& victim : gone)
        st.kill.erase(victim);

    for (auto itr = st.looseSince.begin(); itr != st.looseSince.end();)
        itr = victimGuids.count(itr->first) && !st.kill.count(itr->first) ? std::next(itr) : st.looseSince.erase(itr);

    auto const eligible = [&st](Player* member)
    {
        return IsDutyBot(member) && member->IsAlive() && !IsTankPlayer(member) && !IsVampire(member) &&
               !IsMindControlled(member) && !IsCarrier(member) && !member->HasAura(SPELL_BQL_PACT) &&
               RoleOf(st, member) != TankRole::Mirror;
    };

    std::unordered_set<ObjectGuid> covered;
    std::vector<ObjectGuid> ended;
    for (auto const& [holder, victim] : st.cc)
    {
        Player* member = ObjectAccessor::GetPlayer(*bot, holder);
        Player* target = ObjectAccessor::GetPlayer(*bot, victim);
        PlayerbotAI* ai = member ? GET_PLAYERBOT_AI(member) : nullptr;
        bool const spent = target && ai && !IsControlled(target) && !CcSpellFor(ai, target);
        if (!victimGuids.count(victim) || st.kill.count(victim) || !member || !IccSameInstance(bot, member) ||
            !eligible(member) || spent)
            ended.push_back(holder);
        else
            covered.insert(victim);
    }

    for (ObjectGuid const& holder : ended)
        st.cc.erase(holder);

    for (Player* victim : victims)
    {
        if (covered.count(victim->GetGUID()) || st.kill.count(victim->GetGUID()))
            continue;

        Player* best = nullptr;
        float bestDistance = 0.0f;
        for (Player* member : members)
        {
            if (member == victim || st.cc.count(member->GetGUID()) || !eligible(member))
                continue;

            PlayerbotAI* ai = GET_PLAYERBOT_AI(member);
            float const distance = member->GetExactDist2d(victim);
            if (ai && CcSpellFor(ai, victim) && (!best || distance < bestDistance))
            {
                best = member;
                bestDistance = distance;
            }
        }

        if (best)
        {
            st.cc[best->GetGUID()] = victim->GetGUID();
            covered.insert(victim->GetGUID());
        }
    }

    for (Player* victim : victims)
    {
        ObjectGuid const guid = victim->GetGUID();
        if (st.kill.count(guid))
            continue;

        if (covered.count(guid) || IsControlled(victim))
        {
            st.looseSince.erase(guid);
            continue;
        }

        uint32 const since = st.looseSince.emplace(guid, now).first->second;
        if (getMSTimeDiff(since, now) >= KILL_GRACE_MS)
        {
            st.looseSince.erase(guid);
            st.kill.insert(guid);
        }
    }
}

Player* KillTarget(BqlState const& st, Player* bot)
{
    if (st.kill.empty() || !IsRaidDpsBot(bot) || IsVampire(bot) || st.cc.count(bot->GetGUID()) ||
        RoleOf(st, bot) == TankRole::Mirror)
        return nullptr;

    Player* best = nullptr;
    float bestDistance = 0.0f;
    for (ObjectGuid const& guid : st.kill)
    {
        Player* victim = ObjectAccessor::GetPlayer(*bot, guid);
        if (!victim || victim == bot || !victim->IsAlive() || !victim->IsInCombat() || !IccSameInstance(bot, victim) ||
            !IsMindControlled(victim) || !bot->IsValidAttackTarget(victim))
            continue;

        float const distance = bot->GetExactDist2d(victim);
        if (!best || distance < bestDistance || (distance == bestDistance && victim->GetGUID() < best->GetGUID()))
        {
            best = victim;
            bestDistance = distance;
        }
    }

    return best;
}

void UpdatePactSpot(BqlState& st, std::vector<Player*> const& members, uint32 now)
{
    bool const up = std::any_of(members.begin(), members.end(),
                                [](Player* member) { return member->HasAura(SPELL_BQL_PACT); });
    if (!up)
    {
        st.pactSpot = 0;
        st.pactSinceMs = 0;
        st.pactHuman.Clear();
        return;
    }

    if (!st.pactSinceMs)
        st.pactSinceMs = now;

    if (!st.pactSpot || InShadow(st, ICC_BQL_PACT_SPOTS[st.pactSpot - 1], PACT_SPOT_CLEAR))
    {
        st.pactSpot = 1;
        for (uint8 i = 0; i < BQL_PACT_SPOT_COUNT; ++i)
            if (!InShadow(st, ICC_BQL_PACT_SPOTS[i], PACT_SPOT_CLEAR))
            {
                st.pactSpot = i + 1;
                break;
            }
    }

    if (st.pactHuman)
    {
        auto const held = std::find_if(members.begin(), members.end(),
                                       [&st](Player* member) { return member->GetGUID() == st.pactHuman; });
        if (held == members.end() || !(*held)->HasAura(SPELL_BQL_PACT))
            st.pactHuman.Clear();
    }

    if (st.pactHuman || getMSTimeDiff(st.pactSinceMs, now) < PACT_HUMAN_WAIT_MS)
        return;

    Position const& spot = ICC_BQL_PACT_SPOTS[st.pactSpot - 1];
    for (Player* member : members)
        if (!IsDutyBot(member) && member->HasAura(SPELL_BQL_PACT) && member->GetExactDist2d(spot) > PACT_HUMAN_AWAY &&
            (!st.pactHuman || member->GetGUID() < st.pactHuman))
            st.pactHuman = member->GetGUID();
}

void RefreshDuties(BqlState& st, Player* bot, Creature* queen, uint32 now)
{
    if (st.dutiesRan && getMSTimeDiff(st.dutyMs, now) < DUTY_MS)
        return;

    st.dutiesRan = true;
    st.dutyMs = now;
    std::vector<Player*> const members = MembersOnMap(bot);

    UpdatePhase(st, queen, bot, now);
    UpdateHer(st, queen, now);
    AssignTanks(st, bot, members);

    st.vampires = static_cast<uint32>(std::count_if(members.begin(), members.end(), &IsVampire));
    if (st.vampires || getMSTimeDiff(st.pullMs, now) >= FIRST_BITE_MS)
        st.vampireSeen = true;

    UpdatePactSpot(st, members, now);
    AssignSlots(st, bot, members, queen);
    uint8 const groups = AssignGroups(st, members);
    UpdateGroupSpots(st, queen, groups);
    AssignAirSlots(st, members);
    AssignBites(st, bot, members, queen);
    AssignCc(st, bot, members, now);
}

void UpdateBrake(BqlState& st, Player* bot, Creature* queen)
{
    bool hold = false;
    Unit* holder = queen->GetVictim();
    if (!holder)
        holder = MainTank(st, bot);

    if (IsDutyBot(bot) && !PlayerbotAI::IsHeal(bot) && RoleOf(st, bot) != TankRole::Main && holder &&
        holder != bot && !IsVampire(bot))
    {
        if ((IsTankPlayer(bot) || RoleOf(st, bot) == TankRole::Mirror) && !st.vampireSeen)
            hold = true;
        else
        {
            float const top = queen->GetThreatMgr().GetThreat(holder);
            float const own = queen->GetThreatMgr().GetThreat(bot);
            float const share = st.brake.count(bot->GetGUID()) ? BRAKE_RELEASE : BRAKE_CAP;
            hold = top > 0.0f && own > 0.0f && own >= share * top;
        }
    }

    if (hold)
        st.brake.insert(bot->GetGUID());
    else
        st.brake.erase(bot->GetGUID());
}

bool LanePointClear(BqlState const& st, Position const& point, ObjectGuid ignore = ObjectGuid::Empty)
{
    for (size_t i = 0; i < st.shadows.size(); ++i)
        if ((!ignore || st.shadowOwners[i] != ignore) && st.shadows[i].GetExactDist2d(point) < LANE_CLEAR)
            return false;

    return true;
}

bool PlanLane(BqlState const& st, Player* bot, LanePlan& plan)
{
    bool found = false;
    float bestScore = 0.0f;
    for (uint8 lane = 0; lane < 2; ++lane)
    {
        int16 count = 0;
        Position const* points = LanePoints(lane, count);
        int16 entry = -1;
        for (int16 i = 0; i < count; ++i)
            if (LanePointClear(st, points[i]) &&
                (entry < 0 || bot->GetExactDist2d(points[i]) < bot->GetExactDist2d(points[entry])))
                entry = i;

        if (entry < 0)
            continue;

        for (int8 dir : {int8(1), int8(-1)})
        {
            float run = 0.0f;
            for (int16 k = entry + dir; k >= 0 && k < count && LanePointClear(st, points[k]); k += dir)
                run += LANE_STEP;

            float const score = bot->GetExactDist2d(points[entry]) + (lane ? LANE_INNER_PENALTY : 0.0f) +
                                2.0f * std::max(0.0f, LANE_RUN_WANTED - run);
            if (!found || score < bestScore)
            {
                found = true;
                bestScore = score;
                plan.lane = lane;
                plan.dir = dir;
                plan.next = entry;
            }
        }
    }

    return found;
}

void UpdateLane(BqlState& st, Player* bot)
{
    ObjectGuid const guid = bot->GetGUID();
    if (!IsCarrier(bot))
    {
        st.lanes.erase(guid);
        if (st.curve.count(guid))
            st.curve.erase(guid);

        return;
    }

    auto itr = st.lanes.find(guid);
    bool replan = itr == st.lanes.end();
    if (!replan)
    {
        LanePlan& plan = itr->second;
        int16 count = 0;
        Position const* points = LanePoints(plan.lane, count);
        while (plan.next >= 0 && plan.next < count && bot->GetExactDist2d(points[plan.next]) <= LANE_REACHED)
            plan.next += plan.dir;

        replan = plan.next < 0 || plan.next >= count || !LanePointClear(st, points[plan.next], guid);
    }

    if (replan)
    {
        LanePlan plan;
        if (!PlanLane(st, bot, plan))
        {
            st.lanes.erase(guid);
            if (st.curve.count(guid))
                st.curve.erase(guid);

            return;
        }

        st.lanes[guid] = plan;
    }

    LanePlan const& plan = st.lanes[guid];
    st.curve[guid] = static_cast<uint8>(plan.lane * 2 + (plan.dir > 0 ? 1 : 2));
}

bool StandParkedOnTankSpot(BqlState const& st, ObjectGuid guid)
{
    auto const itr = st.stands.find(guid);
    return itr != st.stands.end() && itr->second.kind == StandKind::Tank && itr->second.parked;
}

void UpdateStand(BqlState& st, Player* bot, Creature* queen)
{
    Stand& stand = st.stands[bot->GetGUID()];
    StandKind kind = StandKind::None;
    Position spot;
    float respot = 0.5f;
    if (!OnSideDuty(bot) && !KillTarget(st, bot))
    {
        TankRole const role = RoleOf(st, bot);
        uint8 const ground = Held(st.slot, bot);
        uint8 const air = Held(st.airslot, bot);
        uint8 const group = Held(st.group, bot);
        Unit* victim = queen->GetVictim();
        if (role == TankRole::Main)
        {
            if (st.phase != IccBqlPhase::Ground || !victim || victim == bot ||
                queen->GetExactDist2d(ICC_BQL_TANK_SPOT) <= MAIN_LEASH)
            {
                kind = StandKind::Tank;
                spot = ICC_BQL_TANK_SPOT;
            }
        }
        else if (air && AirSpread(st))
        {
            kind = StandKind::Air;
            spot = NodeSpot(air);
        }
        else if (role == TankRole::Mirror)
        {
            Player* main = MainTank(st, bot);
            if (main && main != bot)
            {
                kind = StandKind::Mirror;
                spot = StandParkedOnTankSpot(st, main->GetGUID())
                           ? Position(ICC_BQL_TANK_SPOT.GetPositionX(),
                                      ICC_BQL_TANK_SPOT.GetPositionY() + BQL_MIRROR_OFFSET,
                                      ICC_BQL_TANK_SPOT.GetPositionZ())
                           : main->GetPosition();
                respot = 1.5f;
            }
        }
        else if (ground)
        {
            kind = StandKind::Node;
            spot = NodeSpot(ground);
        }
        else if (group && group <= GROUP_MAX && st.groupValid[group - 1])
        {
            kind = StandKind::Group;
            spot = st.groupSpots[group - 1];
        }
    }

    if (kind != StandKind::None && InShadow(st, spot, SHADOW_CLEAR))
        kind = StandKind::None;

    if (kind == StandKind::None)
    {
        stand.kind = StandKind::None;
        return;
    }

    if (stand.kind != kind || stand.spot.GetExactDist2d(spot) > respot)
    {
        stand.kind = kind;
        stand.spot = spot;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= BQL_STAND_UNPARK : distance <= BQL_STAND_PARK;
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    BqlState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    auto const itr = st->stands.find(botAI->GetBot()->GetGUID());
    return itr != st->stands.end() && itr->second.kind != StandKind::None ? &itr->second : nullptr;
}

Unit* ResolveTarget(PlayerbotAI* botAI, BqlState const& st, Creature* queen)
{
    Player* bot = botAI->GetBot();
    if (!IsTankPlayer(bot) && !IsRaidDpsBot(bot))
        return nullptr;

    if (IsFrenzied(bot))
    {
        NoteDerived(bot, "bql.target", "bite");
        return nullptr;
    }

    bool const braked = st.brake.count(bot->GetGUID()) != 0;
    if (Unit* skull = GetRaidHumanSkull(botAI, &IsSkullUnit))
        if (!braked || skull != queen)
        {
            NoteDerived(bot, "bql.target", "skull");
            return skull;
        }

    if (Player* vampire = KillTarget(st, bot))
    {
        NoteDerived(bot, "bql.target", "kill");
        return vampire;
    }

    if (braked)
    {
        NoteDerived(bot, "bql.target", "brake");
        return nullptr;
    }

    NoteDerived(bot, "bql.target", "boss");
    return queen;
}

void NotePact(BqlState const& st, Player* bot)
{
    if (!RaidObs::Active() || !IsDutyBot(bot))
        return;

    std::string value = "none";
    if (IsPactWalker(bot) && st.pactSpot)
        value = st.pactHuman ? "human" : "spot " + std::to_string(st.pactSpot);

    NoteDerived(bot, "bql.pact", value);
}

bool IsFeared(Player* member) { return member->HasAuraType(SPELL_AURA_MOD_FEAR); }

void UpdateMassDispel(BqlState& st, PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    ObjectGuid const guid = bot->GetGUID();
    Player* pick = nullptr;
    if (st.phase != IccBqlPhase::Ground && bot->getClass() == CLASS_PRIEST && IsDutyBot(bot) && !IsFeared(bot) &&
        !IsFrenzied(bot) && botAI->GetAiObjectContext()->GetValue<uint32>("spell id", "mass dispel")->Get())
    {
        std::vector<Position> taken;
        for (auto const& [priest, centre] : st.massDispel)
        {
            Player* other = priest != guid ? ObjectAccessor::GetPlayer(*bot, priest) : nullptr;
            Player* held = other ? ObjectAccessor::GetPlayer(*bot, centre) : nullptr;
            if (held && other->IsAlive() && !IsFeared(other))
                taken.push_back(held->GetPosition());
        }

        std::vector<Player*> feared;
        for (Player* member : MembersOnMap(bot))
        {
            Aura* fear = member->GetAura(SPELL_BQL_INCITE_TERROR);
            if (fear && fear->GetDuration() > MASS_DISPEL_CAST_MS &&
                std::none_of(taken.begin(), taken.end(), [member](Position const& centre)
                             { return member->GetExactDist2d(centre) <= MASS_DISPEL_RADIUS; }))
                feared.push_back(member);
        }

        uint32 bestCount = 0;
        float bestDistance = 0.0f;
        for (Player* centre : feared)
        {
            float const distance = bot->GetExactDist(centre);
            if (distance > MASS_DISPEL_RANGE || !bot->IsWithinLOSInMap(centre))
                continue;

            auto const inside = [centre](Player* other) { return other->GetExactDist2d(centre) <= MASS_DISPEL_RADIUS; };
            uint32 const count = std::min(
                MASS_DISPEL_MAX, static_cast<uint32>(std::count_if(feared.begin(), feared.end(), inside)));
            if (count > bestCount || (count == bestCount && distance < bestDistance))
            {
                pick = centre;
                bestCount = count;
                bestDistance = distance;
            }
        }

        if (bestCount < MASS_DISPEL_MIN)
            pick = nullptr;
    }

    if (pick)
        st.massDispel[guid] = pick->GetGUID();
    else if (st.massDispel.count(guid))
        st.massDispel.erase(guid);
}

void CallHumanTank(BqlState& st, Player* sender, Creature* queen, uint32 now)
{
    if (st.humanCallRan && getMSTimeDiff(st.humanCallMs, now) < HUMAN_CALL_CHECK_MS)
        return;

    st.humanCallRan = true;
    st.humanCallMs = now;
    bool const end = EndWindow(st, now);
    for (Player* member : MembersOnMap(sender))
    {
        if (IsDutyBot(member) || !IsTankPlayer(member))
            continue;

        bool const held = queen->GetVictim() == member;
        bool const linked = member->HasAura(SPELL_BQL_BLOOD_MIRROR_LINK);
        bool const low = member->GetHealthPct() < BQL_TANK_DEFENSIVE_PCT;
        if ((!held && !linked) || (!low && !end))
            continue;

        uint32 const spellId = ReadyTankDefensiveSpell(member, held, 0, 0);
        if (spellId)
            CallHumanTankDefensive(sender, member, spellId, 0, low ? "you are low" : "Berserk is a minute away",
                                   "bql.humancall");
    }
}
}

IccBqlPhase IccBqlPhaseOf(Player* bot, Unit* boss)
{
    IccBqlPhase const phase = PhaseOf(boss);
    if (RaidObs::Active())
    {
        static char const* const NAMES[] = {"ground", "center", "air"};
        RaidObs::NoteDerived(bot, "bql.phase", NAMES[static_cast<uint8>(phase)]);
    }

    return phase;
}

bool IsBqlLive(PlayerbotAI* botAI) { return GetBqlQueen(botAI) != nullptr; }

Creature* GetBqlQueen(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != ICC_MAP_ID ||
        bot->GetExactDist2d(ICC_BQL_ROOM_CENTER) > LIVE_RANGE)
        return nullptr;

    Creature* queen = QueenUnit(bot);
    return queen && queen->IsAlive() && queen->IsInCombat() ? queen : nullptr;
}

Unit* GetBqlTarget(PlayerbotAI* botAI)
{
    Creature* queen = nullptr;
    BqlState* st = LiveState(botAI, &queen);
    if (!st)
        return nullptr;

    TargetMemo& memo = st->targets[botAI->GetBot()->GetGUID()];
    uint32 const now = getMSTime();
    if (!memo.set || memo.ms != now)
    {
        memo.set = true;
        memo.ms = now;
        memo.pick = ResolveTarget(botAI, *st, queen);
    }

    return memo.pick;
}

Unit* GetBqlTauntTarget(PlayerbotAI* botAI)
{
    Creature* queen = nullptr;
    BqlState* st = LiveState(botAI, &queen);
    Player* bot = botAI->GetBot();
    if (!st || RoleOf(*st, bot) != TankRole::Main || PhaseOf(queen) != IccBqlPhase::Ground)
        return nullptr;

    Unit* victim = queen->GetVictim();
    if (!victim || victim == bot || !ClassTauntReady(botAI))
        return nullptr;

    if (queen->GetDiminishing(DIMINISHING_TAUNT) >= DIMINISHING_LEVEL_3)
        return nullptr;

    return bot->GetExactDist(queen) <= TAUNT_RANGE && bot->IsWithinLOSInMap(queen) ? queen : nullptr;
}

bool IsBqlBraked(PlayerbotAI* botAI)
{
    BqlState* st = LiveState(botAI);
    return st && st->brake.count(botAI->GetBot()->GetGUID());
}

bool IsBqlBiting(PlayerbotAI* botAI) { return IsBqlLive(botAI) && IsFrenzied(botAI->GetBot()); }

Player* GetBqlBiteTarget(PlayerbotAI* botAI)
{
    Creature* queen = nullptr;
    BqlState* st = LiveState(botAI, &queen);
    Player* bot = botAI->GetBot();
    if (!st || !IsFrenzied(bot))
        return nullptr;

    auto const itr = st->bite.find(bot->GetGUID());
    Player* target = itr != st->bite.end() ? ObjectAccessor::GetPlayer(*bot, itr->second) : nullptr;
    return target && IccSameInstance(bot, target) && BiteCandidate(*st, target, queen->GetVictim()) ? target : nullptr;
}

Unit* GetBqlFrenzyCc(PlayerbotAI* botAI, uint32& spellId)
{
    spellId = 0;
    BqlState* st = LiveState(botAI);
    if (!st)
        return nullptr;

    Player* bot = botAI->GetBot();
    auto const itr = st->cc.find(bot->GetGUID());
    if (itr == st->cc.end())
        return nullptr;

    Player* victim = ObjectAccessor::GetPlayer(*bot, itr->second);
    if (!victim || !victim->IsAlive() || !IccSameInstance(bot, victim) || !IsMindControlled(victim) ||
        IsControlled(victim) || !bot->IsWithinLOSInMap(victim))
        return nullptr;

    spellId = CcSpellFor(botAI, victim);
    return spellId && botAI->CanCastSpell(spellId, victim) ? victim : nullptr;
}

bool IsBqlVampireInSplash(PlayerbotAI* botAI, float range)
{
    BqlState* st = LiveState(botAI);
    if (!st)
        return false;

    Player* bot = botAI->GetBot();
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    for (Player* member : MembersOnMap(bot))
        if (member != bot && IsMindControlled(member) && !st->kill.count(member->GetGUID()) &&
            (bot->GetExactDist2d(member) <= range || (target && target->GetExactDist2d(member) <= range)))
            return true;

    return false;
}

bool IsBqlPactMember(PlayerbotAI* botAI) { return IsBqlLive(botAI) && IsPactWalker(botAI->GetBot()); }

bool GetBqlPactWalk(PlayerbotAI* botAI, Position& spot)
{
    BqlState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !st->pactSpot || !IsPactWalker(bot))
        return false;

    if (Player* human = st->pactHuman ? ObjectAccessor::GetPlayer(*bot, st->pactHuman) : nullptr)
    {
        if (!human->IsAlive() || !IccSameInstance(bot, human) || bot->GetExactDist2d(human) <= PACT_HUMAN_PARK)
            return false;

        spot = human->GetPosition();
        return true;
    }

    Position const& pact = ICC_BQL_PACT_SPOTS[st->pactSpot - 1];
    if (bot->GetExactDist2d(pact) <= PACT_PARK)
        return false;

    spot = pact;
    return true;
}

bool IsBqlShadowCarrier(PlayerbotAI* botAI) { return IsBqlLive(botAI) && IsShadowWalker(botAI->GetBot()); }

bool GetBqlShadowsWalk(PlayerbotAI* botAI, Position& spot)
{
    BqlState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || !IsShadowWalker(bot))
        return false;

    auto const itr = st->lanes.find(bot->GetGUID());
    if (itr == st->lanes.end())
        return false;

    int16 count = 0;
    Position const* points = LanePoints(itr->second.lane, count);
    if (itr->second.next < 0 || itr->second.next >= count)
        return false;

    spot = points[itr->second.next];
    return true;
}

bool IsBqlInShadow(PlayerbotAI* botAI)
{
    BqlState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || OnSideDuty(bot))
        return false;

    bool const inside = InShadow(*st, bot->GetPosition(), SHADOW_TRIGGER);
    if (!inside)
        NoteDerived(bot, "bql.dodge", "none");

    return inside;
}

bool FindBqlDodgeSpot(PlayerbotAI* botAI, Position& spot)
{
    BqlState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    std::vector<HazardCircle> hazards;
    for (Position const& shadow : st->shadows)
        hazards.emplace_back(shadow, SHADOW_CLEAR);

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    bool const melee = target && !PlayerbotAI::IsRanged(bot);
    Position found;
    if (melee)
        found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, 2.0f, static_cast<float>(M_PI) / 8.0f,
                                                  nullptr,
                                                  [target](float x, float y)
                                                  {
                                                      return target->GetExactDist2d(x, y) <=
                                                             target->GetCombatReach() + MELEE_KEEP;
                                                  });

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
        found = FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP, nullptr);

    if (found.GetPositionX() == 0.0f && found.GetPositionY() == 0.0f)
    {
        NoteDerived(bot, "bql.dodge", "blocked");
        return false;
    }

    NoteDerived(bot, "bql.dodge", melee ? "melee" : "clear");
    spot = found;
    return true;
}

Position GetBqlWalkVia(PlayerbotAI* botAI, Position const& goal, Position const* held)
{
    BqlState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return goal;

    std::vector<HazardCircle> crossings;
    std::vector<HazardCircle> spots;
    Position const from = bot->GetPosition();
    for (Position const& shadow : st->shadows)
    {
        spots.emplace_back(shadow, SHADOW_CLEAR);
        if (shadow.GetExactDist2d(from) >= SHADOW_TRIGGER && shadow.GetExactDist2d(goal) >= SHADOW_TRIGGER)
            crossings.emplace_back(shadow, SHADOW_TRIGGER);
    }

    auto const clear = [&crossings](Position const& a, Position const& b)
    {
        return std::none_of(crossings.begin(), crossings.end(),
                            [&](HazardCircle const& shadow) { return SegmentCrossesCircle(a, b, shadow); });
    };

    if (held && bot->GetExactDist2d(*held) > DETOUR_REACHED && clear(from, *held) && clear(*held, goal))
        return *held;

    if (clear(from, goal))
    {
        NoteDerived(bot, "bql.detour", "none");
        return goal;
    }

    float const straight = from.GetExactDist2d(goal);
    Position const via = FindNearestPositionClearOfHazards(
        bot, spots, DETOUR_SWEEP, 2.0f, static_cast<float>(M_PI) / 8.0f, &goal,
        [&](float x, float y)
        {
            Position const point(x, y, from.GetPositionZ());
            return from.GetExactDist2d(point) + point.GetExactDist2d(goal) <= straight + DETOUR_MAX_EXTRA &&
                   clear(from, point) && clear(point, goal);
        });

    if (via.GetPositionX() == 0.0f && via.GetPositionY() == 0.0f)
    {
        NoteDerived(bot, "bql.detour", "blocked");
        return goal;
    }

    NoteDerived(bot, "bql.detour", "via");
    return via;
}

bool IsBqlHoldingStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand &&
           (stand->kind == StandKind::Node || stand->kind == StandKind::Air || stand->kind == StandKind::Group);
}

bool IsBqlHoldingTankStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && (stand->kind == StandKind::Tank || stand->kind == StandKind::Mirror);
}

bool GetBqlStandWalk(PlayerbotAI* botAI, Position& spot)
{
    Stand const* stand = HeldStand(botAI);
    if (!stand || stand->parked)
        return false;

    spot = stand->spot;
    return true;
}

bool IsBqlMovementOwned(PlayerbotAI* botAI)
{
    BqlState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st)
        return false;

    return HeldStand(botAI) || OnSideDuty(bot) || InShadow(*st, bot->GetPosition(), SHADOW_TRIGGER);
}

bool IsBqlFearWindow(PlayerbotAI* botAI)
{
    Creature* queen = nullptr;
    BqlState* st = LiveState(botAI, &queen);
    if (!st)
        return false;

    switch (PhaseOf(queen))
    {
        case IccBqlPhase::Center:
            return true;
        case IccBqlPhase::Air:
            return false;
        default:
            return st->nextCenterMs &&
                   static_cast<int32>(st->nextCenterMs - getMSTime()) <= static_cast<int32>(FEAR_LEAD_MS);
    }
}

bool IsBqlSelfWardDue(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PRIEST || bot->HasAura(SPELL_FEAR_WARD) || !IsBqlFearWindow(botAI))
        return false;

    AiObjectContext* context = botAI->GetAiObjectContext();
    uint32 const ward = context->GetValue<uint32>("spell id", "fear ward")->Get();
    return ward && context->GetValue<uint32>("spell id", "mass dispel")->Get() && !bot->HasSpellCooldown(ward) &&
           botAI->CanCastSpell(ward, bot);
}

Player* GetBqlMassDispelTarget(PlayerbotAI* botAI)
{
    BqlState* st = LiveState(botAI);
    Player* bot = botAI->GetBot();
    if (!st || IsFeared(bot))
        return nullptr;

    auto const itr = st->massDispel.find(bot->GetGUID());
    Player* centre = itr != st->massDispel.end() ? ObjectAccessor::GetPlayer(*bot, itr->second) : nullptr;
    return centre && centre->IsAlive() && IccSameInstance(bot, centre) && centre->HasAura(SPELL_BQL_INCITE_TERROR) &&
                   bot->GetExactDist(centre) <= MASS_DISPEL_RANGE
               ? centre
               : nullptr;
}

bool GetBqlWhirlHit(PlayerbotAI* botAI, RaidPredictedHit& hit)
{
    Creature* queen = GetBqlQueen(botAI);
    Aura* whirl = queen ? queen->GetAura(SPELL_BQL_BLOODBOLT_WHIRL) : nullptr;
    AuraEffect* effect = whirl ? whirl->GetEffect(EFFECT_0) : nullptr;
    if (!effect)
        return false;

    int32 const ticksLeft = effect->GetTotalTicks() - static_cast<int32>(effect->GetTickNumber());
    SpellInfo const* bolt = sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellIdForDifficulty(SPELL_BQL_WHIRL_BOLT, queen));
    SpellInfo const* shroud =
        sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellIdForDifficulty(SPELL_BQL_SHROUD_OF_SORROW, queen));
    if (ticksLeft <= 0 || !bolt || !shroud)
        return false;

    auto const base = [](SpellInfo const* info)
    {
        SpellEffectInfo const& damage = info->Effects[EFFECT_0];
        return static_cast<float>(std::max(damage.BasePoints + (damage.DieSides + 1) / 2, 0));
    };

    float const left = static_cast<float>(ticksLeft);
    float const presence = 1.0f + PRESENCE_PER_STACK * static_cast<float>(queen->GetAuraCount(SPELL_BQL_PRESENCE));
    hit = RaidPredictedHit();
    hit.startMs = getMSTime() + static_cast<uint32>(std::max(effect->GetPeriodicTimer(), 0)) + WHIRL_FLIGHT_MS;
    hit.durationMs = static_cast<uint32>((ticksLeft - 1) * std::max(effect->GetAmplitude(), 0));
    hit.damage = static_cast<uint32>((base(bolt) * left / WHIRL_TICKS + base(shroud) * left) * presence);
    hit.school = SPELL_SCHOOL_MASK_SHADOW;
    return true;
}

RaidCover GetBqlWhirlCover(PlayerbotAI* botAI)
{
    if (!IsBqlLive(botAI))
        return RaidCover::None;

    RaidPredictedHit hit;
    return GetRaidCover(botAI, ICC_DEFENSIVE_SCOPE, GetBqlWhirlHit(botAI, hit) ? &hit : nullptr);
}

bool IsBqlWhirlCoverHeld(PlayerbotAI* botAI)
{
    RaidPredictedHit hit;
    return GetBqlWhirlHit(botAI, hit) && RaidHitLeadIn(hit);
}

uint32 GetBqlTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    Creature* queen = nullptr;
    BqlState* st = LiveState(botAI, &queen);
    Player* bot = botAI->GetBot();
    bool const tank = IsTankPlayer(bot);
    if (!st || !bot->IsAlive() || !IsDutyBot(bot) || (!tank && RoleOf(*st, bot) != TankRole::Mirror))
        return 0;

    bool const held = queen->GetVictim() == bot;
    bool const linked = bot->HasAura(SPELL_BQL_BLOOD_MIRROR_LINK);
    bool const low = bot->GetHealthPct() < BQL_TANK_DEFENSIVE_PCT;
    bool const end = EndWindow(*st, getMSTime()) && (held || linked);
    if (!low && !end)
    {
        if (noteKind)
            NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    if (!tank)
        return NextPhysicalDefensive(botAI, bot, noteKind, 0);

    return NextTankDefensive(botAI, bot, noteKind, held);
}

bool IsBqlTankDefensiveHeld(PlayerbotAI* botAI)
{
    BqlState* st = LiveState(botAI);
    return st && botAI->GetBot()->GetHealthPct() >= BQL_TANK_LAST_RESORT_PCT && !EndWindow(*st, getMSTime());
}

void TickBqlEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    Creature* unit = QueenUnit(bot);
    if (!unit || !unit->IsAlive() || !unit->IsInCombat())
    {
        if (bqlStates.Find(bot->GetInstanceId()))
            bqlStates.Reset(bot->GetInstanceId());
        return;
    }

    Creature* queen = GetBqlQueen(botAI);
    if (!queen)
        return;

    BqlState& st = bqlStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    Scan(st, bot, now);
    RefreshDuties(st, bot, queen, now);
    CallHumanTank(st, bot, queen, now);
    IccBqlPhaseOf(bot, queen);
    UpdateBrake(st, bot, queen);
    UpdateMassDispel(st, botAI);
    UpdateLane(st, bot);
    UpdateStand(st, bot, queen);
    NotePact(st, bot);
}

RaidBurstWindow IccBloodQueenBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    BqlState* st = LiveState(botAI);
    uint32 const peak = Is25(botAI->GetBot()) ? VAMPIRE_PEAK_25 : VAMPIRE_PEAK_10;
    window.allowLust =
        st && st->pullMs && (st->vampires >= peak || getMSTimeDiff(st->pullMs, getMSTime()) >= LUST_LATEST_MS);
    return window;
}

std::vector<uint32> const& GetBqlOpenerBosses(PlayerbotAI* /*botAI*/)
{
    static std::vector<uint32> const bosses = {NPC_BQL_BLOOD_QUEEN};
    return bosses;
}

uint8 IccBloodQueenResistSchools(PlayerbotAI* /*botAI*/, uint8 /*previous*/) { return RAID_RESIST_SHADOW; }
