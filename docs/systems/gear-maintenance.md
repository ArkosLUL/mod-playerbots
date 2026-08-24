# Gear maintenance: bags, and clearing out old gear

What keeps a bot's bags usable over a long-running realm. Scoring and upgrade decisions are in
[itemization.md](itemization.md); what a bot picks up and how it rolls is in [loot.md](loot.md).

## `destroygear` — bulk-destroy outdated gear

```
destroygear <X> [confirm]
dg <X> [confirm]
```

`X` is an item-level margin: `dg 20` destroys bagged gear whose slot-equivalent equipped item is more
than 20 item levels better, `dg 0` anything strictly worse. Without `confirm` it previews. Nothing
else covered this — `destroy [link]` is one item at a time, `smart destroy` only fires autonomously
above 90% bag fill and is disabled whenever the master is a real player, and `s gray` / `s vendor`
need a vendor in range and will not touch soulbound raid gear, which is most of the pile.

**Confirmation is stateless.** `dg 20 confirm` recomputes from scratch, so there is no pending state
per bot and no way for a stale preview to destroy something else. Whisper, party, raid, guild and
custom channels all work without extra code — every entry point converges on
`PlayerbotAI::HandleCommand`, and the party/raid hook already fans out to the whole group.

Per bagged item: keep unless it is `ITEM_CLASS_WEAPON` or `ITEM_CLASS_ARMOR` → apply the protection
gate → resolve slots with `sRandomItemMgr->GetViableSlots(proto->InventoryType)`, skipping anything
that returns `nullptr` (bags, quivers, ammo, non-equippables) → take the **lowest** `ItemLevel` among
the viable slots that actually hold something → destroy when `baseline - ItemLevel > X`. Taking the
minimum is the conservative choice: a bagged ring only dies if it is worse than the *worse* of the
two equipped rings. An item whose viable slots are all empty is skipped — there is no baseline, so
nothing proves it junk.

`GetViableSlots` had to move into the public section of `RandomItemMgr.h`; it is a const lookup into
a table built once at init, so there is nothing to protect. It is reused rather than replaced because
the module already carries four other `InventoryType` → slot mappings, and this is the only
data-driven one that covers cloak and tabard.

| Protected | Why |
|---|---|
| `ITEM_FLAG_NO_USER_DESTROY` | The core refuses to destroy these anyway; skipping avoids a bogus "destroyed" line |
| `ITEM_FLAG_IS_BOUND_TO_ACCOUNT` | Heirlooms — their template `ItemLevel` is low and fixed, so an iLvl rule eats every one |
| `Quality >= ITEM_QUALITY_LEGENDARY` | Covers legendary (5), artifact (6), heirloom (7) |
| `StartQuest != 0` | The item starts a quest |
| `INVTYPE_TABARD`, `INVTYPE_BODY` | Cosmetic; item level is meaningless there |
| `ITEM_USAGE_QUEST` | Needed for a quest the bot is on |

**Deliberately not protected:** unique-equipped items (`ITEM_FLAG_UNIQUE_EQUIPPABLE` covers most raid
trinkets and a lot of rings — precisely what stacks up tier after tier and what this exists to clear),
and BiS-listed items, since something 20+ iLvl below what is already in that slot is by definition not
that bot's BiS any more.

Limits, all accepted: **bags only** — `InventoryAction::IterateItems` tests
`mask == ITERATE_ITEMS_IN_BANK` rather than a bitwise test, so bank cannot be combined with bags
anyway. Comparison is pure item level, so a stat-perfect but lower-iLvl piece can go and the margin is
the only knob; and gear the bot cannot use at all (plate on a rogue) is judged the same way, so a
high-iLvl unusable piece survives.

## `InitBags` upgrades in place

`PlayerbotFactory::InitBags` is the only source of a bot's four bag-slot containers and hardcodes
**51809 "Portable Hole" (24 slots)**. It runs from `MaintenanceAction` (alt bots gated on
`AiPlayerbot.AltMaintenanceBags`) and from `PlayerbotFactory::Randomize`.

It used to take a `destroyOld` flag and was wrong either way. With `destroyOld = false` the only skip
test was item-id equality, so any other occupied slot hit `if (old_bag) continue;` and never upgraded
however small the bag was. With `destroyOld = true` the `DestroyItem` ran and *then* the same
`continue` fired — `old_bag` was still a non-null, now stale, pointer — so `EquipNewItem` never ran
and `Randomize()` destroyed the bag **and its contents**, leaving the slot empty. The flag is gone;
`InitBags()` now upgrades any plain bag smaller than 51809 and moves the old bag's contents across.

**Death Knights were the whole motivation.** They start with four 12-slot Deathweave Bags from
`CharStartOutfit`, so every slot is occupied at creation and maintenance never upgraded any of them —
measured at 53,489 characters still on the 12-slot bags against 120 on Portable Holes. Every other
class starts bagless.

**Quivers, ammo pouches and profession bags are never touched**, by `InitBags` or by the loot path.
The loot path needed fixing to keep that true: `ItemUsageValue::GetSmallestBagSize` started its loop
at `INVENTORY_SLOT_BAG_START + 1`, and that off-by-one was the *only* reason a hunter's quiver in slot
19 survived — correcting the bound alone would have made the quiver report as the smallest bag and get
swapped out. Both it and `EquipAction::GetSmallestBagSlot` now skip any slot whose item is not
`ITEM_CLASS_CONTAINER` + `ITEM_SUBCLASS_CONTAINER`. The two must agree: a protected container has to
be invisible to both, or one will offer a slot the other refuses. `GetSmallestBagSize` keeps `0` for a
genuinely empty slot and returns `uint32` max when every slot is occupied and none is replaceable, so
`Calculate` yields `ITEM_USAGE_NONE`.
