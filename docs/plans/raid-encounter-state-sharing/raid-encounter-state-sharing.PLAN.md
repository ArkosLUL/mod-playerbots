# Share the raid encounter state across map threads, and spread the Thorim camp out of the cone

Traces read with `python tools/botobs/postmortem.py <file>` from `modules/mod-playerbots`; files live in
`env/dist/logs/botobs/`. **Timestamps are milliseconds.** Scratchpad scripts behind every number here:
`lib.py` (loader, needs `G:/` paths not `/g/`), `sum.py`, `roster.py`, `ring2.py`, `bliz.py`, `deaths.py`,
`tank2.py`, `tankdmg.py`, `lc.py`, `cone.py`, `orbs.py`, `undo.py`, `per.py`, `dbc.py`, `hzdump.py`,
`loop.py`, `feas.py`, `anchor.py`, `solve.py`.

## Context

Two pulls on 9 Sep 2026 verifying `afdf9e665`, 25-man roster. RaidObs sees **one** tank (Bulwark,
protection paladin); Ecoterrorist is now feral cat and reports `melee`; Dragon and Deathsong are humans.

| | pull D `603_1_thorim_1788984793` | pull E `603_1_thorim_1788985301` |
|---|---|---|
| phase 2 | 2:43.0 -> 4:12.9 (1.5 min) | 3:03.5 -> 5:30.6 (2.45 min) |
| boss HP at wipe | 79.8% | 51.3% |
| phase 2 deaths | 26 | 31 |
| Thorim off anchor | median 8.0, p90 21.5 | median 4.8, p90 27.1 |
| tank movement | 108 yd/min, 36% moving | 19 yd/min, 6% moving |
| melee movement | 178 yd/min, 45% moving | 255 yd/min, 62% moving |

**`afdf9e665` did what it claimed.** `thorim.p2role` emitted 23 notes, all at 0:00, and never changed:
Bulwark resolved `maintank` and held it. Thorim reached the anchor in 10.4 s / 10.7 s against 25 s, median
off-anchor 16.0 -> 8.0 / 4.8. No `thorim sif blizzard action` move by the tank. Ranged sit on the six new
spots (median 0.0 yd from the nearest), no Chain Lightning burst reached 8 targets in pull E, Chain
Lightning fell 359k/536k -> 247k/383k, largest 8 yd group in pull D median 11 -> 6.

**Why they died.** Pull D was Lightning Charge: 12 of 26 fatal blows, 320k, and one burst at 3:28.620 hit
13 people for 202k and killed 10 inside two seconds. Pull E was the tank dying at 5:05 with the boss at
52%, after which Thorim ate 9 ranged, 3 melee and a healer in 25 s (15 of 31 fatal blows are plain melee).
Bulwark took 500k in that phase: 268k melee, 102k Blizzard, 39k Unbalancing Strike, and fell from 100% to
dead in 9 s. Sif is 40-42% of all phase 2 damage taken (Frostbolt Volley alone 1.12M in pull E) with no
positional answer. Thorim spent 20% of pull D and 35% of pull E attacking the human Dragon.

### The root cause: six copies of every latch

Melee oscillation got worse, not better. The `thorim.ringspot` probe shipped in `afdf9e665` answered why -
**the slot flips, not the latch.** Obliteration cycled slot 0/1/2 246 times in 90 s and the "latched"
bearing re-struck with it (239/244/224/254 for the same slot, i.e. `RingAnchorBearing` recomputed off a
boss that had drifted).

`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp:78` declares the state `thread_local`, commented *"a bot is
only ever updated from its own map's thread, so this needs no lock"*. The premise is false in the second
half: `MapUpdater::schedule_update` (core, `src/server/game/Maps/MapUpdater.cpp:141`) pushes each map onto
a shared queue with no affinity, and the effective thread count is **`AC_MAP_UPDATE_THREADS=6`** - the
gitignored `configurationOverrides/*.env` override beating `MapUpdate.Threads = 1` in `worldserver.conf`.
So there are six copies of the state and a bot reads a different one each tick.

The proof is exact: `thorim.slot` emits **6 notes per bot for all 8 melee bots** (48 in pull D, 47 in E),
and `thorim.squad` 6 per bot for 23 of 25. One write per worker thread that ever ticked the pull.

This corrupts every per-bot latch in the encounter, not just the ring: `ringBearings`, `ringArrived`,
`squads`, `balconyStep`, `arenaAnchorArrived`, `orbEscapes`, `dpsTargets`, `barrierBailing`,
`followMasterStripped`, `petRecalls`, `resetDone`. Every ring, oscillation and hazard-cadence measurement
taken before this fix was measured against noise.

**The fix already exists in the tree three times** - `UldEncounter_FlameLeviathan.cpp:72`,
`UldEncounter_IronAssembly.cpp:65`, `UldEncounter_Mimiron.cpp:364` all carry *"Not thread_local. A map is
updated by one thread at a time but is never pinned to one, and MapUpdate.Threads is 6 here..."* with a
mutex plus accessor. IronAssembly's comment even records the same trace signature: *"six identical
ironassembly.alive rows per transition, one per thread that ever ticked the pull."*

### Lightning Charge, measured

`62466` verified end to end: effect 2 is school damage, `ImplicitTargetA = 104`
(`TARGET_UNIT_CONE_ENEMY_104`, `TARGET_SELECT_CATEGORY_CONE`, `TARGET_DIR_FRONT` per core
`SpellInfo.cpp:317`), `EffectRadiusIndex 41` = **150 yd**. `acore_world.spell_cone` overrides the
implicit 104 degrees with **`ConeDegrees = 75`**, and `Position::HasInArc` halves the arc, so it is a
**75 degree frontal cone, 150 yd**. No `spell_linked_spell` rows, no `spell_script_names` row. Cast time 0
(`CastingTimeIndex 1`), damage lands in the same millisecond as the cast, so **there is no reaction
window** - only pre-positioning. It fires on a fixed **15.0 s** timer and always targets a **Thunder Orb**,
of which there are 7 at fixed positions, so the cone bearing is one of 7 known values.

The six current camp spots span only a **70 degree bearing wedge** from the anchor, just inside the 75
degree cone. One cone catches all six: that is the 12 and 13 target bursts.

## What is being changed

### 1. Share the Thorim encounter state (the root cause)

`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`, copying `UldEncounter_IronAssembly.cpp:65-80` and
`:1106-1135` verbatim in shape:

- Drop `thread_local` from line 78, add `std::mutex thorimStatesMutex;` beside it and `#include <mutex>`.
- Add `ThorimEncounterState& ThorimStateFor(Player* bot)` that takes the lock and returns
  `thorimStates[bot->GetInstanceId()]`. References into an `unordered_map` survive rehashing, so the lock
  only has to cover the lookup; the state itself is still only touched by one thread at a time.
- Replace the **15** `thorimStates[bot->GetInstanceId()]` sites with `ThorimStateFor(bot)`.
- `FindState` (`:121`) takes the lock around its `find`. Its 19 call sites need no change.
- `ThorimBotHasEncounterState` (`:1946`) and `ResetThorimEncounterState` (`:1963`) take the lock, matching
  IronAssembly's shape - including the `clearInstance` erase.
- Rewrite the line 76-78 comment to state the real reason, the way the three sibling files do.

### 2. The same port in the five other files that still have it

Same pattern, one mutex per file so contention stays inside the encounter. These are all **latches**, so
all of them are producing wrong answers today:

| file | state | what six copies break |
|---|---|---|
| `Uld/Util/UldEncounter_Hodir.cpp:104` | `latched` (bot -> shelter guid) | shelter assignment flips |
| `Uld/Util/UldEncounter_Hodir.cpp:375` | `latched` (bot -> `StarlightLatch`) | a *position* latch, so oscillation |
| `Uld/Util/UldEncounter_Ignis.cpp:273` | `_ignisTankArcStates` | tank arc slot flips |
| `Uld/Action/UldActions_FlameLeviathan.cpp:34` | `flKiteDirection` | kite sense flips - the very thing its comment says must never happen |
| `EoE/Util/EoEEncounter_Drakes.cpp:37,49` | `stackAngleCache`, `fixateCache` | stack angle and fixate latch flip |
| `EoE/Util/EoEEncounter_Malygos.cpp:67` | `layoutCache` | has a `latched` flag, so it is a latch |

**Convert but note as hygiene, not a bug fix:** `Malygos` `phaseCache`, `creatureCache`, `bossCache` and
`Drakes` `healerRosterCache` are genuine time-bounded caches of live world state - a rebuild on another
thread recomputes the same answer, which is what the Malygos comment at `:56-60` reasons correctly. They
are converted for consistency with the latches sharing their file, and the commit message should say which
half fixes a defect and which is tidying, so a reviewer is not misled.

### 3. Spread the ranged camp out of the cone

Six spots, keeping the tank anchor where it is. Solved against four simultaneous hard constraints: at
least 22 yd from `ULDUAR_THORIM_PHASE2_TANK_SPOT` so the radius 8 melee ring cannot bridge in; at least 12
yd clear of Sif's Blizzard loop; pairwise at least 11 yd for Chain Lightning's 8 yd jump; and minimising
the worst single 75 degree cone drawn from the anchor toward any of the 7 Thunder Orbs.

A fifth constraint went in after the first solve: nothing past 32 yd from the anchor. The unconstrained
best put a spot at 34.0, and most caster nukes reach 30 to 36, while the camp that demonstrably held
ranged on their points topped out at 32. Capping at 32 costs nothing on the cone.

Result: **a single cone catches 4 of 6 instead of 6 of 6**, bearing span 88 degrees, tightest pair 11.6
yd, worst Blizzard clearance 13.0 yd, furthest spot 31.8 yd. Shipped, every z from navprobe:

| # | x | y | z | yd from anchor | bearing | Blizzard clear |
|---|---|---|---|---|---|---|
| 1 | 2123.00 | -282.00 | 419.528 | 31.8 | -67.3 | 17.4 |
| 2 | 2124.50 | -270.50 | 419.729 | 22.5 | -52.4 | 18.3 |
| 3 | 2137.50 | -269.00 | 419.845 | 31.4 | -31.4 | 26.4 |
| 4 | 2132.50 | -257.50 | 419.845 | 22.3 | -12.6 | 23.9 |
| 5 | 2142.00 | -250.50 | 419.691 | 31.3 | +3.9 | 16.4 |
| 6 | 2131.50 | -245.00 | 419.612 | 22.1 | +20.2 | 13.0 |

All six probed `PATHFIND_NORMAL` with poly distance under 0.22 and a path length matching straight line,
so none of them needs a detour off the anchor. If any of this is re-solved, the rule stands: Every one goes through `navprobe` for both point and path from
the anchor before it ships, and the z comes from the probe, not from the table above - per
`modules/mod-playerbots/CLAUDE.md` and `docs/engine/pitfalls.md`. Coming out of a trace proves a body stood
there, not that the point is a good destination. navprobe lives in the **core fork** at `src/tools/navprobe`:

```
MSYS_NO_PATHCONV=1 docker run --rm \
  -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro \
  --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master --map 603 point <x> <y> <z>
```

Drop any failure and re-solve the set with `solve.py` rather than nudging a point by hand - that is how the
last round's two bad layouts happened.

Code side: the `ULDUAR_THORIM_PHASE2_RANGE*_SPOT` block (`UldEncounter_Thorim.cpp:60-66`) and the comment
above `ULDUAR_THORIM_RANGED_SLOTS` in the header, which currently claims a 22 yd tank-spot gap and 16 yd
Blizzard clearance. `ULDUAR_THORIM_RANGED_SLOTS` stays 6, `RangedSpot`'s table needs no reshaping, and the
round-robin in `TryGetThorimPhase2Spot`'s Ranged branch is unchanged.

## Deliberately not doing

- **Moving the tank anchor.** 16 yd east of the current spot opens the feasible camp window from 95 to 190
  degrees and takes the worst cone down to **2 of 6**. It is the only way past 4 of 6, and it is a tank
  positioning change on the pull where tank positioning finally works, so it wants its own round and its
  own trace. Numbers are in `anchor.py` and `solve.py` when we come back to it.
- **A Lightning Charge dodge node.** Cast time is 0 and the damage lands in the same millisecond, so there
  is nothing to react to. Any answer has to be pre-positioning, which is what step 3 is.
- **Melee Blizzard beyond the state fix.** Melee are hit *on their assigned point* - median 6.4-6.9 yd from
  the boss, which is the radius 8 ring - because the bunny's 40-point circuit passes 5.4 yd from the anchor
  and 23% of its track overlaps the ring. No static ring avoids it. The dodge node itself is healthy: 13 and
  24 accepted moves and **zero** undone by a ring move within 3 s, because
  `ThorimPhase2PositioningTrigger` stands down while it is up (`UldTriggers_Thorim.cpp:156`). It is reactive
  (`TooCloseToCreature`), so a bot eats a tick before stepping out, and a bot oscillating across three ring
  slots crosses the bunny far more often than one standing still. Re-measure after step 1 before touching it.
- **The tank's Blizzard damage.** Deliberate and documented at `UldTriggers_Thorim.cpp:338-342`: dodging it
  towed the boss into the ranged camp. 102k measured against the comment's estimate of ~100k a pull. It was
  a fifth of what killed Bulwark, so it is worth revisiting, but not by re-enabling the dodge.
- **Sif.** 40-42% of phase 2 damage taken and the largest single source. Frostbolt Volley is DBC radius 200
  with no positional answer, and Frost Nova already has a dodge node.
- **Thorim attacking the human.** 20% and 35% of the phase. Nothing in this module can move a player or
  out-threat one.

## Verification

Static, from `modules/mod-playerbots`:

- `python apps/codestyle/codestyle-cpp.py` clean for the touched files. Its three standing failures
  (`DBCStructure.h` tabs, `mmaps_generator`) are pre-existing.
- No line over 120 columns; files stay LF and ASCII. Check the encoding in Python reading bytes, not with
  `grep -P` - the locale here rejects it and the pass is then meaningless.
- Per-TU syntax check in `acore/ac-wotlk-build:master` against `/azerothcore/build/compile_commands.json`
  for each changed `.cpp`: take the entry for the file, drop `-c` and `-o`, add `-fsyntax-only`, run from
  its `directory`. Needs `MSYS_NO_PATHCONV=1` and `$(pwd -W)` for the mount source or Git Bash mangles the
  paths. Expect only the two standing `-Wunused-parameter` warnings in `UldEncounter_Thorim.cpp`
  (`:1358`, `:1825`). Takes ~2.5 min, so background it - and do not edit the tree while it runs, the mount
  is live.
- navprobe clean for all six ranged spots, point and path.
- Copy this plan to `docs/plans/raid-encounter-state-sharing/raid-encounter-state-sharing.PLAN.md` as the
  first implementation step.

In game, one 25-man hard-mode Thorim pull, then re-read the trace:

1. **`thorim.slot` emits at most 1-2 notes per bot, not 6.** This single number is the whole state fix.
   Same for `thorim.squad`: 25-ish notes for the pull, not 180-224.
2. **`thorim.ringspot` holds.** One slot per melee bot for the phase, and one bearing to within a couple of
   degrees, against 246 slot changes for one bot in 90 s.
3. **Melee under ~120 yd/min/head and under ~30% of the phase moving**, against 178/45% and 255/62%.
4. **No Chain Lightning burst reaches 8 targets** and largest 8 yd group stays under 8 for most of the
   phase, against >= 8 for 34% (D) and 72% (E) - pull E's figure should fall furthest, since its melee
   churn is what bridges the blob.
5. **No single Lightning Charge burst hits more than ~8**, against 13 and 12, and its share of fatal blows
   drops from 12 of 26.
6. **Melee Blizzard damage falls** from 99k / 195k without touching the dodge node, purely because they
   stop crossing the bunny. If it does not, the reactive trigger is the next thing to look at.
7. **Ranged and healers still take zero Blizzard** and still sit on their spots (median 0.0 yd from the
   nearest), with the camp moved.
8. **No regression** on: `thorim.p2role` still one note per bot at 0:00 and never `melee` for a tank;
   Thorim's median off-anchor still under ~8 yd and reached inside ~11 s; the tank still under ~60 yd/min;
   phase 1 deaths still 0; pickup vacuum still under ~2 s; `combat formation move` still owning 0 moves;
   the squad split still forming; hard mode still won with Sif dealing damage through phase 2; the taunt
   guard still showing `rel 0` on class taunts while a tank holds the boss.
9. **Spot-check the other five encounters** for the same signature rather than a full pull each: any
   `ObsGuidMap`-backed note in Hodir, Ignis, FlameLeviathan or EoE should stop arriving in groups of six.
