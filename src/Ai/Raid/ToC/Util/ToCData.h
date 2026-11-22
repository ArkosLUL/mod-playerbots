#ifndef PLAYERBOTS_RAID_TOCDATA_H
#define PLAYERBOTS_RAID_TOCDATA_H

#include <algorithm>

#include "Define.h"
#include "ObjectDefines.h"
#include "Position.h"

struct RaidDefensiveScope;

namespace TrialOfTheCrusaderHelpers
{

inline constexpr uint32 TRIAL_OF_THE_CRUSADER_MAP_ID = 649;

constexpr float PlayerMeleeRange(float unitReach)
{
    return std::max(unitReach + DEFAULT_COMBAT_REACH + 4.0f / 3.0f, NOMINAL_MELEE_RANGE);
}

extern RaidDefensiveScope const TOC_DEFENSIVE_SCOPE;

extern const Position ARENA_CENTER;

enum class ToCNpcs : uint32
{
    NPC_GORMOK          = 34796,
    NPC_ACIDMAW         = 35144,
    NPC_DREADSCALE      = 34799,
    NPC_ICEHOWL         = 34797,
    NPC_SNOBOLD_VASSAL  = 34800,
    NPC_SLIME_POOL      = 35176,

    NPC_JARAXXUS        = 34780,
    NPC_MISTRESS_OF_PAIN = 34826,
    NPC_FEL_INFERNAL    = 34815,
    NPC_LEGION_FLAME    = 34784,
    NPC_NETHER_PORTAL   = 34825,
    NPC_INFERNAL_VOLCANO = 34813,

    NPC_ANUBARAK            = 34564,
    NPC_FROST_SPHERE        = 34606,
    NPC_NERUBIAN_BURROWER   = 34607,
    NPC_SWARM_SCARAB        = 34605,
    NPC_PURSUING_SPIKE      = 34660,

    NPC_FJOLA_LIGHTBANE     = 34497,
    NPC_EYDIS_DARKBANE      = 34496,
    NPC_LIGHT_ESSENCE       = 34568,
    NPC_DARK_ESSENCE        = 34567,
};

inline constexpr uint32 SPELL_IMPALE            = 66331;

inline constexpr uint32 SPELL_SWEEP             = 66794;

inline constexpr uint32 SPELL_FEL_FIREBALL      = 66532;
inline constexpr uint32 SPELL_INCINERATE_FLESH  = 66237;
inline constexpr uint32 SPELL_LEGION_FLAME      = 66197;

inline constexpr uint32 SPELL_NETHER_POWER_10N  = 66228;
inline constexpr uint32 SPELL_NETHER_POWER_25N  = 67106;
inline constexpr uint32 SPELL_NETHER_POWER_10H  = 67107;
inline constexpr uint32 SPELL_NETHER_POWER_25H  = 67108;

inline constexpr uint32 SPELL_MARK              = 67574;
inline constexpr uint32 SPELL_LEECHING_SWARM    = 66118;
inline constexpr uint32 SPELL_SUBMERGE_ANUB     = 65981;

inline constexpr uint32 SPELL_LIGHT_ESSENCE     = 65686;
inline constexpr uint32 SPELL_DARK_ESSENCE      = 65684;
inline constexpr uint32 SPELL_LIGHT_VORTEX      = 66046;
inline constexpr uint32 SPELL_DARK_VORTEX       = 66058;
inline constexpr uint32 SPELL_LIGHT_TOUCH       = 65950;
inline constexpr uint32 SPELL_DARK_TOUCH        = 66001;
inline constexpr uint32 SPELL_LIGHT_TWIN_PACT   = 65876;
inline constexpr uint32 SPELL_DARK_TWIN_PACT    = 65875;

}

#endif
