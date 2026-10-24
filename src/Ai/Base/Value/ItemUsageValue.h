/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ITEMUSAGEVALUE_H
#define PLAYERBOTS_ITEMUSAGEVALUE_H

#include <string>
#include <vector>

#include "NamedObjectContext.h"
#include "SharedDefines.h"
#include "Value.h"

// Shared UTF-8 lowercase helper used by item/loot logic.
std::string ToLowerUtf8(std::string const& s);

class Item;
class Group;
class Player;
class PlayerbotAI;

struct ItemTemplate;
enum RollVote : uint8;

struct ParsedItemUsage
{
    uint32 itemId = 0;
    int32 randomPropertyId = 0;
};

// Shared helper: infer profession SkillLine for a recipe item.
// Uses RequiredSkill when available, otherwise falls back to SubClass/name heuristics.
uint32 GetRecipeSkill(ItemTemplate const* proto);

// Shared loot/spec helpers used by ItemUsageValue and loot-roll logic
struct SpecTraits
{
    uint8 cls = 0;
    std::string spec;
    bool isCaster = false;
    bool isHealer = false;
    bool isTank = false;
    bool isPhysical = false;
    bool isDKTank = false;
    bool isWarProt = false;
    bool isEnhSham = false;
    bool isFeralTk = false;
    bool isFeralDps = false;
    bool isHunter = false;
    bool isRogue = false;
    bool isWarrior = false;
    bool isRetPal = false;
    bool isProtPal = false;
};

struct ItemStatProfile
{
    bool hasINT = false;
    bool hasSPI = false;
    bool hasMP5 = false;
    bool hasSP = false;
    bool hasSTR = false;
    bool hasAGI = false;
    bool hasSTA = false;
    bool hasAP = false;
    bool hasARP = false;
    bool hasEXP = false;
    bool hasHIT = false;
    bool hasHASTE = false;
    bool hasCRIT = false;
    bool hasDef = false;
    bool hasAvoid = false;
    bool hasBlockValue = false;
    bool hasSpellPen = false;
    // Melee/ranged hit, crit, haste or expertise. Those ratings only itemize physical gear, but they
    // are not a physical *stat*, so they need their own flag to count toward hasPhysical.
    bool hasPhysicalRating = false;
};

SpecTraits GetSpecTraits(Player* bot);
ItemStatProfile BuildItemStatProfile(ItemTemplate const* proto);

enum ItemUsage : uint32
{
    ITEM_USAGE_NONE = 0,
    ITEM_USAGE_EQUIP = 1,
    ITEM_USAGE_REPLACE = 2,
    ITEM_USAGE_BAD_EQUIP = 3,
    ITEM_USAGE_BROKEN_EQUIP = 4,
    ITEM_USAGE_QUEST = 5,
    ITEM_USAGE_SKILL = 6,
    ITEM_USAGE_USE = 7,
    ITEM_USAGE_GUILD_TASK = 8,
    ITEM_USAGE_DISENCHANT = 9,
    ITEM_USAGE_AH = 10,
    ITEM_USAGE_KEEP = 11,
    ITEM_USAGE_VENDOR = 12,
    ITEM_USAGE_AMMO = 13
};

// Whether a bot would wear an item and where. Loot votes, auto-equip, vendor buys and calc all read
// this one decision.
struct GearChoice
{
    // EQUIP, REPLACE, BAD_EQUIP (unfit, fills an empty slot), BROKEN_EQUIP or NONE
    ItemUsage usage = ITEM_USAGE_NONE;
    // equipment or bag slot the item goes to, NULL_SLOT for none
    uint8 slot = 255;
    // weapon swap: where the weapon worn in slot moves to
    uint8 incumbentTo = 255;
    // over the slots whose occupant changes; old has each slot's threshold applied
    float newScore = 0.0f;
    float oldScore = 0.0f;
    // the check that refused it, or why it was taken
    char const* reason = "";
};

// candidate is the bag item when there is one, for the broken and binding checks; null for a drop
// or a link. force (the equip command) skips the fitness checks and thresholds and takes the best
// legal slot even without a gain.
GearChoice EvaluateGear(PlayerbotAI* botAI, ItemTemplate const* proto, int32 randomPropertyId,
                        Item const* candidate = nullptr, bool force = false);

// Copies in bags and bank. Player::GetItemCount counts worn ones whatever inBankAlso says.
uint32 CountUnwornCopies(Player* bot, uint32 itemId);

class ItemUsageValue : public CalculatedValue<ItemUsage>, public Qualified
{
public:
    ItemUsageValue(PlayerbotAI* botAI, std::string const name = "item usage") : CalculatedValue<ItemUsage>(botAI, name)
    {
    }

    static std::string BuildItemUsageParam(uint32 itemId, int32 randomPropertyId);
    static bool IsLockboxItem(ItemTemplate const* proto);

    ItemUsage Calculate() override;

protected:
    ItemUsage QueryItemUsageForEquip(ItemTemplate const* proto, int32 randomPropertyId = 0);
    ItemUsage QueryItemUsageForAmmo(ItemTemplate const* proto);

private:
    bool IsItemUsefulForQuest(Player* player, ItemTemplate const* proto);
    bool IsItemNeededForSkill(ItemTemplate const* proto);
    bool IsItemUsefulForSkill(ItemTemplate const* proto);
    bool IsItemNeededForUsefullSpell(ItemTemplate const* proto, bool checkAllReagents = false);
    bool HasItemsNeededForSpell(uint32 spellId, ItemTemplate const* proto);
    float CurrentStacks(ItemTemplate const* proto);
    float BetterStacks(ItemTemplate const* proto, std::string const usageType = "");

public:
    static std::vector<uint32> SpellsUsingItem(uint32 itemId, Player* bot);
    static bool SpellGivesSkillUp(uint32 spellId, Player* bot);
    static std::string const GetConsumableType(ItemTemplate const* proto, bool hasMana);
};

// Refactor EquipActions: dedicated value used for equip-upgrade triggers
class ItemUpgradeValue : public ItemUsageValue
{
public:
    ItemUpgradeValue(PlayerbotAI* botAI, std::string const name = "item upgrade") : ItemUsageValue(botAI, name) {}
    ItemUsage Calculate() override;
};

// Loot roll helpers (used by LootRollAction)
char const* RollVoteText(RollVote v);
RollVote CalculateLootRollVote(Player* bot, ItemTemplate const* proto, int32 randomProperty, ItemUsage usage,
                               Group* group);

#endif
