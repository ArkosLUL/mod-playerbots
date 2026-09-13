#ifndef PLAYERBOTS_ULDACTIONS_VEZAX_H
#define PLAYERBOTS_ULDACTIONS_VEZAX_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Vezax.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class VezaxResetEncounterStateAction : public Action
{
public:
    VezaxResetEncounterStateAction(PlayerbotAI* ai) : Action(ai, "vezax reset encounter state action") {}

    bool Execute(Event event) override;
};

// Carry the mark away from everyone else. The leech skips whoever holds it, so this is not an escape
// from its own damage - it is getting ten seconds of 5000-a-tick off the people standing nearby.
class VezaxMarkOfTheFacelessAction : public MovementAction
{
public:
    VezaxMarkOfTheFacelessAction(PlayerbotAI* ai) : MovementAction(ai, "vezax mark of the faceless action") {}

    bool Execute(Event event) override;
};

// The other side of it: step out of the leech around someone else's mark.
class VezaxMarkOfTheFacelessBreakAction : public MovementAction
{
public:
    VezaxMarkOfTheFacelessBreakAction(PlayerbotAI* ai)
        : MovementAction(ai, "vezax mark of the faceless break action")
    {
    }

    bool Execute(Event event) override;
};

// Step out of where the missile in flight will land. Each bot walks its own group's strafe, so the
// group arrives with its shape intact and the impact keeps its bearing relative to all of them.
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
