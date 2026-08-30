# Flame Leviathan: the blast is on the wrong object, and the fleet parks in one circle

## Context

Yesterday's fixes shipped (commits `5a8fbd275`, `2ad24e8c3`) and today produced the first RaidObs
traces of this fight, plus **the first kill**:

| trace | length | outcome |
|---|---|---|
| `603_1_flame-leviathan_1788099810.ndjson` | 379 s (encounter live to 349 s) | `idle` — wipe |
| `603_1_flame-leviathan_1788100330.ndjson` | 303 s | **`kill`** |

Called them A and B below. The chronicle log for both is already gone (mod-chronicle rotated at
18:01, after the 17:30/17:37 pulls), so everything here comes from the traces. `AC_AI_PLAYERBOT_
ULDUAR_FLAME_LEVIATHAN_HARD_MODE=1` is set and verified in the running worldserver, so tower dodging
was active for both.

The user asked three questions. All three have answers, and two of them expose defects in what
shipped yesterday.

Vehicle combat reaches, which several of these numbers turn on:
Flame Leviathan **15.0**, Salvaged Siege Engine **7.7**, Salvaged Demolisher **2.25**,
Salvaged Chopper **1.0**.

## Q1 — "Siege Engines can sometimes be stuck in place doing nothing"

Real, and not a bot state-machine bug: it is **Hodir's Fury's 60-second stun** landing on the
vehicle.

The clearest case is Shadow's siege engine in pull A. It sat at exactly `(305.0, -101.1)`,
orientation `2.62`, at 100 % HP, from **127.2 s to 187.2 s — exactly 60.0 s**. Across that window the
drive action kept working: 10 `MoveTo` calls were accepted, with goals up to 122 yd away, and 54 more
were refused as `wait` behind the move already "running". A **Hodir's Fury Targetting Reticle sat
1.3 yd away for 64 consecutive snapshot frames.**

Why an accepted move produces no movement:
[PointMovementGenerator.cpp:36](src/server/game/Movement/MovementGenerators/PointMovementGenerator.cpp#L36)
returns from `DoInitialize` **without launching a spline** when the unit has `UNIT_STATE_NOT_MOVE`.
The generator registers (snapshots show `moveGen = 8`, POINT) and the unit never moves.
`MovementAction::MoveToImpl` reports `Issued` as soon as it calls `DoMovePoint`, so `ok=1` in the
trace means "MovePoint was called", never "the vehicle moved".

`SPELL_HODIRS_FURY_STUN` = **62297**: DurationIndex 3 = **60 000 ms**, `EffectAura_2` = 12
(`MOD_STUN`), EffectRadiusIndex 13 = **10 yd**.

Totals for stationary-while-still-issuing-moves, excluding corpses and post-encounter frames:
**73 s (A) / 96 s (B)** of driver time. Player-side stun load was **523 bot-seconds over 15
applications (A)** and 64 s over 7 (B).

Salvaged vehicles get no protection. The immunity in
[boss_flame_leviathan.cpp:767](src/server/scripts/Northrend/Ulduar/Ulduar/boss_flame_leviathan.cpp#L767)
is applied in `boss_flame_leviathan_seat::PassengerBoarded`, i.e. only to players riding the **boss's
own** seats.

## Q2 — "Demolishers do not try to free trapped inside ice vehicles"

**There is nothing to free.** No ice object exists: the full creature inventory of pull A lists no
prison/block entry, only the reticles and their strike creatures. "Trapped in ice" is the same 60 s
62297 stun — Hodir's Fury is the frost tower's ability and reads as ice on screen.

62297 has `Mechanic = 0` and no dispel type, so it is not dispellable, not trinketable, and not
reachable by any mechanic-clearing effect. The only removal in the whole encounter is
`instance->DoRemoveAurasDueToSpellOnPlayers(SPELL_HODIRS_FURY_STUN)`, called from `Reset()` and
`JustDied()`. A demolisher can do nothing for a frozen vehicle. **The only lever is not being hit.**

### How the hazard actually works, which changes the dodge

[`npc_hodirs_fury`](src/server/scripts/Northrend/Ulduar/Ulduar/boss_flame_leviathan.cpp) (entry
33108) is not a chaser at the moment that matters:

1. `me->SetWalk(true)` — it **walks**.
2. Every 30 s it picks a target within 200 yd and `MoveFollow(target, 0.0f, 0.0f)` — homes onto it.
3. On `MovementInform(FOLLOW_MOTION_TYPE)` — i.e. when it **arrives** — it roots itself
   (`SetControlled(true, UNIT_STATE_STUNNED)`) and starts a **5000 ms fuse**.
4. The fuse ends: it summons `NPC_HODIRS_FURY` overhead and casts 62533, landing the 10 yd blast
   **where it stopped**.

So while it walks it is harmless, and once it commits it is a **static 5-second telegraph**. What
lets it arrive at all is a vehicle that is standing still.

Measured against detected commits (reticle goes stationary ≥4 s after moving ≥2 yd):

| | drivers in the circle at fuse start | cleared 10 yd in 5 s | caught |
|---|---|---|---|
| A | 20 | 15 (75 %) | 5 |
| B | 36 | 33 (92 %) | 3 |

Escapers reached a median 19 yd. So the dodge mostly works — but every miss costs 60 s, and a frozen
vehicle cannot dodge the next one either.

### The real multiplier: the fleet stands in one circle

`FlameLeviathanRearPoint` returns **one point per vehicle class**, so every demolisher drives to the
same spot, every siege engine to another, every chopper to a third. Share of the fight with N
distinct vehicles inside a single 10 yd circle:

| | ≥4 vehicles | ≥6 vehicles |
|---|---|---|
| A | **50.0 %** | 17.2 % |
| B | **57.3 %** | 30.1 % |

One Hodir's Fury therefore takes out a whole class. In pull A at t = 260.14 s the trace shows **six
bots receiving 62297 within the same millisecond**; the pull fell apart shortly after.

## Q3 — "Does the Chopper ahead of FL keep enough distance from Battering Ram?"

**No — and the check shipped yesterday cannot see it, because the shape of the spell is wrong.**

Battering Ram (62376) is cast `me->CastSpell(me->GetVictim(), SPELL_BATTERING_RAM, false)` when
`me->IsWithinCombatRange(me->GetVictim(), 15.0f)`
([boss_flame_leviathan.cpp:472-478](src/server/scripts/Northrend/Ulduar/Ulduar/boss_flame_leviathan.cpp#L472)).
Its `ImplicitTargetA_1` is **53 = `TARGET_DEST_TARGET_ENEMY`** with `ImplicitTargetB_1` = 16 and
EffectRadiusIndex 20 = 25 yd.

It is a **25 yd sphere centred on the pursued vehicle**, not a cone off the boss's front. Yesterday's
`FlameLeviathanInBatteringRamArc` tests "within 25 yd of the *boss* and inside his frontal 180°",
which is the wrong object.

The lead chopper is the worst-placed vehicle and the most invisible to the check:

- `FlameLeviathanLeadPoint` parks it at `bossReach 15 + ULDUAR_FL_TAR_LEAD_DIST 30` = **45 yd ahead of
  the boss**; measured median distance to the boss 39.9 (A) / 38.8 (B) yd.
- The shipped gate is `dist2d(vehicle, boss) <= 25 + GetObjectSize()`. A chopper's CombatReach is
  **1.0**, so that gate is 26 yd — **never true for the tar lead**. It never backs off.
- It was nonetheless inside the real 25 yd blast for **15.1 % (A) / 32.4 % (B)** of all frames in
  which the boss could fire, with a p25 distance to the victim of 14.5 yd in B.

Across all stations, scoring only frames where the boss was within `IsWithinCombatRange(victim, 15)`:

| | real exposure caught by the gate | of what the gate fires on, false alarms |
|---|---|---|
| A | 477 / 1310 = **36.4 %** | 742 / 1219 = **60.9 %** |
| B | 480 / 1413 = **34.0 %** | 572 / 1052 = **54.4 %** |

Share of each station's real exposure that is invisible to the check: **tar-lead 77 % / 78 %,
demolisher 72 % / 81 %, chopper 65 % / 68 %, siege 54 % / 54 %.** So it misses two thirds of the
danger and spends more than half its activations dragging a vehicle off station for nothing.

## Bonus defect found while checking the probes: the per-instance state is `thread_local`

`AC_MAP_UPDATE_THREADS=6` in the running worldserver (the `.conf` says `1`; the env override wins).
[UldBossHelper.cpp:2593](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L2593) declares

```cpp
thread_local std::unordered_map<uint32 /*instanceId*/, FlameLeviathanState> flStates;
```

A map is not updated by two threads at once, but it is not pinned to one either, so across ticks the
same instance sees **up to six independent copies** of `ventClaimedBy`, `pursuedVehicle`,
`pursueSeenMs` and `pullTraced`. That is exactly the observed symptom: `fl.pursued` emitted **199 (A)
and 448 (B)** notes for roughly 12 and 16 real switches, flapping to `none` and back within 1.5 s for
the same bot. It also silently weakens the vent-interrupt claim.

`_ignisTankArcStates` ([UldBossHelper.cpp:2242](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L2242)) has the
same defect.

## Plan

### 1. Save the analysis harness first

Every number in this document came from throwaway scripts in the session scratchpad. They are the
only way to check whether any of the changes below worked, so commit them before anything else —
once the scratchpad is gone the baseline is unreproducible.

[tools/botobs/postmortem.py](tools/botobs/postmortem.py) already has the right shape: a `Trace`
loader plus one `show_*` function per `--flag`. Two of the analyses are not FL-specific and belong
there as new modes reusing that loader:

- `--stalls` — stationary runs where the owning action kept issuing **accepted** `MoveTo` calls, i.e.
  ordered to move and didn't. Must exclude corpses and post-encounter frames, and must not count a
  legitimately parked bot: parking returns before `MoveTo`, so it emits no `move` record at all, and
  that absence is the discriminator. This is what found the 60 s freeze.
- `--clump R` — largest number of distinct positions inside one R-yard circle over time. Generic
  AoE-stacking metric; `--clump 10` is what produced the 50 % / 57 % figure.

The rest needs encounter knowledge, so put it in a new `tools/botobs/flame_leviathan.py`:
pursued-vehicle identification from the boss's `snap` target, Battering Ram exposure scored against
both the real blast and the shipped predicate, and Hodir's Fury commit/escape scoring.

It currently reconstructs vehicles by clustering riders on identical coordinates. **Delete that once
step 2 lands** and read the vehicle rows directly — leaving the inference in would quietly outlive
the gap it works around.

### 2. Put the vehicles in the trace

Everything above about vehicles was reconstructed by clustering riders on identical coordinates,
because salvaged vehicles appear in `unit` records but never in `snap` rows.

- [src/Bot/Obs/RaidObs.cpp](src/Bot/Obs/RaidObs.cpp) `BuildSnapshotPayload` (line 756): in the roster
  loop, after each player's row, also emit `player->GetVehicleBase()` when it is a creature, deduped
  through a local `unordered_set` (several riders share one vehicle). Call `EnsureUnit` so it is
  named. This is generic — it also covers Malygos drakes, Oculus and the ICC gunship — and costs at
  most one row per distinct vehicle. Vehicle **HP** is the payoff: "vehicle destroyed" is currently
  only inferable from the rider suddenly running a melee rotation.
- Add an `fl.frozen` probe in `TickFlameLeviathan`: `ObsValue<bool>` per bot fed from
  `FlameLeviathanRiddenVehicle(bot)->HasUnitState(UNIT_STATE_NOT_MOVE)`. It emits only on change, so
  it brackets each 60 s stun exactly and makes Q1 a one-line query instead of an inference.

### 3. Make the per-instance state actually per-instance

Drop `thread_local` from `flStates` (and `_ignisTankArcStates`) and guard the container with a
`std::mutex`. The entries themselves are only touched by the one thread updating that map at that
tick; it is the container that needs protecting. Contention is negligible at this call rate.

Do this **before** step 4 — the blast test reads `pursuedVehicle`, and a per-thread copy of it would
make the new rule flap exactly like the probe does.

### 4. Correct the Battering Ram model

In [UldBossHelper.cpp](src/Ai/Raid/Uld/Util/UldBossHelper.cpp):

- Replace `FlameLeviathanInBatteringRamArc` with a blast test against the **pursued vehicle**.
  `FlameLeviathanStateFor(bot).pursuedVehicle` already holds its guid; resolve it through the map.
- New `FlameLeviathanShouldClearBatteringRam`: back off when
  `myVehicle->GetExactDist2d(pursued) <= ULDUAR_FL_BATTERING_RAM_RADIUS + myVehicle->GetObjectSize()`
  **and** `boss->IsWithinCombatRange(pursued, 15.0f)` — use the core's own predicate so the bot's idea
  of "he can fire" matches the script's exactly.
- The pursued vehicle stays exempt; it is already kiting.
- **Delete the switch-prediction term.** `FlameLeviathanPursueSwitchImminent` /
  `FlameLeviathanMsSincePursue` / `ULDUAR_FL_PURSUE_PERIOD_MS` / `ULDUAR_FL_PURSUE_CLEAR_LEAD_MS` can
  all go. With the blast anchored on the victim the rule re-aims itself the moment the pursued guid
  changes (the scan runs every 200 ms), so predicting the switch buys nothing and is where most of the
  54–61 % false alarms came from.

In [UldActions_FlameLeviathan.cpp](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp)
`ClearBatteringRam`: flee radially away from the **pursued vehicle**, not from the boss. Keep the
`2.0f * ULDUAR_FL_ARRIVE_TOLERANCE` overshoot — `DriveTo` parks anywhere within one deadband and a
single one leaves the vehicle on the blast edge.

Also clamp the lead chopper. `FlameLeviathanLeadPoint` should lead along the boss's facing by
`min(ULDUAR_FL_TAR_LEAD_DIST, dist2d(boss, pursued) - ULDUAR_FL_BATTERING_RAM_RADIUS - size)`,
falling back to the rear station when that goes non-positive. The tar still lands in his path,
because his path is the boss→victim line — it just stops short of the blast.

### 5. Fan the station points out (bearing only)

`FlameLeviathanOffsetPoint` / `FlameLeviathanRearPoint` return one point per class. Give each vehicle
a stable slot: its rank among live same-class vehicles ordered by GUID — the ordering
`FlameLeviathanIsTarLead` already uses — then offset the bearing by `slot * spread`, centred so the
class stays balanced about its nominal bearing.

Pick `spread` from the radius so neighbours land more than one Hodir's Fury circle apart:
`2.0f * std::asin(12.0f / (2.0f * radius))` for a 12 yd target spacing (10 yd blast plus margin),
clamped to a sane maximum so a large class does not wrap around him. At the three radii that is
roughly 25° for siege (23 yd), 17° for choppers (35 yd) and 9° for demolishers (65 yd).

**Stand distances stay exactly as they are.** Ram's 18 yd cone, Sonic Horn's 35 yd cone and the
10–70 yd pyrite band all keep their current geometry; only the stacking changes.

### 6. Fix the Hodir's Fury dodge

In `ClearHazard`:

- Drop the perpendicular break for `NPC_FL_HODIRS_FURY_TARGET`. It was reasoned from "it chases", but
  once it arrives it roots itself and the blast lands where it stopped, so **radial is what maximises
  distance from a now-fixed point.** Treat all three reticles the same.
- Size the flee off the real 10 yd effect radius plus margin instead of
  `ULDUAR_FL_TOWER_HAZARD_RADIUS` (18). The vehicle only has to clear 10 yd, and a shorter dodge
  returns it to station sooner. Keep 18 yd as the *scan* radius — that is the warning band.
- In `DriveTo`, **do not park** while `GetFlameLeviathanNearestTowerHazard` is non-null. A stationary
  vehicle is the only thing a walking reticle can catch.

### 7. Let a frozen vehicle give up its roles

- `FlameLeviathanIsTarLead` skips only dead or pursued members, so a frozen chopper holds the lead
  slot for its whole 60 s. Add a liveness test — vehicle not in `UNIT_STATE_NOT_MOVE`, rider not
  stunned — to both the self-check and the peer loop.
- Same test in `FlameLeviathanIsVentInterrupter`, and release `ventClaimedBy` when the claimant is
  frozen, so the channel is re-elected instead of blocked.

### 8. Write the findings down

- [docs/raids/ulduar.md](docs/raids/ulduar.md) — correct the Battering Ram bullet to the 25 yd sphere
  on the pursued vehicle; add Hodir's Fury's walk → root → 5 s fuse → 60 s undispellable stun; add the
  clumping figure and why one station point per class is the multiplier.
- [docs/engine/pitfalls.md](docs/engine/pitfalls.md) — the general lesson: **read a spell's shape from
  its implicit target type, not from what the boss looks like it is doing.** `TARGET_DEST_TARGET_ENEMY`
  anchors on the victim; only `TARGET_UNIT_CONE_ENEMY_*` is a cone off the caster. This doc is
  referenced by `CLAUDE.md`, so run `/compact-docs-writer` before editing it.
- Copy this plan to
  `docs/plans/flame-leviathan-blast-and-freeze/flame-leviathan-blast-and-freeze.PLAN.md`, and add a
  line to the existing
  `docs/plans/flame-leviathan-vehicle-survival/flame-leviathan-vehicle-survival.PLAN.md` Status
  section recording that its Battering Ram step shipped against the wrong target shape and is
  superseded here.

## Files

- [src/Bot/Obs/RaidObs.cpp](src/Bot/Obs/RaidObs.cpp) — vehicle rows in `BuildSnapshotPayload`
- [src/Ai/Raid/Uld/Util/UldBossHelper.h](src/Ai/Raid/Uld/Util/UldBossHelper.h) — fan-out declarations;
  remove the pursue-prediction constants
- [src/Ai/Raid/Uld/Util/UldBossHelper.cpp](src/Ai/Raid/Uld/Util/UldBossHelper.cpp) — state locking,
  blast test, fan-out, lead-point clamp, role liveness, `fl.frozen`
- [src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp)
  — `ClearBatteringRam`, `ClearHazard`, `DriveTo` parking, `HoldStation`
- [src/Ai/Raid/Uld/Trigger/UldTriggers_FlameLeviathan.cpp](src/Ai/Raid/Uld/Trigger/UldTriggers_FlameLeviathan.cpp)
  — already routes through `FlameLeviathanShouldClearBatteringRam`; confirm it still compiles
- [tools/botobs/postmortem.py](tools/botobs/postmortem.py) — new `--stalls` and `--clump` modes
- `tools/botobs/flame_leviathan.py` — new; the FL-specific scoring
- docs as listed in step 8

## Verification

The module cannot be compiled headless here, so this needs a hand-off build and a live re-pull with
both towers standing. Today's traces are the baseline; re-run the same measurements on the new one.

1. Trace opens at `env/dist/logs/botobs/603_*_flame-leviathan_*.ndjson`, and `snap` rows now carry
   `Salvaged Siege Engine` / `Demolisher` / `Chopper` guids.
2. `postmortem.py <file> --notes fl` — `fl.pursued` should emit on the order of **one note per real
   switch** instead of 199/448, and `fl.frozen` should bracket each stun.
3. Numbers to beat:

   | measure | A | B | target |
   |---|---|---|---|
   | stationary while still issuing MoveTo | 73 s | 96 s | ≈0 outside real stun windows |
   | drivers caught by a committed Hodir's Fury | 5 / 20 | 3 / 36 | lower |
   | fight with ≥4 vehicles in one 10 yd circle | 50 % | 57 % | well under |
   | tar-lead inside the 25 yd blast | 15.1 % | 32.4 % | ≈0 |
   | real blast exposure the backoff catches | 36.4 % | 34.0 % | much higher |
   | backoff activations that were false alarms | 60.9 % | 54.4 % | much lower |

4. Guard against a regression on the thing that already works: pull B was a **kill**, and the
   fan-out is the change most likely to disturb it. Check Ram and Sonic Horn hit rates and pyrite
   uptime did not drop.

Every row above is produced by the harness from step 1, so re-running it on the new trace is the
whole check. Keep both of today's traces alongside it — they are the baseline, and the chronicle logs
that would otherwise corroborate them are already rotated away.
