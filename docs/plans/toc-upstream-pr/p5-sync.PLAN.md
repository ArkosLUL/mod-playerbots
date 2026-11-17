# p5-sync

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks), the Wave 3
adaptation list and the **`p5-sync`** paragraph bind this lane. Repeatable; runs on the user's word.

1. Base: the export base in the overview's Status line. Delta: `git diff <base> Custom --
   src/Ai/Raid/ToC` plus the helper files the table marks #2985.
2. Port the delta into `trial-of-the-crusader` (worktree `build-tocpr-wt/trial-of-the-crusader`)
   and the helper part into `raid-shared-helpers` (worktree `build-uldpr-wt/raid-shared-helpers`),
   applying the same adaptations as `p3-toc` (`git apply --3way`, then strip RaidObs). One commit
   per branch; merge the helpers commit into `ulduar-rework` and `trial-of-the-crusader`.
3. Parity: the ported delta differs from Custom's only by RaidObs removal and listed adaptations.
4. Checks on every touched branch; report the new export base.

## Task list

## Decisions for review

## Blocked
