#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSBALL_H

#include "Define.h"
#include "Position.h"

class Player;
class PlayerbotAI;
class Unit;

// Faction Champions positioning: healers and ranged stay put, the Cyclone stand, the healers' step off the
// melee, the trinket opener, the Nitro Boosts run, the kite and the casters' escapes. Every reader answers null or
// false unless the encounter is live, but the pre-pull Demonic Circle and FactionChampionsKitesFrom.
namespace TrialOfTheCrusaderHelpers
{

// Once the kill target has been out, a Cyclone druid stands 20, 26 or 30 yd from it where it still reaches
// the pack healer nearest it after a 6 yd drift, else where it reaches it now (fc.cyclone). Kept while that
// healer stays in Cyclone reach and the kill target within 36 yd of it. None while it holds a CC duty.
bool FactionChampionsCycloneStand(PlayerbotAI* botAI, Position& spot);

// Healers and ranged, tanks and holders aside: they move only to dodge, run, kite, step off, CC or get their
// target in range
bool FactionChampionsStaysPut(PlayerbotAI* botAI);

// A melee champion with the member as victim that the member runs from: the member alive, not a tank and not
// the champion's holder. Reads no refresh, so the champion refresh may ask it.
bool FactionChampionsKitesFrom(Player* member, Unit* champion);

// A healer not casting, rooted or stunned, with a melee champion within 8 yd: 10 yd away from them, on the
// floor and clear of Bladestorm and Hellfire. Latched to arrival or 2.5 s, then none for 3 s.
bool FactionChampionsStepOff(PlayerbotAI* botAI, Position& spot);

// Heroic: the bot whose ready instant AoE fear or Frost Nova reaches the most champions with their PvP
// trinket up, three or more, casts it where it stands, so they burn it. Not a healer or the grip DK, and
// never with a champion the grip guard holds in reach. Comes back with the trinkets. The spell, and the
// unit to cast it at. Written to fc.opener.
Unit* FactionChampionsOpener(PlayerbotAI* botAI, uint32& spellId);

// Where a bot runs on Nitro Boosts: three or more champions, or two melee champions, have it as victim, one
// melee champion does from within 8 yd unrooted and unslowed, or it is fc.nextfocus, at 30% health or more.
// Away from the melee on it (else all on it, else those within 30 yd), straight or 30-120° off: on each
// bearing the farthest clear floor spot up to the boost's reach (~87 yd), stepping back 5 yd, 15 yd at least
// and, for a non-healer, 10 yd off other healers; the one farthest from every melee champion, a tie to the
// longer run. Latched to arrival or the boost's end. Not for a tank, a holder, a CC duty holder or a bot
// rooted or stunned. Each run notes fc.nitro.
bool FactionChampionsChased(PlayerbotAI* botAI, Position& spot);

// Where a bot kites: a melee champion it kites from (FactionChampionsKitesFrom) is within 30 yd, however
// close. 15 yd hops away from them, on the floor, clear of AoE and off other healers, farthest from every
// melee champion; latched to arrival or 2.5 s, dropped once none is within 30 yd. Not while rooted, stunned,
// on a Nitro run or a CC duty. Each hop notes fc.kite.
bool FactionChampionsKite(PlayerbotAI* botAI, Position& spot);

// A melee champion the bot kites from is within 45 yd: reaching its target or a heal target would walk it
// back in
bool FactionChampionsKiteHold(PlayerbotAI* botAI);

constexpr uint32 SPELL_DEMONIC_CIRCLE_SUMMON = 48018;

struct FactionChampionsEscapeCast
{
    uint32 spellId = 0;
    Unit* target = nullptr;
    bool face = false;  // turn to facing first: Blink leaps forward, Disengage back, Dragon's Breath is a cone
    float facing = 0.0f;
};

// A healer or ranged bot with a melee champion on it within 8 yd, in control and not silenced: the first
// ready instant, CC before movement. Mage Frost Nova, Blink 20 yd away from every melee champion within 15 yd
// (on the floor, clear of Bladestorm and Hellfire, landing 12+ yd off all of them), Dragon's Breath; warlock
// Shadowfury on the nearest, Demonic Circle: Teleport when the circle is 12+ yd farther from them, instant
// Howl of Terror, Death Coil on the nearest; priest Psychic Scream; hunter Disengage with one within 5 yd. An
// AoE only with no champion inside held by our breakable fear, stun or confuse or grip-guarded, a fear or
// Dragon's Breath not on the kill target.
bool FactionChampionsEscape(PlayerbotAI* botAI, FactionChampionsEscapeCast& cast);

// A warlock parked on its Champions staging stand without its Demonic Circle, or with under a minute left
bool FactionChampionsCircleDue(PlayerbotAI* botAI);

}

#endif
