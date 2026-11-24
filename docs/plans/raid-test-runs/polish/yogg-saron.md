# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases`), tests in `tools/botobs/tests/test_botobs.py`
(`YoggPhases`); rounds 1-23 in `history/yogg-saron.md`. Window 35 is the first on the professions rosters (all eleven
at 450, perks and profession enchants), so trends across window 35 and earlier compare unlike rosters.

**Fight model.**

- **Phase 1** (129-152 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s (the reader since
  r22; the code and `yogg.cloudreach` still hold the old 11.6-60.8 at 3.0, Q1 step 2). The 21.5 yd station sits in
  reach of orbits 1-3 and wakes 4-6 of a phase 1's 14-16 Guardians. Bot tanks pull Guardians onto the stack (6.5 yd),
  one focus per instance, melee stand in it and eat every nova, the back line holds the station. Sara needs ~11 deaths
  inside 15 yd. A nova gap (6 s after any death, a Guardian under 30% is not killable), a chain rule (not killable
  beside a lower one), a non-tank AoE hold and a free low Guardian taking the focus back (r23) space the novas; a held
  Guardian still loses 2-4%/s to tank area damage, DoTs, pets and novas. What kills: two low Guardians side by side
  (the second dies inside the gap once the first's nova takes 3-7% off it), one nova from full on a bar of 21.5-23.9k
  (enhancement shaman, rogues; a 10%-resisted nova is 22.5-24k), a nova on bars left at 68-70% after a weak 8 s of
  healing, and Guardians piling up off the stack. Melee lost here shrink every brain team and the platform after.
- **Phase 2** (353-357 s). A portal wave every 80 s; the brain team (10, melee first, exactly one healer) clears an
  illusion room, then hits the Brain until Induce Madness at 60 s. A tank breaks Crusher Diminish Power once the
  team's carriers are below. The platform kills tentacles; spawns speed up each wave, so phase 3 starts with 1-3
  tentacles where the raid kept its melee, 5-10 where it lost 3-6 in phase 1.
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies; one spawns every 10 s and leaves stasis at
  +4 s on zero threat onto a fixed melee (first in the map's player list) unless a taunt or hit comes first. Ranged
  stop at 30% (the user's floor), so only tank, area and gaze-time melee damage takes a Guardian to Weakened (under
  10%, no Empowered stacks, ~1/6 of a 30% one's swing). Yogg falls ~0.1%/s on melee and DoTs. With 1-3 leftovers
  phase 3 lasts 164-278 s and ends in a kill or at 1.6-17%; with 5-10 the raid dies in 76-109 s at 25-30%.

**Compositions on ce636f992 (window 37, one pull per pool).** A meets the done rule (uld4, 5 deaths); B and C wiped
in phase 3.

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 kill, 31 deaths (3 tentacles); uld4 kill, 5 deaths (1) |
| B | uld2, uld5 | uld2 Yogg 26.2% (six melee lost at 2:14-2:15, 9 tentacles); uld5 14.3% (3) |
| C | uld3, uld6 | uld3 27.1% (four melee lost at 1:25, 6); uld6 1.65% (2, paladin tank dead to Deafening Roar at 11:50) |

**Why B and C fail where A kills** (22 pulls on windows 35-37: A 6, B 8, C 8; designs per the roster table, 25-man
fields 2 tanks, 4 healers, all 19 DPS):

1. **B loses its melee in phase 1**: 4 of 8 B pulls lost 3-6 melee in one nova tick (C 1, A 1), three of them
   reaching phase 3 with 9-10 tentacles, a fifth with 5 after losing two. No composition cause found: B's melee
   bars are the largest (rogues 26k, A's 23k), healing in the 8 s after a nova is alike, pets on held Guardians alike.
2. **B's tanks (prot warrior + bear) are the weakest phase 3 pair.** Two 30 yd single taunts, no Righteous Defense
   or Death Grip: every taunt cooling at 25-50% of Guardian swings on non-tanks before the first tank death (C 24-45%,
   A 21-36% on the control's own casts, 0-2% counting the DK's unused Death Grip); fresh Guardians on a raider a median
   1.1-2.7 s (A 0.3-1.8, C 0.3-1.5); raiders killed by a fresh one before the first tank death B 18, A 6, C 3. They
   deal 8-10k/s to the pile (A's prot paladin + blood DK 11-16k/s), so fewer Guardians reach Weakened (50-61%, A
   59-78%) and the tanks fall sooner. r24 picks the taunts.
3. **B's and C's ranged deal less in phase 3**: median 79k/s and 87k/s against A's 111k/s (B frost mage and MM where
   A has fire and BM; C no balance druid). Observed, not root-caused.
4. **C is closest**: 6 of 8 phase 3s run 185-278 s, one a kill at 33 deaths, the rest ending at 1.65-17%; its bear
   is the weak half (Growl only; the pair deals 6.4-15.9k/s, Weakens 35-76%).

**Open failures, ranked (window 37).**

1. Phase 3 Immortal Guardians, 131. Stuck as a throughput race (rounds 3-5); r24 takes B's taunt starvation, a new
   root cause; Q3 asks for a burn, Q4 for Misdirection.
2. Shadow Nova, 17: two low Guardians side by side 9 (uld2's handover pair 5, uld3's chain-held one beside a parked
   one 4), one nova on bars at 68-70% 3 (uld5 2:11), from full on low bars 3 (uld4), other 2. Pairs picked in r20
   and r23.
3. Death Ray 5, Drain Life 3, Deafening Roar 2, Lunatic Gaze 2, Sara's Blessing 1, Hand of Sacrifice 1.

Leads, not picked: a chain-held Guardian on the stack waits on a lower one parked under the 35% floor while both
bleed (uld3 1:19-1:22); the DK tank's Death Grip is held by the phase 3 taunt hold and never cast by the control; the
warrior's Challenging Shout and bear's Challenging Roar likewise; Death Ray victims on a `phase 2 spacing action` walk
(round 19); the pet fallback sends a pet into a swinging Crusher's reach (round 17); a Fervor tank's run drags its
Guardians off the stack (round 19); `p1station`'s band test reads `GetDistance2d`, so `parked` never latches (round
19); `YoggSaronCloudLead` projects at run speed against 2.0 yd/s clouds (round 19); Crusher reach 4.5 since
`2026_11_06_01`, the reader and doc say 8 (round 18); Army of the Dead ghouls wake phase 3 Guardians; a Diminish
Power claim has no timeout; casts finishing on a Guardian gone unkillable and pets left on it (round 20); tanks on a
gap-held Guardian come from `tank assist` or a kept target, a tank-side fix being a question; back-line Guardians the
DK's Dark Command cannot reach (round 21); the first nova of a pull can go from 10% to dead in 0.2 s (uld4 0:23.9).

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
- r22 reader on the measured orbits and the station's cost per orbit (Q1 step 1): reader, 4-6 a pull, no `no cloud`.
- r22 Diminish Power break once that Crusher's stack is carried below (Q2): kept (window 35).
- r23 a free low Guardian takes the phase 1 focus back outside the handover too: kept (window 37).
- r24 a warrior tank keeps Vigilance on the other tank in phase 3: pending.

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
picked only when it ranks first. Left alone by choice: the shaman's Reincarnation onto the stack, late Guardians woken
by the back line, the raid working the next Guardian while a low one waits out its gap.

**Open questions.**

- **Q3, a burn at the end of phase 3** (round 23, unanswered). Should ranged leave the Guardians and put everything
  on Yogg once he is under ~12%? Ranged spend 83-90% of 71-126k/s on Guardians over 30% and only DoTs on Yogg, so
  Yogg falls ~0.1%/s on melee. Window 37's uld6 died with Yogg at 1.65% ~100 s after crossing 12%, and uld1 killed
  as its last raiders died; with the ranged on him the last 12% (~5.3M) takes ~40 s. The risk: Guardians not brought
  back to 30% swing harder and drain more for those 40 s, so the tanks may fall sooner. **Recommended: yes, for one
  window, at 12%**, judged by Yogg's end and the time from 12% to the tank deaths.
- **Q4, Misdirection on fresh Immortal Guardians** (round 24). Should a hunter put Misdirection on a bot tank and
  shoot each fresh Guardian at its spawn, so it opens on the tank instead of the fixed melee? A hit ends the 4 s
  stasis and the redirected threat makes the tank its first victim; today about half go to a melee at +4.0 s (uld4 14
  of 23, uld5 9 of 17), and a warrior's or bear's 30 yd taunt reaches them only as they walk in. Two hunters give 4
  a minute against 6 spawns, and the tanks' taunts are left for returns, chains and swaps. The risk: a tank that
  leads by only three shots can still lose the Guardian to the healers' threat before its own attacks land.
  **Recommended: yes, for one window after round 24's**, if B's fresh Guardians still sit on a raider over 1 s.

## Round 24 (2026-11-25, window 37 build ce636f992: round 23)

**Data.** Window report `scratchpad/window37/window37/window37-yogg-saron.md` (six pulls, one per pool), widened to
22 pulls since ce2ffa6be with `raidtest.py report --head ce2ffa6be --pulls 8` (A 6, B 8, C 8). Traces
`603_15…1795556061`, `603_16…1795556067`, `603_20…1795556557`, `603_17…1795556068`, `603_18…1795556073`,
`603_19…1795556506` (uld1-uld6) with `--phases --novas --phase3`, the other 16 with `--phase3`; scratch counts of
melee health before each nova, healing after it, pets on gap-held Guardians, focus changes per death, taunt readiness
at Guardian swings on non-tanks and Vigilance procs.

Deaths by mechanic (phase 1 / 2 / 3):

- uld1 (A) kill: nova 1 / Death Ray 1 / Immortal Guardian 27, Drain Life 2; Yogg 1.56% 2 s before the kill.
- uld4 (A) kill: nova 3 (shaman twice, rogue, from full) / Death Ray 1 / Immortal Guardian 1.
- uld2 (B): nova 5, Sara's Blessing 1 (2:14-2:15) / — / Immortal Guardian 24, Gaze 1; Yogg 26.2%.
- uld5 (B): nova 3 (2:11, one nova) / — / Immortal Guardian 25, Roar 1, Hand of Sacrifice 1, Drain Life 1; 14.3%.
- uld3 (C): nova 4 (1:25) / Death Ray 2 / Immortal Guardian 26; 27.1%.
- uld6 (C): nova 1 / Death Ray 1, Gaze 1 / Immortal Guardian 28, Roar 1; 1.65% at 12:26.

**Verdict on round 23 (a free low Guardian takes the focus all phase 1): kept**, on its own target in every design:
`focus past a free low one on the stack` A 1.1 / 1.4 s, B 0.8 / 0.5, C 3.4 / 0.7 (window 35 12.5-19.6); `lower` 4-8
before the handover, `chain` 0 (window 35 4-6); pairs under the gap A 1 / 0, B 1 / 1, C 1 / 0; phase 1 129-149 s;
focus changes 1.6-2.6 a death (1.8). Not met: three ticks killed three or more, and pairs killed 9 (5; target 0).
Both pairs came from two held low Guardians side by side, not a free one. uld2 2:13.7 (handover, lowest first): the
raid killed G68977 at 2.2% while G68976 stood ~5 yd off at 5.5%, tank-held; the nova took 3.2% off it and splash the
rest in 1.4 s, five melee dead (they sat at 23-57% for 4 s from 2:08). uld3 1:19.2-1:22.9: G68961 (23.7%, on the
stack) waited on the chain rule for G68940 (19.4%, 14 yd out, parked under the 35% floor); both bled ~2.5%/s with
nobody on them, G68940 died on reaching the stack and G68961 (13.6%, 6.6% after the nova) 2.7 s later, four melee
dead. uld5's 2:11 triple was one nova on a rogue, the shaman and a rogue at 68-70%, 8 s after a nova followed by 61k
of healing on the melee (others 67-238k).

**Stop rules.** Not polished: B and C have no kill. Phase 3 stays stuck as a race, but B's taunt starvation is a new
root cause. Nova pairs were picked in r20 and r23, so not a third time. Q1 step 2 does not rank first.

**Picked: B's tanks run out of taunts in phase 3** (Immortal Guardian, 131, and the design gap the brief asks for).
uld5 (B) phase 3: 9 of 17 Guardians opened on a melee at +4.0 s, 9 of 15 sat on a raider before a tank held them
(median 1.1 s, max 6.5), three raiders killed by fresh Guardians before the first tank death, when 17 of 36 swings on
non-tanks came with no bot tank's taunt off cooldown (A's uld4 1 of 55 counting Death Grip, 18 without). Root cause:
the warrior's Taunt and the bear's Growl are B's only phase 3 taunts (the control owns all of them,
`UldActions_YoggSaron.cpp:88-103`), and the warrior's Vigilance, whose every hit resets Taunt (50725,
`spell_warr_vigilance_trigger`), sits where `CastVigilanceAction::GetTarget` (`WarriorActions.cpp:101-139`) puts it:
the highest-gear dps. uld5 kept it on hunter Uldvau all pull (29 resets in phase 3), uld2 on a melee warrior then a
rogue (16). A bug: the control intends every loose Guardian taunted, and the warrior holds a reset it never aims at
the tank being hit.

**Fix.** `yogg-saron vigilance` (`ACTION_RAID + 2.5`, `YoggSaronVigilanceTarget`): in phase 3 a bot warrior tank
casts Vigilance on the living tank with the most attackers within 30 yd, unless its own Vigilance is already on
a tank or that tank carries another warrior's; `yogg.vigilance` names the tank. Only B fields a warrior tank at 25 and
10; a human tank is a valid target, a human warrior runs nothing. Same code at 10 and 25, normal and hard mode. No
shared code: the generic picker stays as is and stands down once the bot's Vigilance is on a group member. Reader:
`--phase3`'s per-tank line reads each class's taunts (`every taunt it has cooling`, was Hand of Reckoning only, 0 for
non-paladins) with Vigilance resets, plus `no bot tank with a taunt ready` before the first tank death; tested.

**Next window should show** in B (uld2, uld5): `yogg.vigilance` on the bear early in phase 3; the warrior's `every
taunt it has cooling` far under the bear's, `no bot tank with a taunt ready` under 20% of swings (25-50%); fresh
Guardians on a raider a median under 1 s (1.1-2.7) and raiders killed by a fresh one before the first tank death under
1 a pull (2.25). A and C unchanged. Back out if B's tanks die sooner or `first off a tank` turns onto the warrior
or the raid more often: Vigilance moves 10% of the bear's threat to the warrior (59665).

**Seen, not picked.** A's DK tank had Death Grip ready at every swing on a non-tank while the control never casts it;
B's warrior and bear hold Challenging Shout and Roar all phase 3; uld6 lost its paladin tank to Deafening Roar at
11:50 with Yogg near 5% and wiped at 1.65%.
