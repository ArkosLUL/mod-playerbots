# Polish log: Kologarn (25N)

Trace slug `kologarn`, raid `uld`, scenario `uld-kologarn` (drop on the walkway 39 yd west of him at
(1758, -24), the harness sets him on the tank). Kologarn has no hard mode, so a kill counts as is.
Reader: `tools/botobs/bosses/kologarn.py <trace>` (`--eyebeam`, `--trash`, `--deaths`).

## Round 1 (2026-11-21, build 14318f5fe)

**Data.** One pull per pool, all valid (`batch.py --strict --since 14318f5fe --boss kologarn`):
uld2 `tr-20261121-185743-4` (`603_5_kologarn_1795287512`, window 2, wipe 2:52), uld1
`tr-20261121-192219-9` (`603_9_kologarn_1795288972`, window 3, kill 3:29), uld3
`tr-20261121-192700-10` (`603_10_kologarn_1795289252`, window 3, wipe 3:07).

**Stop rules.** Not polished: 27-32 deaths a pull. First round.

**Deaths by mechanic** (every one after the antechamber trash joined at 0:24-0:26).

| Pull | Deaths |
|---|---|
| uld2 wipe | 15 antechamber trash, 9 Shockwave, 2 Focused Eyebeam, 1 Rubble, 1 Overhead Smash Tremor, 1 One-Armed Overhead Smash, 1 Kologarn melee on a non-tank, 1 Petrifying Breath |
| uld1 kill | 18 antechamber trash, 4 Shockwave, 1 Rubble, 1 Focused Eyebeam, 1 Overhead Smash, 1 Overhead Smash Tremor, 1 fall |
| uld3 wipe | 18 Shockwave, 6 antechamber trash, 3 Kologarn melee on a non-tank, 2 Overhead Smash Tremor, 1 Overhead Smash, 1 Kologarn melee on a tank, 1 Focused Eyebeam |

Shockwave (13-15k raid-wide every 17 s) only kills once the trash has taken the healers: uld3 lost
three of four healers to trash and dwarves by 2:27, then 9 and 6 died to the 2:44 and 3:02 sweeps.

**Picked: antechamber trash, pulled by the eyebeam runner (bug).** In all three pulls the first
eyebeam target (a ranged at the drop) left the walkway through the door, walked down the stairs and
brought up the trash: the Golem/Mender/Dwarf pack engaged it at (1695, -7, 434) at 0:24-0:26, the
Chamber Overseer at 0:30-0:33. 7 of 21 runs begun on the walkway left it (`--eyebeam`); uld1 lost a
second pack at 2:22 the same way.

Cause: `GetKologarnEyebeamEscapeStep` (`UldEncounter_Kologarn.cpp`) clamps steps to the box
x 1745-1780 / y -48..-2, but the floor is narrower: west of x 1750 only the door corridor, y -33..-15,
is walkway, and the south edge runs diagonally from (1750, -34) to (1760, -48) (walked extent over 4
traces). Each of the 7 departures followed an accepted
`kologarn eyebeam action` move (forced) to (1746.8-1750.8, -2..-6) or (.., -41..-48); e.g. uld2 Uldyao
0:15.473 -> (1748.0, -2.0, 448.0), held 4.7 s, walked (1758, -2) -> (1749, -15) -> door -> stairs ->
(1675, -6, 427). `PathGenerator` resolves an off-floor end point to the nearest poly within 50 yd
vertically, the antechamber floor below, and builds an incomplete path to it. One more bot (uld1
Uldxac 2:50) walked out through generic `avoid aoe` during the collapse.

Not the harness: runs starting away from the drop (uld1 2:04 from (1766, -11), 2:28 from (1771, -51))
left the same way.

Fix: each escape candidate goes through `Map::CheckCollisionAndGetValidCoords` from the bot (navmesh
raycast, cut where it leaves the floor); a failed one is skipped and the < 3 yd rule now applies to
the cut point. Holds for 10 and 25 (same spell pair, same room).

**Question (orchestrator / user): despawn the antechamber trash before Kologarn pulls?**
*Recommended: yes*, entries 34190, 34196, 34197, 34198, 34199 (and 34203, the Overseer's Displacement
Device) in the `uld-kologarn` row's `preDespawn`, as for XT-002 and Ignis: a raid reaching Kologarn
has cleared them. The fix is judged by `--eyebeam` ("runs that left the walkway"), which reads the
same with the trash gone.

**Also seen, not picked.** The first eye wave lands on the raid still stacked at the drop: 8-16 beam
hits on bystanders, no death from it. Focused Eyebeam killed 4 and Rubble 2 over the three pulls,
all after the trash joined.

**Next window should show.** `--eyebeam`: "runs that left the walkway: 0 of N", the westmost x of a
run 1745 or more; `--trash`: "never engaged" if the trash stays; no `kologarn eyebeam action` move to
a point at x < 1750 outside y -33..-15, or south of that diagonal edge. Deaths then show Kologarn's own mechanics.
