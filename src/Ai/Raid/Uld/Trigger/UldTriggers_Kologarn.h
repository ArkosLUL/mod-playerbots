#ifndef PLAYERBOTS_ULDTRIGGERS_KOLOGARN_H
#define PLAYERBOTS_ULDTRIGGERS_KOLOGARN_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Kologarn
//
class KologarnMarkDpsTargetTrigger : public Trigger
{
public:
    KologarnMarkDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn mark dps target trigger") {}
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

class KologarnAttackDpsTargetTrigger : public Trigger
{
public:
    KologarnAttackDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn attack dps target trigger") {}
    bool IsActive() override;
};

class KologarnRtiTargetTrigger : public Trigger
{
public:
    KologarnRtiTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn rti target trigger") {}
    bool IsActive() override;
};

class KologarnCrunchArmorTrigger : public Trigger
{
public:
    KologarnCrunchArmorTrigger(PlayerbotAI* ai) : Trigger(ai, "kologarn crunch armor trigger") {}
    bool IsActive() override;
};

#endif
