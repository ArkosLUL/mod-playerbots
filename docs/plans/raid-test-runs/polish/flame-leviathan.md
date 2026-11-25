# Polish log: Flame Leviathan (25-man, four towers)

Reader `tools/botobs/bosses/flame_leviathan.py` (`--summary` is `--mechanics`; also `--riders`,
`--hulls`, `--pyrite`, `--vents`, `--ram`, `--fury`, `--inferno`, `--adds`, `--corners`), tests in
`tools/botobs/tests/test_botobs.py` (`FlameLeviathanReader`). Design doc
`docs/raids/ulduar/flame-leviathan.md`. Older rounds: `history/flame-leviathan.md` (none yet).

## State

**Setup.** Trace slug `flame-leviathan`, raid `uld`, DC row `uld-leviathan`: the raid boards beside
the vehicle line, DC stages the fleet inside the arena and pulls. Hard mode is all four towers; the
trace shows it as his `mhp` 268.9M (3.84x). 25-man: 5 each of siege engine, demolisher, chopper.

**Fight model.** Everything happens from vehicles; a crewed bot is immune to his AoE, so deaths
are bots on foot after their hull died. Hulls die to Missile Barrage (34-42% of hull loss, no
answer), Flame Vents when the reserve engine misses (20-41%), Battering Ram on the Pursued hull and
the Inferno trail. Boss damage is Blue Pyrite from the demolishers (energy from crates the
demolisher gunners shoot down and grab) plus Systems Shutdown: kill his four turrets from his seats
and he is stunned 20 s at +50% damage taken. Gathering Speed makes him faster all fight, so a long
fight loses the fleet; in window48 the fleet was gone by 6-9 min in every wipe.

**Compositions on d9964f052 (window48, 6 pulls, lane AF's catapult build).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | wipes 9:48 (29 deaths), 10:19 (31) |
| B | uld2, uld5 | kill 7:18 (12 deaths), wipe 8:59 (28) |
| C | uld3, uld6 | wipes 11:32 (30), 19:00 (26) |

The build before (11-25 11:20, still with the crate repeat cheat) killed 4 of 6, median 6.5 deaths
and 5:25; one build is not a regression call.

**Open failures, ranked.**

1. The fight runs long, hulls run out, bots on foot die: Lash 43, Battering Ram 31, Inferno 23,
   Hodir's Fury 22, Flame Vents 21, Missile Barrage 16 (all on foot). Root cause picked r1: the
   stun cycle stalls (riders past 45° of a turret deal ~370/s), 2-4 shutdowns in 7-19 min.
2. Demolishers dry 22-95% of their time (crates reach few of them; riders' demolishers have no
   gunner to grab). r1's rider cap is the user's lever here.
3. Throw misses: 12/22 to 18/21 landed; from behind him 27 of 51 missed.

**Fixes in place.**

- r1: rider cap 2/1, riders ranged DPS but never hunters, `set facing`/Blink/Frost Nova/Disengage/
  charges blocked on his seats, rider targets the turret nearest its heading within ±90°,
  stranded/stalled riders leave, a loaded gunner unloads to its gun at shutdown. Verdict: next
  window.

**Ideas rejected.** None yet.

**User answers in force (2026-11-25).** Riders kill all of his turrets, not only their own seat's
pair; riders are ranged DPS, hunters excluded if the turrets sit inside a hunter's minimum range
(they do, r1); 2 riders at 25 and 1 at 10, the other demolisher gunners stay and shoot down the
Pyrite carriers; the throw stays capped at 70 yd; a gunner still in the catapult when the stun
starts gets back on its gun.

**Open questions.** Q1 (r1, not yet asked, waits on window49): aim throws from behind him so riders
land on rear seats? A rear rider reaches all four turrets, a front one only the two fronts, but in
window48 throws from behind missed 27 of 51 against 14 of 96 from his front half. Recommendation:
not yet; r1's stranded jump handles a front landing, see what it costs first.

## Round 1 (build d9964f052, window48)

**Data.** `window48-flame-leviathan.md`; traces `603_15_…1795608370` (uld1 A), `603_18_…1795608381`
(uld4 A), `603_16_…1795608360` (uld2 B, kill), `603_17_…1795608370` (uld5 B),
`603_14_…1795608330` (uld3 C), `603_13_…1795608288` (uld6 C). Sections `--summary`, `--hulls`,
`--vents`, `--pyrite`, new `--riders`; scratch reads of turret health, rider snapshot columns
(target, dealt) and act verdicts; core `boss_flame_leviathan.cpp`, VehicleSeat.dbc, Spell.dbc.

**Deaths per pull.** All on foot after the hull died (`--hulls`: 0 aboard). uld1 29: Lash 6,
Vents 6, Ram 5 from 7:05; uld4 31: Lash 10, Vents 8 from 6:27; uld5 28: Ram 13, Lash 11 (first at 2:51, the crew
of a siege engine the Inferno killed at 2:48); uld3 30: Lash 9, Inferno 7; uld6 26: Fury 7, Ram 7,
Lash 7, mostly 7:15-8:46; uld2 kill 12 from 6:09 (Inferno 5, Barrage 4).

**Failure picked: riders stall on turrets they cannot face.**

- Damage per rider by its target turret's bearing against his heading (`--riders`, all six
  pulls): ≤45° 2.9-4.1k/s, 69-79° 113-726/s, behind 62-718/s. Bearings are fixed: seats 0-3 are
  rear-left, rear-right, front-left, front-right (VehicleSeat 3043/3065/3071/3072); from a rear
  seat the turrets sit at 0/17/45/79°, from a front seat 0/69/116/148°.
- Mechanism: `set facing` (ACTION_MOVE + 7) fires past ±45° (`IsFacingValue`), cannot turn a
  passenger for long (his moves reset it) and outranks the rotation. uld2 Uldyap (warlock,
  rear-right) on turret 69309 at 79°: snapshot dealt flat at 204,985 from 3:28 to 5:36 while the
  act log shows only `set facing OK`; on 69311 at 45° it dealt ~5k/s at once. At 69-79° riders did
  1127/s while he stood still against 374/s while he moved.
- Effect: stays of 181 s (uld5, three riders, rear-right turret alive), 323 s (uld4), 338 s and
  285 s (uld3), 863 s (uld6, only 2 shutdowns in 19 min).
- Pairs: 25-man partner is seat ^ 2, so a pair is one side; a turret is `NOT_SELECTABLE` until a
  rider of its side boards. Uldyap alone killed its side's two turrets in 26 s, then could not touch
  the other side's 69309 (99.0% for 30 s) until Uldyaj landed there.
- Hunters: shot minimum range `max(1.5 + 2.0 + 4/3, 5)` = 5 yd (`Spell::CheckRange`,
  `GetMeleeRange`; turret model 28829 reach 2.0), own turret 1.2 yd and partner 2.6/4.8 yd from
  every seat; seated hunters cast Raptor Strike/Mongoose Bite/Wing Clip, 0.6-1.7k/s. 2-3 hunter
  stays per pull.
- A gnome mage Blinked off his seat 2-6 s after every landing (uld1 Uldxag 4 times, `blink OK`
  after Escape Artist and Frost Nova); Blink is a spell, not a movement the `Exclusive` rule vetoes.
- Riders at once: max 4 in every pull (the old rule loaded while seats needed riders).

**Classification.** Bug (riders do not do what the strategy intends) plus the user's answers.

**Fix.** `UldEncounter_FlameLeviathan.cpp`: `FlameLeviathanWantsCatapult` loads ranged DPS non-hunters
only, while riders + claims < cap (2/1) and not down to one turret with a rider aboard; seat plan
deals demolisher guns to non-hunter ranged first; `FlameLeviathanRiderTurret` picks the selectable
turret nearest the heading within ±90°; `FlameLeviathanRiderStranded`. `UldActions_FlameLeviathan.cpp`:
the seat action leaves when stranded or when its target lost no health for 15 s
(`fl.jump stranded/stalled`); `CatapultPassengerAction` unloads to its gun at shutdown or after
10 s (`fl.unload`). `UldDefinition_FlameLeviathan.cpp`: rule `flame leviathan seat hold` blocks
`set facing`, Frost Nova, Blink, Disengage and charges on his seats. Reader: `--riders` and a digest
line in `--summary`. No shared code.

**Next window should show** (`--summary` digest, `--riders`): riders at once ≤ 2, hunters seated 0,
no `ejected` stays from Blink, flank damage near the ahead rate (≥2k/s), no stay past ~90 s, more
shutdowns per minute (window48: one per 65-230 s), `fl.unload shutdown` where a loaded gunner met a
stun, fewer `dry` frames on the three gunned demolishers, and shorter fights with fewer on-foot
deaths. If flank stays low, the stall jump should show as `stalled` and a re-throw follow.

**Previous fix.** None (first round). Lane AF's catapult cycle works end to end: throws landed
12/22-18/21, 2-4 shutdowns a pull at 1.7-6.4x damage taken (most 2.1-3.0x).
