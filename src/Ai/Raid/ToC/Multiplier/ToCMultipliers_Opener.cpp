#include "ToCMultipliers_Opener.h"

#include "Action.h"
#include "AttackAction.h"
#include "Playerbots.h"
#include "RaidRedirectThreat.h"
#include "ToCHelpers_Opener.h"
#include "ToCHelpers_Shared.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// Area damage from this close counts as hitting the boss
constexpr float OPENER_AREA_THREAT_REACH = 30.0f;
}  // namespace

float ToCOpenerHoldMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<RaidRedirectThreatAction*>(action) || !ToCOpenerHolds(botAI))
        return 1.0f;

    Unit* const current = AI_VALUE(Unit*, "current target");
    Unit* target = action->GetTarget();
    if (!target)
        target = current;

    if (dynamic_cast<AttackAction*>(action) && !dynamic_cast<MeleeAction*>(action))
        return target == current && IsToCOpenerHeld(botAI, target) ? 0.0f : 1.0f;

    if (IsToCOpenerHeld(botAI, target))
        return 0.0f;

    return IsToCAreaDamage(botAI, action) && GetToCOpenerHeldBossNear(botAI, OPENER_AREA_THREAT_REACH) ? 0.0f
                                                                                                    : 1.0f;
}
