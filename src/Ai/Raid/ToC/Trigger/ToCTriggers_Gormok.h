#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_GORMOK_H

#include "ToCTriggers_NorthrendBeasts.h"
#include "Trigger.h"

class GormokTankDutyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok tank duty";

    GormokTankDutyTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokSwapTankDutyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok swap tank duty";

    GormokSwapTankDutyTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokTankSwapNeededTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok tank swap needed";

    GormokTankSwapNeededTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok tank defensive";

    GormokTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokImpaleResetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok impale reset";

    GormokImpaleResetTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokCancelHandOfProtectionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok cancel hand of protection";

    GormokCancelHandOfProtectionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokSnobolledTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok snobolled";

    GormokSnobolledTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokStompRangeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok stomp range";

    GormokStompRangeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class GormokFireBombIncomingTrigger : public Trigger
{
public:
    static constexpr char const* Name = "gormok fire bomb incoming";

    GormokFireBombIncomingTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
