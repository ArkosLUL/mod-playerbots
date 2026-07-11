#ifndef PLAYERBOTS_ULDACTIONS_VEZAX_H
#define PLAYERBOTS_ULDACTIONS_VEZAX_H

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

class VezaxCheatAction : public Action
{
public:
    VezaxCheatAction(PlayerbotAI* ai) : Action(ai, "vezax cheat action") {}

    bool Execute(Event event) override;
};

class VezaxShadowCrashAction : public MovementAction
{
public:
    VezaxShadowCrashAction(PlayerbotAI* ai) : MovementAction(ai, "vezax shadow crash action") {}

    bool Execute(Event event) override;
};

class VezaxMarkOfTheFacelessAction : public MovementAction
{
public:
    VezaxMarkOfTheFacelessAction(PlayerbotAI* ai) : MovementAction(ai, "vezax mark of the faceless action") {}

    bool Execute(Event event) override;
};

class VezaxSaroniteVaporsAction : public MoveAwayFromCreatureAction
{
public:
    VezaxSaroniteVaporsAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "vezax saronite vapors action", NPC_VEZAX_SARONITE_VAPORS, 6.0f) {}
};

#endif
