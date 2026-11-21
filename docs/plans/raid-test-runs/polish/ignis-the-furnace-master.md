# Ignis polish log (25-man normal, no hard mode)

Reader: none under `tools/botobs/bosses/`. What this round used instead: `postmortem.py <trace>
--threat 33118` (share of time Ignis spent on a tank), `--track <main tank>` (his distance to
Ignis), `--bot <main tank>` (moves and vetoes). Main tank is roster slot `aa` (pool letter x/y/z).

## Round 1 (build 14318f5fe)

**Data:** window 3, 3 valid pulls, one per pool, runs `tr-20261121-193121-11` (uld1),
`-193602-12` (uld2), `-194004-13` (uld3); traces `env/dist/logs/botobs/603_<inst>_ignis-the-furnace-master_<epoch>.ndjson`.
Window 2's uld1 wipe (`tr-20261121-185743-3`, trace `603_3_..._1795287470`) was voided by a 10 s
freeze; read for the pattern only.

| Pool | Tanks (MT, assist) | Pull (epoch) | Out | Ignis on a tank | Deaths by mechanic |
|---|---|---|---|---|---|
| uld1 | prot pala, blood DK | 1795289516 | kill 3:35 | 54% | Ignis melee 5, Flame Jets 3, Scorch 2, Scorched Ground 1 (11) |
| uld2 | prot warrior, bear | 1795289764 | kill 3:27 | 99.6% | 0 |
| uld3 | prot pala, bear | 1795290036 | kill 3:25 | 99.1% | 0 |
| uld1 (w2, void) | prot pala, blood DK | 1795287470 | wipe 3:17 | 33% | 30, same pattern |

**Failure picked: the main tank loses Ignis on the drag and is stranded at the anchor** (all 11
uld1 deaths; the void w2 wipe too).

- uld1: the MT (Uldxaa) took the pull at 0.3 s and issued one `MoveTo` to his arc slot
  (579.3, 273.1), 92 yd away, from `IgnisMainTankPositionAction` (`UldActions_Ignis.cpp:66-73`).
  Ignis switched to melee at 1.0 s, the assist DK (Uldxam) taunted at 2.8 s, and Ignis stayed at
  (583.8, 371.9). The walk went on regardless: the MT stood at the slot 94-99 yd from Ignis from
  0:12 to 2:30, potted twice. The DK held Ignis by the raid until 0:45, left for a construct, and
  Ignis then walked through the raid: every death after that.
- Why the guards missed it: the trigger's victim check and the action's melee-range leash
  (`IsWithinMeleeRange || IsIgnisScorchWindow`) are only read when a move is issued, and one move
  covered the whole drag. In w2 the walk started during Scorch's root, which the leash lets
  through, and went 110 yd.
- Why nothing brought him back: `ignis tank movement` (`UldDefinition_Ignis.cpp:78`) blocks reach,
  charge, follow and flee for the main tank all fight (`IgnisPlacedTank` = `IsMainTank`), so
  `reach melee` was vetoed every tick, and Hand of Reckoning failed out of range (0:12).
- uld3's paladin MT walked off the same way, but its pull threat outlasted the bear's 3 s Growl and
  Ignis came back at 5.8 s. uld2's warrior taunted and Thunder Clapped before the walk.

**Classification: bug.** The strategy intends the main tank to hold Ignis at the anchor.

**Fix:**
- `GetIgnisMainTankLeg` (`UldEncounter_Ignis.cpp`): the drag walks `ULDUAR_IGNIS_TANK_DRAG_LEG`
  (5 yd) per move, so the leash and victim checks run before every leg and a broken drag leaves him
  ~5 yd past where it broke, inside taunt range. A spot inside Ignis' melee reach (arc steps on the
  anchor) still goes in one move.
- `IgnisPlacedTank` (`UldDefinition_Ignis.cpp`): the main tank counts as placed only while Ignis'
  victim is him, so a main tank that lost Ignis gets reach and the AoE dodges back and walks in to
  taunt.

**Harness note:** the drop at Ignis' feet (13.8 yd) stacks the raid inside his reach at the pull,
so any early threat loss lands on a ranged (uld1's first death, 0:09). Not the cause of the
stranding, which is the strategy's.

**Next window should show:** in every pool Ignis on a tank above ~95% (`--threat 33118`), the main
tank within ~20 yd of Ignis all fight apart from Slag Pot (`--track`), Ignis reaching the anchor
region (y below ~290) within ~25 s, and no Ignis melee deaths on non-tanks. uld1 is the pool to
watch.
