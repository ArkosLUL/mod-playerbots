# Lane G: polish one boss strategy from test pulls

The workflow prompt handing you this brief names the boss, its trace slug, its raid (`uld` or `toc`), the round number, your mod-playerbots worktree and branch (under `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/`, branched from Custom), and the window report files holding its newest pulls. Work only in that worktree.

The effort is `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in your worktree: read "Polish loop", the decisions table and "Roster drafts". The orchestrator pulls each boss with nobody online on three different balanced rosters (pools `<raid>1`-`<raid>3`), Ulduar at 25-man hard mode and ToC at 25-man heroic. mod-dungeon-clear only handles logistics; the fork's raid strategy runs the whole fight. Each round you make one change set; the orchestrator merges it and rebuilds, and the next window's pulls judge it.

Your polish log is `docs/plans/raid-test-runs/polish/<slug>.md`: create it in round 1, and in later rounds read all of it first. Each round appends: the data read (run ids, trace files), deaths by mechanic per pull, the failure picked and its root cause with evidence (trace rows, file:line), the fix, what the next window should show if it works, and from round 2 on the verdict on the previous fix.

Each round:

1. Read the window report files the prompt names. Regenerate or widen them with `python tools/raidtest/raidtest.py report --boss <slug> --raid <raid> --head <sha> --pulls 10`, which only reads files on disk. Traces are `G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/<map>_<instance>_<slug>_<epoch>.ndjson`; read them with `tools/botobs/batch.py`, `tools/botobs/postmortem.py` and the boss's reader in `tools/botobs/bosses/`. If the boss has no reader and you need one, write it following the existing readers.
2. Check the stop rules first. If one window gave a kill on all three pools with at most 3 deaths in each pull, the boss is polished: change nothing and report so. If the log shows the same failure picked in three rounds and still present, stop: change nothing and report what was tried.
3. Judge the previous fix by the mechanic it targeted (did those deaths stop?), not by kills: a window holds too few pulls to read a kill rate.
4. Rank failures by deaths across all pools and pick the worst; take a second one only if it is independent and small. Root-cause it in the trace and the code.
5. Classify it. A bug (bots fail to do what the strategy already intends) you fix. A plan change (a new tactic, a different tank, kill or target order, new positions or assignments) you do not implement: write the question with your recommended answer into the log and the report.
6. Fix it with the `raid-boss-strategy-recipe` skill (invoke it). Every fix must hold for 10 and 25 man and for normal and hard mode / heroic. Change only this boss's files; if shared code must change, name it in the report, since every boss using it gets pulled again.
7. Close per `CLAUDE.local.md` in your worktree: `PB_REPO=<worktree> ~/.claude/scripts/pb-check <changed paths>`; durable facts into the boss's findings doc under `docs/raids/` (the polish log stays in the plan folder and is committed too); commit with messages in files named `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-<slug>-r<round>-<n>.txt` and `git commit -F`, no AI trailer.

Boundaries: no Docker, SOAP or server; never run the runner's `cycle` or `window`; no merge, push, amend, rebase or branch.

Report as your final message: status (`changed`, `polished`, `stuck` or `question only`); the previous fix's verdict; deaths by mechanic per pull in the data you read; what you changed and why, with commits; what the next window must show; questions for the user with your recommendation; shared code touched.
