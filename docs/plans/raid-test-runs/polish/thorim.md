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

**Stop rules:** not checked this round (window 14 belongs to the next one). Not stuck: no failure was picked.
