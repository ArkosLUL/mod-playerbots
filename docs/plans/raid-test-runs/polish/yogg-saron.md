# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases`), tests in `tools/botobs/tests/test_botobs.py`
(`YoggPhases`); rounds 1-19 in `history/yogg-saron.md`.

**Fight model.**

- **Phase 1** (131-158 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s (the code and
  reader still hold the old 11.6-60.8 at 3.0: Q1). Bot tanks pull Guardians onto the stack (6.5 yd), one focus per
  instance, melee stand in it and eat every nova, the back line holds the 21.5 yd station. Sara needs ~11 deaths
  inside 15 yd. A nova gap (6 s after any death, a Guardian under 30% is not killable), a chain rule (not killable
  beside a lower one) and an AoE hold space the novas. What kills: two novas inside the gap on the melee, and the
  first seconds of the 18 s handover (melee and tanks walk to the anchor at 21.5 yd, bearing 109°, with the
  leftovers). Melee lost here shrink every brain team after (melee first).
- **Phase 2** (355-440 s at 4-5 cycles). A portal wave every 80 s; the brain team (10, melee first, exactly one
  healer) clears an illusion room (Stormwind / Icecrown 20-30 s, Chamber 26-33 s), then hits the Brain (8.91M,
  scaled) until Induce Madness at 60 s: Stormwind / Icecrown 14-24% a cycle, Chamber 6-13%. The platform kills
  tentacles under Crusher Diminish Power (−20% a stack, carried below until the room clear's stun or a tank's
  break). Spawns speed up each wave, so a slow cycle grows the stock until Corruptor, Squeeze and Induce Madness
  wipe the raid in phase 2.
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies (30% floor), one spawns every 10 s and beacon
  heals restore them; the tanks fall at +50-120 s and the pile wipes the raid. Yogg ends at 10.6-23.6% (window
  29), 7.5% at best (window 21 uld3, one Crusher at the start).

**Compositions on 897faf0d8 (window 29, one pull per pool, no kill).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | phase 3, Yogg 23.6% / 10.6%; 3 / 2 nova deaths |
| B | uld2, uld5 | uld2 phase 3, Yogg 10.8%; uld5 lost 4 melee in phase 1, then a 903 s phase 2 collapse, Brain 71.8% |
| C | uld3, uld6 | uld3 lost 7 of 10 melee to novas (9 deaths), phase 2 collapse, Brain 50.9%; uld6 phase 3, Yogg 13.5% |

**Open failures, ranked (window 29).**

1. Phase 3 Immortal Guardians, 118. Stuck (rounds 3-5): a throughput race, not picked again without a new root cause.
2. Phase 2 collapse, 49 (uld3 23, uld5 26: Corruptor 21, Squeeze 12, Induce Madness 11), downstream of phase 1
   melee losses and slow cycles.
3. Phase 1 Shadow Nova, 20, all melee: 9 on the second nova of a pair inside the gap, 11 at the handover's start, 4
   of them the enhancement shamans' Reincarnation loop. Round 20 targets the pairs.
4. Outside the collapses: Death Ray 2, Lunatic Gaze 1, Sara's Blessing 1.

Leads, not picked: Death Ray victims on a body detour walk issue no dodge for ~1 s (round 19, uld2 8:00.4); the
pet fallback sends a pet to an add inside a swinging Crusher's reach (round 17); a Fervor tank's run drags its
Guardians off the stack (round 19); `p1station`'s band test reads `GetDistance2d`, so `parked` never latches
(round 19); `YoggSaronCloudLead` projects at run speed against 2.0 yd/s clouds (round 19, unchecked); Crusher reach
4.5 since `2026_11_06_01`, the reader and doc say 8 (round 18); Army of the Dead ghouls wake phase 3 Guardians in
the spawn ring; a Diminish Power claim has no timeout; the judgement node spends a holy paladin's GCD for nothing;
casts finishing on a Guardian gone unkillable and pets left on it (round 20).

**Fixes in place.**

- r1 bot-led raid walks into the room (`YoggSaronBotLedRaid`): fixed.
- r2 Brain melee spot inside the re-sniffed 29.83 yd swing: fixed.
- r3 Crusher area hold (r15 adds Bladestorm, Sweeping Strikes, Blade Flurry; r20 summons): fixed.
- r4 phase 3 class taunt hold: working.
- r5 ranged DoTs on Yogg in phase 3, r8 Flame Shock (user): working.
- r6 nova ladder needMs counts down to the nova: fixed.
- r7 Guardians read by centre, bot tank pulls them onto the stack: fixed.
- r8 own-portal clicks: spent clicks fixed; late entries left to r11.
- r9 exit cast hold before the latch: working.
- r10 one healer per brain team: fixed.
- r11 body detour held on a held portal spot, nearest-pair spots: fixed.
- r12 stage wait, join only within 40 yd (user): fixed. Reprisal heal first (user): fires, not mostly on the held bot.
- r13 sight regroup: works on its own check. Casts cut under a Death Ray: fixed.
- r14 exit lead 1.2× (user): kept, no melee charmed.
- r16 a bot tank breaks Diminish Power once the team is below (user (b)): kept; late when a straggler holds it (Q2).
- r17 chain taunt at stasis end: fixed. Burst on phase 3 adds (shared `BurstCooldowns`): fixed on its own check.
- r18 swap deferral for a Guardian on a raider (user): kept, 3 deferrals, no tank death. Phase 3 melee held at the
  spot: fixed (82-88% within 15 yd).
- r19 back-line dodges stay outside the band: fixed (0 inside, 0 back-line nova hits in window 29).
- r20 AoE hold by `HitsNeighbours`, tank's stack attack killable first: pending window 30.

**Tried and rejected.**

- Leaving a beacon-healed Guardian alone in phase 3 (r3): user declined, too risky.
- Round 9's join of any live tentacle: made cycles worse (joiners ended 53-85 yd out); narrowed in r12.
- Phase 3 ranged on the lowest leftover Crusher only (r7): user wants phase 3 to start with at most one Crusher.
- Ranged-first brain team (r8): user keeps melee first.
- Healer under 30% mana sent after the others (r13): withdrawn in r15, no case seen.
- Melee stepping out of the phase 1 nova (findings doc): late on 44 of 74, woke Guardians.

**User answers in force.** Keepers: Freya only. Phase 3: no leave-alone for healed Guardians; ranged keep DoTs (Flame
Shock included) on Yogg; a bot tank taunts a fresh Guardian on a non-tank before a swap while the holder is over
35%. Phase 2 goal: start phase 3 with at most one Crusher. Brain team melee first, exactly one healer; a finished bot
waits at the stage unless a live tentacle stands within 40 yd of it; the room healer heals a reprisal-held bot first;
exit lead 1.2×; Diminish Power broken by a bot tank after the team clicks. Left alone by choice: the shaman's
Reincarnation onto the stack, late Guardians woken by the back line, the raid working the next Guardian while a low
one waits out its gap.

**Open questions (unanswered, not built).**

- **Q1 (r19), cloud geometry.** Re-derive the phase 1 positions from the measured orbits (15.3 / 20.9 / 27.4 / 41.4 /
  48.4 / 54.1 yd, 2.0 yd/s): the 21.5 yd station is reached by three orbits, not one, and melee could stand out to
  ~6.8 yd without waking orbit 1. Recommended: yes, in two steps, the reader's orbit labels and a per-orbit cost at
  the station first, then one navprobed position change at a time, the melee core first.
- **Q2 (r19), break per carrier.** The Diminish Power break fires once every team member is below, so a straggler
  held it to +19-25 s and the team carried ×0.85-0.51 for 26-40 s. Fire it once the bots carrying that Crusher's
  stack are below? Recommended: yes, for one window.
- **r18, deferral scope.** The build defers a swap for any Guardian on a raider, fresh or slipped, about twice what
  the answer named. Recommended: keep; no answer recorded.

## Round 20 (2026-11-24, window 29 build 897faf0d8: rounds 17-19)

**Data.** Window report `scratchpad/window29/window29/window29-yogg-saron.md` (`raidtest.py report --boss yogg-saron
--raid uld --head 897faf0d8`): six valid pulls, one per pool, 0 humans, Freya on the raid. Traces
`603_{4,5,6,7,8,9}_yogg-saron_17955377{74,59,54,64,70,70}` (uld1, uld2, uld3, uld4, uld5, uld6), read with
`yogg_saron.py --novas --phases --brain --portals --tentacles --phase2 --phase3 --crush --clouds`, `postmortem.py`
and scratch scripts (`scratchpad/yogg-r20/`: each phase 1 Guardian pair under 6 s with who targeted and cast on
the second, and over the 27 pulls of windows 19-29 every gap window's loss, aimed damage and area actions).

Deaths by mechanic:

- uld1 (A): nova 3 (Uldxai 1:35 and 2:17, Uldxat 2:09); phase 3 Immortal Guardian 29, Yogg 23.6%.
- uld2 (B): Sara's Blessing 1, nova 1 (2:11); phase 3 Immortal Guardian 29, Yogg 10.8%.
- uld3 (C): nova 9 (0:58.3 five melee, a pair 2.2 s apart; Uldzaj 0:42 and 1:22; 2:28 two); phase 2 from 6:32
  Corruptor 8, Squeeze 5, Induce Madness 7, Death Ray, Crush, Brain Link 1 each.
- uld4 (A): nova 2 (2:12), Death Ray 1; phase 3 Immortal Guardian 30, Yogg 10.6%.
- uld5 (B): nova 4 (1:09; 2:12 two, a 2.2 s pair; 2:17); healer to a Death Ray 3:08; phase 2 Corruptor 13, Squeeze 7,
  Induce Madness 4, Death Ray 1, Sara 1 (17:01).
- uld6 (C): nova 1, Lunatic Gaze 1; phase 3 Immortal Guardian 30, Yogg 13.5%.

**Verdict on round 19 (back-line dodges stay outside the band): fixed.** Dodges inside 20.5 yd 0 of 93 / 83 / 107 /
133 / 159 / 93 (window 27: 36 / 35 / 33), hits on one standing there 0 (12 / 1 / 1), back line caught in phase 1 0 in
all six (9 / 1 / 2), every nova death a melee. `the raid's own feet` 5-8 (6 / 5 / 5), phase 1 137-158 s (131-141; uld3's
158 s lost seven melee). Round 18: swaps deferred 1 / 2 / 0 / 0, no tank death within 10 s; phase 3 melee within 15 yd
of the spot 82 / 88 / 86 / 86%. Round 17's chain: 10 of 20 short taunts chained, 3 non-tanks swung at (uld4).

**Stop rules.** No kill. Phase 3 stays stuck. Phase 1 novas were picked in rounds 6, 7 and 19 for other root causes,
each fixed on its own check (ladder stacks, Guardians off the stack, back-line walks); this one is the gap leak.

**Ranking.** Phase 3 118 (stuck); phase 2 collapse 49, both behind phase 1 melee losses; phase 1 nova 20; Death Ray
2 outside the collapses.

**Picked: the second Guardian of a pair dies inside the nova gap.** uld3 0:56.140: a Guardian died 5.3 yd out and its
nova chain took 8862 (1.8 yd out, 3.5 yd from it) from 11.3% to 4.8%; 8862 lost the last 50.6k in 2.0 s with no
non-tank targeting it.
Both tanks had it as their target (Uldzaa's Judgement of Vengeance and Vindication on it), Uldzay's Whirlwind at
0:56.179 put Deep Wounds on it, Uldzam's Death and Decay ticked on it; its nova at 0:58.347 killed Uldzao, Uldzax,
Uldzay, Uldzaw and Uldzai. uld5 2:10.106: the second sat at 7.5% → 3.6% with Uldvaa (Cleave, Heroic Strike, Shield
Slam) and Uldvab on it, and died 2.2 s later on Uldvax and Uldvay.

Over windows 19-29 (27 pulls, 202 gap windows, the new `--phases` line): a low Guardian within 10 yd of the middle
lost 3.0-3.9%/s inside the gap, 34k/s, so one under ~20% at the first death never outlasts it. Bots aimed 9.6k/s at
it (ranged still on it 5.1k, tanks 3.8k, melee 0.6k), the rest was splash. Two causes in the strategy's own
guards:

1. `YoggSaronPhase1StackTarget` (`UldEncounter_YoggSaron.cpp:3719-3737` before the fix, called from
   `ControlPhaseOne`, `UldActions_YoggSaron.cpp:2063`) gave the tank the lowest Guardian in melee reach, which inside
   a gap is the one held back: a tank on it in 64-100% of gap samples, 1.23 tanks a sample.
2. `YoggSaronPhase1AoeHoldMultiplier` (`UldMultipliers_YoggSaron.cpp:171-184` before) held only `Aoe`-typed
   actions, so in the same windows it vetoed Seed, Chain Lightning, Volley and Blizzard while Maul 334, Swipe (Bear)
   208, Consecration 169, Multi-Shot 139, Divine Storm 85, Whirlwind 71, Thunder Clap 46, Hammer of the Righteous
   37 and Magma Totem 31 ran. The doc names Consecration and Explosive Trap among its reaches; neither is typed `Aoe`.

Bug: the gap and the hold exist to keep that Guardian alive for 6 s. Fix (`UldMultipliers_YoggSaron.h/.cpp`,
`UldEncounter_YoggSaron.cpp`): `HitsNeighbours`, the Crusher hold's test made shared in the file (`IsRaidAreaDamage`
plus Maul, Hammer of the Righteous, Heart Strike, Avenger's Shield; heals and a positive spell exempt unless it is a
named multi-target action, a summon or periodically triggers on enemies), now keys the phase 1 and leftover AoE
hold too; summons were missing from the Crusher hold's exemption test, so Magma Totem and Explosive Trap now count
there as well. The tank's stack attack takes the lowest killable Guardian in reach, else the healthiest. Same at 10
and 25, normal and hard mode, with or without a player (a human's area spells and target are his own). No shared
code changed (`IsRaidAreaDamage` and `RaidSplashCache` only read). Reader: `--phases` `low Guardians in the gap`
(`gap_leak`, `actions_in_windows`, tested).

**Next window should show:** `low Guardians in the gap` under 3.0-3.9%/s, `a tank on one` near 0% (64-100%), `area
run` without Maul, Swipe, Consecration, Divine Storm, Whirlwind, Thunder Clap or Multi-Shot; fewer pairs under the
6 s gap than 1-3 a pull and fewer nova deaths on a pair than 9 (uld3 5, uld5 3, uld1 1). Back it out if phase 1 runs
past ~165 s or Guardians start swinging at non-tanks in phase 1 (the tanks lose area threat during holds).

**Seen, not picked.**

- The handover's first seconds took 11 of the 20 nova deaths, mostly single novas on melee walking to the anchor;
  the doc's open item (the pile is one blast).
- Ranged finish cast-time spells on a Guardian gone unkillable (the AI yields while casting), 5.1k/s of the gap
  loss, and 1.35 pets a sample sat on low Guardians.
- uld5's healer Uldvae rode one Constrictor for 54 s (6:44-7:38) and died to Squeeze.
