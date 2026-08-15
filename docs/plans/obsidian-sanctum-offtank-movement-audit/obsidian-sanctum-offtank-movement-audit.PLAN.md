# Obsidian Sanctum — off-tank movement audit, and melee stop fighting the tsunami dodge

## Context

Eleventh round of in-game defects on the Sartharion 3-drake strategy
(`modules/mod-playerbots/src/Ai/Raid/OS/`). The user builds and raids, then reports. Rounds A-G are
committed (HEAD `4384ce6d7`); rounds H, I and J are implemented in the working tree and **not yet
built**, so this report describes pre-J behaviour. The doc rewrite covering E-J is drafted and still
unapplied.

Round K is two items:

1. The off-tank oscillates a lot. Audit his movement logic and keep only the locations he needs.
2. Melee oscillate while fighting drakes. They must not be guarded on the X axis by the Lava Tsunami
   dodge.

### The mechanism behind every oscillation in this file

`MovementAction::MoveTo` calls `IsDuplicateMove`
([MovementActions.cpp:177, :939](modules/mod-playerbots/src/Ai/Base/Actions/MovementActions.cpp#L939)),
which returns true when the same destination was requested inside `maxWaitForMove`. So a hold action
issues its move on one tick and **returns false on the next**, handing the tick to every lower-priority
action. Anything below it that wants a different destination then gets a turn, and the hold drags the
bot back after that. Two actions with different destinations and a live trigger each will alternate
forever. Every finding below is an instance of this.

### Already fixed by round J, unbuilt

The dominant off-tank cause is gone in the working tree. Pre-J the multiplier only zeroed generic
movers for him while his target was a Lava Blaze or a whelp (`IsHeldAtRange`); against a **drake** all
of `ReachTargetAction`, `CastReachTargetSpellAction`, `CombatFormationMoveAction` and `FollowAction`
stayed live, so they took the tick every time `os offtank hold` released it. Round J zeroed all of
them for him unconditionally, plus `AvoidAoeAction`, `MoveOutOfCollisionAction` and
`MoveOutOfEnemyContactAction` for everyone. What follows is what is left after that.

Decisions settled with the user: Vesperon's spot moves north, adds never move the off-tank, and melee
follow the drake on X while dodging on Y.

---

## Item 1 — off-tank movement audit

Every destination the off-tank can be sent to, post-J:

| action | priority | destination | verdict |
|---|---|---|---|
| `os return to platform` | `EMERGENCY+2` | (anchor X, **`SafeCorridorY`**) | Y is wrong — it is not his hold |
| `os avoid twilight fissure` | `EMERGENCY+1` | (fissure X ±10, **`SafeCorridorY`**) | 28yd off his hold to dodge a 5.5yd blast |
| `os tsunami corridor` | `EMERGENCY` | (own X, `SafeCorridorY`) | necessary |
| `os drake landing position` | `RAID+5` | inbound drake's spot, wave-overridden Y | necessary |
| `os offtank hold` | `RAID+4` | newest drake's spot / **east end of the line for an add** / Tenebron's spot | drop the east end |

Nothing else moves him: `os drake rear`, `os sartharion flank` and `sartharion melee positioning` all
gate on `!IsOffTank`, and the generic movers are zeroed.

### 1a. The fissure sidestep must keep his own Y

`OsAvoidTwilightFissureAction` builds its two X-step candidates at `corridorY = SafeCorridorY(bot)`
([OSActions.cpp:51, :62-67](modules/mod-playerbots/src/Ai/Raid/OS/OSActions.cpp#L51)). For ranged that
is where they already stand. The off-tank stands on a drake spot — Tenebron's Y is 563.38 against a
raid corridor at 535.5 — so a fissure inside 8yd sends him 28yd south, and `os offtank hold` walks him
back once the fissure expires. Melee on a drake have exactly the same problem. The comment on those
candidates already states the intent ("keeping the Y a tsunami dodge just bought"); the code does not
match it.

```cpp
// Only a live wave dictates Y here. Otherwise the sidestep keeps the Y the bot is holding: for ranged
// that is the corridor either way, but the off-tank and melee on a drake stand up to 28yd off it, and
// walking there to dodge a 5.5yd blast costs a round trip on every fissure.
float const stepY = ClassifyTsunamiWave(bot) != TsunamiWave::None ? SafeCorridorY(bot)
                                                                  : bot->GetPositionY();
```

Use `stepY` in place of `corridorY` in both candidate lists. The main tank is unaffected: he returns
false outright while a wave is live, and between waves he is standing on his corridor Y already.

### 1b. Adds never move the anchor

`OffTankAnchor` reads `OffTankChargeFor`, which falls through to the nearest Lava Blaze or whelp inside
30yd when no drake is down. `DrakeTankSpotFor` has no spot for an add, so the anchor collapses to
`OffTankHoldX` — the east end of the raid line at (~3265.6, 535.5), **32yd** from Tenebron's spot — and
back again when the add dies or drifts past 30yd. No hysteresis. Taunt reaches 30yd, so the add comes
to him regardless.

Split the drake pick out of `OffTankChargeFor` and let only that drive the anchor:

```cpp
// Newest drake down, or nullptr. Sartharion calls them Tenebron, Shadron, Vesperon on a fixed
// schedule, so reverse call order is landing recency.
Unit* NewestLandedDrake(Player* bot);
```

`OffTankChargeFor` becomes `NewestLandedDrake(bot)` else the add lookup, so the **taunt target** is
unchanged. `OffTankAnchor` uses `NewestLandedDrake` only:

```cpp
// The anchor tracks drakes and nothing else. Taunt reaches 30yd and a Lava Blaze or a whelp walks to
// him, so letting one set the anchor only bought a 32yd round trip every time one spawned or died
// inside that radius. With nothing down he waits on Tenebron's spot: it is the first drake called, so
// he stands where he will tank it 20s early, and 60yd from Sartharion.
Unit* drake = NewestLandedDrake(bot);
Position const* spot = drake ? DrakeTankSpotFor(drake->GetEntry()) : &TENEBRON_TANK_SPOT;
```

`spot` can no longer be null, so the `OffTankHoldX` fallback goes. `OffTankHoldX` and
`ADD_PILE_OFFSET_X` then have no callers and are removed. `OFFTANK_EAST_OFFSET` stays — melee still
meet a landing drake east of its touchdown ([OSActions.cpp:238](modules/mod-playerbots/src/Ai/Raid/OS/OSActions.cpp#L238)).

### 1c. Vesperon's spot moves north to (3266.0, 556.0, 59.513)

Round J's placeholder at Y 542 is 2.0yd from the left wave line at 540 and 6.0yd from the right one at
548 against an 8.5yd lethal half-width — inside **both** patterns, so the off-tank round-trips on every
wave. Tenebron (563.38) and Shadron (534.66) are each safe under one pattern and move only for the
other; Vesperon must be too.

Y 556 is 16yd clear of both left lines (540, 572) and 8yd from both right ones (548, 564), so he moves
for right waves only — Tenebron's profile. The cone requirement round J established still holds: the
drake lands at (3269.71, 532.79) and stops ~7yd short of the off-tank along that ray, at ~(3267.1,
549.1) facing ~99°. The raid's home hold (3256.58, 535.5) is 17.2yd away, outside the 15yd Shadow
Breath; its left-wave hold (3256.58, 551.0) is 10.7yd away and 71° off the facing, outside any
plausible arc.

**Still a placeholder.** Every other spot in that file is a hand-measured in-game reading and this one
must be too before it is trusted.

Recompute the clearance block in the `OSHelpers.h` header comment:

```
//   Vesperon 556.000  16.00 to the left line at 572,  8.00 to the right one at 564
```

and re-derive the Vesperon paragraph above it for the new Y.

### 1d. `os return to platform` uses his own hold

It sends everyone to `SafeCorridorY`, which for the off-tank is the raid corridor rather than his drake
spot, so an off-platform correction is followed by a second walk back north.

```cpp
if (IsOffTank(bot))
{
    Position const anchor = OffTankAnchor(bot);
    return MoveToClamped(anchor.GetPositionX(), anchor.GetPositionY(), 0.0f);
}
```

`OffTankAnchor` already carries the wave override, so this is his hold and not a third destination.

### Left alone deliberately

- **The wave round trip itself.** Left waves leave 500.5-515.5 and 548.5-563.5 safe, right waves leave
  ≤491.5, 524.5-539.5 and ≥572.5. The two sets are disjoint, and the platform ends at Y 569 so the
  ≥572.5 band does not exist. One trip per adverse wave is the floor.
- **The 3s landing pre-position window.** `FindInboundDrake` requires flight speed rate ≥2.0, which the
  script sets only when the dive starts, and the dive lasts ~3-5s. Widening the window buys nothing.
- **The long walk when a newer drake lands.** Each drake is held beside its own touchdown by design;
  the drake he leaves trails him on threat.

---

## Item 2 — melee fight the tsunami dodge

`os drake rear` (`ACTION_MOVE+6`) and `os sartharion flank` (`ACTION_MOVE+5`) know nothing about waves.
While one is live, `os tsunami corridor` (`ACTION_EMERGENCY`) moves Y and keeps X; on the next tick the
duplicate-move guard makes it return false, the tick drops to the rear or flank action, and that walks
the bot straight back to the drake's Y. They alternate for the whole ~11s a wave is up and never reach
the corridor.

The flank is worse than a stalemate. With the boss parked at (3228.52, 504.68) facing his tank, the
northern flank point lands at ~(3241.1, 518.7) — **5.3yd from the left wave line at 524**, so the flank
action actively walks melee into the wave the dodge just pulled them out of.

### Fix

Both actions take the corridor Y while a wave is live and keep choosing X from the drake or boss
geometry. The off-tank drags the drake to that same corridor Y, so melee arrive on its tail as it gets
there and keep their uptime through the wave.

`OsDrakeRearAction`, per candidate bearing:

```cpp
float y = wave == TsunamiWave::None ? drake->GetPositionY() + reach * std::sin(bearing)
                                    : SafeCorridorY(bot);
ClampDestination(x, y);

// The rear check only matters inside the breath. A wave puts melee well outside it, and the off-tank
// drags the drake to the same corridor Y, so they are back on its tail as it arrives.
Position const candidate(x, y, bot->GetPositionZ(), 0.0f);
if (drake->GetExactDist2d(x, y) <= DRAKE_BREATH_RANGE && !BehindDrake(drake, candidate))
    continue;
```

`DRAKE_BREATH_RANGE = 15.0f` is a new constant in `OSHelpers.h`; the figure is already stated in the
comments there and has no constant yet.

`OsSartharionFlankAction`, same override:

```cpp
float y = wave == TsunamiWave::None ? flank.second : SafeCorridorY(bot);
```

Its existing post-clamp `InSartharionCone` re-check is already range-aware (60yd frontal, 30yd rear),
so it needs no extra gate. Under a left wave melee-on-boss land at ~(3241.1, 511.09): 14.1yd from him,
inside melee range, 111° off the frontal cone and 69° off the rear. Under a right wave round J's
`CorridorGroup::Melee` already hands them 535.5, where the flank trigger goes quiet at 31yd.

### Why this removes the X guard

The dodge keeps the bot's X for a single tick; the rear or flank action then replaces it with the
drake- or boss-relative X **at the same Y**. Neither destination undoes the other, so nothing pins
melee to an X for tsunami reasons — Y is the only axis a wave kills on and the only one the dodge now
decides.

---

## Files

All under `modules/mod-playerbots/src/Ai/Raid/OS/`.

| file | change |
|---|---|
| `OSHelpers.h` | `DRAKE_BREATH_RANGE`; declare `NewestLandedDrake`; remove `OffTankHoldX` and `ADD_PILE_OFFSET_X`; recompute Vesperon's clearance row and re-derive its paragraph |
| `OSHelpers.cpp` | `NewestLandedDrake` split out of `OffTankChargeFor`; `OffTankAnchor` tracks drakes only; delete `OffTankHoldX`; `VESPERON_TANK_SPOT` → (3266.0, 556.0, 59.513) |
| `OSActions.cpp` | fissure sidestep keeps the bot's own Y; `os return to platform` uses the off-tank's anchor; `os drake rear` and `os sartharion flank` take the corridor Y under a wave |

No trigger, strategy or context change, and no new action or trigger classes, so `OSTriggers.cpp`,
`OSStrategy.cpp`, `OSTriggerContext.h` and `OSActionContext.h` are untouched.

## Docs

The E-J rewrite is still unapplied, so round K folds into the same pass: the off-tank's destination
inventory and which of them were dropped, the fissure sidestep keeping its own Y, Vesperon's spot and
clearances at Y 556, and melee holding the corridor Y while keeping the drake's X. Presented as a
unified diff with the word delta measured from the files, applied only on approval.

## Verification

**Static**

1. `python apps/codestyle/codestyle-cpp.py` — no new findings under `src/Ai/Raid/OS/`.
2. Grep `OffTankHoldX` and `ADD_PILE_OFFSET_X` across `src/` — no hits left.
3. Grep `VESPERON_TANK_SPOT` — still read only through `DrakeTankSpotFor`.
4. `NewestLandedDrake` returns only drake entries, so `DrakeTankSpotFor` in `OffTankAnchor` cannot
   return null.
5. Trigger and action name parity unchanged at 16/16/16 against 15 action creators plus the documented
   `rear flank` borrow.

**In-game**

- The off-tank holds his drake's spot and stays on it. A Lava Blaze or whelp spawning near him does not
  move him — he taunts it from where he stands. A Twilight Fissure sidesteps him ~10yd along X at his
  own Y, not 28yd south to the raid line.
- He settles at the new Vesperon spot and only dodges for **right** waves there, not both. Vesperon
  faces roughly north and its breath misses the raid line under both wave patterns. Re-measure the
  coordinate before trusting it.
- Melee on a drake move to the raid corridor Y when a wave goes out **and stay there**, tracking the
  drake along X instead of bouncing between it and the corridor. Melee on Sartharion under a left wave
  hold Y ~511 on his flank rather than being walked out to ~518.7.
- Round E-J behaviour still holds: the pull drag and settle, the off-tank never swinging at Sartharion
  and keeping bear form, melee behind the drakes, no mage Blinks or hunter Disengages, nobody standing
  east of X 3268, burst at Tenebron 70%, the full portal squad on the acolyte call.

**Not in scope:** no build (the module cannot be compiled headless here) and no git operation — nothing
is committed without an explicit instruction naming the command.
