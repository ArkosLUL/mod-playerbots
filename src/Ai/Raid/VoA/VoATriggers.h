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
    BOSS_EMALON = 33993,
    NPC_TEMPEST_MINION = 33998,
    SPELL_LIGHTNING_NOVA_10_MAN = 64216,
    SPELL_LIGHTNING_NOVA_25_MAN = 65279,

    // Archavon the Stone Watcher
    BOSS_ARCHAVON = 31125,
    SPELL_ROCK_SHARDS = 58678,
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
class ArchavonMarkBossTrigger : public Trigger
{
public:
    ArchavonMarkBossTrigger(PlayerbotAI* ai) : Trigger(ai, "archavon mark boss trigger") {}
    bool IsActive() override;
};

class ArchavonRockShardsSpreadTrigger : public Trigger
{
public:
    ArchavonRockShardsSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "archavon rock shards spread trigger") {}
    bool IsActive() override;
};

#endif
