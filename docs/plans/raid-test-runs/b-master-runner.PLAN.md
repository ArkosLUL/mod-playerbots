Lane B of `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in mod-playerbots
(`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots`): read its Decisions, "Facts the work
rests on" and "The cycle" sections first. Create a worktree for this lane and work only there:
`git -C G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots worktree add G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-b -b rt/b Custom`.
Other sessions edit the main checkout at the same time; never edit it.

Part 1, a master off the instance. Bots read their master as "the player leading the raid" in these files
under `src/Ai/Raid/`: `ToC/Util/ToCHelpers_Staging.cpp` (`StagingLeader`),
`ToC/Action/ToCActions_FactionChampionsDefence.cpp`, `Uld/Trigger/UldTriggers_FlameLeviathan.cpp`,
`Uld/Trigger/UldTriggers_Thorim.cpp`, `Uld/Action/UldActions_Thorim.cpp`, `Uld/Util/UldEncounter_Thorim.cpp`,
`Uld/Trigger/UldTriggers_YoggSaron.cpp`, `Uld/Util/UldEncounter_YoggSaron.cpp`,
`Uld/Action/UldActions_Auriaya.cpp` (grep `GetMaster()` there for every site). In an automated test run
the bots' master is a stand-in GM character that is not in the raid and not on the instance, so these
reads see nobody (`StagingLeader` returns `nullptr`) or the wrong player. Add one shared helper, in
`src/Ai/Raid/RaidCombatHelpers.{h,cpp}` or wherever fits better: the master when in world on this bot's
map and instance, else the group leader when in world on this map and instance, else `nullptr`. Read
what each site uses the master for, and switch the sites where it stands for the raid's leader. Leave
Flame Leviathan's vehicle check (`master->GetVehicle()`) alone, and leave any site that means the
command sender. With a human master in the raid, behaviour must not change.

Part 2, the runner, `tools/raidtest/`, Python stdlib only:
- `soap.py`: run one GM command over the worldserver's SOAP (`urn:AC` `executeCommand`, HTTP basic
  auth), credentials from `tools/raidtest/.env` (`SOAP_URL`, `SOAP_USER`, `SOAP_PASS`; the file is
  gitignored and may not exist yet); never print or log the credentials; a SOAP fault is an error.
- `raidtest.py`: a `cycle` command implementing "The cycle" in the plan for one boss (trace slug,
  DC scenario token, raid pool `uld`/`toc`, difficulty, default 3 valid pulls, cap 6 pulls, flags to
  skip the baseline or the rebuild), plus a `report` command that runs only the reporting step. Facts:
  - Server lock: create `G:/DevStuff/GitHub/.wave-loop/locks/server` with `mkdir` (atomic) plus an
    `owner` file; refuse if it exists, printing the owner; always release.
  - Rebuild, from the core root `G:/DevStuff/GitHub/azerothcore-wotlk-pb`:
    `docker compose -f docker-compose.yml -f docker-compose.override.yml -f .agents/plans/worldserver-build/compose.build.yml build --progress plain ac-worldserver`
    (log to a file; judge it by the compose exit code itself, never by a later command's), then
    `docker compose <same -f flags> up -d --no-deps ac-worldserver`, then wait for
    `worldserver-daemon) ready` in `docker logs --since <restart time> ac-worldserver`.
  - Refuse before building when any tracked file is modified, or any untracked non-ignored file sits
    under `src/`, in the core repo or any `modules/*` repo: it would ship in the build.
  - Pull counting and validity: reuse the `tools/botobs/raidobs` package (read `trace.py`,
    `validity.py`, `encounter.py` and how `tools/botobs/batch.py` applies `--strict`, `--since` and
    `--boss`) rather than re-deriving; a pull counts when it is valid under `--strict`, its build is
    newer than HEAD's commit and its trace header's difficulty matches. Baseline pulls are the newest
    valid ones of the same boss and difficulty built before HEAD.
  - DC interface (lanes A1-A3 build it in the same wave, so code against this contract and keep parsing
    in one place): `.dc test start <scenario> roster=<raid> diff=<10n|10h|25n|25h> pulls=<n>`,
    `.dc test status`, `.dc test stop`; the first call may answer that the driver is still logging in:
    retry for up to two minutes. Run records are JSON lines in `dc_testruns.jsonl` in the worldserver's
    working directory inside the `ac-worldserver` container (take that path as a setting; read it with
    `docker exec ac-worldserver cat`), each listing its pulls with start and end time and outcome.
  - Report (markdown on stdout): per pull outcome and duration, deaths with cause, hard mode on/off from
    the trace header, `batch.py --strict --since <HEAD> --boss <slug>`, `--split-at <HEAD>`, `--probes`,
    the boss reader `tools/botobs/bosses/<slug>.py` when one exists, and every `veto` row whose
    multiplier is `dungeon clear` or `dungeon clear combat` inside a pull window (contamination).
  - Unit tests in `tools/raidtest/tests/` (`python -m unittest discover -s tools/raidtest/tests`) for
    parsing, pull counting, the lock and the dirty-tree check, with SOAP and subprocess mocked. Never run
    the runner itself, and never start, stop, build or exec into a Docker container or call SOAP:
    `pb-check`'s own throwaway check containers are the only exception.

Close the lane per the module's `CLAUDE.local.md` cycle: `PB_REPO=G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-b ~/.claude/scripts/pb-check <changed paths>`,
the raidtest tests, then commit on `rt/b` with `git commit -F <file>`, the message in a file named
`commit-msg-b-<n>.txt` in
`C:\Users\boss2\AppData\Local\Temp\claude\4e3815b0e3fef148\fd2ca691-4a3e-4009-ba39-dc5273ee9f0c\scratchpad`.
Skip the docs step: durable docs come at the effort's close-out; never touch `docs/plans/`. Never merge,
push, amend, rebase or force. Report: commits (hash, subject), every master site and what you did with
it, the runner's CLI, check results, decisions you made alone, anything unfinished.
