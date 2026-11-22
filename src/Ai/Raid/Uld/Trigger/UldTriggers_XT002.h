#ifndef PLAYERBOTS_ULDTRIGGERS_XT002_H
#define PLAYERBOTS_ULDTRIGGERS_XT002_H

#include <string>

#include "GenericTriggers.h"
#include "Trigger.h"

class XT002DebuffCarrierTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 debuff carrier trigger";

    XT002DebuffCarrierTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002AvoidHazardTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 avoid hazard trigger";

    XT002AvoidHazardTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002BombHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 bomb heal trigger";

    XT002BombHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002BombCarrierHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 bomb carrier heal trigger";

    XT002BombCarrierHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002RaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 raid position trigger";

    XT002RaidPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002SetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 set dps priority trigger";

    XT002SetDpsPriorityTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002PummellerTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 pummeller taunt trigger";

    XT002PummellerTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002RedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 redirect threat trigger";

    XT002RedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class XT002HolderBuffTrigger : public Trigger
{
public:
    XT002HolderBuffTrigger(PlayerbotAI* botAI, std::string const name, uint8 classId, std::string const spell,
                           char const* strategy = nullptr)
        : Trigger(botAI, name), classId(classId), spell(spell), strategy(strategy)
    {
    }

    bool IsActive() override;

private:
    uint8 classId;
    std::string spell;
    char const* strategy;
};

#endif
