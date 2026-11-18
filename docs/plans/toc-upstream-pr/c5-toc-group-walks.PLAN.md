# c5-toc-group-walks

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**Wave 3** section bind this lane.

Behaviour change on `Custom`, branch `tocpr/c5-toc-group-walks`: every group walk that a trigger's
`IsActive` or a multiplier reaches uncached gets an O(1) per-tick cost, for upstream's O(1) trigger
rule. The sites (function ← trigger, who reaches it) are listed under "Group-walk audit" in the
Decisions for review of `c1-toc-base.PLAN.md` (Jaraxxus, Twins), `c2-toc-anubarak.PLAN.md`
(Anub'arak) and `c3-toc-fc-beasts.PLAN.md` (Faction Champions, Jormungars). Their line numbers are
at Custom `1039c9045`; find each by function name.

Shape, per site:
- Default: a per-group, per-ms memo of the walk's group-wide result in the encounter's per-instance
  state, shared by every bot, like `GetJaraxxusPinnedCasters` with `JaraxxusState::pinned`
  (`Util/ToCHelpers_Jaraxxus.cpp`): store guids, never `Player*`/`Unit*`, resolve them per call, and
  a groupless bot's result never serves another bot. A per-bot answer (which healer, which hunter)
  stays per bot, computed from the memo.
- Where a cheap test (aura, cooldown, class, role) already decides the answer for most bots, test it
  before the walk instead, if that alone makes the cost O(1) for every bot outside the narrow window.
- Sites over the same walk share one memo.

The only allowed behaviour difference is the memo's: a member's change between two bots' ticks in
one ms shows one tick later. Picks, order and tie-breaks stay the same; a site that can't keep that
stays as it is, logged under Decisions for review with the reason.

Out of scope:
- `HumanInsideRelease` (`Util/ToCHelpers_Staging.cpp`): only `NoteStage`'s trace label reaches it,
  and `p3` deletes it with the trace.
- Walks reached only from actions or chain opens, or already behind a memo, latch or roster interval.
- The shared raid files (`src/Ai/Raid/Raid*.{h,cpp}`): wave 2 has ported them to the PR stack, so
  keep every edit under `src/Ai/Raid/ToC/`.

Also delete unused code (user, 2026-11-18), each referenced only by its own declaration and
definition: `AvoidCreatureClusterAction` with `FleeFromCreatureCluster` (`Action/ToCActions_Shared.*`),
`GetCreatureClusterCenter` and `GetNearestCreatureByEntry` (`Util/ToCHelpers_Shared.*`),
`ANUBARAK_PIT_CENTER` (`Util/ToCData.*`), `SPELL_MASSIVE_CRASH_*` (`Util/ToCData.h`); drop includes
left unused and doc mentions. Re-check with `git grep` before deleting. Duplicated helpers stay.

The lead may split by encounter into at most 3 implementers (Jaraxxus + Twins, Anub'arak,
FC + Jormungars); a memo helper shared across encounters stays with the finisher. The finisher also
greps `src/Ai/Raid/ToC` for `ForEachGroupMember`, `GetFirstMember`, `AliveMembersOnMap` and
`GroupOnMap` to confirm no trigger or multiplier still reaches an uncached walk, and records each new
memo in its boss doc under `docs/raids/trial-of-the-crusader/` (`lord-jaraxxus.md` describes the
pinned-cast memo). `tools/nativetest/` tests of touched code keep passing.

## Task list

## Decisions for review

## Known gaps

## Blocked
