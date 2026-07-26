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
        : MoveAwayFromCreatureAction(botAI, "ignis scorched ground action", NPC_IGNIS_SCORCHED_GROUND, 8.0f) {}
};

class IgnisIronConstructAction : public Action
{
public:
    IgnisIronConstructAction(PlayerbotAI* botAI) : Action(botAI, "ignis iron construct action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
