/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYMULTIPLIERS_H
#define PLAYERBOTS_ONYMULTIPLIERS_H

#include "Multiplier.h"
#include "RaidAntiFear.h"

// The parts of the Onyxia rules that depend on an action's target, which no rule sees. Each is wrapped
// to the families it looks at.

// Damage on her while the bot's threat latch holds: until the main tank has had her for a few seconds,
// at 90% of his threat, and while she lands. Heals and anything aimed elsewhere go through. AI_VALUE
// "threat" is not used: it is a uint8 percentage and wraps.
class OnyxiaThreatHoldMultiplier : public Multiplier
{
public:
    OnyxiaThreatHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "onyxia threat hold") {}
    float GetValue(Action* action) override;
};

// Nobody but the main tank taunts her: a second tank taking her turns her breath through the raid.
class OnyxiaTauntGuardMultiplier : public Multiplier
{
public:
    OnyxiaTauntGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "onyxia taunt guard") {}
    float GetValue(Action* action) override;
};

class OnyxiaBurstWindowMultiplier : public Multiplier
{
public:
    OnyxiaBurstWindowMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "onyxia burst window") {}
    float GetValue(Action* action) override;
};

// No closing on, and no walking over to heal, anything standing by a live egg. Whelps leave the pit
// on their own; following them in hatches more.
class OnyxiaEggGuardMultiplier : public Multiplier
{
public:
    OnyxiaEggGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "onyxia egg guard") {}
    float GetValue(Action* action) override;
};

class OnyxiaAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    OnyxiaAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "onyxia anti fear totem guard")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif
