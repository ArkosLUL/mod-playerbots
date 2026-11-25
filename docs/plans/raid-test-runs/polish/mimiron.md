# Mimiron polish log (hard mode, Firefighter, 25 and 10)

Rounds 1-23: [history/mimiron.md](history/mimiron.md).

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
rogue, DK, enh shaman, ret paladin, mage, hunter (four melee, two ranged).

**Fight model.** Z1 13 s; P1 MK II 69-94 s; Z2+H1 48 s (the raid holds a band 40 yd out, walks in 12.75 s
before P2); P2 VX-001 68-100 s; H2 24 s; P3 ACU 115-171 s at 25, set by Magnetic Core landings ~54 s apart
(17-47 points each; airborne it loses ~0.02-0.2%/s); H3 32 s (fire bots culled); P4 the three parts at
50%, 100-160 s at 25. A clean 25n pull reaches P4 by 6:30-7:15; every body lost before P4 costs the clock.
In P4 the ACU hovers 15 yd above the chassis (z 379.3 against 364.3 in every snapshot), out of every
melee swing, so its pace is the ranged's and the pair waits on it (`--p4pace`). At 10n that makes P4 the
DPS check: both ranged alive, 0.33-0.37%/s and 134 s (window 49 uld4 kill); one left, 0.13-0.19%/s and
Berserk. At 25n P4 is a mana race: once two of four healers are out Hand Pulse kills (`--pulse`). What
loses bodies: the P2 storm after a barrage (Heat Wave, Rapid Burst, the Frost Bomb), P3 adds at 10,
Water Spray, P4 mines and Hand Pulse.

**Compositions.**

- `06854755f` (window 49, round 23 built): 25n only uld5 B, kill 9:15 with 11 (a DC setup check stopped
  the job; DC 0b2488a). 10n: A **met** (uld4 kill 9:08 with 1; uld1 P2 wipe 11); B no (uld2 P3 wipe 11,
  uld5 Berserk 10); C no (uld3 and uld6 Berserk, 11 each). Hard mode real in all seven.
- `37cc72a94` (windows 46 and filler 47, round 22 built), 25n: A **met** (uld4 kill 1, uld1 kill 6), C
  **met** (uld3 kill 0, uld6 kill 6), B not (uld5 kill 10, uld2 Berserk 30 with the ACU at 2.5%).

**Open failures, ranked** (window 49, 66 deaths):

1. 10n P4 out of clock: uld3, uld5, uld6 Berserk (23). Each lost a ranged DPS first (P1 Napalm Shell, P2
   Heat Wave, P2 Frost Bomb, then a P4 Water Spray). Q12.
2. P2 storm 14: uld1 9 after its two healers fell (P1 mine 0:25, Rapid Burst 2:50); one each in uld2,
   uld3, uld6; 25n uld5 2.
3. Hand Pulse 8 (25n uld5 6). Q11 dropped.
4. Water Spray 7: uld2's four melee at 5:14 from two sprays 0.1 s apart as the ACU lifted; uld6's two in
   P4 from fire bots left over from H3 (round 24).
5. 10n P3 adds 5 (uld2, after its Water Spray four); P1 4 (mine, Napalm Shell 2, Plasma Blast on a tank).

**Fixes in place** (round: what; verdict).

- r1: hazard read without a sight test, standing bots leave fire at any health in a handover; held.
- r2-3: handover hold band and walk-in, one code path with or without a player; held.
- r4: core fetch at any ACU health; held.
- r5: spray escape crosses to the far side, lanes 18 yd, no siren screen on it; held.
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
- r23: a bear bomb tank shifts to Dire Bear Form in P3 and the hold zeroes `caster form`; held (bear form
  100% of P3 in all five B and C pulls, Bomb Bot deaths 1).
- r24: fire bots alive when P4 starts stay cull targets for DPS (the user's Q4 covered H3 only); pending.

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

**Watch.** Melee and the off-tank sent to the unreachable P4 ACU walk between slot and stack (174 `arc
spread <-> reach melee` A-B-A in uld3's P4); if a core change seats it without hover, melee reach it and
the P4 model changes. The H3 cull loses to leftover Junk and Assault Bots (DPS take those first). The P2
Frost Bomb fuse: melee and tanks flee to one clump, a burst on one hits all. The P4 `combat formation move`
(generic disperse 4.0, shared code) runs unscreened off-slot: walk-ins into live bombs, mine hits (uld1's
P1 healer at 0:25). Siren legs across a spray lane. A bot stuck off the navmesh has no flee leg at all.
`mimiron.phase` flickers (H3 0.3 s, P2b 0.25 s), splitting reader spans. Shared code, not raised: healer
mana (priests overheal P3), Reincarnation into the killing lane, Ice Block ending inside a Frost Bomb
fuse. DC side: auras carried over a logout, Boomer XP-500 trash west of the room.

## Round 24 (build 06854755f, round 23 built, stock spawn)

**Data:** window 49 (`window49-mimiron-10n-job4.md`, six pulls; `window49-mimiron-25n-job3.md`, one) and
filler 47 on round 22's build (`filler47-mimiron-10n-job1.md`, `filler47-mimiron-25n-job2.md`). Traces
`603_<inst>_mimiron_<epoch>`: window 49 10n uld1 `18_1795611027`, uld2 `16_1795610997`, uld3
`17_1795611002`, uld4 `15_1795610986`, uld5 `21_1795611503`, uld6 `14_1795610924`, 25n uld5
`13_1795610900`; filler 47 10n uld4-6 `28_1795605526`, `29_1795605527`, `30_1795605512`, 25n uld1-3
`31_1795605531`, `32_1795605540`, `33_1795605546`. Hard mode real in all thirteen. Scratch `mim24/`.

| Pool | Out | Deaths |
|---|---|---|
| 10n uld1 A | wipe 4:34 | 11: P1 mine (healer, 0:25), Napalm Shell; P2 Rapid Burst 4, Heat Wave 5 |
| 10n uld2 B | wipe 7:20 | 11: P2 Rapid Burst; P3 Water Spray 5, Junk Bot 3, Assault Bot, ACU |
| 10n uld3 C | Berserk | 11: P2 Heat Wave (hunter); P4 Hand Pulse; Berserk 9 |
| 10n uld4 A | **kill** 9:08 | 1: P4 Rocket Strike |
| 10n uld5 B | Berserk | 10: P1 Plasma Blast (tank), Napalm Shell (mage); P4 Hand Pulse; Berserk 7 |
| 10n uld6 C | Berserk | 11: P2 Frost Bomb (hunter); P3 Bomb Bot; P4 Water Spray 2 (mage, shaman); Berserk 7 |
| 25n uld5 B | **kill** 9:15 | 11: P2 Rapid Burst 2; P4 Hand Pulse 6, Laser Barrage, mine |

**Stop rules:** not polished (25n has only B on this build; 10n B and C open). Not stuck: this round's
failure is new.

**Verdict on round 23 (bear bomb tank in bear form): held.** `--bombbots` on all five B and C pulls (10n
uld2, uld3, uld5, uld6, 25n uld5): the bear in a bear form 100% of P3, taunt action OK 4-5 and FAILED 0 a
pull (window 46: FAILED 4-9, 0%), 4-5 bombs taunted each (median 40-1206 ms after first sight), every
blast on him but one; Bomb Bot deaths 1 (uld6 collateral, 5:10) against 6 at 10n in window 46 and 10 in
filler 47.

**Not picked first: 10n P4 runs out the clock (plan question Q12).** `--p4pace`: the ACU sits 15.0 yd
above the chassis through every P4 (z 379.3/364.3, five pulls, both sizes); `IsWithinMeleeRange` is 3D
(`Unit.cpp:790`) and its reach 0 gives 5 yd. Melee aimed at it dealt their ranged spells only (bear
0.1-0.2k/s, rogue 0.4-1.0k/s; 174 `arc spread <-> reach melee` A-B-A in uld3). The ACU lost 0.33%/s with
both ranged alive (uld4, 134 s), 0.19% with one (uld3, ACU 8% at Berserk), 0.13% (uld5) and 0.11% (uld6,
none left from 7:23). The pair waits at the floor, so P4 lasts the ACU's HP over ranged damage.

**Failure picked: fire bots left by H3 are kept in P4 (bug).** Q4 culls them in H3 "so phase 4 starts
without them"; `ScanMimironFireBots` (`UldEncounter_Mimiron.cpp:2210`) folds live ones to the cull only
while `IsMimironPhase4Handover` reads a spent, unselectable ACU, so once P4 makes it selectable survivors
are kept again: excluded as targets, behind the splash guard.

- 2 of 11 H3s read opened with nine fire bots (10n uld6, filler 47 25n uld2). DPS spent the handover on
  leftover adds: uld6 melee on a Junk Bot then the Assault Bot 6:16-6:47, uld2 all 15 DPS on one Assault
  Bot 6:41-7:10; only the tanks hit fire bots.
- uld6: four into P4 at 36-56%, two alive at the end; Water Spray killed the mage at 7:23 (its last
  ranged DPS) and the shaman at 8:08, splash holds 41; the ACU then lost 0.11%/s, Berserk at 28%.
- 25n uld2: six into P4, one at 1-6% alive 7:50-10:00; Deafening Siren through P4, 184 raid-s silenced,
  splash holds 62; Berserk with the ACU at 2.5%, 30 dead.
- The other nine H3s ended with none (`--firebots`).

**Fix** (Mimiron only, hard mode, 10 and 25; a real player kills or ignores them as he likes): the fold
culls in P4 too (`IsMimironPhase4`), so a survivor stays a DPS priority target after leftover adds, off
the target exclusions and the splash guard. Healers run `mimiron set dps priority` in P4, so its fire bot
entry skips them as H3's trigger gate does, and they keep their part (`UldActions_Mimiron.cpp:1300`, review
fix; else a far fire bot would pull them off the row by `reach spell`). Nothing summons more in P4 (`SetData(1, 4)` resets the ACU's
events). Reader: `--p4pace` (parts every 30 s, loss rates, the ACU's height, aim and damage by role), test
`PacePull`. Findings doc: the ACU's height and what it means for melee, the off-tank and the pace; the
cull in P4.

**Next window should show** (`--firebots`, `--p4pace`): after an H3 that leaves fire bots, P4 closes
with none (gone = open within ~15 s), P4 Water Spray, siren and splash holds near 0, no healer aimed at
a fire bot; H3 and P3 unchanged.
With none left (most pulls) the fix is not exercised.

**Questions for the user.**

- **Q12 (plan change, roster): 10-man ranged damage on the P4 ACU.** The ACU hovers out of melee reach in
  P4, so only ranged attacks bring it down, and the other two parts must wait for it. 10-man brings two
  ranged DPS; with both alive P4 took 134 s and the kill came at 9:08, with one lost before or early in P4
  it ran past Berserk in three of six window 49 pulls. Options: (a) put a third ranged DPS among each
  design's first six (for example a warlock for the rogue), which changes every 10-man pull of every
  boss; (b) 10-man healers also hit the ACU in P4 while the raid is healthy (~1-2k/s, risks the Hand Pulse
  race); (c) leave it and waive the 10n stop rule for Mimiron. Recommendation: (a).

**Shared code:** none (Mimiron fire bot fold, reader and its test, findings doc, this log).
