# Fix `InitBags`: upgrade existing bags without clobbering quivers

## Context

Bots get their four bag-slot containers from exactly one function,
`PlayerbotFactory::InitBags` (`src/Bot/Factory/PlayerbotFactory.cpp:2727`), which hardcodes item
**51809 "Portable Hole" (24 slots)**. It has two callers:

- `MaintenanceAction::Execute` — `src/Ai/Base/Actions/TrainerAction.cpp:189` (main bots) and `:215`
  (alt bots, gated on `AiPlayerbot.AltMaintenanceBags`) — both call `InitBags(false)`.
- `PlayerbotFactory::Randomize` — `src/Bot/Factory/PlayerbotFactory.cpp:849` — calls `InitBags()`
  i.e. `destroyOld = true`.

Current body:

```cpp
for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
{
    uint32 newItemId = 51809;
    Item* old_bag = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (old_bag && old_bag->GetTemplate()->ItemId == newItemId)
        continue;

    uint16 dest;
    if (!CanEquipUnseenItem(slot, dest, newItemId))
        continue;

    if (old_bag && destroyOld)
        bot->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);

    if (old_bag)
        continue;                       // <-- reached even after the destroy

    bot->EquipNewItem(dest, newItemId, true);
}
```

Two defects:

1. **No upgrade path.** The only skip test is item-id equality; any other occupied slot falls into
   `if (old_bag) continue;`. With `destroyOld = false` (the maintenance path) an existing bag is
   never replaced, however small it is.
2. **`destroyOld = true` empties the slot.** `DestroyItem` runs, then `if (old_bag) continue;` fires
   because `old_bag` is still a non-null (now stale) pointer, so `EquipNewItem` never runs. The
   `Randomize()` path therefore destroys the bag *and its contents* and leaves the slot empty.

Effect measured against the live `acore_characters` DB (`character_inventory`, `bag = 0`, slots
19-22):

| class | slot 19 contents | chars |
|---|---|---|
| Death Knight | 38145 Deathweave Bag (12 slots) | 53489 |
| Death Knight | 51809 Portable Hole (24 slots) | 120 |
| Hunter | 2101 Light Quiver / 2102 Small Ammo Pouch | 54355 |
| every other class | 51809 Portable Hole | ~125 each |

Death Knights start with four 12-slot Deathweave Bags from `CharStartOutfit`, so all four slots are
occupied at creation and `maintenance` never upgrades any of them. Every other class starts
bagless. Hunters start with a quiver or ammo pouch in slot 19 (slots 20-22 correctly get Portable
Holes) — that behaviour is **correct and must be preserved**.

### Intended outcome

`maintenance` upgrades any plain bag smaller than 51809 to 51809, moving the old bag's contents
into the new one, while never touching quivers, ammo pouches, or profession bags.

## Approach

### 1. Rewrite `PlayerbotFactory::InitBags` (`src/Bot/Factory/PlayerbotFactory.cpp:2727`)

Drop the `destroyOld` parameter — with contents preserved there is no longer a reason for the two
paths to differ, and `Randomize()` clears the inventory anyway. Update the declaration at
`PlayerbotFactory.h:87` and all three call sites (`PlayerbotFactory.cpp:849`,
`TrainerAction.cpp:189`, `TrainerAction.cpp:215`).

New logic, per bag slot:

```cpp
void PlayerbotFactory::InitBags()
{
    // Bags are INVTYPE_BAG, so CanUnequipItem refuses to move them in combat.
    if (bot->IsInCombat())
        return;

    uint32 const newItemId = 51809;
    ItemTemplate const* newProto = sObjectMgr->GetItemTemplate(newItemId);
    if (!newProto)
        return;

    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        Item* oldBag = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (oldBag)
        {
            ItemTemplate const* oldProto = oldBag->GetTemplate();
            // Quivers, ammo pouches and profession bags hold things a generic bag can't.
            if (oldProto->Class != ITEM_CLASS_CONTAINER || oldProto->SubClass != ITEM_SUBCLASS_CONTAINER)
                continue;
            if (oldProto->ContainerSlots >= newProto->ContainerSlots)
                continue;
        }

        uint16 dest;
        if (!CanEquipUnseenItem(slot, dest, newItemId))
            continue;

        if (!oldBag)
        {
            bot->EquipNewItem(dest, newItemId, true);
            continue;
        }

        // Park the new bag in the backpack, then let core's bag-exchange path in SwapItem
        // move the old bag's contents across.
        Item* newBag = StoreNewItemInInventorySlot(bot, newItemId, 1);
        if (!newBag)
            continue;

        uint8 srcBag = newBag->GetBagSlot();
        uint8 srcSlot = newBag->GetSlot();
        bot->SwapItem((srcBag << 8) | srcSlot, (INVENTORY_SLOT_BAG_0 << 8) | slot);
        // Whatever is left at the source is an empty bag: the old one on success, the new one if
        // the swap was refused.
        bot->DestroyItem(srcBag, srcSlot, true);
    }
}
```

Why this is safe — verified against `src/server/game/Entities/Player/PlayerStorage.cpp`:

- `StoreNewItemInInventorySlot` (`PlayerbotFactory.cpp:2605`, existing file-local helper) calls
  `CanStoreNewItem(INVENTORY_SLOT_BAG_0, NULL_SLOT, ...)`. With `bag == INVENTORY_SLOT_BAG_0` and
  `slot == NULL_SLOT`, `CanStoreItem` searches only `INVENTORY_SLOT_ITEM_START..INVENTORY_SLOT_ITEM_END`
  (`PlayerStorage.cpp:1341`) — the backpack. The new bag can never land inside the bag being
  replaced, so `SwapItem`'s self-nesting guards at `PlayerStorage.cpp:3686` and `:3693` cannot trip.
- `SwapItem` reaches the "bag swap with item exchange" branch at `PlayerStorage.cpp:3903`: the
  source bag is empty and not at a bag position, so it becomes `emptyBag` and the equipped bag
  becomes `fullBag`. Every item is checked with `ItemCanGoIntoBag` against the generic destination
  (always passes, `BagFamily == 0`), the count fits (24 >= 12), the items move, then the positions
  swap.
- `CanUnequipItem(dst, swap)` at `PlayerStorage.cpp:3715` is called with `swap = true` because the
  source bag is empty, so the `EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS` check at `:2130` is bypassed.
- `ItemTemplate::CanChangeEquipStateInCombat` (`ItemTemplate.h:705`) returns false for
  `INVTYPE_BAG`, hence the early `IsInCombat()` return; a bot in combat just gets upgraded on the
  next `maintenance`.

Failure modes are all no-ops that retry next run: backpack full, in combat, `CanEquipUnseenItem`
refuses.

### 2. Stop the loot path clobbering quivers and profession bags

`ItemUsageValue::GetSmallestBagSize` (`src/Ai/Base/Value/ItemUsageValue.cpp:1562`) starts its loop
at `INVENTORY_SLOT_BAG_START + 1`, skipping slot 19. That off-by-one is currently the only reason a
hunter's quiver survives the loot path — fixing it alone would make the quiver report as the
smallest bag and get swapped out. Fix both helpers together:

- `ItemUsageValue::GetSmallestBagSize` — start at `INVENTORY_SLOT_BAG_START`, `continue` past any
  slot whose item is not `ITEM_CLASS_CONTAINER` + `ITEM_SUBCLASS_CONTAINER`, keep returning `0` when
  a slot is genuinely empty, and return `std::numeric_limits<uint32>::max()` when every slot is
  occupied and none is replaceable, so `Calculate` yields `ITEM_USAGE_NONE`.
- `EquipAction::GetSmallestBagSlot` (`src/Ai/Base/Actions/EquipAction.cpp:35`) — apply the same
  `continue` guard, so a protected container is never returned as the swap target. Keep `0` as the
  "no slot" sentinel; `EquipAction::EquipItem:87` already treats `> 0` as success.

Both helpers must agree: a slot holding a quiver, ammo pouch, or profession bag is invisible to
both.

## Files to change

| File | Change |
|---|---|
| `src/Bot/Factory/PlayerbotFactory.cpp:2727` | Rewrite `InitBags` per above |
| `src/Bot/Factory/PlayerbotFactory.h:87` | Drop the `destroyOld` parameter |
| `src/Bot/Factory/PlayerbotFactory.cpp:849` | `InitBags()` call site |
| `src/Ai/Base/Actions/TrainerAction.cpp:189,215` | `InitBags(false)` -> `InitBags()` |
| `src/Ai/Base/Value/ItemUsageValue.cpp:1562` | Fix `GetSmallestBagSize` |
| `src/Ai/Base/Actions/EquipAction.cpp:35` | Fix `GetSmallestBagSlot` |

Out of scope by decision: the bag item id stays hardcoded as `51809`; no new config key.

## Verification

Static (available here):

1. `grep -rn "InitBags" src/` — exactly four hits, all with the new no-arg signature.
2. Confirm `ITEM_CLASS_CONTAINER`, `ITEM_SUBCLASS_CONTAINER`, `ITEM_CLASS_QUIVER` are already
   reachable in each edited translation unit (`ItemTemplate.h` comes in via the existing includes —
   `ItemUsageValue.cpp:1302` and `PlayerbotFactory.cpp:2047` already use these constants).

Runtime (needs a worldserver build — cannot be compiled headless in this workspace):

3. Baseline query, before the change:
   ```sql
   SELECT c.class, ii.itemEntry, COUNT(*) FROM characters c
     JOIN character_inventory ci ON ci.guid = c.guid
     JOIN item_instance ii ON ii.guid = ci.item
    WHERE ci.bag = 0 AND ci.slot BETWEEN 19 AND 22
    GROUP BY c.class, ii.itemEntry;
   ```
   (`mysqlsh --sql --uri root:password@localhost:3306/acore_characters`.)
4. In game, pick a DK bot with four Deathweave Bags (38145), put a marker item in one of them, run
   `maintenance` on it out of combat. Expect: all four slots become 51809, the marker item is still
   in the bot's inventory, no Deathweave Bag remains.
5. Repeat on a hunter bot. Expect: slot 19 keeps its quiver/ammo pouch (2101/2102), slots 20-22 stay
   51809, ammo still equipped (`InitAmmo` runs right after `InitBags` in
   `MaintenanceAction::Execute`).
6. Re-run the query from step 3. Expect item 38145 gone from slots 19-22, and the hunter quiver
   counts in slot 19 unchanged.
7. Run `maintenance` on a bot while it is in combat — expect no change and no error spam, then a
   successful upgrade once out of combat.
8. Check `AiPlayerbot.MaintenanceCommand` and `AiPlayerbot.AltMaintenanceBags` effective values with
   `docker exec ac-worldserver env | grep ^AC_` before concluding a bot "didn't upgrade".
