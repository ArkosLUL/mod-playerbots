# p2-helpers

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

New branch `raid-shared-helpers` from `raid-encounter-framework`, per the **`p2-helpers`** paragraph and the
"helpers PR" rows of the "What Ulduar needs" table in `ulduar-upstream-pr.PLAN.md`. Port each piece from
`Custom` (already on upstream names), only the parts Ulduar calls (`git grep` the symbol under
`src/Ai/Raid/Uld`), RaidObs stripped, GPLv2 headers, upstream comment rule
(`git show upstream/test-staging:.agents/docs/cpp-playerbots.md`). Commit as `feat(Raid): <summary>`, one commit
per coherent piece; no push. `git grep -n "RaidObs\|ObsValue\|ObsGuid" raid-shared-helpers -- src` must be empty.

## Task list

## Decisions for review

## Blocked
