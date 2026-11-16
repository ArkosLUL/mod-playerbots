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

Re-verified on Custom `905e22d9e`. One lead, no split: six small candidates, mostly in different files
but T1/T2 share the gate and T1 uses T3's entry set. Commit order: T3, T1, T2, T4, T5, T8.

- **T3** `ToCHelpers_Shared.h/.cpp` (new `ToCEntrySet`, bitset over min..max entry, and
  `GetCreatureListWithEntriesInGrid(WorldObject const*, std::vector<Creature*>&, ToCEntrySet const&,
  float)`: `Acore::CreatureListSearcher` + `Cell::VisitObjects(origin, …, radius)`, check =
  `Contains(GetEntry()) && origin->IsWithinDist(unit, range, false)`). Adopt at
  `ToCHelpers_FactionChampions.cpp` ScanChampions L504-506, `ToCHelpers_FactionChampionsDefence.cpp`
  UpdateHazards L933-935, `ToCHelpers_Jaraxxus.cpp` ScanOf L126-127 (`std::list` → `std::vector`, loops
  unchanged). Proof: harness (old check copied verbatim from `GridNotifiers.h`
  `AllCreaturesMatchingOneEntryInRange` vs new, every entry list in ToC plus edge lists, every entry in
  and around their ranges, both distance outcomes; result and `IsWithinDist` call sequence; list vs
  vector insertion order through a copied `ContainerInserter`); red check: off-by-one in `Contains`.
- **T1** `ToCEncounterGate.cpp` `EntryEngaged` L121-126, FC branch L139-144, `AnyEngaged` L112-119
  removed: `Acore::CreatureSearcher` with check = entry test, `IsWithinDist(c, 200, false)`, then
  `IsEngaged`, same `Cell::VisitObjects(bot, …, 200)`. FC entry test via a `ToCEntrySet` of
  `CHAMPION_ENTRIES`. Proof: review (the cell walk is unchanged, only the per-container loop stops;
  every skipped call is a pure getter; the boolean is ∃ visited creature passing the same tests).
- **T2** same file, `ToCGateState` L75-83 (+ last engaged champion guid), `EncounterEngaged`
  L128-152, header enum (+ `TOC_DATA_JARAXXUS = 10`: `DATA_JARAXXUS` is the 11th entry of
  `TrialOfTheCrusaderObjectData`; fix the L128 comment, Jaraxxus and Icehowl both have slots). Before
  the T1 sweep: candidate (Icehowl slot 8, Jaraxxus slot 10, FC stored guid) passes phase, entry,
  range and engaged tests, its position cell lies within ±1 of the bot's standing cell (valid
  coords), and a one-cell `map->Visit` of that cell's `GridTypeMapContainer` finds it (container
  membership and grid-loaded test exactly as the sweep's own per-cell visit; `GetCurrentCell()` is
  protected in the core, and a position cell can lag the container through the move list, so the
  visit is the exact test). Miss: T1 sweep unchanged; FC stores the sweep's hit or clears. Proof:
  harness with `Compute`/`CalculateCellArea`/`Cell::Visit`/`VisitCircle` cell selection copied in,
  exhaustive over every float bot coordinate per axis then the cross product of distinct axis cases:
  the ±1 block is always visited; red checks: ±2 block and "no cell test" (any candidate within
  200 yd) must miss.
- **T4** `ToCHelpers_FactionChampionsDefence.cpp` `RepickClock::threat` L332, `UpdateFocusClock`
  L2071-2088: guid-sorted flat vector, built into a reused per-state scratch then swapped; insert
  overwrites an equal guid (map `operator[]` semantics), lookup by `lower_bound`. Proof: pure-function
  harness (old map body verbatim vs new) over enumerated old snapshots, ref sequences (duplicates and
  non-players included) and threat values straddling both thresholds; snapshot content and `moved`
  compared; red check: drop the overwrite.
- **T5** `ToCHelpers_NorthrendBeasts.cpp` `Deal` L224: `GatherRoster` built at the first pass-2 duty
  that needs it. Proof: review (nothing between pass 1 and that point feeds the roster;
  `CountsForRoster`/`IsBeastsTank` read only pure getters, `GET_PLAYERBOT_AI` a map find, and
  `specTabCache` is exact: invalidated, never time-windowed).
- **T8** `ToCHelpers_FactionChampionsBall.cpp` `TrinketsInReach` L256-273: one loop writing into a
  nullable vector and returning the count, `return {}` kept as clear + 0; the counting caller L302
  passes null, L702 keeps the vector. Proof: review (same calls, same order, same early exit).

## Candidates done

Harnesses in `build-toc-wt/perf/_harness/t-triggers/`: `extract.py <t2|t3|t4>` pulls the core, Custom
and worktree bodies verbatim into `gen/` (and the red mutation into `gen_red/`), failing on any missed
anchor; `run.sh` builds each harness as is and with `-DRED` inside `acore/ac-wotlk-build:master`.

- **T3** `7cfb6d25e`, `ToCHelpers_Shared` `ToCEntrySet` + `GetCreatureListWithEntriesInGrid`,
  adopted in ScanChampions, UpdateHazards, Jaraxxus ScanOf. Neutral: same `Cell::VisitObjects`, same
  `CreatureListSearcher` phase test, same check (entry membership, then `IsWithinDist(…, false)`
  only on a match), `push_back` in visit order into a vector the callers only iterate. Proof:
  `t3_entryset.cpp`, 1,172,540 cases (every ToC entry list plus edge lists, every entry within 70 of
  each list's range and 40k random, both range outcomes, random cells through a copied
  `ContainerInserter`), 0 mismatches in result, `IsWithinDist` log and container order; red
  (`Contains` shifted by one) 226,908.
- **T1** `2501a4447`, gate `FirstEngaged` (`Acore::CreatureSearcher`) for `EntryEngaged` and the FC
  branch. Neutral by review: `Cell::VisitObjects` still walks every cell (the searcher only returns
  early inside a container, and `Map::Visit`/`VisitCell` create nothing); the skipped calls are
  `GetEntry`, `IsWithinDist`, `IsAlive`, `IsInCombat`, all pure getters; the old answer was "some
  visited creature passes entry, range and `IsEngaged`", which is exactly the searcher's stop
  condition. FC membership via `CHAMPION_ENTRY_SET` is covered by the T3 harness's champion list.
- **T2** `5414da8e1`, gate `SweepFinds` before the sweep for Icehowl (slot 8), Jaraxxus (new
  `TOC_DATA_JARAXXUS = 10`) and FC (`ToCGateState::champion`, the last sweep hit). Neutral: it answers
  true only for a creature that passes the sweep's own phase, entry, range and engaged tests, sits in
  the grid container of a cell within ±1 of the bot's standing cell (found there by the same
  `map->Visit` the sweep uses per cell, so container lag and unloaded grids fall through), with the
  bot on `IsValidMapCoord`; such a cell is always visited, so the sweep would have answered true.
  Every miss runs the T1 sweep unchanged. Added calls (`GetGuidData`, `Map::GetCreature`, one cell
  visit) are pure. Proof: `t2_cells.cpp`, every float bot coordinate per axis in [-17200, 17200]
  passing `IsValidMapCoord`, reduced to 575 (standing, low, high) cell cases, crossed into 330,625 bot
  cells, 2,968,729 block cells checked, 0 unvisited; red: ±2 block 26,028 unvisited, "no cell test"
  (candidates within 202 yd) 111,949 in unvisited cells. Review: the harness skipped bot coordinates
  whose standing cell is invalid, where `Cell::Visit` visits nothing but `SweepFinds` (no standing
  cell test) could still hit; it now fails on one, and none exists under `IsValidMapCoord`.
- **T4** `889be7091` (+ comment `f4afcf785`), Defence `RepickClock::threat` as a guid-sorted vector,
  built in `threatScratch` and swapped. Neutral: the snapshot is only read through the lookup in the
  same loop, so content and `moved` are all that can differ. Proof: `t4_threat.cpp`, old loop from
  Custom vs new, 10,978,082 cases (exhaustive two refreshes of up to two refs over 3 players, a
  non-player, a null victim and 9 values straddling both thresholds, plus 200k random interleaved
  3-champion sequences sharing the scratch), 0 mismatches; red (no overwrite of a repeated guid)
  2,094,970.
- **T5** `797a9a94e`, Beasts `Deal` builds the roster at the first duty pass 2 deals. Neutral by
  review: pass 2 reads the roster only after that point, nothing between pass 1 and it changes what
  `GatherRoster` reads, so the built roster is identical; when skipped, the skipped calls are pure
  getters, `GET_PLAYERBOT_AI` (a map find), `ContainsStrategy` (a mask test) and `GetPlayerSpecTab`,
  whose `specTabCache` is exact (invalidated on all four talent hooks in `Playerbots.cpp`, keyed on
  level and spec mask), so a skipped fill changes no later answer.
- **T8** `d95abfc59`, Ball `CollectTrinketsInReach` with a nullable list; UpdateOpener counts, the
  target pick keeps `TrinketsInReach`. Neutral by review: same calls in the same order
  (`GetExactDist2d`, `FactionChampionsGripGuarded` memo call, `FactionChampionsTrinketReady`,
  `IsImmunedToSpell`), the veto still returns at the same champion with an empty list and count 0,
  and count equals the pushes.

pb-check over the lane's union (9 files, 37 TUs): green.

Review (independent): no behaviour difference found in any hunk; all three harnesses rerun from a fresh
`extract.py` against Custom `905e22d9e`, green with every red check detected. Comment fixes:
`85c4d677a` (two comments that only named the core code they were copied from), `6a3a79e28` (the T8
reflow's "catches it"). pb-check over the union after them: green.

## Candidates dropped

None.

## Decisions for review

- T2 keeps a per-instance hint (`ToCGateState::champion`) shared across bots and refreshes. It is a
  guess validated in full on every use, never an answer: the gate result is the sweep's in every case.
- T2 tests `IsValidMapCoord` on the bot: past the map edge the clamped standing cell leaves ±1 cells
  unvisited (the first harness run found 3,457), so the hint stands down there.
- T2 tests container membership with a one-cell `map->Visit` because `GetCurrentCell()` is protected
  in `MovableMapObject`; a position bound alone would not be exact (move-list lag).
- T4 keeps "last value wins" for a repeated guid although the core's threat list is keyed by victim
  guid and cannot repeat one.
- `ToCEntrySet` allocates one bit per entry between min and max: the Defence scan list spans totems
  (5913) to champions (34475), ~3.5 KB once per process.

## Known gaps

- `ToCHelpers_NorthrendBeasts.cpp` `RefreshIcehowl` says Icehowl has no guid slot; `DATA_ICEHOWL` (8)
  is one. Switching that search to the slot changes when Icehowl is found, so it is a behaviour
  change, left alone.
- Behaviour-changing ideas left out by rule: one refresh per world tick, a guid-only gate (W2: the
  slots now in the header make it a one-liner), arena-centred sweeps, lazy Defence `Update*`.
- T2 only spares the sweep while the boss or the remembered champion is within one cell of the bot;
  between pulls (encounter not in progress) the gate never sweeps anyway.
- T2's one-cell proof is for the 200 yd `ENTRY_SEARCH_RADIUS` only; changing that radius needs the
  `t2_cells.cpp` rerun at the new value, or `SweepFinds` can answer true where the sweep would not.

## Blocked
