# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`, `--defensives`), tests in `tools/botobs/tests/test_algalon.py`.
Rounds 1-14: `history/algalon.md`.

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
Big Bang 2's aftermath and Big Bang 3, on a raid the void left drained. From r15 (Q6) a 5-member team works
wave 3 on 25-man, its speed scaled from three: +2.0 to +3.8 expected.

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
   its hit in 5 of 6 pulls, and they started both B wipes. A team-size limit: Q6, built r15. Picked r8, r13, r14,
   r15.
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
- r15: on 25-man two more ranged join the star team from wave 3's spawn to Big Bang 2's cast (user Q6); the bulk's
  and paced kills' measured speeds scale, beyond the drain, by the team's size now over then — pending.

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

## Round 15 (2026-11-25, no new pulls: window 38 build 008c167b4 still the newest)

**Data.** `window38-algalon.md` again; `--stars` focus spans on all six traces for the wave-3 spawn and Big Bang
1's last bulk span. No scratch scripts: the reader's focus spans now carry the team's size (below).

**Stop rules.** Not polished: on 008c167b4 only C killed (uld3, 2 dead). Not stuck: round 14 wrote the fourth pick
of wave 3's leftovers as a question, and the user answered it (Q6), so this round builds the answer instead of
picking the failure again.

**Verdict on round 14:** no change to judge (question only).

**Picked: Q6, wave 3's leftovers after Big Bang 2** (22 dead in window 38, both B wipes). Built as answered:

- `StarTeamReinforced` (`UldEncounter_Algalon.cpp:1159`): 25-man, the cycle toward Big Bang 2
  (`bigBangCount + 1 == …_STAR_TEAM_EXTRA_BIG_BANG`), outside the cast, and a living star first seen after Big Bang
  1's end. Wave 2 spawns at P1 + 76.5 s, before Big Bang 1's cast (P1 + 90), so the first star seen after its end
  (~P1 + 98) is wave 3's (P1 + 136.5; uld1, uld2, uld3 and uld6 opened their first wave-3 bulk at 2:42.6-2:42.9,
  P1 starting 0:26.1-0:26.3). It closes
  at Big Bang 2's cast, or earlier if every wave-3 star is dead.
- `ElectStarTeam` (`:1169`): fills the core (2/3) as before, then up to `…_STAR_TEAM_EXTRA_25` = 2 ranged only,
  kept in `starTeamExtras`; out of the window the extras leave and the core stays. 10-man unchanged.
- `TeamSpeed` (`:1229`): a measured speed's excess over the 1 %/s drain times team size now over team size when
  measured. The bulk's size is taken at its span start (`bulkFromTeam`, `:3355`; committed `:2965`), since the span
  ending at Big Bang 2's cast is committed after the team shrank; the paced kill speed's before `ElectStarTeam`
  (`:3305`), the team that worked the previous focus. Used by `BulkWorkMs` (`:2613`) and `FinishBand`'s fast
  bound (`:1507`). Outside the window the size is unchanged, so the scale is 1.
- Effect at the spawn: two wave-3 stars (~48% to the line each, plus a 3 s switch) fit cast − 5 s = 39 s from
  +2.91 beyond the drain; scaled 5/3 that needs +1.75 measured with three. Big Bang 1's last bulk span: uld3 +1.6
  (one at the spawn, the second once the 5-member speed commits at the first star's hand-off), uld6 +1.8 (two).
- Reader: `focus_rates` rows carry `team` (the `algalon.starteam` size at the span's start), `--stars` prints it
  per span and the bulk median per team size; test `test_each_focus_span_reads_the_star_team_size_at_its_start`.
- `docs/raids/ulduar/algalon.md`: wave-3 paragraph, the plan bullet and the star team paragraph.

Shared code: none.

**Watch.** The hand-off to the finisher stays at 5 ticks above the line; four members leaving a star instead of two
put roughly twice the in-flight damage on it (three: median 1.3-2.2%, p75 4-4.5%), so `early` releases of
wave-3 under-stars may rise. Extras also step off him for ~44 s once a pull: P1 length against window 38's
310-348 s.

**Next window should show.** `--stars`: focus spans at team 5 from ~P1 + 136.5 to Big Bang 2's cast, and their
bulk median against team 3's (expected +2.0 to +3.8 against +1.2 to +2.3); two wave-3 under-stars planned in most
pulls and in Big Bang 2's window, at most one wave-3 star off after its hit; explosion deaths between Big Bang 2
and 3 under window 38's 22 (uld1 3, uld2 10, uld5 8, uld6 1); wave-3 `early` releases not above window 38's two;
P1 not past 348 s. If `early` releases rise, the finish hand-off should scale with the team (a bug fix, not a
question).
