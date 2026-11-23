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

## Round 3 (2026-11-22, build 697052475)

**Data.** `raidtest.py report --boss faction-champions --raid toc --head 697052475 --pulls 10`, binary 18:21
(`hdr.bin` 1795364516), rounds 1 and 2 in it, roster DKs at full talents: toc1 `tr-20261122-162559-1` kill 7:07
(`649_4_thrakgar_1795364950`), toc2 `tr-20261122-162559-2` wipe 3:47 (`649_3_erin-misthoof_1795364958`), toc3
`tr-20261122-162559-3` kill 6:57 (`649_5_thrakgar_1795364962`). Scripts in the round's scratchpad `fc3/` (gone
with it); the reader's new `--personal` cells cover the key ones.

**Stop rules.** A wipe and 6 and 7 deaths: not polished. Round 3 picks a different failure.

**Deaths by killing blow.**

| Pull | Deaths | By |
|---|---|---|
| toc1 kill | 6 | Narrhok (iced 87%) 2, ranged; Gorgrim (iced), Harkzog, Vivienne, Thrakgar 1 each |
| toc2 wipe | 28 | Ruj'kah 6 + Cat 1, Maz'dinah 5, Ginselle 4, Narrhok (never iced: two ice DKs) 3, Gorgrim (never iced) 3, Harkzog 3, Broln 2 (after his ice DK died at 3:19), Erin 1. From 1:43, 5-6 champions on one ranged after another with no external left (three Hands of Sacrifice and a Hand of Protection went on Tocyak, an Enhancement bot, through 4-5 champion focuses at 0:11-1:30); 25 deaths in 2 min, raid DPS 82k (earlier toc2 kills 117-121k) |
| toc3 kill | 7 | Narrhok (uniced till 5:05) 2: Bladestorm, melee; Ruj'kah 2; Harkzog 2; Ginselle 1 |

The Tauren two: 9 of 41 (window 9: 14 of 21, window 8: 31 of 57).

**Verdict on round 1 (kite): held in part.** `--personal` kite catches: Broln 9 of 22 (41%, window 9 71%), 6 of
them Earth Shock, which no kite answers: melee 3 of 22. Narrhok 18/47, 13/24, 16/46, 40% (window 9 51%); iced
19 of 57 (33%), free 28 of 60. 26 of his 47 catches came from kites started within 8 yd (already in reach:
a CC broken or a switch next to the bot), 15 from 12-16 yd (Charge range). Maz'dinah 18 of 118 (15%).

**Verdict on round 2 (snare): half held.** The autorepeat fix works: hunters take the snare duty (`faction
champions snare` 12 OK rows on toc1, Wing Clip 10 and 16 on toc2 and toc3) and full-speed chases got a snare in
18%, 27%, 36% (window 9 8-16%). The concussive guard never fired: no veto row, and the class still cast it (55
`concussive shot on snare target` OK rows on toc1), iced 50 of 58, 6 of 28, 36 of 56 (window 9: 44/56, 43/52,
33/53). Narrhok out of his Chains still killed 3 on toc2 and 2 on toc3.

**Q1 measured.** Narrhok catches 40% of kites (iced 33%), Broln 41% (all iced, melee 14%); over ~35% both, so
the question goes back to the user (below). Deaths: Narrhok 7 of 41, top on toc1 and tied on toc3; Ruj'kah tops
the window with 9.

**Picked: the melee gates miss the Tauren two (bug, the reach audit's row).**

- White swings on non-tank raiders: Narrhok from a median 8.6, 8.1, 8.9 yd, 52-67% past 8, 23-34% past 10; Broln
  11.3, 94% past 8, 76% past 10; Gorgrim and Maz'dinah 4.7-6.5, 0-9% past 8, 0-3% past 10.
- Escape (`ToCHelpers_FactionChampionsBall.cpp:96` `ESCAPE_MELEE_REACH` 8): of the Tauren's swings on a mage,
  warlock, priest or hunter, 48 of 74 came with no melee champion within 8 yd of the victim, so the escape saw
  none; the rest 7 of 105. A Tauren chase parks at 8.075 yd (both reaches + 0.5). Every `fc.escape` with a Tauren
  nearest had him within 8.2 yd, past 8 only by snapshot lag (twice).
- Step-off (`:50` `STEP_OFF_TRIGGER` 8): the Tauren hit healers from past 8 yd on 14 of 18 swings, the rest 1 of
  14. Sprint and Dash contact (`:60` `BOOST_CONTACT` 8): 9 of 13 on rogues and druids, the rest 6 of 24.
  Disengage (`:118` `DISENGAGE_REACH` 5): Narrhok's 4 swings on hunters all from past 5.
- Peel (`ToCHelpers_FactionChampionsDefence.cpp:143` `PEEL_VICTIM_REACH` 10): 16 of Narrhok's 58 free swings came
  from past 10. Protect (`:169` `PROTECT_MELEE_REACH` 8): the >8 yd swings above go uncounted as attackers.
  AoE dodge (`:84` `DODGE_MELEE_GAP` 8) the same distance, not measurable in a trace.

**Picked too: the concussive guard's name (bug, small, independent).** `e.Block(..., {"concussive shot"})`
(`ToCDefinition_FactionChampions.cpp:175`) lists the creators[] key, but a rule matches `getName()` and
`CastSnareSpellAction::getName` returns `spell + " on snare target"` (`GenericSpellActions.h:399`). pblint passed
it since a creators[] key counts as known.

**Fix.** `FactionChampionsSwingReach` (`ToCHelpers_FactionChampions.cpp`): `GetMeleeRange` + `LEEWAY_BONUS_RANGE`,
7.7 for most, 11.6 for the Tauren two. Escape, step-off, Sprint/Dash/Nitro contact, protect and the AoE dodge
gate on it (8 → 7.7 for the rest); peel on it + 2.25 (10, 13.8); Disengage on `GetMeleeRange` (5.1, 8.9); the
escape's nearest is the one deepest inside its reach. The kite's `KiteReach` became that helper. The guard lists
`concussive shot on snare target`. pblint (shared tooling, no bot behaviour) adds a base's `getName()` suffix to
the names a rule may list and reads an init list on the line after the colon. Reader `--personal`: the nearest
melee champion and its distance at each escape, the escapes with it past 8 yd, kite catches iced apart.

**Next window should show.** `--vetoes`: `faction champions concussive guard` rows on `concussive shot on snare
target`; `--peel` hunter Concussive Shot iced near 0. `--personal`: escapes with Narrhok or Broln nearest past 8
yd (this window 0, 2, 0, snapshot lag), the Tauren's catches and deaths at or below this window's (40%, 41%;
9 of 41), healer `faction champions step off` moves with a Tauren on them.

**Seen, not picked.**

- The class paladin node `hammer of justice on snare target` stuns iced champions: 23 of 50 Hammers on champions
  landed on one in its Chains (17, 9, 16 OK rows), where a stun pops its trinket and the Chains with it. Same
  shape as the concussive guard; next round's candidate.
- toc2's collapse (above): the externals spent on one melee bot before the focus turned to the ranged, and Erin
  took 69 s as kill target.
- Kite catches still count any hit: Gorgrim's 63 of 103 are mostly Death Coil and Icy Touch, Broln's mostly Earth
  Shock.

**Question for the user.** Q1 again, with this window's numbers: may a disarm peel (Disarm, Dismantle) go on an
iced champion whose victim is no tank? Narrhok catches 40% of kites and Broln 41%, over the ~35% bar, but iced
the disarm's reach is smaller: iced Narrhok 33% (19 of 57), iced Broln 41% of which melee 14% (Earth Shock
ignores a disarm), and an iced Tauren killed 2 of 41 (Narrhok on toc1; Broln's two came after his DK died).
Recommended: not yet; this round's reach fix changes escapes, step-offs and externals against both, so judge the
iced split again next window and build it only if iced Narrhok still catches over ~35% or the iced Tauren top the
deaths.

**Review.** Both causes hold. Reread with the snapshot at or before each hit: white swings on non-tank raiders,
Narrhok a median 8.4, 8.1, 8.3 yd (55-64% past 8), Broln 11.3 (88%), the rest 4.6-6.9; Tauren swings on mages,
warlocks, priests and hunters with no melee champion within 8 yd 43 of 80, the rest 19 of 118 (lag included). The
class Concussive Shot ran as `concussive shot on snare target` (55, 17, 45 OK rows), no guard veto. No size, mode or
player branch.

- Review fix: the escape now fires on a Tauren 7.7-11.6 yd off, but `AoeEscapeClear` counted champions within the
  radius + 2 yd, while a player's AoE reaches a creature its combat reach past the radius
  (`WorldObjectSpellAreaTargetCheck`): a Tauren kill target hitting a priest from 10-11.6 yd would take its
  Psychic Scream (reach 14.1). `FrostTrapClear` had the same flat 12 yd against a slick reaching 10 + 0.39 (the
  dynamic object) + combat reach, 16.5 on a Tauren. Both read the champion's combat reach now (`InAoe`).
- Left: the opener's grip-guard check (`CollectTrinketsInReach`, radius + 1) misses a guarded champion its AoE
  reaches at radius + 1-2.25, a Tauren up to radius + 6.075; battle res and corpse crowding count champions within a
  flat 8 yd. Neither is this round's failure.

## Round 4 (2026-11-22, window 11 on build d2874f536)

**Data.** `raidtest.py report --boss faction-champions --raid toc --head d2874f536 --pulls 10`, binary 20:24
(`hdr.bin` 1795371886), rounds 1-3 in it: toc1 `tr-20261122-182719-1` kill 7:52, 5 deaths
(`649_1_erin-misthoof_1795372162`), toc2 `tr-20261122-182719-2` wipe 5:35, 28 (`649_2_caiphus-the-stern_1795372155`),
toc3 `tr-20261122-182719-3` wipe 4:50, 29 (`649_3_thrakgar_1795372158`). Window 10's three traces (round 3) and
window 9's reread for comparison. Scripts in the round's scratchpad `fc4/` (gone with it); the reader's new `--peel`
and `--trinket` lines cover the key ones.

**Stop rules.** Two wipes: not polished. Round 4 picks a different failure.

**Lineups.** toc1 and toc2 drew window 10 toc2's lineup (three healers Liandra, Caiphus, Erin; Broln and Narrhok both
in). toc3 drew window 10 toc1's: four caster DPS (Ruj'kah, Ginselle, Harkzog, Vivienne) behind Liandra, Thrakgar and
Erin, Narrhok the only Tauren.

**Deaths by killing blow.**

| Pull | Deaths | By |
|---|---|---|
| toc1 kill | 5 | Broln (iced) 2, Gorgrim (iced), Ruj'kah, Ginselle 1 each |
| toc2 wipe | 28 | Broln 8 (iced 3, the last his own ice DK Tocyai at 3:49, then 5 free in 40 s), Narrhok (never iced) 5, Ruj'kah 4, Gorgrim (never iced) 3, Maz'dinah 3, Ginselle 2, Harkzog 2, Zhaagrym 1. 8 deaths by 3:30 (window 10's toc2, same lineup: 25) |
| toc3 wipe | 29 | Ruj'kah 6, Ginselle 6, Gorgrim 5 (iced 4), Vivienne 4, Harkzog 4, Maz'dinah 3, Narrhok 1. Both warlocks on Fear duty dead by 0:55 (iced Gorgrim, iced Maz'dinah), every healer by 3:03, all four to casters; Ruj'kah 98→4% in 99 s as kill target |

**Did round 3 make it worse? No.** Window 10: 2 kills, window 11: 1. toc1 killed both windows (6, then 5 deaths). toc2
wiped both windows on the same lineup; this time it held 8 deaths by 3:30 against 25 and broke when Broln killed his
ice DK. The lost kill is toc3: casters made 20 of its 29 kills, the one Tauren there 1, so round 3's Tauren gates
had nothing to do with it; its roster has no Balance druid (no Cyclone duty, CC landed 1.9 per 10 s against 2.7-2.9),
and damage taken ran 11.6k/s against 7.2k/s for toc1 on the same lineup in window 10. What round 3 changed did no
measurable harm: escapes popped an iced Tauren's trinket once (toc2 0:23.9, Chains back 0.8 s later), healers'
`faction champions step off` share went from 2-3% to 2-7%, healing held (toc2 9.7k/s against 9.8k/s).

**Verdict on round 3 (reach, concussive guard): the guard held, the reach is live, the Tauren deaths didn't drop.**

- `--vetoes`: `faction champions concussive guard` 79, 47, 37 rows on `concussive shot on snare target`. Hunter
  Concussive Shot on iced champions 8 of 78 (window 10: 92 of 142); full-speed chases snared 31%, 28%, 40% (18-36%).
- `--personal`: escapes with a Tauren nearest past 8 yd 8, 6, 2 (window 10: 0, 2, 0). Kite catches Narrhok 35 of 84
  (42%, window 10 40%), Broln 32 of 86 (37%, 41%).
- Tauren kills 16 of 62 (window 10: 9 of 41). 11 came with no ice on the Tauren: Narrhok never iced on toc2 and toc3
  (no DK tank there, the two DPS DKs ice Broln or Gorgrim and Maz'dinah), Broln after his DK died.

**Q1 measured (iced Tauren).** Iced Narrhok caught 12 of 18 kites (67%), all by melee, but only on toc1 from 3:52 (the
only roster with a third DK); 9 of the 18 started inside his 11.6 yd reach, and he killed nobody iced. Iced Broln 29
of 78 (37%), 17 (22%) by a physical hit, the rest Earth Shock. The iced Tauren killed 5 of 62, all Broln (toc1 2,
toc2 3 with his own DK), against Ruj'kah's 11. Iced Narrhok is over ~35%, so Q1 goes back to the user (below).

**Failure ranking (62 deaths).**

1. Caster champions, 31 (Ruj'kah 11, Ginselle 9, Harkzog 6, Vivienne 4, Zhaagrym 1), 20 on toc3: they kill once
   the CC holders and healers are gone. No single mechanism in the trace; not root-caused this round.
2. Free melee champions, 19: Narrhok never iced 6, Broln after his DK's death 5, Gorgrim never iced 4, Maz'dinah 4.
   Who gets iced is the user's order (Ret, Enh, Rogue, DK, Warrior last): a plan matter.
3. Iced champions, 12: Gorgrim 5 (Death Coil, Death Grip then Frost Strike), Broln 5, Maz'dinah 2.

**Picked: the class Hammer of Justice on iced champions (bug, the orchestrator's pick from round 3).**

- `hammer of justice on snare target` (`GenericPaladinStrategy.cpp:23`, `CastHammerOfJusticeSnareAction`,
  `PaladinActions.h:272`) stuns the `snare target` value's pick, the first attacker moving at a non-tank. Windows 10-11:
  39 of its 79 Hammers on champions went on a champion its ice duty held in Chains (17 of 37 this window), against
  the Ice duty rule that an iced champion gets no other CC; the peel's own Hammer skips them
  (`ToCHelpers_FactionChampionsDefence.cpp:1905`).
- The cost is the Hammer, not the Chains: windows 9-11, 38 trinkets went off on iced champions (Hammer of Justice 9,
  Psychic Scream 7, the ice DK's own ghoul's Gnaw 5, Charge Stun 4); the DK's Chains came back a median 0.5 s later
  (max 8.8 s) and none led to a death within 10 s. Round 3's "frees them" holds for under a second. Each such Hammer
  is 40-60 s of cooldown the danger champion's peel can't use. Ranks below the three above; fixed as directed.

**Fix** (`ToCMultipliers_FactionChampionsDefence.{h,cpp}`, `ToCDefinition_FactionChampionsDefence.cpp`).
`faction champions ice guard multiplier` zeroes `CastHammerOfJusticeSnareAction` while its target is
`FactionChampionsIced`, so the class Hammer waits for a free champion. Reader: `--peel` counts trinket poppers aimed
at an iced champion by spell and class with the trinkets after (`iced_holds`); `--trinket` counts trinkets on iced
champions by trigger and the median wait for the DK's next Chains (`iced_trinkets`); the guard joins
`FC_MULTIPLIERS`.

**Next window should show.** `--vetoes`: `faction champions ice guard multiplier` rows on `hammer of justice on snare
target`. `--peel`: `Hammer of Justice paladin` near 0 in the iced line (this window 10 (2), 2 (0), 5 (1)).
`--trinket`: no Hammer of Justice among the iced triggers (this window 2, 0, 1). Paladin Hammers on free melee
champions up. No change in deaths expected from this alone.

**Seen, not picked.**

- The ice DK's own ghoul Gnaws its iced champion: 11 Gnaws on iced champions this window (`--peel`), 4 trinkets. Same
  family; warriors' Charge Stun and Intercept (3 and 3 on toc1) and hunters' Intimidation (2) too. All cost about as
  little as the Hammer's.
- Broln killed his own ice DK on toc2 (3:49) and Gorgrim the other DK on toc3 (3:24); each freed a champion for the
  rest of the pull.
- DK champion Chains of Ice on the raid freed by Hand of Freedom within 4 s: 11 of 21, 5 of 13, 5 of 14.

**Question for the user.** Q1, with this window's numbers: may a disarm peel (Disarm, Dismantle) go on an iced Tauren
whose victim is no tank? Iced Narrhok caught 12 of 18 kites (67%, one pull, killed nobody), iced Broln 29 of 78 (37%,
22% by melee). A disarm trips neither the trinket nor the Chains and halves the disarmed hand's damage. On Narrhok
either one stops Mortal Strike, Overpower and Bladestorm; on Broln only Dismantle (both hands) stops Stormstrike and
Lava Lash. Neither stops Charge or Earth Shock. Recommended: yes, Disarm and Dismantle only, expecting a small gain:
the iced Tauren killed 5 of 62.

## Round 5 (2026-11-22, window 12 on build ebe746ad1)

**Data.** `raidtest.py report --boss faction-champions --raid toc --head ebe746ad1 --pulls 10`, rounds 1-4 in it:
toc1 `tr-20261122-194109-1` kill 7:22, 5 deaths (`649_1_thrakgar_1795376590`), toc2 `tr-20261122-194109-2` wipe 5:30,
29 (`649_2_caiphus-the-stern_1795376597`), toc3 `tr-20261122-194109-3` wipe 5:57, 30 (`649_3_erin-misthoof_1795376599`).
Scripts in the round's scratchpad `fc5/` (gone with it); the reader's new `--ice` cells cover the key one.

**Stop rules.** Two wipes: not polished. Round 5 picks a different failure.

**Lineups.** toc1: Liandra, Caiphus, Thrakgar; Maz'dinah, Narrhok, Ruj'kah, Gorgrim, Harkzog, Ginselle, Birana; three bot
DKs (one the tank) ice Gorgrim, Narrhok and Maz'dinah all fight. toc2 and toc3 drew the same lineup, Erin for Thrakgar
and Broln for Birana: four melee champions against rosters B and C, two DPS DKs and no DK tank, so the ice order gives
Broln and Maz'dinah a DK and leaves Gorgrim and Narrhok free. toc3 has no Cyclone druid again (CC landed 2.05 per 10 s
against 2.93 and 3.07).

**Deaths by killing blow.** Iced: the duty's DK's Chains of Ice on it in the 10 s before; lapsed: a duty whose Chains
are older.

| Pull | Deaths | By |
|---|---|---|
| toc1 kill | 5 | iced Maz'dinah, Narrhok, Gorgrim 1 each; Harkzog, Ruj'kah 1 each |
| toc2 wipe | 29 | free Gorgrim 11, free Narrhok 3, Maz'dinah 3 after her DK died (3:36, Hellfire); iced Broln 2; Ginselle 4, Ruj'kah 3, Harkzog, Cat, Zhaagrym 1 each |
| toc3 wipe | 30 | free Narrhok 8 (3 healers), free Gorgrim 4, Maz'dinah 5 after her DK died (1:31, to lapsed Broln), Broln 1 after his DK died; Broln iced 5, lapsed 2; Harkzog 2, Cat 2, Ruj'kah 1 |

**Verdict on round 4 (ice guard): held.** `--vetoes`: `faction champions ice guard multiplier` 91, 53, 49 rows on
`hammer of justice on snare target`. `--trinket`: no Hammer of Justice among the triggers on iced champions (window 11:
2, 0, 1). `--peel` lists 2 paladin Hammers on iced champions on toc1 (window 11: 10, 2, 5), both at 0:01.8-1.9 before
either DK's first Chains landed (Gorgrim's came at 0:11.8), so the code's iced test was rightly false. Paladin Hammers on
free melee champions: not measured.

**Failure ranking (64 deaths).**

1. Free melee champions, 35: never iced 26 (Gorgrim 15, Narrhok 11, all on toc2 and toc3), freed by their DK's death 9.
   Melee champions did half the damage or more on 23 of 29 toc2 deaths and 29 of 30 toc3 deaths.
2. Casters and pets, 17 (window 11: 31): not first this time, so not root-caused; toc3's no-Cyclone lineup lost 5 to
   them.
3. Iced melee champions, 10 (Broln 7, the rest 1 each), plus 2 lapsed.

Never iced is the roster against the ice order (a plan matter, Q2 below). Inside the rest sits a bug: duties that hold a
DK lose their Chains.

**Picked: an ice DK's own wall grip and stale walk keep it off its Chains (bug).**

- Chains gaps over 10 s with the duty's DK alive: 17 on toc1, 8 on toc2, 2 on toc3. Tocyam left Maz'dinah unchained
  1:31-2:15 (44 s; Chains at 1:21 and 2:15) at 27-48 yd while she chased a melee bot at ~2.5 yd/s; she held the
  danger latch (1:46, 2:02), so the danger peel went off Gorgrim, who killed Tocyah at 2:02. Toczai's gap
  1:20.8-1:33.2 let Broln walk 44 yd onto Toczam, the other ice DK, and kill him at 1:31.5, which freed Maz'dinah for
  5 kills; his 2:08.7-2:26.7 gap gave Broln Toczac at 2:20.
- In those gaps every refresh released and re-took the DK's `fc.wall` duty on its own champion (157 notes in Tocyam's
  54 s): the stand sits 27 yd off the champion on the side away from its victim and is kept within 29 yd
  (`ToCHelpers_FactionChampionsDefence.cpp:91`, `:81`, keep `:1486-1489` before the fix), so a champion running at its
  victim leaves it inside one refresh, and `WallVictim` (`:1403`) let an ice DK take its own champion whatever its
  Chains. Each re-take walked `MOVEMENT_FORCED` (`ToCActions_FactionChampionsDefence.cpp:237`) to a stand past the
  20 yd Chains range, and `IsWaitingForLastMove` (core-side `MovementActions.cpp:1059-1071`, equal or lower priority
  waits) refused the ice reach's `MOVEMENT_COMBAT` walk: toc1 518 `faction champions ice reach` moves refused `wait`
  against 45 issued.
- By state, all three pulls: 442 wall holds taken on the DK's own champion, 276 with its Chains 6+ s old (due), 157 of
  them with the Chains gone, which gave 2 grips; in the Chains' first 6 s 166 holds gave 34 grips.
- With no wall duty the ice reach still waited out its own stale leg: it re-aims on every 0.5 yd drift of a spot 15 yd
  off a moving champion, each re-aim refused until the leg's lock expired (`ToCActions_FactionChampionsBall.cpp:54-67`
  before the fix; `WalkTo` stores the new spot before the refused `MoveTo`). Toczai's 1:20-1:33 gap: 30 `wait`s, 2
  legs issued, the DK 26-31 yd off.

**Fix** (`ToCHelpers_FactionChampionsDefence.cpp`, `ToCActions_FactionChampionsBall.cpp`). `OwnChainsHold`: the DK's own
Chains on the champion with over 4 s left (`ICE_RECAST_LEFT_MS`, the recast point); `WallVictim` takes and keeps an ice
DK's own-champion wall duty only while it holds, and `IceDue` reads the same test. The ice reach keeps a leg in flight
that ends within 3 yd of its spot; otherwise it lowers the booking to `MOVEMENT_NORMAL` and re-issues, restoring it when
nothing issued (pitfalls: a single mover is refused its own re-target). Reader `--ice`: per duty its wall holds, those
with its Chains due, and its grips.

**Next window should show.** `--ice`: `Chains due` near 0 (this window 18, 64, 29 on toc1; 22, 118 on toc2; 22, 3 on
toc3, of 29, 97, 61; 60, 135; 50, 10 holds); grips no lower (9, 7, 7; 6, 4; 6, 3); uptime up and the longest gap down
(93/84/93%, 11.8/19.1/12.5 s; 96/68%, 6.2/44.0 s; 96/97%, 8.0/3.1 s). Move rows: far fewer `faction champions ice
reach` `wait`s. No death to a champion whose DK lives but whose Chains lapsed (this window 2), and no ice DK killed by
its own or another lapsed champion.

**Seen, not picked.**

- Externals again piled on one Enhancement bot (Tocyak, toc2): Guardian Spirit, two Hands of Sacrifice, Hand of
  Protection and Lay on Hands in the first 51 s, two more Hands at 2:16-2:17.
- Harkzog's Hellfire killed Maz'dinah's ice DK on toc2 (3:36); two of his nine channels went unkicked (0:28, 1:13).
- Dazed pinned 23 of 59 deaths on toc2 and toc3, Narrhok's Charge Stun 9: kiters hit from behind.
- Full-speed chases snared in the 4 s before: 22%, 18%, 24% (window 11: 31%, 28%, 40%).

**Questions for the user.**

- Q1, this window's numbers: iced Broln killed 7 (toc2 2, toc3 5) and 2 more with his Chains lapsed; iced Narrhok 1 (toc1).
  Recommendation unchanged: yes, Disarm and Dismantle only.
- Q2 (plan): rosters B and C bring two DPS DKs and no DK tank, so four melee champions leave two never iced; Gorgrim and
  Narrhok killed 26 of 59 on toc2 and toc3 this window, 10 in window 11. Over windows 11-12 the free ones killed: Gorgrim
  19, Narrhok 17, Maz'dinah 12, Broln 6. Should the ice order (Ret, Enh, Rogue, DK, Warrior) change when DKs run short?
  Recommended: not yet. Judge this round's fix first, which keeps the iced pair iced (lapses cost toc3 7 deaths through
  Toczam's); if the never-iced DK and Warrior still top the deaths, try the DK ahead of the Rogue for one window.

**Review.** The cause holds in the trace: in Tocyam's 1:21-2:15 the wall duty was re-taken 81 times and the ice reach
issued 18 legs against 200 refused `wait`. Two side effects to watch: a champion whose ice DK's Chains are off gets no
wall grip from anyone, also while Hand of Freedom makes Chains useless (such holds gave 2-3 grips this window); and the
ice reach now replaces a generic `avoid aoe` flee in flight (`MOVEMENT_COMBAT`, retried after 1 s), none seen for an
ice DK in window 12.

## Round 6 (2026-11-22, window 13 on build a55056c40)

**Data.** `raidtest.py report --boss faction-champions --raid toc --head a55056c40 --pulls 10`, rounds 1-5 in it: toc1
`tr-20261122-205905-1` wipe 5:19, 29 deaths (`649_1_caiphus-the-stern_1795381276`), toc2 `tr-20261122-205905-2` kill
7:02, 3 (`649_2_thrakgar_1795381255`), toc3 `tr-20261122-205905-3` kill 7:30, 4 (`649_3_thrakgar_1795381274`). Windows
11 and 12 reread for comparison. Scripts in the round's scratchpad `fc6/` (gone with it); the reader's new `--personal`
line covers the key one.

**Stop rules.** A wipe: not polished. Round 6 picks a different failure.

**Lineups.** toc1 drew window 11 toc1's: healers Liandra, Caiphus, Erin; Maz'dinah, Narrhok, Ruj'kah, Broln, Gorgrim,
Harkzog, Ginselle. Four melee champions against three bot DKs (DK tank Tocxam on Broln, Tocxah on Maz'dinah, Tocxal on
Gorgrim), so Narrhok is never iced. toc2 (Liandra, Caiphus, Thrakgar; Birana for Broln) and toc3 (Liandra, Thrakgar,
Erin; Vivienne for Broln) have three melee champions against two DPS DKs: Narrhok free until a DK's champion died (4:29,
5:08).

**Deaths by killing blow.** Iced: the duty's DK's Chains on it in the 10 s before; lapsed: older, the DK alive.

| Pull | Deaths | By |
|---|---|---|
| toc1 wipe | 29 | Ruj'kah 7, iced Broln 5 + lapsed 1, Ginselle 4, iced Maz'dinah 3 + 1 after her DK died, iced Gorgrim 2 + lapsed 2, Narrhok (never iced) 2, Harkzog 2. 6 dead by 0:45, 4 to Broln; 16 in the collapse from 3:38 |
| toc2 kill | 3 | Narrhok (free) 2, iced Gorgrim 1 |
| toc3 kill | 4 | Ruj'kah 2, Vivienne 2 |

**Why toc1 wiped (it killed with 5 deaths on the same lineup in window 11).** The opening: Broln, iced all pull, dealt
240k in the first 60 s against 81k in window 11 and killed 4 of the first 6. His DK, the tank Tocxam, was Blinded by
Maz'dinah at 0:11.6 and Intimidating-Shouted by Narrhok at 0:14.4 until 0:22.4, so his Chains lapsed 0:16-0:23; kiters he
reached were Dazed (12 of the 29 deaths pinned by Dazed). Three of the eight CC bots died by 0:45 (Tocxas, Tocxap,
Tocxak), the casters then held only 34-45% of their time off the kill target, the three healers took 3:00 to kill, raid
DPS fell to 70k (median 106k), and Ruj'kah killed 7. Narrhok, never iced, killed 2, both in the collapse.

**Verdict on round 5 (own wall grip, stale ice reach): held.**

- `--ice` Chains due: toc1 0, 0, 0 of 27, 19, 27 wall holds; toc2 0, 0, 0 of 29, 44, 11; toc3 1, 1, 0 of 36, 61, 4 (window
  12: 18-118 per duty). Grips 5, 5, 6; 7, 7, 2; 6, 8, 2 (window 12: 9, 7, 7; 6, 4; 6, 3).
- Uptime 98/77/94%, 96/97/98%, 98/97/97%, longest gaps 3.1/33.1/7.1, 7.8/6.6/1.5, 2.3/5.6/2.9 s (window 12: 93/84/93,
  96/68, 96/97; 11.8/19.1/12.5, 6.2/44.0, 8.0/3.1 s). Recasts a median 6.1-6.7 s apart.
- `faction champions ice reach` `wait` rows 0, 0, 0 (window 12: 518, 462, 120), legs issued 116, 59, 68 (45, 47, 11).
  Chains lapse time with the DK alive by state: out of range 35 s, in range 14 s, kiting another champion 37 s (window
  12: 150, 28, 2.5; window 11: 166, 18, 1.3).
- Lapsed-champion deaths 3 (window 12: 2), from two new causes: Broln's while his DK was CC'd (Tocxas 0:21), and
  Gorgrim's while his DK Tocxal kited Broln (Tocxaj 4:03.8; Tocxah 4:08.6, Maz'dinah's ice DK, with iced Maz'dinah 41%).

**Q2 (ice order) not due.** Never-iced champions did not top the deaths: Narrhok killed 2 never iced on toc1 and 2 free
on toc2, 4 of 36, behind Ruj'kah 9 and iced Broln 5. With one free melee champion the danger peel held Narrhok: CC 88,
141, 95 s, danger 203-290 s.

**Failure ranking (36 deaths).**

1. Melee champions, 19: iced 11 (Broln 5, Maz'dinah 3, Gorgrim 3), lapsed 3, free Narrhok 4, Maz'dinah after her DK
   died 1. Melee champions did half the damage or more on 15 of 29, 3 of 3 and 0 of 4 deaths.
2. Casters, 17: Ruj'kah 9, Ginselle 4, Harkzog 2, Vivienne 2; 13 on toc1, most in its collapse. Off the kill target they
   were CC'd 34-45% of their time on toc1 with nobody on them; no bot fault found, they follow the melee losses.

**Picked: a kite re-hop is refused by the bot's own kite walk (bug).**

- `FactionChampionsKiteAction::Execute` (`ToCActions_FactionChampionsBall.cpp:133-149` before the fix) walks each hop at
  `MOVEMENT_FORCED`. The kite re-plans on arrival or after `KITE_STEP_MS` 2.5 s (`ToCHelpers_FactionChampionsBall.cpp:94`,
  latch `:1045-1055`), but the hop in flight holds a lock of its length over the run speed at issue
  (`MovementActions.cpp:366-372`, capped 5 s), and `IsWaitingForLastMove` (`:1059-1070`) refuses an equal priority: a
  Dazed kiter's 15 yd hop locks 4.3 s, so its next hop waits ~1.8 s; a healthy one arriving 2 yd short waits ~0.3 s.
- Move rows: 303 `faction champions kite` re-hops waited behind the bot's own kite walk (3 behind another action's), 276
  issued within 6 s: median 0.3, 1.2, 0.3 s, 110 over 1 s. At the refusal the stale walk ended a median 15.4 yd from the
  chaser against the refused hop's 26.1 (nearer in 102 of the 110 long ones); melee champion damage on the waiting bots
  229k, 290k, 121k (639k). Window 12 the same: 313 refusals with a chaser on the bot, 97 over 1 s, 586k.
- toc1 0:39.1-0:44.3: Tocxao (shadow priest, Dazed at 0:37.0) hopped 14.4 yd at 3.4 yd/s; its next hop off Broln, 0:41.6,
  was refused twice (`hms` 1720 ms, then 696) and issued at 0:43.4 as the lock ran out, while it walked on to the stale
  spot and Broln closed 5.0 → 1.6 yd; 92% → 8%, dead at 0:44.3.
- 13 of the 17 deaths melee champions did half of (the round's script; the reader counts 18) had such a wait in the 5 s
  before (an active kite re-plans every ~2.5 s, so this bounds the gain rather than measures it).
- For an ice DK round 5's ice reach already slipped in: its kite refused, the ice reach lowered the kite's booking and
  walked toward its champion, and the kite re-issued 0.1 s later (Tocxal 2:05-2:10).

**Fix** (`ToCActions_FactionChampionsBall.{h,cpp}`). The kite remembers its last issued hop; when the booking in flight is
that hop at `MOVEMENT_FORCED`, it lowers it to `MOVEMENT_COMBAT` before walking the new hop and restores it when nothing
issued (pitfalls: a single mover is refused its own re-target). A dodge, Nitro run or wall grip in flight still holds. The
latch keeps re-hops to arrival or 2.5 s, so the legs don't flip. Reader `--personal`: re-hops refused by the bot's own kite
walk, how many over 1 s, the median wait, those never issued, the melee champion damage meanwhile (`kite_waits`).

**Next window should show.** `--personal` "re-hops refused by the bot's own kite walk" near 0 (this window 86 of 232, 93
of 228, 97 of 258 hops; 30, 50, 30 over 1 s; 229k, 290k, 121k meanwhile); `faction champions kite` `wait` move rows near
0 (410); hops per kite not far above this window's (113, 128, 143 kites). Kite catches no higher (toc1 Maz'dinah 7/29,
Gorgrim 10/26, Narrhok 4/21, Broln 6/19; toc2 Narrhok 13/47, Gorgrim 25/37, Maz'dinah 9/33; toc3 Narrhok 18/46, Gorgrim
17/45, Maz'dinah 9/41) and fewer deaths melee champions did half of (15/29, 3/3, 0/4).

**Seen, not picked.**

- An ice DK kites another champion off its own: Tocxal kited iced Broln 2:02-2:43 up to 63 yd from Gorgrim (Chains gap 33
  s) and again 3:59-4:09; lapsed Gorgrim then killed Tocxaj and Tocxah. The kite takes any melee champion on a non-tank
  (the user's call) and the ice reach sits under it. If it recurs, a plan question: should an ice DK's hops stay within
  Chains range of its champion?
- The DK tank's CC (Blind, Intimidating Shout) lapsed Broln's Chains for 7 s in toc1's opening.
- Tocyak's Greater Fire Elemental (15438) had Ruj'kah as target in 79% of his toc2 samples off the kill target and out
  of CC, which holds Polymorph, Fear and Hex off him (CC'd 35% against Ginselle 58%, Harkzog 51%).
- The wall grip also refuses its own re-aim: 79 such waits, median 1.6 s.
- Iced Gorgrim kills at range: Tocxap (toc1 0:41.6) took Death Coil and Icy Touch from 13-17 yd, his Chains, then Frost
  Strike.

**Questions for the user.** Q1, this window's numbers: iced Broln killed 5 on toc1 (one more lapsed) and dealt 240k in
its first 60 s; he caught 6 of 19 kites (32%), and iced Gorgrim 7 of 19, 25 of 37, 16 of 44, mostly Death Coil and Icy
Touch. Recommendation unchanged: yes, Disarm and Dismantle only. Q2: not due (above); recommendation unchanged.

**User answer (2026-11-22) to Q1:** yes: a disarm peel (warrior Disarm, rogue Dismantle) may go on an iced
Tauren champion whose victim is not a tank.

## Round 7 (2026-11-23, builds Q1; no new pulls)

**Data.** None new: window 14 is pulling round 6. Window 13 (round 5 build `a55056c40`, the three traces of round 6)
reread for the baseline with the reader's new `--peel` line. Scratch script in the round's scratchpad `fc7/` (gone with
it).

**Stop rules, verdict, ranking.** Skipped: no pull has round 6 in it. Deaths as round 6's table: toc1 wipe 29 (Ruj'kah 7,
iced Broln 5 + lapsed 1, Ginselle 4, Maz'dinah 3 + 1, Gorgrim 2 + 2 lapsed, Narrhok never iced 2, Harkzog 2), toc2 kill 3
(free Narrhok 2, iced Gorgrim 1), toc3 kill 4 (Ruj'kah 2, Vivienne 2).

**Q1 checked before building.**

- Disarm lands on both: Narrhok 34453 and Broln 34455 carry `CreatureImmunitiesId` -12 (25H 35720, 35682: -13), which
  `creature_immunities` gives charm only (-13 adds taunt and Attack Me); no disarm mechanic (3). Addon aura only
  Champion's Aegis 68595 (aura 229). No `spell_linked_spell` row on 676, 51722 or the gated spells. `flags_extra` carries
  `CREATURE_FLAG_EXTRA_ALL_DIMINISH`, so `DIMINISHING_DISARM` applies. Bladestorm 65947's immunity mask 477 maps
  (`SpellInfo` special case) to snare, root, fear, stun and the like, not disarm.
- DBC: Disarm 676 auras 67 (main hand), 278 (ranged), Defensive Stance only (Stances 0x20000, form 18), so only a
  prot warrior casts it; Dismantle 51722 auras 67, 254 (off hand), 278, 60 s. Both 10 s, mechanic 3.
- What it stops (`boss_faction_champions.cpp`, rechecked every 5 s): Narrhok skips Bladestorm's start, Mortal Strike
  (68784 on 25H), Overpower, Sunder Armor, Shattering Throw and Retaliation, never Charge or Intimidating Shout. Broln's
  Lava Lash needs the off hand and Stormstrike skips only with both hands gone, so only Dismantle stops them; his Earth
  Shock (68102) is gated by silence only. Gorgrim (DK) loses only Frost Strike; Chains of Ice, Death Coil, Death Grip,
  Icy Touch and Strangulate go on. So no disarm stops a champion's ranged casts, only its weapon specials.
- White swings: the main hand keeps full damage. `Creature::CalculateMinMaxDamage` multiplies its out-params by 0.5
  before it computes them, so the halving the boss doc cited (`StatSystem.cpp:1156`) is dead. A disarmed off hand neither
  swings (`UnitAI::DoMeleeAttackIfReady`) nor deals damage, so Dismantle takes Broln's off-hand swings too; Narrhok
  carries a shield (46964).
- The PvP trinket fires on fear, freeze, stun or confuse only (`IsCCed`), so a disarm leaves trinket and Chains alone.

**Baseline, window 13** (`--peel`, the iced Tauren's disarm-gated casts while iced): toc1 iced Broln 48 (Stormstrike 25,
Lava Lash 23; 345k of his 748k, melee 323k), 0 disarms on him; toc2 iced Narrhok 2 disarms (5:50), 1 gated cast within
10 s after one, 5 otherwise; toc3 iced Narrhok 0 disarms, 14 gated. Disarms on champions: rogue Dismantle 3, 6, 3; the
prot warrior tank's Disarm 6 on toc2 (4 off a peel duty).

**Fix** (`ToCHelpers_FactionChampionsDefence.cpp`). The peel takes an iced Tauren (Broln, Narrhok) whose victim is no
tank within its swing reach + 2.25 yd, ranked after every free champion, and gives it only `ReadyDisarm`: in melee, not
already disarmed, a rogue's Dismantle or a Defensive Stance warrior's Disarm, the latter never on an Enhancement (it
stops nothing there), free champions included. The peel's DR, immunity, LOS and guard checks apply as before; no other
peel, CC or snare reaches an iced champion. Reader `--peel`: peels on an iced champion counted (`iced`), and per Tauren
the raid's Disarms and Dismantles while iced with its disarm-gated casts while iced, within 10 s after one or not
(`iced_disarms`). Boss doc: the disarm mechanics corrected (no damage halving, Defensive Stance), Peel, Ice duty.

**Next window should show.** `--peel`: "Disarm or Dismantle while iced" above 0 wherever a Tauren is iced (window 13: 0,
2, 0), its gated casts "within 10 s after one" few against "otherwise" (48, 5, 14 this window), `Dismantle rogue ...
iced` above 0, no warrior Disarm on Broln. No more deaths to an iced Tauren (window 13: iced Broln 5 on toc1, iced
Narrhok 0). A small gain at most: two rogues on a 60 s cooldown, in melee only, the second within 15 s at half duration.

**Seen, not picked.** Every warrior Disarm comes from a prot tank in Defensive Stance; the DPS warriors never cast one.

**Questions for the user.** None new. Q2 (ice order with fewer DKs than melee champions) still not due; recommendation
unchanged.
