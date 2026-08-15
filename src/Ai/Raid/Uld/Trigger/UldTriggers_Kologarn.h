#ifndef PLAYERBOTS_ULDTRIGGERS_KOLOGARN_H
#define PLAYERBOTS_ULDTRIGGERS_KOLOGARN_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Kologarn
//
// Targeting is decided per role in code, with no raid target icons: Skull means "everyone DPS this"
// and Moon is the CC channel, so borrowing them for a per-role split corrupts the generic engine
// behaviour. Same model as the Eredar Twins in SWP.
//
class KologarnBodyTankTrigger : public Trigger
{
public:
    KologarnBodyTankTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn body tank trigger") {}
    bool IsActive() override;
};

class KologarnOffTankTrigger : public Trigger
{
public:
    KologarnOffTankTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn off tank trigger") {}
    bool IsActive() override;
};

class KologarnRubbleTankTrigger : public Trigger
{
public:
    KologarnRubbleTankTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn rubble tank trigger") {}
    bool IsActive() override;
};

class KologarnDpsTargetTrigger : public Trigger
{
public:
    KologarnDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn dps target trigger") {}
    bool IsActive() override;
};

class KologarnSmashSwapTrigger : public Trigger
{
public:
    KologarnSmashSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn smash swap trigger") {}
    bool IsActive() override;
};

class KologarnBodyUncoveredTrigger : public Trigger
{
public:
    KologarnBodyUncoveredTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn body uncovered trigger") {}
    bool IsActive() override;
};

class KologarnFallFromFloorTrigger : public Trigger
{
public:
    KologarnFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn fall from floor trigger") {}
    bool IsActive() override;
};

class KologarnRubbleSlowdownTrigger : public Trigger
{
public:
    KologarnRubbleSlowdownTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn rubble slowdown trigger") {}
    bool IsActive() override;
};

class KologarnEyebeamTrigger : public Trigger
{
public:
    KologarnEyebeamTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn eyebeam trigger") {}
    bool IsActive() override;
};

#endif
