/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCTRIGGERS_BLOODPRINCES_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccBpcKelesethTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc keleseth tank";

    IccBpcKelesethTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcMainTankTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc main tank";

    IccBpcMainTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcEmpoweredVortexTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc empowered vortex";

    IccBpcEmpoweredVortexTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcKineticBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc kinetic bomb";

    IccBpcKineticBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcBallOfFlameTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc ball of flame";

    IccBpcBallOfFlameTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
