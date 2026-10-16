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
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Gormok.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_NorthrendBeasts.h"
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

// Also the nb.worm value
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

// Whole arena from anywhere on its floor
constexpr float POOL_SEARCH_RADIUS = 200.0f;
// A new pool starts at 2 yd, so finding it half a second late costs nothing
constexpr uint32 POOL_SEARCH_INTERVAL_MS = 500;
// 1 s cast, then 2.5 s of ticks
constexpr uint32 SPEW_NOTE_TTL_MS = 3500;
// Melee on a worm with no dealt stand aim this far behind it, out of its front
constexpr float MELEE_BEHIND = 6.0f;
// The drag aims this far on from the tank, so the worm keeps facing the same way
constexpr float DRAG_LEAD = 10.0f;
// Pool at full size, the melee round the worm and the worm's offset from its tank. The pad keeps the
// settled worm past the drag trigger, or the drag fires again as it stops.
constexpr float DRAG_POOL_CLEARANCE =
    WORM_POOL_MAX_RADIUS + WORM_MELEE_RANGE + WORM_TANK_OFFSET + WORM_POOL_CLEARANCE_PAD;
constexpr float DRAG_STATIONARY_CLEARANCE =
    WORM_SWEEP_CLEARANCE + WORM_MELEE_RANGE + WORM_TANK_OFFSET + WORM_POOL_CLEARANCE_PAD;
// The sweep finds nothing for an empty list, and a Spew or cure plan can have no circle of its own.
// Also turns down a spot the collision check pulled back onto the bot.
constexpr float FEET_CLEARANCE = 1.0f;
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// Roles come from strategies or talents, so a second is fresh enough and spares the talent walk
constexpr uint32 ROLE_REFRESH_MS = 1000;
// Spell::CheckRange adds melee range to a hunter's 5 yd minimum, 14.3 from a worm's centre; one spare
constexpr float HUNTER_WORM_FLOOR = WORM_MELEE_RANGE + 6.0f;
// Every cure spot round a member this close to the up stationary worm sits inside its Sweep path circle
constexpr float SWEEP_CURE_GAP = WORM_SWEEP_PATH_RADIUS - WORM_CURE_REACH + 1.0f;
// Golden angle: consecutive guid counters land far apart round the circle
constexpr double PREFER_JITTER_STEP = 2.39996;
// The melee stand sits exactly on the role reach, so the search allows float error on it
constexpr float REACH_SEARCH_SLACK = 0.05f;
// A held spot answers to the generic reach trigger itself, or a stand on the reach edge would let go
// on the worm's first step
constexpr float REACH_HOLD_SLACK = 1.0f;

struct WormRead
{
    ObjectGuid guid;
    Position pos;  // carries the facing
    bool submerged = false;
    bool mobile = false;
    bool spewing = false;
    bool sweeping = false;
    bool settled = false;    // hasn't walked or turned for WORM_SETTLE_MS
    bool spewArmed = false;  // spewing, or its next Spew can come within WORM_SPEW_LEAD_MS
};

// Per worm slot, across reads
struct WormSlotClock
{
    ObjectGuid guid;
    uint32 motionMs = 0;
    Position motionPos;  // carries the facing
    uint32 spewScheduleMs = 0;
    bool submerged = false;
    bool spewing = false;
};

// A Spray being cast. The splash lands on its target's spot as the cast ends.
struct WormSpray
{
    ObjectGuid target;  // empty: none
    Position targetPos;
    ObjectGuid worm;
    bool burning = false;
};

struct WormPool
{
    Position pos;
    uint32 tick = 0;
    float radius = 0.0f;
    float radiusSoon = 0.0f;  // 5 s on
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
    bool holder = false;
    bool stationaryHolder = false;
    bool stuck = false;
    bool runner = false;  // may be sent to a stuck carrier
};

struct WormHold
{
    WormHoldView view;
    uint32 checkedMs = 0;
    // What the spot was found against
    ObjectGuid target;
    uint8 worm = 0;
    ObjectGuid anchor;    // the up mobile worm, else the up stationary one
    Position anchorPos;   // carries its facing
    ObjectGuid partner;   // a cure or run hold's
    bool melee = false;
};

struct JormungarsState
{
    RaidObs::ObsValue<uint8> worm{"nb.worm"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
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

    std::vector<WormMember> roster;
    std::unordered_map<ObjectGuid, ObjectGuid> runnerOf;   // stuck carrier -> runner
    std::unordered_map<ObjectGuid, ObjectGuid> carrierOf;  // runner -> stuck carrier

    std::unordered_map<ObjectGuid, WormRole> roles;
    uint32 rolesMs = 0;
    bool rolesRead = false;

    std::unordered_map<ObjectGuid, WormHold> holds;
    std::unordered_map<ObjectGuid, uint32> backoffs;
};

RaidInstanceState<JormungarsState> jormungarsStates;

float PoolRadius(uint32 tick)
{
    return std::min(WORM_POOL_BASE_RADIUS + WORM_POOL_GROWTH_PER_TICK * static_cast<float>(tick),
                    WORM_POOL_MAX_RADIUS);
}

// The pool ticks once a second
uint32 PoolTicksIn(uint32 ms) { return (ms + 999) / 1000; }

float HalfArcOf(uint32 tickSpellId)
{
    if (SpellCone const* cone = sSpellMgr->GetSpellCone(tickSpellId))
        return static_cast<float>(cone->cone_degrees) * static_cast<float>(M_PI) / 360.0f;

    return WORM_SPEW_DEFAULT_HALF_ARC;
}

// origin carries the worm's facing
bool InCone(Position const& origin, float x, float y, float halfArc)
{
    float const dx = x - origin.GetPositionX();
    float const dy = y - origin.GetPositionY();
    if (dx * dx + dy * dy > WORM_SPEW_RANGE * WORM_SPEW_RANGE)
        return false;

    float diff = Position::NormalizeOrientation(std::atan2(dy, dx) - origin.GetOrientation());
    if (diff > static_cast<float>(M_PI))
        diff -= TWO_PI;

    return std::fabs(diff) <= halfArc;
}

// Radians in [0, pi] between origin's facing and the bearing from it to (x, y)
float AngleOffFacing(Position const& origin, float x, float y)
{
    float const diff = Position::NormalizeOrientation(
        std::atan2(y - origin.GetPositionY(), x - origin.GetPositionX()) - origin.GetOrientation());
    return diff > static_cast<float>(M_PI) ? TWO_PI - diff : diff;
}

// Radians in [0, pi] between two facings
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

// Submerge's transform aura shows an invisible model, so only the native id keeps the form
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

// The wedge goes out once per cast, latched until the worm stops spewing
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
    snprintf(params, sizeof(params), "\"facing\":%.2f,\"arc\":%.1f,\"range\":%.0f", worm->GetOrientation(),
             HalfArcOf(spewTick) * 180.0f / static_cast<float>(M_PI), WORM_SPEW_RANGE);
    RaidObs::NoteHazard(map, spewTick, read.pos, "wedge", params, SPEW_NOTE_TTL_MS);
}

// A new guid in the slot is that worm's engage
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

    if (fresh || (clock.submerged && !read.submerged) || (read.spewing && !clock.spewing))
        clock.spewScheduleMs = now;

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
        read.spewing || getMSTimeDiff(clock.spewScheduleMs, now) >= WORM_SPEW_EARLIEST_MS - WORM_SPEW_LEAD_MS;
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

        // SpellAuraEffects.cpp casts every tick of this aura with a radius mod that grows with the tick
        // count, so neither the DBC nor SpellInfoCorrections has the live radius
        Aura* aura = pool->GetAura(SPELL_SLIME_POOL_AURA);
        AuraEffect* grow = aura ? aura->GetEffect(EFFECT_0) : nullptr;
        if (!grow)
            continue;

        WormPool read;
        read.pos = pool->GetPosition();
        read.tick = grow->GetTickNumber();
        read.radius = PoolRadius(read.tick);
        read.radiusSoon = PoolRadius(read.tick + WORM_POOL_SOON_TICKS);
        state.pools.push_back(read);
    }
}

WormMember const* FindMember(JormungarsState const& state, ObjectGuid const& guid)
{
    for (WormMember const& member : state.roster)
        if (member.guid == guid)
            return &member;

    return nullptr;
}

// A Bile carrier the reposition keeps off the raid: no Beasts tank duty, not sent as a runner
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

        entry.bile = member->HasAura(SPELL_BURNING_BILE);

        Unit* dutyWorm = nullptr;
        if (member == mobileHolder)
            dutyWorm = mobileWorm;
        else if (member == stationaryHolder)
            dutyWorm = stationaryWorm;

        entry.holder = member == mobileHolder || member == stationaryHolder;
        entry.stationaryHolder = member == stationaryHolder;
        // Bile's pulse hits its own carrier too, so a carrier of both is cured without help
        entry.stuck = entry.toxin && !entry.bile && (entry.holder || entry.snare <= WORM_TOXIN_STUCK_SLOW);
        // A human never answers the assignment and would leave his carrier waiting
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

    // nb.wormhold is change-only, so a dead bot's last value would run on to the end of the pull
    for (auto const& entry : state.holds)
        if (!FindMember(state, entry.first))
            if (Player* player = ObjectAccessor::GetPlayer(map, entry.first))
                NoteHold(player, "release gone");

    EraseDeparted(state, state.holds);
    EraseDeparted(state, state.backoffs);
}

// A pinned cast or channel keeps the bot on its spot this long
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

// Damage lands on this spot now: a pool (as big as it grows before a pinning cast ends), a Spray
// being cast on or beside the bot, a spewing worm's real cone, a Sweep being cast, or a Bile pulse
// with no Toxin on the bot for it to strip
bool UrgentAt(JormungarsState const& state, ObjectGuid const& self, float x, float y, bool toxin, bool sweepHolder,
              bool sprayDodge, float halfArc, uint32 castRemainingMs)
{
    uint32 const castTicks = PoolTicksIn(castRemainingMs);
    for (WormPool const& pool : state.pools)
        if (pool.pos.GetExactDist2d(x, y) < std::max(pool.radius, PoolRadius(pool.tick + castTicks)))
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

        if (worm.mobile && worm.spewing && InCone(worm.pos, x, y, halfArc))
            return true;

        if (!worm.mobile && !sweepHolder && worm.sweeping && worm.pos.GetExactDist2d(x, y) < WORM_SWEEP_RADIUS)
            return true;
    }

    return false;
}

// Only that worm's holder can walk to a partner beside the up stationary worm, so nobody else is
// paired with one
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

    // A runner already at a carrier serves that one
    for (WormMember const* carrier : stuck)
        if (WormMember const* runner = nearestFree(carrier, WORM_CURE_TRIGGER))
            assign(carrier, runner->guid);

    // Then last ms's pairs, or two runners closing on two carriers can trade them back and forth
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
    // nb.wormhold is change-only, so a hold dropped silently would read as held to the end of the pull
    for (auto const& entry : state.holds)
        if (Player* player = ObjectAccessor::GetPlayer(map, entry.first))
            NoteHold(player, "release phase");

    state.worms.fill(WormRead());
    state.spewNoted.fill(false);
    state.clocks.fill(WormSlotClock());
    state.spray = WormSpray();
    state.poolSearched = false;
    state.poolGuids.clear();
    state.pools.clear();
    state.roster.clear();
    state.runnerOf.clear();
    state.carrierOf.clear();
    state.roles.clear();
    state.rolesRead = false;
    state.holds.clear();
    state.backoffs.clear();
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

// A bot mid hard cast runs no triggers, so its own reposition never fires: this read breaks the cast
// for it, on whichever bot's tick asks first. A channel is left to the bot's own node.
void BreakPinnedCasts(PlayerbotAI* botAI, Map* map, JormungarsState const& state)
{
    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return;

    float const halfArc = WormSpewHalfArc(botAI);
    for (WormMember const& member : state.roster)
    {
        // Holders get no reposition unless sent as runners
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

// Null off map 649. Every worm trigger on every bot asks, so one read per instance per ms.
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
        // Off the encounter only the drop back to 0 is news
        if (state.live || state.worm.Get() != static_cast<uint8>(WormForm::None))
            state.worm = static_cast<uint8>(WormForm::None);

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

int32 HunterIndex(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return -1;

    int32 index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->getClass() != CLASS_HUNTER)
            continue;

        if (member == bot)
            return index;

        ++index;
    }

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

// Melee spots hang off the mobile worm, everything else round both
WormRead const* AnchorWorm(JormungarsState const& state)
{
    WormRead const* mobile = UpWorm(state, true);
    return mobile ? mobile : UpWorm(state, false);
}

bool IsFresh(WormHold const& hold, uint32 now) { return getMSTimeDiff(hold.checkedMs, now) < WORM_HOLD_FRESH_MS; }

// Where a member will stand: its held spot while the plan keeps confirming it
Position HeldSpotOf(JormungarsState const& state, WormMember const& member, uint32 now)
{
    auto const hold = state.holds.find(member.guid);
    return hold != state.holds.end() && IsFresh(hold->second, now) ? hold->second.view.spot : member.pos;
}

// Inside a ring of equal walks each bot takes a different bearing
Position Jittered(Position const& at, ObjectGuid const& guid)
{
    float const angle = static_cast<float>(
        std::fmod(static_cast<double>(guid.GetCounter()) * PREFER_JITTER_STEP, 2.0 * M_PI));
    return Position(at.GetPositionX() + WORM_PREFER_JITTER * std::cos(angle),
                    at.GetPositionY() + WORM_PREFER_JITTER * std::sin(angle), at.GetPositionZ());
}

// Melee bots by guid, even ranks on one side of the worm's back and odd on the other, so every bot
// derives the same split
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
    WormMember const* carrier = nullptr;      // the stuck carrier this runner was sent to
    WormMember const* nearestBile = nullptr;  // the Bile carrier this Toxin carrier walks to
};

// A Toxin carrier that may walk seeks a Bile carrier: any one not stuck, and a stuck one holding no
// beast with no runner coming. It keeps its hold's partner while that one carries Bile, unless a
// nearer carrier already cures it.
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

// --- The plan's rules ---

enum WormRuleFlag : uint8
{
    RULE_HARD = 1,     // plan.circles
    RULE_SPREAD = 2,   // plan.spreadCircles
    RULE_URGENT = 4,   // plan.urgentCircles, so the escape's too
    RULE_ESCAPE = 8,   // the escape's only
    RULE_CLEAN = 16,   // a held-out carrier's circle on a clean member: the loose escape drops it
    RULE_PAIR = 32,    // the pair partner's spread circle: plan.pairCircles drops it
};

// How a spot answers to the casters' spread
enum class SpreadTest : uint8
{
    Strict,
    PairTolerant,
    Skip
};

struct WormSpreadPeer
{
    ObjectGuid guid;
    Position spot;  // its held spot
    bool bot = false;
};

struct WormCircleRule
{
    WormMoveReason reason = WormMoveReason::None;  // None: clearance only
    Position centre;
    float trigger = 0.0f;  // 0 never fires
    float clearance = 0.0f;
    uint8 flags = 0;
};

// Every test a spot answers to, so the bot's own spot (which reason fires) and a held spot (whether it
// still holds) run the same ones
struct WormRules
{
    std::vector<WormCircleRule> circles;
    std::vector<Position> cones;  // the up mobile worm, with its facing
    float halfArc = 0.0f;
    bool spewFires = false;
    bool hasPartner = false;
    Position partnerPos;
    bool hasStand = false;
    Position stand;
    bool reachCrossesPool = false;
    ObjectGuid self;
    std::vector<WormSpreadPeer> spreadPeers;  // empty while the spread is off for the bot
};

// No other peer stands within the trigger of this one's spot
bool LoneSpreadPeer(WormRules const& rules, WormSpreadPeer const& peer)
{
    for (WormSpreadPeer const& other : rules.spreadPeers)
        if (other.guid != peer.guid && other.spot.GetExactDist2d(peer.spot) < WORM_SPREAD_TRIGGER)
            return false;

    return true;
}

// The peer a pair spot may stand beside: the nearest lone one, else nullptr
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

// Only the highest-guid bot of a too-close set fails, so one of them moves and the rest keep their
// spots. A tolerant spot also holds beside one lone peer.
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

// The first rule whose trigger (x, y) fails, in reason order. A held spot answers to the drift deadband
// instead of the melee stand and to roleAccept instead of the reach rule, which is about the bot's own
// walk from where it stands.
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
        // A cure pair stands just outside the real cone, where its walk put it
        bool const atPartner = rules.hasPartner && rules.partnerPos.GetExactDist2d(x, y) <= WORM_CURE_TRIGGER;
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
        return WormMoveReason::Melee;

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

// A straight walk from the bot. A pool grows while it walks, so the pool counts at its size when the
// walk ends.
struct WormPath
{
    Position from;
    float speed = 1.0f;  // yd/s
    std::vector<WormPool> pools;
    std::vector<Position> sweeps;  // up stationary worms, none for its holder
};

// A circle the walk starts in only has to be left: heading out of it and ending outside
bool CircleBlocksWalk(Position const& from, Position const& to, Position const& centre, float radius)
{
    if (centre.GetExactDist2d(from) >= radius)
        return SegmentCrossesCircle(from, to, HazardCircle(centre, radius));

    float const outward = (to.GetPositionX() - from.GetPositionX()) * (from.GetPositionX() - centre.GetPositionX()) +
                          (to.GetPositionY() - from.GetPositionY()) * (from.GetPositionY() - centre.GetPositionY());
    return outward <= 0.0f || centre.GetExactDist2d(to) < radius;
}

bool PathBlocked(WormPath const& path, float x, float y, bool poolsOnly)
{
    Position const to(x, y, path.from.GetPositionZ());
    uint32 const ticks = static_cast<uint32>(std::ceil(path.from.GetExactDist2d(x, y) / path.speed));
    for (WormPool const& pool : path.pools)
        if (CircleBlocksWalk(path.from, to, pool.pos, PoolRadius(pool.tick + ticks) + WORM_POOL_PATH_PAD))
            return true;

    if (poolsOnly)
        return false;

    for (Position const& worm : path.sweeps)
        if (CircleBlocksWalk(path.from, to, worm, WORM_SWEEP_PATH_RADIUS))
            return true;

    return false;
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
    Position partnerPos;
    bool rearArc = false;
    Position rearOf;  // the worm, with its facing
};

// What turns (x, y) down, else nullptr. The escape keeps only the floor, the cones and the path, and
// the cross tier drops the path too.
char const* AcceptFailure(WormAccept const& accept, float x, float y, bool escape, bool skipPath)
{
    if (ARENA_CENTER.GetExactDist2d(x, y) > WORM_SPOT_FLOOR_RADIUS)
        return "path";

    for (Position const& cone : accept.cones)
        if (InCone(cone, x, y, accept.coneArc))
            return "spew";

    if (!escape)
    {
        if (accept.partnerReach && accept.partnerPos.GetExactDist2d(x, y) > WORM_CURE_REACH)
            return "target";

        if (accept.rearArc && AngleOffFacing(accept.rearOf, x, y) < WORM_MELEE_REAR_ARC_DEG * DEG_TO_RAD)
            return "melee";
    }

    return !skipPath && PathBlocked(accept.path, x, y, false) ? "path" : nullptr;
}

// WormSpotStillSafe with the failure named
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

// Generic reach from where the bot stands, `reach melee` for melee and tanks and `reach spell` for the
// rest, would walk it through a pool on the way to its stop point
bool ReachCrossesPool(PlayerbotAI* botAI, Player* bot, JormungarsState const& state, WormRole role, Unit* target,
                      BeastsTankDuty duty, bool heldOut)
{
    if (state.pools.empty() || duty != BeastsTankDuty::None || !target || !target->IsAlive())
        return false;

    bool const melee = role == WormRole::Melee || role == WormRole::Tank;
    // The Bile reach guard already keeps a held-out melee carrier off the stack
    if (melee && heldOut)
        return false;

    float stop = 0.0f;
    if (melee)
    {
        if (bot->IsWithinMeleeRange(target))
            return false;

        stop = bot->GetMeleeRange(target) - 1.0f;
    }
    else
    {
        float const range = botAI->GetRange("spell") + CONTACT_DISTANCE;
        if (bot->IsWithinCombatRange(target, range))
            return false;

        stop = range + bot->GetCombatReach() + target->GetCombatReach() - 1.0f;
    }

    float const dist = target->GetExactDist2d(bot);
    // Past the search radius the reposition can't find a spot in reach either, so generic reach walks it
    if (dist <= stop || dist - stop > WORM_SEARCH_RADIUS)
        return false;

    float const share = stop / dist;
    float const x = target->GetPositionX() + (bot->GetPositionX() - target->GetPositionX()) * share;
    float const y = target->GetPositionY() + (bot->GetPositionY() - target->GetPositionY()) * share;
    return PathBlocked(PathFrom(bot, state), x, y, true);
}

// The role's reach, centre to centre and just inside the generic trigger, plus `slack`. healReach
// already carries both reaches.
std::function<bool(float, float)> RoleReach(PlayerbotAI* botAI, Player* bot, JormungarsState const& state,
                                            WormRole role, Unit* target, HazardCircle const* healReach, bool heldOut,
                                            float slack)
{
    switch (role)
    {
        case WormRole::Heal:
        {
            // Each circle carries that member's own reach
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
                std::vector<Position> worms;
                if (bot->getClass() == CLASS_HUNTER)
                    for (WormRead const& worm : state.worms)
                        if (IsUp(worm))
                            worms.push_back(worm.pos);

                return [at, range, worms](float x, float y)
                {
                    if (at.GetExactDist2d(x, y) > range)
                        return false;

                    return std::none_of(worms.begin(), worms.end(), [x, y](Position const& worm)
                                        { return worm.GetExactDist2d(x, y) < HUNTER_WORM_FLOOR; });
                };
            }
            break;
        case WormRole::Melee:
        case WormRole::Tank:
            // Its reach melee is zeroed for the Bile's span, so a spot in reach would only keep it on the stack
            if (heldOut)
                break;

            // Any target, not just a worm: in a heroic overlap `reach melee` walks a melee straight back
            // to Gormok from a spot out of his reach
            if (target && target->IsAlive() && !(IsWormEntry(target) && IsWormSubmerged(target)))
            {
                Position const at = target->GetPosition();
                float const reach =
                    (IsWormEntry(target) ? WORM_MELEE_RANGE : bot->GetMeleeRange(target)) - 1.0f + slack;
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

    // A walking worm passes any deadband every tick. The spot is judged where it stops.
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
}  // namespace

bool IsWormSubmerged(Unit* worm) { return worm && worm->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE); }

bool IsWormMobile(Unit* worm)
{
    if (!worm)
        return false;

    if (!IsWormSubmerged(worm))
        return ShowsMobile(worm);

    // The two always hold opposite forms and the one still up shows its real one, so the duties flip
    // once both are under, not at each worm's own submerge
    if (Unit* other = OtherLivingWorm(worm))
        if (!IsWormSubmerged(other))
            return !ShowsMobile(other);

    // Every emerge flips the form, except the survivor's, which always comes up mobile
    return worm->HasAura(SPELL_WORM_ENRAGE) || !ShowsMobile(worm);
}

float WormSpewHalfArc(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    return HalfArcOf(bot ? sSpellMgr->GetSpellIdForDifficulty(SPELL_ACIDIC_SPEW_TICK, bot) : SPELL_ACIDIC_SPEW_TICK);
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

    if (IsWormSubmerged(worm) || worm->GetVictim() != bot)
        return false;

    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return false;

    WormRead const* stationary = nullptr;
    for (WormRead const& other : state->worms)
        if (IsUp(other) && !other.mobile && other.guid != worm->GetGUID())
            stationary = &other;

    bool needed = stationary && worm->GetExactDist2d(stationary->pos) < WORM_SWEEP_CLEARANCE + WORM_MELEE_RANGE;
    for (WormPool const& pool : state->pools)
        if (worm->GetExactDist2d(pool.pos) < pool.radiusSoon + WORM_MELEE_RANGE)
            needed = true;

    if (!needed)
        return false;

    for (WormPool const& pool : state->pools)
    {
        plan.hazards.emplace_back(pool.pos, DRAG_POOL_CLEARANCE);
        plan.poolsOnly.emplace_back(pool.pos, DRAG_POOL_CLEARANCE);
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

Unit* GetWormRedirectTarget(PlayerbotAI* botAI, Player*& holder)
{
    holder = nullptr;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive() || (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE))
        return nullptr;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return nullptr;

    // Tricks moves the threat off whatever the rogue hits, and DPS go for the mobile worm, so rogues
    // serve that one. Only hunters, whose dump shot aims at the assigned worm, alternate.
    bool const hunter = bot->getClass() == CLASS_HUNTER;
    int32 const index = hunter ? HunterIndex(bot) : 0;
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

    // Charges left from a Misdirection cast under ground go into this worm, not the bot's DPS target
    bool const chargesLeft = hunter && bot->HasAura(SPELL_MISDIRECTION_PROC);
    if (!IsWormSubmerged(worm) && worm->GetVictim() == dutyHolder && !chargesLeft)
        return nullptr;

    holder = dutyHolder;
    return worm;
}

bool IsWormBileCarrierHeldOut(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive())
        return false;

    JormungarsState* state = Refresh(botAI);
    if (!state || !state->engaged)
        return false;

    return HeldOut(*state, FindMember(*state, bot->GetGUID()), GetBeastsTankDuty(botAI));
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

// Escape, loose, partial and cross spots ignore reach, and the spot guard would strand a bot parked on one
bool WormTierParks(WormSpotTier tier)
{
    return tier != WormSpotTier::Partial && tier != WormSpotTier::Escape && tier != WormSpotTier::Loose &&
           tier != WormSpotTier::Cross;
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
        // Only a run gives a holder a spot, and its carrier is gone
        if (state->holds.count(self))
            ReleaseHold(bot, *state, "target");

        return false;
    }

    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return false;

    WormMember const* partner = carrier ? carrier : pairing.nearestBile;
    bool const cureWalk = partner && bot->GetExactDist(partner->pos) > WORM_CURE_TRIGGER;
    if (partner)
    {
        plan.partner = partner->guid;
        plan.partnerPos = partner->pos;
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
    if (partner)
        rules.partnerPos = partner->pos;

    for (WormPool const& pool : state->pools)
    {
        rules.circles.push_back({WormMoveReason::Pool, pool.pos, pool.radius + WORM_POOL_TRIGGER_PAD,
                                 pool.radiusSoon + WORM_POOL_CLEARANCE_PAD, RULE_HARD | RULE_URGENT});
        if (pool.pos.GetExactDist2d(bx, by) < pool.radius)
            plan.escapeFirst = true;
    }

    // Everyone round the target walks off it, and the target off them, so 10 yd opens within the cast
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

    // Toxin carriers want the pulse
    if (!toxin)
    {
        for (WormMember const& member : state->roster)
        {
            if (!member.bile || member.guid == self)
                continue;

            Position const at = HeldSpotOf(*state, member, now);
            // The run walk can't keep off them, but its escape from their pulse can
            if (cureWalk)
                rules.circles.push_back({WormMoveReason::None, at, 0.0f, WORM_BILE_CLEARANCE, RULE_ESCAPE});
            else
                rules.circles.push_back(
                    {WormMoveReason::Bile, at, WORM_BILE_TRIGGER, WORM_BILE_CLEARANCE, RULE_HARD | RULE_URGENT});
        }
    }

    // A carrier's own pulse lands on everyone round it. A tank with a beast to hold stays on it.
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

    // `reach melee` walks a melee on Gormok straight back into Sweep range, so it only dodges the cast
    bool const sweepOnlyWhileCast = role == WormRole::Melee && target && target->IsAlive() && !IsWormEntry(target);
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

    // Spew can't come before its earliest timer, and a cone kept up all the time turns with every
    // facing swing of the Emerge cast
    for (WormRead const& worm : state->worms)
        if (IsUp(worm) && worm.mobile && worm.spewArmed)
            rules.cones.push_back(worm.pos);

    // Spray's splash is 10 yd round where it lands, so casters keep that far off each other and off the
    // melee round the mobile worm. Nothing sprays while the stationary worm is under ground. Carriers
    // keep to their Bile and cure rules instead.
    bool const bothWorms = !state->worms[SLOT_ACIDMAW].guid.IsEmpty() && !state->worms[SLOT_DREADSCALE].guid.IsEmpty();
    if (!cureWalk && caster && !toxin && !bile && bothWorms && stationaryUp)
    {
        rules.self = self;
        for (WormMember const& member : state->roster)
            if (member.guid != self && IsCaster(member.role) && !member.toxin && !member.bile)
                rules.spreadPeers.push_back({member.guid, HeldSpotOf(*state, member, now), member.bot});

        // Peer circles only guide the search, SpreadFailsAt decides who moves
        WormSpreadPeer const* pairPartner = PairPartnerOf(rules, bot->GetPosition());
        for (WormSpreadPeer const& peer : rules.spreadPeers)
            rules.circles.push_back({WormMoveReason::Spread, peer.spot, 0.0f, WORM_SPREAD_CLEARANCE,
                                     static_cast<uint8>(RULE_SPREAD | (&peer == pairPartner ? RULE_PAIR : 0))});

        if (mobileUp)
            rules.circles.push_back({WormMoveReason::Spread, mobileUp->pos, WORM_MELEE_STACK_TRIGGER,
                                     WORM_MELEE_STACK_CLEARANCE, RULE_SPREAD});
    }

    // Heroic overlap: one mover per bot, so the worm spot also keeps casters out of the stomp
    if (!cureWalk && caster)
        if (Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok))
            if (!GetGormokForSnoboldCarrier(botAI))
                rules.circles.push_back(
                    {WormMoveReason::None, gormok->GetPosition(), 0.0f, GORMOK_STOMP_CLEARANCE, RULE_HARD});

    rules.circles.push_back({WormMoveReason::None, bot->GetPosition(), 0.0f, FEET_CLEARANCE, RULE_HARD});

    // Worms parry from the front: melee bots get a dealt stand behind the mobile worm, unless a cure
    // pairs them or their Bile keeps them off the stack. A stand dealt off a walking worm is stale by
    // the next tick, so generic reach follows it until it stops.
    if (role == WormRole::Melee && !toxin && !bile && mobileUp && mobileUp->settled && targetGuid == mobileUp->guid)
    {
        rules.hasStand = true;
        rules.stand = MeleeStand(*state, self, mobileUp->pos);
        plan.hasStand = true;
        plan.stand = rules.stand;
    }

    rules.reachCrossesPool = !cureWalk && ReachCrossesPool(botAI, bot, *state, role, target, duty, heldOut);

    FillCircles(rules, plan);
    // On heroic a shared Burning Bile kills at its first pulse
    Map* map = bot->FindMap();
    plan.spreadStrict =
        map && map->IsHeroic() && stationaryUp && stationaryUp->guid == state->worms[SLOT_DREADSCALE].guid;

    auto const accept = std::make_shared<WormAccept>();
    accept->path = PathFrom(bot, *state);
    accept->path.sweeps = sweeps;
    accept->cones = rules.cones;
    // On 10N the 60 degree cone plus the pad covers the whole disc round a mobile worm's tank, where a
    // cure walk ends. Just outside the real cone is safe, and urgency covers a Spew being cast.
    accept->coneArc = cureWalk ? halfArc : halfArc + WORM_SPEW_CLEAR_PAD;
    accept->partnerReach = cureWalk;
    accept->partnerPos = plan.partnerPos;
    accept->rearArc = rules.hasStand;
    if (rules.hasStand)
        accept->rearOf = mobileUp->pos;

    std::shared_ptr<WormAccept const> const model = accept;
    plan.accept = [model](float x, float y) { return AcceptFailure(*model, x, y, false, false) == nullptr; };
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

        // An escape, loose, partial or cross spot ignores partner, rear arc and reach by design, and a
        // cross walk was picked knowing its path is blocked
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

            // The cure's own trigger, not the walk's reach: a partner shifting a yard mustn't send it again
            if (cureHold && (!partner || partner->pos.GetExactDist2d(x, y) > WORM_CURE_TRIGGER))
                return "target";

            return partnerTier || roleHold(x, y) ? nullptr : "reach";
        };

        char const* cause = nullptr;
        // A partial walk is only ever toward its partner
        if (view.tier == WormSpotTier::Partial && (!partner || partner->guid != hold.partner))
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

    // An escape spot is off the partner, and re-issuing it every tick of the escape would stall it
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

    // A walk back to the same spot keeps what the spot was found against, or the drift test would
    // restart from wherever the worm has got to by then
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

    // A walk back to the held spot is the same latch, not a new one
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

    // Moving on some other node's walk doesn't count
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
    // A backed off search found no way round, and zeroing reach then strands the bot out of range
    if (!state || !state->engaged || SearchBackedOff(*state, bot->GetGUID(), getMSTime()))
        return false;

    BeastsTankDuty const duty = GetBeastsTankDuty(botAI);
    WormMember const* me = FindMember(*state, bot->GetGUID());
    WormRole const role = me ? me->role : RoleOf(bot);
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return ReachCrossesPool(botAI, bot, *state, role, target, duty, HeldOut(*state, me, duty));
}

}
