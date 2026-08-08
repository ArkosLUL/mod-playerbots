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

class AuriayaSonicScreechTrigger : public Trigger
{
public:
    AuriayaSonicScreechTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya sonic screech trigger") {}
    bool IsActive() override;
};

class AuriayaSeepingEssenceTrigger : public Trigger
{
public:
    AuriayaSeepingEssenceTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya seeping essence trigger") {}
    bool IsActive() override;
};

class AuriayaMarkDpsTargetTrigger : public Trigger
{
public:
    AuriayaMarkDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya mark dps target trigger") {}
    bool IsActive() override;
};

class AuriayaAttackDpsTargetTrigger : public Trigger
{
public:
    AuriayaAttackDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya attack dps target trigger") {}
    bool IsActive() override;
};

class AuriayaSentryTauntTrigger : public Trigger
{
public:
    AuriayaSentryTauntTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya sentry taunt trigger") {}
    bool IsActive() override;
};

class AuriayaTankFacingTrigger : public Trigger
{
public:
    AuriayaTankFacingTrigger(PlayerbotAI* ai) : Trigger(ai, "auriaya tank facing trigger") {}
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
