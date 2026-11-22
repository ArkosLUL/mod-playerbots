# Polish log: Yogg-Saron (25-man, Freya only)

Trace slug `yogg-saron`, raid `uld`, scenario `uld-yogg` (DC `DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, -24), 114 yd west of Sara; `TeleportStart` (1896, -25, 332.3), 84.3 yd out, inside the chamber doors that
close 15 s after the pull. The Keeper mask is Freya only, written as instance data by the user's decision, so
every pull is a partial hard mode and a kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py`; `--phases` prints Guardian deaths and, from round 1, when each role got
inside the room.

## Round 1 (2026-11-22, window 7 build 61011f323 + DC 5443dbc; window 6)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 61011f323 --pulls 10`: one valid pull per pool
in window 7, plus window 6's one pull (the baseline). All 0 humans.

| Window | Pool | Run | Trace | Out | Deaths (31 each) |
|---|---|---|---|---|---|
| 6 | uld2 | `tr-20261121-221348-2` | `603_1_yogg-saron_1795299352` | wipe 3:08, phase 1 | Dark Volley 28, Guardian melee 3 |
| 7 | uld3 | `tr-20261122-102849-36` | `603_36_yogg-saron_1795343390` | wipe 3:03, phase 1 | Dark Volley 29, Guardian melee 2 |
| 7 | uld1 | `tr-20261122-103813-40` | `603_40_yogg-saron_1795343955` | wipe 3:24, phase 1 | Dark Volley 24, Guardian melee 7 |
| 7 | uld2 | `tr-20261122-104055-41` | `603_41_yogg-saron_1795344116` | wipe 4:15, phase 2 from 3:48 | Dark Volley 19, Shadow Nova 8 (one through Hand of Sacrifice), Sara's Blessing 2 (both tanks), Guardian melee 2 |

Not counted: uld1 `tr-20261122-103251-37` (`603_37_…`), same picture (raid at 84 yd for ~2:20, 32 deaths to
Dark Volley), flagged `no-combat` because the combat engine never ran.

**Stop rules.** No kill; first round.

**Picked: nothing walks the raid into the room** (all 124 deaths). The raid stood at DC's start, 84 yd out, for
the first ~2 minutes of every pull (`raidpos`: every healer, ranged and melee at 84 yd from 0:10 to 1:55-2:09).
No healer, ranged or melee bot issued a single move before 1:55-2:09 (first accepted move rows: heal
`reach party member to heal` at 1:55-2:07, ranged `reach spell` 1:57-2:09); only the tanks stepped, `tank face`
toward Guardians at 75-81 yd. Meanwhile Sara's timer kept spawning (7 Guardians by 2:00), each walking to a tank
at 72-80 yd and losing ~0.4%/s; no Guardian died before the raid entered (0 in uld1 and uld3 all phase 1, 2 in
window 6). In uld1 84 of 92 taunt attempts failed, nearly all aimed past their 30 yd. Around 1:50 a tank chased a Guardian into
the room (`reach melee` to 38 yd at 1:47.6), died to the pile, the healers followed it in to heal, ranged
followed on `reach spell`, and 7-9 Guardians casting Dark Volley (35 yd, every 8 s) wiped the raid in 30-60 s.
uld2 entered at 2:10, killed 12 Guardians, reached phase 2 and still wiped.

Root cause: phase 1 positioning waits for the room (`YoggSaronInPhase1Room`, 69.3 yd) because a human leads the
raid in. Under the DC driver (master, not in the group, parked) nobody does. The station trigger returned false
past 69.3 yd (`UldTriggers_YoggSaron.cpp:246` before the fix), the leash likewise and also waited for a
Guardian in sight (`:146-147`), the walk guard stood down outside the room (`UldMultipliers_YoggSaron.cpp:371`),
and the phase 1 focus is only picked inside it (`UldEncounter_YoggSaron.cpp:3521`), so dps had no target. Lane L's research assumed "bots walk in from 72-96 yd themselves";
they do not.

Bug, not a plan change: the strategy's phase 1 positions are unchanged, only who starts the walk. Fix
(`UldEncounter_YoggSaron.h/.cpp`, `UldTriggers_YoggSaron.cpp`, `UldMultipliers_YoggSaron.cpp`):
`YoggSaronBotLedRaid` is true while no real player is in the raid group on the instance (as Thorim's bot-led
gauntlet). `YoggSaronInPhase1Approach` is the room, or for a bot-led raid anything out to
`ULDUAR_YOGG_SARON_P1_DOOR_RADIUS` 90 yd (Sara's start radius; DC's drop and wipe revive sit at 114). The station
trigger, the leash and the phase 1 walk guard use it, and a bot-led raid's leash skips the Guardian test, so
ranged and healers walk to the station and melee and tanks to the middle at the pull, with the existing cloud
walk guard holding a line a cloud will cross. A player-led raid is unchanged. Probes: `yogg.p1station` and
`yogg.p1leash` read `door` for a walk begun outside the room. Holds for 10 and 25 and any Keeper count: same room,
same start.

**Next window should show:** in `--phases`, `inside the room after the pull` with every role's median under
~0:20 and nobody `never inside`; `station re-issues` and `yogg.p1leash` with `door` rows; `moves begun outside the
room` marked as the bot-led walk in; Guardian deaths starting before 1:00 and Dark Volley no longer the bulk of
deaths. Then the real phase 1 shows: nova deaths, Guardians woken on the walk in (`CLOUD SPAWNS`, orbits 3-6
with a raider at the mark), taunts out of reach.

**Seen, not picked.** `batch.py` without `--strict` crashes on a `no-combat` trace (`KeyError: 'no-combat'` in
`show_rows`, `FLAG` lacks the key): shared tool. In uld2 both tanks died to Sara's Blessing ticks (2:03, 3:52).

## Round 2 (2026-11-22, window 9 build 1b45df985 + DC 2ce3824)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 1b45df985 --pulls 10`: one pull per pool, 0 humans,
binary 13:10 UTC (holds round 1). Read with `postmortem.py`, `yogg_saron.py --phases --portals --brain --tentacles
--sanity` and scratch scripts over the snapshots (per-wave room clear, per-bot moves and casts after the door).

| Pool | Run | Trace | Out | Phase 1 | Phase 2 | Deaths |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-134059-16` | `603_16_yogg-saron_1795354916` | wipe 14:42 | 162 s | 685 s, 8 waves, Brain 100 → 37.1% | 33 |
| uld2 | `tr-20261122-134601-17` | `603_17_yogg-saron_1795355217` | wipe 13:33 | 151 s | 673 s, 8 waves, Brain 100 → 46.6% | 32 |
| uld3 | `tr-20261122-135323-18` | `603_18_yogg-saron_1795355660` | wipe 12:44 | 148 s | 628 s, 7 waves, Brain 100 → 58.9% | 32 |

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 1, Sara's Blessing 1; phase 2 Corruptor (Draining Poison, Curse of Doom) 15, Squeeze 8,
  Crusher 3, Insane caught below 3, Laughing Skull 1, Psychosis 1.
- uld2: phase 1 Shadow Nova 3 (one through Hand of Sacrifice); phase 2 Corruptor 11, Squeeze 9, Death Ray 3,
  Crusher 2, Insane 2, Brain Link 1, Psychosis 1.
- uld3: phase 1 Shadow Nova 3; phase 2 Corruptor 10, Squeeze 6, Insane 5, Laughing Skull 2, charmed raider 2,
  Crusher 1, Death Ray 1, Deathsworn Zealot 1, Psychosis 1.

Phase 2 cost one death a pull up to 8:17-9:43; the rest died from there to 14:06, waves 5-8.

**Verdict on round 1: fixed.** Every role was inside the room at a median 0:02.1-0:02.8 (`--phases` `inside the room
after the pull`), nobody `never inside`; 75 phase 1 moves a pull begun outside the room, all the station and leash
walk in. Dark Volley deaths 19-29 a pull → 0, phase 1 deaths 31 → 2/3/3, and Sara fell at 2:28-2:42 with 11-12 Guardian
kills on her, where every window 6/7 pull wiped in phase 1.

**Stop rules.** No kill; the walk-in is not picked again.

**Picked: melee on the Brain stand out of reach half the time** (the phase 2 collapse, 86 deaths). Phase 2 runs
on a clock: `AddPortals` cuts `_summonSpeed` 0.1 a wave, and the platform stock (`--tentacles`) went from 3.2-6.4M
at the first three waves to 13-33M by waves 5-7, when the platform raid fell under it. The Brain lost a median
10.5% a wave over waves 1-4 (4.6-16.8%; it has 8.91M at 25, scaled), so phase 3 needed ~7 waves. Per wave the
room clear takes 21-49 s, the walk from the door to the Brain a median 6-19 s (Chamber 160-170 yd), the exit lead
3-6 s, leaving 6-26 s on the Brain.

The bug inside that window: melee at the Brain spent 44-46% of their time out of a swing (`--brain`, new line
`melee at the Brain`: 56% / 55% / 54% within 29.83 yd 3D of 1839 / 1736 / 1543 samples) and hardly stood still
(scratch: from the door to their exit 19-31 s moving, 0-6 s still a wave). The brain spot walk and `reach melee`
traded them 365 / 329 / 317 times a pull. Uldxal, uld1 wave 3: `reach melee` to (1972.92, −25.41, 236.9) at
6:57.5, `yogg-saron brain spot action` back to (1969.0, −25.4, 237.474) at 6:58.0, and so every ~1.05 s until the
exit at 7:17.7.

Root cause: core `2026_11_06_01` re-sniffed `creature_model_info` (`data/sql/updates/db_world/2026_11_06_01.sql:2524`,
display 28951 `CombatReach` 30 → 27). The spot (`UldEncounter_YoggSaron.cpp:80`, returned to every non-healer melee
by `YoggSaronBrainSpot`, `:739-742`) is 30.15 yd 3D from the Brain at (1981.3, −25.43, 265.0): it was inside the
old 32.8 yd swing and is 0.3 yd outside the new 29.83 (`Unit::GetMeleeRange`, 27 + 1.5 + 4/3). `reach melee`
(`ReachTargetActions.cpp:29`, `IsWithinCombatRange(target, 0.75)`: 29.25) walks the bot in to ~29.3, 3.9 yd off
the spot, past the 3 yd `…_BRAIN_SPOT_ARRIVED_RADIUS` (`UldTriggers_YoggSaron.cpp:578`), so the spot walk
(`ACTION_RAID`, forced) takes it back. In uld1 untriggered melee strikes on the Brain went off almost only inside
30.1 yd (Sinister Strike 28 in, 4 at the spot; Heroic Strike 20 / 4; Shred 8 / 3).

Bug, not a plan change: the strategy already means melee to stand on the Brain behind it; only the spot is wrong.
Fix: `ULDUAR_YOGG_SARON_BRAIN_MELEE_SPOT` (1977.0, −25.43, 236.30), 4.3 yd west of the Brain and behind it, 29.0 yd
3D. The floor under the Brain is a bowl (scratch `floor.py`, every player sample within 8 yd over the three
traces): 29.0-29.2 yd 3D out to 7.5 yd, so the whole 3 yd arrival circle is inside reach melee's 29.25 and no
floor is nearer. The z comes from bots that crossed the spot (z 236.24-236.43 within 1 yd); **navprobe was not run**
(the lane may not use Docker), so the orchestrator may want `point 1977.0 -25.43 236.3` on map 603. Healer spots
stay within 31.7 yd of it (was 36.3). Same room and display at 10 and 25, normal and hard mode. Reader: `--brain`
prints `melee at the Brain` (share within a swing, spot ↔ reach trades); `BRAIN_MELEE_SPOT` moved with the code.

**Next window should show:** `melee at the Brain` within a swing in ≥ 90% of samples and trades near 0 (were
365 / 329 / 317); melee mostly still at the Brain. Then the Brain's loss a wave over waves 1-4 above the 10.5%
median, the Brain lower at the wipe than 37-59%, or phase 3. The DK re-spec lands in the same window, so read the
fix on the reach share and the trades, not on Brain %.

**Seen, not picked.**

- Even fixed, ~15% a wave still needs ~5 waves. The next lever is the room clear (21-49 s of the 60): the first hit
  lands 5-10 s after the team arrives at +0.2 s, Stormwind's last tentacle is first hit at +17-24 s, and in the
  Chamber the far ring at +14-28 s; at door-open only 0-6 of 9 were staged (Stormwind 0-3), so the team stood
  85-170 yd from the melee spot.
- The re-sniff moved other Yogg reaches the code still assumes: Yogg-Saron 30 → 20 (`ULDUAR_YOGG_SARON_COMBAT_REACH`
  30, `UldEncounter_YoggSaron.h:441`, the phase 3 area count at `UldMultipliers_YoggSaron.cpp:312`; the doc's
  ≈34 yd melee range is now ≈22.8, the phase 3 melee spot at 18.9 yd 3D still inside it), Crusher 8 → 4.5 (the
  10.83 yd reach behind `…_CRUSHER_REACH_TRIGGER/CLEAR_RADIUS` 11/13 and `…_PHASE_3_RANGED_CRUSHER_CLEARANCE` 18 is
  now 7.33), Guardian 0 → 4. Phase 3 was never reached; audit these with it.
- The Brain's 8.91M is mod-dungeon-scale's, so phase 2 is longer here than on a stock server.
