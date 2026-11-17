# p1-framework-cares

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the
**`p1-framework-cares`** paragraph bind this lane.

PR branches only, in their existing worktrees under
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-uldpr-wt/<branch>`: `raid-encounter-framework`,
`eoe-malygos-rework`, `raid-shared-helpers`, `ulduar-rework`. Never create a second worktree for
them and never push.

1. In `raid-encounter-framework`: `git fetch upstream` and merge `upstream/test-staging` if it moved
   past `889e9dd6e`.
2. Port the action lookup table from Custom commits `8b38899f4` ("look up each action once per
   boss, not once per rule") and `02e6952c4` (120-column wrap) onto the branch's own
   `src/Ai/Raid/RaidEncounter.{h,cpp}` and `src/Ai/Raid/RaidEncounterRules.h`. Custom's versions of
   those files also carry RaidObs, `traceName`, the `{open, live}` gate struct and ICC-only pieces
   (`LiveMultiplier`, `UnitGate`): port none of that. `tools/nativetest/raid_encounter_rules_test.cpp`
   stays out (no `tools/` upstream). One commit, `perf(Raid): …` subject ≤72 chars.
3. Merge `raid-encounter-framework` into `eoe-malygos-rework` and into `raid-shared-helpers`, then
   `raid-shared-helpers` into `ulduar-rework`, default merge messages. Run the checks on each branch
   over `git diff --name-only upstream/test-staging HEAD` limited to the files the merge touched.
4. Record each branch's new head in the lane report.

## Task list

## Decisions for review

## Blocked
