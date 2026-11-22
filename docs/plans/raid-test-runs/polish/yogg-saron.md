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
