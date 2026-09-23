#include "ToCMultipliers_Jaraxxus.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "MovementActions.h"
#include "Playerbots.h"

using namespace TrialOfTheCrusaderHelpers;

float JaraxxusControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!botAI->IsTank(bot))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::Jaraxxus))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    uint32 const entry = victim->GetEntry();
    bool const tankingJaraxxus =
        entry == static_cast<uint32>(ToCNpcs::NPC_JARAXXUS) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_MISTRESS_OF_PAIN) ||
        entry == static_cast<uint32>(ToCNpcs::NPC_FEL_INFERNAL);

    return tankingJaraxxus ? 0.0f : 1.0f;
}

void AddToCJaraxxusMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new JaraxxusControlTankMovementMultiplier(botAI));
}

ToCBurstWindow ToCJaraxxusBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {};
}
