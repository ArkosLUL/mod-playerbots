# Polish log: Algalon the Observer (25-man, no hard mode)

Reader `tools/botobs/bosses/algalon.py` (`--verdict` first, then `--deaths`, `--stars`, `--collateral`,
`--bigbang`, `--holes`, `--constellations`, `--defensives`), tests in `tools/botobs/tests/test_algalon.py`.
Rounds 1-12: `history/algalon.md`.

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
one finisher takes each star's last 5 ticks) and paces the rest one at a time, 8 s apart, clear of meteors, casts
and their aftermaths. The handler (the third tank) kites constellations into spare holes; the swap tank not holding
him taunts constellations off healers in the 30 s before each cast. Holes are the raid's shelter.
What kills raids: tank burst (holders take 66-161 k in 6 s with both tank healers alive), then him loose; a star
going off on the unphased raid while it is drained (the void's DoT after a hit, the wave-2 deadline kill at
~P1 + 168 s); a Big Bang with no hole in reach; Berserk once phase 2 runs past 360 s. Phase 1 takes 296-338 s of
the 360 s even on a good pull, phase 2 another 54-60 s on a kill.

Every pull has an explosion at ~3:14 (the wave-2 star's deadline kill, 20 s before Big Bang 2's hit). Wave 3's
two under-stars must then reach Big Bang 2's cast in band: one above it dies 5-10 s after the hit, on a raid the
void left drained.

**Compositions on ce636f992 (windows 36-37, round 12; A and B one pull per pool, C two).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 **kill** 6:29, 4 dead (3 tanks in P2) — meets the done rule; uld4 wipe 7:05, Berserk (P1 338 s, 30 of 32 after 6:26) |
| B | uld2, uld5 | uld5 **kill** 6:17, 3 dead (tanks) — meets the done rule; uld2 wipe 7:14, Berserk (P1 323 s, 28 after 6:26) |
| C | uld3, uld6 | 4 wipes: uld3 5:32 and 6:53, uld6 6:02 collapse in P1; uld6 7:19 Berserk (P1 321 s, 30 after 6:26) |

Best pull on disk: window 36 uld5, a kill with 3 dead. A and B met the done rule on ce636f992; every pool must kill
again on the next build.

**C against A and B.** Tanks: pair prot paladin + blood DK (A: prot paladin + prot warrior, B: prot warrior + prot
paladin), handler the bear (as B). Healers: disc, resto shaman, holy priest, resto druid, no holy paladin (A, B
both have one), so the tank healers are disc + resto druid. A and B's holy paladin puts ~27% of 2.2 M on the raid,
C's resto druid 3-20%. In P1's first 200 s A and B healed all the raid took (10.4-14.7 k/s); C fell 0.9-1.5 k/s
behind in its three collapses (healed 11.3-14.2 against 12.6-15.7) and kept pace only in uld6 window 36, the one
that reached P2 on time. So stars catch more of C under the line (15-19 against at most 8), Arcane Barrage kills
only in C (16 of 16), and the star team's mage goes into Ice Block at the 3:14 explosion (2 of 4 C pulls, 0 in A
or B). Constellations stay awake longer in C (1028-1375 s against 887-1048). The blood DK was the first pair death
in all three collapses.

**Open failures, ranked (windows 36-37, 200 deaths in 8 pulls).**

1. Berserk 96 (uld4 30, uld2 28, uld6 w36 30, uld3 w37 8): P1 ran 321-338 s. A damage and time limit.
2. C's P1 collapse 90 (uld3 w36 32, uld6 w37 33, uld3 w37 25 before Berserk): a wave-3 under-star above its band
   (17.9-18.7%) or released `late` at Big Bang 2's cast went off 5-9 s after the hit on 15/19/7 under (3 of 4 C
   pulls, 0 of 4 A/B), with tank and healer deaths around it. Picked r13.
3. Arcane Barrage 16 (all C), mostly double Barrages on bots a star left low.
4. Tank deaths on the kills (uld1 3, uld5 3), Quantum Strike / Phase Punch 21 mostly after Berserk.

Leads, not picked:

- A finisher that never touches its star: uld3 w37 star 8, the elemental shaman who joined the team after the
  hunter died walked between `star` and `reach spell` spots, dodged a meteor and dropped totems for 15 s
  (2:54.7-3:09.7, +0.3 %/s beyond the drain); the star was then released `late` and went off after Big Bang 2's
  hit (2 dead). Other finishes run +0.2-3.4 %/s.
- Under-star release reads a stale bulk speed: `CommitBulkSpeed` commits only when the focus leaves a star, so a
  team stalled on its last under-star (+0.1-0.9 %/s after the 3:14 explosion) still plans at the earlier span's
  speed and misses the `late` window (cast − 15 .. − 8 s).
- A class Ice Block at critical health (uld3 w36, uld6 w37) still runs 10 s; r13 breaks only the node's.
- Pair tanks hit unphased at Big Bang (`noshelter`); the second of two explosions is a star the team chewed while
  on another (r11); wave 2's star in the 16-20 s before Big Bang 2 (r9); splash on idle under-stars (r10-r12).

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
  `algalon.finisher`) — held (below band at the cast 9 of 31 → 7 of 49; early under-star deaths in a cast 22 → 1).
- r13: the tick breaks a personal-node Ice Block once its star is dead and none explodes on the bot within 4.5 s
  (`algalon.unblock`); class Deterrence held for unphased bots in P1 — pending window 38.

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

- Q6 (r8): a fourth ranged on the 25-man star team from wave 3's spawn to Big Bang 2's cast, only if two wave-3
  leftovers after Big Bang 2 become the rule (w36-37: one leftover in 4 of 8 pulls, never two).

## Round 13 (2026-11-25, windows 36-37 build ce636f992: round 12)

**Data.** `window36-algalon.md` (six pools), `window37-algalon.md` (two more C pulls); every pull's `--verdict`,
`--deaths`, `--stars`, `--constellations`, `--tankheal`, `--defensives` (uld3 w36/w37, uld6 w37), `--bigbang`
(uld3 w37). Scratch `alg13/`: `tanks.py` (pair intake and healing per second held), `raidheal.py` (raid damage
against raid healing, P1's first 200 s), `barrage.py`, `bdeaths.py`, `team.py` (star team casts in a span),
`blocks.py` (Ice Block and Deterrence spans, now `--defensives`).

| Pool | Trace `603_*_algalon-the-observer_*` | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 A | `603_12_…_1795555471` | kill 6:29 | 4: on tank 3, star 1 |
| uld4 A | `603_7_…_1795555345` | wipe 7:05 | 32: loose 22, on tank 6, star 2, Big Bang, Smash (30 after Berserk) |
| uld5 B | `603_11_…_1795555409` | kill 6:17 | 3: on tank 3 |
| uld2 B | `603_9_…_1795555376` | wipe 7:14 | 31: loose 21, on tank 6, star, Dark Matter, Big Bang, Smash (28 after Berserk) |
| uld3 C | `603_10_…_1795555409` | wipe 5:32 | 32 in P1: loose 18, Barrage 6, on tank 5, star, Big Bang, Dark Matter |
| uld3 C | `603_13_…_1795556082` | wipe 6:53 | 33: loose 15, on tank 6, Barrage 4, star 4, Black Hole DoT 3, Big Bang |
| uld6 C | `603_8_…_1795555346` | wipe 7:19 | 32: loose 21, on tank 6, 5 others (30 after Berserk) |
| uld6 C | `603_14_…_1795556082` | wipe 6:02 | 33 in P1: loose 13, star 6, on tank 5, Barrage 4, Dark Matter 3, 2 others |

**Stop rules.** Not polished: A (uld1, 4 dead) and B (uld5, 3) killed on ce636f992, C did not. Not stuck: C's
collapse is picked for the first time.

**Verdict on round 12: held** in A, B and C. Under-stars below their band at the cast 7 of 49 (w34: 9 of 31): A 1,
B 2, C 4 (uld6 w36); one went off early in a cast with a death (uld6 w36 5:00.6, 1 dead; w34 22). Finishes ran in
every pull. Above their band: 2 (w34: 0), both C's wave-3 star 7 at Big Bang 2 (uld3 w36 17.9%, uld6 w37 18.7%),
8-9 ticks over, never reached by a finish. A finisher that never touched its star: uld3 w37 (lead above).

**Picked: C's wave-3 under-star going off after Big Bang 2's hit.** uld3 w36 3:43.7 (hit + 9.1 s, 15 under, the
disc died to a Barrage 0.9 s later and the DK holder 2.5 s later: the collapse), uld3 w37 3:39.6 (hit + 5.0, 19
under, 2 dead), uld6 w37 3:40.6 (hit + 6.1, 7 under, 3 dead). In each the bulk stalled after the 3:14 deadline
explosion: star 7 +1.5 then +0.3 %/s (uld3 w36), +0.9 then +0.1 (uld6 w37), against +2-3.4 before it. Bug: two of
the three team members stop attacking at that explosion.

- uld6 w37: the arcane mage Uldwah, 91.9% of a ~20 k pool, under the 21.5 k top roll, so the personal node pressed
  Ice Block at 3:14.46 (`algalon.personal`); it sat the full 10 s to 3:24.46, no heal landing (`team.py`). The MM
  hunter Uldwak's class Deterrence (`low health`, 3.5 s after the hit) stopped its shots 3:17.99-3:22.99.
- uld3 w37: Uldzah Ice Block 3:14.42-3:24.43 the same way; uld3 w36: Uldzak Deterrence 3:16.4-3:21.4.
- Across the 8 pulls: 39 Deterrences (20 on team hunters), every one after the hit, never ahead of one; 8 node Ice
  Blocks, 5 the team mage's, each held 10 s. `ClaimAlgalonExplosionPersonal` (`UldEncounter_Algalon.cpp`) never
  cancels the immunity, and no rule holds the class Deterrence node.

**Fix** (Algalon files only):

- `BreakSpentIceBlocks` in the tick: a personal-node Ice Block (`personalBlocks`) comes off once its star has left
  the scan, no star explodes on the bot within 4.5 s (the press lead plus the stacking span), outside the Big Bang
  cast (the hide run already strips it) and unphased; probe `algalon.unblock`.
- `algalon deterrence hold` blocks the class `deterrence` node for unphased bots in phase 1; phased (Dark Matter)
  and phase 2 keep it.
- Reader: `--defensives` lists each Ice Block and Deterrence a bot raised on itself unphased in P1, the star team's
  share and the broken ones; verdict for a team member held over 2 s. Test `IdleButtons`.
- Doc: `docs/raids/ulduar/algalon.md` (Cooldowns: personal buttons; The trace).

10 and 25-man alike; a human's Ice Block and Deterrence are untouched (the map holds only the node's presses, the
block is a bot rule). Cost: a mage out of its block is exposed to a Barrage it would have ignored, at the same
health it would leave the block with; a low hunter loses a deflect it has never pressed in time.

**Questions for the user.** None.

**Next window should show** (`--defensives`, `--stars`, `--verdict`): "Ice Block and Deterrence … in P1, unphased"
with no Deterrence and every node Ice Block `broken by algalon.unblock` within ~1 s of its star (w36-37: 5-20 s of
team time per pull); Big Bang 2's wave-3 under-stars in their band at the cast, and no explosion within 16 s after
Big Bang 2's hit (w36-37: 3 of 4 C pulls); C a kill with at most 5 dead, A and B again. Watch: a mage dying to a
Barrage right after its block breaks, the finisher that never touches its star, P1 length against Berserk.
