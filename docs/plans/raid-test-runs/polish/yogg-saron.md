# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases` and `--burn`; the room gate is `--room`), tests in
`tools/botobs/tests/test_botobs.py` (`YoggPhases`); rounds 1-25 in `history/yogg-saron.md`. Window 35 is the first
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
  and leaves stasis at +4 s onto a fixed melee unless a taunt or hit comes first. Ranged stop at 30% (the user's
  floor), so only tank, area and gaze-time melee damage takes a Guardian to Weakened. Phase 3 ranged rank a Crusher
  tier 0 (above Guardians): with 3-4 leftovers they spend 60-120 s on tentacles at ~6.5k/s a bot under Diminish
  Power, no Guardian reaches Weakened, and Yogg's first minute is 5.9-6.8% (7.7-8.9% with 1-2). Shadow Beacon every
  45 s marks 3 Guardians that are dragged to lanes (tank first, then DKs / paladins, hunters, bears). Under 12%
  ranged burn Yogg (r25): his fall goes from 0.11-0.12 to 0.14-0.19%/s; the tanks then last 4-45 s, so pulls end at
  2.8-11.3%.

**Compositions on 229335cd3 (window 44, one pull per pool).** No kill; nothing meets the done rule.

| Design | Pools | Outcome (leftovers at phase 3) |
|---|---|---|
| A | uld1, uld4 | uld1 Yogg 7.74%, 32 deaths (2); uld4 21.64%, 32 (3) |
| B | uld2, uld5 | uld2 23.72%, 31 (4); uld5 3.31%, 31 (1) |
| C | uld3, uld6 | uld3 2.80%, 32 (2); uld6 11.30%, 32 (3) |

**Composition differences** (22 pulls on windows 35-37; designs per the roster table, 25-man fields 2 tanks, 4
healers, 19 DPS):

1. **B loses its melee in phase 1** more often (4 of 8 pulls lost 3-6 melee in one nova tick; C 1, A 1). No
   composition cause found.
2. **B's tanks (prot warrior + bear) are the weakest phase 3 pair**: two 30 yd single taunts, no Righteous Defense
   or Death Grip; fresh Guardians on a raider a median 1.5-2.0 s in window 44 (A 0.8-1.6, C 0.3-0.4); 8-10k/s on
   the pile. r24's Vigilance helps on its own target.
3. **B's and C's ranged deal less in phase 3** (median 79k/s and 87k/s against A's 111k/s). Not root-caused.
4. **C is closest**: its bear (Growl only) is the weak half.

**Open failures, ranked (window 44).**

1. Phase 3 Immortal Guardians, 173: every pull. Stuck as a throughput race (rounds 3-5, 17, 24-25). r26 takes the
   collapse that came at a beacon both tanks dragged (uld1, uld2, uld5; uld3 20 s later).
2. Phase 3 leftovers (uld2 4, uld4 3, uld6 3): uld2 and uld4 never crossed 12%, uld6 crossed at +198 s. uld4's
   Crusher (87%) took 83 s at 28k/s with all 9 ranged on it from 34.5 yd, Diminish Power on 55% of phase 3.
3. Phase 3 opener: both tanks on platform Corruptors 42-45 yd out at the Brain's 30%, the first Guardian killed a
   melee and a healer in +4.5-7.4 s (uld5, uld6); uld5's warrior took 15 s to the spot.
4. Shadow Nova 7 (uld4 3, uld1 2 one melee twice, uld2 1, uld3 1), each one melee with nothing up.
5. Death Ray 3, Lunatic Gaze 3, Captured Mercenary Soldier 2, Curse of Doom 1, Drain Life 1.

Leads, not picked: a chain-held Guardian on the stack waits on a lower one parked under the 35% floor while both
bleed (window 37); the DK tank's Death Grip, the warrior's Challenging Shout and the bear's Challenging Roar are held
by the phase 3 taunt hold and never cast by the control; Death Ray victims on a `phase 2 spacing action` walk, a
Fervor tank's run dragging its Guardians off the stack, `p1station`'s band test on `GetDistance2d`,
`YoggSaronCloudLead` projecting at run speed against 2.0 yd/s clouds (round 19); the pet fallback sends a pet into
a swinging Crusher's reach (round 17); Crusher reach 4.5 since `2026_11_06_01`, the reader and doc say 8 (round
18); Army of the Dead ghouls wake phase 3 Guardians; a Diminish Power claim has no timeout, and no tank breaks a
channel in phase 3; casts finishing on a Guardian gone unkillable (round 20); back-line Guardians the DK's Dark
Command cannot reach (round 21); a phase 3 tank keeps its platform target and chases back to it while its forced
walk to the spot reads `wait` (uld5 7:07-7:13).

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
- r25 phase 3 ranged drop the Guardians for Yogg under 12% (Q3): kept (window 44: ranged 0.98-0.99 on Yogg after
  it, his fall 0.14-0.19%/s, B and C end 3.3 / 2.8% against 9.4 / 9.7%; no kill, 0.25%/s not met).
- r26 at most living tanks − 1 bot tanks drag a beacon mark: pending.

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
median 1.5-2.0 s in window 44): a hunter puts Misdirection on a bot tank and shoots each fresh Guardian at its spawn,
so it opens on the tank. Build in r27. The case: a hit ends the 4 s stasis and the redirected threat makes the tank
its first victim; in window 44 most went to a melee at +4.0 s (first off a tank 5 / 14 of 8 / 20 to melee in B,
13 / 6 of 18 / 11 in A, 14 / 13 of 20 / 20 in C), and a warrior's or bear's 30 yd taunt reaches them only as they
walk in. Two hunters give 4 a minute against 6 spawns; hunters already cast Misdirection 5-13 times a phase 3, all
onto one tank. Risk: a tank that leads by only three shots can still lose the Guardian to the healers' threat.
Left alone by choice: the shaman's Reincarnation onto the stack, late Guardians woken by the back line, the raid
working the next Guardian while a low one waits out its gap.

**Open questions.** None.

## Round 26 (2026-11-25, window 44 build 229335cd3: round 25)

**Data.** Window report `scratchpad/window44/window44/window44-yogg-saron.md` (six pulls, one per pool). Traces
`603_8…1795599012`, `603_7…1795599001`, `603_10…1795599031`, `603_11…1795599039`, `603_9…1795599021`,
`603_12…1795599050` (uld1-uld6) with `--phase3 --burn --tentacles --brain`, uld1-4 `--novas`; window 39's six with
`--burn`; scratch reads of leftover targets and ranged damage per target, beacon claims over the 77 Yogg
traces on disk with drag notes.

Deaths by mechanic (phase 1 / 2 / 3):

- uld1 (A): nova 2 (one melee twice) / — / Immortal Guardian 30; Yogg 7.74%.
- uld4 (A): nova 3 / Death Ray 1 / Immortal Guardian 27, Gaze 1; Yogg 21.64%.
- uld2 (B): nova 1 / Death Ray 1 / Immortal Guardian 26, Mercenary Soldier 2, Gaze 1; Yogg 23.72%.
- uld5 (B): — / — / Immortal Guardian 31; Yogg 3.31%.
- uld3 (C): nova 1 / — / Immortal Guardian 31; Yogg 2.80%.
- uld6 (C): — / Curse of Doom 1, skull Gaze 1, Death Ray 1 / Immortal Guardian 28, Drain Life 1; Yogg 11.30%.

**Verdict on round 25 (the burn under 12%): kept**, on Q3's measures. Ranged on Yogg after the crossing 0.98-0.99
(0.01-0.25), `yogg.burn` on 6-10 bots within 3 s; his fall 0.14 / 0.15 / 0.19%/s (uld1 / uld5 / uld3; window 39
0.07-0.15). B (uld5) ended at 3.31% (9.41), tanks +20.6 / +45.5 s after it (+26.7); C (uld3) 2.80% (9.70), +28-30
(+11-20). A's uld1 ended higher, 7.74% (3.63), its first tank dying +7.5 s, 6.7 s after both tanks' beacon drag
(below); uld6 crossed only at +198 s behind three leftovers and lost both tanks +3.8 / +6.5 s into a collapse
already running. Not met: 0.25%/s, a kill. uld2 and uld4 never crossed (leftovers). Guardians at 90%+ after 21-40%
(14-25% before).

**Stop rules.** Not polished: no kill. Phase 3 stays stuck as a race; the both-tank drag is a new root cause.

**Picked: both tanks leave on a beacon drag** (inside phase 3's 173). `YoggSaronBeaconDraggerOrder`
(`UldEncounter_YoggSaron.cpp:1583`) admitted every bot tank while `tanksAlive > 1`, and a tank is free when only
marked Guardians hit it, so with the pile all marked both tanks claimed, the stagers too (uld1 10:31.8, uld5
10:08.2 picked both). The rule's intent, from its commit: "a bot tank only while another tank is left holding the
pile". Window 44: both tanks dragged in uld1 10:36.9, uld2 8:58.5, uld3 10:27.9, uld5 10:13.1. uld2: fresh G2334
(8:57.2) killed Uldyaq, Uldyas, Uldyaw, Uldyak, Uldyan, Uldyam 8:59.4-9:09.6 at 94-99%, 33-38 yd from both tanks;
8 raiders died in the 10.7 s drag, the tanks at 9:12.8 and 9:22.0. uld5: 7 raiders died inside 15 s of the claims, both tanks
+2.2 / +3.4 s after the heal. uld1: Uldxaa died 6.7 s after the release, the pull's first tank death. uld3: tanks
+20-22 s. The 71 older traces had 4 such drags; all 8 had a tank dead +1.9 to +21.9 s after the release. Drags by
one tank or none with no death in the 20 s before: a tank dead within 25 s in 38 of 120, raiders dead inside them
51 (0.4 a mark); inside a running collapse they cost more (uld4's 9:52 heal 7, uld3's 11:27 heal 13).

**Fix.** `YoggSaronBeaconDraggerOrder` counts living tanks and those already dragging, and keeps at most
`alive − dragging − 1` bot tanks (by GUID) at the head of the order, so the next marks go to non-heal paladins and
DKs, then hunters, then bears; the stage picks follow. A human tank counts as a living tank that never drags, so a
bot tank still drags beside one. 10-man (one mark) unchanged; same at normal and hard mode. No shared code. Reader:
`--phase3`'s beacons gain `dragged by` (claimers, tanks dragging of tanks up, raiders dead to the last release) and
`every tank dragging`; `drag_wave` tested.

**Next window should show** `dragged by` with at most one tank on every mark and `every tank dragging: 0`, raiders
dead inside a drag 0-1 a mark (uld2 8, uld5 4 here), and no tank death within 20 s of a release where both had dragged.
Back out if marks go undragged or healed marks reach the pile (`marked pairs`, `other Guardians it reached`).

**Seen, not picked.** Leftovers and the phase 3 opener (open failures 2-3). uld4's ranged stood 34.5 yd from the
Crusher, in reach (30 + its 4.5), dealing ~6.5k/s a bot against ~11k/s on Guardians elsewhere. uld2's two
Mercenary Soldier deaths came in phase 3.
