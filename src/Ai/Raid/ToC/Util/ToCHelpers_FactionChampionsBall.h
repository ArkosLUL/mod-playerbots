#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H

#include "Define.h"
#include "Position.h"

class Player;
class PlayerbotAI;
class Unit;

// Faction Champions positioning: healers and ranged stand off the held champions, the trinket opener and
// the Nitro Boosts escape. Every reader answers null or false unless the encounter is live.
namespace TrialOfTheCrusaderHelpers
{

// The holder's spot while its champion is on it and chasing it (within 10 yd), so the walk takes the
// champion along
bool FactionChampionsHoldWalk(PlayerbotAI* botAI, Position& spot);

// This bot's stand. Healers: 30+ yd from every held champion and holder spot, 6+ yd apart, within 36 yd of
// every holder and the kill target where they can, else the fewest out of reach, then the shortest walk;
// kept while it stays clear, 26+ yd from them and what it heals within 42 yd. Once the kill target has been
// out, a Cyclone druid stands 20, 26 or 30 yd from it where it still reaches the pack healer nearest it
// after a 6 yd drift, else where it reaches it now (fc.cyclone), and ranged take slots 20 and 26 yd beyond
// it; before that, with a champion healer up, 30 and 32 yd off the pack. Ranged slots keep 15 yd off the
// held champions. A ranged stand holds while its layout hasn't moved 6 yd and the slot is still clear;
// free bots take the nearest free slot. None while it holds a CC duty.
bool FactionChampionsBallStand(PlayerbotAI* botAI, Position& spot);

// Heroic: the bot whose ready instant AoE fear or Frost Nova reaches the most champions with their PvP
// trinket up, three or more, casts it where it stands, so they burn it. Not a healer or the grip DK, and
// never with a champion the grip guard holds in reach. Comes back with the trinkets. The spell, and the
// unit to cast it at. Written to fc.opener.
Unit* FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId);

// Where a bot runs on Nitro Boosts: three or more champions have it as victim, or it is fc.nextfocus, at 30%
// health or more. 25 yd away from them (±45°, ±90° to stay on the floor), latched for the boost. Not for a
// tank, a holder, a CC duty holder or a bot rooted or stunned. Each run notes fc.nitro.
bool FactionChampionsChased(PlayerbotAI* botAI, Position& spot);

}

#endif
