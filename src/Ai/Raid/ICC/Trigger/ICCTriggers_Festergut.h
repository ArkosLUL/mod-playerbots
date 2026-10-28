/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_FESTERGUT_H
#define PLAYERBOTS_ICCTRIGGERS_FESTERGUT_H

#include "ICCHelpers_Festergut.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccFestergutGooTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc festergut goo";

    IccFestergutGooTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccFestergutPungentDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc festergut pungent defensive";

    IccFestergutPungentDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccFestergutPungentCoverTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc festergut pungent cover";

    IccFestergutPungentCoverTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccFestergutTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc festergut tank defensive";

    IccFestergutTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccFestergutBloatSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc festergut bloat swap";

    IccFestergutBloatSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccFestergutBrakeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc festergut brake";

    IccFestergutBrakeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccFestergutTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc festergut target";

    IccFestergutTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetFgTarget) {}
};

class IccFestergutStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc festergut stand";

    IccFestergutStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
