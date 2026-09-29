# Sim-derived stat weights for level-80 DPS bots

## Context

Bots score every item, gem and enchant as a dot product of the item's stats and a per-spec weight
table (`StatsWeightCalculator::GenerateBasicWeights`, `src/Mgr/Item/StatsWeightCalculator.cpp`). The
table is hand-written, flat across phases, and predates mod-spell-tweaks. A comparison against wowsim
(the server-exact sim at `G:\DevStuff\GitHub\wowsimwotlk`, "[sim]") at each phase's server BiS gear
found it off by 2x or more on at least one stat for most DPS specs:
- **Hit** is 2 to 2.8x too low almost everywhere.
- **Agility and attack power** are too low for Strength classes.
- **Weapon DPS** is 13 vs 7 for Arms and 2.4 vs 5 for Assassination and Subtlety.
- **Armor penetration, hit and expertise** rise steadily from P1 to P5. For example, Fury ArP goes 2.1 to 3.8.

Goal: level-80 DPS bots take their weights from wowsim, per spec and per gear level. The hand-written
table stays for everything the sim doesn't cover.

Paths are relative to `modules/mod-playerbots/` unless marked [sim] or `[ac]` (the server root
`G:\DevStuff\GitHub\azerothcore-wotlk-pb`). Content phase N = progression tier 12+N, as in
`BisListMgr::ProgressForBot` and [sim]`/CONTEXT.md`.

## Decisions

Settled with the user:
1. Weights come from wide-step sims at the BiS presets, never from the sim's own Stat Weights tool.
   That tool moves each stat ±20, which lands on rotation breakpoints and misreads haste in 19 of 28
   builds. For example, it reads Combat haste as 11.8 where the wide step reads 3.1, and some of its
   values are higher than haste can physically give.
2. Weights are per phase, because hit, expertise and ArP move 1.5 to 2x between P1 and P5.
3. **Scope:** level-80 bots with a DPS row and not specced for PvP. Everything else stays hand-written:
   - tanks, because survival (TMI) depends on the sim's healing model, and threat and survival would
     need a blend the bots don't have;
   - healers, because their sim pages hang in headless Chrome;
   - Beast Mastery hunters, because there is no BM BiS preset;
   - levels below 80, because rating value per point changes with level;
   - PvP specs.
4. **Gear level picks the weights, not content phase.** Each phase's row carries the average item level
   of its BiS gear. A bot interpolates linearly between the two rows bracketing its own average item
   level, clamped to P1 and P5. `ProgressForBot` doesn't fit, because it follows the group leader's
   quest tier or IP's login default, not the bot's gear. That default is phase 1 for an ungrouped
   level-80 bot on this server (`IndividualProgression.Enable = 1`, `AiPlayerbot.LimitProgressionTier = 1`).
   What moves hit and ArP is the bot's own gear.
5. **Cap-aware equip comparisons ship together with the weights.** Loot, equip, buy and cross-armor
   scoring run with `SetOverflowPenalty(false)`, so hit past the cap still counts. With hit roughly 2x
   higher, a capped bot would pick hit gear over better items. The slot comparisons therefore clip hit,
   expertise, ArP and defense against the caps, with the contested slot's current item removed.
6. **On by default**, with `AiPlayerbot.SimWeights.Enable = 0` restoring the hand-written weights.
7. **Wrong-branch fixes in `GenerateBasicWeights`, at every level:**
   - a Blood DK that is not a tank takes the Unholy DPS branch;
   - a Frost or Unholy DK that is a tank takes the Blood tank branch;
   - a non-Shadow priest that is not a healer (Smite) takes the caster branch.

   "Tank" and "healer" are `PlayerbotAI::IsTank` and `IsHeal`, which read the bot's strategies.
   Blood DK bots always get the "tank assist" strategy (`AiFactory.cpp`), so Blood DPS only exists when
   a user removes it. A random ungrouped priest with both "holy dps" and a heal strategy still reads as
   a healer.
8. **Haste is measured with ±300 rating**, not ±100 (WI `SW-gen` verifies this).
   - With ±100, haste zig-zags between phases: Arcane reads 1.05 / 1.67 / 0.51 / 1.25 / 1.57 for P1 to
     P5, Elemental 0.59 / 0.72 / 1.41 / 0.63 / 0.43.
   - That comes from real haste thresholds, not noise.
   - The wider step averages across the thresholds that a bot's non-BiS gear lands around.
9. **One wowsim JSON, one generated SQL file.** wowsim emits sim-native slopes. A playerbots script maps
   them to bot stats and writes a playerbots-DB update, like `apps/bis/generate_bis.py` does for the
   BiS lists.

## Measurement method

- **Inputs:** each build's `IndividualSimSettings`:
  - from its page's own defaults after the build's talent preset (buffs, debuffs, consumes,
    encounter, rotation);
  - race and all professions as presetgen sets them;
  - gear from [sim]`/ui/<dir>/gear_sets/p<N>_bis[_<tree>].gear.json`, P1 to P5;
  - racial traits from that directory's `bis_presets.json`.
- **Capturing the settings needs the browser.**
  - An exported settings JSON keeps the rotation as `TypeAuto` without the APL the page resolves. Such
    an export sims Fire Mage at 0 DPS and Fury at 5.9k DPS instead of 19.7k.
  - So capture the `ComputeStatsRequest` the page posts to its worker: hook
    `Worker.prototype.postMessage` and keep the last message whose `msg === 'computeStats'`.
  - Do that after `__pg.importSettings` of the phase's settings, wait until no new request arrives for
    2 s, and save its `inputData` bytes.
  - `Raid.Parties[0].Players[0]` plus the raid and party buffs, debuffs, tanks and encounter rebuild
    the `IndividualSimSettings`.
  - There is no browser-free path: UI defaults live only in TS (`ui/<spec>/sim.ts`).
- **Steps:**
  - Each stat's slope is `(sim(base+shift+δ) − sim(base+shift−δ)) / 2δ`, applied through
    `Player.BonusStats` (pseudo stats for weapon DPS).
  - Seed 424242, `IsTest`, 10,000 iterations, `core.RunRaidSimAsync`. Record DPS, TPS and TMI.

  | Stat | δ | Shift |
  |---|---|---|
  | Primary stats, crit, MP5, spirit | 100 | 0 |
  | Haste | 300 | 0 |
  | AP, RAP, SP | 200 | 0 |
  | Expertise | 30 | −60 |
  | Melee hit | 100 | −200 |
  | Spell hit | 100 | −250 |
  | ArP | 100 | `max(−100, 100 − base ArP)` |
  | Weapon DPS (MH, OH, ranged) | 10 | 0 |

  - The hit and expertise shifts measure below the cap. The bots already clip anything past it.
  - ArP clamps at 0% (`Unit.ArmorPenetrationPercentage`), so its range must start at or above 0 rating.
    Without that, low-ArP gear reads 0 (Enhancement P1 showed 0.00 instead of 1.29).
- **Stat lists per role:**
  - melee: Str, Agi, Int, SP, AP, MeleeHit, SpellHit, MeleeCrit, SpellCrit, MeleeHaste, SpellHaste,
    ArP, Expertise, MH and OH weapon DPS;
  - hunter: Agi, Int, AP, RAP, MeleeHit, MeleeCrit, MeleeHaste, ArP, ranged DPS;
  - caster: Int, Spirit, SP, MP5, SpellHit, SpellCrit, SpellHaste.
- **Cost:** about 15 s per build and phase (DKs 45 to 100 s). All 140 run in about 40 minutes on 16
  cores, plus about 30 minutes of capture.
- **Prototype:** `C:\Users\boss2\AppData\Local\Temp\claude\g--DevStuff-GitHub-azerothcore-wotlk-pb-modules-mod-playerbots\4cf7f0a1-dc5c-4545-af74-c637fa4645c2\scratchpad\sw\`
  (`capture.py`, `export.py`, `run/wide.go`, `phases.py`) implements all of the above except the
  ±300 haste step. It may be gone; this section is the spec.

## Data flow

```
[sim] tools/statweights  --capture-->  settings/<build>_p<N>.bin  --run-->  weights.json
apps/simweights/generate_weights.py  (weights.json + [ac] item_template.sql)
  --> data/sql/playerbots/updates/<YYYY_MM_DD>_00_playerbots_sim_weights.sql
  --> playerbots DB (applied at startup by the module's DB updater; a changed file re-applies)
  --> SimWeightsMgr (loaded in PlayerbotAIConfig::Initialize)  -->  StatsWeightCalculator
```

## Work items

Order: `SW-gen` → `SW-data` → `SW-core`. `SW-caps` can run anytime; `SW-docs` goes last. `SW-core`
can start on a hand-written fixture SQL before `SW-data` lands.

### SW-gen: wowsim generator ([sim])

Follow [sim]`/CLAUDE.md`: read its doc map rows for dev-environment, testing, workflow and
workstreams. Go runs in the toolchain container. Its phase loop applies, and nothing is committed until
the user says "commit and merge".

- New `tools/statweights/`:
  - **Capture step (Python):** reuses `tools/presetgen/gen.py` (`BUILDS`, `Browser`, `PAGE_JS`,
    `gear_path`, `gear_stem`, `ALL_PROFESSIONS`) read-only, as described under Measurement method.
  - **Runner (Go):** does the stepping.
  - **Merge step:** writes one `weights.json` holding:
    - `sim_commit` and `generated_at`;
    - per build: key, class, spec;
    - per phase: the gear's item ids, base DPS, and per stat `{delta, shift, dps, tps, tmi}`.
  - Resumable: skip outputs that already exist, and re-run a slope whose step changed.
  - Flags: `--base`, `--port` like presetgen, plus `--builds`, `--phases`, `--iterations`.
- Add a README with prerequisites and run commands, a doc map row in [sim]`/CLAUDE.md`, and a row in
  `docs/guide/workstreams.md` owning `tools/statweights/`.
- **Verify:**
  - `go vet` and a build under `with_db`.
  - A full run: P1/P5 results match the comparison within 10%, with the anchor scaled to the bots'
    own weight (Fury P5 hit ≈ 4.7 with Str = 2.5; Arms P5 weapon DPS ≈ 13).
  - Haste with ±300 no longer zig-zags for Arcane, Elemental and Enhancement: each phase-to-phase
    change stays within 40% of the lower value. If it still zig-zags, report the numbers. The fallback,
    a 3-phase moving average of haste in `SW-data`, is the user's call.
  - A second run with seed 777 differs by at most 0.1 in those units on every stat above 0.3.

### SW-data: converter and generated SQL (playerbots)

Owned: `apps/simweights/generate_weights.py` (new), the generated file in `data/sql/playerbots/updates/`.
Mirror `apps/bis/generate_bis.py`: its CLI, its reading of `[ac]/data/sql/base/db_world/item_template.sql`,
and its header and DDL layout.

- **Builds to keys** (class id, tab):

  | Class | Builds and keys |
  |---|---|
  | Warrior | `warrior_arms` 1/0, `warrior_fury` 1/1 |
  | Paladin | `retribution_paladin` 2/2 |
  | Hunter | `hunter_mm` 3/1, `hunter_sv` 3/2 |
  | Rogue | `rogue_assassination` 4/0, `rogue_combat` 4/1, `rogue_subtlety` 4/2 |
  | Priest | `shadow_priest` 5/2, `smite_priest` 5/13 (`SIM_TAB_PRIEST_SMITE`) |
  | Death Knight | `dk_blood` 6/0 (Blood DPS), `dk_frost` 6/1, `dk_unholy` 6/2 |
  | Shaman | `elemental_shaman` 7/0, `enhancement_shaman` 7/1 |
  | Mage | `mage_arcane` 8/0, `mage_fire` 8/1, `mage_frost` 8/2 |
  | Warlock | `warlock_affliction` 9/0, `warlock_demonology` 9/1, `warlock_destruction` 9/2 |
  | Druid | `balance_druid` 11/0, `feral_druid` 11/1 |

  Tank and healer builds are skipped. Exit 1 on an unmapped build.
- **Sim stats to bot stats**, each row the sum of its sim slopes, DPS metric:
  - Direct: `STRENGTH`, `AGILITY`, `INTELLECT`, `SPIRIT`, `SPELL_POWER`, `MANA_REGENERATION` ← MP5,
    `ARMOR_PENETRATION`, `EXPERTISE`.
  - `ATTACK_POWER` ← AttackPower, plus RangedAttackPower for hunters.
  - `HIT` ← MeleeHit + SpellHit, and likewise `CRIT` and `HASTE`. A rating item gives both halves.
  - `MELEE_DPS` ← MainHandDps; `RANGED_DPS` ← RangedDps.
  - A stat the role didn't sim gets no row. It is unmeasured, not worth 0.
- **Anchor:** `STRENGTH` for warriors, DKs and Ret; `AGILITY` for rogues, hunters, Feral and
  Enhancement; `SPELL_POWER` for casters. Every weight is divided by the anchor's slope, so the anchor
  row is 1.0.
- **`gear_ilvl`:** the phase gear's mean raw `ItemLevel` over slots 0 to 18. It skips body, tabard,
  offhand and ranged, and counts an empty slot as 0, exactly as `Player::GetAverageItemLevelForDF`
  counts.
- **DDL:** DROP + CREATE + INSERT, with a header naming the sim commit and generation date.

  ```sql
  CREATE TABLE `playerbots_sim_weights` (
    `class` TINYINT UNSIGNED NOT NULL,
    `tab` TINYINT UNSIGNED NOT NULL,
    `phase` TINYINT UNSIGNED NOT NULL,   -- 1..5
    `gear_ilvl` FLOAT NOT NULL,
    `stat` VARCHAR(24) NOT NULL,         -- StatsType name without STATS_TYPE_
    `weight` FLOAT NOT NULL,             -- per point, anchor = 1.0
    `anchor` TINYINT UNSIGNED NOT NULL,  -- 1 on the anchor row
    PRIMARY KEY (`class`, `tab`, `phase`, `stat`));
  ```

- The filename date must be unique among `data/sql/playerbots/updates/` files; a duplicate name is
  fatal to the updater.
- **Verify:** run it on the `SW-gen` output. Spot-check that Fury P5 `HIT` ≈ 1.9 (4.7 / 2.5 in anchor
  units) and that no row has `anchor = 1` with a weight other than 1.0.

### SW-core: loader and scoring (playerbots)

Owned:
- `src/Mgr/Item/SimWeights.h` (new) and `src/Mgr/Item/SimWeightsMgr.{h,cpp}` (new);
- `src/Mgr/Item/StatsWeightCalculator.{h,cpp}`;
- `src/PlayerbotAIConfig.{h,cpp}` and `conf/playerbots.conf.dist`;
- `src/Ai/Base/Actions/TellLosAction.cpp`;
- `tools/nativetest/sim_weights_test.cpp` (new) and `tools/nativetest/run.sh`.

- **`SimWeights.h`**, header-only and std-only so nativetest can build it, in `namespace SimWeights`.
  Model it on `BisWire.h`.
  - The stat name ↔ index table uses `StatsType` values; the `.cpp` `static_assert`s them against
    `StatsCollector.h`.
  - `TAB_PRIEST_SMITE = 13`, which must not collide with `BisSpecTab`'s 10-12 and 0xFF.
  - `struct PhaseRow { uint8 phase; float ilvl; float w[N]; uint32 measured; }` with an anchor index
    per spec.
  - `Interpolate(rows sorted by ilvl, float ilvl, float out[N], uint32& measured)`: linear between the
    bracketing rows, clamped at both ends.
  - `Merge(float const sim[N], uint32 measured, int anchor, float hand[N])`:
    - Scale every measured stat by `hand[anchor]`, or by 1.0 if that is not positive.
    - Overwrite `hand` with the result, except where `hand < 0` and `|sim| < 0.05`. Those keep their
      negative weight, which exists to repel off-role items (for example SP on melee).
    - Unmeasured stats keep the hand-written value.
- **`SimWeightsMgr`:**
  - `LoadAll()` from `PlayerbotAIConfig::Initialize`, next to `sBisListMgr->LoadAll()`.
  - Queries via `PlayerbotsDatabase.Query`, logging and staying empty like `BisListMgr::LoadRanked`.
  - Builds an immutable snapshot of spec key → rows. Rows with a bad phase, unknown stat or missing
    anchor are dropped with a warning.
  - `Get()` returns the snapshot's `shared_ptr` under a mutex, with a swap on reload. This is the
    `BisDatasetMgr::Get`/`Swap` pattern. It is required because `Initialize` re-runs on
    `.playerbots rndbot reload` (`RandomPlayerbotMgr.cpp`) and `PlayerbotMgr.cpp` while bots score
    items.
  - `static bool ResolveKey(Player*, uint8& cls, uint8& tab)`:
    - DK Blood and not a tank → 6/0;
    - Frost/Unholy DK and not a tank → its own tab;
    - non-Shadow priest and not a healer → 5/13;
    - Feral and not a tank → 11/1;
    - Beast Mastery → false;
    - other tanks and healers → false;
    - everything else → its own tab.
- **`StatsWeightCalculator`:**
  - Resolve lazily on the first `GenerateWeights`, like `bis_source_`, and keep the result for the
    calculator's lifetime.
  - Active only when all of these hold: config on, `lvl == 80`, `!pvpSpec_`, `ResolveKey` succeeds,
    and the snapshot has that key.
  - Otherwise keep a reason string for `calc`.
  - When active, interpolate at `player_->GetAverageItemLevelForDF()` into member arrays, with the
    snapshot released after the copy.
  - `GenerateWeights` = basic + additional (hand-written), then `Merge`, then `ApplyWeightFinetune`
    with the ArP ×1.2 skipped when active, since the phase rows already carry ArP's rising value.
  - Talent bumps on measured stats (Careful Aim, Mental Dexterity) are overwritten by `Merge`. The sim
    builds already carry those talents.
  - Apply the decision-7 branch routing in `GenerateBasicWeights` at all levels.
- **Config:** `AiPlayerbot.SimWeights.Enable` (bool, default 1), added to `PlayerbotAIConfig` and
  conf.dist after the `BisDataset` keys.
- **`calc`** prints one more line: either `weights: sim <class>/<tab> ilvl <x> (P<a> <pct>% → P<b>)`
  or `weights: hand-written (<reason>)`.
- **Verify:** the common playerbots checks (`CLAUDE.local.md`). The nativetest cases:
  - interpolation below P1, between phases and above P5;
  - Merge with anchor scaling, the negative-weight keep, an unmeasured keep, and a non-positive hand
    anchor.

### SW-caps: cap-aware equip comparisons (playerbots)

Owned: `src/Mgr/Item/StatsWeightCalculator.{h,cpp}` (only the replaced-item part), `src/Ai/Base/Value/ItemUsageValue.cpp`,
`src/Ai/Base/Actions/EquipAction.cpp`. Run it after `SW-core`, or in the same stage.

- Replace `SetReplacedItemSet(uint32)` with `SetReplacedItem(Item const*)`.
  - It keeps the set-id behaviour.
  - It also records what the item supplies toward each cap: hit, expertise, ArP and defense rating from
    its template, random property, enchants and gems. Collect these with a `StatsCollector` over
    `CollectItemStats` and the item's enchant slots.
- When a replaced item is set, `ApplyOverflowPenalty` runs even with the penalty off.
  - Each cap's remaining points are computed from the bot's current value minus that item's
    contribution.
  - So incumbent and challenger are both clipped against the same baseline, with the slot empty.
  - Without a replaced item nothing changes. Unslotted scores such as the first `CalculateItem` in
    `QueryItemUsageForEquip` stay unclipped.
- Switch every current `SetReplacedItemSet` call site:
  - `ItemUsageValue.cpp`, the cross-armor loop and the `QueryItemUsageForEquip` slot loop;
  - `EquipAction.cpp`, the dual-wield and Titan's Grip pair and the ring/trinket pair.
- **Verify:** the common checks. `SW-docs` records the behaviour.

### SW-docs

With `/compact-docs-writer`, in `docs/systems/itemization.md`:
- a "Sim stat weights" section after "Talent-driven stat weights", covering:
  - the source and scope;
  - interpolation by gear level and why not content phase;
  - merge rules;
  - the ArP finetune skip;
  - the branch routing;
  - cap-aware slot comparisons and why unslotted scores stay unclipped;
  - the known gaps below;
  - the regeneration steps, pointing at the [sim] tool README and the converter.
- the config row;
- fix the stale line references the section rewrite touches.

Then delete this plan.

## Verification (user, after landing and a rebuild)

- The startup log shows the sim-weights row count.
- `calc <item>` on a level-80 Fury bot prints the sim line with an ilvl bracket. On a level-79 bot or
  a tank it prints hand-written with the reason.
- A hit-capped DPS bot offered a hit item and an equal-ilvl crit or haste item prefers the latter
  (`SW-caps`).
- `AiPlayerbot.SimWeights.Enable = 0` and a restart bring back the hand-written weights.

## Known gaps

- Tanks, healers, BM hunters, PvP specs and levels below 80 keep hand-written weights.
- Feral weapons carry no feral attack power in `StatsCollector` (weapon DPS is scored as weapon DPS).
  Bear gear therefore undervalues weapons badly: the sim puts bear threat at about 51 per weapon DPS,
  against the bots' 3.
- The sim builds use presetgen's talents, race and professions. A bot on a different talent build
  gets slightly different true values.
