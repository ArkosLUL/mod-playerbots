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
    static constexpr char const* Name = "hodir biting cold";

    HodirBitingColdTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// A Snowpacked Icicle Target is up, and this bot is not inside the Safe Area it radiates. That NPC
// is the only Flash Freeze exemption in the fight - a Toasty Fire does not grant one.
class HodirNearSnowpackedIcicleTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir near snowpacked icicle";

    HodirNearSnowpackedIcicleTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// This bot is the paladin that should be running Frost Resistance Aura, and is not running it yet.
// The shared BossFrostResistanceTrigger is deliberately not used here: it takes the first paladin in
// group order whatever its spec, so a three-paladin raid can lose Devotion off the tank or
// Concentration off a healer, and it raises the aura through a ChangeStrategy that nothing removes.
class HodirFrostResistanceTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir frost resistance trigger";

    HodirFrostResistanceTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// An icicle is about to land on or beside this bot. Small icicles always count; a drift icicle only
// counts while it is still falling, because once it lands it becomes the shelter everyone needs.
class HodirIcicleDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir icicle dodge";

    HodirIcicleDodgeTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// The bot is off its anchor and nothing more urgent wants the tick.
class HodirRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir raid position";

    HodirRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Non-tank, non-healer DPS pick their own target: trapped raiders first, then helper blocks, then
// the boss.
class HodirSetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir set dps priority";

    HodirSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Frozen Blows is up and the wrong tank is holding him.
class HodirFrozenBlowsSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir frozen blows swap";

    HodirFrozenBlowsSwapTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// The bot carries Storm Cloud, so it holds its rally point until the charges are spent.
class HodirSpreadStormCloudTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir spread storm cloud";

    HodirSpreadStormCloudTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Somebody nearby is carrying, this bot deals damage, and it has not been handed Storm Power yet.
class HodirCollectStormPowerTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir collect storm power";

    HodirCollectStormPowerTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// A hunter or rogue that could hand its threat to a tank. Screened here as well as in the action's
// isUseful so the other twenty bots never reach the node.
class HodirRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir redirect threat";

    HodirRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

#endif
