# Iron Assembly — one bot tank collects the whole council, and the raid stands in Overload

## Context

Two pulls traced on 2026-09-07, both wipes, both against **hard mode**
(`AC_AI_PLAYERBOT_ULDUAR_IRON_ASSEMBLY_HARD_MODE=1` overrides the conf's `0`, so the
Brundir → Molgeim → Steelbreaker focus order in the trace is correct, not a bug):

| | file | outcome | deaths |
|---|---|---|---|
| **A** | `603_1_stormcaller-brundir_1788810873.ndjson` | wipe at 2:33, all three bosses still up | 13 (+15 wipe cmd) |
| **B** | `603_1_stormcaller-brundir_1788811149.ndjson` | wipe at 5:08, Brundir 2:19, Molgeim 4:29 | 32 |

Both had the same raid: 25 members, **one bot tank** (Bulwark) plus two human death knights
(Felesta, Deathsong) holding group tank roles.

**Last session's Rune of Death fix is confirmed working** and its plan directory can be retired —
every checklist item is green: `stack-rune` fired 26 times in B, Rune of Death fell from 24.7% of
damage taken and 7 of 14 killing blows to **4.7% and zero**, and per-bot re-applications dropped
from 20–31 to **3–7**.

## What the trace says

### Root cause: the bot tank's assignment decides where the entire council stands

The single bot tank ends up meleed by **all three bosses** regardless of assignment — B: Steelbreaker
40 swings, Molgeim 35, Brundir 26. Unheld bosses drift onto him because he is the only real threat
generator. So his assigned spot is effectively the council's parking spot, and the two pulls differ
in exactly that one variable:

```
pull A   Bulwark assigned Steelbreaker  -> spot 11.4 yd from the raid stack
         ranged/healers within 10 yd of a boss   76.7% of samples (median 4.8 yd)
         5 of 5 Overloads covered the stack point
         13 deaths: Molgeim melee 6, Brundir Overload 5, Brundir melee 2

pull B   Bulwark assigned Brundir       -> spot 38.0 yd from the raid stack
         ranged/healers within 10 yd of a boss   10.7% of samples (median 17.7 yd)
         0 of 4 Overloads covered the stack point
         3 deaths in the first 4:45
```

Boss adherence to the designed tank spots confirms only the bot tank's boss ever gets towed:

| | Brundir | Steelbreaker | Molgeim |
|---|---|---|---|
| A (bot tank on Steelbreaker) | **0.0%** | 35.9% | 4.3% |
| B (bot tank on Brundir) | **77.4%** | 20.4% | 1.3% |

*(% of samples within 10 yd of that boss's designed spot)*

Why the assignment differed between two back-to-back pulls:
`GatherIronAssemblyTanks` ([UldEncounter_IronAssembly.cpp:176](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L176))
builds the tank list from `GetGroupMainTank` / `GetGroupAssistTank`, and neither filters on
`GET_PLAYERBOT_AI` — **human players occupy slots**. The bot's index into that list picks its boss
from `GatherIronAssemblyTankOrder` (`[brundir, steelbreaker, molgeim]`). Both helpers also require
the tank to be **alive**, so the list re-indexes when a tank dies. Bulwark landed at index 1 in A and
index 0 in B; nothing in the encounter chose that, and the bosses assigned to humans were never
isolated.

### Contributing cause: the raid spot ignores Overload

`DeriveIronAssemblyRaidSpot` ([UldEncounter_IronAssembly.cpp:549](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L549))
consults Rune of Death and nothing else. Overload is a 20 yd pulse centred on Brundir, and in pull A
**all five Overload circles covered the stack point** (9.9–15.5 yd from it), so the raid-position
action ordered everyone back in while the overload action pulled them out — the identical bug the
rune fix already solved, for a hazard that fires far more often:

```
pull A, raid inside the 20 yd circle when Overload starts:  24, 22, 24, 24, 23  of 25
pull B, same measure (Brundir isolated 40 yd out):          11,  5, 10, 10
```

### Oscillation: the movement gate locks a bot out while it is not travelling

`IsWaitingForLastMove` ([MovementActions.cpp:1037](src/Ai/Base/Actions/MovementActions.cpp#L1037))
refuses any move at the **same or lower** priority until the previous move's travel window expires.
Every competing action here uses `MOVEMENT_COMBAT`, so whichever issues first holds the slot and the
other gets `wait`. Flip-flops inside 1.5 s:

```
pull A   1303  overload  <-> raid position       pull B   1777  raid position <-> reach spell
          868  raid position <-> rune of power           800  raid position <-> rune of power
          724  overload  <-> reach melee                 514  raid position <-> rune of death
```

The visible cost is bots that accept a move and never travel — `--stalls` shows Druidica stationary
**24.3 s** at the stack while the overload action wanted it 16 yd away. Bots hit at 16–19 yd from an
Overload centre had 5.5 s of cast time and only needed ~5 yd of travel.

**Scope note:** the gate is engine-wide, not encounter-specific. This plan does not touch it —
removing the hazard from under the raid removes most of the contention, and the residual should be
re-measured before anyone changes a shared movement primitive.

### B's collapse is a different, legitimate failure

B held together for 4:45 (3 deaths), then lost 29 in 23 s. Steelbreaker was last, and
`KilledUnit` casts `SPELL_ELECTRICAL_CHARGE` on every player kill in phase 3
([boss_assembly_of_iron.cpp:305](src/server/scripts/Northrend/Ulduar/Ulduar/boss_assembly_of_iron.cpp#L305)),
so each death permanently buffs him. High Voltage 63526 per-hit tracks it exactly: 1,685 avg → 2,930
after Brundir dies → **13,248 avg, 30,992 max** after Molgeim dies. It took 24 of 32 killing blows.

High Voltage is **unavoidable by any positioning** — `EffectRadiusIndex 28 = 50,000 yd`, a
whole-instance pulse every 3 s on all 25 members. Nothing here should try to dodge it; the answer is
not to feed phase 3 the first death, which is what the two fixes above are for.

### Checked and correct — do not touch

- Hard-mode kill order (Brundir → Molgeim → Steelbreaker) matches the enabled config flag.
- `ironassembly.alive` latch, `focus`, `spot`, `slot` all read cleanly.
- Rune of Death handling (see Context) — now 4.7% of damage, zero killing blows.
- `MELEE_BOSS_RADIUS = 16` and the 18 yd spread ring: **deliberately left alone**. For a melee spot
  `D` yd from the stack the nearest spread slot is `|18 − D|` and the furthest is `D + 18`, so
  keeping a slot off the boss needs `D ≤ 10` or `D ≥ 26` while keeping every slot in caster range
  needs `D ≤ 12`. Widening breaks the ring; the assignment fix addresses the same harm without it.

## Approach

### 1. Bot tanks claim bosses from the front of the order

In `GatherIronAssemblyTanks`, enumerate **bot** tanks by walking the group directly rather than
through `GetGroupMainTank`/`GetGroupAssistTank`: keep alive members in the instance with
`GET_PLAYERBOT_AI(member)` and `PlayerbotAI::IsTank(member)`, ordered **by GUID**. Deterministic, so
every bot derives the same list; independent of where humans sit in the group roster. There is
precedent for the bot filter in
[UldActions_Razorscale.cpp:268](src/Ai/Raid/Uld/Action/UldActions_Razorscale.cpp#L268).

In `DeriveIronAssemblyAssignedBoss`, **drop the `tanks.size() < 2` gate** (replace with an
`empty()` check returning `none:nobottank`). One bot tank now takes `bosses[0]` = Brundir. That
reverses the existing comment's assumption, and the trace is why: Brundir's spot is 38 yd out,
Overload and Lightning Tendrils are the only boss abilities centred on a boss's own position, and
the lone tank tows the other two with him anyway. Update that comment to match.

Also update the Steelbreaker-empowered swap branch, which currently reads
`IsMainTank(bot) || IsAssistTankOfIndex(bot, 0)`, to "rank < 2 among bot tanks" so it stays
consistent with the new ordering.

Trade-off to state in the comment: a bot tank dying re-ranks the survivors and moves someone onto
Brundir. That is wanted — Brundir must not be left loose — but it is churn, so `ironassembly.tank`
is the probe to watch.

### 2. The raid spot steps off Overload as well as Rune of Death

Generalise `DisplaceIronAssemblyStackPointOffRunes` into a displacement that tests the stack against
both hazard sets, reusing the existing navprobe-verified candidate ring
(`RUNE_SHIFT_HEADINGS = 8`, `RUNE_SHIFT_RADIUS = 25`; re-verified this session, 8/8 on mesh,
settledZ on the floor). Runes keep `RUNE_OF_DEATH_CLEARANCE` (21); Overload uses the existing
`OVERLOAD_CLEARANCE` (25). Simulated against all nine real Overload circles:

```
pull A   all 5 resolve, travel 19.3–26.9 yd, resulting clearance 25.5–36.3
pull B   all 4 correctly need no shift (Brundir already isolated)
worst    Overload exactly on the stack -> travel 26.9, clear 26.9
```

**Latch the chosen heading per instance.** This is the one way Overload differs from a rune: a rune
is dropped and holds still, but Overload is centred on a *walking boss*, so recomputing "nearest
clear heading" every tick can flip the winner mid-cast and scatter the raid. Store the heading in
`IronAssemblyEncounterState` ([UldEncounter_IronAssembly.cpp:41](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L41)),
which is already the per-instance, thread-local place the file keeps state every bot must agree on.
Take the latch while Overload is active and release it when it ends.

`ironassembly.spot` gains an `-overload` suffix beside the existing `-rune`, so the next trace shows
which hazard moved the raid.

## Files

| Path | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp` | Bulk: bot-only `GatherIronAssemblyTanks`; one-bot-tank branch in `DeriveIronAssemblyAssignedBoss`; Overload-aware displacement + heading latch in the state struct |
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.h` | Note-key inventory — the new `spot` suffix and `tank` value. No constant changes |
| `docs/raids/ulduar.md` | Fold in the findings. Reachable from the module `CLAUDE.md`, so **invoke `/compact-docs-writer` up front**, not as cleanup |

Do not touch `MELEE_BOSS_RADIUS`, `SPREAD_RING_RADIUS`, the tank-spot bearings, or
`MovementActions.cpp`.

## Verification

Static, here: `python apps/codestyle/codestyle-cpp.py`. **The module cannot be compiled in this
environment — hand the build off rather than claiming one.**

In-game, one 25-man hard-mode pull, then against the trace:

1. `--notes ironassembly.tank` shows the bot tank on **brundir** from the pull, and no human in the
   ordering. With one bot tank it must never read `none:onetank`.
2. Brundir's adherence to his designed spot (1615.18, 121.02) is high — B managed 77.4% and that is
   the bar; A's 0.0% is the failure being fixed.
3. Ranged/healers within 10 yd of a boss falls from A's 76.7% toward B's 10.7%.
4. Overload circles no longer cover the stack point; raid-inside-at-cast-start falls from 22–24 of
   25 toward B's 5–11. `ironassembly.spot` shows `stack-overload` if one ever does.
5. `overload <-> raid position` flip-flops fall well below A's 1303, and `--stalls` no longer shows
   20 s+ stationary windows wanted by the overload action.
6. Deaths to boss **melee on non-tanks** (A: Molgeim 6, Brundir 2) go to roughly zero.

If the raid still collapses in Steelbreaker phase 3 once 1–6 are green, that is the Electrical
Charge spiral, not a positioning bug — the lever there is raid damage and the first death, and it
needs its own investigation.

## Step 0

Copy this document to `docs/plans/iron-assembly-tank-separation/iron-assembly-tank-separation.PLAN.md`
before starting. Also delete `docs/plans/iron-assembly-rune-of-death/` — its exit condition is met
(see Context) — folding anything durable into `docs/raids/ulduar.md` in the same
`/compact-docs-writer` pass.
