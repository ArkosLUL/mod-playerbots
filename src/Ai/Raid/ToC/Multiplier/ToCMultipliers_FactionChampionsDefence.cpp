#include "ToCMultipliers_FactionChampionsDefence.h"
#include "ToCActions_FactionChampions.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "GenericSpellActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PetsAction.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "ShamanActions.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

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

float FactionChampionsGripGuardMultiplier::GetValue(Action* action)
{
    bool const petToggle = dynamic_cast<TogglePetSpellAutoCastAction*>(action);
    CastSpellAction* cast = petToggle ? nullptr : dynamic_cast<CastSpellAction*>(action);
    if (!petToggle && !cast)
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions) || !FactionChampionsGripGuardActive(botAI))
        return 1.0f;

    if (petToggle)
        return 0.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", cast->getSpell());
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info || !FactionChampionsSpellHoldsTarget(spellId))
        return 1.0f;

    if (info->IsAffectingArea() || info->IsTargetingArea() || info->IsPositive())
        return 0.0f;

    // GetTarget last, it scans every attacker
    return FactionChampionsGripGuarded(botAI, action->GetTarget()) ? 0.0f : 1.0f;
}

void AddToCFactionChampionsDefenceMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FactionChampionsGripGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsAoeGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsPhysicalSwitchGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsDispelGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsPurgeGuardMultiplier(botAI));
}
