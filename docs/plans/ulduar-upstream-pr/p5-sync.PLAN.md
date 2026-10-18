# p5-sync

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Carry `Custom` changes since the export base (Status line of `ulduar-upstream-pr.PLAN.md`) into the PR branches, per
the **`p5-sync`** paragraph there: `git diff <base> Custom -- src/Ai/Raid/Uld` into `ulduar-rework`, and the
helper files listed in the "helpers PR" rows plus `src/Util/RaidDifficulty.h` into `raid-shared-helpers`, the
Ulduar hard mode options in `src/PlayerbotAIConfig.{h,cpp}` and `conf/playerbots.conf.dist` into `ulduar-rework`, applying the same RaidObs strip and
adaptations as `p3-ulduar.PLAN.md`. If upstream was merged into `Custom` since, first merge the same upstream commit
into each PR branch. Parity-check the delta, commit one per branch, then write the new base into the Status line.

## Task list

## Decisions for review

## Blocked
