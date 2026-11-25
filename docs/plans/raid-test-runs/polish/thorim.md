# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`, `--colossus`, `--chainwait`; `raidtest.py report` runs `--summary` on every counted pull). Rounds
1-27: `history/thorim.md`.

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
11-24 18:00 its p50 is +118 s (+134 s on R26's 9 pulls and R27's 6), and the kills held the tanks to +147-191 s or
past the kill; raid %/s before it 0.46-0.52. A gauntlet healer lost in phase 1 costs the most: first tank death
+45-137 s (p50 77) in hard mode, or a late phase 2 in normal mode. Normal mode: kills with 0-4 deaths.

**Compositions on build 37cc72a94 (window 46, round 27), hard mode in all six, all wipes.** A (uld1, uld4): 7.7%
(first tank death +147.5 s), 0.5% (+191.3 s). B (uld2, uld5): 35.8% (+107.1 s), 32.0% (+120.7 s). C (uld3, uld6):
8.8% (+163.6 s), 30.4% (+118.4 s). R26's build (window 44 + filler 45, 9 pulls): kills uld1 (12 deaths) and uld6
(11), wipes at 2.1-33.2%. Not polished on any one build.

**Open failures, ranked** (window 46, 189 deaths):
1. The race: Thorim melee 121 and Unbalancing Strike 7, nearly all after the first tank death. Not a bug; the user
   keeps it polishing with no end condition.
2. Chain Lightning 15, 14 before the first tank death (11 ring): slot to slot 6 (uld3 stepped slot 2 into slot 1
   at 2.5 yd ×3, uld4 ×2 at 5.8 yd and ×1 under lust), tank to slot 2, the opening 2, camp cone steps meeting a
   stepped slot's cone step 3 (uld6), camp seed 1. Over 58 hard-mode pulls 27 of 38 ring-crossing deaths (opening
   aside) came in casts whose bridge closed in the 2.5 s before the landing: moves on the lit orb. R28's target is
   the stepped and deadband share; the rest is Q1 below.
3. Lightning Charge 32, 3 before the first tank death (uld3 a ranged at +15 s, the opening; 2 camp +135 s).
4. Volley 7, Blizzard 5 (no ring melee; uld6 a camp ranged twice, self-res into the zone; tanks 2), Frostbolt 1
   (uld4 ring +152 s), Frost Nova 1.
5. Tank to slot chains: the solver's anchor term is angle-only, so a slot 60-68° off the anchor sits 5.4-8.6 yd
   from the holder; 5 deaths over 58 pulls.
6. Gauntlet healers at the Rune Giant hold (`ThorimGauntletTrailSpot`'s no-path fallback, w39 uld3/uld6): no phase
   1 death in windows 44-46.
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
  across it leaves before the bunny gets there — held (w46: ring in a zone 4-13 bot-s against 20-76, ring Blizzard
  deaths 0; cost: melee in reach before the first tank death 76% against 80%, raid %/s there −2%; kept).
- R28: a stepped ring bot clear of the hazards re-steps while another slot's bot or point is in its jump (slot-clear
  passes only, never across the lit cone); `RingBodyBridgesChain` counts another slot at its point too — pending
  (next window).

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
- Backing R27 out after w46's 0/6 (R28): its target held, first tank death and %/s unchanged, 0 of 6 kills against
  2 of 9 is within noise on one build.

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
- 2026-11-25, Q1 (a) of round 28: a ring walk or step that starts while the orb is lit (cone dodge, Blizzard re-step,
  walk back) and would end within the 8 yd jump of another group's body or point waits until the cast lands (cap
  1 s), only when the cone is still escapable after the wait; replayed before it ships. Build in R29.

**Open questions:** none.

## Round 28 (window 46: build 37cc72a94 with round 27)

**Data:** `window46-thorim.md` and `filler45-thorim.md` (session scratchpad; filler 45 holds window 44's six R26 pulls
plus three more on that build); traces `603_{13,16,14,17,15,18}_thorim_{1795603908,1795604060,1795603959,1795604071,
1795604041,1795604355}` (uld1, uld4, uld2, uld5, uld3, uld6) against R26's `603_{1,4,2,5,3,6}_thorim_17955985xx` and
`603_{28,30,29}_thorim_{1795601614,1795601936,1795601928}`, read with `thorim.py --blizzard/--chains` and session
scratchpad `thorim_r28/` scripts (`dps.py`: reach and %/s before the first tank death; `crossings.py`, `pair.py`,
`cone.py`: each chain crossing, who moved; `replay.py`: both triggers over the 58 hard-mode 25-man pulls since 11-24
18:00).

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | wipe, hard, 7.7% | CL 2 (tank to slot 2 +87 s, camp seed +132 s), then tanks from +147.5 s (US), Thorim melee 24, LC 2, US 1, Volley 1, Frost Nova 1 |
| uld4 (A) | wipe, hard, 0.5% | CL 3 (slot 2 to slot 1: +26.9 s under lust, +147.3 s at 5.8 yd ×2), Frostbolt 1 (ring +152 s), then tanks +191.3 s, Thorim melee 17, LC 9, CL 1, US 1, Volley 1 |
| uld2 (B) | wipe, hard, 35.8% | CL 1 (tank to slot 0 under lust +26.9 s), then tanks from +107.1 s (US), Thorim melee 21, LC 3, US 2, Volley 2, Blizzard 1 |
| uld5 (B) | wipe, hard, 32.0% | CL 2 (the opening, slot to tank to slot), then tanks +120.7 s, Thorim melee 20, LC 6, Volley 1, US 1 |
| uld3 (C) | wipe, hard, 8.8% | LC 1 (ranged +15 s, the opening), Volley 1 (+25 s), LC 2 (camp +135 s), CL 3 (stepped slot 2 into slot 1 at 2.5 yd, +162.2 s), then tanks +163.6 s, Thorim melee 17, LC 5, Volley 1, Blizzard 1 |
| uld6 (C) | wipe, hard, 30.4% | Blizzard 2 (a camp ranged, self-res into the zone, +81 s), CL 3 (camp cone steps meeting stepped slot 2's, +117.4 s), then tanks +118.4 s (Blizzard), Thorim melee 22, LC 4 |

**Stop rules:** waived for Thorim (user, 2026-11-24). Done rule on this build: no kill in any composition.

**Round 27 verdict (the bunny's next 8 yd counts as Blizzard): held in A, B and C; kept.** `--blizzard`: crossing
steps with the bunny p50 13.4 / 15.6 / 12.1 / 12.0 / 18.2 / 14.7 yd short (R26 5.6-10.8); ring melee in a zone 3.9 /
12.2 / 12.8 / 6.6 / 12.5 / 13.1 bot-s (uld1, uld4, uld2, uld5, uld3, uld6; R26's 9 pulls 20.4-75.6); ring Blizzard
deaths 0 (w44 6). Cost, before the first tank death (`dps.py`): melee within 9.08 yd of him 75.8% mean against 79.9%,
melee damage −4%, raid %/s 0.484 against 0.494; stepped out of reach 268 bot-s a pull against 233 (forecast +25). Not
worse where the race is decided: first tank death p50 +134 s against +133.5, Thorim %/s over the fight 0.42-0.46
against 0.42-0.52, wipes at 0.5-35.8% against R26's 2.1-33.2% plus 2 kills of 9; 0 of 6 against 2 of 9 is within
noise on one build. Watch the reach cost.

**Failure picked: Chain Lightning crossing ring slots after a re-solve, through a body that holds** (failure 2's
largest bug share; 14 of w46's 21 deaths before the first tank death were chains).
- uld3 +162.2 s: slot 2 (Uldzai, Uldzaw, Uldzax) stepped at +150.9 s to (2123.0, −249.3), bearing 31°, clear of slot
  1 at 326°. The orb lit at +160.9 s, slot 1 re-solved to 14° and walked to (2123.9, −251.7), 2.5 yd from slot 2;
  the cast ran slot 2 ×3 into slot 1 ×3, 3 dead. A stepped bot clear of zones, the bunny's walk and the cone returned
  from `ThorimRingBlizzardStep` before any body check (`UldEncounter_Thorim.cpp:3491` before this round).
- w44 uld6 +162.5 s: slot 2 re-solved 56° → 20° on the +146.4 s orb and stepped in place at +151.4 s; it held 11 s
  8.0 yd from slot 1 on its point, 3 dead.
- uld4 +147.3 s: lit +146.03 s, slot 1 326° → 18°, slot 2 56° → 78°; slot 1 arrived in ~1.1 s, slot 2's bots, 3.4 yd
  off their point inside the 5 yd deadband, moved at +147.8 s, after the landing: `RingBodyBridgesChain` (`:619`)
  counted slot 1 only where its bots stood. Here slot 2's new point was 7.1 yd from the holder, so it stays
  (failure 5).
- Replay over 58 pulls, trigger true ≥ 0.8 s before the landing: the stepped one before 4 slot-to-slot casts (9
  dead), the deadband one before 2 (3 dead, 1 under lust).
- **Classification: bug.** The ring keeps slots out of each other's jump (R6 deadband break, R21 slot-clear step);
  both checks went stale once the slots moved.

**Fix** (`UldEncounter_Thorim.cpp`, Thorim only, no size, mode or player branch): `ThorimRingBlizzardStep` lets a
stepped bot clear of the hazards hold only while no other slot's bot or point (`RingStepChainBodies`, the camp aside)
lies in its jump; else it re-steps through the slot-clear passes only and holds when they find nothing.
`RingBodyBridgesChain` also counts another slot at its point (`RingSlotPoint`, shared with `RingStepChainBodies`).
Reader `--blizzard`: stepped bot-seconds in another living slot's jump (tested in `test_thorim.py`); boss doc: "It
breaks for a Chain Lightning bridge too", "Clear at the step is not clear for the stay". `pb-check` passes (syntax,
pblint, codestyle, botobs). Review: the re-step skips a point whose walk crosses the lit cone (R24's rule; it fires
on the lit orb), and the reader counts a slot's point only while that slot has a living floor bot (uld3 47.7 → 31.2).

**Next window should show** (hard-mode pulls):
- `--chains` "through a stepped slot: slot to slot" deaths 0 (w44-w46: 9 in 15 pulls).
- `--blizzard` "in another slot's jump" well under w46's 40.1 / 19.4 / 6.3 / 3.5 / 31.2 / 4.7; steps a few more,
  out of reach within ~10 bot-s of w46's 263 / 315 / 238 / 207 / 307 / 282.
- Ring chain deaths before the first tank death under w46's 11; R27 holding (ring in a zone ≤ ~13 bot-s, no ring
  Blizzard death); melee in reach and %/s before the first tank death not lower.

**Seen, not picked:** tank to slot chains (failure 5); camp and ring cone steps converging on one pocket (uld6 +117 s,
f45 uld5 +117 s: neither sees the other's destination); slots at 60° under lust (8.8 yd against the 8.9 jump); the
opening chain (uld5).

**Questions for the user:** Q1 in State. 27 of 38 ring-crossing chain deaths over 58 pulls came in casts whose bridge
closed in the 2.5 s before the landing, from moves on the lit orb (ring re-solve walks, re-steps, camp cone steps);
the chain lands ~1.05 s after the lit and the cone at ~4.9 s, so a ring walk can wait for the chain and still clear
the cone, as R26's camp wait does. Recommended (a).

**Shared code touched:** none (Thorim files and the Thorim reader).
