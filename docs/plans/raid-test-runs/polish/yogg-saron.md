# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases`), tests in `tools/botobs/tests/test_botobs.py`
(`YoggPhases`); rounds 1-22 in `history/yogg-saron.md`. Window 35 is the first on the professions rosters (all eleven
at 450, perks and profession enchants), so trends across window 35 and earlier compare unlike rosters.

**Fight model.**

- **Phase 1** (133-152 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s (the reader since
  r22; the code and `yogg.cloudreach` still hold the old 11.6-60.8 at 3.0, Q1 step 2). The 21.5 yd station sits in
  reach of orbits 1-3 and wakes 4-6 of a phase 1's 14-16 Guardians. Bot tanks pull Guardians onto the stack (6.5 yd),
  one focus per instance, melee stand in it and eat every nova, the back line holds the station. Sara needs ~11 deaths
  inside 15 yd. A nova gap (6 s after any death, a Guardian under 30% is not killable), a chain rule (not killable
  beside a lower one), a non-tank AoE hold and, since r23, a free low Guardian taking the focus back space the novas;
  the tanks' area threat keeps unaimed Guardians off the DoT casters, so held ones still lose 1.5-3%/s. What kills: a
  pair whose second Guardian is under ~3% when the first dies (two novas in one tick), one nova from full on a bar of
  21.5-23.9k (enhancement shaman, rogues; a 10%-resisted nova is 22.5-24k and their buttons cover one nova in six), the
  first seconds of the 18 s handover, and Guardians piling up off the stack (Dark Volley once phase 1 ends). Melee lost
  here shrink every brain team and the platform after (uld5: five melee in one tick, platform 23-33k/s against
  56-82k/s).
- **Phase 2** (355-437 s at 4-5 cycles). A portal wave every 80 s; the brain team (10, melee first, exactly one
  healer) clears an illusion room (Stormwind / Icecrown 20-32 s, Chamber 24-32 s), then hits the Brain until Induce
  Madness at 60 s: Stormwind / Icecrown 15-25% a cycle, Chamber 2-23%. A tank breaks Crusher Diminish Power once the
  team's carriers are below (claims a median 0.0-0.2 s after the first carrier, the walk to the Crusher 2-12 s). The
  platform kills tentacles; spawns speed up each wave, so phase 3 starts with 2-3 tentacles (1.4-2.3M) where the raid
  kept its melee, 6-10 (7.2-10.5M) where it lost them.
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies (30% floor), one spawns every 10 s, each leaves
  its tank at stasis end (+4 s) and beacon heals restore them; with 2-3 leftovers the first death comes at +95-143 s
  and the wipe at +211-265 s. Ranged put 75-88% of 69-90k/s into Guardians over 30% (the user's floor) and DoTs on
  Yogg, melee 55-66% of 61-86k/s on Yogg, so Yogg falls ~0.08-0.11%/s and ends 6.5-17% (uld6 needed ~60 s more); with
  6-10 leftovers the raid dies in 107-139 s at 26.6-27.1%.

**Compositions on ce2ffa6be (window 35, one pull per pool, no kill, all six wiped in phase 3).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 Yogg 26.6% (6 tentacles at phase 3, seven phase 2 deaths); uld4 13.1% (3) |
| B | uld2, uld5 | uld2 Yogg 11.0% (2); uld5 27.1% (10, a double nova killed five melee in phase 1) |
| C | uld3, uld6 | uld3 Yogg 16.9% (2); uld6 6.5% (2) |

**Open failures, ranked (window 35).**

1. Phase 3 Immortal Guardians, 150. Stuck (rounds 3-5): a throughput race, not picked again without a new root cause;
   Q3 asks for a burn.
2. Shadow Nova, 17: one nova from full on a 21.5-23.9k bar 7 (shamans 5, rogues 2), uld5's double nova 5 (r23), one
   on a bar left low by Reincarnation or an earlier nova 3, a leftover after phase 1 1, unexplained 1 (uld6 0:24.8).
3. Death Ray 7 (3 pulls): 5 victims on a `phase 2 spacing action` walk, 2 grabbed by a Constrictor.
4. Lunatic Gaze 4, Draining Poison 3, Drain Life 3, Curse of Doom 2, Brain Link 2, Squeeze 1, Reprisal 1.

Leads, not picked: Death Ray victims on a body detour walk issue no dodge for ~1 s (round 19); the pet fallback sends
a pet to an add inside a swinging Crusher's reach (round 17); a Fervor tank's run drags its Guardians off the stack
(round 19); `p1station`'s band test reads `GetDistance2d`, so `parked` never latches (round 19);
`YoggSaronCloudLead` projects at run speed against 2.0 yd/s clouds (round 19, unchecked); Crusher reach 4.5 since
`2026_11_06_01`, the reader and doc say 8 (round 18); Army of the Dead ghouls wake phase 3 Guardians in the spawn
ring; a Diminish Power claim has no timeout; the judgement node spends a holy paladin's GCD for nothing; casts
finishing on a Guardian gone unkillable and pets left on it (round 20); tanks on a gap-held Guardian come from `tank
assist` or a target kept from before the gap, and `::TankTarget` stays out of the Yogg exclusions on purpose, so a
tank-side fix is a question; the enhancement shaman and rogues die to one nova from full, buttons spent; back-line
Guardians the DK's Dark Command cannot reach (36-45 yd, round 21); B's stack Guardians turn off a tank 4.1-4.8 a
minute (window 33); the first nova of a pull can go from 10% to dead in 0.2 s, under the ladder's reaction (uld4
0:23.9).

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
- r19 back-line dodges stay outside the band: fixed (0 inside, 0 back-line nova hits in windows 29-35).
- r20 AoE hold by `HitsNeighbours`, tank's stack attack killable first: fixed on its target for non-tanks.
- r21 tanks keep their area threat under the phase 1 and leftover hold: fixed (turns 2.5 a minute, Dark Volley 0).
- r22 reader on the measured orbits and the station's cost per orbit (Q1 step 1): reader, 4-6 a pull, no `no cloud`.
- r22 Diminish Power break once that Crusher's stack is carried below (Q2): kept (window 35).
- r23 a free low Guardian takes the phase 1 focus back outside the handover too: pending.

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
its stack are below, without waiting for the rest of the team (Q2, 2026-11-24; its window passed, kept). Cloud
geometry (Q1, 2026-11-24): re-derive phase 1 positions from the measured orbits, one navprobed position change per
round, the melee core first, picked only when it ranks first. Left alone by choice: the shaman's Reincarnation onto
the stack, late Guardians woken by the back line, the raid working the next Guardian while a low one waits out its
gap.

**Open questions.**

- **Q3, a burn at the end of phase 3** (round 23). Should ranged leave the Guardians and put everything on Yogg once
  he is under ~12%? Ranged spend 75-88% of 69-90k/s on Guardians over 30% and only DoTs on Yogg, so Yogg falls
  ~0.1%/s on melee. uld6 crossed 12% 68 s before its wipe and uld2 50 s before; with the ranged on him (~140k/s) the
  last 12% (~5.3M) takes ~40 s. The risk: Guardians not brought back to 30% swing harder and drain more for those
  40 s, so the tanks may fall sooner. **Recommended: yes, for one window, at 12%**, judged by Yogg's end and the time
  from 12% to the tank deaths.

## Round 23 (2026-11-24, window 35 build ce2ffa6be: round 22)

**Data.** Window report `scratchpad/window35/window35/window35-yogg-saron.md`: six valid pulls, one per pool, 0
humans, the first on the professions rosters; baseline window 33 plus filler 33c (12 pulls of 9ad197722). Traces
`603_1_yogg-saron_1795550325`, `603_2_…1795550330`, `603_3_…1795550321`, `603_4_…1795550332`, `603_5_…1795550337`,
`603_6_…1795550338` (uld1-uld6), read with `--crush --brain --tentacles --phase3 --novas --clouds`; scratch counts of
nova deaths by bar and of gap-held Guardians' loss by who was on them; the new `--phases` line over windows 33-35.

Deaths by mechanic (phase 1 / 2 / 3), all six wiped in phase 3:

- uld1 (A): nova 1 (shaman, 21.5k bar, from full) / Death Ray 3, Brain Link 2, script kill 1, Squeeze 1 / Immortal
  Guardian 22, Gaze 2, Draining Poison 1; Yogg 26.6%, 6 tentacles at phase 3.
- uld2 (B): Sara's Blessing 1 / leftover nova 1 / Immortal Guardian 26, Drain Life 3; Yogg 11.0%, 2.
- uld3 (C): nova 4 (shaman from full twice, again at 37% after Reincarnation; a warrior at 62%) / — / Immortal
  Guardian 28; Yogg 16.9%, 2.
- uld4 (A): nova 4 (shaman twice, rogues 23.1k and 23.9k, all from full) / Death Ray 2 / Immortal Guardian 27; Yogg
  13.1%, 3.
- uld5 (B): nova 6 (five at 2:04.80 in one tick; the shaman again at 2:22) / Death Ray 2, Reprisal 1 / Immortal
  Guardian 16, Gaze 2, Draining Poison 2, Curse of Doom 2; Yogg 27.1%, 10.
- uld6 (C): nova 1 / — / Immortal Guardian 31; Yogg 6.5%, 2.

**Verdict on round 22 (Q2, break per carrier): kept**, on its own targets. First carrier below a median 0.2 / 0.1 s
before the claim in A (uld1 15 claims, max 0.2; uld4 6, max 3.0), 0.1 / 0.1 in B (uld2 6, max 2.7; uld5 10, max
8.6), 0.0 / 0.2 in C (uld3 4, max 0.2; uld6 4, max 2.1); the channel left the team a median 2.1-12.2 s after the
claim (the walk). Carried below, per wave median 1.0-24.0 s, 10 of 18 under 10 s (window 33 0.5-26.5). Crushes on a
breaking tank: uld1 2 on Uldxam (24.8k, no death), 0 elsewhere; no Crush death. Claims stay ~3 a wave (uld1 4 on one
Crusher's re-casts twice). Brain per Stormwind / Icecrown cycle 15.0-24.9%, Chamber 2.2-22.5%; tentacles at phase 3
6 / 2 / 2 / 3 / 10 / 2 (window 33 10 / 5 / 7 / 3 / 6 / 7, unlike rosters). Q1 step 1's reader holds: the station woke
4 / 5 / 5 / 5 / 5 / 6 Guardians, no `no cloud` spawn.

**Stop rules.** No kill. Phase 3 stays stuck: every Guardian still leaves its tank at stasis end (+4.0 s), and the
longer phases 3 (211-265 s with 2-3 leftovers) end on the same throughput race; no new root cause, a question (Q3).
Q1 step 2 does not rank first: no death mechanic traces to the melee core's position.

**Picked: a free low Guardian bleeds out while the focus works a healthier one** (the nova, 17, top outside phase 3;
the double nova cost uld5 its pull). uld5 1:52.2-1:56.9: the gap after 1:46.2 was over and G8948 sat free at 6.0-2.2%
on the stack, while the held focus kept every non-tank on G8962 from 38.9% to 27.2%. G8948 died to splash at 1:56.75,
leaving G8962 (27.2%) and G8956 (15.7%) both held in the gap; the warrior tank's Cleave and Revenge, area damage and
a pet took them to 3.8% and 3.5%, and the kill of G8956 at 2:04.800 took G8962 with it: two novas in one tick, five
melee dead, the platform at 23-33k/s after. Root cause: `YoggSaronPhase1Focus` (`UldEncounter_YoggSaron.cpp:3844-3846`)
keeps a killable held focus outside the handover unless it stops counting for Sara; `YoggSaronLowGuardianTakesOver`
gave a free low one the focus back in the handover only. New `--phases` line `focus past a free low one on the stack` (half the bot non-tanks on a healthier
Guardian while the lowest under 30% on the stack is out of its gap, before the handover): 12.5-19.6 s a pull here,
6.2-23.8 over windows 33 / 33c, and 12 runs over 18 pulls ended in a pair under the gap, against 13 pairs in all
(window 33's uld3 chain of three, 33c's 2:02.18 pair included).

Bug: the strategy kills the lowest killable Guardian first and spaces deaths on its own schedule. Fix
(`UldEncounter_YoggSaron.cpp:3844-3846`): `lower` no longer needs the handover, so a killable Guardian under 30% and
lower than the held one takes the focus in all of phase 1 (`yogg.p1focus` `lower`). Gap and chain rules unchanged, so
the raid still works the next Guardian while a low one waits out its gap. Same code at 10 and 25, normal and hard mode;
the focus is the bots', a real player only adds splash. No shared code. Reader: `unparked_rows`, `runs_into_pairs`
and `show_free_low`, tested.

**Next window should show:** `focus past a free low one on the stack` near 0 s (12.5-19.6 here), `yogg.p1focus`
`lower` before the handover; `under the 6 s gap` under 5 in six pulls and no nova killing three or more in one tick;
phase 1 nova deaths from pairs 0 (5 here); phase 1 no longer than 133-152 s and `counting for Sara` still enough.
Back out if phase 1 runs longer or the focus flips between two Guardians more than once a death.

**Seen, not picked.**

- 7 nova deaths from full on 21.5-23.9k bars: the ladder fired (Shamanistic Rage, Cloak of Shadows) and the cooldowns
  ran out; uld4's first nova went from 10% to dead in 0.2 s, before any button.
- Death Ray: 5 of 7 victims were on a `phase 2 spacing action` walk when it hit.
- uld1's phase 2 lost seven (Death Ray 3, Brain Link 2) and reached phase 3 at 9:30 with 6 tentacles.
