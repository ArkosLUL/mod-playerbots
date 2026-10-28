/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_ROTFACE_H
#define PLAYERBOTS_ICCTRIGGERS_ROTFACE_H

#include "ICCHelpers_Rotface.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccRotfaceDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface dodge";

    IccRotfaceDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface tank defensive";

    IccRotfaceTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceDispelTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface dispel";

    IccRotfaceDispelTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceOozeTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface ooze taunt";

    IccRotfaceOozeTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceKiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface kite";

    IccRotfaceKiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceCarrierTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface carrier";

    IccRotfaceCarrierTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccRotfaceTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc rotface target";

    IccRotfaceTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetRfTarget) {}
};

class IccRotfaceStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc rotface stand";

    IccRotfaceStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
