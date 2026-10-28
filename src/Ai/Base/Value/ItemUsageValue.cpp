/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ItemUsageValue.h"
#include "AiFactory.h"
#include "BisListMgr.h"
#include "ChatHelper.h"
#include "DBCStores.h"
#include "GearPlacement.h"
#include "Group.h"
#include "GuildTaskMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "LootAction.h"
#include "LootObjectStack.h"
#include "LootRollAction.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "Playerbots.h"
#include "QuestAction.h"
#include "RandomItemMgr.h"
#include "ServerFacade.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "StatsWeightCalculator.h"
#include "Util.h"
#include "World.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace
{
ParsedItemUsage ParseItemUsageQualifier(std::string const& qualifier)
{
    ParsedItemUsage parsed;
    size_t const pos = qualifier.find(',');

    if (pos != std::string::npos)
    {
        parsed.itemId = atoi(qualifier.substr(0, pos).c_str());
        parsed.randomPropertyId = atoi(qualifier.substr(pos + 1).c_str());
        return parsed;
    }

    parsed.itemId = atoi(qualifier.c_str());

    return parsed;
}
} // namespace

// Lowercase helper for item names using core UTF-8 utilities.
std::string ToLowerUtf8(std::string const& s)
{
    if (s.empty())
        return s;

    std::wstring w;
    if (!Utf8toWStr(s, w))
        return s;

    wstrToLower(w);

    std::string lowered;
    if (!WStrToUtf8(w, lowered))
        return s;

    return lowered;
}

uint32 GetRecipeSkill(ItemTemplate const* proto)
{
    if (!proto)
        return 0;

    // Primary path: DB usually sets RequiredSkill on recipe items.
    if (proto->RequiredSkill)
        return proto->RequiredSkill;

    // Fallback heuristic on SubClass (books used by professions).
    switch (proto->SubClass)
    {
        case ITEM_SUBCLASS_BOOK: // e.g. Book of Glyph Mastery
        {
            // If the name hints glyphs, assume Inscription.
            std::string const lowered = ToLowerUtf8(proto->Name1);
            if (lowered.find("glyph") != std::string::npos)
                return SKILL_INSCRIPTION;
            break;
        }
        case ITEM_SUBCLASS_LEATHERWORKING_PATTERN:
            return SKILL_LEATHERWORKING;
        case ITEM_SUBCLASS_TAILORING_PATTERN:
            return SKILL_TAILORING;
        case ITEM_SUBCLASS_ENGINEERING_SCHEMATIC:
            return SKILL_ENGINEERING;
        case ITEM_SUBCLASS_BLACKSMITHING:
            return SKILL_BLACKSMITHING;
        case ITEM_SUBCLASS_COOKING_RECIPE:
            return SKILL_COOKING;
        case ITEM_SUBCLASS_ALCHEMY_RECIPE:
            return SKILL_ALCHEMY;
        case ITEM_SUBCLASS_FIRST_AID_MANUAL:
            return SKILL_FIRST_AID;
        case ITEM_SUBCLASS_ENCHANTING_FORMULA:
            return SKILL_ENCHANTING;
        case ITEM_SUBCLASS_FISHING_MANUAL:
            return SKILL_FISHING;
        case ITEM_SUBCLASS_JEWELCRAFTING_RECIPE:
            return SKILL_JEWELCRAFTING;
        default:
            break;
    }

    return 0;
}

#include <unordered_set>

static bool IsRoguePoison(ItemTemplate const* proto);

ItemUsage ItemUsageValue::Calculate()
{
    ParsedItemUsage const parsed = ParseItemUsageQualifier(qualifier);
    uint32 itemId = parsed.itemId;
    uint32 randomPropertyId = parsed.randomPropertyId;
    if (!itemId)
        return ITEM_USAGE_NONE;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return ITEM_USAGE_NONE;

    if (IsRealPlayer(botAI->GetMaster()))
    {
        if (IsItemUsefulForSkill(proto) || IsItemNeededForSkill(proto))
            return ITEM_USAGE_SKILL;

        if ((proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_MISC ||
             proto->Class == ITEM_CLASS_REAGENT) &&
            IsItemNeededForClassSpell(proto))
            return ITEM_USAGE_KEEP;
    }
    else
    {
        bool needItem = false;

        if (IsItemNeededForSkill(proto))
            needItem = true;
        else
        {
            bool lowBagSpace = AI_VALUE(uint8, "bag space") > 50;

            if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_MISC ||
                proto->Class == ITEM_CLASS_REAGENT)
                needItem = IsItemNeededForUsefullSpell(proto, lowBagSpace);
            else if (proto->Class == ITEM_CLASS_RECIPE)
            {
                if (bot->HasSpell(proto->Spells[2].SpellId))
                    needItem = false;
                else
                    needItem = bot->CanUseItem(proto) == EQUIP_ERR_OK;
            }
        }

        if (needItem)
        {
            float stacks = CurrentStacks(proto);
            if (stacks < 1)
                return ITEM_USAGE_SKILL;  // Buy more.
            if (stacks <= 2)
                return ITEM_USAGE_KEEP;
        }
    }

    if (proto->Class == ITEM_CLASS_KEY)
        return ITEM_USAGE_USE;

    const uint32_t maxCount = proto->MaxCount;

    if (proto->Class == ITEM_CLASS_CONSUMABLE &&
        (maxCount == 0 || bot->GetItemCount(itemId, false) < maxCount))
    {
        std::string foodType = GetConsumableType(proto, bot->GetPower(POWER_MANA));

        // Treat these as vendor trash rather than buying two stacks of something the bot will never
        // drink. Drinks are untouched - they still refill between pulls.
        if (foodType == "mana potion" && SkipsManaPotions(bot))
            foodType.clear();

        if (!foodType.empty() && bot->CanUseItem(proto) == EQUIP_ERR_OK)
        {
            float stacks = BetterStacks(proto, foodType);
            if (stacks < 2)
            {
                stacks += CurrentStacks(proto);

                if (stacks < 2)
                    return ITEM_USAGE_USE;  // Buy some to get to 2 stacks
                else if (stacks < 3)        // Keep the item if less than 3 stacks
                    return ITEM_USAGE_KEEP;
            }
        }
    }

    if (bot->GetGuildId() && GuildTaskMgr::instance().IsGuildTaskItem(itemId, bot->GetGuildId()))
        return ITEM_USAGE_GUILD_TASK;

    // Starters the bot can take stay in the bags and are used from there; the core deletes the
    // starting item on accept unless the quest also requires it, so they are never equipped.
    if (IsStartQuestItemUsefulFor(bot, proto))
        return ITEM_USAGE_QUEST;

    ItemUsage equip = QueryItemUsageForEquip(proto, randomPropertyId);
    if (equip != ITEM_USAGE_NONE)
        return equip;

    // Get item instance to check if it's soulbound
    Item* item = bot->GetItemByEntry(proto->ItemId);
    bool isSoulbound = item && item->IsSoulBound();

    if ((proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON) &&
        botAI->HasSkill(SKILL_ENCHANTING) &&
        proto->Quality >= ITEM_QUALITY_UNCOMMON)
    {
        // Retrieve the bot's Enchanting skill level
        uint32 enchantingSkill = bot->GetSkillValue(SKILL_ENCHANTING);

        // Only disenchant if skilled enough and binding allows it
        if (enchantingSkill >= proto->RequiredDisenchantSkill &&
            (proto->Bonding == BIND_WHEN_PICKED_UP || (proto->Bonding == BIND_WHEN_EQUIPPED && isSoulbound)))
            return ITEM_USAGE_DISENCHANT;
    }

    Player* master = botAI->GetMaster();
    bool botNeedsItemForQuest = IsItemUsefulForQuest(bot, proto);
    bool masterNeedsItemForQuest = master && sPlayerbotAIConfig.SyncQuestWithPlayer && IsItemUsefulForQuest(master, proto);

    // Identify the source of loot
    LootObject lootObject = AI_VALUE(LootObject, "loot target");

    // Get GUID of loot source
    ObjectGuid lootGuid = lootObject.guid;

    // Check if loot source is an item
    bool isLootFromItem = lootGuid.IsItem();

    // If the loot is from an item in the bot’s bags, ignore syncQuestWithPlayer
    if (isLootFromItem && botNeedsItemForQuest)
        return ITEM_USAGE_QUEST;

    // If this is not a SelfBot acting alone and the master needs this quest item, defer to the master
    if (!IsSelfBot(bot) && masterNeedsItemForQuest)
        return ITEM_USAGE_NONE;

    // If the bot itself needs the item for a quest, allow looting
    if (botNeedsItemForQuest)
        return ITEM_USAGE_QUEST;

    // If the bot has the exact quest requirement amount, keep it to prevent a sell/buy cycle
    for (auto const& questPair : botAI->GetCurrentQuestsRequiringItemId(proto->ItemId))
    {
        Quest const* quest = questPair.first;
        if (!quest)
            continue;

        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
        {
            if (quest->RequiredItemId[i] != proto->ItemId)
                continue;

            uint32 currentCount = AI_VALUE2(uint32, "item count", proto->Name1);
            uint32 requiredCount = quest->RequiredItemCount[i];

            if (currentCount == requiredCount)
                return ITEM_USAGE_KEEP;  // Have exact amount needed, keep it
        }
    }

    if (proto->Class == ITEM_CLASS_PROJECTILE && bot->CanUseItem(proto) == EQUIP_ERR_OK)
    {
        ItemUsage ammoUsage = QueryItemUsageForAmmo(proto);
        if (ammoUsage != ITEM_USAGE_NONE)
            return ammoUsage;
    }

    // Keep only the best stone/oil per weapon and the best poison per family; obsolete or
    // mismatched enhancements fall through to VENDOR/AH.
    if (proto->Class == ITEM_CLASS_CONSUMABLE && IsTemporaryWeaponEnchantment(proto) &&
        bot->CanUseItem(proto) == EQUIP_ERR_OK)
    {
        // Rogue poisons only: stones/oils share consumable subclass 8 but use family-less spells,
        // so they take the weapon branch below instead of being sold as obsolete poisons.
        if (IsRoguePoison(proto))
        {
            if (IsBestPoison(proto))
                return ITEM_USAGE_KEEP;
        }
        else
        {
            Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            Item* offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

            bool isBest = false;
            if (mainHand)
            {
                if (Item* stone = botAI->FindStoneFor(mainHand))
                    isBest = stone->GetEntry() == proto->ItemId;
                if (!isBest)
                    if (Item* oil = botAI->FindOilFor(mainHand))
                        isBest = oil->GetEntry() == proto->ItemId;
            }
            if (!isBest && offHand)
            {
                if (Item* stone = botAI->FindStoneFor(offHand))
                    isBest = stone->GetEntry() == proto->ItemId;
                if (!isBest)
                    if (Item* oil = botAI->FindOilFor(offHand))
                        isBest = oil->GetEntry() == proto->ItemId;
            }

            if (isBest)
                return CurrentStacks(proto) < 1 ? ITEM_USAGE_USE : ITEM_USAGE_KEEP;
        }
    }

    // Need to add something like free bagspace or item value.
    if (proto->SellPrice > 0)
    {
        if (proto->Quality >= ITEM_QUALITY_NORMAL && !isSoulbound && proto->Bonding != BIND_WHEN_PICKED_UP)
            return ITEM_USAGE_AH;
        else
            return ITEM_USAGE_VENDOR;
    }

    return ITEM_USAGE_NONE;
}

std::string ItemUsageValue::BuildItemUsageParam(uint32 itemId, int32 randomPropertyId)
{
    if (randomPropertyId != 0)
        return std::to_string(itemId) + "," + std::to_string(randomPropertyId);

    return std::to_string(itemId);
}

namespace
{
// What one gear evaluation knows about the bot, each part worked out on first use: the traits walk
// the talents and the stat mask builds a whole calculator.
class FitContext
{
public:
    FitContext(Player* bot, StatsWeightCalculator& calculator) : _bot(bot), _calculator(calculator) {}

    Player* Bot() const { return _bot; }

    SpecTraits const& Traits()
    {
        if (!_traits)
            _traits = GetSpecTraits(_bot);
        return *_traits;
    }

    uint32 SmartStatMask()
    {
        if (!_mask)
            _mask = StatsWeightCalculator::BuildSmartStatMask(_bot);
        return *_mask;
    }

    // Listed for this bot at rank 1-6, any slot, at or below its phase cap. A listing is direct
    // evidence the item is itemized for the spec, which the stat checks below can only guess at.
    bool Listed(uint32 itemId)
    {
        if (!sPlayerbotAIConfig.BisGateBypass)
            return false;

        uint8 const rank = _calculator.BisRank(itemId);
        return rank && rank <= BisWire::MAX_RANKS;
    }

private:
    Player* _bot;
    StatsWeightCalculator& _calculator;
    std::optional<SpecTraits> _traits;
    std::optional<uint32> _mask;
};

static bool HasAnyStat(ItemTemplate const* proto, std::initializer_list<ItemModType> mods)
{
    if (!proto)
        return false;

    for (auto const mod : mods)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (proto->ItemStat[i].ItemStatType == mod && proto->ItemStat[i].ItemStatValue != 0)
                return true;
        }
    }

    return false;
}

static bool HasAnyTankAvoidance(ItemTemplate const* proto)
{
    return HasAnyStat(proto, {ITEM_MOD_DEFENSE_SKILL_RATING, ITEM_MOD_DODGE_RATING, ITEM_MOD_PARRY_RATING,
                              ITEM_MOD_BLOCK_RATING});
}

static bool IsRelicForClass(ItemTemplate const* proto, uint8 cls)
{
    if (!proto || proto->InventoryType != INVTYPE_RELIC)
        return false;

    switch (proto->SubClass)
    {
        case ITEM_SUBCLASS_ARMOR_IDOL:
            return cls == CLASS_DRUID;
        case ITEM_SUBCLASS_ARMOR_TOTEM:
            return cls == CLASS_SHAMAN;
        case ITEM_SUBCLASS_ARMOR_LIBRAM:
            return cls == CLASS_PALADIN;
        case ITEM_SUBCLASS_ARMOR_SIGIL:
            return cls == CLASS_DEATH_KNIGHT;
        default:
            return false;
    }
}

static bool IsBodyArmorInvType(uint8 invType)
{
    switch (invType)
    {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
            return true;
        default:
            return false;
    }
}

static bool IsJewelryOrCloak(ItemTemplate const* proto)
{
    if (!proto)
        return false;

    switch (proto->InventoryType)
    {
        case INVTYPE_TRINKET:
        case INVTYPE_FINGER:
        case INVTYPE_NECK:
        case INVTYPE_CLOAK:
            return true;
        default:
            return false;
    }
}

static uint8 PreferredArmorSubclassFor(Player* bot)
{
    if (!bot)
        return ITEM_SUBCLASS_ARMOR_CLOTH;

    uint8 cls = bot->getClass();
    uint32 lvl = bot->GetLevel();

    if (cls == CLASS_MAGE || cls == CLASS_PRIEST || cls == CLASS_WARLOCK)
        return ITEM_SUBCLASS_ARMOR_CLOTH;

    if (cls == CLASS_DRUID || cls == CLASS_ROGUE)
        return ITEM_SUBCLASS_ARMOR_LEATHER;

    if (cls == CLASS_HUNTER || cls == CLASS_SHAMAN)
        return (lvl >= 40u) ? ITEM_SUBCLASS_ARMOR_MAIL : ITEM_SUBCLASS_ARMOR_LEATHER;

    if (cls == CLASS_WARRIOR || cls == CLASS_PALADIN)
        return (lvl >= 40u) ? ITEM_SUBCLASS_ARMOR_PLATE : ITEM_SUBCLASS_ARMOR_MAIL;

    if (cls == CLASS_DEATH_KNIGHT)
        return ITEM_SUBCLASS_ARMOR_PLATE;

    return ITEM_SUBCLASS_ARMOR_CLOTH;
}

static bool IsLowerTierArmorForBot(Player* bot, ItemTemplate const* proto)
{
    if (!bot || !proto)
        return false;
    if (proto->Class != ITEM_CLASS_ARMOR)
        return false;
    if (!IsBodyArmorInvType(proto->InventoryType))
        return false;
    if (proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD || proto->InventoryType == INVTYPE_RELIC ||
        proto->InventoryType == INVTYPE_HOLDABLE)
        return false;

    uint8 preferred = PreferredArmorSubclassFor(bot);
    return proto->SubClass < preferred;
}

// An off-spec ring, neck, trinket or cloak is only worth taking over grey or white gear, or nothing.
static bool IsDesperateJewelryUpgradeForBot(Player* bot, ItemTemplate const* proto)
{
    uint8 jewelrySlots[2];
    uint8 slotsCount = 0;

    switch (proto->InventoryType)
    {
        case INVTYPE_NECK:
            jewelrySlots[0] = EQUIPMENT_SLOT_NECK;
            slotsCount = 1;
            break;
        case INVTYPE_FINGER:
            jewelrySlots[0] = EQUIPMENT_SLOT_FINGER1;
            jewelrySlots[1] = EQUIPMENT_SLOT_FINGER2;
            slotsCount = 2;
            break;
        case INVTYPE_TRINKET:
            jewelrySlots[0] = EQUIPMENT_SLOT_TRINKET1;
            jewelrySlots[1] = EQUIPMENT_SLOT_TRINKET2;
            slotsCount = 2;
            break;
        case INVTYPE_CLOAK:
            jewelrySlots[0] = EQUIPMENT_SLOT_BACK;
            slotsCount = 1;
            break;
        default:
            return false;
    }

    ItemTemplate const* bestProto = nullptr;
    for (uint8 i = 0; i < slotsCount; ++i)
    {
        Item* oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, jewelrySlots[i]);
        if (!oldItem)
            continue;

        ItemTemplate const* oldProto = oldItem->GetTemplate();
        if (oldProto && (!bestProto || oldProto->ItemLevel > bestProto->ItemLevel))
            bestProto = oldProto;
    }

    return !bestProto || bestProto->Quality <= ITEM_QUALITY_NORMAL;
}

static bool IsFallbackNeedReasonableForSpec(FitContext& ctx, ItemTemplate const* proto, bool listed)
{
    if (listed)
        return true;

    SpecTraits const& traits = ctx.Traits();
    uint32 const profile = ctx.SmartStatMask();
    if (profile == SMARTSTAT_NONE)
        return true;

    ItemStatProfile const stats = BuildItemStatProfile(proto);
    bool const hasAnyStat = stats.hasINT || stats.hasSPI || stats.hasMP5 || stats.hasSP || stats.hasSTR ||
        stats.hasAGI || stats.hasSTA || stats.hasAP || stats.hasARP || stats.hasEXP || stats.hasHIT ||
        stats.hasHASTE || stats.hasCRIT || stats.hasDef || stats.hasAvoid || stats.hasBlockValue;

    if (!hasAnyStat)
        return true;

    // Hard role-negative filters (mirror IsPrimaryForSpec): an off-spec item that merely shares one
    // stat with the bot's mask must still not be NEEDed when it is clearly itemized for another role.
    if (!traits.isTank && HasAnyTankAvoidance(proto))
        return false;
    if (traits.isHealer && stats.hasHIT)
        return false;
    if (traits.cls == CLASS_DEATH_KNIGHT && stats.hasAGI)
        return false;
    if ((traits.isHunter || traits.isRogue || traits.isEnhSham) && stats.hasSTR)
        return false;

    if (traits.isTank && !stats.hasSTA && !stats.hasDef && !stats.hasAvoid && !stats.hasBlockValue)
        return false;
    if ((profile & SMARTSTAT_HIT) && stats.hasHIT)
        return true;
    if ((profile & SMARTSTAT_SPELL_POWER) && stats.hasSP)
        return true;
    if ((profile & SMARTSTAT_HASTE) && stats.hasHASTE)
        return true;
    if ((profile & SMARTSTAT_CRIT) && stats.hasCRIT)
        return true;
    if ((profile & SMARTSTAT_INTELLECT) && stats.hasINT)
        return true;
    if ((profile & SMARTSTAT_SPIRIT) && stats.hasSPI)
        return true;
    if ((profile & SMARTSTAT_EXPERTISE) && stats.hasEXP)
        return true;
    if ((profile & SMARTSTAT_ATTACK_POWER) && stats.hasAP)
        return true;
    if ((profile & SMARTSTAT_ARMOR_PEN) && stats.hasARP)
        return true;
    if ((profile & SMARTSTAT_AGILITY) && stats.hasAGI)
        return true;
    if ((profile & SMARTSTAT_STAMINA) && stats.hasSTA)
        return true;
    if ((profile & SMARTSTAT_AVOIDANCE) && (stats.hasAvoid || stats.hasDef))
        return true;
    if ((profile & SMARTSTAT_MP5) && stats.hasMP5)
        return true;
    if ((profile & SMARTSTAT_STRENGTH) && stats.hasSTR)
        return true;

    return false;
}
} // namespace

SpecTraits GetSpecTraits(Player* bot)
{
    SpecTraits t;
    if (!bot)
        return t;
    t.cls = bot->getClass();
    t.spec = AiFactory::GetPlayerSpecName(bot);

    auto specIs = [&](char const* s) { return t.spec == s; };

    const bool holyPal = (t.cls == CLASS_PALADIN && specIs("holy"));
    const bool protPal = (t.cls == CLASS_PALADIN && (specIs("prot") || specIs("protection")));
    t.isProtPal = protPal;
    t.isRetPal = (t.cls == CLASS_PALADIN && !holyPal && !protPal);
    t.isWarrior = (t.cls == CLASS_WARRIOR);
    t.isWarProt = t.isWarrior && (specIs("prot") || specIs("protection"));
    t.isHunter = (t.cls == CLASS_HUNTER);
    t.isRogue = (t.cls == CLASS_ROGUE);
    t.isEnhSham = (t.cls == CLASS_SHAMAN && (specIs("enhance") || specIs("enhancement")));

    // Role flags come from the bot's real role detection, not spec-name strings. Talent-tab names like
    // "elem", "blooddps", "frostdps" or "feraldps" don't encode caster-vs-physical or tank-vs-dps, so
    // string matching left elemental shamans as physical and blood/bear tanks as non-tanks. IsTank keys
    // off the active tank strategy, IsCaster off the ranged strategy - both handle those correctly.
    t.isHealer = PlayerbotAI::IsHeal(bot);
    t.isTank = PlayerbotAI::IsTank(bot);
    t.isCaster = PlayerbotAI::IsCaster(bot);
    t.isPhysical = !t.isCaster;
    return t;
}

namespace
{
    static void UpdateItemStatProfileFromStats(ItemTemplate const* proto, ItemStatProfile& s)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (proto->ItemStat[i].ItemStatValue == 0)
                continue;

            switch (proto->ItemStat[i].ItemStatType)
            {
                case ITEM_MOD_INTELLECT:
                    s.hasINT = true;
                    break;
                case ITEM_MOD_SPIRIT:
                    s.hasSPI = true;
                    break;
                case ITEM_MOD_SPELL_POWER:
                    s.hasSP = true;
                    break;
                case ITEM_MOD_SPELL_HEALING_DONE:
                    s.hasSP = true;
                    break;
                // The generic mods grant all three schools, so they say nothing about the item's role.
                // The school-specific ones do, and hasPhysicalRating is what keeps a plate DPS piece
                // whose only ratings are melee ones from reading as caster gear.
                case ITEM_MOD_HIT_RATING:
                    s.hasHIT = true;
                    break;
                case ITEM_MOD_CRIT_RATING:
                    s.hasCRIT = true;
                    break;
                case ITEM_MOD_HASTE_RATING:
                    s.hasHASTE = true;
                    break;
                case ITEM_MOD_HIT_SPELL_RATING:
                    s.hasHIT = true;
                    break;
                case ITEM_MOD_CRIT_SPELL_RATING:
                    s.hasCRIT = true;
                    break;
                case ITEM_MOD_HASTE_SPELL_RATING:
                    s.hasHASTE = true;
                    break;
                case ITEM_MOD_HIT_MELEE_RATING:
                case ITEM_MOD_HIT_RANGED_RATING:
                    s.hasHIT = true;
                    s.hasPhysicalRating = true;
                    break;
                case ITEM_MOD_CRIT_MELEE_RATING:
                case ITEM_MOD_CRIT_RANGED_RATING:
                    s.hasCRIT = true;
                    s.hasPhysicalRating = true;
                    break;
                case ITEM_MOD_HASTE_MELEE_RATING:
                case ITEM_MOD_HASTE_RANGED_RATING:
                    s.hasHASTE = true;
                    s.hasPhysicalRating = true;
                    break;
                case ITEM_MOD_EXPERTISE_RATING:
                    s.hasEXP = true;
                    s.hasPhysicalRating = true;
                    break;
                case ITEM_MOD_MANA_REGENERATION:
                    s.hasMP5 = true;
                    break;
                case ITEM_MOD_STRENGTH:
                    s.hasSTR = true;
                    break;
                case ITEM_MOD_AGILITY:
                    s.hasAGI = true;
                    break;
                case ITEM_MOD_ATTACK_POWER:
                    s.hasAP = true;
                    break;
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    s.hasARP = true;
                    break;
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                    s.hasDef = true;
                    break;
                case ITEM_MOD_DODGE_RATING:
                case ITEM_MOD_PARRY_RATING:
                case ITEM_MOD_BLOCK_RATING:
                    s.hasAvoid = true;
                    break;
                case ITEM_MOD_BLOCK_VALUE:
                    s.hasBlockValue = true;
                    break;
                case ITEM_MOD_SPELL_PENETRATION:
                    s.hasSpellPen = true;
                    break;
                default:
                    break;
            }
        }
    }

    // An item spell is often just a proc holder whose TriggerSpell carries the real stats
    // (Grim Toll 60436 -> 60437 armor pen, Illustration of the Dragon Soul 60485 -> 60486 spell
    // power). One level of indirection covers every WotLK item; the score engine follows the same
    // chain in StatsCollector::HandleApplyAura, and the two readers disagreeing is what let
    // proc-only gear read as the wrong role.
    static constexpr uint8 MAX_ITEM_SPELL_SCAN_DEPTH = 1;

    static void UpdateItemStatProfileFromSpell(uint32 spellId, ItemStatProfile& s, uint8 depth)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            return;

        for (int eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
        {
            SpellEffectInfo const& effectInfo = spellInfo->Effects[eff];
            if (effectInfo.Effect != SPELL_EFFECT_APPLY_AURA)
                continue;

            if (effectInfo.ApplyAuraName == SPELL_AURA_PROC_TRIGGER_SPELL ||
                effectInfo.ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL)
            {
                if (depth < MAX_ITEM_SPELL_SCAN_DEPTH && effectInfo.TriggerSpell &&
                    effectInfo.TriggerSpell != spellId)
                    UpdateItemStatProfileFromSpell(effectInfo.TriggerSpell, s, static_cast<uint8>(depth + 1));

                continue;
            }

            if (effectInfo.ApplyAuraName == SPELL_AURA_MOD_HEALING_DONE)
            {
                s.hasSP = true;
                break;
            }

            if (effectInfo.ApplyAuraName == SPELL_AURA_MOD_DAMAGE_DONE &&
                (effectInfo.MiscValue & SPELL_SCHOOL_MASK_MAGIC) == SPELL_SCHOOL_MASK_MAGIC)
            {
                s.hasSP = true;
                break;
            }

            if (effectInfo.ApplyAuraName == SPELL_AURA_MOD_RESISTANCE &&
                (effectInfo.MiscValue & SPELL_SCHOOL_MASK_NORMAL) == SPELL_SCHOOL_MASK_NORMAL)
            {
                s.hasAvoid = true;
                break;
            }

            // MOD_RATING MiscValue is a combat-rating bitmask (1 << CR_x), not a raw rating index.
            if (effectInfo.ApplyAuraName == SPELL_AURA_MOD_RATING)
            {
                int32 const ratingMask = effectInfo.MiscValue;

                if (ratingMask & ((1 << CR_DODGE) | (1 << CR_PARRY) | (1 << CR_BLOCK)))
                {
                    s.hasAvoid = true;
                    break;
                }

                // The generic item mods grant all three schools at once, so a caster proc's mask carries
                // the melee and ranged bits too. Only a mask with no spell-side bit at all is evidence
                // the item is itemized for a physical role.
                bool const spellSide =
                    (ratingMask & ((1 << CR_HIT_SPELL) | (1 << CR_CRIT_SPELL) | (1 << CR_HASTE_SPELL))) != 0;

                bool matched = false;

                if (ratingMask & (1 << CR_ARMOR_PENETRATION))
                {
                    s.hasARP = true;
                    matched = true;
                }

                if (ratingMask & (1 << CR_EXPERTISE))
                {
                    s.hasEXP = true;
                    s.hasPhysicalRating = true;
                    matched = true;
                }

                if (ratingMask & ((1 << CR_HIT_MELEE) | (1 << CR_HIT_RANGED)))
                {
                    s.hasHIT = true;
                    s.hasPhysicalRating = s.hasPhysicalRating || !spellSide;
                    matched = true;
                }

                if (ratingMask & ((1 << CR_CRIT_MELEE) | (1 << CR_CRIT_RANGED)))
                {
                    s.hasCRIT = true;
                    s.hasPhysicalRating = s.hasPhysicalRating || !spellSide;
                    matched = true;
                }

                if (ratingMask & ((1 << CR_HASTE_MELEE) | (1 << CR_HASTE_RANGED)))
                {
                    s.hasHASTE = true;
                    s.hasPhysicalRating = s.hasPhysicalRating || !spellSide;
                    matched = true;
                }

                if (spellSide)
                {
                    s.hasHIT = s.hasHIT || (ratingMask & (1 << CR_HIT_SPELL));
                    s.hasCRIT = s.hasCRIT || (ratingMask & (1 << CR_CRIT_SPELL));
                    s.hasHASTE = s.hasHASTE || (ratingMask & (1 << CR_HASTE_SPELL));
                    matched = true;
                }

                if (matched)
                    break;
            }

            // The STR/AGI/INT proc trinkets (Death's Choice, Darkmoon Card: Greatness,
            // Blood of the Old God) deliver their whole payload this way, so without it they read as
            // statless and land off-spec for the role they are itemized for.
            if (effectInfo.ApplyAuraName == SPELL_AURA_MOD_STAT)
            {
                bool const allStats = effectInfo.MiscValue == -1;

                s.hasSTR = s.hasSTR || allStats || effectInfo.MiscValue == STAT_STRENGTH;
                s.hasAGI = s.hasAGI || allStats || effectInfo.MiscValue == STAT_AGILITY;
                s.hasSTA = s.hasSTA || allStats || effectInfo.MiscValue == STAT_STAMINA;
                s.hasINT = s.hasINT || allStats || effectInfo.MiscValue == STAT_INTELLECT;
                s.hasSPI = s.hasSPI || allStats || effectInfo.MiscValue == STAT_SPIRIT;
                break;
            }

            if (effectInfo.ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER ||
                effectInfo.ApplyAuraName == SPELL_AURA_MOD_RANGED_ATTACK_POWER)
            {
                s.hasAP = true;
                break;
            }
        }
    }

    static void UpdateItemStatProfileFromSpells(ItemTemplate const* proto, ItemStatProfile& s)
    {
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            auto const& spell = proto->Spells[i];
            if (!spell.SpellId ||
                (spell.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP && spell.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE))
                continue;

            UpdateItemStatProfileFromSpell(spell.SpellId, s, 0);

            // Preserve original behavior: stop scanning item spells as soon as SP is detected.
            if (s.hasSP)
                break;
        }
    }
} // namespace

ItemStatProfile BuildItemStatProfile(ItemTemplate const* proto)
{
    ItemStatProfile s;
    if (!proto)
        return s;

    UpdateItemStatProfileFromStats(proto, s);

    s.hasAvoid = s.hasAvoid || HasAnyTankAvoidance(proto);

    UpdateItemStatProfileFromSpells(proto, s);

    return s;
}

namespace
{
static bool IsPrimaryForSpec(FitContext& ctx, ItemTemplate const* proto, bool listed)
{
    Player* bot = ctx.Bot();

    // A lower armor class is never primary, listed or not: it falls to the fallback check and pays
    // the cross-armor margin unless listed.
    if (listed && !IsLowerTierArmorForBot(bot, proto))
        return true;

    SpecTraits const& traits = ctx.Traits();
    ItemStatProfile const stats = BuildItemStatProfile(proto);
    bool const hasPhysical =
        stats.hasSTR || stats.hasAGI || stats.hasAP || stats.hasARP || stats.hasPhysicalRating;
    bool const hasCasterPrimary = stats.hasINT || stats.hasSP || stats.hasMP5;
    bool const hasCasterRatings = stats.hasHIT || stats.hasCRIT || stats.hasHASTE;
    bool const hasCaster = hasCasterPrimary || (hasCasterRatings && !hasPhysical);

    // Caster offense ratings should only be considered in the caster context.
    bool const hasCasterOffense = hasCaster && hasCasterRatings;

    if (proto->InventoryType == INVTYPE_RELIC)
    {
        if (!IsRelicForClass(proto, traits.cls))
            return false;

        return IsFallbackNeedReasonableForSpec(ctx, proto, listed);
    }

    if (proto->Class == ITEM_CLASS_WEAPON)
    {
        if ((traits.isHunter || traits.isRogue || traits.isEnhSham) && stats.hasSTR)
            return false;

        if ((traits.isProtPal || traits.isWarProt) && proto->InventoryType == INVTYPE_2HWEAPON)
            return false;

        if (traits.isTank && hasCaster)
            return false;

        if (!traits.isCaster && hasCaster && !hasPhysical)
            return false;

        if (traits.isCaster && hasPhysical && !hasCaster)
            return false;

        if (traits.isCaster)
        {
            bool const isCasterWeapon =
                proto->SubClass == ITEM_SUBCLASS_WEAPON_STAFF || proto->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER ||
                proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD || proto->SubClass == ITEM_SUBCLASS_WEAPON_MACE ||
                proto->SubClass == ITEM_SUBCLASS_WEAPON_WAND;
             return isCasterWeapon && (hasCaster || (hasCasterOffense && !hasPhysical));
        }

        return hasPhysical;
    }

    if (proto->Class != ITEM_CLASS_ARMOR)
        return true;

    if (traits.cls == CLASS_DEATH_KNIGHT && stats.hasAGI)
        return false;

    if ((traits.isHunter || traits.isRogue || traits.isEnhSham) && stats.hasSTR)
        return false;

    if (IsJewelryOrCloak(proto))
    {
        if (traits.isCaster != hasCaster)
            return false;
        return IsFallbackNeedReasonableForSpec(ctx, proto, listed);
    }

    if (IsLowerTierArmorForBot(bot, proto))
        return false;

    if (!traits.isTank && HasAnyTankAvoidance(proto))
        return false;

    if (traits.isTank && HasAnyTankAvoidance(proto))
        return true;

    if (traits.isHealer && stats.hasHIT)
        return false;

    if (traits.isCaster)
        return hasCaster || (hasCasterOffense && !hasPhysical);

    if (traits.isPhysical)
        return hasPhysical && !hasCaster;

    return true;
}

// Rejects items whose itemization belongs to a role the bot doesn't fill, even when the score engine
// values them off a shared stat.
static bool IsRoleItemizationMismatch(SpecTraits const& traits, ItemTemplate const* proto)
{
    ItemStatProfile const stats = BuildItemStatProfile(proto);

    // Spell penetration only itemizes caster-DPS / PvP gear; a healer never wants it.
    if (traits.isHealer && stats.hasSpellPen)
        return true;

    // A pure physical role shouldn't take a weapon itemized for casters, even if it also carries a
    // physical stat the score engine credits (mixed weapons slip past IsPrimaryForSpec's !hasPhysical gate).
    if (!traits.isCaster && !traits.isHealer && proto->Class == ITEM_CLASS_WEAPON)
        return stats.hasINT || stats.hasSP || stats.hasMP5;

    return false;
}

static bool IsBroken(Item const* item)
{
    return item && item->GetUInt32Value(ITEM_FIELD_DURABILITY) == 0 &&
           item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0;
}

// A worn item that fails the checks a new one needs to be worn at all is replaced on any gain.
// The spec and jewelry checks don't count here, or every off-spec piece would lose its margin.
static bool IsWornMisfit(FitContext& ctx, ItemTemplate const* proto, float score)
{
    if (score <= 0.0f)
        return true;

    if (proto->Class == ITEM_CLASS_WEAPON && !sRandomItemMgr.CanEquipWeapon(proto, ctx.Bot()->getClass()))
        return true;

    return !ctx.Listed(proto->ItemId) && IsRoleItemizationMismatch(ctx.Traits(), proto);
}

static bool IsTitanGripType(ItemTemplate const* proto)
{
    return proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE2 || proto->SubClass == ITEM_SUBCLASS_WEAPON_MACE2 ||
           proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD2;
}

static bool IsWeaponHandItem(ItemTemplate const* proto)
{
    switch (proto->InventoryType)
    {
        case INVTYPE_WEAPON:
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
            return true;
        default:
            return false;
    }
}

static GearPlacement::Hand HandOf(ItemTemplate const* proto)
{
    GearPlacement::Hand hand;
    if (!proto)
        return hand;

    switch (proto->InventoryType)
    {
        case INVTYPE_SHIELD:
        case INVTYPE_HOLDABLE:
            hand.kind = GearPlacement::Kind::OffHandItem;
            return hand;
        case INVTYPE_WEAPON:
            hand.kind = GearPlacement::Kind::OneHand;
            break;
        case INVTYPE_WEAPONMAINHAND:
            hand.kind = GearPlacement::Kind::MainHandOnly;
            break;
        case INVTYPE_WEAPONOFFHAND:
            hand.kind = GearPlacement::Kind::OffHandOnly;
            break;
        case INVTYPE_2HWEAPON:
            hand.kind = GearPlacement::Kind::TwoHand;
            break;
        default:
            return hand;
    }

    hand.titanGripType = IsTitanGripType(proto);
    hand.mainHandOnly = proto->InventoryType == INVTYPE_WEAPON && (proto->SubClass == ITEM_SUBCLASS_WEAPON_POLEARM ||
                                                                   proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE);
    return hand;
}

static GearPlacement::Hand HandOf(Item const* item)
{
    return HandOf(item ? item->GetTemplate() : nullptr);
}

// Equipment slots a non-weapon can go to, whatever is worn there.
static uint8 SlotsFor(Player* bot, ItemTemplate const* proto, std::array<uint8, 2>& slots)
{
    auto one = [&slots](uint8 slot)
    {
        slots[0] = slot;
        return uint8(1);
    };
    auto relic = [&](Classes cls)
    { return bot->IsClass(cls, CLASS_CONTEXT_EQUIP_RELIC) ? one(EQUIPMENT_SLOT_RANGED) : uint8(0); };

    switch (proto->InventoryType)
    {
        case INVTYPE_HEAD:
            return one(EQUIPMENT_SLOT_HEAD);
        case INVTYPE_NECK:
            return one(EQUIPMENT_SLOT_NECK);
        case INVTYPE_SHOULDERS:
            return one(EQUIPMENT_SLOT_SHOULDERS);
        case INVTYPE_BODY:
            return one(EQUIPMENT_SLOT_BODY);
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
            return one(EQUIPMENT_SLOT_CHEST);
        case INVTYPE_WAIST:
            return one(EQUIPMENT_SLOT_WAIST);
        case INVTYPE_LEGS:
            return one(EQUIPMENT_SLOT_LEGS);
        case INVTYPE_FEET:
            return one(EQUIPMENT_SLOT_FEET);
        case INVTYPE_WRISTS:
            return one(EQUIPMENT_SLOT_WRISTS);
        case INVTYPE_HANDS:
            return one(EQUIPMENT_SLOT_HANDS);
        case INVTYPE_CLOAK:
            return one(EQUIPMENT_SLOT_BACK);
        case INVTYPE_TABARD:
            return one(EQUIPMENT_SLOT_TABARD);
        case INVTYPE_SHIELD:
        case INVTYPE_HOLDABLE:
            return one(EQUIPMENT_SLOT_OFFHAND);
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
            return one(EQUIPMENT_SLOT_RANGED);
        case INVTYPE_FINGER:
            slots = {EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2};
            return 2;
        case INVTYPE_TRINKET:
            slots = {EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2};
            return 2;
        case INVTYPE_RELIC:
            switch (proto->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_LIBRAM:
                    return relic(CLASS_PALADIN);
                case ITEM_SUBCLASS_ARMOR_IDOL:
                    return relic(CLASS_DRUID);
                case ITEM_SUBCLASS_ARMOR_TOTEM:
                    return relic(CLASS_SHAMAN);
                case ITEM_SUBCLASS_ARMOR_MISC:
                    return relic(CLASS_WARLOCK);
                case ITEM_SUBCLASS_ARMOR_SIGIL:
                    return relic(CLASS_DEATH_KNIGHT);
                default:
                    return 0;
            }
        default:
            return 0;
    }
}

struct BagSlot
{
    uint8 slot = NULL_SLOT;
    uint32 size = 0;
};

// Where a new bag goes: an empty bag slot, else the smallest plain bag. Quivers, ammo pouches and
// profession bags are never swapped out.
static BagSlot SmallestBagSlot(Player* bot)
{
    BagSlot best;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!item)
            return {bag, 0};

        ItemTemplate const* proto = item->GetTemplate();
        if (proto->Class != ITEM_CLASS_CONTAINER || proto->SubClass != ITEM_SUBCLASS_CONTAINER)
            continue;

        uint32 const size = static_cast<Bag const*>(item)->GetBagSize();
        if (best.slot == NULL_SLOT || size < best.size)
            best = {bag, size};
    }

    return best;
}

static bool HasProficiency(Player* bot, ItemTemplate const* proto)
{
    uint32 const skill = proto->GetSkill();
    if (!skill || bot->GetSkillValue(skill))
        return true;

    // heirloom plate and mail can be worn before the skill is learned
    if (proto->Quality == ITEM_QUALITY_HEIRLOOM && proto->Class == ITEM_CLASS_ARMOR)
    {
        if (bot->IsClass(CLASS_PALADIN, CLASS_CONTEXT_EQUIP_ARMOR_CLASS) ||
            bot->IsClass(CLASS_WARRIOR, CLASS_CONTEXT_EQUIP_ARMOR_CLASS))
            return skill == SKILL_PLATE_MAIL;

        if (bot->IsClass(CLASS_HUNTER, CLASS_CONTEXT_EQUIP_ARMOR_CLASS) ||
            bot->IsClass(CLASS_SHAMAN, CLASS_CONTEXT_EQUIP_ARMOR_CLASS))
            return skill == SKILL_MAIL;
    }

    return false;
}

// Template-level refusals: anything here means no slot ever takes the item.
static char const* EquipRefusal(Player* bot, ItemTemplate const* proto, Item const* candidate)
{
    if (proto->InventoryType == INVTYPE_NON_EQUIP)
        return "not equipment";

    if (bot->CanUseItem(proto) != EQUIP_ERR_OK)
        return "can't use";

    // the template CanUseItem skips these two, only the Item overload checks them
    if (!HasProficiency(bot, proto))
        return "no proficiency";

    if (proto->RequiredReputationFaction &&
        uint32(bot->GetReputationRank(proto->RequiredReputationFaction)) < proto->RequiredReputationRank)
        return "reputation too low";

    if (proto->Class == ITEM_CLASS_WEAPON && proto->SubClass == ITEM_SUBCLASS_WEAPON_MISC)
        return "misc weapon";

    // FindEquipSlot has no slot for them, so they were never equipped
    if (proto->Class == ITEM_CLASS_QUIVER)
        return "quiver";

    // Titan's Grip takes no staff or polearm in either hand
    if (bot->CanTitanGrip() && proto->InventoryType == INVTYPE_2HWEAPON && !IsTitanGripType(proto))
        return "not a Titan's Grip weapon";

    if (proto->ScalingStatDistribution)
    {
        ScalingStatDistributionEntry const* ssd = sScalingStatDistributionStore.LookupEntry(proto->ScalingStatDistribution);
        if (ssd && ssd->MaxLevel < DEFAULT_MAX_LEVEL && ssd->MaxLevel < bot->GetLevel())
            return "level too high";
    }

    if (candidate && (candidate->m_lootGenerated || candidate->IsBindedNotWith(bot)))
        return "not ours";

    return nullptr;
}

// Worn items counted by slot: GetItemCount counts them whatever inBankAlso says.
static uint32 WornCopies(Player* bot, uint32 itemId)
{
    uint32 count = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (item->GetEntry() == itemId)
                ++count;

    return count;
}
} // namespace

GearChoice EvaluateGear(PlayerbotAI* botAI, ItemTemplate const* proto, int32 randomPropertyId, Item const* candidate,
                        bool force)
{
    GearChoice choice;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !proto)
        return choice;

    if (char const* refusal = EquipRefusal(bot, proto, candidate))
    {
        choice.reason = refusal;
        return choice;
    }

    if (proto->Class == ITEM_CLASS_CONTAINER)
    {
        if (proto->SubClass != ITEM_SUBCLASS_CONTAINER)
        {
            choice.reason = "not a plain bag";
            return choice;
        }

        BagSlot const bag = SmallestBagSlot(bot);
        choice.slot = bag.slot;
        choice.newScore = float(proto->ContainerSlots);
        choice.oldScore = float(bag.size);
        if (bag.slot == NULL_SLOT)
            choice.reason = "no bag slot";
        else if (force || bag.size < proto->ContainerSlots)
        {
            choice.usage = ITEM_USAGE_EQUIP;
            choice.reason = force ? "forced" : "bigger bag";
        }
        else
            choice.reason = "no bigger";

        return choice;
    }

    StatsWeightCalculator calculator(bot);
    calculator.SetItemSetBonus(sPlayerbotAIConfig.ItemSetUseForUpgrades);
    calculator.SetOverflowPenalty(false);
    calculator.SetBisBonus(true);
    if (sRandomPlayerbotMgr.IsSpecPvp(bot->GetGUID().GetCounter(), bot->getClass()))
        calculator.SetPvpSpec(true);

    FitContext ctx(bot, calculator);
    bool const listed = ctx.Listed(proto->ItemId);
    bool const newBroken = IsBroken(candidate);
    bool const newLower = IsLowerTierArmorForBot(bot, proto);

    char const* unfit = nullptr;
    if (!force)
    {
        if (calculator.CalculateItem(proto->ItemId, randomPropertyId) <= 0.0f)
            unfit = "no score";
        else if (!listed && IsRoleItemizationMismatch(ctx.Traits(), proto))
            unfit = "role mismatch";
        else if (proto->Class == ITEM_CLASS_WEAPON && !sRandomItemMgr.CanEquipWeapon(proto, bot->getClass()))
            unfit = "weapon skill";
        else if (sPlayerbotAIConfig.SmartNeedBySpec && !IsPrimaryForSpec(ctx, proto, listed))
        {
            if (!IsFallbackNeedReasonableForSpec(ctx, proto, listed))
                unfit = "off spec";
            else if (IsJewelryOrCloak(proto) && !IsDesperateJewelryUpgradeForBot(bot, proto))
                unfit = "off-spec jewelry";
        }
    }

    float const upgradeBar = sPlayerbotAIConfig.EquipUpgradeThreshold;
    float lowerArmorBar = upgradeBar;
    if (newLower && !listed)
    {
        bool const leveling = bot->GetLevel() < static_cast<uint32>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
        if (!(ctx.Traits().isHealer && leveling))
            lowerArmorBar = sPlayerbotAIConfig.CrossArmorExtraMargin;
    }

    // "item usage" is recalculated on every read, so no score is taken twice per call
    struct Scored
    {
        uint32 item;
        int32 randomPropertyId;
        uint8 slot;
        Item const* leaving;
        float score;
    };
    std::vector<Scored> scored;
    auto score = [&](ItemTemplate const* item, int32 randomProperty, uint8 slot, Item const* leaving)
    {
        for (Scored const& s : scored)
            if (s.item == item->ItemId && s.randomPropertyId == randomProperty && s.slot == slot && s.leaving == leaving)
                return s.score;

        // both sides of one comparison are scored with the leaving item treated as removed
        calculator.SetReplacedItem(leaving);
        calculator.SetBisContest(proto->ItemId, leaving ? leaving->GetEntry() : 0);
        float const value = calculator.CalculateItem(item->ItemId, randomProperty, slot);
        scored.push_back({item->ItemId, randomProperty, slot, leaving, value});
        return value;
    };
    auto newScore = [&](uint8 slot, Item const* leaving) { return score(proto, randomPropertyId, slot, leaving); };
    auto wornScore = [&](Item const* worn, uint8 slot, Item const* leaving)
    { return score(worn->GetTemplate(), worn->GetItemRandomPropertyId(), slot, leaving); };
    // the bar for replacing what's worn in a slot
    auto barFor = [&](Item const* worn, float scoreThere)
    {
        ItemTemplate const* wornProto = worn->GetTemplate();
        if (IsWornMisfit(ctx, wornProto, scoreThere))
            return 1.0f;
        if (newLower && !IsLowerTierArmorForBot(bot, wornProto))
            return lowerArmorBar;
        return upgradeBar;
    };
    auto change = [&](float newValue, Item const* worn, uint8 slot, Item const* leaving)
    {
        GearPlacement::Change c;
        c.newScore = newValue;
        if (worn)
        {
            c.oldScore = wornScore(worn, slot, leaving);
            c.threshold = barFor(worn, c.oldScore);
        }
        return c;
    };
    auto sameItem = [proto](Item const* worn) { return worn && worn->GetEntry() == proto->ItemId; };
    auto uniqueAllows = [&](uint8 slot) { return bot->CanEquipUniqueItem(proto, slot) == EQUIP_ERR_OK; };

    struct Target
    {
        uint8 slot;
        uint8 incumbentTo;
        Item const* displaced;
    };
    std::vector<GearPlacement::Option> options;
    std::vector<Target> targets;

    if (IsWeaponHandItem(proto))
    {
        Item const* main = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        Item const* off = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
        GearPlacement::Hand const item = HandOf(proto);
        GearPlacement::Hand const mainHand = HandOf(main);
        GearPlacement::Hand const offHand = HandOf(off);

        GearPlacement::Bot hands;
        hands.canDualWield = bot->CanDualWield();
        hands.canTitanGrip = bot->CanTitanGrip();
        hands.keepsShield = off && off->GetTemplate()->Class == ITEM_CLASS_ARMOR &&
                            off->GetTemplate()->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD && ctx.Traits().isTank;

        using GearPlacement::Layout;
        for (uint8 i = 0; i < uint8(Layout::Count); ++i)
        {
            Layout const layout = Layout(i);
            GearPlacement::Option option;
            Target target{NULL_SLOT, NULL_SLOT, nullptr};
            option.legal = GearPlacement::LayoutLegal(layout, item, mainHand, offHand, hands);

            switch (layout)
            {
                // a lone 2H counts against the main hand only: its type penalty stands in for the off hand
                case Layout::NewToMain:
                    target = {EQUIPMENT_SLOT_MAINHAND, NULL_SLOT, main};
                    option.legal = option.legal && !sameItem(main) && uniqueAllows(EQUIPMENT_SLOT_MAINHAND);
                    if (option.legal)
                        option.Add(change(newScore(EQUIPMENT_SLOT_MAINHAND, main), main, EQUIPMENT_SLOT_MAINHAND, main));
                    option.replacesBroken = IsBroken(main) && !newBroken;
                    break;
                case Layout::NewToMainOldToOff:
                    target = {EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND, off};
                    option.legal = option.legal && !sameItem(main) && uniqueAllows(EQUIPMENT_SLOT_MAINHAND);
                    if (option.legal)
                    {
                        option.Add(change(newScore(EQUIPMENT_SLOT_MAINHAND, off), main, EQUIPMENT_SLOT_MAINHAND, off));
                        option.Add(change(wornScore(main, EQUIPMENT_SLOT_OFFHAND, off), off, EQUIPMENT_SLOT_OFFHAND, off));
                    }
                    break;
                case Layout::NewToOff:
                    target = {EQUIPMENT_SLOT_OFFHAND, NULL_SLOT, off};
                    option.legal = option.legal && !sameItem(off) && uniqueAllows(EQUIPMENT_SLOT_OFFHAND);
                    if (option.legal)
                        option.Add(change(newScore(EQUIPMENT_SLOT_OFFHAND, off), off, EQUIPMENT_SLOT_OFFHAND, off));
                    option.replacesBroken = IsBroken(off) && !newBroken;
                    break;
                case Layout::NewToOffOldToMain:
                    target = {EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_MAINHAND, main};
                    option.legal = option.legal && !sameItem(off) && uniqueAllows(EQUIPMENT_SLOT_OFFHAND);
                    if (option.legal)
                    {
                        option.Add(change(newScore(EQUIPMENT_SLOT_OFFHAND, main), off, EQUIPMENT_SLOT_OFFHAND, main));
                        option.Add(change(wornScore(off, EQUIPMENT_SLOT_MAINHAND, main), main, EQUIPMENT_SLOT_MAINHAND, main));
                    }
                    break;
                default:
                    break;
            }

            options.push_back(option);
            targets.push_back(target);
        }
    }
    else
    {
        std::array<uint8, 2> slots{};
        uint8 const count = SlotsFor(bot, proto, slots);
        Item const* worn[2] = {nullptr, nullptr};
        for (uint8 i = 0; i < count; ++i)
            worn[i] = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slots[i]);

        // with both rings or trinkets in place, the one the sim ranks lower goes first
        int8 wornOrder = 0;
        if (count == 2 && worn[0] && worn[1])
            wornOrder = calculator.CompareBisRank(worn[0]->GetTemplate(), worn[1]->GetTemplate(), slots[0]);

        for (uint8 i = 0; i < count; ++i)
        {
            uint8 const slot = slots[i];
            Item const* displaced = worn[i];
            GearPlacement::Option option;
            option.legal = !sameItem(displaced) && uniqueAllows(slot);
            // the off hand is closed while a 2H the bot can't Titan's Grip is worn
            if (slot == EQUIPMENT_SLOT_OFFHAND && bot->IsTwoHandUsed())
                option.legal = false;

            if (option.legal)
                option.Add(change(newScore(slot, displaced), displaced, slot, displaced));
            option.replacesBroken = IsBroken(displaced) && !newBroken;
            if (displaced)
                option.rankOrder = calculator.CompareBisRank(proto, displaced->GetTemplate(), slot);

            if (!displaced)
                option.preference = 2;
            else if ((i == 0 && wornOrder < 0) || (i == 1 && wornOrder > 0))
                option.preference = 1;

            options.push_back(option);
            targets.push_back({slot, NULL_SLOT, displaced});
        }
    }

    calculator.SetReplacedItem(nullptr);
    calculator.SetBisContest(0, 0);

    auto fill = [&](std::size_t index, ItemUsage usage, char const* reason)
    {
        choice.usage = usage;
        choice.slot = targets[index].slot;
        choice.incumbentTo = targets[index].incumbentTo;
        choice.newScore = options[index].NewSum();
        choice.oldScore = options[index].Bar();
        choice.reason = reason;
        return choice;
    };

    bool const anyLegal = std::any_of(options.begin(), options.end(), [](auto const& o) { return o.legal; });
    if (!anyLegal)
    {
        choice.reason = "no free slot";
        return choice;
    }

    // An unfit item only fills an empty slot, which displaces nothing, or replaces a broken item.
    if (unfit)
    {
        for (std::size_t i = 0; i < options.size(); ++i)
            if (options[i].legal && !targets[i].displaced && targets[i].incumbentTo == NULL_SLOT)
                return fill(i, ITEM_USAGE_BAD_EQUIP, unfit);

        for (std::size_t i = 0; i < options.size(); ++i)
            if (options[i].legal && options[i].replacesBroken)
                return fill(i, ITEM_USAGE_EQUIP, "replaces broken");

        choice.reason = unfit;
        return choice;
    }

    int const pick = GearPlacement::Choose(options, force);
    if (pick < 0)
    {
        choice.reason = "no gain";
        return choice;
    }

    GearPlacement::Option const& option = options[pick];
    Item const* displaced = targets[pick].displaced;
    if (!force && displaced && newBroken && !IsBroken(displaced))
        return fill(pick, ITEM_USAGE_BROKEN_EQUIP, "broken");

    char const* reason = force                    ? "forced"
                         : option.replacesBroken ? "replaces broken"
                         : option.rankOrder > 0  ? "sim rank"
                         : !displaced            ? "empty slot"
                                                 : "upgrade";

    bool const replace = proto->Class == ITEM_CLASS_ARMOR && displaced &&
                         displaced->GetTemplate()->SubClass <= proto->SubClass;
    return fill(pick, replace ? ITEM_USAGE_REPLACE : ITEM_USAGE_EQUIP, reason);
}

uint32 CountUnwornCopies(Player* bot, uint32 itemId)
{
    uint32 const owned = bot->GetItemCount(itemId, true);
    uint32 const worn = WornCopies(bot, itemId);
    return owned > worn ? owned - worn : 0;
}

ItemUsage ItemUsageValue::QueryItemUsageForEquip(ItemTemplate const* itemProto, int32 randomPropertyId)
{
    return EvaluateGear(botAI, itemProto, randomPropertyId).usage;
}

ItemUsage ItemUsageValue::QueryItemUsageForAmmo(ItemTemplate const* proto)
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE && bot->getClass() != CLASS_WARRIOR)
        return ITEM_USAGE_NONE;

    if (!RangedWeaponNeedsAmmo(bot))
        return ITEM_USAGE_NONE;

    Item* rangedWeapon = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    uint32 requiredSubClass = 0;

    if (rangedWeapon)
    {
        switch (rangedWeapon->GetTemplate()->SubClass)
        {
            case ITEM_SUBCLASS_WEAPON_GUN:
                requiredSubClass = ITEM_SUBCLASS_BULLET;
                break;
            case ITEM_SUBCLASS_WEAPON_BOW:
            case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                requiredSubClass = ITEM_SUBCLASS_ARROW;
                break;
        }
    }

    // Ensure the item is the correct ammo type for the equipped ranged weapon
    if (proto->SubClass == requiredSubClass)
    {
        float ammoCount = BetterStacks(proto, "ammo");
        float requiredAmmo = (bot->getClass() == CLASS_HUNTER) ? 8 : 2; // Hunters get 8 stacks, others 2
        uint32 currentAmmoId = bot->GetUInt32Value(PLAYER_AMMO_ID);

        // Check if the bot has an ammo type assigned
        if (currentAmmoId == 0)
            return ITEM_USAGE_EQUIP;  // Equip the ammo if no ammo
        // Compare new ammo vs current equipped ammo
        ItemTemplate const* currentAmmoProto = sObjectMgr->GetItemTemplate(currentAmmoId);
        if (currentAmmoProto)
        {
            uint32 currentAmmoDPS = (currentAmmoProto->Damage[0].DamageMin + currentAmmoProto->Damage[0].DamageMax) * 1000 / 2;
            uint32 newAmmoDPS = (proto->Damage[0].DamageMin + proto->Damage[0].DamageMax) * 1000 / 2;

            if (newAmmoDPS > currentAmmoDPS) // New ammo meets upgrade condition
                return ITEM_USAGE_EQUIP;

            if (newAmmoDPS < currentAmmoDPS) // New ammo is worse
                return ITEM_USAGE_NONE;
        }
        // Ensure we have enough ammo in the inventory
        if (ammoCount < requiredAmmo)
        {
            ammoCount += CurrentStacks(proto);

            if (ammoCount < requiredAmmo)  // Buy ammo to reach the proper supply
                return ITEM_USAGE_AMMO;
            else if (ammoCount < requiredAmmo + 1)
                return ITEM_USAGE_KEEP;  // Keep the ammo if we don't have too much.
        }
    }
    return ITEM_USAGE_NONE;
}

bool ItemUsageValue::IsItemUsefulForQuest(Player* player, ItemTemplate const* proto)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
    if (!botAI)
        return false;

    // Core loot gating predicate: also covers quest source items (ItemDrop,
    // e.g. quest keys). The local objective check below is kept because this
    // predicate skips non-raid quests for raid groups.
    if (player->HasQuestForItem(proto->ItemId))
        return true;

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 entry = player->GetQuestSlotQuestId(slot);
        Quest const* quest = sObjectMgr->GetQuestTemplate(entry);
        if (!quest)
            continue;

        // Check if the item itself is needed for the quest
        for (uint8 i = 0; i < 4; i++)
        {
            if (quest->RequiredItemId[i] == proto->ItemId)
            {
                if (player->GetItemCount(proto->ItemId, false) >= quest->RequiredItemCount[i])
                    continue;

                return true; // Item is directly required for a quest
            }
        }

        // Check if the item has spells that create a required quest item
        for (uint8 i = 0; i < MAX_ITEM_SPELLS; i++)
        {
            uint32 spellId = proto->Spells[i].SpellId;
            if (!spellId)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo)
                continue;

            for (uint8 effectIndex = 0; effectIndex < MAX_SPELL_EFFECTS; effectIndex++)
            {
                if (spellInfo->Effects[effectIndex].Effect == SPELL_EFFECT_CREATE_ITEM)
                {
                    uint32 createdItemId = spellInfo->Effects[effectIndex].ItemType;

                    // Check if the created item is required for a quest
                    for (uint8 j = 0; j < 4; j++)
                    {
                        if (quest->RequiredItemId[j] == createdItemId)
                        {
                            if (player->GetItemCount(createdItemId, false) >= quest->RequiredItemCount[j])
                                continue;

                            return true; // Item is useful because it creates a required quest item
                        }
                    }
                }
            }
        }
    }

    return false; // Item is not useful for any active quests
}

bool ItemUsageValue::IsStartQuestItemUsefulFor(Player* player, ItemTemplate const* proto)
{
    if (!proto->StartQuest)
        return false;

    Quest const* quest = sObjectMgr->GetQuestTemplate(proto->StartQuest);
    return quest && player->GetQuestStatus(proto->StartQuest) == QUEST_STATUS_NONE &&
        QuestAction::CanAcceptQuest(player, quest);
}

bool ItemUsageValue::IsItemNeededForSkill(ItemTemplate const* proto)
{
    switch (proto->ItemId)
    {
        case 756:  // Tunnel Pick
            return botAI->HasSkill(SKILL_MINING);
        case 778:  // Kobold Excavation Pick
            return botAI->HasSkill(SKILL_MINING);
        case 1819:  // Gouging Pick
            return botAI->HasSkill(SKILL_MINING);
        case 1893:  // Miner's Revenge
            return botAI->HasSkill(SKILL_MINING);
        case 1959:  // Cold Iron Pick
            return botAI->HasSkill(SKILL_MINING);
        case 2901:  // Mining Pick
            return botAI->HasSkill(SKILL_MINING);
        case 9465:  // Digmaster 5000
            return botAI->HasSkill(SKILL_MINING);
        case 20723:  // Brann's Trusty Pick
            return botAI->HasSkill(SKILL_MINING);
        case 40772:  // Gnomish Army Knife
            return botAI->HasSkill(SKILL_MINING) || botAI->HasSkill(SKILL_ENGINEERING) || botAI->HasSkill(SKILL_BLACKSMITHING) || botAI->HasSkill(SKILL_COOKING) || botAI->HasSkill(SKILL_SKINNING);
        case 40892:  // Hammer Pick
            return botAI->HasSkill(SKILL_MINING) || botAI->HasSkill(SKILL_BLACKSMITHING);
        case 40893:  // Bladed Pickaxe
            return botAI->HasSkill(SKILL_MINING) || botAI->HasSkill(SKILL_SKINNING);
        case 5956:  // Blacksmith Hammer
            return botAI->HasSkill(SKILL_BLACKSMITHING) || botAI->HasSkill(SKILL_ENGINEERING);
        case 6219:  // Arclight Spanner
            return botAI->HasSkill(SKILL_ENGINEERING);
        case 6218:  // Runed copper rod
            return botAI->HasSkill(SKILL_ENCHANTING);
        case 6339:  // Runed silver rod
            return botAI->HasSkill(SKILL_ENCHANTING);
        case 11130:  // Runed golden rod
            return botAI->HasSkill(SKILL_ENCHANTING);
        case 11145:  // Runed truesilver rod
            return botAI->HasSkill(SKILL_ENCHANTING);
        case 16207:  // Runed Arcanite Rod
            return botAI->HasSkill(SKILL_ENCHANTING);
        case 7005:  // Skinning Knife
            return botAI->HasSkill(SKILL_SKINNING);
        case 12709:
            return botAI->HasSkill(SKILL_SKINNING);
        case 19901:
            return botAI->HasSkill(SKILL_SKINNING);
        case 4471:  // Flint and Tinder
            return botAI->HasSkill(SKILL_COOKING);
        case 4470:  // Simple Wood
            return botAI->HasSkill(SKILL_COOKING);
        case 6256:  // Fishing Rod
            return botAI->HasSkill(SKILL_FISHING);
    }

    return false;
}

bool ItemUsageValue::IsItemUsefulForSkill(ItemTemplate const* proto)
{
    switch (proto->Class)
    {
        case ITEM_CLASS_TRADE_GOODS:
        case ITEM_CLASS_MISC:
        case ITEM_CLASS_REAGENT:
        case ITEM_CLASS_GEM:
        {
            if (botAI->HasSkill(SKILL_TAILORING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_TAILORING))
                return true;
            if (botAI->HasSkill(SKILL_LEATHERWORKING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_LEATHERWORKING))
                return true;
            if (botAI->HasSkill(SKILL_ENGINEERING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_ENGINEERING))
                return true;
            if (botAI->HasSkill(SKILL_BLACKSMITHING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_BLACKSMITHING))
                return true;
            if (botAI->HasSkill(SKILL_ALCHEMY) && RandomItemMgr::IsUsedBySkill(proto, SKILL_ALCHEMY))
                return true;
            if (botAI->HasSkill(SKILL_ENCHANTING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_ENCHANTING))
                return true;
            if (botAI->HasSkill(SKILL_FISHING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_FISHING))
                return true;
            if (botAI->HasSkill(SKILL_FIRST_AID) && RandomItemMgr::IsUsedBySkill(proto, SKILL_FIRST_AID))
                return true;
            if (botAI->HasSkill(SKILL_COOKING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_COOKING))
                return true;
            if (botAI->HasSkill(SKILL_JEWELCRAFTING) && RandomItemMgr::IsUsedBySkill(proto, SKILL_JEWELCRAFTING))
                return true;
            if (botAI->HasSkill(SKILL_MINING) && (RandomItemMgr::IsUsedBySkill(proto, SKILL_MINING) ||
                                                  RandomItemMgr::IsUsedBySkill(proto, SKILL_BLACKSMITHING) ||
                                                  RandomItemMgr::IsUsedBySkill(proto, SKILL_JEWELCRAFTING) ||
                                                  RandomItemMgr::IsUsedBySkill(proto, SKILL_ENGINEERING)))
                return true;
            if (botAI->HasSkill(SKILL_SKINNING) && (RandomItemMgr::IsUsedBySkill(proto, SKILL_SKINNING) ||
                                                    RandomItemMgr::IsUsedBySkill(proto, SKILL_LEATHERWORKING)))
                return true;
            if (botAI->HasSkill(SKILL_HERBALISM) && (RandomItemMgr::IsUsedBySkill(proto, SKILL_HERBALISM) ||
                                                     RandomItemMgr::IsUsedBySkill(proto, SKILL_ALCHEMY)))
                return true;

            return false;
        }
        case ITEM_CLASS_RECIPE:
        {
            if (bot->HasSpell(proto->Spells[2].SpellId))
                break;

            switch (proto->SubClass)
            {
                case ITEM_SUBCLASS_LEATHERWORKING_PATTERN:
                    return botAI->HasSkill(SKILL_LEATHERWORKING);
                case ITEM_SUBCLASS_TAILORING_PATTERN:
                    return botAI->HasSkill(SKILL_TAILORING);
                case ITEM_SUBCLASS_ENGINEERING_SCHEMATIC:
                    return botAI->HasSkill(SKILL_ENGINEERING);
                case ITEM_SUBCLASS_BLACKSMITHING:
                    return botAI->HasSkill(SKILL_BLACKSMITHING);
                case ITEM_SUBCLASS_COOKING_RECIPE:
                    return botAI->HasSkill(SKILL_COOKING);
                case ITEM_SUBCLASS_ALCHEMY_RECIPE:
                    return botAI->HasSkill(SKILL_ALCHEMY);
                case ITEM_SUBCLASS_FIRST_AID_MANUAL:
                    return botAI->HasSkill(SKILL_FIRST_AID);
                case ITEM_SUBCLASS_ENCHANTING_FORMULA:
                    return botAI->HasSkill(SKILL_ENCHANTING);
                case ITEM_SUBCLASS_FISHING_MANUAL:
                    return botAI->HasSkill(SKILL_FISHING);
            }
        }
    }

    return false;
}

namespace
{
    // Crafting chains can be cyclic: Remember which items are already being evaluated
    // further up the stack and stop as soon as a cycle closes.
    thread_local std::unordered_set<uint32> itemsBeingEvaluated;

    class UsefullSpellRecursionGuard
    {
    public:
        explicit UsefullSpellRecursionGuard(uint32 itemId)
            : _itemId(itemId), _entered(itemsBeingEvaluated.insert(itemId).second)
        {
        }

        ~UsefullSpellRecursionGuard()
        {
            if (_entered)
                itemsBeingEvaluated.erase(_itemId);
        }

        UsefullSpellRecursionGuard(UsefullSpellRecursionGuard const&) = delete;
        UsefullSpellRecursionGuard& operator=(UsefullSpellRecursionGuard const&) = delete;

        bool Entered() const { return _entered; }

    private:
        uint32 _itemId;
        bool _entered;
    };
}

bool ItemUsageValue::IsItemNeededForUsefullSpell(ItemTemplate const* proto, bool checkAllReagents)
{
    UsefullSpellRecursionGuard guard(proto->ItemId);
    if (!guard.Entered())
        return false;

    for (auto spellId : SpellsUsingItem(proto->ItemId, bot))
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        if (checkAllReagents && !HasItemsNeededForSpell(spellId, proto))
            continue;

        // Class spells consume reagents without creating items; generic profession spells
        // must not make every trainer recipe's reagent worth hoarding.
        if (spellInfo->SpellFamilyName != SPELLFAMILY_GENERIC)
            return true;

        if (SpellGivesSkillUp(spellId, bot))
            return true;

        uint32 newItemId = spellInfo->Effects[EFFECT_0].ItemType;
        if (newItemId && newItemId != proto->ItemId)
        {
            ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", newItemId);

            if (usage != ITEM_USAGE_REPLACE && usage != ITEM_USAGE_EQUIP && usage != ITEM_USAGE_AMMO &&
                usage != ITEM_USAGE_QUEST && usage != ITEM_USAGE_SKILL && usage != ITEM_USAGE_USE)
                continue;

            return true;
        }
    }

    return false;
}

bool ItemUsageValue::IsItemNeededForClassSpell(ItemTemplate const* proto)
{
    for (uint32 spellId : SpellsUsingItem(proto->ItemId, bot))
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (spellInfo && spellInfo->SpellFamilyName != SPELLFAMILY_GENERIC)
            return true;
    }

    return false;
}

static SpellInfo const* GetTemporaryEnchantSpell(ItemTemplate const* proto)
{
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        uint32 spellId = proto->Spells[i].SpellId;
        if (!spellId)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        for (uint8 j = 0; j < MAX_SPELL_EFFECTS; ++j)
            if (spellInfo->Effects[j].Effect == SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY)
                return spellInfo;
    }

    return nullptr;
}

// True for rogue poisons: their temporary enchant spell is the rogue family and carries a
// per-poison family flag (stones/oils share the subclass but use family-less spells).
static bool IsRoguePoison(ItemTemplate const* proto)
{
    if (proto->SubClass != ITEM_SUBCLASS_CONSUMABLE_OTHER)
        return false;

    SpellInfo const* useSpell = GetTemporaryEnchantSpell(proto);
    return useSpell && useSpell->SpellFamilyName == SPELLFAMILY_ROGUE && !useSpell->SpellFamilyFlags.IsEqual(0, 0, 0);
}

bool ItemUsageValue::IsTemporaryWeaponEnchantment(ItemTemplate const* proto)
{
    return GetTemporaryEnchantSpell(proto) != nullptr;
}

bool ItemUsageValue::IsBestPoison(ItemTemplate const* proto)
{
    // Family comes from the use spell's SpellFamilyFlags (per poison type), not item ids.
    if (!IsRoguePoison(proto))
        return false;

    SpellInfo const* useSpell = GetTemporaryEnchantSpell(proto);

    // Keep only the highest-item-level poison per family (one family per weapon); lower ranks
    // fall through to VENDOR/AH.
    for (Item* item : botAI->GetInventoryItems())
    {
        ItemTemplate const* other = item->GetTemplate();
        if (!other || other->ItemId == proto->ItemId)
            continue;
        if (!IsRoguePoison(other))
            continue;

        // Unusable higher ranks (level or class restriction) must not block a usable lower rank.
        if (bot->CanUseItem(other) != EQUIP_ERR_OK)
            continue;

        SpellInfo const* otherUseSpell = GetTemporaryEnchantSpell(other);
        if (otherUseSpell && otherUseSpell->SpellFamilyFlags == useSpell->SpellFamilyFlags &&
            other->ItemLevel > proto->ItemLevel)
            return false;
    }

    return true;
}

bool ItemUsageValue::HasItemsNeededForSpell(uint32 spellId, ItemTemplate const* proto)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;

    for (uint8 i = 0; i < MAX_SPELL_REAGENTS; i++)
    {
        // Same validity check Spell::CheckItems uses: invalid/sentinel reagent ids are skipped.
        if (spellInfo->Reagent[i] <= 0)
            continue;

        if (proto && proto->ItemId == uint32(spellInfo->Reagent[i]) &&
            spellInfo->ReagentCount[i] == 1)  // If we only need 1 item then current item does not need to be
                                              // checked since we are looting/buying or already have it.
            continue;

        ItemTemplate const* reqProto = sObjectMgr->GetItemTemplate(spellInfo->Reagent[i]);
        if (!reqProto)
            continue;

        uint32 count = AI_VALUE2(uint32, "item count", reqProto->Name1);

        if (count < spellInfo->ReagentCount[i])
            return false;
    }

    return true;
}

float ItemUsageValue::CurrentStacks(ItemTemplate const* proto)
{
    uint32 maxStack = proto->GetMaxStackSize();

    std::vector<Item*> found = AI_VALUE2(std::vector<Item*>, "inventory items", chat->FormatItem(proto));

    float itemCount = 0;

    for (auto stack : found)
    {
        itemCount += stack->GetCount();
    }

    return itemCount / maxStack;
}

float ItemUsageValue::BetterStacks(ItemTemplate const* proto, std::string const itemType)
{
    std::vector<Item*> items = AI_VALUE2(std::vector<Item*>, "inventory items", itemType);

    float stacks = 0;

    for (auto& otherItem : items)
    {
        ItemTemplate const* otherProto = otherItem->GetTemplate();

        if (otherProto->Class != proto->Class || otherProto->SubClass != proto->SubClass)
            continue;

        if (otherProto->ItemLevel < proto->ItemLevel)
            continue;

        if (otherProto->ItemId == proto->ItemId)
            continue;

        stacks += CurrentStacks(otherProto);
    }

    return stacks;
}

std::vector<uint32> ItemUsageValue::SpellsUsingItem(uint32 itemId, Player* bot)
{
    std::vector<uint32> retSpells;

    PlayerSpellMap const& spellMap = bot->GetSpellMap();

    for (auto& spell : spellMap)
    {
        uint32 spellId = spell.first;

        if (spell.second->State == PLAYERSPELL_REMOVED || !spell.second->Active)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        // Skip only spells the server itself will not charge reagents for (Glyph of Renewed Life);
        // SPELL_ATTR_PASSIVE alone does not mean the reagent is unused.
        if (bot->CanNoReagentCast(spellInfo))
            continue;

        // Profession spells are generic-family; only their crafting spells (CREATE_ITEM) count.
        // Class spells (poisons, oils, water walking, ...) consume reagents without creating items.
        if (spellInfo->Effects[EFFECT_0].Effect != SPELL_EFFECT_CREATE_ITEM &&
            spellInfo->SpellFamilyName == SPELLFAMILY_GENERIC)
            continue;

        for (uint8 i = 0; i < MAX_SPELL_REAGENTS; i++)
            if (spellInfo->ReagentCount[i] > 0 && uint32(spellInfo->Reagent[i]) == itemId)
                retSpells.push_back(spellId);
    }

    return retSpells;
}

inline int32 SkillGainChance(uint32 SkillValue, uint32 GrayLevel, uint32 GreenLevel, uint32 YellowLevel)
{
    if (SkillValue >= GrayLevel)
        return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_GREY) * 10;

    if (SkillValue >= GreenLevel)
        return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_GREEN) * 10;

    if (SkillValue >= YellowLevel)
        return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_YELLOW) * 10;

    return sWorld->getIntConfig(CONFIG_SKILL_CHANCE_ORANGE) * 10;
}

bool ItemUsageValue::SpellGivesSkillUp(uint32 spellId, Player* bot)
{
    SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* skill = _spell_idx->second;
        if (skill->SkillLine)
        {
            uint32 SkillValue = bot->GetPureSkillValue(skill->SkillLine);

            if (SkillGainChance(SkillValue, skill->TrivialSkillLineRankHigh,
                                (skill->TrivialSkillLineRankHigh + skill->TrivialSkillLineRankLow) / 2,
                                skill->TrivialSkillLineRankLow) > 0)
                return true;
        }
    }

    return false;
}

std::string const ItemUsageValue::GetConsumableType(ItemTemplate const* proto, bool hasMana)
{
    std::string const foodType = "";

    if ((proto->SubClass == ITEM_SUBCLASS_CONSUMABLE || proto->SubClass == ITEM_SUBCLASS_FOOD))
    {
        if (proto->Spells[0].SpellCategory == 11)
            return "food";
        else if (proto->Spells[0].SpellCategory == 59 && hasMana)
            return "drink";
    }

    if (proto->SubClass == ITEM_SUBCLASS_POTION || proto->SubClass == ITEM_SUBCLASS_FLASK)
    {
        for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[j].SpellId);
            if (spellInfo)
                for (int i = 0; i < 3; i++)
                {
                    if (spellInfo->Effects[i].Effect == SPELL_EFFECT_ENERGIZE && hasMana)
                        return "mana potion";

                    if (spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL)
                        return "healing potion";
                }
        }
    }

    if (proto->SubClass == ITEM_SUBCLASS_BANDAGE)
    {
        return "bandage";
    }

    return "";
}

ItemUsage ItemUpgradeValue::Calculate()
{
    ParsedItemUsage parsed = ParseItemUsageQualifier(qualifier);
    uint32 itemId = parsed.itemId;
    uint32 randomPropertyId = parsed.randomPropertyId;
    if (!itemId)
        return ITEM_USAGE_NONE;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return ITEM_USAGE_NONE;

    ItemUsage equip = QueryItemUsageForEquip(proto, randomPropertyId);
    if (equip != ITEM_USAGE_NONE)
        return equip;

    if (proto->Class == ITEM_CLASS_PROJECTILE && bot->CanUseItem(proto) == EQUIP_ERR_OK)
        return QueryItemUsageForAmmo(proto);

    return ITEM_USAGE_NONE;
}

bool ItemUsageValue::IsLockboxItem(ItemTemplate const* proto)
{
    if (!proto)
        return false;

    // Primary, data-driven detection: lockboxes with a lock ID and Misc class.
    if (proto->LockID && proto->Class == ITEM_CLASS_MISC)
        return true;

    // English-only fallback on name (aligns with loot-roll heuristics).
    std::string const nameLower = ToLowerUtf8(proto->Name1);
    if (nameLower.empty())
        return false;

    return nameLower.find("lockbox") != std::string::npos;
}

namespace
{
// Profession helpers: true if the item is a recipe/pattern/book (ITEM_CLASS_RECIPE).
static inline bool IsRecipeItem(ItemTemplate const* proto) { return proto && proto->Class == ITEM_CLASS_RECIPE; }

// Special-case: Book of Glyph Mastery (can own several; do not downgrade NEED on duplicates).
static bool IsGlyphMasteryBook(ItemTemplate const* proto)
{
    if (!proto)
        return false;

    if (proto->Class != ITEM_CLASS_RECIPE || proto->SubClass != ITEM_SUBCLASS_BOOK)
        return false;

    constexpr uint32 SPELL_BOOK_OF_GLYPH_MASTERY = 64323;
    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId == SPELL_BOOK_OF_GLYPH_MASTERY)
            return true;
    }

    if (proto->RequiredSkill == SKILL_INSCRIPTION)
    {
        std::string n = ToLowerUtf8(proto->Name1);
        if (n.find("glyph mastery") != std::string::npos || n.find("book of glyph mastery") != std::string::npos)
            return true;
    }

    return false;
}

// Value object for collectible cosmetics (mounts/pets) used in loot rules.
struct CollectibleInfo
{
    bool isCosmetic = false;   // true if this is a cosmetic collectible (mount/pet)
    bool alreadyOwned = false; // true if the bot already knows/owns it in a meaningful way
};

static CollectibleInfo BuildCollectibleInfo(Player* bot, ItemTemplate const* proto)
{
    CollectibleInfo info;

    if (!bot || !proto)
        return info;

    if (proto->Class != ITEM_CLASS_MISC)
        return info;

#if defined(ITEM_SUBCLASS_MISC_MOUNT) || defined(ITEM_SUBCLASS_MISC_PET)
    bool const isMount =
#  if defined(ITEM_SUBCLASS_MISC_MOUNT)
        proto->SubClass == ITEM_SUBCLASS_MISC_MOUNT
#  else
        false
#  endif
        ;
    bool const isPet =
#  if defined(ITEM_SUBCLASS_MISC_PET)
        proto->SubClass == ITEM_SUBCLASS_MISC_PET
#  else
        false
#  endif
        ;
    if (!isMount && !isPet)
        return info;
#else
    if (proto->SubClass != 2 && proto->SubClass != 5)
        return info;
#endif

    info.isCosmetic = true;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        uint32 const spellId = proto->Spells[i].SpellId;
        if (!spellId)
            continue;

        if (bot->HasSpell(spellId))
        {
            info.alreadyOwned = true;
            return info;
        }
    }

    if (bot->GetItemCount(proto->ItemId, true) > 0)
        info.alreadyOwned = true;

    return info;
}

struct RecipeInfo
{
    uint32 requiredSkill = 0;
    uint32 requiredRank = 0;
    uint32 botRank = 0;
    bool botHasProfession = false;
    bool known = false;
};

static RecipeInfo BuildRecipeInfo(Player* bot, ItemTemplate const* proto)
{
    RecipeInfo info;

    if (!bot || !IsRecipeItem(proto))
        return info;

    info.requiredSkill = GetRecipeSkill(proto);
    info.requiredRank = proto->RequiredSkillRank;

    if (!info.requiredSkill)
        return info;

    info.botHasProfession = bot->HasSkill(info.requiredSkill);
    if (info.botHasProfession)
        info.botRank = bot->GetSkillValue(info.requiredSkill);

    if (bot)
    {
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            uint32 teach = proto->Spells[i].SpellId;
            if (!teach)
                continue;

            SpellInfo const* si = sSpellMgr->GetSpellInfo(teach);
            if (!si)
                continue;

            for (int eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
            {
                if (si->Effects[eff].Effect != SPELL_EFFECT_LEARN_SPELL)
                    continue;

                uint32 learned = si->Effects[eff].TriggerSpell;
                if (learned && bot->HasSpell(learned))
                {
                    info.known = true;
                    break;
                }
            }

            if (info.known)
                break;
        }
    }

    return info;
}

static bool IsProfessionRecipeUsefulForBot(RecipeInfo const& recipe)
{
    if (!recipe.requiredSkill)
        return false;

    if (!recipe.botHasProfession)
        return false;

    if (!sPlayerbotAIConfig.RecipesIgnoreSkillRank && recipe.requiredRank && recipe.botRank < recipe.requiredRank)
        return false;

    if (recipe.known)
        return false;

    return true;
}

static bool IsClassAllowedByItemTemplate(uint8 cls, ItemTemplate const* proto)
{
    if (!proto)
        return true;

    int32 const allowable = proto->AllowableClass;
    if (allowable <= 0)
        return true;

    if (!cls)
        return false;

    uint32 const classMask = static_cast<uint32>(allowable);
    uint32 const thisClassBit = 1u << (cls - 1u);
    return (classMask & thisClassBit) != 0;
}

static bool CanBotUseToken(ItemTemplate const* proto, Player* bot);
static bool RollUniqueCheck(ItemTemplate const* proto, Player* bot);

static inline bool IsLikelyDisenchantable(ItemTemplate const* proto)
{
    if (!proto)
        return false;

    if (proto->DisenchantID > 0)
        return true;

    if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON)
        return false;

    return proto->Quality >= ITEM_QUALITY_UNCOMMON && proto->Quality <= ITEM_QUALITY_EPIC;
}

static int8 TokenSlotFromName(ItemTemplate const* proto)
{
    if (!proto)
        return -1;
    std::string n = std::string(proto->Name1);
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (n.find("helm") != std::string::npos || n.find("head") != std::string::npos)
        return INVTYPE_HEAD;

    if (n.find("shoulder") != std::string::npos || n.find("mantle") != std::string::npos ||
        n.find("spauld") != std::string::npos)
        return INVTYPE_SHOULDERS;

    if (n.find("chest") != std::string::npos || n.find("tunic") != std::string::npos ||
        n.find("robe") != std::string::npos || n.find("breastplate") != std::string::npos ||
        n.find("chestguard") != std::string::npos)
        return INVTYPE_CHEST;

    if (n.find("glove") != std::string::npos || n.find("handguard") != std::string::npos ||
        n.find("gauntlet") != std::string::npos)
        return INVTYPE_HANDS;

    if (n.find("leg") != std::string::npos || n.find("pant") != std::string::npos ||
        n.find("trouser") != std::string::npos)
        return INVTYPE_LEGS;

    return -1;
}

constexpr uint32 SANCTIFICATION_TOKEN_MAX_COUNT = 5u;

static std::array<uint32, 6> const& GetSanctificationTokenIds()
{
    static std::array<uint32, 6> const sanctificationTokenIds = {
        52025,
        52026,
        52027,
        52028,
        52029,
        52030
    };

    return sanctificationTokenIds;
}

static bool IsSanctificationToken(ItemTemplate const* proto)
{
    if (!proto)
        return false;

    for (uint32 const entry : GetSanctificationTokenIds())
    {
        if (proto->ItemId == entry)
            return true;
    }

    return false;
}

static uint32 GetOwnedSanctificationTokenCount(Player* bot)
{
    if (!bot)
        return 0;

    uint32 total = 0;
    for (uint32 const entry : GetSanctificationTokenIds())
        total += bot->GetItemCount(entry, true);

    return total;
}

static uint8 EquipmentSlotByInvTypeSafe(uint8 invType)
{
    switch (invType)
    {
        case INVTYPE_HEAD:
            return EQUIPMENT_SLOT_HEAD;
        case INVTYPE_SHOULDERS:
            return EQUIPMENT_SLOT_SHOULDERS;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
            return EQUIPMENT_SLOT_CHEST;
        case INVTYPE_HANDS:
            return EQUIPMENT_SLOT_HANDS;
        case INVTYPE_LEGS:
            return EQUIPMENT_SLOT_LEGS;
        default:
            return EQUIPMENT_SLOT_END;
    }
}

static bool IsTokenLikelyUpgrade(ItemTemplate const* token, uint8 invTypeSlot, Player* bot)
{
    if (!token || !bot)
        return false;
    uint8 eq = EquipmentSlotByInvTypeSafe(invTypeSlot);
    if (eq >= EQUIPMENT_SLOT_END)
        return true;

    Item* oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, eq);
    if (!oldItem)
        return true;

    ItemTemplate const* oldProto = oldItem->GetTemplate();
    if (!oldProto)
        return true;

    float margin = sPlayerbotAIConfig.TokenILevelMargin;
    return (float)token->ItemLevel >= (float)oldProto->ItemLevel + margin;
}

// For tokens whose name doesn't reveal the slot (e.g. "Trophy of the Crusade" redeems any of the
// five tier pieces): treat as upgrade when any tier slot is empty or behind the token's ilvl.
static bool IsAnyTierSlotLikelyUpgrade(ItemTemplate const* token, Player* bot)
{
    if (!token || !bot)
        return false;

    static constexpr uint8 tierSlots[] = {EQUIPMENT_SLOT_HEAD, EQUIPMENT_SLOT_SHOULDERS, EQUIPMENT_SLOT_CHEST,
                                          EQUIPMENT_SLOT_HANDS, EQUIPMENT_SLOT_LEGS};

    float margin = sPlayerbotAIConfig.TokenILevelMargin;
    for (uint8 const slot : tierSlots)
    {
        Item* oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!oldItem)
            return true;

        ItemTemplate const* oldProto = oldItem->GetTemplate();
        if (!oldProto)
            return true;

        if ((float)token->ItemLevel >= (float)oldProto->ItemLevel + margin)
            return true;
    }

    return false;
}

struct TokenInfo
{
    bool isToken = false;
    bool classCanUse = false;
    int8 invTypeSlot = -1;
    bool likelyUpgrade = false;
};

static TokenInfo BuildTokenInfo(ItemTemplate const* proto, Player* bot)
{
    TokenInfo info;

    if (!proto || !bot)
        return info;

    info.isToken = (proto->Class == ITEM_CLASS_MISC &&
                    proto->SubClass == ITEM_SUBCLASS_JUNK &&
                    proto->Quality == ITEM_QUALITY_EPIC) ||
                   IsSanctificationToken(proto);
    if (!info.isToken)
        return info;

    info.classCanUse = CanBotUseToken(proto, bot);
    info.invTypeSlot = TokenSlotFromName(proto);

    if (info.classCanUse)
    {
        if (info.invTypeSlot >= 0)
            info.likelyUpgrade = IsTokenLikelyUpgrade(proto, static_cast<uint8>(info.invTypeSlot), bot);
        // Sanctification marks excluded: their ilvl says nothing about the 251->264->277 upgrade they gate.
        else if (!IsSanctificationToken(proto))
            info.likelyUpgrade = IsAnyTierSlotLikelyUpgrade(proto, bot);
    }

    return info;
}

static bool TryTokenRollVote(ItemTemplate const* proto, Player* bot, RollVote& outVote)
{
    TokenInfo const token = BuildTokenInfo(proto, bot);

    // Not a token → let other rules decide.
    if (!token.isToken)
        return false;

    if (token.classCanUse)
    {
        if (token.invTypeSlot >= 0)
        {
            // Known slot: NEED only if it looks like an upgrade, otherwise GREED.
            outVote = token.likelyUpgrade ? NEED : GREED;
        }
        else
        {
            // Unknown slot (e.g. T10 sanctification tokens).
            if (IsSanctificationToken(proto) && sPlayerbotAIConfig.SanctificationTokenRollMode == 1u)
            {
                uint32 const ownedTokens = GetOwnedSanctificationTokenCount(bot);
                outVote = (ownedTokens < SANCTIFICATION_TOKEN_MAX_COUNT) ? NEED : GREED;
            }
            else
                outVote = GREED;
        }
    }
    else
    {
        // Not eligible, so GREED.
        outVote = GREED;
    }

    return true;
}

static RollVote ApplyDisenchantPreference(RollVote currentVote, ItemTemplate const* proto, ItemUsage usage,
                                          Group* group, Player* bot)
{
    bool const isDeCandidate = IsLikelyDisenchantable(proto);
    bool const hasEnchantSkill = bot && bot->HasSkill(SKILL_ENCHANTING);

    uint8 const deMode = sPlayerbotAIConfig.DeButtonMode;
    // Mode 0 = no DE button; 1 = enchanters only; 2 = all bots can DE.
    bool const deAllowedForBot = (deMode == 2u) || (deMode == 1u && hasEnchantSkill);

    if (currentVote != NEED &&
        deAllowedForBot &&
        group &&
        (group->GetLootMethod() == NEED_BEFORE_GREED || group->GetLootMethod() == GROUP_LOOT) &&
        isDeCandidate &&
        usage == ITEM_USAGE_DISENCHANT)
        return DISENCHANT;

    return currentVote;
}

static RollVote FinalizeRollVote(RollVote vote, ItemTemplate const* proto, ItemUsage usage, Group* group, Player* bot)
{
    if (sPlayerbotAIConfig.LootRollLevel == 0)
    {
        return PASS;
    }

    // Upgrades-only: bots GREED (not NEED) on their own gear upgrades so real players keep NEED
    // priority; everything else (recipes, mats, disenchant, off-spec, cosmetics) -> PASS.
    // Tier tokens are non-equippable so their usage is never EQUIP/REPLACE — re-check them here
    // so class-eligible token upgrades still get a GREED instead of falling through to PASS.
    if (sPlayerbotAIConfig.RollUpgradesOnly)
    {
        TokenInfo const token = BuildTokenInfo(proto, bot);  // early-outs cheaply for non-tokens
        if (token.isToken)
        {
            if (!token.classCanUse)
                return PASS;

            if (IsSanctificationToken(proto))
                return (sPlayerbotAIConfig.SanctificationTokenRollMode == 1u &&
                        GetOwnedSanctificationTokenCount(bot) < SANCTIFICATION_TOKEN_MAX_COUNT) ? GREED : PASS;

            // Bots never spend tokens; cap at 1 unspent copy.
            if (bot->GetItemCount(proto->ItemId, true) > 0)
                return PASS;

            return token.likelyUpgrade ? GREED : PASS;
        }

        return (vote == NEED && (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE)) ? GREED : PASS;
    }

    vote = ApplyDisenchantPreference(vote, proto, usage, group, bot);

    if (sPlayerbotAIConfig.LootRollLevel == 1)
    {
        // Level 1 = "always greed": never NEED (downgrade to GREED, or PASS a unique duplicate);
        // items already only worth GREED keep the GREED roll.
        if (vote == NEED)
            vote = RollUniqueCheck(proto, bot) ? PASS : GREED;
    }

    return vote;
}

static RollVote CalculateBaseRollVote(Player* bot, ItemTemplate const* proto, int32 /*randomProperty*/, ItemUsage usage)
{
    // Player mimic: upgrade => NEED; useful => GREED; otherwise => PASS
    RollVote vote = PASS;

    CollectibleInfo const collectible = BuildCollectibleInfo(bot, proto);

    bool const isCollectibleCosmetic = collectible.isCosmetic;
    bool const alreadyHasCollectible = collectible.alreadyOwned;

    if (isCollectibleCosmetic)
        vote = alreadyHasCollectible ? GREED : NEED;

    bool recipeChecked = false;
    bool recipeNeed = false;
    bool recipeUseful = false;

    // Professions: NEED on useful recipes/patterns/books when enabled.
    if (sPlayerbotAIConfig.NeedOnProfessionRecipes && IsRecipeItem(proto))
    {
        RecipeInfo const recipe = BuildRecipeInfo(bot, proto);

        recipeChecked = true;
        recipeUseful = IsProfessionRecipeUsefulForBot(recipe);
        if (recipeUseful)
        {
            vote = NEED;
            recipeNeed = true;
        }
        else
            vote = GREED;  // recipe not for the bot -> GREED
    }

    // Do not overwrite the choice if already decided by recipe or cosmetic logic.
    if (!recipeChecked && !isCollectibleCosmetic)
    {
        switch (usage)
        {
            case ITEM_USAGE_EQUIP:
            case ITEM_USAGE_REPLACE:
                vote = NEED;
                break;
            case ITEM_USAGE_BAD_EQUIP:
            case ITEM_USAGE_GUILD_TASK:
            case ITEM_USAGE_SKILL:
            case ITEM_USAGE_USE:
            case ITEM_USAGE_DISENCHANT:
            case ITEM_USAGE_AH:
            case ITEM_USAGE_VENDOR:
            case ITEM_USAGE_KEEP:
            case ITEM_USAGE_AMMO:
                vote = GREED;
                break;
            default:
                vote = PASS;
                break;
        }
    }

    // Lockboxes: if the item is a lockbox and the bot is a Rogue with Lockpicking, prefer NEED (ignored by BoE/BoU).
    const SpecTraits traits = GetSpecTraits(bot);
    const bool isLockbox = ItemUsageValue::IsLockboxItem(proto);
    if (isLockbox && traits.isRogue && bot->HasSkill(SKILL_LOCKPICKING))
        vote = NEED;

    // BoE/BoU rule: by default, avoid NEED on Bind-on-Equip / Bind-on-Use (raid etiquette)
    // BoE/BoU etiquette: avoid NEED on BoE/BoU, except useful profession recipes.
    constexpr uint32 BIND_WHEN_EQUIPPED = 2;  // BoE
    constexpr uint32 BIND_WHEN_USE = 3;       // BoU

    if (vote == NEED && !recipeNeed && !isLockbox && !isCollectibleCosmetic &&
        proto->Bonding == BIND_WHEN_EQUIPPED &&
        !sPlayerbotAIConfig.AllowBoENeedIfUpgrade)

        vote = GREED;

    if (vote == NEED && !recipeNeed && !isLockbox && !isCollectibleCosmetic &&
        proto->Bonding == BIND_WHEN_USE &&
        !sPlayerbotAIConfig.AllowBoUNeedIfUpgrade)

        vote = GREED;

    // Non-unique soft rule: NEED -> GREED on a spare copy in bags or bank, except Book of Glyph
    // Mastery. A worn copy doesn't count, or a second weapon for the off hand never gets NEED.
    if (vote == NEED && !IsGlyphMasteryBook(proto) && CountUnwornCopies(bot, proto->ItemId) > 0)
        vote = GREED;

    // Unique-equip: never NEED a duplicate (already equipped/owned)
    if (vote == NEED && RollUniqueCheck(proto, bot))
        vote = PASS;

    // Final decision (with allow/deny from loot strategy).
    RollVote finalVote = StoreLootAction::IsLootAllowed(proto->ItemId, GET_PLAYERBOT_AI(bot)) ? vote : PASS;

    return finalVote;
}

static bool CanBotUseToken(ItemTemplate const* proto, Player* bot)
{
    if (!proto || !bot)
        return false;

    return IsClassAllowedByItemTemplate(bot->getClass(), proto);
}

static bool RollUniqueCheck(ItemTemplate const* proto, Player* bot)
{
    if (!proto || !bot)
        return false;

    // owned anywhere, worn included
    return proto->HasFlag(ITEM_FLAG_UNIQUE_EQUIPPABLE) && bot->GetItemCount(proto->ItemId, true) > 0;
}
} // namespace

char const* RollVoteText(RollVote v)
{
    switch (v)
    {
        case NEED:
            return "NEED";
        case GREED:
            return "GREED";
        case PASS:
            return "PASS";
        case DISENCHANT:
            return "DISENCHANT";
        default:
            return "UNKNOWN";
    }
}

RollVote CalculateLootRollVote(Player* bot, ItemTemplate const* proto, int32 randomProperty, ItemUsage usage,
                               Group* group)
{
    if (!bot || !proto)
        return PASS;

    RollVote vote = PASS;
    if (!TryTokenRollVote(proto, bot, vote))
    {
        // Let CalculateBaseRollVote decide using loot-aware item usage.
        vote = CalculateBaseRollVote(bot, proto, randomProperty, usage);
    }

    return FinalizeRollVote(vote, proto, usage, group, bot);
}
