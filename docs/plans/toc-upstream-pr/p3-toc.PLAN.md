# p3-toc

Read `toc-upstream-pr.PLAN.md` first: its Decisions, Orchestration (authority, checks), the "What
ToC needs that the stack lacks" table, the **Wave 3** section and Verification bind this lane.

New branch `trial-of-the-crusader` from `raid-shared-helpers`, worktree
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-tocpr-wt/trial-of-the-crusader`. Starts after
`c1`–`c3` merged and `p2-helpers-toc` committed (Status table). The export base is Custom's HEAD
when the lane starts: copy `src/Ai/Raid/ToC/` from that commit (`git archive` or `git show`, never
a checkout in the main tree) and report it as `exportBase`.

Big lane: split by encounter into at most 3 implementers with disjoint files (e.g. Anub'arak;
Faction Champions; Beasts + Jaraxxus + Twins); the raid-wide base files, wiring and registration
stay with the finisher.

Facts for the adaptations listed in Wave 3:
- RaidObs API on Custom: `src/Bot/Obs/RaidObs*.h`. `ObsValue<T>` and `ObsGuidMap<V>` hold real
  state; their `.Raw()`, `.Set()` and implicit reads become plain access.
- The stack's gate type: `git show raid-shared-helpers:src/Ai/Raid/RaidEncounter.h` (`EncounterGate`,
  `EncounterDefinition` constructor).
- Upstream creator check for every string: `git grep 'creators\["<name>"\]' trial-of-the-crusader -- src`.
- Wiring hunks to port from Custom: `src/Ai/Raid/RaidStrategyContext.h`,
  `src/Bot/Engine/BuildShared{Action,Trigger}Contexts.cpp` (path-qualified includes),
  `src/Bot/PlayerbotAI.cpp` (`"trialofthecrusader"` in the instance-strategy list and `case 649`).
  Nothing else in those files.
- GPLv2 header: copy it from any upstream file under `src/Ai/Raid/Uld/` on the branch.

## Task list

## Decisions for review

## Blocked
