#include "ToCMultipliers_Anubarak.h"
#include "ToCActions_Anubarak.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Anubarak.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"

using namespace TrialOfTheCrusaderHelpers;

float AnubarakControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!botAI->IsTank(bot))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::Anubarak))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingAnubarak =
        entry == static_cast<uint32>(ToCNpcs::NPC_ANUBARAK) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER);

    return tankingAnubarak ? 0.0f : 1.0f;
}

float AnubarakProtectSpikeKiteMultiplier::GetValue(Action* action)
{
    // The chase target commits fully to the kite: suppress formation/avoidance/chase and any other
    // movement so only the kite-to-Permafrost action drives this bot.
    bool const competingMove =
        dynamic_cast<CastReachTargetSpellAction*>(action) ||
        (dynamic_cast<MovementAction*>(action) && !dynamic_cast<AnubarakKiteSpikeToPermafrostAction*>(action));
    if (!competingMove)
        return 1.0f;

    if (!bot->HasAura(SPELL_MARK))
        return 1.0f;

    return ToCEncounterIsLive(botAI, ToCEncounter::Anubarak) ? 0.0f : 1.0f;
}

void AddToCAnubarakMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new AnubarakControlTankMovementMultiplier(botAI));
    multipliers.push_back(new AnubarakProtectSpikeKiteMultiplier(botAI));
}

// Lust is saved for the phase 3 Leeching Swarm burn
ToCBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI)
{
    return {true, AnubarakLeechingSwarmActive(botAI)};
}
