# Mimiron polish log (25-man hard mode, Firefighter)

Rounds 1-20: [history/mimiron.md](history/mimiron.md).

## State

**Reader and scenario.** `tools/botobs/bosses/mimiron.py <trace>` (`--help` lists the sections; scratch
scripts live in the lane scratchpad, never carried forward). Scenario `uld-mimiron`: the tank presses the
Big Red Button as the pull's start. Hard mode is real when Summon Flames (Initial) 64563 casts at ~7.1 s,
Flames 64566 ticks from the opening and VX-001 casts Emergency Mode 64582 at phase 2; a kill counts only
then. Berserk 64238 at 10:00 is the clock.

**Fight model.** Z1 13 s; P1 MK II 81-94 s; Z2+H1 48 s (the raid holds a band 40 yd out, walks in 12.75 s
before P2); P2 VX-001 82-100 s; H2 24 s; P3 ACU 115-150 s, set by Magnetic Core landings ~54 s apart (20-47
points each; airborne it loses ~0.2%/s); H3 32 s (fire bots culled); P4 the three parts at 50%, 12.1M,
100-160 s. A clean pull reaches P4 by 6:40-7:36. P4 damage runs ~3.5k/s per living member, so every body
lost before P4 costs the clock. P4 is also a mana race: Hand Pulse takes 11-18k/s off the raid, and once
two of four healers are out (dead or under 10% mana) it kills (`--pulse`). What loses bodies: the P2
storm after a barrage (Rapid Burst, Heat Wave, the Frost Bomb), P3 Bomb Bots and Water Spray, P4 mines,
then P4 Hand Pulse once healers drop out.

**Compositions on `36d7f19b9` (window 32).**

- A (uld1, uld4): **met** the done rule, uld4 kill 8:37 with 2 deaths (mines on barrage legs), hard mode
  (64563 at 7.1 s, Emergency Mode 2:23). uld1 wipe: 13 before Berserk, Hand Pulse 9 from 9:07 after a
  healer died to a mine on a bomb leg (8:06) and the disc priest ran dry.
- B (uld2, uld5): uld5 kill 10:12 with 8 (P2 3, Bomb Bot, P4 mines 2, Plasma Ball 2), parts down after
  Berserk; uld2 wipe, 13 before Berserk (P2 4, Water Spray, Hand Pulse 8 from 8:26).
- C (uld3, uld6): uld6 kill 9:58 with 21 (P2 Rapid Burst 3, P4 Hand Pulse 13 from 8:58); uld3 wipe, 22
  before Berserk (P4 Hand Pulse 13 from 8:15 while two healers were off the floor, see round 21).

**Open failures, ranked** (window 32, 78 deaths before Berserk):

1. P4 Hand Pulse 43 (window 30: 10): 36 after the second healer was out (8:08-8:53 in five pulls), uld3's
   other 4 with two healers above the floor (fixed r21). Plan question Q11 (healer mana).
2. P2 storm 13: Rapid Burst 8, Heat Wave 2, a main tank twice to the barrage, Frost Bomb 1.
3. P4 mines 10, two in each of five pulls, mixed movers (barrage legs 3, `combat formation move` 2).
4. ACU Plasma Ball 6 (late P4).
5. Water Spray 3 (window 30: 5); Bomb Bots 2 (window 30: 12; Q10 open).
6. P4 Laser Barrage over a live Frost Bomb: none this window (3 of 21 pulls in windows 22-30).

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
- r20: a spray step starts the 2.5 s fire hold; held (walk-backs 0-1 a pull).
- r21: the flee fan and its `MoveAway` fallback refuse a destination off the floor (Z 803.11); pending.

**Tried and rejected.**

- The 15% fire bot sweep: it fell inside landings, and dead fire bots only let fire build (Q3).
- Fire bots kept into P4: 25 s of P4, spray deaths and silences for ~0.3M of healable fire (Q4).
- Uncapped fewest-fire settle: 131-140° turns walked ranged across VX-001 into the storm (Q9).
- A 20 yd P4 healer row: spares a quarter of Shock Blast flights, costs a tenth of heal reach (Q8).
- `MoveAwayFromCreature` Bomb Bot step (30 yd legs along the chase); a victim-centred blast disc (left the
  bomb's side open); the siren screen on spray escapes (blocked them).
- A main-tank `Kill()` on adds under cheats: removed.
- A room-radius bound on detour waypoints: the floor is no circle round the room centre (P1 is fought 53
  yd west, bots stand 60-70 yd out), so it needs navprobe.

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

**Open questions.** Q10 (round 20): who takes the Bomb Bots in phase 3. Q11 (round 21): phase 4 healer
mana. Hand Pulse was picked in rounds 15 and 21 (round 17 fixed a feeder); a third pick without Q11
answered is stuck.

**Watch.** Siren legs across a spray lane (`FleeFan` screens the destination only). Detour waypoints off
the room: uld3's druid walked a `detour +65` leg (waypoint 105 yd off centre) onto a ledge at Z 372.4,
90-100 yd west, for 28 s (round 15: uld2, two healers 20 s). P4 `combat formation move` (disperse 4.0)
runs unscreened off-slot: 48 walk-ins into live bombs, 12 mine hits (3 kills) in window 32. A bot
stuck off the navmesh (every path search `nopath`) now has no flee leg at all. `mimiron.phase`
flickers (uld3: H3 0.3 s, P2b 0.25 s), splitting reader spans. Shared code, not raised: healer mana (priests overheal P3
at 55-87%), Reincarnation into the killing lane, Ice Block ending inside a Frost Bomb fuse. DC side:
auras carried over a logout, Boomer XP-500 trash west of the room.

## Round 21 (build 36d7f19b9, round 20 built)

**Data:** window 32 (`window32-mimiron.md`, `--head 36d7f19b9 --pulls 1`), one pull per pool, no DC
veto. Traces `603_<inst>_mimiron_<epoch>`: uld4 `4_…412`, uld1 `1_…416`, uld2 `2_…420`, uld5 `5_…419`,
uld3 `3_…415`, uld6 `6_…396` (epochs 17955443xx-44xx); window 30 (`16-21_…17955393xx`) to compare;
windows 22-32 (40 traces) for the floor scan. Scratch `mim21/`.

| Pool | Out | P4 at | Deaths before Berserk | Two healers out |
|---|---|---|---|---|
| uld4 A | **kill** 8:37 | 6:40 | 2: P4 mines (barrage legs) | never |
| uld1 A | wipe | 7:14 | 13: P2 Rapid Burst; P3 Water Spray; P4 mines 2, Hand Pulse 9 (9:07-9:35) | 8:31 |
| uld2 B | wipe | 7:36 | 13: P2 Rapid Burst 3, Frost Bomb; P3 Water Spray; P4 Hand Pulse 8 | 8:53 |
| uld5 B | **kill** 10:12 | 7:09 | 7: P2 Heat Wave 2, Rapid Burst; P3 Bomb Bot; P4 mines 2, Plasma Ball | 8:29 |
| uld3 C | wipe | 7:06 | 22: P2 barrage 2 (main tank twice); P3 Water Spray; P4 Hand Pulse 13, Plasma Ball 3, mines 2, Frost Bomb | 8:21 |
| uld6 C | **kill** 9:58 | 7:04 | 21: P2 Rapid Burst 3; P3 Bomb Bot; P4 Hand Pulse 13 (8:58-9:36), mines 2, Plasma Ball 2 | 8:08 |

**Stop rules:** A met the done rule (uld4, 2 deaths, hard mode); B (uld5, 8) and C (uld6, 21) not. Not
polished. Not stuck (above).

**Verdict on round 20 (a spray step starts the 2.5 s fire hold): held in A, B and C.** `--firebots`:
walk-backs after a spray step 0-1 a pull (126-861), spray hits after one 0 (19 of 32); Water Spray 3
deaths (5), none after a walk-back; melee P3 moving 28-38% (43-52%), `fire bot <-> reach melee` A-B-A
28-66 (328/887/396); P3 117-148 s (115-150).

**Failure picked: P4 Hand Pulse (43 of 78).** It kills once healing runs out, and the healers went out
three ways.

- `--pulse` (new line): 36 of 43 came after the second of four healers was dead or under 10% mana
  (window 30: 6 of 10, that point at 7:55-10:02 or never). uld5 crossed it at 8:29 and lost nobody to
  Hand Pulse, so it sets the odds, not the outcome.
- Dry priests (uld1, uld2, uld6): Shadowfiend at ~1:05-1:30 and ~6:05-6:35, Hymn of Hope at 3:34-4:11 in
  every pull, so neither reaches P4; P3's 6.1-6.4k/s healed at 55-87% overheal. uld6's disc and holy
  priests sat at 2-7% from ~8:00-8:45. uld4, the one pull never short, cast its second Shadowfiend at 7:21
  and Mana Tide at 7:52. Plan change: Q11.
- Dead healers: uld1 Uldxae, a mine on a Frost Bomb leg (8:06.7); uld2 Uldyaf, P2 Rapid Burst; uld6
  Uldwaf, a mine (9:03.9). Mixed movers, not picked.
- **Off the floor (uld3, bug).** At 7:54.906 Uldzac's `frostbomb ok` leg went to (2700.9, 2610.4,
  **803.11**); he rose to Z 451 for 24 s and died walking back (8:20.6). Uldzaf, the druid with 56-60%
  mana, walked a `detour +65` leg off the room at 7:56.8 (Watch), then a rocket flee at 8:30.6 to Z
  803.11 flew him to 441 until 8:52. With Uldzae dry (8%), uld3 healed on the shaman alone 8:02-8:20 and
  lost 9 in 8:15-8:31. The off-tank Uldzab's bomb leg to Z 803.11 (8:01.5) never moved him; the blast
  killed him (8:04.8). Also uld3 Uldzar 20 s, uld5 Uldvau 23 s, uld6 Uldwac 3 s.
- Code: `FleeFan` (`UldActions_Mimiron.cpp:172`, `:357`) takes `Map::CheckCollisionAndGetValidCoords`'
  answer as an exact leg. Past the room's edge a player's check finds no vmap ground and returns true at
  `GetGridHeight`, 803.11, the terrain above the instance; the exact `MoveTo` then flies a straight
  spline. Z 803 legs, all from the flee actions: 13 in windows 22-32's 40 pulls, 8 in window 32.

**Fix** (Mimiron only, every size and mode, with or without a player): `IsMimironSpotOnFloor` refuses a
point more than `ULDUAR_MIMIRON_FLOOR_TOLERANCE` (3) off the room centre's Z (364.32; standing samples
364.3, up to 366.3 at 75 yd west); the fan and the least-burn fallback skip an exact candidate failing it as `move`. A
failed check keeps the bot's Z and the path search, as before. Review: the unit flee's last resort
swapped the generic `MoveAway` for a copy that skips such a point too. uld3's Uldzap (7:57.254) and
Uldzab (8:01.544) took `frostbomb ok -90` to Z 803 after `frostbomb fallback (move22)`, every path
`nopath`; with the fan refusing it, `MoveAway`'s own -90 bearing would have flown them up. Reader
`--floor` (moves sent off the floor, bot-seconds above it, each stretch over 3 s with the move before
it) and `--pulse`'s two-healers-out time with the Hand Pulse deaths after it; tests for both. Findings doc: the fan's floor trap and
the P4 mana race; `pitfalls.md`: the off-mesh check can also rise to the grid height.

**Next window should show** (`--floor`, `--pulse`):

- No accepted move at Z 803 (`moves sent off the floor` only knockback follow-ons at 368-379) and no
  stretch above the floor longer than a knockback, except a walk off the room (uld3: 20-50 s, uld5 23 s).
- Hand Pulse unchanged until Q11 is built; the two-healers-out time and deaths after it are the baseline
  (36 of 43). Round 20's walk-backs still ~0.

**Not picked.** P2 storm 13 (no single mover). P4 mines 10 (mixed movers). The off-room detour and the
P4 disperse (Watch). ACU Plasma Ball 6.

**Questions for the user.**

- **Q11 (plan change): phase 4 runs out of healer mana.** Hand Pulse hits the raid for 11-18k a second
  through phase 4's 100-160 s, and once two of the four healers are dead or under 10% mana it kills: 36
  of window 32's 43 Hand Pulse deaths (of 78 before Berserk) came after that point, which fell at
  8:08-8:53 in five of six pulls. The priests run dry first: they cast Shadowfiend at ~1:10 and ~6:15
  and Hymn of Hope at ~3:50, so phase 4 gets neither. Options: (a) under Firefighter, bot priests hold
  Hymn of Hope and their second Shadowfiend (back at ~6:05-6:35) until phase 4 opens (6:40-7:35): about
  two fifths of a priest's mana each in phase 4, no DPS lost (uld4, the only pull never short, had its
  second Shadowfiend at 7:21); (b) the scenario takes a fifth healer at 25 (the roster drafts allow 5):
  a quarter more healing in phases 2 and 4, one DPS fewer, ~6% of phase 4's output, ~9 s on a clock B
  and C already lose by seconds; (c) keep. Recommendation: (a), then (b) if two healers still drop out
  before ~9:15.

**Shared code:** none (Mimiron flee fan and spot helper, findings doc, the `pitfalls.md` entry, reader and
its test, this log).
