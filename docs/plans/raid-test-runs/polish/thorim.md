# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`; `raidtest.py report` runs
`--summary` on every counted pull). Rounds 1-21: `history/thorim.md`.

## State

**Fight model.** The start pack (a mock fight in the arena) dies first; Sif casts 62507 20 s after its last death, and
phase 2 must start within 150 s of that or she casts 62565 (normal mode: Thorim −30% health, no Sif). Phase 2 starts
only on a player's hit on Thorim from above z 430, allowed once the Rune Giant dies. The arena squad holds the adds; the
bot gauntlet squad (12) pulls the lever, clears the corridor's two trash packs under the Runic Smash lanes, kills the
Runic Colossus, the ramp's Honor Guards and Acolyte and the Rune Giant (held at (2187.79, −432.78)), walks the
balcony and hits him from `BALCONY_5`. No phase 2 by phase 1 + 5 min wipes the raid to the Lightning Orb. Lever to
62565 is ~170 s; what decides it is Runic Barrier on the Colossus (+11-12 s when it lands early), the corridor under
the smash, and lane flips. Phase 2: tanks hold him on the tank spot (2110.75, −252.65), the waiting tank on the
off-tank spot (2107.25, −252.65), a swap on every Unbalancing Strike; melee on a 3-slot ring (r 8.8, slots ≥60°
apart), ranged on 6 camp spots with shelters from the Lightning Charge cone; Chain Lightning every 15 s, 8 targets at
×1.5 a hop, hop 8 yd (8.9 when both carry Heroism/Bloodlust); hard mode adds Sif's Frostbolt Volley, Frostbolt,
Blizzard and Frost Nova. Blizzard's bunny walks the core's fixed path once at ~7 yd/s (west, then north of the tank
spot, then round the camp), dropping a 10 s zone every 2 s.

**What kills raids.** Hard mode at ilvl 226 is a race the rosters lose: from +30 s the raid takes him 0.34-0.55 %/s
(a kill by the first tank death needs ~0.74), and the tanks fall at +46-196 s to his melee, which grows with the
Lightning Charge stacks, plus Blizzard (22-27% of tank intake); every hard-mode phase 2 on disk (23) wiped, most deaths
being Thorim loose after both tanks fall. Before the first tank death: Blizzard on ring melee and the camp, Chain
Lightning bridging groups, Volley and Frostbolt on healers, Lightning Charge. Normal mode: kills with 0-4 deaths.

**Compositions on build 9f0c9a4c7 (window 31).** A (uld1, uld4): hard-mode wipes, Thorim 44.2% / 43.3%, first tank
death +112.2 / +119.0 s. B (uld2, uld5): uld2 normal kill, 2 deaths; uld5 hard wipe at 55.5%, first tank death
+79.7 s. C (uld3, uld6): uld3 normal kill, 1 death; uld6 hard wipe at 37.1%, first tank death +106.5 s. Hard mode
reached in 4 of 6; no hard-mode kill, so not polished.

**Open failures, ranked** (window 31, 127 deaths):
1. The hard-mode race: Thorim melee 72, nearly all after the first tank death. Not a bug; question 1.
2. Blizzard 12, 9 at or before the first tank death: ring melee stepping out ahead of the bunny (w31 uld1, uld5 3
   each, round 22's fix); camp bots walking to a cone shelter through the trail (w31 uld4 2 at +44 s, the positioning
   row lets a lit-cone camp bot walk under Blizzard); a healer (uld6 +82 s); tanks late.
3. Chain Lightning 12, 5 before the first tank death: stacked camp as the seed (w31 uld1 +72 s, 8 camp hits, 4 dead);
   the opening (w31 uld5 +12 s, slot 0 to the tank to slot 1 under Heroism; w29 uld1); camp RANGE4 8.8 yd from slot 1
   under Heroism (w29 uld1); a lit-orb cone dropping the 60° separation (w26 uld3, w29 uld6).
4. Sif's Volley and Frostbolt 12, before the first tank death 2 (a healer, a ranged; w29 5 on healers). Nothing to
   kick.
5. Lightning Charge 8 (w31 uld6 7, mostly after the tanks).
6. Early tank deaths, +46-80 s (w29 uld5, uld6; w31 uld5): Thorim melee, Blizzard, Lightning Charge.
7. Phase 1: the Runic Colossus' in-combat Smash 62339 (10 yd frontal cone, documented, not handled) on gauntlet
   healers, 4 in two windows (w26 uld2, w29 uld2, uld6); Rune Detonation 2; w31: Rune Giant melee, Honor Guard, Impale.

**Fixes in place** (round: fix — verdict):
- R1: the DPS picker opens nothing before Thorim is in combat — held.
- R2: arena anchors only once Thorim is in combat (user) — held.
- R3: lever used from touching distance; bot leader walks to the Rune Giant — held (gated in R5).
- R4: a corridor bot past `BALCONY_5` may hit Thorim (`ThorimBalconyHitAllowed`) — held.
- R5: Giant walk only once the first Colossus doors open — held (ramp pile 25 → 0 deaths).
- R6: `RingBodyBridgesChain` breaks the deadband for a chain bridge — held (two slots within 8 yd 60-78% → 10-19%).
- R7: gauntlet non-healers don't trail the leader while fighting — held, no time back.
- R8: off-tank spot (2107.25, −252.65) (user); only the tank he hits takes the tank spot — held.
- R9: corridor packs one at a time (picker and leader hold while one is engaged) — held.
- R10: lever wait (user), Colossus pulled from the stop (user), Colossus cache scanned only near his spawn — held.
- R11: Rune Giant hold (user), balcony walk from the ramp box, holder arrival 1 yd exact — held, no time back.
- R12: hold walk lowers the Giant-walk booking; Colossus stop at 29 yd — held.
- R13: ring anchor at the tank spots' midpoint plus `RelatchRingAnchor`; lane followers take the leader's waypoint —
  held.
- R14: lane waypoints near y −360 (user), squad gathered within 20 yd before the Colossus pull — held.
- R15: cannot-reach step for a floor bot Thorim can't reach — held (0 spans in w31).
- R16: corridor flee hold, Revive Pet hold on Sif's clock — held.
- R17: covered ring melee step out of Blizzard, `avoid aoe` held off ring melee (user) — held for standing in zones;
  its points bridged slots (R21) and ran ahead of the bunny (R22).
- R18: the swap fires only within 8 yd of the tank spot — held.
- R19: class taunts from past 8 yd held while he's on a non-tank or nobody, anchor exempt — held.
- R20: the anchor with its taunt cooling gets no spot while he's loose, so it closes on him — held (tank on him
  +0.1-1.6 s in w31, his melee on non-tanks before it 0k).
- R21: the Blizzard step's lead passes stay out of the jump of the camp and the other slots — held (slot to slot
  through a stepped slot: 0 crossings and deaths in w31's four hard pulls, w29 2/0/2/2 deaths).
- R22: the step's first round also stays 11 yd off the bunny's remaining walk, searched to 20 yd — pending
  (window 32).

**Tried and rejected:**
- Balcony runner: the user ruled it out (phase 2 with the squad deep in the corridor).
- The waiting tank stepping out of Blizzard: user no (R18); R19 found no step point out of hop range for a whole sweep.
- A whole-ring rotation off the cone: built and deleted (two whole-ring turns per orb cycle; boss doc).
- The solver trading bearings for Blizzard: removed in R17 (swung slots onto each other).
- A bearing threshold for the ring re-latch: left 16-29% (R13).
- Moving the tank anchor east: breaks 11 of 18 camp points (boss doc).
- A 10-20 yd lookahead of the bunny's walk instead of all of it: 230 / 52 of 641 replayed points covered against 32
  (R22).

**User answers in force:**
- 2026-11-22: hold the arena group at the drop until Thorim is in combat; keep polishing on the 226 rosters with the
  stop rule waived; off-tank spot (2107.25, −252.65), only the waiting tank stands there.
- 2026-11-23: no balcony runner, make the gauntlet clear faster; the gauntlet tank waits at the lever and pulls the
  Colossus from his 30 yd stop; the Giant is walked down to ~15 yd of the add fight; one more lane waypoint near
  y −360.
- 2026-11-24: covered ring melee step out of Blizzard and walk back, `avoid aoe` off ring melee; the waiting tank
  does not step out of Blizzard.
- Bot tanks spend defensives from their class triggers only.

**Open questions:**
1. (Round 20, unanswered; no stop rule applied from it.) How should Thorim finish at ilvl 226? (a) Close him on
   mechanics like Hodir: polished once a window's hard-mode phase 2s lose nobody to a mechanic before the first tank
   death and its normal-mode pulls are kills with at most 3 deaths, then the 10-man check. (b) Pull him on rosters
   near ilvl 239-245 to chase the kill (~1.5x today's damage). (c) A 5th healer and/or a timed tank cooldown chain at
   the late holds (maybe 20-40 s more tank life, short of the gap alone). (d) Keep polishing with no end condition.
   Recommended: (a).

## Round 22 (window 31: build 9f0c9a4c7 with round 21)

**Data:** `window31-thorim.md` (session scratchpad), one valid pull per pool; traces
`603_{7,9,10,11,13,12}_thorim_{1795541779,1795542231,1795542272,1795542293,1795542387,1795542378}` (uld1, uld2, uld4,
uld3, uld5, uld6), read with `thorim.py --blizzard/--chains` and session scratchpad `thorim_r22/` scripts (a timeline,
a bunny-path check and a step replay over the fourteen hard-mode pulls since round 17, w25-w31).

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | wipe, hard (P2 3:25.8), 44.2% | CL 4 (+72 s, 8 camp hits), Volley 1, Blizzard 3 (slot 2 at +78-81 s), then tanks +112 s, Thorim melee 17, Volley 4, CL 1 |
| uld4 (A) | wipe, hard (P2 3:06.9), 43.3% | phase 1 Honor Guard 1; Blizzard 2 (camp at +44 s), Lightning Charge 1, Volley 1, Frost Nova 1, then tanks +119 s, Thorim melee 17, CL 5 |
| uld2 (B) | kill, normal | phase 1 Rune Giant melee 1; CL 1 |
| uld5 (B) | wipe, hard (P2 3:16.0), 55.5% | CL 1 (opening +12 s), tank +79.7 s, Blizzard 3 (slot 2, +80 s), Thorim melee 23 |
| uld3 (C) | kill, normal | phase 1 Impale 1 |
| uld6 (C) | wipe, hard (P2 3:01.2), 37.1% | Blizzard 1 (healer +82 s), Lightning Charge 1, tanks +106 s, Thorim melee 15, Lightning Charge 6, Volley 3 |

**Hard mode in the kills: none.** uld2 and uld3 read normal mode; the four hard-mode pulls wiped.

**Stop rules:** not polished (no hard-mode kill in A, B or C). Not stuck: ring Blizzard deaths were picked in R17 and
now R22 (two rounds), R21's chain crossings are a different failure. Question 1 stays open; no stop rule applied.

**Round 21 verdict (step points clear of the camp and the other slots): held in A, B and C.** `--chains` through a
stepped slot: 0 crossings and 0 deaths in uld1, uld4, uld5, uld6 (w29 deaths 2 / 0 / 2 / 2; camp into one 0, w29
1 / 0 / 1 / 0); Chain Lightning deaths 21 → 12. Steps per pull 37 / 55 / 40 / 44 (w29 17-56). Its "no ring Blizzard
death before the first tank death" failed in uld1 (3): not its clearance but the step running ahead of the bunny,
which predates it (below).

**Failure picked: ring melee stepping out of Blizzard onto the bunny's path ahead.** Blizzard is the top mechanic at
or before the first tank death (9 against Chain Lightning's 5), and its ring deaths came back (0-1 a window since
R17, 6 in w31).
- Trace: uld1 +74.8 s, the bunny at (2109.2, −246.6) heading for (2117.9, −238.6); slot 2's three melee on
  (2117.2, −246.0) stepped to (2124.3, −238.9), 3.5 yd off its next leg; it caught them at +77.6 s and each later
  step went further along its way ((2131.4, −236.1), (2135.3, −234.3)): all three died +78.7-81.7 s, 2.9-5.8 s in
  zones. uld5 +74.8 s the same slot the same way (steps (2118.6, −245.0) → (2129.7, −233.8)), three dead at +80.0 s;
  in its first Blizzard (+33.8 s) Uldvay stood 10.1 s in zones running with it to 20.7 yd off Thorim.
- `--blizzard` (new counts): steps onto the bunny's path ahead 26/37, 37/55, 31/40, 18/44 in w31 (w29 29/46, 33/49,
  11/17, 35/56), under a zone within 8 s 16 / 18 / 25 / 11; ring melee in zones 46.9 / 11.1 / 42.0 / 21.3 bot-s
  (w29 2.7-35.2), longest stays 8.8 and 10.1 s.
- Code: `ThorimRingBlizzardStep` (`UldEncounter_Thorim.cpp:3171`) cleared the live zones and the bunny where it
  stands (`ThorimBlizzardSpots`), nothing of where it goes; the bunny walks the core's fixed path
  (`boss_thorim.cpp:968`) at a run's pace, so the nearest clear point is usually ahead of it.
- **Classification: bug.** The strategy intends the step to leave the Blizzard; its points walked into the next zones.

**Fix** (`UldEncounter_Thorim.cpp/.h`, Thorim only, no size, mode or player branch): the scan keeps the bunnies
apart (`blizzardBunnies`); `ThorimBlizzardPathAhead` builds the bunny's walk from where it stands to the end of
`ULDUAR_THORIM_SIF_BLIZZARD_PATH` (the core's spawn and eight waypoints). The step's four passes run first within 20 yd
and 11 yd clear of that walk, then as before within 15 yd, then anywhere within 20. With no bunny (normal mode) the
first round is skipped. Replay of 641 steps over fourteen hard-mode pulls (no navmesh, floor cut at x 2100 and
y −288): points a zone covered within 8 s 305 → 32, every step found a point, walk p50 6 → 12 yd, p90 14 → 20,
points in a camp body's jump 35 → 120 (173 at 15 yd), in another slot's 39 → 48. Reader: `--blizzard` counts step
points onto the path ahead and under a zone within 8 s, tested in `test_thorim.py`; boss doc: the bunny's walk, the
pass order, the evidence.

`pb-check` passes (compile, pblint, codestyle, botobs tests).

**Next window should show** (hard-mode pulls, `--blizzard`):
- steps onto the bunny's path ahead near 0 (w31 26 / 37 / 31 / 18), under a zone within 8 s near 0 (16 / 18 / 25 /
  11); ring melee in zones well under w31's 11-47 bot-s, longest stay ≤ ~3 s; no ring Blizzard death before the first
  tank death.
- Walk back p50 within ~1 s of w31's 0.3-2.6 s; `--chains` through a stepped slot still 0, camp into one at most 1 a
  pull; melee on Thorim to +100 s still ~100%.

**Seen, not picked:** camp bots walking to a cone shelter through the trail (uld4 2, rank 2); a stacked camp seeding
8 hits (uld1 +72 s, rank 3); the Colossus' Smash (rank 7); the waiting tank's Blizzard 22-49 ticks a pull (user: it
does not step).

**Questions for the user:** none new; question 1 in State stands, recommended (a).

**Shared code touched:** none (Thorim files and the Thorim reader).
