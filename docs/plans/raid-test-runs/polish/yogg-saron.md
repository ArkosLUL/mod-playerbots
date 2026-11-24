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

## Round 7 (2026-11-23, window 14 build 1fa3c332c: round 5 + shared `PickDefensive` fix; round 6 merged, not built)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 1fa3c332c --pulls 10`: one valid pull per pool, 0
humans, Freya's 62670 on the raid in all three. The report's boss reader crashed in `--threat` on every trace (two
functions named `taunt_aim`, the phase 3 one shadowing the phase 1 one); fixed in this round, then read with the full
`yogg_saron.py` and scratch scripts over snapshots and `dmg` rows (each phase 1 nova's Guardian position against the
middle, the ranged spot and its holding tank; the same over all twelve pulls of windows 9, 10, 12 and 14).

| Pool | Run | Trace | Out | Phase 1 | Phase 2 | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261123-104610-25` | `603_25_yogg-saron_1795430816` | wipe 11:21, 32 deaths | 136.8 s, 0 deaths | 434.6 s, 5 waves: Brain −20.7, −19.9, 0.0 (Chamber, nobody reached it), −15.9, −13.6 | from 9:31, 120.6 s, Yogg 29.8 → 26.3% |
| uld2 | `tr-20261123-105112-26` | `603_26_yogg-saron_1795431118` | wipe 5:00, 30 deaths | 161.2 s, 13 deaths | 150.8 s, 1 wave, Brain untouched | never |
| uld3 | `tr-20261123-105152-27` | `603_27_yogg-saron_1795431158` | wipe 13:08, 31 deaths | 142.0 s, 6 deaths | 608.3 s, 7 waves: −17.2, −10.5, −18.4, −19.3, −3.1, −0.6; Brain at 30.9% when Induce Madness took the wave 7 team | never |

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 2 Laughing Skull 3 (wave 3 Chamber); phase 3 Immortal Guardian 28, Lunatic Gaze 1 (a fire mage at 25%,
  finished by the gaze ticks). First phase 3 death at +39.7 s, both tanks dead by +81 s.
- uld2: phase 1 Shadow Nova 13: 1:05 one melee; **1:14 eight** (four ranged, two healers, a Fervor holder, a melee
  through Hand of Sacrifice) from a Guardian 9.3 yd out toward the station; 1:22 three melee left low by it with two
  healers dead; 2:22 one melee. Phase 2 Dark Volley 6 (five phase 1 leftovers), Squeeze 2, Corruptor 2, Death Ray 1,
  Induce Madness script kills 6.
- uld3: phase 1 Shadow Nova 6: 1:05 a healer, from a Guardian 7.4 yd out toward the station; 1:40 a Fervor holder
  that never left; 1:45 two melee (two novas 5.4 s apart); 1:54 and 2:13 one melee twice. Phase 2 Corruptor 10,
  Squeeze 6, Malady 1, Death Ray 1, Induce Madness script kills 7.

**Verdict on round 5 (ranged DoTs on Yogg): working on its own check.** Only uld1 reached phase 3. `ranged DoTs on
Yogg`: hunters Serpent Sting 100 / 86%; shadow SW:P 98, VT 86, DP 85; destruction Corruption 99, Immolate 99;
demonology Corruption 100, Immolate 52; balance IS 89, MF 71; fire Living Bomb 77; affliction Corruption 53, UA 44
(window 12 uld1: 0-48% for most). `yogg.p3dot` wrote every listed spell (corruption 16, immolate 9, serpent sting 7,
vampiric touch 6, insect swarm 5, shadow word: pain 5, living bomb 4, moonfire 4, devouring plague 4, unstable
affliction 2). The two ranged with none are the arcane mage and the elemental shaman (Flame Shock, the user's open
question). No gaze cost seen: the one ranged gaze death was a bot at 25% after Guardian hits, its Living Bomb 7 s
earlier. Yogg's rate is not readable: ranged put 98% into six phase 2 leftover tentacles (below).

**Round 6 confirmed needed (not judged).** `--novas` `ladder row put up over another still running`: 2 / 2 / 2, every
one Icebound Fortitude over Anti-Magic Shell within 0.1-0.2 s (uld1 1:09.2, 2:03.2; uld2 1:00.8 twice; uld3 0:59.3,
0:59.4). Phase 1 ladder casts ran 41 / 20 / 41 a pull (Barkskin 20 / 5 / 16) against 17 in windows 9-12: window 14
also built the shared `PickDefensive` fix, so that is the baseline round 6 is read against.

**Stop rules.** No kill. **Phase 3 is stuck**: picked in rounds 3 (question, declined), 4 (class taunt hold) and 5
(the user's DoTs) and still the bulk of uld1's deaths, so it is not picked again; question below. The nova pick
below is a new root cause (round 6 was the defensive picker).

**Ranking** across pools: phase 3 Immortal Guardians 28 (stuck); phase 1 Shadow Nova 19, plus uld2's collapse
downstream of its 1:14 nova (17 more); Corruptor 12; Induce Madness script kills 13; Squeeze 8; Dark Volley 6
(uld2's leftovers); Laughing Skull 3; Death Ray 2.

**Picked: Guardians die off the stack and catch the back line.** uld2's 0:36.7, 0:57.1 and 1:14.2 novas came from
Guardians 8.2, 9.4 and 9.3 yd from the middle at bearing 174-176° (the station is due west, 180°), 12.1-13.3 yd from
the back line: 13, 13 and 11 back-line hits, six back-line deaths at 1:14. Each stood on the tank Uldyaa, 2.9-3.8 yd
out, and the raid kept hitting it under 35% (the focus, the whole raid on it every frame at 0:36), the floor it would have parked at
off the stack. uld3's 0:54.4 and 1:05.0 the same
at 8.4 and 7.4 yd. Over the twelve pulls of windows 9-14, 151 phase 1 novas: every one that died within 15 yd of the
ranged spot caught the back line (11 novas, 137 hits, 9 deaths), none at 15.1-15.9 yd (8); by distance from the middle,
7 of 10 at 8-10 yd caught it, 5 of 18 at 6.5-8, 2 of 73 inside 5.5.

Root cause: core `2026_11_06_01` gave the Guardian's display 28465 a combat reach of 4 (was 0,
`data/sql/updates/db_world/2026_11_06_01.sql:2140`), the reach audit's "no stale site" missing two effects:

1. `YoggSaronDistanceFromMiddle` read `GetDistance2d` (`UldEncounter_YoggSaron.cpp:3218`), which subtracts it, so
   `YoggSaronGuardianOnTheStack` (≤ 6.5) took Guardians out to 10.5 yd: killable below the 35% park floor
   (`YoggSaronPhase1GuardianKillable`) and ignored by the back line's nova dodge (`GetYoggSaronNovaThreats`). The
   taunt pick (its own `GetDistance2d`, `:3267`) and `YoggSaronGuardianCountsForSara` (≤ 15) read the same 4 yd short.
2. A standing Guardian waits 2-6.2 yd off its tank (stationary samples: p50 3.7-4.7, p90 6.2), on the side it came
   from, so a tank inside its 6.5 / 2.8 leash (holding tanks sat a median 0.8-4.2 yd out) held Guardians to ~12 yd.

Bug, not a plan change: the doc's leash and stack are about where the Guardian dies, 6.5 yd from the middle by
centre. Fix (`UldEncounter_YoggSaron.h/.cpp`, `UldTriggers_YoggSaron.h/.cpp`, `UldActions_YoggSaron.cpp`):

- `YoggSaronDistanceFromMiddle` reads the centre (`GetExactDist2d`), so the stack, Sara-count and taunt tests (the
  taunt now through `YoggSaronGuardianOnTheStack`) mean what the doc says. The phase 1 AoE hold keeps the edge read
  (`GetDistance2d`, inline), since a player's area spell adds the target's reach. On the station's side 6.5 leaves
  the spot 15.0 yd off, the measured line.
- `YoggSaronPhase1TankPullSpot`: a bot tank in phase 1, in the room, outside the handover, holding a Guardian off
  the stack (its victim, the lowest such) walks to `…_P1_TANK_PULL_OFFSET` (1.5 yd) past the middle opposite it,
  until within `…_P1_TANK_PULL_ARRIVED` (1 yd). The Guardian follows to 6.2 − 1.5 + 1 = 5.7 yd at most, on the
  stack, and the tank never leaves the 2.8 yd cloud-free radius, so the control's walk (beyond 2.8) never fights it.
  The guardian positioning node carries it (trigger fires on the pull too; the action walks to the pull spot,
  else the middle), probe `yogg.p1pull` (`pull` / `held`).

Same room, Guardians and reach at 10 and 25, normal and hard mode; a human tank's Guardians park as before, only the
bot tanks pull. No shared code. Reader: `--phases` prints Guardian deaths within 15 yd of the ranged spot and the
tank pulls.

**Next window should show:** `--phases` `within 15 yd of the ranged spot` at 0 (here 0 / 3 / 2) and `reaching the
21.5 yd station (over 6.5 yd out)` under 5 / 5 / 4 of 13; `tank pulls onto the stack` above 0; `--novas` `back line
caught by a nova in phase 1` near 0 (here 2 / 14 / 13 bots); nova deaths no higher than 0 / 13 / 6; phase 1 no
longer than 137-161 s (parks cost time if the pull lags). Round 6's checks land in the same window.

**Seen, not picked.**

- Phase 3 in uld1 opened with six phase 2 leftovers (6.08M: three Crushers at 30-65%, two Corruptors at 100%);
  ranged put 98% into them (leftover Crusher first by design), 0% into Immortal Guardians, one Crusher died at
  +42.5 s; every Guardian left a tank within a median +4.0 s and ran at 90%+ for 79% of samples.
- `YoggSaronLowGuardians` and `GetYoggSaronNovaThreats` sweep by `GetCreatureListWithEntryInGrid`, which adds both
  sizes, so their 17 / 20 yd radii now reach Guardian centres at 22.5 / 25.5 yd: the nova ladder reads Guardians
  out of the nova's reach as a threat. Left while round 6 (the ladder) is judged.
- uld3 missed phase 3 by 0.9% of the Brain: wave 6 took 0.6%, and the wave 7 team was taken by Induce Madness
  before a door opened. Corruptor and Squeeze took 16 of its 18 phase 2 deaths before the script kills, from 10:19.
- `--novas` `resurrected straight back into it` matches deaths to any Rebirth or Reincarnation within 5 s, so one
  cast 0.3 s after uld2's 1:14 nova marked 11 deaths as rezzed: read it per target, not as a count.

**Question for the user (phase 3, stop rule).** Tried: the Empowering Shadows question (round 3, declined), the
class taunt hold (round 4), ranged DoTs on Yogg (round 5). With three leftover Crushers at phase 3, the leftover
Crusher rule kept every ranged on them for the whole phase while the Guardians stayed at full power. Should phase 3
ranged take only the lowest leftover Crusher first and the others after the Immortal Guardians? **Recommended: yes**:
one Crusher's Diminish Power is what the rule answers, and a phase spent on three leaves the tanks alone.

**User answers (2026-11-23).** Flame Shock: yes, elemental shamans keep it on Yogg in phase 3 like the other
ranged DoTs (built in round 8). Round 7's phase 3 Crusher question: not as asked, because the goal is to start
phase 3 with at most one Crusher alive; the user asked how many brain room cycles the bots need to bring the
Brain to 30%, and moved the next round to phase 2.

## Round 8 (2026-11-23, window 15a build c350347ff: round 6 built, round 7 not; window 14 for phase 2)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head c350347ff --pulls 10`: one valid pull per pool,
0 humans, Freya's 62670 on the raid. Phase 2 read over these three and window 14's (round 7's data) with
`yogg_saron.py --portals --tentacles --brain` and scratch scripts over snapshots, `cast`, `move`, `act` and
`note` rows: per wave the entries and clicks, the room's time split, the team's damage rate in the room and on
the Brain, the Brain's health.

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261123-114441-12` | `603_12_yogg-saron_1795434326` | wipe 12:10, 32 deaths | 143.2 s | 602 s, 7 cycles: 10.0, 14.3, 7.1, 1.9, 5.2, 8.3, 0 (Brain at 53.2%) | never |
| uld2 | `tr-20261123-114420-11` | `603_11_yogg-saron_1795434306` | wipe 11:45, 31 deaths | 142.1 s | 452.5 s, 5 cycles: 18.4, 17.2, 8.3, 14.0, 12.2 | from 9:54.7, 122 s, Yogg 30 → 28.9% |
| uld3 | `tr-20261123-114320-10` | `603_10_yogg-saron_1795434245` | wipe 9:03, 31 deaths | 147.0 s | 364 s, 4 cycles: 10.1, 7.2, 5.4, 0 (77.2%; Induce Madness took cycle 4's team) | never |

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 4, Sara's Blessing 1; phase 2 Corruptor 11, Squeeze 8, Grim Reprisal 3 (cycle 3, no
  healer below), Death Ray 1, Laughing Skull 1, Brain Link 1, Insane 1, Constrictor 1.
- uld2: phase 1 Shadow Nova 3; phase 2 Laughing Skull 2, Death Ray 1, Insane 1; phase 3 Immortal Guardian 19,
  Corruptor 4, Lunatic Gaze 1 (both tanks dead by +16 s of the first beacon).
- uld3: phase 1 Shadow Nova 10 (six at 1:48.68), Sara's Blessing 1; phase 2 Squeeze 6, Corruptor 4, Constrictor
  1, Malady 1, Brain Link 1, drained Insane below 1, Induce Madness script kills 6.

**Verdict on round 6 (nova countdown): working on its own check.** `--novas` `ladder row put up over another
still running on the same bot, in phase 1`: 0 / 0 / 0 (window 14: 2 / 2 / 2, every one Icebound Fortitude over
Anti-Magic Shell). Ladder casts 39 / 37 / 36 a pull (window 14: 41 / 20 / 41). Nova deaths 4 / 3 / 10, none
with a button up; round 7 (Guardians pulled onto the stack, the nova fix) was not in this build, and window 15
is pulling it now, so it is not judged here.

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5) and is not picked; phase 2 is the user's new
direction and a new pick.

**Cycles to 30%, the user's question.** Five where phase 3 came (window 14 uld1, window 15a uld2); the other
three that left phase 1 never got there (window 14 uld3 30.9% after seven, window 15a uld1 53.2% after seven,
uld3 77.2% after four). Over the 25 cycles whose room cleared the Brain lost a median 10.5% (2-21%; Stormwind
15.9, Icecrown 13.6, Chamber 8.3). The platform stock went from 3.0-5.0M at cycle 1 to 7.5-25.2M by cycle 5-6.

**Why a cycle does so little.** The team on the Brain deals ~50k/s (nine damage dealers at ~6k/s), 0.57% of its
8.91M a second, and only from its first hit after the door until the 60 s cast ends. Medians per cycle: room
clear 30.8 s (Stormwind 29.0, Icecrown 28.5, Chamber 31.3), first Brain hit 2-4 s after the door from
Stormwind and Icecrown and 9-13 s from the Chamber, melee at the Brain 10-12 s after it (Chamber 18 s), exit
decided 6.1 s before the end, surfacing with 2.1-4.1 s spare. Inside, the team dealt 15-24k/s to the tentacles,
a third of its Brain rate, on ~430-480k of tentacles (53,790 each): melee walked 23-52% of the clear between
tentacles on 30-43 yd rings, fresh openers (an assassination rogue 3.5k/s on its first tentacle, ~6k/s on the
Brain), and Grim Reprisal's 40% hold after ~50k dealt. Ranked by Brain per cycle at 0.57%/s:

| Cause | Measure | Brain per cycle | Kind |
|---|---|---|---|
| Room clear | 30.8 s against ~15 s if the team cleared at its Brain rate after the walk in | ~9% | plan (Q1, Q2) |
| Stormwind/Icecrown stage tail, inside the clear | first staged bot to the door: median 10.3 / 10.5 s, 3-6 of 9 damage dealers waiting at the stage spot while 1-3 tentacles die to one bot each | ~2-3% net of the shorter walk, on two cycles in three | plan (Q1) |
| Melee walk to and from the Brain | ranged hit 2-4 s after the door and leave 3 s before the end by their exit portal; melee arrive 10-12 s after it and leave ~6.6 s before the end (31-34 yd to the exit at the 1.5× lead) | ~5% with 7-9 melee on the team | plan (Q2) |
| Chamber far end | team ends 147-166 yd from the melee spot, Brain hit 7 s later than from the other rooms | ~4% a Chamber cycle | layout (Q2 helps) |
| Exit margin | surfacing with 2.1-4.1 s spare | ~1.5% | user-set lead, left |
| **Portal entries (bug, picked)** | 135 of 395 clicks on a spent portal; 41 bot-waves in late (median 3.8 s, up to 18 s), 7 never; 75 of 260 entries over 2 s after the open; 26 bots holding their spot (18 in window 15a) re-assigned 11-40 yd away by the open's rebuild; 2 cycles without their healer | ~0.5% direct, plus deaths below: window 15a uld1's healerless cycle 3 lost 4 bots for cycles 4-7, which took 1.9 / 5.2 / 8.3 / 0% | bug |

**Why the platform loses the race.** At `_summonSpeed` 1 the spawns bring ~78k health a second (Crusher 2.69M every
50-60 s, Corruptor 0.81M every 30-40, Constrictor 0.1M every 15-20), ~130k after cycle 4 and ~190k after cycle 6,
held only during the stun (25-35 s of each 80). The platform dealt 17-45k/s live and 14-67k/s on stunned tentacles
(window 15a uld1 25.3 / 30.3, uld2 42.8 / 43.1, uld3 16.9 / 14.3; window 14 uld1 44.8 / 67.1, uld3 32.5 / 41.0),
under Diminish Power 63% of the Crusher-alive time in window 15a uld1. So the stock only grows, and faster each
cycle: the cycle count decides what phase 3 opens on. In cycle 4 it was already 6.1-10.7M with 2-4
Crushers, so even three cycles leave one or two Crushers for the last stun and the brain hold to take.

**Picked: bots click a portal that is not theirs, or one already used.** Root cause, three parts:

1. `YoggSaronUsePortalTrigger` / `YoggSaronUsePortalAction` (`UldTriggers_YoggSaron.cpp:443`,
   `UldActions_YoggSaron.cpp:1242` before the fix) clicked whatever portal stood within 2 yd, so a bot walking or
   dodging past another's spot took that portal.
2. A used portal stays 1 s: `boss_yoggsaron_descend_portal::OnSpellClick` (core `boss_yoggsaron.cpp:1900-1923`)
   teleports only while its npcflags are set, then clears them and despawns it after 1 s, and the spellclick row
   `(34072, 51347)` casts the visual either way. `YoggSaronPortalPlan` read any live portal as the bot's own or as
   the fallback (`UldEncounter_YoggSaron.cpp:1142-1153`), so the owner held and clicked a spent one, then walked to
   the nearest live portal, often spent or another's too.
3. The rebuild on the tick the portals appear re-picked every spot greedily in team order (`:1091`), moving bots
   already standing on their portal.

Evidence, window 15a uld1 cycle 3: Uldxac, the disc priest and the cycle's one healer, clicked at +0.4, +0.6 and
+0.9 s (51347 each, no Illusion Room) and never got in; Uldxag, off the plan, took a portal at +9.5 s; the room ran
without a healer and Grim Reprisal killed Uldxai twice and Uldxar, a skull's gaze Uldxaj, and Uldxav went Insane below in
cycle 4: four corpses for the rest of phase 2. Uldxap clicked a spent portal at +0.8 and +1.0 s, then the Death Ray dodge and the walk to the
nearest portal traded until `enter gaveup` at +11.9 s; in at +18.8 s.

Bug, not a plan change: the plan already gives each bot a spot and means `holding` to be clickable. Fix
(`UldEncounter_YoggSaron.h/.cpp`, `UldTriggers_YoggSaron.cpp`, `UldActions_YoggSaron.cpp`): `YoggSaronPortalOpen`
counts a portal only while it carries `UNIT_NPC_FLAG_SPELLCLICK`; `YoggSaronPlannedPortal` returns the open portal on
the bot's planned spot when its intent is `holding`, and the use trigger and action click only that; a bot whose own
portal is gone falls back to the nearest open portal whose spot no living teammate still on the platform holds
(`YoggSaronPortalClaimedByOther`); the portals-up rebuild keeps the spot of every team member within the click
radius of it. Same spots and portals at 10 (four) and 25, normal and hard mode; a human takes a portal as before and
the bot whose spot he took falls back. No shared code. Reader: `--portals` per wave prints entries over 2 s after the
open, clicks on a spent portal with each bot's delay, and portals taken by a bot off the plan (`spent_clicks`,
tested).

**Also built: Flame Shock** (the user's answer): `YoggSaronDotSpells` gives a ranged non-healer shaman Flame
Shock, refreshed like the other damage DoTs; the reader's `ranged DoTs on Yogg` already knows it.

**Next window should show:** `--portals` `clicked a spent portal` 0 every cycle (here 135 clicks by 48 bot-waves
over 28 cycles), `in over 2 s after the open` well under 2.7 a cycle (75 of 260), `assigned, walked and never got
one` near 0 (7), every cycle with its healer, no `taken by a bot off the plan`. Then Brain per cycle no lower than
the 10.5% median, and no cycle that loses a healerless team below. In phase 3, `ranged DoTs on Yogg` with Flame
Shock for the elemental shaman near 85% and `yogg.p3dot` `flame shock` rows. Round 7's checks land in window 15.

**Questions for the user** (plan changes, not built):

- **Q1, kill order inside.** In Stormwind and Icecrown a bot that cleared its share waits at the stage spot, so
  the last 1-3 tentacles die to one bot each while 3-6 of 9 damage dealers wait: a median 10.3-10.5 s from the
  first staged bot to the door. Should a done bot instead join the live tentacle with the fewest attackers, staying
  near the stage when it can? **Recommended: yes**: ~2-3% of the Brain on two cycles in three, against 2-4 s more
  walk from the far tentacles.
- **Q2, who goes in.** The brain team is melee first, so most of it reaches the Brain 10-12 s after the door
  (Chamber 18 s) and leaves ~6.6 s before the end, while ranged hit from the door side within 2-4 s and leave
  3 s before the end. Ranged first would add ~5% of the Brain a cycle, but leave melee on the platform, which may not
  hit a live Crusher (Crush re-aims on whoever swings at it), so the Crushers would die only in the stun or to the
  few ranged left. **Recommended: no, keep melee first**: the platform is already losing the Crusher race, and Q1
  plus this fix come first. Worth asking again if cycles stay at ~10%.
- **Answer to the user's question, for the goal.** At ~0.57% a second the team needs ~44 s on the Brain for 25% a
  cycle, out of 60 s whose room alone takes ~31 s; a 15 s room would still give ~20%. Expect four cycles at best on
  these rosters, not three, and one or two Crushers at the last stun.

**Seen, not picked.**

- uld2's phase 3 (the first in window 15a): ranged spent 99-100% on nine phase 2 leftovers (10.24M, three Crushers)
  for the whole phase; round 7's leftover Crusher question still stands.
- uld3's 1:48.68 nova killed six (four ranged, a healer, a melee): round 7's pull fix was not in this build.
- Chamber cycles took a median 8.3% against 13.6-15.9%: the team ends at the far ring; no inside order found that
  shortens the out-and-back walk overall.

**Review fixes.** The use-portal trigger now reads the plan, and a bot keeps `"diamond"` underground until its
room node renames it, so `YoggSaronPlannedPortal` returns nothing off the platform (`YoggSaronRoomOf` not
`ARENA`), like the enter-portal trigger: the arrivals sit 91-98 yd from the nearest portal, and a plan read past
the 100 yd portal search sees the portals down, rebuilds the next wave's slots mid-wave, and the platform's next
read rebuilds again without the held spots. The holder count above was re-measured: slot notes at each open
against each bot's last snapshot within 2 yd of its old spot.

**User answers (2026-11-23) to round 8's questions.** Q1 yes: in Stormwind and Icecrown a brain team bot that
has finished its share of tentacles joins the live tentacle with the fewest attackers, staying near the stage
spot when it can, instead of waiting at the stage spot (round 9 builds it, and checks the Chamber for the same
idle wait). Q2 yes, keep melee first in the brain team: no change.

## Round 9 (2026-11-23, window 15 build 523b2f60f: round 7 built, round 8 not)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 523b2f60f --pulls 10`: one valid pull per pool,
0 humans, Freya's 62670 on the raid in all three. Read with the full `yogg_saron.py` and scratch scripts over
snapshots, `act`, `cast`, `heal` and `note` rows (per wave the team's time split below by room state and target,
every Induce Madness charm with the bot's last action and long casts, reprisal holds against the heals they got).
Window 16 (build `2d7853f0e`, round 8) pulls Yogg-Saron last and had not reached it when this round closed, so
round 8 is not judged.

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261123-125914-19` | `603_19_yogg-saron_1795438800` | wipe 11:33, 33 deaths | 137.9 s, 0 deaths | 361.9 s, 4 cycles: Stormwind 15.2, Icecrown 22.1, Chamber 13.2, Stormwind 19.4 | from 8:20 with one Corruptor left (0.66M), 205 s, Yogg 29.3 → 11.3% |
| uld2 | `tr-20261123-130035-20` | `603_20_yogg-saron_1795438881` | wipe 10:26, 30 deaths | 139.6 s | 449 s: Chamber 8.6, Stormwind 11.2, Icecrown 10.4, Chamber 0.2 (team charmed), Stormwind never cleared; Brain at 69.2% | never |
| uld3 | `tr-20261123-130215-21` | `603_21_yogg-saron_1795438981` | wipe 12:46, 33 deaths | 140.5 s | 519.7 s, 6 cycles: Stormwind 14.8, Chamber 7.7, Icecrown 18.3, Stormwind 11.4, Chamber 6.1, Icecrown 11.8 | from 11:00 with 8 tentacles (9.4M), 118.5 s |

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 none; phase 2 none; phase 3 Immortal Guardian 33 (first +7.1 s, the first tank at +101.8 s).
- uld2: phase 1 Shadow Nova 1; phase 2 Death Ray 1, then Induce Madness: wave 3's hunter Uldyaj charmed in the
  brain room (killed by the script 8:05), wave 4's Chamber team of 9 charmed at 8:22 (script kills 9), the
  charmed raiders killed 9 on the platform (both tanks among them), Psychosis 2, Squeeze 1, and wave 5's
  Stormwind team, 7 bots under a collapsing platform, never cleared and was charmed (script kills 6).
- uld3: phase 1 Shadow Nova 3 (0:38, 1:14, 2:08); phase 2 Death Ray 1; phase 3 Immortal Guardian 27, Lunatic
  Gaze 1, Corruptor 1.

**Verdict on round 7 (Guardians pulled onto the stack, read from the centre): fixed.** `--phases` `within 15 yd of
the ranged spot` 0 / 0 / 0 (window 14: 0 / 3 / 2); `reaching the 21.5 yd station (over 6.5 yd out)` 1 of 12, 1 of
12, 2 of 13 (5 / 5 / 4 of 13); `tank pulls onto the stack` 17 / 15 / 12 (none before); `--novas` `back line caught
by a nova in phase 1` 0 / 0 / 0 bots (2 / 14 / 13); nova deaths 0 / 1 / 3 (0 / 13 / 6), Guardian deaths a median
3.0-4.6 yd from the middle; phase 1 137.9-140.5 s (137-161). Round 6's stack line 1 / 0 / 0 (uld1: an
Anti-Magic Shell over Icebound Fortitude at 1:57).

**Round 8 baseline from this window** (its portals built after it): clicks on a spent portal 13 / 7 / 14, entries
over 2 s after the open 13 / 14 / 22 of 143 taken, assigned and never got one 2 / 3 / 2, one portal taken off the
plan.

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5), not picked. uld1 met the user's phase 2 goal: four
cycles, phase 3 with one tentacle up, and Yogg still went only 29.3 → 11.3% over 160 s with the ranged on
Guardians 68-100% of every 10 s.

**Ranking** across pools: phase 3 Immortal Guardians 60 (stuck); uld2's Induce Madness chain 28; Shadow Nova 4;
Death Ray 2; Lunatic Gaze 1; Corruptor 1. Brain per cycle over the 13 cycles whose team reached it: median 11.8%
(round 8: 10.5%), Stormwind 14.8, Icecrown 15.1, Chamber 8.2.

**Built first: Q1, the user's answer.** Team time below per wave (scratch, snapshots by `yogg.roomstate` and target):
`staged` took 8-21% of Stormwind teams' time and 5-18% of Icecrown's (uld2 wave 3: 49 s, three bots 16 s each),
while the last tentacles died to one bot each. `YoggSaronTickIllusionShare` (`UldEncounter_YoggSaron.cpp`): when a
bot's share leaves nothing allowed in a live room, `YoggSaronTentacleToJoin` allows the live tentacle with the
fewest attackers (room members, healers out, by `GetTarget`, `YoggSaronTentacleAttackers`), ties to the one
nearest the room's stage spot, keeping the bot's current one while no other has fewer; share note `joined`. Nothing
stages any more, so the `staged` room state, `yogg-saron illusion stage hold`, the stage walk (trigger and action)
and `…_STAGE_ARRIVED_RADIUS` are gone; the stage spots stay as the tie-break. **The Chamber has no such wait**:
it keeps plain spread (no share), and its teams spent no time staged; their still time without a target was the
reprisal hold (3-16%) and 1-2 s of gaps, so nothing was applied there. Same rooms at 10 and 25, normal and hard
mode, with or without a player (a human's target counts as an attacker). Reader: `--brain`'s door line prints
`joined a tentacle`.

**Picked: a cast that outlasts the exit (inside uld2's Induce Madness chain).** Over every phase 2 trace since
window 9 (18 pulls), each Induce Madness charm with the bot's last action and long casts: uld2's hunter Uldyaj
began Revive Pet (982, 8.8-9.2 s) in the brain room at 6.9 s (window 9), 7.8 s (15a) and 5.8 s (15)
before the end and was charmed every time, with no `yogg.exit` verdict: no node runs while the bot casts (uld2
window 15: `revive pet` OK at 6:56.83, its next row at 7:05.75, teleported). `YoggSaronExitHoldCastMultiplier`
held cast-time spells only once the bot was latched leaving, and the latch comes at the lead (3 s here, 9 yd from a
portal). The rest of the chain is not a bug of this kind: wave 4's Chamber cleared at 45.9 s (two of eight damage
dealers sat 26-28 s in reprisal holds the healer never ended, a tenth bot never got a portal), and the other team
charms since window 9 came from clears that opened the door 1.7-13.8 s before the end, or never, leaving no
reachable portal.

Bug: the strategy means a brain room bot to leave on its lead. Fix (`UldEncounter_YoggSaron.h/.cpp`,
`UldMultipliers_YoggSaron.cpp`): the exit read keeps the lead it computes per bot (`exitLeadMs`,
`YoggSaronExitLeadMs`); `YoggSaronInduceMadnessLeftMs` reads the cast. In the brain room before the latch, the
multiplier zeroes a cast-time spell, channel or ability lock whose cast time (channels and locks: duration) plus the
lead exceeds the time left; latched, it holds them all as before. Instants are never held. Same room at 10 and 25,
normal and hard mode, with or without a player. Reader: `--brain`'s `caught by it` prints the cast a bot that never
decided was still in (`cast_at_end`, tested); here `casting Revive Pet 982 from 5.5 s left`.

**Next window should show:** `--brain` door lines in Stormwind and Icecrown with `0/N staged` and `joined a
tentacle` above 0, share `joined` rows; Stormwind and Icecrown room clears under this window's (Stormwind 24.1-32.7
s, median 28.9; Icecrown 18.6-32.0, median 23.6); Brain per cycle above 11.8%. No `caught by it` bot `casting …`
and uld2's Uldyaj never charmed. Round 8's portal checks against the baseline above, and Flame Shock on Yogg in
phase 3.

**Seen, not picked.**

- Reprisal holds the one room healer cannot end: 20 / 177 / 122 s held a pull here, 25 holds over 10 s in windows
  14-15; uld2 wave 4's two ended only at the 15 s Induce Madness release. During such holds the healer's casts
  went mostly to others (Renew, Prayer of Mending, Circle of Healing on bots at 28-90%), 0-3 to the held bot.
  Question below.
- uld1's phase 3 (the first opened as the user wants): ranged on Guardians over the 30% floor 90-100% of the phase,
  every fresh Guardian off its tank at +4 s (13 of 17 to melee), Yogg −0.11%/s; still the stuck item.
- Chamber cycles lose a median 7.7% against 14.8-15.1%: the team ends 161-169 yd from the melee spot and the room
  clear runs 22.6-45.9 s.

**Question for the user.** In an illusion room, should the room's healer heal a reprisal-held bot first, since it
deals nothing until 70%? **Recommended: yes**: uld2's wave 4 lost two of eight damage dealers for 26-28 s of a 46 s
clear, and the charm that followed wiped the pull; the risk is a mate under Grim Reprisal waiting a heal longer.

**Review fixes.** `YoggSaronTentacleToJoin` also keeps the bot's current tentacle while no lower GUID room member is
on it, the spread's settle rule. A joined bot's share allows only the one tentacle it returns, so `SpreadTarget`'s
own keep never applies, and two joiners on one tentacle that both read another with fewer attackers before either
switched would leave together and come back together. The Icecrown orphans, whose cluster's last tentacle dies
under all of them at once, are the likely case.

## Round 10 (2026-11-23, window 16 build 2d7853f0e: round 8; window 17 build 8bc2a82d3: rounds 8 and 9)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 2d7853f0e --pulls 10` and `--head 8bc2a82d3`: one
valid pull per pool per window, 0 humans, Freya's 62670 on the raid. Read with the full `yogg_saron.py` and scratch
scripts (`scratchpad/yogg-r10/`) over snapshots, `note`, `cast`, `act` and `move` rows: per wave the team by role,
its damage on tentacles and on the Brain, where each bot stood when the door opened and when it reached the Brain,
where joiners went, late portal entries against the hazards at their spot, phase 1 raid damage per window.

| Window | Pool | Run | Trace | Out | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|---|
| 16 | uld1 | `tr-20261123-141451-20` | `603_20_yogg-saron_1795443342` | wipe 12:02, 32 deaths | 5: Icecrown 21.0, Chamber 9.5, Stormwind 9.6, Icecrown 19.2, Chamber 10.7 | from 9:45.9 with 3 tentacles (Crushers 4.7, 94.0%; Corruptor 92.4%), Yogg 30 → 19.7% |
| 16 | uld2 | `tr-20261123-141451-21` | `603_21_yogg-saron_1795443347` | wipe 11:32, 31 | 5: I 18.3, C 7.0, S 12.9, I 23.0, C 8.8 | from 9:42.7 with 6 (Crushers 42.2, 25.3, 88.7%; Corruptors 14.8, 74.4, 83.4%), 30 → 24.8% |
| 16 | uld3 | `tr-20261123-141411-19` | `603_19_yogg-saron_1795443297` | wipe 11:28, 31 | 5: C 13.6, S 17.1, I 17.3, C 9.8, S 12.2 | from 9:38.9 with 5 (Crushers 92.3, 98.0%; Corruptors 21.8, 68.2, 96.0%), 30 → 24.7% |
| 17 | uld1 | `tr-20261123-145333-8` | `603_8_yogg-saron_1795445659` | wipe 11:49, 32 | I 14.3, C 8.1, S 11.5, I 6.6, C 0.8, S never cleared; Brain 58.8% | never |
| 17 | uld2 | `tr-20261123-145233-7` | `603_7_yogg-saron_1795445599` | wipe 10:24, 30 | S 10.8, I 5.1, C 1.3, S 2.3, I never cleared (three healers); 80.5% | never |
| 17 | uld3 | `tr-20261123-145554-9` | `603_9_yogg-saron_1795445800` | wipe 14:10, 33 | C 9.6, S 15.0, I 13.7, C 4.9, S 3.6, I 12.2, C 1.5, S never cleared; 39.6% | never |

**Cycles to 30%:** window 16 five in every pull, the first window where all three reached phase 3; window 17 none
(six, five and eight cycles, the Brain at 58.8 / 80.5 / 39.6%). Phase 3 opened with 3 / 6 / 5 tentacles, two,
three and two of them Crushers (uld1's 4.7% one died at +4.1 s), against the user's goal of at most one.

Deaths by mechanic (a rezzed bot dying again counts twice):

- w16 uld1: phase 1 Shadow Nova 2 (Uldxai twice); phase 2 Death Ray 1; phase 3 Immortal Guardian 29.
- w16 uld2: phase 2 Death Ray 1, Induce Madness script kill 1 (Uldyaj); phase 3 Immortal Guardian 29.
- w16 uld3: phase 1 Shadow Nova 2 (Uldzaj twice); phase 2 Corruptor 1; phase 3 Immortal Guardian 28.
- w17 uld1: phase 1 Shadow Nova 6 (three ranged at 1:10.9); phase 2 Laughing Skull 1, Death Ray 1, Squeeze 8,
  Corruptor 7, Psychosis 1, Induce Madness script kills 8 (cycle 6's team, room never cleared).
- w17 uld2: phase 1 Shadow Nova 6 (four melee at 1:51.75); phase 2 Grim Reprisal 1, Laughing Skull 2, Squeeze 5,
  Corruptor 6, Malady 1, Psychosis 2, Brain Link 1, Induce Madness script kills 6.
- w17 uld3: phase 1 Shadow Nova 4; phase 2 Laughing Skull 4, Death Ray 3, Squeeze 8, Corruptor 8, Brain Link 2,
  Psychosis 1, Induce Madness script kills 3.

**Verdict on round 8 (own-portal clicks, Flame Shock): spent clicks fixed, the rest not.** Clicks on a spent portal
0 in every cycle of both windows (window 15: 13 / 7 / 14). Entries over 2 s after the open 26 / 23 / 20 of 50
(w16) and 22 / 18 / 28 of 58 / 41 / 74 (w17) against 13 / 14 / 22 of ~48 a pull in window 15: no better. Never got
one 0 / 0 / 0 (w16), 2 / 3 / 7 (w17, most in collapse waves). A cycle without its healer: w16 uld3 cycle 1 (ten
melee, the bug picked below), w17 uld3 cycle 5 (Uldzac assigned, walked, never got a portal) and cycle 8.
Flame Shock on Yogg 88 / 92 / 87% (w16; w17 had no phase 3), `yogg.p3dot` `flame shock` rows.

**Verdict on round 9: the join made cycles worse; the exit cast hold works.** `joined` rows 15-29 a pull, 4-6 bots a
Stormwind or Icecrown wave. Room clears against window 15 (Stormwind 28.9, Icecrown 23.6) and window 16, the same
build staged: Stormwind 31.6 (24.5-39.8) and Icecrown 29.7 (26.0-37.1), w16 28.6 and 22.1. At the door nobody
stood within 10 yd of the stage spot (w16: 12 bot-waves over its 9 such waves), melee reached the
Brain a median 11.7 s after the door (w16 9.8 s), the first Brain hit came 7.1 s after it (w16 4.4 s), and
Stormwind / Icecrown cycles took a median 11.2% (w16 17.3%). All 30 joins that took a tentacle took one
53-85 yd from the stage, 26 of them with none inside 40 yd alive: the last tentacles are the far ones, so a joiner
ends the clear
there instead of walking back to the door. Joiners dealt 30k / 64k in all (uld1 / uld3), mostly walking 22-35 yd
to their tentacle. The Chamber, untouched by it, also slowed (median 27.2 → 36.0 s), so not all of it is the join:
w17's teams were weaker (16 phase 1 deaths left 1-4 melee dead, uld2's cycles 1-4 went down with 1-5 melee).
Exit hold: no `caught by it` bot casting; 40 holds in uld1 (Army of the Dead 8, Slam 6, Frostbolt 5). uld2's
Revive Pet hunter Uldyaj was charmed again at 7:05, casting nothing: the Chamber cleared at +38.5 s and he
reached the brain room with 3.9 s left, 37 yd from the portal he walked to.

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5). Rounds 8-10 picked three different phase 2 failures.

**Ranking** across the six: phase 3 Immortal Guardians 86 (w16, stuck); w17's phase 2 collapse 59 (Corruptor 21,
Squeeze 21, Induce Madness 17, all from cycle 4-5 on with 10-22M of tentacles on the platform); Shadow Nova 20
(16 in w17); Laughing Skull 7; Death Ray 6; Psychosis 4; Brain Link 3; Malady, Grim Reprisal 1 each.

**Top: the collapse is slow cycles, and its plain cause is round 9's join, a plan question** (below, not built).
The rest of w17's loss: phase 1 raid damage was the same in all three windows (80-104k/s), so the bots were not
weaker; the teams were (dead and benched melee), and with the per-second Brain rate unchanged once the team hits
(0.80 against 0.86%/s, uld1 cycle 1), the cycle lost its seconds to the clear and the walk.

**Picked, a bug: the brain team drops its healer when the melee fill it.** `GetYoggSaronBrainTeam`
(`UldEncounter_YoggSaron.cpp:1014-1022` before the fix) built melee, the first healer, ranged, other healers, then
cut to the spot count, so ten living unbenched melee at 25 (roster C has exactly ten) or four at 10 pushed the
healer off. w16 uld3 cycle 1: ten melee went down, no healer (w15 and w17 uld3 each lost a melee in phase 1, so it
did not show). The doc's rule is "melee first, then exactly one healer". Fix: with a healer available the melee band
stops one slot short. Same at 10 and 25, normal and hard mode, with or without a player (a human non-tank master
already takes one slot off the size). No shared code. Reader: `--portals` wave lines print the team by role and
`NO HEALER` (`room_team_roles`, tested).

**Next window should show:** every `--portals` wave line with `heal 1` unless its healer missed the portal, and a
uld3 cycle with ten melee alive going down as nine melee and a healer. Round 9's join is still built, so cycles
should read as w17's unless the user answers the question first.

**Question for the user (round 9's join, a plan change).** Finished brain team bots that join a tentacle end the
clear 53-85 yd from the door instead of at it, and the clear got no shorter. Should a finished bot go back to
waiting at the stage spot when no live tentacle is within 40 yd of it (`…_STORMWIND_HELPER_RADIUS`, the strategy's
"near the stage"), joining only one inside that? **Recommended: yes**: melee reached the Brain ~2 s sooner and the
first hit ~3 s sooner when staged, about 2% of the Brain on two cycles in three; nothing measured paid for the join.

**Seen, not picked.**

- Late portal entries: of 18-28 a pull, 43% had a Death Ray within its 14 yd clear radius of the bot's spot at the
  open, most trading the phase 2 spacing node against the portal walk; 2-5 a pull were moved to a farther spot
  by the portals-up rebuild, which keeps only bots within 2 yd of theirs.
- Phase 1 double novas: w17 uld2 1:47.86 / 1:51.75 (3.9 s apart, four melee dead) and uld3 1:00.57 / 1:05.33: the
  raid took a second Guardian under 30% while the first waited out the gap, then Death and Decay ticks, Deep
  Wounds, running DoTs and 0-5 `… on attacker` DoT casts a pull on parked Guardians (uld2: Curse of Agony at 1:50.3
  and Corruption at 1:51.3 on a 3% one) finished it inside the gap.
- w17 uld1 1:10.92: a Guardian dying 2.6 yd from the middle hit 20 raiders: cloud dodges had left the back line at
  18.2-19.5 yd from the middle, 16.1-17.4 yd from it; three ranged died.

**Review fixes.** A non-tank healer human master already counts as the team's one healer, so with him no bot
healer goes and the melee fill the bot slots: the fix as built sent a bot healer beside him and one melee fewer,
two healers in a room whose rule is exactly one.

## Round 11 (2026-11-23, window 19 build 99188e0f1: round 10 built, round 9's join still in)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 99188e0f1 --pulls 10`: one valid pull per pool, 0
humans, Freya's 62670 on the raid. Read with the full `yogg_saron.py` and scratch scripts (`scratchpad/yogg-r11/`)
over `note`, `move`, `cast` and snapshot rows: per late portal entry the bot's intents and accepted walks from
25 s before the open, slot changes at the open's rebuild, the corridor trash's path, exit walk times.

| Pool | Run | Trace | Out | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|
| uld1 | `tr-20261123-171514-15` | `603_15_yogg-saron_1795454160` | wipe 12:25, 33 deaths | 6: I 20.0, C 2.3, S 16.8, I 13.6, C 5.1, S 12.2 (held from 36.0%, phase 3 4.4 s before the end) | from 10:53.6 with 11 tentacles (9.47M; Crushers 8.7, 78.7, 96.7%), Yogg 30 → 27.2% |
| uld2 | `tr-20261123-170952-13` | `603_13_yogg-saron_1795453838` | wipe 13:50, 31 | 7: S 13.8, I 14.6, C 9.3, S 13.0, I 12.0, C 5.8 (held from 36.1% at 10.2 s left, survived at 31.5%), S 1.6 (held, phase 3 2.7 s before the end) | from 12:22.7 with 15 (17.50M; six Crushers 30-100%), 30 → 28.2% |
| uld3 | `tr-20261123-171333-14` | `603_14_yogg-saron_1795454059` | wipe 11:38, 30 | 5: S 18.0, C 9.4, I 18.9, S 13.8, C 9.9 (held from 36.0% at 9.5 s left, survived at 30.0%, phase 3 7 s after the end) | from 9:56.5 with 7 (6.70M; Crushers 72.5, 99.5%), 30 → 25.5% |

**Cycles to 30%:** six, seven and five; window 16 five in all three, window 17 none. Phase 3 opened with 11 / 15 / 7
tentacles, three, six and two of them Crushers: the user's goal of at most one was not met. Brain per cycle, held
cycles left out: Stormwind / Icecrown median 14.2% (window 17 11.2%, window 16 17.3%), Chamber 7.2% (2.3-9.4). Room
clears: Stormwind 24.6 s, Icecrown 23.1, Chamber 31.4 (window 17 31.6 / 29.7). From Stormwind and Icecrown the first
Brain hit came a median 8.8 s after the door (window 16 4.4, window 17 7.1), melee on the Brain at 13.8 s (9.8 / 11.7),
4-6 joiners a wave ending at the far tentacles.

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 5 (Uldxai twice; 1:10.6 a Guardian at the middle caught 8 of the back line, Uldxau
  finished by Sara's Blessing; 2:04.9 a leftover killed by splash 5.9 s after the last nova, Uldxas and Uldxau);
  phase 2 none; phase 3 Immortal Guardian 23, Corruptor 2, Faceless Horror 2, Lunatic Gaze 1.
- uld2: phase 1 Shadow Nova 1; phase 2 Death Ray 1, Corruptor 1; phase 3 Immortal Guardian 22, Corruptor 5 (four
  inside the collapse at 13:09-13:13), Lunatic Gaze 1.
- uld3: phase 1 none; phase 2 Corruptor 1; phase 3 Immortal Guardian 23, Faceless Horror 2, Void Beast 2, Brain Link
  1, Deafening Roar 1.

**Verdict on round 10 (one healer per brain team): fixed.** Every `--portals` wave line reads `heal 1`, 18 of 18
(window 16 uld3 cycle 1 went down with ten melee). uld3 (roster C, ten melee, no death before 9:19) sent cycles 1 and
2 down as nine melee and a healer. Phase 2 deaths 0 / 2 / 1 (window 17: 59 in all).

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5) and is not picked. Rounds 8-11 picked four different phase 2
failures (round 8's portal clicks fixed the spent clicks; this is the late entries it left).

**Ranking** across pools: phase 3 Immortal Guardians 68 (stuck); Corruptor 9 (7 in phase 3, from the leftovers);
corridor trash 6 (harness, below); Shadow Nova 6; Lunatic Gaze 2; Death Ray, Brain Link, Deafening Roar 1 each. Phase 2
is nearly deathless now, so the pick is the user's direction: cycles to 30%.

**Picked: brain team bots are off their portal spot when the portals open.** 83 of 180 entries came over 2 s after
the open (21 / 38 / 24), 9 of 18 waves with their healer 2-15 s late. 55 bot-waves had stood on their spot
(`holding`) and were off it at the open: 28 walked off by `yogg-saron body detour`, 22 by the phase 2 spacing dodge
(meant), the rest by the portal walk's own re-route or nothing recorded. An entry whose walk carried a detour walk
came a median 2.8 s late (21 of 32 over 2 s), the rest 0.7 s (62 of 148); windows 16-17 4.4 against 0.2 s (73 of 96).
And the open's rebuild then handed 6 bots that stood 2.7-7.4 yd off their spot a farther one (7.7-16.2 yd); a
seventh reads so only because a Constrictor's Lunge moved it at the open.

uld1 wave 2, the window's worst cycle (Chamber, 37.1 s clear, Brain −2.3%): Uldxac, the disc priest and the room's
healer, reached slot 8 at 4:36.37 (`holding`); at 4:36.67 the detour walked it toward (1996.2, −44.1), after a
Constrictor across the body; at the open (4:37.77) it stood 7.2 yd off, the rebuild gave slot 8 to Uldxan (melee,
detoured off its own slot 9 the same way) and Uldxac slot 2, 16 yd off; in at +15.2 s, and the room ran with its
healer 33.7 yd from the middle. uld2 wave 5: Uldyap holding at 8:38.98, detoured at 8:39.24 toward a Crusher across
the body, grabbed by a Constrictor 18 yd off its spot at 8:41.9, in at +19.3 s.

Root cause, two parts:

1. `YoggSaronBodyDetourTrigger::IsActive` (`UldTriggers_YoggSaron.cpp:665-688`) has no portal gate. The detour walks
   toward the bot's target the way reach does, but it is its own node (`ACTION_RAID − 1`), not a `Family::Reach`, so
   `yogg-saron portal spot hold` (`UldDefinition_YoggSaron.cpp:151`) never blocks it, and under `holding` the portal
   walk (`ACTION_RAID`, `UldTriggers_YoggSaron.cpp:394-407`: spreading and late only) does not run to outrank it.
   Window 19: 32 detour walks under a pending intent (30 `holding`, 2 `late`); windows 16-17: 86.
2. The plan's assignment loop (`UldEncounter_YoggSaron.cpp:1132-1158` before the fix) gave the free spots in team
   order, so at the open's rebuild an earlier off-spot bot took the spot of a later one standing a few yards from it.

Bug, not a plan change: the plan already means a bot to stand on its spot until its portal appears. Fix
(`UldTriggers_YoggSaron.cpp`, `UldEncounter_YoggSaron.cpp`): the body detour stands down while
`YoggSaronPortalWalkPending` (spreading, holding, late); the plan gives free spots by nearest pair, the closest free
bot and spot first, repeatedly, holders within the click radius still kept first. Same spots at 10 (four) and 25,
normal and hard mode; a human takes his own portal as before. No shared code. Reader: `--portals` prints `body detour
walks while holding a portal spot`, and per wave `left a held spot before the open` by the walks that took them
off and `moved off a near spot by the open's rebuild` (`walks_while_holding`, `left_held_spot`, `rebuild_losses`,
tested).

**Next window should show:** `body detour walks while holding a portal spot: 0` (here 9 / 13 / 8); no `yogg-saron
body detour action` in any `left a held spot` line (here 8 / 12 / 8 bot-waves); `moved off a near spot` near 0 (here 1
/ 3 / 3, one a Constrictor's Lunge at the open); `in over 2 s after the open` under 21 / 38 / 24; no healer in over 5 s
late (here 6 waves). Brain per cycle no lower than 14.2% (Stormwind, Icecrown) and 7.2% (Chamber); round 9's join is
still built, so read the fix on the entries.

**Harness, for the orchestrator (DC, by analogy with the Freya, XT-002 and Kologarn trash).** The Descent into Madness
still holds two Faceless Horrors (33772, 2.36M each) at (1880, 78, 342) and (1821, 77, 342), ~100 yd north-west of
Sara, which a real raid has cleared. An enhancement shaman's Feral Spirit wolves, left on the platform while the shaman
is in the Stormwind room below, path toward its x/y and out the corridor: uld3 3:52 pulled one onto the platform, a
warlock pet held it through phase 2, it summoned a Void Beast (33806) at 6:21, and the two killed 4 in phase 3; uld1
10:23 the same, 2 deaths. Recommended: despawn both on `uld-yogg`.

**Question for the user (exit lead, the user's 1.5×).** Over six pulls (windows 16 and 19), 241 melee exits from the
Brain: decided 6.6 s before the end, surfaced a median 3.1 s later (p90 3.3, max 6.0), 3.6 s to spare (p10 3.3). At 1.2×
the lead for the usual 31 yd is 5.3 s: 1.3 s more for every melee on the Brain, ~0.8% of it a cycle, ~5% over six;
the one 6.0 s exit would have been charmed. **Recommended: yes, 1.2× for one window.**

**Still open with the user, not built:** a finished bot goes back to the stage wait unless a live tentacle is within
40 yd (yes); an illusion room's healer heals a reprisal-held bot first (yes).

**Seen, not picked.**

- The brain hold cannot finish a late start: held from 36.0-36.1% with 9.5-10.7 s left, the Brain survived at
  30.0-31.5% (uld2 here, cost a seventh cycle and 10 → 15 tentacles; uld3 here; window 16 uld1; window 14 uld3 at
  34.6% with 12.6 s left). The hold took nothing from those teams (they never reached 32%); they were too slow.
- uld1's 1:10.6 nova: a Guardian dying at the middle caught 8 of the back line, as window 17 uld1's at 1:10.9 did (cloud
  dodges leave the back line 16-19 yd out).
- Chamber cycles 2.3-9.9%: clears 27.9-37.1 s with melee walking 52-65% of them, then 146-169 yd to the melee spot.

**Review fixes.** The detour stands down only under `yogg-saron portal spot hold` (`YoggSaronHoldingPortalSpot`),
where reach does too. Spreading or late, a portal walk that gave up (17-64 `enter gaveup` notes a pull) hands the
tick to reach, and only the detour keeps reach off the body: uld1's two late detours (Uldxai 4:43.4 and 4:44.3, walk
given up) stood in for a `reach melee` whose line passed 13.2 and 14.4 yd from the middle (8.2 yd a few ticks
before), at the 13.3 yd knockback's edge. The 30 holding detours stay gated. `left_held_spot` no longer raises when a hold and the next intent share a millisecond.
Nearest pair re-run on this window's positions at each open keeps 4 of the 6 near bots; Uldxac and Uldzam lose their
spot to a nearer bot either way, so `moved off a near spot` falls only as far as the gate keeps bots on their spot.

**User answers (2026-11-23) to the two open questions.** (1) Yes: an illusion room's healer heals a bot held by
Grim Reprisal first. (2) Yes: a brain team bot that finished its share goes back to the stage wait unless a live
tentacle is within 40 yd of the stage spot, and joins only a tentacle inside that 40 yd (narrows round 9's join).
Round 12 builds both. The exit lead question (1.5× to 1.2×) stays open and is not built.

## Round 12 (2026-11-23, window 21 build 90400b39c + DC 66174c7: round 11 built, the Faceless Horrors despawned)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 90400b39c --pulls 10`: one valid pull per pool, 0
humans, Freya's 62670 on the raid, no DC veto inside a pull. Read with the full `yogg_saron.py` and scratch scripts
(`scratchpad/yogg-r12/`: portal entries per role, Horror and Void Beast units, rezzes) over `cast`, `note` and
snapshot rows. Window 19's report (`scratchpad/window19/window19-yogg-saron.md`) is the baseline.

| Pool | Run | Trace | Out | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|
| uld1 | `tr-20261123-190822-14` | `603_14_yogg-saron_1795460948` | wipe 11:39, 32 deaths | 5: I 20.6, S 11.7, C 12.9, I 11.8, S 13.0 (held from 36.2% at 12.2 s left, phase 3 0.5 s after the end) | from 9:44.8 with 6 tentacles (4.18M; Crushers 7.0, 6.4, 95.4%), 128 s, Yogg 30 → 25.7% |
| uld2 | `tr-20261123-191123-15` | `603_15_yogg-saron_1795461129` | wipe 14:25, 30 | 7: I 15.7, S 12.8, C 8.8, I 13.9, S 11.5, C 4.2, I 1.8 (held from 33.1% at 16.7 s left with 26 tentacles up, survived at 31.3%); wave 8's six bots never opened a door | never |
| uld3 | `tr-20261123-190742-13` | `603_13_yogg-saron_1795460907` | wipe 13:38, 32 | 5: I 21.9, S 11.7, C 8.3, I 15.1, S 12.9 (held from 36.1% at 14.0 s left, phase 3 4.4 s before the end) | from 9:39.1 with 3 (1.44M; one Crusher at 12.3%, dead at +7.1 s), 253 s, Yogg 30 → 7.5% |

**Cycles to 30%:** five, none (seven cycles, Brain at 31.3%) and five; window 19 six, seven and five. Phase 3 opened
with 6 / – / 3 tentacles (window 19: 11 / 15 / 7). uld3 met the user's goal of at most one Crusher, and its phase 3
took Yogg to 7.5%, the lowest yet (11.3% in window 15); uld1 opened with three, two of them under 7.1%. Brain per
cycle, held cycles left out: Stormwind / Icecrown median 13.4% (window 19 14.2%), Chamber 8.6% (7.2%). Room clears
Stormwind 26.1 s, Icecrown 25.1, Chamber 28.1 (24.6 / 23.1 / 31.4). From Stormwind and Icecrown the first Brain hit
came a median 8.5 s after the door and melee were on the Brain at 14.0 s (8.8 / 13.8), with 5-7 joiners a wave.

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 3 (Uldxai, the enhancement shaman, three times: Reincarnation stood him up on the stack
  0.2 s after the 2:04.5 nova, and he was back up 1.5 s and 3.2 s after the next two); phase 2 Crush 1 (Uldxat,
  2:37.6); phase 3 Immortal Guardian 28 (first at +47.3 s, both tanks by +80 s).
- uld2: phase 1 Shadow Nova 4 (one nova at 1:16.5, two melee and two ranged); phase 2, all from 11:32 (cycle 7 on,
  26 tentacles up): Corruptor 13 (Curse of Doom 8, Draining Poison 5), Squeeze 6, Crush 1, Psychosis 1, Induce
  Madness script kills 5.
- uld3: phase 3 Immortal Guardian 32 (two finished by Drain Life; first at +78.3 s, both tanks at 13:01-13:02). No
  death in phases 1-2.

**Verdict on round 11 (body detour held on a held portal spot, nearest-pair spots): fixed.** `body detour walks while
holding a portal spot` 0 / 0 / 0 (window 19: 9 / 13 / 8), and no `left a held spot` line names the detour (8 / 12 / 8
bot-waves): what is left is the phase 2 spacing dodge (meant), Malady and the portal walk. `moved off a near spot by
the open's rebuild` 0 in all 18 waves (1 / 3 / 3). Entries over 2 s after the open 15 / 20 / 18 of 50 / 72 / 50 (21 /
38 / 24 of 60 / 70 / 50), median entry 0.2-0.3 s (0.2 / 2.4 / 1.8). Healers: over 5 s late in 4 waves (6), median
entry 0.2 s (1.75 s). Brain per cycle did not rise: round 9's join is still in this build.

**Verdict on the Horror despawn (DC `66174c7`): fixed.** No Faceless Horror (33772) or Void Beast (33806) unit in any
of the three traces, and no death to either (window 19: Faceless Horror 4, Void Beast 2).

**Reader fix.** `--portals` read one plan rebuild whose `yogg.portalslot` rows straddled a millisecond as two plans,
and named bots on their own spot as `taken by a bot off the plan` (uld1 wave 2: six, uld3 wave 5: three). It now folds
rows within 50 ms into one plan (`plan_rounds`, tested); re-read, every wave has 10 assigned and none off the plan.

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5) and is not picked. This round builds the user's two answers.

**Ranking** across pools: phase 3 Immortal Guardians 60 (stuck); uld2's phase 2 collapse 26; Shadow Nova 7; Crush 1.

**Built: the user's two answers** (`UldEncounter_YoggSaron.h/.cpp`, `UldTriggers_YoggSaron.h/.cpp`,
`UldActions_YoggSaron.h/.cpp`, `UldDefinition_YoggSaron.cpp`):

1. **Stage wait, join only near the stage.** `YoggSaronTentacleToJoin` takes only live tentacles within
   `…_STAGE_JOIN_RADIUS` (40 yd) of the room's stage spot, by the same key as round 9 (fewest attackers, then nearest
   the spot, current one kept). With none, the share latches `staged`: the room state reads `staged`, the illusion
   room node's stage walk (`StageRequired` / `WalkToStage`, arrival `…_STAGE_ARRIVED_RADIUS` 1.5 yd) and `yogg-saron
   illusion stage hold` come back as round 9 removed them. For the measure: in window 17 all 30 joins that took a
   tentacle took one 53-85 yd from the stage, and window 16 (staged) had melee on the Brain at 9.8 s and Stormwind /
   Icecrown cycles at 17.3%.
2. **Reprisal heal first.** `yogg-saron reprisal heal` (`ACTION_RAID − 1`, under the healer station walk, above every
   class heal band and Nature's Swiftness): a bot healer in an illusion room casts a direct heal on the lowest held
   room mate under 70% in heal range and sight (`YoggSaronReprisalHealTarget`), while its own health is 40% or more.
   Spells by class (`YoggSaronReprisalHealSpell`): Penance, Flash Heal, Renew if missing; Riptide, Healing Wave,
   Lesser Healing Wave; Holy Shock, Holy Light, Flash of Light; Rejuvenation if missing, Swiftmend, Nourish. Probe
   `yogg.reprisalheal` (spell). Baseline here: `--brain` `heals in a hold` (new, tested) 14 / 16 / 12 heals on a held
   bot against 21 / 23 / 17 on others in its room (window 19: 41 / 53, 35 / 75, 7 / 28); holds 11 / 10 / 9, median 6.6
   / 3.8 / 3.5 s.

Same rooms at 10 and 25, normal and hard mode. With a player: his target counts as an attacker in the join, and he
is never held (the hold latches in each bot's own tick), so a bot healer puts only held bots first; a human healer
heals as he chooses. No shared code.

**Next window should show:** `--brain` door lines in Stormwind and Icecrown with `staged` above 0 and fewer than 5-7
`joined a tentacle` (0 in Icecrown, see the review note); `room shares` and `room states` with `staged`; melee on the Brain sooner than 14.0 s and the
first hit sooner than 8.5 s after the door; Stormwind / Icecrown Brain per cycle above 13.4%. `heals in a hold` mostly
on the held bot, `reprisal heal` rows by spell, hold medians under 6.6 / 3.8 / 3.5 s. Round 11's entry numbers no
worse.

**Seen, not picked.**

- uld2's collapse: cycle 6 (Chamber) took 4.2% and cycle 7 started its hold from 33.1% with 16.7 s left and 26
  tentacles up, survived at 31.3%, and the next team never opened a door; the brain hold cannot finish a slow cycle
  (round 11's note again).
- Healers still entered over 5 s late in 4 waves (uld1 6.1 and 12.2 s, uld2 8.2, uld3 6.8); not traced yet.
- Uldxai's Reincarnation stood him up inside the phase 1 stack 0.2 s after a nova, and he died to the next one: three
  deaths in uld1, the same bot as window 16's and window 19's two.

**Review notes.** In all three window 21 pulls no Icecrown tentacle or Deathsworn Zealot stood within 40 yd of
`…_ICECROWN_STAGE_SPOT` (nearest 56.2 yd; the clusters' centres sit 61-74 yd off), so in Icecrown the join never fires
and a finished bot always waits, as in round 8; Stormwind had 4-6 units 31-40 yd from its spot. `heals in a hold`
counted heals on bots outside the healer's room (2 / 1 / 0 here) as `on others in its room`; it now counts only room
mates, and the baseline above is re-read with it.

## Round 13 (2026-11-23, window 22 build a88ea2ade: round 12 built)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head a88ea2ade --pulls 10`: one valid pull per pool, 0
humans, Freya's 62670 on the raid, no DC veto inside a pull. Read with the full `yogg_saron.py` and scratch scripts
(`scratchpad/yogg-r13/`) over `cast`, `heal`, `act`, `note` and snapshot rows: each reprisal hold against the room
healer's casts and positions, every Death Ray hit against the victim's casts, each healer's mana at every wave.
Window 21's three pulls (round 12's data) are the baseline.

| Pool | Run | Trace | Out | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|
| uld1 | `tr-20261123-202316-9` | `603_9_yogg-saron_1795465442` | wipe 12:20, 33 deaths | 5: C 14.6, I 21.1, S 16.1, C 7.2, I 11.0 (held from 36.1% at 23.6 s left, phase 3 7.7 s before the end) | from 9:30.3 with 4 tentacles (2.18M; one Crusher at 10.4%, dead at +5.6 s; Corruptors 37.3, 100, 98.2%), 183 s, Yogg 30 → 17.4% |
| uld2 | `tr-20261123-202256-8` | `603_8_yogg-saron_1795465421` | wipe 13:02, 31 | 6: S 11.4, I 21.7, C 8.8, S 11.9, I 8.2, C 0.7; wave 7's Stormwind team of six never opened its door; Brain at 37.3% | never |
| uld3 | `tr-20261123-201954-7` | `603_7_yogg-saron_1795465240` | wipe 11:53, 32 | 5: S 15.9, C 11.3, I 25.4, S 6.2, C 11.2 (held from 36.1% at 10.7 s left, survived at 30.0%, phase 3 3 s after the end) | from 9:57.6 with 6 (5.30M; Crushers 26.4, 96.3%; three Corruptors 78-80%; a Constrictor), 129 s, Yogg 30 → 25.1% |

**Cycles to 30%:** five, none (six, and a seventh whose team never cleared) and five; window 21 the same. Phase 3
opened with 4 / – / 6 tentacles (window 21: 6 / – / 3). uld1 met the user's goal: one Crusher, at 10.4%, dead in 5.6 s.
uld3 opened with two (26.4%, 96.3%) and ranged spent 95% of phase 3 on tentacles under Diminish Power (on the raid 81% of
the phase). Over windows 21-22, two pulls of six opened phase 3 with at most one Crusher: **not reliable yet**.

**Phase 3 numbers for the stuck item** (rounds 3-5). With the goal met, phase 3 still ends on the Immortal Guardian
pile: window 22 uld1 Yogg 30 → 17.4% in 183 s (149k/s raid damage, 27% on Yogg: melee 61% on Yogg, ranged 2% on Yogg,
60% on Guardians over 30%, 29% on the Corruptors); 13 of 14 fresh Guardians off their tank at a median +4.1 s, 11 of
them onto melee; 35% of Guardian samples at 90% or more; first death +60.3 s. Window 21 uld3 (one Crusher at 12.3%):
30 → 7.5% in 253 s. With two Crushers (uld3 here) 30 → 25.1%. So fewer Crushers buys 8-18% of Yogg, not the kill.

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 3 (Uldxai, the enhancement shaman, stood up by Reincarnation 0.2 s after the 1:05.4 nova
  and killed by the next; Uldxad, the resto shaman, at 1:12.4); phase 2 none; phase 3 Immortal Guardian 30 (one by Drain
  Life; first +60.3 s).
- uld2: phase 1 Shadow Nova 2 (2:01.9: Uldyaw through Hand of Sacrifice, Uldyax finished by Sara's Blessing); phase 2
  Death Ray 5 (3:19.5 a hunter mid Revive Pet; 8:45.9 a warlock mid Incinerate, a wave 5 team member; 10:22.6 twice
  and 11:30.5), Corruptor 10 (Curse of Doom 5, Draining Poison 5), Squeeze 8, Psychosis 1, Induce Madness script kills 5
  (12:30, wave 7's team). All but the first Death Ray from 8:45 on.
- uld3: phase 1 Shadow Nova 1; phase 2 Death Ray 1 (6:21.0, in a Constrictor's grab); phase 3 Immortal Guardian 30
  (first +49.5 s).

**Verdict on round 12, stage wait: fixed.** `staged` room states 17 / 17 / 23; at the door 4-6 of 8-9 staged in every
Stormwind and Icecrown wave but uld3's wave 4 (0 of 7, 5 joined: its tentacles stood inside 40 yd); joined at a door 0 /
1 / 5 in all (window 21: 5-7 a wave). From Stormwind and Icecrown the first Brain hit came a median 4.1 s after the door
(8.5) and melee were on the Brain at 11.5 s (14.0); Brain per cycle there 15.9% over the nine cycles cleared without a
brain hold (13.4%), Icecrown 21.4, Stormwind 11.9. Clears: Stormwind 29.1 s, Icecrown 19.1, Chamber 28.2 (26.1 / 25.1 /
28.1); Chamber cycles 8.8% (8.6%).

**Verdict on round 12, reprisal heal: fires, not mostly on the held bot.** `reprisal heal` 13 / 18 / 14 casts; `heals
in a hold` 30 / 23 / 24 on a held bot against 50 / 95 / 29 on others in its room (14 / 16 / 12 against 21 / 23 / 17);
holds 12 / 17 / 17, median 4.6 / 4.5 / 5.4 s (6.6 / 3.8 / 3.5), no shorter. The cause is the pick below, plus uld2's dry
healer (question). Round 11's entries: over 2 s after the open 21 of 50 / 19 of 65 / 16 of 48 (15 / 20 / 18 of 50 / 72 /
50), body detour walks while holding 0 / 1 / 0.

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5) and is not picked. Rounds 8-12 picked five different phase 2
failures; this is the reprisal heal's own check.

**Ranking** across pools: phase 3 Immortal Guardians 60 (stuck); uld2's phase 2 collapse 28 from 8:45 (Corruptor 10,
Squeeze 8, Death Ray 4, Induce Madness 5, Psychosis 1); Shadow Nova 6; Death Ray 2 outside it.

**Picked: a held bot out of the room healer's sight is never healed.** uld2's collapse came out of its sixth cycle
(Chamber, 40.7 s clear, Brain −0.7%), where three of nine damage dealers sat held 9-27 s. Uldyag, held at 40% from
10:18.7, walked in by the regroup to 15.0 yd of the healer at 10:26.7 and waited there 12 s: from 10:25.7 to 10:38.0
the healer cast seven times on mates at 74-89% (PW:S, Prayer of Mending, Renew) and never on him, and no `yogg-saron
reprisal heal` row exists from 10:25 to 10:37. Its trigger was false with the healer at 100% and standing still, the
held bot in the same room, at 40% and 15 yd: of `YoggSaronReprisalHealTarget`'s tests only sight is left. The same in
all three pulls, from the same healer spot, (2133.3, −26.3, 242.0), the Chamber station on its last tentacles at the
raised floor's ramp: uld1 7:34.2-7:36.6 the druid cast three heals on held mates at 31-37% past Uldxaf at 27% and
15.2 yd, and reached him at 7:40.9 only after the station walk had moved her ~10 yd; uld3 5:14.8-5:18.7 four casts went
past Uldzag (37-40%, 15.2 yd) and Uldzai (38%, 15.8 yd), one of them the reprisal heal itself on Uldzaw at 41%. Every
skipped bot stood on the low floor (z 239.7-242.7) toward the room's middle; mates on the raised floor in the same
direction were healed.

Root cause: both heal picks need sight. `YoggSaronReprisalHealTarget` (`UldEncounter_YoggSaron.cpp:4420`) and
`PartyMemberToHeal::Check` (`PartyMemberToHeal.cpp:145`) skip a mate failing `IsWithinLOSInMap`, so neither the reprisal
heal nor any class heal picks it, and `reach party member to heal` never walks to it. `YoggSaronReprisalRegroupTrigger`
(`UldTriggers_YoggSaron.cpp:507`) walks a held bot only past 25 yd and stops it at 15 yd on the straight line, which in
the Chamber lands it below the ramp's lip. Measured: held bots under 70% within 25 yd of the room healer and unhealed
for 3 s, Chamber 72 bot-seconds over the three pulls (healed 50), Stormwind 35 (32 of them uld2's wave 7 under a 1.3%
mana healer). `--brain` `passed over` (new): 7 of 13 / 12 of 34 / 7 of 14 heal casts in a hold went past a lower held
mate in heal distance.

Bug: the regroup exists to bring a held bot where the healer can heal it. Fix (`UldEncounter_YoggSaron.h`,
`UldTriggers_YoggSaron.cpp`, `UldActions_YoggSaron.cpp`): the regroup also fires while the room healer has no line of
sight to the held bot, at any distance, and then walks it to `…_REPRISAL_REGROUP_SIGHT_STOP` (4 yd) of the healer
instead of 15; probe `yogg.regroup` (`range` / `sight`). Same rooms at 10 and 25, normal and hard mode; a human healer
counts as the room's healer as before. No shared code.

**Picked second, independent and small: a bot mid-cast cannot dodge a Death Ray.** The AI yields while a cast-time spell
prepares, so the phase 2 spacing node never runs for it. Over windows 16-22 (15 pulls) four ray hits landed on a bot
casting from before the ray crossed the 9 yd trigger, three of them lethal: uld2 here, hunter Uldyau's Revive Pet (982,
8.9 s) begun 6.3 s before the hit, the ray inside 9 yd for the last 1.3 s; warlock Uldyap's Incinerate (1.85 s) at
8:43.8, two rays stacked inside 9 yd for 1.4 s before 2 × 19,400 at 8:45.9; window 17 uld1's Starfire. A ray walks its
9 yd legs at ~5.5 yd/s, so the dodge has about 1.3 s. Fix (`UldEncounter_YoggSaron.h/.cpp`, Mimiron's
`CutMimironCastsInFire` shape): `YoggSaronCutCastsInDeathRays` in the encounter tick, on a 250 ms instance scan,
requests the interrupt for every bot on the platform that can move, casts with over
`…_DEATH_RAY_CAST_FINISH_MS` (500) left and has a live ray within the 9 yd trigger; probe `yogg.castcut` (spell). Same at
10 and 25, normal and hard mode; real players are never touched. No shared code. Reader: `--crush` Death Ray hit lines
name the cast running in the 1.5 s before (`cut` once cut) and print `casts cut for a ray`.

**Next window should show:** `--brain` `regroup walks` with `sight` rows; `passed over` well under 7 of 13 / 12 of 34 /
7 of 14; `heals in a hold` mostly on the held bot; Chamber hold medians and clears under this window's (28.2 s median)
and Chamber cycles above 8.8%. `--crush` `casts cut for a ray` rows and no Death Ray death on a bot `casting` (here 2).
Round 12's door numbers no worse (first hit ~4 s, melee on the Brain ~11.5 s, Stormwind / Icecrown 15.9%).

**Question for the user (brain team healer, an assignment).** The room healer is the first by Val'anyr, then priest,
druid, shaman, paladin, then GUID, so the same priest goes down in 4-5 of every 5-7 waves and loses 20-40% mana a
trip. uld2 sent its disc priest to wave 6 at 17% (paladin 70%, shaman 42%, holy priest 37%): out at 1.8%, the room
took 40.7 s and the Brain 0.7%; then the holy priest to wave 7 at 2% (paladin 59%), and that room never cleared.
Window 19 uld3 sent priests at 41%, 49% and 5% to waves 3-5. Should a healer under 30% mana when the plan picks the
next team go after every healer above it, latched per wave like the Sanity bench? **Recommended: yes, for one
window**: the cost is a less mobile paladin or shaman on some trips; a dry healer stalls the room's reprisal holds,
which is what ends a pull's phase 2.

**Still open with the user, not built:** the exit lead from the Brain 1.5× to 1.2× (round 11).

**Seen, not picked.**

- Constrictor grabs in uld2: 38, longest 21.5 s, 8 Squeeze deaths and two Death Ray deaths of grabbed bots (uld3 one
  more); 5 rescues cast.
- Uldxai, the enhancement shaman, Reincarnated onto the phase 1 stack and died to the next nova again (fourth window
  running).
- The brain hold again could not finish a late start: uld3 held from 36.1% at 10.7 s left, survived at 30.0%, and
  phase 3 came 3 s after the end anyway.

**Review.** Both fixes hold. uld2 10:18.7-10:45.3 reread: Uldyag stood at (2118.4, −24.3, 242.7), 15.0 yd from
Uldyad at (2133.3, −26.3, 242.0), at 40% for 12 s while Uldyad, at 100% health and 9-16% mana, put PW:S, Prayer of
Mending and Renew on mates at 74-89%; `PartyMemberToHeal::Check` tests only map, charm, twice heal distance and sight,
so sight is the one test left, as the log says. That is inferred, not navprobed, and the findings doc now says so;
Uldyag stood level with the healer (uld1's Uldxaf at z 239.7 was below). A grabbed bot rides the Constrictor's seat,
where `CanMove` is false (`IsInVehicle(true)`), so the cut never touches it; the hold ends once the room's tentacles
are dead, so the sight regroup never pulls a bot back off the brain walk. No size, mode or player branch: a human
room healer is walked to like a bot one, and only bots' casts are cut.

- Review fix: `--crush` read uld2's two grabbed ray deaths (Uldyax's 0.5 s Slam at 10:05.9, Uldyae's Chain Heal at
  10:22.6) as `casting`, four in all against this log's 2; a victim inside a Squeeze aura now reads `grabbed`, and
  uld2 reads 2. Its `Death Ray 5` counts Uldyae twice (death rows at 10:22.57 and 10:23.07), not a rez.
- Review fix: `regroup walks` counted `yogg.regroup` notes, which as a derived note are written only when a bot's
  value changes; it now counts bots per reason.
- Review fix: the reader's new constants and the tests' fixtures carried description comments; removed, with
  `REGROUP_SIGHT_STOP` (only a label used it). `REPRISAL_RESUME_PCT` keeps a one-line note to change it with the
  module's.
- Left: a cut bot whose dodge finds no spot casts again and is cut again every 250 ms while the ray stays within
  9 yd; the dodge's fallback spots make that rare, and no window has run the cut yet.
- Left: a sight regroup is one move, so the bot walks the whole way to 4 yd even when sight comes back sooner, and a
  melee bot walks back once at 70%.
- Left, older than this round: the reader's `SPELL_SQUEEZE` is the 25-man 64126, so at 10-man `--phase2`'s grabs
  and `--crush`'s `grabbed` read nothing (10-man's aura is 64125).

**User answer (2026-11-23) to round 11's exit lead question:** yes, lower the exit lead from the Brain from 1.5× to
1.2× for one window (round 14 builds it). Round 13's healer mana question stays open, not built.

## Round 14 (2026-11-23, no pulls on round 13: merged, not built)

**Data.** No Yogg-Saron trace since window 22 (`--head a88ea2ade`). The baseline for this change is the new `--brain`
`melee exits from the Brain` over window 22's three pulls and windows 16, 17, 19 and 21 (15 pulls in all). Stop rules,
verdict and ranking skipped: nothing ran on round 13.

**Built: the user's answer.** `ULDUAR_YOGG_SARON_EXIT_LEAD_SAFETY` (`UldEncounter_YoggSaron.h`) 1.5 → 1.2, the
multiplier on the walk time in the exit read's lead (`YoggSaronLeaveBrainLevelNow`): `max(…_EXIT_LEAD_FLOOR_MS` 3 s,
2D distance to the nearest Flee portal / 7 yd/s × 1.2`)`. For melee at the Brain, 31 yd from their portal, 6.6 → 5.3 s.
Round 9's exit cast hold reads the same lead (`YoggSaronExitLeadMs`, stored per bot by the exit read), so before the
latch a cast now runs when its cast time plus 5.3 s fits in what is left. Ranged and healers by their portal keep the
3 s floor, which now holds out to 17.5 yd (was 14). Same at 10 and 25, normal and hard mode, with or without a player:
the lead is per bot and reads no player. No shared code.

**Reader** (`--brain`). `melee exits from the Brain`: per wave, tightest first, each melee decision in the brain room
as seconds of Induce Madness left at the decision (the bot's lead) / on surfacing, `charmed` for one the charm's
teleport brought up; then median, p10, p90 and range of both and of decision to surfacing, `under 1 s`, and `charmed
after deciding` by name and seconds left; waves phase 3 cut short left out (`exit_timing`, `spread_line`, tested).
`caught by it` also prints a decided bot's distance from the nearest clean exit at its decision.

**Baseline at 1.5×.** Window 22: 105 melee exits, decided at 6.6 s left (p10 6.5, p90 6.8), surfaced 3.1 s later (p90
3.3, max 3.4) with 3.6 s to spare (p10 3.4, min 2.9), none charmed. Windows 16-22: 499 exits, one walk over 3.5 s, the
6.0 s of round 11's question: window 16 uld3 (`603_19_yogg-saron_1795443297`) wave 1, Uldzam decided at 4:13.1 with
6.5 s left inside an Army of the Dead (4 s) begun 1.1 s before, walked from 4:16.2 and surfaced with 0.5 s left.
Round 9's cast hold, built from window 17, now holds that cast (4 + 5.3 s over the 7.7 s left). Replayed at 1.2× (lead
× 0.8, the same walks), window 22's melee surface with 2.3 s to spare (p10 2.1, min 1.8).

**Next window should show:** `melee exits` left at the decision a median ~5.3 s (6.6 here), `on surfacing` ~2.2 s
(3.6), p10 ~2.0, `under 1 s` 0 or near it, decision to up unchanged (median 3.1, p90 3.3); `charmed after deciding: 0`,
and no melee with `decided with` in `caught by it`. The gain is 1.3 s more on the Brain for every melee, ~0.8% of the
Brain a cycle (round 11): Brain per cycle above round 12's 15.9% (Stormwind / Icecrown, unheld) and 8.8% (Chamber),
too noisy to read in one window. A melee charmed after deciding, or `under 1 s` more than once or twice a pull, means
back to 1.5×. Round 13's checks (`regroup walks` `sight`, `passed over`, `casts cut for a ray`) land in the same window.

**Still open with the user, not built:** round 13's healer mana question (a healer under 30% mana goes after every
healer above it).

**Review.** Built as the user answered, nothing else in C++. The constant feeds only the exit read and, through
`YoggSaronExitLeadMs`, the exit cast hold; no size, mode or player branch. `--brain` on window 22 reread 38/35/32 melee
exits (105), decided at a median 6.6 s left, surfaced with 3.6 s to spare, 26 yd walked in 3.1 s; window 16 uld3's
wave 1 shows the one 6.5/0.5 walk (6.0 s). Window 22's ranged and healers at the Brain all decided at 2.8-3.1 s left
(the floor, `late`) and surfaced with 2.1-2.8 s to spare, so 1.2× leaves them as they were.

- Review fix: `caught by it` lost the comma after `s left` when a decided bot had no row or the pull no clean exit.
- Left: at 1.2× the lead budgets 5.8 yd/s from the Brain; melee now make ~8.4, but the findings doc's older 4-6 yd/s
  walks would be charmed. The revert rule above covers it.

## Round 15 (2026-11-24, window 24 build baf47824b: rounds 13 and 14)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head baf47824b --pulls 10` (window report
`scratchpad/window24/window24-yogg-saron.md`): one valid pull per pool, 0 humans, Freya's 62670 on the raid, no DC veto
inside a pull. Read with the full `yogg_saron.py` and scratch scripts (`scratchpad/yogg-r15/`: Diminish Power stacks per
bot and wave, channel drops against Judgements and seal hits, area casts beside live Crushers over every pull since
window 12, phase 3 positions, team damage dealt per bot) over `aura`, `cast`, `death`, `act` and snapshot rows. Window 22
(round 12's build) is the baseline.

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261123-222828-13` | `603_13_yogg-saron_1795472953` | wipe 11:54, 32 deaths | 131.6 s, 0 deaths | 439.3 s, 5: C 9.5, S 19.0, I 23.4, C 8.5, S 9.6 (held from 36.1% at 19.6 s left, finished at 8.0 s, phase 3 7.2 s before the end) | from 9:30.8 with 4 tentacles (3.95M; Crushers 9.8% and 100%, the second spawned 31.8 s before; Corruptors 23.6, 100%), 143 s, Yogg 29.6 → 20.8% |
| uld2 | `tr-20261123-222908-14` | `603_14_yogg-saron_1795472994` | wipe 8:56, 29 | 143.4 s, 5 deaths | 392.1 s: S 17.6, I 11.1, C 0.9, S never cleared (7 Insane), wave 5 never went down; Brain at 70.5% | never |
| uld3 | `tr-20261123-223008-15` | `603_15_yogg-saron_1795473055` | wipe 13:05, 32 | 154.5 s, 0 deaths | 511.5 s, 6: C 8.8, S 18.9, I 14.1, C 8.6, S 12.3, I 7.3 (held from 36.2% at 30.3 s left, the Brain still reached 30% 13.8 s before the end) | from 11:05.9 with 9 (9.41M; four Crushers 7-96%, five Corruptors), 119 s, Yogg 29.9 → 26.4% |

**Cycles to 30%:** five, none and six (window 22: five, none, five). Phase 3 opened with 4 / – / 9 tentacles, two and four
of them Crushers; uld1's goal-like start (one Crusher at 9.8%, dead at +10.9 s) still carried a fresh 2.69M Crusher.

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 2 Death Ray 1 (Uldxai 3:09, two death rows 0.5 s apart; every walk refused for the 2 s before); phase 3
  Immortal Guardian 30 (first +38.8 s, the tanks at +83 s and +97 s, rezzed and dead again by +110 s).
- uld2: phase 1 Shadow Nova 5 (1:07 Uldyak, low before it, Reincarnated 0.3 s later and dead again at 1:36; 1:48
  Uldyaw, two novas 3.6 s apart, the second Guardian finished inside the gap by Death and Decay, Deep Wounds, Blood Plague
  and a Corruption; 2:13 Uldyag and Uldyat, a handover leftover). Phase 2 Death Ray 3 (Uldyam 3:20 walking for its
  portal; Uldyae twice inside a Constrictor's grab), self-inflicted 2 (Uldyao's Shadow Word: Death backlash 4:45, Uldyav
  7:44), Laughing Skull 1 (Uldyay, held below), Squeeze 6, Curse of Doom 4, Psychosis 1, Insane 7 (Uldyau from the wave 3
  Chamber, six of the wave 4 Stormwind team at 8:55).
- uld3: phase 2 Crush 2 (Uldzax and Uldzaj 6:27, the Bladestorm below), Laughing Skull 1 (held below), Curse of Doom 1;
  phase 3 Immortal Guardian 24 (first +13.1 s, six ranged dead by +43 s, the tanks at +68 s and +71 s), Corruptor 3,
  Lunatic Gaze 1.

**Verdict on round 13, regroup into the healer's sight: works on its own check, the holds no shorter.** `regroup walks`
`sight` 7 / 6 / 8 bots (range 6 / 2 / 7); `passed over` 8 of 19 / 3 of 29 / 5 of 17 heal casts (window 22: 7 of 13 / 12
of 34 / 7 of 14), 16 of 65 against 26 of 61. `heals in a hold` 21 / 13 / 33 on a held bot against 36 / 59 / 58 on
others, still mostly others; holds 13 / 8 / 21, median 5.0 / 5.5 / 4.6 s (4.6 / 4.5 / 5.4). Chamber clears median 30.2 s
(28.2) and Chamber cycles 8.6% (8.8%), not readable: those teams carried Diminish Power (below).

**Verdict on round 13, casts cut under a Death Ray: fixed.** `casts cut for a ray` 14 / 13 / 15 (Lightning Bolt, Revive
Pet twice, Incinerate, Chain Heal ...); no Death Ray death on a casting bot (window 22: 2): Uldxai's and Uldyam's were
walking, Uldyae's two in a grab, which the cut leaves alone.

**Verdict on round 14, exit lead 1.2×: keep it.** 74 melee exits (30 / 8 / 36): decided a median 5.3 s before the end
(p10 5.1-5.2; window 22 6.6), surfaced with 2.2 / 2.3 / 2.1 s to spare (p10 2.0 / 2.1 / 1.9, min 1.9 / 2.1 / 1.8; window
22 3.6, min 2.9), 0 under 1 s, 0 charmed after deciding; decision to surfacing median 3.0 / 3.0 / 3.1 s (p90 3.2 / 3.1 /
3.3), unchanged, so the replay held (2.3 s, p10 2.1, min 1.8). No `caught by it` bot had decided: uld2's seven never
did, in a room that never opened. Brain per unheld Stormwind / Icecrown cycle median 17.6% (window 22 15.9%), Chamber
8.6% (8.8%): no worse, too noisy for the ~0.8% a cycle the change buys. The revert rule (a charmed melee, or `under 1 s`
more than once or twice a pull) did not fire.

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5) and is not picked; its loss is read below. Rounds 8-14 picked
phase 2 failures, none twice.

**Ranking** across pools: phase 3 Immortal Guardians 54 (stuck); uld2's phase 2 collapse 21 from 6:25 (Squeeze 6, Insane
7, Curse of Doom 4, grabbed Death Rays 2, Psychosis 1, self 1); Shadow Nova 5; phase 3 Corruptor and gaze 4; Crush 2;
Death Ray 2 outside a grab; Laughing Skull 2; self-inflicted 1; phase 2 Curse of Doom 1.

**Why uld2 wiped in phase 2: Diminish Power taken below.** Phase 1 cost three melee and a ranged, so the platform killed
Crushers slowly (one lived 3:28-8:55) and four were channelling by 7:13. Each Crusher's 64145 is −20% damage, multiplied
across Crushers, and a bot carries it below until that channel breaks (the re-cast skips Illusion Room 63988 carriers).
New `--brain` line `Diminish Power carried below`, damage dealers' median multiplier until the door: wave 3 (Chamber)
went down under two stacks, ×0.64, cleared in 39.7 s and took 0.9% of the Brain; wave 4 (Stormwind, six bots) under
four, ×0.41: its four damage dealers made 1.1-2.7k/s each over the room (snapshot `dealt`) and the room never opened, Induce Madness
took the team, and the platform, alone under five Crushers, died to Squeeze and Curse of Doom. Window 22's uld2, which
round 13 read as a dry healer and out-of-sight holds, went the same way: waves 6 and 7 at ×0.46 and ×0.41. Across window
24, 14 of 15 waves carried some (×0.41-1.00, ×0.80 typical).

Only three things end the channel. The stun: at each room clear every Crusher's 64145 drops from everybody, the team below
included (uld2 6:44.58, three channels, 36 rows), so a team pays until its door, not at the Brain. A Crusher's death. And a
melee-class hit, which the strategy keeps off live Crushers by design (melee and tanks off them, the pet guard), with
`yogg-saron diminish power judgement` as the stated replacement. **It no longer breaks anything**: core `fe0e00d8e`
(upstream #28297, 2026-11-04, on Custom since 2026-11-09) makes `spell_yogg_saron_diminish_power_aura` return for a
paladin Judgement damage spell (`SpellFamilyFlags[0] & 0x800000`). Over window 24, 33 plain Judgements (54158) mid-channel
dropped none; a prot paladin's Judgement of Vengeance dropped it 2 of 2 times its Seal of Vengeance hit (42463, from
Judgements of the Just) landed with it, a ret's 0 of 2. The channel was up 52 / 78 / 60% of Crusher-alive time. A plan
question, not a bug: the only breaker left is a melee hit on a live Crusher (question below).

**Why both phase 3s were lost to the Guardians: what phase 2 handed over, and Diminish Power.** The tried fixes still do
their part (class taunts held 24 / 7; tank taunts by aim mostly `loose` and `another tank`; ranged DoTs on Yogg 66-100%
in uld1). uld1: the ranged put 83% of 55.8k/s into the four leftovers and 90-95% of their targets for 90 s, the fresh
Crusher dying at +95.3 s; every one of 11 Guardians left stasis on a non-tank (+4.0 s, 8 onto melee), 44% of Guardian
samples sat at 90%+, up to five over 30% stood on one tank from +70 s, and the 11:11 beacon healed six more; Yogg lost
8.8%, melee 72% on him and ranged 1%. uld3: the brain team surfaced 56 yd west, the station released the ranged onto the nine
leftovers from there, 43-80 yd west of Yogg inside the Guardian spawn ring (38-48 yd), so two of nine Guardians spawned
on them and took a ranged at +0.0-0.2 s; ranged 92-100% on tentacles all phase, never at the station, Diminish Power on
the average raider 40% of phase 3, 84% of Guardian samples at 90%+, Yogg 3.5%. Same mechanism as rounds 3-7 with the
user's goal (at most one Crusher) not met; Diminish Power, unbroken, slows the cycles that decide it.

**Picked second, independent and small, a bug: the Crusher area hold never held Bladestorm.** uld3 6:21.2: Uldzax's
Bladestorm 7.7 yd from a channelling Crusher (inside the hold's 9.33), its whirls broke the channel and took the threat,
and the Crush at 6:27.2 killed him and Uldzaj beside him. `YoggSaronCrusherAreaHoldMultiplier` exempted a positive
spell, meant for Heroism and shouts, and Bladestorm 46924 (a self-aura whose periodic Whirlwind hits enemies), Sweeping
Strikes and Blade Flurry read positive. Since window 12 the hold vetoed Bladestorm 12 times and Sweeping Strikes 14, all
for prot warrior tanks, whose missing spell skips the positivity test, and never for the arms and fury warriors and
combat rogues who cast them 150 / 80 / 71 times. Fix (`UldMultipliers_YoggSaron.cpp`): a positive spell is exempt only
when it is no named multi-target action (`facts.multiTarget`) and no periodic trigger of it hits enemies. Same at 10 and
25, normal and hard mode, with or without a player. No shared code.

**Reader.** `--brain` per wave: `Diminish Power carried below`, each team bot's stacks at its first sample below and the
damage dealers' median multiplier until the door (`source_windows`, `stacks_at`, `diminished_damage`, tested).

**Next window should show:** `--crush` `area buffs held by role` with `bladestorm`, `sweeping strikes` or `blade
flurry` on `melee` rows, not only `tank`; no Crush death after a Bladestorm in a Crusher's reach (here 2). `Diminish
Power carried below` as the baseline for the question: here ×0.41-1.00 until the door, 14 of 15 waves. Rounds 13 and 14
checks as above.

**Question for the user (who breaks Diminish Power).** Judgement stopped breaking it with core `fe0e00d8e`, and the
strategy keeps melee, tanks and pets off live Crushers, so outside the stun nothing but a prot paladin's seal hit does: the
brain team takes the stacks below, and both phase 2 collapses (window 22 uld2, window 24 uld2) were rooms fought at
×0.46-0.41. Options: (a)
the off-tank holds one live Crusher in phase 2, so it swings at him and never channels (Crushes on him, others stay off
it); (b) once the brain team has clicked, a bot tank hits each channelling Crusher once and steps out of its reach, so
the re-cast skips the team below (one or two Crushes on the tank per Crusher, the platform debuffed as before); (c)
leave it. **Recommended: (b) for one window**: it ends the collapse mechanism at the cost of a few Crushes on a tank;
(a) does more (platform and phase 3 damage too) but puts a tank under a Crusher for whole waves.

**Round 14's queued question (a healer under 30% mana goes after every healer above it).** No case in window 24: the
lowest healer at a team pick was 41% (uld3 wave 3), the room healers left at 25-38% and were back at 41-94% by the next
pick. Window 22's two cases came in waves already at ×0.46 / ×0.41, so the dry healer was not what stalled them. It no
longer stands as asked; recommended: drop it unless a later window shows a dry healer in a clean room.

**Seen, not picked.**

- uld3 wave 6's brain hold latched at 36.2% with 30.3 s left and nine tentacles up, and the Brain still reached 30% 13.8 s
  before the end with no finish note (DoTs started before 36% and pets keep going).
- uld2 phase 1: a nova-gap double (1:44.8 / 1:48.5) finished by Death and Decay, Deep Wounds, Blood Plague and a
  Corruption on the parked Guardian, and the tank's own Heroic Strike and Cleave on another (0:52.5 / 0:55.1).
- The judgement node still costs a holy paladin a GCD on a channelling Crusher for nothing; left until the question is
  answered, since a prot paladin's still breaks it.

**Review.** The fix holds. Bladestorm 46924's one periodic trigger, Whirlwind 50622, targets an enemy area, which
`SpellInfo::_IsPositiveEffect` never counts against the caster's own aura, so Bladestorm read positive, as Sweeping
Strikes and Blade Flurry do; uld3 reread: Bladestorm at 6:21.174, 64145 off the whole raid at 6:21.181, and again at
6:23.17 and 6:25.18, each re-cast broken by the next whirl. `facts.multiTarget` alone now holds all three; the periodic test
holds an unnamed positive aura whose trigger hits enemies. Window 24's Bladestorm and Sweeping Strikes vetoes were all
uld2's warrior tank. No size, mode or player branch; other phases and bosses untouched.

- Review fix: Diminish Power is −20% a copy, not −21% (64145 effect 2, aura 79, base points −21 + 1 from DieSides 1).
  The reader's `DIMINISH_POWER_DAMAGE` is now 0.80, and this round's multipliers above, reread, and the findings doc's
  stacked figures follow; the reread also gives 14 of 15 waves carrying some, not 13.
- Review fix: `--crush` printed only the 8 commonest held actions, which cut uld2's `bladestorm` and `sweeping
  strikes` (13 kinds); it now prints all, and `area buffs held by role` splits the three buffs by role for the check
  above.
- Review fix: the two new reader helpers carried docstrings; removed. The findings doc and the question said nothing
  breaks the channel outside the stun, right after the prot paladin's seal hit that does.
- Left: the hold gates the cast only, so a Sweeping Strikes or Blade Flurry cast outside the reach still splashes a
  Crusher once the bot walks in beside one.

**User answer (2026-11-24) to round 15's question (who breaks Diminish Power):** yes, (b), for one window: after the
brain team clicks its portals, a bot tank hits each channelling Crusher Tentacle once and steps out of its reach, so
the channel restarts without the brain team in it (round 16 builds it). The healer-mana question stays withdrawn:
not built.

## Round 16 (2026-11-24, no pulls on round 15: merged, not built)

**Data.** No Yogg-Saron trace since window 24 (`--head baf47824b`). Stop rules, verdict and ranking skipped: round 15
has no pulls yet. Window 24's three pulls (`603_13_yogg-saron_1795472953`, `603_14_…_1795472994`,
`603_15_…_1795473055`) reread with the new `--brain` line below and `scratchpad/yogg-r16/entries.py` (each team's
Illusion Room entries, the Crushers whose stack each bot carried in, the tanks' distance to each Crusher at the
team's last entry) are the baseline.

**Built: the user's answer** (`UldEncounter_YoggSaron.h/.cpp`, `UldTriggers_YoggSaron.h/.cpp`,
`UldActions_YoggSaron.h/.cpp`, `UldDefinition_YoggSaron.cpp`). `yogg-saron diminish power break` at `ACTION_RAID +
1.75` (over the resolver and Brain Link, under the pet guard and phase 2 spacing; review moved it from 2.5), all
through `YoggSaronPowerBreakPlan`:

- **When:** phase 2, the team down (no portal still open, or every live, uncharmed member of `brainTeam` under
  Illusion Room 63988, the aura the re-cast skips), and a Crusher channelling 64145 (`SPELL_STATE_CASTING`) whose
  stack a group member under Illusion Room carries (aura caster = that Crusher). A late entrant who caught the
  re-cast makes it fire again for that Crusher.
- **Who:** per Crusher, the nearest bot tank (headless) with no claim, on the platform, alive, not in a seat, not
  charmed, stunned, feared or confused; ties by GUID. Claims sit in `state.powerBreaks` by tank; a claim whose
  owner stops qualifying stops counting, so the other tank takes the Crusher.
- **Walk:** to `GetMeleeRange − 2.5` (8.3 yd) from the Crusher on whichever of 16 bearings has the fewest raiders
  in the Crush cone (±14°, 25 yd), then the shortest walk; spot and exit both 15-55 yd from the middle; routed by
  `YoggSaronBodyRoute`, released by `YoggSaronWalkMakingProgress` (`powerbreak`).
- **Hit:** in melee range, `Attack` plus the first castable of Shield Slam / Revenge / Devastate, Shield of
  Righteousness, Plague / Death / Blood Strike, Mangle (Bear) / Lacerate, all melee-class (64148's 0x28). A strike
  alone does not make the tank the victim (only a white swing does, `DamageTaken`), so a strike that lands first can
  cost no Crush at all.
- **Out:** once no one below carries that Crusher's stack, `AttackStop`, drop the target, walk to 14.5 yd from it on
  the tank's own bearing (turned up to 90° to stay 15-55 yd out), at most 5 s.
- The claim drops on the Crusher's death, Shattered Illusion on it, or phase 2's end. Probe `yogg.powerbreak`
  (`none` / `walk` / `hit` / `out`, then the Crusher's key). `GetYoggSaronChannellingCrushers` now shares
  `YoggSaronPowerChannelling`.

Same at 10 and 25, normal and hard mode. A real player changes only who goes below and who carries a stack, read
off auras on any group member; a human tank never claims, so with human tanks only nobody breaks. No shared code;
no code comments.

**Reader.** `--brain` `Diminish Power carried below` now gives stacks `entering` (first sample below) and `leaving`
(last), and `carried a median / max` s until the bot's last stack dropped (`carried_ms`, tested). `--crush`
`Diminish Power breaks` (`power_break_episodes`, tested; `show_power_breaks`): claims by tank, how many dropped the
channel off the team and a median how long after the claim, Crushes that Crusher landed on the claiming tank per
claim (to 3 s after) with median, max and total, and how often its re-cast caught someone below again (to 10 s
after); then one line per claim.

**Baseline (window 24, no break).** `carried a median` per wave: uld1 13.5 / 6.5 / – / 15.0 / 25.5 s; uld2 17.5 / 30.2
/ 39.5 / 58.5 (wave 4 never opened its door, leaving at 4 stacks); uld3 24.5 / 23.0 / 2.5 / 30.8 / 31.5 / 19.5;
`leaving` 0 everywhere else, the stun having dropped them; damage dealers ×0.80-1.00 until the door under one stack,
×0.64 under two, ×0.41 under four. At the team's last entry the nearest tank stood a median 45 yd from each Crusher
carried down (5-89 yd, 23 Crushers), about 6 s at run speed; both tanks usually stood together.

**Next window should show:** `--crush` `Diminish Power breaks` with a claim in most waves that carried a stack down,
most of them dropping the channel off the team within ~10 s of the claim; Crushes on the claiming tank a median 1-2
per Crusher and no tank death to Crush; `caught someone below` 0 or near it. `--brain` `carried a median` well under
the 20-30 s above (the team's entry spread plus a walk and a swing, ~10-15 s) and damage dealers ×0.90 or better until
the door; room clears and Brain per cycle are too noisy to read in one window. Back it out if a claiming tank dies to
Crush, a Crush cone kills a bot after a break, or claims never drop the channel. Round 15's checks (`area buffs held
by role` with melee rows, no Crush death after a Bladestorm) land in the same window.

**Left:** the judgement node still costs a holy paladin a GCD on a channelling Crusher for nothing (round 15); a prot
paladin's seal hit still breaks the channel, harmlessly now.

**Review.** The root cause holds: window 24 reread with the new `--brain` line gives the baseline above wave for wave,
every carried stack dropping only at the room clear (`leaving` 0) or never (uld2 wave 4). The fix builds the user's
(b) as asked, with no size, mode or player branch: a human tank only never claims, and anyone below counts, bot or
not. Other phases and bosses untouched; `GetYoggSaronChannellingCrushers` reads the same.

- Review fix: the node sat at `ACTION_RAID + 2.5`, over `yogg-saron pet guard` (+2), which returns false so the next
  action still runs. A claiming tank's tick never reached it, so a DK tank's Risen Ghoul or Army ghouls would follow
  it onto the Crusher unhushed, take the victim with a white swing and aim the Crushes at whoever stands behind them.
  Now `ACTION_RAID + 1.75`: the guard runs first every tick, the break still wins over Brain Link and the resolver.
  Window 24's blood DK tank (uld1) raised his ghoul only in phase 3, so no pull showed it.
- Review fix: the claim scan tested who carries a stack below only after the portal grid search, which ran every
  tick per tank whenever a Crusher channelled (52-78% of phase 2); the carried test now filters first.
- Left: a claim has no timeout, so a claimant whose walk keeps stalling holds the Crusher from the other tank until
  `YoggSaronWalkMakingProgress` retries (6 s give up, 4 s retry).

## Round 17 (2026-11-24, window 25 build 3ce456519: rounds 15 and 16)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 3ce456519 --pulls 10` (window report
`scratchpad/window25/window25-yogg-saron.md`): one valid pull per pool, 0 humans, Freya's 62670 on the raid, no DC veto
inside a pull. Read with the full `yogg_saron.py` and scratch scripts (`scratchpad/yogg-r17/`: per Guardian its victims,
taunts and hits from spawn, ranged and tank targets every 5-10 s with the tentacles' lanes, burst casts and vetoes in
phase 3, tank intake against heals, the Crush deaths' last 10 s) over `snap`, `cast`, `act`, `veto`, `dmg` and `note`
rows. Window 24 is the baseline.

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261124-103120-12` | `603_12_yogg-saron_1795516326` | wipe 11:57, 33 deaths | 139.2 s | 435.0 s, 5: C 13.0, S 18.6, I 19.2, C 10.8, S 8.5 (held from 36.0% at 18.6 s left, phase 3 6.0 s before the end) | from 9:34.2 with 5 tentacles (5.86M; Crushers 59.5, 95.9%; Corruptors 30.3, 88.6, 89.7%), 143 s, Yogg 29.7 → 21.7% |
| uld2 | `tr-20261124-102839-10` | `603_10_yogg-saron_1795516165` | wipe 11:32, 31 | 143.6 s | 440.1 s, 5: I 21.0, C 6.2, S 20.3, I 21.4, C 1.2 (held from 31.2% at 14.9 s left, phase 3 3.7 s before the end) | from 9:43.7 with 9 (8.95M; Crushers 31.2, 88.9, 90.7%; six Corruptors), 109 s, 30 → 28.4% |
| uld3 | `tr-20261124-102939-11` | `603_11_yogg-saron_1795516225` | wipe 10:54, 32 | 134.1 s | 355.5 s, 4: S 17.2, I 24.5, C 12.0, S 16.2 (held from 36.0% at 12.6 s left, phase 3 5.3 s before the end) | from 8:09.6 with 3 (1.47M; one Crusher at 13.6%, dead at +6.9 s; Corruptors 37.1, 100%), 165 s, 30 → 18.7% |

**Cycles to 30%:** five, five and four, all three reaching phase 3 (window 24: five, none, six). Phase 3 opened with
two, three and one Crusher: the user's goal (at most one) met in uld3 only.

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 1 (Uldxai, the enhancement shaman, 2:09.3, nothing up); phase 2 Crush 2 (5:42.6, Uldxal and
  the hunter Uldxau, a cone aimed at Uldxau's pet, below), Death Ray 1 (Uldxab, healer, 6:00.2); phase 3 Immortal
  Guardian 28 (first +18.3 s, the tanks at +93 / +95 s), Lunatic Gaze 1.
- uld2: phase 1 Shadow Nova 4 (1:01.8 Uldyak; 1:08.1 Uldyay and Uldyar in one nova; 2:13.4 Uldyak again); phase 2
  Laughing Skull 1, Crush 1 (Uldyam 8:51.4, below); phase 3 Lunatic Gaze 1, Curse of Doom 3 (leftover Corruptors,
  10:14-10:31), Immortal Guardian 21 (the tanks at 10:37.5, 10:42.9 rezzed, and 10:44.7, around the 10:38.8 beacon
  heal).
- uld3: phase 1 Shadow Nova 1, Sara's Blessing 1; phase 2 Death Ray 1 (Uldzae, healer, 6:19.1, no dodge); phase 3
  Immortal Guardian 29 (first +7.1 s, the tanks at +105 / +110 s after the 9:52.4 beacon heal).

**Verdict on round 15 (Bladestorm, Sweeping Strikes, Blade Flurry held as area damage): not exercised.** Melee cast the
three 33 times; one within a Crusher's hold reach, uld2 8:20.0 Uldyax's Bladestorm 2.8 yd from a stunned one with 7.3 s
of stun left, which the hold rightly passes. `area buffs held by role`: uld2's prot warrior only. No Crush death after a
Bladestorm (window 24: 2); this window's three Crush deaths are other mechanisms (below). Kept.

**Verdict on round 16 (a bot tank breaks Diminish Power once the team is below): keep it.** Claims 3 / 5 / 1, Crushes on
a claiming tank 0 in all nine, no tank death to Crush, no Crush death within 10 s of a break, `caught someone below` 0.
The channel left the team a median 5.8 / 8.4 / 2.5 s after the claim, but the tank's own strike broke it in three claims
(uld1 3:36.4, uld2 ~9:19.9, uld3 3:24.05 Shield of Righteousness, which `--crush` lists as `walk/out`, no `hit` row);
most others ended at the room-clear stun. Waves carrying a stack below 7 of 14 (window 24: 14 of 15, chance: Crushers
channelling at the opens), carried a median 13.8 s over those (window 24: 23.8), damage dealers ×0.80-0.98 to the door
(×0.41-1.00). The long carries are late claims: the break waits for the whole team, so uld2 wave 5's claim came
at +19.0 s with its last entrant (+18.9 s) and uld1 wave 5's at +25.2 s, Uldxay never getting a portal; uld2's Uldyaa
sat in a Constrictor until 9:12.0.

**Stop rules.** No kill. Phase 3 was picked in rounds 3-5 and stays stuck as the throughput race; this round's pick is a
new root cause inside it (as round 7's nova was), not a retry of those.

**Ranking** across pools: phase 3 Immortal Guardians 78 (non-tanks dead before the first tank death 3 / 1 / 7: nine to
fresh Guardians, 3 / 0 / 6, and two beacon draggers); Shadow Nova 6; Crush 3; Curse of Doom 3 (phase 3 leftovers); Death Ray 2; Lunatic Gaze 2;
Laughing Skull 1; Sara's Blessing 1.

**Why the three phase 3s were lost.** The same race as rounds 3-7: nothing kills a Guardian without Thorim, one spawns
every 10 s, and the tanks fall once the pile is 6-8 at high health or a beacon heal lands on it. What decided each:

- uld1: ranged spent 95% of 57.9k/s on the five leftovers all phase: the 59.5% Crusher (tier 0, dead +33.5 s, 66 yd from
  the ranged spot), then Corruptor 284, 3 yd from the north-east drag spot and so tier 0 as a beacon-heal reach, at 9.2%
  when the 10:31 mark dragged to that lane healed it back to 40% (dead +85.7 s), then the 95.9% Crusher. Guardians
  stayed at 90%+ 44% of samples, seven on the tanks at 43-89% by 11:04; Yogg 0.08%/s on melee.
- uld2: nine leftovers, three Crushers channelling (Diminish Power on the average raider 59% of phase 3): ranged 96% on
  tentacles at 1-7k/s each, Guardians at 90%+ 90% of samples, Curse of Doom took three ranged, the bear Uldyab kept a
  leftover Corruptor as its target for the first ~20 s, and the 10:38.8 beacon's three marks ended on Uldyab undragged.
- uld3, the goal met: ranged on Guardians over 30% 86% from +10 s, four reached Weakened, Yogg 0.09%/s, the window's
  best. Lost to six non-tanks killed by fresh Guardians (below) and the 9:52.4 beacon: its healed Guardians ended at
  99-100% and both tanks died within 7 s.

**Why tentacles were still up at phase 3.** The platform dealt 41-43k/s live and 51-75k/s stunned against a stock that
grows every cycle (`AddPortals` speeds spawns 0.1 a wave): the last cycle's stock went 7.28 → 5.86M (uld1), 9.59 → 8.95M
(uld2: 30.3k/s, both tanks grabbed in turn, three Crushers channelling) and 3.84 → 1.47M (uld3). The brain hold finishes
at 8 s left whatever the platform holds, so phase 3 opens mid-stun with what is up: all five of uld1's leftovers and
seven of uld2's nine spawned in the last two cycles, and Corruptors, ranked last on the platform, are what remains
(uld1's three spawned 7:41.7, 8:34.2, 8:58.4).

**Picked: a taunt from the pile runs out short of the tank.** At stasis end (+4.0 s) the holding tank taunts the fresh
Guardian from 43-62 yd (mostly Righteous Defense, which reaches it). Guardians walk ~11 yd/s, so at the 3 s fade they
are still 15-27 yd off; the taunt only matched the top threat, ~0 on a fresh Guardian, so
`ThreatManager::ReselectVictim` hands it to the first raider in its melee range above 110% of that, and its first swing
is ready: uld1 9:52.55 Uldxaf (41.9k, Guardian spawned 9:45.47, RD +4.1 s at 54 yd, hit +7.08 s), 10:13.85 Uldxan (RD
+4.2 s at 57 yd, hit +7.15 s), uld3 8:16.68 Uldzay (RD +4.0 s at 54 yd, hit +6.99 s), all dead on the hit. The other
tank taunts it 0.1-0.3 s after the fade (uld3 Growl at +7.2 / +7.3 s on the next two spawns), one swing late. `--phase3`
(new line): taunts run out over 10 yd short 4 / 1 / 5, chained by the other tank before the fade 1 / 0 / 2 (by chance:
uld1 10:47 Dark Command during the RD), a non-tank swung at within 2 s of the fade 2 / 0 / 1, all died. Root cause:
`ControlPhaseThree` skips any Guardian whose victim is a tank, and any carrying a tank's taunt
(`UldActions_YoggSaron.cpp:1905-1941` before the fix), so nobody may re-taunt until the fade has already handed it on.

Bug: the strategy means the stasis-end taunt to bring the Guardian to the tank. Fix (`UldEncounter_YoggSaron.h/.cpp`,
`UldTriggers_YoggSaron.cpp`, `UldActions_YoggSaron.cpp`): `YoggSaronTauntFadesShort` is true for a holdable Guardian
whose victim is another tank, held by that tank's own taunt with `…_TAUNT_CHAIN_LEAD_MS` (1000) or less left, still
outside its melee range, the bot no farther from it than the holder + 5 yd. Such a Guardian fires guardian control and
joins the loose loop as a chain: taunt only (Righteous Defense on the holder would take its pile), past the taunt skip;
probe `yogg.tankhold` `chain taunt`. The latest taunt wins (`ThreatManager::TauntUpdate`), so it walks on to the second
tank, both at the melee spot. The budget is the same taunt the other tank spends today 0.1-0.3 s after the fade. Same at
10 and 25, normal and hard mode; a human holder is chained the same way, only bot tanks chain.

**Picked second, independent and small: no ranged burst cooldown in any phase 3.** `UlduarBurstWindowMultiplier`'s Yogg
branch opens every burst cooldown in phase 3, but the shared `hold burst until tank engaged` zeroes one whose target is
not boss-flagged (`BurstOnBossOnly`), and phase 3 ranged are on Guardians and tentacles all phase: no ranged cast in
any of the three but Shadowfiend, the gate's veto rows on ranged 327 / 154 / 251 (Icy Veins, Mirror Image, Arcane Power,
Combustion, Metamorphosis, Elemental Mastery, Fire Elemental Totem, Force of Nature, Rapid Fire, Bestial Wrath,
Readiness, trinkets, tinkers), not even under the lust node's Heroism. Fix, **shared code** (`BurstCooldowns.h/.cpp`,
`BurstWindowStrategy.cpp`), the entry-keyed release `docs/systems/consumables-and-burst.md` prescribes:
`BurstAllowedOnAdd` lists 33988, 36064 (Immortal Guardians) and 33966, 33985, 33983 (platform tentacles), and the base
gate passes a DPS bot's burst on one. Those entries exist only in this fight, and the Ulduar window still holds phase 2
to the Brain, so no other boss and no other phase changes. Tanks and healers keep the gate (a DK tank's Army of the Dead
is a 4 s channel). Reader: `--phase3` `burst cooldowns` (casts and the gate's veto rows by role).

**Next window should show:** `--phase3` `taunts run out short of the tank` with nearly all chained and `a non-tank swung
at within 2 s of the fade` 0 (here 2 / 0 / 1, all dead); `yogg.tankhold` `chain taunt` rows; non-tanks killed by fresh
Guardians before the first tank death under 3 / 0 / 6 (the swap-starved ones, question below, stay). `burst cooldowns`
with ranged casts in phase 3 and the gate's ranged veto rows near 0 (here 327 / 154 / 251). Round 16 again with 0
Crushes on a claiming tank. Yogg's phase 3 rate is too noisy for one window.

**Question for the user (the swap spends the taunts fresh Guardians need).** uld3 swapped the pile six times in 64 s
(8:46.7-9:50.7, each a Righteous Defense or taunt at under 50%), and the spawns at 9:15.7 and 9:25.7 went 16-17 s
without a taunt and killed four (Uldzad, Uldzak, Uldzax, Uldzaf); uld1's 10:54.2 spawn, untaunted until +10.9 s, killed
its healer Uldxad at +7.1 s between swaps at 10:49.4 and 10:56.7. Should a fresh Guardian on a non-tank be taunted before
the swap while the holder is still over 35%? **Recommended: yes, for one window**: a full-power Guardian one-shots a
healer or a ranged, while a holder at 35-50% under a pile usually lives the 8 s until the swap's next chance; tank
deaths at 35-50% are the risk to watch.

**Seen, not picked.**

- uld1 5:42.6, two Crush deaths: wave 2's stun was ending, the pet guard pulled Uldxau's Serpent off the Crusher at
  5:37.05 and `YoggSaronPetFallbackTarget` sent it to the nearest add, a Corruptor 5.7 yd from that Crusher, still
  inside its reach, so the Crusher kept the pet as victim and coned Uldxal and the hunter himself 24 yd behind it when
  the stun lifted. The fallback takes the nearest add without testing a swinging Crusher's reach (the phase 3 leftover
  pick skips any within 25 yd). Small, Yogg-only: next candidate.
- uld2 8:51.4: Uldyam's Death and Decay went down at 8:40.06 and a Crusher spawned inside it at 8:40.25; its ticks made
  him the victim and he died in place (window 21's Uldyam death has the same shape). The hold gates casts only.
- Uldxai, the enhancement shaman, died to a phase 1 nova again with nothing up, as in windows 16-22.
- Brain team portal entries over 2 s after the open 19 / 16 / 11 a pull, and uld1 wave 5's Uldxay never got a portal.

**Review.** A tank claimed for a beacon drag no longer chains (`YoggSaronTauntFadesShort`): when its drag taunt fails,
guardian control runs, and a chain would spend the taunt the drag needs. A chain lands only inside taunt range (30 yd):
of the 10 short taunts here the other tank was within 30 yd of the Guardian (centre to centre) in 5 at 1 s before the
fade, 8 at 0.5 s and 9 at 0.25 s, so expect most chained, not all.

**User answer (2026-11-24) to round 17's question (the swap spends the taunts fresh Guardians need):** yes, for one
window: a bot tank taunts a fresh Immortal Guardian that is hitting a non-tank before doing a tank swap, as long as the
holding tank is above 35% health (round 18 builds it).

## Round 18 (2026-11-24, window 26 build e834a2708: round 17)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head e834a2708 --pulls 10` (window report
`scratchpad/window26/window26-yogg-saron.md`): one valid pull per pool, 0 humans, Freya's 62670 on the raid, no DC veto
inside a pull. Read with the full `yogg_saron.py` and scratch scripts (`scratchpad/yogg-r18/`: each phase 3 death with
its killer's age, time loose and distance to each tank and their taunt cooldowns, the tanks' notes and casts, who woke
each Guardian, where the melee and ranged stood) over `snap`, `cast`, `dmg`, `note`, `move` and `death` rows. Window 25
(round 16's build) is the baseline, reread with the new `--phase3` lines.

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261124-121450-8` | `603_8_yogg-saron_1795522535` | wipe 11:55, 33 deaths | 135.2 s | 436.4 s, 5: S 19.5, C 8.0, I 18.8, S 19.0, C 4.7 | from 9:31.7 with 4 tentacles (Crushers 47.0, 85.0%; Corruptors 46.2, 75.1%), 144 s, Yogg 29.8 → 22.5% |
| uld2 | `tr-20261124-121349-7` | `603_7_yogg-saron_1795522475` | wipe 11:40, 31 | 139.5 s | 433.3 s, 5: S 15.5, I 22.3, C 4.5, S 16.6, I 11.1 | from 9:32.8 with 5 (Crushers 22.3, 85.8%; Corruptors 10.0, 95.5, 100%), 127 s, 30 → 26.1% |
| uld3 | `tr-20261124-121550-9` | `603_9_yogg-saron_1795522596` | wipe 10:57, 32 | 142.9 s | 378.7 s, 4: S 21.4, I 18.5, C 10.9, S 19.3 | from 8:41.6 with 5 (Crushers 16.4, 99.5%; Corruptors 61.4, 83.6, 100%), 136 s, 30 → 21.8% |

**Cycles to 30%:** five, five and four, all three in phase 3 again (window 25: five, five, four). Phase 3 opened with
two Crushers in every pull: the user's goal (at most one) not met.

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 2 (Uldxai, the enhancement shaman, 1:24.4 and 1:48.4); phase 2 Death Ray 2 (Uldxag 3:16.5,
  Uldxai 4:33.3, both walking for a portal), Laughing Skull 1 (Uldxai 8:52.5); phase 3 Immortal Guardian 28 (first +57.5
  s, the tanks at +72.0 s, 16 s after the 10:27.7 beacon heal, and +95.3 s).
- uld2: phase 1 Shadow Nova 1 (Uldyan 1:45.4); phase 2 none; phase 3 Immortal Guardian 29 (first +7.3 s, the first tank
  at +42.0 s), Lunatic Gaze 1 (Uldyas 10:52.4).
- uld3: phase 1 Sara's Blessing 2 (Uldzax 1:29.6, Uldzat 1:58.5), Shadow Nova 1 (Uldzai 2:14.3); phase 2 none; phase 3
  Immortal Guardian 29 (first +52.7 s, a beacon dragger; the tanks at +68.8 / +73.5 s, 11.4 / 16.2 s after the 9:38.95
  beacon heal).

**Verdict on round 17, chain taunt: fixed on its own check.** `--phase3` `taunts run out short of the tank` 1 / 0 / 3
(window 25: 4 / 1 / 5), chained by the other tank before the fade 1 / 0 / 1 (1 / 0 / 2), a non-tank swung at within 2 s
of the fade 0 / 0 / 0 (2 / 0 / 1, all died); `yogg.tankhold` `chain taunt` 0 / 2 / 3 rows.

**Verdict on round 17, burst on phase 3 adds (shared `BurstCooldowns`): fixed on its own check.** `burst cooldowns`:
ranged casts 12 / 13 / 7 (window 25: none but Shadowfiend), the gate's ranged veto rows 0 / 0 / 0 (327 / 154 / 251);
healer and tank rows stay, as built. Not readable on Yogg: ranged put 92 / 96 / 96% of 54.4 / 35.0 / 45.7k/s into the
leftovers all phase.

**Stop rules.** No kill. Phase 3 was picked in rounds 3-5 and stays stuck as the throughput race; this round builds the
user's answer to round 17's question, and the second pick is a new root cause inside it (as rounds 7 and 17 were), not a
retry.

**Ranking** across pools: phase 3 Immortal Guardians 86 (raiders killed by a fresh Guardian before the first tank death 2
/ 5 / 0, every one out of the tanks' reach; then the pile and the beacon heals); Shadow Nova 4; Death Ray 2; Sara's
Blessing 2; Lunatic Gaze 2. Phase 2 cost 3 / 0 / 0, the lowest yet.

**Baseline for the user's answer** (new `--phase3` lines, up to the first tank death; fresh means no tank has held it
yet). `fresh on a raider`: 5 of 7 Guardians, median 4.7 s, max 12.7 s, 30.3 s in all / 4 of 4, 4.8, 7.7, 18.3 s / 1 of 7,
0.3 s (window 25: 7 of 8, 1.5, 6.3, 14.8 s / 2 of 5, 1.4, 2.0, 2.8 s / 7 of 10, 1.7, 10.1, 26.3 s); `raiders killed by a
fresh Guardian` 2 / 5 / 0 (window 25: 1 / 1 / 4, uld1's Uldxad and uld3's four being round 17's swap-starved cases). In
this window no fresh Guardian met a swap: uld1 swapped three times from 10:43.2, after its last fresh kill, uld2 twice
and uld3 once, after their first tank death. Every one of the seven fresh kills was out of reach: the killer 37-62 yd
from both tanks, whose taunts reach 30.

**Built: the user's answer** (`UldEncounter_YoggSaron.h/.cpp`, `UldActions_YoggSaron.cpp`). `YoggSaronTankSwapTake`
splits into `YoggSaronTankSwapHolder` (whom this bot would take from, a live swap's or a new one's, starting nothing) and
`YoggSaronTankSwapTake(botAI, holder)` (starts it). In guardian control, while the holder is over
`ULDUAR_YOGG_SARON_TANK_SWAP_DEFER_PCT` (35), the taker first runs the loose loop over Guardians whose victim is a
non-tank player at tier 1 (never a drag return or a hunter's Distracting Shot hold): taunt, else Righteous Defense on
the victim; a cast notes `yogg.swapdefer` (`<holder> <health %>`) and ends the tick. Otherwise it takes as before. The
code reads the victim, not the age: before a tank death, a Guardian on a raider is a fresh one or one that slipped,
and both one-shot a healer. The swap now starts on its first take that lands, not when it falls due, so a deferral
never leaves the holder shedding through the 8 s the taker's taunt cools (before, a taker with its taunt spent started
the shed too). A warrior, druid or death knight taker has one taunt, so a deferral waits out its cooldown; a paladin
takes with Righteous Defense next tick. Same at 10 and 25, normal and hard mode; a human holder is read the same way and
only bot tanks defer.

**Picked second, independent and small, a bug: phase 3 melee on Yogg are never held at the melee spot.** Melee samples
on Yogg from +15 s to the first tank death (new `--phase3` `melee on Yogg`): over 30 yd from the melee spot 87 / 87 / 45%
(window 25: 57 / 97 / 54%), within its 15 yd 6 / 12 / 25%, on Yogg's west side, his front, 12-22 yd from him and
within reach of the Guardian spawn ring (38-48 yd from him). The tanks hold the melee spot and taunt 30 yd, so a fresh
Guardian no tank took at stasis end walks away from them after the first player in the map's list, a fixed melee
standing out there (uld1 Uldxay, uld2 Uldyay; `first victim` melee at +3.9-4.1 s 5 / 4 / 0 of 11 / 10 / 11, uld3's
tanks taking theirs at stasis end): uld1 10:15-10:31 Uldxan and Uldxay stood 38-40 yd from the spot on Yogg when G371,
woken at +0.3 s by Uldxal's Army of the Dead ghouls and held by them 34-56 yd out for 21 s, turned on them (both dead,
the tanks 37-62 yd off); uld2's Uldyan (9:40.1) and Uldyaw (10:00.4) died the same way, 39-48 yd from the spot. Root
cause:
`YoggSaronTrigger::PhaseThreeStationReaches` (`UldTriggers_YoggSaron.cpp:71-79`) gave melee a reach of `MeleeDistance`
(0.75) from Yogg's centre: 18.4 yd from the spot minus the 15 yd radius is 3.4, so it read Yogg out of the melee spot's
reach and released them (`yogg.station` `released` as each picks Yogg, e.g. uld1 Uldxar 9:31.8, then `reach melee` to
(1980.4, −47.1), 31 yd from the spot). Yogg's combat reach is 20, so the spot is 18.9 yd 3D inside his 22.25 `reach
melee` range.

Bug: the doc's station claims a bot while standing there costs nothing, and melee wait at the spot for guardian control.
Fix (`UldTriggers_YoggSaron.cpp`): melee reach is `reach melee`'s, `MeleeDistance` plus both combat reaches; the station
then walks melee on Yogg to the spot round the body and parks them. A Guardian counts likewise by its live reach (display
29024: 1, grown by Empowered's scale): released past 17.25 yd plus that reach, was 15.75. Ranged keep `SpellDistance`; the resolver's station test reads only phase 3 ranged, unchanged.
Same at 10 and 25, normal and hard mode, with or without a player. No shared code.

**Next window should show:** for the swap, `swaps deferred for a Guardian on a raider` above 0 wherever a swap meets
one, and `tank deaths within 10 s of one on them` 0 (one or two a pull at 35-50% means back to the old order);
`fresh on a raider` time and `raiders killed by a fresh Guardian` under 30.3 / 18.3 / 0.3 s and 2 / 5 / 0. For the
melee, `melee on Yogg` within 15 yd 80%+ and beyond 30 near 0; melee damage on Yogg no lower than 29.5 / 26.4 / 33.5k/s
(the `damage split` melee rate times its Yogg share); `first off a tank` to melee fewer. Phase 3 still ends on the pile
and the beacon heals.

**Seen, not picked.**

- The first beacon heal ended both tanks in two pulls: uld1's 10:27.7 heal (tank Uldxaa held up to 3 healed, took 256k,
  healed 241k, died +16.0 s), uld3's 9:38.95 (+11.4 / +16.2 s). The stuck throughput race again.
- Ranged on leftovers stand out of the tanks' reach too: ranged samples over 30 yd from the melee spot 17 / 87 / 90%,
  and uld2's three ranged kills (Uldyau, Uldyaj, Uldyao, 9:56-9:59, 48-51 yd from both tanks) came from there, G363
  on a ranged 0.2 s after it spawned, before its stasis ended. The far leftover walk is the user's choice; worth a
  question if fresh kills of ranged persist once melee stand at the spot.
- Army of the Dead in phase 3 (uld1 9:52.9; window 24 uld3 once): its ghouls woke two fresh Guardians in the spawn ring
  (+3.2 s, +0.3 s) and held one out of every taunt for 21 s.
- uld1's Uldxai, the enhancement shaman, died to a phase 1 nova twice again.

**Review.** Both root causes hold. The reader reread gives the log's `melee on Yogg`, `fresh on a raider` and
fresh-kill lines for all three pulls. uld1's melee went `yogg.station` `released` at 9:31.6-9:35.5 and `reach melee`
walked them in on Yogg's west side, Uldxar stopping at (1980.4, −47.1), 22.0 yd from him in 3D: Yogg's reach is 20
(display 28817 since core `2026_11_06_01`), so the findings doc's 30 (melee range ≈ 34 yd) and the reader's
`COMBAT_REACH` entry now read 20. The Guardian reach above is the Immortal Guardian's (display 29024), not the phase 1
Guardian's 4. A paladin taker in a live swap defers its second button too: Righteous Defense takes up to 3, then Hand
of Reckoning goes to a Guardian on a raider before the holder's fourth. Left: the reader and the doc give a Crusher
reach of 8, where `2026_11_06_01` sets display 28814 to 4.5 unscaled; not checked at runtime.

**Question for the user (the deferral's scope).** The answer was for a fresh Guardian; the build defers for any
Guardian on a raider, fresh or one a tank already held that slipped. Over the 17 swaps of windows 25-26, at the moment
a swap fell due 2 met a fresh Guardian on a raider and 3 a slipped one, and over the next 8 s slipped ones sat on
raiders about as often as fresh ones (177 against 161 snapshot rows), so the build defers about twice as often as the
answer asked. **Recommended: keep it for this window**: the reason given, a full-power Guardian one-shots a healer,
holds for a slipped one too, and `tank deaths within 10 s of one on them` counts the cost either way.

## Round 19 (2026-11-24, window 27 build 0687fa6a4: round 18)

**Data.** `raidtest.py report --boss yogg-saron --raid uld --head 0687fa6a4 --pulls 10` (window report
`scratchpad/window27/window27-yogg-saron.md`): one valid pull per pool, 0 humans, Freya's 62670 on the raid, no DC veto
inside a pull. Read with the full `yogg_saron.py` and scratch scripts (`scratchpad/yogg-r19/`: the uld1 handover's
Guardians, raid and back-line moves every snapshot, each back-line phase 1 dodge's destination against the low Guardians
on the stack over all 21 pulls of windows 19-27, cloud radii and speed per pull, the uld2 Death Ray hit) over `snap`,
`move`, `note`, `dmg` and `death` rows. Windows 25-26 are the baseline.

| Pool | Run | Trace | Out | Phase 1 | Phase 2: Brain lost per cycle | Phase 3 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261124-134846-9` | `603_9_yogg-saron_1795528172` | wipe 8:42, 33 deaths | 141.2 s, 12 deaths | 380.7 s, 4: C 2.3, I 0.9, S 0.0, C never cleared; Brain at 96.8% | never |
| uld2 | `tr-20261124-134826-8` | `603_8_yogg-saron_1795528151` | wipe 12:42, 30 | 130.9 s | 602.0 s, 6: C 13.3, I 21.8, S 16.2, C 3.4, I 10.5, S 2.9 (held from 34.6% at 13.2 s left, survived at 31.8%); wave 7's team never opened its door | never |
| uld3 | `tr-20261124-134704-7` | `603_7_yogg-saron_1795528070` | wipe 11:55, 32 | 141.4 s | 434.0 s, 5: I 25.8, S 18.3, C 11.6, I 6.3, S 8.0 (held from 36.2% at 18.9 s left, phase 3 7.6 s before the end) | from 9:35.4 with 6 tentacles (7.79M; Crushers 41.9, 83.2, 95.1%), 140 s, Yogg 30 → 25.9% |

Deaths by mechanic (a rezzed bot dying again counts twice):

- uld1: phase 1 Shadow Nova 12 (1:00.8 two melee; **2:06.0 eight**: three melee, two ranged, three healers; 2:15.8 two
  melee); phase 2 Insane and script kills 6, Squeeze 5, Corruptor 4, Psychosis 2, Laughing Skull 2, Brain Link 1, a
  charmed raider's Living Bomb 1. A brain team of 15 took the Brain 3.2% in four cycles.
- uld2: phase 1 Shadow Nova 3 (melee, one each); phase 2 Squeeze 7, Corruptor 6, Death Ray 4 (**8:00.9 three ranged**,
  11:18 one grabbed), Insane and script kills 4, Grim Reprisal 3, Laughing Skull 2, a charmed raider's Malady 1.
- uld3: phase 1 Shadow Nova 4 (1:52.5 two melee, one through Hand of Sacrifice; 2:12.3 a melee and a Fervor holder);
  phase 2 Laughing Skull, Squeeze, Grim Reprisal, Death Ray (grabbed) 1 each; phase 3 Immortal Guardian 20 (the first
  tank at 10:30.6, +55.2 s), Lunatic Gaze 3 (ranged), Corruptor 1.

**Rounds 17 and 18 did not cause the 2:06 multi-kill or the long phase 2s: their code runs in phase 3 only.**
`git diff e834a2708 0687fa6a4` touches Yogg in `ControlPhaseThree` and `PhaseThreeStationReaches` (called only by the
phase 3 station, positioning and target pick), and round 17's `BurstAllowedOnAdd` passes the base gate on tentacles
while the Ulduar window still holds phase 2 burst to the Brain (`phase 2 burst` on the platform 0 / 0 / 0). The 2:06
multi-kill is an older bug, picked below. uld2's 602 s phase 2 is chance plus known mechanisms: the random illusion
order gave it two Chamber cycles in its first five (window 26's uld2 one), Diminish Power was carried below a median 26.0
/ 31.5 / 40.2 s in cycles 4-6 (×0.85 / ×0.64 / ×0.51 to the door) because the breaks waited for portal entries at +18.8 /
+23.3 / +19.4 s (question below), and the 8:00.9 Death Ray took three ranged (seen, below); it missed phase 3 by 1.8%.
uld1's phase 2 is downstream of its phase 1.

**Verdict on round 18, swap deferral: not exercised.** `swaps deferred for a Guardian on a raider` 0, `yogg.swapdefer`
never written: only uld3 reached phase 3, and before its first tank death no tank held 3 Guardians, the swap's floor
(one swap, 10:41.4, after it). `tank deaths within 10 s of one on them` 0. Kept, per the user's answer for this window.

**Verdict on round 18, phase 3 melee at the spot: fixed on its own check.** uld3 `melee on Yogg` within 15 yd of the
spot 90%, beyond 30 yd 2% (window 26: 6 / 12 / 25% and 87 / 87 / 45%); `first off a tank` to melee 2 of 11 (window
26 `first victim` melee 5 / 4 / 0 of 11 / 10 / 11). Melee damage on Yogg 20.9k/s (61% of 34.2k/s; window 26 29.5 / 26.4 /
33.5k/s): melee near the tanks now take healed and gaze-safe Guardians (30% of their damage), and five of the ten brain
team melee never reached the station. One pull; read again. Round 17's chain taunt did not fire here (2 short taunts, 0
chained, one non-tank swung at and killed).

**Stop rules.** No kill. Phase 3 stays stuck (rounds 3-5) and is not picked. The phase 1 nova was picked in round 7
(Guardians dying off the stack); this is a different root cause, the back line's own walk into the stack.

**Ranking** across pools: phase 2 collapse 47 (uld1 21 after its phase 1, uld2 27 from 8:00), downstream of uld1's
phase 1 and uld2's slow cycles; Shadow Nova 19 (12 in uld1); phase 3 Immortal Guardians 20 (stuck); Death Ray 5;
Lunatic Gaze 3.

**Picked: the back line dodges into the stack.** uld1 2:01.99-2:02.14: nine ranged and healers at the station, 21.5 yd
west, stepped off an orbit-1 cloud (15.3 yd out, passing 6.2 yd inside them) with `yogg-saron phase 1 spacing action`
to (12.8 yd, bearing 163°), 11.3 yd from G869 at 9% on the stack. From 2:02.66 they dodged G867 (8%, dragged off the
stack to 25 yd by the Fervor tank Uldxam) through the middle: destinations 2.0-4.9 yd out, accepted at 2:03.5. G869
died at 2:02.97 (15 raiders, 8 of the back line), G867 at 2:06.03 at 10.4 yd (14 raiders, 6 of the back line, eight
dead). Root cause: `GetYoggSaronNovaThreats` leaves out low Guardians on the stack for ranged and healers, rightly for
the trigger since the station is outside their blast, so the action's hazard set does not have them either, and
`FindNearestPositionClearOfHazards` with its middle bias lands the dodge inward (`UldActions_YoggSaron.cpp`,
`YoggSaronPhase1SpacingAction::Collect`, no `clear` for the back line). Over windows 19-27 (21 pulls), 1003 of 2025
back-line phase 1 dodges ended inside 20.5 yd (887 cloud steps), and 40 of 83 back-line nova hits, 7 of 12 back-line
nova deaths, fell on a bot still standing where one left it (`scratchpad/yogg-r19/inside.py`). Noted as seen in rounds
10 and 11 (window 17 uld1 1:10.9, window 19 uld1 1:10.6), never picked.

Bug: the doc's back line stands outside the stack's blast. Fix (`UldEncounter_YoggSaron.h`, `UldActions_YoggSaron.cpp`):
`ULDUAR_YOGG_SARON_P1_BACK_LINE_INNER_EDGE` (the band's inner edge, 21.5 − 1.0) as `set.clear` and `set.fallbackClear`
for ranged and healers, so a phase 1 dodge, cloud or nova, never lands closer to the middle; a bot already inside is
moved out when the node fires. The trigger is unchanged; the station node walks a bot back from inside the band as
before. Same room and station at 10 and 25, normal and hard mode, with or without a player (bots' dodges only). No
shared code. Reader: `--novas` `back-line phase 1 dodges landing inside the band` and `nova hits on one still standing
there` (`inside_dodge_hits`, tested); here 36 of 83, 12 (3 died) / 35 of 190, 1 / 33 of 52, 1; windows 25-26: 22-76 a
pull, 0-1 hits.

**Found on the way, a stale model: the cloud orbits.** Every trace on disk (windows 6-27) measures the clouds at 15.3 /
20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd from `ULDUAR_YOGG_SARON_MIDDLE` at 2.0 yd/s (Sara's `creature_summon_groups` group 0,
core DB `2026_09_30_00`), while `ULDUAR_YOGG_SARON_CLOUD_ORBITS`, the reader and the findings doc hold the old script's
11.6 / 21.4 / 31.2 / 41.0 / 50.9 / 60.8 at 3.0. So the 21.5 yd station sits in reach of orbits 1, 2 and 3, the
cloud-free core is 6.8 yd (the code uses 2.8, the pile leash 3.1), and `--clouds` labels most spawns `no cloud`. The
findings doc now says so; the constants are a question (positions).

**Next window should show:** `--novas` `back-line phase 1 dodges landing inside the band` 0 and `nova hits on one still
standing there` 0 (here 36 / 35 / 33 and 12 / 1 / 1); `back line caught by a nova in phase 1` under 9 / 1 / 2 bots;
phase 1 nova deaths under 12 / 3 / 4 and no back-line death to one; `CLOUD SPAWNS` `the raid's own feet` no higher than
6 / 5 / 5 (the dodge now steps along the band or out); phase 1 no longer than 131-141 s. Round 18's deferral again
wherever a swap meets a Guardian on a raider, judged by tank deaths within 10 s of one.

**Questions for the user** (plan changes, not built):

- **Q1, the cloud orbits.** The phase 1 positions were derived from cloud orbits the core no longer uses (above): the
  station is reached by three orbits, not one, and melee could stand out to ~6.8 yd without waking orbit 1. Should a
  round re-derive the phase 1 cloud geometry from the measured orbits? **Recommended: yes, in two steps**: first the
  reader's orbit labels and a measurement of what each orbit costs at the station (back-line wakes and steps per pass),
  then one position change at a time with navprobed spots, the melee core first; the only single-orbit band near the
  back line, 29.4-32.9 yd, is past spell range from the stack, as the doc judged for 31.2.
- **Q2, the Diminish Power break waits for the last portal.** Round 16's break fires once every team member is below
  (the user's (b): after the brain team clicks), so a straggler holds it: uld2 cycles 4-6 claimed at +19.1 / +25.2 /
  +19.4 s, and the team below carried ×0.85 / ×0.64 / ×0.51 for 26-40 s. Should the break fire once the members
  carrying that Crusher's stack are below, a straggler still on the platform taking the re-cast down with him?
  **Recommended: yes, for one window**: it frees the rest ~15-20 s sooner, and the straggler carries what he would
  have carried anyway.

**Seen, not picked.**

- uld2 8:00.4: a Death Ray hit seven ranged and healers, three died. Four were on a `yogg-saron body detour action`
  walk and issued no dodge for ~1 s while the ray closed from 13.5 to 3.3 yd (Uldyap: `yogg.deathray` `inside` at
  7:59.41, its next move a spacing walk at 8:00.46, after the first hit); no action row in the gap, cause not found.
  Next candidate.
- uld1 1:48.4: the tank Uldxam took Sara's Fervor holding G867 (26-39%) and ran to 34 yd, dragging it off the stack to
  the handover anchor, where it turned on a ranged at 2:00.5: a Fervor tank's run drags its Guardians.
- The station trigger's band test reads `GetDistance2d` (centre − 1.5), so a bot on the spot reads 20.0, off the 21.5 ±
  1.0 band: `p1station` re-issues `band` at the spot (`ok=0 there`) and `parked` never latches.
- `YoggSaronCloudLead` projects at `GetSpeed(MOVE_RUN)`; the clouds move at 2.0 yd/s, so if their run speed is higher
  the lead overshoots (not checked).
- uld3's phase 3 opened with three Crushers; ranged 96% on tentacles all phase, Yogg 0.04%/s.

**Review.** The root cause holds: uld1's nine dodges to 12.8 yd at 2:01.99-2:02.14 and the walks to 2.0-7.1 yd
through the middle, accepted at 2:03.43-2:03.55, are in the `move` rows; `inside.py` reproduces 1003 of 2025 (887
cloud), 40 of 83 and 7 of 12; every window 27 cloud measures 15.3-54.1 yd at 2.00 yd/s. The fix also takes a cloud
step's inward option, which the middle bias chose 887 times: where nothing past 20.5 yd clears a cloud and its lead
within the 25 yd search, the bot now stays for the station walk, so `the raid's own feet` (6 / 5 / 5 here) judges
that cost. Doc corrected: `InformCloud` reads `GetDistance`, net of Sara's 1.5 and the cloud's 1.0 reach, so orbit 2
(21.4 yd 3D, 18.9) is contact-only too: window 27's 15 summons on orbits 1-2 each had a raider within 8.7 yd, while
orbit 3 fired twice with nobody near. The reader's `--clouds` `inner` list (`CLOUD_ORBITS` under 20) still holds
orbit 1 alone; that is Q1's reader step.
