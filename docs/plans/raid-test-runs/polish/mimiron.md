# Mimiron polish log (25-man hard mode, Firefighter)

Rounds 1-21: [history/mimiron.md](history/mimiron.md).

## State

**Reader and scenario.** `tools/botobs/bosses/mimiron.py <trace>` (`--help` lists the sections; scratch
scripts live in the lane scratchpad, never carried forward). Scenario `uld-mimiron`: the tank presses the
Big Red Button as the pull's start. Hard mode is real when Summon Flames (Initial) 64563 casts at ~7.1 s,
Flames 64566 ticks from the opening and VX-001 casts Emergency Mode 64582 at phase 2; a kill counts only
then. Berserk 64238 at 10:00 is the clock.

**Rosters.** From window 34 every pool has all eleven professions at 450 with their perks and profession
enchants (DC `.dc test roster professions`): compare deaths with window 33 and older only by mechanic and
target, never by total.

**Fight model.** Z1 13 s; P1 MK II 81-94 s; Z2+H1 48 s (the raid holds a band 40 yd out, walks in 12.75 s
before P2); P2 VX-001 72-100 s; H2 24 s; P3 ACU 115-171 s, set by Magnetic Core landings ~54 s apart
(20-47 points each; airborne it loses ~0.2%/s); H3 32 s (fire bots culled); P4 the three parts at 50%,
12.1M, 100-160 s. A clean pull reaches P4 by 6:30-7:15. P4 damage runs ~3.5k/s per living member, so
every body lost before P4 costs the clock; both window 34 wipes reached Berserk with the parts up after a
long P3 (155-171 s, P4 at 7:09 and 7:43). P4 is also a mana race: Hand Pulse takes 11-18k/s off the raid,
and once two of four healers are out (dead or under 10% mana) it kills (`--pulse`); on the profession
rosters that point came at 8:00-9:16 or never and Hand Pulse killed 3 before Berserk. What loses bodies:
P4 mines, the P2 storm after a barrage (Rapid Burst, Heat Wave, the Frost Bomb), Water Spray, a late P4
Frost Bomb, P3 Bomb Bots.

**Compositions on `6464b824a` (window 34).**

- A (uld1, uld4): **met**, uld1 kill 9:10 with 3 (P3 Flames, P4 mines 2), uld4 kill 9:13 with 3 (Heat
  Wave, Hand Pulse, mine), hard mode in both.
- B (uld2, uld5): **met**, uld5 kill 9:05 with 4 (P4 Plasma Ball, Shock Blast, Hand Pulse, mine), hard
  mode. uld2 wipe at Berserk, 7 before (P3 Plasma Ball, Assault Bot; P4 mine, Frost Bomb 4 at 9:53).
- C (uld3, uld6): not met by one death. uld6 kill 9:51 with 6 (P3 Water Spray 2, Bomb Bot; P4 mines 2,
  Hand Pulse); uld3 wipe at Berserk, 9 before (P2: 5 melee, 6 deaths, to Rapid Burst and Heat Wave at
  3:26-3:31, 15 s after barrage 1; P4 Water Spray 2, mine).

**Open failures, ranked** (window 34, 32 deaths before Berserk):

1. P4 mines 8, in every pull; mixed movers (Watch: the P4 disperse).
2. P2 storm 7: uld3's melee sector lost 5 bots in 5 s after barrage 1; uld4 Heat Wave 1.
3. Water Spray 4 (uld6 P3 2, uld3 P4 2); Frost Bomb 4 (uld2, one bomb at 9:53).
4. P4 Hand Pulse 3 (window 32: 43 on the old rosters). Plan question Q11.
5. Plasma Ball 2; Bomb Bot 1 (Q10 built round 22); Flames, Shock Blast, Assault Bot 1 each.

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
- r21: the flee fan and its `MoveAway` fallback refuse a destination off the floor (Z 803.11); held, no
  flee leg left the floor in window 34 (window 32: 8).
- r22: the off-tank takes every P3 Bomb Bot (Q10 a): `p3bomb` slot 12 yd off the ACU's ground point at
  120° off the wedge, class taunt on each bomb, stands in the blast, holds the spot and drops non-bomb
  targets; pending.

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
- Q10 (a) (2026-11-24): the off-tank stands 10-12 yd off the ACU's ground spot, taunts every phase 3 Bomb
  Bot and takes the blast; built in round 22.

**Open questions.** Q11 (round 21): phase 4 healer mana. Hand Pulse was picked in rounds 15 and 21 (round
17 fixed a feeder); a third pick without Q11 answered is stuck.

**Watch.** The P4 `combat formation move` (generic disperse 4.0, shared code) runs unscreened off-slot:
walk-ins into live bombs and mine hits (window 32: 48 walk-ins, 12 mine hits), and the one Z 803.11 leg
left in window 34 (uld4's healer Ulduad, 7:35, 11 s off the floor up to Z 399.5); the flee fan's floor
check does not cover it. Siren legs across a spray lane (`FleeFan` screens the destination only).
Detour waypoints off the room (round 15: a `detour +65` leg onto a ledge 90-100 yd west). A bot stuck
off the navmesh (every path search `nopath`) has no flee leg at all. `mimiron.phase` flickers (H3 0.3 s,
P2b 0.25 s), splitting reader spans. The main tank's generic taunt can still pull a bomb into the pile
(window 34: one Righteous Defense). Shared code, not raised: healer mana (priests overheal P3 at
55-87%), Reincarnation into the killing lane, Ice Block ending inside a Frost Bomb fuse. DC side: auras
carried over a logout, Boomer XP-500 trash west of the room.

## Round 22 (build 6464b824a, round 21 built)

**Data:** window 34 (`window34-mimiron.md`, `--head 6464b824a`, one pull per pool, no DC veto), the
first on the profession rosters. Traces `603_<pool>_mimiron_<epoch>`: uld1 `…548883`, uld4 `…548880`,
uld5 `…548883`, uld6 `…548888`, uld2 `…548878`, uld3 `…548869` (epochs 17955488xx); windows 30 and 32
(`17955393xx`, `17955443xx-44xx`) for the Bomb Bot, taunt and pad baselines. Scratch `mim22/`.

| Pool | Out | P4 at | Deaths before Berserk |
|---|---|---|---|
| uld1 A | **kill** 9:10 | 6:51 | 3: P3 Flames; P4 mines 2 |
| uld4 A | **kill** 9:13 | 6:41 | 3: P2 Heat Wave; P4 Hand Pulse, mine |
| uld5 B | **kill** 9:05 | 6:31 | 4: P4 Plasma Ball, Shock Blast, Hand Pulse, mine |
| uld2 B | wipe 10:12 | 7:09 | 7: P3 Plasma Ball, Assault Bot; P4 mine, Frost Bomb 4 (9:53) |
| uld6 C | **kill** 9:51 | 7:12 | 6: P3 Water Spray 2, Bomb Bot; P4 mines 2, Hand Pulse |
| uld3 C | wipe 10:23 | 7:43 | 9: P2 Rapid Burst and Heat Wave 6 (5 melee, 3:26-3:31); P4 Water Spray 2, mine |

**Stop rules:** A (uld1 3, uld4 3) and B (uld5 4) met the done rule with hard mode real (64563 at
7.1-7.2 s, Emergency Mode at 2:23-2:27); C did not (uld6 6). Not polished. Bomb Bots were picked in
rounds 9 and 20 (the question); this round builds the user's Q10 answer, which State directs.

**Verdict on round 21 (flee legs refuse a spot off the floor): held in A, B and C.** `--floor`: no flee
leg at Z 803 (window 32: 8 in six pulls); moves sent off the floor are knockback follow-ons at Z 368-391,
and one P4 `combat formation move` to Z 803.11 (uld4 Ulduad, 7:35, 11 s up to Z 399.5), the generic
disperse the fan never screened (Watch). Hand Pulse baseline per `--pulse`: two healers out at 8:00
(uld6), 8:27 (uld2), 8:32 (uld5), 9:16 (uld3), never in uld1 and uld4; 3 Hand Pulse deaths before
Berserk, the 30 after it in uld2 and uld3 are Berserk.

**Change: Q10 (a), the off-tank takes the phase 3 Bomb Bots.** Not the worst failure (Bomb Bot 1 death;
window 32: 2, window 30: 12); the user's answer, built as directed.

- Baseline, `--bombbots` and the scratch: 28 bombs in window 34, 13 blew (chased 13, uld6's Uldwak dead;
  collateral 10, none dead), 0-2 taunted a pull, none on purpose. The victim a bomb spawns with sat
  10-23 yd off the spawn in 25 of 28 (3.7-9.8 in the rest), a ranged or healer in 19; the spawn is the
  ACU's ground point, within 2 yd of the room centre in uld1, uld3 and uld5, up to 5-6 in uld4 and uld6,
  6.5-18 in uld2.
- Taunts on bombs in windows 30-34 switched the chase within 250 ms: Growl 7 of 7, Death Grip 3 of 4,
  Righteous Defense 1 of 1 (Feral Charge only on arrival). All four class taunts are 30 yd, 8 s and off
  the global cooldown (DBC 355, 6795, 56222, 62124), against one bomb every 15 s.
- Pads, from adds' first sight: Junk and Assault Bots and fire bots come from the arms at ±60° and 180°,
  so the gaps centre on ±120°. Bystanders within 7 yd of a blast 9.5 yd out at +120° and -120° averaged
  1.25 and 1.24 over 18 pulls, no side better.
- Code (Mimiron only, both sizes and modes, with or without a player; a player as that tank does the job
  himself): `IsMimironBombTank` (`UldEncounter_Mimiron.cpp`) is the first living assist tank while only
  the ACU is up and not riding. `DeriveMimironSpreadSlot` gives him `p3bomb`: the ACU's ground point plus
  `ULDUAR_MIMIRON_BOMB_TANK_DISTANCE` 12 at 120° off the P3 centreline, clamped to the room; the arc
  spread walks him there with its hazard screens. `mimiron bomb bot taunt` (ACTION_RAID + 5) casts the
  class taunt on the live bomb not on him within 30 yd that is nearest its victim, then attacks it.
  `GetMimironBombBotBlast` lets him stand in his own bomb's blast instead of kiting. `mimiron bomb tank
  hold` zeroes reach, charge, tank face, set behind, follow, disperse and the generic taunts; his target
  guard zeroes the pickers; `mimiron bomb tank hold action` drops a non-bomb target so no ranged threat
  pulls adds and their melee onto the spot.
- Cost: he no longer picks up P3 adds; they dealt him 10-30k of their 100-410k a phase.
- Reader `--bombbots`: the first roster taunt per bomb, by whom and how long after first sight; blasts on
  the bomb tank (whoever held `p3bomb`); his distance off the ACU through P3 and from each of his bombs'
  spawn; his target drops. Test `BombTankPull`. Findings doc: the Bomb Bot section, the P3 tank slots,
  `slot` and `--bombbots` in "What a trace answers".

**Next window should show** (`--bombbots`, `--slots`, `--p3`):

- `p3bomb` held by the off-tank, median ~12 yd off the ACU and within 2 yd of it most of P3; target drops
  a handful, not one a tick.
- Most bombs taunted by him within ~0.5 s of first sight and blowing on him; chased ranged and healers
  near 0 except spawn blasts; collateral at or under 10 hits a window with nobody dead; the bomb tank
  never dead to a blast.
- No longer P3 (117-171 s) and no rise in Assault and Junk Bot damage on ranged and healers or in their
  kills. If melee stand in his blasts, the spot or its distance needs revisiting.

**Not picked.** P4 mines 8 (mixed movers; the disperse in Watch). The P2 storm 7: uld3's melee sector
lost 5 bots to Rapid Burst and Heat Wave 15-20 s after barrage 1 (`--spin`: raid health 94 to 72 in the
15 s after, `--wedge`: keep 33, 6 melee dead), a candidate for the next round. Water Spray 4 and the late
Frost Bomb 4, single pulls each.

**Questions for the user.** None new; Q11 stands as asked in round 21.

**Shared code:** none (Mimiron encounter, findings doc, reader and its test, this log).
