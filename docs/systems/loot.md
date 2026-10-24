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

**One decision, `EvaluateGear` (`ItemUsageValue.cpp`), answers "would this bot wear it, and
where"** for the roll vote (`"item usage"`), auto-equip (trade, item push, won rolls), vendor buys
and `calc`, so a vote and a trade can't disagree. Never add a gear check on one path only. Order:

1. **Eligibility** from the template (usable, slot, Titan's Grip type, unique-equipped per target
   slot), plus binding for a bag item.
2. **Fitness**, never overwritten by the comparison: unslotted score > 0, `IsRoleItemizationMismatch`,
   `CanEquipWeapon`, and under `Roll.SmartNeedBySpec` the spec checks (`IsPrimaryForSpec`, else
   `IsFallbackNeedReasonableForSpec`, and an off-spec ring/neck/trinket/cloak only over grey or
   white gear). `RandomItemMgr::CanEquipArmor` is not used: it rejects relics and int-heavy plate.
3. **Placement** (`GearPlacement.h`): wanted when `Σ new > Σ old × threshold` over the slots whose
   occupant changes. The threshold is `EquipUpgradeThreshold`, `Roll.CrossArmorExtraMargin` for a
   lower armor class over the bot's own (`EquipUpgradeThreshold` when BiS-listed or for a leveling
   healer), and 1.0 over a worn item failing the hard checks (score, role, weapon skill). Rings and
   trinkets fill an empty slot first, else replace the weaker worn one.
4. **Result**: EQUIP/REPLACE; an unfit item is BAD_EQUIP into an empty slot only (shirts, tabards,
   a leveling bot's bare slot; never voted on), else NONE, falling to disenchant / AH / vendor.
   A broken worn item is replaced whatever the scores.

`equip <item>`, outfits and the ICC rocket pack pass `force`: no fitness or threshold, best legal
slot even without a gain. "item usage" is recalculated on every read, so `EvaluateGear` must never
read it back for the same item (it would recurse without end).

The cross-armor courtesy (passing on every lower armor class at level cap or in a raid) and its
keys `Roll.StrictCrossArmorAtEndgame`, `Roll.CrossArmorGreedIsPass` and `Roll.UseGroupUsageChecks`
are gone: in WotLK about half of many specs' BiS body armor is a lower class, which the vote passed
on while a trade equipped it.

**`Player::GetItemCount` counts worn items whatever `inBankAlso` says**, so `total > bagOnly` detects
a bank copy, not a worn one. Count worn copies by slot (`CountUnwornCopies`). The roll's
"already owns a copy → GREED" counts only copies not worn, or a second copy for the off hand never
gets a vote; a unique-equipped item owned anywhere never gets NEED.

`FinalizeRollVote` (`ItemUsageValue.cpp`) under `Playerbots.Roll.UpgradesOnly = 1` forces PASS on
everything except a computed NEED whose usage is `ITEM_USAGE_EQUIP`/`REPLACE` (downgraded to GREED).
**So every wrong GREED means the item was misclassified as a genuine equip-upgrade** — the bug is in
the classifier, never in the greed fallback. In practice bots therefore cap at GREED, and the BiS
lists below decide **GREED vs PASS**, not NEED vs GREED. Shipped default is `Roll.UpgradesOnly = 0`.

`IsRoleItemizationMismatch` is the fitness check for role-mismatched itemisation; with the
neighbouring checks it rejects three shapes:

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

- **BiS-listed** (`Playerbots.Bis.GateBypass`; rank 1-6, any slot, at or below the phase cap, from
  the calculator's resolved source, so hand-blind) — one test per evaluation that skips the spec
  checks (`IsPrimaryForSpec` only for the bot's own armor class), skips `IsRoleItemizationMismatch`
  (27 of 57 weapons on the WotLK Enhancement lists carry spell power or intellect), and lowers a
  lower armor class's bar to `EquipUpgradeThreshold`. 0 switches all three off.
- **Score bonus** (`Playerbots.Bis.ScoreBonus`, `Bis.PhaseDecay`) —
  `StatsWeightCalculator::BisRankMultiplier` ([itemization.md](itemization.md#ranked-bis-lists)),
  hand-aware for weapons. `EvaluateGear` sets it, so it moves the vote too; it is not equip-only.

**Known gaps:** tier tokens use neither BiS nor scores (the token vote is item level against the
slot, `TokenILevelMargin`), since nothing maps a token to its pieces. An ungrouped level-80 bot is
capped at its own IP tier (T7 after an IP reset), so Ulduar-and-later listings give it no bonus or
bypass until it groups with a real player; vote and trade both happen grouped, so they agree.

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
