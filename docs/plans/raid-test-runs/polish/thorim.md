# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`, `--colossus`, `--chainwait`; `raidtest.py report` runs `--summary` on every counted pull). Rounds
1-26: `history/thorim.md`.

## State

**Fight model.** The start pack (a mock fight in the arena) dies first; Sif casts 62507 20 s after its last death, and
phase 2 must start within 150 s of that or she casts 62565 (normal mode: Thorim −30% health, no Sif). Phase 2 starts
only on a player's hit on Thorim from above z 430, allowed once the Rune Giant dies. The arena squad holds the adds; the
bot gauntlet squad (12, two healers) pulls the lever, clears the corridor's two trash packs under the Runic Smash lanes,
kills the Runic Colossus (the leader tanks him ~4.4 yd south of him; Smash 62339, a 3 s cast at his target every
15-22 s, 37k in a 10 yd frontal cone), the ramp's Honor Guards and Acolyte and the Rune Giant (held at
(2187.79, −432.78)), walks the balcony and hits him from `BALCONY_5`. No phase 2 by phase 1 + 5 min wipes the raid to
the Lightning Orb. Lever to 62565 is ~170 s; what decides it is Runic Barrier on the Colossus (+11-12 s when it lands
early), the corridor under the smash, and lane flips. Phase 2: tanks hold him on the tank spot (2110.75, −252.65), the
waiting tank on the off-tank spot (2107.25, −252.65), a swap on every Unbalancing Strike; melee on a 3-slot ring (r 8.8,
slots ≥60° apart), ranged on 6 camp spots with shelter rows from the Lightning Charge cone (lit orb 4.6-4.9 s ahead);
Chain Lightning every 15 s with a 0.5-0.6 s cast bar naming its first target, the cast starting p50 0.5 s (max 1.5)
after the lit stamp and landing p50 1.05 s after it, a third of the events casting nothing; 8 targets at ×1.5 a hop,
hop 8 yd (8.9 when both carry Heroism/Bloodlust); hard mode adds Sif's Frostbolt Volley (−50% speed), Frostbolt
(−65%), Blizzard (−65% in a zone) and Frost Nova (root). Blizzard's bunny walks the core's fixed path once at ~7 yd/s
(west, then north of the tank spot, then round the camp), dropping a 10 s zone every 2 s; two ring slot points sit
~5 yd off its west and north legs, so those slots step out on every Blizzard.

**Rosters.** From window 34 every pool carries all eleven professions at 450 with their perks and enchants (DC
`.dc test roster professions`): compare only windows from 35 on.

**What kills raids.** Hard mode at ilvl 226 is a race: the raid takes him ~0.4-0.55 %/s, and the tanks fall to his
melee, which grows with the Lightning Charge stacks (14-18k a swing by +140 s), plus Blizzard and Sif's Volley on both
tanks; most deaths come after, with Thorim loose. The first tank death decides it: over the 49 hard-mode pulls since
11-24 18:00 its p50 is +118 s, and the kills held the tanks to +147-191 s or past the kill. A gauntlet healer lost in
phase 1 costs the most: first tank death +45-137 s (p50 77) in hard mode, or a late phase 2 in normal mode. Normal
mode: kills with 0-4 deaths.

**Compositions on build 229335cd3 (window 44, round 26), hard mode in all six.** A (uld1, uld4): uld1 a kill, 12
deaths (phase 2 at 2:58.2, first tank death +147.1 s); uld4 wipe at 2.1% (+156.5 s). B (uld2, uld5): wipes at 33.2%
(+114.0 s) and 19.1% (+119.9 s). C (uld3, uld6): uld3 wipe at 23.2% (+67.1 s); uld6 a kill, 11 deaths (+187.5 s, six
to Chain Lightning). Filler 43 (round 25's code, the build before): uld4 a hard-mode kill with 4 deaths, uld5 a
normal-mode kill with 2, the rest hard-mode wipes. Not polished on any one build.

**Open failures, ranked** (window 44, 169 deaths):
1. The race: Thorim melee 105 and Unbalancing Strike 11, nearly all after the first tank death. Not a bug; the user
   keeps it polishing with no end condition.
2. Chain Lightning 9, 8 before the first tank death (7 on the ring): uld6 camp ×2 (+132 s), stepped slot 2 into
   slot 1 at 8.0 yd ×3 (+162 s), slot 0 seed (+177 s); uld1 tank to slot 0 (+132 s); uld4 and uld3 within the seed
   slot (+117, +57 s). Over 49 hard-mode pulls the ring's 35 split slot to slot 15, tank to slot 10, seed slot 6,
   camp to slot 4: no single cause.
3. Blizzard 9: ring melee 6 before the first tank death, every one after a step across the bunny's path (R27's
   target); uld2's first tank death; a uld4 healer.
4. Lightning Charge 10 (2 before the first tank death: uld2 ring +90 s, uld4 camp +60 s).
5. Volley 1, Frostbolt 1, Frost Nova 1.
6. Gauntlet healers at the Rune Giant hold (`ThorimGauntletTrailSpot`'s no-path fallback, w39 uld3/uld6): not seen
   in window 44 (no phase 1 death); filler 43 uld5 lost a ranged to corridor Runic Smash.
7. Self-res into the hazard: shared res code; not picked.

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
- R15: cannot-reach step for a floor bot Thorim can't reach — held.
- R16: corridor flee hold, Revive Pet hold on Sif's clock — held.
- R17: covered ring melee step out of Blizzard, `avoid aoe` held off ring melee (user) — held.
- R18: the swap fires only within 8 yd of the tank spot — held.
- R19: class taunts from past 8 yd held while he's on a non-tank or nobody, anchor exempt — held.
- R20: the anchor with its taunt cooling gets no spot while he's loose, so it closes on him — held.
- R21: the Blizzard step's lead passes stay out of the jump of the camp and the other slots — held.
- R22: the step's first round stays 11 yd off the bunny's remaining walk — held.
- R23: a camp bot short of time for its shelter row takes a cone step 45° off the cone; a camp bot in the lit cone
  breaks its channel — held.
- R24: a ring walk that would cross the lit cone holds; a stepped ring bot in the lit cone with no zone-clear point
  steps past 45° anyway — held.
- R25: gauntlet healers trail to 15 yd short of the leader along the path (`ThorimGauntletTrailSpot`) — held on the
  Colossus; its no-path fallback is failure 6.
- R26: a camp shelter walk for orb 1 or 2 that passes another slot's body waits for Chain Lightning
  (`ThorimCampChainWait`, cap 1 s, reach guard, cast hold) (user) — held (w44: 75 waits, camp chain deaths under
  orbs 1-2 0, chains of 4+ camp hits 0, no %/s loss).
- R27: the bunny's next 8 yd of path counts as Blizzard for the ring (`ULDUAR_THORIM_RING_BLIZZARD_LEAD`), so a step
  across it leaves before the bunny gets there — pending (next window).

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
- The chain wait without a reach guard or with a 1.5 s cap (R26 replay): 22 walks newly in the cone before the first
  tank death against 5 saved; 9 against 2 with the guard at 1.5 s (shipped: 5 against 5).
- The ring step kept on its own side of the bunny's path (R27 replay): 204 of 341 crossing orders moved into the
  camp's jump, 68 into another slot's; preferred only at equal chain clearance it changed 110 orders worth 111 ticks.
- A 10 yd ring lookahead instead of 8 (R27 replay): 37 bot-s fewer in zones for 385 more out of reach over 49 pulls.

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
- 2026-11-25, Q1 (a) of round 24: a camp bot whose shelter walk for orb 1 or 2 passes within the 8 yd jump of
  another slot's body waits to set off until Thorim's Chain Lightning has landed, taking R23's cone step when the late
  start leaves the row out of reach; replayed before it ships. Built in R26.

**Open questions:** none.

## Round 27 (window 44: build 229335cd3 with round 26)

**Data:** `window44-thorim.md` and `filler43-thorim.md` (session scratchpad; filler 43 is six pulls of round 25's code
on the build before); traces `603_{1,4,2,5,3,6}_thorim_{1795598584,1795598581,1795598596,1795598597,1795598596,
1795598599}` (uld1, uld4, uld2, uld5, uld3, uld6), read with `thorim.py --chainwait/--chains/--blizzard`,
`postmortem.py --death`, and session scratchpad `thorim_r27/` scripts over the 49 hard-mode pulls since 11-24 18:00
(`step_walks.py`, `replay2.py`, `lookahead.py`).

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | **kill, hard, 12** | Blizzard 1 (ring, crossing step, +114 s), CL 1 (tank to slot 0, +132 s), then tanks +147 s, Thorim melee 8, US 2 |
| uld4 (A) | wipe, hard, 2.1% | Blizzard 3 (ring, crossing steps, +45/+77/+79 s), LC 1 (camp +60 s), CL 1 (seed slot +117 s), then tanks +157 s, Thorim melee 21, LC 2, Blizzard 1, US 1, Frostbolt 1 |
| uld2 (B) | wipe, hard, 33.2% | LC 1 (ring +90 s), then tanks +114 s (Blizzard, melee), Thorim melee 23, LC 2, Blizzard 1, US 2 |
| uld5 (B) | wipe, hard, 19.1% | US 1 (ring +26 s), Blizzard 2 (Uldvak, crossing step, +115 s), then tanks +120 s, Thorim melee 25, US 1, LC 1, Frost Nova 1 |
| uld3 (C) | wipe, hard, 23.2% | pickup swing 1, CL 1 (seed slot +57 s), then tanks +67 s, Thorim melee 25, LC 3, US 2 |
| uld6 (C) | **kill, hard, 11** | pickup swing 1, Volley 1 (camp +24 s), CL 6 (camp ×2 +132 s, stepped slot 2 → slot 1 ×3 +162 s, slot 0 +177 s), then tanks +187.5 s, US 2, Thorim melee 1 |

**Stop rules:** waived for Thorim (user, 2026-11-24). Done rule on this build: no composition has a hard-mode kill
with at most 5 deaths (uld1 12, uld6 11).

**Round 26 verdict (camp shelter walks wait for Chain Lightning): held in A, B and C.** `--chainwait`: 75 waits on
orbs 1-2 (A 24, B 29, C 22), released by the landing 32 and by the 1 s cap 43, wait p50 0.7-1.1 s, max 1.5; camp
chain deaths under orbs 1-2 0 (w39 2), chains of 4+ camp hits there 0 (filler 43 1); waiters hit by the chain 2
standing (uld5), 0 walking; cone on waiters 2 hit, 1 dead, none before the first tank death. Thorim %/s over the
first 30 s 0.32-0.44 (filler 43 hard 0.35-0.41), whole fight 0.42-0.52 (0.43-0.55): no loss. Filler 43 had no camp
chain death under orbs 1-2 either, so the gain is thin on one window. Watch: the cap releases more than the landing.

**Failure picked: ring melee dying to Blizzard after a step across the bunny's path** (6 of w44's 21 deaths before
the first tank death; failure 2's ring chains are larger but split over four crossing kinds with no one cause).
- uld4 Ulduat +74.27 s: on slot 2's point (2117.0, −246.4), 5.2 yd off the bunny's P3-P4 leg, the step fired with
  the bunny 9.0 yd away and ordered (2110.2, −229.8), across that leg; in zones from +74.4 s at 2.4 yd/s, the bunny
  1.9 yd off at +75.5 s, 3 ticks and a Volley, dead +77.4 s. Ulduay +34.22 s the same (order (2112.2, −229.2), bunny
  1.7 yd off at +35.5 s, 56% health), then a cone exit at +41.2 s back through the zones, dead +44.9 s.
- 49 hard-mode pulls, before the first tank death: 341 of 1,318 step orders crossed the bunny's path ahead (the
  bunny p50 ~8 yd short of the crossing, every order within 20 yd), taking 486 ticks and 11 deaths; 582 walks clear
  of it took 7 ticks; 16 of the 20 ring Blizzard deaths came within 10 s of a crossing step.
- Code: `ThorimRingBlizzardStep` and `ThorimRingBlizzardCovered` (`UldEncounter_Thorim.cpp`) act only within 11 yd
  of a zone or the bunny, ~1 s before it reaches the slot; pass 0 (clear of camp, slots, tanks and the path ahead)
  lies on the far side when the near side holds the camp or the next slot; the sweep checks the point, not the walk.
- **Classification: bug.** The step exists to get covered melee out of Blizzard (user, 2026-11-24); it starts too
  late for the walk its own passes choose. The boss doc listed the walk as unfixed.

**Fix** (`UldEncounter_Thorim.cpp/.h`, Thorim only, no size, mode or player branch; ring bots only, so a real
player is untouched): `ThorimRingBlizzardAhead` tests a spot against the bunny's next
`ULDUAR_THORIM_RING_BLIZZARD_LEAD` (8) yd of path at 11 yd; it covers a slot point in `ThorimRingBlizzardCovered`,
starts the step in `ThorimRingBlizzardStep`, and rules step points out in every pass. `ThorimBlizzardLegOf` factors
the leg lookup out of `ThorimBlizzardPathAhead`. Reader `--blizzard` counts steps whose walk crosses the bunny's path
ahead, the bunny's p50 path yards short of the crossing and ticks until the bot's next order (test in
`test_thorim.py`); boss doc: "Covered melee step out" and "A walk across the bunny's path leaves before the bunny
gets there". `pb-check` passes (syntax, pblint, codestyle, botobs).

**Replay** (`lookahead.py`: each ring bot's first step per bunny before the first tank death, 774 steps; departure
when the bunny's next N yd come within 11 yd of the bot, the same step point, 7 yd/s, 2.45 inside a zone's 9.8 yd
reach, against the zones the trace laid):

| Lead | zone bot-s, crossing steps (331) | zone bot-s, other steps (443) | more out of reach, bot-s |
|---|---|---|---|
| shipped (none) | 1,128 | 323 | - |
| 6 yd | 271 | 230 | 980 |
| **8 yd** | **147** | **229** | **1,281** (~26 a pull, ~1.7 s a step) |
| 10 yd | 110 | 229 | 1,666 |

Of the 147 left, 122 come from steps within 4 s of the bunny's spawn, which leaves no lead (196 such crossing steps,
545 bot-s shipped); the later crossings fall 583 → 25. Rejected alternatives in State (own-side step, 10 yd).

**Next window should show** (hard-mode pulls, `--blizzard`):
- crossing steps with the bunny p50 ≥ ~15 yd short of the crossing (w44 6.2-10.8) and their ticks well under w44's
  10 / 23 / 12 / 46 / 11 / 18 (uld1, uld4, uld2, uld5, uld3, uld6).
- ring melee in a zone under w44's 25.6 / 51.0 / 20.4 / 75.6 / 44.8 / 31.2 bot-s, ring Blizzard deaths before the
  first tank death ≤ 1 a window (w44 6).
- the cost: stepped out of reach up ~25 bot-s a pull (w44 165-325), melee on Thorim to +100 s still ≥ 99%, Thorim
  %/s not lower.

**Seen, not picked:** failure 2's ring chains (four crossing kinds); uld6's stepped slot 2 within 8.0 yd of slot 1
at +162 s; the cone exit walking back through live zones (uld4 Ulduay +41 s, 1 death in 49 pulls); R26's cap
releasing more waits than the landing.

**Questions for the user:** none.

**Shared code touched:** none (Thorim files and the Thorim reader).
