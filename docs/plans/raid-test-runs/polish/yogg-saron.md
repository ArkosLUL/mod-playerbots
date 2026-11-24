# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases`), tests in `tools/botobs/tests/test_botobs.py`
(`YoggPhases`); rounds 1-21 in `history/yogg-saron.md`.

**Fight model.**

- **Phase 1** (131-158 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s; the reader uses
  them since r22, the code and `yogg.cloudreach` still hold the old 11.6-60.8 at 3.0 (Q1 step 2). The 21.5 yd station
  sits in reach of orbits 1-3 and costs 4-5 of a phase 1's 14-17 Guardians, about one per pass (`--clouds` `the
  station's cost by orbit`). Bot tanks pull Guardians onto the stack (6.5 yd), one focus per instance, melee stand in
  it and eat every nova, the back line holds the station. Sara needs ~11 deaths inside 15 yd. A nova gap (6 s after
  any death, a Guardian under 30% is not killable), a chain rule (not killable beside a lower one) and a non-tank AoE
  hold space the novas; the tanks' area threat keeps the Guardians nobody aims at off the DoT casters. What kills: a
  chain of novas whose later Guardians are under 10% when the first dies, a nova on the enhancement shaman (bar
  20.6-23.0k against an unresisted 20.9-23.8k), the first seconds of the 18 s handover, and Guardians piling up off
  the stack, whose Dark Volley (every 8 s each) kills the raid once phase 1 ends. Melee lost here shrink every brain
  team after (melee first).
- **Phase 2** (356-440 s at 4-5 cycles). A portal wave every 80 s; the brain team (10, melee first, exactly one
  healer) clears an illusion room (Stormwind / Icecrown 20-30 s, Chamber 26-33 s), then hits the Brain (8.91M,
  scaled) until Induce Madness at 60 s: Stormwind / Icecrown 14-25% a cycle, Chamber 6-13%. The team carries Crusher
  Diminish Power below (−20% a stack) until the room clear's stun or a tank's break. The platform kills tentacles;
  spawns speed up each wave, so phase 3 starts with 3-10 tentacles (1.8-8.3M, window 33) and a slow cycle grows the
  stock until Corruptor, Squeeze and Induce Madness wipe the raid in phase 2.
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies (30% floor), one spawns every 10 s, each leaves
  its tank at stasis end (+4 s) and beacon heals restore them; the tanks fall at +40-120 s and the pile wipes the
  raid. Yogg ends lower the smaller the phase 2 leftover stock: 11.7% with 3 tentacles, 22.6-27.9% with 5-10
  (window 33); window 31's best 3.9%.

**Compositions on 9ad197722 (window 33, one pull per pool, no kill, all six in phase 3).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 Yogg 27.9% (10 tentacles at phase 3); uld4 11.7% (4 cycles, 3 tentacles) |
| B | uld2, uld5 | uld2 Yogg 22.6% (5 tentacles); uld5 25.5% (6) |
| C | uld3, uld6 | uld3 Yogg 26.0% (7, a nova chain killed 3 melee in phase 1); uld6 24.9% (7) |

**Open failures, ranked (window 33).**

1. Phase 3 Immortal Guardians, 155. Stuck (rounds 3-5): a throughput race, not picked again without a new root cause.
2. Phase 2 cycle rate: the leftover stock at phase 3 (3-10 tentacles) sets Yogg's end. Diminish Power carried below
   0.5-26.5 s a wave, ×0.73-1.00 to the door; 11 of 27 breaks claimed 9.5-16.9 s after the first carrier went down
   (r22 builds Q2).
3. Shadow Nova, 9 (window 31: 28): enhancement shaman from full 5, a chain of three under 10% 3 (uld3), Fervor
   doubled 1.
4. Death Ray 6, Lunatic Gaze 5, Curse of Doom 4, Draining Poison 4, Squeeze 2, Deafening Roar 2.

Leads, not picked: Death Ray victims on a body detour walk issue no dodge for ~1 s (round 19); the pet fallback sends
a pet to an add inside a swinging Crusher's reach (round 17); a Fervor tank's run drags its Guardians off the stack
(round 19); `p1station`'s band test reads `GetDistance2d`, so `parked` never latches (round 19);
`YoggSaronCloudLead` projects at run speed against 2.0 yd/s clouds (round 19, unchecked); Crusher reach 4.5 since
`2026_11_06_01`, the reader and doc say 8 (round 18); Army of the Dead ghouls wake phase 3 Guardians in the spawn
ring; a Diminish Power claim has no timeout, and some claims walk 13-26 s and break nothing before the stun
(window 33); the judgement node spends a holy paladin's GCD for nothing; casts finishing on a Guardian gone
unkillable and pets left on it (round 20); tanks on a gap-held Guardian come 87% from `tank assist` or a target kept
from before the gap (round 20 review), and `::TankTarget` stays out of the Yogg exclusions on purpose, so a tank-side
fix is a question; the enhancement shaman dies to one unresisted nova from full, worse when the pull starts without
Blood Pact or Commanding Shout (round 21); back-line Guardians the DK's Dark Command cannot reach (36-45 yd, round
21); B's stack Guardians still turn off a tank 4.1-4.8 a minute, warriors 6 of 12 in uld2 (window 33).

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
- r16 a bot tank breaks Diminish Power (user (b)): kept, 0 Crushes on a breaking tank; r22 drops its team-down wait.
- r17 chain taunt at stasis end: fixed. Burst on phase 3 adds (shared `BurstCooldowns`): fixed on its own check.
- r18 swap deferral for a Guardian on a raider (user): kept, no tank death. Phase 3 melee held at the spot: fixed.
- r19 back-line dodges stay outside the band: fixed (0 inside, 0 back-line nova hits in windows 29-33).
- r20 AoE hold by `HitsNeighbours`, tank's stack attack killable first: fixed on its target for non-tanks.
- r21 tanks keep their area threat under the phase 1 and leftover hold: fixed (turns off a tank 2.5 a minute, was
  5.7; no tank veto; Dark Volley 0, was 21; phase 1 nova deaths 9, was 28).
- r22 reader on the measured orbits plus the station's cost per orbit (Q1 step 1): reader only.
- r22 Diminish Power break once that Crusher's stack is carried below, no team-down wait (Q2): pending, one window.

**Tried and rejected.**

- Leaving a beacon-healed Guardian alone in phase 3 (r3): user declined, too risky.
- Round 9's join of any live tentacle: made cycles worse (joiners ended 53-85 yd out); narrowed in r12.
- Phase 3 ranged on the lowest leftover Crusher only (r7): user wants phase 3 to start with at most one Crusher.
- Ranged-first brain team (r8): user keeps melee first.
- Healer under 30% mana sent after the others (r13): withdrawn in r15, no case seen.
- Melee stepping out of the phase 1 nova (findings doc): late on 44 of 74, woke Guardians.
- Holding the tanks' area damage in the phase 1 hold (r20): Guardians on the stack turned off a tank 5.7 a minute,
  was 2.7-3.4; backed out for tanks in r21.

**User answers in force.** Keepers: Freya only. Phase 3: no leave-alone for healed Guardians; ranged keep DoTs (Flame
Shock included) on Yogg; a bot tank taunts a fresh Guardian on a non-tank before a swap while the holder is over
35%. Phase 2 goal: start phase 3 with at most one Crusher. Brain team melee first, exactly one healer; a finished bot
waits at the stage unless a live tentacle stands within 40 yd of it; the room healer heals a reprisal-held bot first;
exit lead 1.2×; the swap deferral covers any Guardian on a raider, fresh or slipped (judged by tank deaths within 10 s
of a deferral). Diminish Power: a bot tank breaks a Crusher's channel once bots carrying its stack are below, without
waiting for the rest of the team (Q2, 2026-11-24, one window; a straggler takes the re-cast down). Cloud geometry
(Q1, 2026-11-24): re-derive phase 1 positions from the measured orbits in two steps, the reader first (r22), then one
navprobed position change per round, the melee core first. Left alone by choice: the shaman's Reincarnation onto the
stack, late Guardians woken by the back line, the raid working the next Guardian while a low one waits out its gap.

**Open questions.** None.

## Round 22 (2026-11-24, window 33 build 9ad197722: round 21)

**Data.** Window report `scratchpad/window33/window33/window33-yogg-saron.md`: six valid pulls, one per pool, 0
humans. Traces `603_1_yogg-saron_1795545779`, `603_2_…1795545789`, `603_3_…1795545789`, `603_4_…1795545789`,
`603_5_…1795545794`, `603_6_…1795545780` (uld1-uld6), read with `--threat --clouds --novas --crush --brain
--tentacles --phase3`; a scratch count of `yogg-saron phase 1 aoe hold multiplier` vetoes by role.

Deaths by mechanic (phase 1 / 2 / 3), all six wiped in phase 3:

- uld1 (A): — / Curse of Doom 3, Draining Poison 1, Squeeze 2, Death Ray 1 / Immortal Guardian 20, Draining Poison 2,
  Roar 1; Yogg 27.9%.
- uld2 (B): — / Death Ray 2 / Immortal Guardian 29; Yogg 22.6%.
- uld3 (C): nova 4 (a chain at 2:02.7 / 2:06.6 / 2:08.65 killed three, the later two Guardians at 9.3% and 9.6%; a
  Fervor-doubled 40.0k at 2:17.4) / Death Ray 1 / Immortal Guardian 25, Roar 1, Gaze 1; Yogg 26.0%.
- uld4 (A): nova 2 (the shaman Ulduai) / skull Gaze 1, Death Ray 1 / Immortal Guardian 27, Drain Life 1, Gaze 1;
  Yogg 11.7%.
- uld5 (B): — / Brain Link 1 / Immortal Guardian 30; Yogg 25.5%.
- uld6 (C): nova 3 (the shaman Uldwaj, rezzed twice) / Death Ray 1, Curse of Doom 1 / Immortal Guardian 24, Draining
  Poison 1, Gaze 2; Yogg 24.9%.

**Verdict on round 21 (tanks keep their area threat): fixed**, per composition. Turns off a tank within 10 yd of the
middle: A 3 / 1, B 12 / 10, C 5 / 6, 2.5 a minute in all (window 31: 5.7; windows 19-28: 2.7). No hold veto on a tank:
1078 vetoes, all ranged and melee. Gap loss 2.2-3.1%/s with Maul, Swipe, Consecration and Thunder Clap back in `area
run`. Pair nova deaths: one chain in uld3 (3), as round 21's uld3 pair, its later Guardians under 10% and beyond any
gap. Guardians alive at the ring 4 / 3 / 1 / 2 / 1 / 3. Dark Volley 0 (21). B's turns stay above 3.4 a minute (a
lead).

**Stop rules.** No kill. Phase 3 stays stuck: every Guardian leaves its tank at stasis end (+4.0 s), known since
rounds 7 and 17; no new root cause. Yogg's end tracks the phase 2 leftover stock (uld4: 3 tentacles, 1.8M, 11.7%;
the rest 5-10, 4.7-8.3M, 22.6-27.9%), which the phase 2 cycle rate sets.

**Picked: the user's answers Q1 (step 1) and Q2.**

**Q1 step 1, the reader on the measured orbits.** `CLOUD_ORBITS` 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 with
`CLOUD_SPEED` 2.0 (`yogg_saron.py:122-123`). Every spawn now gets an orbit (uld1 had 12 of 17 `no cloud`); `--clouds`
labels phase 1 samples from snapshots, not the code's `yogg.cloudreach`; `PILE_LEASH` reads 6.8; Sara's-timer
orbits are those under 20 yd net of 2.5 reach (orbits 1 and 2); `P1_ROOM_RADIUS` stays the code's 69.3. New `the
station's cost by orbit` (passes of a cloud within 8.5 yd of a back-line bot within 5 yd of the station spot,
Guardians it woke with one at the mark, cloud dodges by bots there). Window 33:

| Pull | orbit 1 woke / passes | orbit 2 | orbit 3 | station of all | cloud steps there |
|---|---|---|---|---|---|
| uld1 | 2 / 3 | 2 / 2 | 1 / 1 | 5 of 17 | 12 (11 orbit 1) |
| uld2 | 2 / 3 | 2 / 3 | 1 / 1 | 5 of 16 | 14 (orbit 2) |
| uld3 | 2 / 3 | 2 / 2 | 1 / 1 | 5 of 14 | 15 (13 orbit 2) |
| uld4 | 2 / 3 | 2 / 2 | 1 / 1 | 5 of 15 | 14 (orbit 2) |
| uld5 | 2 / 4 | 2 / 2 | 1 / 1 | 5 of 14 | 17 (14 orbit 2) |
| uld6 | 2 / 3 | 1 / 3 | 1 / 1 | 4 of 16 | 3 |

So the station's three orbits each wake about one Guardian a pass, orbit 3 included; one pass a pull sends the whole
back line stepping. Next round (Q1 step 2) makes the first navprobed position change, the melee core first.

**Q2, the break per carrier.** `YoggSaronPowerBreakClaim` (`UldEncounter_YoggSaron.cpp:3026-3036`) no longer calls
`YoggSaronBrainTeamDown` (deleted): a Crusher channelling with its `64145` carried under Illusion Room is claimable
at once. Baseline from the new `--crush` column `carried below N s before` (`carrier_wait`): 27 claims, medians per
pull 7.6 / 2.0 / 1.7 / 9.5 / 0.3 / 10.4 s, 11 of 27 waited 9.5-16.9 s (stragglers); Crushes on a breaking tank 0.
`--brain` carried below where carried: medians 0.5-26.5 s, damage dealers ×0.73-1.00 to the door. Same code at 10 and
25, normal and hard mode; breakers stay bot tanks, carriers any group member, so a real player in the team changes
nothing. No shared code.

**Next window should show:** `--clouds` station cost 4-5 a pull again, no `no cloud` spawn. `--crush` every claim
`carried below` under ~1 s; `--brain` carried medians mostly under ~10 s; more claims per wave where a straggler
takes the re-cast down; Crushes on a breaking tank 0 and no Crush death within 10 s of a break; Brain per cycle up
and fewer tentacles at phase 3. Back out if breaking tanks take Crushes or die, or claims pull a tank off its job
more than ~3 times a wave.

**Seen, not picked.**

- Some claims walk 13-26 s and the channel drops only at the stun (uld2 8:54.1 +18.9 s, uld6 7:31.7 +26.0 s).
- uld3's chain: the later two Guardians were under 10% when the first died, as round 21's pair.
- The enhancement shaman died to one nova from full 5 times (uld4, uld6).
