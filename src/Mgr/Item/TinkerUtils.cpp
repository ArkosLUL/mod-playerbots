/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TinkerUtils.h"

#include <algorithm>

#include "DBCStores.h"
#include "Item.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace ai::tinker
{

bool IsUsableTinkerSpell(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !spellInfo->IsPositive())
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        SpellEffectInfo const& effectInfo = spellInfo->Effects[i];
        if (effectInfo.Effect != SPELL_EFFECT_APPLY_AURA)
            continue;

        // Kept in step with StatsCollector::HandleApplyAura - an aura it cannot turn into a stat
        // is worth neither scoring nor a global cooldown.
        switch (effectInfo.ApplyAuraName)
        {
            case SPELL_AURA_MOD_DAMAGE_DONE:
            case SPELL_AURA_MOD_HEALING_DONE:
            case SPELL_AURA_MOD_INCREASE_HEALTH:
            case SPELL_AURA_SCHOOL_ABSORB:
            case SPELL_AURA_MOD_ATTACK_POWER:
            case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
            case SPELL_AURA_MOD_SHIELD_BLOCKVALUE:
            case SPELL_AURA_MOD_STAT:
            case SPELL_AURA_MOD_RESISTANCE:
            case SPELL_AURA_MOD_RATING:
                return true;
            default:
                break;
        }
    }

    return false;
}

uint32 GetTinkerCooldownMs(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return TINKER_FALLBACK_COOLDOWN_MS;

    uint32 cooldown = std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime);
    return cooldown ? cooldown : TINKER_FALLBACK_COOLDOWN_MS;
}

uint32 GetUsableTinkerSpell(Item const* item)
{
    if (!item)
        return 0;

    for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
    {
        SpellItemEnchantmentEntry const* enchant =
            sSpellItemEnchantmentStore.LookupEntry(item->GetEnchantmentId(EnchantmentSlot(slot)));
        if (!enchant)
            continue;

        for (uint8 s = 0; s < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            if (enchant->type[s] != ITEM_ENCHANTMENT_TYPE_USE_SPELL)
                continue;

            if (IsUsableTinkerSpell(enchant->spellid[s]))
                return enchant->spellid[s];
        }
    }

    return 0;
}

}  // namespace ai::tinker
