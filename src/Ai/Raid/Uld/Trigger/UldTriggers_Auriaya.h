#ifndef PLAYERBOTS_ULDTRIGGERS_AURIAYA_H
#define PLAYERBOTS_ULDTRIGGERS_AURIAYA_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "RaidAntiFear.h"
#include "UldEncounter_Auriaya.h"
#include "Trigger.h"

//
// Auriaya
//
class AuriayaFallFromFloorTrigger : public Trigger
{
public:
    static constexpr char const* Name = "auriaya fall from floor trigger";

    AuriayaFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AuriayaSeepingEssenceTrigger : public Trigger
{
public:
    static constexpr char const* Name = "auriaya seeping essence trigger";

    AuriayaSeepingEssenceTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AuriayaSentryTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "auriaya sentry taunt trigger";

    AuriayaSentryTauntTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AuriayaRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "auriaya raid position trigger";

    AuriayaRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AuriayaSetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "auriaya set dps priority trigger";

    AuriayaSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class AuriayaAntiFearTrigger : public RaidAntiFearTrigger
{
public:
    static constexpr char const* Name = "auriaya anti fear trigger";

    AuriayaAntiFearTrigger(PlayerbotAI* ai) : RaidAntiFearTrigger(ai, Name) {}

protected:
    bool FearWindowActive() override { return AuriayaFearWindowActive(botAI); }
};

#endif
