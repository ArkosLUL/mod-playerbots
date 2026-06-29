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

}

#endif
