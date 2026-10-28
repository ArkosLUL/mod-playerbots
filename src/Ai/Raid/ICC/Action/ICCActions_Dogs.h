/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_DOGS_H
#define PLAYERBOTS_ICCACTIONS_DOGS_H

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Dogs.h"
#include "RaidDirectTargeting.h"

class IccDogsCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc dogs cast tank defensive";

    IccDogsCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccDogsCastDecimateCoverAction : public Action
{
public:
    static constexpr char const* Name = "icc dogs cast decimate cover";

    IccDogsCastDecimateCoverAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccDogsSwapAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc dogs swap";

    IccDogsSwapAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

// Drops the attack on the dog while this tank still carries Mortal Wound and the other holds it
class IccDogsYieldAction : public Action
{
public:
    static constexpr char const* Name = "icc dogs yield";

    IccDogsYieldAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccDogsTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc dogs target";

    IccDogsTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetDogsTarget) {}
};

#endif
