# Polish log: Flame Leviathan (25-man, four towers)

Reader `tools/botobs/bosses/flame_leviathan.py` (`--summary` is `--mechanics`; also `--riders`,
`--hulls`, `--pyrite`, `--vents`, `--ram`, `--fury`, `--inferno`, `--kite`, `--adds`, `--corners`),
tests in `tools/botobs/tests/test_botobs.py` (`FlameLeviathanReader`). Design doc
`docs/raids/ulduar/flame-leviathan.md`. Older rounds: `history/flame-leviathan.md`.

## State

**Setup.** Trace slug `flame-leviathan`, raid `uld`, DC row `uld-leviathan`: the raid boards beside
the vehicle line, DC stages the fleet inside the arena and pulls. Hard mode is all four towers; the
trace shows it as his `mhp` 268.9M at 25-man, 88.4M at 10-man. 25-man: 5 each of siege engine,
demolisher, chopper; 10-man: 2 each.

**Fight model.** Everything happens from vehicles; a crewed bot is immune to his AoE, so deaths
are bots on foot after their hull died (10n: Lash from the Freya adds kills most of them). Hulls die
to Missile Barrage (no answer), Flame Vents when the reserve engine misses, Battering Ram on the
Pursued hull, and Mimiron's Inferno on kiting hulls (median 113-127k a patch tick, more under Ram's
all-school debuff, against a blast's 34-48k). Boss damage is Blue Pyrite from the demolishers plus
Systems Shutdown (kill his turrets from his seats: 20 s stun at +50% damage taken). Outside the stun
his loss rate tracks the fleet's pyrite stacks, so losing a hull early is what loses a 10-man pull:
kills held 8-19 fleet stacks from 0:30 to 3:00 and 0.31-0.37%/s outside the stun, wipes dropped to
1.5-8 stacks by 1:00-1:30 and 0.12-0.14%/s. Gathering Speed makes him faster all fight; a long fight loses the
fleet. Riders turn (seat 3044 `ALLOW_TURNING`) and reach every activated turret from any seat.

**Compositions on c9b718c29 (window51, r2's code).**

| Design | 25n (uld pools) | 10n (uld pools) |
|---|---|---|
| A | kill 6:09 0 deaths (uld1), wipe 8:06 27 (uld4) | wipes 6:20 11 (uld1), 6:11 10 (uld4) |
| B | kills 4:53 2 (uld2), 7:20 4 (uld5) | wipe 5:53 11 (uld2), kill 4:12 4 (uld5) |
| C | kills 6:22 0 (uld3), 5:58 2 (uld6) | kills 3:42 0 (uld3), 3:59 2 (uld6) |

25n meets the done rule on this build (four-tower kills on A, B, C with ≤5 deaths); 10n passes B
and C, A fails. Trend (25n median deaths): 6.5, 28.5, 16.5, 2 over the last four builds.

**Open failures, ranked.**

1. Kiting hulls burn in the Inferno trail (r3 fix): 16-34% of 10n hull damage, 22% in the 25n
   wipe, 93-100% of it on kiting hulls; the old gate skipped the fire detour for 20 s after any Ram
   blast. Early siege and demolisher losses (1:20-1:48) start every 10n wipe.
2. Kite legs aimed past him: the first node is the one nearest the hull, so a hull mid-arena can
   drive across him (15 legs in 12 pulls within 60° of his bearing passing inside 30 yd, 10 closing
   to 0.2-15 yd within 6 s, `--kite`). It began uld1 10n's demolisher loss at 1:07. Fix idea: start
   at the nearest node whose leg does not pass within the 50 yd clearance closer than the hull is
   now, else today's pick.
3. A Pursued demolisher whose gunner is riding has no Increased Speed; at 10-man that is one of two
   demolishers, and both A 10n wipes lost the rider's demolisher at 1:45-1:48. If it persists after
   1 and 2, the question is whether the 10-man rider should come from elsewhere (a user answer
   fixes riders as demolisher gunners).
4. Throw misses: up to 5 of 8 throws a pull (uld6 10n), 8 of 22 in uld1 25n.

**Fixes in place.**

- r1: rider cap 2/1, riders ranged DPS but never hunters, Blink/Frost Nova/Disengage/charges
  blocked on his seats, a loaded gunner unloads to its gun at shutdown. Verdict r3: holds (hunters
  seated 0).
- r2: a bot's turn on a turning seat goes into its seat offset (shared `ServerFacade::SetFacingTo`);
  riders attack the nearest activated turret; stranded jump only with its side done. Verdict r3:
  works on A, B, C at 10 and 25 (turned 96-100% while he moves, flank/behind 3.0-13.6k/s, 25n stun
  every 46-91 s, 25n passes), but the stranded rule misfired on every fresh landing (r3 fix).
- r3: the kite detours round fire even under Ram (margin 0 inside his Ram cast range, 8 yd outside);
  a rider never leaves stranded while its Hookshot Aura 62336 lasts. Verdict: next window.

**Ideas rejected.**

- r1's heading limits (turret within ±90° of his heading, a front rider held to the front pair, the
  15 s stall jump, the facing-based stranded jump), r2: riders can turn; the limit was a bot gap.
- Q1 (aim throws from behind him so riders land on rear seats), r2: with turning every seat reaches
  every activated turret, so only the landing side matters.
- Skipping the kite's fire detour while Ram's 20 s debuff is up (from 2026-09-27), r3: the trail
  hits 3x a blast and the debuff amplifies it; hulls burned with him 72 yd away.

**User answers in force (2026-11-25).** Riders kill all of his turrets, not only their own seat's
pair; riders are ranged DPS, hunters excluded if the turrets sit inside a hunter's minimum range
(they do, r1); 2 riders at 25 and 1 at 10, the other demolisher gunners stay and shoot down the
Pyrite carriers; the throw stays capped at 70 yd; a gunner still in the catapult when the stun
starts gets back on its gun. Riders can turn: a Death Knight thrown onto a front seat faced the rear
turrets and killed all four alone.

**Open questions.** None.

## Round 3 (build c9b718c29, window51)

**Data.** `window51-flame-leviathan-25n-job1.md`, `window51-flame-leviathan-10n-job5.md` (6 pulls
each, r2's code). Traces 25n `603_{1..6}_…179561725x/6x`, 10n `603_25_…1795619248` (uld6),
`603_26_…1795619329` (uld1), `603_27_…1795619363` (uld3), `603_28_…1795619369` (uld2),
`603_29_…1795619414` (uld4), `603_30_…1795619413` (uld5), through `--hulls`, `--inferno`,
`--pyrite`, `--vents`, `--riders`, `--mechanics`.

**Deaths, all on foot.** 10n (41): Lash 19, Vents 7, Ram 4, Inferno 3, Fury 3, Barrage 2; uld1 A
11 (Lash 6 from 4:33), uld4 A 10 (Lash 8 from 1:51), uld2 B 11 (Ram 3 at 4:20, Lash 3). 25n (35):
27 in uld4 A (Vents 5 at 5:29, then Lash 7, Fury 4, Ram 3), the rest ≤4 a pull.

**Previous fix (r2), by mechanic.** Riders turned onto a turret behind them while he moved: 25n A
97/96%, B 96/98%, C 97/97%; 10n 100% everywhere (window48 0-38%). Rider damage flank 3.5-5.8k/s,
behind 3.0-13.6k/s against ahead 4.4-5.7k/s (window48 62-325/s). 25n: 5-6 stuns a pull every
46-91 s, fights 4:53-8:06 (window48 7:18-19:00). Works on all three designs. Side effect found: a
rider leaves `stranded` 0.2 s after nearly every hookshot landing and is hooked back, 3-8 times a
landing until the aura's 5 s run out (`--summary` lists; 11-47 such jumps a pull); 25n saw 3-4
riders at once against the cap of 2.

**Failure picked: kiting hulls burn in the Inferno trail.**

- Early hull losses decide 10n: kills kept both demolishers to 3:20+, every wipe lost a siege
  engine or demolisher at 1:20-1:58, and fleet pyrite dropped to 1.5-8 stacks by 1:00-1:30 against
  the kills' 8-19 (`--pyrite`).
- Those losses are Inferno on kiting hulls: Inferno is 16-34% of 10n hull damage (top cause in 4 of
  6), 22% in the 25n wipe, and 93-100% of it lands on kiting hulls (73% in 25n uld1).
- Root cause: `Kite()` (`UldActions_FlameLeviathan.cpp`, old line 1183) skipped `DetourAroundFire`
  while `HasAura(SPELL_FL_BATTERING_RAM)` or he was in Ram cast range. 62376 lasts 20 s (DurationIndex
  18) and refreshes per blast, so the detour stayed off long after he fell out of range. New
  `--inferno` split of kite Inferno damage, in range / out of range with the debuff / neither:
  10n uld1 52/48/0, uld4 4/59/37, uld2 96/4/0, uld6 80/21/0, uld3 86/14/0, uld5 55/39/5; 25n uld2
  17/67/16, uld3 0/100/0, uld4 69/28/3, uld5 0/100/0.
- Example uld4 10n siege `12884903366`: blasts at 0:27 and 0:29, then at 0:46-0:51 it drove
  straight through the trail with him 72-81 yd away: 193,914 ×6 and 375,000, 92.6% → 12.8% in 4 s;
  dead at 1:20.
- Costs: Inferno on hulls median 113-127k, p90 187-198k; Ram median 34-48k. Ram's effect 3 is aura
  87 misc 127 (all schools), so a debuffed hull burns faster. The gate dates from a pull where the
  trail cost 0.7% of fleet health and full-margin detours took 62% of Ram hull damage.

**Second: the stranded rule misreads a fresh landing.** Hookshot 62323 effect 2 is MOD_FACTION 1770
on the caster (`HandleAuraModFaction` also drops `UNIT_FLAG_PLAYER_CONTROLLED`); FactionTemplate
1770 (faction 959, hostile mask 0) and his turret's 1965 (faction 965, our mask 8) are neutral to
each other, so `IsValidAttackTarget` fails the CvC check and `FlameLeviathanRiderTurret` finds
nothing. `spell_hookshot_aura` removes 62323 and 62336 only on a 500 ms tick that finds the rider
seated (`boss_flame_leviathan.cpp` 1683-1694); the bot reacts first, jumps `stranded`, and the next
tick hooks it back. Uld1 10n Uldxag: landed 0:11.371, jumped 0.2 s later and kept jumping until
62336 expired at 0:15.871, then sat and cast at the turret at 0:16.4.

**Classification.** Both bugs: the strategy intends a kite that keeps the hull alive and a rider
that shoots its turret.

**Fix.** `UldActions_FlameLeviathan.cpp` `Kite()`: always offers the fire detour; inside his Ram
cast range with margin 0 (only fire the leg enters), else the 8 yd margin. `DetourAroundFire` takes
the margin (header default 8, the station detour unchanged). `UldEncounter_FlameLeviathan.cpp`
`FlameLeviathanRiderStranded`: false while the bot has `SPELL_FL_HOOKSHOT_AURA`. Both run the same
with a real player in the raid (a human rider is never the leaver; the kite is per driver). No
shared code. Reader: `--inferno` kite-gate block, new `--kite` (legs aimed past him), the summary's
jump line counts `stranded with the hookshot still on`. Doc updated.

**Next window should show.** `--inferno`: "out of range, Ram debuff on" near 0% of kite Inferno
damage, and Inferno's share of hull damage well under 16-34% (10n) / 22% (25n wipe); Ram's share not
above its 13-27% (10n). `--summary`: `stranded with the hookshot still on 0`, a landing followed by
one seated stay, riders at once ≤ cap, earlier first turret damage, more stuns a minute. Outcome: no
siege or demolisher lost before 2:00 in most 10n pulls, an A 10n kill with ≤5 deaths, 25n still
passing. If kiting hulls still burn in range, check whether a non-closing detour existed
(`DetourAroundFire` returns none when every fan point closes on him).
