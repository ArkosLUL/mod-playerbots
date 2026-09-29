#include "ToCHelpers_Jormungars.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <list>
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
namespace
{
constexpr size_t SLOT_ACIDMAW = 0;
constexpr size_t SLOT_DREADSCALE = 1;
constexpr size_t SLOT_COUNT = 2;

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
// Melee aim this far behind their worm, out of its front
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

struct WormRead
{
    ObjectGuid guid;
    Position pos;
    bool submerged = false;
    bool mobile = false;
    bool spewing = false;
    bool sweeping = false;
};

struct WormPool
{
    Position pos;
    float radius;
    float radiusSoon;  // 5 s on
};

struct WormMember
{
    ObjectGuid guid;
    Position pos;
    bool toxin = false;
    int32 snare = 0;
    bool bile = false;
    bool holder = false;
    bool stationaryHolder = false;
    bool stuck = false;
    bool runner = false;  // may be sent to a stuck carrier
};

struct WormWalk
{
    Position spot;
    uint32 issuedMs = 0;
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

    uint32 poolSearchMs = 0;
    bool poolSearched = false;
    std::vector<ObjectGuid> poolGuids;
    std::vector<WormPool> pools;

    std::vector<WormMember> roster;
    std::unordered_map<ObjectGuid, ObjectGuid> runnerOf;   // stuck carrier -> runner
    std::unordered_map<ObjectGuid, ObjectGuid> carrierOf;  // runner -> stuck carrier

    std::unordered_map<ObjectGuid, WormWalk> walks;
};

RaidInstanceState<JormungarsState> jormungarsStates;

float PoolRadius(uint32 tick)
{
    return std::min(WORM_POOL_BASE_RADIUS + WORM_POOL_GROWTH_PER_TICK * static_cast<float>(tick),
                    WORM_POOL_MAX_RADIUS);
}

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
        diff -= 2.0f * static_cast<float>(M_PI);

    return std::fabs(diff) <= halfArc;
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

void NoteCure(Player* bot, char const* branch, ObjectGuid const& other = ObjectGuid::Empty)
{
    if (!RaidObs::Active())
        return;

    std::string note = branch;
    if (!other.IsEmpty())
        note += " " + RaidObs::DescribeAssignment(other);

    RaidObs::NoteDerived(bot, "nb.cure", note);
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

        uint32 const tick = grow->GetTickNumber();
        state.pools.push_back({pool->GetPosition(), PoolRadius(tick), PoolRadius(tick + WORM_POOL_SOON_TICKS)});
    }
}

void RefreshRoster(PlayerbotAI* botAI, Player* bot, JormungarsState& state)
{
    state.roster.clear();

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
        entry.runner = entry.bile && GET_PLAYERBOT_AI(member) != nullptr &&
                       (!entry.holder || (dutyWorm && IsWormSubmerged(dutyWorm)));
        state.roster.push_back(entry);
    };

    Group* group = bot->GetGroup();
    if (!group)
    {
        add(bot);
        return;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        add(ref->GetSource());
}

WormMember const* FindMember(JormungarsState const& state, ObjectGuid const& guid)
{
    for (WormMember const& member : state.roster)
        if (member.guid == guid)
            return &member;

    return nullptr;
}

// Damage lands on this spot now: a pool, a spewing worm's real cone, a Sweep being cast, or a Bile
// pulse with no Toxin on the bot for it to strip
bool UrgentAt(JormungarsState const& state, ObjectGuid const& self, float x, float y, bool toxin, bool sweepHolder,
              float halfArc)
{
    for (WormPool const& pool : state.pools)
        if (pool.pos.GetExactDist2d(x, y) < pool.radius)
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
            if (state.carrierOf.count(runner->guid))
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

        bool const stillRunner = std::any_of(runners.begin(), runners.end(), [&kept](WormMember const* runner)
                                             { return runner->guid == kept->second; });
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

void Disengage(JormungarsState& state)
{
    state.worms.fill(WormRead());
    state.spewNoted.fill(false);
    state.poolSearched = false;
    state.poolGuids.clear();
    state.pools.clear();
    state.roster.clear();
    state.runnerOf.clear();
    state.carrierOf.clear();
    state.walks.clear();
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

        if (UrgentAt(state, member.guid, member.pos.GetPositionX(), member.pos.GetPositionY(), member.toxin,
                     member.stationaryHolder, halfArc))
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
        Disengage(state);
        // Off the encounter only the drop back to 0 is news
        if (state.live || state.worm.Get() != static_cast<uint8>(WormForm::None))
            state.worm = static_cast<uint8>(WormForm::None);

        return &state;
    }

    ReadWorm(map, acidmaw, SPELL_ACIDIC_SPEW, sSpellMgr->GetSpellIdForDifficulty(SPELL_ACIDIC_SPEW_TICK, bot),
             state.worms[SLOT_ACIDMAW], state.spewNoted[SLOT_ACIDMAW]);
    ReadWorm(map, dreadscale, SPELL_MOLTEN_SPEW, sSpellMgr->GetSpellIdForDifficulty(SPELL_MOLTEN_SPEW_TICK, bot),
             state.worms[SLOT_DREADSCALE], state.spewNoted[SLOT_DREADSCALE]);
    RefreshPools(bot, map, state, now);
    RefreshRoster(botAI, bot, state);
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

void SetRoleAccept(PlayerbotAI* botAI, Player* bot, WormRole role, Unit* target, WormMovePlan& plan)
{
    switch (role)
    {
        case WormRole::Heal:
        {
            std::vector<Position> holders;
            for (BeastsTankDuty duty : {BeastsTankDuty::WormMobile, BeastsTankDuty::WormStationary})
            {
                Player* holder = GetBeastsDutyHolder(botAI, duty);
                if (holder && holder->IsAlive() && holder != bot)
                    holders.push_back(holder->GetPosition());
            }

            float const range = botAI->GetRange("heal");
            plan.roleAccept = [holders, range](float x, float y)
            {
                return std::all_of(holders.begin(), holders.end(), [x, y, range](Position const& holder)
                                   { return holder.GetExactDist2d(x, y) <= range; });
            };
            return;
        }
        case WormRole::Ranged:
            if (target && target->IsAlive())
            {
                Position const at = target->GetPosition();
                float const range = botAI->GetRange("spell");
                plan.roleAccept = [at, range](float x, float y) { return at.GetExactDist2d(x, y) <= range; };
                return;
            }
            break;
        case WormRole::Melee:
            // Any target, not just a worm: in a heroic overlap `reach melee` walks a melee straight back
            // to Gormok from a spot out of his reach
            if (target && target->IsAlive() && !(IsWormEntry(target) && IsWormSubmerged(target)))
            {
                Position const at = target->GetPosition();
                float const reach = (IsWormEntry(target) ? WORM_MELEE_RANGE : bot->GetMeleeRange(target)) - 1.0f;
                plan.roleAccept = [at, reach](float x, float y) { return at.GetExactDist2d(x, y) <= reach; };
                return;
            }
            break;
        default:
            break;
    }

    plan.roleAccept = [](float, float) { return true; };
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

    // Tricks moves the threat off whatever the rogue hits, and melee hit the skull, so rogues serve the
    // mobile worm. Only hunters, whose dump shot aims at the assigned worm, alternate.
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

    // Charges left from a Misdirection cast under ground go into this worm, not the rotation's skull
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

    WormMember const* me = FindMember(*state, bot->GetGUID());
    return me && me->bile && GetBeastsTankDuty(botAI) == BeastsTankDuty::None && !state->carrierOf.count(me->guid);
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
        case WormMoveReason::Bile:
            return "bile";
        case WormMoveReason::Sweep:
            return "sweep";
        case WormMoveReason::Spew:
            return "spew";
        case WormMoveReason::Spread:
            return "spread";
        default:
            return "none";
    }
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

    ObjectGuid const self = bot->GetGUID();
    WormMember const* me = FindMember(*state, self);
    bool const toxin = me && me->toxin;
    bool const bile = me && me->bile;
    bool const stuck = me && me->stuck;

    auto const assigned = state->carrierOf.find(self);
    WormMember const* carrier = assigned != state->carrierOf.end() ? FindMember(*state, assigned->second) : nullptr;

    WormMember const* nearestBile = nullptr;
    float nearestBileDist = 0.0f;
    if (toxin && !stuck && !bile)
    {
        for (WormMember const& member : state->roster)
        {
            if (!member.bile || member.guid == self)
                continue;

            float const dist = bot->GetExactDist(member.pos);
            if (!nearestBile || dist < nearestBileDist)
            {
                nearestBile = &member;
                nearestBileDist = dist;
            }
        }
    }

    if (carrier)
        NoteCure(bot, "run", carrier->guid);
    else if (stuck)
    {
        auto const runner = state->runnerOf.find(self);
        NoteCure(bot, "wait", runner != state->runnerOf.end() ? runner->second : ObjectGuid::Empty);
    }
    else if (nearestBile)
        NoteCure(bot, "seek", nearestBile->guid);
    else
        NoteCure(bot, "none");

    BeastsTankDuty const duty = GetBeastsTankDuty(botAI);
    bool const holder = duty == BeastsTankDuty::WormMobile || duty == BeastsTankDuty::WormStationary;
    if (holder && !carrier)
        return false;

    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return false;

    Position partnerPos;
    if (carrier && bot->GetExactDist(carrier->pos) > WORM_CURE_TRIGGER)
    {
        plan.reason = WormMoveReason::Run;
        plan.partner = carrier->guid;
        partnerPos = carrier->pos;
    }
    else if (nearestBile && nearestBileDist > WORM_CURE_TRIGGER)
    {
        plan.reason = WormMoveReason::Cure;
        plan.partner = nearestBile->guid;
        partnerPos = nearestBile->pos;
    }

    bool const cureWalk = plan.reason != WormMoveReason::None;
    WormMoveReason failed = WormMoveReason::None;
    auto const fail = [&failed](WormMoveReason reason)
    {
        if (failed == WormMoveReason::None)
            failed = reason;
    };

    float const bx = bot->GetPositionX();
    float const by = bot->GetPositionY();
    float const halfArc = WormSpewHalfArc(botAI);
    plan.urgent = UrgentAt(*state, self, bx, by, toxin, duty == BeastsTankDuty::WormStationary, halfArc);

    for (WormPool const& pool : state->pools)
    {
        float const clearance = pool.radiusSoon + WORM_POOL_CLEARANCE_PAD;
        plan.circles.emplace_back(pool.pos, clearance);
        plan.urgentCircles.emplace_back(pool.pos, clearance);
        if (pool.pos.GetExactDist2d(bx, by) < pool.radius + WORM_POOL_TRIGGER_PAD)
            fail(WormMoveReason::Pool);
    }

    // Toxin carriers want the pulse
    std::vector<HazardCircle> runBile;
    if (!toxin)
    {
        for (WormMember const& member : state->roster)
        {
            if (!member.bile || member.guid == self)
                continue;

            // The run walk can't keep off them, but its escape from their pulse can
            if (cureWalk)
            {
                runBile.emplace_back(member.pos, WORM_BILE_CLEARANCE);
                continue;
            }

            float const dist = member.pos.GetExactDist2d(bx, by);
            plan.circles.emplace_back(member.pos, WORM_BILE_CLEARANCE);
            plan.urgentCircles.emplace_back(member.pos, WORM_BILE_CLEARANCE);
            if (dist < WORM_BILE_TRIGGER)
                fail(WormMoveReason::Bile);
        }
    }

    // A carrier's own pulse lands on everyone round it. A tank with a beast to hold stays on it.
    if (bile && !carrier && duty == BeastsTankDuty::None)
    {
        for (WormMember const& member : state->roster)
        {
            if (member.guid == self || member.toxin || member.bile)
                continue;

            plan.circles.emplace_back(member.pos, WORM_BILE_CLEARANCE);
            if (member.pos.GetExactDist2d(bx, by) < WORM_BILE_TRIGGER)
                fail(WormMoveReason::Bile);
        }
    }

    WormRole const role = RoleOf(bot);
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    // `reach melee` walks a melee on Gormok straight back into Sweep range, so it only dodges the cast
    bool const sweepOnlyWhileCast = role == WormRole::Melee && target && target->IsAlive() && !IsWormEntry(target);
    if (duty != BeastsTankDuty::WormStationary)
    {
        for (WormRead const& worm : state->worms)
        {
            if (!IsUp(worm) || worm.mobile)
                continue;

            plan.circles.emplace_back(worm.pos, WORM_SWEEP_CLEARANCE);
            if (worm.sweeping)
                plan.urgentCircles.emplace_back(worm.pos, WORM_SWEEP_CLEARANCE);

            if (!cureWalk && (worm.sweeping || !sweepOnlyWhileCast) &&
                worm.pos.GetExactDist2d(bx, by) < WORM_SWEEP_TRIGGER)
                fail(WormMoveReason::Sweep);
        }
    }

    // A cure pair stands just outside the real cone, where its walk put it
    bool const atPartner = (nearestBile && nearestBileDist <= WORM_CURE_TRIGGER) ||
                           (carrier && bot->GetExactDist(carrier->pos) <= WORM_CURE_TRIGGER);
    float const spewTrigger = halfArc + (atPartner ? 0.0f : WORM_SPEW_TRIGGER_PAD);
    std::vector<Position> cones;
    for (WormRead const& worm : state->worms)
    {
        if (!IsUp(worm) || !worm.mobile)
            continue;

        cones.push_back(worm.pos);
        if (!cureWalk && InCone(worm.pos, bx, by, spewTrigger))
            fail(WormMoveReason::Spew);
    }

    bool const caster = role == WormRole::Ranged || role == WormRole::Heal;
    bool const bothWorms = !state->worms[SLOT_ACIDMAW].guid.IsEmpty() && !state->worms[SLOT_DREADSCALE].guid.IsEmpty();
    if (!cureWalk && caster && bothWorms && GetBeastOfDuty(botAI, BeastsTankDuty::WormStationary))
    {
        for (WormMember const& member : state->roster)
        {
            // A cure pair has to stand together
            if (member.guid == self || (toxin && member.bile) || (bile && member.toxin))
                continue;

            plan.circles.emplace_back(member.pos, WORM_SPREAD_CLEARANCE);
            if (member.pos.GetExactDist2d(bx, by) < WORM_SPREAD_TRIGGER)
                fail(WormMoveReason::Spread);
        }
    }

    // Heroic overlap: one mover per bot, so the worm spot also keeps casters out of the stomp
    if (!cureWalk && caster)
        if (Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok))
            if (!GetGormokForSnoboldCarrier(botAI))
                plan.circles.emplace_back(gormok->GetPosition(), GORMOK_STOMP_CLEARANCE);

    plan.circles.emplace_back(bot->GetPosition(), FEET_CLEARANCE);

    if (!cureWalk)
        plan.reason = failed;

    if (plan.reason == WormMoveReason::None)
        return false;

    plan.escapeCircles = plan.urgentCircles;
    plan.escapeCircles.insert(plan.escapeCircles.end(), runBile.begin(), runBile.end());

    // On 10N the 60 degree cone plus the pad covers the whole disc round a mobile worm's tank, where a
    // cure walk ends. Just outside the real cone is safe, and urgency covers a Spew being cast.
    float const coneArc = cureWalk ? halfArc : halfArc + WORM_SPEW_CLEAR_PAD;
    plan.escapeAccept = [cones, coneArc](float x, float y)
    {
        if (ARENA_CENTER.GetExactDist2d(x, y) > WORM_FLOOR_RADIUS)
            return false;

        for (Position const& cone : cones)
            if (InCone(cone, x, y, coneArc))
                return false;

        return true;
    };

    if (cureWalk)
    {
        std::function<bool(float, float)> const floorAndCones = plan.escapeAccept;
        plan.accept = [floorAndCones, partnerPos](float x, float y)
        { return floorAndCones(x, y) && partnerPos.GetExactDist2d(x, y) <= WORM_CURE_REACH; };
    }
    else
        plan.accept = plan.escapeAccept;

    SetRoleAccept(botAI, bot, role, target, plan);

    if (cureWalk)
        plan.preferNear = partnerPos;
    else if (role == WormRole::Melee && IsWormEntry(target) && target->IsAlive() && !IsWormSubmerged(target))
    {
        float const facing = target->GetOrientation();
        plan.preferNear = Position(target->GetPositionX() - MELEE_BEHIND * std::cos(facing),
                                   target->GetPositionY() - MELEE_BEHIND * std::sin(facing), target->GetPositionZ());
    }
    else
        plan.preferNear = bot->GetPosition();

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

void SetWormWalk(Player* bot, Position const& spot)
{
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return;

    WormWalk& walk = jormungarsStates.For(bot->GetInstanceId()).walks[bot->GetGUID()];
    walk.spot = spot;
    walk.issuedMs = getMSTime();
}

void ClearWormWalk(Player* bot)
{
    if (JormungarsState* state = FindState(bot))
        state->walks.erase(bot->GetGUID());
}

bool GetWormWalk(Player* bot, Position& spot, uint32& issuedMs)
{
    JormungarsState* state = FindState(bot);
    if (!state)
        return false;

    auto const walk = state->walks.find(bot->GetGUID());
    if (walk == state->walks.end())
        return false;

    spot = walk->second.spot;
    issuedMs = walk->second.issuedMs;
    return true;
}

bool IsWormWalkInFlight(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Position spot;
    uint32 issuedMs = 0;
    if (!bot || !GetWormWalk(bot, spot, issuedMs))
        return false;

    if (getMSTimeDiff(issuedMs, getMSTime()) >= WORM_WALK_LATCH_MS || !bot->isMoving())
        return false;

    // Moving on some other node's walk doesn't count
    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    return spot.GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= WALK_SPOT_TOLERANCE;
}

}
