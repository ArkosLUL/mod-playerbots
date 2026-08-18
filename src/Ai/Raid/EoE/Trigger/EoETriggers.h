/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOETRIGGERS_H
#define PLAYERBOTS_EOETRIGGERS_H

#include "EoEData.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class MalygosTrigger : public Trigger
{
public:
    MalygosTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos") {}
    bool IsActive() override;
};

class PowerSparkTrigger : public Trigger
{
public:
    // Sparks walk in from the edge; checking every tick buys nothing.
    PowerSparkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos power spark", 200) {}
    bool IsActive() override;
};

// P2: this bot is out of shelter and an Arcane Overload bubble is reachable. The bubble grants
// SPELL_ARCANE_OVERLOAD_PROTECTION (-50% damage taken), which is what makes Surge of Power survivable.
class MalygosBubbleTrigger : public Trigger
{
public:
    // The seek is a walk across the platform, so 200ms of latency is invisible.
    MalygosBubbleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos bubble", 200) {}
    bool IsActive() override;
};

class MalygosFreeDiskTrigger : public Trigger
{
public:
    // A disk sits on the ground until someone takes it; there is nothing to race.
    MalygosFreeDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos free disk", 300) {}
    bool IsActive() override;
};

class MalygosOnDiskTrigger : public Trigger
{
public:
    MalygosOnDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos on disk") {}
    bool IsActive() override;
};

// P2: Surge of Power beam active (surge NPC summoned or boss casting the P2 surge).
class SurgeOfPowerTrigger : public Trigger
{
public:
    // The peel is a MoveAway the MotionMaster carries on with, and the sweep behind this comes
    // back empty almost every time.
    SurgeOfPowerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos surge of power", 200) {}
    bool IsActive() override;
};

// P3: this bot is on a Wyrmrest Skytalon. EoE-owned because the Oculus "group flying"
// trigger also needs the raid leader mounted.
class MalygosDrakeFlightTrigger : public Trigger
{
public:
    MalygosDrakeFlightTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos drake flight") {}
    bool IsActive() override;
};

class DrakeSurgeTrigger : public Trigger
{
public:
    DrakeSurgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "eoe drake surge") {}
    bool IsActive() override;
};

#endif
