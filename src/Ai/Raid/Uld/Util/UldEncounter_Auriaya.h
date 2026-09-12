/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERAURIAYA_H
#define PLAYERBOTS_ULDENCOUNTERAURIAYA_H

#include "Position.h"
#include "UldData.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

// Auriaya.
//
// Sonic Screech is shared damage in a 120-degree cone, so the raid stacks in the arc and splits it
// rather than dodging - whoever eats it alone dies. Auriaya faces her victim, so a main tank holding
// a fixed spot is the entire facing control.
//
// The fight also walks. Every Feral Defender life leaves a Seeping Feral Essence pool that nothing
// despawns until the boss dies, so the raid moves west through fixed stations as they pile up, and
// the station in use is the one with the fewest pools near its two spots.

enum UlduarAuriayaIds
{
    NPC_AURIAYA_SANCTUM_SENTRY = 34014,
    NPC_AURIAYA_FERAL_DEFENDER = 34035,
    NPC_AURIAYA_SEEPING_FERAL_ESSENCE = 34098,
};

constexpr float ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

// 64458 on the stalker is a 1s periodic trigger of 64459, whose radius index (8) is 5 yd in both
// difficulties. Two yards on top for tick granularity and position lag.
constexpr float ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS = 7.0f;

// Sonic Screech (64422 / 64688) carries SPELL_ATTR0_CU_SHARE_DAMAGE, so its 60k (10man) / 200k
// (25man) is divided among everyone in the 120-degree cone. Nobody dodges it: the raid stacks in the
// arc and splits it, and whoever eats it alone dies. Auriaya faces her victim, so a main tank that
// holds a fixed spot is the entire facing control - there is nothing to steer.
//
// The stack sits this far from the boss, on the bearing running from her through the main tank. The
// bearing is rounded to this quantum so that small tank drift cannot shuffle twenty bots; one bucket
// is a 3.9 yd arc at the standoff, which the arrival tolerances below absorb.
constexpr float ULDUAR_AURIAYA_RAID_STANDOFF = 20.0f;
constexpr float ULDUAR_AURIAYA_BEARING_QUANTUM = static_cast<float>(M_PI) / 16.0f;
constexpr float ULDUAR_AURIAYA_MAINTANK_SPOT_TOLERANCE = 3.0f;
constexpr float ULDUAR_AURIAYA_RANGED_SPOT_TOLERANCE = 5.0f;
constexpr float ULDUAR_AURIAYA_HEALER_SPOT_TOLERANCE = 8.0f;

// Every Feral Defender life leaves a Seeping Feral Essence pool, the summon has no duration, and
// nothing despawns them until the boss dies - up to 9 per pull. So the fight walks west in fixed
// steps as they pile up, and the station in use is the one with the fewest pools this close to
// either of its two spots. Three is what the confirmed floor supports; a fourth would put the stack
// past x 1908.
//
// 15 yd is the raid's real footprint around a station point - up to 8 yd of arrival tolerance plus
// the pool radius. The metric is a count rather than a clear/fouled verdict on purpose: counts
// against fixed geometry only ever grow, so the chosen station slides west monotonically. Anything
// derived from the boss's live position oscillates instead, because the boss follows the main tank
// and the main tank follows this verdict.
constexpr int ULDUAR_AURIAYA_STATION_COUNT = 3;
constexpr float ULDUAR_AURIAYA_STATION_FOUL_RADIUS = 15.0f;
constexpr float ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS = 100.0f;

// How far a bot may drift off its anchor to clear a pool, and how close the Feral Defender has to be
// to the boss before melee will swing at it. It re-rolls aggro constantly, so an ungated melee would
// spend the fight chasing it around the room.
constexpr float ULDUAR_AURIAYA_ESSENCE_LEASH = 12.0f;
constexpr float ULDUAR_AURIAYA_MELEE_DEFENDER_RANGE = 15.0f;

// Auriaya's lane, ULDUAR_AURIAYA_STATION_COUNT entries each. The nominal raid points are only ever
// used to retire a station - the stack's real anchor comes off the live boss.
extern const Position ULDUAR_AURIAYA_MAINTANK_SPOTS[];
extern const Position ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[];

// Window in which a counterable fear can land, for the shared anti-fear component. Auriaya's
// Terrifying Screech runs on a fixed 35s cycle, so the whole fight counts.
bool AuriayaFearWindowActive(PlayerbotAI* botAI);

// Auriaya. Resolved by entry rather than "find target": that value walks only the bot's own threat
// list, so every bot fighting a Sanctum Sentry or the Feral Defender would fail to see the boss and
// silently lose its cone dodge and void-zone dodge.
Unit* GetAuriaya(PlayerbotAI* botAI);
bool AuriayaEncounterActive(PlayerbotAI* botAI);

// Anything that pulls - targeting, the sentry taunt, the anchors - waits for this instead of mere
// presence. A Sanctum Sentry's JustEngagedWith calls SetInCombatWithZone on Auriaya, so her flag is
// the whole encounter's, however the raid opens.
bool IsAuriayaEngaged(PlayerbotAI* botAI);

// Sanctum Sentries first: they stay dead and their Strength of the Pack (64369) buffs Auriaya while
// they live, where the Feral Defender only feigns and comes back. Feign is why the Defender goes
// through IsDownOrFeigning - it sits at 1 HP and unselectable between lives, still "alive".
Unit* GetAuriayaFocusTarget(PlayerbotAI* botAI);

// A Sanctum Sentry that is not already on the off-tank. Two spawn with the boss, so picking simply
// "the first alive sentry" would leave the second one loose forever once the first was taunted.
Unit* GetAuriayaLooseSentry(PlayerbotAI* botAI, Player* tank);

// Live Seeping Feral Essence pools around any object. GetCreatureListWithEntryInGrid filters
// nothing, hence the explicit alive check; the stalkers are non-selectable, which rules out
// "possible targets". Pass the smallest radius that answers the question - this runs per bot per
// tick on the dodge path.
std::vector<Unit*> CollectAuriayaEssencePools(WorldObject* from, float radius);

// Every pool in the room, taken off the boss once per bot per tick and shared by everything that
// needs the whole floor rather than the bot's own feet. Empty while she is not in sight. The
// reference is good for the tick that asked for it and no longer - copy it to keep it.
std::vector<Unit*> const& GetAuriayaRoomPools(PlayerbotAI* botAI);

// Which station the fight is standing on: the one with the fewest pools near either of its two spots,
// ties to the lowest index. roomPools is CollectAuriayaEssencePools(boss, ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS).
// Only the main tank reads this. Everyone else picks up the move through the boss, so no two bots can
// disagree.
int GetAuriayaStationIndex(std::vector<Unit*> const& roomPools);

// Where this bot belongs and how far it may stray before walking back. The main tank gets the
// station's fixed spot; ranged and healers get a point derived from the live boss and tank, which is
// what keeps the split working when a human tanks. Melee and the off-tank are unanchored, and get
// false. Trigger and action both go through here so they cannot disagree.
bool GetAuriayaAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance);

// Same, for a caller that already holds the boss. roomPools is optional and only the main tank reads
// it; null falls back to GetAuriayaRoomPools, so passing it saves nothing but a freshness check.
bool GetAuriayaAnchor(PlayerbotAI* botAI, Player* bot, Unit* boss, std::vector<Unit*> const* roomPools,
                      Position& out, float& tolerance);

#endif
