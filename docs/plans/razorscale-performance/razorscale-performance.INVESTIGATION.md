# Razorscale strategy — CPU cost review

## Context

The Razorscale behaviour is working; this is a performance pass over it. The review ranks the
bottlenecks and gives each one a fix plus a verdict on whether that fix can land **without changing
behaviour in any way**.

Status: items 1-5 are implemented (see "What landed" below) but not yet run in game. Items 6-11 are
still open, and the engine-wide and shared-code items remain unproposed.

Scope:
- the 11 Razorscale trigger nodes and their actions;
- `RazorscaleMultiplier` and `RazorscaleBossHelper`;
- the Razorscale arms of the shared Ulduar multipliers.

Engine-wide costs are listed separately and not proposed. All paths are relative to
`modules/mod-playerbots/`.

## What landed for items 1-5

One per-bot cache, `RazorscaleScan` (`src/Ai/Raid/Uld/Util/UldEncounter_Razorscale.h`), now answers
the four repeated lookups once per tick each: the boss, `"nearest hostile npcs"`,
`"possible targets no los"`, and the nearest harpoon per entry. It is keyed on `getMSTime()` and holds
guids. It lives in the `"razorscale scan"` value, created per bot by `RaidUlduarValueContext`
(`src/Ai/Raid/Uld/UldValueContext.h`, registered in `src/Bot/Engine/BuildSharedValueContexts.cpp`), so
triggers, actions and the multiplier all reach the same one.

| Item | Change |
|---|---|
| 1, 3 | Every Razorscale trigger and action reads the boss and the hostile list off the scan. |
| 2 | `RazorscaleMultiplier` returns 1.0 unless `UldEncounterIsLive(ULD_BOSS_RAZORSCALE)`. |
| 4 | `RazorscaleScan::NearestHarpoons` does one grid visit for all four entries and replays the core's own `NearestGameObjectEntryInObjectRangeCheck` per entry. Trigger and `isUseful` test for ranged dps (`IsRazorscaleHarpoonCrew`) before any harpoon lookup; the trigger does it after `UpdateBossAI()`, keeping that side effect. `GetRazorscaleClosestReadyHarpoon` is now the single harpoon pick. |
| 5 | `GetRazorscaleAddKillTarget` picks Sentinel, Watcher and Guardian in one pass over one sweep. |

Verified: the changed translation units pass a per-TU syntax check in `acore/ac-wotlk-build:master`.
No link, and no in-game run yet.

## How the cost is paid (verified in code)

- **Tick rate.** With a real-player master, `GetReactDelay` returns 100 ms, and `YieldThread` adds a
  0–200 ms stagger per bot. That gives each bot 3–10 ticks/s, so a 25-man raid runs about
  **80–240 bot-ticks per second**.
- **Both engines.** `ApplyInstanceStrategies` adds `ulduar` to both the **combat and non-combat**
  engines (`src/Bot/PlayerbotAI.cpp:1796-1797`). The Ulduar triggers and multipliers therefore run
  while bots walk the instance, not only during fights.
- **Triggers.** Every trigger node `Check()`s every tick (default interval 1,
  `src/Bot/Engine/Trigger/Trigger.cpp:12-51`).
  - The Razorscale triggers are wrapped in `UldGatedTrigger` (`src/Ai/Raid/Uld/UldEncounterGate.cpp`).
  - The gate is closed while another encounter is IN_PROGRESS or Razorscale is DONE. It is open
    during her fight and in every between-pull stretch before she dies.
- **Multipliers.**
  - They run only on a popped action whose `isUseful()` is true, and stop at the first `Execute`
    that succeeds (`src/Bot/Engine/Engine.cpp:216-268`).
  - Ulduar registers 51 multipliers.
  - An action that is useful but returns false pays the whole chain every tick, and then the loop
    continues. The pet order does this, and so does the harpoon on every bot but the closest.
- **Values are not cached.** A value with the default interval recomputes on **every** `Get()`, and
  `Get()` returns a copy (`src/Bot/Engine/Value/Value.h:71-85`).
  - `"nearest hostile npcs"` sweeps 100 yd (16 cells) over creatures and players, then runs an
    `IsWithinLOSInMap` raycast **per hostile NPC** (`src/Ai/Base/Value/NearestUnitsValue.cpp:10-23`,
    `NearestNpcsValue.cpp:23-39`).
  - `"possible targets no los"` does the same 16-cell sweep, plus `IsPossibleTarget` per unfriendly
    unit.
  - `"find target"` walks the bot's threatened-by-me list and converts each name UTF-8→UTF-16 and
    lowercases it. Building the key `"find target::razorscale"` costs another two heap allocations
    (`src/Ai/Base/Value/TargetValue.cpp:159-184`).
- **Grid sweeps.**
  - Cells are 66.67 yd, so a 100 yd radius covers 16 cells and 200 yd covers 37 (`CellImpl.h`,
    `VisitCircle`).
  - Instances load all grids (`Map::OnCreateMap`), so every cell in range is walked.
- **Fire patch facts (Spell.dbc).**
  - 63236 is a trigger-missile (effect 32) into 63308. 63308 summons NPC 34188 (effect 28) for
    **22 s**.
  - Razorscale is the summoner, so every patch lands in her `BossAI::summons`. `_JustDied` and her
    `EnterEvadeMode` both call `summons.DespawnAll()` in the same call that sets DONE or NOT_STARTED.
  - She recasts every 6–12 s, so 2–3 patches are usually alive.

## Equivalence verdicts

- **Strict** — pure refactor. Same inputs, same decision, every tick.
- **Same-tick** — a result read more than once in one bot's tick is computed once and shared. Nothing
  a Razorscale reader depends on can change between two reads in one tick:
  - other units don't update during a bot's tick;
  - the engine loop breaks on the first successful `Execute`;
  - the only action that has a side effect and returns false ahead of these readers is the pet order,
    and it can't change any of these lists.
- **Edge** — identical except for one named window, stated in the item.
- **No** — would change behaviour. Listed for completeness, not proposed.

## Bottlenecks, ranked

### 1. `"nearest hostile npcs"` swept 4–9× per bot per tick in the fight

**Callers.**
- Three triggers sweep on every tick: `razorscale avoid devouring flames`, `razorscale avoid sentinel`
  and `razorscale avoid whirlwind`. Patches live 22 s, so the flames trigger is true for almost the
  whole fight.
- The dodge's `Scan()` sweeps again.
- Sentinel `isUseful` + `Execute` sweep again for ranged, and whirlwind `isUseful` + `Execute` for
  non-tanks.
- `RazorscaleMultiplier` sweeps again whenever a generic mover is picked.

**Cost.** Each call repeats the sweep and every LOS raycast. At 80–240 bot-ticks per second that is
several hundred to about two thousand sweeps with raycasts per second, which makes it the heaviest
item in the fight.

**Fix.** Keep one snapshot per bot per tick: the boss pointer plus the NHN GUID list.
- Fill it on the first read of the tick, keyed on `getMSTime()`, the way
  `RazorscaleAvoidDevouringFlameAction::Scan()` already does. Every Razorscale reader walks it.
- Store it in a per-bot value in a new `RaidUlduarValueContext`, registered in
  `src/Bot/Engine/BuildSharedValueContexts.cpp`. `UnderbogMushroomsValue` in
  `src/Ai/Dungeon/UB/UBValueContext.h` uses the same pattern.
- Values are per bot and only touched on that bot's map thread, so it needs no lock.

This turns 4–9 sweeps into 1. **Same-tick.**

### 2. `RazorscaleMultiplier` is not gated on the encounter, so it runs for the whole Ulduar run

**Cost.**
- Its only gate is `bot->GetMapId() != ULDUAR_MAP_ID`.
- `MoversBlocked()` then runs `FindDevouringFlameNear` once per tick (memoised per ms), which is one
  NHN sweep with raycasts.
- Melee also pay a creature grid search around themselves, of radius 7 + distance to target.

**Who pays.** Every bot whose winning candidate is a generic mover. Ulduar is in the non-combat engine
too, so that includes **every follow step between pulls**, and it keeps running after she is dead.

**Fix.** Return `1.0f` first thing unless `UldEncounterIsLive(botAI, ULD_BOSS_RAZORSCALE)`, a
`GetBossState` read, ahead of the six `dynamic_cast`s. The fire patch facts above make this safe:
- no patch exists before she is IN_PROGRESS (her first cast is 9 s after engage);
- none survives the call that ends the encounter.

**Edge.** Despawned patches leave the grid at the end of that map update. In the single update where
she dies or resets, a bot updated after her could still have been held by one of them. Nothing needs
holding at that point.

### 3. `"find target" razorscale` called 10–16× per bot per tick

**Callers.**
- Every Razorscale trigger. The harpoon trigger calls it twice, the second time through
  `UpdateBossAI()`.
- Most actions.
- The triggers also run between pulls until she dies; there they walk a trash or empty list and
  return null.

**Fix.** Fold it into the #1 snapshot. **Same-tick.**

Not proposed: `instance->GetCreature(BOSS_RAZORSCALE)`. **No:** `find target` only resolves her once
this bot is on her threat list, and several triggers depend on that.

### 4. Harpoon: up to 12 GameObject sweeps of 200 yd (37 cells) per bot per tick

**Where the sweeps happen.**
- The trigger (`src/Ai/Raid/Uld/Trigger/UldTriggers_Razorscale.cpp:141-176`) does 4 sweeps, one per
  entry. That is **every bot, every tick of the fight**, even with no harpoon built.
- `isUseful` repeats them for every bot that reaches relevance 30.
- `Execute` repeats them for every ranged DPS. All but the closest return false and the tick
  continues.

**Fixes.**
- **a.** In the trigger, bail out for non-ranged-DPS *after* `UpdateBossAI()` and before the sweeps.
  `isUseful` rejects them anyway. `UpdateBossAI()` still runs its role-assignment side effect, and the
  `isUseful` call this replaces is a no-op in the same tick. **Strict.**
- **b.** In `isUseful`, run the role test before the sweeps. **Strict.**
- **c.** Do one sweep per tick and share it across the trigger, `isUseful` and `Execute`.
  **Same-tick.**
- **d.** Visit the grid once for all four entries, keeping the nearest per entry. It must reproduce
  `NearestGameObjectEntryInObjectRangeCheck` exactly: entry, phase, 3D `IsWithinDistInMap`, and
  spawned-only. **Strict.**

Not proposed:
- Caching harpoon GUIDs across ticks. **No:** `npc_expedition_commander` summons and removes them
  mid-fight (`BuildHarpoon` / `DestroyHarpoons`), so a cache would delay seeing a fresh one.
- Shrinking the 200 yd radius. **No.**

### 5. The pet order runs every tick and never consumes it

**What it runs.** Every tick, for every pet owner:
1. `isUseful` (one find target);
2. the full 51-multiplier chain and `isPossible`;
3. `Execute`: another find target, plus 1–3 `"possible targets no los"` sweeps while she is airborne
   (lowest-health Sentinel, then first Watcher, then first Guardian, each its own sweep);
4. `CommandPetAttack`, which does nothing when the pet is already on target.

**Fix.** Pick the add in one pass over one sweep. This is **Strict**: `GetFirstAliveUnitByEntry`
walks the same list with the same alive-and-entry test. Sharing that sweep with #7 through the
snapshot is **Same-tick**.

Not proposed:
- Throttling. **No:** the pet retargets later.
- Returning false from `isUseful` when the order would be a no-op. That skips the multiplier chain,
  but it is only strict if no multiplier has a side effect on an action it doesn't match. All 51 need
  auditing first; parked.

### 6. `razorscale fire resistance trigger` (paladins, shared code)

**Cost.** `BossFireResistanceTrigger::IsActive` does the expensive work in this order:
1. The boss lookup: a `"possible targets no los"` sweep plus a UTF-16 lowercase of every unit name.
2. `HasAura` by name, which loops 316 aura types with string compares.
3. When the aura is missing, building a `PaladinFireResistanceStrategy` on the stack, which
   heap-allocates 11 creators.

**Who pays.** Every paladin, every tick, both in the fight and between pulls until she dies. Only the
first alive paladin can ever get true.

**Fix.** Evaluate the cheap clauses first (spell known, raid group, first alive paladin, aura already
up) and the boss lookup last. The clauses are side-effect-free and ANDed together. **Strict.**

This lives in the shared `src/Ai/Base/Trigger/BossAuraTriggers.cpp`, which also serves the other
Ulduar aura triggers (7 paladin, 3 hunter) and other raids.

### 7. Kill-target marker (mechanic-tracker bot only)

**Cost.** While she is airborne, `GetRazorscaleKillTarget` runs every tick: one find target plus 1–3
PTNL sweeps. When the skull has to move, `isUseful` rebuilds the trigger and repeats all of it.

**Fix.** The same single-pass pick plus the snapshot. **Strict / Same-tick.** Low priority, since only
one bot pays.

### 8. Threat-redirect multiplier (shared, `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Shared.cpp:46-81`)

**Cost.** For a Misdirection or Tricks candidate it makes 8 separate `GetFirstAliveUnitByEntry` calls,
each a PTNL sweep. Razorscale's is one of the eight, and this runs anywhere in Ulduar.

**Fix.** One sweep that tests membership for all eight entries. **Strict.**

### 9. Role lookups

**Where.**
- The flame-breath trigger runs `IsMainTank` + `IsAssistTankOfIndex` before its cheap cone test, for
  every bot in the ground phase.
- The grounded `isUseful` (off-tanks) and the fuse-armor trigger/`isUseful` (tanks) call
  `IsMainTank(member)` for each group member. Each of those calls walks the member slots again.
- The dodge `Scan()` calls `IsMainTank` every tick.

**Fix.** Do the cone test first, and resolve the main-tank GUID once per call. **Strict.** Low.

### 10. Burst-window fallback (shared, `UldMultipliers_Shared.cpp:145-146`)

**Cost.** In every non-Razorscale Ulduar fight, whenever a burst cooldown is the popped candidate and
she isn't in the 100 yd sweep, `EvaluateWindow` calls `find target` (memoised per ms).

**Fix.** Only fall back while `UldEncounterIsLive(ULD_BOSS_RAZORSCALE)`. **Strict:**
- she only resolves while she has this bot on her threat list;
- `_JustEngagedWith` sets IN_PROGRESS;
- death runs `CombatStop` → `CombatReference::EndCombat` → `ClearThreat` on both sides before
  `_JustDied` sets DONE.

Low.

### 11. Small allocations

- The flying-alone trigger copies `"attackers"` and fills a `vector<Unit*>` just to test it for
  emptiness. Return early instead.
- Three `isUseful()`s construct a Trigger on the stack, which allocates its name, just to reuse
  `IsActive`. Call a shared predicate instead.
- `GuidVector x = AI_VALUE(...)` copies the vector.

**Strict.** Negligible on their own, and mostly absorbed by #1.

### Checked, not a bottleneck

- **`StepClearOfFlames`:** up to 9 collision raycasts, but only while a patch is actually on the bot.
  It already collects the patches once per dodge.
- **`UldGatedTrigger` gate:** one `GetBossState` per trigger, up to 14 when nothing is engaged. These
  are array reads.
- **`IsHarpoonReady`, `IsMechanicTrackerBot`, `CommandPetAttack`, `StopPet`:** O(1) or a short group
  walk.

## Outside this strategy (noted, not proposed)

- **Every default-interval value recomputes on each `Get()`.** A same-tick memo inside
  `CalculatedValue` would save more than everything above combined. It changes semantics for every
  strategy, though (for example a `Set()` followed by a `Get()` in the same tick), so it needs its own
  review.
- **Engine string and logging work.** The engine builds `getName()` copies for PerfMonitor arguments
  even with the monitor off (`Engine.cpp:250, 530`). `LogAction` runs `vsnprintf` for every verdict
  when a real-player master is present.

## Side findings (not performance)

- **Unlocked statics.** `RazorscaleBossHelper::_harpoonCooldowns` and `_lastRoleSwapTime` are
  process-wide statics with no lock. With `AC_MAP_UPDATE_THREADS=6`, two instances fighting
  Razorscale at once would race on them.
- **Stale boss doc.** `docs/raids/ulduar/razorscale.md` says the dodge "holds the tick without
  moving". That hold now lives in `RazorscaleMultiplier`, and `isUseful` only checks for a patch
  within the clear radius.
- **Inaccurate engine doc.** `docs/engine/raid-mechanics-lessons.md:181` says multipliers run once per
  queued action per tick. They actually run per popped useful action, until one executes
  (`Engine.cpp:216-268`). `pitfalls.md` references this doc, so editing it needs the
  `/compact-docs-writer` pass.

## Confirming the ranking (before any fix)

Use the worldserver's built-in performance monitor:

| Command | Effect |
|---|---|
| `.playerbots pmon toggle` | turns the monitor on |
| `.playerbots pmon reset` | clears the counters |
| `.playerbots pmon` / `.playerbots pmon stack` | prints the results |

What it measures:
- It times trigger `Check()`, action `Execute`, and value `Calculate`. In stack mode, values are
  nested under the trigger or action that asked for them.
- It does **not** time `isUseful` or multipliers. Those only show up inside
  `PlayerbotAI::UpdateAIInternal`.
- It takes a mutex and builds a string per call, so read the shares rather than the absolute times.

Baseline runs:
- **One Razorscale pull**, with the counters reset at the pull. `nearest hostile npcs` under the three
  Razorscale triggers should lead the value table.
- **A few minutes walking Ulduar trash**, for #2 and #6.

## Verifying any fix later

- Run the per-TU syntax check in `acore/ac-wotlk-build:master` on each changed `.cpp`.
- Compare pmon before and after on the same kind of pull: call counts per trigger and per value.
- Repeat the standing Razorscale behaviour checks:
  - **Air phase:** pets on adds, skull on adds, no burst.
  - **Harpoon:** the closest ranged DPS fires it.
  - **Devouring Flame:** bots step out to 7 yd or more and hold.
  - **Perma-ground:** the whole raid is on her.
  - **Walking trash:** bots still follow normally (#2).

## Files each fix would touch

| Items | Files |
|---|---|
| 1, 3, 4, 5, 7, 9, 11 | `src/Ai/Raid/Uld/Trigger/UldTriggers_Razorscale.cpp`, `src/Ai/Raid/Uld/Action/UldActions_Razorscale.{h,cpp}`, `src/Ai/Raid/Uld/Util/UldEncounter_Razorscale.{h,cpp}` |
| 1 (snapshot home) | new `src/Ai/Raid/Uld/UldValueContext.h`, `src/Bot/Engine/BuildSharedValueContexts.cpp` |
| 2 | `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Razorscale.{h,cpp}` |
| 6 | `src/Ai/Base/Trigger/BossAuraTriggers.cpp` (shared) |
| 8, 10 | `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Shared.cpp` (shared) |
