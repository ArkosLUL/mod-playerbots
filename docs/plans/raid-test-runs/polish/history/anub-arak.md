# Anub'arak polish history (25-man heroic)

Rounds moved out of `../anub-arak.md`, oldest first.

## Round 1 (build f863bb8b6)

**Data.** Window report `check42-anub-arak.md` (job 1), its 15-row pull table.
Valid: toc2 `649_2_…1795595889`, `…1795596327`; toc5 `649_5_…1795595892`,
`…1795596073`; toc6 `649_6_…1795595893`, `649_7_…1795596939`; toc1
`649_1_…1795595889`, `649_8_…1795596970`; toc4 `649_4_…1795595891`,
`…1795596260`; toc3 `649_3_…1795595891`, `649_9_…1795597051`. Voided (engaged
before the start step): `649_6_…1795596344`, `649_1_…1795596367`,
`649_3_…1795596329`. Kill references CT and CU. Sections read: `--phases`,
`--burrowers`, `--tanks`, `--healers`, `--dps`.

**Deaths.** Burrower melee 420 of 487 in 15 pulls, then Anub'arak melee 21,
Penetrating Cold 18. First side tank death per valid pull:

| Pull | Wipe | First side tank death |
|---|---|---|
| toc2 …5889 | P3 6:29, 27.2% | Tocyab (bear) 1:06, wave 2 |
| toc2 …6327 | P2 1:42, 72.1% | Tocyab 1:03, wave 2 (three on him, two from wave 1) |
| toc5 …5892 | P2 2:15, 72.1% | Tocvaz (paladin) 1:05, wave 2 (Righteous Defense off a healer at 22% gave him four) |
| toc5 …6073 | P3 9:16, 15.6% | Tocvab 8:32, wave 8 |
| toc6 …5893 | P3 6:54, 22.1% | Tocwaz (DK) 5:46, wave 6 |
| toc6 …6939 | P3 7:11, 21.2% | Tocwaz 3:31, wave 4 |
| toc1 …5889 | P1d 7:19, 35.5% | Tocxam (DK) 3:28, wave 4 |
| toc1 …6970 | P2c 7:01, 31.4% | Tocxaz (warrior) 1:07, wave 2 |
| toc4 …5891 | P1c 5:23, 57.7% | Tocuaz (warrior) 1:09, wave 2 |
| toc4 …6260 | P3 8:32, 29.9% | Tocuaz 6:04, wave 6 |
| toc3 …5891 | P3 6:44, 22.5% | Toczab (bear) 5:15, wave 5 |
| toc3 …7051 | P3 7:06, 10.6% | Toczaz (DK) 6:07, wave 6 |

**Failure picked: side tanks die on a phase 1's second wave with every long
defensive cooling.**

- `AnubarakSideTankNeedsDefensive` (`ToCHelpers_Anubarak.cpp:5564`) fires at 5
  Expose Weakness stacks whatever the tank's health, and `NextTankDefensive`
  (`RaidTankDefensive.cpp:59`, weakest first) hands warriors Last Stand then
  Shield Wall, paladins Divine Protection, bears Barkskin then Frenzied
  Regeneration and Survival Instincts, DKs Vampiric Blood then Icebound
  Fortitude. The keep (`AnubarakSideTankDefensiveKept`, `:5572`) held the 120 s+
  ones only from his 50%.
- So each phase 1's first wave took them: 67 of 85 long casts outside phase 3
  went at 50%+ health (`--tanks`), for example toc4 …5891 Tocuaz Last Stand
  0:21.2 at 74% and Shield Wall 0:38.3 at 93%, toc1 …6970 Tocxaz 83% and 74%,
  toc5 …5892 Tocvaz Divine Protection 0:21.1 at 86%.
- Every first side tank death came with each of its long defensives cast 21-147
  s before (`--tanks` death rows, within the 120-300 s cooldowns): Tocuaz 1:09.6
  under two burrowers, 100% to 3% in 15 s, Shield Wall 31 s and Last Stand 48 s
  before.
- Healing is not the gap: in phase 1 the side tanks of four test pulls received
  219-420k heals, CT's 257k and 420k, CU's 133k and 224k.

**Classification: bug.** The defensive is meant to carry the side tank through
the waves; spent at full health on the wave that didn't need it, it is gone for
the one that kills.

**Fix.** `AnubarakSideTankDefensiveKept` keeps a 120 s+ pick while the tank is
at 50%+ on heroic from the pull to `Swarm`, not only from his 50%
(`ANUBARAK_SIDE_DEFENSIVE_KEEP_PCT` removed). Barkskin and Vampiric Blood still
go at 5 stacks; normal is unchanged. Reader: `--tanks` lists the long defensives
a side tank cast outside phase 3 at 50%+ health and, per side tank death, its
last cast of each.

**Next window should show.** `--tanks` long defensives at 50%+ outside phase 3:
0 of N on every pull (`anub.defensive kept` rows instead), and fewer first side
tank deaths on waves 2 and 4, each with a long defensive cast under 50% in that
wave. Judge per composition; A's warriors and B's paladin, whose only long
buttons these are, should gain most.

**Previous fix.** None (first round).

## Round 2 (build 37cc72a94, window 46)

**Data.** `window46-anub-arak.md` (job 2: r1 plus the ritual walk fix
21376d9f9), 12 valid pulls, first then second per pool: toc1
`649_7_…1795603120`, `…1795603732`; toc4 `649_10_…1795603232`, `…1795603841`;
toc2 `649_8_…1795603140`, `…1795603740`; toc5 `649_11_…1795603242`,
`…1795603659`; toc3 `649_9_…1795603202`, `…1795603473`; toc6
`649_12_…1795603253`, `…1795603727`. Kill references CT, CU. Sections:
`--tanks`, `--phases`, `--burrowers`, `--dps`, `--healers`, `--pull`; scratch
reads of death auras, mana, cooldown reuse, heal split.

**Deaths.** Burrower melee 337 of 388, Anub'arak melee 18, Penetrating Cold 12.
First side tank death per pull:

| Pull | Wipe | First side tank death |
|---|---|---|
| toc1 …3120 | P3 9:12, 9.2% | Tocxaz (warrior) 5:52, wave 6 |
| toc1 …3732 | P2b 4:04, 63.6% | Tocxaz 0:26, wave 1 |
| toc4 …3232 | P3 9:22, 11.1% | Tocuaz (warrior) 6:01, wave 6 |
| toc4 …3841 | P3 8:21, 30.0% | Tocuaz 1:18, wave 2 |
| toc2 …3140 | P3 8:56, 9.6% | Tocyab (bear) 8:09, wave 8 |
| toc2 …3740 | P2b 4:32, 48.2% | Tocyab 3:02, wave 3 |
| toc5 …3242 | P1c 6:10, 47.1% | Tocvab (bear) 1:07, wave 2 (battle res, lived to 5:14) |
| toc5 …3659 | P2b 4:01, 51.7% | Tocvab 2:54, wave 3 |
| toc3 …3202 | P1b 3:43, 62.6% | Toczab (bear) 2:45, wave 3 |
| toc3 …3473 | P1c 6:08, 30.6% | Toczab 5:07, wave 5 |
| toc6 …3253 | P3 7:01, 20.6% | Tocwab (bear) 5:54, wave 6 |
| toc6 …3727 | P2b 4:32, 55.6% | Tocwaz (DK) 2:48, wave 3 |

**Re-pull.** Second pulls ran 4:01-8:21 against 3:43-9:22. Death auras: same
raid buffs and Well Fed on all 25 per pull. Cooldowns reset (toc1 Tocxaz Shield
Wall at 7:44 and again 2:55 later at 0:27). Healer mana at the engage: toc1
100/85/85/85, toc3 100/80/81/88, toc4 96/86/82/82, toc5 90/81/84/73, toc6
100/90/97/85, toc2 100/100/100/93; at 2:30-2:50 mean 58-80% in both pulls, no
consistent gap. Engage positions match (`--pull`). Answer: a DC refresh matter
(mana), not the cause; the short second pulls are the side tank failure on wave
3, which toc3's first pull shows too.

**Failure picked: side tanks die at 9 stacks with no long defensive ready.**

- No keep at a death: the last `anub.defensive` before each of 36 side tank
  deaths outside phase 3 is a cast or `covered`, never `kept`; their highest
  health in the 6 s before was 80-100% (one 79.6%). By cast times and death
  auras a long defensive was ready at 3 of them, each blocked by a running one:
  toc1 …3732 Tocxaz 0:26 (Last Stand up, Shield Wall ready), toc3 …3473 Toczab
  5:07 (Barkskin, Frenzied Regeneration), Toczaz 5:09 (Vampiric Blood, Icebound
  Fortitude).
- One tick kills: toc5 …3242 Tocvab 1:07.5 at 55% took 23.3k and 24.8k at
  1:07.827 (postmortem death 0).
- Per hit at 9 stacks test equals CT/CU per class (paladin 9.0-13.3k vs Bulwark
  9.6-12.5k, DK 12.6-15.2k vs Felesta 13.8-15.5k); landed burrower hits a second
  of contact 0.34-0.69 vs 0.29-0.37; side tank burrower damage in phase 1
  6.9-11.6k/s vs 4.5-6.2k/s (CT 1.48M on both side tanks to 5:43, toc1 …3120
  2.5M to 5:52).
- Healing: in the 12 s before each first side tank death the dying tank got
  about a third of the four healers' output, the rest the main tank, the other
  side tank and DPS (toc3 …3202 Toczaf 0 on Toczab, 54k on DPS). Over phase 1
  the side tanks already get 38-59% of it (median 45%) against CT/CU's 39%.
- Rotations match the kills (casts a minute: Felesta Rune Strike 8.8-9.7, Blood
  Boil 5.5-5.9; test DKs 7.3-11.5, 4.8-8.1; Bulwark and test paladins Holy
  Shield 4.8-5.8); burrower lives 30-46 s against 32-43 s.
- Review, scratch reads over the same contact and landed hits as `--tanks`:
  - Gaps between one burrower's consecutive landed hits on a side tank: the
    commonest is 1.2 s in 13 of 14 pulls, 1.3 s in CU, so swings come as fast; one-swing
    gaps over one- to three-swing gaps (0.95-1.5, 2.15-2.75, 3.35-4.0 s) 0.40-0.84
    (median 0.71) against CT/CU 0.51-0.66.
  - Split by the burrower inside or outside the tank's front 180° at the
    snapshot (no dodge, parry or block from behind): front 0.18-0.57 landed a
    second of contact (median 0.33), back 0.53-0.91, back share 15-86% (median
    28%); CT/CU front 0.19-0.28, back 0.53-0.70, back share 13-34%.

**Classification: plan change** (Q1). Nothing the strategy intends fails: the
keep holds, defensives go under 50%, rotations match.

**Change.** Reader only. `--tanks`: a defensive's health is now after the last
logged hit or heal when that is newer than the snapshot (the snapshot overstated
it by up to 30 points: Tocxaz Last Stand 0:20.0 read 55%, was 38%); burrower
contact seconds and landed hits a second per side tank before phase 3; each tank
death's last `anub.defensive` and highest health in the 6 s before. `--pull`:
healer mana at the engage. Tests added.

**Next window should show.** Same code, so the same failure. With Q1 answered (a
fifth healer): side tank burrower damage healed per second up, the first side
tank death later than wave 3 in most pulls, and phase 1 loss per phase still
near 20%.

**Previous fix (r1) verdict.** Works as built: long defensives at 50%+ 4 of 79
(A 2, B 1, C 1). By composition the first side tank death came on a phase 1's
second wave in A 3 of 4 (r1 4 of 4), B 2 of 4 (r1 4 of 4), C 1 of 4 (r1 3 of 4);
on the first wave after a submerge in A 0, B 2, C 3 (r1: C 1). A gained (3 of 4
to phase 3), C lost (1 of 4); one build, so no call.

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
| standing front | 0.29 | 0.23 |
| walking behind | 0.61 | 0.51 |
| walking front | 0.34 | 0.36 |
| standing behind (seconds) | 66 | 12 |
| hits from beyond 6 yd (apart) | 232 | 49 |

Split rates count only hits with the burrower within 6 yd at the snapshot, as
the contact seconds do (review fix; counting the rest read walking behind 0.70
and 0.59, walking front 0.52 and 0.50).

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
the slow). Rim walks are ~14% of test contact at 0.61 a second (~23% of the
in-contact landed hits); backed up they take ~22%: at the walking-front 0.34 a
second that saves ~13% of a rim walk's hits, ~3% of all landed burrower hits
(~6% at the standing 0.29); at 0.39 a second it saves none. Too small to read off
one window's deaths, so judge it by the split.

**Next window should show.** `anub.backup back` rows on every bot side tank;
`--tanks` walking-behind seconds down (test 27% of contact) and walking-front
up; walking landed a second under 0.61; all landed a second under 0.43 per side
tank; first side tank death later than r2's table, per composition. If walking
hits a second sit near 0.39 and the total doesn't drop, backing up only trades
speed: ask the rim crawl question.

**Previous fix (r2) verdict.** r2 changed only the reader and no pulls ran
since, so r1's verdict stands (works).
