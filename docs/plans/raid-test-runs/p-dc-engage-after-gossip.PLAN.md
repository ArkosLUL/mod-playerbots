# Lane P: engage the focus after a gossip start (Twin Val'kyr)

You work in a worktree of the mod-dungeon-clear (DC) clone, on the branch the workflow prompt names (from `local/wrath-raid-tests`). Read DC's `CLAUDE.md` and follow it, except that you never build: the orchestrator's capped Docker rebuild of the merged branch is the compile check, so read every declaration you call. The fork is `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots` (branch Custom); the core is `G:/DevStuff/GitHub/azerothcore-wotlk-pb`.

The problem: the `totc-twins` row (`src/TestRun/DcTestDungeonRegistry.cpp`) starts from Barrett's gossip, which plays the Val'kyr intro, but nothing then pulls the twins. They turn attackable and aggressive about 25.5 s after the gossip (core `instance_trial_of_the_crusader.cpp`, scenes 301-304 and `EVENT_VALKYRIES_ATTACK`), about 85 yd from the raid, outside their aggro range. The fork's bots wait on their door stands for a player to pull, by design. The evidence and the exact recipe are in the fork's `docs/plans/raid-test-runs/polish/val-kyr-twins.md` (round 1, "What DC must do instead"). The user's standing decision is that DC does the logistics and an aggro start sets the boss on the tank (`EngageFocus`, `src/TestRun/DcTestRunRepull.cpp`); this extends that to a gossip start.

Deliver, as commits:

- A generic row option: after a gossip start, once the first focus creature is alive, attackable (no `UNIT_FLAG_NON_ATTACKABLE`, not immune to players) and nobody of the focus is in combat yet, run `EngageFocus(tank)` once for that pull, with a log line. It must run again on every re-pull (a retry gossip replays the intro), never before the focus turns attackable, and never through DC's own pull logic (DC stays held). If the readiness code already has an "attackable" check (another DC lane added one for Algalon), reuse it.
- `totc-twins` using it, engaging Fjola Lightbane on slot 0, the main tank.
- A note in the row comment and the polish log's terms on what Faction Champions and Anub'arak will need from the same option (both also wait for a player to pull, per the fork's `src/Ai/Raid/ToC/Util/ToCHelpers_Staging.cpp` around lines 573 and 674), without building those rows.
- Gtests for the pure logic.

Boundaries: no Docker, SOAP, server or database access; no merge, push, amend, rebase or branch. Commit messages go to files named `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-p-<n>.txt`, committed with `git commit -F`, no AI trailer.

Report as your final message: commits; how a row declares the engage and when it fires; what the Twins run should log on the next live pull; what Faction Champions and Anub'arak need on top; what only a live run can confirm.
