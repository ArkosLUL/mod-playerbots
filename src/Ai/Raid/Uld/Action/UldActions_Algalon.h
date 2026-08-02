#ifndef PLAYERBOTS_ULDACTIONS_ALGALON_H
#define PLAYERBOTS_ULDACTIONS_ALGALON_H

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
// Algalon the Observer
//
class AlgalonCosmicSmashAction : public MovementAction
{
public:
    AlgalonCosmicSmashAction(PlayerbotAI* ai) : MovementAction(ai, "algalon cosmic smash action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AlgalonBigBangHideAction : public MovementAction
{
public:
    AlgalonBigBangHideAction(PlayerbotAI* ai) : MovementAction(ai, "algalon big bang hide action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AlgalonBigBangSoakAction : public Action
{
public:
    AlgalonBigBangSoakAction(PlayerbotAI* ai) : Action(ai, "algalon big bang soak action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AlgalonPhasePunchSwapAction : public AttackAction
{
public:
    AlgalonPhasePunchSwapAction(PlayerbotAI* ai) : AttackAction(ai, "algalon phase punch swap action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AlgalonConstellationKiteAction : public MovementAction
{
public:
    AlgalonConstellationKiteAction(PlayerbotAI* ai) : MovementAction(ai, "algalon constellation kite action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AlgalonDarkMatterMarkAction : public Action
{
public:
    AlgalonDarkMatterMarkAction(PlayerbotAI* ai) : Action(ai, "algalon dark matter mark action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class AlgalonCollapsingStarMarkAction : public Action
{
public:
    AlgalonCollapsingStarMarkAction(PlayerbotAI* ai) : Action(ai, "algalon collapsing star mark action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
