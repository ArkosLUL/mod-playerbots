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
// Fallbacks for a raid that never lands a clean charge. The daze is the real window.
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

// The encounter's own taunts cast inside Execute, so this never sees them.
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

    // a flagged main tank's "lose aggro" taunt fires whenever another tank has its worm
    return IsWormSpewingAtOtherTank(botAI, target) ? 0.0f : 1.0f;
}

// Lust is one 10 min cooldown, so it waits for Icehowl's daze (+100% damage taken for 15 s). The rest come
// back mid fight, so they stay free on Gormok and the worms.
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
