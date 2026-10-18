#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H

#include "Define.h"
#include "Position.h"

class PlayerbotAI;
class Unit;

// Faction Champions positioning, the Warcraft Tavern death ball: bot tanks hold the pack where it
// landed, the raid kills a healer pulled out of it from its far side. Every reader answers null or
// false unless the encounter is live.
namespace TrialOfTheCrusaderHelpers
{

// This bot is a pack tank. Its spot is where the champions landed.
bool FactionChampionsPackTankHeld(PlayerbotAI* botAI, Position& anchor);

// The champion this pack tank hits: the nearest within 12 yd of it that isn't the kill target or
// pulled out, null when none is that close
Unit* FactionChampionsPackTankTarget(PlayerbotAI* botAI);

// This bot's stand: up to two healers with the most armour 34 yd off the pack toward the kill target,
// so the tank stays in heal reach; other healers, then ranged, on slots 20 and 26 yd beyond the kill
// target on the side away from the pack. None for the opener bot.
bool FactionChampionsBallStand(PlayerbotAI* botAI, Position& spot);

// On heroic, from 2.5 to 8 s after the release, one bot walks into the pack and casts an AoE CC so the
// champions burn their PvP trinket. Its spell, and where to stand. Written to fc.opener.
bool FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId, Position& spot);

// Champions in the pack within radius of this bot
uint32 FactionChampionsPackAround(PlayerbotAI* botAI, float radius);

}

#endif
