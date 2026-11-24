# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`), tests in `tools/botobs/tests/test_algalon.py`. Rounds 1-11:
`history/algalon.md`.

## State

**Setup.** Trace slug `algalon` (pulls file under `algalon-the-observer`), raid `uld`, DC scenario `uld-algalon`
(Celestial Planetarium console with a lent key; he is passive until attacked). No hard mode. His health is the
snapshot `hp` of entry 32871. From window 34 the rosters carry all eleven professions at 450 with their perks and
enchants (`.dc test roster professions`): trends across window 33/34 compare unlike rosters.

**Fight model.** A 26 s intro, then phase 1 to 20%; Berserk (47008) 360 s after phase 1 starts, Ascend at 480 s.
Big Bang every 90.5 s (casts at P1 + 90, 180.5, 271, 361.5; 8 s), soaked by a Dispersion priest while the raid hides
in holes. Meteors every 25.5 s from P1 + 30.9: Big Bang 1 and 3 have one 8-10 s before the cast, Big Bang 2 one
inside it. Star waves of 4 / 1 / 4 / 4 every 60 s from P1 + 16.5 drain 1 %/s. The planner puts most stars under
Big Bang (the bulk takes each into a 4-5 tick band that dies in hit − 4 .. hit + 0.6, while the raid is phased;
since r12 one finisher takes each star's last 5 ticks) and paces the rest one at a time, 8 s apart, clear of
meteors, casts and their aftermaths. The handler (the third tank) kites constellations into spare holes; the swap
tank not holding him taunts constellations off healers in the 30 s before each cast. Holes are the raid's shelter.
What kills raids: tank burst (holders take 66-161 k in 6 s with both tank healers alive), then him loose; stars
going off on the unphased raid (under-stars below their band in a cast, two at once, on a drained raid); a Big
Bang with no hole in reach; Berserk once phase 2 runs past 360 s. Phase 1 takes 303-329 s of the 360 s even on a
good pull (w34), phase 2 another 68-82 s.

**Compositions on 6464b824a (window 34, one pull per pool, the first kills of the loop).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 wipe 5:09 in P1 (Uldxaz hit unphased at Big Bang 1, the pair dead 2:08-2:18); uld4 **kill** 6:47, 20 dead (15 after Berserk) |
| B | uld2, uld5 | uld2 **kill** 6:45, 10 dead (5 after Berserk); uld5 P2 at 5:54, wipe 7:16 to Berserk (30 of 33 dead after 6:26) |
| C | uld3, uld6 | uld3 wipe 4:23: two under-stars below their band went off 2.1-3.0 s into Big Bang 2's cast (15 dead); uld6 wipe 6:08 in P1 (stars, the pair 5:37-5:42) |

Best pull on disk: window 34 uld2, a kill with 5 dead before Berserk.

**Open failures, ranked (window 34, 158 deaths).**

1. Berserk 50 (uld4 15, uld2 5, uld5 30): phase 1 runs 303-329 s, the kills ended 19-22 s past it. A damage and
   time limit, not a bug.
2. Him loose and on tank before Berserk 41: uld1 after Big Bang 1's `noshelter` hit, uld3 after Big Bang 2's
   collapse, uld6 5:37-5:42 (healed 50-58 k of 105-107 k in 6 s). Q2 stands.
3. Star explosions 40: under-stars below their band going off in a cast 22 (uld3 15, uld6 4, uld1 2, uld2 1),
   picked r12; `deadline` kills on a drained raid ~13 (uld6 4:33.6 and 5:32.5, 7; uld3 4:07.6, 4).
4. Barrage 13, Cosmic Smash 10, Big Bang 3 (the P2 soaker at Big Bang 4, after Berserk).

Leads, not picked:

- Pair tanks hit unphased at Big Bang (`noshelter`, 5 in w34): uld1 Uldxaz 50-58 yd from every hole at Big Bang 1
  and 2, uld2 Uldyaa 43.9 yd; uld3 Uldzaa and uld6 Uldwaa on `hold` at Big Bang 2 with a constellation they got by
  threat, not a taunt, left 4.0-6.4 s before the hit and were still out. Tanks have no Nitro Boosts (r12).
- The second of two explosions is a star the team chewed while on another (r11); `StarWindow` returns `gap`
  before the due branch (r10); wave 2's star in the 16-20 s before Big Bang 2, `SerialDue` leaves the 10 s
  `smashrecovery` out of its budget (r9).
- Splash on idle under-stars: Wandering Plague, Fire Nova, Magma Totem, Living Bomb, Consecration, Death and
  Decay, the team's own Piercing Shots ticks (r10-r12).
- Personal buttons ready and unpressed at explosion deaths, every window (r2, r7).

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
  (`algalon.offtaunt`, user Q4 (b)) — held (w34: on a healer at the cast 10/16 → 3/17).
- r12: one finisher takes an under-star's last 5 ticks to its line while the team moves on (`algalon.finishstar`,
  `algalon.finisher`) — pending window 35.

**Tried and rejected:**

- A "finish stars under 15%" bypass of the 8 s gap: two explosions 3.2 s apart, 12 dead.
- Wands on under-stars: slow, a damage dealer off him; a far star went off after the strip.
- DoTs on stars: ticked on past the team, 6 of 7 under-stars `early`.
- Opening the second wave-3 leftover inside the gap: lands 8-11 s after the first, worse (r8).
- Lengthening the meteor blackout: moves every due and serial deadline (r3, r9).
- Hiding before the cast for a star dying in its first seconds: the tanks hold him until the cast, and the
  `nearhole` spots are 22-30 yd out (r10).

**User answers in force:**

- 2026-11-22: keep polishing on the ilvl 226 rosters; a kill isn't expected, so the stop rules (polished,
  stuck) don't apply.
- Q2 (r1): no 5th healer at 25.
- Q4 (a) (r4): two holes out of the kite from 60 s before each Big Bang.
- Q4 (b) (2026-11-24): in the 30 s before each Big Bang the swap tank not holding him taunts constellations off
  healers. Built r11.
- Q5 (2026-11-24): tank druids in bear form in every boss fight: keep as is for now. Algalon's own hold stays, no
  shared change.

**Open questions:**

- Q6 (r8): a fourth ranged on the 25-man star team from wave 3's spawn to Big Bang 2's cast, only if two wave-3
  leftovers after Big Bang 2 become the rule (w34: one wave-3 under-star released `late` at 57-70% in 4 of 6 pulls).

## Round 12 (2026-11-24, window 34 build 6464b824a: round 11, rosters with professions)

**Data.** `window34-algalon.md`, one valid pull per pool; every pull's `--deaths`, `--verdict`, `--bigbang`,
`--stars`, `--collateral`, `--constellations`; window 32's six traces for the band count. Scratch `alg12/`:
`line.py` (each bulk leave against the exact band from the star's drain clock, the hits landing after it with
their cast start, the star at the cast), `finish.py`, `starhp.py`.

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | `603_7_…_1795549549` | wipe 5:09 | 32: loose 15, star 8, Barrage 5, on tank 4 |
| uld4 | `603_8_…_1795549549` | kill 6:47 | 20: loose 8, on tank 6, star 3, Barrage, Smash, Big Bang 1 each (15 after Berserk) |
| uld2 | `603_11_…_1795549613` | kill 6:45 | 10: star 3, Smash 2, on tank 2, loose 2, Big Bang 1 (5 after Berserk) |
| uld5 | `603_9_…_1795549550` | wipe 7:16 | 33: loose 21, on tank 7, Smash 2, star, DoT, Big Bang 1 each (30 after Berserk) |
| uld3 | `603_12_…_1795549613` | wipe 4:23 | 32: star 14, loose 8, Smash 4, on tank 4, Barrage 2 |
| uld6 | `603_10_…_1795549592` | wipe 6:08 | 31: star 11, loose 9, Barrage 5, on tank 5, Smash 1 |

**Stop rules.** Waived (user, 2026-11-22). The kills (uld4 20 dead, uld2 10) are over the 5-death bar anyway.

**Verdict on round 11: held** in A, B and C. 15 `algalon.offtaunt` taunts in 17 casts (uld1 1, uld4 2, uld2 5,
uld5 2, uld3 2, uld6 3); constellations on a healer when a cast began 3 in 17 (w32: 10 in 16): uld2 Big Bang 1 and
3, uld6 Big Bang 2. Healer time in the 30 s before a cast: median 28.9 s (w32: 34.5), uld1 Big Bang 3 167.8 s with
the pair dead. No offtaunter was caught out at a cast; the two pair tanks hit unphased while holding a
constellation got theirs by threat.

**Picked: under-stars reaching the cast below their band** (`--stars` "at the cast": w34 9 of 31, 22 dead; w32 5).
Bug: the bulk stops at half a tick under the band's top (`StopLineHp`, `UldEncounter_Algalon.cpp:2561`) on the doc's premise that a spell landing after the stop is 1-2%, but the whole team's crossing volley landed 4-7%,
against 3.5-4.5 ticks of band under the line. `line.py` over the 32 bulk leaves: 30 were that volley (hits cast
before the star crossed), 2 a nibble inside the top half tick; 9 ended below the band, 8 of them volleys.

- uld6 1601: Arcane Blast 4.5% took it from 49.1% across its line to 44.6% (band 41..46], then a Shadow Bolt in
  flight 2.1% and an Auto Shot 1.9%: 3.6% at the cast. 1602: 1.0 s inside its top half tick, a hunter's Aimed Shot
  cast after the crossing plus an Auto Shot 4.8%: 3.1%. Both went off together 0.45 s before their window at
  1:59.6, killing the Dispersion soaker before its button and two runners.
- uld3 1863: a 5.1% Shadow Bolt crossed to 35.8% (band 35..40], Steady Shot and Piercing Shots: 3.6% at the cast,
  off 0.1 s before its window (3:29.6, 6 dead). 1862: a 5.0% Shadow Bolt to 22.7% (band 22..27], Death and Decay
  and Piercing Shots: released `early` at 13% (3:15.2), drained into the cast at 3:28.7 (9 dead).
- Bigger hits since the professions rosters: Shadow Bolt 5-6% crits.

**Fix** (Algalon files only):

- `HandOverFinish`, `ElectFinisher`, `ValidateUnderFinish` in `UnderBulkStar`: with 5 ticks or less above its line
  (`ULDUAR_ALGALON_UNDER_FINISH_TICKS`) the bulk star goes to one finisher, the team bot with the smallest hits
  (hunter, priest, shaman, druid, warlock, mage, the rest), while no finish runs and two members are able; the
  team takes the next under-star (the bulk gate stays open, it was running). The finish ends at the line, on a
  release, at the cast or with the finisher lost; a paced kill takes the finisher along and the finish resumes.
- `StarFocusOf`: each bot's own star for the team action, trigger and `OwnTargeting`, the target guard, the
  exclusions, `starhold` and `StopCastsAtLine` (the others' casts on the finish star cut at once, the
  finisher's at the line). Probes `algalon.finishstar`, `algalon.finisher`.
- Reader: `--stars` lists the finishes (finisher, health, what the rest of the team landed, the cast against the
  band) and counts under-stars below, in and above their band at the cast; the finisher on its finish star counts
  as on the focus in `--collateral`, the star groups and the team share. Tests added.
- Doc: `docs/raids/ulduar/algalon.md` (Stars, Bulk; The trace).

10 and 25-man alike (a team of 2 or 3); the team is bots only, so a human changes nothing. Replay by hand: uld6's
1601 is 4.6 ticks above its line at 1:18.2, so the mage's Arcane Blast landing at 1:18.5 is cut and the hunter's
1-2.9% shots finish it inside the band. Cost: one cut cast per other member per under-star, and the finisher's last
≤ 5% alone (3-5 s) beside the team's next star.

**Questions for the user.** None.

**Next window should show** (`algalon.py --stars`, `--verdict`): "bulk finishes" rows with a hunter finisher in
every pull; "at the cast, released early or not at all: N below their band" well under w34's 9 of 31; no
under-star going off in a cast before hit − 4 s (w34: uld3 Big Bang 2 15 dead, uld6 Big Bang 1 4). Watch: wave-3
`late` releases (w34: 4 of 6, left out of that count), under-stars `above` their band at the cast (w34: 0), a
finisher that never reaches its star, the bulk's median speed.
