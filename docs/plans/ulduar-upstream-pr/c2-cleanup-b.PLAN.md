# c2-cleanup-b

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Behaviour-neutral cleanup on `Custom`, branch `uldpr/c2-cleanup-b`. Apply the **Cleanup lanes** list of `ulduar-upstream-pr.PLAN.md` to the files below. Measurements cut from comments go to the boss doc in `docs/raids/ulduar/`. RaidObs calls stay. Any change that alters behaviour beyond the listed fixes is out of scope: log it under Decisions for review instead.

Files: every `src/Ai/Raid/Uld/**` file whose stem ends in `_FlameLeviathan`, `_Freya`, `_Thorim`, `_Mimiron`.
Includes the `TeleportTo` → `NearTeleportTo` switch with the master-map guard at `Action/UldActions_Thorim.cpp`.

## Task list

Not split: the lane's files already meet most Cleanup items (no `ai`, no `0.f`, few long comments), so the
work is a few hundred mechanical lines across 26 files.

1. [x] Header constants `inline constexpr` in `UldEncounter_{FlameLeviathan,Freya,Mimiron,Thorim}.h` (276 lines).
2. [x] Thorim floor-fall rescue: `NearTeleportTo(...); return true;`, master-map guard.
3. [x] Braces off single-statement bodies (17 sites), `ObjectGuid const&` loops (FL enter-vehicle,
   Mimiron phase 1 trigger), FL rideable list `constexpr` array in an anonymous namespace.
4. [x] Stray literals named (see Decisions).
5. [x] Comments: narration, dashes, >2-line blocks, two misplaced blocks (Thorim header, Thorim trigger
   header), the `}` glued to Thorim's `TryGetThorimPhase2Spot`.
6. [x] Lines over 120 columns wrapped in lane files.
7. [x] Docs: `engine/pitfalls.md` (same-map `TeleportTo` drops combat), `raids/ulduar/thorim.md` (110 yd
   gates name `ULDUAR_THORIM_FLOOR_RANGE`, stale line numbers gone; fall rescue fact).
8. [x] Checks: syntax check over all 72 Ulduar TUs passes (three need the core-headers mount for
   `WorldSession::IsHeadless`, the stale-image case), codestyle passes, pblint 0 errors.

## Decisions for review

- **Floor-fall rescue** (`ThorimFallFromFloorAction`): `NearTeleportTo` onto the master, refused when
  `master->GetMap() != bot->GetMap()`. The listed fix; nothing else here teleports.
- **Named literals**, all values unchanged:
  - FL: Steam Rush cooldown and aim arc (3 and 2 uses), vehicle cooldowns (demolisher Ram, Mortar,
    Shield Generator, Tar, speed buffs, anti-air rocket) next to the Electroshock ones, barrel landing
    jump and minimum flight. `CastVehicle`'s default argument stays 1000.
  - Freya: bomb sweep steps (restate `FindNearestPositionClearOfHazards`' defaults, Hodir precedent),
    Iron Roots search radius.
  - Thorim: `ULDUAR_THORIM_FLOOR_RANGE` (110, two triggers), gate radius and follow distance (trigger and
    action), phase 2 spot tolerance (3 uses), camp sweep steps.
  - Mimiron: DB Target search range, stand-in search bearings/step/ring, flee fan step and max, room
    search range (rocket and cannon, 4 uses), phase 4 disperse distance (3 uses), mine minimum step,
    fire bot sidestep, hold search pad; `IN_MILLISECONDS` for three ms conversions.
  - Left as literals: `SetNextCheckDelay(100)`, float epsilons, trace bucket math, Thorim squad quotas
    and ring solver bounds, the legacy enter-vehicle seat indices and per-size vehicle counts, the
    one-yard arrival and step-in margins already described by a comment.
- **Float order**: the flee fan max is now `5 * (pi / 8)`, bit-identical since dividing by 8 is exact. The
  Mimiron stand-in turn is now `k * (pi / 6)` instead of `(k * pi) / 6`, which can differ by a float ulp
  (~1e-7 rad) in a candidate bearing.
- **Thorim gauntlet trigger**: `if (z > floor) return false; return true;` became
  `return z <= floor;`, same truth table.
- **Freya multiplier** compares against `FreyaGroundTremorHoldCastAction::Name` instead of the string.
- **Comments**: past-tense narration rewritten in all four bosses; measurements cut from comments are
  already in the boss docs (Thorim 6.25 reach, frost share, zone ticks, chain hop multipliers; FL Pursued
  never mid-channel; Freya lasher retreat). Dashes replaced. Mimiron's four-line flee parameter block
  cut to two lines.
- **isUseful/Execute reuse**: none mechanical. Every `isUseful` instantiates its trigger as the shared
  source of truth; storing the result would mean duplicating the trigger's conditions in the action.
- **Braces** kept on `else` chains and around nested `if`/`for` bodies, as c1 did.
- **Docs**: the same-map `TeleportTo` trap went to `engine/pitfalls.md` once, linked from `thorim.md`.

## Known gaps

- 16 lane files (the Action and Trigger `.h`/`.cpp`) have no GPLv2 header; left for the PR branch.
- pblint `assist-index-default`, 6 warnings (Mimiron action/trigger, Thorim triggers/encounter), all
  present on Custom: changing `ignoreDeadPlayers` changes who holds a role, so out of scope.
- `FlameLeviathanEnterVehicleAction` keeps its old lowercase comments and seat-index literals (code
  from upstream's previous Ulduar).

## Blocked

None.
