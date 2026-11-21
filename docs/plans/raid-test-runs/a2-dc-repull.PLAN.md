Lane A2 of `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in mod-playerbots: read its Decisions and
"Facts the work rests on" sections first. Work in the mod-dungeon-clear (DC) clone at
`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-dungeon-clear`, on the already checked-out branch
`local/wrath-raid-tests`, which already carries lane A1 (commits stamped `Session: raid-test-runs A1`:
`TestRunsOnly`, `diff=`, Ulduar/ToC registry rows). Read that repo's `CLAUDE.md` and follow it, with two
exceptions: never create a branch or worktree there, and stamp commits `Session: raid-test-runs A2`.

Goal: a scenario run of one raid boss pulls it again and again in the same instance until the kill,
resetting the raid after every wipe, so an agent can collect several pulls without a human. Write every
change generic, so it could later be offered upstream.

1. Scenario rows (`src/TestRun/DcTestDungeonRegistry.h`, scenario fields near `focusEntries`) gain:
   - `preDone`: boss ids set `DONE` through the instance script after the instance is created and before
     the start; the stale-instance check in `src/TestRun/DcTestRunJob.cpp` (grep `stale instance`)
     ignores them.
   - a start step: `aggro` (today's DC engage of the focus boss, the default) or `gossip` (NPC entry,
     gossip menu id, option index, optionally a second menu/option pair for two-step NPCs). Run gossip
     server-side with the raid leader as the player, doing what the core does for a client's select
     (`WorldSession::HandleGossipSelectOptionOpcode`, `src/server/game/Handlers/MiscHandler.cpp:89` in
     the core) after a real gossip hello. Reuse DC's existing gossip event step
     (`src/Ai/Dungeon/DungeonClear/Data/Events/KarazhanEvents.cpp:80-124`,
     `src/Ai/Dungeon/DungeonClear/Overrides/TrialOfTheChampionDriver.cpp`) where it fits.
   - an optional overall timeout and `ClearRoomBeforeBoss` override per row (raid scenarios: 21600 s, no
     room clear); `stalled` 120 s and `no_progress` 600 s stay as they are.
2. Re-pull in place, for scenario runs only (5-man and full-clear runs keep today's behaviour). A wipe
   no longer ends the run (`party_wiped`) or revives at the entrance (`DcRezRecovery.cpp`). Instead:
   revive everyone at the scenario's drop point without Resurrection Sickness; full health and mana;
   pets revived; every spell cooldown cleared on players and pets, and Sated and Exhaustion removed
   (verify their spell ids in `G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-spell-tweaks\data\dbc-reference\spell.reference.csv`
   if present); the playerbots chat action `maintenance` run on every bot; wait until the focus boss is
   alive, out of combat, back at its home position and the encounter is not in progress (and a gossip
   NPC offers its option again); then the start step again. No `rebuff`: bots buff themselves after a
   resurrection. Run `maintenance` before the first pull too.
3. `pulls=<n>` on `.dc test start` and `.dc test plan start` (default 3) caps pulls per run. A run ends
   `success` at the focus boss's kill, with a new verdict when the cap is spent, or on today's failures.
   The run record lists every pull with its start and end time (epoch ms) and outcome (kill or wipe), so
   a script can match each pull to the RaidObs trace it produced.

Add gtest cases beside the existing ones under `t/` for the pure decision logic. Nothing here can be
compiled or run, so read every declaration you call: mod-playerbots at
`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots` (a heavily diverged fork), core at
`G:\DevStuff\GitHub\azerothcore-wotlk-pb\src`. The orchestrator builds after the wave.

Never start, stop, build or exec into a Docker container, never call SOAP, never touch the live server.
Never push, amend, rebase or force. When done, stage only your files and commit with
`git commit -F <file>`, the message written to a file named `commit-msg-a2-<n>.txt` in
`C:\Users\boss2\AppData\Local\Temp\claude\4e3815b0e3fef148\fd2ca691-4a3e-4009-ba39-dc5273ee9f0c\scratchpad`.
Report: commits (hash, subject), the exact `.dc test` syntax and run-record fields you added, decisions
you made alone, anything unfinished.
