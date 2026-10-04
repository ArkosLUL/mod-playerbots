# Freya - why bots died in the `1791059738` kill

## Context

A Freya 25-man hard-mode pull that **killed the boss** still lost ten raiders. This review separates
the deaths the strategy accepts by design from the ones that are defects, and records how each
mechanic was handled. Three defects came out of it; none is implemented.

Trace: `env/dist/logs/botobs/603_7_elder-stonebark_1791059738.ndjson` (2026-10-03 23:42, build
0.2h after `a14a69517`). 25-man normal, `AC_AI_PLAYERBOT_ULDUAR_FREYA_HARD_MODE=1`, all three Elders
alive, 2 humans (`Deathsong` melee, `Dragon` **tank**, so a human held a role the strategy plays).
Kill at **7:03.058**; 10 death records over 9 players (`Angry` died twice), 19 of 25 standing at the
end. Paths are relative to `modules/mod-playerbots/`.

Method: `tools/botobs/postmortem.py` for deaths, coverage and vetoes, plus direct NDJSON queries for
per-mechanic counts. Every number below is from this one trace, so treat single-pull figures as
indicative and the cross-pull comparisons in [freya.md](../../raids/ulduar/freya.md) as the baseline.

## The pull was healthy

Against the reference pulls in [freya.md](../../raids/ulduar/freya.md) (the `1788613108` kill and the
`1788716763` / `1788717562` wipes), every avoidable mechanic came in lower:

| per minute | this pull | doc kill | doc wipes |
|---|---|---|---|
| all damage taken | **729k** | 899k | 948k / 1163k |
| Sunbeam 62872 | **75k** | 137k | 137k / 271k |
| Ground Tremor 62859 | **188k** | 243k | 224k / 222k |
| Nature's Fury 63570 | **35k** | 105k | 56k / 157k |

Nearest-neighbour among living raiders: median **1.31 yd**, which is the doc's kill profile (1.3),
not the wipe (0.6). Detonate ran **179 hits in 19 blasts, median 9 victims, max 21** - inside the
documented 6.7-10.4 band.

## Mechanic-by-mechanic

| mechanic | handling | evidence |
|---|---|---|
| Trio wave 1 | **textbook** | Snaplasher / Storm Lasher / Water Spirit dead 1:41.98 / 1:42.73 / 1:42.51, spread **0.75s** |
| Trio wave 2 | acceptable | 3:12.74 / 3:11.80 / 3:05.49, spread **7.25s**, Water Spirit first |
| Ancient Conservator | both cleared | 0:10→0:54.98 (44.9s), 4:00→4:54.24 (53.5s) |
| Eonar's Gift | **9 of 9 killed, none bloomed** | each dead in 4.0-8.3s; the doc's failure case is 14.5s with three blooms |
| Detonating Lashers | as designed | 2 waves of 10 at 1:58 and 3:28, cleared in 28.4s and 26.5s, all to 0-4% |
| Nature Bomb | **excellent** | 54 bombs in 7 volleys → only **17 hits on 8 players**; escape latency median **0.65s**, p90 1.85s, against a ~6s fuse; 4 of 94 bot-volleys over 4s |
| Unstable Sun Beam | best acceptance | 291 move orders, **260 accepted (89%)** |
| Sunbeam step-out | ~3.0 victims a cast | 23 casts, 70 hits (doc kill reference 4.5/beam); 86 orders, 58 accepted (67%) |
| Spores / Nature's Fury | works, noisy | 41 spores, 3740 Potent Pheromones, Fury at a third of the reference kill. But `freya move to healing spore` issued **979 orders for 262 accepted (26%)**, the worst churn of any node |
| Ground Tremor | damage unavoidable | 13 casts at 2000 ms; 50000 yd radius, "the control" per the doc. See defect 3 |
| Iron Roots | fast median, bad tail | 27 traps in 9 volleys of 3, ~49.5s apart; most released under 1s, tail `Nightwarrior` **25.3s**, `Obliteration` 5.2s, `Mighty` 3.7s (died), `Deathsong` 2.9s (died) |

Both humans took the most avoidable damage in the raid: Nature's Fury `Deathsong` x10 and `Dragon`
x9, the highest of 23 players, and Unstable Energy `Deathsong` x11, joint highest. The bots handled
those two mechanics better than the humans did.

## The ten deaths

- **4 are the accepted trade.** Melee inside the pile: `Angry` 2:25, `Angry` 3:52, `Shadow` 3:52,
  `Totemist` 3:53, all Detonate, 5-7 lashers within 15 yd. [freya.md](../../raids/ulduar/freya.md)
  records that spreading past the blast was tried (`45d9a3e36`) and cost far more than it saved.
- **1 is a human.** `Deathsong` 6:13, rooted at 6:10.28 and standing in an Unstable Energy pool at
  3.9 of 5.0 yd. No bot decision involved.
- **1 is a tail case.** `Smartface` 5:07: escape ordered 5:06.070 for a volley planted 5:00.828, a
  5.24s latency leaving 0.9s to walk. One of the 4-in-94 outliers above.
- **4 are the two defects below.**

## Defect 1 - the sunbeam step-out routes into Detonate range, and converges on one point

**3 deaths: `Hellflame`, `Prayer`, `Malediction`, inside 0.2s of each other at 3:50.**

A Sunbeam cast at 3:48.453 landed at 3:49.966, during the lasher-wave-2 finish, with the pile around
(2343, -50) at 3-9% hp. Eleven bots ran `freya step out of sunbeam`. Six routed west and were clear
at 17.5-26.1 yd. Five were aimed at **one point**, (2346.2, -37.5):

```
3:48.500 Smartface     dest (2346.2,-38.0)  nearest live lasher  9.0 yd @ 9% hp
3:48.556 Malediction   dest (2346.1,-37.0)  nearest live lasher 10.0 yd @ 9% hp
3:48.657 Nightwarrior  dest (2346.2,-37.5)  nearest live lasher  9.5 yd @ 5% hp
3:48.676 Prayer        dest (2346.2,-37.4)  nearest live lasher  9.6 yd @ 5% hp
3:49.286 Hellflame     dest (2346.2,-37.7)  nearest live lasher  9.3 yd @ 5% hp
```

Pairwise spread of those five destinations: **0.04 to 1.04 yd**. Three of the five died in the blasts
at 3:49.8-3:50.3.

Two causes:

1. **The escape hazard set holds no lashers.** `GetFreyaEscapeHazards`
   (`src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp:588`) returns `BuildFreyaEscapeHazards(bombs,
   beams)` (`:574`) - Nature Bombs and Sun Beams only. The node that does know a lasher is about to
   detonate, `freya lasher about to blow`, sits at `ACTION_RAID + 3` (63), **below** the sunbeam
   step-out and the bomb escape at `ACTION_RAID + 4` (64), so once either fires that knowledge is
   never consulted. `freya ranged camp` wanted these bots west at (2331, -41) in the same tick, but
   it sits at `ACTION_RAID` (60) and lost.
2. **No deconfliction between bots.** `FindNearestPositionClearOfHazards`
   (`src/Util/EncounterHelpers.cpp`) sweeps rings at fixed distance and angle steps outward from the
   bot, so bots standing together with the same hazard set converge on the same answer.
   `freya ranged camp` already carries the lesson - it stops `ULDUAR_FREYA_RANGED_CAMP_SPACING`
   (6 yd) short on the arrival bearing precisely because aiming every bot at the point itself put
   them a median 0.6 yd apart. Neither escape got the same treatment.

**Fix.** Put the *low* lashers into `BuildFreyaEscapeHazards` at the 15 yd Detonate radius - the same
set `freya lasher about to blow` already selects by `ULDUAR_FREYA_LASHER_BAIL_PCT` (15%) - for ranged
and healers only. West was 17.5-26.1 yd clear and six bots found it unaided; a lasher-aware sweep
sends all eleven there.

**Doctrine guard.** Spacing alone cannot answer a 15 yd blast - the doc computes that twelve bots
would need a 31 yd ball, past `AiPlayerbot.SpellDistance` - so the hazard set is the fix and
deconfliction is secondary at best. Scope it to **low lashers only**, and only to bots already
stepping out. Do not touch melee, do not add the healthy pack, and do not zero any mover:
`ReachTargetAction` is the only generic path a bot has to close on a hostile target, which is what
made the `45d9a3e36` lattice halve melee DPS.

## Defect 2 - a rooted bot's escape latch reports success forever

**1 death: `Mighty` 5:24.886, from 100% health to dead in 160 ms.**

```
5:21.092  move  ok=1 by='freya move away nature bomb' pr=forced -> (2343.32,-68.79)
5:21.194  aura  ON Iron Roots 62438 dur=-1        <- rooted 102 ms into the walk
5:21.298  act   rel 65.0 FAILED  freya break iron roots   <- root creature not spawned yet
5:22.220  Strengthened Iron Roots creature spawns
5:22.343  act   rel 65.0 OK      freya break iron roots   <- Attack(root) succeeds, once
5:22.553  act   rel 65.0 FAILED  freya break iron roots
          2.33s with no verdict change and no move order at all
5:24.86   4x Nature Bomb within 2.4 yd -> dead
```

`act` records are written only when that action's verdict changes, so the silence means
`freya break iron roots` stayed FAILED for Mighty's last 2.33s.

Two mechanisms:

1. **`Attack` returns false once the bot is already attacking.** `AttackAction::Attack`
   (`src/Ai/Base/Actions/AttackAction.cpp:165`) returns false on `sameTarget && inCombat &&
   sameAttackMode` ("I am already attacking %target"), and core `Unit::Attack` does the same
   (`src/server/game/Entities/Unit/Unit.cpp:7432-7453`). `FreyaBreakIronRootsAction::Execute` is
   `return Attack(root)` (`src/Ai/Raid/Uld/Action/UldActions_Freya.cpp:660`), so it reports FAILED
   every tick after the first and the engine falls through. Its 65 against the escape's 64 therefore
   wins for exactly one tick.
2. **The bomb escape latch does not test for root.**
   `src/Ai/Raid/Uld/Action/UldActions_Freya.cpp:160` returns true on `latched && stillClear(bombSpot)
   && !bot->IsMovementPreventedByCasting() && dist > CONTACT_DISTANCE`. A rooted bot satisfies all
   four, so the latch claims the tick - deliberately, per its own comment, so combat movement cannot
   pull the bot back onto the bomb - while the bot cannot move at all.

The root was being burned the whole time (100% to roughly 37% by the moment Mighty died); it simply
lost the race to a 6s fuse with four bombs inside 2.4 yd.

This is the same class as the pitfall [freya.md](../../raids/ulduar/freya.md) already records,
*"a dodge that never moves reports success"*. Both latches were taught to break on
`IsMovementPreventedByCasting()`, but not on being rooted or stunned.

**Fix.** Two independent changes:

- Break the bomb-escape and sunbeam-step-out latches on root and stun as well as casting, so a pinned
  bot falls through instead of claiming the tick.
- Have `FreyaBreakIronRootsAction::Execute` return true while the root is alive and already targeted,
  rather than propagating `Attack`'s already-attacking false. The bot is doing the right thing;
  reporting failure hands the tick to an escape that cannot run.

## Defect 3 - `freya ground tremor hold cast` has never once succeeded

Not a death, a throughput loss. Coverage reports `LOST - fired 164x, pushed 164x, ran 0x`. There are
27 verdict rows for it, **all FAILED at rel 92**, spread across all 13 Ground Tremor casts. So the
node does run, and bails every time.

It is **not** a regression from the perf pass (`db77e1184`): the trigger fires, so the
`GetFreyaBossByEntry` lookup resolves. The bail is below it, in `!tremor || !own` or the cast-time
compare (`src/Ai/Raid/Uld/Action/UldActions_Freya.cpp:1044-1062`).

Prime suspect: `FreyaGroundTremorHoldCastTrigger::IsActive` gates on
`bot->HasUnitState(UNIT_STATE_CASTING)`, but the action reads
`bot->GetCurrentSpell(CURRENT_GENERIC_SPELL)`. A **channelled** cast - Volley, Mind Sear, Hurricane,
Drain Life - lives in `CURRENT_CHANNELED_SPELL`, so `own` is null and the action returns false. The
doc's own measurement is that 31 of 37 casts in flight get cut and healers pay the 10s school lockout
hardest, so this is worth closing.

**Fix.** Read `CURRENT_CHANNELED_SPELL` as well. Confirm from a trace which spells were actually in
flight at each tremor cast before touching the cast-time compare, since a channel's
`GetCastTimeRemaining` is not the same quantity as a cast's.

## Checked, not worth acting on

- **`freya move to healing spore` at 26% move acceptance** (979 orders, 262 accepted). The worst
  churn of any node, but Nature's Fury landed at a third of the reference kill's rate, so it is
  working. Cost, not correctness.
- **Trio wave 2's 7.25s spread.** Nothing died from it, and wave 1 was 0.75s.
- **The humans' avoidable damage.** Not the strategy's to fix.
- **Ground Tremor's 188k/min.** 50000 yd radius with no falloff; there is nothing to dodge.

## Verifying a fix

1. `~/.claude/scripts/pb-syntax-check.sh` on every changed `.cpp` plus the `.cpp` that include a
   changed `.h`, then `tools/pblint/pblint.py` on the same paths.
2. One Freya hard-mode pull, then re-run what produced this review:
   - `postmortem.py <trace>` - the deaths, grouped as above.
   - `postmortem.py <trace> --coverage freya` - `freya ground tremor hold cast` must leave `LOST`.
   - no `freya step out of sunbeam` destination within 15 yd of a lasher under 15% hp, and no two
     destinations within ~3 yd in one 2s window.
   - Nature Bomb escape latency must not regress from a 0.65s median.
3. **Judge it on output, not on the hazard it targets.** Melee DPS on the lasher wave and raid damage
   per minute must not drop. The `45d9a3e36` lattice looked correct on its own metric - Detonate fell
   to 1.9% of damage taken - while melee DPS halved and the raid killed 2 of 10 lashers.

## Files each fix would touch

| defect | files |
|---|---|
| 1 | `src/Ai/Raid/Uld/Util/UldEncounter_Freya.{h,cpp}` (`BuildFreyaEscapeHazards`), `src/Ai/Raid/Uld/Action/UldActions_Freya.cpp` |
| 2 | `src/Ai/Raid/Uld/Action/UldActions_Freya.cpp` (both latches, `FreyaBreakIronRootsAction::Execute`) |
| 3 | `src/Ai/Raid/Uld/Action/UldActions_Freya.cpp`, `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Freya.cpp` |

Defects 1 and 2 are independent of each other; both are behaviour changes by design, so neither can
be verified by argument alone - only by a pull.
