/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericPaladinStrategy.h"
#include "GenericPaladinStrategyActionNodeFactory.h"

GenericPaladinStrategy::GenericPaladinStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericPaladinStrategyActionNodeFactory());
}

void GenericPaladinStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode("hammer of justice interrupt",
        { NextAction("hammer of justice", ACTION_INTERRUPT) }));
    triggers.push_back(new TriggerNode("hammer of justice on enemy healer",
        { NextAction("hammer of justice on enemy healer", ACTION_INTERRUPT) }));
    triggers.push_back(new TriggerNode("hammer of justice on snare target",
        { NextAction("hammer of justice on snare target", ACTION_INTERRUPT) }));
    triggers.push_back(new TriggerNode("critical health", { NextAction("divine shield", ACTION_EMERGENCY) }));
    triggers.push_back(new TriggerNode("critical health", { NextAction("lay on hands", ACTION_EMERGENCY + 1) }));
    triggers.push_back(new TriggerNode("party member critical health",
        { NextAction("lay on hands on party", ACTION_EMERGENCY + 2) }));
    // Topping up inside the bubble must stay below the emergency heals on someone who is dying.
    triggers.push_back(new TriggerNode("divine shield low health",
        { NextAction("flash of light", ACTION_EMERGENCY - 1), NextAction("holy light", ACTION_EMERGENCY - 2)}));
    // BoP wipes threat and applies Forbearance, so Hand of Sacrifice goes first.
    triggers.push_back(new TriggerNode("protect party member",
        { NextAction("hand of sacrifice on party", ACTION_EMERGENCY + 3),
          NextAction("blessing of protection on party", ACTION_EMERGENCY + 2.8f) }));
    triggers.push_back(new TriggerNode("hand of freedom on party",
        { NextAction("hand of freedom on party", ACTION_HIGH + 4) }));
}

void PaladinCureStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "cleanse cure disease", { NextAction("cleanse disease", ACTION_DISPEL + 2) }));
    triggers.push_back(
        new TriggerNode("cleanse party member cure disease",
                        { NextAction("cleanse disease on party", ACTION_DISPEL + 1) }));
    triggers.push_back(new TriggerNode(
        "cleanse cure poison", { NextAction("cleanse poison", ACTION_DISPEL + 2) }));
    triggers.push_back(
        new TriggerNode("cleanse party member cure poison",
                        { NextAction("cleanse poison on party", ACTION_DISPEL + 1) }));
    triggers.push_back(new TriggerNode(
        "cleanse cure magic", { NextAction("cleanse magic", ACTION_DISPEL + 2) }));
    triggers.push_back(
        new TriggerNode("cleanse party member cure magic",
                        { NextAction("cleanse magic on party", ACTION_DISPEL + 1) }));
}

void PaladinBoostStrategy::InitTriggers(std::vector<TriggerNode*>& /*triggers*/)
{
}

void PaladinCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("turn undead", { NextAction("turn undead", ACTION_HIGH + 1) }));
}

void PaladinHealerDpsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("healer should attack",
                        {
                            NextAction("hammer of wrath", ACTION_DEFAULT + 0.6f),
                            NextAction("judgement of light", ACTION_DEFAULT + 0.3f),
                            NextAction("exorcism", ACTION_DEFAULT+ 0.1f),
                            }));
}
