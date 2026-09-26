#include "ToCMultipliers_Icehowl.h"

#include <string>

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "ToCActions_Icehowl.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Icehowl.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
bool CompetesWithChargeDodge(Action* action)
{
    if (dynamic_cast<MovementAction*>(action))
        return !dynamic_cast<AttackAction*>(action) && !dynamic_cast<IcehowlClearChargePathAction*>(action);

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
    if (!CompetesWithChargeDodge(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    Position start;
    Position end;
    return IcehowlChargeLatched(botAI, start, end) ? 0.0f : 1.0f;
}

void AddToCIcehowlMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new IcehowlChargeGuardMultiplier(botAI));
}
