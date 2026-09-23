#include "ToCMultipliers_NorthrendBeasts.h"
#include "ToCData.h"
#include "MovementActions.h"
#include "Playerbots.h"

using namespace TrialOfTheCrusaderHelpers;

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

void AddToCNorthrendBeastsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new NorthrendBeastsControlTankMovementMultiplier(botAI));
}

ToCBurstWindow ToCNorthrendBeastsBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {};
}
