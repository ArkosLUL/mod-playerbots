# Freya strategy - CPU cost review

## Context

The Freya behaviour is working (last change `5d9203dce`); this is a performance pass over it. The
review ranks the bottlenecks and gives each a fix plus a verdict on whether that fix can land
**without changing behaviour in any way**.

Status: items 1-11 are implemented in the working tree and uncommitted; item 12 is not. Codestyle and
a per-TU `-fsyntax-only` check pass. Nothing has been built or run, so every verdict below is still an
argument rather than a measurement - the shadow run under "Verifying any fix later" is what would
settle them.

Scope: the 19 Freya trigger nodes and their actions, `UldMultipliers_Freya`, `UldEncounter_Freya`,
and Freya's arms of the shared Ulduar multipliers. Paths are relative to `modules/mod-playerbots/`.

**This lands after four sibling reviews** (`docs/plans/{auriaya-performance,razorscale-performance,
ignis-perf-audit,xt002-perf}/`), whose fixes are uncommitted in the working tree. Items already
proposed there are referenced, not re-proposed, and this review adopts their patterns:
- the per-bot scan held by a value in `RaidUlduarValueContext` (`src/Ai/Raid/Uld/UldValueContext.h`,
  `RazorscaleScan` in `UldEncounter_Razorscale.h`) - the one per-bot store triggers, actions and
  multipliers can all reach;
- the entry-first lookup (`CollectPossibleTargetsByEntry`, `UldEncounter_Auriaya.cpp:31-58`).

Method: static analysis. Costs are operation counts, not timings - there is no profile. Unit counts
come from `env/dist/logs/botobs/603_1_elder-stonebark_1788724466.ndjson` (23 bots + 2 humans, 25-man
hard mode). `AiPlayerbot.SightDistance` is the default 100; `AC_AI_PLAYERBOT_ULDUAR_FREYA_HARD_MODE`
is **1**, so the hard-mode triggers below are live.

## How the cost is paid (verified in code)

- **Tick.** Up to 10/s per bot: `GetReactDelay` returns 100 ms with a real master
  (`PlayerbotAI.cpp:6653`), `YieldThread` adds a 0-200 ms per-bot stagger, and `MapUpdateInterval`
  is 100, so a bot lands every 100-300 ms. Skipped entirely while casting.
- **Both engines.** `ApplyInstanceStrategies` adds `ulduar` to the combat **and** non-combat engines
  (`PlayerbotAI.cpp:1796-1797`), so these triggers also run while bots walk the instance.
- **Chain pass.** Every trigger `Check()`s every tick (interval 1, `Trigger.cpp:35-51`). For each
  popped action whose `isUseful()` passes, **all 51** Ulduar multipliers run before `isPossible()`
  (`Engine.cpp:216-234`). The loop stops at the first `Execute` returning true (`:260-269`); a useful
  action that returns false pays the whole chain and the loop continues. The trace shows 1-18 passes
  in a tick - `Obliteration` popped seven damage actions, all IMPOSSIBLE, at 1:19.19.
- **Values are not cached.** Check interval 1 means `CalculatedValue::Get()` rebuilds on **every**
  read and returns the vector by value (`Value.h:71-85`).

| symbol | one read costs |
|---|---|
| **S** | `"possible targets no los"`: `Cell::VisitObjects` at 100 yd (16 cells) over every unit, `AnyUnfriendlyUnitInObjectRangeCheck` on each, `AttackersValue::IsPossibleTarget` on each hostile, vector copy. Behind `GatherFreyaWaveState` and every `GetFirstAliveUnitByEntry` |
| **N** | `"nearest npcs"`: the same visit **plus a VMAP + dynamic-tree LOS raycast per non-player unit** (`NearestUnitsValue.cpp:18`). This pull's snapshots carry up to 60 creatures (10 bombs, 10 lashers, ghouls, pets, beams, roots), so ~50 raycasts a read. Behind `GetFreyaSpores` and `GetFreyaSunBeamPositions` |
| **F** | `"find target"::"freya"`: walks the bot's threatened-by-me list, UTF-8 to wide plus lowercase per unit, and two heap allocations to build the key (`TargetValue.cpp:159-184`) |
| **C** | `CheckCollisionAndGetValidCoords`: ~15-20 spatial queries (navmesh raycast, height and liquid lookups) |

## Verdict legend

- **Exact** - same decision in every case by construction: pure predicates reordered, a value reused
  inside one call, or a cheap filter moved ahead of an expensive check whose result only mattered for
  the survivors.
- **Tick-exact** - a result read more than once in one bot's tick is computed once. Nothing a Freya
  reader depends on changes between two reads in one tick: other units do not update during a bot's
  tick (`Map.cpp:481-507`), the engine breaks on the first successful `Execute`, and no Freya action
  that returns false mutates these lists. Same guarantee as the existing `cachedAtMs` multipliers.
- **Edge** - identical except in one named window, stated in the item.
- **Not neutral** - would change behaviour. Listed for completeness, not proposed.

## What one bot pays per tick today

Engaged, hard mode, baseline (no escape running):

| role | S | N | F |
|---|---|---|---|
| ranged DPS | 3 triggers + 1 (`set dps priority` Execute) + 1 (tremor gate, ms-cached) **+1 per popped damage action** | 1 | ~13 **+1 per popped damage action** |
| melee DPS | 3-4 + 1 + 1, same per-action term | 1 | ~13 |
| healer | 3 + 1 | 1 | ~12 |
| tank | 1 + 2 (`tank adds` Execute) | 1 | ~13 |
| hunter / rogue | +2-4 (`freya redirect threat` Execute, every tick) ; +1 hunter (nature resistance) | | |

While a mechanic is live, per tick, on top: spore-seeking **+1 S +4 N**; ranged camp **+2 S and 3
camp sweeps**; lasher bail **+2 S**; bomb escape **+1 N +3 GameObject sweeps**; beam dodge **+3 N**.

## Bottlenecks, ranked

### 1. `FreyaTrioSyncMultiplier` pays F + S on every damage action

`UldMultipliers_Freya.cpp:63-83`. Runs for every popped damage action of every DPS bot, so it scales
with passes per tick, not ticks: the seven-action pass above cost 7 F + 7 S. It runs in **every
Ulduar fight**, not just Freya - the multipliers are not encounter-gated.

**Fix.** Return 1.0 unless the current target's entry is Snaplasher, Storm Lasher or Ancient Water
Spirit: `FreyaTrioSyncSuppress` returns false for anything else, and `GatherFreyaWaveState` fills
those three fields by entry. `"current target"` is a manual value, so the test is a pointer read plus
`GetEntry()`. **Exact.** Already proposed as A7 in the Auriaya review; this review confirms it and
notes it is the single biggest Freya item.

### 2. `freya dodge unstable sun beam` costs 1 N per bot per tick, all instance long

`UldTriggers_Freya.cpp:125-147`. The hard-mode config is on, so every bot pays ~50 LOS raycasts every
tick to find beams within 12 yd - during the fight, and in every between-pull stretch until Freya is
DONE, on both engines. The elder fights need it too, so it cannot be gated on Freya being live.

**Fix.** One `Cell::VisitObjects` with `AnyUnitInObjectRangeCheck(bot, sightDistance)` composed with
entry in {33170, 33050}, then `!IsPlayer() && IsWithinLOSInMap` on the matches only - the B3 pattern
from the Auriaya review. Same candidates, same order, same filters, but ~3 raycasts instead of ~50.
**Exact.**

**Do not** substitute `GetCreatureListWithEntryInGrid`: it visits the grid container only and
measures 2D, where `AnyUnitInObjectRangeCheck` is 3D.

### 3. The spore pick runs 2 N, three times a tick

`GetFreyaTargetSpore` (`UldEncounter_Freya.cpp:293-364`) reads beams and spores as two separate N,
and the trigger (`UldTriggers_Freya.cpp:104`), `isUseful` and `Execute` each call it - 6 N per tick
per ranged bot walking to a spore, about 300 raycasts.

**Fixes.** (a) Collect both entry sets in one visit (item 2's pattern): 2 N to 1. **Exact.**
(b) The scan memo (item 6) makes the three calls one. **Tick-exact.**
(c) In the trigger, test `HasAura(SPELL_POTENT_PHEROMONES)` **before** the Conservator lookup
(`:96` runs an S, `:101` the aura check). Most of the raid is already sheltered mid-wave, and the
aura test is a flat_multimap lookup. **Exact.**

### 4. Two hard-mode nodes cost 1 S per bot per tick everywhere in Ulduar

`freya ground tremor hold cast` (`UldTriggers_Freya.cpp:263-272`) and
`FreyaGroundTremorCastGateMultiplier::EvaluateWindow` (`UldMultipliers_Freya.cpp:168-177`, ms-cached)
each resolve Freya through `GetFirstAliveUnitByEntry`, which is a full S. Both run wherever the gate
is open, which is all trash before she dies.

**Fix.** The entry-first lookup (`CollectPossibleTargetsByEntry`, `UldEncounter_Auriaya.cpp:31-58`):
same visit, same check, entry tested first, `IsPossibleTarget` only on matches, no vector copy.
**Exact.** Also the natural home for the other eight `GetFirstAliveUnitByEntry` calls in Freya code.
Leave `EncounterHelpers::GetFirstAliveUnitByEntry` itself alone - it is marked for removal and every
raid uses it.

### 5. The ranged camp sweeps collision up to 108 times, three times a tick

`GetFreyaLasherCampSpot` (`UldEncounter_Freya.cpp:669-703`) walks 9 deltas x 2 signs x 6 radii and
calls C **before** the cheap clearance test, so a boxed-in bearing pays full price. The trigger,
`isUseful` and `Execute` each recompute it, for all 14 ranged and healers, on every tick where a
lasher is below 15%.

This is the one spike candidate. In the trace, 8 of the 15 map updates that ran 50 ms or more late
fall inside the two lasher-finish windows (1:16-1:46 and 5:05-5:44), 13% of the pull. Correlation
only: the world tick also carries every other map and 800-1200 random bots.

**Fixes.** (a) Skip `sign = -1` at `delta == 0`: `bearing + 0` and `bearing - 0` are bit-identical, so
the second pass re-tests six candidates the first already rejected. **Exact.**
(b) The scan memo: 3 sweeps to 1. **Tick-exact.**
Reordering clearance ahead of collision is **Not neutral** - collision clamps a candidate back along
the ray, which is exactly what the post-collision re-test exists to catch.

### 6. `GatherFreyaWaveState` is rebuilt 12 times a tick

Five triggers (`UldTriggers_Freya.cpp:161, 179, 216, 238, 258`), four actions
(`UldActions_Freya.cpp:379, 482, 866, 930`) and three multipliers (`UldMultipliers_Freya.cpp:80, 99,
116`) each build it from a fresh S. Every Freya `isUseful` also rebuilds its trigger and re-runs
`IsActive`, which `ProcessTriggers` already ran this tick.

**Fix.** A `FreyaScan` on the `RazorscaleScan` model: per-bot, held by a `freya scan` value added to
the existing `RaidUlduarValueContext` (`UldValueContext.h`), keyed on `getMSTime()`, storing **GUIDs
not pointers** so a despawn between reads drops out instead of dangling. It holds the S list, the N
list, Freya from `find target`, the camp spot and the main-tank GUID. Every Freya reader goes through
it. **Tick-exact** - and the memo must stay tick-scoped, since a queued action can be up to
`ExpireActionTime` (5 s) old when it is finally popped.

### 7. F is read ~13 times a tick before anything cheap

Thirteen triggers open with `AI_VALUE2(Unit*, "find target", "freya")`. Three of them -
`freya frost nova lashers` (`:205`), `freya trap lashers` (`:227`), `freya summon army` (`:247`) -
run F **before** their class test, so 19 of 23 bots pay a threat-list walk to learn they are not a
mage, hunter or death knight.

**Fix.** Class test first in those three. **Exact.** The rest fold into the item 6 scan.
**Tick-exact.**

### 8. The three crowd-control triggers call `CanCastSpell` before the cheap wave test

`UldTriggers_Freya.cpp:212, 234, 254`. `CanCastSpell` heap-allocates a `Spell` and runs `CheckCast`;
the wave test is a vector walk over the scan. Frost Nova and Frost Trap are off cooldown for most of
the fight, so the full check runs every tick outside lasher waves too.

**Fix.** Wave test first (after the class gate from item 7). Both are side-effect-free predicates
joined by AND. **Exact.**

### 9. `freya redirect threat` runs 2-4 S every tick for hunters and rogues

`FreyaRedirectThreatAction::GetRedirectTank` (`UldActions_Freya.cpp:559-591`) resolves Snaplasher,
Conservator and Freya through three separate `GetFirstAliveUnitByEntry` calls, and `Execute` resolves
Freya again at `:610`. The trigger is true for the whole encounter, so this runs every tick whether or
not the redirect is off cooldown.

**Fix.** One pass over the scan testing all three entries. **Exact** - same list, same alive-and-entry
test.

### 10. The escapes fetch the same hazards twice

`FreyaMoveAwayNatureBombAction::Execute` (`:129`) fetches bombs, then `GetFreyaEscapeHazards` (`:139`)
fetches them again plus the beams; the tank bomb action (`:240`, `:292`) does the same; the beam dodge
(`:647`, `:681`) fetches beams twice.

**Fix.** An overload of `GetFreyaEscapeHazards` taking the vectors the caller already has. **Exact.**

### 11. Chained escape sweeps re-test the same candidate grid

`FindNearestPositionClearOfHazards` (`src/Util/EncounterHelpers.cpp:379-453`) tests up to 240
candidates per call, and the bomb escape chains five calls (`UldActions_Freya.cpp:187-207`), the tank
bomb and beam dodge three each. Same bot position and same steps, so every call after the first
re-asks C about points it already asked about - up to 961 C in one tick for one bot when nothing
clears.

**Fix.** An optional per-call collision memo keyed on ring and angle index, living for one `Execute`.
Deterministic inputs, so **Exact**; the dynamic tree cannot move inside one call. Shared helper, so
other raids gain an optional parameter and no behaviour change. Lowest priority - it only bites in
worst-case geometry.

### 12. Small change, low value

- `FreyaGroundTremorCastGateMultiplier` copies `action->getName()` (`:141`) before its ms-cached early
  return; move the compare below it. **Exact.**
- `FreyaWaveState::LivingTrio()` heap-allocates a vector per call, and `FreyaTrioSyncSuppress` calls it
  twice; a `std::array<Unit*, 3>` removes both. **Exact.**

## Checked, not a bottleneck

- The GameObject sweeps for Nature Bombs (30 yd, 4 cells, GameObject container only).
- `CountFreyaRaidNear`, `GetFreyaRangedCampAnchor`, `GetFreyaRangedDpsRank`: short group walks.
- `GetFreyaConservatorSpore`'s 40 yd creature search.
- `HasAura` by id (flat_multimap), `GetHealthPct`, `GetExactDist2d`.
- The gate itself: the in-flight `UldEncounterGateOpenInPass` (`UldEncounterGate.cpp`) already reads
  the boss states once per trigger pass.

## Shared code Freya pays for, already proposed elsewhere

- The 51-multiplier chain's own lookups (Auriaya review A1-A10). During Freya, `UldThreatRedirectMultiplier`
  (`UldMultipliers_Shared.cpp:45-79`) returns after one S because `NPC_FREYA` heads its list, but it
  pays that S on every Misdirection or Tricks candidate.
- `UlduarBurstWindowMultiplier::EvaluateWindow` (`:105-180`): 1 S per tick, ms-cached, reading Freya's
  Attuned to Nature.
- `BossFireResistanceTrigger` / `BossNatureResistanceTrigger` (`src/Ai/Base/Trigger/BossAuraTriggers.cpp`):
  Freya registers both, so every paladin and hunter pays the boss-name lookup - an S plus a UTF-16
  lowercase per unit - every tick. Razorscale review item 6 proposes the reorder for the shared file.
- Human raid members' role checks: the in-flight `AiFactory::BeginPlayerTalentChange` /
  `EndPlayerTalentChange` caching removes the talent-map walk that `IsMainTank` triggers three times
  per bot per tick here.

## Not behaviour-neutral, leave alone

- A check interval, or a framework memo, on `"possible targets no los"` / `"nearest npcs"`: global and
  stale across ticks.
- Gating the Freya multipliers on `UldEncounterGateOpen`. Items 1 and 4 reach the same saving **Exact**,
  so the encounter gate is not needed here; on its own it would be **Edge** at best.
- Clearance before collision in the camp sweep (item 5).
- Sharing one scan across bots: `IsPossibleTarget` is per bot (visibility, threat, tap, combat state).
- Dropping or narrowing the LOS filter on `"nearest npcs"`, or fewer escape fallback tiers.
- Running `isPossible` before the multipliers in `Engine::DoNextAction`. Skipping `isPossible` for a
  vetoed action looks free, but it needs an audit of every `isPossible` for side effects first; parked.

## Side findings (not performance)

- **`pmon` cannot see the two heaviest values.** Only `UnitCalculatedValue::Get` carries a probe
  (`Value.cpp:122-149`); the generic `CalculatedValue<T>::Get` (`Value.h:71`) has none, so S and N -
  both `GuidVector` - are invisible and show up inside whichever trigger or action asked. F is a
  `UnitCalculatedValue`, so it is timed. The Razorscale review's "it times value `Calculate`" needs
  this qualifier.
- `"nearest npcs"` keeps its LOS filter, so a sun beam behind a tree trunk is silently absent from the
  hazard list the escapes route around. Behaviour, not cost.
- `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` is 12.0 in code, but `62865 Unstable Energy` is
  `EffectRadiusIndex 8` = 5 yd, which is what the traces show. The dodge fires on pools that cannot
  reach the bot.

## Confirming the ranking (before any fix)

`.playerbots pmon toggle`, `pmon reset` at the pull, `pmon` / `pmon stack` after. It times trigger
`Check()` and action `Execute`, not `isUseful` and not the multiplier loop - those only show up inside
`PlayerbotAI::UpdateAIInternal`. Read shares, not absolute times: it takes a mutex and builds a string
per call. Two baselines: one Freya pull, and a few minutes walking Ulduar trash for items 2 and 4.
RaidObs tracing is itself a confounder while a trace is open.

## Verifying any fix later

1. `python apps/codestyle/codestyle-cpp.py`, then a per-TU `-fsyntax-only` of each changed `.cpp`
   against `acore/ac-wotlk-build:master` using `/azerothcore/build/compile_commands.json`.
2. Prove identity rather than arguing it: a temporary shadow build (not committed) where each
   memoised or reordered path also computes the old answer and logs any difference. One pull with
   zero mismatch lines.
3. `pmon` before and after on the same kind of pull: call counts per trigger and per action.
4. The standing Freya behaviour checks still hold: bombs dodged and held for the fuse, spores taken,
   camp formed, trio killed in sync, Iron Roots broken, and raid damage per minute of P2 unchanged.
5. Free signal from the next trace: map updates 50 ms or more late inside the lasher-finish windows -
   8 in 69 s today.

## Files each fix would touch

| Items | Files |
|---|---|
| 1, 4, 12 | `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Freya.{h,cpp}` |
| 2, 3c, 4, 7, 8 | `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp` |
| 3, 5, 6, 9, 10, 12 | `src/Ai/Raid/Uld/Util/UldEncounter_Freya.{h,cpp}` |
| 6 (scan home) | `src/Ai/Raid/Uld/UldValueContext.h` (add `freya scan` beside `razorscale scan`) |
| 9, 10 | `src/Ai/Raid/Uld/Action/UldActions_Freya.cpp` |
| 11 | `src/Util/EncounterHelpers.{h,cpp}` (shared, optional parameter) |

Items 1-4 are the whole win for their cost; 5 and 6 are the structural ones; 7-12 are cheap and
additive. Nothing here needs the others to land first, except that 3b, 5b and 7's tail all ride on
the item 6 scan.
