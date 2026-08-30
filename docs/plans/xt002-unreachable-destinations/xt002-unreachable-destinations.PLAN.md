# XT-002: make the encounter's destinations actually reachable

## Context

Wipe analysis of the 2026-08-30 XT-002 pull, from the RaidObs trace
`env/dist/logs/botobs/603_1_xt-002-deconstructor_1788102538.ndjson` (25-man, 4 healers, wipe at
5:10, 31 deaths). Read with `tools/botobs/postmortem.py`; every number below is measured from that
file, from `navprobe` on map 603, or from source.

The previous round's fix (time-budgeted cell picking, `StopShortOf`, healer anchor) is present in
the tree and shipped. **The trace shows it never executed once.** Both of this encounter's fixed
destinations are unreachable in practice, and the code has no way to notice.

The user reported three things; all three are explained below, and none of them is the mechanic
anyone assumed.

### What actually killed the raid

| source | damage in death rewinds |
|---|---|
| Tympanic Tantrum | 328,086 |
| **Searing Light** | **251,816** |
| Static Charged (Life Sparks) | 70,288 |
| Gravity Bomb | 28,382 |
| Consumption (Void Zones) | 26,263 |

Across the whole pull, Searing Light dealt **1,529,219 damage to bystanders against 297,900 to
carriers** — 84% of it landed on people who were not carrying it, spread over 24 of the 25 raid
members. Nine melee died to it at full duration (9 ticks each) in two clusters, at 3:04–3:06 and
4:02–4:05. Gravity Bomb, the thing the last two rounds of work targeted, is a rounding error.

## Root causes

### A. The Searing Light spot is unreachable — 60 `nopath` in 78 attempts

`ULDUAR_XT002_SEARING_LIGHT_SPOT` (862.737, 12.779, 409.832) is on the navmesh, but
`PathGenerator` returns `PATHFIND_SHORTCUT|PATHFIND_NOPATH` for it from anywhere in the melee
stack. `SearchForBestPath` ([MovementActions.cpp:1848](src/Ai/Base/Actions/MovementActions.cpp#L1848))
accepts only `PATHFIND_NORMAL|PATHFIND_INCOMPLETE`, so `modified_z` stays `INVALID_HEIGHT`,
`MoveToImpl` returns `NoPath` and `MoveTo` returns false. **The bot does not move at all.**

Verified with navprobe: from (866, −12.5) and from the melee lot the same destination paths
`PATHFIND_NORMAL`; from (880/883/886/888/890, −4…−12) every one is `NOPATH`. Sweeping 34 yd around
XT at 16 headings, failures are scattered (0°, 45°, 60°, 135° fail; 30°, 90°, 120°, 150°, 180°
succeed) with the mesh 8/8 present. This is `findSmoothPath` being flaky in this room, not a wall.

Result: Searing Light carriers stood in the melee stack for the full 9 s. That is the wipe.

### B. The Gravity Bomb parking lot has never been used — 0 of 161 carrier moves

`ParkVoidZone` returned `ParkResult::None` on every tick of the fight. Classified by exact
coordinate, the 161 `MoveTo` calls issued by `xt002 debuff carrier action` were:

- **0** to a parking-lot cell
- 78 to the Searing Light spot (60 `nopath`, 14 `dup`, 3 `ok`, 1 `wait`)
- 83 to `MoveClearOf`'s ring — every one of them on a bearing that is a multiple of 45° at a
  distance that is a multiple of 3 yd, clustered at 27–30 yd (the ring's maximum). `StopShortOf`
  never fired either; its destinations would not lie on that lattice.

Reach was never the limiter: measured bot speed in the trace is 7.4–9.3 y/s, giving 55–67 yd of
budget against 28–38 yd to the nearest cell. On the first post-Heartbreak bomb there were zero Void
Zones, so the only remaining filter is the `bot->IsWithinLOS(cellX, cellY, originZ)` gate — it
rejects all 20 cells.

navprobe explains why: the Ulduar WMO ends at y ≈ −29 (a vmap surface at ~404 under the raid,
`none` from y = −30 south, checked down from z = 430 so it is an edge, not a ceiling). The raid
stands where the building geometry is; the lot is past its edge on bare terrain. Paths cross it
fine (`PATHFIND_NORMAL` to every cell tested), but a straight LOS ray clips the building's rim.
**The lot cannot be validated with a LOS check from the bot's position, and never could.**

### C. `MoveClearOf` re-picks the same unpathable point forever

It scores candidates, keeps one winner, and returns whatever `MoveTo` says. 17 of its 83 carrier
calls were `nopath`, and because the scoring is deterministic and the bot has not moved, the next
tick picks the identical failing point.

This is issue #1 exactly. Ecoterrorist took Gravity Bomb at t=135.7 and stood at (882.96, −4.26)
**without moving one yard** for the entire 9 s, re-issuing `MoveTo(902.0, 14.8)` → `nopath` every
tick. The puddle landed at (883.0, −4.3) with **25 raid members inside 20 yd**. Its next bomb froze
6.3 s at (886.70, −9.10) before the ally set shifted enough to yield a pathable point, and got out
with 2.5 s to spare — luck, not logic.

The fix pattern already exists in the codebase: `MoveNear`
([MovementActions.cpp:163-181](src/Ai/Base/Actions/MovementActions.cpp#L163-L181)) sweeps eight
angles and takes the first `MoveTo` that returns true.

### D. Nothing checks a destination against existing puddles

A Void Zone spawned at (864.6, 13.8) at t=177.4 — **2.1 yd from the fixed Searing Light spot** —
and lived 133 s, to the end of the fight. Carriers kept being sent into it: Elemena (t=257.8),
Agony (t=282.8, dead at exactly (862.7, 12.8)) and Malediction (t=310.2) all died with Consumption
damage there. That is issue #3. The anchors are not puddle-checked either.

### E. Healers chase the master; ranged stack on one pixel

`XT002TargetGuardMultiplier` suppresses generic movers only for `carryingDebuff || IsRangedDps`
([UldMultipliers.cpp:236](src/Ai/Raid/Uld/UldMultipliers.cpp#L236)). Healers were deliberately left
out so `combat formation move` could still spread them. The cost:

| role | movement records | non-combat-strategy actions |
|---|---|---|
| heal | 1051 `follow`, 314 anchor, 213 `move to loot` | 973 / 2857 (34%) |
| ranged | 119 anchor, 22 `follow` | 152 / 9014 (1.7%) |
| melee | 259 `reach melee`, 143 `set behind` | 136 / 7332 (1.9%) |

`follow` is registered only on the non-combat engine
([AiFactory.cpp:588](src/Bot/Factory/AiFactory.cpp#L588)), so every one of those 1051 records proves
the healer was out of combat — pure healers drop combat constantly. `ApplyInstanceStrategies`
([PlayerbotAI.cpp:1795-1796](src/Bot/PlayerbotAI.cpp#L1795-L1796)) adds the ulduar strategy to
**both** engines, so the multiplier does run there and a stand-down would bite.

The oscillation is visible tick by tick: `follow` → (884.8, −11.3) at `normal`, anchor →
(866, −12.5) at `combat`, `follow` → (883.4, −15.4), anchor → (866, −12.5) … a ~17 yd round trip
several times per 10 s. That is issue #2.

The mirror problem: ranged **are** suppressed, including disperse (242 `combat formation move`
vetoes), so they all walk onto the single anchor coordinate. Peak **13 living raid members within
3 yd** of (866, −12.5); four bots died on that exact point; six died within one second at t=246–247.
With Searing Light at 8 yd radius, that stack is a wipe button.

## Established facts

Verified this session — do not re-derive.

| Fact | Source |
|---|---|
| Heart died t=92.2s, so Heartbreak was active for every bomb from t=135.7 on | trace, snapshot sweep |
| Tympanic Tantrum channelled at 117–127, 177–187, 237–247, 297–307s; **no slow was active** during the t=135.7 or t=151.8 bombs | trace `cast` + `aura` |
| Only 4 Void Zones spawned all fight: (883.0,−4.3) t=144.8 with 25 raid inside 20 yd; (887.2,−38.1) t=160.8; (864.6,13.8) t=177.4; (874.0,−41.7) t=203.2 | trace |
| `MoveToImpl` is **protected** and returns `RaidObs::MoveOutcome`, distinguishing `NoPath` from `Duplicate`/`Waiting`/`AlreadyThere` | [MovementActions.h:45](src/Ai/Base/Actions/MovementActions.h#L45) |
| Rings of radius 7 and 13 around (866,−12.5,409.8) are **8/8 on mesh**, settling to 409.70–409.81 | navprobe |
| No gameobjects exist on map 603 between (850..910, −70..30), so LOS blocking is WMO/M2 only | `acore_world.gameobject` |
| `avoid aoe` stand-down works (135 vetoes); the Divine Shield / Ice Block stand-down is in place | trace `veto`, UldMultipliers.cpp:198,207 |
| `MoveAwayFromPlayerWithDebuffAction` exists and is wired for Yogg Malady and RS Baltharus/Saviana/Halion | [MovementActions.h:327](src/Ai/Base/Actions/MovementActions.h#L327) |
| `MoveToLootAction` derives from `MovementAction`, so a generic sweep catches it | [MovementActions.h:203](src/Ai/Base/Actions/MovementActions.h#L203) |
| Hodir's ring is the house pattern for deterministic slots: `HodirRingSlotPoint` + `ValidateHodirFloorPoint` + `GetHodirRingSlot`, sorted ranged-then-guid so every bot derives the same layout with no communication | [UldBossHelper.cpp:781-859](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L781-L859) |

## Decisions

Settled with the user this session.

1. **Searing Light stays carrier-only.** Fix the carrier's pathing and puddle avoidance; do not wire
   a bystander step-out. The 9-tick full-duration hits stop once carriers can actually leave.
2. **Deterministic slots for ranged and healers, and suppress every generic mover for both.** Reuse
   the Hodir ring shape rather than leaving disperse to fight the anchor for the tick.
3. **Keep the fixed points; validate them properly.** Drop the LOS gate, rank candidates, and try
   the best few until `MoveTo` is actually accepted. Add Void Zone checks to the Searing Light spot
   and the anchor slots.

## Files to change

### `src/Ai/Raid/Uld/Action/UldActions_XT002.h` / `.cpp`

**Replace the LOS gate and the ignore-the-return-value convention with a real outcome check.** The
current comments justify ignoring `MoveTo`'s result because it goes false on `dup`/`wait` while the
bot walks. That is right, but it also swallows `NoPath`. Call `MoveToImpl` instead and branch on
`RaidObs::MoveOutcome`: `Issued`/`Duplicate`/`Waiting`/`AlreadyThere` mean the cell is good and the
action owns the tick; `NoPath` means try the next candidate.

- `ParkVoidZone`: delete the `IsWithinLOS` filter (fact table above: it rejects all 20 cells for a
  reason no code change can fix). Keep the Void Zone and reach ranking, but collect the ranked cells
  into a small vector instead of one winner, then walk it and issue `MoveToImpl` until one is not
  `NoPath`. Cap the attempts (3–4) so a bad tick cannot run 20 path searches.
- `MoveClearOf`: same shape — keep the ranked candidate list rather than a single winner, try in
  order, return on the first accepted. This is what stops the frozen-in-the-raid case.
- Searing Light branch in `Execute`: reject the spot when a Void Zone is within
  `ULDUAR_XT002_VOID_ZONE_RADIUS`, and fall back through a short list of alternatives before giving
  up. Same `MoveToImpl` outcome check.
- `StopShortOf` keeps its 20 yd pull test, but must also use the outcome check so a `NoPath` stop
  point does not silently own the tick.

### `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp`

Add `GetXT002RangedSlot(botAI, bot, Position& out)` next to the existing XT-002 helpers, modelled
directly on `GetHodirRingSlot`:

- roster of living ranged DPS + healers, sorted ranged-first then guid, so every bot derives the
  same layout;
- slot 0 on the anchor, an inner ring at 7 yd, the rest on an outer ring at 13 yd (both navprobe
  verified above);
- reject a slot inside `ULDUAR_XT002_VOID_ZONE_RADIUS` of a puddle and take the next free one;
- run the result through the same floor validation `ValidateHodirFloorPoint` does
  (`GetMapWaterOrGroundLevel` + `CheckCollisionAndGetValidCoords`);
- `RaidObs::NoteDerived(bot, "xt002.slot", ...)` so the next trace shows the formation directly —
  XT-002 has no note stream today, which is why this analysis had to infer the branch taken from
  move coordinates.

New constants beside the existing block: `ULDUAR_XT002_RANGED_RING_INNER = 7.0f`,
`ULDUAR_XT002_RANGED_RING_OUTER = 13.0f`, `ULDUAR_XT002_RANGED_RING_INNER_SLOTS`.

### `src/Ai/Raid/Uld/Action/UldActions_XT002.cpp` — `XT002RaidPositionAction::Execute`

Ranged and healers move to their slot rather than to the bare anchor. Keep the existing tolerance
bands, measured against the slot. Main tank branch unchanged.

### `src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp` — `XT002RaidPositionTrigger::IsActive`

Same swap: compare against the slot, not `ULDUAR_XT002_RANGED_SPOT`. Keep the healer heal-range
stand-down — it is still correct and is what lets a healer walk out to a parked carrier.

### `src/Ai/Raid/Uld/UldMultipliers.cpp`

One line in the existing `MovementAction` sweep: add healers to the suppressed set alongside
`carryingDebuff || IsRangedDps`. With decision 2 the anchor now spreads bots itself, so the reason
healers were exempt is gone. This kills the 1051 `follow` moves and the 213 `move to loot` moves in
one go, since both are `MovementAction` subclasses and the multiplier runs on the non-combat engine.

### Docs

Update the XT-002 section of `docs/raids/ulduar.md` with the reachability findings — the WMO edge at
y ≈ −29, the flaky smooth path in this room, and the rule that a fixed destination here must be
validated by outcome rather than by LOS. Run `/compact-docs-writer` first, per the governing-doc
rule. Save this plan to `docs/plans/xt002-unreachable-destinations/` on approval.

### Not touched

`docs/engine/pitfalls.md` still tells the reader to read navprobe's `settledZ` column; this build
prints both `settledZ` and `UpdateAllowedPositionZ`. Left for the user to call.

## Verification

The module cannot be compiled in this environment; build and in-game checks are the user's.

**Static:** every `MoveToImpl` call site branches on the outcome and no XT path returns a bare
`MoveTo` result where a `NoPath` would be swallowed; the candidate loops are bounded; new constants
defined once and used; no XT-002 node shares a relevance (91, 90, 64, 63, 61, 60); brace balance on
every touched file.

**In game, hard mode on** — then re-run `postmortem.py` on the new trace and check against this
pull's numbers:

1. `xt002 debuff carrier action` issues destinations that land on exact lot cells. This pull: 0 of
   161. Anything above zero is the LOS fix working.
2. `nopath` count for that action drops from 77 of 161.
3. Searing Light bystander damage drops from 1,529,219. Carrier share should rise well above 16%.
4. No Void Zone spawns with more than a couple of raid members inside 20 yd. This pull: 25.
5. Healer `follow` records drop from 1051, and `move to loot` from 213, toward the ranged figure
   (22 and 0).
6. Peak raid members within 3 yd of the anchor drops from 13; `xt002.slot` notes show a spread ring.

**In game, hard mode off:** no Void Zones exist, so the carrier still uses the dynamic spread and
the Searing Light carrier still uses its spot; add waves, the Pummeller taunt, the Heart floor and
the leash/reach targeting gates are unchanged.
