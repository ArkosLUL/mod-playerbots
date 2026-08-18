# EoE — move the DK grip anchor, cut the per-tick cost, split the files, cut an upstream PR branch

## Context

Four follow-ups on the Eye of Eternity strategy (`modules/mod-playerbots/src/Ai/Raid/EoE/`, map 616,
Malygos), after the previous change pushed `MALYGOS_MAINTANK_OFFSET` from 42 to 46:

1. **The DK can no longer grip Power Sparks.** Reported in game after the tank spot moved out.
2. **Performance.** No per-tick work that can show up as lag with 25 bots.
3. **Layout.** `EoEActions.cpp` was 1706 lines; the raid should follow the SWP/Uld directory shape.
4. **Upstream PR.** The rework goes to `mod-playerbots/mod-playerbots` on a dedicated branch.

Steps 1–4 land in **one commit** on `Custom`, and that commit is not made until the user's Docker
build comes back green. The PR branch is cut from the result.

**Status: code, docs and this plan are written and uncommitted. Waiting on the user's build.**

### Why the grip failed

`POWER_SPARK_GRIP_OFFSET` was the midpoint of the melee stack (+12) and the hunter spot (−14), i.e.
**−1** — the DK parked essentially on the arena centre. Offsets are signed distances from
`MALYGOS_CENTER_POSITION` along the latched landing bearing, positive towards Malygos.

Measured, not estimated:

- Malygos: `creature_model_info.CombatReach` **20** (entry 28859, display 26752). Power Spark:
  CombatReach **0**.
- Chase stop distance is `Unit::GetMeleeRange` (`Unit.cpp:799`), used as `maxRange` at
  `TargetedMovementGenerator.cpp:224`: `20 + 1.5 (DEFAULT_COMBAT_REACH) + 4/3` = **22.83**. With the
  tank at +46, Malygos parks at **r ≈ 23.2**.
- Death Grip 49576 → `RangeIndex` 4 → **30 yd**. `Spell::CheckRange` ends in
  `IsWithinCombatRange(target, max_range)`, which adds both combat reaches → real reach **≈ 31.5**.
- A spark hands its buff over at `IsWithinDist3d(malygos, 12.0f)` (`npc_power_spark::UpdateAI`), so
  on Malygos' own bearing it is spent at **r ≈ 35.2**.
- Sparks spawn at `FourSidesPos` — the same four bearings Malygos lands on, at r 94–107 — and walk
  straight at him. One spawn in four therefore comes down his own bearing, **37 yd** from centre:
  out of grip reach, always.

Bounds on the offset `d`, and where 4.5 sits:

| quantity | at d = 4.5 | limit |
|---|---|---|
| grip reach needed against a spark at r 35.2 | 30.67 | ≤ **31.5** (0.83 margin) |
| `boss ↔ grip`, gates `IsOnPowerSparkGripDuty` | 18.67 | ≥ **18.0** (0.67 margin) |
| `grip ↔ melee stack` | 7.5 | ≤ **8.0**, the corpse pool radius |

Window is `d ∈ [3.7, 5.2]`; 4.5 is centred in it. A +5 candidate left only 0.2 yd on the duty check,
which any inward tank drift would eat.

The corpse pool is worth reaching: spell **55852** (periodic trigger, on the corpse) fires **55849**
(`EffectRadiusIndex` 14 = 8 yd, aura 79 `MOD_DAMAGE_PERCENT_DONE`, base points 49 = **+50 % damage**).
At −1 the pool was 13 yd from the stack and wasted; at +4.5 it lands on them.

### What actually cost time per tick

- **Spent sparks never left the cache.** `npc_power_spark::DamageTaken` zeroes the damage, sets
  `UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE` and despawns after
  **60 s** — the creature stays `IsAlive()` the whole time. `GetEoECreatureGuids` filters on
  `IsAlive()` only, so for a full minute after every spark: `PowerSparkTrigger` (200 ms) kept firing,
  both spark actions kept running `isUseful()`, `GetPowerSparkToKill` could return a corpse *and mask
  a live spark* (it returns only the single best-by-boss-distance), and `GetPowerSparkToSnare` could
  spend a rune on Chains of Ice against it.
- **`AI_VALUE2(Unit*, "find target", "malygos")`** at two sites. `FindTargetValue` has a **1 ms**
  cache window and walks `GetThreatenedByMeList()` doing `Utf8toWStr` + `wstrToLower` + `Utf8FitTo`
  per entry — string allocation per unit per bot per tick.
  `Uld/Util/UldEncounter_IronAssembly.h:38` already records the house ruling against this value.
- **`GetNearestPowerSpark`** read `possible targets no los` — a per-bot `sightDistance` grid sweep
  plus `PossibleTargetsValue::AcceptUnit`'s level/PvP logic — while an instance-shared spark cache
  sat right there.
- **The grip duty check asked the wrong spark.** `IsOnPowerSparkGripDuty` took the spark nearest the
  **bot**, then tested whether *that* spark was within `POWER_SPARK_GRIP_ENGAGE_RADIUS` of the **grip
  spot**. A spark beside the grip spot was ignored whenever another was marginally closer to the DK.
- **200 yd grid sweep.** `GetEoECreatureGuids` called `GetCreatureListWithEntryInGrid(..., 200.0f)`
  anchored on whichever bot refreshed it, every 300 ms per entry — ≈ 13×13 grid cells.
- **P3 roster walk.** `GetDrakeHealerGuids` walked the group and sorted two vectors, per bot per
  tick, for an answer that only depends on group composition and difficulty.

**Trap:** the spent-spark filter must be spark-specific. `creature_template.unit_flags` shows Arcane
Overload, Static Field, Surge of Power and Hover Disk are all `UNIT_FLAG_NOT_SELECTABLE` from spawn —
a global attackable filter in `GetEoECreatureGuids` would blind the P2 bubble seek, the surge dodge
and the disk boarding.

**Trap:** `Acore::AllCreaturesOfEntryInRange::operator()` (`GridNotifiers.h:1504`) always runs
`m_pObject->IsWithinDist(unit, m_fRange, false)` and measures from the object it was constructed
with. Passing `0.0f` filters everything out, and visiting cells around a different point does not
change which object the check measures from — a centre-anchored sweep needs its own check functor.

### House layout, and how the module builds

`Uld/` and `SWP/` both use `Action/` + `Trigger/` + `Util/` with per-encounter files and a data
header. The module has **no `CMakeLists.txt`** — `modules/CMakeLists.txt` calls
`CollectSourceFiles` + `CollectIncludeDirectories` on the module root, so every source subdirectory
lands on the include path: new directories need no build change and includes stay flat
(`#include "EoEData.h"`, never a relative path). A CMake re-configure is still needed to pick up the
new files, which the Docker build does anyway.

## What was done

### 1. DK grip anchor: −1 → +4.5

`POWER_SPARK_GRIP_OFFSET` is now the literal `4.5f` in `Util/EoEData.h`, with a comment carrying both
bounds and the pool rationale. `POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE` unchanged. Nothing else reads
the offset except `GetMalygosP1Layout`.

**Deliberately not done:** no range gate on the grip. `PlayerbotAI::CanCastSpell` returns true for
`SPELL_FAILED_OUT_OF_RANGE` (`PlayerbotAI.cpp:3446`), so the DK still builds and discards a Death
Grip when the chosen spark is beyond reach. Explicit decision, recorded in the raid doc.

### 2. Performance

- **Spark liveness behind one accessor.** `GetLivePowerSparks(Player*, std::vector<Unit*>&)` in
  `Util/EoEEncounter_Malygos.{h,cpp}` reads the instance cache and drops anything carrying
  `UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE`. The filter lives **there**, not in
  `GetEoECreatureGuids` — see the trap above. Consumers: `PowerSparkTrigger::IsActive`,
  `GetPowerSparkToKill`, `GetPowerSparkToSnare`, and grip selection.
- **Grip selection anchored on the grip spot.** `GetNearestPowerSpark(PlayerbotAI*)` is replaced by
  `GetNearestPowerSparkTo(PlayerbotAI*, float x, float y)`, called with the layout's grip point from
  both `IsOnPowerSparkGripDuty` and `GetPowerSparkToGrip`. With no range gate, this selection is the
  only thing bounding what the DK tries to grip.
- **Cached boss lookup.** Both `AI_VALUE2(Unit*, "find target", "malygos")` sites now call
  `GetMalygos`, and the `possible targets` fallback loop in the drake attack action is deleted —
  `GetMalygos` already falls back to a grid sweep.
  Accepted behaviour change: `FindTargetValue` only answers for units the bot already threatens, so
  a bot with no threat used to get `nullptr` and bail. `GetMalygos` answers regardless. Both actions
  only run when the strategy is live in the right phase.
- **Centre-anchored sweep at 120 yd.** `EOE_CACHE_SWEEP_RADIUS` 200 → **120**, swept from
  `MALYGOS_CENTER_POSITION` via a file-local `CreaturesOfEntryNearPoint` functor plus
  `Cell::VisitObjects(centreX, centreY, bot->GetMap(), searcher, radius)`. 120 covers the r 107.2
  spawn ring; everything else lives on the platform (r ≤ 55.5). `CreatureListSearcher` only reads
  `GetPhaseMask()` off its first argument, so passing `bot` while visiting cells around the centre is
  correct. Needs `CellImpl.h`, `GridNotifiers.h`, `GridNotifiersImpl.h`.
- **P3 healer roster cached.** `GetDrakeHealerGuids` gets a `thread_local` instance-keyed cache,
  2 s window, skipped on the no-group path. `GetDrakeFlightAndHealerRank` reads live drake energy and
  stays uncached.

**Deliberately not done:** no eviction for the `thread_local` caches. Six instance-keyed maps, one
entry per instance id per worker thread, tens of bytes each, never freed. Recorded in the raid doc.

**Deliberately not changed:** `MalygosMultiplier::GetValue` keeps resolving the phase per action —
the existing comment explains why the phase must stay out of the 500 ms snapshot, and the call is a
hash lookup plus a time compare.

### 3. Split onto the SWP/Uld shape

Pure move beyond what steps 1 and 2 changed. Old flat `EoEActions.{h,cpp}` and `EoETriggers.{h,cpp}`
are deleted; `EoEStrategy.{h,cpp}`, `EoEActionContext.h` and `EoETriggerContext.h` keep their
contents unchanged (`EoEActionContext.h` still includes `"EoEActions.h"`, now the aggregate).
`EoEMultipliers.cpp` swaps `EoETriggers.h` for `EoEData.h` + `EoEEncounter_Malygos.h`.

The resulting tree is documented in `docs/raids/eye-of-eternity.md` under **Layout**.

Decisions baked into it:

- `MalygosPositionAction` and `MalygosTargetAction` serve phases 1, 2 and 4, so they live in
  `_Shared`.
- `MalygosTrigger::getMalygos` / `::getPhase` became free `GetMalygos(Player*)` /
  `GetMalygosPhase(Player*)` in `Util/EoEEncounter_Malygos.h` — 22 call sites, all inside
  `src/Ai/Raid/EoE/`. Names follow `Uld/Util`; the `Player*` signature stays, because the EoE
  creature cache is already `Player*`-based and `GetMalygosP1Layout` has no `PlayerbotAI*` to hand.
- `MalygosP1Layout` and `GetMalygosP1Layout` stay **together**: the per-pull latch is the struct's
  contract, not an implementation detail of the getter.
- `Util/EoEData.h` is data only — no structs with behaviour, no functions. All constants, banner
  grouped, so a number can be found without knowing which phase owns it. `TWO_PI` moved there as
  `EOE_TWO_PI` and `EOE_LATCH_STALE_MS` with it; both are read from two files now.
- Header guards follow `PLAYERBOTS_EOEACTIONS_SPARKS_H` / `PLAYERBOTS_EOEENCOUNTER_MALYGOS_H`.

### 4. Docs

`docs/raids/eye-of-eternity.md`: new **Layout** section; grip bullet rewritten with +4.5 and both
bounds; the derived boss-park figure corrected to 22.8 / r 23.2 everywhere it appears (it was stated
two ways); the Cost section carries the spent-spark filter, the centre anchor and 120 yd radius, the
`GetMalygos` rename and the `find target` ruling, the healer-roster cache, and both deliberate
limits.

## Files

| File | Change |
|---|---|
| `src/Ai/Raid/EoE/Util/EoEData.h` | **new** — ids + all constants |
| `src/Ai/Raid/EoE/Util/EoEEncounter_Malygos.{h,cpp}` | **new** — creature cache, boss/phase, P1 layout, sparks, bubbles, disks |
| `src/Ai/Raid/EoE/Util/EoEEncounter_Drakes.{h,cpp}` | **new** — stack point, static fields, roster, energy, surge clock |
| `src/Ai/Raid/EoE/Action/EoEActions.h` | **new** — aggregate header |
| `src/Ai/Raid/EoE/Action/EoEActions_{Shared,Sparks,Adds,Drakes}.{h,cpp}` | **new** — action bodies by mechanic |
| `src/Ai/Raid/EoE/Trigger/EoETriggers.{h,cpp}` | **new** — trigger classes only |
| `src/Ai/Raid/EoE/EoEActions.{h,cpp}`, `EoETriggers.{h,cpp}` | **deleted** |
| `src/Ai/Raid/EoE/EoEMultipliers.{h,cpp}` | include list, `GetMalygos` / `GetMalygosPhase` |
| `docs/raids/eye-of-eternity.md` | layout, grip spot, performance notes, the two deliberate limits |

## Verification

**Static, done:**

- `python apps/codestyle/codestyle-cpp.py` reports nothing for any EoE file (both the core repo run
  and the module-local run; the failures it does print are pre-existing, in OS/SWP/Uld and core).
- No `MalygosTrigger::` references survive outside the class's own `IsActive` definition; no
  `GetNearestPowerSpark(` and no `"find target", "malygos"` anywhere.
- Nothing outside `src/Ai/Raid/EoE/` includes a moved header — the three external includes are
  `EoEStrategy.h`, `EoEActionContext.h`, `EoETriggerContext.h`, all unmoved.
- No EoE symbol collides with anything else in the module.
- `cppcheck` is not installed locally — CI covers it.

**Build:** the user runs the Docker build and returns any errors; fix and repeat until green, then
commit.

**In game**, P1, one pull per landing bearing (the layout latches per pull):

1. **Grip.** A spark spawning on Malygos' own bearing gets gripped before it reaches him. Previously
   impossible; this is the headline check.
2. **Grip duty holds.** The DK stays parked at the grip spot rather than flapping between it and the
   melee stack — flapping means `boss ↔ grip` is dipping under 18 and the tank is drifting inward.
3. **Right spark chosen.** With two sparks up, the DK grips the one near his spot, not whichever is
   nearest him.
4. **Pool lands on the raid.** Dead sparks leave the +50 % buff where the melee stack can pick it up.
5. **No corpse chasing.** After a spark dies, melee go straight back to Malygos instead of sitting on
   the corpse for a minute, and the DK does not burn Chains of Ice on it.
6. **P2/P3 unbroken by the sweep change.** Bubbles found and entered, surge dodge fires, disks
   boarded, drakes form up — all four read through the creature cache that moved anchor.
7. **Load.** With 25 bots in P1, worldserver update time no worse than before.

## Remaining: the upstream PR branch

Not started. Only after the commit lands on `Custom`.

- `upstream/test-staging` already ships an EoE strategy and all four wiring sites, so this is a
  rework, not a new feature.
- Every external symbol the EoE tree uses exists upstream **except** `CommandPetAttack` and
  `StopPet` (`src/Ai/Raid/RaidBossHelpers.cpp:385-427`, used once each in
  `Action/EoEActions_Shared.cpp`). The local `RaidBossHelpers.{h,cpp}` carry +188 lines of unrelated
  local work that must **not** ship, so those two functions are hand-copied, never checked out.
- `upstream/test-staging` has no `docs/` directory — all local docs are out of scope.
- The local `TricksOfTheTradeTargetValue` in `RogueValues.cpp` is +73 lines vs upstream. Out of scope.
- Blocking CI: `apps/codestyle/codestyle-cpp.py` and `cppcheck` (empty report required).
  clang-format is `continue-on-error`. PRs target **`test-staging`**, never `master`.
  `.suppress.cppcheck` is a single `cppcheckError` line with no path coupling, so the split cannot
  break it.

Sequence — authorised: `fetch upstream`, `commit` on `Custom`, branch creation.
**Not authorised: push** — stop at the local branch and report.

```
git fetch upstream
git checkout -b eoe-strategy-rework upstream/test-staging
git checkout Custom -- src/Ai/Raid/EoE
git rm <upstream's flat EoE files that the split replaced>
```

`checkout -- <dir>` adds the new files but does not remove the ones they replace, so upstream's flat
`EoEActions.{h,cpp}` / `EoETriggers.{h,cpp}` have to go explicitly. Then hand-add `CommandPetAttack`
and `StopPet` to `src/Ai/Raid/RaidBossHelpers.{h,cpp}`. Single squashed commit.
`git diff upstream/test-staging --stat` must show the old flat EoE files deleted, the 14 new ones
added, and `RaidBossHelpers.{h,cpp}` modified. No `docs/`, no `RogueValues.cpp`.
