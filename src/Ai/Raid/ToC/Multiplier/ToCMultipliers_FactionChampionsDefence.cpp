#include "ToCMultipliers_FactionChampionsDefence.h"
#include "ToCActions_FactionChampions.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "GenericSpellActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "ShamanActions.h"

using namespace TrialOfTheCrusaderHelpers;

float FactionChampionsAoeGuardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<ReachMeleeAction*>(action) && !dynamic_cast<SetBehindTargetAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    return FactionChampionsReachIntoAoe(botAI) ? 0.0f : 1.0f;
}

float FactionChampionsPhysicalSwitchGuardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<FactionChampionsFocusPriorityAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    return FactionChampionsPhysicalSwitchTarget(botAI) ? 0.0f : 1.0f;
}

float FactionChampionsDispelGuardMultiplier::GetValue(Action* action)
{
    CastSpellAction* cure = dynamic_cast<CastCureSpellAction*>(action);
    if (!cure)
        cure = dynamic_cast<CurePartyMemberAction*>(action);

    // Cleanse removes magic too, whichever aura type its node aimed at
    if (!cure || (cure->getSpell() != "dispel magic" && cure->getSpell() != "cleanse"))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    Unit* target = action->GetTarget();
    return target && FactionChampionsDispelBackfires(botAI, target) ? 0.0f : 1.0f;
}

float FactionChampionsPurgeGuardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastPurgeAction*>(action) && !dynamic_cast<CastSpellstealAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    Unit* target = action->GetTarget();
    return target && target->IsCreature() && IsFactionChampion(target->GetEntry()) ? 0.0f : 1.0f;
}

void AddToCFactionChampionsDefenceMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FactionChampionsAoeGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsPhysicalSwitchGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsDispelGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsPurgeGuardMultiplier(botAI));
}
