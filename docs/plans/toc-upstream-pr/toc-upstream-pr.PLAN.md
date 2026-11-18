# Trial of the Crusader strategy → upstream mod-playerbots `test-staging`: orchestrated PR prep

## Context

The user has finished tuning the ToC raid strategy (`src/Ai/Raid/ToC/`, 123 files, ~43.6k lines,
branch `Custom`, strategy key `trialofthecrusader`, map 649) and wants it upstream. Upstream has no
ToC raid strategy at all (only the Trial of the *Champion* dungeon, `src/Ai/Dungeon/TOC/`), so this
is a new raid, not a rework. It follows the Ulduar upstream effort's pattern
(`docs/plans/ulduar-upstream-pr/ulduar-upstream-pr.PLAN.md`, drafts #2876 → #2985 → #2986 published
2026-10-24) and reuses its stack, branches and wave script.

Current stack (all drafts, mergeable, 0 reviews; last upstream merge `889e9dd6e` = today's
`upstream/test-staging` tip): `raid-encounter-framework` #2876 `6c40cd524` → `raid-shared-helpers`
#2985 `034ba2d88` → `ulduar-rework` #2986 `359e73c45`; `eoe-malygos-rework` #2877 on the framework.

Tested by the user on this build: **25N and 25H**. The fights play the same on 10N/10H (user).

## Status

The merge stage alone edits this table. Export base (the Custom commit `trial-of-the-crusader`
mirrors): none yet (set by `p3-toc`, moved by each `sync`).

| Wave | Lane | State | Commit / merge | Decisions for review | Carried over / gaps |
|---|---|---|---|---|---|
| 1 | c1-toc-base | merged; review fixed 3 minor + 1 cleanup | `87966aa89` + review `6bacd8137`, merge `572c83f56` | self-res guard stays in `GenericTriggers.cpp` (no ToC hook reaches the dead engine), PR drops it; pinned-cast memo per group per ms; staging park latch in `OnTick` (also moves on minimal ticks); shared raid headers keep plain `constexpr`; group walks left uncached listed in the brief | no GPLv2 headers (PR branch adds them); rebuild needed |
| 1 | c2-toc-anubarak | merged; review fixed 4 minor + 3 cleanup, 1 recorded | `a86f1e00e` + review `256bf2e2b`, merge `fe8e5314c` | `"tree of life"` kept: Block rules match `getName()`, which is `"tree of life"`, so the overview's `"tree form"` fix was wrong and p3/p4 must not apply it; `kitePatchesSpent` is logic (`KiteWorldFor` reads it), so p3 keeps it despite Wave 3's list; `NoteAnubarakGripCast` → `TrackAnubarakGripCast` | uncached group walks (brief) for p4 or a later pass; 28 pre-existing lines >120 cols; rebuild needed |
| 1 | c3-toc-fc-beasts | merged; review fixed 4 minor + 1 cleanup, 1 decision | `f7889c35a` + review `aba1d6557`, merge `6436a8746` | no behaviour change; Jormungars Nitro fires outside the trace check; `ResolveDpsPick` reads the snobold pick under a skull only while tracing; grip `Note*` → `Track*` (logic, p3 keeps); extra trace-only fields for p3 listed in the brief | 4 uncached group walks for p4; trace-justified comments left for p3 to reword; rebuild needed |
| 1 | p1-framework-cares | committed; review clean, 1 minor recorded | framework `2b39e938f`; merges eoe `b984c3968`, helpers `cca94c10b`, ulduar `fa6dc4af2`; heads verified, RaidObs grep empty | `CaresTable` comment rewrapped to 2 lines; plain `EncounterGate`, no live flag | commit type `perf` and 98-col body vs upstream's list and 72 cols (needs amend; squash-merge hides it); clang-format not run locally; nothing pushed |
| 2 | p2-helpers-toc | committed; review fixed 1 minor, 1 decision added, 3 cleanup recorded | helpers `4d1f74551..5fddb4be8` (8) + review `8fa7a9c68`; ulduar merges `23ac59b16`, `f0d4fe5d1`; heads verified, RaidObs grep empty | `ResolveAlive` lives in ToC, so it goes with p3 (overview table was wrong); `RaidResistAura` election ported whole (`lowerFree`), so #2986's Iron Assembly aura now matches Custom; HoP ranks 2/3 as `fix(Ai/Paladin)`, also changes #2986's Algalon hand check; `"aura mastery"` left out of `RaidCoverClassNodes`; uncached 2-arg `IsRaidAreaDamage` and `IsRaidOpenerHolding` not ported; header constants `inline constexpr` | `RaidResistAura` reads paladin strategies by name (pre-existing); lowerCamelCase public struct fields; 2 shared comments name ToC; `ReadOpener` per-ms grid sweep kept for parity; clang-format not run; nothing pushed |
| 2 | c4-toc-closeout | merged; review fixed 1 minor, 2 minor + 1 cleanup recorded | `6deeb263f` + review `c66c37a81`, merge `85c56df82` | `docs/plans/toc-rework/` deleted (git keeps it; ICC plans still point there); recipe skill RaidObs section added outside the repo; `RaidObsLifecycle.cpp:385` stale heroic Icehowl comment left for a C++ pass; `test_toc_naming.py` samples stale trigger names | dead ToC code p3 would export (`AvoidCreatureClusterAction`, `GetCreatureClusterCenter`, `GetNearestCreatureByEntry`, `ANUBARAK_PIT_CENTER`, `SPELL_MASSIVE_CRASH_*`) and duplicates (`TryClassTaunt`, segment-distance helpers, spell-mover tests); `docs/raids/README.md:88` stale; no rebuild |
| 3a | c5-toc-group-walks | pending | | | |
| 3b | p3-toc | pending | | | |
| 4 | p4-parity-review | pending | | | |
| sync | p5-sync | pending | | | |
| — | user's 25-man clear on Custom with wave 3a merged | pending | | | gates Publishing |
| — | publishing | pending | | | |

States: `pending`, `committed` (PR lanes), `merged` (cleanup lanes), `blocked` (branch and worktree
kept, reason in the lane brief), `skipped` (merge refused because the main tree had a lane file
dirty; branch kept).

## Decisions (user, 2026-11-17)

- **Draft** PR, like Ulduar: community testers try it; marking ready is outside this effort.
- **Cleanup lands on Custom first**; Custom stays the source of truth and a repeatable `sync`
  carries later Custom changes to the PR branches. RaidObs stays on Custom, never in a PR.
- **Workflow waves** like Ulduar; the user says "go" per wave. Every push, PR open or PR text edit
  waits for the user's explicit go.
- **Shared raid code extends #2985** (`raid-shared-helpers`); ToC stacks on #2985 beside Ulduar
  #2986, and `ulduar-rework` merges the grown helpers branch.
- **The perf pass's `CaresTable` goes into #2876** (Custom `8b38899f4` + `02e6952c4`), then merges
  down into #2877, #2985, #2986.
- **One ToC PR**, one commit per encounter after a raid-wide base commit.
- **Tuning is done on every encounter**, so all cleanup lanes start at once.

Decisions (user, grilling round, 2026-11-17):
- PR branches take upstream by **merge, never rebase**.
- Custom-only generic behaviour ToC leans on stays out of the PR and is named in its text as a
  candidate for separate class/engine PRs: `RaidHealerEngine`, `PlayerbotAI.cpp` heal-absorb
  full-health interrupt (`9e9e1aaa5`) and near-teleport idle (`e131b1990`), paladin "aura mastery"
  / "hand of sacrifice on party", rogue "tricks of the trade" action and value.
- The self-res guard leaves shared `GenericTriggers.cpp` for a ToC rule on Custom; if no ToC hook
  reaches a dead bot's engine, Custom keeps it and the PR drops it.
- The four raid root files that collide case-insensitively with the dungeon's `TOC*` files are
  renamed on Custom to `ToCRaidStrategy.{h,cpp}`, `ToCRaidActionContext.h`,
  `ToCRaidTriggerContext.h`.
- ICC work is paused: `c1` restyles the shared raid files on Custom.
- Cleanup may carry two small behaviour changes: cache `JaraxxusPinnedCastTrigger`'s group walk,
  move the Staging trigger's state write out of `IsActive`. (A third, `"tree of life"` →
  `"tree form"`, was dropped: rules match `getName()`, which is `"tree of life"`.)
- Big planner functions (`GetWormMovePlan` 408 lines, Anub'arak `ComputeDuties`, `PlanKite`, …) are
  not split; reviewers decide.
- Bots reading enemy spline destinations and threat lists stays, named openly in the PR text.
- Perf-pass numbers go in the PR text as context only; the pmon justification comes when the user
  marks the PR ready.
- Agents draft the per-boss notes from the boss docs for the user to edit; the disclaimer is the
  user's. The PR text never calls 10N/10H untested: the fights play the same there.
- Publishing waits for one 25-man clear by the user on Custom after wave 3a merges (it changes
  behaviour); wave 3b doesn't.
- (2026-11-18) Every group walk the wave 1 audits found uncached behind a trigger or multiplier gets
  cached on Custom in wave 3a (`c5-toc-group-walks`), before `p3` exports.
- A full capped build of the ToC branch runs before the push, on the user's go.
- Branch `trial-of-the-crusader`, title `feat(Raid/ToC): …`, GPLv2 headers on the PR branch only,
  `ResolveAlive` keeps `boost::container::small_vector`.
- `w5-anubarak-rest-rest` (pending in the ToC rework) is folded into `anubarak.md`'s Known gaps, and
  the ToC rework's `w6-closeout` is absorbed by this effort (its audits by the cleanup lanes, its
  docs pass and recipe-skill section by `c4-toc-closeout`), so `docs/plans/toc-rework/` is deleted at
  the end of wave 2.

## What ToC needs that the stack lacks (measured 2026-11-17)

No fork-only **core** API or mod-spell-tweaks spell: every core call exists upstream, core PRs
#28117 (boss states, Anub'arak guid slot), #28181, #28136 and DB update `2026_11_06_01` are on core
`test-staging`; the local slime-pool SQL is client-visual only. No ToC code calls Ulduar or ICC
code. No cheats (all casts via `botAI->CastSpell`; bots read enemy spline destinations and threat
values, which the PR text names).

| Need | ToC use | Goes to |
|---|---|---|
| `CaresTable` / `CaresSlot` (`RaidEncounter.cpp`, `RaidEncounterRules.h`) | every Definition | #2876 (`p1`) |
| `RaidDirectTargeting.{h,cpp}` (111 lines), `RaidCombatHelpers` (165, incl. `RaidActionCache`, `RaidSpellIdRef`, `ResolveAlive`), `RaidBurstWindow` (99), `RaidOpener` (413) | 21 / 19 / 6 / 5 includes; ICC uses them too | #2985 (`p2`) |
| `RaidDefensive` raid-cover API (`RaidCover`, `GetRaidCover`, `RaidCoverSpellId`, `IsRaidCoverChannel`, `FinishRaidCover`, `RaidHasDivineGuardian`, `RaidHitLeadIn`, `RaidCoverClassNodes`, `RaidPredictedHit::noChannel`), ~360 lines | Jaraxxus, Twins, FC Defence | #2985 |
| `RaidSplashCache` + `SpellSplashes(botAI, action, cache)`; `SummonSplashes` without Trap Launcher | 12 refs, 5 files | #2985 |
| `RaidResistAura`: `IsRaidResistPaladinHolding`, `frostWantsMastery` (no `noteKey`) | Anub'arak heroic frost aura | #2985 |
| `EncounterHelpers::SegmentCrossesCircle`; `PaladinHelper.h` `SPELL_HAND_OF_PROTECTION_RANK_2/3` | 2 / 4 refs | #2985 |
| Wiring: `RaidStrategyContext.h` creator, `BuildShared{Action,Trigger}Contexts.cpp` root contexts, `PlayerbotAI.cpp` `"trialofthecrusader"` in the instance list + `case 649` | — | ToC PR (`p3`) |

Already on the stack, nothing to add: `RaidAntiFear`, `RaidRedirectThreat`, `RaidInstanceState`,
`RaidTankDefensive` (only `noteKind` differs, dropped), `TryMoveTo`/`MoveOutcome`,
`Strategy::OnTick`, `BurstCooldowns`. ToC adds no config option and no `AddSC_`.

## Wave 1 — cleanup on Custom (lanes `c1`–`c3`) and the framework port (`p1`)

**Cleanup lanes**, each in its own files (Definition files go with their encounter's lane):

| Lane | Files | Size |
|---|---|---|
| `c1-toc-base` | ToC root files, umbrellas `ToCRaid{Actions,Triggers,Multipliers}.h`, `Definition/ToCDefinitions.h`, stems `Shared`, `Staging`, `Opener`, `ResistAura`, `Jaraxxus`, `TwinValkyr`; `Util/ToCData.*`, `Util/ToCEncounterGate.*`; the shared raid files of the table above (Custom copies); `GenericTriggers.cpp`; the rename's include sites (`RaidStrategyContext.h`, `BuildShared{Action,Trigger}Contexts.cpp`) | ~11k, lead may split |
| `c2-toc-anubarak` | stem `Anubarak`, `Util/ToCAnubarakKitePlanner.h`, `Util/ToCAnubarakPursuit.h` | ~16.4k, split |
| `c3-toc-fc-beasts` | stems `FactionChampions`, `FactionChampionsDefence`, `FactionChampionsBall`, `Gormok`, `Jormungars`, `Icehowl`, `NorthrendBeasts`; `Util/ToCFactionChampionsGripChain.h` | ~16k, split |

A stem means its `Action/`, `Trigger/`, `Multiplier/`, `Util/ToCHelpers_` and `Definition/` files.
Each lane also owns its encounters' boss docs in `docs/raids/trial-of-the-crusader/`; `c1` owns that
directory's `README.md`.

Every cleanup lane, behaviour unchanged (counts from the 2026-11-17 audit):
- Comments ≤2 lines (182 longer blocks); no history in comments (13 dated lines, 11 "user call"
  lines, ~30 trace/reader mentions; measurements move to the boss doc).
- `ai` → `botAI` (7); private members `_lowerCamelCase` (~50 of 82); namespace `const` →
  `inline constexpr` where possible; braces off single-statement bodies; suffix the two bare double
  literals; name the inline magic literals (e.g. `ToCActions_Shared.cpp:32`,
  `ToCHelpers_FactionChampionsDefence.cpp:2003`, the turn tables in `ToCHelpers_Anubarak.cpp`).
- Make the RaidObs strip mechanical: no real logic inside a trace condition
  (`ToCActions_Jormungars.cpp:479` runs `UseNitroBoosts(bot)` inside `... && RaidObs::Active()`);
  rename real-logic functions that look like trace calls (`NoteAnubarakGripCast`, `NoteGrip`);
  triggers shaped only to keep a trace value current (`ToCTriggers_Jaraxxus.cpp:14,57`) get a
  logic-only shape.
- Trigger cost (upstream's O(1) `IsActive` rule): cache or gate `JaraxxusPinnedCastTrigger`'s
  per-tick group walk (`ToCHelpers_Jaraxxus.cpp:986`); move the state write out of
  `ToCTriggers_Staging.cpp:16` `IsActive`; audit the ~52 group-walk sites in `Util/` and list
  uncached ones under Decisions for review.
- `w6-closeout` audits for the lane's files: every multiplier gated to its encounter and testing
  the action family first; no `GetFirstAliveUnitByEntry` (upstream removed it in #2926); no AI value,
  `prioritized targets` or per-instance state leaking across bosses; lust not spent twice.

`c1` also:
- Rename the four raid root files that collide case-insensitively with the dungeon's `TOC*` files
  (`ToCActionContext.h`, `ToCStrategy.h`, `ToCStrategy.cpp`, `ToCTriggerContext.h`) to raid-unique
  names (e.g. `ToCRaidStrategy.{h,cpp}`, `ToCRaidActionContext.h`, `ToCRaidTriggerContext.h`) and
  fix every include; upstream CI builds on Windows and macOS.
- Move the ToC early returns out of `SelfResurrectTrigger::IsActive`
  (`src/Ai/Base/Trigger/GenericTriggers.cpp:27-28, 302-306`: `FactionChampionsCorpseCrowded`,
  `CorpseInSlimePool`) into ToC, since upstream forbids shared code branching on an encounter.
  Verify the replacement reaches the dead-state engine before relying on it; if no ToC hook can,
  leave Custom as is and log it, and the PR drops the guard.
- Docs: update `docs/raids/trial-of-the-crusader/README.md` "Code layout" for the renames.

The two trigger-cost fixes are the cleanup's only intended behaviour changes. The
`anubarak kite cat hold` Block list keeps `"tree of life"`: `CastTreeFormAction` reports that name,
only its creator key is `"tree form"`.

**`p1-framework-cares`** (PR branches only): port `8b38899f4` + `02e6952c4` onto
`raid-encounter-framework`'s own `RaidEncounter.{h,cpp}`/`RaidEncounterRules.h` (Custom's file
carries RaidObs and ICC-only gate pieces: port only the cares table; the native test stays out, no
`tools/` upstream). Merge `upstream/test-staging` first if it moved. Then merge the framework into
`eoe-malygos-rework` and `raid-shared-helpers`, and `raid-shared-helpers` into `ulduar-rework`.

## Wave 2 — helpers port (`p2`) and ToC closeout docs (`c4`)

**`p2-helpers-toc`** (starts once `c1` merged and `p1` committed): port the #2985 rows above from
cleaned Custom onto `raid-shared-helpers`, RaidObs stripped, trace keys dropped
(`RaidDefensiveScope{mapId}` only, no `toc.cover`/`toc.personal`, no `noteKey`/`noteKind`), GPLv2
headers, only symbols ToC calls. Every string-called action/trigger name in the ported helpers must
resolve upstream (e.g. `RaidCoverClassNodes` and any "aura mastery" call: Ulduar's audit found that
one had no upstream creator). Keep `ResolveAlive`'s `boost::container::small_vector` (boost is a
core dependency) and name it in the #2985 text. Merge the result into `ulduar-rework`; Ulduar code
unchanged.

**`c4-toc-closeout`** (Custom, after `c1`–`c3` merged): `w6-closeout` scope 2: final pass over
`docs/raids/trial-of-the-crusader/`, generic lessons into `docs/engine/`, re-check RaidObs's ToC
trace close against core #28117, fold `w5-anubarak-rest-rest` into `anubarak.md` Known gaps, delete
`docs/plans/toc-rework/`. Its scope 3 (RaidObs section in the `raid-boss-strategy-recipe` skill, via
`compact-skill-creator`) goes too. Docs via `compact-docs-writer` (standing approval).

## Wave 3 — group-walk caches (`c5`), then the ToC PR branch (`p3-toc`)

**3a `c5-toc-group-walks`** (Custom, behaviour change): cache every group walk the wave 1 audits
listed as reached uncached from a trigger's `IsActive` or a multiplier, and delete the unused code
`c4` found; scope in its brief. Merges
into Custom before 3b starts, so the export base carries it.

**3b `p3-toc`**: branch `trial-of-the-crusader` from `raid-shared-helpers`, worktree
`build-tocpr-wt/trial-of-the-crusader`. Big lane: the lead splits by encounter into ≤3 implementers
on disjoint files plus one finisher. Copy cleaned Custom `src/Ai/Raid/ToC/` at the export base
(recorded in the status table) and apply the PR-branch-only adaptations:

- **Strip RaidObs** (240 refs in 19 files): `ObsValue<T>` (30) → `T`, `ObsGuidMap` (18) →
  `std::unordered_map`, fixing the 8 `.Raw()` / 13 `.Set()` sites; delete the ~29 trace-only
  functions (`Note*` wrappers, label helpers like `KitePathText`, `WormMoveReasonName`), trace-only
  fields (`peelNotes`, `kickNotes`, `wallNotes`, `ReadStand`'s `note` param, plus those the c2 and
  c3 briefs list; `kitePatchesSpent` is logic and stays),
  `ToCEncounterSlug`, the `NamePull` call, `LogHitsOn`, `if (RaidObs::Active())` blocks (~140 lines).
- `EncounterDefinition` without `traceName` (8 constructors); gates as the stack's plain function
  pointers (`&ToCGateOpen`, `&ToCRaidWideGateOpen`), not Custom's `{open, live}` struct.
- `RaidOpenerConfig` without `toc.opener`, `RaidResistAuraConfig` without `toc.aura`,
  `NextTankDefensive` without `noteKind` (5 calls).
- Block lists drop names with no upstream creator ("aura mastery", "hand of sacrifice on party",
  "tricks of the trade"); the three `CastTricksOfTheTradeAction` `dynamic_cast`s (FC, Jormungars,
  Twins multipliers) and the `"tricks of the trade target"` read go, or match upstream's own Tricks
  action by name if it has one.
- Wiring per the table above; GPLv2 header on every file.
- Commits `type(Scope/Subscope): Summary` ≤72: (1) raid-wide base: gate, data, `Shared`,
  `Staging`, `Opener`, `ResistAura` stems, root strategy and contexts, registration; (2–6) one per
  encounter (Northrend Beasts, Lord Jaraxxus, Faction Champions, Twin Val'kyr, Anub'arak). Every
  commit compiles where the umbrella headers allow; the tip must.

## Wave 4 — parity review (`p4-parity-review`, one fresh reviewer)

`git diff <export base>:src/Ai/Raid/ToC trial-of-the-crusader:src/Ai/Raid/ToC` contains only the
RaidObs strip and the listed adaptations; the same for the helper files against Custom and the
cares table against Custom. Also upstream's rules (`AGENTS.md`, `.agents/docs/*` on
`upstream/test-staging`): O(1) triggers, shared state guarded, no raw `Unit*` across ticks, no
cheats, relevance bands, comments ≤2 lines. Bugs fixed on Custom first unless upstream-only.

**`p5-sync`** (repeatable, on the user's word): port `<base>..Custom` under `src/Ai/Raid/ToC/` and
the helper files into the PR branches, parity-check the delta, record the new base.

## Branches, worktrees, orchestration

Worktree root `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-tocpr-wt/<name>` (gitignored by the
core's `/build*/`, outside `modules/`).

| Branch | Base | PR | Lands |
|---|---|---|---|
| `tocpr/<c lane>` | `Custom` | — | `--no-ff` into `Custom` |
| `raid-encounter-framework`, `eoe-malygos-rework`, `raid-shared-helpers`, `ulduar-rework` | already checked out in `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-uldpr-wt/<branch>`; work there, never a second worktree | #2876, #2877, #2985, #2986 | push on go |
| `trial-of-the-crusader` | `raid-shared-helpers` | new draft | push + open on go |

- Plan + status table: `docs/plans/toc-upstream-pr/toc-upstream-pr.PLAN.md`; one brief per lane
  next to it (`<lane>.PLAN.md`, written with `prepare-prompt`) with Task list, Decisions for review
  and Blocked sections so a lost run resumes from the worktree.
- Script `build-tocpr-wt/tocpr-wave.js`, adapted from `build-uldpr-wt/uldpr-wave.js`: one lead per
  lane (big lanes split into ≤3 implementers + finisher), 2 lanes at a time, one fresh reviewer per
  lane between its commit and the merge (never shown the lead's report; fixes bugs and minors on the
  lane branch), one merge agent per wave that merges `c*` lanes into Custom, re-runs checks on the
  merged tree, and for `p*` lanes only verifies the branch and updates the status table. Pass
  `done: {lane: report}` to skip finished lanes. Add `build-tocpr-wt` to the module's
  `.claude/settings.local.json` additional directories so `Workflow({scriptPath})` can read it.
- Brief authority line: lane branches, worktrees, commits, `--no-ff` merges into Custom, merges of
  `upstream/test-staging` or a stack parent into a PR branch. Never push, rebase, amend,
  `reset --hard`, force, stash or clean. Never start, stop or restart a Docker container. Stage
  only own files by explicit path (other sessions edit Custom). Never ask the user; log choices
  under Decisions for review. Commit messages go to a fresh file per commit under
  `build-tocpr-wt/_msg/` and are committed with `git commit -F` only after that write succeeded.
- PR branches have no `docs/`: a PR lane keeps its working copy of the brief (Task list, Decisions
  for review, Blocked) at `<worktree>/.agents/plans/toc-upstream-pr/<lane>.PLAN.md`, which
  upstream's `.gitignore` ignores.
- Checks: Custom lanes `~/.claude/scripts/pb-check <changed and new files> src/Ai/Raid/ToC` plus
  `python tools/pblint/pblint.py --spell-difficulty src/Ai/Raid/ToC`; PR branches
  `PB_REPO=<worktree> ~/.claude/scripts/pb-check <changed files>` (pblint and tests skip there).
  On `INCOMPLETE`, rerun with `PB_MAX_FANOUT=all`. Commit text via `use-conversational-language`,
  no AI attribution.
- After each wave this session reports merged commits, decisions for review and whether a rebuild
  is needed (cleanup lanes change no behaviour, but the user's next pull is the only in-game check),
  then waits for "go".

| Wave | Lanes (2 at a time) | Starts |
|---|---|---|
| 1 | `c1-toc-base`, `c2-toc-anubarak`, `c3-toc-fc-beasts`, `p1-framework-cares` | user's go |
| 2 | `p2-helpers-toc`, `c4-toc-closeout` | user's go, wave 1 merged |
| 3a | `c5-toc-group-walks` | user's go, wave 2 merged |
| 3b | `p3-toc` | right after 3a lands cleanly (same run), `p2` committed |
| 4 | `p4-parity-review` | user's go, `p3` committed |
| sync | `p5-sync` | user's word |

## Publishing (this session, each step on the user's explicit go)

0. Starts only after the user's 25-man clear on Custom with wave 3a merged; fixes from it land on
   Custom and travel with `p5-sync`.
1. cppcheck 2.13 (`cppcheck --force --inline-suppr --suppressions-list=./.suppress.cppcheck src/` in
   a throwaway `ubuntu:24.04` `docker run --rm`) on all five branches; empty report required.
   Then a full build of the ToC branch (merged into the module and core `test-staging`, CI flags)
   in a capped throwaway container (`-j 8`, as the Ulduar sync 3 build did; an uncapped build once
   crashed Docker and the live server): g++ and clang, 0 errors, 0 warnings. Drafts get no build CI
   since upstream #3010.
2. Push `raid-encounter-framework`, `eoe-malygos-rework`, `raid-shared-helpers`, `ulduar-rework`
   (fast-forwards); push `trial-of-the-crusader` and open it as a **draft** to `test-staging`.
3. Texts via `handover` + `use-conversational-language`, each shown before posting: #2876 body
   gains the cares table bullet, #2985 body the ToC helpers, new ToC body filling every template
   section. Include: draft for community testing; covers 10N/25N/10H/25H, cleared on 25N and 25H
   (never call 10-man untested: the fights play the same); per-tick cost argued from the code, with
   the Custom perf pass as context only (ToC share of bot CPU 22.7% → 18.0%, the cares table ~90%
   of it); no cheats, but bots read enemy spline destinations and threat values;
   core requirements (#28117, #28181, #28136, DB `2026_11_06_01`) and the hand-mirrored core data
   keys in `ToCEncounterGate.h`; the Custom-only generic dependencies listed under Decisions; file
   layout and the big planner files; "How to test" (10/25, normal/heroic, `trialofthecrusader`);
   per-boss notes drafted from the boss docs, stressing what humans must do (Faction Champions
   starts on the player's `pull`, Anub'arak side tanks are bots, the twins fight from the door).
   The user edits the notes and writes the disclaimer.
4. Title via the `pr-title` skill (`feat(Raid/ToC): …`). Nothing is marked ready.

## Verification

- Per lane: checks green before commit; the merge agent re-runs them on merged Custom.
- `p3` finisher: every touched TU on `trial-of-the-crusader` compiles via `PB_REPO`; every
  string-wired action/trigger name resolves in the stack's creator tables;
  `git grep -n "RaidObs\|ObsValue\|ObsGuid" trial-of-the-crusader -- src` and the same on
  `raid-shared-helpers` are empty; no header basename collides case-insensitively with another
  (`git ls-files src | xargs -n1 basename | tr A-Z a-z | sort | uniq -d` lists none from ToC).
- `p4` parity review, then cppcheck and the capped build (Publishing 1).
- In game: the user's 25-man clear on Custom after wave 3a confirms the cleanup and the caches kept
  behaviour.

## PR text notes

- (c1) The self-res guard (`FactionChampionsCorpseCrowded` and `CorpseInSlimePool` in
  `SelfResurrectTrigger::IsActive`) stays on Custom only. The PR drops it, because no ToC hook
  reaches a dead bot's engine.
- (c1) The raid root files are named `ToCRaidStrategy.{h,cpp}`, `ToCRaidActionContext.h` and
  `ToCRaidTriggerContext.h` so they don't collide case-insensitively with the dungeon's `TOC*` files.
- (c1) The Jaraxxus pinned-cast trigger shares one group walk per group per ms. The staging park
  latch moves in `Strategy::OnTick`, not in `IsActive`.
- (c2) p3 strip list for Anub'arak: keep `kitePatchesSpent` and its increment. Delete `state.grip`,
  `GripMode`, `gripWaits`, `ReadPhase`'s phase/patches/flying/side0/side1 ObsValues and their
  `patchCount`/`flyingCount` locals, `burrowerSide`, `TrackKiter`'s block that notes none on the last
  kiter, and `GetAnubarakEscortSpot`'s post/patchPost/standby out-params. Each `NoteDpsPick` call
  becomes its second argument.
- (c2) Once the notes go, these locals become set-but-unused and must be deleted: `verdict` in
  `AnubarakBurstHeld`, `AnubarakAuraMasteryDue` and `GetAnubarakScarabKite`; `why` in
  `GetAnubarakSideTankAoe`; `label` in `GetAnubarakSwarmHealPick`; `name` in `GetAnubarakGripDuty`;
  `nearSpike` in `GetAnubarakSpikeDodgeSpot`.
- (c3) Logic helpers renamed on Custom: `FactionChampionsGripChain::TrackGrip`,
  `FactionChampionsTrackChainGrip`, `FactionChampionsTrackWallGrip` (formerly `Note*`). p3 and p4
  must keep them when stripping RaidObs.
- (c3) For p3: after the strip, the Jormungars cure-walk boost becomes
  `if (IsWormCureReason(reason) && travel > WORM_CURE_NITRO_FROM) UseNitroBoosts(bot);`. The snobold
  read in `ResolveDpsPick`'s skull branch is trace-only and goes.
- (p1) #2876 gains the cares table: a definition's multipliers share one per-bot table of which
  actions each one cares about (one bit per multiplier, a new table every 64 multipliers), replacing
  a separate `Action*` map in every rule and gated multiplier.
- (p1) The native test from Custom (`tools/nativetest/raid_encounter_rules_test.cpp`) stays out
  because upstream has no `tools/`.
- (p2) For p3: `TOC_DEFENSIVE_SCOPE` becomes `{TRIAL_OF_THE_CRUSADER_MAP_ID}`, and `TOC_OPENER` and
  `TOC_RESIST_AURA` drop their `"toc.*"` key. Upstream has no `IsRaidOpenerHolding`, but ToC never
  calls it.
- (p2) #2985 now carries the ToC helpers. The two-argument `IsRaidAreaDamage` and the Trap Launcher
  name are left out, and `RaidCoverClassNodes` leaves out "aura mastery" because upstream has no
  such action.
- (p2) `ResolveAlive`'s `boost::container::small_vector` belongs in the ToC PR text: the function
  lives in ToC, not in the #2985 helpers.
- (p2) #2985 gains a generic paladin fix: `HasAnyPaladinHandFromCaster` counts Hand of Protection
  ranks 2/3. #2986's Iron Assembly resistance aura now uses Custom's election, where a holder hands
  over to a cheaper paladin mid-fight.
- (c4) Docs only, nothing changes in the PR branches. The dead and duplicated ToC code in c4's
  Known gaps would go into the PR unless c5 or p3 removes it.
