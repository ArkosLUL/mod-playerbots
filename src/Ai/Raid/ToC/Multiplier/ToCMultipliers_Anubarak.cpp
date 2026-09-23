#include "ToCMultipliers_Anubarak.h"
#include "ToCActions_Anubarak.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "ReachTargetActions.h"
#include "ShamanActions.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

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

void AddToCAnubarakMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new AnubarakControlTankMovementMultiplier(botAI));
    multipliers.push_back(new AnubarakProtectSpikeKiteMultiplier(botAI));
    multipliers.push_back(new AnubarakDelayBloodlustUntilLeechingSwarmMultiplier(botAI));
}

ToCBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {};
}
