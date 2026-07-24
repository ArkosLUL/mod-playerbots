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

    // P2 ground hazards.
    triggers.push_back(new TriggerNode("deep breath",
        { NextAction("deep breath dodge", ACTION_EMERGENCY) }));
    triggers.push_back(new TriggerNode("surge of power",
        { NextAction("avoid surge of power", ACTION_EMERGENCY) }));

    // P3 drake flight.
    triggers.push_back(new TriggerNode("group flying",
        { NextAction("eoe fly drake", ACTION_NORMAL + 1) }));
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
