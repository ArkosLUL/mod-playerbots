# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`, `--colossus`; `raidtest.py report` runs `--summary` on every counted pull). Rounds 1-24:
`history/thorim.md`.

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
Chain Lightning every 15 s, cast within 0.8 s of an orb lighting with a 0.5-0.6 s cast bar naming its first target,
landing 0.4-1.4 s after the lit, 8 targets at ×1.5 a hop, hop 8 yd (8.9 when both carry Heroism/Bloodlust); hard mode
adds Sif's Frostbolt Volley (−50% speed), Frostbolt (−65%), Blizzard (−65% in a zone) and Frost Nova (root).
Blizzard's bunny walks the core's fixed path once at ~7 yd/s (west, then north of the tank spot, then round the camp),
dropping a 10 s zone every 2 s.

**Rosters.** From window 34 every pool carries all eleven professions at 450 with their perks and enchants (DC
`.dc test roster professions`): compare only windows from 35 on.

**What kills raids.** Hard mode at ilvl 226 is a race: the raid takes him ~0.5 %/s, and the tanks fall to his melee,
which grows with the Lightning Charge stacks (14-18k a swing by +140 s), plus Blizzard on both tanks (holder and waiting
tank 31-154k each a pull in w37); most deaths come after, with Thorim loose. The first tank death decides it: over the
hard-mode pulls since 11-23 its p50 is +119 s, and the kills held the tanks to +167-191 s or past the kill. W37 A
wiped twice at 4.0% with 0 and 2 deaths before the tanks fell (+152 / +141 s). A gauntlet healer lost in phase 1 (the
Colossus' Smash, R25) brought it to +45-137 s (p50 77) in six hard-mode pulls. Normal mode: kills with 0-4 deaths.

**Compositions on build ce636f992 (window 37, round 24).** A (uld1, uld4): wipes at 4.0% / 4.0%, first tank death
+152.4 / +141.0 s. B (uld2, uld5): uld2 a normal-mode kill, 3 deaths (phase 2 at 3:58.8, 62565 first); uld5 wipe at
45.2%, a gauntlet healer dead to Smash, first tank death +66.8 s. C (uld3, uld6): wipes at 60.7% / 32.1%, first tank
death +44.8 s (both gauntlet healers dead to Smash) / +123.1 s. On the window 35 build hard-mode kills came in A (uld4,
4 and 3 deaths) and C (uld3 16, uld6 7); B has never killed in hard mode. Not polished on any one build.

**Open failures, ranked** (window 37, 161 deaths):
1. The race: Thorim melee 104 and Unbalancing Strike 10, nearly all after the first tank death. Not a bug; the user
   keeps it polishing with no end condition.
2. Gauntlet healers stacked on the Colossus' target (R25's pick): uld3 2 and uld5 1 dead to Smash, first tank deaths
   +44.8 / +66.8 s, 40 of the 104 Thorim melee deaths in those two pulls.
3. Chain Lightning 11: the opening under lust (uld6 +26.9 s, 2 melee), uld4 1 camp (+132.5 s), 8 after the tanks.
   Camp chains through shelter walkers: question 1.
4. Lightning Charge 9: uld6 2 at +120.9 s and uld4 1 camp at +135.6 s before the first tank death, 6 after.
5. Blizzard 4 and Volley 9: uld6 a ring melee 4.1 s in a zone (+79.6 s) and two healers (+80.5-82.6 s), Volley
   on uld6 2 and uld3 1 before the first tank death. Frostbolt 2, Frost Nova 0. Nothing to kick.
6. Phase 1: Stomp 2 (uld1 melee; uld3 a revived healer walked to 2.2 yd of the Giant by `reach spell`), Smash on a melee
   within ~2 yd of the Colossus (uld2), Iron Ring Guard, Honor Guard Cleave, Champion Whirlwind 1 each.
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
  steps past 45° anyway — held (w37 ring cone deaths before the first tank death 1 against w35's 5).
- R25: gauntlet healers trail to 15 yd short of the leader along the path (`ThorimGauntletTrailSpot`) — pending
  (window 38).

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

**Open questions:**
1. Camp shelter walks bridge Chain Lightning (round 24). Choice: (a) a camp bot whose shelter walk passes
   within the 8 yd jump of another slot's body waits to set off until Thorim's Chain Lightning has landed (its cast
   bar shows, 0.5-0.6 s), taking R23's cone step when the late start leaves the row out of reach; (b) re-solve the
   orb 1 and orb 2 shelter rows with navprobe so no two walks pass within 8 yd; (c) leave it. Recommended: (a), no
   new positions and R23's step covers late walkers; replayed before it ships.

## Round 25 (window 37: build ce636f992 with round 24)

**Data:** `window37-thorim.md` (session scratchpad), one valid pull per pool; traces
`603_{23,24,21,22,27,30}_thorim_{1795556848,1795556868,1795556724,1795556806,1795557202,1795557334}` (uld1, uld4,
uld2, uld5, uld3, uld6), read with `thorim.py --ringcone/--blizzard/--deaths`, the new `--colossus`, `postmortem.py
--death/--bot`, and session scratchpad `thorim_r25/` scripts (Thorim's health and the first tank death over the
75 pulls on disk since 11-23, phase 1 deaths, the Smash geometry and the healers' last moves, a stand-off replay).

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | wipe, hard, 4.0% | P1 Stomp 1 (melee); none in phase 2 until the tanks at +152 s; Thorim melee 22, CL 3, Volley 2, LC 2, Frostbolt 1, US 1 |
| uld4 (A) | wipe, hard, 4.0% | CL 1, LC 1 (camp, +132-136 s), then tanks +141 s, Thorim melee 26, US 3, LC 1, Volley 1 |
| uld2 (B) | kill, normal, 3 | P1: Iron Ring Guard melee, Smash (melee within 2 yd), Cleave (healer) |
| uld5 (B) | wipe, hard, 45.2% | P1 Smash 1 (gauntlet healer); Whirlwind 1 (+46 s), tank +66.8 s (US), Thorim melee 19, CL 3, US 2, Champion 3, Volley 1 |
| uld3 (C) | wipe, hard, 60.7% | P1 Smash 2 (both gauntlet healers), Stomp 1 (one revived); Volley 1, tank +44.8 s (melee, 9 Blizzard ticks), Thorim melee 20, US 2, Volley 1, Blizzard 1, CL 1, LC 1 |
| uld6 (C) | wipe, hard, 32.1% | CL 2 (+26.9 s lust), Volley 2, Blizzard 3, LC 2 before the tanks at +123 s; Thorim melee 16, LC 2, CL 1, US 1, Volley 1, Frostbolt 1 |

**Stop rules:** waived for Thorim (user, 2026-11-24). Done rule on this build: B has a kill, but in normal mode; A and C
none.

**Round 24 verdict (ring walks held across the lit cone): held in A, B and C.** `--ringcone`, before the first tank
death: hits 2 / 0 (A), 0 (uld5; uld2 normal 0), 0 / 2 (C); deaths 0 / 0, 0, 0 / 1 (uld6 Uldwaj `blocked`, +116.0 s),
1 against w35's 5. No stepped bot died standing past reach (uld1 +11.1 s one hit, alive). Held walks 3 / 2 / 0 / 0 / 4;
2 of the 9 were hit after the hold released (uld1 +11.1 s alive, uld6 +131.3 s dead after the tanks). Melee on Thorim to
+100 s 99.7-100%. Ring melee in Blizzard 11.3-32.9 bot-s a pull (w35 0.9-54.3); one ring Blizzard death before the first
tank death (uld6 Uldwat, 4.1 s in a zone, +79.6 s; w35 0).

**Failure picked: gauntlet healers standing on the leader in the Runic Colossus' Smash** (the race is no bug; this
loss set the two earliest collapses).
- uld3: at 1:31.9 Uldzaf and Uldzae took `thorim gauntlet positioning action` to (2229.28, −389.65), the leader
  Uldzab's own spot 4.4 yd south of the Colossus, and stood there 18 s; Smash at 1:49.9 hit both (24k, 29k), the tank
  and a melee within 1.9 yd; both healers dead. Phase 2 had 2 of 4 healers: first tank death +44.8 s, wipe at 60.7%.
  uld5 the same (1:22.9 walk, 1:47.0 Smash): Uldvaf dead, first tank death +66.8 s, wipe at 45.2%.
- `--colossus` over 53 pulls with a Smash (61 casts) since 11-23: in 10 both gauntlet healers stood on his target
  (4.4-4.5 yd, 0° off his facing), every time both hit, 12 dead; healers 7-13 yd behind him were never hit. Hard mode
  6 of them, first tank death +44.8-137.3 s (p50 77) against p50 119 in hard pulls without the loss. In all ten the
  healers' last move was the trail onto the leader from the y −360 pull line (28.9-31.3 yd).
- Code: the trail branch of `ThorimGauntletPositioningAction::Execute` (`UldActions_Thorim.cpp:232`) walked to the
  leader's exact spot; `ThorimTrailsGauntletLeader` (`UldEncounter_Thorim.cpp:1761`) only gates the start at 20 yd.
- **Classification: bug.** The 20 yd tether keeps healers in range of the leader; nothing intends them on the tank.

**Fix** (`UldEncounter_Thorim.cpp/.h`, `UldActions_Thorim.cpp`, Thorim only, no size, mode or player branch):
`ThorimGauntletTrailSpot` (`:1774`) gives a trailing healer the path to the leader cut 15 yd short
(`ULDUAR_THORIM_GAUNTLET_HEALER_STANDOFF`, asserted under the tether so the trail doesn't re-fire; core
`ShortenPathUntilDist`), the leader's spot when no normal path exists; non-healers still walk to him (the picker hands
them his target there). Replayed on the ten pulls: all twenty stops land 10.6 yd behind the Colossus (176-180° off his
facing). Reader `--colossus`, tested in `test_thorim.py`; boss doc: the Smash row, the rule and the evidence. `pb-check`
passes (syntax, pblint, codestyle, botobs).

**Next window should show** (`--colossus`, every pull):
- no healer within 6 yd of his target at a Smash, gauntlet healer Smash hits and deaths 0 (w37: 4 hit, 3 dead in 2 of 6).
- gauntlet healers 10-15 yd behind him; no healer death in the corridor or at the Giant from falling out of heal range,
  phase 2 still within Sif's clock in the hard-mode pulls (w37 5 of 6).
- Watch: Stomp and Rune Detonation on healers at the Giant hold (they now stop 15 yd short of the leader there too);
  first tank deaths no earlier than +100 s where no healer died in phase 1.

**Seen, not picked:** the A pulls losing the race by 4% with nothing dying before the tanks; the opening chain under
lust (uld6); uld6's healers in Blizzard; Smash on melee standing inside the Colossus (survived); question 1.

**Questions for the user:** none new; question 1 (State) stays open.

**Shared code touched:** none (Thorim files and the Thorim reader).
