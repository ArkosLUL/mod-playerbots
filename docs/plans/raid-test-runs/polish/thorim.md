# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`; `raidtest.py report` runs
`--summary` on every counted pull). Rounds 1-20: `history/thorim.md`.

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
Blizzard and Frost Nova.

**What kills raids.** Hard mode at ilvl 226 is a race the rosters lose: from +30 s the raid takes him 0.41-0.55 %/s
(a kill by the first tank death needs ~0.74), and the tanks fall at +46-196 s to his melee, which grows with the
Lightning Charge stacks, plus Blizzard (22-27% of tank intake); every hard-mode phase 2 on disk (19) wiped, most deaths
being Thorim loose after both tanks fall. Before the first tank death: Chain Lightning bridging groups, Volley and
Frostbolt on healers, Blizzard on camp walks, Lightning Charge. Normal mode: kills with 0-4 deaths.

**Compositions on build 897faf0d8 (window 29).** A (uld1, uld4): hard-mode wipes, Thorim 35.1% / 18.9%, first tank
death +118.4 / +118.5 s. B (uld2, uld5): uld2 normal kill, 4 deaths (all phase 1); uld5 hard wipe at 56%, first tank
death +47.4 s. C (uld3, uld6): uld3 normal kill, 1 death; uld6 hard wipe at 57%, first tank death +45.9 s. Hard mode
reached in 4 of 6; no hard-mode kill, so not polished.

**Open failures, ranked** (window 29, 162 deaths):
1. The hard-mode race: Thorim melee 65, nearly all after the first tank death. Not a bug; question 1.
2. Chain Lightning 21, 8 before the first tank death. By crossing over the ten hard-mode wipes since round 17
   (`--chains`): through a slot stepped out of Blizzard 16 of 44 (round 21's fix); a lit-orb cone dropping the 60°
   separation (w26 uld3 2, w29 uld6 1); camp RANGE4 (2132.5, −257.5) 8.8 yd from slot 1 under Heroism (w29 uld1
   +27 s, 3); the opening into slot 2 beside the holder (w29 uld1 +12 s, 2); stacked camp spots as the seed.
3. Sif's Volley and Frostbolt on healers before the first tank death (w29 uld1 4, uld6 1). Nothing to kick.
4. Blizzard on camp and healers (w29 uld4 a healer twice at +80 s, uld1 one at +46 s).
5. Early tank deaths, +46-47 s (w29 uld5, uld6): Thorim melee 46-50%, Blizzard 16-31%, Lightning Charge 11%.
6. Phase 1: the Runic Colossus' in-combat Smash 62339 (10 yd frontal cone, documented, not handled) on gauntlet
   healers, 4 in two windows (w26 uld2, w29 uld2, w29 uld6 two at once); Rune Detonation 2.

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
- R15: cannot-reach step for a floor bot Thorim can't reach — held (fired once: w29 uld6 1.6 s, one step).
- R16: corridor flee hold, Revive Pet hold on Sif's clock — held.
- R17: covered ring melee step out of Blizzard, `avoid aoe` held off ring melee (user) — Blizzard ring deaths stopped;
  its points bridged slots (R21).
- R18: the swap fires only within 8 yd of the tank spot — held.
- R19: class taunts from past 8 yd held while he's on a non-tank or nobody, anchor exempt — held.
- R20: the anchor with its taunt cooling gets no spot while he's loose, so it closes on him — held (his melee on
  non-tanks before a tank had him 0-8k a pull, was 348k over 37 pulls).
- R21: the Blizzard step's first pass stays out of the jump of the other slots — pending (window 30).

**Tried and rejected:**
- Balcony runner: the user ruled it out (phase 2 with the squad deep in the corridor).
- The waiting tank stepping out of Blizzard: user no (R18); R19 found no step point out of hop range for a whole sweep.
- A whole-ring rotation off the cone: built and deleted (two whole-ring turns per orb cycle; boss doc).
- The solver trading bearings for Blizzard: removed in R17 (swung slots onto each other).
- A bearing threshold for the ring re-latch: left 16-29% (R13).
- Moving the tank anchor east: breaks 11 of 18 camp points (boss doc).
- The Blizzard step also clear of camp bodies: no point for half the steps in a replay (R21).

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

## Round 21 (window 29: build 897faf0d8 with round 20)

**Data:** window 29, one valid pull per pool; traces
`603_{14,11,10,12,15,13}_thorim_{1795538542,1795538513,1795538450,1795538525,1795538867,1795538534}` (uld1-uld6), read
with `thorim.py` (`--chains` new), `postmortem.py --death` and session scratchpad `thorim_r21/` scripts. Chains also
over the ten hard-mode phase 2s since round 17 (w25 uld1/2, w26 uld1/3, w27 uld1/2, w29 uld1/4/5/6).

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | wipe, hard (P2 3:11.4), 35.1% | CL 7 (+12 s 2, +27 s 3), Sif Volley/Frostbolt 4 by +46 s, then Thorim melee 18 after the tanks |
| uld4 (A) | wipe, hard (P2 2:58.7), 18.9% | Blizzard 2 (a healer, +80 s), then the tanks +118 s, CL 5 in the collapse, Thorim melee 19 |
| uld2 (B) | kill, normal (62565 3:14.4) | phase 1: Runic Smash, Colossus Smash, Honor Guard Cleave, Rune Detonation |
| uld5 (B) | wipe, hard (P2 3:12.0), 56% | CL 3 (+42 s), tank +47 s (Unbalancing Strike), then Thorim melee 23 |
| uld3 (C) | kill, normal (62565 3:22.3) | CL 1 (the opening, +12 s) |
| uld6 (C) | wipe, hard (P2 3:05.8), 57% | phase 1: Colossus Smash 2 (both gauntlet healers, 1:51), Rune Detonation 1; P2: Champion 1 (+6 s), Volley 1, tank +46 s (Lightning Charge), then Champion 5, Frostbolt 4, CL 4, Thorim melee 6 |

**Hard mode in the kills: none.** Both kills (uld2, uld3) read normal mode: Sif cast 62565 at 3:14.4 / 3:22.3, no
`thorim.sifjoined`. The four hard-mode pulls (uld1, uld4, uld5, uld6) all wiped.

**Stop rules:** not polished (no hard-mode kill in A, B or C). Not stuck: rounds 18-21 picked four different causes,
each fix held. Question 1 stays open; no new stop rule applied.

**Round 20 verdict (the anchor closes on a loose Thorim while its taunt cools): held in A, B and C.** A tank on him at
+0.6 / +1.8 s (A), +0.8 / +0.8 s (B), +3.7 / +3.0 s (C); his melee on non-tanks before it 0 / 7k, 8k / 0, 0 / 0 (w27
uld1 36k; 348k over 37 cooling pulls before). In uld4, uld3 and uld6 the anchor's first moves were `reach melee` toward
him (to x 2142-2156 in C, where he walked east after a melee) and the walk to the tank spot came only after it held
him (+1.8 / +3.9 / +3.1 s); no landing death. Rounds 15-19's checks hold: one cannot-reach span (uld6, 1.6 s, one
step); first trades from 7.9-9.4 yd, Thorim within 8.5 yd of the spot; no ring Blizzard death before a tank's.

**Failure picked: Chain Lightning crossing into another ring slot through a slot stepped out of Blizzard.** The race
(rank 1) is no bug; Chain Lightning is the top mechanic before the first tank death.
- Trace: uld5 +30.17 s slot 0 (Uldvan, Uldvam, Uldvar) stepped to (2117.83, −265.20), 1.9 yd from slot 1's three
  melee on their point (2118.7, −263.5), and held there to +42.3 s; the
  +42.0 s cast ran camp ×3 → 8.5 yd (Heroism) → slot 0 ×3 → 1.9 yd → slot 1 ×2, killing Uldvam (32.7k), Uldvax and
  Uldvag (36.8k, 66.0k); the first tank fell 5 s later. uld6 +83.2 s slot 2 stepped within 4.8 yd of slot 1; the +87.2 s
  cast killed two slot 1 melee. uld1 +162.6 s the same through a stepped slot 2 (2). Round 20 saw it in w27 uld1 (6).
- Over the ten hard-mode wipes since round 17 (`--chains`): 16 of 44 Chain Lightning deaths came through a slot on its
  step point or within 3 s of leaving it. Replayed at each of the 452 step moves (`stepbodies.py`/`stepring.py`: zones
  from `hz` rows and the bunny, floor approximated, no navmesh), 125 destinations lay within 5.5 yd plus both reaches
  of another slot's bot or point.
- Code: `ThorimRingBlizzardStep` (`UldEncounter_Thorim.cpp:3131`) took the nearest point 11 yd clear of every zone,
  out of the cone, past the ring and 8 yd off both tank spots, and counted no other slot; once there the bot holds.
- **Classification: bug.** The ring keeps its slots out of each other's jump (solver separation, `RingBodyBridgesChain`),
  and the step already keeps the tank side clear by the jump; its points undid that.

**Fix** (`UldEncounter_Thorim.cpp`, Thorim only, no size, mode or player branch): `RingStepChainBodies` (`:646`) lists
every living floor ring bot of another slot where it stands and that slot's point (its `composed` bearing at r 8.8 round
Thorim), each with a clearance of `ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP` + `ULDUAR_THORIM_RING_CHAIN_MARGIN` + both live
combat reaches (8.5 yd, 9.4 under Heroism). The step tries a new first pass that also clears all of them, within the
same 15 yd, before the three old passes, which stay as the fallback. Camp bodies are not counted: counting them left no
point for 223 of 452 steps in the replay, against 157 for the slots alone. Where the replay found a point, the walk
there was p50 0 / p90 4 yd longer and the point 11.4 yd off Thorim at p50 against 9.7 (both out of melee reach). A
human melee holds no slot, so like a camp body it is not counted; the bots step the same with or without one. Reader:
`--chains`, tested in `test_thorim.py`; boss doc: the pass order, the evidence, `--chains`.

`pb-check` passes (syntax, pblint, codestyle, botobs tests).

**Next window should show** (hard-mode pulls, `--chains`, `--blizzard`):
- "slot to slot through a stepped slot" 0 and deaths through a stepped slot 0 in every pull (w29: 2 / 0 / 2 / 2 deaths in
  uld1 / uld4 / uld5 / uld6); no `thorim sif blizzard action` destination within ~8.5 yd of another slot's bodies.
- Steps per pull about the same (17-56), walk back p50 within ~0.5 s of today's 0.2-1.4 s, no ring Blizzard death before
  the first tank death; melee on Thorim to +100 s still ~100%.
- Watch: a stepped bot standing past melee reach longer; chains still crossing slots by the cone relax or the camp
  under Heroism (ranked 2 in State, not fixed).

**Seen, not picked:** the Colossus' Smash 62339 on gauntlet healers (State rank 6); early tank deaths at +46-47 s (rank
5, the race plus Blizzard and Lightning Charge on the holder); pets crowd the ring (uld5's dead melee stood among Army
ghouls, a warlock pet and a Shadowfiend), so pets can still bridge what the step clears.

**Questions for the user:** none new; question 1 in State stands, recommended (a).

**Shared code touched:** none (Thorim files and the Thorim reader).
