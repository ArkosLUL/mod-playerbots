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
#include "SpellAuras.h"

#include <array>

namespace
{
constexpr uint32 SPELL_DIVINE_PROTECTION = 498;
// Divine Shield, Divine Protection, Hand of Protection and a self Lay on Hands put it up for 2 min, and
// it blocks the other three.
constexpr uint32 SPELL_FORBEARANCE = 25771;

// Weakest first, which here is shortest cooldown first: the strongest button stays in hand longest.
// Bubbles are deliberately absent - a paladin under Divine Shield does no damage and holds nothing. So
// is short rotational mitigation (shield block, bone shield, spell reflection), which keeps running on
// its own class logic; only the real cooldowns are sequenced. Cast names are the strings the class
// contexts already register, so one row serves the hold and the trace note. The cast and the aura both
// go by id: PlayerbotAI::HasAura and the spellbook lookup behind CanCastSpell both compare the DBC
// string exactly, and "anti magic shell" is not "Anti-Magic Shell".
//
// `tank` marks the nine NextTankDefensive has always offered. The two dps-only rows at the bottom are
// out of that set, so the bosses that hold a tank's buttons do not also start holding a rogue's.
//
// `windowMs` is the buff's own duration, which decides whether a button fired now is still up when the
// hit lands. `cutPct` is what it takes off that hit; 0 means it adds health or healing instead of
// cutting damage, which is worth casting but is not something a caller can do arithmetic with.
struct TankDefensive
{
    uint8 playerClass;
    char const* castName;
    uint32 spellId;
    bool magicOnly;
    uint32 windowMs;
    uint8 cutPct;
    bool tank;
    // The buff when the cast is a dummy that triggers it: Last Stand 12975 puts up 12976.
    uint32 auraId = 0;

    uint32 AuraSpell() const { return auraId ? auraId : spellId; }
};

std::array<TankDefensive, 11> const TANK_DEFENSIVES = { {
    { CLASS_WARRIOR, "last stand", 12975, false, 20000, 0, true, 12976 },         // 180s
    { CLASS_WARRIOR, "shield wall", 871, false, 12000, 60, true },                // 300s
    { CLASS_PALADIN, "divine protection", 498, false, 12000, 50, true },          // 180s
    { CLASS_DRUID, "barkskin", 22812, false, 12000, 20, true },                   //  60s
    { CLASS_DRUID, "frenzied regeneration", 22842, false, 10000, 0, true },       // 180s
    { CLASS_DRUID, "survival instincts", 61336, false, 20000, 0, true },          // 180s
    { CLASS_DEATH_KNIGHT, "anti magic shell", 48707, true, 5000, 75, true },      //  45s
    { CLASS_DEATH_KNIGHT, "vampiric blood", 55233, false, 10000, 0, true },       //  60s
    { CLASS_DEATH_KNIGHT, "icebound fortitude", 48792, false, 12000, 30, true },  // 120s
    { CLASS_SHAMAN, "shamanistic rage", 30823, false, 15000, 30, false },         //  60s
    // Cloak of Shadows does not reduce the hit, it takes 90% off the attacker's chance to land a spell
    // at all, and area damage rolls to hit like anything else. So it is a near-total dodge of a magic
    // AoE and worth nothing against a physical one.
    { CLASS_ROGUE, "cloak of shadows", 31224, true, 5000, 90, false },            //  90s
} };

// The row the bot would cast, or the one already running on it, or nullptr. `running` says which of the
// two came back: a button still up means nothing else should go on top of it.
TankDefensive const* PickDefensive(PlayerbotAI* botAI, Player* bot, bool physicalOnly, bool tankOnly,
                                   uint32 needMs, bool& running)
{
    running = false;
    if (!botAI || !bot)
        return nullptr;

    TankDefensive const* weakest = nullptr;
    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (entry.playerClass != bot->getClass() || (physicalOnly && entry.magicOnly))
            continue;

        if (tankOnly && !entry.tank)
            continue;

        // One at a time: anything still running means the bot is already covered, and stacking the
        // next one on top spends two buttons on one window.
        if (bot->HasAura(entry.AuraSpell()))
        {
            running = true;
            return &entry;
        }

        if (entry.windowMs < needMs)
            continue;

        if (!weakest && botAI->CanCastSpell(entry.spellId, bot))
            weakest = &entry;
    }

    return weakest;
}

uint32 NextDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind, bool physicalOnly,
                     bool tankOnly, uint32 needMs)
{
    bool running = false;
    TankDefensive const* picked = PickDefensive(botAI, bot, physicalOnly, tankOnly, needMs, running);
    bool const cast = picked && !running;

    if (noteKind && RaidObs::Active())
        RaidObs::NoteDerived(bot, noteKind, cast ? picked->castName : (running ? "covered" : "none"));

    return cast ? picked->spellId : 0;
}
}  // namespace

uint32 NextTankDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind, bool physicalOnly)
{
    return NextDefensive(botAI, bot, noteKind, physicalOnly, true, 0);
}

uint32 NextMagicDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind, uint32 needMs)
{
    return NextDefensive(botAI, bot, noteKind, false, false, needMs);
}

uint32 NextPhysicalDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind, uint32 needMs)
{
    return NextDefensive(botAI, bot, noteKind, true, false, needMs);
}

uint8 BestMagicDefensiveCut(PlayerbotAI* botAI, Player* bot, uint32 needMs)
{
    bool running = false;
    TankDefensive const* picked = PickDefensive(botAI, bot, false, false, needMs, running);

    return picked ? picked->cutPct : 0;
}

char const* ReadyTankDefensive(Player* bot, bool physicalOnly, uint32 withinMs)
{
    if (!bot)
        return nullptr;

    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (!entry.tank || entry.playerClass != bot->getClass() || (physicalOnly && entry.magicOnly))
            continue;

        if (entry.spellId == SPELL_DIVINE_PROTECTION)
        {
            Aura const* forbearance = bot->GetAura(SPELL_FORBEARANCE);
            if (forbearance &&
                (forbearance->GetDuration() < 0 || static_cast<uint32>(forbearance->GetDuration()) > withinMs))
                continue;
        }

        if (bot->HasSpell(entry.spellId) && bot->GetSpellCooldownDelay(entry.spellId) <= withinMs)
            return entry.castName;
    }

    return nullptr;
}

char const* RunningTankDefensive(Player* bot, bool physicalOnly, uint32 forMs)
{
    if (!bot)
        return nullptr;

    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (!entry.tank || entry.playerClass != bot->getClass() || (physicalOnly && entry.magicOnly))
            continue;

        Aura const* aura = bot->GetAura(entry.AuraSpell());
        if (aura && (aura->GetDuration() < 0 || static_cast<uint32>(aura->GetDuration()) >= forMs))
            return entry.castName;
    }

    return nullptr;
}

bool IsHeldTankDefensive(std::string const& actionName)
{
    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (entry.tank && actionName == entry.castName)
            return true;
    }
    return false;
}

std::vector<std::string> HeldTankDefensiveNames()
{
    std::vector<std::string> names;
    for (TankDefensive const& entry : TANK_DEFENSIVES)
    {
        if (entry.tank)
            names.push_back(entry.castName);
    }

    return names;
}

std::vector<std::string> HeldMagicDefensiveNames()
{
    std::vector<std::string> names;
    for (TankDefensive const& entry : TANK_DEFENSIVES)
        names.push_back(entry.castName);

    return names;
}
