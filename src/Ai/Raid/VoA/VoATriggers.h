/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOATRIGGERS_H
#define PLAYERBOTS_VOATRIGGERS_H

#include "GenericTriggers.h"
#include "Trigger.h"
#include "VoAHelpers.h"

//
// Emalon the Storm Watcher
//
class EmalonMarkBossTrigger : public Trigger
{
public:
    EmalonMarkBossTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon mark boss trigger") {}
    bool IsActive() override;
};

class EmalonLightingNovaTrigger : public Trigger
{
public:
    EmalonLightingNovaTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon lighting nova trigger") {}
    bool IsActive() override;
};

class EmalonOverchargeTrigger : public Trigger
{
public:
    EmalonOverchargeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon overcharge trigger") {}
    bool IsActive() override;
};

class EmalonFallFromFloorTrigger : public Trigger
{
public:
    EmalonFallFromFloorTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon fall from floor trigger") {}
    bool IsActive() override;
};

// The three hold triggers below stay active for the whole encounter rather than only while something
// needs holding. With nothing to do their actions park the bot on its anchor, and that is exactly what
// keeps the generic movers from reclaiming it.
class EmalonMainTankHoldTrigger : public Trigger
{
public:
    EmalonMainTankHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon main tank hold trigger") {}
    bool IsActive() override;
};

class EmalonRingHoldTrigger : public Trigger
{
public:
    EmalonRingHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon ring hold trigger") {}
    bool IsActive() override;
};

class EmalonOffTankHoldTrigger : public Trigger
{
public:
    EmalonOffTankHoldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon offtank hold trigger") {}
    bool IsActive() override;
};

class EmalonAttackPriorityTrigger : public Trigger
{
public:
    EmalonAttackPriorityTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon attack priority trigger") {}
    bool IsActive() override;
};

class EmalonRedirectThreatTrigger : public Trigger
{
public:
    EmalonRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon redirect threat trigger") {}
    bool IsActive() override;
};

//
// Archavon the Stone Watcher
//
class ArchavonRockShardsSpreadTrigger : public Trigger
{
public:
    ArchavonRockShardsSpreadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "archavon rock shards spread trigger") {}
    bool IsActive() override;
};

//
// Koralon the Flame Watcher
//
class KoralonBurningBreathTrigger : public Trigger
{
public:
    KoralonBurningBreathTrigger(PlayerbotAI* botAI) : Trigger(botAI, "koralon burning breath trigger") {}
    bool IsActive() override;
};

class KoralonFlamingCinderSpreadTrigger : public Trigger
{
public:
    KoralonFlamingCinderSpreadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "koralon flaming cinder spread trigger") {}
    bool IsActive() override;
};

//
// Toravon the Ice Watcher
//
class ToravonFreezingGroundTrigger : public Trigger
{
public:
    ToravonFreezingGroundTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toravon freezing ground trigger") {}
    bool IsActive() override;
};

class ToravonFrozenOrbAvoidTrigger : public Trigger
{
public:
    ToravonFrozenOrbAvoidTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toravon frozen orb avoid trigger") {}
    bool IsActive() override;
};

#endif
