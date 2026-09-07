/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERTHORIM_H
#define PLAYERBOTS_ULDENCOUNTERTHORIM_H

#include "ObjectGuid.h"
#include "Position.h"
#include "RaidObs.h"
#include "UldData.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

// Thorim.
//
// Two halves that share nothing. The gauntlet is a corridor walk past the Runic Colossus, whose
// Runic Smash rolls a blast wave up one side at a time and whose Runic Barrier answers every melee
// swing with 2000 arcane. Phase 2 is a stationary fight on the arena floor where Chain Lightning
// arcs 5 yd from victim to victim and Lightning Charge fires a 75 degree cone at whichever pillar
// orb just lit up.

enum UlduarThorimIds
{
    // Thorim
    NPC_DARK_RUNE_ACOLYTE_I = 32886,
    NPC_CAPTURED_MERCENARY_SOLDIER_ALLY = 32885,
    NPC_CAPTURED_MERCENARY_SOLDIER_HORDE = 32883,
    NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY = 32908,
    NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE = 32907,
    NPC_JORMUNGAR_BEHEMOT = 32882,
    NPC_DARK_RUNE_WARBRINGER = 32877,
    NPC_DARK_RUNE_EVOKER = 32878,
    NPC_DARK_RUNE_CHAMPION = 32876,
    NPC_DARK_RUNE_COMMONER = 32904,
    NPC_IRON_RING_GUARD = 32874,
    NPC_RUNIC_COLOSSUS = 32872,
    NPC_ANCIENT_RUNE_GIANT = 32873,
    NPC_DARK_RUNE_ACOLYTE_G = 33110,
    NPC_IRON_HONOR_GUARD = 32875,
    NPC_THORIM_THUNDER_ORB = 33378,  // the pillar orb that lights up 5s before Lightning Charge
    SPELL_UNBALANCING_STRIKE = 62130,

    // The Runic Colossus corridor smash: a 5s cast that lights one row of hand bunnies, then rolls a
    // 10 yd blast wave up the corridor on that side. Cancelled the moment the Colossus is engaged.
    SPELL_THORIM_RUNIC_SMASH_LEFT = 62057,
    SPELL_THORIM_RUNIC_SMASH_RIGHT = 62058,
    // -51% damage taken plus a 2000 arcane damage shield on every melee swing. Recast every 20s for
    // its own 20s duration, so it is up for effectively the whole approach.
    SPELL_THORIM_RUNIC_BARRIER = 62338,
    // Lightning Charge has no cast bar. The only warning is this aura landing on a Thunder Orb;
    // SpellInfoCorrections patches its amplitude to 5000ms, so it ticks once, 5s before the cone.
    SPELL_THORIM_LIGHTNING_ORB_VISUAL = 62186,
    // Phase 1's orb marker, the counterpart to the visual above. It sits on one Thunder Orb for 15s
    // and triggers Lightning Shock (62017) once a second: ~3k nature at 35 yd, measured in 3D.
    SPELL_THORIM_CHARGE_ORB = 62016,

    // Thorim hard mode (arena gauntlet cleared fast enough that Sif joins the fight).
    NPC_SIF = 33196,              // spawns at Thorim's throne, drops into the arena when she joins
    NPC_SIF_BLIZZARD = 32879,     // moving Blizzard ground AoE, only ever exists in hard mode
};

constexpr float ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD = 429.6094f;
constexpr float ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

// Thorim hard mode: bots clear Sif's moving Blizzard, and ranged/healers keep this far from
// Sif herself so her point-blank Frost Nova (cast after she teleports next to a target) misses.
// DBC radius is 13, and the searcher applies through IsWithinDistInMap, which adds both object sizes
// on top - Rune of Death is 13 in the DBC and was measured landing at 15.4. At 12 the dodge stopped
// while still standing in it.
constexpr float ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 15.0f;
constexpr float ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS = 12.0f;

// Cheap first gate for everything Thorim owns, and it takes both halves. Distance alone does not
// separate the wings: Hodir's room sits 136-176 yd from the arena centre against a corridor that runs
// out to 126 yd, and a radius through that 10 yd gap would be luck rather than a gate. Height does -
// the gauntlet is at z 412, the arena floor at 420, and Hodir's floor at 433.
constexpr float ULDUAR_THORIM_ENCOUNTER_PROXIMITY = 200.0f;
constexpr float ULDUAR_THORIM_WING_MAX_Z = 425.0f;

// The one part of the encounter that ceiling cannot admit: the ramp and the upper hallway run
// z 425-438, over Hodir's 433. Height cannot separate those two and neither can distance - the far
// end of the hallway is 177 yd out, inside Hodir's own 136-176 band - so this half of the gate is a
// box. X does the work: Hodir sits at x 1980, and nothing else in Ulduar is in here.
constexpr float ULDUAR_THORIM_BALCONY_BOX_MIN_X = 2100.0f;
constexpr float ULDUAR_THORIM_BALCONY_BOX_MAX_X = 2205.0f;
constexpr float ULDUAR_THORIM_BALCONY_BOX_MIN_Y = -455.0f;
constexpr float ULDUAR_THORIM_BALCONY_BOX_MAX_Y = -280.0f;
constexpr float ULDUAR_THORIM_BALCONY_BOX_MAX_Z = 450.0f;

// AiPlayerbot.SightDistance caps the "possible targets" values at 100 yd, and the Colossus is 131 yd
// from the top pair of corridor waypoints - so its telegraph needs a wider, targeted grid lookup or
// bots at the head of the corridor never see the hand go up.
constexpr float ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE = 150.0f;

// One telegraph is worth 5s of cast, 1s of delay and 3.5s of wave travel. Held a little past that so
// the lane does not flip back while the last bunny is still firing.
constexpr uint32 ULDUAR_THORIM_RUNIC_SMASH_LATCH_MS = 10000;

// Both grid sweeps below are raid-wide answers, so they are computed once per instance per interval
// rather than once per bot. Sharing the result also guarantees no two bots disagree about which lane
// is hot or which orb is lit.
constexpr uint32 ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS = 500;

// Runic Barrier's damage shield only punishes melee swings, so the bail is a health band rather than
// a hard stop: below the first, step out of melee range; above the second, walk back in.
constexpr float ULDUAR_THORIM_BARRIER_BAIL_HEALTH_PCT = 55.0f;
constexpr float ULDUAR_THORIM_BARRIER_RESUME_HEALTH_PCT = 80.0f;
constexpr float ULDUAR_THORIM_BARRIER_BAIL_DISTANCE = 14.0f;  // clear of the 9.1 yd melee reach

// Thorim's combat reach is 6.25 and a player's is 1.5, so melee connects out to 9.1 yd centre to
// centre. A radius 8 ring is inside that, and puts its three slots 11.3 yd apart - clear of Chain
// Lightning, whose jump radius is 5.0 here (spell_jump_distance overrides the 10 yd DBC default).
constexpr float ULDUAR_THORIM_MELEE_RING_RADIUS = 8.0f;
constexpr uint8 ULDUAR_THORIM_MELEE_SLOTS = 3;

// Five, so ten ranged and healers stand two deep instead of the three to four the old trio piled up.
constexpr uint8 ULDUAR_THORIM_RANGED_SLOTS = 5;

// The off-tank sits just off the main tank's bearing: close enough to taunt through the Unbalancing
// Strike swap, far enough that Chain Lightning does not treat the pair as one clump.
constexpr float ULDUAR_THORIM_OFFTANK_BEARING_OFFSET = 0.3491f;  // 20 degrees

// Reach then hold. A tight deadband against a ring recomputed from a moving boss has the bot sliding
// in place forever, and a moving bot casts nothing - the Sapphiron air phase failure.
constexpr float ULDUAR_THORIM_RING_ARRIVE_TOLERANCE = 3.0f;
constexpr float ULDUAR_THORIM_RING_REPOSITION_TOLERANCE = 5.0f;

// How far out the add priority will reach. Arena adds land 19-24 yd from the centre and a ranged bot
// sits up to 14 yd the other side of it, so 50 covers the room without letting an arena bot lock onto
// something down the corridor.
constexpr float ULDUAR_THORIM_DPS_TARGET_RANGE = 50.0f;

// Within one priority tier, how much closer a candidate has to be before the bot drops what it is
// already swinging at. Without a margin the pick oscillates between two adds that are the same
// distance away and the bot never finishes a cast.
constexpr float ULDUAR_THORIM_TARGET_SWITCH_MARGIN = 8.0f;

// How far a melee bot will go for a higher-priority add before settling for the next tier down. Arena
// adds land 19-24 yd from the centre and a Champion is on somebody a second or two later, so anything
// past this has moved twice by the time the bot arrives. Ranged have no such limit - they hit an
// Evoker from where they already stand, and it is melee that were measured spending the fight running.
constexpr float ULDUAR_THORIM_MELEE_TARGET_REACH = 15.0f;

// spell_cone gives 62466 a 75 degree arc at 150 yd. The margin covers Thorim re-orienting onto the
// orb between the tick that picks a rotation and the tick the bot finishes walking it.
constexpr float ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE = 1.3090f;   // 75 degrees
constexpr float ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN = 0.2618f;       // 15 degrees
constexpr float ULDUAR_THORIM_LIGHTNING_CHARGE_RANGE = 150.0f;

// The box boss_thorim.cpp scans every 5s for a living player. Find nobody in it and Thorim summons
// the Lightning Orb, which wipes the raid outright - so these are copied from GetArenaPlayer() and
// must never be widened.
constexpr float ULDUAR_THORIM_ARENA_BOX_MIN_X = 2085.0f;
constexpr float ULDUAR_THORIM_ARENA_BOX_MAX_X = 2185.0f;
constexpr float ULDUAR_THORIM_ARENA_BOX_MIN_Y = -305.0f;
constexpr float ULDUAR_THORIM_ARENA_BOX_MAX_Y = -214.0f;
constexpr float ULDUAR_THORIM_ARENA_BOX_MAX_Z = 425.0f;

// Arena adds all land 19-24 yd from the centre and the nearest box edge is 42 yd out, so this holds
// the arena squad well inside the scan while still letting them chase anything that spawns. The
// corridor mouth is 45.8 yd away, which is what makes the radius alone enough to keep them out of it.
constexpr float ULDUAR_THORIM_ARENA_LEASH_RADIUS = 30.0f;

// What holds a corridor bot's pet, which has no room to be held to. Matches the range the picker
// looks for a target in, so a pet further out than this is past anything its owner could even see
// to kill. Wide enough for a real chase: pets sit 15-21 yd out normally and peak around 50.
constexpr float ULDUAR_THORIM_PET_OWNER_LEASH_RADIUS = ULDUAR_THORIM_DPS_TARGET_RANGE;

// The gauntlet stops taking bodies once the arena would drop this low.
constexpr uint32 ULDUAR_THORIM_ARENA_MIN_MEMBERS = 3;

// The phase 1 arena formation. The tank and the melee on him hold the centre; ranged and healers ring
// them from outside the Dark Rune Champion's Whirlwind, close enough that an add anywhere in the pile
// is still in range. Two radii rather than one so a 25 man ring is not shoulder to shoulder.
//
// These are as wide as the room allows. Arena adds jump to 19-24 yd from the centre, so anything at
// or past 19 drops ranged into the landing zone, and the navmesh gives out past ~26 yd on the south
// side. Wider matters twice over: Whirlwind is an 8 yd circle on an add standing on the tank, and
// Deafening Thunder is a 15 yd blast centred on whoever the Stormhammer hit, so a tight ring hands
// one hammer the whole squad's cast speed.
constexpr float ULDUAR_THORIM_ARENA_RING_INNER = 13.0f;
constexpr float ULDUAR_THORIM_ARENA_RING_OUTER = 18.0f;
constexpr uint8 ULDUAR_THORIM_ARENA_RING_INNER_SLOTS = 5;

// Charge Orb hangs on a Thunder Orb 13.5 yd above the floor and its 35 yd radius is measured in 3D,
// so the field cuts the floor as a 32.3 yd circle - which is why a bot three yards from a victim
// never took a tick. The margin covers the bot still walking when the next tick lands.
constexpr float ULDUAR_THORIM_CHARGED_ORB_RADIUS = 32.3f;
constexpr float ULDUAR_THORIM_CHARGED_ORB_MARGIN = 4.0f;

// Melee are leashed to the tank spot rather than to the box: 24 yd is the furthest an arena add ever
// lands from the centre, so it costs no uptime, and it keeps the pile 21 yd short of the lever gate.
// Standing at the gate is worse than it looks - boss_thorim_arena_npcs::CanAIAttack drops any target
// past x 2180, so a bot that drifts there stops being attackable and the add re-rolls onto a healer.
constexpr float ULDUAR_THORIM_ARENA_MELEE_LEASH = 24.0f;

extern const Position ULDUAR_THORIM_NEAR_ARENA_CENTER;
extern const Position ULDUAR_THORIM_NEAR_ENTRANCE_POSITION;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2;
extern const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3;
// The upper hallway, from the second doors to Thorim's platform. Chained in order, and deliberately
// not down the middle: two Paralytic Field bunnies sit on the centre line at (2134.9, -390.8) and
// (2134.9, -339.7), each a 12 yd circle that stuns for 15s with no cast bar and no world object to
// see. The chamber between the two doorways is ~40 yd wide, so the way past is to hug the east side
// and come back to the middle for the north doorway. Worst leg clears a bunny by 15.8 yd.
extern const Position ULDUAR_THORIM_BALCONY_1;
extern const Position ULDUAR_THORIM_BALCONY_2;
extern const Position ULDUAR_THORIM_BALCONY_3;
extern const Position ULDUAR_THORIM_BALCONY_4;
extern const Position ULDUAR_THORIM_BALCONY_5;
extern const Position ULDUAR_THORIM_JUMP_START_POINT;
extern const Position ULDUAR_THORIM_JUMP_END_POINT;
// Where Thorim ends up, and he is dragged this far west so the melee on him cannot reach the ranged
// camp. Chain Lightning jumps 5 yd body to body, so the two piles touching is what turns it from a
// three man hit into an eight man one. The melee ring at radius 8 is walkable the whole way round,
// the western arc on rim polys.
extern const Position ULDUAR_THORIM_PHASE2_TANK_SPOT;
// Five spots east of him, 23 to 30 yd out, which leaves the nearest 15 yd clear of the melee ring.
// That clearance is the point: Chain Lightning cannot cross it, so a chain stops at whoever shares an
// anchor instead of running the length of the raid. Tightest pair here is 8.1 yd, also over the jump.
//
// The price is Lightning Charge, a 75 degree cone off the boss aimed at whichever of the seven orbs
// lit up. Out here one or two cones cover each spot instead of exactly one, so it roughly doubles -
// from 3% of phase 2 incoming, against Chain Lightning's 20 to 38.
//
// Sif's Blizzard bunny loops the outside of the room, so the rim belongs to her. Worst clearance on
// these is 16.3 yd.
extern const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE4_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE5_SPOT;
// Only used when Thorim's live position cannot produce a ring point, and StaticMeleeSpot only takes
// one within 11 yd of him, so these have to sit on the radius 8 ring around the tank spot. Park him
// somewhere else and they are dead weight.
extern const Position ULDUAR_THORIM_PHASE2_MELEE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_MELEE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_MELEE3_SPOT;
// Inside the melee ring rather than on it, and off the camp's bearing: taunt range for the
// Unbalancing Strike swap without adding a body between the boss and the ranged.
extern const Position ULDUAR_THORIM_PHASE2_OFFTANK_SPOT;

// Both scans below are raid-wide answers, so they are folded once per instance per interval instead
// of once per bot - and sharing the result is also what guarantees no two bots disagree about which
// corridor lane is hot.
struct ThorimEncounterState
{
    // Held rather than re-derived, for the reason Vezax holds his: ranking the raid by guid every
    // tick means one death renumbers everyone behind the corpse and the ring shuffles mid-fight.
    RaidObs::ObsGuidMap<uint8> meleeSlots{"thorim.slot"};

    // Each melee bot's bearing off Thorim, struck the first time it needs a phase 2 spot. The point
    // was derived live from the tank's bearing and a rotation re-solved every call, and between them
    // six melee spent 74 to 92% of phase 2 walking - 950 to 1430 yd each, at 58% of the ranged dps.
    std::unordered_map<ObjectGuid, float> ringBearings;

    // The rotation that clears the ring of the Lightning Charge cone, solved once per lit orb and
    // held. Re-solving per tick had a 5 degree search answering 30 degrees differently between two
    // ticks, and every melee walked the difference.
    ObjectGuid ringRotationOrb;
    float ringRotation = 0.0f;
    bool ringRotationHeld = false;

    // 0 = nothing seen yet, otherwise SPELL_THORIM_RUNIC_SMASH_LEFT / _RIGHT. The side is sticky and
    // the timestamp is not: the timestamp says the wave is still rolling, the side says which lane
    // the squad now walks, and that has to outlive the wave or the formation walks straight back.
    RaidObs::ObsValue<uint32> runicSmashSide{"thorim.smashside"};
    uint32 runicSmashSeenMs = 0;
    uint32 smashScanMs = 0;

    // Phase 1's Charge Orb and phase 2's Lightning Charge visual, cached apart. They used to share one
    // slot keyed on the marker, so either caller evicted the other and nearly every call re-swept the
    // grid - 78 turnovers in 15s in one trace, each a 150 yd sweep, with the guid flapping in step.
    // The melee ring rotation hangs off that guid, which is what put six melee on a 90 degree bearing
    // flip several times a second.
    RaidObs::ObsValue<ObjectGuid> chargedOrbGuid{"thorim.chargedorb"};
    uint32 orbScanMs = 0;

    RaidObs::ObsValue<ObjectGuid> lightningOrbGuid{"thorim.lightningorb"};
    uint32 lightningOrbScanMs = 0;

    // Where each bot was sent to get out of the Charge Orb field. A trace otherwise only shows that a
    // bot moved, not which hazard moved it.
    RaidObs::ObsGuidMap<Position> orbEscapes{"thorim.orbescape"};

    // Struck once and then read straight out of the map. The sight-list lookup it replaces stops at
    // AiPlayerbot.SightDistance, and the upper hallway is 100-145 yd from the platform, so every node
    // keyed on the boss went dark the moment the corridor squad climbed the ramp.
    ObjectGuid bossGuid;

    ObjectGuid colossusGuid;
    uint32 colossusScanMs = 0;

    ObjectGuid runeGiantGuid;
    uint32 runeGiantScanMs = 0;

    RaidObs::ObsGuidSet barrierBailing{"thorim.barrierbail"};

    // Which half of the raid each member belongs to, struck once and then left alone. Recomputing it
    // per tick is what let a role predicate flipping mid-fight walk the arena squad into the corridor.
    RaidObs::ObsGuidMap<uint8> squads{"thorim.squad"};
    RaidObs::ObsValue<bool> squadsAssigned{"thorim.squadsassigned"};

    // When the humans' entries in the map above were last refreshed. They are not struck with the
    // rest: the split only ever picks bots, so a human who walks the corridor would otherwise be
    // filed under the arena for the whole trace. Read from position, and only for the label.
    uint32 humanSquadScanMs = 0;

    // The split is struck before the pull, and RaidObs has no session open until the pull, so the
    // change-only emit above lands in a trace that does not exist yet - which is why no Thorim trace
    // has ever carried the one assignment that decides the fight. Re-noted once a session is up.
    bool squadsNoted = false;

    // Which add each bot was told to burn. The trace can otherwise only show GetVictim(), which is
    // where a bot ended up rather than where it was sent.
    RaidObs::ObsGuidMap<ObjectGuid> dpsTargets{"thorim.dpstarget"};

    // Bots the arena node took "follow master" away from, so the reset can hand it back.
    RaidObs::ObsGuidSet followMasterStripped{"thorim.followstripped"};

    // Which pet was last sent back to its owner. Keyed by owner, because that is what a reader has:
    // pets carry no snapshot row, so this note is the only place a trace says a pet left the arena.
    // How far along the upper hallway each bot has walked. Monotonic on purpose: derived purely from
    // position it would hand back the nearest waypoint, and the nearest one behind you is exactly how
    // a squad ends up walking the corridor a second time.
    RaidObs::ObsGuidMap<uint8> balconyStep{"thorim.balcony"};

    RaidObs::ObsGuidMap<ObjectGuid> petRecalls{"thorim.petrecall"};

    // When each pet was last told to come home. Re-issuing MoveFollow every tick restarts the walk and
    // the pet never arrives, and a plain "already commanded" latch would never fire twice for a pet
    // that strays, comes back and strays again. Keyed by pet, so one stray pet cannot mute another.
    std::unordered_map<ObjectGuid, uint32> petRecallMs;

    // Phase 1 arrival latch. Its own set rather than ringArrived below: the two phases are mutually
    // exclusive on the z threshold today, and sharing a latch across that is a trap waiting for the
    // first time it stops being true.
    RaidObs::ObsGuidSet arenaAnchorArrived{"thorim.arenaarrived"};

    // Reach-then-hold latch. A bot that has arrived stops issuing moves until it drifts past the
    // wider tolerance, because a moving bot casts nothing.
    RaidObs::ObsGuidSet ringArrived{"thorim.ringarrived"};

    // The corridor fight is trash to the instance script, so nothing else opens a trace for it.
    bool gauntletTraced = false;

    // Raid icons are cleared once per pull rather than every tick: a human is free to re-mark something
    // mid-fight and having the bots wipe it back off would be worse than the stale mark ever was.
    RaidObs::ObsValue<bool> marksCleared{"thorim.markscleared"};

    // Whether this raid has ever had him in combat. An untouched Thorim looks identical to one that
    // has just reset, and the reset path clears the squad split that the corridor forms up on before
    // the pull - so without the latch the two fight each other every tick.
    bool engagedSeen = false;

    // Who has already had their own latches cleared since he was last in combat. The reset used to be
    // gated on engagedSeen alone, which is raid-wide, so the first bot through closed the gate on the
    // other 24 and they carried ringArrived, balconyStep and their melee slot into the next pull. One
    // trace had ten of thirteen corridor bots start on a step they earned two attempts earlier.
    // Cleared while he is engaged, so every pull gets a fresh round of resets.
    std::unordered_set<ObjectGuid> resetDone;
};

enum class ThorimSquad : uint8
{
    None,
    Arena,
    Gauntlet
};

enum class ThorimPhase2Role : uint8
{
    None,
    MainTank,
    OffTank,
    Ranged,
    MeleeRing
};

// By entry, never "find target": that value walks only the bot's own threat list and matches on the
// localized creature name, so a bot that is on an add goes blind to the boss.
Unit* GetThorim(PlayerbotAI* botAI);

// Wider, targeted lookup rather than the sight-capped target values: the Colossus is 131 yd from the
// top pair of corridor waypoints, and that is exactly where its telegraph has to be visible.
Unit* GetThorimRunicColossus(PlayerbotAI* botAI);

//
// Target priority
//
// The encounter's own creatures, bucketed by entry in one sweep, for the same reason GetThorim reads
// by entry.
struct ThorimEncounterTargets
{
    std::vector<Unit*> acolytes;
    std::vector<Unit*> evokers;
    std::vector<Unit*> champions;
    std::vector<Unit*> warbringers;
    std::vector<Unit*> commoners;
    std::vector<Unit*> guards;  // Iron Ring and Iron Honor Guard, the corridor's own trash
    std::vector<Unit*> trash;   // the six the raid walks in on, before Thorim starts the waves
    Unit* colossus = nullptr;
    Unit* runeGiant = nullptr;
};

void GatherThorimEncounterTargets(PlayerbotAI* botAI, ThorimEncounterTargets& out);

// Whether a bot may hold this target at all. False for Thorim and Sif while he still holds the
// balcony: both are untouchable up there, and ThorimArenaTargetGuardMultiplier reads a target outside
// the arena box as a reason to stop acting, so a bot that picks one stops doing anything at all.
bool ThorimDpsTargetAllowed(PlayerbotAI* botAI, Unit* target);

// What this bot should be attacking, or nullptr to leave the generic picker alone. This is deliberately
// not a raid icon: an icon is a sticky override - RtiTargetValue hands it back before the smart picker
// runs and IsHighPriority pins it - so a wrong mark cannot be corrected until the bot leaves combat.
Unit* GetThorimDpsTarget(PlayerbotAI* botAI, Player* bot, Unit* currentTarget);

// Whether the encounter had a target for this bot on its last pass. Reads the answer GetThorimDpsTarget
// already cached rather than working it out again: the targeting guard asks per action per tick, and
// re-walking the candidate list that often is a grid-list walk nobody needs.
bool ThorimHasDpsTarget(PlayerbotAI* botAI, Player* bot);

// Drops the icons and the pinned target left over from an earlier pull or an earlier target. Both
// outlive the thing they were set for: an icon sits on the group until somebody overwrites it, and
// "prioritized targets" is only reset when a bot leaves combat, so until this runs a bot can be held
// on a corpse or on the balcony for the rest of the fight. Raid-wide half latched to run once.
void ThorimClearStaleMarks(PlayerbotAI* botAI, Player* bot);

//
// Corridor gauntlet
//
constexpr uint8 ULDUAR_THORIM_GAUNTLET_WAYPOINTS = 6;

// The two lanes are index-matched by y - entry i on one side is the same progress down the corridor
// as entry i on the other - which is what makes a lane swap a straight index map. Each lane is
// inside its own hand's 10 yd blast and 15.5-22.9 yd clear of the other's.
Position const& GetThorimGauntletWaypoint(bool leftLane, uint8 index);

// True when `who` is standing at one of this lane's waypoints, reporting the nearest one.
bool ThorimGauntletLaneIndexInLane(WorldObject const* who, bool leftLane, uint8& index);
bool ThorimGauntletLaneIndex(WorldObject const* who, uint8& index, bool& leftLane);

// The waypoint index the squad is at: the master's, or this bot's own when the master matches no
// waypoint at all - walking the centre line, or off fighting an add.
bool ThorimResolveGauntletIndex(PlayerbotAI* botAI, Player* bot, uint8& index);

// The lane the squad should be in. Sticky once a hand has gone up, so the formation does not walk
// everyone back into the lane they just dodged out of; false before the first telegraph, and once
// the Colossus is engaged, because EVENT_RC_RUNIC_SMASH is cancelled in JustEngagedWith.
bool ThorimPreferredGauntletLane(PlayerbotAI* botAI, bool& useLeftLane);

// True only while a wave is still rolling, which is what separates the urgent dodge from the
// standing lane preference.
bool ThorimRunicSmashImminent(PlayerbotAI* botAI);

//
// Upper hallway
//
constexpr uint8 ULDUAR_THORIM_BALCONY_WAYPOINTS = 6;

// How close counts as standing at a balcony waypoint, and at the last one, close enough to jump.
// Wider than the corridor's 5-6 yd because nothing up here is dodging anything - the point is only to
// keep the squad off the centre line. The drop used to ask for 0.5 yd of its own, which an
// exact-waypoint MoveTo will not reliably hit, and a bot could circle the edge for the rest of the
// fight.
constexpr float ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE = 6.0f;

Position const& GetThorimBalconyWaypoint(uint8 index);

// The Ancient Rune Giant, while it is still worth walking to. Its death opens the second doors and
// sets _isHitAllowed on Thorim, so "gone" is what says the hallway is the squad's next job.
Unit* GetThorimAncientRuneGiant(PlayerbotAI* botAI);

// True once the hallway is open: the Giant is dead and Thorim is not, so there is a platform to walk
// to and a boss to hit when the squad gets there.
bool ThorimBalconyOpen(PlayerbotAI* botAI, Player* bot);

// The waypoint this bot should be heading for, advancing the latch when it arrives. Counts a
// waypoint reached on tolerance or on having passed its y, because the hallway runs one way and a
// bot shoved sideways past a point should not turn around for it.
uint8 ThorimAdvanceBalconyStep(Player* bot);

// Runic Barrier is recast every 20s for its own 20s duration, so "stop attacking while it is up"
// would mean never attacking. Non-tank melee back out on a health band instead and keep their
// target, so ranged and instant abilities carry on from outside the shield's reach.
bool ThorimBarrierBailLatched(PlayerbotAI* botAI, Player* bot);

//
// Phase 1 split
//
// The raid fights phase 1 in two halves and the arena half must never empty out: boss_thorim.cpp
// scans a fixed box every 5 seconds and summons a raid-killing Lightning Orb the first time it finds
// nobody alive inside it. There is no grace period, so this is a hard constraint rather than a
// preference.
//
// Set MEMBER_FLAG_MAINTANK in the raid frame. Without it GetMainTankGuid falls back to the first tank
// in roster order, and a human tank ahead of the bot main tank silently takes the role - which sends
// the bot main tank down the corridor and leaves the arena untanked.
bool ThorimSplitActive(PlayerbotAI* botAI);

// Latched on first ask and stable for the rest of the pull. Anyone with no entry - a late arrival -
// holds the arena, because that is the side that cannot wipe the raid by being short.
ThorimSquad GetThorimSquad(PlayerbotAI* botAI, Player* bot);

// The boss script's own box, so the two sides cannot disagree about what counts as "in the arena".
bool ThorimInArenaBox(WorldObject const* who);

// The whole wing, arena and corridor and the hallway above them. Distance alone reaches Hodir's room,
// so above the wing ceiling only the balcony box counts. Every node here is gated on it, which is why
// it is the right shape for a cheap first test.
bool NearThorimEncounter(Player const* bot);

// Melee are held to the tank spot and everyone else to the wider box radius, because melee are the
// only ones who have to walk out to an add at all.
bool ThorimArenaLeashBreached(PlayerbotAI* botAI, Player* bot);

// Pets that have left the boundary their owner's squad is held to, and are not already walking back.
// Nothing in this codebase ever recalls a pet, so one that ends up chasing something down the corridor
// stays there, pulling packs the gauntlet squad has not reached. The arena squad is held to the room;
// the corridor squad has no room, so it is held to the owner.
//
// A boundary and nothing else: this asks where the pet is, never what is standing on it. Pets have
// the resistances and damage reduction to eat this fight, and none of this steers one around a hazard.
bool ThorimStrayPets(PlayerbotAI* botAI, Player* bot, std::vector<Unit*>& out);

// Sends one strayed pet home and records it. Recall only - it never commands an attack, so it cannot
// undo a stay or a follow the way the pet-attack trigger CombatStrategy dropped used to.
void ThorimRecallPet(Player* bot, Unit* pet);

// Where this bot stands in the arena. The tank holds the centre, ranged and healers get a ring slot
// around him, and melee get the centre only while out of combat - pinning them in the fight would
// cost uptime, and the leash is what holds them there instead. Trigger and action both go through
// here so they cannot disagree.
//
// Not gated on ThorimSplitActive: that also requires combat, and the squad has to walk to its own
// side of the room before the pull rather than after it.
bool GetThorimArenaAnchor(PlayerbotAI* botAI, Player* bot, Position& out);

// Updates this bot's arrival latch and reports whether it still needs to walk. Idempotent, so the
// trigger and the action can both ask.
bool ThorimArenaAnchorNeedsMove(PlayerbotAI* botAI, Player* bot, Position const& spot);

// A settled anchor holder, which is the only window the movement guard covers. Outside it the generic
// movers are what bring a bot back, and freezing them permanently is the Void Reaver failure.
bool ThorimArenaAnchorSettled(PlayerbotAI* botAI, Player* bot);

void ThorimNoteFollowMasterStripped(Player* bot);
bool ThorimFollowMasterStripped(Player const* bot);

//
// Phase 2
//
bool ThorimPhase2Active(PlayerbotAI* botAI);
ThorimPhase2Role GetThorimPhase2Role(PlayerbotAI* botAI, Player* bot);
bool TryGetThorimPhase2Spot(PlayerbotAI* botAI, Player* bot, ThorimPhase2Role role, Position& position);

// Updates this bot's arrival latch and reports whether it still needs to walk. Idempotent, so the
// trigger and the action can both ask.
bool ThorimRingNeedsMove(PlayerbotAI* botAI, Player* bot, Position const& spot);

// A settled ring holder, which is the only window the movement guard covers. Outside it the generic
// movers are what bring a bot back, and freezing them permanently is the Void Reaver failure.
bool ThorimMeleeRingSettled(PlayerbotAI* botAI, Player* bot);

// The Thunder Orb carrying markerSpell, or nullptr. Both orb mechanics announce themselves the same
// way and neither has a cast bar the bots can read: SPELL_THORIM_LIGHTNING_ORB_VISUAL is the 5 second
// warning before phase 2's Lightning Charge, SPELL_THORIM_CHARGE_ORB is phase 1's 15 second field.
Unit* ThorimChargedThunderOrb(PlayerbotAI* botAI, uint32 markerSpell);
bool ThorimLightningChargeActive(PlayerbotAI* botAI);

// Where this bot should stand to be clear of the Charge Orb field, or false if it is already clear.
// Pure, so the trigger can ask as often as it likes; the action records the answer separately.
bool ThorimChargedOrbEscape(PlayerbotAI* botAI, Player* bot, Position& out);
void ThorimNoteOrbEscape(Player* bot, Position const& spot);

void ResetThorimEncounterState(Player* bot, bool clearInstance);
bool ThorimEncounterStateIsStale(PlayerbotAI* botAI);
// Nothing to reset means nothing to do, which keeps the reset node from swallowing every tick before
// the pull - Thorim sits at full health on his balcony for the whole gauntlet.
bool ThorimBotHasEncounterState(Player* bot);

#endif
