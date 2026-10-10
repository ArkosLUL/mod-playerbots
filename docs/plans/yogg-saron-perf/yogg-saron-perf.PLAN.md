# Yogg-Saron strategy: performance work that changes no behaviour

## Context

The hot paths of the Yogg-Saron bot strategy, and the optimizations that leave behaviour untouched.
The list comes from reading the code and counting sweeps per bot per tick; **nothing was measured**,
so the ranking is by counted work, not by time. Nothing here is implemented. Each item is independent
and can ship alone.

Files: `src/Ai/Raid/Uld/` — `Util/UldEncounter_YoggSaron.{h,cpp}` (**enc**),
`Trigger/UldTriggers_YoggSaron.cpp` (**trig**), `Action/UldActions_YoggSaron.{h,cpp}` (**act**),
`Multiplier/UldMultipliers_YoggSaron.cpp`, `Definition/UldDefinition_YoggSaron.cpp` (**def**);
framework `src/Ai/Raid/RaidEncounter.cpp`. Core files are named from the server root:
`instance_ulduar.cpp` and `boss_yoggsaron.cpp` under `src/server/scripts/Northrend/Ulduar/Ulduar/`,
the rest under `src/server/game/`. Line numbers are as of 2026-10-10.

## Where the work goes

One bot tick is `Strategy::OnTick` → all triggers checked → actions popped by relevance, each through
`isUseful` → every multiplier → `Execute` (`Engine.cpp:211-338`).

- **The pass cache only covers trigger checks.** `YoggSaronPassReads` (enc:295) is keyed on
  `EncounterTriggerPassId`, which is 0 in `YoggSaronTick`, in rule predicates, in multipliers and in
  actions. There every `YoggSaronNearestCreature` is a live `FindNearestCreature(entry, 200)`.
- **`YoggSaronPhase` (enc:405) is behind almost everything** and costs 2 such sweeps in phase 1 (Yogg
  miss, Sara; 3 during the handover, when Yogg is up with no Brain yet), 1 in phase 2, 2 in phase 3.
  A 200 yd sweep walks 37 grid cells.
- Live phase reads per bot per tick, outside the cache: the handover tick (enc:1267, every bot, 1-2
  sweeps), the roar tick (paladins, 2-3), the dps resolver (act:1013, 792, 691, 1071 — up to 3 reads,
  so 4-6 sweeps in phase 3), and every rule predicate that reaches it (`YoggSaronEncounterActive` on
  each dps-assist pop, the AoE hold on each AoE action).
- **The gate is open whenever no other boss is engaged and Yogg is alive**, so all 50 Yogg triggers
  run for every bot anywhere in Ulduar between pulls. `z >= 300` is true in most of Ulduar, so the
  "platform only" tests do not gate that.

## The list

**A** is identical by construction. **B** is identical given one game-data fact, named in the item,
that has to be checked before it ships.

### 1. Find Sara, Yogg-Saron and the Brain through the instance script (B, largest)

- **Where:** `YoggSaronNearestCreature` (enc:384-403), both `FindNearestCreature(entry, 200.0f, true)`
  calls, and the raw one in `YoggSaronBrainSpot` (enc:722).
- **Change:** the core already keeps their guids. `instance_ulduar.cpp:116,132,133` maps 33288 →
  `BOSS_YOGGSARON` (12), 33134 → `DATA_SARA` (760), 33890 → `DATA_BRAIN_OF_YOGG_SARON` (761);
  `InstanceScript::OnCreatureCreate/Remove` (`InstanceScript.cpp:75-88, 320-334`) writes and erases
  them, and `Creature::AddToWorld` calls it for summons too. `bot->GetInstanceScript()->GetCreature(type)`
  is a `std::map` find plus one hash lookup. Port the two `DATA_` ids beside `UlduarEncounterId` in
  `UldEncounterGate.h`.
- **Why identical:** the sweep returns the nearest creature with the entry that is alive,
  `bot->IsWithinDist(c, 200)` and `bot->InSamePhase(c)` (`GridNotifiers.h:1354-1368`). The fact it
  rests on: each entry has one creature per instance (Sara is the DB spawn; `boss_yoggsaron.cpp:791`
  and `:950` summon the other two once per attempt), so "nearest" is "the one". Three cases:
  - nothing registered → return null, no sweep;
  - registered and `|dx| + |dy| < 200` → return it iff the three tests above hold. Every cell
    `Cell::VisitCircle` cuts from the 7×7 block (`CellImpl.h:116-162`) is 2+3, 3+2 or 3+3 cells off,
    so each point in it is more than 3 × 66.67 yd away in `|dx| + |dy|`; inside that bound the sweep
    always visits the creature's cell;
  - otherwise (deep corners of an illusion room) → run today's sweep.
- **Saves:** every phase-read sweep for bots on the platform, in the brain room and in most of each
  illusion room, in all four contexts. Also closes item 10 of
  [ulduar-perf-followups](../ulduar-perf-followups/ulduar-perf-followups.PLAN.md) (the Lunatic Gaze
  trigger sweeping 200 yd for every bot in Ulduar) with no boss-state gate.
- **Check first:** `creature` has no second spawn of the three ids on map 603; type 12 is not reused
  in `gameobjectData`.

### 2. Skip the target-exclusion sweep when no candidate is a Guardian of Yogg-Saron (A)

- **Where:** `AppendYoggSaronTargetExclusions` (enc:3116) ← `RaidUlduarStrategy::AppendTargetExclusions`
  (`UldStrategy.cpp:18`) ← `GatherStrategyTargetExclusions` (`TargetValue.cpp:17`) ←
  `TargetValue::FindTarget`, both `AttackerWithoutAuraTargetValue` calcs, `WarlockValues.cpp:49`.
- **Today:** every "dps target" read and every "attacker without aura" value (one per DoT a class
  tracks) runs a 69.3 yd Guardian sweep with no gate — on every boss and trash pull in Ulduar.
- **Change:** pass the caller's `attackers` list down (`GatherStrategyTargetExclusions(botAI, type,
  attackers)` → `Strategy::AppendTargetExclusions(exclusions, type, candidates)`; `Strategy.h:76`, the
  UB and MgT overrides take the new parameter and ignore it). Return before the sweep unless a
  candidate guid has entry 33136; skip the gather when `attackers` is empty.
- **Why identical:** this function only ever adds Guardian guids, and all four callers use the set
  only to drop candidates. Re-reading the "attackers" value inside instead is **not** identical: it
  has a 1 s refresh and can differ from the caller's copy.

### 3. Test the phase before the hazard sweeps (A)

The "phase read last, it is three 200 yd sweeps" comments predate the boss-state test and the pass
cache. Inside a trigger the phase is cached, and with no fight it is one boss-state read, yet these
run their sweeps first. Each is a swap of two side-effect-free operands of `&&` / `||`:

| Site | Sweeps that run first | Wasted |
|---|---|---|
| `YoggSaronPhase2SpacingTrigger` (trig:268) | death ray, crush wedges, crusher reach, nova threats | all of phase 1; every bot in Ulduar between pulls |
| `YoggSaronPhase1SpacingTrigger` (trig:206) | cloud, nova threats | phases 2-3 on the platform |
| `YoggSaronGuardianPositioningTrigger` (trig:145) | sight-range Guardian search | melee outside the leash, phases 2-3 |
| `YoggSaronGuardianToHold` (def:28) | two 70 yd Guardian lists | each tank-assist pop in phase 1 |
| `YoggSaronNovaHold` (enc:2912) | 69 yd Guardian list, then `\|\| InPhase1` | phase 1 (swap so the phase short-circuits) |
| `YoggSaronTankLayOnHandsTrigger` (trig:1035) | group walk + `CanCastSpell` | paladins outside phase 3 |
| `YoggSaronPhase3LustTrigger` (trig:781) | two `CanCastSpell` + group walk | shamans outside phase 3 (add `IsPhase3()` after the Sated test; `YoggSaronPhase3LustReady` already returns false there before it writes) |
| `YoggSaronPhase3ControlTrigger` (trig:384) | `IsDesignatedBotTank`, up to two group walks | tanks outside phase 3 |
| `YoggSaronSanityConservationTrigger` (trig:904) | `IsBotMainTank` (a role lookup; a group walk for tanks) before the Sanity aura read | every tick, anywhere |

Trace notes sit after the conjunction in all of them, and `GuardianPositioning` writes its `returning`
latch from the same boolean.

### 4. Read the Immortal Guardians without a raycast for every NPC (A)

- **Where:** `YoggSaronGuardianLoose` (enc:1468), `YoggSaronGuardianControlTrigger` (trig:889),
  `ControlPhaseThree` (act:1948). Each reads `"nearest npcs"`, which recomputes on every `Get()`: a
  sight-range visit plus a line-of-sight ray for every non-player unit (pets, totems, tentacles), then
  keeps only entries 33988/36064. The trigger reads it twice in one `IsActive`.
- **Change:** one helper in enc using the entry-first check the resolver already has
  (`AnyUnitOfEntriesInRangeCheck`, act:51), then the same `IsWithinLOSInMap` filter; the trigger
  passes its list into `YoggSaronGuardianLoose`.
- **Why identical:** `NearestNpcsValue` is `AnyUnitInObjectRangeCheck(bot, sightDistance)` →
  `!IsPlayer() && IsWithinLOSInMap` (`NearestUnitsValue.cpp:10-23`); filtering by entry before the ray
  keeps the same Guardians in the same order, and all three readers drop everything else.
- **Saves:** per tank per tick in phase 3, three reads of ~50 rays become two of at most ~10.

### 5. Evaluate each resolver candidate once (A)

- **Where:** `YoggSaronSetDpsPriorityAction::ResolveTarget` / `SpreadTarget` (act:785-993).
  `IsAllowedTarget` ends in a line-of-sight ray (act:782) and runs twice for the current target every
  tick in the steady state (act:850 then 876; 962 then 977; and again at 850 for every tentacle
  `SpreadTarget` rejected).
- **Change:** memo `(Unit*, allowed)` in `BrainApproach` (act h:236), which already lives for one
  `ResolveTarget` call.
- **Why identical:** nothing between the two evaluations changes the world; the function only writes
  the memo fields.
- **Saves:** one ray plus the threat and aura reads per dps bot per tick.

### 6. One "does any rule care" lookup per action per boss (A, framework)

- **Where:** `EncounterRuleMultiplier` / `EncounterGatedMultiplier` (`RaidEncounter.cpp:143-232`).
  Ulduar installs 139 of them (38 Yogg's); each owns an `unordered_map<Action*, bool>` and does a find
  for every action that passes `isUseful`. Nearly every answer is "no".
- **Change:** one shared index per bot per definition, `Action* → uint64` mask of the rules that care
  (Yogg 38, Algalon 26), with a one-entry last-action memo; built in
  `EncounterDefinition::AddMultipliers` and held by `shared_ptr`. Each rule stays its own `Multiplier`
  and tests its bit.
- **Why identical:** `RuleCares` and the family mask depend only on the action and are cached per
  action today; names, order, veto credit and perf buckets are untouched.
- **Saves:** 139 hash finds per popped action become 14, on every Ulduar boss. Not Yogg-only.

### 7. Answer once per trigger pass what triggers ask twice (A)

- `YoggSaronPortalPlan` (enc:1087): asked by the enter-portal trigger, up to three times by the Sanity
  trigger and by Brain Link; each phase 2 call is a 100 yd portal search plus state writes, and a
  second portal list when a wave is up. Memo `{intent, spot}` in `YoggSaronPassReads`. A repeat inside
  a pass rewrites nothing: the wave debounce is in seconds and the slot rebuild is keyed on the wave.
- The Sanity trigger body runs twice a pass in phase 3: `YoggSaronPhase3PositioningTrigger` builds a
  `YoggSaronSanityTrigger` and calls it (trig:809). Memo the node's answer. The walk latch is then
  asked once a pass, not twice; the two asks differ only if the clock ticks a millisecond between them.
- `YoggSaronRoomStateOf` (enc:544-546) sweeps the room for tentacles, then
  `YoggSaronBrainRoomApproachable` sweeps it again. Split the door test out and call that. Outside a
  pass this is two grid visits a call.

### 8. Cache the three illusion door guids per instance (B)

- **Where:** `YoggSaronNearestDoor` (enc:362): up to three 200 yd game-object sweeps per
  `YoggSaronBrainRoomApproachable` call outside a pass (resolver, room state), for the brain team.
- **Change:** store each door's guid in `YoggSaronEncounterState` on the first sweep that finds it;
  then resolve by guid and apply the sweep's own test, with the same `|dx| + |dy| < 200` rule and the
  same fallback as item 1. A miss is never cached.
- **Assumption:** one spawn each of 194635-194637 on map 603 (check `gameobject`).

### 9. Small ones (A)

- Objects built every tick to read a constant or call a helper: `FollowMasterStrategy` (trig:442,
  every bot in phase 2), `TankFaceStrategy` / `TankAssistStrategy` (trig:389-394, act:1229-1236),
  `TooCloseToPlayerWithDebuffTrigger` (trig:377, every bot in phase 2), `YoggSaronTrigger` per
  Constrictor or Corruptor candidate (act:720). Use the name, a member, or a free function.
- `YoggSaronCanInterrupt` (enc:2596) builds a vector to test emptiness, for every bot every tick.
- Loop-invariant work: `YoggSaronHandoverState` + `YoggSaronBotTankAlive` (a group walk) per parked
  Guardian in `YoggSaronPhase1Focus` (enc:3315, 3344); `YoggSaronGuardianDiedWithin` per member
  (enc:2749); `YoggSaronCrusherStunHolds` per leftover × crusher (enc:3770).
- `"rti"` read three times in `YoggSaronIllusionRoomTrigger::IsActive` (trig:460, 491, 499);
  `std::string` temporaries in `YoggSaronKickRank` (enc:4656) and `YoggSaronInterruptSpellsFor`
  (enc:2641).

## Looked at and left alone

- **A timed or per-tick phase cache outside triggers.** An action can move the phase mid-tick; item 1
  makes it pointless anyway.
- **Gating the Dark Volley trigger, the exclusion sweep or the nova reads on boss state.** Guardians
  spawn from clouds before the pull (`boss_yoggsaron.cpp:773`).
- **Merging the two Immortal Guardian sweeps in `YoggSaronInterruptCastersAround` (enc:2543).** It
  reorders the casters, and the kick action takes the first it can reach.
- **`"find target" yogg-saron` in the sanity retreat (act:2150)** and the `"nearest npcs"` semantics
  beyond item 4: the value's own filters are part of the answer.
- **Walk latch growth** (ulduar-perf-followups item 9): eviction changes which latch matches.
- **Holding the `YoggSaronStateFor` reference across a pass** (51 call sites, a mutex each): entries
  can be erased, and the gain is a few microseconds.
- **Trigger `checkInterval` throttling:** it changes reaction time.

## Verification

Per item, before its commit:

1. Syntax-check the changed translation units and the ones including a changed header,
   `tools/pblint/pblint.py <paths>`, `python apps/codestyle/codestyle-cpp.py`; `tools/nativetest/run.sh`
   for item 6 (`raid_encounter_rules_test.cpp`).
2. Items 3, 5, 6, 7, 9: the equivalence harness described in ulduar-perf-followups — old bodies from
   `git show HEAD:<file>`, new ones from the tree, compiled together with counting stubs over the
   enumerated inputs, with a red-capability check.
3. Items 1, 4, 8: a temporary shadow build that computes both the old sweep and the new lookup and
   logs any difference; one Yogg pull through all three phases and an illusion wave must log none.
   Remove it before the commit.
4. Item 2: the same shadow check on the exclusion set against the caller's candidates, on a Yogg
   phase 1 pull and on any other boss.
5. Cost: `.playerbots pmon` on one pull before and after, read as ratios between buckets only
   ([observability](../../systems/observability.md)). Multipliers have their own bucket
   (`Engine.cpp:249`), which is what ranks item 6 against item 1.

When item 1 ships, drop item 10 from ulduar-perf-followups and correct the three "phase read last"
comments item 3 touches.
