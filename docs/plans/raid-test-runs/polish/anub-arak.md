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
x=toc1, y=toc2, z=toc3, u=toc4, v=toc5, w=toc6; slot `aa` main tank, side tanks
`ab` (bear, B and C) or `am` (blood DK, A) and `az` (prot warrior A, prot
paladin B, blood DK C). The user's own raid killed 25H on this strategy; three
pulls are on disk for comparison, side tanks Bulwark (prot paladin) and Felesta
(blood DK): kills CT `649_1_anub-arak_1794829976` and CU
`649_2_anub-arak_1794938939`, and CV `649_1_anub-arak_1795027085` (phase 3
wipe finished by `.die`; its phase 1s count).

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
long defensive ready (33 of 36 side tank deaths outside phase 3 in window 46;
in the other 3 a running defensive blocked a ready one, since
`NextTankDefensive` casts one at a time), its burrowers go to healers and
ranged, the other side tank takes four, and the raid follows (burrower melee 28
deaths a pull). Two long defensives per side tank cover about two waves in
three. Test side tanks take 0.43 landed burrower hits a second of contact
against Bulwark's and Felesta's 0.34 (r3 `--tanks`), with the same hit size per
stack and the same four healers. Not gear (r3): per swing from the front both
land 0.55, and no tank shows crits. The gap is a burrower behind the tank 29% of
contact (CT/CU/CV 21%), 95% of it on the tank's hold walk, landing 0.70 a second
(0.59) against 0.29 standing in front (0.24), plus more swings a second in front.

**Compositions on 37cc72a94 (window 46, 12 valid pulls, all wipes).**

| Design | Pools | Outcome (f863bb8b6 in brackets) |
|---|---|---|
| A | toc1, toc4 | 3 of 4 reached phase 3, wipes at 9.2%, 11.1%, 30.0% (1 of 4) |
| B | toc2, toc5 | 1 of 4 reached phase 3, wipe at 9.6%; three collapsed at 47-52% (2 of 4) |
| C | toc3, toc6 | 1 of 4 reached phase 3, wipe at 20.6%; three collapsed at 31-63% (4 of 4) |

One build each, so no regression call on C.

**Open failures, ranked.**

1. Side tank death at 9 stacks with no long defensive ready, then the cascade:
   337 of 388 deaths. Picked r1 (bug, fixed), r2 (plan question Q1), r3 (Q1's
   facing fix). r3 builds the user's own Q1 answer, so it doesn't count toward
   stuck; if the window after r3 still shows it, r4 reports stuck unless the
   user waives it.
2. Phase 3: 4 of 12 pulls reached it, all wiped after side tanks died. The near
   kills (9.2-11.1%) started it at 7:26-7:45 after a third submerge (P1b lost
   19-20%); CT/CU started it at 5:43-5:47 after two (P1b 25%).
3. Anub'arak melee 18, Penetrating Cold 12, Mandibles 7, Impale 6, Shadow Strike
   5: mostly inside collapses.
4. A running defensive blocks a ready long one (3 of 36 side tank deaths,
   above). Shared `RaidTankDefensive.cpp`, so every boss using it would be
   pulled again; not worth it at that count.

**Fixes in place.**

- r1: heroic side tank defensives with a 120 s+ cooldown kept at 50%+ health
  from the pull until `Swarm`. Verdict r2: works (long casts at 50%+ 4 of 79,
  was 67 of 85; no side tank died with one kept); first side tank deaths moved
  to the first wave after a submerge.
- r3: the hold walk backs up, facing its burrowers, while all are on
  Permafrost (`anub.backup`). Verdict pending.

**Ideas rejected.**

- Holding a bear side tank in Dire Bear Form between waves (r2): it leaves form
  once its burrowers die and shifts back 0.2-1.5 s after the next wave spawns;
  1 burrower hit landed on it out of form.
- r1's "test side tanks take bigger hits": at equal stacks hit sizes match.
- r3: a gear gap for the roster: none found (above).
- r3: backing up on the walk to the centre too: a burrower off Permafrost there
  needs frost first, and backing up is 1.56 times slower.

**User answers in force.** The boss doc's user calls (east tank pair in heroic,
DPS split by side, the push, keep killing phase 3 waves, battle res as is, focus
once the side tank carries 6+ stacks). r1's defensive keep from the pull (kept
2026-11-25). Q1 (2026-11-25): polish with 4 healers for now, no fifth healer;
compare the pool side tanks' defense and avoidance with Felesta's and Bulwark's
(done r3: no gap) and build the facing fix (built r3).

**Open questions.** None. If backing up leaves walking hits about equal (see
r3), the next lever is a plan change to ask first: the 7 yd rim crawl itself.

## Round 3 (build 37cc72a94, window 46, no new pulls)

**Data.** `window46-anub-arak.md` as r2 (12 valid pulls; r2 changed only the
reader). Kill references CT, CU, CV. New `--tanks` facing split over all 15;
scratch reads: walks behind by mover, hold walk start distance and hold kind,
burrower hit size per Expose Weakness stack, gaps between landed hits.

**Deaths.** As r2: burrower melee 337 of 388, Anub'arak melee 18, Penetrating
Cold 12; every wipe began with a side tank death (first ones r2's table).

**Avoidance comparison (Q1).** The trace carries no stats and reading gear needs
the database (Docker), so this compares what the stats do to burrower swings:

- Per swing standing with the burrower in front (gaps up to 6 s between one
  burrower's landed hits, 1.22 s a swing): test 0.55 (614 gaps), user 0.55
  (139). Paladins Tocvaz 0.54, Tocyaz 0.51 vs Bulwark 0.57; DKs Tocuam 0.56,
  Tocwaz 0.45, Tocxam 0.51, Toczaz 0.56 vs Felesta 0.50; bears 0.51-0.63,
  warriors 0.59-0.62 (no counterpart).
- Defense: burrowers are level 83, so a tank under 540 would take crits; no
  tank's hits, scaled by 1 + 0.25 a stack, cluster at twice its median (the
  few at 1.62-1.81 are on paladins, Bulwark too, and one bear). Median scaled hit: pool paladins 3.89k
  vs Bulwark 3.66k, DKs 4.22-5.03k vs Felesta 4.87k.
- So no gear gap for the roster.

**Failure picked: side tank burrower hits, Q1's facing part.**

| Contact (before phase 3) | Test (12) | User (CT, CU, CV) |
|---|---|---|
| seconds | 5036 | 1467 |
| walking / behind share | 36% / 29% | 30% / 21% |
| landed a second, all | 0.43 | 0.34 |
| standing front | 0.29 | 0.24 |
| walking behind | 0.70 | 0.59 |
| walking front | 0.52 | 0.50 |
| standing behind (seconds) | 66 | 12 |

- Behind while walking is 95% of behind time; its mover was `anubarak assist
  tank hold burrower` 91% (6 test pulls, 853 of 932 s) and 93% (user).
- By hold: walks to the rim (centre to rim, rim re-issues) carry 53% of it
  (test 696 of 1310 s, user 165 of 315), walks to the centre the rest.
- Cause: the hold walk `MoveTo` runs forward (`ToCActions_Anubarak.cpp` hold
  walk, `GetAnubarakSideTankHold`), so a walking unit faces its path and the
  burrowers trail at its back, where the core rolls no dodge, parry or block.
- Per-hit size and per-swing avoidance match, so the rest of the gap is time
  (walking 36% vs 30%) and swings a second in front (0.29 vs 0.24 landed).

**Change.** `AnubarakSideTankBacksUp`: with every burrower on the side tank on
Permafrost and more of those within 8 yd behind the walk than ahead
(`BackUpKeepsFront`, pure, native test), the hold walk goes backwards; the spike
check times it at `MOVE_RUN_BACK`. `anub.backup` `back`/`fore`/`none`/`frost`.
Reader `--tanks`: the facing split per side tank, landed a swing, `anub.backup`
rows; tests. Boss doc: the rule and r3's facts. No shared code.

**Expected size.** Backing up is 1.56 times slower (4.5 vs 7 yd/s, both under
the slow). Rim walks are ~14% of test contact at 0.70 a second (~23% of landed
hits); backed up they take ~22% at the in-front rate: 0.29 a second saves ~8% of
all landed burrower hits, 0.45 (every swing) saves none.

**Next window should show.** `anub.backup back` rows on every bot side tank;
`--tanks` walking-behind seconds down (test 27% of contact) and walking-front
up; walking landed a second under 0.70; all landed a second under 0.43 per side
tank; first side tank death later than r2's table, per composition. If walking
hits a second sit near 0.45 and the total doesn't drop, backing up only trades
speed: ask the rim crawl question.

**Previous fix (r2) verdict.** r2 changed only the reader and no pulls ran
since, so r1's verdict stands (works).
