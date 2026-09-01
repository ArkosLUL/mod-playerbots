#ifndef PLAYERBOTS_ULDTRIGGERS_HODIR_H
#define PLAYERBOTS_ULDTRIGGERS_HODIR_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Hodir
//

// The bot is carrying Biting Cold and has no Toasty Fire to shed it for free. Deliberately holds no
// state: the raid position trigger instantiates this one to decide whether to stand down, and
// anything latched here would be lost by that stack-allocated copy. The stack threshold lives on the
// action.
class HodirBitingColdTrigger : public Trigger
{
public:
    HodirBitingColdTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir biting cold") {}
    bool IsActive() override;
};

// A Snowpacked Icicle Target is up, and this bot is not inside the Safe Area it radiates. That NPC
// is the only Flash Freeze exemption in the fight - a Toasty Fire does not grant one.
class HodirNearSnowpackedIcicleTrigger : public Trigger
{
public:
    HodirNearSnowpackedIcicleTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir near snowpacked icicle") {}
    bool IsActive() override;
};

// This bot is the paladin that should be running Frost Resistance Aura, and is not running it yet.
// The shared BossFrostResistanceTrigger is deliberately not used here: it takes the first paladin in
// group order whatever its spec, so a three-paladin raid can lose Devotion off the tank or
// Concentration off a healer, and it raises the aura through a ChangeStrategy that nothing removes.
class HodirFrostResistanceTrigger : public Trigger
{
public:
    HodirFrostResistanceTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir frost resistance trigger") {}
    bool IsActive() override;
};

// An icicle is about to land on or beside this bot. Small icicles always count; a drift icicle only
// counts while it is still falling, because once it lands it becomes the shelter everyone needs.
class HodirIcicleDodgeTrigger : public Trigger
{
public:
    HodirIcicleDodgeTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir icicle dodge") {}
    bool IsActive() override;
};

// The bot is off its anchor and nothing more urgent wants the tick.
class HodirRaidPositionTrigger : public Trigger
{
public:
    HodirRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir raid position") {}
    bool IsActive() override;
};

// Non-tank, non-healer DPS pick their own target: trapped raiders first, then helper blocks, then
// the boss.
class HodirSetDpsPriorityTrigger : public Trigger
{
public:
    HodirSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir set dps priority") {}
    bool IsActive() override;
};

// Frozen Blows is up and the wrong tank is holding him.
class HodirFrozenBlowsSwapTrigger : public Trigger
{
public:
    HodirFrozenBlowsSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir frozen blows swap") {}
    bool IsActive() override;
};

// The bot carries Storm Cloud, so it has 4-6 seconds to hand Storm Power to as much of the raid as
// it can reach.
class HodirSpreadStormCloudTrigger : public Trigger
{
public:
    HodirSpreadStormCloudTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir spread storm cloud") {}
    bool IsActive() override;
};

// A hunter or rogue that could hand its threat to a tank. Screened here as well as in the action's
// isUseful so the other twenty bots never reach the node.
class HodirRedirectThreatTrigger : public Trigger
{
public:
    HodirRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir redirect threat") {}
    bool IsActive() override;
};

#endif
