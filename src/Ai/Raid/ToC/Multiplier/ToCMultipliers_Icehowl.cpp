#include "ToCMultipliers_Icehowl.h"
#include "ToCActions_Icehowl.h"
#include "ToCData.h"
#include "ToCHelpers_Icehowl.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "ReachTargetActions.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

float IcehowlSuppressMovementDuringChargeMultiplier::GetValue(Action* action)
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return 1.0f;

    bool const chargePhase = HasMassiveCrashAura(bot) || (icehowl->IsInCombat() && !icehowl->GetVictim());
    if (!chargePhase)
        return 1.0f;

    constexpr float corridorHalfWidth = 14.0f;
    if (!IsBotInChargeCorridor(bot, icehowl, corridorHalfWidth))
        return 1.0f;

    if (dynamic_cast<CastReachTargetSpellAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action) ||
        (dynamic_cast<MovementAction*>(action) &&
         !dynamic_cast<IcehowlClearChargePathAction*>(action)))
    {
        return 0.0f;
    }

    return 1.0f;
}

void AddToCIcehowlMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new IcehowlSuppressMovementDuringChargeMultiplier(botAI));
}
