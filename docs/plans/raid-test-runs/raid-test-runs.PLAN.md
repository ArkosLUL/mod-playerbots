# Unattended raid test runs on mod-dungeon-clear

Lane briefs sit beside this file as `<lane>.PLAN.md`, written when their wave is prepared.

## Resume here

The polish loop runs on the user's standing go ("Polish loop" below). Resumed 2026-11-22 11:00 after a PC shutdown. Window 7 runs on build `61011f323` + DC `5443dbc` (job file `scratchpad/window7-jobs.json`: Twins 10h, Beasts 25h, FC smoke on toc1; Hodir, Iron Assembly, Auriaya, Razorscale, XT-002 at 25n; Kologarn and Ignis 10n; Vezax smoke on uld1, Mimiron smoke on uld2; Thorim, Freya, Yogg-Saron, Algalon data). Lanes in flight: polish XT-002 r3 (`rt/polish-xt-002-r3`), Beasts r4 (`rt/polish-northrend-beasts-r4`), Hodir r2 (`rt/polish-hodir-r2`), runner lane R (`rt/r`, `r-runner-unrefreshed.PLAN.md`); a read-only Shadowfiend investigation for the user. Per boss, at the last update:

| Boss | State | Next |
|---|---|---|
| Jaraxxus | done: 25h kills with 0/0/1 deaths, 10h kills with 0 | none |
| Kologarn, Ignis | polished at 25n (round 1) | 10n check: one pull per pool (window 6 was stopped before it) |
| XT-002 | round 2 merged (hunter arc, lust on the Heart); window 6 (cut short at shutdown): kills with 2 and 0 deaths and one wipe, Heartbreak not yet read | round 3 from window 6's traces (`--head 9938bd2f0`); counts only with Heartbreak |
| Northrend Beasts | round 3 merged (detour for the Toxin runner); window 6 pulled it on all three pools before the stop | round 4 from window 6's traces (`--head 96e2d86c2`); the mobile holder's drag loses threat, which may be a user question |
| Twin Val'kyr | **polished at 25h** on first real data (window 6: three kills, 0 deaths each; DC engages Fjola on the main tank 26 s after the gossip) | 10h check |
| Razorscale | round 1 merged (`3a3e4866c`: each ranged bot its own fire-free spot on the stack circle) | test |
| Hodir | round 1 merged (`5f16288c8`: shed Biting Cold one stack sooner under Frozen Blows; no Misdirection while the shed is armed). Hard mode = no Shatter Chest 62501 cast before the kill (`hodir.py` banner) | test; round 2 implements the user's answer (2026-11-22) on off-tank deaths in Frozen Blows: each tank uses a defensive cooldown in its Frozen Blows window first, then the tanks alternate taunts on purpose if off-tanks still die |
| Iron Assembly | round 1 merged (`ad6d38a22`: healer Shadowfiend held until 50% mana; hard mode read from `iron_assembly.py`, Steelbreaker last); DC `962eb10`: 5 healers at 25 (all three pools ran dry before Steelbreaker was alone) | test in window 7. User (2026-11-22): the ranged stay stacked in Molgeim's phase (no spread). Not picked: the swap tank dies to a one-charge Fusion Punch |
| Auriaya | round 1 merged (`dfbe730f8`: DPS wait until a Sentry is in combat; taunt picks the Sentry on a non-tank first and walks into reach) | test in window 7, then round 2 implements the user's answers (2026-11-22): if the off-tank dies, the main tank takes up to two loose Sentries; DPS attack the off-tank's Sentry, not the nearest |
| Shared priest Shadowfiend | disc and holy cast it as soon as it is off cooldown, at 80-95% mana, seen at Iron Assembly | user (2026-11-22): investigate across every boss with evidence before any fix; a read-only investigation is running |
| Harness: unrefreshed pulls | "the boss engaged before the start step" means no refresh (low mana, Exhaustion from the previous run): Hodir uld2 (a Starfall hit the frozen helpers' ice blocks, which pulls him), Thorim (pack aggro). DC `e8c3b0c`: `uld-hodir` drop (1998, -288), 46 yd from the nearest block, Winter Revenant pack despawned (its patrol turns 26 yd off); `uld-thorim` drop (2152, -232), 31 yd from the pack (both navprobe-clean). DC `5443dbc`: record schema 18, each `bossPulls[]` entry carries `start` `step`/`engaged` | lane R: the runner voids `engaged` pulls; until it merges, check window 7's records by hand |
| Shared tools | `postmortem.py` and `raidtest/pulls.py` label a Biting Cold death "its own blow" (the victim casts the damage on itself) | lane R |
| Harness: shared spread angle | fixed `61011f323`: `MovementAction::GetFollowAngle` spreads over the bot's own group and skips the master only when it is in it (the DC driver is master but not in the group, so every bot got angle 0 and every `MoveNear`/`MoveInside` caller stacked the raid; Hodir, Yogg-Saron, Naxxramas, Sunwell, Black Temple, Tempest Keep use it) | window 7 pulls Hodir and Yogg-Saron on it; the rest when their raids are tested |
| Freya, Vezax, Algalon, Yogg-Saron, Thorim | window 6 smoke: every new start step worked (console use with the lent key, teleport start into Sara's range). Freya kill with 8 deaths (hard mode unread); Algalon wipe 4:35, Yogg-Saron wipe 3:08, Thorim wipe 6:05 (started unrefreshed, see the harness row); Vezax never pulled | data on every pool, then round 1 each |
| Faction Champions, Mimiron | DC rows merged (`2f98dd3`): `totc-champions` engages the first kill-order healer on the main tank after the intro; `uld-mimiron` presses the Big Red Button as every pull's start (new `ObjectStart`), drop navprobe-clean. The fork's FC grip opener needs a player-selected target DC doesn't supply, so bots kill healers in place | smoke one pull each |
| Anub'arak | needs a way to bring him back after a wipe (only a player entering the map respawns him; see DC's `totc-twins` row comment) | brief a DC lane |
| Flame Leviathan | parked by the user | none |

Mechanics: lanes run through one reusable Workflow script (lead, then a fresh reviewer when the lead committed; kinds `polish`, `dc`, `strategy`, `research`, `runner`): `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/raid-polish-lane.js`, invoked with `args` naming the kind, tag, brief, worktree and branch (polish lanes also boss, slug, raid, round, report files and a note). Windows run `python tools/raidtest/raidtest.py window --jobs <file.json>` (no `-I`; `cap` is per window while `pulls` counts every build since the job's `head`, so `cap` must be at least `pulls`). Reports and job files of past windows lived in the session scratchpad and are gone; `raidtest.py report --boss <slug> --raid <raid> --head <sha> --pulls 10` rebuilds any view from the traces.

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
2. Ulduar Razorscale (commander gossip), Iron Assembly, Auriaya, Hodir (lane K).
3. ToC Faction Champions (its opener reads the sender's selected healer: check what DC can supply) and
   Anub'arak, once lane J makes the later Barrett stages start; Ulduar Freya, Mimiron, Vezax.

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
  each pull. An Ulduar kill counts only when its hard mode actually triggered (XT-002: Heartbreak
  landed), not because the server toggle is on. Then one 10-man pull per pool (10h, or 10n with hard mode) as a check; failures go back
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
