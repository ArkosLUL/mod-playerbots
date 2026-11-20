/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DKValues.h"

#include "Group.h"
#include "Playerbots.h"

// DK, Retribution and Enhancement deal a big share of magic damage, which Hysteria doesn't boost.
uint8 HysteriaTargetValue::PhysicalTier(Player* member)
{
    switch (member->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
        case CLASS_DRUID:
        case CLASS_HUNTER:
            return 0;
        default:
            return 1;
    }
}

Unit* HysteriaTargetValue::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return bot->HasAura(SPELL_HYSTERIA) ? nullptr : bot;

    Player* best = nullptr;
    uint8 bestTier = 0;
    float bestAttackPower = 0.0f;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsInWorld() ||
            member->GetMapId() != bot->GetMapId())
        {
            continue;
        }

        if (!PlayerbotAI::IsDps(member) || PlayerbotAI::IsCaster(member))
            continue;

        if (bot->GetDistance(member) > sPlayerbotAIConfig.SpellDistance || member->HasAura(SPELL_HYSTERIA) ||
            member->GetHealthPct() < MIN_HEALTH_PCT)
        {
            continue;
        }

        uint8 const tier = PhysicalTier(member);
        float const attackPower =
            member->GetTotalAttackPowerValue(member->getClass() == CLASS_HUNTER ? RANGED_ATTACK : BASE_ATTACK);
        if (!best || tier < bestTier || (tier == bestTier && attackPower > bestAttackPower))
        {
            best = member;
            bestTier = tier;
            bestAttackPower = attackPower;
        }
    }

    return best;
}
