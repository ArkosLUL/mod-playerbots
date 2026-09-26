#include "ToCMultipliers_Jaraxxus.h"
#include "ToCActions_Jaraxxus.h"
#include "ToCData.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Jaraxxus.h"
#include "EncounterHelpers.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

float JaraxxusControlTankMovementMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CombatFormationMoveAction*>(action))
        return 1.0f;

    if (!PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsTank(bot, true))
        return 1.0f;

    // open, not live: his adds outlive the kill and still need holding
    if (!ToCEncounterGateOpen(botAI, ToCEncounter::Jaraxxus))
        return 1.0f;

    Unit* victim = bot->GetVictim();
    if (!victim)
        return 1.0f;

    bool const held = victim->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_JARAXXUS) || IsJaraxxusAdd(victim);
    return held ? 0.0f : 1.0f;
}

float JaraxxusTauntGuardMultiplier::GetValue(Action* action)
{
    if (!action || !IsTauntAction(bot, action))
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || target->GetEntry() != static_cast<uint32>(ToCNpcs::NPC_JARAXXUS))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::Jaraxxus))
        return 1.0f;

    Unit* victim = target->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holder || holder == bot)
        return 1.0f;

    return (PlayerbotAI::IsTank(holder) || PlayerbotAI::IsTank(holder, true)) ? 0.0f : 1.0f;
}

float JaraxxusKissCastHoldMultiplier::GetValue(Action* action)
{
    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    if (!cast)
        return 1.0f;

    if (!HasMistressKiss(bot))
        return 1.0f;

    // open, not live: heroic Mistresses outlive the kill and keep kissing
    if (!ToCEncounterGateOpen(botAI, ToCEncounter::Jaraxxus))
        return 1.0f;

    return JaraxxusSpellHasCastTime(botAI, cast->getSpell()) ? 0.0f : 1.0f;
}

float JaraxxusIntroHoldMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<MovementAction*>(action) || dynamic_cast<JaraxxusIntroMainTankStandAction*>(action))
        return 1.0f;

    // no live gate: he isn't engaged until the release. The scan is cheaper than walking the group
    if (!GetJaraxxusInIntro(botAI))
        return 1.0f;

    return botAI->IsMainTank(bot) ? 0.0f : 1.0f;
}

float JaraxxusAvoidAoeGuardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    if (!ToCEncounterGateOpen(botAI, ToCEncounter::Jaraxxus))
        return 1.0f;

    return JaraxxusAnyLegionFlame(botAI) ? 0.0f : 1.0f;
}

void AddToCJaraxxusMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new JaraxxusControlTankMovementMultiplier(botAI));
    multipliers.push_back(new JaraxxusTauntGuardMultiplier(botAI));
    multipliers.push_back(new JaraxxusKissCastHoldMultiplier(botAI));
    multipliers.push_back(new JaraxxusIntroHoldMultiplier(botAI));
    multipliers.push_back(new JaraxxusAvoidAoeGuardMultiplier(botAI));
}

// Lust at the pull: he never enrages, and the heroic portal at 20 s still falls inside it.
ToCBurstWindow ToCJaraxxusBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {};
}
