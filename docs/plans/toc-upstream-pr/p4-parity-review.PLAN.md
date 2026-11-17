# p4-parity-review

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks), the "What
ToC needs that the stack lacks" table, the Wave 3 adaptation list and the **Wave 4** section bind
this lane.

Branch `trial-of-the-crusader`, worktree
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-tocpr-wt/trial-of-the-crusader`; for the helper and
framework parts, `build-uldpr-wt/raid-shared-helpers` and `build-uldpr-wt/raid-encounter-framework`.
The export base is in the overview's Status line.

You did not write any of this and get no report from the lanes that did. Check every claim against
the code.

1. `git diff <export base>:src/Ai/Raid/ToC trial-of-the-crusader:src/Ai/Raid/ToC`: classify every
   hunk as RaidObs removal, a listed adaptation, or neither. Neither is a bug.
2. The same for each helper the table marks #2985 (Custom HEAD vs `raid-shared-helpers`) and the
   cares table (Custom vs `raid-encounter-framework`).
3. Upstream's rules (`git show upstream/test-staging:AGENTS.md`, `.agents/docs/*.md`): O(1) trigger
   `IsActive`, shared state guarded, no raw `Unit*` kept across ticks, no cheats, relevance bands,
   comments ≤2 lines, naming. Report rule hits that Custom shares by count and example; fix only
   what is upstream-only.
4. Every string-wired action, trigger and strategy name resolves on the branch.
5. Fix bugs on Custom first when Custom has them too (commit there by explicit path, then carry
   the fix to the branch); upstream-only fixes go on the branch. One commit per fix.

## Task list

## Decisions for review

## Blocked
