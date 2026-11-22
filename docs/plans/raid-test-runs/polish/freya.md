# Freya polish log (25-man hard mode: all three Elders alive)

Reader: `tools/botobs/bosses/freya.py <trace>` (`--waves`, `--tanks`, `--deaths`), written in round 1.
Pulls file under `elder-stonebark` (aliased to `freya`). Hard mode is the banner's Elders line, read off
their essence channels at the engage (from round 4; empowers before), plus the kill chest on a kill; a kill
counts only at 3 of 3. The report's "hard mode" column stays `-` (no Freya key in `cfg.hardmode`).

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

## Round 4 (build 697052475: round 2 built, round 3 merged but not built)

**Data:** `report --boss freya --raid uld --head 697052475 --pulls 10`, one valid pull per pool, no DC veto
inside a pull: `tr-20261122-173632-28` (uld1, DK add tank), `-175417-33` (uld2, bear), `-174354-30` (uld3,
bear); traces `env/dist/logs/botobs/603_{30,35,32}_elder-stonebark_{1795369035,1795370104,
1795369499}.ndjson`. Window 9 is the baseline. Ad hoc scripts: `scratchpad/freya-r4/`.

**Hard mode.** All three pulls 3 of 3 Elders: each Elder cast its essence channel into Freya within 5 ms of
the engage (62483-62485), then Drained of Power at 0:04.9. The uld3 kill cast chest 62952, the core's 25-man
three-Elder chest (`boss_freya.cpp` `summonChestSpell[1][3]`, picked from the Elders Freya found alive at
her engage): a hard-mode kill. The file name: Freya's `JustEngagedWith` calls `SetInCombatWithZone` on each
living Elder, Stonebark first, before her own `OnUnitEnterCombat` hook runs (core `Creature.cpp:2939-2945`),
so Stonebark's engage opens the trace, and `ObsSession::UpgradeBossName` (`RaidObsLifecycle.cpp:594-601`)
never renames a named one. The reader now reads the channels and the chest.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795369035 | wipe 5:28, 31 | Snaplasher 13 (the DK add tank at 1:52.7 by one swing from full, again 5 s after rising, a ranged; the add tank again at 3:56.2, then 9 DPS while loose, both DPS DKs among them), Detonating Lasher 17 (melee 11, Flame Lash 6: second wave at 4:38 with no DK left for Army), Freya melee 1 |
| uld2 | 1795370104 | wipe 10:30 at Berserk, 31 | Berserk 20 (Freya melee 12, Ground Tremor 6, Sunbeam 2), Detonate 6 (melee, 5:04.0-5:06.9), Snaplasher 2 (the bear add tank at 3:19.3, again on rising), Nature Bomb 2, Unstable Energy 1 |
| uld3 | 1795369499 | **kill 6:27, 2** | Snaplasher 1 (a melee 3.5 s into trio 2, before the bear took it at 4:31.9), Nature Bomb 1 |

**Stop rules:** one hard-mode kill in three pools, not polished; no failure picked three rounds running.

**Round 2 verdict: held.** `--beams` against window 9, pooled by role: dodges from past 8 yd heal 12% (49%),
melee 9% (53%), ranged 11% (47%), tank 6% (48%); median walk 6-10 yd; ended farther from Freya heal 33%
(81%), ranged 39% (77%), melee 63% (92%), tank 65% (89%). Healers within 38.5 yd of the Snaplasher's
holder: median 4 in all six trio waves, fewer than 2 for 0/7/0/0/0/1% (window 9 uld2: 26%). Unstable
Energy: 1 death (1); damage rows per beam per 25 alive 7.2 pooled (5.4), victims per beam about flat (4.0
against 3.7), so each hit bot takes 1.8 ticks, not 1.4: a cost to watch, no death. Round 1 still held: no
tank Roar or Shout; one Righteous Defense, on a melee in uld1's collapse.

**Round 3 not built** (`freya.snaphold` declared, never emitted). Its targets here: the loose Snaplasher
after uld1's add tank died (10 deaths, then 17 in a lasher wave with both DKs dead), add tanks killed
within 5 s of rising (uld1 1:57.6, uld2 3:21.8), and uld2's ghoul-held second wave: no lasher in reach 58%,
on Freya 56% (window 9 uld3: 55 / 54; uld3 here 28 / 25).

**Ranking.** (1) The Snaplasher kills its holder, then the raid: 16 deaths plus uld1's 17 lasher deaths
behind them. Round 3 targets the loose half; the add tank's own deaths are question 2. (2) uld2's Berserk,
20: Freya stood at 17% at 10:00 because the Eonar's Gift at 7:17.5 lived 14.2 s and healed her 35% to 82%
(the ranged were 30-60 yd from where it landed), after seven DPS had died, six of them melee to Detonate.
(3) Melee walking back into low lashers' blasts: uld2's 6 deaths and most melee Detonate damage in every
pull. Picked (3): (1) is round 3 plus a plan question, (2) is partly (3) and one late Gift.

**Failure picked: melee walk back into a low lasher's blast right after the bail.**

- uld2 4:57-5:07, the pack on Freya, then loose: 4 lashers under 15% sat 9-14 yd from the melee on Freya.
  Uldyar's orders alternate `freya lasher about to blow` (4:57.1, 5:00.1, 5:01.3, east to x 2367-2369) and
  `reach melee` back to (2360-2361, -42) (4:59.1, 5:01.1, 5:02.0); six Detonates (5:00.4-5:05.0) hit 3-9
  melee each, 6 died.
- All 16 Freya traces: 559 of 862 melee Detonate hits came after the bot bailed within 6 s and was then
  sent back by `reach melee` (11 by `set behind`), 251 while still on the bail's walk. Tanks, who never
  bail, took 299. `--blasts`: 33-57% of melee reach and set-behind orders per lasher wave ended within
  16 yd of a lasher under 15%, three in four of them for Freya; melee inside a low blast 13-57% of
  samples (here 40 / 13, 42 / 37, 48 / 46).
- Code: the bail fires only inside `ULDUAR_FREYA_DETONATE_RADIUS` (`UldTriggers_Freya.cpp:155-170`);
  `StepOutOfBlasts` walks to 16 yd and drops its latch on arrival (`UldActions_Freya.cpp:1218-1247`); then
  `reach melee` (`MeleeCombatStrategy.cpp:15`, `ReachCombatTo`, no hazard check) aims the bot at its
  target inside the blast. The doc's intent is that melee wait outside low lashers ("which is the price").

**Classification: bug.**

**Fix** (`UldDefinition_Freya.cpp` only, any size or difficulty, no player branch): `freya lasher blast
reach guard` zeroes `Family::Reach | Family::Charge` and `freya lasher blast behind guard`
`Family::SetBehind` for a non-tank melee bot whose walk would end within `DETONATE_RADIUS` + 1 (the bail's
own edge) of a lasher under `LASHER_BAIL_PCT`. Reach stops `MeleeDistance` plus both combat
reaches short of the target on the bot's side, set behind that far behind it. The bot idles outside until
the lasher blows (median 2.6 s). Reader: `--blasts`, and the banner's hard mode from channels and chest.

**Cost to check:** melee damage per lasher wave (snapshot `dealt`), intact raid: uld1 77.7, uld2 99.2 and
76.1, uld3 64.3 and 62.9k/s; window 9 51.7 and 55.8. The re-entries mostly got hit before swinging (the
bail re-fires at 15 yd, short of Freya's melee ring), so the loss should be small.

**Next window should show** (`--blasts`, `--tanks`, `--beams`):

- Melee approach orders into a low lasher's blast near 0 (here 52-94 a wave, 37-57%); melee inside a low
  blast well under this window's 37-48% on an intact raid.
- Melee Detonate hits per wave under half of this window's 40-60; no melee Detonate death.
- Melee damage per lasher wave no more than ~20% under 63-99k/s.
- Round 3's checks as written there; rounds 1 and 2 still holding.

**Not picked:** uld2's late Gift (one in 14 that pull; the camp's distance from where it lands is a round 5
candidate if it repeats); uld3's opener swing on a melee before the bear's taunt (1 death).

**Questions:**

2. (pending since round 2, now with data) The add tank dies to capped Snaplasher swings with healers in
   range: capped swings land 18-52k raw on it (DK ~40k health, bear ~55k in form). uld1's DK died from full
   to one 50.9k swing (1:52.7) and to a 44.1k one (3:56.2); uld2's bear to two in a row, 34.3k and 35.5k
   (3:19.3); median 4 healers in range each time. Defensives decided it: the three trio waves the add tank
   held to the end had Survival Instincts, Frenzied Regeneration or Vampiric Blood up; at each death they
   were spent elsewhere or unpressed (DK: Icebound Fortitude, 2 min, at 0:40.8 in a Conservator wave, and
   Vampiric Blood ready but unused both times; bear: Survival Instincts at 0:38 and Frenzied Regeneration
   at 0:25.7 in trio 1, both 3 min, trio 2 at 2:39). Options: (a) the add tank keeps its class defensives
   for the trio waves and spends one once the Snaplasher on it is stacked, as Hodir and Iron Assembly do
   (class defensive nodes held above 25% health all fight, one cooldown per trio wave); (b) the roster's
   highest-health tank as add tank (a DK at 40k still dies to one swing). Recommendation: (a).

**Review:** the cause holds and the guard addresses it, on one code path (no size, mode or player branch).
Reproduced: 559 of 862 melee Detonate hits after a bail then `reach melee`, the `--blasts` rows above, and
77% of blast-bound melee orders over the 16 pulls aimed at Freya. Replaying the guard's endpoint over window
10's lasher-wave orders, it blocks 371 of the 372 `reach melee` orders that ended in a blast and 3 of the 349
that did not. Script: `scratchpad/freya-r4-review/guard_model.py`. Fixed:

- The reach guard zeroed all of `Family::Reach`, which also holds `reach party member to resurrect` (a cat
  druid's Rebirth walks to the corpse) and `reach spell`, neither ending at the melee spot it models. It now
  names `reach melee`, plus `Family::Charge` as before.
- The behind guard tested the point straight behind the target, but `SetBehindTargetAction` walks to the
  nearer of two flank points 3/5 π off the target's facing. It checks both now: 20 of window 10's 20
  set-behind orders into a blast caught (19 before), none of its 126 clear ones.

## Round 5 (build d2874f536: round 3 built, round 4 merged but not built)

**Data:** `report --boss freya --raid uld --head d2874f536 --pulls 10`, one valid pull per pool, no DC veto
inside a pull: `tr-20261122-191816-22` (uld1, DK add tank), `-192040-24` (uld2, bear), `-191959-23` (uld3,
bear); traces `env/dist/logs/botobs/603_{22,24,23}_elder-stonebark_{1795375139,1795375288,
1795375250}.ndjson`. Windows 9 and 10 are the baseline. Ad hoc scripts: `scratchpad/freya-r5/`.

**Hard mode.** All three pulls 3 of 3 Elders from their essence channels. uld1's kill cast chest 62952, the
25-man three-Elder chest: a hard-mode kill, 3 deaths.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795375139 | **kill 6:28, 3** | Unstable Energy 1 (melee, 1:06), Ground Tremor 1 (melee, 1:07), own Iron Roots 1 (ranged, 6:23) |
| uld2 | 1795375288 | wipe 6:25, 31 | Snaplasher 17 (a melee 3.3s into trio 2, the bear add tank 25s in, then 12 non-tanks in three holds and the main tank twice), Freya melee 6 (after the main tank), Detonate 4 (melee), Water Spirit 2 (the bear 0.8s after his soulstone), Nature Bomb 1, own Iron Roots 1 |
| uld3 | 1795375250 | wipe 4:13, 32 | Snaplasher 6 (the bear add tank 15s into trio 2, then 5 in one hold, both DPS DKs among them), Detonating Lasher 23 (the second lasher wave with no DK left for the Army: melee 20, Flame Lash 2, Detonate 1), Freya melee 2, Sunbeam 1 |

**Stop rules:** one hard-mode kill in three pools, not polished. No failure picked three rounds running
(round 2 the add tank's heal range, round 3 the user's answers, round 4 the blast walk-back).

**Round 3 verdict.** (1) The hold: **not held.** The notes fire as built (`on` at each add-tank death, `bark
gone`, `off` once a tank has it), but the bark took 14.6s, 25.7s and 8.9s to run out in uld2 and never did in
uld3 (18.8s, ended when the Snaplasher turned on the main tank), against ~4s expected. Deaths to it during
the holds: 5, 5 and 2 in uld2, 5 in uld3 (window 9 with no hold: 8 and 13). `freya snaplasher hold` never
vetoed `freya set dps priority`. Risen add tanks: uld3's bear soulstoned 0.3s after dying, waited out the
whole hold, growled it still capped at 3:03.6 and held it 22s until it died, so no death within seconds of
rising; uld2's bear died 0.5s after his soulstone to a Tidal Wave, not the Snaplasher. The main tank took it
at each reset and held it with Freya for 6s, then 20s under Shield Wall. (3) The walk to a held pack:
**held where tested.** `--ghouls`, all five held waves: no lasher in reach 1-9%, on Freya 1-6%, 2-13
`freya reach held pack` orders a wave; inside 18 yd 17-51% (windows 9-10's comparable waves 20-95%); no
ranged Detonate death; 1-4 lashers summing 0-26% left when the last ghoul went (windows 9-10's worst
201-388%). No wave this window drifted out of reach like window 9's uld3 second, so the 55/54% case is untested.
uld3's second wave had no Army: both DPS DKs died to the loose Snaplasher.

Rounds 1 and 2 still hold: no tank Roar, Shout or Righteous Defense; Freya off the main tank only in uld1's
opener (DK Dark Command, 1.7s) and after he died. Beam dodges from past 8 yd 3-9% by role; ended farther from
Freya heal 45%, ranged 45%, melee 64%, tank 78% (window 10: 33/39/63/65%); one Unstable Energy death. uld1's
first trio had healers on the DK's side for a median 2, under 2 for 30% of it (window 10: 0-7%), no death.
Round 4 is not in this binary; its baseline here: melee approach orders into a low blast 27-92 a wave
(38-49%), 45-65 melee Detonate hits in each wave with melee alive, 5 melee Detonate deaths.

**Ranking.** (1) The trio-2 chain, 58 of 66 deaths (all from trio 2 on): the add tank dies (question 2), then the loose
Snaplasher kills a non-tank every 2.45s while the hold waits for a reset that does not come, then the main
tank dies holding both (uld2) or the DKs are gone for the next lasher wave (uld3). (2) Melee Detonate, 5,
round 4's target. (3) Scattered singles. The add tank's own death is question 2; the hold not resetting is
picked.

**Failure picked: bots keep hitting the held Snaplasher, so its bark never runs out.**

- Each 64190 cast by the Snaplasher is one direct hit it took. 102 fell inside the four holds; matched by
  millisecond to the hit that caused them, about two thirds came with a bot's own hit.
- Running attacks the multiplier cannot reach: uld3's Uldzau kept Auto Shot on it at 2:43.9, 2:45.7, 2:47.4,
  2:49.4 after his target moved; Uldzak's spider bit it at 2:44.2 and 2:45.7; uld2's healers wanded it at
  5:07.1-5:07.2 and four of them kept it as `current target` through the 25.7s hold. Hurricane (4:45.3) and Mind Sear (4:45.2)
  channels ran on into uld2's first hold.
- Lingering area cast past the 15 yd reach: uld3 Uldzai's Death and Decay at 2:49.243 (17.8 yd from it,
  aimed 16.6 yd) ticked it 11 times to 2:59.2; uld2 Volley at 4:53.4 (19.5 / 20.7 yd) and 4:59.3, procs
  4:55.3 and 4:56.2; Starfall at 5:23.3 (10 yd), procs each second 5:24.3-5:28.3. In a hold the Snaplasher
  walks 25-42 yd, spread 17-22 yd, after its victims.
- Code: the multiplier zeroes only actions aimed at it and `IsRaidAreaDamage` within 15 yd of the caster or
  its aim (`UldMultipliers_Freya.cpp:71-108`), which cannot stop a swing, Auto Shot, a pet or a channel
  already running (`AttackStop` leaves the autorepeat slot, pitfalls.md); Starfall is a self aura, so
  `SpellSplashes` (`RaidSplash.cpp:29-41`) does not call it area.
- What no bot stops: Uldyam's Wandering Plague (unholy talent: a disease tick can hit everything within 8 yd
  of its target), 7 procs in uld2's second hold, his Pestilence having put diseases on it at 5:04.8 and 5:06.1;
  Seed of Corruption and Living Bomb bursts. Dropping the matched bot hits from the traces, the holds still
  reset 5.1-11.8s in (`scratchpad/freya-r5/leaks.py`).

**Classification: bug** (the hold's intent is that nothing hits it); the passive remainder is question 4.

**Fix** (Freya files only, any size or difficulty, no player branch):

- `freya release held snaplasher` (`ACTION_RAID + 2`, at most once a second): while the hold is on (tanks
  exempt once it is free), a bot stops its swing and Auto Shot or wand on it, clears it as `current target`,
  sends its pet to its own target (else stops it), stops other minions on it, interrupts a lingering-area
  channel landing within 30 yd of it, and drops its own lingering auras (Starfall, Bladestorm) within 30 yd.
- `freya snaplasher hold` zeroes lingering area (`IsFreyaLingeringAreaDamage`: a periodic trigger or dummy
  aura with a negative area tick, or a persistent area aura ticking a dummy; plus totems and traps) within
  `ULDUAR_FREYA_SNAPLASHER_HOLD_LINGER_REACH` (30 yd) of the bot or its aim; instant area stays at 15.
- Reader: `--tanks` gives each hold its bark procs and deaths to the Snaplasher, tested.

**Next window should show** (`freya.py --tanks`, `postmortem.py --coverage`):

- Each hold's bark procs well under this window's 23 / 21 / 5 / 53, `bark gone` sooner than 8.9-25.7s, under
  5 deaths a hold; `freya release held snaplasher` fires in each hold.
- No Auto Shot, wand or pet cast at a held Snaplasher after a hold's first second; no Volley, Blizzard,
  Hurricane, Death and Decay or Starfall cast during a hold within 30 yd of it.
- The trio wave still dies around a hold (no wave left hanging on a sibling).
- Round 4's checks as written there; rounds 1-3 still holding.

**Not picked:** the trio-2 opener (uld2's Arms warrior out-threatened the bear's Growl and Maul through
Sweeping Strikes and cleaves on the Storm Lasher beside it and died 3.3s in, as round 4's uld3 melee did;
the threat shed covers picks, not cleaves; one death a window); uld1's first-trio healer range (no death);
Guardian Spirit cast on a mage's Water Elemental (generic healer code).

**Questions:**

2. (pending, this window's numbers) Two bear add-tank deaths, both with four healers in range. uld3's bear
   died 15.5s into trio 2 to 31.6k then 46.0k (~55k health) with nothing left: Barkskin and Frenzied
   Regeneration spent in the first lasher wave (1:05-1:07), Survival Instincts 5s before trio 1, Barkskin
   again at its end. uld2's bear pressed Survival Instincts and Frenzied Regeneration 8s into trio 2; Frenzied
   Regeneration ran out 7s before he died with Survival Instincts still up, to 33.8k and 37.7k 2.4s apart
   (~80k health with it); Barkskin was spent in the Conservator wave. uld1's DK held both waves on Vampiric
   Blood, then Icebound Fortitude. No Pain Suppression, Guardian Spirit, Lay on Hands or Hand of Sacrifice
   went to an add tank all window. Options: (a) the add tank keeps its class defensives for the trio waves
   and spends one at a time once the Snaplasher on it is stacked, so they chain across the wave (Survival
   Instincts 20s, Barkskin 12s, Frenzied Regeneration 10s for a 35-45s wave); (b) the highest-health tank as
   add tank; (c) as (a), plus healers' tank externals on the add tank under a stacked Snaplasher.
   Recommendation: (c).
4. The hold needs 4s with no hit, and an unholy DK's Wandering Plague keeps hitting it while diseases tick on
   it or within 8 yd, which this round cannot stop. Options: (a) keep the hold with this round's release and
   read the holds next window; (b) with no add tank alive, burn it: from 45-75% of 1.36M at ~105-140k raid
   DPS that is 4-10s, 2-4 swings, and it revives without stacks 11s later unless both siblings die in that
   window, for the risen add tank or main tank to take; (c) as (a), plus no Icy Touch, Plague Strike or
   Pestilence on the Snaplasher in trio waves. Recommendation: (a) for one window; if holds still run past
   ~8s, (b).

**Review:** the cause holds and the fix addresses it, on one code path (no size, mode or player branch).
Reproduced: the four holds (14.6, 25.7, 8.9, 18.8s), 68 of their 102 bark procs matched to a bot's hit,
the casts on the held Snaplasher above, and resets 5.1-11.8s in without the bots' share. Over every player
spell in the DBC, `IsFreyaLingeringAreaDamage` matches only the findings doc's list plus Hand of Salvation
(no bot casts it); Army of the Dead's tick is not area. Fixed:

- uld3's paladin main tank's Hammer of the Righteous on the Storm Lasher (2:45.2) chained onto the held
  Snaplasher (Seal of Vengeance on it at 2:45.5): chains other than the named cleaves were not area damage
  to the hold. `IsFreyaChainDamage` (a negative spell hitting 2+ targets: Hammer, Avenger's Shield, Heart
  Strike) now counts at the 15 yd area reach.

Left for the next window's read: a Felguard's Cleave beside it (2 procs in uld2's second hold) and
Soulshatter, whose 50 yd pulse hit it once from 22 yd (uld2 5:59.7), past the 15 yd area reach.
