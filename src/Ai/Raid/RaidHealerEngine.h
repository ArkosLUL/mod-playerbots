/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDHEALERENGINE_H
#define PLAYERBOTS_RAIDHEALERENGINE_H

#include "Action.h"
#include "Multiplier.h"
#include "NamedObjectContext.h"
#include "Strategy.h"
#include "Trigger.h"

class PlayerbotAI;

// Keeps raid healers on the combat engine through a boss fight. A targetless healer drops to the
// non-combat engine (no mana cooldowns, potions or shields) and only gets back through dps assist,
// which any boss that owns targeting zeroes.

// A bot healer in combat while the instance has a boss encounter in progress. Not in a vehicle: those
// seats are driven by their encounter's own nodes.
bool RaidHealerEngineWindow(PlayerbotAI* botAI);

class RaidHealerCombatEngineTrigger : public Trigger
{
public:
    static constexpr char const* Name = "raid healer combat engine trigger";

    RaidHealerCombatEngineTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class RaidHealerCombatEngineAction : public Action
{
public:
    static constexpr char const* Name = "raid healer combat engine action";

    RaidHealerCombatEngineAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

// On a null target, drop target does nothing but switch back to the non-combat engine.
class RaidHealerTargetHoldMultiplier : public Multiplier
{
public:
    RaidHealerTargetHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "raid healer target hold") {}
    float GetValue(Action* action) override;
};

// Added on every raid map next to the raid's own strategy, by ApplyInstanceStrategies.
class RaidHealerEngineStrategy : public Strategy
{
public:
    RaidHealerEngineStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    std::string const getName() override { return "raid healer engine"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

// Its own context without siblings: in the raid strategies' context, adding the raid's strategy
// would remove this one.
class RaidHealerEngineStrategyContext : public NamedObjectContext<Strategy>
{
public:
    RaidHealerEngineStrategyContext()
    {
        creators["raid healer engine"] = &RaidHealerEngineStrategyContext::raid_healer_engine;
    }

private:
    static Strategy* raid_healer_engine(PlayerbotAI* botAI) { return new RaidHealerEngineStrategy(botAI); }
};

class RaidHealerEngineTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidHealerEngineTriggerContext()
    {
        creators["raid healer combat engine trigger"] = &RaidHealerEngineTriggerContext::combat_engine;
    }

private:
    static Trigger* combat_engine(PlayerbotAI* botAI) { return new RaidHealerCombatEngineTrigger(botAI); }
};

class RaidHealerEngineActionContext : public NamedObjectContext<Action>
{
public:
    RaidHealerEngineActionContext()
    {
        creators["raid healer combat engine action"] = &RaidHealerEngineActionContext::combat_engine;
    }

private:
    static Action* combat_engine(PlayerbotAI* botAI) { return new RaidHealerCombatEngineAction(botAI); }
};

#endif
