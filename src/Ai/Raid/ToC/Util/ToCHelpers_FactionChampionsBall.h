#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H

#include "Define.h"
#include "Position.h"

class Player;
class PlayerbotAI;
class Unit;

// Faction Champions positioning: healers and ranged stay put, the Cyclone stand, the healers' step off the
// melee, the trinket opener and the Nitro Boosts escape. Every reader answers null or false unless the
// encounter is live.
namespace TrialOfTheCrusaderHelpers
{

// Once the kill target has been out, a Cyclone druid stands 20, 26 or 30 yd from it where it still reaches
// the pack healer nearest it after a 6 yd drift, else where it reaches it now (fc.cyclone). Kept while that
// healer stays in Cyclone reach and the kill target within 36 yd of it. None while it holds a CC duty.
bool FactionChampionsCycloneStand(PlayerbotAI* botAI, Position& spot);

// Healers and ranged, tanks and holders aside: they move only to dodge, run, step off, CC or get their
// target in range
bool FactionChampionsStaysPut(PlayerbotAI* botAI);

// A healer not casting, rooted or stunned, with a melee champion within 8 yd: 10 yd away from them, on the
// floor and clear of Bladestorm and Hellfire. Latched to arrival or 2.5 s, then none for 3 s.
bool FactionChampionsStepOff(PlayerbotAI* botAI, Position& spot);

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
