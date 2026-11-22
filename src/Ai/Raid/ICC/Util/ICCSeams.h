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

struct IccEncounterHooks
{
    EncounterDefinition const& (*definition)();
    RaidBurstWindow (*burstWindow)(PlayerbotAI* botAI) = nullptr;
    std::vector<uint32> const& (*openerBosses)(PlayerbotAI* botAI) = nullptr;
    uint8 (*resistSchools)(PlayerbotAI* botAI, uint8 previous) = nullptr;
    bool (*fireWantsMastery)(PlayerbotAI* botAI) = nullptr;
};

IccEncounterHooks const* IccLiveHooks(PlayerbotAI* botAI);

RaidBurstWindow IccLiveBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& IccOpenerBosses(PlayerbotAI* botAI);
uint8 IccResistWantedSchools(PlayerbotAI* botAI, uint8 previous);
bool IccResistFireWantsMastery(PlayerbotAI* botAI);
bool IccResistAuraWanted(PlayerbotAI* botAI);

inline constexpr RaidOpenerConfig ICC_OPENER{ICC_MAP_ID, "icc.opener", &IccOpenerBosses};
inline constexpr RaidResistAuraConfig ICC_RESIST_AURA{ICC_MAP_ID, "icc.aura", &IccResistWantedSchools,
                                                      &IccResistFireWantsMastery};
inline constexpr RaidDefensiveScope ICC_DEFENSIVE_SCOPE{ICC_MAP_ID, "icc.cover", "icc.personal"};

using IccResolver = Unit* (*)(PlayerbotAI* botAI);

template <IccResolver Resolve>
bool IccResolverPicks(PlayerbotAI* botAI)
{
    return Resolve(botAI) != nullptr;
}

#endif
