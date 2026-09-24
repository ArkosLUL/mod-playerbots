#ifndef PLAYERBOTS_ULDTRIGGERS_IGNIS_H
#define PLAYERBOTS_ULDTRIGGERS_IGNIS_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Ignis the Furnace Master
//
class IgnisScorchedGroundTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis scorched ground trigger";

    IgnisScorchedGroundTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class IgnisMainTankPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis main tank position trigger";

    IgnisMainTankPositionTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class IgnisConstructTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis construct tank trigger";

    IgnisConstructTankTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class IgnisAttackBrittleConstructTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis attack brittle construct trigger";

    IgnisAttackBrittleConstructTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class IgnisAttackBossTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis attack boss trigger";

    IgnisAttackBossTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class IgnisMoltenConstructAvoidTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis molten construct avoid trigger";

    IgnisMoltenConstructAvoidTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class IgnisFlameJetsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis flame jets trigger";

    IgnisFlameJetsTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class IgnisSlagPotHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "ignis slag pot heal trigger";

    IgnisSlagPotHealTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

#endif
