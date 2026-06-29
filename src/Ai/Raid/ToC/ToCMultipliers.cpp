#include "ToCMultipliers.h"
#include "ToCActions.h"
#include "ToCHelpers.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ReachTargetActions.h"

using namespace TrialOfTheCrusaderHelpers;

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

float NorthrendBeastsControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!botAI->IsTank(bot))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingBeast =
        entry == static_cast<uint32>(ToCNpcs::NPC_GORMOK) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_ACIDMAW) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_DREADSCALE) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_ICEHOWL);

    if (!tankingBeast)
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    return 1.0f;
}

float JaraxxusControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!botAI->IsTank(bot))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingJaraxxus =
        entry == static_cast<uint32>(ToCNpcs::NPC_JARAXXUS) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_MISTRESS_OF_PAIN) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_FEL_INFERNAL);

    if (!tankingJaraxxus)
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    return 1.0f;
}
