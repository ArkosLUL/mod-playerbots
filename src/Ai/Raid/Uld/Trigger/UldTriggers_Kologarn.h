#ifndef PLAYERBOTS_ULDTRIGGERS_KOLOGARN_H
#define PLAYERBOTS_ULDTRIGGERS_KOLOGARN_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

class KologarnBodyTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn body tank trigger";

    KologarnBodyTankTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnOffTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn off tank trigger";

    KologarnOffTankTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnRubbleTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn rubble tank trigger";

    KologarnRubbleTankTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnDpsTargetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn dps target trigger";

    KologarnDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnSmashSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn smash swap trigger";

    KologarnSmashSwapTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnBodyUncoveredTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn body uncovered trigger";

    KologarnBodyUncoveredTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnFallFromFloorTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn fall from floor trigger";

    KologarnFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnRubbleSlowdownTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn rubble slowdown trigger";

    KologarnRubbleSlowdownTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class KologarnEyebeamTrigger : public Trigger
{
public:
    static constexpr char const* Name = "kologarn eyebeam trigger";

    KologarnEyebeamTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

#endif
