# Yogg-Saron strategy: performance work that changes no behaviour

## Context

The hot paths of the Yogg-Saron bot strategy, and the optimizations that leave behaviour untouched.
The list comes from reading the code and counting sweeps per bot per tick; **nothing was measured**,
so the ranking is by counted work, not by time. Items 1-4 are in; 5-9 are open, independent, and can
each ship alone.

Files: `src/Ai/Raid/Uld/` — `Util/UldEncounter_YoggSaron.{h,cpp}` (**enc**),
`Trigger/UldTriggers_YoggSaron.cpp` (**trig**), `Action/UldActions_YoggSaron.{h,cpp}` (**act**),
`Multiplier/UldMultipliers_YoggSaron.cpp`, `Definition/UldDefinition_YoggSaron.cpp` (**def**);
framework `src/Ai/Raid/RaidEncounter.cpp`. Core files are named from the server root:
`instance_ulduar.cpp` and `boss_yoggsaron.cpp` under `src/server/scripts/Northrend/Ulduar/Ulduar/`,
the rest under `src/server/game/`. Line numbers are as of 2026-10-10.

## Where the work goes

One bot tick is `Strategy::OnTick` → all triggers checked → actions popped by relevance, each through
`isUseful` → every multiplier → `Execute` (`Engine.cpp:211-338`).

- As it stood before items 1-4, kept for the reasoning behind 5-9: **the pass cache only covers
  trigger checks.** `YoggSaronPassReads` (enc:295) is keyed on
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

### 1-4. Shipped 2026-10-10, live check still owed

The instance-script lookup for Sara, Yogg and the Brain, the target-exclusion gate, the phase-first
reorders and the entry-first Immortal Guardian list are in; what they established is in
[yogg-saron.md](../../raids/ulduar/yogg-saron.md) and
[raid-mechanics-lessons](../../engine/raid-mechanics-lessons.md). The database facts held: Sara is the
only one of the three with a spawn, and data type 12 is not reused (`UlduarData` starts at 14).

They passed the syntax check, pblint and codestyle only. **Not run:** the shadow comparison on a
live pull (old sweep against new lookup for the three creatures and for the Guardian list, old
exclusion set against new), and `pmon` before and after.

Left out of item 2: skipping the whole gather when the candidate list is empty. Other strategies'
exclusion hooks were not checked for side effects.

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
  same sweep fallback as `YoggSaronFindNearestCreature`. A miss is never cached.
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
3. Item 8, and items 1, 2 and 4 after the fact: a temporary shadow build that computes both the old
   sweep and the new lookup (or exclusion set) and logs any difference; one Yogg pull through all
   three phases and an illusion wave, plus any other boss for item 2, must log none. Remove it before
   the commit.
4. Cost: `.playerbots pmon` on one pull before and after, read as ratios between buckets only
   ([observability](../../systems/observability.md)). Multipliers have their own bucket
   (`Engine.cpp:249`), which is what ranks item 6 against item 1.
