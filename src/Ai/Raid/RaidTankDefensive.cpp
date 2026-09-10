/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidTankDefensive.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "RaidObs.h"
#include "SharedDefines.h"

#include <array>

namespace
{
// Weakest first, which here is shortest cooldown first: the strongest button stays in hand longest.
// Bubbles are deliberately absent - a paladin under Divine Shield does no damage and holds nothing. So
// is short rotational mitigation (shield block, bone shield, spell reflection), which keeps running on
// its own class logic; only the real cooldowns are sequenced. Cast names are the strings the class
// contexts already register, so one row serves both the cast and the multiplier, and the aura is
// matched by id because PlayerbotAI::HasAura compares the DBC string exactly - "anti magic shell" is
// not "Anti-Magic Shell".
struct TankDefensive
{
    uint8 playerClass;
    char const* castName;
    uint32 auraId;
};

std::array<TankDefensive, 9> const TANK_DEFENSIVES = { {
    { CLASS_WARRIOR, "last stand", 12975 },                 // 180s
    { CLASS_WARRIOR, "shield wall", 871 },                  // 300s
    { CLASS_PALADIN, "divine protection", 498 },            // 180s
    { CLASS_DRUID, "barkskin", 22812 },                     //  60s
    { CLASS_DRUID, "frenzied regeneration", 22842 },        // 180s
    { CLASS_DRUID, "survival instincts", 61336 },           // 180s
    { CLASS_DEATH_KNIGHT, "anti magic shell", 48707 },      //  45s
    { CLASS_DEATH_KNIGHT, "vampiric blood", 55233 },        //  60s
    { CLASS_DEATH_KNIGHT, "icebound fortitude", 48792 },    // 120s
} };
}  // namespace

char const* NextTankDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind)
{
    if (!botAI || !bot)
        return nullptr;

    char const* weakest = nullptr;
    bool covered = false;
    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (entry.playerClass != bot->getClass())
            continue;

        // One at a time: anything still running means the tank is already covered, and stacking the
        // next one on top spends two buttons on one window.
        if (bot->HasAura(entry.auraId))
        {
            covered = true;
            weakest = nullptr;
            break;
        }

        if (!weakest && botAI->CanCastSpell(entry.castName, bot))
            weakest = entry.castName;
    }

    if (noteKind && RaidObs::Active())
    {
        char const* picked = weakest ? weakest : (covered ? "covered" : "none");
        RaidObs::NoteDerived(bot, noteKind, picked);
    }

    return weakest;
}

bool IsHeldTankDefensive(std::string const& actionName)
{
    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (actionName == entry.castName)
            return true;
    }
    return false;
}
