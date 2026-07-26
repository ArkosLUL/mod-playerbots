# Void Reaver — ranged bots don't spread to initial positions

## Context

In the Void Reaver strategy (Tempest Keep), non-tank ranged bots are supposed to
fan out to a ring around the boss at pull, then hold and dodge Arcane Orbs. In
practice they never move to the ring — they freeze wherever they started.

The goal is to find the root cause and make ranged bots reliably spread, matching
the behavior of the already-working analog boss Akil'zon in Zul'Aman.

## Investigation / root cause

The relevant pieces:

- Trigger [TKTriggers.cpp:140](../src/Ai/Raid/TK/TKTriggers.cpp#L140) `VoidReaverBossLaunchesArcaneOrbsTrigger` — fires for any ranged bot while the boss is not targeting them. Wiring is correct (context, strategy node at `ACTION_RAID + 1`, action creator all present).
- Multiplier [TKMultipliers.cpp:117](../src/Ai/Raid/TK/TKMultipliers.cpp#L117) `VoidReaverMaintainPositionsMultiplier` disables `CombatFormationMoveAction` while the boss is present. This is intended (keeps bots from auto-running to melee range), but it means **the spread action is the only thing that can position ranged bots** — if it fails, they stand still.
- Action [TKActions.cpp:644](../src/Ai/Raid/TK/TKActions.cpp#L644) `VoidReaverSpreadRangedAction::Execute` — the initial-spread branch (`!hasReachedVoidReaverPosition[guid]`) computes a **raw geometric point** at `radius = 45.0f` around the boss and calls:

  ```cpp
  MoveTo(TEMPEST_KEEP_MAP_ID, targetX, targetY, bot->GetPositionZ(), false,
         false, false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
  ```

**Why it fails:** with `exact_waypoint = false` and `generatePath = true`, this call
goes through [MovementActions.cpp:240](../src/Ai/Base/Actions/MovementActions.cpp#L240)
`SearchForBestPath(...)`. The target `(targetX, targetY, bot Z)` is never validated
against the navmesh. When the raw ring point is off the walkable platform, or the
supplied Z (the bot's own elevation, not the platform floor) is outside the height
search band, `SearchForBestPath` returns `INVALID_HEIGHT` and `MoveTo` returns
`false`. The bot then falls through to lower-priority combat actions — but combat
formation movement is disabled by the multiplier — so it never moves. Every ranged
bot fails the same way, so none spread.

Proof this is the failing step: the action only returns `false` near the boss when
`MoveTo` returns `false` (the distance check `> 2.0f` is otherwise true and issues a
move). So a non-spreading bot ⇒ `MoveTo` is returning false at the ring target.

**Contrast with the working boss.** Akil'zon uses the exact same
`DisableCombatFormationMove` multiplier ([ZAMultipliers.cpp:30](../src/Ai/Raid/ZA/ZAMultipliers.cpp#L30)),
so the multiplier is not the problem. Its spread action ([ZAActions.cpp:84](../src/Ai/Raid/ZA/ZAActions.cpp#L84))
just does:

```cpp
FleePosition(nearestPlayer->GetPosition(), minDistance, minInterval);
```

`FleePosition` ([MovementActions.cpp:2214](../src/Ai/Base/Actions/MovementActions.cpp#L2214))
picks a **navmesh-validated reachable** destination via `BestPositionForRangedToFlee`
and moves with `normal_only`, so it never targets an off-mesh point. That is exactly
why Void Reaver's *second* branch (the post-spread `FleePosition(voidReaver, 20, ...)`
hold) works while the *initial* ring `MoveTo` does not.

Note: The Eye / Void Reaver is a 25-man non-heroic encounter, so there is no heroic
variant to mirror here.

## Fix (chosen: keep the ring, make the target reachable)

Keep the even angular distribution (healer/ranged-DPS index → angle on a ring around
the boss), but make the destination navmesh-reachable so `MoveTo` stops returning
false. All changes are inside `VoidReaverSpreadRangedAction::Execute` in
`modules/mod-playerbots/src/Ai/Raid/TK/TKActions.cpp`; the `GetHealerIndex` /
`GetRangedDpsIndex` helpers are unchanged.

Concrete changes to the `!hasReachedVoidReaverPosition[guid]` branch:

1. **Use the boss/platform floor Z, not the bot's own Z.** The current call passes
   `bot->GetPositionZ()`; if the bot is at a different elevation than the platform
   (e.g. still on the entrance ramp), the navmesh height search near `(targetX,
   targetY)` misses and returns `INVALID_HEIGHT`. Use `voidReaver->GetPositionZ()`
   for the destination Z so the height band is centered on the platform floor.

2. **Stop the current action and force the move**, mirroring the working sibling
   `KaelthasSunstriderSpreadAndMoveAwayFromCapernianAction::RangedBotsDisperse`
   ([TKActions.cpp:1197](../src/Ai/Raid/TK/TKActions.cpp#L1197)):
   call `bot->AttackStop();` + `bot->InterruptNonMeleeSpells(true);` before moving and
   issue the move at `MovementPriority::MOVEMENT_FORCED`, so an in-progress cast /
   auto-attack can't keep the bot rooted in place and the move isn't blocked by a
   lower-priority `IsWaitingForLastMove`.

3. **Guarantee reachability with a radius fallback.** If the full-radius ring point
   is off the platform, `MoveTo` still returns false. Wrap the move in a small
   fallback that shrinks the radius until the path succeeds, e.g. try `45`, then
   `36`, then `27` yards at the same angle and return on the first `MoveTo` that
   returns true. This keeps the even spread while ensuring every bot actually moves
   regardless of exact arena bounds. (Keep `radius` as the starting value; the
   existing `> 2.0f` "arrived" check and the `hasReachedVoidReaverPosition[guid] =
   true` latch stay as-is, using whichever radius succeeded.)

Leave the post-spread `else` branch (`FleePosition(voidReaver, 20, ...)`) untouched —
it already works.

### Sketch

```cpp
// inside the !hasReachedVoidReaverPosition[guid] branch, after angle is computed:
float const targetZ = voidReaver->GetPositionZ();
bool moved = false;
for (float r : { radius, radius * 0.8f, radius * 0.6f })
{
    float tx = voidReaver->GetPositionX() + r * std::cos(angle);
    float ty = voidReaver->GetPositionY() + r * std::sin(angle);
    if (bot->GetExactDist2d(tx, ty) <= 2.0f)   // already at this slot
        break;
    bot->AttackStop();
    bot->InterruptNonMeleeSpells(true);
    if (MoveTo(TEMPEST_KEEP_MAP_ID, tx, ty, targetZ, false, false, false, false,
               MovementPriority::MOVEMENT_FORCED, true, false))
    {
        moved = true;
        break;
    }
}
if (moved)
    return true;
hasReachedVoidReaverPosition[guid] = true;   // arrived (or no reachable slot)
```

(Refactor the two `if (healerIndex...) / else if (rangedDpsIndex...)` blocks so they
produce a single `angle`, then run the loop above once — avoids duplicating the ring
math per role.)

## Verification

Build worldserver, load into a 25-man raid at Void Reaver, pull, and confirm ranged
bots fan out (spread from each other and hold at range from the boss) instead of
freezing, then dodge Arcane Orbs. No SQL or config changes involved.
