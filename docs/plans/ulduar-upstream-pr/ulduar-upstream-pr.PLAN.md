# Ulduar strategy → upstream mod-playerbots `test-staging`: orchestrated PR prep

## Context

The user wants to contribute the local Ulduar strategy (`src/Ai/Raid/Uld/`, 139 files, 54,090 lines,
branch `Custom`) to `mod-playerbots/mod-playerbots`, replacing upstream's old 11-file, 7.5k-line
Ulduar. This session orchestrates the effort and owns the plan; lanes run as Workflow waves like the
ToC rework. One behaviour change lands on Custom first and ships with the PR: the Vezax hard mode
damage hold (Step 0).

State 2026-10-17: the user merged `upstream/test-staging` (`68f1e79b5`) into Custom (`c4052eedf`,
docs `352940a36`), so Custom's merge base is upstream's tip and `git diff upstream/test-staging
Custom` shows only local work. Custom already carries #2854 (config members UpperCamelCase,
`Playerbots.` keys incl. `Playerbots.Ulduar<Boss>HardMode`, distance members) and #2846 (static
`PlayerbotAI` helpers called via the class). Custom kept `GetFirstAliveUnitByEntry` (upstream removed
it in #2926) and its `AppendTargetExclusionsFor(..., candidates)` wrapper over upstream's
`AppendTargetExclusions(set, type)`. #2876 draft on `b0cd0ea7b`, no feedback, mergeable. #2877
draft, no feedback, **conflicting**.

## Status

Export base (Custom commit `ulduar-rework` mirrors): `4d8c308a9` (set by `p3-ulduar`, moved by each `sync`).

| Wave | Lane | State | Commit / merge | Decisions for review | Carried over / gaps |
|---|---|---|---|---|---|
| 0 | step0-vezax-hold | committed | `52ff85161` | Block rule + spell multiplier + hold node (AttackStop, casts on him interrupted, pets passive with stances in a per-instance store); hold-target row takes him without a swing under the hold; probe `vezax.hold`, reader prints it with his health | Not tested in game. Gaps: totems already down keep shooting, DoTs already on him tick out |
| 1 | c1-cleanup-a | merged | `41b6daec3`, merge `2809f111a` | `GetUldUnitByEntry` in `Util/UldTargets.{h,cpp}` (82 calls, c2/c3 files too); global `MoveOutcome` in `MovementActions.h`, RaidObs keeps a static_asserted copy; 4 ToC files renamed for it (may meet refine-fc); Auriaya pools resolved by value; Yogg conf says bots ignore it; Razorscale literals → `RazorscaleBossHelper` constants | 37 lane files lack GPLv2 header (PR branch adds); Razorscale grounded ranged isUseful 14 yd vs MoveInside 18 yd stalls bots 14–18 yd out; `FindNearestHarpoon` unused. Not tested in game. Rebuild needed |
| 1 | p1-framework | committed | `raid-encounter-framework` `0f28ffd35` (upstream merge `d878102f6`) | Tick/OnTick, `CheckScope` + `EncounterTriggerPassId`, plain `BossStateGateLive`; Mocking Blow added to `Family::Taunt` (Custom has it); pet helpers copied verbatim from #2877; brief in the worktree's gitignored `.agents/plans/ulduar-upstream-pr/p1-framework.PLAN.md`; branch tracks `origin/raid-encounter-framework` | New pieces have no caller on #2876 alone, and `Engine::DoNextAction` now calls `OnTick` on every strategy each tick: both go in the #2876 body. No RaidObs (grep empty) |
| 2 | c2-cleanup-b | merged | `a59709241`, merge `f717a6a64` | 276 header constants `inline constexpr`; Thorim fall rescue → `NearTeleportTo` + same-map guard (pitfall recorded); stray literals named (values unchanged; Mimiron stand-in bearing may differ by 1 ulp); FL vehicle list `constexpr` array; Freya Ground Tremor gate compares `::Name`; no isUseful/Execute reuse (not mechanical) | 16 lane files lack GPLv2 header (PR branch adds); 6 pblint assist-index-default warnings kept (fix is behaviour); `FlameLeviathanEnterVehicleAction` keeps upstream's old comments/seat literals. Not tested in game. Rebuild needed |
| 2 | p2-helpers | committed | `raid-shared-helpers` `e82ec26c8` (7 commits on `0f28ffd35`; the 7th adds `RaidDifficulty.h`) | Only symbols Ulduar calls; trace-name params dropped (`noteKind`, `traceKind`); `MoveToImpl` folded into `TryMoveTo`; nature-resist hunter uses upstream's Viper entry (`LowMana / 2`); `CastTrapLauncherExplosiveAction` bare class, no creator; local-only names removed from burst/splash lists; brief in the worktree's gitignored `.agents/plans/ulduar-upstream-pr/p2-helpers.PLAN.md` | **p3/p5: remove the trace-name arg at every Ulduar `Next*Defensive`/`CallHumanTankDefensive` call; `UldActions_Mimiron.cpp` `NextTankDefensive(..., "mimiron.plasma")` still compiles (string → `physicalOnly = true`), check by hand.** RaidAntiFear trigger walks the group (kept as Custom); upstream's nature-resist trigger and hold multiplier can name different hunters (say so in the PR text); trap class has no creator upstream. No RaidObs (grep empty) |
| 2 | p1-eoe-sync | committed | `eoe-malygos-rework` merge `6a61cc6b3` (on `origin` `76fb62f3d`, push is a fast-forward) | 5 content conflicts resolved to the branch side (= Custom); old `EoEActions.{h,cpp}` removed; one `CommandPetAttack`/`StopPet` copy kept (framework's); #2854/#2846 renames in the merge commit so it compiles; Custom's `MalygosGate`/trace name not ported | `RaidDifficulty.h`/`IsRaid25Man` (Custom `f753f7a05`) not ported (behaviour fix): needs a home on the stack (p2/p3 for FL, Thorim, XT-002; later EoE sync). Merged branch not fully built. No RaidObs (grep empty) |
| 3 | c3-cleanup-c | merged | `72608b9ff`, merge `a1c90c57a` | 326 header constants `inline constexpr`; Yogg floor-fall rescue → `NearTeleportTo` + `return true` (no master, no map guard); stray literals named (values unchanged; ring bearings `step*(pi/8)`, bit-identical), four class taunt ids as `SPELL_*` in the Yogg enum; incident comments → present-tense reasons, bulk-star latch reason added to `algalon.md`; two long `SpreadTarget` lambdas → named locals | 8 lane files lack GPLv2 header (PR branch adds). Not tested in game. Rebuild needed |
| 4 | p3-ulduar | committed | `ulduar-rework` `f58435bf0` (17 commits on `raid-shared-helpers` `e82ec26c8`) | Commit 1 removes old Ulduar + registration (compiles alone), commit 2 adds shared files, then one commit per boss, last adds 7 `Playerbots.Ulduar<Boss>HardMode` options (Yogg's dropped, no caller) in conf COMBAT section; RaidObs and trace-only code gone (labels, trace ticks, Describe*/Note*), `ObsGuidSet` → `GuidUnorderedSet`; Thorim `TickHumanSquads` kept (feeds `SquadOf`); `ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS` → `..._SCAN_INTERVAL_MS`; Yogg `candidates` early-out → `BossStateGateLive`; block lists drop names with no upstream creator (incl. tricks of the trade); `AppendTargetExclusionsFor` → `AppendTargetExclusions`; 61 files got GPLv2 header; brief in the worktree's gitignored `.agents/plans/ulduar-upstream-pr/p3-ulduar.PLAN.md` | Commits 2–16 build only together (tip compiles); Custom `923e40eb6` (FL, Freya, Thorim switches dropped) and the 120-col wraps reached the branch via `p5-sync` `3459d70b3`; Thorim, Yogg encounter/triggers, `Playerbots.cpp` pass only `pb-syntax-check-core.sh`; some unread spell constants and out-params kept; 3 Yogg triggers keep label-style `reason`. Not built against upstream core, not tested in game. No RaidObs (grep empty) |
| sync | p5-sync | committed | `ulduar-rework` `3459d70b3` on `f58435bf0` (Custom `ea9937e0b..4d8c308a9`, 16 files) | Applied with `git apply --3way`, 4 conflicts by hand: conf + `PlayerbotAIConfig.cpp` deletions/intro applied to the branch's COMBAT block (now byte-identical to Custom's Ulduar conf block); FL HoldStation detour un-nested without the trace-only `branch` label; Thorim reset takes `sifJoined = false`, not `squadsNoted`. New Thorim latch stripped to `bool sifJoined{}`. Freya Iron Roots/Unstable Sun Beam/Ground Tremor, Thorim Sif Blizzard, FL urgent drive lose the hard-mode gate (O(1): cached values, behind boss gate; = Custom). No upstream merge (merge-base still `68f1e79b5`), no `raid-shared-helpers` commit (no helper file touched) | Intermediate `ulduar-rework` commits still build only together. 129 TUs syntax-checked: 118 pass, 11 only on core drift (pass `pb-syntax-check-core.sh`); codestyle passes; no action/trigger names added or removed. Not built against upstream core, not tested in game. No RaidObs (grep empty) |
| 5 | p4-parity-review | committed | `ulduar-rework` `1d71c052d` on `3459d70b3` (export base `4d8c308a9`, Custom src unchanged since) | No behaviour bugs: full Ulduar diff (97 files, 49 header-only) and helpers match Custom, conf/config/registration/creator names resolve; 5 porting leftovers fixed, comment/shape only (Mimiron FleeShockToAnchor comment restored, Yogg empty `if (comeBack)` folded + stale p1taunt comment dropped, Iron Assembly interrupt-election comment corrected, Mimiron "observability" → scan interval); p2's helper adaptations counted as listed, not bugs; Custom-shared upstream-rule hits reported only (~20 trigger `IsActive` bodies scan grid/walk group, behind boss gate); brief in the worktree's gitignored `.agents/plans/ulduar-upstream-pr/p4-parity-review.PLAN.md` | **PR text: don't claim "no cheats"; name Mimiron core `StoreNewItem` (`UldEncounter_Mimiron.cpp:1526`) and floor-fall `NearTeleportTo` (Auriaya, Kologarn, Thorim, Yogg).** `raid-shared-helpers`: restore the `BossResistanceMultipliers.cpp` veto invariant (must match `BossNatureResistanceTrigger`'s gate) then merge into `ulduar-rework`. Shared with Custom: 13 trailing-underscore members in `UldActions_FlameLeviathan.h`, 8 `navprobe` comments, one >2-line block in `UldEncounterGate.h`. Branch only: 6 unread constants. Intermediate commits build only together; not built against upstream core, not tested in game. No RaidObs (grep empty on both branches) |
| — | publishing | pending (each step on the user's go) | | | |

## Decisions (user, 2026-10-17)

- **Stack**: #2876 (framework, existing draft) → new shared-helpers PR → one Ulduar PR with all 14
  bosses. EoE #2877 stays on #2876 and is updated in this effort; `CommandPetAttack`/`StopPet` move
  from #2877 into #2876.
- **Everything stays draft.** No in-game test or `pmon` from the user; other contributors test the
  drafts. Marking ready is out of this effort. The user writes the disclaimer and per-boss notes.
- **No RaidObs** in any PR. Custom keeps it.
- **Custom is the source of truth.** Behaviour-neutral cleanup lands on Custom first; review fixes
  land on Custom first, except upstream-only adaptations. A repeatable `sync` wave carries later
  Custom changes into the PR branches. The user merges upstream into Custom on their own schedule;
  after each such merge the PR branches merge the same upstream commit.
- **Workflow waves like ToC**: lane worktrees/branches, commits and merges without stopping; the
  user says "go" per wave. Every push or PR open/edit waits for the user's go.
- Kept as is: the 139-file layout (explained in the PR text); the `Strategy::OnTick` engine hook
  (goes into #2876; rewrite the hooks as triggers only if reviewers object); `thread_local` per-tick
  memos (upstream rules allow them); big multi-job actions (left for reviewers).
- PR branches take upstream changes by **merge, never rebase**, so pushes are plain fast-forwards.
- Ulduar keeps an **Ulduar-local entry finder** for the 82 `GetFirstAliveUnitByEntry` calls
  (upstream removed that helper in #2926; its `"find target"` only scans the bot's threat list).
- Out of scope: the local generic changes Ulduar doesn't need to compile (insane-rez guard, potions
  on the move, self-res Insane check, BossAura rework, "raid healer engine"); the Insane-rez gap
  goes into the Yogg-Saron notes as a fact for the user.
- Open draft #2653 (karlverik, XT-002) is mentioned in our PR text only.

## Step 0 — Vezax hard mode damage hold (this session, Custom, before wave 1)

Problem: with `UlduarVezaxHardMode` on, a strong raid kills Vezax before the Saronite Animus forms.
Core (`boss_general_vezax.cpp:266-311`): vapors every 30 s; on the 6th (~180 s), if none was ever
killed, they swirl and ~8 s later the Animus (33524) is summoned and Vezax gets Saronite Barrier
(63364, −99% damage taken). Killing any vapor loses the hard mode. Vezax: 6.3M HP 10-man, 23M 25-man.

Rules: while `VezaxHardModePending` (`UldEncounter_Vezax.cpp:536-544`: config on, Vezax in combat,
no vapor killed, Animus not summoned) and not `VezaxAnimusPhaseActive` (Barrier aura, backstop if the
63145 hook misses): non-tanks stop damaging Vezax at ≤ **15%** health, tanks at ≤ **2%** (threat
doesn't decay, so they keep aggro). Named constants in `UldEncounter_Vezax.h`. Personal damage
cooldowns keep firing as today; the vapor-kill gap (AoE/cleave/pets) stays open.

Implementation, following the `raid-boss-strategy-recipe` skill:
- **Block new damage**: a target-keyed multiplier in `UldMultipliers_Vezax.cpp`, shaped like
  `AlgalonTargetGuardMultiplier` (`UldMultipliers_Algalon.cpp:83-135`, registered with
  `Family::Melee | Family::Spell | Family::PetAttack`): zero melee, pet attack, non-heal casts and
  DoTs on Vezax, AoE pickers near him, and damaging summons by name (Fire Elemental Totem, Army of the
  Dead, Mirror Image: they pass `IsVezaxDamageAction` as buff actions). Never block `Family::Attack`
  as a whole (bots would drop to the non-combat engine).
- **Stop what already runs**: a hold trigger + action that keeps Vezax as current target but calls
  `AttackStop`, interrupts channels and autorepeat aimed at him, and parks every `m_Controlled`
  minion passive, restoring the saved stance on release — copy `UldActions_YoggSaron.cpp:559-618`
  (Hush/Unhush) and `:995-1007` minus the target clear. Gate `VezaxHoldTargetAction`
  (`UldActions_Vezax.cpp:220-237`) so its `Attack()` doesn't restart white swings under the hold.
- **RaidObs note** (Custom only) when the hold starts and ends, with Vezax's health.
- **Docs/conf**: hold section in `docs/raids/ulduar/vezax.md`; rewrite the `UlduarVezaxHardMode` text
  in `conf/playerbots.conf.dist:373-377` (stale "Profound Darkness" wording) to describe the hold.
- Close per `CLAUDE.local.md` (syntax check, pblint, docs, commit). Wave 1 doesn't wait for an
  in-game check; if the user tests it later (10 + 25, hard mode on/off, Vezax's health at the Animus
  summon in the trace), fixes land on Custom and travel with `sync`.

## What Ulduar needs that upstream lacks

No fork-only **core** API: every `InstanceScript` call is stock, `UldScripts.h`/`UldEncounterGate.h`
ids match upstream core. All blockers are module-side:

| Need | Uld use | Goes to |
|---|---|---|
| `Tick`/`OnTick`: `EncounterBuilder::Tick`, `EncounterDefinition::OnTick` (`RaidEncounter.h:101,176`), `Strategy::OnTick` (`Strategy.h:87`), call in `Engine.cpp:211-212` | Algalon, FL, Thorim, Yogg planners/latches; Mimiron keeps only `ResetMimironFightState` (`UldEncounter_Mimiron.cpp:968`); Vezax's tick is tracing-only and goes | #2876 |
| `EncounterTriggerPassId` + `CheckScope` (without RaidObs) | Mimiron `:349`, Yogg `:339` per-pass caches | #2876 |
| `BossStateGateLive` as a plain `bool(PlayerbotAI*, uint32)` | 4 sites | #2876 |
| `CommandPetAttack`/`StopPet` | 8 + 3 calls | #2876 (dropped from #2877) |
| `RaidTankDefensive`, `BurstCooldowns`, `RaidAntiFear`, `RaidRedirectThreat`, `RaidSplash`, `RaidHumanTankCall` (RaidObs stripped), `BossResistanceMultipliers` + `GenericBuffUtils::FindBossByName`/`GetNatureResistanceHunter` | 3–11 files each | helpers PR |
| Local-only `EncounterHelpers`: hazard helpers (`FindNearestPositionClearOfHazards`, `HazardCircle`, `HazardSweepCache`, `ValidateStaticFloorPoint`, `ValidateFloorPoint`), `CastClassTaunt`, `ClassTauntReady`, `IsDownOrFeigning`, frontal-cone pair, Nitro Boosts trio + constants, `SetRtiTarget(ai, rti, unit)`, `GetDynamicObjectPositions` initializer-list overload | ~100 calls | helpers PR |
| `MovementAction::TryMoveTo` + `MoveOutcome` enum (in RaidObs on Custom) | 4 sites (Mimiron, XT-002) | enum moves out of RaidObs on Custom (`c1`); helpers PR |
| `CastTrapLauncherExplosiveAction` (`HunterActions.h:377`) | 1 (Freya) | helpers PR |

PR-branch-only adaptations (Ulduar lane): `git rm` old `UldActions.{h,cpp}`, `UldTriggers.{h,cpp}`,
`Util/UldBossHelper.{h,cpp}` (same basenames **and include guards** as the new files; 81 of their 116
classes redefined); strip RaidObs (~435 refs in 23 files, 55 `ObsValue`/`ObsGuidMap`/`ObsGuidSet`
fields → plain types, `UldBotScripts.cpp:98-100`); 14 definitions → `(ULD_BOSS_X, &BossStateGateOpen,
&DefineX)`; `AppendTargetExclusionsFor(..., candidates)` → upstream `AppendTargetExclusions(set, type)`
(map Custom's exclusion types onto upstream's; drop Yogg's `candidates` early-out at
`UldEncounter_YoggSaron.cpp:3240-3245`); port the hard-mode options as Custom names them
(`Playerbots.Ulduar<Boss>HardMode` in `PlayerbotAIConfig` + `conf/playerbots.conf.dist`; drop Yogg's
if `IsYoggSaronHardModeActive`, `UldHardMode.cpp:83`, has no caller); register `UldValueContext` in `BuildSharedValueContexts.cpp`
and `AddSC_UlduarBotScripts` in `Playerbots.cpp`; trim local-only action names from Block lists
("aura mastery", "beacon of light on tank", "sacred shield on tank", "hand of sacrifice on party",
"pet acid spit"). Docs don't ship (no `docs/` upstream).

## Branches and worktrees

Worktree root `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-uldpr-wt/<name>`.

| Branch | Base | PR | Lands |
|---|---|---|---|
| `uldpr/<cleanup lane>` | `Custom` | — | `--no-ff` into `Custom` |
| `raid-encounter-framework` | `origin/raid-encounter-framework` + `git merge upstream/test-staging` | #2876 | push on go |
| `eoe-malygos-rework` | `origin/eoe-malygos-rework` + merge of the updated framework branch | #2877 | push on go |
| `raid-shared-helpers` | `raid-encounter-framework` | new draft | push + open on go |
| `ulduar-rework` | `raid-shared-helpers` | new draft | push + open on go |

The export base (the Custom commit `ulduar-rework` was ported from) is recorded in the status table;
each `sync` run ports `<base>..Custom` for Ulduar and the helper files and records the new base.

## Waves

| Wave | Lanes (2 at a time) | Starts |
|---|---|---|
| 1 | `c1-cleanup-a` (shared files, Auriaya, Ignis, Kologarn, Razorscale, XT-002, Vezax, Iron Assembly, Hodir; ~16k lines) · `p1-framework` | user's go, after Step 0 |
| 2 | `c2-cleanup-b` (Flame Leviathan, Freya, Thorim, Mimiron; ~18k) · `p2-helpers` · `p1-eoe-sync` | user's go |
| 3 | `c3-cleanup-c` (Algalon, Yogg-Saron; ~20k) | only when the user says their Algalon/Yogg tuning is done |
| 4 | `p3-ulduar` | c1–c3 merged, p2 committed |
| 5 | `p4-parity-review` (one fresh reviewer) | p3 committed |
| sync | `p5-sync` (repeatable) | user's word |

**Cleanup lanes (`c*`)**, all in the lane's files: comments ≤2 lines, no history/incident narration
(measurements go to the boss doc); name stray literals; drop braces on single-statement bodies;
`ai` → `botAI`; namespace `const` → `inline constexpr`
where possible; `isUseful()` stores what it found and `Execute()` reuses it, only where mechanical;
`0.f` → `0.0f`; `ObjectGuid const&` loops; `TeleportTo` → `NearTeleportTo(...); return true;` for
the 8 same-map floor-fall rescues (a same-map `TeleportTo` calls `CombatStop`, so this is also a fix),
plus a `master->GetMap() == bot->GetMap()` guard at `UldActions_Thorim.cpp:358` and
`UldActions_Auriaya.cpp:68`. `c1` also: the Ulduar-local entry finder replacing all 82
`GetFirstAliveUnitByEntry` calls; `MoveOutcome` out of RaidObs; Auriaya's room-scan cache
(`UldEncounter_Auriaya.cpp:72`) keeps GUIDs instead of `Unit*`; the Yogg hard-mode conf text
(describes cheat-kills that no longer exist). RaidObs calls stay on Custom. No splitting of
multi-job actions.

**`p1-framework`**: merge `upstream/test-staging`; add the #2876 rows above with no RaidObs,
`traceName` or `EncounterGate::live`; keep the 3-arg constructor. **`p1-eoe-sync`**: merge the
updated framework branch into `eoe-malygos-rework`, resolve conflicts (`RaidStrategyContext.h`,
#2854 renames — Custom's merged EoE files show the resolved form), drop its own
`CommandPetAttack`/`StopPet`.

**`p2-helpers`**: port the helpers-PR rows from Custom (already on upstream's names and style),
RaidObs stripped, GPLv2 headers, upstream comment rule; no Ulduar code.

**`p3-ulduar`** (big: the lead splits it into ≤3 implementers by boss group with disjoint files + one
finisher): copy Custom's cleaned `src/Ai/Raid/Uld/` at the export base and apply the PR-branch-only
adaptations. Commits: (1) remove the old Ulduar, (2) shared Ulduar files + registration, (3) one per
boss, (4) config options — each compiling on its own where the umbrella headers allow.

**`p4-parity-review`**: `git diff <export base>:src/Ai/Raid/Uld ulduar-rework:src/Ai/Raid/Uld` must
contain only RaidObs removal and the listed adaptations; anything else is a bug. Same for the helper
files against Custom. Also checks upstream's review rules (`AGENTS.md`, `.agents/docs/*`): O(1)
triggers, no unguarded statics, no raw `Unit*` across ticks, no cheats, relevance bands.

**`p5-sync`**: port `<base>..Custom` changes under `src/Ai/Raid/Uld/` and the helper files into
`ulduar-rework` / `raid-shared-helpers` (RaidObs stripped, adapted), one commit per branch, then a
parity check of the delta; record the new base.

## Orchestration

- Plan + status table: `docs/plans/ulduar-upstream-pr/ulduar-upstream-pr.PLAN.md`; one brief per lane
  next to it (`<lane>.PLAN.md`, written with `prepare-prompt`), each with Task list, Decisions for
  review and Blocked sections so a lost run resumes from the worktree.
- Script `build-uldpr-wt/uldpr-wave.js`, adapted from `build-toc-wt/toc-rework-wave.js`: one lead per
  lane (splits only big lanes into ≤3 implementers + finisher), 2 lanes at a time, one merge agent per
  wave; the merge agent merges `c*` lanes into Custom, and for `p*` lanes only verifies the branch
  and updates the status table. `build-uldpr-wt` is added to the module's
  `.claude/settings.local.json` additional directories so `Workflow({scriptPath})` can read it.
- Brief authority line: lane branches, worktrees, commits, `--no-ff` merges into Custom, and merges of
  `upstream/test-staging` or a stack parent into a PR branch. Never push, rebase, amend, reset
  --hard, force, stash or clean. Never start, stop or restart a Docker container. Stage only own
  files by explicit path (other sessions edit Custom concurrently). Never ask the user; log choices
  under Decisions for review.
- Lane checks: `~/.claude/scripts/pb-syntax-check.sh` on changed `.cpp` + includers of changed `.h`
  (`PB_REPO=<worktree>` on PR branches; it compiles against the stale build image's core, so
  upstream `Playerbots.cpp` can't compile there — known); `python apps/codestyle/codestyle-cpp.py`;
  `tools/pblint/pblint.py` on Custom paths. Commit messages via `use-conversational-language`;
  PR-branch commits use `type(Scope/Subscope): Summary` ≤72.
- After each wave this session reports merged commits, decisions for review and whether a rebuild is
  needed, then waits for "go".

## Publishing (this session, each step on the user's explicit go)

1. Upstream's cppcheck (`cppcheck --force --inline-suppr --suppressions-list=./.suppress.cppcheck
   src/`, cppcheck 2.13 in a throwaway `ubuntu:24.04` `docker run --rm`) on all four branches; empty
   report required (drafts skip this CI job, so local is the only check).
2. Push `raid-encounter-framework` and `eoe-malygos-rework` (fast-forward); push `raid-shared-helpers`
   and `ulduar-rework` to `origin` and open them as **draft** PRs to `test-staging`. Drafts run the
   build matrix (Ubuntu g++/clang++, Windows, macOS) against upstream core: the first full build of
   the stack. A macOS link error in `PlayerSearcher<AnyPlayerInObjectRangeCheck>` is upstream's, not
   ours.
3. PR texts via `handover` + `use-conversational-language`, shown before publishing: #2876 body update
   (new framework pieces), helpers PR body, Ulduar PR body filling every template section. The user
   adds the disclaimer and per-boss notes. Include: draft for community testing, no `pmon` measured
   yet; per-tick cost argued from the code (per-instance throttles, per-pass caches, boss-gated
   triggers); "How to Test" for contributors (10/25, `Playerbots.Ulduar<Boss>HardMode` options);
   no cheats left (old Ulduar had 21 `BotCheatMask::raid` sites); file-layout rationale; core drift
   (the fork core makes several Ulduar spells resistible — `56ee3ed2b`, `c1c9ff79b`, `3da20bce6`,
   `887e46714`; it took upstream's core in `62fb09c68`, so #27938, #27854, #27853 and #28079 are in);
   #2653 overlap.
4. PR titles via the `pr-title` skill. Nothing is marked ready in this effort.

## Verification

- Per lane: lane checks green before commit; the merge agent re-runs them on merged Custom.
- `p3-ulduar` finisher: every touched TU of `ulduar-rework` compiles via `PB_REPO`; every string-wired
  action/trigger name resolves in the creator tables; `git grep -n "RaidObs\|ObsValue\|ObsGuid"
  ulduar-rework -- src` and the same on `raid-shared-helpers` are empty; no old Ulduar basenames left.
- `p4-parity-review`, then cppcheck (Publishing 1), then the draft build matrix.
- No in-game test or `pmon` in this effort (user's decision); contributors test the drafts.

## First steps after approval (this session)

1. Step 0 (Vezax hold), committed on Custom.
2. Copy this plan to `docs/plans/ulduar-upstream-pr/ulduar-upstream-pr.PLAN.md` with the status
   table; write the lane briefs (`prepare-prompt`); commit on Custom.
3. Add `build-uldpr-wt` to additional directories; write `uldpr-wave.js`.
4. Save a project memory `ulduar-upstream-pr` (decisions, stack, script path) with its MEMORY.md line.
5. Report, then wait for the user's "go" for wave 1.
