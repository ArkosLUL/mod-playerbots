# BiS-aware loot voting and equipping

## Context

Bots misjudge items whose value is not in the raw stat block. Two live examples traced this session:

- **Grim Toll (40256)** — stat block is hit rating only; its armor-pen power lives in proc spell
  60436 → 60437 (`SPELL_AURA_MOD_RATING`, MiscValue `1 << CR_ARMOR_PENETRATION`). `BuildItemStatProfile`
  does not unwrap proc triggers, so `hasPhysical` is false and `hasCasterRatings` is true →
  `IsPrimaryForSpec` reads it as caster gear → a Fury warrior votes PASS. Traded directly, the bot
  equips it, because the equip path (`ItemUpgradeValue`) skips the spec gate entirely.
- **Illustration of the Dragon Soul (40432)** — `StatsCount = 0`; all power is in on-equip spell
  60485 → 60486 (+20 spell power / +20 healing done, 10 stacks). Stat profile comes back completely
  empty → `hasCaster` false → healers (`isCaster` true) fail the same gate → PASS.

Both are rank-1 BiS for the specs that passed on them.

Two independent fixes, because neither subsumes the other:

1. **Decode the procs.** Makes the general classifier correct for every proc-carrying item, listed or
   not — random-property epics, off-list trinkets, anything met outside a raid BiS context.
2. **Import an authoritative BiS signal** from the **Bistooltip** addon's ranked lists, as an override
   on top.

The BiS signal is **additive only** — an item absent from the lists is never penalized, because the
lists carry just 6 candidates per slot per phase.

## Part 1 — Decode proc-trigger stats

`src/Ai/Base/Value/ItemUsageValue.cpp`, `UpdateItemStatProfileFromSpells` (:756). Today it walks
ON_EQUIP/ON_USE item spells against a flat aura whitelist and stops there, so anything whose power
sits behind `SPELL_AURA_PROC_TRIGGER_SPELL` (aura 42) is invisible.

- Follow `SPELL_AURA_PROC_TRIGGER_SPELL` and `SPELL_AURA_PERIODIC_TRIGGER_SPELL` **one level** into
  `effectInfo.TriggerSpell`, guarding against self-reference. This mirrors what the score engine
  already does at `StatsCollector.cpp:748-752` — the two readers disagreeing is the actual defect.
- Widen the `SPELL_AURA_MOD_RATING` decode past spell ratings. The MiscValue is a `1 << CR_x` bitmask:
  - `CR_ARMOR_PENETRATION` → `hasARP` (+ `hasPhysical` intent)
  - `CR_HIT_MELEE|RANGED`, `CR_CRIT_MELEE|RANGED`, `CR_HASTE_MELEE|RANGED` → the matching rating flag
    **plus** `hasPhysical`
  - `CR_EXPERTISE` → `hasEXP`

Post-fix, Grim Toll reads physical (60437 is exactly `MOD_RATING` / `CR_ARMOR_PENETRATION` / 612) and
Illustration reads SP+heal (60486 is `MOD_DAMAGE_DONE` + `MOD_HEALING_DONE`), so both pass
`IsPrimaryForSpec` on their own merits without any list lookup.

## Part 2 — Source data

`A:\WOW\world of warcraft 3.3.5a hd\interface\addons\Bistooltip\Bistooltip_wowtbc_bislists.lua`

```lua
Bistooltip_wowtbc_bislists["Warrior"]["Fury"]["T7"][12] = {
    ["slot_name"] = "Trinket", ["enhs"] = { },
    [1] = 40256, [2] = 42987, [3] = 40531, [4] = 40684, [5] = 39257, [6] = 37166 }
```

- One row per (class, spec, phase, slot); the integer keys are a **rank list, 1 = best**, normally 6 deep.
- 2823 rows, 32 specs × 6 phases (`PR`, `T7`, `T8`, `T9`, `T10`, `RS`) × ~15 slots.
- Slots: Head, Neck, Shoulder, Back, Chest, Wrist, Hands, Waist, Legs, Feet, Finger, Trinket,
  Weapon, Off hand, Ranged, Relic.
- **Skip ids 130023, 130031, 131004, 150005** — custom ids from another realm, absent from
  `acore_world.item_template` (26 ranked entries).

### Transforms applied at generation

- **Faction twins.** `Bistooltip_horde_to_ali.lua` holds 639 pairs; 454 of the listed ids have a twin
  (e.g. `47617 Icehowl Cinch` ↔ `47855 Icehowl Binding` — same ToC drop, one id per faction). The
  lists carry only one side, and the addon swaps at display time. **Emit both ids at the same rank.**
  A bot only ever sees its own faction's drop, so the extra rows are inert — no faction column, no
  faction argument at lookup.
- **Dual-slot promotion.** Finger and Trinket appear once per spec/phase, so ranks 1 and 2 are the
  intended *pair*. Emit rank 2 as rank 1 for `INVTYPE_FINGER` / `INVTYPE_TRINKET`, and for Fury
  warrior weapons — mirroring the addon's own `NormalizeDualSlotRank` (`Bistooltip.lua:168`).
- **Mage "Fire FFB"** collides with Fire on tab 1 — merge in, keeping the better (lower) rank.

### Spec mapping

31 of 32 addon specs map onto (class, talent tab), reusing the existing `tab 10 = Druid Bear`
sentinel from `playerbots_bis_gear` and adding one:

| Class | tab 0 | tab 1 | tab 2 | sentinel |
|---|---|---|---|---|
| Warrior | Arms | Fury | Protection | |
| Paladin | Holy | Protection | Retribution | |
| Hunter | Beast mastery | Marksmanship | Survival | |
| Rogue | Assassination | Combat | *(no data)* | |
| Priest | Discipline | Holy | Shadow | |
| Death knight | Blood dps | Frost | Unholy | **11 = Blood tank** |
| Shaman | Elemental | Enhancement | Restoration | |
| Mage | Arcane | Fire (+ Fire FFB) | Frost | |
| Warlock | Affliction | Demonology | Destruction | |
| Druid | Balance | Feral dps | Restoration | **10 = Feral tank** |

Rogue Subtlety has no list; tab-2 rogues resolve to no key and the feature no-ops for them.

## Part 3 — Table `playerbots_bis_ranked`

New migration `data/sql/playerbots/updates/2026_08_15_00_playerbots_bis_ranked.sql`, shaped after the
existing `2026_04_28_00_playerbots_bis_gear.sql` (human-readable name columns kept so the table can be
read directly).

```sql
CREATE TABLE `playerbots_bis_ranked` (
    `class`      TINYINT UNSIGNED NOT NULL,
    `tab`        TINYINT UNSIGNED NOT NULL,   -- 0..2; 10 = Druid bear, 11 = DK blood tank
    `phase`      TINYINT UNSIGNED NOT NULL,   -- 0=PR 1=T7 2=T8 3=T9 4=T10 5=RS
    `slot_name`  VARCHAR(16) NOT NULL,
    `item_id`    INT UNSIGNED NOT NULL,
    `bis_rank`   TINYINT UNSIGNED NOT NULL,   -- 1 = best
    `class_name` VARCHAR(16) NOT NULL,
    `spec_name`  VARCHAR(32) NOT NULL,
    `phase_name` VARCHAR(8)  NOT NULL,
    `item_name`  VARCHAR(96) NOT NULL,
    PRIMARY KEY (`class`, `tab`, `phase`, `slot_name`, `item_id`),
    KEY `idx_item` (`item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

Two schema notes, both load-bearing:

- `rank` is reserved in MySQL 8 — the column is **`bis_rank`**.
- **`bis_rank` must not be in the primary key.** Both the Fire/Fire-FFB merge and the faction-twin
  duplication put two different items at the same rank in the same slot. Key on `item_id`; on
  collision keep the lower rank.

**Expected output: 18,426 rows, 31 distinct (class, tab) combos, 3,287 distinct item ids.**

Generation is a **one-shot** step — no converter kept in the repo. Parse rule, so it is reproducible:
match `bislists["<class>"]["<spec>"]["<phase>"][<n>] = { ... }`, strip the `["enhs"] = { ... },`
sub-table **first**, then read the remaining `[k] = <itemId>` pairs in order as ranks 1..N. Resolve
`item_name` from `acore_world.item_template`; drop any id with no row there.

`playerbots_bis_gear` is **left untouched**. The two tables do different jobs and cannot be unified —
the new data is WotLK-only, so regenerating the old one from it would take `/p bis` dark at its
vanilla/TBC ilvl tiers (66–164). Document the split: `playerbots_bis_gear` = "dress this bot",
`playerbots_bis_ranked` = "how much does this bot want this item".

## Part 4 — Runtime lookup: extend `BisListMgr`

`src/Mgr/Item/BisListMgr.{h,cpp}` already loads from `PlayerbotsDatabase` and is initialised at
`PlayerbotAIConfig.cpp:816`. Add alongside the existing `_bis` map:

- `LoadRanked()`, called from the same `LoadAll()`.
- Index keyed by **item id** — every caller asks "what is this one item worth to this bot":
  `std::unordered_map<uint32, std::vector<Entry>>`, `Entry { uint8 cls, tab, phase, rank; }`
  (~3.3k keys, ~18.4k entries; the per-key vector is short enough to scan linearly).
- `static bool ResolveSpecKey(Player* bot, uint8& cls, uint8& tab)` — `AiFactory::GetPlayerSpecTab`
  plus `PlayerbotAI::IsTank` for the two sentinels, with two hard refusals returning `false`:
  - **Role disagreement.** `GetSpecTraits` deliberately takes role from the *active strategy*, not the
    talent tab, so a prot-talented bot running a dps strategy would otherwise be handed Protection
    BiS and get nudged toward avoidance gear while fighting as dps. If `IsTank`/`IsHeal` contradicts
    the tab's implied role, resolve to no key.
  - **PvP spec.** `sRandomPlayerbotMgr.IsSpecPvp(...)` — the lists are PvE-only (no resilience gear
    anywhere in them), so a PvP-specced bot must not be steered by them.

  A wrong key is worse than no key: it doesn't merely fail to help, it bypasses safety gates in the
  wrong direction. Silent no-op is the correct failure mode throughout.
- `uint8 GetBisRank(Player* bot, ItemTemplate const* proto, uint8 maxPhase) const` — best (lowest)
  rank at or below `maxPhase`, 0 when absent.
- `uint8 MaxPhaseForBot(Player* bot)` — from `sProgressionMgr.GetBotProgressionTier`:
  `< IP_TIER_WOTLK (13)` → PR, 13 → T7, 14 → T8, 15 → T9, 16 → T10, `>= 17` → RS. The existing
  fallback already returns `ProgressionTierCap` (18) when mod-individual-progression is absent, which
  clamps to RS = every phase, so no separate "IP missing" branch is needed.
- `bool IsBisListed(Player*, ItemTemplate const*)` = `GetBisRank(..., PHASE_RS) > 0`, i.e. **any
  phase** — this is what the gates use.

`TrainerAction.cpp:377-385` currently open-codes the Druid-bear sentinel; point it at `ResolveSpecKey`
so the two cannot drift.

## Part 5 — Spec gates: BiS bypasses the heuristic

All in `src/Ai/Base/Value/ItemUsageValue.cpp`. Each site gets an early-out **before** the existing
stat reasoning, treating a list hit as proof the item is itemized for this spec:

| Function | Change |
|---|---|
| `IsPrimaryForSpec` (:844) | `if (sBisListMgr->IsBisListed(bot, proto)) return true;` |
| `IsFallbackNeedReasonableForSpec` (:594) | same early `return true` |
| `AdjustUsageForOffspec` jewelry gate (:976) | skip the `IsDesperateJewelryUpgradeForBot` requirement when listed |
| `AdjustUsageForCrossArmor` (:988) | skip the lower-tier-armor block when listed |
| `CalculateBaseRollVote` jewelry check (:2431) | skip the NEED→GREED downgrade when listed |

**`IsRoleItemizationMismatch` (:935) is deliberately NOT bypassed.** Per
[loot.md](modules/mod-playerbots/docs/systems/loot.md) it is the single choke point that fixed three
traced misclassification shapes. No item in the BiS data can trip it — that would take a Fury BiS
weapon carrying INT/SP, or a healer BiS piece with spell penetration — so bypassing it buys nothing
and only creates a hazard if the data ever disagrees. Record this as a deliberate non-change so it
isn't "fixed" later.

Nothing else in the roll chain needs touching. With the gates passing, a BiS item that really is an
upgrade reaches `FinalizeRollVote` as `NEED` + `ITEM_USAGE_EQUIP/REPLACE`, which `Roll.UpgradesOnly`
turns into **GREED** — the existing contract, unchanged. A BiS item the bot has already outgeared
still yields PASS under UpgradesOnly, which is correct.

## Part 6 — Score nudge

`src/Mgr/Item/StatsWeightCalculator`, gated behind a new **`SetBisBonus(bool)`** toggle following the
existing `SetItemSetBonus` / `SetOverflowPenalty` / `SetPvpSpec` pattern. **Default off**, switched on
only by `QueryItemUsageForEquip`.

`CalculateItem` is the single scoring entry point, so an always-on multiplier would also reach
`PlayerbotFactory::InitEquipment`, `RunAutogearFallback`, `/p bis` and
`RandomItemMgr::CalculateItemWeight` — changing what freshly-spawned random bots wear, which is well
outside this change. Turning it on for autogear later is a one-line follow-up once it has been
observed behaving.

Applied in `CalculateItem` immediately after the quality blend (:209-218), alongside the existing
set-bonus / socket / weapon-speed multipliers:

```
rank = sBisListMgr->GetBisRank(player_, proto, sBisListMgr->MaxPhaseForBot(player_));
// ranks 4-6 get gate-pass only, no score change
weight_ *= 1.0f + bisScoreBonus * kRankScale[rank];   // rank 1/2/3 -> 1.0 / 0.667 / 0.333
```

With the shipped `AiPlayerbot.Bis.ScoreBonus = 0.15`: rank 1 → ×1.15, rank 2 → ×1.10, rank 3 → ×1.05.

Calibration, against `EquipUpgradeThreshold = 1.1`: replacement needs `new × bonus > old × 1.1`, so a
rank-1 hit wins while up to **4.3% worse** on raw score, and a rank-3 hit needs to be ~4.5% better.
`weight_` is already scaled by `CalcMixedGearScore(ilvl, quality)`, so the nudge flips near-ties and
cannot beat a meaningfully higher-ilvl item. **Ranks 4–6 are excluded on purpose**: at a ~2.5%
multiplier they are invisible against the 1.1 threshold, and a sixth-choice alternate in a 6-deep
list is not "close to BiS". They still get the gate pass.

The multiplier applies to **both sides** of a slot comparison, so a BiS incumbent correctly resists
being swapped for a marginally better non-BiS piece.

Unlike the gate, the nudge is **phase-limited** (`MaxPhaseForBot`), so a pre-raid BiS item stops
nudging once the bot has progressed past it.

## Part 7 — Config, debug, docs

`src/PlayerbotAIConfig.{h,cpp}` + `conf/playerbots.conf.dist`, in the existing Roll/itemization block:

| Key | Default | Effect |
|---|---|---|
| `AiPlayerbot.Bis.GateBypass` | `1` | BiS hit bypasses the five spec gates (Part 5) |
| `AiPlayerbot.Bis.ScoreBonus` | `0.15` | Rank-1 multiplier bonus; `0` disables the nudge |

Both must no-op cleanly when the table is missing or empty, matching how `BisListMgr::LoadAll` already
logs and returns.

- Extend `TellCalculateItemAction` (`src/Ai/Base/Actions/TellLosAction.cpp`, chat command `calc`) to
  print resolved spec key, BiS rank and applied multiplier next to the score. Primary in-game handle.
- Update `docs/systems/loot.md` (BiS as a fourth answer to the score-engine-vs-spec-gate disagreement
  already documented there, plus the deliberate `IsRoleItemizationMismatch` non-bypass) and
  `docs/systems/itemization.md` (proc-trigger decode, the new multiplier, its phase gating and the
  `SetBisBonus` default-off scope).
- Both are durable version-controlled docs — run `/compact-docs-writer` before writing them.
- **Out of scope, noted as follow-up:** the `enhs` sub-table on every BiS row carries authoritative
  gems and enchants per slot. Wiring it in means either satisfying or breaking the documented
  `BestGemScore` ↔ `ApplyEnchantAndGemsNew` mirror invariant — separate change, separate risk.
- Move this plan to `docs/plans/bis-aware-loot/bis-aware-loot.PLAN.md` once approved.

## Verification

1. **Data sanity** — after applying the migration:
   ```sql
   SELECT COUNT(*) FROM playerbots_bis_ranked;                        -- 18426
   SELECT COUNT(DISTINCT class, tab) FROM playerbots_bis_ranked;      -- 31
   SELECT COUNT(DISTINCT item_id) FROM playerbots_bis_ranked;         -- 3287
   SELECT * FROM playerbots_bis_ranked
     WHERE item_id = 40256 AND class = 1 AND tab = 1;                 -- Grim Toll, Fury, phase 1, bis_rank 1
   SELECT r.item_id FROM playerbots_bis_ranked r
     LEFT JOIN acore_world.item_template i ON i.entry = r.item_id
     WHERE i.entry IS NULL;                                           -- empty (catches any faction twin
                                                                      --  missing from this world DB)
   SELECT class, tab, phase, slot_name, COUNT(*) FROM playerbots_bis_ranked
     GROUP BY 1,2,3,4 HAVING COUNT(*) > 8;                            -- empty; 6 ranks + merges/twins
   ```
2. **Build** — cannot be compiled in this environment; hand off to a real worldserver build.
3. **Part 1 in isolation** — with `AiPlayerbot.Bis.GateBypass = 0` and `Bis.ScoreBonus = 0`, the
   proc-decode alone should already make a Fury bot roll on Grim Toll and a Disc priest on
   Illustration. If it doesn't, the decode is wrong and no amount of BiS data is covering for it.
4. **Score, per bot** — `<fury warrior bot> calc [Grim Toll]` and
   `<disc priest bot> calc [Illustration of the Dragon Soul]` should report a nonzero rank and a
   multiplier > 1. A rogue bot should report rank 0 for Grim Toll unless assassination-listed.
5. **Spec-key refusals** — a prot-talented bot running a dps strategy, and a PvP-specced bot, must
   both report **no BiS key** from `calc`. This is the guard that keeps a wrong key from bypassing
   gates.
6. **Roll** — with `AC_AI_PLAYERBOT_ROLL_UPGRADES_ONLY=1` (current effective value; per CLAUDE.md
   confirm with `docker exec ac-worldserver env | grep ^AC_`), drop both trinkets in a group and
   confirm the previously-passing bots now roll **GREED**, and that a bot already wearing better
   trinkets still passes.
7. **Additive-only promise holds** — an item on no BiS list must vote exactly as it does today. Spot
   check one ordinary tier piece before and after.
8. **Scope containment** — spawn a fresh random bot and confirm its starting gear is unchanged, i.e.
   `SetBisBonus` really is off outside `QueryItemUsageForEquip`.
