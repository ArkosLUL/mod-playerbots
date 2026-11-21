# Lane I: spread DC's pre-pull refresh so it never freezes the server

You work in a worktree of the mod-dungeon-clear (DC) clone: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-dc-w4`, branch `rt/dc-refresh` (from `local/wrath-raid-tests`). Read DC's `CLAUDE.md` and follow it, except that you never build: nothing here has a compiler, and the orchestrator's capped Docker rebuild of the merged branch is the compile check, so read every declaration you call. The fork is `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots` (branch Custom), the core `G:/DevStuff/GitHub/azerothcore-wotlk-pb/src`.

The problem: before every pull of a re-pulled raid run (the first one and after each wipe), `DcTestRunJob::TickRepull` takes `DcTestRepull::Action::Refresh` and calls `RefreshParty()` (`src/TestRun/DcTestRunRepull.cpp`), which runs the fork's `maintenance` action plus cooldown, health, mana and pet resets for all 25 members in one world tick. That stops the world thread for about 7 s: the server log shows `TESTRUN tr-20261121-182711-7 running` at 18:27:11.99 and `pull 1/1: raid refreshed ...` at 18:27:18.94, and RaidObs traces of every other raid on the server show one 6.3-7.0 s snapshot gap at each such moment (seven in one window of six parallel raids). Up to six raids run at once, so each refresh freezes five fights in progress.

Deliver, as commits on `rt/dc-refresh`:

- The refresh spread across ticks so no tick spends more than about 100 ms on it (a member's `maintenance` alone likely costs about 250 ms, so at most one member per tick, with ticks spaced so the rest of the world keeps its cadence; choose the spacing and say why). Every member is refreshed before the pull starts, and the 30 s settle (`DcTestRepull::kSettleMs`) still runs after the last one. Keep the order constraint the code states (maintenance before the health and mana reset). A member that logs out or dies mid-refresh must not stall the run.
- Find out from the fork's code what makes `maintenance` expensive (`src/Ai/Base/Actions/TrainerAction.cpp`, `MaintenanceAction` and what it calls; the `AltMaintenance*` settings apply to roster bots) and report it; do not change the fork or drop any part of maintenance.
- One log line per refresh with the total and the slowest member's time in ms, so a live run shows the cost.
- Check every other place a run start or wipe reset does per-member work in one tick (login, gear restore, revive, grouping, teleport) and report which could also stall; spread them the same way only when they are clearly heavy.

Add or extend gtests in `t/` for any pure logic you change (for example the tracker holding Start until the refresh queue is empty); they cannot run here. Keep everything generic enough to go upstream.

Boundaries: no Docker, SOAP, server or database access; no merge, push, amend, rebase or branch. Commit messages go to files named `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-i-<n>.txt`, committed with `git commit -F`, no AI trailer.

Report as your final message: commits; how the refresh is now paced and how long a 25-man refresh takes in wall time; what makes `maintenance` expensive; other stall candidates found; decisions you made alone; what only a live run can confirm.
