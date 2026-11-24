# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases`), tests in `tools/botobs/tests/test_botobs.py`
(`YoggPhases`); rounds 1-20 in `history/yogg-saron.md`.

**Fight model.**

- **Phase 1** (131-158 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s (the code and
  reader still hold the old 11.6-60.8 at 3.0: Q1). Bot tanks pull Guardians onto the stack (6.5 yd), one focus per
  instance, melee stand in it and eat every nova, the back line holds the 21.5 yd station. Sara needs ~11 deaths
  inside 15 yd. A nova gap (6 s after any death, a Guardian under 30% is not killable), a chain rule (not killable
  beside a lower one) and a non-tank AoE hold space the novas; the tanks' area threat is what keeps the Guardians
  nobody aims at off the DoT casters. What kills: two novas inside the gap on the melee, a nova on the enhancement
  shaman (bar 20.6-23.0k against an unresisted 20.9-23.8k), the first seconds of the 18 s handover (melee and tanks
  walk to the anchor at 21.5 yd, bearing 109°, with the leftovers), and Guardians piling up off the stack (cloud
  touches at the station, turns onto a warlock), whose Dark Volley (every 8 s each) kills the raid once phase 1
  ends. Melee lost here shrink every brain team after (melee first).
- **Phase 2** (355-440 s at 4-5 cycles). A portal wave every 80 s; the brain team (10, melee first, exactly one
  healer) clears an illusion room (Stormwind / Icecrown 20-30 s, Chamber 26-33 s), then hits the Brain (8.91M,
  scaled) until Induce Madness at 60 s: Stormwind / Icecrown 14-24% a cycle, Chamber 6-13%. The platform kills
  tentacles under Crusher Diminish Power (−20% a stack, carried below until the room clear's stun or a tank's
  break). Spawns speed up each wave, so a slow cycle grows the stock until Corruptor, Squeeze and Induce Madness
  wipe the raid in phase 2.
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies (30% floor), one spawns every 10 s and beacon
  heals restore them; the tanks fall at +50-120 s and the pile wipes the raid. Yogg ends at 3.9-12.0% (window 31),
  the best yet.

**Compositions on 9f0c9a4c7 (window 31, one pull per pool, no kill).**

| Design | Pools | Outcome |
|---|---|---|
| A | uld1, uld4 | uld1 wiped at 3:15: 20 Guardians, 8 alive at the handover, Dark Volley 21; uld4 phase 3, Yogg 3.9% |
| B | uld2, uld5 | uld2 phase 3, Yogg 5.9%; uld5 phase 2 collapse over 607 s, Brain 30.9% |
| C | uld3, uld6 | uld3 lost 8 melee in phase 1 (a pair 5), phase 2 collapse, Brain 51.7%; uld6 phase 3, Yogg 12.0% |

**Open failures, ranked (window 31).**

1. Phase 3 Immortal Guardians, 86. Stuck (rounds 3-5): a throughput race, not picked again without a new root cause.
2. Phase 2 collapse, 48 (uld3 22, uld5 26: Curse of Doom 15, Induce Madness 11, Squeeze 9, Draining Poison 9),
   downstream of phase 1 melee losses and slow cycles.
3. Shadow Nova, 28, all melee: enhancement shaman from full 10, handover and leftovers 8, a pair 5 (uld3, the second
   at 4.5% when the first died), Fervor holders still walking out 4, other 1.
4. Dark Volley 21, uld1 only: Guardians off the stack (round 21 targets the turns; the cloud touches are Q1).

Leads, not picked: Death Ray victims on a body detour walk issue no dodge for ~1 s (round 19, uld2 8:00.4); the
pet fallback sends a pet to an add inside a swinging Crusher's reach (round 17); a Fervor tank's run drags its
Guardians off the stack (round 19); `p1station`'s band test reads `GetDistance2d`, so `parked` never latches
(round 19); `YoggSaronCloudLead` projects at run speed against 2.0 yd/s clouds (round 19, unchecked); Crusher reach
4.5 since `2026_11_06_01`, the reader and doc say 8 (round 18); Army of the Dead ghouls wake phase 3 Guardians in
the spawn ring; a Diminish Power claim has no timeout; the judgement node spends a holy paladin's GCD for nothing;
casts finishing on a Guardian gone unkillable and pets left on it (round 20); tanks on a gap-held Guardian come
87% from `tank assist` (lowest threat in melee) or a target kept from before the gap, not the control node
(round 20 review), and `::TankTarget` stays out of the Yogg exclusions on purpose, so a tank-side fix is a question;
the enhancement shaman dies to one unresisted nova from full, worse when the pull starts without Blood Pact or
Commanding Shout (uld1 and uld2 bars 1.7-2.3k short at the pull, round 21); Fervor holders caught by the nova
still walking out on the spacing node, 4 (round 21); back-line Guardians the DK's Dark Command cannot reach
(36-45 yd, uld1, round 21).

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
- r20 AoE hold by `HitsNeighbours`, tank's stack attack killable first: fixed on its target (gap loss 2.1-2.6%/s,
  was 3.0-3.9; pair nova deaths 9 to 5), but the hold on tanks met its back-out condition; r21 exempts tanks.
- r21 tanks keep their area threat under the phase 1 and leftover hold: pending window 32.

**Tried and rejected.**

- Leaving a beacon-healed Guardian alone in phase 3 (r3): user declined, too risky.
- Round 9's join of any live tentacle: made cycles worse (joiners ended 53-85 yd out); narrowed in r12.
- Phase 3 ranged on the lowest leftover Crusher only (r7): user wants phase 3 to start with at most one Crusher.
- Ranged-first brain team (r8): user keeps melee first.
- Healer under 30% mana sent after the others (r13): withdrawn in r15, no case seen.
- Melee stepping out of the phase 1 nova (findings doc): late on 44 of 74, woke Guardians.
- Holding the tanks' area damage in the phase 1 hold (r20): Guardians on the stack turned off a tank 5.7 a minute,
  was 2.7-3.4, 8.0 a minute while a tank was held; backed out for tanks in r21.

**User answers in force.** Keepers: Freya only. Phase 3: no leave-alone for healed Guardians; ranged keep DoTs (Flame
Shock included) on Yogg; a bot tank taunts a fresh Guardian on a non-tank before a swap while the holder is over
35%. Phase 2 goal: start phase 3 with at most one Crusher. Brain team melee first, exactly one healer; a finished bot
waits at the stage unless a live tentacle stands within 40 yd of it; the room healer heals a reprisal-held bot first;
exit lead 1.2×; Diminish Power broken by a bot tank after the team clicks; the swap deferral covers any Guardian on
a raider, fresh or slipped (2026-11-24, judged by tank deaths within 10 s of a deferral; window 29: 3 deferrals, no
tank death). Left alone by choice: the shaman's
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

## Round 21 (2026-11-24, window 31 build 9f0c9a4c7: round 20)

**Data.** Window report `scratchpad/window31/window31/window31-yogg-saron.md`: six valid pulls, one per pool, 0
humans, Freya on the raid. Traces `603_{1,2,3,4,5,6}_yogg-saron_17955415{23,33,19,29,34,40}` (uld1-uld6), read with
`yogg_saron.py --phases --threat --clouds --fervor --novas --phase3 --brain` and `postmortem.py`; scratch scripts
(`scratchpad/yogg-r21/`) over the 48 traces of windows 19-31: Guardian targets on the stack, tank hold vetoes,
swings on non-tanks inside and outside the gap, spawns, each nova death with the victim's bar.

Deaths by mechanic:

- uld1 (A): nova 7 (Uldxai 0:51 and 1:34, Uldxar 1:24, four at 2:12.6 in the handover at 37-63% after Dark Volleys),
  Dark Volley 21 (2:04, then 2:23-2:45), Sara's Blessing 1; wiped 24 s into phase 2.
- uld2 (B): nova 3 (Fervor Uldyar 1:40, the shaman Uldyak 2:01 and 2:17), Sara's Blessing 1; phase 3 Immortal
  Guardian 27, Yogg 5.9%.
- uld3 (C): nova 8 (Fervor Uldzat 0:44 and Uldzay 1:24, a pair at 0:58.3 / 0:59.6 five, Uldzaj 1:16); phase 2 from
  8:16 Curse of Doom 7, Draining Poison 5, Squeeze 4, Induce Madness 3, Psychosis 2, Death Ray 1; Brain 51.7%.
- uld4 (A): nova 4 (the shaman Ulduai 0:44 and 1:24, two at 2:29.8 in the handover); phase 3 Immortal Guardian 29,
  Yogg 3.9%.
- uld5 (B): nova 4 (Fervor Uldvak 1:08, Uldvak 1:52, two on a leftover at 2:39.8), Squeeze 5, Curse of Doom 8,
  Induce Madness 8, Draining Poison 4, Wandering Plague 1; phase 2 over 607 s, Brain 30.9%.
- uld6 (C): nova 2 (the shaman Uldwaj 0:25 and 0:52); phase 3 Immortal Guardian 30, Drain Life 1, Yogg 12.0%.

**Verdict on round 20.** Its target, the second nova of a pair inside the gap: fixed. A: no pair death (uld1 one
pair 4.7 s apart, uld4 two, no deaths). B: no pair. C: uld3 lost five to a pair 1.28 s apart whose second Guardian
was at 4.5% when the first died, out of any gap's reach; uld6 one pair, no deaths. Gap loss 2.1-2.6%/s (3.0-3.9), a
tank on a held one 29-85% (64-100%), each area action 2-6 a pull in the gap (Maul 16-22). Back-out condition: phase
1 ran 141.6-157.3 s, under 165, but **Guardians turned on non-tanks**: within 10 yd of the middle, a Guardian left a
tank for a non-tank 17 / 19 / 17 / 8 / 19 / 5 times (uld1-uld6), 5.7 a minute, against 2.7 over windows 19-28 (36
pulls, none over 13) and 3.4 in window 29; warlocks took 37 of 85 (7 of 49). The turns follow the tanks' holds: 8.0
a minute in the 5 s after a tank's hold veto, 4.0 otherwise. Tanks were held 43-100 s of phase 1 (no tank veto in
window 29): Consecration, Death and Decay, Blood Boil, Pestilence, Swipe, Maul, Thunder Clap.

**Stop rules.** No kill. Phase 3 stays stuck. Not stuck: this round backs out round 20's tank side.

**Picked: round 20's hold on tanks turns Guardians off the stack**, the cause of uld1's wipe with Q1's cloud
touches. The warlock Uldxak took stack Guardians off the tanks 11 times (Guardian 68869 at 1:36 and 1:46, 68872 at
1:52 and 2:00, 68874 at 1:58 and 2:05); each walked to the 21.5 yd station and parked there (not killable off the
stack under 35%). With six more spawned by cloud touches at the station (0:46 twice, 1:34 twice, 1:46, 1:56; the
raid's own feet 12 of 20, 5-9 in the other pulls), phase 1 ended with 8 Guardians alive, one leftover died in 24 s,
and Dark Volley (each Guardian every 8 s, `boss_yoggsaron.cpp:1113-1116`) killed 21; Uldxak died at the station at
2:23 under two Guardians. `YoggSaronPhase1AoeHoldMultiplier::GetValue` (`UldMultipliers_YoggSaron.cpp:186-193`),
keyed on `HitsNeighbours`, held the tanks' area threat whenever a non-focus Guardian under 30% stood within 10 yd of
the middle or the focus (`YoggSaronPhase1AoeHold`, `UldEncounter_YoggSaron.cpp:3897-3922`), far longer than the
6 s gap, and the warlocks' DoTs on attackers out-threatened the tanks on the Guardians nobody aims at.

**Fix (round 20 backed out for tanks).** The multiplier returns 1 for `PlayerbotAI::IsTank(bot)`, so tanks run as
before round 20: none of their area actions is `Aoe`-typed, and window 29 has no tank veto. Non-tanks keep the
`HitsNeighbours` hold (leftover hold too), and the tank's stack attack stays killable first. Same at 10 and 25,
normal and hard mode; a human tank's area spells were always his own. No shared code. Reader: `--threat`
`Guardians within 10 yd of the middle turning off a tank` (`stack_turns`, tested), by the new target's class.

**Next window should show:** turns off a tank at or under ~3.4 a minute and no phase 1 hold veto on a tank; gap
loss between 2.1-2.6 and 3.0-3.9%/s with Maul, Swipe, Consecration and Thunder Clap back in `area run`; pair nova
deaths no worse than window 29's 9; Guardians alive at the ring 1-3.

**Seen, not picked.**

- The enhancement shaman died to one nova from full 10 times (window 29: 6 shaman nova deaths): bar 20.6-23.0k
  against unresisted 20.9-23.8k. uld1's melee started 1.7k short (no Blood Pact at the pull), uld2's 2.3k
  (Commanding Shout up at 11 s); a buff gap at the pull, not round 20.
- Fervor holders still walking out on the spacing node took a doubled nova (37.9-40.8k): 4 (window 29: 1).
- uld1's DK tank aimed Dark Command at back-line Guardians 36-45 yd away: 24 of 31 taunts failed.
- Yogg reached 3.9% (uld4) and 5.9% (uld2), the lowest yet.
