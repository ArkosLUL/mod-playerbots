/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_STATSWEIGHTCALCULATOR_H
#define PLAYERBOTS_STATSWEIGHTCALCULATOR_H

#include <unordered_map>

#include "Player.h"
#include "StatsCollector.h"

#define ITEM_SUBCLASS_MASK_SINGLE_HAND                                                                        \
    ((1 << ITEM_SUBCLASS_WEAPON_AXE) | (1 << ITEM_SUBCLASS_WEAPON_MACE) | (1 << ITEM_SUBCLASS_WEAPON_SWORD) | \
     (1 << ITEM_SUBCLASS_WEAPON_DAGGER) | (1 << ITEM_SUBCLASS_WEAPON_FIST))

enum StatsOverflowThreshold
{
    SPELL_HIT_OVERFLOW = 14,
    MELEE_HIT_OVERFLOW = 8,
    RANGED_HIT_OVERFLOW = 8,
    EXPERTISE_OVERFLOW = 26,
    DEFENSE_OVERFLOW = 140,
    ARMOR_PENETRATION_OVERFLOW = 100
};

enum SmartStatFlag : uint32
{
    SMARTSTAT_NONE = 0,
    SMARTSTAT_HIT = 1u << 0,
    SMARTSTAT_SPELL_POWER = 1u << 1,
    SMARTSTAT_HASTE = 1u << 2,
    SMARTSTAT_CRIT = 1u << 3,
    SMARTSTAT_INTELLECT = 1u << 4,
    SMARTSTAT_SPIRIT = 1u << 5,
    SMARTSTAT_EXPERTISE = 1u << 6,
    SMARTSTAT_ATTACK_POWER = 1u << 7,
    SMARTSTAT_ARMOR_PEN = 1u << 8,
    SMARTSTAT_AGILITY = 1u << 9,
    SMARTSTAT_STAMINA = 1u << 10,
    SMARTSTAT_AVOIDANCE = 1u << 11,
    SMARTSTAT_MP5 = 1u << 12,
    SMARTSTAT_STRENGTH = 1u << 13
};

class StatsWeightCalculator
{
public:
    StatsWeightCalculator(Player* player);
    void Reset();
    float CalculateItem(uint32 itemId, int32 randomPropertyId = 0, int32 slot = -1);
    float CalculateEnchant(uint32 enchantId);
    int32 PickBestRandomPropertyId(uint32 itemId);
    static uint32 BuildSmartStatMask(Player* player);

    void SetOverflowPenalty(bool apply) { enable_overflow_penalty_ = apply; }
    void SetItemSetBonus(bool apply) { enable_item_set_bonus_ = apply; }
    // Set id of the piece occupying the slot being contested. That piece counts as removed, so the
    // incumbent and the challenger are measured against the same baseline. 0 = no slot context.
    void SetReplacedItemSet(uint32 setId) { replaced_item_set_ = setId; }
    void SetQualityBlend(bool apply) { enable_quality_blend_ = apply; }
    void SetPvpSpec(bool isPvp) { pvpSpec_ = isPvp; }
    void SetExcludeResilience(bool exclude) { exclude_resilience_ = exclude; }

    private:
    void GenerateWeights(Player* player);
    void GenerateBasicWeights(Player* player);
    void GenerateAdditionalWeights(Player* player);

    void CalculateRandomProperty(int32 randomPropertyId, uint32 itemId);
    void CalculateItemSetMod(Player* player, ItemTemplate const* proto);
    // statSumWeight is the item's plain weighted stat sum, before the type penalty and set
    // multiplier; socket value is scored as a fraction of it.
    void CalculateSocketBonus(ItemTemplate const* proto, float statSumWeight);
    // Score of the best gem this bot would slot into a socket of the given color.
    float BestGemScore(uint8 socketColor);

    // Resolved on first use and then fixed for this calculator's lifetime, so it stays out of the
    // gem score key.
    uint8 ProgressionTier();

    void CalculateItemTypePenalty(ItemTemplate const* proto);
    float ApplyPreferredSpecWeapons(ItemTemplate const* proto, int32 slot);

    bool NotBestArmorType(uint32 item_subclass_armor);

    void ApplyOverflowPenalty(Player* player);
    void ApplyWeightFinetune(Player* player);

private:
    Player* player_;
    CollectorType type_;
    CollectorType hitOverflowType_;
    std::unique_ptr<StatsCollector> collector_;
    uint8 cls;
    uint8 lvl;
    // -1 until ProgressionTier() resolves it.
    int16 progression_tier_ = -1;
    int tab;
    bool enable_overflow_penalty_;
    bool enable_item_set_bonus_;
    bool enable_quality_blend_;
    uint32 replaced_item_set_ = 0;

    float weight_;
    float stats_weights_[STATS_TYPE_MAX];
    bool pvpSpec_ = false;
    bool exclude_resilience_ = false;
    // key: pvpSpec | excludeResilience | socketColor. Instance-local, so no cross-bot bleed and no
    // locking; scoring every candidate gem per socket per item would otherwise dominate the cost.
    std::unordered_map<uint32, float> best_gem_score_;
};

#endif
