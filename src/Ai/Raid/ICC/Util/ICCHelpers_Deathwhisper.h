/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_DEATHWHISPER_H
#define PLAYERBOTS_ICCHELPERS_DEATHWHISPER_H

#include <vector>

#include "EncounterHelpers.h"
#include "ICCData.h"
#include "Playerbots.h"
#include "RaidBurstWindow.h"

constexpr uint32 SPELL_LDW_DEATH_AND_DECAY = 71001;
constexpr uint32 SPELL_LDW_FROSTBOLT = 71420;
constexpr uint32 SPELL_LDW_DARK_MARTYRDOM_FANATIC = 71236;
constexpr uint32 SPELL_LDW_DARK_MARTYRDOM_ADHERENT = 70903;
constexpr uint32 SPELL_LDW_DOMINATE_MIND = 71289;
constexpr uint32 SPELL_LDW_SHROUD_OF_THE_OCCULT = 70768;

constexpr uint32 NPC_LADY_DEATHWHISPER = 36855;
constexpr uint32 NPC_LDW_CULT_FANATIC = 37890;
constexpr uint32 NPC_LDW_DEFORMED_FANATIC = 38135;
constexpr uint32 NPC_LDW_REANIMATED_FANATIC = 38009;
constexpr uint32 NPC_LDW_CULT_ADHERENT = 37949;
constexpr uint32 NPC_LDW_EMPOWERED_ADHERENT = 38136;
constexpr uint32 NPC_LDW_REANIMATED_ADHERENT = 38010;

constexpr float LDW_DND_REACH = 10.0f;
constexpr float LDW_MARTYRDOM_REACH = 15.0f;
constexpr float LDW_DARK_RECKONING_REACH = 15.0f;
constexpr float LDW_BLAST_REACH_10H = 15.0f;
constexpr float LDW_BLAST_REACH_25H = 20.0f;
constexpr float LDW_SHADE_TRIGGER = 12.0f;
constexpr float LDW_SHADE_CLEAR = 16.0f;
constexpr float LDW_BLAST_IMMINENT = 10.0f;
constexpr float LDW_DODGE_MARGIN = 2.5f;
constexpr float LDW_DODGE_SWEEP = 30.0f;

constexpr uint8 LDW_SWAP_STACKS = 3;
constexpr float LDW_TAUNT_RANGE = 30.0f;

constexpr uint32 LDW_FROSTBOLT_LAST_MS = 1100;
constexpr float LDW_LOW_HEALTH_PCT = 35.0f;

constexpr float LDW_THREAT_CAP = 0.9f;
constexpr float LDW_THREAT_RELEASE = 0.8f;
constexpr float LDW_BRAKE_AREA_REACH = 12.0f;

enum class LdwHazardKind : uint8
{
    None,
    DeathAndDecay,
    Martyrdom,
    Shade,
    Blast,
    DarkReckoning
};

struct LdwHazard
{
    Position pos;
    float trigger;
    float clear;
    LdwHazardKind kind;
};

Creature* LdwBoss(PlayerbotAI* botAI);
bool LdwBarrierUp(Player* bot, Unit* boss);

Unit* LdwTarget(PlayerbotAI* botAI);
Unit* LdwLooseAddToTaunt(PlayerbotAI* botAI);
bool LdwSwapTaunt(PlayerbotAI* botAI);
Player* LdwRedirectTank(PlayerbotAI* botAI);

void LdwUpdateCcDuties(PlayerbotAI* botAI);
Unit* LdwCcDuty(PlayerbotAI* botAI, char const*& spell, bool& waiting);
void LdwLeaveFormFor(PlayerbotAI* botAI, char const* spell);
bool LdwVictimInSplash(PlayerbotAI* botAI, float range);

bool LdwThreatBraked(PlayerbotAI* botAI);

uint32 LdwTankDefensive(PlayerbotAI* botAI);

bool LdwIsFrostboltInterrupter(PlayerbotAI* botAI);
char const* LdwReadyInterrupt(Player* bot, Unit* boss);

std::vector<LdwHazard> LdwHazardsFor(PlayerbotAI* botAI);
LdwHazardKind LdwDodgeKind(PlayerbotAI* botAI);
bool LdwNearHazard(PlayerbotAI* botAI);

std::vector<LdwHazard> LdwDarkReckoningHazards(PlayerbotAI* botAI);
bool LdwDarkReckoningDodging(PlayerbotAI* botAI);

RaidBurstWindow IccDeathwhisperBurstWindow(PlayerbotAI* botAI);

void LdwTick(PlayerbotAI* botAI);

inline bool LdwIsEmpty(Position const& pos) { return !pos.GetPositionX() && !pos.GetPositionY(); }

#endif
