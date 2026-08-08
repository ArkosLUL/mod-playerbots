#ifndef PLAYERBOTS_ULDACTIONS_IGNIS_H
#define PLAYERBOTS_ULDACTIONS_IGNIS_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

//
// Ignis the Furnace Master
//
class IgnisScorchedGroundAction : public MoveAwayFromCreatureAction
{
public:
    IgnisScorchedGroundAction(PlayerbotAI* botAI)
        : MoveAwayFromCreatureAction(botAI, "ignis scorched ground action", NPC_IGNIS_SCORCHED_GROUND,
                                     ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS)
    {
    }
};

// Walks an activated construct through the only chain that kills it: Scorched Ground until it turns
// Molten, then the nearest water pool until it turns Brittle.
class IgnisConstructTankAction : public AttackAction
{
public:
    IgnisConstructTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "ignis construct tank action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IgnisBrittleConstructMarkAction : public Action
{
public:
    IgnisBrittleConstructMarkAction(PlayerbotAI* botAI) : Action(botAI, "ignis brittle construct mark action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IgnisMoltenConstructAvoidAction : public MovementAction
{
public:
    IgnisMoltenConstructAvoidAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "ignis molten construct avoid action")
    {
    }
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IgnisSlagPotHealAction : public Action
{
public:
    IgnisSlagPotHealAction(PlayerbotAI* botAI) : Action(botAI, "ignis slag pot heal action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
