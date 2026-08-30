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
    // Iron Assembly. The council script casts through Unit::CastSpell, which difficulty-maps every
    // id, so each pair below is 10-man then 25-man and callers test both. The auras come first, the
    // damage they trigger after them - 63485 and 61886 are the Tendrils damage triggers and were
    // once mislabelled here as extra Overload auras, which they never are.
    SPELL_LIGHTNING_TENDRILS_10_MAN = 61887,
    SPELL_LIGHTNING_TENDRILS_25_MAN = 63486,
    SPELL_OVERLOAD_10_MAN = 61869,
    SPELL_OVERLOAD_25_MAN = 63481,
    SPELL_CHAIN_LIGHTNING_10_MAN = 61879,
    SPELL_CHAIN_LIGHTNING_25_MAN = 63479,
    SPELL_LIGHTNING_WHIRL_10_MAN = 61915,
    SPELL_LIGHTNING_WHIRL_25_MAN = 63483,
    SPELL_RUNE_OF_DEATH_10_MAN = 62269,
    SPELL_RUNE_OF_DEATH_25_MAN = 63490,
    SPELL_SHIELD_OF_RUNES_10_MAN = 62274,
    SPELL_SHIELD_OF_RUNES_25_MAN = 63489,
    SPELL_FUSION_PUNCH_10_MAN = 61903,
    SPELL_FUSION_PUNCH_25_MAN = 63493,
    SPELL_OVERWHELMING_POWER_10_MAN = 64637,
    SPELL_OVERWHELMING_POWER_25_MAN = 61888,
    // The rune's ground pulse, reapplied every 0.8s to anything standing within 5 yd of it. One id
    // for both raid sizes, and it is what marks a boss as standing in his own damage buff.
    SPELL_RUNE_OF_POWER = 64320,
    // The damage the auras above trigger. Nothing here is ever cast or tested for - these name the
    // hazard in a trace, where the row has to join onto the damage record that explains it. Only
    // Tendrils is a pair: both Overload auras trigger 61878 and both Overwhelming Power auras
    // trigger 61889.
    SPELL_OVERLOAD_DAMAGE = 61878,
    SPELL_LIGHTNING_TENDRILS_DAMAGE_10_MAN = 61886,
    SPELL_LIGHTNING_TENDRILS_DAMAGE_25_MAN = 63485,
    SPELL_MELTDOWN = 61889,
    // NPC_STEELBREAKER / NPC_MOLGEIM / NPC_BRUNDIR come from core ulduar.h via UldScripts.h

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

    // Hodir. Three distinct icicle entries, and mixing them up breaks the fight. 33169 "Icicle" is
    // the small one that lands every 2s and must be dodged. 33173 "Snowpacked Icicle" is the Flash
    // Freeze drift; it must be dodged only while it is still falling, and it leaves a 7 yd Ice
    // Shards pool where it lands. 33174 "Snowpacked Icicle Target" is the invisible dummy 33173
    // spawns beside itself - non-attackable, and the thing that carries the Safe Area aura Flash
    // Freeze checks for. Only 33174 is a shelter; 33173 is a hazard that happens to mark one.
    NPC_HODIR_ICICLE_SMALL = 33169,
    NPC_HODIR_ICICLE_DRIFT = 33173,
    NPC_SNOWPACKED_ICICLE = 33174,
    NPC_TOASTY_FIRE = 33342,
    NPC_HODIR_FLASH_FREEZE_BLOCK = 32938,   // ice block encasing a frozen helper; kill it to free them
    NPC_HODIR_FLASH_FREEZE_PLAYER = 32926,  // same, on a raider - the next Flash Freeze instakills them
    // Starlight is an 8 yd zone centred on whichever druid helper this raid's faction and size got.
    NPC_HODIR_DRUID_ALLIANCE_10 = 32901,
    NPC_HODIR_DRUID_ALLIANCE_25 = 33325,
    NPC_HODIR_DRUID_HORDE_10 = 32941,
    NPC_HODIR_DRUID_HORDE_25 = 33333,
    SPELL_FLASH_FREEZE = 61968,
    SPELL_BITING_COLD_PLAYER_AURA = 62039,
    SPELL_HODIR_FLASH_FREEZE_TRAPPED = 61969,
    SPELL_HODIR_STARLIGHT = 62807,
    SPELL_HODIR_TOASTY_FIRE_AURA = 62821,
    SPELL_HODIR_FROZEN_BLOWS = 62478,  // base id, difficulty-mapped at runtime
    SPELL_HODIR_STORM_CLOUD = 65123,   // base id, difficulty-mapped at runtime
    SPELL_HODIR_STORM_POWER = 63711,   // what the carrier hands out; also difficulty-mapped

    // Freya
    NPC_SNAPLASHER = 32916,
    NPC_STORM_LASHER = 32919,
    NPC_DETONATING_LASHER = 32918,
    NPC_ANCIENT_WATER_SPIRIT = 33202,
    NPC_ANCIENT_CONSERVATOR = 33203,
    NPC_HEALTHY_SPORE = 33215,
    NPC_EONARS_GIFT = 33228,
    GOBJECT_NATURE_BOMB = 194902,
    // +8% healing received per stack, 150 stacks on engage: Freya cannot be killed until the wave
    // adds have taken it off her, so damage on her before then is wasted.
    SPELL_ATTUNED_TO_NATURE = 62519,

    // Freya hard mode: Elders left alive permanently empower Freya with an extra ability each.
    // NPC_FREYA comes from core ulduar.h via UldScripts.h.
    NPC_FREYA_IRON_ROOTS = 33088,               // Ironbranch's Iron Roots trap (selectable)
    NPC_FREYA_STRENGTHENED_IRON_ROOTS = 33168,  // Freya's empowered Iron Roots trap (selectable)
    NPC_FREYA_SUN_BEAM = 33170,                 // Freya's Unstable Sun Beam stalker (non-selectable)
    NPC_FREYA_UNSTABLE_SUN_BEAM = 33050,        // Brightleaf's Unstable Sun Beam stalker (non-selectable)
    SPELL_IRON_ROOTS_DAMAGE = 62283,            // DoT on a player trapped by Ironbranch's roots
    SPELL_IRON_ROOTS_FREYA_DAMAGE = 62861,      // DoT on a player trapped by Freya's roots

    // Applied to allies within 6 yd of a Healthy Spore; grants immunity to Conservator's Grip.
    // 62541 is what the spore casts on itself - this is the spell that actually lands on players.
    SPELL_POTENT_PHEROMONES = 64321,

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

    // Mimiron
    NPC_LEVIATHAN_MKII = 33432,
    NPC_LEVIATHAN_MKII_CANNON = 34071,  // rides the MK II and does its Plasma Blast / Napalm casting
    NPC_VX001 = 33651,
    NPC_AERIAL_COMMAND_UNIT = 33670,
    NPC_BOMB_BOT = 33836,
    NPC_ROCKET_STRIKE_N = 34047,
    NPC_ASSAULT_BOT = 34057,
    NPC_PROXIMITY_MINE = 34362,
    NPC_MIMIRON_DB_TARGET = 33576,  // the barrage beams aim at this; it laps the room clockwise
    NPC_JUNK_BOT = 33855,
    NPC_MAGNETIC_CORE = 34068,
    SPELL_P3WX2_LASER_BARRAGE_1 = 63293,  // the only one that damages: a 104 degree cone, 50000 yd
    SPELL_P3WX2_LASER_BARRAGE_2 = 63297,  // SPELL_EFFECT_DUMMY, places a beam visual, harmless
    SPELL_SPINNING_UP = 63414,
    SPELL_SHOCK_BLAST = 63631,
    SPELL_P3WX2_LASER_BARRAGE_3 = 64042,  // SPELL_EFFECT_DUMMY, places a beam visual, harmless
    SPELL_P3WX2_LASER_BARRAGE_AURA_1 = 63274,
    SPELL_P3WX2_LASER_BARRAGE_AURA_2 = 63300,
    SPELL_MIMIRON_PLASMA_BLAST = 62997,
    SPELL_MIMIRON_NAPALM_SHELL = 63666,
    SPELL_MIMIRON_MAGNETIC_FIELD = 64668,
    ITEM_MIMIRON_MAGNETIC_CORE = 46029,  // 100% drop from the Assault Bot; grounds the ACU
    SPELL_MIMIRON_MAGNETIC_CORE_AURA = 64436,  // the 20s grounding itself, not the field 64668

    // General Vezax
    SPELL_MARK_OF_THE_FACELESS = 63276,
    // 62660 is the cast, 62659 the 10 yd impact, and 63277 the 8 yd field it leaves behind for 20s.
    // Only the field is reactable - the impact resolves the instant the missile lands.
    SPELL_VEZAX_SHADOW_CRASH_CAST = 62660,
    SPELL_VEZAX_SHADOW_CRASH_DMG = 62659,
    SPELL_VEZAX_SHADOW_CRASH_FIELD = 63277,
    SPELL_VEZAX_SEARING_FLAMES = 62661,
    SPELL_VEZAX_SURGE_OF_DARKNESS = 62662,
    SPELL_VEZAX_SARONITE_VAPORS_PUDDLE = 63322,
    // Cast by a dying vapor on itself, so it fires exactly once at the moment the puddle appears.
    SPELL_VEZAX_SARONITE_VAPORS_SPAWN = 63323,
    SPELL_VEZAX_SARONITE_BARRIER = 63364,

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
    SPELL_ALGALON_BIG_BANG_25 = 64584,
    SPELL_ALGALON_PHASE_PUNCH = 64412,
    // The one "phased" aura in the encounter: the holes apply it, and so does the fifth Phase Punch
    // stack through 64417. Wearing it is what makes Big Bang miss you.
    SPELL_ALGALON_BLACK_HOLE_DAMAGE = 62169,

    // Buffs
    SPELL_FROST_TRAP = 13809,

    // Ignis the Furnace Master
    NPC_IGNIS = 33118,
    NPC_IGNIS_IRON_CONSTRUCT = 33121,
    NPC_IGNIS_SCORCHED_GROUND = 33123,
    // Dormant constructs wear this alongside UNIT_FLAG_NOT_SELECTABLE; Activate Construct strips it.
    SPELL_IGNIS_CONSTRUCT_INACTIVE = 38757,
    // Self-buff on the boss for the 3 s he is rooted and rotation-locked; the patch spawns when it
    // falls off, from the orientation frozen at cast start.
    SPELL_IGNIS_SCORCH = 62546,
    SPELL_IGNIS_FLAME_JETS = 62680,
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
    NPC_VEZAX = 33271,
    NPC_VEZAX_SARONITE_VAPORS = 33488,
    NPC_VEZAX_SARONITE_ANIMUS = 33524,

    // Flame Leviathan hard mode (each tower left standing empowers the boss and
    // spawns that tower's periodic ground hazard).
    NPC_FL_THORIM_HAMMER_TARGET = 33364,     // Storm: static lightning-strike marks
    NPC_FL_MIMIRONS_INFERNO_TARGET = 33369,  // Flame: moving fire trail
    // Frost: walks to a target, roots itself on arrival, then fires 5s later where it stopped. The
    // strike carries a 60s stun (62297) with no mechanic and no dispel type, so a vehicle that eats
    // one is out of the fight for a minute and nothing can shorten it.
    NPC_FL_HODIRS_FURY_TARGET = 33108,

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

    // Hodir's Fury's 60s stun, on the vehicle as well as the crew. Cleared by fire, see
    // ULDUAR_FL_HURL_BOULDER_MAX_RANGE.
    SPELL_FL_HODIRS_FURY_STUN = 62297,

    // Salvaged Demolisher (33109) driver seat.
    // Hurl Boulder triggers Boulder 62307, whose third effect triggers Flames 65045; Mortar 62634
    // triggers 62635, whose third triggers Flames 65044. spell_linked_spell maps both Flames to
    // -62297 ("Flames remove ice"), so either one thaws a frozen vehicle. Free, and the boulder's
    // own damage is enemy-only, so aiming one at a frozen ally costs nothing.
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
    // Permanent hard-mode empower once the Heart dies. The core casts 65737 and lets the difficulty
    // conversion pick, so 25-man raids carry 64193 and a check against 65737 alone never fires there.
    SPELL_XT002_HEARTBREAK_10 = 65737,
    SPELL_XT002_HEARTBREAK_25 = 64193,
    SPELL_XT002_SUBMERGE = 37751,
    SPELL_MISDIRECTION = 35079,  // hunter buff; its charges are what the redirect action spends

    // Mimiron hard mode ("Firefighter", Big Red Button pressed): mechs empowered, two extra hazards.
    // NPC_MIMIRON (the boss; sits in his pod, never a bot attack target) comes from core ulduar.h via UldScripts.h.
    NPC_FLAMES_INITIAL = 34363,    // fire seed dropped on players, spawns a spreading node (non-selectable)
    NPC_FLAMES_SPREAD = 34121,     // persistent spreading ground-fire node (non-selectable)
    NPC_FROST_BOMB = 34149,        // VX-001's Frost Bomb; detonates in a large AoE
    NPC_EMERGENCY_FIRE_BOT = 34147  // puts the flames out; three spawn every 45s
};

// Assembly of Iron. Every distance is measured against the spell that motivates it, and the whole
// formation is bounded by what map 603 actually has floor for: from the anchor below, navprobe
// reports 8/8 headings on mesh at 20 and 30 yd, but at 40 the 45 and 135 degree diagonals settle to
// Z -27.7 and -438, and at 50 three of eight headings leave the mesh entirely. Nothing here sits
// outside 30 yd, and the formation uses cardinals so no slot can drift onto a bad diagonal.

// Overload 61878 is 20,000 nature plus a knockdown in 20 yd. Lightning Tendrils 61886/63485 is
// 3000 (10-man) / 5000 (25-man) a second in 18 - not the 10 yd the written guides give, which is
// 61884, a dummy. The extra yards are arrival slack: a bot that stops on the boundary is still in.
constexpr float ULDUAR_IRON_ASSEMBLY_OVERLOAD_RADIUS = 20.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_OVERLOAD_CLEARANCE = 25.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_TENDRILS_RADIUS = 18.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_TENDRILS_CLEARANCE = 28.0f;

// Rune of Death 62269/63490: a 13 yd persistent area aura ticking 2750 shadow every half second for
// 30s. Search wide enough to see one dropped anywhere in the formation.
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_RADIUS = 13.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE = 15.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_SEARCH_RADIUS = 40.0f;

// Meltdown 61889 is 29,250 nature in 15 yd, centred on whoever Overwhelming Power expires on. The
// carrier dies either way - walking this far is what stops it taking the melee with them, and every
// death it causes is another permanent +25% on Steelbreaker via Electrical Charge.
constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_RADIUS = 15.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_MELTDOWN_CLEARANCE = 20.0f;

// Rune of Power pulses 64320 to everything within 5 yd, worth +50% damage, and the rune lives 60s.
// Molgeim drops it on DoSelectLowestHpFriendly, which is a council member rather than a player, so
// the tank walks his boss out of it while the ranged walk in. Capped travel matters: without the cap
// a rune landing on Brundir would drag the entire ranged group into Overload range.
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_RADIUS = 5.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_SOAK_MAX_TRAVEL = 25.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_POWER_DRAG_DISTANCE = 10.0f;

// Formation, all on cardinal bearings from the anchor. Brundir is parked at 28 rather than the 25
// his own Overload needs, so the stack sits 38 yd off him and never has to react to it at all -
// which is what keeps a bot standing still and able to interrupt Lightning Whirl.
constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_BEARING = 0.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_RADIUS = 28.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_STEELBREAKER_BEARING = 2.3562f;  // 3*pi/4
constexpr float ULDUAR_IRON_ASSEMBLY_MOLGEIM_BEARING = 3.9270f;       // 5*pi/4
constexpr float ULDUAR_IRON_ASSEMBLY_MELEE_BOSS_RADIUS = 16.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_STACK_BEARING = 3.1416f;         // pi
constexpr float ULDUAR_IRON_ASSEMBLY_STACK_RADIUS = 10.0f;
// Once Brundir is the last one up his isolation protects nothing, so the raid closes to a second
// point 25 yd short of him: outside Overload, inside caster range of him.
constexpr float ULDUAR_IRON_ASSEMBLY_BRUNDIR_LAST_STACK_RADIUS = 3.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_SLOT_TOLERANCE = 3.0f;

// The hall's two doors spawn at (1671.31, 120.70) and (1501.49, 119.70), 84 and 86 yd from the
// anchor, so this bubble stops short of both. Sight range is 100 yd with no line-of-sight check, so
// without it the council drives bot behaviour from out in the corridor.
constexpr float ULDUAR_IRON_ASSEMBLY_ARENA_RADIUS = 78.0f;
constexpr float ULDUAR_IRON_ASSEMBLY_ARENA_HEIGHT = 10.0f;
// A tank that has drifted this far off its spot is walking, not parked.
constexpr float ULDUAR_IRON_ASSEMBLY_TANK_SPOT_TOLERANCE = 4.0f;

// Static Disruption 61912/63494 is 5000 nature in 6 yd plus +75% nature damage taken in 5, and it
// picks a target beyond 10 yd - so it is a ranged and healer problem, never a melee one. It only
// exists from Steelbreaker's phase 2, which the normal kill order never reaches, so this ring is
// hard mode only and everyone stacks otherwise. 16 slots at 18 yd sit 7.0 yd apart and none of them
// lands further than 29.4 yd from Steelbreaker's spot, inside caster range.
constexpr float ULDUAR_IRON_ASSEMBLY_SPREAD_RING_RADIUS = 18.0f;
constexpr uint8 ULDUAR_IRON_ASSEMBLY_SPREAD_SLOTS = 16;

// Overload, Lightning Tendrils and Meltdown have no world object behind them, so a trace can only
// know their geometry if the encounter writes it. RaidObs::NoteHazard emits on every call, so this
// paces the rows per instance and doubles as their ttl. A second is fine for a 6s channel and tracks
// Brundir closely enough as he drifts through a 16s Tendrils.
constexpr uint32 ULDUAR_IRON_ASSEMBLY_HAZARD_NOTE_INTERVAL_MS = 1000;

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

// What the strike itself actually covers, which is smaller than the band above: Hodir's Fury 10 yd
// (62297), Mimiron's Inferno 9 (62910), Thorim's Hammer 7 (62912). The scan radius is the warning;
// this is the circle a vehicle has to be out of.
constexpr float ULDUAR_FL_TOWER_BLAST_RADIUS = 10.0f;

// Flame Leviathan arena corners, taken from the four NPC_FREYA_WARD_TARGET spawn points in
// boss_flame_leviathan.cpp's SummonTowerHelpers. The kite ring and every "is this inside the
// arena" test are derived from these, so nothing else hardcodes arena geometry.
extern std::vector<Position> const ULDUAR_FL_ARENA_CORNERS;

// Where each vehicle parks relative to the boss. Measured surface-to-surface (added to his combat
// reach), because he has a large model and a raw centre distance would put melee inside him.
// Ram's 18 yd cone is measured with the boss's 15 yd combat reach added, so 8 here is comfortably
// inside it. Distance was never what made Ram miss - facing was.
constexpr float ULDUAR_FL_SIEGE_STAND_DIST = 8.0f;

// Sonic Horn is a 35 yd cone and his reach adds another 15, so a chopper has no reason to sit on top
// of him - 6 yd put it 21 yd from his centre, permanently inside Battering Ram's 25. Out here it
// keeps every shot and is only in the blast while it chooses to be, i.e. on the tar lead.
constexpr float ULDUAR_FL_CHOPPER_STAND_DIST = 20.0f;
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
constexpr float ULDUAR_FL_SONIC_HORN_CONE_RADIUS = 35.0f;

// Each cone also has its own width, from world.spell_cone; Spell::SelectImplicitConeTargets falls
// back to 60 degrees for a spell with no row. These are half-widths already, because HasInArc
// splits the arc it is handed. Nothing widens them at cast time: isInFront passes no target radius,
// so the boss's 15 yd combat reach buys no slack, and CAST_ANGLE_IN_FRONT (120 degrees) is wider
// than all three - which means CastVehicleSpell will not turn the vehicle for these on its own.
// A siege engine 45 degrees off him passes that gate, casts, and Electroshock's 30 degrees finds
// nothing: 71 casts landed once over five pulls.
constexpr float ULDUAR_FL_ELECTROSHOCK_CONE_HALF_ANGLE = 30.0f * float(M_PI) / 180.0f;
constexpr float ULDUAR_FL_RAM_CONE_HALF_ANGLE = 50.0f * float(M_PI) / 180.0f;
constexpr float ULDUAR_FL_SONIC_HORN_CONE_HALF_ANGLE = 25.0f * float(M_PI) / 180.0f;

// Cooldowns the module keeps itself, because Spell::SendSpellCooldown returns early for a creature
// caster and nothing enforces Electroshock's 10 s otherwise. The short one is for a shot that went
// out and did not stop the channel: retry, but not at tick rate - each attempt costs 20 energy.
constexpr uint32 ULDUAR_FL_ELECTROSHOCK_COOLDOWN_MS = 10000;
constexpr uint32 ULDUAR_FL_ELECTROSHOCK_RETRY_MS = 3000;

// Battering Ram is TARGET_DEST_TARGET_ENEMY at radius index 20: a 25 yd sphere centred on whoever he
// is pursuing, not a cone off his front. Distance to him tells you nothing; distance to the Pursued
// vehicle is the whole answer.
constexpr float ULDUAR_FL_BATTERING_RAM_RADIUS = 25.0f;

// He only casts it inside IsWithinCombatRange(victim, 15.0f), which adds both combat reaches on top.
constexpr float ULDUAR_FL_BATTERING_RAM_CAST_RANGE = 15.0f;

// Hurl Boulder is a lobbed shot with a real minimum range (RangeIndex 164), so a demolisher parked
// on top of a frozen ally cannot thaw it and has to back off first. Mortar (RangeIndex 37) has no
// minimum but only reaches 50.
constexpr float ULDUAR_FL_HURL_BOULDER_MIN_RANGE = 10.0f;
constexpr float ULDUAR_FL_HURL_BOULDER_MAX_RANGE = 70.0f;
constexpr float ULDUAR_FL_MORTAR_MAX_RANGE = 50.0f;

// Gap to hold between vehicles of one class: Hodir's Fury's 10 yd blast plus enough that a vehicle
// drifting inside its arrival deadband does not close it. Half the fight ran with four or more
// vehicles inside one such circle, so a single reticle could freeze a whole class for 60 s.
constexpr float ULDUAR_FL_STATION_SPACING = 12.0f;

// The widest the fan may open. Past this the outer slots stop being "behind him" at all.
constexpr float ULDUAR_FL_STATION_MAX_ARC = 2.0f * float(M_PI) / 3.0f;

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

// Shadow Crash lands as a missile: 62660 is instant with Speed 10, so its destination is fixed at
// cast time and a bot 26 yd out has ~2.6s to leave it. The impact (62659) is 10 yd and knocks back,
// so standing still is not an option either way. 12 yd of travel is about 1.7s and leaves time to
// walk back into the field the missile drops.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS = 10.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_CLEARANCE = 12.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_SEARCH_RADIUS = 25.0f;

// FindNearestPositionClearOfHazards answers with the nearest clear spot, which for a caster standing
// on the impact is as likely to point inward as outward. Ranged keep to the band their blocks live
// in, so a dodge never dumps one in the melee ball. Healers get their own ceiling instead of that
// floor: stepping past 12.5 yd from the boss puts them back in the Shadow Crash target pool, which is
// the whole reason they stand where they do.
constexpr float ULDUAR_VEZAX_DODGE_BAND_MIN = 16.0f;
constexpr float ULDUAR_VEZAX_DODGE_BAND_MAX = 36.0f;
constexpr float ULDUAR_VEZAX_HEALER_DODGE_BAND_MAX = 12.0f;

// Both Vezax ground hazards are 8 yd: the Shadow Crash field (63277) and the puddle a killed
// Saronite Vapor leaves on its corpse (63322). Plus a yard of slack, since a bot that stops exactly
// on the boundary is still taking ticks.
constexpr float ULDUAR_VEZAX_HAZARD_RADIUS = 8.0f;
constexpr float ULDUAR_VEZAX_HAZARD_CLEARANCE = 9.0f;
// Wide enough to cover the whole ranged formation. Callers that only care about what is under
// their own feet pass a tighter radius - the sweep is two grid searches and the raid runs it often.
constexpr float ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS = 60.0f;
constexpr float ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS = 25.0f;

// The field is worth +100% magic damage, +100% cast speed and -70% mana cost for 20s, which is the
// only real answer to Aura of Despair - so mana casters walk into it rather than out of it. Healers
// never do: it also cuts healing done by 75%, which halves their throughput outright.
// Capped travel, or every caster abandons its slot for one 8 yd circle and Shadow Crash catches the
// lot of them next cast.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL = 15.0f;

// The puddle deals 100 * 2^stacks every 4s and hands back half as mana. Leave once the next tick
// would take this share of current health - a fixed stack cap kills undergeared 10-man healers and
// leaves value on the table for geared 25-man ones.
constexpr float ULDUAR_VEZAX_VAPOR_SOAK_MAX_TICK_HP_PCT = 0.35f;

// Vezax formation. He spawns dead centre of his room facing north (o 1.658) and the raid comes down
// from the north: the trash pack sits at y 109-137, and the only door - 194750 at y 31.5 - is
// DOOR_TYPE_PASSAGE, so it opens when he dies, on the way to Yogg. Everything the raid stands on goes
// north of the anchor, leaving the southern half for the Mark of the Faceless spots. Every radius
// below is navprobe-verified on map 603: the floor is a WMO, flat at Z 342.378, with no holes at any
// bearing inside 45 yd.
constexpr float ULDUAR_VEZAX_ARC_ORIENTATION = 1.5708f;

// Three blocks, not three arcs. A block is a centre bearing, two rows, and three slots per row at a
// fixed chord spacing; each row's arc width is derived from that spacing, so the shape holds at any
// radius.
//
// Ranged take two of them because the field is 8 yd and the impact is 10. At 3.7 yd spacing a group's
// furthest pair sits 7.97 yd apart, so a field landing on any slot covers the other five - and two
// groups 43.8 yd apart cannot both be caught by one impact.
//
// Healers sit at 11.25 because SelectTarget skips anything within 3 yd of Vezax plus *both* combat
// reaches - 3 + 8 + 1.5 = 12.5 - so a healer inside that line is never a Shadow Crash target, and no
// crash then lands nearer the boss than 14.5 yd. Their ring is measured from the boss, not the
// anchor: the exclusion is his, and a ranged pull can leave him yards off his spawn.
constexpr float ULDUAR_VEZAX_RANGED_GROUP_OFFSET = 1.0f;
constexpr float ULDUAR_VEZAX_HEALER_RADIUS = 11.25f;
constexpr float ULDUAR_VEZAX_HEALER_SPACING = 4.5f;
constexpr float ULDUAR_VEZAX_RANGED_NEAR_RADIUS = 24.5f;
constexpr float ULDUAR_VEZAX_RANGED_FAR_RADIUS = 27.5f;
// A thirteenth ranged bot would otherwise get no slot and fall through to the melee de-clump, which
// walks it onto the boss. This row is deliberately 9 yd off the near one, outside the one-field
// guarantee: overflow is somewhere to stand, not somewhere to soak.
constexpr float ULDUAR_VEZAX_RANGED_OVERFLOW_RADIUS = 33.5f;
constexpr float ULDUAR_VEZAX_RANGED_SPACING = 3.7f;
constexpr uint8 ULDUAR_VEZAX_BLOCK_ROW_SLOTS = 3;

// Slot index space, which a RaidObs trace writes as a bare number: [0,6) healers, [6,12) group L,
// [12,18) group R, [18,21) L overflow, [21,24) R overflow. The main tank is not in here - it has
// exactly one holder and comes straight off IsMainTank.
constexpr uint8 ULDUAR_VEZAX_HEALER_SLOTS = 6;
constexpr uint8 ULDUAR_VEZAX_RANGED_GROUP_SLOTS = 6;
constexpr uint8 ULDUAR_VEZAX_RANGED_OVERFLOW_SLOTS = 3;
constexpr uint8 ULDUAR_VEZAX_RANGED_SLOTS =
    2 * (ULDUAR_VEZAX_RANGED_GROUP_SLOTS + ULDUAR_VEZAX_RANGED_OVERFLOW_SLOTS);
constexpr uint8 ULDUAR_VEZAX_TOTAL_SLOTS = ULDUAR_VEZAX_HEALER_SLOTS + ULDUAR_VEZAX_RANGED_SLOTS;

// Per band, because the packing differs. The arrival deadband is twice the tolerance, so the old 2.0
// was wider than the 3.7 yd gap between ranged neighbours. Healers have 2.25 yd of room between the
// melee ring at 10.25 and the target-exclusion line at 12.5, and 0.8 is what keeps them inside both.
constexpr float ULDUAR_VEZAX_SLOT_TOLERANCE = 1.2f;
constexpr float ULDUAR_VEZAX_HEALER_SLOT_TOLERANCE = 0.8f;
constexpr float ULDUAR_VEZAX_TANK_SLOT_TOLERANCE = 3.0f;

// The hall runs 70 yd north and west of the anchor, so this stops well short of any wall. It is not
// meant to reach the entrance: outside it the movement multiplier is inert, so generic movement
// carries a bot in and the gate opens on arrival. Widening it is what would put a bot on a path
// through a wall, which is why it stays where it is.
constexpr float ULDUAR_VEZAX_ARENA_RADIUS = 45.0f;
constexpr float ULDUAR_VEZAX_ARENA_HEIGHT = 10.0f;

// Melee and the tank hold the boss rather than take slots, so all they get is a nudge apart.
constexpr float ULDUAR_VEZAX_MELEE_DECLUMP_RADIUS = 4.0f;

// Mark of the Faceless drains 5000/s from every ally within 15 yd and heals Vezax for it. Ranged step
// straight outward along their own bearing, an 18 yd walk rather than the 40-odd it takes to reach
// the far side of the room - the debuff lasts 10s and travel is the whole cost of the mechanic.
// Capped inside the arena bubble: past it the formation gate goes false, the movement multiplier
// hands the generic movers back, and the bot wanders instead of coming home.
//
// Everyone else keeps the three fixed spots behind the boss. The core only marks someone inside 15 yd
// when fewer than 9 (25m) / 4 (10m) players are further out, and the twelve ranged always clear that
// bar, so that path is the corner case rather than the common one.
constexpr float ULDUAR_VEZAX_MARK_SEPARATION = 18.0f;
constexpr float ULDUAR_VEZAX_MARK_MAX_RADIUS = 44.0f;
constexpr float ULDUAR_VEZAX_MARK_SPOT_RADIUS = 26.0f;
constexpr float ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET = 2.3208f;  // pi/2 + 0.75, clear of the blocks
constexpr float ULDUAR_VEZAX_MARK_SPOT_TOLERANCE = 3.0f;
constexpr uint8 ULDUAR_VEZAX_MARK_SPOT_COUNT = 3;

// The handler is whoever the mana actually helps. The puddle trades health on a 100 * 2^stacks curve
// for half of it back as mana, which is worth paying at 10% and not at 80% - so nobody low means
// nobody kills, and the vapor despawns on its own. Healers rank first among those who qualify.
constexpr uint8 ULDUAR_VEZAX_VAPOR_HANDLERS = 2;
constexpr uint8 ULDUAR_VEZAX_VAPOR_HANDLER_MANA_PCT = 10;
// Close to the vapor before killing it, or the puddle drops wherever the bot happened to be standing
// and reaching it costs the walk below instead of nothing.
constexpr float ULDUAR_VEZAX_VAPOR_KILL_RANGE = 5.0f;
constexpr float ULDUAR_VEZAX_VAPOR_SOAK_MAX_TRAVEL = 25.0f;

// Mimiron P3Wx2 Laser Barrage. The damage is 63293, a TARGET_UNIT_CONE_ENEMY_104 cone: 104 degrees
// wide with a 50000 yd radius, so distance from VX-001 buys nothing and only bearing matters.
constexpr float ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE = 52.0f * static_cast<float>(M_PI) / 180.0f;
// 15 rather than 12: the cone lands damage every 250 ms and turns 2.7 degrees in that time, so 12 was
// about one bot reaction tick with nothing to spare. This still leaves a 120 degree safe wedge.
constexpr float ULDUAR_MIMIRON_BARRAGE_MARGIN = 15.0f * static_cast<float>(M_PI) / 180.0f;

// Waypoint velocity on path 13395. NPC 33576 laps a 707 yd polygon that a circle of radius 113.2
// centred 2.7 yd from ULDUAR_MIMIRON_ROOM_CENTER fits, so predicting its position by rotating it
// about the room centre lands within a couple of degrees of bearing over a whole barrage. Clockwise.
constexpr float ULDUAR_MIMIRON_DB_TARGET_SPEED = 20.8988f;

// 63274 runs 10 s once 63414's single tick starts it. Only used while Spinning Up, when the barrage
// aura does not exist yet to be read; from ignition on the live duration is preferred.
constexpr float ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS = 10.0f;

// Rotate in bounded steps rather than aiming one move at the far side of the ring: creatures are not
// in the navmesh, so a long chord walks straight through VX-001, and crossing the apex crosses every
// bearing the cone covers. A 40 degree chord stays within 6% of the ring radius.
constexpr float ULDUAR_MIMIRON_BARRAGE_STEP = 40.0f * static_cast<float>(M_PI) / 180.0f;

// Added to VX-001's combat reach (8) to get the smallest ring a bot may orbit on. Melee sit inside
// that, and an orbit at their own radius runs through the model.
constexpr float ULDUAR_MIMIRON_BARRAGE_RING_MARGIN = 6.0f;

// The phase 4 main tank orbits inside the chassis's chase range instead, so the MK II stays put and
// the cone apex with it. It cannot simply hold its spot: 20000 per 250 ms tick kills it outright.
//
// 1.5 is chosen against Unit::GetMeleeRange - reach 8 plus a player's 1.5 plus 4/3, so 10.83 yd. A
// tank orbiting at 9.5 never leaves that, so the chassis never chases and the apex holds still.
// This is load-bearing for every other bot: simulation clears every bearing, radius and offset
// against a stationary apex, and only ever fails once the apex is dragged around under the raid.
constexpr float ULDUAR_MIMIRON_BARRAGE_TANK_RING_MARGIN = 1.5f;

// A bot turns around VX-001 at (7.0 yd/s / radius) against a 10.6 deg/s sweep. Holding the raid
// inside this radius makes the worst-case 52 degree rotation fit the 4 s Spinning Up warning and
// still leaves 1.6x speed margin; break-even is 38 yd, where a bot can never out-turn the cone.
constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS_MAX = 24.0f;

// Ranged ring, kept inside ULDUAR_MIMIRON_SPREAD_RADIUS_MAX so a barrage rotation from it still
// fits the Spinning Up window. The tolerance is what stops bots pacing over a yard of drift.
constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS = 22.0f;
constexpr float ULDUAR_MIMIRON_SPREAD_TOLERANCE = 5.0f;

// How far inside the bot's own spell range the outermost wedge row is allowed to sit, which is what
// caps the row count. Bots cast out to AiPlayerbot.SpellDistance, 28.5 by default, and a slot past
// that does not self-correct: "reach spell" is ACTION_HIGH and the formation is ACTION_RAID, so the
// formation wins every tick and walks the bot back out.
constexpr float ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN = 4.0f;

// Everything inside this of the room centre is walkable and flat at Z 364.31 (navprobe, 16 headings).
// Past it a boss-anchored formation hangs over the edge once the MK II has been dragged to the wall.
constexpr float ULDUAR_MIMIRON_ROOM_RADIUS = 40.0f;

// Shock Blast 63631 is TARGET_SRC_CASTER with a 15 yd radius on a 4 s cast, so 18 clears it with
// margin. Centre to centre: the flee used to be built out of GetDistance2d, which had already taken
// off the MK II's combat reach of 8 and the bot's own 1.5, and so ran everyone out to 29.5 yd.
constexpr float ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST = 18.0f;

// Phase 3 staging fan, ranged and healers only. Bomb Bots blast 5 yd, so no two bots may share one
// and 6 keeps a detonation to a single victim.
constexpr float ULDUAR_MIMIRON_PHASE3_SPACING = 6.0f;
constexpr float ULDUAR_MIMIRON_PHASE3_MIN_RADIUS = 18.0f;

// Half-width of the staging wedge. The north-east and south-east arms leave the room centre at 59
// degrees, so only a crowded outer row reaches a bearing anything walks down, and the west arm is
// excluded outright. Narrower than this and a 25-man ranged group will not fit inside casting range.
constexpr float ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE = 60.0f * static_cast<float>(M_PI) / 180.0f;

// Loot range for an Assault Bot corpse, and how close the Magnetic Core has to be used: 64444 places
// its summon by nearest entry, so the bot has to be standing under the Aerial Command Unit.
constexpr float ULDUAR_MIMIRON_CORE_LOOT_RANGE = 5.0f;
constexpr float ULDUAR_MIMIRON_CORE_USE_RANGE = 12.0f;

// How far the carrier will go looking for an Assault Bot corpse. They die wherever the raid stopped
// them, and the corpse only lasts 25 s, so the node has to start walking rather than wait for the bot
// to happen to be standing on one.
constexpr float ULDUAR_MIMIRON_CORE_SEARCH_RANGE = 60.0f;

// Phase 4 only ends when all three parts are channelling Self Repair at once, and that cast is 15 s,
// so they have to come down level rather than one at a time. Percent, not raw health: the Aerial
// Command Unit's HealthModifier is 200 against 300, so ordering on raw health ranked it last every
// tick and it never kept pace. Bots stop at 10 % and wait for the other two, because all three sit on
// the same point server-side and melee cleave splashes every one of them.
constexpr float ULDUAR_MIMIRON_PHASE4_HOLD_PCT = 10.0f;

// Where melee and tanks wait out a phase handover. Eight yards puts them inside melee range of a
// combat-reach 8 mech the moment it goes live. Staging only - see GetMimironSpreadSlot for why melee
// never get a slot during a live phase.
constexpr float ULDUAR_MIMIRON_STAGING_MELEE_RADIUS = 8.0f;

// How far a grid scan looks for a mech that is not attackable yet. The MK II parks 58 yd off centre
// between phases and a ranged bot can be another 40 out on top of that.
constexpr float ULDUAR_MIMIRON_STAGING_SEARCH_RANGE = 200.0f;

// Proximity Mines fire on anyone inside 1.9 yd and blast for 3 yd (66351). They are non-attackable,
// so the only handling is refusing to walk a bot into one. Ten land inside 15 yd after every Shock
// Blast, so a wide avoid radius leaves no clear ground at all and the raid just paces; these are
// deliberately tight, and eating the odd 3 yd blast is cheaper than losing a dodge to it.
constexpr float ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS = 3.0f;
constexpr float ULDUAR_MIMIRON_MINE_CLEARANCE = 3.5f;
constexpr float ULDUAR_MIMIRON_MINE_MAX_STEP = 5.0f;

// Rocket Strike markers burn a 5 s fuse and blast 3 yd, and the rocket picks its target from players
// beyond 15 yd - that is the ranged ring. Destinations near a live marker have to be refused for as
// long as it lives, or a bot that dodged walks straight back onto it.
constexpr float ULDUAR_MIMIRON_ROCKET_CLEARANCE = 8.0f;

// Napalm Shell splashes 5 yd around its target. Bomb Bots blast 5 yd on melee contact (63801) and
// match player run speed, so the extra yard here only buys time for ranged to kill them.
constexpr float ULDUAR_MIMIRON_NAPALM_RADIUS = 6.0f;
constexpr float ULDUAR_MIMIRON_BOMB_BOT_RADIUS = 8.0f;

// How much ground a Bomb Bot must still have to cover before a snare is worth a global. It runs
// 8.0 yd/s and dies in about three casts, so snaring one already inside this costs more than it buys.
constexpr float ULDUAR_MIMIRON_BOMB_BOT_SNARE_MIN_APPROACH = 15.0f;

// How often the raid-wide observability pass folds its answers, and the lifetime it stamps on a
// barrage hazard row. 250 matches the default Obs.SnapshotIntervalMs, so every snapshot has a cone
// row beside it, and it is also the barrage's own damage tick.
constexpr uint32 ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS = 250;

// Freya hard mode: bots step this far out of an Unstable Sun Beam before it detonates. Exact beam
// radius is DBC, not in the server script, so this is a conservative default to confirm in-game.
constexpr float ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS = 12.0f;

// Freya trio wave (Snaplasher / Storm Lasher / Ancient Water Spirit). Each member starts its own 11s
// revive timer on death and comes back unless all three are down when it expires, so they have to die
// together. The band only covers the last tenth of the wave - about 6s of raid damage out of the 60s
// the next wave takes to spawn - so it is deliberately wide.
constexpr float ULDUAR_FREYA_TRIO_SYNC_WINDOW_PCT = 30.0f;    // below this the trio outranks other adds
constexpr float ULDUAR_FREYA_TRIO_FLOOR_RELEASE_PCT = 15.0f;  // all members below: free burn to the finish
constexpr float ULDUAR_FREYA_TRIO_HARD_FLOOR_PCT = 10.0f;     // never cross while a sibling is still high

// Freya: Potent Pheromones (64321) is a 6 yd ally aura on a Healthy Spore. It is the only counter to
// Conservator's Grip, which is a 50000 yd pacify-silence and so cannot be outranged.
constexpr float ULDUAR_FREYA_SPORE_RADIUS = 6.0f;

// Spores are summoned 20 yd out from the Conservator in three directions, so this only has to cover
// that ring with room for the boss having been dragged part of the way to one.
constexpr float ULDUAR_FREYA_SPORE_SEARCH_RADIUS = 40.0f;

// Freya: Detonate (62598) radius, and it fires on death, not on a timer. Every 10s a Detonating
// Lasher wipes its own threat list and charges a random player, so no amount of threat holds one -
// see the corral constants below for what replaces tanking it.
constexpr float ULDUAR_FREYA_DETONATE_RADIUS = 15.0f;

// Detonate rolls 4162-4837 and has no difficulty entry, so it is the same in 10- and 25-man. Ranged
// focus one lasher at a time, which makes being inside two blasts at once the exception - it is not
// what sets this floor.
constexpr uint32 ULDUAR_FREYA_DETONATE_FLEE_HEALTH = 5500;

// Melee never chase a lasher; past this they stay on whatever they were already hitting. Deliberately
// tight: "nearby" has to mean the lasher came to the melee group, not that the group crosses the room.
constexpr float ULDUAR_FREYA_MELEE_LASHER_RANGE = 12.0f;

// Margin before the add tank moves between two near-equal trio members. Its own damage is what closes
// the gap, so without this it would swap every few ticks and lose swing timers to nothing.
constexpr float ULDUAR_FREYA_TANK_TRIO_SWITCH_PCT = 5.0f;

// Nature Bomb (64587) is 10 yd in both raid sizes and lands at the target's own feet, leaving ~6s to
// clear the full radius from a standing start. The extra yard covers the bot's own reach so it does
// not clip the edge of the blast while holding still.
constexpr float ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS = 11.0f;

// Where the escape aims, deliberately past the trigger radius: landing on the boundary would re-fire
// the node every tick as combat movement pulls the bot back toward its target.
constexpr float ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS = 13.0f;

// How far the escapes look for hazards to route around. Every one of Freya's comes in numbers - a
// bomb per player, ten lashers, overlapping sun beams - so stepping clear of the nearest is not
// enough; the whole cluster has to be visible or the bot walks out of one and into the next.
constexpr float ULDUAR_FREYA_HAZARD_SEARCH_RADIUS = 30.0f;

// Detonating Lasher corral. Threat cannot hold one - every 10s it wipes its threat list and charges a
// uniformly random player within 80 yd - so the pack is gathered by walking it somewhere and pinned
// there with a snare patch and a periodic root instead. 35 yd behind Freya, measured from her *home*
// orientation because she turns to face whoever tanks her. The whole 35 yd ring around her spawn
// probes on-mesh; at 50 yd the southern headings stop settling on the floor.
constexpr float ULDUAR_FREYA_LASHER_CORRAL_DISTANCE = 35.0f;
constexpr float ULDUAR_FREYA_LASHER_CORRAL_ARRIVE = 5.0f;   // close enough to count as delivered
constexpr float ULDUAR_FREYA_LASHER_CORRAL_COMMIT = 12.0f;  // inside this a hurt dragger finishes the trip
constexpr float ULDUAR_FREYA_LASHER_PACK_CLEAR = 16.0f;     // one yard past Detonate

// Frost Nova is a 10 yd sphere centred on the caster, so this is also how close the mage has to stand
// to the pack - inside Detonate range, which is why the nova node is followed out by the step-out one.
constexpr float ULDUAR_FREYA_FROST_NOVA_RADIUS = 10.0f;
constexpr uint32 ULDUAR_FREYA_LASHER_PACK_MIN_COUNT = 6;

// Frost Trap lays its patch at the hunter's feet and the patch is 10 yd, so posting one Detonate
// radius short of the corral covers the lane back to the raid while keeping the hunter out of the blast.
constexpr float ULDUAR_FREYA_LASHER_TRAP_OFFSET = 16.0f;

// Hodir.
//
// Starlight (62807) is the fight's biggest throughput lever: aura 193 runs through
// HandleModCombatSpeedPct, which applies to cast time as well as all three attack timers, for +50%.
// Its DBC row says 8, but it does not behave like 8: across two traces, bots holding the aura sit at
// a median 2.0 yd from the zone and p90 3.3, while bots without it are already at 7.6 by the tenth
// percentile. 4 is what it reaches, and that is small enough that a zone holds one bot at the 4.5 yd
// spacing icicles force - so it is a per-bot opportunity, never something to build a formation on.
//
// Toasty Fire (62821) measures true to its 11 (with-aura p90 11.9) and is the one worth standing in:
// it stops Biting Cold, which is otherwise a tenth of the raid's time spent walking. It grants no
// Flash Freeze exemption, whatever the old comment here claimed - only the Snowpacked Icicle Target
// does that, through 65705 -> 62464.
constexpr float ULDUAR_HODIR_STARLIGHT_RADIUS = 4.0f;
constexpr float ULDUAR_HODIR_TOASTY_FIRE_RADIUS = 11.0f;
constexpr float ULDUAR_HODIR_SAFE_AREA_RADIUS = 9.0f;
// The run parks at TOLERANCE and only releases at RELEASE. MoveInside lands the bot at exactly
// TOLERANCE from the centre, so testing the same number at both ends means arriving in the shelter
// releases the bot the same tick and the ring anchor walks it straight back out - measured at 18-22
// yd out with the freeze 2 s away. Both rings stay inside the 9 yd Safe Area (62464, radius index 40).
constexpr float ULDUAR_HODIR_SAFE_AREA_TOLERANCE = 6.0f;
constexpr float ULDUAR_HODIR_SAFE_AREA_RELEASE = 8.0f;
static_assert(ULDUAR_HODIR_SAFE_AREA_RELEASE < ULDUAR_HODIR_SAFE_AREA_RADIUS,
              "the release ring has to stay inside what Safe Area actually covers");
static_assert(ULDUAR_HODIR_SAFE_AREA_TOLERANCE < ULDUAR_HODIR_SAFE_AREA_RELEASE,
              "the park ring has to sit inside the release ring or arriving releases the bot");

// Storm Power lands on allies within 3 yd of the carrier, and the carrier only has 4 (10man) / 6
// (25man) one-second ticks to spend, so it tours the ring rather than searching for a cluster.
constexpr float ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS = 3.0f;

// Two different pools, two different radii, both 13000-14000 a hit. Icicle 33169 leaves Ice Shards
// 62457 in 4 yd; Snowpacked Icicle 33173 leaves Ice Shards 65370 in 7 yd. Clearing everything to 6
// stepped bots to the edge of the big one and killed four of them in one pull. Bots step past the
// edge rather than onto it, so each clear carries 2 yd of margin over its own radius.
constexpr float ULDUAR_HODIR_ICE_SHARDS_RADIUS = 4.0f;
constexpr float ULDUAR_HODIR_ICE_SHARDS_CLEAR = 6.0f;
constexpr float ULDUAR_HODIR_BIG_SHARDS_RADIUS = 7.0f;
constexpr float ULDUAR_HODIR_BIG_SHARDS_CLEAR = 9.0f;

// An icicle summon lives 7000ms (62234/62462, DurationIndex 165) but detonates at 3700ms: its AI
// casts the fall effect at 2000ms and that aura's single 1700ms tick triggers the blast. The last
// 3300ms are inert, and at one icicle every 2s roughly half of those alive have already blown.
constexpr uint32 ULDUAR_HODIR_ICICLE_SPENT_MS = 3300;

// Slots are laid out concentrically at a minimum separation of 4.5 yd, because 62457 splashes 4, so
// one icicle catches one bot instead of five. The ring is sized to fit inside a Toasty Fire - a slot
// outside one is a bot walking a Biting Cold shuttle instead of standing still and casting - but it
// only ever sits in a fire that lands inside the caster band, and this mage drops them 33 to 43 yd
// from Hodir, so in practice the ring forms on the fixed anchor and pays the shuttle.
//
// The outer ring sits 4.5 yd beyond the inner one so a bot shedding Biting Cold can step outward
// without closing on its neighbours.
constexpr float ULDUAR_HODIR_RAID_RING_INNER = 4.5f;
constexpr float ULDUAR_HODIR_RAID_RING_OUTER = 9.0f;
constexpr uint32 ULDUAR_HODIR_RAID_RING_INNER_SLOTS = 6;

// Arrival tolerance doubles as the re-anchor threshold.
constexpr float ULDUAR_HODIR_RING_SPOT_TOLERANCE = 2.0f;
constexpr float ULDUAR_HODIR_MAINTANK_SPOT_TOLERANCE = 3.0f;
// Exact at 9 + 2 = 11, so a bot sitting at the far edge of its tolerance is on the fire's boundary.
static_assert(ULDUAR_HODIR_RAID_RING_OUTER + ULDUAR_HODIR_RING_SPOT_TOLERANCE <=
                  ULDUAR_HODIR_TOASTY_FIRE_RADIUS,
              "the outer ring plus its arrival tolerance has to stay inside a Toasty Fire");
static_assert(ULDUAR_HODIR_RAID_RING_OUTER - ULDUAR_HODIR_RAID_RING_INNER > ULDUAR_HODIR_ICE_SHARDS_RADIUS,
              "the two rings have to sit more than one Ice Shards radius apart");

// Measured against Hodir himself, not the tank spot, which he leaves: he drifted 10-25 yd off it and
// a fixed-point gate let the centre land 6.8 yd from him with a 4.5 yd inner ring. The gap does not
// have to clear the whole ring - a slot that still lands close to him simply never gets walked to,
// because the position trigger checks that the slot is clear before it fires.
constexpr float ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP = 15.0f;

// The far end of the same band: how far from Hodir a caster may stand and still reach him. A Shadow
// Bolt is 30 and that is the shortest range in the raid. Both the fire and the Starlight zone are
// tested against this, because both are reasons to stand somewhere other than the ring slot, and a
// spot that cannot reach the boss is worth nothing whatever else it gives.
//
// Picking fires off the fixed anchor instead let the centre drift to a p75 of 29 yd from him and a
// max of 52, with the ring's far side 45 out and the casters walking a reach spell back in.
constexpr float ULDUAR_HODIR_CASTER_MAX_BOSS_GAP = 30.0f;
static_assert(ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP < ULDUAR_HODIR_CASTER_MAX_BOSS_GAP,
              "the caster band has to have room between its ends");

constexpr float ULDUAR_HODIR_DODGE_LEASH = 12.0f;
constexpr float ULDUAR_HODIR_DECLUMP_RADIUS = 4.5f;

// The dodge decides to leave on the radius that actually kills, and lands on the clear that carries
// margin. Testing the clear at both ends is what had bots stepping out of pools they were never in:
// the small one triggers over 2.25x the area it kills in.
constexpr float ULDUAR_HODIR_DODGE_TRIGGER_MARGIN = 0.5f;

// The dodge holds one destination rather than deriving a new one every tick. Inside ARRIVE it has got
// there and picks again; slip further than SLIP back from its closest approach and something else is
// steering the bot, so it re-issues from where the bot actually is and takes the movement slot back.
constexpr float ULDUAR_HODIR_DODGE_ARRIVE = 1.5f;
constexpr float ULDUAR_HODIR_DODGE_SLIP = 1.0f;

// How far the Starlight step looks for zones. Deliberately short of the room radius: it runs per bot
// per tick and sweeps every world object in range, and a zone beyond this is out of reach of any slot
// the bot could be on.
constexpr float ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS = 30.0f;

// Where a bot stands once it has stepped into a Starlight zone: this far from the zone centre, on the
// bearing of the slot it came from, so several bots in one zone spread around it rather than piling
// on a point. Any number may share a zone - one Ice Shards hit is 41% of a health pool (p90 54%), so
// an icicle catching two of them is two heals, and +50% to every cast and swing is worth that.
//
// Stand radius plus arrival tolerance has to stay inside what Starlight reaches: 2 + 1 = 3, against a
// measured p90 of 3.3 for bots actually holding the aura.
constexpr float ULDUAR_HODIR_STARLIGHT_STAND_RADIUS = 2.0f;
constexpr float ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE = 1.0f;
static_assert(ULDUAR_HODIR_STARLIGHT_STAND_RADIUS + ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE <=
                  ULDUAR_HODIR_STARLIGHT_RADIUS,
              "a bot at the edge of its tolerance has to still be inside Starlight");

// Where the two ends of the shed shuttle sit when the bot is standing in Starlight. Both ends and the
// straight line between them stay inside the zone, so the aura survives the shuttle that would
// otherwise walk the bot out of it. Sized off the p90 of 3.3 that bots actually holding the aura
// measure at rather than off the 4 the radius nominally reaches, so both ends keep a yard of margin.
//
// 5 yd end to end is a yard shorter than the declump leg, which is the shortest single move that
// spans two aura ticks - but the shuttle chains, alternating ends until the aura is gone, so the bot
// never stops and it is the chain rather than the leg that covers the ticks.
constexpr float ULDUAR_HODIR_STARLIGHT_SHED_RADIUS = 2.5f;
static_assert(ULDUAR_HODIR_STARLIGHT_SHED_RADIUS < ULDUAR_HODIR_STARLIGHT_RADIUS,
              "both ends of the shed shuttle have to stay inside Starlight");

// Ranged and healers hold at least this far from Hodir. His combat reach plus a raider's is roughly
// 13 yd, and Frozen Blows turns one of his swings into 20000-30000, so a caster inside this is one
// swing from dead whether or not it has aggro.
constexpr float ULDUAR_HODIR_RANGED_MIN_BOSS_GAP = 15.0f;

// How far a bot may drift from its slot before it is walked home regardless of anything else. The
// slot is not a restoring force any more, so without a hard leash a bot that stepped out for one
// dodge after another ends up out of heal range with nothing pulling it back.
constexpr float ULDUAR_HODIR_RETURN_LEASH = 20.0f;

// Biting Cold is 1s ticks that damage 200*2^stacks every tick. A stack comes off only on the second
// moving tick and any stationary tick in between resets that progress, so shedding needs sustained
// movement - a hop cannot do it. Two moving ticks means more than a second of continuous travel, so
// legs are 6 yd and chain until the aura is gone. Arming at 2 stacks costs ~33% movement duty and
// ~600/s; arming at 1 would cost half the raid's cast uptime for 200/s less.
constexpr float ULDUAR_HODIR_SHUTTLE_HALF_LEG = 3.0f;
constexpr float ULDUAR_HODIR_SHUTTLE_BEARING = -0.785398f;  // -pi/4, parallel to the SW bevel
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS = 2;

// Standing in Starlight is worth a stack: +50% to every cast and swing against 1600 a tick. Only 36
// of 3298 stack applications in a six minute kill ever reached 3, so this mostly just stops the shed
// interrupting a zone rather than actually letting stacks run.
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT = 3;

// A trapped raider dies to the next Flash Freeze 48s later, so freeing them outranks the boss - but
// the block has little health, so only the nearest few bots leave what they were doing.
constexpr float ULDUAR_HODIR_TRAPPED_ALLY_RANGE = 45.0f;
constexpr uint32 ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS = 5;

// A helper block is the same creature but a very different problem: one Flash Freeze freezes every
// helper at once - measured at 8 blocks a cycle, 11 cycles out of 11 - so a per-block cap multiplies
// by that. 8 x 5 slots over 18 eligible bots put every dps on ice, up to 11 of them on one block a
// median 21 yd from the boss. This is the ceiling across every block that is up, one breaker each.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_BREAKERS = 8;

// Never empty the ranged group for ice. Sized off who is actually there rather than off raid size,
// so eight blocks against a 10-man's three ranged still leaves someone on the boss.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE = 2;

// Frozen Blows' melee add-on lands 12645-28929 on a 45287 hp tank in 25man - a max roll is 64% of
// the pool - and two arrive about 2.4s apart. Below this, taunting into an open window is a death.
constexpr float ULDUAR_HODIR_TAUNT_HEALTH_FLOOR = 50.0f;

constexpr float ULDUAR_HODIR_ROOM_SEARCH_RADIUS = 100.0f;

// XT-002: how far a carrier actually walks. Twice the 12yd splash, because the raid does not step aside
// for it - the carrier is the only one that moves, and a bomb that lands on the edge of the radius
// still clips whoever drifted a yard the wrong way.
constexpr float ULDUAR_XT002_DEBUFF_CLEAR_RADIUS = 25.0f;

// XT-002: how far Gravity Bomb's expiry burst yanks raiders towards the carrier (63025/64233, effect
// 1). The damage half only reaches 12 yd, but being pulled into a fresh Void Zone is what kills, so
// this is the number a drop point has to beat when the carrier cannot make it to the lot.
constexpr float ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS = 20.0f;

// XT-002: Boom is roughly 10 yd, and a Boombot also detonates at 50% health, so melee leave margin
// rather than trading the hit for a few swings.
constexpr float ULDUAR_XT002_BOOMBOT_AVOID_RADIUS = 12.0f;

// XT-002 hard mode: Void Zone's Consumption pool. The damage half (64208) is 5 yd and does not grow,
// so this is a 1 yd buffer on top.
constexpr float ULDUAR_XT002_VOID_ZONE_RADIUS = 6.0f;

// XT-002: how far off its spot a bot is allowed to sit before it walks back. The tank's is loose
// enough to survive XT drifting a step. Ranged and healers each own a slot rather than the anchor
// itself, so their band only has to absorb drift - widening it instead of spreading them is what put
// thirteen bots inside 3 yd of one coordinate and handed Searing Light the whole group.
constexpr float ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE = 3.0f;
constexpr float ULDUAR_XT002_RANGED_SPOT_TOLERANCE = 5.0f;

// XT-002: the ranged/healer formation, as half-axes of an ellipse. It is wider north to south than
// east to west on purpose: east-west is the line to XT and to the tank spot, so spreading along it
// costs spell and heal range where spreading across it costs nothing. Every slot stays inside 30 yd of
// XT and 39 yd of the tank. Searing Light is 8 yd, which the outer ring beats and the inner one does
// not quite - 14 bots cannot all sit 8 yd apart and stay in range, so the aim is to cost one splash
// two or three bots instead of the whole group.
//
// The centre sits north of the ranged anchor rather than on it. The parking lots run along y = -41,
// and an ellipse centred on the anchor puts its southern slots 14 yd from the nearest cell - inside
// Gravity Bomb's 20 yd pull, so an expiring puddle would drag those bots into it. Offset, the nearest
// cell is 23 yd from the southernmost slot.
constexpr float ULDUAR_XT002_RANGED_RING_OFFSET_Y = 6.0f;
constexpr float ULDUAR_XT002_RANGED_RING_INNER_X = 6.0f;
constexpr float ULDUAR_XT002_RANGED_RING_INNER_Y = 8.0f;
constexpr float ULDUAR_XT002_RANGED_RING_OUTER_X = 9.0f;
constexpr float ULDUAR_XT002_RANGED_RING_OUTER_Y = 12.0f;
constexpr size_t ULDUAR_XT002_RANGED_RING_INNER_SLOTS = 6;

// XT-002 hard mode: Void Zone parking grid, walked +x and -y from whichever Gravity Bomb origin fits
// the carrier's role. The origin is the corner nearest the raid and the grid runs away from it, so the
// cells that only get used once the lot fills are the far ones. Step is just over the Void Zone
// diameter so consecutive drops cannot overlap.
constexpr float ULDUAR_XT002_BOMB_GRID_STEP = 6.0f;
constexpr int ULDUAR_XT002_BOMB_GRID_X_CELLS = 5;
constexpr int ULDUAR_XT002_BOMB_GRID_Y_CELLS = 4;

// XT-002 hard mode: arrival deadband for a parking cell, and the drift tolerated before the carrier
// re-issues a move. A bot that re-issues every tick slides in place and cannot cast, and Gravity Bomb
// only lasts 9s.
constexpr float ULDUAR_XT002_BOMB_CELL_ARRIVED = 2.0f;
constexpr float ULDUAR_XT002_BOMB_CELL_REENGAGE = 5.0f;

// XT-002 hard mode: taken off the debuff before working out how far a carrier can still walk. Covers
// the reaction delay and one engine tick, plus a little for the navmesh path being longer than the
// straight line the reach is measured along.
constexpr uint32 ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS = 1500;

// XT-002 hard mode: clearance a parking cell is preferred to have, so a carrier that settles at the
// edge of the arrival deadband is still outside Consumption. A preference and not a gate: as a gate
// one puddle would block five cells of twenty and the lot would run out mid-fight.
constexpr float ULDUAR_XT002_BOMB_CELL_PREFERRED_CLEARANCE =
    ULDUAR_XT002_VOID_ZONE_RADIUS + ULDUAR_XT002_BOMB_CELL_ARRIVED;

// XT-002 hard mode: clearance the walk to a parking cell keeps from puddles already down. The carrier
// action outranks "xt002 avoid hazard action", so nothing else protects a carrier on the way in.
constexpr float ULDUAR_XT002_BOMB_APPROACH_CLEARANCE = 7.5f;

// XT-002: adds spawn at toy piles 78-125yd out and walk in, and one that never paths away from its
// pile sits there for the rest of the fight. Nothing out there needs fetching - Scrapbots and Boombots
// come to XT, Pummellers chase whoever they aggro - so anything this far from the boss is left alone.
// Life Sparks are exempt: they spawn on the Searing Light carrier and chase players, not XT.
constexpr float ULDUAR_XT002_ADD_LEASH_RADIUS = 60.0f;

// XT-002: how far a bot travels for an add, measured from itself. Ranged reach the Life Spark spot
// without leaving their anchor; melee never need to move, since every add either walks to XT or chases
// a player home. Tanks are exempt - going and getting the Pummeller is the off-tank's job.
constexpr float ULDUAR_XT002_RANGED_ENGAGE_RANGE = 35.0f;
constexpr float ULDUAR_XT002_MELEE_ENGAGE_RANGE = 15.0f;

// Taunt, Growl, Dark Command and Hand of Reckoning are all 30yd.
constexpr float ULDUAR_XT002_TAUNT_RANGE = 30.0f;

// Anchored on XT rather than the carrier, so one lookup covers both parking origins wherever the
// carrier happens to be standing when the debuff lands.
constexpr float ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS = 100.0f;

// XT-002: how many ranked destinations a mover will offer the pathfinder before giving up for the
// tick. findSmoothPath refuses points in this room that are plainly walkable - it returns
// PATHFIND_NOPATH for the Searing Light spot from most of the melee stack while the mesh is intact
// underneath - and the refusal is deterministic, so a mover that re-offers its one winner never moves
// the bot at all. Which points it refuses does not follow the geometry: probing the ring below from
// the melee stack, four of the eight path normally and four do not, in no particular arrangement. Each
// attempt costs AiPlayerbot.MaxMovementSearchTime path builds, and only carriers run this loop.
constexpr size_t ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS = 5;

// XT-002: how far off the Searing Light spot its alternates sit. Clears the Void Zone radius, so a
// puddle covering the spot cannot cover the ring as well - one parked itself 2 yd from the spot and
// stayed for the last 133s of a fight, and every carrier sent there afterwards stood in Consumption.
constexpr float ULDUAR_XT002_SEARING_LIGHT_DETOUR = 10.0f;

// How much room a Searing Light destination keeps from a formation slot. The 8yd splash plus margin.
// Part of the ring always points back at the raid, and those headings are the nearest ones, so a
// carrier picked them 132 times in one pull with 4 to 6 raiders inside the splash every time.
constexpr float ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE = 12.0f;

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
// 62548 triggers its damage in a 13 yd radius, so anything under that is not actually a dodge.
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS = 15.0f;
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_PARK_DISTANCE = 3.0f;

// A Scorched Ground creature that lands this close to a water trigger never gets lit
// (boss_ignis.cpp skips SPELL_SCORCHED_GROUND within 25 yd of the water), so it stacks no Heat and
// the tank must not park a construct on it.
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS = 25.0f;

// Molten wipes the construct's threat table and adds a heavy fire aura, so everyone who is not the
// construct tank clears this much room. Sized against Shatter (62383, 18850 damage in 13 yd) rather
// than the Molten pulse itself, which only reaches ~7 yd - the blast is what actually kills a bot.
constexpr float ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS = 15.0f;

// The construct spawns run x 543..631 / y 217..338 and Ignis starts at (586.5, 378.8), so a tank
// standing at a water pool is already ~120 yd from the boss and further still from the far wall.
// Everything Ignis-side searches the grid at this radius rather than going through "nearest npcs",
// which is capped at AiPlayerbot.SightDistance (100 yd) and drops anything out of line of sight.
constexpr float ULDUAR_IGNIS_ROOM_SEARCH_RADIUS = 200.0f;

// Scorch summons its patch at boss + 20 yd along the boss's facing, and the boss faces the main
// tank, so where the tank stands is what picks the landing spot for every patch in the fight.
constexpr float ULDUAR_IGNIS_SCORCH_SPAWN_RANGE = 20.0f;

// Ignis' combat reach is 8.0, which puts melee range at roughly 10.8 yd centre to centre - the tank
// radius is that with a little slack so a step off the spot does not drop him out of range.
constexpr float ULDUAR_IGNIS_TANK_RADIUS = 9.5f;
constexpr float ULDUAR_IGNIS_TANK_SPOT_TOLERANCE = 2.0f;

// The tank works three slots 60 degrees apart, advancing one every Scorch, so the patches land in a
// fan instead of on top of each other. A 60 degree step at this radius leaves him 17.3 yd from the
// patch he just dropped and 26.8 yd from the one before - both outside the 13 yd burn.
constexpr uint8 ULDUAR_IGNIS_TANK_ARC_SLOTS = 3;
constexpr float ULDUAR_IGNIS_TANK_ARC_STEP = static_cast<float>(M_PI) / 3.0f;

// Bearing from the anchor to the tank, and therefore the direction the patch fan points: south.
// Patches south, water pools east and west, raid north - the three cannot collide.
constexpr float ULDUAR_IGNIS_TANK_BEARING = -static_cast<float>(M_PI) / 2.0f;

// Melee are admitted to a Brittle construct only once its aura has less than this left, as a
// fallback for a raid with nobody ranged in position. Brittle itself runs 15 s.
constexpr uint32 ULDUAR_IGNIS_BRITTLE_MELEE_FALLBACK_MS = 7000;

// Slag Pot ticks for ten seconds and cannot be dispelled or moved out of, but it does not kill from
// full, so healers only pile onto the victim once the ticks have actually opened a gap.
constexpr float ULDUAR_IGNIS_SLAG_POT_HEAL_HP_PCT = 85.0f;

// Algalon the Observer. His room is a 47 yd disc around the home position with a floor at Z 417.32,
// and he evades the moment he leaves it, so nothing here may pull him or a tank past the edge.
constexpr float ULDUAR_ALGALON_ROOM_RADIUS = 47.0f;
constexpr float ULDUAR_ALGALON_ROOM_SEARCH_RADIUS = 60.0f;

// Black Hole (62168) and Worm Hole (65250) both project a 6 yd field with no target cap, so a single
// hole shelters the whole raid - and standing in one outside a Big Bang costs 1531 a tick.
constexpr float ULDUAR_ALGALON_SHELTER_RADIUS = 6.0f;

// The kiter parks past the hole, on the far side from the constellation, so the chase drags the
// constellation through the field. Anything under 6 yd would park the kiter inside it instead.
constexpr float ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 9.0f;

// With no hole to spend, the kiter just walks its constellation off the raid. Arcane Barrage is
// capped at one target, so all this has to buy is that the one target keeps being the kiter.
constexpr float ULDUAR_ALGALON_KITE_CROWD_RADIUS = 20.0f;
constexpr float ULDUAR_ALGALON_KITE_LEAD_DISTANCE = 15.0f;

// Phase Punch is a 45s aura refreshed every 15.5s. Swapping at 3 leaves the off-tank arriving at its
// own third stack 46.5s after taking the boss, against a partner whose aura only fell off 45s after
// its third - one missed tick and neither tank may taunt. Four gives ~17s of margin.
constexpr uint32 ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 4;

// Big Bang repeats every 90.5s, and the window the raid needs a hole standing in before one lands.
// Dispersion is a 120s cooldown against that cadence, which is why the soak duty has to rotate.
constexpr uint32 ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS = 90500;
constexpr uint32 ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS = 30;

// Collapsing Star pacing. Each death is 16-21k unavoidable raid damage, and Collapse drains 1% of
// max health a second, so an ignored star kills itself after ~100s. The 60s summon only tops up to
// four alive, which is how four ignored stars end up exploding within seconds of each other.
constexpr float ULDUAR_ALGALON_STAR_PACING_RAID_HP_PCT = 80.0f;
constexpr uint32 ULDUAR_ALGALON_STAR_PACING_GAP_MS = 8000;
// Below this Collapse finishes the star on its own, so the pacing gate steps aside and lets the raid
// choose the moment rather than have it chosen for them.
constexpr float ULDUAR_ALGALON_STAR_FINISH_HP_PCT = 15.0f;

// Cosmic Smash: full damage inside 6 yd, dmg/dist*2 out to 10, dmg/dist beyond. The marker lands
// exactly 4s before the meteor, so the clearance has to be reached in one move.
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_MARKER_RADIUS = 11.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE = 12.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_SEARCH_RADIUS = 40.0f;

// Algalon formation. The raid arrives from the +Y side, so the tank slot sits on the -Y edge of the
// hole square and Algalon ends up facing away from everyone. Rings hang off that slot rather than
// off the room centre, because the tank is what healers have to stay in range of.
//
// Every radius and arc below is navprobe-verified on map 603: the floor is a WMO, flat at Z 417.321,
// 19/19 candidate points on mesh at 0.04 from poly. The arcs are trimmed rather than full half
// circles because the two -Y worm hole spots sit level with the tank slot, 10.2 and 16.8 yd out; the
// trims keep every slot at least 7.7 yd from all four, clear of the 6 yd field.
constexpr float ULDUAR_ALGALON_HEALER_RADIUS = 14.0f;
constexpr float ULDUAR_ALGALON_RANGED_INNER_RADIUS = 20.5f;
constexpr float ULDUAR_ALGALON_RANGED_OUTER_RADIUS = 27.0f;
constexpr uint8 ULDUAR_ALGALON_HEALER_SLOTS = 4;
constexpr uint8 ULDUAR_ALGALON_RANGED_INNER_SLOTS = 6;
constexpr uint8 ULDUAR_ALGALON_RANGED_OUTER_SLOTS = 8;
constexpr uint8 ULDUAR_ALGALON_TOTAL_SLOTS =
    ULDUAR_ALGALON_HEALER_SLOTS + ULDUAR_ALGALON_RANGED_INNER_SLOTS + ULDUAR_ALGALON_RANGED_OUTER_SLOTS;
constexpr float ULDUAR_ALGALON_HEALER_ARC_CENTER = 1.5708f;        // +Y, 35..145 degrees
constexpr float ULDUAR_ALGALON_HEALER_ARC_WIDTH = 1.9199f;
constexpr float ULDUAR_ALGALON_RANGED_INNER_ARC_CENTER = 1.7977f;  // 28..178 degrees
constexpr float ULDUAR_ALGALON_RANGED_INNER_ARC_WIDTH = 2.6180f;
constexpr float ULDUAR_ALGALON_RANGED_OUTER_ARC_CENTER = 1.5708f;  // 0..180 degrees
constexpr float ULDUAR_ALGALON_RANGED_OUTER_ARC_WIDTH = 3.1416f;
constexpr float ULDUAR_ALGALON_SLOT_TOLERANCE = 2.0f;
// Slots hold 8.9 to 12.1 yd apart, which is Cosmic Smash's cheap falloff band for the neighbours of
// whoever gets marked. A hole that lands on a slot is only stepped around, never fled from.
constexpr float ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS = 25.0f;

// Per instance, not per bot: the state tick does two sweeps and 25 bots asking every frame is the
// per-tick cost the raid-mechanics notes warn about.
constexpr uint32 ULDUAR_ALGALON_STATE_TICK_MS = 250;

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
uint32 GetXT002HeartbreakSpellId(Player* bot);

// This bot owns the Pummeller: the first assist tank, falling back to the main tank when the raid has
// no second tank left. Both the taunt and the tank's target priority read it, so they cannot disagree
// about who is holding the add.
bool IsXT002PummellerTank(PlayerbotAI* botAI, Player* bot);

// Close enough to XT to be worth engaging. Anything further out is still sitting at its toy pile.
// True when XT cannot be found, so the gate can never strand a bot with nothing to hit.
bool IsXT002AddEngageable(PlayerbotAI* botAI, Unit* unit);

// Nearest live add of `entry` inside the leash around XT and within `botReach` of the bot. Nearest
// rather than first-found: GetFirstAliveUnitByEntry lets an add stuck at a pile mask the one actually
// hitting the raid. The leash applies whatever `botReach` is passed.
Unit* GetXT002EngageableAdd(PlayerbotAI* botAI, Player* bot, uint32 entry, float botReach);

// This bot's own spot in the ranged/healer formation, laid out just north of ULDUAR_XT002_RANGED_SPOT.
// Every bot sorts the same roster the same way and reads its own index out of it, so the layout needs
// no communication and survives a death mid-fight. Healers take the centre and the inner ring because
// they are the ones who need the tank in range; ranged dps fill the outer. A slot sitting in a Void
// Zone is dealt out. False when the bot is neither ranged dps nor a healer, or the group is gone.
bool GetXT002RangedSlot(PlayerbotAI* botAI, Player* bot, Position& out);

// Whether a point keeps `clearance` from every formation slot except this bot's own.
bool XT002PointClearOfFormation(Player* bot, float x, float y, float clearance);

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

// Which station the fight is standing on: the lowest whose two spots are both clear of pools, or,
// once every station is polluted, whichever one keeps the nearest pool furthest away. Only the main
// tank reads this - everyone else picks up the move through the boss, so no two bots can disagree.
int GetAuriayaStationIndex(PlayerbotAI* botAI);

// Where this bot belongs and how far it may stray before walking back. The main tank gets the
// station's fixed spot; ranged and healers get a point derived from the live boss and tank, which is
// what keeps the split working when a human tanks. Melee and the off-tank are unanchored, and get
// false. Trigger and action both go through here so they cannot disagree.
bool GetAuriayaAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance);

// Class taunt, mirroring ICC's IccCastClassTaunt. Non-tank classes return false.
bool UldCastClassTaunt(PlayerbotAI* botAI, Unit* target);

// Hodir. By entry, not "find target": that value walks only the bot's own threat list, so any bot
// parked on an ice block would stop seeing the boss and silently lose its Flash Freeze shelter.
Unit* GetHodir(PlayerbotAI* botAI);

// Anything that pulls - targeting, the anchors - waits for this instead of mere presence. Hodir
// never calls SetInCombatWithZone, so his flag flips exactly when someone engages him.
bool IsHodirEngaged(PlayerbotAI* botAI);

// True while Hodir is casting Flash Freeze. 61968 is a 9 s cast and the trace measures cast-to-land at
// 9.03 s, so this is the whole window and nothing but it: it opens 3.8 s before the drift lands and
// leaves the shelter behind, and closes exactly when the freeze resolves.
bool IsHodirFlashFreezeIncoming(PlayerbotAI* botAI);

// True while Hodir carries Frozen Blows. Shared so the swap trigger and the taunt guard cannot
// disagree about whether the window is open. The server's spelldifficulty_dbc maps 62478 -> 63512
// for 25man, which the client DBC does not, so the difficulty lookup has to stay.
bool HodirFrozenBlowsActive(PlayerbotAI* botAI, Player* bot);

// True when taunting Hodir right now would kill this bot: Frozen Blows is up, the bot is under
// ULDUAR_HODIR_TAUNT_HEALTH_FLOOR, and somebody else is already holding him. That last clause is the
// rescue valve - with nobody on him the taunt is the save and has to survive.
bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot);

// The Snowpacked Icicle Target the whole raid shelters at during Flash Freeze.
Creature* GetHodirSharedShelter(PlayerbotAI* botAI, Player* bot);

// The Toasty Fire the ranged formation forms on, or nullptr. Picked nearest Hodir rather than nearest
// the bot, for the same reason the shelter is: two derivations of "which fire" disagree and the
// formation oscillates between them. Only a fire that leaves the whole ring inside the caster band
// qualifies, so on most pulls there is none and this returns nullptr all fight.
Creature* GetHodirRaidFire(PlayerbotAI* botAI, Player* bot);

// Where the ranged formation is centred: a Toasty Fire when one sits inside the caster band,
// otherwise ULDUAR_HODIR_RAID_ANCHOR. The fire is what the ring wants to be inside - it stops Biting
// Cold, which is the difference between standing still and walking a shuttle. Deriving it fresh on
// every call is deliberate: a cached centre shared across the raid but validated against one bot's
// own state gets rewritten by whichever bot has stepped out, and the formation thrashes.
//
// onFire says whether the centre is a fire rather than the anchor. Callers need it because the rules
// that only hold inside a fire cannot be told from the position alone, and asking again would cost a
// second grid sweep.
Position GetHodirRingCentre(PlayerbotAI* botAI, Player* bot, bool* onFire = nullptr);

// Where this bot belongs and how far it may stray. Tanks get their fixed corner spots; ranged and
// healers get a formation slot, swapped for a spot inside a Starlight zone whenever one has landed
// within reach of that slot. Melee are unanchored and get false. Trigger and action both go through
// here so they cannot disagree.
bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance);

// This bot's formation slot: centre, then an inner ring, then an outer ring, all inside the fire.
// Ranged dps are ranked ahead of healers and ties break on guid, so every bot derives the same layout
// without sharing state. The raw point is validated against the ground and the collision mesh before
// it is returned - MoveTo rejects an off-mesh destination silently.
bool GetHodirRingSlot(PlayerbotAI* botAI, Player* bot, Position const& centre, Position& out);

// The Starlight zone this bot is standing in, or false. The centre, not the bot's own spot, so the
// shuttle can be laid out around it.
bool GetHodirStarlightZoneAt(PlayerbotAI* botAI, Player* bot, Position& out);

// Where this bot moves to shed Biting Cold. Tanks alternate between two fixed points beside their
// spot so the boss cannot be walked out of the corner; a bot in a Starlight zone shuttles across the
// zone so it keeps the aura; everyone else takes the nearest point that is clear of the rest of the
// raid. Legs are long enough to cover two aura ticks.
bool GetHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out);

// False once an icicle has already detonated. It lingers another 3.3s after the blast, and treating
// that corpse as live both inflates the hazard set past what any dodge can clear and keeps bots
// walking back and forth over a spot that is already safe.
bool IsHodirIcicleLethal(Creature* icicle);

// The paladin that carries Frost Resistance Aura for this fight, or nullptr. Every point of the
// damage that kills this raid is frost, so the aura is worth a paladin's slot - but the slot is
// exclusive, so whoever holds it gives up their own. Prefers a paladin that is neither tanking nor
// healing, because retribution gives up the least; falls back to any paladin that knows the spell.
Player* GetHodirResistancePaladin(PlayerbotAI* botAI, Player* bot);

// True when this bot is one of the ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS non-healers in range of the
// block, ranked by guid. Ranking by distance instead would re-shuffle the set every tick and bots
// would flick between the block and the boss.
bool IsHodirTrappedAllyBreaker(PlayerbotAI* botAI, Player* bot, Unit* block);

// The flash-frozen helper this bot should break, or nullptr. Budgeted across every block that is up
// rather than per block, and drawn from the ranged only - a melee that leaves makes a 21 yd round trip
// for a block a ranged bot can shoot from nearer where it already stands. Asked once per bot rather
// than once per block, because the sweep it needs is not cheap and one Flash Freeze puts up eight.
Unit* GetHodirAssignedHelperBlock(PlayerbotAI* botAI, Player* bot);

// Freya. Everything the encounter needs from one grid pass, so the priority action, the tank action
// and both multipliers cannot disagree about what is up.
struct FreyaWaveState
{
    Unit* eonarsGift = nullptr;
    Unit* conservator = nullptr;
    Unit* snaplasher = nullptr;
    Unit* stormLasher = nullptr;
    Unit* waterSpirit = nullptr;
    std::vector<Unit*> detonatingLashers;

    std::vector<Unit*> LivingTrio() const;

    // Any living member below the sync window: the raid must finish this trio before it touches
    // anything else, or the members already low revive.
    bool TrioLocked() const;

    // Every living member at or below the release threshold - the last seconds, where nothing may
    // pull a bot away and no member is held back.
    bool TrioReleased() const;
};

void GatherFreyaWaveState(PlayerbotAI* botAI, FreyaWaveState& state);

// Whether damage on this trio member has to stop so the three converge. A backstop for damage the
// targeting cannot steer - a swing mid-animation, a DoT already ticking - since GetFreyaTrioAssignment
// has normally moved bots off a suppressed member already.
bool FreyaTrioSyncSuppress(FreyaWaveState const& state, Unit* target);

// Which trio member this bot should be hitting. Greedy load balance over remaining health, recomputed
// every tick: every bot walks the same group order over the same numbers and reaches the same split,
// so no shared state is needed. Suppressed members drop out of the candidate list, which is what makes
// the floor redistribute bots instead of idling them.
Unit* GetFreyaTrioAssignment(PlayerbotAI* botAI, FreyaWaveState const& state);

// What this tank should be on. The main tank always gets Freya; the add tank gets the Snaplasher first
// (Hardened Bark 62663 stacks +10% damage done per hit taken, so it needs a dedicated sink), then the
// Conservator, then a trio member, then a lasher standing next to it, then Freya.
//
// The trio pick is the highest-health non-suppressed member, and that is deliberate: tank damage is
// invisible to GetFreyaTrioAssignment, which only counts DPS, so aiming it at the member furthest from
// the floor makes the unaccounted damage help convergence instead of skewing it. currentTarget is what
// keeps that pick from flipping as the tank's own damage closes the gap.
//
// Lashers are reachable only through GetFreyaLocalLasherTarget, so a tank can damage one already on top
// of it but can never walk one back into the raid.
Unit* GetFreyaTankTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget);

// The nearest living lasher inside range, sticky on currentTarget with a switch margin. Range doubles as
// the leash - a lasher that runs past it is dropped, which stops a bot being towed across the room every
// time the add retargets. Melee and tanks pass ULDUAR_FREYA_MELEE_LASHER_RANGE; ranged pass their spell
// range, since for them "local" means anything they can shoot without moving.
Unit* GetFreyaLocalLasherTarget(PlayerbotAI* botAI, FreyaWaveState const& state, Unit* currentTarget, float range);

// The lasher every ranged bot should be on. Lowest health, GUID breaking ties: raid-wide agreement with
// no shared state, and self-stabilising, since the add being focused stays the lowest.
Unit* GetFreyaRangedLasherFocus(FreyaWaveState const& state);

// The Healthy Spore the Conservator is being parked on. Keyed off the Conservator and never off the
// calling bot, so the tank doing the dragging and the melee walking to shelter resolve the same spore
// without communicating.
Unit* GetFreyaConservatorSpore(PlayerbotAI* botAI, Unit* conservator);

// True while any bot in the group that counts as ranged DPS is alive. Eonar's Gift is a ranged job,
// but a melee-only raid still has to kill it or Freya heals 30-60%.
bool FreyaHasLivingRangedDps(PlayerbotAI* botAI);

// Every live Nature Bomb near the bot. GameObjects, not creatures: the bomb NPC is banished and never
// shows up in the npc value lists.
std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius);

// Where a Detonating Lasher wave is gathered: ULDUAR_FREYA_LASHER_CORRAL_DISTANCE behind Freya. Read
// from Creature::GetHomePosition, never the live orientation - she pivots to face whoever is tanking
// her, which would swing the corral around the room. Returns Position() before Freya is found.
Position GetFreyaLasherCorral(PlayerbotAI* botAI);

// Where the trap hunter stands: on the line from the corral back toward Freya, one Detonate radius
// short of it, so the Frost Trap patch covers the lane back to the raid from outside the blast.
Position GetFreyaLasherTrapPost(PlayerbotAI* botAI);

// Takes a point rather than a bot: the same count is wanted both around a bot (is this pile lethal)
// and around the corral (is it full enough that nobody else should be ferrying to it).
uint32 CountFreyaLashersNear(Position const& centre, FreyaWaveState const& state, float radius);

// The live lasher currently chasing this bot, or nullptr. The one bot it can actually walk somewhere.
Unit* GetFreyaLasherChasing(Player* bot, FreyaWaveState const& state);

// The one hunter that holds the trap post. Lowest GUID among living hunter bots in the group, the same
// tie-break GetFreyaRangedLasherFocus uses, so every bot agrees on it without any shared state.
bool IsFreyaLasherTrapHunter(PlayerbotAI* botAI);

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

// Alive and actually fighting. Every Ignis node hangs off this: the room is 200 yd wide and the boss
// is visible from the whole of it, so proximity alone has bots dodging and kiting on the way in.
bool IsIgnisEngaged(PlayerbotAI* botAI);

// Activated = Ignis has cast Activate Construct on it: selectable, aggressive, and worth tanking.
bool IsIgnisConstructActivated(Unit const* construct);

// 10 Heat stacks from standing in Scorched Ground. Molten also resets the construct's threat.
bool IsIgnisConstructMolten(Unit const* construct);

// A Molten construct brought to the water. One hit of 5000 (10-man) / 3000 (25-man) shatters it.
bool IsIgnisConstructBrittle(Unit const* construct);

// Lowest GUID of the Brittle constructs, not the nearest one: every bot has to converge on the same
// construct without anything shared to agree through, and one hit ends it.
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

// The lit patch nearest this tank's own pool, so the two construct tanks work opposite sides of the
// fan instead of both walking to whichever patch happens to be closest.
Unit* GetIgnisAssignedScorchedGround(PlayerbotAI* botAI, WorldObject const* from, int8 tankIndex);

// 0 or 1 for the two assist tanks that kite constructs, -1 for everyone else. Assist tanks only, on
// purpose: a raid without one skips the kiting path entirely rather than pulling the boss around
// behind a construct or feeding a DPS to a Molten one.
int8 GetIgnisConstructTankIndex(PlayerbotAI* botAI, Player* bot);

// Fixed by tank index, not by distance. The anchor sits almost exactly between the two pools, so
// nearest-pool is a coin flip that always lands the same way and would stack both Molten pulses and
// both Shatters in one spot.
Position const& GetIgnisAssignedWaterPool(int8 tankIndex);

// Where the main tank stands: an arc slot around the room-centre anchor, advanced one step on each
// Scorch. Ignis is rooted for those 3 s, so the move costs nothing and does not disturb the patch.
Position GetIgnisMainTankPosition(PlayerbotAI* botAI, Player* bot);

// The 3 s window in which Ignis is rooted and rotation-locked. The patch lands when it ends.
bool IsIgnisScorchWindow(Unit* boss);

// Flame Jets is a 2.7 s observable cast, unlike most of what this raid throws, so bots can see it
// coming and stop feeding casts into the knockback lockout that follows.
bool IsIgnisFlameJetsCasting(Unit* boss);

Player* GetIgnisSlagPotVictim(PlayerbotAI* botAI);
bool IsIgnisSlagPotVictim(Player* bot);

// Kologarn. Everything here resolves by entry rather than through "find target": that value only
// walks the bot's threatened-by-me list, so a tank parked on the body never sees the arms and the
// whole focus plan silently collapses onto the body.
Unit* GetKologarn(PlayerbotAI* botAI);
Unit* GetKologarnRightArm(PlayerbotAI* botAI);

// The entry lookups above are a proximity scan over SightDistance and answer "is Kologarn nearby",
// never "is he engaged". Every trigger that picks a target has to ask this instead, or the raid
// pulls him from 100yd the moment he comes into range.
bool KologarnEncounterActive(PlayerbotAI* botAI);
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

// True for exactly one siege engine per channel: highest energy, guid breaking ties, and then the
// channel is claimed so nobody else fires into it. Ranking alone is not enough - casting spends 20
// energy, which promotes the next engine in the same tick, and four of them emptied 80 energy into
// one channel 30 ms apart. The Ram energy reserve reads this too, so the fuel travels with the duty.
bool FlameLeviathanIsVentInterrupter(PlayerbotAI* botAI, Player* bot);

// Claims the channel now being cast at, so the ranking above stops handing it to the next engine.
// Called once the interrupt has actually gone out.
void FlameLeviathanClaimVentChannel(Player* bot);

// A cone spell lands nothing unless the vehicle is genuinely pointed at him, and CastVehicleSpell
// only turns for targets outside 120 degrees - wider than any of these cones. Returns true when the
// shot is on; otherwise starts the turn and leaves the cast for a later tick.
bool FlameLeviathanFaceForCone(Unit* vehicleBase, Unit* target, float halfAngle, float radius);

// Alive, mounted, and neither the hull nor the driver frozen. Any role handed out by election has to
// ask this, or a 60 s Hodir's Fury stun takes the role with the vehicle.
bool FlameLeviathanCrewUsable(Player* member);

// Nearest crewed vehicle held by Hodir's Fury's stun, inside the casting band of whatever will thaw
// it. `from` is excluded, so a driver never aims at the hull it is sitting in - it could not cast
// anyway. A gunner passes its turret instead, which deliberately leaves its own demolisher eligible:
// the seat is not stunned when only the hull is, and Mortar has no minimum range.
Unit* FlameLeviathanFrozenVehicle(Player* bot, Unit* from, float minRange, float maxRange);

// The vehicle currently wearing Pursued, or null. Everything Battering Ram is measured against.
Unit* FlameLeviathanPursuedVehicle(PlayerbotAI* botAI, Player* bot);

// True while this vehicle is inside the 25 yd sphere Battering Ram drops on the Pursued vehicle.
bool FlameLeviathanInBatteringRamBlast(Unit* vehicleBase, Unit* pursued);

// The whole "get out of the blast" test in one place, because the urgent-drive trigger and the drive
// action both ask it and a trigger that fires wider than its action just demotes the cast node for
// nothing. True when this vehicle shares the blast with the Pursued one and he is close enough to
// fire. No switch prediction: the rule re-aims itself the moment the Pursued guid changes.
bool FlameLeviathanShouldClearBatteringRam(PlayerbotAI* botAI, Player* bot);

// Lowest-guid chopper driver that is neither Pursued nor frozen. It runs ahead of the boss dropping
// tar in his path; a Pursued chopper is already driving away from him and drops tar for free.
bool FlameLeviathanIsTarLead(PlayerbotAI* botAI, Player* bot);

bool FlameLeviathanInArena(Position const& pos, float margin = 0.0f);

Position FlameLeviathanRearPoint(Unit* boss, float standDist, float bearingOffset = 0.0f);

// Where this vehicle sits in its class's fan, as an angle off the class bearing. One station point
// per class stacked the whole class on one spot, and a single Hodir's Fury then froze all of it.
float FlameLeviathanStationBearingOffset(Player* bot, Unit* vehicleBase, float radius);
Position FlameLeviathanLeadPoint(Unit* boss, float standDist);

// How far ahead of him the lead chopper can sit without parking inside Battering Ram's sphere.
// Zero when there is no room at all, which means the slot has to be given up for now.
float FlameLeviathanTarLeadDistance(Unit* boss, Unit* pursued, float size);

// Eight nodes hugging the arena walls, corners chamfered. Built once from ULDUAR_FL_ARENA_CORNERS.
std::vector<Position> const& FlameLeviathanKiteRing();

// Proximity Mines are non-selectable, so they never reach "possible targets" and pathing knows
// nothing about them. Movement actions check their intended destination through this first.
bool IsMimironSpotMineSafe(Player* bot, Position const& dest,
                           float clearance = ULDUAR_MIMIRON_MINE_CLEARANCE);

// Same idea for anywhere a bot is asked to stand rather than flee to: mines plus any Rocket Strike
// marker still burning its fuse. Positioning that ignores markers walks a bot that just dodged one
// straight back onto it.
bool IsMimironSpotSafe(Player* bot, Position const& dest);

// The main tank's slot in phases 1 and 4 is a boss-holding spot, not somewhere it is free to refuse:
// the MK II parks on top of the mine field it just laid, so a tank that will not stand in one never
// brings the boss back.
bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot);

// The 20 s a Magnetic Core buys. The Aerial Command Unit is on the floor, passive, and taking +50%
// damage, and its own UpdateAI is short-circuited for the whole aura so nothing new spawns for 25 s.
// It is the only stretch of phase 3 in which the boss can be killed at all.
bool IsMimironAcuGrounded(PlayerbotAI* botAI);

// The ranged snare this bot can put on a Bomb Bot, or empty for a class that has none. Roots are
// deliberately absent, but not because they break: neither Entangling Roots nor Frost Nova carries
// AURA_INTERRUPT_FLAG_TAKE_DAMAGE in 3.3.5. They are absent because a Bomb Bot has to be stopped
// while it is still approaching, and both of those land only at or around the caster.
// Trigger and action both read this, or the two disagree about who is covered.
std::string GetMimironBombBotSnare(Player* bot);

// How far a Bomb Bot still has to run before it reaches whoever it is chasing. Measured from its own
// victim rather than from the caster: a hunter 25 yd away would otherwise spend a global snaring one
// that is already two yards from a healer. Falls back to the caster's own distance before it picks
// a victim.
float GetMimironBombBotApproach(Player* bot, Unit* bombBot);

// The mech the ranged formation is shaped around. Phase order is MK II, VX-001, Aerial Command Unit,
// then all three together, and VX-001 is the one that stays parked once they reassemble.
Unit* GetMimironRingFocus(PlayerbotAI* botAI);

// The mech to form up on when none of them is attackable yet. A defeated mech keeps
// UNIT_FLAG_NOT_SELECTABLE and the next one carries it until its phase starts, so "possible targets no
// los" is blind for the whole handover - 47.75 s from phase 1 to 2, 24 s to phase 3, 31.8 s to phase 4.
// Nothing else fires either, so the engine falls through to follow at relevance 1.0 and the raid trails
// its master. A grid scan does see them, which is enough to walk everyone to the next phase in advance.
Unit* GetMimironStagingFocus(Player* bot);

// Any of the three constructs actually fighting. Presence says nothing here: Leviathan MK II is a DB
// spawn that sits in the room unselectable until the button is pushed, and GetFirstAliveUnitByEntry
// does not filter selectability.
bool IsMimironEngaged(PlayerbotAI* botAI);

// Phase 4, start to finish. Keyed on VX-001 riding the chassis rather than on all three being
// attackable: a part pushed under 15000 sets UNIT_FLAG_NON_ATTACKABLE and drops out of the target list,
// and the phase is at its most time-critical after that, not over.
bool IsMimironPhase4(Player* bot);

// What this bot should be hitting in phase 4. nullptr means hold - everything it is allowed to touch is
// already at ULDUAR_MIMIRON_PHASE4_HOLD_PCT, and pushing a part under early costs the whole rendezvous.
// `melee` is a parameter rather than derived from the bot so the pet node can ask for a melee answer on
// behalf of a hunter.
Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee);

// Who fetches the Magnetic Core. Group order so every bot computes the same answer, but melee first:
// they are already standing on the Assault Bot when it dies, whereas the plain first-bot-in-group pick
// is usually a ranged bot 22 yd out that never comes within loot range of anything.
Player* GetMimironCoreCarrier(PlayerbotAI* botAI);

// The P3Wx2 Laser Barrage cone as it will actually be, worked out live rather than latched. The beams
// follow VX-001's facing, which the core repoints at NPC 33576 on every tick of the barrage aura, so
// the bearing to that NPC is the centreline - but only from the moment the barrage lands. Spinning Up
// aims once and then holds for four seconds, during which 33576 travels another 42.6 degrees, so the
// cone ignites well clockwise of where the boss is visibly pointing.
struct MimironBarrageWindow
{
    bool valid = false;
    float lead = 0.0f;       // world bearing of the centreline at ignition, or right now if firing
    float sweep = 0.0f;      // clockwise radians still to come, below one full turn
    float rate = 0.0f;       // radians per second, 0 while still spinning up
    float untilLive = 0.0f;  // seconds until the first damage tick, 0 once firing
};

// Live every tick, so a moving apex, a rotating chassis and the bearing rate swinging 8.3-14.7 deg/s
// off centre all fall out for free, and every bot derives the same cone without coordinating.
MimironBarrageWindow GetMimironBarrageWindow(Player* bot, Unit* vx001);

// Whether `dest` is still clear of everywhere the beams will sweep, `travelSeconds` from now.
// Predictive on purpose: a MOVEMENT_FORCED leg holds the movement lock for its whole duration - up to
// 2.6 s for an 18 yd Shock Blast flee - and the cone turns about 10.6 degrees a second, so "clear when
// the move was issued" is the wrong question to ask. Measured clockwise from the ignition centreline
// and never folded to a signed angle: the band is 240 degrees wide at the room centre, wider off it.
//
// Takes the window rather than reading it, because GetMimironBarrageWindow runs a grid scan for the
// DB Target and callers test a fan of a dozen bearings against the same cast.
bool IsMimironSpotBarrageSafe(Unit* vx001, MimironBarrageWindow const& window, Position const& dest,
                              float travelSeconds);

// Where this bot stands between barrages. Ranged fan out over a full ring round the room rather than
// around VX-001, whose facing swings to whoever it last Rapid Burst; Rapid Burst and Hand Pulse are
// both 104 degree cones, so covering more bearings than one cone can hold is the only thing that
// helps. Returns false for roles this does not place. Trigger and action must both call this or the
// two disagree about where the bot belongs.
bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out);

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

// Cheap first gate for everything Thorim owns, and it takes both halves. Distance alone does not
// separate the wings: Hodir's room sits 136-176 yd from the arena centre against a corridor that runs
// out to 126 yd, and a radius through that 10 yd gap would be luck rather than a gate. Height does -
// the gauntlet is at z 412, the arena floor at 420, and Hodir's floor at 433.
constexpr float ULDUAR_THORIM_ENCOUNTER_PROXIMITY = 200.0f;
constexpr float ULDUAR_THORIM_WING_MAX_Z = 425.0f;

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

// Mimiron hard mode: bots flee a persistent fire node when this close (cells are small), and clear
// the Frost Bomb's larger explosion. Exact radii are DBC, so these are conservative defaults to
// confirm in-game.
constexpr float ULDUAR_MIMIRON_FLAMES_RADIUS = 5.0f;
constexpr float ULDUAR_MIMIRON_FROST_BOMB_RADIUS = 12.0f;
constexpr float ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

// Seeping Feral Essence's radius is DBC, so 10 yd is a conservative guess.
constexpr float ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS = 10.0f;

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
// steps as they pile up, and a station is retired once a pool lands this close to either of its two
// spots. Three is what the confirmed floor supports; a fourth would put the stack past x 1908.
constexpr int ULDUAR_AURIAYA_STATION_COUNT = 3;
constexpr float ULDUAR_AURIAYA_STATION_CLEAR_RADIUS = 12.0f;
constexpr float ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS = 100.0f;

// How far a bot may drift off its anchor to clear a pool, and how close the Feral Defender has to be
// to the boss before melee will swing at it. It re-rolls aggro constantly, so an ungated melee would
// spend the fight chasing it around the room.
constexpr float ULDUAR_AURIAYA_ESSENCE_LEASH = 12.0f;
constexpr float ULDUAR_AURIAYA_MELEE_DEFENDER_RANGE = 15.0f;
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

extern const Position ULDUAR_IGNIS_BOSS_ANCHOR;
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
// Only used when Thorim's live position cannot produce a ring point. The arena floor has a hole south
// of y = -288, so nothing here sits past the tank spot.
extern const Position ULDUAR_THORIM_PHASE2_MELEE1_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_MELEE2_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_MELEE3_SPOT;
extern const Position ULDUAR_THORIM_PHASE2_OFFTANK_SPOT;
// VX-001 fights here and the Aerial Command Unit is summoned overhead, so a ring anchored to this
// point holds still while the mechs turn and charge about.
extern const Position ULDUAR_MIMIRON_ROOM_CENTER;
// Phase 3 staging, 18 yd east of the room centre. The add summon pads sit on three arms - west,
// north-east and south-east - so the east wedge is the one stretch of floor nothing walks down.
// Grouping there funnels every Junk and Assault Bot into the melee instead of into a lone ranged bot.
// navprobe: this point and a 12 yd fan around it are 16/16 on mesh, flat at Z 364.31.
extern const Position ULDUAR_MIMIRON_PHASE3_STAGE;
extern const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT;
// Assembly of Iron room centre, read off Brundir's out-of-combat channel wander in
// boss_assembly_of_iron.cpp. navprobe: on mesh, vmap floor 427.267, and see the ring results above.
extern const Position ULDUAR_IRON_ASSEMBLY_ANCHOR;
extern const Position ULDUAR_VEZAX_ANCHOR;
// Algalon's home position, and the tank slot the formation hangs off - 18.7 yd out from home on the
// -Y edge of the hole square, 10.2 yd clear of the nearest worm hole spot.
extern const Position ULDUAR_ALGALON_ROOM_CENTER;
extern const Position ULDUAR_ALGALON_TANK_SLOT;
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

// Auriaya's lane, ULDUAR_AURIAYA_STATION_COUNT entries each. The nominal raid points are only ever
// used to retire a station - the stack's real anchor comes off the live boss.
extern const Position ULDUAR_AURIAYA_MAINTANK_SPOTS[];
extern const Position ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[];
extern const Position ULDUAR_HODIR_MAINTANK_SPOT;
extern const Position ULDUAR_HODIR_OFFTANK_SPOT;
extern const Position ULDUAR_HODIR_RAID_ANCHOR;

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

    static std::vector<HarpoonData> const& GetHarpoonData();

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
