#ifndef PLAYERBOTS_ULDACTIONS_HODIR_H
#define PLAYERBOTS_ULDACTIONS_HODIR_H

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

// Run to the Snowpacked Icicle Target the rest of the raid is running to. Standing inside its Safe
// Area is the only way to survive Flash Freeze.
class HodirMoveSnowpackedIcicleAction : public MovementAction
{
public:
    HodirMoveSnowpackedIcicleAction(PlayerbotAI* botAI) : MovementAction(botAI, "hodir move snowpacked icicle") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Step out from under a falling icicle, preferring a destination that is still inside Starlight.
class HodirIcicleDodgeAction : public MovementAction
{
public:
    HodirIcicleDodgeAction(PlayerbotAI* botAI) : MovementAction(botAI, "hodir icicle dodge action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Jump on the spot to shed Biting Cold. A jump counts as movement, which is what the aura checks,
// and unlike walking it does not take the bot anywhere.
class HodirBitingColdJumpAction : public MovementAction
{
public:
    HodirBitingColdJumpAction(PlayerbotAI* ai) : MovementAction(ai, "hodir biting cold jump") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hold the bot's anchor: a fixed corner spot for the two tanks, a ring slot for ranged and healers.
class HodirRaidPositionAction : public MovementAction
{
public:
    HodirRaidPositionAction(PlayerbotAI* ai) : MovementAction(ai, "hodir raid position action") {}
    bool Execute(Event event) override;

private:
    bool _anchorReached = false;
};

// Trapped raiders, then flash-frozen helpers, then the boss.
class HodirSetDpsPriorityAction : public AttackAction
{
public:
    HodirSetDpsPriorityAction(PlayerbotAI* ai) : AttackAction(ai, "hodir set dps priority action") {}
    bool Execute(Event event) override;

private:
    Unit* ResolveTarget(Unit* currentTarget);
};

// Taunt Hodir off the tank Frozen Blows would kill, and take him back when it drops.
class HodirFrozenBlowsSwapAction : public AttackAction
{
public:
    HodirFrozenBlowsSwapAction(PlayerbotAI* ai) : AttackAction(ai, "hodir frozen blows swap action") {}
    bool Execute(Event event) override;
};

// Carry Storm Cloud around the ranged ring so Storm Power lands on as much of the raid as its 4-6
// one-second ticks reach.
class HodirSpreadStormCloudAction : public MovementAction
{
public:
    HodirSpreadStormCloudAction(PlayerbotAI* ai) : MovementAction(ai, "hodir spread storm cloud") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    int8 _direction = 0;
};

#endif
