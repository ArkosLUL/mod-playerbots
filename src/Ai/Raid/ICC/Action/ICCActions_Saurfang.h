/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SAURFANG_H
#define PLAYERBOTS_ICCACTIONS_SAURFANG_H

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Saurfang.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

class IccDbsRuneSwapAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc dbs rune swap";

    IccDbsRuneSwapAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

// Drops the attack on him while another tank holds him and this one carries Rune of Blood
class IccDbsRuneYieldAction : public Action
{
public:
    static constexpr char const* Name = "icc dbs rune yield";

    IccDbsRuneYieldAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccDbsBeastTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc dbs beast taunt";

    IccDbsBeastTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccDbsBeastControlAction : public Action
{
public:
    static constexpr char const* Name = "icc dbs beast control";

    IccDbsBeastControlAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccDbsMarkHealAction : public Action
{
public:
    static constexpr char const* Name = "icc dbs mark heal";

    IccDbsMarkHealAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccDbsTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc dbs target";

    IccDbsTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetDbsTarget) {}
};

// Holds its spot while the walk there is in flight: every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccDbsStandAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc dbs stand";

    IccDbsStandAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    bool BookedOn(Position const& spot);

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

#endif
