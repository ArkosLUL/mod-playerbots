#include "ToCHelpers_Icehowl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "Creature.h"
#include "Group.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCHelpers_NorthrendBeasts.h"

namespace TrialOfTheCrusaderHelpers
{
namespace
{
// Also the nb.charge value
enum class IcehowlChargePhase : uint8
{
    None = 0,
    Crash = 1,
    Gaze = 2,
    Charge = 3,
    Daze = 4,
    Rage = 5,
};

constexpr float GATE_ANGLE_MIN = 1.0f;
constexpr float GATE_ANGLE_MAX = 2.0f;

// A cycle runs about 9 s from his jump to the centre to the end of the charge, so a line older than
// this is stale whatever he's doing.
constexpr uint32 CHARGE_LATCH_MAX_MS = 12000;

// Shorter than the 30 s he always spends out of a cycle, so a silence this long means the end of the
// last one went unseen.
constexpr uint32 CHARGE_REFRESH_GAP_MS = 20000;

constexpr uint32 LANE_NOTE_TTL_MS = 4000;

struct IcehowlChargeState
{
    RaidObs::ObsValue<uint8> phase{"nb.charge"};
    RaidObs::ObsValue<ObjectGuid> gaze{"nb.gaze"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value

    bool inCycle = false;
    uint32 lastPassiveMs = 0;
    ObjectGuid gazeTarget;
    bool frozen = false;
    bool latched = false;
    bool expired = false;
    uint32 latchMs = 0;
    Position start;
    Position end;
};

RaidInstanceState<IcehowlChargeState> chargeStates;

void ResetCycle(IcehowlChargeState& state)
{
    state.inCycle = false;
    state.gazeTarget = ObjectGuid::Empty;
    state.frozen = false;
    state.latched = false;
    state.expired = false;
    state.latchMs = 0;
}

void NoteLane(Map* map, IcehowlChargeState const& state)
{
    if (!RaidObs::Active())
        return;

    char params[80];
    snprintf(params, sizeof(params), "\"ex\":%.1f,\"ey\":%.1f,\"half\":%.0f", state.end.GetPositionX(),
             state.end.GetPositionY(), ICEHOWL_TRAMPLE_RADIUS);
    RaidObs::NoteHazard(map, SPELL_TRAMPLE, state.start, "lane", params, LANE_NOTE_TTL_MS);
}

// Same maths as EVENT_JUMP_BACK, angle measured from the centre toward the charge's end.
void LatchLane(Map* map, IcehowlChargeState& state, float angle, uint32 now, bool note)
{
    float const cx = ARENA_CENTER.GetPositionX();
    float const cy = ARENA_CENTER.GetPositionY();
    float const cz = ARENA_CENTER.GetPositionZ();
    float const dx = std::cos(angle);
    float const dy = std::sin(angle);
    float const reach =
        angle > GATE_ANGLE_MIN && angle < GATE_ANGLE_MAX ? ICEHOWL_CHARGE_REACH_GATE : ICEHOWL_CHARGE_REACH;

    state.start = Position(cx - dx * ICEHOWL_CHARGE_BACK, cy - dy * ICEHOWL_CHARGE_BACK, cz);
    state.end = Position(cx + dx * reach, cy + dy * reach, cz);

    if (!state.latched)
    {
        state.latched = true;
        state.latchMs = now;
    }

    if (note)
        NoteLane(map, state);
}

// REACT_PASSIVE in combat only from the jump to the centre until the charge ends, so the cycle reads
// straight off it. The gaze keeps the whole raid stunned, so the line only matters from the jump
// back, and from then on he stands on it himself.
IcehowlChargeState* RefreshCharge(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    IcehowlChargeState& state = chargeStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;

    Unit* icehowl = GetEngagedBeast(botAI, NorthrendBeast::Icehowl);
    Creature* creature = icehowl ? icehowl->ToCreature() : nullptr;

    if (!creature || creature->GetReactState() != REACT_PASSIVE)
    {
        ResetCycle(state);

        IcehowlChargePhase outcome = IcehowlChargePhase::None;
        if (IsIcehowlStaggered(icehowl))
            outcome = IcehowlChargePhase::Daze;
        else if (HasIcehowlFrothingRage(icehowl))
            outcome = IcehowlChargePhase::Rage;

        state.phase = static_cast<uint8>(outcome);
        state.gaze = ObjectGuid::Empty;
        return &state;
    }

    if (!state.inCycle || getMSTimeDiff(state.lastPassiveMs, now) > CHARGE_REFRESH_GAP_MS)
    {
        ResetCycle(state);
        state.inCycle = true;
    }

    state.lastPassiveMs = now;

    // Sticky until he leaves passive, or a charge that never arrives would keep every bot's movers off
    // for the rest of the pull.
    if (state.latched && getMSTimeDiff(state.latchMs, now) > CHARGE_LATCH_MAX_MS)
    {
        state.latched = false;
        state.expired = true;
    }

    // GetPlayer finds a dead gaze target too, which the script still charges at.
    Player* target = ObjectAccessor::GetPlayer(*creature, creature->GetTarget());
    bool const atCentre = creature->GetExactDist2d(ARENA_CENTER.GetPositionX(), ARENA_CENTER.GetPositionY()) <=
                          ICEHOWL_CENTRE_TOLERANCE;

    IcehowlChargePhase phase = IcehowlChargePhase::Crash;
    if (state.frozen)
        phase = IcehowlChargePhase::Charge;
    else if (atCentre && target && target->IsAlive())
    {
        phase = IcehowlChargePhase::Gaze;
        state.gazeTarget = target->GetGUID();
        if (!state.expired)
            LatchLane(map, state, creature->GetAngle(target), now, !state.latched);
    }
    else if (atCentre)
    {
        // A gaze target dying mid gaze leaves the line where it was.
        if (!state.gazeTarget.IsEmpty())
            phase = IcehowlChargePhase::Gaze;
    }
    else if (target || !state.gazeTarget.IsEmpty())
    {
        phase = IcehowlChargePhase::Charge;
        state.frozen = true;
        if (target && state.gazeTarget.IsEmpty())
            state.gazeTarget = target->GetGUID();

        if (!state.expired)
            LatchLane(map, state, creature->GetAngle(ARENA_CENTER.GetPositionX(), ARENA_CENTER.GetPositionY()),
                      now, true);
    }

    state.phase = static_cast<uint8>(phase);
    state.gaze = state.gazeTarget;
    return &state;
}

constexpr uint32 BREATH_ROSTER_INTERVAL_MS = 1000;
// Bearing counts land on whole multiples of the step, this only absorbs float noise
constexpr float BEARING_EPSILON_DEG = 0.001f;

// Also the deal order
enum class BreathRole : uint8
{
    Healer,
    Ranged,
    Melee
};

struct BreathMember
{
    ObjectGuid guid;
    BreathRole role;
};

struct BreathBearing
{
    int32 step;  // steps round from straight behind him, negative one way
    float room;  // clear floor out along it
};

struct IcehowlBreathState
{
    RaidObs::ObsValue<uint32> arc{"nb.arc"};

    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value

    bool active = false;
    bool latched = false;
    Position spot;
    float front = 0.0f;  // bearing to his victim
    float anchor = 0.0f;
    float stepDeg = 0.0f;
    std::vector<BreathBearing> bearings;

    uint32 rosterMs = 0;
    bool rosterValid = false;
    std::vector<BreathMember> roster;  // deal order

    std::unordered_map<ObjectGuid, int32> slots;
    bool dealDirty = false;
};

RaidInstanceState<IcehowlBreathState> breathStates;

float DegToRad(float degrees) { return degrees * static_cast<float>(M_PI) / 180.0f; }

float AngleBetween(float lhs, float rhs)
{
    return std::fabs(std::remainder(lhs - rhs, 2.0f * static_cast<float>(M_PI)));
}

// 60° on 10N from its spell_cone row, the other three ids have none
float BreathStepDeg(Unit* icehowl)
{
    float cone = ICEHOWL_BREATH_DEFAULT_CONE_DEG;
    uint32 const breath = sSpellMgr->GetSpellIdForDifficulty(SPELL_ARCTIC_BREATH, icehowl);
    if (SpellCone const* row = sSpellMgr->GetSpellCone(breath))
        if (row->cone_degrees > 0)
            cone = static_cast<float>(row->cone_degrees);

    return cone / 2.0f + ICEHOWL_SPREAD_MARGIN_DEG;
}

BreathBearing const* FindBearing(IcehowlBreathState const& state, int32 step)
{
    for (BreathBearing const& bearing : state.bearings)
        if (bearing.step == step)
            return &bearing;

    return nullptr;
}

BreathMember const* FindMember(IcehowlBreathState const& state, ObjectGuid const& guid)
{
    for (BreathMember const& member : state.roster)
        if (member.guid == guid)
            return &member;

    return nullptr;
}

float BearingAngle(IcehowlBreathState const& state, int32 step)
{
    return state.anchor + DegToRad(static_cast<float>(step) * state.stepDeg);
}

// The probe measured from the latched spot, and he may have drifted along the bearing since
float RoomLeft(IcehowlBreathState const& state, BreathBearing const& bearing, Unit* icehowl)
{
    float const angle = BearingAngle(state, bearing.step);
    float const drift = (icehowl->GetPositionX() - state.spot.GetPositionX()) * std::cos(angle) +
                        (icehowl->GetPositionY() - state.spot.GetPositionY()) * std::sin(angle);
    return bearing.room - drift;
}

bool RoomRanShort(IcehowlBreathState const& state, Unit* icehowl)
{
    return std::any_of(state.bearings.begin(), state.bearings.end(), [&state, icehowl](BreathBearing const& bearing)
                       { return RoomLeft(state, bearing, icehowl) < ICEHOWL_SPREAD_MIN_ROOM; });
}

// Past 90° off his back he parries melee and hastes his next swing
bool CanSeat(IcehowlBreathState const& state, BreathRole role, int32 step)
{
    return role != BreathRole::Melee ||
           static_cast<float>(std::abs(step)) * state.stepDeg <= ICEHOWL_SPREAD_MELEE_ARC_DEG + BEARING_EPSILON_DEG;
}

// Outward from his back, k = 0, 1, -1, 2, -2, ..., stopping short of his front by a step plus the turn
// a re-latch allows, so a breath on his victim misses every bearing until the next re-latch. A bearing
// the wall cuts short is skipped and the next one further round takes its place.
void ProbeBearings(Player* bot, Map* map, IcehowlBreathState& state)
{
    state.bearings.clear();

    float const step = state.stepDeg;
    uint32 const wanted =
        2 * static_cast<uint32>(std::floor(ICEHOWL_SPREAD_HALF_ARC_DEG / step + BEARING_EPSILON_DEG)) + 1;
    int32 const reach = static_cast<int32>(
        std::floor((180.0f - step - ICEHOWL_SPREAD_RELATCH_TURN_DEG) / step + BEARING_EPSILON_DEG));

    float const sx = state.spot.GetPositionX();
    float const sy = state.spot.GetPositionY();
    float const sz = state.spot.GetPositionZ();
    for (int32 i = 0; i <= 2 * reach && state.bearings.size() < wanted; ++i)
    {
        int32 const k = (i % 2) ? (i + 1) / 2 : -(i / 2);
        float const angle = BearingAngle(state, k);
        float x = sx + ICEHOWL_SPREAD_PROBE * std::cos(angle);
        float y = sy + ICEHOWL_SPREAD_PROBE * std::sin(angle);
        float z = sz;

        // From the bot: the floor is a gameobject, and the walk out takes a player's path over it
        float const room =
            map->CheckCollisionAndGetValidCoords(bot, sx, sy, sz, x, y, z) ? std::hypot(x - sx, y - sy) : 0.0f;
        if (room >= ICEHOWL_SPREAD_MIN_ROOM)
            state.bearings.push_back({k, room});
    }
}

void LatchBreath(Player* bot, Map* map, Unit* icehowl, float front, IcehowlBreathState& state)
{
    state.latched = true;
    state.spot = icehowl->GetPosition();
    state.front = front;
    state.anchor = Position::NormalizeOrientation(front + static_cast<float>(M_PI));
    state.stepDeg = BreathStepDeg(icehowl);
    ProbeBearings(bot, map, state);
    state.dealDirty = true;
}

BreathRole RoleOf(Player* member)
{
    if (PlayerbotAI::IsHeal(member))
        return BreathRole::Healer;

    return PlayerbotAI::IsRanged(member) ? BreathRole::Ranged : BreathRole::Melee;
}

// Humans can't be moved and tanks hold him
bool CanHoldBearing(Player* member, uint32 instanceId)
{
    return member && member->IsInWorld() && member->IsAlive() &&
           member->GetMapId() == TRIAL_OF_THE_CRUSADER_MAP_ID && member->GetInstanceId() == instanceId &&
           GET_PLAYERBOT_AI(member) && !IsBeastsTank(member);
}

void RebuildRoster(Player* bot, IcehowlBreathState& state, uint32 now)
{
    state.roster.clear();
    uint32 const instanceId = bot->GetInstanceId();
    auto const add = [&state, instanceId](Player* member)
    {
        if (CanHoldBearing(member, instanceId))
            state.roster.push_back({member->GetGUID(), RoleOf(member)});
    };

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            add(ref->GetSource());
    }
    else
        add(bot);

    std::sort(state.roster.begin(), state.roster.end(),
              [](BreathMember const& lhs, BreathMember const& rhs)
              {
                  if (lhs.role != rhs.role)
                      return lhs.role < rhs.role;

                  return lhs.guid < rhs.guid;
              });

    // Dead or gone. A bot coming back is dealt a bearing afresh.
    for (auto it = state.slots.begin(); it != state.slots.end();)
    {
        if (FindMember(state, it->first))
            ++it;
        else
            it = state.slots.erase(it);
    }

    state.rosterMs = now;
    state.rosterValid = true;
    state.dealDirty = true;
}

// Sticky: a bot keeps its bearing for as long as it's kept. Healers pick first, one to a bearing
// where they can, nearest 60° off his back; the rest fill the emptiest bearing nearest his back. A
// melee with no bearing near enough his back gets none, and `set behind` places it.
void DealBreath(IcehowlBreathState& state)
{
    size_t const count = state.bearings.size();
    if (!count)
        return;

    std::vector<uint32> occupants(count, 0);
    std::vector<uint32> healers(count, 0);
    auto const seat = [&occupants, &healers](size_t index, BreathRole role)
    {
        ++occupants[index];
        if (role == BreathRole::Healer)
            ++healers[index];
    };

    std::vector<BreathMember const*> unseated;
    for (BreathMember const& member : state.roster)
    {
        auto const slot = state.slots.find(member.guid);
        if (slot != state.slots.end())
        {
            auto const kept = std::find_if(state.bearings.begin(), state.bearings.end(),
                                           [&slot](BreathBearing const& bearing)
                                           { return bearing.step == slot->second; });
            if (kept != state.bearings.end() && CanSeat(state, member.role, kept->step))
            {
                seat(static_cast<size_t>(kept - state.bearings.begin()), member.role);
                continue;
            }

            state.slots.erase(slot);
        }

        unseated.push_back(&member);
    }

    float const stepDeg = state.stepDeg;
    auto const healerKey = [&](size_t index)
    {
        int32 const k = state.bearings[index].step;
        float const offBearing =
            std::fabs(static_cast<float>(std::abs(k)) * stepDeg - ICEHOWL_SPREAD_HEALER_BEARING_DEG);
        return std::make_tuple(healers[index], offBearing, occupants[index], k);
    };
    auto const otherKey = [&](size_t index)
    {
        int32 const k = state.bearings[index].step;
        return std::make_tuple(occupants[index], std::abs(k), k);
    };

    for (BreathMember const* member : unseated)
    {
        bool const healer = member->role == BreathRole::Healer;
        size_t best = count;
        for (size_t index = 0; index < count; ++index)
        {
            if (!CanSeat(state, member->role, state.bearings[index].step))
                continue;

            if (best == count || (healer ? healerKey(index) < healerKey(best) : otherKey(index) < otherKey(best)))
                best = index;
        }

        if (best == count)
            continue;

        state.slots[member->guid] = state.bearings[best].step;
        seat(best, member->role);
    }
}

// Off under a charge and in a heroic overlap, which the charge and worm rules own. A victim out of his
// reach means he's walking after it (Whirl, the end of a charge), and the layout would chase him.
bool IsBreathSpreadUp(PlayerbotAI* botAI, Unit* icehowl, Unit*& victim)
{
    Creature* creature = icehowl->ToCreature();
    if (!creature || creature->GetReactState() == REACT_PASSIVE)
        return false;

    if (GetBeastsStageMask(botAI) != BEASTS_STAGE_ICEHOWL)
        return false;

    Position start;
    Position end;
    if (IcehowlChargeLatched(botAI, start, end))
        return false;

    victim = icehowl->GetVictim();
    return victim && victim->ToPlayer() && icehowl->IsWithinMeleeRange(victim);
}

void TurnBreathOff(IcehowlBreathState& state)
{
    state.active = false;
    state.latched = false;
    state.arc = 0;
}

// Every bot's trigger, action and multiplier ask, so one read per instance per ms.
IcehowlBreathState* RefreshBreath(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Map* map = bot ? bot->FindMap() : nullptr;
    if (!map || map->GetId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId() || !bot->IsAlive())
        return nullptr;

    IcehowlBreathState& state = breathStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.memoValid && state.memoMs == now)
        return &state;

    state.memoMs = now;
    state.memoValid = true;

    Unit* icehowl = GetEngagedBeast(botAI, NorthrendBeast::Icehowl);
    if (!icehowl)
    {
        state.slots.clear();
        state.rosterValid = false;
        TurnBreathOff(state);
        return &state;
    }

    Unit* victim = nullptr;
    if (!IsBreathSpreadUp(botAI, icehowl, victim))
    {
        TurnBreathOff(state);
        return &state;
    }

    // A drift toward a wall gets re-probed before it eats into the ranged band, so every kept bearing
    // fits all three bands
    float const front = icehowl->GetAngle(victim);
    if (!state.latched || icehowl->GetExactDist2d(state.spot) > ICEHOWL_SPREAD_RELATCH_MOVE ||
        AngleBetween(front, state.front) > DegToRad(ICEHOWL_SPREAD_RELATCH_TURN_DEG) || RoomRanShort(state, icehowl))
        LatchBreath(bot, map, icehowl, front, state);

    // Walled in on every bearing. Stays latched, or the walls get probed again every ms until he moves.
    if (state.bearings.empty())
    {
        state.active = false;
        state.arc = 0;
        return &state;
    }

    if (!state.rosterValid || getMSTimeDiff(state.rosterMs, now) >= BREATH_ROSTER_INTERVAL_MS)
        RebuildRoster(bot, state, now);

    if (state.dealDirty)
    {
        DealBreath(state);
        state.dealDirty = false;
    }

    state.active = true;
    state.arc = static_cast<uint32>(state.bearings.size());
    return &state;
}

BreathBearing const* SlotOf(IcehowlBreathState const& state, ObjectGuid const& guid)
{
    if (!state.active)
        return nullptr;

    auto const slot = state.slots.find(guid);
    return slot != state.slots.end() ? FindBearing(state, slot->second) : nullptr;
}

// Clamp, don't chase: the bot keeps its own distance to him inside its band
bool ResolveBreathStand(PlayerbotAI* botAI, Player* bot, IcehowlBreathState const& state,
                        IcehowlBreathStand& stand, int32& step)
{
    BreathBearing const* bearing = SlotOf(state, bot->GetGUID());
    BreathMember const* member = FindMember(state, bot->GetGUID());
    Unit* icehowl = GetEngagedBeast(botAI, NorthrendBeast::Icehowl);
    if (!bearing || !member || !icehowl)
        return false;

    float bandMin = ICEHOWL_SPREAD_MELEE_MIN;
    float bandMax = ICEHOWL_SPREAD_MELEE_MAX;
    if (member->role == BreathRole::Healer)
    {
        bandMin = ICEHOWL_SPREAD_HEALER_MIN;
        bandMax = ICEHOWL_SPREAD_HEALER_MAX;
    }
    else if (member->role == BreathRole::Ranged)
    {
        bandMin = ICEHOWL_SPREAD_RANGED_MIN;
        bandMax = ICEHOWL_SPREAD_RANGED_MAX;
    }

    float const top = std::min(bandMax, RoomLeft(state, *bearing, icehowl) - 1.0f);
    if (top < bandMin)
        return false;

    float const distance = std::clamp(bot->GetExactDist2d(icehowl), bandMin, top);
    float const angle = BearingAngle(state, bearing->step);

    stand.spot = Position(icehowl->GetPositionX() + distance * std::cos(angle),
                          icehowl->GetPositionY() + distance * std::sin(angle), icehowl->GetPositionZ());
    stand.icehowl = icehowl->GetPosition();
    stand.bearing = angle;
    stand.bandMin = bandMin;
    stand.bandMax = top;
    step = bearing->step;
    return true;
}
}  // namespace

bool IcehowlChargeLatched(PlayerbotAI* botAI, Position& start, Position& end)
{
    IcehowlChargeState const* state = RefreshCharge(botAI);
    if (!state || !state->latched)
        return false;

    start = state->start;
    end = state->end;
    return true;
}

float DistanceToIcehowlCharge(PlayerbotAI* botAI, float x, float y)
{
    Position start;
    Position end;
    if (!IcehowlChargeLatched(botAI, start, end))
        return std::numeric_limits<float>::max();

    return DistanceToIcehowlLane(start, end, x, y);
}

float DistanceToIcehowlLane(Position const& start, Position const& end, float x, float y)
{
    float const sx = start.GetPositionX();
    float const sy = start.GetPositionY();
    float const dx = end.GetPositionX() - sx;
    float const dy = end.GetPositionY() - sy;
    float const lengthSq = dx * dx + dy * dy;

    float t = lengthSq > 0.0f ? ((x - sx) * dx + (y - sy) * dy) / lengthSq : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    return std::hypot(x - (sx + t * dx), y - (sy + t * dy));
}

bool IsIcehowlStaggered(Unit* icehowl) { return icehowl && icehowl->HasAura(SPELL_STAGGERED_DAZE); }

bool HasIcehowlFrothingRage(Unit* icehowl)
{
    return icehowl && icehowl->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_FROTHING_RAGE, icehowl));
}

bool GetIcehowlBreathStand(PlayerbotAI* botAI, IcehowlBreathStand& stand)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return false;

    IcehowlBreathState const* state = RefreshBreath(botAI);
    int32 step = 0;
    bool const found = state && ResolveBreathStand(botAI, bot, *state, stand, step);

    if (RaidObs::Active() && !IsBeastsTank(bot))
        RaidObs::NoteDerived(bot, "nb.spread", found ? std::to_string(step) : std::string("none"));

    return found;
}

bool IsOnIcehowlBreathStand(IcehowlBreathStand const& stand, float x, float y, float degrees, float yards)
{
    float const dx = x - stand.icehowl.GetPositionX();
    float const dy = y - stand.icehowl.GetPositionY();
    float const distance = std::hypot(dx, dy);
    if (distance < stand.bandMin - yards || distance > stand.bandMax + yards)
        return false;

    return AngleBetween(std::atan2(dy, dx), stand.bearing) <= DegToRad(degrees);
}

bool HasIcehowlBreathSlot(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    IcehowlBreathState const* state = bot ? RefreshBreath(botAI) : nullptr;
    return state && SlotOf(*state, bot->GetGUID());
}

}
