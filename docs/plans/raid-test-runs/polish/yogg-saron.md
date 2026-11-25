# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases` and `--burn`; the room gate is `--room`), tests in
`tools/botobs/tests/test_botobs.py` (`YoggPhases`); rounds 1-27 in `history/yogg-saron.md`. Window 35 is the first
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
  Army of the Dead lands +111-127 s in (14 of 18 pulls on windows 44-48, one more at +170 s) and its ghouls hold 9-14
  Guardians for their 40 s, through the third beacon. Under 12% ranged burn Yogg (r25): his fall goes from 0.10-0.14 to 0.14-0.19%/s;
  the tanks then last 4-58 s, so pulls end at 1.8-11.3%.

**Compositions on d9964f052 (window 48, one pull per pool, r26 and r27).** No kill; nothing meets the done rule.

| Design | Pools | Outcome: Yogg's end, deaths (leftovers at phase 3) |
|---|---|---|
| A | uld1, uld4 | uld1 9.72%, 33 (3); uld4 10.00%, 33 (2) |
| B | uld2, uld5 | uld2 8.32%, 31 (3); uld5 15.30%, 31 (3) |
| C | uld3, uld6 | uld3 14.28%, 31 (4); uld6 7.25%, 32 (2) |

r25's code (229335cd3, 12 pulls) ended 1.83-23.72%, five pulls under 3.4%.

**Composition differences** (22 pulls on windows 35-37; designs per the roster table, 25-man fields 2 tanks, 4
healers, 19 DPS):

1. **B loses its melee in phase 1** more often (4 of 8 pulls lost 3-6 melee in one nova tick; C 1, A 1). No
   composition cause found.
2. **B's tanks (prot warrior + bear) are the weakest phase 3 pair**: two 30 yd single taunts, no Righteous Defense
   or Death Grip; fresh Guardians on a raider a median 1.2-2.2 s a pull on 229335cd3 (A 0.7-1.6, C 0.3-0.4);
   8-10k/s on the pile. r24's Vigilance helps on its own target.
3. **B's and C's ranged deal less in phase 3** (median 79k/s and 87k/s against A's 111k/s). Not root-caused.
4. **C is closest**: its bear (Growl only) is the weak half.

**Open failures, ranked (d9964f052, 6 pulls; 229335cd3, 12).**

1. Phase 3 Immortal Guardians, 178 in window 48: every pull. Stuck as a throughput race (rounds 3-5, 17, 24-25).
   The collapse near 12%: the beacon after Army of the Dead spread its heal to 6-12 Guardians in 4 of 6 pulls of
   window 48 (r28 takes it); then the ghouls' 40 s ends with the pile on the tanks.
2. Phase 3 leftovers (1-4 a pull): uld3 (4) and uld5 (3, Diminish Power on 23% of phase 3) never crossed 12%.
3. Phase 3 opener: both tanks on platform Corruptors 42-45 yd out at the Brain's 30%, the first Guardian killed a
   melee and a healer in +4.5-7.4 s (uld5, uld6 of filler 45).
4. Shadow Nova 7 in window 48 (5 of 6 pulls).
5. Squeeze 2, Sara's Blessing 1, Psychosis 1, Drain Life 1, a script kill 1.

Leads, not picked: Army of the Dead ghouls wake phase 3 Guardians and drop 9-14 at once when they expire (holding
it in phase 3 would be a plan change); a chain-held Guardian on the stack waits on a lower one parked under the 35%
floor while both bleed (window 37); the DK tank's Death Grip, the warrior's Challenging Shout and the bear's
Challenging Roar are held by the phase 3 taunt hold and never cast by the control; Death Ray victims on a `phase 2
spacing action` walk, a Fervor tank's run dragging its Guardians off the stack, `p1station`'s band test on
`GetDistance2d`, `YoggSaronCloudLead` projecting at run speed against 2.0 yd/s clouds (round 19); the pet fallback
sends a pet into a swinging Crusher's reach (round 17); Crusher reach 4.5 since `2026_11_06_01`, the reader and doc
say 8 (round 18); a Diminish Power claim has no timeout, and no tank breaks a channel in phase 3; casts finishing on
a Guardian gone unkillable (round 20); back-line Guardians the DK's Dark Command cannot reach (round 21); a phase 3
tank keeps its platform target and chases back to it while its forced walk to the spot reads `wait` (uld5 7:07-7:13).

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
- r28 no dragger whose pet or summon an unmarked Guardian is hitting: pending.

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

## Round 28 (2026-11-25, window 48 on build d9964f052: rounds 26 and 27)

**Data.** Reports `scratchpad/window48/window48/window48-yogg-saron.md` (six pulls, r26 and r27 together) and
`scratchpad/filler45/filler45-yogg-saron.md` (r25's twelve). Traces `603_1…1795607074`, `603_4…1795607080`,
`603_2…1795607074`, `603_5…1795607066`, `603_3…1795607080`, `603_6…1795607067` (uld1, uld4, uld2, uld5, uld3, uld6),
all eighteen with `--phase3`; scratch reads of each heal's marks, holders and lanes, and of Army of the Dead casts
with the Guardians on ghouls per 5 s.

Deaths by mechanic (phase 1 / 2 / 3):

- uld1 (A): nova 1 / Squeeze 1 / Immortal Guardian 31; Yogg 9.72%.
- uld4 (A): nova 1 / Squeeze 1 / Immortal Guardian 31; Yogg 10.00%.
- uld2 (B): nova 1 / — / Immortal Guardian 30; Yogg 8.32%.
- uld5 (B): nova 2 / — / Immortal Guardian 29; Yogg 15.30% (tank dead at +106 s, 3 leftovers).
- uld3 (C): nova 2, Sara's Blessing 1 / Psychosis 1 / Immortal Guardian 25, Drain Life 1, script 1; Yogg 14.28%.
- uld6 (C): — / — / Immortal Guardian 32; Yogg 7.25%.

**Verdicts, each judged on its own measures.**

- r26 (at most living tanks − 1 bot tanks drag): fixed. `every tank dragging` 0 of 19 marks (r25 4 of 38); raiders
  dead inside a drag 0-1 a mark while both tanks stood (uld2's 18 and uld5's 4 came after a tank death). Kept.
- r27 (hunters Misdirect fresh Guardians, Q4 for one window): missed every measure it named. Shots 43 of 105 spawns,
  10 inside the stasis, median +8.3 s after the spawn; first victim a tank 20 of 43 (33 shots came after the
  stasis). On a raider before a tank held them, misdirected against the rest: A 4/8 1.6 s, 3/7 2.4 s against 4/9 1.4 s,
  4/10 1.4 s; B 4/6 1.0 s, 3/6 1.7 s against 7/11 2.0 s, 2/3 3.1 s; C 3/6 1.0 s, 4/8 1.6 s against 6/10 0.9 s, 3/8
  0.3 s. Raiders killed by a misdirected one 4 (target 0). `first off a tank` to ranged 14 of 104, not rising (r25
  29 of 202). Fresh-Guardian medians rose for A (1.6, 2.4 s; r25 0.7-1.6) and C (1.0, 0.3 s; r25 0.3-0.4), raiders
  killed by a fresh Guardian 8 in 6 pulls (r25 8 in 11 besides its both-tank drag). Backed out: Q4's window is spent
  and it bought nothing.

**Stop rules.** Not polished: no kill. Not stuck: r28 picks a new mechanism.

**Picked: the beacon after Army of the Dead heals the ghouls' pile.** In window 48 that mark spread its heal to 12,
10, 6, 0 and 10 other Guardians (uld1, uld4, uld2, uld3, uld6; uld5's came after a tank death), with 9-14 Guardians
on the ghouls at the mark. The tanks died 12-23 s after the heal in uld1, uld4 and uld6 (uld1 10:43 → 11:04, 11:06;
uld6 10:46 → 10:58, 11:00), near Yogg's 12% crossing.

Root cause. A DK casts its second Army of the Dead +111-127 s into phase 3 (all six pulls; r25 9 of 12, one at
+170 s) and the ghouls take the pile off the tanks (the tanks held 0-5 at the heal). That DK is a backup dragger
and a stager: in 11 of the 12 marks with 9+ Guardians on ghouls it staged and dragged (r25 too: 603_22, 603_27,
603_25, 603_23, 603_24, 603_10; only 603_8's DK did not, heal reach 0). Its ghouls reached its lane and took the
mark back 4-9 s after its taunt (uld1 Uldxal: taunt +0.2 s, ghoul holds the mark at the north-east lane +4.1 s, `missed`), the pile
with them; heal reach a median 6 (0-12) over those 11. `YoggSaronBeaconDraggerOrder`
(`UldEncounter_YoggSaron.cpp:1594`) excludes a bot an unmarked Guardian is hitting, but read the ghoul as the
victim, never its owner.

**Fix.** The dragger order also skips a bot whose pet or summon an unmarked Guardian is hitting (the victim's
`GetCharmerOrOwnerGUID`), for claims and stage picks alike; hunter pets and an unholy ghoul are covered the same way.
10 and 25, normal and hard mode, humans as victims or owners alike. r27's code is reverted in full (action, trigger,
node, `misdirection on main tank` hold, ids and constants, reader section and tests); `…_HUNTER_SHOT_RANGE` is
`…_DISTRACTING_SHOT_RANGE` again. No shared code. Reader: `--phase3`'s `dragged by` adds `on their pets at the mark`
and the beacon block `a dragger's pets held Guardians` (`pet_piles`, tested); on window 48 it reads uld1 Uldxal 9,
uld6 Uldwam 11.

**Next window must show:**

- r28: `on their pets at the mark` none on every mark, `a dragger's pets held Guardians` 0; the beacon after a phase 3
  Army of the Dead reaching 0-2 other Guardians (was a median 6 over 11 drags); the tanks living past +20 s from the
  12% crossing (window 48 +10-23 s; r25's best +28-58 s). Back out if a mark goes undragged for want of draggers.
- r27's back-out: `fresh on a raider` medians back near r25's (A 0.7-1.6, B 1.2-2.2, C 0.3-0.4 s) and raiders
  killed by a fresh Guardian at most 1 a pull.

**Seen, not picked.** The ghouls still hold 9-14 Guardians for 40 s and drop them at once near the 12% crossing
(lead). uld5 lost a tank at the second heal with Diminish Power on 23% of phase 3 (open failure 2).
