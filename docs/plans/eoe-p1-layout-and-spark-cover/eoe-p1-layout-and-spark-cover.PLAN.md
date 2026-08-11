# EoE (Malygos) round 11 — rotating P1 layout, Power Spark cover, P3 drake rotation & Flame Shield

## Context

Ongoing test-run-driven iteration on the Eye of Eternity playerbot strategy
(`modules/mod-playerbots/src/Ai/Raid/EoE/`, map 616). Four problems from the latest pull, all
confirmed against the core script
(`src/server/scripts/Northrend/Nexus/EyeOfEternity/boss_malygos.cpp` and `eye_of_eternity.h`):

1. **P1 positioning is one fixed north/south line.** Malygos does not always land north. He lands
   35 yd from `CenterPos` on `CenterPos.GetAngle(me)` — whatever bearing his idle circuit left him
   on — so on three pulls out of four the whole raid layout is rotated wrong relative to him.
2. **Nobody can shoot far-side Power Sparks.** They walk straight at Malygos at 6 yd/s and reach him.
3. **P3 drakes fire Engulf in Flames at 2 combo points** instead of stacking Flame Spike first.
4. **P3 Flame Shield is unreliable before Surge of Power**, and must not cancel a Static Field dodge.

A fifth item (Rogue/Mage/Paladin immunities to escape Vortex) was investigated and **dropped** — see
"Not doing" at the end.

### Root cause 1 — the P1 spots are absolute constants on one bearing

All four P1 spots (`EoEActions.h`) are hardcoded points on the north/south line through `CenterPos`
`{754.395, 1301.27}`. Expressed as signed offsets along the boss bearing they are:

| spot | constant | offset from centre |
|---|---|---|
| tank | `MALYGOS_MAINTANK_POSITION` | **+42** (towards the boss) |
| melee/healer stack | `MALYGOS_STACK_POSITION` | **+12** |
| ranged dps | `MALYGOS_RANGED_POSITION` | **−14** (away from the boss) |
| DK spark grip | `POWER_SPARK_GRIP_POSITION` | **−1** |

i.e. the existing layout is exactly this offset set rotated to bearing **90°**. Malygos never lands
on 90°. His four idle waypoints (`FourSidesPos`) sit on the diagonals:

| index | position | bearing from centre |
|---|---|---|
| 0 | `{686.417, 1235.52}` | **−135.95°** (−2.3729 rad) |
| 1 | `{828.182, 1379.05}` | **+46.51°** (+0.8117 rad) |
| 2 | `{681.278, 1375.796}` | **+134.45°** (+2.3467 rad) |
| 3 | `{821.182, 1235.42}` | **−44.60°** (−0.7783 rad) |

Bearing is fixed at pull time: `JustEngagedWith` puts the boss in combat and schedules
`EVENT_INTRO_MOVE_CENTER` at 0 ms, which snapshots `CenterPos.GetAngle(me)` and flies him radially
in to 35 yd; `EVENT_INTRO_LAND` then drops him straight down. So from the first tick bots see
(phase 4 + full health = the intro), the boss's bearing from centre is already the landing bearing.

Ground is fine on the diagonals: the Exit Portal sits at `{724.684, 1332.92}` = 43.4 yd out on
bearing 133.2°, and the Nexus Raid Platform GO is centred on `CenterPos`. The portal is phased out
by `DATA_HIDE_IRIS_AND_PORTAL` when the fight starts, so a tank spot at 42 yd cannot collide with it.

### Root cause 2 — the ranged line is out of range of every far-side spark

`npc_power_spark` spawns at one of the four `FourSidesPos` corners (94–107 yd from centre) and
re-issues `MovePoint(0, *malygos)` every 2 s at `speed_run` 0.85714 → **6.0 yd/s**. It hands Malygos
its buff at 12 yd (`IsWithinDist3d`) and dies to ~12k damage.

Malygos parks ~22.8 yd short of the tank spot, i.e. ~19 yd from centre. The ranged spot is 14 yd on
the *opposite* side, so ranged stand **~33 yd from the boss**. A spark closing from the boss's far
side is `33 + 12 ≈ 45` yd away at the moment it delivers the buff — it never enters the 28.5 yd
`spellDistance`, so ranged never get a cast off. Meanwhile the melee stack is ~7–12 yd from him and
is forbidden from touching sparks (`KillPowerSparkAction::isUseful` requires `IsRangedDps`).

The ranged spot cannot simply move in: `Spell::CheckRange` adds `GetMeleeRange` to the minimum for
`SPELL_RANGE_RANGED`, so Malygos' CombatReach of 20 inflates a hunter's 5 yd minimum to **~27.8 yd**
centre-to-centre. **Hunters** are the only ranged with a minimum range; casters have none.

Two further defects compound it: `MalygosTargetAction` (P1 branch) and `KillPowerSparkAction` both
take the **first** spark out of `possible targets no los` with **no distance check**, so a ranged bot
locks onto a spark 100 yd away and stands there casting nothing while the boss goes unhit.

### Root cause 3 — Engulf fires at 2 combo points

`EoEDrakeAttackAction::DrakeDpsAction` (`EoEActions.cpp`) uses `comboPoints >= 2`.

### Root cause 4 — a failed Flame Shield locks itself out for 30 s

`DrakeSurgeShieldAction::Execute` calls `botAI->CastVehicleSpell(SPELL_FLAME_SHIELD, drake)` and
stamps `drake->AddSpellCooldown(SPELL_FLAME_SHIELD, 0, 30000)` whenever it returns true. But
`PlayerbotAI::CastVehicleSpell` (`src/Bot/PlayerbotAI.cpp`) **returns true even when
`spell->prepare()` fails its `CheckCast`** — it only inspects the result in the `seat->CanControl()
&& isMoving() && GetCastTime()` branch. One silent failure therefore blocks the shield for the rest
of the phase's next 30 s.

The dodge interaction is safe but only by luck and is worth pinning down: Flame Shield is a
self-cast, so `CastVehicleSpell`'s `failWithDelay` turn-the-vehicle branch is skipped
(`spellTarget == vehicleBase`), and its `StopMoving()` branch needs a non-zero cast time. What does
cost a tick is `Execute` returning **true** on a cast — `eoe fly drake` (`ACTION_EMERGENCY`) sits
directly below `drake surge shield` (`ACTION_EMERGENCY + 5`) and is skipped for that tick.

Trigger side is already correct: `DrakeSurgeTrigger` reads the boss AI's guid slots
(`EOE_DATA_FIRST_SURGE_TARGET_GUID`), which hold the **drake** guid and are filled 3 s before the
beam in both 10-man (`SetGUID` in `EVENT_SPELL_PH3_SURGE_OF_POWER`) and 25-man (the 60939 warn
selector script). No change needed there.

## Changes

All paths relative to `modules/mod-playerbots/`.

### 1. Rotating P1 layout (`src/Ai/Raid/EoE/EoEActions.h`, `EoEActions.cpp`)

Replace the four position constants with offsets plus a resolved layout.

**`EoEActions.h`** — drop `MALYGOS_MAINTANK_POSITION`, `MALYGOS_STACK_POSITION`,
`MALYGOS_RANGED_POSITION`, `POWER_SPARK_GRIP_POSITION`; add the signed offsets (`+42`, `+12`, `−14`,
`−1`), the four landing bearings as a `float[4]` (values in the table above, with the source
`FourSidesPos` entries named in the comment — core script headers are not on a module's include
path, same reason `EOE_DATA_MALYGOS` is mirrored), and:

```cpp
struct MalygosP1Layout
{
    std::pair<float, float> tank;
    std::pair<float, float> stack;
    std::pair<float, float> ranged;
    std::pair<float, float> grip;
};

// The P1 hold spots for this pull. Malygos lands 35y from centre on whatever bearing his idle
// circuit left him on, so the whole layout is the same offsets rotated onto the nearest of his four
// waypoint bearings. Latched on the first resolve of the encounter and shared instance-wide, so the
// raid cannot end up half on one set and half on another, and cleared when the encounter resets.
MalygosP1Layout const& GetMalygosP1Layout(Player* bot);
```

**`EoEActions.cpp`** — implement it next to the other helpers, with a `thread_local
std::unordered_map<uint32 /*instanceId*/, …>` latch modelled on the `phaseCache` /`creatureCache`
pair in `EoETriggers.cpp` (a bot is only updated from its own map thread, so no lock). Resolve:

- `MalygosTrigger::getPhase(bot) == 0` → clear the latch for this instance and fall through to a
  live resolve (nothing reads the layout out of combat anyway).
- No latch yet → `MalygosTrigger::getMalygos(bot)`, bearing `atan2(bossY - cy, bossX - cx)`, pick the
  index of the landing bearing with the smallest wrapped angular difference, store it. Guard a boss
  within 1 yd of centre (cannot happen — he is 35 yd out) by keeping index 0.
- Build the four spots as `centre + offset * (cos θ, sin θ)`.

Then swap the call sites — all of them, there are only three:

- `MalygosPositionAction::Execute`, P1 branch (`EoEActions.cpp:~300-320`) — the four-way `spot`
  selection. The round-10 `MALYGOS_MELEE_HOLD_DISTANCE` clamp on the stack spot stays exactly as is;
  with the layout now rotated onto the boss it should almost never fire.
- `IsOnPowerSparkGripDuty` (`~181`, `~188`) — the two grip-spot distance tests.
- `PullPowerSparkAction::isUseful` (`~522`) — the grip-spot proximity gate.

### 2. Power Spark cover (`EoEActions.h`, `EoEActions.cpp`, `EoETriggers.cpp`)

**Split the ranged hold.** Only hunters need the far spot. In the P1 branch of
`MalygosPositionAction::Execute`, `rangedDps` becomes `rangedDps && bot->IsClass(CLASS_HUNTER)` for
choosing `layout.ranged`; every other ranged dps falls through to the stack alongside melee and
healers, ~7–12 yd from the boss and inside the melee hold clamp. Casters have no minimum range, and
the P1 multiplier already zeroes `FleeAction`, `RunAwayAction`, `CastBlinkBackAction` and
`CastDisengageAction`, so nothing tries to walk them back out. From the stack a spark is inside
28.5 yd for ~4–5 s before it reaches Malygos, from any bearing. No new movement, so no oscillation.

**Range-gate spark targeting, and let melee help.** Add a shared helper next to
`GetNearestPowerSpark`:

```cpp
// The spark this bot should be hitting: of the ones it can actually reach from where it stands, the
// one closest to Malygos, i.e. the one about to hand him his buff. nullptr when none is in reach - a
// bot that locks onto a spark 100y out just stands there while the boss goes unhit. Reach is the
// bot's own: spell range for ranged, melee range for everyone else, because nobody walks in P1.
Unit* GetPowerSparkToKill(PlayerbotAI* botAI);
```

Implemented over `GetEoECreatures(bot, NPC_POWER_SPARK, …)` (the instance-wide 300 ms guid cache).
The reach test is `bot->IsWithinCombatRange(spark, sPlayerbotAIConfig.spellDistance)` for ranged and
`bot->IsWithinMeleeRange(spark)` for melee — melee dps get to burn a spark that walks into reach on
its way to the boss, which on the stack is most of them, and drop back to Malygos when it leaves.
Add a couple of yards of slack on the melee side (a `POWER_SPARK_MELEE_STICKY` constant) so a spark
crossing the edge of reach at 6 yd/s does not make the bot flip target every tick. No movement is
involved either way: the P1 multiplier keeps `ReachTargetAction` zeroed for non-tanks, so a bot only
ever swings at what is already in front of it.

Use it in:

- `MalygosTargetAction::Execute` P1 branch — replaces the "first spark in `possible targets no los`"
  scan, and the `IsRangedDps` gate widens to "any dps that is not the boss tank". When the helper
  returns nullptr the bot stays on Malygos. **The tank never switches** — dropping the boss swings
  his Arcane Breath cone.
- `KillPowerSparkAction::isUseful` / `::Execute` — same, replacing both open-coded scans and the
  `IsRangedDps` gate.

**Cheaper trigger.** `PowerSparkTrigger::IsActive` (`EoETriggers.cpp`) swaps its
`possible targets no los` walk for `AnyEoECreature(bot, NPC_POWER_SPARK)`, matching the rest of the
round-7 caching work.

`GetNearestPowerSpark` stays as it is — the DK grip deliberately wants the nearest spark to the grip
spot, reachable or not.

### 3. P3 drake rotation (`EoEActions.h`, `EoEActions.cpp`)

Add `const uint8 DRAKE_ENGULF_COMBO = 3;` beside `DRAKE_LIFE_BURST_COMBO` and use it in
`EoEDrakeAttackAction::DrakeDpsAction` in place of the bare `>= 2`.

### 4. Flame Shield (`EoEActions.cpp`)

`DrakeSurgeShieldAction::Execute`:

- Only stamp the manual 30 s cooldown once the shield is actually up (`drake->HasAura(
  SPELL_FLAME_SHIELD)` after the cast) — otherwise leave it uncooled so the next tick retries inside
  the 3 s warning window. `CastVehicleSpell` reports success even when `Spell::prepare` rejects the
  cast, and the manual stamp exists only because the core does not cool a vehicle spell down by
  itself.
- Return **false** whether or not the cast landed, so `eoe fly drake` still gets the tick and a
  Static Field dodge keeps or re-issues its spline. Same pattern `MalygosPositionAction` and
  `EoEFlyDrakeAction` already use to hand a tick on.
- Comment why it is safe to fire mid-flight: the shield is a self-cast, so `CastVehicleSpell` never
  reaches its turn-the-vehicle delay or its `StopMoving()` branch (instant cast, zero cast time).

### 5. Docs (`docs/raids/eye-of-eternity.md`)

Reference documentation, so ordinary prose conventions apply. Update:

- The "P1 — three fixed hold spots on one north/south line" bullet → the rotating layout: the four
  landing bearings, the offsets, that the set is latched at the intro and shared instance-wide, and
  that the spots are still fixed *within* a pull (a spot that chases the boss live is what swept the
  Arcane Breath cone through the raid). Keep the `MALYGOS_MELEE_HOLD_DISTANCE` clamp passage.
- The `MALYGOS_RANGED_POSITION` bullet → hunters only, and why (the ~27.8 yd inflated minimum);
  casters on the stack, and why that is what lets anyone reach a far-side spark.
- The Power Spark bullet → the 6.0 yd/s walk-in, the 12 yd hand-off, the range gate and
  most-urgent-first pick, and that melee dps now help on any spark that walks into their reach while
  the tank stays on the boss.
- The P3 bullets → `DRAKE_ENGULF_COMBO`, and the Flame Shield verify-then-cool / hand-the-tick-back
  behaviour.

## Not doing — Vortex immunities

Dropped by decision after investigation. On this core the immunities cannot work:
`instance_eye_of_eternity.cpp::VortexHandling()` makes the **player** cast `SPELL_VORTEX_4` (55853)
on a Vortex trigger with `TRIGGERED_FULL_MASK`, which sets `TRIGGERED_IGNORE_CASTER_AURAS` and so
skips `CheckCasterAuras` entirely (`Spell.cpp:6065`); and `AuraEffect::HandleAuraControlVehicle`
applies the aura to the **trigger** and pulls the caster in, so there is no player-side aura for
Divine Shield's purge-on-immunity to remove. Bots would burn the cooldown and be vortexed anyway.
Making it work would need a core-side change to `VortexHandling()` to skip immune players. (Aside:
the rogue tool for this was Cloak of Shadows, not Shadowstep — Shadowstep never blocked Vortex.)

## Verification

Static first; the module cannot be compiled headless here, so the build and the in-game checks are a
hand-off.

1. **Static**: no new files and no wiring changes — `RaidStrategyContext.h`,
   `BuildSharedActionContexts.cpp`, `BuildSharedTriggerContexts.cpp`, `PlayerbotAI.cpp`,
   `EoEStrategy.cpp`, `EoEActionContext.h`, `EoETriggerContext.h` and `EoEMultipliers.*` are all
   untouched. Check brace balance, `awk 'length>120'` on the sources (only the pre-existing
   `EoEActions.h` `AvoidSurgeOfPowerAction` ctor line should flag) and `length>104` on the doc, and
   grep that no reference to the four removed position constants survives.
2. **Geometry** (arithmetic, no server): on each of the four bearings, tank at 42 yd is inside the
   platform (Exit Portal at 43.4 yd), hunter spot is 14 yd past centre → ~33 yd from a boss parked
   19 yd out, above the ~27.8 yd inflated minimum; stack at 12 yd → ~7 yd from him, and the round-10
   clamp holds it within 15 yd.
3. **P1, several pulls**: the raid layout must rotate to face wherever Malygos lands. Tank between
   raid and boss every time, melee in reach without walking, hunters shooting with no
   `SPELL_FAILED_TOO_CLOSE`, and no bot oscillating (`debug move` should show no repeated
   `MoveAway`/`blink back`/`disengage`). Two pulls landing on different bearings is the real test.
4. **Power Sparks**: pull until sparks spawn from each of the four corners. Every spark should die
   before reaching 12 yd of Malygos — watch for `SAY_BUFFED_BY_SPARK` / `SPELL_POWER_SPARK_MALYGOS_BUFF`
   (56152) on him, which is the failure signal. Bots must not target a spark they cannot reach, melee
   should pick one up as it walks past them and drop back to Malygos after, and the tank must stay on
   the boss throughout.
5. **P3 rotation**: drakes cast Flame Spike three times, then Engulf in Flames.
6. **P3 Flame Shield**: pick a drake, watch it get fixated by Surge of Power. Flame Shield must go up
   inside the 3 s window, and if it happens during a Static Field dodge the drake must keep flying to
   its new stack point rather than stopping. A drake that fails one attempt must retry, not go quiet
   for 30 s.
7. **Regression**: P2 (bubbles, Nexus Lords, hover disks) is untouched — spot-check that it still
   behaves as it did last round.
