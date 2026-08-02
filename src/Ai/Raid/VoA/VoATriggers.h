/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOATRIGGERS_H
#define PLAYERBOTS_VOATRIGGERS_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "PlayerbotAIConfig.h"
#include "Trigger.h"

enum VoAIDs
{
    // Emalon the Storm Watcher
    AURA_OVERCHARGE = 64217,
    NPC_TEMPEST_MINION = 33998,
    SPELL_LIGHTNING_NOVA_10_MAN = 64216,
    SPELL_LIGHTNING_NOVA_25_MAN = 65279,

    // Archavon the Stone Watcher
    SPELL_ROCK_SHARDS = 58678,

    // Koralon the Flame Watcher
    SPELL_BURNING_BREATH = 66665,

    // Toravon the Ice Watcher
    NPC_FROZEN_ORB = 38456,
    SPELL_FREEZING_GROUND = 72090,
};

//
// Emalon the Storm Watcher
//
class EmalonMarkBossTrigger : public Trigger
{
public:
    EmalonMarkBossTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon mark boss trigger") {}
    bool IsActive() override;
};

class EmalonLightingNovaTrigger : public Trigger
{
public:
    EmalonLightingNovaTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon lighting nova trigger") {}
    bool IsActive() override;
};

class EmalonOverchargeTrigger : public Trigger
{
public:
    EmalonOverchargeTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon overcharge trigger") {}
    bool IsActive() override;
};

class EmalonFallFromFloorTrigger : public Trigger
{
public:
    EmalonFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, "emalon fall from floor trigger") {}
    bool IsActive() override;
};

//
// Archavon the Stone Watcher
//
class ArchavonRockShardsSpreadTrigger : public Trigger
{
public:
    ArchavonRockShardsSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "archavon rock shards spread trigger") {}
    bool IsActive() override;
};

//
// Koralon the Flame Watcher
//
class KoralonBurningBreathTrigger : public Trigger
{
public:
    KoralonBurningBreathTrigger(PlayerbotAI* ai) : Trigger(ai, "koralon burning breath trigger") {}
    bool IsActive() override;
};

class KoralonFlamingCinderSpreadTrigger : public Trigger
{
public:
    KoralonFlamingCinderSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "koralon flaming cinder spread trigger") {}
    bool IsActive() override;
};

//
// Toravon the Ice Watcher
//
class ToravonFreezingGroundTrigger : public Trigger
{
public:
    ToravonFreezingGroundTrigger(PlayerbotAI* ai) : Trigger(ai, "toravon freezing ground trigger") {}
    bool IsActive() override;
};

class ToravonFrozenOrbAvoidTrigger : public Trigger
{
public:
    ToravonFrozenOrbAvoidTrigger(PlayerbotAI* ai) : Trigger(ai, "toravon frozen orb avoid trigger") {}
    bool IsActive() override;
};

#endif
