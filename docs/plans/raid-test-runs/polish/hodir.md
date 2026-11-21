# Polish log: Hodir (25-man hard mode)

Trace slug `hodir`, raid `uld`, scenario `uld-hodir` (drop at (1998, -262), in his room ~20 yd south
of the frozen helpers, aggro start). Hard mode is a timer, not a toggle: a kill counts only if Hodir
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
