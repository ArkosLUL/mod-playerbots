# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`; `raidtest.py report` runs `--summary` on every counted pull). Rounds 1-23: `history/thorim.md`.

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
Lightning every 15 s, cast within 0.8 s of an orb lighting with a 0.5-0.6 s cast bar naming its first target,
landing 0.4-1.4 s after the lit, 8 targets at ×1.5 a hop, hop 8 yd (8.9 when both carry Heroism/Bloodlust); hard mode adds
Sif's Frostbolt Volley (−50% speed), Frostbolt (−65%), Blizzard (−65% in a zone) and Frost Nova (root). Blizzard's
bunny walks the core's fixed path once at ~7 yd/s (west, then north of the tank spot, then round the camp), dropping
a 10 s zone every 2 s.

**Rosters.** From window 34 every pool carries all eleven professions at 450 with their perks and enchants (DC
`.dc test roster professions`): compare only windows from 35 on.

**What kills raids.** Hard mode at ilvl 226 is a race: from +30 s the raid takes him 0.41-0.54 %/s (w35), and the
tanks fall at +93-183 s to his melee, which grows with the Lightning Charge stacks, plus Blizzard on the waiting tank;
most deaths come after, with Thorim loose. W35 uld1 and uld2 wiped with him at 3.1% and 1.5%, so every death before
the first tank death costs the kill. Before it (w35, 19): Chain Lightning 7, Lightning Charge 7 (ring 5, camp 2),
Frost Nova 2, Blizzard, Frostbolt, Volley 1 each. From +100 s every cone one-shots cloth. Normal mode: kills with 0-4
deaths.

**Compositions on build ce2ffa6be (window 35, all hard mode).** A (uld1, uld4): uld4 kill, 4 deaths (the first
hard-mode kill); uld1 wipe at 3.1%, first tank death +183.3 s. B (uld2, uld5): wipes at 1.5% / 35.6%, first tank
death +180.2 / +93.5 s. C (uld3, uld6): wipes at 14.9% / 38.4%, first tank death +167.8 / +117.0 s. Not polished: B
and C have no kill.

**Open failures, ranked** (window 35, 183 deaths):
1. The race: Thorim melee 92 and Unbalancing Strike 8, all after the first tank death. Not a bug; the user keeps it
   polishing with no end condition.
2. Chain Lightning 17, 7 before the first tank death: a camp chain through shelter walkers 3 (uld1 +177.6 s orb 1, six
   camp hits; uld3 the same at +177.3 after the tanks), opening chains under lust 3 (uld3 +11.8 s slot-tank-slot at
   3.0-3.3 yd, uld4 +26.9 s slot to slot 9.0 yd), tank to a stepped slot 1 (uld2). On 37 hard pulls, chains of 4+ camp
   hits through walkers before the first tank death: orb 1 4 casts 5 dead, orb 2 4 casts 7 dead. Question 1.
3. Lightning Charge 17: ring 5 before the first tank death (round 24's fix: uld1 3 stepped bots with no clear point, 2
   dead; uld3 2 walking back across the cone, 1 rooted by Frost Nova); camp 2 (uld5 a Chain Heal holding the walk 2 s,
   uld4 slowed); 10 after the tanks.
4. Blizzard 6: uld6's waiting tank 8 s in a zone on the off-tank spot (no step, user), dead +117.0 s, then the holder
   (the first tank deaths); uld3 healer, uld1, uld2 1 each. Ring Blizzard deaths 0 (w33 6).
5. Sif's Frostbolt 5, Volley 5, Frost Nova 3 (2 before the first tank death, uld6). Nothing to kick.
6. Self-res into the hazard (uld6 tank back into the zone): shared res code; not picked.
7. Phase 1: Colossus Smash 62339 (uld5), Runic Smash (uld4), Whirlwind (uld6), 1 each.

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
- R17: covered ring melee step out of Blizzard, `avoid aoe` held off ring melee (user) — held (w35 ring Blizzard
  deaths 0).
- R18: the swap fires only within 8 yd of the tank spot — held.
- R19: class taunts from past 8 yd held while he's on a non-tank or nobody, anchor exempt — held.
- R20: the anchor with its taunt cooling gets no spot while he's loose, so it closes on him — held.
- R21: the Blizzard step's lead passes stay out of the jump of the camp and the other slots — held.
- R22: the step's first round stays 11 yd off the bunny's remaining walk — held (0 points onto the path ahead in w35).
- R23: a camp bot short of time for its shelter row takes a cone step 45° off the cone; a camp bot in the lit cone
  breaks its channel — held (w35 cone deaths on walks before the first tank death 2 against w33's 8, hits 4 against
  11, 19 steps).
- R24: a ring walk that would cross the lit cone holds; a stepped ring bot in the lit cone with no zone-clear point
  steps past 45° anyway — pending (window 36).

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
1. Camp shelter walks bridge Chain Lightning (round 24). Choice: (a) a camp bot whose shelter walk passes within the
   8 yd jump of another slot's body waits to set off until Thorim's Chain Lightning has landed (its cast bar shows,
   0.5-0.6 s), taking R23's cone step when the late start leaves the row out of reach; (b) re-solve the orb 1 and orb 2
   shelter rows with navprobe so no two walks pass within 8 yd; (c) leave it. Recommended: (a), no new positions and
   R23's step covers late walkers; replayed before it ships.

## Round 24 (window 35: build ce2ffa6be with round 23)

**Data:** `window35-thorim.md` (session scratchpad), one valid pull per pool, all hard mode; traces
`603_{9,10,12,7,8,11}_thorim_{1795551088,1795551098,1795551156,1795550986,1795551073,1795551124}` (uld1, uld4, uld2,
uld5, uld3, uld6), read with `thorim.py --shelters/--chains/--blizzard`, the new `--ringcone`, and session scratchpad
`thorim_r24/` scripts (a ring cone hit classifier, a camp chain batch and a cone-hold replay over the 37 hard-mode
pulls on disk). Filler 33c (round 22's build): six normal-mode pulls, kills on uld3, uld4, uld6.

| Pool | Out | Deaths by mechanic |
|---|---|---|
| uld1 (A) | wipe, hard, 3.1% | LC 2 (ring slot 2 stepped north, +46 s), CL 3 (camp orb 1 walkers +177.6 s), then tanks +183 s, Thorim melee 14, CL 4, Volley 3, LC 2, US 1, Blizzard 1, Frostbolt 1 |
| uld4 (A) | **kill, hard, 4** | Runic Smash 1, Volley 1, CL 1 (+26.9 s lust), LC 1 (camp, slowed) |
| uld2 (B) | wipe, hard, 1.5% | Frostbolt 1, CL 1 (tank to stepped slot 0), then tanks +180 s, Thorim melee 23, US 2, Blizzard 1, LC 1 |
| uld5 (B) | wipe, hard, 35.6% | Colossus Smash 1, LC 1 (camp, Chain Heal 2 s late), then tank +93.5 s, Thorim melee 25, US 1, Frostbolt 1, Nova 1 |
| uld3 (C) | wipe, hard, 14.9% | CL 2 (+11.8 s lust), Blizzard 1 (healer), LC 3 (ring: 1 rooted, 2 walking back), then US +167.8 s, Thorim melee 14, LC 3, CL 3, Frostbolt 2, Volley 1, US 1 |
| uld6 (C) | wipe, hard, 38.4% | Whirlwind 1, Nova 2, then waiting tank in Blizzard +117 s, Blizzard 2 more, Thorim melee 16, LC 4, CL 3, US 2 |

**Stop rules:** waived for Thorim (user, 2026-11-24). Done rule: A has a hard-mode kill with 4 deaths on this build,
B and C none.

**Round 23 verdict (slowed camp ranged step out of the cone): held on its targets in A, B and C.** `--shelters`, before
the first tank death: walks 4 / 6 (A), 0 / 15 (B), 12 / 8 (C); steps 2 / 2, 0 / 4, 7 / 4, nearly all there; hits 0 /
1, 0 / 2, 0 / 1 (4 against w33's 11), deaths 0 / 1, 0 / 1, 0 / 0 (2 against 8). Slot 5's long rows killed none. The two
deaths: uld4 Ulduad on row (1, 4) 16.9 yd, Frostbolt and Volley slowing it after it set off, 5.5 short; uld5 Uldvae
in a Chain Heal from 0.1 s before the lit, set off 2 s late on row (2, 2) 23.6 yd (no step found), 5.7 short. Camp hits
off a walk only in the opening's first two cones (3). The windows compare unlike rosters (professions from w34).

**Failure picked: Lightning Charge on the melee ring before the first tank death** (5 of w35's 7 cone deaths there;
CL ties at 7, but its worst share is question 1, a plan change).
- uld1 +41.0 s orb 6: slot 2's three bots had stepped to (2109-2114, −228) at the north wall, 20-27 yd from him.
  `--ringcone`: Uldxay standing past reach, dead; Uldxat stood 3.2 s, stepped at +44.2 s, dead mid-walk; Uldxai hit.
  No `thorim sif blizzard action` move from +38.8 to +44.2 s: every pass of `ThorimRingBlizzardStep`
  (`UldEncounter_Thorim.cpp:3340`) wants 11 yd off every zone and the ring's 52.5° off the cone; west was wall
  (x ~2100) and zones (2108.4, −247.7), (2118.7, −238.0), east the cone. A replayed sweep (no navmesh) found such
  points only against the west wall, x 2098-2101.
- uld3 +161.1 s orb 0: Uldzax and Uldzag stepped out at the lit to (2131.1, −258.0); the ring point uncovered at
  +164.2 s, `thorim phase 2 positioning action` walked them back to (2113.3, −246.3) straight across the cone
  (16.4 → 6.6 yd from him), dead at +166.1 s. `ThorimRingWantsMove` (`:3066`) never looked at the walk.
- uld3 +116.3 s: Uldzaj rooted by Frost Nova at the lit, 4 `blocked` moves, dead. Not fixable here.
- 37 hard-mode pulls on disk, before the first tank death: 54 ring hits, 10 dead (blocked 3, walking back after a
  step 3, past 12.5 yd of him 2, rooted 1, other walks 1). Cone-hold replay (a walk issued while lit, from outside the ring's cone, crossing it):
  60 walks, 26 of them hit, 10 dead (4 before the first tank death); held time ~4 bot-s a pull.
- **Classification: bug.** The step and the ring solver both intend the ring out of the lit cone; neither the walk
  back nor a step with no zone-clear point honoured it.

**Fix** (`UldEncounter_Thorim.cpp/.h`, Thorim only, no size, mode or player branch): `ThorimRingWantsMove` answers
stay while the orb is lit, the bot stands outside the ring's cone and its straight walk to the point enters it
(sampled each yard); the stay sets the arrival latch, so the movement guard holds generic movers. Derived probe
`thorim.ringconehold` (`hold`/`none`). `ThorimRingBlizzardStep`, all passes failed and the bot within 45° of the lit
cone (`ULDUAR_THORIM_CONE_STEP_MARGIN`, renamed from the camp's): nearest point past 45° within 20 yd in the arena box,
first clear of the zones' 9.8 yd reach (`ULDUAR_THORIM_BLIZZARD_ZONE_REACH`), else inside one. Reader `--ringcone`,
tested in `test_thorim.py`; boss doc: both rules and the section. `pb-check` passes.

**Next window should show** (hard-mode pulls, `--ringcone`):
- ring cone deaths before the first tank death ≤ 1 (w35: 5, 1 rooted); no stepped bot hit "standing, past reach"; ring
  hits "walking (thorim phase 2 positioning action)" before the first tank death 0-1 a window.
- held walks ≥ 1 a pull, held and hit 0; `--phase2` melee on Thorim to +100 s still ≥ 99%; `--blizzard` ring melee in
  zones no higher than w35 (0.9-54.3 bot-s a pull, deaths 0).

**Seen, not picked:** camp chains through shelter walkers (question 1); opening chains under lust; the waiting tank
in Blizzard (user's rule); a cast-time spell holding a shelter walk (server-side only).

**Questions for the user:** question 1 (State).

**Shared code touched:** none (Thorim files and the Thorim reader).
