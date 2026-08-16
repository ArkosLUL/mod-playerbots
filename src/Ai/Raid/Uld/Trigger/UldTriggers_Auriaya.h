#ifndef PLAYERBOTS_ULDTRIGGERS_AURIAYA_H
#define PLAYERBOTS_ULDTRIGGERS_AURIAYA_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "RaidAntiFear.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Auriaya
//
class AuriayaFallFromFloorTrigger : public Trigger
{
public:
    AuriayaFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya fall from floor trigger") {}
    bool IsActive() override;
};

class AuriayaSeepingEssenceTrigger : public Trigger
{
public:
    AuriayaSeepingEssenceTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya seeping essence trigger") {}
    bool IsActive() override;
};

class AuriayaSentryTauntTrigger : public Trigger
{
public:
    AuriayaSentryTauntTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya sentry taunt trigger") {}
    bool IsActive() override;
};

class AuriayaRaidPositionTrigger : public Trigger
{
public:
    AuriayaRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya raid position trigger") {}
    bool IsActive() override;
};

class AuriayaSetDpsPriorityTrigger : public Trigger
{
public:
    AuriayaSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya set dps priority trigger") {}
    bool IsActive() override;
};

class AuriayaAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    AuriayaAntiFearTrigger(PlayerbotAI* ai) : RaidAntiFearTrigger(ai, "auriaya anti fear trigger") {}

protected:
    bool FearWindowActive() override { return AuriayaFearWindowActive(botAI); }
};

#endif
