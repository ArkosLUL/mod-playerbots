# EoE (Malygos) playerbot fixes — P1 ranged DPS, P2 targeting/pets, P2 disk diving

## Context

Four in-raid problems observed on the `wotlk-eoe` strategy (`modules/mod-playerbots/src/Ai/Raid/EoE/`):

1. **P1 ranged bots deal far too little damage.** They keep trying to back off the boss and get
   dragged back to their hold spot.
2. **P2 ranged DPS should hit Nexus Lords first** (if reachable), Scions only as a fallback, and must
   never walk out of their Arcane Overload bubble to do it.
3. **P2 hunter/warlock pets cannot touch Scions of Eternity** — the Scions hover 20-30y up — so pets
   must always be pointed at Nexus Lords instead.
4. **P2 melee on Hover Disks dive to the platform floor** right after reaching a Scion, then climb
   back up, over and over.

Root causes were confirmed against the core and module sources; each is a distinct bug.

### Root cause 1 — P1 ranged (two compounding bugs)

`MALYGOS_STACK_POSITION` (`EoEActions.h:31`) is `{754.395, 1313.27}`. Malygos parks ~21.5y short of
the tank at `{754.395, 1343.27}`, i.e. around **y≈1322**, so everyone on the stack sits **~8.5y from
his centre**.

* **Hunters cannot shoot at all.** `creature_model_info` gives Malygos `CombatReach = 20`.
  `Spell::CheckRange` (`src/server/game/Spells/Spell.cpp`) for `SPELL_RANGE_RANGED` computes
  `minRangeCombined = min_range + m_caster->GetMeleeRange(target)` = `5 + (1.5 + 20 + 4/3)` ≈ **27.8y**,
  then fails with `SPELL_FAILED_TOO_CLOSE` if `IsWithinRange(target, 27.8)`. At 8.5y every hunter shot
  is rejected.
* **Everyone else ping-pongs.** `EnemyTooCloseForSpellTrigger` (`src/Ai/Base/Trigger/RangeTriggers.cpp:14`)
  uses `target->IsWithinCombatRange(bot, MIN_MELEE_REACH)`, which adds both combat reaches → threshold
  **~23.5y**. It fires constantly on the stack. It is wired to high-relevance escape actions:
  `RangedCombatStrategy.cpp:14` → `flee` @34, `RunawayStrategy.cpp:13` → `runaway` @50,
  `BalanceDruidStrategy.cpp:202` → `flee` @39, `RestoShamanStrategy.cpp:68` → `flee` @39,
  `GenericMageStrategy.cpp:152` → `blink back` @35, `GenericHunterStrategy.cpp:105` → `disengage` @35,
  priest/warlock/resto-druid equivalents. All outrank `MalygosPositionAction` (`ACTION_MOVE` = 30), so
  the bot steps out, the position action drags it back next tick, and it never finishes a cast.
  `MalygosMultiplier` phase 1 (`EoEMultipliers.cpp:28-60`) does **not** zero any of these (phase 2 does
  zero `FleeAction`).

### Root cause 2 — P2 target priority is inverted

`EoEActions.cpp:254`: `Unit* newTarget = (botAI->IsRangedDps(bot) && scionOfEternity) ? scionOfEternity : nexusLord;`
— Scions win over Nexus Lords for ranged DPS, the opposite of what's wanted. The in-reach gate
(`EoEActions.cpp:217`, `sPlayerbotAIConfig.spellDistance` = 28.5) already prevents leaving the bubble,
but it measures `GetExactDist2d`, which ignores the Scions' 20-30y altitude and so overstates reach.

### Root cause 3 — pets are never redirected

There is zero pet handling in `src/Ai/Raid/EoE/`. The generic pet-attack trigger is commented out
(`src/Ai/Base/Strategy/CombatStrategy.cpp:53-55`), so pets just assist their owner's target and chase
an unreachable airborne Scion forever. The raid-facing helpers already exist:
`CommandPetAttack(PlayerbotAI*, Unit*)` and `StopPet(PlayerbotAI*)` in
`src/Ai/Raid/RaidBossHelpers.h` (impl `RaidBossHelpers.cpp:331-375`); precedent caller
`src/Ai/Raid/SSC/SSCActions.cpp:1966`.

### Root cause 4 — generic melee chase drives the disk into the ground

`MalygosMultiplier` phase 2 (`EoEMultipliers.cpp:81-86`) deliberately exempts vehicle riders from the
movement suppression. So once `MalygosRideDiskAction` parks next to a Scion and calls `Attack(scion)`,
the generic `ReachMeleeAction` fires. Its `ReachCombatTo` (`MovementActions.cpp:816+`) → the
`MoveTo(WorldObject*, …)` overload calls **`bot->UpdateAllowedPositionZ(dx, dy, dz)`**
(`MovementActions.cpp:796`), which clamps Z to the terrain under the bot — ~266 on the platform. That
ground Z then goes into `MovementAction::MoveTo`'s vehicle branch (`MovementActions.cpp:190-213`),
which flies the **disk** there. Next tick `MalygosRideDiskAction` sees `closestDist > 5` and climbs
back up. `CombatFormationMoveAction`, `AvoidAoeAction` and `SetBehindTargetAction` can do the same.

Secondary: `mm->MovePoint(0, tx, ty, scion->GetPositionZ())` at `EoEActions.cpp:555` leaves
`generatePath` at its default `true`, i.e. a terrain (2D mmap) path — the module's own convention is
`generatePath = !vehicleBase->CanFly()` (`MovementActions.cpp:194`, and see the comment in
`DoMovePoint`, `MovementActions.cpp:1857`).

Confirmed from the core script (`src/server/scripts/Northrend/Nexus/EyeOfEternity/boss_malygos.cpp:1104-1114`):
Scion disks orbit at radius 30 from `CenterPos`, descending 2y per waypoint but **floored at
`CenterPos.z + 20`** — so the user's observation that Scion Z stops changing is correct, and a fixed
target Z is safe.

## Changes

All files under `modules/mod-playerbots/src/Ai/Raid/EoE/` unless stated.

### 1. New P1 ranged-DPS hold spot

**`EoEActions.h`** — add next to the existing spots:

```cpp
// P1 ranged dps spot, 14y south of centre. Malygos stops about 21.5y short of the tank (y~1322), and
// his CombatReach of 20 pushes a hunter's 5y minimum range out to roughly 28y of centre distance
// (Spell::CheckRange adds GetMeleeRange for SPELL_RANGE_RANGED). From here that is ~34.5y, so a bot
// drifting the full MALYGOS_P1_POSITION_TOLERANCE toward him can still shoot.
const std::pair<float, float> MALYGOS_RANGED_POSITION = {754.395f, 1287.27f};
```

**`EoEActions.cpp`**, `MalygosPositionAction::Execute`, phase 1 branch (~line 110-136): pick the spot
three ways instead of two.

```cpp
std::pair<float, float> const& spot = isBossTank  ? MALYGOS_MAINTANK_POSITION
                                    : botAI->IsRangedDps(bot) ? MALYGOS_RANGED_POSITION
                                                              : MALYGOS_STACK_POSITION;
```

Healers keep `MALYGOS_STACK_POSITION`: at y=1313 they are 30y from the tank, inside a 40y heal range,
and 26y from the new ranged spot. The ranged spot is 14y from centre, well inside the 30y anti-fall
ring used in phases 2/4, and inside the Exit Portal's 43.4y ground radius. Malygos faces north at the
tank, so the Arcane Breath cone still points away from both stacks.

### 2. Stop the P1 escape ping-pong

**`EoEMultipliers.cpp`**, phase 1 branch — add, modelled on
`src/Ai/Raid/ZA/ZAMultipliers.cpp:44-64`:

```cpp
// Malygos' CombatReach of 20 keeps "enemy too close for spell" active for anyone on the stack
// spots, and every class wires that trigger to an escape at 34-50 relevance - above
// MalygosPositionAction. Left alone the bot steps out, gets dragged back next tick and never
// finishes a cast.
if (dynamic_cast<FleeAction*>(action) || dynamic_cast<RunAwayAction*>(action) ||
    dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action))
{
    return 0.0f;
}
```

Includes: `MageActions.h` (`CastBlinkBackAction`) and `HunterActions.h` (`CastDisengageAction`);
`FleeAction`/`RunAwayAction` come from the already-included `MovementActions.h`.

### 3. Hunters may peel onto Power Sparks

**`EoEActions.cpp`**, `KillPowerSparkAction::isUseful` (~line 313): drop the
`if (bot->IsClass(CLASS_HUNTER)) { return false; }` line and the "Hunters stay on the boss" sentence
from the comment above it. The exclusion existed because a hunter parked on the stack could not shoot
anything; from the new spot they can.

### 4. P2 target priority — Nexus Lord first for ranged DPS

**`EoEActions.cpp`**, `MalygosTargetAction::Execute` phase 2 branch:

* Change the reach comparison from `bot->GetExactDist2d(unit)` to **`bot->GetExactDist(unit)`** (both in
  the candidate scan ~line 230 and in the sticky-target check ~line 263). Scions hover 20-30y up; a 2D
  test claims reach the bot does not have.
* Replace line 254 with:

```cpp
// Nexus Lords are on the ground and die faster; take one whenever it can be hit from where the bot
// already stands. Scions never land, so they are the ranged-only leftover.
Unit* newTarget = nexusLord;
if (!newTarget && botAI->IsRangedDps(bot)) { newTarget = scionOfEternity; }
```

* Update the comment above the `reach` gate to say it is a 3D distance.

The existing sticky-target rule needs no change: it only holds when
`currentTarget->GetEntry() == newTarget->GetEntry()`, so a bot on a Scion switches the moment a Nexus
Lord comes into reach. The existing "nothing in reach" fallback (~line 247) stays as-is so the class
rotation is still armed.

Bubble discipline needs no new code: the reach gate keeps the bot's target hittable from where it
stands, and `EoEMultipliers.cpp:81-86` already zeroes `ReachTargetAction`/`FollowAction` for
non-vehicle ranged and healers in P2.

### 5. P2 pets always on Nexus Lords

**`EoEActions.cpp`** — `#include "RaidBossHelpers.h"`. At the **top** of the phase 2 branch of
`MalygosTargetAction::Execute`, before any early return (including the `IsHeal` and `GetVehicle`
ones, so a disk rider's ghoul is handled too):

```cpp
// Scions hover 20-30y up and no pet can reach them, so pets stay on the grounded Nexus Lords
// whatever their owner is shooting at. CommandPetAttack no-ops when the pet is already on target.
if (Unit* lord = bot->FindNearestCreature(NPC_NEXUS_LORD, 250.0f, true))
{
    CommandPetAttack(botAI, lord);
}
else
{
    StopPet(botAI);
}
```

Nearest-anywhere, not in-reach: pets travel on their own. `CommandPetAttack` already no-ops when there
is no pet or when `pet->GetVictim() == target`, so this is cheap and tick-safe. Caveat to accept: on
ticks where a higher-relevance action wins (bubble seek @92, surge @90, disk @64) `malygos target`
does not run, so redirection can lag a tick or two.

### 6. P2 disk riders — kill the dive

**`EoEMultipliers.cpp`**, phase 2 branch — add before the existing ranged/healer clause:

```cpp
// A disk rider's chase actions move the *disk*: ReachCombatTo runs the endpoint through
// UpdateAllowedPositionZ, which clamps Z to the platform floor, so the disk dives 25y to the ground
// and MalygosRideDiskAction has to climb back up. MalygosRideDiskAction is an AttackAction that
// steers the vehicle itself, so nothing else may move this bot.
if (bot->GetVehicle() &&
    (dynamic_cast<MovementAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action)))
{
    return 0.0f;
}
```

`MalygosBoardDiskAction` is an `EnterVehicleAction` (a `MovementAction`) but is unaffected — the gate
is `bot->GetVehicle()`, which is null while boarding. `ReachTargetActions.h` is already included;
`MovementActions.h` too.

**`EoEActions.cpp`**, `MalygosRideDiskAction::Execute` — make both `MovePoint` calls straight 3D
splines instead of terrain paths, matching `MovementAction::DoMovePoint`:

```cpp
mm->MovePoint(0, tx, ty, scion->GetPositionZ(), FORCED_MOVEMENT_NONE, 0.f, 0.f,
              /*generatePath*/ false, /*forceDestination*/ true);
```

Same for the descent call at ~line 508. Add a short comment on the first one: the disk flies, so a
mesh path would snap the destination to the platform. (Signature:
`MotionMaster::MovePoint(uint32 id, float x, float y, float z, ForcedMovement, float speed, float orientation, bool generatePath, bool forceDestination, …)`,
`src/server/game/Movement/MotionMaster.h:242`.)

### 7. Docs

**`docs/raids/eye-of-eternity.md`** (96 lines) — the "Per-phase behaviour" section is already stale (it
describes an `avoid arcane breath` action that does not exist and an old tank-hold rule). Update the P1
paragraph for the three-spot layout and the min-range reason, the P2 paragraph for the Nexus-Lord-first
priority and pet redirection, and add the disk-rider movement lockout to "Fragile by design".

## Verification

Static, then in-game — the module cannot be compiled headless here.

1. **Build**: `RaidEoEStrategy` sources only; no new files, no registration changes (all four wiring
   sites in `RaidStrategyContext.h`, `BuildSharedActionContexts.cpp`, `BuildSharedTriggerContexts.cpp`,
   `PlayerbotAI.cpp` stay untouched). Confirm the new includes resolve: `MageActions.h`,
   `HunterActions.h` in `EoEMultipliers.cpp`; `RaidBossHelpers.h` in `EoEActions.cpp`.
2. **Geometry check** (arithmetic, no server needed): boss ≈ y 1322; ranged spot y 1287.27 → 34.7y;
   worst-case 5y drift north → 29.7y > 27.8y min range. Ranged spot to centre = 14y < 30y ring.
   Healer (y 1313.27) to tank (y 1343.27) = 30y < 40y.
3. **P1 in-game**: pull Malygos with a hunter, a mage and a healer in the raid.
   * Hunters visibly auto-shoot and Steady Shot; no `SPELL_FAILED_TOO_CLOSE` in the log.
   * Nobody oscillates: with `debug move` on, `MoveAway`/`blink back`/`disengage` should not appear for
     bots holding either stack.
   * Spawn a Power Spark; hunters now peel onto it with the other ranged.
4. **P2 in-game**: check with `.debug` / bot target inspection that ranged DPS target a **Nexus Lord**
   while one is within ~28.5y in 3D, and only fall to a Scion after the Lords die or move out of reach.
   Confirm ranged bots stay inside their Arcane Overload bubble the whole time.
5. **P2 pets**: a hunter or warlock bot's pet stays on a Nexus Lord even while its owner shoots a
   Scion; when the last Lord dies the pet stops rather than chasing skyward.
6. **P2 disks**: watch a melee bot board a disk. It should climb once, park ~3y from a Scion at the
   Scion's altitude, and stay there attacking — no descents until every Scion is dead, at which point
   it flies to `MALYGOS_CENTER_POSITION` at Z 266.1 and dismounts.
7. **Regression**: P3 (drakes) and P4 (transition gather) are untouched — spot-check that the drake
   flight formation and the centre gather still work.

---

# Round 2 — findings from the first test run

Applied on top of the above. Durable detail lives in `docs/raids/eye-of-eternity.md`.

1. **Regression: melee boarded disks and sat still.** `AttackAction` derives from `MovementAction`,
   so the new P2 vehicle lockout zeroed `MalygosRideDiskAction` (an `AttackAction`) along with the
   generic chases. The lockout now names `MalygosRideDiskAction`, `MalygosTargetAction` and
   `LeaveVehicleAction` as exemptions.
2. **DK Death Grip fed Malygos the spark.** Grip pulls to the caster and `npc_power_spark` buffs him
   at 12 yd, so a dps DK on the melee stack delivered it. `PullPowerSparkAction` now needs the bot
   ≥ 24 yd from Malygos *and* further out than the spark.
3. **Lag.** `MalygosMultiplier::GetValue` resolved the phase once per action, each resolve costing up
   to three 250 yd grid sweeps. Now: 500 ms per-bot memo (multipliers are per-bot, no locking),
   `getMalygos` via `InstanceScript::GetCreature(DATA_MALYGOS)`, add searches down to 100 yd.
4. **Static Field killed bots.** Three causes: the avoid action sat out any in-flight
   `POINT_MOTION_TYPE` move (including the dps range-close), it dodged only the nearest of two live
   fields, and the follow formation flew drakes straight back in. Rewritten with a self-owned flee
   latch, all-fields clearance sampling (16 headings, shortest workable hop), and multiplier
   suppression of `EoEFlyDrakeAction` plus the `DrakeDpsAction` range-close while a field is inside
   20 yd.
