# Freya add waves - what is still open after the 2026-10-11 pulls

## Context

Post-mortem of three 25-man hard-mode Freya pulls. Traces in `env/dist/logs/botobs/`:
`603_44_elder-stonebark_1791742873` (A1, reset at 1:27), `_1791743089` (A2, reset at 2:39.9),
`_1791743444` (K, kill 6:33). Combat log:
`chronicle-app-1:/root/.chronicle/storage/raidlogs/logs/4a60af0e-fd53-46c6-b1da-73747c09fd98`
(epoch-ms; pull t0 = 1791742874728 / 1791743091118 / 1791743446079).

The camp geometry and the three Snaplasher fixes shipped and live in
[docs/raids/ulduar/freya.md](../../raids/ulduar/freya.md). The two items below are **proposed, not
built**; each needs its own go-ahead.

## 1. Army of the Dead was missing or late

| wave | Army | lashers on ghouls at +5.5s / +10s | lasher melee + Flame Lash on raid | deaths |
|---|---|---|---|---|
| A1 1:06 | **none** | 0 / 0 | **416k** in 20s | 5 (both humans) |
| A2 1:56 | bot, cast at **+6.1s** | 0 / 8 | 102k | `Malediction` at +6.5s, 4 lashers on her |
| K 0:10 | bot, cast at +0.2s | 9 / 10 | 15.7k | 0 |
| K 3:35 | both humans, +2.1s | 7 / 10 | 0.3k | 0 |

- **A1:** `Obliteration` (the one bot DK) spent Army on Auriaya at 18:13:35, 8m45s before the wave.
- **A2:** `freya summon army` FAILED at +1.35s and landed at +6.05s; in between the DK cast Icy Touch,
  Howling Blast and Frost Strike. Army costs one rune of each kind, so the rotation kept eating them
  as they came back. Rune state is not in the trace - treat as the working hypothesis.

**Fix.** `FreyaArmyRuneReserveMultiplier`, in the shape of `FreyaLasherTrapReserveMultiplier`
([UldMultipliers_Freya.cpp](../../../src/Ai/Raid/Uld/Multiplier/UldMultipliers_Freya.cpp)): for a DK
with lashers alive and Army off cooldown, zero every action whose spell has a rune cost. Bounded by the
rune cooldown, on one bot.

**Research before building:** a second taunt sink for the pull's second lasher wave, which the bot DK
cannot cover (10 min cooldown) - in K the humans did. Candidates must pass the three checks in
`CLAUDE.md` (spellbook, DBC, trigger chains) before anything keys on them. Wave order is knowable:
each block of three holds one wave of each kind, so the third is certain.

## 2. One bomb escape returned false for a whole fuse (the kill's only death)

87 of 88 exposed bot-volleys escaped, median **0.61s**. `Power` at 6:17.2 did not: bomb at 0.0 yd,
three more inside 10, `freya move away nature bomb` FAILED on four ticks running, **no move order
issued**, not rooted or stunned by any aura - so every sweep tier and `StepOffNatureBombs` came back
empty. Bot at (2356.27, -37.18, 425.17); bombs at (2356.3,-37.2) (2353.2,-41.5) (2352.8,-46.0)
(2352.7,-45.0) (2348.9,-49.3) (2348.2,-48.7) (2373.3,-53.0). The step-off bearing points at y -28,
past the room's solid floor. **Diagnose first:** `navprobe` the ring candidates from that spot, then
make the last tier try other bearings instead of one.

## Not proposed

- The human tank's death in A2 is variance at 99 stacks. External cooldowns could cover it through
  `RaidTankDefensive`, which another session is editing.
- Holding Misdirection for the 0:10 wave: unproven.

## Verification

1. `~/.claude/scripts/pb-check <changed files>`.
2. One hard-mode pull: Army cast inside 1s of the pack spawning, lashers on ghouls at +5.5s;
   `postmortem.py <trace> --coverage freya` for the new multiplier.
3. Lasher wave clear time and raid damage a minute must not get worse.

When both ship, fold the results into `freya.md` through `/compact-docs-writer` and delete this file.
