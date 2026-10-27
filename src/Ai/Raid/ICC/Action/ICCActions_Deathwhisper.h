/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_DEATHWHISPER_H
#define PLAYERBOTS_ICCACTIONS_DEATHWHISPER_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Deathwhisper.h"
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

class IccDarkReckoningAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc dark reckoning";

    IccDarkReckoningAction(PlayerbotAI* botAI, std::string const name = Name)
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

class IccRangedPositionLadyDeathwhisperAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc ranged position lady deathwhisper";

    IccRangedPositionLadyDeathwhisperAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    bool MaintainRangedSpacing();
};

class IccAddsLadyDeathwhisperAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc adds lady deathwhisper";

    IccAddsLadyDeathwhisperAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    bool IsTargetedByShade(uint32 shadeEntry);
    bool HandleAddTargeting(Unit* boss);
    bool UpdateRaidTargetIcon(Unit* target, Unit* boss);
    bool HandleNonTankAddEvasion();
    bool IsAdd(Unit* unit);
    bool IsAssistTankAlive();
    bool ApplyNearbyAddCC();
    bool ApplyCCToAdd(Unit* add);
    bool IsAddsAlive();
    bool EngageBoss();
    Unit* FindAndCollectAdd(Unit* boss);
    Unit* FindAddNearBoss(Unit* boss, float maxDist);

};

class IccShadeLadyDeathwhisperAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc shade lady deathwhisper";

    IccShadeLadyDeathwhisperAction(PlayerbotAI* botAI, std::string const name = Name)
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

#endif
