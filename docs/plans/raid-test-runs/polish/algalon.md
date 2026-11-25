# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`, `--defensives`), tests in `tools/botobs/tests/test_algalon.py`.
Rounds 1-13: `history/algalon.md`.

## State

**Setup.** Trace slug `algalon` (pulls file under `algalon-the-observer`), raid `uld`, DC scenario `uld-algalon`
(Celestial Planetarium console with a lent key; he is passive until attacked). No hard mode. His health is the
snapshot `hp` of entry 32871. From window 34 the rosters carry all eleven professions at 450 with their perks and
enchants (`.dc test roster professions`): trends across window 33/34 compare unlike rosters.

**Fight model.** A 26 s intro, then phase 1 to 20%; Berserk (47008) 360 s after phase 1 starts, Ascend at 480 s.
Big Bang every 90.5 s (casts at P1 + 90, 180.5, 271, 361.5; 8 s), soaked by a Dispersion priest while the raid hides
in holes. Meteors every 25.5 s from P1 + 30.9: Big Bang 1 and 3 have one 8-10 s before the cast, Big Bang 2 one
inside it. Star waves every 60 s from P1 + 16.5 top up to four alive and drain 1 %/s: 4, then 1, then 3 (wave 2's star
still lives at P1 + 136.5 in every pull), then up to 4. The planner puts most stars under Big Bang (the
bulk takes each into a 4-5 tick band that dies in hit − 4 .. hit + 0.6, while the raid is phased; one finisher takes
each star's last 5 ticks) and paces the rest one at a time, 8 s apart, clear of meteors, casts and their aftermaths.
The handler (the third tank) kites constellations into spare holes; the swap tank not holding him taunts
constellations off healers in the 30 s before each cast. Holes are the raid's shelter.
What kills raids: tank burst (holders take 65-161 k in 6 s with both tank healers alive), then him loose; a star
going off on the unphased raid while it is drained (the void's DoT after a hit, wave 3's leftovers after Big Bang 2);
a Big Bang with no hole in reach; Berserk once phase 2 runs past 360 s. Phase 1 takes 296-348 s of the 360 s, phase 2
another 54-60 s on a kill.

Wave 2's star dies at ~P1 + 150-170 s (an open or deadline kill, or chewed after one was dropped), 18-37 s before
Big Bang 2's hit. Wave 3's three stars belong under Big Bang 2. The plan takes as many as the team's measured speed fits by cast − 5 s,
two only from +2.9 %/s beyond the drain (`BulkWorkMs`, `PlanUnderStars`), and the 3-member team's pull medians run
+1.2 to +2.3 (single spans +0.2 to +4.5): one or two land in the window and the rest go to deadline kills between
Big Bang 2's aftermath and Big Bang 3, on a raid the void left drained.

**Compositions on 008c167b4 (window 38, round 13; one pull per pool).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 wipe 7:00, Berserk (P1 317 s, 26 of 34 after 6:26); uld4 wipe 6:16, holders died 3:49, 4:41, 5:32 (healers alive), loose in P2 |
| B | uld2, uld5 | uld2 wipe 4:54 in P1 (disperser died to a Barrage, wave 3's pair drained 4 s apart, 10 dead); uld5 wipe 4:37 in P1 (two wave-3 leftovers 8 dead, then the tanks) |
| C | uld3, uld6 | uld3 **kill** 6:36, 2 dead (tanks at Berserk) — meets the done rule; uld6 wipe 6:33 (no hole at Big Bang 1, collapse at 5:55) |

A and B met the done rule on ce636f992 (windows 36-37), C on 008c167b4: no build has all three yet.

**C against A and B** (windows 36-37). Tanks: pair prot paladin + blood DK (A: prot paladin + prot warrior, B: prot
warrior + prot paladin), handler the bear (as B). Healers: disc, resto shaman, holy priest, resto druid, no holy
paladin (A, B both have one), so the tank healers are disc + resto druid; A and B's holy paladin puts ~27% of 2.2 M on
the raid, C's resto druid 3-20%. In P1's first 200 s A and B healed all the raid took (10.4-14.7 k/s); C fell 0.9-1.5
k/s behind in its three collapses (healed 11.3-14.2 against 12.6-15.7) and kept pace only in uld6 window 36, the one
that reached P2 on time. So stars caught more of C under the line (15-19 against at most 8), Arcane Barrage killed
only in C (16 of 16), and the team's mage went into Ice Block at the 3:14 explosion (2 of 4 C pulls, 0 in A or B).
Constellations stayed awake longer in C (1028-1375 s against 887-1048). The blood DK was the first pair death in all
three collapses.

**Open failures, ranked (window 38, 163 deaths in 6 pulls).**

1. Wave 3's leftovers after Big Bang 2: 22 direct (uld1 3, uld2 10, uld5 8, uld6 1); two wave-3 stars went off after
   its hit in 5 of 6 pulls, and they started both B wipes. A team-size limit: Q6. Picked r8, r13, r14.
2. Tank burst and loose: on tank 26, loose 65. Berserk (uld1, uld3's two), holders at 95-104 k in 6 s with 30-37 k
   healed (uld4), and every collapse.
3. Arcane Barrage 26, mostly in collapses on bots a star left low; uld2's disperser died to one at 3:05 (`nosoak`, no
   under-star at Big Bang 2).
4. Big Bang 1 with no hole (uld6, 4 dead): see leads.

Leads, not picked:

- Raid holes eaten before Big Bang 1 (uld6): the `urgent` shelter kill left a hole 2.7 yd from the off-tank slot and
  the constellation the swap tank off-taunted (r11) ate it 5 s later; the one before went to a constellation chasing a
  healer beside it.
- Fallback deadline on a far star (uld2 3:56, no disperser): wave-4 1931 at 90% was killed while wave-3 1864/1863 at
  26/30% were only held by `smashrecovery`; they drained 4 s apart into the 4:21 meteor (10 dead).
- Finisher overshoot by shots in flight (uld5 1840): Steady Shot 1.8 + Kill Shot 3.5 + Auto Shot 2.1% in 0.4 s, from
  16.5 to 9.9%, `early`, off in Big Bang 2's cast on 21. Kill Shot hit an under-star 3 times in 14 pulls.
- A paced kill dropped by position keeps its casts in flight (uld2 1807 at 27.5%: 14.7% landed after, off 8.4 s later
  with a meteor live).
- A deadline kill opened 0.12 s past the 16 s aftermath landed 1 s sooner than `FinishBand`'s fast bound (uld5 1838,
  hit + 15.1 s, 12 under, 5 dead).
- Kite blocked 5-36 s in every pull: the handler in `taunt` with one held, a hole spare and its taunt cooling.
- A class Ice Block at critical health runs 10 s (5 in window 38, 4 on the star team); r13 breaks only the node's.
- A finisher that never touches its star: uld3 w37 star 8, the elemental shaman who joined the team after the hunter
  died walked between `star` and `reach spell` spots, dodged a meteor and dropped totems for 15 s; the star went
  `late` and off after Big Bang 2's hit (2 dead).
- Under-star release reads a stale bulk speed: `CommitBulkSpeed` commits only when the focus leaves a star, so a team
  stalled on its last under-star still plans at the earlier span's speed and misses the `late` window (cast − 15 ..
  − 8 s).
- Pair tanks hit unphased at Big Bang (`noshelter`: w38 uld4, uld5, uld6); the second of two explosions is a star the
  team chewed while on another (r11); wave 2's star in the 16-20 s before Big Bang 2 (r9); splash on idle
  under-stars (r10-r12).

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
  Explosive Shot ticks count in `pendingHp` — held.
- r10: Revenge held near stars; bots' pets' area and chain autocasts off near stars (`algalon.petsplash`) — held.
- r11: the swap tank not holding him taunts constellations off healers in the 30 s before each cast
  (`algalon.offtaunt`, user Q4 (b)) — held.
- r12: one finisher takes an under-star's last 5 ticks to its line while the team moves on (`algalon.finishstar`,
  `algalon.finisher`) — held.
- r13: the tick breaks a personal-node Ice Block once its star is dead and none explodes on the bot within 4.5 s
  (`algalon.unblock`); class Deterrence held for unphased bots in P1 he is not hitting — held (w38: no Deterrence,
  every node block broken in 0.6-6.1 s).

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

**Open questions:**

- Q6 (r8, asked again r14 with its trigger met): more ranged on the 25-man star team from wave 3's spawn to Big Bang
  2's cast, and how many. Round 14 has the case; recommendation two (five members).

## Round 14 (2026-11-25, window 38 build 008c167b4: round 13)

**Data.** `window38-algalon.md` (six pools); every pull's `--verdict`, `--deaths`, `--stars`, `--defensives`;
`--constellations`, `--holes` (uld6), `--tankheal` (uld4); `--stars` on windows 36-37's 8 traces for the leftovers.
Scratch `alg14/`: `kitepeek.py` (handler, holes and constellations each second), `starhits.py` (one star's hits,
casts and health), `killshot.py` (hits of 3%+ on under-stars), `dropped.py` (paced kills dropped with the star alive).

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 A | `603_1_…_1795559380` | wipe 7:00 | 34: loose 15, on tank 7, star 3, other 3, Barrage 2, 4 others (26 after Berserk) |
| uld4 A | `603_4_…_1795559380` | wipe 6:16 | 33: loose 21, on tank 5, Barrage 5, Smash, other |
| uld2 B | `603_2_…_1795559380` | wipe 4:54 | 31 in P1: star 11, Barrage 8, on tank 5, loose 5, Smash 2 |
| uld5 B | `603_5_…_1795559381` | wipe 4:37 | 31 in P1: loose 11, star 9, Barrage 6, on tank 4, Smash |
| uld3 C | `603_3_…_1795559380` | kill 6:36 | 2: on tank 2 (Berserk) |
| uld6 C | `603_6_…_1795559382` | wipe 6:33 | 32: loose 13, star 11, Barrage 5, on tank 3 |

**Stop rules.** Not polished: C killed (uld3, 2 dead), A and B did not on this build. Not stuck: wave 3's leftovers
were picked in r8 (team spot) and r13 (team stalls); this third pick is a plan question, so a fourth pick with Q6
unanswered and the failure still on top is `stuck`.

**Verdict on round 13: held** in A, B and C. No Deterrence pressed unphased in P1 (w36-37: 39). Node Ice Blocks broken
by `algalon.unblock` after 1.2 s (uld1), 6.1 (uld2), 2.1 (uld4), 0.6 (uld6); uld5's cut by the hide run. The five that
ran 10 s were all the class `ice block` node at critical health (uld3 3:42 right after an explosion, uld4 4:20,
uld2/uld6 in collapses, uld5 off the team). C's wave-3 under-star above its band at Big Bang 2: 1 of 2 C pulls (uld3
1873 at 14.2%, off at hit + 6.1 s, 11 under, 0 dead; w36-37 3 of 4).

**Picked: wave 3's leftovers after Big Bang 2** (22 dead, the start of both B wipes). Plan change, not a bug.

- Wave 3 spawned three stars in all six pulls (wave 2's star alive at P1 + 136.5 in each), so its four-star row in
  `docs/raids/ulduar/algalon.md` was wrong (fixed).
- Under-stars toward Big Bang 2, planned at the spawn / in all: uld1 2/2 (1917 `late` at 71%), uld2 1/1 (`nosoak` at
  3:07: the shadow priest died to a Barrage at 3:05), uld3 1/2 (1873 above its band), uld4 0/1 (`early`: Arcane
  Missiles 5.1% + Explosive Shot), uld5 1/1 (`early`: the finisher's in-flight volley), uld6 1/3 (1907 `late` at
  50%). Landed in the window: 4 of 18.
- Bulk beyond the drain (median per pull): +2.3, +1.8, +1.4, +1.2, +2.0, +1.8, planned +5. `BulkWorkMs` puts each
  wave-3 star at ~48% to its line plus a 3 s switch, against cast − 5 s = 39 s left at the spawn: two need +2.9.
  uld6 added its second and third mid-cycle after spans of +4.5 and +4.0; uld1's two came off wave 1's bulk.
- Wave-3 stars off after Big Bang 2's hit (spawn read off "on the drain alone"): uld1 4:05.0 (0 dead), 4:14.7 (10
  under, 3); uld2 4:19.7 (1), 4:23.7 (18 under, 9); uld3 3:40.6, 4:06.3 (9 under, 0); uld4 4:04.9, 4:15.9 (0); uld5
  3:49.75 (12 under, 5), 4:07.6 (9 under, 3); uld6 4:05.6 (1). The kills (uld3 here, uld5 w36) had two as well and
  lost nobody: deaths follow the raid's health when they go off.
- In most pulls three members can't take two wave-3 stars ~48% each plus wave 2's deadline kill in 39 s. No speed
  bug found: pull medians match r8-r9's +1.6 to +3.0 (r9: walking ~30%, casting at the star ~40% of focus time).

**Not picked** (one pull each, leads in State): uld6's raid holes eaten before Big Bang 1 (4 dead); uld2's fallback
deadline on a 90% star; uld5's finisher overshoot and the 1838 kill at hit + 15.1 s; uld2's dropped paced kill.
Tank burst: Q2 stands (uld4: 95-104 k in 6 s, 30-37 k healed, the tank healers 19 yd off).

**Questions for the user.**

- Q6: on 25-man, from wave 3's spawn (P1 + 136.5 s) to Big Bang 2's cast (P1 + 180.5 s), add ranged to the star team,
  and how many? Wave 3's three stars should die under Big Bang 2. The team's speed decides how many the plan takes:
  two need +2.9 %/s beyond the drain and three members ran +1.2 to +2.3, so in 5 of 6 pulls two wave-3 stars were
  killed or drained between Big Bang 2's aftermath and Big Bang 3 on a drained raid (22 dead, both B wipes).
  One more member gives ~+1.6 to +3.1, two give ~+2.0 to +3.8. Cost: those damage dealers off him for ~44 s once per
  pull, about 3 s (one) or 6 s (two) of phase 1 against the enrage, partly repaid since the leftovers' deadline kills
  also take the team. The plan must then scale the speed it measured with three. Recommendation: two (five members),
  25-man only; 10-man unchanged.

**Next window should show.** No change this round: on this build, two wave-3 stars after Big Bang 2's hit in most
pulls again. Once Q6 is built: two wave-3 under-stars in Big Bang 2's window in most pulls, at most one wave-3 star
after its hit, explosion deaths between Big Bang 2 and 3 under window 38's 22, and P1 against 310-348 s.
