# Freya: revert the Detonating Lasher spread

Undo the positioning half of `45d9a3e36` and put `freya ranged camp` back. Army of the Dead, the
frost trap re-gate and the frost nova re-gate from that commit all stay.

## Why

`45d9a3e36` replaced the raid's 10 yd gather with a 16 yd lattice plus a multiplier pinning bots to
their slots. Pull `env/dist/logs/botobs/603_1_elder-stonebark_1788552829.ndjson` (2026-09-04) is the
first test of it. It did exactly what it was built to do and the raid died faster.

| | six pulls of 2026-09-02 (camp) | 1788552829 (spread) |
|---|---|---|
| Detonate damage taken | 127k-777k, 4.6-16.3% of all damage | **36.5k, 1.9%** |
| median bot to nearest lasher | 3.7-10.2 yd | **17.5 yd** (p90 **46 yd**) |
| melee DPS on the wave | 40.0k-77.7k | **21.6k** |
| raid DPS on the wave | 101k-231k | 103k |
| wipe at | 2:06-4:01 | **1:53.9** |

The raid stopped killing the wave. Ten lashers spawned at 0:10; the raid ground all ten to 0-8% by
1:25 and killed **two**. Eonar's Gift then healed the survivors back to ~65%, and from 1:27 to the
wipe sixteen living bots did **9.5k raid DPS between them**, about 590 each.

### Mechanism

`FreyaLasherSpreadHoldMultiplier` returns 0.0 for `ReachTargetAction`, which is the base class of
both `reach melee` and `reach spell`. Those are the only generic path a bot has for closing on a
hostile target: every other generic mover (`flee`, `runaway`, `move out of enemy contact`,
`follow`) retreats or is non-combat, and `CastSpellAction` does not move at all - out of range it
passes `isUseful` and `isPossible` and fails silently inside `Spell::prepare`. So a bot on a lattice
corner with the nearest lasher 40 yd away can never reach it, and `FreyaSetDpsPriorityAction` nulls
any lasher past its reach, leaving it no add target at all. In-trace `Ecoterrorist` sat a median
40.0 yd from the nearest lasher, `Stormweaver` pointed at one in 4 of 296 samples, and `reach spell`
was vetoed 55 times against `reach melee` 53.

Detonate was never what killed the raid: at 13-16% of damage taken it was always behind lasher melee
and Flame Lash. The spread traded a minority damage source for the raid's ability to kill the wave.

## Changes

### Delete

| Symbol | Location |
|---|---|
| `FreyaLasherSpreadTrigger` | `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.{h,cpp}` |
| `FreyaLasherSpreadAction` | `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}` |
| `FreyaLasherSpreadHoldMultiplier` | `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Freya.{h,cpp}` |
| `GetFreyaLasherSpreadSlot` and statics `BuildFreyaSpreadMembers`, `FreyaSpreadLatticeIndex`, `BuildFreyaSpreadCellOrder`, `ClampToRoom` | `src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp` |
| `ULDUAR_FREYA_LASHER_SPREAD_{SPACING,COLUMNS,TOLERANCE,ANCHOR_GRID,FALLBACK}`, `ULDUAR_FREYA_ROOM_{X,Y}_{MIN,MAX}` | `src/Ai/Raid/Uld/Util/UldEncounter_Freya.{h,cpp}` |

`RaidObs.h`, `<cmath>`, `<string>` and `<utility>` come out of `UldEncounter_Freya.cpp` with them -
the spread was their only user. `<algorithm>` and `<limits>` stay.

`freya lasher pack step out` stays deleted. It accepted once across six pulls, so it is dead code
either way, and the nova and trap no longer hang off its gate.

### Restore from `45d9a3e36^`, verbatim

- `GetFreyaRangedCampAnchor(PlayerbotAI*)` in `UldEncounter_Freya.cpp`, declaration in the header.
- `ULDUAR_FREYA_RANGED_CAMP_TOLERANCE = 10.0f`, `ULDUAR_FREYA_HEALER_CAMP_TOLERANCE = 15.0f`.
- `FreyaRangedCampTrigger` and `FreyaRangedCampAction`.

### Rewire

- `UldTriggerContext.h` / `UldActionContext.h`: `freya_lasher_spread` back to `freya_ranged_camp`.
  Leave the `freya_summon_army` pair alone.
- `UldStrategy.cpp`: `freya lasher spread` @ `ACTION_RAID + 3` becomes `freya ranged camp` @
  `ACTION_RAID`, its old band - the lowest, below the spore and the tank ladder. Nova, trap and army
  keep `ACTION_RAID + 2`. Drop `FreyaLasherSpreadHoldMultiplier` from `InitMultipliers` and keep
  `FreyaLasherTrapReserveMultiplier`.

### Out of scope

`FreyaSetDpsPriorityAction`'s reach clamp, `ULDUAR_FREYA_MELEE_LASHER_RANGE`,
`FreyaLasherFinishAoeMultiplier` and the pack helpers all predate `45d9a3e36` and are untouched by
it. `ULDUAR_FREYA_DETONATE_RADIUS` was already declared-but-unreferenced before the commit.

## Verification

The module cannot be compiled headless, so step 2 onward is a hand-off.

1. **Static**: `grep -rn "LasherSpread\|SPREAD_\|ULDUAR_FREYA_ROOM_" src/` returns nothing; both
   restored constants are referenced; `freya ranged camp` resolves in both context maps.
2. **Build** the worldserver in Docker.
3. **Re-pull** Freya 25 hard mode and capture a trace.
4. **Measure** against `1788552829` and the six 2026-09-02 pulls in `env/dist/logs/botobs/`:
   - melee DPS on the wave 21.6k -> back in the 40k+ band.
   - median bot to nearest lasher 17.5 yd / p90 46 yd -> back under 10 / under 22.
   - lashers killed in the first wave 2 of 10 -> the wave cleared.
   - wipe at 1:53.9 -> later than 2:06, the worst of the six.
   - Army of the Dead casts by the bot DK and ghoul taunts must not regress: 9 and 213.
   - Frost Trap casts > 0, Explosive Trap casts during a wave = 0.
5. `python tools/botobs/postmortem.py <trace>` - deaths read Detonate again rather than lasher
   melee, which is the trade being accepted here.
