#ifndef PLAYERBOTS_RAID_TOCHELPERS_SHARED_H
#define PLAYERBOTS_RAID_TOCHELPERS_SHARED_H

#include "PlayerbotAI.h"
#include "RaidCombatHelpers.h"
#include "RaidDirectTargeting.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// The ToC spellings the Anub'arak helpers and triggers still call
inline constexpr auto& IsToCDpsBot = IsRaidDpsBot;
inline constexpr auto& GetToCHumanSkull = GetRaidHumanSkull;
inline constexpr auto& ToCPetOffPick = RaidPetOffPick;

// Nearest alive creature of the given entry within radius of the bot (nullptr if none)
Unit* GetNearestCreatureByEntry(Player* bot, uint32 entry, float radius);

// Average position of all alive creatures of the given entry within radius of the bot.
// Returns false (and leaves center untouched) when none are found. Used to flee away from a
// whole cluster/trail of hazards rather than a single nearest patch.
bool GetCreatureClusterCenter(Player* bot, uint32 entry, float radius, Position& center);

}

#endif
