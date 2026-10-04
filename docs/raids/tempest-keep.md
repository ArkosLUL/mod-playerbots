# Tempest Keep — The Eye (map 550)

Strategy key `tempestkeep`. Cross-raid conventions are in [README.md](README.md).

## Void Reaver

Ranged bots back off and spread with `FleePosition` (`VoidReaverRangedBackOffAndSpreadAction`), which
picks reachable ground. Upstream's rewrite removed the old spread-to-a-ring step, whose raw ring
points fell off the navmesh and froze every ranged bot: that trap is in
[../engine/pitfalls.md](../engine/pitfalls.md) ("Movement that silently no-ops").

Void Reaver is 25-man non-heroic, so there is no heroic variant to mirror.

## Also here

**Bots arriving with the wrong strategy** was first diagnosed on Void Reaver: most bots carried `ssc`
instead of `tempestkeep`, so no Void Reaver trigger ever fired. That is a persistence bug, not an
encounter one — see the instance-strategy rule in
[../engine/action-selection.md](../engine/action-selection.md).
