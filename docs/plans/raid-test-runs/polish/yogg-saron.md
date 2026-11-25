# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases` and `--burn`; the room gate moved to `--room` in r25),
tests in `tools/botobs/tests/test_botobs.py` (`YoggPhases`); rounds 1-24 in `history/yogg-saron.md`. Window 35 is the
first on the professions rosters (all eleven at 450, perks and profession enchants), so trends across window 35 and
earlier compare unlike rosters.

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
- **Phase 2** (275-357 s). A portal wave every 80 s; the brain team (10, melee first, exactly one healer) clears an
  illusion room, then hits the Brain until Induce Madness at 60 s. A tank breaks Crusher Diminish Power once the
  team's carriers are below. The platform kills tentacles; spawns speed up each wave, so phase 3 starts with 1-3
  tentacles where the raid kept its melee, 5-10 where it lost 3-6 in phase 1.
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies; one spawns every 10 s and leaves stasis at
  +4 s on zero threat onto a fixed melee (first in the map's player list) unless a taunt or hit comes first. Ranged
  stop at 30% (the user's floor), so only tank, area and gaze-time melee damage takes a Guardian to Weakened (under
  10%, no Empowered stacks, ~1/6 of a 30% one's swing). Ranged put 54-86% of 77-112k/s on Guardians over 30% and
  Yogg falls ~0.12%/s, so he crosses 12% at +129-154 s; in window 39 four wipes lost their tanks 11-93 s after it and
  ended at 3.6-9.7%, uld2 stalled at 12.3%, and uld6 killed 88 s after it with both tanks up. With 5-10 leftovers the
  raid dies in 76-109 s at 25-30%. r25 builds the burn under 12% (Q3).

**Compositions on 6694fb83e (window 39, one pull per pool).** C meets the done rule (uld6, 3 deaths); A and B wiped
in phase 3 near the end. Phase 1 cost only 7 nova deaths in six pulls.

| Design | Pools | Outcome (tentacles at phase 3) |
|---|---|---|
| A | uld1, uld4 | uld1 Yogg 3.63%, 33 deaths (1); uld4 4.75%, 32 (1) |
| B | uld2, uld5 | uld2 12.31%, 31 (3); uld5 9.41%, 31 (2) |
| C | uld3, uld6 | uld3 9.70%, 32 (3); uld6 kill, 3 deaths (2) |

**Why B and C fail where A kills** (22 pulls on windows 35-37: A 6, B 8, C 8; designs per the roster table, 25-man
fields 2 tanks, 4 healers, all 19 DPS; window 39 has A wiping twice too, at 3.6-4.8%):

1. **B loses its melee in phase 1**: 4 of 8 B pulls lost 3-6 melee in one nova tick (C 1, A 1), three of them
   reaching phase 3 with 9-10 tentacles, a fifth with 5 after losing two. No composition cause found: B's melee
   bars are the largest (rogues 26k, A's 23k), healing in the 8 s after a nova is alike, pets on held Guardians alike.
2. **B's tanks (prot warrior + bear) are the weakest phase 3 pair.** Two 30 yd single taunts, no Righteous Defense
   or Death Grip: every taunt cooling at 25-50% of Guardian swings on non-tanks before the first tank death (C 24-45%,
   A 21-36% on the control's own casts, 0-2% counting the DK's unused Death Grip); fresh Guardians on a raider a median
   1.1-2.7 s (A 0.3-1.8, C 0.3-1.5); raiders killed by a fresh one before the first tank death B 18, A 6, C 3. They
   deal 8-10k/s to the pile (A's prot paladin + blood DK 11-16k/s), so fewer Guardians reach Weakened (50-61%, A
   59-78%) and the tanks fall sooner. r24's Vigilance helps on its own target (Fixes in place).
3. **B's and C's ranged deal less in phase 3**: median 79k/s and 87k/s against A's 111k/s (B frost mage and MM where
   A has fire and BM; C no balance druid). Observed, not root-caused.
4. **C is closest**: 6 of 8 phase 3s run 185-278 s, one a kill at 33 deaths, the rest ending at 1.65-17%; its bear
   is the weak half (Growl only; the pair deals 6.4-15.9k/s, Weakens 35-76%).

**Open failures, ranked (window 39).**

1. Phase 3 Immortal Guardians, 144: every pull, the tanks falling 11-93 s after Yogg crosses 12%. Stuck as a
   throughput race (rounds 3-5); r24 took B's taunt starvation; r25 builds Q3's burn; Q4 asks for Misdirection.
2. Shadow Nova 7: uld1 2, uld2 2 and uld5 2 (each one melee twice), uld3 1; no tick killed two.
3. Death Ray 3, Drain Life 3, Lunatic Gaze 2, Crush 1, Deafening Roar 1, Hand of Sacrifice 1.

Leads, not picked: a chain-held Guardian on the stack waits on a lower one parked under the 35% floor while both
bleed (uld3 1:19-1:22, window 37); the DK tank's Death Grip is held by the phase 3 taunt hold and never cast by the
control; the warrior's Challenging Shout and bear's Challenging Roar likewise; Death Ray victims on a `phase 2 spacing
action` walk, a Fervor tank's run dragging its Guardians off the stack, `p1station`'s band test on `GetDistance2d`
so `parked` never latches, `YoggSaronCloudLead` projecting at run speed against 2.0 yd/s clouds (all round 19); the
pet fallback sends a pet into a swinging Crusher's reach (round 17); Crusher reach 4.5 since
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
- r24 a warrior tank keeps Vigilance on the other tank in phase 3: kept (window 39: resets 57-86 a phase 3, was
  16-29; fresh kills 0, was 2.25; taunt-ready mixed, fresh on a raider 1.3 / 1.8 s).
- r25 phase 3 ranged drop the Guardians for Yogg under 12% (Q3): pending.

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
picked only when it ranks first. Phase 3 burn (Q3, 2026-11-25, one window, built in r25): once Yogg is under 12%,
ranged leave the Guardians and put everything on him; judged by Yogg's end and the time from 12% to the tank deaths.
Left alone by choice: the shaman's Reincarnation onto the stack, late Guardians woken by the back line, the raid
working the next Guardian while a low one waits out its gap.

**Open questions.**

- **Q4, Misdirection on fresh Immortal Guardians** (round 24). Should a hunter put Misdirection on a bot tank and
  shoot each fresh Guardian at its spawn, so it opens on the tank instead of the fixed melee? A hit ends the 4 s
  stasis and the redirected threat makes the tank its first victim; today most go to a melee at +4.0 s (window 39:
  13 of 18 and 14 of 17 in B, 10-16 of 18-24 elsewhere), and a warrior's or bear's 30 yd taunt reaches them only as they walk in. Two
  hunters give 4 a minute against 6 spawns, and the tanks' taunts are left for returns, chains and swaps. Hunters
  already cast Misdirection 10-15 times a phase 3, all onto one tank. The risk: a tank that leads by only three shots
  can still lose the Guardian to the healers' threat before its own attacks land. **Recommended: yes, for one window
  after round 25's burn**, if B's fresh Guardians still sit on a raider over 1 s (window 39: 1.3 / 1.8 s).

## Round 25 (2026-11-25, window 39 build 6694fb83e: round 24)

**Data.** Window report `scratchpad/window39/window39/window39-yogg-saron.md` (six pulls, one per pool). Traces
`603_12…1795560525`, `603_7…1795560485`, `603_8…1795560495`, `603_11…1795560520`, `603_10…1795560514`,
`603_9…1795560500` (uld1-uld6) with `--phase3` and the new `--burn`; Vigilance resets (50725 cast rows on the warrior)
counted per phase 3 in B.

Deaths by mechanic (phase 1 / 2 / 3):

- uld1 (A): nova 2 / Death Ray 1 / Immortal Guardian 27, Roar 1, Hand of Sacrifice 1, Drain Life 1; Yogg 3.63%.
- uld4 (A): — / — / Immortal Guardian 31, Drain Life 1; Yogg 4.75%.
- uld2 (B): nova 2 (one melee twice) / — / Immortal Guardian 29; Yogg 12.31%.
- uld5 (B): nova 2 (one melee twice) / Death Ray 1 / Immortal Guardian 26, Gaze 2; Yogg 9.41%.
- uld3 (C): nova 1 / Death Ray 1 / Immortal Guardian 29, Drain Life 1; Yogg 9.70%.
- uld6 (C) kill: — / Crush 1 / Immortal Guardian 2.

**Verdict on round 24 (Vigilance on the other tank): kept**, on its own target in B. `yogg.vigilance` named the bear
in both B pulls; the warrior's Taunt reset 86 (uld2) and 57 (uld5) times in phase 3 (window 37 16 and 29), and it
taunted 36 / 34 times against the bear's 21 / 18. Raiders killed by a fresh Guardian before the first tank death 0 /
0 (2.25 a pull). Not met: `no bot tank with a taunt ready` 1 of 12 in uld2 (8%) but 24 of 35 in uld5 (69%, was
25-50%); fresh Guardians on a raider a median 1.3 / 1.8 s (target under 1). B's first tank death: uld5 +161 s, uld2's
warrior +65 s (back by a rez, again at 11:03); `first off a tank` went to melee 13 / 14, ranged 3 / 2. A and C
unchanged.

**Stop rules.** Not polished: only C has a kill on this build. Not stuck: Q3 is the user's answer, built as asked.

**Picked: the user's Q3 burn** against phase 3's Immortal Guardian deaths (144, every pull). Evidence, `--burn` on
window 39: five pulls crossed 12% at +129-154 s into phase 3; Yogg fell 0.11-0.13%/s in the 60 s before and
0.07-0.15%/s after up to the first tank death; tanks died after the crossing at +11-20 s (uld3), +27-30 (uld5),
+41-43 (uld4), +83-93 (uld1), and uld6 killed at +88 s with both tanks up. `--phase3`'s damage split: ranged
77-112k/s, 54-86% on Guardians over 30% (uld2 32% on tentacles), Yogg 1-9%; after the crossing the ranged bots aimed at Guardians 74-98% of
samples. Cause: `IsAllowedTarget` (`UldActions_YoggSaron.cpp:660-697`) keeps a Guardian over 30% allowed for phase 3
ranged at tier 2, ahead of Yogg's tier 7, to the end. The last 12% is 5.3M; ranged on him make it ~40 s.

**Fix.** `YoggSaronPhase3Burn` (`UldEncounter_YoggSaron.cpp:2106`): a ranged non-healer on the platform in phase 3
sees Yogg alive under `ULDUAR_YOGG_SARON_PHASE_3_BURN_PCT` (12); `yogg.burn` writes `guardians` / `burn` on change.
`ResolveTarget` sets `brain.burn` (`:739`) and the Guardian case (`:675`) then allows one only as a gaze target, any
Keepers. Tentacle tiers stay above Yogg, the gaze exception stays (no cast on Yogg in a gaze), DoTs, pets, drags and
melee unchanged; switching off a Guardian already stops pets on it. Same code at 10 and 25, normal and hard mode; a
human ranged does its own thing. No shared code. Reader: `--burn` (in `--summary`): the crossing, Yogg's fall before
and after, each tank's death from it, the roster's last life, ranged targets and Guardian power after, `yogg.burn`;
`burn_crossing` and `fall_rate` tested. The room gate left `--phases` for `--room` to keep the summary near size.

**Next window should show** `yogg.burn` on every ranged bot within a second of the crossing; ranged bots after it
on Yogg 0.8+ of samples (0.01-0.25), Yogg falling 0.25%/s or more after it (0.07-0.15); a kill where the tanks
live ~40 s past 12% (uld1, uld4, uld6 here), and Yogg's end lower where they don't. Judge the risk by the first tank
death after the crossing against window 39's +11 to +83 s and `Guardians at 90%+ after` (13-48%); back out if the
tanks fall sooner and Yogg ends no lower.

**Seen, not picked.** Phase 1 cost 7 nova deaths, no tick killing two; phase 3 leftovers 1-3 everywhere. Hunters
cast Misdirection 10-15 times a phase 3, all onto one tank (Q4). uld2 bottomed at 12.31% and never crossed.
