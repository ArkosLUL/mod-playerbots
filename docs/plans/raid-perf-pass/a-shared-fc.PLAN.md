# a-shared-fc: shared raid helpers and ToC FC/Beasts multipliers

Lane of [raid-perf-pass.PLAN.md](raid-perf-pass.PLAN.md) (wave 1, beside `t-triggers`). The overview's
neutrality rule, file ownership, git, Docker and verification rules bind this lane.

Implement the overview's "Wave 1 — lane `a-shared-fc`" items 1–6 (A0 splash cache API, R4 cares
table, N2 `RaidBurstWindow`, the `RaidObsEngine.cpp` guard, N1/R3 cache parts, T7 one opener sweep).
Line references there predate `44b6dcaa0`; re-verify against Custom.

- Multipliers run once per popped useful action, so per-call savings multiply. Baseline totals over a
  full 25H clear (2026-11-16): FC grip guard 749 ms, `hold burst until tank engaged` 638 ms, FC
  suppress aoe 489 ms, then ~0.3 µs each across the other ToC multipliers. Dumps:
  `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/perf/_pmon/run2/`.
- Shared helpers keep every old signature: the ToC upstream PR later ports or skips these changes,
  and other raids keep calling the old forms.
- `RaidOpener.cpp` carries both A0 adoption and T7; commit them separately. T7: `ReadOpener` does one
  entry-set sweep instead of one per boss entry, buckets hits per entry in config order and keeps
  visit order inside each bucket, so `bosses` comes out identical. Proof: bucket-order harness.
- R4 touches `RaidEncounter.h`/`RaidEncounterRules.h`, which fan out to 100+ TUs: on `INCOMPLETE`,
  rerun `pb-check` with `PB_MAX_FANOUT=all` in the background and commit only once green.
- Lane `t-triggers` owns the ToC `Util` helpers in parallel; a change you need there goes in your
  report, never into the file.

One commit per fix, each compiling (`pb-check` over its paths), harness files in
`build-toc-wt/perf/_harness/a-shared-fc/`, never committed.

## Task list

Sites re-verified against Custom `905e22d9e`. Lead implements all of it (no split). Harnesses in
`build-toc-wt/perf/_harness/a-shared-fc/`.

Found while verifying: `CastSpellAction::spell` is not fixed after construction (`PullAction::InitPullAction`
reassigns it in `isPossible`/`Execute`), so a cached `"spell id"` `Value*` is re-fetched whenever
`getSpell()` differs from the name it was fetched for. Names (`getName`), threat types and dynamic types are
fixed per `Action*` (every `getName` override derives from members set in the constructor; every
`getThreatType` override returns a constant).

1. **OBS** `RaidObs::NoteHazardCircle` (`src/Bot/Obs/RaidObsEngine.cpp:540`): return before building the
   `"rad":` string when `!Active()`. Proof: review (`Num` is a pure `snprintf`; `NoteHazard` returns on
   `!Active()` as its first statement).
2. **A0** `RaidCombatHelpers.h/.cpp`, `RaidSplash.h/.cpp`: `RaidActionCache<Facts>` (per-`Action*` facts with a
   last-action shortcut), `RaidSpellIdRef` (cached `"spell id"` `Value*`, re-fetched on a spell change,
   `Get()` on every read), `RaidSplashCache`, and overloads `IsRaidAreaDamage(botAI, action, RaidSplashCache&)`
   and `SpellSplashes(botAI, action, RaidSplashCache&)`; old signatures untouched. Proof: equivalence harness
   (old bodies from Custom vs new, every action kind × name set × threat type × spell info shape, spell renames
   and spell id changes mid-sequence; compares results and the `"spell id"` value creation and `Get()` sequence).
3. **N2** `RaidBurstWindowMultiplier::GetValue` (`RaidBurstWindow.cpp:14`): per-`Action*` burst/lust facts
   instead of a name copy and two set lookups per action; `IsInCombat` and the per-ms window memo unchanged.
   Proof: review (pure functions of the fixed name and the bot's fixed class).
4. **A0-opener** `RaidOpenerHoldMultiplier` (`RaidOpener.h/.cpp:280`): a `RaidSplashCache` member for its
   `IsRaidAreaDamage`. Proof: the A0 harness plus review of the one-call swap.
5. **T7** `ReadOpener` (`RaidOpener.cpp:63`): one `GetCreatureListWithEntryInGrid(list, entries, r)` sweep (the
   core's multi-entry overload: same visit, same `IsWithinDist` test), then hits taken entry by entry in config
   order, visit order inside an entry; `liveBosses` still read once at the same point. Proof: bucket-order
   harness (old vs new `ReadOpener` bodies over enumerated visit lists, configs with repeats, multi-tick carry-over).
6. **R4** `RaidEncounter.cpp`: one `CaresTable` per definition per bot (a 64-bit cares mask per action, last-action
   shortcut), bound to each encounter multiplier after construction in `AddMultipliers` (a definition past 64
   multipliers starts another table); `CaresSlot`/`CaresBits` std-only in `RaidEncounterRules.h`;
   `RaidEncounter.h` unchanged; `BotHasRole` not cached. Proof: nativetest bit i == `RuleCares`/family overlap of
   slot i over every combination of the named family bits × names, plus the 64-slot cap.
7. **N1a** `FactionChampionsSuppressAoeMultiplier` + `Splashes()` (`ToCMultipliers_FactionChampions.cpp:26,47`):
   cached heal/threat/own-splash/summon facts, `SpellSplashes` via the cache, both `"spell id"` `Get()`s kept.
   Proof: harness (old vs new bodies, `ToCEncounterIsLive` memo stub in the call log).
8. **N1b** `FactionChampionsLateInterruptMultiplier`, `FactionChampionsGripGuardMultiplier`: cached casts and the
   `" on enemy healer"` test, `RaidSpellIdRef` for `"spell id"`; every memo/helper call stays in place. Proof:
   harness (old vs new bodies with logging stubs for every helper they call).
9. **N1c** FC `ThreatRedirectVeto`, `CcGuard`; FC Defence `AoeGuard`, `DispelGuard`, `PurgeGuard`: per-`Action*`
   cast results (the cure classes never reassign `spell`). Proof: review.
10. **R3** Gormok `SnoboldCarrier`, Jormungars `MoveGuard`/`BileReachGuard`/`RedirectGuard`, Icehowl
    `ChargeGuard`: per-`Action*` casts and name tests (`"avoid aoe"`, `ToCEncounterOfTrigger`). Proof: harness
    (the Gormok/worm name logic moved into a helper, so review alone was not enough).

## Candidates done

Harnesses: `build-toc-wt/perf/_harness/a-shared-fc/` (`python -I run.py [name]`, `python -I rules_red.py`); old
bodies cut verbatim from `905e22d9e` by `extract.py`, new from the worktree, each red variant must fail. The
independent review reran all of them green with every red failing; the splash harness now also feeds a spell id
that has no SpellInfo.

| id | site | why neutral | proof | commit |
|---|---|---|---|---|
| OBS | `RaidObs::NoteHazardCircle` | `Num` is a pure `snprintf`; `NoteHazard` returned on `!Active()` first thing, so the skipped string was always thrown away | review | `4ae762591` |
| A0 | `RaidSplash.*`, `RaidCombatHelpers.*`: `RaidActionCache`, `RaidSpellIdRef`, `RaidSplashCache`, cache overloads of `IsRaidAreaDamage`/`SpellSplashes` | caches only per-action facts fixed at construction (threat type, name set hits, casts); `"spell id"` value is created at the same first read and `Get()` runs on every read; `Value*` re-fetched when `getSpell()` changes | harness `splash`: 600,668 calls, 0 mismatches; 4 reds fail | `4f6b44b54` |
| N2 | `RaidBurstWindowMultiplier::GetValue` | burst/lust are pure functions of the fixed name and the bot's fixed class; `IsInCombat` and the per-ms window memo run at the same points | review | `ee8d2f669` |
| A0-opener | `RaidOpenerHoldMultiplier` | swaps one `IsRaidAreaDamage` call for the cache overload at the same point | review + harness `splash` | `304cff41c` |
| T7 | `ReadOpener` | the core's multi-entry overload runs the same cell visit with the same entry and `IsWithinDist` tests; hits taken entry by entry in config order keep each entry's visit order; `liveBosses` read once at the same point; `Map::Visit` loads no grid, so the sweeps are pure | harness `opener`: 3,560,880 calls (every world of up to 2 creatures × 8 configs incl. repeats, 150k random worlds, 10 ticks each with world changes), 0 mismatches; 2 reds fail | `3a0aaaf3d` |
| R4 | `RaidEncounter.cpp` `CaresTable`, `RaidEncounterRules.h` `CaresSlot`/`CaresBits` | `ClassifyAction` and `getName` are pure and fixed per action, so computing every slot's bit at first sight equals each multiplier computing its own; gate/role/predicate calls only follow a true cares, as before | nativetest (every combination of 13 families × 8 names × 13 slots, 64-slot cap; 4 reds fail) + harness `cares` (engine loop over definitions of 3–130 multipliers: 108,312 passes, 0 mismatches; 3 reds fail) | `8b38899f4`, `02e6952c4` |
| N1a | `FactionChampionsSuppressAoeMultiplier` + `Splashes` | as A0; both `"spell id"` `Get()`s kept; the memo still runs only after the splash test | harness `fc` | `b91f8e930` |
| N1b | FC `LateInterrupt`, FC Defence `GripGuard` | casts and the `" on enemy healer"` test fixed per action; memo, helper and `GetTarget` calls in the same order under the same conditions | harness `fc`: 1,168,851 calls over all three FC multipliers, 0 mismatches; 7 reds fail | `e7b5c1203` |
| N1c | FC `ThreatRedirectVeto`, `CcGuard`; FC Defence `AoeGuard`, `DispelGuard`, `PurgeGuard` | each cached test is the old cast expression on a fixed dynamic type, run before the first memo call as before; the cure classes never reassign `spell` (only `CastShootAction`'s ctor and `PullAction::InitPullAction` do) | review | `0a35e1f0c` |
| R3 | Gormok `SnoboldCarrier`, Jormungars `MoveGuard`/`BileReachGuard`/`RedirectGuard`, Icehowl `ChargeGuard` | casts and name tests fixed per action; `ToCEncounterOfTrigger` reads a constexpr table; the class check stays ahead of the redirect cache | harness `beasts`: 1,373,760 calls, 0 mismatches; 4 reds fail | `407bb950e` |
| — | comments | wording only | pb-check | `ef9dcf80d` |

## Candidates dropped

- FC `PhysicalSwitchGuard`, Icehowl `BreathSpreadGuard`, NB `ControlTankMovement`: one `dynamic_cast` each; a
  pointer-keyed hash lookup costs about the same, so caching buys nothing.
- NB `TauntGuard`: `EncounterHelpers::IsTauntAction` reads `IsTank` (a role, never cached) and lives in `src/Util`.

## Decisions for review

- `RaidSpellIdRef` copies `getSpell()` and compares it on every read because `PullAction` swaps its spell at run
  time; still far cheaper than the `"spell id::<name>"` build, hash lookup and `dynamic_cast` it replaces.
- `DispelGuard` caches a `getSpell()` comparison: safe only while no cure class reassigns `spell`.
- `NoteHazardCircle`: a trace opening between the new `Active()` check and the one inside `NoteHazard` (a
  nanosecond window on another thread) loses one hazard row, the race every `if (Active())` guard already has.
- R4 binds the table after construction (`static_cast` in `AddMultipliers`, valid because `AddRule`/`HandWritten`
  are the only creators), which keeps `RaidEncounter.h` and `MultiplierCreator` unchanged.
- T7 uses the core's `GetCreatureListWithEntryInGrid(list, entries, r)`, which came from upstream AC (#22286), so
  the ToC PR needs no core change.
- For wave 2: `RaidActionCache` and `RaidSpellIdRef` (`RaidCombatHelpers.h`) serve the Anub'arak `SlowOrSpread`,
  `StopsCast` and `ProtectSpikeKite` caches; ICC sites take a `RaidSplashCache` member and the cache overloads.

## Known gaps

- OBS, N2, A0-opener and N1c are proven by review only, as the overview allows for trace gates and cast caching.
- Harness stubs model `IsWithinDist` and phase as a flag and the grid visit as a list; the real
  `Cell::VisitObjects` is shared by both overloads and not exercised. The cares harness stubs `ClassifyAction`.
- No pmon numbers yet: the after pull (step 5) measures the gain.
- Review: the subjects of `e7b5c1203` (80 chars) and `b91f8e930` (74) run past the 72-char limit. They stay,
  since the lane may not amend or rebase.

## Blocked
