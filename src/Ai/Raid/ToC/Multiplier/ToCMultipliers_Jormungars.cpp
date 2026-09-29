#include "ToCMultipliers_Jormungars.h"

#include <string>

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"

using namespace TrialOfTheCrusaderHelpers;

namespace
{
// Spells that move the bot: the charge family leaps it back to its target, blink and disengage throw
// it a fixed distance. The last two by name, so no class headers.
bool IsSpellMover(Action* action)
{
    if (!dynamic_cast<CastSpellAction*>(action))
        return false;

    if (dynamic_cast<CastReachTargetSpellAction*>(action))
        return true;

    std::string const name = action->getName();
    return name == "blink" || name == "disengage";
}
}  // namespace

float NorthrendWormsMoveGuardMultiplier::GetValue(Action* action)
{
    bool const spellMover = IsSpellMover(action);
    // AttackAction is a MovementAction too, and zeroing it would leave the bot without a target
    if (!spellMover && (!dynamic_cast<MovementAction*>(action) || dynamic_cast<AttackAction*>(action)))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts) || !IsWormWalkInFlight(botAI))
        return 1.0f;

    if (spellMover)
        return 0.0f;

    std::string const name = action->getName();
    if (name == "avoid aoe")
        return 1.0f;

    // The reposition walk itself, and on heroic Gormok's walks and Icehowl's charge dodge
    ToCEncounter encounter = ToCEncounter::None;
    if (ToCEncounterOfTrigger(name, encounter) && encounter == ToCEncounter::NorthrendBeasts)
        return 1.0f;

    return 0.0f;
}

float NorthrendWormsBileReachGuardMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<ReachMeleeAction*>(action) && !dynamic_cast<CastReachTargetSpellAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    return IsWormBileCarrierHeldOut(botAI) ? 0.0f : 1.0f;
}

float NorthrendWormsRedirectGuardMultiplier::GetValue(Action* action)
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return 1.0f;

    bool const classRedirect = dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
                               dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action) ||
                               dynamic_cast<CastTricksOfTheTradeAction*>(action);
    if (!classRedirect)
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts))
        return 1.0f;

    return (GetBeastsStageMask(botAI) & BEASTS_STAGE_WORMS) ? 0.0f : 1.0f;
}

void AddToCJormungarsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new NorthrendWormsMoveGuardMultiplier(botAI));
    multipliers.push_back(new NorthrendWormsBileReachGuardMultiplier(botAI));
    multipliers.push_back(new NorthrendWormsRedirectGuardMultiplier(botAI));
}
