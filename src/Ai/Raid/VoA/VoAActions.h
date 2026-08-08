/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOAACTIONS_H
#define PLAYERBOTS_VOAACTIONS_H

#include "Action.h"
#include "Event.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"

//
//  Emalon the Storm Watcher
//

class EmalonLightingNovaAction : public MovementAction
{
public:
    EmalonLightingNovaAction(PlayerbotAI* botAI) : MovementAction(botAI, "emalon lighting nova action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class EmalonOverchargeAction : public Action
{
public:
    EmalonOverchargeAction(PlayerbotAI* botAI) : Action(botAI, "emalon overcharge action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class EmalonFallFromFloorAction : public Action
{
public:
    EmalonFallFromFloorAction(PlayerbotAI* botAI) : Action(botAI, "emalon fall from floor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

//
//  Archavon the Stone Watcher
//

class ArchavonRockShardsSpreadAction : public MovementAction
{
public:
    ArchavonRockShardsSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "archavon rock shards spread action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

//
//  Koralon the Flame Watcher
//

class KoralonBurningBreathAction : public MovementAction
{
public:
    KoralonBurningBreathAction(PlayerbotAI* botAI) : MovementAction(botAI, "koralon burning breath action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class KoralonFlamingCinderSpreadAction : public MovementAction
{
public:
    KoralonFlamingCinderSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "koralon flaming cinder spread action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

//
//  Toravon the Ice Watcher
//

class ToravonFreezingGroundAction : public MovementAction
{
public:
    ToravonFreezingGroundAction(PlayerbotAI* botAI) : MovementAction(botAI, "toravon freezing ground action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ToravonFrozenOrbAvoidAction : public MovementAction
{
public:
    ToravonFrozenOrbAvoidAction(PlayerbotAI* botAI) : MovementAction(botAI, "toravon frozen orb avoid action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
