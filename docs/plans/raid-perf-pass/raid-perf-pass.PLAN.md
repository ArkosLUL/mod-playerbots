# Raid perf pass: ToC, shared raid code, ICC — zero behavior change

In flight since 2026-11-08. The status table is the source of truth for what ran.

## Status

| Step | Lane | State | Commits | Notes |
|---|---|---|---|---|
| 1 | baseline pull | done | | 2026-11-16 full ToC 25H clear, one pmon dump per fight in `build-toc-wt/perf/_pmon/run2/`; ToC triggers + multipliers 22.7% of raid bot think time |
| 2 | go/no-go | go | | user, 2026-11-16: re-aimed at the hot triggers (lane `t-triggers`), no behavior-changing W items |
| 3 | a-shared-fc (wave 1) | pending | | |
| 3 | t-triggers (wave 1) | pending | | |
| 3 | b-anub-twins-jarax (wave 2) | pending | | |
| 3 | c-icc (wave 2) | pending | | |
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
