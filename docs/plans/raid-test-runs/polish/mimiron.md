# Mimiron polish log (hard mode, Firefighter, 25 and 10)

Rounds 1-24: [history/mimiron.md](history/mimiron.md).

## State

**Reader and scenario.** `tools/botobs/bosses/mimiron.py <trace>` (`--help` lists the sections; scratch
scripts live in the lane scratchpad, never carried forward). Scenario `uld-mimiron`: the tank presses the
Big Red Button as the pull's start. Hard mode is real when Summon Flames (Initial) 64563 casts at ~7.1 s,
Flames 64566 ticks from the opening and VX-001 casts Emergency Mode 64582 at phase 2; a kill counts only
then. Berserk 64238 at 10:00 is the clock. 10n hard mode runs from window 46.

**Spawn.** Rounds 1-22 ran on a hand-placed 25-man Mimiron (guid 8000473, (2748.17, 2577.3)); window 46
on the stock one (2742.53, 2560.99). It changes nothing: at the pull he already rides the MK II, the core
summons VX-001 and the ACU at fixed points (`boss_mimiron.cpp` 532, 284), no bot code reads his position.
Older windows compare directly.

**Rosters.** From window 34 every pool has all eleven professions at 450 with their perks and profession
enchants: compare deaths with window 33 and older only by mechanic and target, never by total. Tanks:
A blood DK off-tank, B and C a bear. 10-man takes the first six DPS, the same classes in every design:
rogue, DK, enh shaman, ret paladin, mage, hunter (four melee, two ranged); a warlock joins Mimiron's
10-man group from the build after window 50 (Q12).

**Fight model.** Z1 13 s; P1 MK II 69-94 s; Z2+H1 48 s (the raid holds a band 40 yd out, walks in 12.75 s
before P2); P2 VX-001 68-100 s; H2 24 s; P3 ACU 115-171 s at 25, set by Magnetic Core landings ~54 s apart
(17-47 points each; airborne it loses ~0.02-0.2%/s); H3 32 s (fire bots culled); P4 the three parts at
50%, 100-160 s at 25. A clean 25n pull reaches P4 by 6:30-7:15; every body lost before P4 costs the clock.
In P4 the ACU hovers 15 yd above the chassis (z 379.3 against 364.3 in every snapshot), out of every
melee swing, so its pace is the ranged's and the pair waits on it (`--p4pace`). At 10n that makes P4 the
DPS check: both ranged alive, 0.33-0.37%/s and 134 s (window 49 uld4 kill); one left, 0.13-0.19%/s and
Berserk. At 25n P4 is a mana race: once two of four healers are out Hand Pulse kills (`--pulse`), so a
healer lost early in P4 is a wipe (window 50 uld1). What loses bodies: the P2 storm after a barrage
(Heat Wave, Rapid Burst, the Frost Bomb), P3 Water Spray where fire bots crowd the melee and adds at 10,
the opening P4 Frost Bomb, P4 mines and Hand Pulse.

**Compositions.**

- `0200b8827` (window 50, round 23's code), 25n: A no (uld1 wipe 32, uld4 kill 8), B **met** (uld2 kill
  3; uld5 kill 12), C **met** (uld6 kill 1, uld3 kill 2). Hard mode real in all six.
- `06854755f` (window 49, round 23 built): 10n A **met** (uld4 kill 9:08 with 1; uld1 P2 wipe 11); B no
  (uld2 P3 wipe 11, uld5 Berserk 10); C no (uld3 and uld6 Berserk, 11 each).
- `37cc72a94` (windows 46 and filler 47, round 22 built), 25n: A **met** (uld4 kill 1, uld1 kill 6), C
  **met**, B not.
- Round 24 (merged) and round 25 have no pulls yet.

**Open failures, ranked** (window 50, 25n, 59 deaths):

1. A lost a healer to the opening P4 Frost Bomb in both pulls. uld4: a `combat formation move` put the
   holy paladin at 31 yd 1.5 s before the blast and the escape's +112.5° leg cut back to ~29 yd (r25).
   uld1: a `detour -65` slot leg (waypoint (2791.7, 2557.4), 48 yd off centre) left the resto shaman at
   46 yd east, where every fan bearing was refused as `move` (bearing 0 `nopath`) and the fallback walked
   it out to 58 yd, 25.4 yd from the bomb. Then the 7:11 tank-spot drag (39 yd west) left the healers
   walking 40-58 yd off VX-001 under Hand Pulse: paladin dead 7:17, shaman 7:25, 18 Hand Pulse deaths,
   Self Repair with the ACU at 4.2%, wipe. A detour bound needs navprobe of the east edge (x 2785-2800,
   y 2535-2560) first (tried and rejected without it).
2. Water Spray 6 (uld4 4, uld2 2): three stood 3-4 s in a lane on `spray unsafe` (r25); one walked back by
   `mimiron approach target action` 40 ms after the 2.5 s hold; one had no escape at all (the reader's
   lanes had it in one for 2 s); one after a fire dodge.
3. Hand Pulse in the 25n mana race: uld5 B 8 after two of four healers ran dry at 8:43 (Q11 dropped).
4. Bomb Bot 4: uld5 3 (2 of 4 taunted; one blew at its first sample on 4 bots, one on the bear and 7
   more); uld4 1.
5. Rapid Burst 3 (uld4, uld5, uld6), mines 4 (uld1 2, uld3 2).
6. 10n P4 out of clock (window 49: uld3, uld5, uld6 Berserk); Q12's warlock lands next build.

**Fixes in place** (round: what; verdict).

- r1: hazard read without a sight test, standing bots leave fire at any health in a handover; held.
- r2-3: handover hold band and walk-in, one code path with or without a player; held.
- r4: core fetch at any ACU health; held.
- r5: spray escape crosses to the far side, lanes 18 yd, no siren screen on it; held, but crowded lanes
  beat both steps (r25).
- r6: melee walk back into sight after Shock Blast; held.
- r7: holder screen (60.1 yd) and unreachable step; held.
- r8: stranded walk-in least-burn leg; Rapid Burst read via the Burst Target; both held.
- r9: Bomb Bot step-out off the blast line, a few yards; held.
- r10: spray dodge never defers to the barrage; held.
- r11: short barrage arcs over P4 mines; held for ranged.
- r12: fire bot cull in H3 (Q4); ACU off-tank in P4 (Q5); cast cut in fire; least-burn flames fallback;
  all held.
- r13: bomb leg test for the formation and the main tank's anchor; charge guard; held.
- r14: Revive Pet hold all fight; held.
- r16: P2 settle off fire (Q6); P4 healers on the 18 yd front row (Q7); held.
- r17: mine trigger and step at 3 yd ground distance; held.
- r19: settle turns at most 90° (Q9); held.
- r20: a spray step starts the 2.5 s fire hold; held.
- r21: the flee fan refuses a destination off the floor (Z 803.11); held.
- r22: the off-tank takes every P3 Bomb Bot (Q10 a), `p3bomb` slot, class taunt, holds the spot; held.
- r23: a bear bomb tank shifts to Dire Bear Form in P3 and the hold zeroes `caster form`; held.
- r24: fire bots alive when P4 starts stay cull targets for DPS (Q13); pending, no pulls.
- r25: spray escape ladder (8 and 12 yd after the lane-edge step); Frost Bomb escape fan stops at 90°;
  pending.

**Tried and rejected.**

- The 15% fire bot sweep: it fell inside landings, and dead fire bots only let fire build (Q3).
- Fire bots kept into P4: 25 s of P4, spray deaths and silences for ~0.3M of healable fire (Q4).
- Uncapped fewest-fire settle: 131-140° turns walked ranged across VX-001 into the storm (Q9).
- A 20 yd P4 healer row: spares a quarter of Shock Blast flights, costs a tenth of heal reach (Q8).
- `MoveAwayFromCreature` Bomb Bot step (30 yd legs along the chase); a victim-centred blast disc (left the
  bomb's side open); the siren screen on spray escapes (blocked them).
- A main-tank `Kill()` on adds under cheats: removed.
- A room-radius bound on detour waypoints: the floor is no circle round the room centre, so it needs
  navprobe.

**User answers in force.**

- One code path: a strategy runs the same with or without a real player; a player only changes who does
  a job he does himself.
- Q1: every Firefighter raid holds a band ~40 yd out through the 1→2 handover and walks in over its last
  ~13 s.
- Q3: no 15% fire bot sweep. Q4: fire bots kept through P3, culled in H3 so P4 starts without them. Q5:
  the off-tank tanks the ACU in P4.
- Q6 (a) with Q9 (a): once the beams stop, the P2 wedge takes the fewest-fire 120° arc within 90° of the
  re-aim.
- Q7 (a): P4 healers on the wedge's front row; Q8: that row stays at 18 yd.
- Q10 (a) (2026-11-24): the off-tank stands 10-12 yd off the ACU's ground spot, taunts every phase 3 Bomb
  Bot and takes the blast.
- Q11 (2026-11-24 yes, dropped 2026-11-25): the phase 4 healer mana change is not built.
- Q12 (2026-11-25): only Mimiron's 10-man group gets a third ranged DPS, a warlock (DC row `uld-mimiron`,
  DC `d8859c6`; the 10n groups had an arcane mage and a hunter); every other boss's 10-man group unchanged.
- Q13 (2026-11-25): fire bots still alive when P4 starts are culled early in P4 (r24), extending Q4.

**Open questions.** None.

**Watch.** `combat formation move` (generic disperse 4.0, shared code) runs unscreened off-slot in P4:
20-25 walk-ins a bomb in uld1, uld2 and uld6, one that started uld4's healer death; adding it to
`mimiron frost bomb guard` is the next step if bomb hits on escapers remain. The P4 tank-spot drag moves the raid
25-39 yd every ~30 s under Hand Pulse. Melee and the off-tank sent to the unreachable P4 ACU walk between
slot and stack; if a core change seats it without hover, the P4 model changes. The H3 cull loses to
leftover Junk and Assault Bots. The P2 Frost Bomb fuse: melee and tanks flee to one clump. The barrage
dodge walks onto a live P4 bomb when no ring step is clean (9 of 44 bomb hits, all within 10 s of
Berserk). Siren legs across a spray lane. A bot off the navmesh has no flee leg at all.
`mimiron.phase` flickers (H3 0.3 s, P2b 0.25 s), splitting reader spans. Shared code, not raised: healer
mana (priests overheal P3), Reincarnation into the killing lane, Ice Block ending inside a Frost Bomb
fuse. DC side: auras carried over a logout, Boomer XP-500 trash west of the room.

## Round 25 (build 0200b8827, round 23's code, stock spawn)

**Data:** window 50 (`window50-mimiron.md`, 25n, one pull per pool). Traces `603_<inst>_mimiron_<epoch>`:
uld1 `25_1795615039`, uld2 `26_1795615062`, uld3 `27_1795615070`, uld4 `28_1795615233`, uld5
`29_1795615273`, uld6 `30_1795615427`. Hard mode real in all six (64563 at 7.08-7.14 s, 64582 cast).
Scratch `mim25/` (bomb hits over 99 traces, spray stats and a ladder replay over the last 40).

| Pool | Out | Deaths |
|---|---|---|
| uld1 A | wipe 10:24 | 32: P4 Frost Bomb (healer 6:41), Hand Pulse 18 from 7:17, mines 2, then Self Repair: Plasma Ball 5, MK II 5, Flames 2 |
| uld2 B | **kill** 9:35 | 3: P3 Water Spray 2 (6:16); P4 Flames |
| uld3 C | **kill** 9:09 | 2: mines (P1, P4) |
| uld4 A | **kill** 9:34 | 8: P2 Rapid Burst; P3 Water Spray 4, Bomb Bot; P4 Frost Bomb (healer 7:15), Hand Pulse |
| uld5 B | **kill** 9:28 | 12: P2 Rapid Burst; P3 Bomb Bot 3; P4 Hand Pulse 8 (after 8:43) |
| uld6 C | **kill** 8:44 | 1: P2 Rapid Burst |

**Stop rules:** not polished (A open on this build; 10n B and C open on window 49's). Not stuck: `spray
unsafe` was picked once before (r5), the Frost Bomb escape bearing never.

**Verdict on round 24:** none, its build has no pulls. Round 23 (bear bomb tank) held again: every B and C
bomb tank in a bear form, uld2 and uld6 5 of 5 and 4 of 4 taunted on him; uld5 taunted 2 of 4.

**Failure picked: Water Spray escapes refused on every step (bug).** `MimironFireBotAction`
(`UldActions_Mimiron.cpp:1171`) tried the lane edge (5 yd less the bot's offset) and the far side (5
plus it); with six fire bots round the melee (12 lanes) both land in other lanes, so the fan logged
`spray none` and `spray unsafe` every tick and the bot stood in its line until the spray came. uld2
6:12.7-6:16.7: Uldyar, Uldyag and the tank Uldyaa 4 s in a lane, two dead; uld4 6:13.9-6:17.2 Ulduax 3 s,
dead. Last 40 traces: 142 hits, 30 dead, 10 of them within 3.5 s of a `spray unsafe` (37 hits). Replay of
the 170 stuck moments (lanes rebuilt as the C++ does): 25 not in a lane by the model, 40 cleared at the
edge step (the model's lanes differ), 80 at 8 yd, 25 at 12, none unresolved; out of every lane in a
median 0.8 s, at most 1.6.

**Second, independent and small: the Frost Bomb escape's 112.5° bearings (bug).** The fan takes the hop
onto 34 yd at any bearing to ±112.5°; past 90° the leg passes closer to the bomb than it starts, at
112.5° from 31 yd down to 28.6 over 30 yd. Over 60 traces the ±112.5° legs were 158 of 5,053 escapes and
the last leg before 11 of 20 blast hits on an escaping bot (9 dead); ±22.5-45° 1,020 legs, none. uld4
7:13.7: `combat formation move` took Ulduab 32.7→31 yd, the escape went `ok +113` (bearings to 90°
refused for mines and fire), hit at 29.2 yd.

**Fix** (Mimiron only, hard mode, 10 and 25, with or without a player):

- After the edge step both sides repeat at 8 and 12 yd (`ULDUAR_MIMIRON_FIREBOT_SPRAY_ESCAPE_LADDER`,
  notes `spray+8`, `spray+12`); only the last rung's far side takes the unscreened fallback.
- The Frost Bomb escape fan refuses a bearing past 90° (`ULDUAR_MIMIRON_FROST_BOMB_FAN_MAX`, counted as
  `bomb`); with all to 90° refused, the unit flee's fallback walks straight away.
- Reader: `--firebots` adds the `spray unsafe` notes, the Water Spray hits and deaths within 3 s of one,
  and ladder legs by rung, and counts a ladder leg as a spray step for walk-backs; `--bomb` adds escape
  legs past 90° and the blast hits whose victim's last escape was one. Tests for both. Findings doc:
  both fixes and the reader lines.

**Next window should show** (`--firebots`, `--bomb`): P3 `spray escapes refused on every rung` near 0
(window 50: uld2 133, uld4 20) with `ladder legs +8`/`+12` in their place, Water Spray deaths at 1 or
fewer a pull; `escape legs past 90 deg` 0 and no blast hit after an escape. Round 24's check stands: after
an H3 that leaves fire bots, P4 closes with none.

**Not picked.**

- uld1's stranded healer: needs navprobe of the east edge before a detour bound (State, failure 1).
- The `combat formation move` walk-in into the P4 bomb (Watch); the bearing fix covers uld4's death.
- uld5's Bomb Bots: 2 of 4 taunted, one on the pile at its first sample.
- The P4 tank-spot drag under Hand Pulse (uld1 7:11, 39 yd); Hand Pulse in the mana race (Q11 dropped).

**Questions for the user.** None.

**Shared code:** none (Mimiron action and constants, reader and its test, findings doc, this log).
