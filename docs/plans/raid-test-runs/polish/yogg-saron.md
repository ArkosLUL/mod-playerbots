# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases` and `--burn`; the room gate is `--room`), tests in
`tools/botobs/tests/test_botobs.py` (`YoggPhases`); rounds 1-26 in `history/yogg-saron.md`. Window 35 is the first
on the professions rosters (all eleven at 450, perks and profession enchants), so trends across window 35 and
earlier compare unlike rosters.

**Fight model.**

- **Phase 1** (129-152 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s (the reader since
  r22; the code and `yogg.cloudreach` still hold the old 11.6-60.8 at 3.0, Q1 step 2). The 21.5 yd station sits in
  reach of orbits 1-3 and wakes 4-6 of a phase 1's 12-16 Guardians. Bot tanks pull Guardians onto the stack (6.5 yd),
  one focus per instance, melee stand in it and eat every nova, the back line holds the station. Sara needs ~11 deaths
  inside 15 yd. A nova gap (6 s after any death, a Guardian under 30% is not killable), a chain rule, a non-tank AoE
  hold and a free low Guardian taking the focus back (r23) space the novas. What kills: two low Guardians side by
  side, one nova from full on a bar of 21.5-23.9k (enhancement shaman, rogues, a warrior), bars left at 68-70%, and
  Guardians piling up off the stack. Melee lost here shrink every brain team and the platform after.
- **Phase 2** (275-357 s). A portal wave every 80 s; the brain team (10, melee first, exactly one healer) clears an
  illusion room, then hits the Brain until Induce Madness at 60 s, holding at 32% while a tentacle lives and
  finishing at 8 s left whatever the platform holds. The platform never clears a wave (stock grows each cycle), so
  phase 3 opens mid-stun with 1-4 tentacles (Crusher 2.69M, Corruptor 0.81M).
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies; one spawns every 10 s at ~40 yd from Yogg
  and leaves stasis at +4 s onto a fixed melee unless a taunt or hit comes first (r27: a hunter's misdirected shot
  opens it on a tank; a bot hunter stands in reach of a third of spawns at spawn, half before a tank holds them).
  Ranged stop at 30% (the user's floor), so only tank, area and gaze-time melee damage takes a Guardian to
  Weakened. Phase 3 ranged rank a Crusher
  tier 0 (above Guardians): with 3-4 leftovers they spend 60-120 s on tentacles at ~6.5k/s a bot under Diminish
  Power, no Guardian reaches Weakened, and Yogg's first minute is 5.9-6.8% (7.7-8.9% with 1-2). Shadow Beacon every
  45 s marks 3 Guardians that are dragged to lanes (tank first, then DKs / paladins, hunters, bears). Under 12%
  ranged burn Yogg (r25): his fall goes from 0.11-0.14 to 0.14-0.19%/s; the tanks then last 4-58 s, so pulls end at
  1.8-11.3%.

**Compositions on 229335cd3 (window 44 and filler 45, two pulls per pool).** No kill; nothing meets the done rule.

| Design | Pools | Outcome: Yogg's end, deaths (leftovers at phase 3) |
|---|---|---|
| A | uld1, uld4 | uld1 7.74%, 32 (2) / 2.17%, 32 (2); uld4 21.64%, 32 (3) / 3.03%, 32 (2) |
| B | uld2, uld5 | uld2 23.72%, 31 (4) / 11.81%, 30 (2); uld5 3.31%, 31 (1) / 13.54%, 31 (2) |
| C | uld3, uld6 | uld3 2.80%, 32 (2) / 15.69%, 32 (2); uld6 11.30%, 32 (3) / 1.83%, 32 (1) |

**Composition differences** (22 pulls on windows 35-37; designs per the roster table, 25-man fields 2 tanks, 4
healers, 19 DPS):

1. **B loses its melee in phase 1** more often (4 of 8 pulls lost 3-6 melee in one nova tick; C 1, A 1). No
   composition cause found.
2. **B's tanks (prot warrior + bear) are the weakest phase 3 pair**: two 30 yd single taunts, no Righteous Defense
   or Death Grip; fresh Guardians on a raider a median 1.2-2.2 s a pull on 229335cd3 (A 0.7-1.6, C 0.3-0.4);
   8-10k/s on the pile. r24's Vigilance helps on its own target.
3. **B's and C's ranged deal less in phase 3** (median 79k/s and 87k/s against A's 111k/s). Not root-caused.
4. **C is closest**: its bear (Growl only) is the weak half.

**Open failures, ranked (229335cd3, 12 pulls).**

1. Phase 3 Immortal Guardians, 350: every pull. Stuck as a throughput race (rounds 3-5, 17, 24-25). r26 takes the
   collapse that came at a beacon both tanks dragged (4 of 38 marks, all in window 44); r27 (Q4) the fresh
   Guardians: 111 of 179 sat on a raider before a tank held them, killing 15 raiders before the first tank death.
2. Phase 3 leftovers (1-4 a pull): 4 of 12 pulls never crossed 12% (uld2, uld4 of window 44; uld3 15.69%, uld5
   13.54% with 2 each), uld6 crossed at +198 s. uld4's Crusher (87%) took 83 s at 28k/s with all 9 ranged on it
   from 34.5 yd, Diminish Power on 55% of phase 3.
3. Phase 3 opener: both tanks on platform Corruptors 42-45 yd out at the Brain's 30%, the first Guardian killed a
   melee and a healer in +4.5-7.4 s (uld5, uld6); uld5's warrior took 15 s to the spot.
4. Shadow Nova 14 (uld3 4 in filler 45, three on one melee), each one melee with nothing up.
5. Death Ray 5, Lunatic Gaze 4, Captured Mercenary Soldier 2, Curse of Doom 1, Drain Life 1, Hand of Sacrifice 1,
   Sara's Blessing 1.

Leads, not picked: a chain-held Guardian on the stack waits on a lower one parked under the 35% floor while both
bleed (window 37); the DK tank's Death Grip, the warrior's Challenging Shout and the bear's Challenging Roar are held
by the phase 3 taunt hold and never cast by the control; Death Ray victims on a `phase 2 spacing action` walk, a
Fervor tank's run dragging its Guardians off the stack, `p1station`'s band test on `GetDistance2d`,
`YoggSaronCloudLead` projecting at run speed against 2.0 yd/s clouds (round 19); the pet fallback sends a pet into
a swinging Crusher's reach (round 17); Crusher reach 4.5 since `2026_11_06_01`, the reader and doc say 8 (round
18); Army of the Dead ghouls wake phase 3 Guardians; a Diminish Power claim has no timeout, and no tank breaks a
channel in phase 3; casts finishing on a Guardian gone unkillable (round 20); back-line Guardians the DK's Dark
Command cannot reach (round 21); a phase 3 tank keeps its platform target and chases back to it while its forced
walk to the spot reads `wait` (uld5 7:07-7:13); hunters all stand east, so a misdirected opener reaches only spawns
on that side (stationing them apart is a position change); the control still spends a taunt on a Guardian a hunter
has claimed.

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
- r16 a bot tank breaks Diminish Power (user (b)): kept, 0-2 Crushes on a breaking tank a window, none lethal.
- r17 chain taunt at stasis end: fixed. Burst on phase 3 adds (shared `BurstCooldowns`): fixed on its own check.
- r18 swap deferral for a Guardian on a raider (user): kept, no tank death. Phase 3 melee held at the spot: fixed.
- r19 back-line dodges stay outside the band: fixed (0 inside, 0 back-line nova hits in windows 29-37).
- r20 AoE hold by `HitsNeighbours`, tank's stack attack killable first: fixed on its target for non-tanks.
- r21 tanks keep their area threat under the phase 1 and leftover hold: fixed (turns 2.5 a minute, Dark Volley 0).
- r22 reader on the measured orbits and the station's cost per orbit (Q1 step 1): reader.
- r22 Diminish Power break once that Crusher's stack is carried below (Q2): kept (window 35).
- r23 a free low Guardian takes the phase 1 focus back outside the handover too: kept (window 37).
- r24 a warrior tank keeps Vigilance on the other tank in phase 3: kept (window 39).
- r25 phase 3 ranged drop the Guardians for Yogg under 12% (Q3): kept (12 pulls: ranged 0.92-0.99 on Yogg after
  it, his fall 0.14-0.19%/s, ends 1.8-7.7% in six of its eight crossings; no kill, 0.25%/s not met).
- r26 at most living tanks − 1 bot tanks drag a beacon mark: pending (no pull yet).
- r27 a hunter Misdirects each fresh Guardian onto a tank (Q4): pending.

**Tried and rejected.**

- Leaving a beacon-healed Guardian alone in phase 3 (r3): user declined, too risky.
- Round 9's join of any live tentacle: made cycles worse (joiners ended 53-85 yd out); narrowed in r12.
- Phase 3 ranged on the lowest leftover Crusher only (r7): user wants phase 3 to start with at most one Crusher.
- Ranged-first brain team (r8): user keeps melee first.
- Healer under 30% mana sent after the others (r13): withdrawn in r15, no case seen.
- Melee stepping out of the phase 1 nova (findings doc): late on 44 of 74, woke Guardians.
- Holding the tanks' area damage in the phase 1 hold (r20): Guardians on the stack turned off a tank 5.7 a minute,
  was 2.7-3.4; backed out for tanks in r21.

**User answers in force.** Keepers: Freya only. Phase 3: no leave-alone for healed Guardians; ranged take Guardians
to 30% and keep DoTs (Flame Shock included) on Yogg; a bot tank taunts a fresh Guardian on a non-tank before a swap
while the holder is over 35%. Phase 2 goal: start phase 3 with at most one Crusher. Brain team melee first, exactly
one healer; a finished bot waits at the stage unless a live tentacle stands within 40 yd of it; the room healer heals
a reprisal-held bot first; exit lead 1.2×; the swap deferral covers any Guardian on a raider, fresh or slipped (judged
by tank deaths within 10 s of a deferral). Diminish Power: a bot tank breaks a Crusher's channel once bots carrying
its stack are below, without waiting for the rest of the team (Q2, 2026-11-24). Cloud geometry (Q1, 2026-11-24):
re-derive phase 1 positions from the measured orbits, one navprobed position change per round, the melee core first,
picked only when it ranks first. Phase 3 burn (Q3, 2026-11-25, built in r25): once Yogg is under 12%, ranged leave
the Guardians and put everything on him; judged by Yogg's end and the time from 12% to the tank deaths.
Misdirection on fresh Guardians (Q4, 2026-11-25, yes, for one window since B's fresh Guardians sat on a raider a
median 1.5-2.0 s in window 44, built in r27): a hunter puts Misdirection on a tank and shoots each fresh Guardian at
its spawn, so it opens on the tank. Two hunters give 4 a minute against 6 spawns. Risk: a tank that leads by only
a few shots can still lose the Guardian to the healers' threat.
Left alone by choice: the shaman's Reincarnation onto the stack, late Guardians woken by the back line, the raid
working the next Guardian while a low one waits out its gap.

**Open questions.** None.

## Round 27 (2026-11-25, filler 45 on build 229335cd3: round 25; builds Q4)

**Data.** Reports `scratchpad/filler45/filler45-yogg-saron.md` (window 44's six pulls and six more on the same build)
and `scratchpad/window44/window44/window44-yogg-saron.md`. New traces `603_22…1795600849`, `603_27…1795601345`,
`603_25…1795601264`, `603_23…1795601183`, `603_24…1795601193`, `603_26…1795601325` (uld1, uld4, uld2, uld5, uld3,
uld6); all twelve with `--phase3`; a scratch read of every phase 3 spawn against the bot hunters' positions. No
pull on r26's code yet.

Deaths by mechanic, the six new pulls (phase 1 / 2 / 3):

- uld1 (A): — / Death Ray 1 / Immortal Guardian 31; Yogg 2.17%.
- uld4 (A): nova 1, Sara's Blessing 1 / — / Immortal Guardian 30; Yogg 3.03%.
- uld2 (B): — / — / Immortal Guardian 29, Hand of Sacrifice 1; Yogg 11.81%.
- uld5 (B): nova 1 / — / Immortal Guardian 29, Gaze 1; Yogg 13.54%.
- uld3 (C): nova 4 (Uldzaj three times) / Death Ray 1 / Immortal Guardian 27; Yogg 15.69%.
- uld6 (C): nova 1 / — / Immortal Guardian 31; Yogg 1.83%.

**Verdicts.** r26 (at most living tanks − 1 bot tanks drag a mark): pending, no pull. Its baseline on r25's code,
12 pulls: `every tank dragging` 4 of 38 marks (uld1, uld2, uld3, uld5 of window 44, none in the six new), raiders
dead inside them 12 (uld2 8, uld5 4). r25 (burn under 12%), six more pulls: uld1, uld4, uld6 crossed, his fall
0.12-0.14 → 0.17-0.18%/s, ends 2.17 / 3.03 / 1.83% with the tanks dead +40-58 s after; uld2 crossed only once both
tanks were dead, uld3 and uld5 never (2 leftovers each). Kept; no kill.

**Stop rules.** Not polished: no kill. Phase 3 is still the stuck race; this round builds the user's Q4.

**Picked: Q4, a hunter Misdirects each fresh Guardian onto a tank.** Baseline over the 12 pulls (`--phase3`), up to
each first tank death: 111 of 179 Guardians sat on a raider before a tank first held them (median a pull A 0.7-1.6
s, B 1.2-2.2, C 0.3-0.4), first victim a tank 67 of 179, raiders killed by a fresh Guardian 15 (7 in uld2's
both-tank drag); `first off a tank` a median +4.0-4.3 s, 5-17 of them to melee a pull. The generic `misdirection on
main tank` cast 5-13 a phase 3, all on one tank, proccing on whatever the hunter hit.

Mechanics read. `boss_yoggsaron_immortal_guardian::Reset` (`boss_yoggsaron.cpp:1978`) casts Recently Spawned 64497
(DBC duration 8: 15 s) and roots it for 4 s; `JustEngagedWith` ends the stasis with `SetInCombatWithZone`.
`ThreatManager::AddThreat` (`ThreatManager.cpp:456-482`) books the redirect share through a nested `AddThreat`
before the attacker's own, so the tank's reference is created first, `UpdateVictim` picks it and
`JustStartedThreateningMe` engages the Guardian on it; a spell hit's `SetInCombatWith` (`Spell.cpp:2698`) is combat
without a threat reference. 34477: 30 s cooldown, 1.5 s GCD; its proc 35079 lasts 4 s (duration 35). Arcane Shot
instant (cast index 18 plus the ranged 500 ms stays under 0), 6 s cooldown, 40 yd/s.

**Fix.** `yogg-saron guardian misdirect` (`ACTION_RAID + 2`, `YoggSaronMisdirectGuardian` in
`UldEncounter_YoggSaron.cpp` after `YoggSaronVigilanceTarget`): a bot hunter not dragging, staged or returning claims the oldest fresh Guardian
(64497 up, unmarked, holdable, on no tank or on one leading by 1.5× at most) within 34 yd in sight, one hunter a
Guardian (`yogg.misdirect`, 8 s at most); with Misdirection ready it casts it on the tank with the most Guardians,
then the healthiest, never one shedding a swap or under `YoggSaronTankLowHold`, then shoots (Arcane, Chimera, Aimed,
Multi-Shot, Steady) only where the shot lands inside the redirect (`YoggSaronMisdirectShotLands`: 34477 unspent, or
35079's time left over cast + flight + 0.5 s), else the Guardian would open on the hunter. It keeps shooting while
35079 lasts, not under the burn. During a gaze it acts only on a Guardian already inside its 120° cast arc
(`yogg.mdshot` `gaze` otherwise; review fix: a shot turns the bot to its target, possibly into Yogg).
`yogg-saron misdirection hold` blocks `misdirection on main tank` in phase 3.
`…_DISTRACTING_SHOT_RANGE` renamed `…_HUNTER_SHOT_RANGE`. Any tank, humans included; 10 and 25, normal and hard
mode alike. No shared code. Reader: `--phase3` tags `misdirected +s` per Guardian and prints `misdirected` (claims,
shots, inside the stasis, first victim a tank; time on a raider and deaths, misdirected against the rest);
`misdirected_openers` tested.

**Reach.** The two bot hunters stood within 35 yd of 57 of 179 spawns at the spawn, 68 inside the stasis, 84 before
a tank first held them: expect a third to a half of spawns opened, under the 4 of 6 the cooldown allows.

**Next window must show, each judged apart** (r26 acts only between a beacon claim and its release, r27 only on
Guardians under 15 s old):

- r27: `misdirected` shots near the in-reach spawns, first victim a tank on nearly every shot, inside-stasis shots
  opening at the shot rather than +4.0 s; misdirected Guardians' time on a raider near 0 against the rest; `raiders
  killed by a misdirected one` 0; `first off a tank` to ranged not rising. Back out if a misdirected Guardian's first
  victim is the hunter or it flips to ranged within seconds of the opener.
- r26: `dragged by` at most one tank a mark, `every tank dragging` 0 (baseline 4 of 38), raiders dead inside a drag
  0-1 a mark. Deaths inside a drag window are r26's; deaths to a Guardian no tank had held yet are r27's.

**Seen, not picked.** Hunters all stand east, so reach caps r27; stationing them apart is a position change, after
this window. The control still spends a taunt on a Guardian a hunter has claimed.

