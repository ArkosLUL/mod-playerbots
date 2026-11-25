# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--melee`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`, `--defensives`), tests in `tools/botobs/tests/test_algalon.py`.
Rounds 1-18: `history/algalon.md`.

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
30 s before each cast. Holes are the raid's shelter. Three constellations wake at P1 + 60 (1:26) and dive on
healers just as `urgent` (no hole, cast ≤ 30 s) kills the shelter star (~1:31): until Big Bang 1 that is the
raid's only hole.
What kills raids: Berserk once phase 1 runs past ~305 s (phase 2 takes 52-66 s on a kill; P1 + P2 must fit the
360 s); tank burst (holders take 73-110 k in 6 s with both tank healers alive), then him loose; a star going off on
the unphased raid while it is drained (an under-star released `early` or `late`, or above its band at the cast; a
deadline kill after Big Bang 2; `urgent` kills 4 s apart once a constellation closes the only hole before Big
Bang 1); an under-star dying in the cast before runners reach a hole; no soaker, so no under-stars for that cast.

Wave 3's stars belong under Big Bang 2. From r15 (Q6) a 5-member team works them on 25-man (P1 + 136.5 to the
cast). Wave 2's single star is never under Big Bang 1 in practice; it ends as a deadline kill at P1 + 162-169 s that
takes the 5-member team off wave 3 for 3-4 s, after which wave-3 stars go `late` (w51 uld2 3 of 3, w50 3, w48 5).

**Compositions on c9b718c29 (window 51, round 18; one pull per pool).** A meets the done rule on this build.

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld4 kill 6:09, 4 dead (P1 288 s); uld1 wipe 5:51 in P1: its soaker died to Barrage at 3:24, so no under-stars for Big Bang 3 and wave 4's two stars drained 16 s after the hit, 1 s apart (8 dead), then loose |
| B | uld2, uld5 | uld2 wipe 7:11: P1 320 s with 7 dead, Berserk in P2; uld5 wipe 5:45: the only hole and `urgent`'s replacement closed by constellations, three explosions in 16 s (4 dead, 2 healers), Big Bang 2 hit both pair tanks unphased, tanks dead 4:30-4:49, loose 42 s |
| C | uld3, uld6 | uld3 kill 6:42, 10 dead (P1 310 s with 6; Berserk killed 3 tanks); uld6 wipe 4:10: uld5's cascade (8 dead at 1:47-1:48, 2 healers), tanks dead 2:37-3:49, loose |

A met the done rule on 37cc72a94 (windows 46-47) and c9b718c29 (window 51); A and B on ce636f992 (windows 36-37),
C on 008c167b4: no build has all three yet.

**C against A and B** (windows 36-37). Tanks: pair prot paladin + blood DK (A: prot paladin + prot warrior, B: prot
warrior + prot paladin), handler the bear (as B). Healers: disc, resto shaman, holy priest, resto druid, no holy
paladin (A, B both have one), so the tank healers are disc + resto druid. C fell 0.9-1.5 k/s behind the raid's
damage in P1's first 200 s in its collapses, so stars caught more of C under the line and Barrage killed more in C.

**Open failures, ranked (window 51, 140 deaths in 6 pulls: loose 53, star 34, on tank 24, Barrage 16, Big Bang 4,
Dark Matter 4, Smash 3, Black Hole DoT 2).**

1. Constellations closing the raid's only hole before Big Bang 1, then `urgent` kills 4 s apart (w51 uld5, uld6
   wipes; uld1; w50 uld1, uld5; w46 uld6): picked r19.
2. Tank burst and him loose after it: Q2 stands.
3. A soaker lost before a cast (uld1: Barrage at 3:24), so no under-stars and the wave drains after the hit.
4. Wave 3 `late` after wave 2's deadline kill (fight model).
5. Barrage 16.
6. Berserk: uld2 P1 320 s with 7 dead, uld3 310 s.

Leads, not picked:

- Dives and area threat: a constellation diving on a healer past the only hole (w51 uld6 1:39.5, 7.6 yd up), or one
  on a healer 8 yd off it pulled across by the swap tank's Consecration (w51 uld5 1:38.5); r19 doesn't cover them.
- A channel's ticks count in `pendingHp`: a team warlock's Drain Soul under 25% zeroes the star's work left (w50
  uld5, uld3 under r17's cap; w51 none).
- Finisher overshoot by its own shots: Explosive Shot ticks after the finish, Chimera, Kill Shot (w48 5 stars).
- An under-star dying at hit − 4..−3 s catches runners whose hole is far (w46 uld6 3 dead; w51 uld4 Big Bang 2,
  5 runners outside a field, 3 dead).
- Pair hit unphased at Big Bang (`noshelter`): w51 uld5 Big Bang 2 both (44-48 yd from the hole), w46 uld6, uld2,
  uld4.
- Phase 2 Unleashed Dark Matter loose 9-34 s each (w51 uld2 4 dead, w48 uld1, uld4).
- Splash on a finished under-star (Living Bomb, Ignite, Wandering Plague; Mirror Image) (w46 uld6, uld3).
- Kite blocked 15-30 s: the handler in `taunt` with one held, a hole spare, its taunt cooling (w51 uld3 30 s).
- Under-star release reads a stale bulk speed: `CommitBulkSpeed` commits only when the focus leaves a star.

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
  Explosive Shot ticks count in `pendingHp` — held (cuts nothing already flying).
- r10: Revenge held near stars; bots' pets' area and chain autocasts off near stars (`algalon.petsplash`) — held.
- r11: the swap tank not holding him taunts constellations off healers in the 30 s before each cast
  (`algalon.offtaunt`, user Q4 (b)) — held.
- r12: one finisher takes an under-star's last ticks to its line while the team moves on (`algalon.finishstar`,
  `algalon.finisher`) — held.
- r13: the tick breaks a personal-node Ice Block once its star is dead and none explodes on the bot within 4.5 s
  (`algalon.unblock`); class Deterrence held for unphased bots in P1 he is not hitting — held.
- r15: on 25-man two more ranged join the star team from wave 3's spawn to Big Bang 2's cast (user Q6), measured
  speeds scaled by team size — held.
- r16: the finisher hand-off at 2.5 ticks over the line per team member (`FinishHandOverHp`), restored r18 — held
  (w51: Big Bang 2's under-stars in band in uld3-5, one `team, no hand-off` `early`, `early` 4 over all cycles).
- r17: a held melee bot out of his reach takes the melee spot (`MeleeHoleSpot`, `algalon.spot` `melee`) — held
  (P1 blocked 0-24 s a pull in w50-51).
- r19: with Big Bang due or `urgent` running, the handler taunts only along lines clear of the raid's holes and
  `urgent`'s star (`side`/`blocked`), and holding stays 12 yd off them (`clear`) — pending.

**Tried and rejected:**

- A "finish stars under 15%" bypass of the 8 s gap: two explosions 3.2 s apart, 12 dead.
- Wands on under-stars: slow, a damage dealer off him; a far star went off after the strip.
- DoTs on stars: ticked on past the team, 6 of 7 under-stars `early`.
- Opening the second wave-3 leftover inside the gap: lands 8-11 s after the first, worse (r8).
- Lengthening the meteor blackout: moves every due and serial deadline (r3, r9).
- Hiding before the cast for a star dying in its first seconds: the tanks hold him until the cast, and the
  `nearhole` spots are 22-30 yd out (r10).
- Capping the hand-off by what the finisher alone does by 3 s before the cast (r17): the team kept Big Bang 2's
  last stars to the cast, two team volleys `early` and one stall `above`, 2 wipes (w50); backed out r18.

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

## Round 19 (2026-11-25, window 51 build c9b718c29: round 18)

**Data.** `window51-algalon.md`, `window50-algalon.md`; every w51 pull's `--verdict`, `--deaths`, `--stars`,
`--melee`, `--smash`; `--holes` on the 30 traces of windows 46-51; `--constellations` on uld1, uld5, uld6. Scratch
`alg19/` (the geometry of each eat, the shelter re-pick, the taunt line), folded into `--holes`.

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths (reader causes) |
|---|---|---|---|
| uld1 A | `603_21_…_1795618922` | wipe 5:51 in P1 | 32: star 9, Barrage 8, loose 7, on tank 6, Smash, BH DoT |
| uld4 A | `603_23_…_1795618973` | kill 6:09 | 4: star 3 (Big Bang 2's runners), Big Bang |
| uld2 B | `603_19_…_1795618880` | wipe 7:11, P1 320 s | 31: loose 15, star 5, on tank 4, Dark Matter 4, Barrage, BH DoT, Big Bang |
| uld5 B | `603_24_…_1795619014` | wipe 5:45 in P1 | 31: loose 20, star 4, Barrage 3, on tank 3, Big Bang |
| uld3 C | `603_20_…_1795618899` | kill 6:42 | 10: on tank 5, star 2, Barrage 2, Big Bang |
| uld6 C | `603_22_…_1795618942` | wipe 4:10 in P1 | 32: star 11, loose 11, on tank 6, Smash 2, Barrage 2 |

**Stop rules.** Not polished: B has no kill, C's has 10 dead. Not stuck: the hand-off (r16-r18) is left alone and
this pick is new. Constellations closing the raid's holes were picked by other routes in r4 (built r5, Q4 (a)) and
r11 (Q4 (b)); a fourth round on that symptom calls stuck.

**Verdict on round 18 (the hand-off backout): held.** Against its Next window list:

- Big Bang 2's under-stars in band with a hand-off in uld3, uld4, uld5; one `team 5, no hand-off` `early` (uld1
  1880, an Arcane Blast 5.6% 8.6 s before the cast; w50 2). uld2's three went `late` (31/68/20%, 11-14 s before the
  cast) after wave 2's deadline kill took the five off them (2:47-2:53): the fight model's lead, not the hand-off.
- `early` over all cycles 4 (w50 9, w48 5), `above` at the cast 1 (uld1 1881, 9.8% over 9.6), bulk breaks 3 and
  none from a channel (w50 5).
- `--melee` P1 blocked 0-24 s a pull. No Smash deaths at 3:30 (w50 8); uld4 lost 3 there to Big Bang 2's
  under-stars going off in the cast on 5 runners outside a field (lead).
- uld4 reached P2 with 4 P1 deaths at P1 288 s and killed 17 s inside Berserk.

**Picked: the raid's only hole closed before Big Bang 1, then `urgent`'s cascade** (the uld5 and uld6 wipes).

- `--holes`: the shelter hole closed at 1:38.5 (uld5), 1:39.5 (uld6), 1:41.4 (uld1), w50 uld1 1:40.0, uld5 1:49.5,
  w46 uld6 1:44.2; none in the other 17 pulls of windows 46-48. `urgent` then kills `LowestStar`: uld5 1594 died 1:42.9 and closed 1:43.7, uld6
  1602 1:43.6 and 1:44.5; the third (1593 4.6 s later, 1603 3.9 s later) hit 18-19 under the line with a meteor
  live: 4 and 8 dead, 2 healers each, then the tanks.
- 5 of the 8 crossed the hole to the handler: uld1 (the DK's Icy Touch at 1:39.7 from 11.2 yd off the hole, across
  at 3.6 yd up), w50 uld1 (taunted from 19-30 yd), w50 uld5 (handler 8.8-11.3 yd off, gather), uld5 1594 (taunted
  across the dying star at 1:40.0), uld6 1602 (handler held them 4.9-7.8 yd from it as it died). The others: a dive
  past the hole (uld6 1:39.5), the swap tank's Consecration (uld5 1:38.5).
- Code: `GetAlgalonHandlerConstellation` chose `taunt` for any loose one and `AlgalonConstellationTauntAction` cast
  the class taunt, else `Attack`, from wherever the handler stood, line unchecked (the off-tank's is,
  `ConstellationPathClear`); holding with no spare hole (`kept`) it stood anywhere.

**Bug**: the strategy keeps the raid's holes off the kite with Big Bang due; the handler's taunts and held ones
did not. **Fix** (`UldEncounter_Algalon.cpp/.h`, `UldActions_Algalon.cpp`, `UldDefinition_Algalon.cpp`): raid
points are `IsRaidShelterHole`'s holes and, while `urgent` runs, `LowestStar`. `TauntLine`: a loose one's line to
the handler must clear them by 7.5 yd, else the handler walks to the nearest spot 12 or 20 yd from it whose line
clears them by 8-9.5 yd, 12 yd off them, 8 off every hole, clear of meteors, out of his reach (`side`); with none
(`blocked`) it leaves it, `AlgalonMayDamage` vetoing its damage, and the kite movement rule holds the generic
movers. `KeepSpot`: holding any with no spare hole or gathering, within 12 yd of a point it walks away to the
widest spot 14 yd off (`clear`, the kite action). Nothing changes outside the 60 s reserve or `urgent`. Replayed
on the 8 eats: the line passed 0.2-4.7 yd from the hole; a side spot lay 18-35 yd from the handler for all but uld5
1:38.5 (`blocked`). Dropped before build: re-picking the shelter star away from constellations, which changed no
re-pick within the tanks' 35 yd reach in windows 46-51.

**Reader.** `--holes` lists each raid hole a constellation closed with Big Bang due (`raid_holes_lost`; test
`test_a_raid_hole_closed_with_big_bang_due_names_the_eater_and_the_handler`); `side` and `clear` are kite stages.
w50-51: 17 in 12 pulls, 8 before Big Bang 1. `docs/raids/ulduar/algalon.md` records the rule.

Bots only, 10 and 25 alike; the handler is always a bot. Shared code: none.

**Next window should show.** `--holes`: no raid hole closed before Big Bang 1 by one on the handler (w50-51: 5),
and the rest only dives or a swap tank's pull. No `urgent` kills 4 s apart before Big Bang 1 (w51 uld5, uld6).
`algalon.kite` shows `side`/`clear`, `blocked` rare; `--constellations` still 0 on a healer at each cast; hole
stock at Big Bang 2 and 3 not lower.
