#include "ToCMultipliers_TwinValkyr.h"
#include "ToCActions_TwinValkyr.h"
#include "ToCData.h"
#include "ToCHelpers_TwinValkyr.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "ReachTargetActions.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

float TwinValkyrControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!botAI->IsTank(bot))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingTwin =
        entry == static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE);

    if (!tankingTwin)
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    return 1.0f;
}

float TwinValkyrPrioritizeEssenceSwapMultiplier::GetValue(Action* action)
{
    // Mirror the vortex/touch triggers: only non-tanks swap, and only on a genuine colour mismatch.
    if (botAI->IsTank(bot) || !TwinValkyrEncounterActive(botAI))
        return 1.0f;

    bool const needSwap =
        (TwinValkyrLightVortexActive(botAI) && !HasLightEssence(bot)) ||
        (TwinValkyrDarkVortexActive(botAI) && !HasDarkEssence(bot)) ||
        (bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_LIGHT_TOUCH)) && !HasLightEssence(bot)) ||
        (bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_DARK_TOUCH)) && !HasDarkEssence(bot));
    if (!needSwap)
        return 1.0f;

    // Commit fully to the portal run: suppress formation/avoidance/chase and any other movement so only
    // the essence-swap actions drive this bot.
    if (dynamic_cast<CastReachTargetSpellAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action) ||
        (dynamic_cast<MovementAction*>(action) &&
         !dynamic_cast<TwinValkyrEssenceActionBase*>(action)))
    {
        return 0.0f;
    }

    return 1.0f;
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
