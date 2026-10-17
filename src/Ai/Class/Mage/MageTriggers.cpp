/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MageTriggers.h"
#include "DynamicObject.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "TargetValue.h"
#include "Value.h"

namespace
{
constexpr uint32 SPELL_ARCANE_POWER = 12042;
constexpr uint32 SPELL_PRESENCE_OF_MIND = 12043;
constexpr uint32 SPELL_ICY_VEINS = 12472;
constexpr uint32 SPELL_BLOODLUST = 2825;
constexpr uint32 SPELL_HEROISM = 32182;
constexpr uint32 SPELL_ARCANE_BLAST_STACK = 36032;
constexpr float ARCANE_EVOCATION_MIN_FIGHT_SECONDS = 20.0f;
}

bool NoManaGemTrigger::IsActive()
{
    static const std::vector<uint32> gemIds = {
        33312,  // Mana Sapphire
        22044,  // Mana Emerald
        8008,   // Mana Ruby
        8007,   // Mana Citrine
        5513,   // Mana Jade
        5514    // Mana Agate
    };

    for (uint32 gemId : gemIds)
    {
        if (bot->GetItemCount(gemId, false) > 0)  // false = only in bags
            return false;
    }
    return true;
}

bool ArcaneIntellectTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("arcane brilliance", GetTarget());
}

bool MageArmorTrigger::IsActive()
{
    Unit* target = GetTarget();
    return botAI->HasSpell("mage armor") && !botAI->HasAura("mage armor", target) &&
           !botAI->HasAura("ice armor", target) && !botAI->HasAura("frost armor", target) &&
           !botAI->HasAura("molten armor", target);
}

bool MoltenArmorTrigger::IsActive()
{
    Unit* target = GetTarget();
    return botAI->HasSpell("molten armor") && !botAI->HasAura("molten armor", target) &&
           !botAI->HasAura("ice armor", target) && !botAI->HasAura("frost armor", target) &&
           !botAI->HasAura("mage armor", target);
}

bool FrostNovaOnTargetTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    return botAI->HasAura(spell, target);
}

bool FrostbiteOnTargetTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    return botAI->HasAura(spell, target);
}

bool NoFocusMagicTrigger::IsActive()
{
    constexpr uint32 SPELL_FOCUS_MAGIC = 54646;
    if (!bot->HasSpell(SPELL_FOCUS_MAGIC))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        if (member->HasAura(SPELL_FOCUS_MAGIC, bot->GetGUID()))
            return false;
    }
    return true;
}

bool DeepFreezeCooldownTrigger::IsActive()
{
    constexpr uint32 SPELL_DEEP_FREEZE = 44572;
    return !bot->HasSpell(SPELL_DEEP_FREEZE) ||
           SpellCooldownTrigger::IsActive();
}

bool PresenceOfMindTrigger::IsActive()
{
    // Arcane Power stays locked while Presence of Mind is up, and only a cast-time spell spends it.
    // So it only opens an Arcane Power that's ready, and never without Arcane Blast to spend it on.
    if (bot->HasSpellCooldown(SPELL_ARCANE_POWER) || !botAI->HasSpell("arcane blast"))
        return false;

    return BoostTrigger::IsActive();
}

bool ArcaneEvocationTrigger::IsActive()
{
    if (!AI_VALUE2(bool, "has mana", "self target") ||
        AI_VALUE2(uint8, "mana", "self target") >= sPlayerbotAIConfig.MediumMana)
        return false;

    uint32 spellId = AI_VALUE2(uint32, "spell id", "evocation");
    if (!spellId || bot->HasSpellCooldown(spellId))
        return false;

    // not in the middle of a burst, the channel would eat most of it
    if (bot->HasAura(SPELL_PRESENCE_OF_MIND) || bot->HasAura(SPELL_ARCANE_POWER) || bot->HasAura(SPELL_ICY_VEINS) ||
        bot->HasAura(SPELL_BLOODLUST) || bot->HasAura(SPELL_HEROISM))
        return false;

    // a held stack goes into Arcane Missiles first (the conserve node), the channel would let it expire
    Aura const* stack = bot->GetAura(SPELL_ARCANE_BLAST_STACK);
    if (stack && stack->GetStackAmount() >= ARCANE_BLAST_CONSERVE_STACKS)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    return target->GetHealth() / AI_VALUE(float, "estimated group dps") >= ARCANE_EVOCATION_MIN_FIGHT_SECONDS;
}

const std::unordered_set<uint32> FlamestrikeNearbyTrigger::FLAMESTRIKE_SPELL_IDS = {
    2120, 2121, 8422, 8423, 10215, 10216, 27086, 42925, 42926
};

bool FlamestrikeNearbyTrigger::IsActive()
{
    for (uint32 spellId : FLAMESTRIKE_SPELL_IDS)
    {
        Aura* aura = bot->GetAura(spellId, bot->GetGUID());
        if (!aura)
            continue;

        DynamicObject* dynObj = aura->GetDynobjOwner();
        if (!dynObj)
            continue;

        float dist = bot->GetDistance2d(dynObj->GetPositionX(), dynObj->GetPositionY());
        if (dist <= radius)
            return true;
    }
    return false;
}

bool ImprovedScorchTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    static const uint32 ImprovedScorchExclusiveDebuffs[] = {// Shadow Mastery
                                                            17794, 17797, 17798, 17799, 17800,
                                                            // Winter's Chill
                                                            12579,
                                                            // Improved Scorch
                                                            22959};

    for (uint32 spellId : ImprovedScorchExclusiveDebuffs)
    {
        if (target->HasAura(spellId))
            return false;
    }

    return DebuffTrigger::IsActive();
}

bool LivingBombOnAttackersTrigger::IsActive()
{
    if (AI_VALUE(uint8, "attacker count") > MAX_SPREAD_TARGETS)
        return false;

    return DebuffOnAttackerTrigger::IsActive();
}

const std::unordered_set<uint32> BlizzardChannelCheckTrigger::BLIZZARD_SPELL_IDS = {
    10,     // Blizzard Rank 1
    6141,   // Blizzard Rank 2
    8427,   // Blizzard Rank 3
    10185,  // Blizzard Rank 4
    10186,  // Blizzard Rank 5
    10187,  // Blizzard Rank 6
    27085,  // Blizzard Rank 7
    42938,  // Blizzard Rank 8
    42939   // Blizzard Rank 9
};

bool BlizzardChannelCheckTrigger::IsActive()
{
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        spell && BLIZZARD_SPELL_IDS.count(spell->m_spellInfo->Id))
    {
        return !HasEnoughAoeTargets(botAI, bot->GetPosition(), sPlayerbotAIConfig.SightDistance, minEnemies);
    }

    return false;
}
