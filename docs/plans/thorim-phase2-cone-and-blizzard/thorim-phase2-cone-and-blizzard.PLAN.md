# Thorim phase 2: pre-dodge Lightning Charge, and slide melee around the Blizzard instead of fleeing it

Traces read with `python tools/botobs/postmortem.py <file>` from `modules/mod-playerbots`; files live in
`env/dist/logs/botobs/`. **Timestamps are milliseconds.** Scratchpad scripts behind every number here:
`lib.py`/`core.py` (loaders, need `G:/` paths not `/g/`), `sum2.py`, `deaths.py`, `bliz.py`, `blizrad.py`,
`blizarc.py`, `blizwhere.py`, `flee.py`, `ring.py`, `ringdodge.py`, `cone.py`, `pred.py`, `lc2.py`,
`cover2.py`, `final2.py`, `camp.py`, `slotchurn.py`, `conflict.py`, `wander2.py`, `loop2.py`, `fit.py`,
`anchor.py`, `move2.py`, `mv2.py`, `ringspot.py`, `orbnote.py`, `mdist.py`, `cl.py`.

## Context

Four Thorim pulls on 10 Sep 2026, 25-man hard mode, verifying `b5b636f40` and `cd83b829a`.

| | F `1789061861` 20:43 | G `1789062467` 20:48 | H `1789062701` 20:58 | I `1789067854` 22:22 |
|---|---|---|---|---|
| phase 2 | 3:02.8 -> 4:58.4 (1.93 min) | never reached | 3:08.7 -> 5:53.6 (2.75 min) | 2:48.7 -> 4:41.4 (1.88 min) |
| boss HP at wipe | 61.9% | 100% | 59.0% | 64.0% |
| phase 1 deaths | 0 | 25 | 1 | 0 |
| phase 2 deaths | 30 | 0 | 30 | 31 |

**G is not an attempt.** All 23 bots die inside 0.25 s at 0:44.8 with no damage record: a wipe command.
Ignore it. F, H and I are the data.

**`b5b636f40` did what it claimed.** `thorim.slot` emits 7 notes for a whole pull, not 6 per bot;
`thorim.squad` 26-29 for 25 members, not 180. Melee movement fell from 178/255 yd/min/head and 45%/62% of
the phase moving (pulls D/E, 9 Sep) to **128/66/84 yd/min and 32%/17%/22%**. `thorim.p2role` is stable.
The boss reaches the anchor in 8.9-16.6 s and holds; median off-anchor 4.4/4.8/6.8 yd.

### Why they died

Phase 2 damage taken:

| | F | H | I |
|---|---|---|---|
| Frostbolt Volley 62604 | 27.5% | 22.7% | 25.5% |
| melee (spell 0) | 18.9% | 21.1% | 20.4% |
| **Lightning Charge 62466** | **16.6%** | **12.6%** | **15.7%** |
| Blizzard 62602 | 11.4% | 9.9% | 10.3% |
| Frostbolt 62601 | 9.0% | 11.7% | 8.6% |
| Chain Lightning 64390 | 3.6% | 10.3% | 7.3% |

**Lightning Charge is the top fatal blow in all three, and getting worse: 9 of 30, 11 of 30, 13 of 31.**

- H, 3:54.906: seven dead inside 0.1 s (Trueshot, Smartface, Malediction, Hellflame, Tree, Power,
  Stormweaver). The next cone 15 s later, same orb, killed three more.
- F, 4:33.505: ten hit for 212k, five ranged dead at once.
- I, 3:49.773: eight hit for 148k, five ranged dead at once; the 3:34.676 cone killed three and the
  4:19.729 cone killed three more. **Eleven of the raid's 31 phase 2 deaths came from three cones.**

Sif is 38-42% of phase 2 damage taken and has no positional answer. Chain Lightning's one big burst (H,
4:20.739, 8 targets for 243k) was entirely melee/tank/human - the melee ring, not the camp.

### The Lightning Charge chain, measured end to end

`boss_thorim.cpp` (core, `src/server/scripts/Northrend/Ulduar/Ulduar/`):

1. `EVENT_THORIM_LIGHTNING_CHARGE` -> `CastSpell(me, SPELL_LIGHTNING_PILLAR_P2)` (62976, `:737`).
   `spell_thorim_lightning_pillar_P2` retargets it to a **random pillar bunny** (`NPC_PILLAR`, entry
   32892, "Thorim Event Bunny", 7 of them).
2. `boss_thorim_pillar::SpellHit` (`:1012-1018`) makes the nearest **Thunder Orb** (entry 33378, 7 of
   them, each directly above one pillar at the same x,y) cast `SPELL_LIGHTNING_ORB_VISUAL` (62186) on
   itself.
3. 62186 is a 5000 ms aura whose periodic trigger DBC amplitude is 8000 ms - patched to **5000** by
   `SpellInfoCorrections.cpp:2005`, so it ticks exactly **once, 5 s later**, casting 62278 at Thorim.
4. Thorim's `SpellHit` (`:615-620`) does `SetOrientation(GetAngle(caster))` and casts 62466 at the orb.

62466 is a **75 degree frontal cone** (`acore_world.spell_cone` overrides the implicit 104 degrees;
`Position::HasInArc` halves the arc), 150 yd deep, 0 cast time, damage in the same millisecond.

**Measured over 46 casts across 6 pulls: the lead from the 62976 cast to the damage is 4.91-5.18 s, every
time, and the pillar's x,y is identical to the orb's.** The boss did not move at all in 15 of 17 windows
checked; worst cone-bearing drift over the 5 s was 5.1 degrees, during the opening walk. Thorim's recorded
orientation is *not* usable - he has turned back to his victim by the next snapshot - but the orb is.

**The module already sees this.** `ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL)`
(`UldEncounter_Thorim.cpp:1888`) is a shared per-instance 500 ms grid scan, and `thorim.lightningorb`
fires 0.03-0.59 s after each pillar cast in all three traces. **4.4-5.0 s of usable warning.** It is
already consumed by `LightningChargeOffset` (`:459`) for the melee ring and it works. Ranged and healers
do not consult it.

Static spread cannot fix this: the seven cones leave only a 16-28 degree wedge of bearing permanently
clear at any anchor, nowhere near enough for six spots 11 yd apart. The camp itself is exactly where it is
meant to be - median 0.0 yd from the nearest spot in all three pulls - so this is not a positioning
failure, it is a missing mechanic.

Minimum off-cone angle per (orb, camp spot), taken over the tank spot and every settled boss position in
F, H and I. Under 40 degrees means the spot is in that cone:

| orb | pos | spot1 | spot2 | spot3 | spot4 | spot5 | spot6 |
|---|---|---|---|---|---|---|---|
| 1 | (2145.5, -222.6) | 108.2 | 93.2 | 72.3 | 49.9 | **34.9** | **13.4** |
| 2 | (2164.2, -233.5) | 87.1 | 72.1 | 51.1 | **28.2** | **13.2** | **0.5** |
| 3 | (2164.6, -293.0) | **30.5** | **15.1** | **1.9** | **24.3** | 40.8 | 57.1 |
| 4 | (2105.0, -292.6) | **28.4** | **39.4** | 66.7 | 85.6 | 102.1 | 118.4 |
| 5 | (2093.0, -263.0) | 79.2 | 90.3 | 118.4 | 137.3 | 153.8 | 162.1 |
| 6 | (2104.9, -233.4) | 157.4 | 159.1 | 136.3 | 113.3 | 98.3 | 79.0 |
| 7 | (2124.3, -222.6) | 133.1 | 118.1 | 97.2 | 75.1 | 60.2 | **40.8** |

Twelve pairs across five orbs. Orb 3 - the south-east one, straight through the camp - covers four spots
and is behind the mass-kill casts in H and I. Because bots stack 2.5-2.9 deep on spots 1 and 2, even
orb 4's single covered spot killed three people in I at 3:34.676.

### Why melee eat Blizzard, and why the current dodge makes it worse

Ranged and healers take **zero**. Every point lands on melee (54.9-59.8%), the tank (18.2-34.9%) and a
human (8.5-26.8%).

Sif's Blizzard bunny walks a **fixed world-space circuit** - distance-to-arena-centre statistics identical
across pulls (mean 28.8-29.1, sd 7.9, min 19.6, max 42.7), while the spread relative to the boss or to Sif
is twice as loose. It never comes within 19.6 yd of the arena centre (2126.0, -256.35), but
`ULDUAR_THORIM_PHASE2_TANK_SPOT` is 15.5 yd off-centre and the circuit passes **5.3 yd** from it. The
radius-8 melee ring sits 2.7 yd *inside* the track, and a Blizzard overlaps it 43-50% of the time one
exists, with up to six bunnies at once.

The dodge mostly cannot act: **161 of 199 attempts in F, 204 of 246 in H are refused by the movement
gate** (`IsWaitingForLastMove`, `MovementActions.cpp:1037`). Worse, the ones that *do* land are
actively harmful. `MoveAwayFromCreatureAction` (`MovementActions.cpp:2925`) scores eight compass rays out
to 30 yd and takes the point **furthest** from the bunny, so every accepted melee flee measured:

| | accepted melee flees | run asked for (median) | goal distance from the boss (median / max) | took another Blizzard tick within 6 s |
|---|---|---|---|---|
| F | 26 | 30.0 yd | 34.8 / 41.3 | 15 of 26 |
| H | 21 | 30.0 yd | 34.6 / 47.6 | 6 of 21 |
| I | 17 | 30.0 yd | 32.7 / 52.9 | 6 of 17 |

It always takes the outermost ray, dumps a melee bot 33-53 yd from the boss, and fails to avoid the
Blizzard 23-58% of the time anyway - because the circuit is a ring, so running outward can land on a
different arc of it. It also blocks every other move for the length of that 30 yd walk, which is what
`--stalls` reports: Ecoterrorist stationary 31.7 s and Totemist 15.0 s in pull I, both "wanted by: thorim
sif blizzard action, furthest goal 30 yd".

That is why **half the melee Blizzard damage is taken away from the ring**: 34% in F, 56% in H, 58% in I
comes from melee more than 12 yd from the boss. The flee creates its own exposure.

The effective Blizzard reach is **9.8 yd**, not 8: over 161 ticks the furthest hit was 9.8, p99 9.8,
nothing past 10. That is the corrected DBC radius of 8 plus both combat reaches through
`IsWithinDistInMap`. `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS = 15.0` is the generic dodge's trigger radius, not
the damage radius.

**A ring rotation always exists.** Sampling the radius-8 ring every 10 degrees across 890 snapshots with a
Blizzard up, the ring was **never** fully covered - the clear fraction bottoms out at 53%. At an 11 yd
clearance the arc to the nearest clear bearing is a median 3-7 yd, p90 6-10, max 15.1, and **blocked zero
times** in any pull.

## What is being changed

### 1. Lightning Charge shelter for ranged and healers

The camp keeps its six home spots. While an orb is lit, a bot whose home spot is inside that orb's cone
stands on a precomputed shelter instead.

**Two thresholds, deliberately different from the melee pair.** The melee ring uses
`ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN = 15 degrees` on top of the 37.5 half-arc because at 8 yd from the
boss a 1 yd shift swings the bearing 7 degrees. A ranged bot at 15-32 yd sees 2-4 degrees for the same
shift. So:

- **in-cone test: 40 degrees** (37.5 + 2.5), evaluated live off the boss's actual position.
- **shelter placement: 42.5 degrees**, and required to hold at *every* settled boss position seen in F, H
  and I - not just the median. A bot standing on its shelter therefore reads at least 2.5 degrees outside
  the test that sent it there, so the pair cannot chatter.

Add to `UldEncounter_Thorim.h`: `ULDUAR_THORIM_LIGHTNING_CHARGE_RANGED_MARGIN = 0.0436f` (2.5 degrees),
the seven `NPC_THORIM_THUNDER_ORB` reference positions, and the shelter table.

**The shelter table.** Solved on a 0.5 yd grid (`final2.py`) against: at least 42.5 degrees off the cone
bearing from every settled boss origin; at least 22 yd from `ULDUAR_THORIM_PHASE2_TANK_SPOT` so the
radius-8 melee ring cannot bridge Chain Lightning in; at most 32 yd from the boss so the shorter caster
nukes still reach; at least 9 yd from every other simultaneously occupied point (Chain Lightning's jump is
8.0 centre to centre); at most 26 yd of run; Blizzard-track clearance preferred. All twelve are navprobed,
point and path - `PATHFIND_NORMAL`, poly distance <= 0.69, path length equal to the straight-line
distance, z taken from the probe:

| lit orb | slot | shelter | run | to tank | to boss | off-cone | track |
|---|---|---|---|---|---|---|---|
| 1 (2145.5, -222.6) | 5 | (2142.00, -254.85, 419.814) | 4.3 | 31.3 | 24-30 | 42.6 | 19.8 |
| 1 | 6 | (2113.00, -227.35, 420.293) | 25.6 | 25.4 | 26-30 | 43.2 | 12.0 |
| 2 (2164.2, -233.5) | 4 | (2132.00, -276.35, 419.755) | 18.9 | 31.8 | 24-30 | 67.9 | 25.1 |
| 2 | 5 | (2130.50, -262.85, 419.905) | 16.9 | 22.2 | 14-20 | 43.5 | 26.7 |
| 2 | 6 | (2119.50, -225.85, 420.293) | 22.6 | 28.2 | 28-31 | 52.2 | 12.0 |
| 3 (2164.6, -293.0) | 1 | (2116.50, -283.35, 419.509) | 6.6 | 31.2 | 27-30 | 42.5 | 12.5 |
| 3 | 2 | (2107.50, -283.35, 420.104) | 21.3 | 30.9 | 27-32 | 59.2 | 8.0 |
| 3 | 3 | (2114.50, -274.35, 419.562) | 23.6 | 22.0 | 18-21 | 43.3 | 7.7 |
| 3 | 4 | (2140.00, -241.35, 419.502) | 17.8 | 31.4 | 25-31 | 58.0 | 8.2 |
| 4 (2105.0, -292.6) | 1 | (2129.50, -278.35, 419.702) | 7.5 | 31.8 | 25-30 | 43.2 | 22.8 |
| 4 | 2 | (2125.50, -269.85, 419.755) | 1.2 | 22.7 | 15-21 | 43.1 | 19.4 |
| 7 (2124.3, -222.6) | 6 | (2132.50, -245.85, 419.762) | 1.3 | 22.8 | 17-23 | 42.6 | 13.9 |

Tightest occupied pair while each orb is lit, counting shelters and unmoved home spots together: 9.9 /
9.2 / 9.0 / 9.4 / 10.6.

The last two entries are 1.2 and 1.3 yd - below `ULDUAR_THORIM_RING_ARRIVE_TOLERANCE`, so those bots stand
still. That is correct: at 39.4 and 40.8 degrees they are outside the real 37.5 cone already, and the
entries exist so the table is total.

Orbs 5 and 6 threaten nothing and get no entries.

**Budget.** Longest run 25.6 yd, about 3.7 s at bot run speed, plus up to 0.59 s to detect the orb =
4.3 s against a 4.91 s worst measured lead. That is the tightest number in the whole design. A 21 yd cap
would be safer but makes orbs 1 and 3 infeasible, so the slack is what it is - verification point 3 below
exists to catch it.

**Code.** In `UldEncounter_Thorim.cpp`:

- `ThorimLightningChargeShelter(PlayerbotAI*, Player*, uint8 slot, Position& out)`: ask
  `ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL)` for the lit orb; resolve its index
  by nearest of the seven reference positions and **bail if nothing is within a couple of yards** - a
  mismatch must fall back to the home spot, never guess; test `RangedSpot(slot)`'s bearing off the boss
  against the cone bearing at 40 degrees; return the table entry if covered.
- `TryGetThorimPhase2Spot`'s Ranged branch (`:1757-1782`): after the round-robin picks `slot`, return the
  shelter when there is one, otherwise `RangedSpot(slot)`.
- Probe it: `RaidObs::NoteDerived(bot, "thorim.shelter", ...)` with slot, orb index and home/sheltered.

### 2. Why this will not oscillate

This is the failure mode the last two rounds hit, so it is designed against explicitly and there is trace
evidence for each part.

- **The destination is latched on orb identity, and the shelter flag is sticky.** Once a bot is flagged
  for the currently lit orb it stays flagged until a *different* orb lights - at least 15 s later. There
  is deliberately no "run home when the orb goes dark": that is a second full run for nothing, and it is
  the reason `LightningChargeOffset` already holds its offset the same way (`:471-474`). Store it on
  `ThorimEncounterState` beside `ringOffsets`, keyed the same (`{ObjectGuid orb, uint8 slot}`).
- **Sticky also absorbs boss drift.** The test is re-evaluated each tick until the bot arrives, so a boss
  that drifts *into* threatening this slot still leaves time to move; it can never drift the bot back out
  and cancel a walk in progress.
- **The same mechanism is already proven in these traces.** `thorim.ringspot` writes only on change and
  emits 20, 25 and 31 notes over 7-8 melee bots against 8, 11 and 7 orb lightings - **fewer notes than
  orb changes**, so at most one write per bot per orb and no chatter. The melee ring is the harder case,
  since its bearing is recomputed off a moving boss.
- **The camp is already stable.** Bots change camp spot 7, 8 and 10 times across a whole phase, all
  clustered in the first ~40 s while the raid spreads out into the camp.
- **Arrive/reposition deadband already exists** - `ULDUAR_THORIM_RING_ARRIVE_TOLERANCE = 3.0` /
  `REPOSITION 5.0`, applied by `ThorimRingNeedsMove`.

Two real oscillation sources found in the traces, both closed here:

- **The Blizzard dodge outranks the shelter walk.** Both nodes move at `MOVEMENT_COMBAT`, and
  `IsWaitingForLastMove` only lets a *strictly higher* priority through, while
  `ThorimPhase2PositioningTrigger` stands down outright whenever `ThorimSifBlizzardTrigger` is active
  (`UldTriggers_Thorim.cpp:156`). So a ranged bot with a Blizzard on it during the warning gets flung
  30 yd by the generic flee instead of walking to its shelter - possibly deeper into the cone. Measured
  frequency: 4-5% of bot-windows (3-4 bots a pull), 2 of every 7-10 warning windows. Close it by standing
  `ThorimSifBlizzardTrigger` down for a ranged or healer bot whose shelter is live and not yet reached -
  the same shape as the existing tank exemption. It trades up to 5 s of Blizzard ticks (~3k each) for a
  20k cone hit.
- **The ranged slot is recomputed live from group order** every call (`:1763-1779`), so a death
  reshuffles it - and the shelter table is keyed by slot. Observed once (Prayer, H, 5:17.496 spot 6 -> 4
  -> 6 within 2.3 s). Latch it the way `EnsureMeleeSlot` (`:196`) latches the melee slot.

### 3. Melee ring: rotate off the Blizzard instead of fleeing it

`LightningChargeOffset` already returns a bearing offset off the latched ring bearing. Extend that step
into one "safe ring bearing" pass in the same place, applied after the cone offset:

- If the ring point at the current bearing is within `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE` of any live
  `NPC_SIF_BLIZZARD`, step outwards in 2 degree increments both ways and take the first bearing whose ring
  point is clear of *every* bunny.
- New constant `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE = 11.0f`, documented as the measured 9.8 yd reach
  plus a yard of slack. Not `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS`, which is the generic dodge's trigger
  radius: at 15 the arc grows to a median 9.5-11.7 yd for no extra safety.
- Deadband: keep the offset while the current point is still clear, re-solve only when it becomes covered.
  Without it the offset chases a moving bunny every tick.
- The cone offset wins where they conflict - a cone is 20k in one instant, a Blizzard tick is ~3k.
- **Found during implementation, not in the original plan:** `ThorimRingNeedsMove`'s 5 yd reposition
  deadband swallows about half the slides, so a bot would hold station in the damage - it would have
  blocked 21% of the needed slides in F, 49% in H and 73% in I. `RingSlideBeatsTheDeadband` lets a bot
  standing inside `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE` of a bunny skip the deadband, but only when the
  spot it has been handed is itself clear - otherwise walking there is churn for nothing. The off-tank
  shares that helper and is unaffected in practice: its spot is not Blizzard-adjusted, so if it is
  standing in one its spot usually is too and the first test refuses.

Then exempt melee-ring bots from the generic node in `ThorimSifBlizzardTrigger::IsActive`
(`UldTriggers_Thorim.cpp:333`), alongside the existing tank exemption: `ThorimPhase2Role::MeleeRing` now
answers this on the ring. That also stops `ThorimPhase2PositioningTrigger` standing down for them
(`:156`), which is what has to happen for the rotated point to be walked, and it removes the 30 yd flee
that is currently producing 56-58% of their Blizzard damage and the multi-second stalls.

Ranged and healers keep the generic node except during a live shelter walk (section 2). Tanks stay exempt
for the documented reason (`:337-342`): dodging tows the boss into the camp.

## Deliberately not doing

- **Moving the tank anchor.** (2135, -263) is on-mesh and sits 29.7 yd off the Blizzard track against
  today's 5.3, which would leave the whole melee ring 21 yd clear and take the tank's 61-126k a pull with
  it. But the camp is defined 22-32 yd off the anchor, so every ranged spot and all twelve shelters would
  have to be re-solved. It is the right next round, not this one. Numbers are in `fit.py` and `anchor.py`.
- **The tank's Blizzard damage.** Deliberate and documented at `UldTriggers_Thorim.cpp:337-342`.
- **Melee that wander off the ring for reasons other than the flee.** Totemist alone spends 42-52% of the
  phase past 12 yd from the boss in every pull and eats 60-92k Blizzard for it; the rest sit at 7-31%.
  Re-measure once the 30 yd flee is gone - it is the obvious cause and may be the only one.
- **Chain Lightning.** One 8-target burst in H for 243k, all melee/tank/human, so it is the melee ring
  chaining rather than the camp; the camp's tightest occupied pair is 11.6 at home and 9.0 while
  sheltered, both above the 8.0 yd jump.
- **Sif.** 38-42% of phase 2 damage taken. Frostbolt Volley is DBC radius 200 with no positional answer.
- **A wider ranged cone margin.** 15 degrees like the melee ring makes the shelter set infeasible at 9 yd
  separation and forces runs that do not fit in the warning window.

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
- The twelve shelter coordinates are already navprobed. Re-probe any that get re-solved; drop a failure
  and re-run `final2.py` rather than nudging a point by hand.
- Copy this plan to `docs/plans/thorim-phase2-cone-and-blizzard/thorim-phase2-cone-and-blizzard.PLAN.md`
  as the first implementation step.

In game, one 25-man hard-mode Thorim pull, then re-read the trace:

1. **No Lightning Charge burst hits more than 2-3**, against 11, 8 and 11 in F, I and H, and its share of
   fatal blows drops from 9/30, 11/30 and 13/31. This is the whole change.
2. **`thorim.shelter` writes at most one line per bot per orb**, the same way `thorim.ringspot` does
   today. More than that is the oscillation and the latch is wrong.
3. **Every covered bot has arrived before the cone lands.** Cross-check each 62976 cast against the 62466
   damage rows 5 s later: a bot damaged while noted as sheltered means the shelter is wrong; a bot still
   walking means 25.6 yd does not fit and the run cap has to come down, which needs the anchor move to
   stay feasible.
4. **Melee Blizzard falls** from 184-236k, and the split shifts back onto the ring: the off-ring share
   should collapse from 34/56/58% once the 30 yd flee is gone.
5. **`thorim sif blizzard action` issues nothing for a melee bot**, and the multi-second stalls attributed
   to it disappear from `--stalls`.
6. **Melee movement does not go back up.** Under ~120 yd/min/head and ~30% of the phase moving, against
   128/32%, 66/17% and 84/22%. The ring rotation is cheap - median 3-7 yd of arc - so this should improve.
7. **Ranged and healers still take zero Blizzard** and still hold their spots, home or shelter.
8. **Chain Lightning does not get worse.** No burst past 8, and the camp's tightest occupied pair stays
   above 8 yd while sheltered.
9. **No regression** on: `thorim.slot` still ~1 note per bot and `thorim.squad` ~25 for the pull;
   `thorim.ringspot` still one latched bearing per bot; `thorim.p2role` still one note per bot at 0:00 and
   never `melee` for a tank; the boss still reaching the anchor inside ~17 s and holding under ~8 yd off
   it; phase 1 deaths still 0-1; hard mode still won with Sif dealing damage through phase 2.
