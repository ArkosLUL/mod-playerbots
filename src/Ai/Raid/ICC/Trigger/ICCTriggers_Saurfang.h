/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_SAURFANG_H
#define PLAYERBOTS_ICCTRIGGERS_SAURFANG_H

#include "ICCHelpers_Saurfang.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccDbsRuneSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs rune swap";

    IccDbsRuneSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsRuneYieldTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs rune yield";

    IccDbsRuneYieldTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsBeastTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs beast taunt";

    IccDbsBeastTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsBeastControlTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs beast control";

    IccDbsBeastControlTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsMarkHealTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs mark heal";

    IccDbsMarkHealTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsProtectTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs protect";

    IccDbsProtectTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs tank defensive";

    IccDbsTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsMarkDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs mark defensive";

    IccDbsMarkDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDbsTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc dbs target";

    IccDbsTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetDbsTarget) {}
};

class IccDbsStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dbs stand";

    IccDbsStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
