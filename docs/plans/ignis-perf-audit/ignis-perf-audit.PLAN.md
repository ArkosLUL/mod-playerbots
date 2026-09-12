# Ignis strategy — performance audit and behaviour-preserving fixes

## Context

The Ignis strategy works in-game. The ask: find its performance bottlenecks and say, for each, whether
it can be fixed **without changing behaviour in any way**. This plan is the audit plus the fix set
that meets that bar. Everything that would only be "practically identical" is listed separately and
is **not** part of the change unless the user opts in.

**Step 0:** copy this document to
`modules/mod-playerbots/docs/plans/ignis-perf-audit/ignis-perf-audit.PLAN.md` (project planning
directory). Delete it once the work lands.

Code under audit (paths relative to `modules/mod-playerbots/src/Ai/Raid/Uld/`):
`Util/UldEncounter_Ignis.{h,cpp}`, `Trigger/UldTriggers_Ignis.cpp`, `Action/UldActions_Ignis.cpp`,
`Multiplier/UldMultipliers_Ignis.cpp`, node table in `UldStrategy.cpp:89-132`, multipliers
registered at `UldStrategy.cpp:925-929`.

---

## How the code is driven — the cost model

All from code reading; line refs are to `modules/mod-playerbots/src` unless noted.

- **Every Ignis trigger runs every engine tick.** All use the default `checkInterval = 1`, and
  `Trigger::needCheck` returns true for anything under 2 (`Bot/Engine/Trigger/Trigger.cpp:41`).
- **The Ulduar strategy is live in both engines for all of map 603**, combat and non-combat
  (`Bot/PlayerbotAI.cpp:1796-1797`).
- **Triggers are gated, multipliers are not.** `UldGatedTrigger` closes every Ignis trigger while
  another Ulduar encounter is `IN_PROGRESS` or once Ignis is `DONE`, and leaves them open between
  pulls and during the fight (`Ai/Raid/Uld/UldEncounterGate.cpp:58-83`). Multipliers have no gate:
  `Engine::DoNextAction` runs every multiplier on every popped action that passed `isUseful()`,
  every tick (`Bot/Engine/Engine.cpp:216-231`). **So the four Ignis multipliers run for the whole
  Ulduar instance, including after Ignis is dead.**
- **Every popped Ignis action re-runs its whole trigger** in `isUseful()`. This re-check is
  load-bearing: queue entries survive up to `ExpireActionTime` = 5000 ms
  (`Script/WorldThr/Queue.cpp:116`), so it cannot be dropped.
- **Creature state is stable across one bot's AI tick.** `Map::Update` runs every player (bot AI
  runs inside `Player::Update` via `OnPlayerAfterUpdate`, `Script/Playerbots.cpp:157`) before
  `UpdateNonPlayerObjects` (`src/server/game/Maps/Map.cpp:482-506`).
- **Effective config** (`docker exec ac-worldserver env`): `MapUpdate.Threads = 6`,
  `ReactDelay = 100`, `PerfMonEnabled = 0`.
- **Raid setup** (`acore_characters.group_member`): one 25-member raid, main-tank flag set, one
  human. Role checks on a bot are a strategy bitmask test (`PlayerbotAI::IsTank`,
  `Bot/PlayerbotAI.cpp:2299-2303`). **On a human they walk the whole talent map and build a
  `std::map` every call** (`AiFactory::GetPlayerSpecTab`, `Bot/Factory/AiFactory.cpp:67-142`).

### The two expensive primitives

- **`FindNearestCreature(NPC_IGNIS, 200)`** (`GetIgnis`). A 200 yd `Cell::VisitObjects`: ~7×7
  cells of 66.7 yd, octagon-trimmed by `Cell::VisitCircle`, touching every creature in them
  (`src/server/game/Grids/Cells/CellImpl.h:65-160`).
- **`GetCreatureListWithEntryInGrid(…, 200)`** (construct and patch scans). The same visit, plus one
  `std::list` heap node per match — up to 20 constructs per call.

---

## Bottlenecks, ranked

Counts per bot per engine tick, taken from the code paths. Per-call cost is not measured here; the
ranking weighs call count × work × how long the situation lasts. PerfMon confirms it (Verification).

| # | Where | Cost today | When | Fixable with zero behaviour change? |
|---|---|---|---|---|
| 1 | `IgnisTankMovementMultiplier` (`UldMultipliers_Ignis.cpp:66-83`) | Runs `IsMainTank` and `GetIgnisConstructTankIndex` (2× `GetGroupAssistTank`, each calling `IsTank` on every member, so **2 talent walks of the human**, plus a `std::vector` alloc) **before** the cheap `dynamic_cast` filter, on every popped action of every non-main-tank bot | **Whole instance, forever** | **Yes** — P2, P3 |
| 2 | Every Ignis trigger opens with `IsIgnisEngaged` → 200 yd search | 8 searches, all returning "not engaged" | Between pulls until Ignis dies | **Yes** — P1 |
| 3 | Fight, non-tank bot: triggers | ~12 searches (`IsIgnisEngaged` then `GetIgnis` again in the same trigger; `IgnisAttackBossTrigger` rebuilds and re-runs `IgnisAttackBrittleConstructTrigger`), 4 allocating list scans, 6 talent walks (3 triggers × `GetIgnisConstructTankIndex`) | Ignis fight | **Mostly** — P1, P2, P6, P7, P8 get it to ~4 searches, 3 scans (1 alloc each), 0 talent walks. Going below that needs Tier 2 |
| 4 | `IgnisDisableDefaultTargetingMultiplier`, `IgnisFlameJetsHoldCastMultiplier` | 200 yd search per popped assist action; 200 yd search per bot per ms whenever a spell action pops | **Whole instance, forever** | **Yes** — P1 |
| 5 | `IgnisMultiplier` (`:42-64`), `IgnisFlameJetsHoldCastMultiplier` (`:107`) | `Action::getName()` returns `std::string` **by value** — a heap copy (names > SSO) on every popped action; two aura lookups before the `dynamic_cast`. The Flame Jets name check is **unreachable**: `IgnisFlameJetsHoldCastAction` derives from `Action`, not `CastSpellAction`, so it already returned at `:102` | **Whole instance, forever** | **Yes** — P4, P5 |
| 6 | Fight, assist tanks: `GetIgnisConstructTankIndex` | ~4-6 calls per tick (triggers, `isUseful` re-runs, multipliers), 2 talent walks each | Ignis fight, 2 bots | **No** at Tier 1 — Tier 2 memo |
| 7 | Main tank: `GetIgnisMainTankPosition` | Up to 3× per tick (trigger, `isUseful`, `Execute`): mutex, `IsMainTank`, a 200 yd search, `GetMapWaterOrGroundLevel`, and a VMAP/dynamic-tree `CheckCollisionAndGetValidCoords` raycast | Ignis fight, 1 bot | **Partly** — P7 removes the inner search. The repeat height+raycast needs Tier 2 |
| 8 | `isUseful()` re-running whole triggers | ×2–×3 on popped Ignis actions | Ignis fight | **No** — the re-check guards 5 s-old queue entries. It gets cheap once 1-3 land |
| 9 | `IgnisAttackBrittleConstructAction` burst loop; `IgnisSlagPotHealAction` | Up to 12 `CanCastSpell(std::string)` lookups | Brittle / Slag Pot windows only | Yes, but not worth it |

---

## Phase 1 — zero behaviour change (the proposed work)

Each item is identical by construction. The argument goes in the review, not in the code comments.

**P1. `GetIgnis` rejects in O(1) through the instance script, and only searches when the search could
succeed.**

```cpp
InstanceScript* instance = bot->GetInstanceScript();
if (!instance)
    return bot->FindNearestCreature(NPC_IGNIS, R, true);      // unchanged fallback

Creature* tracked = instance->GetCreature(ULD_BOSS_IGNIS);   // UldEncounterGate.h:21, == BOSS_IGNIS
if (!tracked || !tracked->IsAlive() || !bot->IsWithinDist(tracked, R) || !bot->InSamePhase(tracked))
    return nullptr;

return bot->FindNearestCreature(NPC_IGNIS, R, true);          // exact grid-coverage semantics
```

*Why identical:* map 603 has exactly **one** Ignis spawn and no script summons entry 33118 (checked:
`acore_world.creature` count = 1; no `smart_scripts` / C++ summon). `instance_ulduar` registers him
through the base `OnCreatureCreate` / `OnCreatureRemove` (`instance_ulduar.cpp:102-105,656-658`,
`InstanceScript.cpp:75-88,320-334`). So the search can only ever return `tracked` or null. The early
returns replicate the searcher's own check (`GridNotifiers.h:1362`: alive, `IsWithinDist` with its
defaults, `InSamePhase`), so they fire only where the search would also return null. Everything else
still runs the real search, which keeps the octagon cell coverage exact. Precedent for the lookup:
`GetVezax` (`UldEncounter_Vezax.cpp:155-165`).

Same pattern for the derived checks — the predicate is evaluated on `tracked` first, and the search
runs only when it would pass:
- `IsIgnisEngaged` also rejects on `!tracked->IsInCombat()`. Between pulls this turns all 8 trigger
  searches, plus the multiplier ones, into hash lookups.
- The Flame Jets window, in `EvaluateWindow` and `IgnisFlameJetsTrigger`, rejects when `tracked`
  is not casting 62680/63472. No search outside the 2.7 s cast.

**P2. `GetIgnisConstructTankIndex` returns -1 for a non-tank before walking the group.**
`if (!botAI->IsTank(bot)) return -1;`
*Why identical:* `GetGroupAssistTank` only ever returns a member that passed `PlayerbotAI::IsTank`
(`Util/EncounterHelpers.cpp:267`) — the same function with the same defaults. A non-tank can never
equal its result. This removes every talent walk from 22 of 25 bots.

**P3. `IgnisTankMovementMultiplier`: action-type filter first.** Move the four `dynamic_cast`s above
the role check. All three predicates are pure, so the order does not change the result.

**P4. `IgnisMultiplier`: no string copy, cheap test first.**
`dynamic_cast<MovementAction*>(action) && IsIgnisSlagPotVictim(bot)` (operands swapped), and
`dynamic_cast<IgnisScorchedGroundAction*>(action)` in place of `getName() == "ignis scorched ground
action"`. *Why identical:* that name is produced only by `IgnisScorchedGroundAction`'s constructor.

**P5. `IgnisFlameJetsHoldCastMultiplier`: delete the unreachable name check** at `:105-108` (see #5).

**P6. `IgnisAttackBossTrigger`: resolve the boss once, test the current target before the nested
Brittle trigger.** `boss = GetEngagedIgnis()` → construct-tank index → `current target == boss →
false` → only then `IgnisAttackBrittleConstructTrigger`. *Why identical:* the result is
`engaged && index<0 && target!=boss && !brittle` in any order. `CurrentTargetValue::Get` is a pure
lookup (`Ai/Base/Value/CurrentTargetValue.cpp:10-20`), and nothing between the two old `GetIgnis`
calls mutates anything. A bot already on the boss skips the nested construct scan.

**P7. One lookup per trigger, cheapest predicate first.** Add `Unit* GetEngagedIgnis(PlayerbotAI*)`
(the P1 `IsIgnisEngaged` that returns the unit) and use it wherever a trigger calls `IsIgnisEngaged`
and then `GetIgnis`: molten avoid, flame jets, main tank position. Pass the resolved boss into
`GetIgnisMainTankPosition` instead of re-looking it up inside. Order pure, bot-local tests first:
`IsTank` (attack brittle), `IsHeal` (slag pot), `HasUnitState(UNIT_STATE_CASTING)` (flame jets),
construct-tank index (construct tank, which after P2 is a bitmask for non-tanks). For main tank
position, put `IsMainTank` first: with the main-tank flag set it is a 25-slot flag scan. Without the
flag it falls back to an `IsTank` walk that can hit the human, so either order is defensible — both
are identical.

**P8. Construct and patch scans into a `std::vector`.** Replace `GetCreatureListWithEntryInGrid` with
its own body — `Acore::AllCreaturesOfEntryInRange` + `Acore::CreatureListSearcher` +
`Cell::VisitObjects` (`Object.cpp:2614-2619`) — writing into a reserved `std::vector<Creature*>`.
*Why identical:* `ContainerInserter` just `push_back`s in visit order for any container
(`GridNotifiers.h:196-208`), so the element order is identical and so are ties. One allocation per
scan instead of one per match.

### Expected effect (counts per bot per tick)

For a non-tank bot (22 of 25):

| Situation | Today: searches / list scans / talent walks / string copies | After Phase 1 |
|---|---|---|
| Between pulls, Ignis alive | 8 + multipliers / 0 / 2 per popped action / 1-2 per popped action | ~0 / 0 / 0 / 0 |
| Another boss live, or Ignis dead | multipliers only (~1-2) / 0 / 2 per popped action / 1-2 per popped action | ~0 / 0 / 0 / 0 |
| Ignis fight | ~14-16 / 4-7 / 6 + 2 per popped action / 1-2 per popped action | ~4 / 3 (1 alloc each) / 0 / 0 |

The two assist tanks still pay 2 talent walks per `GetIgnisConstructTankIndex` call — only on popped
movers once P3 lands, but ~4-6 times a tick during the fight (#6).

---

## Possible, but only "identical within a tick" — not included unless asked

**Tier 2: per-bot, per-tick memo** of the engaged Ignis, the construct-tank index, one shared
construct pass (Brittle pick plus nearest Molten), the nearest lit patch, and the main-tank spot.
Keyed on (bot GUID, `getMSTime()`), the key `XT002BurstWindowMultiplier` and
`IgnisFlameJetsHoldCastMultiplier` already use. Fight cost drops to ~1 search, ~2 scans, and 1
height+raycast for the main tank; it also clears #6 and #7.

*Exact caveat:* a memo read later in a tick returns what an earlier read saw. That differs from a
fresh read only if an action in between **changed** Ignis, a construct, a patch, group roles or the
bot's position **and still returned false** (the engine stops at the first `true`). None of the
Ignis actions do that. The generic class actions were not audited for it.

**Tier 1b: mirror the core's grid coverage.** Replicate `Cell::CalculateCellArea` plus the
`VisitCircle` octagon against `ignis->GetCurrentCell()`, so P1 never needs the search at all. That
is provably identical, but it copies ~40 lines of core internals that would go silently wrong the
day AC changes cell visiting. Not recommended.

## Would change behaviour — rejected

- Raising trigger `checkInterval` — changes reaction latency.
- Shrinking the 200 yd radius — changes who can see the patches and constructs from the pools.
- Caching the main-tank spot across ticks — the collision clip depends on the bot's current
  position, and the ground height on dynamic-object floors.
- Dropping the `isUseful()` re-check — stale 5 s queue entries would execute.
- One room snapshot shared by all 25 bots — each bot's search area differs, and ties would resolve
  in a different order.
- Gating on `GetBossState(IGNIS) == IN_PROGRESS` instead of `IsInCombat()` — the two do not flip
  on the same tick at pull and evade.

---

## Side findings (not performance — not in this change unless asked)

- **Data race:** `ignisTankDrivenConstructGuid` (`UldEncounter_Ignis.cpp:37`) is an unlocked
  process-wide `static unordered_map`. With `MapUpdate.Threads = 6`, two raids on Ignis at once
  touch it from different threads, and an insert can rehash under a concurrent read. The arc-state
  map beside it got a mutex in `b5b636f40`; this one did not. Same fix, no single-raid behaviour
  change.
- **Wrong header comment:** `UldEncounter_Ignis.h:18-26` says Flame Jets makes constructs Molten,
  melee shatters them, and patches never despawn. The code, the core script and `ulduar.md` all say
  otherwise.

## Cross-cutting, out of scope

- `PlayerbotAI::IsTank/IsHeal/IsRanged` on a human recompute the talent spec every call. Every raid
  helper that loops the group pays it per human per call. A per-player spec cache would fix it
  module-wide.
- `UldGatedTrigger::Check` recomputes the gate (up to 14 `GetBossState` reads) for each of the 165
  Ulduar triggers per bot per tick. Computing it once per tick would help all of Ulduar.
- `Action::getName()` returning by value makes every name comparison in every multiplier allocate.

---

## Files to touch (Phase 1)

| File | Change |
|---|---|
| `Util/UldEncounter_Ignis.h/.cpp` | P1 (`GetIgnis`, `IsIgnisEngaged`, new `GetEngagedIgnis`, Flame Jets/Scorch predicates on `tracked` first); P2; P7 (`GetIgnisMainTankPosition` takes the boss); P8 (three list scans → vector) |
| `Trigger/UldTriggers_Ignis.cpp` | P6, P7 ordering and single lookup |
| `Action/UldActions_Ignis.cpp` | Pass the resolved boss to `GetIgnisMainTankPosition` |
| `Multiplier/UldMultipliers_Ignis.cpp` | P3, P4, P5 |

Includes: `InstanceScript.h` and `UldEncounterGate.h` (for `ULD_BOSS_IGNIS`) in the encounter file;
`GridNotifiers.h` and `CellImpl.h` for P8; `UldActions_Ignis.h` in the multiplier file for P4.

## Verification

1. **Build** — the module does not compile headless here, so the build goes to the user.
2. **Shadow check for P1 and P8** (temporary, removed before commit): compute the old and new result
   side by side, return the old one, and `LOG_ERROR` on any mismatch (GUID for `GetIgnis`; the
   element sequence for the scans). Run one full Ignis attempt plus some trash. Expect zero
   mismatches.
3. **Measure with PerfMon**, same scenario before and after: `.playerbots pmon toggle`, then
   `.playerbots pmon reset`, then play (a) 2 minutes of trash before Ignis, (b) an Ignis pull
   through two construct cycles, (c) 2 minutes of another boss after Ignis is dead. Read it with
   `.playerbots pmon` (and `.playerbots pmon tick`). The `ignis *` trigger rows should drop sharply
   in (a) and (b). Multipliers are not instrumented, so #1, #4 and #5 show only in the total — (c)
   is the scenario that isolates them.
4. **Behaviour regression:** rerun the in-game checks from `docs/raids/ulduar.md` → Ignis — tank
   arc and patch fan, the two construct loops and their pools, ranged shatter, Flame Jets hold,
   Slag Pot heals. Every change claims identical behaviour, so any difference is a bug, not a
   tuning question.
