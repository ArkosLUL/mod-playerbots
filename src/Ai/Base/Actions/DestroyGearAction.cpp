/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DestroyGearAction.h"
#include "Event.h"
#include "ItemUsageValue.h"
#include "ItemVisitors.h"
#include "Playerbots.h"
#include "RandomItemMgr.h"

#include <cstdlib>
#include <sstream>

namespace
{
// A 25-man raid running this at once would otherwise whisper hundreds of lines.
constexpr uint32 MAX_PREVIEW_LINES = 10;
}

bool DestroyGearAction::Execute(Event event)
{
    uint32 margin = 0;
    bool confirm = false;
    if (!ParseParams(event.getParam(), margin, confirm))
    {
        botAI->TellError("Usage: dg <ilvl margin> [confirm]");
        return false;
    }

    std::vector<Candidate> candidates = Collect(margin);

    if (candidates.empty())
    {
        std::ostringstream out;
        out << "No gear more than " << margin << " ilvl below equipped.";
        botAI->TellMaster(out);
        return true;
    }

    if (!confirm)
    {
        std::ostringstream head;
        head << "Gear cleanup, more than " << margin << " ilvl below equipped: " << candidates.size() << " items";
        botAI->TellMaster(head);

        for (size_t i = 0; i < candidates.size() && i < MAX_PREVIEW_LINES; ++i)
        {
            ItemTemplate const* proto = candidates[i].item->GetTemplate();
            std::ostringstream line;
            line << "  " << chat->FormatItem(proto) << " " << proto->ItemLevel << " (equipped "
                 << candidates[i].baseline << ")";
            botAI->TellMaster(line);
        }

        if (candidates.size() > MAX_PREVIEW_LINES)
        {
            std::ostringstream more;
            more << "  ... +" << (candidates.size() - MAX_PREVIEW_LINES) << " more";
            botAI->TellMaster(more);
        }

        std::ostringstream tail;
        tail << "Say 'dg " << margin << " confirm' to destroy.";
        botAI->TellMaster(tail);
        return true;
    }

    for (Candidate const& candidate : candidates)
        bot->DestroyItem(candidate.item->GetBagSlot(), candidate.item->GetSlot(), true);

    std::ostringstream out;
    out << "Destroyed " << candidates.size() << " gear items more than " << margin << " ilvl below equipped.";
    botAI->TellMaster(out);
    return true;
}

bool DestroyGearAction::ParseParams(std::string const& param, uint32& margin, bool& confirm)
{
    std::istringstream in(param);

    std::string marginToken;
    if (!(in >> marginToken))
        return false;

    // Length cap keeps a silly number from wrapping into a small margin through the uint32 cast.
    if (marginToken.size() > 4 || marginToken.find_first_not_of("0123456789") != std::string::npos)
        return false;

    margin = static_cast<uint32>(strtoul(marginToken.c_str(), nullptr, 10));

    std::string token;
    if (in >> token)
    {
        if (token != "confirm")
            return false;

        confirm = true;
    }

    return !(in >> token);
}

bool DestroyGearAction::IsProtected(ItemTemplate const* proto) const
{
    // The core refuses to destroy these, so skipping them avoids reporting a destroy that never
    // happened.
    if (proto->HasFlag(ITEM_FLAG_NO_USER_DESTROY))
        return true;

    // Heirlooms carry a fixed low template item level, so a plain ilvl rule would eat every one.
    if (proto->HasFlag(ITEM_FLAG_IS_BOUND_TO_ACCOUNT))
        return true;

    if (proto->Quality >= ITEM_QUALITY_LEGENDARY)
        return true;

    if (proto->StartQuest)
        return true;

    // Shirts and tabards are cosmetic - their item level says nothing about how good they are.
    return proto->InventoryType == INVTYPE_TABARD || proto->InventoryType == INVTYPE_BODY;
}

bool DestroyGearAction::IsNeededForQuest(Item* item) const
{
    std::string const usageParam = ItemUsageValue::BuildItemUsageParam(item->GetTemplate()->ItemId,
                                                                      item->GetItemRandomPropertyId());

    return AI_VALUE2(ItemUsage, "item usage", usageParam) == ITEM_USAGE_QUEST;
}

bool DestroyGearAction::SlotBaseline(ItemTemplate const* proto, uint32& baseline) const
{
    std::vector<EquipmentSlots> const* slots =
        sRandomItemMgr.GetViableSlots(static_cast<InventoryType>(proto->InventoryType));
    if (!slots)
        return false;

    bool found = false;
    for (EquipmentSlots slot : *slots)
    {
        Item* equipped = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!equipped)
            continue;

        // Lowest of the viable slots: a bagged ring only counts as junk if it loses to the weaker
        // of the two the bot already wears.
        uint32 const level = equipped->GetTemplate()->ItemLevel;
        if (!found || level < baseline)
            baseline = level;

        found = true;
    }

    return found;
}

std::vector<DestroyGearAction::Candidate> DestroyGearAction::Collect(uint32 margin)
{
    CollectItemsVisitor visitor;
    IterateItems(&visitor, ITERATE_ITEMS_IN_BAGS);

    std::vector<Candidate> candidates;
    for (Item* item : visitor.items)
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
            continue;

        if (IsProtected(proto))
            continue;

        uint32 baseline = 0;
        if (!SlotBaseline(proto, baseline))
            continue;

        if (baseline <= proto->ItemLevel || baseline - proto->ItemLevel <= margin)
            continue;

        // Last, because it runs the full item-usage calculation - only worth paying for an item
        // that is otherwise already condemned.
        if (IsNeededForQuest(item))
            continue;

        candidates.push_back({item, baseline});
    }

    return candidates;
}
