# Polish log: Faction Champions (25H)

Trace slug `faction-champions`, raid `toc`, scenario `totc-champions`: after the intro DC engages the first
kill-order healer on the main tank. The grip chain needs a player's `pull`, which DC doesn't give, so the
healers die in place. Traces file under the first champion engaged. Reader:
`tools/botobs/bosses/faction_champions.py`.

## Round 1 (2026-11-22, build 02925e260)

**Data.** Window 8, pools from `env/dist/logs/dc/dc_testruns.jsonl` (the round's prompt had toc1 and toc3
swapped): toc1 `tr-20261122-114251-18` wipe 10:49 (`649_19_erin-misthoof_1795347937`), toc2
`tr-20261122-114231-16` kill (`649_17_erin-misthoof_1795347922`, 6:24 in the trace), toc3
`tr-20261122-114231-17` kill (`649_16_erin-misthoof_1795347913`, 7:31). Window 7 smoke, toc1
`tr-20261122-093459-16` kill (`649_16_caiphus-the-stern_1795340203`). Scripts in the round's scratchpad
`fc/` (gone with it); the reader's new `--personal` line covers the key one.

**Stop rules.** Not polished (toc1 wiped, toc3 9 deaths). First round.

**Deaths by killer.** Narrhok (Warrior) and Broln (Enhancement) are the Tauren, combat reach 6.075.

| Pull | Deaths | By |
|---|---|---|
| toc1 w8, wipe | 34 | Broln, iced all fight by the DK tank, 15 (healers 7, Tocxac 4 of them across 3 Rebirths; melee 4, ranged 3, tank 1); Gorgrim (DK) 10, 7 while Tocxal iced him; Mage 4; Narrhok 2; Hunter 1; pets 2. 8 of 10 dead at the wipe, Broln (last in kill order) untouched |
| toc2 w8, kill | 1 | Hunter |
| toc3 w8, kill | 9 | Narrhok, uniced till 4:53, 5 (healer Toczae 3, ranged 2); Gorgrim, Hunter, Mage, Malithas (Ret, iced) 1 each |
| toc1 w7, kill | 13 | Broln (iced) 9, Gorgrim (iced) 2, Hunter 1, Warlock 1 |

The two Tauren: 31 of 57.

**Picked: a crawling Tauren chaser catches its kiter as the kite starts (bug).**

- Melee hits within 120 ms of a snapshot: Broln and Narrhok from a median 9.1-9.3 yd, p90 11.0-11.4; Gorgrim,
  Maz'dinah and Malithas p90 7.5-7.7, max 8.4.
- DB `2026_11_06_01` gave models 29784 (Broln) and 29793 (Narrhok) CombatReach 6.075, the others 2.25:
  `GetMeleeRange` on a player 8.9 yd (others 5.1), plus `LEEWAY_BONUS_RANGE` 2.66 while both move
  (`Unit::IsWithinMeleeRange`): 11.6 (7.7).
- `KiteChasers` (`ToCHelpers_FactionChampionsBall.cpp:408-415` before the fix) skipped a crawling chaser (run
  rate ≤ 0.6: Chains of Ice's first ~5 s, any 50% snare) past a flat `KITE_CRAWL_REACH` 10 yd, set when every
  champion's reach was 1.5. The kite started inside the Tauren's moving reach.
- Kite starts (first hop, none in the 3 s before) with the nearest chaser at ≤ 4.2 yd/s and the note at 9 yd,
  hit by it within 2 s: Broln 57/64, Narrhok 38/49, Gorgrim 6/75, Maz'dinah 10/72. From 12 yd on, Broln and
  Narrhok drop to 12-33%.
- toc1 1:06-1:22: Tocxag (mage) kited from Broln at 11, 9, 12, 9, 9 and 8 yd; he hit it from 9-11 yd after
  each (melee 9950 at 1:07.8, Lava Lash 1:12.7, Stormstrike 1:20.0); dead at 1:22.2.
- `BreakChasedCasts` left the leeway out, so a cast with up to ~1.6 s left could finish with a Tauren at its
  standing reach, inside its moving one once the kite began.

**Fix** (`ToCHelpers_FactionChampionsBall.cpp`). `KiteReach` = `GetMeleeRange` + `LEEWAY_BONUS_RANGE`; a
crawler counts within `KiteReach` + 2.25 yd (10 for the 2.25-reach champions as before, 13.8 for the Tauren
two, the Ret's 17 a floor); the cast break subtracts `KiteReach`. Reader: `--personal` prints per melee
champion the kites it was the nearest chaser at and how many it hit within 2 s (`kite_catches`);
`show_damage` survives a champion with 0 damage (it crashed the toc1 and toc2 reports).

**Next window should show.** That `--personal` line for Broln and Narrhok well under today's 52/73, 52/66
and 30/50, 17/36, 27/41, 32/58 (47-79%), toward Maz'dinah's and Malithas' 24-30%, and fewer deaths to the
two. `fc.castbreak` notes rise some (toc1: 26).

**Seen, not picked.**

- Gorgrim's damage is half ranged (Death Coil 332k, Icy Touch 132k of 934k on toc1), which no kite answers.
- An iced champion gets no peel, CC or danger (the user's 2026-10-28 call), and the Enhancement is neither in
  the danger order nor early in the kill order, so on toc1 it roamed the whole 10 min.
- The DK tank's wall grips landed Broln 7-10 yd from a healer twice (3:31, 5:56); Tocxac died 5 and 15 s
  later. The stand keeps within 38 yd of a healer, no distance off the raid.
- toc1: Tocxac raised three times, dead again after 22, 27 and 105 s, twice to Broln.
- Roster DKs at ~25 of 71 talents: toc1 fields three (blood tank, frost, unholy), all on ice duty. Chains of
  Ice needs no talent (uptime 84-99%), but raid DPS was 64k (77k in window 7), toc1 kills took 36-117 s each
  against 27-61 s on toc2 and toc3, and the 10 min fight is where the Enhancement piled up its kills. Not
  worked around.

**Question for the user.** Q1, only if the next window still shows the iced Tauren killing: may a disarm peel
(Disarm, Dismantle) go on an iced champion whose victim is no tank? A disarm trips neither the PvP trinket nor
the Chains, halves its weapon damage and gates its weapon abilities (`UNIT_FLAG_DISARMED`). Recommended: yes,
if Broln or Narrhok still catch over ~35% of kites or top the deaths.

## Round 2 (2026-11-22, window 9 on build 1b45df985)

**Data.** Window 9, `raidtest.py report --boss faction-champions --raid toc --head 1b45df985 --pulls 10`: toc1
`tr-20261122-131418-1` kill 7:02 (`649_3_erin-misthoof_1795353392`), toc2 `tr-20261122-131418-2` kill 6:37
(`649_4_erin-misthoof_1795353405`), toc3 `tr-20261122-131419-3` kill 6:51 (`649_6_caiphus-the-stern_1795353409`).
Window 8's three traces (round 1) reread for the snare numbers. Scripts in the round's scratchpad `fc2/` (gone
with it); the reader's new `--peel` lines cover the key ones.

**Stop rules.** Three kills, but 12 and 7 deaths. Not polished. Round 2 picks a different failure.

**Verdict on round 1: not judged.** The window 9 binary dates from 15:10 (trace `hdr.bin` 1795353008), before the
round 1 fix existed (`6890b74d5` 15:30, merged `b7c91ff35` 15:42), so window 9 is more pre-fix data. Its
`--personal` kite catches match window 8: Broln 35 of 49 (71%), Narrhok 18/39, 23/48, 29/49 (46-59%), Gorgrim
20/36, 15/43, 21/39, Maz'dinah 8/37, 3/40, 14/44. Window 10 judges round 1 against these.

**Deaths by killer.** Broln is only in toc1's lineup.

| Pull | Deaths | By |
|---|---|---|
| toc1 w9, kill | 12 | Broln (iced all fight by the DK tank, 92% uptime) 10: ranged 9, healer 1; Maz'dinah (iced) 1; Gorgrim (iced, Death Coil) 1 |
| toc2 w9, kill | 2 | Liandra (Holy Paladin) melee on a Dazed melee bot 1; Gorgrim (iced, Frost Strike) 1 |
| toc3 w9, kill | 7 | Narrhok, uniced till 4:40, 4: two full-speed chases with no snare (1:09, 1:21), one Bladestorm chase (2:28), one snared crawl hit from 9-11 yd (3:25, round 1's case); Ruj'kah (Hunter) Shoot 3: melee, ranged, healer |

The Tauren two: 14 of 21. Narrhok out of his Chains killed 9 on toc3 over windows 8 and 9 (window 8: 5, two of
them after a Charge).

**Picked: no snare reaches an unslowed melee champion running down a kiter (bug).**

- `--peel` full-speed chases (a melee champion out of its ice duty's Chains at 6+ yd/s within 15 yd of the
  raider it targets, no tank): window 9 44.0, 22.5 and 57.0 s, snared in the 4 s before 8%, 16% and 11%; window
  8 55.0, 29.5 and 90.0 s at 14%, 14% and 17%. 298 s in all, 13% snared. Narrhok alone 21.5, 21.5 and 40.0 s in
  window 9. A champion runs ~8 yd/s against our 7, so an unsnared kite only delays. A snare that holds ends the
  run, so a snared step is mostly the one it landed in (window 9: 24 of 27 within 1 s of the cast).
- toc3 0:52-1:09: Narrhok on Toczal at 6-8 yd/s, 2-3 yd off from 0:56, no snare, peel or CC on him until a
  Polymorph at 1:09.260, 86 ms before the killing Mortal Strike. Toczak's Concussive Shot came off cooldown at
  0:55.3 with him 31-38 yd from Narrhok; he spent it at 0:59.040 on Gorgrim, Chains of Ice on him from 0:58.865.
  1:16-1:22: Narrhok on Toczav after his Charge, no snare; Toczau's was ready from 1:16.5 at 13-18 yd and went on
  Gorgrim at 1:18.938.
- Hunters seldom take the snare duty: 25 of its 154 casts in the six pulls, each with Auto Shot off.
  `UpdateSnares` takes only `FreeToAct` snarers (`ToCHelpers_FactionChampionsDefence.cpp:1740`, `FreeToAct` at
  `:1119` before the fix: `!IsNonMeleeSpellCast(false)`), and that counts the autorepeat slot
  (`Unit::IsNonMeleeSpellCast`, core `Unit.cpp:4094`), where Auto Shot sits all fight. Hunter snapshot rows show
  a current spell in 92-96% of live samples (Auto Shot alone 26-42%). The peel duty's Freezing Arrow and the
  Hellfire kick's Silencing Shot lose them the same way. After the fix a hunter still skips during a cast-time
  shot (Steady Shot), 25-46% of its time.
- Their Concussive Shots come mostly from the class nodes instead (`GenericHunterStrategy.cpp:68` `has aggro`,
  `:77` `concussive shot on snare target`), whose `SnareTargetValue` (`SnareTargetValue.cpp:12-55`) takes the first
  attacker moving at a non-tank or fleeing. Windows 8 and 9, `--peel`: 350 hunter Concussive Shots on champions,
  228 (65%) on one in its ice duty's Chains; by the round's script, 46 on casters (13%) and 18 on feared ones
  (5%). The 12 s cooldown is usually spent when a free champion turns on a kiter.

**Fix** (`ToCHelpers_FactionChampionsDefence.cpp`, `ToCDefinition_FactionChampions.cpp`). `MidCast` skips the
autorepeat slot; `FreeToAct`, the CC dispellers and `ReadyRangedInterrupt` use it, so a hunter with Auto Shot
going (or a bot firing a wand) can take the snare, peel, protect and other duties. The rule `faction champions
concussive guard` zeroes the class `concussive shot` while the fight is live, so the cooldown waits for the snare
duty. Reader: `--peel` counts snares landing on an iced champion (`iced`) and prints the full-speed chases and
the share a snare reached; `concussive guard` joins `FC_MULTIPLIERS`.

**Next window should show.** `--vetoes`: `faction champions concussive guard` rows on `concussive shot`.
`--peel`: hunter Concussive Shot `iced` near 0 (window 9: 44 of 56, 43 of 52, 33 of 53) and the full-speed
chases snared well above 8-17%, toward 30%+. Fewer deaths to Narrhok out of his Chains on toc2 and toc3 (window
9: 4 on toc3, window 8: 5). Round 1's kite catches (above) are read on the same window.

**Seen, not picked.**

- Pets Daze our kiters most: Dazed applications by the hunter champion's Cat 21, 16 and 28 and the warlock's
  Zhaagrym 3, 20 and 9, against 1-10 per melee champion. Pets are never picked or CC'd.
- Ruj'kah (Hunter) killed 3 on toc3, a healer among them; only CC and the kill order answer the casters.
- Narrhok's Charge (8-25 yd, Charge Stun) opens two of window 8's toc3 deaths; no kite answers a Charge.
- Roster DKs still at ~25 of 71 talents (DC re-specs them before window 10). Not worked around.

**Question for the user.** Q1 from round 1 still waits on a window with round 1 in it. On pre-fix code the
Tauren two killed 14 of 21 in window 9 and Broln was hit within 2 s after 71% of the kites he chased, Narrhok
46-59%, against ~35% as the bar. The question: may a disarm peel (Disarm, Dismantle) go on an iced champion whose
victim is no tank? Recommended: yes, applied in the round after window 10 only if Broln or Narrhok still catch
over ~35% of kites or top the deaths there.
