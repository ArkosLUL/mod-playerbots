# Thorim polish log (25-man hard mode)

Reader: `tools/botobs/bosses/thorim.py` from round 8 (`--deaths`, `--phase2`; `raidtest.py report` runs it on
every counted pull). Rounds 1-7 read the traces with throwaway scripts.

## Round 1 (build 61011f323, DC 5443dbc)

**Data:** window 7, one pull per pool, all void: `tr-20261122-102044-33` (uld1), `-102606-34`
(uld2), `-101241-29` (uld3); traces
`env/dist/logs/botobs/603_{33,34,29}_thorim_{1795342850,1795343172,1795342366}.ndjson`. Window 6's
`603_3_thorim_1795299232` (uld3, old drop) read for comparison. Every DC record says `start:
engaged`; the refresh stopped after 5 of 25 members, so the raid opened at 38-100% health and some
casters under 10% mana. No DC veto rows inside a pull.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795342850 | wipe 5:37 | Lightning Orb 9 (5:33); arena Champion 8, Warbringer 4, Evoker 2 (4:11-5:20); Iron Ring Guard Impale 2 (gauntlet, 2:14-2:25); Runic Colossus Smash 1 |
| uld2 | 1795343172 | wipe 6:00 | Lightning Orb 13 (5:56); Champion 6, Warbringer 3, Evoker 2, Commoner 1; Deafening Thunder 2 (5:02); Runic Smash 1 (2:16) |
| uld3 | 1795342366 | wipe 6:14 | Lightning Orb 27 (5:52-6:10, some bots twice); Evoker 2, Champion 2 in the same seconds; Runic Smash 1 (2:10) |

**Failure picked: the raid opens on the start pack before DC's start step** (3 of 3 pulls, and
window 6's one). The in-fight deaths are not picked: an unrefreshed raid doesn't measure the
strategy, and the brief keeps the in-fight plan out of this round.

- Pre-roll: all 25 stand on the drop (2152.00, -232.00) with no target. From -2.0 / -2.2 / -3.0 s
  (uld3 / uld1 / uld2) the non-tank, non-healer bots take the start pack's Dark Rune Acolyte 32886 as
  their target one by one (13 / 13 / 17 of them by the engage) and walk at it (move generator 8,
  heading -117°, the bearing to its spawn (2129.09, -277.14), 50.6 yd from the drop). No tank or
  healer has a target before the pull. Window 6: Uldzay on the Acolyte at +0.18 s, same pattern.
- uld3's first in-pull snapshot (+6 ms): Thorim and the whole pack target Uldzak, the MM hunter who
  took the Acolyte at -1.7 s, so his shot opened it. `pull` src `engage` (Thorim engaged a player).
- Code: `ThorimDpsPriorityTrigger::IsActive` (`UldTriggers_Thorim.cpp:21-45`) had no combat check;
  for DPS it fires whenever `GetThorimDpsTarget` (`UldEncounter_Thorim.cpp:1187`) offers a pick,
  which needs only Thorim alive and hostile, and the arena tiers start with acolytes inside
  `ULDUAR_THORIM_DPS_TARGET_RANGE` (50) and the arena box. `ThorimDpsPriorityAction::Execute` then
  `Attack()`s. The `ulduar` strategy sits on both engines (`PlayerbotAI.cpp:1947-1948`) and
  `BossStateGate` is open while no boss is in progress (`RaidEncounterRules.h:211-224`).
- Core: the pack is a mock fight (`boss_thorim_start_npcs`); `DamageTaken` from a player or pet
  turns all six on that player and `SetInCombatWithZone`s Thorim. The Acolyte is passive and heals
  the mock fight out of combat; the rest are in combat with each other.
- The drop is not the cause: it's 31 yd from the pack, but the picker reaches 50 yd. A drop past
  ~52 yd would hide the bug, which also fires for a human-led raid walking into the arena.

**Classification: bug.** No strategy starts an encounter; this node steers targets inside the fight.

**Fix** (`UldTriggers_Thorim.cpp`, Thorim only): `ThorimDpsPriorityTrigger::IsActive` returns false
until Thorim is in combat, the pull signal `ThorimSplitActive` and `ThorimEncounterStateIsStale`
already read (`JustEngagedWith` at the pull). No size or mode branch, so 10/25 and normal/hard mode
alike. A battle-rezzed DPS out of combat still picks, since the gate is the boss's combat, not the
bot's.

**Risk for window 8, unmeasured: Acid Breath on the pre-pull formation.** Before the pull
`GetThorimArenaAnchor` walks the arena squad in: tank and melee to the centre (2134.99, -263.12),
ranged and healers to ring slots (the arena healers did, headings -88° and -136°, in all three
traces; the tank never moved in the 4-5 s pre-roll, cause unknown). In the mock fight the Behemoth
casts Acid Breath 62315 (enemy cone 24°, 50 yd, 6000 damage) at its victim every 12 s, the Captain,
who stands by the centre; by spawn coordinates the centre is ~9° off the Behemoth-Captain line,
inside the 12° half-angle. A hit puts that bot in combat with the Behemoth, and with Thorim still out
of combat nothing holds the generic pickers (`ThorimDpsPicked` needs a pick or the split), so `dps
assist` or `tank assist` would open on it. The pre-roll keeps no creature or damage rows, so if
window 8 still reads `start: engaged`, look for a bot near the centre that takes the Behemoth as its
first target.

**Question for the user (plan change, pre-pull positions):** if window 8 shows the pull opening off
the Behemoth, may round 2 hold the arena anchors until Thorim is in combat, so the arena squad
waits where it stands and walks in at the pull? Recommended: yes. The start pack dies first and
phase 1's adds come 20 s after its sixth death, so walking 35-40 yd at the pull costs nothing.

**Next window should show** (DC record, pre-roll `snap` rows): every Thorim pull `start: step`, so
the runner counts it; no non-tank with a target before the pull; the first in-pull snapshot's pack
on the DC main tank (aggro start on the Behemoth); a full refresh (all 25 near full health and mana
at the open). Only then do the in-fight deaths say anything; round 2 reads them.

**User answer (2026-11-22):** yes. If the next window shows the pull starting early from the
Behemoth's Acid Breath, round 2 holds the arena group at the drop until Thorim is in combat and
walks it to its spots at the pull.

## Round 2 (build 1b45df985 with round 1, DC 2ce3824)

**Data:** window 9, one pull per pool, all void (`start: engaged`): `tr-20261122-132528-10` (uld1),
`-132749-11` (uld3), `-133049-12` (uld2); traces
`env/dist/logs/botobs/603_{9,11,12}_thorim_{1795353942,1795354089,1795354265}.ndjson`. Throwaway
scripts read the pre-roll `snap` rows, the first in-pull `dmg`/`cast` rows and the DC records'
`bossPulls[].startMs`. No DC veto rows inside a pull. Phase 2 never started (`thorim.p2start` 0
throughout); every pull ended at the Lightning Orb.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795353942 | wipe 6:09 | Lightning Orb 18 (5:54); arena Champion 6, Warbringer 2, Evoker 2 (5:54-6:03); Runic Smash 1 (4:43) |
| uld2 | 1795354265 | wipe 6:15 | Lightning Orb 18 (6:03); Warbringer 6, Champion 5, Evoker 1 (6:03-6:18) |
| uld3 | 1795354089 | wipe 6:15 | Lightning Orb 18 (6:01); Champion 11, Evoker 1 (6:02-6:18) |

**Round 1 verdict: held.** No bot holds a target in any pre-roll snapshot of the three pulls
(window 7: 13-17 DPS on the Acolyte), and the first bot action on the pack is never a pick.

**Failure picked: the pull still opens before DC's start step**, now from the Behemoth's Acid
Breath, the risk round 1 named. In-fight deaths not picked: the pulls are still void.

- Pre-roll: the arena squad reaches its anchors (tank and melee on the centre (2135.0, −263.1),
  ranged and healers on ring slots), and 2-7 s later one snapshot drops every bot at the centre
  plus the one on ring slot (2129.4, −251.4), 6-7 bots, by 10-64% each (the tank least): uld3 at
  −3.8 s, uld2 at −0.25 s, uld1 in the open's own tick. Nobody at the drop or on another ring slot
  drops. The in-pull rows name it: Acid Breath 62415 from the Behemoth, 8-11.6k direct plus ~1k
  ticks every 2 s on the same bots, the next direct hit 12 s later.
- What opens each pull, matched to DC's `startMs` within 0.3 s:
  - uld1: Uldxai (enhancement shaman at the centre) Lightning Shield 49279 on the Behemoth at
    +9 ms, same tick as the first breath; pull `src: engage`.
  - uld3: the first breath (−3.8 s) hits no shaman, so the bots only enter combat (the trace opens
    on `mark` at 0); the second breath at +8.23 s hits Uldzaj (enhancement, just arrived) and his
    Lightning Shield lands at +8.230. DC `startMs` 1795354097484.
  - uld2: the second breath at +11.663 s hits Uldyap; his imp Dagham casts Firebolt on the
    Behemoth at +11.669. DC `startMs` 1795354276972.
- Core: `boss_thorim_start_npcs::DamageTaken` from a player or pet zone-combats Thorim
  (`boss_thorim.cpp:1073-1092`); `EVENT_JB_ACID_BREATH` casts at the victim every 12 s
  (`:1238-1240`), the Captain during the mock fight. Summons (`:354`, `:362`): Behemoth
  (2146.6, −266.7), Captain (2131.3, −259.2); the centre is 9° off that line, the drop 73°.
- Code: `GetThorimArenaAnchor` (`UldEncounter_Thorim.cpp:2156`) answered before Thorim's combat,
  so `ThorimArenaPositioningTrigger` walked the squad into the cone before the pull.

**Classification:** the user's 2026-11-22 answer covers it.

**Fix** (`UldEncounter_Thorim.cpp`/`.h`, Thorim only): `GetThorimArenaAnchor` returns false until
Thorim is in combat. Before the pull the arena squad stays where DC dropped it (nothing else moves
it: the gauntlet squad stood at the drop all pre-roll in every trace); at the pull the leash (drop
35.4 yd from the centre, past 30) and the anchor walk it in. Its users: the positioning
trigger/action, the leash action (split only, so unchanged) and `ThorimArenaAnchorSettled` (the
anchor guard is off before the pull). No size or mode branch. A human-led raid now follows its master
until the pull instead of forming up early.

**Watch:** the Behemoth turns on the DC tank at the drop, with the raid around him. Its breath timer
runs on from the mock fight, so a breath in the first ~5 s while the squad walks in can hit more
bots than the 3-5 around the tank its in-fight breaths hit in window 9. One breath left its targets
at 35-90% health, so it is no wipe at full health; count bots hit by 62415 in the first 15 s.

**Next window should show:** every Thorim DC record `start: step` and counted; no 62415 hit and no
bot target before the open; the open's first damage on the pack from the DC tank; then, with valid
pulls, the in-fight read starts: so far every pull dies to the Lightning Orb without phase 2.

## Round 3 (build b40af8aab with round 2 and lane T, DC 4d3978b)

**Data:** window 10, one pull per pool, all valid (`start: step`): `tr-20261122-152622-4` (uld1),
`-5` (uld2), `-6` (uld3); traces `env/dist/logs/botobs/603_{4,5,6}_thorim_{1795361316,1795361326,1795361326}.ndjson`.
First pulls with full DK talents and soulstoned tanks. Window 9's six bot-led pulls
(`603_{9,11,12,13,14,15}_thorim_*`, void at the time) read for the gauntlet too. Throwaway scripts read
`snap`, `act`/`move` (lever and lead actions), `dmg` (62415) and `death` rows. No DC veto row inside a pull.
`thorim.squad` notes are missing from uld2/uld3; the corridor squad was read off positions there.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795361316 | wipe 6:10 | Lightning Orb 22 (5:47-6:05); arena Champion 4 (5:47-6:00); gauntlet Runic Smash 4, Iron Ring Guard 3 (3:43-4:06) |
| uld2 | 1795361326 | wipe 6:10 | Lightning Orb 19 (5:47-6:05); Champion 6, Warbringer 3, Evoker 2 (4:26, then 5:51-6:05); Runic Smash 1 (2:38) |
| uld3 | 1795361326 | wipe 6:11 | Lightning Orb 19 (5:48-6:06); Champion 10 (5:52-6:05); Impale 1 (3:01), Runic Smash 1 (3:25), Colossus Smash 1 (4:04) |

**Round 2 verdict: held.** All three DC records `start: step`. No 62415 hit before the open; the only
pre-roll targets are in the last snapshot (−0.10 to −0.51 s), all on the Behemoth: DC's own aggro start.
The breath now lands on the raid gathered at the drop: 4 / 10 / 14 bots at +5.0 / +7.9 / +7.4 s
(uld1/2/3), 6-12 again at +17-20 s, no death (first death 2:38). Not picked: health, no lives.

**Failure picked: the bot-led gauntlet stops at the dead Runic Colossus.** Phase 2 never starts because
nobody reaches the Rune Giant, let alone Thorim:

| Pull | lever action / at its stop / lever pulled | Colossus dead | after it |
|---|---|---|---|
| uld1 | 26.8 / 33.0 / 189.6 s | ~290 s | squad idle at y −355..−410 until the orb |
| uld2 | 27.2 / 34.5 / 117.8 s | ~208 s | idle 2.5 min |
| uld3 | 28.1 / 33.5 / 159.2 s | ~254 s | idle |
| window 9 (6) | at its stop 39-50 s, pulled 73.9-240.2 s | 166-343 s | idle; nobody past y −410, Giant never engaged |

- Walk: `ThorimGauntletLeadTrigger` (`UldTriggers_Thorim.cpp:115`) is the waypoint step or
  `ThorimGauntletLeadColossus` (`UldEncounter_Thorim.cpp:1580`), which returns nullptr once the Colossus is
  dead. Nothing else moves the squad: the Giant (spawn (2134.57, −440.32, 438.33), `boss_thorim.cpp:386`)
  is ~100 yd on up a ramp and `GatherThorimEncounterTargets` drops anything past 50 yd
  (`UldEncounter_Thorim.cpp:1104`); the Iron Honor Guards at (2220.3, −436.2) and (2198.1, −428.8) are
  56-80 yd from where the squad stood. A real player leading walks on himself, so this only bites bot-led.
- Lever (the small second, same lane-N code): the action walked to `MoveTo(lever, GetInteractionDistance()
  − 1)`. `MoveTo(WorldObject*)` measures `GetDistance`, net of the bot's 1.5 reach and the lever's 1.17
  size (scale 3), so every leader stopped 6.6-6.7 yd from the centre, approaching from the west (all nine
  pulls). `IsAtInteractDistance` (`GameObject.cpp:3067`) is the lock spell's range (lock 1845 → Opening
  68398, range 5) around display 295's box ×3: local x −5.75..6.75, y −5.43..5.48. At local x −6.5..−6.7
  the bot was outside, so the action re-issued the same move ("there") until a fight swung the leader in
  diagonally (inside at local (−5.19, −4.15) in uld1 at 189.6 s). Waits of 25-198 s at the stop (nine pulls), out of the 5 min clock.

**Classification: bug.** The user-approved lane N (2026-11-21) has a bot lead the gauntlet with no player
in it; its walk ends at the Colossus and its lever walk stops short of use range.

**Fix** (Thorim files only, no size or mode branch, same code with or without a real player):
- `ThorimGauntletLeadRuneGiant` (`UldEncounter_Thorim.cpp:1594`, header `:555`): past the last waypoint
  and the portcullis, the bot leader walks to the Giant while he is alive, attackable and out of combat.
  "Colossus dead" is the Giant losing `UNIT_FLAG_IMMUNE_TO_PC` (`boss_thorim.cpp:1376`, with the first
  doors at `:1367`), not the cached Colossus lookup, which arena melee also fill from 162 yd and which can
  read empty while he lives. The portcullis gate keeps the 150 yd Giant scan off the arena, where a miss
  would read as "Giant dead" to `ThorimBalconyOpen`. The action (`UldActions_Thorim.cpp:312`) paths to the
  Giant's own position at combat priority, not `MoveTo(unit, d)`, whose straight-line point can land off
  the mesh up the ramp; and walks all the way in, since once an Iron Honor Guard engages the Giant spawns
  guards every 10-20 s until he is pulled (`boss_thorim.cpp:1285`, `:1510-1516`) and the squad's picker takes
  guards first. Followers tether as before (25 yd, healers 20).
- Lever action (`UldActions_Thorim.cpp:285`): `MoveTo(lever, 0)`, touching distance (~2.7 yd from the
  centre). Bots have stood 0.3-4 yd from it on most bearings in these traces, the west side included.
- Probe `thorim.leverpulled` (`ObsValue` on the latch, header `:435`).
- Review: the Giant walk also needs the leader within 150 yd of the Giant's spawn. The scan reaches
  ~159 yd with both sizes (Giant combat reach 7.5), a leader done with the left lane can stand ~165 yd
  out, and that miss is cached for every bot as "Giant dead" to the balcony walk.

**Not fixed, predicted next blocker: no bot may hit Thorim from the balcony in phase 1.** Phase 2 starts
only on damage from a player above z 430. `GetThorimDpsTarget` picks him for a corridor bot up there
(`UldEncounter_Thorim.cpp:1259`, meant for exactly this), but `ThorimDpsTargetAllowed` (`:1148`) forbids
him to every bot while he is on the balcony, so `ThorimDpsPriorityAction` refuses the pick and drops him as
a target, and `ThorimBalconyGuardMultiplier` (`UldMultipliers_Thorim.cpp:90`) zeroes Attack/Reach on him
for corridor bots above the floor during the split; the generic pickers are off for non-tanks then. Every
earlier phase 2 had a human hit him. If window 11 shows the squad at the edge with Thorim untouched, that is
round 4: allow him for a corridor bot past `ULDUAR_THORIM_BALCONY_5` (clear of both trap fields).

**Next window should show** (lead `move` rows, `thorim.leverpulled`, Giant `snap` rows): the lever pulled
~6 s after the lever action starts (~30-45 s, was 74-240 s); the Colossus dead ~100 s later; then
`thorim gauntlet lead action` moves aimed at (2134.6, −440.3, 438.3) with the leader's distance to the Giant
closing (a `nopath` row or a flat distance means the route needs navprobe); Iron Honor Guards and the Giant
engaged and the Giant dead; corridor bots above z 430 on the balcony chain. Sif's clock (~170 s from the
pull) is likely still missed, so a phase 2 here would be normal mode. Gauntlet deaths may rise (Honor
Guards, Rune Detonation, Stomp); the orb still wipes if the predicted blocker holds.

## Round 4 (data from build 697052475: round 2 code, round 3 merged but not built)

**Data:** window 10's rerun, one pull per pool, all valid (`start: step`): `tr-20261122-162559-4` (uld1),
`-5` (uld2), `-6` (uld3); traces `env/dist/logs/botobs/603_{6,7,8}_thorim_{1795364897,1795364926,1795364898}.ndjson`,
read through `raidtest.py report --boss thorim --raid uld --head 697052475 --pulls 10` and throwaway scripts over
`snap` rows (bot height and progress; Thorim, Colossus, Giant). No DC veto row inside a pull. Stop rules, verdict
and ranking skipped: round 3 has no pulls yet. Phase 2 never started.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795364897 | wipe 6:07 | Lightning Orb 27 (5:44-6:03); arena Champion 2, Warbringer 1 (5:44-5:49) |
| uld2 | 1795364926 | wipe 6:09 | Lightning Orb 20 (5:46-6:05); Champion 6, Warbringer 2, Evoker 2 (5:46-6:04) |
| uld3 | 1795364898 | wipe 6:10 | Lightning Orb 11 (5:46-6:05); Champion 9, Warbringer 3 (5:47-6:04); gauntlet Runic Colossus 7 (5:50-5:57), Runic Smash 2 (5:17, 5:33) |

Gauntlet, round 2 code: the Colossus died at ~2:21 (uld1) and ~2:32 (uld2), then the squad stood at y −351..−411
until the orb; uld3 only reached him at ~5:17 and died there. Nobody went up the ramp.

**Failure picked: the blocker round 3 predicted, no bot may hit Thorim from the balcony in phase 1.**

- Code: `ThorimDpsTargetAllowed` (`UldEncounter_Thorim.cpp:1148`) refused Thorim to every bot while he is above
  the floor line, so for a corridor DPS on the platform `ThorimDpsPriorityTrigger` fired every tick on the pick
  `GetThorimDpsTarget` offered (`:1258`, any corridor bot above the floor line) and the action dropped it.
  `ThorimBalconyGuardMultiplier` (`UldMultipliers_Thorim.cpp:90`) zeroed Attack/Reach on him for corridor bots
  above the floor during the split, and `thorim disable automatic targeting` (`UldDefinition_Thorim.cpp:156`)
  holds the generic DPS pickers then. Healers keep theirs, but the trigger pulled him off them too.
- Core: `boss_thorim::DamageTaken` (`boss_thorim.cpp:515-547`) starts phase 2 only for `who->IsPlayer()` above
  z 430 once `_isHitAllowed` (the Rune Giant's `JustDied`, `:1474-1483`), so pets cannot either.
- Traces: none reached the balcony. No bot in these three pulls stood above z 427.3; across all 16 Thorim
  traces on disk, all bot-only, only one bot is ever above z 430 in phase 1, briefly at 0:53 (`603_3`).

**Classification: bug.** The strategy means the corridor squad to hit him from the platform: `GetThorimDpsTarget`
offers him for that, and the balcony walk leaves the edge tick "to whatever shoots him"
(`UldActions_Thorim.cpp:360`). Without a human nothing ever starts phase 2.

**Fix** (Thorim files only, no size, mode or player branch):
- `ThorimBalconyHitAllowed` (`UldEncounter_Thorim.cpp:1174`, header `:500`): a bot above the floor line, in the
  corridor squad, during the split (Thorim in combat on his balcony), whose `thorim.balcony` latch has passed
  `ULDUAR_THORIM_BALCONY_5` (read, never advanced: the walk owns it). The ranged hold spots use the same step gate;
  the latch only advances along the chain, so both Paralytic Fields are behind the bot.
- Review: the bot must also stand at or past `BALCONY_5` (`ThorimBalconyWaypointReached`, `:142`, the latch's own
  test). The walk only rereads the latch once the Giant is dead, so a step left from an earlier pull on the same
  instance could read as past it from the Giant's ramp and hand him to a bot ~140 yd off, behind the second doors.
- `ThorimDpsTargetAllowed` allows Thorim for that bot (Sif stays forbidden); `GetThorimDpsTarget` offers him only
  to that bot (`:1292`); `ThorimBalconyGuardMultiplier` passes Attack/Reach on him for it (`:120`).
- Geometry: the jump edge (2137.1, −291.2) is 8.9 yd from Thorim (2131.0, −297.6), inside his 9.08 yd melee
  reach, so the walk's end is the firing spot for melee and ranged; `BALCONY_5` is 21.4 yd from him, and the
  north trap (2134.9, −339.7) is south of both.
- Arena bots never pass the gate (squad, height), and the arena target guard still drops him for them, so the
  arena hold is unchanged. A corridor healer or tank past `BALCONY_5` may now keep him if a generic picker hands
  him over; a wand or swing from up there starts phase 2 as well.
- With a real player in the raid the code is the same: the first corridor bot past `BALCONY_5` starts phase 2
  unless the player hit him first.

**Next window should show** (rounds 3 and 4 together; round 3's lever and Giant checks come first): after the
Giant dies, corridor bots' `thorim.balcony` reaching 5-6; their `thorim.dpstarget` naming Thorim only from then;
the first player `dmg` row on Thorim from a corridor bot at z ~438, seconds after the first one reaches the edge;
Thorim under z 429.6 within ~2 s and `thorim.p2start` set; no arena bot and no corridor bot short of
`BALCONY_5` ever targeting him before that. A squad at the edge with Thorim untouched means the gate stays shut
(read the step latch, LOS, and which multiplier zeroed `attack`). Then the Lightning Orb deaths stop and phase 2
deaths become the read; Sif's clock (~170 s from the pull) is likely missed, so normal mode.

## Round 5 (build d2874f536 with rounds 3 and 4)

**Data:** window 11, one pull per pool, all valid (`start: step`, no DC veto row inside a pull):
`tr-20261122-182719-4` (uld1), `-5` (uld2), `-6` (uld3); traces
`env/dist/logs/botobs/603_{4,5,6}_thorim_{1795372129,1795372130,1795372131}.ndjson`, read through
`raidtest.py report --boss thorim --raid uld --head d2874f536 --pulls 10` and throwaway scripts over `snap`
(hp and position of Thorim, Sif, Colossus, Giant, Honor Guards, start pack), `note` (`thorim.leverpulled`,
`thorim.balcony`, `thorim.dpstarget`, `thorim.p2start`), `act`/`move` (lever, lead, balcony), Sif's `cast` rows and
`death` rows. `dmg` rows only carry damage to bots, so the first hit on Thorim is read off the pick and the drop.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795372129 | wipe 6:10 | ramp pile 13 (1:34-2:20: Iron Honor Guard 7, Colossus 3, Giant 2, Iron Ring Guard 1); corridor Iron Ring Guard 2 (1:20-1:24); Lightning Orb 11, Warbringer, Champion, Evoker 1 each (5:46-6:05) |
| uld2 | 1795372130 | wipe 6:10 | ramp pile 11 (1:26-2:00: Iron Honor Guard 6, Iron Ring Guard 4, Colossus 1); corridor Runic Smash 3, Iron Ring Guard 1 (1:01-1:14); Champion 8, Lightning Orb 7, Warbringer 1 (5:47-6:06) |
| uld3 | 1795372131 | **kill** 6:34, normal mode | Iron Ring Guard 1 (1:23), Colossus Smash 1 on the ramp (2:34), Thorim Chain Lightning 2 (4:33) |

"Ramp pile" is every gauntlet death south of the Colossus' spawn (y < −395), where Colossus, Honor Guards and
Giant were fought at once.

**Hard mode: no pull.** Sif cast Touch of Dominion (62507) at 0:46.6 / 0:47.1 / 0:50.0 and 62565 on Thorim 150 s
later, at 3:16.7 / 3:17.2 / 3:20.1 (uld1/2/3); uld3's phase 2 came at 3:51.2, 31 s late, and Sif was last seen
at 3:25.0. Phase 1 starts 20 s after the sixth start-pack death (26.5 / 27.0 / 30.0 s), not at the pull.

| Pull | lever action / pulled | lead aims at Giant | Colossus engaged / dead | Giant engaged / dead | balcony walk / `BALCONY_5` | phase 2 |
|---|---|---|---|---|---|---|
| uld1 | 0:27.0 / 0:32.8 | 1:07.9 | 1:19 / never (89% at 2:41, then reset) | ~1:59 / never | none | never |
| uld2 | 0:27.2 / 0:32.0 | 1:04.7 | 1:20 / never | ~1:59 / never | none | never |
| uld3 | 0:30.4 / 0:37.3 | 1:33.2 | 1:39 / ~3:08 | 1:59 / ~3:26 | 3:26.7 / 3:47.4 | 3:51.2 |

**Round 3 verdict: lever held, Giant walk fires too early.** Lever pulled 4.8-6.9 s after the action started
(was 25-198 s at the stop). The lead walk paths up the ramp and reaches the Giant in all three pulls, but it
aimed at him with the Colossus alive and untouched each time (lead `move` rows to (2134.57, −440.32, 438.33) at
the times above), so the squad fought Colossus, both Honor Guards and the Giant at once (uld1/uld2 also one
Honor Guard the Giant spawned).
uld1 and uld2 died there (24 deaths); uld3's bear tank held Colossus and Giant together and lived.

**Round 4 verdict: held** (uld3, the only pull to reach the platform). Uldzau (hunter) latched `BALCONY_5` at
3:47.44, took Thorim as `thorim.dpstarget` at 3:47.73 (the first pick of him by anyone), stepped to the edge at
3:50.96; `thorim.p2start` at 3:51.15, Thorim under z 429.6 at 3:51.25. No arena bot targeted him before. One
leak: at `BALCONY_4` (3:45.68) `dps assist` handed him to the same hunter, who cast one Serpent Sting (49001)
before `thorim dps priority action` failed and `drop target` cleared him at 3:47.15; it did not start phase 2.
Not fixed: harmless, 1.5 s.

**Why uld1 and uld2 never started phase 2:** the whole corridor squad (12) died in the ramp pile by 2:20 / 2:00,
so nobody killed the Giant (`ACTION_ALLOW_HIT` never sent) or reached the platform; the arena squad held until the
not-in-time orb (phase 1 + 5 min, 5:46-5:47) and Berserk adds.

**Stop rules:** not polished (one kill, normal mode). Not stuck: rounds 3, 4 and 5 picked three different causes,
and rounds 3-4's targets now work (uld3 reached the Giant and started phase 2).

**Failure ranking** (all pools): ramp pile 25 (+ the 30 orb/arena deaths of the two wipes it caused); corridor
trash 7 (Iron Ring Guard 4, Runic Smash 3); phase 2 Chain Lightning 2.

**Failure picked: the bot leader walks to the Rune Giant with the Colossus alive** (all three pulls).

- Code: `ThorimGauntletLeadRuneGiant` (`UldEncounter_Thorim.cpp:1646`) gated the walk on the Giant without
  `UNIT_FLAG_IMMUNE_TO_PC`. Past the last waypoint, `ThorimGauntletLeadColossus` (`:1632`) returns nothing once
  the leader is inside 30 yd of the Colossus, or when the cached Colossus lookup reads empty (all three pulls: the
  leader stood 37 / 72 / 58 yd out), so the Giant check came next and passed.
- Core/DB: the flag is set only on the 10-man templates (`2026_08_03_02`: 32872, 32873). In 25-man the creature's
  template is the difficulty row (`Creature::InitEntry`, `m_creatureInfo`), and 33148/33149 have `unit_flags` 0, so
  the 25-man Giant is pullable from the start. Trace: the Giant took player damage at ~1:59-2:01 in all three
  pulls with the Colossus at 91-95%.
- The Colossus' `JustDied` (`boss_thorim.cpp:1363-1377`) opens the first doors (GO 194557, (2227.7, −413.3))
  before it clears the Giant's flag; Thorim's `Reset` → `CloseDoors` shuts them. Shut doors did not stop the walk.

**Classification: bug.** Round 3's intent is "after the Colossus".

**Fix** (`UldEncounter_Thorim.cpp`/`.h`, Thorim only): `ThorimColossusDoorsOpen` (`:1467`) reads the first doors
(`GO_STATE_ACTIVE`), found once within 150 yd and cached by guid in `ThorimEncounterState::colossusDoorsGuid`;
`ThorimGauntletLeadRuneGiant` gates on it instead of the flag. A miss reads as shut, so the leader holds as in
round 2. Same code for 10/25 and both modes (at 10-man the doors and the flag change together); with a real
player leading the gauntlet nothing changes, the lead nodes only run for a bot leader.

**Seen, not fixed:** after uld3's Giant died at (2186.9, −438.5, 427.6), 8 of the 11 living corridor bots stood
below z 429.6, so `ThorimBalconyOpen` skipped them and they walked back through the corridor (still there at
4:20). The Giant was dragged down the ramp by a tank holding the Colossus; with the fix he should be fought at
his spot (z 438). If window 12 still shows bots on the ramp below 429.6 after his death, that is a candidate.

**Next read after this: Sif's clock.** Measured: last waypoint at 65-93 s, Colossus engaged at 1:19-1:39 here;
alone he died 23-33 s after the engage in window 10 (`603_{6,7}`, 1:58 → 2:21 / 2:32), against ~86 s in uld3's
pile. Giant ~30 s from full (uld3: 70% to dead in 21 s), balcony walk ~25 s, plus ~15 s up the ramp and the Honor
Guards. Serial, that puts phase 2 around 3:10-3:50 against a 3:17-3:20 deadline: hard mode on a fast pull at best.
No question yet: window 12 gives the serial timings.

**Next window should show** (lead `move` rows, Colossus/Giant `snap` hp, `thorim.balcony`, Sif's 62565): no lead
move aimed at (2134.57, −440.32, 438.33) while the Colossus is alive; the first such move within ~1 s of the
Colossus' death; Honor Guards engaged only after that; no gauntlet deaths at y < −395 before the Colossus dies;
the Giant dead and most of the corridor squad on the balcony chain; phase 2 in all three pulls, so no Lightning
Orb wipe. Hard mode only where phase 2 lands before Sif's 62565 cast (`thorim.sifjoined` emitted).

## Round 6 (build ebe746ad1 with round 5)

**Data:** window 12, one pull per pool, all valid (`start: step`, no DC veto row inside a pull):
`tr-20261122-194109-4` (uld1), `tr-20261122-194109-5` (uld2), `tr-20261122-194110-6` (uld3); traces
`env/dist/logs/botobs/603_{4,5,6}_thorim_{1795376561,1795376561,1795376560}.ndjson`, read through
`raidtest.py report --boss thorim --raid uld --head ebe746ad1 --pulls 10`, `postmortem.py` and throwaway scripts
(session scratchpad `thorim_r6/`) over `snap` (positions, hp, mana, Thorim's victim), `note` (`thorim.leverpulled`,
`thorim.balcony`, `thorim.p2start`, `thorim.sifjoined`, `thorim.ringspot`), lead `move` rows, `cast` (Sif 62507/62565,
Unbalancing Strike, cooldowns), `aura` (62130), `dmg` (Chain Lightning hops, phase 2 intake by source), `heal` and `death`.

**All three pulls reached phase 2 in hard mode** (Sif joined, no 62565 cast) and wiped there; no death in phase 1.

| Pull | Out | Phase 2 | Thorim at wipe | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | wipe 6:33, 33 deaths | 3:15.8 | 13.1% | Thorim loose (melee on non-tanks after the tanks fell) 14; tanks 5 (melee 3, Unbalancing Strike 2); Lightning Charge 5; Chain Lightning 3; Frostbolt Volley 3; Blizzard 2; Unbalancing Strike on a non-tank 1 |
| uld2 | wipe 5:48, 31 deaths | 3:18.6 | 39.2% | Thorim loose 15; tanks 4 (melee 2, Unbalancing Strike 1, Sif Frostbolt 1); Chain Lightning 3; Lightning Charge 3; Frostbolt Volley 3; Sif Frostbolt 2; Unbalancing Strike on a non-tank 1 |
| uld3 | wipe 6:55, 32 deaths | 3:09.2 | 9.4% | Thorim loose 20; tanks 4 (melee 3, Lightning Charge 1); Chain Lightning 3; Frostbolt Volley 3; Blizzard 1; Unbalancing Strike on a non-tank 1 |

Tank deaths count soulstone and rez repeats. Two of uld1's five tank deaths took a Chain Lightning hop (29.4k and 11k)
in the killing burst; the 5:43.4 one went from 93.6% to dead in one tick (hop 5 plus Unbalancing Strike).

| Pull | lever pulled | Colossus engaged / last seen | lead aims at Giant | Honor Guards engaged | Giant engaged / last seen | `BALCONY_5` | phase 2 | Sif 62507 + 150 s | sifjoined |
|---|---|---|---|---|---|---|---|---|---|
| uld1 | 0:36.1 | 1:34.3 / 1:52.8 | 1:53.2 | 2:00.6, 2:11.8 | 2:12.2 / 2:59.5 | 3:12.6 | 3:15.8 | 3:18.4 | 3:43.9 |
| uld2 | 0:34.2 | 1:37.2 / 2:02.4 | 2:02.9 | 2:07.7, 2:12.7 | 2:24.2 / 3:01.5 | 3:15.4 | 3:18.6 | 3:17.6 | 3:46.6 |
| uld3 | 0:33.1 | 1:23.8 / 1:53.5 | 1:53.9 | 2:01.0, 2:05.5 | 2:14.5 / 2:52.0 | 3:05.9 | 3:09.2 | 3:16.8 | 3:37.4 |

"Last seen" is the last snapshot, at 0-0.7% hp. uld2's balcony edge step was at 3:17.8 and the hit that starts phase 2
precedes `thorim.p2start` by up to ~1 s, so it beat the deadline by under a second; uld1 by ~3 s, uld3 by ~8 s. All 12
corridor bots stood on the balcony at phase 2 and on the floor 30 s later (round 5's walk-back did not recur).

**Round 5 verdict: held, and nothing got worse.** Every check passed: the first lead move at the Giant came 0.4-0.5 s
after the Colossus' last snapshot and none before; Honor Guards were engaged only after it; no gauntlet death at all
(ramp pile 25 → 0, corridor trash 7 → 0); the Giant died and the corridor squad took the balcony chain in all three;
phase 2 in all three, so no Lightning Orb. The shorter pulls are the hard-mode phase 2 race, not phase 1: uld2's 5:48
is P2 +145 s with Thorim at 39%, where window 11's wipes were the orb at 5:46-5:47 with Thorim untouched.

**Watch item (cached Colossus lookup), checked: real, harmless this window.** `GetThorimRunicColossus`
(`UldEncounter_Thorim.cpp:1001`) caches one guid per instance for 500 ms; arena melee refresh it from ~162 yd through
`ThorimTickBarrierBail` (`:1664`) and `TickRunicSmash` (`:928`, run by every bot from `ThorimRunicSmashTrigger`), and that
150 yd scan misses. With the doors gate a miss now only skips a tick: from the last waypoint (1:20.1 / 1:17.8 / 1:04.0)
to the 30 yd stop the leaders got lead orders at most 2.6 s apart and closed 68-77 → 37.5 yd in 9-12 s (two walked back
once to pick up an add). The same miss can drop the Runic Smash latch (`TickRunicSmash` clears `runicSmashSide`): no
Runic Smash death this window (round 5's uld2 had 3). Not fixed; if Runic Smash deaths come back, let only a scanner
within 150 yd of the Colossus' spawn refresh the cache.

**Stop rules:** not polished (no kill). Not stuck: rounds 3-6 picked four different causes, each fixed one held.

**Failure ranking** (96 deaths): the tanks falling, then Thorim loose on the raid, 62 (13 + 49) in all three pulls;
Sif 11 (Volley 9, Frostbolt 2); Chain Lightning 9, plus a hop in two tank deaths; Lightning Charge 8 (4 of them one cast
in uld1's melt); Blizzard 3.

**The top cause, the tanks falling, is the hard-mode damage race, not a bug.** Phase 2 intake was 5.33 / 3.80 / 6.16 M,
Sif 54 / 55 / 52% of it (Volley 1.86 / 1.29 / 1.91 M, undodgeable; Blizzard 244 / 182 / 394k, tanks 206 / 160 / 289k of it
on the tank spot), Thorim melee 26-34%, Chain Lightning 5-6%, Unbalancing Strike 5-6%, Lightning Charge 2-6%. Healer mana
at phase 2 start [61, 54, 86, 80] / [82, 48, 43, 49] / [59, 59, 100, 57]; at the first tank death [33, 20, 5, 30] (+118.6 s)
/ [1, 26, 1, 30] (+106.9 s) / [44, 27, 30, 15] (+190.0 s, the stack ceiling, Thorim 15%). The tank handling works: an
Unbalancing Strike swap followed every landed strike, mostly within 0.1-3.7 s (uld2's +26.6 s strike took a second taunt,
6.5 s; uld3's one 20 s gap was a strike that missed, no 62130 aura), burst and Heroism went out at P2 +0-35 s, and melee
held Thorim in 98-100% of snapshots from P2 +15 s to +100 s. Thorim's `mhp` read
27.19 M. Raid damage on him ran 0.41-0.47%/s, about 30 s short of a kill when uld3's tanks fell. Question 1 below.

**Failure fixed: Chain Lightning crossing between melee ring slots** (the race's largest strategy-side leak). Seven
casts were lethal: camp pairs 2 (uld1 +27.0 s: 2 dead; +117.4: 1), slot to slot 2 (uld2 +102.3: Uldyak, Uldyan,
Uldyag; uld3 +132.4: Uldzay, Uldzat), a tank to slot 2 3 (uld1 +147.5 and +177.6: tank Uldxaa twice; uld3 +147.5:
Uldzaj). 32 / 54 / 56% of all Chain Lightning damage landed on hops after the chain crossed to another group.

- Trace: the solver's slot points stood 8.3-8.8 yd apart at p10 and 12.4 at the median, never inside the jump; the
  bodies of two different slots were within 8 yd of each other in **60.7 / 59.9 / 77.9%** of phase 2 snapshots (median
  closest pair 7.1-7.4 yd). Bodies drift up to ~6 yd off their point (p90 5.5-5.9 yd, bearing p90 36-38°) after a slot's
  composed bearing moves, without Thorim moving (81-86% of the > 4.5 yd cases had the boss within 2 yd of where he was
  at the bot's last ring move). uld2 +102.3: slot 0 solved at 234° stood at −90° (36° off, "arrived"), 7.1 yd from slot
  1; uld3 +132.4: slot 0 solved 248° stood at −86..−100°, 7.3 yd from slot 1.
- Core: `Spell::SearchChainTargets` hops to the closest unhit target with `IsWithinDist(unit, 5.0)` (3D, both combat
  reaches; `spell_jump_distance` 5.0 for 62131/64390, `2026_03_11_00`).
- Code: `ThorimRingWantsMove` (`UldEncounter_Thorim.cpp:2590`) keeps an arrived ring bot until it is 5 yd
  (`GetDistance`, ~6.5 exact) off its point, unless `RingSpotBeatsTheDeadband` (`:644`) sees it in the lit cone or a
  Blizzard zone; nothing re-checks the separation the solver (`RingBearingSeparated`, 60°) put between the points.

**Classification: bug.** The solver means the slots to sit out of each other's jump; the deadband undoes it.

**Fix** (`UldEncounter_Thorim.cpp`/`.h`, Thorim only): `RingBodyBridgesChain` (`:610`) is true when a ring bot stands
within `ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP` (5) + `ULDUAR_THORIM_RING_CHAIN_MARGIN` (0.5) of a living floor body outside
its own slot (another slot, a tank, the camp; the core's `IsWithinDist`), and its own point is clear of every such body
by the same measure. `RingSpotBeatsTheDeadband` ORs it in, after the zone check (no walk into a Blizzard) and only for a
point outside the lit cone. A point that is not clear stays put, so the other bot, whose point is clear, is the one
that moves. Pure read, no latch. No size, mode or player branch: a real player's body counts like a bot's; a human
never gets a melee slot entry (`EnsureMeleeSlot` runs only for the bot itself), so a human melee is a body outside every
slot. Replayed on these snapshots it would fire in 14-31% of ring snapshots, ~2-3 new moves per ring bot per minute; the
tank-to-slot-2 bridge stays (slot 2's point is 7.1 yd from the off-tank spot).

**Review:** counting another slot's ring body where it stands deadlocked the commonest bridge. Two adjacent slots at
the solver's minimum (8.8 yd apart), each drifted toward the other, block each other's points, so neither moves: in the
replay 24 / 36 / 14% of phase 2 snapshots (uld1/2/3) kept a ring-to-ring pair inside 8 yd that neither escape clears,
the stuck pairs' points a median 8.8 yd apart. `RingBodyBridgesChain` now counts another slot's ring bot at its slot's
point (`RingSlotPointsApart`, `:604`, the chord between the composed bearings), since it walks back too, and every
other body where it stands; and a bot within `ULDUAR_THORIM_RING_CHAIN_MIN_STEP` (1 yd) of its point never moves for a
bridge, so a body that won't leave can't make it reissue the move every tick. Replayed: such stuck pairs in 9 / 9 / 13%
of snapshots; the escape fires in 24-38% of ring snapshots, 2.5-3.2 new moves per ring bot per minute. Both lethal
slot-to-slot casts still fire through the 6 s before the cast (uld2's drifted slot 1 bots now too, blocked before by
slot 0's bodies).

**Questions for the user** (plan changes, not built):
1. Hard mode at ilvl 226 is now a race the rosters lose at the mana wall (+100-120 s) or the stack ceiling (+190 s).
   Keep polishing Thorim's mechanics on the 226 rosters with the stop rule waived, as for Algalon and Vezax? Or give
   `uld-thorim` a 5th healer (one DPS fewer, ~4% damage)? Recommended: keep polishing with the stop rule waived; revisit
   a 5th healer only if the mana wall is still the first tank death once the chain and tank-spot leaks are closed.
2. The off-tank spot (2114.25, −252.65) sits 7.1 yd from slot 2's default point, so every chain reaching a tank runs
   into slot 2 (3 lethal casts, 2 tank deaths). Move the off-tank spot 3.5 yd west of the tank spot instead,
   (2107.25, −252.65), ~10 and ~14 yd from slots 0 and 2, after navprobe and a Blizzard-track check? Recommended: yes.

**Next window should show** (snapshot geometry with `thorim.ringspot`, Chain Lightning `dmg` rows, `death`): bodies of
two ring slots within 8 yd in well under 60-78% of phase 2 snapshots (target under ~15%); no Chain Lightning hop from one
ring slot straight to another; melee Chain Lightning deaths in the ring from 5 toward 0 (tank-to-slot-2 bridges remain);
melee still on Thorim in 98%+ of snapshots and raid damage on him no lower than 0.41-0.47%/s. Gauntlet unchanged: phase
2 in hard mode (`thorim.sifjoined`) in all three, no phase 1 deaths. The pulls still wipe at the mana wall or the ceiling.

## Round 7 (build a55056c40 with round 6)

**Data:** window 13, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261122-205906-4`
(uld1), `-5` (uld2), `-6` (uld3); traces `env/dist/logs/botobs/603_{4,5,6}_thorim_{1795381228,1795381235,1795381236}.ndjson`,
read through `raidtest.py report --boss thorim --raid uld --head a55056c40 --pulls 10` and throwaway scripts (session
scratchpad `thorim_r7/`) over `snap` (hp, position, target and `dealt` of every unit), `note` (`thorim.leverpulled`,
`thorim.balcony`, `thorim.p2start`, `thorim.sifjoined`, `thorim.squad`, `thorim.smashside`), `move` and `act` rows, Sif's
`cast` rows and `death`. Window 12's three pulls (`603_{4,5,6}_thorim_{1795376561,1795376561,1795376560}`) re-read for the
gauntlet. `thorim.squad` notes are missing from uld2/uld3 again; the corridor squad was read off positions.

**Three kills, all normal mode.** Sif cast 62565 at 3:13.5 / 3:17.9 / 3:15.6 in all three pulls (uld1 included: the
orchestrator's note said uld1 was hard mode, it was not), no `thorim.sifjoined`, and Sif dealt no damage; phase 2 came at
3:32.1 / 3:34.9 / 3:24.2. Not polished: no kill counts.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795381228 | kill 6:03, normal | Iron Ring Guard Impale 3 (melee 1:09.5, gauntlet tank 1:11.1, ranged 1:17.6); Runic Smash 1 (the same tank after his soulstone, 1:17.2); Rune Detonation 1 (melee, 2:38.3) |
| uld2 | 1795381235 | kill 5:58, normal | Iron Honor Guard melee 1 (ranged, 2:47.6) |
| uld3 | 1795381236 | kill 5:41, normal | none |

**Round 6 verdict: held.** Bodies of two ring slots within 8 yd in 18.6 / 11.8 / 9.7% of phase 2 snapshots (window 12:
60.7 / 59.9 / 77.9%); no Chain Lightning death and no slot-to-slot hop; melee on Thorim in 100% of snapshots from P2 +15 s to
+100 s. Chain Lightning still crosses from the tanks into a slot (uld2: 80% of its 117k after a crossing, every cast seeded
on a tank), the bridge the pending off-tank spot question is about. Phase 2 was normal mode, so its intake says nothing
about the hard-mode race. The gauntlet half of the check failed: phase 2 missed Sif in all three and phase 1 had 6 deaths.

**Why uld1 had 5 deaths.** After pack 1's Acolyte died (0:49.7), the gauntlet picker's first tier (acolytes within 50 yd)
handed the squad pack 2's Acolyte 35-45 yd south, still out of combat; chasing it pulled pack 2's two Iron Ring Guards
(0:57.5, 0:59.3) with pack 1's two at 50-60%, so four guards lived from 0:59 to 1:11 and Impale killed three bots in 8 s.
The gauntlet tank, at 1-23% health, was moving south toward the Colossus approach while tanking; he died at 1:11.1, took his
soulstone (15%), and the lead walk to the 30 yd Colossus stop (straight `MoveTo`, unwaypointed, no smash check) put him at
(2228.9, −358.7), 7 yd from the y −359 Left Hand bunny row, as the 62058 smash landed (1:17.2). The Rune Detonation death was a
melee on the Giant. The same double pull happened in all six pulls of windows 12 and 13; only uld1 lost bots to it.

**Step timeline** (pack dead = 62507 − 20 s):

| Pull | pack dead | lever | Colossus engaged / dead | lead at Giant | Honor Guards engaged | Giant engaged / dead | `BALCONY_5` | phase 2 | 62565 |
|---|---|---|---|---|---|---|---|---|---|
| uld1 | 0:23.5 | 0:29.6 | 1:26.8 / 2:01.8 | 2:02.2 | 2:11.8, 2:14.7 | 2:23.3 / 3:13.2 | 3:29.8 | 3:32.1 | 3:13.5 |
| uld2 | 0:27.8 | 0:33.0 | 1:47.3 / 2:11.8 | 2:12.4 | 2:18.0, 2:19.1 | 2:28.8 / 3:15.3 | 3:31.7 | 3:34.9 | 3:17.9 |
| uld3 | 0:25.5 | 0:32.1 | 1:37.7 / 2:03.4 | 2:04.2 | 2:10.7, 2:13.5 | 2:22.4 / 3:07.4 | 3:20.8 | 3:24.2 | 3:15.6 |

**Where the gauntlet loses time** (seconds per segment from the lever; the deadline is 62565, 163.5-165.0 s after the lever):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | Giant dead to hit | lever to phase 2 | vs 62565 |
|---|---|---|---|---|---|---|---|---|---|---|
| w13 uld1 | 6.1 | 12.4 | 39.3 | 5.5 | 35.0 | 10.0 | 61.5 | 18.9 | 182.5 | +18.6 |
| w13 uld2 | 5.1 | 16.8 | 40.5 | 17.0 | 24.4 | 6.3 | 57.3 | 19.6 | 181.9 | +16.9 |
| w13 uld3 | 6.6 | 14.3 | 44.3 | 7.0 | 25.7 | 7.3 | 56.7 | 16.8 | 172.1 | +8.6 |
| w12 uld1 | 7.7 | 15.7 | 33.0 | 9.5 | 18.5 | 7.8 | 59.0 | 16.3 | 159.8 | made |
| w12 uld2 | 6.6 | 17.0 | 34.5 | 11.5 | 25.2 | 5.3 | 53.8 | 17.1 | 164.4 | made |
| w12 uld3 | 6.3 | 12.7 | 34.4 | 3.5 | 29.7 | 7.5 | 51.1 | 17.3 | 156.2 | made |

No shared code changed between the two builds' gauntlets; window 13 is the same strategy on worse luck. The fights run well
below what the squad can do: 3.9-4.2 M of corridor health at 23.5-33k average against 40-56k on the Colossus. The Honor
Guard, Acolyte and Giant segment is the largest (51-62 s; the adds alone took 29-42 s for 652k), then the corridor trash
(33-44 s for 1.09 M). Moving-without-damage DPS bot-seconds by the move's source, six pulls of nine DPS bots: Honor Guards +
Giant 580 `thorim gauntlet positioning action`, 233 `reach melee`, 104 `flee`; corridor trash 423 `thorim runic smash
action` (the lane dodge, needed), 180 `reach melee`, 139 positioning; to the Colossus 124 smash, 102 positioning.

**Failure picked: the bot-led gauntlet tether drags DPS off live targets** (the biggest time loss that is a bug).

- Code: `ThorimGauntletPositioningTrigger` (`UldTriggers_Thorim.cpp:45`) and its action's trail branch
  (`UldActions_Thorim.cpp:231`) walk every non-healer to a bot leader's position once more than
  `ULDUAR_THORIM_LEAD_FOLLOW_DISTANCE` (25 yd) away, at `ACTION_RAID`, whatever the bot is fighting. The leader walks on in the
  fight at combat priority: down the corridor while tanking Iron Ring Guards, and up the ramp to the Giant "until he is in
  combat" (round 3), leaving the Honor Guards and the Acolyte on the DPS and pets at the ramp foot.
- Trace: between the first trash pull and the Giant's death, 517 tether orders went to a DPS bot holding a live gauntlet
  target, 443 of them from this branch (57-91 a pull; the lane formation branch 74); 279 of the 517 sent a melee or ranged
  bot farther from its target and out of its reach. uld1, melee Uldxat on the ramp Acolyte at 1-5 yd:
  tethered toward the leader at 2:18.8, 2:19.5, 2:21.3, 2:22.0, 2:22.6, 2:26.6, each answered by `set behind` or `reach
  melee`; ranged Uldxaq on the foot Honor Guard at 6-8 yd sent 67 and 94 yd up the ramp (2:22.6, 2:31.2). That Honor Guard,
  held by a hunter pet, went 83% → 16% in 30 s (2:17-2:47, ~7k dps). Per pull, 116-185 DPS bot-seconds moving without
  damage behind the tether and the reach back to the target, three quarters of it on the Honor Guards and Giant.
- The boss doc already says a tether on non-healers "would fight `reach melee`"; the bot-leader branch did exactly that.

**Classification: bug.** The tether is meant to keep the squad with the leader between fights, not to pull it off the mob it
is killing.

**Fix** (Thorim files only, no size or mode branch): `ThorimGauntletFighting` (`UldEncounter_Thorim.cpp:1435`) is true while
the bot's current target is an Iron Ring or Honor Guard, a gauntlet Acolyte (33110), the Colossus or the Giant, alive, hostile,
in combat and within the 50 yd pick range. `ThorimTrailsGauntletLeader` (`:1457`) holds the 25 yd trail for a non-healer
while it fights; healers keep their 20 yd tether, and the gate walk and the lane formation are unchanged. Trigger and action
share it. With nothing left in range the tether walks the bot to the leader as before, where the picker hands it his target.
A real player leading changes nothing: non-healers never trailed a human leader.

**Review:** that bot-leader-only trail (`ThorimGauntletLeaderIsBot`, now removed) was a bot-only branch, and its one reason,
a tether fighting `reach melee`, is what the fighting gate removes. Non-healers now trail a real player leader the same way,
one code path for every raid; bot-led pulls are unchanged. Replayed on the six pulls, the gate holds ~400 of the 443 trail
orders sent to a DPS with a live gauntlet target; the rest had a mob at full health with no target (23) or past 50 yd (19).

**Not fixed, measured** (all six pulls):
- **The squad is pinned at the last waypoint while the leader waits at his 30 yd Colossus stop.** The Runic Smash dodge keeps
  followers swapping between the two last waypoints (y −329), 55-65 yd from the Colossus, outside the 50 yd pick range, until
  one drifts in: leader at the stop to engage 15.0 (he arrived mid-trash) / ~9 / 4.8 s (w13), 3.9 / 4.2 / 1.7 s (w12). Question 3.
- **Generic `flee` paths below the corridor floor** in the right lane: a 4-5 yd ranged or healer flee west at the ramp foot
  (2217, −423) sank 6 / 1 bots (w13 uld1/uld3) and 7 / 2 (w12) to z ~390 and back over 6.5-7 s; at (2219, −354) one ranged
  went to z 264 for 56 s (w13 uld1), outside the fall rescue's 110 yd. Needs navprobe; next-round candidate (hold `flee` for
  corridor bots there, or fix the spot).
- **The Runic Smash latch is still cleared by arena scans** (round 6's watch item): `thorim.smashside` flips to 0 from arena
  bots about every second. Its only smash death this window was on the unwaypointed approach, where the lane logic does not
  run, so it stays unfixed.

**Stop rules:** not polished (three normal-mode kills). Not stuck: rounds 3-7 picked five different causes; each fix held.

**Questions for the user** (new tactics, not built; seconds per pull from these traces):
1. **A balcony runner.** One corridor ranged DPS starts the balcony walk once no Honor Guard or Acolyte lives and the Giant is
   under ~70%, so it stands at `BALCONY_5` when he dies and starts phase 2 at once. The walk from his death to the hit took
   16.8-19.6 s (w13) and 16.3-17.3 s (w12); the runner's lost damage on the Giant is ~1.5-2 s. Saves ~12-14 s. Recommended: yes.
2. **The gauntlet tank waits at the lever** once the main tank has the start pack, instead of walking to it after the sixth
   death: the lever went 5.1-6.6 s (w13) and 6.3-7.7 s (w12) after that death, Sif's clock having started at it. Saves ~5-6 s.
   Recommended: yes.
3. **The gauntlet tank pulls the Colossus himself** at his 30 yd stop once no corridor trash lives near him (the smash stops at
   the engage and the squad's tether brings it in), instead of waiting for a follower to drift inside 50 yd. Saves 2-9 s,
   ~4 s on average. Recommended: yes.
The pending off-tank spot question (2107.25, −252.65) is unchanged and not built. Window 13 missed the deadline by 8.6 / 16.9 /
18.6 s; the fix (est. 6-10 s) plus questions 1 and 2 would cover all three.

**Next window should show** (`move` rows by `thorim gauntlet positioning action`, `snap` target and `dealt`, segment times):
non-healer trail orders (to the leader's position) issued while the bot's target is a live, in-combat gauntlet mob within
50 yd near 0 (was 57-91 a pull); DPS bot-seconds moving without damage behind the tether and the reach back from 116-185 a pull
toward ~20; the Honor Guard + Acolyte + Giant segment from 51-62 s toward ~45 s and the corridor trash no longer; lever to phase
2 under the 163-165 s budget on the faster pulls, so `thorim.sifjoined` in at least one. Watch: DPS finishing the ramp-foot adds
without healers (healers still trail the leader to the Giant), so Honor Guard or Acolyte deaths at the foot; bots idle there
with no target for more than ~3 s after their adds die (the tether should walk them up); longer lead stalls in the corridor
(the leader waits up to 15 s a step while DPS finish trash).

**User answers (2026-11-22):** Q1: keep polishing on the 226 rosters with the stop rule waived. Q2: move
the off-tank spot to (2107.25, -252.65), navprobed on the floor at z 419.371 with a straight path from the
tank spot, then compare before and after, since boss positioning affects other things. Only the waiting
tank stands there (the holder takes the anchor), 3.5 yd from it like today, so a trade stays inside the
ring's 5 yd reposition tolerance.

## Round 8 (builds the user's Q2 answer; round 7 not judged here)

**Data:** the "before" numbers come from window 12 (`603_{4,5,6}_thorim_{1795376561,1795376561,1795376560}`, build ebe746ad1,
hard mode, round 5 ring code) and window 13 (`603_{4,5,6}_thorim_{1795381228,1795381235,1795381236}`, build a55056c40, normal
mode, round 6 ring code), read with the new reader `tools/botobs/bosses/thorim.py --phase2` and replays in session scratchpad
`thorim_r8/` (`replay.py`, `bliz.py`, `hazards.py`, `spots.py`). Window 14's three Thorim pulls landed while this round ran
(`tr-20261123-095334-{4,5,6}`, traces `603_{3,5,6}_thorim_{1795427707,1795427695,1795427705}`: uld1 a hard-mode wipe at 5:55, 32
deaths, phase 2 at 3:18.2; uld2 and uld3 normal-mode kills with 2 and 1 phase 1 deaths). They run the same phase 2 code as
window 15 minus this change, so they are the closest "before" and get their own column; round 7's verdict, the stop rules
and the ranking are left to the round that reads window 14's report. Deaths by mechanic: rounds 6 and 7's tables, and
window 14 uld1: phase 2 Thorim melee on a non-tank 22, on a tank 3, Blizzard 2, Chain Lightning 2, Lightning Charge 2,
Frostbolt Volley 1; uld2 Iron Ring Guard Impale 1, Whirling Trip 1; uld3 Iron Ring Guard melee 1.

**Change** (`UldEncounter_Thorim.cpp`, Thorim only, no size, mode or player branch):
- `ULDUAR_THORIM_PHASE2_OFFTANK_SPOT` (2114.25, −252.65, 419.508) → **(2107.25, −252.65, 419.371)**, 3.5 yd west of the tank
  spot instead of east, navprobed by the orchestrator (floor, straight path from the tank spot).
- **Bug found building it:** `TryGetThorimPhase2Spot` (`:2515`) sent the anchor tank (`MainTank` role, latched for the phase)
  to the tank spot even while the other tank held Thorim, so after every trade that took him off the anchor tank both tanks
  stood on the tank spot: the waiting tank stood within 2 yd of the holder for **40-58%** of its waiting time in all nine
  pulls of windows 12-14, and the off-tank spot was used about half the time. The doc and the user's answer both say every waiting tank takes
  it. Now a tank he is hitting takes the tank spot, any other the off-tank spot while a tank holds him, and the anchor tank
  keeps the tank spot only while no tank holds him (its pickup, unchanged). A human tank holding him makes the bot tank the
  waiting one (`PlayerbotAI::IsTank` reads a human by spec); no human is moved.

**Every other user of the old spot, and what moves** (Thorim rests 4.8-7.4 yd south-east of the tank spot, bearing −37° to
−61°, in all six pulls):

| User | Old spot | New spot | Moves |
|---|---|---|---|
| Code: positioning trigger/action, `ThorimTankOnAnchor` (tank anchor guard) | only reader of the constant | same | the waiting tank's point, 7 yd west |
| Melee ring (`SolveRingBearings` off Thorim's live position and the tank-spot bearing) | not an input | not an input | nothing in the solve; `RingBodyBridgesChain` counts the waiting tank where it stands, so slot 2's bridge escapes should fire less |
| Static melee fallback spots | MELEE1 4.5 yd, MELEE2/3 8.7 | 11.5 / 8.7 | no |
| Ranged homes, shelters, opening spots | nearest 18.9 (RANGE4, RANGE6) | nearest 21.7 (OPENING1), 22.9 (orb 2 slot 2 shelter) | no |
| Chain spread: a non-tank body (players and pets) within 8 yd of the spot, settled P2 snapshots, replayed at fixed points | 84-100%; slot 2 67-100% | 18-36%, w13 uld1 88%; slot 2 0-46% | the target |
| Sif's Blizzard track (`boss_thorim_sif_blizzard` waypoints) | 7.2 yd off the path | **1.0 yd** (leg (2104.3, −256.3) → (2109.3, −246.4)); tank spot 4.1 | replayed zone drops (one per 2 s along the traced bunny, 10 s life, 9.8 yd reach), hard-mode ticks a pull: old 46/30/50, new 55/36/61 = the tank spot's |
| Sif's Frost Nova (teleport box x 2108-2150, y −284..−238) | inside; her floor spots 2.5-8.0 yd at nearest | outside; 8.1-9.8 | fewer novas on the waiting tank |
| Arena tether and scan box (phase 1 only: `ThorimArenaLeashBreached` returns early outside the split) | 23.2 yd from the centre | 29.65 yd, inside the 30 yd leash and the box | no |
| Thorim's melee reach for the waiting tank | 2.9-5.7 yd from his centre | 7.1-10.2; past 9.0 yd (where `reach melee` fires: 0.75 + 0.5 contact + 1.5 + 6.25) in 16/17/99.6% (w12) and 0/0/87% (w13) of snapshots | the waiting tank steps up to ~1.7 yd in (reach is exempt from the tank anchor guard); the positioning row only walks it back past 2.5 yd exact (`GetDistance` nets its 1.5 reach), so no tug-of-war expected |
| Thorim's position and facing | holder always on the tank spot; facing p50 0.0-0.2° off the boss→tank-spot axis | unchanged for the holder | during a taunt from the off-tank spot he faces ~19° off the axis instead of ~47°, until the taunter reaches the tank spot |

**Before/after measures** (`thorim.py <trace> --phase2`, one line each; settled = from P2 +15 s, held by a tank; "waiting" =
any living floor tank he is not hitting). Window 12 is hard mode on round 5's ring code, window 13 normal mode on round 6's,
window 14 the current code (uld1 hard, uld2/uld3 normal): compare a window 15 pull with window 14 first, then the same mode.

| Measure (reader line) | w12 uld1 / uld2 / uld3 (hard) | w13 uld1 / uld2 / uld3 (normal) | w14 uld1 (hard) / uld2 / uld3 (normal) | After should |
|---|---|---|---|---|
| CL casts; hits on players | 12, 32 / 5, 15 / 11, 31 | 6, 22 / 5, 19 / 7, 21 | 7, 27 / 9, 23 / 7, 20 | same |
| CL hits on tanks (damage) | 7 (77k) / 0 / 9 (44k) | 4 (13k) / 6 (13k) / 2 (6k) | 2 (26k) / 0 / 2 (23k) | about the same (the pair still bridges) |
| CL hits on slot 0/1/2; slot 2 damage | 0/0/6, 72k / 3/3/0, 0 / 3/3/9, 112k | 0/0/4, 26k / 3/2/3, 39k / 4/3/0, 0 | 3/5/3, 29k / 6/3/0, 0 / 0/3/3, 15k | slot 2 hits and damage down |
| CL tank-to-ring hops; CL deaths | 2, 3 / 0, 3 / 2, 3 | 2, 0 / 3, 0 / 0, 0 | 1, 2 / 0, 0 / 0, 0 | hops toward 0 |
| Tank deaths, first at | 5 +118.6 s / 4 +106.9 / 4 +190.0 | none | 4 +82.4 s / none / none | no earlier |
| Thorim off the tank spot p50 | 6.6 / 4.8 / 6.2 yd | 4.8 / 4.8 / 7.4 | 7.2 / 4.8 / 7.5 | unchanged |
| Facing off the axis p50; share over 20° | 0.2°, 3.5% / 0.2°, 2.3% / 0.2°, 2.4% | 0.0°, 1.1% / 0.0°, 1.4% / 0.2°, 1.6% | 0.2°, 3.3% / 0.0°, 2.3% / 0.2°, 2.0% | unchanged |
| Trades; Thorim moved across one p50 / p90 | 13, 0.0/0.0 / 6, 0.0/13.8 / 12, 0.0/2.1 yd | 6, 0.0/0.0 / 6, 0.0/0.0 / 9, 0.0/2.6 | 6, 0.0/0.0 / 8, 0.0/0.0 / 7, 0.0/2.6 | p50 0, p90 no higher (a taunt from 7-10 yd may step him toward the taunter) |
| Strike to swap p50 / max; time on a non-tank with a tank up | 0.3/3.6 s, 5.0 s / 0.5/6.4, 2.1 / 0.3/2.4, 1.9 | 0.3/1.5, 3.4 / 0.2/3.1, 0.3 / 0.2/3.1, 0.0 | 0.2/3.2 (1 unanswered), 2.0 / 0.3/0.7, 1.7 / 0.2/0.3, 0.2 | no slower |
| Waiting tank offset from the tank spot (median); on the holder | (+3.5, 0) 40.0% / (0, 0) 58.0% / (0, 0) 52.9% | (+1.5, 0) 43.6% / (+3.5, 0) 41.6% / (+1.8, 0) 40.2% | (0, 0) 51.3% / (0, 0) 52.4% / (+1.9, 0) 42.8% | (−3.5, 0) or up to ~1.7 yd toward him; on the holder ~0% |
| Waiting tank from Thorim p50; past 9.0 yd | 5.7, 5.3% / 4.8, 0.5% / 6.2, 0.3% | 4.4, 0% / 3.2, 0% / 5.6, 0% | 7.2, 4.1% / 4.8, 0% / 5.5, 0% | 7-10 yd; past 9.0 where he rests far |
| Waiting tank moves (accepted): positioning, reach melee | 12, 1 / 2, 0 / 5, 2 | 1, 0 / 2, 0 / 3, 0 | 4, 1 / 3, 0 / 2, 0 | positioning up by the trade walks only; reach melee some; **20+ positioning a pull = tug-of-war** |
| Waiting tank within 8 yd of a non-tank body / ring body / slot 2 | 94.5 / 44.9 / 44.9% · 100 / 65.9 / 53.7% · 77.4 / 52.9 / 52.9% | 100 / 99.6 / 98.7% · 97.3 / 52.9 / 52.9% · 82.7 / 39.2 / 39.2% | 91.6 / 46.3 / 46.3% · 100 / 20.0 / 13.9% · 55.9 / 47.7 / 44.1% | non-tank ~20-40%, slot 2 under ~10% |
| Holder within 8 yd of a ring body | 19.5 / 33.8 / 18.5% | 99.6 / 5.3 / 13.0% | 34.9 / 10.5 / 18.0% | unchanged (the holder's spot did not move) |
| Ring: in his front arc; two slots within 8 yd; on Thorim to +100 s; moves per ring bot a minute | 23.2%, 62.3%, 100%, 4.7 / 25.4%, 60.3%, 100%, 4.3 / 31.8%, 83.5%, 100%, 4.0 | 24.1%, 16.1%, 100%, 3.2 / 31.9%, 8.0%, 100%, 2.0 / 58.2%, 5.2%, 100%, 2.7 | 31.6%, 27.3%, 99.8%, 7.4 / 18.8%, 4.3%, 100%, 3.3 / 30.1%, 16.0%, 100%, 2.7 | front arc and uptime unchanged; moves a little down |
| Camp off Thorim p50; a camp body within 8 yd of a tank | 25.3, 6.3% / 25.7, 1.1% / 25.4, 0.0% | 26.7, 1.8% / 26.7, 0.4% / 24.2, 2.3% | 24.7, 0.9% / 25.8, 0.7% / 24.1, 0.0% | unchanged |
| Tank hazards holder / waiting: Blizzard hits (damage) | 28 (86k) / 35 (120k) · 29 (96k) / 17 (63k) · 37 (145k) / 40 (143k) | none (normal mode) | 27 (99k) / 18 (36k) · none · none | hard mode: waiting about the holder's (replay +6-11 ticks a pull over the old spot) |
| Lightning Charge, Frost Nova hits holder / waiting | LC 1/4, FN 0/1 · LC 0/0, FN 1/1 · LC 2/1, FN 1/1 | LC 1/1 · 2/3 · 0/2, FN 0 | LC 0/1, FN 1/0 · LC 0/1 · LC 1/1 | Frost Nova on the waiting tank no higher; LC read only |
| Thorim health lost | 0.45 / 0.42 / 0.41 %/s | 0.66 / 0.70 / 0.73 | 0.42 / 0.65 / 0.70 | no lower in the same mode |

**Watch in window 15:** the waiting tank on the old line `(+3.5, 0)` or "on the holder" above a few percent means the build
missed the change; positioning moves by the waiting tank past ~20 a pull mean `reach melee` and the positioning row fight
over it (then hold Reach for a waiting tank on its spot); a hard-mode pull is needed for the Blizzard and Frost Nova lines.
Round 7 (gauntlet) is judged on window 14 by the next round.

**Review:** the positioning action backed the anchor tank (`MainTank` role) into every spot, so after its taunt from the
off-tank spot it walked the 3.5 yd east to the tank spot backward, its back to Thorim, without parry or block for the walk
(the old east spot cost the off-tank the same, walking west forward). Now the tank he hits backs in only when its spot lies
away from him (`ThorimPhase2PositioningAction::Execute`); every other move is forward. The root cause holds in five of the
nine pulls read per snapshot with `thorim.p2role`: the anchor tank waiting on an off-tank holder in 207-363 snapshots a
pull, the reverse in 3-28.

**Stop rules:** not checked this round (window 14 belongs to the next one). Not stuck: no failure was picked.

## Round 9 (window 14: build 1fa3c332c with round 7 and the shared PickDefensive fix; round 8 merged, not built)

**Data:** window 14, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-095334-4`
(uld1), `-5` (uld2), `-6` (uld3); traces `env/dist/logs/botobs/603_{3,5,6}_thorim_{1795427707,1795427695,1795427705}.ndjson`,
read through `raidtest.py report --boss thorim --raid uld --head 1fa3c332c --pulls 10`, `thorim.py --deaths --phase2`,
`postmortem.py` (deaths, `--bot` timelines) and scripts in session scratchpad `thorim_r7/` (`seg.py`, `budget.py`, `branch.py`,
`tetherloss.py`) and `thorim_r9/` (`perbot_seg.py`, `movesrc.py`, `corr_targets.py`, `loose.py`, `pack2.py`, `replay_hold.py`,
`p2heal.py`, `bliz_heal.py`) over `snap`, `move`, `act`, `heal`, `dmg`, Sif's `cast` rows (`"sp":62565`) and `note`
(`thorim.leverpulled`, `thorim.p2start`, `thorim.sifjoined`). Windows 12 and 13 (six pulls) re-read for the corridor.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795427707 | wipe 5:55, **hard mode** (phase 2 3:18.2, no 62565 cast) | phase 2: Thorim melee on a non-tank 22, on a tank 3, Blizzard 2 (the holy paladin twice), Chain Lightning 2, Lightning Charge 2, Frostbolt Volley 1 (a tank) |
| uld2 | 1795427695 | kill 6:30, normal (62565 at 3:16.6, phase 2 3:56.3) | Iron Ring Guard: Whirling Trip 1 (ranged, 1:15.2), Impale 1 (ranged, 1:22.3) |
| uld3 | 1795427705 | kill 6:06, normal (62565 at 3:18.2, phase 2 3:42.6) | Iron Ring Guard melee 1 (ranged, 1:31.2) |

**Step timeline and segments** (from the lever; budget lever → 62565 163.7 s; uld1's deadline 62507 + 150 s = 3:19.3):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs 62565 |
|---|---|---|---|---|---|---|---|---|---|---|
| uld1 | 4.5 | 12.2 | 46.0 | 3.2 | 24.3 | 7.7 | 55.8 | 15.2 | 164.4 | made by 1.1 |
| uld2 | 6.3 | 14.4 | 48.0 | 13.5 | 40.3 | 6.5 | 62.2 | 18.6 | 203.5 | +39.7 |
| uld3 | 6.4 | 14.5 | 46.2 | 12.3 | 30.4 | 7.3 | 57.0 | 20.2 | 188.1 | +24.4 |

**Round 7 verdict: the mechanic stopped, no time came back.** Trail orders sent to a DPS bot holding a live gauntlet target
88 / 57 / 91 a pull (window 13) → 8 / 5 / 4; DPS bot-seconds moving without damage behind the tether and the reach back 185 / 144 /
116 → 42 / 27 / 55. But the Honor Guard + Acolyte + Giant segment stayed at 56-62 s (target ~45), DPS walking up the ramp with no
target grew 19 / 12 / 15 → 72 / 58 / 69 bot-seconds, and the corridor trash got longer, 39-44 s → 46-48 s (window 12: 33-34.5 s),
at 22-25k squad dps against 31-33k in window 12. Lever to phase 2 made the budget in one pull (uld1, by ~1 s), as asked.

**Why uld2 and uld3 missed Sif's clock** (against uld1): uld2 +39.1 s = Colossus +16.0, to Colossus +10.3, adds and Giant +6.4,
balcony +3.4, the rest +3.0; uld3 +23.7 s = to Colossus +9.1, Colossus +6.1, balcony +5.0, the rest +3.5.
- The Colossus is dead DPS from the corridor: 42.8k with all nine DPS (uld1), 33.6k with eight (uld3, a warlock dead), 25.1k with
  seven (uld2, the balance druid and a warlock dead); per-bot output and movement on him are otherwise alike.
- To the Colossus: the squad stood 12-13.5 s with no target while the leader waited at his 30 yd stop (round 7's "pinned at the
  last waypoint", round 7 question 3). Balcony 15-20 s (question 1).

**Why Thorim's melee reached 22 non-tanks in uld1:** every one died after both tanks were down (5:28.8, P2 +130.6 s); with a tank
up he spent 2.0 s on a non-tank all phase. The first tank death (blood DK Uldxam, +82.4 s) came with healer mana at 46.6%, not
the mana wall (window 12: 5-33% at +107-190 s). Thorim lost 0.42%/s, the same as window 12's hard-mode pulls (0.41-0.45). What
differed: the holy paladin Uldxab died at +80.6 s and +119.4 s, 1.8 s and 0.4 s before a tank death, to Blizzard on top of a
Volley (11.7k at full health, then two 5.6-5.9k ticks); healers took 30k of Blizzard in uld1 against 0 in all three window 12
hard-mode pulls. At +80.6 s her escape (`ThorimCampBlizzardEscape`, `UldEncounter_Thorim.cpp:2710` at c350347ff) walked from a
zone dropped 6.5 yd from her, slowed by the Volley, past the next zone's edge (8.7 yd): it tests the end point clear by 11 yd,
not the walk. Chain Lightning one-shot two melee stacked 0.4 yd apart at +72 s (28.3k, 37.7k). The DK tank cast Death Strike 5
times in the 82 s before his first death against 17 Icy Touches spending the Frost rune (class rotation, shared, not touched).
**Classification:** the hard-mode race round 6 described, on one hard-mode pull. Not picked. Candidate if a second hard-mode
pull shows a healer Blizzard death: score the escape walk with `ThorimWalkUnderBlizzard`.

**Stop rules:** not polished (two normal-mode kills). Not stuck: rounds 3-9 picked seven different causes.

**Failure ranking:** the uld1 phase 2 wipe 32 (the race, above); corridor Iron Ring Guards 3 (and the DPS they took from the rest
of the gauntlet); no other phase 1 death.

**Failure picked: the bot-led corridor fights both trash packs at once** (all nine 25-man pulls of windows 12-14).

- Pack 2 (Acolyte (2227.5, −345.4), guards (2217.7, −337.4) and (2235.3, −338.3)) started moving 6-16 s after pack 1, with both of
  pack 1's guards up, every pull. Three paths opened it, whichever came first:
  - **The picker** (first in four pulls: w13 uld1, uld3, w14 uld2, uld3). `GetThorimDpsTarget`'s gauntlet tiers
    (`UldEncounter_Thorim.cpp:1200-1206` at c350347ff) take any Acolyte in the 50 yd sweep, idle or not; a DPS took pack 2's Acolyte
    from 41-49 yd within 1 s of pack 1's Acolyte dying (w14 uld2 0:57.77 Uldyax at 48 yd, pack 2 moved 1:00.3).
  - **The leader** (first in four: w12 ×3, w14 uld1). `ThorimGauntletLeadStep` (`:1508` at c350347ff) steps on whenever the squad is
    within 30 yd, fight or not: all 63 lead moves between the first corridor engage and the last guard's death came with engaged
    trash up, 52 of them with the leader's own target engaged. The right lane's fifth waypoint (2212.1, −318.2) is 20.0 yd from pack
    2's west guard: w14 uld1's pack 2 moved at 0:55.3, the leader 20.4 yd away, before any bot targeted it.
  - **A Disengage** (w13 uld2). Hunter Uldyau, his target pack 1's dying Acolyte, cast Disengage at 1:03.96 and stood 22.3 yd
    from pack 2's east guard when pack 2 turned on him at 1:04.82, with no bot targeting pack 2 and the leader 33-40 yd off.
    Neither half of the fix below holds it.
- Effect: the tank-leader walks south onto pack 2 and pack 1's guards stay on ranged and pets. Iron Ring Guard seconds on non-tank
  players 22 / 88 / 51 (w14; w12-13 28-71); w14 uld3: pack 1's guards sat on a voidwalker and a hunter 0:58-1:30 while the tank held
  pack 2, then killed the warlock; uld2: one pack 1 guard on the warlock Uldyav for 30 s (0:52.8-1:22.3) until Impale killed him,
  pack 2's guards on the druid Uldyas until Whirling Trip did. Window 13 uld1's three Impale deaths were the same four-guard window.
  Round 7 likely made it worse: DPS no longer trail the leader mid-fight, so the fight no longer moves with the tank (corridor 39-44
  → 46-48 s, guard seconds on ranged 20-32 → 20-58).
- Core: `boss_thorim_gauntlet_npcs` is a plain `ScriptedAI`, out of combat until aggro or damage; `JustEngagedWith`
  `CallForHelp(25)`, and the packs stand ~40 yd apart, so only bots join them.

**Classification: bug.** The corridor squad and its leader are meant to clear the corridor pack by pack, like round 7's tether is
meant to move the squad between fights; neither a picker handing out the next pack nor a tank walking off the guards he holds is
part of the strategy.

**Fix** (`UldEncounter_Thorim.cpp`, Thorim only, no size, mode or player branch):
- `DropIdleGauntletMobsWhileFighting` (`:1182`, called in `GetThorimDpsTarget` `:1235`): while any corridor mob (`IsThorimGauntletMob`,
  `:1167`: Iron Ring and Honor Guards, gauntlet Acolyte 33110, Colossus, Giant) in the bot's 50 yd sweep is in combat, the idle ones
  are dropped from the gauntlet picks. With nothing engaged the picker opens the next pack as before, so the walk-in, the Colossus and
  the Giant open unchanged. The arena entries and the start pack are not filtered.
- `ThorimGauntletLeaderFighting` (`:1423`): the leader's target is an engaged corridor mob within 50 yd (`ThorimGauntletFighting`, round
  7) or a corridor mob attacks him. `ThorimGauntletLeadStep` (`:1581`) and `ThorimGauntletLeadColossus` (`:1607`) hold while it is true;
  the waypoint bookkeeping before it still runs, the Runic Smash lane dodge is a separate row and still moves him, and the Giant walk is
  not held (the Giant spawns Honor Guards until he is pulled). Replayed (lever to Colossus): the own-target half holds 52 of the 61 lead moves made with an engaged corridor mob within 50 yd;
  most of the rest had a guard at 100% (just aggroed) as target or one on him.
- A real player leading changes nothing for the lead (the lead nodes run only for a bot leader); the picker runs the same for every
  corridor bot behind any leader.

**Next window should show** (`pack2.py`, `loose.py`, `replay_hold.py`, `perbot_seg.py`, `budget.py` on the new traces):
- pack 2's first movement after pack 1's last death in every pull, and no bot targeting an idle corridor mob while another is
  engaged in its 50 yd;
- no `thorim gauntlet lead action` move while the leader's target is an engaged corridor mob (52 over nine pulls before);
- Iron Ring Guard seconds on non-tank players from 22-88 toward ~20 a pull, and no Iron Ring Guard death (3);
- all nine corridor DPS alive at the Colossus, so the Colossus in ~24-30 s; the corridor itself no longer than 46-48 s (serial packs
  add a walk between them but lose the loose guards);
- still open: the 12-13.5 s wait at the Colossus stop and the 15-20 s balcony walk (questions 1 and 3), so hard mode only on the
  faster pulls. Round 8's phase 2 measures land in the same window (its table above).
- Watch: a leader holding more than ~15 s on one step while nothing dies (a guard kited out of reach), and pack 2 now opened by DPS
  from 39-50 yd once pack 1 is dead rather than by the tank. A bot's own escape to ~22 yd of pack 2 (Disengage, w13 uld2) still
  opens it early.

**Questions for the user:** none new. Round 7's three (balcony runner, the gauntlet tank waiting at the lever, the gauntlet tank pulling
the Colossus) stand, recommended yes; this window adds 12.3 / 13.5 s at the Colossus stop (uld3 / uld2, 3.2 s in uld1) and balcony
walks of 15.2-20.2 s.

**User answers (2026-11-23) to round 7's questions:** Q1 (balcony runner): no. Starting phase 2 while the gauntlet squad
is still deep in the corridor is a recipe for disaster; the gauntlet clear itself has to get faster. Q2 (the gauntlet
tank waits at the lever while the start pack dies): yes. Q3 (the gauntlet tank pulls the Colossus from his 30 yd stop
once the corridor trash is dead): yes. Round 10 builds Q2 and Q3 and works on the user's direction: optimize how the
gauntlet is cleared.

## Round 10 (window 15: build 523b2f60f with rounds 8 and 9; window 15a: build c350347ff, round 8 only)

**Data:** window 15, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-122603-8`
(uld1), `tr-20261123-122522-7` (uld2), `tr-20261123-122644-9` (uld3); traces
`env/dist/logs/botobs/603_{8,7,9}_thorim_{1795436805,1795436765,1795436846}.ndjson`. Window 15a: `tr-20261123-112654-{1,2,3}`,
traces `603_{1,2,3}_thorim_{1795433300,1795433289,1795433304}`. Read through `raidtest.py report --boss thorim --raid uld --head
523b2f60f --pulls 10` (window 15a's report from the orchestrator), `thorim.py --phase2`, `postmortem.py --bot`/`--probes`, round 7
and 9 scripts (`budget.py`, `seg.py`, `dps.py`, `perbot_seg.py`, `movesrc.py`, `loose.py`, `pack2.py`) and new ones in session
scratchpad `thorim_r10/` (`startpack.py`, `notarget.py`, `tethergap.py`, `leadstop.py`, `smashlatch.py`, `smashcost.py`,
`dodgeundo.py`, `smashhits.py`, `waittank.py`, `holderpos.py`, `colaggro.py`) over `snap`, `move`, `act`, `dmg` (62465), `haz` (62057/62058),
`note` (`thorim.smashside`, `thorim.leverpulled`, `thorim.p2start`, `thorim.p2role`, `thorim.anchortank`) and Sif's `cast` rows.

| Window | Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|---|
| 15 | uld1 | 1795436805 | wipe 5:46, **hard mode** (phase 2 3:09.1, deadline 62507 + 150 s = 3:14.7) | phase 1: Rune Giant Stomp 1 (the gauntlet tank, 2:27.1); phase 2: Thorim melee on a non-tank 17, on a tank 4, Chain Lightning 4 (one cast at 5:06.2 after the tanks fell), Lightning Charge 4, Blizzard 2, Sif Frostbolt 1 |
| 15 | uld2 | 1795436765 | kill 5:54, normal (62565 3:16.6, phase 2 3:33.0) | none |
| 15 | uld3 | 1795436846 | kill 5:37, normal (62565 3:16.0, phase 2 3:20.9) | Runic Colossus Smash 62339 1 (melee, 1:58.1) |
| 15a | uld1 | 1795433300 | kill 6:40, normal (phase 2 3:35.6) | Iron Ring Guard Impale 1, melee 1 (1:06-1:16); Runic Smash 2 (one cast, 1:20.7); Giant Stomp 1, Rune Detonation 1 (3:13-3:17); Chain Lightning 2 (4:02.6) |
| 15a | uld2 | 1795433289 | kill 6:05, normal (phase 2 3:35.9) | Iron Ring Guard melee 1 (1:12.2); Chain Lightning 3 |
| 15a | uld3 | 1795433304 | kill 5:47, normal (phase 2 3:25.8) | Iron Ring Guard Whirling Trip 1 (1:15.1); arena Champion Mortal Strike 1 (2:22.8); Iron Honor Guard Shield Smash 1 (2:27.4) |

**Round 8 verdict (off-tank spot): held in four of six pulls, one gap.** The waiting tank stood at (−3.5, 0) and on the holder
1.2-1.5% (w15 uld1, uld2; w15a uld1; windows 12-14: 40-58%). In w15 uld3 (50.8%) and w15a uld2 (30.0%; w15a uld3 14.7%) the
anchor tank, holding Thorim again after its taunt from the off-tank spot, stopped 2.1-2.3 yd short of the tank spot on the
off-tank side ((−1.9, −0.9) / (−2.2, −0.7) in every snapshot it held), inside the tanks' arrival test (`GetDistance` ≤ 1 yd,
2.5 yd exact), so the waiting tank on its spot stood within 2 yd of it; the other tank's holds sat on (0, 0). Other lines:
trades moved him p50 0 yd; strike to swap p50 0.2-0.4 s; waiting-tank moves 4-9 a pull (no tug-of-war); tank-to-ring chain
hops 0-2; Thorim lost 0.54-0.74 %/s in normal mode (w13: 0.66-0.73). Hard mode (w15 uld1): Blizzard on the waiting tank 45 hits
(160k) against the holder's 21 (75k) (w14 uld1: 18 (36k) / 27 (99k)), the cost of the new spot's 1 yd clearance from the
Blizzard track; first tank death at +103.1 s (w14 +82.4). Candidate next round: the holder stopping short (a bug: round 8
means the holder on the tank spot).

**Round 9 verdict (corridor packs one at a time): held.** Pack 2 first moved 3.5-4.1 s after pack 1's last death in all three
pulls (before: with pack 1's guards up in nine of nine); no lead move while the leader's target was an engaged corridor mob (52
over nine pulls before); Iron Ring Guard seconds on non-tank players 2 / 19 / 6 (w14 22 / 88 / 51); no Iron Ring Guard death (w14:
3). Corridor 43.0 / 47.2 / 36.7 s (w14 46-48).

**Stop rules:** not polished (the one hard-mode phase 2 wiped). Not stuck: rounds 3-10 picked eight different causes.

**Gauntlet timeline** (seconds per segment from the lever; budget lever → 62565 163.2-164.8 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w15 uld1 | 5.3 | 12.6 | 43.0 | 16.0 | 19.5 | 7.5 | 43.2 | 17.4 | 159.2 | made by 5.6 |
| w15 uld2 | 6.4 | 14.2 | 47.2 | 9.7 | 21.2 | 7.3 | 61.8 | 18.7 | 180.0 | +16.4 |
| w15 uld3 | 6.5 | 11.8 | 36.7 | 16.2 | 28.8 | 7.2 | 52.3 | 15.4 | 168.5 | +4.9 |
| w15a uld1 | 6.1 | 12.6 | 44.5 | 3.7 | 31.4 | 7.6 | 62.7 | 18.6 | 181.1 | +17.1 |
| w15a uld2 | 6.8 | 14.6 | 39.0 | 8.3 | 35.2 | 7.5 | 57.3 | 20.1 | 182.0 | +18.8 |
| w15a uld3 | 5.2 | 12.6 | 51.8 | 2.0 | 25.7 | 7.8 | 54.0 | 17.3 | 171.2 | +6.4 |
| fastest on record per segment | 4.5 | 11.8 | 33.0 | 2.0 | 18.5 | 5.3 | 43.2 | 15.2 | 156.2 (w12 uld3) | |

**Where the seconds go** (window 15, against the fastest segments):
1. **Honor Guards, Acolyte and Giant, 43-62 s.** The leader walks past the adds and pulls the Giant at his spawn ("all the way in",
   round 3: the Giant spawns an Honor Guard 20 s after the first one engages, then every 10 s until he is pulled), and holds him
   alone 4.5 / 19.5 / 9.8 s while the squad kills the adds 40-60 yd down the ramp; then the squad walks up, 5-8 s with almost no
   damage, and the Giant dies 22-25 s after it arrives. Ranged had no target 19-32% of the segment; 10-14 tether orders a pull sent a
   DPS between targets toward the leader at the Giant while an engaged add was within 50 yd, most when the next pick was out of
   sight up the ramp (`Attack()` needs line of sight, so the tether is what brings them in). Where to fight the Giant is a tactic:
   question below.
2. **The wait at the Colossus stop, 3.7 / 4.6 / 8.4 s** (w14 −0.3-10.0, w15a 1.0-5.8): the leader stands 30 yd out until a follower
   drifts inside the 50 yd pick range. User's Q3, built.
3. **The lever, 5.3-6.5 s after the sixth start-pack death:** the gauntlet tank has soldiers or the Captain on him for
   19-54 mob-seconds of the start pack (`startpack.py`, w15a uld1/uld3) and walks ~35 yd to the lever after it. User's Q2,
   built.
4. **Corridor trash, 36.7-47.2 s at 22-29k** against 37-54k on the Colossus: DPS walk 19-69% of it, 34-151 bot-seconds of it Runic
   Smash dodges; pack 2 starts 3.5-4.1 s after pack 1 dies (round 9's serial cost, accepted). The dodge itself is needed: 93 smash
   hits landed on the corridor squad in these six pulls (1-25 a pull), 2 deaths in w15a uld1, each costing the rest of the gauntlet
   DPS (two dead ranged made a Colossus 40 s against 24 s in window 14).
5. **Balcony walk, 15.4-18.7 s:** walking pace for ~150 yd; the user ruled out the runner.

**Bug picked with the two answers: arena scans switch the Runic Smash dodge off.** `GetThorimRunicColossus`
(`UldEncounter_Thorim.cpp:1011` at 523b2f60f) dropped its cached guid on every 500 ms rescan and scanned from whichever bot asked,
and every bot runs `TickRunicSmash` through `ThorimRunicSmashTrigger::IsActive` (the squad check comes after
`ThorimRunicSmashImminent`). Arena bots ask from 158-175 yd, past the 150 yd search: a miss reads "no Colossus" and clears
`runicSmashSide` and `runicSmashSeenMs`. Between the lever and the Colossus' engage, arena bots cleared the latch 13 / 16 / 22 times
(w15 uld1/2/3; `smashlatch.py`), leaving it off 31.6 / 17.4 / 29.6 s of 66 / 64 / 57 s. While it is off the dodge trigger is quiet and
`ThorimPreferredGauntletLane` gives no safe lane. **81 of the 93 corridor smash hits landed in a cast during which the latch had
dropped** (`smashcost.py`, six pulls). The boss doc's own rule is "armed 10 s from the last scan that saw a cast". Churn from it is
small (13 of 341 dodges undone within 4 s, one leader lane flip), so this is a damage and lives fix more than seconds.

**Classification:** Q2 and Q3 are the user's answers; the latch is a bug (round 6 and 7 saw it, unfixed while it cost no death).

**Fix** (Thorim files only, no size, mode or player branch):
- **Lever wait (Q2).** `ThorimLeverWaiting` (`UldEncounter_Thorim.cpp`): the bot gauntlet leader, Thorim in combat on his balcony
  (`ThorimSplitActive`), lever not yet pulled and still locked (`GO_FLAG_NOT_SELECTABLE`), the leader not past the portcullis, and no
  start-pack mob attacking him (`ThorimStartPackOn`: the main tank has the pack). `ThorimGauntletLeverTrigger` fires on it until he is
  at interact distance; the action walks him to touching distance and uses the lever the moment it unlocks. Rule `thorim lever wait`
  (`UldDefinition_Thorim.cpp`): `OwnMovement` keeping Attack and AvoidAoe with only the lever action as mover, and a `Block` on tank
  assist, taunts and the DPS pickers, so he neither walks back to the pack nor taunts soldiers to the gate. A start-pack mob on him
  drops the wait and he tanks it as before. Followers at the gate: `ThorimFightingStartPack` keeps a non-healer whose target is a live
  start-pack mob within 50 yd on the pack (trigger and action of the gate walk); healers still gather on the leader, ~30 yd from the
  pack. The same holds for a real player waiting at the gate: his non-healers finish the pack first.
- **Colossus pull (Q3).** `ThorimGauntletLeadColossus`: within 30 yd of an out-of-combat Colossus and with no Iron Ring Guard or
  corridor Acolyte (33110) alive within 50 yd (`ThorimCorridorTrashNear`), it keeps the Colossus as the lead target; the lead action
  then walks to his own position at combat priority until he is in combat, and `ThorimGauntletColossusPull` fires
  `ThorimDpsPriorityTrigger` for the leading tank so `GetThorimDpsTarget` hands him the Colossus and he attacks it (proximity aggro
  is unproven: in seven pulls checked nobody came within 32 yd exact of the idle Colossus; `colaggro.py`). His `JustEngagedWith`
  interrupts the smash cast and cancels the next. Beyond 30 yd the walk to the stop is unchanged.
- **Latch (bug).** `GetThorimRunicColossus` keeps a live cached Colossus and returns it to every bot; only a bot within 150 yd of his
  spawn (2227.5, −396.18) scans, so an arena miss no longer clears anything and arena bots read his real casts. The barrier bail
  (`ThorimBarrierBailColossus`, `ThorimTickBarrierBail`) now checks the gauntlet squad, since arena melee would otherwise see a
  shielded Colossus (harmless, but noise in `thorim.barrierbail`).

**Estimated gain** on window 15: Q2 ~5-6 s, Q3 2.7-7.4 s (the stop wait less ~1 s to aggro): uld3 made by ~8 s, uld2 still ~7 s
late, uld1 ~13 s early. The latch fix saves hits and lives, not measured seconds. The ramp (item 1) decides the slow pulls.

**Next window should show:**
- Q2: `thorim gauntlet lever action` moves within ~1-6 s of the pull; the leader at interact distance before the sixth start-pack
  death; `thorim.leverpulled` within ~1 s of the sixth death (62507 − 20 s; was 4.5-7.7 s); no Growl or tank assist from him while he
  waits; gauntlet non-healers on the pack until it dies (no gate-walk move while their target is a start-pack mob). Watch the main
  tank's health with all six on him and the Behemoth's Acid Breath on the gate group; a start-pack death would be new.
- Q3: a lead move at the Colossus' own position and the leader's `thorim.dpstarget` on him within ~1 s of the leader reaching
  his stop with the corridor clear, the Colossus engaged within ~2-3 s of the arrival (`leadstop.py`: wait 3.7-10 s before); "to
  Colossus" 2-12 s (was 9.7-16.2). Watch a 62465 hit on the leader during the walk-in.
- Latch: `thorim.smashside` never back to 0 between the first seen cast and the Colossus' engage (cleared 13-22 times a pull before),
  no arena bot setting it to 0; corridor smash hits well under 10 a pull (1-25), no Runic Smash death.
- Lever to phase 2 ~150-170 s; hard mode in two of three if the ramp segment stays near 43-52 s.

**Questions for the user** (tactics, not built):
1. **Fight the Rune Giant where the squad kills his adds.** Today the gauntlet tank pulls the Giant at the top of the ramp and holds
   him alone 4.5-19.5 s while the squad kills two Honor Guards and an Acolyte 40-60 yd lower, then the squad walks up 5-8 s with
   almost no damage; the gauntlet healers trail the tank up and leave the add fight 15-30 yd behind. Should the tank, once the Giant is
   pulled, walk him back down to ~15 yd from the add fight, so adds and Giant die in one place? Saves ~5-8 s a pull and keeps the
   healers on everyone; costs the Giant's Stomp (around him) and Rune Detonation (a random player) landing among the squad up to ~20 s
   earlier. Recommended: yes.

**Review (round 10).** The latch cause holds: `smashlatch.py` and `smashcost.py` rerun on the six pulls give the same clears
(arena bots 158-175 yd out, 13 / 16 / 22 a pull) and 81 of 93 hits. At the Colossus stop no Iron Honor Guard is within 50 yd
(nearest 73-77 yd), so the pull's picker hands the leader the Colossus, not a guard behind the shut doors (scratchpad
`thorim_r10_review/nearstop.py`). Q2 and Q3 are the user's answers, the latch a bug; no size, mode or player branch; Thorim files
only. Fixed:
- The start-pack exemption covered only the gate walk: while the leader walks from the pack to the gate, non-healers more than
  25 yd behind still took the tether toward him. `ThorimTrailsGauntletLeader` now skips a `ThorimFightingStartPack` bot too.
- `thorim lever wait` let a bear's Feral Charge and a Warbringer Charge through (`Family::Charge` is no mover), taking a waiting
  leader back into the pack whenever his target came within 25 yd; the rule now holds charges.

**User answer (2026-11-23) to round 10's question:** yes. Once the Rune Giant is pulled, the gauntlet tank walks him back down to
about 15 yd from the Honor Guard/Acolyte add fight, so the squad and healers fight both together.

## Round 11 (window 16: build 2d7853f0e with round 10)

**Data:** window 16, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-133438-1` (uld1),
`-2` (uld2), `-3` (uld3); traces `env/dist/logs/botobs/603_{1,2,3}_thorim_{1795440972,1795440974,1795440980}.ndjson`, read through
`raidtest.py report --boss thorim --raid uld --head 2d7853f0e --pulls 10` (its `thorim.py` phase 2 lines), `postmortem.py --death`/
`--probes`, round 7 and 10 scripts (`seg.py`, `budget.py`, `dps.py`, `lead.py`, `leadstop.py`, `smashlatch.py`, `smashcost.py`,
`smashhits.py`, `startpack.py`, `holderpos.py`) and new ones in session scratchpad `thorim_r11/` (`r10checks.py`, `ramp.py`,
`addfight.py`, `gianttrack.py`, `stood.py`, `rampidle.py`, `shortstop.py`, `tankacts.py`, `rawpos.py`) over `snap`, `move`, `act`,
`aura` (32182), `dmg` (62465) and `note` rows. For the Giant hold: windows 13-16's twelve pulls plus window 15a's three for the add
fight, window 11 uld3 (`603_6_thorim_1795372131`, the one pull whose Giant came down the ramp) and all 37 Thorim traces for stood
positions.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795440972 | kill 5:51, normal (62565 3:22.3, phase 2 3:34.9) | none |
| uld2 | 1795440974 | kill 5:45, normal (62565 3:24.6, phase 2 3:26.5) | none |
| uld3 | 1795440980 | kill 5:43, normal (62565 3:19.4, phase 2 3:24.2) | phase 2 Chain Lightning 1 (melee at 5:06.5, 31k, his Fire Elemental 2.2 yd off) |

**Round 10 verdict:**
- **Lever wait (Q2): held.** The leader stood 2.7 yd from the lever by 0:05 in all three; `thorim.leverpulled` 0.11 / 0.18 / 0.08 s
  after the sixth start-pack death (was 4.5-7.7 s), so lever → 62565 is 169.8-169.9 s (was 163.2-164.8). No taunt, tank assist,
  charge or picker from him while waiting; no gate-walk move by a non-healer with a start-pack target; main tank lowest 79 / 97 / 98%;
  no start-pack death.
- **Colossus pull (Q3): held as built, no seconds back.** Engaged 1.8 / 0.5 / 0.5 s after the leader reached his stop (was 3.7-10 s);
  "to Colossus" 9.5 / 9.3 / 7.3 s (w15 9.7-16.2); no 62465 hit on him during the walk-in. But the squad, held on the last lane
  waypoints 55-65 yd out until the engage, now walks in after it: squad damage on him stayed under 10k/s for 6-9 s and the Colossus
  took 35.7 / 31.4 / 32.6 s (w15 19.5-28.8). To-Colossus plus Colossus 40-45 s (w15 31-45). Question 1.
- **Smash latch: held.** `thorim.smashside` never cleared between the first seen cast and the engage (13-22 arena clears a pull
  before); all 25 corridor smash hits (11 / 4 / 10) landed with the latch held (w15: 81 of 93 with it dropped, 15.5 a pull); no Runic
  Smash death. Under 10 a pull in one of three.
- **Lever to phase 2 150-170 s: missed.** 182.6 / 171.8 / 174.8 s, 12.7 / 1.9 / 4.9 s after 62565: normal mode in all three.

**Round 8 gap (the anchor tank short of the tank spot): still there, now root-caused.** uld2's anchor tank (warrior) held him from
(−2.7, −0.5) for 33 of its 212 holding snapshots (4:06.5-4:14.5), the waiting tank on the holder 8.1%; uld1 and uld3 sat on (0, 0).
The tanks' arrival test `GetDistance(spot) ≤ 1` nets the bot's combat reach, 1.5 × its model scale, and Heroism 32182 / Bloodlust 2825
carry +30% `SPELL_AURA_MOD_SCALE`, so under lust it passes to 2.95 yd exact. The tank had stepped 0.9 yd in from the off-tank spot
(`reach melee`), took him at 4:06.5 standing 2.78 yd off the tank spot, and logged no positioning row (rotation only) until Heroism
ended at 4:15.2; at its next pickup (4:34.2, no lust) it walked to (0, 0) in 0.7 s. Window 15's (−1.9, −0.9) and (−2.2, −0.7) holds,
2.1-2.3 yd exact, pass even without lust (2.5 yd).

**Gauntlet timeline** (seconds per segment from the lever; budget lever → 62565 169.8-169.9 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w16 uld1 | 0.1 | 14.6 | 46.8 | 9.5 | 35.7 | 3.8 | 53.5 | 18.7 | 182.6 | +12.7 |
| w16 uld2 | 0.2 | 15.2 | 34.5 | 9.3 | 31.4 | 5.3 | 59.3 | 16.8 | 171.8 | +1.9 |
| w16 uld3 | 0.1 | 14.8 | 41.0 | 7.3 | 32.6 | 8.5 | 54.3 | 16.5 | 174.8 | +4.9 |
| fastest before | 4.5 | 11.8 | 33.0 | 2.0 | 18.5 | 5.3 | 43.2 | 15.2 | 156.2 | |

Against the fastest segments the Colossus loses 13-17 s (the walk-in above, then Runic Barrier), the adds and Giant 10-16 s, the
corridor 1.5-14 s. On the ramp the leader pulls the foot Honor Guard, walks up past the second one and the Acolyte (both take him)
and pulls the Giant 11.5-12 s after the first guard engages; the squad kills the foot guard and the Acolyte at the ramp foot, then
walks ~65 yd up to the second guard and the Giant: DPS spent 84-101 bot-seconds a pull without a target between the first guard's
engage and the Giant's death (w15 85-96), the Giant stood 40-62 yd (p50) from the hold spot below, and died 18.6-20.1 s after the last
add.

**Stop rules:** not polished (no hard mode). Not stuck: rounds 3-11 picked nine different causes.

**Failure ranking:** deaths, phase 2 Chain Lightning 1 (a melee in a pile with a guardian; normal mode, not picked). On the clock:
the Colossus walk-in 6-9 s (question 1), the ramp walk (the user's answer, built), the corridor trash (spread 34.5-46.8 s).

**Change** (Thorim files only, no size, mode or player branch):
1. **Rune Giant hold (the user's answer).** `ThorimGauntletGiantHeld` (`UldEncounter_Thorim.cpp`): the bot gauntlet leader, past the
   portcullis and within 150 yd of the Giant's spawn, while the Giant is in combat on him. `ThorimGauntletGiantHold` adds the walk:
   farther than `ULDUAR_THORIM_RUNE_GIANT_HOLD_TOLERANCE` (3 yd, 2D exact) from `ULDUAR_THORIM_RUNE_GIANT_HOLD_SPOT`
   **(2187.79, −432.78, 426.39)**, the lead trigger fires and the lead action walks him there at combat priority. While he holds the
   Giant, `GetThorimDpsTarget` hands him the Giant and the tank branch of `ThorimDpsPriorityTrigger` fires until he targets him; rule
   `thorim giant hold`: `OwnMovement` keeping Attack and AvoidAoe with the lead action as the only mover, and a `Block` on tank assist,
   charges and the DPS pickers, so `tank face`, `reach melee` or a Feral Charge cannot walk the Giant into the add fight. Taunts stay
   (an add taunted off a healer comes to him). With a real player leading nothing changes: the lead nodes run only for a bot leader.
   - The spot: the median position of the Honor Guards and Acolytes fought off the leader over fifteen 25-man pulls is
     (2202.6, −430.7, 418.8); the spot is 15.0 yd (2D) up the ramp from it. The Giant stood on it in window 11 uld3 (2:21.2) and
     players within 0.6 yd ((2188.02, −433.31, 426.31) w11 uld3, (2187.7, −431.8, 426.34) w13 uld2).
   - Stomp (62413: 10 yd around the Giant, a ~3 yd knockback every 8 s) pushes the tank down-ramp; the 3 yd tolerance walks him back,
     so the Giant should settle 1-8 yd up-ramp of the spot, 15-22 yd from the adds.
2. **Balcony walk from the ramp (needed by 1).** The Giant now dies at z ~426 with the squad at z 412-426, below the floor line, where
   `ThorimBalconyOpen` refused every bot (window 11 uld3: 8 of 11 walked back down the corridor). `ThorimOnBalconySide`: above the
   floor line, or inside the ramp box `ULDUAR_THORIM_RAMP_BOX_*` (x 2150-2235, y −445..−385); below the line the walk also needs the
   second Colossus doors (GO 194558, (2135.08, −417.905, 440.0)) open, which the Giant's death does and Thorim's reset undoes, not the
   Giant scan, whose cached miss reads "Giant dead". `ThorimBalconyWalk` (the `thorim balcony guard` gate) takes the same side test.
3. **Holder arrival (round 8's gap, bug).** `ThorimTankSpotNeedsMove`: the tank Thorim is hitting counts as arrived only within
   `ULDUAR_THORIM_PHASE2_HOLDER_TOLERANCE` (1 yd, 2D exact); the waiting tank keeps the 1 yd net test, which leaves room for its reach
   step. The holder needs no such room: Thorim chases his victim.

**New coordinate to navprobe before the merge:** `ULDUAR_THORIM_RUNE_GIANT_HOLD_SPOT` (2187.79, −432.78, 426.39): on the mesh at that
z; a path down from the Giant's spawn (2134.57, −440.32, 438.33); a path up to `ULDUAR_THORIM_BALCONY_1` (2141.0, −408.0, 438.247)
with the second doors open. The ramp box is a region test, not a walked point.

**Estimated gain:** the walk up the ramp (most of the 84-101 idle DPS bot-seconds, ~5-8 s) against a balcony walk starting ~30 yd
farther down (~4-5 s): about 1-4 s a pull. The hold's case is one place to fight and heal, not the clock.

**Next window should show:**
- Hold: a `thorim gauntlet lead action` move to (2187.79, −432.78) within ~1 s of the Giant's first hit on the leader; the Giant's
  `snap` track coming down the ramp and his distance to the spot p50 under ~8 yd (40-62 now); the leader's `thorim.dpstarget` the
  Giant while he holds; no `tank face`, `reach melee` or charge move from him then; DPS bot-seconds without a target from the first
  Honor Guard's engage to the Giant's death well under 84-101; the Honor Guards + Acolyte + Giant segment under 53.5-59.3 s.
- Balcony: `thorim balcony advance action` moves from corridor bots below z 429.6 within ~1 s of the Giant's death; nobody walking
  north of y −385 after it; the first `BALCONY_5` latch 18-22 s after his death (15.4-20.2 now).
- Holder: the anchor tank's holds on (0, 0) in ~98%+ and the waiting tank on the holder under ~2% in every pull, Heroism or not;
  holder positioning moves no more than ~5 above the waiting tank's (a tug-of-war shows as dozens).
- Watch: Stomp and Rune Detonation hits and deaths among the squad and healers at the hold (none before); a Giant that turns on a
  squad member instead of the leader (no hold then, he walks into the adds).
- Unexplained, not picked: uld2's ring had two slots within 8 yd in 94.1% of settled snapshots (uld1 31.2%, uld3 64.5%; window 15
  3.9-38.3%) with its slot bearings 90° apart and no Chain Lightning death; reread if ring Chain Lightning deaths come back.

**Questions for the user** (tactics, not built):
1. **The squad walks to the Colossus with the leader.** Since round 10 he pulls the Colossus from his 30 yd stop at once, but the
   squad still waits on the last lane waypoints (y −318/−329), 55-65 yd out, until the engage ends the smash: squad damage stays
   under 10k/s for 6-9 s after the pull and the Colossus took 31-36 s against 19-29 before. Extend each lane by one index-matched
   waypoint near y −360 (left x ~2242, right x ~2212, ~39 yd from him, picked from trace positions and navprobed) so the Runic Smash
   dodge holds the squad there when the leader goes on? Saves ~6-9 s a pull. Recommended: yes.

**Review (round 11).** The holder cause holds: in uld2 Heroism 32182 sat on both tanks 3:35.2-4:15.2, Uldyaa took Thorim at 4:06.5
from (−2.7, −0.5), 2.75 yd exact, with no positioning move until the swap at 4:14.5, Uldyab 0.9 yd from it on the off-tank spot.
32182 carries aura 61 (`SPELL_AURA_MOD_SCALE`, +30%) and `Player::SetObjectScale` sets combat reach to 1.5 × scale, so `GetDistance`
≤ 1 passed to 2.95 yd. Giant hold: window 11 uld3's Giant walked the ramp at ~7.3 yd/s and stood on the hold spot at 2:21.2; GO
194558 is `DATA_THORIM_SECOND_DOORS`, opened only by the Giant's `JustDied` and closed by Thorim's `CloseDoors` on reset. The hold
is the user's answer, the holder a bug; no size, mode or player branch (a real gauntlet leader keeps the job); Thorim files only.
`pb-check` passes. Fixed:
- `ThorimBalconyWaypointReached` also counts a waypoint once the bot's y passes it, a measure of progress only on the balcony: a
  ramp-box bot at y > −408 read as past `BALCONY_1` (> −396: `BALCONY_2`) and was aimed at a balcony waypoint from the floor below.
  Waypoints now count only above the floor line. In window 11 uld3 every box bot stood at y < −428 when the Giant died.
- `ThorimBalconyGuardMultiplier` still zeroed phase 2's reach only above the floor line while the guard rule now covers the ramp box;
  it takes `ThorimOnBalconySide` too, so a phase 2 straggler on the ramp is not reach-walked back through the corridor.
- The findings doc said the doors stand in for the Giant scan below the line; the walk needs both.

Left: the hold spot is not navprobed (no Docker in the review), though the Giant and players stood on it and walked the ramp to it.
The leader walks the Giant ~54 yd down with his back to him (~7 s, no dodge, parry or block from behind): watch the Giant's melee on
him during the walk.

## Round 12 (window 18: build c01caf9cc with round 11)

**Data:** window 18, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-153900-1` (uld1),
`-2` (uld2), `-3` (uld3); traces `env/dist/logs/botobs/603_{1,2,3}_thorim_{1795448420,1795448430,1795448432}.ndjson`, read through
`raidtest.py report --boss thorim --raid uld --head c01caf9cc --pulls 10` (its `thorim.py` phase 2 lines), `postmortem.py --death`, round 7,
10 and 11 scripts (`seg.py`, `budget.py`, `leadstop.py`, `r10checks.py`, `holderpos.py`, `gianttrack.py`, `ramp.py`, `rampidle.py`) and new
ones in session scratchpad `thorim_r12/` (`holder.py`, `corridor.py`, `tgtdps.py`, `idle.py`, `nodmg.py`, `balcony.py`, `walkback.py`,
`giantdmg.py`, `holdmoves.py`, `colpos.py`, `ev.py`, `botrow.py`, `snapsq.py`, `auras.py`) over `snap`, `move`, `act`, `aura`, `dmg`, `cast`
and `note` rows. Window 16's three pulls (`603_{1,2,3}_thorim_{1795440972,1795440974,1795440980}`) re-read as the baseline.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795448420 | kill 5:55, normal (62565 3:20.7, phase 2 3:37.8) | phase 2: Dark Rune Champion Mortal Strike + melee on an arena ranged walking to its spot, P2 +3.9 s |
| uld2 | 1795448430 | kill 5:59, normal (62565 3:16.5, phase 2 3:30.9) | phase 2: Thorim's first melee (29.2k) on a warlock 10.5 yd from his landing, P2 +1.1 s |
| uld3 | 1795448432 | kill 5:46, normal (62565 3:20.7, phase 2 3:29.2) | phase 1 at the Giant hold: Rune Detonation 10.6k + Stomp 8.8k + Iron Honor Guard Cleave 8.9k on a rogue in 0.34 s (2:42.7) |

**Round 11 verdict:**
- **Giant hold: positions held, no time came back.** The Giant stood p50 4 / 3 / 5 yd from the spot (w16: 47 / 62 / 40); the leader's only
  accepted move from the Giant's engage to his death was the hold walk (no `tank face`, `reach melee`, charge or taunt). But the hold walk
  was refused ("wait") for **2.7 / 3.0 / 2.8 s** after the Giant engaged (check: within ~1 s), and the Honor Guards + Acolyte + Giant segment
  was 59.7 / 55.5 / 53.0 s against 53.5 / 59.3 / 54.3 (check: shorter). DPS bot-seconds dealing no damage over the last 2 s in that
  segment 60 / 40 / 52 (w16 71 / 77 / 64); the snapshot-target count used before reads 112 / 85 / 89 (w16 85 / 101 / 84), but casters cast
  with an empty snapshot target, so it overcounts ranged. The Giant took 8-13% while the adds lived and died 20-23 s after the last add
  (w16 18.6-20.1): the picker kills adds first, so the hold only moves where the serial kill happens.
- **Balcony walk from the ramp: held.** First `thorim balcony advance action` move +0.1-0.5 s after the Giant's death (median +0.5-0.8);
  nobody on the floor north of y −385 afterwards; first `BALCONY_5` +20.3 / +18.7 / +17.1 s (predicted 18-22). Giant death to phase 2
  22.8 / 21.9 / 20.5 s (w16 18.7 / 16.8 / 16.5): the hold's ~4 s cost, as estimated.
- **Holder arrival: held.** The tank Thorim hits stood on (0, 0) in 94.0-97.4% of holding snapshots, the rest the 3.5 yd trade walk from
  the off-tank spot along y +0.0 (no short hold like w16's (−2.7, −0.5)); under Heroism 94.0 / 96.8 / 96.2%, without 96.7 / 96.4 / 97.4%.
  Waiting tank on the holder 1.9 / 1.5 / 1.2% (w16 uld2 8.1%); waiting-tank positioning moves 7 / 6 / 5.
- **Stomp and Rune Detonation on the squad** (leader excluded): Stomp 11 / 13 / 14 hits (w16 15 / 22 / 9), Rune Detonation 26 / 4 / 14
  (w16 21 / 19 / 13); one death (uld3, above). The Giant turned on a squad melee once (uld2 2:25.2-2:25.4) and came straight back.

**Gauntlet timeline** (seconds per segment from the lever; budget lever → 62565 168.5 / 169.5 / 170.0 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w18 uld1 | 1.6 | 13.0 | 39.0 | 12.1 | 32.7 | 6.3 | 59.7 | 22.8 | 185.6 | +17.1 |
| w18 uld2 | 0.5 | 15.0 | 49.2 | 7.2 | 30.0 | 5.1 | 55.5 | 21.9 | 183.9 | +14.4 |
| w18 uld3 | 0.05 | 14.7 | 48.7 | 13.0 | 21.3 | 7.5 | 53.0 | 20.5 | 178.6 | +8.5 |
| w16 uld1 / 2 / 3 | 0.1-0.2 | 14.6-15.2 | 46.8 / 34.5 / 41.0 | 9.5 / 9.3 / 7.3 | 35.7 / 31.4 / 32.6 | 3.8-8.5 | 53.5 / 59.3 / 54.3 | 18.7 / 16.8 / 16.5 | 182.6 / 171.8 / 174.8 | +12.7 / +1.9 / +4.9 |

uld1's lever came 1.6 s after the pack: a start-pack mob was on the leader until 0:28.8, which drops the wait as designed. uld2's corridor
lost ~6 s between the packs (pack 2 engaged 11.3 s after pack 1, not 4.5): a Runic Smash flip took the leader from the left lane back to
the right while the squad stood. uld3's pack 2 took 28.8 s: a mage's Frost Nova (1:10.98) rooted its guards in the smash lane, the leader
walked back to them and the melee stayed in the safe lane 14-19 yd away until the smash flipped at 1:26.2 (`reach melee` against
`thorim runic smash action` 9.4 flips a minute, 2.6 before). Both one-pull cases, not picked.

**Stop rules:** not polished (three normal-mode kills, 1 death each). Not stuck: rounds 3-12 picked ten different causes.

**Failure ranking:** deaths are one a pull from three different mechanics; the failure is Sif's clock, missed by 8.5-17.1 s. On the clock,
against window 16: the adds and Giant unchanged, the balcony +4 s; the hold walk's lock and the overshoot it causes ~5 s in all three pulls
(below); the Colossus stop 5.2 s in uld3 (below); the Colossus walk-in (the pending lane waypoint question, not built).

**Bug 1 (all three pulls): the hold walk waits out the leader's own walk to the Giant.**
- Trace: the leader re-issues the walk to the Giant's spawn (2134.57, −440.32, 438.33) at combat priority every ~5 s on the way up. The Giant
  engaged at 2:27.07 / 2:24.40 / 2:26.77 with the leader at x ~2154-2156, but every hold move until 2:29.81 / 2:27.43 / 2:29.55 came back
  `wait` (`hms` counting down from the Giant walk's booking), so he ran on to x 2134.7-2140 and only then turned: ~15-20 yd up and back,
  ~4.5-5 s, with the Honor Guards on him (Hamstring, a 3 s Shield Smash stun and a Stomp knockback on the way down in uld1). He reached the
  spot 15.1 / 13.6 / 12.2 s after the engage; the squad, its foot adds dead, stood without damage for 2-6 s and walked up to meet the guard he carried.
- Code: `ThorimGauntletLeadAction::Execute` (`UldActions_Thorim.cpp:291-297` at c01caf9cc) walks to the Giant with `MoveTo(…,
  MOVEMENT_COMBAT)`, booking `last movement` for the walk's length (up to `MaxWaitForMove`, 5 s); the hold walk is also `MOVEMENT_COMBAT`, and
  `MovementAction::IsWaitingForLastMove` refuses anything not strictly above the booking (pitfalls, "A single mover is refused its own
  re-target").
- **Classification: bug** (round 11's walk-down, delayed by its own previous leg).

**Bug 2 (uld3): the Colossus stop sits on the pull test.**
- Trace: the leader reached (2235.44, −359.53), 37.50 yd exact from the Colossus, at 1:41.8; every later lead move re-issued that same
  point as `dup` until a DPS pulled the Colossus at 1:46.97 (leader `tank assist` at 1:46.60); uld1 and uld2 stopped 37.5 yd out too and
  pulled at once (0.5-2.0 s).
- Code: the action walks to `MoveTo(colossus, ULDUAR_THORIM_LEAD_COLOSSUS_DISTANCE)`, a point where `GetDistance` is 30 to float precision,
  and `ThorimGauntletLeadColossus` / `ThorimGauntletColossusPull` pull only at `GetDistance ≤ 30`; one arrival lands a hair over, and
  `MoveTo(WorldObject*)` then recomputes the same point (needToGo ~0), a duplicate.
- **Classification: bug** in round 10's Colossus pull (the user's Q3).

**Fix** (Thorim files only, no size, mode or player branch; the lead nodes run only for a bot leader, so a real player leading is unchanged):
1. `ThorimGiantWalkBooked` (`UldEncounter_Thorim.cpp`): the bot's `last movement` is a combat-priority walk aimed within 3 yd
   (`ULDUAR_THORIM_RUNE_GIANT_HOLD_TOLERANCE`) of the Giant's spawn. The hold branch of the lead action lowers that booking to
   `MOVEMENT_NORMAL` before its own combat-priority walk, the Onyxia / Putricide re-aim precedent; a forced dodge still blocks it, and any
   other booking (an AvoidAoe step) is left alone.
2. The walk to the Colossus stops `ULDUAR_THORIM_LEAD_COLOSSUS_STOP_MARGIN` (1 yd) inside the 30 yd pull test (29 yd); the beyond-the-stop
   and pull tests keep 30, so a leader waiting there for trash does not move between them.

`pb-check` passes (syntax check of the 14 including TUs, pblint, codestyle).

**Next window should show:**
- Hold: the first hold move (`thorim gauntlet lead action` to (2187.79, −432.78)) accepted within ~0.5 s of the Giant's engage, no `wait`
  row aimed at the hold spot; no leader position west of x ~2148 after the engage; the leader at the spot ~7-10 s after the engage (12.2-15.1 now);
  the Honor Guards + Acolyte + Giant segment ~3-5 s shorter than 53-60 s.
- Colossus: in every pull the leader's lead move to the Colossus' own position within ~1 s of reaching his stop (36.5 yd exact), no `dup`
  rows there; "to Colossus" no longer 12-13 s from a stall.
- Phase 2 ~3-8 s earlier, so hard mode is possible on the fastest pool; the others stay 5-12 s late until the Colossus walk-in question
  and the serial add-then-Giant kill are addressed.
- Watch: the Giant's melee and the carried Honor Guards on the leader during the shorter walk down (his health dipped to 22-29% at
  2:33-2:36 in uld2/uld3 this window).

**Seen, not picked:** ranged squad bots at the ramp foot (z 412) lose line of sight to adds on the ramp (z 416-422); `invalid target` then
drops the target (`AttackersValue::IsValidTarget` needs LOS) while `GetThorimDpsTarget` reads `possible targets no los`, so they idle until the
25 yd trail walks them up (uld2: four ranged 2:18.9-2:23.5). Generic LOS handling, shared.

**Questions for the user:** none new. The lane waypoint near y −360 stays pending and unbuilt.

## Round 13 (window 19: build 99188e0f1 with round 12)

**Data:** window 19, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-164646-6` (uld1),
`tr-20261123-164525-4` (uld2), `tr-20261123-164546-5` (uld3); traces `env/dist/logs/botobs/603_{6,4,5}_thorim_{1795452448,1795452367,1795452388}.ndjson`,
read through `raidtest.py report --boss thorim --raid uld --head 99188e0f1 --pulls 10` (its `thorim.py` lines), `thorim.py --phase2`,
`postmortem.py`, round 7 and 12 scripts (`budget.py`, `seg.py`, `ev.py`, `botrow.py`, `colpos.py`) and new ones in session scratchpad
`thorim_r13/` (`r12checks.py`, `leadstall.py`, `cohesion.py`, `chain.py`, `chainstats.py`, `ringat.py`, `thorimtrack.py`, `anchordrift.py`,
`anchorgeo.py`, `relatch_geo.py`, `at.py`) over `snap`, `move`, `act`, `cast`, `aura`, `dmg` (64390) and `note` rows (`thorim.ringspot`,
`thorim.leverpulled`, `thorim.p2start`, `thorim.sifjoined`). Windows 14-18 (15 pulls) re-read for the chains and the ring anchor.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795452448 | kill 5:52, normal (62565 3:15.4, phase 2 3:26.0) | phase 1: Rune Giant Stomp on a melee at the hold spot after a Rune Detonation (2:39.5) |
| uld2 | 1795452367 | kill 6:17, normal (62565 3:21.0, phase 2 3:57.7) | phase 1: Dark Rune Champion Mortal Strike on an arena healer (1:59.1) |
| uld3 | 1795452388 | wipe 5:28, **hard mode** (phase 2 3:15.1, deadline 62507 + 150 s = 3:23.1) | one Chain Lightning cast at 4:12.2 (P2 +57 s): both tanks and two melee (3 by its blow, the holder by the next swing); the tanks' second lives: Unbalancing Strike 1 (4:22.8), Thorim melee 1 (5:03.6); then Thorim melee on non-tanks 24, Unbalancing Strike on a healer 1, Lightning Charge 1 |

**Round 12 verdict: both fixes held.**
- **Hold walk:** the first hold move was accepted −0.27 / +0.09 / −0.26 s from the Giant's engage (w18 refused 2.7-3.0 s); no `wait` aimed at the
  hold spot until the leader stood on it (uld1's one, 2:30.7, came behind a `reach melee` booking after he arrived); westmost leader position
  after the engage x 2158.2 / 2156.5 / 2159.0 (w18 2134.7-2140); leader on the spot +4.3 / +7.7 / +10.7 s (w18 12.2-15.1); his lowest health
  67 / 38 / 43% (w18 22-29%). Honor Guards + Acolyte + Giant 51.2 / 55.8 / 53.2 s (w18 59.7 / 55.5 / 53.0): shorter only in uld1.
- **Colossus stop:** the lead move to his own position 0.3 s after the leader reached his stop (36.5-37.1 yd exact) in uld1 and uld3; in uld2 the
  leader arrived as the trash died and went straight in; no `dup` row. "To Colossus" 9.5 / 1.5 / 5.8 s (w18 12.1 / 7.2 / 13.0).

**Gauntlet timeline** (seconds per segment from the lever; budget lever → Sif's deadline 169.9 / 169.8 / 169.9 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w19 uld1 | 0.2 | 17.2 | 45.7 | 9.5 | 30.5 | 4.8 | 51.2 | 21.5 | 180.5 | +10.6 |
| w19 uld2 | 0.2 | 17.0 | 61.5 | 1.5 | 36.5 | 10.9 | 55.8 | 23.2 | 206.5 | +36.7 |
| w19 uld3 | 0.1 | 12.3 | 40.2 | 5.8 | 24.0 | 5.6 | 53.2 | 20.8 | 161.9 | made by 8.0 |
| w18 uld1 / 2 / 3 | 1.6 / 0.5 / 0.05 | 13.0-15.0 | 39.0 / 49.2 / 48.7 | 12.1 / 7.2 / 13.0 | 32.7 / 30.0 / 21.3 | 5.1-7.5 | 59.7 / 55.5 / 53.0 | 22.8 / 21.9 / 20.5 | 185.6 / 183.9 / 178.6 | +17.1 / +14.4 / +8.5 |
| w16 uld1 / 2 / 3 | 0.1-0.2 | 14.6-15.2 | 46.8 / 34.5 / 41.0 | 9.5 / 9.3 / 7.3 | 35.7 / 31.4 / 32.6 | 3.8-8.5 | 53.5 / 59.3 / 54.3 | 18.7 / 16.8 / 16.5 | 182.6 / 171.8 / 174.8 | +12.7 / +1.9 / +4.9 |

uld3 is the fastest lever-to-phase-2 since window 12 (156.2 s) and the first hard mode since window 15.

**Why uld2's phase 2 came at 3:58** (+44.6 s against uld3, budget missed by 36.7 s):
- **Corridor +21.3 s.** Pack 1 died ~1:12.9 and pack 2 engaged at 1:33.0. The leader stepped to the right lane's waypoints 2 (1:14.4) and 3
  (1:17.6); the squad stood on waypoints 0 (five ranged) and 1 (four melee), 31-43 yd behind, with no move order from 1:20.8 to 1:27.1, while his
  lead step held on the 30 yd cohesion check (`ThorimGauntletLeadStep`, `UldEncounter_Thorim.cpp:1760` after this round) for up to its 15 s
  stall. A Runic Smash dodge at 1:27.1 moved everyone to the left lane and broke it. Cause: `ThorimGauntletPositioningTrigger`
  (`UldTriggers_Thorim.cpp:53`) moved only bots standing off the lane to the leader's waypoint; a bot standing at an earlier waypoint of the lane
  never moved, and waypoints 2-5 have overlapping radii, so a leader walking 2 → 3 never left the lane for the trail branch. `leadstall.py`
  (leader idle on a lane waypoint, no engaged corridor mob, a squad non-healer past 30 yd, followers in the lane behind him): 7.5 / 2.7 / 1.0 s in
  window 19, 1.3 s in w18 uld2, 1.0 s in w15 uld3. Plus 5.9 s at waypoint 0 waiting for the squad from the gate (0:41.7-0:47.7).
- **Colossus +12.5 s.** He lost only 4% in the first 9 s after the pull (the squad walks in from the last waypoints, the pending lane waypoint
  question), then charged north at 2:11 and was finished at y −363..−370, so the ramp walk started 40 yd north of his spawn: **ramp +5.3 s**.
- Honor Guards + Acolyte + Giant +2.6 s, balcony +2.4 s, walk to trash +4.7 s.

**uld3's hard-mode wipe: not the windows 14-15 pattern.** The aftermath is the same (24 non-tanks to Thorim's melee once no tank stood), but the
tanks did not fall to the race: at 4:11.8 both tanks stood at 93-100% health, healers at full health with 41-60% mana, Thorim at 75.8%. One
Chain Lightning (cast 4:11.71 on a melee) hit 8 players at ×1.35-1.6 per hop (`EffectChainAmplitude`, boss doc): three slot-1 melee stacked at
(2114.5, −260.5), then a 7.6 yd hop (likely through Uldzal's pet, 2.6 yd from the last melee) to slot 2's Uldzam and Uldzat, then Uldzax 4.3 yd on, then the holder
4.0 yd on (61.5k) and the waiting tank 3.5 yd on (92.4k). Windows 14-15's first tank death came at +82-103 s with healers at 47% mana or a
healer down to Blizzard.

**Root cause: the ring anchor was stale.** `SolveRingBearings` latches `ringAnchor` once, at Thorim's landing (144° to the tank spot), and never
again. At 4:01.8 the waiting tank taunted from the west off-tank spot; Thorim walked to him and, once the taunter walked back east to the tank
spot, rested 1.6 then (4:07.6) 4.5 yd south-west of it, where the bearing to the tank spot is 44°. Slot 2's solved bearing 54° then put its point
4.4 yd from the holder: the slot stood behind the tanks. The round 8 move of the off-tank spot west is what lets a taunt re-seat him on that
side; before it (windows 12-14) he rested south-east in 100% of settled snapshots:

| Pulls | Thorim south-west of the tank spot | latched slot point within 8 yd of a tank spot (replay) | reader: holder within 8 yd of a ring body |
|---|---|---|---|
| w18 uld1, w16 uld2, w16 uld3, w15a uld2 | 89 / 94 / 90 / 61% of settled snapshots | 89 / 94 / 90 / 61% | 80.2 / 12.8 / 67.6 / 47.8% |
| w19 uld3, w15 uld1 | 13 / 1% | 24 / 96% | 47.0 / 71.4% |
| the other nine post-round-8 pulls | 0% | 0% in six; 100 / 100 / 90% in w16 uld1, w15a uld1, w15a uld3 | 8.9-36.3%; 92.7 / 88.0 / 36.3% |

The long chains of windows 15-19 run through such a slot or across a pet: w16 uld3 (8 hops, tanks 7th and 8th at 49k and 64k, one death),
w15a uld1 (8, two deaths), w15 uld1 (7, four deaths). Each of these crossed a 9-12 yd gap no player hop can make (8 yd with both reaches), so
pets bridge slots too; uld3's 7.6 yd hop had Uldzal's pet 2.6 yd from the last melee.

**Classification: bug.** The ring's slots are meant at +90 / +180 / +270° from the tank side, leaving it clear (boss doc); a latch taken on the
other side of the tank spot puts a slot on it.

**Stop rules:** not polished (one hard-mode pull, a wipe). Not stuck: rounds 3-13 picked eleven different causes; round 12's both held.

**Failure ranking:** the uld3 phase 2 wipe, 32 deaths, triggered by the chain through the stale-anchored slot (4 deaths in the cast, the rest
Thorim loose); then one phase 1 death each in uld1 (Stomp at the hold) and uld2 (arena Champion). On Sif's clock: uld2's corridor stall (7.5 s,
independent and small, fixed as the second), the Colossus walk-in (the pending question), uld2's charged Colossus.

**Fix** (Thorim files only, no size, mode or player branch):
1. **Ring anchor.** `RingAnchorBearing` (`UldEncounter_Thorim.cpp:546`) aims at the midpoint of the tank and off-tank spots, so the tank side is
   the pair. `RelatchRingAnchor` (`:843`, from `SolveRingBearings` every 500 ms solve, `:927`): with Thorim not moving and a tank (bot or human)
   holding him, when a latched slot point (8.8 yd round his live position) lies within 8 yd (`ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP` + 2 ×
   `DEFAULT_COMBAT_REACH`) of either tank spot and the live anchor's ring clears the spots by at least `ULDUAR_THORIM_RING_ANCHOR_GAIN` (0.5 yd)
   more, it re-latches: each slot takes the new +90 / +180 / +270 bearing of the permutation whose largest turn from its composed bearing is
   smallest (ties: least total), and the arrival latches drop (erased one by one, so `thorim.ringarrived` records it). Replay over 18 pulls
   (`relatch_geo.py`): latched slot point within 8 yd of a tank spot 42.3% of settled snapshots → 2.0%, 0-3 re-latches a pull, none in eight
   of the nine pulls already clear (w18 uld3: two in the opening). Rejected: a bearing threshold (25°: 16%, 35°: 29%; at 4-6 yd, 20° of drift
   already brings a slot within the jump) and
   the tank spot alone as anchor (the waiting tank 6.4-7.0 yd from a slot whenever he sits south-west; the midpoint 8.2-9.0, 7.7 at his deepest).
   In uld3 it would have re-latched at 4:07.6 (slot 2's three melee turning 100°, ~13.5 yd), 4.1 s before the chain.
2. **Lane followers.** `ThorimGauntletPositioningTrigger` (`UldTriggers_Thorim.cpp:81`) also fires for a squad bot standing in the lane at an
   earlier waypoint than the leader's, unless it is fighting (`ThorimGauntletFighting`: its target a gauntlet mob in combat within 50 yd); the
   action already walks it to the leader's waypoint. The boss doc's intent: followers take the leader's waypoint.

**Next window should show:**
- Ring: `thorim.ringspot` `bearing` (the latched one) changing for all slots at once only where Thorim settled with a latched slot point within
  8 yd of a tank spot: none in a pull where he rests south-east, 1-3 where he settles south-west. Latched slot point within 8 yd of a tank spot
  (from the notes' `bearing` and his position) in under ~5% of settled snapshots (42% before; 61-94% in the four south-west pulls). Reader:
  holder within 8 yd of a ring body at or under ~35% in every pull (47-93% in the drifted pulls); no Chain Lightning with both tanks in it after
  four or more ring bodies. Watch: melee on Thorim to +100 s still ~100%, moves per ring bot a minute (2.6-3.5, 8.1 in w19 uld3) up by at most
  one walk per re-latch; a re-latch while he is being dragged (it should never fire with him moving).
- Corridor: `leadstall.py`'s in-lane-behind spans gone (7.5 / 2.7 / 1.0 s), a `thorim gauntlet positioning action` move to the leader's
  waypoint from a follower standing behind him in the lane within ~1 s of his waypoint index advancing; the corridor trash no longer 61.5 s
  from a stall. Watch: a follower walking into pack 2 ahead of the leader's engage, or pulled off a live corridor mob (the fighting gate).
- Hard mode still only on the faster pulls; the Colossus walk-in (pending question) and pet bridges remain.

**Seen, not picked:**
- **Pets bridge ring slots.** The 7-8 hop chains of windows 15-16 each crossed a 9-12 yd gap between slots that only a pet can make (the
  recorder logs no pet damage). Pets are chased onto their slot bearing at range 0 (~6.5-7 yd from him) within 22.5°, so two adjacent slots' pets can sit
  ~5.4 yd apart. Next-round candidate if the re-latch does not stop the long chains.
- uld2's Colossus charged north at 2:11 (49%) and was finished at y −363..−370; the leader then started the ramp walk 40 yd north of the spawn.

**Questions for the user:** none new. The lane waypoint near y −360 stays pending and unbuilt.

**Review (round 13).** The anchor cause holds: in uld3 Thorim stood 4.54 yd from the tank spot at bearing 44° from 4:07.6 to the cast,
slot 2 latched at 54° (`thorim.ringspot`), its point 4.4 yd from the holder, Uldzax on it and 4.0 yd from him; the 64390 cast at
4:11.71 hit 8 and killed Uldzat, Uldzax and Uldzaa, then Uldzab on the next swing. `relatch_geo3.py` (the code's rule: no distance
gate, 0.5 yd gain) rerun on the 18 pulls gives 42.3% → 2.0% at 0-3 re-latches a pull; with the midpoint anchor at landing too
(scratchpad `thorim_r13_review/relatch_midland.py`) 1.2%, 19 re-latches. The snapshot's moving flag is the `isMoving()` the code
reads, so the replay's settle test is the code's. Corridor: in uld2 the leader stood on right lane waypoint 3 from 1:19.7 to 1:27.2
with five ranged on waypoint 0 and four melee on 1, and both healers stopped on waypoint 2 once inside it (no positioning move after
1:20.8), which is the in-lane gap. Both are bugs against the doc's intent (tank side clear, followers on the leader's waypoint); no
size, mode or player branch (a real tank or gauntlet leader takes the same path); Thorim files only; no comments. `pb-check` passes.
Fixed: the findings doc still said the ring "cannot rotate as the boss shuffles", above the re-latch paragraph.

Left: the re-latch has no settle time, so a stop mid-drag can fire it (in the replay's 0-3 a pull; w18 uld3 two in the opening). In
uld3's re-latch slot 2 turns 100° and its straight walk passes ~2.6 yd from the off-tank spot for ~2 s: watch chains during re-latch
walks next window.

**User answer (2026-11-23) to round 11's question:** yes. One more lane waypoint near y −360 in each lane (left x ~2242, right
x ~2212, about 39 yd from the Colossus), index-matched like the other lane pairs, so the squad walks up to the Colossus with the
leader. Points picked from trace positions; round 14 builds it.

## Round 14 (window 21: build 90400b39c with round 13)

**Data:** window 21, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-184054-5` (uld1),
`tr-20261123-184054-6` (uld2), `tr-20261123-183954-4` (uld3); traces `env/dist/logs/botobs/603_{5,6,4}_thorim_{1795459306,1795459310,1795459237}.ndjson`,
read through `raidtest.py report --boss thorim --raid uld --head 90400b39c --pulls 10` (its `thorim.py` lines), `postmortem.py --death`,
round 7, 10 and 13 scripts (`budget.py`, `leadstop.py`, `leadstall.py`, `chain.py`) and new ones in session scratchpad `thorim_r14/`
(`ringcheck.py`, `ringtime.py`, `track.py`, `bunnypos.py`, `smashreach.py`, `finegrid.py`, `sink.py`, `sinkwide.py`, `stoodat.py`,
`wpcheck.py`) over `snap`, `move`, `note`, `dmg`, `heal`, `cast` (Sif `"sp":62507`/`62565`) rows; all 46 Thorim traces on disk for the
corridor floor, the smash reach and the new waypoints.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795459306 | kill 5:55, normal (62565 3:24.9, phase 2 3:38.0) | none |
| uld2 | 1795459310 | kill 5:59, normal (62565 3:18.2, phase 2 3:37.8) | none |
| uld3 | 1795459237 | wipe 6:04, **hard mode** (62507 0:46.0, no 62565 cast, `thorim.sifjoined`; phase 2 3:12.2 against 3:16.0) | phase 2: Thorim melee on a non-tank 17, on a tank 1; Blizzard 5; Unbalancing Strike 3; Sif Frostbolt 2, Frostbolt Volley 2; Chain Lightning 1; Lightning Charge 1 |

**Round 13 verdict:**
- **Ring re-latch: held.** It fired where a latched slot point came within 8 yd of a tank spot: uld3 at 4:03.0 (2.2 s after a trade left
  Thorim south-west of the tank spot) and 5:22, uld1 once at 4:11; uld2 never needed it. Latched slot point within 8 yd of a tank spot 3.9 /
  0.0 / 16.0% of settled snapshots; uld3's rest is geometry, Thorim resting 4.6 yd south-south-west of the tank spot, where the best ring
  clears the off-tank spot by 8.0 yd. Holder within 8 yd of a ring body 10.0 / 26.3 / 40.7% (≤ ~35% asked: uld3 over); moves per ring bot a
  minute 3.8 / 4.0 / 5.6; melee on Thorim to +100 s 100 / 100 / 99.7%.
- **Chains through the tanks:** none lethal. One in uld2 (5:34.6, normal mode): three slot 2 melee, then holder 13k and waiting tank 16.6k,
  all at full health. Not the anchor: a lit orb (5:33.8) turned slot 2's solved bearing 56° → 78°, which the 60° anchor separation allows,
  and that point stood 7.5 yd from the holder. uld3's one Chain Lightning death (3:38.7) hopped 8.9 yd from the RANGE2 camp into slot 1,
  before any re-latch, no tank in it.
- **Lane followers: held.** `leadstall.py` in-lane-behind spans gone: 0.0 / 1.8 / 1.5 s, all on waypoint 0 with the squad still walking
  from the gate (w19 7.5 / 2.7 / 1.0 with followers on earlier waypoints). Corridor trash 40.7 / 43.3 / 45.5 s (w19 45.7 / 61.5 / 40.2). No
  phase 1 death in any pull.

**Gauntlet timeline** (seconds per segment from the lever; budget lever → Sif's deadline 170.0 / 170.0 / 169.4 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w21 uld1 | 0.09 | 17.0 | 40.7 | 14.0 | 35.5 | 7.0 | 48.0 | 20.8 | 183.0 | +13.1 |
| w21 uld2 | 0.05 | 13.6 | 43.3 | 8.5 | 36.7 | 7.3 | 56.2 | 24.0 | 189.6 | +19.6 |
| w21 uld3 | 0.58 | 15.0 | 45.5 | 7.5 | 24.3 | 3.5 | 47.0 | 22.9 | 165.6 | made by 3.8 |
| w19 uld1 / 2 / 3 | 0.1-0.2 | 12.3-17.2 | 45.7 / 61.5 / 40.2 | 9.5 / 1.5 / 5.8 | 30.5 / 36.5 / 24.0 | 4.8-10.9 | 51.2 / 55.8 / 53.2 | 21.5 / 23.2 / 20.8 | 180.5 / 206.5 / 161.9 | +10.6 / +36.7 / made by 8.0 |

uld1's 14.0 s to the Colossus: a Runic Smash flip (1:34.8) took the leader from the right lane's last waypoint across 30 yd to the
left's, then 4.3 s to his stop and 1.5 s there. The Colossus took 35.5 / 36.7 s in uld1/uld2 with the squad walking in from 67-69 yd.

**uld3's hard-mode wipe: the race again.** First tank death at P2 +101.7 s (4:53.9, bear Uldzab: nine Thorim swings, 98.8k in 10 s, Thorim
56.7%), healers alive at 28-53% mana; Thorim lost 0.37%/s (hard-mode pulls before: 0.41-0.45). Six melee died before it, by P2 +78.6 s:
Sif's Volley 2 and Frostbolt 1, Chain Lightning 1, Blizzard 2. The waiting tank took Blizzard 49 hits (195k) against the holder's 28 (95k),
the round 8 spot's cost (w15 uld1 45 / 21).

**Seen, not picked: the ring under Blizzard.** At 4:24.6 Blizzard covered slot 0's latched 176° with Thorim south-west of the tank spot,
beside Sif's track; the solver's relax 1 (slot separation dropped) moved it to 134° (4.6 yd from the off-tank spot), then 308°, slot 1's
own bearing, within 0.5 s. Off their arrived latch the three melee lost the movement guard, and generic `avoid aoe` walked them in 3-5 yd
hops still inside the zone, over the tank spots (Uldzag 100 → 72 → 38 → 18%, eight ticks, 44k); the Volley at 4:30.4 and the next ticks
killed all three. One pull; which constraint gives way when Blizzard covers the ring is a tactic. If a second hard-mode pull shows it, ask:
may a covered slot step out past the ring to a clear spot until the 10 s zone fades, instead of merging into another slot or the tank side?

**Stop rules:** not polished (one hard-mode pull, a wipe). Not stuck: rounds 3-14 picked twelve different causes; round 13's held.

**Failure ranking:** the uld3 hard-mode race, 32 deaths (not a bug: rounds 6 and 9); no phase 1 death. On Sif's clock, missed by 13.1 /
19.6 s on the normal pulls: the Colossus walk-in (the user's answer, built here), the lane flip at the last waypoint (30 yd).

**Change: the user's lane waypoint** (Thorim files only, no size, mode or player branch).
- **Where the floor is.** Between y −344 and −366 the corridor floor narrows to x ~2216-2240 (`finegrid.py`); x 2212 at y −360 is off it.
  Every deep sink in the 46 traces (five, to z 388 down to −28) was a westward `flee` or barrier bail from x 2215-2220 aimed past that
  edge (`sink.py`). So the pair sits where players stood and leaders stopped for the pull:
  - left `ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_2_YARDS_1` **(2237.20, −360.50, 412.1348)**: 1.9 yd from the uld1 leader's stop (2235.3, −360.52);
    a pet stood on it, players walked through (2239.4, −360.1); 124 player samples within 2 yd, none below the floor.
  - right `ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_2_YARDS_1` **(2219.50, −360.57, 412.1348)**: the uld2 leader's stop; ranged stood within
    1-2 yd for minutes ((2220.41, −361.33), (2221.4-2221.7, −360.7)); 2405 samples within 2 yd on the floor (the 9 below are one bot
    already fallen under at (2219.1, −353.8)).
  - 36.97 / 36.50 yd from the Colossus spawn (`GetDistance` ~29.5 / 29, inside the 30 yd pull test), not 39: north of y −358 the right
    lane strip between the floor edge and the left hand's blast is under 2 yd wide. 17.7 yd apart, so a dodge there crosses 17.7 yd
    instead of the last pair's 30.
- **Radius 2 yd.** A smash reaches 10 yd plus the target's reach (`smashreach.py`: 605 hits, farthest 11.3 yd 2D), and the lane test is
  net of the bot's reach (3.5 yd exact, ~4 under Heroism). Left clears the right hand's nearest bunny by 16.0 yd, right the left's by
  15.7, so a bot at the disc's edge stays clear; at 10 yd both discs would cover the centre line, which both rows hit, and a bot there
  would read as in either lane and never dodge.
- **Gather before the pull** (`ThorimGauntletSquadWithin`, the lead step's cohesion loop factored out): the leader now pulls from the
  new pair, so `ThorimGauntletLeadColossus` also waits until every squad non-healer bot is within 20 yd
  (`ULDUAR_THORIM_LEAD_COLOSSUS_GATHER_DISTANCE`), under the step's 15 s cap from his arrival. The step's 30 yd would pass at once:
  followers trail 25 yd behind and only take his waypoint once he stands on it. Followers on the other lane's waypoint are 17.7 yd off.
- **No follower opens the idle Colossus.** `GetThorimDpsTarget` offers an out-of-combat Colossus only through
  `ThorimGauntletColossusPull` (the bot leader, at his stop, trash clear, squad gathered): the new pair is inside every follower's 50 yd
  pick, which would otherwise pull him onto a DPS during the gather (before round 10 a follower's pick did, 1-10 s late). A real player
  leading pulls him himself, as with any lead job; a non-tank fallback leader pulls through the same gate.

`pb-check` passes (syntax over the 14 including TUs, pblint, codestyle).

**Coordinates to navprobe before the merge:** both points on the mesh at z 412.13; the paths left lane last waypoint (2242.1904, −329.0533)
→ left new, right lane last waypoint (2212.1956, −328.0144) → right new (its straight line skims the floor edge at x ~2217, y −345..−355
where bots fell: no path segment under z ~410), left new ↔ right new (the dodge), and each new point → the Colossus spawn (2227.5,
−396.18, 412.18).

**Next window should show:**
- After pack 2's death, a `thorim gauntlet lead action` move to (2237.20, −360.50) or (2219.50, −360.57); followers' `thorim gauntlet
  positioning action` moves to the same index in the safe lane once he stands on it; his Colossus attack only once every squad
  non-healer is within 20 yd (≤ 15 s after his arrival).
- The Colossus' first victim the gauntlet leader; no follower targeting him before his engage.
- Squad damage on the Colossus over 10k/s within ~2 s of the engage (6-9 s now), the Colossus segment from 31-37 s toward ~25 s; "to
  Colossus" up to ~4 s longer for the gather; lever to phase 2 ~3-5 s shorter, so hard mode on more than the fastest pool.
- No smash hit on a bot standing on the new pair; no bot under z 409 near it.
- Watch: the Colossus aggroing on the squad at 36.5 yd (nobody stood inside 37.5 yd of him idle before); a Colossus tanked within 3.5 yd of
  the pair after his Charge pulling off-lane melee back to it (the formation's off-lane walk has no fighting gate).

**Questions for the user:** none new (the ring-under-Blizzard question waits for a second hard-mode pull).

**Review (round 14).** The cause holds: at the Colossus' first health drop the w21 uld1/uld2 squads stood 67-69 yd from him (the
y −329 waypoints sit 68.7 / 69.9 yd from his spawn; earlier rounds' 55-65 is about their `GetDistance`) and he lost 1.5 / 2.1% in the
first 6 s, against 9.8% in uld3, whose squad had already walked to 40-48 yd; w16 the same (56-98 yd, 1.4-5.7%). Leader stops
(2235.3, −360.52) and (2219.44, −360.29); the uld2 leader walked right waypoint 5 to the new right point at z 412.13 (x 2216-2219.6 over
y −337..−355). Within 1 yd of the new left point 33 floor samples in 8 pulls, none below (floor to x 2239.8 at y −360); of the right
235, 8 below from one fallen bot. Smash reach rerun: 605 hits, farthest 11.34 yd. No size, mode or player branch (the gather counts
squad bots like the step's cohesion check; a real leader pulls him himself); Thorim files only; no comments. The user's x ~2242 / ~2212
at ~39 yd moved onto the floor and inside the 30 yd test, within the answer's "picked from trace positions". In the 30 Colossus fights on
disk the leader never stood within 3.5 yd of either new point, so the off-lane walk without a fighting gate stays a watch. `pb-check`
passes. Fixed: the findings doc credited the trace-picked coordinates to the user and gave 55-65 yd for the ~69 yd of the old last
waypoints.

Left: the answer's "navprobed" is still open (no Docker in a lane or review): both points and the three paths above before the merge.
The leader alone still walks the 16-18 yd between the last two lane discs (y ~−340 to −357) with no lane index, so with no smash dodge,
as on the old walk to his stop; followers cross it on his index once he stands on the new point.

## Round 15 (window 23: build e440e6f17 with round 14)

**Data:** window 23, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-205728-1` (uld1),
`-2` (uld2), `-3` (uld3); traces `env/dist/logs/botobs/603_{1,2,3}_thorim_{1795467529,1795467529,1795467538}.ndjson`, read through
`raidtest.py report --boss thorim --raid uld --head e440e6f17 --pulls 10` (its `thorim.py` lines), `postmortem.py`, round 7-14 scripts
(`budget.py`, `dps.py`, `pack2.py`, `leadstall.py`, `smashcost.py`, `smashhits.py`, `corridor.py`, `walkin.py`, `leadstop.py`, `p2heal.py`)
and new ones in session scratchpad `thorim_r15/` (`r14checks.py`, `sifclock.py`, `barrier.py`, `p2rank.py`, `tankdeaths.py`, `p2open.py`,
`opening.py`, `hpfine.py`, `victim.py`, `immune.py`, `dealt.py`, `bosscasts.py`, `swap.py`, `flat.py`, `ringbliz.py`, `meleebliz.py`) over
`snap`, `move`, `act`, `cast`, `aura`, `dmg`, `heal` and `note` rows. Hard-mode phase 2s compared: w14 uld1
(`603_3_thorim_1795427707`), w15 uld1 (`603_8_thorim_1795436805`), w19 uld3 (`603_5_thorim_1795452388`), w21 uld3
(`603_4_thorim_1795459237`); all 31 Thorim phase 2s on disk for the untouchable scan.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795467529 | wipe 6:13, **hard mode** (62507 0:51.0, no 62565, `thorim.sifjoined`; phase 2 3:00.0 against 3:21.0) | phase 2: Thorim melee on a non-tank 15, Blizzard 4, Chain Lightning 3, Thorim melee on a tank 3, Sif Frostbolt 2, Frostbolt Volley 2, Unbalancing Strike 2, Lightning Charge 1 |
| uld2 | 1795467529 | kill 6:21, normal (62565 3:20.3, phase 2 3:55.8) | phase 1: Iron Ring Guard Impale 1 (ranged Uldyau, 0:56.8), Runic Smash 2 (ranged Uldyas 1:02.6, Uldyau's second life 1:18.7) |
| uld3 | 1795467538 | kill 5:48, normal (62565 3:17.4, phase 2 3:31.7) | none |

**Round 14 verdict: the pull line held, the Colossus now runs on Runic Barrier.**
- Gather: at the leader's walk-in move (1.1-1.2 s after he stood on the new right point: 1:28.3 / 1:44.7 / 1:39.5) every squad
  non-healer stood 15.6-22.1 yd exact from him (inside the 20 yd `GetDistance` test), healers 15.1-17.0.
- No follower opened the Colossus: nobody but the leader targeted him before his first health drop; his first swing went to the
  leader in all three.
- Walk-in: the squad stood 37-45 / 48-54 / 47-55 yd from him at his first health drop (w21 uld1/uld2: 67-69) and he lost 12.0 / 7.7 /
  7.9% in the first 6 s (w21 1.5 / 2.1 / 9.8%).
- "To Colossus" 11.2 / 10.5 / 7.0 s (w21 14.0 / 8.5 / 7.5); the Colossus 20.2 / 37.8 / 33.3 s (w21 35.5 / 36.7 / 24.3): shorter only in
  uld1. What decides it is Runic Barrier (62338, −51% damage taken, 20 s): it landed at +11.7 / +12.0 s with him at 67 / 60% in uld2 /
  uld3 (w21 at +11.2-17.7 s, 21-81%), and the rest took 21-26 s; uld1 killed him in 20.2 s before any barrier, the only Colossus of six
  without one.
- Falls: no player under z 409 within 20 yd of either new point; the only corridor players under z 400 were uld2's two healers at the
  ramp foot (2217.3, −422.5) at 2:37.9, the known flee sink.

**Gauntlet timeline** (seconds per segment from the lever; budget lever → Sif's deadline 169.9 / 169.7 / 169.8 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w23 uld1 | 0.12 | 12.6 | 36.2 | 11.2 | 20.2 | 3.5 | 44.5 | 20.6 | 148.9 | made by 21.0 |
| w23 uld2 | 0.25 | 14.2 | 51.2 | 10.5 | 37.8 | 5.2 | 62.0 | 24.4 | 205.3 | +35.6 |
| w23 uld3 | 0.23 | 14.1 | 52.5 | 7.0 | 33.3 | 5.2 | 51.0 | 21.0 | 184.2 | +14.4 |
| w21 uld1 / 2 / 3 | 0.05-0.58 | 13.6-17.0 | 40.7 / 43.3 / 45.5 | 14.0 / 8.5 / 7.5 | 35.5 / 36.7 / 24.3 | 3.5-7.3 | 48.0 / 56.2 / 47.0 | 20.8 / 24.0 / 22.9 | 183.0 / 189.6 / 165.6 | +13.1 / +19.6 / made by 3.8 |

uld1 is the fastest lever-to-phase-2 on record (156.2 s before, w12 uld3). Against it:
- **uld2 +56.4 s:** corridor +15.0 (pack 2 opened 28.8 s after pack 1, uld1 18.7: Impale killed a ranged at 0:56.8 and Runic Smash
  another at 1:02.6; 12 smash hits, and 16 of uld2/uld3's 25 landed at y −300..−310 in the right lane, where pack 1 is fought), the
  Colossus +17.6 (barrier at +11.7 s, two ranged dead: 27k against 52k), adds and Giant +17.5 (62.0 s, two DPS down), balcony +3.8.
- **uld3 +35.3 s:** corridor +16.3 (pack 2's Acolyte took 22 s with eight DPS on it at 2-8k/s while the squad crossed lanes under the
  smash, and healed itself 63 → 77%; 13 smash hits), the Colossus +13.1 (barrier at +12.0 s), adds and Giant +6.5.
- uld1 took one smash hit, no corridor death and no barrier. The latch held through every cast (`smashcost.py`: 26 of 26 hits).
Not picked: the corridor under Runic Smash and Runic Barrier luck; the orchestrator asked for the hard-mode phase 2 this round.

**Hard-mode phase 2, third wipe in a row** (five pulls since window 14; tank intake and mana from `p2rank.py`, `p2heal.py`):

| Pull | first tank death | Thorim then | healer mana then | non-tank deaths before it | Thorim %/s 0-30 s / 30-60 / 60-90 / 90-120 |
|---|---|---|---|---|---|
| w14 uld1 | +82.4 s, melee | 65.4% | 47% | Chain Lightning 2, Blizzard 1 | 0.20 / 0.59 / 0.49 / 0.53 |
| w15 uld1 | +103.1 s, melee | 53.5% | 38% | Lightning Charge 2 | 0.29 / 0.55 / 0.48 / 0.45 |
| w19 uld3 | +57.1 s, one Chain Lightning (round 13 fixed it) | 75.7% | 55% | Chain Lightning 2 | 0.31 / 0.54 / 0.44 / 0.35 |
| w21 uld3 | +101.7 s, melee | 56.8% | 42% | Volley 2, Blizzard 2, Chain Lightning 1, Frostbolt 1 | 0.28 / 0.53 / 0.45 / 0.41 |
| w23 uld1 | +131.0 s, melee | 58.4% | 26% (one healer dead) | Frostbolt 2, Blizzard 2, Chain Lightning 1, Volley 1, Lightning Charge 1 | **0.00** / 0.28 / 0.44 / 0.48 |

What kills the tanks (20 tank deaths): Thorim's melee 13, Unbalancing Strike 4, Chain Lightning 1, Volley 1, Blizzard 1. His swings on a
tank grow with the Lightning Charge stacks, median 6.6-9.2k over the first 30 s to 11.5-14.2k by +90-150 s (max 16.8-30.4k), and a tank
dies 3.7-16 s into a hold, 100% → dead in 2-3 swings. Tank intake from phase 2 to the first tank death: Thorim melee 36-51%, Blizzard
18-27%, Unbalancing Strike 6-13%, Volley 6-9%. What kills the raid before the tanks: Chain Lightning 6, Blizzard 5, Lightning Charge 3,
Volley 3, Frostbolt 3; after them Thorim loose on 15-24 a pull.

Ranked:
1. **The damage race.** Thorim stands at 53-76% when the first tank falls. The raid deals 130-145k/s on him from +30 s (0.44-0.59%/s),
   half that in the first 30 s while the corridor squad comes off the balcony (0-25k/s from its nine DPS until +20 s, by design) and the
   camp clears Evokers; at that pace he needs ~200 s of floor against tanks falling at +82-131 s. Not a bug.
2. **w23 uld1: Thorim untouchable for 25 s (bug, fixed below).** The raid dealt 56-68k/s to +15 s, then 0.4-5.7k/s from +17 to
   +41.5 s and 130k/s after; he went 97.8% → 100.0% at +23.3 s and held there.
3. **The melee ring under Blizzard** (round 14's watch, now in a second hard-mode pull): w23 uld1 melee took 55 Blizzard ticks for 248k
   with 3 melee deaths (w21 uld3: 21 for 102k, 2 deaths; w14-w19 0-13). Each time slot 0's solved bearing swung 207 → 183 → 159 → 281°
   (4:11.0-4:12.5) and 193 → 169 → 283° (5:27.7-5:28.8) inside 1.5 s; off the arrived latch, generic `avoid aoe` walked them in 3-5 yd hops
   through two overlapping zones (double ticks, 10.3k a second). Tactic: question 1.
4. **Blizzard on the tanks**, 236-298k a pull since round 8's spot (w21, w23; holder 28-83k, waiting tank 74-197k before the first tank
   death). The waiting tank still takes Thorim at 79-100% health, so it taxes healing, not the hold. Not picked.
5. **Sif's Volley and Frostbolt**: 50-57% and 14-22% of non-tank intake before the first tank death; the Volley is uninterruptible and Sif
   unselectable, so nothing kicks either. Nothing to do.
6. **Healer mana**: 26-55% at the first tank death in all five; not the binding limit here (window 12's two pulls were).

**Failure picked: Thorim's cannot-reach latch (w23 uld1).**
- Trace: the corridor tank Uldxam came down from the balcony (Thorim's victim from +9.4 s, mid-jump at z 430) and dragged him onto the
  tank spot; at +16.5-16.7 s Thorim stopped 2.4 yd from him while he was still walking in, and from then until +41.7 s Thorim's health
  did not fall with 9 melee, 8 ranged and both tanks on him, Uldxam standing still on (2110.75, −252.65) the whole time. Uldxaa's swap
  taunts at +26.95, +33.16 and +39.16 (`thorim unbalancing strike swap action` OK, then FAILED, victim unchanged) did nothing, so Uldxam
  held him 38 s through a landed Unbalancing Strike. Thorim's Chain Lightning, due at +26.4 s, never cast; the next, at +41.57 s (cast time
  500 ms), ended it, and damage resumed within 0.2 s. Cost: ~25 s at zero plus 2.2% regenerated, about 29 s of the race (at the first tank
  death he would have stood near 44%, not 58%: still a wipe).
- Core: `ChaseMovementGenerator` flags the victim unreachable when a chase path fails (`TargetedMovementGenerator.cpp:177`), and
  `Creature::IsEvadingAttacks` (`Creature.h:138`) is then true, so every hit and taunt evades; 5 s on, `IsEvadeRegen`
  (`CombatManager.h:128`) regenerates him. The flag clears only on a dispatched chase move (`TargetedMovementGenerator.cpp:194`), a finalised one, a lost target or a cast
  that stops him; with the victim standing still inside his reach (8.25 yd exact: contact 0.5 plus 6.25 and 1.5), `PositionOkay` holds
  and nothing re-paths. Mimiron's MK II has the same latch, fixed bot-side (`mimiron unreachable step action`).
- Rare: 1 of the 31 phase 2s on disk shows a held stretch of 3 s or more (`flat.py`, and the reader's new line over every trace).

**Classification: bug.** The strategy holds Thorim on the tank spot for the raid to burn; a latched flag voids the hits and the swap.

**Fix** (Thorim files only, no size, mode or player branch):
- `ThorimUnreachableBy` (`UldEncounter_Thorim.cpp:3113`): phase 2, Thorim alive and in combat; reads `GetCannotReachTarget()` into the new
  probe `thorim.unreach` (stamped when it changes) and returns him when the flag has named this bot for
  `ULDUAR_THORIM_UNREACH_GRACE_MS` (1 s) and the bot is below the floor line (a balcony bot's answer is its jump).
- `ThorimUnreachableStep` (`:3138`): a point his `GetCombatReach()` plus the bot's plus `ULDUAR_THORIM_UNREACH_STEP_MARGIN` (4) from his
  centre (11.75 yd), straight away from him, else turned in 30° steps up to 120° until it lies within `ULDUAR_THORIM_UNREACH_ARENA_RADIUS`
  (30) of the arena centre (the off-tank spot sits at 29.65), preferring one clear of Blizzard; nothing once the bot stands 2 yd short of
  it or farther. (Review: each point is also pulled back onto the navmesh and must still stand 9.75 yd from him.)
- `ThorimUnreachableTrigger` / `ThorimUnreachableStepAction` (`UldTriggers_Thorim.cpp:189`, `UldActions_Thorim.cpp:467`) at ACTION_RAID + 3,
  `MOVEMENT_FORCED` (a combat-priority positioning booking cannot refuse it), backing out while facing him; both `thorim movement guard` and
  `thorim tank anchor guard` pass it. The chase re-paths as soon as the bot leaves 8.25 yd, which clears the flag; the positioning row then
  walks it back. A human the flag names is not moved, as with any job a human does.
- Reader: `thorim.py --phase2` prints `untouchable`: the flag's spans, whom they named, step moves, and stretches of 3 s or more a tank
  held him without his health falling (works on old traces; w23 uld1 reads "+16.5..+41.7 s 97.8% to max 100.0%").

`pb-check` passes (syntax over 41 TUs, pblint, codestyle, botobs tests).

**Next window should show:**
- `thorim.unreach` in every phase 2 (a "0" note at least); any span naming a floor bot past 1 s followed within ~0.5 s by a
  `thorim unreachable step action` move from it and the flag back to 0 within ~2 s of the step; the reader's held-without-losing-health
  stretches at 0 (w23 uld1: 25.2 s).
- Watch: step moves by a holder more than a few a pull (a flag that flickers, then raise the grace); Thorim dragged off the tank spot
  by a step and the ring re-latching behind it; a step walking a holder into a Blizzard zone or the off-tank spot.
- Gauntlet: hard mode only on a fast pull with no barrier luck; the corridor (Runic Smash hits 12-13 against 1) is next round's candidate.

**Seen, not picked:** the blood DK holder channelled Army of the Dead for 4 s on Thorim (w23 uld1 +22.7-26.7 s, during the latch), shared DK
class code.

**Questions for the user** (tactic, not built):
1. **The melee ring under Blizzard.** When Sif's Blizzard covers a melee slot, the solver gives up the slot's spacing and swings it onto
   another slot or the tank side within 1.5 s, and generic `avoid aoe` then walks the melee in short hops through the zones: 248k and 3
   melee deaths in w23 uld1, 102k and 2 in w21 uld3. May a covered slot's melee instead step straight out past the ring to the nearest point
   11 yd clear of every zone (about 3-6 yd farther from Thorim, out of his melee reach for up to the zone's 10 s) and walk back once it
   fades, with `avoid aoe` held off ring melee in phase 2? Recommended: yes; the uptime it costs (a covered slot sits out up to ~10 s of a
   36-41 s Blizzard cycle) is less than the melee lost to it now.

**Review (round 15).** The cause holds. In w23 uld1 Thorim's health stops at 97.79% at +16.7 s, the holder stands on the tank spot from
+17.0 s with Thorim 3.6 then 4.8 yd off him, health jumps 97.77 → 100.00% at +23.0-23.3 s (the 5 s regen delay plus a 2 s regen tick after
a flag set near +16.6 s), all three swap taunts land OK then FAILED with the victim unchanged, and his +41.6 s Chain Lightning (500 ms cast)
ends it; only a set cannot-reach flag gives all of that. Not explained: at +20.4-20.7 s he moved 1.2 yd under his chase (moving 1, move
generator 5) without the flag clearing, since the regen still came on the first timer; the new probe will show such moves. The reader
over the 31 phase 2s on disk finds the one held stretch and no other. No size, mode or player branch; Thorim files only; no comments.
Fixed: the step trusted the 30 yd disc as floor. Thorim lands 2.7-6.4 yd from the centre on the balcony side (bearing ~260°) and the
corridor tank lands ~15 yd out there, while across 49 traces no player stood on the floor past 22.5-26.6 yd at bearings 255-315°, so the
first case the step answers could aim into the wall under the balcony. Each point now takes the ground height and
`CheckCollisionAndGetValidCoords`, as `RingPoint` does, and must still stand 9.75 yd from him; the constant is renamed
`ULDUAR_THORIM_UNREACH_ARENA_RADIUS`, and the fix line above no longer says his reach doubled. `pb-check` passes.

## Round 16 (window 24: build baf47824b with round 15)

**Data:** window 24, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261123-220519-1` (uld1),
`-2` (uld2), `-3` (uld3); traces `env/dist/logs/botobs/603_{1,2,3}_thorim_{1795471607,1795471611,1795471614}.ndjson`, read through
`raidtest.py report --boss thorim --raid uld --head baf47824b --pulls 10` (its `thorim.py` lines), round 7-15 scripts (`budget.py`, `seg.py`,
`dps.py`, `leadstop.py`, `leadstall.py`, `pack2.py`, `smashcost.py`, `barrier.py`, `sifclock.py`, `p2rank.py`, `meleebliz.py`, `chain.py`) and
new ones in session scratchpad `thorim_r16/` (`leadtl.py`, `squadat.py`, `pets.py`, `revive.py`, `othertgt.py`, `sinkport.py`, `flees.py`,
`zgrid_top.py`, `opencl.py`, `opening.py`, `blizdeath.py`) over `snap`, `move`, `act`, `cast`, `aura`, `dmg`, `death` and `note` rows; all 36
Thorim traces on disk with a lever for the corridor flees, teleports and Revive Pet casts, all 34 phase 2s for the opening Chain Lightning.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795471607 | wipe 4:58, **hard mode** (62507 0:47.4, no 62565, `thorim.sifjoined`; phase 2 3:08.4 against 3:17.4) | phase 2: Thorim melee on a non-tank 10, Chain Lightning 7, Blizzard 6, Thorim melee on a tank 3, Frostbolt Volley 2, Lightning Charge 2, Frostbolt 1, Unbalancing Strike 1 |
| uld2 | 1795471611 | kill 6:10, normal (62565 3:30.0, phase 2 3:54.1) | phase 2: Chain Lightning 1 (a healer on a camp spot, P2 +27 s) |
| uld3 | 1795471614 | kill 6:04, normal (62565 3:18.1, phase 2 3:50.1) | phase 2: Chain Lightning 2 (melee, the opening cast at P2 +11.3 s) |

**Round 15 verdict (cannot-reach step): not triggered, no side effect.** `thorim.unreach` noted "0" in all three phase 2s and never named
anyone; no `thorim unreachable step action` move; the reader's held-without-losing-health stretches 0 in all three (w23 uld1 25.2 s). The
latch showed once in 31 phase 2s before, so one window cannot judge it; its checks stand for the next.

**Gauntlet timeline** (seconds per segment from the lever; budget lever → Sif's deadline 169.9 / 169.8 / 169.6 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w24 uld1 | 0.15 | 12.5 | 41.8 | 13.0 | 18.3 | 4.3 | 47.3 | 23.9 | 160.9 | made by 8.9 |
| w24 uld2 | 0.16 | 21.6 | 39.3 | 29.7 | 20.2 | 4.6 | 55.7 | 23.0 | 194.0 | +24.2 |
| w24 uld3 | 0.37 | 15.0 | 62.7 | 9.8 | 32.5 | 5.6 | 54.0 | 22.4 | 201.8 | +32.2 |
| w23 uld1 / 2 / 3 | 0.12-0.25 | 12.6-14.2 | 36.2 / 51.2 / 52.5 | 11.2 / 10.5 / 7.0 | 20.2 / 37.8 / 33.3 | 3.5-5.2 | 44.5 / 62.0 / 51.0 | 20.6 / 24.4 / 21.0 | 148.9 / 205.3 / 184.2 | made by 21.0 / +35.6 / +14.4 |

**Why uld2 and uld3 ran late** (against uld1, which took 4 Runic Smash hits, no Runic Barrier and lost only its two healers to the
teleport below, which the leader's step hold ignores):
- **Corridor bots teleported to the arena (uld2 4 bots, uld3 5, uld1 2).** A ranged or healer `flee` from the lane waypoints at y −276 and
  −295 sinks under z 410 and `ThorimFallFromFloorTrigger` (`UldTriggers_Thorim.cpp:144`) `NearTeleportTo`s it onto the raid leader in the
  arena (`UldActions_Thorim.cpp:406`), 13-22 s from its squad. uld3 lost three of its four ranged and both healers at 0:58.5-1:04.0, mid
  pack 1: pack 1 took 24.9 s (uld1 16.3), and the squad dealt ~6k/s over pack 2's first 10 s, crossing lanes under the smash while they
  caught up; uld2 lost two ranged and both
  healers at 1:15.0-1:15.5, as pack 1 died: pack 2 opened 9.0 s after it (uld1 3.4 s). Cause below.
- **uld2's squad hunter recasting Revive Pet (~16.5 s).** Uldyau's pet never comes back: 982 completes (its 3 s aura lands) with no pet,
  and `PetIsDeadValue` (`StatsValues.cpp:45`) reads his `character_pet` row as a dead pet, so he recasts it all fight: 7 casts (63 s)
  before phase 2, 4 (30 s) in it; arena hunter Uldyaj 5 / 8. Roster B's two hunters have done this in all 12 uld2 Thorim pulls on disk
  (squad hunter 35-62 s of phase 1 in the pulls he lived through). His 0:36.1 cast, 40 yd off at the gate, held the leader's 30 yd step hold: first lead move 6.7 s after
  the lever (uld1 / uld3 0.2-0.3 s). His 1:56.2 cast, 32 yd off on the old last left waypoint, held the Colossus gather: the rest stood
  within 20 yd by ~1:57.5, the pull came at 2:07.3, with two Runic Smash lane flips of the leader in between.
- **uld3's Colossus +14.2 s:** Runic Barrier at +11.6 s, him at 64% (uld1, uld2: none).
- **Smash hits** 4 / 29 / 26, all with the latch held (`smashcost.py`); uld3's pack 2 opened in the smash lanes at 1:17.1 when the leader's
  walk from right waypoint 1 to 4 took him within 20 yd of its west guard, then a dodge carried him across both lanes with it on him.

**uld1's hard-mode phase 2** (`p2rank.py`): first tank death at P2 +47.7 s with Thorim at 84.7% (the five hard-mode pulls before: +57-131 s at
53-76%), DPS alive 19 / 17 / 8 / 8 / 1 at +0 / 30 / 60 / 90 / 120 s, Thorim losing 0.28 / 0.32 / 0.17 / 0.06 %/s. 17 non-tanks died first:
- **Chain Lightning 7.** The opening cast (3:19.8, +11.4 s), Thorim 12.6 yd off the tank spot and moving: corridor DK tank Uldxam took him
  off the balcony (+6.1 s), the main tank's swap taunt walked him to the tank spot (+7.4 s), and Uldxam took him back on threat (+10.4 s,
  no taunt row) from 17 yd south-east after channelling Army of the Dead (3:16.7, 4 s; w23 uld1 too), so the holder stood on the far side
  of him from the tank spot. 8 hops: three melee south, the holder, four melee east; Uldxax, Uldxah, Uldxay dead at 32.7k / 49.1k / 83.1k.
  Then two camp casts: +26.4 s seeded on a camp spot holding a ranged, a melee and a healer (Uldxaw, Uldxax dead), +41.5 s through three
  ranged on one camp spot (Uldxaq, Uldxas). The +11 s opening casts over the 34 phase 2s on disk: no death before window 24, then 3 (uld1)
  and 2 (uld3: eight melee strung between two slots during the drag, no tank in it).
- **Blizzard 6.** Slot 0's Uldxai and Uldxaf (3:39.2-3:45.3): its solved bearing swung 203 → 177 → 153 → 281° in 1.7 s as a zone came, the
  walk to 281° (15 yd round him) came back `wait` for ~5 s behind `avoid aoe`'s combat-priority bookings while both stood in two
  overlapping zones (10.4k/s); Uldxai's Reincarnation stood up in the zone and died 4 s later. Camp healer Uldxad and ranged Uldxak walked
  from home to their shelter (slot 5, orb 1) under the lit cone through two zones with a Volley (3:50.7-3:54.3), and Uldxad's Reincarnation
  died in the zone 0.9 s later. Melee 40 Blizzard ticks for 185k (w23 uld1 55 / 248k, w21 uld3 21 / 102k).
- Frostbolt Volley 2, Lightning Charge 2 (melee, 3:54.3).

**Stop rules:** not polished (no hard-mode kill). Not stuck: rounds 3-16 picked fourteen different causes.

**Failure ranking:**
1. **uld1's hard-mode phase 2 wipe, 32 deaths.** The damage race (rounds 6, 9, 15), lost early: 9 DPS dead by +60 s. Its leaks are the
   queued Blizzard tactic (re-check below), the camps stacked by construction (14 bots on 6 spots) and the opening chain (5 deaths over
   two pulls, new this window). No bug to fix here this round.
2. **uld2 and uld3 missed hard mode** (+24.2, +32.2 s): bugs, fixed below.
3. Chain Lightning in the two normal-mode kills, 3.

**Bug picked: corridor bots flee off the floor and the fall rescue sends them to the arena.**
- Trace (`flees.py`, `sinkport.py`, 36 pulls): 1072 accepted `flee` moves by gauntlet-squad bots in the corridor (~30 a pull, 680 by healers),
  174 dipping under z 411 within 3 s, 71 followed by a teleport into the arena; 47 bots ported from (2216-2218, −274..−296) and
  (2237-2238, −265..−295), 13-22 s each before they re-entered the corridor, in 14 of 36 pulls (w24 2 / 4 / 5 bots). Stationary players
  stand on the floor only at x 2216-2240 at y −276 and 2218-2238 at y −295 (`zgrid_top.py`); right waypoints 1-2 (2217.4, −275.9),
  (2217.9, −295.0) and left 1-2 (2237.2, −275.8), (2236.9, −294.6) sit 1-2 yd inside those edges. Uldzav (w24 uld3): `flee` 0:57.79 from
  (2218.1, −275.9) to (2212.4, −275.9), z 409.8 at 0:58.33, at (2139.9, −262.9) in the arena at 0:59.55, fighting arena adds, back in the
  corridor 13.5 s later.
- Code: `enemy too close for spell` (`RangeTriggers.cpp:14`) fires when the bot's target is within its melee reach and attacking someone
  else, which is every mob the leader tanks on the waypoint the squad shares; `FleeAction` (`MovementActions.cpp:2015`) moves
  `FleeDistance` straight away from it, off the edge. The rescue fires below z 410 within 110 yd of the arena centre, 2 yd under the
  corridor floor (412.1), set against the arena floor (419.5), and teleports to `GetRaidLeaderOnInstance`, the arena's group leader.
- **Classification: bug.** The squad is meant to hold its lane waypoint and walk with the leader; a generic step off the floor that ends 80
  yd back in the arena is no part of it. (Round 7 saw the flee sinks farther south and left them.)

**Second, independent and small: a gauntlet hunter's Revive Pet stalls the squad** (uld2 above). **Bug:** the step hold and the gather
are meant to wait for walkers on a 170 s clock, not for a 9 s cast. The petless roster B hunters are a harness fault (orchestrator: DC
should give them their pets back, or clear the stale `character_pet` row); the hold stands without it, since any hunter reviving between
packs stalls the squad the same way.

**Fix** (Thorim files only, no size, mode or player branch; rules run in bot engines only, so a real player is never held):
- `ThorimGauntletSquadOnClock` (`UldEncounter_Thorim.cpp:2170`): Thorim in combat on his balcony (`ThorimSplitActive`) and the bot in the
  gauntlet squad. `ThorimCorridorFleeHeld` (`:2175`) adds `PastThorimPortcullis`.
- `thorim corridor flee hold` (`UldDefinition_Thorim.cpp:185`): `Block` on `Family::Flee` while `ThorimCorridorFleeHeld`. Its triggers
  (`enemy too close for spell`, a hunter's `enemy too close for auto shot`, 63 of the 1072) fire on a mob attacking the bot itself only when
  it is rooted or frozen, so almost no escape from damage is lost; hunters keep Disengage, which already outranks their flee and in 58
  corridor casts sank no hunter without a flee after it.
- `thorim revive pet hold` (`:186`): `Block` on `revive pet` while `ThorimGauntletSquadOnClock`, from the pull to phase 2, gate included.
  Arena hunters and phase 2 unchanged.
- The rescue itself is unchanged: with the flee held nothing measured still reaches it in the corridor.

`pb-check` passes (syntax over 14 TUs, pblint, codestyle).

**Estimated gain on window 24:** uld2 ~16 s from the hold plus ~5 s at the pack 2 gap, ~173 s against 169.8; uld3 ~8-10 s (pack 1 and the
walk back), ~192 s, still ~22 s late on Runic Barrier and the smash. Hard mode on the pulls without barrier luck.

**Next window should show:**
- No `flee` move by a gauntlet-squad bot past the portcullis between the pull and phase 2 (w24: 28 / 11 / 33), `thorim corridor flee hold`
  veto rows instead; no squad bot teleported from the corridor into the arena (2 / 4 / 5); no corridor squad bot targeting an arena add.
- Roster B (uld2): no `revive pet` cast by its squad hunter before phase 2 (w24: 7, 63 s), `thorim revive pet hold` vetoes; the leader's first
  lead move within ~1 s of the lever; the Colossus pull within ~4 s of the leader reaching the pull line unless a smash flips him.
- Pack 2 opened within ~4-5 s of pack 1's last death where the leader fought pack 1 at waypoint 2 (uld2 9.0, uld3 8.8 s).
- Watch: Iron Ring Guard damage and deaths on corridor ranged and healers, who no longer step out of a tanked mob's reach (Whirling Trip,
  Impale), against window 24's none; a hunter stuck in melee reach of a tanked mob not shooting; a sink from a barrier bail.
- Round 15's checks again (`thorim.unreach`, step moves, held stretches).

**The queued Blizzard question still stands, recommendation yes.** Window 24 uld1 is the third hard-mode pull with it: the same solver swing
(203 → 281° in 1.7 s) and generic `avoid aoe` hops inside overlapping zones, 3 melee deaths, and the ring's escape walk refused `wait`
for ~5 s behind those hops' bookings, which holding `avoid aoe` off ring melee in phase 2 removes. Seen with it, not asked: a camp bot's
shelter walk under the lit cone crosses Blizzard by design, and here it crossed two overlapping zones (~11k/s) with a Volley, two deaths;
reincarnated and soulstoned bots stand up inside the zone that killed them.

**Seen, not picked:** the opening Chain Lightning (above; a candidate if it kills again); the corridor DK tank's Army of the Dead channel
while holding Thorim, the second pull running (shared DK class code); roster B's petless hunters in phase 2 (Uldyaj 8 casts, 65 s), which
the hold leaves to the harness fix.

**Questions for the user:** none new. The Blizzard question stays queued with the recommendation yes.

**User answer (2026-11-24) to the Blizzard question (round 15, re-asked in round 16):** yes. Melee covered by Sif's Blizzard
step straight out past the ring to a point 11 yd clear of every zone and walk back when it fades, and the generic dodge (`avoid
aoe`) is kept off ring melee in phase 2. Round 17 builds it.

## Round 17 (builds the user's Blizzard answer; round 16 merged, no pulls yet)

**Data:** window 24 (build baf47824b with round 15), read in round 16: `tr-20261123-220519-{1,2,3}`, traces
`env/dist/logs/botobs/603_{1,2,3}_thorim_{1795471607,1795471611,1795471614}.ndjson`; report `raidtest.py report --boss thorim
--raid uld --head baf47824b --pulls 10`. Stop rules, verdict and ranking skipped: round 16 has no pulls yet. For the Blizzard, the six
hard-mode phase 2s on disk: w14 uld1 `603_3_thorim_1795427707`, w15 uld1 `603_8_thorim_1795436805`, w19 uld3 `603_5_thorim_1795452388`,
w21 uld3 `603_4_thorim_1795459237`, w23 uld1 `603_1_thorim_1795467529`, w24 uld1 `603_1_thorim_1795471607`, through the new
`thorim.py --blizzard` and a per-slot replay in session scratchpad `thorim_r17/` (`stepsim2.py`: one body per slot at 7 yd/s, zones
from the snapshots' `hz` rows plus the bunny, floor from traced player positions, `westfloor.py`).

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795471607 | wipe 4:58, **hard mode** (phase 2 3:08.4) | phase 2: Thorim melee on a non-tank 10, Chain Lightning 7, Blizzard 6, Thorim melee on a tank 3, Frostbolt Volley 2, Lightning Charge 2, Frostbolt 1, Unbalancing Strike 1 |
| uld2 | 1795471611 | kill 6:10, normal (62565 3:30.0) | phase 2: Chain Lightning 1 |
| uld3 | 1795471614 | kill 6:04, normal (62565 3:18.1) | phase 2: Chain Lightning 2 |

**Ring melee under Blizzard before the change** (`--blizzard` "all" line; ring bot-seconds within the 9.8 yd damage reach, longest
stay, ticks, Blizzard deaths):

| Pull | in a zone | longest stay | ticks | deaths |
|---|---|---|---|---|
| w14 uld1 | 7.6 bot-s | 1.2 s | 0 | 0 |
| w15 uld1 | 19.9 | 5.6 | 13 (46k) | 0 |
| w19 uld3 | 17.9 | 3.3 | 6 (29k) | 0 |
| w21 uld3 | 19.7 | 6.0 | 21 (102k) | 2 |
| w23 uld1 | 50.5 | 10.0 | 55 (248k) | 3 |
| w24 uld1 | 32.3 | 8.7 | 40 (185k) | 3 |

**Change (the user's answer;** Thorim files only, no size, mode or player branch: a real player's melee is never moved, and the step
counts nobody's body):
- `SolveRingBearings` (`UldEncounter_Thorim.cpp`) no longer weighs Blizzard: relax 0 is cone plus 60° separation, relax 1 the cone
  only (`RingBearingClearOfBlizzard` deleted). That relax was the swing onto another slot or the tank side.
- `ThorimRingBlizzardCovered`: the slot point within 11 yd (`ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE`) of a zone or the bunny, or, for
  a bot standing clear more than 3 yd off it, the straight walk there passing one. Each change flips the new probe `thorim.ringstep`
  (`ObsGuidSet`) and drops `ringArrived`, so the walk back takes the 3 yd tolerance (with the 5 yd one a bot 3-6 yd out would stay
  out of reach). The positioning trigger and action stand down for a ring bot while it holds.
- `ThorimRingBlizzardStep`, run by `ThorimSifBlizzardTrigger` / `ThorimSifBlizzardAction` for ring melee at `MOVEMENT_FORCED`: nothing
  while the bot stands clear of every zone and the lit cone (it holds there); else the nearest point 11 yd clear of every zone, out of
  the cone (ring margin), in the arena box, ties to the one nearest the slot point, from
  `FindNearestPositionClearOfHazards` (navmesh-checked): past the ring and 8 yd off both tank spots within 15 yd
  (`ULDUAR_THORIM_RING_BLIZZARD_STEP_RADIUS`), else past the ring within 15, else anywhere within 20
  (`ULDUAR_THORIM_RING_BLIZZARD_LAST_STEP_RADIUS`).
- Rules (`UldDefinition_Thorim.cpp`): `thorim ring aoe hold` blocks `AvoidAoe` for ring melee on the floor in phase 2 (the user's
  "generic dodge kept off"); `thorim ring blizzard step` blocks `Reach` and `Charge` while stepped (else `reach melee` walks the bot
  back into the zone); `thorim movement guard` counts a stepped bot as settled and no longer passes `AvoidAoe`.
- Reader: `thorim.py --blizzard` (and in every report), per Blizzard: each slot point's covered seconds; ring bots within the 9.8 yd
  reach with seconds, stays, longest stay, ticks and damage; deaths; step moves, stepped bot-seconds and those past 9.0 yd of him; the
  walk back from each step's end to within 9.0 yd and ticks on the way. Test `RingBlizzard` in `test_thorim.py`.

**Why the fallbacks.** The literal rule (past the ring and clear of the tank side) found no point for 14-79% of the covered time in the
replay: slot 0 sits south-west of him between the west wall (x ~2100, `westfloor.py`), the tank spots and the trail's corner, and
that replay left melee in zones 18-87 bot-s a pull against 7.6-50.5 as fought. Dropping the tank clearance, then the ring, closes
it. Replay of the built rule, per pull as the table above: in a zone 4 / 2 / 0 / 0 / 6 / 3 bot-s; melee in reach of him 86.1 /
81.0 / 81.3 / 85.6 / 90.5 / 81.6% against 87.5 / 88.3 / 84.4 / 88.3 / 92.2 / 80.1% as fought (up to 7 points less: the uptime the
question priced); walk back p50 0.7-2.5 s, max 4.5 s; 16-50 step orders a pull.

`pb-check` passes (syntax over 16 TUs, pblint, codestyle, botobs tests).

**Next window should show** (hard-mode pulls only; `--blizzard`, `--phase2`):
- `thorim.ringstep` flips for the ring bots of a covered slot within ~0.5 s of the cover, `thorim sif blizzard action` moves by ring
  melee, no `avoid aoe` move by a ring bot in phase 2 (`thorim ring aoe hold` vetoes instead).
- Ring melee in a zone well under 7.6-50.5 bot-s a pull (replay 0-6), longest stay ≤ ~2 s, Blizzard ticks on ring melee from 0-55 toward
  a handful, no ring melee Blizzard death (2-3 in the last three hard-mode wipes).
- Walk back p50 ~1-2.5 s, max ≤ ~5 s, ~0 ticks on the way; stepped bot-seconds past 9.0 yd about the slot's covered seconds.
- Damage: `melee ring` "on Thorim to +100 s" still ~100%, his health lost per second in hard mode no lower than 0.24-0.48 %/s.
- Watch: Chain Lightning on a stepped bot near a tank (the second and third passes drop the tank clearance); a stepped bot held past
  reach after its slot clears (the walk-under test); pets on a covered slot dying (not in the trace: Revive Pet casts in phase 2).
- Normal-mode pulls: no `thorim.ringstep` row, `--blizzard` reads "no Sif Blizzard in phase 2", the ring unchanged.

**Questions for the user:** none new.

**Review (round 17).** The cause holds. In w24 uld1 slot 0's solved bearing went 203 → 177 → 153 → 281° from 3:38.7 to 3:40.4;
from 3:40.5 every positioning walk of Uldxai and Uldxaf came back `wait` behind `avoid aoe` combat bookings while both took 5.0-5.3k
ticks, and Uldxai died at 3:45.3. `--blizzard` gives the before table and a rerun of `stepsim2.py` the built replay figures. The
replay's steps by pass: past the ring and off the tanks 17-44 bot-s a pull, past the ring 1.5-6.9, anywhere 0-1.6. The answer's
wording alone (the first two passes) leaves 10 and 21 bot-s in zones in w21 uld3 and w23 uld1, so the third pass is the one step
beyond the user's yes; it is logged under "Why the fallbacks". No size, mode or player branch; Thorim files only; no comments. Left:
a ring bot holding Thorim takes no step and no longer has `avoid aoe`, but ring melee held him inside a zone 0-0.2 bot-s over the
six pulls. Fixed: the Blizzard trigger ran `TryGetThorimPhase2Spot` (the slot pass over the group, the `thorim.ringspot` note, a
`RingPoint` raycast) every tick for every ring bot in phase 2, normal mode included, where it used to return at once.
`ThorimRingBlizzardStep` now asks whether the bot stands in a zone or the lit cone first; while it holds, the positioning trigger,
which reads the slot every tick anyway, keeps `thorim.ringstep` current. `pb-check` passes.

## Round 18 (window 25: build 3ce456519 with rounds 16 and 17)

**Data:** window 25, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261124-100512-1` (uld1),
`-2` (uld2), `-3` (uld3); traces `env/dist/logs/botobs/603_{1,2,3}_thorim_{1795514775,1795514771,1795514773}.ndjson`, read through
`raidtest.py report --boss thorim --raid uld --head 3ce456519 --pulls 10` (its `thorim.py` lines), `postmortem.py --death`, round 7-17
scripts (`budget.py`, `corridor.py`, `leadtl.py`, `leadstop.py`, `pack2.py`, `flees.py`, `sinkport.py`, `revive.py`, `othertgt.py`,
`p2rank.py`, `p2heal.py`, `chain.py`, `sifclock.py`) and new ones in session scratchpad `thorim_r18/` (`vetoes.py`, `steppass.py`,
`mana.py`, `healcasts.py`, `opening.py`, `victims.py`, `swaps.py`, `drag.py`, `units.py`, `dealt.py`, `evadescan.py`) over `snap`,
`move`, `act`, `veto`, `cast`, `aura`, `dmg`, `heal` and `note` rows. For the opening trade: all 37 Thorim phase 2s on disk with a
tank-to-tank swap; for the corridor stall: all 39 Thorim traces with a lever.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795514775 | wipe 6:48, **hard mode** (62507 0:54.2, `thorim.sifjoined`; phase 2 3:14.2 against 3:24.2) | phase 2: Thorim melee on a non-tank 15, Lightning Charge 6, Chain Lightning 5, Thorim melee on a tank 3, Frostbolt Volley 2, Frostbolt 1, Unbalancing Strike 1 |
| uld2 | 1795514771 | wipe 5:13, **hard mode** (62507 0:44.7, `thorim.sifjoined`; phase 2 3:04.7 against 3:14.7) | phase 2: Thorim melee on a non-tank 23, on a tank 3, Frostbolt Volley 2, Unbalancing Strike 2, Lightning Charge 1 |
| uld3 | 1795514773 | kill 5:56, normal (62565 3:14.6, phase 2 3:41.4) | none |

**Round 16 verdict (corridor flee hold, Revive Pet hold): held.** No accepted `flee` by a gauntlet-squad bot between the lever and phase 2
(w24: 28 / 11 / 33), `thorim corridor flee hold` vetoes 25 / 13 / 23 instead; no squad bot teleported from the corridor to the arena (w24:
2 / 4 / 5); no corridor death at all. No Revive Pet cast in any pull (the harness gave roster B its pets back), so its hold never had to
fire. uld2's first lead move came 0.25 s after the lever (w24 6.7 s); the Colossus pull 1.8 / 1.4 / 2.2 s after the leader reached his stop;
pack 2 moved 1.8 / 3.3 / ~3 s after pack 1's last death (w24 uld2 / uld3 9.0 / 8.8 s). Corridor squad bots targeted arena adds only on the
balcony at the jump (uld2 3:00-3:01).

**Round 17 verdict (ring melee step out of Blizzard): the deaths stopped; the fallback never fired.**

| Pull | ring melee in a zone | stays p50 / max | ticks | Blizzard deaths | steps; stepped / out of reach | walk back p50 / max |
|---|---|---|---|---|---|---|
| w25 uld1 (5 Blizzards) | 29.3 bot-s | 2.4 / 3.7 s | 21 (97k) | 0 | 61; 250.5 / 203.1 bot-s | 1.2 / 12.5 s |
| w25 uld2 (3) | 11.4 | 1.2 / 3.9 | 5 (22k) | 0 | 25; 76.9 / 72.7 | 2.0 / 14.0 |
| before: w24 uld1, w23 uld1 | 32.3, 50.5 | max 8.7, 10.0 | 40 (185k), 55 (248k) | 3, 3 | - | - |

- Fallback: every one of the 86 distinct step destinations (`steppass.py`, classified by the bot's and Thorim's snapshot positions) lies
  past the ring, 8 yd or more off both tank spots and within 15 yd: the first pass. The second and third passes (past the ring near a
  tank spot, anywhere within 20 yd) never produced a step, so they cost nothing here.
- No accepted `avoid aoe` move by a ring melee in phase 2 (one `thorim ring aoe hold` veto, uld2). Melee on Thorim to +100 s 100%.
- In a zone 11-29 bot-s against the replay's 0-6: each bot still stands 1-4 s in the zone a passing bunny drops on it before it is clear.
  Ticks 5-21 a pull. The two 12.5 / 14.0 s walk backs are single outliers, not read further.

**Why both hard-mode phase 2s were lost: the healers ran dry and the tanks fell; Thorim was at 26.2% / 56.2%.**

| Pull | first tank death | Thorim then | healer mana then | non-tank deaths before it | Thorim %/s 0-30 / 30-60 / 60-90 / 90-120 / 120-150 s |
|---|---|---|---|---|---|
| uld1 | +195.9 s, Lightning Charge on the holder | 26.2% | 0 / 1 / 1 / 8 | 11: Chain Lightning 5, Lightning Charge 4, Frostbolt 1, Volley 1 | 0.20 / 0.51 / 0.45 / 0.37 / 0.39 |
| uld2 | +96.2 s, Volley on the holder | 56.2% | dead / 1 / 4 / 43 | 2: Thorim's landing swing 1, Volley 1 (the holy paladin) | 0.28 / 0.58 / 0.48 / 0.33 / - |

- **Tank deaths.** uld1: holder Uldxaa (prot paladin) went 100 → dead in 8 s under Thorim melee 35.7k, Blizzard 31.5k (9 ticks: both tank
  spots sat in Blizzard 5) and a 22.9k Lightning Charge, healed 35k; Uldxam (blood DK, struck) 0.7 s later, melee 75.9k and Blizzard 47.1k
  in its last 19 hits. uld2: holder Uldyab (bear) took Thorim melee 70k, Frost Nova 10k and a 7.2k Volley in its last 10 s, healed 46k;
  struck Uldyaa took him next and died 1 s later to two swings (33.3k, 17.3k). Both soulstones died within 2 s. Before the first
  tank death tank intake was 1.90 / 0.88 M: Thorim melee 52 / 43%, Blizzard 25 / 27%, Unbalancing Strike 11 / 12%.
- **The waiting tank's Blizzard.** Reader: holder / waiting 39 hits (124k) / 87 (349k) in uld1, 20 (77k) / 37 (155k) in uld2 (w15-w23
  waiting 45-49 hits, 160-197k). Before the first tank death the waiting tank took 345k / 155k of Blizzard, 82 / 65% of its intake. By
  design: `ThorimSifBlizzardTrigger` refuses both tank roles in phase 2 and the off-tank spot sits 1.0 yd off the bunny's track (round 8,
  the user's spot). Question 1.
- **Mana.** Healers entered phase 2 at 66 / 55% mean (uld1 39-97, uld2 44-67) and burned ~0.35-0.45 points a second each: uld1 34% at +94 s,
  12% at +140 s, 5% at +186 s; uld2 46% at +44 s, then the holy paladin (66% mana, 44.7% health) died at +77.7 s to Frostbolt 10.2k and a
  Volley 11.3k, and the other three stood at 4 / 4 / 46% by +89 s. Overheal ran 10-16k/s against 20-25k/s effective in uld1. Mana Tide
  at +2 s (uld1) and +63 s (uld2); both uld2 priests had spent Divine Hymn on the start pack (0:04.6, 0:06.8). Class healing code, shared.
- **Thorim's health per second against the clock.** Berserk is phase 2 + 5 min; the binding clock is the mana wall, +96 s (uld2) and
  +196 s (uld1). A kill before it needs 1.04 / 0.51 %/s on average; the raid managed 0.40 / 0.36, 0.46-0.59 at 119-154k/s of DPS output
  once every DPS was on him (+35 s on) and 0.20-0.28 in the first 30 s. Per point of damage the hard-mode Thorim is ~1.4x tougher than the
  normal one: uld3 lost 0.70-0.99 %/s at 128-181k/s of the same output (`opening.py`). uld1's 26.2% at the first tank death is the lowest
  since window 12's 15% (w14-w24: 53-85%); its first 30 s and its 11 early deaths are the margin.

**Why uld3 reached Thorim at 3:41: a 21 s corridor stall on two guards that evaded every hit.** Lever to phase 2 196.6 s against the
169.7 s budget (+26.9 s); corridor trash 71.2 s (uld1 / uld2 39.3 / 43.5 s). From 1:14.0 to 1:35.4 Iron Ring Guard 5cd stood 2.3 yd from
warlock Uldzav at (2242.1, −310.1), hitting him, at 91% then regenerating to 100% at 1:20.3, and guard 5cb on his pet regenerated 81 →
100% at 1:28.7; the nine squad non-healers attacked 5cd (Mortal Strike and Sunder Armor rows `OK` on its guid) and dealt 0-11k per 2 s
against 30-55k before (`dealt.py`). The Runic Smash lane dodge (`thorim runic smash action`, 1:10.5) had walked Uldzav onto the left lane's
waypoint `ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1` (2242.12, −310.15), arriving 1:15.7, and he stood there until the next dodge at 1:34.5;
5cd then followed and died in 14 s; his felguard Flaadhun held 5cb. That is the cannot-reach evade of round 15 on a corridor mob
(`ChaseMovementGenerator` → `SetCannotReachTarget`, hits evade, regen 5 s later), though bots stood on that waypoint in many pulls before.
Once in 39 traces (`evadescan.py`: no other corridor guard held 8 s or more without losing health). Not fixed; without it uld3 would still
have been ~7 s late (to-Colossus 14.5 s, Honor Guards + Giant 50.3 s).

**Gauntlet timeline** (seconds per segment from the lever; budget lever → Sif's deadline 170.0 / 169.8 / 169.7 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w25 uld1 | 0.0 | 13.1 | 39.3 | 11.2 | 19.7 | 4.6 | 52.5 | 19.7 | 160.0 | made by 10.0 |
| w25 uld2 | 0.15 | 14.6 | 43.5 | 11.4 | 22.0 | 3.0 | 42.0 | 23.2 | 159.8 | made by 10.1 |
| w25 uld3 | 0.25 | 14.2 | 71.2 | 14.5 | 21.7 | 3.5 | 50.3 | 21.2 | 196.6 | +26.9 |
| w24 uld1 / 2 / 3 | 0.15-0.37 | 12.5-21.6 | 41.8 / 39.3 / 62.7 | 13.0 / 29.7 / 9.8 | 18.3 / 20.2 / 32.5 | 4.3-5.6 | 47.3 / 55.7 / 54.0 | 22.4-23.9 | 160.9 / 194.0 / 201.8 | made by 8.9 / +24.2 / +32.2 |

**Phase 2 timelines.**
- uld1 (from 3:14.2): main tank Uldxaa picks him up at +0.3 s and has him on the tank spot by +5 s; the first Unbalancing Strike lands
  ~+8 s; corridor tank Uldxam, mid-jump 41 yd out, takes him with Dark Command at +10.4 s (below); Lightning Charge kills two corridor
  melee chasing him at the balcony foot at +15.9 s (54-59% after Sif's Frost Nova), a Frostbolt a ranged at +16.6; he is back on the
  spot at +23 s. Volley + Frostbolt kill a ranged at +58 s; a Chain Lightning at +87 s hops 8 times and its last three hops kill three
  melee on one slot point 8.0 yd from the holder (59k, 82k, 117k); Lightning Charge kills two ranged on a shelter walk slowed by the Volley
  at +136 s; a chain through a healer and a ranged stacked on one camp spot kills both at +163 s; the tanks fall at +196 s.
- uld2 (from 3:04.7): Thorim's landing swing kills ranged Uldyaj at +3.2 s; the main tank's pickup leaves him 24-27 yd off the tank spot
  to +6 s; corridor tank Uldyab Growls him from the balcony 43 yd out at +7.0 s and he is back at +15.6 s; the holy paladin dies at
  +77.7 s; the tanks at +96-97 s; the raid by 5:13.

**Stop rules:** not polished (no hard-mode kill). Not stuck: rounds 3-18 picked sixteen different causes; rounds 16 and 17 held.

**Failure ranking** (64 deaths, all in the two hard-mode phase 2s):
1. **The hard-mode race at the mana wall**, 2 wipes (51 of the deaths after the first tank death). Not a bug as a whole (rounds 6, 9,
   15, 16); its leaks: the waiting tank's Blizzard (question 1), the opening below, 11 early non-tank deaths in uld1.
2. **The opening swap drag** (bug, fixed below): in every pull of this window; 2 of uld1's early deaths, and 0.20-0.28 %/s over the
   first 30 s against 0.46-0.58 after.
3. **Chain Lightning** 5 (uld1): a slot point 8.0 yd from the holder (the ring anchor's angle-only gap the boss doc names) and two ranged
   on one camp spot. **Lightning Charge** on two shelter walkers under the Volley slow (the doc's uncapped shelter run). Not picked.
4. **uld3 missed hard mode** by 26.9 s: the one-off corridor evade above.

**Bug picked: the corridor tank answers the first Unbalancing Strike from the balcony and drags Thorim off the tank spot.**
- Trace (`victims.py`, `swaps.py`, `drag.py`): in all three pulls the first tank-to-tank trade came at +7.0 / +10.4 / +8.7 s from the
  corridor tank 41-43 yd off the tank spot at z 429-433 (on the balcony or mid-jump), and Thorim followed it to the balcony foot, 28.5-36.3 yd
  off the tank spot, until walked back at +15.1-23.0 s. Uldxam's rows: `thorim unbalancing strike swap action` `OK` at +8.1 s (z 435,
  `Attack`), +9.2 s and +10.4 s, the last casting Dark Command 56222 on Thorim. Over the 37 phase 2s on disk with a swap, **32** opened
  with this trade (+6.7 to +13.8 s, the taker 37-44 yd off the tank spot; Thorim 16-36 yd off it within 8 s, back at +11-28 s), losing
  0.10-0.39 %/s from the drop until back against 0.46-0.92 over the next 30 s (one untouchable pull aside); deaths during it: w25 uld1 2
  (Lightning Charge on melee chasing him), w24 uld3 2 (Chain Lightning through melee strung along the drag). Since round 8, of the 168
  other swaps 149 came from within 1 yd of the off-tank spot, 15 more from within 7.4 yd of the tank spot, and four from 13-23 yd (three
  corridor tanks still walking in at +13-20 s, one at +111 s).
- Code: `ThorimUnbalancingStrikeSwapTrigger::IsActive` (`UldTriggers_Thorim.cpp:254`) checked roles, the partner's strike and its own, never
  where the bot stood; `ThorimTakeBossAction::Execute` (`UldActions_Thorim.cpp:513`) then `Attack`s and taunts from there. The pickup trigger
  (`:293`) has the floor gate the swap lacked, and the boss doc's design is a trade between two fixed spots 3.5 yd apart that moves him
  inside the ring's 5 yd reposition tolerance.
- **Classification: bug.** The swap is meant to trade him at the tank spots; a taunt from the balcony is the drag the fixed spots exist to
  prevent.

**Fix** (Thorim files only, no size, mode or player branch): `ThorimUnbalancingStrikeSwapTrigger` fires only for a bot within
`ULDUAR_THORIM_SWAP_TANK_SPOT_RADIUS` (8 yd, exact 2D) of the tank spot (`UldTriggers_Thorim.cpp:284`, header `:129`). The struck holder keeps
him until the corridor tank lands and walks in (~25-33 yd from its landing, ~4-5 s), inside the strike's 15 s; the action is unchanged. The
radius keeps every swap since round 8 but the balcony and walk-in ones, including a main tank's taunt back toward the spots from 6.1-7.4 yd.
A real tank holding him is answered the same way; a human waiting tank is never moved. Reader: `thorim.py --phase2` prints a new `opening`
line: the first trade's time, the taker's distance off the tank spot, his farthest from it over the next 8 s, and health lost per second
over the first 30 s (w25: +10.5 s, 41.3 yd, 31.5 yd, 0.20 / +7.0 s, 43.4, 36.3, 0.28 / +8.8 s, 43.1, 28.5, 0.44). Test in `test_thorim.py`.
Not addressed: a corridor tank taking him on threat without a taunt (w24 uld1, +6.1 s, after Army of the Dead).

`pb-check` passes (syntax over 14 TUs, pblint, codestyle, botobs tests).

**Estimated gain:** the drag cost ~5-13 s at 0.10-0.39 %/s against ~0.45: about 2-4% of Thorim's health a hard-mode pull (5-9 s of the
race), plus the melee it killed. Alone it would not have saved uld1 (23.1% at the wipe).

**Next window should show:**
- `opening` line: the first trade's taker within 8 yd of the tank spot in every pull (w25 41-43 yd), Thorim no farther than ~8-10 yd off
  it in the 8 s after (28.5-36.3 yd), the first trade ~+13-18 s (+7-10.5 s now), health lost over the first 30 s up from 0.20-0.28 %/s in
  hard mode (0.44 normal).
- `trades`: strike to swap max up to ~8-10 s for the first strike only, 0 unanswered; no `thorim unbalancing strike swap action` row from a
  bot above z 429.6.
- No death in the first 25 s of phase 2 from Lightning Charge or Chain Lightning on melee chasing him.
- Watch: the first-strike holder's health between +8 s and the swap (no tank death before +47 s on record); a corridor tank that lands and
  never walks in (the strike then goes unanswered); a threat drag without a taunt (w24 uld1).
- Rounds 15-17's checks again (`thorim.unreach`, flee and Blizzard vetoes, `--blizzard`).

**Questions for the user** (tactic, not built):
1. **The waiting tank steps out of Sif's Blizzard while it still carries Unbalancing Strike.** It is excused from every Blizzard dodge in
   phase 2 and its spot sits 1.0 yd off the bunny's track, so it took 345k / 155k of Blizzard before the first tank death (82 / 65% of its
   intake; uld1's 87 hits for 349k). A struck tank cannot take him back for the strike's 15 s and the next strike comes 20 s after the last,
   so stepping 11 yd clear of a zone during those 15 s and walking back for the swap costs the hold nothing. Recommended: yes.

**Seen, not picked:** the corridor guards' cannot-reach evade (uld3, above; a candidate if it recurs: the bot a corridor mob cannot reach
steps so the chase re-paths, as round 15's Thorim step does); Chain Lightning through a ring slot point 8.0 yd from the holder (uld1 +87 s, 3
deaths); two healers and two ranged stacked on single camp spots (uld1 +163 s); priests spending Divine Hymn on the start pack (shared).

**Review (round 18).** The cause holds. The `opening` line reproduces the three w25 figures; Uldxam's swap action rows in uld1 are `OK` at
+8.1 / +9.2 / +10.4 s. Over the on-disk phase 2s, 33 of 37 opening trades had the taker past 8 yd (w24 uld1's threat take among them). Of the
280 `OK` swap action rows in phase 2, 223 came from within 8 yd of the tank spot (149 on the off-tank spot) and 57 from past 10, none from
8-10, so the radius has no near miss. The corridor tank landed at +5.6-13.2 s against the first strike at +6.4-8.0 s, leaving ~30 yd to walk
inside the strike's 15 s. One code path for every size, mode and raid: the waiting tank stands on the fixed off-tank spot, under a human holder
too. Fixed: the boss doc's "~1.4x less health per point of damage" in hard mode is 62565's −30% maximum health on the normal-mode Thorim
(1/0.7 = 1.43), already under "Hard mode changes what a trace means", so the clause is dropped. `pb-check` passes.

**User answer (2026-11-24) to round 18's question:** not approved. The user fears Chain Lightning hopping from the stepped-out waiting tank
onto other players and is unsure the step is worth it. Round 19 measures that risk instead and builds nothing for it.

## Round 19 (window 26: build e834a2708 with round 18)

**Data:** window 26, one pull per pool, all valid (`start: step`, no DC veto row inside a pull): `tr-20261124-115845-1` (uld1), `-2` (uld2),
`-3` (uld3); traces `env/dist/logs/botobs/603_{1,2,3}_thorim_{1795521579,1795521585,1795521588}.ndjson`, read through `raidtest.py report
--boss thorim --raid uld --head e834a2708 --pulls 10` (its `thorim.py` lines), round 7-18 scripts (`budget.py`, `leadstop.py`, `leadtl.py`,
`barrier.py`, `sifclock.py`, `p2rank.py`, `mana.py`, `chain.py`, `victims.py`, `vetoes.py`) and new ones in session scratchpad `thorim_r19/`
(`botrows.py`, `opencasts.py`, `taunts.py`, `strikes.py`, `loose.py`, `stacks.py`, `bridge.py`, `clstep.py`, `clspan.py`, `clfloor.py`,
`spellrow.py`) over `snap`, `act`, `cast`, `aura`, `dmg`, `move`, `veto` and `note` rows. For the taunts: all 40 Thorim phase 2s on disk. For
the waiting tank's step: the four hard-mode phase 2s of windows 25-26 (`603_1_thorim_1795514775`, `603_2_thorim_1795514771` and the two
above). Spell data from `mod-spell-tweaks/data/dbc-reference/spell.reference.csv` and the core.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795521579 | wipe 6:10, **hard mode** (62507 0:48.2, `thorim.sifjoined`; phase 2 3:05.8 against 3:18.2) | phase 2: Thorim melee on a non-tank 18, Chain Lightning 7, Thorim melee on a tank 3, Frostbolt Volley 2, Dark Rune Champion Whirlwind 1, Lightning Charge 1, Unbalancing Strike 1 |
| uld2 | 1795521585 | kill 5:50, normal (62565 3:27.3, phase 2 3:38.4) | phase 1: Runic Colossus Smash 62339 on a gauntlet healer (2:19.0, the Colossus at ~20%) |
| uld3 | 1795521588 | wipe 6:34, **hard mode** (62507 0:45.7, `thorim.sifjoined`; phase 2 3:06.2 against 3:15.7) | phase 2: Thorim melee on a non-tank 19, Chain Lightning 5, Thorim melee on a tank 4, Lightning Charge 2, Blizzard 1, Frostbolt Volley 1 |

**Round 18 verdict (the swap fires only within 8 yd of the tank spot): held for the swap; uld1's drag came from a class taunt.**
- No `thorim unbalancing strike swap action` taunt from past the gate in any pull (`taunts.py`). uld2 and uld3 opened with the swap from
  7.6 / 8.4 yd (snapshot lag on a walking taker) at +14.2 / +15.6 s, Thorim no farther than 5.9 / 5.7 yd off the tank spot (w25: 41-43 yd,
  28.5-36.3 yd). uld3's first strike (+6.9 s) was answered at +15.4 s by the corridor tank walking in from 45.8 yd, the holder 53 → 71%.
  No Lightning Charge or Chain Lightning death in the first 25 s of any phase 2. Health lost over the first 30 s: 0.26 / 0.37 %/s hard mode
  (w25 0.20 / 0.28), 0.58 normal (w25 0.44).
- **What took him 25.3 yd off in uld1:** at +6.2 s Thorim flicked onto the mage Uldxag for one snapshot. The holder Uldxaa answered with
  Righteous Defense from 2.6 yd off the tank spot (+6.30 s), but the corridor DK tank Uldxam, mid-jump at (2138, −283, z 427.6), 40.5 yd off
  it, had already cast Dark Command (+6.28 s) from its class `lose aggro` trigger (`BloodDKStrategy.cpp:134`, act `dark command` rel 23 =
  `ACTION_HIGH + 3`), no Thorim node involved. Thorim was on him from +7.8 s, 24.8 yd off the tank spot by +9.8 s, back at +14 s. The first
  Unbalancing Strike (+7.0 s) came after it. Not a threat take.
- Over the 40 phase 2s on disk, 5 opened this way: a corridor tank's class Dark Command or Growl from the balcony or mid-jump, 38-45 yd off the
  tank spot, while Thorim was on a non-tank (w18 uld1, w23 uld2, w24 uld1, w24 uld2, w26 uld1), dragging him 14-41 yd off it in the next 8 s.
- Code: `ThorimTauntWouldSteal` (`UldDefinition_Thorim.cpp:56` at e834a2708), the `thorim taunt guard` `Block` on `Family::Taunt`, held a
  class taunt only while Thorim's victim was another tank. The swap and pickup taunt through `DoSpecificAction` (`Engine::ExecuteAction` applies
  no multiplier), so the guard never touched them, and round 18's gate never touched the class taunt.

**Gauntlet timeline** (seconds per segment from the lever; budget lever → Sif's deadline 169.8 / 170.0 / 169.5 s):

| Pull | lever after pack | walk to trash | corridor trash | to Colossus | Colossus | ramp | Honor Guards + Acolyte + Giant | balcony | lever to phase 2 | vs Sif |
|---|---|---|---|---|---|---|---|---|---|---|
| w26 uld1 | 0.18 | 15.2 | 38.7 | 8.0 | 20.5 | 3.3 | 47.2 | 24.5 | 157.4 | made by 12.4 |
| w26 uld2 | 0.03 | 14.2 | 42.3 | 13.7 | 32.7 | 6.6 | 48.3 | 23.3 | 181.1 | +11.1 |
| w26 uld3 | 0.49 | 15.6 | 42.8 | 11.5 | 21.4 | 3.2 | 43.5 | 21.9 | 160.0 | made by 9.5 |
| w25 uld1 / 2 / 3 | 0.0-0.25 | 13.1-14.6 | 39.3 / 43.5 / 71.2 | 11.2 / 11.4 / 14.5 | 19.7 / 22.0 / 21.7 | 3.0-4.6 | 52.5 / 42.0 / 50.3 | 19.7 / 23.2 / 21.2 | 160.0 / 159.8 / 196.6 | made by 10.0 / 10.1 / +26.9 |

**Why uld2 reached Thorim at 3:38** (11.1 s past Sif's deadline, 23.7 s behind uld1's lever-to-phase-2): **Runic Barrier** at +10.1 s with the Colossus at 75% (`barrier.py`;
uld1 at 7%, uld3 at 41%), so he took 32.7 s against 20.5 / 21.4 (+11-12 s); a **Runic Smash lane flip** at 1:35.7 walked the leader from the
right lane's waypoint across ~26 yd to the left one and down it, so trash end to his Colossus stop took 13.2 s against 6.3 / 9.7 (+3.5-7 s); the
ramp 6.6 s against 3.2-3.3. Both are the luck rounds 15-18 named; uld2's pack also died at 0:37.2, 9.0-11.5 s later than the others, which Sif's
clock (62507 at pack death + 20 s) does not count. Its one death: the gauntlet healer Uldyaf to the Colossus' Smash 62339 (26.9k) at 2:19.0.

**Why both hard-mode phase 2s were lost: the tanks fell at +133.4 / +136.7 s with Thorim at 41.6 / 40.1%** (`p2rank.py`, `strikes.py`,
`mana.py`, `stacks.py`):

| Pull | first tank death | Thorim then | healer mana at P2 / then | non-tank deaths before it | Thorim %/s 0-30 / 30-60 / 60-90 / 90-120 / 120-150 s | DPS alive +0 / +60 / +120 |
|---|---|---|---|---|---|---|
| uld1 | +133.4 s, blood DK Uldxam, 5.6 s into its hold | 41.6% | 52 / 57 / 51 / 59 → 30 / 1 / 34 / 45 | 4: Champion Whirlwind, Lightning Charge, Volley, Chain Lightning | 0.26 / 0.51 / 0.47 / 0.47 / 0.48 | 19 / 19 / 18 |
| uld3 | +136.7 s, bear Uldzab, 9.0 s into its hold | 40.1% | 93 / 80 / 95 / 60 → 26 / 30 / 53 / 23 | 6: Chain Lightning 3, Volley, Blizzard, Lightning Charge | 0.36 / 0.50 / 0.47 / 0.42 / 0.46 | 19 / 19 / 17 |

- **Tank deaths.** Every later Unbalancing Strike up to the first death was answered by the swap within 0.1-0.3 s, the struck tank on the
  tank spot and the taker on the off-tank spot (the first: uld1 by the drag at +0.8 s, uld3 at +8.5 s by design). Thorim's landed swings on the tanks grew from 7-8 a 30 s at median
  7.8-9.0k (+0-30 s) to 23-24 at 14.3-17.8k (+120-150 s), 2 → 11-12k/s of melee alone. The dying tank took 108k / 145k in its last 10 s
  (Thorim melee 77k / 120k) against 57k / 84k healed. Tank intake from phase 2 to the first death 1.16 / 1.22 M: Thorim melee 54-55%,
  Blizzard 22-23% (the waiting tank 175k / 192k of it), Unbalancing Strike 8-13%, Volley 7-8%. Both soulstones died within 3-5 s; the other
  tank held 22 / 40 s more.
- **Mana** was not dry this time (w25: 0-8% at the first tank death): 28 / 33% mean, one priest at 1%. Healing throughput on the holder at
  Thorim's late stacks is the limit.
- **Thorim's health per second.** To finish him by the first tank death the raid needed 0.73-0.75 %/s from the landing; it dealt 0.44 on
  average and 0.42-0.51 per 30 s from +30 s on, with 19 DPS alive to +90 s in both. The raid lived longer than in window 25 (4 / 6 non-tank
  deaths before the tanks, w25 uld1 11) and the tanks fell later than uld2's +96 s, but earlier than uld1's +196 s.
- **Not a bug as a whole:** the hard-mode race of rounds 6, 9, 15, 16 and 18 on ilvl 226 rosters (the user's Q1 answer, stop rule waived).

**Phase 2 timelines.**
- uld1 (from 3:05.8): Uldxaa picks him up +0.3 s (Hand of Reckoning +3.2 s) and has him near the tank spot by +6 s; the flick and Uldxam's
  Dark Command above, Thorim dragged to 24.8 yd off and back by +14 s; swaps every ~20 s; a Champion's Whirlwind kills a ranged +22 s,
  Lightning Charge a melee +46 s, a Volley a ranged +78 s, a chain through two camp spots a ranged +102 s; Thorim 44% at the +127.8 s swap;
  Uldxam dies +133.4 s, its soulstone +138.6 s, Uldxaa +160.3 s; a chain seeded on slot melee runs into a camp spot of two ranged and a healer
  at +162.4 s (8 hits, 5 dead); Uldxaa's soulstone dies +167.9 s; the raid by 6:10.
- uld3 (from 3:06.2): Uldzaa picks him up +0.5 s; first strike +6.9 s, corridor tank Uldzab swaps from 8.4 yd at +15.4 s; Blizzard and a Volley
  kill a camp healer and a ranged at +44.9 s; one chain kills two melee at +101.8 s (below); Uldzab dies +136.7 s, its soulstone +139.7 s,
  Uldzaa +179.7 s and +194.4 s; the raid by 6:34.
- uld2 (normal, from 3:38.4): no death; Thorim 0.58 %/s over the first 30 s, 0.76 over the phase.

**Stop rules:** not polished (no hard-mode kill; uld2's kill is normal mode). Not stuck: round 18 and round 19 both pick the opening drag, by two
different code paths (the swap action, then the class taunt); round 18's half held.

**Failure ranking** (66 deaths, 65 in the two hard-mode phase 2s):
1. **The hard-mode race**, 2 wipes, 51 deaths after the first tank death. Not a bug (above).
2. **Chain Lightning**, 12: 4 before the tanks fell (uld1 1 through two camp spots; uld3 3, one cast killing two melee), the rest in the
   collapse, 5 of them one cast through a stacked camp spot (accepted by construction, boss doc). Not picked (below).
3. **The opening drag by a class taunt** (bug, fixed below): uld1, and 5 of 40 phase 2s on disk; 0.26 %/s over uld1's first 30 s against uld3's
   0.37, ~3% of Thorim, no death this window.
4. Lightning Charge 3, Volley 3, Blizzard 1 (a camp healer), Champion Whirlwind 1; uld2's Colossus Smash 1.

**Bug picked: a tank's class taunt drags Thorim off the tank spot from the balcony.**
- **Classification: bug.** The tanks trade him between two fixed spots (boss doc), round 18 gated the swap action on that, and the guard exists
  to stop class taunts fighting the encounter for him; a `lose aggro` Dark Command from 40 yd is the drag both meant to prevent.
- **Fix** (Thorim files only, no size, mode or player branch): `ThorimAtTankSpots` (`UldEncounter_Thorim.cpp`, header next to
  `ThorimTankSpotNeedsMove`): within `ULDUAR_THORIM_SWAP_TANK_SPOT_RADIUS` (8 yd, exact 2D) of the tank spot. The swap trigger uses it in place
  of its inline test (`UldTriggers_Thorim.cpp`). `ThorimTauntWouldSteal` still blocks every class taunt while another tank holds him, and now
  also one from a bot outside `ThorimAtTankSpots` while he is on a non-tank or on nobody. The holder and the waiting tank on their spots keep
  their class taunts; a far tank still takes him through Thorim's own pickup (the anchor tank whenever it stands on the floor, and a dead
  anchor's partner is re-elected anchor), which taunts through `DoSpecificAction` and no `Block` reaches. A human tank is never held (rules run in bot engines).
- Replayed on the 40 phase 2s (`taunts.py`): of the class taunts cast on Thorim, the rule holds the five balcony ones and one more, a main
  tank's Righteous Defense from 11.4 yd at +3.9 s with him on a melee (w13 uld1, where its pickup ran as well). A Death Grip from 31.3 yd with
  Thorim already on the caster (w12 uld1 +180.8 s) passes, as before. Every other class taunt came from within 3.5 yd of the tank spot.
- Boss doc: the taunt guard paragraph and the step measurement below (`/compact-docs-writer`, +166 words).

`pb-check` passes (syntax over 14 TUs, pblint, codestyle).

**Next window should show:**
- `opening` line: the first trade's taker within 8 yd of the tank spot in every pull, Thorim no farther than ~8-10 yd off it over the next 8 s.
- `thorim taunt guard` veto rows on a class taunt (`dark command`, `growl`, `taunt`, `hand of reckoning`, `righteous defense`, `death grip`) from a
  corridor tank in the first ~15 s; no class taunt cast on Thorim by a tank past 8 yd of the tank spot (`taunts.py`).
- Thorim on a non-tank with a tank up for no longer than ~1-2 s at a time (reader's "on a non-tank with a tank up": 3.6 / 7.2 s now, mostly the
  landing and the collapse), answered by the holder's class taunt or the pickup.
- Watch: a non-tank held past ~3 s while the only living floor tank stands past 8 yd of the tank spot and is not the anchor tank (the pickup's
  off-tank branch needs the group's main tank gone).
- Rounds 15-18's checks again: `thorim.unreach` (0 spans this window), corridor flee vetoes (24 / 27 / 19, no accepted flee), ring Blizzard
  (no ring melee Blizzard death; 9.8 / 14.6 bot-s in a zone, 3 / 10 ticks).

**The waiting tank's Blizzard step (the user's Chain Lightning concern), measured, not re-proposed.**
- **Spell data.** 64390 (25-man; 62131 at 10) `EffectChainTargets` 8: the target plus 7 hops, each the unhit body nearest the last victim, with
  LOS, at `EffectChainAmplitude` **×1.5** a hop (5k → ~85k by the eighth). Hop range: `spell_jump_distance` **5.0** (`2026_03_11_00`, over the
  core's magic-chain default of 10) tested by `IsWithinDist`, which adds both bodies' combat reach: **8.0 yd** between two players (1.5 each), **8.9 yd**
  when both carry Heroism or Bloodlust (+30% scale), pets by their own reach. The seed is `SelectTarget(Random, 0)` over his whole threat list,
  tanks included, every 15 s (`boss_thorim.cpp:737-740`).
- **When the step would fire.** Every Sif sweep in these four hard-mode phase 2s covered the off-tank spot (within 11 yd of a zone or the bunny):
  13 spans of 11.4-12.6 s, the waiting tank struck for 43-100% of each. 7 of the 23 Chain Lightning casts with a tank pair landed inside one.
- **At each of those 7 casts** (`clstep.py`, bodies at the landing, cast + 500 ms): the nearest point 11 yd clear of every zone and the bunny, on
  the floor and out of the lit cone, lay 8.5-9.0 yd east of the off-tank spot (once 0.5 yd), between the tank spot and Thorim. There the waiting
  tank would stand **3.5-6.4 yd from the holder, 6.1-7.4 from the nearest ring melee (11.9 once), 16.1-25.2 from the nearest camp player and
  2.5-4.7 from the nearest pet (9.3 once)**: inside the 8 yd hop of some body at all 7. A point also out of hop range of every player and pet
  existed at all 7, but **15.5-18.8 yd out** (4.9 once; at 8.9 yd 16.4-24.6, 5.8 once).
- **Over the whole span** (`clspan.py`, a fixed point that stays 11 yd clear of every zone and out of the cone the whole time): only **3 of 13**
  spans had one that also stayed out of 8.0 yd of every player and pet throughout (16.3-19.6 yd out), **1 of 13** at 8.9 yd; the best fixed
  point's smallest gap to a body over a span was 5.5-10.7 yd (p50 6.9).
- **Answer:** no step point stays out of hop range of everyone. The nearest clear point puts the tank 2.5-7.4 yd from a pet, ring melee or the
  holder at every cast; a far point exists at a cast instant, 15.5-18.8 yd out, but holds for a whole sweep in 3 of 13 (1 under Heroism). Not
  re-proposed. The tax stays: 175k / 192k of Blizzard on the waiting tank before the first tank death here (Blizzard 22-23% of all tank intake).

**Seen, not picked:**
- **A lit-orb cone pushing a ring slot into another's hop range.** uld3 +101.2 s: slot 1's solved bearing went 315 → 271° (relax 1, cone only,
  separation dropped), its three melee walked to (2113.3, −265.6), 6.7-7.1 yd from slot 0's bodies at (2107.2, −262.8), and the Chain Lightning
  that comes with each orb (+101.8 s) ran 6 hits across both slots, killing Uldzaj (38k) and Uldzag (62k). A candidate if ring chain deaths recur.
- Thorim flicking onto a non-tank in the first seconds of phase 2 (uld1 +6.2 s onto a mage): threat right after the pickup taunt (boss doc,
  "Thorim's threat table is empty at the phase change"); the holder answered it in 0.1 s.
- uld2's corridor: Runic Barrier at 75% and a lane flip, luck as in rounds 15-18.

**Questions for the user:** none new. The waiting tank's step is not re-asked (above).

**Shared code touched:** none (Thorim files only).

**Review (round 19).** The cause holds. Uldxam's uld1 rows: generic `dark command` `OK` at +6.28 s (rel 23, `BloodDKStrategy.cpp:134`),
56222 on Thorim, Thorim on the mage, no Thorim node; the other four balcony casts are class taunts too (w24 uld1's a generic `dark command`
0.24 s after a `thorim dps priority action` row). Fixed: **the anchor tank keeps its class taunts past 8 yd.** The pickup's
`DoSpecificAction("taunt spell")` runs no alternative, so with that spell on cooldown the class fallback is the anchor's only answer. w13
uld1 (`603_4_thorim_1795381228`): Thorim sat on two melee from the landing to +3.9 s; the pickup ran at +0.04 s and `FAILED` at +0.26 s and
not again ("where its pickup ran as well" above is not borne out); Hand of Reckoning and Righteous Defense were `IMPOSSIBLE` at +2.3 s; the
anchor's Righteous Defense at +3.87 s, target Thorim, 10.0-11.4 yd off the tank spot, took him back. Round 19's rule held that cast until the
anchor was inside 8 yd (+4.4 s); anchors walk in at ~4.4 yd/s (26.4 → 7.7 yd in 4.3 s there), so from a 20-31 yd pickup the hold could
last 3-5 s. The anchor is a bot on the floor and keeps the role while eligible, so the five balcony casters (corridor tanks, never anchor
there) stay held; replayed on the 40 phase 2s the rule now holds those five and nothing else. One code path for every size, mode and raid.
Next window: no `thorim taunt guard` veto row from the anchor tank. `pb-check` passes.
