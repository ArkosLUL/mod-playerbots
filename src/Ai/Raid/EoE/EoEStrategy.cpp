/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEStrategy.h"
#include "EoEMultipliers.h"

void RaidEoEStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // One node per trigger name, not one per action: Engine::ProcessTriggers re-Check()s a trigger
    // once for every node referencing it while it is quiet.
    triggers.push_back(new TriggerNode("malygos",
        { NextAction("malygos position", ACTION_MOVE),
          NextAction("malygos target", ACTION_RAID + 1),
          // P2 spellsteal gates itself on class and phase.
          NextAction("malygos spellsteal", ACTION_RAID + 2) }));

    // P1 Power Sparks: DK grips them away, everyone else kills them.
    triggers.push_back(new TriggerNode("malygos power spark",
        { NextAction("malygos pull power spark", ACTION_RAID + 3),
          NextAction("malygos kill power spark", ACTION_RAID + 2) }));

    // P2 shelter. The bubble beats the surge dodge, which is only a fallback.
    triggers.push_back(new TriggerNode("malygos bubble",
        { NextAction("malygos seek bubble", ACTION_EMERGENCY + 2) }));
    triggers.push_back(new TriggerNode("malygos surge of power",
        { NextAction("malygos avoid surge of power", ACTION_EMERGENCY) }));

    // P2 hover disks: melee ride a freed disk up to the Scions.
    triggers.push_back(new TriggerNode("malygos free disk",
        { NextAction("malygos board disk", ACTION_RAID + 4) }));
    triggers.push_back(new TriggerNode("malygos on disk",
        { NextAction("malygos ride disk", ACTION_RAID + 4) }));

    // P3 drake flight, at emergency relevance because a moving or off-arc vehicle cannot cast at
    // all, so it has to settle before the rotation runs.
    triggers.push_back(new TriggerNode("malygos drake flight",
        { NextAction("eoe fly drake", ACTION_EMERGENCY),
          NextAction("eoe drake attack", ACTION_NORMAL + 5) }));
    triggers.push_back(new TriggerNode("eoe drake surge",
        { NextAction("eoe drake surge shield", ACTION_EMERGENCY + 5) }));
}

void RaidEoEStrategy::InitMultipliers(std::vector<Multiplier*> &multipliers)
{
    multipliers.push_back(new MalygosMultiplier(botAI));
}
