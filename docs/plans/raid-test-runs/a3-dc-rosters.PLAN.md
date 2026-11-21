Lane A3 of `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in mod-playerbots: read its Decisions and
"Facts the work rests on" sections first. Work in the mod-dungeon-clear (DC) clone at
`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-dungeon-clear`, on the already checked-out branch
`local/wrath-raid-tests`, which already carries lanes A1 and A2 (commits stamped
`Session: raid-test-runs A1` / `A2`: `TestRunsOnly`, `diff=`, Ulduar/ToC rows, scenario `preDone` and
start steps, re-pull in place, `pulls=`). Read that repo's `CLAUDE.md` and follow it, with two exceptions:
never create a branch or worktree there, and stamp commits `Session: raid-test-runs A3`.

Goal: each raid has a fixed, dedicated pool of characters, geared once, and every scenario fields the
same members for the comp its fight needs. Write every change generic, so it could later be offered
upstream.

1. Pools. A pool template per raid (format and location yours; the worldserver's working directory,
   beside the `dc_*` files, is fine) lists members by class, spec (`PremadeSpecName` names as the fork
   uses them), race and gender, plus the pool's target average item level and progression tier. The
   orchestrator fills the real templates later; ship a small example. `.dc test roster create <raid>`
   creates dedicated accounts `dcroster<raid><n>` (10 characters each; their names must never match
   `Playerbots.RandomBotAccountPrefix`, `rndbot` by default), creates the characters (reuse `src/TestRun/DcTestDriver.cpp`
   `TryProvision`'s account and character creation, or mod-playerbots'
   `RandomPlayerbotFactory::CreateRandomBot`), brings them to level 80, applies the spec's talents and
   glyphs, gears them once to the target average item level with mod-playerbots' own gear selection
   (`PlayerbotFactory::InitEquipment` and what `src/Util/DcBotProvisioning.cpp` already does; find what
   its gear limit measures and how to land on the target), gems and enchants, pet, ammo. It records the
   pool (names, guids, achieved average item level per member) somewhere it survives restarts.
   `.dc test roster gear <raid> <ilvl>` re-gears on demand; `.dc test roster list <raid>` prints the pool.
   All `Console::Yes`.
2. Progression tier. mod-individual-progression (`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-individual-progression`)
   gates content by a per-character tier, read as the highest rewarded hidden quest `66000 + tier`; the
   fork's notes are in `docs/systems/itemization.md` ("Reading a bot's tier", "Tiers → content") in
   mod-playerbots. Ulduar needs tier 14, Trial of the Crusader 15. Find how that module decides a
   character's tier at login (it stomps bot-account characters to 0/8/13 by level) and how it gates raid
   entry, then make pool characters hold their pool's tier across logins.
3. Comps. A scenario row carries a comp per size (10 and 25): tank count, healer count, and minimum
   counts of required classes or specs. `roster=<raid>` on `.dc test start` and `.dc test plan start`
   picks members from the pool to meet the scenario's comp for the run's difficulty, deterministically
   (the same members every run), and refuses with a clear message when the pool cannot meet it. Roles
   come from playerbots' `IsTank`/`IsHeal` on each member, not from slot 0/1 as `party=` does today. The
   run record carries pool, target item level and the picked members.

Add gtest cases beside the existing ones under `t/` for the pure logic (picking, template parsing).
Nothing here can be compiled or run, so read every declaration you call: mod-playerbots at
`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots` (a heavily diverged fork), core at
`G:\DevStuff\GitHub\azerothcore-wotlk-pb\src`. The orchestrator builds after the wave.

Never start, stop, build or exec into a Docker container, never call SOAP, never touch the live server.
Never push, amend, rebase or force. When done, stage only your files and commit with
`git commit -F <file>`, the message written to a file named `commit-msg-a3-<n>.txt` in
`C:\Users\boss2\AppData\Local\Temp\claude\4e3815b0e3fef148\fd2ca691-4a3e-4009-ba39-dc5273ee9f0c\scratchpad`.
Report: commits (hash, subject), the exact command syntax and template format, how the tier is held,
decisions you made alone, anything unfinished.
