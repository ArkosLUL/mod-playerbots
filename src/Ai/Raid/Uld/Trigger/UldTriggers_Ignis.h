#ifndef PLAYERBOTS_ULDTRIGGERS_IGNIS_H
#define PLAYERBOTS_ULDTRIGGERS_IGNIS_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Ignis the Furnace Master
//
class IgnisScorchedGroundTrigger : public Trigger
{
public:
    IgnisScorchedGroundTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis scorched ground trigger") {}
    bool IsActive() override;
};

class IgnisMainTankPositionTrigger : public Trigger
{
public:
    IgnisMainTankPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis main tank position trigger") {}
    bool IsActive() override;
};

class IgnisConstructTankTrigger : public Trigger
{
public:
    IgnisConstructTankTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis construct tank trigger") {}
    bool IsActive() override;
};

class IgnisAttackBrittleConstructTrigger : public Trigger
{
public:
    IgnisAttackBrittleConstructTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis attack brittle construct trigger") {}
    bool IsActive() override;
};

class IgnisAttackBossTrigger : public Trigger
{
public:
    IgnisAttackBossTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis attack boss trigger") {}
    bool IsActive() override;
};

class IgnisMoltenConstructAvoidTrigger : public Trigger
{
public:
    IgnisMoltenConstructAvoidTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis molten construct avoid trigger") {}
    bool IsActive() override;
};

class IgnisFlameJetsTrigger : public Trigger
{
public:
    IgnisFlameJetsTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis flame jets trigger") {}
    bool IsActive() override;
};

class IgnisSlagPotHealTrigger : public Trigger
{
public:
    IgnisSlagPotHealTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis slag pot heal trigger") {}
    bool IsActive() override;
};

#endif
