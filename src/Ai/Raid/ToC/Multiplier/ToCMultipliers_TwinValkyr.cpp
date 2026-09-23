#include "ToCMultipliers_TwinValkyr.h"
#include "ToCActions_TwinValkyr.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_TwinValkyr.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"

using namespace TrialOfTheCrusaderHelpers;

float TwinValkyrControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!botAI->IsTank(bot))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::TwinValkyr))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingTwin =
        entry == static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE);

    return tankingTwin ? 0.0f : 1.0f;
}

float TwinValkyrPrioritizeEssenceSwapMultiplier::GetValue(Action* action)
{
    // Commit fully to the portal run: suppress formation/avoidance/chase and any other movement so only
    // the essence-swap actions drive this bot.
    bool const competingMove =
        dynamic_cast<CastReachTargetSpellAction*>(action) ||
        (dynamic_cast<MovementAction*>(action) && !dynamic_cast<TwinValkyrEssenceActionBase*>(action));
    if (!competingMove)
        return 1.0f;

    // Mirror the vortex/touch triggers: only non-tanks swap, and only on a genuine colour mismatch.
    if (botAI->IsTank(bot))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::TwinValkyr))
        return 1.0f;

    bool const needSwap =
        (HasLightTouch(bot) && !HasLightEssence(bot)) ||
        (HasDarkTouch(bot) && !HasDarkEssence(bot)) ||
        (TwinValkyrLightVortexActive(botAI) && !HasLightEssence(bot)) ||
        (TwinValkyrDarkVortexActive(botAI) && !HasDarkEssence(bot));

    return needSwap ? 0.0f : 1.0f;
}

void AddToCTwinValkyrMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new TwinValkyrControlTankMovementMultiplier(botAI));
    multipliers.push_back(new TwinValkyrPrioritizeEssenceSwapMultiplier(botAI));
}

ToCBurstWindow ToCTwinValkyrBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {};
}
