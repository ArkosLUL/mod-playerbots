/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PriestActions.h"
#include "Event.h"
#include "Playerbots.h"

namespace
{
// Weakened Soul locks a target out for 15 s and a shield absorbs damage without raising health %, so
// the raider we just shielded stays the lowest-health one and keeps owning "party member to heal".
// Picking the next shieldable body instead is what makes the shield roll across a raid.
Unit* FindShieldTarget(PlayerbotAI* botAI, Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    float const range = botAI->GetRange("heal");
    MinValueCalculator calc(100);

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player || player->isDead() || player->IsGameMaster() || player->IsFullHealth())
            continue;

        if (player->GetMapId() != bot->GetMapId())
            continue;

        if (player->GetDistance2d(bot) > range || !bot->IsWithinLOSInMap(player))
            continue;

        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
            continue;

        calc.probe(player->GetHealthPct(), player);
    }

    return (Unit*)calc.param;
}
}

bool CastRemoveShadowformAction::Execute(Event /*event*/)
{
    botAI->RemoveAura("shadowform");
    return true;
}

bool CastRemoveShadowformAction::isUseful() { return botAI->HasAura("shadowform", AI_VALUE(Unit*, "self target")); }

bool CastPowerWordShieldAction::isUseful()
{
    return CastBuffSpellAction::isUseful() && !botAI->HasAura("weakened soul", GetTarget());
}

Unit* CastPowerWordShieldOnPartyAction::GetTarget()
{
    Unit* target = HealPartyMemberAction::GetTarget();
    if (target && !botAI->HasAnyAuraOf(target, "weakened soul", "power word: shield", nullptr))
        return target;

    return FindShieldTarget(botAI, bot);
}

bool CastPowerWordShieldOnPartyAction::isUseful()
{
    return HealPartyMemberAction::isUseful() && !botAI->HasAura("weakened soul", GetTarget());
}

bool CastPowerWordShieldOnMainTankAction::isUseful()
{
    return BuffOnMainTankAction::isUseful() && !botAI->HasAura("weakened soul", GetTarget());
}

Unit* CastShadowfiendAction::GetTarget()
{
    if (Unit* target = AI_VALUE(Unit*, "current target"))
        return target;

    return AI_VALUE(Unit*, "grind target");
}

Unit* CastPowerWordShieldOnNotFullAction::GetTarget() { return FindShieldTarget(botAI, bot); }

bool CastPowerWordShieldOnNotFullAction::isUseful()
{
    return GetTarget();
}
