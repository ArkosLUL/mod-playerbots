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

**Review.** The case holds: on window 13 toc1 Broln dealt 707k of his 748k while iced, Lava Lash 165k and Stormstrike's
two hits 180k, and the reader reruns the baseline above unchanged. The core facts check out: Lava Lash gated on the off
hand, Stormstrike on both, the halving dead, `IsCCed` blind to a disarm. No size, mode or player branch: the entries are
the base ones on every difficulty, and a player's disarm counts through the unit flags.

- Review fix: `ReadyDisarm` read Broln as disarmed on the main-hand flag, so a main-hand-only disarm (a player's Disarm,
  Psychic Horror) kept the rogues' Dismantle off him, though only the off hand gates Stormstrike and Lava Lash. On an
  Enhancement it reads the off-hand flag now.
- Left: the disarm goes to the Tauren two by entry, as the user answered, so against a Horde raid the Alliance Warrior
  and Enhancement get none while iced.

## Round 8 (2026-11-23, window 14 on build 1fa3c332c)

**Data.** `raidtest.py report --boss faction-champions --raid toc --head 1fa3c332c --pulls 10`, rounds 1-6 and the shared
tank-defensive fix (`75179aba3`, `6aba04b73`) in it, round 7 (`d3e7d1025`) merged but not built: toc1
`tr-20261123-095334-1` wipe 3:55, 31 deaths (`649_1_caiphus-the-stern_1795427739`), toc2 `tr-20261123-095334-2` kill
6:22, 6 (`649_2_caiphus-the-stern_1795427742`), toc3 `tr-20261123-095334-3` wipe 3:28, 29
(`649_4_erin-misthoof_1795427733`). Window 13 reread for comparison. Scripts in the round's scratchpad `fc8/` (gone with
it); the reader's new `--adds` cell covers the fixed one.

**Stop rules.** Two wipes: not polished. Round 8 picks a different failure.

**Lineups.** toc1 and toc3 drew window 13 toc1's: healers Liandra, Caiphus, Erin; Maz'dinah, Narrhok, Ruj'kah, Broln,
Gorgrim, Harkzog, Ginselle. toc1 (roster A, three DKs) leaves Narrhok never iced, toc3 (roster C, two DKs, no Cyclone
druid) Narrhok and Gorgrim; that pairing also wiped in windows 11 and 12. toc2 drew window 13 toc2's (Birana for Broln).

**Deaths by top damage dealer** (its share of the champion damage in the 10 s before). Iced: its duty's Chains in the
10 s before; freed: its ice DK dead.

| Pull | Deaths | By |
|---|---|---|
| toc1 wipe | 31 | iced Broln 7, never-iced Narrhok 6, Ginselle 6, Ruj'kah 5, iced Gorgrim 4 + freed 2 (Tocxal died 3:30), iced Maz'dinah 1. 10 dead by 2:06, when the third healer died; 5 of the 8 CC bots dead by 1:50 |
| toc2 kill | 6 | Ruj'kah 3, Narrhok not yet iced 1, Birana 1, iced Gorgrim 1 |
| toc3 wipe | 29 | Ruj'kah 7, freed Broln 6 (Toczai died 2:12.9), never-iced Narrhok 4 and Gorgrim 4, Harkzog 3, Ginselle 3, Maz'dinah iced 1 + freed 1 (Toczam died 2:48.1). 5 dead by 2:05, 24 after |

**Verdict on round 6 (kite re-hop): held.** `--personal` re-hops refused by the bot's own kite walk 0 of 214, 244, 188
hops (window 13: 86, 93, 97; 30, 50, 30 over 1 s); `faction champions kite` `wait` move rows 3, 1, 1 (410); hops per kite
1.77, 1.89, 1.94 (2.05, 1.78, 1.80). Melee-only catches (a swing or weapon special within 2 s), same lineups: toc1 27%
(20%), toc2 19% (30%). Deaths melee champions did half of: 19/31, 2/6, 17/29 (15/29, 3/3, 0/4).

**Worse than window 13? No.** toc1, same lineup and roster: wipe 29 at 4:38 → 31 at 3:55, raid DPS 70k → 87k, the three
healers dead by 2:06 (2:57), first death at 0:59 (0:05, 6 by 0:45). toc2, same lineup: kill 3 → 6, 6.4 min both. toc3's
loss is the lineup draw above. The shared fix reaches this fight only through `toc.personal` (a 4 s focus hit): picks
per pull alike (toc1 37 against 33, the same spells); `churn.toc.personal` per minute rose (13.8 → 19.2, 2.3 → 6.0,
3.9 → 17.9) as a fading defensive stopped reading as cover. Healthstones "over 70%" (4/16, 3/6, 5/16) are the reader's
pre-hit sample: Tocxak, Tocxap, Tocxaw and Tocxat each used theirs right after a 5-12k hit took them under 70%.

**Round 7 baseline** (not built): iced Broln's disarm-gated casts 36 (toc1) and 27 (toc3), no disarm while iced.

**Failure ranking (66 deaths).**

1. Casters 28: Ruj'kah 15 (5, 3, 7), Ginselle 9, Harkzog 3, Birana 1.
2. Free melee champions 24: never iced 15 (Narrhok 11, Gorgrim 4), freed by their ice DK's death 9 (Broln 6, Gorgrim
   2, Maz'dinah 1).
3. Iced melee champions 14: Broln 7 (round 7's), Gorgrim 5 (Death Coil, Icy Touch), Maz'dinah 2.

**Picked: Ruj'kah (Hunter), the top killer on every pool (plan, Q3).**

- 9 of his 15 kills came while he was the kill target, fourth after the three healers, in the collapse: on it 108.6 s
  (100 → 18%) on toc1 and 82.8 s (100 → 36%) on toc3, against 45.3 s on the toc2 kill. On toc3 48% of his damage went on
  healers.
- Off the latch his CC is DR-bound: Polymorph and Fear each 10 + 5 s, then ~15 s with neither landing over 2.5 s; no
  Cyclone on roster C. Free 53, 54, 71 s before the latch took him. Duties came within ~1 s of each DR reset (toc3:
  Polymorph DR clear ~1:16.8, duty 1:17.7, after Toczah died at 1:15.2), so no bot fault in the chain. In a 1-3 s
  handover he kills cloth in two shots: toc2 Tocyad 86% → dead at 1:14.5 (Steady Shot 11.6k, Shoot 7.3k), Polymorph
  cast 1:12.9, landed 1:14.0, the shots already out.
- The script stops him whole on a disarm: `npc_toc_hunter::myCanCast` is false under `UNIT_FLAG2_DISARM_RANGED`, checked
  before every event and his Shoot (`boss_faction_champions.cpp:1242-1262`, `:1333-1337`). Disarm 676 and Dismantle 51722
  carry aura 278 (round 7), with their own DR and no trinket. The peel aims at melee champions only, so a disarm on him is a
  new tactic.

**Fixed too (bug, independent and small): the totem duty went to the Enhancement's own ice DK.**

- `--adds` (new `ice` cell): toc1 34 holds, 64.0 s, 32 of them Tocxam's (DK tank, Broln's ice DK); toc3 2, 35.1 s, by
  Toczai; window 13 toc1 7, 31.3 s. The long ones: Tocxam's Tremor 1:15.4 (19.3 s), Windfury 1:46.1 (19.3 s) and 3:24.5
  (18.2 s), Toczai's Tremor 0:19.1 (33.6 s), window 13 Tocxam's Tremor 2:47.5 (24.4 s); those totems lived 23-35 s,
  every other holder's 0-5 s. toc1 1:15-1:35: Tocxam 13.2 yd off the totem throughout, target the totem 41 samples and
  Broln 36, no move row. 0:45-0:52: 28 holds of 0.0-0.7 s on one Windfury.
- `CanKillTotem` (`ToCHelpers_FactionChampionsDefence.cpp:963-968` before the fix) takes the nearest non-healer bot in
  control within 30 yd; Broln drops totems at his feet, next to his ice DK, which hits only its own champion: `faction
  champions ice target` (`ACTION_RAID + 2`) and `faction champions kill totem` (`+ 2.7`) each fire while the other's
  target is current (`ToCTriggers_FactionChampionsBall.cpp:29`, `ToCTriggers_FactionChampionsDefence.cpp:35`), so its
  target flips and nothing lower, `reach melee` included, gets a turn. No champion had Tocxam as victim at 1:15, so the
  kite hold wasn't what held it.
- Cost not measured in deaths: a live Tremor clears our fears on champions within 30 yd (5 such Fears held 0.2-2.4 s
  against medians 2.2-3.5 s elsewhere), Windfury hastes the melee champions.

**Fix** (`ToCHelpers_FactionChampionsDefence.cpp`). `CanKillTotem` wants no ice duty (`FactionChampionsIceDutyOf`), as
the snare duty and physical switch already do; a held duty is released on the next refresh. Reader `--adds`: an `ice`
cell per `fc.totem` hold and the count and seconds held by a bot on an ice duty.

**Next window should show.** `--adds` "held by a bot on an ice duty: 0" wherever Broln is in (this window 34 / 64.0 s and
2 / 35.1 s), his Tremor and Windfury Totems gone within ~5 s (23-35 s here), ice uptime unchanged (95/94/99%, 100/95%).
Round 7 on the same window: `--peel` "Disarm or Dismantle while iced" above 0 on Broln, his gated casts while iced mostly
"within 10 s after one" (36 and 27 "otherwise" here).

**Seen, not picked.**

- Externals go early: toc1 Pain Suppression and two Hands of Sacrifice on Tocxai (Enhancement) at 0:03-0:25; toc3 six
  by 0:17, four on melee bots. The healers' later focuses found none (toc1 Tocxac at 0:56.7, 7 champions, dead 3 s
  later). Melee at any health is the user's call.
- Breakable CC on the casters ends early (Ginselle's Polymorphs 0.1-4.7 s), burning DR. No `dmg` row reaches a champion,
  so the breaker is unknown; raid casts on a champion within 8 yd peak at the breaks, single-target ones too.
- Free Narrhok's white swings hit 12-15k from 11-12 yd (Tocxas 0:51.3 and 0:59.5); his toc1 gaps between CC ran 8-15 s.

**Questions for the user.**

- Q3 (new, plan): may a rogue's Dismantle, or a Defensive Stance warrior's Disarm, go on the Hunter champion whenever one
  is in melee reach and he isn't disarmed? It stops all his shots and abilities for 10 s (then 5, 2.5 by DR) without his
  trinket or a DR shared with Polymorph or Fear. Recommended: yes, the kill target first, where 9 of his 15 kills came
  and the rogues already stand; off the latch only from a bot already in reach.
- Q2 (plan, due): never-iced champions killed 15 of 66, the top melee cause and the first killer in both wipes (Tocxas
  0:59.5, Toczav 1:10.2). Three DKs always leave the Warrior free, two the DK and the Warrior. Recommendation unchanged:
  the DK ahead of the Rogue for one window, which changes only the two-DK rosters.

**Shared code.** None; the fix is in this fight's files and its reader.

**Review.** The case holds: `--adds` on windows 11-15 (old builds) gives every Broln totem hold over 12 s to an ice duty
DK, and every wall grip overlapping a totem hold is an ice DK's too. At each long hold's start a non-healer bot off ice
duty stood within 30 yd (toc1 1:46.1 only Tocxag, 19.8 yd, and Tocxau, 26.9), so the duty keeps a taker. No size, mode
or player branch: ice duties go to bot DKs on every difficulty, and a player in the raid leaves both duties to the bots
as before.

- Review fixes: the counts and the mechanism above (32 of 34 holds Tocxam's, 28 short ones, other holders 0-5 s; the
  two triggers' target flip, not the holds); the boss doc's totem lifetimes (23-35 s, where 18-34 was the hold length)
  and the flip.
- Left: the hold churns on one holder every 0.1-0.7 s and comes back within 25 ms (toc1 0:45-0:52, Tocxam steady 6.5
  yd off, so not the 30 yd edge); cause not traced, harmless while the same bot retakes it. A chased melee holder can't
  reach its totem either, the kite hold zeroing `reach melee`: window 12 toc2 Tocyag (Rogue), Maz'dinah's victim 16-17
  yd off, held a Windfury 11.0 and 8.0 s. Not seen in windows 13-15; watch `--adds` for long holds by bots off ice duty.

**User answers (2026-11-23) to round 8's questions.** Q3: yes. Q2: no, the ice order stays as it is.

## Round 9 (2026-11-23, window 15a on build c350347ff; builds Q3)

**Data.** Window 15a, `raidtest.py report --boss faction-champions --raid toc --head c350347ff --pulls 10`, round 7 built,
round 8 not: toc1 `tr-20261123-112654-4` kill 6:54, 3 deaths (`649_4_caiphus-the-stern_1795433344`), toc2
`tr-20261123-112654-5` kill 6:54, 2 (`649_5_thrakgar_1795433339`), toc3 `tr-20261123-112654-6` wipe 7:20, 30
(`649_6_thrakgar_1795433335`). Window 14 (round 8's traces) reread for the Hunter baseline. Scripts in the round's
scratchpad `fc9/` (gone with it); the reader's new `--peel` Hunter line covers the key one.

**Stop rules.** Not polished: toc3 wiped. Ranking skipped: the round builds Q3 as answered, and window 15 is pulling round
8.

**Lineups.** toc1 window 14 toc1's (Broln, Narrhok, Maz'dinah, Gorgrim; three DKs ice Maz'dinah, Gorgrim, Broln; Narrhok
from 3:50). toc2 and toc3: Malithas for Broln, two DKs on Malithas and Maz'dinah; toc2 iced Gorgrim from 3:24 and Narrhok
from 3:58, toc3 Gorgrim from 4:30 and Narrhok never.

**Deaths by killing blow.**

| Pull | Deaths | By |
|---|---|---|
| toc1 kill | 3 | iced Broln 2 (melee 1:32, Lava Lash 5:20), Ruj'kah 1 |
| toc2 kill | 2 | Narrhok (free) 1, Ruj'kah 1 |
| toc3 wipe | 30 | Narrhok (never iced) 8, Gorgrim 8 (6 before his ice duty), Malithas 4 (3 after his ice DK Toczai died at 5:04), Harkzog 4, Ruj'kah 2, Maz'dinah 2, Ginselle 1, Zhaagrym 1 |

**Verdict on round 7 (iced Tauren disarm): works where it lands, too rare to matter.** `--peel`: iced Broln (toc1) took 2
(first 1:05.9), his disarm-gated casts 0 within 10 s after one, 56 otherwise (window 14: 36 and 27, window 13: 48), and
still killed 2 of the 3. Iced Narrhok 1 (toc1) and 2 (toc2), all from 5:20, 0 after, 8 otherwise each. The two rogues'
Dismantles (60 s cooldown) came to 10, 9 and 8 a pull, 7, 5 and 4 of them on Narrhok.

**Q3 checked before building.**

- Both Hunter champions, Ruj'kah 34448 and Alyssia Moonstalker 34467 (difficulty entries 35724-35726, 35662-35664), run
  `npc_toc_hunter` with `CreatureImmunitiesId` -12, -13 on heroic (charm; heroic adds taunt and Attack Me), and
  `CREATURE_FLAG_EXTRA_ALL_DIMINISH`, so both are taken.
- `npc_toc_hunterAI::myCanCast` (`boss_faction_champions.cpp:1242-1245`) is false under `UNIT_FLAG2_DISARM_RANGED`,
  checked before every event (`:1261`) and gating the Shoot after it (`:1333`); he never melees. Disarm 676 and Dismantle
  51722 both carry aura 278; the cast check wants a weapon in a disarmed slot (`SpellInfo.cpp:1763-1783`), he has main
  hand 47156 and ranged 48711.
- Deterrence 65871 (5 s, under 25%, every 90 s): parry and deflect +100%, pacify. Pacify stops his casts but not the
  triggered Shoot (`TRIGGERED_FULL_MASK` ignores caster auras), and `Unit::MeleeSpellHitResult` parries a disarm from his
  front only.

**Baseline** (`--peel` Hunter line, then the round's script). Window 15a: disarms on Ruj'kah 0, 1 (the prot tank's class
Disarm, 2:08.5, kill target), 0; his casts 115, 83, 97 and damage 463k, 364k, 419k with none within 10 s of one (toc2: 1
cast, 3k). Kill target 45.6, 43.0, 52.9 s, a rogue within 5.5 yd of him 86%, 86%, 90% of it; off the latch a rogue or
warrior that close 29%, 14%, 13%. Killing blows 1, 1, 2, as kill target 0, 1, 1. Window 14: disarms 0, 1, 0; casts 122,
73, 116; kill target 108.6, 45.3, 82.8 s, a rogue that close 29%, 92%, 18%; killing blows 5, 2, 7, as kill target 3, 0,
5.

**Fix** (`ToCHelpers_FactionChampionsDefence.cpp`). The peel takes the Hunter champion, not CC'd, guarded or ranged-disarmed
(`DisarmedFor` reads his ranged flag), whatever his victim, and gives him only `ReadyDisarm`: a rogue's Dismantle or a
Defensive Stance warrior's Disarm, in melee. While his Deterrence is up, only a peeler behind him. DR, immunity, LOS and
guard checks as before. Kill target or not it's one rule: the peel never walks, so on the latch the rogues already standing
on him take it and off it only a bot already in reach.

Ordering against the iced Tauren: each refresh serves the entries in rank order, each taking the nearest untaken peeler
with a ready spell, and the Hunter ranks after every melee champion, free or iced. A rogue in reach of both spends that
refresh's Dismantle on the Tauren. Nothing reserves a cooldown for a need that comes later.

Reader `--peel`: per Hunter champion the raid's Disarms and Dismantles, those on the kill target, and his casts and damage
while one held or not (`hunter_disarms`; DR-cut spans since the review below).

**Next window should show.** `--peel` Hunter line: disarms above 0 on every pool, mostly on the kill target (window 15a 0,
1, 0); his casts and damage "while one held" near 0 against "otherwise" (window 15a 0 casts and 0k on all three, review
reader); fewer Ruj'kah kills as kill target with a rogue alive and in reach (window 14: 3 of the 8, window 15a 2 of 2).
Narrhok's and the iced Tauren's disarms not far below this window's (Narrhok 7, 5, 4; iced Broln 2), since the Hunter's
Dismantle spends the same 60 s cooldown. A small gain at most: about one Dismantle per rogue in a 45-55 s kill-target
hold.

**Seen, not picked.** The prot warrior tank's class Disarm lands on casters with nothing to gate, one each: Harkzog and
Thrakgar here, Birana, Harkzog, Liandra and Thrakgar in window 14.

**Questions for the user.** None.

**Shared code.** None; the change is in this fight's helper and its reader.

**Review.** The fix holds as built: entries are the base ones on every size and difficulty (both Hunters' difficulty rows
carry the same immunities and `ALL_DIMINISH`, no `NO_PARRY`), peelers are bots as before, and a player's disarm counts
through the ranged flag. The Deterrence check matches `MeleeSpellHitResult`: parry from the front only, its deflect only on
ranged attacks. The `IsPhysicalChampion` rewrite is the same set.

- Root cause narrower than stated. By killing blow window 14 gave Ruj'kah 14, 8 as kill target (round 8's 15 and 9 count
  the top damage dealer). 5 of those 8 came with both rogues dead: toc1 3:10.1 (Tocxat died 1:24.8, Tocxaf 2:56.3),
  toc3 2:48.1, 2:49.7, 2:55.9 and 3:15.4 (Toczat 2:25.0, Toczag 2:42.4), neither roster bringing a prot warrior. Only 3
  had a rogue within 5.5 yd (4.3, 5.4, 4.5 yd). A rogue that close for 29%, 92%, 18% of his kill-target time there; the
  86-90% is window 15a's, where both his kill-target kills had both rogues within 5 yd.
- Review fixes: the reader's "within 10 s after one" would count casts past a DR-cut disarm, which the fix chains (10, 5,
  2.5 s), and both baseline leaks were artefacts: the casts came 9.92 s (window 15a toc2) and 9.99 s (window 14 toc2,
  two) after the disarm's cast record, its aura running out up to a map update early, and the 3k was a Shoot fired
  0.1 s before the Disarm. `hunter_disarms` now holds each disarm for its DR-cut span (`disarm_windows`: 10 s halved per
  step, none once immune, reset 15 s after the last ran out) less its last 250 ms, and books damage at the cast that
  fired it; both landed disarms read 0 casts and 0k while held. The boss doc's Peel evidence, which paired window 14's
  kills with window 15a's rogue share, is corrected.
- Left: `iced_disarms` keeps the flat 10 s window; one of window 15a's five iced disarms was DR-cut (toc1 Narrhok 5:21.5,
  a free Dismantle 13.4 s before), with no gated cast in its 10 s either way. A cast record is not a landing: a dodged or
  missed disarm still reads as held, the trace carrying no aura on a champion. The prot tank can spend its Disarm on the
  Hunter and lack it for a Bladestorm within 60 s, which Q3's answer covers.

## Round 10 (2026-11-23, windows 15 and 16 on builds 523b2f60f and 2d7853f0e)

**Data.** Window 16 (`2d7853f0e`, rounds 8 and 9 in it): toc1 `tr-20261123-133438-4` kill 6:56, 3 deaths
(`649_4_thrakgar_1795441009`), toc2 `tr-20261123-133438-5` kill 7:00, 4 (`649_5_erin-misthoof_1795441016`), toc3
`tr-20261123-133438-6` wipe 5:01, 29 (`649_6_caiphus-the-stern_1795441012`). Window 15 (`523b2f60f`, round 8 in it):
toc1 `tr-20261123-121740-4` kill 6:29, 0 (`649_4_erin-misthoof_1795436364`), toc2 `tr-20261123-121740-5` kill 7:51, 5
(`649_5_erin-misthoof_1795436373`), toc3 `tr-20261123-121741-6` wipe 6:56, 29 (`649_6_thrakgar_1795436387`). For the toc3
comparison, every pull of windows 10-16 (24, traces in the rounds above). Scripts in the round's scratchpad `fc10/`
(gone with it); the reader's new `--adds` elemental lines cover the fixed one.

**Stop rules.** toc3 wiped in both windows: not polished. The failure picked is new. (toc3 did kill in window 13,
4 deaths; its other windows since 11 wiped.)

**Lineups.** toc1 both windows: Caiphus, Thrakgar, Erin; Maz'dinah, Narrhok, Ruj'kah, Gorgrim, Malithas, Harkzog,
Ginselle. toc2: window 15 Liandra, Caiphus, Erin with Broln for Malithas; window 16 Liandra, Thrakgar, Erin with
Vivienne for Malithas. toc3 both windows: Liandra, Caiphus, Thrakgar; Maz'dinah, Narrhok, Ruj'kah, Gorgrim, Harkzog,
Ginselle, Birana; two DKs on Maz'dinah and Gorgrim, Narrhok never iced.

**Deaths by killing blow.** Iced: its duty's Chains in the 10 s before; freed: its ice DK dead.

| Pull | Deaths | By |
|---|---|---|
| w15 toc1 kill | 0 | - |
| w15 toc2 kill | 5 | Gorgrim before his ice duty, iced Maz'dinah, iced Broln, Ruj'kah, Harkzog 1 each |
| w15 toc3 wipe | 29 | never-iced Narrhok 6, freed Gorgrim 6 (Toczam died 4:45) + iced 2, Birana 4, Ruj'kah 2 + Cat 2, Harkzog 3, Ginselle 2, iced Maz'dinah 1 + freed 1 |
| w16 toc1 kill | 3 | iced Gorgrim 2, Ginselle 1 |
| w16 toc2 kill | 4 | Ginselle 2, Ruj'kah 1, iced Gorgrim 1 |
| w16 toc3 wipe | 29 | Ruj'kah 7, Ginselle 6, Harkzog 5, Birana 4, never-iced Narrhok 3, iced Gorgrim 2 + freed 1, iced Maz'dinah 1. Healers dead at 1:55, 1:58, 2:16 |

**Verdict on round 9 (Hunter disarm): works where it lands; on toc3 nobody was left to land it.** `--peel` Hunter
line, window 16: disarms on Ruj'kah 3, 3, 2 (2, 2, 0 on the kill target; window 15: 0, 0, 0), his casts and damage while
one held 0 and 0k on all three (68, 75, 112 casts otherwise). His kill-target kills: 0, 0, 4, the four on toc3 at
2:23-3:25 with both rogues dead (Toczat 1:17.9, Toczag 1:59.5; latch from 2:21.5), roster C bringing no prot warrior.
Window 14: 5 of 8 likewise.

**Verdict on round 8 (no totem duty for an ice DK): held.** Window 15 toc2, the one pull with Broln: `--adds` held by a
bot on an ice duty 0 (window 15a toc1, round 7's build: 5 holds, 54.7 s); 13 of 14 totems gone in 0.0-3.5 s, the last,
a Windfury at 6:26, lived 30 s unheld near the kill (7:14); his ice DK's uptime 100%, longest gap 0.2 s. Window 16 had
no Broln.

**What makes toc3 different** (windows 10-16). Without Broln, toc1 killed 4 of 4 and toc2 4 of 4, toc3 2 of 6 on the
same three lineups. Ranked:

1. No ranged druid (roster C: cat and resto). The Balance bot's Cyclone duty, Cyclone stand and Entangling Roots peel
   don't exist there, and all its CC breaks on damage. Champion healers off the latch were CC'd 10-22% of the first 120 s
   on toc3, 19-35% on toc1 and toc2. On Narrhok, the free Warrior on every roster, the Balance bot alone put 15-17
   Cyclones and 12-20 Roots a pull (windows 14-16 toc1, toc2); toc3's control casts on him 0-17 against 8-55. Comp or
   tactic: Q4.
2. Two DKs leave Narrhok never iced, as on roster B, whose Balance bot covers him (above). The ice order stays (user,
   2026-11-23).
3. No prot warrior: the Hunter's disarms rest on two rogues (round 9 verdict), and Narrhok gets only their Dismantles
   (roster B's tank added 0-2 Disarms a pull). Q5.
4. The raid's own Greater Fire Elemental, every roster, worst where CC is all breakable. Picked below.

Damage taken in the first 120 s: toc3 wipes 11.6-15.0k/s, kills on the other pools mostly 9.5-12.0k/s; casters did 22 of
toc3's 29 window 16 deaths.

**Picked: the Greater Fire Elemental sits on a champion off the kill target and breaks the CC on it (bug).**

- `npc_pet_shaman_fire_elemental::UpdateAI` (core `pet_shaman.cpp:107-117`) attacks the shaman's selected unit once,
  then keeps its victim till it dies: 2 minutes. Window 16 toc3: Toczaj (Enhancement) dropped the totem at 0:10.761
  while hitting Liandra (his Flametongue on her at 0:11.0); the elemental took Thrakgar, a healer off the latch, and held him
  0:10.8-2:10.5, 73 s of it off the latch: 5 Polymorphs held a median 1.3 s, 5 Fears 1.8 s.
- 24 pulls: elementals targeted the kill target in 14% (Enhancement's) and 22% (Elemental's) of their samples, another
  champion 61-71%, a pet 14-15%. Polymorph begun on their target held a median 0.7 s (49; elsewhere 2.1 s over 771), Fear
  1.8 s (51; 3.8 over 697). A healer off the latch in 4 pulls (toc3 windows 10, 14, 16, toc1 window 11), else mostly a
  caster the CC duty wanted (toc1 window 16: both on Ginselle 85 and 120 s, Polymorphs a median 0.2 s).
- The CC duty's `ClearForBreakable` (`ToCHelpers_FactionChampions.cpp:803`) sees members and `GetPet` only, so it keeps
  casting into it. Its Fire Shield (5 yd, 3 s) and Fire Nova (10 yd) didn't measurably break Polymorph nearby (1 of 9 and
  1 of 15 held at a pulse inside reach ended there).
- Cost of losing it: 14-22% of its time on the kill target; the Elemental shaman's lived 1-4 s in 16 of 24 pulls.

**Fix** (`ToCMultipliers_FactionChampions.cpp`). `fire elemental totem` (both class nodes' action name) joins `OWN_SPLASH`,
so `faction champions suppress aoe multiplier` zeroes it while the fight is live. Reader `--adds`: each raid Greater
Fire Elemental, its kill-target share, the champion it targeted most off it, and the Polymorphs and Fears begun on its
target (`raid_fire_elementals`). Boss doc: AoE row, Known gaps (no ranged druid).

**Review fix.** The multiplier can't stop a player's totem, so the CC duty's `ClearForBreakable` and the Peel's
`ClearOfOurHits` now check every member's `m_Controlled` victim (pets and guardians, CC bots' too) where they checked
only `GetPet`. Other raid guardians off the kill target, windows 10-16: Spirit Wolf on a champion 10% of its samples
(Polymorph begun on its target 13, held a median 1.6 s; Fear 12, 2.2 s), Shadowfiend 21%, Mirror Image 5% (8
Polymorphs, 0.8 s); the DK ghoul and Gargoyle sit on their owner's iced champion (84%, 82%), which takes no CC anyway.

**Next window should show.** `--vetoes`: `faction champions suppress aoe multiplier` rows on `fire elemental totem`.
`--adds`: "raid Greater Fire Elementals in the pull: 0" on every pool (window 16: 2, 2, 2). `--cc` on a toc3-like
lineup: Polymorph and Fear held on the healers that weren't kill target first above window 16's (Caiphus 12.8 s,
Thrakgar 20.0 s). A small gain at most: one champion's CC for up to 2 min a pull.

**Seen, not picked.**

- Externals spent before the healers were focused, as in earlier rounds: window 16 toc3 Pain Suppression on Toczax
  (Warrior) at 0:01.4, Guardian Spirit, three Hands of Sacrifice and a Hand of Protection on Toczaj (Enhancement) by
  0:53; none ready when Toczae and Toczad died at 1:55-1:58.
- No Mass Dispel on window 16 toc3 (0 casts): Liandra's Divine Shield ran 12.1 s, first kill at 61.4 s (toc1, toc2:
  34.2, 34.9). Also a full 12 s on window 10 toc1 and toc2, 11 toc2, 12 toc1; 0.2-1.1 s in the other eight.
- Ginselle's Arcane Explosion: 494k and 501k in the toc3 wipes, 368k and 380k in toc2 kills, 6.5-8.9 victims a cast.

**Questions for the user.**

- Q4 (new, plan): roster C has no ranged druid. (a) In this fight, a cat druid bot takes the Cyclone duty and the
  Entangling Roots peel when the raid has no ranged druid bot, leaving Cat Form for each cast; (b) swap toc3's cat for a
  Balance druid in `dc_roster_toc3.conf` (the bear tank keeps Improved Leader of the Pack, as on roster B); (c) leave
  it. Recommended: (a), which also covers a real raid with no Balance druid, at about 3 s of the cat's casting per 27 s
  Cyclone cycle.
- Q5 (new, plan): with no rogue alive and no prot warrior, a DPS warrior bot switches to Defensive Stance to Disarm
  the Hunter while he's the kill target. Recommended: yes, kill target only, after Q4's window if one change at a time
  is wanted: 9 of his 15 kill-target kills in windows 14-16 came with both rogues dead.

**Shared code.** None; the change is in this fight's multiplier, its CC and Peel clear checks, and its reader.

## Round 11 (2026-11-23, window 18 on build c01caf9cc)

**Data.** `raidtest.py report --boss faction-champions --raid toc --head c01caf9cc --pulls 10`, round 10 in it: toc1
`tr-20261123-153900-4` kill 7:54 (DC 8:30), 10 deaths (`649_4_erin-misthoof_1795448470`), toc2 `tr-20261123-153900-5` kill
6:21, 2 (`649_5_thrakgar_1795448442`), toc3 `tr-20261123-153900-6` kill 6:12, 3 (`649_6_thrakgar_1795448450`). Windows 15
and 16 toc1 (same lineup) reread for the comparison, windows 14-18 (15 pulls) for the CC breaks and Chains lapses.
Scripts in the round's scratchpad `fc11/` (gone with it); the reader's new `--cc` line covers the key one.

**Stop rules.** Kills on all three pools, toc2 and toc3 within 3 deaths, toc1 at 10: not polished, one pool short.
toc3's first kill since window 13 came on a new lineup (Liandra, Thrakgar, Erin; Vivienne for Birana, no Caiphus).

**Lineups.** toc1 window 16 toc1's: Caiphus, Thrakgar, Erin; Maz'dinah, Narrhok, Ruj'kah, Gorgrim, Malithas, Harkzog,
Ginselle; three DKs ice Maz'dinah, Gorgrim and Malithas, Narrhok from 4:25. toc2 and toc3: Liandra, Thrakgar, Erin;
Maz'dinah, Narrhok, Ruj'kah, Gorgrim, Harkzog, Vivienne, Ginselle.

**Deaths by top damage dealer** (its share in the 10 s before). Iced: its duty's Chains in the 10 s before; lapsed: older,
its DK alive; second life: the same bot again after Reincarnation or Rebirth.

| Pull | Deaths | By |
|---|---|---|
| toc1 kill | 10 | iced Gorgrim 2 (Tocxak 1:25, Tocxap 2:41), iced Malithas 1 (Tocxac 3:52), iced Narrhok 1 (Tocxac's second life 4:46, one 19.8k swing from 100%), lapsed Maz'dinah 1 (Tocxas 0:49), lapsed Malithas 1 (Tocxag 5:13), Ruj'kah 2 (Tocxai 0:25 under a 9-champion focus from 0:03, Narrhok 31% of it; Tocxap's second life 3:29), Ginselle 1 (Tocxai's second life 1:37), free Narrhok 1 (Tocxaq 4:10) |
| toc2 kill | 2 | Ruj'kah 2 (Tocyaq 1:49.9 with Vivienne 32% and Ginselle 31%, Tocyap 1:58.5 with Ginselle 33%) |
| toc3 kill | 3 | Harkzog 2 (Toczas 0:44, Toczad 1:39), Ruj'kah 1 as kill target (Toczal 2:47) |

**Verdict on round 10 (no raid fire elemental, no breakable CC into a raid summon's target): held.**

- `--adds`: raid Greater Fire Elementals 0, 0, 0 (window 16: 2, 2, 2); `--vetoes`: `fire elemental totem` 27, 69, 69.
- Polymorph, Fear and Hex cast at a champion a raid summon had as target: 0, 0, 2 (window 16: 14, 6, 10), both on toc3
  with the caster's own summon (Toczav's Felguard on Vivienne, Toczaq's Mirror Images on Narrhok).
- Polymorph off the kill target held a median 2.2, 2.5, 1.9 s (windows 15-16, six pulls: 1.6 s), Fear 3.7, 4.4, 3.6 s
  (3.7 s).
- Healers off the latch, our Polymorph, Fear, Cyclone and Banish held over their time off it: Thrakgar 35%, 51%, 17%, Erin 67%, 60%,
  63% (window 16: Thrakgar 47%, 37%, toc3 23%; Erin 57%, 59%; Caiphus on toc3 25%). toc3's Thrakgar got 8.3 s in 46.5 s
  (window 16's toc3: 20.0 s in 83.4 s), on a different lineup; no clear gain there.

**Why toc1 took 10 deaths and 8:30.** Same lineup and roster as windows 15-16 (0 and 3 deaths, 5:53 and 6:20), and the
champions dealt the same rate (6.4k/s against 5.9k/s and 6.5k/s); the pull ran long and the deaths came with it.

1. The opening: 9 champions on Tocxai (Enhancement) 0:03-0:25, a Hand of Sacrifice and Lay on Hands spent on him (windows
   15-16: 4 champions on the cat for 8 s). Their threat pick weighs distance, health and armour (`RecalculateThreat`
   every ~9 s), so this is the draw.
2. Two of the three Fear warlocks dead by 1:25: Tocxas to Maz'dinah in her DK's only Chains gap (below), Tocxak from full
   after Lay on Hands in 2.5 s to iced Gorgrim's swing and Frost Strike and Malithas' Crusader Strike. They cast 7 Fears
   (windows 15-16: 21 and 15); CC landed 2.26 per 10 s (2.69, 2.60).
3. Every later kill hold ran 10-27 s longer: Maz'dinah 48 s (35, 36), Gorgrim 46 (29, 36), Narrhok 55 (34, 37), Malithas
   61 (34, 34). The extra 1.5-2 min with three iced melee champions alive gave them 4 kills from 2:41, and two of the
   pull's Chains lapses came there.
4. 3 of the 10 were second lives, standing up unbuffed: Tocxap from 97.9% in 0.2 s, Tocxac from 100% in one swing.

**Failure ranking (15 deaths).**

1. Caster champions 8: Ruj'kah 5, Harkzog 2, Ginselle 1. Our breakable CC on them breaks early, much of it to our own
   splash (picked below).
2. Iced melee champions 4, all toc1: Gorgrim 2 (Death Coil, Frost Strike), Malithas 1, Narrhok 1.
3. Chains lapses after an ice DK's wall walk 2, toc1 (picked too).
4. Free Narrhok 1.

**Picked: our own splash breaks our CC (bug).**

- The AoE rule suppresses the raid's own splash, but `faction champions suppress aoe multiplier`
  (`ToCMultipliers_FactionChampions.cpp:21-32` before the fix) took only area spells, two summon lists and named ones.
  Starfall (53201, a self aura whose stars pick enemies within 30 yd), Searing Totem (`TotemAI` takes the nearest
  attackable unit) and the shared `MULTI_TARGET_ACTIONS` (Bladestorm, Sweeping Strikes, Cleave, Blade Flurry, Swipe (cat),
  Multi-Shot) went out: `--aoe` raid splash on window 18 Cleave 1143, 1309, 808, Multi-Shot 49, 41, 58, Swipe (Cat) 40, 0,
  7, Sweeping Strikes 9, 10, 11, Starfall 8, 7, 0, Bladestorm 6, 4, 4, Blade Flurry 4, 4, 4, Searing Totem 1, 6, 7. None of
  their spells carries `SPELL_ATTR6_DO_NOT_CHAIN_TO_CROWD_CONTROLLED_TARGETS` (DBC), so all hit a held champion.
- Raid cast rows naming a champion our Polymorph or Fear held, windows 14-18, those ending it within 0.7 s in brackets:
  Starfall stars 111 (87); warrior Deep Wounds, Trauma and Blood Frenzy 79 (50), of which 47 inside the warrior's
  Bladestorm and 24 after its Cleave; Searing Totem attacks 51 (21); Blade Flurry 31 (26); Sweeping Strikes 29 (27).
  A lower bound: most hits write no row.
- toc2 1:46.8-1:52.8: Tocyax's Bladestorm on Erin (kill target, 2 yd) broke Ginselle's Polymorph as it began (3.2 yd),
  Vivienne's Fear at 1:48.98 and the Polymorph and Fear recast on her at 1:49.8 and 1:49.97 (5.5 yd); Ruj'kah's Polymorph
  ended at 1:49.8, 11 yd off. Those three dealt 99% of Tocyaq's damage (dead 1:49.95) and 91% of Tocyap's (1:58.5), the
  pull's only deaths. toc1 3:29: Ruj'kah's Fear broke 1.4 s in with Tocxax cleaving 3.8 yd off, and he killed Tocxap 2.8 s
  later.

**Picked too: an ice DK's wall walk outruns its own Chains (bug, small, independent).**

- An ice duty DK takes the wall duty on its own champion while its Chains has over 4 s left (`OwnChainsHold`,
  `WallVictim`, `ToCHelpers_FactionChampionsDefence.cpp:1403-1429` before the fix) and walks up to 6 s to a stand 22-27 yd
  off the champion, past Chains range. At 4 s left the duty releases before the grip, the champion has run on at its
  victim, and the ice reach walks back after the Chains ran out.
- toc1 0:40.5-0:51.0: Tocxah took Maz'dinah's wall duty with 6.2 s of Chains left, released at 0:42.8 short of the stand,
  walked back 8 s and recast at 0:50.97; Maz'dinah, unchained from 0:46.8, killed Tocxas at 0:49.8. 5:02.9-5:14: Tocxam
  took Malithas' at 8.9 s left, re-elected three stands as Malithas ran, released at 5:07.75, his Chains out at 5:11.8;
  Malithas killed Tocxag at 5:13.8.
- Windows 14-18: 64 Chains lapses (181 s) followed such a walk with no grip, 2 of them with a kill (both above); 4
  followed one with a grip (21 s) and 24 had no walk (140 s), none with a kill.

**Fix** (`ToCMultipliers_FactionChampions.cpp`, `ToCHelpers_FactionChampionsDefence.cpp`). The suppress multiplier also
zeroes the shared multi-target actions (`RaidSplashCache::Facts::multiTarget`, buffs included) and lists `searing totem`
and `starfall`. An ice DK's own-champion wall duty is elected and kept only while its Chains has over 4 s plus the walk to
the stand plus 1 s left (`WallWalkFits`, `OwnChainsHold`'s new spare). Reader: `--cc` counts the raid's cast rows naming a
champion our Polymorph, Fear or Hex held, by spell, with those it ended within 0.7 s (`splash_hits`); `--aoe`'s raid splash
adds Starfall, Sweeping Strikes, Blade Flurry, Swipe (Cat) and Searing Totem. Boss doc: AoE and Wall grip rows, Known gaps,
`--cc`. Every size and difficulty; a player's own casts and grips untouched.

**Next window should show.** `--vetoes`: `faction champions suppress aoe multiplier` rows on `starfall`, `bladestorm`,
`cleave`, `sweeping strikes`, `blade flurry`, `swipe (cat)`, `multi-shot`, `searing totem`. `--aoe` raid splash near 0
for all of them (above). `--cc` raid hits: Starfall, Searing Totem's Attack, Blade Flurry, Sweeping Strikes and the
warrior procs near 0 (window 18: Starfall 12, 6, 0; Attack 0, 13, 7; Deep Wounds, Trauma, Blood Frenzy 6, 7, 17).
Polymorph off the kill target above a median 2.2, 2.5, 1.9 s, Fear above 3.7, 4.4, 3.6 s; fewer caster kills (8 of 15).
`--ice` on a three-DK pool: longest Chains gaps under toc1's 4.2, 7.3, 11.8, 10.1 s, uptime no lower (97, 95, 95, 93%),
grips no far below 6, 7, 9, 1. Kill holds no longer than toc2's and toc3's 29-44 s (the splash lost lands mostly off
the kill target).

**Seen, not picked.**

- Our CC also breaks on the bear tank's Maul (its glyph's second target: Infected Wounds on a held champion 37 times, 34
  ending it, windows 14-18), Mirror Images on their mage's Polymorph target (19, 12), and a Hammer of Justice on a held
  champion (25, stun over our CC).
- Iced Gorgrim kills at range again (Death Coil, Icy Touch; 23 of his 41 kites caught on toc1). No kite answers it; only
  a silence would, a plan matter not asked yet.
- Second lives die fast: unbuffed after Reincarnation or Rebirth (Tocxac's max health under 19.8k).
- toc3: Harkzog under no CC 1:22.6-1:38.4 while he killed Toczad.

**Questions for the user.** None new. Q4 (cat druid Cyclone duty without a ranged druid) and Q5 (a DPS warrior disarms
the Hunter with no rogue alive and no prot warrior) still wait; not built.

**Shared code.** None changed: the multiplier reads the shared `MULTI_TARGET_ACTIONS` list through `RaidSplashCache`, and
the change is in this fight's multiplier, its wall duty and its reader.

**Review.** Both causes hold: windows 14-18's `--cc` raid hits sum to the figures above (Starfall 111 (87), Searing
Totem 51 (21), Blade Flurry 31 (26)), and window 18 toc1's two lapses read as written (Tocxah's holds 0:38.0-0:42.8,
Tocxam's 5:02.9-5:07.7, each released at 4.0 s of Chains, then 6-7 s of ice reach that never got back in range). No
size, mode or player branch.

- Review fix: the stand search took the stand farthest from the victim within 6 s and only then tested the Chains fit,
  so an ice DK whose farthest stand didn't fit took no wall duty though a nearer one did; the search now caps the walk
  at what the Chains leave (`WallWalkBudget`).
- Review fix: Call of the Elements dropped the bar's Magma Totem past the multiplier, 20 in windows 14-18, all from
  Enhancement bots (`AiFactory` gives them `magma`). The multiplier zeroes it while the fire slot holds Magma or Searing
  Totem, and `--aoe`'s raid splash counts the totems it drops (window 18: Magma Totem 0, 2, 0); next window 0.

## Round 12 (2026-11-23, window 20 on build 5d9e7320d)

**Data.** `raidtest.py report --boss faction-champions --raid toc --head 5d9e7320d --pulls 10`, round 11 in it: toc1
`tr-20261123-174440-1` kill 6:21 (DC 6:57), 2 deaths (`649_1_thrakgar_1795455979`), toc2 `tr-20261123-174440-2` kill
6:32 (7:09), 5 (`649_2_erin-misthoof_1795455977`), toc3 `tr-20261123-174440-3` kill 5:52 (6:27), 0
(`649_3_erin-misthoof_1795455980`). Window 18 (round 10's three) reread for the comparison, windows 14-20 (18 pulls) for
the kite starts and circle teleports. Scripts in the round's scratchpad `fc12/` (gone with it); the reader's new
`--personal` line covers the key one.

**Stop rules.** Kills on all three pools, toc1 and toc3 within 3 deaths, toc2 at 5: not polished, one pool short again
(window 18: toc1 at 10).

**Lineups.** toc1 window 18 toc2's (Liandra, Thrakgar, Erin; Maz'dinah, Narrhok, Ruj'kah, Gorgrim, Harkzog, Vivienne,
Ginselle), three DKs ice Gorgrim, Narrhok and Maz'dinah. toc2 and toc3: Liandra, Caiphus, Erin; Maz'dinah, Narrhok,
Ruj'kah, Broln, Gorgrim, Harkzog, Ginselle; two DKs ice Broln and Maz'dinah, then Gorgrim and Narrhok as the kill order
frees them (toc2 Narrhok from 4:33, toc3 from 4:09). Broln's first lineup since window 15.

**Deaths by top damage dealer** (its share in the 10 s before). Iced: its duty's Chains in the 10 s before.

| Pull | Deaths | By |
|---|---|---|
| toc2 kill | 5 | iced Broln 3: Tocyah (mage) 2:27.7, 100%, Broln walking at Tocyae passed her 3-5 yd off as she stood casting, the recount at ~2:26.8 made her his pick, Stormstrike 10.9k + 10.5k from 5.1 yd and a 5.0k swing, full to dead in 0.5 s; Tocyap (warlock) 2:51.1, 59% (Narrhok not yet iced 38%), Broln took him 11-12 yd off mid-Chaos Bolt (a 9.0k swing from 12.2 yd), Shadowfury, Narrhok's Mortal Strike, then his Demonic Circle: Teleport landed 9 yd from Broln, Lava Lash 13.0k and a 10.2k swing; Tocyao (shadow priest) 5:48.0, 81%, Broln on him from 5:37, kites started 13.0-13.6 yd off hit by Stormstrike from 12.5 yd and swings from 11.7-11.8, Dazed twice by Zhaagrym, iced Narrhok's Charge. Ruj'kah (kill target) 1: Tocyad (disc priest) 2:30.4, 100%, standing in his Frost Trap. Narrhok before his ice duty 1: Tocyal (warlock) 4:07.8, 74%, kiting, Dazed and Charge Stun |
| toc1 kill | 2 | iced Narrhok 2: Tocxak (warlock) 1:14.4, 76% (Ruj'kah's Shoot the blow), Narrhok on him from 0:36 with three Charges (0:35.5, 0:50.6, 1:00.7), then 1:02.2-1:06.2 with no kite hop open at the wall (every turn off the floor or within 10 yd of a healer) and a 17.8k swing, Dazed at 1:09.5 halving his kite; Tocxas (warlock) 1:49.5, 54% (iced Gorgrim 42%), Mortal Strike from 13.3 yd, swing from 11.3, Charge Stun and Gorgrim's Chains of Ice on him |
| toc3 kill | 0 | - |

**Verdict on round 11 (own splash suppressed, wall walk fits the Chains): held, and cost no kill time.**

- `--vetoes` `faction champions suppress aoe multiplier`: starfall 36, 34, - (roster C has no Balance bot), bladestorm 51,
  63, 47, cleave 31, 51, 29, sweeping strikes 32, 42, 33, blade flurry 73, 75, 63, swipe (cat) 15, -, 14 (roster B has no
  cat), multi-shot 62, 51, 52, searing totem 37, 34, 35, call of the elements 7, 9, 7, magma totem 33, 31, 32.
- `--aoe` raid splash: none on all three (window 18: Cleave 1143, 1309, 808, Multi-Shot 49, 41, 58 and the rest; Magma
  Totem 0, 2, 0).
- `--cc` raid hits on our held CC: Starfall, Searing Totem's Attack, Blade Flurry, Sweeping Strikes, Deep Wounds, Trauma
  and Blood Frenzy 0 on all three (window 18: Starfall 12, 6, 0; Attack 0, 13, 7; the warrior procs 6, 7, 17). Left:
  Infected Wounds 0, 3, 10 (0, 3, 8 ending it), Mirror Image bolts 0, 0, 5 (5), interrupts and dispels.
- Off the kill target, Polymorph held a median 5.0, 9.7, 5.0 s (window 18: 2.2, 2.5, 1.9), Fear 5.1, 6.0, 5.0 s (3.7,
  4.4, 3.6), Cyclone 5.1, 6.0, - (5.7, 5.8, -).
- Caster kills by top damage 1 of 7 (window 18: 8 of 15), Ruj'kah as kill target.
- `--ice` on the three-DK pool (toc1): longest gaps 16.3, 8.2, 9.0 s (window 18: 4.2, 7.3, 11.8, 10.1), the first two the
  opening's first Chains, the third mid-fight with no wall hold before it; uptime 95, 98, 97% (97, 95, 95, 93); grips 7,
  8, 7 (6, 7, 9, 1). No gap over 5 s followed a wall walk on any pool, and no death to a lapsed champion (window 18: 2).
- Kill time: toc1 ran window 18 toc2's and toc3's lineup in 6:20.6 (6:20.6, 6:12.2), kill holds Maz'dinah 41.0 s (36.6,
  36.8), Gorgrim 34.2 (29.2, 33.7), Narrhok 28.3 (32.4, 33.0), Vivienne 41.0 (43.8, 37.7), the healers 36.4-42.9
  (34.6-43.2). toc3's 5:52 is the log's fastest kill. No measurable cost from losing Cleave and Bladestorm.

**Failure ranking (7 deaths).**

1. Iced Tauren 5: Broln 3 (toc2), Narrhok 2 (toc1).
2. Narrhok before his ice duty 1, Ruj'kah as kill target 1.

**Picked: the kite starts inside the Tauren's strike reach (bug).**

- The champions' weapon specials are melee-range spells (range index 2, 5 yd). `Spell::CheckRange` tests them at
  `IsWithinMeleeRange(target, 5 − MIN_MELEE_REACH)` with the leeway, `5 − 2 × MIN_MELEE_REACH` without
  (`Spell.cpp:7396-7407`), so while both move they reach 3 yd past the swing reach: 10.7 yd, the Tauren two 14.6. Mortal
  Strike (`me->GetVictim()`) gets all of it; Overpower and Sunder wait for `GetDistance2d < 5` and Stormstrike and Lava
  Lash for `SelectTarget(MaxThreat, 0, 5.0f)`, 12.6 yd, which picks any raider in it (`boss_faction_champions.cpp:2082-2106`).
  Hits on moving bots, eight pulls: Mortal Strike p90 14.0 yd (max 14.9), Overpower 12.3 (14.9), Stormstrike 12.3 (12.6),
  Lava Lash 11.6 (13.0), white swings 12.0.
- The kite counts a crawling chaser only within its swing reach + 2.25 yd (13.8 for the Tauren;
  `ToCHelpers_FactionChampionsBall.cpp:360-367` before the fix), and the cast break measures the gap against the swing
  reach (`:531-537`), so a kite from an iced Tauren starts inside his weapon specials.
- Windows 14-20, iced chasers' kites by start, a weapon hit within 2 s (the reader's new line): Tauren inside their strike
  reach 106 of 328 (32%), past it 5 of 81 (6%); the other melee champions 164 of 664 (25%) and 38 of 435 (9%). Window 20:
  Broln inside 10 of 31 and 6 of 27, past 1 of 9 and 0 of 11; Narrhok (toc1) inside 16 of 31, past 0 of 12.
- toc2 5:37.6-5:38.1: Tocyao broke Mind Blast and kited at 13.6 yd; Dazed (3.5 yd/s) against Broln's 3.6-4.4 yd/s, Broln's
  Stormstrike landed 0.5 s later from 12.5 yd. toc1 1:46.5: Narrhok's Mortal Strike on kiting Tocxas from 13.3 yd.
- Not all of it: 191 of the 256 iced Tauren kites that began inside 13.8 yd began with the Tauren picking a bot already that
  close (72 hit within 2 s), which no gate moves (Q6).

**Fixed too (bug, small, independent): a Demonic Circle teleport lands next to another melee champion.** The escape's
circle had to be 12+ yd farther from the mean of the chasers in swing reach only (`ToCHelpers_FactionChampionsBall.cpp:970-972`
before the fix), where Blink also wants 12+ yd off every melee champion. Windows 14-20: 14 of 82 teleports landed within a
melee champion's swing reach + 2.25 yd, 106k of melee damage in the next 4 s and Tocyap's death (Broln, also on him, 14.5
yd off when he teleported and 9.0 from the circle).

**Fix** (`ToCHelpers_FactionChampions.{h,cpp}`, `ToCHelpers_FactionChampionsBall.cpp`). `FactionChampionsStrikeReach` =
swing reach + `NOMINAL_MELEE_RANGE` − `MIN_MELEE_REACH` (10.7, 14.6). The kite counts a crawler within it + 2.25 yd (13.0,
the Tauren 16.8, the Ret's 17 floor kept), and the cast break measures the gap against it. The circle teleport also wants
12+ yd off every melee champion (`LANDING_MIN_CLEAR`, Blink's 12). Reader `--personal`: per melee champion its iced kites
started inside its strike reach or past it, and those a white swing or weapon special hit within 2 s (`iced_kite_starts`).
Boss doc: reach paragraph, Kite and Escapes rows, two Known gaps, `--personal`. Every size and difficulty; a player's own
moves untouched.

**Next window should show.** `--personal` "iced kites started inside the chaser's strike reach or past it": Broln and
Narrhok far more past than inside (window 20: Broln inside 31 and 27, past 9 and 11; Narrhok toc1 inside 31, past 12) and
their weapon hits inside down from 10, 6 and 16. `fc.castbreak` notes up some (16, 32, 15), kites per pull up a little
(272, 282, 304 hops). No Demonic Circle: Teleport row in the escape table with its nearest melee champion under 12 yd at
+2 s from a champion not on the bot. Fewer deaths to an iced Tauren (window 20: 5 of 7). Kill time no longer (6:21,
6:32, 5:52).

**Seen, not picked.**

- Iced Broln put 46-57% of his damage on raiders other than his victim (window 20 toc2 561k of 1206k, toc3 442k of 772k,
  window 15 toc2 489k of 992k), Stormstrike and Lava Lash on his top threat within 12.6 yd: the ranged he passes and his
  own ice DK (134k, 145k, 156k), which hits him by the user's call. Q6.
- A kite finds no hop at the wall when every turn leaves the floor or lands within 10 yd of another healer; the bot then
  stands (Tocxak 1:02-1:06 above). 2% of chased samples over windows 14-20, 207k of melee damage, 2 deaths within 5 s.
- Dazed from a hit in the back halves a kiter's speed for 4 s, and an iced Tauren at 3.6-5.2 yd/s late in its Chains then
  keeps up (Tocxak, Tocyao). Iced Narrhok charged Tocxak three times in 25 s.
- toc3's ice DK on Maz'dinah cast its first Chains at 0:30.0 (toc1 Gorgrim 0:16.3, Narrhok 0:08.2): the opening's Chains
  come late. toc3 lost nobody.
- toc3 landed CC at 1.62 per 10 s (2.31, 2.42) and still killed with 0 deaths.

**Questions for the user.**

- Q6 (new, plan): healers step off a melee champion within its reach; ranged stay put. Should a ranged bot, not casting,
  step 10 yd off a Tauren champion (Broln, Narrhok) that isn't on it once it comes within his strike reach, as healers do?
  His Stormstrike and Lava Lash take whoever tops his threat within 12.6 yd and every recount takes a near one: 191 of 256
  close iced Tauren kites in windows 14-20 began with him picking a bot already within 13.8 yd, and iced Broln put 46-57%
  of his damage off his victim. Recommended: yes, the Tauren two only, one window as a trial, judged on their deaths and the
  raid's kill time.
- Q4 and Q5 still wait; not built.

**Shared code.** None: the strike reach is this fight's helper, the reach facts went into its boss doc, and the reader is
its own.
