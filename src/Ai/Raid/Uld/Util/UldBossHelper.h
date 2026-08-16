/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDBOSSHELPER_H
#define PLAYERBOTS_ULDBOSSHELPER_H

#include "AiObject.h"
#include "AiObjectContext.h"
#include "EventMap.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "UldScripts.h"
#include <string>
#include <unordered_map>
#include <vector>

constexpr uint32 ULDUAR_MAP_ID = 603;

enum UlduarIDs
{
    // Iron Assembly
    SPELL_LIGHTNING_TENDRILS_10_MAN = 61887,
    SPELL_LIGHTNING_TENDRILS_25_MAN = 63486,
    SPELL_OVERLOAD_10_MAN = 61869,
    SPELL_OVERLOAD_25_MAN = 63481,
    SPELL_OVERLOAD_10_MAN_2 = 63485,
    SPELL_OVERLOAD_25_MAN_2 = 61886,
    SPELL_RUNE_OF_POWER = 64320,
    // NPC_STEELBREAKER / NPC_MOLGEIM / NPC_BRUNDIR come from core ulduar.h via UldScripts.h
    SPELL_FUSION_PUNCH = 61903,
    SPELL_OVERWHELMING_POWER = 64637,

    // Kologarn
    NPC_LEFT_ARM = 32933,
    NPC_RIGHT_ARM = 32934,
    NPC_RUBBLE = 33768,
    NPC_KOLOGARN_EYEBEAM_LEFT = 33632,
    NPC_KOLOGARN_EYEBEAM_RIGHT = 33802,

    // Overhead Smash / One-Armed Overhead Smash both trigger 63355. 64002 is the -25% variant, which
    // nothing on this core applies - it is read too so the swap survives a core change.
    SPELL_CRUNCH_ARMOR = 63355,
    SPELL_CRUNCH_ARMOR_ALT = 64002,

    // Grip victims ride the right arm and are instakilled on expiry unless the arm releases them.
    SPELL_STONE_GRIP_10 = 62166,
    SPELL_STONE_GRIP_25 = 63981,

    SPELL_FOCUSED_EYEBEAM_10_2 = 63346,
    SPELL_FOCUSED_EYEBEAM_10 = 63347,
    SPELL_FOCUSED_EYEBEAM_25_2 = 63976,
    SPELL_FOCUSED_EYEBEAM_25 = 63977,

    // Hodir
    NPC_SNOWPACKED_ICICLE = 33174,
    NPC_TOASTY_FIRE = 33342,
    SPELL_FLASH_FREEZE = 61968,
    SPELL_BITING_COLD_PLAYER_AURA = 62039,
    // Hodir hard mode (3-min timed kill): free the flash-frozen helpers, then exploit their buffs.
    NPC_HODIR_FLASH_FREEZE_BLOCK = 32938,  // ice block encasing a frozen helper NPC; kill it to free them
    SPELL_HODIR_STORM_CLOUD = 65123,       // shaman buff on a random player; base id, difficulty-mapped at runtime

    // Freya
    NPC_SNAPLASHER = 32916,
    NPC_STORM_LASHER = 32919,
    NPC_DETONATING_LASHER = 32918,
    NPC_ANCIENT_WATER_SPIRIT = 33202,
    NPC_ANCIENT_CONSERVATOR = 33203,
    NPC_HEALTHY_SPORE = 33215,
    NPC_EONARS_GIFT = 33228,
    GOBJECT_NATURE_BOMB = 194902,
    SPELL_ATTUNED_TO_NATURE = 62519,  // damage reduction Freya carries for the whole wave phase

    // Freya hard mode: Elders left alive permanently empower Freya with an extra ability each.
    // NPC_FREYA comes from core ulduar.h via UldScripts.h.
    NPC_FREYA_IRON_ROOTS = 33088,               // Ironbranch's Iron Roots trap (selectable)
    NPC_FREYA_STRENGTHENED_IRON_ROOTS = 33168,  // Freya's empowered Iron Roots trap (selectable)
    NPC_FREYA_SUN_BEAM = 33170,                 // Freya's Unstable Sun Beam stalker (non-selectable)
    NPC_FREYA_UNSTABLE_SUN_BEAM = 33050,        // Brightleaf's Unstable Sun Beam stalker (non-selectable)
    SPELL_IRON_ROOTS_DAMAGE = 62283,            // DoT on a player trapped by Ironbranch's roots
    SPELL_IRON_ROOTS_FREYA_DAMAGE = 62861,      // DoT on a player trapped by Freya's roots

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
    SPELL_UNBALANCING_STRIKE = 62130,

    // Mimiron
    NPC_LEVIATHAN_MKII = 33432,
    NPC_VX001 = 33651,
    NPC_AERIAL_COMMAND_UNIT = 33670,
    NPC_BOMB_BOT = 33836,
    NPC_ROCKET_STRIKE_N = 34047,
    NPC_ASSAULT_BOT = 34057,
    NPC_PROXIMITY_MINE = 34362,
    SPELL_P3WX2_LASER_BARRAGE_1 = 63293,
    SPELL_P3WX2_LASER_BARRAGE_2 = 63297,
    SPELL_SPINNING_UP = 63414,
    SPELL_SHOCK_BLAST = 63631,
    SPELL_P3WX2_LASER_BARRAGE_3 = 64042,
    SPELL_P3WX2_LASER_BARRAGE_AURA_1 = 63274,
    SPELL_P3WX2_LASER_BARRAGE_AURA_2 = 63300,

    // General Vezax
    SPELL_MARK_OF_THE_FACELESS = 63276,
    SPELL_VEZAX_SHADOW_CRASH = 63277,

    // Yogg-Saron
    ACTION_ILLUSION_DRAGONS = 1,
    ACTION_ILLUSION_ICECROWN = 2,
    ACTION_ILLUSION_STORMWIND = 3,
    NPC_GUARDIAN_OF_YS = 33136,
    NPC_YOGG_SARON = 33288,
    NPC_OMINOUS_CLOUD = 33292,
    NPC_RUBY_CONSORT = 33716,
    NPC_AZURE_CONSORT = 33717,
    NPC_BRONZE_CONSORT = 33718,
    NPC_EMERALD_CONSORT = 33719,
    NPC_OBSIDIAN_CONSORT = 33720,
    NPC_ALEXTRASZA = 33536,
    NPC_MALYGOS_ILLUSION = 33535,
    NPC_NELTHARION = 33523,
    NPC_YSERA = 33495,
    GO_DRAGON_SOUL = 194462,
    NPC_SARA_PHASE_1 = 33134,
    NPC_LICH_KING_ILLUSION = 33441,
    NPC_IMMOLATED_CHAMPION = 33442,
    NPC_SUIT_OF_ARMOR = 33433,
    NPC_GARONA = 33436,
    NPC_KING_LLANE = 33437,
    NPC_DEATHSWORN_ZEALOT = 33567,
    NPC_INFLUENCE_TENTACLE = 33943,
    NPC_DEATH_ORB = 33882,
    NPC_BRAIN = 33890,
    NPC_CRUSHER_TENTACLE = 33966,
    NPC_CONSTRICTOR_TENTACLE = 33983,
    NPC_CORRUPTOR_TENTACLE = 33985,
    NPC_IMMORTAL_GUARDIAN = 33988,
    NPC_LAUGHING_SKULL = 33990,
    NPC_SANITY_WELL = 33991,
    NPC_DESCEND_INTO_MADNESS = 34072,
    NPC_MARKED_IMMORTAL_GUARDIAN = 36064,
    SPELL_SANITY = 63050,
    SPELL_BRAIN_LINK = 63802,
    SPELL_MALADY_OF_THE_MIND = 63830,
    SPELL_SHADOW_BARRIER = 63894,
    SPELL_TELEPORT_TO_CHAMBER = 63997,
    SPELL_TELEPORT_TO_ICECROWN = 63998,
    SPELL_TELEPORT_TO_STORMWIND = 63989,
    SPELL_TELEPORT_BACK = 63992,
    SPELL_CANCEL_ILLUSION_AURA = 63993,
    SPELL_INDUCE_MADNESS = 64059,
    SPELL_LUNATIC_GAZE_YS = 64163,
    SPELL_SQUEEZE = 64125,  // Constrictor Tentacle's grip; base id, difficulty-mapped at runtime
    SPELL_WEAKENED = 64162,  // Immortal Guardian's killable window; Thorim's Titanic Storm executes it
    GO_FLEE_TO_THE_SURFACE_PORTAL = 194625,

    // Algalon the Observer
    PB_NPC_ALGALON = 32871,
    PB_NPC_LIVING_CONSTELLATION = 33052,
    PB_NPC_COLLAPSING_STAR = 32955,
    PB_NPC_BLACK_HOLE = 32953,
    PB_NPC_WORM_HOLE = 34099,
    PB_NPC_UNLEASHED_DARK_MATTER = 34097,
    NPC_ALGALON_ASTEROID_TARGET_1 = 33104,
    NPC_ALGALON_ASTEROID_TARGET_2 = 33105,
    SPELL_ALGALON_BIG_BANG = 64443,
    SPELL_ALGALON_PHASE_PUNCH = 64412,
    SPELL_ALGALON_COSMIC_SMASH = 62301,
    SPELL_ALGALON_BLACK_HOLE_DAMAGE = 62169,

    // Buffs
    SPELL_FROST_TRAP = 13809,

    // Ignis the Furnace Master
    NPC_IGNIS = 33118,
    NPC_IGNIS_IRON_CONSTRUCT = 33121,
    NPC_IGNIS_SCORCHED_GROUND = 33123,
    // Dormant constructs wear this alongside UNIT_FLAG_NOT_SELECTABLE; Activate Construct strips it.
    SPELL_IGNIS_CONSTRUCT_INACTIVE = 38757,
    SPELL_IGNIS_MOLTEN = 62373,
    SPELL_IGNIS_BRITTLE_10 = 62382,
    SPELL_IGNIS_BRITTLE_25 = 67114,
    SPELL_IGNIS_SLAG_POT_10 = 62717,
    SPELL_IGNIS_SLAG_POT_25 = 63477,
    SPELL_IGNIS_STRENGTH_OF_THE_CREATOR = 64473,

    // Auriaya
    NPC_AURIAYA_SANCTUM_SENTRY = 34014,
    NPC_AURIAYA_FERAL_DEFENDER = 34035,
    NPC_AURIAYA_SEEPING_FERAL_ESSENCE = 34098,

    // General Vezax
    NPC_VEZAX_SARONITE_VAPORS = 33488,
    NPC_VEZAX_SARONITE_ANIMUS = 33524,

    // Flame Leviathan hard mode (each tower left standing empowers the boss and
    // spawns that tower's periodic ground hazard).
    NPC_FL_THORIM_HAMMER_TARGET = 33364,     // Storm: static lightning-strike marks
    NPC_FL_MIMIRONS_INFERNO_TARGET = 33369,  // Flame: moving fire trail
    NPC_FL_HODIRS_FURY_TARGET = 33108,       // Frost: chases a random player then drops frost

    // Flame Leviathan. Vehicle entries come from core ulduar.h via UldScripts.h; these are the
    // boss's own spells and the units the vehicles interact with.
    SPELL_FL_PURSUED = 62374,          // random vehicle every 31s, 35s; the boss then drives at it
    SPELL_FL_GATHERING_SPEED = 62375,  // +5% speed, stacks to 20, re-applied every 15s
    SPELL_FL_BATTERING_RAM = 62376,    // used on the pursued target inside 15 yd
    SPELL_FL_FLAME_VENTS = 62396,      // 10s channel; Electroshock interrupts it
    SPELL_FL_MISSILE_BARRAGE = 62400,
    NPC_FL_TURRET = 33139,             // boss-mounted turrets; never worth a vehicle's cast
    NPC_FL_DEFENSE_TURRET = 33142,
    NPC_FL_POOL_OF_TAR = 33090,        // faction 1965, i.e. the boss's; harmless to the raid
    NPC_FL_PYRITE_CONTAINER = 33189,   // grabbable crate, +25 energy to a demolisher
    NPC_FL_MECHANOLIFT = 33214,        // shot down with Anti-Air Rocket to drop a crate

    // Salvaged Siege Engine (33060) driver seat.
    SPELL_FL_RAM = 62345,
    SPELL_FL_ELECTROSHOCK = 62522,
    SPELL_FL_STEAM_RUSH = 62346,       // CHARGE_DEST along our own facing, not toward a target

    // Salvaged Siege Turret (33067), the siege engine's gunner.
    SPELL_FL_FIRE_CANNON = 62358,      // physical school despite the name; never ignites tar
    SPELL_FL_ANTI_AIR_ROCKET_SIEGE = 62359,
    SPELL_FL_SHIELD_GENERATOR = 64677,

    // Salvaged Chopper (33062).
    SPELL_FL_SONIC_HORN = 62974,
    SPELL_FL_TAR = 62286,              // pool spawns 9 yd BEHIND the chopper
    SPELL_FL_SPEED_BOOST = 62299,

    // Salvaged Demolisher (33109) driver seat.
    SPELL_FL_HURL_BOULDER = 62306,
    SPELL_FL_HURL_PYRITE_BARREL = 62490,
    SPELL_FL_DEMOLISHER_RAM = 62308,
    SPELL_FL_BLUE_PYRITE_DOT = 68605,  // stacking DoT the barrel leaves on the boss, 10s, 10 stacks

    // Salvaged Demolisher Mechanic Seat (33167), the demolisher's gunner.
    SPELL_FL_MORTAR = 62634,
    SPELL_FL_ANTI_AIR_ROCKET = 64979,
    SPELL_FL_GRAB_CRATE = 62479,
    SPELL_FL_INCREASED_SPEED = 62471,

    // Thorim hard mode (arena gauntlet cleared fast enough that Sif joins the fight).
    NPC_SIF = 33196,              // spawns at Thorim's throne, drops into the arena when she joins
    NPC_SIF_BLIZZARD = 32879,     // moving Blizzard ground AoE, only ever exists in hard mode

    // XT-002 Deconstructor. NPC_XT002, NPC_XT_TOY_PILE, NPC_XS013_SCRAPBOT and
    // NPC_HEART_OF_DECONSTRUCTOR come from core ulduar.h via UldScripts.h.
    PB_NPC_XT002_PUMMELLER = 33344,    // aggressive add, wants an off-tank
    PB_NPC_XT002_BOOMBOT = 33346,      // explodes on reaching XT or at 50% health; melee must not touch it
    PB_NPC_XT002_LIFE_SPARK = 34004,   // hard mode only, spawned by an expiring Searing Light
    PB_NPC_XT002_VOID_ZONE = 34001,    // hard mode only, dropped by an expiring Gravity Bomb
    SPELL_XT002_SEARING_LIGHT_10 = 63018,
    SPELL_XT002_SEARING_LIGHT_25 = 65121,
    SPELL_XT002_GRAVITY_BOMB_10 = 63024,
    SPELL_XT002_GRAVITY_BOMB_25 = 64234,
    SPELL_XT002_EXPOSED_HEART = 63849,  // channeled by the Heart while it is vulnerable
    SPELL_XT002_HEARTBREAK = 65737,     // permanent hard-mode empower once the Heart dies
    SPELL_XT002_SUBMERGE = 37751,
    SPELL_MISDIRECTION = 35079,  // hunter buff; its charges are what the redirect action spends

    // Mimiron hard mode ("Firefighter", Big Red Button pressed): mechs empowered, two extra hazards.
    // NPC_MIMIRON (the boss; sits in his pod, never a bot attack target) comes from core ulduar.h via UldScripts.h.
    NPC_FLAMES_INITIAL = 34363,    // fire seed dropped on players, spawns a spreading node (non-selectable)
    NPC_FLAMES_SPREAD = 34121,     // persistent spreading ground-fire node (non-selectable)
    NPC_FROST_BOMB = 34149         // VX-001's Frost Bomb; detonates in a large AoE
};

// Flame Leviathan hard-mode tower bitmask, used to pick which ground hazards to dodge.
enum FlameLeviathanTowerFlags
{
    FL_TOWER_STORM = 0x1,
    FL_TOWER_FLAMES = 0x2,
    FL_TOWER_FROST = 0x4,
    FL_TOWER_LIFE = 0x8,
    FL_TOWER_ALL = 0xF
};

// Vehicle keeps this clear of any active-tower ground hazard (strike / fire / frost).
constexpr float ULDUAR_FL_TOWER_HAZARD_RADIUS = 18.0f;

// Flame Leviathan arena corners, taken from the four NPC_FREYA_WARD_TARGET spawn points in
// boss_flame_leviathan.cpp's SummonTowerHelpers. The kite ring and every "is this inside the
// arena" test are derived from these, so nothing else hardcodes arena geometry.
extern std::vector<Position> const ULDUAR_FL_ARENA_CORNERS;

// Where each vehicle parks relative to the boss. Measured surface-to-surface (added to his combat
// reach), because he has a large model and a raw centre distance would put melee inside him.
constexpr float ULDUAR_FL_SIEGE_STAND_DIST = 8.0f;    // Ram reaches 15 yd; leave headroom
constexpr float ULDUAR_FL_CHOPPER_STAND_DIST = 6.0f;
constexpr float ULDUAR_FL_DEMOLISHER_BAND = 50.0f;    // inside the 10-70 yd hurl band, outside Battering Ram
constexpr float ULDUAR_FL_TAR_LEAD_DIST = 30.0f;      // how far ahead of him the lead chopper parks

// Re-facing costs a spline, so only correct a facing that has really drifted. Roughly 6 degrees.
constexpr float ULDUAR_FL_FACING_TOLERANCE = 0.1f;

// A loose arrival deadband on purpose: it is what stops a group of vehicles piling onto one
// coordinate, and a tight one makes a bot slide in place instead of ever holding still.
constexpr float ULDUAR_FL_ARRIVE_TOLERANCE = 8.0f;

// Only re-issue a MoveTo once the goal has drifted this far. Re-stamping the same destination
// restarts the spline every tick and the vehicle crawls instead of arriving.
constexpr float ULDUAR_FL_REPOSITION_EPSILON = 6.0f;

// Energy costs the strategy gates on. Only the ones used as thresholds are listed; the rest of the
// spellbook is free. Siege engines, turrets and choppers regenerate 20 per 2s against a cap of 100.
constexpr uint32 ULDUAR_FL_ELECTROSHOCK_COST = 20;
constexpr uint32 ULDUAR_FL_RAM_COST = 40;
constexpr uint32 ULDUAR_FL_STEAM_RUSH_COST = 40;
constexpr uint32 ULDUAR_FL_SONIC_HORN_COST = 20;
constexpr uint32 ULDUAR_FL_FIRE_CANNON_COST = 20;
constexpr uint32 ULDUAR_FL_SPEED_BOOST_COST = 50;
constexpr uint32 ULDUAR_FL_INCREASED_SPEED_COST = 25;
constexpr uint32 ULDUAR_FL_PYRITE_BARREL_COST = 5;

// Ram, Electroshock and Sonic Horn are TARGET_UNIT_CONE_ENEMY_104: a frontal cone whose reach is
// the effect radius, not the spell range. Spell::CheckRange short-circuits on RangeEntry ID 1 and
// waves them all through, so these radii are the real limit and nothing else enforces them.
constexpr float ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS = 25.0f;
constexpr float ULDUAR_FL_RAM_CONE_RADIUS = 18.0f;

// A pyrite crate energizes for 25, so grabbing one above this wastes part of it.
constexpr uint32 ULDUAR_FL_CRATE_GRAB_CEILING = 75;

// Demolishers do not regenerate energy (no UNIT_FLAG2_REGENERATE_POWER), so a full tank is 20
// barrels and pyrite crates are the only refill. Barrel above the reserve, boulder below it; the
// reserve is what keeps Increased Speed (25) affordable when Pursued lands.
constexpr uint32 ULDUAR_FL_PYRITE_RESERVE = 30;
constexpr float ULDUAR_FL_CRATE_DETOUR_RADIUS = 60.0f;  // how far a starved demolisher leaves its band

// Wall-hugging kite ring. The corners are inset off the walls so MoveTo has mesh to land on, and
// each 90-degree corner is chamfered into two nodes so a kiting vehicle rounds it instead of
// driving into it while the boss cuts the diagonal.
constexpr float ULDUAR_FL_KITE_WALL_INSET = 15.0f;
constexpr float ULDUAR_FL_KITE_CORNER_CHAMFER = 35.0f;
constexpr float ULDUAR_FL_KITE_ADVANCE_DIST = 30.0f;    // switch nodes on approach, never on arrival
constexpr float ULDUAR_FL_KITE_BOSS_CLEARANCE = 50.0f;  // a node this close to him is not a destination

// Vezax hard mode: ranged/healers stay outside the Saronite Animus' Profound Darkness (63420).
constexpr float ULDUAR_VEZAX_PROFOUND_DARKNESS_RADIUS = 15.0f;

// Shadow Crash strafe: the band each role keeps to Vezax while walking out of the puddle, and the
// arc length of one step. Melee stay inside their reach so the dodge does not cost the whole cast.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_MELEE_MIN_RANGE = 4.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_MELEE_MAX_RANGE = 8.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_RANGED_MIN_RANGE = 13.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_RANGED_MAX_RANGE = 17.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_STEP_YARDS = 5.0f;

// Mimiron: closest a bot trailing the P3Wx2 Laser Barrage will stand to VX-001. Tighter radii keep
// the run needed to stay behind the beam short.
constexpr float ULDUAR_MIMIRON_BARRAGE_MIN_RADIUS = 10.0f;

// Freya hard mode: bots step this far out of an Unstable Sun Beam before it detonates. Exact beam
// radius is DBC, not in the server script, so this is a conservative default to confirm in-game.
constexpr float ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f;

// Hodir hard mode: a bot within this of a Toasty Fire counts as protected (no Biting Cold, Flash
// Freeze exemption), so it only seeks a fire when further out. Matches the Snowpacked Icicle stack range.
constexpr float ULDUAR_HODIR_TOASTY_FIRE_RADIUS = 5.0f;

// Hodir hard mode: a Storm Cloud carrier spreads Storm Power (crit-damage buff) to allies within this
// range. Exact radius is DBC, so this is a conservative default; the carrier joins the pack when it has
// fewer than a couple of allies this close.
constexpr float ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS = 10.0f;

// XT-002: Searing Light and Gravity Bomb both splash around their carrier, so everyone else keeps
// this far away. In hard mode the Gravity Bomb's Void Zone lands on the carrier's feet too.
constexpr float ULDUAR_XT002_DEBUFF_SPREAD_RADIUS = 12.0f;

// XT-002: Boom is roughly 10 yd, and a Boombot also detonates at 50% health, so melee leave margin
// rather than trading the hit for a few swings.
constexpr float ULDUAR_XT002_BOOMBOT_AVOID_RADIUS = 12.0f;

// XT-002 hard mode: Void Zone's Consumption pool. Exact radius is DBC, so this is a conservative
// default to confirm in-game.
constexpr float ULDUAR_XT002_VOID_ZONE_RADIUS = 6.0f;

// XT-002: how far off its anchor a bot is allowed to sit before it walks back. The tank's is loose
// enough to survive XT drifting a step; the ranged one is a deliberate blob rather than a point, so
// bots settle instead of shoving each other off the same pixel.
constexpr float ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE = 3.0f;
constexpr float ULDUAR_XT002_RANGED_SPOT_TOLERANCE = 5.0f;

// XT-002 hard mode: Void Zone parking grid, walked +x/+y from whichever Gravity Bomb origin fits the
// carrier's role. Step is just over the Void Zone diameter so consecutive drops cannot overlap.
constexpr float ULDUAR_XT002_BOMB_GRID_STEP = 6.0f;
constexpr int ULDUAR_XT002_BOMB_GRID_ROWS = 4;
constexpr int ULDUAR_XT002_BOMB_GRID_COLS = 3;

// Anchored on XT rather than the carrier, so one lookup covers both parking origins wherever the
// carrier happens to be standing when the debuff lands.
constexpr float ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS = 100.0f;

// XT-002 normal mode: bots stop damaging the exposed Heart here so an in-flight hit cannot kill it
// and flip the raid into hard mode by accident.
constexpr float ULDUAR_XT002_HEART_SAFE_HP_PCT = 15.0f;

// XT-002 normal mode: the last Heart phase is over below this, so the held burst cooldowns are free.
constexpr float ULDUAR_XT002_FINAL_PUSH_HP_PCT = 25.0f;

// Ignis: a Molten construct turns Brittle once it is this close to one of the room's two water
// triggers (boss_ignis.cpp polls FindNearestCreature(NPC_WATER_TRIGGER, 18.0f) once a second).
constexpr float ULDUAR_IGNIS_WATER_BRITTLE_RADIUS = 18.0f;

// Everyone but the construct tank clears Scorch's burning patch by this much; the tank parks the
// tighter distance instead, so the construct walking into melee range ends up on it stacking Heat.
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS = 8.0f;
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_PARK_DISTANCE = 3.0f;

// A Scorched Ground creature that lands this close to a water trigger never gets lit
// (boss_ignis.cpp skips SPELL_SCORCHED_GROUND within 25 yd of the water), so it stacks no Heat and
// the tank must not park a construct on it.
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS = 25.0f;

// Molten wipes the construct's threat table and adds a heavy fire aura, so everyone who is not the
// construct tank clears this much room. Exact aura radius is DBC, so this is a conservative default.
constexpr float ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS = 12.0f;

// The construct spawns run x 543..631 / y 217..338 and Ignis starts at (586.5, 378.8), so a tank
// standing at a water pool is already ~120 yd from the boss and further still from the far wall.
// Everything Ignis-side searches the grid at this radius rather than going through "nearest npcs",
// which is capped at AiPlayerbot.SightDistance (100 yd) and drops anything out of line of sight.
constexpr float ULDUAR_IGNIS_ROOM_SEARCH_RADIUS = 200.0f;

// Slag Pot ticks for ten seconds and cannot be dispelled or moved out of, but it does not kill from
// full, so healers only pile onto the victim once the ticks have actually opened a gap.
constexpr float ULDUAR_IGNIS_SLAG_POT_HEAL_HP_PCT = 85.0f;

// Off-tank taunts once the active tank reaches this many Phase Punch stacks
constexpr uint32 ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 3;

// Kiter stops this far past the Black Hole (away from the constellation) to stay out of its phase/damage aura
constexpr float ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 5.0f;

// Designated Big Bang soaker: the first alive Shadow Priest in the raid, who stays out and pops
// Dispersion (90% damage reduction) to survive Big Bang instead of hiding in a hole. Big Bang is
// unavoidable — full immunity (Paladin Divine Shield) does not prevent it, only mitigation survives.
// Returns nullptr if the raid has no living Shadow Priest.
Player* GetAlgalonBigBangSoakerPriest(Player* bot);

// XT-002 Deconstructor. These use GetFirstAliveUnitByEntry rather than "find target": the Heart
// never attacks anyone, so it never lands on a bot's threat list and "find target" cannot resolve it.
Unit* GetXT002(PlayerbotAI* botAI);

// The Heart while it is actually vulnerable - alive, selectable and channeling Exposed Heart.
// Damage dealt to it transfers to XT, which makes this window the encounter's damage multiplier.
Unit* GetXT002ExposedHeart(PlayerbotAI* botAI);

// XT is down in a Heart phase: not selectable and not attacking anyone.
bool IsXT002Submerged(PlayerbotAI* botAI);

// Difficulty-mapped debuff ids (the 10- and 25-man versions are separate spells).
uint32 GetXT002SearingLightSpellId(Player* bot);
uint32 GetXT002GravityBombSpellId(Player* bot);

// Yogg-Saron phase reads. Yogg is not reliably on a bot's threat list, so both scan for the creature
// instead of going through "find target".
bool YoggSaronInPhase2(PlayerbotAI* botAI);
bool YoggSaronInPhase3(PlayerbotAI* botAI);

// Windows in which a counterable fear can land, for the shared anti-fear component. Auriaya's
// Terrifying Screech runs on a fixed 35s cycle, so the whole fight counts; Yogg-Saron fears in P2
// (Malady of the Mind, which re-casts on removal) and again in P3 (Deafening Roar).
bool AuriayaFearWindowActive(PlayerbotAI* botAI);
bool YoggSaronFearWindowActive(PlayerbotAI* botAI);

// Auriaya. Resolved by entry rather than "find target": that value walks only the bot's own threat
// list, so every bot fighting a Sanctum Sentry or the Feral Defender would fail to see the boss and
// silently lose its cone dodge and void-zone dodge.
Unit* GetAuriaya(PlayerbotAI* botAI);
bool AuriayaEncounterActive(PlayerbotAI* botAI);

// Sanctum Sentries first: they stay dead and their Strength of the Pack (64369) buffs Auriaya while
// they live, where the Feral Defender only feigns and comes back. Feign is why the Defender goes
// through IsDownOrFeigning - it sits at 1 HP and unselectable between lives, still "alive".
Unit* GetAuriayaFocusTarget(PlayerbotAI* botAI);

// A Sanctum Sentry that is not already on the off-tank. Two spawn with the boss, so picking simply
// "the first alive sentry" would leave the second one loose forever once the first was taunted.
Unit* GetAuriayaLooseSentry(PlayerbotAI* botAI, Player* tank);

// Centroid of the alive raid, ignoring the two tanks so their own positions cannot drag the bearing
// the main tank is steering Auriaya's cone away from.
Position GetAuriayaRaidCentroid(Player* bot);

// Signed shortest rotation, in (-pi, pi], from Auriaya's facing to "away from the raid centroid".
// Returns false when there is no boss or the raid is too close for the bearing to mean anything.
bool GetAuriayaFacingError(PlayerbotAI* botAI, Player* bot, float& error);

// Class taunt, mirroring ICC's IccCastClassTaunt. Non-tank classes return false.
bool UldCastClassTaunt(PlayerbotAI* botAI, Unit* target);


// Dark Rune add the raid should be killing, most urgent first: Sentinel (whirlwinds the raid) >
// Watcher (ranged caster) > Guardian, lowest health first within a tier so the raid focuses one down
// instead of splitting across two. Returns nullptr when none are up.
Unit* GetRazorscaleAddKillTarget(PlayerbotAI* botAI);

// What the skull belongs on right now: the boss whenever she is on the floor - harpoon knockdowns
// included, since she is damageable then - and otherwise the add above.
Unit* GetRazorscaleKillTarget(PlayerbotAI* botAI);

// Ignis the Furnace Master. These search the grid rather than going through "find target": a bot
// parked on an Iron Construct never has Ignis on its threat list, and a dormant construct carries
// UNIT_FLAG_NOT_SELECTABLE, which drops it out of "possible targets" entirely. The room is also
// wider than SightDistance, so the cached "nearest npcs" list goes blind at the water pools.
Unit* GetIgnis(PlayerbotAI* botAI);

// Activated = Ignis has cast Activate Construct on it: selectable, aggressive, and worth tanking.
bool IsIgnisConstructActivated(Unit const* construct);

// 10 Heat stacks from standing in Scorched Ground. Molten also resets the construct's threat.
bool IsIgnisConstructMolten(Unit const* construct);

// A Molten construct brought to the water. One hit of 5000 (10-man) / 3000 (25-man) shatters it.
bool IsIgnisConstructBrittle(Unit const* construct);

Unit* GetIgnisBrittleConstruct(PlayerbotAI* botAI);
Unit* GetIgnisNearestMoltenConstruct(PlayerbotAI* botAI, WorldObject const* from);

// The construct the tank is currently walking through the loop: nearest activated one that has not
// turned Brittle yet. Once it is Brittle the tank is done and the raid takes over.
//
// Sticky per tank: a construct Ignis activates closer to the tank must not steal the walk, because
// the one already picked up has had its threat wiped by Molten and would peel straight into the raid.
Unit* GetIgnisDrivenConstruct(PlayerbotAI* botAI, Player* tank);

// Scorched Ground the construct tank can actually use, i.e. one that is lit. Patches that land
// within ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS of the water are skipped by the core and
// stack no Heat, so parking on one would stall the Heat -> Molten chain for good.
Unit* GetIgnisNearestScorchedGround(PlayerbotAI* botAI, WorldObject const* from);
Position const& GetIgnisNearestWaterPool(WorldObject const* from);

// Assist tank, with no fallback on purpose: a raid without one skips the kiting path entirely rather
// than pulling the boss around behind a construct or feeding a DPS to a Molten one.
Player* GetIgnisConstructTank(PlayerbotAI* botAI, Player* bot);

Player* GetIgnisSlagPotVictim(PlayerbotAI* botAI);
bool IsIgnisSlagPotVictim(Player* bot);

// Kologarn. Everything here resolves by entry rather than through "find target": that value only
// walks the bot's threatened-by-me list, so a tank parked on the body never sees the arms and the
// whole focus plan silently collapses onto the body.
Unit* GetKologarn(PlayerbotAI* botAI);
Unit* GetKologarnRightArm(PlayerbotAI* botAI);
Unit* GetKologarnNearestRubble(PlayerbotAI* botAI, WorldObject const* from);
bool KologarnHasRubble(PlayerbotAI* botAI);

// Nearest rubble not already on the off-tank, so the pickup sweeps the loose ones first instead of
// re-taunting whatever it is already holding. Falls back to the nearest of any.
Unit* GetKologarnLooseRubble(PlayerbotAI* botAI, Player* bot);

// Highest stack count of either Crunch Armor id present. Both are read so the swap keeps working if
// the core ever switches to the -25% variant.
uint8 GetKologarnCrunchArmorStacks(Unit const* unit);

// Grip victims are stunned vehicle passengers on the right arm, so movement orders only fight the
// ride. The body tank is exempt - the core strips the caster's victim from the target list.
bool IsKologarnStoneGripped(Unit const* unit);

// The tank the boss is actually swinging at. Petrifying Breath goes out the moment this one leaves
// melee range, so it doubles as the "is the body covered" test.
Player* GetKologarnBodyTank(PlayerbotAI* botAI);
bool IsKologarnBodyTank(PlayerbotAI* botAI, Player* bot);

// Main tank or first assist tank, whichever is not currently holding the body. Derived rather than
// stored, so the rubble duty follows the taunt swap on its own.
bool IsKologarnOffTank(PlayerbotAI* botAI, Player* bot);

// What a DPS bot should be hitting: rubble for ranged while any are up, otherwise the right arm
// while it lives, otherwise the body. Melee never take the rubble - they stay on the arm so the
// grips keep getting cut short. Triggers and actions both go through this, or the two derivations
// disagree and the bot retargets every tick.
Unit* GetKologarnDpsTarget(PlayerbotAI* botAI, Player* bot);

// The off-tank with no rubble to hold adds damage to the arm, but only while it can still reach the
// body with a taunt.
Unit* GetKologarnOffTankTarget(PlayerbotAI* botAI, Player* bot);

// The eye chasing this bot, if any. Bystanders get the nearest eye inside the react radius instead.
Unit* GetKologarnEyebeamChasing(PlayerbotAI* botAI, Player* bot);
Unit* GetKologarnNearestEyebeam(PlayerbotAI* botAI, Player* bot, float radius);

// Held to the dead arm's side of the raid, never behind it: -X is the lane an eyebeam target runs
// down, and the guide keeps it clear.
Position GetKologarnRubbleHoldSpot(PlayerbotAI* botAI, Unit* rubble);

// Next step of the escape run, clamped to the walkway so it never crosses the pit. The entrance
// lane (-X) is preferred, per the guide, but the eye lives 10s and that lane is only ~5s of running,
// so the run turns along the walkway rather than parking against the clamp when it runs out.
Position GetKologarnEyebeamEscapeStep(Player* bot, Unit* eye);

// Flame Leviathan. The whole fight is from vehicles, and a trigger and its action must call the
// same helper here or the two derivations disagree about who is doing what.

// Resolved by entry, never through "find target" or "attackers": threat on this fight belongs to
// the vehicle creature rather than the bot player, so the boss is usually absent from both.
Unit* FlameLeviathanBoss(PlayerbotAI* botAI);

// Everything else stays inert until this is true, so the raid can still drive into the arena and
// pull normally instead of being dragged at the boss the moment it seats.
bool FlameLeviathanEngaged(PlayerbotAI* botAI);

// The vehicle a bot is actually riding into battle. A gunner sits in seat 0 of a turret creature
// that is itself bolted into the siege engine / demolisher, so this walks one link up for them.
Unit* FlameLeviathanRiddenVehicle(Player* bot);

// Seat 0 of the turret and mechanic seat both carry CAN_CONTROL, so IsInVehicle(true) is true for
// a gunner and MoveTo would happily steer the bolted-on turret model. Gate movement on this.
bool FlameLeviathanIsDriver(Player* bot);

// Pursued lands on whichever unit the boss's spell picked, which may be the ridden vehicle or the
// parent underneath it, so both links are checked.
bool FlameLeviathanIsPursued(Player* bot);

bool FlameLeviathanIsVentChanneling(Unit* boss);

// True for exactly one siege engine at a time: highest energy, guid breaking ties. Casting drops
// the actor to the back of its own queue, so the duty rotates with no shared state. The Ram energy
// reserve reads this too, so the fuel travels with the duty.
bool FlameLeviathanIsVentInterrupter(PlayerbotAI* botAI, Player* bot);

// Lowest-guid chopper driver that is not currently Pursued. It runs ahead of the boss dropping tar
// in his path; a Pursued chopper is already driving away from him and drops tar for free.
bool FlameLeviathanIsTarLead(PlayerbotAI* botAI, Player* bot);

bool FlameLeviathanInArena(Position const& pos, float margin = 0.0f);

Position FlameLeviathanRearPoint(Unit* boss, float standDist);
Position FlameLeviathanLeadPoint(Unit* boss);

// Eight nodes hugging the arena walls, corners chamfered. Built once from ULDUAR_FL_ARENA_CORNERS.
std::vector<Position> const& FlameLeviathanKiteRing();

constexpr float ULDUAR_KOLOGARN_AXIS_Z_PATHING_ISSUE_DETECT = 420.0f;
constexpr float ULDUAR_KOLOGARN_EYEBEAM_RADIUS = 3.0f;

// Kologarn stands at (1797.15, -24.40) facing o=pi, so the entrance is -X and the arms split along
// Y. The walkway runs from the Shattered Walkway Door (x 1740.84) to the broken span at x 1782,
// beyond which boss_kologarn_pit_kill_bunny instakills anything that falls in.
constexpr float ULDUAR_KOLOGARN_ROOM_SEARCH_RADIUS = 100.0f;
constexpr float ULDUAR_KOLOGARN_WALKWAY_X_MIN = 1745.0f;
constexpr float ULDUAR_KOLOGARN_WALKWAY_X_MAX = 1780.0f;
constexpr float ULDUAR_KOLOGARN_WALKWAY_Y_MIN = -48.0f;
constexpr float ULDUAR_KOLOGARN_WALKWAY_Y_MAX = -2.0f;
constexpr float ULDUAR_KOLOGARN_WALKWAY_Z = 448.0f;

// Beam 63346 is a 3 yd AoE and the eye chases at 5.5 yd/s against a player's 7.0, so bots start
// moving well before it lands and running actually outpaces it.
constexpr float ULDUAR_KOLOGARN_EYEBEAM_REACT_RADIUS = 8.0f;
constexpr float ULDUAR_KOLOGARN_EYEBEAM_SAFE_DISTANCE = 12.0f;
constexpr float ULDUAR_KOLOGARN_EYEBEAM_RUN_STEP = 10.0f;

// Rubble outrun players (8.0 vs 7.0 yd/s), so the off-tank holds them rather than kiting. The offset
// is lateral, toward the dead arm's side: -X is the eyebeam escape lane and has to stay clear.
constexpr float ULDUAR_KOLOGARN_RUBBLE_HOLD_OFFSET = 18.0f;

// Crunch Armor is -20% armour per stack, 4 stacks, 45s, on a 14s Smash timer.
constexpr uint8 ULDUAR_KOLOGARN_CRUNCH_ARMOR_SWAP_STACKS = 2;

// Taunt range, so the off-tank only wanders off to DPS the arm while it can still take the body back.
constexpr float ULDUAR_KOLOGARN_TAUNT_RANGE = 30.0f;
constexpr float ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD = 429.6094f;
constexpr float ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

// Thorim hard mode: bots clear Sif's moving Blizzard, and ranged/healers keep this far from
// Sif herself so her point-blank Frost Nova (cast after she teleports next to a target) misses.
constexpr float ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 12.0f;
constexpr float ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS = 12.0f;

// Mimiron hard mode: bots flee a persistent fire node when this close (cells are small), and clear
// the Frost Bomb's larger explosion. Exact radii are DBC, so these are conservative defaults to
// confirm in-game.
constexpr float ULDUAR_MIMIRON_FLAMES_RADIUS = 5.0f;
constexpr float ULDUAR_MIMIRON_FROST_BOMB_RADIUS = 12.0f;
constexpr float ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

// Sonic Screech (64422) is a 120-degree cone per spell_cone, and HasInArc takes the full arc, not
// the half-angle. Seeping Feral Essence's radius is DBC, so 10 yd is a conservative guess.
constexpr float ULDUAR_AURIAYA_SONIC_SCREECH_CONE = 2.0f * static_cast<float>(M_PI) / 3.0f;
constexpr float ULDUAR_AURIAYA_SONIC_SCREECH_RANGE = 45.0f;
constexpr float ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS = 10.0f;

// Main tank steering Auriaya's cone away from the raid. The tank holds still inside the tolerance,
// because a bot that never stops moving never lands a cast; the step is small so the arc stays on
// the navmesh, and a raid huddled on the boss gives a bearing too noisy to chase.
constexpr float ULDUAR_AURIAYA_FACING_TOLERANCE = 0.15f;
constexpr float ULDUAR_AURIAYA_FACING_ARC_STEP = 0.125f;
constexpr float ULDUAR_AURIAYA_FACING_MIN_RAID_DIST = 8.0f;
constexpr float ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 300.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 200.0f;
constexpr float ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS = 50.0f;

// Yogg-Saron reduced-Keeper hard mode: a bot whose Sanity (63050, 100 stacks) is at or below this
// pulls behind Yogg and faces away to conserve it. With Freya absent there are no Sanity Wells, so the
// drain is one-way - kept low so only near-Insane bots pull out. Confirm in-game.
constexpr uint32 ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD = 15;

extern const Position ULDUAR_IGNIS_WATER_POOL_WEST;
extern const Position ULDUAR_IGNIS_WATER_POOL_EAST;
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
extern const Position ULDUAR_THORIM_JUMP_END_POINT;
extern const Position ULDUAR_THORIM_PHASE2_TANK_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE1RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE1MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE2RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE2MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE3RANGE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE2_SIDE3MELEE_SPOT;
extern const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
extern const Position ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT;
extern const Position ULDUAR_YOGG_SARON_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;
extern const Position ULDUAR_XT002_MAINTANK_SPOT;
extern const Position ULDUAR_XT002_RANGED_SPOT;
extern const Position ULDUAR_XT002_SEARING_LIGHT_SPOT;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED;

class RazorscaleBossHelper : public AiObject
{
public:
    // Enums and constants specific to Razorscale
    enum RazorscaleUnits : uint32
    {
        UNIT_RAZORSCALE          = 33186,
        UNIT_DARK_RUNE_SENTINEL  = 33846,
        UNIT_DARK_RUNE_WATCHER   = 33453,
        UNIT_DARK_RUNE_GUARDIAN  = 33388,
        UNIT_DEVOURING_FLAME     = 34188,
    };

    enum RazorscaleGameObjects : uint32
    {
        GO_RAZORSCALE_HARPOON_1 = 194519,
        GO_RAZORSCALE_HARPOON_2 = 194541,
        GO_RAZORSCALE_HARPOON_3 = 194542,
        GO_RAZORSCALE_HARPOON_4 = 194543,
    };

    enum RazorscaleSpells : uint32
    {
        SPELL_SENTINEL_WHIRLWIND = 63806,
        SPELL_STUN_AURA         = 62794,
        SPELL_FUSE_ARMOR        = 64821
    };

    static constexpr uint32 FUSEARMOR_THRESHOLD = 2;

    // The Devouring Flame stalker's tick (64704 / 64733) carries radius index 8. The clear radius adds
    // the margin a step needs to actually leave the patch rather than stopping on its edge.
    static constexpr float DEVOURING_FLAME_RADIUS = 5.0f;
    static constexpr float DEVOURING_FLAME_CLEAR_RADIUS = DEVOURING_FLAME_RADIUS + 2.0f;

    // Constants for arena parameters
    static constexpr float RAZORSCALE_FLYING_Z_THRESHOLD = 440.0f;
    static constexpr float RAZORSCALE_ARENA_CENTER_X = 587.54f;
    static constexpr float RAZORSCALE_ARENA_CENTER_Y = -175.04f;
    static constexpr float RAZORSCALE_ARENA_RADIUS = 30.0f;

    // Harpoon cooldown (seconds)
    static constexpr time_t HARPOON_COOLDOWN_DURATION = 5;

    // Structure for harpoon data
    struct HarpoonData
    {
        uint32 gameObjectEntry;
    };

    explicit RazorscaleBossHelper(PlayerbotAI* botAI)
        : AiObject(botAI), _boss(nullptr) {}

    bool UpdateBossAI();
    Unit* GetBoss() const;

    bool IsGroundPhase() const;
    bool IsFlyingPhase() const;

    // Same phase reads against a boss unit the caller already holds, for code that must not run
    // UpdateBossAI() first - it reassigns the raid's tank roles as a side effect.
    static bool IsGroundPhaseFor(Unit* boss);
    static bool IsFlyingPhaseFor(Unit* boss);

    // Nearest live Devouring Flame patch within radius of the bot, or nullptr.
    static Unit* FindDevouringFlameNear(PlayerbotAI* botAI, float radius);

    // Centres of the live patches within radius of the bot. The dodge sweep tests a dozen-plus
    // candidate destinations against the same set, and one grid search beats one per candidate.
    static void CollectDevouringFlames(Player* bot, float radius, std::vector<Position>& out);

    // True when one of the collected patches covers (x, y).
    static bool DevouringFlameBlocks(std::vector<Position> const& flames, float x, float y);

    // True when a Devouring Flame patch covers (x, y). Searched around the bot, so the radius has to
    // reach a destination he is not standing on yet as well as the patch's own reach around it.
    static bool DevouringFlameBlocks(Player* bot, float x, float y);

    static bool IsHarpoonReady(GameObject* harpoonGO);
    static void SetHarpoonOnCooldown(GameObject* harpoonGO);
    GameObject* FindNearestHarpoon(float x, float y, float z) const;

    static const std::vector<HarpoonData>& GetHarpoonData();

    void AssignRolesBasedOnHealth();
    bool AreRolesAssigned() const;
    bool CanSwapRoles() const;

private:
    Unit* _boss;

    // A map to track the last role swap *per bot* by their GUID
    static std::unordered_map<ObjectGuid, std::time_t> _lastRoleSwapTime;

    // The cooldown that applies to every bot
    static const std::time_t _roleSwapCooldown = 10;

    static std::unordered_map<ObjectGuid, time_t> _harpoonCooldowns;
};

// template <class BossAiType>
// class GenericBossHelper : public AiObject
// {
// public:
//     GenericBossHelper(PlayerbotAI* botAI, std::string name) : AiObject(botAI), _name(name) {}
//     virtual bool UpdateBossAI()
//     {
//         if (!bot->IsInCombat())
//         {
//             _unit = nullptr;
//         }
//         if (_unit && (!_unit->IsInWorld() || !_unit->IsAlive()))
//         {
//             _unit = nullptr;
//         }
//         if (!_unit)
//         {
//             _unit = AI_VALUE2(Unit*, "find target", _name);
//             if (!_unit)
//             {
//                 return false;
//             }
//             _target = _unit->ToCreature();
//             if (!_target)
//             {
//                 return false;
//             }
//             _ai = dynamic_cast<BossAiType*>(_target->GetAI());
//             if (!_ai)
//             {
//                 return false;
//             }
//             _event_map = &_ai->events;
//             if (!_event_map)
//             {
//                 return false;
//             }
//         }
//         if (!_event_map)
//         {
//             return false;
//         }
//         _timer = _event_map->GetTimer();
//         return true;
//     }
//     virtual void Reset()
//     {
//         _unit = nullptr;
//         _target = nullptr;
//         _ai = nullptr;
//         _event_map = nullptr;
//         _timer = 0;
//     }

// protected:
//     std::string _name;
//     Unit* _unit = nullptr;
//     Creature* _target = nullptr;
//     BossAiType* _ai = nullptr;
//     EventMap* _event_map = nullptr;
//     uint32 _timer = 0;
// };

#endif
