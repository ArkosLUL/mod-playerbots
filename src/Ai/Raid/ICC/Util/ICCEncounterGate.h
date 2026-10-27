/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCENCOUNTERGATE_H
#define PLAYERBOTS_ICCENCOUNTERGATE_H

#include "RaidEncounter.h"

class PlayerbotAI;

// Boss state ids as InstanceScript keys them. Mirrors icecrown_citadel.h's DataTypes, which is under
// scripts/ and can't be included from here.
enum IccEncounterId : uint32
{
    ICC_BOSS_LORD_MARROWGAR = 0,
    ICC_BOSS_LADY_DEATHWHISPER = 1,
    ICC_BOSS_GUNSHIP_BATTLE = 2,
    ICC_BOSS_DEATHBRINGER_SAURFANG = 3,
    ICC_BOSS_FESTERGUT = 4,
    ICC_BOSS_ROTFACE = 5,
    ICC_BOSS_PROFESSOR_PUTRICIDE = 6,
    ICC_BOSS_BLOOD_PRINCE_COUNCIL = 7,
    ICC_BOSS_BLOOD_QUEEN_LANA_THEL = 8,
    ICC_BOSS_SISTER_SVALNA = 9,
    ICC_BOSS_VALITHRIA_DREAMWALKER = 10,
    ICC_BOSS_SINDRAGOSA = 11,
    ICC_BOSS_THE_LICH_KING = 12,
    ICC_BOSS_SINDRAGOSA_GAUNTLET = 13,
    ICC_BOSS_BLOOD_PRINCE_TRASH = 14,

    // No boss state: Stinky and Precious and the Rotting Frost Giants, gated on their units, and the
    // raid-wide definitions.
    ICC_BOSS_NONE = 255
};

// For a raid-wide definition, with ICC_BOSS_NONE and no trace name: open in every fight, never live.
inline bool IccRaidWideGateOpen(PlayerbotAI* /*botAI*/, uint32 /*encounterId*/) { return true; }
inline bool IccRaidWideGateLive(PlayerbotAI* /*botAI*/, uint32 /*encounterId*/) { return false; }
inline constexpr EncounterGate IccRaidWideGate{&IccRaidWideGateOpen, &IccRaidWideGateLive};

#endif
