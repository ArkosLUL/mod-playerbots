/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DESTROYGEARACTION_H
#define PLAYERBOTS_DESTROYGEARACTION_H

#include "InventoryAction.h"

#include <vector>

class Item;
class PlayerbotAI;

struct ItemTemplate;

// "destroygear <margin> [confirm]" - drops bagged weapons/armor sitting more than <margin> item
// levels below whatever the bot wears in that slot. Without "confirm" it only reports.
class DestroyGearAction : public InventoryAction
{
public:
    DestroyGearAction(PlayerbotAI* botAI) : InventoryAction(botAI, "destroygear") {}

    bool Execute(Event event) override;

private:
    struct Candidate
    {
        Item* item;
        uint32 baseline;
    };

    static bool ParseParams(std::string const& param, uint32& margin, bool& confirm);

    bool IsProtected(ItemTemplate const* proto) const;
    bool IsNeededForQuest(Item* item) const;
    bool SlotBaseline(ItemTemplate const* proto, uint32& baseline) const;
    std::vector<Candidate> Collect(uint32 margin);
};

#endif
