# Unattended raid test runs on mod-dungeon-clear

Lane briefs sit beside this file as `<lane>.PLAN.md`, written when their wave is prepared.

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
| Wave 4 (remaining bosses) | in batches inside the polish loop; batch 1 = lane H |
| Wave 5 (FL/Thorim/Algalon/Yogg research, close-out) | open |

## Roster drafts (user-reviewed 2026-11-21; identical copies vs these three still open)

27 per roster: a normal 25-man pull takes 2 tanks, 4 healers, all 19 DPS; *italic* members (third tank,
fifth healer) replace a DPS only when a comp asks. Every normal pull carries 3 paladins, a feral druid
(Improved Leader of the Pack), a disc priest and a resto shaman (user: disc buffs and Mana Tide matter;
a missing Blessing of Sanctuary is fine). Same three designs for `uld` (ilvl 226) and `toc` (ilvl 245).
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
change, the agent rebuilds and restarts the worldserver, gets **3 valid pulls** of the boss under test
with no human client logged in, and reports from the RaidObs traces.

## Decisions (grilled with the user, 2026-11-21)

| # | Decision |
|---|---|
| Driver | **Extend mod-dungeon-clear** (`jrad7/mod-dungeon-clear`, "DC"), patches as local commits on our clone, written generic so they could go upstream later (publishing only on the user's go) |
| Rebuild | The agent rebuilds (capped) and restarts the worldserver every cycle; a restart disconnects anyone online |
| DC outside tests | **Off everywhere** outside a `.dc test` run; remove `+dungeon clear` from `AC_PLAYERBOTS_NON_COMBAT_STRATEGIES` in `configurationOverrides/Playerbot.env` (keep `+worldbuff`) |
| Rosters | **Three pools per raid** (2026-11-21 review), **all Alliance**, on dedicated accounts, never used for normal play. Each is a balanced roster like the user's own: every raid-wide buff and debuff covered, ~3 paladins (auras), ~3 warlocks (a soulstone per tank), no class stacked. Strategies must work for many rosters, so templates do not lock tanks or specs beyond that. Whether the three are identical copies or three different balanced rosters: open. Lane C's pool tables (`c-research.FINDINGS.md` section 2) are superseded |
| Comps | Each boss scenario declares tanks, healers and only the classes/specs its strategy strictly needs; the same members are picked every run. Healers **4 at 25, 2 at 10** by default, at most 5 / 3 where a fight needs it. A scenario may name its main tank (Vezax: a manaless tank). DPS death knights are created before a DK tank, so the tank never casts Freya's Army. 10-man third tank only on Anub'arak. The roster table is **reviewed by the user** before any pool is created |
| Gear | The fork's sim-weighted gearing, pool average **ilvl 226 (Ulduar)** and **245 (ToC)**; previous-tier BiS cannot beat hard modes / heroics. Gear set once; re-gear only on demand |
| Pulls | A run stays in one instance and **re-pulls after a wipe**; a kill ends the run (next pull gets a fresh instance) |
| Wipe reset | Revive everyone at the drop point (no Resurrection Sickness), full health and mana, pets revived, all spell cooldowns cleared including Sated/Exhaustion, **full `maintenance` before every pull** (as the user does manually), wait until the boss is back at spawn, out of combat, encounter not in progress; **no `rebuff`** (bots buff after resurrection) |
| Difficulty | 3 pulls at one difficulty per cycle: the one the change targets. Defaults: **ToC 25-man heroic, Ulduar 25-man hard mode** from the start (the strategies work; the hard versions show the rough edges). ICC later starts on normal. Heroic Beasts' first-pull gossip needs achievement 3916 or 3917 on the clicker, so pool members get it |
| Ulduar hard mode | Follows the `Ulduar*HardMode` toggles in `Playerbot.env` (all on today); every report states it |
| No-player logout | `DisabledWithoutRealPlayer` stays on. Its logout runs once per absence and spares bots whose master is a self-mastered bot (the DC driver); before, the driver kept it repeating and every later masterless roster login was dropped |
| DC during a pull | DC stays held through every pull (logistics only: spawn, gear, group, teleport, reset); the fork's raid strategies run the whole fight, an aggro start sets the boss on the tank. Replaces the earlier "keep DC's pre-boss muster" answer, which let DC's multipliers veto strategy actions inside the fight |
| Scope | A cycle tests the boss the change touches; a **sweep** (3 pulls of every automated boss of a raid) only when the user says "sweep" |
| When | A cycle runs **automatically** after every committed change under `src/Ai/Raid/Uld` or `src/Ai/Raid/ToC`, unless the user says skip. While the polish loop runs, such a change goes into the next window instead |
| Baseline | Fewer than 3 valid pulls of that boss and difficulty from the build before the change → run 3 on the live build before rebuilding. In the polish loop a pool's pulls from earlier builds are the baseline; no separate phase |
| Limits | No wall-clock cap. A pull ends at kill, wipe, enrage, or DC's stall checks (`stalled` 120 s, `no_progress` 600 s, which counts the boss's health moving). A cycle stops after **6 pulls** without 3 valid ones. DC's run timeout 2 h → 6 h. A failed build, a crash or a driver that never logs in stops the cycle with a report, no retry around it |
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
2. ToC Faction Champions (its opener reads the sender's selected healer: check what DC can supply),
   Anub'arak; Ulduar Razorscale (commander gossip), Auriaya.
3. Ulduar Hodir, Freya, Iron Assembly, Mimiron, Vezax.

The orchestrator navprobes each drop, merges, and the next window gives each new boss one pull per
pool; a boss with a pull on every pool joins the polish loop.

## Polish loop (go 2026-11-21)

A rebuild restarts the only worldserver and kills every pull in flight, so polish and new-boss work
share one build schedule.

- **Server (orchestrator only):** build, then a test window, repeat. A window runs a job list on one
  build across all six pools (`raidtest.py window`, lane F `f-runner-window.PLAN.md`), up to 6 raids at
  once unless its load line degrades (one to three parallel 25-man fights read snapshot gaps p95
  ~320 ms, max < 450). Rebuild only between windows, only when something new is merged. Idle pool time
  goes to Wave 4 checks and extra pulls of bosses in polish.
- **Polish lanes:** one Workflow run per boss per round (lead + fresh reviewer), brief
  `g-polish.PLAN.md`, worktree `build-toc-wt/rt-polish-<slug>`, branch `rt/polish-<slug>-r<n>` from
  Custom per round. The lane's memory between rounds is `polish/<slug>.md` in this folder. In polish:
  XT-002 (25n hard mode), Northrend Beasts (25h); at most 4 bosses at once.
- **Fix authority:** a lane fixes bugs (bots not doing what the strategy intends) directly; a plan change
  (new tactic, different tank, kill or target order, new positions or assignments) goes to the user as
  a question in the window report.
- **Judging a fix:** by the mechanic it targets (did those deaths stop), not by kill rate.
- **Done:** a boss is polished when one window gives a kill on all three pools with at most 3 deaths in
  each pull. Then one 10-man pull per pool (10h, or 10n with hard mode) as a check; failures go back
  into polish. A lane that picks the same failure three rounds running and still sees it stops and
  reports.
- **Shared code:** a lane changes only its boss's files; a shared change is named in its report and
  every boss using it is pulled again next window.
- **Reports:** in chat after every window: pulls, kills, deaths by mechanic, what changed, questions.

### Wave 5
Research only: Flame Leviathan (vehicle driving), Thorim (gauntlet walk), Algalon (console + key),
Yogg-Saron (Keeper gossip, master's room); findings and questions to the user before building any.
Close-out: fold durable facts into `docs/systems/raid-test-runs.md` (cycle, DC patches and why, traps)
and delete the plan; one `CLAUDE.md` line pointing at the runner and doc; in `CLAUDE.local.md` the
standing rebuild authorization and the automatic cycle after Uld/ToC commits (both through
`/compact-docs-writer`); update the `botobs-trace-retention` memory and save a project memory.

## The cycle (what the runner does)

1. Take the server lock. Refuse, and report, if the core or module trees carry dirty files this cycle
   didn't make (they would ship in the build).
2. Baseline: if fewer than 3 valid pulls of the boss and difficulty exist from the previous build, run
   them first on the live build.
3. The change is already committed (closing cycle), so the binary will be newer than HEAD. Capped
   rebuild, restart, wait for ready, retry until the DC driver is online.
4. `.dc test plan start <scenario> roster=<raid> diff=<d>` over SOAP until `batch.py --strict --since
   HEAD --boss <slug>` counts 3 valid pulls, or 6 pulls.
5. Report: outcome and duration per pull, deaths with cause, hard mode on/off, the boss reader's view,
   `--probes` for keys the change added, `--split-at HEAD` moved metrics, and any `veto` row naming a DC
   multiplier inside a pull (contamination check). Release the lock.

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
