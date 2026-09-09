# Thorim phase 2: keep the tank on the anchor, and break the Chain Lightning chain

Traces read with `python tools/botobs/postmortem.py <file>` from `modules/mod-playerbots`; files live in
`env/dist/logs/botobs/`. **Timestamps are milliseconds.** Scratchpad scripts that produced every number
below: `lib.py` (loader, needs `G:/` paths not `/g/`), `p2.py`, `taunt.py`, `tanks.py`, `cl.py`, `osc.py`,
`ring.py`, `lock.py`, `sum.py`.

Three pulls, 9 Sep 2026, same 24-man roster: 2 bot tanks (Bulwark protection paladin, Ecoterrorist feral
druid), 1 human (Deathsong, **unholy** DK - Necrosis/Wandering Plague/Ebon Plague/Unholy Blight, so
`PlayerbotAI::IsTank` is false for them and there are exactly two tanks).

| file | phase 2 | wipe | deaths | Thorim median off anchor |
|---|---|---|---|---|
| A `603_1_thorim_1788975537` | 2:50.5 | 4:45.8 | 30 | 6.6 after 3:15, **22-36 for the first 25 s** |
| B `603_1_thorim_1788976162` | 3:19.0 | 3:42.9 | 24 | phase collapsed in 24 s |
| C `603_1_thorim_1788976546` | 3:09.0 | 5:12.1 | 30 | **16.0, p90 26.2** |

Build carries `8942c4744`: the taunt guard is visibly working, `hand of reckoning` and `righteous
defense` show `rel 0` in the act log exactly while another tank holds Thorim.

---

## Context

### 1. Why the boss gets tanked out at the north-east side (user question 1)

Two different causes, one per pull, and neither is the taunt war that `8942c4744` fixed.

**Pull A - the off-tank was still in the gauntlet.** Thorim lands at 2:50.5. Bulwark (arena squad) picks
him up and is standing on the anchor at 2:55.9 with the boss 5.9 yd away. Correct. But a taunt only
forces the target for 3 s and then hands the boss to the highest *real* threat, and Bulwark had none:
Thorim sat at 100% and untouched through all of phase 1, so the threat table was empty and a hunter beat
him. At **2:56.706**, exactly 3 s after Bulwark's 2:53.717 Hand of Reckoning, Thorim's Unbalancing Strike
goes to **Trueshot, 44 yd out**. Bulwark's re-taunts are on cooldown (`righteous defense IMPOSSIBLE`
2:56.199, `thorim tank pickup action FAILED` 2:56.822) and he chases with `reach melee` from 5.4 yd off
the anchor out to 28.3. Meanwhile Ecoterrorist is at (2149.5, -331.1), **87 yd away**, walking back up
the corridor under `thorim balcony advance action`; at 2:57.845 he growls Thorim from (2137.2, -289.4),
**45 yd off the anchor**. The guard allows it correctly - a hunter has the boss - and Thorim runs to him.
They meet at 3:00.45 at (2137.4, -277.6), gap 0.7 yd, **in the middle of the ranged camp**, 36.5 yd off
the anchor. 3:01.708 Chain Lightning, 3:02.24 three dead. Thorim only reaches the anchor at 3:15.

**Pull C - the tank holding the boss was put in the melee ring.** `thorim phase 2 positioning action`
handed **Bulwark 156 melee-ring points and zero anchor points** while he held Thorim for 51.9 s of the
123 s phase. A melee-ring point is 8 yd off the boss, so it moves with the boss: a tank standing on it
never pulls him anywhere. Thorim sat at (2134.7, -263.1), 26 yd out, from 3:10 to 3:35. Ecoterrorist got
the anchor, so he was `MainTank` and Bulwark fell past both `IsMainTank` and `IsAssistTankOfIndex(_, 0)`
in `GetThorimPhase2Role` (`UldEncounter_Thorim.cpp:1661`) into `MeleeRing`. With two tanks that should be
impossible, which means `PlayerbotAI::IsTank(Bulwark)` - `ContainsStrategy(STRATEGY_TYPE_TANK)`,
`PlayerbotAI.cpp:2299` - was false for him on that pull even though the RaidObs roster recorded him as a
tank at 0:00. Whatever the reason, the role must not be able to demote a tank into an 8 yd orbit.

Two more things ride on the same root:
- `TakesMeleeSlot` (`:168`) let him eat one of the 3 melee slots.
- The phase-2 tank exemption in `ThorimSifBlizzardTrigger` keys off the role, so Bulwark took **2 accepted
  `thorim sif blizzard action` moves**, one to 39.6 yd off the anchor. That is last round's fix regressing
  through the role, not through the trigger.

Thorim's victim changed 13 times in pull A and 31 in pull C, and **never once to a pet** - the 13-15 pet
Growls per pull from `Worm (Nightwarrior)` and `Wolf (Trueshot)` land but never take the boss. So the
flips are a threat problem, not a taunt problem.

### 2. Why several ranged died to Chain Lightning at one point (user question 2)

`Chain Lightning 64390`: `EffectChainTargets_1 = 8`, **`EffectChainAmplitude_1 = 1.5`**, so the 8th
target takes about 17x the first, and the primary is random.

**The jump is 8.0 yd centre to centre, not the DBC 10.** `Spell.cpp:2177` takes a per-spell override from
`spell_jump_distance`, and `acore_world.spell_jump_distance` has 64390 at **5.0**; the reach test at
`:2213`/`:2228` is `IsWithinDist`, which adds both combat reaches, so two players jump at 5.0 + 1.5 + 1.5.
The trace agrees exactly: across the six real bursts the minimum spanning tree over the victims has a max
edge of 8.06 yd.

At 8.0 yd the raid is one blob for about half the phase - largest connected group **median 7, p90 22, max
24** in pull A and **median 6, p90 19, max 23** in pull C, **>= 8 in 46% and 45% of snapshots**. Median
nearest-neighbour distance is **0.5 yd**, p10 **0.0**: bots stand on top of each other. 14 ranged and
healers share 5 spots whose closest pairs are 8.1-8.6 yd apart - right on the 8.0 yd boundary, which
arrival drift and three bodies per point close. Every observed burst reached 7 or 8 targets.

| burst | targets | damage | tail | dead | boss off anchor |
|---|---|---|---|---|---|
| A 3:02.237 | 8 | 144,278 | 29k/33k/55k | 3 | 34.3 |
| A 3:32.456 | 8 | 195,726 | 36k/35k/71k | 3 | **6.4** |
| B 3:30.731 | 8 | 179,067 | 24k/44k/60k | 5 | 20.1 |
| C 3:35.895 | 8 | 186,759 | 22k/51k/66k | 4 | 24.4 |
| C 4:36.264 | 8 | 239,929 | 27k/47k/**108k** | 2 | 16.5 |

Chain Lightning alone is **359k of pull A's and 536k of pull C's** phase-2 damage taken. The boss standing
in the camp makes it worse but is not required - pull A's 3:32 burst had him on the anchor and still
killed three ranged. What decides it is that a chain of 8 always exists.

**Lightning Charge 62466 has no answer for ranged at all.** 500k in pull A and 255k in pull C, of which
301k / 130k lands on ranged and healers. Four of pull A's deaths at 4:21 and three of pull C's at 4:54
have Lightning Charge as the final blow. The 5 static spots never leave the cone; only the melee ring has
cone logic.

### 3. Melee oscillation (user question 3) and Bulwark's (user question 4)

| | pull A | pull C |
|---|---|---|
| melee moving / yd per min per head | 48% / 181 | 27% / 107 |
| **tank** moving / yd per min per head | 16% / 52 | **51% / 218** |
| ranged | 13% / 49 | 21% / 79 |

Melee is better than the 71% / 282 that motivated last round, but the oscillation moved onto the tank:
Bulwark alone logged **53 destination changes, median leg 14.1 yd, 811 yd walked** in 2 minutes, because
he is in the ring (see 1).

The remaining melee oscillation is a defect I could not close from the trace. The destination flips
between two ring points about 144 degrees apart with the boss stationary, several times a second:

```
pull C, Bulwark, boss parked at (2115.2, -254.7), no orb lit 3:55.118 -> 4:04.884
  3:55.881 thorim phase 2 positioning action  bearing 246  r8.0
  3:58.647 thorim phase 2 positioning action  bearing  30  r8.0
  4:00.707 thorim phase 2 positioning action  bearing 246  r8.0
  4:02.638 thorim phase 2 positioning action  bearing  30  r8.0
  4:04.489 thorim phase 2 positioning action  bearing 246  r8.0
```

The current code cannot do this: with no orb lit `LightningChargeOffset` returns the held offset
unchanged, `LatchedRingBearing` is latched per bot, `meleeSlots` is an `ObsGuidMap` and emitted no change
after 3:05, and `thorim reset encounter state action` never fires in phase 2 (checked in both traces), so
nothing erases `ringBearings`. One of those three is not behaving as written, and `ringBearings`
(`UldEncounter_Thorim.h:277`) is the only one that is a plain `std::unordered_map` and therefore invisible
in the trace. That is why step 3 below adds the probe instead of a rewrite.

A second, separate contributor is confirmed: **two actions drive the same spot every ~200 ms**.
`ThorimLightningChargeTrigger` (`UldTriggers_Thorim.cpp:221`) and `ThorimPhase2PositioningTrigger`
(`:148`) both call `TryGetThorimPhase2Spot` for melee and both issue `MoveTo`, contesting the movement
gate on nearly every tick (`[wait]` rows). The charge node also skips the Blizzard and Frost Nova
stand-downs the positioning node deliberately honours, so it walks bots back into Blizzard.

---

## The change

### 1. A tank never lands in the melee ring

`GetThorimPhase2Role` (`Util/UldEncounter_Thorim.cpp:1661`):

- Before the index tests, return `MainTank` when the bot is currently Thorim's victim. Whoever holds him
  owns the anchor, whatever the group ordering says.
- After `IsMainTank` and `IsAssistTankOfIndex(bot, 0)`, add a final `PlayerbotAI::IsTank(bot)` test that
  returns `OffTank`. A third tank parks on the off-tank bearing, which is 8 yd off the boss but is only
  ever taken while somebody else holds him.
- Belt and braces against the strategy-set flicker that produced pull C: use
  `PlayerbotAI::IsTank(bot) || PlayerbotAI::IsTank(bot, /*bySpec*/ true)` for that last test, so a bot
  whose tank strategy is momentarily absent is still not handed a melee slot.

`TakesMeleeSlot` (`:168`): exclude any member for whom either `IsTank` form is true, replacing the
"a third tank is left in the ring" comment - that is exactly what went wrong.

This alone fixes pull C's boss position, Bulwark's oscillation, and the Blizzard regression.

### 2. One mover for the phase 2 spot

Delete `ThorimLightningChargeTrigger` / `ThorimLightningChargeAction` and their wiring
(`UldStrategy.cpp`, `UldActionContext.h`, `UldTriggerContext.h`, the two header declarations), and delete
`ThorimLightningChargeActive` (`UldEncounter_Thorim.cpp:1855`) with them. The cone answer is baked into
the spot `ThorimPhase2PositioningAction` already walks to, `ThorimRingNeedsMove`'s 5 yd reposition
tolerance is smaller than any real cone step, and the charge node's only other effect is to override the
hazard stand-downs.

### 3. Make the ring decision visible

`NoteDerived(bot, key, value)` (`Bot/Obs/RaidObs.h:201`) emits only when the derived answer changes, which
is free while the ring is stable and decisive when it is not. Add three probes inside
`TryGetThorimPhase2Spot`, in the helper rather than at the call sites:

- `thorim.p2role` - the resolved `ThorimPhase2Role`, on every call.
- `thorim.ringslot` - the slot from `MeleeSlotOf`.
- `thorim.ringbearing` - `bearing` and `offset` in degrees, formatted to whole degrees so a stable ring
  emits nothing.

One pull then says whether the 144 degree flip is the slot, the latch or the offset, and the ring fix
lands next round against evidence.

### 4. Respread the ranged camp so a chain of 8 cannot exist

Replace the 5 ranged spots with **6**, pairwise at least 11 yd apart. The old camp's closest pair is
8.1 yd, which a bot standing one yard off its point is already inside; 11 yd holds up with a bot off its
point on either side.

Six is the ceiling, not a preference. Three constraints fight each other in a room this size:

- **Sif's Blizzard bunny reaches far further in than the rim.** 1894 hazard samples across the three
  traces cover x 2104-2165, y -280 to -232 at radius 8, so the whole north-east is her lane. Every spot
  clears the sampled path by >= 16 yd; the old camp's worst was 18.4.
- **The melee ring must not bridge into the camp.** The ring is 8 yd off the boss, so every camp spot sits
  >= 22 yd from the tank spot. A first solve ignored this and left a ring slot 6.2 yd from a camp point,
  which in an idealised formation merged ring and camp into a component of 5 - worse than the layout it
  replaced.
- **Inside the arena floor**, y > -284 and within 29 yd of the arena centre (2126.0, -256.35, from the six
  ground-level Thunder Orbs).

Under all three, six points fit at 11 yd and five at 12. Modelled on a formation where everyone stands
exactly on their point, old and new both cap the chain at 3 targets; **the win is margin, not the ideal
case.** What produced the observed 7 and 8 target chains is bots not holding their points, the boss
wandering the camp, and probably pets - see below.

Spots, against the anchor `ULDUAR_THORIM_PHASE2_TANK_SPOT` (2110.7483, -252.65265):

| slot | x | y | z (probed) | yd to anchor | Blizzard clearance |
|---|---|---|---|---|---|
| 1 | 2132.75 | -252.65 | 419.775 | 22.0 | 20.3 |
| 2 | 2121.52 | -282.25 | 419.508 | 31.5 | 16.1 |
| 3 | 2123.05 | -270.89 | 419.701 | 22.1 | 16.8 |
| 4 | 2132.98 | -275.67 | 419.726 | 32.0 | 26.1 |
| 5 | 2131.50 | -263.69 | 419.847 | 23.5 | 27.2 |
| 6 | 2142.05 | -259.31 | 419.822 | 32.2 | 22.7 |

**navprobe run, all 6 pass.** Every one is on the mesh (distance to poly <= 0.24) and every path from the
anchor is `PATHFIND_NORMAL` at straight-line length. The z below is `UpdateAllowedPositionZ` from the probe,
not borrowed from a neighbouring spot - per `modules/mod-playerbots/CLAUDE.md` and
[docs/engine/pitfalls.md](../../engine/pitfalls.md).
navprobe lives in the **core fork** at `src/tools/navprobe`, not in this module:

```
MSYS_NO_PATHCONV=1 docker run --rm \
  -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro \
  --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master --map 603 point <x> <y> <z>
```

A first nine-spot solve ignored the Blizzard lane and put two points 5.6 and 6.8 yd from the bunny's
path; they navprobed clean, which is exactly why the hazard check has to be its own step. If a later edit
moves any of these, re-probe it and re-solve the whole set against all three constraints rather than
nudging a failed point by hand.

Code side: `ULDUAR_THORIM_RANGED_SLOTS` 5 -> 6 (`UldEncounter_Thorim.h:124` area), the
`ULDUAR_THORIM_PHASE2_RANGE*_SPOT` block (`UldEncounter_Thorim.cpp:60-64`) and `RangedSpot`'s table. The
round-robin in `TryGetThorimPhase2Spot`'s Ranged branch needs no change.

---

## Deliberately not doing

- **Rewriting the melee ring spot.** Step 3 traces it first; the rewrite lands next round against the
  probe output rather than against a hypothesis.
- **Lightning Charge cone avoidance for ranged.** Worth 130-300k a pull, but it puts the ranged camp back
  in motion while the ring's own movement bug is still open. Next round, after the probe.
- **Pet Growl.** 13-15 casts a pull at Thorim from hunter pets, but the boss's victim never once became a
  pet in any of the three traces, so there is no measured cost to fix.
- **Pets as Chain Lightning links.** There are a median of 6 and a p90 of 25 live pets and guardians in
  phase 2, and a hostile chain's target search takes any enemy of the caster, so they almost certainly
  both take jumps and bridge between clumps. The recorder only writes `dmg` for raid members, so no trace
  can currently confirm it - which makes this the most likely reason the observed chains reached 7 and 8
  where an idealised formation caps at 3. Extending the recorder to log pet damage is the prerequisite,
  and it is a change to `Bot/Obs`, not to Thorim.
- **The taunt cadence.** `thorim tank pickup action` fires 6-7 times a pull against 13-31 victim changes,
  because a 3 s taunt on an empty threat table hands the boss straight back. Keeping the tank parked and
  swinging (step 1) is the cheap half; re-taunt cadence is a separate question and needs its own trace.
- **Sif.** 34% of pull C's phase-2 damage and the largest single non-Thorim source, but Frostbolt Volley
  is DBC radius 200 with no positional answer and Frost Nova already has a dodge node.

---

## Verification

Static, from `modules/mod-playerbots`:

- `python apps/codestyle/codestyle-cpp.py` clean for the touched files. Its three standing failures
  (`DBCStructure.h` tabs, `mmaps_generator`) are pre-existing.
- No line over 120 columns; files stay LF and ASCII. Check the encoding in Python reading bytes, not with
  `grep -P` - the locale here rejects it and the pass is then meaningless.
- Per-TU syntax check in `acore/ac-wotlk-build:master` against `/azerothcore/build/compile_commands.json`
  for each changed `.cpp`: take the entry for the file, drop `-c` and `-o`, add `-fsyntax-only`, run from
  its `directory`. Needs `MSYS_NO_PATHCONV=1` and `$(pwd -W)` for the mount source or Git Bash mangles the
  paths. Expect only the two standing `-Wunused-parameter` warnings in `UldEncounter_Thorim.cpp`. Takes
  ~2.5 min, so run it backgrounded - and do not edit the tree while it runs, the mount is live.
- navprobe clean for all 9 ranged spots, point and path.

In game, one 25-man hard-mode pull, then re-read the trace:

1. **No melee-ring point is ever handed to a tank.** `thorim.p2role` never reads `MeleeRing` for Bulwark
   or Ecoterrorist, against 156 ring points for Bulwark in pull C. Both tanks' accepted
   `thorim phase 2 positioning action` destinations are the anchor or the off-tank bearing.
2. **Thorim's median distance from (2110.7, -252.7) under ~8 yd, p90 under ~15**, against 16.0 / 26.2 in
   pull C, and he is on the anchor within ~10 s of landing rather than 25.
3. **No `thorim sif blizzard action` move by either tank**, against 2 accepted for Bulwark.
4. **Tanks move under ~60 yd/min/head and under ~20% of the phase**, against 218 / 51%.
5. **Largest 8-yd connected group under 8 for nearly all of the phase**, against >= 8 for 45-46% of it,
   and no Chain Lightning burst reaches 8 targets. Burst totals well under the 180-240k seen now, and no burst
   kills more than one bot.
6. **Chain Lightning damage taken well under 359k / 536k**, and the tail hit under ~40k against 108k. No
   burst should exceed 3 targets while the camp is formed.
   Also watch **Blizzard damage on ranged**: the camp moved, and the 16 yd clearance is derived from three
   traces of bunny samples, not from the spawn rule.
7. **`thorim.ringbearing` shows what the melee ring actually does.** Either it holds - one value per bot
   for the phase, which means removing the second mover was the whole story - or it flips, and the note
   names which of slot, latch or offset moved.
8. **No regression** on: pickup vacuum still under ~2 s; phase 1 deaths still 0; `combat formation move`
   still owning 0 moves; the squad split still forming; hard mode still won with Sif dealing damage
   through phase 2; the taunt guard still showing `rel 0` on class taunts while a tank holds the boss.
