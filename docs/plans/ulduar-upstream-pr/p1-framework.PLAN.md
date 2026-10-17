# p1-framework

Read `ulduar-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks) and the section named below bind this lane.

Update PR #2876 (branch `raid-encounter-framework`, from `origin/raid-encounter-framework`) per the
**`p1-framework`** paragraph and the four `#2876` rows of the "What Ulduar needs" table in
`ulduar-upstream-pr.PLAN.md`. First `git merge upstream/test-staging` (merge, never rebase). Source of each
piece: the same symbol on `Custom` (`src/Ai/Raid/RaidEncounter.{h,cpp}`, `RaidEncounterRules.h`,
`src/Bot/Engine/Strategy/Strategy.h`, `src/Bot/Engine/Engine.cpp`, `src/Util/EncounterHelpers.cpp` for
`CommandPetAttack`/`StopPet`), stripped of RaidObs, `traceName` and `EncounterGate::live`. Upstream rules:
`git show upstream/test-staging:AGENTS.md` and `.agents/docs/*` (comments one line, two at most; no braces on
single-statement bodies; `_lowerCamelCase` privates). Commit on the branch as
`feat(Raid): <summary>`; no push. `RaidEncounterRules.h` touches mean `tools/nativetest/run.sh` from the main tree
does not apply here (no `tools/` upstream); syntax check with `PB_REPO=<worktree>`.

## Task list

## Decisions for review

## Blocked
