# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`;
`raidtest.py report` runs `--summary` on every counted pull). Rounds 1-22: `history/thorim.md`.

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
apart), ranged on 6 camp spots with shelter rows from the Lightning Charge cone (lit orb 4.6-4.9 s ahead); Chain
Lightning every 15 s, landing 0.4-1.4 s after an orb lights, 8 targets at ×1.5 a hop, hop 8 yd (8.9 when both carry
Heroism/Bloodlust); hard mode adds Sif's Frostbolt Volley (−50% speed), Frostbolt (−65%), Blizzard (−65% in a zone)
and Frost Nova (root). Blizzard's bunny walks the core's fixed path once at ~7 yd/s (west, then north of the tank
spot, then round the camp), dropping a 10 s zone every 2 s.

**What kills raids.** Hard mode at ilvl 226 is a race the rosters lose: from +30 s the raid takes him 0.34-0.55 %/s
(a kill by the first tank death needs ~0.74), and the tanks fall at +46-196 s to his melee, which grows with the
Lightning Charge stacks, plus Blizzard (22-27% of tank intake); every hard-mode phase 2 on disk (28) wiped, most deaths
being Thorim loose after both tanks fall. Before the first tank death: Lightning Charge on camp bots short of a
shelter row, Blizzard on ring melee walking a step and the camp, Chain Lightning bridging groups, Volley and Frostbolt
on healers. From +100 s every cone one-shots cloth. Normal mode: kills with 0-4 deaths.

**Compositions on build 9ad197722 (window 33).** A (uld1, uld4): hard-mode wipes, Thorim 36.5% / 18.0%, first tank
death +119.2 / +147.5 s. B (uld2, uld5): hard wipes at 42.3% / 45.2%, first tank death +82.2 / +87.4 s. C (uld3,
uld6): uld3 hard wipe at 33.1%, first tank death +157.1 s; uld6 normal kill, 1 death. Hard mode reached in 5 of 6; no
hard-mode kill, so not polished.

**Open failures, ranked** (window 33, 155 deaths):
1. The hard-mode race: Thorim melee 93 and Unbalancing Strike 9, nearly all after the first tank death. Not a bug;
   the user keeps it polishing with no end condition.
2. Lightning Charge 19, 8 before the first tank death, all camp bots short of a shelter row: slot 5's rows for orbs 0
   and 1 (25.6 / 21.8 yd across the Blizzard track) 7, row (2, 2) 1; walked under Sif's slows or begun late (casts,
   roots, an Evocation). After the tanks: rows reached with Thorim loose off the tank spot. Round 23's fix; if slot 5
   still dies, the next lever is shorter rows for slot 5 (a navprobe re-solve, a plan change for the user).
3. Blizzard 11: ring melee 6 before the first tank death (uld1 2, uld2 1, uld3 3), each walking a step at 2.4 yd/s
   under a zone or Frostbolt, the walk crossing the bunny's next leg or the fresh trail (round 22's longer points);
   camp 3; uld2's first tank death.
4. Chain Lightning 9, 4 before the first tank death (uld4 slot to slot 3, one through a stepped slot; uld3 1).
5. Sif's Volley 8, Frostbolt 3, Frost Nova 2, about 5 before the first tank death. Nothing to kick.
6. Self-res into the hazard: a bot back from Reincarnation or a soulstone at 20% in the zone that killed it dies again
   within 1.1 s (w33 4: Uldxap, Uldxai, Uldyak, Uldzaj). Shared res code; not picked.
7. Early tank deaths, +82 / +87 s in B (Blizzard then melee; Unbalancing Strike).
8. Phase 1: the Runic Colossus' in-combat Smash 62339 (10 yd frontal cone, not handled) on a gauntlet healer (w33
   uld5; 4 in w26-w29).

**Fixes in place** (round: fix — verdict):
- R1: the DPS picker opens nothing before Thorim is in combat — held.
- R2: arena anchors only once Thorim is in combat (user) — held.
- R3: lever used from touching distance; bot leader walks to the Rune Giant — held (gated in R5).
- R4: a corridor bot past `BALCONY_5` may hit Thorim (`ThorimBalconyHitAllowed`) — held.
- R5: Giant walk only once the first Colossus doors open — held (ramp pile 25 → 0 deaths).
- R6: `RingBodyBridgesChain` breaks the deadband for a chain bridge — held (two slots within 8 yd 60-78% → 4-32%).
- R7: gauntlet non-healers don't trail the leader while fighting — held, no time back.
- R8: off-tank spot (2107.25, −252.65) (user); only the tank he hits takes the tank spot — held.
- R9: corridor packs one at a time (picker and leader hold while one is engaged) — held.
- R10: lever wait (user), Colossus pulled from the stop (user), Colossus cache scanned only near his spawn — held.
- R11: Rune Giant hold (user), balcony walk from the ramp box, holder arrival 1 yd exact — held, no time back.
- R12: hold walk lowers the Giant-walk booking; Colossus stop at 29 yd — held.
- R13: ring anchor at the tank spots' midpoint plus `RelatchRingAnchor`; lane followers take the leader's waypoint —
  held.
- R14: lane waypoints near y −360 (user), squad gathered within 20 yd before the Colossus pull — held.
- R15: cannot-reach step for a floor bot Thorim can't reach — held (0 spans in w33).
- R16: corridor flee hold, Revive Pet hold on Sif's clock — held.
- R17: covered ring melee step out of Blizzard, `avoid aoe` held off ring melee (user) — held for standing in zones;
  its points bridged slots (R21) and ran ahead of the bunny (R22); its walk is rank 3.
- R18: the swap fires only within 8 yd of the tank spot — held.
- R19: class taunts from past 8 yd held while he's on a non-tank or nobody, anchor exempt — held.
- R20: the anchor with its taunt cooling gets no spot while he's loose, so it closes on him — held (tank on him
  +0.6-3.8 s in w33).
- R21: the Blizzard step's lead passes stay out of the jump of the camp and the other slots — held (w33 one stepped
  crossing, uld4).
- R22: the step's first round stays 11 yd off the bunny's remaining walk, searched to 20 yd — held on its points (0 of
  136 onto the path ahead, 0 under a zone within 8 s); ring Blizzard deaths before the first tank death unchanged (6).
- R23: a camp bot that can't reach its shelter row before the cone at its current speed takes a cone step (nearest
  point 45° off the cone in reach); a camp bot in the lit cone breaks its channel to walk — pending (window 34).

**Tried and rejected:**
- Balcony runner: the user ruled it out (phase 2 with the squad deep in the corridor).
- The waiting tank stepping out of Blizzard: user no (R18); R19 found no step point out of hop range for a whole sweep.
- A whole-ring rotation off the cone: built and deleted (two whole-ring turns per orb cycle; boss doc).
- The solver trading bearings for Blizzard: removed in R17 (swung slots onto each other).
- A bearing threshold for the ring re-latch: left 16-29% (R13).
- Moving the tank anchor east: breaks 11 of 18 camp points (boss doc).
- A 10-20 yd lookahead of the bunny's walk instead of all of it: 230 / 52 of 641 replayed points covered against 32
  (R22).
- A body-clearance first pass for the camp cone step: replay saved 4 of 16 dying walkers against 6 (longer walks,
  R23).

**User answers in force:**
- 2026-11-22: hold the arena group at the drop until Thorim is in combat; off-tank spot (2107.25, −252.65), only the
  waiting tank stands there.
- 2026-11-23: no balcony runner, make the gauntlet clear faster; the gauntlet tank waits at the lever and pulls the
  Colossus from his 30 yd stop; the Giant is walked down to ~15 yd of the add fight; one more lane waypoint near
  y −360.
- 2026-11-24: covered ring melee step out of Blizzard and walk back, `avoid aoe` off ring melee; the waiting tank
  does not step out of Blizzard.
- 2026-11-24: keep polishing Thorim on the 226 rosters with no end condition; the stop rules stay waived for him, as
  for Algalon, and he is not closed on mechanics.
- Bot tanks spend defensives from their class triggers only.

**Open questions:** none.

## Round 23 (window 33: build 9ad197722 with round 22)

**Data:** `window33-thorim.md` (session scratchpad), one valid pull per pool; traces
`603_{7,9,11,10,8,12}_thorim_{1795546500,1795546545,1795546567,1795546559,1795546520,1795546576}` (uld1, uld2, uld5,
uld3, uld6, uld4), read with `thorim.py --blizzard/--chains/--shelters`, session scratchpad `thorim_r23/` scripts (a
walk timeline, a ring zone-entry read, a cone-step replay over 19 hard-mode pulls since w23).

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | wipe, hard (P2 3:13.5), 36.5% | LC 2 (slot 5 walk +46 s), Blizzard 3 (Uldxap after a self-res, ring Uldxai +76 s twice), then tanks +119 s, Thorim melee 19, LC 5, CL 2, US 1 |
| uld4 (A) | wipe, hard (P2 2:57.5), 18.0% | pickup swing 1, CL 3 (+87, +132 s), Blizzard 1 (healer), LC 2 (slot 5 +121 s, row (2, 2) +136 s), then tanks +148 s, Thorim melee 19, Volley 3, US 2, LC 1, Nova 1 |
| uld2 (B) | wipe, hard (P2 2:56.4), 42.3% | LC 1 (slot 5, Evocation, +76 s), Volley 1 and Blizzard 1 (ring, self-res), tank +82 s (Blizzard), Thorim melee 23, US 2, LC 1 |
| uld5 (B) | wipe, hard (P2 3:08.9), 45.2% | phase 1 Smash 1; Frost Nova 1, Volley 1, Blizzard 1 (camp), tank +87 s (US), then Thorim melee 13, LC 3, US 2, CL 2, Frostbolt 2, Volley 1, Blizzard 1 |
| uld3 (C) | wipe, hard (P2 3:05.4), 33.1% | Frostbolt 1 (healer +17 s), LC 3 (slot 5 +61, +121 s ×2), Volley 1, Blizzard 3 (ring +78-81 s), CL 1, tanks +157 s, then Thorim melee 19, LC 1, Volley 1, US 1 |
| uld6 (C) | kill, normal | CL 1 |

**Hard mode in the kills: none** (uld6 read normal mode). **Stop rules:** waived for Thorim by the user's 2026-11-24
answer; not polished in any case (no hard-mode kill).

**Round 22 verdict (step points clear of the bunny's walk ahead): held on its points in A, B and C, not on the
deaths.** `--blizzard`: steps onto the path ahead 0/25, 0/35 (A), 0/28, 0/12 (B), 0/36 (C) against w31's 18-37 a
pull; under a zone within 8 s 0 everywhere (w31 11-25). Ring melee in zones 14.9 / 16.5 (A, w31 46.9 / 42.0), 29.1 /
29.3 (B, w31 uld5 21.3), 36.3 bot-s (C); longest stays 4.8-6.8 s against the hoped ≤ ~3; ring Blizzard deaths before
the first tank death 2 / 0, 1 / 0, 3 (w31 6 in all). Walk back p50 0.6-1.9 s (w31 0.3-2.6); `--chains` through a
stepped slot 1 crossing (uld4); melee on Thorim to +100 s 100%. Cause of what is left: 35 of 37 ring zone entries came
while stepped, walking at 2.4 yd/s (Blizzard or Frostbolt −65%) across the trail: uld3 Uldzay +30.6 s stepped 19.7 yd
west to (2092.6, −269.7) across the bunny's next leg, Frostbolt-slowed from the start, 4.5 s in zones; Uldzax +75.2 s
18 yd north to (2110.3, −229.4) back over the fresh trail, dead at +78.2 s. Rank 3 now.

**Failure picked: Lightning Charge on camp bots short of their shelter row** (8 of the 19, all before the first tank
death, against the ring's 6 Blizzard deaths).
- `--shelters`, w33 hard pulls: 62 walks before the first tank death, 11 hit, 8 dead (uld1 2, uld2 1, uld3 3, uld4 2);
  7 on slot 5 (rows (0, 5) (2113.0, −227.35) 25.6 yd, (1, 5) (2119.5, −226.85) 21.8 yd), 1 on row (2, 2) 23.6 yd.
- Trace: uld1 +41.1 s orb 1, Uldxau rooted by Frost Nova to +41.8 s, walks at 7.0 yd/s, enters a zone at +42.7 s and
  drops to 2.4 yd/s, 11.0 yd short at the cone (+46.1 s), dead with Uldxap (14.8 short). uld1 +131.1 s orb 0: Volley
  at +132.4 s, 3.5 yd/s, 7.3 / 11.7 yd short. uld2 +71.2 s: Uldyah channels Evocation; its move is accepted at +71.4 s
  but it stands until +75.8 s, dead at +75.9 s.
- 19 hard-mode pulls since w23: 146 walks before the first tank death (the opening left out), 51 short, 24 hit, 13
  dead, 11 of the 13 on slot 5's two rows. DBC: 62602 / 62576 aura 33 −65%, 62601 −65%, 62604 −50%, 62605 root.
- Code: `ThorimRangedSpot` (`UldEncounter_Thorim.cpp:870`) hands the row from `ULDUAR_THORIM_SHELTER_SPOTS`, whose
  budget (25.6 yd ≈ 3.7 s against 4.9 s, boss doc) assumed full speed and a prompt start.
- **Classification: bug.** The strategy intends the camp out of the cone before it lands; slowed or late, the long
  rows can't be reached.

**Fix** (`UldEncounter_Thorim.cpp/.h`, `UldActions_Thorim.cpp`, Thorim only, no size, mode or player branch):
`ThorimCampShelterInTime` compares the row's distance with run speed × time left (lit + 4.8 s − one scan), × 0.35 if
the walk crosses a zone not yet slowing the bot. Short and not rooted, `ThorimCampConeStep` (once per orb, latched in
`RangedShelter`) takes the nearest point within that reach (≤ 20 yd) 45° off the cone, 12-32 yd from him, 11 yd clear
of zones, its walk clear of them unless the bot stands in one, ties to the row; note `slot N orb M step`. The
positioning action breaks a ranged bot's channel while it stands in the lit cone. Replay over the 146 walks (no
navmesh, the floor cut at x 2100 and y −288, speed from the bot's slows, success = the bot moved the step's distance
by the cast, deaths within 1.5 s of it): 11 of 25 hit walkers and 6 of 16 dying ones out of the cone, of w33's 8
deaths 3 (+ the Evocation by the channel break); 5 currently safe walks modelled worse (2 bots that never moved, 2 a
yard or two short, 1 in the opening). Reader: `--shelters`,
tested in `test_thorim.py`; boss doc: Sif's slows, the step, the ring step's slowed walk, the section.

`pb-check` passes (compile, pblint, codestyle, botobs tests).

**Next window should show** (hard-mode pulls, `--shelters`):
- steps ≥ 1 a pull, nearly all "there"; camp cone deaths on walks before the first tank death well under w33's 8
  (≤ 4), hits on walks under 11; no walker standing through the warning on a channel.
- camp hit off a walk no higher than w33 (only in the opening's first two cones); `--chains` camp crossings no
  higher (w33: camp to slot 3, slot to camp 2).

**Seen, not picked:** the ring step's slowed walk across the trail (rank 3, next); casts delaying the walk 1-2.3 s (8
of 24 hits; the AI yields through a cast's preparing phase, so only a server spell script could interrupt it);
self-res back into the zone (rank 6, shared code); cones after the tanks fall with Thorim loose off the tank spot.

**Questions for the user:** none new; question 1 is answered (keep polishing, no end condition).

**Shared code touched:** none (Thorim files and the Thorim reader).
