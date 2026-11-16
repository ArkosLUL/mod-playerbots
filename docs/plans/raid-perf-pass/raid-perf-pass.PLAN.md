# Raid perf pass: ToC, shared raid code, ICC — zero behavior change

In flight since 2026-11-08. The status table is the source of truth for what ran.

## Status

| Step | Lane | State | Commits | Notes |
|---|---|---|---|---|
| 1 | baseline pull | done | | 2026-11-16 full ToC 25H clear, one pmon dump per fight in `build-toc-wt/perf/_pmon/run2/`; ToC triggers + multipliers 22.7% of raid bot think time |
| 2 | go/no-go | go | | user, 2026-11-16: re-aimed at the hot triggers (lane `t-triggers`), no behavior-changing W items |
| 3 | a-shared-fc (wave 1) | merged; review found no behavior diff, recorded 1 minor, fixed 1 harness cleanup | `f31dd9ac6` (lane `4ae762591`…`ecddf5558`) | Done: OBS, A0, A0-opener, N2, T7, R4, N1a, N1b, N1c, R3. Dropped: N1c-single-cast (one cast, a cache gains nothing), NB-TauntGuard (reads IsTank, lives in engine `src/Util`). Decisions: RaidSpellIdRef rechecks `getSpell()` each read (PullAction swaps spell); DispelGuard caches a `getSpell()` compare (safe while no cure class reassigns spell); NoteHazardCircle `Active()` race as every existing guard; R4 binds the table via static_cast in AddMultipliers; T7 uses upstream AC #22286's multi-entry grid search; wave 2 can reuse RaidActionCache/RaidSpellIdRef. Gaps: OBS/N2/A0-opener/N1c review-only; harness stubs model IsWithinDist, phase, grid visit, ClassifyAction; subjects of `e7b5c1203` (80) and `b91f8e930` (74) over 72 chars. Rebuild needed. |
| 3 | t-triggers (wave 1) | merged; review fixed 1 bug (T2 harness skipped invalid standing cells), 2 minor + 1 cleanup comments, recorded 1 minor | `aa42c6900` (lane `7cfb6d25e`…`aead9e58d`) | Done: T1, T2, T3, T4, T5, T8; none dropped. Decisions: T2 keeps a shared per-instance hint guid (`ToCGateState::champion`), always re-checked; T2 needs IsValidMapCoord; T2 tests container membership by a one-cell `map->Visit` (`GetCurrentCell()` is protected); T4 keeps last value wins for a repeated guid; ToCEntrySet bitset ~3.5 KB for the Defence list. Gaps: RefreshIcehowl comment wrongly says no guid slot (left, a slot search changes behavior); T2's proof holds only at the 200 yd ENTRY_SEARCH_RADIUS; T2 saves the sweep only while the boss or champion is within one cell. Rebuild needed. |
| 3 | b-anub-twins-jarax (wave 2) | merged; review found no behavior diff, fixed 1 minor (stale red counts in the brief), recorded 1 minor | `5414916ba` (lane `a114e58a7`…`897ad3ec5`) | Done: T3a, T3b, RA, T6, KS, TG-A, TG-T, R11, R6, NM, R6-J, N1-TI, N1-TR. Dropped: N1 one-cast guards (one cast, a lookup costs the same), N1 taunt guards (reads IsTank, lives in `src/Util`), R6 heal casts (single cast, a cache lookup costs as much), SideTankAreaGuard name copy (side tanks only, too little gain). Decisions: RA returns a by-value boost `small_vector` (upstream may want std-only); T6 assumes the member list is fixed between two back-to-back walks; R11 folds Feign Death into the held branch (same calls, same verdict); N1-TI moves `TwinInterruptKind` into the header at global scope; TG-A leaves NoteHands, the `++` note and literal notes ungated; SideTankAreaGuard argument order compiler-chosen as before; `Active()` race as every guard. Gaps: T6/KS/TG-A/TG-T review-only; T3 harness models the cell visit as a list; anub harness has no value refresh clocks and synthetic action types; RA commit body claims an unmeasured heap cost; per-commit compiles checked by reading; subjects of `3a6048f71` (77), `15c8376ea` (75), `6770c17cd` (77) over 72 chars. Rebuild needed. |
| 3 | c-icc (wave 2) | merged; review found no behavior diff, fixed 2 minor (N3 comment pointed at removed code, brief's NO null-check list), recorded 1 minor | `ef1dfd517` (lane `16ebe62fe`…`65b020a4f`) | Done: S1, N3, NO, NG-LK, NG-BPC, NG-SG, NG-SGA, NG-PP, NG-RF, NG-LM. Dropped: SG-tomb (KindOf already caches per action), NO-LKLate (every caller already inside an `Active()` guard), NG-VD/NG-LKL (rare notes, no measurable gain). Decisions: guarded notes keep the `Active()` race (wave 1 OBS); N3 keeps per-entry grouping so equivalence needs no unique guid; Svalna's wrapper now takes `char const*` only (`-Wunused-function`); guards also cover drakes and Marrowgar. Gaps: NO/NG review-only, S1 per-site proof textual; n3 harness models the grid visit as a list; no ICC baseline pmon; not attempted: splash multipliers' heal `dynamic_cast`, BPC DirectHit `"spell id"` lookup (RaidSpellIdRef); subject of `057b91e1c` (73) over 72 chars. Rebuild needed. |
| 5 | after pull | pending | | |
| 7 | closeout | pending | | |

States: `pending`, `merged`, `blocked` (branch and worktree kept, reason in the row), `dropped`.

## Context

ToC strategies are mostly done. The user wants a performance pass that makes raid bots cheaper on
server CPU **without changing any bot behavior**, run by an orchestrator that delegates the work.

- Scope (user's choice): ToC, shared raid helpers (`src/Ai/Raid/*.cpp/h`), RaidObs call sites
  (fork-owned `src/Bot/Obs` included), ICC. Changes land on Custom only. ToC goes upstream later as a
  PR stacked on the helpers PR (#2876 → #2985); maintainers there are strict about performance and
  run weaker machines, so per-check cost matters. ToC-only fixes ride that PR as they are; shared
  helpers keep their old signatures so the ToC PR effort can port or skip them (`RaidOpener.cpp` is
  not on the helpers branch and rides the ToC PR). No PR branch is touched here.
- The user holds ToC code edits while the pass runs: FC and Beasts files free again once wave 1
  merges, the rest after wave 2.
- Out of scope (user's choice): engine/core code (`src/Bot/Engine`, `src/Ai/Base`,
  `PlayerbotAI.cpp`; keeps upstream merges easy), Ulduar, older raids, the RaidInstanceState lock
  cache (R1: crash risk, no TSan on Docker Desktop), the optional N4 and `NoteDerived(char const*)`
  overload (low gain).
- Other ToC/ICC work (`w5-anubarak-rest-rest`, `refine-fc-13`, ICC `r14-closeout`) waits until this
  pass merges. `refine-anubarak-32` landed as `44b6dcaa0`; the tree was clean at planning time.

Why raid code is hot: per bot pass in ToC, ~127 interval-1 trigger nodes plus ~118 multipliers, and
each multiplier runs once per popped useful action; 28 are ungated and run on every action anywhere
in ToC. Expected gain is modest (a few µs per action per bot); widening the per-ms memos would gain
more but changes freshness, so the rule excludes it.

Baseline (2026-11-16, 25H, per fight): Beasts 17.3%, Jaraxxus 16.9%, FC 29.2%, Twins 20.9%, Anub'arak
23.0%. Multipliers 18.5 s spread at 0.3–1 µs each; triggers 12.7 s concentrated: FC personal
defensive 74.5 µs/check, anubarak kiter needs immunity 44.9, gormok tank duty 10–21 (in later
fights), twin valkyr pact interrupt duty 22.0, FC opener 16.6, FC should focus 15.9. Cause: a bot
pass takes 0.82–1.05 ms, so every per-instance per-ms memo (ReadGate, FC/Defence/Ball `Refresh`,
ReadPhase/Sweep, ReadTwins, ReadBeasts, ReadOpener) refreshes on nearly every pass, and the first
node in trigger order that reads one pays its whole refresh; those rows measure the refresh, not
their own logic. The fixes make the refresh bodies cheaper.

## The neutrality rule (binds every agent)

Same world-state sequence → every bot fires the same triggers, pushes the same actions in the same
order with the same relevance, executes the same action, causes the same side effects in the same
order, emits the same player-visible output, and writes byte-identical RaidObs traces when a trace
is open.

- **A call to a memo function is itself a side effect.** ReadGate, ReadPhase/Sweep, FC `Refresh`,
  `BossStateGateOpen`, every `_checkedMs`: the first call in a ms fixes what every later caller in
  that ms sees, and actions run between multiplier passes. `"spell id"` caches for 20 s, so skipping
  a `Get()` shifts its next recompute. Never add, remove or move such a call, not even by putting a
  cheap check in front of it. Only make calls cheaper.
- Allowed: the same result computed more cheaply at the same moment; caching per `Action*` facts
  fixed at construction (type, name, spell, threat type) inside one multiplier instance; caching the
  `Value<uint32>*` pointer (fetched lazily at the old first-call point), never the spell id;
  skipping trace-argument building when `RaidObs::Active()` is false, only where the arguments are
  pure; cheaper data structures with identical results and order.
- Forbidden: new or widened time-based memos, results shared across bots or moments, skipped
  side-effecting calls, changed multiplier names/order/count (veto credit, NoteBlock and pmon names
  key on them), changed push/tie order or check intervals, thread_local, a global `Action*`-keyed
  cache (addresses get reused after logout).
- Settled in `docs/engine/raid-mechanics-lessons.md`, do not re-audit: no memo on
  `"possible targets no los"`/`"nearest npcs"`, no `isPossible` before multipliers, leave
  `GetFirstAliveUnitByEntry` and `VisitCircle` alone.
- A candidate the lane can't prove in the harness (or, for mechanical ones, by review) is dropped
  and recorded with its reason, never shipped "probably".

## Lanes (disjoint files; ids from the planner's table; line refs predate `44b6dcaa0`, re-verify)

**Wave 1 — lane `a-shared-fc`** (shared helpers + ToC FC/beasts):
1. A0 API: `IsRaidAreaDamage(botAI, action, RaidSplashCache&)` and `SpellSplashes(…, RaidSplashCache&)`
   in `RaidSplash.cpp`/`RaidCombatHelpers.cpp`, caching per `Action*` the name-set lookups, cast/heal
   casts, threat type and the `"spell id"` Value* pointer; old signatures kept. Adopt in `RaidOpener`.
2. R4: one cares table per definition per bot plus a last-`Action*` shortcut in
   `RaidEncounter.cpp/.h`; the bitset builder goes std-only into `RaidEncounterRules.h` (ToC 88 → ~8
   hash lookups per action, ICC ~170 → ~20). Do not cache `BotHasRole`.
3. N2: `RaidBurstWindow` per-`Action*` name/class result.
4. `RaidObsEngine.cpp` NoteHazardCircle checks `Active()` before building its string.
5. N1/R3 (cache part only, no reorder): FC multipliers' `Splashes()` cache, Value* pointer in
   LateInterrupt and GripGuard; the ungated FC Defence, Gormok, Jormungars, Icehowl, NB multipliers'
   per-action casts and name copies.
6. T7: `ReadOpener` (`RaidOpener.cpp`) does one entry-set sweep instead of one per boss entry,
   bucketing hits per entry in config order and keeping visit order inside a bucket.

**Wave 1 — lane `t-triggers`** (memo refresh bodies in ToC `Util`; ids T1–T8, sites in the brief):
T1 gate sweep stops testing at the first hit; T2 gate tries a candidate creature first (guid slot or
last engaged champion) behind an exact container-cell test, falling back to T1; T3 an entry-set
creature list search helper in `ToCHelpers_Shared` (vector, O(1) entry test) adopted in ScanChampions,
FC Defence `UpdateHazards` and the Jaraxxus scan; T4 FC Defence `UpdateFocusClock` threat snapshot
as a flat reused vector; T5 Beasts `GatherRoster` built only when pass 2 needs it; T8 FC Ball
`TrinketsInReach` counts instead of building a vector.

**Wave 2 — branched from Custom after wave 1 merged, two lanes in parallel:**
- **`b-anub-twins-jarax`:** `ToCMultipliers_Anubarak.cpp/.h` (R6 via the splash cache, R11
  ProtectSpikeKite per-`Action*` classification, Value* pointer in SlowOrSpread and StopsCast);
  `ToCHelpers_Anubarak.cpp` `ResolveAlive` on a stack buffer keeping its snapshot (never loop the
  state vector in place: a nested ReadPhase→Sweep clears `state.burrowers`) and the pure trace gates;
  TwinValkyr and Jaraxxus multipliers (N1 part, `TwinValkyrInterruptHoldMultiplier` name copy);
  TwinValkyr trace gates (`GetWantedEssence`, `GetTwinDpsTarget`); T3 adoption in Anub'arak Sweep and
  Twins ScanArena; T6 `IsAnubarakTankPlayer` once per member per refresh for `StableMainTankGuid`
  and `RankSideTanks`, and a reused `CountSpentKitePatches` set. Not neutral, leave alone: the
  Anub note with `++` in its argument, `NoteHands`, Staging `NoteStage`, any memo on
  `GetAnubarakDpsTarget`, `ProtectorOrder`.
- **`c-icc`:** splash cache at the 15 `IsRaidAreaDamage` sites and Sindragosa's (cache members in
  the multiplier headers; leave `IccLichKingSplashMultiplier`'s own cache and `IccDbsAreaDamage`'s
  order as they are); `char const*` overloads for the 13 note wrappers and `Active()` guards on pure
  argument builds (LichKing `Tag()`, BloodPrinces, Rotface, Sindragosa, Putricide); N3
  `IccGetCreaturesByEntries` in one grid visit with the identical per-entry predicate and range.

**File ownership** (a lane edits only its own files plus its brief; a change needed elsewhere goes in
the report's `outsideFiles`/notes, never into the file):
- `a-shared-fc`: `src/Ai/Raid/*.cpp/h`, `src/Bot/Obs/RaidObsEngine.cpp`,
  `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_{FactionChampions,FactionChampionsDefence,Gormok,Jormungars,Icehowl,NorthrendBeasts}.*`,
  `tools/nativetest/raid_encounter_rules_test.cpp`.
- `t-triggers`: `src/Ai/Raid/ToC/Util/ToCEncounterGate.*` and
  `src/Ai/Raid/ToC/Util/ToCHelpers_{Shared,FactionChampions,FactionChampionsDefence,FactionChampionsBall,NorthrendBeasts,Jaraxxus}.*`.
- `b-anub-twins-jarax`: `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_{Anubarak,TwinValkyr,Jaraxxus}.*`,
  `src/Ai/Raid/ToC/Util/ToCHelpers_{Anubarak,TwinValkyr}.*`.
- `c-icc`: `src/Ai/Raid/ICC/**`.
- Merge stage: this plan's status table and anything a lane lists as needed outside its files.

## Orchestration

- Script `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/raid-perf-wave.js`, copied from
  `build-toc-wt/icc-rebuild-wave.js` and kept in its shape: one lead per lane (splits only a big
  lane into ≤3 implementers + finisher), one fresh reviewer per lane that never sees the lead's
  report, one merge agent per wave, two lanes at a time; wave 2 starts on its own only if wave 1
  merged with green checks, else everything stops and I report. Swap the ICC fields (guides,
  navprobe, cheats, difficulties) for `candidatesDone` (id, site, why neutral, proof, commit),
  `candidatesDropped` (id, reason), `harnessRuns`.
- Reviewer lens: per hunk, try to build an input where old and new differ (memo-call count/order,
  side effects, staleness, `Reset()`, map threads), check trace byte parity, re-run the harnesses.
- Git (user approved for this effort): worktrees `build-toc-wt/perf/<lane>`, branches `perf/<lane>`,
  **one commit per fix** inside a lane (bisectable, revertable alone), `--no-ff` merges into
  `Custom`, plan/status commits on `Custom`. Never push, amend, rebase, reset --hard, force, stash or
  clean. Messages via unique files in `build-toc-wt/perf/_msg/` with `-F`, worded per
  `use-conversational-language`, no AI attribution.
- Docker: never start, stop, restart or exec into an existing container. Throwaway `docker run --rm`
  from pb-check, `tools/nativetest/run.sh`, or a harness compile in `acore/ac-wotlk-build:master`
  (capped `--memory 4g --cpus 4`) is fine. Never build the server. Never ask the user.
- Agents: per wave two leads + two reviewers + merge = 10 (plus implementers only if a lead splits).
  Run: `Workflow({scriptPath: "G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/raid-perf-wave.js", args: {waves: ["1", "2"]}})`;
  `done: {lane: report}` skips lanes already committed.

## Steps

1. **Baseline (done):** a full ToC 25H clear recorded by
   `build-toc-wt/perf/_pmon/pmon_auto.py` (one dump per fight; see step 5). It ran on a build from
   before `6632778ad`, `4b9bdbce9`, `568566f6b` (FC, Twins, Beasts tuning); the user kept it anyway.
2. **Go/no-go (done):** go, re-aimed at the hot triggers.
3. Briefs `docs/plans/raid-perf-pass/<lane>.PLAN.md`; script `build-toc-wt/raid-perf-wave.js`;
   launch waves 1+2 chained.
4. Report merged commits, dropped candidates, review tallies.
5. **After (user):** rebuild, then a full ToC 25H clear while
   `python -I build-toc-wt/perf/_pmon/pmon_auto.py build-toc-wt/perf/_pmon/run3` runs (started by
   the orchestrator only on the user's explicit go; it drives pmon over SOAP, resetting at each pull
   and dumping at each fight end with pmon off, and stops after the Anub'arak kill). Compare run3
   with run2 per fight (`_pmon/run1/aggregate.py`, `share.py`) by per-check cost of the targeted
   triggers and multipliers and by share of `UpdateAIInternal`, never absolute times. FC's champion
   set is random per run: compare FC per check only.
6. If the new build seems to act differently: I locate the fix with read-only tools and propose
   reverting its commit; revert only on the user's word.
7. Closeout: fold durable lessons (memo calls are side effects; what is cacheable per `Action*`)
   into `docs/engine/raid-mechanics-lessons.md` "What a strategy costs per raid", and add one line
   each for the skipped candidates to its open cheap wins (engine: double-hash value lookups, the
   queue's name-copy compares, `LogAction` formatting; widened per-ms memos; R1; Ulduar and older
   raids; W1 one refresh per world tick via `GameTime::GetGameTimeMS()`, W2 gate by guid only, W3
   arena-centred ~95 yd sweeps, W4 lazy FC Defence `Update*`), via `compact-docs-writer`. Also fix
   that doc's `GetCreature` shortcut bound: it is position-based (`|dx|+|dy| < r`), but a sweep
   visits by container cell, so a provable shortcut tests `GetCurrentCell()` (cell moves reach the
   grid only through the map's move list). Delete the plan dir; commit.

## Verification

- Per lane: `~/.claude/scripts/pb-check <changed paths>` with `PB_REPO=<worktree>` (CI-strict
  compile, pblint, codestyle, nativetests); `RaidEncounter.h`/`RaidEncounterRules.h` fan out to 100+
  TUs, so rerun with `PB_MAX_FANOUT=all` in the background on `INCOMPLETE`.
- Equivalence harness (`raid-mechanics-lessons.md` "Proving a refactor behaviour-neutral") for every
  rewritten function: IsRaidAreaDamage, SpellSplashes, FC Splashes, SlowOrSpread, ProtectSpikeKite,
  ResolveAlive, N3. Old bodies from `git show Custom:<file>`, new from the worktree, counting stubs
  for `Action`, `SpellInfo`, `"spell id"` and memo calls; enumerate all predicate combinations,
  repeat cases to hit caches; compare results **and** the exact `Get()`/memo call sequence; red
  check: drop one short-circuit, the mismatch must show. Harness files in
  `build-toc-wt/perf/_harness/<lane>/`, never committed.
- R4: extend `tools/nativetest/raid_encounter_rules_test.cpp`: bit i == RuleCares(rule i) for every
  combination.
- T2: harness with the core's `CalculateCellArea`/`VisitCircle` cell math copied in as the oracle,
  enumerating bot and candidate positions and cells; red check: drop the cell test, mismatches must
  appear. T3: membership and order harness against `GetCreatureListWithEntryInGrid`'s check. T4: pure
  function harness (snapshot and `moved` count). T7: bucket-order harness. T1, T5, T6, T8: review.
- Every commit compiles: `pb-check` over the fix's changed paths before each commit, then over the
  lane's union before handing over.
- Trace gates, cast caching, const refs: compile + reviewer proof that every skipped argument is pure.
- Merge agent: pb-check over the union of merged files on the main tree.
- pmon shows the gain; the harnesses and reviews are the proof that behavior didn't change.

## Closeout notes

Engine lessons from the lanes, to fold into `docs/engine` at step 7.

- `CastSpellAction::spell` is not fixed after construction: `PullAction::InitPullAction` reassigns
  it in `isPossible` and `Execute`. A per-`Action*` cache keyed on `getSpell()` must recheck it on
  each read, as `RaidSpellIdRef` does.
- Per-`Action*` facts safe to cache inside one multiplier: dynamic type, `getName` (every override
  derives from members set in the constructor), `getThreatType` (every override returns a
  constant). Shared tools: `RaidActionCache<Facts>` and `RaidSpellIdRef` in `RaidCombatHelpers.h`.
- The core has a multi-entry `GetCreatureListWithEntryInGrid(list, entries, range)` (upstream AC
  #22286) with the same visit and `IsWithinDist` test as the single-entry one; taking its hits entry
  by entry in config order reproduces one sweep per entry. `Map::Visit` never loads a grid, so sweeps
  are pure.
- Values and actions in an `AiObjectContext` live until the `PlayerbotAI` is destroyed, after its
  engines, so a multiplier can hold a `Value*` or key on an `Action*` for its whole life.
- Equivalence harness pattern: cut definitions out of the base commit and the worktree by regex
  anchor plus brace matching into separate namespaces over global counting stubs, compare results and
  the call log per call, write red checks as text mutations that must match exactly once
  (`build-toc-wt/perf/_harness/a-shared-fc/extract.py`, `run.py`).
- `MovableMapObject::GetCurrentCell()` is protected, so module code can't read a creature's
  container cell, and a cell computed from position can lag the container until
  `Map::MoveAllCreaturesInMoveList` runs. The exact module-side test is a one-cell
  `map->Visit(Cell(coord), TypeContainerVisitor<searcher, GridTypeMapContainer>)` looking for the
  creature pointer; `Map::Visit`/`VisitCell` skip unloaded grids and create no cells.
- For a bot on `IsValidMapCoord`, `Cell::VisitObjects` at r = 200 always visits the 3x3 block around
  the bot's standing cell (proven over every float coordinate). Past the map edge the clamped
  standing cell breaks that, and the ±2 block is not always visited either (`VisitCircle` drops
  corners).
- The `GetCreature` shortcut fix in step 7: a position bound like `|dx|+|dy| < r` is not exact. The
  provable version combines the sweep's own tests, a ±1-cell bound and a container visit of that
  cell (not `GetCurrentCell()`, which is protected).
- A per-instance "last hit" guid can spare a boolean sweep without changing its answer, if it is
  accepted only after every test the sweep applies plus proof the sweep visits its container, and
  falls back to the unchanged sweep on any miss.
- `boost::container::small_vector` is available to module code (the core already pulls in
  Boost.Container). Returned by value it keeps snapshot semantics with no heap allocation for short
  creature lists, which matters where a nested memo refresh can clear the source list mid-loop.
- An equivalence harness can cover every dynamic type a guard can see: make each cast target
  inherit `Action` virtually and build `Combo<mask>` classes from conditional virtual bases, so a
  `dynamic_cast` cross-cast works for any subset of targets.
- A red check that maps one branch onto another passes silently when the two are equivalent (Feign
  Death and the held path in ProtectSpikeKite). Pick reds that change the verdict, and read such a
  pass as a hint to merge the branches.
- `NoteDerived` and every other ProbeTarget-based probe do nothing until `Active()`, so an
  `Active()` gate is neutral wherever the argument build is pure; the exception is an argument with
  a side effect, such as `++` inside the call.
- `std::string(nullptr)` throws in libstdc++, so a `char const*` note overload, or a guard that
  skips building a string, is neutral only once every non-literal `char const*` value is proven
  non-null.
- To list every call that resolves to a new overload: in a scratch copy, mark it `[[deprecated]]`
  and append `#error` to each TU; pb-syntax-check prints a failed TU's warnings, so every caller
  shows up.
- If a sibling overload leaves the old form of an anonymous-namespace function with no callers,
  CI's `-Wunused-function` fails: replace the old form instead.
- Taking a multi-entry search's hits entry by entry rebuilds per-entry searches exactly; a sort
  afterwards makes the grouping irrelevant only if its key is unique.
