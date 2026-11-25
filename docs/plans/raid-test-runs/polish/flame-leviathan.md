# Polish log: Flame Leviathan (25-man, four towers)

Reader `tools/botobs/bosses/flame_leviathan.py` (`--summary` is `--mechanics`; also `--riders`,
`--hulls`, `--pyrite`, `--vents`, `--ram`, `--fury`, `--inferno`, `--adds`, `--corners`), tests in
`tools/botobs/tests/test_botobs.py` (`FlameLeviathanReader`). Design doc
`docs/raids/ulduar/flame-leviathan.md`. Older rounds: `history/flame-leviathan.md`.

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
fight loses the fleet; in window48 the fleet was gone by 6-9 min in every wipe. A rider can turn
(seat 3044 `ALLOW_TURNING`) and reaches every activated turret from any seat; a turret activates
when a rider boards its side (25-man pair seat ^ 2 is one side).

**Compositions on d9964f052 (window48, 6 pulls, lane AF's catapult build).** No pulls on r1 or r2
code yet.

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | wipes 9:48 (29 deaths), 10:19 (31) |
| B | uld2, uld5 | kill 7:18 (12 deaths), wipe 8:59 (28) |
| C | uld3, uld6 | wipes 11:32 (30), 19:00 (26) |

The build before (11-25 11:20, still with the crate repeat cheat) killed 4 of 6, median 6.5 deaths
and 5:25; one build is not a regression call.

**Open failures, ranked.**

1. The fight runs long, hulls run out, bots on foot die: Lash 43, Battering Ram 31, Inferno 23,
   Hodir's Fury 22, Flame Vents 21, Missile Barrage 16 (all on foot). Root cause (r1, corrected
   r2): the stun cycle stalls because a bot rider's turn snapped back to his heading on every move
   of his, so riders dealt ~340-370/s at turrets past 45°; 2-4 shutdowns in 7-19 min.
2. Demolishers dry 22-95% of their time (crates reach few of them; riders' demolishers have no
   gunner to grab). r1's rider cap is the user's lever here.
3. Throw misses: 12/22 to 18/21 landed; from behind him 27 of 51 missed.

**Fixes in place.**

- r1: rider cap 2/1, riders ranged DPS but never hunters, Blink/Frost Nova/Disengage/charges
  blocked on his seats, a loaded gunner unloads to its gun at shutdown. Verdict: next window.
- r2: a bot's turn on a turning seat goes into its seat offset (shared `ServerFacade::SetFacingTo`);
  riders attack the nearest activated turret, `set facing` unblocked, r1's heading pick and
  stalled/stranded jumps removed; a rider leaves stranded only when its side is done, the other
  side unactivated and the cap full (lowest-guid bot). Verdict: next window.

**Ideas rejected.**

- r1's heading limits (turret within ±90° of his heading, a front rider held to the front pair, the
  15 s stall jump, the facing-based stranded jump), r2: riders can turn; the limit was a bot gap.
- Q1 (aim throws from behind him so riders land on rear seats), r2: with turning every seat reaches
  every activated turret, so only the landing side matters.

**User answers in force (2026-11-25).** Riders kill all of his turrets, not only their own seat's
pair; riders are ranged DPS, hunters excluded if the turrets sit inside a hunter's minimum range
(they do, r1); 2 riders at 25 and 1 at 10, the other demolisher gunners stay and shoot down the
Pyrite carriers; the throw stays capped at 70 yd; a gunner still in the catapult when the stun
starts gets back on its gun. Riders can turn: a Death Knight thrown onto a front seat faced the rear
turrets and killed all four alone.

**Open questions.** None.

## Round 2 (build 0200b8827, no new pulls)

**Data.** No pulls on r1's code yet; the prompt names `window48-flame-leviathan.md` again, read in
r1 (deaths per pull in history r1). This round acts on the user's correction of r1. Traces
`603_13_…1795608288` (uld6 C) and `603_16_…1795608360` (uld2 B) through `--riders`; core
`Vehicle.cpp`, `MovementHandler.cpp`, `Spell.cpp`, `boss_flame_leviathan.cpp`; Vehicle.dbc and
VehicleSeat.dbc rows 335-338, 340-342, 345, 380, 385.

**Failure picked: riders cannot keep a turn (a bot gap, not a game rule).**

- The game lets a rider turn: vehicle 341 (his seat 33114) seat 0 is VehicleSeat 3044, flags
  0xC201642B with `VEHICLE_SEAT_FLAG_ALLOW_TURNING` 0x400 and `CAN_ATTACK`, no `CAN_CONTROL`.
- A player's turn: the client sends a movement packet with `MOVEMENTFLAG_ONTRANSPORT` and its
  seat-relative facing in `transport.pos`; `WorldSession::HandleMoverRelocation`
  (`MovementHandler.cpp` ~433) does `mover->m_movementInfo = movementInfo`, so the offset keeps it.
- A bot's turn: `ServerFacade::SetFacingTo` only did `SetOrientation` (and sends no heartbeat while
  rooted, which a passenger is). `Vehicle::RelocatePassengers` (`Vehicle.cpp` 560) rebuilds every
  passenger's position and facing from `m_movementInfo.transport.pos` plus the base's orientation on
  each move of his (Leviathan → seat creature → rider), so the turn snapped back to his heading.
- Effect: `Spell::_cast` re-runs `CheckCast(false)` and `CheckRange` checks facing (±90°) for every
  cast-time spell (`Spell.cpp` 7411), so a cast at a turret past 90° failed at completion; r1's
  `set facing` loop came from the same snap.
- Trace (new `--riders` line, samples whose target sits past 90° of his heading): uld6 riders
  faced it 10 of 3,259 while he moved, 1,005 of 1,103 while he stood; uld2 25 of 66 moving.
- Turret activation is unchanged: a rider activates its seat's turret and its partner's
  (`ActivateTurrets`), Start the Engine reinstalls them, so a side needs a rider each cycle.

**Classification.** Bug, plus the user's answer that riders can turn.

**Fix.** Shared `src/Util/ServerFacade.cpp`: `SetFacingTo` also writes the seat-relative facing
(`KeepSeatFacing`) on an `ALLOW_TURNING` seat the bot does not control, as a client's packet does.
A rider's facing calls go through it (`set facing`, `PlayerbotAI::CastSpell` before and during a
cast, `AttackAction`). Affected seats where bots sit in Ulduar and ToC: only his seat 3044; the other
Ulduar vehicles' turning seats (siege engine seat 7, demolisher seats 1-2) hold creatures or no bot,
Kologarn's arm, Ignis and the Constrictor Tentacle seats do not turn; other raids unchecked.
`UldEncounter_FlameLeviathan.cpp`: `FlameLeviathanRiderTurret` picks the nearest activated turret;
`FlameLeviathanRiderStranded` is true only with no activated turret left, a live one waiting, riders
at the cap and the bot the lowest-guid bot rider (a human rider never blocks it). Seat action:
stalled jump and its state removed. Definition: `set facing` no longer blocked. Reader: `--riders`
and the `--summary` digest print the turned counts; `CAST_ARC_DEG` replaces `RIDER_ARC_DEG`.

**Next window should show** (`--riders`, `--summary`): the turned line near 100% while he moves
(window48 0-38%); flank and behind damage near the ahead rate (≥2k/s; window48 62-325/s); no
`stalled` ends; `stranded` only after two riders sat on one side; riders at once ≤ 2, hunters
seated 0; shutdowns every ~60 s or faster and shorter fights with fewer on-foot deaths. If riders
still miss a turret behind them, check for a transport spline overwriting the offset
(`Unit::UpdateSplinePosition`).

**Previous fix.** r1: no pulls on its code yet; its heading workarounds are withdrawn here.
