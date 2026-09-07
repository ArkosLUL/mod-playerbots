# Thorim: stop the disperse leak, and pull the phase 2 camp off the melee ring

Traces, all 25-man hard mode (Sif joins in every one), read with
`python modules/mod-playerbots/tools/botobs/postmortem.py <file>` from `modules/mod-playerbots`.
**Timestamps are milliseconds.**

| file | outcome | phase 2 |
|---|---|---|
| `603_1_thorim_1788809246.ndjson` | wipe 5:02, 28 dead | 3:09-5:02 |
| `603_1_thorim_1788809679.ndjson` | wipe 4:44, 26 dead | 3:15-4:44 |
| `603_1_thorim_1788810082.ndjson` | wipe 4:33, 7 dead + 23 to a wipe command | 3:34-4:33 |

Baselines: `603_1_thorim_1788557437.ndjson` (the 6:31 kill, 3 dead) and
`603_1_thorim_1788727248.ndjson` (last night, the old camp).

Build carries `15daf5648` (confirmed: bots walk to `(2131, -284)` and `(2136, -271)`, which only
exist in that commit) plus the `de0ccf468` test-staging merge.

---

## Context

### The camp move did what it was for

Lightning Charge 62466, as a share of all phase 2 incoming: **19.1% last night, 3.4% and 3.1%
tonight**, and it does not appear in the top nine at all in the first attempt. That part is settled
and nothing below undoes it.

### Question 1: yes, arena dps is down, and it is not a Thorim change

`UldActions_Mimiron.cpp:197` does `SET_AI_VALUE(float, "disperse distance", 6.0f)` and `:447` sets
4.0. **Nothing ever clears it.** Mimiron is the only thing in the module that writes the value, it
has no reset node (Thorim, Vezax, Algalon and Iron Assembly all have one), and `RESET_AI_VALUE` for
it exists only in the `disperse disable` chat command. The raid ran three Mimiron pulls at 21:49,
22:07 and 22:13, then Thorim at 22:32.

So every bot walked into Thorim with the generic spread mover already armed:

| | kill | last night | tonight |
|---|---|---|---|
| `combat formation move` in phase 1 | **0** | **0** | 586 / 414 / 439 |
| same, phase 2 | **0** | **0** | 563 / 267 / 385 |
| first one fires at | - | - | 39 ms / 307 ms / 686 ms into the pull |

It is armed before the pull begins, which is the proof it was not set by anything in this encounter.

What it costs, in yards walked per minute (share of snapshots moving):

| | kill | last night | tonight |
|---|---|---|---|
| phase 1 ranged | 66 (16%) | 105 (26%) | 134 / 116 / 117 (~31%) |
| phase 1 heal | 78 (20%) | 100 (25%) | 135 / 122 / 118 (~32%) |
| phase 2 ranged | 27 (7%) | 74 (19%) | **277 / 185 / 261 (50-75%)** |
| phase 2 heal | 41 (10%) | 90 (25%) | **168 / 223 / 254 (45-69%)** |

A moving bot casts nothing. Cast counts per bot-second are flat (2.96 to 3.35 across all six
traces), so this is uptime lost to walking, not a rotation change. Downstream, phase 1 incoming
**from arena adds** is 630,728 over 205 s in the kill run (3,077/s) against 1,192,708 / 995,401 /
1,259,199 tonight (**5,105 to 6,310/s**) - adds living longer, exactly as observed. They then
survive into phase 2: Dark Rune damage is 23% of phase 2 incoming in the second attempt and 44% in
the third, against 1.7% last night.

Spawn counts are script-driven and identical (Commoner 97-112, Warbringer 26-32, Champion 17-20,
Evoker 14-16 in every trace), so this is kill speed, not spawn rate.

The first attempt also carried `mimiron.slot` and `ironassembly.*` notes into the Thorim trace -
same class of leak, different values, and worth a look afterwards but not in scope here.

### Question 2: yes, melee and ranged are too close, and `15daf5648` did that

`spell_jump_distance` has `(64390, 5)` - Chain Lightning daisy-chains, each hop 5 yd or less. So a
chain can only cross from the melee pile to the ranged camp if some pair bridges the gap.

Distance from each melee or tank to the nearest ranged or healer, sampled every 5 s through phase 2:

| | median | share under 5 yd |
|---|---|---|
| kill run | **19.9 yd** | 8% |
| last night | 12.8 yd | 18% |
| tonight | 8.9 / 6.9 yd | 16% / **41%** |

And the chains change shape with it. Kill run casts are role-pure - `[hrrrr]`, `[hmmmmm]` - because
the two groups were 20 yd apart. Tonight: `[hhmmmmmm]`, `[hmmmmrrr]`, `[hhhrrrrr]`, eight victims a
cast, MST links 7.3 to 7.8 yd. Chain Lightning went from 8.9% of phase 2 incoming to **20.4% /
11.2% / 38.5%**, and it killed four of the seven dead in the third attempt (21,955 to 42,387 a hit).

Cause: the shipped anchors sit 14.8-26.0 yd from the boss where the old ones sat 19.1-33.1, while
the melee ring is radius 8. Measured, ranged sat 19-22 yd from the tank spot tonight against 28-29
in both baselines, and they hold their anchors (median 2-3 yd off), so this is the anchor set, not
drift.

### On the tank coordinates

The proposal was **(2110.7483, -252.65265)**. Re-probed tonight, and two things I said about it
before were wrong:

- The melee ring at radius 8 around it is **8/8 on mesh** (the 135 and 180 degree points sit on rim
  polys, distance-to-poly 0.64 and 0.86, settled Z 420.146). My "off the floor" reading came from a
  radius-34 disc model, not from the mesh.
- The Blizzard objection was overstated. Measured as the share of the bunny's walked path that
  passes within 15 yd of any point on the melee ring: **38.5% there, against 32.1% at (2125, -260),
  33.5% at today's (2122, -263) and 34.4% at (2118, -263)**. The bunny loops the whole room, so
  every parking spot is swept; the min-clearance number I quoted is not what the melee actually eat.

It also gives the best separation of anything on the floor: **15.1 yd** from the melee ring to the
nearest ranged anchor, against 14.0 at (2125, -260) and 12.6 at today's spot. The cost is cone
coverage - from there the five-anchor camp averages 1.6 of 7 cones instead of 1.0, so Lightning
Charge roughly goes 3% -> 5% of incoming. That is a good trade against Chain Lightning at 20-38%.

**Use the proposed coordinates.**

---

## The change

### 1. Mimiron must not leak `disperse distance`

New `MimironResetEncounterStateTrigger` / `MimironResetEncounterStateAction` pair, modelled on
`VezaxResetEncounterStateTrigger` (`UldTriggers_Vezax.cpp:24`) and its action
(`UldActions_Vezax.cpp:34`):

- **Trigger:** bot is on `ULDUAR_MAP_ID`, `AI_VALUE(float, "disperse distance") >= 0.0f` (the
  `DisperseDistanceValue` default is -1.0, `src/Bot/Engine/Value/Value.h:359`), and no live
  `NPC_LEVIATHAN_MKII` / `NPC_VX001` / `NPC_AERIAL_COMMAND_UNIT` in `"possible targets"` - the same
  scan `MimironPhase1PositioningTrigger` already does at `UldTriggers_Mimiron.cpp:49`.
- **Action:** `RESET_AI_VALUE(float, "disperse distance")`, return false so the tick still runs.
- **Wiring:** `UldStrategy.cpp` beside the other reset nodes (see `:667` for Vezax) at
  `ACTION_EMERGENCY + 10`, plus creators in `UldActionContext.h` and `UldTriggerContext.h`.

Sight distance is 100 yd, so at Thorim no Mimiron NPC is visible and the reset fires on the first
tick. Inside the Mimiron room the two setters and this reset can trade for a tick between waves,
which is harmless - the positioning triggers already re-set on the next tick.

### 2. Phase 2 camp, rebuilt on the proposed tank spot

`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp:59-68` - eleven `Position` constants and the comment
blocks above them in the `.h`. No logic changes.

Every coordinate below is navprobe-verified on map 603 (`docker compose --profile tools run --rm
ac-navprobe --map 603 point X Y Z` from the repo root, `MSYS_NO_PATHCONV=1` on Git Bash). Z is
`UpdateAllowedPositionZ`; distance-to-poly is in the last column.

| constant | x | y | z | poly |
|---|---|---|---|---|
| `PHASE2_TANK_SPOT` | 2110.7483 | -252.65265 | 419.440 | 0.060 |
| `PHASE2_OFFTANK_SPOT` | 2115.0 | -247.0 | 419.458 | 0.022 |
| `PHASE2_MELEE1_SPOT` | 2118.75 | -252.65 | 419.596 | 0.060 |
| `PHASE2_MELEE2_SPOT` | 2110.75 | -244.65 | 419.359 | 0.060 |
| `PHASE2_MELEE3_SPOT` | 2110.75 | -260.65 | 419.485 | 0.012 |
| `PHASE2_RANGE1_SPOT` | 2127.0 | -269.0 | 419.789 | 0.085 |
| `PHASE2_RANGE2_SPOT` | 2132.0 | -262.0 | 419.846 | 0.235 |
| `PHASE2_RANGE3_SPOT` | 2135.0 | -270.0 | 419.845 | 0.230 |
| `PHASE2_RANGE4_SPOT` | 2140.0 | -259.0 | 419.847 | 0.168 |
| `PHASE2_RANGE5_SPOT` | 2125.0 | -279.0 | 419.603 | 0.061 |

Ring check: `ring 2110.7483 -252.65265 419.44 8 8` returns 8/8 on mesh, settled Z 419.359-420.146.

Ranged geometry, against the melee ring at radius 8 around the tank spot:

| slot | from boss | gap to the ring | cones | Blizzard | from arena centre |
|---|---|---|---|---|---|
| RANGE1 | 23.1 | 15.1 | 1 | 20.9 | 9.9 |
| RANGE2 | 23.2 | 15.2 | 2 | 26.8 | 3.2 |
| RANGE3 | 29.8 | 21.8 | 1 | 27.7 | 6.9 |
| RANGE4 | 29.9 | 21.9 | 2 | 22.0 | 6.5 |
| RANGE5 | 30.0 | 22.0 | 2 | 16.3 | 18.8 |

Tightest ranged pair 8.06 yd, clear of the 5 yd jump. All five within 30 yd of the boss, all 3-19 yd
from the arena centre, none near the `y = -288` hole - safer than the shipped set, which put three
anchors at `y = -284`.

The melee trio are the fallback `StaticMeleeSpot` takes when `RingPoint` fails, and it is gated on
`boss->GetDistance(spot) <= ULDUAR_THORIM_MELEE_RING_RADIUS + ULDUAR_THORIM_RING_ARRIVE_TOLERANCE`
(11 yd), so they have to move with the tank spot or they can never be taken. The three above are on
the 8 yd ring at 0, 90 and 270 degrees.

The off-tank sits 7.1 yd from the tank on the far side from the camp, so the taunt swap stays in
range without putting a second body on the ranged bearing.

Rewrite the comment blocks in `UldEncounter_Thorim.h:233-259`. The current text claims every anchor
is on exactly one cone and cites Lightning Charge at 19% of incoming - both stop being true here.
Say instead that the camp is placed against Chain Lightning's 5 yd jump, that the boss is parked far
enough west that the melee pile and the ranged camp cannot bridge, and that one or two cones is the
price.

The arena leash does not need touching: `ThorimArenaLeashBreached` returns early unless
`ThorimSplitActive`, which is phase 1 only, so the 30 yd radius never sees the phase 2 ring.

## Deliberately not doing

- **Nothing about the number of ranged anchors.** Fourteen ranged and healers over five slots is
  2-3 deep, and they wrap correctly already (`% ULDUAR_THORIM_RANGED_SLOTS` at
  `UldEncounter_Thorim.cpp:1740`; the `std::min` in `RangedSpot` is a dead bound). A same-anchor pair
  is always inside 5 yd, so chains of 2-3 remain - but they cannot grow past that once the groups
  are 15 yd apart. Six anchors is possible from here and costs 1 yd of gap; not worth it yet.
- **Nothing about pets.** Stormweaver's death has a Fire Elemental, a Worm and three Treants at
  4.2-4.3 yd, all inside the jump range, so pets can relay a chain across a gap the bots respect.
  Worth a look if 8-victim casts survive this change.
- **Nothing about Sif.** Frostbolt Volley is DBC radius 200 with no positional answer.
- **No ranged cone dodge.** Still superseded.

## Verification

Static:

- `grep -n "PHASE2_TANK_SPOT\|PHASE2_OFFTANK_SPOT\|PHASE2_MELEE\|PHASE2_RANGE" src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`
  shows the eleven new values and nothing else changed.
- `python apps/codestyle/codestyle-cpp.py` passes, no line over 120 columns, files stay LF.
- Per-TU syntax check in `acore/ac-wotlk-build:master` against `/azerothcore/build/compile_commands.json`
  for both changed `.cpp` files: mount `modules/mod-playerbots/src` read-only, take the entry for the
  file, drop `-c` and `-o`, add `-fsyntax-only`, run from its `directory`. Expect only the two
  pre-existing `-Wunused-parameter` warnings in `UldEncounter_Thorim.cpp`. It does not link.

In game, one 25-man hard-mode pull **immediately after a Mimiron pull**, so the leak fix is actually
exercised, then re-read the trace:

1. **`combat formation move` is back to zero** in both phases. This is the headline and it is a
   binary check.
2. **Phase 2 ranged and healer travel back under ~80 yd/min** from tonight's 168-277.
3. **Phase 1 incoming from arena adds back near 3,000/s** from tonight's 5,105-6,310.
4. **Chain Lightning back near 9% of phase 2 incoming** from 20-38%, and no cast mixing `m` and `r`
   roles in its victim list.
5. **Melee to nearest ranged median back near 15-20 yd**, share under 5 yd in single digits.
6. **Lightning Charge stays under about 6%** of phase 2 incoming. It was 3.1-3.4% tonight and going
   to 1.6 cones should roughly halve the margin, not undo the win.
7. **Regressions.** Thorim actually parks at (2110.7, -252.7); nobody on the arena floor below
   `y = -288` or with Z near -27.7; Paralytic Field stays at zero; Blizzard damage stays small;
   every gauntlet bot still emits `thorim.balcony` from step 0; every melee still gets a fresh
   `thorim.slot`.
