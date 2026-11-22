# Lane T: one code path for every raid at Yogg-Saron and Thorim

Work only in the mod-playerbots worktree `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-t` on branch `rt/t` (branched from Custom). The effort is `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` there: Ulduar and ToC strategies are polished from unattended test raids with no player in them, and the end goal is upstream, where real people lead raids of bots. User decision (2026-11-22): a strategy runs the same code whether or not a real player is in the raid, so the unattended tests exercise what real raids run; a real player only changes who does a job the player is doing.

Two features currently run only when no real player is in the raid; make each run in every raid:

1. **Yogg-Saron phase 1 walk-in.** `YoggSaronBotLedRaid` and `YoggSaronInPhase1Approach` (`src/Ai/Raid/Uld/Util/UldEncounter_YoggSaron.cpp` ~3440-3480) start phase 1 positioning at Sara's 90 yd (`ULDUAR_YOGG_SARON_P1_DOOR_RADIUS`) only for a bot-only raid, else at the room's 69.3 yd. Make the 90 yd approach apply to every raid, and remove the bot-led check where it only served this gate (station and leash triggers in `src/Ai/Raid/Uld/Trigger/UldTriggers_YoggSaron.cpp` ~52 and ~148, the walk guard in `src/Ai/Raid/Uld/Multiplier/UldMultipliers_YoggSaron.cpp`). Read why the gate existed in `docs/raids/ulduar/yogg-saron.md` ("A bot-led raid walks itself in") and keep the cloud walk guard on the walk.
2. **Thorim gauntlet lead.** `ThorimBotLedGauntlet` (`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` ~1409) lets a bot tank lead the gauntlet and a squad bot pull the lever only when no real player is on the instance; `ThorimGauntletLeader` otherwise returns the raid leader. Change the rule to: a bot leads and pulls the lever unless a real player is in the gauntlet squad, in which case that player leads (and the bots leave the lever to them). Callers are in `src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp` ~224 and `src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp` ~66; read how the gauntlet squad is assigned before deciding how "a real player in the squad" is tested. `docs/raids/ulduar/thorim.md` describes the gauntlet.

A third gate is being added by a running lane at Mimiron (phase 1 to 2 handover position, `rt/polish-mimiron-r2`); it is not on your branch and not yours to change.

Keep player-led behaviour sensible: a real player standing elsewhere must not be dragged along, and bots must not fight a player who is doing the job themselves. Every change must hold for 10 and 25 man and normal and hard mode. Rename helpers whose names no longer fit. Update the two findings docs so they no longer describe bot-only behaviour.

Close per `CLAUDE.local.md` in the worktree: `PB_REPO=<worktree> ~/.claude/scripts/pb-check <changed paths>`; durable facts into the findings docs; commit with messages in files `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-t-<n>.txt` and `git commit -F`, no AI trailer.

Boundaries: no Docker, SOAP or server; never run the runner's `cycle` or `window`; no branch, merge, push, amend or rebase.

Report as your final message: what changed per feature with commits, what a player-led raid now does differently at each, and what the next unattended window must show.
