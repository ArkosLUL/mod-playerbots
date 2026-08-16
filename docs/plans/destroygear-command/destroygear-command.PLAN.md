# `destroygear` — bulk-destroy outdated gear from bot bags

## Context

Bots hoard gear. Autogear, loot rolls, and quest rewards keep pushing weapons and armor into bot
bags, and nothing ever clears them out. The existing tools don't cover it:

- `destroy [item link]` ([DestroyItemAction.cpp:12](src/Ai/Base/Actions/DestroyItemAction.cpp#L12))
  destroys one linked item at a time — useless for 20 bots × 15 stale items.
- `smart destroy` ([DestroyItemAction.cpp:42](src/Ai/Base/Actions/DestroyItemAction.cpp#L42)) only
  fires autonomously at >90% bag fill, and is disabled whenever the master is a real player.
- `s gray` / `s vendor` ([SellAction.cpp:60](src/Ai/Base/Actions/SellAction.cpp#L60)) needs a vendor
  in range and won't touch soulbound raid gear, which is most of the pile.

Goal: one chat command a player can whisper to a bot, or say in party/raid, that clears every piece
of gear in bags sitting more than X item levels below what the bot has equipped in that slot.

## Command

```
destroygear <X>            -> preview: list what would be destroyed
destroygear <X> confirm    -> destroy it
dg <X> [confirm]           -> same, short alias
```

`X` is the item-level margin. `dg 20` means "destroy bagged gear whose slot-equivalent equipped item
is more than 20 item levels better". `dg 0` destroys anything strictly worse.

Confirmation is **stateless** — `dg 20 confirm` recomputes from scratch, so there is no pending-state
to store per bot and no way for a stale preview to destroy the wrong thing. A player who already
knows what they want can skip straight to `dg 20 confirm`.

Whisper, party, raid, guild and custom channels all work with no extra code: every entry point in
[Playerbots.cpp:171-252](src/Script/Playerbots.cpp#L171-L252) converges on
`PlayerbotAI::HandleCommand`, and the party/raid hook already fans out to every bot in the group.

## Selection rule

Per bagged item:

1. **Class gate** — keep unless `Class` is `ITEM_CLASS_WEAPON` or `ITEM_CLASS_ARMOR`.
2. **Protection gate** — see below; protected items are never destroyed.
3. **Slot resolution** — `sRandomItemMgr->GetViableSlots((InventoryType)proto->InventoryType)`
   ([RandomItemMgr.cpp:3083](src/Mgr/Item/RandomItemMgr.cpp#L3083)). Returns
   `std::vector<EquipmentSlots> const*`, or `nullptr` for bags/quivers/ammo/non-equip — skip those.
   Rings, trinkets and one-handers map to two slots; two-handers map to mainhand only.
4. **Baseline** — the *lowest* `ItemLevel` among the viable slots that actually have something
   equipped. Taking the minimum is the conservative choice: a bagged ring only dies if it's worse
   than the *worse* of the two equipped rings. If none of the viable slots is filled, skip the item
   — there is no baseline to compare against, so we can't prove it's junk.
5. **Verdict** — destroy when `baseline - proto->ItemLevel > X`.

`GetViableSlots` was declared in a private helper block and had to be moved into the public section
of `RandomItemMgr.h` — it is a const lookup into a table built once at init, so there is nothing to
protect. Reusing it avoids writing a fifth InventoryType→slot mapping; the module already has
four ([PlayerbotAI::FindEquipSlot](src/Bot/PlayerbotAI.cpp#L6250),
[_fillGearScoreData](src/Bot/PlayerbotAI.cpp#L5122),
[GetPossibleInventoryTypeListBySlot](src/Bot/Factory/PlayerbotFactory.cpp#L5694), and
[EquipmentSlotByInvTypeSafe](src/Ai/Base/Value/ItemUsageValue.cpp#L2307)), and `GetViableSlots` is
the only data-driven one that covers cloak and tabard.

### Protection gate

| Check | Why |
|---|---|
| `proto->HasFlag(ITEM_FLAG_NO_USER_DESTROY)` | The core refuses to destroy these anyway; skipping avoids a bogus "destroyed" report. |
| `proto->HasFlag(ITEM_FLAG_IS_BOUND_TO_ACCOUNT)` | Heirlooms. Their template `ItemLevel` is low and fixed, so an iLvl rule would eat every one of them. |
| `proto->Quality >= ITEM_QUALITY_LEGENDARY` | Covers legendary (5), artifact (6), heirloom (7). |
| `proto->StartQuest != 0` | Item starts a quest. |
| `InventoryType` is `INVTYPE_TABARD` or `INVTYPE_BODY` | Cosmetic; item level is meaningless there. |
| `AI_VALUE2(ItemUsage, "item usage", …) == ITEM_USAGE_QUEST` | Needed for a quest the bot is on. Built via `ItemUsageValue::BuildItemUsageParam(itemId, randomPropertyId)`. |

Not protected, deliberately:

- **Unique-equipped items** (`ITEM_FLAG_UNIQUE_EQUIPPABLE`). That flag covers most raid trinkets and
  a lot of rings — precisely the items that stack up tier after tier and that this command exists to
  clear. They get judged on item level like everything else.
- **BiS-listed items** (`sBisListMgr->IsBisListed`). Something 20+ iLvl below what's already in that
  slot is by definition not this bot's BiS any more.

## Files

### New: `src/Ai/Base/Actions/DestroyGearAction.h` / `.cpp`

```cpp
class DestroyGearAction : public InventoryAction
{
public:
    DestroyGearAction(PlayerbotAI* botAI) : InventoryAction(botAI, "destroygear") {}

    bool Execute(Event event) override;

private:
    struct Candidate { Item* item; uint32 baseline; };

    bool ParseParams(std::string const& param, uint32& margin, bool& confirm) const;
    bool IsProtected(ItemTemplate const* proto) const;
    bool SlotBaseline(ItemTemplate const* proto, uint32& baseline) const;
    std::vector<Candidate> Collect(uint32 margin) const;
};
```

`Execute` walks bags with `CollectItemsVisitor` via
`IterateItems(&visitor, ITERATE_ITEMS_IN_BAGS)`
([InventoryAction.cpp:39](src/Ai/Base/Actions/InventoryAction.cpp#L39)), builds the candidate list,
then either reports it or destroys with
`bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true)` — the same call
`DestroyItemAction::DestroyItem` uses.

Do **not** reuse `DestroyItemAction::DestroyItem`: it takes a `FindItemVisitor` and re-walks the bags
per item, which would destroy every stack sharing an item id and re-report each one. Collect once,
destroy from the collected `Item*` list.

Collect all candidates *before* destroying any of them, since `IterateItems` walks live bag slots.
Destroy order does not matter: `Player::DestroyItem` clears the slot in place and never compacts a
bag, so earlier removals can't invalidate later positions.

Run the `ITEM_USAGE_QUEST` check last, after the class, flag, slot and margin filters. It goes
through `StatsWeightCalculator`, and a raid-wide `dg` would otherwise pay for it on every item in
every bot's bags rather than only on the ones already condemned.

### Output

Preview, one whisper block per bot:

```
Gear cleanup, 20+ ilvl below equipped: 7 items
  [Boots of the Aerie] 187 (equipped 213)
  [Bracers of Havok] 200 (equipped 226)
  ... +5 more
Say 'dg 20 confirm' to destroy.
```

Cap the item list at 10 lines with a `+N more` tail — a 25-man raid running `dg 20` otherwise dumps
hundreds of whispers. Confirm prints one summary line only:

```
Destroyed 7 gear items 20+ ilvl below equipped.
```

Nothing matched, either mode: `No gear more than 20 ilvl below equipped.` Bad or missing margin:
`TellError("Usage: dg <ilvl margin> [confirm]")` and return `false`.

Format item links with `chat->FormatItem(proto)`
([ChatHelper.h:46](src/Bot/Cmd/ChatHelper.h#L46)).

### Wiring — three sites, all required

1. **[src/Ai/Base/ChatActionContext.h](src/Ai/Base/ChatActionContext.h)** — add the include, then
   alongside the existing `destroy` entries at lines 151 and 257:
   ```cpp
   creators["destroygear"] = &ChatActionContext::destroygear;
   static Action* destroygear(PlayerbotAI* botAI) { return new DestroyGearAction(botAI); }
   ```

2. **[src/Ai/Base/ChatTriggerContext.h](src/Ai/Base/ChatTriggerContext.h)** — two entries, each with
   its **own** factory and its own internal trigger name:
   ```cpp
   creators["destroygear"] = &ChatTriggerContext::destroygear;
   creators["dg"]          = &ChatTriggerContext::dg;

   static Trigger* destroygear(PlayerbotAI* botAI) { return new ChatCommandTrigger(botAI, "destroygear"); }
   static Trigger* dg(PlayerbotAI* botAI)          { return new ChatCommandTrigger(botAI, "dg"); }
   ```
   This is the part that's easy to get wrong. `NamedObjectContext::create`
   ([NamedObjectContext.h:97](src/Bot/Engine/NamedObjectContext.h#L97)) caches by *lookup key*, so
   two keys pointing at one factory produce two separate trigger objects that share an internal
   name — and only the one whose internal name has a matching `TriggerNode` ever gets polled. That
   is exactly why `equip [item]` and `inventory [item]` silently do nothing today while `e` and
   `inv` work. Each alias needs a distinct internal name plus its own `TriggerNode`.

3. **[src/Ai/Base/Strategy/ChatCommandHandlerStrategy.cpp](src/Ai/Base/Strategy/ChatCommandHandlerStrategy.cpp)** —
   trigger name matches action name for the long form, so the constructor list is enough; the alias
   needs an explicit node:
   ```cpp
   // in InitTriggers, near the other aliases around line 42
   triggers.push_back(new TriggerNode("dg", { NextAction("destroygear", relevance) }));

   // in the constructor, near line 120
   supported.push_back("destroygear");
   ```

No build-file change: the module has no `CMakeLists.txt` and AzerothCore globs module sources, so
new files under `src/` are picked up on the next cmake configure. `help` picks up `destroygear`
automatically from `ChatActionContext::supports()`
([HelpAction.cpp:23](src/Ai/Base/Actions/HelpAction.cpp#L23)); the `dg` alias won't appear there,
which matches how every other alias behaves.

### Param parsing

`ExternalEventHelper::ParseChatCommand`
([ExternalEventHelper.cpp:12](src/Bot/Engine/ExternalEventHelper.cpp#L12)) splits right-to-left and
sets `param` to *everything* after the matched space, so `dg 20 confirm` arrives as
`name="dg", param="20 confirm"`. Parse with a leading integer plus an optional `confirm` token;
reject anything else.

## Verification

Static, before handing off (the module can't be compiled headless in this environment):

- Grep that `"destroygear"` appears in all three wiring files and that `"dg"` has both a
  `ChatTriggerContext` entry and a `TriggerNode`.
- Confirm the internal trigger names (`"destroygear"`, `"dg"`) match their `TriggerNode` names
  exactly — the alias bug above is silent at runtime, not a compile error.

In-game, after the user builds:

1. Whisper a geared bot `dg 20` → preview lists items with both item levels, no items disappear.
2. `inv` / `c` on that bot, then `dg 20 confirm`, then `inv` again → exactly the previewed items are
   gone, count matches the summary line.
3. `dg 0` on a bot with a bag full of quest greens → most of it goes.
4. Say `dg 20` in raid chat with several bots → every bot replies, nothing is destroyed.
5. Hand a bot a heirloom and a quest-starter item, run `dg 0 confirm` → both survive.
6. Equip nothing in one slot (e.g. remove a trinket), put a low trinket in bags, `dg 0 confirm` →
   the bagged trinket survives, since there's no baseline.
7. `dg` with no number, and `dg abc` → usage error, nothing destroyed.
8. `help` → `destroygear` is in the whisper list.

## Known limits, worth stating in the PR

- Bags only. Bank items are untouched; `InventoryAction::IterateItems`
  ([InventoryAction.cpp:47](src/Ai/Base/Actions/InventoryAction.cpp#L47)) uses `mask ==
  ITERATE_ITEMS_IN_BANK` rather than a bitwise test, so bank can't be combined with bags anyway.
- Pure item-level comparison. A stat-perfect but lower-iLvl item can be destroyed if it falls past
  the margin; the margin is the knob for that.
- Gear the bot can't even use (plate on a rogue) is judged by iLvl like anything else, so a high-iLvl
  unusable piece survives.

## Follow-up

On approval, copy this document to `docs/plans/destroygear-command/destroygear-command.PLAN.md` per
the project's planning-directory convention before starting implementation.
