Lane A1 of `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in mod-playerbots: read its Decisions and
"Facts the work rests on" sections first. Work in the mod-dungeon-clear (DC) clone at
`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-dungeon-clear`, on the already checked-out branch
`local/wrath-raid-tests`. Read that repo's `CLAUDE.md` and follow it, with two exceptions: never create a
branch or worktree there (stay on `local/wrath-raid-tests`), and stamp commits `Session: raid-test-runs A1`.

Goal: DC test runs can target Ulduar and Trial of the Crusader at any raid difficulty, and DC does
nothing to a bot outside a test run. Write every change generic (no fork-specific assumptions), so it
could later be offered upstream.

1. `DungeonClear.TestRunsOnly` (bool, default 0 = today's behaviour; document it in
   `conf/mod_dungeon_clear.conf.dist`). At 1, nothing of DC acts on a bot that is not in an active
   `.dc test` run: `src/DcStrategyGate.cpp` installs none of `dungeon clear`, `dungeon clear combat`,
   `food`, `worldbuff` on such a bot and strips the DC strategies it finds; the login hook in
   `src/DungeonClearModule.cpp` likewise; in-game `.dc on` and the `dc on` chat keyword refuse with a
   message. Grep for every other place that installs DC behaviour on a bot (`+dungeon clear`, the
   multipliers, value wrappers keyed on a run) and gate it the same way. Test runs keep working unchanged.
2. Raid difficulty: `diff=10n|10h|25n|25h` on `.dc test start` and `.dc test plan start`
   (`src/DungeonClearCommand.cpp`), carried on the run, the plan and the run record, and shown by
   `.dc test status`. Replace the hard-coded `RAID_DIFFICULTY_10MAN_NORMAL` in
   `src/TestRun/DcTestRunJob.cpp` (grep `SetRaidDifficulty`). `MaxPlayers` in
   `src/TestRun/DcTestDungeonRegistry.h` reads the chosen difficulty's `MapDifficulty` entry instead of
   difficulty 0, and a run's default `size` becomes that difficulty's max players. Allow heroic for raid
   rows whose map has a heroic `MapDifficulty` (the refusal is in `src/TestRun/DcTestRunManager.cpp`);
   keep the 5-man `heroic` keyword working; decide and document what `diff=` means on a 5-man.
3. Registry rows for Ulduar (map 603) and Trial of the Crusader (map 649) in
   `src/TestRun/DcTestDungeonRegistry.cpp`, beside the classic raid rows, level 80, entrance from the
   `areatrigger_teleport` rows for those maps in
   `G:\DevStuff\GitHub\azerothcore-wotlk-pb\data\sql\base\db_world\areatrigger_teleport.sql`. Read how a
   raid row's other fields are used and set them to fit; make sure `DcDungeonAccess::GrantEntry` covers
   what both maps require.

Add gtest cases beside the existing ones under `t/` for the pure logic you add. Nothing here can be
compiled or run (`t/run_tests.sh` needs a Linux build tree, and no syntax check covers this module), so
read every declaration you call: mod-playerbots at `G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots`
(a heavily diverged fork, not stock playerbots), core at `G:\DevStuff\GitHub\azerothcore-wotlk-pb\src`. The
orchestrator builds after the wave.

Never start, stop, build or exec into a Docker container, never call SOAP, never touch the live server.
Never push, amend, rebase or force. When done, stage only your files and commit with
`git commit -F <file>`, the message written to a file named `commit-msg-a1-<n>.txt` in
`C:\Users\boss2\AppData\Local\Temp\claude\4e3815b0e3fef148\fd2ca691-4a3e-4009-ba39-dc5273ee9f0c\scratchpad`.
Report: commits (hash, subject), what each item now does, decisions you made alone, anything unfinished.
