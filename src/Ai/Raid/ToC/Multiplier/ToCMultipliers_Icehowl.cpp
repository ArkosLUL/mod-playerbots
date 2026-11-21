#include "ToCMultipliers_Icehowl.h"

#include <string>

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "ToCActions_Icehowl.h"
#include "ToCActions_Jormungars.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Icehowl.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// The pool walks mind a latched lane themselves, and the crash stuns a bot wherever it stands, pool or not
bool CompetesWithChargeDodge(Action* action)
{
    if (dynamic_cast<MovementAction*>(action))
        return !dynamic_cast<AttackAction*>(action) && !dynamic_cast<IcehowlClearChargePathAction*>(action) &&
               !dynamic_cast<NorthrendWormsLeavePoolAction*>(action) &&
               !dynamic_cast<NorthrendWormsRepositionAction*>(action);

    if (!dynamic_cast<CastSpellAction*>(action))
        return false;

    if (dynamic_cast<CastReachTargetSpellAction*>(action))
        return true;

    // Both throw the bot a fixed distance that can land it in the line. By name, so no class headers.
    std::string const name = action->getName();
    return name == "blink" || name == "disengage";
}
}  // namespace

float IcehowlChargeGuardMultiplier::GetValue(Action* action)
{
    if (!_competes.Get(action, [action] { return CompetesWithChargeDodge(action); }))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    return IcehowlChargeCycleActive(botAI) ? 0.0f : 1.0f;
}

// SetBehindTargetAction is a CombatFormationMoveAction too
float IcehowlBreathSpreadGuardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    return HasIcehowlBreathSlot(botAI) ? 0.0f : 1.0f;
}
