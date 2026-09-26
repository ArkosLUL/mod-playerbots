#include "ToCHelpers_Icehowl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "Creature.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
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

}
