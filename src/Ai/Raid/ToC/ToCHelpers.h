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

}

#endif
