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
(`ReachTargetActions.cpp:31`, `IsWithinCombatRange(target, 0.75)`: 29.25; its trigger `enemy out of melee range`
fires past 29.75, `RangeTriggers.cpp:159`) walks the bot in to ~29.3, 3.9 yd off
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

## Round 3 (2026-11-22, window head 697052475, binary 16:21 UTC)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 697052475 --pulls 10`: one valid pull per pool, 0
humans, holds round 2. Read with the report's `yogg_saron.py` views and scratch scripts over the snapshots (per
Guardian victims and taunts, Guardian health lost and regained, Crusher facing and victim per bot).

**Keepers: one, Freya.** Her passive 62670 was on the raid in all three pulls (`--phases` `Keepers freed`), as DC's
`uld-yogg` row writes it. Not zero, whatever the window's label says.

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per descent | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-172108-24` | `603_26_yogg-saron_1795368118` | wipe 13:23, 34 deaths | 139 s | 438 s, 5 waves: 17.3, 9.6, 22.0, 18.2, 2.9 | 237 s, Yogg 30 → 13.2% |
| uld2 | `tr-20261122-174354-29` | `603_31_yogg-saron_1795369500` | wipe 8:07, 29 deaths | 159 s | 291 s, 3 waves: 16.1, 0.5, 0.4 (Brain at 82.9%) | never |
| uld3 | `tr-20261122-172909-26` | `603_28_yogg-saron_1795368595` | wipe 12:50, 32 deaths | 145 s | 522 s, 6 waves: 13.0, 13.2, 22.4, 1.5, 14.7, 5.3 | 118 s, Yogg 30 → 28.5% |

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 1; phase 2 none; phase 3 Immortal Guardian 33 (one by Drain Life). Before the tanks fell,
  7: six fresh Guardians that left their first taunt for a healer or ranged (11:02-12:20) and one that spawned 11 yd
  from the ranged spot and was woken by their damage. Both tanks at 12:29 / 12:33 under beacon-healed Guardians at
  77-100%, then the loose pile killed the rest.
- uld2: phase 1 Shadow Nova 6 (four melee at 2:28.09, two Guardians dying in one tick by splash during the handover);
  phase 2 Crush 5, Corruptor 4, Squeeze 3, Constrictor 1; at 7:29.97 the ten left alive, the wave 3 Stormwind team
  out too late, went Insane together under Induce Madness, Sara evaded and the script killed them (10).
- uld3: phase 1 Shadow Nova 3 (one melee three times); phase 2 Grim Reprisal 2, Laughing Skull 1, Crush 1, Corruptor
  1; phase 3 Immortal Guardian 20, Corruptor 2, Squeeze 1, Lunatic Gaze 1. Phase 3 opened with 13 leftover tentacles
  (13.9M; waves 4 and 6 lost the Brain 1.5% and 5.3%), the ranged spent 84% of theirs on them, Guardians stayed at
  90%+ and both tanks died at 12:08-12:09.

**Verdict on round 2: fixed.** `melee at the Brain` within a swing 94% / 89% / 95% of samples (round 2: 56 / 55 / 54%),
brain spot ↔ reach melee trades 0 / 0 / 0 (were 365 / 329 / 317). The Brain lost a median 17.8% (uld1) and 13.1%
(uld3) a wave over waves 1-4 against 10.5%, and phase 3 came for the first time, in uld1 and uld3. The DK re-spec
landed in the same window, so the Brain rate is not the fix's alone.

**Stop rules.** No kill. Rounds 1-3 picked three different failures.

**Ranking** across pools: phase 3 Immortal Guardians 53, uld2's Insane wipe 10 (the end of its phase 2 collapse),
Shadow Nova 10 (four separate situations), Corruptor 7, Crush 6, Squeeze 4.

**Top: phase 3 is lost on throughput, a plan question.** uld1 is the clean case (4 leftovers, Heroism at +5.6 s).
Over its phase 3 the Guardians lost 15.4M and regained 14.0M (snapshot health, 91%). 10.75M of the raid's damage went
into Guardians while Empowering Shadows healed them (3.68M / 3.23M / 3.26M / 0.58M across the four beacons), and they
still came out at 25-93%. The ranged put all their 82k/s into Guardians over the 30% floor and nothing into Yogg
(`--phase3` damage split), so Yogg fell ~0.09%/s (~49k/s) on melee alone, 30 → 15.2% by the tank deaths at +172 s. The bots do
what the rules say (floor at 30%, beacon-healed ones fair game, both the user's choices), so this is not a bug.

Question for the user: in phase 3 without Thorim, should the raid leave a Guardian alone while it carries Empowering
Shadows (64486 / 64468) and hit Yogg instead, resuming once the heal ends? **Recommended: yes, for one window.**
Bringing three healed Guardians from full to 30% after the heal costs ~1.2M (3 × 70% of 571,518) against the 3.2-3.7M
spent during it, so ~2M a beacon moves to Yogg (57.2M here, scaled, so 3.5%); over uld1's three beacons before the
tank deaths that is ~10.5% of Yogg, where 15.2% was left. The risk is the one the current rule answered: three full-power healed
Guardians on the tanks and draggers for up to 20 s. In uld1 the tanks took 88-171k and were healed 86-180k in those
windows with the raid on the healed ones; at full power expect roughly a quarter more, against the healed-pile
defensives that exist now.

**Picked second, independent and small: melee area damage makes the bot a Crusher's victim** (6 deaths, and the start
of uld2's collapse: four of its platform raid dead 30 s into phase 2). uld2 3:01-3:12: both tanks and three melee on a
Corruptor 5.2-5.3 yd from a live Crusher. The Crusher was channelling Diminish Power (64145 in its snapshot cast
slot to 3:03.44), its victim flipping between ranged and a pet 14-45 yd out on threat, since the cast zeroes its threat list. The
bear's Swipe (Bear) and glyphed Maul hit it (Infected Wounds on it at 3:02.66, 3:04.48, 3:06.30): their threat made
the bear its victim at 3:03.44 (facing 354°, the bear on its axis) and, melee-class, broke the channel (cast slot 0 by
3:03.95), so it swung at once; his re-hits kept the threat. Not `DamageTaken`: spell damage is
`SPELL_DIRECT_DAMAGE` and its grab fires on white swings only. The bear took 16 Crushes (221k) and lived; the melee `set behind` the
Corruptor onto the bear's spot (off-axis 0-1° at 5.2 yd) and the warrior tank's `tank face` did the same, so the cone
killed Uldyak, Uldyam, Uldyaw (3:08.1-3:09.0) and Uldyaa twice (3:11.0, 3:12.2). The spacing node fired `wedge` at
3:07.7-3:08.3, too late. uld3 9:00: Uldzay's Whirlwind put Deep Wounds on a channelling Crusher 3.3 yd away, it turned
to him at 9:00.46-9:00.67, the channel broken, and Crushed him three times where he stood (9:04.1). Melee get no reach circle on a swinging Crusher
(`GetYoggSaronCrusherReaches`, `UldEncounter_YoggSaron.cpp:4054`) and its victim is exempt from the wedge, so nothing
moved either. Area casts by a bot within a live Crusher's reach + 2 yd in phases 2-3: 28 / 89 / 138 of 477 / 164 /
386, mostly tanks (Maul, Swipe, Hammer of the Righteous, Consecration).

Bug: the strategy already keeps melee off Crushers (`IsAllowedTarget`), area damage goes round it. Fix
(`UldMultipliers_YoggSaron.h/.cpp`, `UldEncounter_YoggSaron.h/.cpp`, `UldDefinition_YoggSaron.cpp`):
`yogg-saron crusher area hold multiplier` (`Family::Spell`) zeroes a harmful `IsRaidAreaDamage` action (not a heal,
not a positive spell such as Heroism or a shout), plus `maul`, `hammer of the righteous`, `heart strike` and `avenger's
shield` (they also hit enemies next to their target), in phases 2 and 3 on the platform while
`YoggSaronInSwingingCrusherReach`: a live Crusher within its live `GetMeleeRange` (4.5 + 1.5 + 4/3 = 7.33) +
`ULDUAR_YOGG_SARON_CRUSHER_AREA_HOLD_SLACK` (2 yd) whose stun does not hold. Same room and reach at 10 and 25, normal
and hard mode, with or without a player. Reader: `--crush` prints the multiplier's vetoes by action.

**Next window should show:** `--crush` `area held in a swinging Crusher's reach` with Swipe (Bear), Maul, Cleave,
Hammer of the Righteous, Consecration, Heart Strike; Crush hits with a tank or melee victim near 0 (uld2 21 on the tanks, uld3 3 on
a melee) and no Crush deaths (were 5 / 1); platform tentacle damage (`--tentacles` live / stunned) no lower than this
window's 43.1 / 67.9k, 20.9 / 11.0k, 40.9 / 56.1k a second.

**Reach audit row (Crusher 8 → 4.5): no cost seen, left.** No phase 3 station shift (`yogg.station` has no `shifted`
in uld1 or uld3), and both Crush deaths' victims stood at 3.3-5.3 yd, inside the live 7.33 reach, where melee never
get the 11 / 13 yd circle. The new hold reads the live reach.

**Seen, not picked.**

- Fresh phase 3 Guardians: the first taunt lands at stasis end (+4 s), mostly a Righteous Defense from 40-65 yd, and
  its 3 s run out mid-walk (Guardians cover ~36 yd in that time), so heal threat takes it to a healer or ranged: six
  of uld1's seven early deaths. A taunt cast during stasis did not hold either (12:14.0 Hand of Reckoning at +3.8 s,
  a healer its victim a second later). Before the tank deaths, tank taunts from beyond 30 yd lost the Guardian
  within 3.5 s 4 times of 14, from inside 30 yd 3 of 30: too thin to build on yet.
- Chamber waves are the weak ones: Brain −0.5 to −9.6% against −13 to −22% for Stormwind and Icecrown, nobody
  staged at door open and the team 149-166 yd from the melee spot.
- uld2's 2:28.09 double nova, during the handover: two Guardians at 0.1% and 2.8%, 5.5 yd apart, the second killed
  in the same tick by the first's nova chain, the simultaneous pair the nova gap cannot cover.

## Round 4 (2026-11-22, window head ebe746ad1)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head ebe746ad1 --pulls 10`: one valid pull per pool, 0
humans, holds round 3; Freya's 62670 on the raid in all three. Read with the report's `yogg_saron.py` views and scratch
scripts over snapshots, `act` and `cast` rows (per phase 3 Guardian: victims from spawn, taunts at it by source node and
range, each tank's taunts cooling when it came within 30 yd).

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per descent | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-201901-18` | `603_18_yogg-saron_1795378786` | wipe 13:01, 34 deaths | 138 s | 443 s, 5 waves: 22.3, 15.6, 4.5, 21.8, 5.8 | from 9:41, 213 s, Yogg 30 → 16.7% |
| uld2 | `tr-20261122-201159-16` | `603_16_yogg-saron_1795378365` | wipe 8:25, 29 deaths | 156 s | 312 s, 3 waves: 4.1, 13.4, 0.6 (Brain at 81.9%) | never |
| uld3 | `tr-20261122-201459-17` | `603_17_yogg-saron_1795378545` | wipe 11:30, 31 deaths | 155 s | 434 s, 5 waves: 16.6, 21.9, 9.1, 15.9, 6.5 | from 9:49, 112 s, Yogg 30 → 25.5% |

Against window 10 (8:07-13:23) the same shape, neither shorter nor worse: uld1 reached phase 3 at 9:41 (9:37) and left
Yogg at 16.7% (13.2%); uld3 one descent sooner, 9:49 (11:07), at 25.5% (28.5%); uld2 again an Induce Madness wipe in
phase 2. Deaths 34 / 29 / 31 against 34 / 29 / 32.

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 2 (Uldxai twice); phase 2 none; phase 3 Immortal Guardian 32 (one by Drain Life). Before
  the first tank death (11:48.6, +128 s), 7, every one a fresh Guardian 4.6-10.7 s old swinging at a ranged or healer
  before a tank held it; then the tanks and the pile.
- uld2: phase 1 Shadow Nova 7 (four melee at 1:45.24 with nothing raid-wide left, the one paladin's Aura Mastery and
  Divine Sacrifice spent at 1:02.7; novas caught the back line three times), Dark Volley 2 (melee, 2:18.3); phase 2
  Insane 11 (one in wave 1, ten of the wave 3 Stormwind team, out late after a 49.4 s clear with 2 melee on the team),
  charmed raiders 4, Squeeze 3, Corruptor 2.
- uld3: phase 1 Shadow Nova 1; phase 2 Grim Reprisal 1; phase 3 Immortal Guardian 29. Before the first tank death
  (10:32.5, +43 s), 5: the first Guardian spawned 66 yd from the melee spot among the brain team walking back from
  (1924.7, −25.4), out of every taunt, and killed four there (9:53.9-10:03.1); a fresh one took a healer at 10:29.1.

**Verdict on round 3: fixed.** Crush deaths 5 / 1 → 0 / 0 / 0, Crush hits 0 / 0 / 2 (uld3, the cone past its victim
onto a melee, 10.8k; were 21 on uld2's tanks and 3 on a melee). `--crush` `area held in a swinging Crusher's reach` 14 /
15 / 2 (Avenger's Shield, Hammer of the Righteous, Heart Strike, Consecration, Death and Decay; Thunder Clap, Shockwave,
Cleave, Bladestorm, Sweeping Strikes). Platform tentacle damage live / stunned 50.6 / 65.2k and 42.3 / 65.3k, not under
43.1 / 67.9k and 40.9 / 56.1k; uld2 12.6 / 15.1k against 20.9 / 11.0k with 7 melee dead in phase 1.

**Stop rules.** No kill. Phase 3 is picked a second time (round 3 asked its throughput question).

**Ranking** across pools: phase 3 Immortal Guardians 61; uld2's phase 2 collapse 20, downstream of its phase 1 (a brain
team of 4 healers, 10 ranged and 2 melee took the Brain 4.1 / 13.4 / 0.6%); Shadow Nova 10 and Dark Volley 2 (9 in
uld2); Grim Reprisal 1.

**Top: phase 3, still the throughput race the pending question covers** (not built). Its numbers here: uld1's three
beacons before the wipe (10:36, 11:24, 12:09) drew 3.93M / 3.67M / 0.91M of raid damage into the healed Guardians during
the heal, which still ended them at 13-100%, 28-93% and 72-100%; Yogg −0.13 /
−1.11 / −0.98% over each 20 s. Ranged 80.2k/s, 76% into Guardians over 30%, 19% tentacles, none on Yogg; Yogg fell
0.077%/s on melee (29.7 → 16.7% in 170 s). uld3 had one beacon (10:44) with both tanks dead within 3 s, and its ranged
spent 94% on the 6 leftovers (5.52M at phase 3). By round 3's sum (three healed Guardians back to the floor cost ~1.2M),
leaving the first two heals alone would have moved ~5.2M, ~9% of Yogg's 57.2M, against 16.7% left: not a kill alone.

**Picked inside it, a bug: the class taunts spend the guardian control's cooldowns.** The deaths before each collapse
are fresh Guardians, and each loose one met tanks with no taunt ready: two tanks hold two or three taunts per 8 s
against a spawn every 10 s, and uld1 swapped 12 times (the taker's taunt plus the holder's 8 s shed). The `lose aggro` nodes
(`TankPaladinStrategy.cpp:125` hand of reckoning → righteous defense → avenger's shield, `BearDruidStrategy.cpp:139`
growl, `BloodDKStrategy.cpp:134` dark command, `TankWarriorStrategy.cpp:219` taunt) ran on the tank's current target
whenever `yogg-saron guardian control` had nothing to do. Over this window's and round 3's phase 3s, before the first tank
death, they cast 26 taunts (4 / 7 / 8 / 7; plus two Death Grips in uld1, dark command's fallback, that moved no Guardian:
grip-immune), none taking a Guardian off a non-tank: 9 on rooted tentacles (uld3 9:52-10:01:
Hand of Reckoning twice on a Corruptor, Growl twice on a Crusher, Righteous Defense on Uldzab taunting two Corruptors and
a Crusher), 14 on Guardians the other tank held (uld1 11:07.7: Righteous Defense on the DK took three, 9 s after the
10:58.5 swap had given them to him), 3 on the caster's own. Round 3's uld1 paladin met the 11:38.2 and 12:10.2 spawns
with Hand of Reckoning spent by the class node 2.7 / 2.3 s earlier; both swung at a non-tank first.

Bug, not a plan change: the strategy gives phase 3 taunting to the control (loose Guardians first, one pile moved only by
the swap rule, never a dragged mark). Fix (`UldDefinition_YoggSaron.cpp`): `yogg-saron phase 3 taunt hold`, a `Block` on
`Family::Taunt` for bot tanks while `YoggSaronInPhase3`. The control, swap and drag cast through `PlayerbotAI::CastSpell`
and are untouched; a held node's fallbacks (Heroic Throw, Avenger's Shield) are pushed at relevance 0.003, so they
almost never win a tick. Same at 10 and 25, normal and hard mode, with or without a player. Reader: `--phase3` prints
`tank taunts by aim` and `class taunts held`.

**Next window should show:** `class taunts held` rows (hand of reckoning, righteous defense, growl, dark command, taunt);
`tank taunts by aim` with no `tentacle` or `own` (here uld1 {loose 27, another tank 23}, uld3 {tentacle 10, loose 7,
another tank 9}); fewer fresh Guardians swinging at a non-tank before the first tank death than 4 of 13 / 2 of 4 (round 3:
4 / 1); the first tank death later than +128 s / +43 s. Phase 3 still ends on the pile until the user answers.

**Seen, not picked.**

- Righteous Defense at stasis end from 56-68 yd, on the zero-threat first victim: its 3 s end mid-walk, and 2 of 4 such
  Guardians in uld1 (10:45.5, 11:15.7) reached a non-tank. The doc means it to reach one still walking in.
- The brain team surfaces at (1924.7, −25.4), 74 yd from the melee spot just outside the Guardian spawn ring, and walks
  through the ring for ~40 s (uld3: 4 of 10 never reached their station).
- uld2's nova cluster: roster B has one paladin with Aura Mastery and Divine Sacrifice, and 6 of 12 Guardian deaths fell
  over 6.5 yd out, catching the back line.

**User answer (2026-11-22) to the Empowering Shadows question:** no, the risk is too great. Instead, ranged
DPS keep their DoTs up on Yogg-Saron in phase 3: today they mostly use area damage and do little to Yogg.

## Round 5 (2026-11-22, no pulls on round 4: merged, not built)

**Data.** Builds the user's answer above on the four phase 3s already logged (deaths by mechanic under rounds 3
and 4): window 12 (`--head ebe746ad1`) uld1 `603_18_yogg-saron_1795378786`, uld3 `603_17_yogg-saron_1795378545`;
window 10 uld1 `603_26_yogg-saron_1795368118`, uld3 `603_28_yogg-saron_1795368595`. Read with the new `--phase3`
`ranged DoTs on Yogg` and scratch scripts over cast rows and snapshots. Stop rules, verdict and ranking skipped:
nothing ran on round 4.

**Measured.** Uptime over each bot's living time, from its casts at Yogg at base durations (a floor: creature
auras have no row). Cast share counts each ranged bot's casts at area / Guardian / tentacle / Yogg, buffs and
heals left out.

| Pull | Phase 3 | Ranged | Casts | DoT uptime on Yogg, % |
|---|---|---|---|---|
| w12 uld1 | 182 s, Yogg 30 → 16.7% | 80.2k/s, 0 on Yogg | 25 / 33 / 33 / 7% | affliction Corruption 69, UA 32, CoA 17; destruction Corruption 23, Immolate 0; demonology (alive 40 s) 100 / 75; shadow SW:P 85, VT 71, DP 0; balance IS 21, MF 7; hunters Serpent Sting 18, 38; fire Living Bomb 14; arcane, elemental none |
| w10 uld1 | 207 s, 30 → 13.1% | 82.2k/s, 0.1k on Yogg | 32 / 45 / 12 / 10% | affliction 90 / 45 / 48; destruction 26 / 0; demonology 100 / 36; shadow 83 / 62 / 0; balance 33 / 13; hunters 29, 16; fire 13 |
| w12 uld3 | 82 s, 30 → 25.5% | 38.3k/s, 0 on Yogg | 1 / 1 / 89 / 7% | shadow 96 / 73; one warlock Corruption 100, Immolate 70; hunter 69; two warlocks, fire, arcane, elemental none |
| w10 uld3 | 87 s, 30 → 29.3% | 9.5k/s, 0 on Yogg | 0 / 0 / 98 / 1% | one warlock Corruption 27; hunter 96 (alive 31 s); the rest none |

Instead, in uld1 the area casts (Volley, Blizzard, Flamestrike, Hurricane, Seed of Corruption, Mind Sear, Chain
Lightning) and Guardian single target took 58-77% of ranged casts, tentacles the rest; in uld3 the leftovers took
89-98%. Yogg only got a DoT from the class "on attacker" nodes, which take any attacker without one: uld1 w12's
affliction warlock put Corruption on Guardians 15 times and on Yogg 6. Ranged damage by target was 0-0.1k/s on
Yogg. A DoT tick is credited to the bot's current target (`dealt` is cumulative), so the trace cannot isolate it:
Yogg's loss minus the melee's target-credited damage leaves −6.2 to +2.6k/s, so DoTs, pets and splash on Yogg came
to a few k/s at most.

**Change (user's answer, so not a bug fix).** `yogg-saron phase 3 yogg dot` (`UldEncounter_YoggSaron.h/.cpp`
`YoggSaronPhase3YoggDot`, `UldTriggers_YoggSaron.h/.cpp`, `UldActions_YoggSaron.h/.cpp`, `UldDefinition_YoggSaron.cpp`,
`ACTION_HIGH + 5`): a ranged non-healer bot in phase 3, off the brain level, casts its first DoT missing on Yogg or
ending within its cast time + 1 s, by `CastSpell` on Yogg, so its target stays with the resolver, guardian control
and the 30% floor. Warlock Corruption, then Unstable Affliction or, without it, Immolate; shadow priest Vampiric
Touch, Shadow Word: Pain, Devouring Plague; balance druid Insect Swarm, Moonfire; hunter Serpent Sting; fire mage
Living Bomb. Only in range and sight and never during a Lunatic Gaze (the cast faces Yogg). Left out: curses (the
class curse choice owns them) and Flame Shock (it shares the shock cooldown the main target's Lava Burst crit
needs). Same at 10 and 25, normal and hard mode, with or without a player. Reader: `--phase3` `ranged DoTs on
Yogg` and `yogg.p3dot` by spell.

**Gain, estimated.** Per-DoT rates at ~3,000 spell power with the usual talents (affliction Corruption and UA
~0.6k/s each, Immolate ~0.45k, VT and SW:P ~0.5k, DP ~0.35k, IS and MF ~0.35-0.4k, Serpent Sting ~0.2k, Living
Bomb ~0.5k). Raising w12 uld1's uptimes to ~90% adds ~2.8k/s on Yogg (2.5-4k/s) beside the melee's 42k/s: ~0.5M over
its 182 s phase 3, 0.8-1.3% of Yogg's 57.2M where 16.7% was left. Each DoT user moves ~5% of its GCDs off Guardians
and tentacles. Not a kill by itself: phase 3 still ends on the tank deaths.

**Next window should show:** `ranged DoTs on Yogg` at 85%+ for each listed DoT of a living ranged bot (here 0-48%
for most); `yogg.p3dot` rows for every listed spell; Lunatic Gaze sanity on ranged no higher; ranged damage by
target otherwise unchanged. Yogg's phase 3 loss rate rises too little to read in one window. Round 4's checks
(`class taunts held`, `tank taunts by aim`, fresh Guardians on non-tanks, first tank death) land in the same window.

**Review fixes.** Living Bomb and Devouring Plague now wait for their own to run out. A refresh never explodes the
bomb (`spell_mage_living_bomb` fires only on expiry or dispel), so the 1 s lead cost a third of each bomb's
damage; refreshing a leech restarts its tick timer (`AuraEffect::CalculatePeriodic` keeps it only for periodic
damage). The node moved to `ACTION_HIGH + 4.1`, under the shadow priest's Shadowform (24.2: a DoT keeps the crit
it was cast with) and low-mana Dispersion (25, a tie before).

## Round 6 (2026-11-23, no pulls on round 5: window 14 in flight)

**Data.** No trace since window 12. Round 5 and the shared `PickDefensive` fix (`a797da29f`) are first pulled in
window 14, so no trace shows this bug yet. Measured on all nine pulls of windows 9, 10 and 12 (rounds 2-4) with
scratch scripts over snapshots, `cast` and `dmg` rows: each Guardian's first read under 10% against its nova, and
each phase 1 ladder cast against the next nova on the caster. Stop rules, verdict and ranking skipped.

**Picked (the orchestrator's bug): the nova defensive stacks a second button on a running one.** `PickDefensive`
(`RaidTankDefensive.cpp:134-169`) counts a running row as cover only while its aura lasts `CoverMs(needMs)`
(`:122-130`): `needMs` for a cut row, 3 s more for a health row. Yogg-Saron's callers, before the fix:

| Caller | `needMs` | After `a797da29f` | Change |
|---|---|---|---|
| `YoggSaronNovaDefensiveTrigger::IsActive` (`UldTriggers_YoggSaron.cpp:893`), `YoggSaronNovaDefensiveAction::Execute` (`UldActions_YoggSaron.cpp:2076`, `yogg.novadef`) | `YoggSaronNovaNeedMs(bot, true)` → `YoggSaronNovaNeedFrom` (`UldEncounter_YoggSaron.cpp:2853`): always 5000 (`…_NOVA_NOW_WINDOW_MS`), its list holding only Guardians under 10% in reach | a 5 s Anti-Magic Shell or Cloak of Shadows reads as no cover the tick after its cast, and a DK casts Vampiric Blood or Icebound Fortitude on top; a Barkskin from the last nova with under 5 s left gets Frenzied Regeneration or Survival Instincts | countdown |
| `YoggSaronNovaAfterOwnButton` (`UldEncounter_YoggSaron.cpp:2865`, under `YoggSaronNovaVictim`) via `BestMagicDefensiveCut` | `YoggSaronNovaNeedFrom(low, member)`: 5000 | a DK under Anti-Magic Shell reads as the next row's cut (Icebound 30, Vampiric Blood 0), so a healer spends an external on him | countdown |
| `YoggSaronCallHumanNova` (`UldEncounter_YoggSaron.cpp:4957`) via `ReadyTankDefensiveSpell` | `YoggSaronNovaNeedMs(human, …)`: 5000, or 12000 off the 10% gate | none: `FindReadyTankDefensive` has no running test | countdown at the 10% gate, same pick (every tank row runs 5 s or more); 12000 kept |
| `YoggSaronBeaconDefensiveTrigger::IsActive` (`UldTriggers_YoggSaron.cpp:931`), `YoggSaronBeaconDefensiveAction::Execute` (`UldActions_YoggSaron.cpp:2167`, `yogg.dragdef`) | 0 | none: healed Guardians hit now and keep hitting, so a running cut row covers to its end and a health row to 3 s before, the shared rule | none |
| `YoggSaronBeaconExternalVictim` (`RunningTankDefensive`, `ReadyTankDefensive`), `YoggSaronTankWrathHeld`, `YoggSaronCallHumanPile`, the `yogg-saron nova defensive hold` Block | none | not `PickDefensive` | none |

**Evidence.** Under 10% to the nova: 135 novas, median 1.2 s, 90% within 3.8 s, 16 past 3.7 s (the findings doc
said never), 5 past 5 s (longest 9.8 s, window 9 uld1 at 1:52.97). Phase 1 ladder casts over the nine pulls (156,
17 a pull): Barkskin 33, Cloak of Shadows 33, Anti-Magic Shell 24, Shamanistic Rage 18, Divine Protection 16,
Icebound Fortitude 10, Last Stand 2, Shield Wall 1; the nova hit the caster a median 1.8 s after the cast, 9 of 81
past 4 s, 3 past 5 s. Each of the 24 Anti-Magic Shells would draw a second DK row under the shared fix. Under the
old rule (any running row covers) no bot put a ladder row over a running one in phase 1 in any of the nine (`--novas`,
new line); the stacks seen fall in phases 2-3 on tanks (bears, a DK), outside the nova node, not looked at.

**Fix** (`UldEncounter_YoggSaron.h/.cpp`). `YoggSaronNovaNeedFrom` returns the time left to the soonest nova in
reach: `…_NOVA_NOW_DUE_MS` (4000) from the Guardian's first read under 10%, then `…_NOVA_OVERDUE_NEED_MS` (1) once
due, so a running button covers while it outlasts the nova and, overdue, until it fades. The first read is kept per
Guardian in the encounter state (`novaGates`), stamped room-wide each tick by `YoggSaronNovaVictim`'s scan (healers
and paladins) and on any need read. Due at 4 s, not 5: a 5 s button cast on the stamping tick reads a few ms short
once its aura first updates (the bot AI runs in `OnPlayerAfterUpdate`, after an aura tick whose diff starts before
the cast), and the next row goes on top; 4 s still covers 90% of novas. The trigger, action and
`BestMagicDefensiveCut` read it unchanged through `YoggSaronNovaNeedMs` / `YoggSaronNovaNeedFrom`; the human call's
pick is unchanged. Nothing reads a player; same room, Guardians and gates at 10 and 25, normal and hard mode. No
shared code. Reader: `--novas` prints `ladder row put up over another still running on the same bot, in phase 1`
(`ladder_stacks`, tested).

**Next window should show:** window 14 (round 5 plus the shared fix, before this one) a DK's Anti-Magic Shell
followed within a second by Vampiric Blood or Icebound Fortitude, so that `--novas` line above 0. On this fix no
row on that line within a second of the one it lands on; a row over one with under 4 s left at the next Guardian's
10% read (7 s for a health row) is the countdown working. Phase 1 ladder casts no more than 17 a pull, and nova
deaths no higher than 1-9 a pull.

**Review fixes.** The reader reads Last Stand by its aura 12976 (`NOVA_DEFENSIVE_AURAS`; the cast id 12975 never
shows as an aura), so `--novas` covers, the stack line and `--phase3` covers see a running Last Stand, and the stack
line leaves humans out. Re-read over the nine pulls, the line stays 0. `yogg.novadef` `covered` dropped from the
checks: the trigger stands down while a row covers, so the action seldom runs to note it.
