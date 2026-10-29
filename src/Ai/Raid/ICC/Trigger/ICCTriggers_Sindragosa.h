/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_SINDRAGOSA_H
#define PLAYERBOTS_ICCTRIGGERS_SINDRAGOSA_H

#include "ICCHelpers_Sindragosa.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccSindragosaColdTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa blistering cold";

    IccSindragosaColdTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaSplashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa splash";

    IccSindragosaSplashTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaBombHideTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa bomb hide";

    IccSindragosaBombHideTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaBeaconTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa frost beacon";

    IccSindragosaBeaconTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa tank defensive";

    IccSindragosaTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa taunt";

    IccSindragosaTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaBuffetHideTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa buffet hide";

    IccSindragosaBuffetHideTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaUnchainedTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa unchained magic";

    IccSindragosaUnchainedTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaFreedomTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa hand of freedom";

    IccSindragosaFreedomTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaBeaconHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa beacon heal";

    IccSindragosaBeaconHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaHoldTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa hold";

    IccSindragosaHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSindragosaTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc sindragosa target";

    IccSindragosaTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetSgTarget) {}
};

class IccSindragosaStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sindragosa stand";

    IccSindragosaStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
