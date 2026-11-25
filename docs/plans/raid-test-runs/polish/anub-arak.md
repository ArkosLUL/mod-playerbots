# Polish log: Anub'arak (25-man heroic)

Reader `tools/botobs/bosses/anub_arak.py` (`--tanks`, `--burrowers`,
`--healers`, `--dps`, `--phases`, `--pull`; its `--summary` is the banner only),
tests in `tools/botobs/tests/test_anub_arak.py`. Older rounds:
`history/anub-arak.md`.

## State

**Setup.** Trace slug `anub-arak`, raid `toc`, DC scenario `totc-anubarak` (3
tanks, 4 healers at 25). A window runs one DC run of two pulls per pool: the
first waits out the Lich King scene, the second re-pulls in the same instance
after DC's revive and refresh (`OnPlayerEnter` replayed). Names: pool letters
x=toc1, y=toc2, z=toc3, u=toc4, v=toc5, w=toc6; slot `aa` main tank (paladin A
and C, warrior B), side tanks `ab` (bear, B and C) or `am` (blood DK, A) and
`az` (prot warrior A, prot paladin B, blood DK C). The main tank is the group's
main tank flag, else its first tank (`StableMainTankGuid`); the other tanks are
side tanks (`RankSideTanks`). The user's own raid killed 25H on this strategy;
three pulls are on disk for comparison, Ecoterrorist (bear) on him, side tanks
Bulwark (prot paladin) and Felesta (blood DK): kills CT
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
gates drop 6.5%, `fore` walks 2.6% (r4).

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
   first death of 11 of 12 pulls. **Stuck (r4):** picked r1 (defensive keep,
   works), r2 (Q1, answered: 4 healers), r3 (backing up, half works, no change
   in landed hits or deaths). Waits on Q2 and Q3.
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
  Permafrost (`anub.backup`). Verdict r4: half works. Backed-up walks land 0.28
  a second; walking behind fell from 27% to 21% of contact, but walking time
  rose and the landed rate held (0.36 in contact, windows 46 and 49 0.38 and
  0.36); 38% of the walking under `back` ran forward because the move gates
  ignore `backwards`: `IsWaitingForLastMove` made 2515 of the 2516 logged hold
  walk rejections in window 50, `IsDuplicateMove` 1.

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
once the side tank carries 6+ stacks). r1's defensive keep from the pull (kept
2026-11-25). Q1 (2026-11-25, restated for r4): polish with 4 healers for now,
no fifth healer; compare the pool side tanks' defense and avoidance with
Felesta's and Bulwark's (done r3: no gap) and build the facing fix (built r3).
- Q2 (2026-11-25): yes. In heroic, bots pick the side tanks by how they take
  burrower hits, prot paladin first, then blood DK, prot warrior, bear last, and
  the tank left over takes Anub'arak (at 10 man the same order picks the one side
  tank), instead of the main tank flag deciding; a human tank keeps the job the
  flag gives him. Expected: C the user's setup (bear on him, paladin plus DK on
  burrowers), A paladin plus DK (warrior on him), B paladin plus warrior (bear on
  him). Bears took 6.1k burrower damage a contact second against a paladin's
  3.5k and died first in 17 of 20 pulls (r4).
- Q3 (2026-11-25): no. The hold walk is not re-issued when the back-up decision
  flips; the stuck rule stands for r3's gap.

**Open questions.** None.

## Round 4 (build 0200b8827, window 50, stuck)

**Data.** `window50-anub-arak.md`: 12 valid pulls on r3's build, first then
re-pull per pool: toc1 `649_19_…1795614139`, `…4604`; toc4 `649_22_…4270`,
`…4687`; toc2 `649_20_…4259`, `…4734`; toc5 `649_23_…4313`, `…4765`; toc3
`649_21_…4269`, `…4745`; toc6 `649_24_…4394`, `…4848`. Baselines window 46 (12,
`…179560xxxx`) and window 49 (6, `649_19-25_…17956114xx-19xx`, built 14:27
before r3). `--tanks` and `--phases` on all 30; scratch reads: facing split per
composition, behind time by mover and `anub.backup`, gait under each value,
burrower damage per class, first side tank death by class.

**Deaths.** Burrower melee 318 of 386.

| Pull | Wipe | Burrower / other deaths | First side tank death |
|---|---|---|---|
| toc1 …4139 | P3 24.0% | 31 / Anub 2 | Tocxaz (warrior) 5:56 P1c |
| toc1 …4604 | P3 27.1% | 28 / Anub 3, Cold, Slash | Tocxam (DK) 1:09 P1 |
| toc4 …4270 | P2c 48.6% | 30 / Impale 2 | Tocuaz (warrior) 3:29 P1b |
| toc4 …4687 | P1c 60.7% | 33 / - | Tocuaz 1:18 P1 |
| toc2 …4259 | P3 20.4% | 25 / Anub 2, Swarm, Cold, Shadow Strike | Tocyab (bear) 5:59 P3 |
| toc2 …4734 | P3 21.0% | 27 / Mandibles 2, Anub 2 | Tocyab 5:53 P3 |
| toc5 …4313 | P3 26.8% | 27 / Anub 3, Cold | Tocvab (bear) 3:41 P1b |
| toc5 …4765 | P1b 61.2% | 26 / Impale, Anub, Mandibles, Shadow Strike, Slash | Tocvaz (paladin) 2:42 P1b |
| toc3 …4269 | P3 10.1% | 21 / Anub 4, Cold 4, Swarm 2, Shadow Strike, Mandibles | Toczab (bear) 1:11 P1 |
| toc3 …4745 | P3 5.3% | 21 / Cold 6, Anub 5, Swarm | Toczab 1:13 P1 |
| toc6 …4394 | P3 24.1% | 26 / Anub 3, Mandibles 2, Cold, Slash | Tocwab (bear) 2:43 P1b |
| toc6 …4848 | P3 14.5% | 23 / Mandibles 4, Cold 4, Anub 2 | Tocwab 5:59 P1c |

**Stop rules.** Not polished (no kill). Stuck: failure 1 was picked r1, r2 and
r3 and is still first (above); r3 set r4 to report stuck unless the user waived
it, and the prompt restated only the 4-healer answer. No strategy change.

**Previous fix (r3) verdict: half works, no effect on hits or deaths.**

| Contact before phase 3 | A (w46 / w49 → w50) | B | C |
|---|---|---|---|
| walking behind share | 31 / 30% → 26% | 26 / 24% → 16% | 24 / 25% → 21% |
| walking front share | 8 / 7% → 18% | 12 / 8% → 15% | 6 / 8% → 18% |
| landed a second | 0.39 / 0.37 → 0.39 | 0.38 / 0.33 → 0.32 | 0.37 / 0.39 → 0.37 |
| walking behind, landed a second | 0.60 / 0.57 → 0.68 | 0.61 / 0.56 → 0.57 | 0.62 / 0.60 → 0.71 |

Against r3's list: `anub.backup back` on all 24 side tanks, yes; walking behind
27% → 21% of contact, walking front 6-12% → 15-18%, yes; walking landed 0.49 a
second (0.54, 0.52), but walking behind 0.65 (0.61); all landed 0.40 with hits
from beyond 6 yd (0.43), 11 of 24 side tanks at 0.43+; first side tank death
(median, r2's table) A 2:23 (3:35), B 4:47 (2:58), C 1:58 (3:58): one build,
noise.

Where behind time went (hold walk, last `anub.backup`, share of contact, landed
a second): `frost` 9.6% 0.64, `back` 6.5% 0.69, `fore` 2.6% 0.76; `reach melee`
1.6%. In front: `back` 10.5% 0.28 (93% backward gait), `frost` 2.3% 0.40,
`fore` 2.2% 0.31. Before r3 (18 pulls) hold walks were 24.6% behind at 0.61 and
5.9% front at 0.35. Under `back`, 315 of 825 s walking ran forward (97% forward
gait). Of 257 flips to `back`, 26 issued a hold walk within 0.3 s, 50 after
`wait` rejections 0.3-2.6 s later, 181 none before the next change; 161 of the
257 met a logged `wait` rejection first, the rest none logged (rejections are
throttled to one a second per action in the trace). Cause:
`MovementAction::MoveToImpl` drops the move at `IsWaitingForLastMove`
(MovementActions.cpp:1059, same priority; 2515 of the 2516 logged hold walk
rejections) or `IsDuplicateMove` (:1047, same destination; 1), neither
comparing `backwards`, so a flip with the forward walk running changes nothing
(ToCActions_Anubarak.cpp:435-440).

**Side tank class (new read).** 30 pulls, before phase 3 or the tank's death:

| Class | Tank-pulls | Landed a second | Burrower damage a contact second | First side tank death |
|---|---|---|---|---|
| bear | 20 | 0.43 | 6.1k | 17 of 20 (B 9, C 8) |
| warrior | 10 | 0.45 | 5.1k | 7 of A's 10 |
| blood DK | 20 | 0.40 | 4.5k | 3 of A's 10, 2 of C's 10 |
| prot paladin | 10 | 0.38 | 3.5k | 1 of B's 10 |
| Felesta, Bulwark (CT, CU, CV) | 3 each | 0.33, 0.34 | 3.7k, 3.0k | Felesta in CU, Bulwark in CV, both in phase 3 |

The 4 bear deaths in window 50 with `anub.defensive kept` (Toczab 1:13, Tocvab
3:41, Tocwab 2:43 and 2:57) had frenzied regeneration ready, Tocvab survival
instincts too, and 94-100% top health in the 6 s before.

**Change.** None to the strategy. Boss doc: r3's measured result and the class
read. This log.

**Questions** (State has them in full). Q2, a plan change: in heroic pick the
side tanks by class (prot paladin, blood DK, prot warrior, bear last), the one
left over on him; recommended yes among bots. Q3: waive the stuck rule to
re-issue the hold walk when the back-up decision flips; recommended only with
Q2.

**Next window should show.** Same code, same failure. If Q2 is built: the bear
(B, C) and the warrior (A) on him; side tank burrower damage a contact second
nearer 3.5-4.5k; first side tank death later than this table per composition.
