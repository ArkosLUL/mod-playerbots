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
