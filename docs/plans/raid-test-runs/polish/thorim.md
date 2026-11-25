# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` (`--deaths`, `--phase2`, `--blizzard`, `--chains`, `--shelters`,
`--ringcone`, `--colossus`, `--chainwait`, `--ringwait`; `raidtest.py report` runs `--summary` on every counted
pull). Rounds 1-28: `history/thorim.md`.

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
after the lit stamp and landing p50 1.05 s after it, a third of the events casting nothing (one cause: the random pick
lands on a pet and fails the spell's player-only check); 8 targets at ×1.5 a hop, hop 8 yd (8.9 when both carry
Heroism/Bloodlust), **players only** (`SPELL_ATTR3_ONLY_ON_PLAYER`: pets never take a hop); hard mode adds Sif's
Frostbolt Volley (−50% speed), Frostbolt (−65%), Blizzard (−65% in a zone) and Frost Nova (root). Blizzard's bunny walks
the core's fixed path once at ~7 yd/s (west, then north of the tank spot, then round the camp), dropping a 10 s zone
every 2 s; two ring slot points sit ~5 yd off its west and north legs, so those slots step out on every Blizzard.

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
(11), wipes at 2.1-33.2%. Not polished on any one build. No pull yet on R28's or R29's code.

**Open failures, ranked** (window 46, 189 deaths):
1. The race: Thorim melee 121 and Unbalancing Strike 7, nearly all after the first tank death. Not a bug; the user
   keeps it polishing with no end condition.
2. Chain Lightning 15, 14 before the first tank death (11 ring): slot to slot 6 (uld3 stepped slot 2 into slot 1
   at 2.5 yd ×3, uld4 ×2 at 5.8 yd and ×1 under lust), tank to slot 2, the opening 2, camp cone steps meeting a
   stepped slot's cone step 3 (uld6), camp seed 1. Over 58 hard-mode pulls 27 of 38 ring-crossing deaths (opening
   aside) came in casts whose bridge closed in the 2.5 s before the landing: moves on the lit orb. R28 targets the
   stepped and deadband share, R29 the moves on the lit orb.
3. Lightning Charge 32, 3 before the first tank death (uld3 a ranged at +15 s, the opening; 2 camp +135 s).
4. Volley 7, Blizzard 5 (no ring melee; uld6 a camp ranged twice, self-res into the zone; tanks 2), Frostbolt 1
   (uld4 ring +152 s), Frost Nova 1.
5. Tank to slot chains: the solver's anchor term is angle-only, so a slot 60-68° off the anchor sits 5.4-8.6 yd
   from the holder; 5 deaths over 58 pulls. R29's wait covers those on the lit orb.
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
  passes only, never across the lit cone); `RingBodyBridgesChain` counts another slot at its point too — pending.
- R29: a ring walk or Blizzard step started while `ThorimChainWaitDue` holds, ending in another group's jump, waits
  for the chain (`ThorimRingChainWait`), only from a spot clear of other groups' bodies, outside a Blizzard zone and
  while the cone stays escapable (user) — pending.

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
  another slot's body waits to set off until Thorim's Chain Lightning has landed, taking R23's cone step when the late
  start leaves the row out of reach; replayed before it ships. Built in R26.
- 2026-11-25, Q1 (a) of round 28: a ring walk or step that starts while the orb is lit (cone dodge, Blizzard re-step,
  walk back) and would end within the 8 yd jump of another group's body or point waits until the cast lands (cap
  1 s), only when the cone is still escapable after the wait; replayed before it ships. Built in R29.

**Open questions:** none.

## Round 29 (no new pulls; R28 merged after window 48's build)

**Data:** no pull on R28's code yet; window 46 (`window46-thorim.md`) was read in round 28. Replay of the user's Q1
over the 58 hard-mode 25-man pulls since 11-24 18:00 (73 Thorim traces on disk, 15 normal mode) with session
scratchpad `thorim_r29/replay.py` (`summary.py`, `deaths.py`, `delta.py`, `mismatch.py` read its output).

**Stop rules:** waived for Thorim (user, 2026-11-24). **Round 28 verdict:** pending, no pull on its code.

**Built: the user's Q1 (a) of round 28, ring moves on the lit orb wait for Chain Lightning** (failure 2's lit-orb
share and failure 5's lit-orb tank bridges). Classification: plan change, answered by the user.

**Found on the way: Chain Lightning never hits pets.** The replay's hop model (nearest unhit body within 8 yd)
reproduced the group order of 281 of 446 hard-mode chains with pets as bodies and 360 without, the model running on
through pets where the real chain stopped. The core says why: 62131/64390 carry AttributesEx3 0x100,
`SPELL_ATTR3_ONLY_ON_PLAYER` (`spell.reference.csv`), so `Spell::GetSearcherTypeMask` (`Spell.cpp:2161`) searches
players only and `SpellInfo::CheckTarget` (`SpellInfo.cpp:1813`) fails a non-player, including a seed
`SelectTarget(Random)` picks off the threat list (`boss_thorim.cpp:738`; `CheckCast`, `Spell.cpp:6115`). The boss
doc said pets chain ("totems and nothing else"); corrected there and in `docs/systems/observability.md`, whose
Thorim hypothesis (pets explain 7-8 hop chains) it refutes. R26's camp replay included pets.

**Replay** (per lit orb: a cast that starts by lit + 1 s holds until it lands, else the cap; a ring order from
`thorim phase 2 positioning action` or `thorim sif blizzard action` while due, ending within 8.5 yd of another
group's living floor player or another slot's point, holds; a bot inside the ring's cone line holds only if its
destination is in reach at its speed by lit + 4.3 s; a held bot stands where ordered, then walks straight; the chain
re-run hop by hop over players at the landing):

| Rule | Holds | Bot-s a pull | Hop 5+ hits | Group crossings | Chains grew / shrank | Deaths gone (reproduced chains) |
|---|---|---|---|---|---|---|
| as fought | - | - | 48 | 96 | - | - |
| destination only (the answer's text) | 996 | 13.9 | 38 | 71 | 7 / 27 | 15 of 44 (12 of 29 early) |
| **plus a clear spot (shipped)** | **703** | **10.3** | **31** | **74** | **1 / 20** | **12 of 44 (9 of 29 early)** |

Shipped rule: wait p50 0.86 s, p90 1.15, max 1.6; 591 holds before the first tank death; 587 ring walks, 77 steps, 39
walks back; bridged by a tank 314, another slot's body 301 or point 212, the camp 98; 71 start out of reach, 42 find the
bot already walking (not stopped in code); the cone test lets 16 walk; at the cone 2 held walks newly inside, 8
newly out; no held bot stands in a zone. Normal mode: 159 holds over 15 pulls. Early chains (before the first tank
death): hop 5+ hits 33 → 22.

**Fix** (`UldEncounter_Thorim.cpp/.h`, `UldTriggers_Thorim.cpp`, `UldActions_Thorim.cpp`, `UldMultipliers_Thorim.cpp`,
`UldDefinition_Thorim.cpp`; Thorim only, no size, mode or player branch, a human is a body): `ThorimRingChainWait`
gates the ring positioning walk (trigger and action, after `ThorimRingWantsMove`, no arrival latch) and the ring
Blizzard step; `RingSpotInOtherGroupJump` tests the destination (bodies and other slots' points) and the bot's spot
(bodies); `ThorimChainWaitSetOff` is shared with the camp wait; `ThorimCampConeReach` / `ThorimCampShelterInTime`
renamed `ThorimConeReach` / `ThorimWalkBeatsCone`. Per bot per lit stamp the hold releases once (`ringChainHolds`,
also in `TickThorim`), note `thorim.ringchainwait` `wait walk|step` / `none`. While waiting: the movement guard
counts the bot settled, new rule `thorim ring chain wait` blocks Reach and Charge, the chain-wait cast hold covers it.
Reader `--ringwait` (tested in `test_thorim.py`); boss doc "A ring move under the lit orb waits for the chain",
"Pets never chain". `pb-check` passes.

**Review fix:** nothing kept a held bot out of Blizzard. The replay checked zones only at the hold's start (0 of 703),
but a step fires on the bunny's 8 yd lead, which it walks in about a second, so a zone can land on a bot held for a
step. `ThorimRingChainWait` now releases a bot carrying a zone's aura (62576/62602), at the start or mid-wait; boss
doc bullet "outside Blizzard".

**Next window with R29 should show** (hard-mode pulls; R28's window comes first):
- `--ringwait`: ~12 waits a pull, p50 ~0.9 s, most released by the landing; waiters hit by the chain while walking
  ~0; cone hits on waiters ≤ 1 a window.
- `--chains`: ring-crossing chain deaths before the first tank death down about a third on R28's window, hop 5+ ring
  hits down; R27 and R28 holding.
- Watch: melee in reach and %/s before the first tank death not lower (most waits stand in reach).

**Seen, not picked:** `ThorimPetsOffRingSlot` now guards hops pets cannot take (harmless, no uptime cost; a cleanup
candidate); walks already under way at the lit are not stopped (6% of holds); the opening chain; `--phase2`'s
"bodies within 8 yd of the waiting tank" still counts pets.

**Questions for the user:** none.

**Shared code touched:** none (Thorim files and the Thorim reader); shared doc `docs/systems/observability.md`
(one bullet: Thorim's chain is player-only).
