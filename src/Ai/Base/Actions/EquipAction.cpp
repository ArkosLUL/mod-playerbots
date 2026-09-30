/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EquipAction.h"
#include "Event.h"
#include "ItemCountValue.h"
#include "ItemPackets.h"
#include "ItemUsageValue.h"
#include "ItemVisitors.h"
#include "Playerbots.h"
#include "StatsWeightCalculator.h"
#include <utility>

namespace
{
enum class WeaponLayout
{
    Keep,
    NewInMainHand,
    NewInMainHandOldToOffHand,
    NewInOffHand,
    NewInOffHandOldToMainHand
};

void AutoEquipToSlot(Player* bot, Item* item, uint8 slot)
{
    WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
    ObjectGuid itemGuid = item->GetGUID();
    packet << itemGuid << slot;
    WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(packet));
    nicePacket.Read();
    bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);
}
}  // namespace

bool EquipAction::Execute(Event event)
{
    std::string const text = event.getParam();
    ItemIds ids = chat->parseItems(text);
    EquipItems(ids);
    return true;
}

void EquipAction::EquipItems(ItemIds ids)
{
    for (ItemIds::iterator i = ids.begin(); i != ids.end(); i++)
    {
        FindItemByIdVisitor visitor(*i);
        EquipItem(&visitor);
    }
}

// Return bagslot with smalest bag, or 0 when there is nowhere we may put one.
uint8 EquipAction::GetSmallestBagSlot()
{
    uint8 curBag = 0;
    uint32 curSlots = 0;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!item)
            return bag;

        ItemTemplate const* proto = item->GetTemplate();
        // Never swap a plain bag over a quiver, ammo pouch or profession bag. Must stay in step
        // with ItemUsageValue::GetSmallestBagSize, which decides whether we want the bag at all.
        if (proto->Class != ITEM_CLASS_CONTAINER || proto->SubClass != ITEM_SUBCLASS_CONTAINER)
            continue;

        uint32 size = ((Bag const*)item)->GetBagSize();
        if (!curBag || size < curSlots)
        {
            curBag = bag;
            curSlots = size;
        }
    }

    return curBag;
}

void EquipAction::EquipItem(FindItemVisitor* visitor)
{
    IterateItems(visitor);
    std::vector<Item*> items = visitor->GetResult();
    if (!items.empty())
        EquipItem(*items.begin());
}

void EquipAction::EquipItem(Item* item)
{
    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();
    ItemTemplate const* itemProto = item->GetTemplate();
    uint32 itemId = itemProto->ItemId;
    uint8 invType = itemProto->InventoryType;

    // Handle ammunition separately
    if (invType == INVTYPE_AMMO)
    {
        if (!RangedWeaponNeedsAmmo(bot))
            return;

        bot->SetAmmo(itemId);
        std::ostringstream out;
        out << "equipping " << chat->FormatItem(itemProto);
        botAI->TellMaster(out);
        return;
    }

    // Handle bags first
    bool equippedBag = false;
    if (itemProto->Class == ITEM_CLASS_CONTAINER)
    {
        // Attempt to equip as a bag
        uint8 newBagSlot = GetSmallestBagSlot();

        if (newBagSlot > 0)
        {
            uint16 src = ((bagIndex << 8) | slot);
            uint16 dst = ((INVENTORY_SLOT_BAG_0 << 8) | newBagSlot);
            bot->SwapItem(src, dst);
            equippedBag = true;
        }
    }

    // If we didn't equip as a bag, try to equip as gear
    if (!equippedBag)
    {
        // Ranged weapons aren't handled by the rest of the weapon equip logic
        // Handle them early here to avoid issues.
        if (invType == INVTYPE_RANGED || invType == INVTYPE_THROWN || invType == INVTYPE_RANGEDRIGHT)
        {
            WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
            ObjectGuid itemguid = item->GetGUID();
            packet << itemguid << uint8(EQUIPMENT_SLOT_RANGED);

            WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(packet));
            nicePacket.Read();
            bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);

            std::ostringstream out;
            out << "Equipping " << chat->FormatItem(itemProto) << " in ranged slot";
            botAI->TellMaster(out);
            return;
        }

        uint8 dstSlot = botAI->FindEquipSlot(itemProto, NULL_SLOT, true);

        // Check if the item is a weapon and whether the bot can dual wield or use Titan Grip
        bool isWeapon = (itemProto->Class == ITEM_CLASS_WEAPON);
        bool canTitanGrip = bot->CanTitanGrip();
        bool canDualWield = bot->CanDualWield();

        bool isTwoHander = (invType == INVTYPE_2HWEAPON);
        bool isValidTGWeapon = false;
        if (canTitanGrip && isTwoHander)
        {
            // Titan Grip-valid 2H weapon subclasses: Axe2, Mace2, Sword2
            isValidTGWeapon = (itemProto->SubClass == ITEM_SUBCLASS_WEAPON_AXE2 ||
                               itemProto->SubClass == ITEM_SUBCLASS_WEAPON_MACE2 ||
                               itemProto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD2);
        }

        // Check if the main hand currently has a 2H weapon equipped
        Item* currentMHItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        bool have2HWeaponEquipped = (currentMHItem && currentMHItem->GetTemplate()->InventoryType == INVTYPE_2HWEAPON);

        // bool canDualWieldOrTG = (canDualWield || (canTitanGrip && isTwoHander));
        bool canDualWieldOrTG = (canDualWield || isTwoHander);

        // If this is a weapon and we can dual wield or Titan Grip, check if we can improve main/off-hand setup
        if (isWeapon && canDualWieldOrTG)
        {
            Item* mainHandItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            Item* offHandItem  = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

            StatsWeightCalculator calculator(bot);
            calculator.SetItemSetBonus(sPlayerbotAIConfig.itemSetUseForUpgrades);
            calculator.SetOverflowPenalty(false);

            auto isValidTG = [canTitanGrip](ItemTemplate const* proto)
            {
                return canTitanGrip && proto->InventoryType == INVTYPE_2HWEAPON &&
                       (proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE2 ||
                        proto->SubClass == ITEM_SUBCLASS_WEAPON_MACE2 ||
                        proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD2);
            };
            // a 2H that leaves no room for anything in the off hand
            auto isLoneTwoHander = [&](ItemTemplate const* proto)
            {
                return proto->InventoryType == INVTYPE_2HWEAPON && !isValidTG(proto);
            };
            auto canGoMainHand = [&](ItemTemplate const* proto)
            {
                if (proto->InventoryType == INVTYPE_2HWEAPON)
                    return !canTitanGrip || isValidTG(proto);
                return proto->InventoryType == INVTYPE_WEAPON || proto->InventoryType == INVTYPE_WEAPONMAINHAND;
            };
            auto canGoOffHand = [&](ItemTemplate const* proto)
            {
                return proto->InventoryType == INVTYPE_WEAPON || proto->InventoryType == INVTYPE_WEAPONOFFHAND ||
                       isValidTG(proto);
            };

            auto score = [&calculator](Item* weapon, uint8 slot)
            {
                return weapon ? calculator.CalculateItem(weapon->GetTemplate()->ItemId,
                                                         weapon->GetItemRandomPropertyId(), slot)
                              : 0.0f;
            };
            // both setups scored with the leaving weapon treated as removed, so its set bonus and
            // capped ratings count for neither side
            auto gainOver = [&](Item* main, Item* off, Item* leaving)
            {
                calculator.SetReplacedItem(leaving);
                float const current =
                    score(mainHandItem, EQUIPMENT_SLOT_MAINHAND) + score(offHandItem, EQUIPMENT_SLOT_OFFHAND);
                return score(main, EQUIPMENT_SLOT_MAINHAND) + score(off, EQUIPMENT_SLOT_OFFHAND) - current;
            };

            WeaponLayout best = WeaponLayout::Keep;
            float bestGain = 0.0f;
            auto consider = [&](WeaponLayout layout, Item* main, Item* off, Item* leaving)
            {
                float const gain = gainOver(main, off, leaving);
                if (gain > bestGain)
                {
                    bestGain = gain;
                    best = layout;
                }
            };

            // A lone 2H pushes the off hand out, but the off hand stays on both sides here and cancels.
            // Keeps this in line with ItemUsageValue, which only weighs a 2H against the main hand.
            if (canGoMainHand(itemProto))
                consider(WeaponLayout::NewInMainHand, item, offHandItem, mainHandItem);

            if (canGoMainHand(itemProto) && !isLoneTwoHander(itemProto) && mainHandItem &&
                canGoOffHand(mainHandItem->GetTemplate()))
                consider(WeaponLayout::NewInMainHandOldToOffHand, item, mainHandItem, offHandItem);

            // with the main hand empty, a weapon that fits there goes there
            if (canGoOffHand(itemProto) &&
                (mainHandItem ? !isLoneTwoHander(mainHandItem->GetTemplate()) : !canGoMainHand(itemProto)))
                consider(WeaponLayout::NewInOffHand, mainHandItem, item, offHandItem);

            if (canGoOffHand(itemProto) && offHandItem && canGoMainHand(offHandItem->GetTemplate()))
                consider(WeaponLayout::NewInOffHandOldToMainHand, offHandItem, item, mainHandItem);

            switch (best)
            {
                case WeaponLayout::Keep:
                    return;
                case WeaponLayout::NewInMainHand:
                    AutoEquipToSlot(bot, item, EQUIPMENT_SLOT_MAINHAND);
                    break;
                case WeaponLayout::NewInMainHandOldToOffHand:
                    // the old main hand drops into the new weapon's bag slot first
                    AutoEquipToSlot(bot, item, EQUIPMENT_SLOT_MAINHAND);
                    if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND) == item)
                    {
                        AutoEquipToSlot(bot, mainHandItem, EQUIPMENT_SLOT_OFFHAND);

                        std::ostringstream moveMsg;
                        moveMsg << "Main hand upgrade found. Moving " << chat->FormatItem(mainHandItem->GetTemplate())
                                << " to offhand";
                        botAI->TellMaster(moveMsg);
                    }
                    break;
                case WeaponLayout::NewInOffHand:
                    AutoEquipToSlot(bot, item, EQUIPMENT_SLOT_OFFHAND);
                    break;
                case WeaponLayout::NewInOffHandOldToMainHand:
                    // the old off hand drops into the new weapon's bag slot first
                    AutoEquipToSlot(bot, item, EQUIPMENT_SLOT_OFFHAND);
                    if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) == item)
                    {
                        AutoEquipToSlot(bot, offHandItem, EQUIPMENT_SLOT_MAINHAND);

                        std::ostringstream moveMsg;
                        moveMsg << "Offhand upgrade found. Moving " << chat->FormatItem(offHandItem->GetTemplate())
                                << " to main hand";
                        botAI->TellMaster(moveMsg);
                    }
                    break;
            }

            bool const inMainHand =
                best == WeaponLayout::NewInMainHand || best == WeaponLayout::NewInMainHandOldToOffHand;
            std::ostringstream out;
            out << "Equipping " << chat->FormatItem(itemProto) << (inMainHand ? " in main hand" : " in offhand");
            botAI->TellMaster(out);
            return;
        }

        // If not a special dual-wield/TG scenario or no improvement found, fall back to original logic
        if (dstSlot == EQUIPMENT_SLOT_FINGER1 ||
            dstSlot == EQUIPMENT_SLOT_TRINKET1 ||
            (dstSlot == EQUIPMENT_SLOT_MAINHAND && canDualWield &&
                ((invType != INVTYPE_2HWEAPON && !have2HWeaponEquipped) || (canTitanGrip && isValidTGWeapon))))
        {
            // Handle ring/trinket dual-slot logic
            Item* const equippedItems[2] = {
                bot->GetItemByPos(INVENTORY_SLOT_BAG_0, dstSlot),
                bot->GetItemByPos(INVENTORY_SLOT_BAG_0, dstSlot + 1)
            };

            if (equippedItems[0])
            {
                if (equippedItems[1])
                {
                    // Both slots are full - pick the worst item to replace, but only if new item is better
                    StatsWeightCalculator calc(bot);
                    calc.SetItemSetBonus(sPlayerbotAIConfig.itemSetUseForUpgrades);
                    calc.SetOverflowPenalty(false);

                    int32 newItemRandomProp = item->GetItemRandomPropertyId();
                    int32 firstRandomProp = equippedItems[0]->GetItemRandomPropertyId();
                    int32 secondRandomProp = equippedItems[1]->GetItemRandomPropertyId();

                    // Score the candidate once per slot, each time with the piece it would displace
                    // treated as removed, so the incumbent's set bonus and capped ratings count for it only.
                    calc.SetReplacedItem(equippedItems[0]);
                    float newItemScoreVsFirst = calc.CalculateItem(itemId, newItemRandomProp, dstSlot);
                    float firstItemScore = calc.CalculateItem(equippedItems[0]->GetTemplate()->ItemId, firstRandomProp,
                                                              dstSlot);

                    calc.SetReplacedItem(equippedItems[1]);
                    float newItemScoreVsSecond = calc.CalculateItem(itemId, newItemRandomProp, dstSlot + 1);
                    float secondItemScore = calc.CalculateItem(equippedItems[1]->GetTemplate()->ItemId, secondRandomProp,
                                                               dstSlot + 1);
                    calc.SetReplacedItem(nullptr);

                    // Determine which slot (if any) should be replaced
                    bool betterThanFirst = newItemScoreVsFirst > firstItemScore;
                    bool betterThanSecond = newItemScoreVsSecond > secondItemScore;

                    // Early return if new item is not better than either equipped item
                    if (!betterThanFirst && !betterThanSecond)
                        return;

                    if (betterThanFirst && betterThanSecond)
                    {
                        // New item is better than both - replace the worse of the two equipped items
                        if (firstItemScore > secondItemScore)
                            dstSlot++; // Replace second slot (worse)
                        // else: keep dstSlot as-is (replace first slot)
                    }
                    else if (betterThanSecond)
                        dstSlot++; // Only better than second slot - replace it
                }
                else
                {
                    // Second slot empty, use it
                    dstSlot++;
                }
            }
        }

        // Equip the item in the chosen slot
        {
            WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
            ObjectGuid itemguid = item->GetGUID();
            packet << itemguid << dstSlot;
            WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(packet));
            nicePacket.Read();
            bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);
        }
    }

    std::ostringstream out;
    out << "Equipping " << chat->FormatItem(itemProto);
    botAI->TellMaster(out);
}

ItemIds EquipAction::SelectInventoryItemsToEquip()
{
    CollectItemsVisitor visitor;
    IterateItems(&visitor, ITERATE_ITEMS_IN_BAGS);

    ItemIds items;
    for (auto i = visitor.items.begin(); i != visitor.items.end(); ++i)
    {
        Item* item = *i;
        if (!item)
            continue;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (!itemTemplate)
            continue;

        //TODO Expand to Glyphs and Gems, that can be placed in equipment
        //Pre-filter non-equipable items
        if (itemTemplate->InventoryType == INVTYPE_NON_EQUIP)
            continue;

        int32 randomProperty = item->GetItemRandomPropertyId();
        uint32 itemId = item->GetTemplate()->ItemId;

        std::string const itemUsageParam = ItemUsageValue::BuildItemUsageParam(itemId, randomProperty);

        ItemUsage usage = AI_VALUE2(ItemUsage, "item upgrade", itemUsageParam);

        // Warriors/rogues only use the ranged slot as a stat stick: a BAD_EQUIP (zero-score) gun/bow
        // contributes nothing, so don't fill an empty slot with it. Wands are excluded by class.
        if (usage == ITEM_USAGE_BAD_EQUIP && itemTemplate->IsRangedWeapon() &&
            (bot->getClass() == CLASS_WARRIOR || bot->getClass() == CLASS_ROGUE))
            continue;

        if (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE || usage == ITEM_USAGE_BAD_EQUIP)
            items.insert(itemId);
    }
    return items;
}

bool EquipUpgradesPacketAction::Execute(Event event)
{
    if (!sPlayerbotAIConfig.autoEquipUpgradeLoot && !sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;
    std::string const source = event.GetSource();
    if (source == "trade status")
    {
        WorldPacket p(event.getPacket());
        p.rpos(0);
        uint32 status;
        p >> status;

        if (status != TRADE_STATUS_TRADE_ACCEPT)
            return false;
    }

    else if (source == "item push result")
    {
        WorldPacket p(event.getPacket());
        p.rpos(0);
        ObjectGuid playerGuid;
        uint32 received, created, sendChatMessage, itemSlot, itemId;
        uint8 bagSlot;

        p >> playerGuid;
        p >> received;
        p >> created;
        p >> sendChatMessage;
        p >> bagSlot;
        p >> itemSlot;
        p >> itemId;

        ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
        if (!item || item->InventoryType == INVTYPE_NON_EQUIP)
            return false;
    }

    ItemIds items = SelectInventoryItemsToEquip();
    EquipItems(items);
    return true;
}

bool EquipUpgradeAction::Execute(Event /*event*/)
{
    ItemIds items = SelectInventoryItemsToEquip();
    EquipItems(items);
    return true;
}
