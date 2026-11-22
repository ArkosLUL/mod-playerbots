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
