# Bots take BiS items, gems, enchants and reforges from the BisTooltipAC dataset

## Context

Bots rank loot and equip decisions from `playerbots_bis_ranked`, generated from the retail-Wowhead
Bistooltip addon lists (`apps/bis/generate_bis.py`). The user now runs a server-tailored pipeline:
wowsimwotlk optimizer → `acbis` export → `acore_world` tables `bistooltip_dataset`,
`bistooltip_subject`, `bistooltip_block` → `mod-bis-tooltip` serves them to the BisTooltipAC addon.
Goal: bots use that dataset for item ranks, and apply its gems, enchants and reforges.

The work runs as an orchestrated **wave loop**, reusing wowsimwotlk's
(`G:\DevStuff\GitHub\wowsimwotlk\docs\wave-loop\wave-loop.RUNBOOK.md` and `wave-loop.workflow.js`):
this session is the orchestrator; per work item (WI) an implementer agent and then a fresh reviewer
agent work in the WI's own worktree; the orchestrator merges WIs into an integration branch, runs one
cross-review agent over the wave's diff, lands the wave on `Custom`, reports and waits for "continue".

## Decisions (settled with the user)

1. **Scope, per bot, never mixed per item:** its roster subject (by character guid) → else a matching
   per-spec subject → else the old Bistooltip lists.
2. **Other items:** exact rank-1 item → the sim's exact enchant, gems, reforge. A different item in the
   slot → the slot's sim enchant if it fits the item; gems from today's picker restricted to the gems
   the sim chose for this bot ("palette"), falling back to today's full pool when nothing fits.
3. **Triggers unchanged:** the dataset changes *what* `ApplyEnchantAndGemsNew` applies, not when.
4. **Reforges on**, exact rank-1 items only, via mod-reforging, plus a local **uncommitted** lock patch
   in the mod-reforging clone.
5. **Grilled:** `Enable = 1` guarded by a placeholder refusal; roster match strict (class + tree +
   role); the sim's enchant trusted over today's blacklist and cloak rule; an exact item whose export
   has no `r` loses its reforge; imports picked up by a version poll plus a reload command;
   `autogear bis` out of scope; pins yield when the meta can't otherwise activate; a bot behind every
   block uses the old lists.
6. **Orchestration:** wowsim wave loop, not the ToC lanes. Agents never build or restart the server; the
   user rebuilds after a landed wave.

## Verified facts

- Payload per slot: `<simSlot>:<item>[,<item>...][(<enh>,...)][+<delta>]`, ≤6 ranked items; only rank 1
  carries `e<enchant SPELL id>`, `g<gem item>` per socket (template sockets in order, then the extra
  prismatic; **empty sockets dropped**), `r<fromItemMod>-<toItemMod>`. Reference codec and vectors:
  `G:\DevStuff\GitHub\wowsimwotlk\tools\database\azerothcore\bisdata_wire.go` (`DecodeBlock`,
  `decodeBisSlot`, `checkBisSlot`, `parseBisInt`), `bisdata_wire_test.go`; spec names
  `bisdata_slots.go` `BisSpecNames`.
- Sim slot → AC `EQUIPMENT_SLOT`: `{0,1,2,14,4,8,9,5,6,7,10,11,12,13,15,16,17}`. Content phase 1..5 =
  `BisProgress{BIS_EXP_WOTLK, 1..5}` from `BisListMgr::ProgressForBot`.
- mod-bis-tooltip's state is private, so playerbots reads the tables via `WorldDatabase`. **A query on a
  missing table or column aborts the worldserver**: guard with `information_schema.COLUMNS` first
  (`FindMissing` in `[ac]/modules/mod-bis-tooltip/src/BisTooltipBridge.cpp` ~137-238).
- Live dataset `b235db6c` is a placeholder (one Fury payload on all 55 subjects). Real exports carry
  roster raiders only, no healers. Roster raiders hold all professions.
- Bot AI (so `ApplyEnchantAndGemsNew`) runs on map threads. mod-reforging's `reforgingDataMap` is an
  unlocked `unordered_map`, read by every player's `OnPlayerApplyItemModsBefore` and erased by
  `OnPlayerAfterMoveItemFromInventory` (trade, mail, AH, guild bank, off-hand auto-unequip).
- The build image `acore/ac-wotlk-build:master` bakes in an **unpatched** mod-reforging, and playerbots
  TUs compile with its `-I`. So `pb-syntax-check.sh` sees `__has_include("item_reforge.h")` true but
  checks playerbots against the old header: playerbots code must compile against both.

Paths are relative to `modules/mod-playerbots/` unless marked `[ac]` (server root) or `[reforge]`
(`[ac]/modules/mod-reforging`).

## Orchestration

The procedure, paths, standing authorizations and agent rules are in
[sim-bis-dataset.RUNBOOK.md](sim-bis-dataset.RUNBOOK.md); the per-wave pipeline is
`sim-bis-dataset.workflow.js`; merge and cross-review checks are `int-check.sh`. Effort base: `b6c4fa9ed`
(`Custom` when `bis-integration` was cut).

## Current wave

- Wave: D, running.
- Base SHA: `24380fe6e`.
- Workflow runId: `wf_a2799f2e-1cd`.

## Status

The orchestrator alone edits this table.

| Wave | WI | State | Merge | Notes |
|---|---|---|---|---|
| A | BIS-codec | merged | `c6714748d` | |
| A | BIS-reforge-lock | applied | | uncommitted in [reforge] by design |
| B | BIS-dataset | merged | `99aa77093` | |
| C | BIS-ranks | merged | `d038ec245` | |
| C | BIS-enhance | merged | `45b8d6566` | |
| D | BIS-docs | pending | | |

States: `pending`, `merged`, `applied` (BIS-reforge-lock: reviewed and left uncommitted), `red`,
`blocked` (worktree kept, reason in Notes).

## Wave registry

| Wave | WIs, in parallel |
|---|---|
| A | `BIS-codec` · `BIS-reforge-lock` |
| B | `BIS-dataset` (stages: loader, source) |
| C | `BIS-ranks` · `BIS-enhance` (stages: refactor, dataset, reforge) |
| D | `BIS-docs`, then the whole-effort cross-review |

## Work items

Common verify for C++ WIs, from the worktree: `PB_REPO=<wt> ~/.claude/scripts/pb-syntax-check.sh
<changed .cpp>`, then `PB_REPO=<wt> PB_MAX_FANOUT=60 ~/.claude/scripts/pb-syntax-check.sh <changed .h>`
(the cap keeps the first 60 TUs alphabetically, so one shared run can cut the changed `.cpp`),
`python tools/pblint/pblint.py <changed paths>`, `PB_SANITIZE=address,undefined sh
tools/nativetest/run.sh`.

### BIS-codec (wave A)
Owned: `src/Mgr/Item/BisWire.h` (new), `tools/nativetest/bis_wire_test.cpp` (new),
`tools/nativetest/run.sh`.
- `BisWire.h`, header-only, std-only, `namespace BisWire` (no clash with mod-bis-tooltip's
  `BisTooltip::Wire` in the shared static lib): `Slot { simSlot; items; enchantSpell; gems; reforgeFrom,
  reforgeTo; hasDelta; delta }`, `Block { slots; Find(simSlot) }`, `Decode(payload, Block&, error)` as a
  faithful port of the Go decoder (canonical integers; ≤1 `e`, then `g*`, then ≤1 `r`; strictly
  increasing slots; 1..6 unique items; delta only with a rank 2). `Checksum()` (Adler-32, 8 lowercase
  hex), `EQUIP_SLOT`, sentinel tabs (10 Feral tank, 11 Blood tank, 12 Fury-Prot),
  `SpecKeyFor(classId, specName) → optional<{tab, variant}>` (Fire FFB and Destruction fire are
  variants of tabs 1 and 2). No `Define.h`.
- Test: `run.sh` gains `-I /module/src/Mgr/Item`; `CHECK` macro as in `raid_instance_state_test.cpp`.
  Vectors from `bisdata_wire_test.go`: full decode, round-trip strings, every-slot-full loop, every
  `TestDecodeBlockRejects` string, `Checksum("Wikipedia") == "11e60398"`, `Checksum("") == "00000001"`,
  table spot checks; plus one real payload with its checksum from
  `C:\Users\boss2\AppData\Local\Temp\claude\g--DevStuff-GitHub-wowsimwotlk\cd047fef-67b5-43a9-9c78-eca3e8ae7548\scratchpad\phase6\bis_dataset_stage2.sql`.
- Verify: nativetest, pblint.
- `BisWire::TAB_*` duplicate `BisSpecTab` (no `Define.h` in the header): change both together.

### BIS-reforge-lock (wave A)
Owned: [reforge]`/src/*` (the callers of `reforgingDataMap` and `GetReforgingData`). No worktree.
- `mutable std::shared_mutex` around `reforgingDataMap`: shared for lookups, exclusive in `LoadFromDB`,
  `Reforge`, `RemoveReforge`, `HandleCharacterRemove`.
- `GetReforgingData` returns `std::optional<ReforgingData>` by value (a pointer into the map outlives
  the lock); its callers switch to `auto` + bool test + `->`, which also compiles against the old
  pointer API.
- The mutex is non-recursive: `_ApplyItemMods` (via `OnPlayerApplyItemModsBefore`) and `SendItemPacket`
  re-enter `GetReforgingData`, so a lock covers only the map access, never those calls or a DB call.
- Verify: `PB_REPO=G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-reforging
  ~/.claude/scripts/pb-syntax-check.sh <changed src/*.cpp>`.

### BIS-dataset (wave B)
Owned: `src/Mgr/Item/BisDatasetMgr.{h,cpp}` (new), `src/Mgr/Item/BisListMgr.{h,cpp}`,
`src/PlayerbotAIConfig.{h,cpp}`, `conf/playerbots.conf.dist`, `src/Script/PlayerbotCommandScript.cpp`,
`src/Script/Playerbots.cpp` (poll hook only).

Stage **loader**:
- `BisDatasetSubject`: id, kind, guid, name, cls, specName, tab (`BIS_TAB_NONE` if unmapped), variant,
  `blocks[1..5]`, `ranks` item → [(phase, best rank across sim slots)], `PhaseAtOrBelow(cap)`,
  `RankFor(item, cap, outPhase)` with `GetBisRankFor` semantics.
- `BisDatasetSnapshot`: version/simCommit/catalogDate/objective, subjects, `rosterByGuid`, `specByKey`
  (`cls<<8|tab`; canonical beats variant; duplicate canonical → lowest id + warning), `items` set.
- `BisDatasetMgr` singleton: `Reload(error)`, `Get() → shared_ptr<Snapshot const>` (mutex-guarded swap,
  `AiFactory` style), `Describe()`, `Update(diff)`. `static_assert`s `BisWire` tabs and slots against
  `BIS_TAB_*` / `EQUIPMENT_SLOT_*`.
- Load: column guard (only the selected columns; missing → no dataset, info log, success) → exactly one
  dataset row → subjects, blocks (subject exists; phase 1..5, 6..9 ignored with a warning; checksum
  matches; `Decode` succeeds; any failure rejects the whole dataset) → re-read version, fail if changed →
  **placeholder refusal** (one distinct payload across subjects of ≥2 classes). A failed reload keeps
  the old snapshot.
- Called in `PlayerbotAIConfig::Initialize` right after `sBisListMgr->LoadAll()` (~`:847`). That runs
  from `OnBeforeWorldInitialized`, before DBCs, `SpellMgr` and `ObjectMgr`: decode and index only.
- `Update(diff)` from `PlayerbotsWorldScript::OnUpdate` (`src/Script/Playerbots.cpp` ~403, world
  thread): every `PollSeconds`, column guard + `SELECT version`; `Reload` on a change or when the tables
  appear or vanish; a failed reload logs once per version.
- `.playerbots bis reload` (SEC_ADMINISTRATOR) / `status` (SEC_GAMEMASTER), `Console::Yes`, under
  `playerbotsCommandTable`; `status` also lists dataset enchant spells without
  `SPELL_EFFECT_ENCHANT_ITEM` and gems without `GemProperties`.
- Config (`PlayerbotAIConfig.h` ~434, `.cpp` ~786, conf.dist after `Bis.PhaseDecay`):

  | Key | Default | Meaning |
  |---|---|---|
  | `AiPlayerbot.BisDataset.Enable` | 1 | Read `bistooltip_*` and prefer it per bot; no-op without the tables or with a placeholder export |
  | `AiPlayerbot.BisDataset.Enhancements` | 1 | Dataset enchants, pinned gems, palette; 0 keeps only the rank/gate signal |
  | `AiPlayerbot.BisDataset.Reforges` | 1 | Sim reforge on exact rank-1 items; no-op without mod-reforging or `Reforging.Enable = 0` |
  | `AiPlayerbot.BisDataset.PollSeconds` | 300 | World-thread version check; 0 = startup and the command only |
- As built: `BisDatasetMgr.h` includes `BisListMgr.h` (tabs, `BIS_PHASE_MAX`), so `BisListMgr.h`
  forward-declares `BisDatasetSnapshot`/`BisDatasetSubject`. Singleton `sBisDatasetMgr` is a reference.
  An empty `bistooltip_dataset` means no dataset, not a failure. Indexes skip subjects with no phase
  1..5 block or an unmapped spec name (warning). A failed reload records the version it read, so the
  poll retries only a new one; "changed while loading" keeps the starting version, so it retries.

Stage **source** (`BisListMgr`):
```cpp
struct BisSource {
    enum class Kind : uint8 { None, Lists, Dataset } kind;
    uint8 cls, tab;                                     // ResolveSpecKey result
    BisProgress progress;                               // Dataset: {WOTLK, effective block phase}
    std::shared_ptr<BisDatasetSnapshot const> dataset;  // pins the snapshot across a reload
    BisDatasetSubject const* subject;
};
BisSource ResolveSource(Player* bot, BisProgress max) const;
uint8 GetBisRankFor(uint32 itemId, BisSource const& src, uint8* outPhase = nullptr) const;
```
1. `ResolveSpecKey` fails → None. 2. Default Lists with `progress = max`; stop if disabled, no snapshot,
   or `max.expansion != WOTLK`. 3. Roster `rosterByGuid[guid low]` when `cls` and `tab` match (sentinels
   as tabs) and `PhaseAtOrBelow(max.phase) != 0`. 4. Else spec `specByKey[cls<<8|tab]`, same check,
   variants only without a canonical subject. 5. Match → Dataset, `progress.phase = PhaseAtOrBelow`.
- `GetBisRankFor(src)`: None → 0; Lists → existing overload; Dataset → `subject->RankFor`.
  `GetBisRank`: cheap reject in neither `_ranked` nor the snapshot's `items`, then `ResolveSource`.
  `IsBisListed` and `ItemUsageValue.cpp` stay untouched.
- Verify: common; syntax check includes the `.cpp` files including `BisListMgr.h` and
  `PlayerbotAIConfig.h` (fan-out 60).
- As built: `GetBisRank` reads the snapshot once and passes it to a private static `ResolveSource`
  overload.

### BIS-ranks (wave C)
Owned: `src/Mgr/Item/StatsWeightCalculator.{h,cpp}`, `src/Ai/Base/Actions/TellLosAction.cpp`.
- `StatsWeightCalculator.h` (~122-128): `bis_key_valid_/bis_cls_/bis_tab_/bis_progress_` → `BisSource
  bis_source_`. `BisRankMultiplier` (~`.cpp:233-273`) resolves once and decays against
  `bis_source_.progress.phase` (the effective phase; otherwise a dataset without an RS block decays every
  item to 0). Rank scale `{1.0, 0.8, 0.6}` unchanged.
- `calc` (`TellLosAction.cpp` ~152-182): print by kind; the Dataset line names version, subject (roster
  name or spec), effective phase and cap, rank, listed phase, and the any-phase rank.
- Verify: common.
- As built: the held `BisSource` pins the snapshot for the calculator's lifetime. `calc`'s any-phase
  rank reruns the source at `progress.phase` `BIS_PHASE_MAX` (Dataset) or `BIS_MAX_PHASE[expansion]`
  (Lists); its Lists and no-key output is unchanged.

### BIS-enhance (wave C)
Owned: `src/Bot/Factory/PlayerbotFactory.{h,cpp}`, `src/Bot/Factory/BisReforge.{h,cpp}` (new; the
world-thread operation lives in `BisReforge.cpp`, not the shared `PlayerbotOperations.h`).

Stage **refactor** (behaviour-neutral) in `ApplyEnchantAndGemsNew` (~5352-5950):
`gemEnchantIfUsable(gem)` (body of the `availableGems` loop); `gemBudgetOk(gem, usedById,
usedByCategory)` (unique/limit block of `pickBestGem`); `pickBestGem` takes the pool; `pickGem` tries
`paletteGems` then `availableGems` at the meta pick, colored fill and meta steering; the enchant scan
split into `spellGatesPass` / `enchantGatesPass`.

Stage **dataset**:
- Plan helper near `GetEnchantIdOfSpell` (~`:124`): resolve `BisSource` once; when Dataset and
  `Enhancements` or `Reforges` (enchants, pins and palette need `Enhancements`), take the
  effective-phase block and build per AC slot `{sim slot, exact}`. Weapons strictly per hand
  (Titan's Grip = two independent matches; a 2H with no sim 15 leaves the off-hand to today's
  picker). Rings: same-index exact pass, then cross pass; a non-exact ring takes the unused
  sim slot for its enchant. Trinkets carry no enhancements. `paletteGems` = the block's `g` ids passing
  `gemEnchantIfUsable` (not restricted to the gem cache; `gemBudgetOk` enforces unique-equipped).
- Enchant per slot, first that applies: (1) the dataset spell, exact or not, when it has
  `SPELL_EFFECT_ENCHANT_ITEM`, passes `spellGatesPass` (`LimitEnchantExpansion`,
  `IsFitToSpellRequirements`, `BaseLevel`, `IsEnchantSpellAllowed(tier)`) and the enchant's
  `requiredSkill`/`requiredLevel`; deliberately not the cache blacklist, enchant-flags check or
  engineering-cloak rule (the sim chose from the server's obtainable catalog, and it carries DK
  runeforges the cache path never sees); (2) `GetRuneforgeEnchantId`; (3) today's scan.
- Pinned gems (exact items): `pinnedGem`/`pinned` on `SocketToGem`. After `ApplyPrismaticSocket`, with
  T = colored template sockets (meta included), P = prismatic present, n = dataset gems:

  | Case | Mapping |
  |---|---|
  | n == T+P | positional; gem T goes to the prismatic socket |
  | n == T+1, P == 0 | the sim had a prismatic the bot lacks: drop the trailing gem |
  | n == T, P == 1 | pin template sockets; the prismatic goes to the picker |
  | otherwise, or a meta/non-meta mismatch | pin nothing on this item (dropped empty sockets make it ambiguous) |

  Pinned pass after the meta pre-deactivate, before the meta pick: a pin must pass
  `gemEnchantIfUsable` and `gemBudgetOk`, else that socket falls to palette → full pool. Valid colored
  pins apply now and `trackGem`; a valid meta pin joins `metaToApply` and sets `metaCondition`.
- Later passes skip pinned sockets. Steering tries unpinned sockets first; only if the meta still can't
  activate does it re-gem pinned ones, unpinning each. The bonus swap loop skips pairs with a
  still-pinned socket but keeps them in bonus scoring.

Stage **reforge** (exact items, `Reforges` on, mod-reforging present and enabled):
- `BisReforge.{h,cpp}`: `#if __has_include("item_reforge.h")` (include `<string> <vector>
  <unordered_map>`, `Item.h`, `Player.h` first; the header uses them without including them).
- The map thread only queues: `BisReforgeOperation{botGuid, itemGuid, entry, from, to}` (from = to = 0
  means remove) via `PlayerbotWorldThreadProcessor::instance().QueueOperation`. It never reads
  mod-reforging state, since its map is shared across threads.
- World thread: find the bot and item; require still equipped, same entry, `GetEnabled()`, and for a
  reforge `IsReforgeableStat(from) && IsReforgeableStat(to)` (`Reforge()` checks neither; failing
  either keeps any old reforge). Read `GetReforgingData` only as `auto data = ...; if (data)` or
  `!data`, then `->` or `*`: `nullptr` compares, `has_value()`, `value()` and `value_or` each break
  against the baked pointer API or the patched `optional`. Already `{from, to}` → done. Reforged
  otherwise → `RemoveReforge` only; a later request reforges. Unreforged → `Reforge` for a reforge
  request. Failures log at debug.
- At most one write per item per 10 s (world-thread map, item guid → last write): `RemoveReforge`'s
  async DELETE and `Reforge`'s INSERT share the `character_reforging` key (`item_guid`) and race once
  `CharacterDatabase.WorkerThreads > 1`, and the queue drains ≥50 ms apart, ≤100 ops each, so two
  `ApplyEnchantAndGemsNew` runs' requests can share a batch.
- The queue drains in `OnWorldUpdate`, after `sMapMgr->Update` has waited out every map thread: that,
  not the lock, makes touching the bot safe.
- Call site in the slot loop: exact item with `r` → request it; exact item without `r` → request
  removal; non-exact → nothing. The first pass scores unpinned sockets on pre-reforge stats; the next
  trigger converges.
- Verify: common, with `src/Bot/Factory/PlayerbotFactory.cpp` and `BisReforge.cpp` in the syntax check.

### BIS-docs (wave D)
Owned: `docs/systems/itemization.md`, `docs/systems/loot.md`. With `/compact-docs-writer`, promote the
WI sections' durable facts:
- itemization.md, new "Sim BiS dataset" after "Ranked BiS lists": source order, effective phase, exact
  vs palette, the alignment table and why, pins budgeted first and yielding only to the meta, enchant
  validity vs the cache path, runeforge precedence, reforges queued to the world thread and the
  mod-reforging lock dependency, the deliberate `BestGemScore` exception (the palette narrows
  placement, not socket valuation). Replace the "drops gems and enchants" line; config rows for the
  four keys.
- loot.md "Ranked BiS lists": the signal comes from the dataset when the bot resolves to it; replace the
  no-reload line (lists: restart or `.playerbots rndbot reload`; dataset: poll, or `.playerbots bis
  reload|status`).
- Known gaps: healers have no dataset blocks; acbis labels a raider "Blood dps" without the main-tank
  flag, so a tanking DK falls back to the lists.
- Keep `Reforging.Enable` fixed at runtime: mod-reforging's reload walks only `sWorldSessionMgr`
  sessions, which hold no bots, so a toggle leaves reforged bots' stats drifted until they relog.
- After the whole-effort cross-review lands, the orchestrator deletes `docs/plans/sim-bis-dataset/` in
  a final commit on `bis-integration` and lands it.

## Verification

- Per WI: its verify commands (implementer, then the reviewer re-runs them).
- Per merge and after the cross-review: `int-check.sh <pre-merge sha> <name>` in [int].
- Live, by the user after landing (agents never rebuild): rebuild with a CMake re-run; the startup log
  shows the placeholder refusal for `b235db6c`, and `.playerbots bis status` agrees. After a real
  import, the poll logs the new version within `PollSeconds`; `calc <item>` on a roster bot prints the
  dataset line; `maintenance` on it gives the sim's enchants and gems on rank-1 items, palette gems on
  others, and the reforge a tick later; a non-roster bot's `calc` still prints the lists.
