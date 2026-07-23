# Plan: Gear-only loot strategy for bots

## Context

Bots currently loot almost everything with a sell price — scrolls, food, water, recipes, crafting
materials, vendor junk. This clutters bot bags and pulls loot the owner does not care about. Goal:
a mode where bots loot **only gear pieces** (armor + weapons), uncommon quality and up, while still
honoring quest items and the always-loot whitelist.

### How looting works today (investigation result)

- Per-item decision: `StoreLootAction::IsLootAllowed(itemid, botAI)` —
  [LootAction.cpp:466](../src/Ai/Base/Actions/LootAction.cpp#L466).
  Order: always-loot whitelist → `MaxCount` cap → `StartQuest` → active-quest required items →
  fallback `lootStrategy->CanLoot(proto, context)`.
- Strategies: [LootStrategyValue.cpp](../src/Ai/Base/Value/LootStrategyValue.cpp) —
  `NormalLootStrategy` (delegates to `ItemUsageValue`, which classifies food/scrolls/reagents/junk as
  useful via SKILL/USE/KEEP and a SellPrice→AH/VENDOR fallback), `GrayLootStrategy`,
  `DisenchantLootStrategy`, `AllLootStrategy`. Base class `LootStrategy` in
  [LootObjectStack.h](../src/Mgr/Item/LootObjectStack.h#L17).
- Strategy selection: `LootStrategyValue::instance(name)` (string → strategy). Default hardcoded
  `normal` in [LootStrategyValue.h:18](../src/Ai/Base/Value/LootStrategyValue.h#L18).
  Set per-bot at runtime by the master via the `ll` chat command
  ([LootStrategyAction.cpp:53](../src/Ai/Base/Actions/LootStrategyAction.cpp#L53)).
- **No** global config default and **no** gear-only mode exist today.

Because quest/whitelist/StartQuest are checked *before* the strategy, a gear-only strategy still
loots quest items and anything on the always-loot list — no special handling needed for those.

## Approach

Add a new `EquipLootStrategy` (token `equip`/`gear`) that loots only armor + weapons of uncommon+
quality, and a new `AiPlayerbot.LootStrategy` config that sets the default strategy for all bots.
It does **not** inherit `NormalLootStrategy`, so food/water/scrolls/recipes/reagents/vendor-junk are
all excluded.

### Files to change

1. **[LootStrategyValue.cpp](../src/Ai/Base/Value/LootStrategyValue.cpp)**
   - Add class (matching existing style; re-read exact file first — prior read was lossy):
     ```cpp
     class EquipLootStrategy : public LootStrategy
     {
     public:
         bool CanLoot(ItemTemplate const* proto, AiObjectContext* /*context*/) override
         {
             return (proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON) &&
                    proto->Quality >= ITEM_QUALITY_UNCOMMON;
         }
         std::string const GetName() override { return "equip"; }
     };
     ```
   - Add static definition: `LootStrategy* LootStrategyValue::equip = new EquipLootStrategy();`
   - Add branch in `instance()`: `if (strategy == "equip" || strategy == "gear" || strategy == "eq") return equip;`

2. **[LootStrategyValue.h](../src/Ai/Base/Value/LootStrategyValue.h)**
   - Declare `static LootStrategy* equip;`.
   - Make the default config-driven: `#include "PlayerbotAIConfig.h"` and change the constructor
     initializer to `ManualSetValue<LootStrategy*>(botAI, LootStrategyValue::instance(sPlayerbotAIConfig.lootStrategy), name)`.
     (Verify no circular include; `instance` is declared in-class so the call is legal.)

3. **[PlayerbotAIConfig.h](../src/PlayerbotAIConfig.h)** — add field `std::string lootStrategy;`.

4. **[PlayerbotAIConfig.cpp](../src/PlayerbotAIConfig.cpp)** — in the LOOTING block
   (near line 342): `lootStrategy = sConfigMgr->GetOption<std::string>("AiPlayerbot.LootStrategy", "normal");`

5. **conf/playerbots.conf.dist** — document `AiPlayerbot.LootStrategy` in the LOOTING section
   (~lines 293-328). List accepted values: `normal`, `gray`, `disenchant`, `all`, `equip`. Default `normal`.

### Notes / edge cases

- `NormalLootStrategy` short-circuits `true` when the loot source is an item in the bot's bags
  (`lootGuid.IsItem()`) — e.g. opening a looted container. `EquipLootStrategy` intentionally omits
  this, so non-gear container contents are skipped under gear-only. Quest items from containers are
  still caught by the quest checks in `IsLootAllowed`. If we want containers to dump everything,
  add the same `IsItem()` early-return — leaving out per the "gear only" intent.
- Default stays `normal`; behavior unchanged unless the admin sets `AiPlayerbot.LootStrategy = equip`
  or a master runs `ll equip`.

## Verification

- Build the module (only if requested; builds are slow).
- Set `AiPlayerbot.LootStrategy = equip`, spawn a bot, kill mobs; confirm bot skips cloth/leather
  scraps, food, water, scrolls, recipes, gray/white gear, but loots uncommon+ armor/weapons and
  active quest items.
- Runtime toggle: `ll equip` / `ll normal` and query with `ll ?`; use `ll ?<item>` to preview
  will/won't-loot for specific items.
