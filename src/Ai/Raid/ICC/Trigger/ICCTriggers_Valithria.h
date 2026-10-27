/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_VALITHRIA_H
#define PLAYERBOTS_ICCTRIGGERS_VALITHRIA_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccValithriaGroupTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria group";

    IccValithriaGroupTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaZombieKiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria zombie kite";

    IccValithriaZombieKiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaPortalTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria portal";

    IccValithriaPortalTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria heal";

    IccValithriaHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaDreamCloudTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria dream cloud";

    IccValithriaDreamCloudTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
