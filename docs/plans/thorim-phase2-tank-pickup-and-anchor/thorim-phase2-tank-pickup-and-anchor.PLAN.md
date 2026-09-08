# Thorim phase 2: pick the boss up, and keep him on the anchor

Traces read with `python tools/botobs/postmortem.py <file>` from `modules/mod-playerbots`;
files in `env/dist/logs/botobs/`. **Timestamps are milliseconds.**

| file | outcome | phase 1 ended | Sif damage |
|---|---|---|---|
| `603_1_thorim_1788892411.ndjson` | wipe 4:45.9, 31 dead | 2:53.575 | 1,041,391 |
| `603_1_thorim_1788892833.ndjson` | wipe ~4:07, 30 dead | 2:54.324 | 520,008 |
| `603_1_thorim_1788557437.ndjson` (baseline) | **kill** 6:31, 3 dead | 3:25.306 | **0** |

Build carries `ca240b3da` (the west tank spot and the Mimiron disperse reset) and `68572b75c`.

---

## Context

### Both of today's pulls were hard mode. The 6:31 "kill" everyone compares against was not.

Sif leaves at the phase change unless the gauntlet reaches Thorim inside three minutes. Today
phase 1 ended at **2:53.6 and 2:54.3** - the race was won, Sif stayed for the whole fight and dealt
520k-1.04M. In the 6:31 kill she vanished at 3:25.0 and dealt **zero**. Any metric compared across
those two is comparing hard mode to normal mode, so this plan compares today against yesterday's
hard-mode wipes where it can.

### Phase 1 is not the problem, and the arena squad is fine

Deaths by phase: **1 and 0 in phase 1, 30 and 30 in phase 2.** Thorim killed 20 and 15, Sif 8 and
12, adds 2 and 3.

Arena squad throughput is flat against the baseline: **47,896/s and 52,739/s against 53,317/s**.
Damage per add killed is the same too (173k vs 166k). What changed is the clock - winning the
hard-mode race ended phase 1 **31 s early**, so fewer waves died:

| | kill run | today |
|---|---|---|
| adds killed in phase 1 | 66 | 48 / 44 |
| kills per minute | 19.3 | 16.6 / 15.1 |
| still alive at the phase change | 45 | 53 / 51 |

So the adds the pull ends with are the price of winning the race, not a DPS regression. Worth
knowing separately: arena adds are **never** tanked - 6-8% of them target a tank today, 0% in the
kill run - so they free-roam onto the camp, and 39%/32% of phase 1 add damage lands on ranged. That
is chronic and costs almost no lives, so it is out of scope here.

### `ca240b3da` verified: the disperse leak is fixed

Moves owned by `combat formation move`: **1149 yesterday (420 accepted), 0 today.** Phase 2 ranged
travel fell from 138 yd/min to **54 and 62** (kill run 26), healers 116 to 55. That half of last
night's change is done.

### Why phase 2 kills the raid: nothing ever picks Thorim up

Three defects, each independently confirmed.

**1. The main tank cannot acquire the boss - it is a deadlock.**

`ThorimDpsTargetAllowed` (`UldEncounter_Thorim.cpp:759`) returns `true` for everything once the boss
is on the floor; the guard only exists to stop bots hitting him on the balcony in phase 1. So the
tank branch of `ThorimDpsTargetTrigger` (`UldTriggers_Thorim.cpp:55`), which reads
`currentTarget && !allowed`, is **always false in phase 2** - no node ever puts a tank on Thorim.
`ThorimDisableAutomaticTargetingMultiplier` says so in its own comment
(`UldMultipliers_Thorim.cpp:108`): *"the encounter has no tank-targeting node to put in its place"*.

Tanks are left to generic `tank assist`, which in an arena holding 50 live adds keeps them on a Dark
Rune Warbringer. Bulwark's target was a Warbringer from 2:52 to 3:12, a Champion to 3:24, and only
then Thorim.

And `ThorimPhase2PositioningTrigger`'s MainTank branch (`UldTriggers_Thorim.cpp:170`) requires
`boss->GetVictim() == bot` - he must **already** hold the boss to be told to walk him to the anchor.
Bulwark's phase 2 positioning scored **0 OK in both attempts**.

Cost: **15.3 s and 9.6 s** before any tank held the boss, 3 deaths inside each vacuum, and Thorim
spent **29% of phase 2 on a non-tank**, taking 64% and 73% of his phase 2 damage output there.
Unbalancing Strike landed on Angry, Deathsong, Trueshot and Malediction - the last for 64,960 on a
ranged bot.

This is chronic, not new: the kill run had a 20.2 s vacuum and was 49% untanked. It only kills now
because Sif is alive through it.

**2. The Blizzard dodge drags the boss across the arena.**

`ThorimSifBlizzardAction` is a plain `MoveAwayFromCreatureAction` at `ACTION_RAID + 3`, and
`ThorimPhase2PositioningTrigger` explicitly stands down while it is active - so it outranks the
anchor and knows nothing about who is holding the boss. It is gated on `IsThorimHardModeActive`,
which is a config flag (`AC_AI_PLAYERBOT_ULDUAR_THORIM_HARD_MODE=1`, confirmed in the container).

Last night's tank spot was moved west onto the Blizzard bunny's path deliberately - eating Blizzard
was accepted - but the code does not accept it. From the trace: Bulwark held Thorim parked at
(2117.7, -257.2) from 3:30 to 4:00, then Blizzard fired and he was sent to (2132.0, -231.4) at
4:04.6, (2159.9, -233.4) at 4:09.0, (2101.7, -261.7) at 4:11.1. **Thorim followed every step.**

The damage this dodge is avoiding is trivial: Blizzard did **36,283 and 32,799** to the two tanks
across a whole attempt, against 11k-38k from a single Thorim melee swing.

**3. The off-tank has no absolute anchor, so the boss drifts whenever he holds it.**

The main tank's spot is the absolute `ULDUAR_THORIM_PHASE2_TANK_SPOT`. The off-tank's is
`RingPoint(bot, boss, bearing)` - 8 yd off the boss's **live** position
(`UldEncounter_Thorim.cpp:1752`). So when the off-tank has the boss he walks to a point measured
from the boss, the boss follows him, the point moves, and the pair drifts with nothing pulling them
back. The Unbalancing Strike swap itself is correct and fires once per pull as designed; it is what
happens after the swap that has no anchor.

**Net effect.** Thorim walked **256 yd and 92 yd in phase 2 (137 and 76 yd/min) against 39 yd
(12.5 yd/min) in the kill run**, sitting a median 21.3 and 29.9 yd off the tank spot. That also
destroys what last night's camp was for: melee-to-nearest-ranged came out at a median of **12.6 and
7.9 yd**, not the 15+ the anchor geometry gives, and Chain Lightning was 8.1% of phase 2 incoming in
the first attempt but 25.3% in the second. The wider camp only pays off if the boss is anchored -
untanked, the extra distance just means Thorim walks through the camp to reach anyone.

---

## The change

Five edits, all in `src/Ai/Raid/Uld/`. No coordinates change.

### 1. A tank-pickup node

New `ThorimTankPickupTrigger` / `ThorimTankPickupAction` pair, following the layout of the existing
`ThorimUnbalancingStrikeSwap*` pair.

- **Trigger.** Active when `ThorimPhase2Active(botAI)`, the boss is alive and hostile, and
  `boss->GetVictim() != bot`. Returns false when the victim is **any** tank - a tank already has
  him and the swap node owns the trade, which is what stops the two of them ping-ponging. The main
  tank picks up whenever it is free; the first assist tank picks up only when the main tank is dead
  or absent (`PlayerbotAI::GetMainTankGuid`), so only one bot is ever taunting.
- **Action.** Same body as `ThorimUnbalancingStrikeSwapAction::Execute`
  (`UldActions_Thorim.cpp:421`): `Attack(boss)` if the boss is not the current target, else
  `DoSpecificAction("taunt spell", event, true)`. Extract that body into a shared helper in
  `UldEncounter_Thorim.cpp` and have both actions call it rather than duplicating it -
  `"taunt spell"` is registered for all four tank specs.
- **Wiring.** `UldStrategy.cpp` beside the other Thorim nodes at `ACTION_RAID + 2` (matching the
  swap), plus creators in `UldActionContext.h` and `UldTriggerContext.h`.

### 2. Stop `tank assist` fighting the pickup

`ThorimDisableAutomaticTargetingMultiplier` (`UldMultipliers_Thorim.cpp:103`) currently exempts
`TankAssistAction` and every tank, and its comment gives the reason: there was no tank-targeting
node. There is one now. Bring `TankAssistAction` into scope and drop the tank exemption **only while
`ThorimPhase2Active`**, so phase 1 arena tanking is untouched. Rewrite the two comment blocks that
state the old reasoning.

### 3. Give the tank a picker again once `tank assist` is muted

Muting `tank assist` leaves a hole: `ThorimDpsPriorityTrigger`'s tank branch
(`UldTriggers_Thorim.cpp:55`) short-circuits on `ThorimDpsTargetAllowed`, which is always true in
phase 2, so a tank that is not currently picking the boss up has **no** target node at all and sits
on whatever add it held when the boss dropped. Gate that short-circuit on `!ThorimPhase2Active` so
the encounter's own picker steers tanks in phase 2 like everybody else - it already returns the boss
and nothing else there. With `tank assist` zeroed there is still only one live picker per bot, which
is what that branch existed to guarantee.

### 4. Whoever holds Thorim eats the Blizzard

`ThorimSifBlizzardTrigger::IsActive` (`UldTriggers_Thorim.cpp:302`): return false when
`GetThorim(botAI)->GetVictim() == bot`. A tank without the boss still dodges. With the trigger off,
`ThorimPhase2PositioningTrigger` stops standing down and the anchor holds.

### 5. Whoever holds Thorim owns the anchor

`TryGetThorimPhase2Spot` (`UldEncounter_Thorim.cpp:1752`), OffTank branch: when
`boss->GetVictim() == bot`, hand back `ULDUAR_THORIM_PHASE2_TANK_SPOT` instead of the boss-relative
ring point. `ThorimPhase2PositioningTrigger` needs the matching change - the off-tank holding the
boss takes the MainTank branch's `bot->GetDistance(spot) > 1.0f` test rather than
`ThorimRingNeedsMove`, since the ring tolerance is meant for a spot that orbits.

## Deliberately not doing

- **Nothing about the arena adds.** They are never tanked in any trace including the kill run, they
  killed 2 and 3 bots out of 61, and the count at the phase change is the cost of winning the
  hard-mode race. Revisit only if phase 2 stabilises and they still matter.
- **No coordinate changes.** The camp geometry from `ca240b3da` is sound; it was never given a
  stationary boss to be measured against.
- **Nothing about Sif's Frostbolt Volley.** DBC radius 200, no positional answer.
- **No second look at the ranged anchor count.** Same reasoning as last night.

## Verification

Static:

- `python apps/codestyle/codestyle-cpp.py` clean for the touched files; no line over 120 columns;
  files stay LF/ASCII.
- Per-TU syntax check in `acore/ac-wotlk-build:master` against
  `/azerothcore/build/compile_commands.json` for each changed `.cpp`: mount
  `modules/mod-playerbots/src` read-only, take the entry for the file, drop `-c` and `-o`, add
  `-fsyntax-only`, run from its `directory`. Expect only the two pre-existing
  `-Wunused-parameter` warnings in `UldEncounter_Thorim.cpp` (`:1382`, `:1788`). It does not link,
  and takes minutes - run it backgrounded.

In game, one 25-man hard-mode pull, then re-read the trace. The scratchpad scripts that produced
every number above are reusable (`vac.py`, `drift.py`, `tank.py`, `chain.py`, `kills.py`, `cfm.py`).

1. **A tank holds Thorim within ~2 s of the phase change**, against 15.3 s and 9.6 s. Binary check.
2. **Thorim walks under ~40 yd in phase 2**, against 256 and 92, and sits a median under ~5 yd from
   (2110.7, -252.7), against 21.3 and 29.9.
3. **Under 10% of his phase 2 damage lands on non-tanks**, against 64% and 73%.
4. **Melee-to-nearest-ranged median back to 15-20 yd**, against 12.6 and 7.9, with Chain Lightning
   in single digits.
5. **Only one tank taunts at a time** - no cast-by-cast alternation of `boss->GetVictim()` between
   Bulwark and Ecoterrorist outside an Unbalancing Strike swap.
6. **Blizzard damage to the holding tank stays small** - it was 36k and 33k a pull while dodging, so
   standing in it should stay well inside what the healers already cover.
7. **No regression** on: `combat formation move` still owning 0 moves; phase 1 deaths still 0-1;
   arena squad damage still near 50k/s; `thorim.squad` split still 1/2 with the gauntlet tank in
   squad 2; hard-mode race still won inside 3 minutes.
