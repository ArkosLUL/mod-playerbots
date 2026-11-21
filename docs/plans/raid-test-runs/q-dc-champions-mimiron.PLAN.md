# Lane Q: DC rows for Faction Champions and Mimiron

You work in a worktree of the mod-dungeon-clear (DC) clone: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-dc-w4`, branch `rt/dc-q` (from `local/wrath-raid-tests`). Read DC's `CLAUDE.md` and follow it, except that you never build: the orchestrator's capped Docker rebuild of the merged branch is the compile check, so read every declaration you call. The fork is `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots` (branch Custom); the core is `G:/DevStuff/GitHub/azerothcore-wotlk-pb` (ToC and Ulduar scripts under `src/server/scripts/Northrend/`, world DB in `data/sql/base/db_world/` plus `data/sql/updates/db_world/`).

The effort is `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in the fork (decisions table, "Polish loop"). DC's rows in `src/TestRun/DcTestDungeonRegistry.cpp` and their row features are the pattern: `totc-twins` (Barrett stage start with `preDone`, `firstPullAfterS`, `EngageAfterGossip`, which pulls the focus on the main tank once it turns attackable), `uld-algalon` (`useObjects`, a once-per-run GameObject use with a lent key, and an attackable-focus readiness) and `uld-xt002` (aggro start). The row comment on `totc-twins` already sketches what Faction Champions need. A kill counts toward "polished" only when the hard mode triggered (ToC: heroic difficulty; Mimiron: the self-destruct hard mode).

Deliver, as commits on `rt/dc-q`:

- **`totc-champions`:** Barrett 35766's gossip menu and option ids (normal and heroic, first pull and retry), `preDone {0, 1}`, the summoned champions as focus (the instance picks them at random from the faction opposite the raid, which is all Alliance), `EngageAfterGossip`, success on ToC boss 2 DONE, and what a wipe resets. The fork's staging expects the pack near its stands (`src/Ai/Raid/ToC/Util/ToCHelpers_Staging.cpp` around line 573): check which champion should land on the main tank and say how the focus order decides it. The opener reads the clicker's selected healer (plan Wave 4 note): find out what DC must supply for it.
- **`uld-mimiron`:** how the fight starts normally and in hard mode (the Big Red Button), whether hard mode must be chosen before every pull, and the smallest row feature that does that (`useObjects` runs once per run today). The drop point with evidence, trash, wipe and readiness, comps, success on boss 10 DONE.
- Gtests for any pure logic you change.

The orchestrator checks every coordinate with navprobe before a live run. Keep everything generic enough to go upstream.

Boundaries: no Docker, SOAP, server or database access; no merge, push, amend, rebase or branch. Commit messages go to files named `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-q-<n>.txt`, committed with `git commit -F`, no AI trailer.

Report as your final message: commits; tokens and `.dc test start` commands (25h for ToC, 25n for Ulduar); every coordinate with its evidence and what navprobe must confirm; start, wipe, readiness, success and hard-mode setup per boss; decisions you made alone; what is most likely to break first.
