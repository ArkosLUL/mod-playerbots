/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDENCOUNTER_H
#define PLAYERBOTS_RAIDENCOUNTER_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Multiplier.h"
#include "RaidEncounterRules.h"
#include "Trigger.h"

class Action;
class PlayerbotAI;

// Open: the encounter's mechanics may run. Live: the encounter itself is in progress.
//
// A raid strategy registers every boss at once, so without a gate a Kologarn pull still runs Mimiron,
// Vezax and Flame Leviathan every tick. 2026-08-31 had twenty bots spend a second trying to board a
// leftover siege vehicle mid-Kologarn, because that trigger only asks whether the master is in one.
struct EncounterGate
{
    bool (*open)(PlayerbotAI* botAI, uint32 encounterId);
    bool (*live)(PlayerbotAI* botAI, uint32 encounterId);
};

// Keyed by the InstanceScript boss index. RaidEncounterRules::GateOpen has the rule.
//
// Gate on the fight, not the unit: a drake Sartharion calls in never starts its own encounter, so its
// mechanics belong under Sartharion's id.
bool BossStateGateOpen(PlayerbotAI* botAI, uint32 encounterId);
bool BossStateGateLive(PlayerbotAI* botAI, uint32 encounterId);
inline constexpr EncounterGate BossStateGate{&BossStateGateOpen, &BossStateGateLive};

// Non-zero only while this bot's gated trigger is inside Check, and different for every trigger pass.
// Engine::ProcessTriggers checks every trigger before any action runs and no raid trigger changes the
// world, so a read cached under one id is exact for the rest of that pass. Actions, multipliers, ticks
// and isUseful always get 0: an action returning false can still drop a target or damage a boss past
// a threshold, so nothing may be cached past the checks.
uint32 EncounterTriggerPassId(PlayerbotAI* botAI);

// Wraps rather than subclasses, so the trigger classes and their IsActive bodies stay as they are.
class EncounterGatedTrigger : public Trigger
{
public:
    EncounterGatedTrigger(PlayerbotAI* botAI, Trigger* inner, EncounterGate gate, uint32 encounterId,
                          char const* traceName);
    ~EncounterGatedTrigger() override;

    Event Check() override;
    bool IsActive() override;
    bool IsBuffTrigger() override;
    bool IsDebuffTrigger() override;
    std::vector<NextAction> getHandlers() override;
    void Reset() override;
    Unit* GetTarget() override;
    Value<Unit*>* GetTargetValue() override;
    std::string const GetTargetName() override;
    void ExternalEvent(std::string const param, Player* owner = nullptr) override;
    void ExternalEvent(WorldPacket& packet, Player* owner = nullptr) override;

private:
    Trigger* inner;
    EncounterGate gate;
    uint32 encounterId;
    char const* traceName;
};

// The only place an Action is mapped to rule families.
RaidEncounterRules::FamilyMask ClassifyAction(Action* action);

enum class EncounterRow : uint8
{
    Plain,
    Mover  // passes the boss's OwnMovement rules
};

class EncounterBuilder;

// One boss, declared once: its trigger/action rows, rules, hand-written multipliers and tick. The
// raid's contexts and strategy are built from it, and its gate covers all of it.
class EncounterDefinition
{
public:
    using Define = void (*)(EncounterBuilder&);
    using Predicate = bool (*)(PlayerbotAI*);
    using TickFn = void (*)(PlayerbotAI*);
    using TriggerCreator = std::function<Trigger*(PlayerbotAI*)>;
    using ActionCreator = std::function<Action*(PlayerbotAI*)>;
    using MultiplierCreator = std::function<Multiplier*(PlayerbotAI*)>;

    // traceName is what RaidObs::NamePull files a pull under.
    EncounterDefinition(uint32 encounterId, EncounterGate gate, char const* traceName, Define define);

    // Register after any other wrapping pass over the same map, or a trigger is gated twice.
    void RegisterTriggers(std::unordered_map<std::string, TriggerCreator>& creators) const;
    void RegisterActions(std::unordered_map<std::string, ActionCreator>& creators) const;
    void AddTriggerNodes(std::vector<TriggerNode*>& triggers) const;
    void AddMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers) const;

    // Housekeeping for Strategy::OnTick. Runs only while the gate is open.
    void OnTick(PlayerbotAI* botAI) const;

private:
    friend class EncounterBuilder;

    struct Row
    {
        std::string trigger;
        TriggerCreator makeTrigger;
        std::string action;
        ActionCreator makeAction;
        float priority;
        EncounterRow flag;
    };

    uint32 encounterId;
    EncounterGate gate;
    char const* traceName;
    std::vector<Row> rows;
    std::vector<MultiplierCreator> multipliers;  // in declaration order, which is veto order
    TickFn tick = nullptr;
};

// What a definition function gets. Rules and multipliers keep the order they're declared in: the
// first one to zero an action is the one a trace credits with the veto.
class EncounterBuilder
{
public:
    using FamilyMask = RaidEncounterRules::FamilyMask;
    using RoleMask = RaidEncounterRules::RoleMask;
    using Predicate = EncounterDefinition::Predicate;

    // Both classes carry their own name as a static `Name`, used by their constructors.
    template <class T, class A>
    void Node(float priority, EncounterRow flag = EncounterRow::Plain)
    {
        Node(T::Name, [](PlayerbotAI* ai) -> Trigger* { return new T(ai); }, A::Name,
             [](PlayerbotAI* ai) -> Action* { return new A(ai); }, priority, flag);
    }

    // For a class built under several names, which takes them from the row.
    void Node(std::string trigger, EncounterDefinition::TriggerCreator makeTrigger, std::string action,
              EncounterDefinition::ActionCreator makeAction, float priority,
              EncounterRow flag = EncounterRow::Plain);

    // An empty movers list means every Mover row of this definition.
    void OwnMovement(char const* name, RoleMask roles, Predicate predicate,
                     FamilyMask keep = RaidEncounterRules::Family::Attack | RaidEncounterRules::Family::Reach,
                     std::vector<std::string> movers = {});
    void OwnTargeting(char const* name, RoleMask roles, Predicate predicate,
                      FamilyMask pickers = RaidEncounterRules::Family::DpsAssist |
                                           RaidEncounterRules::Family::TankAssist);
    void Block(char const* name, RoleMask roles, Predicate predicate, FamilyMask families,
               std::vector<std::string> names = {});
    void Exclusive(char const* name, RoleMask roles, Predicate predicate, FamilyMask families = 0,
                   std::vector<std::string> names = {});

    // For what no rule kind fits. The multiplier only sees actions in these families, and only while
    // the gate is open.
    template <class M>
    void Multiplier(FamilyMask families)
    {
        HandWritten(families, [](PlayerbotAI* ai) -> ::Multiplier* { return new M(ai); });
    }

    void Tick(EncounterDefinition::TickFn tick);

private:
    friend class EncounterDefinition;

    explicit EncounterBuilder(EncounterDefinition& definition) : definition(definition) {}

    void AddRule(char const* name, RoleMask roles, Predicate predicate, RaidEncounterRules::Rule rule,
                 bool moverRows);
    void HandWritten(FamilyMask families, EncounterDefinition::MultiplierCreator make);

    EncounterDefinition& definition;
    std::vector<std::shared_ptr<RaidEncounterRules::Rule>> moverRules;  // filled once define returns
};

#endif
