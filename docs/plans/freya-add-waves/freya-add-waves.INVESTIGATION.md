# Freya add waves - what is still open after the 2026-10-11 pulls

## Context

Post-mortem of three 25-man hard-mode Freya pulls. Traces in `env/dist/logs/botobs/`:
`603_44_elder-stonebark_1791742873` (A1, reset at 1:27), `_1791743089` (A2, reset at 2:39.9),
`_1791743444` (K, kill 6:33). Combat log:
`chronicle-app-1:/root/.chronicle/storage/raidlogs/logs/4a60af0e-fd53-46c6-b1da-73747c09fd98`
(epoch-ms; pull t0 = 1791742874728 / 1791743091118 / 1791743446079).

The camp geometry and the three Snaplasher fixes shipped and live in
[docs/raids/ulduar/freya.md](../../raids/ulduar/freya.md), as do the Army fixes. The item below is
**proposed, not built** and needs its own go-ahead.

## Bomb escapes that returned false for a whole fuse

87 of 88 exposed bot-volleys escaped, median **0.61s**. `Power` at 6:17.2 did not: bomb at 0.0 yd,
three more inside 10, `freya move away nature bomb` FAILED on four ticks running, **no move order
issued**, not rooted or stunned by any aura - so every sweep tier and `StepOffNatureBombs` came back
empty. Bot at (2356.27, -37.18, 425.17); bombs at (2356.3,-37.2) (2353.2,-41.5) (2352.8,-46.0)
(2352.7,-45.0) (2348.9,-49.3) (2348.2,-48.7) (2373.3,-53.0). The step-off bearing points at y -28,
past the room's solid floor. navprobe: that spot is on the mesh (dz +0.001).

**Second case** (`603_26_elder-stonebark_1795181889`, 6:27.7-6:33.8): a rebirthed mage stood 6s on two
bombs (0.0 and 1.1 yd) at (2406.1, −35.54, 421.47), escape FAILED, no move, dead at 100% of a
post-rez ~17k. navprobe there: terrain 419.34, mesh 0.88 yd **above** him, a 13 yd ring all on mesh
but settling 417.2-422.9, so he was in the stream at the room's east edge. **Diagnose first:** what
`Map::CheckCollisionAndGetValidCoords` returns from a swimming start (every sweep candidate goes through
it), then for the solid-floor case make the last tier try other bearings instead of one.

## Not proposed

- The human tank's death in A2 is variance at 99 stacks. External cooldowns could cover it through
  `RaidTankDefensive`, which another session is editing.
- Holding Misdirection for the 0:10 wave: unproven.

## Verification

1. `~/.claude/scripts/pb-check <changed files>`.
2. One hard-mode pull: no bot sits in a bomb for a whole fuse without a move order.

When it ships, fold the results into `freya.md` through `/compact-docs-writer` and delete this file.
