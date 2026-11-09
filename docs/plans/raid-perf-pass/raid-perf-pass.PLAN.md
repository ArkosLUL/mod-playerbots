# Raid perf pass: ToC, shared raid code, ICC — zero behavior change

In flight since 2026-11-08. The status table is the source of truth for what ran.

## Status

| Step | Lane | State | Commits | Notes |
|---|---|---|---|---|
| 1 | baseline pull | pending | | user rebuilds from Custom, one Anub'arak 25 pull with pmon |
| 2 | go/no-go | pending | | |
| 3 | a-shared-fc (wave 1) | pending | | |
| 3 | b-anub-twins-jarax (wave 2) | pending | | |
| 3 | c-icc (wave 2) | pending | | |
| 5 | after pull | pending | | |
| 7 | closeout | pending | | |

States: `pending`, `merged`, `blocked` (branch and worktree kept, reason in the row), `dropped`.

## Context

ToC strategies are mostly done. The user wants a performance pass that makes raid bots cheaper on
server CPU **without changing any bot behavior**, run by an orchestrator that delegates the work.

- Scope (user's choice): ToC, shared raid helpers (`src/Ai/Raid/*.cpp/h`), RaidObs call sites
  (fork-owned `src/Bot/Obs` included), ICC. Changes stay on Custom; no port to the upstream PR stack
  (#2876 → #2985 → #2986), whose copies of `RaidEncounter.*`/`RaidSplash.cpp` already differ.
- Out of scope (user's choice): engine/core code (`src/Bot/Engine`, `src/Ai/Base`,
  `PlayerbotAI.cpp`; keeps upstream merges easy), Ulduar, older raids, the RaidInstanceState lock
  cache (R1: crash risk, no TSan on Docker Desktop), the optional N4 and `NoteDerived(char const*)`
  overload (low gain).
- Other ToC/ICC work (`w5-anubarak-rest-rest`, `refine-fc-13`, ICC `r14-closeout`) waits until this
  pass merges. `refine-anubarak-32` landed as `44b6dcaa0`; the tree was clean at planning time.

Why raid code is hot: per bot pass in ToC, ~127 interval-1 trigger nodes plus ~118 multipliers, and
each multiplier runs once per popped useful action; 28 are ungated and run on every action anywhere
in ToC. Expected gain is modest (a few µs per action per bot); widening the per-ms memos would gain
more but changes freshness, so the rule excludes it. A baseline pull decides whether the pass runs.

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

**Wave 2 — branched from Custom after wave 1 merged, two lanes in parallel:**
- **`b-anub-twins-jarax`:** `ToCMultipliers_Anubarak.cpp/.h` (R6 via the splash cache, R11
  ProtectSpikeKite per-`Action*` classification, Value* pointer in SlowOrSpread and StopsCast);
  `ToCHelpers_Anubarak.cpp` `ResolveAlive` on a stack buffer keeping its snapshot (never loop the
  state vector in place: a nested ReadPhase→Sweep clears `state.burrowers`) and the pure trace gates;
  TwinValkyr and Jaraxxus multipliers (N1 part, `TwinValkyrInterruptHoldMultiplier` name copy);
  TwinValkyr trace gates (`GetWantedEssence`, `GetTwinDpsTarget`). Not neutral, leave alone: the
  Anub note with `++` in its argument, `NoteHands`, Staging `NoteStage`, any memo on
  `GetAnubarakDpsTarget`, `ProtectorOrder`.
- **`c-icc`:** splash cache at the 15 `IsRaidAreaDamage` sites and Sindragosa's (cache members in
  the multiplier headers; leave `IccLichKingSplashMultiplier`'s own cache and `IccDbsAreaDamage`'s
  order as they are); `char const*` overloads for the 13 note wrappers and `Active()` guards on pure
  argument builds (LichKing `Tag()`, BloodPrinces, Rotface, Sindragosa, Putricide); N3
  `IccGetCreaturesByEntries` in one grid visit with the identical per-entry predicate and range.

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
- Agents: wave 1 lead + reviewer + merge, wave 2 two leads + two reviewers + merge = 8.

## Steps

1. **Baseline (user):** rebuild the server from current Custom, then one Anub'arak 25 pull on the
   usual difficulty: `.playerbots pmon toggle` in the instance, `.playerbots pmon reset` at the pull,
   `.playerbots pmon stack` after, `.playerbots pmon toggle` to switch off. Never `pmon tick`
   (unguarded null deref).
2. **Go/no-go:** I read `pmon_total.json` from the container (read-only `docker exec cat`), report
   the share of bot think time in ToC triggers and multipliers with a recommendation (rule of thumb:
   under ~10% → cancel); the user decides.
3. On go: save this plan as `docs/plans/raid-perf-pass/raid-perf-pass.PLAN.md` with a status table
   and one brief per lane written through `prepare-prompt`; commit them on Custom; write the script;
   launch waves 1+2 chained.
4. Report merged commits, dropped candidates, review tallies.
5. **After (user):** rebuild, the same pull and the same four commands; I compare the two
   `pmon_total.json` files by ratios (targeted multipliers vs trivial ones, multiplier and trigger
   share vs `UpdateAIInternal`), not absolute times.
6. If the new build seems to act differently: I locate the fix with read-only tools and propose
   reverting its commit; revert only on the user's word.
7. Closeout: fold durable lessons (memo calls are side effects; what is cacheable per `Action*`)
   into `docs/engine/raid-mechanics-lessons.md` "What a strategy costs per raid", and add one line
   each for the skipped candidates to its open cheap wins (engine: double-hash value lookups, the
   queue's name-copy compares, `LogAction` formatting; widened per-ms memos; R1; Ulduar and older
   raids), via `compact-docs-writer`; delete the plan dir; commit.

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
- Trace gates, cast caching, const refs: compile + reviewer proof that every skipped argument is pure.
- Merge agent: pb-check over the union of merged files on the main tree.
- pmon shows the gain; the harnesses and reviews are the proof that behavior didn't change.
