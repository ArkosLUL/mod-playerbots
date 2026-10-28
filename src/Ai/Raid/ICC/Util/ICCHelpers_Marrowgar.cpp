/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Marrowgar.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "CellImpl.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "EncounterHelpers.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroupReference.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCSeams.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MoveSpline.h"
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
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
constexpr float BOSS_SEARCH_RADIUS = 200.0f;
constexpr uint32 BOSS_RESCAN_MS = 1000;
// Swept from him, so one refresh covers the room whichever bot asks
constexpr float HAZARD_SEARCH_RADIUS = 110.0f;
constexpr uint32 HAZARD_SCAN_MS = 100;
constexpr uint32 ROSTER_REFRESH_MS = 1000;
// He has settled once he drifts less than this for BOSS_SETTLE_MS
constexpr float BOSS_SETTLE_DRIFT = 0.5f;
constexpr uint32 BOSS_SETTLE_MS = 500;
// While he moves, stands round him hold on where they latched until he is this far from it
constexpr float BOSS_HOLD_DRIFT = 3.0f;
// A head waits 450 ms before its first step, stops for good at a wall, and is done after 12 s
constexpr float HEAD_MOVED = 0.5f;
constexpr uint32 HEAD_STALL_MS = 700;
constexpr uint32 HEAD_LIFETIME_MS = 12500;
// The patches a head is still to lay that count, about 1.35 s of its walk
constexpr uint32 HEAD_LOOKAHEAD = 3;
// While he charges, the nearest two patches of each line of the cross he drops where he stops. The heads
// cover the rest once they spawn.
constexpr float CROSS_FIRST = 2.5f;
constexpr uint32 CROSS_LOOKAHEAD = 2;
constexpr uint32 CROSS_NOTE_TTL_MS = 3000;
// Re-centre: settled past START from his spot, the main tank walks PULL yd beyond the spot so he stops
// near it, done within DONE. Pulls that keep failing are given up until the next storm.
constexpr float RECENTRE_START = 8.0f;
constexpr float RECENTRE_DONE = 5.0f;
constexpr float RECENTRE_PULL = 18.0f;
constexpr uint32 RECENTRE_MAX_PULLS = 3;
constexpr float STAND_RELATCH = 2.0f;
constexpr float STAND_FLOOR_TOLERANCE = 3.0f;
constexpr uint32 BAITS_10 = 3;
constexpr uint32 BAITS_25 = 8;
constexpr float DEG = static_cast<float>(M_PI) / 180.0f;

constexpr std::array<uint32, 3> SPIKE_ENTRIES = {NPC_SPIKE1, NPC_SPIKE2, NPC_SPIKE3};

enum class StandKind : uint8
{
    Tank,
    Pull,
    OffTank,
    Inner,
    Bait,
    Anchor,
    Stack
};

char const* StandName(StandKind kind)
{
    switch (kind)
    {
        case StandKind::Tank:
            return "tank";
        case StandKind::Pull:
            return "pull";
        case StandKind::OffTank:
            return "offtank";
        case StandKind::Inner:
            return "inner";
        case StandKind::Bait:
            return "bait";
        case StandKind::Anchor:
            return "anchor";
        default:
            return "stack";
    }
}

bool IsStormStand(StandKind kind) { return kind == StandKind::Anchor || kind == StandKind::Stack; }

struct Stand
{
    StandKind kind = StandKind::Tank;
    Position raw;
    Position spot;
    // Where he stood when a stand round him latched
    Position boss;
    bool held = false;
    bool parked = false;
};

struct Head
{
    ObjectGuid guid;
    Position last;
    uint32 firstSeenMs = 0;
    uint32 lastMoveMs = 0;
};

struct LmState
{
    ObjectGuid boss;
    uint32 bossScanMs = 0;
    bool bossScanned = false;  // 0 is a real getMSTime value
    Position bossLast;
    uint32 bossMovedMs = 0;
    bool bossTracked = false;

    uint32 rosterMs = 0;
    bool rosterBuilt = false;
    RaidObs::ObsGuidMap<uint8> duty{"lm.duty"};
    // 1 for the anchor at 180 degrees, every tank's, 2 and 3 for 60 and 300
    RaidObs::ObsGuidMap<uint8> anchor{"lm.anchor"};
    std::vector<ObjectGuid> offTanks;
    std::vector<ObjectGuid> inner;
    std::vector<ObjectGuid> baits;
    std::vector<ObjectGuid> stack;

    bool pulling = false;
    Position pull;
    uint32 pulls = 0;

    // Bone Slice is off in a storm and resumes LM_SLICE_DELAY_MS after it. sliceWindow counts the windows.
    bool sliceArmed = false;
    uint32 sliceOpenMs = 0;
    uint32 sliceWindow = 0;
    std::unordered_map<ObjectGuid, uint32> defensiveWindow;
    uint32 humanCallWindow = 0;

    bool stormArmed = false;
    uint32 stormStartMs = 0;

    uint32 hazardMs = 0;
    bool hazardScanned = false;
    std::vector<Head> heads;
    std::vector<Position> circles;
    Position notedCross;
    bool crossNoted = false;

    uint32 spikeMs = 0;
    bool spikesRead = false;
    std::vector<ObjectGuid> spikes;
    std::vector<bool> spikeFar;
    std::vector<ObjectGuid> rangedDps;

    std::unordered_map<ObjectGuid, Stand> stands;
};

RaidInstanceState<LmState> lmStates;
// Bots whose saved rti was checked this instance, so a player's later order sticks
RaidInstanceState<std::unordered_set<ObjectGuid>> lmRtiChecked;

LmState* LiveState(PlayerbotAI* botAI)
{
    if (!IsLmLive(botAI))
        return nullptr;

    Player* bot = botAI->GetBot();
    return bot->GetInstanceId() ? &lmStates.For(bot->GetInstanceId()) : nullptr;
}

template <class Fn>
void ForEachMember(Player* bot, Fn&& fn)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        fn(bot);
        return;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInWorld() && member->GetMapId() == bot->GetMapId() &&
            member->GetInstanceId() == bot->GetInstanceId())
            fn(member);
    }
}

bool IsLmTank(Player* player) { return PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true); }

bool IsSpikeEntry(uint32 entry)
{
    return std::find(SPIKE_ENTRIES.begin(), SPIKE_ENTRIES.end(), entry) != SPIKE_ENTRIES.end();
}

bool IsLmUnit(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && (creature->GetEntry() == NPC_LORD_MARROWGAR || IsSpikeEntry(creature->GetEntry()));
}

bool IsMeleeDpsBot(Player* player)
{
    return GET_PLAYERBOT_AI(player) && IsRaidDpsBot(player) && !PlayerbotAI::IsRanged(player);
}

Player* GetMainTank(Player* bot)
{
    Player* mainTank = nullptr;
    ForEachMember(bot,
                  [&mainTank](Player* member)
                  {
                      if (!mainTank && member->IsAlive() && PlayerbotAI::IsMainTank(member))
                          mainTank = member;
                  });
    return mainTank;
}

// Who baits Coldflame first: hunters can't shoot from inside him, then the casters least hurt by moving
uint8 BaitRank(Player* player)
{
    switch (player->getClass())
    {
        case CLASS_HUNTER:
            return 0;
        case CLASS_WARLOCK:
            return 1;
        case CLASS_PRIEST:
            return 2;
        case CLASS_DRUID:
            return 3;
        case CLASS_SHAMAN:
            return 4;
        case CLASS_MAGE:
            return 5;
        default:
            return 6;
    }
}

Position Offset(Position const& origin, float angle, float distance)
{
    return Position(origin.GetPositionX() + distance * std::cos(angle),
                    origin.GetPositionY() + distance * std::sin(angle), origin.GetPositionZ());
}

// From the bearing away from the tank spot outwards, alternating sides in rank order
Position BaitSlot(Position const& from, size_t index)
{
    int32 const rank = static_cast<int32>(index);
    int32 const steps = (rank % 2 ? 1 : -1) * ((rank + 1) / 2);
    float const bearing = ICC_LM_TANK_SPOT.GetAngle(&ICC_LM_BOSS_SPOT) + steps * LM_BAIT_SPACING_DEG * DEG;
    return Offset(from, bearing, LM_BAIT_RADIUS);
}

int32 IndexOf(std::vector<ObjectGuid> const& guids, ObjectGuid guid)
{
    auto const itr = std::find(guids.begin(), guids.end(), guid);
    return itr == guids.end() ? -1 : static_cast<int32>(itr - guids.begin());
}

void TrackBoss(LmState& state, Creature* boss, uint32 now)
{
    if (!state.bossTracked || boss->isMoving() || state.bossLast.GetExactDist2d(boss) > BOSS_SETTLE_DRIFT)
    {
        state.bossLast = boss->GetPosition();
        state.bossMovedMs = now;
        state.bossTracked = true;
    }
}

bool BossSettled(LmState const& state, uint32 now)
{
    return state.bossTracked && getMSTimeDiff(state.bossMovedMs, now) >= BOSS_SETTLE_MS;
}

void RefreshRoster(LmState& state, Player* bot, uint32 now)
{
    if (state.rosterBuilt && getMSTimeDiff(state.rosterMs, now) < ROSTER_REFRESH_MS)
        return;

    state.rosterBuilt = true;
    state.rosterMs = now;

    Player* mainTank = nullptr;
    std::vector<Player*> bots;
    ForEachMember(bot,
                  [&](Player* member)
                  {
                      if (!member->IsAlive())
                          return;
                      if (!mainTank && PlayerbotAI::IsMainTank(member))
                          mainTank = member;
                      if (GET_PLAYERBOT_AI(member))
                          bots.push_back(member);
                  });
    std::sort(bots.begin(), bots.end(), IccGuidLess());

    std::vector<Player*> tanks;
    std::vector<Player*> healers;
    std::vector<Player*> ranged;
    for (Player* member : bots)
    {
        if (member == mainTank)
            continue;
        if (IsLmTank(member))
            tanks.push_back(member);
        else if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsRanged(member))
            ranged.push_back(member);
    }
    std::stable_sort(ranged.begin(), ranged.end(),
                     [](Player* a, Player* b) { return BaitRank(a) < BaitRank(b); });

    uint32 const wanted = IsRaid25Man(bot) ? BAITS_25 : BAITS_10;
    std::vector<Player*> baits;
    for (Player* member : ranged)
        if ((member->getClass() == CLASS_HUNTER || baits.size() < wanted) && baits.size() < LM_BAIT_SLOTS)
            baits.push_back(member);
    // One bait outside is all the core needs to keep Coldflame off everyone inside
    if (baits.empty() && !healers.empty())
        baits.push_back(healers.front());

    auto const isBait = [&baits](Player* member)
    { return std::find(baits.begin(), baits.end(), member) != baits.end(); };

    // Every tank takes the anchor by the tank spot, and an anchor no tank holds the bait whose ring slot
    // is nearest it
    std::unordered_map<ObjectGuid, uint8> slots;
    if (mainTank && GET_PLAYERBOT_AI(mainTank))
        slots[mainTank->GetGUID()] = 1;
    for (Player* tank : tanks)
        slots[tank->GetGUID()] = 1;

    bool const tanksAnchor = !slots.empty();
    auto const anchorNearestBait = [&baits, &slots](Position const& anchor, uint8 slot)
    {
        Player* nearest = nullptr;
        float nearestDistance = 0.0f;
        for (size_t i = 0; i < baits.size(); ++i)
        {
            if (slots.count(baits[i]->GetGUID()))
                continue;

            float const distance = BaitSlot(ICC_LM_BOSS_SPOT, i).GetExactDist2d(anchor);
            if (!nearest || distance < nearestDistance)
            {
                nearest = baits[i];
                nearestDistance = distance;
            }
        }

        if (nearest)
            slots[nearest->GetGUID()] = slot;
    };
    anchorNearestBait(ICC_LM_STORM_ANCHOR_60, 2);
    anchorNearestBait(ICC_LM_STORM_ANCHOR_300, 3);
    if (!tanksAnchor)
        anchorNearestBait(ICC_LM_STORM_ANCHOR_180, 1);

    state.offTanks.clear();
    state.inner.clear();
    state.baits.clear();
    state.stack.clear();

    std::unordered_map<ObjectGuid, uint8> duties;
    for (Player* member : bots)
    {
        ObjectGuid const guid = member->GetGUID();
        IccLmDuty duty = IccLmDuty::Inner;
        if (member == mainTank)
            duty = IccLmDuty::MainTank;
        else if (std::find(tanks.begin(), tanks.end(), member) != tanks.end())
            duty = IccLmDuty::OffTank;
        else if (isBait(member))
            duty = IccLmDuty::Bait;
        duties[guid] = static_cast<uint8>(duty);

        if (duty == IccLmDuty::OffTank)
            state.offTanks.push_back(guid);
        else if (duty == IccLmDuty::Inner)
            state.inner.push_back(guid);

        if (!slots.count(guid))
            state.stack.push_back(guid);
    }
    // Rank order, not guid order: bait slots fan out from the middle, so hunters take the centre ones
    for (Player* member : baits)
        state.baits.push_back(member->GetGUID());

    std::vector<ObjectGuid> departed;
    for (auto const& [guid, value] : state.duty.Raw())
        if (!duties.count(guid))
            departed.push_back(guid);
    for (ObjectGuid guid : departed)
        state.duty.erase(guid);
    for (auto const& [guid, value] : duties)
        state.duty.Set(guid, value);

    departed.clear();
    for (auto const& [guid, value] : state.anchor.Raw())
        if (!slots.count(guid))
            departed.push_back(guid);
    for (ObjectGuid guid : departed)
        state.anchor.erase(guid);
    for (auto const& [guid, value] : slots)
        state.anchor.Set(guid, value);
}

uint8 DutyOf(LmState const& state, ObjectGuid guid)
{
    auto const itr = state.duty.find(guid);
    return itr == state.duty.end() ? 0 : itr->second;
}

uint8 AnchorOf(LmState const& state, ObjectGuid guid)
{
    auto const itr = state.anchor.find(guid);
    return itr == state.anchor.end() ? 0 : itr->second;
}

Position const& AnchorSpot(uint8 slot)
{
    switch (slot)
    {
        case 2:
            return ICC_LM_STORM_ANCHOR_60;
        case 3:
            return ICC_LM_STORM_ANCHOR_300;
        default:
            return ICC_LM_STORM_ANCHOR_180;
    }
}

// The main tank holds him at the tank spot. Settled off his spot after a storm, it walks a pull point past
// the spot first, so he follows back onto it.
void MainTankSpot(LmState& state, Player* bot, Creature* boss, uint32 now, StandKind& kind, Position& raw)
{
    if (BossSettled(state, now) && boss->GetVictim() == bot)
    {
        float const off = state.bossLast.GetExactDist2d(ICC_LM_BOSS_SPOT);
        auto const parked = state.stands.find(bot->GetGUID());
        bool const atPull = state.pulling && parked != state.stands.end() && parked->second.kind == StandKind::Pull &&
                            parked->second.parked;
        if (state.pulling && (off <= RECENTRE_DONE || atPull))
            state.pulling = false;
        if (off <= RECENTRE_DONE)
            state.pulls = 0;

        if (!state.pulling && off > RECENTRE_START && state.pulls < RECENTRE_MAX_PULLS)
        {
            float const away = state.bossLast.GetAngle(&ICC_LM_BOSS_SPOT);
            state.pull = EncounterHelpers::ValidateFloorPoint(bot, Offset(ICC_LM_BOSS_SPOT, away, RECENTRE_PULL));
            state.pulling = true;
            ++state.pulls;
        }
    }

    kind = state.pulling ? StandKind::Pull : StandKind::Tank;
    raw = state.pulling ? state.pull : ICC_LM_TANK_SPOT;
}

// The boss position a stand round him derives from: where he settled, or while he moves, where a held stand
// of this kind latched, as long as he is still near it. Otherwise reach follows him until he settles.
bool StandBoss(LmState const& state, ObjectGuid guid, StandKind kind, Creature* boss, uint32 now, Position& from)
{
    if (BossSettled(state, now))
    {
        from = state.bossLast;
        return true;
    }

    auto const held = state.stands.find(guid);
    if (held == state.stands.end() || !held->second.held || held->second.kind != kind ||
        boss->GetExactDist2d(held->second.boss) > BOSS_HOLD_DRIFT)
        return false;

    from = held->second.boss;
    return true;
}

bool DesiredStand(LmState& state, PlayerbotAI* botAI, Creature* boss, IccLmPhase phase, uint32 now, StandKind& kind,
                  Position& raw, Position& from)
{
    Player* bot = botAI->GetBot();
    if (bot->HasAura(SPELL_LM_IMPALED))
        return false;

    ObjectGuid const guid = bot->GetGUID();
    bool const storm = phase != IccLmPhase::Normal;
    if (uint8 const slot = AnchorOf(state, guid); storm && slot)
    {
        kind = StandKind::Anchor;
        raw = AnchorSpot(slot);
        return true;
    }

    // A spike pick hands the bot to reach, or the stand would walk it back off the spike
    if (Unit* pick = GetLmTarget(botAI); pick && pick != boss)
        return false;

    if (storm)
    {
        int32 const index = IndexOf(state.stack, guid);
        if (index < 0)
            return false;

        float const step = 2.0f * static_cast<float>(M_PI) / std::max<size_t>(state.stack.size(), 1);
        kind = StandKind::Stack;
        raw = Offset(ICC_LM_STORM_STACK, step * index, LM_STACK_SLOT_RADIUS);
        return true;
    }

    switch (static_cast<IccLmDuty>(DutyOf(state, guid)))
    {
        case IccLmDuty::MainTank:
            // Taken off it, the class taunt brings him back from wherever reach takes the tank
            if (boss->GetVictim() && boss->GetVictim() != bot)
                return false;

            MainTankSpot(state, bot, boss, now, kind, raw);
            return true;
        case IccLmDuty::OffTank:
        {
            Player* mainTank = GetMainTank(bot);
            int32 const index = IndexOf(state.offTanks, guid);
            if (!mainTank || mainTank == bot || mainTank->isMoving() || index < 0)
                return false;

            // Beside it across his bearing: both in his reach, both inside Bone Slice's 5 yd
            float const across = mainTank->GetAngle(boss) + static_cast<float>(M_PI) / 2.0f;
            float const side = index % 2 ? -1.0f : 1.0f;
            float const distance = index < 2 ? LM_TANK_STACK_OFFSET : 1.75f * LM_TANK_STACK_OFFSET;
            kind = StandKind::OffTank;
            raw = Offset(mainTank->GetPosition(), across, side * distance);
            return true;
        }
        case IccLmDuty::Inner:
        {
            int32 const index = IndexOf(state.inner, guid);
            if (index < 0 || !StandBoss(state, guid, StandKind::Inner, boss, now, from))
                return false;

            // He faces the main tank on the tank spot, so his back is the bearing from it to him
            size_t const count = state.inner.size();
            float const share = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) - 0.5f : 0.0f;
            kind = StandKind::Inner;
            raw = Offset(from, ICC_LM_TANK_SPOT.GetAngle(&from) + share * LM_INNER_ARC_DEG * DEG, LM_INNER_RADIUS);
            return true;
        }
        case IccLmDuty::Bait:
        {
            int32 const index = IndexOf(state.baits, guid);
            if (index < 0 || !StandBoss(state, guid, StandKind::Bait, boss, now, from))
                return false;

            kind = StandKind::Bait;
            raw = BaitSlot(from, static_cast<size_t>(index));
            return true;
        }
        default:
            return false;
    }
}

void ReleaseStand(Player* bot, Stand& stand)
{
    stand.held = false;
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "lm.stand", "none");
}

void UpdateStand(LmState& state, PlayerbotAI* botAI, Creature* boss, IccLmPhase phase, uint32 now)
{
    Player* bot = botAI->GetBot();
    Stand& stand = state.stands[bot->GetGUID()];

    StandKind kind = StandKind::Tank;
    Position raw;
    Position from;
    if (!DesiredStand(state, botAI, boss, phase, now, kind, raw, from))
    {
        ReleaseStand(bot, stand);
        return;
    }

    if (!stand.held || stand.kind != kind || stand.raw.GetExactDist2d(raw) > STAND_RELATCH)
    {
        Position const floor = EncounterHelpers::ValidateFloorPoint(bot, raw);
        stand.raw = raw;
        stand.boss = from;
        stand.spot = floor.GetExactDist(raw) <= STAND_FLOOR_TOLERANCE ? floor : raw;
        stand.kind = kind;
        stand.held = true;
        stand.parked = false;
    }

    float const distance = bot->GetExactDist2d(stand.spot);
    stand.parked = stand.parked ? distance <= LM_STAND_UNPARK : distance <= LM_STAND_PARK;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "lm.stand", StandName(kind));
}

Stand const* HeldStand(PlayerbotAI* botAI)
{
    if (!IsLmLive(botAI))
        return nullptr;

    Player* bot = botAI->GetBot();
    LmState* state = bot->GetInstanceId() ? lmStates.Find(bot->GetInstanceId()) : nullptr;
    if (!state)
        return nullptr;

    auto const itr = state->stands.find(bot->GetGUID());
    return itr != state->stands.end() && itr->second.held ? &itr->second : nullptr;
}

void ScanColdflame(LmState& state, Creature* boss, uint32 now)
{
    if (state.hazardScanned && getMSTimeDiff(state.hazardMs, now) < HAZARD_SCAN_MS)
        return;

    state.hazardScanned = true;
    state.hazardMs = now;
    state.circles.clear();

    uint32 const patchId = sSpellMgr->GetSpellIdForDifficulty(SPELL_LM_COLDFLAME_PATCH, boss);
    std::list<WorldObject*> objects;
    Acore::AllWorldObjectsInRange check(boss, HAZARD_SEARCH_RADIUS);
    Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(boss, objects, check,
                                                                           GRID_MAP_TYPE_MASK_DYNAMICOBJECT);
    Cell::VisitObjects(boss, searcher, HAZARD_SEARCH_RADIUS);
    for (WorldObject* object : objects)
    {
        DynamicObject* patch = object->ToDynObject();
        if (patch && patch->GetSpellId() == patchId)
            state.circles.push_back(patch->GetPosition());
    }

    std::list<Creature*> found;
    boss->GetCreatureListWithEntryInGrid(found, NPC_COLDFLAME, HAZARD_SEARCH_RADIUS);
    std::vector<Head> heads;
    for (Creature* coldflame : found)
    {
        auto const known = std::find_if(state.heads.begin(), state.heads.end(),
                                        [coldflame](Head const& head) { return head.guid == coldflame->GetGUID(); });
        Head head =
            known != state.heads.end() ? *known : Head{coldflame->GetGUID(), coldflame->GetPosition(), now, now};
        if (head.last.GetExactDist2d(coldflame) > HEAD_MOVED)
        {
            head.last = coldflame->GetPosition();
            head.lastMoveMs = now;
        }
        heads.push_back(head);

        uint32 const age = getMSTimeDiff(head.firstSeenMs, now);
        bool const stalled = age > HEAD_STALL_MS && getMSTimeDiff(head.lastMoveMs, now) > HEAD_STALL_MS;
        if (age > HEAD_LIFETIME_MS || stalled)
            continue;

        for (uint32 step = 0; step < HEAD_LOOKAHEAD; ++step)
            state.circles.push_back(
                Offset(coldflame->GetPosition(), coldflame->GetOrientation(), step * LM_COLDFLAME_STEP));
    }
    state.heads = std::move(heads);

    if (boss->HasAura(SPELL_LM_BONE_STORM) && !boss->movespline->Finalized())
    {
        G3D::Vector3 const end = boss->movespline->FinalDestination();
        Position const destination(end.x, end.y, end.z);
        for (uint32 line = 0; line < 4; ++line)
        {
            float const angle = static_cast<float>(M_PI) / 4.0f + line * static_cast<float>(M_PI) / 2.0f;
            for (uint32 step = 0; step < CROSS_LOOKAHEAD; ++step)
                state.circles.push_back(Offset(destination, angle, CROSS_FIRST + step * LM_COLDFLAME_STEP));
        }

        if (RaidObs::Active() && (!state.crossNoted || state.notedCross.GetExactDist2d(destination) > 1.0f))
        {
            float const reach = CROSS_FIRST + (CROSS_LOOKAHEAD - 1) * LM_COLDFLAME_STEP + LM_COLDFLAME_REACH;
            RaidObs::NoteHazardCircle(boss->GetMap(), SPELL_LM_COLDFLAME_STORM, destination, reach, CROSS_NOTE_TTL_MS);
            state.notedCross = destination;
            state.crossNoted = true;
        }
    }
}

bool ClearOf(Position const& spot, std::vector<Position> const& circles, float radius)
{
    return std::none_of(circles.begin(), circles.end(),
                        [&spot, radius](Position const& circle) { return circle.GetExactDist2d(spot) < radius; });
}

void ReadSpikes(LmState& state, Player* bot, uint32 now)
{
    if (state.spikesRead && state.spikeMs == now)
        return;

    state.spikesRead = true;
    state.spikeMs = now;
    state.spikes.clear();
    state.spikeFar.clear();
    state.rangedDps.clear();

    std::vector<Creature*> spikes;
    std::vector<Player*> melee;
    Player* mainTank = nullptr;
    ForEachMember(bot,
                  [&](Player* member)
                  {
                      if (!member->IsAlive())
                          return;
                      if (!mainTank && PlayerbotAI::IsMainTank(member))
                          mainTank = member;

                      // The victim rides its spike, so the aura finds every spike without a sweep
                      if (member->HasAura(SPELL_LM_IMPALED))
                      {
                          Unit* base = member->GetVehicleBase();
                          Creature* spike = base ? base->ToCreature() : nullptr;
                          if (spike && spike->IsAlive() && IsSpikeEntry(spike->GetEntry()))
                              spikes.push_back(spike);
                          return;
                      }

                      if (IsMeleeDpsBot(member))
                          melee.push_back(member);
                      else if (GET_PLAYERBOT_AI(member) && IsRaidDpsBot(member))
                          state.rangedDps.push_back(member->GetGUID());
                  });
    std::sort(spikes.begin(), spikes.end(), IccGuidLess());
    std::sort(state.rangedDps.begin(), state.rangedDps.end());

    for (Creature* spike : spikes)
    {
        bool const sliced = mainTank && spike->GetExactDist2d(mainTank) <= LM_SLICE_KEEPOUT;
        bool const reached = std::any_of(melee.begin(), melee.end(), [spike](Player* member)
                                         { return member->GetExactDist2d(spike) <= LM_MELEE_SPIKE_REACH; });
        state.spikes.push_back(spike->GetGUID());
        state.spikeFar.push_back(sliced || !reached);
    }
}

void NoteSpike(Player* bot, char const* rule)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "lm.spike", rule);
}

// The aura lands when the 3 s cast ends, and he starts to spin then
IccLmPhase ReadPhase(Creature* boss)
{
    if (boss->HasAura(SPELL_LM_BONE_STORM))
        return IccLmPhase::Storm;

    return boss->FindCurrentSpellBySpellId(SPELL_LM_BONE_STORM) ? IccLmPhase::StormCast : IccLmPhase::Normal;
}

bool HoldsAnchor(Player* player)
{
    LmState* state = player->GetInstanceId() ? lmStates.Find(player->GetInstanceId()) : nullptr;
    return state && AnchorOf(*state, player->GetGUID()) != 0;
}

// Bone Storm lasts 20 s on 10-man and 30 s on 25-man (EVENT_BEGIN_BONE_STORM). The core shortens the aura
// just after it lands, so a longer reading means no time has passed.
uint32 StormElapsedMs(Aura const* storm, bool raid25)
{
    uint32 const total = raid25 ? 30000 : 20000;
    int32 const left = storm->GetDuration();
    return left < 0 || static_cast<uint32>(left) > total ? 0 : total - static_cast<uint32>(left);
}

// Signed, so a time already past reads negative
int32 UntilMs(uint32 when, uint32 now) { return static_cast<int32>(when - now); }

// Bone Slice is off from the storm cast, and resumes LM_SLICE_DELAY_MS after the aura ends
void ArmWindows(LmState& state, Player* bot, Creature* boss, IccLmPhase phase, uint32 now)
{
    if (phase != IccLmPhase::Normal)
        state.sliceArmed = false;
    else if (!state.sliceArmed)
    {
        state.sliceArmed = true;
        state.sliceOpenMs = now + LM_SLICE_DELAY_MS;
        ++state.sliceWindow;
    }

    Aura const* storm = phase == IccLmPhase::Storm ? boss->GetAura(SPELL_LM_BONE_STORM) : nullptr;
    if (!storm)
        state.stormArmed = false;
    else if (!state.stormArmed)
    {
        state.stormArmed = true;
        state.stormStartMs = now - StormElapsedMs(storm, IsRaid25Man(bot));
    }
}

// Where the window sits for a tank: due from LM_DEFENSIVE_LEAD_MS before Bone Slice resumes until
// LM_DEFENSIVE_SPAN_MS after
bool InSliceWindow(LmState const& state, uint32 now)
{
    int32 const until = UntilMs(state.sliceOpenMs, now);
    return state.sliceArmed && until <= static_cast<int32>(LM_DEFENSIVE_LEAD_MS) &&
           -until < static_cast<int32>(LM_DEFENSIVE_SPAN_MS);
}

bool SlicingNow(LmState const& state, uint32 now) { return state.sliceArmed && UntilMs(state.sliceOpenMs, now) <= 0; }

// The human main tank gets the same window as a bot tank, as a call he presses himself
void CallHumanTank(LmState& state, Player* sender, Creature* boss, uint32 now)
{
    Unit* victim = boss->GetVictim();
    Player* human = victim ? victim->ToPlayer() : nullptr;
    if (!human || GET_PLAYERBOT_AI(human) || !human->IsAlive() || !IsLmTank(human))
        return;

    bool const window = InSliceWindow(state, now) && state.humanCallWindow != state.sliceWindow;
    bool const low = SlicingNow(state, now) && human->GetHealthPct() < LM_TANK_DEFENSIVE_PCT;
    if (!window && !low)
        return;

    uint32 const spellId = ReadyTankDefensiveSpell(human, true, 0, 0);
    if (!spellId)
        return;

    uint32 const hitIn = window ? static_cast<uint32>(std::max(UntilMs(state.sliceOpenMs, now), 0)) : 0;
    if (CallHumanTankDefensive(sender, human, spellId, hitIn, window ? "Bone Slice resumes" : "you are low",
                               "lm.humancall") &&
        window)
        state.humanCallWindow = state.sliceWindow;
}
}  // namespace

bool IsLmLive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ICC_MAP_ID)
        return false;

    InstanceScript* instance = bot->GetInstanceScript();
    return instance && instance->GetBossState(ICC_BOSS_LORD_MARROWGAR) == IN_PROGRESS;
}

Creature* GetLmBoss(PlayerbotAI* botAI)
{
    LmState* state = LiveState(botAI);
    if (!state)
        return nullptr;

    Player* bot = botAI->GetBot();
    Creature* boss = state->boss.IsEmpty() ? nullptr : bot->GetMap()->GetCreature(state->boss);
    if (!boss || !boss->IsAlive())
    {
        uint32 const now = getMSTime();
        if (state->bossScanned && getMSTimeDiff(state->bossScanMs, now) < BOSS_RESCAN_MS)
            return nullptr;

        state->bossScanned = true;
        state->bossScanMs = now;
        boss = bot->FindNearestCreature(NPC_LORD_MARROWGAR, BOSS_SEARCH_RADIUS);
        state->boss = boss ? boss->GetGUID() : ObjectGuid::Empty;
    }

    return boss && boss->IsAlive() && boss->IsInCombat() ? boss : nullptr;
}

IccLmPhase GetLmPhase(Player* bot, Creature* boss)
{
    IccLmPhase const phase = ReadPhase(boss);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "lm.phase",
                             phase == IccLmPhase::Storm       ? "bonestorm"
                             : phase == IccLmPhase::StormCast ? "stormcast"
                                                              : "normal");
    return phase;
}

Unit* GetLmTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !IsRaidDpsBot(bot) || bot->HasAura(SPELL_LM_IMPALED))
        return nullptr;

    Creature* boss = GetLmBoss(botAI);
    LmState* state = boss ? LiveState(botAI) : nullptr;
    if (!state)
        return nullptr;

    if (Unit* skull = GetRaidHumanSkull(botAI, &IsLmUnit))
    {
        NoteSpike(bot, "skull");
        return skull;
    }

    ReadSpikes(*state, bot, getMSTime());
    Map* map = bot->GetMap();
    // A walk to a spike in a heroic storm crosses his lanes, so only one the bot reaches from its stand counts
    bool const storm = !state->spikes.empty() && ReadPhase(boss) != IccLmPhase::Normal;
    float const stormReach = PlayerbotAI::IsRanged(bot) ? LM_STORM_RANGED_REACH : LM_MELEE_SPIKE_REACH;
    std::vector<Creature*> spikes;
    std::vector<Creature*> far;
    for (size_t i = 0; i < state->spikes.size(); ++i)
    {
        Creature* spike = map->GetCreature(state->spikes[i]);
        if (!spike || !spike->IsAlive() || (storm && bot->GetExactDist2d(spike) > stormReach))
            continue;

        spikes.push_back(spike);
        if (state->spikeFar[i])
            far.push_back(spike);
    }

    if (spikes.empty())
    {
        NoteSpike(bot, "boss");
        return boss;
    }

    if (!PlayerbotAI::IsRanged(bot))
    {
        // Cleave the spikes in reach, never one next to the main tank, where Bone Slice would hit the melee
        Player* mainTank = GetMainTank(bot);
        Creature* pick = nullptr;
        for (Creature* spike : spikes)
        {
            if (bot->GetExactDist2d(spike) > LM_MELEE_SPIKE_REACH ||
                (mainTank && spike->GetExactDist2d(mainTank) <= LM_SLICE_KEEPOUT))
                continue;
            if (!pick || bot->GetExactDist2d(spike) < bot->GetExactDist2d(pick))
                pick = spike;
        }

        NoteSpike(bot, pick ? "melee" : "boss");
        return pick ? pick : boss;
    }

    // Ranged take the spikes no melee reaches first, split evenly by guid, then help on the rest
    std::vector<Creature*> const& pool = far.empty() ? spikes : far;
    int32 const index = IndexOf(state->rangedDps, bot->GetGUID());
    if (index < 0)
    {
        NoteSpike(bot, "boss");
        return boss;
    }

    std::vector<size_t> const sizes = IccBalancedGroupSizes(state->rangedDps.size(), pool.size());
    size_t const bucket = IccAssignedBucketIndex(static_cast<size_t>(index), sizes);
    NoteSpike(bot, far.empty() ? "spread" : "far");
    return pool[std::min(bucket, pool.size() - 1)];
}

bool IccLmHoldsStand(PlayerbotAI* botAI) { return HeldStand(botAI) != nullptr; }

bool IccLmHoldsNormalStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && !IsStormStand(stand->kind);
}

bool IccLmHoldsStormStand(PlayerbotAI* botAI)
{
    Stand const* stand = HeldStand(botAI);
    return stand && IsStormStand(stand->kind);
}

bool GetLmStandSpot(PlayerbotAI* botAI, Position& spot)
{
    Stand const* stand = HeldStand(botAI);
    if (!stand)
        return false;

    spot = stand->spot;
    return true;
}

bool GetLmStandWalk(PlayerbotAI* botAI, Position& spot)
{
    Stand const* stand = HeldStand(botAI);
    if (!stand || stand->parked)
        return false;

    // Nothing walks a bot back onto a line the dodge just took it off
    std::vector<Position> circles;
    GetLmColdflame(botAI, circles);
    Player* bot = botAI->GetBot();
    if (!ClearOf(stand->spot, circles, LM_DODGE_TRIGGER + LM_STAND_PARK))
        return false;
    for (Position const& circle : circles)
        if (EncounterHelpers::SegmentCrossesCircle(bot->GetPosition(), stand->spot, {circle, LM_DODGE_TRIGGER}))
            return false;

    spot = stand->spot;
    return true;
}

void GetLmColdflame(PlayerbotAI* botAI, std::vector<Position>& circles)
{
    circles.clear();
    Creature* boss = GetLmBoss(botAI);
    LmState* state = boss ? LiveState(botAI) : nullptr;
    if (!state)
        return;

    ScanColdflame(*state, boss, getMSTime());
    circles = state->circles;
}

bool IsLmInColdflame(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive() || bot->HasAura(SPELL_LM_IMPALED))
        return false;

    std::vector<Position> circles;
    GetLmColdflame(botAI, circles);
    bool const inside = !ClearOf(bot->GetPosition(), circles, LM_DODGE_TRIGGER);

    // lm.dodge is change-only and the dodge only runs inside, so the way out is written here
    if (!inside && RaidObs::Active())
        RaidObs::NoteDerived(bot, "lm.dodge", "clear");

    return inside;
}

uint32 GetLmTankDefensive(PlayerbotAI* botAI, char const* noteKind)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return 0;

    Creature* boss = GetLmBoss(botAI);
    LmState* state = boss ? LiveState(botAI) : nullptr;
    if (!state)
        return 0;

    uint8 const duty = DutyOf(*state, bot->GetGUID());
    if (duty != static_cast<uint8>(IccLmDuty::MainTank) && duty != static_cast<uint8>(IccLmDuty::OffTank))
        return 0;

    uint32 const now = getMSTime();
    auto const spent = state->defensiveWindow.find(bot->GetGUID());
    bool const windowOpen =
        InSliceWindow(*state, now) && (spent == state->defensiveWindow.end() || spent->second != state->sliceWindow);
    bool const emergency = SlicingNow(*state, now) && bot->GetHealthPct() < LM_TANK_DEFENSIVE_PCT;
    if (!windowOpen && !emergency)
    {
        // Change-latched, so the way out of a window is written too
        if (noteKind && RaidObs::Active())
            RaidObs::NoteDerived(bot, noteKind, "idle");

        return 0;
    }

    return NextTankDefensive(botAI, bot, noteKind, true);
}

bool IsLmTankDefensiveHeld(PlayerbotAI* botAI)
{
    return IsLmLive(botAI) && botAI->GetBot()->GetHealthPct() >= LM_TANK_LAST_RESORT_PCT;
}

void LatchLmTankDefensive(PlayerbotAI* botAI)
{
    LmState* state = LiveState(botAI);
    if (state)
        state->defensiveWindow[botAI->GetBot()->GetGUID()] = state->sliceWindow;
}

bool GetLmStormHit(PlayerbotAI* botAI, RaidPredictedHit& hit)
{
    Creature* boss = GetLmBoss(botAI);
    LmState* state = boss ? LiveState(botAI) : nullptr;
    Aura const* storm = state && state->stormArmed ? boss->GetAura(SPELL_LM_BONE_STORM) : nullptr;
    SpellInfo const* tick =
        storm ? sSpellMgr->GetSpellInfo(sSpellMgr->GetSpellIdForDifficulty(SPELL_LM_BONE_STORM_HIT, boss)) : nullptr;
    if (!tick || storm->GetDuration() < 0)
        return false;

    // The first window that hasn't passed its late edge, counted from the storm's first tick
    uint32 const windowMs = LM_STORM_TICK_MS * LM_STORM_WINDOW_TICKS;
    uint32 const now = getMSTime();
    uint32 const firstTick = state->stormStartMs + LM_STORM_TICK_MS;
    int32 const past = static_cast<int32>(now - firstTick) - static_cast<int32>(RAID_COVER_WINDOW_LATE_MS);
    uint32 const index = past <= 0 ? 0 : (static_cast<uint32>(past) + windowMs - 1) / windowMs;
    uint32 const start = firstTick + index * windowMs;
    if (UntilMs(start, now) > static_cast<int32>(2 * RAID_COVER_WINDOW_LEAD_MS))
        return false;

    uint32 const end = now + static_cast<uint32>(storm->GetDuration());
    uint32 ticks = 0;
    while (ticks < LM_STORM_WINDOW_TICKS && UntilMs(end, start + ticks * LM_STORM_TICK_MS) > 0)
        ++ticks;

    if (!ticks)
        return false;

    SpellEffectInfo const& effect = tick->Effects[EFFECT_0];
    int32 const perTick = std::max(effect.BasePoints + (effect.DieSides + 1) / 2, 0);
    float const divisor = std::max(std::sqrt(std::max(LM_STORM_RAID_DISTANCE - 9.0f, 0.0f)), 1.0f);

    hit = RaidPredictedHit();
    hit.startMs = start;
    hit.durationMs = (ticks - 1) * LM_STORM_TICK_MS;
    hit.damage = static_cast<uint32>(static_cast<float>(perTick * static_cast<int32>(ticks)) / divisor);
    hit.school = SPELL_SCHOOL_MASK_NORMAL;
    hit.noChannel = &HoldsAnchor;
    return true;
}

RaidCover GetLmStormCover(PlayerbotAI* botAI)
{
    RaidPredictedHit hit;
    RaidCover const cover = GetRaidCover(botAI, ICC_DEFENSIVE_SCOPE, GetLmStormHit(botAI, hit) ? &hit : nullptr);
    return cover == RaidCover::AntiMagicZone ? RaidCover::None : cover;
}

bool IsLmStormCoverHeld(PlayerbotAI* botAI)
{
    RaidPredictedHit hit;
    return GetLmStormHit(botAI, hit) && RaidHitLeadIn(hit);
}

void TickLmEncounter(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ICC_MAP_ID || !bot->GetInstanceId())
        return;

    // A saved rti of cross or star keeps a bot off the skull in the icon-led fights after him
    if (lmRtiChecked.For(bot->GetInstanceId()).insert(bot->GetGUID()).second)
    {
        Value<std::string>* rti = botAI->GetAiObjectContext()->GetValue<std::string>("rti");
        if (rti->Get() == "cross" || rti->Get() == "star")
            rti->Set("skull");
    }

    // A wipe or a kill ends the pull: the next one rebuilds the roster and every latch
    if (!IsLmLive(botAI))
    {
        if (lmStates.Find(bot->GetInstanceId()))
            lmStates.Reset(bot->GetInstanceId());
        return;
    }

    // Evading home he is out of combat but still in progress, and nothing holds a stand round him
    Creature* boss = GetLmBoss(botAI);
    if (!boss)
    {
        if (LmState* state = lmStates.Find(bot->GetInstanceId()))
            if (auto const held = state->stands.find(bot->GetGUID()); held != state->stands.end() && held->second.held)
                ReleaseStand(bot, held->second);
        return;
    }

    LmState& state = lmStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    TrackBoss(state, boss, now);

    IccLmPhase const phase = GetLmPhase(bot, boss);
    if (phase != IccLmPhase::Normal)
    {
        state.pulling = false;
        state.pulls = 0;
    }

    ArmWindows(state, bot, boss, phase, now);
    RefreshRoster(state, bot, now);
    CallHumanTank(state, bot, boss, now);
    UpdateStand(state, botAI, boss, phase, now);
}

std::vector<uint32> const& GetLmOpenerBosses(PlayerbotAI* /*botAI*/)
{
    static std::vector<uint32> const bosses = {NPC_LORD_MARROWGAR};
    return bosses;
}
