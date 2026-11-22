/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_GUNSHIP_H
#define PLAYERBOTS_ICCACTIONS_GUNSHIP_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Gunship.h"
#include "MovementActions.h"
#include "RaidDirectTargeting.h"

class Item;

class IccGunshipWalkAction : public MovementAction
{
public:
    IccGunshipWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    Position const* WalkInFlight();
    bool WalkTo(Position const& spot, MovementPriority priority);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccGunshipDodgeAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc gunship dodge";

    IccGunshipDodgeAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccGunshipRocketJumpAction : public IccGunshipWalkAction
{
public:
    static constexpr char const* Name = "icc gunship rocket jump";

    IccGunshipRocketJumpAction(PlayerbotAI* botAI) : IccGunshipWalkAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    bool Fire(Item* pack, Position const& destination);
};

class IccGunshipTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc gunship taunt";

    IccGunshipTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccGunshipTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc gunship tank defensive";

    IccGunshipTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccGunshipEnterCannonAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc gunship enter cannon";

    IccGunshipEnterCannonAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccCannonFireAction : public Action
{
public:
    static constexpr char const* Name = "icc cannon fire";

    IccCannonFireAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;

private:
    bool TryCast(char const* spell, Unit* target, Unit* cannon);
};

class IccGunshipTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc gunship target";

    IccGunshipTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GsbTarget) {}
};

class IccGunshipStandAction : public IccGunshipWalkAction
{
public:
    static constexpr char const* Name = "icc gunship stand";

    IccGunshipStandAction(PlayerbotAI* botAI) : IccGunshipWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccGunshipRocketPackSetupAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc gunship rocket pack setup";

    IccGunshipRocketPackSetupAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    bool AcquireRocketPack();
    bool EquipRocketPack();
};

#endif
