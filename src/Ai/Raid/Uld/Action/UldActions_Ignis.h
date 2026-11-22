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

class IgnisScorchedGroundAction : public MovementAction
{
public:
    static constexpr char const* Name = "ignis scorched ground action";

    IgnisScorchedGroundAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

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

class IgnisConstructTankAction : public AttackAction
{
public:
    static constexpr char const* Name = "ignis construct tank action";

    IgnisConstructTankAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

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
