/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_VALITHRIA_H
#define PLAYERBOTS_ICCTRIGGERS_VALITHRIA_H

#include "ICCHelpers_Valithria.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccValithriaDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria dodge";

    IccValithriaDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaKiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria kite";

    IccValithriaKiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaDreamTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria dream";

    IccValithriaDreamTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaPortalTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria portal";

    IccValithriaPortalTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria taunt";

    IccValithriaTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaSnareTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria snare";

    IccValithriaSnareTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaVoidTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria void";

    IccValithriaVoidTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc valithria target";

    IccValithriaTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetVdTarget) {}
};

class IccValithriaHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria heal";

    IccValithriaHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaInterruptTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria interrupt";

    IccValithriaInterruptTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaFreedomTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria freedom";

    IccValithriaFreedomTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria tank defensive";

    IccValithriaTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaLustTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria lust";

    IccValithriaLustTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaGuardianSpiritTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria guardian spirit";

    IccValithriaGuardianSpiritTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccValithriaBeaconTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valithria beacon";

    IccValithriaBeaconTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
