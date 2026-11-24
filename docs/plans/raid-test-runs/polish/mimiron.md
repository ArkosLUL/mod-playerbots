# Mimiron polish log (25-man hard mode, Firefighter)

Rounds 1-19: [history/mimiron.md](history/mimiron.md).

## State

**Reader and scenario.** `tools/botobs/bosses/mimiron.py <trace>` (`--help` lists the sections; scratch
scripts live in the lane scratchpad, never carried forward). Scenario `uld-mimiron`: the tank presses the
Big Red Button as the pull's start. Hard mode is real when Summon Flames (Initial) 64563 casts at ~7.1 s,
Flames 64566 ticks from the opening and VX-001 casts Emergency Mode 64582 at phase 2; a kill counts only
then. Berserk 64238 at 10:00 is the clock.

**Fight model.** Z1 13 s; P1 MK II 82-91 s; Z2+H1 48 s (the raid holds a band 40 yd out, walks in 12.75 s
before P2); P2 VX-001 82-96 s; H2 24 s; P3 ACU 115-150 s, set by Magnetic Core landings ~54 s apart (20-47
points each; airborne it loses ~0.2%/s); H3 32 s (fire bots culled); P4 the three parts at 50%, 12.1M,
130-140 s at 85-90k/s with 25 alive. A clean pull reaches P4 by 6:44-7:27. P4 damage runs ~3.5k/s per
living member, so every body lost before P4 costs the clock: raids entering P4 with 20-23 meet Berserk with
the parts at 2-12%. What loses bodies: the P2 storm after a barrage (Heat Wave, Rapid Burst, the Frost
Bomb), P3 Bomb Bots and Water Spray, then P4 Hand Pulse once healers fall.

**Compositions on `897faf0d8` (window 30).**

- A (uld1, uld4): **met** the done rule, uld4 kill 9:11 with 3 deaths, hard mode (64563 at 7.2 s, Emergency
  Mode at 2:26); uld1 kill 9:48 with 5.
- B (uld2, uld5): no kill. uld2 Berserk with the parts at 5-7% (P4 70k/s, 19.5 alive); uld5 lost P3 (264 s,
  five landings) after losing a healer and three ranged in P1-P2.
- C (uld3, uld6): no kill. Berserk with the parts at 2-5% (69k/s, 21.7 alive) and 9-12% (59k/s, 20.1).
- B and C's off-tank is a bear: the ACU sat on it 63-83% of P4 (A's blood DK 99%).

**Open failures, ranked** (window 30, deaths before Berserk):

1. Bomb Bots 12: chased ranged 7, spawn blasts on the centre pile 4, a Death-Gripped bomb's collateral 1.
   Plan question Q10.
2. Phase 2 storm 10 (5 inside the 25 s after the beams); Rapid Burst during Frost Bomb escapes.
3. P4 Hand Pulse 10, all with 1-3 healers left or in the last 15 s.
4. P4 Laser Barrage over a live Frost Bomb (3 of 21 pulls since window 22, all ~9:44-9:52): the barrage
   step walked 22-24 legs into the bomb's radius, then cone and fire refused every escape (`frostbomb
   unsafe`): uld3 4 and uld6 3 dead to the blast, uld6 5 more pinned in Flames.
5. Water Spray 5 (4 after a walk-back, fixed in round 20).
6. Mines 4; ACU Plasma Ball 2 (the bear losing the ACU).

**Fixes in place** (round: what; verdict).

- r1: hazard read without a sight test, standing bots leave fire at any health in a handover; held.
- r2-3: handover hold band and walk-in, one code path with or without a player; held.
- r4: core fetch at any ACU health; held.
- r5: spray escape crosses to the far side, lanes 18 yd, no siren screen on it; held. Player core carrier:
  untested (no player-led pull).
- r6: melee walk back into sight after Shock Blast; held.
- r7: holder screen (60.1 yd) and unreachable step; held, no MK II heal since.
- r8: stranded walk-in least-burn leg; Rapid Burst read via the Burst Target; both held.
- r9: Bomb Bot step-out off the blast line, a few yards; held (collateral near 0 outside spawn blasts).
- r10: spray dodge never defers to the barrage; held.
- r11: short barrage arcs over P4 mines; held for ranged.
- r12: fire bot cull in H3 (Q4); ACU off-tank in P4 (Q5, leaks with a bear); cast cut in fire; least-burn
  flames fallback; all held.
- r13: bomb leg test for the formation and the main tank's anchor; charge guard on Charge, Intercept,
  Killing Spree; held.
- r14: Revive Pet hold all fight; held.
- r16: P2 settle off fire (Q6); P4 healers on the 18 yd front row (Q7); held.
- r17: mine trigger and step at 3 yd ground distance; held.
- r19: settle turns at most 90° (Q9); held in window 30.
- r20: a spray step starts the 2.5 s fire hold; pending.

**Tried and rejected.**

- The 15% fire bot sweep: it fell inside landings, and dead fire bots only let fire build (Q3).
- Fire bots kept into P4: 25 s of P4, spray deaths and silences for ~0.3M of healable fire (Q4).
- Uncapped fewest-fire settle: 131-140° turns walked ranged across VX-001 into the storm (Q9).
- A 20 yd P4 healer row: spares a quarter of Shock Blast flights, costs a tenth of heal reach (Q8).
- `MoveAwayFromCreature` Bomb Bot step (30 yd legs along the chase); a victim-centred blast disc (left the
  bomb's side open); the siren screen on spray escapes (blocked them).
- A main-tank `Kill()` on adds under cheats: removed.

**User answers in force.**

- One code path: a strategy runs the same with or without a real player; a player only changes who does
  a job he does himself.
- Q1: every Firefighter raid holds a band ~40 yd out through the 1→2 handover and walks in over its last
  ~13 s.
- Q3: no 15% fire bot sweep. Q4: fire bots kept through P3, culled in H3. Q5: the off-tank tanks the ACU
  in P4.
- Q6 (a) with Q9 (a): once the beams stop, the P2 wedge takes the fewest-fire 120° arc within 90° of the
  re-aim.
- Q7 (a): P4 healers on the wedge's front row; Q8: that row stays at 18 yd.

**Open questions.** Q10 (round 20): who takes the Bomb Bots in phase 3.

**Watch.** Siren legs across a spray lane (42 of 575 in windows 25-27, 2 dead; `FleeFan` screens the
destination only). Shared code, not raised: healer mana in P4, Reincarnation into the killing lane, Ice
Block ending inside a Frost Bomb fuse. DC side: auras carried over a logout (a normal-mode void in round 4),
Boomer XP-500 trash west of the room.

## Round 20 (build 897faf0d8, round 19 built)

**Data:** window 30 (`window30-mimiron.md`, `--head 897faf0d8 --pulls 1`), one pull per pool, no DC
veto. Traces `603_<inst>_mimiron_<epoch>`: uld4 `20_…381`, uld1 `16_…397`, uld2 `17_…401`, uld5
`19_…384`, uld3 `18_…396`, uld6 `21_…390` (epochs 17955393xx); windows 22-30 (21 traces) for the Bomb Bot
and spray rates. Scratch `mim20/`.

| Pool | Out | P1/P2/P3 s | P4 | Deaths before Berserk |
|---|---|---|---|---|
| uld4 A | **kill** 9:11 | 85/85/117 | 6:44, 89k/s, 25 alive | 3: mine, Heat Wave, Bomb Bot |
| uld1 A | **kill** 9:48 | 86/85/145 | 7:13, 85k/s | 5: Bomb Bot (gripped), Water Spray 2, Hand Pulse 2 |
| uld2 B | wipe | 83/96/150 | 7:27, 70k/s, 19.5, parts 5-7% | 14: P2 Rapid Burst 3; P3 ACU 1, Bomb Bot 1; P4 Hand Pulse 5 (9:49-9:53), Frost Bomb, Flames, mine, ACU |
| uld5 B | wipe | 82/83/**264** | 9:06, 15 alive | 11: P1 Napalm; P2-H2 3; P3 Bomb Bot 3, Water Spray 2, Assault Bot; H3 Water Spray |
| uld3 C | wipe | 83/90/145 | 7:16, 69k/s, 21.7, parts 2-5% | 10: P2 3; P3 Bomb Bot; P4 Hand Pulse, Flames, Frost Bomb 4 (9:57) |
| uld6 C | wipe | 83/86/149 | 7:14, 59k/s, 20.1, parts 9-12% | 21: P1 Flames; P3 Bomb Bot 5 (a spawn blast 4); P4 mines 2, Hand Pulse 2, Rocket, Flames 7, Frost Bomb 3 (9:46-9:56) |

**Stop rules:** A met the done rule (uld4, 3 deaths, hard mode); B and C have no kill. Not polished. Not
stuck: Bomb Bots were picked once (round 9, the step-out), and this round asks rather than repeats.

**Verdict on round 19 (settle turn capped at 90°): held.** Every settle 90° or less: `fire` 75° (uld4),
35° (uld1), 80° (uld2), 84° (uld3), 68° (uld6), `keep` (uld5); window 27 turned 126° and 129°. In the 25 s
after the beams healers moved 15-35% and healed 688-933k, raid deaths 1/0/3/0/1/0 (126-140° turns: 0-12).
uld2's three died to Rapid Burst on the escape from the Frost Bomb that landed 3.8 s after its settle, 21
inside its 30 yd; the storm, not the turn.

**Failure picked: Bomb Bots (12, all six pulls). Plan change.**

- Bombs spawn on the ACU's ground point (z 364.34) with a victim already picked (uld6 4:45.730: victim
  Uldwax 4.2 yd off, blast 208 ms later). The ACU hovers ~16 yd up with a 3D 30 yd chase, so it parks over
  its victim: the main tank, pinned to the room centre by `p3tank`, in 73 of 109 bombs (windows 22-30).
  The findings doc's "30 yd standoff, 3.7 s of free fire" never held: chased ranged began 9-22 yd off.
- The 109 bombs: 50 killed; 52 chases, a chased ranged DPS dead in 21 of 33, a tank in 2 of 13; 7 spawn
  blasts (victim within 5.5 yd), 25 hits, 6 dead. uld6's hit ten melee and a ranged on a Junk Bot beside
  the main tank: 4 dead, and P4 opened with 23 at 59k/s.
- The tank `lose aggro` taunt took 23 bombs (Dark Command, else Death Grip; Growl; Feral Charge); the tank
  always lived, and others were hit only where he stood in the pile (uld1 4:34.4, Uldxam's Death Grip, a
  melee dead; 3 dead in 21 pulls).
- Classification: every rule does what it intends (ranged stand and shoot; no step answers a 0.2 s blast);
  a fix is a new assignment or position: Q10.

**Second, independent and small: the spray step is walked straight back (bug, fixed).**

- `mimiron fire hold guard` holds `reach melee`, `reach spell`, heal reach, `set behind`, `follow` and the
  target approach 2.5 s after a flames dodge, but after a spray step only while the bot is inside a lane
  (3.5 yd); the step lands it 5.0 off (`MimironFireBotAction::Execute` never called
  `NoteMimironFireDodge`), so the next tick's mover walks it back.
- Window 30: 3,327 walk-backs within 2.5 s of the bot's own spray step in P3 and H3, 46% ending in a lane
  (other reach legs 18%); P3 `fire bot <-> reach melee` A-B-A 328/887/396 (uld1/uld5/uld6). 19 of 32 P3
  spray hits and 4 of 5 spray deaths came within 3 s of one (uld1 Uldxay and Uldxai, uld5 Uldvay and
  Uldvag); windows 22-30: 10 of 21 P3 and H3 spray deaths.
- Fix (Mimiron only, every size and mode, with or without a player): a spray step that issues calls
  `NoteMimironFireDodge`. Cost: that hold would have covered 13-24% of melee P3 time in window 30.
- Reader `--firebots`: walk-backs after a spray step and the spray hits within 3 s of one (test). A step
  is an accepted fire bot leg whose last flee note reads `spray ok` or `spray fallback`: `NoteDerived`
  skips a repeated note, so the notes alone missed 41% of them (review). Findings doc: the hold, the
  Bomb Bot spawn geometry (ACU a median 7-9 yd off its victim in five pulls) and its open item.

**Next window should show** (`--firebots`, `--flips`, `--walk`, `--bombbots`):

- P3 walk-backs after a spray step near 0 (126-861 a pull now), `fire bot <-> reach melee`/`set behind`/
  target approach A-B-A far under 328/887/396, spray hits after a walk-back near 0 (19 of 32).
- Water Spray deaths under 5; melee P3 moving share under 43-52% and P3 length no longer (115-150 s with
  no early losses). If melee output on adds falls, shorten the hold for spray steps.
- Bomb Bots unchanged until Q10 is answered; round 19's settle checks as above.

**Not picked.** P2 storm (10): no single mover. The late P4 barrage over a live Frost Bomb (12 dead in two
pulls at ~9:44-9:57; the parts were 2-12% at Berserk either way; choosing between beam and bomb is not
small). P4 Hand Pulse (10) with 1-3 healers. The bear off-tank losing the ACU (B, C).

**Questions for the user.**

- **Q10 (plan change): who takes the Bomb Bots in phase 3.** A Bomb Bot spawns under the ACU, which
  hovers over the main tank at the room centre, and runs at whoever it picks at 8 yd/s; its blast
  (19.7-25.5k) one-shots a caster, not a tank. Ranged stand and shoot by design and die in 21 of 33 chases;
  a tank dies in 2 of 13; and a bomb that picks someone standing at the centre blows at once on everyone
  there (uld6: 11 hit, 4 dead). It was the top killer in window 30 (12) and costs B and C the bodies that
  set their P4 clock. Options: (a) the off-tank takes every Bomb Bot: he stands 10-12 yd off the ACU's
  ground point, clear of the melee and the wedge, taunts each bomb as it spawns (Growl, Dark Command,
  Righteous Defense, Hand of Reckoning or Taunt, all 8 s against one bomb every 15 s or more) and takes the
  blast alone while ranged keep shooting it; (b) a chased ranged uses its own immunity or defensive (Ice
  Block, Dispersion, Divine Shield, Barkskin), which covers 3-4 of ~10 ranged; (c) keep. Recommendation:
  (a). Neither option stops a spawn blast on the centre pile (6 dead in 21 pulls); that needs melee to
  kill adds away from the main tank, which I would ask about only if spawn blasts stay after (a).

**Shared code:** none (Mimiron action, findings doc, reader and its test, this log).
