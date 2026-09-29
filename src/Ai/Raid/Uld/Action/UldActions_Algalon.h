#ifndef PLAYERBOTS_ULDACTIONS_ALGALON_H
#define PLAYERBOTS_ULDACTIONS_ALGALON_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldTriggers.h"
#include "Vehicle.h"

//
// Algalon the Observer
//
class AlgalonBigBangSoakAction : public Action
{
public:
    static constexpr char const* Name = "algalon big bang soak action";

    AlgalonBigBangSoakAction(PlayerbotAI* ai) : Action(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonBigBangExternalAction : public Action
{
public:
    static constexpr char const* Name = "algalon big bang external action";

    AlgalonBigBangExternalAction(PlayerbotAI* ai) : Action(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonBigBangHideAction : public MovementAction
{
public:
    static constexpr char const* Name = "algalon big bang hide action";

    AlgalonBigBangHideAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonCosmicSmashAction : public MovementAction
{
public:
    static constexpr char const* Name = "algalon cosmic smash action";

    AlgalonCosmicSmashAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonLeaveBlackHoleAction : public MovementAction
{
public:
    static constexpr char const* Name = "algalon leave black hole action";

    AlgalonLeaveBlackHoleAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}
    bool Execute(Event event) override;
};

// Taunts Algalon onto this tank, and walks to him if the taunt can't reach.
class AlgalonTakeBossAction : public AttackAction
{
public:
    AlgalonTakeBossAction(PlayerbotAI* ai, char const* name) : AttackAction(ai, name) {}
    bool Execute(Event event) override;
};

class AlgalonTankPickupAction : public AlgalonTakeBossAction
{
public:
    static constexpr char const* Name = "algalon tank pickup action";

    AlgalonTankPickupAction(PlayerbotAI* ai) : AlgalonTakeBossAction(ai, Name) {}
};

class AlgalonPhasePunchSwapAction : public AlgalonTakeBossAction
{
public:
    static constexpr char const* Name = "algalon phase punch swap action";

    AlgalonPhasePunchSwapAction(PlayerbotAI* ai) : AlgalonTakeBossAction(ai, Name) {}
};

class AlgalonConstellationTauntAction : public AttackAction
{
public:
    static constexpr char const* Name = "algalon constellation taunt action";

    AlgalonConstellationTauntAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonDarkMatterTankAction : public AttackAction
{
public:
    static constexpr char const* Name = "algalon dark matter tank action";

    AlgalonDarkMatterTankAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonConstellationKiteAction : public MovementAction
{
public:
    static constexpr char const* Name = "algalon constellation kite action";

    AlgalonConstellationKiteAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}
    bool Execute(Event event) override;

private:
    bool _spotReached = false;
};

class AlgalonStarTeamAction : public AttackAction
{
public:
    static constexpr char const* Name = "algalon star team action";

    AlgalonStarTeamAction(PlayerbotAI* ai) : AttackAction(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonStarMarkAction : public Action
{
public:
    static constexpr char const* Name = "algalon star mark action";

    AlgalonStarMarkAction(PlayerbotAI* ai) : Action(ai, Name) {}
    bool Execute(Event event) override;
};

class AlgalonRaidPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "algalon raid position action";

    AlgalonRaidPositionAction(PlayerbotAI* ai) : MovementAction(ai, Name) {}
    bool Execute(Event event) override;

private:
    bool _slotReached = false;
};

#endif
