# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`, `--colossus`, `--chainwait`, `--ringwait`, `--spree`; `raidtest.py report` runs `--summary` on every
counted pull). Rounds 1-29: `history/thorim.md`.

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
waiting tank on the off-tank spot (2107.25, −252.65), a swap on every Unbalancing Strike; he stands 4.5-4.8 yd off the
tank spot, east (x ~2115.5) or, at 17% of cones, west by the off-tank spot (x ~2107); melee on a 3-slot ring (r 8.8,
slots ≥60° apart), ranged on 6 camp spots with shelter rows from the Lightning Charge cone (lit orb 4.6-4.9 s ahead;
the rows were solved with the apex on the tank spot); Chain Lightning every 15 s with a 0.5-0.6 s cast bar naming its
first target, the cast starting p50 0.5 s (max 1.5) after the lit stamp and landing p50 1.05 s after it, a third of
the events casting nothing (one cause: the random pick lands on a pet and fails the spell's player-only check); 8
targets at ×1.5 a hop, hop 8 yd (8.9 when both carry Heroism/Bloodlust), **players only**; hard mode adds Sif's
Frostbolt Volley (−50% speed), Frostbolt (−65%), Blizzard (−65% in a zone) and Frost Nova (root). Blizzard's bunny walks
the core's fixed path once at ~7 yd/s (west, then north of the tank spot, then round the camp), dropping a 10 s zone
every 2 s; two ring slot points sit ~5 yd off its west and north legs, so those slots step out on every Blizzard.
Killing Spree puts a rogue 3.3 yd behind him for 2.5 s, in the jump of the holder and the slots near his back.

**Rosters.** From window 34 every pool carries all eleven professions at 450 with their perks and enchants (DC
`.dc test roster professions`): compare only windows from 35 on.

**What kills raids.** Hard mode at ilvl 226 is a race: the raid takes him ~0.43-0.57 %/s from +30 s, and the tanks
fall to his melee, which grows with the Lightning Charge stacks (14-18k a swing by +110 s), plus Blizzard and Sif's
Volley on both tanks (tank intake before the first tank death over 64 pulls: melee 4.1k/s, Blizzard 2.0k/s, Unbalancing
Strike 0.9k/s, Volley 0.6k/s, Lightning Charge 0.25k/s); most deaths come after, with Thorim loose. The first tank
death decides it: p50 +120 s over w49-w50's 10 hard-mode pulls, and the kills held the tanks to +170-188 s. A gauntlet
healer lost in phase 1 costs the most. Deaths before the first tank death over the 64 hard-mode pulls since 11-24 18:00:
Chain Lightning ring 51, Lightning Charge camp 43, Blizzard ring 21, Chain Lightning camp 20, Blizzard camp 17, Volley
camp 16. Normal mode: kills with 0-4 deaths.

**Compositions** (hard mode unless noted). R28's build (window 49): A (uld1, uld4) kill 5 deaths, wipe 34.9%; B (uld2,
uld5) normal-mode kill 1, wipe 33.4%; C (uld3, uld6) wipe 13.3%, kill 7. R29's build (window 50): A wipe 36.7%, kill 6;
B wipes 9.6% and 33.7%; C normal-mode kill 0, wipe 1.2%. Not polished on any one build. No pull yet on R30's code.

**Open failures, ranked** (w49-w50, 10 hard-mode pulls; before the first tank death unless noted):
1. The race: Thorim melee 151 and Unbalancing Strike 15 over both windows, nearly all after the first tank death. Not a
   bug; the user keeps it polishing with no end condition.
2. Lightning Charge on the camp 11 (w49 uld4 7: rows inside the live cone with him west, R30; slowed walkers short of
   their rows; a step he was dragged across, w50 uld1 +31 s). Over 64 pulls 28 of the 43 died short of a row, mostly
   under Volley or rooted.
3. Chain Lightning on the ring 8: Killing Spree rogues (R30), seeds inside a slot of 3-4 (w50 uld2 +117 s, uld6
   +102 s), the opening.
4. Blizzard on the tanks, 24% of their intake (the tank spot sits 4.1 yd off the track; the user keeps the waiting tank
   there; moving the anchor east is a full re-solve, boss doc).
5. Tank to slot chains: the solver's anchor term is angle-only; R29's wait covers those on the lit orb.
6. Gauntlet healers at the Rune Giant hold (`ThorimGauntletTrailSpot`'s no-path fallback): no phase 1 death since w44.
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
  (`ThorimCampChainWait`, cap 1 s, reach guard, cast hold) (user) — held.
- R27: the bunny's next 8 yd of path counts as Blizzard for the ring (`ULDUAR_THORIM_RING_BLIZZARD_LEAD`) — held (ring
  Blizzard deaths 0 in w49-w50 but w49 uld4's 2 after its healers died).
- R28: a stepped ring bot clear of the hazards re-steps while another slot's bot or point is in its jump;
  `RingBodyBridgesChain` counts another slot at its point too — held (slot to slot through a stepped slot 0 in
  w49-w50).
- R29: a ring walk or Blizzard step started while `ThorimChainWaitDue` holds, ending in another group's jump, waits
  for the chain (`ThorimRingChainWait`) (user) — held (ring chain hits per pull 13.0 → 7.8, hop 5+ 6 → 2).
- R30: `killing spree` held for ring bots outside 5.5-9.0 s after the last lit stamp; a shelter row inside the live
  cone takes the cone step — pending.

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
- Backing R27 out after w46's 0/6 (R28): its target held, first tank death and %/s unchanged.
- The ring chain wait also from a spot already in another group's jump (R29 replay): 996 holds, 7 chains grew, hits
  at hop 5+ 48 → 38 against 31 shipped.

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
  another slot's body waits to set off until Thorim's Chain Lightning has landed (built in R26).
- 2026-11-25, Q1 (a) of round 28: a ring walk or step that starts while the orb is lit and would end within the 8 yd
  jump of another group's body or point waits until the cast lands (cap 1 s), only when the cone is still escapable
  (built in R29).

**Open questions:** none.

## Round 30 (window 49: build 06854755f with R28; window 50: build 0200b8827 with R29)

**Data:** `window49-thorim.md` (R28 alone) and `window50-thorim.md` (R29 on R28), session scratchpad. Traces w49
`603_{1,4,2,5,3,6}_thorim_{1795609812,1795609801,1795609807,1795609806,1795609812,1795609806}`, w50
`603_{12,7,8,10,11,9}_thorim_{1795613383,1795613189,1795613231,1795613250,1795613333,1795613241}` (uld1, uld4, uld2,
uld5, uld3, uld6), read with `thorim.py --chains/--blizzard/--ringwait/--shelters/--spree` and `postmortem.py`;
corpus figures over the 64 hard-mode pulls since 11-24 18:00 (79 valid traces) from session scratchpad `thorim_r30/`.

| Pool | w49 (R28) | w50 (R29) |
|---|---|---|
| uld1 (A) | kill, hard, 5: CL 2 (+27 s under lust; tank to stepped slot 0 +148 s), Frostbolt, US, Blizzard | wipe 36.7%: Stomp, CL 1 (+27 s under lust), LC 1 (healer +31 s), Volley 1 (same healer, rezzed), tanks from +107.5 s, melee 22 |
| uld4 (A) | wipe 34.9%: Smash, LC 5 at +76 s (both slot 1 healers) and 2 at +91 s, Volley/Blizzard 2, tanks +118 s, melee 15 | kill, hard, 6: Volley 2, LC 1, Frostbolt 1, US 1, Frost Nova 1 |
| uld2 (B) | kill, normal mode, 1 (Smash) | wipe 9.6%: LC 1 (ring +46 s), CL 1 (slot 1 seed), Frostbolt 1, tanks +161 s, melee 24 |
| uld5 (B) | wipe 33.4%: CL 1 (+102 s), tanks from +107 s (US, then Blizzard ×2), melee 23 | wipe 33.7%: tanks +114 s first, melee 22, LC 6 |
| uld3 (C) | wipe 13.3%: CL 2 (the +12 s opening chain under lust, Killing Spree), tanks +122 s, melee 21, LC 3, CL 2 | kill, normal mode, 0 |
| uld6 (C) | kill, hard, 7: Frost Nova 1, LC 2 (+121 s, slowed), tanks 4 at +171-185 s | wipe 1.2%: Frostbolt 1, CL 2 (+87 s Killing Spree; +102 s slot 0 of 4), Volley 1, tanks +107 s, melee 21 |

**Stop rules:** waived for Thorim (user, 2026-11-24). Done rule: w49 has A (uld1, 5 deaths, hard mode), B only in
normal mode, C 7; w50 none.

**R28 verdict (re-step clear of another slot's jump): held in A, B and C.** Chain deaths through a stepped slot, slot
to slot, 0 in both windows (w44-w46: 9); w49 stepped bot-s in another slot's jump 9.0 / 6.3 / 4.8 / 2.7 / 0.5 (uld1,
uld4, uld5, uld3, uld6) against w46's 40.1 / 19.4 / 3.5 / 31.2 / 4.7; chain deaths 15 (w46) → 7; R27 holding.

**R29 verdict (ring moves on the lit orb wait for the chain): held in A, B and C.** `--ringwait`: 13-18 waits a pull,
p50 0.8-1.0 s, max 1.7, 47 of 78 released by the landing; waiters hit while walking 0, standing 7 (1 dead, uld6 +102 s,
its own slot of 4); cone hits on waiters 5 against the forecast ≤ 1 (1 dead, after the first tank death). Ring chain
hits per hard pull 13.0 → 7.8, hop 5+ ring hits 6 → 2 (w46 11), chain deaths 7 → 4 (A 2 → 1, B 1 → 1, C 4 → 2). No
cost: raid %/s from +30 s to the first tank death 0.47-0.55 (w49 0.43-0.57, w46 0.50-0.56), melee within 9.08 yd
66-80% (w49 69-81%, w46 71-79%).

**Failure picked: Killing Spree bridges Chain Lightning** (failure 3; chains are the top killer before the first tank
death over 64 pulls).
- w50 uld6 +87.1 s: Uldwag (slot 2) cast Killing Spree at +83.3 s, 12.3 s after the +71 s lit, landed at
  (2111.5, −260.6), 3.3 yd behind him (275°), and stood there until it ended (+86.4 s, its positioning moves `dup`);
  walking back at the landing 4.0 yd off its point, it took the chain from slot 2 ×3 on 5.5 yd to slot 1 ×3, 1 dead.
- w49 uld3 +12.0 s: Uldzag's spree at hop 3 of 7, 2 dead in slot 0 past it.
- Corpus: 143 sprees by ring bots; 53 of the 120 after a lit began within 2 s of it (burst release at the opening,
  then the 75 s glyphed cooldown, five orb cycles); 15 chains hit a spree rogue, 13 crossing through it, 8 dead past
  it (6 before the first tank death); all 9 opening-chain deaths came in casts that hit one; cone hits 10. The 18
  sprees that began 5.5-9.0 s after a lit: no chain, no cone.
- Cause: `CombatRogueStrategy`'s `killing spree` (energy ≤ 50, burst allowed) has no Thorim gate.
- **Classification: bug.** The ring's spacing is the strategy's intent; Algalon, Mimiron and Yogg-Saron already
  block it by name.

**Second (independent, small): shelter rows inside the live cone.** w49 uld4 +76.0 s: he sat west at
(2106.8, −250.5); Ulduak and Ulduaq on slot 0's orb 2 row (2116.5, −283.35) stood 37.1° off the live axis, Ulduaw on
slot 2's (2114.5, −274.35) 35.7°, all dead; +91.0 s Ulduag on slot 0's row dead. The table (`UldEncounter_Thorim.cpp`
`ULDUAR_THORIM_SHELTER_SPOTS`) puts those rows 42.5° and 43.3° off from the tank spot, and `ThorimRangedSpot` never
tested a row against the live cone. Corpus: west at 17% of cones before the first tank death; rows ≤ 37.2° off had
25 occupants, 7 hit, 4 dead; rows ≥ 39.2° off 38, none hit. **Classification: bug.**

**Fix** (`UldEncounter_Thorim.cpp/.h`, `UldDefinition_Thorim.cpp`; Thorim only, no size, mode or player branch):
- `thorim killing spree hold` blocks `killing spree` for a ring bot on the floor in phase 2 except 5.5-9.0 s after
  the last lit stamp (`ULDUAR_THORIM_KILLING_SPREE_OPEN_MS` / `_CLOSE_MS`; `lightningOrbLastLitMs`, set with the lit
  stamp and kept through the dark orb, reset with the encounter, as `lightningOrbLitMs` now is too); before the first
  lit it holds.
- `ThorimRangedSpot` takes the cone step when the row itself is in the live cone (`ThorimCampSpotInLitCone`), also
  for a bot already on it; `ThorimCampChainWait` no longer counts such a row as in reach.
- Reader `--spree` (tested in `test_thorim.py`); boss doc "Killing Spree waits for the dark orb", "The table's apex is
  the tank spot; the cone's is Thorim". `pb-check` passes.

**Next window should show** (hard-mode pulls):
- `--spree`: sprees begin 5.5-9.0 s after the lit; in a chain 0 and in the cone 0 (w49-w50: 4 chains, 3 dead past);
  opening-chain deaths 0 (w49-w50: 2).
- `--shelters`: no camp bot dying on a row; with him west, orb 2's slot 0 takes a `step`.
- R28 and R29 holding (`--chains`, `--ringwait`); watch rogue damage (a spree waits up to ~11 s).

**Seen, not picked:** slowed camp walkers short of their rows (28 of 43 camp cone deaths over 64 pulls); a cone step
he is dragged across after the lit (w50 uld1 +31 s healer, 36.7° on its step); slot 2's row with him west has no step
in reach, so it still walks into 35.8°; seeds inside a slot of 4 (w50 uld6 +102 s); the cone on the tanks
(0.25k/s); Blizzard on the tanks (failure 4).

**Questions for the user:** none.

**Shared code touched:** none (Thorim files and the Thorim reader).
