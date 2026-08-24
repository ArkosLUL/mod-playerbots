#ifndef PLAYERBOTS_ULDACTIONS_HODIR_H
#define PLAYERBOTS_ULDACTIONS_HODIR_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidRedirectThreat.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

// None of the actions in this file override isUseful. Every one of these is reached from exactly one
// trigger node, which is already the gate, and the obvious implementation - constructing that trigger
// on the stack - silently breaks any trigger that keeps per-bot state.

// Run to the Snowpacked Icicle Target the rest of the raid is running to. Standing inside its Safe
// Area is the only way to survive Flash Freeze.
class HodirMoveSnowpackedIcicleAction : public MovementAction
{
public:
    HodirMoveSnowpackedIcicleAction(PlayerbotAI* botAI) : MovementAction(botAI, "hodir move snowpacked icicle") {}
    bool Execute(Event event) override;
};

// Raise Frost Resistance Aura. Cast directly rather than through ChangeStrategy: the shared boss
// resistance node adds "rfrost" and nothing anywhere removes it, so the pick outlives the encounter
// and is wrong on the next pull. HodirPaladinAuraMultiplier holds the slot open while this runs.
class HodirFrostResistanceAction : public Action
{
public:
    HodirFrostResistanceAction(PlayerbotAI* botAI) : Action(botAI, "hodir frost resistance action") {}
    bool Execute(Event event) override;
};

// Step out from under an icicle that has not detonated yet.
class HodirIcicleDodgeAction : public MovementAction
{
public:
    HodirIcicleDodgeAction(PlayerbotAI* botAI) : MovementAction(botAI, "hodir icicle dodge action") {}
    bool Execute(Event event) override;

private:
    Position _dest;
    // Closest the bot has got to _dest so far, so a drag away from it can be told from ordinary walking.
    float _destDist = 0.0f;
};

// Shed Biting Cold by moving. A stack only comes off on the second moving tick and any stationary
// tick resets that progress, so this chains 6 yd legs until the aura is gone rather than hopping.
class HodirBitingColdShedAction : public MovementAction
{
public:
    HodirBitingColdShedAction(PlayerbotAI* ai) : MovementAction(ai, "hodir biting cold shed") {}
    bool Execute(Event event) override;

private:
    bool _shedding = false;
};

// Hold the bot's anchor: a fixed corner spot for the two tanks, a formation slot for ranged and
// healers.
class HodirRaidPositionAction : public MovementAction
{
public:
    HodirRaidPositionAction(PlayerbotAI* ai) : MovementAction(ai, "hodir raid position action") {}
    bool Execute(Event event) override;
};

// Trapped raiders, then flash-frozen helpers, then the boss.
class HodirSetDpsPriorityAction : public AttackAction
{
public:
    HodirSetDpsPriorityAction(PlayerbotAI* ai) : AttackAction(ai, "hodir set dps priority action") {}
    bool Execute(Event event) override;

private:
    Unit* ResolveTarget();
};

// Taunt Hodir off the tank Frozen Blows would kill, and take him back when it drops.
class HodirFrozenBlowsSwapAction : public AttackAction
{
public:
    HodirFrozenBlowsSwapAction(PlayerbotAI* ai) : AttackAction(ai, "hodir frozen blows swap action") {}
    bool Execute(Event event) override;
};

// Feed Misdirection and Tricks of the Trade to whichever tank is holding Hodir right now. A taunt
// sets threat equal to the top of the table rather than above it, so the swap leaves the incoming
// tank at parity with the best DPS every time and the redirect is what buys back a lead.
class HodirRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    HodirRedirectThreatAction(PlayerbotAI* ai) : RaidRedirectThreatAction(ai, "hodir redirect threat action") {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

// Carry Storm Cloud around the ranged formation so Storm Power lands on as much of the raid as its
// 4-6 one-second ticks reach.
class HodirSpreadStormCloudAction : public MovementAction
{
public:
    HodirSpreadStormCloudAction(PlayerbotAI* ai) : MovementAction(ai, "hodir spread storm cloud") {}
    bool Execute(Event event) override;

private:
    int8 _direction = 0;
    // Storm Cloud only ever loses stacks, so a rise means a fresh carry and a fresh lap direction.
    uint8 _lastStacks = 0;
};

#endif
