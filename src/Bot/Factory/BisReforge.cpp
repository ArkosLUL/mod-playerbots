/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BisReforge.h"

#if __has_include("item_reforge.h")

// item_reforge.h uses these without including them
#include <string>
#include <unordered_map>
#include <vector>
#include "Item.h"
#include "Player.h"
#include "item_reforge.h"

#include "Log.h"
#include "ObjectAccessor.h"
#include "PlayerbotOperation.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Timer.h"
#include <memory>

namespace
{
// Two writes to one item's reforge close together are two async statements on the same
// character_reforging row, and several DB worker threads can run them out of order.
constexpr uint32 REFORGE_WRITE_GAP_MS = 10 * IN_MILLISECONDS;

// item guid counter -> getMSTime() of our last write to its reforge, world thread only
std::unordered_map<uint32, uint32> lastReforgeWrites;

bool ReforgeWriteAllowed(uint32 itemGuidLow, uint32 now)
{
    auto const last = lastReforgeWrites.find(itemGuidLow);
    return last == lastReforgeWrites.end() || getMSTimeDiff(last->second, now) >= REFORGE_WRITE_GAP_MS;
}

void RecordReforgeWrite(uint32 itemGuidLow, uint32 now)
{
    std::erase_if(lastReforgeWrites,
                  [now](auto const& write) { return getMSTimeDiff(write.second, now) >= REFORGE_WRITE_GAP_MS; });
    lastReforgeWrites[itemGuidLow] = now;
}

// mod-reforging keeps its data in a map every player's item updates read, so all of this stays
// on the world thread, map threads only queue it
class BisReforgeOperation : public PlayerbotOperation
{
public:
    BisReforgeOperation(ObjectGuid botGuid, ObjectGuid itemGuid, uint32 entry, uint32 from, uint32 to)
        : m_botGuid(botGuid), m_itemGuid(itemGuid), m_entry(entry), m_from(from), m_to(to)
    {
    }

    bool Execute() override
    {
        Player* bot = ObjectAccessor::FindPlayer(m_botGuid);
        if (!bot)
            return false;

        Item* item = bot->GetItemByGuid(m_itemGuid);
        if (!item || !item->IsEquipped() || item->GetEntry() != m_entry)
        {
            LOG_DEBUG("playerbots", "BisReforgeOperation: {} no longer wears item {}", bot->GetName(), m_entry);
            return false;
        }

        ItemReforge* reforging = sItemReforge;
        if (!reforging->GetEnabled())
            return false;

        if (m_from && (!reforging->IsReforgeableStat(m_from) || !reforging->IsReforgeableStat(m_to)))
        {
            LOG_DEBUG("playerbots", "BisReforgeOperation: {} -> {} is not reforgeable here (item {}, bot {})",
                      m_from, m_to, m_entry, bot->GetName());
            return false;
        }

        // only bool tests and -> on data: the pointer and std::optional versions of mod-reforging
        // both support those and nothing else in common
        auto data = reforging->GetReforgingData(item);
        if (data && data->stat_decrease == m_from && data->stat_increase == m_to)
            return true;

        if (!data && !m_from)
            return true;

        // two ApplyEnchantAndGemsNew runs can queue requests for one item into the same batch
        uint32 const now = getMSTime();
        if (!ReforgeWriteAllowed(m_itemGuid.GetCounter(), now))
        {
            LOG_DEBUG("playerbots", "BisReforgeOperation: item {} (bot {}) had its reforge changed moments ago, skipped",
                      m_entry, bot->GetName());
            return false;
        }

        // a different reforge is only removed here, a later request sets the new one
        if (data)
        {
            if (!reforging->RemoveReforge(bot, item))
            {
                LOG_DEBUG("playerbots", "BisReforgeOperation: could not remove the reforge on item {} (bot {})",
                          m_entry, bot->GetName());
                return false;
            }
        }
        else if (!reforging->Reforge(bot, m_itemGuid, m_from, m_to))
        {
            LOG_DEBUG("playerbots", "BisReforgeOperation: reforge {} -> {} refused on item {} (bot {})", m_from, m_to,
                      m_entry, bot->GetName());
            return false;
        }

        RecordReforgeWrite(m_itemGuid.GetCounter(), now);
        return true;
    }

    ObjectGuid GetBotGuid() const override { return m_botGuid; }
    std::string GetName() const override { return "BisReforgeOperation"; }
    bool IsValid() const override { return ObjectAccessor::FindPlayer(m_botGuid) != nullptr; }

private:
    ObjectGuid m_botGuid;
    ObjectGuid m_itemGuid;
    uint32 m_entry;
    uint32 m_from;
    uint32 m_to;
};
}  // namespace

void RequestBisReforge(Player* bot, Item* item, uint32 from, uint32 to)
{
    if (!bot || !item)
        return;

    auto op = std::make_unique<BisReforgeOperation>(bot->GetGUID(), item->GetGUID(), item->GetEntry(), from, to);
    if (!PlayerbotWorldThreadProcessor::instance().QueueOperation(std::move(op)))
        LOG_DEBUG("playerbots", "RequestBisReforge: world thread queue full, dropped item {} (bot {})",
                  item->GetEntry(), bot->GetName());
}

#else

void RequestBisReforge(Player* /*bot*/, Item* /*item*/, uint32 /*from*/, uint32 /*to*/) {}

#endif
