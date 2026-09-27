# Sim BiS dataset wave loop runbook

How the orchestrator session runs a wave, and the rules every work-item (WI) agent follows. Adapted from
wowsimwotlk's `docs/wave-loop/wave-loop.RUNBOOK.md`. Registry, WI specs, current wave and status are in
[sim-bis-dataset.PLAN.md](sim-bis-dataset.PLAN.md).

## Paths and branches

| Name | Path | Branch |
|---|---|---|
| [pb] | `G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots` | `Custom`, shared with other sessions |
| [int] | `G:\DevStuff\GitHub\azerothcore-wotlk-pb\build-bis-wt\int` | `bis-integration` |
| WI worktree | `G:\DevStuff\GitHub\azerothcore-wotlk-pb\build-bis-wt\wi-<id>` | `bis-<id>` |
| [reforge] | `G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-reforging` | its own repo; never written by git |
| [loop] | `G:\DevStuff\GitHub\.wave-loop\pb-bis` | args, results, check logs |

`build-bis-wt` sits under the core's gitignored `/build*/` and outside `modules/`, where the core CMake
would build a worktree as a second module.

## Standing authorizations

Given by the user when approving the plan on 2026-09-27. After a `/compact`, the user's "continue"
re-affirms them.

**Git** (orchestrator only; WI agents never write git state):
- Create `bis-integration` and [int] off `Custom`. Per WI: branch `bis-<id>` and its worktree off
  `bis-integration`; commit it after review; `git merge --no-ff` it into `bis-integration`;
  `git worktree remove`; `git branch -d`.
- Commit status updates, cross-review fixes and doc promotion on `bis-integration`.
- `git merge Custom` into `bis-integration` at wave start, and whenever the land fast-forward fails.
- After a green wave, `git -C [pb] merge --ff-only bis-integration`, with [pb] checked to be on `Custom`.
- Commit messages via `-F <file>`, subject ≤72, body ≤3 lines of why, voice per
  `use-conversational-language`, no AI attribution.
- mod-reforging gets no git writes: its patch stays uncommitted.
- Never push, amend, rebase, reset, clean, stash or force.

**Server:** the DB stays SELECT-only. No rebuild, restart, config change or Docker container start/stop:
the user rebuilds.

**Docs:** drafted with `/compact-docs-writer` and applied without a second ask.

## Wave procedure

1. **Start.** Read the PLAN's "Current wave". If `Custom` moved, `git -C [int] merge Custom` and run
   `int-check.sh`. Record the wave base SHA.
2. **Set up each WI.** `git -C [int] worktree add -b bis-<id> <wt> bis-integration`. `BIS-reforge-lock`
   gets none: it edits [reforge] directly, once `git -C [reforge] status --porcelain` is empty.
3. **Run**, only on the user's explicit go for this wave (auto mode shows no prompt). Write the args to
   [loop]`/wave<X>-args.json`, then `Workflow({script, args})` with this
   directory's `sim-bis-dataset.workflow.js` inline: the tool reads a `scriptPath` only in the session's
   working directories. Record the runId in the PLAN and wait for the notification. Save the results to
   [loop]`/wave<X>-results.json`.
4. **Integrate** green WIs one at a time, in registry order:
   1. `git -C <wt> branch --show-current`. Stage only owned paths, by explicit path. Commit.
   2. `git -C [int] merge --no-ff bis-<id>`. Resolve conflicts with `/resolving-merge-conflicts`.
   3. `bash [int]/docs/plans/sim-bis-dataset/int-check.sh <pre-merge sha> <name>`.
   4. `git -C [int] worktree remove <wt>`, `git -C [int] branch -d bis-<id>`. Update the WI's Status row.
      Commit.

   A red or blocked WI stays unmerged and carries forward. For `BIS-reforge-lock`, step 4 is only the
   Status row and the mod-reforging syntax check (`int-check.sh` runs it while [reforge] is dirty).
5. **Cross-review.** One Agent reviews `git -C [int] diff <wave base>..bis-integration`, plus
   `git -C [reforge] diff` when dirty, by hand at high effort. It fixes the findings and re-runs
   `int-check.sh`. Commit. The last wave reviews the whole effort from its base, `b6c4fa9ed`.
6. **Land.** Merge `Custom` again if it moved and re-check, then `git -C [pb] merge --ff-only
   bis-integration`. If [pb]'s local changes block it, report and stop.
7. **Report and stop.** Cover merged WIs and the findings fixed, carried WIs, doc changes, and whether
   the user must rebuild (new `.cpp` files also need a CMake re-run). Update "Current wave". Wait for
   "continue".

## Rules for WI agents

**Scope and git**
- Work only in your worktree, with absolute paths. `BIS-reforge-lock` works only in [reforge]`/src`.
  Never touch [pb], [int] or another WI's worktree.
- Edit only your owned paths. A change needed anywhere else goes in `contractChangeRequests`.
- No git writes. Leave everything uncommitted.
- Never build, restart or reconfigure the server, nor start or stop a Docker container. The DB is
  SELECT-only.

**Checks**
- Run exactly your WI's verify commands, from your worktree with `PB_REPO` pointing at it.
- Send long output to a file in [loop]`/logs/<id>/`, never inside a checkout, and read it with grep or
  tail: whatever you read stays in your context.
- `src/Bot/Engine/BuildSharedStrategyContexts.cpp` always fails the syntax check here (a case-insensitive
  mount artifact). Never block on it.
- The syntax check compiles against the build image's **unpatched** mod-reforging headers; playerbots code
  must compile against both the old and the patched mod-reforging API.

**Code and docs**
- Comments: invoke `use-conversational-language` before writing any, and follow the no-nonsense-comments
  rule. Never mention plans, waves or WIs in code.
- Docs: record durable facts, traps and decisions only in your WI's section of the PLAN, with
  `/compact-docs-writer`. `BIS-docs` promotes them; the orchestrator owns Status and "Current wave".

## Workflow contract

`args`:

```
{wave, baseSha, runbook, items: [{id, worktree, branch, specPath, specSection, ownedPaths, verify,
diffCmd?, notes?, priorReport?, stages?: [{label, notes, priorReport?}]}]}
```

- `branch` null means the item edits a checkout in place ([reforge]); then pass `diffCmd`.
- `priorReport`, a finished implementer's report from an earlier run, sends the item straight to review;
  on a stage, it skips that stage.
- `stages` replaces the single implementer with fresh agents run in order in the same worktree, each
  handed the earlier stages' reports.

Each item is a pipeline: implement (per stage if staged) → a fresh reviewer, by hand at high effort,
fixes every finding and re-verifies → if red, one repair agent and a second review. It returns
`{id, status: green|red|blocked, report}` with `report` =
`{status, summary, filesChanged, verification{commands, passed, tail}, contractChangeRequests, followUps,
findings{bugs, minor, cleanup}}`.

## Resume

After a `/compact` or restart:
1. Read this file and the PLAN's "Current wave": the wave, base SHA, runId and WI statuses.
2. A running Workflow: wait for its notification. One that was killed, or whose agents died: re-run the
   script inline for the unfinished items only, not with `resumeFromRunId`.
   - A finished implementer or stage (its `result` line in the run's `journal.jsonl`) goes in as
     `priorReport`, on the item or its stage. Write a long report to a file and let `priorReport` point
     at it.
   - An agent cut off mid-work keeps its uncommitted work. Its replacement gets a note saying what was
     already done, so it continues instead of starting over.
3. Continue at the first unfinished step.
