# Iron Assembly — shared state, gap-closers, and the tank/rune deadlock

## Context

Two pulls traced 2026-09-08, both 25-man hard mode
(`AC_AI_PLAYERBOT_ULDUAR_IRON_ASSEMBLY_HARD_MODE=1`), both wipes, same raid: **one bot tank**
(Bulwark) plus two human death knights.

| | file | outcome | deaths | Brundir | Molgeim |
|---|---|---|---|---|---|
| **C** | `603_1_runemaster-molgeim_1788893582.ndjson` | wipe 4:42.3 | 31 | 1:48.7 | 3:52.2 |
| **D** | `603_1_stormcaller-brundir_1788894038.ndjson` | wipe 4:18.0 | 27 | 2:02.7 | 4:03.6 |

**Yesterday's fixes (commit `68572b75c`) are confirmed working.** `ironassembly.tank` reads
`brundir` from 0:00 in both pulls with no human in the ordering. Overload covered the stack point in
**0 of 6** windows (35–41 yd away, vs 5 of 5 yesterday); members inside the circle at cast fell from
22–24 to **9–10**, all of them melee. `ironassembly.spot` never emitted `stack-overload`.

Both raids got two of three bosses down and then collapsed in the Steelbreaker-only window: C lost
**21 of 31 deaths in the last 50 s**, D wiped **14 s** into it. High Voltage 63526 took 64.5% / 63.6%
of all damage and 13/31 and 20/27 killing blows — it is `EffectRadiusIndex 28` = 50,000 yd and
unavoidable, so the lever is how long that phase lasts and how few corpses feed Electrical Charge.

Four defects below, plus a raid-policy change the user asked for.

## What the traces say

### 1. The encounter state exists six times over — the root cause of the arena wander

`ironAssemblyStates` is `thread_local` (`UldEncounter_IronAssembly.cpp:67`). `ObsValue::operator=`
emits **only when the value changes** (`src/Bot/Obs/RaidObs.h:248-266`), yet `ironassembly.alive`
emits **six identical rows per transition, from six different bots, inside 0.5 s** — in both pulls,
at all three transitions. Six writes of one value can only be six separate state objects.

Two sibling files already document this exact bug and fix it.
`UldEncounter_FlameLeviathan.cpp:72` says: *"Not thread_local. A map is updated by one thread at a
time but is never pinned to one, and **MapUpdate.Threads is 6 here**, so per-thread copies hand the
same instance a fresh state whenever the pool reassigns it — which silently resets the vent claim and
made fl.pursued flap 199 times."* `UldEncounter_Mimiron.cpp:364` says the same.

Consequences, both measured:

- **Spread slots collide and churn.** `Trueshot slot=0` at 3:52.155 and `Prayer slot=0` at 3:52.313
  — two bots on one slot, impossible with a shared map. 71 slot writes for 11 bots in 4.5 s, and
  only slots 0–10 of 16 ever used. Each thread assigns "first free" from its own map, so a bot has a
  different bearing on the 18 yd ring depending on which thread ticks it.
- **Bots never park.** In C's last phase every ranged bot has **4–6 distinct raid-position
  destinations** and **44–59 destination jumps >5 yd** in 50 s (Trueshot's four are up to 36 yd
  apart), travelling **285–334 yd**. That is the "wander around the arena".
- **Yesterday's `stackShiftHeading` latch is per-thread**, so the raid can hold up to six different
  shift headings at once. It did not bite in these pulls only because Overload never covered the
  stack.

### 2. Warriors and the feral druid charge back into Overload

`IronAssemblyMovementGuardMultiplier` early-outs on `!dynamic_cast<MovementAction*>(action)`
(`UldMultipliers_IronAssembly.cpp:57`). Charge, Intercept and both Feral Charges are
`CastReachTargetSpellAction`, which derives from `CastSpellAction`, **not** `MovementAction` — so
they pass straight through, and the *server's* spell effect translocates the bot with no
`MovementPriority` involved at all.

The trace gives a clean control group. The only three bots that cast a gap-closer during an Overload
window are the only three non-tank bots that took Overload damage:

```
pull C   10 gap-closer casts in windows:  Mighty Charge x4, Angry Intercept x4, Ecoterrorist Feral Charge x2
pull D   16 gap-closer casts in windows:  Mighty Charge x8, Angry Intercept x4, Ecoterrorist Feral Charge x4

Overload damage taken, pull D:  Mighty 68,392 (4 hits)   Bulwark 52,854 (tank, 4)   Angry 20,370 (1)
```

Mighty took **more Overload damage than the tank**. The other five melee — Justice, Shadow, Assasin,
Totemist, Obliteration — have no gap-closer and took **zero**. Angry died to Overload in C at 0:45.7
and in D at 1:09.0, in both cases mid-`reach melee` right after an Intercept.

The escape action walks them to ~16–19 yd, which is exactly Charge/Intercept's usable band, so the
dodge itself arms the ability that undoes it.

### 3. The tank and the Rune of Power drag-out deadlock, and Brundir never leaves the rune

`IronAssemblyRuneOfPowerTrigger::IsActive` fires when the tank's **assigned boss** carries Rune of
Power and is attacking that tank — i.e. exactly the case reported: a rune under Brundir while Bulwark
holds him. The drag-out (`ACTION_RAID + 2`, `MoveAway`) and the tank-spot action (`ACTION_RAID + 6`)
both move at `MOVEMENT_COMBAT`, so `IsWaitingForLastMove` makes them trade the slot:

```
0:35.271  -> (1615.2,121.0)  wait   tank assignment
0:35.271  -> (1621.9,108.0)  wait   rune of power
0:35.579  -> (1615.2,121.0)  wait   tank assignment
0:35.580  -> (1622.3,106.6)  wait   rune of power        <- drag target recedes every tick
   ... 5 s of this, drag destination drifting to (1605.2,138.4) ...
```

339 such flip-flops in C, 78 in D. **Brundir never moved**: stationary at (1614.6, 122.1) from
0:39.2, which is 1.3 yd from his designed spot. Overload then fired at 0:39.8–0:45.6 centred on that
point — Bulwark 18,968, Angry 27,500 (killed), Mighty 18,107.

The drag destination recedes because `MoveAway(boss, 10.0f)` is recomputed from the bot's live
position each tick. Rune of Power is only **5 yd** (`RUNE_OF_POWER_RADIUS`), so a fixed, deterministic
spot clears it easily — the boss buff is worth removing, the yo-yo is not.

*(Ranged were correctly excluded: 179 `ironassembly.soak = none:far` rows, the 25 yd travel cap
doing its job. Rune of Power never lands on a player — 876 aura applications, all from standing in a
rune whose carrier is a council member.)*

### 4. The Rune of Death escape picks a new compass direction every tick

`TryGetIronAssemblyEscapeSpot` (`UldActions_IronAssembly.cpp:32-49`) calls the `vector<Position>`
overload of `FindNearestPositionClearOfHazards`, which has no `preferNear`. `EncounterHelpers.h:73-75`
names the failure precisely: *"Without it the sweep takes the first angle that passes, which is a
fixed compass direction and has nothing to do with where the bot wants to end up."* Hodir and Freya
both pass it; Iron Assembly does not.

The ring recentres on the bot's current position each tick, so the chosen angle moves as the bot
walks. Measured across all bots: **21–26% of consecutive escape destinations jump >5 yd**, with up to
25 distinct destinations for a single bot, and 707 (C) / 636 (D) `raid position <-> rune of death`
flip-flops among ranged and healers.

Secondary defect in the same helper: `IronAssemblyOverloadAction` merges runes into the Overload
hazard vector (`UldActions_IronAssembly.cpp:109-112`), and the single-clearance signature then gives
runes clearance **25** instead of 21. The `HazardCircle` overload exists for exactly this.

### Checked and correct — do not touch

- Tank ranking, the Brundir-first assignment, and the Overload-aware stack displacement: all
  confirmed working this pull. Keep them.
- Hard-mode kill order, `focus`, `interrupt`, `soak` caps: all read cleanly.
- `MELEE_BOSS_RADIUS = 16`, `SPREAD_RING_RADIUS = 18`, tank-spot bearings: unchanged, per the
  geometry constraint recorded in `docs/raids/ulduar/iron-assembly.md`.
- `MovementActions.cpp` / `IsWaitingForLastMove`: engine-wide, still out of scope. Fixes 1 and 3
  remove most of the contention; re-measure before touching a shared primitive.

## Approach

### 1. One encounter state per instance, not per thread

In `UldEncounter_IronAssembly.cpp`, replace

```cpp
thread_local std::unordered_map<uint32, IronAssemblyEncounterState> ironAssemblyStates;
```

with the mutex-guarded pattern already used by Mimiron (`UldEncounter_Mimiron.cpp:364-375`) and Flame
Leviathan (`UldEncounter_FlameLeviathan.cpp:72-88`): a `std::mutex`, a plain `std::unordered_map`,
and an `IronAssemblyStateFor(Player*)` accessor holding the lock only across the lookup (references
into an `unordered_map` survive rehashing). Replace every
`ironAssemblyStates[bot->GetInstanceId()]` with a call to it.

Copy the *reasoning* from those files into the comment, not the process story — the state struct's
existing "needs no lock" comment at `:64-66` is the misconception to remove.

This alone fixes the spread-slot churn, restores `stackShiftHeading` to a real raid-wide latch, and
stops `ironassembly.alive` emitting six rows a transition.

**Scope decision (confirmed):** Iron Assembly only. `thorimStates`
(`UldEncounter_Thorim.cpp:76`), `_ignisTankArcStates`, `flKiteDirection`, Hodir's two latches and the
EoE caches have the same shape — record that in the doc as a known follow-up, change nothing.

### 2. Gap-closer guard

Add `IronAssemblyChargeGuardMultiplier` to
`src/Ai/Raid/Uld/Multiplier/UldMultipliers_IronAssembly.{h,cpp}`, copying
`MimironChargeGuardMultiplier` (`UldMultipliers_Mimiron.cpp:76-84`) — a single
`dynamic_cast<CastReachTargetSpellAction*>` catches all four spells, where the ICC and Ruby Sanctum
enumerations each miss one.

Gate it on the **existing `IronAssemblyMemberMustMove(botAI, bot)`** rather than a raw hazard window,
so a melee bot fighting Steelbreaker 40 yd from Brundir keeps its charge and only a bot actually
committed to a dodge loses it. Register beside the other two in `UldStrategy.cpp:894`.

The bot then falls through its ActionNode alternative chain to `reach melee`, which *is* a
`MovementAction` and the existing movement guard already handles.

### 3. Displace the tank spot instead of dragging

Delete `IronAssemblyRuneOfPowerAction` (the tank drag-out), its trigger, and its `UldStrategy.cpp:237`
registration. Move the responsibility into `TryGetIronAssemblyTankSpot`
(`UldEncounter_IronAssembly.cpp:496-519`): after picking the boss's designed bearing/radius, if the
boss carries `SPELL_RUNE_OF_POWER`, sweep the rune's position (same `GetDynamicObjectPositions` route
as `GatherIronAssemblyRunesOfDeath`, spell 64320) and rotate the bearing at the **same radius** to the
nearest candidate at least `ULDUAR_IRON_ASSEMBLY_TANK_RUNE_CLEARANCE` from it.

New constant: `TANK_RUNE_CLEARANCE = 12.0f` — the 5 yd rune, plus the ~5 yd the boss stops short at
when it follows the tank, plus margin.

Candidates are ±22.5°, ±45°, ±67.5°, ±90° off the designed bearing. **navprobe verified this
session**, anchor `(1587.18, 121.02, 427.27)`: the 28 yd ring is **16/16 on mesh** and the 16 yd ring
is **16/16**, every settledZ on the floor at 427.27 — so every candidate at either radius is walkable.
Displacement is `2·R·sin(θ/2)`: at R=28 a single step is 10.9 yd and two is 21.4; at R=16, 6.2 and
12.3.

One action, one destination, no contention, and the boss ends up somewhere deterministic instead of
wherever a receding `MoveAway` left it.

### 4. Stabilise the hazard escape

Change `TryGetIronAssemblyEscapeSpot` to take `std::vector<HazardCircle>` and forward a `preferNear`,
so each hazard carries its own clearance and the sweep breaks ties toward where the bot wants to be
rather than toward compass zero. Pass the bot's own raid spot as `preferNear` where it has one
(`TryGetIronAssemblyRaidSpot`), otherwise its current position. Update the three callers — Overload,
Lightning Tendrils, Rune of Death — giving runes `RUNE_OF_DEATH_CLEARANCE` (21) and Brundir
`OVERLOAD_CLEARANCE` (25) / `TENDRILS_CLEARANCE` (28) instead of one clearance for the whole vector.

### 5. Hold DPS cooldowns until Steelbreaker is alone (hard mode only)

Add `IronAssemblyHoldDpsCooldownsMultiplier` in the same multiplier files:

```
if (!IronAssemblyFormationActive(botAI) || !IsIronAssemblyHardModeActive(botAI)) return 1.0f;
if (!IsDpsCooldownAction(bot, action))                                          return 1.0f;
return IsSteelbreakerEmpowered(botAI) ? 1.0f : 0.0f;
```

`IsDpsCooldownAction` already exists (`src/Util/EncounterHelpers.cpp`, used by Black Temple at
`BTMultipliers.cpp:32`) and covers per-class burst, trinkets, racials, and shaman Bloodlust/Heroism
even on a resto shaman. `IsSteelbreakerEmpowered` is exactly "Steelbreaker alive, other two dead".

Hard mode only, because the normal order kills Steelbreaker first and the condition would never
release. The trade the comment should record: total damage is unchanged (one health pool), but
concentrating burst in phase 3 shortens the only window where Electrical Charge compounds — which is
where 21 of 31 and 20 of 27 deaths happened.

## Files

| Path | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp` | Mutex-guarded state + accessor (1); rune-aware tank spot (3); `HazardCircle` escape helper (4) |
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.h` | `TANK_RUNE_CLEARANCE`; drop the drag-distance constant; note-key inventory |
| `src/Ai/Raid/Uld/Action/UldActions_IronAssembly.{h,cpp}` | Delete the drag-out action; retarget the three escape callers (4) |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.{h,cpp}` | Delete `IronAssemblyRuneOfPowerTrigger` |
| `src/Ai/Raid/Uld/Multiplier/UldMultipliers_IronAssembly.{h,cpp}` | Charge guard (2); cooldown hold (5) |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | Drop the drag-out node at `:237`; register two multipliers at `:894` |
| `docs/raids/ulduar/iron-assembly.md` | Fold in the findings. Reachable from the module `CLAUDE.md`, so **invoke `/compact-docs-writer` up front** |

Do not touch `MELEE_BOSS_RADIUS`, `SPREAD_RING_RADIUS`, the tank-spot bearings,
`MovementActions.cpp`, or any other encounter's `thread_local` state.

## Verification

Static, here: `python apps/codestyle/codestyle-cpp.py`. **The module cannot be compiled in this
environment (no `compile_commands.json`, empty build volume) — hand the build off rather than
claiming one.**

In-game, one 25-man hard-mode pull, then `tools/botobs/postmortem.py` against the trace:

1. `--notes ironassembly.alive` shows **one** row per transition, not six. This is the single
   cheapest proof fix 1 landed.
2. `--notes ironassembly.slot` shows each bot assigned once, no two bots sharing a slot, and slots
   spread across 0–15 rather than clustering in 0–10.
3. Ranged travel in the Steelbreaker-only phase falls from 285–334 yd toward the ~40 yd a
   spread-and-hold formation needs; raid-position destination jumps >5 yd per bot fall from 44–59
   toward single digits.
4. Mighty, Angry and Ecoterrorist take **zero** Overload damage, and no gap-closer casts appear
   inside an Overload window.
5. `iron assembly rune of power action` no longer appears in the move stream at all; the
   `rune of power <-> tank assignment` flip-flops (339 / 78) go to zero; Brundir is >12 yd from any
   Rune of Power he carries.
6. Consecutive Rune of Death escape destinations jumping >5 yd falls from 21–26% into single digits,
   and `raid position <-> rune of death` flip-flops fall well below 707.
7. No DPS cooldown or Bloodlust is used before `ironassembly.alive` reaches `1`; the
   Steelbreaker-only phase is shorter than C's 50 s and costs fewer than 21 deaths.

If the raid still collapses in phase 3 once 1–7 are green, that is the Electrical Charge spiral
rather than a positioning bug, and the remaining levers are healing throughput and the first death.

## Step 0

Copy this document to
`docs/plans/iron-assembly-oscillation/iron-assembly-oscillation.PLAN.md` before starting.
`docs/plans/iron-assembly-tank-separation/` can be deleted in the same pass — its exit condition is
met (see Context), with anything durable folded into `docs/raids/ulduar/iron-assembly.md`.
