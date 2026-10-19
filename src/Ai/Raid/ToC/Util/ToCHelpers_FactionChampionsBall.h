#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H

#include "Define.h"
#include "Position.h"

class PlayerbotAI;
class Unit;

// Faction Champions positioning: the frontline holds the pack where it landed, the kill squad kills a
// healer pulled out of it from its far side. Every reader answers null or false unless the encounter
// is live.
namespace TrialOfTheCrusaderHelpers
{

// This bot is on the frontline. Its spot is where the champions landed.
bool FactionChampionsFrontlineHeld(PlayerbotAI* botAI, Position& anchor);

// The champion this frontline bot hits, dealt per instance from those within 20 yd of the landing spot,
// not pulled, hittable and not under a CC a hit breaks: first one no frontline DPS has threat on, then
// one on a raider off the frontline, then the rest; melee before casters, nearest the landing spot
// first. Each gets the nearest free member, spares go round again. Kept while it qualifies and nothing
// of a higher tier is left uncovered. Written to fc.front. Null when it holds none.
Unit* FactionChampionsFrontlineTarget(PlayerbotAI* botAI);

// This bot's stand. All healers but the one with the least armour heal the frontline from 28 yd off
// the landing spot, 45-75° off the line to the pulled healer. The rest, then ranged, take slots 20 and
// 26 yd beyond a pulled kill target; with the kill target still in the pack and a champion healer up,
// 30 and 32 yd off the pack instead. None for the opener bot.
bool FactionChampionsBallStand(PlayerbotAI* botAI, Position& spot);

// On heroic, from 2.5 to 8 s after the release, one bot walks into the pack and casts an AoE CC so the
// champions burn their PvP trinket. Its spell, and where to stand. Written to fc.opener.
bool FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId, Position& spot);

// Champions in the pack within radius of this bot
uint32 FactionChampionsPackAround(PlayerbotAI* botAI, float radius);

}

#endif
