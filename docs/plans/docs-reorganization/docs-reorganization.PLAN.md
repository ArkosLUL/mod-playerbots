# mod-playerbots documentation reorganization

## Context

`modules/mod-playerbots/docs/` holds **72 markdown files / 12,824 lines**, almost all of which are
implementation plans and findings for work that has already shipped (the git log maps
one-to-one onto the doc names). The tree has three problems:

1. **Transient bulk.** Every doc carries "Files to touch", pasteable code blocks, step-by-step
   task lists and a post-hoc `## Verification` section. Averaged across the tree that is roughly
   45% of the content, and none of it is useful now the work is merged.
2. **Severe duplication.** The engine's action-selection model is re-derived in ~9 files; "the
   module has no `CMakeLists.txt`, the core globs `src/**`" appears in 11; the threat-redirect
   rubric in 4; the burst-gate mechanism in 4; `ApplyEnchantAndGemsNew` in 5. Priest knowledge is
   split across 3 overlapping files, warrior/hunter/paladin across 2 each.
3. **Stale and contradictory content.** Several docs describe code that a later doc deleted, cite
   config defaults that do not match `playerbots.conf.dist`, or end with "Open questions" answered
   in the same file.

The durable value is real and worth keeping: engine semantics, a large catalogue of silent-failure
traps, verified encounter facts, and design decisions with their rationale (including explicit
"this is not a defect, do not re-audit" records).

**Outcome:** replace the 72 files with a ~25-file, ~2,700-line knowledge base organised by subject
rather than by task, holding only decisions, traps, verified facts and open gaps. In-flight plans
get their own directory and are deleted once distilled.

Nothing in `src/`, `README.md` or `AGENTS.md` references any path under `docs/`, so the tree can be
restructured freely. Every deleted file stays recoverable via git history.

## Target structure

```
docs/
  README.md                          index + how to maintain this tree
  engine/
    action-selection.md              how the bot picks an ability
    pitfalls.md                      the trap catalogue
  systems/
    itemization.md                   scoring, gems, enchants, sockets, set bonuses, professions, progression gating
    loot.md                          loot strategies + roll/vote decisions
    consumables-and-burst.md         burst windows, potions, lust, tinkers, ritual of souls
  classes/
    warrior.md  hunter.md  rogue.md  deathknight.md  paladin.md
    priest.md   shaman.md  mage.md   warlock.md      druid.md
  raids/
    README.md                        cross-raid conventions and rubrics
    naxxramas.md  ulduar.md  obsidian-sanctum.md  eye-of-eternity.md
    vault-of-archavon.md  trial-of-the-crusader.md  sunwell.md  tempest-keep.md
  plans/
    <slug>/<slug>.PLAN.md            in-flight work only; deleted once distilled
```

## Before starting

Invoke `/compact-docs-writer` and follow it for the whole authoring cycle. These docs are
version-controlled and re-read by agents every session, so token economy is a hard requirement, not
a cleanup pass.

## Content rules for every new doc

- **Keep:** verified facts (spell/item/NPC ids, timers, thresholds), design decisions **with their
  rationale**, traps and silent-failure modes, "confirmed correct — do not re-audit" records,
  known-open gaps, config names with their **shipped** defaults.
- **Drop:** file-by-file change lists, pasteable code, step ordering, `## Verification` sections,
  the "cannot build headless, hand off to the user" paragraph, and doc-shape meta ("write section 3
  as a table…").
- **State each shared fact exactly once**, in `engine/` or `systems/`, and reference it from the
  class/raid docs rather than restating it.
- Anchor facts to `path:line` where the original doc did — those citations are the main reason the
  knowledge is trustworthy.

## Work

### 1. `docs/engine/action-selection.md`

Single authoritative statement of the engine model, drawn mainly from
`classes/priest-all-specs-findings.md` (richest framework block), `raids/toc/ToC-strategy-findings.md`
§1 (primitive table + factory registration), and `general/burst-cooldown-windows-plan.md`.

Cover: the five primitives (Strategy / Trigger / Action / Multiplier / `Value<T>`);
`Engine::DoNextAction` pushing trigger `NextAction`s **plus** ungated `PushDefaultActions`, popping
descending, breaking on first `true`, ties by insertion order; the `ACTION_*` constants
(`Strategy.h:53-65`); `NamedObjectContext` registration and how class contexts resolve before
shared; `BuffTrigger` / `DebuffTrigger` / `beforeDuration` / `needLifeTime` semantics; multipliers
multiply so gates AND for free, and a 0 multiplier still pushes `/*A*/` alternatives; `CanCastSpell`
using `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST` so resource gating must live in a trigger; the
nested (non-exclusive) party health bands; `HealerAutoSaveManaMultiplier`'s exact veto rules and
why `estAmount`/efficiency metadata is a functional gate; no cast-while-moving model and no spell
queue; `CastTimeStrategy`'s ×0.1; the umbrella-header trick and the no-`CMakeLists` build fact.

### 2. `docs/engine/pitfalls.md`

The trap catalogue — the single highest-value artefact in the whole reorg. Group by failure shape,
one entry each, drawn from across the tree:

- **Silent name failures.** Unregistered trigger names are skipped with `continue` and no warning;
  unregistered actions log `A:<name> - UNKNOWN`; spell names resolve by string. Known casualties:
  `blade fury`, `"high threat"`, `conflagrate`, `chaos bolt`, `freezing trap on cc`,
  `cure party member`, and the live `"…shadow resistance triggerr"` typo. `TwoTriggers` creator key
  must equal `getName()`. Contrast: multiplier `dynamic_cast` mistakes fail at compile time.
- **Movement vs casting.** `CanCastSpell`/`CastSpell` refuse any non-zero cast time while
  `isMoving()`, and the `StopMoving()` calls are commented out — this explains a whole class of
  "healer never casts" bugs (`naxxramas/sapphiron-healer-heal-starvation-plan.md`). Reach-then-hold
  with hysteresis is the fix shape; do **not** lower priority or widen the multiplier whitelist.
- **Navmesh.** `MoveTo(generatePath=true)` never validates the target against the navmesh and
  returns `false` on `INVALID_HEIGHT`; `FleePosition` does validate via
  `BestPositionForRangedToFlee`. `MoveInside(distance = 0)` effectively never returns false.
  `ReachCombatTo` paths to the boss **origin** and bails on degraded path types.
- **Target visibility.** `"find target"` walks only the bot's own threat list and needs an exact
  full-name match on `creature_template.name` — use `GetFirstAliveUnitByEntry` otherwise.
  `AvoidAoeAction` only sees dynobject auras, damaging trap GOs, and `UNIT_FLAG_NOT_SELECTABLE`
  trigger NPCs; non-selectable stalkers never enter target lists, so scan `"nearest npcs"`.
- **Detection.** Mechanics wired to casts bots cannot observe (Heigan's GO-cast eruption, Noth's
  `triggered` Blink). Positional phase checks false-positive at the pull. `GetData` is a completion
  flag as often as a live signal (Vezax, Mimiron, Hodir, Thorim's never-cast `64324`) — prefer a
  boss aura, which also tells you *which*, not just how many.
- **State sharing.** Trigger/action/multiplier each hold separate helper instances; cross-object
  state needs a file-static GUID-keyed map. `GenericBossHelper<BossAiType>` is unusable when the AI
  class is file-local.
- **Assorted.** `DisperseDistanceValue` defaults to `-1.0f` (generic de-clumper inert);
  `ignoreDeadPlayers` default `false` silently deletes a role on death; `YieldThread` overwrites
  `SetNextCheckDelay(0)`; `BoostTrigger` is dead in raid PvE (`balance <= 50`); `isInFront`/
  `isInBack` default to `arc = M_PI` so both are always true; instance strategies are
  location-derived and must never be restored from persisted bot data
  (`general/Wrong-boss-strat-fix.md`); a derived `InitTriggers` cannot remove a base node —
  relocate it; never build an A→B→A `getAlternatives` cycle.

### 3. `docs/systems/`

- **`itemization.md`** — merges `ip-progression-item-gating/` (both files), `set-bonus-and-socket-scoring/`,
  `tank-gemming-and-unobtainable-enchants/`, `profession-gear-enhancements/`, and the meta-gem half
  of `loot/bot-loot-voting-and-meta-gem-fix-plan.md`. State `ApplyEnchantAndGemsNew` as the one
  place all enchanting/gemming happens (currently explained 5 times) and the
  `BestGemScore`-must-mirror-it invariant with its one deliberate exception. Keep the IP tier→patch
  table and the DB-verified gem/enchant classification tables verbatim — that is irreplaceable
  reference data. Keep: only cut gems have `GemProperties`; the (ItemLevel, Quality) fallback and
  why id ranges fail; detect IP via `GetQuestTemplate(66001)`, never `hasPassedProgression`; the
  set-bonus multiplier resetting to 1.0 exactly when the set completes; the `weight_` units trap;
  `CalculateEnchant` calling `Reset()`; `PRISMATIC_ENCHANTMENT_SLOT` vs `SOCK_ENCHANTMENT_SLOT`;
  the level-gate double-filter trap; `DEFENSE_OVERFLOW = 140` is correct; the druid exclusion.
- **`loot.md`** — from `loot/gear-only-loot-strategy-plan.md` plus the scoring half of
  `bot-loot-voting-and-meta-gem-fix-plan.md`. The two-engine model (score engine vs spec gate, bugs
  live in their disagreement), `IsLootAllowed` check order, the five strategies, `ll` commands, and
  the `Roll.UpgradesOnly` consequence.
- **`consumables-and-burst.md`** — merges `burst-on-boss-only/`, `dps-offensive-potions/`,
  `heroism-before-potion/`, `potion-expansion-gate-ammo-spam/`, `engineering-tinkers/`,
  `ritual-of-souls/`, and the shared half of `general/burst-cooldown-windows-plan.md`. Describe the
  burst gate once: `IsBurstCooldownAction` as the single registry, why name-matching beats
  `dynamic_cast`, the canonical cooldown list and its deliberate exclusions, the dwell constants,
  `holdState.Reset()` on every skip path. Then potions (id ladder table, off-GCD vs GCD ordering
  gotcha, expansion cutoffs and the `continue`-not-`break` fallthrough), tinkers (the
  `HandleUseItemOpcode` non-validation fact), and Ritual of Souls' strategy-override pattern.

### 4. `docs/classes/` — one file per class

Merge each class's overlapping docs into one, keeping the post-ship findings as the base (they
correct the plans) and folding in unique plan content. Consolidations:

- **priest.md** ← `priest-all-specs-findings.md` (base) + settled decisions from
  `priest-improvements-plan.md`; delete `priest-strategy-improvements-plan.md` (fully superseded
  work order, both deliverables exist).
- **warrior.md** ← `warrior-arms-fury-dps-findings.md` (base; it corrects the plan twice) +
  `fury-and-arms-rotation-improvements-plan.md`.
- **hunter.md** ← `hunter-dps-rotation-findings.md` (base) + `hunter-strategy-improvements-plan.md`.
- **paladin.md** ← `holy-paladin-healing-findings.md` (base) + `holy-paladin-improvements-plan.md`
  (~50% byte-identical).
- **rogue.md** ← `assa-combat-strategy-improvements-plan.md`; **deathknight.md** ←
  `frost-dk-rotation-improvements-plan.md`; **shaman.md** ← `resto-shaman-healing-improvements-findings.md`;
  **druid.md** ← `feral-cat-faerie-fire/`.
- **mage.md** / **warlock.md** — the audit bodies in `mage-fire-arcane-rotation-improvements-plan.md`
  and `warlock-rotation-audit-findings.md` are durable findings; move them here. The unexecuted
  *work order* framing moves to `docs/plans/` (§6).

Each file: current relevance ladder per spec, design decisions with rationale, open gaps, and the
"confirmed correct — do not re-audit" list. Move framework facts out to `engine/`. Preserve the
existing cross-doc precedent anchors (the paladin base-node relocation pattern, the shaman
`PartyMemberToProtect` revival) as links, or the "do not re-litigate" chain breaks.

### 5. `docs/raids/` — one file per raid, plus README

`raids/README.md` carries the cross-raid material currently repeated per boss: the threat-redirect
rubric and its over-vetoing corollary (stated 4×), the movement-suppression multiplier idiom (8×),
priority conventions (`ACTION_RAID` assignments, `ACTION_EMERGENCY + 6` with `MOVEMENT_FORCED`,
dispels ranking below raid priority), boss-helper patterns and the per-instance shared clock, and
the **raid-cheat dependency warning** (`BotCheats = "food,taxi,raid"` is on by default; the
CHEAT-ONLY inventory from `ulduar/ulduar-boss-strategy-gap-analysis.md` is a latent wipe list for
non-cheat servers).

Per-raid files use one section per boss: verified facts and ids, decisions with rationale, known
gaps. Largest merges:

- **naxxramas.md** ← 11 docs. Keep the deterministic-clock facts (Impale T+15s then 20s, tank not
  excluded, no range filter; zero-warning Locust Swarm; Heigan's RNG-free eruption schedule; Noth's
  fixed 110s/70s; Four Horsemen's every-tick >45yd punish), the per-boss redirect verdicts, and the
  commented-out-boss inventory.
- **ulduar.md** ← 17 docs. Resolve the three-layer hard-mode overlap: `ulduar-hard-mode-config-detection-plan.md`
  is authoritative (config is the single source of truth, detector holds the check, follower model),
  and it supersedes `ulduar-hard-mode-plan.md`'s Step-0 list and all of
  `ulduar-vezax-hardmode-config-plan.md`. **Fix while merging:** the Yogg-Saron doc still documents
  `YoggActiveKeeperMask` / `YoggThorimKeeperActive` / `GetBotInstanceScript` as live — that code was
  deleted. Drop the "Open questions" blocks that are answered later in their own files (Assembly,
  Leviathan/Thorim). Keep the Razorscale `UpdateBossAI()` side-effect warning, the Freya
  empower-events-outlive-the-Elder fact, Mimiron's never-a-target detection, Thorim's never-cast
  `64324`, and the Algalon zero-target evade caveat.
- **vault-of-archavon.md** ← 3 docs, including the ruled-out list (Archavon has no Choking Cloud).
- **eye-of-eternity.md**, **obsidian-sanctum.md**, **trial-of-the-crusader.md**,
  **tempest-keep.md** (the Void Reaver navmesh case study — cross-link to `engine/pitfalls.md`),
  **sunwell.md** (`sunwell-strategies-merge-plan.md` is 85% a one-off git runbook; keep only the
  `"rs"` / map-724 bug and the instance-strategy key list).

Fold `raids/misdirection-strategy-phase-one-raids.md` and
`raids/ulduar/ulduar-os-eoe-threat-redirect-findings.md` together — they are ~85% the same text —
into `raids/README.md` plus per-raid verdict tables.

### 6. `docs/plans/` — in-flight work only

Move the six not-yet-shipped items here as `docs/plans/<slug>/<slug>.PLAN.md`, trimmed of the
framework preamble now living in `engine/`:

- `shared-anti-fear-component-plan.md` (has an unresolved open item)
- `revive-noth-the-plaguebringer-strategy.md`
- `priest-improvements-plan.md` (executable spec; Shadow/Disc ladders still future)
- the three audit work orders: `rotation-audit-findings.md`, `warlock-rotation-audit-findings.md`,
  `mage-fire-arcane-rotation-improvements-plan.md` — their findings bodies go to the class docs,
  only the remaining to-do stays here.

**Verify first:** `xt-002-deconstructor-strategy-plan.md` reads as unshipped, but
`XT002BurstWindowMultiplier` / `XT002TargetGuardMultiplier` are cited as existing by two other
docs. Grep `src/` — if shipped, distil into `ulduar.md` and delete; otherwise move to `plans/`.

### 7. `docs/README.md`

Short index: what each directory holds, plus the maintenance rule that makes this stick — **a plan
lives in `docs/plans/` only while the work is in flight; when it ships, its durable content moves
into the matching `engine/` / `systems/` / `classes/` / `raids/` doc and the plan is deleted.**

### 8. Delete the old tree

Remove all 72 originals and the now-empty per-task directories. Git history is the archive.

## Stale facts to correct during the merge

Do not copy these forward as written:

| Source | Wrong | Correct |
|---|---|---|
| `set-bonus-and-socket-scoring` | `ItemSet.BonusWeight = 0.25`, `Socket.MaxMultiplier = 1.5` | **0.15** / **1.3** per `conf/playerbots.conf.dist:299,313` |
| `dps-offensive-potions` | potion dwell "~4s"; offensive-potion toggle "optional" | 5s per `heroism-before-potion`; shipped as `AiPlayerbot.OffensivePotions = 1` |
| `ulduar-yoggsaron-hardmode-findings` | `YoggActiveKeeperMask` / `GetBotInstanceScript` described as live | deleted by the config-detection refactor |
| `profession-gear-enhancements` | table says Hyperspeed "Applied and used" | its own item 2 says "DROPPED, then reopened" |
| `ulduar-hodir-hardmode-findings` | cites the Vezax `GetData(1)` gotcha in the wrong file | it is in `ulduar-hard-mode-plan.md` |

Also carry forward two live unfixed bugs so they are not lost with their host docs: the `errorDelay`
throttle integer-dividing 100ms to 0s, and the `"algalon observer"` vs `"algalon the observer"`
exact-match mismatch.

## Verification

1. **Coverage.** Before deleting, build a checklist of the durable nuggets (the surveys identified
   roughly 30 engine/pitfall facts, 23 itemization/consumable facts, and per-boss fact tables) and
   confirm each appears exactly once in the new tree. This is the only step that can lose
   information permanently — do it before `git rm`.
2. **Links.** `grep -rn "](.*\.md" docs/` → every relative link resolves.
3. **No external breakage.** Re-confirm nothing outside `docs/` references a doc path:
   `grep -rn "docs/" --include=*.md --include=*.h --include=*.cpp src README.md`.
4. **Config accuracy.** Cross-check every `AiPlayerbot.*` default in the new docs against
   `conf/playerbots.conf.dist`.
5. **Size.** Knowledge base should land near ~2,700 lines across ~25 files, down from 12,824 across
   72. If a merged file exceeds ~300 lines, it still holds transient content.
6. **Fresh-read test.** `docs/README.md` plus one raid file should be enough for a cold session to
   start work on that raid without opening anything else.

## Follow-up

The stored memory `raid-findings-doc-location.md` says raid findings go in `docs/raids/<raid>/`.
Update it after the reorg: durable raid knowledge → `docs/raids/<raid>.md`; in-flight plans →
`docs/plans/<slug>/`.
