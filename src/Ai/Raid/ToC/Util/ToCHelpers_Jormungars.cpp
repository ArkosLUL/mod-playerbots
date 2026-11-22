#include "ToCHelpers_Jormungars.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "InstanceScript.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidRedirectThreat.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Gormok.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "ToCHelpers_Shared.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{
static_assert(WORM_SPOT_FLOOR_RADIUS == ICEHOWL_CHARGE_REACH_GATE - 2.0f,
              "the spot floor sits 2 yd inside Icehowl's gate-side charge end");

namespace
{
constexpr size_t SLOT_ACIDMAW = 0;
constexpr size_t SLOT_DREADSCALE = 1;
constexpr size_t SLOT_COUNT = 2;

constexpr float DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;
constexpr float TWO_PI = 2.0f * static_cast<float>(M_PI);

enum class WormForm : uint8
{
    None = 0,
    DreadscaleMobile = 1,
    AcidmawMobile = 2,
    Submerged = 3,
    LoneDreadscale = 4,
    LoneAcidmaw = 5,
};

enum class WormRole : uint8
{
    Tank,
    Heal,
    Ranged,
    Melee
};

constexpr float POOL_SEARCH_RADIUS = 200.0f;
constexpr uint32 POOL_SEARCH_INTERVAL_MS = 500;
constexpr uint32 SLIME_POOL_PUSH_PASSES = 2;
constexpr uint32 BILE_SCATTER_FRESH_MS = 2000;
constexpr uint32 BILE_SCATTER_HOLD_MS = 6000;
constexpr float BILE_HEALER_REACH_PAD = 3.0f;
constexpr uint32 SLIME_POOL_READ_STALE_MS = 2000;
constexpr uint32 SLIME_POOL_RISE_TICKS = 3;
constexpr uint32 SPEW_NOTE_TTL_MS = 3500;
constexpr float MELEE_BEHIND = 6.0f;
static_assert(MELEE_BEHIND <= WORM_STATIONARY_MELEE_RANGE - 1.0f, "the aim is inside either form's role reach");
constexpr float DRAG_LEAD = 10.0f;
constexpr uint32 DRAG_POOL_HORIZON_TICKS = 15;
constexpr float DRAG_OFFSETS = WORM_MELEE_RING + WORM_TANK_OFFSET + WORM_POOL_CLEARANCE_PAD;
constexpr float DRAG_STATIONARY_CLEARANCE = WORM_SWEEP_CLEARANCE + DRAG_OFFSETS;
constexpr float FEET_CLEARANCE = 1.0f;
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
constexpr uint32 ROLE_REFRESH_MS = 1000;
constexpr float HUNTER_MIN_RANGE_PAD = 6.0f;
constexpr float SWEEP_CURE_GAP = WORM_SWEEP_PATH_RADIUS - WORM_CURE_REACH + 1.0f;
constexpr double PREFER_JITTER_STEP = 2.39996;
constexpr float REACH_SEARCH_SLACK = 0.05f;
constexpr float REACH_HOLD_SLACK = 1.0f;
constexpr float SPEW_PATH_STEP = 1.0f;
constexpr float SPEW_EXIT_SLACK = 4.0f;

struct WormRead
{
    ObjectGuid guid;
    Position pos;
    bool submerged = false;
    bool mobile = false;
    bool spewing = false;
    bool sweeping = false;
    bool settled = false;
    bool spewArmed = false;
};

struct WormSlotClock
{
    ObjectGuid guid;
    uint32 motionMs = 0;
    Position motionPos;
    uint32 spewMarkMs = 0;
    uint32 spewDelayMs = 0;
    bool submerged = false;
    bool spewing = false;
};

struct WormSpray
{
    ObjectGuid target;
    Position targetPos;
    ObjectGuid worm;
    bool burning = false;
};

struct WormPool
{
    Position pos;
    uint32 tick = 0;
    uint32 lastTick = 0;
    float radius = 0.0f;
    float radiusSoon = 0.0f;
};

struct WormMember
{
    ObjectGuid guid;
    Position pos;
    WormRole role = WormRole::Melee;
    bool bot = false;
    bool toxin = false;
    int32 snare = 0;
    bool bile = false;
    uint32 bileAgeMs = 0;
    bool holder = false;
    bool stationaryHolder = false;
    bool stuck = false;
    bool runner = false;
};

struct WormHold
{
    WormHoldView view;
    uint32 checkedMs = 0;
    ObjectGuid target;
    uint8 worm = 0;
    ObjectGuid anchor;
    Position anchorPos;
    ObjectGuid partner;
    bool melee = false;
};

struct BileScatter
{
    Position spot;
    uint32 dealtMs = 0;
};

struct JormungarsState
{
    RaidObs::ObsValue<uint8> worm{"nb.worm"};

    uint32 memoMs = 0;
    bool memoValid = false;
    bool live = false;
    bool engaged = false;

    std::array<WormRead, SLOT_COUNT> worms;
    std::array<bool, SLOT_COUNT> spewNoted{};
    std::array<WormSlotClock, SLOT_COUNT> clocks;
    WormSpray spray;

    uint32 poolSearchMs = 0;
    bool poolSearched = false;
    std::vector<ObjectGuid> poolGuids;
    std::vector<WormPool> pools;
    uint32 poolsReadMs = 0;

    std::vector<WormMember> roster;
    std::unordered_map<ObjectGuid, ObjectGuid> runnerOf;
    std::unordered_map<ObjectGuid, ObjectGuid> carrierOf;

    std::unordered_map<ObjectGuid, WormRole> roles;
    uint32 rolesMs = 0;
    bool rolesRead = false;

    std::unordered_map<ObjectGuid, WormHold> holds;
    std::unordered_map<ObjectGuid, uint32> backoffs;
    std::unordered_map<ObjectGuid, BileScatter> bileScatter;
    std::unordered_map<ObjectGuid, uint32> bileScatterTried;
    std::unordered_map<ObjectGuid, uint32> reachHolds;
    std::unordered_map<ObjectGuid, uint32> dragNeededSince;
    RaidObs::ObsGuidMap<uint8> brake{"nb.brake"};
    std::unordered_map<ObjectGuid, uint32> brakeReadMs;
    ToCGroupMemo<std::vector<ObjectGuid>> hunters;
};

RaidInstanceState<JormungarsState> jormungarsStates;

float PoolRadius(uint32 tick)
{
    return WORM_POOL_BASE_RADIUS + WORM_POOL_GROWTH_PER_TICK * static_cast<float>(tick);
}

uint32 PoolTicksIn(uint32 ms) { return (ms + 999) / 1000; }

float PoolRadiusIn(WormPool const& pool, uint32 ticks)
{
    return PoolRadius(std::min(pool.tick + ticks, pool.lastTick));
}

float DragPoolClearance(WormPool const& pool) { return PoolRadiusIn(pool, DRAG_POOL_HORIZON_TICKS) + DRAG_OFFSETS; }

float HalfArcOf(uint32 tickSpellId)
{
    if (SpellCone const* cone = sSpellMgr->GetSpellCone(tickSpellId))
        return static_cast<float>(cone->cone_degrees) * static_cast<float>(M_PI) / 360.0f;

    return WORM_SPEW_DEFAULT_HALF_ARC;
}

bool InCone(Position const& origin, float x, float y, float halfArc)
{
    float const dx = x - origin.GetPositionX();
    float const dy = y - origin.GetPositionY();
    if (dx * dx + dy * dy > WORM_SPEW_REACH * WORM_SPEW_REACH)
        return false;

    float diff = Position::NormalizeOrientation(std::atan2(dy, dx) - origin.GetOrientation());
    if (diff > static_cast<float>(M_PI))
        diff -= TWO_PI;

    return std::fabs(diff) <= halfArc;
}

float AngleOffFacing(Position const& origin, float x, float y)
{
    float const diff = Position::NormalizeOrientation(
        std::atan2(y - origin.GetPositionY(), x - origin.GetPositionX()) - origin.GetOrientation());
    return diff > static_cast<float>(M_PI) ? TWO_PI - diff : diff;
}

bool InSpewDanger(Position const& origin, float x, float y, float halfArc)
{
    return InCone(origin, x, y, halfArc) || (origin.GetExactDist2d(x, y) < WORM_TANK_OFFSET &&
                                             AngleOffFacing(origin, x, y) < static_cast<float>(M_PI) / 2.0f);
}

float TurnBetween(float from, float to)
{
    float const turn = Position::NormalizeOrientation(to - from);
    return turn > static_cast<float>(M_PI) ? TWO_PI - turn : turn;
}

bool IsUp(WormRead const& worm) { return !worm.guid.IsEmpty() && !worm.submerged; }

bool IsWormEntry(Unit const* unit)
{
    if (!unit)
        return false;

    uint32 const entry = unit->GetEntry();
    return entry == static_cast<uint32>(ToCNpcs::NPC_ACIDMAW) || entry == static_cast<uint32>(ToCNpcs::NPC_DREADSCALE);
}

bool ShowsMobile(Unit* worm)
{
    uint32 const displayId = worm->GetNativeDisplayId();
    return displayId == static_cast<uint32>(ToCDisplayIds::MODEL_ACIDMAW_MOBILE) ||
           displayId == static_cast<uint32>(ToCDisplayIds::MODEL_DREADSCALE_MOBILE);
}

Unit* OtherLivingWorm(Unit* worm)
{
    InstanceScript* instance = worm->GetInstanceScript();
    if (!instance)
        return nullptr;

    uint32 const data =
        worm->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_ACIDMAW) ? TOC_DATA_DREADSCALE : TOC_DATA_ACIDMAW;
    Creature* other = worm->GetMap()->GetCreature(instance->GetGuidData(data));
    return other && other->IsAlive() ? other : nullptr;
}

WormRole RoleOf(Player* bot)
{
    if (IsBeastsTank(bot))
        return WormRole::Tank;
    if (PlayerbotAI::IsHeal(bot))
        return WormRole::Heal;
    return PlayerbotAI::IsRangedDps(bot) ? WormRole::Ranged : WormRole::Melee;
}

bool IsCaster(WormRole role) { return role == WormRole::Ranged || role == WormRole::Heal; }

void NoteCure(Player* bot, char const* branch, ObjectGuid const& other = ObjectGuid::Empty)
{
    if (!RaidObs::Active())
        return;

    std::string note = branch;
    if (!other.IsEmpty())
        note += " " + RaidObs::DescribeAssignment(other);

    RaidObs::NoteDerived(bot, "nb.cure", note);
}

void NoteHold(Player* bot, std::string const& value)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "nb.wormhold", value);
}

void ReadWorm(Map* map, Unit* worm, uint32 spewCast, uint32 spewTick, WormRead& read, bool& spewNoted)
{
    read = WormRead();
    if (!worm)
    {
        spewNoted = false;
        return;
    }

    read.guid = worm->GetGUID();
    read.pos = worm->GetPosition();
    read.submerged = IsWormSubmerged(worm);
    read.mobile = IsWormMobile(worm);
    if (!read.submerged)
    {
        read.spewing = worm->FindCurrentSpellBySpellId(spewCast) != nullptr || worm->HasAura(spewCast);
        read.sweeping =
            worm->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_SWEEP, worm)) != nullptr;
    }

    if (!read.spewing)
    {
        spewNoted = false;
        return;
    }

    if (spewNoted)
        return;

    spewNoted = true;
    if (!RaidObs::Active())
        return;

    char params[80];
    snprintf(params, sizeof(params), "\"facing\":%.2f,\"arc\":%.1f,\"range\":%.1f", worm->GetOrientation(),
             HalfArcOf(spewTick) * 180.0f / static_cast<float>(M_PI), WORM_SPEW_HIT_RANGE);
    RaidObs::NoteHazard(map, spewTick, read.pos, "wedge", params, SPEW_NOTE_TTL_MS);
}

void ClockWorm(Unit* worm, WormRead& read, WormSlotClock& clock, uint32 now)
{
    if (!worm)
        return;

    bool const fresh = clock.guid != read.guid;
    if (fresh)
    {
        clock = WormSlotClock();
        clock.guid = read.guid;
    }

    auto const markSpew = [&clock, now](uint32 delayMs)
    {
        clock.spewMarkMs = now;
        clock.spewDelayMs = delayMs;
    };

    if (fresh)
        markSpew(WORM_SPEW_FIRST_MS);
    else if (clock.submerged && !read.submerged)
        markSpew(0);
    if (read.spewing && !clock.spewing)
        markSpew(WORM_SPEW_REPEAT_MS);

    clock.submerged = read.submerged;
    clock.spewing = read.spewing;

    if (fresh || worm->isMoving() || read.pos.GetExactDist2d(clock.motionPos) > WORM_SETTLE_MOVE ||
        TurnBetween(clock.motionPos.GetOrientation(), read.pos.GetOrientation()) > WORM_SETTLE_TURN_DEG * DEG_TO_RAD)
    {
        clock.motionMs = now;
        clock.motionPos = read.pos;
    }

    read.settled = getMSTimeDiff(clock.motionMs, now) >= WORM_SETTLE_MS;
    read.spewArmed =
        read.spewing || getMSTimeDiff(clock.spewMarkMs, now) + WORM_SPEW_LEAD_MS >= clock.spewDelayMs;
}

void ReadSpray(Unit* worm, WormRead const& read, uint32 sprayId, bool burning, WormSpray& spray)
{
    if (!worm || !IsUp(read) || read.mobile)
        return;

    Spell* cast = worm->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!cast || cast->getState() != SPELL_STATE_PREPARING || cast->m_spellInfo->Id != sprayId)
        return;

    ObjectGuid const targetGuid = cast->m_targets.GetUnitTargetGUID();
    Unit* target = targetGuid.IsEmpty() ? nullptr : ObjectAccessor::GetUnit(*worm, targetGuid);
    if (!target)
        return;

    spray.target = targetGuid;
    spray.targetPos = target->GetPosition();
    spray.worm = read.guid;
    spray.burning = burning;
}

void RefreshPools(Player* bot, Map* map, JormungarsState& state, uint32 now)
{
    if (!state.poolSearched || getMSTimeDiff(state.poolSearchMs, now) >= POOL_SEARCH_INTERVAL_MS)
    {
        state.poolSearched = true;
        state.poolSearchMs = now;
        state.poolGuids.clear();

        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, static_cast<uint32>(ToCNpcs::NPC_SLIME_POOL), POOL_SEARCH_RADIUS);
        for (Creature const* pool : found)
            if (pool->IsAlive())
                state.poolGuids.push_back(pool->GetGUID());
    }

    state.pools.clear();
    for (ObjectGuid const& guid : state.poolGuids)
    {
        Creature* pool = map->GetCreature(guid);
        if (!pool || !pool->IsAlive())
            continue;

        Aura* aura = pool->GetAura(SPELL_SLIME_POOL_AURA);
        AuraEffect* grow = aura ? aura->GetEffect(EFFECT_0) : nullptr;
        if (!grow)
            continue;

        int32 left = aura->GetDuration();
        if (TempSummon* summon = pool->ToTempSummon())
            left = std::min(left, static_cast<int32>(summon->GetTimer()));

        WormPool read;
        read.pos = pool->GetPosition();
        read.tick = grow->GetTickNumber();
        read.lastTick = read.tick + PoolTicksIn(static_cast<uint32>(std::max(left, 0)));
        read.radius = PoolRadius(read.tick);
        read.radiusSoon = PoolRadiusIn(read, WORM_POOL_SOON_TICKS);
        state.pools.push_back(read);
    }

    state.poolsReadMs = now;
}

void ClearPools(JormungarsState& state)
{
    state.poolSearched = false;
    state.poolGuids.clear();
    state.pools.clear();
}

WormMember const* FindMember(JormungarsState const& state, ObjectGuid const& guid)
{
    for (WormMember const& member : state.roster)
        if (member.guid == guid)
            return &member;

    return nullptr;
}

bool RunsFar(WormMember const& carrier) { return carrier.toxin && carrier.snare <= WORM_TOXIN_STUCK_SLOW; }
float RunTrigger(WormMember const& carrier) { return RunsFar(carrier) ? WORM_RUN_TRIGGER : WORM_CURE_TRIGGER; }

bool HeldOut(JormungarsState const& state, WormMember const* me, BeastsTankDuty duty)
{
    return me && me->bile && duty == BeastsTankDuty::None && !state.carrierOf.count(me->guid);
}

bool SearchBackedOff(JormungarsState const& state, ObjectGuid const& guid, uint32 now)
{
    auto const backoff = state.backoffs.find(guid);
    return backoff != state.backoffs.end() && getMSTimeDiff(backoff->second, now) < WORM_SEARCH_BACKOFF_MS;
}

template <typename ByGuid>
void EraseDeparted(JormungarsState const& state, ByGuid& byGuid)
{
    for (auto entry = byGuid.begin(); entry != byGuid.end();)
        entry = FindMember(state, entry->first) ? std::next(entry) : byGuid.erase(entry);
}

void RefreshRoster(PlayerbotAI* botAI, Player* bot, Map* map, JormungarsState& state, uint32 now)
{
    state.roster.clear();
    if (!state.rolesRead || getMSTimeDiff(state.rolesMs, now) >= ROLE_REFRESH_MS)
    {
        state.roles.clear();
        state.rolesMs = now;
        state.rolesRead = true;
    }

    uint32 const toxinId = sSpellMgr->GetSpellIdForDifficulty(SPELL_PARALYTIC_TOXIN, bot);
    Player* mobileHolder = GetBeastsDutyHolder(botAI, BeastsTankDuty::WormMobile);
    Player* stationaryHolder = GetBeastsDutyHolder(botAI, BeastsTankDuty::WormStationary);
    Unit* mobileWorm = GetBeastOfDuty(botAI, BeastsTankDuty::WormMobile);
    Unit* stationaryWorm = GetBeastOfDuty(botAI, BeastsTankDuty::WormStationary);
    uint32 const instanceId = bot->GetInstanceId();

    auto const add = [&](Player* member)
    {
        if (!member || !member->IsInWorld() || !member->IsAlive() ||
            member->GetMapId() != TRIAL_OF_THE_CRUSADER_MAP_ID || member->GetInstanceId() != instanceId)
            return;

        WormMember entry;
        entry.guid = member->GetGUID();
        entry.pos = member->GetPosition();
        entry.bot = GET_PLAYERBOT_AI(member) != nullptr;

        auto role = state.roles.find(entry.guid);
        if (role == state.roles.end())
            role = state.roles.emplace(entry.guid, RoleOf(member)).first;
        entry.role = role->second;

        if (Aura* toxin = member->GetAura(toxinId))
        {
            entry.toxin = true;
            if (AuraEffect* slow = toxin->GetEffect(EFFECT_0))
                entry.snare = slow->GetAmount();
        }

        if (Aura* bile = member->GetAura(SPELL_BURNING_BILE))
        {
            entry.bile = true;
            entry.bileAgeMs = static_cast<uint32>(std::max(bile->GetMaxDuration() - bile->GetDuration(), 0));
        }

        Unit* dutyWorm = nullptr;
        if (member == mobileHolder)
            dutyWorm = mobileWorm;
        else if (member == stationaryHolder)
            dutyWorm = stationaryWorm;

        entry.holder = member == mobileHolder || member == stationaryHolder;
        entry.stationaryHolder = member == stationaryHolder;
        entry.stuck = entry.toxin && !entry.bile && (entry.holder || entry.snare <= WORM_TOXIN_STUCK_SLOW);
        entry.runner = entry.bile && entry.bot && (!entry.holder || (dutyWorm && IsWormSubmerged(dutyWorm)));
        state.roster.push_back(entry);
    };

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            add(ref->GetSource());
    }
    else
        add(bot);

    for (auto const& entry : state.holds)
        if (!FindMember(state, entry.first))
            if (Player* player = ObjectAccessor::GetPlayer(map, entry.first))
                NoteHold(player, "release gone");

    EraseDeparted(state, state.holds);
    EraseDeparted(state, state.backoffs);
    EraseDeparted(state, state.bileScatter);
    EraseDeparted(state, state.bileScatterTried);
    EraseDeparted(state, state.reachHolds);
    EraseDeparted(state, state.dragNeededSince);
}

uint32 PinnedCastRemainingMs(Player* player)
{
    if (!player->IsMovementPreventedByCasting())
        return 0;

    int32 remaining = 0;
    if (Spell* cast = player->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        if (cast->getState() == SPELL_STATE_PREPARING)
            remaining = std::max(remaining, cast->GetCastTimeRemaining());

    if (Spell* channel = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        if (channel->getState() == SPELL_STATE_CASTING)
            remaining = std::max(remaining, channel->GetCastTimeRemaining());

    return static_cast<uint32>(remaining);
}

bool SprayLandsNear(JormungarsState const& state, ObjectGuid const& self, float x, float y)
{
    if (state.spray.target.IsEmpty())
        return false;

    if (state.spray.target != self)
        return state.spray.targetPos.GetExactDist2d(x, y) < WORM_SPRAY_TRIGGER;

    return std::any_of(state.roster.begin(), state.roster.end(), [&self, x, y](WormMember const& member)
                       { return member.guid != self && member.pos.GetExactDist2d(x, y) < WORM_SPRAY_TRIGGER; });
}

bool UrgentAt(JormungarsState const& state, ObjectGuid const& self, float x, float y, bool toxin, bool sweepHolder,
              bool sprayDodge, float halfArc, uint32 castRemainingMs)
{
    uint32 const castTicks = PoolTicksIn(castRemainingMs);
    for (WormPool const& pool : state.pools)
        if (pool.pos.GetExactDist2d(x, y) < PoolRadiusIn(pool, castTicks))
            return true;

    if (sprayDodge && SprayLandsNear(state, self, x, y))
        return true;

    if (!toxin)
        for (WormMember const& member : state.roster)
            if (member.bile && member.guid != self && member.pos.GetExactDist2d(x, y) < WORM_SPLASH_RADIUS)
                return true;

    for (WormRead const& worm : state.worms)
    {
        if (!IsUp(worm))
            continue;

        if (worm.mobile && worm.spewing && InSpewDanger(worm.pos, x, y, halfArc))
            return true;

        if (!worm.mobile && !sweepHolder && worm.sweeping && worm.pos.GetExactDist2d(x, y) < WORM_SWEEP_RADIUS)
            return true;
    }

    return false;
}

bool CureReachable(JormungarsState const& state, WormMember const& walker, WormMember const& partner)
{
    if (walker.stationaryHolder)
        return true;

    for (WormRead const& worm : state.worms)
        if (IsUp(worm) && !worm.mobile && worm.pos.GetExactDist2d(partner.pos) < SWEEP_CURE_GAP)
            return false;

    return true;
}

void PairRunners(JormungarsState& state)
{
    std::unordered_map<ObjectGuid, ObjectGuid> const previous = std::move(state.runnerOf);
    state.runnerOf.clear();
    state.carrierOf.clear();

    std::vector<WormMember const*> stuck;
    std::vector<WormMember const*> runners;
    for (WormMember const& member : state.roster)
    {
        if (member.stuck)
            stuck.push_back(&member);
        if (member.runner)
            runners.push_back(&member);
    }

    if (stuck.empty() || runners.empty())
        return;

    std::sort(stuck.begin(), stuck.end(),
              [](WormMember const* lhs, WormMember const* rhs) { return lhs->guid < rhs->guid; });

    auto const assign = [&state](WormMember const* carrier, ObjectGuid const& runner)
    {
        state.runnerOf[carrier->guid] = runner;
        state.carrierOf[runner] = carrier->guid;
    };

    auto const nearestFree = [&state, &runners](WormMember const* carrier, float maxDist) -> WormMember const*
    {
        WormMember const* best = nullptr;
        float bestDist = 0.0f;
        for (WormMember const* runner : runners)
        {
            if (state.carrierOf.count(runner->guid) || !CureReachable(state, *runner, *carrier))
                continue;

            float const dist = carrier->pos.GetExactDist(runner->pos);
            if (dist > maxDist)
                continue;

            if (!best || dist < bestDist || (!(bestDist < dist) && runner->guid < best->guid))
            {
                best = runner;
                bestDist = dist;
            }
        }

        return best;
    };

    for (WormMember const* carrier : stuck)
        if (WormMember const* runner = nearestFree(carrier, RunTrigger(*carrier)))
            assign(carrier, runner->guid);

    for (WormMember const* carrier : stuck)
    {
        if (state.runnerOf.count(carrier->guid))
            continue;

        auto const kept = previous.find(carrier->guid);
        if (kept == previous.end() || state.carrierOf.count(kept->second))
            continue;

        bool const stillRunner = std::any_of(runners.begin(), runners.end(), [&](WormMember const* runner)
                                             { return runner->guid == kept->second &&
                                                      CureReachable(state, *runner, *carrier); });
        if (stillRunner)
            assign(carrier, kept->second);
    }

    for (WormMember const* carrier : stuck)
    {
        if (state.runnerOf.count(carrier->guid))
            continue;

        if (WormMember const* runner = nearestFree(carrier, FLT_MAX))
            assign(carrier, runner->guid);
    }
}

void Disengage(Map* map, JormungarsState& state)
{
    for (auto const& entry : state.holds)
        if (Player* player = ObjectAccessor::GetPlayer(map, entry.first))
            NoteHold(player, "release phase");

    state.worms.fill(WormRead());
    state.spewNoted.fill(false);
    state.clocks.fill(WormSlotClock());
    state.spray = WormSpray();
    state.roster.clear();
    state.runnerOf.clear();
    state.carrierOf.clear();
    state.roles.clear();
    state.rolesRead = false;
    state.holds.clear();
    state.backoffs.clear();
    state.bileScatter.clear();
    state.bileScatterTried.clear();
    state.reachHolds.clear();
    state.dragNeededSince.clear();

    std::vector<ObjectGuid> braked;
    for (auto const& entry : state.brake)
        if (entry.second)
            braked.push_back(entry.first);
    for (ObjectGuid const& guid : braked)
        state.brake.Set(guid, 0);
    state.brakeReadMs.clear();
}

WormForm FormOf(PlayerbotAI* botAI, JormungarsState const& state, Unit* acidmaw, Unit* dreadscale)
{
    if ((acidmaw && state.worms[SLOT_ACIDMAW].submerged) || (dreadscale && state.worms[SLOT_DREADSCALE].submerged))
        return WormForm::Submerged;

    if (acidmaw && dreadscale)
        return GetBeastOfDuty(botAI, BeastsTankDuty::WormMobile) == dreadscale ? WormForm::DreadscaleMobile
                                                                                 : WormForm::AcidmawMobile;

    return dreadscale ? WormForm::LoneDreadscale : WormForm::LoneAcidmaw;
}

void BreakPinnedCasts(PlayerbotAI* botAI, Map* map, JormungarsState const& state)
{
    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return;

    float const halfArc = WormSpewHalfArc(botAI);
    for (WormMember const& member : state.roster)
    {
        if (member.holder && !state.carrierOf.count(member.guid))
            continue;

        Player* player = ObjectAccessor::GetPlayer(map, member.guid);
        PlayerbotAI* memberAI = player ? GET_PLAYERBOT_AI(player) : nullptr;
        Spell* cast = player ? player->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
        if (!memberAI || !cast || cast->getState() != SPELL_STATE_PREPARING || !player->IsMovementPreventedByCasting())
            continue;

        uint32 const remaining = static_cast<uint32>(std::max(cast->GetCastTimeRemaining(), 0));
        if (UrgentAt(state, member.guid, member.pos.GetPositionX(), member.pos.GetPositionY(), member.toxin,
                     member.stationaryHolder, true, halfArc, remaining))
            memberAI->RequestSpellInterrupt();
    }
}

void BreakPoolCasts(PlayerbotAI* botAI, Map* map, JormungarsState const& state)
{
    Group* group = botAI->GetBot()->GetGroup();
    Position chargeStart;
    Position chargeEnd;
    if (state.pools.empty() || !group || IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsInWorld() || !member->IsAlive() || member->GetMap() != map)
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        Spell* cast = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!memberAI || !cast || cast->getState() != SPELL_STATE_PREPARING || !member->IsMovementPreventedByCasting())
            continue;

        uint32 const ticks = PoolTicksIn(static_cast<uint32>(std::max(cast->GetCastTimeRemaining(), 0)));
        for (WormPool const& pool : state.pools)
        {
            if (pool.pos.GetExactDist2d(member) < PoolRadiusIn(pool, ticks))
            {
                memberAI->RequestSpellInterrupt();
                break;
            }
        }
    }
}

void DealBileScatter(PlayerbotAI* botAI, Map* map, JormungarsState& state, uint32 now);

JormungarsState* Refresh(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    JormungarsState& state = jormungarsStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;
    state.live = ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts);

    Unit* acidmaw = state.live ? GetEngagedBeast(botAI, NorthrendBeast::Acidmaw) : nullptr;
    Unit* dreadscale = state.live ? GetEngagedBeast(botAI, NorthrendBeast::Dreadscale) : nullptr;
    state.engaged = acidmaw || dreadscale;
    if (!state.engaged)
    {
        Disengage(map, state);
        if (state.live || state.worm.Get() != static_cast<uint8>(WormForm::None))
            state.worm = static_cast<uint8>(WormForm::None);

        if (ToCEncounterGateOpen(botAI, ToCEncounter::NorthrendBeasts))
        {
            RefreshPools(bot, map, state, now);
            BreakPoolCasts(botAI, map, state);
        }
        else
            ClearPools(state);

        return &state;
    }

    ReadWorm(map, acidmaw, SPELL_ACIDIC_SPEW, sSpellMgr->GetSpellIdForDifficulty(SPELL_ACIDIC_SPEW_TICK, bot),
             state.worms[SLOT_ACIDMAW], state.spewNoted[SLOT_ACIDMAW]);
    ReadWorm(map, dreadscale, SPELL_MOLTEN_SPEW, sSpellMgr->GetSpellIdForDifficulty(SPELL_MOLTEN_SPEW_TICK, bot),
             state.worms[SLOT_DREADSCALE], state.spewNoted[SLOT_DREADSCALE]);
    ClockWorm(acidmaw, state.worms[SLOT_ACIDMAW], state.clocks[SLOT_ACIDMAW], now);
    ClockWorm(dreadscale, state.worms[SLOT_DREADSCALE], state.clocks[SLOT_DREADSCALE], now);
    state.spray = WormSpray();
    ReadSpray(acidmaw, state.worms[SLOT_ACIDMAW], sSpellMgr->GetSpellIdForDifficulty(SPELL_PARALYTIC_SPRAY, bot),
              false, state.spray);
    ReadSpray(dreadscale, state.worms[SLOT_DREADSCALE], sSpellMgr->GetSpellIdForDifficulty(SPELL_BURNING_SPRAY, bot),
              true, state.spray);
    RefreshPools(bot, map, state, now);
    RefreshRoster(botAI, bot, map, state, now);
    PairRunners(state);
    DealBileScatter(botAI, map, state, now);
    BreakPinnedCasts(botAI, map, state);
    state.worm = static_cast<uint8>(FormOf(botAI, state, acidmaw, dreadscale));
    return &state;
}

JormungarsState* FindState(Player* bot)
{
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    return jormungarsStates.Find(bot->GetInstanceId());
}

int32 HunterIndex(Player* bot, JormungarsState const& state)
{
    Group* group = bot->GetGroup();
    if (!group)
        return -1;

    std::vector<ObjectGuid> const& hunters = state.hunters.Get(bot, [group](std::vector<ObjectGuid>& out)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsAlive() && member->getClass() == CLASS_HUNTER)
                out.push_back(member->GetGUID());
        }
    });

    for (size_t i = 0; i < hunters.size(); ++i)
        if (hunters[i] == bot->GetGUID())
            return static_cast<int32>(i);

    return -1;
}

WormRead const* FindWorm(JormungarsState const& state, ObjectGuid const& guid)
{
    if (guid.IsEmpty())
        return nullptr;

    for (WormRead const& worm : state.worms)
        if (worm.guid == guid)
            return &worm;

    return nullptr;
}

WormRead const* UpWorm(JormungarsState const& state, bool mobile)
{
    for (WormRead const& worm : state.worms)
        if (IsUp(worm) && worm.mobile == mobile)
            return &worm;

    return nullptr;
}

WormRead const* AnchorWorm(JormungarsState const& state)
{
    WormRead const* mobile = UpWorm(state, true);
    return mobile ? mobile : UpWorm(state, false);
}

bool IsFresh(WormHold const& hold, uint32 now) { return getMSTimeDiff(hold.checkedMs, now) < WORM_HOLD_FRESH_MS; }

Position HeldSpotOf(JormungarsState const& state, WormMember const& member, uint32 now)
{
    auto const scatter = state.bileScatter.find(member.guid);
    if (scatter != state.bileScatter.end())
        return scatter->second.spot;

    auto const hold = state.holds.find(member.guid);
    return hold != state.holds.end() && IsFresh(hold->second, now) ? hold->second.view.spot : member.pos;
}

Position Jittered(Position const& at, ObjectGuid const& guid)
{
    float const angle = static_cast<float>(
        std::fmod(static_cast<double>(guid.GetCounter()) * PREFER_JITTER_STEP, 2.0 * M_PI));
    return Position(at.GetPositionX() + WORM_PREFER_JITTER * std::cos(angle),
                    at.GetPositionY() + WORM_PREFER_JITTER * std::sin(angle), at.GetPositionZ());
}

std::vector<HazardCircle> HealerReach(PlayerbotAI* botAI, Player* bot, JormungarsState const& state, uint32 now,
                                      float slack)
{
    std::vector<HazardCircle> reach;
    float const range =
        botAI->GetRange("heal") + bot->GetCombatReach() + DEFAULT_COMBAT_REACH - BILE_HEALER_REACH_PAD + slack;
    for (WormMember const& member : state.roster)
        if (member.bot && member.role == WormRole::Heal && member.guid != bot->GetGUID() && !member.stuck)
            reach.emplace_back(HeldSpotOf(state, member, now), range);

    return reach;
}

bool InHealerReach(std::vector<HazardCircle> const& reach, float x, float y)
{
    return reach.empty() || std::any_of(reach.begin(), reach.end(), [x, y](HazardCircle const& circle)
                                        { return circle.first.GetExactDist2d(x, y) <= circle.second; });
}

Position MeleeStand(JormungarsState const& state, ObjectGuid const& self, Position const& worm)
{
    uint32 rank = 0;
    for (WormMember const& member : state.roster)
        if (member.bot && member.role == WormRole::Melee && member.guid < self)
            ++rank;

    float const side = rank % 2 == 0 ? 1.0f : -1.0f;
    float const angle = worm.GetOrientation() + static_cast<float>(M_PI) + side * WORM_MELEE_GROUP_DEG * DEG_TO_RAD;
    return Position(worm.GetPositionX() + WORM_MELEE_STAND * std::cos(angle),
                    worm.GetPositionY() + WORM_MELEE_STAND * std::sin(angle), worm.GetPositionZ());
}

struct CurePairing
{
    WormMember const* carrier = nullptr;
    WormMember const* nearestBile = nullptr;
};

CurePairing PairingOf(JormungarsState const& state, Player* bot, WormMember const* me, BeastsTankDuty duty)
{
    CurePairing pairing;
    if (!me)
        return pairing;

    auto const assigned = state.carrierOf.find(me->guid);
    if (assigned != state.carrierOf.end())
        pairing.carrier = FindMember(state, assigned->second);

    bool const walks = !me->stuck || (duty == BeastsTankDuty::None && !state.runnerOf.count(me->guid));
    if (!me->toxin || me->bile || !walks)
        return pairing;

    float nearestDist = 0.0f;
    for (WormMember const& member : state.roster)
    {
        if (!member.bile || member.guid == me->guid || !CureReachable(state, *me, member))
            continue;

        float const dist = bot->GetExactDist(member.pos);
        if (!pairing.nearestBile || dist < nearestDist)
        {
            pairing.nearestBile = &member;
            nearestDist = dist;
        }
    }

    auto const hold = state.holds.find(me->guid);
    if (!pairing.nearestBile || hold == state.holds.end() || hold->second.view.reason != WormMoveReason::Cure)
        return pairing;

    WormMember const* kept = FindMember(state, hold->second.partner);
    if (kept && kept->bile && CureReachable(state, *me, *kept) &&
        (nearestDist > WORM_CURE_TRIGGER || bot->GetExactDist(kept->pos) <= WORM_CURE_TRIGGER))
        pairing.nearestBile = kept;

    return pairing;
}

enum WormRuleFlag : uint8
{
    RULE_HARD = 1,
    RULE_SPREAD = 2,
    RULE_URGENT = 4,
    RULE_ESCAPE = 8,
    RULE_CLEAN = 16,
    RULE_PAIR = 32,
};

enum class SpreadTest : uint8
{
    Strict,
    PairTolerant,
    Skip
};

struct WormSpreadPeer
{
    ObjectGuid guid;
    Position spot;
    bool bot = false;
};

struct WormCircleRule
{
    WormMoveReason reason = WormMoveReason::None;
    Position centre;
    float trigger = 0.0f;
    float clearance = 0.0f;
    uint8 flags = 0;
};

struct WormRules
{
    std::vector<WormCircleRule> circles;
    std::vector<Position> cones;
    float halfArc = 0.0f;
    bool spewFires = false;
    bool hasPartner = false;
    Position partnerPos;
    float partnerTrigger = WORM_CURE_TRIGGER;
    bool hasStand = false;
    Position stand;
    WormMoveReason standReason = WormMoveReason::Melee;
    bool reachCrossesPool = false;
    ObjectGuid self;
    std::vector<WormSpreadPeer> spreadPeers;
};

bool LoneSpreadPeer(WormRules const& rules, WormSpreadPeer const& peer)
{
    for (WormSpreadPeer const& other : rules.spreadPeers)
        if (other.guid != peer.guid && other.spot.GetExactDist2d(peer.spot) < WORM_SPREAD_TRIGGER)
            return false;

    return true;
}

WormSpreadPeer const* PairPartnerOf(WormRules const& rules, Position const& from)
{
    WormSpreadPeer const* partner = nullptr;
    float nearest = 0.0f;
    for (WormSpreadPeer const& peer : rules.spreadPeers)
    {
        float const dist = peer.spot.GetExactDist2d(from);
        if ((partner && dist >= nearest) || !LoneSpreadPeer(rules, peer))
            continue;

        partner = &peer;
        nearest = dist;
    }

    return partner;
}

bool SpreadFailsAt(WormRules const& rules, Position const& spot, bool pairTolerant)
{
    std::vector<WormSpreadPeer const*> nearPeers;
    for (WormSpreadPeer const& peer : rules.spreadPeers)
        if (peer.spot.GetExactDist2d(spot) < WORM_SPREAD_TRIGGER)
            nearPeers.push_back(&peer);

    if (nearPeers.empty())
        return false;

    if (pairTolerant && nearPeers.size() == 1 && LoneSpreadPeer(rules, *nearPeers.front()))
        return false;

    return std::none_of(nearPeers.begin(), nearPeers.end(),
                        [&rules](WormSpreadPeer const* peer) { return peer->bot && rules.self < peer->guid; });
}

WormMoveReason FirstFailedRule(WormRules const& rules, float x, float y, bool heldSpot, SpreadTest spread)
{
    bool const skipSpread = spread == SpreadTest::Skip;
    WormMoveReason first = WormMoveReason::None;
    for (WormCircleRule const& rule : rules.circles)
    {
        if (rule.trigger <= 0.0f || (skipSpread && rule.reason == WormMoveReason::Spread))
            continue;

        if (first != WormMoveReason::None && first <= rule.reason)
            continue;

        if (rule.centre.GetExactDist2d(x, y) < rule.trigger)
            first = rule.reason;
    }

    if (first != WormMoveReason::None && first < WormMoveReason::Spew)
        return first;

    if (rules.spewFires)
    {
        bool const atPartner = rules.hasPartner && rules.partnerPos.GetExactDist2d(x, y) <= rules.partnerTrigger;
        float const arc = rules.halfArc + (atPartner ? 0.0f : WORM_SPEW_TRIGGER_PAD);
        for (Position const& cone : rules.cones)
            if (InCone(cone, x, y, arc))
                return WormMoveReason::Spew;
    }

    if (first == WormMoveReason::None && !skipSpread &&
        SpreadFailsAt(rules, Position(x, y, 0.0f), spread == SpreadTest::PairTolerant))
        first = WormMoveReason::Spread;

    if (first != WormMoveReason::None || heldSpot)
        return first;

    if (rules.hasStand && rules.stand.GetExactDist2d(x, y) > WORM_HOLD_TRIGGER)
        return rules.standReason;

    return rules.reachCrossesPool ? WormMoveReason::Reach : WormMoveReason::None;
}

void FillCircles(WormRules const& rules, WormMovePlan& plan)
{
    bool paired = false;
    for (WormCircleRule const& rule : rules.circles)
    {
        HazardCircle const circle(rule.centre, rule.clearance);
        if (rule.flags & RULE_HARD)
            plan.circles.push_back(circle);
        if (rule.flags & RULE_SPREAD)
        {
            plan.spreadCircles.push_back(circle);
            if (rule.flags & RULE_PAIR)
                paired = true;
            else
                plan.pairCircles.push_back(circle);
        }
        if (rule.flags & RULE_URGENT)
            plan.urgentCircles.push_back(circle);
        if (rule.flags & (RULE_URGENT | RULE_ESCAPE))
        {
            plan.escapeCircles.push_back(circle);
            if (!(rule.flags & RULE_CLEAN))
                plan.looseCircles.push_back(circle);
        }
    }

    plan.pairable = paired;
    if (!paired)
        plan.pairCircles.clear();
}

struct WormPath
{
    Position from;
    float speed = 1.0f;
    std::vector<WormPool> pools;
    std::vector<Position> sweeps;
    std::vector<Position> cones;
    float coneArc = 0.0f;
};

bool CircleBlocksWalk(Position const& from, Position const& to, Position const& centre, float radius)
{
    if (centre.GetExactDist2d(from) >= radius)
        return SegmentCrossesCircle(from, to, HazardCircle(centre, radius));

    float const outward = (to.GetPositionX() - from.GetPositionX()) * (from.GetPositionX() - centre.GetPositionX()) +
                          (to.GetPositionY() - from.GetPositionY()) * (from.GetPositionY() - centre.GetPositionY());
    return outward <= 0.0f || centre.GetExactDist2d(to) < radius;
}

float SpewSideExit(Position const& origin, float x, float y, float halfArc)
{
    float const ax = std::cos(origin.GetOrientation());
    float const ay = std::sin(origin.GetOrientation());
    float const px = x - origin.GetPositionX();
    float const py = y - origin.GetPositionY();
    float const along = px * ax + py * ay;
    if (along <= 0.0f)
        return 0.0f;

    float const side = std::fabs(px * ay - py * ax);
    float const coneEdge = std::min(along * std::tan(halfArc),
                                    std::sqrt(std::max(WORM_SPEW_REACH * WORM_SPEW_REACH - along * along, 0.0f)));
    float const discEdge =
        along < WORM_TANK_OFFSET ? std::sqrt(WORM_TANK_OFFSET * WORM_TANK_OFFSET - along * along) : 0.0f;
    return std::max(std::max(coneEdge, discEdge) - side, 0.0f);
}

bool SpewBlocksWalk(Position const& from, Position const& to, Position const& cone, float halfArc)
{
    float const fx = from.GetPositionX();
    float const fy = from.GetPositionY();
    float const length = from.GetExactDist2d(to);
    bool inside = InSpewDanger(cone, fx, fy, halfArc);
    float const budget = inside ? SpewSideExit(cone, fx, fy, halfArc) + SPEW_EXIT_SLACK : 0.0f;
    uint32 const steps = std::max<uint32>(1, static_cast<uint32>(std::ceil(length / SPEW_PATH_STEP)));
    for (uint32 step = 1; step <= steps; ++step)
    {
        float const share = static_cast<float>(step) / static_cast<float>(steps);
        bool const in = InSpewDanger(cone, fx + (to.GetPositionX() - fx) * share,
                                     fy + (to.GetPositionY() - fy) * share, halfArc);
        if (in && (!inside || length * share > budget))
            return true;

        inside = in;
    }

    return inside;
}

bool PathBlocked(WormPath const& path, Position const& from, float x, float y, bool poolsOnly)
{
    Position const to(x, y, from.GetPositionZ());
    float const walked = path.from.GetExactDist2d(from) + from.GetExactDist2d(x, y);
    uint32 const ticks = static_cast<uint32>(std::ceil(walked / path.speed));
    for (WormPool const& pool : path.pools)
        if (CircleBlocksWalk(from, to, pool.pos, PoolRadiusIn(pool, ticks) + WORM_POOL_PATH_PAD))
            return true;

    if (poolsOnly)
        return false;

    for (Position const& worm : path.sweeps)
        if (CircleBlocksWalk(from, to, worm, WORM_SWEEP_PATH_RADIUS))
            return true;

    for (Position const& cone : path.cones)
        if (SpewBlocksWalk(from, to, cone, path.coneArc))
            return true;

    return false;
}

bool PathBlocked(WormPath const& path, float x, float y, bool poolsOnly)
{
    return PathBlocked(path, path.from, x, y, poolsOnly);
}

WormPath PathFrom(Player* bot, JormungarsState const& state)
{
    WormPath path;
    path.from = bot->GetPosition();
    path.speed = std::max(bot->GetSpeed(MOVE_RUN), 1.0f);
    path.pools = state.pools;
    return path;
}

struct WormAccept
{
    WormPath path;
    std::vector<Position> cones;
    float coneArc = 0.0f;
    bool partnerReach = false;
    float partnerReachYd = WORM_CURE_REACH;
    Position partnerPos;
    bool rearArc = false;
    Position rearOf;
};

char const* AcceptFailure(WormAccept const& accept, Position const& from, float x, float y, bool escape,
                          bool skipPath)
{
    if (ARENA_CENTER.GetExactDist2d(x, y) > WORM_SPOT_FLOOR_RADIUS)
        return "path";

    for (Position const& cone : accept.cones)
        if (InSpewDanger(cone, x, y, accept.coneArc))
            return "spew";

    if (!escape)
    {
        if (accept.partnerReach && accept.partnerPos.GetExactDist2d(x, y) > accept.partnerReachYd)
            return "target";

        if (accept.rearArc && AngleOffFacing(accept.rearOf, x, y) < WORM_MELEE_REAR_ARC_DEG * DEG_TO_RAD)
            return "melee";
    }

    return !skipPath && PathBlocked(accept.path, from, x, y, false) ? "path" : nullptr;
}

char const* AcceptFailure(WormAccept const& accept, float x, float y, bool escape, bool skipPath)
{
    return AcceptFailure(accept, accept.path.from, x, y, escape, skipPath);
}

char const* SpotUnsafeCause(WormRules const& rules, WormAccept const& accept, bool escape, bool skipPath,
                            Position const& spot)
{
    float const x = spot.GetPositionX();
    float const y = spot.GetPositionY();
    for (WormCircleRule const& rule : rules.circles)
        if ((rule.flags & RULE_URGENT) && rule.centre.GetExactDist2d(x, y) < rule.clearance)
            return WormMoveReasonName(rule.reason);

    return AcceptFailure(accept, x, y, escape, skipPath);
}

bool ScatterCarrier(JormungarsState const& state, WormMember const& member)
{
    return member.bot && member.bile && !member.holder && member.role != WormRole::Tank &&
           !state.carrierOf.count(member.guid);
}

std::vector<Position> UpStationaryWorms(JormungarsState const& state)
{
    std::vector<Position> worms;
    for (WormRead const& worm : state.worms)
        if (IsUp(worm) && !worm.mobile)
            worms.push_back(worm.pos);

    return worms;
}

bool ScatterSpotSafe(JormungarsState const& state, Position const& spot, float halfArc)
{
    if (ARENA_CENTER.GetExactDist2d(spot) > WORM_SPOT_FLOOR_RADIUS)
        return false;

    for (WormPool const& pool : state.pools)
        if (pool.pos.GetExactDist2d(spot) < pool.radius + WORM_POOL_TRIGGER_PAD)
            return false;

    for (WormRead const& worm : state.worms)
    {
        if (!IsUp(worm))
            continue;

        if (worm.mobile ? worm.spewArmed && InSpewDanger(worm.pos, spot.GetPositionX(), spot.GetPositionY(), halfArc)
                        : worm.pos.GetExactDist2d(spot) < WORM_SWEEP_TRIGGER)
            return false;
    }

    return true;
}

void DealBileScatter(PlayerbotAI* botAI, Map* map, JormungarsState& state, uint32 now)
{
    float const halfArc = WormSpewHalfArc(botAI);
    for (auto dealt = state.bileScatter.begin(); dealt != state.bileScatter.end();)
    {
        WormMember const* member = FindMember(state, dealt->first);
        bool const keep = member && ScatterCarrier(state, *member) &&
                          getMSTimeDiff(dealt->second.dealtMs, now) < BILE_SCATTER_HOLD_MS &&
                          ScatterSpotSafe(state, dealt->second.spot, halfArc);
        dealt = keep ? std::next(dealt) : state.bileScatter.erase(dealt);
    }

    for (auto tried = state.bileScatterTried.begin(); tried != state.bileScatterTried.end();)
        tried = getMSTimeDiff(tried->second, now) < BILE_SCATTER_FRESH_MS ? std::next(tried)
                                                                          : state.bileScatterTried.erase(tried);

    std::vector<WormMember const*> fresh;
    for (WormMember const& member : state.roster)
        if (ScatterCarrier(state, member) && member.bileAgeMs < BILE_SCATTER_FRESH_MS &&
            !state.bileScatter.count(member.guid) && !state.bileScatterTried.count(member.guid))
            fresh.push_back(&member);

    if (fresh.empty())
        return;

    std::sort(fresh.begin(), fresh.end(),
              [](WormMember const* lhs, WormMember const* rhs) { return lhs->guid < rhs->guid; });

    std::vector<Position> const sweeps = UpStationaryWorms(state);
    std::vector<ObjectGuid> pending;
    for (WormMember const* member : fresh)
        pending.push_back(member->guid);

    for (WormMember const* member : fresh)
    {
        pending.erase(std::remove(pending.begin(), pending.end(), member->guid), pending.end());
        std::vector<HazardCircle> circles;
        for (WormMember const& other : state.roster)
            if (other.bile && other.guid != member->guid &&
                std::find(pending.begin(), pending.end(), other.guid) == pending.end())
                circles.emplace_back(HeldSpotOf(state, other, now), WORM_BILE_CLEARANCE);

        bool const shares = std::any_of(state.roster.begin(), state.roster.end(), [&](WormMember const& other)
                                        {
                                            return other.bile && other.guid != member->guid &&
                                                   other.pos.GetExactDist2d(member->pos) < WORM_BILE_CLEARANCE;
                                        });
        Player* player = shares ? ObjectAccessor::GetPlayer(map, member->guid) : nullptr;
        PlayerbotAI* memberAI = player ? GET_PLAYERBOT_AI(player) : nullptr;
        if (!memberAI)
            continue;

        for (WormPool const& pool : state.pools)
            circles.emplace_back(pool.pos, pool.radiusSoon + WORM_POOL_CLEARANCE_PAD);
        for (Position const& worm : sweeps)
            circles.emplace_back(worm, WORM_SWEEP_CLEARANCE);

        std::vector<HazardCircle> withClean = circles;
        for (WormMember const& other : state.roster)
            if (other.guid != member->guid && !other.toxin && !other.bile)
                withClean.emplace_back(HeldSpotOf(state, other, now), WORM_BILE_CLEARANCE);

        WormAccept accept;
        accept.path = PathFrom(player, state);
        accept.path.sweeps = sweeps;
        for (WormRead const& worm : state.worms)
            if (IsUp(worm) && worm.mobile && worm.spewArmed)
                accept.cones.push_back(worm.pos);
        accept.coneArc = halfArc + WORM_SPEW_CLEAR_PAD;
        accept.path.cones = accept.cones;
        accept.path.coneArc = accept.coneArc;
        auto const safe = [&accept](float x, float y) { return AcceptFailure(accept, x, y, true, false) == nullptr; };

        bool const clearHere =
            std::none_of(withClean.begin(), withClean.end(), [member](HazardCircle const& circle)
                         { return circle.first.GetExactDist2d(member->pos) < circle.second; });
        if (clearHere && safe(member->pos.GetPositionX(), member->pos.GetPositionY()))
        {
            state.bileScatter[member->guid] = {member->pos, now};
            continue;
        }

        std::function<bool(float, float)> inReach;
        std::vector<HazardCircle> const healers =
            member->role == WormRole::Heal ? std::vector<HazardCircle>() : HealerReach(memberAI, player, state, now, 0.0f);
        Unit* target = memberAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        if (member->role == WormRole::Ranged && target && target->IsAlive())
        {
            Position const at = target->GetPosition();
            float const range =
                memberAI->GetRange("spell") + player->GetCombatReach() + target->GetCombatReach() - 1.0f;
            inReach = [at, range, healers](float x, float y)
            { return at.GetExactDist2d(x, y) <= range && InHealerReach(healers, x, y); };
        }
        else if (!healers.empty())
            inReach = [healers](float x, float y) { return InHealerReach(healers, x, y); };

        HazardSweepCache sweep;
        Position const prefer = Jittered(member->pos, member->guid);
        if (circles.empty())
            circles.emplace_back(member->pos, 0.0f);

        auto const search = [&](std::vector<HazardCircle> const& hazards, std::function<bool(float, float)> const& accepts)
        {
            return FindNearestPositionClearOfHazards(player, hazards, WORM_SEARCH_RADIUS, 2.0f,
                                                     static_cast<float>(M_PI) / 8.0f, &prefer, accepts, &sweep);
        };
        auto const reachable = [&](float x, float y) { return safe(x, y) && (!inReach || inReach(x, y)); };

        Position spot = search(withClean, reachable);
        if (spot == Position() && inReach)
            spot = search(withClean, safe);
        if (spot == Position())
            spot = search(circles, reachable);
        if (spot == Position() && inReach)
            spot = search(circles, safe);

        if (spot != Position())
            state.bileScatter[member->guid] = {spot, now};
        else
            state.bileScatterTried[member->guid] = now;
    }
}

enum class ReachHazard : uint8
{
    None,
    Crosses,
    EndsIn
};

bool GenericReachStop(PlayerbotAI* botAI, Player* bot, WormRole role, Unit* target, Position& stop, float& walk)
{
    float reach = 0.0f;
    if (role == WormRole::Melee || role == WormRole::Tank)
    {
        if (bot->IsWithinMeleeRange(target))
            return false;

        reach = bot->GetMeleeRange(target) - 1.0f;
    }
    else
    {
        float const range = botAI->GetRange("spell") + CONTACT_DISTANCE;
        if (bot->IsWithinCombatRange(target, range))
            return false;

        reach = range + bot->GetCombatReach() + target->GetCombatReach() - 1.0f;
    }

    float const dist = target->GetExactDist2d(bot);
    if (dist <= reach)
        return false;

    float const share = reach / dist;
    stop = Position(target->GetPositionX() + (bot->GetPositionX() - target->GetPositionX()) * share,
                    target->GetPositionY() + (bot->GetPositionY() - target->GetPositionY()) * share,
                    bot->GetPositionZ());
    walk = dist - reach;
    return true;
}

ReachHazard ReachThroughHazard(PlayerbotAI* botAI, Player* bot, JormungarsState const& state, WormRole role,
                               Unit* target, BeastsTankDuty duty, bool heldOut, uint32 now)
{
    if (duty != BeastsTankDuty::None || !target || !target->IsAlive())
        return ReachHazard::None;

    if ((role == WormRole::Melee || role == WormRole::Tank) && heldOut)
        return ReachHazard::None;

    Position stop;
    float walk = 0.0f;
    if (!GenericReachStop(botAI, bot, role, target, stop, walk) || walk > WORM_SEARCH_RADIUS)
        return ReachHazard::None;

    float const x = stop.GetPositionX();
    float const y = stop.GetPositionY();
    for (WormPool const& pool : state.pools)
        if (pool.pos.GetExactDist2d(x, y) < pool.radiusSoon + WORM_POOL_TRIGGER_PAD)
            return ReachHazard::EndsIn;

    WormMember const* me = FindMember(state, bot->GetGUID());
    if (me && !me->toxin && !me->bile)
        for (WormMember const& member : state.roster)
            if (member.bile && member.guid != me->guid &&
                HeldSpotOf(state, member, now).GetExactDist2d(x, y) < WORM_BILE_TRIGGER)
                return ReachHazard::EndsIn;

    return PathBlocked(PathFrom(bot, state), x, y, true) ? ReachHazard::Crosses : ReachHazard::None;
}

std::function<bool(float, float)> RoleReach(PlayerbotAI* botAI, Player* bot, JormungarsState const& state,
                                            WormRole role, Unit* target, HazardCircle const* healReach, bool heldOut,
                                            float slack)
{
    switch (role)
    {
        case WormRole::Heal:
        {
            std::vector<HazardCircle> reach;
            float const heal = botAI->GetRange("heal") + bot->GetCombatReach() - 1.0f + slack;
            for (BeastsTankDuty duty : {BeastsTankDuty::WormMobile, BeastsTankDuty::WormStationary})
            {
                Player* holder = GetBeastsDutyHolder(botAI, duty);
                if (holder && holder->IsAlive() && holder != bot)
                    reach.emplace_back(holder->GetPosition(), heal + holder->GetCombatReach());
            }

            if (healReach)
                reach.emplace_back(healReach->first, healReach->second + slack);

            return [reach](float x, float y)
            {
                return std::all_of(reach.begin(), reach.end(), [x, y](HazardCircle const& circle)
                                   { return circle.first.GetExactDist2d(x, y) <= circle.second; });
            };
        }
        case WormRole::Ranged:
            if (target && target->IsAlive())
            {
                Position const at = target->GetPosition();
                float const range =
                    botAI->GetRange("spell") + bot->GetCombatReach() + target->GetCombatReach() - 1.0f + slack;
                std::vector<HazardCircle> floors;
                if (bot->getClass() == CLASS_HUNTER)
                    for (WormRead const& worm : state.worms)
                        if (IsUp(worm))
                            floors.emplace_back(worm.pos, (worm.mobile ? WORM_MOBILE_MELEE_RANGE
                                                                       : WORM_STATIONARY_MELEE_RANGE) +
                                                              HUNTER_MIN_RANGE_PAD);

                std::vector<HazardCircle> const healers =
                    heldOut ? HealerReach(botAI, bot, state, getMSTime(), slack) : std::vector<HazardCircle>();
                return [at, range, floors, healers](float x, float y)
                {
                    if (at.GetExactDist2d(x, y) > range || !InHealerReach(healers, x, y))
                        return false;

                    return std::none_of(floors.begin(), floors.end(), [x, y](HazardCircle const& floor)
                                        { return floor.first.GetExactDist2d(x, y) < floor.second; });
                };
            }
            break;
        case WormRole::Melee:
        case WormRole::Tank:
            if (heldOut)
            {
                std::vector<HazardCircle> const healers = HealerReach(botAI, bot, state, getMSTime(), slack);
                return [healers](float x, float y) { return InHealerReach(healers, x, y); };
            }

            if (target && target->IsAlive() && !(IsWormEntry(target) && IsWormSubmerged(target)))
            {
                Position const at = target->GetPosition();
                float const reach =
                    (IsWormEntry(target) ? WormMeleeRange(target) : bot->GetMeleeRange(target)) - 1.0f + slack;
                return [at, reach](float x, float y) { return at.GetExactDist2d(x, y) <= reach; };
            }
            break;
    }

    return [](float, float) { return true; };
}

bool AnchorDrifted(JormungarsState const& state, WormHold const& hold)
{
    if (hold.anchor.IsEmpty())
        return false;

    WormRead const* worm = FindWorm(state, hold.anchor);
    if (!worm || !IsUp(*worm))
        return true;

    if (!worm->settled)
        return false;

    if (worm->pos.GetExactDist2d(hold.anchorPos) > (hold.melee ? WORM_HOLD_MELEE_DRIFT : WORM_HOLD_DRIFT))
        return true;

    float const turn = TurnBetween(hold.anchorPos.GetOrientation(), worm->pos.GetOrientation());
    return hold.melee && turn > WORM_HOLD_MELEE_TURN_DEG * DEG_TO_RAD;
}

void ReleaseHold(Player* bot, JormungarsState& state, char const* cause)
{
    state.holds.erase(bot->GetGUID());
    NoteHold(bot, std::string("release ") + cause);
}

struct ToxinHealState
{
    uint32 memoMs = 0;
    bool memoValid = false;
    bool open = false;
    bool cureRuns = false;
    RaidObs::ObsGuidMap<ObjectGuid> carrierOf{"nb.toxinheal"};
};

RaidInstanceState<ToxinHealState> toxinHealStates;

bool HasPlayer(std::vector<Player*> const& players, ObjectGuid const& guid)
{
    return std::any_of(players.begin(), players.end(), [&guid](Player* player) { return player->GetGUID() == guid; });
}

ToxinHealState* RefreshToxinHeals(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    ToxinHealState& state = toxinHealStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;
    state.open = ToCEncounterGateOpen(botAI, ToCEncounter::NorthrendBeasts);

    std::vector<Player*> carriers;
    std::vector<Player*> healers;
    bool bile = false;
    Group* group = bot->GetGroup();
    if (group && state.open)
    {
        uint32 const toxinId = sSpellMgr->GetSpellIdForDifficulty(SPELL_PARALYTIC_TOXIN, bot);
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld() || !member->IsAlive() ||
                member->GetMapId() != TRIAL_OF_THE_CRUSADER_MAP_ID || member->GetInstanceId() != bot->GetInstanceId())
                continue;

            bile = bile || member->HasAura(SPELL_BURNING_BILE);
            if (member->HasAura(toxinId))
                carriers.push_back(member);
            else if (PlayerbotAI::IsHeal(member) && GET_PLAYERBOT_AI(member))
                healers.push_back(member);
        }
    }

    JormungarsState const* worms = Refresh(botAI);
    state.cureRuns = bile && worms && worms->engaged;
    if (state.cureRuns)
        carriers.clear();

    std::vector<ObjectGuid> ended;
    for (auto const& duty : state.carrierOf)
        if (!HasPlayer(healers, duty.first) || !HasPlayer(carriers, duty.second))
            ended.push_back(duty.first);

    for (ObjectGuid const& healer : ended)
        state.carrierOf.erase(healer);

    std::sort(carriers.begin(), carriers.end(),
              [](Player* lhs, Player* rhs) { return lhs->GetGUID() < rhs->GetGUID(); });
    for (Player* carrier : carriers)
    {
        ObjectGuid const carrierGuid = carrier->GetGUID();
        if (std::any_of(state.carrierOf.begin(), state.carrierOf.end(),
                        [&carrierGuid](auto const& duty) { return duty.second == carrierGuid; }))
            continue;

        Player* nearest = nullptr;
        float nearestDist = 0.0f;
        for (Player* healer : healers)
        {
            if (state.carrierOf.count(healer->GetGUID()))
                continue;

            float const dist = healer->GetExactDist(carrier);
            if (!nearest || dist < nearestDist || (!(nearestDist < dist) && healer->GetGUID() < nearest->GetGUID()))
            {
                nearest = healer;
                nearestDist = dist;
            }
        }

        if (nearest)
            state.carrierOf[nearest->GetGUID()] = carrierGuid;
    }

    return &state;
}
}

bool IsWormSubmerged(Unit* worm) { return worm && worm->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE); }

bool IsWormMobile(Unit* worm)
{
    if (!worm)
        return false;

    if (!IsWormSubmerged(worm))
        return ShowsMobile(worm);

    if (Unit* other = OtherLivingWorm(worm))
        if (!IsWormSubmerged(other))
            return !ShowsMobile(other);

    return !ShowsMobile(worm);
}

float WormMeleeRange(Unit* worm) { return IsWormMobile(worm) ? WORM_MOBILE_MELEE_RANGE : WORM_STATIONARY_MELEE_RANGE; }

float WormSpewHalfArc(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    return HalfArcOf(bot ? sSpellMgr->GetSpellIdForDifficulty(SPELL_ACIDIC_SPEW_TICK, bot) : SPELL_ACIDIC_SPEW_TICK);
}

bool IsWormSpewing(PlayerbotAI* botAI, Unit* worm)
{
    JormungarsState* state = worm ? Refresh(botAI) : nullptr;
    WormRead const* read = state ? FindWorm(*state, worm->GetGUID()) : nullptr;
    return read && read->spewing;
}

bool IsWormSpewingAtOtherTank(PlayerbotAI* botAI, Unit* worm)
{
    Unit* victim = worm ? worm->GetVictim() : nullptr;
    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    return victimPlayer && victimPlayer != botAI->GetBot() && IsBeastsTank(victimPlayer) &&
           IsWormSpewing(botAI, worm);
}

bool GetMobileWormDrag(PlayerbotAI* botAI, Unit* worm, WormDragPlan& plan)
{
    plan = WormDragPlan();
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !worm || !bot->IsAlive())
        return false;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return false;

    if (GetBeastsTankDuty(botAI) != BeastsTankDuty::WormMobile ||
        GetBeastOfDuty(botAI, BeastsTankDuty::WormMobile) != worm)
        return false;

    if (IsWormSubmerged(worm) || worm->GetVictim() != bot || !IsWormMobile(worm))
        return false;

    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return false;

    WormRead const* stationary = nullptr;
    for (WormRead const& other : state->worms)
        if (IsUp(other) && !other.mobile && other.guid != worm->GetGUID())
            stationary = &other;

    bool needed = stationary && worm->GetExactDist2d(stationary->pos) < WORM_SWEEP_CLEARANCE + WORM_MELEE_RING;
    for (WormPool const& pool : state->pools)
        if (worm->GetExactDist2d(pool.pos) < pool.radiusSoon + WORM_MELEE_RING)
            needed = true;

    Map* map = bot->FindMap();
    plan.edge = map && map->IsHeroic();
    if (plan.edge && ARENA_CENTER.GetExactDist2d(worm->GetPositionX(), worm->GetPositionY()) < WORM_EDGE_WORM_MIN)
        needed = true;

    if (!needed)
    {
        state->dragNeededSince.erase(bot->GetGUID());
        return false;
    }

    for (WormPool const& pool : state->pools)
    {
        float const clearance = DragPoolClearance(pool);
        plan.hazards.emplace_back(pool.pos, clearance);
        plan.poolsOnly.emplace_back(pool.pos, clearance);
    }

    if (stationary)
        plan.hazards.emplace_back(stationary->pos, DRAG_STATIONARY_CLEARANCE);

    float const dx = bot->GetPositionX() - worm->GetPositionX();
    float const dy = bot->GetPositionY() - worm->GetPositionY();
    float const heading = dx * dx + dy * dy > 0.01f ? std::atan2(dy, dx) : bot->GetOrientation();
    plan.preferNear = Position(bot->GetPositionX() + DRAG_LEAD * std::cos(heading),
                               bot->GetPositionY() + DRAG_LEAD * std::sin(heading), bot->GetPositionZ());
    return true;
}

bool MobileWormDragWaits(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return false;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return false;

    ObjectGuid const self = bot->GetGUID();
    uint32 const now = getMSTime();
    auto const since = state->dragNeededSince.try_emplace(self, now).first;
    if (getMSTimeDiff(since->second, now) >= WORM_DRAG_CURE_WAIT_MS)
        return false;

    WormMember const* me = FindMember(*state, self);
    if (!me || !me->bile)
        return false;

    for (WormMember const& walker : state->roster)
    {
        if (!walker.toxin || walker.bile || walker.stuck || walker.pos.GetExactDist(me->pos) <= WORM_CURE_TRIGGER)
            continue;

        WormMember const* nearest = nullptr;
        float nearestDist = 0.0f;
        for (WormMember const& carrier : state->roster)
        {
            if (!carrier.bile || carrier.guid == walker.guid || !CureReachable(*state, walker, carrier))
                continue;

            float const dist = walker.pos.GetExactDist(carrier.pos);
            if (!nearest || dist < nearestDist)
            {
                nearest = &carrier;
                nearestDist = dist;
            }
        }

        if (nearest == me)
            return true;
    }

    return false;
}

Unit* GetWormRedirectTarget(PlayerbotAI* botAI, Player*& holder)
{
    holder = nullptr;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive() || (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE))
        return nullptr;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return nullptr;

    bool const hunter = bot->getClass() == CLASS_HUNTER;
    int32 const index = hunter ? HunterIndex(bot, *state) : 0;
    if (index < 0)
        return nullptr;

    BeastsTankDuty duty = index % 2 == 0 ? BeastsTankDuty::WormMobile : BeastsTankDuty::WormStationary;
    Player* dutyHolder = GetBeastsDutyHolder(botAI, duty);
    Unit* worm = GetBeastOfDuty(botAI, duty);
    if (duty == BeastsTankDuty::WormStationary && (!dutyHolder || !worm))
    {
        duty = BeastsTankDuty::WormMobile;
        dutyHolder = GetBeastsDutyHolder(botAI, duty);
        worm = GetBeastOfDuty(botAI, duty);
    }

    if (!dutyHolder || !worm || !dutyHolder->IsAlive() || dutyHolder == bot)
        return nullptr;

    bool const chargesLeft = hunter && bot->HasAura(SPELL_MISDIRECTION_PROC);
    if (!IsWormSubmerged(worm) && worm->GetVictim() == dutyHolder && !chargesLeft)
        return nullptr;

    holder = dutyHolder;
    return worm;
}

namespace
{
uint8 WormBrakeBits(PlayerbotAI* botAI, JormungarsState*& state)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    state = bot ? Refresh(botAI) : nullptr;
    if (!state || !state->engaged || !IsRaidDpsBot(bot))
        return 0;

    ObjectGuid const guid = bot->GetGUID();
    auto const held = state->brake.find(guid);
    uint8 const bits = held != state->brake.end() ? held->second : 0;
    uint32 const now = getMSTime();
    auto const stamp = state->brakeReadMs.find(guid);
    if (stamp != state->brakeReadMs.end() && stamp->second == now)
        return bits;

    state->brakeReadMs[guid] = now;
    Map* map = bot->GetMap();
    uint8 next = 0;
    for (size_t slot = 0; bot->IsAlive() && slot < SLOT_COUNT; ++slot)
    {
        Creature* worm = IsUp(state->worms[slot]) ? map->GetCreature(state->worms[slot].guid) : nullptr;
        Player* holder = worm ? GetBeastsDutyHolder(botAI, GetDutyOfBeast(botAI, worm)) : nullptr;
        if (!holder || holder == bot)
            continue;

        uint8 const bit = static_cast<uint8>(1u << slot);
        float const top = worm->GetThreatMgr().GetThreat(holder);
        float const own = worm->GetThreatMgr().GetThreat(bot);
        float const share = (bits & bit) ? WORM_THREAT_RELEASE : WORM_THREAT_CAP;
        if (top > 0.0f && own > 0.0f && own >= share * top)
            next |= bit;
    }

    if (next != bits)
        state->brake.Set(guid, next);

    return next;
}
}

bool IsWormThreatBraked(PlayerbotAI* botAI, Unit* unit)
{
    if (!IsWormEntry(unit))
        return false;

    JormungarsState* state = nullptr;
    uint8 const bits = WormBrakeBits(botAI, state);
    for (size_t slot = 0; bits && slot < SLOT_COUNT; ++slot)
        if ((bits & (1u << slot)) && state->worms[slot].guid == unit->GetGUID())
            return true;

    return false;
}

bool IsWormThreatBraking(PlayerbotAI* botAI)
{
    JormungarsState* state = nullptr;
    return WormBrakeBits(botAI, state) != 0;
}

bool IsWormThreatBrakedNear(PlayerbotAI* botAI, Position const& at, float range)
{
    JormungarsState* state = nullptr;
    uint8 const bits = WormBrakeBits(botAI, state);
    for (size_t slot = 0; bits && slot < SLOT_COUNT; ++slot)
        if ((bits & (1u << slot)) && state->worms[slot].pos.GetExactDist2d(at) <= range)
            return true;

    return false;
}

bool IsWormBileCarrierHeldOut(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return false;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return false;

    WormMember const* me = FindMember(*state, bot->GetGUID());
    return me && me->bile && GetBeastsTankDuty(botAI) == BeastsTankDuty::None;
}

char const* WormMoveReasonName(WormMoveReason reason)
{
    switch (reason)
    {
        case WormMoveReason::Cure:
            return "cure";
        case WormMoveReason::Run:
            return "run";
        case WormMoveReason::Pool:
            return "pool";
        case WormMoveReason::Spray:
            return "spray";
        case WormMoveReason::Bile:
            return "bile";
        case WormMoveReason::Sweep:
            return "sweep";
        case WormMoveReason::Clump:
            return "clump";
        case WormMoveReason::Spew:
            return "spew";
        case WormMoveReason::Spread:
            return "spread";
        case WormMoveReason::Melee:
            return "melee";
        case WormMoveReason::Reach:
            return "reach";
        default:
            return "none";
    }
}

char const* WormSpotTierName(WormSpotTier tier)
{
    switch (tier)
    {
        case WormSpotTier::Stand:
            return "stand";
        case WormSpotTier::Full:
            return "full";
        case WormSpotTier::Pair:
            return "pair";
        case WormSpotTier::NoSpread:
            return "nospread";
        case WormSpotTier::Partner:
            return "partner";
        case WormSpotTier::Direct:
            return "direct";
        case WormSpotTier::Detour:
            return "detour";
        case WormSpotTier::Partial:
            return "partial";
        case WormSpotTier::Escape:
            return "escape";
        case WormSpotTier::Loose:
            return "loose";
        case WormSpotTier::Cross:
            return "cross";
    }

    return "full";
}

bool WormTierParks(WormSpotTier tier)
{
    return tier != WormSpotTier::Detour && tier != WormSpotTier::Partial && tier != WormSpotTier::Escape &&
           tier != WormSpotTier::Loose && tier != WormSpotTier::Cross;
}

bool WormHoldParks(WormMoveReason reason, WormSpotTier tier)
{
    return WormTierParks(tier) && reason != WormMoveReason::Spray;
}

bool IsWormCureReason(WormMoveReason reason)
{
    return reason == WormMoveReason::Cure || reason == WormMoveReason::Run;
}

bool GetWormMovePlan(PlayerbotAI* botAI, WormMovePlan& plan)
{
    plan = WormMovePlan();
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return false;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->live)
        return false;

    if (!state->engaged)
    {
        NoteCure(bot, "none");
        return false;
    }

    uint32 const now = getMSTime();
    ObjectGuid const self = bot->GetGUID();
    WormMember const* me = FindMember(*state, self);
    bool const toxin = me && me->toxin;
    bool const bile = me && me->bile;
    bool const stuck = me && me->stuck;
    BeastsTankDuty const duty = GetBeastsTankDuty(botAI);

    CurePairing const pairing = PairingOf(*state, bot, me, duty);
    WormMember const* carrier = pairing.carrier;
    if (carrier)
        NoteCure(bot, "run", carrier->guid);
    else if (pairing.nearestBile)
        NoteCure(bot, "seek", pairing.nearestBile->guid);
    else if (stuck)
    {
        auto const runner = state->runnerOf.find(self);
        NoteCure(bot, "wait", runner != state->runnerOf.end() ? runner->second : ObjectGuid::Empty);
    }
    else
        NoteCure(bot, "none");

    bool const holder = duty == BeastsTankDuty::WormMobile || duty == BeastsTankDuty::WormStationary;
    if (holder && !carrier)
    {
        if (state->holds.count(self))
            ReleaseHold(bot, *state, "target");

        return false;
    }

    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return false;

    WormMember const* partner = carrier ? carrier : pairing.nearestBile;
    bool const runsFar = carrier && RunsFar(*carrier);
    float const partnerTrigger = runsFar ? WORM_RUN_TRIGGER : WORM_CURE_TRIGGER;
    bool const cureWalk = partner && bot->GetExactDist(partner->pos) > partnerTrigger;
    if (partner)
    {
        plan.partner = partner->guid;
        plan.partnerPos = partner->pos;
        plan.partnerReach = runsFar ? WORM_RUN_REACH : WORM_CURE_REACH;
    }

    float const bx = bot->GetPositionX();
    float const by = bot->GetPositionY();
    float const halfArc = WormSpewHalfArc(botAI);
    plan.urgent = UrgentAt(*state, self, bx, by, toxin, duty == BeastsTankDuty::WormStationary, !cureWalk, halfArc,
                           PinnedCastRemainingMs(bot));

    WormRole const role = me ? me->role : RoleOf(bot);
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    ObjectGuid const targetGuid = target ? target->GetGUID() : ObjectGuid::Empty;
    bool const heldOut = HeldOut(*state, me, duty);
    bool const caster = IsCaster(role);
    WormRead const* mobileUp = UpWorm(*state, true);
    WormRead const* stationaryUp = UpWorm(*state, false);

    WormRules rules;
    rules.halfArc = halfArc;
    rules.spewFires = !cureWalk;
    rules.hasPartner = partner != nullptr;
    rules.partnerTrigger = partnerTrigger;
    if (partner)
        rules.partnerPos = partner->pos;

    for (WormPool const& pool : state->pools)
    {
        rules.circles.push_back({WormMoveReason::Pool, pool.pos, pool.radius + WORM_POOL_TRIGGER_PAD,
                                 pool.radiusSoon + WORM_POOL_CLEARANCE_PAD, RULE_HARD | RULE_URGENT});
        if (pool.pos.GetExactDist2d(bx, by) < pool.radius)
            plan.escapeFirst = true;
    }

    WormSpray const& spray = state->spray;
    if (!cureWalk && !spray.target.IsEmpty())
    {
        if (spray.target != self)
            rules.circles.push_back({WormMoveReason::Spray, spray.targetPos, WORM_SPRAY_TRIGGER, WORM_SPRAY_CLEARANCE,
                                     RULE_HARD | RULE_URGENT});
        else
            for (WormMember const& member : state->roster)
                if (member.guid != self && member.pos.GetExactDist2d(spray.targetPos) < 2.0f * WORM_SPRAY_TRIGGER)
                    rules.circles.push_back({WormMoveReason::Spray, member.pos, WORM_SPRAY_TRIGGER,
                                             WORM_SPRAY_CLEARANCE, RULE_HARD | RULE_URGENT});
    }

    if (!toxin)
    {
        for (WormMember const& member : state->roster)
        {
            if (!member.bile || member.guid == self)
                continue;

            Position const at = HeldSpotOf(*state, member, now);
            if (cureWalk)
                rules.circles.push_back({WormMoveReason::None, at, 0.0f, WORM_BILE_CLEARANCE, RULE_ESCAPE});
            else
                rules.circles.push_back(
                    {WormMoveReason::Bile, at, WORM_BILE_TRIGGER, WORM_BILE_CLEARANCE, RULE_HARD | RULE_URGENT});
        }
    }

    if (heldOut)
    {
        for (WormMember const& member : state->roster)
        {
            if (member.guid == self || member.toxin || member.bile)
                continue;

            rules.circles.push_back({WormMoveReason::Bile, HeldSpotOf(*state, member, now), WORM_BILE_TRIGGER,
                                     WORM_BILE_CLEARANCE, RULE_HARD | RULE_ESCAPE | RULE_CLEAN});
        }
    }

    bool const sweepOnlyWhileCast = role == WormRole::Melee && target && target->IsAlive() &&
                                    (!IsWormEntry(target) || !IsWormMobile(target));
    std::vector<Position> sweeps;
    if (duty != BeastsTankDuty::WormStationary)
    {
        for (WormRead const& worm : state->worms)
        {
            if (!IsUp(worm) || worm.mobile)
                continue;

            sweeps.push_back(worm.pos);
            bool const fires = !cureWalk && (worm.sweeping || !sweepOnlyWhileCast);
            rules.circles.push_back({WormMoveReason::Sweep, worm.pos, fires ? WORM_SWEEP_TRIGGER : 0.0f,
                                     WORM_SWEEP_CLEARANCE,
                                     static_cast<uint8>(RULE_HARD | (worm.sweeping ? RULE_URGENT : 0))});
        }
    }

    for (WormRead const& worm : state->worms)
        if (IsUp(worm) && worm.mobile && worm.spewArmed)
            rules.cones.push_back(worm.pos);

    if (!cureWalk && caster && !toxin && !bile && stationaryUp)
    {
        rules.self = self;
        for (WormMember const& member : state->roster)
            if (member.guid != self && IsCaster(member.role) && !member.toxin && !member.bile)
                rules.spreadPeers.push_back({member.guid, HeldSpotOf(*state, member, now), member.bot});

        WormSpreadPeer const* pairPartner = PairPartnerOf(rules, bot->GetPosition());
        for (WormSpreadPeer const& peer : rules.spreadPeers)
            rules.circles.push_back({WormMoveReason::Spread, peer.spot, 0.0f, WORM_SPREAD_CLEARANCE,
                                     static_cast<uint8>(RULE_SPREAD | (&peer == pairPartner ? RULE_PAIR : 0))});

        if (mobileUp)
            rules.circles.push_back({WormMoveReason::Spread, mobileUp->pos, WORM_MELEE_STACK_TRIGGER,
                                     WORM_MELEE_STACK_CLEARANCE, RULE_SPREAD});
    }

    if (!cureWalk && caster && !toxin && !bile && mobileUp)
        rules.circles.push_back({WormMoveReason::Clump, mobileUp->pos, WORM_MELEE_RING, WORM_CASTER_CLUMP_CLEARANCE,
                                 RULE_HARD});

    if (!cureWalk && caster)
        if (Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok))
            if (!GetGormokForSnoboldCarrier(botAI))
                rules.circles.push_back(
                    {WormMoveReason::None, gormok->GetPosition(), 0.0f, GORMOK_STOMP_CLEARANCE, RULE_HARD});

    rules.circles.push_back({WormMoveReason::None, bot->GetPosition(), 0.0f, FEET_CLEARANCE, RULE_HARD});

    bool const meleeStand =
        role == WormRole::Melee && !toxin && !bile && mobileUp && mobileUp->settled && targetGuid == mobileUp->guid;
    if (meleeStand)
    {
        rules.hasStand = true;
        rules.stand = MeleeStand(*state, self, mobileUp->pos);
        plan.hasStand = true;
        plan.stand = rules.stand;
    }
    else if (heldOut)
    {
        auto const scatter = state->bileScatter.find(self);
        if (scatter != state->bileScatter.end())
        {
            rules.hasStand = true;
            rules.stand = scatter->second.spot;
            rules.standReason = WormMoveReason::Bile;
            plan.hasStand = true;
            plan.stand = rules.stand;
        }
    }

    rules.reachCrossesPool =
        !cureWalk && ReachThroughHazard(botAI, bot, *state, role, target, duty, heldOut, now) != ReachHazard::None;

    FillCircles(rules, plan);
    Map* map = bot->FindMap();
    plan.spreadStrict =
        map && map->IsHeroic() && stationaryUp && stationaryUp->guid == state->worms[SLOT_DREADSCALE].guid;

    auto const accept = std::make_shared<WormAccept>();
    accept->path = PathFrom(bot, *state);
    accept->path.sweeps = sweeps;
    accept->cones = rules.cones;
    accept->coneArc = cureWalk ? halfArc : halfArc + WORM_SPEW_CLEAR_PAD;
    for (WormRead const& worm : state->worms)
        if (IsUp(worm) && worm.mobile && (cureWalk ? worm.spewing : worm.spewArmed))
            accept->path.cones.push_back(worm.pos);
    accept->path.coneArc = accept->coneArc;
    accept->partnerReach = cureWalk;
    accept->partnerReachYd = plan.partnerReach;
    accept->partnerPos = plan.partnerPos;
    accept->rearArc = meleeStand;
    if (meleeStand)
        accept->rearOf = mobileUp->pos;

    std::shared_ptr<WormAccept const> const model = accept;
    plan.accept = [model](float x, float y) { return AcceptFailure(*model, x, y, false, false) == nullptr; };
    plan.acceptVia = [model](Position const& via, float x, float y)
    { return AcceptFailure(*model, via, x, y, false, false) == nullptr; };
    plan.escapeAccept = [model](float x, float y) { return AcceptFailure(*model, x, y, true, false) == nullptr; };
    plan.crossAccept = [model](float x, float y) { return AcceptFailure(*model, x, y, true, true) == nullptr; };

    HazardCircle healReach;
    bool hasHealReach = false;
    if (role == WormRole::Heal)
    {
        Unit* healTarget = botAI->GetAiObjectContext()->GetValue<Unit*>("party member to heal")->Get();
        if (healTarget && healTarget != bot && healTarget->IsAlive())
        {
            healReach = HazardCircle(healTarget->GetPosition(), botAI->GetRange("heal") + bot->GetCombatReach() +
                                                                    healTarget->GetCombatReach() - 1.0f);
            hasHealReach = true;
        }
    }

    HazardCircle const* healCircle = hasHealReach ? &healReach : nullptr;
    plan.roleAccept = RoleReach(botAI, bot, *state, role, target, healCircle, heldOut, REACH_SEARCH_SLACK);
    std::function<bool(float, float)> const roleHold =
        RoleReach(botAI, bot, *state, role, target, healCircle, heldOut, REACH_HOLD_SLACK);

    if (cureWalk)
        plan.preferNear = partner->pos;
    else if (heldOut)
        plan.preferNear = Jittered(bot->GetPosition(), self);
    else if (rules.hasStand)
        plan.preferNear = rules.stand;
    else if (role == WormRole::Melee && IsWormEntry(target) && target->IsAlive() && !IsWormSubmerged(target))
    {
        float const facing = target->GetOrientation();
        plan.preferNear = Position(target->GetPositionX() - MELEE_BEHIND * std::cos(facing),
                                   target->GetPositionY() - MELEE_BEHIND * std::sin(facing), target->GetPositionZ());
    }
    else
        plan.preferNear = Jittered(bot->GetPosition(), self);

    auto const held = state->holds.find(self);
    if (held != state->holds.end())
    {
        WormHold& hold = held->second;
        WormHoldView& view = hold.view;
        bool const walkOnly = !WormHoldParks(view.reason, view.tier);
        bool const cureHold = IsWormCureReason(view.reason);
        bool const partnerTier = view.tier == WormSpotTier::Partner || view.tier == WormSpotTier::Direct;
        float const dist = bot->GetExactDist2d(view.spot);

        auto const walkingFailure = [&]()
        {
            return SpotUnsafeCause(rules, *accept, plan.urgent || walkOnly, view.tier == WormSpotTier::Cross,
                                   view.spot);
        };

        auto const parkedFailure = [&]() -> char const*
        {
            float const x = view.spot.GetPositionX();
            float const y = view.spot.GetPositionY();
            SpreadTest spread = SpreadTest::Strict;
            if (partnerTier || view.tier == WormSpotTier::NoSpread)
                spread = SpreadTest::Skip;
            else if (view.tier == WormSpotTier::Pair)
                spread = SpreadTest::PairTolerant;

            WormMoveReason const failed = FirstFailedRule(rules, x, y, true, spread);
            if (failed != WormMoveReason::None)
                return WormMoveReasonName(failed);

            if (cureHold && (!partner || partner->pos.GetExactDist2d(x, y) > partnerTrigger))
                return "target";

            return partnerTier || roleHold(x, y) ? nullptr : "reach";
        };

        char const* cause = nullptr;
        if ((view.tier == WormSpotTier::Detour || view.tier == WormSpotTier::Partial) &&
            (!partner || partner->guid != hold.partner))
            cause = "target";
        else if (!walkOnly)
        {
            if (cureHold)
            {
                if (!partner || partner->guid != hold.partner)
                    cause = "target";
            }
            else if (cureWalk)
                cause = "cure";
            else if (role != WormRole::Heal && hold.target != targetGuid)
                cause = "target";

            if (!cause && hold.worm != state->worm.Get())
                cause = "phase";
            else if (!cause && !cureHold && AnchorDrifted(*state, hold))
                cause = "drift";
            else if (!cause && !cureHold && hasHealReach &&
                     healReach.first.GetExactDist2d(view.spot) > healReach.second + REACH_HOLD_SLACK)
                cause = "heal";
        }
        else if (cureWalk && !cureHold && !plan.urgent)
            cause = "cure";

        if (!cause)
        {
            if (view.parked)
            {
                bool const pushed = dist > WORM_HOLD_TRIGGER ||
                                    (dist > WORM_HOLD_ARRIVE && (plan.urgent || (cureHold && cureWalk)));
                if (pushed)
                {
                    cause = walkingFailure();
                    if (!cause)
                    {
                        view.parked = false;
                        NoteHold(bot, "unpark");
                    }
                }
                else
                    cause = parkedFailure();
            }
            else if (dist <= WORM_HOLD_ARRIVE)
            {
                if (walkOnly)
                    cause = "arrived";
                else
                {
                    cause = parkedFailure();
                    if (!cause)
                    {
                        view.parked = true;
                        NoteHold(bot, "park");
                    }
                }
            }
            else
                cause = walkingFailure();
        }

        if (cause)
            ReleaseHold(bot, *state, cause);
        else
        {
            hold.checkedMs = now;
            plan.held = true;
            plan.hold = view;
            plan.reason = view.parked ? WormMoveReason::None : view.reason;
            return true;
        }
    }

    if (cureWalk)
        plan.reason = carrier ? WormMoveReason::Run : WormMoveReason::Cure;
    else
        plan.reason = FirstFailedRule(rules, bx, by, false, SpreadTest::Strict);

    if (plan.reason != WormMoveReason::None && !plan.urgent && SearchBackedOff(*state, self, now))
        plan.reason = WormMoveReason::None;

    return true;
}

bool WormSpotStillSafe(WormMovePlan const& plan, Position const& spot)
{
    float const x = spot.GetPositionX();
    float const y = spot.GetPositionY();
    for (HazardCircle const& circle : plan.urgentCircles)
        if (circle.first.GetExactDist2d(x, y) < circle.second)
            return false;

    std::function<bool(float, float)> const& accept = plan.urgent ? plan.escapeAccept : plan.accept;
    return !accept || accept(x, y);
}

void SetWormHold(PlayerbotAI* botAI, Position const& spot, WormMoveReason reason, WormSpotTier tier, bool parked)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return;

    ObjectGuid const self = bot->GetGUID();
    uint32 const now = getMSTime();
    auto existing = state->holds.find(self);

    bool const same = existing != state->holds.end() && existing->second.view.reason == reason &&
                      existing->second.view.tier == tier &&
                      existing->second.view.spot.GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE;
    if (!same)
    {
        WormHold fresh;
        Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        fresh.target = target ? target->GetGUID() : ObjectGuid::Empty;
        fresh.worm = state->worm.Get();
        if (WormRead const* anchor = AnchorWorm(*state))
        {
            fresh.anchor = anchor->guid;
            fresh.anchorPos = anchor->pos;
        }

        WormMember const* me = FindMember(*state, self);
        WormRole const role = me ? me->role : RoleOf(bot);
        fresh.melee = role == WormRole::Melee || role == WormRole::Tank;
        if (IsWormCureReason(reason))
        {
            CurePairing const pairing = PairingOf(*state, bot, me, GetBeastsTankDuty(botAI));
            WormMember const* partner = pairing.carrier ? pairing.carrier : pairing.nearestBile;
            if (partner)
                fresh.partner = partner->guid;
        }

        existing = state->holds.insert_or_assign(self, fresh).first;
    }

    WormHold& hold = existing->second;
    hold.view.spot = spot;
    hold.view.reason = reason;
    hold.view.tier = tier;
    hold.view.issuedMs = now;
    hold.view.parked = parked;
    hold.checkedMs = now;

    if (!same)
        NoteHold(bot, std::string("set ") + WormMoveReasonName(reason) + " " + WormSpotTierName(tier));
    if (parked)
        NoteHold(bot, "park");
}

bool IsWormSpotHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JormungarsState* state = bot && bot->IsAlive() ? FindState(bot) : nullptr;
    if (!state)
        return false;

    auto const hold = state->holds.find(bot->GetGUID());
    return hold != state->holds.end() && IsFresh(hold->second, getMSTime());
}

bool IsWormWalkInFlight(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JormungarsState* state = bot ? FindState(bot) : nullptr;
    if (!state)
        return false;

    auto const hold = state->holds.find(bot->GetGUID());
    if (hold == state->holds.end())
        return false;

    WormHoldView const& view = hold->second.view;
    if (view.parked || getMSTimeDiff(view.issuedMs, getMSTime()) >= WORM_WALK_LATCH_MS || !bot->isMoving())
        return false;

    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    return view.spot.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= WALK_SPOT_TOLERANCE;
}

void BackOffWormSearch(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (JormungarsState* state = bot ? FindState(bot) : nullptr)
        state->backoffs[bot->GetGUID()] = getMSTime();
}

bool WormReachCrossesPool(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return false;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return false;

    uint32 const now = getMSTime();
    ObjectGuid const self = bot->GetGUID();
    BeastsTankDuty const duty = GetBeastsTankDuty(botAI);
    WormMember const* me = FindMember(*state, self);
    WormRole const role = me ? me->role : RoleOf(bot);
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    ReachHazard const hazard = ReachThroughHazard(botAI, bot, *state, role, target, duty, HeldOut(*state, me, duty), now);
    if (hazard == ReachHazard::None)
    {
        state->reachHolds.erase(self);
        return false;
    }

    if (!SearchBackedOff(*state, self, now))
        return true;

    if (hazard != ReachHazard::EndsIn)
        return false;

    auto const held = state->reachHolds.try_emplace(self, now).first;
    return getMSTimeDiff(held->second, now) < WORM_REACH_HOLD_MS;
}

bool WormReachIntoSpew(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return false;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged || GetBeastsTankDuty(botAI) != BeastsTankDuty::None)
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target || !target->IsAlive())
        return false;

    WormMember const* me = FindMember(*state, bot->GetGUID());
    Position stop;
    float walk = 0.0f;
    if (!GenericReachStop(botAI, bot, me ? me->role : RoleOf(bot), target, stop, walk))
        return false;

    float const coneArc = WormSpewHalfArc(botAI) + WORM_SPEW_CLEAR_PAD;
    for (WormRead const& worm : state->worms)
        if (IsUp(worm) && worm.mobile && worm.spewArmed &&
            SpewBlocksWalk(bot->GetPosition(), stop, worm.pos, coneArc))
            return true;

    return false;
}

Player* GetWormToxinHealTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    ToxinHealState* state = bot && bot->IsAlive() ? RefreshToxinHeals(botAI) : nullptr;
    if (!state)
        return nullptr;

    auto const duty = state->carrierOf.find(bot->GetGUID());
    return duty != state->carrierOf.end() ? ObjectAccessor::GetPlayer(*bot, duty->second) : nullptr;
}

char const* GetWormToxinSelfPurge(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive() || IsBeastsTank(bot))
        return nullptr;

    ToxinHealState* state = RefreshToxinHeals(botAI);
    if (!state || !state->open || state->cureRuns)
        return nullptr;

    Aura* toxin = bot->GetAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_PARALYTIC_TOXIN, bot));
    AuraEffect* slow = toxin ? toxin->GetEffect(EFFECT_0) : nullptr;
    if (!slow || slow->GetAmount() > WORM_TOXIN_STUCK_SLOW)
        return nullptr;

    switch (bot->getClass())
    {
        case CLASS_PALADIN:
            return "divine shield";
        case CLASS_MAGE:
            return "ice block";
        default:
            return nullptr;
    }
}

std::vector<HazardCircle> GetSlimePoolCircles(PlayerbotAI* botAI, uint32 ticks, float pad)
{
    std::vector<HazardCircle> circles;
    JormungarsState const* state = Refresh(botAI);
    if (!state)
        return circles;

    for (WormPool const& pool : state->pools)
        circles.emplace_back(pool.pos, PoolRadiusIn(pool, ticks) + pad);

    return circles;
}

Position PushOutOfSlimePools(PlayerbotAI* botAI, Position const& spot)
{
    std::vector<HazardCircle> const circles = GetSlimePoolCircles(botAI, WORM_POOL_SOON_TICKS, WORM_POOL_CLEARANCE_PAD);
    Position out = spot;
    for (uint32 pass = 0; pass < SLIME_POOL_PUSH_PASSES; ++pass)
    {
        for (HazardCircle const& circle : circles)
        {
            Position const& centre = circle.first;
            if (centre.GetExactDist2d(out) >= circle.second)
                continue;

            float const cx = centre.GetPositionX();
            float const cy = centre.GetPositionY();
            float angle = centre.GetExactDist2d(out) > FEET_CLEARANCE ? centre.GetAngle(&out) : centre.GetAngle(&ARENA_CENTER);
            Position pushed(cx + circle.second * std::cos(angle), cy + circle.second * std::sin(angle), out.GetPositionZ());
            if (ARENA_CENTER.GetExactDist2d(pushed) > WORM_SPOT_FLOOR_RADIUS)
            {
                angle = centre.GetAngle(&ARENA_CENTER);
                pushed = Position(cx + circle.second * std::cos(angle), cy + circle.second * std::sin(angle),
                                  out.GetPositionZ());
            }

            out = pushed;
        }
    }

    return out;
}

bool InSlimePoolWithoutWorms(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return false;

    JormungarsState const* state = Refresh(botAI);
    if (!state || state->engaged)
        return false;

    uint32 const ticks = PoolTicksIn(PinnedCastRemainingMs(bot));
    return std::any_of(state->pools.begin(), state->pools.end(), [bot, ticks](WormPool const& pool)
                       { return pool.pos.GetExactDist2d(bot) < PoolRadiusIn(pool, ticks) + WORM_POOL_TRIGGER_PAD; });
}

bool FindSlimePoolExit(PlayerbotAI* botAI, Position& spot)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    JormungarsState const* state = bot ? Refresh(botAI) : nullptr;
    if (!state || state->pools.empty())
        return false;

    std::vector<HazardCircle> circles;
    for (WormPool const& pool : state->pools)
        circles.emplace_back(pool.pos, pool.radiusSoon + WORM_POOL_CLEARANCE_PAD);

    Position chargeStart;
    Position chargeEnd;
    bool const charge = IcehowlChargeLatched(botAI, chargeStart, chargeEnd);
    WormPath const path = PathFrom(bot, *state);
    auto const floor = [&](float x, float y, bool pathTest)
    {
        if (ARENA_CENTER.GetExactDist2d(x, y) > WORM_SPOT_FLOOR_RADIUS)
            return false;

        if (charge && DistanceToIcehowlLane(chargeStart, chargeEnd, x, y) < ICEHOWL_CHARGE_CLEARANCE)
            return false;

        return !pathTest || !PathBlocked(path, x, y, true);
    };

    std::function<bool(float, float)> inReach;
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (target && target->IsAlive() && !PlayerbotAI::IsHeal(bot))
    {
        Position const at = target->GetPosition();
        float const reach = PlayerbotAI::IsRanged(bot)
                                ? botAI->GetRange("spell") + bot->GetCombatReach() + target->GetCombatReach() - 1.0f
                                : bot->GetMeleeRange(target) - 1.0f;
        inReach = [at, reach](float x, float y) { return at.GetExactDist2d(x, y) <= reach; };
    }

    HazardSweepCache sweep;
    Position const prefer = Jittered(bot->GetPosition(), bot->GetGUID());
    auto const search = [&](std::function<bool(float, float)> const& accept)
    {
        spot = FindNearestPositionClearOfHazards(bot, circles, WORM_SEARCH_RADIUS, 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, &prefer, accept, &sweep);
        return spot != Position();
    };

    if (inReach && search([&](float x, float y) { return floor(x, y, true) && inReach(x, y); }))
        return true;

    if (search([&](float x, float y) { return floor(x, y, true); }))
        return true;

    return search([&](float x, float y) { return floor(x, y, false); });
}

bool CorpseInSlimePool(Player* dead)
{
    JormungarsState const* state = dead ? FindState(dead) : nullptr;
    if (!state || state->pools.empty() || getMSTimeDiff(state->poolsReadMs, getMSTime()) > SLIME_POOL_READ_STALE_MS)
        return false;

    return std::any_of(state->pools.begin(), state->pools.end(), [dead](WormPool const& pool)
                       {
                           return pool.pos.GetExactDist2d(dead) <
                                  PoolRadiusIn(pool, SLIME_POOL_RISE_TICKS) + WORM_POOL_TRIGGER_PAD;
                       });
}

}
