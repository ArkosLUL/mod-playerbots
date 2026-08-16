# BiS data: Vanilla/TBC support + regeneration pipeline

## Context

`playerbots_bis_ranked` feeds bot loot votes and equip decisions in mod-playerbots. It is imported
from the Bistooltip addon at `A:\WOW\world of warcraft 3.3.5a hd\interface\addons\Bistooltip\`, and
covered WotLK only. Three questions drove this work: is the data stale, can Vanilla and TBC be added,
and can all of it reach the module.

### What the investigation established

**The WotLK item data was not meaningfully stale.** The addon's rank-1 pick was compared against live
wowtbc.gg across 12 specs x 6 phases = 1071 (spec, phase, slot) cells: **98% agreement**,
faction-twin-aware (PR 98%, T7 96%, T8 97%, T9 99%, T10 100%, RS 100%). Most disagreements were the
old local file's custom-realm ids `130023 / 130031 / 150005` injected at rank 1 on weapon slots.

**The addon has since been replaced with upstream** ([Spyr581/Bistooltip](https://github.com/Spyr581/Bistooltip),
master), which already ships all three expansions and the source switch. That resolves the
Vanilla/TBC question and drops the custom ids.

**wowtbc.gg is scrapable.** Gatsby site; `/page-data/<exp>/bis-list/<slug>/page-data.json` returns
`bisList` (per-item, per-phase `bis` flag + `enchant` + `gems`) and `gearData` (name, ilvl, rarity,
slot, armour type, stats, sockets). No item ids — resolution is by name + ilvl. No repo in the fork
chain ships a generator, so this is the only real refresh path.

**The BiS score bonus already reaches loot rolls.** `SetBisBonus(true)` at
`src/Ai/Base/Value/ItemUsageValue.cpp:1325` lives inside `QueryItemUsageForEquip`, and that same call
produces the `usage` that voting consumes: `LootRollAction` -> `"loot usage"` ->
`LootUsageValue::Calculate` (`:1246`) -> `ItemUsageValue::Calculate` -> `QueryItemUsageForEquip`.

**Two conf values on this server are overridden by env and the conf is misleading** (per the module's
CLAUDE.md rule, read with `docker exec ac-worldserver env | grep ^AC_`):

| Key | Conf | Effective |
|---|---|---|
| `AiPlayerbot.Roll.UpgradesOnly` | `0` | **`1`** |
| `AiPlayerbot.Roll.TokenILevelMargin` | `0.1` | **`5`** |

`UpgradesOnly = 1` is intended and stays. It makes `FinalizeRollVote` short-circuit to
`(vote == NEED && (usage == EQUIP || REPLACE)) ? GREED : PASS`, so **bots never NEED** and the BiS
signal decides **GREED vs PASS**. `TokenILevelMargin` is an absolute ilvl delta
(`token->ItemLevel >= oldProto->ItemLevel + margin`, `:2353`), not the ratio its `0.1` default
suggests. No `AC_*` overrides exist for any `Bis.*` key.

**Raids run Group Loot**, so the voting path is live (`LootRollAction.cpp:67-68` forces PASS only on
`MASTER_LOOT` / `FREE_FOR_ALL`).

### Current state of the addon directory

Upstream's `Bistooltip.toc` loads `Loot_Sources.lua`, `Bistooltip_classes.lua`, the three
`Bistooltip_{wotlk,tbc,classic}_bislists.lua`, `Bistooltip_horde_to_ali.lua`, `Core.lua`,
`Config.lua`, `Bistooltip.lua`, `Bislist.lua`. `Config.lua:10-20` defines all three sources and
`Config.lua:247-261` dispatches the globals; `Bistooltip.lua:206-212` searches all three lists. **The
addon-side multi-expansion work is done upstream and does not need building.**

Data verified against `data/sql/base/db_world/item_template.sql`:

| File | Rows | Specs | Distinct item ids | Resolve |
|---|---|---|---|---|
| `Bistooltip_classic_bislists.lua` | 1936 | 19 | 1190 | **1190/1190** |
| `Bistooltip_tbc_bislists.lua` | 2406 | 27 | 1935 | **1935/1935** |
| `Bistooltip_wotlk_bislists.lua` | 3283 (2822 unique keys) | 32 | 2836 | **2836/2836** |

Phases: Vanilla `PR, P1..P6`; TBC `PR, T4, T5, T6, ZA, SWP`; WotLK `PR, T7, T8, T9, T10, RS`.
Vanilla/TBC have no `Ranged` slot (ranged weapons sit under `Weapon`).

The `.toc` no longer loads the fork's UI — `BislistUI.lua`, `Constants.lua`, `DataProvider.lua`,
`StateManager.lua`, `UIFramework.lua`, `EmblemData.lua`, `GemData.lua`, `ObjectPool.lua`, `Utils.lua`,
`ui/`, `util/` are orphaned on disk, as is the pre-replacement `Bistooltip_wowtbc_bislists.lua`.

### Decisions

| # | Question | Decision |
|---|---|---|
| 1 | Sequencing | Importer + module first; scraper second |
| 2 | IP tier -> (expansion, phase) | Ship the mapping below as a `static constexpr` table, not arithmetic |
| 3 | Cross-expansion matching | Bot's own expansion only |
| 4 | Vanilla `Fury-Prot` | New sentinel `tab 12`, gated on `tab == 1 && IsTank` |
| 5 | Discipline priest below WotLK | Accept the no-op; do not alias onto Holy |
| 6 | Roll votes | Nothing to change — the bonus already reaches them |
| 7 | Orphaned addon UI | Delete, plus the stale `Bistooltip_wowtbc_bislists.lua` |
| 8 | `Roll.UpgradesOnly` | Leave at `1`; GREED is the intended ceiling so the player keeps NEED |
| 9 | Loot method | Group Loot — voting path is live |
| 10 | Rollout gate | Always-on; no new config key |
| 11 | `PhaseDecay` | One global `1.0`; revisit per-expansion only with evidence |
| 12 | Tier tokens | Out of scope; recorded as a known gap |
| 13 | Regeneration source | Scrape wowtbc.gg |
| 14 | Custom realm ids | Dropped (already gone with the upstream replacement) |
| 15 | Phase representation | `expansion` column; per-expansion phase 0..N |
| 16 | `enhs` gems/enchants | Addon display only; module keeps score-based gemming |

### Outcome

Bots below IP tier 13 go from **no BiS signal at all** to scored and gate-bypassed, moving them from
PASS to GREED on their listed items and making them equip listed upgrades. Plus a re-runnable
generator so "stale" stops being a recurring question.

---

## Part 1 — Module: expansion-aware BiS (first)

The data is on disk and fully resolvable, so this lands without waiting on the scraper.

### 1.1 Importer

Committed script in `modules/mod-playerbots/apps/bis/` (the repo's only existing tooling is
`apps/codestyle/codestyle-cpp.py`, so `apps/` is the established home; there is no `CMakeLists.txt`).

Input: the three `Bistooltip_*_bislists.lua`. Output: one dated migration rebuilding the table.

Carry over from the existing migration's header
(`data/sql/playerbots/updates/2026_08_15_00_playerbots_bis_ranked.sql:1-12`):

- **Faction twins** — `Bistooltip_horde_to_ali.lua` (639 pairs) holds one side of each Alliance/Horde
  drop pair. Emit **both ids at the same rank**; no faction column. A bot only sees its own faction's
  drop, so the extra rows are inert.
- **Dual-slot promotion** — `Finger` and `Trinket` appear once per spec/phase, so ranks 1 and 2 are the
  intended *pair* (confirmed against the site, where `finger 1` and `finger 2` are separate slots each
  with their own `bis` item). Emit rank 2 as rank 1 for `Finger` / `Trinket`, and for Fury warrior
  `Weapon` **and** `Off hand` — the addon keys on `slot_name`, so no InventoryType lookup is needed.
- **`bis_rank` stays out of the primary key** — twins and Mage Fire/Fire FFB both put two items at the
  same rank in one slot.
- Skip `-1` padding and any id absent from `item_template`.

New, specific to the upstream files:

- **Last assignment wins.** The WotLK file assigns 461 keys twice, 367 conflicting — all of them in
  phase `PR`, because the file carries two complete pre-raid blocks (~line 72 and ~line 2130). Lua is
  last-wins and the shipped migration already used the second block. Verified low-stakes: **both
  blocks give the same rank-1 pick** (111/114 agreement with live each); they differ only in ranks
  2-6. Match Lua semantics and move on. Classic and TBC have zero duplicates.
- **Per-expansion spec -> (class, tab) tables.** The spec sets differ:

  | | Vanilla (19) | TBC (27) | WotLK (32) |
  |---|---|---|---|
  | Warrior | Fury, **Fury-Prot** | Arms, Fury, Protection | Arms, Fury, Protection |
  | Priest | Holy, Shadow | Holy, Shadow | Discipline, Holy, Shadow |
  | Rogue | Combat | Assassination, Combat, **Subtlety** | Assassination, Combat |
  | Hunter | Marksmanship | BM, MM, Survival | BM, MM, Survival |
  | Mage | Arcane, Fire | Arcane, Fire, Frost | Arcane, Fire (+Fire FFB), Frost |
  | Paladin | Holy, Retribution | Holy, Protection, Retribution | Holy, Protection, Retribution |
  | Warlock | Affliction, Destruction | Affliction, Demonology, Destruction | all three |
  | Druid | Balance, Feral dps, **Feral tank**, Resto | same | same |
  | Death knight | — | — | Blood dps, **Blood tank**, Frost, Unholy |

  Keep the sentinels `tab 10 = Druid feral tank`, `tab 11 = DK blood tank`, and add
  **`tab 12 = Warrior fury-prot`** for the Vanilla-only fury-specced tank, resolved on
  `tab == 1 && IsTank`. Folding it into tab 2 hands a tanking list to a bot the rest of the code
  treats as prot; folding it into tab 1 hands tank gear to a DPS warrior. `MakeKey`
  (`src/Mgr/Item/BisListMgr.h:89`) packs tab into 8 bits, so 12 is safe.

  TBC Subtlety maps to rogue tab 2, which WotLK cannot fill. Vanilla and TBC have no Discipline priest
  list, so a disc priest below WotLK resolves to tab 0, finds no rows, and the feature no-ops —
  accepted, same as Rogue Subtlety today (`src/Mgr/Item/BisListMgr.cpp:152`).

### 1.2 Schema

`playerbots_bis_ranked` gains `expansion TINYINT UNSIGNED NOT NULL` (0 = Vanilla, 1 = TBC,
2 = WotLK) as the leading primary-key column:

```sql
PRIMARY KEY (`expansion`, `class`, `tab`, `phase`, `slot_name`, `item_id`)
```

WotLK rows keep their existing phase numbers (0=PR, 1=T7 … 5=RS) with `expansion = 2`, so that half of
the data is unchanged. Vanilla uses phases 0..6, TBC 0..5. Generated wholesale, so this is a new dated
file that drops and recreates — no ALTER.

Migrations apply via `data/sql/playerbots/base/updates_include.sql:26-29` against `acore_playerbots`;
naming is `YYYY_MM_DD_NN_description.sql` and filenames must be globally unique.

`playerbots_bis_gear` (the `/p autogear bis` table) is untouched — keyed by ilvl, already spans
Vanilla and TBC ilvls, answers a different question.

### 1.3 `src/Mgr/Item/BisListMgr.h` / `.cpp`

- Add `enum BisExpansion : uint8 { BIS_EXP_VANILLA = 0, BIS_EXP_TBC = 1, BIS_EXP_WOTLK = 2 }`.
- Keep the existing `BisPhase` enum for WotLK (`h:19-28`); add `kMaxPhase[3] = {6, 5, 5}` and replace
  the `BIS_PHASE_MAX` uses at `cpp:179` and `src/Ai/Base/Actions/TellLosAction.cpp:172`.
- `RankedEntry` (`h:91-97`) gains `uint8 expansion`.
- Load query (`cpp:62-63`) becomes
  `SELECT item_id, expansion, class, tab, phase, bis_rank FROM playerbots_bis_ranked`.
- Add `struct BisProgress { uint8 expansion; uint8 phase; }`.
- `MaxPhaseForBot` (`cpp:171-180`) becomes `ProgressForBot(Player*) -> BisProgress`, replacing the
  `tier < IP_TIER_WOTLK -> PRERAID` collapse that currently flattens all of Vanilla and TBC onto
  *WotLK* pre-raid. Express it as a **`static constexpr BisProgress kTierToProgress[19]`** indexed by
  `sProgressionMgr.GetBotProgressionTier`, so any single tier can be retuned without touching logic.
  IP's ladder is at `modules/mod-individual-progression/src/IndividualProgression.h:229-247`; the
  intended values follow "tier N unlocks the content named in its comment":

  | IP tier | Unlocks | Cap |
  |---|---|---|
  | 0..7 | vanilla pre-raid … Naxx40 | `{ VANILLA, min(tier, 6) }` -> PR, P1, P2, P3, P4, P5, P6, P6 |
  | 8..12 | Kara/Gruul/Mag … Sunwell | `{ TBC, (tier - 8) + 1 }` -> T4, T5, T6, ZA, SWP |
  | 13..18 | WotLK Naxx/EoE/OS … RS | `{ WOTLK, min(tier - 12, 5) }` -> T7, T8, T9, T10, RS, RS |

  The WotLK row reproduces today's `BIS_PHASE_T7 + (tier - IP_TIER_WOTLK)` exactly, so level-80
  behaviour is bit-identical. TBC's and WotLK's `PR` are never a cap — harmless, since the cap is
  inclusive-downward and PR rows always match.

- `GetBisRankFor` (`cpp:182-216`) takes a `BisProgress` and skips entries where
  `entry.expansion != max.expansion` **before** the `entry.phase > max.phase` test. Own-expansion-only
  is deliberate: admitting a lower expansion's tail would give a level-80 bot a gate bypass on
  ilvl-164 TBC epics, and the phase-decay arithmetic has no defined distance across a boundary.
- Because comparisons stay inside one expansion, the decay arithmetic (`bis_max_phase_ - entryPhase`)
  needs no cross-expansion ordinal. `outPhase` keeps its meaning, but its "not found" sentinel at
  `cpp:185, 195` should become an explicit `bool` return or an out-of-range value now that phase 0 is
  a valid low phase in three expansions.

`PhaseDecay` stays one global `1.0`. Vanilla's `P1..P6` sit closer together in ilvl than WotLK's
tiers, so the same rule punishes a smaller real gap — revisit only if Vanilla bots visibly cling to
old gear.

### 1.4 Call sites

- `src/Mgr/Item/StatsWeightCalculator.cpp:239-270` — the memoized `bis_max_phase_`
  (`StatsWeightCalculator.h:123-127`) becomes a `BisProgress`. Rank scale and decay unchanged.
- `src/Ai/Base/Value/ItemUsageValue.cpp:287` `IsBisForBot` / `IsBisListed` — signature follows
  through; no logic change. The three gate bypasses (`:612`, `:984`, `:1159`) are unchanged.
- `src/Ai/Base/Actions/TellLosAction.cpp:163-177` — the `calc` debug line must print expansion
  alongside `phase cap` and `listed at phase`, or Vanilla/TBC results are unreadable in-game.
- `src/Ai/Base/Actions/TrainerAction.cpp:382` reads `playerbots_bis_gear` — unaffected.

No change to the roll path. Verified safe for low-level bots: `AdjustUsageForCrossArmor` early-returns
unless `usage == ITEM_USAGE_BAD_EQUIP` (`:1132`), and `BAD_EQUIP` only comes from
`QueryItemUsageForEquip`, which already passed `BotCanUseItem` (`:1265`). A level-55 bot cannot get a
bypass on a level-60 Vanilla BiS piece.

### 1.5 Config and docs

- `conf/playerbots.conf.dist:472-495` — document that lists match within the bot's own expansion. Keep
  all three conf copies in sync (`modules/mod-playerbots/conf/playerbots.conf.dist`,
  `env/dist/etc/modules/playerbots.conf`, `env/dist/etc/modules/playerbots.conf.dist`).
- `docs/systems/loot.md` and `docs/systems/itemization.md` have zero mention of BiS — add a section
  covering the ranked lists, the expansion match, the generator, and the fact that under
  `Roll.UpgradesOnly` the BiS signal decides GREED vs PASS rather than NEED vs GREED.

---

## Part 2 — Generator (`apps/bis/`)

Same script as the importer with a fetch front-end. This is what makes "regenerate" possible; the
importer alone only re-imports whatever upstream last shipped.

### Sources

| Expansion | page-data URL | Phase keys | Addon labels |
|---|---|---|---|
| Vanilla | `https://wowtbc.gg/page-data/classic/bis-list/<slug>/page-data.json` | `pre-bis, p1..p6` | `PR, P1..P6` |
| TBC | `https://wowtbc.gg/page-data/bis-list/<slug>/page-data.json` | `pre-bis, t4, t5, t6, za, swp` | `PR, T4, T5, T6, ZA, SWP` |
| WotLK | `https://wowtbc.gg/page-data/wotlk/bis-list/<slug>/page-data.json` | `pre-bis, t7, t8, t9, t10, t10.5` | `PR, T7, T8, T9, T10, RS` |

TBC lives at the site root (the site is "wowtbc"); `/tbc/...` 404s. WotLK's last phase is `t10.5` on
the site but `RS` in the addon — keep the addon label.

Spec slug is `<spec>-<class>` in kebab case, matching addon spec names: `restoration-shaman`,
`feral-tank-druid`, `feral-dps-druid`, `beast-mastery-hunter`, `unholy-death-knight`. All resolve 200
except `subtlety-rogue` on WotLK, which 404s — the site has no WotLK Subtlety list either.

Cache every fetched JSON to disk (~350-820 KB each, ~90 requests) so re-runs are offline. Throttle.

### Ranking algorithm (verified against the shipped data)

The site marks exactly one `bis: true` per slot per phase, but the addon stores 6 ranks. The rule,
confirmed on Shaman/Restoration T8 Legs, Finger and Trinket:

1. Filter `bisList` to items whose `slot` matches and whose `phase` array contains the target label.
2. Keep them in **`bisList` array order** — that ordering is the site's own ranking.
3. Move the item whose `item[<phase>]["bis"]` is `true` to rank 1.
4. Truncate to 6, pad with `-1`.

Worked example (T8 Legs, resto shaman): phase-filtered array order is Tortured Earth, Weary Mystic,
Zodiac, **Worldbreaker Legguards (bis)**, Profound Darkness, Stoneweaver -> emitted as
`46202, 45544, 45845, 46049, 46034, 45274`, byte-identical to the shipped row.

For `finger`/`trinket` the site has `<slot> 1` and `<slot> 2`: rank 1 = the `bis` item of slot 1,
rank 2 = the `bis` item of slot 2, ranks 3-6 = remaining items in array order, deduplicated. Verified
on resto shaman T8 Finger (`Nebula Band`, `Starshine Circle`, then Conductive Seal / Fire Orchid
Signet / Pyrelight Circle / Starshine Signet).

### Name -> item id resolution

Resolve offline against `data/sql/base/db_world/item_template.sql` (`entry` is column 1, `name` is
column 5; names use both `\'` and `''` escaping, so a naive regex breaks on `Brilliant Autumn's Glow`).

Match key is **(name, ilvl)** — ilvl comes from the `gearData` array in the same payload. Name alone
is ambiguous in WotLK because 10/25 and normal/heroic variants share names.

1. Strip the site's ` (H)` suffix for the lookup; use `gearData.ilvl` to disambiguate.
2. Fall back to `rarity` + armour `type` when ilvl still ties.
3. **Fail loudly** — any unresolved name aborts with a report. Silent drops are how a spec quietly
   loses a slot.

Emit a resolution report (unresolved names, ambiguous picks, per-expansion counts) beside the output.

### Outputs

Drop-in replacements for the files upstream's `.toc` loads, keeping upstream's global names — WotLK's
are still legacy-named:

| File | Globals |
|---|---|
| `Bistooltip_classic_bislists.lua` | `Bistooltip_classic_bislists`, `Bistooltip_classic_classes`, `Bistooltip_classic_phases` |
| `Bistooltip_tbc_bislists.lua` | `Bistooltip_tbc_bislists`, `Bistooltip_tbc_classes`, `Bistooltip_tbc_phases` |
| `Bistooltip_wotlk_bislists.lua` | `Bistooltip_wotlk_bislists`, **`Bistooltip_wowtbc_classes`**, **`Bistooltip_wowtbc_phases`** |

Include `enhs` (gems/enchants) — the addon UI reads it; the module ignores it. Emit the SQL migration
from the same in-memory model so the addon files and the DB can never disagree.

The addon directory is **not a git repo**. Back it up before writing (a `Bistooltip.zip` sibling
already exists as precedent).

---

## Part 3 — Addon cleanup

Delete the orphaned fork UI — `BislistUI.lua`, `Constants.lua`, `DataProvider.lua`, `StateManager.lua`,
`UIFramework.lua`, `EmblemData.lua`, `GemData.lua`, `ObjectPool.lua`, `Utils.lua`, `ui/`, `util/` —
plus the stale `Bistooltip_wowtbc_bislists.lua`. None are in the `.toc`; carrying dead files makes the
next upstream pull ambiguous. Re-porting them onto the upstream base is a standalone addon project
with no bearing on the module, and the backup keeps that option open.

Out of scope regardless: `Loot_Sources.lua` has no Vanilla or TBC instance loot (AtlasLoot 5.04
extract, per its own TODO at `:3-4`), `GemData.lua` holds WotLK gems only, `EmblemData.lua` models
WotLK emblems only.

---

## Verification

1. **Importer round-trip.** Regenerate from the three local `.lua` files and confirm:
   `SELECT expansion, COUNT(*), COUNT(DISTINCT CONCAT(class,'-',tab)) FROM playerbots_bis_ranked GROUP BY expansion;`
   Expect 3 expansions, ~31 spec keys for WotLK (Rogue Subtlety absent), fewer for Vanilla/TBC per the
   spec table in 1.1. Every id must exist in `item_template` — verified today at **5961/5961**, so any
   skip is an importer bug.
2. **Duplicate handling.** Spot-check a conflicting WotLK `PR` key (e.g.
   `Death knight / Blood tank / PR / Head`) and confirm the imported ranks match the **last**
   assignment: `41387, 42549, 44902, 37633, 37135, 34400`.
3. **Migration applied.** `worldserver.conf.dist:299` ships `Updates.EnableDatabases = 7`, which does
   not include the playerbots bit (8) — confirm the new file actually ran.
4. **In-game, sub-WotLK.** `sBisListMgr->LoadAll()` runs once from `PlayerbotAIConfig::Initialize`
   (`PlayerbotAIConfig.cpp:819`) with **no reload path**, so restart worldserver. On a bot below IP
   tier 13, run `calc` (`TellLosAction.cpp:138`) against a TBC or Vanilla BiS item and confirm the
   right expansion, a non-zero rank, and a cap matching the table in 1.3.
5. **In-game, voting.** In a Group Loot raid, confirm a bot now **GREEDs** (not PASSes) a listed
   upgrade, and still never NEEDs — `Roll.UpgradesOnly = 1` must keep NEED reserved for you.
6. **Regression on the fixed case.** Elemena (resto shaman, IP tier 14) must still resolve
   `{ BIS_EXP_WOTLK, T8 }` and still equip `45544`.
7. **Generator self-check** (Part 2 only). Regenerate WotLK and diff against
   `Bistooltip_wotlk_bislists.lua`. Every difference must be explainable as a genuine site change or a
   `Darkmoon Card: Greatness` naming variant; anything else is a ranking or resolution bug.

Nothing here is compilable in this environment — the module cannot be built headless, so C++
verification is static review plus the in-game checks above.

---

## Out of scope / known gaps

- **Tier tokens bypass BiS entirely.** `TryTokenRollVote` runs *before* `CalculateBaseRollVote` and
  decides on raw ilvl (`IsTokenLikelyUpgrade` / `IsAnyTierSlotLikelyUpgrade`); under `UpgradesOnly`
  the token branch in `FinalizeRollVote` picks GREED vs PASS the same way. Tokens are how bots
  acquire tier sets and the lists hold the redeemed *pieces*, so a token redeeming a rank-1 piece gets
  no BiS credit. TBC adds T4/T5/T6 tokens to the same path. Fixing it needs a token ->
  redeemed-piece mapping that does not exist in the module (`TokenSlotFromName` only parses the
  token's name for a slot) — a data problem of its own size.
- `sPlayerbotAIConfig.itemSetBonusWeight` has code default `0.25f` while every shipped conf sets
  `0.15`. Unrelated, still worth aligning.
- The 367 conflicting WotLK `PR` rows are an upstream data bug worth reporting to Spyr581.
- Using `enhs` to drive bot gemming/enchanting — the module keeps
  `PlayerbotFactory::ApplyEnchantAndGemsNew` (`PlayerbotFactory.cpp:5247`) and its score mirror
  `StatsWeightCalculator::BestGemScore` (`:831-904`).
- Re-porting the fork's addon UI onto the upstream base.

---

## Implementation notes (what actually shipped)

- **`Roll.TokenILevelMargin` needed no change.** The conf comment already reads "Minimum item level
  delta"; the `0.1` default just means "any strict increase". An earlier draft of this plan called it
  a ratio — it is not, and the doc was already right.
- **Fury dual-slot promotion covers `Off hand` too**, matching the migration it replaces. Frost DK,
  Enhancement and Rogue also dual-wield but keep genuinely distinct main/off-hand lists (identical
  lists appear in only 3-4 of 6 phases even for Fury), so their rank 2 stays a runner-up. That
  asymmetry is deliberate, not an oversight.
- **`BisPhase` is retained but now unused in code** — it documents WotLK's phase numbering next to
  the Vanilla and TBC ladders, which have no named constants.
- **Generator drops rows silently only for unresolvable ids**, which is currently none: all 5961
  distinct ids across the three files exist in `item_template`. Unmapped spec names abort the run.
