/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "StatsWeightCalculator.h"
#include "AiFactory.h"
#include "BisListMgr.h"
#include "DBCStores.h"
#include "ItemEnchantmentMgr.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "ProgressionMgr.h"
#include "RandomItemMgr.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "SpellMgr.h"
#include "StatsCollector.h"
#include "Unit.h"

#include <algorithm>
#include <memory>

namespace
{
constexpr uint32 SPELL_MOLTEN_ARMOR_RANKS[] = { 30482, 43045, 43046 };
constexpr uint32 SPELL_FEL_ARMOR_RANKS[] = { 28176, 28189, 47892, 47893 };
constexpr uint32 SPELL_CAREFUL_AIM = 34484;
constexpr uint32 SPELL_HUNTER_VS_WILD = 56341;
constexpr uint32 SPELL_ARMORED_TO_THE_TEETH = 61222;
constexpr uint32 SPELL_MENTAL_DEXTERITY = 51885;
constexpr uint32 SPELL_ROGUE_SWORD_SPECIALIZATION = 13964;
constexpr uint32 SPELL_POLEAXE_SPECIALIZATION = 12785;
constexpr uint32 SPELL_NERVES_OF_COLD_STEEL = 50138;
constexpr uint32 SPELL_SHADOW_FOCUS = 15835;
constexpr uint32 SPELL_ARCANE_FOCUS = 12840;

// Primary nuke school per caster spec, so single-school spell power items
// (e.g. +51 fire damage) score for the specs they actually benefit
uint32 SpecPrimarySpellSchoolMask(uint8 cls, int tab)
{
    switch (cls)
    {
        case CLASS_MAGE:
            if (tab == MAGE_TAB_ARCANE)
                return SPELL_SCHOOL_MASK_ARCANE;
            if (tab == MAGE_TAB_FIRE)
                return SPELL_SCHOOL_MASK_FIRE;
            if (tab == MAGE_TAB_FROST)
                return SPELL_SCHOOL_MASK_FROST;
            break;
        case CLASS_WARLOCK:
            if (tab == WARLOCK_TAB_DESTRUCTION)
                return SPELL_SCHOOL_MASK_FIRE;
            return SPELL_SCHOOL_MASK_SHADOW;
        case CLASS_PRIEST:
            if (tab == PRIEST_TAB_SHADOW)
                return SPELL_SCHOOL_MASK_SHADOW;
            break;
        case CLASS_DRUID:
            if (tab == DRUID_TAB_BALANCE)
                return SPELL_SCHOOL_MASK_NATURE;
            break;
        case CLASS_SHAMAN:
            if (tab == SHAMAN_TAB_ELEMENTAL)
                return SPELL_SCHOOL_MASK_NATURE;
            break;
        default:
            break;
    }
    return 0;
}

// ItemSetEntry pairs spells[j] with the piece count that triggers it. Several spells can share one
// threshold (that is still a single set bonus to the player), so count distinct thresholds only.
uint32 ActiveSetBonuses(ItemSetEntry const* set, uint32 pieces)
{
    uint32 seen[MAX_ITEM_SET_SPELLS];
    uint32 active = 0;
    for (size_t j = 0; j < MAX_ITEM_SET_SPELLS; ++j)
    {
        uint32 threshold = set->items_to_triggerspell[j];
        if (!threshold || !set->spells[j] || pieces < threshold)
            continue;

        if (std::find(seen, seen + active, threshold) != seen + active)
            continue;

        seen[active++] = threshold;
    }
    return active;
}

// Smallest piece count above `pieces` that triggers another bonus, 0 if the set is maxed out.
uint32 NextSetThreshold(ItemSetEntry const* set, uint32 pieces)
{
    uint32 next = 0;
    for (size_t j = 0; j < MAX_ITEM_SET_SPELLS; ++j)
    {
        uint32 threshold = set->items_to_triggerspell[j];
        if (!threshold || !set->spells[j] || threshold <= pieces)
            continue;

        if (!next || threshold < next)
            next = threshold;
    }
    return next;
}

uint32 EquippedSetPieces(Player* player, uint32 setId)
{
    for (ItemSetEffect const* eff : player->ItemSetEff)
    {
        if (eff && eff->setid == setId)
            return eff->item_count;
    }
    return 0;
}
}

template <size_t Size>
bool HasAnySpell(Player* player, uint32 const (&spellIds)[Size])
{
    for (uint32 const spellId : spellIds)
    {
        if (player->HasSpell(spellId))
            return true;
    }

    return false;
}

StatsWeightCalculator::StatsWeightCalculator(Player* player) : player_(player)
{
    if (PlayerbotAI::IsHeal(player))
        type_ = CollectorType::SPELL_HEAL;
    else if (PlayerbotAI::IsCaster(player))
        type_ = CollectorType::SPELL_DMG;
    else if (PlayerbotAI::IsTank(player))
        type_ = CollectorType::MELEE_TANK;
    else if (PlayerbotAI::IsMelee(player))
        type_ = CollectorType::MELEE_DMG;
    else
        type_ = CollectorType::RANGED;
    cls = player->getClass();
    lvl = player->GetLevel();
    tab = AiFactory::GetPlayerSpecTab(player);
    collector_ = std::make_unique<StatsCollector>(type_, cls, SpecPrimarySpellSchoolMask(cls, tab));

    if (cls == CLASS_DEATH_KNIGHT && tab == DEATH_KNIGHT_TAB_UNHOLY)
        hitOverflowType_ = CollectorType::SPELL;
    else if (cls == CLASS_SHAMAN && tab == SHAMAN_TAB_ENHANCEMENT)
        hitOverflowType_ = CollectorType::SPELL;
    else if (cls == CLASS_ROGUE)
        hitOverflowType_ = CollectorType::SPELL;
    else
        hitOverflowType_ = type_;

    enable_overflow_penalty_ = true;
    enable_item_set_bonus_ = true;
    enable_quality_blend_ = true;
}

void StatsWeightCalculator::Reset()
{
    collector_->Reset();
    weight_ = 0;
    for (uint32 i = 0; i < STATS_TYPE_MAX; i++)
    {
        stats_weights_[i] = 0;
    }
}

float StatsWeightCalculator::CalculateItem(uint32 itemId, int32 randomPropertyIds, int32 slot)
{
    ItemTemplate const* proto = &sObjectMgr->GetItemTemplateStore()->at(itemId);

    if (!proto)
        return 0.0f;

    Reset();

    collector_->CollectItemStats(proto);

    if (randomPropertyIds != 0)
        CalculateRandomProperty(randomPropertyIds, itemId);

    if (enable_overflow_penalty_)
        ApplyOverflowPenalty(player_);

    GenerateWeights(player_);
    for (uint32 i = 0; i < STATS_TYPE_MAX; i++)
    {
        weight_ += stats_weights_[i] * collector_->stats[i];
    }

    // Socket value is expressed relative to the item's own stats, so it needs the plain stat sum,
    // before the type penalty and set multiplier scale weight_ away from that space.
    float statSumWeight = weight_;

    CalculateItemTypePenalty(proto);

    if (enable_item_set_bonus_)
        CalculateItemSetMod(player_, proto);

    CalculateSocketBonus(proto, statSumWeight);

    if (enable_quality_blend_)
    {
        // Heirloom items scale with player level
        // Use player level as effective item level for heirlooms - Quality EPIC
        // Else - Blend with item quality and level for normal items
        if (proto->Quality == ITEM_QUALITY_HEIRLOOM)
            weight_ *= PlayerbotFactory::CalcMixedGearScore(lvl, ITEM_QUALITY_EPIC);
        else
            weight_ *= PlayerbotFactory::CalcMixedGearScore(proto->ItemLevel, proto->Quality);
    }

    // Apply weapon speed governance if slot is provided and this is a weapon
    if (sPlayerbotAIConfig.preferredSpecWeapons && slot >= 0 && proto->Class == ITEM_CLASS_WEAPON)
        weight_ *= ApplyPreferredSpecWeapons(proto, slot);

    if (enable_bis_bonus_)
        weight_ *= BisRankMultiplier(proto);

    return weight_;
}

float StatsWeightCalculator::BisRankMultiplier(ItemTemplate const* proto)
{
    if (sPlayerbotAIConfig.bisScoreBonus <= 0.0f)
        return 1.0f;

    if (!bis_key_resolved_)
    {
        bis_key_resolved_ = true;
        bis_key_valid_ = BisListMgr::ResolveSpecKey(player_, bis_cls_, bis_tab_);
        if (bis_key_valid_)
            bis_progress_ = BisListMgr::ProgressForBot(player_);
    }

    if (!bis_key_valid_)
        return 1.0f;

    // Phase-limited, unlike the spec gates: a pre-raid BiS piece should stop pulling once the bot has
    // progressed past it. Only this bot's own expansion is consulted.
    uint8 entryPhase = 0;
    uint8 const rank = sBisListMgr->GetBisRankFor(proto->ItemId, bis_cls_, bis_tab_, bis_progress_, &entryPhase);

    // Ranks 4-6 are filler alternates, and against EquipUpgradeThreshold (1.1) a ~2.5% nudge is
    // invisible anyway. They still get the spec-gate pass, just no score change.
    if (!rank || rank > 3)
        return 1.0f;

    // Without this a leftover pre-raid rank-1 piece is worth exactly as much as the current tier's
    // rank-1, so the two bonuses cancel and the equipped lower-ilvl piece keeps the slot on the
    // upgrade threshold alone.
    float phaseScale = 1.0f;
    if (sPlayerbotAIConfig.bisPhaseDecay > 0.0f && entryPhase < bis_progress_.phase)
    {
        uint8 const behind = bis_progress_.phase - entryPhase;
        phaseScale = std::max(0.0f, 1.0f - sPlayerbotAIConfig.bisPhaseDecay * behind);
    }

    // Flat-ish, because ranks 1-3 of a slot's list are near-equivalent picks - the old 1/3 for rank 3
    // left it below the upgrade threshold's own noise.
    static constexpr float kRankScale[3] = {1.0f, 0.8f, 0.6f};
    return 1.0f + sPlayerbotAIConfig.bisScoreBonus * kRankScale[rank - 1] * phaseScale;
}

float StatsWeightCalculator::CalculateEnchant(uint32 enchantId)
{
    SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);

    if (!enchant)
        return 0.0f;

    Reset();

    collector_->CollectEnchantStats(enchant);

    if (enable_overflow_penalty_)
        ApplyOverflowPenalty(player_);

    GenerateWeights(player_);
    for (uint32 i = 0; i < STATS_TYPE_MAX; i++)
    {
        weight_ += stats_weights_[i] * collector_->stats[i];
    }

    return weight_;
}

void StatsWeightCalculator::CalculateRandomProperty(int32 randomPropertyId, uint32 itemId)
{
    if (randomPropertyId > 0)
    {
        ItemRandomPropertiesEntry const* item_rand = sItemRandomPropertiesStore.LookupEntry(randomPropertyId);
        if (!item_rand)
        {
            return;
        }

        for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
        {
            uint32 enchantId = item_rand->Enchantment[i - PROP_ENCHANTMENT_SLOT_0];
            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (enchant)
                collector_->CollectEnchantStats(enchant);
        }
    }
    else
    {
        ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(-randomPropertyId);
        if (!item_rand)
        {
            return;
        }

        for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
        {
            uint32 enchantId = item_rand->Enchantment[i - PROP_ENCHANTMENT_SLOT_0];
            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            uint32 enchant_amount = 0;

            for (int k = 0; k < MAX_ITEM_ENCHANTMENT_EFFECTS; ++k)
            {
                if (item_rand->Enchantment[k] == enchantId)
                {
                    enchant_amount = uint32((item_rand->AllocationPct[k] * GenerateEnchSuffixFactor(itemId)) / 10000);
                    break;
                }
            }

            if (enchant)
                collector_->CollectEnchantStats(enchant, enchant_amount);
        }
    }
}

int32 StatsWeightCalculator::PickBestRandomPropertyId(uint32 itemId)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return 0;

    bool isSuffix = false;
    uint32 poolEntry = proto->RandomProperty;
    if (!poolEntry)
    {
        poolEntry = proto->RandomSuffix;
        isSuffix = true;
    }
    if (!poolEntry)
        return 0;

    std::vector<uint32> const& pool = sRandomItemMgr.GetEnchantmentPool(poolEntry);
    if (pool.empty())
        return 0;

    Reset();
    GenerateWeights(player_);

    int32 bestId = 0;
    float bestScore = 0.0f;
    for (uint32 enchId : pool)
    {
        int32 candidate = isSuffix ? -static_cast<int32>(enchId) : static_cast<int32>(enchId);

        collector_->Reset();
        CalculateRandomProperty(candidate, itemId);

        float score = 0.0f;
        for (uint32 i = 0; i < STATS_TYPE_MAX; ++i)
            score += stats_weights_[i] * collector_->stats[i];

        if (bestId == 0 || score > bestScore)
        {
            bestId = candidate;
            bestScore = score;
        }
    }

    collector_->Reset();
    return bestId;
}

void StatsWeightCalculator::GenerateWeights(Player* player)
{
    GenerateBasicWeights(player);
    GenerateAdditionalWeights(player);
    ApplyWeightFinetune(player);
}

void StatsWeightCalculator::GenerateBasicWeights(Player* player)
{
    // Basic weights
    stats_weights_[STATS_TYPE_STAMINA] += 0.1f;
    stats_weights_[STATS_TYPE_ARMOR] += 0.001f;
    stats_weights_[STATS_TYPE_BONUS] += 1.0f;
    stats_weights_[STATS_TYPE_MELEE_DPS] += 0.01f;
    stats_weights_[STATS_TYPE_RANGED_DPS] += 0.01f;

    if (cls == CLASS_HUNTER && (tab == HUNTER_TAB_BEAST_MASTERY || tab == HUNTER_TAB_SURVIVAL))
    {
        stats_weights_[STATS_TYPE_AGILITY] += 2.5f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 1.5f;
        stats_weights_[STATS_TYPE_HIT] += 1.7f;
        stats_weights_[STATS_TYPE_CRIT] += 1.4f;
        stats_weights_[STATS_TYPE_HASTE] += 1.6f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_RANGED_DPS] += 7.5f;
    }
    else if (cls == CLASS_HUNTER && tab == HUNTER_TAB_MARKSMANSHIP)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 2.3f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 2.25f;
        stats_weights_[STATS_TYPE_HIT] += 2.1f;
        stats_weights_[STATS_TYPE_CRIT] += 2.0f;
        stats_weights_[STATS_TYPE_HASTE] += 1.8f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_RANGED_DPS] += 10.0f;
    }
    else if (cls == CLASS_ROGUE && tab == ROGUE_TAB_COMBAT)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 1.9f;
        stats_weights_[STATS_TYPE_STRENGTH] += 1.1f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 1.8f;
        stats_weights_[STATS_TYPE_HIT] += 2.1f;
        stats_weights_[STATS_TYPE_CRIT] += 1.4f;
        stats_weights_[STATS_TYPE_HASTE] += 1.7f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 2.0f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 7.0f;
    }
    else if (cls == CLASS_DRUID && tab == DRUID_TAB_FERAL && !PlayerbotAI::IsTank(player))
    {
        stats_weights_[STATS_TYPE_AGILITY] += 2.2f;
        stats_weights_[STATS_TYPE_STRENGTH] += 2.4f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 2.3f;
        stats_weights_[STATS_TYPE_HIT] += 1.9f;
        stats_weights_[STATS_TYPE_CRIT] += 1.5f;
        stats_weights_[STATS_TYPE_HASTE] += 2.1f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 2.1f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 15.0f;
    }
    else if (cls == CLASS_ROGUE && (tab == ROGUE_TAB_ASSASSINATION || tab == ROGUE_TAB_SUBTLETY))
    {
        stats_weights_[STATS_TYPE_AGILITY] += 1.5f;
        stats_weights_[STATS_TYPE_STRENGTH] += 1.1f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 1.2f;
        stats_weights_[STATS_TYPE_HIT] += 2.1f;
        stats_weights_[STATS_TYPE_CRIT] += 1.1f;
        stats_weights_[STATS_TYPE_HASTE] += 1.8f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 2.1f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 5.0f;
    }
    else if (cls == CLASS_WARRIOR && tab == WARRIOR_TAB_FURY)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 0.8f;
        stats_weights_[STATS_TYPE_STRENGTH] += 2.5f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 0.8f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 2.1f;
        stats_weights_[STATS_TYPE_HIT] += 2.3f;
        stats_weights_[STATS_TYPE_CRIT] += 2.2f;
        stats_weights_[STATS_TYPE_HASTE] += 0.8f;
        stats_weights_[STATS_TYPE_INTELLECT] -= 2.0f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 2.0f;
        stats_weights_[STATS_TYPE_DEFENSE] -= 1.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 2.5f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 7.0f;
    }
    else if (cls == CLASS_WARRIOR && tab == WARRIOR_TAB_ARMS)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 0.8f;
        stats_weights_[STATS_TYPE_STRENGTH] += 2.5f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 0.8f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 1.7f;
        stats_weights_[STATS_TYPE_HIT] += 2.0f;
        stats_weights_[STATS_TYPE_CRIT] += 1.9f;
        stats_weights_[STATS_TYPE_HASTE] += 0.8f;
        stats_weights_[STATS_TYPE_INTELLECT] -= 2.0f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 2.0f;
        stats_weights_[STATS_TYPE_DEFENSE] -= 1.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 1.4f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 7.0f;
    }
    else if (cls == CLASS_DEATH_KNIGHT && tab == DEATH_KNIGHT_TAB_FROST)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 0.5f;
        stats_weights_[STATS_TYPE_STRENGTH] += 2.5f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 0.5f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 2.7f;
        stats_weights_[STATS_TYPE_HIT] += 2.3f;
        stats_weights_[STATS_TYPE_CRIT] += 2.2f;
        stats_weights_[STATS_TYPE_HASTE] += 2.1f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 2.5f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 7.0f;
    }
    else if (cls == CLASS_DEATH_KNIGHT && tab == DEATH_KNIGHT_TAB_UNHOLY)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 0.5f;
        stats_weights_[STATS_TYPE_STRENGTH] += 2.5f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 0.5f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 1.3f;
        stats_weights_[STATS_TYPE_HIT] += 2.2f;
        stats_weights_[STATS_TYPE_CRIT] += 1.7f;
        stats_weights_[STATS_TYPE_HASTE] += 1.8f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 1.5f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 5.0f;
    }
    else if (cls == CLASS_PALADIN && tab == PALADIN_TAB_RETRIBUTION)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 0.5f;
        stats_weights_[STATS_TYPE_STRENGTH] += 2.5f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 0.5f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 1.5f;
        stats_weights_[STATS_TYPE_HIT] += 1.9f;
        stats_weights_[STATS_TYPE_CRIT] += 1.7f;
        stats_weights_[STATS_TYPE_HASTE] += 1.6f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 2.0f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 9.0f;
    }
    else if ((cls == CLASS_SHAMAN && tab == SHAMAN_TAB_ENHANCEMENT))
    {
        stats_weights_[STATS_TYPE_AGILITY] += 1.4f;
        stats_weights_[STATS_TYPE_STRENGTH] += 1.1f;
        stats_weights_[STATS_TYPE_INTELLECT] += 0.3f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_SPELL_POWER] += 0.5f;
        stats_weights_[STATS_TYPE_ARMOR_PENETRATION] += 0.9f;
        stats_weights_[STATS_TYPE_HIT] += 2.1f;
        stats_weights_[STATS_TYPE_CRIT] += 1.5f;
        stats_weights_[STATS_TYPE_HASTE] += 1.8f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 2.0f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 8.5f;
    }
    else if (cls == CLASS_WARLOCK ||
             (cls == CLASS_MAGE && tab != MAGE_TAB_FIRE) ||
             (cls == CLASS_PRIEST && tab == PRIEST_TAB_SHADOW) ||
             (cls == CLASS_DRUID && tab == DRUID_TAB_BALANCE))
    {
        stats_weights_[STATS_TYPE_INTELLECT] += 0.3f;
        stats_weights_[STATS_TYPE_SPIRIT] += 0.6f;
        stats_weights_[STATS_TYPE_SPELL_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_HIT] += 1.1f;
        stats_weights_[STATS_TYPE_CRIT] += 0.8f;
        stats_weights_[STATS_TYPE_HASTE] += 1.0f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_RANGED_DPS] += 1.0f;
    }
    else if (cls == CLASS_MAGE && tab == MAGE_TAB_FIRE)
    {
        stats_weights_[STATS_TYPE_INTELLECT] += 0.3f;
        stats_weights_[STATS_TYPE_SPIRIT] += 0.7f;
        stats_weights_[STATS_TYPE_SPELL_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_HIT] += 1.2f;
        stats_weights_[STATS_TYPE_CRIT] += 1.1f;
        stats_weights_[STATS_TYPE_HASTE] += 0.8f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_RANGED_DPS] += 1.0f;
    }
    else if (cls == CLASS_SHAMAN && tab == SHAMAN_TAB_ELEMENTAL)
    {
        stats_weights_[STATS_TYPE_INTELLECT] += 0.5f;
        stats_weights_[STATS_TYPE_SPELL_POWER] += 1.2f;
        stats_weights_[STATS_TYPE_HIT] += 1.1f;
        stats_weights_[STATS_TYPE_CRIT] += 0.8f;
        stats_weights_[STATS_TYPE_HASTE] += 1.0f;
        stats_weights_[STATS_TYPE_MANA_REGENERATION] += 0.5f;
    }
    else if ((cls == CLASS_PALADIN && tab == PALADIN_TAB_HOLY) ||
             (cls == CLASS_SHAMAN && tab == SHAMAN_TAB_RESTORATION))
    {
        stats_weights_[STATS_TYPE_INTELLECT] += 0.9f;
        stats_weights_[STATS_TYPE_SPIRIT] += 0.15f;
        stats_weights_[STATS_TYPE_HEAL_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_MANA_REGENERATION] += 0.9f;
        stats_weights_[STATS_TYPE_CRIT] += 0.6f;
        stats_weights_[STATS_TYPE_HASTE] += 0.8f;
    }
    else if ((cls == CLASS_PRIEST && tab != PRIEST_TAB_SHADOW) ||
             (cls == CLASS_DRUID && tab == DRUID_TAB_RESTORATION))
    {
        stats_weights_[STATS_TYPE_INTELLECT] += 0.8f;
        stats_weights_[STATS_TYPE_SPIRIT] += 0.6f;
        stats_weights_[STATS_TYPE_HEAL_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_MANA_REGENERATION] += 0.9f;
        stats_weights_[STATS_TYPE_CRIT] += 0.6f;
        stats_weights_[STATS_TYPE_HASTE] += 0.8f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_RANGED_DPS] += 1.0f;
    }
    else if ((cls == CLASS_WARRIOR && tab == WARRIOR_TAB_PROTECTION) ||
             (cls == CLASS_PALADIN && tab == PALADIN_TAB_PROTECTION))
    {
        stats_weights_[STATS_TYPE_AGILITY] += 0.2f;
        stats_weights_[STATS_TYPE_STRENGTH] += 1.3f;
        stats_weights_[STATS_TYPE_STAMINA] += 3.0f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 0.2f;
        stats_weights_[STATS_TYPE_DEFENSE] += 2.5f;
        stats_weights_[STATS_TYPE_PARRY] += 2.0f;
        stats_weights_[STATS_TYPE_DODGE] += 2.0f;
        // stats_weights_[STATS_TYPE_RESILIENCE] += 2.0f;
        stats_weights_[STATS_TYPE_BLOCK_RATING] += 1.0f;
        stats_weights_[STATS_TYPE_BLOCK_VALUE] += 0.5f;
        stats_weights_[STATS_TYPE_ARMOR] += 0.15f;
        stats_weights_[STATS_TYPE_HIT] += 2.0f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 2.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 3.0f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 2.0f;
    }
    else if (cls == CLASS_DEATH_KNIGHT && tab == DEATH_KNIGHT_TAB_BLOOD)
    {
        stats_weights_[STATS_TYPE_AGILITY] += 0.2f;
        stats_weights_[STATS_TYPE_STRENGTH] += 1.3f;
        stats_weights_[STATS_TYPE_STAMINA] += 3.0f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 0.2f;
        stats_weights_[STATS_TYPE_DEFENSE] += 2.5f;
        stats_weights_[STATS_TYPE_PARRY] += 2.0f;
        stats_weights_[STATS_TYPE_DODGE] += 2.0f;
        stats_weights_[STATS_TYPE_BLOCK_RATING] -= 2.0f;
        stats_weights_[STATS_TYPE_BLOCK_VALUE] -= 2.0f;
        // stats_weights_[STATS_TYPE_RESILIENCE] += 2.0f;
        stats_weights_[STATS_TYPE_ARMOR] += 0.15f;
        stats_weights_[STATS_TYPE_HIT] += 2.0f;
        stats_weights_[STATS_TYPE_SPELL_POWER] -= 1.0f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 3.0f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 2.0f;
    }
    else
    {
        // BEAR DRUID TANK
        stats_weights_[STATS_TYPE_AGILITY] += 2.2f;
        stats_weights_[STATS_TYPE_STRENGTH] += 2.4f;
        stats_weights_[STATS_TYPE_STAMINA] += 4.0f;
        stats_weights_[STATS_TYPE_ATTACK_POWER] += 1.0f;
        stats_weights_[STATS_TYPE_DEFENSE] += 0.3f;
        stats_weights_[STATS_TYPE_DODGE] += 0.7f;
        // stats_weights_[STATS_TYPE_RESILIENCE] += 1.0f;
        stats_weights_[STATS_TYPE_ARMOR] += 0.15f;
        stats_weights_[STATS_TYPE_HIT] += 3.0f;
        stats_weights_[STATS_TYPE_CRIT] += 1.3f;
        stats_weights_[STATS_TYPE_HASTE] += 2.3f;
        stats_weights_[STATS_TYPE_EXPERTISE] += 3.7f;
        stats_weights_[STATS_TYPE_MELEE_DPS] += 3.0f;
    }
}

void StatsWeightCalculator::GenerateAdditionalWeights(Player* player)
{
    uint8 cls = player->getClass();
    // int tab = AiFactory::GetPlayerSpecTab(player);
    if (cls == CLASS_HUNTER)
    {
        if (player->HasAura(SPELL_CAREFUL_AIM))
            stats_weights_[STATS_TYPE_INTELLECT] += 1.1f;
        if (player->HasAura(SPELL_HUNTER_VS_WILD))
            stats_weights_[STATS_TYPE_STAMINA] += 0.3f;
    }
    else if (cls == CLASS_WARRIOR)
    {
        if (player->HasAura(SPELL_ARMORED_TO_THE_TEETH))
            stats_weights_[STATS_TYPE_ARMOR] += 0.03f;
    }
    else if (cls == CLASS_SHAMAN)
    {
        if (player->HasAura(SPELL_MENTAL_DEXTERITY))
            stats_weights_[STATS_TYPE_INTELLECT] += 1.1f;
    }
    else if (cls == CLASS_MAGE)
    {
        if (!HasAnySpell(player, SPELL_MOLTEN_ARMOR_RANKS))
        {
            if (tab != MAGE_TAB_FIRE)
                stats_weights_[STATS_TYPE_SPIRIT] -= 0.6f;
            else
                stats_weights_[STATS_TYPE_SPIRIT] -= 0.7f;
        }
    }
    else if (cls == CLASS_WARLOCK)
    {
        if (!HasAnySpell(player, SPELL_FEL_ARMOR_RANKS))
            stats_weights_[STATS_TYPE_SPIRIT] -= 0.4f;
    }

    if (pvpSpec_ && !exclude_resilience_)
        stats_weights_[STATS_TYPE_RESILIENCE] += 7.0f;
    else if (!pvpSpec_)
        stats_weights_[STATS_TYPE_RESILIENCE] -= 3.0f;
}

namespace
{
constexpr float kSmartStatWeightThreshold = 0.2f;

uint32 BuildSmartMaskFromWeights(float const* weights)
{
    uint32 mask = SMARTSTAT_NONE;

    if (weights[STATS_TYPE_HIT] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_HIT;
    if (weights[STATS_TYPE_SPELL_POWER] >= kSmartStatWeightThreshold ||
        weights[STATS_TYPE_HEAL_POWER] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_SPELL_POWER;
    if (weights[STATS_TYPE_HASTE] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_HASTE;
    if (weights[STATS_TYPE_CRIT] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_CRIT;
    if (weights[STATS_TYPE_INTELLECT] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_INTELLECT;
    if (weights[STATS_TYPE_SPIRIT] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_SPIRIT;
    if (weights[STATS_TYPE_EXPERTISE] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_EXPERTISE;
    if (weights[STATS_TYPE_ATTACK_POWER] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_ATTACK_POWER;
    if (weights[STATS_TYPE_ARMOR_PENETRATION] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_ARMOR_PEN;
    if (weights[STATS_TYPE_AGILITY] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_AGILITY;
    if (weights[STATS_TYPE_STAMINA] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_STAMINA;
    if (weights[STATS_TYPE_DEFENSE] >= kSmartStatWeightThreshold ||
        weights[STATS_TYPE_DODGE] >= kSmartStatWeightThreshold ||
        weights[STATS_TYPE_PARRY] >= kSmartStatWeightThreshold ||
        weights[STATS_TYPE_BLOCK_RATING] >= kSmartStatWeightThreshold ||
        weights[STATS_TYPE_BLOCK_VALUE] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_AVOIDANCE;
    if (weights[STATS_TYPE_MANA_REGENERATION] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_MP5;
    if (weights[STATS_TYPE_STRENGTH] >= kSmartStatWeightThreshold)
        mask |= SMARTSTAT_STRENGTH;

    return mask;
}
}  // namespace

uint32 StatsWeightCalculator::BuildSmartStatMask(Player* player)
{
    if (!player)
        return SMARTSTAT_NONE;

    StatsWeightCalculator calculator(player);
    calculator.Reset();
    calculator.GenerateWeights(player);

    return BuildSmartMaskFromWeights(calculator.stats_weights_);
}

void StatsWeightCalculator::CalculateItemSetMod(Player* player, ItemTemplate const* proto)
{
    uint32 itemSet = proto->ItemSet;
    if (!itemSet)
        return;

    ItemSetEntry const* setEntry = sItemSetStore.LookupEntry(itemSet);
    if (!setEntry)
        return;

    // Baseline is the set as it would look with the contested slot empty, so the piece being
    // replaced does not count toward its own score.
    uint32 base = EquippedSetPieces(player, itemSet);
    if (replaced_item_set_ == itemSet && base > 0)
        base--;

    uint32 gained = ActiveSetBonuses(setEntry, base + 1) - ActiveSetBonuses(setEntry, base);

    float multiplier = 1.0f + sPlayerbotAIConfig.itemSetBonusWeight * gained;
    if (!gained && NextSetThreshold(setEntry, base))
        multiplier += sPlayerbotAIConfig.itemSetProgressWeight * (base + 1);

    weight_ *= multiplier;
}

void StatsWeightCalculator::CalculateSocketBonus(ItemTemplate const* proto, float statSumWeight)
{
    uint32 socketNum = 0;
    float socketValue = 0.0f;
    for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS;
         ++enchant_slot)
    {
        uint8 socketColor = proto->Socket[enchant_slot - SOCK_ENCHANTMENT_SLOT].Color;

        if (!socketColor)  // no socket slot
            continue;

        socketNum++;
        if (sPlayerbotAIConfig.socketValueFactor > 0.0f)
            socketValue += BestGemScore(socketColor);
    }

    if (!socketNum)
        return;

    // Gem scores live in the raw stat-sum space, so the ratio is taken against statSumWeight rather
    // than the running weight_. Expressing socket value as a fraction of the item's own stats keeps
    // the multiplier unit-consistent and free of magic constants.
    float multiplier;
    if (socketValue > 0.0f && statSumWeight > 0.001f)
        multiplier = 1.0f + sPlayerbotAIConfig.socketValueFactor * (socketValue / statSumWeight);
    else
        multiplier = 1.0f + socketNum * sPlayerbotAIConfig.socketWeightPerSocket;

    // A cap below 1.0 would turn the bonus into a penalty on every socketed item, so floor it.
    multiplier = std::min(multiplier, std::max(1.0f, sPlayerbotAIConfig.socketMaxMultiplier));

    weight_ *= multiplier;
}

uint8 StatsWeightCalculator::ProgressionTier()
{
    // Costs a group lookup plus up to 18 quest status reads, and only socketed items ever ask for it,
    // so it is not worth paying on construction: these calculators are built per item evaluation.
    if (progression_tier_ < 0)
        progression_tier_ = static_cast<int16>(sProgressionMgr.GetBotProgressionTier(player_));

    return static_cast<uint8>(progression_tier_);
}

float StatsWeightCalculator::BestGemScore(uint8 socketColor)
{
    // Only the flags a caller can flip between CalculateItem calls need to be in the key; everything
    // else the score depends on (bot, class, spec, level) is fixed for this calculator's lifetime.
    uint32 key = (pvpSpec_ ? 1u << 9 : 0) | (exclude_resilience_ ? 1u << 8 : 0) | socketColor;

    auto it = best_gem_score_.find(key);
    if (it != best_gem_score_.end())
        return it->second;

    // Nothing to score before the factory has loaded its gem pool; don't cache that as a real 0.
    if (PlayerbotFactory::enchantGemIdCache.empty())
        return 0.0f;

    bool isMetaSocket = (socketColor & SOCKET_COLOR_META) != 0;

    // CalculateEnchant calls Reset(), which would wipe the weight and collector state of the
    // in-flight CalculateItem, so score the candidate gems on a throwaway calculator.
    // Cap priority is deliberately left off here: an under-cap tank would re-rank whole gear pieces
    // on their socket value and churn gear as it crosses the cap. The estimate is a little low for
    // that bot until it is crit-immune.
    StatsWeightCalculator gemCalculator(player_);
    gemCalculator.SetPvpSpec(pvpSpec_);
    gemCalculator.SetExcludeResilience(exclude_resilience_);

    float best = 0.0f;
    for (uint32 const& enchantGem : PlayerbotFactory::enchantGemIdCache)
    {
        ItemTemplate const* gemTemplate = sObjectMgr->GetItemTemplate(enchantGem);
        if (!gemTemplate)
            continue;

        if (sPlayerbotAIConfig.limitEnchantExpansion && lvl <= 70 && enchantGem >= 39900)
            continue;

        // Has to match ApplyEnchantAndGemsNew exactly, or a socketed item gets scored on gems the
        // bot's realm progression will not let it actually socket.
        if (!sProgressionMgr.IsGemAllowed(gemTemplate, ProgressionTier()))
            continue;

        if (gemTemplate->ItemLevel > lvl)
            continue;

        GemPropertiesEntry const* gemProperties = sGemPropertiesStore.LookupEntry(gemTemplate->GemProperties);
        if (!gemProperties)
            continue;

        // meta gems only go in meta sockets, colored gems only in colored sockets
        bool isMetaGem = gemProperties->color == SOCKET_COLOR_META;
        if (isMetaGem != isMetaSocket)
            continue;

        uint32 enchant_id = gemProperties->spellitemenchantement;
        if (!enchant_id)
            continue;

        SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
        if (!enchant || (enchant->slot != PERM_ENCHANTMENT_SLOT && enchant->slot != TEMP_ENCHANTMENT_SLOT))
            continue;

        if (enchant->requiredLevel > lvl)
            continue;

        // Skill-gated gems are skipped: a bot's profession skill is not checked here. Jeweler's gems
        // carry that requirement on the item rather than the enchant, hence both checks.
        if (enchant->requiredSkill || gemTemplate->RequiredSkill)
            continue;

        best = std::max(best, gemCalculator.CalculateEnchant(enchant_id));
    }

    best_gem_score_[key] = best;
    return best;
}

void StatsWeightCalculator::CalculateItemTypePenalty(ItemTemplate const* proto)
{
    // // penalty for different type armor
    // if (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass >= ITEM_SUBCLASS_ARMOR_CLOTH &&
    //     proto->SubClass <= ITEM_SUBCLASS_ARMOR_PLATE && NotBestArmorType(proto->SubClass))
    // {
    //     weight_ *= 1.0;
    // }
    if (proto->Class == ITEM_CLASS_WEAPON)
    {
        // double hand
        bool isDoubleHand = proto->Class == ITEM_CLASS_WEAPON &&
                            !(ITEM_SUBCLASS_MASK_SINGLE_HAND & (1 << proto->SubClass)) &&
                            !(ITEM_SUBCLASS_MASK_WEAPON_RANGED & (1 << proto->SubClass));

        if (isDoubleHand)
        {
            weight_ *= 0.5;
            // spec without double hand
            // enhancement, rogue, ice dk, unholy dk, shield tank, fury warrior without titan's grip but with duel wield
            if (((cls == CLASS_SHAMAN && tab == SHAMAN_TAB_ENHANCEMENT && player_->CanDualWield()) ||
                 (cls == CLASS_ROGUE) || (cls == CLASS_DEATH_KNIGHT && tab == DEATH_KNIGHT_TAB_FROST) ||
                 (cls == CLASS_WARRIOR && tab == WARRIOR_TAB_FURY && !player_->CanTitanGrip() &&
                  player_->CanDualWield()) ||
                 (cls == CLASS_WARRIOR && tab == WARRIOR_TAB_PROTECTION) ||
                 (cls == CLASS_PALADIN && tab == PALADIN_TAB_PROTECTION) ||
                 (cls == CLASS_PALADIN && tab == PALADIN_TAB_HOLY)))
            {
                weight_ *= 0.1;
            }
        }
        // spec with double hand
        // fury without duel wield, arms, bear, retribution, blood dk
        if (!isDoubleHand)
        {
            if ((cls == CLASS_HUNTER && !player_->CanDualWield()) ||
                (cls == CLASS_WARRIOR && tab == WARRIOR_TAB_FURY && !player_->CanDualWield()) ||
                (cls == CLASS_WARRIOR && tab == WARRIOR_TAB_ARMS) || (cls == CLASS_DRUID && tab == DRUID_TAB_FERAL) ||
                (cls == CLASS_PALADIN && tab == PALADIN_TAB_RETRIBUTION) ||
                (cls == CLASS_DEATH_KNIGHT && tab == DEATH_KNIGHT_TAB_BLOOD) ||
                (cls == CLASS_SHAMAN && tab == SHAMAN_TAB_ENHANCEMENT && !player_->CanDualWield()))
            {
                weight_ *= 0.1;
            }
            // caster's main hand (cannot duel weapon but can equip two-hands stuff)
            if ((cls == CLASS_MAGE || cls == CLASS_PRIEST || cls == CLASS_WARLOCK || cls == CLASS_DRUID ||
                (cls == CLASS_SHAMAN && !player_->CanDualWield())) &&
                !(cls == CLASS_PALADIN && tab == PALADIN_TAB_HOLY))
            {
                weight_ *= 0.65;
            }
            if (cls == CLASS_PALADIN && tab == PALADIN_TAB_HOLY)
            {
                weight_ *= 0.8;
            }
        }
        // fury with titan's grip
        if ((!isDoubleHand || proto->SubClass == ITEM_SUBCLASS_WEAPON_POLEARM ||
             proto->SubClass == ITEM_SUBCLASS_WEAPON_STAFF) &&
            (cls == CLASS_WARRIOR && tab == WARRIOR_TAB_FURY && player_->CanTitanGrip()))
        {
            weight_ *= 0.1;
        }

        if (cls == CLASS_HUNTER && proto->SubClass == ITEM_SUBCLASS_WEAPON_THROWN)
        {
            weight_ *= 0.1;
        }

        if (lvl >= 10 && cls == CLASS_ROGUE && (tab == ROGUE_TAB_ASSASSINATION || tab == ROGUE_TAB_SUBTLETY) &&
            proto->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER)
        {
            weight_ *= 1.5;
        }

        if (cls == CLASS_ROGUE && player_->HasAura(SPELL_ROGUE_SWORD_SPECIALIZATION) &&
            (proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD || proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE))
        {
            weight_ *= 1.1;
        }
        if (cls == CLASS_WARRIOR && player_->HasAura(SPELL_POLEAXE_SPECIALIZATION) &&
            (proto->SubClass == ITEM_SUBCLASS_WEAPON_POLEARM || proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE2))
        {
            weight_ *= 1.1;
        }
        if (cls == CLASS_DEATH_KNIGHT && player_->HasAura(SPELL_NERVES_OF_COLD_STEEL) && !isDoubleHand)
        {
            weight_ *= 1.3;
        }
        bool slowDelay = proto->Delay > 2500;
        if (cls == CLASS_SHAMAN && tab == SHAMAN_TAB_ENHANCEMENT && slowDelay)
            weight_ *= 1.1;
    }
}

bool StatsWeightCalculator::NotBestArmorType(uint32 item_subclass_armor)
{
    if (player_->HasSkill(SKILL_PLATE_MAIL))
    {
        return item_subclass_armor != ITEM_SUBCLASS_ARMOR_PLATE;
    }
    if (player_->HasSkill(SKILL_MAIL))
    {
        return item_subclass_armor != ITEM_SUBCLASS_ARMOR_MAIL;
    }
    if (player_->HasSkill(SKILL_LEATHER))
    {
        return item_subclass_armor != ITEM_SUBCLASS_ARMOR_LEATHER;
    }
    return false;
}

void StatsWeightCalculator::ApplyOverflowPenalty(Player* player)
{
    {
        float hit_current, hit_overflow;
        float validPoints;
        if (hitOverflowType_ & CollectorType::SPELL)
        {
            hit_current = player->GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE);
            hit_current +=
                player->GetTotalAuraModifier(SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT);  // suppression (18176)
            hit_current += player->GetRatingBonusValue(CR_HIT_SPELL);

            if (cls == CLASS_PRIEST && tab == PRIEST_TAB_SHADOW && player->HasAura(SPELL_SHADOW_FOCUS))
                hit_current += 3;
            if (cls == CLASS_MAGE && tab == MAGE_TAB_ARCANE && player->HasAura(SPELL_ARCANE_FOCUS))
                hit_current += 3;

            hit_overflow = SPELL_HIT_OVERFLOW;
            if (hit_overflow > hit_current)
                validPoints = (hit_overflow - hit_current) / player->GetRatingMultiplier(CR_HIT_SPELL);
            else
                validPoints = 0;
        }
        else if (hitOverflowType_ & CollectorType::MELEE)
        {
            hit_current = player->GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);
            hit_current += player->GetRatingBonusValue(CR_HIT_MELEE);
            hit_overflow = MELEE_HIT_OVERFLOW;
            if (hit_overflow > hit_current)
                validPoints = (hit_overflow - hit_current) / player->GetRatingMultiplier(CR_HIT_MELEE);
            else
                validPoints = 0;
        }
        else
        {
            hit_current = player->GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);
            hit_current += player->GetRatingBonusValue(CR_HIT_RANGED);
            hit_overflow = RANGED_HIT_OVERFLOW;
            if (hit_overflow > hit_current)
                validPoints = (hit_overflow - hit_current) / player->GetRatingMultiplier(CR_HIT_RANGED);
            else
                validPoints = 0;
        }
        collector_->stats[STATS_TYPE_HIT] = std::min(collector_->stats[STATS_TYPE_HIT], validPoints);
    }

    {
        if (type_ & CollectorType::MELEE)
        {
            float expertise_current, expertise_overflow;
            expertise_current = player->GetUInt32Value(PLAYER_EXPERTISE);
            expertise_current += player->GetRatingBonusValue(CR_EXPERTISE);
            expertise_overflow = EXPERTISE_OVERFLOW;

            float validPoints;
            if (expertise_overflow > expertise_current)
                validPoints = (expertise_overflow - expertise_current) / player->GetRatingMultiplier(CR_EXPERTISE);
            else
                validPoints = 0;

            collector_->stats[STATS_TYPE_EXPERTISE] = std::min(collector_->stats[STATS_TYPE_EXPERTISE], validPoints);
        }
    }

    {
        if (type_ & CollectorType::MELEE)
        {
            float defense_current, defense_overflow;
            defense_current = player->GetRatingBonusValue(CR_DEFENSE_SKILL);
            defense_overflow = DEFENSE_OVERFLOW;

            float validPoints;
            if (defense_overflow > defense_current)
                validPoints = (defense_overflow - defense_current) / player->GetRatingMultiplier(CR_DEFENSE_SKILL);
            else
                validPoints = 0;

            collector_->stats[STATS_TYPE_DEFENSE] = std::min(collector_->stats[STATS_TYPE_DEFENSE], validPoints);
        }
    }

    {
        if (type_ & (CollectorType::MELEE | CollectorType::RANGED))
        {
            float armor_penetration_current, armor_penetration_overflow;
            armor_penetration_current = player->GetRatingBonusValue(CR_ARMOR_PENETRATION);
            armor_penetration_overflow = ARMOR_PENETRATION_OVERFLOW;

            float validPoints;
            if (armor_penetration_overflow > armor_penetration_current)
                validPoints = (armor_penetration_overflow - armor_penetration_current) /
                              player->GetRatingMultiplier(CR_ARMOR_PENETRATION);
            else
                validPoints = 0;

            collector_->stats[STATS_TYPE_ARMOR_PENETRATION] =
                std::min(collector_->stats[STATS_TYPE_ARMOR_PENETRATION], validPoints);
        }
    }
}

void StatsWeightCalculator::ApplyWeightFinetune(Player* player)
{
    {
        if (type_ & (CollectorType::MELEE | CollectorType::RANGED))
        {
            float armor_penetration_current /*, armor_penetration_overflow*/;  // not used, line marked for removal.
            armor_penetration_current = player->GetRatingBonusValue(CR_ARMOR_PENETRATION);
            if (armor_penetration_current > 50)
                stats_weights_[STATS_TYPE_ARMOR_PENETRATION] *= 1.2f;
        }
    }

    // A tank short of 540 defense is taking crits, which no amount of stamina fixes. Outweigh the
    // stamina gems until the cap is reached; ApplyOverflowPenalty caps the stat at what is still
    // missing, so the last gem before the cap is only worth the part that counts.
    // Bears are exempt: Survival of the Fittest grants crit immunity, so they never chase the cap.
    if (enable_cap_priority_ && (type_ & CollectorType::MELEE_TANK) && cls != CLASS_DRUID &&
        player->GetRatingBonusValue(CR_DEFENSE_SKILL) < DEFENSE_OVERFLOW)
    {
        stats_weights_[STATS_TYPE_DEFENSE] = std::max(stats_weights_[STATS_TYPE_DEFENSE], DEFENSE_UNDERCAP_WEIGHT);
    }
}

namespace
{
// Speed preference is a tiebreaker between comparable weapons, never a reason to keep a worse item:
// the strong weight is for specs with a mechanic that actually scales off weapon speed, the weak one
// for specs where everything normalises and the preference is cosmetic.
constexpr float kSpeedWeightStrong = 0.15f;
constexpr float kSpeedWeightWeak = 0.05f;

// Ramp instead of a threshold. A hard cutoff let 100 ms decide the entire multiplier, so a 2.5s epic
// scored below a 2.6s green.
float PreferSlow(uint32 delay, uint32 lo, uint32 hi, float weight)
{
    float const t = (static_cast<float>(delay) - static_cast<float>(lo)) / static_cast<float>(hi - lo);
    return 1.0f + weight * std::clamp(t, 0.0f, 1.0f);
}

float PreferFast(uint32 delay, uint32 lo, uint32 hi, float weight)
{
    float const t = (static_cast<float>(hi) - static_cast<float>(delay)) / static_cast<float>(hi - lo);
    return 1.0f + weight * std::clamp(t, 0.0f, 1.0f);
}
}  // namespace

float StatsWeightCalculator::ApplyPreferredSpecWeapons(ItemTemplate const* proto, int32 slot)
{
    // Applies to mainhand, offhand, and ranged slots only.
    if (slot != EQUIPMENT_SLOT_MAINHAND &&
        slot != EQUIPMENT_SLOT_OFFHAND  &&
        slot != EQUIPMENT_SLOT_RANGED)
        return 1.0f;

    uint32 const delay = proto->Delay;  // milliseconds
    bool const isTwoHand = proto->InventoryType == INVTYPE_2HWEAPON;

    // Hunter: melee weapons are stat sticks - speed irrelevant. Steady Shot adds the un-normalised
    // ranged damage range plus ammo DPS times weapon speed, which is what makes a slow ranged weapon
    // worth real damage - Aimed Shot normalises and Chimera/Explosive never touch weapon damage.
    if (cls == CLASS_HUNTER)
    {
        if (slot == EQUIPMENT_SLOT_RANGED)
            return PreferSlow(delay, 2600, 3000, kSpeedWeightStrong);
        return 1.0f;
    }

    // Feral Druid: forms normalise attack speed; raw weapon Delay is irrelevant.
    if (cls == CLASS_DRUID && tab == DRUID_TAB_FERAL)
        return 1.0f;

    // Frost DK dual-wields and every ability it uses normalises (1H to 2.4s, Killing Machine is PPM),
    // so weapon speed changes nothing.
    if (cls == CLASS_DEATH_KNIGHT && tab == DEATH_KNIGHT_TAB_FROST)
        return 1.0f;

    switch (cls)
    {
        // Deep Wounds bleeds for a share of un-normalised average weapon damage, so slow weapons are a
        // real gain for every warrior spec. Poleaxe Specialization is handled separately off the aura -
        // gating on axes here would push warriors who never took the talent onto them.
        case CLASS_WARRIOR:
            if (tab == WARRIOR_TAB_ARMS)
            {
                if (slot == EQUIPMENT_SLOT_MAINHAND)
                    return PreferSlow(delay, 3000, 3600, kSpeedWeightStrong);
            }
            else if (tab == WARRIOR_TAB_FURY)
            {
                if (!player_->CanDualWield())
                {
                    // Pre-DW: treat like Arms - slow 2H in mainhand only.
                    if (slot == EQUIPMENT_SLOT_MAINHAND)
                        return PreferSlow(delay, 3000, 3600, kSpeedWeightStrong);
                }
                else if (player_->CanTitanGrip())
                {
                    // Titan's Grip: slow 2H in both hands.
                    return PreferSlow(delay, 3000, 3600, kSpeedWeightStrong);
                }
                else if (!isTwoHand)
                {
                    // 1H DW: slow 1H in both hands. 2H excluded so a 2H heirloom can't ride the 1H ramp.
                    return PreferSlow(delay, 2200, 2600, kSpeedWeightStrong);
                }
            }
            else if (tab == WARRIOR_TAB_PROTECTION)
            {
                // Prot: slow 1H in mainhand. Shield in offhand, no speed preference.
                if (slot == EQUIPMENT_SLOT_MAINHAND && !isTwoHand)
                    return PreferSlow(delay, 2200, 2600, kSpeedWeightStrong);
            }
            break;

        // Seals proc off PPM and every strike normalises, so paladin speed preference is cosmetic.
        case CLASS_PALADIN:
            if (tab == PALADIN_TAB_RETRIBUTION)
            {
                if (slot == EQUIPMENT_SLOT_MAINHAND)
                    return PreferSlow(delay, 3000, 3600, kSpeedWeightWeak);
            }
            else if (tab == PALADIN_TAB_PROTECTION)
            {
                if (slot == EQUIPMENT_SLOT_MAINHAND && !isTwoHand)
                    return PreferSlow(delay, 2200, 2600, kSpeedWeightWeak);
            }
            break;

        // Blood / Unholy want a 2H, but Heart Strike and Scourge Strike normalise to 3.3s, so speed
        // within the 2H pool barely matters.
        case CLASS_DEATH_KNIGHT:
            if (tab == DEATH_KNIGHT_TAB_BLOOD || tab == DEATH_KNIGHT_TAB_UNHOLY)
            {
                if (slot == EQUIPMENT_SLOT_MAINHAND)
                    return PreferSlow(delay, 3000, 3600, kSpeedWeightWeak);
            }
            break;

        // Windfury multiplies its AP bonus by weapon speed while the proc rate itself is PPM-normalised,
        // so slow weapons are a genuine gain in both hands. Nothing rewards matching MH/OH speeds.
        case CLASS_SHAMAN:
            if (tab == SHAMAN_TAB_ENHANCEMENT)
            {
                if (!player_->CanDualWield())
                {
                    // Pre-Dual Wield: Enhancement plays like a 2H spec.
                    if (slot == EQUIPMENT_SLOT_MAINHAND)
                        return PreferSlow(delay, 3000, 3600, kSpeedWeightStrong);
                }
                else if (!isTwoHand)
                {
                    return PreferSlow(delay, 2200, 2600, kSpeedWeightStrong);
                }
            }
            break;

        case CLASS_ROGUE:
            if (tab == ROGUE_TAB_COMBAT)
            {
                // Combat Potency refunds energy on a flat chance per off-hand hit, so more off-hand
                // swings is real throughput. Mainhand only feeds normalised strikes.
                if (slot == EQUIPMENT_SLOT_MAINHAND)
                    return PreferSlow(delay, 2200, 2600, kSpeedWeightWeak);
                if (slot == EQUIPMENT_SLOT_OFFHAND)
                    return PreferFast(delay, 1300, 1800, kSpeedWeightStrong);
            }
            else  // Assassination / Subtlety: daggers, and daggers normalise to 1.7s.
            {
                if (proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER)
                    break;
                if (slot == EQUIPMENT_SLOT_MAINHAND)
                    return PreferSlow(delay, 1400, 1800, kSpeedWeightWeak);
                if (slot == EQUIPMENT_SLOT_OFFHAND)
                    return PreferFast(delay, 1300, 1800, kSpeedWeightWeak);
            }
            break;

        default:
            break;
    }

    return 1.0f;
}
