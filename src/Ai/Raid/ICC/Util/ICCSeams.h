/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCSEAMS_H
#define PLAYERBOTS_ICCSEAMS_H

#include <vector>

#include "Define.h"
#include "ICCData.h"
#include "RaidBurstWindow.h"
#include "RaidDefensive.h"
#include "RaidOpener.h"
#include "RaidResistAura.h"

class EncounterDefinition;
class PlayerbotAI;
class Unit;

// What one encounter feeds the raid-wide helpers while its definition is live. A null hook takes no part.
struct IccEncounterHooks
{
    EncounterDefinition const& (*definition)();
    // The burst cooldowns let through right now. Null lets all of them through.
    RaidBurstWindow (*burstWindow)(PlayerbotAI* botAI) = nullptr;
    // The bosses a DPS bot holds its damage on until a tank has them. Null holds on none.
    std::vector<uint32> const& (*openerBosses)(PlayerbotAI* botAI) = nullptr;
    // RAID_RESIST_* bits wanted, given the set the last read wanted. Null raises no resistance aura.
    uint8 (*resistSchools)(PlayerbotAI* botAI, uint8 previous) = nullptr;
    // Fire goes to a paladin with Aura Mastery. Null for never.
    bool (*fireWantsMastery)(PlayerbotAI* botAI) = nullptr;
};

// The first hooks row whose definition is live, null with none.
IccEncounterHooks const* IccLiveHooks(PlayerbotAI* botAI);

RaidBurstWindow IccLiveBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& IccOpenerBosses(PlayerbotAI* botAI);
uint8 IccResistWantedSchools(PlayerbotAI* botAI, uint8 previous);
bool IccResistFireWantsMastery(PlayerbotAI* botAI);
// The live encounter has a resistance aura hook at all
bool IccResistAuraWanted(PlayerbotAI* botAI);

inline constexpr RaidOpenerConfig ICC_OPENER{ICC_MAP_ID, "icc.opener", &IccOpenerBosses};
inline constexpr RaidResistAuraConfig ICC_RESIST_AURA{ICC_MAP_ID, "icc.aura", &IccResistWantedSchools,
                                                      &IccResistFireWantsMastery};
inline constexpr RaidDefensiveScope ICC_DEFENSIVE_SCOPE{ICC_MAP_ID, "icc.cover", "icc.personal"};

// An encounter's direct-targeting pick: a player's skull first (GetRaidHumanSkull), and only ever a unit
// in combat, since Attack() has no pre-pull guard.
using IccResolver = Unit* (*)(PlayerbotAI* botAI);

// The OwnTargeting predicate that goes with a resolver
template <IccResolver Resolve>
bool IccResolverPicks(PlayerbotAI* botAI)
{
    return Resolve(botAI) != nullptr;
}

#endif
