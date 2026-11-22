#include "ToCMultipliers_Gormok.h"

#include <string>

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Gormok.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
bool IsBlinkOrDisengage(Action* action)
{
    if (!dynamic_cast<CastSpellAction*>(action))
        return false;

    std::string const name = action->getName();
    return name == "blink" || name == "disengage";
}

bool StaysFree(std::string const& name)
{
    if (name == "avoid aoe")
        return true;

    ToCEncounter encounter = ToCEncounter::None;
    return ToCEncounterOfName(name, encounter) && encounter == ToCEncounter::NorthrendBeasts;
}
}

float GormokSnoboldCarrierMultiplier::GetValue(Action* action)
{
    auto const classify = [action]
    {
        Facts facts;
        facts.spellMover = IsBlinkOrDisengage(action);
        facts.mover = dynamic_cast<MovementAction*>(action) && !dynamic_cast<AttackAction*>(action);
        facts.staysFree = !facts.spellMover && facts.mover && StaysFree(action->getName());
        return facts;
    };

    Facts const& facts = _facts.Get(action, classify);
    if (!facts.spellMover && !facts.mover)
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts) || !GetGormokForSnoboldCarrier(botAI))
        return 1.0f;

    if (facts.spellMover)
        return 0.0f;

    return facts.staysFree ? 1.0f : 0.0f;
}
