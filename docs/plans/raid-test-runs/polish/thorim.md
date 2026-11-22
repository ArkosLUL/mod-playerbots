# Thorim polish log (25-man hard mode)

Reader: none yet (`tools/botobs/bosses/` has no `thorim.py`). Round 1 needed only the pre-roll
`snap` rows (target, position, move generator per roster bot), read with throwaway scripts.

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
- `ThorimBalconyHitAllowed` (`UldEncounter_Thorim.cpp:1165`, header `:501`): a bot above the floor line, in the
  corridor squad, during the split (Thorim in combat on his balcony), whose `thorim.balcony` latch has passed
  `ULDUAR_THORIM_BALCONY_5` (read, never advanced: the walk owns it). The ranged hold spots use the same step gate;
  the latch only advances along the chain, so both Paralytic Fields are behind the bot.
- `ThorimDpsTargetAllowed` allows Thorim for that bot (Sif stays forbidden); `GetThorimDpsTarget` offers him only
  to that bot (`:1279`); `ThorimBalconyGuardMultiplier` passes Attack/Reach on him for it (`:120`).
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
