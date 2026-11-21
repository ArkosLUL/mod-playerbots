# Lane N: Thorim's gauntlet and lever without a real player

You work in the mod-playerbots worktree `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-n`, branch `rt/n` (from Custom). Invoke the `raid-boss-strategy-recipe` skill before changing strategy code.

Today Thorim's gauntlet group walks only behind the raid leader, and no bot uses the lever. That works when the user leads, but not under the unattended test harness (mod-dungeon-clear, "DC"): there the bots' master is a headless GM bot off the map, and group slot 0, the arena main tank, leads, so the gauntlet never moves and every pull wipes at the 5-minute orb. The research behind this is `docs/plans/raid-test-runs/l-wave5-research.FINDINGS.md`, Thorim section ("Strategy blockers" and the master-reading sites it cites); the strategy lives under `src/Ai/Raid/Uld/` (Thorim files) with findings in `docs/raids/ulduar/thorim.md`.

The user approved this change (2026-11-21): while no real player is in the gauntlet group, the gauntlet tank walks the corridor route himself, the group follows him, and a gauntlet bot pulls the lever when it unlocks. With a real player leading, nothing changes. The hard mode (Sif present) needs the gauntlet done within 150 s of the lever, so the bot-led walk should not be slower than a led one; say what time it can reach.

Deliver the change, closed per `CLAUDE.local.md` in your worktree: `PB_REPO=<worktree> ~/.claude/scripts/pb-check <changed paths>`, durable facts into `docs/raids/ulduar/thorim.md`, commit with messages in files named `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-n-<n>.txt` and `git commit -F`, no AI trailer. It must hold at 10 and 25 man and in normal and hard mode, and change only Thorim's files unless shared code truly has to change (name it).

Boundaries: no Docker, SOAP or server; no merge, push, amend, rebase or branch. A DC lane adds Thorim's test row at the same time; you don't touch DC.

Report as your final message: commits; how a bot-led gauntlet and lever work and how they step aside for a real player; the expected gauntlet time against the 150 s clock; shared code touched; what only a live run can confirm.
