# Freya polish log (25-man hard mode: all three Elders alive)

Reader: `tools/botobs/bosses/freya.py <trace>` (`--waves`, `--tanks`, `--deaths`), written in round 1.
Pulls file under `elder-stonebark` (aliased to `freya`). Hard mode is the banner's Elders line, read off
their empowers; a kill counts only at 3 of 3. The report's "hard mode" column stays `-` (no Freya key in
`cfg.hardmode`).

## Round 1 (build 61011f323, DC 5443dbc)

**Data:** window 7, first data under the harness, 3 valid pulls, one per pool: `tr-20261122-102749-35`
(uld1), `-103332-38` (uld2), `-102004-32` (uld3); traces
`env/dist/logs/botobs/603_{35,38,32}_elder-stonebark_{1795343325,1795343672,1795342863}.ndjson`. Window
6's uld1 kill (`603_12_elder-stonebark_1795299782`, 7:33, 8 deaths, same Freya code) read as the
comparison: 3 of 3 Elders, so a hard-mode kill. All four pulls had 3 of 3 Elders (Ground Tremor, Iron
Roots and Unstable Sun Beam all fired). No DC veto inside a pull.

Wave order (`--waves`): uld1 lashers 0:10, Conservator 1:10, trio 2:10, dead in the first set; uld2
Conservator, lashers, trio, trio 2:58, Conservator 3:58, lashers 4:58; uld3 Conservator, trio 0:56,
lashers 1:58, Conservator 2:46, trio 3:46, lashers 4:47.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795343325 | wipe 2:59, 33 | Detonating Lasher 13 (9 in the first wave before the collapse), Snaplasher 7 (both tanks), Storm Lasher 4, Conservator 3 (Nature's Fury 2), Sunbeam 2, Ground Tremor 1, Freya melee 1, Tidal Wave 1, Ironroot Lasher (Elder trash) 1 |
| uld2 | 1795343672 | wipe 5:19, 31 | Elder trash 11 (Nature's Blade 7, Guardian of Life 3, Ironroot Lasher 1), Detonating Lasher 8 (second wave, raid already down), Conservator melee 4, Snaplasher 3 (add tank, then 2 melee), Ground Tremor 2, own Iron Roots 2, Freya melee 1 |
| uld3 | 1795342863 | wipe 6:35, 33 | Detonating Lasher 17 (16 in the second wave), Freya melee 6 (one at 0:02, five after the main tank died at 6:09), Nature Bomb 4, own Iron Roots 2, Snaplasher 1 (add tank), Water Spirit melee 1, Ground Tremor 1, Sunbeam 1 |

**Failure picked: generic area taunts break the tank split in trio waves.** The add tank's death opens
the collapse in uld2 and uld3.

- Every mid-fight swap of Freya onto the add tank follows a bear add tank's Challenging Roar by
  0.1-0.3 s: uld2 2:59.8 → Freya on Uldyab 3:00.1-3:05.8; uld3 1:07.9 → 1:08.1-1:11.2; uld3 4:08.8 →
  4:08.9-4:14.6. The act rows are `challenging roar` at rel 26.5, the bear's `high aoe` node
  (`BearDruidStrategy.cpp:138`); `HighAoeTrigger` wants 4 attackers within 8 yd of the current target
  (`GenericTriggers.h:320-323`), and a trio wave beside Freya is 4. The warrior's twin is
  `TankWarriorStrategy.cpp:250-257` (uld2's main tank shouted at 1:34.8 in a lasher wave).
- The paladin main tank's `hand of reckoning` (lose aggro) falls back to `righteous defense`
  (`TankPaladinStrategy.cpp:52-58`), aimed at its current target's victim (`PaladinActions.cpp:65-72`).
  uld3 1:11.4 and 4:12.1 it landed on the add tank holding the Storm Lasher and took the Snaplasher too
  (trio 2: Snaplasher on the main tank 40%, against 6% in trio 1). The bear Growled it back at 4:18.3 at
  full Hardened Bark and died at 4:20.25 to one 43,481 swing from 94.8%.
- uld2: the add tank died at 3:14.2 (Snaplasher 121k in 5 swings, Freya 18k; heals on him 1.8k/s against
  9.0k/s in trio 1). The Snaplasher then killed two melee before the main tank taunted it at 3:29.3;
  with no add tank the next Conservator went to the main tank and the raid fell apart (27 deaths during
  it). uld3: with the add tank dead, nobody picked up the second lasher wave (melee held 33% of lasher
  time, ghouls 26%, against 73% ghouls in wave 1): 16 lasher kills, wipe.
- Control: uld1 and window 6 run a DK add tank, which has no area taunt. Freya never left the main tank
  mid-fight there, and the Snaplasher sat on the add tank 94-99% of each wave in the kill.
- The strategy intends neither: the add tank taunts only the Snaplasher and Conservator, and the lasher
  pickup gates its own shouts on Freya out of reach "or it pulls her off the main tank"
  (`UldActions_Freya.cpp:98-109`).

**Classification: bug.**

**Fix** (`UldDefinition_Freya.cpp` only, any size or difficulty):

- `freya area taunt hold` blocks `challenging shout` / `challenging roar` for every tank while Freya
  lives. The lasher pickup casts its shouts directly, so it keeps them. Both names left `freya lasher
  taunt reserve`, which now only repeats them.
- `freya righteous defense hold` blocks `righteous defense` while the bot's current target is on another
  tank (`FreyaTargetOnOtherTank`). One rescuing a non-tank still runs, as does the pickup's direct cast.

**Not picked:**

- Elder trash (uld2 11 deaths, uld1 1): four packs of Mangrove Ent 33525, Ironroot Lasher 33526,
  Nature's Blade 33527, Guardian of Life 33528 (16 units) stand round the room. The north-east pack's
  Guardian of Life 1421 wanders over (2350-2362, 4-29), 50-70 yd north of the tank anchor. uld2: a Sun Beam
  dodge put melee Uldyax at (2358.5, -11.7) at 3:53 and the pack engaged at 3:54; uld1: a `freya ranged
  camp` walk to (2382, 1) at 2:52 got a ranged killed by the Ironroot Lasher. Question 1.
- Lasher waves (uld1's first, uld3's second): ghouls held 8-10 lashers while alive but were gone 26-28 s
  after the Army with 9 lashers up. Then Eonar's Gift lived 14.5 s and healed the pack: uld1 1:16.5
  (spawned as the Conservator's Grip landed; all 10 ranged on it, 59% left), uld3 5:02.6 (pack 409% →
  801% summed) and 5:47.6. uld3's wave also lacked the add tank. Round 2 candidate if it persists.
- uld3 0:02: an Arms warrior Charged Freya at 0:00.3, before the main tank's Hand of Reckoning at
  0:03.1, and took a 28.6k swing.
- Pull opener (uld1, window 6): the DK add tank's generic Dark Command on Freya at 0:01.2 (its ladder
  ends on her with no add up) put her on the DK 0:03.2-0:05.2, no death. The paladin's Righteous
  Defense on the DK at 0:01.3 (Hand of Reckoning spent at 0:01.1) was its rescue, which took her back
  at 0:01.5; the new hold blocks that rescue too.

**Next window should show** (`freya.py --tanks`, `postmortem.py --vetoes`), on uld2 and uld3 (bear add
tanks); uld1 (DK) is the control:

- No Challenging Roar or Shout from a tank outside a lasher wave, no Righteous Defense on a tank; vetoes
  under `freya area taunt hold` and `freya righteous defense hold`.
- Freya on the main tank throughout, no span on the add tank while both live.
- Each Snaplasher at 90%+ on the add tank in every trio wave (window 7's second waves: 30% and 56%).
- Add-tank deaths to the Snaplasher are not this fix's test (review below).

**Questions:**

1. Should DC's `uld-freya` row also despawn the four Elder trash packs (16 units) before each pull, as it
   does the Swarmers and the Guardian Lasher patrol? Recommendation: yes. A raid clears them on the way
   in, the Elders stay up so hard mode is unchanged, and the bots' hazard dodges otherwise reach the
   wandering Guardian of Life (uld2's 11 deaths started there).

**Review:** the taunts crossing the split are real and the fix stands, but they did not kill the add
tanks, so the collapse they open has a different cause.

- uld2: the Roar's Freya damage (18k) was healed back (Uldyab 100% at 3:03.8). He died 8.4 s after she
  left, to Snaplasher swings of 31.5k, 24.5k and 38.9k plus a 10.9k Tidal Wave: 13.6k/s on him over his last
  10.6 s against 9.2k/s in trio 1, effective heals level (8.1k/s against 9.0k/s), after two `freya dodge unstable sun
  beam` moves (3:03.6, 3:06.8) took him to (2347, -22), 40-48 yd from all four healers (11-36 yd in
  trio 1).
- uld3: Uldzab died to one 43,481 swing from 94.8% (42.4k of 44.7k, no absorb). Trio 1's Snaplasher hit
  him for 29.7-39.1k at full health, and both Snaplashers proc'd Hardened Bark at the same rate, so the
  Righteous Defense swap did not raise the swing.
- Round 2 candidates: the add tank's Sun Beam dodges carrying the Snaplasher out of heal range (bug); a
  ~45k bear add tank against 29-43k capped Snaplasher swings (plan question: tank choice or cooldowns).

## Round 2 (build 1b45df985, DC 2ce3824)

**Data:** window 9, round 1 code plus the Elder trash despawn, one valid pull per pool, all 3 of 3 Elders,
no DC veto inside a pull: `tr-20261122-135748-19` (uld1, DK add tank), `-140131-20` (uld2, bear),
`-140816-23` (uld3, bear); traces `env/dist/logs/botobs/603_{19,20,23}_elder-stonebark_{1795355920,
1795356142,1795356548}.ndjson`. Window 8's three pulls (`603_33_…_1795350333`, `603_32_…_1795350272`,
`603_31_…_1795350071`, wipes) ran a binary built before round 1 merged and serve as extra baseline. Ad hoc
scripts: `scratchpad/freya-r2/`.

Wave order: uld1 Conservator 0:10, trio 0:58, lashers 1:58, Conservator 2:58; uld2 Conservator, lashers
0:58, trio 1:38, trio 2:39; uld3 trio, Conservator, lashers 1:49, Conservator, trio 3:22, lashers 4:23.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795355920 | wipe 3:24, 33 | Detonating Lasher 22 (melee 18, Flame Lash 4; the raid was down to 21 when the wave came), Snaplasher 9 (the add tank at 1:12 and again after he rose at 1:15, then 7 DPS), Unstable Energy 1, Conservator melee 1 |
| uld2 | 1795356142 | wipe 2:54, 31 | Snaplasher 14 (add tank 1:59, main tank 2:14, then 12 others), Freya melee 6 (after the main tank), Water Spirit 4 (Tidal Wave 3), Storm Lasher 4, Detonate 3 (first lasher wave) |
| uld3 | 1795356548 | wipe 7:22, 32 | all in the second lasher wave (4:23), none before: Detonating Lasher 22 (Flame Lash 12, melee 10), Nature Bomb 3, Freya melee 3, Ground Tremor 2, own Iron Roots 2 |

**Round 1 verdict: held.** `freya.py --tanks` lists no Challenging Roar, Challenging Shout or Righteous
Defense from a tank in any pull (window 7: three Roars, two Righteous Defenses on the add tank). Freya
never sat on the add tank while both tanks lived; her only other spans are the uld1 opener (0:01.3-0:02.8,
the DK's Dark Command, as before) and after the main tank died. uld3's bear held each Snaplasher 95% and
100% of its wave (window 7's second waves: 30%, 56%). The add tanks still die: uld1's DK 14 s and uld2's
bear 20 s into the trio, both to the Snaplasher with no taunt involved, which round 1's review expected.

**Trash despawn: worked.** None of the 16 Elder trash units (33525-33528) appear in window 9's traces,
against four of each in windows 7 and 8. No trash death.

**Ranking.** (1) The Snaplasher kills its tank and then the raid: uld1 and uld2, 23 Snaplasher deaths plus
the collapse behind them; the Snaplasher killed its holder in 7 of the last 9 pulls. (2) The second lasher
wave on an intact raid: uld3, 31 deaths. Picked (1).

**Failure picked: the beam escapes take the healers out of range of the add tank.**

- uld2: the healers stood 26-27 yd from the bear at 1:42-1:46. A Sunbeam step-out at 1:46.1 took three
  of them ~11 yd along +x to (2379, -46); an Unstable Sun Beam landed on them at 1:47.9 (0.4-2.2 yd)
  and the dodge walked them 14-16 yd further +x to (2393, -46). Five `reach spell` walks back
  (1:50-1:57) were each undone by a re-dodge from 11.5-12.7 yd (1:52.0, 1:56.5). The bear's last 10 s:
  0.9 healers within 38.5 yd on average, 6.5k of heals against 76k taken (Tidal Wave 12.5k, Snaplasher
  30.1k and 32.4k). The one priest in range (28-33 yd) wanded the trio.
- Window 7 uld2 (round 1's review): the bear's own dodges (3:03.6 from 1.1 yd, 14 yd walk; 3:06.8 from
  11.9 yd) took him 40-48 yd from the healers; 0.1 in range at his death.
- Code: the trigger fires inside `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` 12 yd
  (`UldTriggers_Freya.cpp:147`) and walks to `_SUN_BEAM_CLEARANCE` 15 yd (`UldEncounter_Freya.h:74`),
  while Unstable Energy 62865 never landed past 7.3 yd (1159 applications over ten pulls, p99 6.5). The
  sweep without `preferNear` returns its first clear angle (`EncounterHelpers.cpp:756-757`), angle 0 = +x:
  177 of 338 escapes from on top of a beam went +x and 74% ended farther from Freya.
- `freya.py --beams`, window 9: 32-67% of each role's dodges started past 8 yd; 53-100% ended farther
  from Freya; fewer than 2 healers had the Snaplasher's holder in range for 26% of uld2's first trio
  wave, 9% of uld1's, 1% and 0% of uld3's.
- Not this cause: uld1's DK died with all four healers in range (41.7k healed in one second) to a 7.0k
  Sunbeam plus a 40.4k swing from full, on 43.5k health with about 25 of 71 talents (re-spec before
  window 10). He rose at 1:15 and a 41.7k swing killed him again at 1:19.6.

**Classification: bug.** The 12 yd constant was a recorded open gap against the DBC, and the camp code
already says healers must keep the tanks in range.

**Fix** (Freya files only, any size or difficulty):

- `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` 12 → 8 yd, `_SUN_BEAM_CLEARANCE` 15 → 11 yd (3 yd hysteresis
  kept; the radius + 1 fallback is 9). Brightleaf's own beam (62217) is 4 yd, covered. Every escape that
  routes around beams uses the same clearance.
- Both beam escapes (`freya dodge unstable sun beam`, `freya step out of sunbeam`) pass Freya as
  `preferNear` (`GetFreyaBeamEscapeBias`), in every fallback; ties inside one ring only, so no walk gets
  longer.
- Reader: `freya.py --beams` (dodges per role from past the trigger, walk, farther from Freya; healers
  within 38.5 yd of each Snaplasher's holder), with a test.

**Next window should show** (`freya.py --beams`, `--tanks`):

- Unstable Sun Beam dodges from past 8 yd down from window 9's 32-67% by role to the reading lag: window 9
  read 8-14% of its dodges past its own 12 yd trigger, all but 11 of 55 within a yard of it. Median walk
  at most 11 yd, and far fewer than window 9's 53-100% of dodges ending farther from Freya.
- Healers within 38.5 yd of the Snaplasher's holder: median 3+, fewer than 2 for under 10% of each trio
  wave (uld2: 26%).
- Unstable Energy no worse: 1 death; 36 / 59 / 114 hits on 12 / 9 / 27 beams.
- Round 1 still held: no area taunt or Righteous Defense on a tank.
- An add tank that still dies to the Snaplasher with 2+ healers in range is question 2, not this fix.

**Not picked:**

- uld3's second lasher wave (31 deaths, Army on time at 4:26.3, ghouls 4:27-5:02): the pack sat on Freya
  under the ghouls while the ranged camp, anchored on the lowest-GUID ranged bot, drifted with its beam
  escapes to 30-43 yd off. 43% of ranged ticks under the ghouls had no lasher inside SpellDistance
  (28.5), so `ResolveFreyaDpsTarget` handed them Freya (7.7 of 9 ranged on her at 4:40). The pack was
  at 350% (7 lashers) when the ghouls went, against 15% in uld2's first wave (uld3's first died under
  its ghouls); Eonar's
  Gift (5:03) lived 14.4 s and took it from 262% to 521%. Question 3. Window 10's re-specced DKs change
  this wave's damage too, so read it again there.
- The loose Snaplasher after its tank dies (question 1) and the add tank's health (question 2).

**Questions:**

1. Who takes the Snaplasher once the add tank is dead? Today nobody: uld1's killed 8 in 32 s, uld2's the
   main tank and 11 more. Its 30-43k swings come from Hardened Bark alone, which resets after 4 s without
   a hit, and a tank that rises re-taunts it capped (uld1's DK died 4 s after rising). Options: (a) the
   main tank taunts it and holds Freya too; (b) with no add tank alive, DPS hold damage on the Snaplasher
   until its bark resets, then the main tank or a risen add tank takes it. Recommendation: (b).
2. A bear add tank (42-43k health) against capped Snaplasher swings of 30-43k: two landed swings without
   a heal between kill it. Options: (a) keep the bears and spend a tank defensive (Barkskin, Survival
   Instincts, Icebound Fortitude, Shield Wall, Divine Protection) while the add tank holds a Snaplasher
   past N stacks, as Iron Assembly's punch node; (b) make the roster's highest-health tank the add tank.
   Recommendation: wait for window 10 (heal range fixed, DKs re-specced); if add tanks still die with 2+
   healers in range, (a).
3. While ghouls hold the lasher pack, should ranged with no lasher in reach walk to SpellDistance of the
   pack focus instead of shooting Freya? It is a new position (the reach cap was set so a focus out of
   reach never walks a bot into a blast, but a held pack 30-43 yd off is not chasing anyone).
   Recommendation: yes, only while a ghoul holds the focus lasher.

**Review:** the cause holds and the fix addresses it. Reproduced: Unstable Energy applications max 7.3 yd
over the ten pulls, 177 of 338 on-top escapes +x, uld2's healers re-dodging at 11.5-12.7 yd. 10-man
Unstable Energy (62451) is 4 yd, inside the 8 yd trigger; no size, mode or player branch.

- Reader fixed: `--beams` dropped a dodge episode whose first order went out before the snapshot showing
  its beam (59 of 475 in window 9, 51 of them from on top of it), which inflated the past-8-yd share.
  It now reads that snapshot's beams too; the window 9 figures above are the corrected ones.
- Damage ticks of 62865 land out to 9.8 yd (p95 8.4) because the aura stays on a bot walking out until the
  next update; reach comes from applications, not damage.
- Watch in lasher waves: the Freya bias also pulls the ranged camp's escapes toward the pack under the
  ghouls. Low lashers are in the ranged escape hazards, so read ranged Detonate deaths next window.
- Left for a later round (roots, not beams): `ULDUAR_FREYA_ROOTED_BEAM_RADIUS` 6 yd misses the 80 of 1159
  applications past 6, so a bot rooted 6-7.3 yd from a beam gets no Blink, Hand of Freedom or break.

**User answers (2026-11-22) to round 2's questions:** (1) option (b): while no add tank is alive, DPS stop
hitting the Snaplasher until its Hardened Bark stacks reset, then the main tank or a risen add tank takes it;
(3) yes: while ghouls hold the lasher pack, ranged with no lasher in reach walk to SpellDistance of the
pack's focus lasher instead of shooting Freya. Question 2 (bear add tank health) waits for window 10.

## Round 3 (on Custom d70bd0a5d, no new pulls)

**Data:** none new. Round 2 is merged but not pulled (window 10 crashed before Freya), so no verdict, stop
rule or ranking this round. Built the user's answers 1 and 3 against window 9's pulls (`report --head
1b45df985`, same traces as round 2). Baselines from the new `freya.py --ghouls`, ranged DPS samples while
ghouls held the pack focus: uld3 wave 2 (4:31-4:57) 55% with no lasher inside 28.5 yd, 54% on Freya, 4%
inside 18 yd of one; every other held wave in windows 7 and 9 read 0-19% out of reach, 0-7% on Freya.

**Facts checked for the hold** (DBC CSV, core): Hardened Bark passive 62664 / 64191 has `ProcTypeMask`
139944 = taken melee, ranged and spell hits (magic and none class), no `TAKEN_PERIODIC`; the stack aura
62663 / 64190 lasts 4000 ms (`DurationIndex` 35), cap 99. Window 9's 25-man traces cast 64190 only. No
`spell_proc` override. Damage shields deal damage without procs (`Unit::DealDamageShieldDamage`).
64190 has no speed aura (that is 10-man 62663); the findings doc said otherwise and is corrected.

**Built (1), the Snaplasher hold** (Freya files only, any size or difficulty, no player branch):

- `TickFreya` (new definition tick) latches a per-instance hold, note `freya.snaphold`: on when the
  Snaplasher's victim is not a tank and `GetGroupAssistTank(bot, 0)` is null; off when a tank holds it, the
  Snaplasher dies or a new one spawns. It stays on through the add tank's rise.
- While on: `GetFreyaTrioAssignment` drops it (no fallback onto suppressed siblings); new `freya
  snaplasher hold` multiplier zeroes any non-heal action aimed at it and `IsRaidAreaDamage` with it within
  15 yd of the caster or its target; the hunter dump shot skips it; the add tank's ladder skips it while
  any stack is left.
- After the reset (no 62663 / 64190 aura): a living add tank's ladder taunts it as before; with none, the
  main tank class-taunts it (`FreyaTankAddsAction`) and attacks it while it is on him in melee range,
  ahead of the Storm Lasher.

**Built (3), the walk to a ghoul-held pack:**

- New node `freya reach held pack` (`ACTION_RAID + 1`): a ranged DPS bot whose target is Freya, with no
  lasher inside `SpellDistance` (28.5) while the ranged aim (`GetFreyaRangedLasherAim`, now shared with
  targeting) is ghoul-held, ring-sweeps to the nearest spot within 25.5 yd of the focus and at least 18 yd
  (`ULDUAR_FREYA_LASHER_CAMP_STANDOFF`, the back line's blast clearance) from every living lasher, clear of
  bombs and beams. Latched 3 s, `MOVEMENT_COMBAT`, never cuts a cast; no spot keeps today's behaviour.
- "Held" (`IsFreyaLasherHeldByGhoul`): the focus's victim is an Army ghoul, or a ghoul's victim is the
  focus, which bridges the second after each 10 s threat wipe.
- `freya ranged camp`'s gather stands down for ranged DPS while the focus is held, or it would pull them
  back out of reach; healers keep gathering on the anchor, the low-lasher bail still runs.

**Reader:** `--tanks` lists `freya.snaphold` changes; new `--ghouls` per lasher wave (above), tested.

**Next window should show:**

- (1) After an add-tank death with the Snaplasher alive: `Snaplasher hold on`, `on, bark gone` about 4 s
  later, then the Snaplasher on the main tank (or a risen add tank) and `hold off`; no DPS bot left on the
  Snaplasher through the hold (`postmortem.py --vetoes`: `freya snaplasher hold` never vetoes `freya set dps
  priority`); deaths to the Snaplasher after its tank
  died well under window 9's 8 (uld1) and 13 (uld2); a risen add tank does not die to it within seconds of
  rising.
- (3) `--ghouls`: ranged with no lasher in reach and on Freya well under 55% / 54% in a wave like uld3's
  second; `freya reach held pack` orders present; the inside-18 yd share not up; ranged Detonate deaths
  not up; the pack lower when the ghouls expire.

**Known open, not built:** a third tank shifting into the add tank slot counts as an add tank (no hold,
immediate capped taunt; no Freya comp has one); a Snaplasher past the main tank's 30 yd taunt waits unhit.
Question 2 waits for window 10.

**Review:** both answers are built as asked, on one code path (no size, mode or player branch); the
`--ghouls` baselines reproduce (the other held waves' out-of-reach range corrected above: window 7 uld3's
second read 19%). Fixed:

- `freya snaplasher hold` zeroed `freya set dps priority` and `freya tank adds` too: both are `AttackAction`s
  aimed at the current target, so a DPS bot on the Snaplasher when the hold began could never be moved off
  it, and its auto attack kept the bark up, so the hold never ended. Both pickers are exempt now; neither
  picks a held Snaplasher.
- A tank going to a freed Snaplasher was zeroed as well, and its first hit brought a stack back, which sent
  the add tank's ladder away again (a risen druid's Growl fails in caster form, so the action falls to
  `Attack`). The reset now latches (note `2`, `IsFreyaHeldSnaplasherFree`) and tanks may act on it from then.
- `freya reach held pack` re-ran its collision sweep every tick when no spot cleared the pack; it now waits
  `ULDUAR_FREYA_HELD_PACK_RETRY_MS` (1 s).
