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

**Review fix.** With the stand walk blocked the reposition backs off 1 s, and generic `reach melee`
(rel 21) took over: the same melee walked 18-20 yd into spewing cones by it (toc3 4:17.40 Toczat,
4:18.60 Toczao; toc1 4:21.28-4:21.37 six melee). New `northrend worms spew reach guard` zeroes
`reach melee`, `reach spell` and the charges while that walk fails the Spew path test.

**Next window should show.** No Spew deaths; no `nb.wormmove` stand walk crossing an armed cone (watch
`nb.wormhold release path` counts, and `none melee` backoffs while armed), and `spew reach guard`
vetoes instead of `reach melee` walks into a spewing cone. No `disengage`/`blink`/
`flee` OK rows between `nb.charge` 1 and 2, no Slime Pool death in Icehowl's stage.

## Round 2 (2026-11-21, build d8078b2bf, window 1)

**Data.** Valid (no freeze): toc1 `tr-20261121-183213-8` (`649_8_gormok-the-impaler_1795286012`, kill
8:18), toc2 `tr-20261121-183233-9` (`649_9_gormok-the-impaler_1795286032`, kill 8:46). Voided by the
runner's freeze rule (`dfa51a8b7`, ~6.5 s world stops from other raids' run setup), read with care:
toc1 `649_2_…_1795285394` (kill; freezes 3:10, 3:58, 4:45, 7:19, 7:30), toc2 `649_1_…_1795285394` (kill;
same freezes), toc3 `649_3_…_1795285875` (wipe; freezes only 1:00 and 1:20, Gormok's stage, so its worm
stage reads clean), toc3 `649_3_…_1795285394` (wipe; same freezes as toc1/2, deaths cluster at freeze
ends). Baseline before r1: `cycle-beasts-2` (6 pulls on `580a1e13a`). Reports `window-1-beasts.md`,
`window-1.md`.

**Stop rules.** Not polished: toc3 wiped twice, toc2 had 7 deaths. Spew picked in round 1 and 2.

**Deaths by mechanic.**

| Pull | Deaths |
|---|---|
| toc1 649_8 | 3 Acidmaw melee on melee DPS (3:40-3:54), its tank Tocxaa paralysed by Toxin 3:21-3:57 (runner "no spot" 17.3 s) |
| toc2 649_9 | 1 Molten Spew (Tocyae heal, 4:17, again 4:18 after a self-res), 1 Slime Pool (Tocyas, 4:38), tanks: Dreadscale melee 4:41, Ferocious Butt 5:18; 2 Arctic Breath (5:32) |
| toc1 649_2 (void) | 1 Acidic Spew (Tocxac, 3:23, 56.4 yd out), 1 Molten Spew (Tocxaj, 4:19, 55.5 yd out) |
| toc2 649_1 (void) | 1 Slime Pool (4:05), 1 Molten Spew on a tank (4:51), both at a freeze's end |
| toc3 649_3_…875 (void) | 1 Acidmaw melee (4:02), 6 Molten Spew (4:37-4:39), 2 Slime Pool (4:42); Berserk 8:39.9 with Icehowl at 24.9%: 22 more |
| toc3 649_3_…394 (void) | freeze-shaped: Impale 1 (1:06), Paralytic Spray 1, Toxin 4 (3 at 3:16.2 and 1 at 4:51, freeze ends), Bile 3, Spew 1 tank, Slime Pool 2; tanks gone, Icehowl kills 21 |

**Verdict on round 1.** Stand walk through an armed cone: gone. No Spew tick in 6 pulls lands on a
`stand` walk; `spew reach guard` vetoes `reach melee`/`reach spell`/charges 5-32 times a pull. Icehowl
charge guard: held; no `disengage`/`blink`/`flee` OK row in any `nb.charge` 1 span, no Slime Pool death
in Icehowl's stage. Spew still kills by two other ways (below).

**Picked: Molten Spew, 7 deaths (bug).** The cone follows Dreadscale's victim while he stands rooted
channelling, and the mobile holder moved it. 649_9: lone Dreadscale emerged at (582.9, 123.6), Spew
cast 4:15.47 on Tocyaa standing in an old pool; at 4:15.494 Tocyaa `drag 30` north to (577.7, 156.2)
(`nb.wormmove`), facing 2.67 → 1.76 (52°) over the casters; Tocyae's `spew full` spot released every
130 ms (`release spew`) as the cone chased it, 4 ticks, dead. 649_3_…875: Dreadscale spewed at 4:35.54
on third tank Toczaz (facing 1.83); the duty went to Toczaa, who taunted at 4:36.61 from among the
casters (facing → 2.55) and `drag 20` at 4:36.90 (→ 1.95): 59 non-tank ticks, 6 dead. Cause:
`DragOffHazards` (`ToCActions_Jormungars.cpp`) only skipped the heroic edge drag while spewing; the pool
drag and the hold's taunt had no Spew test.

Fix: the mobile hold doesn't taunt while its worm spews at another tank (`IsBeastsTank`), and drags
neither start nor continue while it spews (`IsWormSpewing`, an in-flight drag `StopMoving`s).

**Second: Spew reach short by 1.5 yd (bug).** The tick's cone uses `IsWithinDist3d`, range 55 plus the
player's combat reach 1.5. `InCone` tested 55, so escapes parked at 55-56.5: 99 of 385 non-tank Spew
ticks across 12 pulls landed 55-56.7 yd out (both 649_2 deaths; the baseline 649_7 Acidic Spew wipe
start, 5 casters at 54-56.4). Fix: `WORM_SPEW_REACH` 57.5 (hit range 56.5 + 1) in `InCone` and
`SpewSideExit`; the wedge note's `range` is now 56.5.

**Reader.** `--worms` Spew wedges print `swing` (degrees the worm turned off the wedge's facing) and
each victim with the yards it walked.

**Not fixed, for later rounds.**

- Acidmaw mobile tank paralysed by Bite Toxin, no runner reaches it, melee pull threat and die (649_8:
  3; baseline 649_6: 4). Cure-walk spot search fails round a tank in the worm's front; needs its own
  round.
- Slime Pool deaths in the worm stage (649_9 Tocyas: `reach spell` into the pool and `pool escape` every
  ~1.5 s for 8 s, then a 30 yd `cross` walk) — the pool reach guard's 4 s streak cap lets reach win.
- toc3 Icehowl reached Berserk (8:39.9 at 24.9%) after the Spew wipe of the casters.
- A non-holder victim still runs the Spew rule and drags the cone with it: a healer on the emerge's
  threat wipe, or a tank with no worm duty, which now keeps the worm through the Spew (649_3_…875
  Toczaz, duty `none` 4:36.41, `move 8 spew escape` 4:36.42, ~4° of swing). No death traced to it yet.

**Review fix.** The class `lose aggro` taunt (`hand of reckoning` ACTION_HIGH + 7, warrior `taunt`
ACTION_INTERRUPT + 1, …) fires for a flagged main tank whenever another tank holds its target
(`HasAggroValue`), and the hold failing mid Spew lets it run; `northrend beasts taunt guard` passed it
for the holder. The guard now zeroes the holder's taunt on a worm spewing at another tank
(`IsWormSpewingAtOtherTank`, shared with the hold). DC flags slot 0 main tank, and the deal puts the
flagged main tank first.

**Next window should show.** Spew wedges with a holder victim: `swing` under ~10° and that victim
walking ~0 yd (no `drag` note within 3.5 s of a wedge); no taunt by the mobile holder mid Spew, the
class one included; no non-tank Spew tick 55-57 yd out; Spew deaths only on a non-tank victim.

## Round 3 (2026-11-21, build a038bba60, window 4)

**Data.** 6 valid 25H pulls, no freeze: toc1 `649_20_…_1795292856` (wipe 10:02, 34 deaths),
`649_15_…_1795292227` (kill, 1); toc2 `649_22_…_1795292925` (kill, 1), `649_16_…_1795292272` (kill, 3);
toc3 `649_21_…_1795292936` (kill, 1), `649_17_…_1795292293` (kill, 3). Report `window-4.md`,
regenerated `raidtest.py report --boss northrend-beasts --raid toc --head a038bba60 --pulls 10`.

**Stop rules.** Not polished: the toc1 wipe. Spew was picked in rounds 1-2; this round picks another
failure.

**Deaths by mechanic.**

| Pull | Deaths |
|---|---|
| toc1 649_20 | 3 Acidmaw melee on ranged (3:53-3:58), its holder paralysed; 5 Burning Bile (melee and a healer, 3:58-4:00, one Spray on the melee group with Acidmaw loose in the raid); 1 Sweep; 3 Molten Spew (healer at the 4:19 emerge, then the holder and the ranged that inherited the cone, 4:44-4:45, two healers down); Berserk 8:29 with Icehowl at 12.2%: 17 Massive Crash, 5 more |
| toc1 649_15 | 1 Icehowl melee on a melee (6:08) |
| toc2 649_22 | 1 Acidmaw melee on a melee (3:25), the holder's edge drag 2.5 s after its emerge taunt |
| toc2 649_16 | 1 Acidmaw melee on a melee (3:38), the Toxin-snared holder dragging 26 yd; 1 holder to enraged lone Dreadscale melee + Spew (4:39); 1 Icehowl melee on a tank |
| toc3 649_21 | 1 Dreadscale melee on a melee (3:08) |
| toc3 649_17 | 1 Slime Pool (stuck Toxin melee, no runner, 3:50); 1 lone Dreadscale melee on a ranged (4:59); 1 Icehowl melee on a tank |

**Verdict on round 2.** Held. Steady-state Spew wedges on a holder: 12 of 14 swing 0-2° with the
victim walking 0-1 yd. The two others aren't the drag or the hold's taunt: 649_16 3:40.6 (27°, the
tank that had just taunted Acidmaw off the casters fired the class `charge` at the cast start, 5
non-tank ticks, nobody died) and 649_20 4:41.6 (73°, the holder died mid Spew and the cone moved to a
ranged). Non-tank Spew ticks 54 yd and out: 2 of 55 (one melee at a 649_15 emerge), against 99 of 385.
No Spew death from a swept cone; the rest are emerge threat races and the wipe's cascade.

**Picked: Acidmaw takes a non-tank while its holder carries Toxin, 5 deaths and the wipe (bug).**
649_20: Bite at 3:23.6 put Toxin on holder Tocxaa (581.4, 106.2), 37.8 yd out, 7.4 yd in front of
Acidmaw (578.3, 113.0) facing it; pools dropped at the worm's feet at 3:32 and 3:44. Runner Tocxav took
Bile from the 3:33 Burning Spray (`nb.cure run` 3:36.4) and stood at (573.6, 127.8), 15.6 yd behind the
worm: `none run` from 3:37.8 to 3:57.3. Tocxaa paralysed ~3:37, sat 41 s in the pools; at 3:50.9
Acidmaw left it for the casters (Tocxaj, Tocxas, Tocxak dead by 3:58), Toxin `expired` at 4:39 with
`runner 40.6 s` no spot. Cause: a run or cure walk is a single straight leg (`PathBlocked`,
`ToCHelpers_Jormungars.cpp`). From behind the worm every straight walk to a spot within 8.5 yd of the
tank passes within 2-5.6 yd of the worm's centre, through the pool's path circle (radius at the walk's
end + 1, ~6 yd at 3:38); the far side is past the 44 yd floor; `partial` (`ToCActions_Jormungars.cpp`)
only runs past 30 yd. Same in window 1 (649_8, 3 deaths): runner Tocxak 6.4 yd from the stationary
Dreadscale, the tank south of it, every leg into the Sweep path circle; and 649_21 (`runner 9.2 s`).
A replica of the search on these moments finds no `direct` spot and a two-leg route in each
(649_20 3:38: 12 yd leg then 12 yd, 24 against 23 straight; 649_8: 24 + 50).

Fix: `detour` tier after `direct`: the first leg of the shortest two-leg walk to a direct spot, turn
point on 16 bearings × 6 yd rings out to 30, clear of the hard circles with the escape accept; the
second leg passes the full accept from the turn point (`acceptVia`; `PathBlocked` grows pools over
both legs). Walk-only like `partial`, released on arrival, so the next tick walks the rest.

**Second, not fixed: the mobile holder's drag loses Acidmaw to melee.** 649_22: edge drag at 3:23.5,
2.5 s after the emerge growl, the worm took a melee at 3:24.0. 649_16: the Toxin-snared bear dragged
26 yd at a crawl from 3:33, and as it left the worm's 14.8 yd melee range the worm took a melee
(3:37.4). The walking holder stops hitting while melee keep their 110% race. A fix (no drag on a thin
threat lead, or while snared) trades pools under the melee for the hold; for round 4 if it recurs.

**Not fixed, for later rounds.** The emerge threat race (Spew at once on a mobile emerge swings
15-68° across non-tanks: 649_20 4:19 killed a healer); a stuck Toxin melee with no runner died in a
pool (649_17, `runner 0.0 s`); the warrior class `charge` by a fresh holder mid Spew.

**Review fix.** 649_20's runner was not behind the worm by choice: its Spray dodge (`move 28 spray
escape` 3:33.9, walk-only) ran on after `nb.cure run` at 3:36.4 and released `arrived` at 3:37.8,
carrying it from (585.0, 123.1), 14 yd from a `direct` spot, to the blocked one. Same carry-over on
every window-4 run or seek begun mid dodge: 649_16 (1.3 s), 649_21 (1.7 s), 649_17 (3.0 s), and
649_8 twice (0.6, 0.2 s). A walk-only hold with no cure reason now releases `cure` once a cure walk
is owed and the bot isn't urgent (a cure walk skips the Spray rule; a pool escape still inside the
pool keeps walking). A replica of `direct`/`detour` on 649_20 3:36-3:57 and 649_8 3:31-3:48 agrees
with the lane's: no direct spot, a detour every 2 s sample (24-46 yd, 649_8 62-87).

**Next window should show.** `move <yd> run detour` (or `cure detour`) notes where `none run` streaks
were, each followed by a `direct`/`partner` walk and a cure; Toxin on the mobile holder ending `cured`
with `runner` no-spot under ~2 s; no Acidmaw kill on a non-tank while its holder carries Toxin; after
`nb.cure run`/`seek`, a `release cure` instead of a Spray dodge's `release arrived`.
