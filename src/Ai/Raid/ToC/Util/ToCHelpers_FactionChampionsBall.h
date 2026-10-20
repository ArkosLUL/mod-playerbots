#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H

#include "Define.h"
#include "Position.h"

class Player;
class PlayerbotAI;
class Unit;

// Faction Champions positioning: the frontline holds the pack wherever it goes, the kill squad kills a
// healer pulled out of it from its far side. Every reader answers null or false unless the encounter
// is live.
namespace TrialOfTheCrusaderHelpers
{

// This bot is on the frontline. Its spot is the pack centre.
bool FactionChampionsFrontlineHeld(PlayerbotAI* botAI, Position& centre);

// The champion this frontline bot hits, dealt per instance from those within 20 yd of the pack centre,
// not pulled, hittable and not under a CC a hit breaks: first one no frontline DPS has threat on, then
// one on a raider off the frontline, then the rest; melee before casters, nearest the centre first. Each gets the nearest free member, spares go round again. Kept while it qualifies and nothing
// of a higher tier is left uncovered. Written to fc.front. Null when it holds none.
Unit* FactionChampionsFrontlineTarget(PlayerbotAI* botAI);

// This bot's stand. Every healer heals the frontline from 28 yd off the pack centre, 45-75° off the line
// to the kill target once it's been out, each keeping its spoke while it lives. Once the kill target has
// been out, a Cyclone druid stands 20 or 26 yd from it where it also reaches the pack healer nearest it
// (fc.cyclone), and ranged take slots 20 and 26 yd beyond it; before that, with a champion healer up, 30
// and 32 yd off the pack. A stand holds while its layout hasn't moved 6 yd and the slot is still clear;
// free bots take the nearest free slot. None for the opener bot, nor while it holds a CC duty.
bool FactionChampionsBallStand(PlayerbotAI* botAI, Position& spot);

// On heroic, from 2.5 to 8 s after the release, one bot walks into the pack and casts an AoE CC so the
// champions burn their PvP trinket. Its spell, and where to stand. Written to fc.opener.
bool FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId, Position& spot);

// Champions in the pack within radius of this bot
uint32 FactionChampionsPackAround(PlayerbotAI* botAI, float radius);

// Nitro Boosts before a walk of 15+ yd, never while the speed is already up
void FactionChampionsBoostWalk(Player* bot, float distance);

// Where a chased ranged bot or healer runs: a melee champion within 8 yd has it as victim and its Nitro
// Boosts are ready, so it boosts 25 yd straight away (±45°, ±90° to stay on the floor). Latched for the
// boost. Not for a tank, the frontline, a CC duty holder or a bot rooted or stunned.
bool FactionChampionsChased(PlayerbotAI* botAI, Position& spot);

}

#endif
