/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOAACTIONS_H
#define PLAYERBOTS_VOAACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "Event.h"
#include "MovementActions.h"
#include "NaxxActions.h"
#include "PlayerbotAI.h"
#include "VoAHelpers.h"

//
//  Emalon the Storm Watcher
//

// Every Emalon anchor is either a constant or the boss position plus a constant offset, and the boss
// himself is pinned by the main tank, so there is no moving reference for a slot to chase. That plus
// the arrival tolerances below is what keeps the formation from oscillating.
class EmalonPositioningAction : public MovementAction
{
public:
    EmalonPositioningAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // Clamps into the chamber, drops the move when already inside tolerance, and issues it at combat
    // priority otherwise.
    bool MoveToClamped(float x, float y, float tolerance);
};

class EmalonLightingNovaAction : public EmalonPositioningAction
{
public:
    EmalonLightingNovaAction(PlayerbotAI* botAI) : EmalonPositioningAction(botAI, "emalon lighting nova action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class EmalonMainTankHoldAction : public EmalonPositioningAction
{
public:
    EmalonMainTankHoldAction(PlayerbotAI* botAI) : EmalonPositioningAction(botAI, "emalon main tank hold action") {}
    bool Execute(Event event) override;
};

class EmalonRingHoldAction : public EmalonPositioningAction
{
public:
    EmalonRingHoldAction(PlayerbotAI* botAI) : EmalonPositioningAction(botAI, "emalon ring hold action") {}
    bool Execute(Event event) override;

private:
    // Per-bot, because the context builds one action instance per bot. The slot is recomputed every
    // tick but only re-issued once it has drifted this far from the point already walked to.
    bool hasDest = false;
    float destX = 0.0f;
    float destY = 0.0f;
};

class EmalonOffTankHoldAction : public AttackAction
{
public:
    EmalonOffTankHoldAction(PlayerbotAI* botAI) : AttackAction(botAI, "emalon offtank hold action") {}
    bool Execute(Event event) override;
};

class EmalonAttackPriorityAction : public AttackAction
{
public:
    EmalonAttackPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "emalon attack priority action") {}
    bool Execute(Event event) override;
};

class EmalonRedirectThreatAction : public NaxxRedirectThreatAction
{
public:
    EmalonRedirectThreatAction(PlayerbotAI* botAI) : NaxxRedirectThreatAction(botAI, "emalon redirect threat action") {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
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
