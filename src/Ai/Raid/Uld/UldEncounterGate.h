/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERGATE_H
#define PLAYERBOTS_ULDENCOUNTERGATE_H

#include <string>

#include "RaidEncounter.h"

class PlayerbotAI;

// Boss ids ported from ulduar.h in AC, which lives in AC's /scripts directory and so cannot be
// included from here. These are the indices InstanceScript::GetBossState is keyed by.
enum UlduarEncounterId
{
    ULD_BOSS_LEVIATHAN = 0,
    ULD_BOSS_IGNIS = 1,
    ULD_BOSS_RAZORSCALE = 2,
    ULD_BOSS_XT002 = 3,
    ULD_BOSS_ASSEMBLY = 4,
    ULD_BOSS_KOLOGARN = 5,
    ULD_BOSS_AURIAYA = 6,
    ULD_BOSS_FREYA = 7,
    ULD_BOSS_HODIR = 8,
    ULD_BOSS_MIMIRON = 9,
    ULD_BOSS_THORIM = 10,
    ULD_BOSS_VEZAX = 11,
    ULD_BOSS_YOGGSARON = 12,
    ULD_BOSS_ALGALON = 13
};

// Bosses without a definition yet are gated by the prefix their trigger names carry, through the same
// boss-state gate a definition uses.

// False for a name belonging to no single encounter, which is then left ungated.
bool UldEncounterOfTrigger(std::string const& triggerName, uint32& bossId);

// The name a pull is traced under. Null for an id with no entry.
char const* UldEncounterName(uint32 bossId);

inline bool UldEncounterGateOpen(PlayerbotAI* botAI, uint32 bossId) { return BossStateGateOpen(botAI, bossId); }
inline bool UldEncounterIsLive(PlayerbotAI* botAI, uint32 bossId) { return BossStateGateLive(botAI, bossId); }
inline uint32 UldTriggerPassId(PlayerbotAI* botAI) { return EncounterTriggerPassId(botAI); }

class UldGatedTrigger : public EncounterGatedTrigger
{
public:
    UldGatedTrigger(PlayerbotAI* botAI, Trigger* inner, uint32 bossId)
        : EncounterGatedTrigger(botAI, inner, BossStateGate, bossId, UldEncounterName(bossId))
    {
    }
};

#endif
