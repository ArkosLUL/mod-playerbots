# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`), tests in `tools/botobs/tests/test_algalon.py`. Rounds 1-10:
`history/algalon.md`.

## State

**Setup.** Trace slug `algalon` (pulls file under `algalon-the-observer`), raid `uld`, DC scenario `uld-algalon`
(Celestial Planetarium console with a lent key; he is passive until attacked). No hard mode. His health is the
snapshot `hp` of entry 32871.

**Fight model.** A 26 s intro, then phase 1 to 20%, enrage 360 s after it starts. Big Bang every 90.5 s (casts
at P1 + 90, 180.5, 271; 8 s), soaked by a Dispersion priest while the raid hides in holes. Meteors every 25.5 s
from P1 + 30.9: Big Bang 1 and 3 have one 8-10 s before the cast, Big Bang 2 one inside it. Star waves of 4 / 1 /
4 / 4 every 60 s from P1 + 16.5 drain 1 %/s. The planner puts most stars under Big Bang (the bulk takes each
into a 4-5 tick band that dies in hit − 4 .. hit + 0.6, while the raid is phased) and paces the rest one at a time,
8 s apart, clear of meteors, casts and their aftermaths. The handler (the third tank) kites constellations into
spare holes; since r11 the swap tank not holding him also taunts constellations off healers in the 30 s before
each cast. Holes are the raid's shelter. What kills raids: tank burst (holders take 66-161 k in 6 s with both
tank healers alive), then him loose; stars going off on the unphased raid (two at once, on a drained raid, or in
a cast's first seconds); a Big Bang with no hole in reach. At ilvl 226 he loses 0.22-0.26 %/s, so phase 1 alone
needs 320-365 of the 360 s even deathless.

**Compositions on 36d7f19b9 (window 32, one pull per pool, every pull a phase 1 wipe).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 5:14 (him at 44%), the pair dead 3:26-3:57 after Big Bang 2, a reset at Big Bang 3; uld4 5:27 (38%) |
| B | uld2, uld5 | uld2 5:24 (38%), deathless to 4:09; uld5 5:26 (45%) |
| C | uld3, uld6 | uld3 4:16 (52%) after two stars 1.9 s apart before Big Bang 2; uld6 4:10 (53%), Big Bang 1's only hole 49-66 yd from the tanks |

Best pull on disk: window 21 uld3 (round 9's build), phase 2 at 6:03 with 5 dead, wipe 7:16 at 9.9%.

**Open failures, ranked (window 32, 192 deaths).**

1. Him loose 71, on tank 35: a healing and gear limit (Q2 answered). uld6's started at Big Bang 1: its only hole
   49-66 yd from all three tanks, Uldwaz hit, both pair tanks dead 2:07-2:11.
2. Star explosions 51: two within 2-9 s on a raid under the line (uld3 3:21.7 + 3:23.6, 17 dead; uld2 4:06.6 +
   4:09.7; uld5 4:06.6 + 4:15.6, 5 each), kills on a raid drained by him loose (uld1 4:17, 4:28; uld2 4:26, 7
   dead; uld4 4:05), stars in Big Bang 2 and 3's casts (6).
3. Constellation Barrage 13 (uld5 8).
4. Cosmic Smash 8, Big Bang 7, Black Hole DoT 7.

Leads, not picked:

- The second of two explosions is a star the team chewed while on another: uld2's 4:09.7 star lost 13.5% beyond
  its drain while not the focus (star team 35.3 k in its last 10 s), window `under` (r11).
- A far hole at Big Bang 1, whose one hole is the shelter star's: uld6's tanks 49-66 yd from it (r11); swap tanks
  30-41 yd from every hole at Big Bang 2 (w30 uld4).
- The bulk's own overshoot at its line: casts landing in the tick it crosses, projectiles in flight (a Shadow Bolt
  flies ~1 s, Arcane Missiles keeps 2-3 missiles out), Auto Shot (r10).
- A cast in progress while the bulk's star sits between its band's top and the line lands (uld3 1666, Arcane
  Blast 3.9%); cutting from the band's top needs a way out of that half tick for teams without instants (r10).
- `StarWindow` returns `gap` before the due branch, so no due kill opens within 8 s of an explosion even when it
  would land after the gap (uld1 1584, r10).
- Wandering Plague, Fire Nova, Magma Totem, Living Bomb and Consecration still chew stars: 3 under-stars fell
  below their band in w32 (r10, r11).
- Wave 2's star in the 16-20 s before Big Bang 2: `SerialDue` leaves the 10 s `smashrecovery` out of its budget
  (r9).
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
- r8: the ranged team holds 12-28 yd from its star (`starhold`) — held, walking 37% → 25-31%.
- r9: casts on an under-star the team left are cut at once; the target guard judges the spell's own target;
  Explosive Shot ticks count in `pendingHp` — held.
- r10: Revenge held near stars; bots' pets' area and chain autocasts off near stars (`algalon.petsplash`) — held
  (w32: no Revenge on a star, one 1.8 k Felguard Cleave in 6 pulls; stars in a cast's first 3.4 s 4 → 1, 18 → 0
  dead).
- r11: the swap tank not holding him taunts constellations off healers in the 30 s before each cast
  (`algalon.offtaunt`, user Q4 (b)) — pending window 33.

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
- Q4 (b) (2026-11-24): yes: in the 30 s before each Big Bang the swap tank not holding him taunts constellations
  off healers. Built r11.
- Q5 (2026-11-24): tank druids in bear form in every boss fight: keep as is for now. Algalon's own hold stays, no
  shared change.

**Open questions:**

- Q6 (r8): a fourth ranged on the 25-man star team from wave 3's spawn to Big Bang 2's cast, only if two wave-3
  leftovers after Big Bang 2 become the rule (w30: two `late` releases in uld4 and uld6).

## Round 11 (2026-11-24, window 32 build 36d7f19b9: round 10)

**Data.** Window 32, one valid pull per pool (`window32-algalon.md`); every pull's `--verdict`, `--deaths`,
`--collateral`, `--constellations`, `--holes`, `--bigbang`, `--stars`, `--loose`. Scratch `alg11/`: `gate.py`
(replays the r11 taunt gate on the 30 s before each cast), `bosshp.py` (his lowest health).

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | `603_12_…_1795545156` | wipe 5:14, him 44% | 34: loose 12, on tank 8, star 8, Big Bang 4, Black Hole DoT 1, Cosmic Smash 1 |
| uld4 | `603_7_…_1795545050` | wipe 5:27, 38% | 33: loose 14, star 7, on tank 6, Black Hole DoT 3, Cosmic Smash 2, Big Bang 1 |
| uld2 | `603_10_…_1795545151` | wipe 5:24, 38% | 31: star 12, loose 10, on tank 6, Barrage 2, Big Bang 1 |
| uld5 | `603_11_…_1795545151` | wipe 5:26, 45% | 30: star 8, Barrage 8, loose 8, on tank 5, Cosmic Smash 1 |
| uld3 | `603_9_…_1795545141` | wipe 4:16, 52% | 32: star 13, loose 7, on tank 6, Black Hole DoT 3, Cosmic Smash 2, Barrage 1 |
| uld6 | `603_8_…_1795545110` | wipe 4:10, 53% | 32: loose 20, on tank 4, star 3, Cosmic Smash 2, Barrage 2, Big Bang 1 |

What started each wipe:

- uld1: the pair died 3:26-3:57 (healed 42.8 k of 96.7 k in 6 s, both tank healers up), loose 9 s; stars on the
  drained raid 4:17 and 4:28 (8 dead); Big Bang 3 with the pair dead: the warrior soaker died, nobody unphased.
- uld4: a chewed star 4 s after Big Bang 2's hit on 16 under (2), a deadline kill 31 s after it on 11 under (4),
  tanks 4:09-4:10; Big Bang 3 with him loose 26.4 s, 17 dead.
- uld2: deathless to 4:09; star 6 (chewed, 3.0 s after 4:06.6's) on 15 under (5), an open kill at 4:26 on 15
  under (7), then the pair 4:31-4:44 and him loose 20.5 s.
- uld5: Barrage 2:18, 2:54, 4:08; a chewed star 9 s after 4:06.6's deadline kill on 17 under (5); the pair
  4:33-5:18.
- uld3: a deadline kill chewed at 3:21.7 and star 1611 drained 1.9 s later, 11-13 s before Big Bang 2: 17 dead.
- uld6: Big Bang 1's only hole 49-66 yd from all three tanks (`noshelter`); Uldwaz hit, both pair tanks dead
  2:07-2:11 (healed 14-15 k of 56-68 k); Uldwaa phased by a 5th Phase Punch at 3:26 (loose 11.8 s), then loose
  24.8 s from 3:41.8 (21 dead).

**Stop rules.** Waived (user, 2026-11-22).

**Verdict on round 10: held** in all three designs. Off the star team, no Revenge on a star in 6 pulls (w30:
7.5-27.2 k in 4); pet Cleave 1.8 k in uld5 only (w30: 3.5-16.1 k in 6), no Thunderstomp. A "pet splash autocast
held" line for every Felguard warlock (uld1 5, uld4 9 + a Thunderstomp 15, uld2 5, uld5 18, uld3 8, uld6 11),
`revenge` 5-10 vetoes a pull. Stars going off in a cast before hit − 4.6 s: 1 (uld2 1:57.5, nobody under the
line, 0 dead; w30: 4, 18 dead). Under-stars below their band 3 (w30: 2): Wandering Plague / Fire Nova / Magma
Totem (uld2), and two from the team's own casts (uld5 Shadow Bolt 5.1%, uld3 Arcane Missiles 2.4%).

**Picked: the user's Q4 (b)**, answered 2026-11-24 (build it this round). Constellations held healers through
the 30 s before 16 casts: 0.7-133.4 s of constellation time each (median 34.5 s), and 10 were on a healer when a
cast began (uld5 Big Bang 2: 4 on Uldvae; uld6 Big Bang 2: 3 on Uldwad, 2 on Uldwae; uld3 Big Bang 2: 1). A
healer holding one hides last and its dive can take the raid's near hole (w30 uld6, 20 dead); the handler's one
taunt per 8 s can't take three at once. The ranked worst, tank burst and him loose, is Q2's healing limit; the
star pairs are leads.

**Built** (Algalon files only):

- `GetAlgalonOffTankConstellation`: a swap tank that is not the handler, in the 30 s before a cast, picks the
  nearest active constellation on a healer within 28 yd, skipping the handler's selected target, whose straight
  path to it clears every hole by 7.5 yd (so the pull eats none). Only while its class taunt is ready and no swap
  can need it before its 8 s cooldown ends: the holder a pair tank, unphased, at 70% or more and 2 stacks or
  fewer; with no disperser latched, not within 23 s of the cast (the early swap's and handover's 15 s); not the
  soaker or backup, no swap or pickup due.
- `AlgalonOffTankTaunt` node at `ACTION_RAID + 6`, under the swap and pickup: casts the class taunt, notes
  `algalon.offtaunt` (the healer's guid). It never damages the constellation (target guard unchanged); at the
  cast the existing chased-bot hold sends it in last.
- Reader: `--constellations` prints, per Big Bang, constellation time on healers in the 30 s before, those on a
  healer at the cast, and the swap tank's taunts (test added).
- Doc: `docs/raids/ulduar/algalon.md` (constellations, the trace).

10 and 25-man alike; a human holder or healer changes nothing, and a human off-tank simply doesn't taunt.
Replay (`gate.py`, the gate without the path and range checks): with a healer holding one, the gate stood open
19.2 of 30.1 s before uld5's Big Bang 2, 23.5 of 25.5 s in uld2's, 19.0 of 29.7 s in uld3's, mostly shut by the
holder under 70%; shut in uld6's and uld1's Big Bang 3 with the pair dead.

**Questions for the user.** None.

**Next window should show** (`algalon.py --constellations`, `--verdict`): `algalon.offtaunt` taunts in the 30 s
before casts with a third tank handling; constellation time on healers there below w32's median 34.5 s per cast,
and "on a healer at the cast" near 0 (w32: 10 in 16 casts); fewer "a constellation loose on <healer>, not the
handler" verdicts. Watch: swaps late or a 5th Phase Punch (`--swaps`), pickups with the off-tank's taunt cooling
(`--loose`), a swap tank `noshelter` or hit at a cast while holding a constellation (`--bigbang`), holes lost in
the 30 s before casts (`--holes`).
