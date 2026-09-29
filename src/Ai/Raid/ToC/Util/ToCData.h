#ifndef PLAYERBOTS_RAID_TOCDATA_H
#define PLAYERBOTS_RAID_TOCDATA_H

#include "Define.h"
#include "Position.h"

// Raid-wide values only. Boss-specific ids, constants and positions go in that boss's
// Util/ToCHelpers_<Stem>.h.
namespace TrialOfTheCrusaderHelpers
{

constexpr uint32 TRIAL_OF_THE_CRUSADER_MAP_ID = 649;

// Center of the Crusaders' Coliseum arena (trial_of_the_crusader.h Locs[LOC_CENTER])
extern const Position ARENA_CENTER;

// Center of the underground nerubian pit where Anub'arak is fought (boss_anubarak_trial.cpp
// AnubLocs[0], Z ~142). This is a different floor from ARENA_CENTER (the upper coliseum at Z ~393).
extern const Position ANUBARAK_PIT_CENTER;

// Boss entries from trial_of_the_crusader.h NPCs, adds and hazards from the NPC enum of the boss
// script named on each section.
enum class ToCNpcs : uint32
{
    // Northrend Beasts (boss_northrend_beasts.cpp GormokNPCs and JormungarNPCs)
    NPC_GORMOK          = 34796,
    NPC_ACIDMAW         = 35144,
    NPC_DREADSCALE      = 34799,
    NPC_ICEHOWL         = 34797,
    NPC_SNOBOLD_VASSAL  = 34800,
    NPC_SLIME_POOL      = 35176, // Acidmaw/Dreadscale slime pool: a persistent ground hazard

    // Lord Jaraxxus (boss_lord_jaraxxus.cpp JaraxxusNPCs)
    NPC_JARAXXUS        = 34780,
    NPC_MISTRESS_OF_PAIN = 34826,
    NPC_FEL_INFERNAL    = 34815,
    NPC_LEGION_FLAME    = 34784,
    NPC_NETHER_PORTAL   = 34825,
    NPC_INFERNAL_VOLCANO = 34813,

    // Anub'arak (boss_anubarak_trial.cpp AnubNPCs)
    NPC_ANUBARAK            = 34564,
    NPC_FROST_SPHERE        = 34606, // flying sphere; becomes a grounded Permafrost patch when killed
    NPC_NERUBIAN_BURROWER   = 34607, // phase 1 add
    NPC_SWARM_SCARAB        = 34605, // submerge-phase add
    NPC_PURSUING_SPIKE      = 34660, // submerge-phase chase mob

    // Twin Val'kyr (boss_twin_valkyr.cpp ValkyrNPCs)
    NPC_FJOLA_LIGHTBANE     = 34497, // the Light twin (controls the encounter, casts Light Vortex)
    NPC_EYDIS_DARKBANE      = 34496, // the Dark twin (casts Dark Vortex)
    NPC_LIGHT_ESSENCE       = 34568, // portal NPC; its gossip grants the Light Essence aura
    NPC_DARK_ESSENCE        = 34567, // portal NPC; its gossip grants the Dark Essence aura
};

// From the spell enum of the boss script named on each section, as the 10N id. Every cast carries
// the map difficulty's id, so read a remapped one through sSpellMgr->GetSpellIdForDifficulty.

// Northrend Beasts - Gormok the Impaler (boss_northrend_beasts.cpp GormokSpells)
constexpr uint32 SPELL_IMPALE            = 66331; // stacking bleed on the current tank, drives the tank swap

// Acidmaw & Dreadscale (boss_northrend_beasts.cpp JormungarSpells)
// 15 yd knockback circle around the worm (DBC radius index 18)
constexpr uint32 SPELL_SWEEP             = 66794;

// Icehowl (boss_northrend_beasts.cpp IcehowlSpells)
constexpr uint32 SPELL_MASSIVE_CRASH_10N = 66683;
constexpr uint32 SPELL_MASSIVE_CRASH_25N = 67660;
constexpr uint32 SPELL_MASSIVE_CRASH_10H = 67661;
constexpr uint32 SPELL_MASSIVE_CRASH_25H = 67662;

// Lord Jaraxxus (boss_lord_jaraxxus.cpp JaraxxusSpells)
constexpr uint32 SPELL_FEL_FIREBALL      = 66532; // interruptible cast on the current tank
constexpr uint32 SPELL_INCINERATE_FLESH  = 66237; // heal-absorb debuff on a random player
constexpr uint32 SPELL_LEGION_FLAME      = 66197; // spawns the pursuing ground fire

// Nether Power: stacking spell-power buff on the boss
constexpr uint32 SPELL_NETHER_POWER_10N  = 66228;
constexpr uint32 SPELL_NETHER_POWER_25N  = 67106;
constexpr uint32 SPELL_NETHER_POWER_10H  = 67107;
constexpr uint32 SPELL_NETHER_POWER_25H  = 67108;

// Anub'arak (boss_anubarak_trial.cpp AnubSpells)
constexpr uint32 SPELL_MARK              = 67574; // on the player the Pursuing Spike is chasing
constexpr uint32 SPELL_LEECHING_SWARM    = 66118; // P3 area aura, carried by the players and never by the boss
constexpr uint32 SPELL_SUBMERGE_ANUB     = 65981; // boss submerge aura (P2)

// Twin Val'kyr (boss_twin_valkyr.cpp ValkyrSpells). Essences are player auras from the portal
// NPCs' gossip.
constexpr uint32 SPELL_LIGHT_ESSENCE     = 65686; // matches Light Vortex / Light Touch
constexpr uint32 SPELL_DARK_ESSENCE      = 65684; // matches Dark Vortex / Dark Touch
// Vortex casts, read from the casting twin's current spell
constexpr uint32 SPELL_LIGHT_VORTEX      = 66046;
constexpr uint32 SPELL_DARK_VORTEX       = 66058;
// Touch DoTs, heroic only. The script casts the 10H ids 67297/67282, same difficulty rows.
constexpr uint32 SPELL_LIGHT_TOUCH       = 65950;
constexpr uint32 SPELL_DARK_TOUCH        = 66001;
// Twin's Pact heal-to-full 15 s cast (Fjola casts Light, Eydis Dark)
constexpr uint32 SPELL_LIGHT_TWIN_PACT   = 65876;
constexpr uint32 SPELL_DARK_TWIN_PACT    = 65875;

}

#endif
