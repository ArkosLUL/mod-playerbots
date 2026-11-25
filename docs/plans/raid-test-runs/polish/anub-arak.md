# Polish log: Anub'arak (25-man heroic)

Reader `tools/botobs/bosses/anub_arak.py` (`--tanks`, `--burrowers`, `--healers`, `--dps`, `--phases`; its
`--summary` is the banner only), tests in `tools/botobs/tests/test_anub_arak.py`. Older rounds:
`history/anub-arak.md`.

## State

**Setup.** Trace slug `anub-arak`, raid `toc`, DC scenario `totc-anubarak` (3 tanks, 4 healers at 25). The first
pull waits out the Lich King scene; a re-pull replays `OnPlayerEnter`. Second pulls that engaged before DC's start
step are voided (3 of 6 on f863bb8b6); a DC lane fixes that. Names: pool letters x=toc1, y=toc2, z=toc3, u=toc4,
v=toc5, w=toc6; slot `aa` main tank, side tanks `ab` (bear, B and C) or `am` (blood DK, A) and `az` (prot warrior
A, prot paladin B, blood DK C). The user's own raid killed 25H on this strategy; two kills are still on disk for
comparison: CT `649_1_anub-arak_1794829976` and CU `649_2_anub-arak_1794938939`.

**Fight model.** He submerges 80 s after the engage and after each emerge, for 62 s; phase 3 starts at 30%, which
needs about 20% off him per phase 1, and the raid must then kill him before the 10 min Berserk. Heroic burrower
waves (4, 753k each, 2 per side tank) come 5 s and 50 s into each phase 1, then every 45 s in phase 3, from the west
exits, reaching the east tank pair 15-20 s later. A side tank holding two reaches 9 Expose Weakness stacks (+225%
physical) for 25-45 s a wave. Off Permafrost, after 30 s, a burrower under 80% submerges back to full.

**What kills raids.** A side tank dies under its burrowers at 9 stacks, they go to healers and ranged, the other
side tank takes four, and the raid follows (burrower melee 28 deaths a pull). On the test pools the first side tank
death came on a phase 1's second wave (waves 2, 4, 6, 8) in 11 of 12 pulls. Against CT/CU the burrower lifetimes,
focus timing, raid DPS (about 160k/s in phase 1) and healing on the side tanks match; the test side tanks take
bigger hits at similar health (logged burrower hits: Bulwark median 6.1k, test prot paladin 9.0k, bears 10.5-13k;
60-83k health on both), so they are less durable, not misplayed.

**Compositions on f863bb8b6 (12 valid pulls, all wipes).**

| Design | Pools | Outcome |
|---|---|---|
| A | toc1, toc4 | 1 of 4 reached phase 3 (wipe 8:32 at 29.9%); three collapsed in P1c-P1d at 31.4-57.7% |
| B | toc2, toc5 | 2 of 4 reached phase 3 (27.2%, 15.6%); two collapsed after wave 2, wipes 1:42 and 2:15 |
| C | toc3, toc6 | 4 of 4 reached phase 3, wipes at 10.6-22.5% |

**Open failures, ranked.**

1. Side tank death on a phase 1's second wave, then the cascade: 420 burrower melee deaths. Picked r1.
2. Phase 3: 7 of 12 wipes, side tanks dead 1-17 s in, then burrowers and the swarm. Not yet looked into.
3. A bear side tank (B, C) leaves Dire Bear Form between waves for the non-combat strategy's Rejuvenation or Mark of
   the Wild: 15 drops in 7 of 8 B/C pulls, out 1.4-7.3 s, back about when the next wave comes up, so little damage
   so far. A `caster form` block would also stop a druid tank's `anubarak battle res side tank`.
4. Anub'arak melee 21, Penetrating Cold 18, Mandibles 11, Leeching Swarm 6, Impale 4, Shadow Strike 4, Freezing
   Slash 3: mostly inside collapses.

**Fixes in place.**

- r1: heroic side tank defensives with a 120 s+ cooldown are kept while the tank is at 50%+ from the pull until
  `Swarm` (was from his 50%). Verdict pending.

**Ideas rejected.** None yet.

**User answers in force.** The boss doc's user calls (east tank pair in heroic, DPS split by side, the push, keep
killing phase 3 waves, battle res as is). The 2026-11-04 defensive keep from his 50% is widened by r1.

**Open questions.** None asked. If side tanks still die on second waves with a long defensive spent under 50%, the
next levers are plan changes: a fifth healer at 25 (the roster's spare healer replacing a DPS, as lane C's research
proposed) or more DPS on burrowers in phase 1.

## Round 1 (build f863bb8b6)

**Data.** Window report `check42-anub-arak.md` (job 1), its 15-row pull table. Valid: toc2 `649_2_…1795595889`,
`…1795596327`; toc5 `649_5_…1795595892`, `…1795596073`; toc6 `649_6_…1795595893`, `649_7_…1795596939`; toc1
`649_1_…1795595889`, `649_8_…1795596970`; toc4 `649_4_…1795595891`, `…1795596260`; toc3 `649_3_…1795595891`,
`649_9_…1795597051`. Voided (engaged before the start step): `649_6_…1795596344`, `649_1_…1795596367`,
`649_3_…1795596329`. Kill references CT and CU. Sections read: `--phases`, `--burrowers`, `--tanks`, `--healers`,
`--dps`.

**Deaths.** Burrower melee 420 of 487 in 15 pulls, then Anub'arak melee 21, Penetrating Cold 18. First side tank
death per valid pull:

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

**Failure picked: side tanks die on a phase 1's second wave with every long defensive cooling.**

- `AnubarakSideTankNeedsDefensive` (`ToCHelpers_Anubarak.cpp:5564`) fires at 5 Expose Weakness stacks whatever
  the tank's health, and `NextTankDefensive` (`RaidTankDefensive.cpp:59`, weakest first) hands warriors Last
  Stand then Shield Wall, paladins Divine Protection, bears Barkskin then Frenzied Regeneration and Survival
  Instincts, DKs Vampiric Blood then Icebound Fortitude. The keep (`AnubarakSideTankDefensiveKept`, `:5572`)
  held the 120 s+ ones only from his 50%.
- So each phase 1's first wave took them: 67 of 85 long casts outside phase 3 went at 50%+ health (`--tanks`),
  for example toc4 …5891 Tocuaz Last Stand 0:21.2 at 74% and Shield Wall 0:38.3 at 93%, toc1 …6970 Tocxaz 83% and
  74%, toc5 …5892 Tocvaz Divine Protection 0:21.1 at 86%.
- Every first side tank death came with each of its long defensives cast 21-147 s before (`--tanks` death rows,
  within the 120-300 s cooldowns): Tocuaz 1:09.6 under two burrowers, 100% to 3% in 15 s, Shield Wall 31 s and
  Last Stand 48 s before.
- Healing is not the gap: in phase 1 the side tanks of four test pulls received 219-420k heals, CT's 257k and 420k,
  CU's 133k and 224k.

**Classification: bug.** The defensive is meant to carry the side tank through the waves; spent at full health on
the wave that didn't need it, it is gone for the one that kills.

**Fix.** `AnubarakSideTankDefensiveKept` keeps a 120 s+ pick while the tank is at 50%+ on heroic from the pull to
`Swarm`, not only from his 50% (`ANUBARAK_SIDE_DEFENSIVE_KEEP_PCT` removed). Barkskin and Vampiric Blood still go
at 5 stacks; normal is unchanged. Reader: `--tanks` lists the long defensives a side tank cast outside phase 3 at
50%+ health and, per side tank death, its last cast of each.

**Next window should show.** `--tanks` long defensives at 50%+ outside phase 3: 0 of N on every pull (`anub.defensive
kept` rows instead), and fewer first side tank deaths on waves 2 and 4, each with a long defensive cast under 50%
in that wave. Judge per composition; A's warriors and B's paladin, whose only long buttons these are, should gain most.

**Previous fix.** None (first round).
