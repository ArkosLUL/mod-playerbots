/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSStrategy.h"
#include "OSMultipliers.h"
#include "Strategy.h"

void RaidOsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Movement precedence, highest first. Left implicit these tie and the off-tank and melee
    // oscillate between the drake pile and the safe corridor.
    //
    // Getting back onto the arena sits above both dodges: off the platform nothing can hit the bot
    // and the bot can do nothing, so there is no mechanic left worth reacting to.
    //
    // The fissure dodge wins outright: smallest move, tightest fuse. The corridor dodge outranks
    // every hold, so the pile is abandoned mid-fight when a wave goes out - it is Y-only and keeps X,
    // so the west-to-east order re-forms on the new corridor. "rear flank" sits below both on
    // purpose: eating one Shadow Breath is survivable, standing in a tsunami is not. It also sits
    // below the two nodes that own the cases it gets wrong - Sartharion's rear is a cone of its own,
    // and a drake's is free ground its 90-120 degree band never reaches. It keeps the adds.
    triggers.push_back(new TriggerNode("os off platform",
                                       { NextAction("os return to platform", ACTION_EMERGENCY + 2) }));
    triggers.push_back(new TriggerNode("os twilight fissure",
                                       { NextAction("os avoid twilight fissure", ACTION_EMERGENCY + 1) }));
    triggers.push_back(new TriggerNode("os tsunami corridor",
                                       { NextAction("os tsunami corridor", ACTION_EMERGENCY) }));
    // Top of the RAID band, so a hold that keeps returning true cannot starve either of them, and
    // still under every dodge: a bear tank standing in a tsunami is no better off for being in form,
    // and neither is one under Survival Instincts. The cooldown goes first because it only fires at
    // all in the window the tank is dying in, and it costs one tick.
    triggers.push_back(new TriggerNode("os main tank cooldown",
                                       { NextAction("os main tank cooldown", ACTION_RAID + 7) }));
    triggers.push_back(new TriggerNode("os tank shapeshift",
                                       { NextAction("os tank shapeshift", ACTION_RAID + 6) }));
    triggers.push_back(new TriggerNode("os drake landing",
                                       { NextAction("os drake landing position", ACTION_RAID + 5) }));
    triggers.push_back(new TriggerNode("os offtank hold",
                                       { NextAction("os offtank hold", ACTION_RAID + 4) }));
    triggers.push_back(new TriggerNode("os tranquilize",
                                       { NextAction("os tranquilize enrage", ACTION_RAID + 3) }));
    triggers.push_back(new TriggerNode("os redirect threat",
                                       { NextAction("os redirect threat", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("os main tank hold",
                                       { NextAction("os main tank hold", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("os raid hold",
                                       { NextAction("os raid hold", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("twilight portal enter",
                                       { NextAction("enter twilight portal", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("twilight portal exit",
                                       { NextAction("exit twilight portal", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("sartharion dps",
                                       { NextAction("sartharion attack priority", ACTION_RAID) }));
    triggers.push_back(new TriggerNode("os drake rear",
                                       { NextAction("os drake rear", ACTION_MOVE + 6) }));
    triggers.push_back(new TriggerNode("os sartharion flank",
                                       { NextAction("os sartharion flank", ACTION_MOVE + 5) }));
    triggers.push_back(new TriggerNode("sartharion melee positioning",
                                       { NextAction("rear flank", ACTION_MOVE + 4) }));
}

void RaidOsStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new SartharionMultiplier(botAI));
    multipliers.push_back(new SartharionBurstWindowMultiplier(botAI));
}
