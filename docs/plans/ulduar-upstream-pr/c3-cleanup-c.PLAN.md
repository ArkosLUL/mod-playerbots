# c3-cleanup-c

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Behaviour-neutral cleanup on `Custom`, branch `uldpr/c3-cleanup-c`. Apply the **Cleanup lanes** list of `ulduar-upstream-pr.PLAN.md` to the files below. Measurements cut from comments go to the boss doc in `docs/raids/ulduar/`. RaidObs calls stay. Any change that alters behaviour beyond the listed fixes is out of scope: log it under Decisions for review instead.

Files: every `src/Ai/Raid/Uld/**` file whose stem ends in `_Algalon` or `_YoggSaron`. Includes the four
`TeleportTo` → `NearTeleportTo` switches in `Action/UldActions_YoggSaron.cpp`. Runs only after the user said their
own Algalon/Yogg-Saron work is paused; other sessions may have committed there since the plan was written, so
start from current `Custom`.

## Task list

Not split: the lane's files already meet most Cleanup items (no `ai`, no `0.f`, no `isUseful` override, measurement
passes done earlier), so the work is a few hundred mechanical lines across 14 of the 18 files.

1. [x] Header constants `inline constexpr` in `UldEncounter_{Algalon,YoggSaron}.h` (326 lines).
2. [x] Yogg floor-fall rescue (`YoggSaronFallFromFloorAction`, 4 marks): `NearTeleportTo(...); return true;`.
3. [x] Braces off single-statement bodies (36 sites), `ObjectGuid const&` loops (2, Yogg bench).
4. [x] Stray literals named (see Decisions); `IN_MILLISECONDS` for five Yogg ms conversions.
5. [x] Comments: past-tense incident narration rewritten (about 40 sites), dashes replaced, the 3-4 line blocks
   and three orphaned comment blocks in `UldEncounter_YoggSaron.h` cut to 2 lines.
6. [x] Lines over 120 columns wrapped in lane files (10).
7. [x] Docs: `raids/ulduar/yogg-saron.md` (fall rescue uses `NearTeleportTo`), `raids/ulduar/algalon.md` (bulk star
   latch rationale). Every other measurement cut from a comment was already in the boss doc.
8. [x] Checks: syntax check over all 72 Ulduar TUs and the three context builders passes (three need the core-headers
   mount for `WorldSession::IsHeadless`, the stale-image case), codestyle passes, pblint clean on all lane files.

## Decisions for review

- **Floor-fall rescue** (`YoggSaronFallFromFloorAction`): `NearTeleportTo` to the same spots and orientations,
  then `return true`. The listed fix; a failed teleport no longer hands the tick on. No master involved, so no
  map guard.
- **Named literals**, all values unchanged:
  - Algalon: `ULDUAR_ALGALON_SWEEP_STEP`/`_ANGLE_STEP`/`_BEARINGS` (clear-spot search, far-hole standoff, hole detour,
    slot displacement), `ULDUAR_ALGALON_SHELTER_IN_RADIUS` (shelter radius less 1.5, 3 uses),
    `ULDUAR_ALGALON_AREA_TAUNT_RANGE` (Challenging Shout/Roar), set-behind spot angle in the walk guard, kite hole
    penalty in `NearestShelter`.
  - Yogg-Saron: `ULDUAR_YOGG_SARON_SEARCH_RANGE` (200 yd, 7 uses), sweep steps (3 spacing sweeps), `SANITY_LOW`
    (40), `P1_PARKED_STACK_SCALE`, `MALADY_SPREAD_RADIUS` (trigger and action), `BRAIN_PHASE_3_SHARE`,
    `BRAIN_HOLD_SEEN_MS`, `PHASE_3_STACK_RADIUS`, `BEACON_STAGERS_10/25`, the four class taunt ids as
    `SPELL_*` enum entries, set-behind spot angle in the movement guard, locals for the Brain Link crusher search and
    the sanity conservation spot.
  - Left as literals: kill-order tier and tank-healer rank numbers, Yogg phase numbers, the assist tank index bound,
    float epsilons, buffer sizes, the personal button table, the 135 degree gaze turn already described by its
    comment, the Algalon hazard circle radius in a RaidObs-only call.
- **Float order**: ring bearings are now `step * (pi / 8)` instead of `(step * pi) / 8`, and the set-behind angle
  `side * (3 * pi / 5)` instead of `((side * 3) * pi) / 5`. Both bit-identical: dividing by 8 is exact and `side`
  is plus or minus one.
- **Comments**: rewrites keep the rationale in present or conditional tense; every incident the narration cited
  (Barkskin druids, Reincarnation eater, two late hunters, 17 caught, 7 and 11 healed, 10.7k to 6.7k healing, 39 yd
  and 57%, 60% tank line, heading flips) is already in `algalon.md` or `yogg-saron.md`.
- **Braces** kept on `else` chains and around nested `if`/`for` bodies, as c1 and c2 did.
- Two long lambdas in `YoggSaronSetDpsPriorityAction::SpreadTarget` moved into named locals to fit 120 columns.

## Known gaps

- 8 lane files (Action and Trigger `.h`/`.cpp`) have no GPLv2 header; left for the PR branch.

## Blocked

None.
