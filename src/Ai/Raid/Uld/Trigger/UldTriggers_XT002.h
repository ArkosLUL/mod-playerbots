#ifndef PLAYERBOTS_ULDTRIGGERS_XT002_H
#define PLAYERBOTS_ULDTRIGGERS_XT002_H

#include <string>

#include "GenericTriggers.h"
#include "Trigger.h"

// XT-002 Deconstructor. Every trigger is false without XT, so the shared Ulduar strategy idles elsewhere.

// Carries Searing Light, Gravity Bomb or both. One trigger since one action has to own the bot: two nodes at
// one relevance pull it between destinations.
class XT002DebuffCarrierTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 debuff carrier trigger";

    XT002DebuffCarrierTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

// Melee never stand next to a Boombot; ranged kill it from outside the blast. Void Zones only count for a
// bot not carrying a debuff, since the carrier action owns those.
class XT002AvoidHazardTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 avoid hazard trigger";

    XT002AvoidHazardTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

// The healer sent to a Gravity Bomb carrier, while that carrier is out of its heal range.
class XT002BombHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 bomb heal trigger";

    XT002BombHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

// Anchors the tank XT is on, ranged and healers once XT is in combat, or bots walk there before the pull.
// Stands down for any dodge, a tank without aggro, and the healer sent to a bomb carrier.
class XT002RaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "xt002 raid position trigger";

    XT002RaidPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

// Every role's target comes from the encounter's priority order, tanks included: generic tank targeting
// drags XT into the add pile. No raid icons: they're group-global.
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

// One healer tank buff for the XT-002 buff tank. strategy: only healers running that combat strategy, for a
// buff the class node casts in one spec only.
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
