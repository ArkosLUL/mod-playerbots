/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_GUNSHIP_H
#define PLAYERBOTS_ICCTRIGGERS_GUNSHIP_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccInCannonTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc in cannon";

    IccInCannonTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccGunshipCannonNearTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc gunship cannon near";

    IccGunshipCannonNearTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccGunshipRocketJumpTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc gunship rocket jump";

    IccGunshipRocketJumpTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccGunshipRocketPackSetupTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc gunship rocket pack setup";

    IccGunshipRocketPackSetupTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
