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
