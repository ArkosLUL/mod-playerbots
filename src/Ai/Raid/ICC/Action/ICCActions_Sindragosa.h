/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SINDRAGOSA_H
#define PLAYERBOTS_ICCACTIONS_SINDRAGOSA_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "ICCHelpers_Sindragosa.h"
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

class IccSindragosaGroupPositionAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa group position";

    IccSindragosaGroupPositionAction(PlayerbotAI* botAI)
            : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool HandleTankPositioning(Unit* boss);
    bool HandleNonTankPositioning();
    bool MoveIncrementallyToPosition(Position const& targetPos, float maxStep);
};

class IccSindragosaFrostBeaconAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc sindragosa frost beacon";

    IccSindragosaFrostBeaconAction(PlayerbotAI* botAI)
            : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool HandleSupportActions();
    bool HandleBeaconedPlayer(Unit const* boss);
    bool HandleNonBeaconedPlayer(Unit const* boss);
    bool MoveToPositionIfNeeded(Position const& position, float tolerance);
    bool MoveToPosition(Position const& position);
    bool TryDropTombFlares(Unit const* boss);

    private:
    static constexpr float POSITION_TOLERANCE = 1.0f;
    static constexpr float TOMB_POSITION_TOLERANCE = 0.5f;
    static constexpr float MIN_SAFE_DISTANCE = 13.0f;
    static constexpr float MOVE_TOLERANCE = 2.0f;
    static constexpr uint32 FLARE_ITEM_COOLDOWN_MS = 1000;
};

class IccSindragosaHotAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa hot";

    IccSindragosaHotAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaBlisteringColdAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc sindragosa blistering cold";

    IccSindragosaBlisteringColdAction(PlayerbotAI* botAI)
            : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaUnchainedMagicAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa unchained magic";

    IccSindragosaUnchainedMagicAction(PlayerbotAI* botAI)
            : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaChilledToTheBoneAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa chilled to the bone";

    IccSindragosaChilledToTheBoneAction(PlayerbotAI* botAI)
            : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaMysticBuffetAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc sindragosa mystic buffet";

    IccSindragosaMysticBuffetAction(PlayerbotAI* botAI)
            : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaFrostBombAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc sindragosa frost bomb";

    IccSindragosaFrostBombAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    struct FrostBombContext
    {
        Unit* marker = nullptr;
        std::vector<Unit*> tombs;
    };
    bool CollectContext(FrostBombContext& ctx) const;
    int ResolveGroupIndex(Group* group) const;
    void PinGroupToCurrentZone();
    std::vector<Unit*> SelectTombs(std::vector<Unit*> const& tombs, int groupIndex, int groupCount) const;
    Unit* ResolveStickyTomb(std::vector<Unit*> const& myTombs);
    bool HandleRtiMarking(Group* group, int groupIndex, std::vector<Unit*> const& myTombs, Unit* losTomb);
};

#endif
