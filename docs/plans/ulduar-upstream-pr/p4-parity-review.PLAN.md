# p4-parity-review

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Review branches `ulduar-rework` and `raid-shared-helpers` against `Custom` at the export base recorded in the
Status line of `ulduar-upstream-pr.PLAN.md`, per the **`p4-parity-review`** paragraph there. Every difference must be
RaidObs removal or an adaptation the overview lists; report anything else with file:line as a bug, and fix bugs on
the branch (commit, no push). Also review against upstream's rules (`git show upstream/test-staging:AGENTS.md`,
`.agents/docs/code-review.md`, `.agents/docs/cpp-playerbots.md`). Report findings grouped bugs, minor, cleanup.

Fix on the branch only porting errors, where the branch differs from Custom beyond the listed adaptations.
A finding Custom shares (an upstream-rule breach, or a bug present on Custom too) is reported, never fixed
on the branch: such fixes land on Custom first and travel by `sync`, so the branch stays a mirror. Each
reported finding says whether Custom shares it.

## Task list

## Decisions for review

## Blocked
