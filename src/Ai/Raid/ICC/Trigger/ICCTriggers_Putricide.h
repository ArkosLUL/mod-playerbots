/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_PUTRICIDE_H
#define PLAYERBOTS_ICCTRIGGERS_PUTRICIDE_H

#include "ICCHelpers_Putricide.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccPutricideDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide dodge";

    IccPutricideDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide swap";

    IccPutricideSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide tank defensive";

    IccPutricideTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideKiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide kite";

    IccPutricideKiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricidePlagueTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide plague";

    IccPutricidePlagueTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideStackTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide stack";

    IccPutricideStackTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideAbominationTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide abomination";

    IccPutricideAbominationTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideBrakeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide brake";

    IccPutricideBrakeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc putricide target";

    IccPutricideTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetPpTarget) {}
};

class IccPutricideStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide stand";

    IccPutricideStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
