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
        "bloodlust", "heroism", "berserking", "blood fury", "use trinket",
        // warrior
        "recklessness", "death wish",
        // rogue
        "adrenaline rush", "blade flurry",
        // mage
        "arcane power", "icy veins", "combustion", "mirror image", "presence of mind",
        // warlock
        "metamorphosis",
        // hunter
        "rapid fire", "bestial wrath", "readiness",
        // priest
        "shadowfiend",
        // druid
        "berserk", "force of nature",
        // paladin
        "avenging wrath",
        // death knight
        "killing machine", "army of the dead", "summon gargoyle",
        // shaman
        "fire elemental totem", "elemental mastery"};
}  // namespace

bool IsBurstCooldownAction(std::string const& actionName)
{
    return burstCooldownNames.find(actionName) != burstCooldownNames.end();
}

bool MainTankHasHeldBoss(Player* bot, Unit* boss, BurstHoldState& state, uint32 dwellMs)
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

    ObjectGuid mainTankGuid = PlayerbotAI::GetMainTankGuid(group);
    Unit* victim = boss->GetVictim();
    if (!mainTankGuid || !victim || victim->GetGUID() != mainTankGuid)
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
