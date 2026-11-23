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

## Round 6 (build ebe746ad1: round 4 built, round 5 merged but not built)

**Data:** `report --boss freya --raid uld --head ebe746ad1 --pulls 10`, one valid pull per pool, no DC veto
inside a pull: `tr-20261122-200637-14` (uld1, DK add tank), `-200417-13` (uld2, bear), `-200818-15` (uld3,
bear); traces `env/dist/logs/botobs/603_{14,13,15}_elder-stonebark_{1795378042,1795377898,
1795378140}.ndjson`. Windows 10 and 11 are the baseline. Ad hoc scripts: `scratchpad/freya-r6/`.

**Hard mode.** All three pulls 3 of 3 Elders from their essence channels. No kill.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795378042 | wipe 10:22 at Berserk, 32 | Berserk 20 (Freya melee 7 from 10:00.6, Ground Tremor 12 at 10:15.6, Sunbeam 1), Snaplasher 8 (the DK add tank at 5:21.3, then 7 non-tanks in a 25.5s hold), Detonating Lasher 2 (ranged, 2:36.9, 3:06.4), Unstable Energy 1, own Iron Roots 1 |
| uld2 | 1795377898 | wipe 5:58, 30 | Detonating Lasher 12 (9 in the second wave's first 41s, 7 of them ranged; 3 in the trio wave to lashers Eonar's Gift healed back), Snaplasher 10 (the bear add tank at 5:08.1, the main tank at 5:29.4, 8 non-tanks), Sunbeam 2, Unstable Energy 2, Freya melee 2 (after the main tank), Storm Lasher 1 (the risen bear), Tidal Wave 1 |
| uld3 | 1795378140 | wipe 4:51, 32 | Detonating Lasher 13 (3 under the second wave's ghouls, a melee and the bear add tank at 3:36.8 after they expired, then 8 healers and ranged 3:46.8-4:40.4 to two low lashers the 3:32 Gift healed back), Snaplasher 7 (the risen bear at 3:53.5, the main tank at 4:36.5, 5 non-tanks), Tidal Wave 4, Unstable Energy 3, Freya 3, Conservator 1, Storm Lasher 1 |

**Stop rules:** no kill, not polished. No failure picked three rounds running (round 4 the melee blast
walk-back, round 5 the hold's bark, now the low lashers).

**Round 4 verdict: held, at no melee damage cost.** `--blasts` per lasher wave: melee approach orders into
a low lasher's blast 6/234, 5/113, 4/105, 23/219, 3/131, 14/241 (2-11%; round 4's window 37-57%, round 5's
38-49%); melee inside a low blast 13-23% of samples (37-48%); melee Detonate hits 13, 12, 1, 9, 2, 1 a wave
(40-65); no melee Detonate death (round 5's window: 5). Melee damage over each wave's first 40s
(`melee_dps40.py`): 74.4, 78.3, 101.0, 71.6, 81.3, 92.6k/s, against 49.6-99.2k on windows 10-11's waves
with the melee intact.

**Round 3's hold without round 5's release** (round 5's baseline; `--tanks`): uld1 25.5s, 35 bark hits, 7
deaths to the Snaplasher; uld2 8.7s / 4 / 2 and 15.3s / 16 / 4 (pull ended); uld3 3.0s / 0 / 0 (the trio
spawned 0.3s after the add tank died to lashers; he rose and took it), 16.7s / 24 / 3 (ended when it turned
on the main tank, no reset), 4.6s / 3 / 1 (pull ended). Round 5's window: 14.6, 25.7, 8.9, 18.8s; 23, 21, 5,
53 hits; 5, 5, 2, 5 deaths. New: uld1's hold began at 5:21.35 with the Snaplasher at 13% and its siblings
at 9-20%. They died at 5:26.0 and 5:26.5 and revived at 5:37.3-5:37.7 while it sat held at 1-5%; the risen DK
took it at 5:47.3, it died at 5:52.5 and revived at 6:04.0, and the trio never died together again. With 18
alive and Gifts living at 6:31, 7:17 and 8:46 (healing all three back to 70-94%), it lasted to Berserk: 27
of the pull's 32 deaths follow from that hold. Question 4.

**Question 2's numbers** (`freya-r5/addtank.py`). Every add-tank death came with its defensives spent in
the wave before: uld1's DK used Icebound Fortitude at 3:26.8 in a lasher wave (ready again 5:26.8) and
Vampiric Blood 11s into trio 2 (off at 4:57.4), and died at 5:21.3 to 31.3k and 34.2k swings 2.4s apart.
uld2's bear pressed Survival Instincts, Frenzied Regeneration and Barkskin at 4:24.8-4:28.8 with four
lashers on him, 4s before trio 2 spawned; all were gone by 4:44.8 and he died at 5:08.1 to 25.9k, 33.0k and
34.8k in 4.9s under Divine Guardian. uld3's bear spent Frenzied Regeneration and Barkskin on lashers at
3:19.6 and 3:26.8, died to lasher melee (11 hits) at 3:36.8 as trio 2 spawned, rose on his soulstone at
3:37.1, got Lay on Hands at 3:37.4 and Pain Suppression at 3:46.8 (the first external other than Lay on
Hands on an add tank in four windows) and died under it at 3:53.5 to 22.6k and 21.0k. Lay on Hands reached
uld1's DK twice, both outside trio 2; Hand of Sacrifice reached uld2's bear at 5:26.5, risen on his
soulstone, who died under it at 5:32.5 to a Lightning Lash.

**Rounds 1-3 still hold.** No tank Roar, Shout or Righteous Defense across the split; Righteous Defense
only on melee (uld1 10:04.8, 10:12.9 after the main tank died; uld3 3:41.5). One Challenging Roar: uld2's bear
at 5:17.1, right after rising, from the generic `challenging roar` node (a `tank assist` act just before
it), so Freya's rules were not running for him (below); Freya never left the main tank. Beam dodges from
past 8 yd: heal 17%, melee 8%, ranged 17%, tank 5% (round 5's window 3-9%); ended farther from Freya heal
39%, melee 59%, ranged 46%, tank 68%. Healers on the add tank in trio 1: median 4, never under 2. Unstable
Energy 6 deaths (1), 3 of them in uld3's collapse. `--ghouls`: no lasher in reach 0-12%, on Freya 0-3%,
0-6 `freya reach held pack` orders; inside 18 yd of a lasher 15-100% (17-51%), from where the waves
spawned (below).

**Ranking.** (1) The trio-2 chain: Snaplasher 25 (8, 10, 7) plus uld1's Berserk 20 that followed from the
hold. The add tank's deaths are question 2, the hold's length round 5 and question 4, the hold's sync loss
question 4 (new option below). (2) Lashers left under the bail line: 10 deaths to low lashers Eonar's Gift
healed back (uld3 8, uld2 2), and uld1's 2:46 Gift healing the packs while the back line bailed from one.
(3) uld2's second wave start: 9 deaths in its first 41s. (4) Singles: Unstable Energy 6, Tidal Wave 5.
Picked (2), the largest cause nothing pending targets.

**Failure picked: low lashers hitting a player are left alone, so the bails never end.**

- `--lows` (new): lashers under the bail line 8s or more while hitting a player, per wave 4, 0, 0, 3, 0, 5 (12
  in three pulls; windows 10-11: 6 in six). Measured from first under 15% to death: median 3.6s, but those on a
  tank median 8.0s (6 of 15 past 8s) and those on the back line 9.7s (4 of 7); windows 10-11: on a tank
  3.6-6.4s (3 of 11 past 8s), on the back line 4 in all, none past 8s. Ghoul-held ones blow a median 3.2s
  (3.4-4.2 before).
- Nothing aims at them: of the ranged DPS samples in reach of a low lasher hitting a player, 31, 29, 40, 5,
  0, 34% shot one and 39, 26, 11, 12, 52, 37% another lasher (the rest the Gift, the trio, Freya). uld3
  3:21.5-3:23.8: 68928 at 13-14% on Uldzah, 7-8 ranged DPS 15.5-28.5 yd off it, every one on another lasher.
  Their pick is `GetFreyaRangedLasherAim`, the pack focus (most lashers within 8 yd, lowest GUID), else the
  nearest lasher in reach (`UldActions_Freya.cpp:558-568` at `a55056c40`); the lowest-health pick
  (`GetFreyaRangedLasherFocus`) only runs for a finishing pack of 3+ (`UldEncounter_Freya.cpp:1098-1134`).
  Melee keep out by design (bail, round 4's guards); tanks hold them and don't bail.
- One hitting a back-line bot runs with it at 8.0 yd/s, so the back-line bail (`freya ranged camp`,
  `MOVEMENT_FORCED`, cast interrupt) can't clear it: 249 bail orders from a low lasher hitting the back line
  (62, 4, 0, 70, 0, 113 a wave; 39 in windows 10-11's six pulls), about 60 of them by its own victim. uld3
  3:19-3:44: lasher 68928 sat at 5-13% chasing Uldzah then Uldzal; Uldzah (the camp anchor) took 8 bail hops
  of 4 yd east at 3:19.9-3:25.5 and the back line drifted to a median 47-57 yd from Freya, the bots around the
  lasher targeting it but moving (`who_on.py`).
- The cost: the Gift at 3:32.2 spawned 79 yd from the ranged, lived 14.5s and healed 68928 and 68934 from
  5-6% to ~65%; with the trio up at 3:36.5 (it outranks lashers) they ran loose and killed Uldzal, Uldzad,
  Uldzak, Uldzae, Uldzaq, Uldzah, Uldzar, Uldzan (3:46.8-4:40.4). uld2: 68983 and 68984, low from 4:29-4:35,
  healed back at 5:15 by the 5:03 Gift (ranged 46-62 yd off, alternating `freya ranged camp` and `reach
  spell`), killed 2. uld1: 68903, low 2:37-2:52 on the back line; the 2:46 Gift lived with all ten ranged on it
  43-56 yd off, none in reach, the anchor Uldxag alternating `reach spell` with bails from it.

**Classification: bug.** The bails and round 4's guards hold every bot outside a low lasher because it
"blows in seconds" (findings doc); nothing makes that true for one off the pile.

**Fix** (Freya files only, any size or difficulty, no player branch): `GetFreyaLowLasherOnPlayer` returns
the lowest lasher under `ULDUAR_FREYA_LASHER_BAIL_PCT` whose victim is a player and that is within
`SpellDistance` of the bot (GUID ties); `ResolveFreyaDpsTarget` gives a ranged DPS bot that ahead of the
pack focus, inside the lasher rung (Gift, Conservator and trio still come first). Ghoul-held lashers keep
the pile's AoE. A bot inside its blast still bails first, then shoots from 18 yd. Reader: `--lows`, tested.

**Next window should show** (`freya.py --lows`, `--waves`):

- Lashers 8s+ under the bail line while hitting a player near windows 10-11's 1 a pull (here 4, 3, 5: uld1's
  first lasher wave, uld2's and uld3's second); ranged in reach of one shooting another lasher near 0%
  (11-52%), on one well up (0-40%); back-line bail orders from one hitting the back line well under 249 for
  three pulls.
- No lasher healed back from under 15% by a Gift; no healer or ranged death to such a lasher in a trio
  wave (10 here).
- Ghoul-held waves no slower: those lashers blow a median 3.2-4.2s after going low.
- Round 5's checks as written there; rounds 1-4 still holding.

**Not picked:**

- uld2's second wave start (9 deaths, 3:37.5-4:13.0): the back line stood where the pack spawned after the
  Conservator wave (back-line centroid (2370, -31), pack (2365, -31), Freya 9 yd off), the Army came 3.8s
  after the spawn behind a Ground Tremor cast (the trigger waits it out by design), three lashers' first
  swings killed Uldyaq at 3:37.46, and with the back line a median 2.5-4.5 yd from a lasher the ghouls held
  2-6 of 9 (wave 1, back line 22-29 yd off once held: 10 of 10). A camp position question if it repeats.
- A risen bot runs generic code until it is back on Freya's threat list: `FreyaAlive` reads `GetFreyaScan
  (botAI).Boss()`, a `find target` threat-list walk, so `freya area taunt hold` and the `tank assist` zero
  were off for uld2's bear at 5:16.8-5:17.1. No swap followed; one Roar.
- Gifts living when the back line starts far off (all nine that healed spawned a median 38-79 yd from the
  ranged; findings doc known-open). This fix shortens the bail half of it.
- Unstable Energy 6, Tidal Wave 5.

**Questions:**

2. (pending, this window's numbers above) The add tank's defensives were spent in the wave before every
   death this window, twice on lashers right before the trio. Recommendation unchanged: (c), the add tank
   keeps its class defensives for the trio waves and chains them once the Snaplasher on it is stacked, plus
   healers' tank externals on it.
4. (pending, this window's numbers above) Without the release the holds ran 3-25.5s, and uld1's shows a cost
   none of (a)-(c) covers: a hold that starts on a nearly dead Snaplasher lets its siblings revive. New
   option (d): no hold while a sibling of its wave is down or the Snaplasher is under the trio sync window
   (30%); the raid finishes it inside the siblings' 11s. Recommendation: (a) with (d) for the next window;
   (b) if holds still run past ~8s with the release.
5. Lashers left when a trio wave spawns: the trio slot outranks lashers and no tank takes lashers during a
   trio wave, so Eonar's Gift heals them and they kill the back line (uld2: four at 73-84% summed at the
   spawn, the last three healed from 52% to 229% at 5:15, 3 deaths; uld3's 8 above). This round's fix finishes low ones before the
   trio comes, not higher ones after. Options: (a) keep the trio first; (b) ranged finish lashers left
   from an earlier wave before the trio slot while their summed health is under one lasher's (100%, ~235k);
   (c) the main tank taunts them. Recommendation: (b).

**User answers (2026-11-22):** Q2 (c): the add tank keeps its class defensives for the trio waves and
chains them once the Snaplasher on it is stacked, and healers put tank externals (Pain Suppression,
Guardian Spirit, Hand of Sacrifice) on it then. Q4: drop the Snaplasher hold; the user would kite it at
high stacks because the stacks slow it. Orchestrator check: only the 10-man stack 62663 slows (aura 33,
-5% a stack); the 25-man 64190 has no speed aura (DBC; the core casts 62664 at both sizes and the 25-man
proc is 64191 -> 64190), so the 25-man replacement went back to the user. Q5 (b): ranged finish lashers
left from an earlier wave before the trio while their summed health is under one lasher's; the user's aim
is that a wave is cleared before the next arrives, so find why it is not.

## Round 7 (on Custom b21cda8ee, no new pulls)

**Data:** none new. Rounds 5 and 6 are merged but not built, so no verdict, stop rule or ranking. Built the
user's Q2 (c) and Q5 (b), and read windows 10-12 (`report --head ebe746ad1`, the traces of rounds 4-6) for
why a lasher wave outlives the next spawn. Ad hoc scripts: `scratchpad/freya-r7/` (`wave_timeline.py`,
`gift_reach.py`, `gift_camp.py`).

**Why a lasher wave is not cleared before the next.** The core spawns the next wave 60s after the last, or
5s after the current set dies (`boss_freya.cpp` `EVENT_FREYA_ADDS_SPAM`, `ACTION_ADD_DIED`). With the raid
intact, windows 10-11's lasher waves died in 31-49s; window 12's ran 85s (uld1 1:56), and past the pull's end
(uld2 3:32, uld3 2:36). In all three an Eonar's Gift lived its full cast mid-wave and healed the lashers
about 60% each:

- uld1: the 2:01 Gift (14.6s) took the pack from 693% to 869% summed; the 2:46 one (14.5s) took the last two
  from 8% and 12% to 66% and 67%, and the next lasher wave spawned on them at 2:56.
- uld2: the trio came on the 60s timer at 4:32.5 with four lashers at 76% summed; ranged went to the trio
  (it outranks lashers), and the 5:03 Gift took the last three from 50% to 228%.
- uld3: the 2:47 Gift took the pack from 560% to 746%; the 3:32 one took three at 15% to two at 130%, and
  the trio spawned at 3:36.5.

The Gifts that lived spawned a median 38-79 yd from the ranged; 0-2 ranged were within 28.5 yd for most of
each life. Ranged move orders during them: `reach spell` against `freya ranged camp`, 47/46, 8/27, 35/12,
23/48. Of those camp orders, 100 were bails from a low lasher on the back line (round 6's target, though a
Gift outranks that pick) and 25 the camp's gather pulling a ranged bot whose target was the Gift back to
the anchor (15, 2, 2, 6). So: (1) a Gift living, because the back line can't reach it (bails, gather,
distance), and (2) the trio taking the ranged off an unfinished tail.

**Built** (Freya files only, any size or difficulty, no player branch):

- Q5 (b): `IsFreyaLasherTail` (living lashers summing under one lasher's max health); while a trio member
  lives, a ranged DPS bot's lasher pick (low one on a player, else the ranged aim, else the nearest in
  reach) goes ahead of the trio slot in `ResolveFreyaDpsTarget`. Out of reach it falls through to the trio.
- Bug, cause (1): `freya ranged camp`'s gather stands down for a ranged DPS bot whose target is a Gift, as it
  does for a ghoul-held pack; the bail still runs. The strategy makes the Gift a ranged job, and the gather
  undid their walk to it.
- Q2 (c): `freya add tank defensive hold` blocks `HeldTankDefensiveNames` for assist tank 0 while Freya lives
  and is not on him, above `ULDUAR_FREYA_ADD_TANK_DEFENSIVE_LAST_RESORT_PCT` (25%). `freya add tank defensive`
  (`ACTION_RAID + 4`): a tank with a Snaplasher of 25+ bark stacks on him (`GetFreyaStackedSnaplasherTank`,
  about 4s into its wave) casts `NextTankDefensive` (physical), which waits while one runs, so they chain.
  `freya add tank external` (`ACTION_RAID + 4`): a healer casts Pain Suppression, Guardian Spirit or Hand of
  Sacrifice on that tank whenever none of the three is up. Notes `freya.adddef`, `freya.addext`; `--tanks`
  lists them, tested.
- Not touched: the Snaplasher hold (Q4, with the user).

**Next window should show** (`freya.py --tanks`, `--waves`, `--lows`; `gift_reach.py`, `gift_camp.py`):

- Q2: no add-tank cooldown in a lasher or Conservator wave above 25% health; in each trio wave a
  `freya.adddef` row about 4s in, then the next as each runs out, and `freya.addext` rows on the add tank;
  add-tank deaths to the Snaplasher under rounds 4-6's 1-2 a pull.
- Q5: no trio spawning onto lashers summing under 100% that are then healed back by a Gift; ranged on the
  tail while it is in reach.
- Gift: no gather order from a ranged bot targeting a Gift; fewer Gifts living 12s+ in lasher waves (window
  12: 5 in its three long waves), and lasher waves done before the next spawn.
- Rounds 5 and 6 checks as written there; rounds 1-4 still holding.

**Questions:**

6. A Gift outranks a low lasher hitting a back-line bot, so the ranged keep bailing from it instead of
   walking to the Gift: 100 of the bails above. Options: (a) keep the Gift first; (b) a ranged DPS bot with
   a low lasher on a player inside `SpellDistance` shoots it before the Gift (12-35k left, one or two casts),
   then the Gift. Recommendation: (b), read after round 6's low-lasher pick is built.

**Review:** the cause holds. Reproduced on window 12 (`wave_timeline.py`, `gift_camp.py`): uld1 693% to
869% at 2:11-2:16 and the 8/12% tail to 66/67% by 3:02; uld2's four lashers at 76% when the trio came at
4:33, then 50% to 228% at 5:14-5:19; uld3's 15% to 130% at 3:39-3:44; 100 bails and 25 gathers (15, 2, 2,
6) during the four Gifts. Windows 10-11's intact lasher waves ran 31-49s. Fixed:

- The tail pick also ran with the trio locked, against "once any trio member is below 30% the trio
  outranks the other adds" (findings doc), so ranged could leave a sync kill for a lasher. It now waits
  for `!trioLocked`, as the Conservator does. uld3's tail (2 lashers at 99%, 4:05) still had ~17s before
  the lock (Water Spirit under 30% at 4:22).
- `freya.adddef` and `freya.addext` were written only on a cast, and `NoteDerived` drops a value equal
  to the bot's last one for that key, so the same button cast in a later trio wave left no row. The
  defensive trigger now notes too (`covered` while one runs, `none` with nothing left), and a healer
  with nothing to cast under a stacked Snaplasher notes `none`; `--tanks` hides both, tested.

Next window, read the Q5 check as "ranged on the tail while it is in reach and no trio member is under
30%".

**User answer (2026-11-22) to Q4, after the 25-man check:** drop the Snaplasher hold (rounds 3 and 5).
Kite a loose Snaplasher only while its live speed is below the kiter's (its stacks slow it at 10-man only);
otherwise, with no add tank alive, the raid kills it. Follow-up (2026-11-23): at 25 man the loose Snaplasher
is killed.

## Round 8 (on Custom 1fa3c332c, no new pulls)

**Data:** none new: round 7 is being pulled in window 14, so no verdict, stop rule or ranking. Built the
user's Q4 answer. Baseline from the reader's new loose view (`freya.py --tanks`) on window 12 (`603_{14,13,15}
_elder-stonebark_{1795378042,1795377898,1795378140}`, round 3's hold without the release) and window 13
(`603_{17,18,16}_elder-stonebark_{1795383500,1795383581,1795383459}`, hold plus release; `report --head
a55056c40`). Ad hoc scripts: none.

**What the code did with a loose Snaplasher before this round** (Custom 1fa3c332c, any size):

- Kite: none. Nothing moved a bot away from a Snaplasher.
- Hold (rounds 3, 5): `TickFreya` latched it once the Snaplasher was off every tank with no assist tank 0
  alive; then no bot hit it (multiplier on aimed, area within 15 yd, chain, lingering within 30 yd; the
  release node stopped swings, Auto Shot, wands, pets, channels and own lingering auras), the trio split and
  the redirect dropped it, and the add tank's ladder skipped it while stacked. Once no stack was left
  ("free"), a living add tank's ladder taunted it, else the main tank class-taunted it and attacked it while
  it was on him in melee range, ahead of the Storm Lasher.
- Target order: DPS split over the trio (held one out), with `ShedSnaplasherThreat` moving a bot on it to the
  healthiest sibling at 80% of the highest living tank's threat after the 5 s lead, whoever held it.

**Facts checked:** `spell.reference.csv`: 62663 has aura 79 (+10%) and aura 33 (`EffectBasePoints` -6,
`DieSides` 1: -5% a stack), cap 99; 64190 has 79 and 61 (scale), no 33. Core `Unit::UpdateSpeed` applies the
strongest `MOD_DECREASE_SPEED` and `AuraEffect::CalculateAmount` multiplies it by the stacks, so 10-man runs
8.0 x (1 - 0.05 n): under 7.0 from 3 stacks, 0 from 20 (`SetSpeed` clamps the rate at 0). `speed_run`
1.14286 for 32916 (base `creature_template.sql`): 8.0 yd/s against a player's 7.0. Under 30% health
`UpdateSpeed` also slows a creature `(int)(1.66 x (hp - 30))` % unless snare-immune or a dungeon boss; the
Snaplasher's immunity set -269 has no SNARE and `flags_extra` 0, so 25-man drops under 7.0 at -13%, about 22%
health.

**Built** (Freya files only, any size or difficulty, no player branch):

- The hold is gone: `TickFreya` and its instance state and note `freya.snaphold`, `GetFreyaHeldSnaplasher`,
  `IsFreyaHeldSnaplasherFree`, `freya snaplasher hold` (with `IsFreyaLingeringAreaDamage`,
  `IsFreyaChainDamage` and both reaches), `freya release held snaplasher` (`FreyaStopHittingHeldSnaplasher`),
  and its skips in the trio split, the add tank's ladder and the redirect. The main tank's taunt of a freed
  one and his attack on a Snaplasher on him go too (round 2's option (a), not chosen).
- `freya kite snaplasher` (`ACTION_RAID + 3`): a non-tank bot a Snaplasher is hitting
  (`GetFreyaSnaplasherToKite`) whose live `GetSpeed(MOVE_RUN)` is above the Snaplasher's walks, once it is
  inside the Snaplasher's melee range + 5 yd, to the nearest spot 10 yd past that range, toward straight away,
  clear of the escape hazards, on a walk that never crosses its swing; `MOVEMENT_FORCED`, cast cut, latched
  1 s. A melee kiter's trio pick goes to the healthiest sibling (none: down the ladder), and `freya
  snaplasher kite reach guard` / `behind guard` block its `reach melee`, charges and set behind ending inside
  that margin. Note `freya.kite` on the chased bot: "<Snaplasher> vs <own> run speed", "" once off it.
- `FreyaShedsSnaplasherThreat` sheds only while a tank other than the main tank holds it. Without that, any
  main-tank threat on a loose one (Consecration, a chain) made every DPS on it shed to a sibling, a hold by
  another name.
- Reader: `--tanks` lists each stretch a Snaplasher spent on a non-tank (deaths to it, bark hits, kite orders,
  speed reads), tested; the hold rows are gone.

**After the change** (one code path, the speeds decide):

- 10-man: a loose Snaplasher on a non-tank bot is kited from 3 stacks and stands still from 20; the raid's
  hits keep the stacks up, so its victim steps out and the raid kills it in the split. Under 3 stacks (8.0-7.2
  yd/s) its victim takes it. The opener (a DPS pulls it before the add tank's taunt) kites the same way.
- 25-man: no slow above 30%, so no kite: it kills its victim and the next while the raid kills it in the
  split, the sync floor unchanged. Under about 22% it drops below 7.0 and its victim kites it, the sync
  floor's wait included.
- Both: a risen add tank taunts it at once, stacked or not; the main tank never takes it on purpose.

**Baseline** (loose stretches, deaths to the Snaplasher): window 12 uld1 5:21.5-5:47.3 at 9% to 1%, 7 (its
siblings revived meanwhile); uld2 5:08-5:54, four stretches, 8; uld3 3:53.6-4:10.0 3 and 4:36.8-4:46.8 2.
Window 13 uld1 none; uld2 one 1 s opener, 0; uld3 2:03.7-2:31.8 at 70% to 61%, 11, plus short stretches
with 2.

**Next window should show** (`freya.py --tanks`, `postmortem.py --vetoes`, `--coverage`):

- No `freya.snaphold` note and no `freya snaplasher hold` or `freya release held snaplasher` row.
- 25-man loose stretches above about 22% with `freya.kite` reads "8.00 vs 7.00" and no kite order, bark
  hits climbing (the raid keeps hitting it), and the stretch ending with its death or a tank, not a 20 s+
  wait at 1-9% (window 12 uld1). Under about 22%, reads below the bot's speed and kite orders from its victim.
- A 10-man check pull with a loose Snaplasher: reads under 7.0 from 3 stacks, kite orders, near 0 deaths
  to it. Melee kiters: vetoes under the two kite guards, no `reach melee` into it.
- Round 7's checks as written there; rounds 1-6 still holding.

**Questions:**

7. "The raid kills it" is built as: the loose Snaplasher stays in the DPS split and under the sync floor,
   with nothing extra on it. The other reading is a burn (every DPS on it first): 4-10 s to kill, but its
   siblings rarely die inside its 11 s revive, so it comes back and the wave restarts its sync.
   Recommendation: keep the split.

**Review:** built as the user answered, on one code path (no size, mode or player branch), no shared code.
Reproduced: the baseline stretches (`--tanks` on windows 12-13, adjacent stretches summed), the DBC (62663
auras 79, 33 at -5 a stack and 61; 64190 79 and 61) and the core (`UpdateSpeed`'s health slow, the slow
aura times its stacks in `CalculateAmount`, `SetSpeed` clamping at 0). Fixed:

- `FreyaShedsSnaplasherThreat` never shed while a non-tank held the Snaplasher, add tank alive or not, so
  a damage dealer ripping it off a living add tank after the head start put every bot on it back on it,
  and the next one took it once he died, while the taunt cooled down (before round 3 they shed). It now
  measures against the living add tank's threat alone (`GetGroupAssistTank(bot, 0)`); none, or none on
  it (risen, not yet taunting): the pick stands. The main tank's stray threat still never counts.
- `freya.kite` rounded to tenths, so 25-man's first read under 7.0 (6.96 at 22%) printed "7.0 vs 7.0"
  beside kite orders. It writes hundredths now.
- The kite's swing circle went negative with the bot within 0.25 yd of the Snaplasher's centre, and
  `SegmentCrossesCircle` squares the radius, so every walk was refused. Clamped at 0.

Left: a melee kiter whose sibling stands inside the kite margin idles (its reach is blocked) rather than
going down the ladder; the 5 s head start still sheds with no add tank alive (a trio spawning after his
death, as before round 3); `--tanks` splits one loose run wherever the Snaplasher briefly has no player
target, so sum adjacent stretches.

## Round 9 (build 1fa3c332c: round 7 built, round 8 merged but not built)

**Data:** window 14, `report --boss freya --raid uld --head 1fa3c332c --pulls 10`, one valid pull per pool, no
DC veto inside a pull: `tr-20261123-103707-22` (uld1, DK add tank), `-104450-24` (uld2, bear), `-104429-23`
(uld3, bear); traces `env/dist/logs/botobs/603_{22,24,23}_elder-stonebark_{1795430269,1795430731,
1795430711}.ndjson`. Windows 12 and 13 are the baseline. Ad hoc scripts: `scratchpad/freya-r9/` (`extcasts.py`,
`defhp.py`, `tankhp.py`, `healpos.py`, `gift_gathers.py`), plus `freya-r5/addtank.py`, `freya-r7/`,
`freya-r6/low_life.py`. Round 8 is not in this binary: the hold still ran, so it is judged by nothing here.

**Hard mode.** All three pulls 3 of 3 Elders from their essence channels. uld1's kill cast chest 62952: a
hard-mode kill, 3 deaths.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795430269 | **kill 7:11, 3** | Detonating Lasher 3 (ranged Flame Lash 1:07, ranged 4:46, melee 5:29) |
| uld2 | 1795430731 | wipe 4:21, 31 | Flame Lash 2 (the bear add tank and a melee, 0:20.0); trio 1: the bear add tank at 2:32.6 to the Snaplasher with nothing up, then the held Snaplasher at 2-7% killed 3 melee (2:35-2:40) while its siblings revived (2:43, 2:45.8) and so did it (2:56); the 3:01 lasher wave landed on the live trio: 25 (Flame Lash 8, lasher melee 9, Storm Lasher 3, Snaplasher 2, own Iron Roots 2, Freya 1, the main tank at 4:13) |
| uld3 | 1795430711 | wipe 5:34, 29 | lasher melee 1 (ranged, 1:00.6); trio 2: the bear add tank at 3:20.9 to the Snaplasher with nothing up, then the held Snaplasher at 2-3% killed 3 melee (3:28-3:35) while its siblings revived (3:38.6); the Conservator (3:44) and lasher wave (4:44) landed on the live trio: 24 (lasher melee 8, Flame Lash 7, Freya 7 with the main tank dead at 5:22.7, Nature's Fury 1, own Iron Roots 1) |

**Stop rules:** one hard-mode kill in three pools, not polished. The add tank's death was picked in round 2
(heal range) and is again here; round 7 built answers, not a pick.

**Round 7 verdict.**

- Q2, the add tank's own buttons: **held.** No held button above 25% in a lasher or Conservator wave but one
  case: uld2's bear, risen on his soulstone at 0:20.3, cast Barkskin and Survival Instincts at 43% (0:21.4)
  with no veto, because the hold read Freya through `find target`, and a risen bot is not on her threat list
  (round 6's known gap). Last resorts under 25%: uld3's Survival Instincts under lashers (1:03.8, 15%),
  Barkskin (3:57, risen) and Survival Instincts (5:15.9) in the collapse. `freya.adddef` 2.6-7.3s into each
  trio wave with a button ready (uld1's second: Vampiric Blood 25s in, when it came off cooldown), the next
  1.4-1.5s after the last ran out.
- Q2, the healers' externals: **not held.** New `--tanks` cover line, own / external / both / none seconds
  per trio wave: uld1 22.0/8.0/8.0/15.3 of 37.3 and 10.0/0/0/30.2 of 40.2; uld2 22.0/20.0/19.5/9.1 of 31.6,
  died bare; uld3 22.0/0/0/20.0 of 42.0 and 12.0/0/0/24.4 of 36.4, died bare (windows 12-13: none 10-45s a
  wave). Three faults: (1) externals ran alongside his own buttons (uld1 Pain Suppression with Icebound
  Fortitude; uld2 Hand of Sacrifice with Barkskin, Pain Suppression with Frenzied Regeneration), so uld2's
  bear was bare 5.5s later; (2) class code spent them first: uld3's disc priest Pain Suppression on a melee
  and holy priest Guardian Spirit on a ranged at 1:01.5-1:03.8 in the lasher wave, uld2's holy priest Guardian
  Spirit on a warlock's Felguard at 0:07.3, so uld3 got none at all; (3) uld1's holy paladin noted Hand of
  Sacrifice 20 times and never landed one: he healed from 35-42 yd (`healpos.py`), it reaches 30, and
  `CanCastSpell` passes `SPELL_FAILED_OUT_OF_RANGE` (`PlayerbotAI.cpp:3617`).
- Add-tank deaths to the Snaplasher 0/1/1 (rounds 4-6: 1-2 a pull), both bare with the Snaplasher at 7-10%
  and its siblings at 3-19%.
- Q5: no trio spawned onto a lasher tail (every intact lasher wave ended before the next spawn): untested.
- Gift: **held.** `gift_camp.py` counts 3 and 7 gathers from a bot targeting the Gift, but every one had a
  lasher under 15% 9.6-16 yd off (`gift_gathers.py`): bails at the blast edge its 15.5 yd cut misreads. Gifts
  living 12s+ in intact lasher waves 2 of 4 (window 12: 5 in three); lasher waves 46-70s, each done before
  the next spawn. Both of those Gifts healed a ghoul-held pack's lashers from 4-11% back to 52-94% (uld1
  1:28-1:33, uld2 0:40-0:46) with 9-10 ranged on the Gift.
- Round 6: **held.** `--lows` lashers 8s+ under the bail line on a player 0/0/1 (4/3/5); ranged in reach on
  one 86/89/76/40%, on another 0/2/8/10%; back-line bails 110 over the pulls (249). `low_life.py` median 4.0s,
  ghoul-held 3.7s (3.2-4.2). Its "no low lasher healed back by a Gift" failed twice (the Gifts above).
- Round 5: the release fired in each hold (uld2 89 times by 22 bots, uld3 7), but both holds began on a
  Snaplasher at 7-8% whose siblings then revived: question 4 (d)'s cost, gone with round 8.
- Rounds 1-4: no tank Roar or Shout; Righteous Defense on a pet (uld1 0:04.7), a melee (2:01.8) and two
  ranged in uld3's collapse. Freya on the main tank 99.2/98.8/94.1% (uld3 off him from 4:49.6, in the
  collapse). Beam dodges from past 8 yd 0-24% by role; ended farther from Freya heal 62% pooled (round 5's
  45%). `--ghouls` no lasher in reach 0-9%, on Freya 0-2%. `--blasts` melee orders into a low blast 0-17%,
  Detonate hits 0-14 a wave, no melee Detonate death.

**Ranking.** (1) The trio chain, both wipes: 57 of 60 deaths from the add tank's death on. The add tank dies
bare at the wave's end (2); the held Snaplasher at 2-7% kills 6 while its siblings revive (round 8 drops the
hold); the next waves land on a live trio (49). (2) Lasher deaths on an intact raid: 6. Picked the add tank's
death, the part round 8 does not touch.

**Failure picked: the add tank dies bare at the end of a trio wave; the externals add no cover.** Evidence
above. Each death needed one layer: uld3's bear took 31.4k at 91% (3:18.5), was healed to 63%, then 40.4k
(3:20.9); Pain Suppression (40%), Guardian Spirit or Hand of Sacrifice (30%) each turns that into a survival.

**Classification: bug.** Q2 (c) is (a), keep the add tank's buttons for the trio and spend one at a time so
they chain, plus the healers' externals under a stacked Snaplasher; nothing kept the externals, and round 7
cast them in parallel.

**Fix** (Freya files only, any size or difficulty, no player branch):

- `freya add tank external hold` (Role::Heal) blocks the class `pain suppression`, `pain suppression on
  party`, `guardian spirit on party` and `hand of sacrifice on party` while Freya lives and an add tank does
  who is not holding her (`IsFreyaAddTankExternalHeld`). Lay on Hands is not in the user's list and stays free.
- `IsFreyaAddTankCovered`: the externals wait while one of the three is up or the tank has an own button
  running or ready (`RunningTankDefensive` / `ReadyTankDefensive`, physical), so one layer runs at a time.
  `freya.addext` notes `covered` then.
- `GetFreyaTankExternal` checks spell range (`GetMaxRange(true)`) and LOS before `CanCastSpell`.
- Both holds find Freya by entry (`GetFreyaBossByEntry`) when the scan misses her, so a risen add tank keeps
  his buttons.
- Reader: `--tanks` cover line per Snaplasher, from auras (the `freya.addext` note precedes a cast that can
  fail), tested.

**What it would have changed here:** uld2 keeps Survival Instincts and Guardian Spirit: Barkskin, Frenzied
Regeneration, then Survival Instincts from ~2:24 to ~2:44, past the 2:32.6 swing, with three externals still
in hand. uld3's first trio gets Pain Suppression and Guardian Spirit after Frenzied Regeneration
(its 20s bare), but its second still gets Barkskin alone (question 8). uld1 unchanged but for no Hand of
Sacrifice tries and Pain Suppression after Vampiric Blood instead of on Icebound Fortitude.

**Cost to watch:** healers' Pain Suppression, Guardian Spirit and Hand of Sacrifice no longer go to DPS,
healers or the main tank while an add tank lives (window 14: 7 such casts, none on a main tank, 2 of them in
uld1's kill on a healer who lived), and a disc priest's own critical-health Pain Suppression shares the
held name.

**Next window should show** (`freya.py --tanks`, `extcasts.py`, `postmortem.py --vetoes`):

- Cover lines with `both` near 0 (window 14: 8.0, 19.5s) and `none` well under 9-30s in single and first trio
  waves; externals landing only after his own run out.
- No healer Pain Suppression, Guardian Spirit or Hand of Sacrifice on anyone but a Snaplasher's tank while the
  add tank lives; vetoes under `freya add tank external hold`.
- No `freya.addext` name without the aura landing (no out-of-range tries); `covered` notes in trio waves.
- A risen add tank: vetoes under `freya add tank defensive hold` within a second of rising, no button above 25%.
- No veto under either hold before Freya's engage or after the sixth wave.
- Add-tank deaths to the Snaplasher under 1 a pull; any left should be the second of back-to-back trio waves.
- Round 8's checks as written there; rounds 1-7 still holding.

**Not picked:** lasher deaths on an intact raid (6, no common cause); Gifts healing ghoul-held packs from
under 15% (two this window); `FreyaAlive` (area taunt hold, owned targeting) still reads the scan, so a risen
bot runs generic taunts and targeting until Freya has him on her threat list.

**Questions:**

8. Two trio waves can come back to back, 60s apart (uld1 and uld3 this window). The add tank's 3-minute
   buttons (Frenzied Regeneration, Survival Instincts; Icebound Fortitude 2) and the healers' externals cover
   one trio wave, not two: uld3's bear held the second on Barkskin alone and died bare 36s in. Options: (a)
   accept it; (b) DPS leave the Snaplasher alone until both siblings are near the sync floor, so its bark
   stays down (stacks drop 4s after the last hit; 7 stacks at +2s, capped by +15s), then burn it: the add tank
   takes capped swings for ~15s instead of ~35s; the risk is the sync (the findings doc records that withheld
   Snaplasher damage, in a hold with no tank, kept the trio from dying together); (c) the main tank takes the
   second wave's Snaplasher while the add tank's buttons are down. Recommendation: read window 15 first; if
   the only add-tank deaths left are second waves, (b).
9. (reviewer) The external hold keeps Pain Suppression, Guardian Spirit and Hand of Sacrifice off the main
   tank for the whole add phase, since one spent on him is not back for the next trio; Q2 (c) named the add
   tank only. Options: (a) keep it; (b) let the class code cover the main tank under Freya alone. Window 14
   sent none to a main tank, whose only deaths came in collapses. Recommendation: (a).
6. and 7. (still open, not built) as written in rounds 7 and 8.

**Shared code:** none changed; `RunningTankDefensive` and `ReadyTankDefensive` are read, not edited.

**Review:** the traces bear the causes out (uld2's risen bear cast Barkskin and Survival Instincts at
0:20.9-0:21.4 with no veto; uld1's paladin stood 35-42 yd from the add tank through both trio waves; the
cover lines reproduce), on one code path (no size, mode or player branch), no shared code. Fixed:

- The entry fallback finds Freya before the pull too, and hold rules run whenever the gate is open, so
  within 100 yd of her both holds ran on trash; the external hold also ran on after the sixth wave with no
  Snaplasher left (uld1's last 1.5 minutes). Both now need Freya in combat with Attuned to Nature up.
- `--tanks`' "died under" missed the auras a death strips: the trace logs their removal 1-2 ms before the
  death row (windows 13-14). It takes 50 ms of slack, tested.
- The reader's new docstrings and constant comment went (comments rule); the Last Stand id keeps a hazard
  line.

Left: externals also wait on an own button that is off cooldown but unaffordable (a DK's Vampiric Blood
with no blood rune, up to one rune cooldown), since `ReadyTankDefensive` reads the cooldown only; changing
it is shared code.

**Shared fix (`rt/shared-ready-defensive`):** `ReadyTankDefensive` now skips a button the tank can't pay for now.

**Shared fix review:** window 15 (round 9 built) bears it out. In `603_11_elder-stonebark_1795437270` the DK add
tank's Icebound Fortitude fell at 71.16 s (`freya.adddef` `none` 71.35 s), Heart Strike read `IMPOSSIBLE` or
`FAILED` to 74.38 s (no Blood rune) and his first Vampiric Blood went at 74.61 s, while every healer's
`freya.addext` stayed `covered` from 59.4 s to 84.7 s: 3.5 s bare with the externals held (again 215.24-216.07
s). Fixed: the cost check ignored `withinMs`, so the look-aheads (Algalon's `DefensiveHandoverDue` 5-20 s out,
`SoakCovered`, the soak note, the prep check; Yogg-Saron's 30 s Wrath hold) read a rune or runic power spent
this tick as no button then, and a DK holder (two of three Algalon pools) would be handed over for nothing. A
rune now counts once its cooldown ends within `withinMs`; other costs are judged only at 0. Watch: the add
tank's own button doesn't wait out a running external, so an external spent in a rune gap gets Vampiric Blood
on top once the rune is back.

**User answers (2026-11-23):** Q6 yes, as put to the user: a ranged DPS bot with a low lasher hitting the
back line in reach shoots it before the Gift. Q7 yes: the loose Snaplasher stays in the normal damage split (as built). Q9 no: do not keep
the healers' externals off the main tank during the add phase. Q8 waits for window 15.

## Round 10 (build c350347ff: round 8 built, round 9 merged but not built)

**Data:** window 15a, one valid pull per pool at each size, no DC veto inside a pull. 25-man:
`tr-20261123-113618-9` (uld1), `-113537-8` (uld2), `-113516-7` (uld3); traces `env/dist/logs/botobs/603_{9,8,7}
_elder-stonebark_{1795433820,1795433778,1795433758}.ndjson`. 10-man check (2 tanks, 2 healers, 4 melee, 2
ranged): `tr-20261123-115845-15` (uld1), `-115804-14` (uld2), `-115422-13` (uld3); traces `603_{15,14,13}
_elder-stonebark_{1795435160,1795435120,1795434900}`. Window 14 is the baseline. Ad hoc scripts:
`scratchpad/freya-r10/` (`loose.py`, `low_vs_gift.py`), plus `freya-r7/gift_reach.py`, `wave_timeline.py`,
`trio_hp.py`. No ranking this round (window 15 is pulling round 9): the user's answers are built.

**Hard mode.** All six pulls 3 of 3 Elders from their essence channels; the 25-man kills cast chest 62952,
the 10-man kill 62950.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 25 | 1795433820 | **kill 6:31, 5** | Unstable Energy 2 (melee, 4:00-4:01), Nature Bomb 2 (6:18-6:19), own Iron Roots 1 |
| uld2 25 | 1795433778 | **kill 6:45, 3** | Nature Bomb 2, Flame Lash 1 (ranged, 4:32) |
| uld3 25 | 1795433758 | **kill 6:18, 4** | Detonating Lasher 3 (melee Detonate 2:29.8; melee Flame Lash and ranged lasher melee 4:32.5), Nature Bomb 1 |
| uld1 10 | 1795435160 | **kill 8:51, 0** | none |
| uld2 10 | 1795435120 | wipe 5:26, 11 | Snaplasher 3 (loose), Tidal Wave 2 (the add tank 4:27, the main tank 5:15), Ground Tremor 2, lasher melee 2, Conservator 1, Water Spirit 1 |
| uld3 10 | 1795434900 | wipe 6:51, 12 | Detonating Lasher 4 (a melee 5:23, healers 5:33, 5:44, 5:53), Freya 7 from 6:11, the main tank at 6:18 among them (melee 4, Ground Tremor 2, Sunbeam 1), Nature Bomb 1 |

**Stop rules:** a 25-man hard-mode kill on all three pools, but 5 and 4 deaths in two: not polished.

**Round 8 verdict: held where tested; the 25-man case is untested.** No `freya.snaphold` note and no hold or
release row in any trace.

- 25-man: no add tank died, so nothing ran loose but three openers of 1 s (0 deaths). One read 2.40 vs 7.00
  at 96% (uld2 0:57.8, under the bear's Infected Wounds and a Blizzard's Chilled): the live-speed rule kites
  then too; no order, as the bot never came inside the margin. No Snaplasher death at 25.
- 10-man, the kite fired: uld2's add tank died at 4:27.3 and the Snaplasher ran loose 4:27-5:17 at 12% to 35%
  (`--tanks`: 10 kite orders, slower in 9 of 9 reads). It read 0.00 vs 7.00 from the first second (20+
  stacks) and stood at (2337.2, -32.4) for 50 s while each victim stayed 16-22 yd off (`loose.py`). The 3
  it killed walked into its ~10 yd reach on other orders: Uldyai `freya nature fury bail` 4:41.2 to 9.3 yd of it,
  Uldyag the same 5:10.9 to 7.4 yd (the bail is `ACTION_RAID + 4`, above the kite), healer Uldyac's spore walk
  5:17.4 to 7.7 yd, one swing each. Neither the Nature's Fury bail nor the spore walk avoids a Snaplasher.
  Melee kiters: vetoes under both kite guards (Uldyai 4:28.6). uld1 and uld3 had 1 s openers only.

**Why the two 10-man pulls wiped: Eonar's Gift lived.** With two ranged DPS, the only Gift takers while
either lives, 16 of 27 Gifts lived to heal in the three 10-man pulls (all 7 in uld2), against 1 of 25 at
25-man; the 10-man Gift has 27,193 health (25-man 90,643). In uld2 the ranged were 31-56 yd off three of them
(`gift_reach.py`).

- uld2: trio 2 (3:13) took three Gift heals of +25-30% to every member (~3:45, ~4:30, ~5:15; `trio_hp.py`),
  at 42-60%, 7-12% and 1-6%, so it never died together. The bear add tank held it 73 s and died bare at 4:27
  (cover own 24.0 / external 20.0 / both 4.2 / none 33.5 s) to three swings and a Tidal Wave; the loose
  Snaplasher above, the Conservator (4:12) and the third lasher wave (5:13) landed on the live trio, and the
  main tank died at 5:15.
- uld3: the 5:01 Gift lived (both ranged on it in reach, 93% to 12%) and took the second lasher wave's six
  from 105% to 253% summed; low lashers then killed a melee and both healers (5:23-5:53, one healer twice),
  and the main tank died to Freya at 6:18 with no healer left. The 5:46 Gift lived too, both ranged on it while
  a lasher at 5% sat 14-31 yd off on the tank. Question 10.

**Built** (Freya files only, any size or difficulty, no player branch):

- Q9: `freya add tank external hold` is now a hand-written multiplier (`FreyaAddTankExternalHoldMultiplier`,
  `Family::Spell`, same veto name): a healer's class `pain suppression`, `pain suppression on party`,
  `guardian spirit on party` or `hand of sacrifice on party` is zeroed while `IsFreyaAddTankExternalHeld` holds
  unless the action's target is the main tank (`PlayerbotAI::IsMainTank`). DPS, healers, pets and a disc
  priest's self cast stay held; `freya add tank external` still covers the add tank one layer at a time.
- Q6: in `ResolveFreyaDpsTarget`, a ranged DPS bot that takes the Gift puts `GetFreyaLowLasherOnPlayer` (under
  `_LASHER_BAIL_PCT`, victim a player, inside `SpellDistance`) ahead of it, except while the trio is locked,
  as the tail pick waits. Built to option (b)'s wording, so a low lasher on a tank or melee counts as well as
  one on the back line: a Gift heals any of them back to ~65%. The bot stays on it while it is the lowest such
  lasher in reach; a lower one takes the bot, and once none is left the Gift does.
- Reader: `--lows` adds, of the in-reach samples, those with Eonar's Gift up and the share on the Gift, tested.
  Baseline here: 25-man 142 (46% on the Gift), 22 (95%) and 9 (11%), 36 (56%); 10-man 2 (100%), 10 (60%) and
  92 (98%); window 14 20 (100%), 12 (8%), 180 (24%), 285 (68%).

**Next window should show** (`freya.py --lows`, `--tanks`, `postmortem.py --vetoes`):

- Q6: `--lows` "with Eonar's Gift up" share on the Gift near 0 (above); Gifts living 12 s+ no more often.
- Q9: healer Pain Suppression, Guardian Spirit or Hand of Sacrifice on the main tank allowed while an add tank
  lives; still none on DPS, healers or pets then; `freya add tank external hold` vetoes only on those.
- Round 9's checks as written there (window 15 pulls it); round 8: no 10-man death to a Snaplasher standing
  at 0.00 from its own victim.

**Not built:** the Nature's Fury bail and the spore walk ignore a loose Snaplasher's reach (10-man, 3 deaths):
a bug candidate (add it to their hazards while it reads 0 speed), next round's pick if it repeats.

**Questions:**

10. Eonar's Gift at 10-man: two ranged DPS cannot kill it inside its 12 s cast, and each one that lives heals
    the trio about 30% or the lashers about 60%: both 10-man wipes this window. Options: (a) keep the Gift a
    ranged job; (b) while fewer than `ULDUAR_FREYA_GIFT_SHARE` (5) ranged DPS live, the melee DPS nearest the
    Gift fill the share up to 5 (25-man rosters carry 9-10 ranged, so unchanged there). Recommendation: (b).
8. (waits for window 15) as written in round 9.

**Shared code:** none.

**Review:** both answers are built as given, on one code path (no size, mode or player branch: a player main
tank counts through the group's main tank, a player a lasher hits counts like a bot), no shared code. The
traces bear the round out: the 10-man loose Snaplasher (`--tanks`: 50.3 s, 10 kite orders, slower in 9 of 9
reads; Uldyai's bail 4:41.2, Uldyag's 5:10.9, Uldyac's spore walk 5:17.4), 16 of 27 10-man Gifts living 12 s+
(uld1's last, at 8:46.7, cut by the kill) against 1 of 25 at 25-man, and the `--lows` baselines. Fixed:

- The findings doc said two thirds of the ranged in reach sat on the Gift; pooled `--lows` reads 52% (window
  14, 497 samples), 52% (window 15a 25-man, 209) and 94% (10-man, 104). Q6's built line overstated the
  stickiness: a lower low lasher takes the bot.

Watch: Q6 counts a low lasher on a tank or melee too (the option's wording), so a tank-held pile going low
one lasher at a time can hold the Gift takers off the Gift for several casts. The next window's "Gifts living
12 s+ no more often" check covers it.

**Orchestrator (2026-11-23):** narrowed Q6 to what the user approved, a low lasher hitting the back line.
`GetFreyaLowLasherOnPlayer` takes `backLineOnly`, and the pick ahead of the Gift passes it: the victim must be
a ranged DPS or healer player, not a tank (`IsTank`, `IsRangedDps`, `IsHeal`, which read a human's spec
too). The round 6 lasher pick still takes any player. The watch item above no longer applies.

**User answer (2026-11-23) to Q10:** yes, at 10 man (any size, by rule) melee help kill Eonar's Gift: while
fewer than `ULDUAR_FREYA_GIFT_SHARE` (5) ranged DPS are alive, the melee DPS nearest the Gift join until 5 are
on it.

## Round 11 (build 2d7853f0e: round 10 built, the shared fix d70101432 merged but not built)

**Data:** window 15 (build 523b2f60f, round 9 built): `tr-20261123-123347-11` (uld1, DK add tank),
`-123307-10` (uld2, bear), `-123407-12` (uld3, bear); traces `env/dist/logs/botobs/603_{11,10,12}_elder-stonebark
_{1795437270,1795437228,1795437289}.ndjson`. Window 16 (build 2d7853f0e, round 10 built; `report --head 2d7853f0e
--pulls 10`): `tr-20261123-134321-7` (uld1), `-8` (uld2), `-9` (uld3); traces `603_{7,8,9}_elder-stonebark_
{1795441471,1795441476,1795441465}`. One valid 25-man pull per pool in each, no DC veto inside a pull. Windows
14 and 15a are the baseline. Ad hoc scripts: `scratchpad/freya-r11/` (`gift_walk.py`, `gift_away.py`,
`camp_kind.py`, `gather_inflight.py`, `gift_hits.py`, `q6_check.py`), plus `freya-r7/wave_timeline.py`,
`gift_reach.py`, `freya-r9/extcasts.py`, `defhp.py`, `tankhp.py`.

**Hard mode.** All six pulls 3 of 3 Elders from their essence channels; the four kills cast chest 62952.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 w15 | 1795437270 | **kill 6:27, 1** | Detonate 1 (melee 3:15) |
| uld2 w15 | 1795437228 | wipe 5:40, 30 | Unstable Energy 7 (three healers and a melee 3:47.5-3:48.4, one healer twice, in three beams on a spore walk; a melee 1:52.7, a ranged 5:13); second lasher wave (4:39, raid at 20) 23: lasher 18 (melee 11, Flame Lash 6, Detonate 1), Freya 5 (Ground Tremor 2, Sunbeam 2, melee 1) |
| uld3 w15 | 1795437289 | **kill 7:44, 5** | lasher 4 (ranged, 4:07-4:27, in a second lasher wave that ran 90s), Nature Bomb 1 |
| uld1 w16 | 1795441471 | **kill 6:17, 1** | Nature Bomb 1 |
| uld2 w16 | 1795441476 | **kill 6:22, 2** | Unstable Energy 1 (melee), Nature Bomb 1 |
| uld3 w16 | 1795441465 | wipe 5:06, 29 | second lasher wave (2:33, never cleared): lasher 8 (4 ranged and a melee 2:41.8-3:09.5, 3 in the collapse); Snaplasher 10 (the bear add tank at 3:52.8 under Survival Instincts and Frenzied Regeneration, again 4:07.3 after his soulstone, then 8 non-tanks 4:10-4:51); Freya 5 (melee 3, the main tank twice; Ground Tremor 1, Sunbeam 1); Storm Lasher 4; Water Spirit 1; Conservator 1 |

**Stop rules:** window 16 killed on uld1 (1 death) and uld2 (2), wiped on uld3; window 15 killed on uld1 (1) and
uld3 (5), wiped on uld2. Not polished. No failure picked three rounds running (round 9 the add tank's death,
rounds 8 and 10 built answers).

**Round 9 verdict (window 15): held.**

- Cover lines: `both` 0.0s in all six trio waves (window 14: 8.0, 19.5s); `none` 11.7/4.5, 1.9/2.4, 5.1/6.3s
  (9-30s).
- Healer Pain Suppression, Guardian Spirit and Hand of Sacrifice went only to the add tank (8 casts); the hold
  vetoed only healers' class externals. Every `freya.addext` name landed its aura (8 of 8, window 16 6 of 6).
- Holds vetoed only inside the add phase (0:15 to before each sixth wave's end).
- Add-tank deaths to the Snaplasher 0/0/0 (window 14: 0/1/1; rounds 4-6: 1-2 a pull). uld3's two trio waves
  2:49 apart: the bear held the second 69.5s (own 49.5s, external 13.8s, none 6.3s).

**Round 10 verdict (window 16).**

- Q9, externals allowed on the main tank: **untested.** No healer external went to a main tank (he died only in
  uld3's collapse); none to DPS, healers or pets; the hold vetoed only healers' class externals.
- Q6, a back-line low lasher before the Gift: **partly held.** Ranged DPS samples in reach of a low lasher hitting
  a ranged DPS or healer with a Gift up (`q6_check.py`; the reader's `--lows` counts any victim): 45, 36% on the
  Gift and 44% on that lasher (windows 14-15a: 614, 54% and 21%). Gifts living 12s+: 3 of 23 (window 15: 3 of 25).
- Round 9 still holds: `both` 0.0s, `none` 3.4/3.0 (uld2), 1.4/6.8 (uld3), 20.6/4.2s (uld1, whose first wave ran
  61.2s: the DK's Vampiric Blood, Icebound Fortitude and both externals were spent by 0:45.7, no death; the
  reader had read that wave on the main tank, fixed below); every external named landed.
- Rounds 1-8: no tank Roar or Shout, Righteous Defense only on non-tanks; Freya on the main tank 99.3/100/95.2%
  (uld3 off him from 4:44.7, in the collapse). Beam dodges from past 8 yd 0-17% by role. `--ghouls` no lasher in
  reach 1-10%, on Freya 0-4%. `--blasts` 0-14% of melee approach orders into a low blast, no melee Detonate death
  (window 15: 2). `--lows` lashers 8s+ on a player 0-1 a wave. No 10-man pull, so round 8's 10-man check waits.

**Q8 decided from the evidence: a question, not built.** No two trio waves came 60s apart in windows 15-16 (gaps
2:23-4:06). The one add-tank death to the Snaplasher in six pulls came in a pull's second trio wave (uld3 w16,
3:23 after the first), but not bare: Barkskin at 3:39.95, Survival Instincts at 3:47.96 (43%), Frenzied
Regeneration at 3:50.71 (19%), dead at 3:52.757 to a Gift-healed lasher's 6.5-8.5k swings between the
Snaplasher's 9.9-31.2k (`tankhp.py`); no external, held as `covered` while his own buttons were ready. Question
8 below.

**Ranking.** (1) A lasher wave Eonar's Gift keeps alive past the next spawn: all three bad pulls, 56 deaths
(w15 uld2 23 after its Unstable Energy 7, w16 uld3 29, w15 uld3 4). Gifts that lived healed the lashers 171% →
558% summed (w15 uld2 5:01), 25% → 139% under a trio just spawned (w16 uld3 3:31), 117% → 416% with the trio
due 17s later (w15 uld3 4:17). (2) Unstable Energy 8, 5 of them in one beam cluster. (3) Nature Bomb 3,
singles. Picked (1).

**Why each of those Gifts lived** (`gift_walk.py`, `gift_hits.py`, `camp_kind.py`; no damage row reaches the
trace for a Gift, casts at it do):

- w16 uld3 3:31: a walk in flight. `freya ranged camp`'s gather (to (2344, -90), about 50 yd from Freya, where
  the anchor Uldzah's own generic flee from a lasher at 3:28.2 took him) went out to five ranged at 3:29.7-3:31.4; the Gift
  spawned at 3:31.27 14.9 yd from Freya, the five took it, and the walk carried them on: Uldzan 10 → 58 yd,
  Uldzak 39 → 58, Uldzau 41 → 58, Uldzap 53 → 58, Uldzaq 50 → 58 (`gather_inflight.py`), their next order 0.8-7.2s
  later. Round 7 stood the gather down for a bot whose target is a Gift, which stops new gather orders, not one
  already walking: `MoveTo`'s `IsWaitingForLastMove` (`MovementActions.cpp:1059-1071`) holds a same-priority
  `reach spell` up to `MaxWaitForMove` (5s) and Uldzan issued none for 7.2s. 83% left.
- w15 uld2 5:01: the pack between. Ranged centroid (2360, -14), lashers (2357, -40), Gift (2343, -67): each `reach
  spell` step ended within 15 yd of a low lasher and the bail walked it back, 19 bails while 9 ranged held it as
  target and none came within 28.5 yd. 98% left.
- w15 uld3 4:17: 7 of 8 ranged stayed on lashers all 14.5s, the mage Uldzah after an Invisibility at 4:11.7; six
  ran generic `dps assist` unvetoed at 4:25.4-4:26.0 (`FreyaOwnsTargets` false). Not root-caused.

Across windows 14-16 (12 pulls), ranged walking away from a Gift while targeting it: 74 bot-seconds on the ones
that lived, 51 of them under `freya ranged camp` (`gift_away.py`).

**Failure picked: a gather walk under way carries Gift takers away from the Gift.**

**Classification: bug.** Round 7's intent, "the gather undid their walk to it", is defeated by the walk already
running; the bail-loop case is a position question (Q11).

**Fix** (Freya files only, any size or difficulty, no player branch): `FreyaSetDpsPriorityAction::
CutWalkAwayFromGift`: a DPS bot whose resolved target is a Gift stops, once per Gift, a walk at
`MOVEMENT_COMBAT` or below whose end lies farther from the Gift than it stands (`GetMotionMaster()->Clear()`,
`StopMoving()`, `last movement` cleared), so `reach spell` or `reach melee` takes it there at once. Forced
escapes (bails, dodges, bombs) run on; once per Gift so a generic flee re-issued next tick isn't cut again.
Note `freya.giftcut` "<bot> of <walk's end> yd".

**Built, Q10:** `TakesEonarsGift`'s melee branch: a melee DPS bot takes the Gift once `IsFreyaGiftFiller` says it
is among the `ULDUAR_FREYA_GIFT_SHARE` minus living-ranged-DPS melee DPS bots nearest it (2D, GUID ties), and
stays on that Gift. Ranged DPS counts every living group member by role, players too, as before; the ranking
counts bots only, so a human melee never takes a bot's slot. Replaces `FreyaHasLivingRangedDps` (melee only
with no ranged alive, after the 5s head start with a pack up). 25-man rosters carry 9-10 ranged: unchanged
until fewer than 5 live.

**Reader:** `--waves` gives each Gift the most ranged and melee DPS on it in one snapshot and its cut walks; the
cover line takes the tank who held the Snaplasher longest (it took the first one, so w16 uld1's first wave read
the main tank's 0% on it as 61.5s bare). Tested. Baselines: 10-man window 15a Gifts at most 2 ranged and 0 melee
on them (one 1 melee), 16 of 27 living.

**Next window should show** (`freya.py --waves`, `gather_inflight.py`, `gift_away.py`):

- `freya.giftcut` notes on Gifts taken mid-walk; ranged walking away from a Gift they target under `freya ranged
  camp` only from bails (window 16 uld3 3:31: 5-48 yd); no Gift living because a gather carried its takers off.
- Fewer Gifts living 12s+ in lasher waves (windows 15-16: 6 of 48 lived, 4 of them in lasher waves) and lasher
  waves done before the next spawn.
- 10-man (Q10): Gift lines with 3 melee next to the 2 ranged; Gifts living 12s+ well under window 15a's 16 of
  27. 25-man: 0 melee on a Gift while 5+ ranged live.
- Rounds 9 and 10 as above; Q9's main-tank externals when he needs one.

**Not picked:**

- The bail loop with the pack between (Q11) and the camp anchor's own flee moving every gather (w16 uld3: to the
  south wall).
- w15 uld3 4:17's ranged on lashers through a Gift with `dps assist` unvetoed: read `FreyaOwnsTargets` next
  round; Invisibility and Feign Death (6 in w16 uld3) may take a bot off Freya's threat list, which turns every
  `Boss()` trigger off for it (round 6's known gap for a risen bot).
- Unstable Energy 5 in one second (w15 uld2 3:47.5): two of the healers dodged at 3:43.5, then `freya move to
  healing spore action` (3:45.2) walked them back across three overlapping beams. The spore walk ignores beams, as it
  ignores a loose Snaplasher (round 10). Next round's candidate if it repeats.
- The add tank's death with buttons ready but unpressed while Barkskin ran (externals held as `covered`): one case.

**Questions:**

8. (decided from windows 15-16) Two trio waves 60s apart: none came, and the one add-tank death was a second
   trio wave 3:23 after the first, with Survival Instincts and Frenzied Regeneration up, to a lasher a Gift had
   healed beside the Snaplasher. Options as in round 9: (a) accept it; (b) DPS leave the Snaplasher until both
   siblings near the sync floor, then burn it; (c) the main tank takes a second wave's Snaplasher.
   Recommendation: (a). (b) withholds damage from the Snaplasher, which the findings doc records as why the trio
   could never die together, and the evidence shows no button gap; this round's fix goes at the lasher that
   killed him. Revisit if a back-to-back pair shows an add tank dying bare.
11. With the lasher pack between the back line and a Gift, the ranged never reach it: each `reach spell` step
    ends inside a low lasher's blast and the bail walks it back (w15 uld2 5:01: 19 bails, 0 of 9 in reach,
    lived at 98%, then 21 deaths). Options: (a) leave it; (b) a ranged DPS bot whose target is a Gift and has no
    spot in reach walks to the nearest spot within `SpellDistance` of it, at least `DETONATE_RADIUS` + 1 from
    every low lasher, on a walk that crosses no blast, as `freya reach held pack` does for the pack; (c) melee
    DPS fill the Gift whenever no ranged is in reach of it. Recommendation: (b).
6. and 7. answered (round 9's user answers).

**Shared code:** none changed. The shared fix d70101432 (`ReadyTankDefensive` judges a button's cost for when the
tank needs it) is on Custom, not in window 16's build; it changes `IsFreyaAddTankCovered` for a DK add tank.

**Review:** the round holds: the traces bear out the pick (w16 uld3 3:31, `gather_inflight.py` as quoted; 6
of 48 Gifts lived 12s+ in windows 15-16), Q10 is built as answered on one code path, no shared code. Fixed:
`CutWalkAwayFromGift` spent its once-per-Gift cut the first tick it saw the Gift, walking or not, so a gather
sent after a Q6 low-lasher pick died (camp and priority share `ACTION_RAID`) was never cut; it now latches
only on a cut. Left: `IsFreyaGiftFiller` ranks by distance alone, so a latched filler a dodge carries
farther frees its rank and one more melee can join.

## Round 12 (build 8bc2a82d3: round 11 and the shared fix d70101432 built)

**Data:** window 17, `report --boss freya --raid uld --head 8bc2a82d3 --pulls 10` (plus `--diff 10n`), one valid
pull per pool at each size, no DC veto inside a pull. 25-man: `tr-20261123-144612-5` (uld1, DK add tank),
`-144551-4` (uld2, bear), `-144612-6` (uld3, bear); traces `env/dist/logs/botobs/603_{5,4,6}_elder-stonebark_
{1795445223,1795445194,1795445227}.ndjson`. 10-man: `tr-20261123-151600-14` (uld1), `-151359-13` (uld2),
`-152000-15` (uld3); traces `603_{14,13,15}_elder-stonebark_{1795446997,1795446875,1795447237}`. Windows 14-16 and
15a are the baseline. Ad hoc scripts: `scratchpad/freya-r12/` (`addtank_tl.py`, `wave_hold.py`,
`spawn_on_back.py`, `submerge_moves.py`), plus `freya-r6/army_delay.py`, `lasher_tgts.py`, `freya-r9/extcasts.py`,
`healpos.py`, `freya-r11/gift_walk.py`, `gift_away.py`, `gather_inflight.py`.

**Hard mode.** All six pulls 3 of 3 Elders from their essence channels; the kills cast chest 62952 (25) and 62950 (10).

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 25 | 1795445223 | wipe 5:16, 31 | first lasher wave (0:55) 6: ranged 5 at 1:05-1:15 (lasher melee 2, Flame Lash 3), a melee 1:35; trio 1: the DK add tank at 2:26.5 bare (below), again 3:00.5 after his soulstone; then Snaplasher 7 in all, Storm Lasher 5, Water Spirit 3, Ground Tremor 2, Nature's Fury 2, Sunbeam 1, own Iron Roots 1, lasher 3 in the 4:57 wave, Freya melee 1 |
| uld2 25 | 1795445194 | wipe 4:43, 30 | the bear add tank to Flame Lash at 0:19.9 (risen); second lasher wave (3:06): lasher 22 (melee 17, Flame Lash 4, Detonate 1; 7 back line 3:12-3:28), Conservator melee 2, Freya melee 2, own Iron Roots 2, Sunbeam 1 |
| uld3 25 | 1795445227 | **kill 7:32, 4** | Freya melee 2 (ranged, 3:20-3:23), Detonate 1 (melee), Nature Bomb 1 |
| uld1 10 | 1795446997 | **kill 8:16, 1** | lasher melee 1 (ranged) |
| uld2 10 | 1795446875 | wipe 10:23 at Berserk, 11 | the bear add tank to the Snaplasher at 4:34.6 (end of trio 2), Tidal Wave 1, Conservator melee 1, Sunbeam 1 (5:18); Berserk 7 (Freya melee 6, Sunbeam 1) |
| uld3 10 | 1795447237 | wipe 10:00.5, 12 | own Iron Roots 1 (healer, 2:47), the bear add tank to the Snaplasher at 4:13.9 (end of trio 2), Water Spirit melee 1, Nature's Fury 1, Sunbeam 2, Ground Tremor 1 (5:43); the last 5 at 9:43-10:00, Freya at 55%, before Berserk was cast (Freya melee 3 from the main tank's death at 9:52 on, Ground Tremor 1, Sunbeam 1) |

**Stop rules:** one 25-man hard-mode kill (4 deaths), not polished. The add tank's death was picked in round 2 (heal
range) and round 9 (externals cast alongside his buttons); this round picks its mirror, his own button cast on a
running external, which round 9's verified fix (w15-16: `both` 0.0s in all twelve trio waves) never covered and
d70101432 made likelier. Different cause each time; flagged here in case the orchestrator counts it as the third.

**Round 11 verdict.**

- Gift cut: **held.** `freya.giftcut` 36 notes at 25, 43 at 10. `gather_inflight.py`: 23 bots took a Gift mid
  gather, median 0.0 yd lost (w16 uld3 3:31: 5-48 yd). Cut walks: spore walks (12 of 17 in uld1 25), camp gathers,
  generic `flee`, and the 10-man melee fillers' `reach melee` / `set behind`.
- Gifts living 12s+: 25-man 6 of 23 (windows 15-16: 6 of 48), **not fewer**, but none from a gather: uld1 1:16 in
  the first lasher collapse (5 ranged dead), uld1 3:34 and 4:18 in a Conservator wave (spore walks 8.6s and 5.6s
  off them, which the Grip's pacify needs), uld2 3:30 (4 ranged alive), uld3 2:48 (Q11's bail loop: 8.7s of bails
  from low lashers 15 yd off the Gift, 2% left), uld3 5:49 (after the add phase, 47 yd from the back line, bomb
  escapes 9.3s).
- Q10, melee on the Gift: **held.** 10-man Gifts had 2-4 melee beside 1-2 ranged; 1 of 37 lived 12s+ (window 15a:
  16 of 27). 25-man melee on a Gift only with 4 or fewer ranged alive (uld1 4:18: 2 melee, uld2 3:30: 1, uld2 4:16:
  5 with 2 alive, a ranged dying mid-Gift and latched fillers staying).
- Lasher waves done before the next spawn: no (uld1 25 92s past the trio, uld3 25 87s past the Conservator).

**Round 10's Q9: held where tested.** uld3 25 4:03.1 Pain Suppression on the main tank in trio 2 with the add tank
alive; no healer external on DPS, healers or pets while an add tank lived. Rounds 1-9: no tank Roar or Shout;
Righteous Defense only on non-tanks (5); Freya on the main tank 98.6/97.6/99.0% (25), 99.7/97.9/96.7% (10).

**Did round 11 or d70101432 make the 25-man worse?** Neither opened a wipe. uld1's first deaths came at 1:05 in the
first lasher wave, before any trio or Gift; uld2's at 3:12 in the second lasher wave, the next Gift at 3:30. Round
11's cuts touched no lasher-wave death. d70101432 did cost uld1's DK at 2:26.5 (`addtank_tl.py`): Icebound Fortitude
off at 2:15.08, Vampiric Blood off cooldown but no blood rune, so `IsFreyaAddTankCovered` read no own button ready
(`ReadyTankDefensive` → `CanPayDefensive(…, 0)`, `RaidTankDefensive.cpp:96-108`) and Pain Suppression went out at
2:15.40; the rune came back and Vampiric Blood went on top at 2:15.68 (`freya add tank defensive` never looks at
externals, `UldTriggers_Freya.cpp:82-90` at `c01caf9cc`). Both ran out at 2:23.4 and 2:25.7, all four healers noted
`none` (no Guardian Spirit on roster A, Hand of Sacrifice out of reach), and a 47.5k raw swing killed him at 2:26.46.
Before d70101432 the healers would have held `covered` and Pain Suppression would have landed about 2:25.7, under
that swing. He rose, took Hand of Sacrifice at 2:29.6 and died bare at 3:00.5 with every button and external spent
(the trio ran 85s on a raid six DPS short).

**Why both 10-man wipes ran to 10 minutes: too few left to beat Berserk.** uld2 took Berserk (47008 at 10:00.03,
Freya melee 84-222k from 10:03); uld3 fell apart at 9:43-10:00 with Freya at 55%, the main tank dying at 9:52 to
plain swings, before Berserk came. Freya is untouchable until the sixth wave dies (~5:30-6:20); the kill burned her
100% → 0 in 2:45 with 9 alive. In both wipes the
healers ran dry first: both under 25% by 3:30-4:30 and near 0 for minutes, as in all six 10-man pulls (window 15a's
too; 25-man's four keep 25-60%). Trio 2's bear died at its end on his last button: uld2 4:34.6 under Frenzied
Regeneration, both healers 37-74 yd off for the wave's first 27s (low lashers chasing the back line south, `freya
ranged camp` bails), then in range from 4:24 to his death, the priest at 0-2% mana and the paladin at 12-25% holding
Hand of Sacrifice (last cast 1:40) as `covered` under Survival Instincts, then Frenzied Regeneration, while the bear
sat at 3-32%; uld3 4:13.9 under Frenzied Regeneration with both healers in range at 1% and 12% mana; Pain Suppression
still held as `covered`. 3 and 5 more died by 5:18 / 5:43, the burn ran from ~6:00 with 7 and 5 alive, Freya at
56% / 55% at 10:00; uld2's 8:04 Gift lived 14.2s and healed her 64% → 88%. Question 13.

**Ranking** (89 deaths). (1) The lasher pack on the back line at 25: 28 lasher kills, both 25-man wipes (uld1's
first wave, uld2's second). A plan question (Q12). (2) The add tank's death and what follows: uld1 25's DK
(the bug below), the two 10-man bears (healers dry, Q13), each opening a chain (Snaplasher 7 in uld1 25; 3 and 5
dead, then 12 at 9:43-10:19 at 10). (3) Singles: Sunbeam 6, Ground Tremor 4, own Iron Roots 4, Nature's Fury 3, Tidal Wave 3.
Picked the bug in (2).

**Why the lasher wave lands on the back line** (`wave_hold.py`, `spawn_on_back.py`, `submerge_moves.py`, 30 waves of
windows 14-17 at 25): the pack spawns within 15 yd of Freya and stands 5s; the back line was standing in that ring
(uld2 3:06: a median 4.7 yd from a lasher, 13 of 14 within 10) or the camp gather walked it in (uld1 0:55: 9.3 → 2.1
yd by the wake, 13 of 22 camp orders ending nearer a lasher; at least half did in 10 of 30 waves). Over the first
30s after the wake, six waves kept the back line a median 4-6 yd from a lasher and it took 116-514k of lasher
damage; 21 of the other 23 with the raid intact, at 10-24 yd, took under 90k. Clean waves also stood 2-7 yd off at
the wake, so distance there alone does not decide; uld2's Army was cut at 5 ghouls by a Ground Tremor cast 0.5s
into the channel (lasher time on ghouls 34% against 56-93% in clean waves; lashers on the back line 49%). No node
moves the back line off a submerged pack (findings doc: "nothing intercepts a fresh pack"), so a fix is a new
position: Q12.

**Failure picked: the add tank's own button goes on top of a running external.** 3 of 11 trio waves (`--tanks` cover
`both`): uld1 25 trio 1 7.7s (above), uld1 10 trio 2 5.2s (Icebound Fortitude off cooldown 3:53.5 during Hand of
Sacrifice 3:48.8-4:00.8, cast 3:55.6), uld2 10 trio 1 4.2s (Barkskin 1:48.05 during Hand of Sacrifice from 1:40.4).
So the gap predates d70101432 (any button coming off cooldown under an external); d70101432 added the rune-gap
route for a DK.

**Classification: bug.** Q2 (c) as round 9 built it is one layer at a time; the healers wait for his buttons, his
buttons did not wait for theirs.

**Fix** (Freya files only, any size or difficulty, no player branch): `HasFreyaTankExternal` (Pain Suppression,
Guardian Spirit, Hand of Sacrifice on him); `FreyaAddTankDefensiveTrigger` stands down while one is up and notes
`freya.adddef` `covered`, and the action re-checks. `IsFreyaAddTankCovered` reads the same helper. Class nodes under
25% still fire as a last resort.

**What it would have changed here:** uld1 25: Vampiric Blood waits to 2:23.4 and runs to 2:33.4, through the
2:24.0 and 2:26.5 swings. uld1 10: Icebound Fortitude 4:00.8-4:12.8 instead of 3:55.6-4:07.6. uld2 10 trio 1:
Barkskin from 1:52.4.

**Next window should show** (`freya.py --tanks`, `addtank_tl.py`):

- Cover lines `both` 0.0s in every trio wave (window 17: 7.7, 5.2, 4.2s); no own button cast while Pain
  Suppression, Guardian Spirit or Hand of Sacrifice is on him, the next one within about a second of its end;
  `freya.adddef` `covered` while an external runs.
- No add tank dying bare right after an external and his own button ran out together.
- Round 9-11 checks as above; the Q12 baseline: `wave_hold.py` back-line lasher damage per wave.

**Questions:**

12. A fresh lasher pack spawns within 15 yd of Freya and stands 5s; the back line is often inside that ring or the
    camp gather walks it in, and both 25-man wipes this window started there (28 lasher kills). Options: (a) leave
    it; (b) while a lasher is submerged, ranged and healers within `_LASHER_CAMP_STANDOFF` (18 yd) of one walk to
    the nearest spot 18 yd from every submerged lasher (the back-line bail's walk), and the gather waits for the
    wake; (c) only the gather waits, no walk. Recommendation: (b), judged by back-line lasher damage over a wave's
    first 30s. The evidence is mixed (clean waves too stood 2-7 yd off at the wake), so it is a trial.
13. (orchestrator, DC comp) 10-man Freya hard mode with two healers runs them dry by 3:30-4:30 in six of six pulls,
    and both 10-man wipes this window followed. The decisions table allows 3 at 10-man where a fight needs it.
    Recommendation: three healers in the `uld-freya` 10-man comp.
8. and 11. pending with the user, not built. Q8: no two trio waves came 60s apart in window 17 (gaps 1:51-3:00).
   Q11: uld3 25's 2:48 Gift lived on 8.7s of bails from low lashers between it and the back line.

**Shared code:** none changed; `RaidTankDefensive` is read, not edited.

**Review:** the pick holds and the fix addresses it, on one code path (no size, mode or player branch), Freya files
only. Reproduced: `--tanks` cover `both` 7.7, 5.2 and 4.2s; all three own casts came from `freya add tank defensive`
(rel 64), not a class node, so the stand-down catches each. uld1 25's Vampiric Blood had not been cast before
2:15.68, so the healers' `none` at 2:15.40 was the blood rune. uld2 10's Pain Suppression at 4:11.98 failed (no
aura) and Survival Instincts came off cooldown 0.17s later (cast 4:12.29): a fourth overlap had it landed. Fixed: the
uld2 10 trio-2 death above, which read as out of heal range; both healers were in range for his last 10s.

Left, a round 13 question (not built): the externals count a running health row (Frenzied Regeneration, Survival
Instincts, Vampiric Blood, Last Stand) as cover at any health, and the external hold keeps the healers' class
externals off the add tank at any health, so uld2 10's paladin kept Hand of Sacrifice in hand while the bear sat at
3-32%. His own buttons have the 25% last resort; the externals have none. Options: (a) leave it, one layer at a time
(Q2 (c)); (b) below `ULDUAR_FREYA_ADD_TANK_DEFENSIVE_LAST_RESORT_PCT` an external may go on top of a running own
button. Recommendation: (b), alongside Q13.

## Round 13 (build 99188e0f1: round 12 built)

**Data:** window 19, `report --boss freya --raid uld --head 99188e0f1 --pulls 10`, one valid 25-man pull per pool, no DC
veto inside a pull: `tr-20261123-170632-12` (uld1, DK add tank), `-170109-10` (uld2, bear), `-170511-11` (uld3, bear);
traces `env/dist/logs/botobs/603_{12,10,11}_elder-stonebark_{1795453633,1795453311,1795453552}.ndjson`. Window 17 is the
baseline. Ad hoc scripts: `scratchpad/freya-r13/` (`handoff.py` layer gaps on the add tank, `bomb_cross.py` and
`bomb_walks.py` walks into live bombs over windows 8-19, `burn.py`), plus `freya-r12/addtank_tl.py`, `wave_hold.py`,
`spawn_on_back.py`, `submerge_moves.py`, `freya-r9/healpos.py`.

**Hard mode.** All three pulls 3 of 3 Elders from their essence channels; both kills cast chest 62952.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795453633 | **kill 6:51, 3** | Nature Bomb 2 (two warriors, 6:21.66, a Charge and an Intercept into three live bombs); Freya melee 1 (the main tank, 5:44.9, after six Nature Bomb hits took him 100% → 11% on `freya tank hold freya`'s walk onto the anchor's bombs) |
| uld2 | 1795453311 | wipe 6:42, 30 | Flame Lash 1 (melee 0:19.9); Snaplasher 2 (the bear add tank in trio 2, 1:55 after trio 1: 4:14.4 bare, 4:16.8 after his soulstone); the 4:35 lasher wave with no add tank alive and the back line on the submerged pack: 27 (Flame Lash 9, lasher melee 8, Freya melee 6, Unstable Energy 2, Sunbeam 1, own Iron Roots 1) |
| uld3 | 1795453552 | **kill 6:23, 4** | Snaplasher 2 (the bear add tank in both trio waves, 0:44.6 and 4:58.3, under Survival Instincts); lasher melee 1 (melee 2:51); Unstable Energy 1 (healer 3:16) |

**Stop rules:** kills on uld1 (3 deaths, at the line) and uld3 (4, one over), uld2 wiped: not polished, one pool short and
one death over. No failure picked three rounds running (rounds 9 and 12 the add tank, round 11 the Gift walk; this round
Nature Bombs).

**Round 12 verdict: held.** `--tanks` cover `both` 0.0s in all six trio waves (window 17: 7.7, 5.2, 4.2s). No own button
cast while an external was on him; `freya.adddef` noted `covered` while one ran (uld1 2:23.03, uld2 4:05.54). Untested in
part: no own button came off cooldown under an external this window (uld1's Icebound Fortitude and Vampiric Blood, uld2's
Barkskin, were cooling down). No add tank died after an external and his own button ended together. Layer handoffs
(`handoff.py`, windows 15-19): 0.0-0.6s, twice 1.2-1.7s, and uld2's below.

Add-tank deaths, 4 (window 17: 1), none the round 12 overlap:

- uld2 trio 2 (3:39.9, 1:55 after trio 1, so Frenzied Regeneration and Survival Instincts still cooling): Barkskin
  3:42.9-3:54.9, Guardian Spirit 3:55.1-4:05.1, Pain Suppression 4:05.4-4:13.35, then bare; a 42.1k swing at 58.7% killed
  him at 4:14.41. Hand of Sacrifice was in hand, but its paladin Uldyac began a Holy Light on him at 4:13.21, 0.14s
  before Pain Suppression ran out, and a bot mid-cast runs no trigger (`PlayerbotAI.cpp:335-417` waits out a preparing
  cast); it went out at 4:15.74 on him risen from his soulstone, who died again at 4:16.83.
- uld3 trio 1 (0:10, the pull's first wave) and trio 2 (4:26.5): died under Survival Instincts, cover external 0.0s in
  both, every healer noting `covered` (his own button running) with Pain Suppression and Guardian Spirit in hand. Trio 1:
  30.4k at 100%, 33.9k at 70.5%, 36.5k at 42.6%; all four healers stood on the DC drop (2362, -78) 39-43 yd from him for
  the whole wave, nothing moving them (the camp runs in lasher waves only), healed him back to 100% twice. Trio 2: Barkskin,
  Frenzied Regeneration, Survival Instincts under 25% (the class node's last resort), then 40.6k at 100% and 44.8k at 61.2%. The pending 25%
  line (round 12 review question) would have caught neither death.

**Rounds 1-11 still hold.** No tank Roar or Shout; Righteous Defense only on pets (uld1 0:05.2, uld3 0:08.1). Freya on the
main tank 98.4/94.3/99.8%. Beam dodges from past 8 yd 0-16% by role. `--ghouls` no lasher in reach 0-9%, on Freya 0-8%.
`--blasts` 2-14% of melee approach orders into a low blast, no melee Detonate death. `--lows` lashers 8s+ on a player 0-3
a wave. Gifts living 12s+: 3 of 26 (uld1 1:16 and 4:17, uld2 2:47 in a Conservator wave with at most 2 ranged on it;
window 17: 6 of 23); `freya.giftcut` notes present. Q10: a melee on a Gift with 5+ ranged alive once (uld3 2:45, 9
ranged and 1 melee). Q12 baseline (`wave_hold.py`, back-line lasher damage over the first 30s after the wake): uld1 19k
and 19k, uld2 46k and 273k, uld3 16k and 28k.

**Ranking** (37 deaths). (1) uld2's 4:35 lasher wave, 27: the back line on the submerged pack (median 7.2 yd at the spawn,
3.2 at the wake, 13 of 14 within 10 yd; `submerge_moves.py`: 23 `freya ranged camp` orders in the submerge, 15 ending
nearer a lasher, 6.6 → 2.8 yd), the Army on time with 8 ghouls but holding 48% of lasher time against 57-89% in the other
five waves, the back line 39%, 273k back-line lasher damage, and no add tank alive for the pickup. That is Q12, with the
user. uld1's two waves also woke on the back line (3.5 yd) and took 19k each, held 71-76% by ghouls. (2) Add-tank deaths,
4: uld3's two are the externals-under-a-health-button question (round 12 review), uld2's the back-to-back trio (Q8) plus
the mid-cast handoff. (3) Bots walking or charging into live Nature Bombs: 3, all of uld1's deaths. (4) Singles: Unstable
Energy 3, Flame Lash 1, lasher melee 1. (1) and (2) are with the user; picked (3), the top bug, which also decides the
stop rule: every uld1 death, and the extra deaths of past kills.

**Failure picked: Freya's own movement walks bots into live Nature Bombs.** `bomb_cross.py` over the 36 25-man pulls
of windows 8-19: of 20 Nature Bomb deaths, 10 were melee that `freya move away nature bomb` walked from a clear spot
13-17 yd out straight back through the bombs, 2 the warriors' charges above, 8 stood in one (rooted or channelling ranged,
escapes 0.4-0.8s before the blast). `bomb_walks.py`, windows 14-19 (18 pulls), orders from outside every live bomb's 11
yd that pass inside one: `freya move away nature bomb` (melee) 96 of 287, 36 then hit by it; `freya tank hold freya` 25
of 38, 8 hit; charges 9 of 26, 2 hit; `reach melee` (melee) 23 of 158, 3 hit.

- The escape's melee re-sweep (`UldActions_Freya.cpp:243-268` at `99188e0f1`): latched, out of Freya's reach, it sweeps
  for an in-reach spot clear of hazards and walks there at `MOVEMENT_FORCED`, never asking what the walk crosses; with
  the bombs between the bot and Freya the nearest such spot is past them. w15 uld3 Uldzag 6:31.89: 13.7 yd east of three
  bombs, sent to (2355.9, -49.4) 15 yd west of them, died 3.4s later on the way, 8.3 yd from them. w17 uld3 Uldzaw
  7:28.12, 0.6s before the blast, same.
- The tank hold (`UldTriggers_Freya.cpp:368-378`): fires on Freya past the 20 yd leash alone; the tank escape only fires
  inside 11 yd. uld1 5:40.11: the main tank, clear at (2343, -69), walked home to the anchor with six live bombs within
  2.4-9 yd of it (landed 5:37.97), was 0.9s from them when the escape fired, and they blew at 5:43.93.
- Charges: once the escape's latch had dropped (Uldxay's re-derive got `MoveTo` refused, zeroing `bombSpotMs`; Uldxax's
  spot was taken by a beam), `isUseful` went false and Intercept (6:20.66) and Charge (6:21.03, both `Family::Charge`)
  landed them 6.5 and 5.0 yd from three bombs that blew at 6:21.66. Nothing guards a charge or `reach melee` against bombs
  (`freya nature bomb melee guard` blocks `set behind` only).

**Classification: bug.** The escape and the tank's bomb handling exist to keep bots out of live bombs, and the doc's hold
("`reach melee` would walk it back through the blast") already says melee must not walk back in.

**Fix** (Freya files only, any size or difficulty, no player branch):

- `FreyaWalkCrossesNatureBomb` (bombs whose `_AVOID_RADIUS` circle already holds the walker excepted).
- `freya move away nature bomb`: the melee re-sweep takes only a spot whose straight walk stays `_AVOID_RADIUS` (11) from
  every live bomb, else holds; the first escape tries in reach and walk clear, then walk clear, before the old sweeps.
- `freya tank hold freya`: waits while a live bomb lies within `_CLEAR_RADIUS` (13) of the anchor or `_AVOID_RADIUS` of the
  tank's walk to it (a fuse is at most 6s).
- `freya nature bomb reach guard` (Role::Melee, non-tank): zeroes `reach melee` and `Family::Charge` whose stop on the
  bot's side of its target lies, or whose straight walk passes, within `_AVOID_RADIUS` of a live bomb.
- Reader: `freya.py --bombs` (orders and charges into a live bomb per action and role, those then hit, how each bomb death
  got in), tested.

**Cost to watch:** melee wait out a bomb (at most 6s) instead of walking through it. Baseline, Freya's burn after the last
wave in the ten kills of windows 14-19: 74-107s, melee 46.5-62.5k/s (`burn.py`).

**Next window should show** (`freya.py --bombs`, `--tanks`; `postmortem.py --vetoes`):

- `--bombs`: `freya move away nature bomb` (melee), `freya tank hold freya` and charges (Charge, Intercept, Feral Charge -
  Cat) into a live bomb near 0 (window 19 uld1: 14 of 28, 2 of 2, 3 of 3); no bomb death "walked in" by any of them;
  vetoes under `freya nature bomb reach guard`.
- Nature Bomb deaths well under windows 14-19's 11 in 18 pulls; no main tank death after bomb hits on his walk home.
- Melee damage over the burn no more than ~10% under 46.5-62.5k/s; burn time no longer than 74-107s.
- Round 12's checks as written there; rounds 1-11 still holding.

**Not picked:** uld3 trio 1's healers on the drop 39-43 yd from the add tank (no node places healers in a pull-opening trio
wave; a position question if it repeats); ranged Nature Bomb deaths standing in one (late or blocked escapes, 8 of 20);
`freya lasher about to blow` walking melee into a live bomb (uld2: 2 of 13, both hit).

**Questions** (all pending with the user; window 19's numbers):

8. Two trio waves close together: uld2's came 1:55 apart and the bear died in the second with Barkskin and two externals
   spent and Frenzied Regeneration and Survival Instincts cooling. Recommendation unchanged: (a), accept it.
12. A back-line walk-out from a submerged pack: uld2's 4:35 wave (above) is the case, 27 deaths, the gather walking the
    back line from 6.6 to 2.8 yd of the pack during the submerge. Recommendation unchanged: (b) as a trial.
14. (round 12 review, an external on top of the tank's own running button below 25%) Both uld3 deaths came above 25% (42.6%
    and 61.2% before the killing swing), under Survival Instincts, with Pain Suppression and Guardian Spirit held all wave.
    Options: (a) leave it; (b) as put: below 25% an external may go on top of a running own button; (c) a running
    health-only button (Survival Instincts, Frenzied Regeneration, Vampiric Blood, Last Stand) no longer holds the
    externals, only one that cuts damage taken (Barkskin, Icebound Fortitude, Shield Wall, Divine Protection) does, so a
    healer layers Pain Suppression or Guardian Spirit over Survival Instincts at once. Recommendation: (c), which covers
    three of the last five add-tank deaths (window 16 uld3 too); (b) covers none of window 19's.
15. The next external waits on a healer mid-cast (uld2: Hand of Sacrifice 2.4s late, the bear dead 1.06s after Pain
    Suppression). Options: (a) leave it; (b) the next external may go out once the running one has 1.5s left (a cast
    time), so two externals overlap up to 1.5s. Recommendation: (b).
11. and 13. pending as written in rounds 11 and 12.

**Shared code:** none changed; `SegmentCrossesCircle` (`EncounterHelpers`) is read, not edited.

**Review:** the pick holds and the fix addresses it, on one code path (no size, mode or player branch), Freya files only.
Reproduced: uld1's Intercept 6:20.656 and Charge 6:21.029 into three bombs; the re-sweep walks behind w15 uld3 Uldzag
6:31.893 and w17 uld3 Uldzaw 7:28.122; uld1's hold walk 5:40.114 with bombs 2.5-12.0 yd from the anchor and its walk 0.5-3.5
yd from them. The reach guard's stop or walk test catches all 8 charges that landed inside 11 yd of a bomb in windows 15-17
and 19, two Feral Charge - Cat (which leaps behind the target) among them.

- Corrected: the tank escape did not wait for 11 yd. Its trigger was live from 5:41.21 (10.6 yd), 2.7s before the blast,
  but its 3s latch on the 5:39.891 kite spot returned true without moving him, the hold's `MoveTo` having replaced that walk,
  until 5:42.89; its next move came at 5:43.002. Fixed: the latch also needs his last `MoveTo` to be the kite spot.
- Fixed: `--bombs` counted each charge twice (the triggered Intercept stun 20253, Charge 34846 and Feral Charge - Cat 61138
  carry the same name); now once per cast, pinned by the test. The uld1 baseline above is corrected (3 of 3, not 4 of 4).
- Left, no payoff seen: the first escape's walk-clear preference. In windows 15-17 and 19 first escapes (from inside a
  bomb's avoid radius) crossed another live bomb in melee 40 of 319 and ranged and healers 2 of 451, none then hit by it;
  melee may now land out of reach instead, so read it with the burn check.
- Left: the hold yields only before a walk. A volley landing on a walk in flight leaves the spline running; the escape
  catches it at 11 yd, now on time. Volleys are 18s apart, so rare.
- Left, a question for a later round: no bomb node covers a tank other than the main tank (`freya near nature bomb`
  skips tanks, `freya tank nature bomb` is main tank only, "assist tank 0 is excluded" for the Conservator's spore, which
  phase 2 has none of). uld1's off tank Uldxam stood 8.7 yd from the anchor's bombs on `reach melee` and took four hits at
  5:43.93.

**User answers (2026-11-23):** Q8 yes, accept two trio waves close together (no change). Q11 yes, option (b): a
dedicated walk for Gift takers to a spot in spell range of the Gift and clear of low-lasher blasts, on a path that
crosses none. Q12 yes, option (b), as a one-window trial: while a lasher of a fresh pack is still submerged, ranged
and healers within 18 yd of one walk to 18 yd from every submerged lasher, and the camp gather waits until they wake.
Q13 yes, a third healer in the 10-man comp (the orchestrator changes the DC roster row). Round 12 review's option (b)
yes: below the add tank's 25% last-resort line, a healer external may go on top of his running own cooldown. Pending,
not built: Q14 (option c, only damage-cutting own buttons hold back the externals) and Q15 (the next external at 1.5s
left on the running one).

## Round 14 (build 90400b39c: round 13 built)

**Data:** window 21, `report --boss freya --raid uld --head 90400b39c --pulls 10`, one valid 25-man pull per pool, no DC
veto inside a pull: `tr-20261123-184856-8` (uld1, DK add tank), `-184856-9` (uld2, bear), `-184756-7` (uld3, bear);
traces `env/dist/logs/botobs/603_{8,9,7}_elder-stonebark_{1795459785,1795459792,1795459719}.ndjson`. Window 19 is the
baseline, windows 8-21 (40 Freya traces) for the bomb escapes. Ad hoc scripts: `scratchpad/freya-r14/` (`rows.py`,
`bomb_latency.py`, `bomb_perbot.py`, `bomb_z.py`, `races.py`, `godisplay.py`), plus `freya-r13/burn.py`,
`bomb_deaths.py`, `bomb_track.py`, `bomb_entry.py`, `freya-r12/wave_hold.py`, `submerge_moves.py`, `addtank_tl.py`.

**Hard mode.** All three pulls 3 of 3 Elders from their essence channels; both kills cast chest 62952.

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 | 1795459785 | **kill 6:03, 1** | Flame Lash 1 (melee 4:08.9, second lasher wave) |
| uld2 | 1795459792 | wipe 8:09, 30 | second lasher wave (3:28): lasher melee 3 (a ranged 3:34.8, a melee 3:48.7, the bear add tank 4:08.8); trio 2 (4:28) with the bear risen: Snaplasher 4 (the bear again 4:52.6, two melee 5:13.0 and 5:31.2, the main tank 5:53.8), own Iron Roots 1, Nature Bomb 1 (melee 5:39.5); then Freya melee 19 and Nature Bomb 2 (a healer 7:45.7, a rooted ranged 8:04.5) |
| uld3 | 1795459719 | **kill 6:32, 7** | Nature Bomb 4 (three melee 6:08.3 in one volley, one 6:26.3); Unstable Energy 3 (two ranged 3:57.05, 0.4-0.5s into their dodges, one again after Reincarnation 4:00.1) |

**Stop rules:** kills on uld1 (1 death) and uld3 (7, four of them this round's bug), uld2 wiped: not polished. Picks:
round 12 the add tank, round 13 bot moves into live bombs, this round the bomb escape blocked by the bomb itself; two
Nature Bomb rounds running, each a different cause.

**Round 13 verdict: held.** `--bombs`: `freya move away nature bomb` (melee) walks into a live bomb 1 of 12 in uld3,
none elsewhere (window 19 uld1: 14 of 28); `freya tank hold freya` and charges into one 0 (2 of 2, 3 of 3); no bomb
death walked in. `freya nature bomb reach guard` vetoed 17/11/18 times a pull. Burn after the last wave 78s (uld1) and
90s (uld3), melee 57.6 and 50.8k/s (`burn.py`; 74-107s, 46.5-62.5k/s). No main tank died after bomb hits on his walk
home. But Nature Bomb deaths 0/3/4 (windows 14-19: 11 in 18 pulls), every one "stood in it": below.

**Rounds 1-12 still hold.** Cover `both` 0.0s in all six trio waves; one Righteous Defense on a melee (uld1 0:13.2), no
Roar or Shout; Freya on the main tank 100/85.2/100% (uld2 off him from his death at 5:53.8). `--ghouls` no lasher in
reach / on Freya 0/0 and 11/10% (uld1), 16/17 and 0/0 (uld2), 0/4 and 0/0 (uld3). `--blasts` 3-18 melee approach
orders into a low blast a wave, no melee Detonate death. `--lows` uld2's second wave left 6 lashers 8s+ under the line
on a player (its collapse), the others 0-1. Gifts living 12s+: 4 of 28 (uld1 0:31 in trio 1, uld2 0:31 in a lasher
wave, 5:05 and 5:50 in the collapse).

**Ranking** (38 deaths). (1) uld2's second lasher wave and what followed, 30: the back line on the submerged pack (14 of
14 within 18 yd at the spawn and the wake, 20 camp orders in the submerge, none ending clear; Army at +5.3s, ghouls 31%
of lasher time, 69k on the back line), the bear add tank dead to the pile at 4:08.8, trio 2 at 4:28 on a raid without
him; Q12 is built this round, the add tank's death is Q16. (2) Nature Bomb 7, six of them bots whose escape could not
start (picked) and one rooted for 6s. (3) Unstable Energy 3, Flame Lash 1.

**Failure picked: a bomb that lands on a bot blocks its escape for 5s.**

- uld3 6:02.257: four bombs on three melee in reach of Freya at (2362, -31), z 423.9. `freya move away nature bomb`
  FAILED at 6:02.29-6:02.43 with no move row (every sweep, `StepOffNatureBombs` included, found no spot), then went at
  6:07.33-6:07.53, 5.07-5.27s after the landing; all three died at 6:08.32, 15-16 yd short (`rows.py`,
  `bomb_track.py`). Uldzag the same, 6:20.26 → 6:25.53, dead 6:26.33; uld2 Uldyay 5:33.55 → 5:38.70, dead 5:39.51,
  and healer Uldyac 7:39.75 → 7:44.76, dead 7:45.72.
- Cause: GO 194902 is display 391 `World\Goober\G_PulsatingPlant.mdx`, bounds ±2.4 yd by -0.14..0.74 yd
  (GameObjectDisplayInfo.dbc, `godisplay.py`), solid while `GO_STATE_READY`; `boss_freya_nature_bomb::UpdateAI` sets
  `GO_STATE_ACTIVE` at 5000 ms and `GameObject::SetGoState` collides only in READY (`GameObject.cpp:2594`).
  `Map::CheckCollisionAndGetValidCoords` raycasts the dynamic tree at half the bot's height (`Map.cpp:3519`); from a
  short bot, or one on a slope, standing in the plant every ray hits it, so `FindNearestPositionClearOfHazards`
  (`EncounterHelpers.cpp:739`) refuses every point. The walk itself is never blocked: nothing collides a walking bot
  with a gameobject (pitfalls, Hodir's campfire).
- Scale (`bomb_perbot.py`, `bomb_z.py`, 40 pulls): of 1652 bots a bomb landed on (1.5 yd) that ordered an escape, 101
  did so 4.5s+ late; the latency histogram has 86 at 5.0-6.0s against 6 at 4.0-4.5s. It repeats per bot: each pool's
  gnome warlock 9-18 times. The bomb's height over the bot is 0.0 yd in both groups. Window 21: 6/10/7 of 62/84/68
  landings (`--bombs`), window 19 2/0/0.

**Classification: bug.** The escape exists to take bots out of a bomb; the strategy never meant the bomb's own model to
hold them in it.

**Fix** (Freya files only, any size or difficulty, no player branch): with the collision chain empty, the escape
re-runs its in-reach walk-clear, walk-clear and `_AVOID_RADIUS` + 1 sweeps through `FindFreyaStaticFloorSpot` (the same
rings, each point through `ValidateStaticFloorPoint`, the navmesh raycast without the dynamic pass) before
`StepOffNatureBombs`. Escapes the collision chain already finds are unchanged. Reader: `--bombs` gives, per pull, the
bombs landing at a non-tank's feet and how soon each ordered its escape, tested.

**Built, the user's answers** (Freya files only, any size or difficulty, no player branch):

- Q11: `freya reach gift` (`ACTION_RAID + 1`): a ranged DPS bot whose target is a Gift, out of `SpellDistance` of it,
  with a low or submerged lasher within its distance plus 18 yd and outside every low lasher's blast (the bail goes
  first there), sweeps to the nearest spot within `SpellDistance` - 1 of the Gift (preferring the Gift), clear of bombs,
  beams, low lashers by `_DETONATE_RADIUS` + 1 and submerged ones by 18, on a straight walk crossing none of those
  lasher circles but one already holding it; latched 3s, retried after 1s, `MOVEMENT_COMBAT`. No spot leaves
  `reach spell` as before. Submerged lashers count, so the walk never ends on a waking pack Q12 would walk it off.
  Reader: each Gift line counts its `freya reach gift` walks, tested.
- Q12 (trial): `IsFreyaLasherSubmerged` (alive, `REACT_PASSIVE`: the core's first 5s); `freya ranged camp` fires for a
  ranged DPS or healer within 18 yd of one and walks it through the bail's walk clear of every submerged and low
  lasher, and the gather waits while any lasher is submerged. Reader: new `--submerge`, tested. Baseline (window 21, at
  the spawn and the wake): uld1 13 then 14 of 14 within 18 yd, 9 camp orders none clear, 61k on the back line in the
  30s after the wake, and 12 then 13 of 14, 0k; uld2 14 and 14 of 14, 20 orders none clear, 69k; uld3 13 and 13 of 13,
  11 orders, 0k; window 19 uld2's wipe wave 13 then 14 of 14, 273k.
- Round 12 review (b): `IsFreyaAddTankCovered` counts only a running external under
  `_ADD_TANK_DEFENSIVE_LAST_RESORT_PCT` (25%), so `freya add tank external` goes out on top of his running or ready own
  button there.

**Next window should show** (`freya.py --bombs`, `--submerge`, `--waves`, `--tanks`):

- `--bombs`: escapes ordered from 4.5s on near 0 (window 21: 6/10/7); no Nature Bomb death of a bot free to walk (window
  21: 6); round 13's walk-ins still near 0.
- Q12: the back line within 18 yd of a lasher at the wake well under 13-14 of 14, camp orders in the submerge ending
  18 yd clear, back-line lasher damage over the 30s after the wake no higher (61k, 69k worst here, 273k in window 19);
  lasher waves still dying before the next spawn.
- Q11: `freya reach gift` walks on Gifts with low lashers about; Gifts living 12s+ in lasher waves no more often (here
  1 of 28 Gifts).
- 25%: any external landing on the add tank under 25% while his own button runs (cover `both` above 0 only then).
- Rounds 1-13 as above.

**Not picked:** uld3's two Unstable Energy deaths in one beam cluster (dodges ordered 0.4-0.5s before death); uld2's
loose Snaplasher at 12% killed its kiter (9 kite orders, slower in 45 of 45 reads) with a 79k swing; a rooted ranged in
a bomb (uld2 8:04.5); the main tank's escape has no static fallback (a bomb landed at his feet once in 40 pulls).

**Questions:**

16. uld2's bear add tank died to a lasher pile at 4:08.8 with his buttons held: 100% at 4:02, Frenzied Regeneration
    vetoed by `freya add tank defensive hold` at 66% (4:07.0), then 53% → 30% → 16% → dead in 0.7s (two Flame Lashes
    and a swing), so the 25% last resort never got a tick; trio 2 came 20s later onto a raid without him (27 more
    deaths). Options: (a) keep the hold as Q2 (c) built it; (b) release it while 3+ lashers hit him, at the cost of his
    3-minute buttons for the next trio; (c) raise his last-resort line to 50% while lashers hit him. Recommendation: (a)
    for another window (one case in six pulls since the hold), then (c) if an add tank dies to lashers with buttons held
    again.
14. and 15. pending as written in round 13.

**Shared code:** none changed; `ValidateStaticFloorPoint`, `FindNearestPositionClearOfHazards` and
`SegmentCrossesCircle` are read, not edited.

**Review:** the pick holds and the fix addresses it, on one code path (no size, mode or player branch), Freya files
only. Reproduced: uld3's three melee `freya move away nature bomb` FAILED at 6:02.285-6:02.427 with no move row and
moved at 6:07.328-6:07.531; `--bombs` 6/10/7 of 62/84/68 from 4.5s on; over the 40 traces (`bomb_perbot.py`) 74
escapes at 5.0s and 12 at 5.5s against 6 at 4.0s. The 5.0s edge is the plant's only state change
(`boss_freya_nature_bomb::UpdateAI` sets `GO_STATE_ACTIVE`); the dynamic pass is `Map.cpp:3519`, which
`ValidateStaticFloorPoint` skips and `MoveTo` never runs. Q11, Q12 and the 25% line match the user's answers. Fixed:

- `freya reach gift`'s walk check exempted every lasher circle already holding the bot, and the Gift walk outranks
  the camp's walk-off, so a Gift taker within 18 yd of a submerged lasher could walk across the pack to a spot past
  it. It now goes at most `CONTACT_DISTANCE` deeper into such a circle; with no spot the camp walks it off first.
- Its 3s latch returned true after a beam dodge or bomb escape had replaced the walk, idling the bot at the
  dodge's spot; it now also needs its `MoveTo` to be the bot's last, as round 13's tank escape does.

Left: `freya reach held pack`'s latch has the same gap (round 3, no case seen). `FindFreyaStaticFloorSpot` takes a
point whose navmesh raycast failed as it is, where the collision sweep drops it; it runs only once that sweep found
nothing, and Freya's floor is open.

## Round 15 (build a88ea2ade: round 14 built)

**Data:** window 22, `report --boss freya --raid uld --head a88ea2ade --pulls 10` (plus `--diff 10n`), one valid pull
per pool at each size, no DC veto inside a pull. 25-man: `tr-20261123-201433-4` (uld1, DK add tank), `-201513-5` (uld2,
bear), `-201513-6` (uld3, bear); traces `env/dist/logs/botobs/603_{4,5,6}_elder-stonebark_{1795464917,1795464964,
1795464971}.ndjson`. 10-man (three healers, DC 518a87e): `tr-20261123-203740-11` (uld1), `-203800-12` (uld2),
`-203337-10` (uld3); traces `603_{11,12,10}_elder-stonebark_{1795466296,1795466316,1795466054}`. Window 21 is the
baseline, windows 14-22 (24 25-man pulls) for the Gifts and the Army. Ad hoc scripts: `scratchpad/freya-r15/`
(`lash_tl.py`, `ranged_tl.py`, `gift_in_lashers.py`, `gift_casts.py`, `gift_offcasts.py`, `gift_dots.py`,
`army_tries.py`, `wake_pick.py`), plus `freya-r12/wave_hold.py`, `freya-r14/rows.py`.

**Hard mode.** All six pulls 3 of 3 Elders from their essence channels; the kills cast chest 62952 (25) and 62950 (10).

| Pool | Pull | Out | Deaths by mechanic |
|---|---|---|---|
| uld1 25 | 1795464917 | **kill 6:54, 4** | Detonate 1 (melee 1:46.8), Flame Lash 1 (ranged 3:43.8), lasher melee 1 (melee 3:53.9), own Iron Roots 1 (ranged 3:55.2) |
| uld2 25 | 1795464964 | wipe 5:35, 31 | Snaplasher 1 (melee 0:13.4, trio 1 opener), Storm Lasher 1 (healer 3:13.9), Unstable Energy 1 (4:07.3); the 4:40 lasher wave (Army +6.8s) and the collapse: lasher melee 12, Flame Lash 5, Freya melee 7 (the main tank from 4:54.9), Unstable Energy 2, Sunbeam 2 |
| uld3 25 | 1795464971 | wipe 2:24, 32 | all inside the first lasher wave (0:10), which never ended: lasher melee 13, Flame Lash 7, Storm Lasher 3, Ground Tremor 2, Freya melee 1, Conservator 2 (melee, Nature's Fury), Water Spirit 1, Snaplasher 1, Unstable Energy 1, own Iron Roots 1 |
| uld1 10 | 1795466296 | **kill 9:17, 0** | none |
| uld2 10 | 1795466316 | **kill 9:25, 1** | lasher melee 1 (ranged 5:47.1) |
| uld3 10 | 1795466054 | **kill 9:06, 3** | Flame Lash 1 (healer 1:30.1), Lightning Lash 1 (healer 5:43.2), Snaplasher 1 (the bear add tank 6:04.9 at the end of trio 2, every button and external spent, healers noting `none`) |

**Stop rules:** 25-man one kill in three (uld1, 4 deaths, one over the line); not polished. Window 21 killed two
(1 and 7), window 15a three (5, 3, 4), no window yet three at 3 or fewer. 10-man check: three hard-mode kills with
0/1/3 deaths, so it counts as passed once 25-man is polished. No failure picked three rounds running (round 13 bomb
walk-ins, round 14 the bomb's own collision; this round the Gift's takers).

**What wiped uld3 at 2:24, and round 14's part in it.** The first lasher wave (0:10) started clean: Army +1.6s,
ghouls 76% of lasher time, 0k on the back line over the 30s after the wake. At 0:31.8 a Gift spawned at (2371,
-37), 31 yd from the ghoul-held pack and 29-41 yd from the ranged; it lived with 8% left and at 0:43.8 healed the
pack from 2-50% (154% summed, plus the add tank's lasher at 70%) to 51-92% (492%), 8s before the ghouls expired. From
0:52 the pack went onto the back line; low lashers chasing ranged bots kept them bailing (112 back-line bails, 8
lashers 8s+ under the line); the 1:16.6 Gift spawned at (2320, -43), 36-58 yd from the ranged, took no damage (Q6
picks sent its takers to those low lashers, which they never finished while bailing) and healed four lashers at 1-10%
back to ~60% at 1:28; the Conservator (1:10) and trio (2:10) landed on it.

- Why the 0:31.8 Gift lived (`gift_casts.py`, `rows.py`): of its takers' casts, 25 hit it and 12 were `… on attacker`
  DoT spreads onto lashers and Freya (Uldzal cast `corruption on attacker` every GCD 0:32.4-0:39.8 with the Gift as
  his target); besides, a bail at 0:35.4 (the pack focus under 15% with the back line 14 yd off), a Sunbeam step-out at
  0:37.1, the five-bot share starting at four (Uldzak, one of the five, rooted 41 yd off, shooting his roots) and a Q6
  flip at 0:40.8 (that lasher at 4% on Uldzaq after the 10s reset). It stood at 13% half a second before the heal.
- Round 14 did not cause it. The Q11 Gift walk ran 4 times on that Gift (Uldzal, Uldzan, Uldzav, Uldzar, each about 6
  yd to within 27 yd, then casting), once on each later one; none ended in a blast and no walker died. Q12 did not run:
  the back line stood 20 yd off the pack at the spawn, 0 camp orders in the submerge. No bomb, no trio until 2:10.

**Round 14 verdicts.**

- Bomb escape through the static floor: **held.** `--bombs` escapes ordered from 4.5s on 0 of 72 landings (uld1 25),
  0 of 32/39/38 (10-man); window 21: 6/10/7 of 62/84/68. No Nature Bomb death in the four pulls with bombs (window 21:
  7). Round 13's walk-ins still 0 for `freya move away nature bomb`, the tank hold and charges.
- Q11, the Gift walk: **held where tested, no harm seen.** 11 walks on 5 Gifts at 25 (uld1 1:16 and 3:31, uld3's
  three), each a few yards into reach; no walker hit by a blast or killed after one. Gifts living 12s+: 3 of 19 at 25
  (all uld3's endless wave; window 21: 4 of 28), 2 of 36 at 10.
- Q12, the submerge walk-out (trial): **does what it was built to, but moves the back line out of the ghouls'
  reach** (`--submerge`, `wake_pick.py`). At the wake 4-6 of 14 within 18 yd of a lasher (window 21: 13-14 of 14), the
  camp orders in the submerge ending clear (20/20, 11/13, 8/8, 12/12). But back-line lasher damage over the 30s after
  the wake did not drop: 28k, 80k, 12k, 266k (window 21: 66k, 16k, 8k, 69k, 0k, 0k). Ghoul Taunt 43263 reaches 15 yd
  round each ghoul (DBC radius index 18) and the ghouls spawn on the DK in melee; in windows 14-21, with the back line
  within 22 yd of the Army's DK at the wake, ghouls took 61 of the 71 lashers that woke onto a back-line bot and 1 of
  23 waves put 100k+ on the back line; farther, 11 of 47 and 8 of 18. Q12 left the back line 27-36 yd from the DK in
  all five 25-man waves: ghouls took 4 of 15. uld2's 4:40 wave (6 woke onto the back line 36 yd off, 2 taken, Army 6.8s
  late: runes spent on Death and Decay 2.1s and Plague Strike 0.8s before the spawn, the first try `FAILED` at +2.1s)
  put 266k on the back line, 13 lasher kills in 30s, 27 deaths. 10-man waves 0-67k. Question 17.
- 25% line: **untested.** Cover `both` 0.0s in all four 25-man and five 10-man trio waves; no add tank stood under 25%
  with his own button running and an external in hand.
- Rounds 1-13: no tank Roar or Shout; Righteous Defense only on melee (uld3 1:35.5, 1:43.8, 2:11.9); Freya on the main
  tank 99.1/91.4/93.1% (off him only in the collapses). `--ghouls` no lasher in reach 2-12%, on Freya 0-5%. `--blasts`
  0-8 melee orders into a low blast a wave; one melee Detonate death (uld1 1:46.8). `--lows` 8s+ on a player 0 outside
  uld3's collapse.

**Ranking** (67 deaths at 25). (1) uld3's endless first wave, 32: the 0:31.8 Gift living by half a second, then the
1:16.6 Gift untouched. (2) uld2's 4:40 wave, 27: the back line out of the ghouls' reach (Q12, a user's answer) and a
late Army (the rune gap, below). (3) Singles. Picked (1)'s root, the Gift takers' casts going elsewhere; (2) is
Question 17.

**Failure picked: Gift takers spend their GCDs spreading DoTs onto other adds.** The class rotation's `… on
attacker` actions (`CastDebuffSpellOnAttackerAction`: Corruption, Unstable Affliction, Seed of Corruption, Immolate,
Curse of Agony, Shadow Word: Pain, Vampiric Touch, Insect Swarm, Moonfire, Serpent Sting, Living Bomb, Icy Touch)
run whatever the bot's target. Over windows 14-22 at 25 (`gift_dots.py`, the reader's count): the takers of 191 Gifts
landed 1353 of them off the Gift, 7.1 a Gift, 24% of their casts on Gifts that died and 33% on the 27 that lived; 9 of
those 27 lived with 25% or less left, and 8 of the 9 had 7+ spreads (window 21's uld1 0:31 at 18% with 15, uld2 0:31
at 6% with 18, this uld3 0:31 at 8% with 12). 10-man: 2 over 36 Gifts.

**Classification: bug.** The Gift is the ranged job and `FreyaSetDpsPriorityAction` makes it their target; their GCDs
going to other adds is the bots not doing what the strategy sets.

**Fix** (Freya files only, any size or difficulty, no player branch): `freya gift dot hold` (`e.Block`, `Role::Dps`)
zeroes `Family::DebuffOnAttacker` while the bot's current target is a living Eonar's Gift. Reader: each `--waves` Gift
line counts the `… on attacker` casts its takers (by their target in the snapshot before) landed off it, tested.

**Cost to watch:** the pack and trio lose the takers' DoT spreads for a Gift's life (up to 12s, four or five times a
pull); they resume once it dies.

**Next window should show** (`freya.py --waves`, `postmortem.py --vetoes`):

- Gift lines with DoT spreads by its takers near 0 (window 22 at 25: 138 over 19 Gifts); vetoes under `freya gift dot
  hold`.
- Gifts living 12s+ fewer (windows 14-22 at 25: 27 of 191, 9 of them with 25% or less left).
- Lasher waves no slower to die with no Gift up; rounds 1-14's checks as above.

**Not picked:** the Army rune gap: the reserve starts at the spawn, so a rune spent in the seconds before delays Army;
second-wave Armies landed a median 4.5s after the spawn over windows 14-22 (first waves 1.6s), most after a `FAILED`
first try; a fix needs the next wave's time and kind, larger than this round. The share's five counting a rooted bot.
The 10-man add tank's bare end of trio 2 (every button and external spent, one case).

**Questions:**

17. Q12's walk-out (trial, window 22's numbers above) clears the back line off the waking pack, but leaves it 27-36 yd
    from the Army's DK, where the ghouls' 15 yd taunt took 4 of the 15 lashers that woke onto it (windows 14-21: 61
    of 71 within 22 yd of the DK, 11 of 47 farther), and back-line damage after the wake did not drop (28k, 80k, 12k,
    266k). Options: (a) keep the trial another window; (b) drop the walk-out, keep the gather's wait for the wake; (c)
    drop the trial, back to round 13. Recommendation: (c); (b) still leaves the back line wherever it stood.
16. (round 14) uld3's risen bear add tank died to the lasher pile at 2:00.4, 99% to 32% in 6s, with Barkskin
    (1:53.3), Frenzied Regeneration (1:55.7) and Survival Instincts (1:58.2, at 32%) vetoed by `freya add tank
    defensive hold`; the case round 14 named, though inside a collapse. Recommendation: (c), raise his last-resort line to 50% while
    lashers hit him, as round 14 set for a repeat.
14. and 15. pending as written in round 13; no case this window (the only add-tank trio death, 10-man uld3, had no
    button or external left).

**Shared code:** none changed; `CastDebuffSpellOnAttackerAction` and `Family::DebuffOnAttacker` are read, not edited.

**Review:** the cause holds and the hold addresses it, on one code path (no size, mode or player branch), Freya files
only. Reproduced: uld3's 0:31.8 Gift (Uldzal's Corruption onto a lasher or Freya every GCD 0:32.5-0:39.8, the Gift his
target), 27 of 191 Gifts living, 8 of the 9 at 25% or less with 7+ spreads, and Ghoul Taunt's 15 yd (DBC radius
index 18). Fixed:

- The reader counted every `… on attacker` action a taker ran, but 353 of those 1706 (36 of window 22's 174) landed on
  the Gift itself, the action picking it as an attacker without the DoT. It now counts one only when the bot's own cast
  at that moment landed off the Gift; the figures above and in the findings doc are the corrected ones.
- `plague strike on attacker` came off the hold: no DK strategy runs it (nor `icy touch on attacker`), and no Freya
  trace holds either. `Family::DebuffOnAttacker` holds every spread a DPS bot runs; the one melee variant outside it,
  `rend on attacker`, is a tank warrior's.

**User answers (2026-11-23):** Q14 yes, option (c), replacing round 14's 25% rule: only a running own button that
cuts damage taken (Barkskin, Icebound Fortitude, Shield Wall, Divine Protection) holds back the healers' externals;
health-only buttons (Survival Instincts, Frenzied Regeneration, Vampiric Blood, Last Stand) no longer do. Q15 yes,
option (b): the next external may go out once the running one has 1.5s left, so two externals overlap by up to
1.5s, since a healer mid-cast sends it late. Q16 and Q17 pending.

## Round 16 (on Custom 17b74a9a4, no new pulls)

**Data:** none new. Round 15 (the Gift DoT hold) is merged but not built, so no verdict, stop rule or ranking.
Window 22's view is `report --boss freya --raid uld --head a88ea2ade --pulls 10`, round 15's traces. Built the
user's Q14 and Q15 and removed round 14's 25% rule. Ad hoc scripts: none.

**Built** (Freya files only, any size or difficulty, no player branch; a human add tank reads the same auras and
cooldowns):

- `IsFreyaAddTankCovered`, the cover test behind `freya add tank external`: covered while (1) an external has
  `ULDUAR_FREYA_ADD_TANK_EXTERNAL_LEAD_MS` (1500) or more left (Q15), else (2) when any own physical button runs
  (`RunningTankDefensive`), only if one of `FREYA_TANK_DAMAGE_CUTS` is up: Shield Wall 871, Divine Protection 498,
  Barkskin 22812, Icebound Fortitude 48792 (Q14; the same four rows as the shared table's physical tank rows with
  a cut, read by aura id because that table exposes no cut for a running button), else (3) with nothing of his
  running, while a button is ready and affordable (`ReadyTankDefensive`, unchanged: his goes first).
- The 25% line is gone from the cover test (round 14's `GetHealthPct() < _LAST_RESORT_PCT` return). The constant
  stays for `freya add tank defensive hold`.
- `HasFreyaTankExternal(tank, forMs = 0)`: an external with at least `forMs` left. Only the cover test passes the
  lead.
- Reader: the `--tanks` cover line splits `both` (its share on a damage cut of his) and adds `two externals` and
  `no damage cut` (neither his damage cut nor an external), tested.

**Every caller checked:**

- `GetFreyaTankExternal` (the healers' `freya add tank external`) and the healer's `freya.addext` note
  (`covered` / `none`): both read the new test.
- `FreyaAddTankDefensiveTrigger` and `FreyaAddTankDefensiveAction` (the add tank's own `freya add tank defensive`):
  call `HasFreyaTankExternal(bot)` with no lead, so his own buttons still wait out any running external to its
  end, and `NextTankDefensive` still runs one own button at a time. Unchanged.
- `freya add tank defensive hold` (`IsFreyaAddTankDefensiveHeld`, the class nodes above 25%) and `freya add tank
  external hold` (the healers' class externals off everyone but the add tank and the main tank) do not call the
  cover test. Unchanged; Q16 would change the first.

**What changes per tank class** (`NextTankDefensive` order, physical rows; one trio wave with everything ready):

- Druid (the bear add tank of uld2 and uld3): Barkskin first, which holds the externals as before; Frenzied
  Regeneration and Survival Instincts now each take one at once, a tick after he presses it. Roster C (Pain
  Suppression, Guardian Spirit): Barkskin 0-12s, Frenzied Regeneration 12-22 with Pain Suppression 12-20, Guardian
  Spirit 18.5-28.5, Survival Instincts 28.5-48.5 alone (it waits out Guardian Spirit); before, one layer at a
  time to 60s. Roster B adds Hand of Sacrifice on Survival Instincts, 28.5-40.5.
- Death knight (uld1's add tank): Vampiric Blood first, now with an external at once; Icebound Fortitude holds
  them; Anti-Magic Shell is never offered (physical). Roster A (Pain Suppression, Hand of Sacrifice): Vampiric
  Blood 0-10 with Pain Suppression 0-8, Hand of Sacrifice 6.5-18.5, Icebound Fortitude 18.5-30.5; before, to 42s.
  The rune gap is as before: Vampiric Blood unaffordable is not ready, so an external goes and Vampiric Blood
  waits it out.
- Warrior (no Freya add tank on the three rosters; roster B's main tank): Last Stand first, now with an external
  at once; Shield Wall holds them.
- Paladin (no Freya add tank; roster A's and C's main tank, who can end up holding a Snaplasher): Divine
  Protection holds them, as before; under Forbearance (a Lay on Hands on him) it is not ready, so they go.
- Any class, below 25%: round 14 let an external go on top of any own button; now not on a damage cut, and a ready
  button with nothing running holds them for his press, as before round 14.
- Any class: an external hands over 1.5s early only when no button of his is ready; with one ready the healers
  wait and he presses it at the external's end.

So the add tank gets a damage cut from the wave's start and layers on his health buttons, but his chain ends
11-12s sooner (the cost the user took with Q14), and a long wave's tail goes bare sooner. Windows 21-22's 25-man
trio waves ran 31.7-40.8s with the add tank alive, window 22's 10-man ones 46.2-78.0s.

**Baseline** (`--tanks`, new fields: `no damage cut` minus `none` is the time under a health button alone):

- Window 22, 25-man: uld1 (DK) 0.0 and 10.0s, uld2 (bear) 24.9 and 0.0s; `both` 0.0s, `two externals` 0.0s in
  every wave. 10-man: 10.0 and 14.3s, 28.0 and 0.0s, 27.9 and 27.7s.
- Window 21, 25-man: uld1 10.0 and 7.2s, uld2 19.1s (trio 2's longest holder was the warrior main tank, 19.9s),
  uld3 21.5 and 0.0s.

**Next window should show** (`freya.py --tanks`):

- `both` above 0 only on health buttons: "0.0s on a damage cut" in every trio wave, but for a class node's last
  resort under 25%; time under a health button alone (above) well down where an external was in hand.
- `two externals` at most about 1.5s per handoff from one external to the next, and no gap between them.
- `freya.addext` casts landing under Frenzied Regeneration, Survival Instincts or Vampiric Blood, none under
  Barkskin or Icebound Fortitude.
- The cost: `none` at the end of long trio waves; add-tank deaths to the Snaplasher no more than windows 21-22's
  (window 21 uld2's bear, risen after dying to lashers; window 22 none at 25-man, and at 10-man uld3's bear bare at
  the end of trio 2 with every layer spent).
- Round 15's checks as written there; rounds 1-14 still holding.

**Left:**

- If add tanks die under a health button once the externals went on the one before (Survival Instincts after
  Guardian Spirit on roster C), the question is whether his health buttons should stop waiting out an external,
  so they run alongside it.
- A ready button he cannot cast (a risen bear in caster form for Frenzied Regeneration or Survival Instincts)
  holds the externals until he can, now at any health, as before round 14.
- The 1.5s lead covers a handoff between externals only; one from his damage cut to an external still waits
  for its end (windows 15-19's handoffs: 0.0-0.6s, twice 1.2-1.7s).

**Questions:** none new; Q16 and Q17 pending as written in round 15.

**Shared code:** none changed; `RunningTankDefensive` and `ReadyTankDefensive` are read, not edited.

**Review:** Q14 and Q15 built as answered (Q14 "replaces" the 25% rule), on one code path (no size, mode or player
branch), Freya files only. Reproduced on window 19: uld3's bear died under Survival Instincts at 0:44.602 and
4:58.276, the healers on `covered` from 0:16.2, which the new test answers with an external at once. uld2's paladin
was mid Holy Light (cast rows are cast starts: 4:11.725 and 4:13.211, 1.34s each) when Pain Suppression reached 1.5s
left, so the lead sends Hand of Sacrifice at 4:13.211 in place of the second, 0.14s before it ran out and 1.2s before
the bear died (sent 4:15.737); the other healers had no external left and the bear no button (Barkskin back at
4:42.9). The baseline matches `--tanks` on windows 21-22, wave lengths too. Fixed: reflowed the reader's ragged cover
paragraph. Left:

- The lane commit's message has no blank line after its subject, so git reads all four lines as the subject (no
  amend).
- A ready button he cannot press yet holds the externals at any health; now known-open in the findings doc.
