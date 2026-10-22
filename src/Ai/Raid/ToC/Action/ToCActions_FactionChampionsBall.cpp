#include "ToCActions_FactionChampionsBall.h"

#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsBall.h"
#include "ToCHelpers_FactionChampionsDefence.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
constexpr float HOLD_ARRIVE = 2.0f;
constexpr float STAND_ARRIVE = 1.5f;
// a walk already headed here, so a reissue would only restart its spline
constexpr float SAME_SPOT = 0.5f;

bool WalkingTo(Position const* walking, Position const& spot)
{
    return walking && walking->GetExactDist2d(&spot) < SAME_SPOT;
}
}  // namespace

bool FactionChampionsHoldAction::Execute(Event /*event*/)
{
    return AttackDirect(FactionChampionsHoldOf(botAI).champion);
}

bool FactionChampionsHoldWalkAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsHoldWalk(botAI, spot) || bot->GetExactDist2d(&spot) <= HOLD_ARRIVE)
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    return WalkTo(spot);
}

bool FactionChampionsHoldControlAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* champion = FactionChampionsHoldControl(botAI, spellId);
    return champion && botAI->CastSpell(spellId, champion);
}

bool FactionChampionsBallStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsBallStand(botAI, spot) || bot->GetExactDist2d(&spot) <= STAND_ARRIVE)
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    return WalkTo(spot);
}

bool FactionChampionsChasedAction::Execute(Event /*event*/)
{
    Position spot;
    if (!FactionChampionsChased(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (WalkingTo(WalkInFlight(), spot))
        return true;

    // a cast in progress pins the feet
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (!bot->HasAura(EncounterHelpers::NITRO_BOOSTS_SPEED_SPELL))
        EncounterHelpers::UseNitroBoosts(bot);

    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED);
}

bool FactionChampionsOpenerAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* target = FactionChampionsOpener(botAI, spellId);
    return target && botAI->CastSpell(spellId, target);
}
