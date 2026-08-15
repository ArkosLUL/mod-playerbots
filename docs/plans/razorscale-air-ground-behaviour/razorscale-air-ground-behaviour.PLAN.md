# Razorscale — air/ground behaviour fixes

## Context

Four behavioural defects in the Razorscale (Ulduar) playerbot strategy, reported from live raids:

1. Pets (hunter/warlock/shaman elemental) sit idle or stay on the boss while she is airborne
   instead of helping kill the Dark Rune adds.
2. When Razorscale lands, the raid keeps DPSing trash — the skull raid icon stays parked on an add.
3. Bloodlust and personal burst cooldowns are spent while she is airborne and takes no damage.
4. Bots step out of a Devouring Flame patch but immediately walk back into it.

All four are root-caused below. Existing strategy files:
[UldActions_Razorscale.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Razorscale.cpp),
[UldTriggers_Razorscale.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Razorscale.cpp),
[UldBossHelper.h:652-731](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.h#L652-L731),
[UldMultipliers.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp),
[UldStrategy.cpp:39-80](modules/mod-playerbots/src/Ai/Raid/Uld/UldStrategy.cpp#L39-L80).

### Encounter facts this plan relies on (verified against core + DBC)

- Air phase parks her at `RazorFlightPos` Z=456.84 or `RazorFlightPos2` Z=475.18; ground is
  Z=391.64, landing Z=408.51. The existing `RAZORSCALE_FLYING_Z_THRESHOLD = 440.0f` separates them
  correctly (`boss_razorscale.cpp:174-177`).
- `RazorFlightPos2` is (619.1, -238.1, 475.2) — over 100 yd 3D from most of the raid, i.e. **outside
  `AiPlayerbot.SightDistance` (100.0)**.
- Devouring Flame patch = NPC 34188 casting 64709, a 2 s periodic trigger of **64704 / 64733, whose
  damage radius index is 8 = 5.0 yards**. The current code clears only 3.5 yd, so bots stop while
  still burning.
- Adds spawn in the air phase only (`EVENT_SUMMON_MINIONS`, `PHASE_AIR`); harpoon knockdowns and the
  sub-50% perma-ground are both real burn windows.

## Root causes

| # | Symptom | Cause |
|---|---|---|
| 1 | Pets ignore adds | `PetAttackAction`'s trigger node is commented out globally (`CombatStrategy.cpp:53-55`). Razorscale has no pet handling at all, so pets keep their last target. |
| 2 | Skull stuck on trash | Two ungated markers: `RazorscaleAvoidSentinelAction` (`UldActions_Razorscale.cpp:147-168`) and `RazorscaleFocusCasterTrigger/Action` (`:722-734`) both set skull with no phase check, and nothing ever moves it onto the boss. `DpsTargetValue::Calculate` prefers the RTI target, so the whole raid follows the stale skull. |
| 3 | Lust fired while airborne | `UlduarBurstWindowMultiplier::EvaluateWindow` resolves the boss from `AI_VALUE(GuidVector, "possible targets no los")`, capped at 100 yd. At `RazorFlightPos2` she falls out of that sweep, the function reaches `return {}`, and `BurstWindow`'s default member initialisers are `allowAll = true; allowLust = true` (`UldMultipliers.h:80-84`) — so the gate opens instead of closing. |
| 4 | Walk back into fire | `MoveAway(flame, 3.5f)` steps 3.5 yd from the bot's *current* spot, inside the 5 yd damage radius; `isUseful()` then goes false, and `razorscale grounded` (ACTION_RAID) / `reach melee` / `combat formation move` immediately reclaim the tick and walk the bot back. Off-tanks skip the dodge entirely during the ground phase (`:69-70`). |

## Changes

### A. Helper — `Util/UldBossHelper.h` / `.cpp` (`RazorscaleBossHelper`)

Add constants and two statics (statics so multipliers can use them without `UpdateBossAI()`, which
reassigns the raid main tank):

```cpp
static constexpr float DEVOURING_FLAME_RADIUS       = 5.0f;  // 64704/64733 radius index 8
static constexpr float DEVOURING_FLAME_CLEAR_RADIUS = 7.0f;  // radius + step margin

static Unit* FindDevouringFlameNear(PlayerbotAI* botAI, float radius);
static bool  DevouringFlameBlocks(Player* bot, float x, float y);
```

- `FindDevouringFlameNear` — nearest alive entry-34188 unit within `radius` of the bot, scanned from
  `AI_VALUE(GuidVector, "nearest hostile npcs")` (same source the current action uses).
- `DevouringFlameBlocks` — mirrors `FissureBlocks`
  ([OSHelpers.cpp:585-608](modules/mod-playerbots/src/Ai/Raid/OS/OSHelpers.cpp#L585-L608)):
  `GetCreatureListWithEntryInGrid(found, 34188, DEVOURING_FLAME_CLEAR_RADIUS + bot->GetExactDist2d(x, y))`,
  return true if any is within `DEVOURING_FLAME_CLEAR_RADIUS` of the point. Ranks by distance to the
  *point*, which `FindUnitByEntries` cannot do.

### B. Devouring Flame dodge-and-hold — `RazorscaleAvoidDevouringFlameAction` (issue 4)

Rewrite `Execute`/`isUseful` as a two-band action:

1. **Step out** — if a flame is within `DEVOURING_FLAME_CLEAR_RADIUS` of the bot, walk to a
   destination clear of *every* flame. Iterate `MoveAway`-style candidate bearings (`init_angle ±
   k·π/8`) at step `CLEAR_RADIUS - currentDistance + 1.0f`, reject any candidate where
   `DevouringFlameBlocks` is true or `CheckCollisionAndGetValidCoords` fails, then `MoveTo(...,
   MOVEMENT_COMBAT)`. Return true.
2. **Hold** — else if the bot's current target exists and `DevouringFlameBlocks(bot, target->GetPositionX(), target->GetPositionY())`,
   return true **without moving**. Consuming the tick is what stops `razorscale grounded`,
   `reach melee`, `reach spell` and `combat formation move` from dragging the bot back; casts are
   unaffected. Releases the moment the tank has dragged the boss clear, so melee uptime returns.
3. Otherwise return false.

`isUseful()` must mirror exactly the same two conditions — the current version uses a different
main-tank multiplier than `Execute` (`:41` vs `:84`), which makes the MT "useful" at 8.05 yd and then
not move. Keep the main tank's 2.3× widening for the air phase only, computed once and shared.

Drop the off-tank ground-phase skip at `:69-70`: a 7 yd sidestep still leaves them stacked on the MT,
and `razorscale grounded` re-stacks them once the patch is gone.

Raise the node so fire outranks the spacing moves that could shove a bot back into it:

```cpp
// UldStrategy.cpp
{ NextAction("razorscale avoid devouring flames", ACTION_RAID + 4) }   // was ACTION_RAID + 1
```

### C. Movement guard — new `RazorscaleMultiplier` in `UldMultipliers.h` / `.cpp`

Scoped strictly to a live dodge (a permanent veto is the freeze bug called out in
[OSMultipliers.cpp:150-154](modules/mod-playerbots/src/Ai/Raid/OS/OSMultipliers.cpp#L150-L154)):

```cpp
// while FindDevouringFlameNear(botAI, DEVOURING_FLAME_CLEAR_RADIUS) is non-null:
//   0.0f for ReachTargetAction, CastReachTargetSpellAction, CombatFormationMoveAction,
//        RearFlankAction, FollowAction, AvoidAoeAction
```

`AvoidAoeAction` is included because it sits at `ACTION_EMERGENCY` (90), outranks every node here,
and flees with no knowledge of the other patches. Register in `RaidUlduarStrategy::InitMultipliers`
([UldStrategy.cpp:592-617](modules/mod-playerbots/src/Ai/Raid/Uld/UldStrategy.cpp#L592-L617)).
Ask the cheap `bot->GetMapId() != ULDUAR_MAP_ID` / `dynamic_cast<MovementAction*>` questions before
the flame scan, matching `FlameLeviathanVehicleMovementMultiplier`.

### D. Skull ownership — one marker, phase-aware (issue 2)

Rename `RazorscaleFocusCasterTrigger` / `RazorscaleFocusCasterAction` to
`RazorscaleKillTargetTrigger` / `RazorscaleKillTargetAction`, names
`razorscale kill target trigger` / `razorscale kill target action`, and make them the sole owner of
the skull:

- Grounded (`boss->GetPositionZ() <= RAZORSCALE_FLYING_Z_THRESHOLD`, so harpoon knockdowns count):
  desired skull target = **Razorscale**.
- Airborne: first alive Sentinel → Watcher → Guardian (Sentinels are the top kill priority; the
  Watcher is the caster).
- Trigger stays gated on `IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID)` and goes false when the
  group icon already sits on the desired target.
- Action: `MarkTargetWithSkull(bot, target)` + `SetRtiTarget(botAI, "skull", target)`.

Delete the skull block from `RazorscaleAvoidSentinelAction` (`:133-170`) — it keeps only the ranged
8 yd move-away, and its `isUseful()` loses the tank-always-true early returns at `:181-193`. Moon on
the boss during the air phase and its clearing on landing (`RazorscaleIgnoreBossAction` /
`RazorscaleGroundedAction`) are unchanged: moon is excluded from every DPS target scan, so it keeps
bots off her while she is untouchable, and skull takes over the instant she lands.

Rename the 4 wiring sites: `UldTriggers_Razorscale.h`, `UldActions_Razorscale.h`,
`UldTriggerContext.h` (creator + factory), `UldActionContext.h` (creator + factory), plus the
`UldStrategy.cpp` trigger node.

### E. Pets — new trigger/action pair (issue 1)

`RazorscalePetControlTrigger` (`razorscale pet control trigger`): boss resolvable via
`AI_VALUE2(Unit*, "find target", "razorscale")`, boss alive, and `bot->GetGuardianPet()` non-null.

`RazorscalePetControlAction` (`razorscale pet control action`):

- Airborne: target = nearest alive Sentinel → Watcher → Guardian; `CommandPetAttack(botAI, add)`.
  `StopPet(botAI)` if no add is up.
- Grounded: `CommandPetAttack(botAI, boss)`.
- **Always `return false`** so the tick falls through to the dodge and positioning nodes — the
  command is a side effect, not a movement. Same shape as the pet block in
  [OSActions.cpp:504-523](modules/mod-playerbots/src/Ai/Raid/OS/OSActions.cpp#L504-L523), which runs
  ahead of its own early return.

`CommandPetAttack` ([RaidBossHelpers.cpp:333-362](modules/mod-playerbots/src/Ai/Raid/RaidBossHelpers.cpp#L333-L362))
already no-ops when the pet is on target, respects `REACT_PASSIVE`, and covers hunter/warlock pets,
shaman elementals and DK ghouls via `GetGuardianPet()`.

Wire at `ACTION_RAID + 5` — highest of the Razorscale nodes, harmless because it never consumes the
tick.

### F. Burst window — `UlduarBurstWindowMultiplier::EvaluateWindow` (issue 3)

Two changes in [UldMultipliers.cpp:257-298](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L257-L298):

```cpp
// after the "possible targets no los" sweep, before the razorscale branch:
if (!razorscale)
    razorscale = AI_VALUE2(Unit*, "find target", "razorscale");   // threat list, no distance cap
```

`FindTargetValue` walks `GetThreatenedByMeList()`, and `DoZoneInCombat()` on her flight point puts
the whole raid on her threat table — so she resolves at any range, unlike the 100 yd sweep. It does
**not** touch `UpdateBossAI()`, so the tank-reassignment hazard documented at `ulduar.md:568-570`
does not apply.

Then collapse the Razorscale arm to release lust on any landing (your call — knockdowns included):

```cpp
if (razorscale)
{
    bool const grounded = razorscale->GetPositionZ() <= RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD;
    return {grounded, grounded};
}
```

`IsGroundPhaseFor` stays in use by `UldThreatRedirectMultiplier` — do not remove it.

## Files touched

| File | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` | Flame radius constants, `FindDevouringFlameNear`, `DevouringFlameBlocks` |
| `src/Ai/Raid/Uld/Action/UldActions_Razorscale.h` / `.cpp` | Rewrite flame dodge; strip skull from sentinel action; rename focus-caster → kill-target and make it phase-aware; new pet-control action |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_Razorscale.h` / `.cpp` | Rename + rework kill-target trigger; new pet-control trigger |
| `src/Ai/Raid/Uld/UldTriggerContext.h`, `UldActionContext.h` | Rename two creators/factories, add two |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | Re-prioritise the flame node, rename the kill-target node, add the pet node, register `RazorscaleMultiplier` |
| `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp` | New `RazorscaleMultiplier`; burst-window boss fallback + lust arm |
| `docs/raids/ulduar.md` | Update the burst table row (`:556`), the Sev-1 gap row (`:626`, now stale — focus and Flame Breath both exist), and add the flame clear-radius / skull-ownership facts |

## Verification

Compiling this module headless is not possible here, so hand-off is: static review + an in-game run.

Static, before hand-off:
- `grep -rn "razorscale" src/Ai/Raid/Uld/Uld{Trigger,Action}Context.h src/Ai/Raid/Uld/UldStrategy.cpp`
  — every trigger and action name must appear in a creator, a factory and (for nodes) a `TriggerNode`.
  A missing creator is a silent no-op at runtime, not a compile error.
- Confirm nothing else references `razorscale focus caster` after the rename.
- Confirm no multiplier path reaches `RazorscaleBossHelper::UpdateBossAI()`.

In-game, one Razorscale pull (10-man is enough; 2 harpoons):
1. **Air phase** — pets engage Dark Rune adds within a few seconds of each spawn wave; skull sits on
   a Sentinel (or Watcher/Guardian when none), moon on the boss; no bot casts Bloodlust, Berserking,
   trinkets or class burst. Verify from the second air phase too, when she sits at `RazorFlightPos2`
   past the 100 yd sweep — that is the case the old code missed.
2. **Harpoon knockdown** — skull flips to Razorscale, moon clears, pets switch to her, lust and
   personal cooldowns fire.
3. **Devouring Flame** — a bot caught in a patch steps to ≥7 yd and stays out for the patch's whole
   life; no oscillation in and out. Watch a melee DPS and the off-tank specifically, and confirm they
   re-engage once the tank has dragged the boss off the patch rather than standing out the timer.
4. **Perma-ground (<50%)** — whole raid on the boss, remaining adds die to cleave.

Enable `AiPlayerbot.TellWhenAvoidAoe = 1` during the test run for dodge chatter, and watch the
worldserver console for the `"<name> set as main tank!"` yell — an unexpected burst of those means a
multiplier or trigger reached `UpdateBossAI()`.

## Save location

Before implementation, copy this plan to
`modules/mod-playerbots/docs/plans/razorscale-air-ground-behaviour/razorscale-air-ground-behaviour.PLAN.md`.
