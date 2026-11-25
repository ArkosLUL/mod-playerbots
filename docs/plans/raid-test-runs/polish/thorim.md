# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`, `--colossus`, `--chainwait`; `raidtest.py report` runs `--summary` on every counted pull). Rounds
1-25: `history/thorim.md`.

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
(west, then north of the tank spot, then round the camp), dropping a 10 s zone every 2 s.

**Rosters.** From window 34 every pool carries all eleven professions at 450 with their perks and enchants (DC
`.dc test roster professions`): compare only windows from 35 on.

**What kills raids.** Hard mode at ilvl 226 is a race: the raid takes him ~0.4-0.55 %/s, and the tanks fall to his
melee, which grows with the Lightning Charge stacks (14-18k a swing by +140 s), plus Blizzard and Sif's Volley on both
tanks; most deaths come after, with Thorim loose. The first tank death decides it: over the hard-mode pulls since 11-23
its p50 is +119 s, and the kills held the tanks to +167-191 s or past the kill. A gauntlet healer lost in phase 1 (the
Colossus' Smash until R25, the Rune Giant hold in w39) costs the most: first tank death +45-137 s (p50 77) in hard
mode, or a late phase 2 in normal mode. Normal mode: kills with 0-4 deaths.

**Compositions on build 6694fb83e (window 39, round 25).** A (uld1, uld4): uld4 a hard-mode kill with 0 deaths (phase
2 at 2:50.7); uld1 wipe at 14.0%, first tank death +155.6 s. B (uld2, uld5): uld2 a normal-mode kill, 1 death (phase 2
at 3:29.3); uld5 wipe at 39.0%, first tank death +78.4 s (Volley, then melee). C (uld3, uld6): normal-mode kills, 2 and
4 deaths (phase 2 at 3:26.2 / 3:18.0), each after a gauntlet healer died at the Giant hold. B has never killed in hard
mode; C last did on the window 35 build. Not polished on any one build.

**Open failures, ranked** (window 39, 71 deaths):
1. The race: Thorim melee 46 and Unbalancing Strike 3, nearly all after the first tank death. Not a bug; the user
   keeps it polishing with no end condition.
2. Chain Lightning 11: uld1 a camp chain through orb 2's shelter walkers (+87.2 s, 5 camp hits, hop 5 dead; R26's
   target) and tank to slot 2 (+57.3 s) before the tanks, 5 after; uld6 slot 1, tanks, slot 0 (+56.9 s, 3 dead,
   normal); uld3 the opening under lust (+11.8 s).
3. Gauntlet healers at the Rune Giant hold: uld3 Uldzae and uld6 Uldwae walked onto the leader's exact spot at the
   hold (the `ThorimGauntletTrailSpot` fallback when the path is not `PATHFIND_NORMAL`; Uldwae came down the ramp
   z 438 → 426), dead at 2:32 / 2:36 to Rune Detonation, Stomp and Honor Guard Cleave; both pulls went normal mode.
   Next round's candidate.
4. Lightning Charge 4: uld1 2 camp walkers before the first tank death (Uldxak row (1, 4) under Volley, 5.7 yd short;
   Uldxag rooted by Frost Nova), 2 after.
5. Blizzard 2, Volley 1, Frostbolt 1 (uld5's Volley on the holder began its tank deaths). Nothing to kick.
6. Phase 1: Honor Guard melee 1 (uld2 ranged), besides the two Cleaves in 3.
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
  Colossus (w39 Smash on healers 0 hit, 0 dead against w37's 4 hit, 3 dead); its no-path fallback is failure 3.
- R26: a camp shelter walk for orb 1 or 2 that passes another slot's body waits for Chain Lightning
  (`ThorimCampChainWait`, cap 1 s, reach guard, cast hold) (user) — pending (next window).

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

## Round 26 (window 39: build 6694fb83e with round 25)

**Data:** `window39-thorim.md` (session scratchpad), one valid pull per pool; traces
`603_{1,4,2,5,3,6}_thorim_{1795560092,1795560103,1795560093,1795560101,1795560098,1795560104}` (uld1, uld4, uld2,
uld5, uld3, uld6), read with `thorim.py --colossus/--chains/--shelters` and the new `--chainwait`, `postmortem.py
--death`, and session scratchpad `thorim_r26/` scripts (`replay.py`: the wait rule over the 95 Thorim pulls on disk,
53 of them hard mode; `healer_moves.py`).

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | wipe, hard, 14.0% | CL 2 (tank to slot 2 +57.3 s; camp orb 2 walkers +87.2 s), LC 2 (camp walkers, Volley / Frost Nova), then tanks +155.6 s, Thorim melee 19, CL 5, LC 2, Blizzard 1, US 1 |
| uld4 (A) | **kill, hard, 0** | none |
| uld2 (B) | kill, normal, 1 | P1 Honor Guard melee (ranged) |
| uld5 (B) | wipe, hard, 39.0% | none before the tanks (+78.4 s, Volley then melee); Thorim melee 24, US 2, Blizzard 1, Frostbolt 1 |
| uld3 (C) | kill, normal, 2 | P1 gauntlet healer at the Giant hold (Cleave); CL 1 (opening under lust) |
| uld6 (C) | kill, normal, 4 | P1 gauntlet healer at the Giant hold (Cleave); CL 3 (slot 1, tanks, slot 0, +56.9 s) |

**Stop rules:** waived for Thorim (user, 2026-11-24). Done rule on this build: A has a hard-mode kill (uld4, 0
deaths); B and C killed only in normal mode.

**Round 25 verdict (gauntlet healers 15 yd short of the leader): held on the Smash in A, B and C.** `--colossus`:
healers within 6 yd of his target 0, Smash hits on healers 0, deaths 0 (w37: 4 hit, 3 dead); healers 9.8 yd behind
him at 175-177° (uld3) or 17.4-17.8 yd off his flank (uld2, uld5), none out of heal range; no Smash cast in uld1,
uld4, uld6. Watch items: corridor Runic Smash killed no healer. The Giant hold did: uld3 Uldzae (at the Giant, 0.1 yd)
and uld6 Uldwae, whose `thorim gauntlet positioning action` moves ran from the ramp top (z 438) to the leader's exact
spot (2187.79, −432.78), the trail's fallback when the path is not normal; Rune Detonation, Stomp and Honor Guard
Cleave, both pulls then normal mode. Before R25 every trail took that spot, so the fallback is what is left
(failure 3). Hard-mode phase 2 3 of 6 (w37 5 of 6): uld3 and uld6 after the healer loss; uld2 (no loss, the Colossus
lived through two Smashes) unexplained.

**Failure built: the user's Q1 (a), camp Chain Lightning through shelter walkers** (failure 2's camp share).
- uld1 +87.2 s, orb 2: Uldxas, Uldxao, Uldxav (slot 1), Uldxap (slot 2), Uldxaw; Uldxaw dead at hop 5. Uldxap's walk
  to row (2, 2) passed 0.8 yd from another slot's body, Uldxav's 2.4 yd; the replayed chain with the held walkers standing
  stops after four hops, none on Uldxap or Uldxaw.
- 53 hard-mode pulls on disk, before the first tank death: camp chain deaths by orb 4, 6, 10, 0, 0, 0, 0 (orbs 0-6),
  16 of 20 under orbs 1 and 2; chains of 4+ camp hits 5 and 6 there.
- Timing over 395 hard-mode casts, from the lit stamp: cast start p50 506 ms, p90 995, max 1523; landing p50 1049,
  p90 1552, max 2095. 191 of 586 lit orbs had no cast: the trace's cast hook fires after `CheckCast`, and
  `boss_thorim.cpp:737` repeats the event in 15 s whether or not the cast goes out.
- **Classification: plan change, answered by the user** (Q1 (a)); built as answered.

**Fix** (`UldEncounter_Thorim.cpp/.h`, `UldMultipliers_Thorim.cpp/.h`, `UldDefinition_Thorim.cpp`, Thorim only, no
size, mode or player branch): `ThorimCampChainWait` decides once per orb on orbs 1 and 2
(`ULDUAR_THORIM_CHAIN_WAIT_ORBS`): a camp bot whose straight walk to its row passes within 5 yd plus both reaches of
any floor member outside its own ranged slot (a real player has no slot, so counts) stands, note `slot N orb M wait`,
until the Chain Lightning cast bar ends (`chainLightningEndMs` from Thorim's current spell, read in `TickThorim` and
on each call). No bar by lit + 1 s (`ULDUAR_THORIM_CHAIN_WAIT_CAP_MS`) releases it; a cast ended up to 5 s before the
lit counts as landed. It waits only if, setting off at the bar's end or the cap, the row or a cone step is in reach,
or it stands out of the lit cone (`ThorimCampShelterInTime` / `ThorimCampConeStep` take a set-off time).
`ThorimChainWaitCastHoldMultiplier` zeroes cast-time and channelled spells while it waits. Beyond the answer's text,
both from the replay: the cap and the reach guard. Reader `--chainwait`, tested in `test_thorim.py`; boss doc:
"A walk that passes another slot's body waits for the chain" and the section. `pb-check` passes (syntax, pblint,
codestyle, botobs).

**Replay** (`replay.py`, 53 hard-mode pulls, 1,251 orb 1 and 2 walks; set-off at the landing + 150 ms or the cap;
the chain re-run hop by hop to the nearest unhit body within 8 yd, pets included; cone hit ≤ 37.5° off its axis, the
widest decoded hit 37.2°; a delayed walk replayed as its own track shifted, or a straight walk to the step):
- 651 walks wait (453 before the first tank death), 548 set off later; the guard lets 63 walk (2 chain hits, 0 dead).
- Chain: all 14 walkers a chain killed (10 before the first tank death) stand at the landing; on the 80 chains the hop
  model reproduces, camp hits 143 → 118, at hop 5 or later 14 → 4.
- Cone: 11 delayed walks newly end in it, 11 leave it (5 / 5 before the first tank death). Normal mode (42 pulls):
  405 waits, 1 new cone hit, 3 chain deaths stand at the landing.

**Next window should show** (hard-mode pulls, `--chainwait`, `--shelters`):
- waits on orbs 1-2 with crossing walks, most released by the landing; waiters hit by the chain while walking ~0;
  camp chain deaths under orbs 1-2 before the first tank death 0-1 a window, chains of 4+ camp hits there ≤ 1.
- cone hits and deaths on waiters before the first tank death ≤ 1 a window; `--shelters` hits on walks no higher than
  w39 (uld1: 2 hit, 2 dead).
- Watch: Thorim %/s over the first 100 s not lower (the wait holds cast-time spells up to ~1 s an orb).

**Seen, not picked:** the Giant hold trail fallback (failure 3, next); tank to ring chains (uld1 +57.3 s, uld6 +56.9
s); the opening chain under lust (uld3); uld5's Volley on the holder.

**Questions for the user:** none.

**Shared code touched:** none (Thorim files and the Thorim reader).
