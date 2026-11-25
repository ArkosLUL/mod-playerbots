# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases` and `--burn`; the room gate is `--room`), tests in
`tools/botobs/tests/test_botobs.py` (`YoggPhases`); rounds 1-28 in `history/yogg-saron.md`. Window 35 is the first
on the professions rosters (all eleven at 450, perks and profession enchants), so trends across window 35 and
earlier compare unlike rosters.

**Fight model.**

- **Phase 1** (129-156 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s (the reader since
  r22; the code and `yogg.cloudreach` still hold the old 11.6-60.8 at 3.0, Q1 step 2). The 21.5 yd station sits in
  reach of orbits 1-3 and wakes 4-6 of a phase 1's 12-17 Guardians. Bot tanks pull Guardians onto the stack (6.5 yd),
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
  and leaves stasis at +4 s onto a fixed melee unless a taunt or hit comes first. Ranged stop at 30% (the user's
  floor), so only tank, area and gaze-time melee damage takes a Guardian to Weakened. Phase 3 ranged rank a Crusher
  tier 0 (above Guardians): with 3-4 leftovers they spend 60-120 s on tentacles at ~6.5k/s a bot under Diminish
  Power, no Guardian reaches Weakened, and Yogg's first minute is 5.9-6.8% (7.7-8.9% with 1-2). Shadow Beacon every
  45 s marks 3 Guardians that are dragged to lanes (one tank, then DKs / paladins, hunters, bears). A DK's second
  Army of the Dead lands +107-127 s in (18 of 24 pulls on windows 44-49, two more at +170-180 s) and its ghouls hold
  9-14 Guardians for their 40 s, through the third beacon. Under 12% ranged burn Yogg (r25): his fall goes from
  0.09-0.14 to 0.13-0.19%/s; the race is then the tanks' life against ~70 s to the kill. Tanks die under the pile:
  bot tanks faced only their target, leaving live Guardians behind them 38% of their time (45% in gazes), where a
  player gets no dodge, parry or block and each landed twice as often (r29).

**Compositions on 06854755f (window 49, one pull per pool, r28).** C meets the done rule (uld3 kill, 1 death).

| Design | Pools | Outcome: Yogg's end, deaths (leftovers at phase 3) |
|---|---|---|
| A | uld1, uld4 | uld1 1.78%, 32 (1); uld4 7.90%, 33 (3) |
| B | uld2, uld5 | uld2 5.78%, 30 (2); uld5 20.63%, 31 (3) |
| C | uld3, uld6 | uld3 kill, 1; uld6 kill, 7 |

Phase 3 to 12%: C 124-129 s, A 140-172 s, B 174 s (uld5 never). Tank deaths after 12%: uld1 +56/+57 s, uld2
+22/+39, uld4 +19/+21, uld6 +48 (kill at +63), uld3 none. Window 48 (r26-r27) had no kill.

**Composition differences** (22 pulls on windows 35-37; designs per the roster table, 25-man fields 2 tanks, 4
healers, 19 DPS):

1. **B loses its melee in phase 1** more often (4 of 8 pulls lost 3-6 melee in one nova tick; C 1, A 1). No
   composition cause found.
2. **B's tanks (prot warrior + bear) are the weakest phase 3 pair**: two 30 yd single taunts, no Righteous Defense
   or Death Grip; fresh Guardians on a raider a median 1.2-2.2 s a pull on 229335cd3 (A 0.7-1.6, C 0.3-0.4);
   8-10k/s on the pile. r24's Vigilance helps on its own target.
3. **B's and C's ranged deal less in phase 3** (median 79k/s and 87k/s against A's 111k/s). Not root-caused.
4. **C is closest**: its bear (Growl only) is the weak half.

**Open failures, ranked (06854755f, 6 pulls).**

1. Phase 3 Immortal Guardians, 121 in window 49: every pull. Stuck as a throughput race (rounds 3-5, 17, 24-25);
   r29 takes the tanks' facing. The pile on one tank reaches 8-14 after 12% and kills it 19-57 s in.
2. Phase 3 leftovers (1-3 a pull): A and B reach 12% 10-50 s after C; uld5 (3, a Crusher and two Corruptors, one
   still up) lost both tanks at its second beacon, its two marks inside 20 yd of Yogg on the pile (7 others
   healed).
3. Phase 3 opener: both tanks on platform Corruptors 42-45 yd out at the Brain's 30%, the first Guardian killed a
   melee and a healer in +4.5-7.4 s (uld5, uld6 of filler 45); uld5 again lost 3 in +16-18 s.
4. Shadow Nova 6 in window 49 (3 of 6 pulls; one enhancement shaman 3 times in uld4).
5. Death Ray 3, Drain Life 3, Squeeze 1.

Leads, not picked: Army of the Dead ghouls wake phase 3 Guardians and drop 9-14 at once when they expire (holding
it in phase 3 would be a plan change); a chain-held Guardian on the stack waits on a lower one parked under the 35%
floor while both bleed (window 37); the DK tank's Death Grip, the warrior's Challenging Shout and the bear's
Challenging Roar are held by the phase 3 taunt hold and never cast by the control; Death Ray victims on a `phase 2
spacing action` walk, a Fervor tank's run dragging its Guardians off the stack, `p1station`'s band test on
`GetDistance2d`, `YoggSaronCloudLead` projecting at run speed against 2.0 yd/s clouds (round 19); the pet fallback
sends a pet into a swinging Crusher's reach (round 17); Crusher reach 4.5 since `2026_11_06_01`, the reader and doc
say 8 (round 18); a Diminish Power claim has no timeout, and no tank breaks a channel in phase 3; casts finishing on
a Guardian gone unkillable (round 20); back-line Guardians the DK's Dark Command cannot reach (round 21); a phase 3
tank keeps its platform target and chases back to it while its forced walk to the spot reads `wait` (uld5 7:07-7:13);
r29 retargets for facing only while every lead is past 1.5, so a thin-lead target still caps the pile in front at
~73% out of gazes (any target 87%).

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
- r26 at most living tanks − 1 bot tanks drag a beacon mark: fixed (window 48: every tank dragging 0 of 19 marks,
  was 4 of 38).
- r28 no dragger whose pet or summon an unmarked Guardian is hitting: fixed (window 49: pets held 0 of 20 marks,
  the beacon after Army of the Dead reached 0-1 others, was a median 6).
- r29 a bot tank faces its pile (`yogg-saron tank pile facing`, gaze `pile`, `pile attack`): pending.

**Tried and rejected.**

- Leaving a beacon-healed Guardian alone in phase 3 (r3): user declined, too risky.
- Round 9's join of any live tentacle: made cycles worse (joiners ended 53-85 yd out); narrowed in r12.
- Phase 3 ranged on the lowest leftover Crusher only (r7): user wants phase 3 to start with at most one Crusher.
- Ranged-first brain team (r8): user keeps melee first.
- Healer under 30% mana sent after the others (r13): withdrawn in r15, no case seen.
- Melee stepping out of the phase 1 nova (findings doc): late on 44 of 74, woke Guardians.
- Holding the tanks' area damage in the phase 1 hold (r20): Guardians on the stack turned off a tank 5.7 a minute,
  was 2.7-3.4; backed out for tanks in r21.
- Hunters Misdirecting fresh Guardians onto a tank (r27, Q4, one window): backed out in r28; 10 of 43 shots inside
  the stasis, misdirected ones on a raider as often as the rest, 4 raiders killed by them.

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
Left alone by choice: the shaman's Reincarnation onto the stack, late Guardians woken by the back line, the raid
working the next Guardian while a low one waits out its gap.

**Open questions.** None.

## Round 29 (2026-11-25, window 49 on build 06854755f: round 28)

**Data.** Report `scratchpad/window49/window49/window49-yogg-saron.md` (six pulls). Traces `603_11…1795610260`,
`603_9…1795610228`, `603_7…1795610208`, `603_8…1795610218`, `603_12…1795610273`, `603_10…1795610238` (uld1, uld4,
uld2, uld5, uld3, uld6), all with `--phase3` and the new `--facing`; scratch reads of phase 3 Army of the Dead casts
and a replay of tank headings over the snapshots.

Deaths by mechanic (phase 1 / 2 / 3):

- uld1 (A): nova 1 / Death Ray 1, Squeeze 1 / Immortal Guardian 28, Drain Life 1; Yogg 1.78%.
- uld4 (A): nova 3 (one enhancement shaman) / — / Immortal Guardian 29, Drain Life 1; Yogg 7.90%.
- uld2 (B): — / — / Immortal Guardian 29, Drain Life 1; Yogg 5.78%.
- uld5 (B): nova 2 / Death Ray 1 / Immortal Guardian 28; Yogg 20.63% (both tanks dead at the second beacon).
- uld3 (C): — / — / Immortal Guardian 1; kill.
- uld6 (C): — / Death Ray 1 / Immortal Guardian 6 (both tanks at 10:02, kill at 10:17).

**Verdicts.**

- r28 (no dragger whose pets hold Guardians): fixed. `on their pets at the mark` none on all 20 marks, `a dragger's
  pets held Guardians` 0. Phase 3 Army of the Dead at +107-118 s in four pulls (uld6 +180 s, uld5 none); the beacon
  after it reached 0 other Guardians in uld1, uld4, uld3 and uld6 and 1 (100 → 94%) in uld2, was a median 6. No mark
  undragged. First tank death after 12%: +19-56 s (uld3 none), window 48 +10-23 s. Kept.
- r27's back-out: fresh on a raider medians A 0.5, 1.1 s; B 1.9, 2.1; C 0.3, 0.3, back in r25's range. Raiders
  killed by a fresh Guardian 2 in uld1 (11:20-11:22, in the collapse) and 2 in uld5, 0 elsewhere.

**Stop rules.** Not polished: C has its kill (uld3, 1 death; Freya-only mask), A and B none. Not stuck: phase 3
Guardians stay the top failure, but r29 picks a new mechanism.

**Picked: Guardians behind the tanks.** `Unit::RollMeleeOutcomeAgainst` (core `Unit.cpp:3068-3106`) rolls no
dodge for a player hit from behind and no parry or block for anyone. `--facing`, six pulls to the first tank death,
live (sampled 10%+, not Weakened, as the code counts them) Guardians within 12 yd of their tank:

| | behind, share of Guardian-s | landed a second behind / front | damage a Guardian-second behind / front |
|---|---|---|---|
| out of gazes | 38% (995 of 2624 s) | 0.32 / 0.17 | 3.9k / 2.0k |
| gazes | 45% (396 of 887 s) | 0.35 / 0.19 | 4.5k / 2.1k |

Every bot tank of every pull shows it (behind 24-54%); 0.32-0.35 a second sits near the 0.41 swing ceiling
(Guardians swing every 2.4 s). Cause: a tank faces only its target (`set facing` at ±45°, `AttackAction` at
±60°), control step 3 (`UldActions_YoggSaron.cpp` `ControlPhaseThree`) targets the thinnest lead, mostly the
newest Guardian from wherever it walked in, and in a gaze a tank with its target under 65° from Yogg faces straight
away (`away` / `tankfree`), which covers 39-41% of its pile. A replay of headings: keeping the target inside ±40°
covers 73% out of gazes (now 62%) and, with Yogg behind, 67% in gazes (now 55%); any target 87% / 75%; any
heading 89% / 76%. A bug: the strategy holds the pile on the tank and the tank throws its avoidance away.

**Fix.**

- `YoggSaronPile::Best` (`Util/UldYoggSaronPile.h`, native test `tools/nativetest/yogg_pile_test.cpp`): the heading
  putting the most bearings inside ±80°, target inside ±40°, Yogg at least 100° off when given, ties to a preferred
  heading.
- `yogg-saron tank pile facing` (`ACTION_RAID + 2.75`, returns false): a still phase 3 bot tank, not dragging,
  outside a gaze, turns there when it covers more live Guardians swinging at it (`yogg.pileface` before, after).
- Gaze: such a tank takes `pile` (target kept, Yogg behind) where a heading exists, turning again once its target
  sits past ±40° (review: the 0.1 rad tolerance alone let `set facing` turn it to the target, Yogg in front), else
  `tankfree` / `away` turn it to the pile's best heading with Yogg behind instead of straight away.
- Control step 3: while every Guardian in reach and the current one lead past 1.5, attack the one letting the tank
  face most of its pile (`yogg.tankhold` `pile attack`); a thin lead keeps the old rule.
- Reader `--facing` (tests `in_front`, `facing_split`), live at 10%+ like the code (review; was 1%+, 41% / 47%
  behind); boss doc: tanks in phase 3, gaze `pile`, probe table.

Same code at 10 and 25, normal and hard mode; a human tank is never turned. No shared code.

**Next window must show:**

- `--facing` per composition: behind under 28% out of gazes (now 38%) and under 38% in gazes (now 45%);
  `yogg.pileface` on every bot tank, `yogg.gaze` `pile` rows, some `pile attack`.
- Landed a second per side near window 49's (0.32 / 0.17): front rising toward behind means the turn bought nothing.
- First tank death after 12% later than window 49 per composition (A +19-56 s, B +22, C +48 or none).
- No threat cost: `first off a tank` with a cause other than `none` at most window 49's 3-5 a pull, fresh-on-raider
  medians as above. Back `pile attack` out if those rise.

**Seen, not picked.** uld5's second beacon marked two Guardians 17-18 yd from Yogg on the tank's pile; Uldvai's
Dark Command didn't take, the heal reached 7 others and both tanks died +8-10 s. uld4 lost both tanks within 10 s
of a swap deferral (holder at 74% and 48%), in the collapse after 12%. uld2's healers were at 2-17% mana (three of
four) by +180 s. A and B reach 12% 10-50 s after C (leftovers 1-3).
