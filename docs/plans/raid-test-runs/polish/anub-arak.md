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
paladin B, blood DK C). The user's own raid killed 25H on this strategy; two
kills are on disk for comparison: CT `649_1_anub-arak_1794829976` and CU
`649_2_anub-arak_1794938939` (side tanks Bulwark, prot paladin, and Felesta,
blood DK).

**Re-pull state (r2).** Same as a first pull in cooldowns (DC resets them:
Shield Wall, Rebirth, Hymn of Hope reused 175-500 s apart), raid buffs and Well
Fed, health and positions at the engage. Only healer mana differs: 82-98% mean
at the engage (single healers 73-100%) against 100%, a DC refresh matter. By
wave 3 the healers stood at 58-80% either way, so it did not cause the short
second pulls; those died to the same side tank failure.

**Fight model.** He submerges 80 s after the engage and after each emerge, for
62 s; phase 3 starts at 30%, which needs about 23% off him per phase 1 to come
with two submerges, and the raid must then kill him before the 10 min Berserk.
Heroic burrower waves (4, 753k each, 2 per side tank) come 5 s and 50 s into
each phase 1, then every 45 s in phase 3, from the west exits. A side tank
holding two reaches 9 Expose Weakness stacks (+225% physical) for 25-60 s a
wave; at 9 stacks a landed hit is 15-19k on a bear (about 95k health), 13-15k on
a DK, 11-14k on a warrior, 9-13k on a paladin, and two can land in one tick. Off
Permafrost, after 30 s, a burrower under 80% submerges back to full.

**What kills raids.** A side tank goes from 80-100% to dead in under 6 s with no
long defensive ready (33 of 36 side tank deaths outside phase 3 in window 46; in
the other 3 a running Last Stand, Barkskin or Vampiric Blood blocked a ready
one, since `NextTankDefensive` casts one at a time), its burrowers go to healers
and ranged, the other side tank takes four, and the raid follows (burrower melee
28 deaths a pull). Two long defensives per side tank cover about two waves in
three. Against CT/CU, hit size per stack, swing speed (1.22 s), rotations and
phase 1 DPS (144-173k/s) match, but test side tanks are landed on 0.34-0.69
times a second of burrower contact against Bulwark's and Felesta's 0.29-0.37
(`--tanks`; paladins closest), so they take 6.9-11.6k/s of burrower damage in
phase 1 against 4.5-6.2k/s, with the healers already putting more on them.
The extra hits are swings the tank fails to avoid (r2 review, scratch read): a
burrower's next swing after a landed hit landed too about 71% of the time
(40-84%) against 51-66%. From the tank's back, where it cannot dodge, parry or
block, hits land 0.53-0.91 a second of contact against 0.18-0.57 from its front,
in CT/CU too; test side tanks had a burrower at their back 15-86% of contact
(median 28%) against 13-34%, and their front rate is up as well (median 0.33
against 0.27). So mostly the pool tanks' avoidance (gear or stats; the trace
does not carry them), partly facing.

**Compositions on 37cc72a94 (window 46, 12 valid pulls, all wipes).**

| Design | Pools | Outcome (f863bb8b6 in brackets) |
|---|---|---|
| A | toc1, toc4 | 3 of 4 reached phase 3, wipes at 9.2%, 11.1%, 30.0% (1 of 4) |
| B | toc2, toc5 | 1 of 4 reached phase 3, wipe at 9.6%; three collapsed at 47-52% (2 of 4) |
| C | toc3, toc6 | 1 of 4 reached phase 3, wipe at 20.6%; three collapsed at 31-63% (4 of 4) |

One build each, so no regression call on C.

**Open failures, ranked.**

1. Side tank death at 9 stacks with no long defensive ready, then the cascade:
   337 of 388 deaths. Picked r1 (bug, fixed), r2 (plan question Q1).
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
  from the pull until `Swarm`. Verdict r2: works (long casts at 50%+ by health
  after the last hit 4 of 79, was 67 of 85; no side tank died with one kept);
  first side tank deaths moved from a phase 1's second wave (11 of 12 → 6) to
  the first wave after a submerge (1 → 5), the defensives spent under 50% on the
  wave before still cooling.

**Ideas rejected.**

- Holding a bear side tank in Dire Bear Form between waves (r2): it leaves form
  once its burrowers die, casts Rejuvenation or Mark of the Wild (100
  Rejuvenations in 8 B/C pulls) and shifts back 0.2-1.5 s after the next wave
  spawns; 1 burrower hit landed on it out of form. Its rage refills from the
  first hit.
- r1's "test side tanks take bigger hits": at equal stacks hit sizes match
  CT/CU; the gap is how often they land.

**User answers in force.** The boss doc's user calls (east tank pair in heroic,
DPS split by side, the push, keep killing phase 3 waves, battle res as is, focus
once the side tank carries 6+ stacks). r1's defensive keep from the pull (kept
2026-11-25).

**Open questions.**

- **Q1 (r2): should heroic 25 get a fifth healer?** Recommended: yes, swap the
  roster's spare healer in for one DPS in DC's `totc-anubarak` scenario; no
  strategy code changes. All 12 window 46 wipes began when a side tank (the tank
  holding two burrowers) went from 80-100% health to dead in under 6 s at 9
  Expose Weakness stacks, almost always with its long cooldowns (Shield Wall,
  Icebound Fortitude and the like) still recharging from an earlier wave. Those
  cooldowns cover only about two waves in three. The bot side tanks take as much
  per hit as Felesta and Bulwark did in the user's kills, but get hit about 1.3
  times as often per second (more of the burrowers' swings land), so they take
  1.3-1.9 times the burrower damage with the same four healers. The cost is one
  DPS in 18; the closest wipes already needed a third submerge. The other choice
  is code: one healer sticks to each side tank while it holds burrowers. That
  takes healing from the main tank and the five Penetrating Cold carriers, so
  try it only if the fifth healer's healing goes to the raid instead of the side
  tanks. Review note: the extra hits are mostly swings the pool's side tanks
  fail to dodge, parry or block (a burrower's next swing after a landed hit
  lands about 71% of the time against 58% on Felesta and Bulwark), so compare
  their defense and avoidance with those two first: if DC's ilvl 245 tank gear
  is short there, a fifth healer would hide a gap the user's raid doesn't have.
  A third choice is code and a plan change: a burrower stands at a side tank's
  back about 28% of its contact time, where its hits land 2-3 times as often;
  keeping both in front would cut landed hits by up to a quarter.

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
