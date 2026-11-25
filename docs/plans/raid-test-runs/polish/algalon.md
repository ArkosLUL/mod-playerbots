# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`, `--defensives`), tests in `tools/botobs/tests/test_algalon.py`.
Rounds 1-15: `history/algalon.md`.

## State

**Setup.** Trace slug `algalon` (pulls file under `algalon-the-observer`), raid `uld`, DC scenario `uld-algalon`
(Celestial Planetarium console with a lent key; he is passive until attacked). No hard mode. His health is the
snapshot `hp` of entry 32871. From window 34 the rosters carry all eleven professions at 450 with their perks and
enchants (`.dc test roster professions`): trends across window 33/34 compare unlike rosters.

**Fight model.** A 26 s intro, then phase 1 to 20%; Berserk (47008) 360 s after phase 1 starts, Ascend at 480 s.
Big Bang every 90.5 s (casts at P1 + 90, 180.5, 271, 361.5; 8 s), soaked by a Dispersion priest while the raid hides
in holes. Meteors every 25.5 s from P1 + 30.9: Big Bang 1 and 3 have one 8-10 s before the cast, Big Bang 2 one
inside it. Star waves every 60 s from P1 + 16.5 top up to four alive and drain 1 %/s: 4, then 1, then 3 (4 when
wave 2's star died first), then up to 4. The planner puts most stars under Big Bang (the bulk takes each into a 4-5
tick band that dies in hit − 4 .. hit + 0.6, while the raid is phased; one finisher takes each star's last ticks)
and paces the rest one at a time, 8 s apart, clear of meteors, casts and their aftermaths. The handler (the third
tank) kites constellations into spare holes; the swap tank not holding him taunts constellations off healers in the
30 s before each cast. Holes are the raid's shelter.
What kills raids: tank burst (holders take 73-110 k in 6 s with both tank healers alive), then him loose; a star
going off on the unphased raid while it is drained (an under-star released `early`, a deadline kill after Big Bang
2); an under-star dying in the cast before runners reach a hole; Berserk once phase 2 runs past 360 s. Phase 1
takes 294-348 s of the 360 s, phase 2 another 54-60 s on a kill.

Wave 3's stars belong under Big Bang 2. From r15 (Q6) a 5-member team works them on 25-man (P1 + 136.5 to the
cast): w46 bulk +2.7 to +4.5 %/s beyond the drain against the 3-member +1.6 to +2.3, and 12 of 19 wave-3 stars
died under Big Bang 2 (w38: 4 of 18). A 5-member team's leavers put 7-11% on a star after a hand-off (r16).

**Compositions on 37cc72a94 (window 46, round 15; one pull per pool).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 **kill** 6:16, 2 dead — meets the done rule; uld4 wipe 6:52: two wave-3 stars `early`, one off in Big Bang 2's cast (4 dead + 1 meteor), holders dead 5:08/5:12, Berserk in P2 |
| B | uld2, uld5 | uld2 wipe 6:36: holder dead 3:08 (102 k in 6 s), P1 never ended (Berserk at 370 s); uld5 wipe 4:49 in P1: holder dead 3:09.9 (110 k in 6 s), the pair gone by 4:22 |
| C | uld3, uld6 | uld3 wipe 5:09 in P1: two wave-4 stars `early`, deadline kill 4:30 on a drained raid (4 dead, 3 healers), the pair 4:36-4:46; uld6 wipe 7:02: wave-3 star `early` by splash, an under-star in Big Bang 2's cast (3 runners), Big Bang 3 killed both pair tanks (`noshelter`), P2 with one tank |

A met the done rule on 37cc72a94; A and B on ce636f992 (windows 36-37), C on 008c167b4: no build has all three
yet.

**C against A and B** (windows 36-37). Tanks: pair prot paladin + blood DK (A: prot paladin + prot warrior, B: prot
warrior + prot paladin), handler the bear (as B). Healers: disc, resto shaman, holy priest, resto druid, no holy
paladin (A, B both have one), so the tank healers are disc + resto druid; A and B's holy paladin puts ~27% of 2.2 M on
the raid, C's resto druid 3-20%. C fell 0.9-1.5 k/s behind the raid's damage in P1's first 200 s in its collapses,
so stars caught more of C under the line and Arcane Barrage killed only in C (w36-37). The blood DK was the first
pair death in all three of those collapses.

**Open failures, ranked (window 46, 167 deaths in 6 pulls: loose 85, star 36, on tank 22, Barrage 16, Big Bang 5).**

1. Tank burst and loose: on tank 22, loose 85. Both B wipes began with a holder dead at P1 + 162-164 s with both
   tank healers alive 19-21 yd off (uld2 102 k in 6 s, 25 k healed; uld5 110 k, 60 k healed, Lay on Hands 8 s
   before); elsewhere holders die after healers do, or to Berserk. Q2 stands.
2. Under-stars released `early`: 6 in 4 pulls (5 from the finisher hand-off), 9 dead and uld3's wipe. Picked r16.
3. Under-stars going off in the cast on bots still outside a field: ~10 dead (uld3 BB1 1, uld4 BB3 2, uld5 BB2 2,
   uld6 BB2 3 star-team runners 24-28 yd from a hole, uld6 BB3 the pair `noshelter`).
4. Arcane Barrage 16, mostly on bots a star left low.

Leads, not picked:

- An under-star dying at hit − 4..−3 s catches runners whose hole is 23-28 yd off (uld6 BB2 Uldwah, Uldwal,
  Uldwav, 3 dead, 6.8 k after an explosion 5 s before); the `nearhole` spot is 22 yd and the star team holds by
  its star.
- Pair hit unphased at Big Bang (`noshelter`): uld6 BB3 both died (2 holes at the cast, one lost in it, median 40
  yd); w46 uld2 BB3, uld4 BB2 one each; w38 uld4, uld5, uld6.
- Splash on a finished under-star: an off-team mage's Living Bomb 2.0% plus Ignite and Wandering Plague over 15 s
  (uld6 1887, `early` at 11%); Mirror Image frostbolts on a star its mage left (uld3 1919).
- A wave-3 star not planned at the spawn (uld2 1872 when wave 3 brought four, uld5 1863): deadline kills after
  Big Bang 2, 1 dead each.
- Raid holes eaten before Big Bang 1 (w38 uld6): an `urgent` shelter kill's hole 2.7 yd from the off-tank slot,
  eaten 5 s later by the constellation the swap tank off-taunted.
- Kite blocked 15-30 s (w46 uld2, uld5): the handler in `taunt` with one held, a hole spare and its taunt cooling.
- A class Ice Block at critical health runs 10 s (w46 uld3 the team mage after an explosion, 255 s); r13 breaks
  only the node's.
- Under-star release reads a stale bulk speed: `CommitBulkSpeed` commits only when the focus leaves a star.
- Finisher overshoot by its own shots in flight (w38 uld5 1840: Steady + Kill + Auto Shot 6.6% in 0.4 s).

**Fixes in place** (round: fix — verdict):

- r1: a dropped star keeps its slope 4 s; a kill colliding with another death drops — held.
- r2: no due kill lands 16 s after a hit while anyone is under the line (`recovery`) — held.
- r3: a kill carried across the hit drops while the raid is drained — held, rarely exercised.
- r4: constellations up to 7.5 yd count as low; a floor-moved kite spot refuses its hole — held.
- r5: two holes kept out of the kite from 60 s before each Big Bang (user Q4 (a)) — held.
- r6: the meteor wait skipped when a star dies in the cast; a released star draining into the cast stretches the
  hide — held.
- r7: no kill lands 5-10 s after an impact while anyone is under the line (`smashrecovery`) — held. Tank druids stay
  in bear form — held.
- r8: the ranged team holds 12-28 yd from its star (`starhold`) — held.
- r9: casts on an under-star the team left are cut at once; the target guard judges the spell's own target;
  Explosive Shot ticks count in `pendingHp` — held (cuts nothing already flying: r16).
- r10: Revenge held near stars; bots' pets' area and chain autocasts off near stars (`algalon.petsplash`) — held.
- r11: the swap tank not holding him taunts constellations off healers in the 30 s before each cast
  (`algalon.offtaunt`, user Q4 (b)) — held.
- r12: one finisher takes an under-star's last ticks to its line while the team moves on (`algalon.finishstar`,
  `algalon.finisher`) — held; its 5-tick hand-off too small (r16).
- r13: the tick breaks a personal-node Ice Block once its star is dead and none explodes on the bot within 4.5 s
  (`algalon.unblock`); class Deterrence held for unphased bots in P1 he is not hitting — held.
- r15: on 25-man two more ranged join the star team from wave 3's spawn to Big Bang 2's cast (user Q6), measured
  speeds scaled by team size — held (wave-3 deaths after Big Bang 2's hit 22 → 3).
- r16: the finisher hand-off at 2.5 ticks over the line per team member (`FinishHandOverHp`) — pending.

**Tried and rejected:**

- A "finish stars under 15%" bypass of the 8 s gap: two explosions 3.2 s apart, 12 dead.
- Wands on under-stars: slow, a damage dealer off him; a far star went off after the strip.
- DoTs on stars: ticked on past the team, 6 of 7 under-stars `early`.
- Opening the second wave-3 leftover inside the gap: lands 8-11 s after the first, worse (r8).
- Lengthening the meteor blackout: moves every due and serial deadline (r3, r9).
- Hiding before the cast for a star dying in its first seconds: the tanks hold him until the cast, and the
  `nearhole` spots are 22-30 yd out (r10).

**User answers in force:**

- 2026-11-24 (replaces 2026-11-22's "a kill isn't expected, stop rules waived" after window 34's first kills):
  Algalon runs under the normal done rule, each of A, B, C a kill with at most 5 deaths on one build (no hard
  mode to check), and the stop rules (polished, stuck) apply again.
- Q2 (r1): no 5th healer at 25.
- Q4 (a) (r4): two holes out of the kite from 60 s before each Big Bang.
- Q4 (b) (2026-11-24): in the 30 s before each Big Bang the swap tank not holding him taunts constellations off
  healers. Built r11.
- Q5 (2026-11-24): tank druids in bear form in every boss fight: keep as is for now. Algalon's own hold stays, no
  shared change.
- Q6 (2026-11-25, yes): on 25-man only, two more ranged join the star team (five members) from wave 3's spawn to Big
  Bang 2's cast. Built r15.

**Open questions:** none.

## Round 16 (2026-11-25, window 46 build 37cc72a94: round 15)

**Data.** `window46-algalon.md`; every pull's `--verdict`, `--deaths`, `--stars`; `--defensives`, `--tankheal`,
`--tanks`, `--bigbang` on uld2, uld4, uld5, uld6; `--collateral` on uld3, uld4, uld6. Scratch `alg16/peek.py` (one
star's samples, hits and casts), folded into the reader as the early-release breakdown below.

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths (reader causes) |
|---|---|---|---|
| uld1 A | `603_1_…_1795602671` | kill 6:16 | 2: star 1, Barrage 1 |
| uld4 A | `603_4_…_1795602673` | wipe 6:52 | 34: loose 20, on tank 6, star 6, Smash, Big Bang |
| uld2 B | `603_2_…_1795602672` | wipe 6:36 | 32: loose 15, star 10 (9 after Berserk), on tank 5, Barrage 2 |
| uld5 B | `603_5_…_1795602674` | wipe 4:49 | 31: loose 15, on tank 6, Barrage 5, star 4, Big Bang |
| uld3 C | `603_3_…_1795602672` | wipe 5:09 | 33: loose 14, star 8, Barrage 6, on tank 4, Smash |
| uld6 C | `603_6_…_1795602675` | wipe 7:02 | 35: loose 21, star 7, Big Bang 3, Barrage 2, on tank, other |

**Stop rules.** Not polished: only A killed (uld1, 2 dead). Not stuck: the under-star hand-off was picked once
before (r12) and is the first pick of its margin.

**Verdict on round 15: held** for wave 3's leftovers after Big Bang 2. Team 5 bulk medians +3.4 (uld1), +4.2,
+2.7, +4.0, +2.8, +4.5 against team 3's +1.6 to +2.3. Wave-3 stars under Big Bang 2: uld1 2/3 (1879 `late` at 56%),
uld2 3/4, uld3 3/3, uld4 0/3, uld5 2/3, uld6 2/3. Off after its hit: 4 stars, 3 dead (w38: 22). A: uld1 held, uld4's
three went before Big Bang 2 instead (below); B held in both; C held in uld3, uld6's 1887 went `early` by splash.
P1 294-325 s where it ended (w38 310-348).

**Picked: under-stars released `early` after the finisher hand-off.** Bug. `HandOverFinish`
(`UldEncounter_Algalon.cpp:2873`) hands a bulk star over at 5 ticks over its line whatever the team's size;
`StopCastsAtLine` then cuts the leavers' casts, but no shot already flying, and a volley can cross the 5 ticks in
one 250 ms tick. `--stars` per early release (from its last sample over its band to the release):

- uld4 1844 (team 5): 46.2% at 2:53.937 (5.5 ticks over), Mind Blast + Explosive Shot to 41.9% by the next tick,
  hand-off 2:54.266; then Lightning Bolt 4.8% and Shadow Bolt 2.6% flying, 33.5% at 2:54.742: `early`, deadline
  kill 3:13.9 (8 under, 0 dead).
- uld4 1843 (team 5): hand-off 3:08.037 at 33.8%; Lightning Bolt 4.9% flying and the hunter's shots after its
  latch cleared, 7.5%: 19.7%, 0.4% under the release line; chewed into Big Bang 2's cast at 3:29.5 with a meteor
  live (11 hit, 4 dead, 1 more to the meteor).
- uld2 1925 (team 3): hand-off 4:22.115 at 47.5%; Arcane Missiles 7.6% still flying: `early` 4:26.4 at 33%.
- uld3 1918 (team 3): 5.0 ticks over its line, a hair over the hand-off, from 3:54.755 to 3:55.966 with three
  casts under way; Arcane Blast 6.3% and Chimera Shot 2.9% landed 30 ms apart, Lightning Bolt 4.3% 0.4 s later:
  73.5 → 60%. 1919: Arcane Blast 6.3% crossed its line before the hand-off, then Aimed Shot 2.6% and Mirror
  Image. Both `early` 60.4 s before the cast; deadline kills 4:13.9 (0 dead) and 4:30.3 (10 under, 4 dead with 3
  healers), the pair dead by 4:46.
- uld6 1887 (team 5): in its band at 3:03 after the finish; Living Bomb 2.0% from an off-team mage, Ignite,
  Wandering Plague: splash, a lead.

**Fix** (Algalon files only):

- `FinishHandOverHp` (`:2866`): the hand-off margin is 2.5 ticks per team member (`…_UNDER_FINISH_TICKS` × size /
  `…_STAR_TEAM_10`): 5 for two (10-man unchanged), 7.5 for three, 12.5 for five. Replay by hand: 1844 hands over
  at 53.9% (2:52.927), before the Arcane Blast, Mind Blast, Shadow Bolt and Lightning Bolt that took it under; 1918
  at 3:54.755, cutting Arcane Blast and Lightning Bolt; 1919 at 82.2%, before its Arcane Blast; 1925 2.5% higher,
  so its Arcane Missiles end near the line.
  Cost: the finisher alone does 2.5% (team 3) or 7.5% (team 5) more, ~1-4 s beside the team's next star.
- Reader: `--stars` prints under each `early` release who hit it from its last sample over its band to the
  release (`team`, `left`, `finisher`, `off`), the team's size and the hand-off; finish rows carry the team's size.
  Tests: `UnderOvershoot`, `UnderFinish` updated.
- `docs/raids/ulduar/algalon.md`: the Bulk bullet.

Bots only (the team never takes a human), so a player changes nothing; 10-man keeps 5 ticks.

Shared code: none.

**Not picked:** under-stars in the cast catching runners (~10 dead, leads); B's holders at P1 + 162-164 s (Q2).

**Next window should show.** `--stars`: `early` releases under w46's 6, none with `left` over 2.5% per team member;
finish rows at team 5 and 3 with the rest of the team under 5%; wave-3 under Big Bang 2 at least w46's 12 of 19;
under-stars `above` their band at the cast not over w46's 0 (the longer finish is the risk), no rise in `late`.
