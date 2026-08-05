# Tempest Keep — The Eye (map 550)

Strategy key `tempestkeep`. Cross-raid conventions are in [README.md](README.md).

## Void Reaver — the navmesh case study

Ranged bots never spread to their ring and froze wherever they started. This is the reference case
for two traps that recur elsewhere, both detailed in [../engine/pitfalls.md](../engine/pitfalls.md).

The spread action computed a **raw geometric point** at radius 45 around the boss and called
`MoveTo(..., exact_waypoint = false, generatePath = true, …, bot->GetPositionZ(), …)`. That path goes
through `SearchForBestPath`, which **never validates the target against the navmesh** — when the raw
ring point is off the walkable platform, or the supplied Z (the bot's own elevation, not the platform
floor) is outside the height search band, it returns `INVALID_HEIGHT` and `MoveTo` returns `false`.

What turned a failed move into a permanent freeze: `VoidReaverMaintainPositionsMultiplier` disables
`CombatFormationMoveAction` while the boss is present — intended, so bots do not auto-run to melee —
which means **the spread action is the only thing that can position ranged bots**. When it fails they
fall through to lower-priority combat actions, and there are none that move them.

**The contrast that proves it**: Akil'zon in Zul'Aman uses the exact same
`DisableCombatFormationMove` multiplier, so the multiplier is not the problem. Its spread action just
calls `FleePosition(nearestPlayer->GetPosition(), minDistance, minInterval)` — and `FleePosition`
picks a **navmesh-validated reachable** destination via `BestPositionForRangedToFlee`, so it never
targets an off-mesh point. That is also why Void Reaver's *second* branch (the post-spread
`FleePosition` hold) worked while the initial ring `MoveTo` did not.

The fix keeps the even angular ring but makes the destination reachable: use the **boss/platform
floor Z** rather than the bot's own; `AttackStop()` + `InterruptNonMeleeSpells(true)` before moving
and issue at `MOVEMENT_FORCED`, so an in-progress cast cannot keep the bot rooted and the move is not
blocked by a lower-priority `IsWaitingForLastMove`; and shrink the radius (45 → 36 → 27) until the
path succeeds.

Void Reaver is 25-man non-heroic, so there is no heroic variant to mirror.

## Also here

`KaelthasSunstriderSpreadAndMoveAwayFromCapernianAction::RangedBotsDisperse` is the working sibling
whose `AttackStop` + `MOVEMENT_FORCED` shape the Void Reaver fix copies.

**Bots arriving with the wrong strategy** was first diagnosed on Void Reaver: most bots carried `ssc`
instead of `tempestkeep`, so no Void Reaver trigger ever fired. That is a persistence bug, not an
encounter one — see the instance-strategy rule in
[../engine/action-selection.md](../engine/action-selection.md).
