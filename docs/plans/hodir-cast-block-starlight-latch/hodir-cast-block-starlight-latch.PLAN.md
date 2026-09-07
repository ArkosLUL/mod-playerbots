# Hodir: let a casting bot dodge, and stop the Starlight latch flapping

## Context

`env/dist/logs/botobs/603_1_hodir_1788722807.ndjson` (schema v10, 25 raiders, **kill at 5:48.164**,
6 deaths) is the first pull on `a92b89886` "Hodir: latch the Starlight stand, the shed leg and the
Storm Cloud lap". Compared against `603_1_hodir_1788378556.ndjson`, the 6:20.382 kill that motivated
that commit, the latches worked:

| | 6:20 kill | this pull |
|---|---|---|
| outcome | kill 6:20.382 | **kill 5:48.164** |
| raid dps | 115,296 | **124,438** |
| 40% / 30% / 20% / 10% | 3:33.6 / 4:21.3 / 5:02.5 / 5:40.3 | 3:05.4 / 3:53.3 / 4:33.5 / 5:10.4 |
| melee within 8 yd of Hodir | 49.8% | 58.6% |
| ranged beyond 30 yd | 18.3% | 8.8% |
| healers beyond 30 yd | 16.4% | 8.4% |
| moving, share of alive time | 58.7% | 53.9% |
| distance walked | 38,917 yd | 30,973 yd |
| stalls (6s+ still, accepted moves) | 318 s | 208 s |
| worst Storm Cloud carry, boss gap | 142 yd, ended in a death | 37 yd |
| **deaths** | **2** | **6** |

Two things went the wrong way and this plan fixes both. Everything else about the pull is an
improvement and is not touched.

Read a trace with `python tools/botobs/postmortem.py <file>` (`--stalls`, `--clump`, `--notes`,
`--bot <name>`). Schema: `docs/systems/observability.md`. Hodir code:
`src/Ai/Raid/Uld/{Action,Trigger,Multiplier,Util}/*_Hodir.*`.

---

## Problem 1 - a bot mid-cast cannot dodge, and nothing knows it

`Unit::IsMovementPreventedByCasting()` (`src/server/game/Entities/Unit/Unit.cpp:4372`) is true for
**any** cast in progress unless a channel carries `IsActionAllowedChannel`.
`PointMovementGenerator<T>::DoInitialize`
(`src/server/game/Movement/MovementGenerators/PointMovementGenerator.cpp:36`) then returns without
launching a spline, and `DoUpdate` (line 119) calls `StopMoving()` and returns every tick.

So `MoveTo` succeeds at the bot layer, the move record reads `ok`, `LastMovement` books the slot for
1000 ms, `IsDuplicateMove` (`src/Ai/Base/Actions/MovementActions.cpp:1025`) refuses every re-issue -
and the bot has not taken one step. `snap.u`'s `moving` column is literally `unit->isMoving()`
(`src/Bot/Obs/RaidObsSnapshot.cpp:55`), so the trace shows the stall but no action reacts to it.

**Smartface died twice to this, both to Ice Shards 62457.**

- **1:58.886.** Channelling Blizzard from 1:53.3. Frozen at (1965.32, -254.08) for 5.7 s. An icicle
  spawned **at 0.0 yd** at 1:55.079. The dodge issued (1970.87, -251.78) at 1:55.205, accepted at
  `forced` priority; position never changed. 13,987 at 1:58.884.
- **2:12.887.** Channelling **Evocation**, an 8 s deliberate stand-still, from 2:09.283 with an
  icicle 3.0 yd off. Dodge re-pointed at 2:09.455 accepted, then `dup`, `dup`, `dup`. 13,987.

There is room to act: an icicle gives about 3.6 s between spawning and detonating, which is what
`ULDUAR_HODIR_ICICLE_SPENT_MS = 3300` already encodes, and a dodge leg is 6 yd.

Raid-wide the pin costs 101 bot-seconds, 1.4% of alive time and 2.9% for ranged, and it was 119
bot-seconds in the previous pull - **not a regression**, a hole the tighter formation finally walked
into. Time held: Hurricane 29 s, Mind Flay 12 s, Evocation 11 s, Blizzard 9 s, Volley 8 s.

`PlayerbotAI::RequestSpellInterrupt()` (`src/Bot/PlayerbotAI.cpp:4291`) already exists for exactly
this and is consumed on the next AI update (`PlayerbotAI.cpp:288` for a preparing cast, `:363` for a
channel). `src/Ai/Raid/Uld/Util/UldBotScripts.cpp:26-53` uses it for both Vezax ground effects, with
the same "only break the cast of a bot that is about to be told to move" rule.

## Problem 2 - the Starlight latch flaps between zones, and the note hides it

`hodir.starlight` notes fell 437 to 131, which reads like the latch holding. It is not: the note
records the **rule**, not the zone, so re-latching onto a different zone still prints `stand` and
`RaidObs::NoteDerived` emits nothing (`src/Bot/Obs/RaidObsEngine.cpp:283` emits only on change).

`hodir.anchor` is where it shows:

| | 6:20 kill | this pull |
|---|---|---|
| real x/y anchor moves | 693 | 1,201 |
| median gap between them | 3,867 ms | **320 ms** |
| median hop size | 13 yd | 9 yd |
| Trueshot: hops / distinct points | 63 / 25 | 146 / **14** |

Ten hops per distinct point is flapping, and **1,137 of the 1,201 hops happen with `stand` in
force**. The cause is `FindHodirStarlightStand` (`src/Ai/Raid/Uld/Util/UldEncounter_Hodir.cpp:312`):
`standRejects` (line 365) re-tests the latched stand against a moving Hodir every tick, and line 406
**erases the latch outright** on a miss. The re-sweep ranks zones by `slot.GetExactDist2d(&zone)` and
lands on a different one, which rejects on the next tick, and back. There is no hysteresis on the
hard 15 / 30 yd `ULDUAR_HODIR_RANGED_MIN_BOSS_GAP` / `ULDUAR_HODIR_CASTER_MAX_BOSS_GAP` thresholds.

It is not costing distance yet - `hodir raid position action` walked 7,885 to 3,849 yd and issued
4,389 to 3,081 moves - but the anchor is no longer a stable target and the next thing that leans on
it will pay.

---

## Approach

Two edits, no new files, no new constants, no schema change.

### E1 - break the cast that is pinning the feet (`UldActions_Hodir.cpp`)

Add to the anonymous namespace at the top of `src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp`
(beside `IsEmptyPosition` and `IsClearOfHazards`):

```cpp
// PointMovementGenerator never launches a spline while IsMovementPreventedByCasting is true, so the
// MoveTo below is accepted, books the movement slot, and the bot stands still until the cast ends.
// Blizzard and Evocation held one caster in an Ice Shards pool for 5.7s and one for 3.5s; both died.
void BreakCastPinningTheFeet(PlayerbotAI* botAI, Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        botAI->RequestSpellInterrupt();
}
```

Call it as the **first statement** of `HodirIcicleDodgeAction::Execute` (line 62) and of
`HodirMoveSnowpackedIcicleAction::Execute` (line 46). Both are safe there because both triggers
already gate on "in danger and has to relocate":

- `HodirIcicleDodgeTrigger::IsActive` (`UldTriggers_Hodir.cpp:97`) needs a lethal icicle within
  `ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN`, and excludes tanks.
- `HodirNearSnowpackedIcicleTrigger::IsActive` (`UldTriggers_Hodir.cpp:68`) needs a shelter to exist
  and the bot to be beyond `ULDUAR_HODIR_SAFE_AREA_RELEASE` from it.

**Deliberately not the Biting Cold shed.** It holds the slot for 22% of movement time and 9,267 yd,
so interrupting for it would gut caster uptime for a 200 * 2^stacks tick that the arm threshold
already caps at 800. Ice Shards is 14,000 and Flash Freeze is lethal; Biting Cold is neither. Watch
the stack distribution in the next trace and revisit only if it moves.

### E2 - keep the latched Starlight zone across a reject (`UldEncounter_Hodir.cpp:382-407`)

Two changes inside the `held != latched.end()` block:

1. **Erase only when the zone is gone.** Move `latched.erase(held)` inside a `if (!stillUp)` branch.
   When the zone is still on the floor but `standRejects` fires, return `false` **without erasing**,
   so `DeriveHodirAnchor` (line 711) falls through to the ring slot for that tick and the same stand
   comes back the moment the reject clears. That is the whole fix for the flap: the bot can no
   longer be handed a *different* zone by a re-sweep that only ran because the boss drifted a yard.
2. **Report the reject.** Emit `RaidObs::NoteDerived(bot, "hodir.starlight", standRejects(...))` on
   that path (it already returns `"fire"` / `"noreach"`), so a held-but-rejected tick is
   distinguishable in the trace from a bot that never had a zone.

And close the instrumentation gap that hid this, at both `hodir.starlight` `"stand"` sites (line 400
and line 446): emit `"stand " + RaidObs::DescribeDerived(zone)` instead of bare `"stand"`, using the
latched zone and `bestZone` respectively. `DescribeDerived(Position const&)` is already used for
`hodir.anchor` at line 733. Note volume stays bounded by actual re-latches, which is the number this
plan exists to drive down.

Nothing else in `FindHodirStarlightStand` changes: the sweep, the bearing, and `standRejects` itself
all stay as they are.

**Contingency, do not build it now.** If E2 moves the flap from zone-to-zone to stand-to-ring-slot -
`hodir.anchor` hops stay high while `hodir.starlight` starts alternating `stand <zone>` and
`noreach` - the next step is hysteresis on the threshold itself: drop a latch at
`ULDUAR_HODIR_CASTER_MAX_BOSS_GAP + 2` and only take a fresh one at
`ULDUAR_HODIR_CASTER_MAX_BOSS_GAP`. Read the notes before writing that.

## Files

- `src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp` - E1, the helper plus two call sites
- `src/Ai/Raid/Uld/Util/UldEncounter_Hodir.cpp` - E2, the latch block and the two note sites
- `docs/raids/ulduar/hodir.md` - the measurements above. Reachable from `CLAUDE.md` via
  `docs/engine/pitfalls.md`, so this edit needs its own **`/compact-docs-writer`** invocation
- `docs/plans/hodir-cast-block-starlight-latch/hodir-cast-block-starlight-latch.PLAN.md` - copy of
  this plan, written first

No `CMakeLists.txt` change; AzerothCore globs module sources. Run
`python apps/codestyle/codestyle-cpp.py` from `modules/mod-playerbots` before calling it done. There
is no headless build path here, so the build and the pull are the user's.

## Verification

Rebuild, restart `ac-worldserver`, confirm the effective config with
`docker exec ac-worldserver env | grep ^AC_`, then pull Hodir with the same raid.

| check | this pull | expected |
|---|---|---|
| cast-blocked movement (point gen, not moving, mid-cast) | 101 bot-seconds, 2.9% of ranged alive time | under 30 bot-seconds |
| deaths to Ice Shards 62457 | 3 | 0 |
| Ice Shards 62457 damage taken | 221,646 | under 150,000 |
| time inside a lethal icicle radius | 15.3% of alive time | under 12% |
| `hodir.anchor` real x/y hops | 1,201 at p50 320 ms | under 400, p50 over 3 s |
| hops per distinct anchor point, worst bot | 146 / 14 | under 3:1 |
| `hodir.starlight` values | `stand` 54, `noreach` 32, `none` 24, `fire` 21 | `stand <zone>` dominant, few distinct zones per bot |
| stall time (`--stalls`) | 208 s | under 120 s |
| raid dps | 124,438 | no worse than 124,000 |
| kill time | 5:48.164 | no worse than 5:50 |
| Biting Cold 62188 self-damage | 963,922 (15.8% of damage taken) | no worse than 1,050,000 |

The last two rows are the guard rails: E1 spends casts to save lives and E2 stands a bot on its ring
slot for the ticks a latched zone rejects, so both can cost throughput. If dps drops, read
`hodir.starlight` for reject frequency before touching anything else.

Keep `603_1_hodir_1788722807.ndjson` for the before/after. Retention is 7 days.

## Out of scope

- **Bulwark, three deaths, half the total.** He took **27 of the 35 Frozen Blows 63511 hits for
  506k** against Dragon's 8 for 163k; last pull it was Dragon 19 / 371k and Bulwark 10 / 186k.
  `hodir frozen blows swap action` ran 11 times (7 OK) versus 29 (17 OK). Worst hit 28,204 against
  roughly a 39.5k pool, and the 4:35.537 death went 61.8% to zero in one swing. Healing was not the
  constraint: he received 1,425,511 against 762,568, nearest healer p50 18.1 yd, p90 26.9 (better
  than the previous pull's 29.7) - though at the 2:10.833 death all four healers were 30-35 yd out.
  Dragon is human-controlled, so part of the split is outside our code. **The user asked for this to
  be investigated before any change is proposed.** Start at `HodirFrozenBlowsSwapAction::Execute`
  and `HodirTauntWouldBeSuicide` (`UldEncounter_Hodir.cpp:70`), and check whether
  `HodirGuardMultiplier`'s taunt veto (`UldMultipliers_Hodir.cpp:65`) is standing the swap down.
- **The tighter stack.** Twelve or more bots inside one 10 yd circle went 39.7% to 56.1% of the
  pull, which is why icicle exposure rose 11.2% to 15.3% and Ice Shards damage 177k to 222k: icicles
  target players, so a tighter raid shares pools. Widening `ULDUAR_HODIR_DECLUMP_RADIUS` or the
  dodge clear would buy that back but risks trading away the dps the tighter formation just bought.
  The user deferred it; re-read the exposure number after E1 lands, since two of the three icicle
  deaths were the cast pin rather than the spacing.
- **Killing Spree.** Assasin died at 3:05.818 to Ice Shards 13,580. At 3:05.079 he was at
  (1977.70, -272.14) with the nearest icicle 5.7 yd off, walking a shed leg to (1969.86, -264.17);
  at 3:05.393 the snapshot has `moving=0`, `moveGen=0` and position (1976.95, -267.69), a 4.5 yd
  relocation with no spline, **1.7 yd from a live pool**. Killing Spree 51690 had been up 0.6 s with
  1.4 s left. A `HodirGuardMultiplier` veto is possible - it already zeroes `CastReachTargetSpellAction`
  during Flash Freeze - but the teleport destination is unpredictable, so any radius rule is a guess
  for one death. Deferred by the user.
- **The Biting Cold shed.** Still the biggest single mover at 9,267 yd, 30% of all raid walking, and
  the leg latch did not lengthen legs (p50 hold 934 ms against 947 before). What it did do is
  convert gate contests into duplicate re-offers: `wait` 4,122 to 1,559, `dup` 262 to 2,564. Working
  as designed - the chain has to keep going while the aura is up - but it is where the next
  movement-cost win is.
