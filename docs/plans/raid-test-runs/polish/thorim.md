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
