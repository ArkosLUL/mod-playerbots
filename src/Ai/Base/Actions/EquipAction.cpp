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
#include <utility>

namespace
{
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
    EquipItems(ids, true);
    return true;
}

void EquipAction::EquipItems(ItemIds ids, bool force)
{
    for (ItemIds::iterator i = ids.begin(); i != ids.end(); i++)
    {
        FindItemByIdVisitor visitor(*i);
        EquipItem(&visitor, force);
    }
}

void EquipAction::EquipItem(FindItemVisitor* visitor, bool force)
{
    IterateItems(visitor);
    std::vector<Item*> items = visitor->GetResult();
    if (!items.empty())
        EquipItem(*items.begin(), force);
}

void EquipAction::EquipItem(Item* item, bool force)
{
    ItemTemplate const* itemProto = item->GetTemplate();

    if (itemProto->InventoryType == INVTYPE_AMMO)
    {
        if (!RangedWeaponNeedsAmmo(bot))
            return;

        bot->SetAmmo(itemProto->ItemId);
        std::ostringstream out;
        out << "equipping " << chat->FormatItem(itemProto);
        botAI->TellMaster(out);
        return;
    }

    GearChoice const choice = EvaluateGear(botAI, itemProto, item->GetItemRandomPropertyId(), item, force);
    if (choice.usage != ITEM_USAGE_EQUIP && choice.usage != ITEM_USAGE_REPLACE && choice.usage != ITEM_USAGE_BAD_EQUIP)
        return;

    if (itemProto->Class == ITEM_CLASS_CONTAINER)
    {
        bot->SwapItem((item->GetBagSlot() << 8) | item->GetSlot(), (INVENTORY_SLOT_BAG_0 << 8) | choice.slot);
        std::ostringstream out;
        out << "Equipping " << chat->FormatItem(itemProto);
        botAI->TellMaster(out);
        return;
    }

    // taken before the first move, which drops it into the new item's bag slot
    Item* moving = choice.incumbentTo != NULL_SLOT ? bot->GetItemByPos(INVENTORY_SLOT_BAG_0, choice.slot) : nullptr;

    AutoEquipToSlot(bot, item, choice.slot);

    if (moving && bot->GetItemByPos(INVENTORY_SLOT_BAG_0, choice.slot) == item)
    {
        AutoEquipToSlot(bot, moving, choice.incumbentTo);

        std::ostringstream moveMsg;
        if (choice.slot == EQUIPMENT_SLOT_MAINHAND)
            moveMsg << "Main hand upgrade found. Moving " << chat->FormatItem(moving->GetTemplate()) << " to offhand";
        else
            moveMsg << "Offhand upgrade found. Moving " << chat->FormatItem(moving->GetTemplate()) << " to main hand";
        botAI->TellMaster(moveMsg);
    }

    std::ostringstream out;
    out << "Equipping " << chat->FormatItem(itemProto);
    if (choice.slot == EQUIPMENT_SLOT_MAINHAND && itemProto->Class == ITEM_CLASS_WEAPON)
        out << " in main hand";
    else if (choice.slot == EQUIPMENT_SLOT_OFFHAND && itemProto->Class == ITEM_CLASS_WEAPON)
        out << " in offhand";
    else if (choice.slot == EQUIPMENT_SLOT_RANGED && itemProto->InventoryType != INVTYPE_RELIC)
        out << " in ranged slot";
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
    if (!sPlayerbotAIConfig.AutoEquipUpgradeLoot && !sRandomPlayerbotMgr.IsRandomBot(bot))
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
    EquipItems(items, false);
    return true;
}

bool EquipUpgradeAction::Execute(Event /*event*/)
{
    ItemIds items = SelectInventoryItemsToEquip();
    EquipItems(items, false);
    return true;
}
