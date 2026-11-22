# Polish log: Hodir (25-man hard mode)

Trace slug `hodir`, raid `uld`, scenario `uld-hodir` (aggro start; drop at (1998, -262), ~20 yd south
of the frozen helpers, through window 5, then (1998, -288) from DC `e8c3b0c`, see round 2). Hard mode is a timer, not a toggle: a kill counts only if Hodir
never cast Shatter Chest `62501` (it goes out ~180 s after the engage). `tools/botobs/bosses/hodir.py
<trace>` prints `Rare Cache kept` / `shattered` in its banner; `--deaths` lists each death's killing
spell, Biting Cold stacks and Frozen Blows window. The runner's "hard mode" column reads `-` here.

## Round 1 (2026-11-22, build 0c08a0922, DC 7495350)

**Data.** Window 5, one pull per pool, all counted valid (`raidtest.py report --boss hodir --raid uld
--head 0c08a0922`):

| Pool | Run | Trace | Out | Cache | Deaths |
|---|---|---|---|---|---|
| uld1 | `tr-20261121-214258-30` | `603_30_hodir_1795297433` | kill 2:58 | kept (1.8 s inside) | 1: off-tank Frozen Blows swing |
| uld2 | `tr-20261121-214801-31` | `603_31_hodir_1795297683` | kill 4:46 | shattered 2:59.9 | 13: Frozen Blows raid tick 6, Biting Cold 5, Freeze 2 |
| uld3 | `tr-20261121-214056-29` | `603_28_hodir_1795297331` | kill 3:21 | shattered 3:00.0 | 5: off-tank Frozen Blows swing 2, Frozen Blows raid tick 2 (resto shaman, Reincarnation), Biting Cold 1 (MM hunter, 5 stacks) |

17 of 19 deaths fell inside a Frozen Blows window (Hodir's 20 s aura, 15 s after each Flash Freeze).
"Its own blow: a fall, lava or .die" in the report is Biting Cold `62188`, which the victim casts on
itself. Pace 0-3:00 against the 234.6k the cache needs: uld1 236k, uld3 216k, uld2 165k.

**Stop rules.** Not polished: only uld1 is a hard-mode kill with at most 3 deaths. First round.

**uld2 is a harness failure.** DC logged "pull 1/1: the boss engaged before the start step" 2 s after
the run began, so its refresh never ran: healers entered at 55-60% mana (two priests at 1-6% by
2:15-2:30), ranged at 63%, and the whole raid carried Exhaustion from the Auriaya run before it, so
lust came only at 4:34 when it expired (uld1/uld3 lusted at 0:18). Hodir's first target was Uldyas, a
balance druid whose Starfall stars (`53195`) were hitting the helper ice blocks (32938) at t=0; the
blocks stand 20.1-34.5 yd from the drop, inside Starfall's 30, and `npc_ulduar_flash_freeze::DamageTaken`
calls Hodir's `AttackStart` while he is `NOT_STARTED`. Why the druid cast before the start step is
not in the trace, which opens at the engage. Its 11 raid-tick / Biting Cold deaths at 2:58-3:50 follow
from the healers' mana.

**Picked: ranged and healers dying to Biting Cold plus the Frozen Blows raid tick** (14 deaths, 11 in
uld2; uld3 Uldzak 2:07, Uldzad 2:58 and 3:00). Bug: the shed arms at 4 stacks (5 in Starlight)
whatever Frozen Blows does, so bots sit at 3-5 stacks through the 20 s window while `64545` adds 3,999
every 2 s. 8 of the 14 carried 3-5 stacks (`--deaths`); the other 6 had 0-2, two just after a
resurrection and three (uld2 3:49.85) dying to one raid tick at 1 stack, which is healing, not cold; Biting Cold was 15-61% of each one's last
22 hits. Ranged spent 109-188 bot-seconds a pull at 3+ stacks inside the windows, healers 28-60. Out of
a fire, bots stood still at the new arm point 108 / 163 / 147 bot-seconds over 0-3:00 (reader,
`--buffs`, re-scored on these traces).

Second cause, same failure: Uldzak (uld3) armed at 4 stacks at 2:00.0, walked two legs, then
`hodir redirect threat action` (`ACTION_RAID + 4`, over the shed's +1) cast Misdirection and two dump
Steady Shots (1.6 s each, `RaidRedirectThreatAction::Execute`), standing it 3.5 s at 4-5 stacks; dead
at 2:07.1. Rare (3 armed redirects over three pulls), but the node never checked the shed.

Fix (`UldEncounter_Hodir.h/.cpp`, `UldTriggers_Hodir.cpp`, `UldActions_Hodir.cpp`):
`IsHodirBitingColdShedArmed(botAI, bot)` lowers both arm points by
`ULDUAR_HODIR_BITING_COLD_SHED_STACKS_FROZEN_BLOWS_DROP` 1 while `HodirFrozenBlowsActive`, and
`HodirRedirectThreatTrigger` stands down while the shed is armed. Frozen Blows and Biting Cold are
the same in 10 and 25 and in hard mode, so it holds for all four. Reader: Rare Cache verdict in the
banner, `--deaths`, and the arm point in `--buffs` follows the Frozen Blows window.

**Next window should show** (on a clean uld2 run): `--deaths` with no ranged or healer dying at 3+
Biting Cold stacks inside a Frozen Blows window; `--buffs` "standing still at the arm point out of a
fire" under ~100 bot-seconds; no `hodir redirect threat action` OK while a hunter holds 4+ stacks out
of a fire. Watch the cost: pace 0-3:00 should not drop below this window's 216-236k.

**Questions (orchestrator / user).**

1. *Harness:* void a pull DC logs as "the boss engaged before the start step"? *Recommended: yes*, in
   the runner, since the refresh (mana, cooldowns, Exhaustion) never ran. Also move the `uld-hodir`
   drop south to about (1998, -288) so no helper block is within 40 yd of it; navprobe it first.
2. *Plan:* the off-tank died in all three tank deaths, each time inside a Frozen Blows window, after 2-3
   swings in ~5 s (25-31k each plus 4k melee). The main tank's generic taunt (`hand of reckoning`,
   `righteous defense`, relevance 27) fires 0.1-0.6 s after each swap taunt and flips Hodir back for
   part of the window, against the swap's intent, but the flips also cut his swings to 3-5 a window
   against ~8 on a steady holder. Options: (a) keep the swap and stop the main tank's taunt in a
   window, so the off-tank eats all of it; (b) alternate deliberately, the tank not holding taunting
   after each swing; (c) tie each tank's defensive cooldown to the window. *Recommended: (c) first*,
   since it adds survival without moving more swings onto either tank, then (b) if off-tanks still
   die.

**Also seen, not picked.** uld3 missed the cache by ~21 s: Hodir was at 15.1% when the last freeze
landed, above `_LAST_WAVE_HEALTH_PCT` 12, so the raid spent 102 bot-seconds breaking blocks whose fire
came at 2:59.5; skipping them would still have ended ~3:16. `set behind` right after a dodge ended in
a live icicle pool 22-36 times a pull (no death).

## Round 2 (2026-11-22, on Custom 61011f323)

**Data.** No new pulls: round 1 (`5f16288c8`) has not been pulled yet, so the stop rules, the verdict
on round 1 and the failure ranking are skipped. Window 5's three traces (round 1's table) re-read for
the tanks with the new `hodir.py --tanks`. Deaths by mechanic per pull, as round 1: uld1 1 (off-tank
Frozen Blows swing); uld2 13 (Frozen Blows raid tick 6, Biting Cold 5, Freeze 2); uld3 5 (off-tank
Frozen Blows swing 2, raid tick 2, Biting Cold 1).

**Picked: the off-tank dying to Frozen Blows swings** (uld1 Uldxam 2:56.1, uld3 Uldzab 2:08.3 and
2:56.2), with the user's answer (2026-11-22) to round 1's question 2: option (c), each tank spends a
defensive in its own Frozen Blows window. Option (b), deliberate alternating taunts, only if off-tanks
still die once (c) has been pulled; not built.

Evidence (`--tanks`, death auras):
- All three died inside a window with no tank button up: the bear had none ready, the DK had
  Anti-Magic Shell.
- uld1, blood DK: the class `low health` node (`GenericDKStrategy.cpp:68`) cast Icebound Fortitude at
  1:18.25 in window 1 after an 18.6k swing, so it was on cooldown for window 3; Anti-Magic Shell was
  never cast all pull (no class node casts it on a tank: it is only `anti magic zone`'s fallback,
  `GenericDKStrategy.cpp:40-42`, which nothing triggers); `vampiric blood` came back IMPOSSIBLE twice.
  Window 3: 29.9k at 91.8% (2:51.0), then 26.6k at 53.4% (2:56.1) killed.
- uld3, bear: after a 27.3k swing the class nodes cast Survival Instincts 1:11.12, Frenzied
  Regeneration 1:11.96 and Barkskin 1:12.24 (`FeralDruidStrategy.cpp:87`, `BearDruidStrategy.cpp:123`,
  `GenericDruidStrategy.cpp:58` at `almost full health`, 85%), all in window 1. Window 2 (1:51) had
  nothing ready (Barkskin back at 2:12): 27.3k, 30.7k, 25.0k in 5 s with a Lay on Hands between, dead
  2:08.3. Barkskin again at 2:24.96, between windows, so window 3 had nothing either: dead 2:56.2.
- uld2's bear cast Barkskin six times by class node: at 0:15.97 between windows, and once in each of
  the five windows, including two (1:08.06, 4:28.65) where he held it under a second.
- Flips: each swap taunt drew the other tank's taunt 0.1-0.6 s later. Over the 11 windows, stints with
  no swing ran 0.18-0.71 s for flips and 1.4-6.5 s for the main tank (mostly at a window's start);
  stints that took a swing ran 1.6-18.5 s.

Root cause: nothing ties a tank's button to its window. The class nodes react to health
(`TankWarriorStrategy.cpp:237`/`245`, `BloodDKStrategy.cpp:153` too) and spend the buttons after the
swing that hurt, all at once, or between windows.

**Fix** (`UldEncounter_Hodir.h/.cpp`, `UldTriggers_Hodir.h/.cpp`, `UldActions_Hodir.h/.cpp`,
`UldDefinition_Hodir.cpp`; reader `tools/botobs/bosses/hodir.py`, tests in `test_botobs.py`):
- `hodir tank defensive` (`ACTION_RAID + 4`, `UldDefinition_Hodir.cpp:86`): `GetHodirTankDefensive`
  (`UldEncounter_Hodir.cpp:176`) returns `NextTankDefensive(..., physicalOnly false)` for a tank Hodir
  has held `ULDUAR_HODIR_TANK_DEFENSIVE_HOLD_MS` 1000 inside a Frozen Blows window, once per tank per
  window (`HodirTankDefensiveLatch::spentWindow`, the aura's apply time). 1 s clears every flip and
  catches every stint that took a swing above. `63511` is frost (school 16, magic), so Anti-Magic
  Shell counts.
- `hodir tank defensive hold` (`:107`): `HeldTankDefensiveNames()` blocked for `Role::Tank` while he is
  engaged and the tank is at 25% (`_LAST_RESORT_PCT`) or more.
- Probe `hodir.defensive`: `idle`, `wait`, `spent`, the button, `covered`, `none`.
- Button order is `NextTankDefensive`'s, shortest cooldown first: DK Anti-Magic Shell (45 s, every
  window), then Vampiric Blood, Icebound Fortitude; bear Barkskin, then Frenzied Regeneration,
  Survival Instincts; warrior Last Stand, Shield Wall; paladin Divine Protection only (3 min).
- Holds for 10 and 25 and for hard mode: Frozen Blows is the same aura by difficulty (62478 / 63512),
  and hard mode is only the timer.
- Cost: a main tank holding him 1 s+ at a window's start spends its button there even with no swing
  (uld1's paladin held 5-6 s at the start of windows 1 and 2 with none), and a paladin's one button
  then misses the next three windows.

**Next window should show** (`--tanks`, `--deaths`): in every window each tank he holds 1 s+ casts one
button by `node`, none inside a flip; no `class` button above 25% health, between windows or in them;
fewer than window 5's 3 off-tank Frozen Blows deaths over 11 windows, and any that remain with
`hodir.defensive` reading `none` (nothing ready) or a button up. Off-tanks still dying means (b).

**Harness, from the next build.** The `uld-hodir` drop is (1998, -288), 46 yd from the nearest ice
block, with the Winter Revenant pack despawned (DC `e8c3b0c`). Shared `MovementAction::GetFollowAngle`
now spreads bots under the DC driver (`61011f323`): window 5 gave every bot angle 0, so every
`MoveInside` park (shelter run, Storm Cloud spread and collect) put them all on one point at the park
radius. Window 5's shelter and Storm Power figures can move without a Hodir change.

**Window 7, read at review** (landed during this round; build `61011f323` = round 1, the new drop and
the spread fix; all three started by the step, so refreshed):

| Pool | Run | Trace | Out | Cache | Deaths | Pace 0-3:00 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-091848-4` | `603_4_hodir_1795339196` | kill 3:45 | shattered | 2: off-tank Frozen Blows swing | 183k |
| uld2 | `tr-20261122-091848-5` | `603_6_hodir_1795339202` | kill 4:02 | shattered | 1: off-tank Frozen Blows swing | 175k |
| uld3 | `tr-20261122-091848-6` | `603_5_hodir_1795339205` | kill 3:42 | shattered | 2: off-tank Frozen Blows swing, melee Hodir melee (outside a window) | 193k |

- *Round 1 verdict:* its mechanic stopped. No ranged or healer died; standing still at the arm point
  41 / 22 / 28 bot-seconds (target under ~100).
- *Its cost check failed:* pace fell from window 5's 216-236k, so no pool kept the cache. Drop and
  spread angle changed in the same build, so the cause is open; uld1's `hodir.shuttle` churn went
  72 → 202 against window 5. Round 3 should split it.
- *Round 2's pick holds:* all four off-tank deaths came inside a window with `--tanks` "button up:
  none", and uld3's bear again spent all three buttons in window 1 (1:11.9-1:14.8).
- *Watch the main tank:* its first 1 s+ stint in a window took no swing in 12 of 23 windows (w5 + w7),
  with him barely moving, so the node spends there. In window 5 uld1's paladin would have spent
  Divine Protection at ~1:06 with no swing, leaving none for window 3, where he took two (51k).
  Main-tank deaths after such a spend mean the hold should wait for the first swing.

## Round 3 (2026-11-22, on Custom 1b45df985)

**Data.** Window 8 (round 2 code, all three started by the step), `raidtest.py report --boss hodir
--raid uld --head 02925e260`; window 7 and window 5 re-read beside it (`--head 61011f323`, round 1's
table) with `hodir.py` and replays in the session scratchpad (`hodir-r3/`).

| Pool | Run | Trace | Out | Cache | Deaths | Pace 0-3:00 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-112132-4` | `603_4_hodir_1795346605` | kill 4:00 | shattered | 1: healer Freeze 3:49.6 (4 Biting Cold stacks, outside a window) | 169k |
| uld2 | `tr-20261122-112133-5` | `603_6_hodir_1795346628` | kill 3:48 | shattered | 0 | 178k |
| uld3 | `tr-20261122-112133-6` | `603_3_hodir_1795346638` | kill 3:41 | shattered | 1: off-tank Frozen Blows swing 2:55.7 (Barkskin up) | 188k |

**Stop rules.** Not polished: no pool kept the cache (234.6k wanted). Third round, third failure.

**Round 2 verdict: held.** Off-tank Frozen Blows deaths 3 in 11 windows (w5), 4 in 12 (w7, all with no
button up), 1 in 12 (w8): uld3's bear, Barkskin up, after 6 swings (157k) in window 2 and 4 (90k) in
window 3. Every tank held 1 s+ in a window cast its `node` button when one was ready; the only `class`
buttons came under 25% (Icebound Fortitude at 4%, Frenzied Regeneration at 24%). `--tanks` had read
two `node` casts as `class` (Anti-Magic Shell at 99%, Divine Protection at 62%): the node's OK row
lands a millisecond after its cast, fixed. No main tank died, so option (b) stays unbuilt.

**Picked: pace.** Every window 7 and 8 kill missed the cache. By window (boss dps, w5 → w7/8): 0:00-0:20
73-76k → 60-71k, 0:20-0:57 337-345k → 213-263k, 0:57-1:45 239-265k → 168-233k, later about even.
By role (snapshot `dealt`), against w5 over 0-3:00 ranged lost ~29k dps-equivalent, 18k of it in
0:20-0:57 (ranged 178-219k → 81-132k there), melee ~14k, mostly 0:57-2:34.

Cause 1, bug: **a ranged or healer bot latched onto the fire Hodir is then held on gets no stand until
Flash Freeze.** `FindHodirFireStand` (`UldEncounter_Hodir.cpp`) returned false on a rejected latched
stand but kept the latch until the fire died. From the south drop Hodir is 13-16 yd south of the centre
at 5 s in four pulls (w5: 1 yd), so the first fires land south among the ranged, who latch a stand on
one; the hold then picks that fire (nearest the centre within 25) and the tank drags him onto it.
w8 uld1: Uldxaq `hodir.stand fire` 21.606 (anchor 2004,-259), `hodir.tankhold` 4294968875 21.792,
`none` 25.220 until 58.260 after the freeze; Uldxak 21.804 → 24.643 → 54.342; healer Uldxae 21.784
→ 24.748; a legal fire at (2015, -242), 16 yd from him, came up at 36.1 s. A replay of the latch from
the snapshots (it agrees with `hodir.stand` on ~90% of samples) puts ranged and healers stuck 32-56% of
0:00-0:57 in four of six pulls (w7 all three, w8 uld1) against 0-4% in w5; ranged stood in a fire
15-35 yd from him 15-22% of the first fire window there, 45-53% in w8 uld2/uld3, 71-72% in w5. With a
3 s drop the replay offers a fire stand 42-71% of 0:20-0:57 instead of 9-25% in those four.

Cause 2, bug from the shared spread (`61011f323`): **Storm Cloud carrier and receivers parked on
different points.** Both actions used `MoveInside(rally, 2)` (`UldActions_Hodir.cpp` spread and
collect), which `MoveNear` offsets by `GetFollowAngle`: angle 0 put everyone on one point, the spread
puts each bot on its own point of a 2 yd ring, up to 4 yd from the carrier, outside the 3 yd pulse.
Parked receivers lacking Storm Power stood inside the pulse 48/70/43% (w5) against 45/20/37/36/24/44%;
Storm Power 6.4-7.1 a cloud against 5.4-6.6; 7 clouds timed out with charges left in six pulls, none
in three; median time to spend 4-6 s against 5.5-15 s.

Third, harness, not a bug: from (1998, -288) the raid walks 26 yd further, the first mage blocks break
2-4 s later (9.6-14.1 s against 7.7-9.9), the first fire lands at 19.2-22.0 s against 18.0-18.6.

**Fix** (`UldEncounter_Hodir.h/.cpp`, `UldActions_Hodir.cpp`; reader `hodir.py`, tests):
- The fire latch drops after 3 s of continuous band reject, like Starlight's (constant renamed
  `ULDUAR_HODIR_STAND_REJECT_DROP_MS`). New probe `hodir.firestand`: `stand <fire>`, `held noreach`,
  `none`.
- Carrier and receivers walk to the rally point itself (`MoveTo`, park check unchanged at 2).
- Reader: `--storm` (clouds, Storm Power a cloud by role, spent or timed out, parked receivers inside
  the pulse); `--buffs` adds ranged in a legal fire over the first fire window and 0-3:00, and the
  fire latch's held-out-of-band runs.
- 10/25 and hard mode alike: neither depends on size or the timer.

**Next window should show:** `--buffs` "fire latch held out of the band" runs no longer than ~3.5 s,
and ranged in a legal fire over the first fire window at 45%+ on every pool; `--storm` parked
receivers inside the pulse at 50%+ and no cloud timing out with charges left; 0:20-0:57 boss dps up
from 213-263k. The replay suggests roughly +10-15k over 0-3:00, short of 234.6k: the opening (drop)
and the melee loss stay.

**Also seen, not picked.** Melee `hodir biting cold shed` moves rose to 32-105 a pull from 7-29 in w5:
round 1 arms the shed one stack sooner under Frozen Blows for every role, though its deaths were
ranged and healers. If melee pace stays down, try the drop for ranged and healers only.

**Questions (orchestrator / user).**

1. *Harness or plan:* the south drop costs ~3 s on the first fire and pulls Hodir off the centre at the
   start. (a) keep it, a real raid enters from the south door too; (b) DC walks the raid to about
   (1998, -262) as part of the start step, where a Starfall on a block no longer pulls him early;
   (c) the ranged wait at the drop until the tank holds him at the centre. *Recommended: (a)*, and
   re-judge after this round, since the latch was the part that cost most.
2. *Shared code (no change made):* the spread in `GetFollowAngle` turns every `MoveInside` /
   `MoveNear` meant to stack bots on one point into a ring of that radius; 40 calls outside Hodir
   (Yogg-Saron, Razorscale, Naxx, Sunwell, Tempest Keep, Black Temple). *Recommended:* check each stack
   point when its raid is next pulled.
