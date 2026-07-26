# Set-bonus stickiness + value-based socket scoring

## Step 0 — save this plan into the repo

First action on approval: write this document verbatim to
`modules/mod-playerbots/docs/set-bonus-and-socket-scoring/set-bonus-and-socket-scoring.PLAN.md`,
matching the existing `docs/<slug>/<slug>.PLAN.md` convention (`docs/burst-on-boss-only/`,
`docs/dps-offensive-potions/`, …). Then implement.

## Context

Bots replace tier/set pieces with marginal stat upgrades and destroy active 2/4/6-piece
bonuses, and they undervalue gem sockets. Two root causes in the item scoring engine:

**Set bonuses are effectively dead on the upgrade path.**
`StatsWeightCalculator::CalculateItemSetMod` (`src/Mgr/Item/StatsWeightCalculator.cpp:664-706`)
is the only place item sets are read anywhere in the module, and it is disabled via
`SetItemSetBonus(false)` on *every* runtime equip/loot call site. It only ever runs during
initial gearing (`PlayerbotFactory::InitEquipment`) and quest-reward picks. Beyond that, the
formula itself is backwards:

- `multiplier += 0.1f * itemCount` only while `itemCount < max_items`; once the set is
  **complete** the multiplier resets to `1.0f`. Stickiness is zero exactly when the bonuses
  are live.
- It reads `player->ItemSetEff`, i.e. *currently equipped* state. When comparing a candidate
  against the incumbent set piece, the incumbent's own contribution counts toward both scores,
  so the multiplier cancels out. Breaking a bonus is never penalised.
- Piece count is treated linearly; the actual 2/4/6 spell thresholds
  (`ItemSetEntry::items_to_triggerspell[]`) are ignored.

**Sockets are counted, but with a flat guess.**
`CalculateSocketBonus` (`StatsWeightCalculator.cpp:708-725`) applies `1.0 + socketNum * 0.03`
— 3% per socket, unconditional, no config, socket colour and meta sockets ignored. A meta +
2 sockets on a raid piece is worth far more than 9%.

**Outcome wanted:** bots keep set pieces that carry an active bonus unless the replacement is
a genuinely large upgrade, actively work toward the next threshold, and value sockets at what
the gems they would actually slot are worth.

### Decisions taken

| Topic | Decision |
|---|---|
| Set weight | Moderate, config-exposed: +25% per bonus kept/gained, +3% per piece of progress |
| Sockets | Value-based — score each socket as the best gem the bot would slot, cached |
| Scope | Equip decisions **and** loot-roll voting |
| `socketBonus` double-count | Leave as-is, out of scope |

### Out of scope (known limitations to leave alone)

- Tier **tokens** carry no `ItemSet`, so `IsTokenLikelyUpgrade` / `IsAnyTierSlotLikelyUpgrade`
  (`ItemUsageValue.cpp:2156-2200`) stay pure-ilvl. Bots still roll on tokens the old way.
- `StatsCollector.cpp:81-85` keeps crediting full `socketBonus` enchant stats regardless of
  whether the bot can colour-match.
- `RandomItemMgr::CalculateItemWeight` (`RandomItemMgr.cpp:1049-1058`) keeps
  `SetItemSetBonus(false)` — it feeds a cached, player-independent table.

---

## Part 1 — Set-bonus delta model

### 1a. Rewrite the scoring in `StatsWeightCalculator`

`src/Mgr/Item/StatsWeightCalculator.h` — add state + setter alongside the existing toggles
(`:56-60`):

```cpp
void SetItemSetBonus(bool apply) { enable_item_set_bonus_ = apply; }
// Set id of the piece occupying the slot being contested; that piece is treated as removed so
// the incumbent and the challenger are measured against the same baseline. 0 = no context.
void SetReplacedItemSet(uint32 setId) { replaced_item_set_ = setId; }
```
plus `uint32 replaced_item_set_ = 0;` in the private members (`:79-94`).

`src/Mgr/Item/StatsWeightCalculator.cpp` — file-local helpers near the top:

```cpp
// ItemSetEntry pairs spells[j] with the piece count that triggers it; entries can be zero or
// duplicated, so guard both.
uint32 ActiveSetBonuses(ItemSetEntry const* set, uint32 pieces);
uint32 NextSetThreshold(ItemSetEntry const* set, uint32 pieces);   // smallest threshold > pieces, 0 if none
uint32 EquippedSetPieces(Player* player, uint32 setId);            // from player->ItemSetEff
```

Replace the body of `CalculateItemSetMod` (`:664-706`) with the delta model:

```
base   = EquippedSetPieces(player, proto->ItemSet)
         minus 1 if replaced_item_set_ == proto->ItemSet   // vacate the contested slot
gained = ActiveSetBonuses(set, base + 1) - ActiveSetBonuses(set, base)

multiplier = 1 + itemSetBonusWeight * gained
if (gained == 0 && NextSetThreshold(set, base))
    multiplier += itemSetProgressWeight * (base + 1)

weight_ *= multiplier
```

Why this fixes it: the incumbent 4th piece of a 4pc set is scored at `base = 3`, so
`gained = 1` and it gets +25%. A non-set challenger is scored at its own `base = 0`,
`gained = 0`, no bonus. The set piece now has to be beaten by 25% *on top of*
`equipUpgradeThreshold`. Swapping T7 chest for T7.5 chest is neutral — both are the same set,
both score `gained = 1` off the same vacated baseline. A complete set is protected instead of
abandoned, which is the exact case the old code zeroed out.

### 1b. Enable it on the runtime paths

Each site currently calls `SetItemSetBonus(false)`. Gate on the new config flag and supply
slot context where a slot is known.

**`src/Ai/Base/Value/ItemUsageValue.cpp` — `QueryItemUsageForEquip` (`:1106-1330`).** This is
the primary path; `LootUsageValue`, `ItemUpgradeValue` and `ItemUsageValue` all funnel through
it, so loot-roll voting is covered by this one change.

Structural detail that matters: `itemScore` is currently computed **once** before the slot loop
(`:1174`) and reused at `:1176` and `:1281`. Slot context requires it to be recomputed per slot.

- Keep the pre-loop `CalculateItem` with `SetReplacedItemSet(0)` — it only feeds the
  `shouldEquip` "is this usable at all" flag (`:1176-1177`) and the empty-slot branch
  (`:1242-1248`). Set multipliers are strictly positive so the non-zero test is unaffected.
- Inside the loop (`:1236`), after resolving `oldItemProto` (`:1250`), call
  `calculator.SetReplacedItemSet(oldItemProto->ItemSet)` and recompute **both** the candidate
  score and `oldScore` before the `shouldEquipInSlot` (`:1256`) and `isBetter` (`:1281`)
  comparisons. Use a slot-local `itemScore` so the pre-loop value stays intact for the
  empty-slot branch.

**`src/Ai/Base/Value/ItemUsageValue.cpp` — `AdjustUsageForCrossArmor` (`:986-1064`).** Same
pattern: enable, and set the replaced set id from each equipped item as the loop
(`:1044`) scores it.

**`src/Ai/Base/Actions/EquipAction.cpp`** — `:153-162` (MH/OH placement) and `:278-290`
(ring/trinket slot choice). Enable, and set the replaced set id from the item currently in the
slot being scored against. These use raw `>` comparisons with no threshold, so without this
they would happily undo what `QueryItemUsageForEquip` just protected.

**`src/Ai/Base/Actions/BuyAction.cpp:66-90`** — enable, leave `SetReplacedItemSet(0)`; vendor
candidates are compared against each other, not against a specific slot.

### 1c. Config

`src/PlayerbotAIConfig.h` / `.cpp` (load next to the gear block at `.cpp:702-723`) and
`conf/playerbots.conf.dist` (next to `AiPlayerbot.EquipUpgradeThreshold`, `:286-288`):

| Option | Type | Default | Meaning |
|---|---|---|---|
| `AiPlayerbot.ItemSet.UseForUpgrades` | bool | `1` | Master switch for 1b. Off ⇒ current behaviour |
| `AiPlayerbot.ItemSet.BonusWeight` | float | `0.25` | Score boost per set bonus kept or gained |
| `AiPlayerbot.ItemSet.ProgressWeight` | float | `0.03` | Per-piece nudge toward the next threshold |

---

## Part 2 — Value-based socket scoring

### 2a. Gem-value cache

New file-local cache in `src/Mgr/Item/StatsWeightCalculator.cpp` (or a small
`SocketValueCache` helper beside it):

```cpp
// key: cls | tab | level | pvpSpec | socketColor  →  best gem score for that socket
static std::unordered_map<uint64, float> s_bestGemScore;
static std::shared_mutex s_bestGemScoreMutex;
```

Cache fill reuses the existing startup-built gem pool `PlayerbotFactory::enchantGemIdCache`
(public static, `PlayerbotFactory.h:225`, populated at `PlayerbotFactory.cpp:498-514`) and
mirrors the eligibility filters already written in `PlayerbotFactory::ApplyEnchantAndGemsNew`
(`:4998-5036` for availability, `:5074-5111` for socket/gem matching):

- valid `GemPropertiesEntry` and non-zero `spellitemenchantement`
- `limitEnchantExpansion` + level ≤ 70 ⇒ skip gem ids ≥ 39900
- `gemTemplate->ItemLevel <= bot level`, `enchant->requiredLevel <= bot level`
- enchant slot is `PERM_ENCHANTMENT_SLOT` or `TEMP_ENCHANTMENT_SLOT`
- meta gems (`gemProperties->color == SOCKET_COLOR_META`) only for meta sockets, coloured
  gems only for coloured sockets

Score each candidate with `CalculateEnchant` on a **separate** `StatsWeightCalculator`
instance — `CalculateEnchant` calls `Reset()`, which would wipe `weight_` and the collector
mid-`CalculateItem`. That temporary never touches socket valuation, so no recursion.

Two deliberate differences from the factory's `pickBestGem`:
- **No** `score *= 1.2f` colour-match nudge (`:5102-5103`) — that biases *which* gem to slot,
  not what the socket is worth.
- **No** jeweller's-gem cap — scoring a hypothetical socket, not allocating a real budget.

Thread safety: bots tick on parallel map threads, so guard the map with the shared mutex
(read-lock lookup, write-lock on miss). Entries are small and bounded (~classes × tabs ×
levels × colours), so no eviction needed.

### 2b. Rewrite `CalculateSocketBonus`

The unit problem: `weight_` gets multiplied by `CalcMixedGearScore(ilvl, quality)` at
`:158-167` — roughly `ilvl * 1.1^quality`, so a few hundred. Raw gem scores live in the
un-multiplied stat-sum space. Adding them directly would be off by two orders of magnitude,
and adding them pre-blend would scale gem value by the host item's ilvl, which is wrong.

Keep the multiplier form but derive it from real gem value, expressed as a fraction of what
the item's own stats are worth:

```
baseWeight = weight_ as of the stat-sum + item-type-penalty stage   // pre-quality-blend
socketValue = Σ over sockets of BestGemScore(cls, tab, lvl, pvp, socketColor)

if (baseWeight > epsilon)
    multiplier = 1 + socketValueFactor * (socketValue / baseWeight)
else
    multiplier = 1 + socketWeightPerSocket * socketNum         // fallback, old behaviour
multiplier = min(multiplier, socketValueMaxMultiplier)
weight_ *= multiplier
```

This is unit-consistent, needs no magic constant, and naturally gives sockets a larger relative
share on stat-light items and a smaller one on stat-dense items. Meta sockets need no special
case — the cache returns the best *meta* gem for `SOCKET_COLOR_META`, which is already worth
more than a coloured gem. The clamp guards against blowups when `baseWeight` is near zero or
negative.

Call site (`:156`) stays where it is: after `CalculateItemTypePenalty` and
`CalculateItemSetMod`, before the quality blend.

### 2c. Config

| Option | Type | Default | Meaning |
|---|---|---|---|
| `AiPlayerbot.Socket.ValueFactor` | float | `1.0` | Scales the gem-derived socket multiplier. `0` ⇒ fallback path only |
| `AiPlayerbot.Socket.MaxMultiplier` | float | `1.5` | Clamp on the socket multiplier |
| `AiPlayerbot.Socket.WeightPerSocket` | float | `0.03` | Flat fallback when gem value is unusable |

---

## Files touched

| File | Change |
|---|---|
| `src/Mgr/Item/StatsWeightCalculator.h` | `SetReplacedItemSet`, `replaced_item_set_`, helper decls |
| `src/Mgr/Item/StatsWeightCalculator.cpp` | Rewrite `CalculateItemSetMod` + `CalculateSocketBonus`; gem-value cache |
| `src/Ai/Base/Value/ItemUsageValue.cpp` | `QueryItemUsageForEquip` (`:1165-1174`, `:1236-1282`), `AdjustUsageForCrossArmor` (`:1016-1044`) |
| `src/Ai/Base/Actions/EquipAction.cpp` | `:153-162`, `:278-290` |
| `src/Ai/Base/Actions/BuyAction.cpp` | `:66-90` |
| `src/PlayerbotAIConfig.h` / `.cpp` | 6 new options |
| `conf/playerbots.conf.dist` | 6 new documented entries |

---

## Verification

No headless build is available in this workspace, so this is a static-review + in-game pass.

**Static, before handing off to a build:**
1. Confirm the `ItemSetEntry` field names used (`spells[]`, `items_to_triggerspell[]`) match
   this core's `DBCStructure.h`, and that `MAX_ITEM_SET_SPELLS` is in scope.
2. Confirm `SOCKET_COLOR_META`, `MAX_GEM_SOCKETS`, `SOCK_ENCHANTMENT_SLOT`,
   `sGemPropertiesStore` are reachable from `StatsWeightCalculator.cpp`'s existing includes
   (`PlayerbotFactory.h` is already included for `CalcMixedGearScore`).
3. Grep that no `SetItemSetBonus(false)` remains on a path that should now be enabled, and
   that `RandomItemMgr.cpp:1055` deliberately still has it.

**In-game (needs a compiled server):**
1. `AiPlayerbot.ItemSet.UseForUpgrades = 0` ⇒ scores must be byte-identical to today's build
   for a bot in non-set gear. Regression gate.
2. `<bot> calc [item]` (`TellCalculateItemAction`, `TellLosAction.cpp:138-153`, registered as
   `calc` in `ChatActionContext.h:322`) prints a raw score. Compare on the same bot:
   - a socketed item vs an otherwise identical unsocketed one — gap should now clearly exceed
     the old flat 3%/socket
   - a meta-socketed item — should outscore an equivalent coloured-socket item
3. Gear a bot into exactly 4 pieces of a tier set. `.additem` a non-set piece ~15% better on
   raw stats into the slot holding the 4th piece. With `AutoEquipUpgradeLoot = 1` the bot
   must **not** swap. Repeat with an item ~40% better — it should swap.
4. Same bot at 3 set pieces: a 4th set piece that is a *slight* raw downgrade should still be
   equipped (crossing the 4pc threshold).
5. Loot roll: a group-loot set piece that would complete a threshold should draw NEED
   (`LootRollAction.cpp:61` → `CalculateLootRollVote`), where before it read as marginal.
6. Sanity-check server startup time and a `.playerbot rndbot init`-style mass gearing run —
   the gem cache fills lazily on first miss per (class, spec, level, colour); confirm no
   noticeable stall.
