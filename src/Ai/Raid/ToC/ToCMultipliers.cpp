#include "ToCMultipliers.h"
#include "ToCActions.h"
#include "ToCHelpers.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ReachTargetActions.h"
#include "ShamanActions.h"

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

float AnubarakControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!botAI->IsTank(bot))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingAnubarak =
        entry == static_cast<uint32>(ToCNpcs::NPC_ANUBARAK) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER);

    if (!tankingAnubarak)
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    return 1.0f;
}

float AnubarakProtectSpikeKiteMultiplier::GetValue(Action* action)
{
    if (!bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MARK)))
        return 1.0f;

    // The chase target commits fully to the kite: suppress formation/avoidance/chase and any other
    // movement so only the kite-to-Permafrost action drives this bot.
    if (dynamic_cast<CastReachTargetSpellAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action) ||
        (dynamic_cast<MovementAction*>(action) &&
         !dynamic_cast<AnubarakKiteSpikeToPermafrostAction*>(action)))
    {
        return 0.0f;
    }

    return 1.0f;
}

float AnubarakDelayBloodlustUntilLeechingSwarmMultiplier::GetValue(Action* action)
{
    // Only gate Bloodlust during the Anub'arak encounter; the other ToC bosses share this strategy
    // and must not have their Lust/Heroism suppressed.
    bool const inAnubarakFight =
        GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ANUBARAK)) || AnubarakSubmerged(botAI);
    if (!inAnubarakFight)
        return 1.0f;

    // Once the boss is in the Leeching Swarm phase, allow Bloodlust/Heroism for the burn
    if (AnubarakLeechingSwarmActive(botAI))
        return 1.0f;

    if (dynamic_cast<CastBloodlustAction*>(action) ||
        dynamic_cast<CastHeroismAction*>(action))
    {
        return 0.0f;
    }

    return 1.0f;
}

float FactionChampionsSuppressAoeMultiplier::GetValue(Action* action)
{
    // Only gate AoE while champions are alive; the other ToC bosses share this strategy and must keep
    // their AoE (e.g. Northrend Beasts add waves, Anub'arak burrowers/scarabs).
    if (!action || !FactionChampionsEncounterActive(botAI))
        return 1.0f;

    // Champions stack a damage-reduction aura when several are hit by the same AoE, so bots single-
    // target. AoE heals are exempt so healers can still raid-heal through the pile.
    if (action->getThreatType() == Action::ActionThreatType::Aoe && !dynamic_cast<CastHealingSpellAction*>(action))
        return 0.0f;

    return 1.0f;
}

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
