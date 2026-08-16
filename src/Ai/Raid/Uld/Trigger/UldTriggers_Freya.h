#ifndef PLAYERBOTS_ULDTRIGGERS_FREYA_H
#define PLAYERBOTS_ULDTRIGGERS_FREYA_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Freya
//
class FreyaNearNatureBombTrigger : public Trigger
{
public:
    FreyaNearNatureBombTrigger(PlayerbotAI* ai) : Trigger(ai, "freya near nature bomb") {}
    bool IsActive() override;
};

// Owns every DPS bot's target for the whole encounter, so the trio wave can be split three ways.
class FreyaSetDpsPriorityTrigger : public Trigger
{
public:
    FreyaSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "freya set dps priority") {}
    bool IsActive() override;
};

// Assist tank 0 holds the Snaplasher, whose Hardened Bark ramps its damage the more it is struck.
class FreyaTankAddsTrigger : public Trigger
{
public:
    FreyaTankAddsTrigger(PlayerbotAI* ai) : Trigger(ai, "freya tank adds") {}
    bool IsActive() override;
};

// Detonating Lashers blow up for ~4-7k on death and cannot be tanked, so bots that would not
// survive the blast step outside it.
class FreyaAvoidDetonatingLasherTrigger : public Trigger
{
public:
    FreyaAvoidDetonatingLasherTrigger(PlayerbotAI* ai) : Trigger(ai, "freya avoid detonating lasher") {}
    bool IsActive() override;
};

class FreyaMoveToHealingSporeTrigger : public Trigger
{
public:
    FreyaMoveToHealingSporeTrigger(PlayerbotAI* ai) : Trigger(ai, "freya move to healing spore trigger") {}
    bool IsActive() override;
};

// Hard mode: bot is trapped by Iron Roots (Ironbranch or Freya-cast) and must break out.
class FreyaBreakIronRootsTrigger : public Trigger
{
public:
    FreyaBreakIronRootsTrigger(PlayerbotAI* ai) : Trigger(ai, "freya break iron roots") {}
    bool IsActive() override;
};

// Hard mode: bot is standing in an Unstable Sun Beam and must step out before it detonates.
class FreyaDodgeUnstableSunBeamTrigger : public Trigger
{
public:
    FreyaDodgeUnstableSunBeamTrigger(PlayerbotAI* ai) : Trigger(ai, "freya dodge unstable sun beam") {}
    bool IsActive() override;
};

#endif
