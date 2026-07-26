# Fix wrong bot loot-roll voting + meta-socket re-gem

## Context

Two independent defects reported on a live server (`configurationOverrides/Playerbot.env`):

**A. Bots GREED on items useless to them.** Because the server runs `AiPlayerbot.Roll.UpgradesOnly = 1`,
`FinalizeRollVote` (ItemUsageValue.cpp:2238-2239) forces PASS on *everything* except a computed NEED whose
usage is `ITEM_USAGE_EQUIP`/`ITEM_USAGE_REPLACE` (which it downgrades to GREED). So every wrong GREED means
the item was misclassified as a **genuine equip-upgrade** for that bot. The bug is in the equip/upgrade
classifier, not the greed fallback. Observed cases: warrior greed on druid staff (30883), rogue greed on a
statless ranged weapon whose only property is a chance-on-ranged-hit AP proc (31323), healer greed on a
spell-penetration armour piece (30884). `smartNeedBySpec` defaults to **true** and is ON here.

**B. Already-gemmed bots don't re-gem to satisfy their meta gem on `maintenance`.** `ApplyEnchantAndGemsNew`
runs unconditionally on maintenance and recomputes gems from scratch, but the meta bonus stays inactive for
bots that already had gems.

The two stat systems that drive A disagree, and the gaps live in the disagreement:
- **Score engine** — `StatsCollector::CollectItemStats` → `StatsWeightCalculator::CalculateItem` → `itemScore`,
  which makes `QueryItemUsageForEquip` return EQUIP/REPLACE.
- **Spec gate** — `BuildItemStatProfile` → `IsPrimaryForSpec` / `IsFallbackNeedReasonableForSpec`, the
  `smartNeedBySpec` off-spec safety net (runs only in the loot path via `AdjustUsageForOffspec`).

Chosen approach (confirmed with user): fix at the **root** (score + usage engine) so bots also stop
auto-EQUIPPING these items, not only rolling on them.

## Root causes

### Rogue ranged stat-stick (31323)
A Rogue uses the **MELEE** collector (StatsWeightCalculator.cpp:51-62; `IsMelee` true). In
`StatsCollector::CollectItemStats` (StatsCollector.cpp:23-65) that makes a ranged weapon score nonzero two ways:
its base ranged DPS is added unconditionally (lines 25-29, `STATS_TYPE_RANGED_DPS`, basic weight 0.01) and its
`ITEM_SPELLTRIGGER_CHANCE_ON_HIT` proc is scored as melee AP (lines 53-61 gated only by `type_ & MELEE`, plus
`SPELL_AURA_MOD_ATTACK_POWER` at 582-584). A melee never makes ranged attacks, so both are worthless — but they
make `itemScore != 0` → EQUIP. The spec net misses it too: `BuildItemStatProfile` only scans ON_EQUIP/ON_USE
spells (ItemUsageValue.cpp:753-757), so the proc is invisible, and `IsFallbackNeedReasonableForSpec` has a
"no stats → reasonable" shortcut (ItemUsageValue.cpp:603-608) that returns true for a statless item.

### Healer spell-penetration armour (30884)
Real INT/SP score positive → EQUIP. `ITEM_MOD_SPELL_PENETRATION` is collected but has **zero weight**
(StatsCollector.cpp:537-539; no `stats_weights_` entry anywhere) and is **not** read by `BuildItemStatProfile`
(ItemUsageValue.cpp:689-748), so nothing flags the piece as caster-DPS/PvP itemisation. `IsPrimaryForSpec`
returns true for a healer (caster path, ItemUsageValue.cpp:919-920).

### Warrior off-role weapon (30883)
Pure-caster weapons are already rejected (`IsPrimaryForSpec` line 873). Reproduces when the weapon also carries
raw stats a physical class weights (agi/crit/sta, or druid feral AP) into an empty/weak weapon slot, so
`hasPhysical` is true and the reject at 873 (`… && !hasPhysical`) doesn't fire.

### Meta socket not re-applied on already-gemmed bots
`ApplyEnchantAndGemsNew` defers meta gems and applies them last (PlayerbotFactory.cpp:5307-5313):

```cpp
bot->ApplyEnchantment(ms.item, slot, false);      // remove
ms.item->SetEnchantment(slot, metaId, 0, 0, guid); // NO-OP when id unchanged (Item.cpp:923)
bot->ApplyEnchantment(ms.item, slot, true);        // re-add
```

`Player::ApplyEnchantment` gates on the condition for **both** apply and remove (PlayerStorage.cpp:4421). When a
bot's meta gem is already socketed but was **inactive** (its colour condition was unmet at login) and the same
run's colour-steering (Step C) now satisfies the condition, the leading `ApplyEnchantment(false)` sees the
condition met and **removes meta stats that were never applied** (phantom −stats); the paired
`ApplyEnchantment(true)` re-adds them → net **zero** → meta stays inactive. Fresh bots are unaffected because
their meta slot is empty (the remove no-ops at PlayerStorage.cpp:4414). Re-running maintenance repeats the wash
and changes nothing — matching "gems don't change + requirement not fulfilled". The core avoids this exact trap
with `wasactive` tracking (Player.cpp:11256-11262); the module doesn't. `CorrectMetaGemEnchants` is not a drop-in
fix — it compares before/after a single changed socket, not a from-scratch recompute.

## Fixes

### 1. Score fix — melee/tank ranged stat-stick (`src/Mgr/Item/StatsCollector.cpp`, `CollectItemStats` 23-65)
When `proto->IsRangedWeapon()` and `type_ & CollectorType::MELEE`: skip the `STATS_TYPE_RANGED_DPS` add (25-29)
and skip the `ITEM_SPELLTRIGGER_CHANCE_ON_HIT` branch (53-61). Real `ItemStat` mods and ON_EQUIP/ON_USE stats
still count, so a genuine stat-stick (agi/AP/crit) is still valued; a statless ranged weapon now scores 0 →
`shouldEquip=false` → BAD_EQUIP → PASS. Hunters use the `RANGED` collector and are untouched.

### 2. Usage fix — role/itemisation mismatch reject (`src/Ai/Base/Value/ItemUsageValue.cpp`)
- Extend `ItemStatProfile` + `UpdateItemStatProfileFromStats` (689-748) with `hasSpellPen`
  (`ITEM_MOD_SPELL_PENETRATION`) and `hasFeralAP` (`ITEM_MOD_FERAL_ATTACK_POWER`).
- Add a helper `IsRoleItemizationMismatch(bot, proto)` returning true when:
  - `IsHeal(bot)` and `hasSpellPen` (spell pen = caster-DPS/PvP marker; **healers only**, per user), **or**
  - the bot is a pure physical role (`!isCaster && !isHealer`) and `proto->Class == ITEM_CLASS_WEAPON` and the
    weapon carries caster-primary stats (`hasINT || hasSP || hasMP5`) **or** druid-only feral AP
    (`hasFeralAP && cls != CLASS_DRUID`) — i.e. a caster/feral weapon on a physical class. The `cls != DRUID`
    guard keeps a feral druid's own feral staff valid.
- Call it in `QueryItemUsageForEquip` right after `itemScore` is computed (~1145-1148): if it returns true,
  force `shouldEquip = false`. This single choke covers **both** loot rolls and auto-equip (BAD_EQUIP →
  GREED → PASS under UpgradesOnly, and won't auto-equip). `IsPrimaryForSpec` is not reached for these because
  BAD_EQUIP short-circuits `AdjustUsageForOffspec` (line 936); no mirror edit needed there.

### 3. Meta fix — clean meta lifecycle (`src/Bot/Factory/PlayerbotFactory.cpp`, `ApplyEnchantAndGemsNew`)
- **Pre-deactivate pass**: after the socket-collection loop (~5172) and before colour gems are rearranged
  (~5196), for each entry in `metaSockets` that currently holds a meta enchant, call
  `bot->ApplyEnchantment(item, EnchantmentSlot(slot), false)`. At this point colour gems are still the old ones,
  so the condition gate matches the actual applied state and the remove is correct (removes only if truly
  applied; no-ops otherwise).
- **Step D** (5307-5313): drop the leading `ApplyEnchantment(false)`; keep `SetEnchantment(metaId)` +
  `ApplyEnchantment(true)`. With the meta now in a known-unapplied state and colour gems satisfying the
  condition, the single `ApplyEnchantment(true)` applies the meta exactly once. Fresh bots behave identically.

## Verification

Headless build/compile of the module isn't possible in this environment — static review + hand-off to the
user's build. Steps for the user:

1. **Build** the server with the module and restart.
2. **Voting** (in-game, with `Roll.UpgradesOnly=1`):
   - Rogue: trigger a group roll on 31323 (statless ranged, AP proc) → expect **PASS** (was GREED).
   - Healer: roll on 30884 (spell-pen armour) → expect **PASS**.
   - Warrior: roll on 30883 (druid staff) → expect **PASS**.
   - Regression: confirm a real upgrade for each class still rolls NEED→GREED, hunters still value ranged
     weapons, feral druids still value feral staves, casters still value caster weapons.
3. **Meta socket**: take an already-gemmed bot whose meta bonus is inactive (colour requirement unmet), run
   `maintenance` → expect colour gems steered as needed and the **meta bonus now active**; run `maintenance`
   again → stable, meta stays active. Verify via `.gm on` inspect or the meta gem showing lit.
   Optional: temporary `LOG_INFO` after Step C printing `EnchantmentFitsRequirements(metaCondition,-1)` and the
   colour counts to confirm the branch taken, if a case still fails.

## Open items / risks
- Exact stats of items **30883 / 30884 / 31323** live in the world DB (not queryable from the repo). Fixes are
  written against the mechanism, not the specific rows; user should confirm the three items behave as expected
  and that the stat-type assumptions (staff has caster or feral AP; ranged weapon is statless) hold.
- Reject rules are heuristics: watch for false positives (e.g. a physical class legitimately wanting a weapon
  that happens to carry a caster stat — rare in WotLK itemisation).

## Implementation deviations (as built)
- **`hasFeralAP` dropped from Fix 2.** `ITEM_MOD_FERAL_ATTACK_POWER` (=40) does not exist in this 3.3.5 tree —
  it's commented out in `ItemTemplate.h:62` (`not in 3.3`). Feral AP is not an `ItemStat` row in WotLK (it's a
  computed green stat on the weapon), so there is no proto signal to test and the branch would not compile.
  `IsRoleItemizationMismatch` therefore only rejects a physical-role weapon on `hasINT || hasSP || hasMP5`
  (caster staff/weapon), not the feral-weapon-on-non-druid case. A feral staff carrying only physical stats
  (agi/str/sta) has no distinguishing signal and is left as-is — same class of false positive already flagged
  under risks. If 30883 is a *caster* staff it's covered; if it's a *feral* staff it is not.
- `hasSpellPen` was added to `ItemStatProfile` (`ITEM_MOD_SPELL_PENETRATION` = 47 exists); the healer spell-pen
  reject is implemented as planned.
