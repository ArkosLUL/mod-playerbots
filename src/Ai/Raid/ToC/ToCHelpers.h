#ifndef PLAYERBOTS_TOCHELPERS_H
#define PLAYERBOTS_TOCHELPERS_H

#include "PlayerbotAI.h"

namespace TrialOfTheCrusaderHelpers
{

enum class ToCNpcs : uint32
{
    // Northrend Beasts
    NPC_GORMOK          = 34796,
    NPC_ACIDMAW         = 35144,
    NPC_DREADSCALE      = 34799,
    NPC_ICEHOWL         = 34797,
    NPC_SNOBOLD_VASSAL  = 34800,

    // Lord Jaraxxus
    NPC_JARAXXUS        = 34780,
    NPC_MISTRESS_OF_PAIN = 34826,
    NPC_FEL_INFERNAL    = 34815,
    NPC_LEGION_FLAME    = 34784,
    NPC_NETHER_PORTAL   = 34825,
    NPC_INFERNAL_VOLCANO = 34813,

    // Anub'arak
    NPC_ANUBARAK            = 34564,
    NPC_FROST_SPHERE        = 34606, // flying sphere; becomes a grounded Permafrost patch when killed
    NPC_NERUBIAN_BURROWER   = 34607, // phase 1 add
    NPC_SWARM_SCARAB        = 34605, // submerge-phase add
    NPC_PURSUING_SPIKE      = 34660, // submerge-phase chase mob
};

enum class ToCSpells : uint32
{
    // Acidmaw & Dreadscale (worms). The bite/spray spells carry the debuff aura directly.
    SPELL_BURNING_BITE      = 66879,
    SPELL_BURNING_SPRAY     = 66902,

    // Icehowl. Massive Crash is applied to players with a difficulty-specific spell id.
    SPELL_MASSIVE_CRASH_10N = 66683,
    SPELL_MASSIVE_CRASH_25N = 67660,
    SPELL_MASSIVE_CRASH_10H = 67661,
    SPELL_MASSIVE_CRASH_25H = 67662,

    // Lord Jaraxxus
    SPELL_FEL_FIREBALL      = 66532, // interruptible cast on the current tank
    SPELL_INCINERATE_FLESH  = 66237, // heal-absorb debuff on a random player
    SPELL_LEGION_FLAME      = 66197, // spawns the pursuing ground fire

    // Nether Power: stacking spell-power buff on the boss (one id per difficulty)
    SPELL_NETHER_POWER_10N  = 66228,
    SPELL_NETHER_POWER_10H  = 67106,
    SPELL_NETHER_POWER_25N  = 67107,
    SPELL_NETHER_POWER_25H  = 67108,

    // Anub'arak
    SPELL_MARK              = 67574, // on the player the Pursuing Spike is chasing
    SPELL_PERMAFROST        = 66193, // aura on a grounded Frost Sphere; despawns a spike that reaches it
    SPELL_LEECHING_SWARM    = 66118, // raid-wide drain on the boss during phase 3 (<30%)
    SPELL_SUBMERGE_ANUB     = 65981, // boss submerge aura (phase 2)
};

// Faction Champions (Trial of the Crusader, third encounter). Both faction rosters are listed
// because players always fight the opposing faction's champions. Entries are identical across
// 10/25 and normal/heroic (difficulty is an instance property), so no difficulty variants exist.
enum class ToCFactionChampions : uint32
{
    // Healers (priority kill / crowd-control / interrupt targets)
    NPC_ALLIANCE_DRUID_RESTORATION   = 34469,
    NPC_ALLIANCE_SHAMAN_RESTORATION  = 34470,
    NPC_ALLIANCE_PALADIN_HOLY        = 34465,
    NPC_ALLIANCE_PRIEST_DISCIPLINE   = 34466,
    NPC_HORDE_DRUID_RESTORATION      = 34459,
    NPC_HORDE_SHAMAN_RESTORATION     = 34444,
    NPC_HORDE_PALADIN_HOLY           = 34445,
    NPC_HORDE_PRIEST_DISCIPLINE      = 34447,

    // Damage dealers
    NPC_ALLIANCE_DEATH_KNIGHT        = 34461,
    NPC_ALLIANCE_DRUID_BALANCE       = 34460,
    NPC_ALLIANCE_HUNTER              = 34467,
    NPC_ALLIANCE_MAGE                = 34468,
    NPC_ALLIANCE_PALADIN_RETRIBUTION = 34471,
    NPC_ALLIANCE_PRIEST_SHADOW       = 34473,
    NPC_ALLIANCE_ROGUE               = 34472,
    NPC_ALLIANCE_SHAMAN_ENHANCEMENT  = 34463,
    NPC_ALLIANCE_WARLOCK             = 34474,
    NPC_ALLIANCE_WARRIOR             = 34475,
    NPC_HORDE_DEATH_KNIGHT           = 34458,
    NPC_HORDE_DRUID_BALANCE          = 34451,
    NPC_HORDE_HUNTER                 = 34448,
    NPC_HORDE_MAGE                   = 34449,
    NPC_HORDE_PALADIN_RETRIBUTION    = 34456,
    NPC_HORDE_PRIEST_SHADOW          = 34441,
    NPC_HORDE_ROGUE                  = 34454,
    NPC_HORDE_SHAMAN_ENHANCEMENT     = 34455,
    NPC_HORDE_WARLOCK                = 34450,
    NPC_HORDE_WARRIOR                = 34453,
};

enum class ToCDisplayIds : uint32
{
    MODEL_ACIDMAW_STATIONARY    = 29815,
    MODEL_ACIDMAW_MOBILE        = 29816,
    MODEL_DREADSCALE_STATIONARY = 26935,
    MODEL_DREADSCALE_MOBILE     = 24564,
};

constexpr uint32 TRIAL_OF_THE_CRUSADER_MAP_ID = 649;

// Center of the Crusaders' Coliseum arena (trial_of_the_crusader.h Locs[LOC_CENTER])
extern const Position ARENA_CENTER;

// Center of the underground nerubian pit where Anub'arak is fought (AnubLocs[0], Z ~142).
// This is a different floor from ARENA_CENTER (the upper coliseum at Z ~393).
extern const Position ANUBARAK_PIT_CENTER;

// True while the worm is in its mobile (chasing) form rather than stationary
bool IsWormMobile(Unit* worm);

// True if the bot sits inside Icehowl's forward charge corridor (the ~12y wide lane he tramples)
bool IsBotInChargeCorridor(Player* bot, Unit* icehowl, float halfWidth);

// True if the bot has any difficulty variant of the Massive Crash aura (charge is imminent)
bool HasMassiveCrashAura(Player* bot);

// Nearest alive creature of the given entry within radius of the bot (nullptr if none)
Unit* GetNearestCreatureByEntry(Player* bot, uint32 entry, float radius);

// Average position of all alive creatures of the given entry within radius of the bot.
// Returns false (and leaves center untouched) when none are found. Used to flee away from a
// whole cluster/trail of hazards rather than a single nearest patch.
bool GetCreatureClusterCenter(Player* bot, uint32 entry, float radius, Position& center);

// True if Jaraxxus currently has any difficulty variant of the Nether Power buff
bool JaraxxusHasNetherPower(Unit* jaraxxus);

// The add that should be killed first: Mistress of Pain if up, otherwise Fel Infernal
Unit* GetPriorityJaraxxusAdd(PlayerbotAI* botAI);

// The second add to tank when both spawn types overlap (Fel Infernal while a Mistress of Pain
// is still up). Returns nullptr unless both adds are alive, so a lone add is handled by the
// priority assignment alone.
Unit* GetSecondaryJaraxxusAdd(PlayerbotAI* botAI);

// Anub'arak

// True during the submerge phase (phase 2): a Pursuing Spike or Swarm Scarab is alive, or the boss
// itself carries the submerge aura. Used to gate behaviours that only make sense while burrowed.
bool AnubarakSubmerged(PlayerbotAI* botAI);

// True during the final phase (phase 3): the boss carries the Leeching Swarm aura, or has dropped
// below 30% health. Gates the saved-up Bloodlust/Heroism burn.
bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI);

// Nearest grounded Permafrost patch (a Frost Sphere that has been destroyed and now carries the
// Permafrost aura) within radius. Returns nullptr when no patch has been seeded yet.
Unit* GetNearestPermafrost(Player* bot, float radius);

// Faction Champions

// True if the entry is any Faction Champion (either faction roster)
bool IsFactionChampion(uint32 entry);

// True if the entry is a Faction Champion healer spec (Resto Druid/Shaman, Holy Paladin, Disc Priest)
bool IsFactionChampionHealer(uint32 entry);

// True while at least one Faction Champion is alive (the encounter is in progress). Used to gate the
// shared ToC strategy's Faction Champions behaviour so it stays idle during the other three bosses.
bool FactionChampionsEncounterActive(PlayerbotAI* botAI);

// The champion to focus down: the lowest-current-health alive healer, or (once every healer is dead)
// the lowest-current-health champion of any spec. Returns nullptr when no champion is alive.
Unit* GetPriorityFactionChampion(PlayerbotAI* botAI);

// A second alive healer to crowd-control, distinct from killTarget. Returns the highest-health
// off-target healer (so it stays locked longest) or nullptr when fewer than two healers are up.
Unit* GetCcFactionChampionHealer(PlayerbotAI* botAI, Unit* killTarget);

}

#endif
