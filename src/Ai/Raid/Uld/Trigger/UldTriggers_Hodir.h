#ifndef PLAYERBOTS_ULDTRIGGERS_HODIR_H
#define PLAYERBOTS_ULDTRIGGERS_HODIR_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Hodir
//

// Biting Cold only stacks on a bot that has held still through four ticks, so any movement clears
// it. Ranged rarely reach this trigger because dodging icicles already keeps them moving; it exists
// for the 12-24s after each Flash Freeze, when the small icicles are switched off and the Toasty
// Fires have just been wiped, and for tanks, who are rarely icicle targets.
class HodirBitingColdTrigger : public Trigger
{
public:
    HodirBitingColdTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir biting cold") {}
    bool IsActive() override;

private:
    uint32 _stillSince = 0;
};

// A Snowpacked Icicle Target is up, and this bot is not inside the Safe Area it radiates. That NPC
// is the only Flash Freeze exemption in the fight - a Toasty Fire does not grant one.
class HodirNearSnowpackedIcicleTrigger : public Trigger
{
public:
    HodirNearSnowpackedIcicleTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir near snowpacked icicle") {}
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

#endif
