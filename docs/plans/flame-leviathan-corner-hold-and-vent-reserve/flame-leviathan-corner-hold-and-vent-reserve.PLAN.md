# Flame Leviathan 2026-09-17: the corner posts hold adds they cannot shoot, and the vent reserve dodges itself out of reach

## Context

The 17:34 pull (`603_2_flame-leviathan_1789666489.ndjson`, 1:58, 15 hulls, none lost) is the first
trace on the `9ed7440bb` build. It is a kill, but not a clean one: his health steps from 91.26% to
23.73% in one 0.2 s frame at t=37.0 (~218M of a 322,697,632 max), the same step the 09-16 pull took at
t=82. Outside it he loses ~15%/min, so a real kill runs ~7 minutes. Every figure below therefore comes
from a 2 minute sample: Battering Ram and the long-fight measures have almost no data in it.

Three things from the last cycle are confirmed fixed and must not regress: Battering Ram damage 0 (was
22.2 / 22.4%), Mimiron's Inferno 0 hull frames inside a patch (was 1.19M / 0.38M), drive moves refused
`wait` 11-19% by class (was 57-59%).

What is left: adds and the one lost vent channel are 45% of all hull damage, and both trace back to
geometry the code already describes but never actually reaches.

## Findings

### 1. Corner containment holds the add and then cannot hurt it

The posting works better than `--corners` reports. Its `held` column reads 0 of 8 because it uses the
add's *first* victim, and `npc_freya_ward_summon` zone-engages every player, pet and vehicle within
250 yd at zero threat, so the victim is decided before anyone can shoot. It resolves to Salvaged
Chopper #1576, the first bot in the roster, for 11 of 15 adds. The posted engine then out-threats it
and takes the add within **1.0-3.2 s**, after the add has moved 0-51 yd.

The failure is what happens next:

- The add closes to melee at a **median 9.7 yd** from the hull, and **76.5%** of held-add frames are
  under 10 yd. Fire Cannon 62358 is RangeIndex 164, minimum **10 yd** (checked in SpellRange.dbc), so
  the gunner's only gun cannot fire at the thing chewing its own hull.
- Ram 62345 is RangeIndex 11, 0-15 yd, **no cooldown**, so it covers exactly that band. But a posted
  engine never turns (`SiegeEngineAction`, "a posted one owes its facing to its corner", and the drive
  passes `ULDUAR_FL_ARENA_CORNERS[slot]` as the facing target). Measured: of held-add frames inside
  Ram's 15 yd, **87.4% are outside the 100 degree cone, and 69.5% are 120-180 degrees off**, i.e.
  behind the engine. The add walks out toward the chopper, gets taken, and comes back from the arena
  side, which is the engine's rear.
- `FlameLeviathanHeldByAnotherPost` then keeps the whole rest of the fleet off it, because its victim
  is a siege engine parked inside `ULDUAR_FL_CORNER_HOLD_RADIUS` of a post.

The result is measurable: held adds lose **1.5-2.1 %/s**, against **7.2-22.1 %/s** for the ones that
stay outside 10 yd. Wards of Life lived 24.2, 25.5, 31.0 and 52.5 s. Adds pile up (0/4/2/5/7/6 alive
per 20 s), Lash is 485,595 damage, 15.3% of all hull damage, and "adds within 8 yd" is the top cause on
exactly the two posted engines, 1580 and 1598.

Secondary: the engine parks 16-21 yd from the spawn point, not the 12 yd `ULDUAR_FL_CORNER_STANDOFF`
intends, because of the arrival deadband. That is outside Ram's 15 yd, so the post cannot open on a
fresh ward even when it is dead ahead.

### 2. The vent reserve is the only interrupter, and a hammer mark takes it out of reach

3 of 4 channels were cut at one tick. The fourth ran all 11: 945,000 damage, 29.8% of hull damage, and
hull health falls 4.24% per 5 s while channelling against 0.71% otherwise. The sequence is exact:

| t | what |
|---|---|
| 59.94 | Storm tower drops its 8 marks (`SummonCreature(33364, 157+rand()%200, -140+rand()%200, ...)` x8, 24 s despawn, 5-20 s fuse each); one lands 5.4 yd from reserve hull 1574, which is 33.3 yd from his centre and in reach |
| 60.03 | Flame Vents 62396 starts |
| 60.145 | `flame leviathan interrupt vents` runs, returns FAILED: it bails whenever `GetFlameLeviathanNearestTowerHazard` finds anything |
| 61.0 | hull is 12.8 yd from the mark, clear of the 7 yd circle, but the forced dodge legs keep coming |
| 62.04 | `fl.rush=vent` fires Steam Rush, 40 energy |
| 62.244 | a forced `hazard:hammer` leg overrides the charge |
| 60.2 → 70.1 | hull drifts 40.2 → 52.4 yd, never back inside Electroshock's 40 yd; the action is never attempted again |

It is never attempted again because `FlameLeviathanIsVentInterrupter` requires
`FlameLeviathanCanElectroshock`, which requires cone range, so losing the range kills the trigger, not
just the shot. Only 4 `interrupt vents` act rows exist in the whole pull, one per channel.

Nobody could cover. The other four siege engines were 51, 63, 147 and 211 yd out: three on corner
posts, one kiting Pursued. With five engines, four posts and one reserve, the reserve is the only
candidate every channel.

The padding is what makes the dodge continuous. The reserve was inside the *padded* hazard reach
(7 yd radius + hull size + margin, roughly 17-20 yd for a siege engine) for the whole channel while
being inside the actual 7 yd circle only for the first second. Eight marks scattered over a 200x200
box against a ~20 yd reach covers most of the middle of the arena.

### 3. Pyrite is close to target, and what is left is range and energy

Mean stacks 4.4 / 5.8 / 5.9 / 3.3 (was 1.4-3.3); past 70 yd 17-40% (was 48-72%); at 10 stacks 29-37%
for three drivers, 0% for Trueshot, who only got a hull at 0:35.

The refresh rule is solved: 1 `late` in the whole pull. The 8 stack losses are 4 `fail`, 3 `dry`, 1
`late`, and `fail` is purely range: **383 of 385 `fail` frames had the demolisher past 70 yd**.
Demolisher COMBAT moves are the worst class for refusals at 19%.

`dry` is new and real: 13-33% of driver time with no energy. The crate ledger reads 46 casts on 20
crates for 48 credits, with "energy at the grab: median 90%, above the 75 ceiling 30 of 46" - but the
reader samples the snapshot *after* the grab's +25 energize, so that column is probably an artifact and
must be re-measured before the gate is touched.

### 4. What hurt the vehicles

3,171,705 logged across 839 rows, 15 hulls, none lost.

| source | share | note |
|---|---|---|
| Missile Barrage 62400 | 39.7% | untargeted, unavoidable |
| Flame Vents 63847 | 29.8% | the single unblocked channel |
| Lash 65062 | 15.3% | the adds, finding 1 |
| Thorim's Hammer 62912 | 15.2% | see below |
| Battering Ram | 0% | 4 Pursued spans, he never closed inside 30 yd |
| Mimiron's Inferno | 0% | 0 hull frames inside a patch |

**Thorim's Hammer is not a 7 yd circle.** `spell_thorims_hammer::RecalculateDamage` is
`dist <= 7 ? base : base / max(dist - 6, 1)`, so every strike hits every hull on the map with 1/d
falloff. 117 hits, median 4,131, median 56.9 yd from the nearest mark, only 1 inside the circle. A
direct hit is around 210k. So the dodge earns its keep and that 483k tail is a floor, not a miss. Only
3 of the 117 landed on a hull in the `hazard:hammer` branch; 49 landed on hulls holding station.

## Changes

### A. Let a posted engine turn onto the add it is holding

`src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp`

1. **Drive, corner branch** (~line 621). Today `DriveTo(post, ULDUAR_FL_ARENA_CORNERS[slot])`. Pick the
   add this hull is holding (victim == this hull, or `FlameLeviathanBestAdd` inside
   `ULDUAR_FL_CORNER_HOLD_RADIUS` of the post) and pass it as the facing target; fall back to the
   corner when there is none. This is the same shape `HoldStation` already uses for unposted hulls
   (`DriveTo(goal, add, false)`), so the drive and the cast node still agree on the facing.
2. **`SiegeEngineAction`** (~line 401). The posted-engine no-turn exception stays only while the shot
   is the boss. When the shot is an add, run `FlameLeviathanFaceForCone` like an unposted engine does;
   change 1 makes the drive point the same way, so they cannot fight. The vent reserve's exception is
   untouched.
3. **`ULDUAR_FL_CORNER_STANDOFF`**. The post sits 16-21 yd from the spawn in practice against the 12 yd
   the constant intends, because `DriveTo` parks inside `ULDUAR_FL_ARRIVE_TOLERANCE`. Subtract the
   tolerance from the standoff so the parked hull, not the goal, ends up inside Ram's 15 yd of the
   corner. Re-comment with the measured numbers.

### B. Do not claim an add the post has no shot at

`src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp`, `FlameLeviathanHeldByAnotherPost`.

The claim currently holds on victim plus post proximity alone. Add the only two conditions under which
the holder can actually fire: the add is inside Ram's `ULDUAR_FL_RAM_CONE_RADIUS` of the holder, or it
is outside `ULDUAR_FL_FIRE_CANNON_MIN_RANGE` of it. An add that is neither is in the dead band that
produced 1.5-2.1 %/s, and the fleet should be allowed to splash it. Both tests are instantaneous, no
new state. Comment the why: a claim that outlives the holder's reach is what kept Wards of Life alive
for 52 s.

This is the safety net if A under-delivers; with A working it should almost never fire.

### C. Keep the vent reserve inside Electroshock range

`src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp`, `src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.{h,cpp}`

1. **Strict circle for the reserve.** Add a reach argument (or a `strict` flag) to
   `GetFlameLeviathanNearestTowerHazard` so the vent reserve, while `FlameLeviathanMsToNextVent` is 0 or
   inside `ULDUAR_FL_VENT_RUSH_LEAD_MS`, tests the hazard's own radius plus hull size with no margin.
   Everyone else keeps the padded reach. Measured: the reserve was inside the padded reach for the full
   10 s channel and inside the real circle for about 1 s.
2. **`FlameLeviathanInterruptVentsAction::Execute`.** `dodging` keeps
   `FlameLeviathanShouldClearBatteringRam` but takes the strict hazard test from 1, so a mark 12 yd
   away no longer hands the channel away.
3. **Dodge toward the boss.** In `ClearHazard`, when the caller is the vent reserve during a channel,
   rank candidate clear points by keeping the boss inside `ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS` of the
   hull's edge instead of by nearest. Fall back to nearest when no such point clears.
4. **`RushToVents`.** Refuse the rush while a strict hazard or `FlameLeviathanShouldClearBatteringRam`
   is live, so the 40 energy is not spent on a charge a forced leg overrides 0.2 s later.

Note for the record, no change: there is no second interrupter to promote. With four corners manned the
other engines sat 51-211 yd out all channel, so the reserve is structurally alone. If C is not enough,
the next lever is posting three corners, not a better election.

### D. Pyrite: lead him, and re-measure the crate ceiling

1. **Station lead** (`HoldStation`, demolisher branch). `FlameLeviathanRearPoint` is built from his
   current position, so a hull driving to it arrives where he was. Offset the goal along his velocity by
   the hull's travel time. That is the 17-40% past 70 yd and the 383 of 385 `fail` frames.
2. **Crate ceiling.** Fix the reader first (E), then decide. Do not touch
   `ULDUAR_FL_CRATE_GRAB_CEILING` on a post-energize reading.

### E. Reader

`tools/botobs/bosses/flame_leviathan.py`

1. **`--corners`**: drop the first-victim `held` column, which the zone-engage decides before any bot
   can act, and replace it with hold quality: spawn to first siege-engine victim, distance moved by
   then, and once held, the share of frames under `FIRE_CANNON_MIN_RANGE`, the share outside the
   holder's Ram cone, and the add's %/s. Print held vs free %/s side by side.
2. **`--vents`**: per channel, the reserve's drive branch at channel start, its distance to his edge
   each second, whether it was ever inside the strict hazard circle as against the padded one, and any
   `fl.rush` note followed by a forced leg within 1 s.
3. **`--hulls`**: teach it `damage / max(dist - 6, 1)` so hammer damage splits into "inside the 7 yd
   circle" and "falloff tail", instead of reading 15.2% as a dodge failure.
4. **`--pyrite`**: read crate-grab energy from the snapshot before the grab, not after.
5. Tests in `tools/botobs/tests/test_botobs.py` for each new pure helper: hold-quality split, strict vs
   padded reach, hammer falloff bucketing.

### F. Docs (`/compact-docs-writer` at cycle close)

`docs/raids/ulduar/flame-leviathan.md`:
- Corner section: the zone-engage decides the first victim, the post takes the add in 1-3.2 s, and the
  hold only pays once the engine can turn. The 10 yd Fire Cannon floor and Ram's 0-15 yd no-cooldown
  band as the two halves of one loop.
- Vent section: five engines, four posts, one reserve, so the reserve is alone; the strict circle rule;
  losing range kills the trigger, not just the shot.
- Thorim's Hammer: the 1/d falloff, ~210k inside 7 yd against ~4k at 50, and that the tail is a floor.
- Baseline table for 2026-09-17 17:34.

`docs/engine/pitfalls.md`:
- A padded hazard reach turns a sparse hazard into a permanent one when the field carries many marks at
  once; pad for a hull that must keep a firing position only by what would actually hit it.
- A claim that says "this unit is mine to kill" needs a liveness test, or it protects the target.

## Verification

Offline:
- `~/.claude/scripts/pb-syntax-check.sh` on `UldActions_FlameLeviathan.cpp`,
  `UldEncounter_FlameLeviathan.cpp` and the includers of the changed headers, at most 15 per call.
- `tools/pblint/pblint.py` on the same paths.
- `python -m unittest discover -s tools/botobs/tests` (tools/ changes, so required).
- Re-run `--corners`, `--vents`, `--hulls`, `--pyrite` on `603_2_flame-leviathan_1789666489.ndjson` and
  reproduce: held adds 76.5% under 10 yd, 87.4% outside the Ram cone, 1.5-2.1 %/s held against
  7.2-22.1 %/s free, 383 of 385 `fail` frames past 70 yd, 1 of 117 hammer hits inside the circle.

Live, after a module rebuild, a four-tower pull:

| measure | 09-17 17:34 | target |
|---|---|---|
| held-add frames outside the holder's Ram cone | 87.4% | under 25% |
| held-add %/s | 1.5-2.1 | above 6 |
| adds alive at once, peak | 7 | under 4 |
| Lash share of hull damage | 15.3% | under 5% |
| vent channels run full with a siege engine alive | 1 of 4 | 0 |
| reserve distance to his centre through a channel | 33 → 52 yd | stays under 40 |
| Steam Rush overridden by a forced leg within 1 s | 1 of 3 | 0 |
| demolisher frames past 70 yd | 17-40% | under 15% |
| demolisher mean stacks | 3.3-5.9 | above 6 |
| Battering Ram / Inferno hull damage | 0 / 0 | unchanged |

## Watch for

- **A posted engine spinning.** It now turns for adds, so two adds on opposite sides could make it
  alternate. `FlameLeviathanBestAdd` already picks once rather than falling through; check `fl.corner`
  ticks for a facing that never settles.
- **Adds pulled out of the corner** by change B, which is exactly what the `CORNER_HOLD_RADIUS` comment
  warns about: a 76k Fire Cannon from elsewhere can pass the 130% ranged threat switch. B should only
  fire in the dead band, so if adds start leaving manned posts, tighten its conditions.
- **The reserve standing in a hammer mark** now that its reach is strict. A direct hit is ~210k, so one
  is instantly visible in `--hulls`.
- **The reserve parked out of reach anyway** if change C3 finds no clear point that keeps him in range.
- **The demolisher lead overshooting** when he turns: a goal projected along a velocity he is about to
  reverse is worse than a stale one. Watch `fail` and the refusal rate together.
- **His health step** repeating. It is not restart-specific (09-16 did the same) and nothing here
  depends on it, but a 2 minute fight will keep starving every long-fight measure in the table above.
