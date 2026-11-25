# Raid polish loop: more pulls, shorter cycles, fewer tokens

## Context

The unattended Ulduar polish loop (`docs/plans/raid-test-runs/`, orchestrator session `fd2ca691…`)
works but is slow and token-heavy. Measured over windows 19-27 and all 117 polish rounds since 11-22:

- **A cycle takes ~98 min and runs its two halves one after the other**: lanes ~46 min (server idle),
  then rebuild + window ~55 min (lanes idle). Build is <5%; not worth touching.
- **Only 3 of 6 pool slots work**: `uld1-3` run, `toc1-3` had zero runs in 8 windows. Load is flat
  from 1 to 6 raids (snapshot-gap p95 307 ms). Host: 16 cores / 32 threads, 61 GB.
- **One pull per composition can't separate regression from chance**: per-pool kill rates differ a lot
  (Vezax uld1 5/8, uld3 2/8), repeats are coin flips; window 27's "regressions" are inside the noise.
- **Lead agent** (median 24 min, 31.6M cache-read tokens, peak context 431K): re-reads the whole polish
  log each round (20-28% of its cache reads by round 15-20, logs now 150-224 KB); reads the raw window
  report (median 82 KB, ~85% full boss-reader dumps); opens the 77 KB plan; spends ~53% of wall time on
  trace analysis through one-off scripts copied round to round (vezax: 365 scratch .py files).
- **Orchestrator**: every lane notification is cut at ~8K chars (report 8.7K + review 5K + 2.7K
  diagnostics), costing ~200 follow-up reads and inflating its context (8 compactions since 11-21).
- Runner overheads: a pool is held ~71 s after its run ends (20 s poll + 45 s settle, `window.py:203`);
  start-up re-parses every past trace of each boss (`pools.py` `Ledger.load` → `pulls.py` `read_facts`,
  memory cache only); the report step takes ~6 min at the window's end.

User decisions (2026-11-24): new pools `uld4-6` as **copies of A/B/C**; **6 pulls per boss per round**
(one per pool); **polished** = on one build, each composition (A, B, C) has a kill with hard mode
triggered and **at most 5 deaths**, out of its 2 pulls; **this session builds** on a branch and switches
the loop over at a break. Grilled the same day: **two fixed boss groups take turns** (not per-boss
events); **filler pulls** while the server waits, **run to their end** before a rebuild; **model and
effort unchanged** for leads and reviewers (revisit after the re-measurement).

Expected: per-boss round ~75 min instead of 98, with 6 pulls instead of 3 plus filler pulls (~2.6x
pulls per hour per boss); late-round lead cache reads down ~30-40%; notifications no longer cut.

Re-measured 2026-11-24 over windows 29-35 and 14 lanes (before → after): pulls per hour ~8 → 30; run
end to the pool's next start 71 → 13 s; a boss's round every ~100 → 63-97 min, the server pulling ~86%
of the time; lead median cache reads 44M → 32M (28.5M without the four log-migration rounds), peak
context 502K → 421K, polish log read 161K → ~20K chars; lane result 14K (cut) → 2.4K chars; the
orchestrator 2.8M → 5.7M cache reads per lane. Unchanged: lead wall time ~24 min, ~90% of it writing
~132K output tokens; the reviewer at 10 min, with a real code fix in 4 of 14 rounds. Missed: digests
15-32 KB (Yogg-Saron's six phase summaries are 20 KB of 31), ~6 scratch scripts per lead. Lanes are the
bottleneck: a group waits for its slowest lane, and filler 35c ran 15 min past the last merge.

## A. Runner — worktree `build-toc-wt/rt-loop-speedup`, branch `rt/loop-speedup` from Custom

1. **Pools.** `pools.py:20` → per-raid count (`uld` 6, `toc` 3); composition of a pool = A/B/C by
   `(n-1) % 3` (uld1/uld4 = A …). Update `raidtest.py:4,13,454,494` help text, `report.py:24` load
   reference ("one to six"), `tests/test_cycle.py:30,177-191`.
2. **Free a pool when its run leaves `.dc test status`** (`window.py` `plan()`): don't wait for the
   record and settle; read the record and traces later as now. First read DC's run teardown to confirm a
   new run on the same pool is accepted at once (members logged out); if not, wait only for that.
3. **Per-job report as soon as the job completes** (no pool wants it, no flight of it left), written to
   `--report-dir/<label>-<boss>.md` with a `job N done: <file>` log line; replaces the orchestrator's
   `split_window.py`. The end-of-window report keeps only the header table and the load line.
4. **On-disk cache of `TraceFacts`** for closed traces, keyed by path + size + mtime + a version stamp
   built from the sources `read_facts` depends on (`pulls.py`, `raidobs/validity.py`, `raidobs/trace.py`);
   used by `Corpus.facts`. Cuts start-up (2-3 min) and report time.
5. **Digest report** (window default; `report --full` keeps today's output), target ≤15 KB per boss
   for 6 pulls:
   - pulls table with pool and composition, outcome, duration, hard mode, deaths;
   - deaths: per pull the deaths up to the wipe cascade in full, the rest as counts by killing ability;
     one table of deaths by mechanic across all pulls;
   - stop-rule line per composition (best pull: kill, hard mode, deaths ≤5);
   - trend table: this build vs the previous 3 builds of the boss (pulls, kills, median deaths, top 3
     death mechanics, per composition), replacing the 1-vs-1 `--split-at` section (`report.py:247-254`);
   - boss reader run with `--summary` per pull instead of every section (`report.py:256-259`).
6. **`--summary` in `raidobs/cli.py` `run_sections`**: banner plus the sections a reader lists in a
   module-level `SUMMARY` tuple; banner only when it lists none. Mark 1-3 sections ("what killed us,
   when, which phase") in the active readers: `yogg_saron.py`, `mimiron.py`, `general_vezax.py`,
   `thorim.py`, `algalon.py`.
7. `docs/plans/raid-test-runs/window.sh`: pass `--report-dir "$OUT"`.
8. Tests under `tools/raidtest/tests` for 1-5 (pool defaults and compositions, early release, per-job
   report timing, cache invalidation on a changed source, digest sections); one fresh reviewer agent on
   the branch before the merge.

## B. Lane script — `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/raid-polish-lane.js`

1. `LEAD_SCHEMA`: replace `report` with `summary` (≤1500 chars: status, previous fix verdict per
   composition, change + commits, questions with recommendation, shared code). The full round write-up
   lives in the polish log already.
2. Reviewer gets a schema (`commits`, `summary` ≤800 chars). Its prompt: the brief, the log's State
   section and newest round, `git diff Custom...<branch>`; drop the plan file.
3. The script returns only `{status, commits, summary, review: {commits, summary}}` (<3K chars).
4. Measure before adopting: a custom agent type (`agentType`) with a tool allowlist (Read, Edit, Write,
   Bash, PowerShell, Grep, Glob, Skill) to drop ~68K chars of unused tool schemas from every call. Spawn
   one test agent, compare its first-call context with a current lead's (~46K tokens fixed) and confirm
   CLAUDE.md files and skills still load; adopt only if both hold.

**Built (2026-11-24):** A on branch `rt/loop-speedup`. B as
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/raid-polish-lane.next.js` (swapped in at the
switch; it takes an optional `args.agentType`). B.4's type is `~/.claude/agents/raid-lane.md`, measured
2026-11-24: its first call is 29.5K tokens against a lead's 46K, with the `CLAUDE.md` files, the user's
rules and the skills all loaded, so lanes pass it; a session sees only agent types that existed at its
start. Offline digests on
window 27's pulls (3 per boss, build `0687fa6a4`): Yogg-Saron 15.7 KB (window report 182 KB),
Mimiron 8.6 (116), Thorim 9.7 (35), Vezax 5.8 (90); Yogg-Saron's `phases` summary is ~3.5 KB of a pull's
share. Trace-facts cache: cold report 56 s, warm 6 s, ~0.5-1 MB per boss under
`%TEMP%/raidtest/facts/<code stamp>/`.

**Behaviour changes to know:** a pool goes to the next job as soon as its run leaves status, so a job's
retry after an invalid pull waits one pull of the next job on that pool; a stale build (older than a
job's head) stops the runs of every job it fails. A start refused because a member is still logged in
is retried for 60 s. `--report-dir` must be new or empty (a rerun needs a new label); a report that
fails still leaves its file, naming the error and the rerun command; the lock is released before the
window waits for its report processes. The done rule reads only the newest build's pulls.

**For the raid-test-runs close-out** (`docs/systems/`): the reader `--summary` flag and `SUMMARY`
tuple, the digest's sections, the facts cache and its code stamp, `window --stop-file`.

## C. Lane brief — rewrite `docs/plans/raid-test-runs/g-polish.PLAN.md` (through `prepare-prompt`)

- Self-contained: carries every loop rule a lane needs (fix authority, judging by mechanic, the new done
  rule, the three-round stop, shared code, one code path, no comments, pools A/B/C x2, difficulty), so
  lanes never open `raid-test-runs.PLAN.md`.
- Read order: the log's `## State` + newest round, then the digest; the history file only by grep; the
  full reader only for the failure picked, by section flag.
- Judge per composition across its 2 pulls plus the trend table; calling a regression needs the trend.
- Done rule as decided; the 10-man check (one pull per pool) uses the same per-composition rule.
- Recurring analysis goes into `bosses/<boss>.py` as a section (summary-marked if it answers "what
  killed us"), never a scratch script copied forward.
- Log protocol: the first round under this brief reads the whole log once, writes `## State` (≤12 KB:
  fight model, per-composition status, open failures ranked, fixes in place with verdicts, rejected
  ideas, user answers in force) and moves every round to `polish/history/<slug>.md`. Each later round
  updates State, moves the previous round to history, and writes its own round below State.

## D. Loop orchestration — for the loop session at the switch

First step after approval: save this plan as `docs/plans/raid-test-runs/u-loop-speedup.PLAN.md`
(the folder's lane-plan naming). This section is what the loop session follows from the switch on:
- **Two boss groups take turns**, balanced by fight length: group 1 = Yogg-Saron + Thorim (~21 min
  of pulls), group 2 = Mimiron + Vezax (~20 min; Algalon joins it if it comes back). Rebalance when a
  boss is done. Cycle: group 1's window runs on build *n* (one pull per boss per pool) while group 2's
  lanes code; once all of group 2's lanes have merged: one rebuild, group 2's window, while group 1's
  lanes code; and so on. A boss whose lane changed nothing stays in its group's window (more data).
  In a group window the boss whose lanes run longer goes first (on 11-24 Algalon before Mimiron,
  Thorim before Yogg-Saron), so the group's last lane ends sooner; pulls no lane waits on (10-man
  checks, extra pulls) go after the group's jobs.
- **Lanes start per job**: as each boss's report file appears, launch its lane; don't wait for the
  window's end. `window.sh` passes `--report-dir <out>/<label>`; the window's log prints
  `job N (...) done; report goes to <file>` and the file appears whole once written, so
  `watch-reports.sh` under Monitor is the trigger. `split_window.py` is no longer needed.
- **Filler while the server waits** for the other group's lanes: first pending 10-man checks, then
  done bosses whose shared code changed, then extra pulls of the group that just ran on the same build.
  Running filler pulls finish before the rebuild; no new filler pull starts once the next build is
  ready: section E's stop file. Never restart with runs in flight. Raids in flight, build ahead and job
  order: section E.
- Questions to the user: one numbered batch per full cycle (after both groups reported), never per
  lane.
- Read lane summaries only; open a polish-log round only when a question needs it (`lane_tail.py`
  retired).
- Plan file: move window narratives from "Resume here" to `docs/plans/raid-test-runs/windows.md`
  (append-only, a few lines per window); keep "Resume here" ≤40 lines; collapse done bosses' table rows
  to one line; update "Polish loop", decisions table ("three pools", done rule, "3 valid pulls").

## E. Ten copies per roster (user, 2026-11-25)

Supersedes the 2026-11-24 pool count and done rule above.
- **Pools:** `<raid>7`-`<raid>30` copy `<raid>1`-`<raid>3` (pool n is design A, B or C by (n-1) mod
  3), each created, its death knights respecced and its professions applied like `<raid>4`-`<raid>6`.
  Name prefixes: Ulduar Uldb-Uldt and Uleb-Ulef, `uld14` Uleg; ToC Tocb-Toct without Toch, then
  Tcrb-Tcrg. A prefix ending in the first suffix letter (Ulda + aa) breaks the core's
  three-equal-letters rule, and Uldi and Toch clash with random-bot names.
- **Done:** on one build every pool has a valid pull and at least 80% of each composition's pulls (8 of
  10) are clean: a kill with at most 5 deaths, for Ulduar with hard mode triggered. The 10-man check
  by the same rule (provisional). Each done boss gets one re-check job on all pools as filler; a
  failing one rejoins the loop (provisional).
- **Server:** `--max-raids 30` across both raids, so a window with Ulduar and ToC jobs runs them in
  priority order; the runner starts at most 6 runs per 10 s poll, so bot logins and DC's
  realm-wide pre-pull refresh (~10 s per 25-man raid) come in waves. `AC_MAP_UPDATE_THREADS` 24 (was 6,
  `configurationOverrides/ServerPerformance.env`); RaidObs cap `AC_PLAYERBOTS_OBS_MAX_DIR_MB` 204800
  (was 51200, `Playerbot.env`): traces run ~21 MB a pull. Switched in one go, no stepped load check
  (user). At 6 raids the worldserver used 1.1-1.65 of 32 cores (6 map threads ~25% busy each, world
  tick p95 12-19 ms) and 5.4 of 47 GB. If a window overloads (load-line p95 well above ~310 ms, pulls
  voided for freezes, DC spawn timeouts), lower `--max-raids` for the next one; no code change.
- **Digest at 30 pulls:** each pull's first 48/pulls deaths in full (2 to 8); boss-reader `--summary`
  only for each composition's pulls with the fewest and the most deaths; the trend counts
  clean/kills/pulls per composition.
- **Build ahead** (the server sat ~5 min idle per rebuild: ~2 min until the build started, ~2.5 min
  build, ~1 min restart): once every lane for the next build has merged, run
  `bash docs/plans/raid-test-runs/build.sh <out> <label>` while the window still runs, and write the
  next jobs file. Merge nothing until it returns (`merge-lane.sh` refuses meanwhile; DC hand merges
  wait too). It records the commits of the three trees it built; `window.sh <jobs> <head> <out> <label>
  prebuilt` then only restarts, and refuses if a tree moved since (run it without `prebuilt`).
- **Job order:** polish jobs first, by the lane rule in section D; then filler (pending 10-man checks,
  done-boss re-checks, extra pulls), longest runs first, so every window ends on short fights. Median
  run minutes, 25-man: Anub'arak 13.4, Yogg-Saron 12.5, Mimiron 10.8, Razorscale 10.5, Beasts 9.5,
  XT-002 9.4, Flame Leviathan 8.7, Faction Champions 8.5, Algalon 7.6, Freya and Thorim 7.2, Assembly
  6.5, Jaraxxus 6.4, Vezax 6.2, Auriaya 5.6, Twins 5.5, Hodir 4.3, Ignis and Kologarn 4.1. Keep filler
  queued so the server never waits for merges; `window.sh` passes `--stop-file <out>/<label>.stop`:
  touch it once the build ahead is done and every polish job has reported, then restart when the
  window exits.

## Switch at a break (this session, with the user)

When A-C are reviewed and committed, I tell you; the switch happens during the loop's lane phase (no
window running, loop session not merging):
1. Take the server lock (`G:/DevStuff/GitHub/.wave-loop/locks/server` + owner file).
2. Copy `env/dist/logs/dc/dc_roster_uld{1,2,3}.conf` to `uld{4,5,6}` with free name prefixes; over
   SOAP `.dc test roster create uld4` … `uld6` one at a time; `.dc test roster list` and
   `.dc test roster respec` to confirm 27 members, tier 14, ilvl ~226, DKs 71/71.
3. `--no-ff` merge `rt/loop-speedup` into Custom; move `raid-polish-lane.js` to
   `raid-polish-lane.prev.js` and `raid-polish-lane.next.js` to `raid-polish-lane.js`; release the lock.
4. I hand you a short prompt (written through `prepare-prompt`) to paste into the loop session: read
   `u-loop-speedup.PLAN.md` section D, restructure the plan file, run the next window on `uld1-6`.

## Assumptions (provisional, the user may overturn)

- The 10-man check uses the same per-composition rule (one kill, hard mode, ≤5 deaths each of A/B/C).
- Each active boss's own next lane migrates its polish log to State + history (it knows the fight; doing
  it here would mean reading ~900 KB of logs).
- The trend table compares this build with the previous 3 builds.
- Lanes get the digest; the full reader is run by the lead, section by section, only for the failure
  it picked.
- ToC gets `toc4`-`toc6` too (user, 2026-11-24: Anub'arak is not polished yet), copies of
  `toc1`-`toc3` at ilvl 245 with name prefixes Tocu/Tocv/Tocw; Ulduar's copies use Uldu/Uldv/Uldw.

## Verification

- `PB_REPO=<worktree> ~/.claude/scripts/pb-check <changed paths>`; `python -m unittest discover -s
  tools/raidtest/tests` in the worktree.
- Offline digest on window 27 data: `raidtest.py report --boss yogg-saron --raid uld --head 0687fa6a4
  --pulls 10` → ≤15 KB (today 182 KB) and still shows deaths by mechanic and phase timings; same for
  mimiron, vezax, thorim. Run it twice: the second (warm cache) start-up must be much faster.
- First window after the switch: 6 raids in flight, load p95 ~307 ms, no pull voided for a freeze;
  run end → next start on a pool ≤15 s (was ~71 s); per-job report files appear as jobs end.
- Next lane round, from its transcript: polish log + report reads ≤30 KB together, notification not
  cut. Re-measure lead cache reads the same way as the 11-24 baseline after ~10 rounds.
