# Unattended raid test runs on mod-dungeon-clear

Lane briefs sit beside this file as `<lane>.PLAN.md`, written when their wave is prepared.

## Resume here

The polish loop runs on the user's standing go ("Polish loop" below), scheduled by `u-loop-speedup.PLAN.md` section D since 2026-11-24: two boss groups take turns, six pools per raid, a report file per job. Window history, merges and past question batches: `windows.md` (append a few lines per window). Each boss's state: `polish/<slug>.md` (`## State` and the newest round; older rounds in `polish/history/<slug>.md` once its first lane under the new brief migrated it).

**Done** (polished at 25 plus the 10-man check): Jaraxxus, Twin Val'kyr, Northrend Beasts, Faction Champions, Kologarn, Ignis, Razorscale, Auriaya, Iron Assembly, XT-002, Hodir, Freya ("Bosses" below).

**Groups:** group 1 = Yogg-Saron (round 21 next) and Thorim (r22); group 2 = Mimiron (r20 running) and Algalon (r10 running; it fell out of the loop by oversight after window 21 and came back in window 30). **Vezax polished at 25n** in window 30; its 10-man check is window 31's last job. Rebalance when a boss is done.

**Window 31** (rebuild of the commit recording this, started 2026-11-24 ~19:40): group 1 on Yogg-Saron r20 and Thorim r21, then Vezax 10n; jobs `scratchpad/window31-jobs.json`, log `scratchpad/window31/window31.out`, reports `scratchpad/window31/window31/window31-<boss>.md`. Windows 29-30 results: `windows.md`.

**Next:**
- Launch each group 1 lane (Yogg-Saron r21, Thorim r22) as its report file appears; Vezax's 10n report decides whether Vezax is done (the same per-composition rule).
- Merge Mimiron r20 and Algalon r10 as they finish; once window 31 has ended and both have merged: rebuild, window 32 = group 2 while group 1's lanes code; filler pulls while the server waits (section D).
- Report each window in chat; one numbered question batch per full cycle, after both groups reported.

**Open batch** (sent with window 28's launch, unanswered; build nothing from it before the answers; recommendation in brackets):
1. Vezax: every healer priest holds its external for the late Surge, not one (keep).
2. Vezax r17's three readings: the +5 s grace, the hold until the bot targets the Animus, potions wait (keep all).
3. Yogg-Saron Q1: redo the phase 1 cloud geometry from the measured orbits (15.3/20.9/27.4/41.4/48.4/54.1 yd, 2.0 yd/s), in two steps (yes).
4. Yogg-Saron Q2: the Diminish Power break fires once the bots carrying that Crusher's stack are below, not the whole team (yes, one window).
5. Thorim, finishing at ilvl 226: (a) close on mechanics like Hodir, polished once hard-mode phase 2s lose nobody to a mechanic before the first tank death and normal-mode pulls are kills with at most 3 deaths, then the 10-man check ((a); asked under the old 3-death rule).

**Not asked yet:** Algalon second taunter (Q4 b); Iron Assembly self Lay on Hands.

**Mechanics:**
- Lanes: create the branch (`git -C build-toc-wt/rt-polish-<slug> switch -q -c rt/polish-<slug>-r<n> Custom`), then `Workflow({scriptPath: "G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/raid-polish-lane.js", args: {kind: "polish", tag, wt, branch, boss, slug, raid, round, reports: [<report file>], note}})` with no `agentType` (`~/.claude/agents/raid-lane.md` is untested). The note ends: "<Boss> only; every change must run the same with or without a real player in the raid. Raid code comments were stripped on Custom (c163ec11a): write no code comments." A lane returns `{status, commits, summary, review: {commits, summary}}`. Other kinds (`dc`, `strategy`, `research`, `runner`) take a `brief`; the old script is `raid-polish-lane.prev.js`.
- Windows: with a rebuild `bash docs/plans/raid-test-runs/window.sh <jobs.json> <head> <out> <label>`; without one `python tools/raidtest/raidtest.py window --jobs <file> --head <sha> --max-raids 6 --skip-rebuild --report-dir <out>/<label> --owner "raid polish loop orchestrator: <label>"`. Every window gets a new label (a report folder already holding reports is refused). Jobs leave `pools` out (all six). `--head` is never newer than the commit the running binary was built from: a pull counts only if its binary was built after the head's commit time, so a newer head fails every job as a stale build.
- Merges: `git merge-tree --write-tree Custom <branch>`, then `git merge --no-ff -q -F <file> <branch>`; `git diff HEAD^1 HEAD -- src | grep -cE '^\+\s*(//|/\*)'` must print 0. A lane appending to a polish log the orchestrator also changed conflicts at the file's end: merge Custom into the lane branch and keep both.
- Any view of past pulls: `raidtest.py report --boss <slug> --raid <raid> --head <sha> --pulls <n>`. Combat reach audit: `reach-audit.FINDINGS.md`.

## Bosses

Earlier per-boss detail: the old table in `windows.md`.

| Boss | State |
|---|---|
| Jaraxxus | done: 25h kills with 0/0/1 deaths, 10h kills with 0 |
| Twin Val'kyr | done: 25h kills with 0 deaths (window 6), 10h kills with 0 (window 7) |
| Northrend Beasts | done: 25h kills with 1/3/3 (window 7, r3), 10h kills with 0 (window 8, r4) |
| Faction Champions | done: 25h kills with 2/0/3 (window 24), 10h kills with 0/1/0 (window 24b) |
| Kologarn, Ignis | done: polished at 25n on r1, 10n checks in window 7 (kills, 0 deaths) |
| Razorscale | done: 25n kills with 0 deaths (window 7, r1), 10n kills with 0 (window 8) |
| Auriaya | done: 25n kills with 0/0/1 (window 8, r2), 10n kills with 0 (window 9) |
| Iron Assembly | done: 25n hard-mode kills with 2/3/3 (window 14), 10n 1/1/2 (window 13b) |
| XT-002 | done: 25n hard-mode kills with 0/1/1 (window 14, r11), 10n 0/1/0 (window 13b) |
| Hodir | done: polished at 25 without the cache (the user's call), window 16 on r9: kills with 0/1/0; 10n kills with 0/1/2 |
| Freya | done: 25n hard-mode kills with 1/2/2 (window 25, r17-r18), 10n hard-mode kills with 0/0/0 (window 25b) |
| Yogg-Saron | group 1, round 21 next |
| Thorim | group 1, round 22 next |
| Mimiron | group 2, round 20 running |
| Vezax | polished at 25n (window 30 on r17: hard-mode kills with 0/0/0/0/1, one wipe); 10n check in window 31 |
| Algalon | group 2, round 10 running (back from window 30); the user: polish mechanics on the 226 rosters, no kill expected, the stop rule waived |
| Anub'arak | needs a way to bring him back after a wipe (only a player entering the map respawns him; see DC's `totc-twins` row comment); brief a DC lane |
| Flame Leviathan | parked by the user |

## Status

| Step | State |
|---|---|
| Wave 1: env changes (`Playerbot.env`: `+dungeon clear` dropped, Obs retention 60 d / 51200 MB, `AccountInstancesPerHour` 100) | done 2026-11-21, live at next restart |
| Wave 1: `tools/raidtest/.env` gitignored | done 2026-11-21 |
| Wave 1: DC clone + local branch | done: `modules/mod-dungeon-clear`, `local/wrath-raid-tests` at `142732c` |
| Wave 1: SOAP account | done: `RAIDTEST` (GM 3, all realms), login in gitignored `tools/raidtest/.env` |
| Wave 1: DC conf | none needed: the container entrypoint installs `mod_dungeon_clear.conf.dist` into the bind-mounted `env/dist/etc/modules`; settings go through `AC_DUNGEON_CLEAR_*` env vars |
| Wave 1: DC compat | done: `ea7f427` (local `RollUniqueCheck`; the only one of 207 DC TUs that failed), `d7605fe` (driver account read-back race latched a refusal) |
| Wave 1: restart | done; env confirmed (Obs 60 d / 51200 MB, instances 100, `+worldbuff` only) |
| Wave 1: 5-man headless smoke | `tr-20261121-101742-1` (uk) failed at spawning_bots and crashed the worldserver. (1) Fork `9c8bdbf1d`: `PlayerbotHolder::FindMasterByAccount` finds a self-mastered bot master, so driver-run bots log in under the driver's PlayerbotMgr. (2) DC `1d44289`: `UnbindFromMap` unbinds only binds the core has loaded (`playerBindStorage[guid]` is null otherwise and `PlayerUnbindInstance` segfaults); verified by `tr-20261121-130527-3`, whose teardown no longer crashes |
| Smoke after wave 2 | random-draw runs `tr-20261121-130020-1`, `-130317-2` still fail at spawning_bots: the driver's PlayerbotMgr is only serviced from `OnSessionUpdate`, which a headless master never gets, so its bots' teleport acks and packets are never drained. Fixed: fork `a664c7951` (`PlayerbotsMgr::UpdateHeadlessMasterSessions` in the world `OnUpdate`), DC `0714513` (spawn timeout names each missing slot). **Smoke passed** `tr-20261121-133435-1`: headless random-draw uk run, 5/6 bosses then a wipe on Ingvar, clean teardown, record on the host. The masterless `party=` path, which `roster=` runs use, also passes spawning (`tr-20261121-130527-3`; level-1 characters then fail teleporting) |
| User review 2026-11-21 | answered; recorded in the decisions table (Rosters, Comps, Difficulty, No-player logout, DC pre-boss muster). Open: the three roster drafts (identical or varied), then Wave 3 go. Parallel runs need a one-time load check (one 25-man fight alone vs next to a second) |
| Game port | done 2026-11-21: Windows had reserved TCP 8043-8142 (Hyper-V/WinNAT; the dynamic range starts at 1024). Fixed as admin: stop winnat, `netsh int ipv4 add excludedportrange protocol=tcp startport=8085 numberofports=1` (persistent), start winnat. `.env` `DOCKER_WORLD_EXTERNAL_PORT` back to empty (8085) and `acore_auth.realmlist` port back to 8085. 3724 and 7878 reserved the same way (servers stopped first, since a bound port may not take the exclusion); 8085, 3724 and 7878 answer |
| DC result files | done: `Playerbot.env` sets `DC_TESTRUNS_FILE`, `DC_TESTRUN_LIVE_FILE`, `DC_TESTPLANS_FILE`, `DC_LAB_TRACE_DIR` and `AC_DUNGEON_CLEAR_TEST_RUN_ROSTER_DIR` under `/azerothcore/env/dist/logs/dc` (host `env/dist/logs/dc`), plus `AC_DUNGEON_CLEAR_TEST_RUNS_ONLY=1`. The runner's default `--dc-runs-file` (`/azerothcore/dc_testruns.jsonl`) must move there in wave 3 |
| Wave 2 (lanes A1-A3, B, C) | done 2026-11-21. DC `bcece6a`+`b4aa733` (A1), `a67164f`+`09cef94` (A2), `2e80368`+`ea39b4f` (A3); fork `rt/b` merged as `df7986351`; findings in `c-research.FINDINGS.md`. The capped build compiled all of it; DC's gtests have not run (they need a full `BUILD_TESTING` build) |
| Wave 3 (pilots) | go 2026-11-21: three different rosters. XT-002 drop (815.0, -10.0, 409.803, o 0.0) navprobe-clean, ~37 yd from his patrol (path 1360540) and ~55 yd from scrapyard trash; Beasts drop at Barrett (559.15, 90.56, 395.27). Lanes D and E done (workflow `wf_85538f2e-272`). DC `88b1e68` (scenarios `uld-xt002`, `totc-beasts`: boss-state success predicate, re-pull gated on "success is the kill", multi-NPC gossip with `clickerAchievements` 3916, `summonsFocus` keeps DC held through Beasts pulls so the fork's ToC staging positions the raid and DC's muster does not run there, a map-649 roster anchor on Icehowl), `d25d9ec` + `a0a97bc` (parties of five in slot order for every raid run; Icehowl's early kill credit is not a kill). Templates `dc_roster_{uld,toc}{1,2,3}.conf` in `env/dist/logs/dc` (prefixes Uldx/y/z, Tocx/y/z). Fork `580a1e13a` merges `rt/e`: `raidtest.py cycle --raid uld|toc` runs pools `<raid>1-3` in parallel, 1 valid pull per pool, cap 2, pairs baseline per pool, matches traces by instance id, reads the run log from the host. Pools created 2026-11-21 (6 x 27, tanks read as tanks incl. bear and blood DK); one member in four pools came out naked: DC `RestoreGear` gated the best roll's re-equip on the core `CanEquipNewItem`, which refuses while the bot casts or is stunned; fixed in DC `4c6866f` (equip the snapshot directly, fail the member if the restore comes back short), pools re-geared. Pilot single pulls: `tr-20261121-154253-1` XT-002 25n hard mode on `uld2`, harness clean end to end, wipe at 5.5 min (first death Heart at 3:17, then XT 4:16-5:00); `tr-20261121-154933-2` Beasts 25h on `toc2`, gossip start with achievement 3916 granted, **kill, 0 deaths**, 8 min. Load baseline (solo XT pull, RaidObs snapshot gaps, target 250 ms): mean 250, p95 318, p99 330, max 445. First runner cycle (XT-002 25n, `uld1` + `uld3` in parallel; `uld2` reused the pilot pull): harness and report end to end, but both pulls carried DC veto rows inside the fight (`dungeon clear zeroed xt002 set dps priority action` x19, `dps assist` x19) and scrapyard trash joined: an aggro start released DC, whose pull logic and multipliers then ran alongside the strategy (the Beasts pilot, DC held throughout, had zero vetoes). Fixed: DC now stays held through every pull of every re-pulled row, an aggro start sets the focus boss on the tank (`EngageWithTarget`), and DC's pre-boss muster no longer runs (supersedes the "DC pre-boss muster" decision); the runner never counts a pull with a DC veto inside the fight. Load check passed: two parallel 25-man fights read the same as one (snapshot gaps p95 322 / 323 ms, max 342, none over 400), and three read the same again (p95 322-323, max 348). DC `d47634c` (hold through every pull, aggro start = `EngageWithTarget` on the tank), `e9f488e` (only a boss-state DONE predicate re-pulls: with DC held its all-cleared never fires, so a kill on `uld3` ran into `no_progress`; XT-002 row now `BossStateEquals(3, DONE)`); fork `937240174` (runner drops a pull with a DC veto inside the fight). **XT-002 25n hard mode pilot done:** 6 clean pulls, no DC vetoes, kill ends the run: uld1 wipe 4:31 / kill 3:28 (0 deaths); uld2 wipe 3:13 / wipe 6:04; uld3 kill 2:44 (0 deaths) / wipe 7:53. **Northrend Beasts 25h pilot done:** 3/3 kills, no DC vetoes: toc2 0 deaths 8:01; toc1 2 deaths (ranged, Slime Pool at 5:42) 8:09; toc3 6 deaths (4 melee to Dreadscale's Molten Spew at 4:20, 1 Slime Pool, a tank to Icehowl at 5:27) 8:40. Wave 3 complete; the cycle works for both pilots (`raidtest.py cycle --boss xt-002 --scenario uld-xt002 --raid uld`, `--boss northrend-beasts --scenario totc-beasts --raid toc`) |
| Polish loop | go 2026-11-21 (standing, see "Polish loop"). DC rule 2 "never use worktrees" removed on our branch (`4aa501b`). Round 1: lane F (runner `window`), polish XT-002 r1, polish Northrend Beasts r1, lane H (Wave 4 batch 1) |
| One code path (user, 2026-11-22) | a strategy runs the same code with or without a real player in the raid, so the unattended tests exercise what real raids run; a real player only changes who does a job that player is doing. Done: lane T merged (`b40af8aab`): Yogg-Saron's walk-in from 90 yd runs in every raid; Thorim's gauntlet is led by the first real player in the gauntlet squad (at the gate or past the portcullis), else the bot tank, and no bot touches the lever while a player leads; review fixed the gauntlet tether staying on in phase 2 and a mid-corridor takeover walking back. Mimiron r3 merged (`8f7957811`): the handover hold runs in every Firefighter raid, with `follow` held only until phase 2. Lane reviewers now flag a bot-only branch. A player-led check of all three needs the user in the raid |
| Wave 4 (remaining bosses) | in batches inside the polish loop; batch 1 = lane H, merged as DC `8ef511f` (rows `totc-jaraxxus`, `totc-twins`, `uld-ignis`, `uld-kologarn`; new row fields `preDespawn`, `firstPullAfterS`); Ignis and Kologarn drops navprobe-clean |
| Window 1 (2026-11-21) | build `d8078b2bf` (Beasts r1 fix), six raids at once: Beasts 25h 4 kills / 2 wipes (toc3), XT-002 25n 4 kills / 2 wipes. Load p95 336 ms, but every DC run start and wipe reset froze the whole server ~6.5 s (`RefreshParty` runs `maintenance` on 25 bots in one tick), hitting the other raids mid-fight. Runner `dfa51a8b7` voids a pull with a freeze over 2 s; lane I (`i-dc-refresh-spread.PLAN.md`) spreads the refresh |
| Window 2 (smoke, build `14318f5fe` + DC `2b846f6`) | the new rows compile. XT-002 with the trash despawned (16 removed): kill, 0 deaths. Kologarn: wipe 2:56. Ignis: wipe 3:17 (8 trash removed; a 10 s freeze voided it). Beasts toc3: kill. **Jaraxxus and Twins never started**: stuck in "preparing pull 1" until no_progress, no start line; lane J (`j-dc-toc-stage-start.PLAN.md`) after lane I. Until lane I lands, windows run `--max-raids 1` |
| Window 3 (one raid at a time, same build) | clean (max gap 448 ms). XT-002 trash-free kills on uld1 (1 death) and uld2 (0); Ignis kills on all three (11, 0, 0); Kologarn a kill with 27 deaths and a wipe |
| DC lanes I, J, K | I (`c074c2b`): pre-pull refresh one member per tick, spaced 3x its cost realm-wide, so a 25-man refresh takes ~30 s and never freezes the server. J (`9d5e1b2`): `preDone` moved a boss straight from TO_BE_DECIDED to DONE, which the core takes as loading, so ToC never ran its DONE scene or summoned the next Barrett; now NOT_STARTED first. Plus a "start held" log naming what a stalled start waits for. K (`7495350`): rows `uld-razorscale` (Expedition Commander gossip), `uld-assembly`, `uld-auriaya` (Storm Tempered Keepers despawned), `uld-hodir`, and Ulduar roster anchors for the spell-credited Assembly and Hodir; all four drops navprobe-clean |
| Polish rounds merged | Beasts r2 `a038bba60` (no tank drag or taunt while a worm spews; Spew reach 56.5 yd); Kologarn r1 `f762e83f3` (eyebeam escape steps cut at the walkway's navmesh edge); Ignis r1 `1c7577271` (drag in 5 yd legs, movement block only while Ignis is on the main tank). DC `65be7ab`: Kologarn's antechamber trash despawned, by analogy with the XT-002 answer |
| Window 4 (build `1a95e2554` + DC lanes I, J; six raids) | **Jaraxxus 25h polished on first data**: kills on all three pools with 0/0/1 deaths; 10h check next. Twins: the gossip start fires and both Val'kyr are summoned and turn hostile (combat log), but no bot engages them and the runs end no_progress; Twins lane r1 investigates. Beasts r2: kills with 1/3/3/1/1 deaths and one toc1 wipe at 10:02, so not yet polished; r3 running. XT-002 (r1 code): kills 0 and 9 deaths, uld3 one void (2.5 s gap) then a kill with 2. Kologarn (pre-r1): kill 7, two wipes. Ignis (pre-r1): kills 22/0/0. Load p95 354, p99 496, max 2.5 s: the refresh's ~0.6 s ticks are the new p99 |
| Wave 5 research | lane L merged (`270e3f037`, `l-wave5-research.FINDINGS.md`): Algalon and Yogg-Saron need DC work only (medium), Thorim and Flame Leviathan also need strategy changes (large); seven questions to the user open. XT-002 r2 (hunter arc 37 yd out, all nine spots navprobe-clean) held for the user's answer on new positions |
| User answers on Wave 5 and XT-002 r2 (2026-11-21) | Build Algalon and Yogg-Saron now (lane M, with Thorim's DC row); Thorim gets a bot-led gauntlet and lever (lane N); Flame Leviathan not now, and if ever with all 4 towers; Yogg-Saron with Freya only, the Keeper mask written as instance data; no second shadow priest. XT-002 hunter arc merged (`37f380d03`); lust goes on the Heart in hard mode (`9938bd2f0`). Wave 4 batch 3 = Freya, Vezax (lane O); Mimiron after lane M's GameObject-use feature; FC and Anub'arak after the Twins lane |
| Lanes M, N, O merged | DC `3380889` (M): rows `uld-algalon` (console use with a lent key, attackable-focus readiness), `uld-yogg` (Keeper mask Freya-only as persistent data, teleport start inside Sara's 90 yd), `uld-thorim` (aggro start on the Jormungar Behemoth); `9609f86` puts Yogg's drop and start on the navprobed floor (start was 1.3 yd under it). DC `f731ac1` (O): `uld-freya`, `uld-vezax`; `4d71175` lets a comp list fallback main tanks (Vezax: DK, else warrior, else bear, since roster B has no DK tank); `022a1e1` Freya's drop z onto the floor. Fork `9266e2398` (N): with no real player on the instance the gauntlet squad's tank leads the corridor and a squad bot pulls the lever. Twins: harness must pull them (lane P, `p-dc-engage-after-gossip.PLAN.md`) |
| Window 5 (build `0c08a0922` + DC `7495350`) | **Kologarn polished at 25n** (r1: 7 kills, 0 deaths), **Ignis polished at 25n** (r1: 6 kills, at most 2 deaths), **Jaraxxus done** (10h: 3 kills, 0 deaths). First data: Razorscale kills 14/1/0, Iron Assembly 3 wipes, Auriaya kills 10/2/3, Hodir kills 1 (2:58)/13/5; polish r1 lanes for all four. Beasts on r2 code: kills 2/3/12. Load p95 385, p99 547, max 1.7 s. Lane P (`0498f87`): a gossip row can engage its focus on the main tank once it turns attackable (Twins); lane Q (FC, Mimiron) running |
| XT-002 user answers 2026-11-21 | Scrapyard trash despawned before the pulls (DC `2b846f6`: Pummellers spawned at the west piles pulled the packs in 7 of 9 wipes); an Ulduar kill counts toward "polished" only when its hard mode triggered |
| Wave 5 (FL/Thorim/Algalon/Yogg research, close-out) | open |

## Roster drafts (user-reviewed 2026-11-21)

27 per roster: a normal 25-man pull takes 2 tanks, 4 healers, all 19 DPS; *italic* members (third tank,
fifth healer) replace a DPS only when a comp asks. Every normal pull carries 3 paladins, a feral druid
(Improved Leader of the Pack), a disc priest and a resto shaman (user: disc buffs and Mana Tide matter;
a missing Blessing of Sanctuary is fine). Same three designs for `uld` (ilvl 226) and `toc` (ilvl 245); pools 1 and 4 are A, 2 and 5 B, 3 and 6 C.
Order DPS DKs before a DK tank; give each party a Draenei; order the first 6 DPS for 10-man interrupts.
At pool creation confirm the premade `bear pve` / `cat pve` specs take Improved Leader of the Pack.

| | A | B | C |
|---|---|---|---|
| Tanks | prot paladin, blood DK, *prot warrior* | prot warrior, bear druid, *prot paladin* | prot paladin, bear druid, *blood DK* |
| Healers | holy paladin, disc priest, resto shaman, resto druid, *holy priest* | holy paladin, disc priest, resto shaman, holy priest, *resto druid* | disc priest, resto shaman, holy priest, resto druid, *holy paladin* |
| DPS (19) | ret; affli, demo, destro; shadow; arcane, fire; balance, cat; ele, enh; surv, bm; as, combat; arms, fury; frost DK, unholy DK | ret, ret; affli, demo, destro; shadow; arcane, frost mage; balance; ele, enh; mm, surv; as, combat; arms, fury; frost DK, unholy DK | ret, ret; affli, demo, destro; shadow; arcane, fire; cat; ele, enh; bm, mm; as, combat; arms, fury; frost DK, unholy DK |

## Context

Testing an Ulduar or ToC strategy change today needs the user in game leading the bot raid: walk it to
the boss, start the pull, reset after a wipe, repeat. That is the bottleneck. Goal: after a committed
change, the agent rebuilds and restarts the worldserver, gets **a valid pull on each roster pool** of the boss under test
with no human client logged in, and reports from the RaidObs traces.

## Decisions (grilled with the user, 2026-11-21)

| # | Decision |
|---|---|
| Driver | **Extend mod-dungeon-clear** (`jrad7/mod-dungeon-clear`, "DC"), patches as local commits on our clone, written generic so they could go upstream later (publishing only on the user's go) |
| Rebuild | The agent rebuilds (capped) and restarts the worldserver every cycle; a restart disconnects anyone online |
| DC outside tests | **Off everywhere** outside a `.dc test` run; remove `+dungeon clear` from `AC_PLAYERBOTS_NON_COMBAT_STRATEGIES` in `configurationOverrides/Playerbot.env` (keep `+worldbuff`) |
| Rosters | **Six pools per raid** (`<raid>1`-`<raid>3` from the 2026-11-21 review, `<raid>4`-`<raid>6` copies of them since 2026-11-24 with name prefixes Uldu/Uldv/Uldw and Tocu/Tocv/Tocw), **all Alliance**, on dedicated accounts, never used for normal play. Each is a balanced roster like the user's own: every raid-wide buff and debuff covered, ~3 paladins (auras), ~3 warlocks (a soulstone per tank), no class stacked. Strategies must work for many rosters, so templates do not lock tanks or specs beyond that. Three different balanced designs, A, B and C ("Roster drafts"). Lane C's pool tables (`c-research.FINDINGS.md` section 2) are superseded |
| Comps | Each boss scenario declares tanks, healers and only the classes/specs its strategy strictly needs; the same members are picked every run. Healers **4 at 25, 2 at 10** by default, at most 5 / 3 where a fight needs it. A scenario may name its main tank (Vezax: a manaless tank). DPS death knights are created before a DK tank, so the tank never casts Freya's Army. 10-man third tank only on Anub'arak. The roster table is **reviewed by the user** before any pool is created |
| Gear | The fork's sim-weighted gearing, pool average **ilvl 226 (Ulduar)** and **245 (ToC)**; previous-tier BiS cannot beat hard modes / heroics. Gear set once; re-gear only on demand |
| Pulls | A run stays in one instance and **re-pulls after a wipe**; a kill ends the run (next pull gets a fresh instance) |
| Wipe reset | Revive everyone at the drop point (no Resurrection Sickness), full health and mana, pets revived, all spell cooldowns cleared including Sated/Exhaustion, **full `maintenance` before every pull** (as the user does manually), wait until the boss is back at spawn, out of combat, encounter not in progress; **no `rebuff`** (bots buff after resurrection) |
| Difficulty | One pull per pool (6 per boss per round) at one difficulty: the one the change targets. Defaults: **ToC 25-man heroic, Ulduar 25-man hard mode** from the start (the strategies work; the hard versions show the rough edges). ICC later starts on normal. Heroic Beasts' first-pull gossip needs achievement 3916 or 3917 on the clicker, so pool members get it |
| Ulduar hard mode | Follows the `Ulduar*HardMode` toggles in `Playerbot.env` (all on today); every report states it |
| No-player logout | `DisabledWithoutRealPlayer` stays on. Its logout runs once per absence and spares bots whose master is a self-mastered bot (the DC driver); before, the driver kept it repeating and every later masterless roster login was dropped |
| DC during a pull | DC stays held through every pull (logistics only: spawn, gear, group, teleport, reset); the fork's raid strategies run the whole fight, an aggro start sets the boss on the tank. Replaces the earlier "keep DC's pre-boss muster" answer, which let DC's multipliers veto strategy actions inside the fight |
| Scope | A cycle tests the boss the change touches; a **sweep** (3 pulls of every automated boss of a raid) only when the user says "sweep" |
| When | A cycle runs **automatically** after every committed change under `src/Ai/Raid/Uld` or `src/Ai/Raid/ToC`, unless the user says skip. While the polish loop runs, such a change goes into the next window instead |
| Baseline | A pool with no valid pull of that boss and difficulty from the build before the change gets one on the live build before the rebuild. In the polish loop a pool's pulls from earlier builds are the baseline; no separate phase |
| Limits | No wall-clock cap. A pull ends at kill, wipe, enrage, or DC's stall checks (`stalled` 120 s, `no_progress` 600 s, which counts the boss's health moving). A pool stops after **2 pulls** (`cap`) without a valid one. DC's run timeout 2 h → 6 h. A failed build, a crash or a driver that never logs in stops the cycle with a report, no retry around it |
| Traces | `Playerbots.Obs.RetentionDays` 7 → 60, `MaxDirMB` 5120 → 51200 in `Playerbot.env` (no override exists today; G: has 391 GB free, biggest trace 28 MB). `MaxFileMB` 256 stays |
| Instance limit | `AccountInstancesPerHour` raised to 100 server-wide |
| SOAP | The user creates a dedicated GM account and fills gitignored `tools/raidtest/.env`; no other credentials are read |
| Test Deck | Skipped; DC is driven over SOAP and its result files read directly |
| Reports | In chat only; traces and DC's run log are the record; durable conclusions go to `docs/raids/` |
| Pilot | XT-002 (aggro start) and Northrend Beasts (Barrett gossip start) |
| Master fix | Committed on Custom only, not ported into the upstream PR stacks unless asked |
| Orchestration | The agent orchestrates; the work runs as Workflow waves, each launched only on the user's "go"; the polish loop runs on one standing go |
| Git authority | mod-playerbots: lane branches and worktrees, `--no-ff` merges into Custom without waiting. DC clone: lane branches and worktrees from `local/wrath-raid-tests`, `--no-ff` merges into it. Never push, amend, rebase or force |

## Facts the work rests on

**DC** (paths in `jrad7/mod-dungeon-clear`):
- `.dc test start|status|stop|plan …` are `Console::Yes`, so SOAP drives them
  (`src/DungeonClearCommand.cpp:303-375`); in-game `.dc on/go/…` are not.
- Headless driver `src/TestRun/DcTestDriver.cpp`: auto-creates `Dcdriver` (Human Warrior, account
  `dcdriver`), logs it in with no socket via `sRandomPlayerbotMgr.AddPlayerBot(guid, 0)`, makes it its
  own master with GM security, parked `+stay +passive`. It is master of every bot but **not in the
  group**; slot 0 leads; `ReassertMaster` re-sets it every tick. The first console call may answer
  "logging in, retry".
- `party=Name,...` logs existing characters in masterless and never regears them; only slot 0 is
  marked tank and slot 1 healer.
- Raid maps hard-coded to `RAID_DIFFICULTY_10MAN_NORMAL` (`src/TestRun/DcTestRunJob.cpp:1142-1146`);
  `MaxPlayers` reads difficulty 0 only (`DcTestDungeonRegistry.h:185-192`); heroic refused for raid
  rows (`DcTestRunManager.cpp:70-74`); registry has no 603/649 (`DcTestDungeonRegistry.cpp:128-165`).
- Scenario rows (`DcTestDungeonRegistry.h:96-130`): drop point + `focusEntries`, other bosses skipped,
  success = focus boss dead. Stale-instance check refuses completed encounters (`DcTestRunJob.cpp:1502`).
  `DungeonClear.ClearRoomBeforeBoss` (default 1) must be 0 for us. Gossip step precedents:
  `KarazhanEvents.cpp:80-124`, the Trial of the Champion driver. Raid wipe recovery lives in
  `DcRezRecovery.cpp:429-435, 793-911` (revives at the entrance today).
- Once an encounter is live DC stands down (`DcBossStandDown.h`) except encounter events, hold-fire and
  a relevance-35 LOS assist; its multiplier still suppresses stock follow/grind/travel/pull for bots in
  a run.
- `DcStrategyGate` (`src/DcStrategyGate.cpp:67-151`) adds `dungeon clear`, `food`, `worldbuff` to
  every bot on any dungeon or raid map while the module is enabled.
- Watchdog progress counts entering/leaving combat, a new victim, or the victim's health moving
  (`DcTestRunJob.cpp:~2100`); limits from `TestRun.NoProgressS` / `TestRun.OverallTimeoutS` or the row.
- Results in the worldserver's working directory: `dc_testruns.jsonl`, `dc_testrun_live.json`,
  `dc_testplans.jsonl`; `trace=1` adds `lab_traces/<runId>.jsonl`; `tools/dc_test_run.py <id>` reads a
  run. Logs (`DungeonClear.log`) truncate on restart.
- Signatures it uses match our fork (spot-checked, not compiled). C++20, static module build. Its own
  `CLAUDE.md` forbids worktrees in its repo and asks for feature branches and commit discipline; follow
  it there. Its "the user always builds" line is superseded by the user's rebuild authorization.

**Fork and server**:
- Bots read the master in 12 raid files: `ToC/Util/ToCHelpers_Staging.cpp` (`StagingLeader`
  :1027-1040: master, else group leader only when there is no master; a master off the instance gives
  `nullptr`), `ToCActions_FactionChampionsDefence.cpp`, `Uld/Trigger/UldTriggers_FlameLeviathan.cpp`
  (`master->GetVehicle()`), `UldTriggers_Thorim.cpp`, `UldActions_Thorim.cpp`, `UldEncounter_Thorim.cpp`,
  `UldTriggers_YoggSaron.cpp`, `UldEncounter_YoggSaron.cpp`, `UldActions_Auriaya.cpp` (all under
  `src/Ai/Raid/`).
- No strategy starts an encounter. ToC: every stage is Barrett's gossip (core
  `instance_trial_of_the_crusader.cpp:223-252`; `docs/raids/trial-of-the-crusader/README.md:251-290`).
  Ulduar: Razorscale = Expedition Commander gossip, Yogg = Keeper gossip, Algalon = console + key,
  Mimiron hard mode = button, Thorim = lever after 6 gauntlet deaths, Flame Leviathan = vehicles; the
  rest start on tank aggro.
- `maintenance` (`src/Ai/Base/Actions/TrainerAction.cpp:188-287`) never regears. Roster bots count as
  alt bots under the DC driver (`IsAltBot`: master is a SelfBot), so the `AltMaintenance*` flags apply;
  shipped defaults re-init talents, glyphs, gems and enchants too.
- mod-individual-progression stomps bot-account level-80 characters to tier 13 at login; Ulduar needs
  14, ToC 15 (`docs/systems/itemization.md:380-411`). Check how its gate treats the pool accounts and set
  each pool's tier.
- `+worldbuff` and a world buff matrix are already on for all bots via `Playerbot.env`.
- SOAP on via `configurationOverrides/Soap.env`, port 7878, also `127.0.0.1:7878`; commands run on
  the world thread. Traces: `G:\DevStuff\GitHub\azerothcore-wotlk-pb\env\dist\logs\botobs\`; readers
  `tools/botobs/batch.py` (`--strict`, `--since`, `--split-at`, `--baseline`, `--probes`),
  `postmortem.py`, `bosses/<boss>.py`.
- Capped rebuild: memory `worldserver-rebuild`. Server lock: `mkdir
  G:/DevStuff/GitHub/.wave-loop/locks/server` plus an owner file. Effective config:
  `docker exec ac-worldserver env | grep ^AC_`.

## Orchestration rules

- Load `workflow-authoring` before writing the wave script; the script sits in
  `G:\DevStuff\GitHub\azerothcore-wotlk-pb\build-toc-wt\` (an added directory the Workflow tool reads).
  Each wave under 10 agents. Launch a wave only on the user's "go"; report after each.
- **Agents never touch Docker** (no build, restart, navprobe, SOAP, live run): the orchestrator does all
  of it. Every brief says so.
- One fresh reviewer per lane between its commits and the merge, never shown the lead's report; it fixes
  bugs and minors on the lane branch.
- Unique commit-message file names per lane in the shared scratchpad.
- mod-playerbots lanes: worktrees under `G:\DevStuff\GitHub\azerothcore-wotlk-pb\build-toc-wt\rt-<lane>`
  (two levels under the core root, and `build-toc-wt` is an added directory), branch `rt/<lane>`; closing cycle per `CLAUDE.local.md`
  (`pb-check`, docs, commit). DC lanes: worktree `build-toc-wt/rt-dc-<lane>`, branch `rt/dc-<lane>` from
  `local/wrath-raid-tests`, one lane at a time; the capped rebuild after the merge is their compile check.

## Waves

### Wave 1 (orchestrator only)
- **Stop and ask the user** for the clone into `modules/mod-dungeon-clear` and the local branch
  `local/wrath-raid-tests` on a pinned master commit; record it in the plan.
- DC conf: `DungeonClear.Enable=1`, appender/logger lines (`conf/mod_dungeon_clear.conf.dist:677-699`).
- `Playerbot.env`: drop `+dungeon clear`; `Obs.RetentionDays` 60, `Obs.MaxDirMB` 51200; raise
  `AccountInstancesPerHour` to 100 in the matching override file. Confirm `ApplyInstanceStrategies=1`.
- The user fills `tools/raidtest/.env`.
- Capped rebuild in the background (first build compiles all of DC), restart, confirm the new env
  values, smoke-test a headless 5-man `.dc test` over SOAP, and record where the worldserver's working
  directory lives.

### Wave 2 (three lanes)
- **A, DC clone, sequential, reviewer after each:**
  - A1: `DungeonClear.TestRunsOnly` (gate installs nothing outside a test run); `diff=10n|10h|25n|25h`
    on `test start` / `test plan start`; `MaxPlayers` per difficulty; heroic for raid rows; Ulduar (603)
    and ToC (649) registry rows.
  - A2: scenario `preDone` (boss ids set `DONE` after instance creation, excluded from the stale check)
    and a start step (gossip on an NPC entry, or tank aggro); in-place wipe reset per the decisions
    table, re-pull until kill or cap; full `maintenance` before every pull; run timeout 6 h.
  - A3: `.dc test roster create <raid>` (pool from a template on `dcroster<raid><n>` accounts, 10
    characters each; `TryProvision`'s creation path or `RandomPlayerbotFactory::CreateRandomBot`), level
    80, gear once to the pool's ilvl with the fork's gearing, set the mod-individual-progression tier;
    `.dc test roster gear <raid> <ilvl>`; scenario comps per size and deterministic member picking;
    roles from `IsTank`/`IsHeal`; `roster=<raid>` on runs; run record carries pool, ilvl, members.
- **B, mod-playerbots worktree:** a helper beside `StagingLeader` (master when on this instance, else
  the group leader when on this instance, else `nullptr`) used at the 12 sites where the master stands
  for the raid's leader; read each site's use first; leave Flame Leviathan's vehicle check alone. Runner
  `tools/raidtest/`: `soap.py` (stdlib), `raidtest.py` implementing the cycle below, unit tests for its
  parsing and pull counting.
- **C, read-only research:** the comp table (every Ulduar and ToC boss, 10 and 25: tanks, healers,
  classes/specs the strategy assigns, with source citations and guesses marked) and the pool templates
  as its union; start path for XT-002 and Northrend Beasts (boss states, data, gossip menu/option ids
  from the instance scripts) and drop-point candidates. Ends with **the user's review of the comp
  table**.

### Wave 3 (orchestrator only)
Merge, rebuild, deploy; navprobe the drop points; create both pools after the comp table is approved;
pilot cycles for XT-002 25N and Northrend Beasts 25N; fix what the pilots expose.

### Wave 4 (DC batches inside the polish loop)
Scenario rows for the remaining bosses, one DC lane per batch (brief per batch beside this plan):
1. ToC Jaraxxus, Twin Val'kyr; Ulduar Ignis, Kologarn (lane H).
2. Ulduar Razorscale (commander gossip), Iron Assembly, Auriaya, Hodir (lane K).
3. ToC Faction Champions (its opener reads the sender's selected healer: check what DC can supply) and
   Anub'arak, once lane J makes the later Barrett stages start; Ulduar Freya, Mimiron, Vezax.

The orchestrator navprobes each drop, merges, and the next window gives each new boss one pull per
pool; a boss with a pull on every pool joins the polish loop.

## Polish loop (go 2026-11-21)

A rebuild restarts the only worldserver and kills every pull in flight, so polish and new-boss work
share one build schedule. Scheduling since 2026-11-24 is `u-loop-speedup.PLAN.md` section D.

- **Server (orchestrator only):** two boss groups take turns. Group 1's window runs on build *n* while
  group 2's lanes code; once all of group 2's lanes have merged, one rebuild and group 2's window while
  group 1's lanes code; and so on. A boss whose lane changed nothing stays in its group's window. A
  window pulls each of its bosses once per pool (`<raid>1`-`<raid>6`, 6 pulls), at most 6 raids at once.
  Never rebuild with runs in flight. **Filler** while the server waits for the other group's lanes:
  pending 10-man checks, then done bosses whose shared code changed, then extra pulls of the group that
  just ran on the same build; running filler finishes before the rebuild, and none starts once the other
  group's lanes have all merged.
- **Polish lanes:** one Workflow run per boss per round (lead + fresh reviewer), brief
  `g-polish.PLAN.md`, worktree `build-toc-wt/rt-polish-<slug>`, branch `rt/polish-<slug>-r<n>` from
  Custom per round, launched as soon as its boss's report file appears. The lane's memory between
  rounds is `polish/<slug>.md` (`## State` and the newest round) and `polish/history/<slug>.md`.
- **Fix authority:** a lane fixes bugs (bots not doing what the strategy intends) directly; a plan change
  (new tactic, different tank, kill or target order, new positions or assignments) goes to the user as
  a question.
- **Judging a fix:** by the mechanic it targets (did those deaths stop), per composition across its
  pulls and the trend table, not by kill rate; a regression needs the trend over several builds.
- **Done:** on one build, each composition (A = pools 1 and 4, B = 2 and 5, C = 3 and 6) has a kill with
  at most 5 deaths among its pulls. An Ulduar kill counts only when its hard mode actually triggered
  (the boss reader), not because the server toggle is on. Then the 10-man check, one pull per pool (10h,
  or 10n with hard mode), by the same rule; failures go back into polish. A lane that picks the same
  failure three rounds running and still sees it stops and reports.
- **Shared code:** a lane changes only its boss's files; a shared change is named in its summary and
  every boss using it is pulled again (filler).
- **Reports:** in chat after each window: pulls, kills, deaths by mechanic, what changed. Questions go to
  the user as one numbered batch per full cycle (after both groups reported), never per lane.
- **No code comments:** raid code comments were stripped on Custom (`c163ec11a`, knowledge in the
  boss docs). A lane branched before it runs `python -I tools/commentstrip/commentstrip.py strip
  --keep-from c163ec11a src/Ai/Raid/<Raid>` on its branch and commits that before merging; `file:line`
  refs written before it point at `c163ec11a^`.

### Wave 5
Research only: Flame Leviathan (vehicle driving), Thorim (gauntlet walk), Algalon (console + key),
Yogg-Saron (Keeper gossip, master's room); findings and questions to the user before building any.
Close-out: fold durable facts into `docs/systems/raid-test-runs.md` (cycle, DC patches and why, traps)
and delete the plan; one `CLAUDE.md` line pointing at the runner and doc; in `CLAUDE.local.md` the
standing rebuild authorization and the automatic cycle after Uld/ToC commits (both through
`/compact-docs-writer`); update the `botobs-trace-retention` memory and save a project memory. Close-out material also sits in the
old boss table's harness rows (`windows.md`) and `u-loop-speedup.PLAN.md`'s close-out list.

## The cycle (what the runner does)

1. Take the server lock. Refuse, and report, if the core or module trees carry dirty files this cycle
   didn't make (they would ship in the build).
2. Baseline: each pool with no valid pull of the boss and difficulty from the previous build gets one
   on the live build first.
3. The change is already committed (closing cycle), so the binary will be newer than HEAD. Capped
   rebuild, restart, wait for ready, retry until the DC driver is online.
4. `.dc test plan start <scenario> roster=<raid> diff=<d>` over SOAP on every pool at once, until each
   pool has a valid pull built after HEAD or ran 2 (`--pulls 1`, `--cap 2`).
5. Report: a digest per boss (pulls with pool and composition, the done rule per composition, deaths by
   mechanic, each pull's first deaths, the trend over the last four builds, each boss reader's
   `--summary`); `--full` gives the old per-pool dump. Pulls with a DC veto inside the fight never
   count. Release the lock.

## Verification

- Wave 1: env shows the new `AC_PLAYERBOTS_OBS_*` and instance values; a headless 5-man `.dc test`
  over SOAP ends `success` with no client logged in.
- Wave 2: with `TestRunsOnly=1` a bot in a manual instance carries no `dungeon clear` strategy
  (`.playerbots inspect <bot> strategy`); a `diff=25n` trace header reads 25-man normal; a roster run
  logs in the scenario's comp, tanks marked from talents, the same members twice; after a wipe the raid
  re-pulls in the same instance with cooldowns and Sated cleared; `pb-check` passes on lane B.
- Wave 3: each pilot cycle, with nobody online, yields 3 traces `batch.py --strict --since HEAD` counts
  valid, the encounter's own nodes reach `--coverage`, and no DC-multiplier `veto` row falls inside a
  pull. A manual pull with the user leading reads the same as before lane B (staging probes unchanged).
