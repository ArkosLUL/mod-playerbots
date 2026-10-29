/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCTRIGGERS_BLOODPRINCES_H

#include "ICCHelpers_BloodPrinces.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccBpcDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc dodge";

    IccBpcDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc tank defensive";

    IccBpcTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcBallTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc ball";

    IccBpcBallTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcPrinceTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc prince taunt";

    IccBpcPrinceTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcNucleusTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc nucleus";

    IccBpcNucleusTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcSoakTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc soak";

    IccBpcSoakTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBpcTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc bpc target";

    IccBpcTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetBpcTarget) {}
};

class IccBpcStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bpc stand";

    IccBpcStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
