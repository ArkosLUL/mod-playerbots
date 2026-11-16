# t-triggers: cheaper ToC memo refreshes

Lane of [raid-perf-pass.PLAN.md](raid-perf-pass.PLAN.md) (wave 1, beside `a-shared-fc`). The overview's
neutrality rule, file ownership, git, Docker and verification rules bind this lane.

Make the per-instance per-ms memo refreshes that the hottest ToC trigger rows pay cheaper, without
adding, dropping, reordering or front-running a single memo call. Every row below measures a refresh,
not its trigger's own logic: the first node in `ToCStrategy.cpp` `InitTriggers` order that reads a
memo pays it. Baseline µs per check (2026-11-16, 25H): FC personal defensive 74.5 (Defence `Refresh`),
gormok tank duty 10.0 in Jaraxxus and 21.3 in FC (ReadGate; its inner trigger never runs after the
beasts stage), FC opener 16.6 (Ball `Refresh`), FC should focus 15.9 / anti fear 8.4 / pet autocast
9.4 (FC `Refresh`, and the Defence `Refresh` for pet owners), northrend worms misplaced 9.0, gormok
dps target 6.5. Dumps: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/perf/_pmon/run2/`.

Candidates. Line numbers are as of Custom `568566f6b`; re-verify. Estimates come from reading only:
no profiling build is allowed.

- **T1, gate sweep stops at the first hit** (`ToCEncounterGate.cpp` `EntryEngaged` ~121 and the FC
  branch of `EncounterEngaged` ~142). Replace list building + `AnyEngaged` with
  `Acore::CreatureSearcher` (stops testing after a hit, `GridNotifiersImpl.h` ~433) whose check is the
  exact predicate of the current call: the entry test and range check of the check class behind
  `GetCreatureListWithEntryInGrid` (`GridNotifiers.h` ~1525, which today copies the entry vector and
  runs a 28-way `any_of` per creature), then `IsEngaged`. Same visit, same phase filter, same boolean.
  Proof: review. Estimate 3–9 µs (Jaraxxus), more for FC.
- **T2, gate tries a candidate first** (same file; `ToCGateState` ~75, `ReadGate` ~155). Before the
  T1 sweep, test one candidate: Jaraxxus by its guid slot (`DATA_JARAXXUS` in the core's
  `src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/trial_of_the_crusader.h`;
  derive its implicit value, and fix the gate's comment claiming Jaraxxus has no slot if it is
  wrong), Icehowl by slot `TOC_DATA_ICEHOWL`, champions by the last engaged champion guid stored in
  `ToCGateState`. The candidate must pass the sweep's full test plus an exact visit test: its
  container cell (`GetCurrentCell()`) within ±1 cell of the bot's container cell. A position bound
  (`|dx|+|dy| < r`) is not exact, because a cell move reaches the grid containers only through the
  map's move list (`Map.cpp` ~826-843); ±1 cell is visited by every r = 200 visit whatever the float
  rounding (`CellImpl.h` ~116-162). On a miss run T1 unchanged, so the hint only ever answers true
  where the sweep would. Proof: harness with `CalculateCellArea`/`VisitCircle` copied in as the
  oracle over enumerated positions and cells; red check: drop the cell test, mismatches must appear.
  Drop T2 if the proof does not close. Saves nearly the whole sweep while a fight is live.
- **T3, entry-set list search helper** in `ToCHelpers_Shared.h/.cpp` (no new `.cpp`):
  `Acore::CreatureListSearcher` into a `std::vector` (`ContainerInserter` takes any container with
  `push_back`, `GridNotifiers.h` ~196-217) with an O(1) entry test and otherwise the identical
  check, visit, phase and order. Adopt in `ScanChampions` (`ToCHelpers_FactionChampions.cpp` ~503),
  FC Defence `UpdateHazards` (`ToCHelpers_FactionChampionsDefence.cpp` ~932) and the Jaraxxus scan
  (`ToCHelpers_Jaraxxus.cpp` ~127). Lane `b-anub-twins-jarax` adopts it in Anub'arak and Twins in
  wave 2, so keep its interface general. Proof: membership and order harness against the current
  check plus review of the call sites. 1–5 µs per sweep.
- **T4, flat threat snapshot** (`UpdateFocusClock`, `ToCHelpers_FactionChampionsDefence.cpp`
  ~2059). `RepickClock::threat` is read only inside `UpdateFocusClock`; replace the per-champion
  `std::unordered_map` rebuilt every refresh with a flat vector of (guid, threat) pairs swapped with a
  reused scratch vector. The snapshot and the `moved` count must come out identical, including where
  the old code depended on map iteration order. Proof: pure-function harness. 8–15 µs per refresh.
- **T5, lazy Beasts roster** (`ToCHelpers_NorthrendBeasts.cpp`, `GatherRoster` ~157, called ~224 in
  `Deal`). Build the roster only when pass 2 finds a live duty without a holder. First confirm that
  skipping `GatherRoster` (25 `IsBeastsTank` = two `IsTank` each: `GET_PLAYERBOT_AI` lookups and the
  `specTabCache` mutex, `AiFactory.cpp` ~139-179) has no side effect beyond filling a pure cache;
  otherwise drop it. Proof: review. 2–4 µs.
- **T8, count instead of a vector** (`ToCHelpers_FactionChampionsBall.cpp` ~302, where only
  `TrinketsInReach(...).size()` is used). Add a counting variant; the iterating caller ~702 keeps the
  vector. Every `FactionChampionsGripGuarded` call inside is a Defence `Refresh` memo call: keep each
  call, its order and the early `return {}` exactly. Proof: review. ~1 µs.

Not in this lane: T6 and T3's Anub'arak/Twins adoption (lane b), T7 (`RaidOpener.cpp`, lane a), and
any behavior-changing idea (per-tick memos, guid-only gate, arena-centred sweeps, lazy Defence
`Update*`); note such ideas under Known gaps, never implement them. No candidate needs `RaidObs`
changes; trace bytes must stay identical.

One commit per candidate, each compiling (`pb-check` over its paths), harness files in
`build-toc-wt/perf/_harness/t-triggers/`, never committed.

## Task list

## Candidates done

## Candidates dropped

## Decisions for review

## Known gaps

## Blocked
