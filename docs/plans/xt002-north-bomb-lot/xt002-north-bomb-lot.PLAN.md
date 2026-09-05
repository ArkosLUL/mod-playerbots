# XT-002: a second Void Zone parking lot north of the raid

## Context

The 2026-09-05 XT-002 pull was a kill — trace
`env/dist/logs/botobs/603_1_xt-002-deconstructor_1788631900.ndjson` (25-man hard mode, 8:30.7,
4 bots dead, 5 death records because Elemena logged twice a second apart). Everything from the
Heartbreak round is holding: healers targetless 1.7–5.4% against 100% before, Power Word: Shield
144 casts, Beacon of Light 8, Prayer of Mending 32, Pain Suppression 1, zero `nopath` moves in
1,533 movement records, and Void Zone Consumption down to 175,003 of 7,173,415 raid damage taken.

Every Gravity Bomb puddle now lands in a parking lot, but both lots are **south** of the raid, so a
bot in a northern formation slot walks the length of the room while a bot in a southern slot walks
23 yd. The user asked for the lots to be mirrored so a carrier goes to whichever side it is already
on. This plan mirrors the **ranged** lot only; the melee lot cannot move, for a navmesh reason
established below.

Read the trace with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`. Schema is v10:
the record type key is `e`, and `t` is milliseconds **from the pull**, not an epoch.

### What the lots cost today

17 Void Zones spawned; 15 landed within 2.9 yd of a cell, one at 16.1 yd (below), one at 0.2 yd of
an earlier puddle. Capacity was never the binding constraint — 10 of 20 ranged cells and 12 of 20
melee cells were still free at the kill. **Distance is the constraint.**

Twelve Gravity Bombs landed on ranged or healers. Walk to the nearest free south cell against the
nearer of two lots:

| | south lot only | nearer of two | change |
|---|---|---|---|
| total walked | 390 yd | 354 yd | −9% |
| worst single walk | 46.3 yd | 37.2 yd | −20% |
| carriers switching side | — | 2 of 12 | Fel at (864.01, 5.20), Tree at (869.01, 0.42) |

The average is small and the worst case is what matters. `TravelReach` is
`GetSpeed(MOVE_RUN) × (duration − 1500 ms)`, so ~50 yd at full speed but **~25 yd during Tympanic
Tantrum**, which halves movement. Five bombs landed inside a tantrum window this pull. One of them,
Fel at t=262.0, was 46.3 yd from the south lot and 23.0 yd from a north lot — the only cell set
inside reach at half speed. It made the walk (its puddle landed 2.2 yd off-cell) but it ran 50.7 yd
in a 9 s debuff, the longest run of the fight.

## Why only the ranged lot moves

The Scrapyard floor is terrain, not WMO — `navprobe point` returns terrain 409.803 with the vmap
surface far below — and it is continuously on mesh across x 800–940, y −90..+70. But a sunken WMO
roof underlies the yard and rises east: vmap reads 393 at x=865, 405 at x=890, and emerges as a
411.15 step at x≈900. Path dumps through that band drop to Z 398–406 and oscillate between two
waypoints, so `findSmoothPath` answers `PATHFIND_SHORTCUT|PATHFIND_NOPATH`.

Measured, this splits the room cleanly by **the carrier's own x**:

- From the ranged formation (x ≤ 872) every northern destination probed answers `PATHFIND_NORMAL`.
  The full 20-cell north grid below is **60/60 reachable** from the three most easterly ranged
  positions in the trace, `(875, −6.5)`, `(872, −6.5)` and `(864, 5.2)`.
- From the melee stack (x ≥ 875) everything north of y ≈ +3 answers `NOPATH`. A 48-point fan around
  `(884, −21)` at radii 20/30/40 has every heading from 180° through 315° open and the whole
  northern arc closed. **The melee lot has to stay where it is**, and it is already the shortest walk
  in the fight (15–31 yd).

## New coordinates (navprobe-verified)

**`ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_NORTH` = `(837.0746, 28.01061, 409.811)`**, grid running
`+x` and `+y`, so cells span x 837.07–861.07, y 28.01–46.01.

This is the existing south origin reflected in y about the formation centre
(`ULDUAR_XT002_RANGED_SPOT.y + ULDUAR_XT002_RANGED_RING_OFFSET_Y` = −6.5):
`2 × −6.5 − (−41.01061) = 28.01061`. The x band is unchanged, so the two lots are the same shape.

- All 20 cells on mesh, poly distance 0.03–0.18, settled Z 409.811 uniformly at the corners and
  centre probed.
- Nearest cell to the northernmost formation slot `(864, 5)` is **23.2 yd** — clear of Gravity Bomb's
  20 yd pull, and exactly the figure the south lot already achieves against `(864, −18)`. The 6 yd
  `RANGED_RING_OFFSET_Y` that was added to protect the south now buys symmetry: both edges land at
  23.2 yd instead of 16.5 south / 28.5 north.
- 50.8 yd from the Searing Light spot, so its 10 yd ring cannot reach a cell.
- Furthest cell from XT's worst measured position `(897, −23.5)` is 91.8 yd, inside
  `ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS` but with only 8 yd to spare — see below.

## Files to change

### `src/Ai/Raid/Uld/Util/UldEncounter_XT002.h` / `.cpp`

- Rename `ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED` to `..._RANGED_SOUTH` (3 use sites) and add
  `..._RANGED_NORTH` at the coordinates above.
- Add a lot descriptor, because the north grid walks `+y` where the other two walk `−y`:

  ```cpp
  struct XT002BombLot
  {
      Position origin;      // the corner cell nearest the raid
      float yDirection;     // +1 or -1: the way the grid walks away from it
  };
  ```

- Add `std::vector<XT002BombLot> GetXT002BombLots(PlayerbotAI* botAI, Player* bot)` — one lot for
  melee, two for ranged and healers. Argument order matches `GetXT002EngageableAdd` in the same file.
  Both call sites in the action currently branch on `botAI->IsMelee(bot)` themselves; this replaces
  that branch so the two cannot disagree about which grid a bot owns.
- Raise `ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS` from 100 to 110. The scan is anchored on XT, and the
  north lot's far corner sits 91.8 yd from where XT was measured at its most easterly. A puddle
  outside the scan is a cell the ranking believes is free.

### `src/Ai/Raid/Uld/Action/UldActions_XT002.cpp` / `.h`

- **`ParkVoidZone`** ([UldActions_XT002.cpp:308](src/Ai/Raid/Uld/Action/UldActions_XT002.cpp#L308)):
  wrap the cell loop in a loop over `GetXT002BombLots`, and take the y step from `lot.yDirection`.
  Add `float z` to the local `Cell` struct and pass `candidate.z` to `IssueMove` — with three grids
  the origin Z is no longer a single value. **The ranking itself does not change**: it is already
  reachable → room → clear approach → shortest walk, so extending the pool from 20 cells to 40 makes
  a ranged carrier pick the nearer lot with no new comparison, which is the behaviour asked for.
- **`InsideParkingLot`** ([UldActions_XT002.cpp:152](src/Ai/Raid/Uld/Action/UldActions_XT002.cpp#L152)):
  return true if the bot is inside **any** of its role's lots. The y extent has to be built from
  `min`/`max` of the origin and far edge rather than assuming the origin is the top.
- **`StopShortOf`** and **`ApproachIsClear`** are unchanged — both work off the chosen cell.
- Update the `ParkVoidZone` doc comment in the header, which says "the role's parking grid" singular.

### `docs/raids/ulduar/xt002.md`

- The lead-in under **"The parking lot is a time budget, not a coordinate"** (line ~153) says
  "The grid is 5×4 from a raid-facing origin, walking +x / −y … all 40 cells are navprobe-verified".
  It becomes three grids, 60 cells, and the y step is per-lot.
- Add why the ranged lot is doubled and the melee lot is not: the reachability split at the carrier's
  own x, with the 60/60 and the melee fan as the evidence.
- Extend the existing `findSmoothPath` bullet (line ~140) with the cause now that it is known — the
  sunken WMO roof under the yard and the Z 398–406 waypoints — since that bullet currently reports
  the refusals as unexplained.
- Governing-doc rule: `docs/raids/ulduar/xt002.md` is referenced from the raid docs, so run
  `/compact-docs-writer` **before** editing it, not as cleanup.

## Found while measuring, not in this plan

Left out deliberately; each is the user's call whether to spend a round on.

1. **Searing Light hurts the raid on the walk, not at the destination.** 1,030,308 total, of which
   **568,592 landed on raiders other than the carrier — 100% of it while the carrier was still
   moving, none once parked**, and 61% inside the first quarter of the run. All 24 runs passed 1–17
   raiders inside the 8 yd splash; in 23 of 24 the destination itself was clean. There is one exit
   and it is south-west, so an east-side carrier crosses the whole formation to reach it. A second
   spot at `(900, 26)` with the existing 10 yd ring, ranked by **how many raiders the run passes**
   rather than by endpoint clearance, replays at 161 → 102 crossings (−37%) for 52 extra yards walked
   across the whole fight. Ranked by *nearest*, the same second spot is taken once in 24 and saves 7%.
   `(900, 26)` is on mesh at 409.811 with 8/8 of its ring on mesh, and `(900, 36)` — the ring's 90°
   point — is the only north-east point reachable from the melee stack.
2. **One puddle landed in the melee stack.** Mighty parked on cell `(871.52, −48.04)` at t=398.1,
   was re-issued at t=401.5 (`blocked`), re-targeted across the lot to `(904.91, −43.98)`, and its
   bomb expired mid-run at `(887.02, −26.09)` — 16.1 yd off any cell, in the melee. The `parked`
   latch should have held it. One event in 23 bombs.
3. **Stale header comment.** `XT002SetDpsPriorityAction`'s comment at
   [UldActions_XT002.h:168](src/Ai/Raid/Uld/Action/UldActions_XT002.h#L168) still says healers "get
   nothing at all and have any leftover target cleared". The `.cpp` was corrected last round; the
   header was not.
4. **`docs/engine/pitfalls.md`** still tells the reader to read navprobe's `settledZ` column while
   this build prints both `settledZ` and `UpdateAllowedPositionZ`. Referenced from `CLAUDE.md`, so it
   needs its own `/compact-docs-writer` pass.

## Verification

The module cannot be compiled here; the build and the in-game pull are the user's.

**Static:** no bare `ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED` left; no `botAI->IsMelee(bot)` lot
branch left in the action; every `IssueMove` loop still bounded by
`ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS`; XT-002 node relevances still unique (91, 90, 64, 63, 61, 60);
brace balance on every touched file.

**In game, hard mode on**, then re-run `postmortem.py` and compare against this pull:

1. Void Zones appear in the north grid at all — `y ≥ 28` in the spawn list. This pull: none, y ranged
   −26.1 to −52.8.
2. Ranged and healer carriers standing north of the formation centre (y > −6.5) go north. This pull
   the two that would have switched were Fel at t=262.0 and Tree at t=484.4.
3. No carrier walks more than ~37 yd to a cell. This pull: 46.3 yd worst, and 50.7 yd actually run.
4. No puddle lands more than ~3 yd off a cell. This pull: 16 of 17 within 2.9 yd, one at 16.1.
5. Free cells at the kill rise from 10/20 ranged. Watch that the north lot is being *used*, not just
   present.
6. Nothing regresses on the fixes already in: healer targetless share stays under ~10% (this pull
   1.7–5.4%), `power word: shield` and `beacon of light` evaluations stay above 0 (579/24 this pull),
   `nopath` move count stays at 0, Void Zone Consumption stays near 175k of ~7.2M.

**In game, hard mode off:** no Void Zones spawn at all, so `ParkVoidZone` is never reached and the
carriers still use the dynamic spread and the Searing Light spot. Add waves, the Pummeller taunt, the
Heart floor and the leash/reach targeting gates are untouched.

## On approval

Copy this file to `docs/plans/xt002-north-bomb-lot/xt002-north-bomb-lot.PLAN.md` before starting, per
the plans-directory rule.
