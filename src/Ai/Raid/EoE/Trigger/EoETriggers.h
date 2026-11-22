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
    static constexpr char const* Name = "malygos";

    MalygosTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class PowerSparkTrigger : public Trigger
{
public:
    static constexpr char const* Name = "malygos power spark";

    PowerSparkTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 200) {}
    bool IsActive() override;
};

class MalygosBubbleTrigger : public Trigger
{
public:
    static constexpr char const* Name = "malygos bubble";

    MalygosBubbleTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 200) {}
    bool IsActive() override;
};

class MalygosFreeDiskTrigger : public Trigger
{
public:
    static constexpr char const* Name = "malygos free disk";

    MalygosFreeDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 300) {}
    bool IsActive() override;
};

class MalygosOnDiskTrigger : public Trigger
{
public:
    static constexpr char const* Name = "malygos on disk";

    MalygosOnDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class SurgeOfPowerTrigger : public Trigger
{
public:
    static constexpr char const* Name = "malygos surge of power";

    SurgeOfPowerTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 200) {}
    bool IsActive() override;
};

class MalygosDrakeFlightTrigger : public Trigger
{
public:
    static constexpr char const* Name = "malygos drake flight";

    MalygosDrakeFlightTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class DrakeSurgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "eoe drake surge";

    DrakeSurgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
