/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEStrategy.h"
#include "EoEMultipliers.h"
#include "Strategy.h"

void RaidEoEStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("malygos",
        { NextAction("malygos position", ACTION_MOVE) }));
    triggers.push_back(new TriggerNode("malygos",
        { NextAction("malygos target", ACTION_RAID + 1) }));

    // P1 Power Sparks: DK grips them away, everyone else kills them before they reach Malygos.
    triggers.push_back(new TriggerNode("power spark",
        { NextAction("pull power spark", ACTION_RAID + 3) }));
    triggers.push_back(new TriggerNode("power spark",
        { NextAction("kill power spark", ACTION_RAID + 2) }));

    // P2: strip the Nexus Lords' Haste. Hangs off the boss trigger rather than one of its own so it
    // costs nothing outside P2 - the action gates itself on class and phase.
    triggers.push_back(new TriggerNode("malygos",
        { NextAction("malygos spellsteal", ACTION_RAID + 2) }));

    // P2 shelter. The bubble beats the surge dodge, which is only the fallback for bots that
    // cannot reach one.
    triggers.push_back(new TriggerNode("malygos bubble",
        { NextAction("malygos seek bubble", ACTION_EMERGENCY + 2) }));
    triggers.push_back(new TriggerNode("surge of power",
        { NextAction("avoid surge of power", ACTION_EMERGENCY) }));

    // P2 hover disks: melee ride a freed disk up to the Scions.
    triggers.push_back(new TriggerNode("malygos free disk",
        { NextAction("malygos board disk", ACTION_RAID + 4) }));
    triggers.push_back(new TriggerNode("malygos on disk",
        { NextAction("malygos ride disk", ACTION_RAID + 4) }));

    // P3 drake flight.
    // Flight positioning outranks the drake rotation: it parks and turns the vehicle, then stands
    // down for the tick. A moving or off-arc vehicle cannot cast, so this has to settle first.
    triggers.push_back(new TriggerNode("malygos drake flight",
        { NextAction("eoe fly drake", ACTION_NORMAL + 6) }));
    triggers.push_back(new TriggerNode("drake combat",
        { NextAction("eoe drake attack", ACTION_NORMAL + 5) }));
    triggers.push_back(new TriggerNode("static field",
        { NextAction("avoid static field", ACTION_EMERGENCY) }));
    triggers.push_back(new TriggerNode("drake surge",
        { NextAction("drake dodge surge", ACTION_EMERGENCY + 5) }));
}

void RaidEoEStrategy::InitMultipliers(std::vector<Multiplier*> &multipliers)
{
    multipliers.push_back(new MalygosMultiplier(botAI));
}
