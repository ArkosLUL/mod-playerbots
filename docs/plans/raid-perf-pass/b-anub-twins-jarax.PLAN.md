# b-anub-twins-jarax: Anub'arak, Twins and Jaraxxus

Lane of [raid-perf-pass.PLAN.md](raid-perf-pass.PLAN.md) (wave 2, branched from Custom after wave 1
merged, beside `c-icc`). The overview's neutrality rule, file ownership, git, Docker and verification
rules bind this lane.

Implement the overview's lane `b-anub-twins-jarax` items, including its "Not neutral, leave alone"
list. Line references there predate `44b6dcaa0`, and `4b9bdbce9` reworked `ToCHelpers_TwinValkyr.*`
since; re-verify everything against Custom.

- Wave 1 merged the splash cache API (lane `a-shared-fc`) and the entry-set list search helper in
  `ToCHelpers_Shared` (lane `t-triggers`, its T3); read their commits on Custom before using them.
- Hot rows (2026-11-16, 25H, µs per check): `anubarak kiter needs immunity` 44.9, the first Anub'arak
  node, so it pays the ReadPhase refresh, Sweep (`ToCHelpers_Anubarak.cpp` ~862, 120 yd over 4
  entries), `RefreshSideTanks` and the claim refresh; `twin valkyr pact interrupt duty` 22.0, which
  pays ReadTwins (ScanArena `ToCHelpers_TwinValkyr.cpp` ~225). Dumps:
  `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/perf/_pmon/run2/`.
- T3 adoption: Sweep and ScanArena use the shared helper; same visit, check, phase and order.
  Proof: review against the helper's harness from wave 1.
- T6: `StableMainTankGuid` and `RankSideTanks` each call `IsAnubarakTankPlayer` twice per member
  (with linear `IsAssistant` scans); compute it once per member per refresh and pass it on. Reuse
  the `CountSpentKitePatches` set (`ToCHelpers_Anubarak.cpp` ~1760); its notes only count, so trace
  bytes stay the same. Proof: review.
- `ResolveAlive` on a stack buffer: never loop the state vector in place, since a nested
  ReadPhase→Sweep clears `state.burrowers`. Proof: harness.

One commit per fix, each compiling (`pb-check` over its paths), harness files in
`build-toc-wt/perf/_harness/b-anub-twins-jarax/`, never committed.

## Task list

Re-verified on Custom `3175a814b` (the lane base; harness "old" bodies come from it). One lead, no
split: thirteen small candidates in eight files, the helper ones independent of the multiplier ones but
each part too small to pay for its own agent. Harnesses in `build-toc-wt/perf/_harness/b-anub-twins-jarax/`.

Helpers:
- **T3a** `ToCHelpers_Anubarak.cpp` `Sweep` L861-862: `std::list` + core multi-entry search → vector +
  `GetCreatureListWithEntriesInGrid` over a `ToCEntrySet` of `SWEEP_ENTRIES`; loop unchanged. Proof: the
  wave 1 T3 harness (its lists include `SWEEP_ENTRIES` in this order) rerun, plus review (same origin,
  radius, visit, phase test, order; the loop only iterates).
- **T3b** `ToCHelpers_TwinValkyr.cpp` `ScanArena` L224-225: same swap over `TWIN_ARENA_ENTRIES`. Proof as T3a.
- **RA** `ToCHelpers_Anubarak.cpp` `ResolveAlive` L890: returns a `boost::container::small_vector<Creature*,
  16>` (core already ships Boost.Container), still a fresh snapshot per call; `ResolveSpheres`,
  `GetAnubarakBurrowers` keep `std::vector` through the same body; the two stored locals (L4509 sort,
  L7107) become the small vector. Proof: harness (old body vs new for both list types over every guid
  list of up to 7 entries × missing/dead/alive plus random ones past 16, result order and the `GetCreature`/`IsAlive` log; the
  L4509 sort on both); red: drop the `IsAlive` test.
- **T6** `StableMainTankGuid` L1058, `RankSideTanks` L1076, callers `RefreshSideTanks` L1111 and
  `GetAnubarakPrepullTanks` L5966: the walk tells its caller it found the main tank by walking, so
  `RankSideTanks` skips `IsAnubarakTankPlayer` for members ahead of that tank (all known non-tanks).
  Proof: review (IsTank pure, nothing runs between the walks).
- **KS** `CountSpentKitePatches` L1760: `landed` becomes a cleared per-state scratch swapped with
  `kitePatchesSeen`. Proof: review (notes carry only the counter, so iteration order can't reach the trace).
- **TG-A** Anub'arak computed-argument notes L1753, L1823, L3064-3069 (not `NoteHands`), L3145, L5427,
  L5534, L8139 behind `RaidObs::Active()`. Proof: review (pure argument builds; `NoteDerived` is a no-op
  when inactive).
- **TG-T** Twins `GetWantedEssence` L1161 and `GetTwinDpsTarget` L1329-1331 notes behind `Active()`.
  Proof: review.

Multipliers:
- **R11** `AnubarakProtectSpikeKiteMultiplier` (ungated): per-`Action*` "held while marked" in a
  `RaidActionCache`; `HasAura`/`AnubarakEngaged` at the same points. Proof: harness `anub`.
- **R6** Anub'arak `ThreatCap`, `BurrowerThreatCap`, `BurrowerGuard`, `SideTankAreaGuard`, `SphereGuard`:
  a `RaidSplashCache` member each and the cache overload of `IsRaidAreaDamage`; `SlowOrSpreadOnSphere` and
  `StopsCast` read the cast and the `"spell id"` value through the same cache. Proof: harness `anub`.
- **NM** `BurrowerGuard` (`"bladestorm"`, `MovesBurrower`) and `SphereGuard` (`"starfall"`,
  `IsSphereCrowdControl`) name tests per `Action*`. Proof: harness `anub`.
- **R6-J** `JaraxxusThreatCapMultiplier`: `RaidSplashCache` member. Proof: harness `anub`.
- **N1-TI** `TwinValkyrInterruptHoldMultiplier` (ungated): `InterruptKindOf(getName())` per `Action*`.
  Proof: harness `anub`.
- **N1-TR** `TwinValkyrRedirectGuardMultiplier` (ungated): its four casts per `Action*`, class test
  first as before. Proof: harness `anub`.

Left alone by rule: the `++` note in `CountSpentKitePatches`, `NoteHands`, any memo on
`GetAnubarakDpsTarget`, `ProtectorOrder`; one-cast guards (Twins/Jaraxxus tank movement, kiss hold, avoid
aoe), the taunt guards (`IsTauntAction` reads `IsTank`).

## Candidates done

Harnesses in `build-toc-wt/perf/_harness/b-anub-twins-jarax/`: `python -I run.py [resolve|anub]` cuts the old
bodies verbatim out of `3175a814b` and the new ones out of the worktree (`extract.py`), builds and runs each
in a capped `acore/ac-wotlk-build:master` container, then every red mutation, which must fail;
`python -I extract_t3.py t3` + `run.sh t3` reruns the wave 1 entry-set harness against this worktree.
pb-check over the lane's 8 changed source files (9 TUs): green.

| id | site | why neutral | proof | commit |
|---|---|---|---|---|
| T3a | `ToCHelpers_Anubarak.cpp` `Sweep` | same `Cell::VisitObjects` from the same origin and radius, same `CreatureListSearcher` phase test, same check (entry membership, then `IsWithinDist(…, false)` only on a match), pushed in visit order into a vector the loop only iterates | `t3_entryset.cpp` (its lists include `SWEEP_ENTRIES` in this order): 1,172,540 cases, 0 mismatches; red 226,908 | `a114e58a7` |
| T3b | `ToCHelpers_TwinValkyr.cpp` `ScanArena` | as T3a over `TWIN_ARENA_ENTRIES` | as T3a (list included) | `37bf278ac` |
| RA | `ResolveAlive` | same body returning a `boost::container::small_vector<Creature*, 16>` by value, so still a snapshot; `ResolveSpheres` and `GetAnubarakBurrowers` keep `std::vector` through the same template; the four sphere loops call `ResolveAlive(…, state.spheres)`, which `ResolveSpheres` is; the dealer sorts the same sequence | harness `resolve`: 287,380 calls (every list of up to 7 slots × empty guid/missing/dead/alive × map or none × repeated guids, 200k random lists of 8-40), output order, the sort and the `GetMap`/`GetCreature`/`IsAlive` log, 0 mismatches; red (no `IsAlive` test) 421,882 | `8bd712470` |
| T6 | `StableMainTankGuid`, `RankSideTanks` and both callers | without a flagged tank, every living member ahead of the walk's answer (all of them when it finds none) failed `IsAnubarakTankPlayer`; `RankSideTanks` skips exactly those, which its own test would have turned down; null and dead members it skips anyway; `IsTank` is pure (strategy mask, exact `specTabCache`) and nothing runs between the two walks; the added `GetGUID` is a pure getter | review | `889db6b3d` |
| KS | `CountSpentKitePatches` | `landed` is a cleared per-state scratch swapped into `kitePatchesSeen`: same content; iteration order can differ, but each hit only bumps the counter and notes its new value, so notes and count are the same | review | `a7586de4f` |
| TG-A | Anub'arak notes `anub.p3wave`, `anub.push` (both), `anub.kitepath`, `anub.kitepred`, `anub.kitestage`, `anub.subspot`, `anub.judge` | arguments are pure (`to_string`, `lround`, `snprintf`, `DescribeDerived`, `GetName`, `GetPowerPct`, the two kite text builders); `NoteDerived` does nothing without an open trace (`ProbeTarget` tests `Active()` first); every gated block held only the note | review | `9063ea690` |
| TG-T | Twins `GetWantedEssence`, `GetTwinDpsTarget` notes | `ReasonName`, `ColourName`, `TwinColourOf` are switches on fixed values; `rule` is only read by the note | review | `15c8376ea` |
| R11 | `AnubarakProtectSpikeKiteMultiplier` | "held while marked" is a pure function of the action's type and name; the old Feign Death branch made the same `HasAura`, then `AnubarakEngaged` only if marked, with the same verdict as the held path, so it joins it; `HasAura`/`AnubarakEngaged` run under the same conditions | harness `anub` | `981d76f5e` |
| R6 | Anub'arak `ThreatCap`, `SphereGuard`, `BurrowerGuard`, `BurrowerThreatCap`, `SideTankAreaGuard`; `SlowOrSpreadOnSphere`, `StopsCast` | the wave 1 cache overload at each `IsRaidAreaDamage` site; the cast and the `"spell id"` value come from the same per-action facts, created at the same first read and `Get()` on every read; every memo and helper call stays in place | harness `anub` | `02a687664` |
| NM | `SphereGuard` (starfall, crowd control), `BurrowerGuard` (bladestorm, movers) | pure functions of the fixed name, worked out at first sight after the same memo calls | harness `anub` | `3a6048f71` |
| R6-J | `JaraxxusThreatCapMultiplier` | as R6 | harness `anub` | `5f0494b9d` |
| N1-TI | `TwinValkyrInterruptHoldMultiplier` | `InterruptKindOf` of the fixed name; the encounter memo and shield read run under the same conditions | harness `anub` | `2f0aef500` |
| N1-TR | `TwinValkyrRedirectGuardMultiplier` | the four casts of a fixed type, still after the class test (a null action caches false, as the casts gave) | harness `anub` | `8ff51744f` |

Harness `anub`: all nine changed multipliers, old vs new, sharing one context per side as a bot does:
every subset of the ten spike-kite cast targets (1,024 types built by virtual inheritance, so each
`dynamic_cast` sees a real dynamic type), the 16 redirect subsets and the cast/heal kinds, three actions per
type spread over 34 names, 3 threat types and 6 spells, spell swaps (PullAction) and spell id changes per
round, 392 SpellInfo shapes plus
no id and no info, every helper answer flipping per round, null actions, hunter/rogue/warrior bots.
20,287,683 calls, 45,963,900 logged events (every memo, helper, `GetTarget`, `"current target"`, `"spell
id"` create and `Get()`, `HasAura`, `GetMap`/`IsHeroic`, distance, cast state), 0 mismatches; each
multiplier both holds and passes. Reds, all failing: cache ignores its key 3,495,763; attack hold
flipped 89,319; spell mover freed 1,290,899; Feign Death freed 750,788; kite action held 119,602; spell id
not refetched 1,447,274; area aura ignored in `SlowOrSpreadOnSphere` 7; `StopsCast` skips the `Get()`
17,400; starfall dropped 15,021; crowd control dropped 44,736; bladestorm dropped 34,500; movers dropped
5,456; Jaraxxus area test dropped 1,016,506; interrupt kind misread 226,990; one redirect cast dropped
1,429; Anub'arak area test dropped 656,060.

Review (lead, three lenses over `Custom...perf/b-anub-twins-jarax`): no behaviour difference found in any
hunk; harnesses rerun on the final tree, green with every red failing; one commit per fix, each
pb-checked; edits only in the lane's files.

## Candidates dropped

- Twins and Jaraxxus tank movement guards, `JaraxxusKissCastHold`, `JaraxxusAvoidAoeGuard`: one
  `dynamic_cast` each; a pointer-keyed lookup costs about the same.
- Twins and Jaraxxus taunt guards: `EncounterHelpers::IsTauntAction` reads `IsTank` (a role, never
  cached) and lives in `src/Util`.
- The leading `dynamic_cast<CastHealingSpellAction*>` in the R6 multipliers: one cast; the splash cache's
  `heal` fact would cost a lookup of its own, since each multiplier's cache sees an action once per pass.
- `SideTankAreaGuard`'s `getName()` copy: only side tanks get past its side gate, too little to pay for a
  second cache.

## Decisions for review

- RA uses `boost::container::small_vector` (header-only; the core already includes Boost.Container in
  `Unit.h`). The upstream ToC PR may prefer a std-only buffer; behaviour is the same either way.
- T6 relies on the group's member list not changing between the two back-to-back walks: same map thread,
  no code between them; a membership change from another thread was already a race in the old code.
- R11 folds Feign Death into the held branch instead of keeping a third kind: same calls, same order, same
  verdict.
- N1-TI moves `TwinInterruptKind` from the `.cpp`'s anonymous namespace into the multiplier header (global
  scope, no other user of the name) so the cache member can name it.
- TG-A gates every computed note argument in the file except the ones excluded by rule; `anub.kiteplan` and
  `anub.kitemove` pass literals and stay as they are.
- `SideTankAreaGuard` passes `getName()`, `GetTarget()` and the area test as arguments in an order the
  compiler picks; the new call has the same shape, and the only two value reads it could reorder
  (`GetTarget`'s value and `"spell id"`) don't depend on each other.
- `Active()` gates carry the same nanosecond race as every existing guard: a trace opening between the
  gate and `ProbeTarget`'s own test loses one note.

## Known gaps

- T6, KS, TG-A, TG-T: review only, as the overview allows; T3a/T3b rest on the wave 1 harness, whose
  stubs model the cell visit as a list.
- Harness `anub` models helpers as per-round world bits and value refresh clocks not at all; the action
  types are synthetic subsets, a superset of the real hierarchy.
- RA's commit body says the heap copy was most of `ResolveAlive`'s cost; not measured. It stays, since
  the lane may not amend.
- No pmon numbers yet: the after pull (step 5) measures the gain.
- Subjects over 72 chars: `3a6048f71` (77), `15c8376ea` (75), `6770c17cd` (77). They stay, since the lane
  may not amend.

## Blocked
