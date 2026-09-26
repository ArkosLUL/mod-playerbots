/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BurstCooldowns.h"

#include <string>
#include <unordered_set>

#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Timer.h"
#include "Unit.h"

namespace
{
    // CastSpellAction names its Action after the spell, so the strings here are the same ones the
    // class strategies put into NextAction(...).
    std::unordered_set<std::string> const burstCooldownNames = {
        // raid-wide, racial and item
        "bloodlust", "heroism", "berserking", "blood fury", "use trinket", "use tinker",
        // warrior
        "recklessness", "death wish",
        // rogue
        "adrenaline rush", "blade flurry", "killing spree",
        // mage
        "arcane power", "icy veins", "combustion", "mirror image", "presence of mind",
        // warlock
        "metamorphosis",
        // hunter
        "rapid fire", "bestial wrath", "readiness",
        // priest
        "shadowfiend", "power infusion",
        // druid
        "berserk", "force of nature",
        // paladin
        "avenging wrath",
        // death knight
        "killing machine", "army of the dead", "summon gargoyle",
        // shaman
        "fire elemental totem", "elemental mastery",
        // consumable
        "offensive potion"};

    // Bosses that never take a victim at all, so nothing is ever "held" on them. VX-001 overrides
    // AttackStart to do nothing and its UpdateAI never calls UpdateVictim, so neither GetVictim()
    // nor the threat manager's pick is ever set: it faces whoever Rapid Burst rolled and swings at
    // nobody.
    std::unordered_set<uint32> const noVictimBosses = {33651};

    // Trial of the Crusader's Faction Champions, both rosters. Their script resets every player's
    // threat on the pull, 2 s later, then every ~9 s, weighted by distance, health and armour.
    std::unordered_set<uint32> const unstableVictimBosses = {
        // Horde
        34441, 34444, 34445, 34447, 34448, 34449, 34450, 34451, 34453, 34454, 34455, 34456, 34458, 34459,
        // Alliance
        34460, 34461, 34463, 34465, 34466, 34467, 34468, 34469, 34470, 34471, 34472, 34473, 34474, 34475};
}  // namespace

bool IsBurstCooldownAction(std::string const& actionName)
{
    return burstCooldownNames.find(actionName) != burstCooldownNames.end();
}

bool BossTakesNoVictim(Unit const* boss)
{
    return boss && noVictimBosses.find(boss->GetEntry()) != noVictimBosses.end();
}

bool BossHasNoStableVictim(Unit const* boss)
{
    return boss && unstableVictimBosses.find(boss->GetEntry()) != unstableVictimBosses.end();
}

bool IsManaReturnCooldown(Player* bot, std::string const& actionName)
{
    return bot && bot->getClass() == CLASS_PRIEST && actionName == "shadowfiend";
}

bool TankHasHeldBoss(Player* bot, Unit* boss, BurstHoldState& state, uint32 dwellMs)
{
    if (!bot || !boss)
    {
        state.Reset();
        return false;
    }

    Group* group = bot->GetGroup();
    if (!group)
    {
        state.Reset();
        return false;
    }

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (!holder || holder->GetGroup() != group || !PlayerbotAI::IsTank(holder))
    {
        state.Reset();
        return false;
    }

    uint32 now = getMSTime();
    if (state.boss != boss->GetGUID() || !state.sinceMs)
    {
        state.boss = boss->GetGUID();
        state.sinceMs = now;
    }

    return getMSTimeDiff(state.sinceMs, now) >= dwellMs;
}
