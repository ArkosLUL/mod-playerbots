# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--melee`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`, `--defensives`), tests in `tools/botobs/tests/test_algalon.py`.
Rounds 1-16: `history/algalon.md`.

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
What kills raids: Berserk once phase 1 runs past ~305 s (phase 2 takes 52-66 s on a kill; P1 + P2 must fit the
360 s); tank burst (holders take 73-110 k in 6 s with both tank healers alive), then him loose; a star going off on
the unphased raid while it is drained (an under-star released `early` or `late`, or above its band at the cast; a
deadline kill after Big Bang 2); an under-star dying in the cast before runners reach a hole.

Wave 3's stars belong under Big Bang 2. From r15 (Q6) a 5-member team works them on 25-man (P1 + 136.5 to the
cast). Wave 2's single star is never under Big Bang 1 in practice; it ends as a deadline kill at P1 + 162-169 s that
takes the 5-member team off wave 3 for 3-4 s, after which wave-3 stars go `late` (w48 5 in 4 pulls).

**Compositions on d9964f052 (window 48, round 16; one pull per pool).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 **kill** 6:42, 8 dead (3 after Berserk), P1 309 s; uld4 wipe 7:03: P1 331 s with 5 dead, Berserk in P2 (melee held off him by a hole 526 s) |
| B | uld2, uld5 | uld2 wipe 6:23 in P1: star 12, 5th stack loose at 5:56; uld5 wipe 7:12: P1 325 s with 1 dead, Berserk in P2 (melee held 262 s) |
| C | uld3, uld6 | uld3 wipe 6:03 in P1: two wave-3 `late` releases, off 6 s after Big Bang 2's hit (4 dead) and at 4:01 (3), Barrage 11; uld6 wipe 6:00 in P1: Big Bang 2's last star above its band at the cast, off 1.1 s after the hit (6 dead) |

A met the done rule on 37cc72a94 (windows 46-47: uld1 2 dead, uld4 2 dead); A and B on ce636f992 (windows 36-37),
C on 008c167b4: no build has all three yet. On 37cc72a94 B killed once with 8 dead (filler 47 uld5).

**C against A and B** (windows 36-37). Tanks: pair prot paladin + blood DK (A: prot paladin + prot warrior, B: prot
warrior + prot paladin), handler the bear (as B). Healers: disc, resto shaman, holy priest, resto druid, no holy
paladin (A, B both have one), so the tank healers are disc + resto druid. C fell 0.9-1.5 k/s behind the raid's
damage in P1's first 200 s in its collapses, so stars caught more of C under the line and Barrage killed more in C.

**Open failures, ranked (window 48, 169 deaths in 6 pulls: loose 66, star 36, on tank 31, Barrage 26, Big Bang 3,
other 7).**

1. Berserk: A and B pulls reaching P2 with ≤ 5 P1 deaths ran P1 324-331 s (uld4, uld5: 59 dead after Berserk);
   melee held off him by a hole across their way in (`--melee` P1 blocked 262-526 s against 0-136 in the kills,
   P2 up to 351 s past a Worm Hole). Picked r17.
2. Big Bang 2's wave-3 stars: `late` after wave 2's deadline kill (5), one `above` (uld6 1865, 6 dead), explosions
   after the hit on a drained raid (uld3 4 + 3 dead). The `above` picked r17 (hand-off cap).
3. Under-stars `early` from the finisher's own shots: 5 in 4 pulls, 1 dead.
4. Arcane Barrage 26, mostly on bots a star left low in the C collapses.
5. Tank burst: Q2 stands.

Leads, not picked:

- Wave 2's star paced between Big Bang 1's hit and wave 3's spawn (P1 + 98 .. 136.5): `recovery`, `raidhp` and
  meteors close the window, so it becomes the deadline kill that makes wave 3 `late` (w48 uld1, uld2, uld3, uld5).
- Finisher overshoot by its own shots: a hunter finisher's Explosive Shot ticks land after its finish ends (2.4-6%),
  plus Chimera 3.5% and Kill Shot 3.6% crossing the line between ticks (w48 uld1 1604, uld2 1595, uld4 1605, 1932,
  uld5 1852).
- An under-star dying at hit − 4..−3 s catches runners whose hole is 23-28 yd off (w46 uld6 BB2, 3 dead).
- Pair hit unphased at Big Bang (`noshelter`): w46 uld6 BB3 both died; w46 uld2, uld4 one each.
- Phase 2 Unleashed Dark Matter loose 9-34 s each (w48 uld1, uld4), Dark Matter killed 2 in uld5.
- Splash on a finished under-star (Living Bomb, Ignite, Wandering Plague; Mirror Image) (w46 uld6, uld3).
- Kite blocked 15-30 s: the handler in `taunt` with one held, a hole spare, its taunt cooling (w46, w48 uld1).
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
- r16: the finisher hand-off at 2.5 ticks over the line per team member (`FinishHandOverHp`) — held for the leavers
  (no `left` over the margin); its last-star risk came true once (uld6 1865).
- r17: a held melee bot out of his reach takes the melee spot (`MeleeHoleSpot`, `algalon.spot` `melee`); the
  hand-off capped by what the finisher alone does by 3 s before the cast — pending.

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

## Round 17 (2026-11-25, window 48 build d9964f052: round 16)

**Data.** `window48-algalon.md`; `filler47-algalon.md` (six more pulls on r15's 37cc72a94); every w48 pull's
`--verdict`, `--deaths`, `--stars`; the new `--melee` on all 12 traces. Scratch `alg17/` (P1 damage by role from
`snap.u[11]`, melee distance to him with walk-guard vetoes), folded into `--melee`.

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths (reader causes) |
|---|---|---|---|
| uld1 A | `603_9_…_1795607892` | kill 6:42, P1 309 s | 8: on tank 3, Barrage 2, loose, Smash, Big Bang (3 after Berserk) |
| uld4 A | `603_10_…_1795607892` | wipe 7:03, P1 331 s | 33: loose 21, on tank 6, Barrage 2, star 2, Smash, Big Bang (28 in P2) |
| uld2 B | `603_11_…_1795607912` | wipe 6:23 in P1 | 30: star 12, loose 9, on tank 5, Barrage 4 |
| uld5 B | `603_7_…_1795607871` | wipe 7:12, P1 325 s | 32: loose 21, on tank 7, Dark Matter 2, star, Big Bang (31 in P2) |
| uld3 C | `603_12_…_1795607913` | wipe 6:03 in P1 | 32: Barrage 11, star 8, loose 7, on tank 4, Black Hole DoT 2 |
| uld6 C | `603_8_…_1795607872` | wipe 6:00 in P1 | 34: star 13, Barrage 7, loose 7, on tank 6, other |

**Stop rules.** Not polished: A's kill had 8 dead, B and C no kill. Not stuck: melee held off by holes is a first
pick; the finisher hand-off its second (r16, r17), each time a different margin.

**Verdict on round 16: held for its target.** `early` releases 5 in 4 pulls (w46 6 in 4): uld1 1604, uld4 1605 and
1932, uld2 1595 (team 3), uld5 1852 (team 5); `left` 0.8-6.0% each, inside the 7.5 / 12.5 margin, and the share
over it came from the finisher, a hunter in every pull: Explosive Shot ticks after its finish ended (2.4-6.0%,
read as `left`), Chimera 3.5%, Kill Shot 3.6%, Auto Shot 1.5-2.1%. A 3, B 2, C 0; 1 dead of them (uld4 1932's
deadline kill). The watched risk came true once: uld6 1865 (C, team 5, Big Bang 2's last star) went to the finisher
7.9 s before the cast at 28.5% after wave 2's deadline kill held the team 3:11-3:15; alone it did +1.2 %/s beyond
the drain where it needed ~1.5: 11.3% at the cast over its 9.6% top, off 1.1 s after the hit, 23 hit, 6 dead.
Wave 3 under Big Bang 2: 8 of 15 (w46 12 of 19); `late` 5 (uld1 1873, uld2 1847, uld3 1851 and 1852, uld5 1851),
each after wave 2's deadline kill (lead).

**Picked: melee held off him by a hole (the Berserk wipes).** Bug: the strategy keeps melee on him, but nothing
brings a held one in.

- P1 damage by role (`snap.u[11]`): melee 43.8 k/s (uld4), 48.8 (uld5) against 52.0-57.8 in the kills (w48 uld1,
  f47 uld4, uld5); ranged 63-68 in all. Same roster on uld4: Ulduan (ret) 4.2 k/s against 7.0, Ulduah (DK) 4.5
  against 5.9.
- uld4: the star at 4:11.1 died 8.4 yd from Ulduaf, a hole 11 yd from him on the melee side. After the 4:18 meteor
  dodge Ulduan, Ulduah and Ulduar stood 30 yd off him until Big Bang 3 (42 s), then 38-49 yd off from 5:07 to
  phase 2 (50 s): `algalon walk guard` vetoed `reach melee` and `set behind`, the stop point 11.83 yd along their
  bearing lying 7.0 yd from the hole (< 7.5). `AlgalonWalkGuardMultiplier` (`UldMultipliers_Algalon.cpp:348`)
  only holds, and `RawSlot` (`UldEncounter_Algalon.cpp:1983`) gives a released melee bot no spot.
- `--melee` P1 seconds blocked (outside Big Bang casts): w48 uld4 526, uld5 262, uld3 73, uld6 55, uld2 6.5, uld1 0;
  f47 uld1 375, uld6 157, uld4 136, uld2 110, uld3 29, uld5 0. P2: f47 uld3 351 (Worm Hole across their way, P2
  99.7 s into Berserk), w48 uld4 146. P1 324-331 s with over 220 s blocked; 301-309 s in the kills.

**Second, small and independent: the hand-off cap** r16 named for one `above` on a cycle's last star (bug).

**Fix** (Algalon files only):

- `MeleeHoleSpot` (`UldEncounter_Algalon.cpp`), a `RawSlot` branch `melee` after `nearhole` and the star team's:
  a melee bot (no tank, not phased) on him, out of his melee range, whose straight way in the walk guard holds,
  gets the nearest of 16 points 3 yd inside his melee range (`ULDUAR_ALGALON_MELEE_SPOT_INSET`) in the room and
  8 yd clear of holes, a straight walk first; the raid position action walks it (detour round holes as a slot).
  Back in reach, or once its straight way in clears, the branch ends and the generic melee takes over.
- `FinishHandOverHp`: the margin is at most the finisher alone's work by 3 s before the cast (the bulk's closing
  speed, `BulkClosingPctPerSec` split out of `BulkWorkMs`, over the team's size); none under 3 s. uld6 1865 by hand:
  at r16's hand-off, 3:18.8, it stood ~11.7% over its line with 7.9 s to the cast, where the finisher
  alone closes at most ~4.9% (5 %/s capped closing over 5) by cast − 3 s, so the team keeps it; at the team's
  +3.1 %/s on it (3:00-3:11) the hand-off comes ~3:22.3 with under 1% left. A team slower than that (+0.5 %/s
  3:15-3:18, back from the deadline kill) still leaves it above its band at the cast.
- Reader `--melee` (per melee bot and phase: in reach / blocked / out, melee-spot seconds) and a verdict at 10 s
  blocked in a phase; tests `MeleeWayIn`. `docs/raids/ulduar/algalon.md`: walk guard, Bulk bullet, the trace.

Bots only, so a human melee or tank changes nothing; 10-man runs the same geometry. Shared code: none.

**Not picked:** finisher overshoot by its own shots (5, 1 dead); wave 2's deadline kill making wave 3 `late` (lead);
Barrage; Q2.

**Next window should show.** `--melee` P1 blocked under 20 s a pull (w48 up to 526) with `spot` seconds where a
hole sat by him, P2 blocked near 0; P1 under ~310 s where P1 deaths stay ≤ 5, and A and B pulls reaching P2 killing
before Berserk. `--stars`: no under-star `above` its band at the cast (w48 1) and `early` not above w48's 5.
Regression watch: a stray phase from `algalon raid position action` in `--phasing` (the spot walk into a field).
