/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_ROTFACE_H
#define PLAYERBOTS_ICCTRIGGERS_ROTFACE_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccRotfaceTankPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface tank position";

    IccRotfaceTankPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceGroupPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface group position";

    IccRotfaceGroupPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceMoveAwayFromExplosionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface move away from explosion";

    IccRotfaceMoveAwayFromExplosionTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, Name), _castEndMs(0), _castEnded(false), _wasCasting(false)
    {
    }
    bool IsActive() override;

private:
    uint32 _castEndMs;
    bool _castEnded;
    bool _wasCasting;
};

class IccRotfaceAvoidVileGasTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface avoid vile gas";

    IccRotfaceAvoidVileGasTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
