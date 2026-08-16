# Loot: what bots pick up and how they roll

Scoring itself lives in [itemization.md](itemization.md); this is the decision layer on top.

## Per-item loot decision

`StoreLootAction::IsLootAllowed` (`src/Ai/Base/Actions/LootAction.cpp:466`) checks in this order:

1. always-loot whitelist
2. `MaxCount` cap
3. `StartQuest`
4. active-quest required items
5. `lootStrategy->CanLoot(proto, context)`

**Quest items and the whitelist are checked before the strategy**, so a new loot strategy never needs
quest handling of its own.

## Strategies

Defined in `src/Ai/Base/Value/LootStrategyValue.cpp`; base class `LootStrategy` in
`src/Mgr/Item/LootObjectStack.h:17`. Name → strategy mapping is `LootStrategyValue::instance(name)`.

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

Set the default with `AiPlayerbot.LootStrategy` (shipped default `normal`). At runtime the master
uses `ll <strategy>`, queries with `ll ?`, and previews a specific item with `ll ?<item>`.

## Roll voting

Two independent stat systems drive a roll, and **the bugs live in their disagreement**:

- **Score engine** — `StatsCollector::CollectItemStats` → `StatsWeightCalculator::CalculateItem` →
  `itemScore`, which is what makes `QueryItemUsageForEquip` return EQUIP/REPLACE.
- **Spec gate** — `BuildItemStatProfile` → `IsPrimaryForSpec` / `IsFallbackNeedReasonableForSpec`,
  plus the `smartNeedBySpec` off-spec safety net, which runs only in the loot path via
  `AdjustUsageForOffspec`.

`FinalizeRollVote` (`ItemUsageValue.cpp:2478`) under `AiPlayerbot.Roll.UpgradesOnly = 1` forces
PASS on everything except a computed NEED whose usage is `ITEM_USAGE_EQUIP`/`REPLACE` (downgraded to
GREED). **So every wrong GREED means the item was misclassified as a genuine equip-upgrade** — the
bug is in the classifier, never in the greed fallback. Shipped default is `Roll.UpgradesOnly = 0`.

Three misclassification shapes were traced and fixed at the root, so bots also stop auto-*equipping*
these items rather than only rolling on them:

- **Melee on a statless ranged weapon.** A rogue uses the MELEE collector, so a ranged weapon scored
  nonzero twice: base ranged DPS added unconditionally (`STATS_TYPE_RANGED_DPS`, weight 0.01) and its
  `ITEM_SPELLTRIGGER_CHANCE_ON_HIT` proc scored as melee AP. A melee never makes ranged attacks, so
  both are worthless. The spec net missed it too — `BuildItemStatProfile` only scans ON_EQUIP/ON_USE
  spells, so the proc is invisible, and `IsFallbackNeedReasonableForSpec` has a "no stats →
  reasonable" shortcut. Fix: for `IsRangedWeapon() && (type_ & MELEE)`, skip both. Hunters use the
  RANGED collector and are untouched.
- **Healer on spell-penetration armour.** Real INT/SP score positive → EQUIP.
  `ITEM_MOD_SPELL_PENETRATION` is collected but carries **zero weight** and is not read by
  `BuildItemStatProfile`, so nothing flags the piece as caster-DPS/PvP itemisation.
- **Physical class on a caster weapon.** Pure-caster weapons are already rejected, but the reject is
  `… && !hasPhysical`, so a weapon also carrying agi/crit/sta slips through into an empty slot.

`IsRoleItemizationMismatch(bot, proto)` is the single choke point, called in
`QueryItemUsageForEquip` right after `itemScore`; returning true forces `shouldEquip = false`, which
covers both loot rolls and auto-equip. `IsPrimaryForSpec` is never reached for these, because
BAD_EQUIP short-circuits `AdjustUsageForOffspec`.

**As built, the feral case was dropped.** `ITEM_MOD_FERAL_ATTACK_POWER` (=40) does not exist in
3.3.5 — it is commented out at `ItemTemplate.h:62`. Feral AP is a computed green stat, not an
`ItemStat` row, so there is no proto signal to test. A feral staff carrying only physical stats
(agi/str/sta) is therefore indistinguishable from a valid physical weapon and is left alone.

These rejects are heuristics. The known false-positive risk is a physical class legitimately wanting
a weapon that happens to carry a caster stat — rare in WotLK itemisation.

**Skull-marking drives DPS target choice**: `DpsTargetValue::Calculate` → `RtiTargetValue`, with
`rti` defaulting to `"skull"`.

## Ranked BiS lists

`playerbots_bis_ranked` (loaded by `BisListMgr`, imported by `apps/bis/generate_bis.py` from the
Bistooltip addon) is an authoritative answer to "which spec is this item itemized for", which the
stat heuristics above can only guess at. Proc-only trinkets are the motivating case: `Grim Toll`
(40256) carries armour pen in a proc, so the classifier reads it as caster gear and a Fury warrior
votes PASS.

The signal is **additive only** — the lists hold six candidates per slot per phase, so an absent item
is never penalised. It reaches a roll two ways:

- **Gate bypass** (`AiPlayerbot.Bis.GateBypass`) — `IsBisForBot` short-circuits
  `IsFallbackNeedReasonableForSpec` (`:612`), `IsPrimaryForSpec` (`:984`) and `AdjustUsageForCrossArmor`
  (`:1159`). `IsRoleItemizationMismatch` is deliberately **not** bypassed.
- **Score bonus** (`AiPlayerbot.Bis.ScoreBonus`, `Bis.PhaseDecay`) — applied in
  `StatsWeightCalculator::BisRankMultiplier`, gated on `SetBisBonus(true)`. That is set in
  `QueryItemUsageForEquip`, so it moves the `usage` that voting consumes; it is not equip-only.

Under `Roll.UpgradesOnly = 1` bots cap at GREED, so in practice the lists decide **GREED vs PASS**,
not NEED vs GREED.

### Expansion matching

Vanilla, TBC and WotLK lists are all loaded. `BisListMgr::ProgressForBot` maps the bot's
individual-progression tier to a `{expansion, phase}` ceiling via a static table, and `GetBisRankFor`
**only matches rows in the bot's own expansion**. Admitting a lower expansion's tail would give a
level-80 bot a gate bypass on ilvl-164 TBC epics, and phase numbers restart per expansion, so the
decay arithmetic would be comparing unrelated ladders.

Phase numbering: Vanilla `PR, P1..P6` (0-6); TBC `PR, T4, T5, T6, ZA, SWP` (0-5); WotLK
`PR, T7, T8, T9, T10, RS` (0-5).

Two specs get no signal at all and that is intended: Rogue Subtlety has no list in any expansion, and
Vanilla/TBC have no Discipline priest list. `ResolveSpecKey` also refuses whenever the talent tab and
the role the bot actually plays disagree — a wrong list is worse than none, because it bypasses the
spec gates in the wrong direction.

`LoadAll` runs once at startup with **no reload path**, so regenerated data needs a worldserver
restart.
