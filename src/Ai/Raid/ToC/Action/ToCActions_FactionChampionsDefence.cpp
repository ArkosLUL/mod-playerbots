#include "ToCActions_FactionChampionsDefence.h"

#include <vector>

#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "Timer.h"
#include "ToCHelpers_FactionChampionsDefence.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
constexpr float AOE_SEARCH_RADIUS = 30.0f;
constexpr float AOE_ARRIVE = 1.5f;
// how long a latched spot is trusted while the walk to it is in flight
constexpr uint32 AOE_LATCH_MS = 3000;
// standing still this long past the issue means something stopped the walk short
constexpr uint32 AOE_STALL_MS = 500;

bool IsClearOf(Position const& spot, std::vector<HazardCircle> const& hazards)
{
    for (HazardCircle const& hazard : hazards)
        if (spot.GetExactDist2d(hazard.first) < hazard.second)
            return false;

    return true;
}

// FindNearestPositionClearOfHazards answers Position() when nothing is clear
bool IsNoSpot(Position const& spot) { return !spot.GetPositionX() && !spot.GetPositionY(); }

// a FORCED leg keeps its lock for its walk time, the same window MoveTo's gate waits out
bool ForcedLegHolds(LastMovement const& last)
{
    return last.priority == MovementPriority::MOVEMENT_FORCED && last.lastdelayTime + last.msTime > getMSTime();
}
}  // namespace

bool FactionChampionsAvoidAoeAction::IssueLeg(Position const& spot, uint32 now)
{
    // our last leg still holds the FORCED lock and a lock only yields to a higher priority, so step
    // it down for the replacement and put it back if nothing went out
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const replacing =
        last.priority == MovementPriority::MOVEMENT_FORCED &&
        last.lastMoveShort.GetExactDist2d(_lastIssued.GetPositionX(), _lastIssued.GetPositionY()) < 1.0f;
    if (replacing)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    RaidObs::MoveOutcome const outcome =
        TryMoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_FORCED);

    if (outcome == RaidObs::MoveOutcome::Issued)
    {
        _lastIssued = spot;
        _spot = spot;
        _hasSpot = true;
        _issuedMs = now;
        return true;
    }

    if (replacing)
        last.priority = MovementPriority::MOVEMENT_FORCED;

    // already walking there
    if (outcome == RaidObs::MoveOutcome::Duplicate)
    {
        _spot = spot;
        _hasSpot = true;
        _issuedMs = now;
        return true;
    }

    _hasSpot = false;
    return false;
}

bool FactionChampionsAvoidAoeAction::Execute(Event /*event*/)
{
    if (!FactionChampionsInAoe(botAI))
    {
        _hasSpot = false;
        return false;
    }

    // a running cast pins the feet, the spline never launches while it lasts
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    std::vector<HazardCircle> const clearances = FactionChampionsAoeClearances(botAI);
    uint32 const now = getMSTime();

    // every MoveTo clears the motion master, so re-deriving under a walk in flight would restart it
    // each tick. Still re-check the spot, a bladestorming warrior keeps walking
    if (_hasSpot)
    {
        uint32 const age = getMSTimeDiff(_issuedMs, now);
        bool const arrived = bot->GetExactDist2d(&_spot) <= AOE_ARRIVE;
        bool const stalled = !arrived && !bot->isMoving() && age > AOE_STALL_MS;

        if (!arrived && !stalled && age < AOE_LATCH_MS && IsClearOf(_spot, clearances))
            return true;

        _hasSpot = false;

        // MoveTo refuses the same point for 5 s, moving or not
        if (stalled)
            AI_VALUE(LastMovement&, "last movement").clear();
    }

    Position const spot = FindNearestPositionClearOfHazards(bot, clearances, AOE_SEARCH_RADIUS);
    if (IsNoSpot(spot))
        return false;

    return IssueLeg(spot, now);
}

bool FactionChampionsMassDispelAction::Execute(Event /*event*/)
{
    Unit* target = FactionChampionsMassDispelTarget(botAI);
    if (!target)
        return false;

    // CastSpell refuses any cast-time spell while the bot walks. A FORCED leg is a dodge, let it finish
    if (bot->isMoving())
    {
        LastMovement& last = AI_VALUE(LastMovement&, "last movement");
        if (ForcedLegHolds(last))
            return false;

        bot->StopMoving();
        last.clear();
    }

    return botAI->CastSpell("mass dispel", target);
}

// A queued basket can pop ticks after its trigger fired, once the shield is gone
bool FactionChampionsMassDispelAction::isUseful() { return FactionChampionsMassDispelTarget(botAI) != nullptr; }

bool FactionChampionsDispelCcAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Unit* member = FactionChampionsDispelCcTarget(botAI, spell);
    return member && spell && botAI->CastSpell(spell, member);
}

// A queued basket can pop ticks after its trigger fired, once the CC is off or another bot took it
bool FactionChampionsDispelCcAction::isUseful()
{
    char const* spell = nullptr;
    return FactionChampionsDispelCcTarget(botAI, spell) != nullptr;
}

bool FactionChampionsPhysicalSwitchAction::Execute(Event /*event*/)
{
    Unit* target = FactionChampionsPhysicalSwitchTarget(botAI);
    if (!target || AI_VALUE(Unit*, "current target") == target)
        return false;

    return Attack(target);
}

bool FactionChampionsPurgeKillTargetAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Unit* killTarget = FactionChampionsPurgeTarget(botAI, spell);
    return killTarget && spell && botAI->CastSpell(spell, killTarget);
}

// A queued basket can pop ticks after its trigger fired, once the buff is gone
bool FactionChampionsPurgeKillTargetAction::isUseful()
{
    char const* spell = nullptr;
    return FactionChampionsPurgeTarget(botAI, spell) != nullptr;
}
