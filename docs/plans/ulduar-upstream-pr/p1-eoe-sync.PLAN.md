# p1-eoe-sync

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Update PR #2877 (branch `eoe-malygos-rework`, from `origin/eoe-malygos-rework`) per the **`p1-eoe-sync`**
paragraph of `ulduar-upstream-pr.PLAN.md`: `git merge raid-encounter-framework` (already carrying
`upstream/test-staging`), resolve conflicts using `Custom`'s merged `src/Ai/Raid/EoE/` as the reference for the
#2854 renames, and drop the branch's own `CommandPetAttack`/`StopPet` from `src/Util/EncounterHelpers.{h,cpp}`
(the framework now has them). Behaviour of EoE must not change. Commit on the branch; no push.

## Task list

## Decisions for review

## Blocked
