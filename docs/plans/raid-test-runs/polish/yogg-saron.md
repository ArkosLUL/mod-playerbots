# Polish log: Yogg-Saron (25-man, Freya only)

## State

**Setup.** Trace slug `yogg-saron`, raid `uld`, DC scenario `uld-yogg` (`DcTestDungeonRegistry.cpp:437-450`): drop at
(1866, −24), start (1896, −25, 332.3) 84 yd out, doors close 15 s after the pull. Keeper mask Freya only, written as
instance data by the user's decision, so every kill counts as the hard mode chosen. Reader
`tools/botobs/bosses/yogg_saron.py` (`--summary` is `--phases` and `--burn`; the room gate is `--room`), tests in
`tools/botobs/tests/test_botobs.py` (`YoggPhases`); rounds 1-29 in `history/yogg-saron.md`. Window 35 is the first
on the professions rosters (all eleven at 450, perks and profession enchants), so trends across window 35 and
earlier compare unlike rosters.

**Fight model.**

- **Phase 1** (129-156 s). Clouds orbit at 15.3 / 20.9 / 27.4 / 41.4 / 48.4 / 54.1 yd at 2.0 yd/s (the reader since
  r22; the code and `yogg.cloudreach` still hold the old 11.6-60.8 at 3.0, Q1 step 2). The 21.5 yd station sits in
  reach of orbits 1-3 and wakes 4-6 of a phase 1's 12-17 Guardians. Bot tanks pull Guardians onto the stack (6.5 yd),
  one focus per instance, melee stand in it and eat every nova, the back line holds the station. Sara needs ~11 deaths
  inside 15 yd. A nova gap (6 s after any death, a Guardian under 30% is not killable), a chain rule, a non-tank AoE
  hold and a free low Guardian taking the focus back (r23) space the novas. What kills: two low Guardians side by
  side, one nova from full on a bar of 21.5-23.9k (enhancement shaman, rogues, a warrior), bars left at 62-70%, and
  Guardians piling up off the stack. The handover walk (Sara dead to the ring, 18 s) is where most phase 1 deaths now
  land: 6 of window 51's 8. Melee lost here shrink every brain team, the platform and the phase 3 drag pool.
- **Phase 2** (275-357 s). A portal wave every 80 s; the brain team (10, melee first, exactly one healer) clears an
  illusion room, then hits the Brain until Induce Madness at 60 s, holding at 32% while a tentacle lives and
  finishing at 8 s left whatever the platform holds. The platform never clears a wave (stock grows each cycle), so
  phase 3 opens mid-stun with 1-4 tentacles (Crusher 2.69M, Corruptor 0.81M).
- **Phase 3** (Brain at 30%). With no Thorim no Immortal Guardian dies; one spawns every 10 s at ~40 yd from Yogg
  and leaves stasis at +4 s onto a fixed melee unless a taunt or hit comes first. Ranged stop at 30% (the user's
  floor), so only tank, area and gaze-time melee damage takes a Guardian to Weakened. Phase 3 ranged rank a Crusher
  tier 0 (above Guardians). Shadow Beacon every 45 s marks 3 Guardians that are dragged to lanes (one tank, then
  DKs / paladins, hunters, bears); a mark nobody drags heals the tanks' pile to full and both tanks die in 6-10 s.
  Draggers die mid-drag (16 of 115 paladin, DK and bear drags on windows 49-51, 6 before the heal, 10 in the 10 s
  after, some in collapses), so the pool shrinks each beacon. A DK's second Army of the Dead lands +107-127 s in and its ghouls hold 9-14 Guardians
  for 40 s. Under 12% ranged burn Yogg (r25): his fall goes from 0.09-0.15 to 0.14-0.19%/s; the race is then the
  tanks' life against ~60-75 s to the kill. Tanks die under the pile, 8-14 Guardians after 12%; r29's facing turn
  took the share behind them from 38% to 30% (45% to 32% in gazes).

**Compositions on c9b718c29 (window 51, one pull per pool, r29).** A meets the done rule (uld1 kill, 2 deaths).

| Design | Pools | Outcome: Yogg's end, deaths (leftovers at phase 3) |
|---|---|---|
| A | uld1, uld4 | uld1 kill, 2 (2); uld4 kill, 4 (2) |
| B | uld2, uld5 | uld2 kill, 6 (2); uld5 10.55%, 31 (3) |
| C | uld3, uld6 | uld3 16.41%, 32 (2); uld6 kill, 11 (2) |

Phase 3 to 12%: C uld6 134 s, A 138-153 s, B 153-184 s, uld3 never. First tank death after 12%: A none, uld2
+65 s, uld6 +69 s, uld5 +2 s (both tanks already under 9-12 at the crossing). Window 49 (r28): kills uld3 C 1 and
uld6 C 7 only.

**Composition differences** (22 pulls on windows 35-37; designs per the roster table, 25-man fields 2 tanks, 4
healers, 19 DPS):

1. **B loses its melee in phase 1** more often (4 of 8 pulls lost 3-6 melee in one nova tick; C 1, A 1). No
   composition cause found.
2. **B's tanks (prot warrior + bear) are the weakest phase 3 pair**: two 30 yd single taunts, no Righteous Defense
   or Death Grip; fresh Guardians on a raider a median 1.5-1.7 s a pull on c9b718c29 (A 0.3, C 0.5-0.8); behind
   them 35-36% of the pile time after r29 (A 27%, C 28-31%). r24's Vigilance helps on its own target.
3. **B's and C's ranged deal less in phase 3** (window 51: B 78-82k/s, C 71-82k/s, A 97-104k/s). Not root-caused.
4. **C is closest**: its bear (Growl only) is the weak half.

**Open failures, ranked (c9b718c29, 6 pulls).**

1. Phase 3 Immortal Guardians, 77 in window 51 (121 in window 49): 58 in the two wipes. uld3: beacon 3 found two
   draggers for three marks; two marks healed on the pile (one unclaimed, the bear's Growl missed on the other),
   reaching 11 other Guardians, and both tanks died +6.0/+6.4 s (Q5). uld5: B's late race, tanks dead at the 12%
   crossing.
2. Draggers killed by their marks (window 51: uld3 4, uld6 3): a false `missed` verdict at a Drain Life's end (r30),
   defensives spent before the heal, the return queue (3 marks back on 2 tanks' 8 s taunts).
3. Fresh Guardians on raiders: 10 pre-collapse deaths in window 51 (14 in window 49), mostly ranged and healers at
   +2-12 s of a Guardian's life.
4. Phase 1 at the handover: 6 (uld3 three melee in one tick at 62-64%, 6.6 s after the previous nova; Sara's
   Blessing tails 2); Shadow Nova earlier 2 (one enhancement shaman). Death Ray 1.

Leads, not picked: Army of the Dead ghouls drop 9-14 Guardians at once when they expire; a chain-held Guardian on
the stack waits on a lower one parked under the 35% floor while both bleed (window 37); the DK tank's Death Grip,
the warrior's Challenging Shout and the bear's Challenging Roar are held by the phase 3 taunt hold and never cast
by the control; a swap taker with one taunt (warrior, bear) spends it on a deferral and takes the pile 8 s later
(uld5 holder dead 6.9 s after one at 45%; window 49 uld4 both tanks inside 10 s); draggers press their own
defensive at the taunt, ~10 s before the heal (12 of 26 presses), and find it cooling 45 s later (uld3: two IBFs
and a DP at beacon 1, both DKs dead at beacon 2); 5 of 13
`missed` verdicts on windows 49-51 were taunts that took and lost the mark at the 3 s fade (the dragger deals no
threat while walking); a phase 3 tank keeps its platform target and chases back to it (uld5 window 49); r29
retargets for facing only while every lead is past 1.5. Older leads in history (rounds 17-20): Death Ray walks,
`p1station`'s band test, `YoggSaronCloudLead`'s speed, the pet fallback, Crusher reach 4.5, the Diminish Power
claim's timeout, casts on unkillable Guardians.

**Fixes in place.**

- Fixed or working: r1 bot-led raid walks in; r2 Brain melee spot; r3 Crusher area hold (r15, r20 add more); r4
  phase 3 class taunt hold; r5 ranged DoTs on Yogg, r8 Flame Shock (user); r6 nova ladder counts down; r7 Guardians
  by centre, pulled onto the stack; r8 spent portal clicks; r9 exit cast hold; r10 one healer per brain team; r11
  body detour held on a portal spot.
- r12 stage wait, join within 40 yd (user): fixed; reprisal heal first (user): fires. r13 sight regroup, casts cut
  under a Death Ray: fixed. r14 exit lead 1.2× (user): kept.
- r16 a bot tank breaks Diminish Power (user (b)): kept, 0-2 Crushes on a breaking tank a window, none lethal.
- r17 chain taunt at stasis end: fixed. Burst on phase 3 adds (shared `BurstCooldowns`): fixed on its own check.
- r18 swap deferral for a Guardian on a raider (user): kept; tank deaths within 10 s of one: 2 in window 49, 1 in 51.
  Phase 3 melee held at the spot: fixed.
- r19 back-line dodges stay outside the band: fixed (0 inside, 0 back-line nova hits in windows 29-37).
- r20 AoE hold by `HitsNeighbours`, tank's stack attack killable first: fixed on its target for non-tanks.
- r21 tanks keep their area threat under the phase 1 and leftover hold: fixed (turns 2.5 a minute, Dark Volley 0).
- r22 reader on the measured orbits and the station's cost per orbit (Q1 step 1): reader.
- r22 Diminish Power break once that Crusher's stack is carried below (Q2): kept (window 35).
- r23 a free low Guardian takes the phase 1 focus back outside the handover too: kept (window 37).
- r24 a warrior tank keeps Vigilance on the other tank in phase 3: kept (window 39).
- r25 phase 3 ranged drop the Guardians for Yogg under 12% (Q3): kept (window 51: ranged 0.88-1.00 on Yogg after
  it, his fall 0.14-0.19%/s in the four kills).
- r26 at most living tanks − 1 bot tanks drag a beacon mark: fixed (window 48: every tank dragging 0 of 19 marks).
- r28 no dragger whose pet or summon an unmarked Guardian is hitting: fixed (window 49: pets held 0 of 20 marks);
  window 51's undragged uld3 mark is not this rule's (no candidate left to skip).
- r29 a bot tank faces its pile (`yogg-saron tank pile facing`, gaze `pile`, `pile attack`): kept (window 51:
  behind 30% out of gazes, 32% in, was 38% / 45%; Guardian deaths 121 → 77, kills 2 → 4).
- r30 a drag taunt's `missed` verdict waits 1 s past a Drain Life that hid the switch: pending.

**Tried and rejected.**

- User declined: a beacon-healed Guardian left alone (r3), phase 3 ranged on the lowest Crusher only (r7), a
  ranged-first brain team (r8).
- Round 9's join of any live tentacle: joiners ended 53-85 yd out; narrowed in r12. A low-mana healer sent last
  (r13): withdrawn in r15, no case seen.
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

**Open questions.**

- **Q5 (r30): a beacon mark with no free dragger.** Only a bot with its taunt ready and no unmarked Guardian on it
  claims, so a tank holding the pile never drags. When draggers run out, or a taunt misses with no spare, the mark
  heals on the pile: uld3 (window 51: two draggers for three marks, the bear's Growl missed) and uld5 (window 49:
  three for three, a Dark Command missed) each lost both tanks within 10 s of that heal and wiped at 16-21%. (a)
  Leave it. (b) When a mark has no free dragger, the bot tank holding fewer Guardians drags it, its own Guardians in
  tow, still within r26's one-tank limit: the heal then reaches its few Guardians and spares the main pile and the
  other tank, who holds every fresh spawn alone for ~15 s. **Recommended: (b)**, only when no other bot can claim.

## Round 30 (2026-11-25, window 51 on build c9b718c29: round 29)

**Data.** Reports `scratchpad/window51/window51/window51-yogg-saron.md`, `scratchpad/window49/.../window49-yogg-saron.md`.
Traces `603_8…1795617686`, `603_12…1795617808`, `603_7…1795617615`, `603_11…1795617768`, `603_10…1795617706`,
`603_9…1795617692` (uld1, uld4, uld2, uld5, uld3, uld6) with `--facing` and `--phase3`, window 49's six for the drag
reads; `postmortem.py --death` on uld3's draggers; scratch reads of phase 3 deaths by killer age, `missed` verdicts
and dragger damage before and after the heal.

Deaths by mechanic (phase 1 / 2 / 3):

- uld1 (A) kill: Sara's Blessing 1 (handover) / — / Guardian 1 (in the burn).
- uld4 (A) kill: nova 2 (one enhancement shaman) / — / Guardian 2 (fresh, +85-91 s).
- uld2 (B) kill: — / — / Guardian 6 (ranged 3, two fresh; a tank twice and a melee in the last 12 s).
- uld5 (B) wipe 10.55%: nova 1 (handover) / Death Ray 1 / Guardian 29 (3 before, tanks 11:13-11:17 at the 12%
  crossing, then 23).
- uld3 (C) wipe 16.41%: nova 3 (one tick at the handover, three melee at 62-64%) / — / Guardian 29 (fresh 1,
  draggers 4, then beacon 3 and 24).
- uld6 (C) kill: Sara's Blessing 1 (handover) / — / Guardian 10 (fresh 3, draggers 3, both tanks +69 s past 12%).

**Verdict on r29 (tanks face their pile): kept.** `--facing` to the first tank death, behind share out of gazes A 27%,
B 35%, C 28% (all 30%, was 38%), in gazes A 27%, B 36%, C 31% (32%, was 45%): A and C met the targets, B the gaze
one only. Landed a second front 0.11-0.22, behind 0.23-0.53, front no higher than window 49's 0.17. `yogg.pileface`
7-28 turns on every bot tank, gaze `pile` 7-14 a pull, `pile attack` 2-16. First tank death after 12%: A none (was
+19-56 s), uld2 +65 s (was +22), uld6 +69 s (was +48); uld5 +2 s crossing at +184 s with both tanks under 9-12. No
threat cost: `first off a tank` with a cause 2-4 a pull, fresh-on-raider medians A 0.3, B 1.5-1.7, C 0.5-0.8 s.
Guardian deaths 121 → 77, kills 2 → 4, median deaths 30.5 → 8.5.

**Stop rules.** Not polished: A has its kill (uld1, 2 deaths, Freya-only mask), B's best is 6 (uld2), C's 11 (uld6).
Not stuck: phase 3 Guardians stay first, but r30 picks a new mechanism (beacon drags were r26 and r28, each fixed).

**Picked: draggers lost to a false `missed` verdict.** uld3's wipe came at beacon 3 (10:36): `beaconstage` picked
two (Uldzak, Uldzas) for three marks, the bear's Growl missed, one mark had no claim, and the heal at 10:46.3 reached
11 other Guardians on the pile; both tanks died +6.0/+6.4 s. Every paladin and DK was dead by then: Uldzao in the
handover nova, Uldzaw on its beacon 1 return (+5.6 s, its DP spent 8 s before the heal), Uldzai on its beacon 2
return (+7.0 s, IBF spent at beacon 1), and Uldzam to a verdict that was wrong:

- 9:49.06 Guardian …633 (a mark) starts Drain Life on Uldzaj; 9:49.40 Uldzam's Dark Command on it; 9:51.381 a kick
  cuts the channel; 9:51.390 `yogg.beacondrag missed`; the claim goes to the hunter Uldzau; 9:51.601 …633 one-shots
  Uldzam (33.8k at 100%, the taunt had taken); 9:54.35 Uldzau's Distracting Shot takes …633, which kills Uldzau at
  9:58.8. One verdict, two draggers.
- uld5 10:26.887: the same, Uldvan's Hand of Reckoning, the channel cut 10:26.9, the mark on Uldvan 0.14 s later;
  the claim went to a second paladin.

Cause: `YoggSaronBeaconDragGuardian` (`UldEncounter_YoggSaron.cpp:1708-1720`) waits out a channel that hides the
victim switch (`channelMs` elapsed, `…_TAUNT_LAND_MS`), but the tick the channel ends `channelMs` reads −1 and the
check runs at once; the Guardian picks its victim on its next update, 0.1-0.2 s later. A bug: the doc already says a
running channel defers the verdict. On windows 49-51, 13 verdicts: 2 of these (the reader's new line), 5 taunts that
took and lost the mark at the 3 s fade, 6 that never took.

**Fix.** `BeaconDrag::hiddenMs` stamps each check that sees a channel begun no later than `…_TAUNT_LAND_MS` after the
taunt; the verdict waits until 1 s (`…_TAUNT_LAND_MS`) past the last such check; a new taunt clears it. Unchanged: no
channel, or one begun later (the taunt had its second), checks at 1.5 s. Reader `--phase3`: `missed taunts` (verdicts,
the mark on its dragger within 1 s after, at a Drain Life's end), `missed_verdicts`, tested; window 51 reads uld3
2/1/1, uld5 1/1/1, uld4 2/0/0. Boss doc: the bullet and the trace note. Same code at 10 and 25, normal and hard mode,
human draggers or not. No shared code.

**Next window must show:**

- `missed taunts`: the mark on its dragger within 1 s after 0 and at a Drain Life's end 0 (window 51: 2 of 6).
- No dragger killed within 2 s of a `missed` on its own claim; the hand-off on a real miss at most 1 s later than
  before (`missed` to the next `claim`).
- r29 held: behind under 30% / 32% per composition, first tank death after 12% no earlier than window 51.

**Question.** Q5 in State (a mark with no free dragger; recommended: the tank with fewer Guardians drags it).

**Seen, not picked.** uld3's handover nova at 2:10.3 came 6.6 s after the previous one: three melee at 62-64% had
drawn ~12k of healing between, the healers on phase 1 station and spacing walks. A single-taunt swap taker (uld5's
bear) spent Growl on a deferral at holder 45% and the holder died 6.9 s later, before the take. Draggers press
their defensive at the taunt, ~10 s before the heal; 12 of 26 presses on windows 49-51.
