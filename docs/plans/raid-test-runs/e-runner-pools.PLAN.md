# Lane E: runner cycles across three roster pools

You work in the mod-playerbots worktree `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-e`, branch `rt/e` (from Custom `a93bedfc7`). The effort is `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in that worktree: read its decisions table, "Roster drafts" and "The cycle". The runner is `tools/raidtest/` (`raidtest.py` cycle/report CLI, `dc.py` DC replies and records, `pulls.py` trace counting, `server.py`, `report.py`, `soap.py`; 97 tests in `tools/raidtest/tests`). Today it drives one pool per raid (`roster=uld|toc`), one DC run at a time, default difficulty 25n.

Decisions to build on:

- A raid has three pools with different balanced rosters: `uld1`, `uld2`, `uld3` and `toc1`, `toc2`, `toc3`. A cycle tests the boss on all three at once: one DC run per pool, started together, each `.dc test start <scenario> roster=<pool> diff=<d> pulls=<n>`.
- The valid-pull target is per pool, 1 by default (3 per cycle), with a cap of 2 pulls per pool (6 per cycle); both stay options.
- Baselines pair per pool: a pool's test pulls are compared only with that pool's baseline pulls. A pool short of baseline gets them first on the live build, all short pools in parallel, before the rebuild.
- Default difficulty: 25h for `--raid toc`, 25n for `--raid uld` (Ulduar hard modes come from server toggles). `--diff` overrides.
- Parallel runs of one boss overlap in time, so match traces to DC runs by instance id plus the run's time window. Trace files are `G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/<map>_<instanceId>_<boss-slug>_<epoch>.ndjson`; DC's run record (one JSON line per run, written to `/azerothcore/env/dist/logs/dc/dc_testruns.jsonl` in the container, which is `G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/dc/dc_testruns.jsonl` on the host) carries `runId`, `instanceId`, `mapId`, `difficulty`, `pool`, `poolMembers`, `result`, `failReason`, `pullCap` and `bossPulls: [{n, startMs, endMs, outcome}]` (epoch ms; DC source `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-dungeon-clear/src/TestRun/DcTestRunRecord.cpp`). Reading the host file directly instead of `docker exec cat` is fine.
- The report keeps today's sections, split per pool: each pool's pulls (outcome, duration, deaths, hard mode), its baseline-versus-test comparison (`batch.py --split-at` over that pool's traces only), then the overall line. The scenario tokens come from a parallel DC lane, so keep `--scenario` free text.

Keep the server lock, dirty-tree check, rebuild and restart as they are. A sweep across several bosses is out of scope; name in the report what it would need.

Boundaries: no Docker, SOAP or server, and never run the runner against the live server. Cover parsing, pool pairing and trace matching with unit tests. Close per `CLAUDE.local.md` except the docs step (the orchestrator folds docs later): `PB_REPO=G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-e ~/.claude/scripts/pb-check <changed paths>` and `python -m unittest discover -s tools/raidtest/tests`, then commit on `rt/e` with message files named `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-e-<n>.txt` and `git commit -F`, no AI trailer. No merge, push, amend or rebase.

Report as your final message: commits; the CLI as it now stands; how pool pairing and trace matching work; decisions you made alone; every assumption about DC's replies or record that only a live run can confirm.
