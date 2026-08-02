/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericHunterStrategy.h"

class GenericHunterStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericHunterStrategyActionNodeFactory()
    {
        creators["rapid fire"] = &rapid_fire;
        creators["mongoose bite"] = &mongoose_bite;
        creators["raptor strike"] = &raptor_strike;
        creators["explosive trap"] = &explosive_trap;
    }

private:
    static ActionNode* rapid_fire([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("rapid fire",
                              /*P*/ {},
                              /*A*/ { NextAction("readiness") },
                              /*C*/ {});
    }
    static ActionNode* mongoose_bite([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("mongoose bite",
                              /*P*/ {},
                              /*A*/ { NextAction("raptor strike") },
                              /*C*/ {});
    }
    static ActionNode* raptor_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("raptor strike",
                              /*P*/ { NextAction("melee") },
                              /*A*/ {},
                              /*C*/ {});
    }
    static ActionNode* explosive_trap([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("explosive trap",
                              /*P*/ {},
                              /*A*/ { NextAction("immolation trap") },
                              /*C*/ {});
    }
};

GenericHunterStrategy::GenericHunterStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericHunterStrategyActionNodeFactory());
}

void GenericHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);

    // Mark/Ammo/Mana Triggers
    triggers.push_back(new TriggerNode("no ammo", { NextAction("equip upgrades packet action", ACTION_MOVE) }));
    triggers.push_back(new TriggerNode("hunter's mark", { NextAction("hunter's mark", ACTION_HIGH + 9.5f) }));
    triggers.push_back(new TriggerNode("rapid fire", { NextAction("rapid fire", ACTION_HIGH + 9) }));
    triggers.push_back(
        new TriggerNode("aspect of the viper", { NextAction("aspect of the viper", ACTION_HIGH + 8) }));

    // Aggro/Threat/Defensive Triggers
    triggers.push_back(new TriggerNode("has aggro", { NextAction("concussive shot", ACTION_HIGH) }));
    triggers.push_back(
        new TriggerNode("low tank threat", { NextAction("misdirection on main tank", ACTION_HIGH + 7) }));
    // On packs the redirect has to be up before Volley/Multi-Shot, so keep it on cooldown instead of
    // waiting for threat to already be lost.
    triggers.push_back(new TriggerNode("misdirection on main tank and light aoe",
                                       { NextAction("misdirection on main tank", ACTION_HIGH + 7) }));
    triggers.push_back(new TriggerNode("low health", { NextAction("deterrence", ACTION_MOVE + 5) }));
    triggers.push_back(
        new TriggerNode("concussive shot on snare target", { NextAction("concussive shot", ACTION_HIGH) }));
    triggers.push_back(new TriggerNode("medium threat", { NextAction("feign death", ACTION_MOVE + 5) }));
    triggers.push_back(new TriggerNode("hunters pet medium health", { NextAction("mend pet", ACTION_HIGH + 2) }));
    triggers.push_back(new TriggerNode("hunters pet low health", { NextAction("mend pet", ACTION_HIGH + 1) }));

    // Dispel Triggers
    triggers.push_back(
        new TriggerNode("tranquilizing shot enrage", { NextAction("tranquilizing shot", ACTION_RAID + 1) }));
    triggers.push_back(
        new TriggerNode("tranquilizing shot magic", { NextAction("tranquilizing shot", ACTION_RAID + 1) }));

    // A pet lost to raid damage takes Kill Command and Bestial Wrath with it, so both specs need a way
    // back mid-fight. Sits under every real shot but above the Steady/Auto Shot filler, so it never
    // costs a GCD the rotation wanted and still gets a chance to fire.
    triggers.push_back(new TriggerNode("no pet", { NextAction("call pet", ACTION_DEFAULT + 0.25f) }));
    triggers.push_back(new TriggerNode("hunters pet dead", { NextAction("revive pet", ACTION_DEFAULT + 0.24f) }));

    // Ranged-based Triggers
    triggers.push_back(new TriggerNode("trap launcher: explosive trap no cd",
                                       { NextAction("trap launcher: explosive trap", ACTION_NORMAL + 7) }));
    triggers.push_back(new TriggerNode("enemy within melee", { NextAction("explosive trap", ACTION_MOVE + 7),
                                                               NextAction("mongoose bite", ACTION_HIGH + 2),
                                                               NextAction("wing clip", ACTION_HIGH + 1) }));

    triggers.push_back(new TriggerNode("enemy too close for auto shot", { NextAction("disengage", ACTION_MOVE + 5),
                                                                          NextAction("flee", ACTION_MOVE + 4) }));
}

// ===== AoE Strategy, 2/3+ enemies =====
AoEHunterStrategy::AoEHunterStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI) {}

void AoEHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("volley channel check", { NextAction("cancel channel", ACTION_HIGH + 3) }));
    triggers.push_back(new TriggerNode("medium aoe", { NextAction("volley", ACTION_HIGH + 2) }));
    triggers.push_back(new TriggerNode("light aoe", { NextAction("multi-shot", ACTION_HIGH + 1) }));
}

void HunterCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("scare beast", { NextAction("scare beast on cc", ACTION_HIGH + 3) }));
    triggers.push_back(new TriggerNode("freezing trap", { NextAction("freezing trap on cc", ACTION_HIGH + 3) }));
}

void HunterTrapWeaveStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("immolation trap no cd", { NextAction("reach melee", ACTION_HIGH + 3) }));
}
