# Flame Leviathan strategy rework

## Context

The Ulduar Flame Leviathan strategy (`src/Ai/Raid/Uld/.../*_FlameLeviathan.*`) is the only Ulduar
encounter fought entirely from vehicles, and its bot behaviour is a thin stub: one action that
switches on the vehicle entry and fires one or two spells, plus a corner-kite when the boss chases.
Half of each vehicle's spellbook is never touched, the boss is resolved through a value that usually
does not contain him, the demolisher's pyrite economy is not modelled at all, and nothing drives a
vehicle toward the boss — so the generic movers own the vehicle for most of the fight.

This rework gives each seat its real spellbook, replaces the corner kite with a wall loop, models the
pyrite economy, adds a Flame Vents interrupt rotation, and puts **all** vehicle movement behind a
single action so the generic movers cannot fight it.

## Verified facts

Sources: `Spell.dbc` / `Vehicle.dbc` / `VehicleSeat.dbc` / `SummonProperties.dbc` (via
`modules/mod-spell-tweaks/data/dbc-reference/` and direct binary parse),
`acore_world.creature_template{,_spell}` / `creature_immunities` / `vehicle_template_accessory`, and
`src/server/scripts/Northrend/Ulduar/Ulduar/boss_flame_leviathan.cpp`. No `spell_dbc` override rows
exist for any spell below, so the DBC values are authoritative.

### Spellbooks (`creature_template_spell`)

| Vehicle | Entry | Spells (id, energy cost, cooldown) |
|---|---|---|
| Salvaged Siege Engine | 33060 | Ram 62345 (40) · Electroshock 62522 (20, 10s) · Steam Rush 62346 (40, 15s) |
| Salvaged Siege Turret | 33067 | Fire Cannon 62358 (20) · Anti-Air Rocket 62359 (10, 250ms) · Shield Generator 64677 (0, 60s) |
| Salvaged Chopper | 33062 | Sonic Horn 62974 (20) · Tar 62286 (0, 15s) · Speed Boost 62299 (50) · First Aid Kit 64660 · Grab Pyrite 67372 |
| Salvaged Demolisher | 33109 | Hurl Boulder 62306 (0) · Hurl Pyrite Barrel 62490 (5) · Ram 62308 (0, 4s) · Throw Passenger 62324 |
| Demolisher Mechanic Seat | 33167 | Mortar 62634 (0, 1s) · Anti-Air Rocket 64979 (0, 250ms) · Grab Crate 62479 (0) · Increased Speed 62471 (25) · Load into Catapult 64414 |

**Every one of these is instant** (`CastingTimeIndex 1` = 0 ms). `CanCastVehicleSpell` only refuses
cast-time spells while the base is moving, so driving and casting compose freely here — there is no
"kite or DPS, pick one" trade-off.

### Seat topology — and gunners *can* steer

Positional map from `Vehicle.dbc` → `VehicleSeat.dbc`:

| Vehicle | Seat | Flags |
|---|---|---|
| Siege Engine 33060 | 0 driver | CONTROL + CAST |
| | 1, 2 | **no flags at all** |
| | 7 | accessory 33067, TURN + ATTACK |
| Siege Turret 33067 | 0 gunner | **CONTROL** + CAST |
| Demolisher 33109 | 0 driver | CONTROL + CAST |
| | 1 | accessory 33167, TURN + ATTACK |
| | 2 | accessory 33620 Earthen Stoneshaper |
| | 3 | ENTER_EXIT only — dead seat |
| Mech Seat 33167 | 0 gunner | **CONTROL** + CAST |
| Chopper 33062 | 0 driver | CONTROL + CAST |
| | 1 | ENTER_EXIT only — dead seat |

Seat 0 of the turret and the mech seat carries `CAN_CONTROL`, so `MovementAction::MoveTo` passes its
`seat->CanControl()` check for a **gunner** and calls `DoMovePoint` on the *turret creature* bolted
into seat 7. **Never gate movement on `IsInVehicle(true)` on this encounter** — gate on
`vehicleBase->GetEntry() ∈ {33060, 33062, 33109}`.

No seat a bot can occupy has `CAN_ATTACK` (that flag sits on accessory seats, which hold NPCs), so
`CastSpellAction::isUseful` already blocks class spells for every bot in a vehicle. No
targeting-suppression multiplier is needed.

`instance_ulduar.cpp:1030-1100` spawns **2 of each vehicle in 10-man, 5 of each in 25-man** at
`x = 119.8`, west of the arena — exactly **10 / 25 usable slots, zero slack**.

### Pyrite economy — the demolisher does not regenerate

`Creature::Regenerate` returns early at `Creature.cpp:977` without `UNIT_FLAG2_REGENERATE_POWER`
(0x800). Max energy is **100** for all of them (`Unit.cpp:12581`).

| Vehicle | `unit_flags2` | Regen | Costs | Refill |
|---|---|---|---|---|
| Siege Engine / Turret / Chopper | 2048 | **+20 / 2s** | — | — |
| **Demolisher 33109** | **0** | **none** | Barrel 5 · Boulder 0 · Ram 0 · *plus* Increased Speed POWER_BURNs **25** off it | **+25 per crate** |
| **Mech Seat 33167** | **0** | **none** | Increased Speed **25**; everything else free | **none** |

A full demolisher tank is **20 barrels**, then Hurl Boulder forever unless crates arrive. The mech
seat's 100 is never refilled (`62473` targets the demolisher, not the seat), giving the gunner
exactly **4 Increased Speeds for the whole fight** — ample, since a given demolisher takes Pursued
once or twice.

Crate chain: **80 `Mechanolift 304-A` (33214) spawns**, each carrying a `33218 Pyrite Safety
Container` in seat 1 (`vehicle_template_accessory`). Shot down → container falls →
`MovementInform` summons **`33189 Liquid Pyrite`** → grabbable. Anti-Air Rocket reaches **1000 yd**
(`rngIdx 174`) and lifts spawn across `x -523..440, y -528..303` — far wider than the arena — so
*where* you shoot decides whether the crate is reachable.

`spell_vehicle_grab_pyrite` (`boss_flame_leviathan.cpp:1486-1530`) branches:
- **Demolisher gunner**: casts `62496` on the parent demolisher → `62473 Reload Ammo` →
  `SPELL_EFFECT_ENERGIZE` **+25**, crate despawns.
- **Chopper**: no energy — the crate loads into the chopper's rear seat to be *ferried to other
  vehicles*. This is the retail supply line and no bot code touches it. **Deferred** (see gaps).

### Tar and Blaze — harmless to us, real damage to him

**Pool of Tar 33090 is faction `1965`, the same faction as Flame Leviathan.** Our vehicles are
`2105`. `SummonProperties 64` is `Category 0 / Faction 0 / Flags 0` and the pool is not a vehicle, so
`TempSummon::InitStats:246-254` never overrides the template faction with the summoner's. Both
effects use `ImplicitTargetB = 30` (`TARGET_UNIT_SRC_AREA_ALLY`), so they only ever hit FL's faction:

| Spell | Effect | Hits |
|---|---|---|
| `62287` Tar snare, 50 yd | `MOD_DECREASE_SPEED -75%`, `Mechanic 11` | FL's faction. **Blocked on FL** — `creature_immunities` row `-362` masks SNARE. Adds are *not* immune. |
| `62290` Burning Tar, 15 yd | 5400 fire per 1s tick, 45s | FL and his adds. **Never our vehicles.** |

`62286 Tar` summons the pool at `TARGET_DEST_CASTER_BACK`, 9 yd **behind** the chopper. Ignition
needs a fire-school spell to hit the pool, which is `UNIT_FLAG_NOT_SELECTABLE` and cannot be aimed at:

| Spell | School | Ignites? |
|---|---|---|
| `62635` Mortar (demolisher gunner) | 4 fire, `DEST_AREA_ENEMY` | **yes** |
| `62489` Blue Pyrite (from Hurl Pyrite Barrel) | 68 = Arcane\|Fire, 20 yd `DEST_AREA_ENEMY` | **yes** |
| `62358` **Fire Cannon** | **1 = physical** despite the name | no |
| `62306` Hurl Boulder, `62974` Sonic Horn | physical | no |
| `63847` Flame Vents | fire, but targets *FL's* enemies — the pool is his ally | no |

Ignition is therefore incidental: demolishers hurl fire AoE at FL, and pools sit where FL drives.

### Boss mechanics

- **Flame Vents 62396 is a channel.** `AttributesEx = 200` carries `0x40`
  (`SPELL_ATTR1_CHANNELED_2`), 10s, `PERIODIC_TRIGGER_SPELL_WITH_VALUE` every 1s firing 63847 (15000
  damage, 500 yd radius — not dodgeable). `EVENT_VENT` repeats every 20s.
  `boss_flame_leviathan::SpellHit` maps Electroshock to `InterruptNonMeleeSpells(false)`, which
  interrupts `CURRENT_CHANNELED_SPELL` unconditionally (`Unit.cpp:4343-4345`). The interrupt is real.
- **Electroshock is range-limited by its cone, not by spell range.** `Spell::CheckRange` returns OK
  immediately for `RangeEntry->ID == 1` (`Spell.cpp:7097`), so the real limit is the effect radius:
  Ram, Electroshock and Sonic Horn are all `TARGET_UNIT_CONE_ENEMY_104` with `EffectRadiusIndex`
  19 / 20 / 21 = **18 / 25 / 35 yd**. The interrupt rotation must gate on range or a siege engine
  parked across the arena wins the ranking and lands nothing. FL is `MECHANIC_INTERRUPT`-immune, but
  only effect 1 carries that mechanic — effect 2 (8489 damage) lands, so the spell hits and the
  script hook fires.
- **Pursued 62374**, random vehicle every 31s, 35s duration; FL then drives at it and uses
  `Battering Ram 62376` whenever `IsWithinCombatRange(victim, 15.0f)`. The aura is a far better
  signal than `flame->GetVictim()`.
- **FL gets faster all fight.** `Gathering Speed 62375` is `MOD_SPEED_ALWAYS +5%`, **stacks to 20**,
  600s, re-applied every 15s, cleared only on reset:

  | Unit | speed_run | yd/s |
  |---|---|---|
  | Flame Leviathan | 0.71429 | 5.0 → **10.0** at 20 stacks |
  | Siege Engine / Demolisher | 1.0 | 7.0 |
  | Chopper | 2.0 | 14.0 |

  A siege engine outruns him for roughly two minutes and loses after ~8 stacks. A wasteful kite path
  is fatal, and Steam Rush / Increased Speed / Speed Boost are the late-fight answer.
- **Arena box**: the four `NPC_FREYA_WARD_TARGET` spawns in `SummonTowerHelpers` sit at the corners —
  `(159.4, 64.1, 409.8)`, `(382.9, 74.0, 411.6)`, `(374.0, -141.0, 411.0)`, `(157.7, -140.3, 409.8)`.
  FL's home is `(322.39, -14.5, 409.8)`. The existing `corners` array is these four, rounded.
- **`"attackers"` usually does not contain FL.** `AttackersValue::AddAttackersOf` walks
  `player->GetThreatMgr().GetThreatenedByMeList()`, and threat here belongs to the vehicle creature,
  not the bot player. This is why Electroshock frequently lands on a Mechanolift.
- **Steam Rush 62346** is `CHARGE_DEST` at `TARGET_DEST_CASTER_FRONT` — a forward dash up to 70 yd,
  so it only escapes if the vehicle already faces away from FL.
- **Shield Generator 64677** is `SCHOOL_ABSORB` **15** for 5s on a 60s cooldown. Negligible; wired
  anyway because it is free.
- **Hurl Pyrite Barrel needs no ammo** (no `CasterAuraSpell`) — 5 energy is the only gate. It
  triggers `62489` (54000 AoE) applying stacking DoT **68605 Blue Pyrite** (12000/tick, 10s, 10 stacks).

## Decisions

| # | Decision |
|---|---|
| Movement gating | Gate on `vehicleBase->GetEntry()`, never `IsInVehicle(true)` |
| Movement ownership | **One** action owns all vehicle movement, including hard-mode hazard clearance |
| Engage | Pursue and the suppression multiplier stay inert until FL is attackable and the bot is in combat |
| Kite | Wall-hugging ring, not corner-to-corner |
| Interrupt | Rotate by energy desc, guid asc; priority above hazard clearance |
| Siege energy | Only the currently top-ranked interrupter reserves 20; everyone else spends freely |
| Demolisher | Barrel at `energy >= 30`, boulder below; driver detours to a crate below 30 |
| Lifts | Shoot only lifts inside the arena box plus the detour radius |
| Positioning | Siege + Chopper on FL's rear arc; Demolishers in their hurl band |
| Tar | One designated chopper leads ahead of FL; Pursued choppers are excluded from the role but still cast Tar opportunistically |
| Shield Generator | Only while Pursued |
| Parked | The drive action returns `false` once parked and facing |
| Adds | Targeting fallback only — never a movement claim |
| **Deferred** | Boarding's master-dependency; chopper pyrite ferry; seat-shortfall fallback; ground-target `CastVehicleSpell` overload |

## Files

| File | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}` | FL spell/NPC constants, geometry + threshold constants, shared helpers |
| `src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.{h,cpp}` | Rework rotation; new drive + interrupt actions; delete the tower-hazard action |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_FlameLeviathan.{h,cpp}` | New triggers; delete the tower-hazard trigger |
| `src/Ai/Raid/Uld/UldActionContext.h`, `UldTriggerContext.h` | Register new names, drop removed ones |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | Rewire the FL nodes, push the new multiplier |
| `src/Ai/Raid/Uld/UldMultipliers.{h,cpp}` | New `FlameLeviathanVehicleMovementMultiplier` |
| `docs/raids/ulduar.md` | Replace the Flame Leviathan section with the verified facts above |
| `docs/engine/raid-mechanics-lessons.md` | Add the waypoint-kite lesson |

No `CMakeLists.txt`; new files are globbed. No new files needed — everything lands in the existing
per-boss split.

## Implementation

### 1. Constants and shared helpers

Every FL spell id is a raw literal today, and `availableVehicles` is defined twice at namespace scope
(`UldActions_FlameLeviathan.cpp:31`, `UldTriggers_FlameLeviathan.cpp:19`). Hoist all of it into
`UldBossHelper.h`, mirroring the core enum values per the "mirror, do not include" convention:

```
SPELL_FL_PURSUED = 62374, SPELL_FL_GATHERING_SPEED = 62375, SPELL_FL_BATTERING_RAM = 62376,
SPELL_FL_FLAME_VENTS = 62396, SPELL_FL_MISSILE_BARRAGE = 62400,
SPELL_FL_RAM = 62345, SPELL_FL_ELECTROSHOCK = 62522, SPELL_FL_STEAM_RUSH = 62346,
SPELL_FL_FIRE_CANNON = 62358, SPELL_FL_ANTI_AIR_ROCKET_SIEGE = 62359, SPELL_FL_SHIELD_GENERATOR = 64677,
SPELL_FL_SONIC_HORN = 62974, SPELL_FL_TAR = 62286, SPELL_FL_SPEED_BOOST = 62299,
SPELL_FL_HURL_BOULDER = 62306, SPELL_FL_HURL_PYRITE_BARREL = 62490, SPELL_FL_DEMOLISHER_RAM = 62308,
SPELL_FL_MORTAR = 62634, SPELL_FL_ANTI_AIR_ROCKET = 64979, SPELL_FL_GRAB_CRATE = 62479,
SPELL_FL_INCREASED_SPEED = 62471, SPELL_FL_BLUE_PYRITE_DOT = 68605,
NPC_FLAME_LEVIATHAN = 33113, NPC_FL_TURRET = 33139, NPC_FL_DEFENSE_TURRET = 33142,
NPC_FL_PYRITE_CONTAINER = 33189, NPC_FL_MECHANOLIFT = 33214, NPC_FL_POOL_OF_TAR = 33090,
```

Geometry and thresholds:

```
ULDUAR_FL_ARENA_CORNERS                // the four Freya-ward positions
ULDUAR_FL_SIEGE_STAND_DIST     = 8.0f  // Ram is a 15 yd spell; leave headroom for the model
ULDUAR_FL_CHOPPER_STAND_DIST   = 6.0f
ULDUAR_FL_DEMOLISHER_BAND      = 50.0f // inside the 10-70 yd hurl range, off the Battering Ram line
ULDUAR_FL_TAR_LEAD_DIST        = 30.0f // how far ahead of FL the lead chopper parks
ULDUAR_FL_REPOSITION_EPSILON   = 6.0f  // re-issue MoveTo only when the goal moved this far
ULDUAR_FL_CRATE_DETOUR_RADIUS  = 60.0f // how far a starved demolisher will leave its band
ULDUAR_FL_PYRITE_RESERVE       = 30.0f // barrel above this, boulder below; keeps 25 for Increased Speed
ULDUAR_FL_KITE_WALL_INSET      = 15.0f // keep the ring off the wall so MoveTo has mesh to land on
ULDUAR_FL_KITE_CORNER_CHAMFER  = 35.0f // cut each 90-degree corner into two nodes
ULDUAR_FL_KITE_ADVANCE_DIST    = 30.0f // switch to the next node while still this far out
ULDUAR_FL_KITE_BOSS_CLEARANCE  = 50.0f // a node this close to FL is not a destination
```

Helpers, declared in `UldBossHelper.h` and defined **once** in `UldBossHelper.cpp` — a trigger and
its action must call the same function or the two derivations disagree:

- `Unit* FlameLeviathanBoss(PlayerbotAI*)` — `GetFirstAliveUnitByEntry(botAI, NPC_FLAME_LEVIATHAN)`
  (`RaidBossHelpers.h:29`). **Never `"find target"` or `"attackers"`.**
- `bool FlameLeviathanEngaged(PlayerbotAI*)` — boss resolved, not `UNIT_FLAG_NON_ATTACKABLE`, bot in
  combat. Everything else is inert until this is true, so the raid can drive in and pull normally.
- `Unit* FlameLeviathanRiddenVehicle(Player*)` — `bot->GetVehicleBase()`, or *its* `GetVehicleBase()`
  for a gunner, so a gunner resolves the siege engine / demolisher underneath it.
- `bool FlameLeviathanIsDriver(Player*)` — ridden base entry ∈ `{33060, 33062, 33109}`.
- `bool FlameLeviathanIsPursued(Player*)` — `SPELL_FL_PURSUED` on the ridden vehicle **or** its
  parent. Pursued lands on whichever unit the spell picked; walk both links.
- `bool FlameLeviathanIsVentChanneling(Unit* boss)` — `GetCurrentSpell(CURRENT_CHANNELED_SPELL)` with
  `m_spellInfo->Id == SPELL_FL_FLAME_VENTS`.
- `bool FlameLeviathanIsVentInterrupter(PlayerbotAI*, Player*)` — walk the group; keep members whose
  ridden vehicle is a Siege Engine, alive, `!HasSpellCooldown(SPELL_FL_ELECTROSHOCK)` and
  `energy >= 20`. Rank by energy desc, guid asc; true only for the top entry. Casting drops the actor
  to the back of its own queue, so the duty self-rotates with no shared state. **The Ram reserve in
  the rotation reads this same helper** — one helper, two readers.
- `bool FlameLeviathanIsTarLead(PlayerbotAI*, Player*)` — lowest-guid living chopper driver that is
  **not** Pursued.
- `bool FlameLeviathanInArena(Position const&, float margin)` — point-in-box against
  `ULDUAR_FL_ARENA_CORNERS`. Used for lift targeting and to keep destinations inside the arena.
- `Position FlameLeviathanRearPoint(Unit* boss, float standDist)` — bearing
  `GetOrientation() + M_PI`, radius `boss->GetCombatReach() + standDist`, Z from the boss.
- `Position FlameLeviathanLeadPoint(Unit* boss)` — same along `GetOrientation()` at the lead distance.
- `std::vector<Position> const& FlameLeviathanKiteRing()` — built once into a function-local static
  from `ULDUAR_FL_ARENA_CORNERS`: inset each corner by the wall inset toward the centroid, then
  replace it with two nodes at the chamfer distance along each adjoining edge. Eight nodes, ordered
  around the ring, Z interpolated from the corners. Building from the corners rather than hardcoding
  eight literals keeps inset and chamfer tunable in one place.

### 2. `FlameLeviathanVehicleAction` — rotation rework

Keep the registered name `"flame leviathan vehicle"` and the entry dispatch. **Change the base class
from `MovementAction` to `Action`** — it no longer moves, which also makes the multiplier's family
split clean.

- Resolve the boss with `FlameLeviathanBoss`, not by scanning `"attackers"` for entry 33113.
- Keep the `"attackers"` scan only to pick the nearest add, still skipping 33139 / 33142. Adds are a
  targeting fallback and **never** a movement claim (`ulduar.md:97` already relies on this loop for
  the Life tower).
- **Delete `MoveAvoidChasing` and the early-return that calls it.** Movement moves to §3.
- Delete the dead `GetAttacker()` declaration (`.h:32`, no definition) and the unused includes
  (`CombatStrategy.h`, `FollowMasterStrategy.h`, `RtiValue.h`, `RtiTargetValue.h`,
  `TankAssistStrategy.h`); same two in the trigger `.cpp`.

Keep the `AddSpellCooldown(id, 0, N)` GCD stand-in after each successful cast — creature casters get
no server-side cooldown, and without it the action re-fires every tick. Use the real DBC
`RecoveryTime` where there is one, 1000 ms otherwise.

- **`SiegeEngineAction`** (driver): Electroshock moves out to §4. Cast Ram 62345 at `energy >= 40`,
  except when `FlameLeviathanIsVentInterrupter` holds for this bot — then require `energy >= 60` so
  the interrupt stays funded. While Pursued, require `energy >= 80` so Steam Rush stays affordable.
- **`SiegeEngineTurretAction`** (gunner):
  1. Shield Generator 64677 while `FlameLeviathanIsPursued` and off cooldown. Verify by
     `HasAura(SPELL_FL_SHIELD_GENERATOR)` — `CastVehicleSpell` returns true even when `CheckCast` failed.
  2. Anti-Air Rocket 62359 at a Mechanolift **inside the arena box plus the detour radius**.
  3. Fire Cannon 62358 at the boss. *(Today this action only ever casts 62358.)*
- **`DemolisherAction`** (driver):
  1. `Aura* own = boss->GetAura(SPELL_FL_BLUE_PYRITE_DOT, vehicleBase_->GetGUID())` — the
     **caster-scoped** overload, which is what "the stacks applied by them" means. The current code
     reads any caster's aura.
  2. Barrel when `!own || own->GetDuration() <= 5000 || own->GetStackAmount() < 10` **and**
     `energy >= ULDUAR_FL_PYRITE_RESERVE`. The existing `energy >= 20` clause was a crude fuel
     reserve, not a bug — with no regen and 20 barrels per tank, the floor has to stay.
  3. Otherwise Hurl Boulder 62306 (free).
- **`DemolisherTurretAction`** (gunner):
  1. Increased Speed 62471 while Pursued, aura missing, `energy >= 25`. Verify by aura.
  2. Grab Crate 62479 at a `33189` within 50 yd whenever the **demolisher's** energy is at or below
     75, so the +25 is not wasted.
  3. Anti-Air Rocket 64979 at a Mechanolift inside the arena box plus the detour radius.
  4. Mortar 62634 at the boss (free, 1s cooldown).
- **`ChopperAction`** (driver): Tar 62286 when off cooldown and FL is behind us
  (`!vehicleBase->HasInArc(M_PI / 2.0f, boss)`) — true by construction whenever this chopper is
  Pursued or is the Tar lead. Then Sonic Horn 62974 at `energy >= 20`. Speed Boost moves to §3.

### 3. `FlameLeviathanDriveAction` — the single movement owner

New action `"flame leviathan drive"` (`MovementAction`). **Drivers only** —
`FlameLeviathanIsDriver(bot)`, so gunners can never steer their turret. Inert unless
`FlameLeviathanEngaged`.

`FlameLeviathanTowerHazardAction` and `FlameLeviathanTowerHazardTrigger` are **deleted** and their
behaviour folded in as precedence step 2. A separate hazard action at a higher priority does not
avoid the two-owner bounce, it only decides who wins each alternating tick — and
`raid-mechanics-lessons.md:12-14` is explicit that the fix is deleting an owner, never re-ordering.

Internal precedence, first match wins, exactly one `MoveTo` per tick:

1. **Pursued → kite the wall ring** (below).
2. **Hard-mode hazard within `ULDUAR_FL_TOWER_HAZARD_RADIUS`** → move to the nearest point clear of
   it, reusing `GetFlameLeviathanNearestTowerHazard` and the existing flee geometry, at
   `MOVEMENT_FORCED`.
3. **Demolisher below `ULDUAR_FL_PYRITE_RESERVE` with a `33189` inside `ULDUAR_FL_CRATE_DETOUR_RADIUS`**
   → drive to it. Crates are the demolisher's entire throughput past the first 20 barrels.
4. **Chopper that is the Tar lead** → `FlameLeviathanLeadPoint(boss)`; once parked, face **away** from
   FL so the 9 yd pool drops into his path.
5. **Otherwise hold station** — Siege Engine and non-lead Chopper at
   `FlameLeviathanRearPoint(boss, …)`; Demolisher at `ULDUAR_FL_DEMOLISHER_BAND` on the rear bearing.
   Demolishers spawn ~200 yd from FL against a 70 yd hurl range, so "hold the band" means *reach* it
   first, not stand still.

Movement discipline, from `raid-mechanics-lessons.md` and `pitfalls.md`:

- **Latch the goal.** Re-issue `MoveTo` only when the fresh point is more than
  `ULDUAR_FL_REPOSITION_EPSILON` from the last issued one. Re-stamping the same destination restarts
  the spline and the vehicle crawls.
- **Reach then hold.** Generous arrival deadband, latch "arrived", `StopMoving()`, then
  `vehicleBase->SetFacingToObject(boss)` (or away, for the Tar lead). Holding the facing is what
  stops `CastVehicleSpell` burning a tick on `SetFacingToObject` before every cast
  (`PlayerbotAI.cpp:4110-4129`). **Return `false` once parked and facing**, so lower nodes run and
  "am I actually moving" stays honest in the log.
- **Clamp, don't chase.** FL's rear bearing swings while he drives. Keep the bearing the latched
  point holds and slide only along `[boss, point]`, continuous at the clamp distance.
- `MovementPriority::MOVEMENT_COMBAT` (`MOVEMENT_FORCED` for step 2), destinations kept inside the
  arena box.

**The kite (step 1)** replaces `MoveAvoidChasing` (`UldActions_FlameLeviathan.cpp:89-113`), whose
failure is that it only advances once `GetExactDist(corner) < 5.0f` — the vehicle drives all the way
into the corner while FL cuts the diagonal. Over `FlameLeviathanKiteRing()`:

- **Latch the direction once per pull** in a `thread_local std::unordered_map<uint32 instanceId, int8>`
  defined in exactly one `.cpp` (trigger, action and multiplier each hold their own object, so this
  cannot live on the action). On the first Pursued, pick the sense whose neighbouring node is farther
  from FL. **Never reverse** — reversing runs into the pursuer, and a direction that flips on a
  distance test is the oscillation mechanism itself.
- **Entry node**: nearest node ahead in the latched direction, skipping any within
  `ULDUAR_FL_KITE_BOSS_CLEARANCE` of FL.
- **Advance early**: step to `i + dir` as soon as within `ULDUAR_FL_KITE_ADVANCE_DIST` of node `i`,
  never on arrival. With chamfered corners this rounds every turn. Drop the old
  `target->GetExactDist(bot) < 50.0f` half of the condition — it made progress conditional on FL
  being close, which is backwards.
- **Skip nodes FL owns**: if `i + dir` is within the clearance of FL, take `i + 2*dir`.
- **Reset** node index and latched direction when the Pursued window closes. The current
  `avoidChaseIdx` never resets at all.
- Then spend the escape button, once already moving away: **Siege Engine** Steam Rush 62346 at
  `energy >= 40`, off cooldown, and FL behind us (`!HasInArc(M_PI / 2.0f, boss)`) — `CHARGE_DEST`
  fires along our facing, so casting it with FL in front dashes into him. **Chopper** Speed Boost
  62299 at `energy >= 50` with the aura missing. **Demolisher** nothing — its gunner owns Increased
  Speed.

Because ring nodes are inset from straight walls, the "a straight spline is a chord" trap does not
apply — each hop runs along a wall, not across the room. Tar pools are **not** treated as hazards;
they cannot damage our faction.

### 4. Flame Vents interrupt

New trigger `"flame leviathan flame vents"` — **per-tick, unthrottled**; the channel is the whole
window. Active when `FlameLeviathanEngaged`, `FlameLeviathanIsVentChanneling(boss)`, the ridden base
is a Siege Engine, and `FlameLeviathanIsVentInterrupter`.

New action `"flame leviathan interrupt vents"` (plain `Action`): re-resolve the boss, cast 62522,
then `AddSpellCooldown(SPELL_FL_ELECTROSHOCK, 0, 10000)` to match the DBC `RecoveryTime`.

Wired above hazard clearance: the cast is instant and works while moving, so it costs one tick, and a
full channel is 10 ticks of 15000 against one hazard tick.

### 5. Movement-suppression multiplier

`FlameLeviathanVehicleMovementMultiplier` in `UldMultipliers.{h,cpp}`, pushed from
`RaidUlduarStrategy::InitMultipliers`. Model on `OccFlyingMultiplier`
(`src/Ai/Dungeon/OC/OCMultipliers.cpp:36-45`).

- Return `1.0f` unless map 603, the ridden base is an FL vehicle, **and `FlameLeviathanEngaged`** —
  it must stay inert pre-pull or the raid cannot drive in.
- Then a single `dynamic_cast<MovementAction*>`; non-movement actions return `1.0f` with no further
  tests.
- Return `0.0f` for every `MovementAction` except `FlameLeviathanDriveAction`,
  `FlameLeviathanEnterVehicleAction` and `LeaveVehicleAction`. `dynamic_cast` to the **concrete**
  classes, never a shared base.

**Known risk (Void Reaver precedent):** once live, `FlameLeviathanDriveAction` is the only thing that
can move a vehicle. If it silently returns false — off-mesh point, boss unresolved — vehicles freeze
for the whole fight. Keep its failure paths loud in the debug log.

### 6. Strategy wiring

`FlameLeviathanDriveAction` is wired to **two** trigger nodes at different priorities. The engine
caches actions by name, so both nodes drive one instance and one latched destination — still a single
owner.

| Trigger | Action | Priority |
|---|---|---|
| `flame leviathan flame vents` | `flame leviathan interrupt vents` | `ACTION_RAID + 4` |
| `flame leviathan drive urgent` (Pursued **or** hazard in range) | `flame leviathan drive` | `ACTION_RAID + 3` |
| `flame leviathan vehicle near` | `flame leviathan enter vehicle` | `ACTION_RAID + 2` *(existing)* |
| `flame leviathan on vehicle` | `flame leviathan vehicle` | `ACTION_RAID + 1` *(existing)* |
| `flame leviathan on vehicle` | `flame leviathan drive` | `ACTION_RAID + 0.5` |

Register every new trigger and action name in `UldTriggerContext.h` / `UldActionContext.h` (creators
map **and** static factory), and remove the tower-hazard entries. Names resolve at runtime and fail
silently — a `TriggerNode` with no `creators[...]` entry is skipped with no warning, an unregistered
action only logs `A:<name> - UNKNOWN`. Cross-check every string in both directions.

### 7. Docs

Replace the Flame Leviathan section of `docs/raids/ulduar.md` (lines 85-98) with the verified facts
above, **including the derivations, not just the conclusions** — three wrong conclusions came out of
this area in one session (snare works / Blaze hurts us / crates are cosmetic), and "Tar does not slow
FL" without "because `62287` is `Mechanic 11` and `creature_immunities` row `-362` masks SNARE"
invites the next reader to try it again. Record the deferred items as known gaps.

Add to `docs/engine/raid-mechanics-lessons.md` under "Movement that oscillates": **a waypoint kite
must advance on approach, not on arrival.** Advancing once the mover reaches the node drives it into
the node — fatal when the node is a corner and the pursuer can cut the diagonal. Chamfer the corners
and switch while still a turn-radius out.

## Known gaps, deliberately not addressed

- **Boarding depends on the raid leader.** `FlameLeviathanVehicleNearTrigger` returns false unless
  `master->GetVehicle()` — the shape `raid-mechanics-lessons.md:53-54` names as a defect (Oculus
  `GroupFlyingTrigger`): one human who has not mounted freezes the raid.
- **Chopper pyrite ferry.** The `spell_vehicle_grab_pyrite` chopper branch exists and nothing uses
  it. Worth building only once the simpler loop is proven in-game.
- **Zero seat slack.** 10 seats for 10 players, 25 for 25. A bot that loses a boarding race is on
  foot in an arena where Flame Vents hits for 15000 at 500 yd. No retreat fallback.
- **`PlayerbotAI::CastVehicleSpell(uint32, float, float, float)`** is declared at `PlayerbotAI.h:558`
  and never defined, so ground-targeting is unavailable. Deliberate tar ignition needs it.

## Verification

The module cannot be compiled headless here, so verification splits.

**Static:**

1. Every registered string appears in both `creators[...]` and every `NextAction` / `TriggerNode`
   referencing it; the deleted tower-hazard names appear nowhere.
2. Every `dynamic_cast` in the multiplier names a concrete action class that exists.
3. No spell id remains a raw literal in the FL action/trigger files.
4. `availableVehicles` has exactly one definition.
5. Each helper is declared in `UldBossHelper.h` and defined exactly once in `UldBossHelper.cpp`.
6. Exactly one action can call `MoveTo` for a vehicle.

**In-game:**

1. Build, start a 10-man Ulduar with a bot raid, `.go` to the FL staging area.
2. Bots board before the pull and the raid can still drive in — the multiplier must be inert until FL
   is attackable.
3. Boarding fills two Demolishers + two Siege Engines first, then turrets, mech seats and choppers.
4. Watch a Flame Vents cycle (every 20s): exactly **one** siege engine casts Electroshock, the channel
   drops before completing, and a different one covers the next cycle.
5. Siege engines and choppers sit behind FL rather than trailing the raid leader; one chopper parks
   ahead of him leaving tar pools in his path; pools ignite when barrels and mortars land.
6. Demolishers reach their hurl band, Blue Pyrite climbs to 10 stacks and gets refreshed, and a
   starved demolisher detours to a crate and returns.
7. Gunners shoot only Mechanolifts near the arena, and crates get collected.
8. Take Pursued: the vehicle runs the **wall loop**, keeps one direction for the window, rounds
   corners without stopping in them, never turns back into FL; a siege engine only Steam Rushes with
   FL behind it; the demolisher gunner pops Increased Speed.
9. **Let the fight run past ~2 minutes** so Gathering Speed reaches 8+ stacks and FL is faster than a
   Siege Engine. A kite that only works while the vehicles are faster proves nothing.
10. **Confirm nothing freezes.** Any vehicle standing still while not parked-and-facing means the
    drive action is failing under the suppression multiplier.
11. Confirm no gunner ever steers — watch for a turret model detaching or sliding.
12. Re-run with `AiPlayerbot.UlduarFlameLeviathanHardMode = 1` and confirm hazard clearance still
    happens now that it lives inside the drive action, including while Pursued.
13. Ring nodes cannot be validated offline — `src/tools/navprobe` is still an unimplemented plan
    (`docs/plans/navprobe-offline-navmesh-tool/`). A vehicle stopping short of a node is the tell for
    an off-mesh destination; raise `ULDUAR_FL_KITE_WALL_INSET` if it happens.
