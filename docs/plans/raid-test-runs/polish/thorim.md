# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`, `--colossus`, `--chainwait`, `--ringwait`, `--spree`, `--stalled`; `raidtest.py report` runs
`--summary` on every counted pull). Rounds 1-30: `history/thorim.md`.

## State

**Fight model.** The start pack (a mock fight in the arena) dies first; Sif casts 62507 20 s after its last death, and
phase 2 must start within 150 s of that or she casts 62565 (normal mode: Thorim −30% health, no Sif). Phase 2 starts
only on a player's hit on Thorim from above z 430, allowed once the Rune Giant dies. The arena squad holds the adds; the
bot gauntlet squad (12, two healers) pulls the lever, clears the corridor's two trash packs under the Runic Smash lanes,
kills the Runic Colossus (the leader tanks him ~4.4 yd south of him; Smash 62339, a 3 s cast at his target every
15-22 s, 37k in a 10 yd frontal cone), the ramp's Honor Guards and Acolyte and the Rune Giant (held at
(2187.79, −432.78)), walks the balcony and hits him from `BALCONY_5`; the corridor tank jumps down at +5-12 s and
walks ~40 yd to the off-tank spot. No phase 2 by phase 1 + 5 min wipes the raid to the Lightning Orb. Lever to 62565 is
~170 s; what decides it is Runic Barrier on the Colossus (+11-12 s when it lands early), the corridor under the smash,
and lane flips. Phase 2: tanks hold him on the tank spot (2110.75, −252.65), the waiting tank on the off-tank spot
(2107.25, −252.65), a swap on every Unbalancing Strike; he stands 4.5-4.8 yd off the tank spot, east (x ~2115.5) or,
at 17% of cones, west by the off-tank spot (x ~2107); melee on a 3-slot ring (r 8.8, slots ≥60° apart), ranged on 6
camp spots with shelter rows from the Lightning Charge cone (lit orb 4.6-4.9 s ahead; the rows were solved with the
apex on the tank spot); Chain Lightning every 15 s with a 0.5-0.6 s cast bar naming its first target, the cast starting
p50 0.5 s (max 1.5) after the lit stamp and landing p50 1.05 s after it, the first at +12 s, a third of the events
casting nothing (one cause: the random pick lands on a pet and fails the spell's player-only check); 8 targets at ×1.5
a hop, hop 8 yd (8.9 when both carry Heroism/Bloodlust), **players only**; hard mode adds Sif's Frostbolt Volley (−50%
speed), Frostbolt (−65%), Blizzard (−65% in a zone) and Frost Nova (root). Blizzard's bunny walks the core's fixed path
once at ~7 yd/s (west, then north of the tank spot, then round the camp), dropping a 10 s zone every 2 s; two ring slot
points sit ~5 yd off its west and north legs, so those slots step out on every Blizzard. Killing Spree puts a rogue
3.3 yd behind him for 2.5 s, in the jump of the holder and the slots near his back.

**Rosters.** From window 34 every pool carries all eleven professions at 450 with their perks and enchants (DC
`.dc test roster professions`): compare only windows from 35 on. Pools B and C send a bear as corridor tank, A a DK.

**What kills raids.** Hard mode at ilvl 226 is a race: the raid takes him ~0.43-0.57 %/s from +30 s, and the tanks
fall to his melee, which grows with the Lightning Charge stacks (14-18k a swing by +110 s), plus Blizzard and Sif's
Volley on both tanks (tank intake before the first tank death over 64 pulls: melee 4.1k/s, Blizzard 2.0k/s, Unbalancing
Strike 0.9k/s, Volley 0.6k/s, Lightning Charge 0.25k/s); most deaths come after, with Thorim loose. The first tank
death decides it: p50 +120 s over w49-w50's 10 hard-mode pulls, +167 s over w51's 5, and the kills held the tanks to
+167-188 s. A gauntlet healer lost in phase 1 costs the most, and so do DPS lost in the opening chain (w51 uld5: 3 at
+12 s, wipe at 36%). Deaths before the first tank death over the 64 hard-mode pulls since 11-24 18:00: Chain Lightning
ring 51, Lightning Charge camp 43, Blizzard ring 21, Chain Lightning camp 20, Blizzard camp 17, Volley camp 16. Normal
mode: kills with 0-4 deaths.

**Compositions** (hard mode unless noted). R29's build (window 50): A wipe 36.7%, kill 6; B wipes 9.6% and 33.7%; C
normal-mode kill 0, wipe 1.2%. R30's build (window 51): A kill 3 (uld4), wipe 15.1%; B kill 3 (uld2), wipe 36.0%; C
normal-mode kill 0 (uld3), kill 8 (uld6). A and B meet the done rule on R30's build, C does not. No pull yet on R31's
code.

**Open failures, ranked** (w50-w51, 10 hard-mode pulls; before the first tank death unless noted):
1. The race: Thorim melee 143 and Unbalancing Strike 16 over both windows, nearly all after the first tank death. Not
   a bug; the user keeps it polishing with no end condition.
2. Chain Lightning 10 (ring 8, camp 2): the opening chain through the corridor bear's charge stall (R31, w51 uld5 3),
   seeds inside a slot of 3-4 (w50 uld2, uld6), a spree (w50 uld6), low-health finishes (w51 uld6 +117 s camp ×2,
   +132 s slot 2).
3. Lightning Charge on the camp 4: slowed walkers short of their rows (w51 uld4 +31 s, a step 3.0 yd short under
   Frostbolt); a healer on a stalled walk (R31, w51 uld4 +106 s). Over 64 pulls 28 of 43 died short of a row.
4. Volley 4 (camp 3: w50 uld1 +98 s, w51 uld1 +77 s, uld2 +170 s; ring 1: w50 uld4 +58 s).
5. Blizzard on the tanks, 24% of their intake (the tank spot sits 4.1 yd off the track; the user keeps the waiting tank
   there; moving the anchor east is a full re-solve, boss doc).
6. Tank to slot chains: the solver's anchor term is angle-only; R29's wait covers those on the lit orb.
7. Gauntlet healers at the Rune Giant hold (`ThorimGauntletTrailSpot`'s no-path fallback): no phase 1 death since w44.
8. Self-res into the hazard: shared res code; not picked.

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
  Colossus; its no-path fallback is failure 7.
- R26: a camp shelter walk for orb 1 or 2 that passes another slot's body waits for Chain Lightning
  (`ThorimCampChainWait`, cap 1 s, reach guard, cast hold) (user) — held.
- R27: the bunny's next 8 yd of path counts as Blizzard for the ring (`ULDUAR_THORIM_RING_BLIZZARD_LEAD`) — held.
- R28: a stepped ring bot clear of the hazards re-steps while another slot's bot or point is in its jump;
  `RingBodyBridgesChain` counts another slot at its point too — held (slot to slot through a stepped slot 0 in
  w49-w51).
- R29: a ring walk or Blizzard step started while `ThorimChainWaitDue` holds, ending in another group's jump, waits
  for the chain (`ThorimRingChainWait`) (user) — held (w51 waiters dead by the chain 0).
- R30: `killing spree` held for ring bots outside 5.5-9.0 s after the last lit stamp — held (w51 10 sprees at
  5.6-8.4 s, in a chain 0, in the cone 0); a shelter row inside the live cone takes the cone step — not exercised (no
  west-seat cone in w51), no camp death on a row.
- R31: phase 2 walks re-issue once stalled 500 ms (`ThorimReleaseStalledWalk`); a tank bound for the off-tank spot
  holds its charges (`thorim waiting tank charge hold`) — pending.

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

## Round 31 (window 51: build c9b718c29 with R30; window 50: build 0200b8827 with R29)

**Data:** `window51-thorim.md` (R30 on R29) and `window50-thorim.md` (R29), session scratchpad. Traces w51
`603_{14,17,13,18,15,16}_thorim_{1795618428,1795618485,1795618398,1795618537,1795618445,1795618452}` (uld1, uld4,
uld2, uld5, uld3, uld6), read with `thorim.py --spree/--shelters/--chains/--ringwait/--blizzard/--stalled`; corpus
figures over the 73 hard-mode pulls since 11-24 18:00 (session scratchpad `thorim_r31/`).

| Pool | w51 (R30) |
|---|---|
| uld1 (A) | wipe 15.1%: Volley 1 (+77 s), then tanks from +102.6 s, melee 22, US 3, LC 2, Volley 1 |
| uld4 (A) | **kill, hard, 3**: LC 2 (+31 s cone step 3.0 yd short under Frostbolt; +106 s healer on a stalled walk), US 1 (+187 s) |
| uld2 (B) | **kill, hard, 3**: Volley 1 (+170 s), tank melee 2 (+177 s) |
| uld5 (B) | wipe 36.0%: Smash 1 (P1 healer), CL 3 (+11.9 s, the opening chain through the corridor bear), then tanks +119.8 s, melee 22, Blizzard 1, US 1 |
| uld3 (C) | kill, normal mode, 0 (phase 2 at 3:35) |
| uld6 (C) | kill, hard, 8: Blizzard 1 (ring +79 s), CL 3 (+117 s camp ×2 and +132 s slot 2, all low health), US 1, Blizzard 1 and melee 1 on tanks, CL 1 (+192 s) |

**Stop rules:** waived for Thorim (user, 2026-11-24). Done rule on R30's build: A (uld4, 3) and B (uld2, 3) in hard
mode; C only in normal mode (uld3) or with 8 (uld6). Not polished.

**R30 verdict.** Killing Spree hold: **held in A, B and C** (uld5 had no spree): 10 sprees from ring bots, all
5.6-8.4 s after the lit, in a chain 0, in the cone 0 (w49-w50: 4 chains, 3 dead past); the opening chain deaths this
window (uld5 3) hit no spree. Shelter rows in the live cone: **not exercised**, no cone with him on the west seat
(x ≤ 2108) in w51; no camp bot died on a row (w49 uld4 4). First tank death p50 +167 s (w49-w50 +120 s), raid %/s
0.38-0.54.

**Failure picked: walks stalled by the booking, and the waiting tank's charge** (failures 2 and 3).
- w51 uld5 +9.57 s: corridor bear Uldvab, walking from the jump landing to the off-tank spot (move issued +9.26 s),
  cast `feral charge - bear` (rel 18) and landed at (2119.1, −259.4), 6.1 yd south-east of him between slot 0
  (−120°) and slot 1 (16°). Its positioning moves then came back `dup` 17 times until +14.2 s, standing still 13.6 yd
  short. The +11.9 s chain ran slot 0 ×3, 8.6 yd (lust) to Uldvab, 6.7 yd to slot 1 ×2, 8.4 yd to slot 2 ×2: 3 dead,
  the wipe's early loss.
- w51 uld4 +101.9 s: healer Ulduae's shelter walk (25.6 yd) was issued while a cast pinned it (generator up, no
  spline), then refused `dup` at +102.9, +104.0, +105.1 s while it cast instants; the cone killed it at +106.1 s, not a
  yard walked.
- Cause: `MovementAction::IsDuplicateMove` (`MovementActions.cpp:1047`) refuses a repeat of the last destination for
  `MaxWaitForMove` (5 s) whether or not the bot moves (pitfalls: "A walk stopped short is never re-issued"), and
  Thorim's phase 2 movers had no release (Hodir, Algalon, Anub'arak each have one). The charge is a
  `CastReachTargetSpellAction` (`Charge` family); `thorim tank anchor guard` covers a tank only once on its spot, and
  the walking tank's positioning action returns false until it arrives (`UldActions_Thorim.cpp:470`), so lower
  actions run each tick.
- Corpus (73 hard-mode pulls, before the first tank death; `--stalled`): 373 stalls of 1 s or more (ranged 153,
  melee 143, tank 41, heal 36; 87 right after a Killing Spree, up to 6.5 s ~6 yd off the point), 1,250 bot-s; 31 of
  290 camp cone hits and 6 deaths on stalled bots; 12 chains through a stalled bot, 12 dead past it (10 in opening
  chains: the charged bear in 5 with 8 dead, a spree rogue in 2). Tank charges while the other tank held him: 49.
- **Classification: bug** (both). The walks are the strategy's own; the waiting tank's place is the off-tank spot
  (user, 2026-11-22).

**Fix** (Thorim files only, no size, mode or player branch):
- `ThorimReleaseStalledWalk` (`UldEncounter_Thorim.cpp`) clears `last movement` when the bot isn't moving and its
  booking is over `ULDUAR_THORIM_WALK_STALL_MS` (500) old; `ThorimPhase2PositioningAction` and
  `ThorimSifBlizzardAction` call it right before their `MoveTo` (after every wait and hold).
- `thorim waiting tank charge hold` (`UldDefinition_Thorim.cpp`): `Block` on `Family::Charge` for a tank while
  `ThorimTankBoundForOffTankSpot` (phase 2, on the floor, its phase 2 spot is the off-tank spot). The anchor with no
  spot (R20) still charges.
- Reader `--stalled` (tested in `test_thorim.py`); boss doc "A stalled walk re-issues after 0.5 s", "A tank bound
  for the off-tank spot holds its charges". `pb-check` passes.

**Next window should show** (hard-mode pulls):
- `--stalled`: refused repeats near 0 (w51 12-43 a pull), stalls of 1 s or more only cast pins (re-issued), well
  under w51's 3-9 a pull; cone hits and deaths while stalled 0; charges from a tank while the other held him 0
  (w51 3 of 5 hard pulls had one).
- `--chains` at +12 s: no tank group between two ring slots; `--spree` rogues back on their point ~1 s after the
  spree ends.
- R29 and R30 holding; melee in reach and %/s before the first tank death not lower.

**Seen, not picked:** cast-time spells pinning camp walkers in the lit cone (R23 breaks only channels; a candidate if
`--stalled` shows re-issued pins dying in the cone); low-health camp bots finished by a 2-hit chain (uld6); Volley on
the camp; slots at 60° under lust (8.8 yd against the 8.9 jump: uld5's slot 1 to slot 2 hop); Blizzard on the tanks.

**Questions for the user:** none.

**Shared code touched:** none (Thorim files, the Thorim reader and its test, the boss doc).
