# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--melee`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`, `--defensives`), tests in `tools/botobs/tests/test_algalon.py`.
Rounds 1-17: `history/algalon.md`.

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
takes the 5-member team off wave 3 for 3-4 s, after which wave-3 stars go `late` (w48 5 in 4 pulls, w50 3 in 2).

**Compositions on 0200b8827 (window 50, round 17; one pull per pool).** Six wipes; d9964f052 (window 48, r16) had
uld1's kill (8 dead, P1 309 s).

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 wipe 6:18 in P1 (352 s): 3 tanks dead 4:27-4:43, loose; uld4 wipe 6:55: P1 312 s with 13 dead, Berserk in P2 |
| B | uld2, uld5 | uld2 wipe 6:16 in P1 (350 s), loose 12; uld5 wipe 4:36: Big Bang 2's last star stalled above its band (Drain Soul flips), off 6 s after the hit on 22, every tank dead by 3:54, loose 29 s (19 dead) |
| C | uld3, uld6 | uld3 wipe 5:18: a team Kill Shot took Big Bang 2's last star under its band 1.8 s before the cast, off in it on 23 (19 not yet hiding); uld6 wipe 6:54: P1 333 s with 12 dead, Berserk in P2 |

A met the done rule on 37cc72a94 (windows 46-47: uld1 2 dead, uld4 2 dead); A and B on ce636f992 (windows 36-37),
C on 008c167b4: no build has all three yet. On 37cc72a94 B killed once with 8 dead (filler 47 uld5).

**C against A and B** (windows 36-37). Tanks: pair prot paladin + blood DK (A: prot paladin + prot warrior, B: prot
warrior + prot paladin), handler the bear (as B). Healers: disc, resto shaman, holy priest, resto druid, no holy
paladin (A, B both have one), so the tank healers are disc + resto druid. C fell 0.9-1.5 k/s behind the raid's
damage in P1's first 200 s in its collapses, so stars caught more of C under the line and Barrage killed more in C.

**Open failures, ranked (window 50, 193 deaths in 6 pulls: loose 77, star 36, on tank 30, Barrage 22, Smash 13,
Black Hole DoT 5, Dark Matter 3, Big Bang 3, other 4).**

1. Big Bang 2's last stars under r17's hand-off cap (backed out r18): the team kept them to the cast; `early` by
   team volleys with no hand-off (uld3 1840, uld4 1873), one stalled `above` (uld5 1888); uld3 and uld5 wiped on
   them. Plus `late` after wave 2's deadline kill (w50 3, w48 5).
2. Tank burst and him loose after it: Q2 stands.
3. Cosmic Smash in Big Bang 2's hide: the 3:30 meteor lands 4 s before the hit; hiders walked back inside 10 yd
   by the hide action (w50 5, w48 3) and low hiders inside 15 yd die (w50 8 dead at 3:30, w48 0).
4. Arcane Barrage 22.
5. Berserk: picked r17 (melee spot, held for blocked time); no A or B pull reached P2 with ≤ 5 P1 deaths in w50,
   so P1 length is untested.

Leads, not picked:

- Wave 2's star paced between Big Bang 1's hit and wave 3's spawn (P1 + 98 .. 136.5): `recovery`, `raidhp` and
  meteors close the window, so it becomes the deadline kill that makes wave 3 `late` (w48 uld1, uld2, uld3, uld5).
- A channel's ticks count in `pendingHp`: a team warlock's Drain Soul under 25% zeroes the star's work left, so the
  bulk drops and resumes each cast (`--stars` bulk breaks: w50 uld5 3, uld3 2, both under the cap; w48 none).
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
  (no `left` over the margin); its last-star risk came true once (uld6 1865). Restored r18 — pending.
- r17: a held melee bot out of his reach takes the melee spot (`MeleeHoleSpot`, `algalon.spot` `melee`) — held
  (P1 blocked 0-27 s a pull, w48 up to 526).

**Tried and rejected:**

- A "finish stars under 15%" bypass of the 8 s gap: two explosions 3.2 s apart, 12 dead.
- Wands on under-stars: slow, a damage dealer off him; a far star went off after the strip.
- DoTs on stars: ticked on past the team, 6 of 7 under-stars `early`.
- Opening the second wave-3 leftover inside the gap: lands 8-11 s after the first, worse (r8).
- Lengthening the meteor blackout: moves every due and serial deadline (r3, r9).
- Hiding before the cast for a star dying in its first seconds: the tanks hold him until the cast, and the
  `nearhole` spots are 22-30 yd out (r10).
- Capping the hand-off by what the finisher alone does by 3 s before the cast (r17, the bulk's speed over the
  team's size): the team kept Big Bang 2's last stars to the cast, two team volleys `early` and one stall `above`,
  2 wipes (w50); backed out r18.

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

## Round 18 (2026-11-25, window 50 build 0200b8827: round 17)

**Data.** `window50-algalon.md`, `window48-algalon.md`; every w50 pull's `--verdict`, `--deaths`; `--stars`,
`--smash`, `--melee`, `--phasing` on all 12 w48/w50 traces. Scratch `alg18/` (window flips with the channel cast
before them), folded into `--stars` as the bulk breaks.

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths (reader causes) |
|---|---|---|---|
| uld1 A | `603_18_…_1795613849` | wipe 6:18 in P1 | 34: loose 14, star 7, Barrage 6, on tank 6, Smash |
| uld4 A | `603_13_…_1795613694` | wipe 6:55, P1 312 s | 33: loose 17, on tank 6, star 4, Smash 2, other 2, Dark Matter, Big Bang |
| uld2 B | `603_15_…_1795613725` | wipe 6:16 in P1 | 30: loose 12, star 5, Barrage 5, on tank 5, Smash 3 |
| uld5 B | `603_14_…_1795613703` | wipe 4:36 in P1 | 31: loose 21, on tank 4, star 3, Barrage 2, Smash |
| uld3 C | `603_17_…_1795613788` | wipe 5:18 in P1 | 33: star 8, Barrage 7, Smash 5, on tank 4, loose 4, Black Hole DoT 2, Dark Matter 2, Big Bang |
| uld6 C | `603_16_…_1795613747` | wipe 6:54, P1 333 s | 32: star 9, loose 9, on tank 5, Black Hole DoT 3, Barrage 2, other 2, Smash, Big Bang |

**Stop rules.** Not polished: no kill. Not stuck: this round backs out a regression rather than picking a failure;
the finisher hand-off has now been touched in r16, r17 and r18, so a fourth round on it calls stuck.

**Verdict on round 17, per part.**

- Melee spot: **held.** `--melee` P1 seconds blocked by a hole (outside the casts): uld1 3.0, uld4 5.3, uld5 26.7,
  uld2, uld3, uld6 0 (w48: 6.5-526); P2 at most 1.7; spot seconds 0-10.8 a pull. No stray phase from the spot walk
  (`--phasing`: one stray, a tank face). A and B: no pull reached P2 with ≤ 5 P1 deaths (uld4 13, uld6 12), so its
  effect on P1 length is untested.
- Hand-off cap: **regressed, backed out.** Judged by mechanism, not by kill rate: the capped margin is 0 from 3 s
  before the cast and, for a 5-member team at +2.5 %/s, about half a tick per second left before that (6 at
  15 s, against r16's 12.5), so the team kept Big Bang 2's last star to the end; `--stars` shows each failure on
  that path, w48 none:
  - uld3 1840 (C): no hand-off; Uldzak's Kill Shot 7.7% from 11.6% 1.8 s before the cast, `early` at 3%, off in
    the cast at 3:26.7 on 23 outside a field (19 not yet hiding); 7 dead by 3:33, the pull fell from there.
  - uld4 1873 (A): no hand-off; Shadow Bolt 6.4%, Arcane Blast 4.1%, Mind Flay 2.4% from 28.8% to 16.5% 14.6 s
    before the cast, `early`.
  - uld5 1888 (B): no hand-off; 76 → 20% by 3:21.9 at +2.9 %/s, then idle: Uldval's Drain Soul on it under 25%
    zeroed `BulkLeftHp` through `pendingHp`, `UnderBulkStar` returned none, the window fell to `raidhp`,
    `StopCastsAtLine` cut the channel, and the bulk came back 0.3 s later, three times (3:21.9, 3:23.4, 3:24.5).
    14.4% at the cast over its 9.6% top, off 6.1 s after the hit on 22 (17 under the line, 2 dead); every tank dead
    by 3:54, loose 29 s, 19 dead. uld3's 1840 shows the same flips twice (3:20.0, 3:21.4).
  BB2 under-stars `early` 4 (w48 1), `above` 1 (w48 1); all cycles `early` 9 (w48 5).

**Picked: the backout** (bug: the cap broke the finisher hand-off r16 set up). `FinishHandOverHp` and
`HandOverFinish` (`UldEncounter_Algalon.cpp`) are r16's again: 2.5 ticks over the line per team member, no cap;
`BulkClosingPctPerSec` folds back into `BulkWorkMs`. The code now differs from d9964f052 only by `MeleeHoleSpot`.
r16's one last-star `above` (uld6 1865) stays a known risk.

**Reader.** `--stars` lists the bulk broken off for 2 s or less and back, with the last cast on its star in the
400 ms before (`bulk_breaks`), test `BulkBreaks`. `docs/raids/ulduar/algalon.md`: the Bulk bullet loses the cap and
records why; the `pendingHp` sentence records a channel's ticks.

Bots only, 10 and 25 alike; a human changes nothing. Shared code: none.

**Not picked:** the Drain Soul `pendingHp` flip (a lead: seen only under the cap); Cosmic Smash in Big Bang 2's
hide (8 dead at 3:30, most left low by Big Bang 2's stars); tank burst (Q2); Barrage.

**Next window should show.** `--stars`: Big Bang 2 under-stars with a hand-off before the cast (no `team, no
hand-off` `early`, w50 2), `early` back to w48's 5 or under across all cycles (w50 9), `above` at the cast at most
1, no bulk breaks to `raidhp`/`gap` (w50 5). `--melee` P1 blocked under 30 s a pull. Fewer Smash deaths at 3:30
(w50 8) once Big Bang 2's stars stop draining hiders. A and B pulls reaching P2 with ≤ 5 P1 deaths end P1 under
~310 s.
