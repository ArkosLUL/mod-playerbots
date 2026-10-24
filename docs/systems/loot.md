# Loot: what bots pick up and how they roll

Scoring itself lives in [itemization.md](itemization.md); this is the decision layer on top.

## Per-item loot decision

`StoreLootAction::IsLootAllowed` (`src/Ai/Base/Actions/LootAction.cpp`) checks in this order:

1. always-loot whitelist
2. `MaxCount` cap
3. `StartQuest`
4. active-quest required items
5. `lootStrategy->CanLoot(proto, context)`

**Quest items and the whitelist are checked before the strategy**, so a new loot strategy never needs
quest handling of its own.

## Strategies

Defined in `src/Ai/Base/Value/LootStrategyValue.cpp`; base class `LootStrategy` in
`src/Mgr/Item/LootObjectStack.h`. Name → strategy mapping is `LootStrategyValue::instance(name)`.

| Token | Behaviour |
|---|---|
| `normal` | Delegates to `ItemUsageValue` — food, scrolls, reagents and junk all count as useful via SKILL/USE/KEEP plus a SellPrice→AH/VENDOR fallback |
| `gray` | Gray-quality only |
| `disenchant` | Disenchantable items |
| `all` | Everything |
| `equip` (aliases `gear`, `eq`) | Armor and weapons, uncommon quality and up, nothing else |

`equip` deliberately does **not** inherit `NormalLootStrategy`, and deliberately omits its
`lootGuid.IsItem()` short-circuit — so opening a looted container under `equip` skips non-gear
contents. Quest items from containers are still caught by step 4 above.

Set the default with `Playerbots.LootStrategy` (shipped default `normal`). At runtime the master
uses `ll <strategy>`, queries with `ll ?`, and previews a specific item with `ll ?<item>`.

## Roll voting

Two independent stat systems drive a roll, and **the bugs live in their disagreement**:

- **Score engine** — `StatsCollector::CollectItemStats` → `StatsWeightCalculator::CalculateItem` →
  `itemScore`, which is what makes `QueryItemUsageForEquip` return EQUIP/REPLACE.
- **Spec gate** — `BuildItemStatProfile` → `IsPrimaryForSpec` / `IsFallbackNeedReasonableForSpec`,
  plus the `SmartNeedBySpec` off-spec safety net, which runs only in the loot path via
  `AdjustUsageForOffspec`.

`FinalizeRollVote` (`ItemUsageValue.cpp`) under `Playerbots.Roll.UpgradesOnly = 1` forces PASS on
everything except a computed NEED whose usage is `ITEM_USAGE_EQUIP`/`REPLACE` (downgraded to GREED).
**So every wrong GREED means the item was misclassified as a genuine equip-upgrade** — the bug is in
the classifier, never in the greed fallback. In practice bots therefore cap at GREED, and the BiS
lists below decide **GREED vs PASS**, not NEED vs GREED. Shipped default is `Roll.UpgradesOnly = 0`.

`IsRoleItemizationMismatch(bot, proto)` is the single choke point for role-mismatched itemisation,
called in `QueryItemUsageForEquip` right after `itemScore`; returning true forces
`shouldEquip = false`, which covers both loot rolls and auto-equip. `IsPrimaryForSpec` is never
reached for these, because BAD_EQUIP short-circuits `AdjustUsageForOffspec`. It rejects three shapes:

- **Melee on a statless ranged weapon.** A rogue uses the MELEE collector, so a ranged weapon would
  score nonzero twice: base ranged DPS (`STATS_TYPE_RANGED_DPS`, weight 0.01) and its
  `ITEM_SPELLTRIGGER_CHANCE_ON_HIT` proc scored as melee AP. A melee never makes ranged attacks, so
  for `IsRangedWeapon() && (type_ & MELEE)` both are skipped. The spec net would miss it too, because
  `IsFallbackNeedReasonableForSpec` has a "no stats → reasonable" shortcut. Hunters use the RANGED
  collector and are untouched.
- **Healer on spell-penetration armour.** Real INT/SP score positive → EQUIP.
  `ITEM_MOD_SPELL_PENETRATION` is collected but carries **zero weight** and is not read by
  `BuildItemStatProfile`, so nothing else flags the piece as caster-DPS/PvP itemisation.
- **Physical class on a caster weapon.** Pure-caster weapons are rejected elsewhere, but that reject
  is `… && !hasPhysical`, so a weapon also carrying agi/crit/sta would slip through into an empty
  slot.

**No feral case.** `ITEM_MOD_FERAL_ATTACK_POWER` (=40) does not exist in 3.3.5 — it is commented out
in the core's `ItemModType` (`ItemTemplate.h`). Feral AP is a computed green stat, not an `ItemStat`
row, so there is no proto signal to test, and a feral staff carrying only physical stats is
indistinguishable from a valid physical weapon.

These rejects are heuristics. The known false-positive risk is a physical class legitimately wanting
a weapon that happens to carry a caster stat — rare in WotLK itemisation.

## Ranked BiS lists

`playerbots_bis_ranked` (loaded by `BisListMgr`, imported by `apps/bis/generate_bis.py` from the
Bistooltip addon) is an authoritative answer to "which spec is this item itemized for", which the
stat heuristics above can only guess at. Proc-only trinkets are the motivating case: `Grim Toll`
(40256) carries armour pen in a proc, so the classifier reads it as caster gear and a Fury warrior
votes PASS.

The signal is **additive only** — the lists hold six candidates per slot per phase, so an absent item
is never penalised. A bot resolving to the sim BiS dataset instead
([itemization.md](itemization.md#sim-bis-dataset)) gets this whole signal from its own dataset
subject in place of the lists, never mixed per item. It reaches a roll two ways:

- **Gate bypass** (`Playerbots.Bis.GateBypass`) — `IsBisForBot` short-circuits
  `IsFallbackNeedReasonableForSpec`; `IsPrimaryForSpec` unless the item is body armour of a lower
  class than the bot's preferred one (`IsLowerTierArmorForBot`), a call that belongs to
  `AdjustUsageForCrossArmor`; and `AdjustUsageForCrossArmor` itself, which returns EQUIP when the bot
  can wear the class, after its etiquette and group gates. `IsRoleItemizationMismatch` is
  deliberately **not** bypassed.
- **Score bonus** (`Playerbots.Bis.ScoreBonus`, `Bis.PhaseDecay`) —
  `StatsWeightCalculator::BisRankMultiplier` ([itemization.md](itemization.md#ranked-bis-lists)).
  `QueryItemUsageForEquip` sets it, so it moves the `usage` that voting consumes; it is not
  equip-only.

### Expansion matching

Vanilla, TBC and WotLK lists are all loaded. `BisListMgr::ProgressForBot` maps the bot's
individual-progression tier to a `{expansion, phase}` ceiling via a static table, and `GetBisRankFor`
**only matches rows in the bot's own expansion**. Admitting a lower expansion's tail would give a
level-80 bot a gate bypass on ilvl-164 TBC epics, and phase numbers restart per expansion, so the
decay arithmetic would be comparing unrelated ladders.

Phase numbering: Vanilla `PR, P1..P6` (0-6); TBC `PR, T4, T5, T6, ZA, SWP` (0-5); WotLK
`PR, T7, T8, T9, T10, RS` (0-5).

Two specs get no list signal and that is intended: Rogue Subtlety has no list in any expansion, and
Vanilla/TBC have no Discipline priest list. `ResolveSpecKey` also refuses whenever the talent tab and
the role the bot actually plays disagree — a wrong list is worse than none, because it bypasses the
spec gates in the wrong direction.

`LoadAll` runs at startup and on `.playerbots rndbot reload` (which reruns
`PlayerbotAIConfig::Initialize`), so regenerated list data needs one or the other. The sim dataset
reloads on its own version poll (`BisDataset.PollSeconds`) or on `.playerbots bis reload`;
`.playerbots bis status` shows what is loaded.
