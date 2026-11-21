# Polish log: Northrend Beasts (25H)

Trace slug `northrend-beasts`, raid `toc`. Traces under `env/dist/logs/botobs/`.

## Round 1 (2026-11-21, build 580a1e13a)

**Data.** One kill per pool: toc1 `tr-20261121-164305-4` (`649_5_gormok-the-impaler_1795279469`,
8:09), toc2 `tr-20261121-154933-2` (`649_2_gormok-the-impaler_1795276251`, 7:15), toc3
`tr-20261121-164305-5` (`649_4_gormok-the-impaler_1795279463`, 8:40). Report
`raidtest.py report --boss northrend-beasts --raid toc --head 580a1e13a`.

**Stop rules.** Not polished: toc3 had 6 deaths. No earlier rounds.

**Deaths by mechanic.**

| Pull | Deaths |
|---|---|
| toc1 | 2 Slime Pool (ranged, Icehowl stage 5:42) |
| toc2 | 0 |
| toc3 | 4 Molten Spew (melee, lone Dreadscale's emerge 4:19-4:20), 1 Slime Pool (melee, 4:21), 1 Icehowl melee (third tank, 5:27) |

**Picked: Molten Spew, 4 deaths (bug).** Dreadscale, Acidmaw dead, emerged at (593.6, 137.4) facing
3.0 rad, straight at the melee 40-55 yd west, and spewed at once. The melee's first escapes worked
(Toczat, Toczag `move 2 spew escape` 4:17.77), then the reposition's `stand` tier sent them to their
stands behind him: `move 60 spew stand` (Toczat 4:17.93, Toczag 4:17.94), `move 59 spew stand`
(Toczao 4:18.77), `move 65 melee stand` (Toczaw 4:18.62), all four dead on that walk 40-46 yd out
(postmortem `--death 0..3`: 5-9 Spew ticks each). Cause: the path test (`PathBlocked`,
`ToCHelpers_Jormungars.cpp`) covered pools and Sweep only, so a stand outside the cone passed
however its walk ran through it.

Fix: armed Spew cones (danger region with the spot's arc) join the path test (`SpewBlocksWalk`): a walk
from outside never enters; one from inside must never re-enter and stay inside no longer than a
straight step off the axis + 4 yd. Cure and run walks test only a Spew under way (Toxin race). Also
the Bile scatter's path.

**Second: Slime Pool after the crash, 2 deaths (bug).** toc1 survival hunters Tocxaj, Tocxau stood
4.4 yd from the centre; Icehowl jumped there (5:38.7), they `disengage`d (5:39.74, 5:40.00) 15.6 yd
into the pool at (588.5, 155.2) (r ~17), the leave-pool walk started, and Massive Crash (5:41.2)
stunned them inside: 3 ticks, dead. `icehowl charge guard` only ran from the gaze. Fix: the guard runs
from his jump (`IcehowlChargeCycleActive`, capped 12 s before the gaze) and spares the worm reposition
too. Measured: the crash knockback moved no bot in 6 crashes over the 3 traces (median 0.0 yd).

**Not fixed, for later rounds.**

- toc3 Toczai (frost DK, Bile carrier held out): bile reach guard zeroes `reach melee` but not
  `reach spell`, which walked it 4 yd into a pool (4:17.38); trapped between two pools and the 44 yd
  spot floor, `none pool`, then a 30 yd `cross` walk through the pool's middle (preferNear is the
  worm's back). Generic reach also walks through pools when the stop is past the 30 yd search.
- toc3 Toczaz (third tank, blood DK): Icehowl's duty bounced Toczaa → Toczaz → Toczab → Toczaz in
  5:18-5:24; died to 2 melee + Ferocious Butt (79k in 1.7 s).

**Next window should show.** No Spew deaths; no `nb.wormmove` stand walk crossing an armed cone (watch
`nb.wormhold release path` counts, and `none melee` backoffs while armed). No `disengage`/`blink`/
`flee` OK rows between `nb.charge` 1 and 2, no Slime Pool death in Icehowl's stage.
