# Polish log: Twin Val'kyr (25H)

Trace slug `val-kyr-twins`, raid `toc`, scenario `totc-twins` (Barrett 35770 gossip, `preDone` {0,1,2},
60 s first-pull wait). Reader: `tools/botobs/bosses/val_kyr_twins.py`.

## Round 1 (2026-11-21, build 1a95e2554, DC 9d5e1b2)

**Data.** Window 4, one run per pool, all `no_progress` at 600 s, no trace (a ToC trace opens at the
engage): toc1 `tr-20261121-200302-9` (instance 9), toc2 `tr-20261121-200342-10` (11), toc3
`tr-20261121-200342-11` (10). Each logged `pull 1/1 started (gossip)`. The chronicle logs
`instance_649_{9,10,11}_*.log` were already gone from `env/dist/logs/chronicle_logs`; the run records
in `env/dist/logs/dc/dc_testruns.jsonl` (lines 58-60) carry a teardown snapshot of every member.

**Stop rules.** Not polished: no kill. First round.

**Deaths by mechanic.** None: no pull engaged, 25/25 alive in every run.

**Picked: the twins are never pulled (harness, not a fork bug).** In all three teardown snapshots
every member stands exactly on its fork-dealt door stand, out of combat: Fjola's tank on (560.5, 88.2)
(Tocxaa, Tocyaa, Toczaa: slot 0, `MEMBER_FLAG_MAINTANK`), Eydis's on (565.5, 88.2), the rest on the
melee/heal/ranged rows (`ToCHelpers_TwinValkyr.cpp:65-77`). So staging and the door deal ran as
designed and the raid waited for the pull. By design nothing in the fork starts it:

- A bot tank's hold walks to its door spot until a twin is in combat or a pull was called
  (`TwinValkyrTankHoldAction::HoldTwin`, `ToCActions_TwinValkyr.cpp:315-317`);
  `TwinValkyrFetchDue` before the pull needs a tank's `pull target` on a twin (`PullCalled`,
  `ToCHelpers_TwinValkyr.cpp:1628-1644, 1698-1699`), which only the `attack` / `tank attack` command
  sets, and that reads `master->GetTarget()` (`AttackAction.cpp:32-51`): under DC the master is the
  headless driver off the map.
- Staging blocks tank and dps assist (`ToCDefinition_Staging.cpp`), `twin valkyr tank target guard`
  zeroes tank assist while a twin is attackable.
- The twins turn aggressive at (585.5, 170) / (545.5, 170), ~85 yd from the door, far outside their
  23 yd proximity aggro (`instance_trial_of_the_crusader.cpp:777-785`).

Beasts and Jaraxxus engage because their scripts attack on their own; the twins need a puller.

**What DC must do (no fork change).** Keep the Barrett gossip start and add an engage after it, every
pull (the retry gossip replays scenes 301-304): once Fjola Lightbane 34497 (the row's first focus) is
alive and `!HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE)` (cleared with `REACT_AGGRESSIVE` by
`EVENT_VALKYRIES_ATTACK`, ~25.5 s after the gossip: 13 + 3 + 6.25 + 3.25 s) and neither twin is in
combat, run the existing `EngageFocus(tank)` once, `tank` being slot 0 (the gossip's clicker, the
`MEMBER_FLAG_MAINTANK` member the fork deals Fjola to, `ResolveTwinTanks` :272-285). Fjola's
`JustEngagedWith` then zone-combats the raid and sends Eydis at her nearest player
(`boss_twin_valkyr.cpp:218-226`); Eydis's bot tank taunts her in at 30 yd (`HoldTwin` :323-328).
Not earlier: before the flag clears they are passive and mid scripted flight. Not the `attack` chat
command (master off map), not DC's own pull logic (DC stays held).

The same gap will meet Faction Champions ("nobody in aggro range, so a player starts it",
`ToCHelpers_Staging.cpp:573`) and Anub'arak (stands kept out of his proximity aggro, :674-675) in Wave
4 batch 3.

**Question for the user.** Should the harness pull (DC engages Fjola on the main tank, recommended:
keeps "no strategy starts an encounter" and matches the doc's player-pull path), or should the fork's
tanks pull by themselves when no real player is in the raid (a plan change)?

**Next window should show.** A DC log line for the engage ~25 s after the gossip, a
`val-kyr-twins` trace per pool, Fjola's first victim the slot-0 tank (`val_kyr_twins.py --tanks`),
Eydis taken by her own tank within a few seconds of reaching the tunnel. Deaths then become the
round-2 data.
