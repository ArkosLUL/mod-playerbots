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

## Candidates done

## Candidates dropped

## Decisions for review

## Known gaps

## Blocked
