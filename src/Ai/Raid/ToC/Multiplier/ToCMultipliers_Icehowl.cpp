#include "ToCMultipliers_Icehowl.h"
#include "ToCActions_Icehowl.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Icehowl.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "ReachTargetActions.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

float IcehowlSuppressMovementDuringChargeMultiplier::GetValue(Action* action)
{
    bool const competingMove =
        dynamic_cast<CastReachTargetSpellAction*>(action) ||
        (dynamic_cast<MovementAction*>(action) && !dynamic_cast<IcehowlClearChargePathAction*>(action));
    if (!competingMove)
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return 1.0f;

    bool const chargePhase = HasMassiveCrashAura(bot) || (icehowl->IsInCombat() && !icehowl->GetVictim());
    if (!chargePhase)
        return 1.0f;

    constexpr float corridorHalfWidth = 14.0f;
    return IsBotInChargeCorridor(bot, icehowl, corridorHalfWidth) ? 0.0f : 1.0f;
}

void AddToCIcehowlMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new IcehowlSuppressMovementDuringChargeMultiplier(botAI));
}
