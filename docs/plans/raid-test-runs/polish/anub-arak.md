# Polish log: Anub'arak (25-man heroic)

Reader `tools/botobs/bosses/anub_arak.py` (`--tanks`, `--burrowers`,
`--healers`, `--dps`, `--phases`, `--pull`; its `--summary` is the banner only),
tests in `tools/botobs/tests/test_anub_arak.py`. Older rounds:
`history/anub-arak.md`.

## State

**Setup.** Trace slug `anub-arak`, raid `toc`, DC scenario `totc-anubarak` (3
tanks at 25 and 10, 4 healers at 25, 2 at 10). A window runs one DC run of two pulls per pool:
the first waits out the Lich King scene, the second re-pulls in the same
instance after DC's revive and refresh (`OnPlayerEnter` replayed). Names: pool
letters x=toc1, y=toc2, z=toc3, u=toc4, v=toc5, w=toc6; tanks `aa` (DC's main
tank flag and its aggro start target: paladin A and C, warrior B), `ab` (bear, B
and C) or `am` (blood DK, A), `az` (prot warrior A, prot paladin B, blood DK C).
Up to r4 the flag holder took him and the others the sides. From r5 heroic picks
by class (boss doc, `AnubarakMainTank`, `RankSideTanks`): on him A `az`
(warrior), B and C `ab` (bear); side tanks A `aa` + `am` (paladin, DK), B `aa` +
`az` (warrior, paladin), C `aa` + `az` (paladin, DK). The user's own raid killed
25H on this strategy; three pulls are on disk for comparison, Ecoterrorist
(bear) on him, side tanks Bulwark (prot paladin) and Felesta (blood DK): kills CT
`649_1_anub-arak_1794829976` and CU `649_2_anub-arak_1794938939`, and CV
`649_1_anub-arak_1795027085` (phase 3 wipe finished by `.die`; its phase 1s
count).

**Re-pull state (r2).** Same as a first pull in cooldowns, raid buffs, Well Fed,
health and positions at the engage; only healer mana differs (82-98% mean),
a DC refresh matter that did not cause the short second pulls.

**Fight model.** He submerges 80 s after the engage and after each emerge, for
62 s; phase 3 starts at 30%, which needs about 23% off him per phase 1 to come
with two submerges, and the raid must then kill him before the 10 min Berserk.
Heroic burrower waves (4, level 83, 753k each, 2 per side tank) come 5 s and
50 s into each phase 1, then every 45 s in phase 3, from the west exits. A side
tank holding two reaches 9 Expose Weakness stacks (+225% physical) for 25-60 s a
wave; at 9 stacks a landed hit is 15-19k on a bear (about 95k health), 13-15k on
a DK, 11-14k on a warrior, 9-13k on a paladin, and two can land in one tick. Off
Permafrost, after 30 s, a burrower under 80% submerges back to full. The side
tank's hold is the patch centre while a burrower on it is off Permafrost, then
the rim 7 yd toward him: a 5 s crawl under the patch's 80% slow.

**What kills raids.** A side tank goes from 80-100% to dead in under 6 s with no
long defensive ready or one kept by r1 (4 bear deaths in window 50), its
burrowers go to healers and ranged, the other side tank takes four, and the raid
follows. The bear is the weak side tank (r4, 30 pulls): burrower damage a
contact second before phase 3 bear 6.1k, warrior 5.1k, DK 4.5k, paladin 3.5k
(Felesta 3.7k, Bulwark 3.0k); a bear side tank died first in 17 of the 20 pulls
with one, the warrior in 7 of A's 10, the paladin in 1 of B's 10. Avoidance and
hit size match the user's tanks (r3). Facing: a burrower behind lands 0.6-0.7
hits a second against 0.28 in front, walking or standing; behind time on r3's
build is forward walks: centre walks 9.6% of contact, `back` decisions the move
gates drop 6.5%, `fore` walks 2.6% (r4). In the user's kills he opened on
Felesta and Ecoterrorist took him at 7.7-7.8 s (r5).

**Compositions on 0200b8827 (window 50, 12 valid pulls, all wipes).**

| Design | Pools | Outcome (window 49 pre-r3, one a pool, in brackets) |
|---|---|---|
| A | toc1, toc4 | 2 of 4 reached phase 3, wipes at 24.0%, 27.1%; toc4 48.6%, 60.7% (2 of 2: 4.0%, 20.6%) |
| B | toc2, toc5 | 3 of 4 reached phase 3, wipes at 20.4-26.8%; one at 61.2% (1 of 2: 16.5%) |
| C | toc3, toc6 | 4 of 4 reached phase 3, wipes at 5.3%, 10.1%, 14.5%, 24.1% (2 of 2: 11.8%, 19.4%) |

Phase 3 reach went from 4 of 12 (window 46) to 5 of 6 (window 49) before r3,
so not r3's doing; in window 50, 8 of the 9 reached it at 5:40-6:03 after two
submerges, toc6 `…4848` at 7:28 after three.

**Open failures, ranked.**

1. Side tank death, then the cascade: burrower melee 318 of 386 deaths; the
   first death of 11 of 12 pulls. Picked r1 (defensive keep, works), r2 (Q1,
   answered: 4 healers), r3 (backing up, half works), stuck r4; r5 builds the
   user's Q2 (tank jobs by class), so the bear leaves the burrowers.
2. Phase 3: 9 of 12 reached it, wiped at 5.3-27.1%, each after side tanks died.
3. Anub'arak melee 27, Penetrating Cold 18, Mandibles 10, Leeching Swarm 4,
   Impale 3, Freezing Slash 3, Shadow Strike 3: mostly inside collapses.
4. A running defensive blocks a ready long one (shared `RaidTankDefensive.cpp`,
   every boss using it pulled again); r1's keep at 50% is too late for a bear
   that drops from 94-100% in under 6 s (4 deaths in window 50).

**Fixes in place.**

- r1: heroic side tank defensives with a 120 s+ cooldown kept at 50%+ health
  from the pull until `Swarm`. Verdict r2: works (long casts at 50%+ 4 of 79,
  was 67 of 85).
- r3: the hold walk backs up, facing its burrowers, while all are on
  Permafrost (`anub.backup`). Verdict r4: half works (backed-up walks land 0.28
  a second, but 38% of the walking under `back` ran forward through the move
  gates; landed rate and deaths unchanged).
- r5: heroic tank jobs by class (Q2) and the main tank taunts him off a bot
  side tank. Verdict: due next window.

**Ideas rejected.**

- Holding a bear side tank in Dire Bear Form between waves (r2): it leaves form
  once its burrowers die and shifts back 0.2-1.5 s after the next wave spawns;
  1 burrower hit landed on it out of form.
- r1's "test side tanks take bigger hits": at equal stacks hit sizes match.
- r3: a gear gap for the roster: none found.
- r3: backing up on the walk to the centre too: a burrower off Permafrost there
  needs frost first, and backing up is 1.56 times slower. Still the biggest
  behind time (9.6% of contact at 0.64, r4); a lead if Q3 reopens facing.
- r4: the rim crawl question r3 held back: a facing walk lands 0.28-0.29 a
  second like standing (0.28), so the crawl costs nothing once it faces.

**User answers in force.** The boss doc's user calls (east tank pair in heroic,
DPS split by side, the push, keep killing phase 3 waves, battle res as is, focus
once the side tank carries 6+ stacks, heroic tank jobs by class). r1's defensive
keep from the pull (kept 2026-11-25). Q1 (2026-11-25, restated for r4): polish
with 4 healers for now, no fifth healer; compare the pool side tanks' defense
and avoidance with Felesta's and Bulwark's (done r3: no gap) and build the
facing fix (built r3).
- Q2 (2026-11-25): yes, built r5. In heroic, bots pick the side tanks paladin,
  DK, warrior, bear, and the tank left over takes Anub'arak (at 10 man the same
  order picks the one side tank); a human tank keeps the job the main tank flag
  gives him and the bots fill the other jobs by that order, one code path with
  or without a real player. This waives r4's stuck rule for failure 1.
- Q3 (2026-11-25): no. The hold walk is not re-issued when the back-up decision
  flips; the stuck rule stands for r3's gap.

**Open questions.** None.

## Round 5 (build 0200b8827, no new pulls, Q2 built)

**Data.** No pulls since r4: `window50-anub-arak.md` (12 valid pulls on r3's
build, read in r4). New read, his first melee targets: the pools' toc1
`649_19_…4139`, toc2 `649_20_…4259`, toc3 `649_21_…4269` opened on `aa` at
0.6-1.1 s and it kept him; CT opened on Felesta (2 hits), Ecoterrorist from
7.8 s, CU Felesta 5 hits, Ecoterrorist from 7.7 s. DC's scenario row
(`DcTestDungeonRegistry.cpp:475`, 3 tanks at 25 and 10) and its aggro start
(`DcTestRunRepull.cpp:504` `EngageWithTarget(tank)`, 0 threat, `tank` = the
flagged slot, `DcTestRunJob.cpp:1220`).

**Deaths.** As r4: burrower melee 318 of 386; the first death a side tank in 11
of 12 pulls.

**Stop rules.** Not polished (no kill). Stuck since r4 on failure 1; the user's
Q2 answer (yes) waives it for this build, Q3 (no) leaves the hold walk as is.

**Previous fix.** r4 changed nothing; r3's verdict stands (half works).

**Failure and cause.** Failure 1 through the roster: the jobs came from the flag
(`StableMainTankGuid`: the flag holder on him; `RankSideTanks`: the other tanks
by bots first, assistants, group order), so B and C held burrowers with the bear
(6.1k a contact second, first to die in 17 of its 20 pulls) and A with the
warrior (5.1k) while the paladin (3.5k) sat on him in A and C.

**Change (Q2, a plan change the user asked for).**

- `ToCHelpers_Anubarak.cpp` `AnubarakMainTank`: heroic, a flagged human tank
  keeps him; else the tank-strategy bots, alive and in the map, rank paladin, DK,
  warrior, druid, assistants first within a class, and the one ranked third
  takes him (second of two, the only one), latched in `state.mainTank` for the pull
  (`ClearPullLatches` clears it), so a side tank's death never moves him. Normal
  keeps the flag, else the first living tank.
- `RankSideTanks`: heroic ranks bots before humans as before, then tank-strategy
  before tank-by-spec (a cat with Thick Hide never outranks a real tank), class,
  assistants. Dropped the `walked` skip: a no-op, since the walked main is the
  first living tank and nothing before it passed the same filter.
  `GetAnubarakPrepullTanks` uses both, so the pre-pull stands match the pull.
- `ToCActions_Anubarak.cpp` main tank hold: also taunts him off a bot side tank
  (`anub.pickup taunt`). The aggro start opens him on `aa`, a side tank in every
  design now, and the hold only taunted off a non-tank while `anubarak taunt
  guard` blocks the class taunt with him on a tank.
- 10 man: the same order; two tanks give the better one the side, DC's three
  give two sides.
- Boss doc: the heroic pick, taunt guard, `pickup` row. No shared code.

Picks per design (25H; the 10H check the same): A on him `az` warrior,
burrowers `aa` paladin + `am` DK; B on him `ab` bear, burrowers `aa` warrior +
`az` paladin; C on him `ab` bear, burrowers `aa` paladin + `az` DK (the user's
setup). Class spells the flag aims (`... on main tank`: Beacon of Light, Sacred
Shield, Earth Shield, Power Word: Shield, Renew, Lifebloom, Fear Ward, Thorns)
now land on `aa`, a side tank; Misdirection and Tricks stay blocked all fight
(`anubarak redirect guard`).

**Next window should show.** `anub.side0`/`side1` the side tanks above, `--tanks`
naming the main tank above; `anub.pickup taunt` from him in the first seconds
and him off `aa` within about 2 s, not the user's 7.7 s. Side tank burrower
damage a contact second nearer 3.5-4.5k (B was bear + paladin, now warrior +
paladin; C bear + DK, now paladin + DK; A warrior + DK, now paladin + DK); first
side tank death later than r4's medians (A 2:23, B 4:47, C 1:58); burrower melee
under 318 of 386. Watch the bear on him in B and C (the user's kills had one)
and the warrior in A, and whether the paladin side tank holds where B's did (1
first death in 10). No change in what the hold walk does.
