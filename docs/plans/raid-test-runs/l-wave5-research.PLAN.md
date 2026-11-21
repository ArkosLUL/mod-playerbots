# Lane L: Wave 5 research (Flame Leviathan, Thorim, Algalon, Yogg-Saron)

Research only: no code changes. You work in the mod-playerbots worktree `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/rt-l`, branch `rt/l` (from Custom), and your one deliverable is `docs/plans/raid-test-runs/l-wave5-research.FINDINGS.md` in it, committed.

The effort is `docs/plans/raid-test-runs/raid-test-runs.PLAN.md`: read its decisions table, "Polish loop" and "Facts the work rests on". The orchestrator tests Ulduar and ToC boss strategies with nobody online: mod-dungeon-clear (DC, clone at `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-dungeon-clear`, branch `local/wrath-raid-tests`) logs in a 25-man roster pool, teleports it to a drop point near the boss, starts the encounter, and after a wipe revives, refreshes and re-pulls in place until a kill. A scenario row in `src/TestRun/DcTestDungeonRegistry.cpp` declares that: drop point, focus entries, start step (aggro, where the harness sets the boss on the tank, or a gossip chain on an NPC with optional clicker achievements, as `totc-beasts` does), `preDone` (boss ids set DONE on the fresh instance), `preDespawn` (trash removed before the first pull), `firstPullAfterS`, a `BossStateEquals(<id>, DONE)` success predicate, and comps per size. The DC driver is a headless GM bot that masters every roster bot but is not in the group and not on the map; slot 0 leads the group. The plan folder's `h-dc-wave4-batch1.PLAN.md` and `k-dc-wave4-batch2.PLAN.md` show what a normal row takes.

Four Ulduar bosses don't fit that shape. For each, find out what an unattended test would take:

- **Flame Leviathan** (vehicles: the raid must board Salvaged Demolishers, Siege Engines and Choppers; hard mode is the towers left standing).
- **Thorim** (the gauntlet run and the arena; hard mode is reaching him in time with Sif present).
- **Algalon** (summoned at the Celestial Planetarium console with a key item; one-hour despawn timer).
- **Yogg-Saron** (the Keepers' help comes from gossip; hard mode is fewer Keepers; Sara starts the fight; brain-room portals).

For each boss cover, with file:line references:

1. How the encounter starts and resets in the core (`src/server/scripts/Northrend/Ulduar/Ulduar/`, `ulduar.h` boss ids, spawns in `data/sql/base/db_world/` plus `data/sql/updates/db_world/`): prerequisites (other bosses DONE, instance data, items, achievements, game objects), what a wipe resets, and how hard mode is chosen.
2. What the fork's strategy expects (`src/Ai/Raid/Uld/`, findings in `docs/raids/ulduar/<boss>.md`), especially anything that reads the bots' master or needs a real player: the plan's "Facts" list the master-reading sites (Flame Leviathan's `master->GetVehicle()`, Thorim, Yogg-Saron), and the fork has a raid-leader helper beside `StagingLeader` in `src/Ai/Raid/ToC/Util/ToCHelpers_Staging.cpp`. Say how each site behaves under the DC driver (a self-mastered GM bot off the map).
3. What DC would need beyond what it has: new generic row features, a harness step, or nothing. Name the smallest set that works, and anything that can't be done unattended at all.
4. Drop point candidates with evidence (spawns, patrols, trash within 60 yd), comps at 10 and 25 from `c-research.FINDINGS.md` section 1, and the biggest risks.
5. A rough size: small (a row like batch 1), medium (one new generic DC feature), large (several, or fork changes).

End the file with numbered questions for the user, one per decision the user has to make (for example whether a boss is worth automating at all, or which hard-mode setup to test first). Write each question so it stands on its own: say concretely what the choice is between and why it matters, in plain words, then give your recommended answer. Invoke the `explain-in-simple-language` skill for these questions.

Prefer the repo's sources: the core scripts and SQL decide, the fork's docs describe the strategy. No web access is needed; if you use any, never Wowhead or noobtoboss (they forbid agent access).

Boundaries: no Docker, SOAP, server or database access; change no file except the findings file; no merge, push, amend, rebase or branch. Commit with a message in a file named `C:/Users/boss2/AppData/Local/Temp/claude/4e3815b0e3fef148/fd2ca691-4a3e-4009-ba39-dc5273ee9f0c/scratchpad/commit-msg-l-<n>.txt` and `git commit -F`, no AI trailer.

Report as your final message: the commit, a five-line summary per boss (start, what DC needs, strategy blockers, size, top risk) and the questions as written in the file.
