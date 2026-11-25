# Mimiron polish log (hard mode, Firefighter, 25 and 10)

Rounds 1-22: [history/mimiron.md](history/mimiron.md).

## State

**Reader and scenario.** `tools/botobs/bosses/mimiron.py <trace>` (`--help` lists the sections; scratch
scripts live in the lane scratchpad, never carried forward). Scenario `uld-mimiron`: the tank presses the
Big Red Button as the pull's start. Hard mode is real when Summon Flames (Initial) 64563 casts at ~7.1 s,
Flames 64566 ticks from the opening and VX-001 casts Emergency Mode 64582 at phase 2; a kill counts only
then. Berserk 64238 at 10:00 is the clock. 10n hard mode runs from window 46 (uld1-3 at 10n, uld4-6 at
25n there).

**Spawn.** Rounds 1-22 ran on a hand-placed 25-man Mimiron (guid 8000473, (2748.17, 2577.3)); window 46
on the stock one (2742.53, 2560.99). It changes nothing: at the pull he already rides the MK II (first
snapshot (2742.3, 2571.8, 368.0) in both), the core summons VX-001 and the ACU at fixed points
(`boss_mimiron.cpp` 532, 284), no bot code reads his position, his home only sets the 80 yd leash and his
seat on the MK II the 150 yd Flames pick. Older windows compare directly.

**Rosters.** From window 34 every pool has all eleven professions at 450 with their perks and profession
enchants: compare deaths with window 33 and older only by mechanic and target, never by total. Tanks:
A blood DK off-tank, B and C a bear.

**Fight model.** Z1 13 s; P1 MK II 69-94 s; Z2+H1 48 s (the raid holds a band 40 yd out, walks in 12.75 s
before P2); P2 VX-001 68-100 s; H2 24 s; P3 ACU 115-171 s at 25, set by Magnetic Core landings ~54 s apart
(17-47 points each; airborne it loses ~0.02-0.2%/s); H3 32 s (fire bots culled); P4 the three parts at
50%, 100-160 s. A clean 25n pull reaches P4 by 6:30-7:15; every body lost before P4 costs the clock. P4 is
a mana race: once two of four healers are out Hand Pulse kills (`--pulse`). At 10n the ACU's add timers
are the 25n ones with half the raid, so P3 is the DPS check: window 46 wiped there at 5:46, 5:47 and 8:36
with the ACU down 29, 29 and 88 points. What loses bodies: the P2 storm after a barrage (Heat Wave,
Rapid Burst, the Frost Bomb), P3 Bomb Bots and adds at 10, Water Spray, P4 mines and Hand Pulse.

**Compositions on `37cc72a94` (window 46).**

- 25n A (uld4): **met**, kill 9:10 with 1 (P3 Water Spray), hard mode real.
- 25n B (uld5): not met, kill 9:19 with 10 (P2 Heat Wave 2; P4 Hand Pulse 7, mine).
- 25n C (uld6): not met, kill 9:45 with 6 (P2 Rapid Burst 5 in a Frost Bomb fuse; P3 Water Spray).
- 10n A (uld1), B (uld2), C (uld3): wipes in P3, 11, 11 and 12 dead, hard mode real.
- Window 36 (round 22's build, 25n, all pools) met at 25: uld2 B 4, uld3 C 3, uld4 A 2, uld5 B 2, uld6 C 3;
  uld1 A wiped at Berserk.

**Open failures, ranked** (window 46, 51 deaths):

1. 10n P3 adds: Junk Bot 11, Assault Bot 5, Plasma Ball 3, after bodies lost earlier; nothing tanks Junk
   or Assault Bots (main tank pinned, off-tank on bombs).
2. P2 storm 11 (25n 7, 10n 4): uld6's 12 melee and tanks clumped 28-37 yd off VX-001 in the second Frost
   Bomb's fuse, every Rapid Burst on one hit all.
3. Bomb Bot 7 (10n): the bear bomb tank never taunted (round 23).
4. Hand Pulse 7 (uld5 only). Q11 dropped (user, 2026-11-25).
5. Water Spray 5; mines 2.

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
- r22: the off-tank takes every P3 Bomb Bot (Q10 a), `p3bomb` slot, class taunt, holds the spot; held in
  A (DK, 9 of 11 taunted at 25n, no Bomb Bot death); never ran for a bear (B, C).
- r23: a bear bomb tank shifts to Dire Bear Form in P3 and the hold zeroes `caster form`; pending.

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
- Q3: no 15% fire bot sweep. Q4: fire bots kept through P3, culled in H3. Q5: the off-tank tanks the ACU
  in P4.
- Q6 (a) with Q9 (a): once the beams stop, the P2 wedge takes the fewest-fire 120° arc within 90° of the
  re-aim.
- Q7 (a): P4 healers on the wedge's front row; Q8: that row stays at 18 yd.
- Q10 (a) (2026-11-24): the off-tank stands 10-12 yd off the ACU's ground spot, taunts every phase 3 Bomb
  Bot and takes the blast; built in round 22, made to work for a bear in round 23.
- Q11 (2026-11-24 yes, dropped 2026-11-25): the phase 4 healer mana change is not built.

**Open questions.** None.

**Watch.** The 10n P3 add pile: if 10n still wipes in P3 once bombs are taunted, ask whether a tank picks
up Junk Bots there. The P2 Frost Bomb fuse: melee and tanks flee to one clump, a burst on one hits all.
The P4 `combat formation move` (generic disperse 4.0, shared code) runs unscreened off-slot: walk-ins
into live bombs, mine hits, one Z 803.11 leg in window 34. Siren legs across a spray lane. Detour
waypoints off the room. A bot stuck off the navmesh has no flee leg at all. `mimiron.phase` flickers
(H3 0.3 s, P2b 0.25 s), splitting reader spans. The main tank's generic taunt can still pull a bomb into
the pile. Shared code, not raised: healer mana (priests overheal P3), Reincarnation into the killing
lane, Ice Block ending inside a Frost Bomb fuse. DC side: auras carried over a logout, Boomer XP-500
trash west of the room.

## Round 23 (build 37cc72a94, round 22 built, stock spawn)

**Data:** window 46, the first on the stock spawn and the first 10n hard mode. `window46-mimiron-25n-job5.md`
and `window46-mimiron-10n-job4.md`, one pull per pool; window 36 (`--head 42bff9a03 --diff 25n`, epochs
17955547xx) for round 22's verdict and the spawn comparison. Traces `603_<inst>_mimiron_<epoch>`: 25n uld4
`23_1795604540`, uld5 `21_1795604468`, uld6 `24_1795604742`; 10n uld1 `19_1795604358`, uld2
`20_1795604391`, uld3 `22_1795604502`. Scratch `mim23/`. Hard mode real in all six (64563 at 6.96-7.19 s,
64582 at 2:10-2:21).

| Pool | Out | Deaths |
|---|---|---|
| 25n uld4 A | **kill** 9:10 | 1: P3 Water Spray |
| 25n uld5 B | **kill** 9:19 | 10: P2 Heat Wave 2 (3:18); P4 Hand Pulse 7 (7:45, 8:47-8:55), mine |
| 25n uld6 C | **kill** 9:45 | 6: P2 Rapid Burst 5 (3:23-3:28); P3 Water Spray |
| 10n uld1 A | wipe 5:46 | 11: P2 Heat Wave, Rapid Burst 2; P3 Water Spray 2, Junk Bot 2, Plasma Ball 2, Bomb Bot, Assault Bot |
| 10n uld2 B | wipe 5:47 | 11: P1 mine (healer); P2 Heat Wave; P3 Junk Bot 6, Bomb Bot, Assault Bot, Plasma Ball |
| 10n uld3 C | wipe 8:36 | 12: P3 (287 s) Bomb Bot 5, Assault Bot 3, Junk Bot 3, Water Spray |

**Stop rules:** 25n A met, B and C not; 10n none. Bomb Bots were picked in rounds 9, 20 (Q10) and 22; not
stuck, since round 22's design held where it ran (A) and never ran in B and C (below), and Q10 directs it.

**Spawn:** it does not explain the 25n difference (State, Spawn). uld5's 10 are the P4 mana race (two
healers out by ~8:45, Hand Pulse 7) and uld6's 6 the P2 storm uld3 also had in window 34; window 36's own
uld1 A wiped at Berserk with 30.

**Verdict on round 22 (off-tank takes the P3 Bomb Bots): held in A, never ran in B and C.** `--bombbots`:
A's DK taunted 2/4, 4/4 (window 36), 3/3 (46) at 25n and 2/4 at 10n, every blast on him, no 25n Bomb
Bot death in A. The bear bomb tank in B and C taunted 0 bombs in all eight pulls of windows 36 and 46
at both sizes (taunt action FAILED 4-9 a pull); Bomb Bot deaths there 5 in window 36, 6 at 10n.

**Failure picked: the bear bomb tank never in bear form (bug).** Not the worst by count (the 10n P3 adds
16, the P2 storm 11, below), but its root in C and part of B, and independent and small.

- Every bear dropped Dire Bear Form (aura 9634 removed) 22-24 s before P3, when VX-001's death fired `drop
  target` (`ChooseTargetActions.cpp:78`, non-combat engine). There his party heals take `caster form` as
  prerequisite (uld5: `drop target` 228.8 s, `caster form` 229.9 s, `rejuvenation on party`), and the bomb
  tank target guard zeroes every picker, so nothing selects a target or re-enters combat: 14-37 party
  heals from P3 on, 0% of P3 in bear form in all eight pulls.
- `ClassTauntReady` checks only the cooldown and `CanCastSpell` passes `SPELL_FAILED_NOT_SHAPESHIFT`
  (`PlayerbotAI.cpp:3616`), so `mimiron bomb bot taunt` fired on each bomb and Growl failed. The DK's
  Dark Command needs no form, which is why A worked.
- 10n uld3 C lost 2 ranged to bombs at 4:26 and 5:05, then P3 ran 287 s with the adds piling (Assault
  Bot alive up to 193 s, 3 landings for 78 points); 25n window 36 uld3 and uld6 lost 2 each.

**Fix** (Mimiron only, 10 and 25, both modes; a real player as bomb tank does his own form):
`MimironBombTankFormTrigger` / `Action` (`ACTION_RAID` + 5.5, above the taunt) cast Dire Bear Form, else
Bear Form, on a druid bomb tank out of them; the `mimiron bomb tank hold` block also zeroes `caster form`,
so the party heals' prerequisite never shifts him back (the heals then fail in form, harmlessly). Reader:
`--bombbots` prints the bomb tank's taunt action verdicts and a druid's share of P3 in a bear form (window
46: uld5 FAILED 4, 0%; uld3 FAILED 9, 0%; uld4 FAILED 1, OK 3). Test `BombTankPull`. Findings doc: the Bomb
Bot section and "What a trace answers".

**Next window should show** (`--bombbots`, `--p3`):

- B and C: the bear in a bear form ~100% of P3, taunt action mostly OK, most bombs taunted within ~0.5 s
  of first sight and blowing on him, Bomb Bot deaths near 0 at both sizes, the bomb tank never dead to a
  blast.
- 10n C keeps its ranged through P3's first minute and P3 shortens; if 10n still wipes in P3 with bombs
  taunted, the add pile is next (Watch).

**Not picked.**

- 10n P3 adds 16: downstream. A and B entered P3 3 and 2 down, a healer each (uld1 3:14, uld2 1:05), C lost
  two ranged to bombs; the main tank stays pinned and the off-tank on bombs, so Junk Bots did 241k into
  uld3's healers.
- P2 storm 11: mixed. uld6: the second Frost Bomb landed 17.9 yd off VX-001 (3:12.6, fuse to 3:24.5); all
  10 melee and 2 tanks fled into one 24° arc 28-37 yd south, `mimiron frost bomb guard` held their reach,
  and bursts carried by the clump hit all 12 a tick while Heat Wave landed (raid 91 to 62%). uld5's 2 melee
  died to Heat Wave just after that bomb's blast; 10n uld1 had no bomb then. A spread melee escape would be
  a positions question; one pull of evidence, not raised yet.
- Hand Pulse 7 (uld5, Q11 dropped); Water Spray 5; mines 2.

**Questions for the user.** None.

**Shared code:** none (Mimiron definition, trigger and action, reader and its test, findings doc, this log).
