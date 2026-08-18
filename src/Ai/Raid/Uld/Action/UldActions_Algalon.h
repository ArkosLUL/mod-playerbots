#ifndef PLAYERBOTS_ULDACTIONS_ALGALON_H
#define PLAYERBOTS_ULDACTIONS_ALGALON_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

//
// Algalon the Observer
//
class AlgalonResetEncounterStateAction : public Action
{
public:
    AlgalonResetEncounterStateAction(PlayerbotAI* ai) : Action(ai, "algalon reset encounter state action") {}
    bool Execute(Event event) override;
};

class AlgalonBigBangHideAction : public MovementAction
{
public:
    AlgalonBigBangHideAction(PlayerbotAI* ai) : MovementAction(ai, "algalon big bang hide action") {}
    bool Execute(Event event) override;
};

class AlgalonBigBangSoakAction : public Action
{
public:
    AlgalonBigBangSoakAction(PlayerbotAI* ai) : Action(ai, "algalon big bang soak action") {}
    bool Execute(Event event) override;
};

class AlgalonCosmicSmashAction : public MovementAction
{
public:
    AlgalonCosmicSmashAction(PlayerbotAI* ai) : MovementAction(ai, "algalon cosmic smash action") {}
    bool Execute(Event event) override;
};

class AlgalonLeaveBlackHoleAction : public MovementAction
{
public:
    AlgalonLeaveBlackHoleAction(PlayerbotAI* ai) : MovementAction(ai, "algalon leave black hole action") {}
    bool Execute(Event event) override;
};

class AlgalonPhasePunchSwapAction : public AttackAction
{
public:
    AlgalonPhasePunchSwapAction(PlayerbotAI* ai) : AttackAction(ai, "algalon phase punch swap action") {}
    bool Execute(Event event) override;
};

class AlgalonConstellationTauntAction : public AttackAction
{
public:
    AlgalonConstellationTauntAction(PlayerbotAI* ai) : AttackAction(ai, "algalon constellation taunt action") {}
    bool Execute(Event event) override;
};

class AlgalonConstellationKiteAction : public MovementAction
{
public:
    AlgalonConstellationKiteAction(PlayerbotAI* ai) : MovementAction(ai, "algalon constellation kite action") {}
    bool Execute(Event event) override;

private:
    bool _spotReached = false;
};

class AlgalonCollapsingStarFocusAction : public Action
{
public:
    AlgalonCollapsingStarFocusAction(PlayerbotAI* ai) : Action(ai, "algalon collapsing star focus action") {}
    bool Execute(Event event) override;
};

class AlgalonDarkMatterTankAction : public AttackAction
{
public:
    AlgalonDarkMatterTankAction(PlayerbotAI* ai) : AttackAction(ai, "algalon dark matter tank action") {}
    bool Execute(Event event) override;
};

class AlgalonDarkMatterMarkAction : public Action
{
public:
    AlgalonDarkMatterMarkAction(PlayerbotAI* ai) : Action(ai, "algalon dark matter mark action") {}
    bool Execute(Event event) override;
};

class AlgalonRaidPositionAction : public MovementAction
{
public:
    AlgalonRaidPositionAction(PlayerbotAI* ai) : MovementAction(ai, "algalon raid position action") {}
    bool Execute(Event event) override;

private:
    bool _slotReached = false;
};

#endif
