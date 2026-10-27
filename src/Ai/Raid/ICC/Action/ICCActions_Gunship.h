/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_GUNSHIP_H
#define PLAYERBOTS_ICCACTIONS_GUNSHIP_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Gunship.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <set>

class IccCannonFireAction : public Action
{
public:
    static constexpr char const* Name = "icc cannon fire";

    IccCannonFireAction(PlayerbotAI* botAI, std::string const name = Name) : Action(botAI, name) {}
    bool Execute(Event event) override;

private:
    Unit* FindValidCannonTarget();
    bool TryCastCannonSpell(uint32 spellId, Unit* target, Unit* vehicleBase);
};

class IccGunshipEnterCannonAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc gunship enter cannon";

    IccGunshipEnterCannonAction(PlayerbotAI* botAI, std::string const name = Name)
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;

private:
    Unit* FindBestAvailableCannon();
    bool IsValidCannon(Unit* vehicle);
    bool EnterVehicle(Unit* vehicleBase, bool moveIfFar);
};

class IccGunshipRocketJumpAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc gunship rocket jump";

    IccGunshipRocketJumpAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

private:
    enum class GunshipSide
    {
        NONE,
        ALLY,
        HORDE
    };

    GunshipSide DetectShip() const;
    Item* FindRocketPack() const;
    bool UseRocketPack(Position const& destination, bool walkIfOutOfRange);
    bool RocketPackJumpToward(Position const& target);
    bool ExitCannonIfSeated();
    bool CleanupSkullIcon(uint8 skullIconIndex);
    bool UpdateBossSkullIcon(Unit* boss, uint8 skullIconIndex);
    bool IsMainTankOnEnemyShip(GunshipSide side) const;
    bool AnyNonTankAwayFromFriendly(GunshipSide side) const;
    Unit* FindNearestFriendlyCannon(GunshipSide side) const;
};

class IccGunshipRocketPackSetupAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc gunship rocket pack setup";

    IccGunshipRocketPackSetupAction(PlayerbotAI* botAI, std::string const name = Name)
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;

private:
    Item* FindRocketPack() const;
    bool AcquireRocketPack();
    bool EquipRocketPack();
};

#endif
