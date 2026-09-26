#include "ToCMultipliers_NorthrendBeasts.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Icehowl.h"
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

    BeastsTankDuty const duty = GetDutyOfBeast(botAI, AI_VALUE(Unit*, "current target"));
    if (duty == BeastsTankDuty::None)
        return 1.0f;

    return GetBeastsDutyHolder(botAI, duty) == bot ? 1.0f : 0.0f;
}

void AddToCNorthrendBeastsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new NorthrendBeastsControlTankMovementMultiplier(botAI));
    multipliers.push_back(new NorthrendBeastsTauntGuardMultiplier(botAI));
}

// Everything goes into Icehowl's daze (+100% damage taken for 15 s). Lust is one 10 minute cooldown,
// so it's held through Gormok and the worms or it's gone by then. The rest come back mid fight, so
// they stay free on those two.
ToCBurstWindow ToCNorthrendBeastsBurstWindow(PlayerbotAI* botAI)
{
    ToCBurstWindow window;
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
