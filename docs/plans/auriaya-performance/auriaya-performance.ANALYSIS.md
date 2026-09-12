# Auriaya performance: bottlenecks and behavior-neutral fixes

## Context

The Auriaya strategy works (commit `3ed192169`, since split per boss in `65703f240`). The user asked for a
performance investigation: a list of bottlenecks, and for each whether it can be fixed without changing
behavior in any way.

**Scope.** What one bot pays per AI tick during an engaged 25-man Auriaya pull. `RaidUlduarStrategy`
registers all 165 Ulduar triggers and ~50 multipliers for every encounter, on both the combat and
non-combat engines (`PlayerbotAI.cpp:1796-1797`), so the Auriaya pull also pays for the shared multiplier
chain. Both are covered. Generic class rotations are out of scope.

**Method.** Static analysis only - no profiler or build here. Costs are operation counts, not timings.
Unit counts come from the RaidObs trace of the 5 Sep pull, `env/dist/logs/botobs/603_1_auriaya_1788633777.ndjson`
inside `ac-worldserver` (23 bots + 2 humans). `AiPlayerbot.SightDistance` is the default 100 (no `AC_`
override).

## Cost model

- **Tick.** Up to 10 per second per bot: `GetReactDelay()` returns `ReactDelay` (100 ms) when the bot has a
  real master (`PlayerbotAI.cpp:6653`). Skipped while casting.
- **Chain pass.** Every trigger is checked every tick (interval 1). For each popped action whose
  `isUseful()` passes, `Engine::DoNextAction` runs **every** multiplier before `isPossible()`
  (`Engine.cpp:216-234`). The trace shows 1-18 passes per tick; that's a lower bound, since RaidObs
  dedups repeated verdicts.
- **Root cause.** `"possible targets no los"` and `"nearest npcs"` use check interval 1, so
  `CalculatedValue::Get()` (`Value.h:71-85`) rebuilds them on **every read** and returns the `GuidVector`
  by value. Ulduar code reads them 20-60 times per bot per tick. No module code reads either through
  `LazyGet`, so dropping a read changes nothing another reader sees.
- **World state is frozen for the length of one bot's tick.** `Map::Update` runs every player (bot AI
  runs inside `Player::Update`) before `UpdateNonPlayerObjects` (core `Map.cpp:481-507`), on one thread.
- **`isUseful()` re-checks are load-bearing.** Queue entries survive up to `AiPlayerbot.ExpireActionTime`
  (5 s), so a popped action can be ticks old. Any memo must be scoped to one tick, never across ticks.

| symbol | what one read costs | who reads it |
|---|---|---|
| **S** | `Cell::VisitObjects` at 100 yd over every unit, `AnyUnfriendlyUnitInObjectRangeCheck` on each, `AttackersValue::IsPossibleTarget` on each hostile, a vector copy, then `GetUnit` per guid | every `GetFirstAliveUnitByEntry` (`EncounterHelpers.cpp:294`): `GetAuriaya`, `GetHodir`, `IsMimironEngaged` (3x) |
| **N** | the same visit, plus a VMAP + dynamic-tree **LOS raycast per non-player unit** (`NearestUnitsValue.cpp:18`). Snapshots in this pull carry 49-67 non-player units (pets, totems, guardians, adds, pools), so each read is about 50 raycasts | `GetXT002`, `GetAlgalon`, `AuriayaSetDpsPriorityAction` |
| **G100 / G7** | `GetCreatureListWithEntryInGrid` (creature grid only, 2D) at 100 or 7 yd | Auriaya pool searches |

## Verdict legend

- **Exact**: same decision in every case by construction. Pure predicates reordered, a value reused inside
  one call (nothing can change world state mid-call: one map thread, no action runs inside `IsActive` or
  `GetValue`), or a filter applied before an expensive check whose result only mattered for survivors.
- **Tick-exact**: same unless an action that returned `false` earlier in the same tick changed the
  boss or pool state. That's the same guarantee as the existing `cachedAtMs` multipliers
  (`UlduarBurstWindowMultiplier`, `RazorscaleMultiplier`).
- **Not neutral**: don't do it.

## A. Shared Ulduar multiplier chain

These run on every chain pass, for every bot, in every Ulduar fight. During Auriaya one pass costs
**5 S + 3 N** (~150 raycasts) before anything returns 0, and every one of those multipliers returns 1.0.
This is the largest item.

| # | where | cost per pass | fix | verdict |
|---|---|---|---|---|
| A1 | `XT002TargetGuardMultiplier::GetValue` (`UldMultipliers_XT002.cpp:84`) calls `GetXT002` first | 1 N | Return 1.0 before the lookup unless the action is Misdirection/Tricks-on-MT, `AvoidAoeAction`, Divine Shield/Ice Block, `DpsAssistAction`, `TankAssistAction`, a `MovementAction` that isn't an `AttackAction`, **or** the current target's entry is `NPC_HEART_OF_DECONSTRUCTOR`. Every 0.0 path needs one of these. Optionally make `GetFirstAliveNpcByEntry` (`UldEncounter_XT002.cpp:53`) entry-first (B2) | Exact |
| A2 | `AlgalonTargetGuardMultiplier` and `AlgalonControlMovementMultiplier` (`UldMultipliers_Algalon.cpp:76, :111`) call `AlgalonEncounterActive` first, which goes through `FirstNpc` to `CollectNpcs` and builds a vector | 2 N | Room gate before the lookup: return 1.0 when the bot's 2D distance from `ULDUAR_ALGALON_ROOM_CENTER` is more than `sightDistance + 47` (his evade radius, documented in `UldEncounter_Algalon.cpp:24`) + ~20 margin for bounding radii. Algalon can't be in `"nearest npcs"` there, and `AlgalonTickEncounterState` only runs when he's found. Read `sightDistance` from config. **Don't** use an action or target pre-gate instead: inside his room it skips throttled tick calls | Exact (by his evade leash) |
| A3 | `MimironTargetGuardMultiplier::GetValue` calls `IsMimironEngaged` first | 3 S | Can't just reorder: `IsMimironEngaged` deliberately drives `TickMimironObs`, the throttled wipe-reset and trace fold. Exact options: (a) collapse the three `GetFirstAliveUnitByEntry` calls into one pass over the list (3 S to 1 S); (b) when the action can't be vetoed (anything but a non-tank `DpsAssistAction`), skip the lookups whenever `TickMimironObs`'s own throttle (`MimironObsStateFor` + `ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS`) would discard them, which means exposing that check. **Another session has uncommitted edits to `UldEncounter_Mimiron.*` (2026-09-11), so re-read it first** | Exact |
| A4 | `HodirGuardMultiplier::GetValue` (`UldMultipliers_Hodir.cpp:59`) calls `IsHodirEngaged` first | 1 S | Return 1.0 before the lookup unless: `IsHodirTauntAction(name)`, `DpsAssistAction`, `TankAssistAction`, `CastReachTargetSpellAction`, or a `MovementAction` that isn't an `AttackAction`. `GetHodir` has no side effects | Exact |
| A5 | `AuriayaMovementGuardMultiplier::GetValue` (`UldMultipliers_Auriaya.cpp:44`) calls `IsAuriayaEngaged` first. Auriaya-owned, paid in every Ulduar fight | 1 S | Compute the veto condition first: `(!IsTank && DpsAssist) \|\| ((IsMainTank \|\| IsRanged) && Movement && !Attack && !ReachTarget && !encounterMovers.count(name))`. Only call `IsAuriayaEngaged` when that's true | Exact |
| A6 | `IgnisTankMovementMultiplier::GetValue` (`UldMultipliers_Ignis.cpp:73`) runs `GetIgnisConstructTankIndex` (two `GetGroupAssistTank` group walks) before its type filter, for every non-main-tank | 2 group walks | Move the `dynamic_cast` filter above the role test | Exact |
| A7 | `FreyaTrioSyncMultiplier::GetValue` (`UldMultipliers_Freya.cpp:75`) runs `"find target"`/`freya` (a threat-list walk with a UTF-8 to wide conversion per unit) on every damage action of DPS bots | 1 threat walk | Return 1.0 unless the current target's entry is Snaplasher, Storm Lasher or Ancient Water Spirit. `FreyaTrioSyncSuppress` returns false for anything else, and `GatherFreyaWaveState` fills those fields by entry | Exact |
| A8 | `FreyaGroundTremorCastGateMultiplier::EvaluateWindow` | 1 S per tick (ms-cached), non-melee casts | Entry-first lookup (B2) | Exact, low |
| A9 | `RazorscaleMultiplier::MoversBlocked` reads `"nearest hostile npcs"` (LOS per hostile) | once per tick, generic movers only | Entry-first lookup for `UNIT_DEVOURING_FLAME` | Exact, low |
| A10 | `AiObject::getName()` returns `std::string` by value (`AiObject.h:36`) | ~10 copies per pass, heap for names over 15 chars | Take the name once per `GetValue`, after the type gates | Exact, low |

After A1-A7, a pass during Auriaya costs 0 lookups almost always (A3a leaves 1 S; A3b leaves one only for
a non-tank `DpsAssistAction`).

## B. Auriaya's own nodes

### B1. The trigger phase rescans the boss 7-9 times per tick

Per bot per tick, engaged, not in a pool (`UldTriggers_Auriaya.cpp`):

| trigger | melee | ranged / healer | main tank | off-tank |
|---|---|---|---|---|
| fall from floor (`:19`) | 1 S | 1 S | 1 S | 1 S |
| sentry taunt (`:80`) | 1 S | 1 S | 1 S | 2 S (+ `GetAuriayaLooseSentry`) |
| seeping essence (`:31`) | 1 S + G7 | 1 S + G7 | 1 S + G7 | 1 S + G7 |
| anti fear (priests and shamans only) | +1 S | +1 S | - | - |
| set dps priority (`:69`) | 1 S | 1 S | 1 S | 1 S |
| raid position (`:39`) | 3 S + G7 | 4 S + G7 + G100 | 5 S + G7 + 2 G100 | 3 S + G7 |
| **total** | **7 S** | **8 S + G100** | **9 S + 2 G100** | **8 S** |
| **after the fixes** | **1 S** | **2 S + G100** | **1 S + G100** | **2 S** |

Fixes, all **Exact** (pure predicates evaluated in one call):
- fall from floor: test `GetPositionZ() < ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT` before `AuriayaEncounterActive`.
- seeping essence: run the 7 yd `CollectAuriayaEssencePools(bot, ...)` before `AuriayaEncounterActive`, so S is only paid while standing in a pool.
- sentry taunt: `IsAssistTankOfIndex(bot, 0, true)` before `IsAuriayaEngaged`.
- set dps priority: `IsTank` before `IsAuriayaEngaged`.
- raid position:
  - Return false first when `!IsMainTank && !IsRanged`. `GetAuriayaAnchor` returns false for those roles regardless.
  - Resolve the boss once.
  - Replace the nested `AuriayaSeepingEssenceTrigger` with a direct `CollectAuriayaEssencePools(bot, ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS)`, since boss presence is already established.
  - Add `GetAuriayaAnchor` / `GetAuriayaStationIndex` overloads that take the boss and the room-wide pool list, and reuse that one list for the fouled-anchor check. It's the same call on the same boss at the same radius.

### B2. `GetAuriaya` itself costs a full S

Add a local entry-first helper and use it for `GetAuriaya` (and for A1, A8 if wanted). It runs the same
`Cell::VisitObjects(bot, searcher, sightDistance)` with the same `AnyUnfriendlyUnitInObjectRangeCheck(bot, bot, range)`,
composed with `GetEntry() == entry`. Then, in visit order, return the first match that is alive and passes
`AttackersValue::IsPossibleTarget(unit, bot, range)` (which is what `PossibleTargetsValue::AcceptUnit` does for
a creature). Same candidates, same order, no vector copy, and `IsPossibleTarget` only runs on matches.

**Exact in PvE.** `IsPossibleTarget`'s only side effect stops a player's pet attacking in a PvP-prohibited
zone. Leave `EncounterHelpers::GetFirstAliveUnitByEntry` alone: it's marked for removal and every raid uses it.

### B3. The target priority action runs a full LOS sweep every tick

`AuriayaSetDpsPriorityAction::Execute` (`UldActions_Auriaya.cpp:234`) reads `"nearest npcs"` (1 N, about 50
raycasts) every tick for every non-tank. It usually returns false because the target is already right, so
it costs the tick without ever consuming it, and it only wants three entries.

Fix: the same `Cell::VisitObjects` with `AnyUnitInObjectRangeCheck(bot, sightDistance)` composed with entry in
{Sentry, Defender, Auriaya}, then `!IsPlayer() && bot->IsWithinLOSInMap(unit)` on matches only. **Exact.**

**Don't** substitute `GetCreatureListWithEntryInGrid`. It visits the grid container only, and
`AllCreaturesOfEntryInRange` measures 2D (`GridNotifiers.h:1510`) where `AnyUnitInObjectRangeCheck` is 3D
(`:1067`).

### B4. The pool dodge can raycast 34 times per tick per bot in a pool

`AuriayaSeepingEssenceAction::Execute` (`UldActions_Auriaya.cpp:97-146`) scores the current spot, the anchor
and 32 ring points. Every candidate inside the leash pays `IsWithinLOS` **before** its score. This runs every
tick while the bot is within 7 yd of a pool, including a boxed-in bot that stays put (`bestIsCurrent`).

Fix: leash test, then score, then `found && !better` rejection, and only then LOS. LOS only ever removed
candidates that would have become the new best, so the sequence of best states is identical. **Exact.**
Also pass the boss into `GetAuriayaAnchor`, and reuse the room pool list for the main tank's station index.

### B5. The raid position action resolves the boss again

`AuriayaRaidPositionAction::Execute` (`:174`) does 1-2 S plus G100 for the main tank. Use the B1 overloads. **Exact.**

### B6. Trigger, then `isUseful`, then `Execute` each recompute

Every Auriaya action's `isUseful` rebuilds its trigger and re-runs `IsActive`, which `ProcessTriggers`
already ran this tick. `Execute` then resolves the boss, anchor and pools again. Off-anchor raid position
pays up to 3x the trigger's cost. The off-tank's `GetAuriayaLooseSentry` reads the list three times.

Only a per-tick memo removes this: a single-slot `thread_local` keyed on (bot guid, `getMSTime()`) holding
the boss, the room pool list and the station index. `Unit*` stays valid within a tick because removals are
deferred to the end of the map update. **Tick-exact**, not Exact.

## C. Negligible, no action

- ~159 closed `UldGatedTrigger` gates per tick. Each is a handful of `GetBossState` reads (`UldEncounterGate.cpp:58`).
- Anti-fear (class gate first), `UlduarBurstWindowMultiplier` (ready burst actions only, ms-cached), the 7 yd pool searches.
- The Thorim, Iron Assembly, Vezax, Kologarn and Flame Leviathan guards: a room or type gate already comes first.

## D. Not behavior-neutral, leave alone

- A check interval, or a framework memo, on `"possible targets no los"` / `"nearest npcs"`. It's global and stale across ticks.
- Gating multipliers on `UldEncounterGateOpen`. A boss in sight during another encounter would stop being guarded.
- Moving `IsMimironEngaged` behind the action test. That starves the housekeeping.
- An action or target pre-gate on the Algalon guards (see A2).
- Caching the dodge result across ticks. Ranged anchors follow the boss, and dynamic-tree LOS can change.
- Running `isPossible` before multipliers in `Engine::DoNextAction`. The Mimiron and Algalon multipliers have side effects, and vetoes are logged.

## E. Adjacent, not measured

- Between pulls nothing is IN_PROGRESS, so all 165 Ulduar triggers are open and run their own boss lookups for every bot, on trash and idle.
- Framework: `Engine::LogAction` (`Engine.cpp:672-704`) formats every engine event into `lastAction` for bots with a real master (`LogInGroupOnly` defaults to 1), even with debug logging off.

## Expected effect

Ranged DPS bot, one tick, two chain passes (priority action FAILED, then a spell OK):

| | before | Exact tier | + B6 memo |
|---|---|---|---|
| target-list rescans (S) | 21 | 5-7 | ~3 |
| full LOS sweeps (N, ~50 raycasts each) | 7 (~350 raycasts) | 0, plus one entry-filtered visit (at most 3 raycasts) | same |
| 100 yd creature searches | 1 | 1 | 1 |

Each extra chain pass costs 5 S + 3 N before, and 0-1 S after.

## Rounds

1. **Round 1: B1-B5, implemented, uncommitted** (all Exact). Touches only
   `UldEncounter_Auriaya.{h,cpp}`, `UldTriggers_Auriaya.cpp` and `UldActions_Auriaya.cpp`:
   - `CollectPossibleTargetsByEntry` (anonymous namespace, `UldEncounter_Auriaya.cpp`) backs `GetAuriaya`
     and `GetAuriayaLooseSentry`.
   - `GetAuriayaAnchor` gained a boss-taking overload with an optional room pool list, and
     `GetAuriayaStationIndex` now takes that list.
   - `AnyUnitOfEntriesInRangeCheck` (anonymous namespace, `UldActions_Auriaya.cpp`) backs the priority
     action's sweep.
2. **Deferred to a later round: all of section A**, A5 included. Suggested order when it comes: A5, A2,
   A4, A7, then A3 after re-reading `UldEncounter_Mimiron.*`. Three items are other sessions' work, so
   check what landed before touching them: A1 is B1 of `docs/plans/xt002-perf` (in progress in the tree
   as of 2026-09-11), A6 is in `docs/plans/ignis-perf-audit`, and A9 is in
   `docs/plans/razorscale-performance`.
3. **B6 is not in round 1.** It is Tick-exact, not Exact, so it needs the user's explicit OK.

Delete this doc once everything that is going to land has landed.

## Verification

- **Build.** The user builds; nothing compiles here.
- **Performance.**
  1. `.playerbots pmon toggle`, then `.playerbots pmon reset` at the pull.
  2. `.playerbots pmon` after the kill.
  3. Compare the per-call average of `PlayerbotAI::UpdateAIInternal I` and the Auriaya trigger and action rows, same roster, before and after.
  - `AC_AI_PLAYERBOT_PERF_MON_ENABLED=0` today.
  - Multiplier time only shows inside the total.
- **Behavior.**
  - Review each Exact item against the equivalence argument above.
  - In game, the Auriaya checklist:
    - burst fires on Auriaya and Vezax, and stays shut in Yogg P1;
    - sentries are targeted first;
    - pools are dodged inside the 12 yd leash;
    - the station only ever slides west.
  - Run the RaidObs trace through `python tools/botobs/postmortem.py`. It should show the same targeting order and the same shape of dodge and anchor moves.
