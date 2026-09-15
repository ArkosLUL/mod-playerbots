# Cutting iterations out of the raid-strategy loop — Tier 4

## Context

Tier 1 (`2816b7171`) made a trace say whether the pull counts. Tier 2 (`4fe60957e`) made the engine
count what each node did and made the corpus queryable. Tier 3 (`a6195995a`) made all declared probe
keys legible and added before/after comparison.

Measuring again, the bottleneck has moved into the **analysis layer**, and it is the same disease the
recorder had at Tier 2: it grew a second copy of itself instead of a shared spine. Every new fight is
re-implemented from scratch, and the reusable half stays trapped inside one boss's file.

The clearest statement of it is the Yogg-Saron plan's own, written this week and now shipped
(`33beff4c1`):

> Every number in section 6 came from a throwaway script; none of it is reproducible from the CLI
> today.

Its item 5 was scheduled rework to port that script into `yogg_saron.py`. **That port is the
recurring tax**, and it is paid per fight.

### Measured this pass

1. **~45 direct `math.dist`/`math.hypot` calls across 4 files, no shared helper**, in two dialects
   (`yogg_saron.py` uses `dist` on 2-tuples, `flame_leviathan.py` uses `hypot(dx, dy)`). "Distance
   from a fixed reference point" appears at 7 sites for Yogg's `BODY` alone and is a named function
   exactly once anywhere — `flame_leviathan.nearest_corner`.
2. **Six `position_at` implementations with three different policies** (last-at-or-before,
   nearest-no-tolerance, nearest-within-1500 ms): `yogg_saron.py:643`, `:403`, `:1140`,
   `analysis.py:17`, `views.py:407`, `flame_leviathan.py:277`/`:304`. Six `tracks` builders of the
   same `(t, x, y, …)` shape that `analysis.position_runs` already consumes. Three medians — one
   hand-rolled at `yogg_saron.py:654` in a file that imports `statistics` at line 42 and calls
   `statistics.median` at line 280.
3. **Two Tier 3 functions were re-implemented inside `yogg_saron.py` within one cycle.**
   `phase_spans` (`:206`) is `probes.latch_windows` un-grouped. `missing_probes` (`:220`) is
   `probes.silent_keys`, but backed by a hand-kept `PROBE_KEYS` tuple that is **already wrong** — it
   omits `yogg.p1dodge`, `yogg.p1station` and `yogg.p1taunt`, three keys the same file reads at
   `:363`, `:366`, `:369`. That is exactly the `CONSUMED_KEYS` failure Tier 3 deleted, back inside a
   week, which says the shared function was not findable.
4. **The most reusable view in the tree is trapped in one boss's file.** `show_threat`
   (`yogg_saron.py:1078`) — time-weighted share of who a hostile set was targeting, by role, off
   `snap.u[7]` — would answer the same question on Kologarn, Auriaya or the Iron Assembly unchanged;
   its `REDIRECT_SPELLS`/`TAUNT_SPELLS` are class spells, not fight spells. So would `show_frozen`
   (`:582`, "held a target and cast nothing for N s" — the cast-side twin of `views.stall_windows`)
   and `show_vetoes` (`:627`). Only Yogg has a scorer; the surviving corpus is 13 Yogg, 4 Vezax,
   5 Iron Assembly and 2 unnamed.
5. **The recorder still writes more than anything reads, now at field level.** `snap.u[8]`
   (isMoving) and `u[9]` (movement generator) have **zero readers** at 4 Hz per unit. `u[11]`
   (cumulative damage dealt) is maintained by `AccrueDamageDealt` — pet attribution and all — solely
   to be checked by one invariant, and never reported: there is no throughput metric anywhere.
   `dmg.sc`/`ab`/`rs` ride the highest-volume record unread. 124 probe keys are declared; 4 are read
   by name.

### Defects found by running the tools

- **Two full pulls are unreachable.** `603_4_ulduar_1789323574` and `_1789323949` (33k and 35k lines)
  carry no rename record, stay filed under the map name, are invisible to `--boss`, and their
  coverage folds every node into "gate shut this pull" — 342 of them. The documented mitigation is to
  remember to name the pull.
- **`--valid` returns nothing.** 0 of 13 on Yogg, and 0 of 55 on Gluth before you cleaned it. All
  three decidable kinds fire near-universally, which is the vacuity Tier 2 diagnosed for
  `human-in-raid` and Tier 3 fixed once, back in three new forms.
- **`flame_leviathan.py:142` raises `IndexError` on any pre-v8 trace** — `row[7]` with no length
  guard, inside `Frame.__init__`, so all five FL views die. `yogg_saron.py` guards the identical
  column three times.
- **`yogg_saron.py:690` is always 0** — it counts tentacle deaths from `death` records, which are
  roster-only (`RaidObsCombat.cpp:304`). Same class as the `fl.station` bug.
- **`--split-at` mixes noise into the findings** — `apply oil <-> clean quest log` and
  `arcane blast <-> arcane missiles` rank alongside the raid streams, and `flip.yogg.phase` is called
  *moved* on 0.14 vs 0.13 because the printed ranges round to disjoint.
- **34 tests, zero renderers.** 41 printing entry points, none covered. Nothing runs the suite: no
  runner script, no CI job, no git hook, and it is absent from the `CLAUDE.local.md` close-out. That
  is the structural reason the `show_validity` `NameError` shipped. (An exhaustive scope scan found
  no other undefined-name bug, so that class is now clean.)

---

## 4.1 A geometry spine (no build)

New `tools/botobs/geometry.py`. One owner for what is currently copied:

- `at(trace, guid, t, policy)` — position at a time, with the three policies named rather than
  re-chosen per call site; replaces the six implementations.
- `dist2` / `dist3`, `radius(point, anchor)`, and `edge(point, circles)` — promote
  `flame_leviathan.py:511`, the signed distance to the nearest circle's edge and the best primitive
  in either scorer.
- `nearest(point, candidates)` — replaces 7 open-coded `min(..., key=...)` sites.
- `track(trace, guids, cols)` — one builder for the six, emitting the shape
  `analysis.position_runs` already consumes.
- `anchors()` / `radii()` — parse `const Position NAME = Position(x, y, z)` and
  `constexpr float NAME = Nf` out of `src/Ai/Raid/**`, `lru_cache`d, the way `probes.declared_keys`
  parses probe declarations. **191 anchors and 364 radii are available today, no build**, so a view
  can take `--from ULDUAR_YOGG_SARON_MIDDLE --band ULDUAR_YOGG_SARON_P1_LEASH` by name.

Then delete the duplicates rather than leaving them beside the spine: `yogg_saron.py`'s
`phase_spans`, `missing_probes`, `median`, `position_at`, `tracks`, and its dead `P1_LEASH` /
`BRAIN_LEVEL_Z` constants.

## 4.2 The generic views, lifted out of the scorers (no build)

Promote to shared modules, parameterised by anchor, creature entry or spell:

- **`--threat`** — time-weighted share of who a hostile set was targeting, by role, plus taunt and
  redirect casts ranked against the living candidates by health. From `yogg_saron.show_threat`.
- **`--idle`** — held a target and cast nothing for N s. From `show_frozen`.
- **`--vetoes`** — counter over `trace.of("veto")` by (multiplier, action). From `show_vetoes`.

And the two the Yogg investigation needed and had to hand-write:

- **`--moves [ACTION] --from ANCHOR`** — per mover: count, median origin radius, median destination
  radius, share that ended further out, share that ended inside a named band. The `move` record
  carries **destination only** (`RaidObsEngine.cpp:354-370`), so origin comes from a snap join — the
  join the throwaway script did by hand. This is what turns Tier 3's "`move.by` churns 2,396 times"
  into "957 `flee` moves, 96.4% ended further from the band".
- **`--where EVENT --from ANCHOR`** — radius histogram for deaths, spawns or casts against a named
  anchor, producing the "13 Guardian deaths, 4 outside 15 yd" table for any fight.

## 4.3 Selection and comparison stop lying (no build)

- **Recover an unnamed pull.** Where no rename record exists, resolve the encounter from `unit.en`
  against the boss entries in the raid tree, so those two 35k-line traces stop being invisible to
  `--boss` and their coverage stops folding 342 nodes into "gate shut this pull".
- **Rank the comparison.** Separate raid streams from class-rotation noise in `--split-at`, and stop
  calling a metric moved when the separation is below the printed precision.

## 4.4 Re-aim the disqualifiers (no build)

- `stale-build` compares against HEAD, and the loop commits after every pull, so every trace is stale
  by construction. Compare against the commit under test, and report a trace's position in history
  rather than a verdict against a moving HEAD.
- `hardmode-off` fires on every normal-mode pull of a hard-mode-capable boss. Disqualify only when
  hard mode is what is being tested.
- `human-role` correctly fires whenever you play your own character, and belongs in the same
  "what is this pull evidence *about*" frame rather than a bare reject.

## 4.5 Tests that actually run (no build)

- Smoke-test all 41 printing entry points against the fixture — the exact class of defect that
  shipped.
- Thicken `fixtures/coverage-v12.ndjson`: it holds only `hdr`/`pull`/`snap`/`covdef`/`cov`/`end`, so
  roughly 10 of the 16 `verify_checks` iterate empty lists and pass trivially.
- Fix `flame_leviathan.py:142` and `yogg_saron.py:690`, with a test each.
- Add the suite to the `CLAUDE.local.md` close-out beside pblint. That is a governing doc, so
  `/compact-docs-writer` opens the cycle before the edit, not after.

## 4.6 The C++ half (needs your build)

- **Emit the anchors.** Each encounter writes its named anchors and radii into the header once per
  pull, making 4.1's parse the fallback rather than the source.
- **Stop writing the dead columns.** `snap.u[8]`/`u[9]` at 4 Hz per unit and `dmg.sc`/`ab`/`rs` on
  the highest-volume record. `u[11]` either becomes a real throughput metric or goes, and
  `AccrueDamageDealt` goes with it.
- **Discard a session that never engaged.** `CloseSession` (`RaidObsLifecycle.cpp:186`) has no such
  rule, which is what produced the 55 three-second Gluth files you cleaned by hand.
- **`PerfMonitor::start` takes `std::string const name` by value** (`PerfMonitor.h:68`) while
  `trigger->getName()` returns by value, and the `perfMonEnabled` early-out sits inside the callee
  (`PerfMonitor.cpp:21-22`) — two string constructions per trigger and per action, every tick, with
  the monitor off. `Engine/Engine.cpp:273` and `:627`.

---

## Files

| Purpose | Path |
|---|---|
| Geometry spine | `tools/botobs/geometry.py` (new) |
| Generic views | `tools/botobs/views.py`, `probes.py`, `postmortem.py` |
| Scorers onto the spine | `tools/botobs/yogg_saron.py`, `flame_leviathan.py` |
| Recovery and ranking | `tools/botobs/batch.py`, `metrics.py`, `obstrace.py` |
| Disqualifiers | `tools/botobs/validity.py` |
| Tests | `tools/botobs/tests/`, `tools/botobs/fixtures/` |
| Recorder | `src/Bot/Obs/RaidObsSnapshot.cpp`, `RaidObsCombat.cpp`, `RaidObsLifecycle.cpp` |
| Anchors, perf | `src/Ai/Raid/Uld/Util/*`, `src/Bot/Debug/PerfMonitor.h`, `src/Bot/Engine/Engine.cpp` |
| Docs | `docs/systems/observability.md`, `docs/engine/pitfalls.md`, `CLAUDE.local.md` |

## Verification

Each item must **reproduce** a known result, not merely run clean. The Yogg numbers below are the
throwaway script's, from `603_4_yogg-saron_1789498092`:

- **4.2** — `--threat` gives 35.2% tank time and 6 of 13 taunts on the focus Guardian.
  `--where death --from ULDUAR_YOGG_SARON_MIDDLE` gives 13 deaths, 4 outside 15 yd.
  `--moves flee` gives 957 moves, median destination 24.9, 96.4% further out. All from the CLI.
- **4.2 generality** — `--threat` produces a sensible table on a Vezax and an Iron Assembly trace
  with no Yogg knowledge in it.
- **4.3** — `--boss yogg-saron` reaches the two `ulduar` traces, and `--coverage` on them stops
  folding every node away. `--split-at` no longer ranks `apply oil <-> clean quest log` beside the
  raid streams, and no longer calls `flip.yogg.phase` moved on 0.14 against 0.13.
- **4.4/4.5** — all 24 traces read the same invariant results as before. Renderer smoke tests fail
  against a deliberately broken renderer, as the Tier 3 corrupted-fixture tests do.
- **4.6** — `pb-syntax-check.sh` on every changed `.cpp` plus the `.cpp` including a changed `.h`,
  scoped `pblint.py`, whole-tree `codestyle-cpp.py`. Then on your next pull: the header carries the
  encounter's anchors and no sub-engagement file is written. `fl.frozen` stays open from Tier 3
  until the next Flame Leviathan pull.

## Not in this cycle

- The 7 shipped plan directories still in `docs/plans/`, against `docs/README.md`'s own rule — you
  chose to leave them.
- `UldEncounterGate` port to the other 21 raids (~176 `AI_VALUE2("find target")` sites at check
  interval 1); Ulduar's 54 ungated multipliers; 282 boilerplate multiplier classes; stale counts at
  `UldEncounterGate.h:36,60` and `.cpp:90`.
- `probes.py`'s docstring says 96 probe keys; it is now 124.
