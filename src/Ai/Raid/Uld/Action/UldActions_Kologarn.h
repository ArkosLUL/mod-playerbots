#ifndef PLAYERBOTS_ULDACTIONS_KOLOGARN_H
#define PLAYERBOTS_ULDACTIONS_KOLOGARN_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class KologarnBodyTankAction : public AttackAction
{
public:
    KologarnBodyTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "kologarn body tank action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KologarnOffTankAction : public AttackAction
{
public:
    KologarnOffTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "kologarn off tank action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KologarnRubbleTankAction : public AttackAction
{
public:
    KologarnRubbleTankAction(PlayerbotAI* botAI) : AttackAction(botAI, "kologarn rubble tank action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KologarnDpsTargetAction : public AttackAction
{
public:
    KologarnDpsTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, "kologarn dps target action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KologarnSmashSwapAction : public AttackAction
{
public:
    KologarnSmashSwapAction(PlayerbotAI* botAI) : AttackAction(botAI, "kologarn smash swap action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KologarnBodyUncoveredAction : public AttackAction
{
public:
    KologarnBodyUncoveredAction(PlayerbotAI* botAI) : AttackAction(botAI, "kologarn body uncovered action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KologarnFallFromFloorAction : public Action
{
public:
    KologarnFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, "kologarn fall from floor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KologarnRubbleSlowdownAction : public Action
{
public:
    KologarnRubbleSlowdownAction(PlayerbotAI* botAI) : Action(botAI, "kologarn rubble slowdown action") {}
    bool Execute(Event event) override;
};

class KologarnEyebeamAction : public MovementAction
{
public:
    KologarnEyebeamAction(PlayerbotAI* botAI) : MovementAction(botAI, "kologarn eyebeam action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
