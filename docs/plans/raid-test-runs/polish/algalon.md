# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--collateral`,
`--bigbang`, `--holes`), tests in `tools/botobs/tests/test_algalon.py`. Rounds 1-9: `history/algalon.md`.

## State

**Setup.** Trace slug `algalon` (pulls file under `algalon-the-observer`), raid `uld`, DC scenario `uld-algalon`
(Celestial Planetarium console with a lent key; he is passive until attacked). No hard mode. His health is the
snapshot `hp` of entry 32871.

**Fight model.** A 26 s intro, then phase 1 to 20%, enrage 360 s after it starts. Big Bang every 90.5 s (casts
at P1 + 90, 180.5, 271; 8 s), soaked by a Dispersion priest while the raid hides in holes. Meteors every 25.5 s
from P1 + 30.9: Big Bang 1 and 3 have one 8-10 s before the cast, Big Bang 2 one inside it. Star waves of 4 / 1 /
4 / 4 every 60 s from P1 + 16.5 drain 1 %/s. The planner puts most stars under Big Bang (the bulk takes each
into a 4-5 tick band that dies in hit − 4 .. hit + 0.6, while the raid is phased) and paces the rest one at a time,
8 s apart, clear of meteors, casts and their aftermaths. The handler kites constellations into spare holes;
holes are the raid's shelter. What kills raids: tank burst (holders take 66-161 k in 6 s with both tank
healers alive), then him loose; stars going off on the unphased raid (two at once, on a drained raid, or in a
cast's first seconds); a Big Bang with no hole in reach. At ilvl 226 he loses 0.22-0.26 %/s, so phase 1 alone
needs 320-365 of the 360 s even deathless.

**Compositions on 897faf0d8 (window 30, one pull per pool, every pull a phase 1 wipe).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 2:36, him at 74%, after two stars in Big Bang 1's cast; uld4 5:25 (38%), both swap tanks lost in Big Bang 2's hide |
| B | uld2, uld5 | uld2 6:12 (26%), the pair burst from 5:27; uld5 4:54 (43%) |
| C | uld3, uld6 | uld3 6:03 (34%); uld6 5:36 (42%) after a Big Bang 3 with no hole in reach |

Best pull on disk: window 21 uld3 (round 9's build), phase 2 at 6:03 with 5 dead, wipe 7:16 at 9.9%.

**Open failures, ranked (window 30, 193 deaths).**

1. Tank burst 31, then loose 60: a healing and gear limit (Q2 answered).
2. Star explosions 54: in a Big Bang cast's first seconds, before anyone hid, 18 (round 10's pick); the collapse
   15; paced kills on a drained raid 12; under-stars in uld6's holeless Big Bang 3 7; chewed stars in aftermaths 4.
3. Constellation Barrage 18, 14 of them in collapses.
4. Big Bang 16: uld6's Big Bang 3 13, its near holes eaten by constellations diving on healers (Q4 (b)).
5. Black Hole DoT 8: uld3's bots left low by Big Bang 3's stars, in the void.

Leads, not picked:

- The bulk's own overshoot at its line: casts landing in the tick it crosses, projectiles in flight (a Shadow Bolt
  flies ~1 s, Arcane Missiles keeps 2-3 missiles out), Auto Shot. 5 of 8 `early` releases on round 9's builds came
  within 1 s of the bulk leaving the star (r10).
- A cast in progress while the bulk's star sits between its band's top and the line lands (uld3 1666, Arcane
  Blast 3.9%); cutting from the band's top needs a way out of that half tick for teams without instants (r10).
- `StarWindow` returns `gap` before the due branch, so no due kill opens within 8 s of an explosion even when it
  would land after the gap (uld1 1584, r10).
- Wandering Plague, Living Bomb, Consecration and Magma Totem still chew stars (r10).
- An under-star above its band at the cast from a slow bulk (w21 uld2 1638 at 9.2%, bulk +0.4 %/s): it went off
  1.2 s after the hit, 14 dead.
- Wave 2's star in the 16-20 s before Big Bang 2: `SerialDue` leaves the 10 s `smashrecovery` out of its budget
  (r9).
- Swap tanks 30-41 yd from every hole at Big Bang 2 (w30 uld4: one ran through a meteor, one `noshelter` with no
  button).
- Personal buttons ready and unpressed at explosion deaths, every window (r2, r7).

**Fixes in place** (round: fix — verdict):

- r1: a dropped star keeps its slope 4 s; a kill colliding with another death drops — held.
- r2: no due kill lands 16 s after a hit while anyone is under the line (`recovery`) — held.
- r3: a kill carried across the hit drops while the raid is drained — held, rarely exercised.
- r4: constellations up to 7.5 yd count as low; a floor-moved kite spot refuses its hole — held.
- r5: two holes kept out of the kite from 60 s before each Big Bang (user Q4 (a)) — held; dives on healers still
  take them (w30 uld6).
- r6: the meteor wait skipped when a star dies in the cast; a released star draining into the cast stretches the
  hide — held.
- r7: no kill lands 5-10 s after an impact while anyone is under the line (`smashrecovery`) — held. Tank druids stay
  in bear form — held.
- r8: the ranged team holds 12-28 yd from its star (`starhold`) — held, walking 37% → 25-31%.
- r9: casts on an under-star the team left are cut at once; the target guard judges the spell's own target;
  Explosive Shot ticks count in `pendingHp` — held (team on idle under-stars 21% → 13% of a star a pull, no DoT on a
  star, `early` releases 2.0 → 0.9 a pull).
- r10: Revenge held near stars; bots' pets' area and chain autocasts off near stars (`algalon.petsplash`) —
  pending window 31.

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

**Open questions:**

- Q4 (b) (r4, not yet asked): a second taunter: the swap tank not holding him taunts constellations off healers
  in the 30 s before a Big Bang. Still matters: in w30 uld6 a constellation 10.5 s awake dove on a healer and ate
  the raid's last near hole 0.4 s before Big Bang 3 (20 dead). Recommendation: yes.
- Q5 (r7, shared code, another lane): keep tank druids in bear form in every boss fight. Recommendation: a druid
  lane.
- Q6 (r8): a fourth ranged on the 25-man star team from wave 3's spawn to Big Bang 2's cast, only if two wave-3
  leftovers after Big Bang 2 become the rule (w30: two `late` releases in uld4 and uld6).

## Round 10 (2026-11-24, window 30 build 897faf0d8: round 9 and every shared change since)

**Data.** Window 30, one valid pull per pool (`window30-algalon.md`), plus window 21 (build 90400b39c, round 9;
`603_16/17/18_…_17954618xx`, unread by any lane) for the verdict. Scratch `alg10/`: `offfocus.py` (team damage on
idle under-stars, `early` releases against the team's last switch), `undersplash.py` (hits on idle under-stars in
the 25 s before each cast, by source), `castearly.py` (stars going off in a cast before hit − 4.6 s, all 37
traces), `starhits.py`, `starnear.py`.

| Pool | Run | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|---|
| uld1 | `tr-20261124-171319-30` | `603_30_…_1795540502` | wipe 2:36, him 74% | 31: star 11, loose 7, on tank 6, Barrage 3, Dark Matter in Big Bang 2, Black Hole DoT 1, Big Bang 1 |
| uld4 | `tr-20261124-171207-28` | `603_28_…_1795540431` | wipe 5:25, 38% | 33: loose 16, Barrage 6, star 5, on tank 3, Big Bang 2, Cosmic Smash 1 |
| uld2 | `tr-20261124-171711-33` | `603_33_…_1795540734` | wipe 6:12, 26% | 32: star 11, loose 7, on tank 6, Barrage 5, Cosmic Smash 1, Dark Matter 1, Black Hole DoT 1 |
| uld5 | `tr-20261124-171249-29` | `603_29_…_1795540471` | wipe 4:54, 43% | 31: loose 13, star 9, on tank 6, Barrage 3 |
| uld3 | `tr-20261124-171339-31` | `603_31_…_1795540521` | wipe 6:03, 34% | 33: star 11, loose 10, Black Hole DoT 6, on tank 6 |
| uld6 | `tr-20261124-171339-32` | `603_32_…_1795540522` | wipe 5:36, 42% | 32: Big Bang 13, star 7, loose 7, on tank 4, Barrage 1 |

What started each wipe:

- uld1: two stars at 1:57.58, 1.5 s into Big Bang 1's cast, on 22 runners: 11 dead, 4 healers (the pick). Tanks
  died 2:07-2:23, then loose.
- uld4: Big Bang 2's hide, holes 30-41 yd from the pair: Ulduaa ran through a meteor (7.6 yd) into an under-star,
  Ulduaz on `noshelter` with no button died to the hit. 5th stack on the last tank at 4:28, loose 10.5 s.
- uld2: the pair died to burst 5:27-5:54 with a tank healer alive; a `deadline` kill on 13 under the line, 8 dead.
- uld5: under-star 1644 went off 0.8 s before its window with a meteor live (3 dead): the bulk's Arcane Missiles put
  8.6% on it in 1.1 s at its line. Tank deaths 4:06-4:25, tank healer alive 15-26 yd off.
- uld3: a `deadline` kill on 5 under (3:12.4, 4 dead); 1666 went off 0.6 s into Big Bang 3's cast (3 dead), 6 more to
  the void's DoT after it; tank deaths from 5:32.
- uld6: a constellation on the handler (4:40.5) and one diving on the healer Uldwad (4:56.7) ate the near holes;
  Big Bang 3's only hole stood 44-73 yd out: 13 to the hit, 7 to two under-stars.

**Stop rules.** Waived (user, 2026-11-22). The pick is round 9's family, under-stars cut below their band, a second
round.

**Verdict on round 9: held** (w21 and w30, 9 pulls). No `early` release within 3 s of a paced kill taking the team off
(w19: 1); the 5 within 1 s of a switch all followed the bulk's own. No DoT on a star (w19: 2). Team damage on
under-stars it wasn't on: 14.3, 13.1, 3.1 (w21); 12.6, 13.7, 12.8, 9.3, 10.8, 13.8% of a star (w30), against 23.1,
15.7, 21.2. `early` releases 1, 2, 0 and 2, 1, 1, 0, 1, 0 (w19: 4, 0, 2). Watch items: bulk +1.6 to +3.0 %/s beyond the
drain (w21 uld2 +0.4); w21 uld3 1642 at 12.1% at Big Bang 2's cast; `late` releases 6 in w30's 6 pulls.

**Picked: stars going off in a Big Bang cast's first seconds, before anyone hid** (18 dead, 4 of 6 pulls; uld1's
wipe). Since round 6, 8 such stars in 18 pulls killed 23, every one an under-star cut below its band, all but one
within 16.4 s of the cast. For Big Bang 1 and 3 the meteor 8-10 s before the cast, its blackout and `smashrecovery`
close the last ~14 s, so a late `early` release drains into the cast with the raid out. Bug: two splash sources
reach stars past the hold.

- uld1 1585, under, 14.6% at 1:48.1 (inside its band), then the holder Uldxaz's Revenge for 3775 and 3952 (1:50.8,
  1:55.9), Living Bomb 2765, Wandering Plague 2178, Magma Totem 772: 1.6% at the cast against its 4.0-9.6% band, gone
  on 1584's tick. Revenge (57823) is single-target in the DBC; Improved Revenge (12799) adds a melee chain jump, 10 yd
  from him (`Spell::SearchChainTargets`) plus the star's size, and `revenge` was in no hold set.
- Felguard Cleave (47994: weapon damage, chain 2) is a PetAI autocast, which no bot action or multiplier sees: 1.4%
  of the 2.9% that took uld3's 1666 below its band at 4:41.7 (the bulk's last Arcane Blast had left it 1.4% above the
  bottom), 0.7% of uld5's 1585.
- Off-team hits on idle under-stars in the 25 s before a cast, 9 pulls, % of a star: pet Cleave 4.8, Revenge 4.5,
  Living Bomb 4.1, Wandering Plague 3.5, Consecration 3.1, Maul 1.3.
- uld1 1584 (`early` 38 s out, the bulk 8% past its line) had no slot: due from 1:31.9, but the 1:31.8 shelter kill's
  gap, then the 1:48 meteor's blackout and `smashrecovery` ran into the prebang.

**Fix** (Algalon files only):

- `AlgalonSplashHoldMultiplier`: `revenge` joins the chains, held while a star is within 15 yd of the bot or 20 yd
  of him. Revenge went out 0-22 times a pull, half with a star that near.
- `HoldPetSplash` (tick): a bot's pet with a star within 15 yd of it, or the same reach of its victim (the star it
  hits aside), has its non-positive area and chain autocasts switched off (`Pet::ToggleAutocast`), remembered per
  owner and back on once none is near, outside phase 1, or before the wipe reset; `algalon.petsplash`
  `off|on <spell>` on the owner. Humans' pets are left alone.
- Reader: `--collateral` counts the holds per owner (test added).

10 and 25-man alike. Replay: uld1's 1585 keeps 3.0%, 4.6% at the cast, inside its band, so 1584 goes off alone at
cast + 1.5 s with 3 under the line; uld3's 1666 keeps 1.4%, landing on its band's bottom at 4:41.7. Cost: the
Felguard's Cleave on him while a star is near (one within 20 yd of him 37-44% of the time stars lived), a few
Revenges.

**Not picked.** The leads in State: the bulk's overshoot at its line and in-band casts, the `gap` return, the
remaining splash, w21 uld2's late under-star (14 dead), uld4's tanks far from holes, uld6's holes (Q4 (b)).

**Questions for the user.** Q4 (b), carried in State, not yet asked.

**Next window should show** (`algalon.py --collateral`, `--verdict`, `--stars`; scratch `undersplash.py`): no Revenge
and no pet Cleave or Thunderstomp in "damage on stars, off the star team" (w30: Revenge 7.5-27.2 k in 4 pulls, pet
Cleave 3.5-16.1 k in 6); `revenge` among the splash hold vetoes; a "pet splash autocast held" line for each Felguard
warlock, with `on` rows after each `off`; fewer "fell below its band" verdicts (w30: 2) and stars going off in a cast
before hit − 4.6 s (w30: 4, 18 dead). Watch: him loose by threat (w30: uld4 once), the Felguard's damage on him.
