#include "ToCMultipliers_Gormok.h"

#include <string>

#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Gormok.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// Both fire on a carrier that close to Gormok and throw it back out of his melee. By name, so no class
// headers.
bool IsBlinkOrDisengage(Action* action)
{
    if (!dynamic_cast<CastSpellAction*>(action))
        return false;

    std::string const name = action->getName();
    return name == "blink" || name == "disengage";
}
}  // namespace

float GormokSnoboldTargetGuardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DpsAssistAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    return GetGormokSnoboldPick(botAI) ? 0.0f : 1.0f;
}

float GormokSnoboldCarrierMultiplier::GetValue(Action* action)
{
    bool const spellMover = IsBlinkOrDisengage(action);
    // AttackAction is a MovementAction too, and zeroing it would strand the carrier without a target
    if (!spellMover && (!dynamic_cast<MovementAction*>(action) || dynamic_cast<AttackAction*>(action)))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts) || !GetGormokForSnoboldCarrier(botAI))
        return 1.0f;

    if (spellMover)
        return 0.0f;

    std::string const name = action->getName();
    if (name == "avoid aoe")
        return 1.0f;

    // The beasts' own movers stay, the carrier walk included: on heroic a carrier can still have a
    // worm or Icehowl's charge to dodge
    ToCEncounter encounter = ToCEncounter::None;
    if (ToCEncounterOfTrigger(name, encounter) && encounter == ToCEncounter::NorthrendBeasts)
        return 1.0f;

    return 0.0f;
}

void AddToCGormokMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new GormokSnoboldTargetGuardMultiplier(botAI));
    multipliers.push_back(new GormokSnoboldCarrierMultiplier(botAI));
}
