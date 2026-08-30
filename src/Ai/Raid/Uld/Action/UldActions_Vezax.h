#ifndef PLAYERBOTS_ULDACTIONS_VEZAX_H
#define PLAYERBOTS_ULDACTIONS_VEZAX_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldEncounter_Vezax.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class VezaxResetEncounterStateAction : public Action
{
public:
    VezaxResetEncounterStateAction(PlayerbotAI* ai) : Action(ai, "vezax reset encounter state action") {}

    bool Execute(Event event) override;
};

class VezaxMarkOfTheFacelessAction : public MovementAction
{
public:
    VezaxMarkOfTheFacelessAction(PlayerbotAI* ai) : MovementAction(ai, "vezax mark of the faceless action") {}

    bool Execute(Event event) override;
};

class VezaxVaporPuddleClearAction : public MovementAction
{
public:
    VezaxVaporPuddleClearAction(PlayerbotAI* ai) : MovementAction(ai, "vezax vapor puddle clear action") {}

    bool Execute(Event event) override;
};

// Step out of where the missile in flight will land. Per bot rather than as a group: clearing a
// 8 yd block from a 10 yd blast by moving it as one needs about 18 yd, which is the whole 2.6s of
// flight with nothing left for the walk back.
class VezaxShadowCrashDodgeAction : public MovementAction
{
public:
    VezaxShadowCrashDodgeAction(PlayerbotAI* ai) : MovementAction(ai, "vezax shadow crash dodge action") {}

    bool Execute(Event event) override;
};

class VezaxSearingFlamesInterruptAction : public Action
{
public:
    VezaxSearingFlamesInterruptAction(PlayerbotAI* ai)
        : Action(ai, "vezax searing flames interrupt action")
    {
    }

    bool Execute(Event event) override;
};

class VezaxSurgeOfDarknessAction : public Action
{
public:
    VezaxSurgeOfDarknessAction(PlayerbotAI* ai) : Action(ai, "vezax surge of darkness action") {}

    bool Execute(Event event) override;
};

// Hard mode: switch to and kill the invulnerability-granting Saronite Animus.
class VezaxSaroniteAnimusAction : public AttackAction
{
public:
    VezaxSaroniteAnimusAction(PlayerbotAI* ai) : AttackAction(ai, "vezax saronite animus action") {}

    bool Execute(Event event) override;
};

class VezaxVaporSoakAction : public MovementAction
{
public:
    VezaxVaporSoakAction(PlayerbotAI* ai) : MovementAction(ai, "vezax vapor soak action") {}

    bool Execute(Event event) override;
};

class VezaxKillVaporAction : public AttackAction
{
public:
    VezaxKillVaporAction(PlayerbotAI* ai) : AttackAction(ai, "vezax kill vapor action") {}

    bool Execute(Event event) override;
};

// Walk into the nearest Shadow Crash field and hold it.
class VezaxShadowCrashSoakAction : public MovementAction
{
public:
    VezaxShadowCrashSoakAction(PlayerbotAI* ai) : MovementAction(ai, "vezax shadow crash soak action") {}

    bool Execute(Event event) override;
};

class VezaxRaidPositionAction : public MovementAction
{
public:
    VezaxRaidPositionAction(PlayerbotAI* ai) : MovementAction(ai, "vezax raid position action") {}

    bool Execute(Event event) override;

private:
    bool _slotReached = false;
};

#endif
