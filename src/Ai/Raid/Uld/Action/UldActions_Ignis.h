#ifndef PLAYERBOTS_ULDACTIONS_IGNIS_H
#define PLAYERBOTS_ULDACTIONS_IGNIS_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldTriggers.h"
#include "Vehicle.h"

//
// Ignis the Furnace Master
//
class IgnisScorchedGroundAction : public MovementAction
{
public:
    static constexpr char const* Name = "ignis scorched ground action";

    IgnisScorchedGroundAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Holds the boss on a three-slot arc around the room centre. Scorch summons its patch 20 yd along
// the boss's facing and the boss faces the main tank, so this is what picks where every patch in the
// fight lands - away from both water pools, where they light, and away from the raid.
class IgnisMainTankPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "ignis main tank position action";

    IgnisMainTankPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    bool _spotReached = false;
};

// Walks an activated construct through the only chain that kills it: Scorched Ground until it turns
// Molten, then this tank's own water pool until it turns Brittle.
class IgnisConstructTankAction : public AttackAction
{
public:
    static constexpr char const* Name = "ignis construct tank action";

    IgnisConstructTankAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// One hit of 5000 (10-man) / 3000 (25-man) shatters a Brittle construct, and the blast is what
// actually kills it - the raid's damage never would.
class IgnisAttackBrittleConstructAction : public AttackAction
{
public:
    static constexpr char const* Name = "ignis attack brittle construct action";

    IgnisAttackBrittleConstructAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name)
    {
    }
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IgnisAttackBossAction : public AttackAction
{
public:
    static constexpr char const* Name = "ignis attack boss action";

    IgnisAttackBossAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IgnisMoltenConstructAvoidAction : public MovementAction
{
public:
    static constexpr char const* Name = "ignis molten construct avoid action";

    IgnisMoltenConstructAvoidAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name)
    {
    }
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Flame Jets knocks the whole raid back and locks casting for six seconds afterwards, so a cast that
// cannot land before it hits is thrown away either way.
class IgnisFlameJetsHoldCastAction : public Action
{
public:
    static constexpr char const* Name = "ignis flame jets hold cast action";

    IgnisFlameJetsHoldCastAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IgnisSlagPotHealAction : public Action
{
public:
    static constexpr char const* Name = "ignis slag pot heal action";

    IgnisSlagPotHealAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
