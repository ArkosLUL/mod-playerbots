# p2-helpers-toc

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks), the "What
ToC needs that the stack lacks" table and the **`p2-helpers-toc`** paragraph bind this lane.

Branch `raid-shared-helpers`, worktree
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-uldpr-wt/raid-shared-helpers` (already checked out
there; never a second worktree, never push). Starts after `c1-toc-base` merged into Custom and
`p1-framework-cares` committed; check both in the overview's Status table.

1. Merge the current `raid-encounter-framework` into the branch if it isn't in yet.
2. Port every row marked #2985 in the table from Custom's HEAD: the four new files whole, the
   listed symbols only for the partial ones. Strip RaidObs and trace keys; GPLv2 header on new
   files; upstream comment and naming rules. Find the exact ToC call sites with
   `git grep <symbol> Custom -- src/Ai/Raid/ToC` so nothing ToC calls is missed and nothing it
   doesn't call is added.
3. Every action, trigger or strategy name the ported code passes as a string must have a creator on
   this branch (`git grep 'creators\["<name>"\]'`); drop or adapt a call that doesn't and log it.
4. One commit per file group, `feat(Raid): …` subjects ≤72 chars.
5. Merge the branch into `ulduar-rework` (worktree `build-uldpr-wt/ulduar-rework`); the merge must
   change no `src/Ai/Raid/Uld/` file. Checks on both branches.
6. Draft the #2985 PR-body bullet for the new helpers in the lane report (not posted).

## Task list

## Decisions for review

## Blocked
