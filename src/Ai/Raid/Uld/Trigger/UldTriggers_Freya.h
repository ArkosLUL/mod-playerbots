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

class FreyaMarkDpsTargetTrigger : public Trigger
{
public:
    FreyaMarkDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "freya mark dps target trigger") {}
    bool IsActive() override;
};

class FreyaMoveToHealingSporeTrigger : public Trigger
{
public:
    FreyaMoveToHealingSporeTrigger(PlayerbotAI* ai) : Trigger(ai, "freya move to healing spore trigger") {}
    bool IsActive() override;
};

#endif
