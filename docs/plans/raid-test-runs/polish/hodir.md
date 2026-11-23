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

**Review (round 3).** Cause 1 holds: w8 uld1 Uldxaq latched the fire at (2001, -251), the tank held
Hodir at (2000, -248), its stand sat 11.9 yd from him until 58 s. Cause 2 only in part: of the w7/w8
parked receivers outside the pulse, the 2 yd ring is a minority. The carrier stood within 2 yd of its
rally 18-27% of carry samples (w5 14/19/59%), ranged carriers over 4 yd off it 43-66%, and
`hodir raid position action` was the commonest next mover of a parked ranged receiver and moved ranged
and healer carriers off the rally: once the spread (2 yd) or collect (4 yd) stands down, the anchor or
declump walks the bot to its stand, and that walk's `MOVEMENT_COMBAT` lock refuses the walk back until
it lands. Melee receivers leave by `reach melee` / `set behind` (generic, untouched).
- Fix: `HodirRaidPositionTrigger` stands down for a carrier (`HodirSpreadStormCloudTrigger`) and for
  a receiver within park 2 of the rally without Storm Power (`GetHodirStormPowerRally`, now shared
  with the collect trigger).
- Reader: `--storm` adds the carrier's share within 2 yd of its rally; a fire latch run stops at a
  shed, shelter, collect or spread act or a carry, where the position node stands down and both probes
  go stale; the first fire window ends at the first landing after the first fire.
- Next window adds: `--storm` carrier within 2 yd of its rally well above 18-27%.

**User answer (2026-11-22):** shed Biting Cold with short hops instead of walking legs, using the
fork's existing jump code (no port of upstream PR #3014). A hop must land about a yard away: a
jump that lands on its start point leaves the movement flags off (`MoveSplineInit::Launch` treats it
as orientation only), so `isMoving()` stays false and no stack drops.

## Round 4 (2026-11-22, on Custom eda15422d)

**Data.** No new pulls: round 3 (`e54259b02`) is merged but not pulled (window 9 is pulling round 2
code), so the stop rules, the verdict on round 3 and the failure ranking are skipped. Window 8's three
traces (round 3's table) re-read with the new `hodir.py --shed` as the walking baseline. Deaths by
mechanic per pull, as round 3: uld1 1 (healer Freeze, 4 Biting Cold stacks), uld2 0, uld3 1 (off-tank
Frozen Blows swing).

**Built: the user's answer, hop instead of walk.** Walking legs (window 8, per pull, by role):

| | uld1 | uld2 | uld3 |
|---|---|---|---|
| ranged: stacks/s, ticks used, off the spot p50 | 0.43, 83%, 11.3 yd | 0.50, 93%, 15.1 | 0.42, 82%, 6.5 |
| melee | 0.42, 84%, 9.7 | 0.60, 104%, 9.6 | 0.41, 84%, 8.6 |
| heal | 0.38, 87%, 6.0 | 0.43, 81%, 17.0 | 0.44, 82%, 12.7 |
| tank | 0.47, 91%, 6.9 | 0.53, 100%, 3.2 | 0.48, 94%, 3.6 |
| melee in reach / Starlight held, of sheds that started there | 61% / 70% (ranged), 67% (heal) | 76% / none | 64% / 82% |

"Ticks used" is twice the stacks shed over the Biting Cold ticks (`62188` rows) in the shed, so 100% is
a stack every two ticks, the most the aura allows. The walk already sheds near that rate; what it costs
is the spot. Ice Shards 15 / 5 / 14 hits; Biting Cold 1.20 / 0.78 / 1.07 M.

Core, checked: `spell_hodir_biting_cold_player_aura` (`boss_hodir.cpp:1257-1297`) reads `isMoving()`
once per tick; two moving ticks in a row drop a stack. `isMoving()` is `MOVEMENTFLAG_MASK_MOVING`, which
`MoveSplineInit::Launch` sets for any spline but a same-point one (`MoveSplineInit.cpp:69-90`) and
`Unit::DisableSpline` clears on finalize (`Unit.cpp:718-722`). `MoveJump` (`MotionMaster.cpp:676-694`)
lasts length over `speedXY` and returns at `speedXY` ≤ 0.1. In `Unit::Update` the aura ticks in
`_UpdateSpells` (`:548`) before `UpdateSplineMovement` (`:642`), same diff, and bot AI runs after it
(`OnPlayerAfterUpdate`), so a hop up past the second tick by any margin covers both.

**Fix** (`UldEncounter_Hodir.h/.cpp`, `UldActions_Hodir.h/.cpp`; shared `MovementActions.h/.cpp`; reader
`hodir.py --shed`, tests):
- `HodirBitingColdShedAction` hops (`GetHodirShedHop`) between two ends `_SHED_HOP_HALF` 0.5 either side
  of where the shed started, each hop to the far end (a yard). Flight is the aura's `GetPeriodicTimer()`
  + one period + 150 ms, 1.15-2.15 s, so each hop covers the next two ticks and drops a stack; ~0.4 yd
  high (`speedZ` 4). Spot and axis latched per bot, re-taken once pushed 1.5 yd off.
- Axis: melee hop toward him and back, so the inbound hop faces him (a spline faces the bot along its
  travel, and a player's swing needs him within 60° unless it stands within 2 yd of his centre);
  everyone else square to the Starlight zone they stand in, else to him.
- A hop holds the controlled motion slot (`CanMove` false), so the icicle dodge and the shelter run cut
  it first (`CutShedHop`: `MotionMaster::Clear(false)` plus the `last movement` booking).
  `IsHodirShedHopInFlight` matches the spline's destination, so a knockback isn't mistaken for one.
- In flight the shed returns false (instants go out); a normal cast is left to finish, since a hop
  launches mid-cast and cancels it; a channel is broken as before.
- **Walking legs stay only where no hop end works**: an end inside a live icicle's dodge trigger
  (4.5 / 7.5, tanks exempt, as they eat icicles), or both ends pulled within 0.3 yd by the floor check
  (a wall; the room is open floor). Tank, shelter and Starlight legs keep their branches as fallback.
- Shared: `MovementAction::JumpTo` takes optional `speedXY` / `speedZ`, defaulting to run speed as before,
  so no other caller changes (Nexus, Naxx Thaddius, Sunwell Twins, Tempest Keep, Thorim, battlegrounds).
- Probe `hodir.shuttle` adds `hop` and `cut`. Reader `--shed`: per role stacks/s, ticks used, samples
  moving, offset off the spot p50/p90, hops, walked legs, cut hops, and how much of each shed a bot that
  started in Starlight, in melee reach or inside a Storm Cloud pulse stayed there. `--churn`'s own-spot
  leg count skips hops.
- 10/25 and hard mode alike: Biting Cold is the same aura and timer in all four.

**Next window should show** (`--shed`, against the table above): ticks used 90%+ and 0.45+ stacks/s
for every role; off the spot p50 under 1 yd, p90 under 2 (a cut hop re-homes); hops far above walked
legs; melee in reach 90%+ and Starlight held 90%+ of the sheds that started there; Storm Cloud pulse
kept likewise where any. No more Ice Shards hits than 5-15 a pull (a hop that wasn't cut in time) and
no Flash Freeze death (a shelter run held by a hop). Snapshots of a shedding bot should read `moving` 1
with `movegen` 16 (effect) in flight.

**Not built, watch.** The outbound melee hop faces away from him, so melee lose swings for about half
of each shed beyond 2 yd of his centre: `MoveJump` can't keep the facing (`OrientationFixed` isn't
reachable from it). If melee dps during sheds matters, the options are a module-side spline that sets
`OrientationFixed` (shared movement code) or a core `MoveJump` option.

**Review (round 4).** Core claims hold: `spell_hodir_biting_cold_player_aura` ticks on `EFFECT_0`
(`SPELL_AURA_PERIODIC_DUMMY`), so `GetEffect(EFFECT_0)` reads the right timer; `AuraEffect::Update`
keeps `m_periodicTimer` as time to the next tick; `MoveJump` launches at `Mutate` with
`generatePath` false, so the spline ends exactly on the latched end. `--shed` on window 8 reproduces
the walking table.
- Fixed, bug: a melee hop home is a world point, but the melee spot is wherever he is. In window 8's
  melee sheds he moved 4+ yd during 8 of 16 (uld1), 1 of 7 (uld2) and 2 of 10 (uld3), 10 of the 11 with
  a taunt inside the shed; a home left where the shed started would have had him in reach 55 / 91 /
  47% of the samples of sheds that started in reach, and 11 / 1 / 5 melee sheds started out of his
  reach already. Hops kept such a bot hopping out of reach for the whole shed, since the shed outranks `reach
  melee`. `GetHodirShedHop` now returns false for a melee non-tank whose home is beyond
  `GetMeleeRange` of its victim (him, or a block it is breaking, which then keeps its hops), so the
  walking legs (which prefer points near him) close the gap; once a leg moves it 1.5 yd the home
  re-latches and hops resume.
- Fixed, minor: the `hodir.shuttle` `hop` probe fired before `JumpTo`, so a refused hop (a walk still
  booked) read as one; it now fires in `LatchHodirShedHop`, after the jump went out.
- Fixed, minor: a tank standing in Starlight hopped square to the zone, not to him, against the
  findings doc ("square to him", like the fallback legs); `HodirShedHopAxis` now skips Starlight for
  tanks.
- Left: tanks hop square to him, so he sits at ~90° and is behind them through the second half of
  each hop and the pause after it (no parry or block, no swings). The walking legs did the same, so
  no regression; radial hops would trade it for a full outbound half facing away.
- Next window adds: `--shed` melee walked legs above zero (the out-of-reach sheds), with melee in
  reach still the figure to judge.

## Round 5 (2026-11-22, on Custom cbc0f206b)

**Data.** Window 10 (build `697052475`: rounds 3 and 4, roster DKs re-specced to 71 points),
`raidtest.py report --boss hodir --raid uld --head 697052475 --pulls 10`; windows 5, 7, 8 and 9 re-read
beside it (window 9, round 2 code: `603_30_hodir_1795357824`, `603_31_hodir_1795358182`,
`603_33_hodir_1795358594`, kills 3:49 / 3:55 / 3:25). Scratch scripts in the session scratchpad
`hodir-r5/` (role and slice split from snapshot `dealt` by target, buff states, breakers, hold geometry).

| Pool | Run | Trace | Out | Cache | Deaths | Pace 0-3:00 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-165800-16` | `603_18_hodir_1795366726` | kill 3:27 | shattered 3:00.1 | 0 | 203.6k |
| uld2 | `tr-20261122-171946-23` | `603_25_hodir_1795368040` | kill 3:23 | shattered 3:00.0 | 0 | 205.0k |
| uld3 | `tr-20261122-170422-19` | `603_21_hodir_1795367106` | kill 3:23 | shattered 3:00.0 | 1: off-tank (bear) Frozen Blows swing 2:09.8, in a window, Frenzied Regeneration up | 197.6k |

Window 9 deaths: uld1 2 (the un-respecced DK off-tank, Frozen Blows swing in a window, no button up,
1:19.8 and 3:39.1), uld2 1 (ranged Freeze 3:04.5, 3 Biting Cold stacks, outside a window), uld3 0.

**Stop rules.** Not polished: kills on all three with 0/0/1 deaths, but the cache shattered in all
three (234.6k wanted). Pace is picked for the second time (rounds 3 and 5), not three running.

**Round 4 verdict (hops), against window 9's walking legs.** The raid walked less and kept its spots;
Biting Cold did not fall.
- Walked 10.6-11.3k yd a pull against 11.6-14.6k (w9) and 12.8-15.5k (w8); 31-36% undone within 5 s
  against 35-41%.
- `--shed` (w9 → w10): ranged 0.41-0.48 → 0.49-0.53 stacks/s, ticks used 86-93% → 95-115%, off the
  spot p50 8.6-10.9 → 0.5 yd (p90 12.4-22.9 → 4.1-6.3); healers p50 4.0-10.2 → 0.3-0.6; tanks
  4.1-13.3 → 0.5; melee p50 10.5-11.4 → 0.5-3.1, ticks 74-98% → 83-100%. Melee in reach through
  sheds that started there 53-100% → 70 / 90 / 100%; Starlight held 74-100% (ranged and healers).
  Hops 11-126 a role against 0-30 walked legs (uld2 melee 30: the out-of-reach sheds).
- Biting Cold 0-3:00 0.78-0.89M → 0.92-1.01M, bot-seconds at 4+ stacks 30-36 → 37-58, standing still
  at the arm point 15-32 → 21-29; no Biting Cold death in either window. Ice Shards 6-12 → 9-17 hits;
  no Flash Freeze death. Snapshots read `moving` 1 / `movegen` 16 in flight.
- Cost: melee deal 0.57-0.69 of their standing rate while shedding (walking legs 0.64-0.70), and
  shedding is 2-5% of melee time, so the whole melee shed costs 0.3-1.6k dps a pull.
- **Melee hop facing, measured:** of melee shedding samples beyond 2 yd of him, 45-53% face more than
  60° off him (walking legs 56-72%). A facing fix would buy back at most about half of the 0.3-1.6k
  above, so no question to the user.

**Round 3 verdict (first data).** Fire latch held out of the band with no stand: 3 / 11 / 8 runs,
longest 4.3 / 7.3 / 3.6 s (w8: runs of 20-43 s). Ranged in a legal fire over the first fire window
47 / 51 / 60% (target 45%+; w8 15-22% in four of six). Storm Cloud: carrier within 2 yd of its rally
50 / 68 / 41% (w7-w8 18-27%); parked receivers inside the pulse 57 / 45 / 41% (target 50%+); clouds
timed out with charges left 2 / 3 / 2 (target none). Boss dps 0:20-0:57 265 / 287 / 223k (w7-w8
213-263k). Held, the storm part only in part.

**Round 2's mechanic:** one off-tank Frozen Blows death in 9 windows (w8 1 in 12), button up. Option
(b) stays unbuilt.

**Picked: pace.** Where the 30-37k goes, boss dps from his health per slice (open 0-~20 s, first fire
window to the first landing, then landing to landing), w5 (uld1 236k, uld3 216k) → w10:

| Slice | w5 | w9 | w10 | w10 against w5, over 0-3:00 |
|---|---|---|---|---|
| open | 67-71k | 46-66k | 59-78k | 0 |
| first fire window | 325-331k | 182-245k | 221-286k | −14.6k |
| cycle 2 | 242-265k | 192-226k | 177-244k | −9.9k |
| cycle 3 | 203-207k | 195-206k | 180-262k | +1.3k |
| to 3:00 | 120-198k | 143-197k | 147-239k | +4.3k |

By role, damage on Hodir off snapshot `dealt` by target (~95% of his health loss; helpers deal the
rest): ranged w5 125k / 78k → w9 67-89k → w10 72-82k; melee w5 92k / 120k → w9 85-94k → w10
103-112k (both DKs 4-6k each → 10-14k each after the re-spec, so non-DK melee are flat to down);
tanks 6-8k, healers 0. With a fire burning boss dps ran 263-283k (w5) against 215-244k (w10); with
none 150-155k against 129-175k. The first fire window's loss is ranged: 206 / 171k (w5) against
96 / 167 / 104k, with Storm Power on ranged 52-67% of it against 4 / 69 / 17% (the first carrier was
a melee bot or a healer standing away from the fires, the boss's pick) and their fires 26-31 yd from
him against 21-23 (where the freed mages dropped them).

Per ranged bot, total dealt per second: ~20k in a fire with Storm Power, ~10k in a fire, 4-6k with
neither. The largest share the strategy controls is **ice**: ranged spend 27-32% of their time
shooting helper and trapped blocks (w7-w10; w5 uld1 18%) at 4.6-6.7k a second against 9.5-14.5k on
Hodir, moving 39-48% of it. New `--blocks` line, w10: 543 / 552 / 486 bot-seconds, 5.8 / 5.1 / 6.1k a
second (on Hodir 10.9 / 12.3 / 13.2k), moving 46 / 45 / 44%, walked by `hodir raid position action`
19 / 23 / 23%, over 35 yd from the block 22 / 27 / 26% (w5 uld1 330 bot-seconds, 28%, 11%).

**Root cause, bug** (the findings doc's open item): the targeting node hands a ranged bot a block, and
`HodirRaidPositionTrigger` (relevance +0, so it runs whenever the bot's casting leaves room) walks it
to its Starlight or fire stand, or by the band and declump rules, away from the block. Of its accepted
moves on a bot targeting a block, those under a Starlight stand went away from the block 48-74% of the
time and ended past 30 yd of it 41-79% (w9-w10, 111-143 such moves a pull); under no stand, 32-60%
ended past 30 yd. `reach spell` then walks it back (2-10% of on-block samples).

**Fix** (`UldTriggers_Hodir.cpp`): `HodirRaidPositionTrigger::IsActive` stands down for a ranged
non-healer whose `current target` is a live helper (32938) or trapped-raider (32926) block, and notes
`hodir.stand` `block`. Healers are left out: nothing hands them a block, yet in w10 uld1 their snapshot
target was a block 100-112 s each. Dodge, shelter run and shed are separate nodes and still fire. Holds for 10
and 25 and hard mode alike: the blocks and the breaker budget are the same, and nothing here reads a
player.

**Second, the reach audit's Hodir row, confirmed** (his combat reach 7.5 since `2026_11_06_01`):
- Tank hold: still, he sat a median 2.5-5.0 yd off the hold point (p90 5.3-9.5) in windows 5-10; his
  holder was a median 5.4-9.2 yd away, up to his chase stop of 9.5 (7.5 + 1.5 + `CONTACT_DISTANCE`); 26 / 37 /
  29% of the melee within 10.33 yd of him stood outside the held fire's 11 yd (w9 16-36%, w5 19-26%),
  none of them would with him on the point. Fix (`UldEncounter_Hodir.cpp`, `.h`): `HodirTankSpot`
  stands both tanks the live chase stop off the point (`GetCombatReach` of both plus
  `CONTACT_DISTANCE`, 9.5 here) instead of the fixed 4.5, so he stops on it; the off-tank's spot is
  already inside his chase stop when it taunts, so the taunt leaves him there. The spot still goes
  through `ValidateStaticFloorPoint`; the farthest one now sits 34.5 yd from the centre against 29.5
  (not navprobed: no Docker in this lane).
- Starlight: a stand 10.0-10.33 yd from him was inside his melee range; starlit casters stood there in
  0.1-2.6% of their starlit samples, and he turned on a healer or ranged bot in 0-9 samples a pull.
  Fix: the stand's inner edge is his live `GetMeleeRange(bot)`; `ULDUAR_HODIR_STARLIGHT_MIN_BOSS_GAP`
  and `_HOLD_TANK_OFFSET` deleted. Reader `STARLIGHT_BAND` follows (10.33).

Reader (`hodir.py`, tests): `--blocks` ends with the ranged-on-ice line above; `STARLIGHT_BAND` from
his melee range. Findings doc: breakers, tank hold, Starlight gap, Storm Power's aura 61 (`MOD_SCALE`,
not haste), the pace split.

**Next window should show** (`--blocks`, `--hold`, `--pace`):
- ranged shooting ice: walked by the position node near 0% (19-23%), moving under ~30% (44-46%), over
  35 yd from the block under ~10% (22-27%), dealt per second up from 5.1-6.1k, bot-seconds under 400
  (486-552); mage blocks and "next fire" after each landing sooner (15-23 s); `hodir.stand` `block`
  in the notes.
- `--hold`: "Hodir more than 5 yd off the hold point" in fire windows well under this window's 20-30 s;
  melee within 10 yd of him inside the held fire 90%+ (72-81%).
- pace 0-3:00 up roughly 10k from 198-205k: ~150 bot-seconds back on Hodir at ~11k plus earlier fires.
  Still short of 234.6k; what is left sits with Storm Power on ranged in the first fire window.
- watch: breakers standing within 15 yd of him (icicle or Freeze shared with the melee), ranged Ice
  Shards hits, and no main-tank death from the tanks standing 9.5 yd off.

**Also seen, not picked.** Storm Power per cloud 5.5-6.1 with 2-3 clouds a pull timing out: a healer
or ranged carrier standing out of a fire and away from the others (uld3 25.8-55.8 s: three charges in
30 s) gathers nobody inside the 15 yd collect leash. The fourth helper wave (~2:35, him at 21-29%)
costs 98-154 bot-seconds on ice for a fire that lands ~2:50-2:55; roughly a wash before 3:00.

**Review (round 5).** The root cause holds: `--blocks` reproduces the w10 line, and on w9 uld1 and
all three w10 pulls the position node was the commonest accepted mover of a ranged bot targeting ice
(112-145 moves a pull, against 45-115 for `reach spell`); under a Starlight stand 57-77% of its moves
went away from the block. The 9.5 chase stop matches the core (`TargetedMovementGenerator.cpp:256`,
`CONTACT_DISTANCE` + both reaches).
- Fixed, bug: the tank walking legs (the shed's fallback when no hop end works) ran square to the
  hold bearing for both tanks, which is radial for the off-tank. At the old 4.5 offset both ends
  stayed in his reach; at 9.5 the far end sat 12.5 yd from the point, past his 10.33 melee range, so
  with the off-tank holding him in a Frozen Blows window a leg would drag him off the fire. Each tank's
  legs now run square to its own line from the point (ends 9.96 yd out).
- Watch: the main tank now stands 9.5 yd from a held fire's centre, 1.5 inside its 11 yd aura (4.5
  before), so a tank standing 1.5 yd further out than its spot loses the fire and sheds Biting Cold
  itself.

## Round 6 (2026-11-22, on Custom a113fac87)

**Data.** Window 11 (build `d2874f536`: round 5), `raidtest.py report --boss hodir --raid uld --head
d2874f536 --pulls 10`; windows 5 and 10 re-read beside it. Scratch scripts in the session scratchpad
`hodir-r6/` (per bot-sample tables, paired buff worth, Storm Cloud pulses and a one-pulse replay,
fires, fourth wave, Singed deficit).

| Pool | Run | Trace | Out | Cache | Deaths | Pace 0-3:00 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-184305-10` | `603_10_hodir_1795373027` | kill 3:18 | shattered 3:00.0 | 0 | 208.0k |
| uld2 | `tr-20261122-184405-11` | `603_11_hodir_1795373093` | kill 3:12 | shattered 3:00.0 | 0 | 214.6k |
| uld3 | `tr-20261122-184405-12` | `603_12_hodir_1795373098` | kill 3:20 | shattered 2:59.9 | 1: ranged Frozen Blows swing 1:10.7 (threat pull) | 208.8k |

uld3's death: Uldzaq (fire mage), shooting its helper block 20.7 yd from him, took Hodir off the
off-tank bear 6 s into the window (1:09.5); the bear's swap taunt at 1:09.7 logged OK but he stayed on
the mage, dead to one swing (35,968 + 8,361 melee) at 1:10.7. Bots on Hodir run `neglect threat`. One
pull, not picked.

**Stop rules.** Not polished: the cache shattered on all three. Without the cache, window 11 would
qualify (kills on all three, 0/0/1 deaths). Pace, picked in rounds 3 and 5, is picked a third time
below (round 4 had no pulls to pick from) and the gap is still there, so the stop rule holds this
round to no change; option (c) below lifts it for one more round.

**Round 5 verdict: held for the breakers, in part for the hold.**
- `--blocks` ranged on ice (w10 → w11): walked by the position node 19-23% → 8/5/6%; moving 44-46% →
  32/33/30%; over 35 yd from the block 22-27% → 18/21/17% (target under ~10: missed); dealt per second
  5.1-6.1k → 7.0/6.8/8.0k; bot-seconds 486-552 → 481/488/381; next fire after a later wave's spawn mean
  17.8 → 15.5 s; `hodir.stand block` 110-149 notes a pull. Breakers within 15 yd of him 24-38% →
  17/20/42% of their ice samples; Ice Shards 9-17 → 3/8/12 hits.
- `--hold`: melee within 10 yd inside the held fire 72-81% → 87/82/90%, but he still stood over 5 yd
  off the point for 31/37/29 s of the fire windows (w10 20-30 s): the 9.5 yd tank spot did not move it.
- Pace 197.6-205.0k → 208.0-214.6k, +8.4k on the mean as predicted; no main-tank death.

**Picked: pace, sized before fixing.** Gap to 234.6k: 26.6 / 19.9 / 25.7k. Three measurements reset
what the levers are worth (all now in the findings doc):
- Storm Power, paired within a bot (on Hodir, standing, Singed 20+, same fire and lust state): ×1.72-1.93
  ranged, ×1.75-1.77 melee; on the all-states rate (6.6k a second without it) ~+4.9k a bot-second. The
  core agrees: aura 163 scales the whole crit in `SpellCriticalDamageBonus` (+135% on 1.5× / 2×).
- A fire, paired the same way with Singed capped: ranged ×1.08 (+0.9k), melee ×0.98, lust pairs ×0.88
  (n 8); Starlight ×1.30 / ×1.38. Round 5's ~20k / ~10k / 4-6k per-state rates were Storm Power and
  lust, not the fire.
- **Storm Power has no cap of 6 a cloud.** `spell_hodir_storm_power_aura::OnApply` spends a charge per
  receiver, but the pulse that empties the cloud already picked its targets and all keep the buff:
  single clouds 12 and 13 (uld2 56.8 s, 103.1 s), single pulses 9-11, all 198 applications fresh.
  Storm Power never lands on its carrier (`SPELL_ATTR1_EXCLUDE_CASTER`, AttributesEx 0x80000) or on a
  bot already holding it (`conditions` source 13 on 63711 / 65134: aura 63711 / 65134, negated), so
  neither spends a charge: 0 of 495 applications in windows 5-11 hit one, though holders stood within
  3 yd of a later carrier on 52-604 samples a pull. Clouds stay near 6 (6.3-6.5) because receivers
  walk in one pulse at a time.

| Lever, dps over 0-3:00 (k) | uld1 | uld2 | uld3 |
|---|---|---|---|
| One shared Storm Power pulse for every dps but the carrier lacking it within the 15 yd leash, 4 s after landing (replay, union of 30 s spans, every one of them buffed; at the all-states ~4.9k a bot-second, the paired ~7k in brackets) | +11.8 (+16.8) | +7.9 (+11.3) | +12.5 (+17.9) |
| the same within 20 yd (a positions change) | +16.8 (+24.0) | +12.6 (+18.0) | +14.4 (+20.6) |
| Singed never lapsing (deficit after the ramp × ~1.5k a stack) | 0.0 | 0.1 | 7.2 |
| fourth helper wave skipped (its ice against the Storm Power and Starlight it paid back before 3:00) | +4.9 | +2.5 | −4.4 |
| first-fire-window Storm Power to ranged instead of melee | ≤2.0 | ≤2.6 | ≤0.9 |
| fire placement (ranged in a fire in the first window 58/58/78% against w5's 89-91%) | ≤0.5 | ≤0.5 | ≤0.5 |
| clouds timing out with charges left | 0 | ~4 (2 clouds, 5 charges, 2 dps within 15 yd of the carrier) | 0 |
| **sum, 15 yd leash** | **~19.2** | **~17.6** | **~21.1** |
| not the strategy: south drop (first fire 20.9-21.8 s against 18.0-18.2) | ~3.5 | ~3.5 | ~3.5 |
| not the strategy: the threat death | | | ~5 |

The w5 pulls that reached 216-236k had the north drop and early ranged carriers (w5 uld1 ranged held
Storm Power 46% of their time, 52% of the first fire window); w10's Singed lapsed in all three pulls
(2.4-5.6k).

**Verdict: the levers the strategy holds do not reliably close the gap at ilvl 226.** Sums at ~4.9k a
bot-second, already with every dps in the leash buffed, are ~18-21k against 20-27k; every lever at
its ~7k ceiling at once reaches it on uld2 and uld3 with the 15 yd leash (21.0k, 26.5k), on uld1 only
at 20 yd (24.2k at 15 against 26.6k). No build this round: question 1 below.

**If (c) is chosen, the build and what window 12 should show.** Hodir only: a receiver inside the
leash starts its walk at cloud age `GATHER_MS` minus its walk time (age off `GetMaxDuration() -
GetDuration()`, pulses on whole seconds), so all land between two pulses (~3.5 s) and share the next;
past that age it walks at once as now; a bot already inside 3 yd stays. Rally, leash and the carrier
holding still unchanged. Reader: `--storm` per cloud its pulses and the dps within the leash at landing
against those buffed. Window 12: Storm Power a cloud from 6.3-6.5 to 9+, dps coverage from 35/50/38%
toward 50-60%, no cloud trickling one a pulse, kills under 3:10.

**Also seen.** In uld1 the first two mage fires sat 44-50 yd from him (median over their life), so the
first fire in the caster band came at 42 s; by the paired figures it cost little, and Singed reached 25
at 35.7 / 39.9 / 37.3 s anyway. uld3's Singed lapse: the held fire died at the 1:46 landing and a fire
17.5 yd from him burned 2:01.5-2:13 with nobody in it (ranged on wave 3 ice or in Starlight).

**Questions (user).**
1. *Hard mode at ilvl 226.* (a) Accept Hodir without the Rare Cache on the 226 rosters (the user
   already expects no Algalon or Vezax kill there) and judge it on kills and deaths: window 11 then
   counts as polished and the 10-man check follows; (b) keep chasing the cache on 226; (c) lift the
   stop rule for one more round for the shared Storm Power pulse (the biggest lever; every kill gets
   faster either way), judged by Storm Power a cloud and kill time, then (a) if window 12 still
   shatters; (d) raise the Hodir roster gear. *Recommended: (c).*
2. *Only with (c): widen `_STORM_CLOUD_COLLECT_LEASH` 15 → 20?* A positions change, ~+2-5k more.
   *Recommended: not in the same round*; judge the shared pulse at 15 first.

**Review (round 6).** The no-cap reading holds (`spell_hodir_storm_power_aura::OnApply` spends per
receiver; `pulses.py` reproduces the single clouds and pulses). Fixed:
- The carrier never gets its own Storm Power (0 of 495 applications), but the one-pulse replay
  (`cloudsim.py`) counted it as a receiver: the 15 yd lever fell from 8.3-14.1k to 7.9-12.5k, the
  sums from ~18-22k to ~18-21k. The verdict and question 1 stand. Its "realistic ×0.7" was the
  all-states ~4.9k a bot-second with every dps in the leash buffed, now labelled so; w10's Singed
  lapsed in all three pulls (2.4-5.6k), not two.
- The "refreshes spend nothing" evidence was the carrier, which its pulse skips; a holder is skipped
  too, by a `conditions` row, not refreshed. Findings doc corrected.
- Pace is the third pick, so the stop rule applies; (c) now says it lifts it. (a) no longer reads as
  what the user chose for Algalon and Vezax, which keep polishing.
- Left, pre-existing: `GetHodirStormPowerRally`'s comment says a holder's pulse "would be spent", but
  the pulse skips holders; the guard itself is right. Fold into the next code round.

**User answers (2026-11-22):** Q1 (c), Hodir only: one more round building the shared Storm Power pulse
(every damage dealer within the collect leash arrives together and shares one pulse), the stop rule lifted
once; if the cache still shatters after it, accept Hodir without the cache on 226. Q2: keep the 15 yd leash.
Extra pulls on round 5 (window 12b): one kept the cache (kill at 2:46.9), the others 232.2k and 180.6k.

## Round 7 (2026-11-23, on Custom b21cda8ee)

**Data.** Window 12b, extra pulls on round 5 code (no Hodir change since), `raidtest.py report --boss
hodir --raid uld --head ebe746ad1 --pulls 10 --runs
tr-20261122-204654-22,tr-20261122-204654-23,tr-20261122-204654-24`. Scratch scripts in the session
scratchpad `hodir-r7/` (role split, per-cloud leash and pulses, casts in flight at landing).

| Pool | Run | Trace | Out | Cache | Deaths | Pace 0-3:00 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261122-204654-22` | `603_22_hodir_1795380476` | kill 2:46.9 | **kept** | 0 | 251.8k |
| uld2 | `tr-20261122-204654-23` | `603_24_hodir_1795380478` | kill 3:44.5 | shattered 3:00.1 | 1: ranged Ice Shards 3:03.0 (2 Biting Cold stacks, after the timer) | 180.6k |
| uld3 | `tr-20261122-204654-24` | `603_23_hodir_1795380477` | kill 3:01.7 | shattered 3:00.0 | 1: off-tank (bear) Frozen Blows swing 2:51.7, in window 3, Barkskin up | 232.2k |

**Stop rules.** Not polished: one pool kept the cache. Pace is picked a fourth time; the user lifted
the stop rule for this one round (option (c), Hodir only), so the shared Storm Power pulse is built.

**Round 5 verdict, more pulls.** Breakers held: ranged on ice walked by the position node 9/6/5%,
moving 28/29/31%, over 35 yd from the block 15/14/19% (w11 17-21%), 310/500/378 bot-seconds. The hold
did not move: Hodir over 5 yd off the point for 35/47/65 s of the fire windows (w11 29-37). No main-tank
death.

**What the cache pull did that the 180.6k one did not** (snapshot `dealt` on him by role, uld1 → uld2):
ranged 113.1k → 67.3k, melee 104.6k → 95.2k, so 46k of the 71k gap is the ranged. Ranged held Storm
Power 47% → 22% of their time, stood in a fire 37% → 19%, and had both at once 49% → 4% of the first
fire window (fire 85% → 44% there); ice 310 → 500 bot-seconds; Ice Shards 3 → 21 hits. Clouds: 66
Storm Power (7.3 a cloud, ranged 26) against 45 (5.6, ranged 14). uld2's first cloud sat 30 s on the
off-tank bear (3 Storm Power) and its second on a melee among melee; 2 of its 8 went to a tank. uld1's
second landed on a healer among ranged in the first fire and buffed six within 2 s. He picks the
carrier, so the pull swings on luck the shared pulse should flatten.

**The gather's room.** Per cloud (`--storm`, new): damage dealers lacking Storm Power within 15 yd of
the carrier at landing 45/56/71, of whom that cloud buffed 27/33/42 (60/59/59%). Collect walked them
in the moment it landed, so the first pulse or two took 4-9 and emptied it (uld2 2:52.5: 16 in the
leash, 8 at 1.0 s, cloud gone; uld3 2:04.1: 17, 6 by 2.0 s). Storm Power on pulse 4: 3/0/6 of
66/45/52. Of the 170 leash dealers, 27 ranged had a cast in flight at landing; it would have made one
late for pulse 4.

**Built** (`UldEncounter_Hodir.h/.cpp`, `UldTriggers_Hodir.cpp`, `UldActions_Hodir.cpp`,
`UldMultipliers_Hodir.h/.cpp`, `UldDefinition_Hodir.cpp`; reader `hodir.py --storm`, test):
- `GetHodirStormGather` (replaces the triggers' `GetHodirStormPowerRally`): a receiver (damage dealer,
  no Storm Power, not the carrier, within the 15 yd leash of the lowest-guid carrier's rally) gets
  `msToPulse`, the time to pulse 4 (`ULDUAR_HODIR_STORM_GATHER_TICK`) off the cloud's `EFFECT_0` tick
  number and periodic timer, and `msToWalk`, that less its walk to park 2.0 at run speed less
  `_GATHER_LEAD_MS` 600. The collect trigger fires only once `msToWalk` ≤ 0, so all arrive in the
  second before pulse 4; past it they walk at once. Probe `hodir.gather`: `none`, `wait`, `walk`, `in`.
- `_COLLECT_RELEASE` 4.0 → 2.5, `static_assert`ed between park and the 3 yd pulse: at 4.0 a receiver
  3-4 yd out never walked in. The position node stands down inside release (was park).
- `HodirStormGatherCastHoldMultiplier`: a receiver still to walk starts no cast-time spell that would
  end past its due time + 250 ms, since a bot mid-cast runs no action until it ends. Channels go out
  and the due walk breaks them (`BreakCastPinningTheFeet`). No hold while rooted or 1.5 s past the pulse.
- The due walk clears a pending `MOVEMENT_COMBAT` walk to elsewhere (position node, shed) whose lock
  refused it; a `MOVEMENT_FORCED` dodge keeps its lock; the freeze run's `OwnMovement` blocks collect.
- `hodir storm gather hold`: from due to 1.5 s past pulse 4, a receiver inside release is blocked from
  `Reach` and `SetBehind`, which pulled melee parked out of his reach back to him before the pulse.
- Leash, rally, carrier pick and the carrier holding still unchanged. 10 and 25 alike (4 / 6 charges,
  same 1 s pulse), hard mode is only the timer; a player carrier standing still is gathered on the same
  way, and no branch reads a player.
- Reader: `STORM_COLLECT_RELEASE_YD` 2.5, so "parked at the rally" re-scores window 12b at 86/68/47%
  inside the pulse (60/71/35% at 4.0).

**Next window should show** (`--storm`):
- damage dealers in the leash at landing buffed by their cloud 85%+ (59-60%); Storm Power a cloud 9+
  (5.6-7.3); most Storm Power on pulse 4 (0-6); a cloud's first pulse holding only bots that were
  inside at landing;
- ranged Storm Power share 50%+ on every pool (47/22/37%), pace 0-3:00 above 234.6k on two pools;
- `hodir.gather` turning `walk` 1.5-3.4 s after a landing, and the receivers reaching park before 4.0 s;
- watch: Ice Shards hits and dodges among the gathered clump (10+ bots in 3 yd for about a second),
  melee time out of reach, `hodir storm gather cast hold` vetoes on ranged.
If the cache still shatters on 226, the user's answer stands: accept Hodir without it and judge it on
kills and deaths (window 11 and 12b qualify on those).

**Also seen.** Tank carriers: the bear carried two of uld2's clouds and holds no rally (the spread
trigger skips tanks), so receivers gather on where it stood; the gather worked when it stayed (w12b
uld3 1:15.6, seven on two pulses) and gave 2 in 30 s when a Flash Freeze began 1.3 s after it landed
(uld2 1:35.7, 9 in the leash, none buffed). uld3's off-tank death came after 5 swings (136.6k) in
window 2 and 4 in window 3, Barkskin up: one off-tank death in 10 windows here.

**Review (round 7).** The cause holds: of the Storm Power landing on pulses 1-3, 10/22/22 of 31/33/29
went to bots over 3 yd from the carrier at landing, so walked in (scratchpad `hodir-r7-review/early.py`;
uld2 2:52.5's eight stood 3.0-6.9 yd out). Clouds a clump empties on pulse 1 stay out of reach (uld1
2:13.3: nine within 1 yd), but had uld1's other eight survived to pulse 4 they would have buffed ~44
of its 45 leash dealers, so 85%+ is reachable. No player branch, 10 and 25 alike, only Hodir's files. Fixed:
- The cast hold stayed on while he cast Flash Freeze, whose run's `OwnMovement` blocks the walk, so a
  receiver sheltering inside the leash was held off cast-time spells up to ~5.5 s for nothing. 5 of 25
  clouds landed in a freeze cast (uld1 0:54.0, 0:55.1, 1:38.2; uld2 0:51.3; uld3 0:56.8). No hold
  during the cast now.
- The multiplier resolved and timed every spell before reading the slack; it now reads the slack
  (cached per ms) first, so a bot not waiting on a gather skips `CalcCastTime` per action.
Left, for the next window's `--storm` rows: during the wait the position node and shed now run for a
receiver outside release (collect pre-empted them from landing), and one walked past 15 yd drops out;
a cloud drifters (healers, tanks) empty before pulse 4 buffs no waiting receiver. The due walk also
clears a shed hop's jump lock, so `DoMovePoint` ends the hop mid-air as a dodge does, without the
`hodir.shuttle cut` note.

## Round 8 (2026-11-23, on Custom c350347ff)

**Data.** Window 14 (build `1fa3c332c`: round 7 plus the shared `PickDefensive` fix), `raidtest.py
report --boss hodir --raid uld --head 1fa3c332c --pulls 10`; windows 10, 11 and 12b re-read for the
cloud timing. Scratch scripts in the session scratchpad `hodir-r8/`: `slices.py` (per slice rates and
states), `deficit.py` (damage on him by role, Storm Power, ice), `gather_detail.py` / `classify.py`
(each leash receiver's fate), `shamans.py` (clouds per freeze cycle by shaman).

| Pool | Run | Trace | Out | Cache | Deaths | Pace 0-3:00 |
|---|---|---|---|---|---|---|
| uld1 | `tr-20261123-101943-13` | `603_13_hodir_1795429235` | kill 3:16.3 | shattered 2:59.97 | 0 | 212.3k |
| uld2 | `tr-20261123-102023-15` | `603_15_hodir_1795429265` | kill 2:52.7 | **kept** | 1: off-tank (bear) Frozen Blows swing 2:52.6, window 3, Barkskin up | 243.4k |
| uld3 | `tr-20261123-101943-14` | `603_14_hodir_1795429230` | kill 3:08.6 | shattered 2:59.99 | 0 | 223.2k |

**Stop rules.** The cache still shatters after round 7 (two pools of three), so the user's answer to
round 6 applies: Hodir is judged on kills and deaths at 226, and window 14 qualifies (three kills, 0/1/0
deaths), as windows 11 and 12b did. **Polished at 25n; the 10-man check follows.** Pace was also
picked in rounds 3, 5, 6 and 7, the lift was for round 7 only, so no pace change is built. The workflow
asked for a full round, so the analysis below is complete; its top cause is a plan change (question 1).

**Round 7 verdict: the timing works, the leash does not hold.** Against its checks (w12b → w14):
- Storm Power on pulse 4 9 of 163 → 44 of 192 (20/17/7): moved, but not most of it. Wherever the
  receivers stayed in the leash pulse 4 took them all: uld1 2:08.7 seven, 2:14.8 six, 2:56.3 seven;
  uld3 1:45.2 six. `hodir.gather` turned `walk` 1.6-3.3 s after landing, `in` at 3.4-3.8 s.
- Leash dealers buffed by their cloud 59-60% → 59/70/58% (target 85%+); Storm Power a cloud
  5.6-7.3 → 6.9/6.0/6.3 (9+); ranged Storm Power share 47/22/37% → 25/35/34% (50%+); pace above
  234.6k on one pool, not two. Missed.
- Per leash receiver at landing (143 over 0-3:00): pulse 1-3 45, pulse 4 33, pulse 5+ 13, from the
  other cloud 24, none 28. Of the 28: 11 walked out of the 15 yd leash while waiting
  (`GetHodirStormGather` tests it live, `UldEncounter_Hodir.cpp:447`; position node, shed, melee
  chasing him; uld1 2:56.3: five ranged at 12.9-13.8 yd went `none` 0.4-1.4 s in), 7 stood round the
  other of two carriers (`GetHodirStormCloudCarrier` takes the lowest guid, `:400`), 6 had the walk cut
  by icicle dodges 3-5 times, 4 lost a cloud emptied on pulses 1-3 by bots inside at landing.
- Watch items: cast-hold vetoes 26/11/15 a pull; Ice Shards 9/6/3 hits; no ranged or healer death.

**Round 2's mechanic:** one off-tank Frozen Blows death in 9 windows (uld2 2:52.6, Barkskin up), as
before; option (b) stays unbuilt.

**Where uld1 and uld3 lost the seconds.** Level with uld2 at the second landing: bots' damage on him to
1:46.3 22.3 / 21.9 / 22.3 M (uld1/uld2/uld3). Boss dps per slice, first fire window / cycle 2 / cycle 3
to the 2:35 landing / after it: uld1 303/238/207/156k, uld2 273/252/276/279k, uld3 302/239/242/175k.
From 1:46.3 to uld2's kill the bots dealt 11.6 / 18.0 / 13.6 M on him (uld1 −6.4 M, ranged −3.1 and
melee −3.3; uld3 −4.4 M, ranged −1.5 and melee −2.9):
- Storm Power ~2.1-2.3 M: damage dealers held it 576 / 809 / 556 bot-seconds there, worth ~9k a
  bot-second (on him with it 15-19k, without 8k). Clouds landing then: uld1 2, uld2 4, uld3 2 (both on
  ranged carriers away from the melee: 8 Storm Power of 12 charges, both timed out).
- Ice ~1.3 / 0.6 M: ranged on ice 220 / 68 / 143 bot-seconds. The last wave landed at 17.4 / 11.4 /
  14.7%; uld2 was under `_LAST_WAVE_HEALTH_PCT` 12 (`:1326`) and broke none, uld1 and uld3 spent 93 / 53
  ranged bot-seconds on it before 3:00, ~3.3 / 2.4 s.
- The rest is melee uptime: in uld1 a mage dropped a fire 0.5 s after the 2:35 landing, the hold moved
  to it and the Frozen Blows swap followed, so Hodir walked 90 yd from the landing to 3:00 and melee
  stood within 8 yd of him 36% against 55%; uld2's melee stood in Starlight 39% of cycle 3, uld1's 1%.

**Top cause: the second Storm Cloud of a freeze cycle.** A freed shaman casts Storm Cloud 1 s after
`ScheduleAbilities`, then every 30 s until the next freeze traps it (`boss_hodir.cpp:956`, `:1010`);
its first cloud lands ~3.5 s after its block breaks. Over 27 freeze cycles in windows 10-14 a shaman's
cloud repeated in the cycle every time its first landed within 15.5 s of the blocks spawning (11 of 11)
and never from 16.4 s (0 of 41). Seats are mage ×5, mage ×5, druid, druid, shaman, shaman, priest,
priest under a budget of 8, then 3 once no mage is iced (`GetHodirAssignedHelperBlock`, `:1383-1397`),
so the first shaman gets one breaker once a mage is free (5-14 s in, `hodir.dpstarget`), the second
once a druid is, and they broke 10-24 s in. First clouds after the blocks, freeze cycles 1 and 2: uld2
+15.4/+17.5 and +14.3/+15.2 (three repeats), uld3 +15.3/+21.4 and +16.4/+17.4 (one), uld1 +20.3/+22.3
and +21.4/+27.5 (none). The block order and breaker counts are a plan change: question 1.

**Not built** (stop rules), ready for a round if the user lifts them: two bugs in round 7's gather
and the last-wave threshold.
- Latch the receiver set at landing (within 15 yd) instead of testing the leash live, and stand the
  position node down for a latched receiver until its walk is due (11 misses).
- Gather each receiver on its nearest carrier, latched, instead of the lowest guid (7 misses; the two
  shamans' clouds landed within a second of each other in 7 of 13 pairs).
- `_LAST_WAVE_HEALTH_PCT` 12 was sized at the 234.6k cache pace (21.6 s); with nobody on ice the raid
  ran 279-291k after the last landing (w14 uld2, w12b uld1), so 12% is ~18 s while the last fire's
  Singed covers 25: about 17%.

**Next window should show** (only if question 1 is built): first clouds within 15 s of the blocks in
most freeze cycles and four clouds a freeze cycle (now 2-3); damage dealers' Storm Power from the second
landing to 2:52 near uld2's 809 bot-seconds; ranged on ice per wave up no more than ~20 bot-seconds, the
second mage still free within ~17 s (Singed lapses 25 s after the landing); pace above 234.6k on two
pools.

**Questions (user).**
1. *Plan, only if the cache is still wanted at ilvl 226 (it was accepted without it):* free the helper
   shamans with the mages. Order the helper blocks mage, shaman, druid, priest, give a shaman block 3
   breakers, and keep the budget of 8 until both mages and both shamans are free, then 3 for druids and
   priests. Worth up to ~3 more clouds before 3:00 a pull, roughly +10-25k over 0-3:00 against gaps of
   11-22k; costs ~10-20 ranged bot-seconds more on ice a wave, and Starlight later: the druid blocks
   wait for the second mage instead of the first (~4 s median), while the second mage keeps its five
   breakers. *Recommended: yes*, one round with the stop rule lifted, built with the two gather bugs
   and judged by clouds per freeze cycle and kill time; else Hodir stays polished.

**Review (round 8).** The cause holds: the lane's `shamans.py` over the 12 pulls of windows 10, 11,
12b and 14 reproduces 11 of 11 repeats from first clouds within 15.5 s, 0 of 41 from 16.4 s, and each
pool's first-cloud times; blocks (32938) spawn 9.9-10.0 s after the Flash Freeze cast, as it assumes;
`pulses.py` gives pulse 4 20/17/7 of 69/66/57. Seats and line refs match the code. No code changed, so
no size, mode or player branch to check. Fixed:
- Question 1's cost named the second mage's fire, but its five seats sort ahead of any shaman either
  way; what waits is the druid blocks. Second mage freed 0.2-19.8 s after the first (median ~4 s, last
  hits, scratchpad `hodir-r8-review/magegap.py`).
- Findings doc: the 7.6-9.9% last wave read as current; it was the two pulls that set the skip,
  window 14's landed at 11.4-17.4%.

**User answer (2026-11-23):** Q1 yes, free the helper shamans together with the mages: one more round
with the stop rule lifted to chase the cache (Hodir stays polished at 25 without it), built with
the two gather bugs; `_LAST_WAVE_HEALTH_PCT` stays.

## Round 9 (2026-11-23, on Custom 523b2f60f)

**Data.** No new pulls: window 15 is pulling Hodir at 10-man as the 10-man check, not awaited. Stop
rules, verdict and ranking skipped (the user's answer above). Deaths by mechanic per pull, window 14
as round 8: uld1 0, uld2 1 (off-tank Frozen Blows swing, window 3, Barkskin up), uld3 0. Windows 10,
11, 12b and 14 (12 pulls; window 13 had no Hodir) re-read for the replay below, scratchpad
`hodir-r9/` (`extract.py`, `calib.py`, `replay.py`, `pace.py`, `firsts.py`).

**Built** (`UldEncounter_Hodir.h/.cpp`, `UldTriggers_Hodir.cpp`; reader `hodir.py`, tests):
- Block order mage, shaman, druid, priest (`HodirHelperKind`); `ULDUAR_HODIR_SHAMAN_BLOCK_BREAKERS`
  3; the budget of 8 holds while a mage or shaman block is up, then `_LATE_BLOCK_BREAKERS` 3. Seats:
  mage ×5 + mage ×3, then (first mage free) mage ×5 + shaman ×3 (`static_assert`ed within 8), then
  shaman ×3 + shaman ×3 + druid + druid, then druid, druid, priest.
- Receivers latched at landing: the first `GetHodirStormGather` call to see a new cloud (carrier and
  apply time, `stormLanding`) latches every damage dealer within the 15 yd leash of its rally on it,
  each on the nearest of the clouds new in that pass (`LatchHodirStormReceivers`). A dealer still
  lacking Storm Power keeps its live latch; one holding it is latched too and walks in if it lapses
  during that cloud. The gather reads the latch (same carrier alive, same apply time), with no live
  leash test; `GetHodirStormCloudCarrier` (lowest guid) is gone. A carrier is never a receiver.
- `HodirRaidPositionTrigger` stands down for any latched receiver (wait, walk, in), not only inside
  release. New probe `hodir.receiver`: the carrier, or `none`.
- Reader: `--storm` adds what each leash dealer at landing got (pulse 1-3, 4, 5+, other cloud, none)
  and the clouds per freeze cycle by shaman, as seconds after the blocks spawned; parked samples
  follow `hodir.receiver` (lowest guid on older traces). `--blocks` adds time on ice after the last
  mage and shaman were free.
- 10/25, modes, player: 10-man has one helper of each kind and a budget of 1 (3 ranged less 2), so the
  shaman now goes before the druid there too; hard mode is only the timer. Players are latched like
  anyone and a player carrier gathers the same way; block seats stay bots only, as before.

**Replay of the new order.**
- *Melt Ice* (new fact): a freed mage casts `64528` (aura 89, 10% of the block a second for 10 s) on
  every helper block lacking it, one every 2 s when its own casts allow: the 61 blocks no bot was
  seated on died a median 9.5 s after the first mage was free (most 4-14 s, a few up to 33).
  Breakers shorten that.
- *Model:* each non-mage block loses its recorded health, less what its recorded seats dealt (a seat
  counts 1.5 s after it is given, at the pull's ranged rate on ice, 5.1-8.0k a second), plus what
  the replayed order's seats deal; past its recorded death the rest keeps its last rate. Mage blocks
  keep their recorded deaths (same seats in both orders). A first cloud lands 3.5 s after its shaman
  block dies (3.1-4.3 over 66 pairs) and repeats 30.0-31.4 s later, only when that lands over ~1 s
  before the trap (margin 0.3-10.1 s with a repeat, up to +0.9 without). The same model of the
  current order gives 10.0 clouds before 3:00 a pull against 9.1 recorded, so it runs a little
  early; the estimate below is model against model.
- *Result,* freeze cycles 1-2: shaman blocks freed median +12.1 s after the blocks spawn (model of the
  current order +15.1, recorded +16.2); first clouds median +15.6 s, 60% within 16 s (recorded 23%);
  clouds per freeze cycle 3.5 / 3.0 (recorded 2.5 / 2.2, model 2.8 / 2.3), the pull's cycle 4.0
  (3.5); clouds before 3:00 11-14 a pull (recorded 7-11, model 8-12), +2.1 a pull. Not four a cycle:
  the second shaman block gets its three only once the second mage is free (median 9.6 s in, 5.8-19.2),
  so ~40% of first clouds still land past 16 s.
- *Pace,* 0-3:00, at the recorded 4.1-6.1 damage dealers a cloud buffed and 4.9-7k a Storm Power
  bot-second: Storm Power +6-11k, ice −3.6-4.8k (+36-85 breaker bot-seconds a pull at 9.7-15.3k
  on Hodir), Starlight −0.5k (druid blocks freed median +18.4 s against +15.6). Net +3 to +6k (seat
  delay 1.0-2.5 s: +1 to +7k); per pull −2.8 to +14k, the two pulls that already had four clouds a
  cycle (w11 uld2, w14 uld2) losing ~2.5k to the ice.
- *The gather fixes on top:* 18 of window 14's 143 leash receivers missed by walking out of the leash
  (11) or standing round the other carrier (7), ~6 a pull at ~25 s each: +4-6k.
- Together roughly +5-12k against window 14's gaps of 22.3k (uld1) and 11.4k (uld3): the cache on
  the pools already pacing near it, not on all three.

**Next window should show** (`--storm`, `--blocks`):
- clouds per freeze cycle 3+ in cycles 1-2 (window 14 mean ~2.5), most first clouds within 16 s of
  the blocks;
- shaman blocks dying before the druid blocks, the second mage freed no later than now (window 14
  cycles 1-2: 6.3-10.7 s);
- `what they got`: `none` well under window 14's 23 / 6 / 5 (34 of 152 over the fight), and no
  receiver dropping to `hodir.receiver none` while its cloud lives and it lacks Storm Power;
- `hodir.receiver` naming the nearer carrier when two clouds land together;
- ranged on ice up ~15-25 bot-seconds a wave (window 14: 276-435 a pull), druid blocks ~3 s later;
- pace 0-3:00 above 234.6k on the pools pacing 223k+.
- Watch: a latched receiver walking back 15+ yd to a rally after a shelter run, the position node
  standing down ~3.5 s on a waiting ranged receiver, cast-hold vetoes (26 / 11 / 15 a pull).

**Also seen.** If the next window still shows ~3 clouds a cycle, the remaining lever is the second
shaman block ahead of the second mage's last two breakers, which pushes the second fire back (Singed
lapses 25 s after a landing); not proposed until this round is judged.
