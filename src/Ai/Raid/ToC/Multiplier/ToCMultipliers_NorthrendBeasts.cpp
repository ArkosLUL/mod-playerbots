#include "ToCMultipliers_NorthrendBeasts.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "EncounterHelpers.h"
#include "MovementActions.h"
#include "Playerbots.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
constexpr float ICEHOWL_LUST_HEALTH_PCT = 50.0f;
constexpr float ICEHOWL_BURST_HEALTH_PCT = 30.0f;
}

float NorthrendBeastsControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!IsBeastsTank(bot))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    return GetDutyOfBeast(botAI, bot->GetVictim()) != BeastsTankDuty::None ? 0.0f : 1.0f;
}

float NorthrendBeastsTauntGuardMultiplier::GetValue(Action* action)
{
    if (!action || !EncounterHelpers::IsTauntAction(bot, action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    BeastsTankDuty const duty = GetDutyOfBeast(botAI, target);
    if (duty == BeastsTankDuty::None)
        return 1.0f;

    if (GetBeastsDutyHolder(botAI, duty) != bot)
        return 0.0f;

    return IsWormSpewingAtOtherTank(botAI, target) ? 0.0f : 1.0f;
}

RaidBurstWindow ToCNorthrendBeastsBurstWindow(PlayerbotAI* botAI)
{
    RaidBurstWindow window;
    Unit* icehowl = GetEngagedBeast(botAI, NorthrendBeast::Icehowl);
    if (!icehowl)
    {
        window.allowLust = false;
        return window;
    }

    bool const staggered = IsIcehowlStaggered(icehowl);
    float const health = icehowl->GetHealthPct();
    window.allowLust = staggered || health <= ICEHOWL_LUST_HEALTH_PCT;
    window.allowAll = staggered || health <= ICEHOWL_BURST_HEALTH_PCT;
    return window;
}
