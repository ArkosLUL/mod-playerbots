# Ignis the Furnace Master — guide parity, direct targeting, no RTI

## Context

The Ulduar strategy's Ignis block implements only the construct-disposal loop. Measured against the
encounter script it is missing every positioning decision the fight is built on, two of its hazard
radii are smaller than the spells they dodge, and its raid focus is routed through the group's skull
raid-target icon.

Three things are wanted: bring the encounter to guide parity, drop the skull/RTI dependency entirely
in favour of SWP-style direct `Attack()` targeting, and reuse SWP's movement idioms for positioning.
The design below is the settled output of a grilling session — every branch was put to the user and
answered.

The decisive fact, taken from the core script rather than any guide: **Scorched Ground spawns 20 yd
along Ignis' facing, and Ignis faces the main tank.** The main tank's position therefore chooses
where every fire patch in the fight lands — and today nothing in the module chooses it.

**Step 0 of implementation:** copy this document to
`modules/mod-playerbots/docs/plans/ignis-guide-parity/ignis-guide-parity.PLAN.md` (project planning
directory convention). On completion, fold the durable parts into `docs/raids/ulduar.md` and delete
the plan.

---

## Verified encounter facts

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_ignis.cpp`, `Spell.dbc` via
`modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv`, and `acore_world`.
These override anything a guide says.

### Timers (from `JustEngagedWith` / `UpdateAI`)

| Event | First | Repeat | Notes |
|---|---|---|---|
| Scorch (62546) | 10 s | 20 s | Boss rooted + rotation disabled 3 s; patch spawns at the **end** of the 3 s, from the orientation frozen at cast start |
| Flame Jets (62680) | 32 s | 25 s | 2.7 s cast, observable (`CastSpell(victim, …, false)`) |
| Activate Construct (62488) | 40 s / 30 s | same | 10-man / 25-man; **one** construct per cast; 20th cast → Berserk (64238) |
| Grab → Slag Pot | 25 s | 24 s | Delays all other events 6 s |

### Spells and radii

| Spell | Id | Effect |
|---|---|---|
| Scorch | 62546 | 3 s self aura on the boss; at its last tick summons Scorched Ground (33123) at `boss + 20 yd · u(boss orientation)`, z 361.0, 30 s despawn. **Skipped entirely if that point is within 25 yd of a water trigger** |
| Scorched Ground | 62548 | On the patch NPC: triggers 62549 (10-man, 1885) / 63475 (25-man, 3016) fire damage in **13 yd**, and 62343 → Heat (65667) on the boss's allies in **10 yd** |
| Heat | 65667 | 5 s, stacking, +5% speed/haste per stack; at 10 stacks the construct casts Molten and **wipes its threat table** |
| Molten | 62373 | **30 s duration.** Pulses 62530 (~1885, ~7 yd) |
| Brittle | 62382 (10) / 67114 (25) | **15 s.** Applied by the construct's own 1 s poll when it is Molten and within **18 yd** of water trigger 22515 |
| Shatter | 62383 | Procs on a single hit ≥ **5000** (62382) / **3000** (67114). **18850 damage in 13 yd**, kills the construct, removes one Strength stack |
| Flame Jets | 62680 | Radius 50000 (raid-wide): 5655 + 1000/s for 6 s + knockback that **locks casting for 6 s** |
| Slag Pot | 62717 (10) / 63477 (25) | 10 s vehicle ride. Core never picks the boss's victim or any construct's victim, so **neither tank can be potted** |
| Strength of the Creator | 64473 | +20% boss damage per living activated construct |

### Geometry and movement (`acore_world`, map 603)

```
Water trigger 22515 west  (526.771, 277.796, 360.802)
Water trigger 22515 east  (646.771, 277.796, 360.802)
Ignis 33118 spawn         (586.542, 378.798, 360.923)  orientation 4.7822
Iron Construct 33121      20 spawns: x = 543.1 (west row) and x = 630.4 (east row), y 216.8 … 337.5
Boss leash box            x 490–690, y 130–410 (outside → EnterEvadeMode)

speed_run: Ignis 1.42857 -> 10.0 yd/s   Iron Construct 1.28571 -> 9.0 yd/s   player 7.0 yd/s
```

`CombatReach`: Ignis **8.0**, Iron Construct 1.75. Melee range against Ignis is
`max(8.0 + ~1.5 + 4/3, 5) ≈ 10.8 yd` centre-to-centre — every stand-off distance below is sized
against that, not against 5.

**Ignis outruns a player.** He stays glued to a tank moving at full run speed, so the in-combat drag
needs no per-tick throttle — a melee-range leash check is enough.

---

## Gap analysis — current code vs the above

Files: `src/Ai/Raid/Uld/{Action/UldActions_Ignis,Trigger/UldTriggers_Ignis,Util/UldBossHelper}.*`,
`UldStrategy.cpp:89-122`, `UldMultipliers.cpp:154-175`.

| # | Gap | Evidence |
|---|---|---|
| 1 | **No positioning of any kind.** No main-tank spot, no facing control. Patches land wherever the boss happens to face — including on the raid, and including inside the 25 yd water exclusion where they never light, which stalls the construct loop indefinitely | no positioning action exists |
| 2 | **Scorch dodge radius too small.** `ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS = 8.0f` against a **13 yd** damage radius — bots flee to a distance still inside the fire | `UldBossHelper.h:427` |
| 3 | **Molten avoid radius too small.** `ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS = 12.0f` against Shatter's **13 yd** — every bot at the "safe" distance eats ~18.8k when the construct dies | `UldBossHelper.h:437` |
| 4 | **Flame Jets unhandled**, so every caster eats a 6 s lockout every 25 s | absent from the code |
| 5 | **Raid focus goes through the group skull icon** | `UldActions_Ignis.cpp:92-93`, `UldTriggers_Ignis.cpp:58,86`, `UldStrategy.cpp:113-114` → generic `"attack rti target"` |
| 6 | **Melee are told to hit Brittle constructs**, putting them inside the 13 yd Shatter blast | `IgnisAttackBrittleConstructTrigger` excludes tanks only |
| 7 | **`ignisTankDrivenConstructGuid` is a process-wide static** keyed only by tank GUID — shared across instances, never cleared on wipe or map change | `UldBossHelper.cpp:626` |
| 8 | **Trigger and action read different sources.** The trigger uses `FindNearestCreature` (grid); the inherited `MoveAwayFromCreatureAction` reads the LOS/sight-filtered `"nearest npcs"`. The trigger can stay hot while the action can never succeed | `UldTriggers_Ignis.cpp:29` vs `MovementActions.cpp:2837` |
| 9 | **No combat gate** — every node fires on "Ignis alive within 200 yd", so bots mark and dodge while merely walking past | every `IsActive()` in `UldTriggers_Ignis.cpp` |
| 10 | **One construct tank, no pool split** — `GetIgnisConstructTank` is a bare `GetGroupAssistTank(botAI, bot, 0)` | `UldBossHelper.cpp:765` |
| 11 | `SetRtiTarget`'s `"rti target"` write is a **no-op**: `RtiTargetValue` is a `CalculatedValue` with `checkInterval == 1`, so `Get()` recalculates and discards the `Set()`. Only `Group::SetTargetIcon` ever persisted | `RaidBossHelpers.cpp:95-108`, `Value.h:71-85` |

Ignis has no hard mode; 10/25 is handled by checking both spell ids.

---

## Design

### D1 — No skull, anywhere

**`IgnisBrittleConstructMarkAction` / `IgnisBrittleConstructMarkTrigger` are the only things in the
codebase that put a skull on anything Ignis-related** (audited: `BossMarkSkullAction` is wired for
VoA only; nothing else in `src/Ai/Raid/Uld` touches an icon for this boss). Delete both classes,
their context registrations, their strategy node, and the `"attack rti target"` node. Drop the
`RtiValue.h` / `RtiTargetValue.h` includes from both Ignis files.

Nothing replaces it — no icon is set, cleared, or read for this encounter. A stale skull left over
from earlier trash stays cosmetic, because `AttackRtiTargetAction` is zeroed for the whole fight
(D5). If a skull is still seen moving around during Ignis after this change, the source is the
raid-wide `mark rti` strategy (`MarkRtiStrategy.cpp:13`), which is a global bot strategy and out of
scope here.

### D2 — Layout, and how the raid gets there

**No pre-positioning.** Nothing moves before the pull; the human leader decides where the pull
happens. All positioning is in-combat only, and every trigger below carries a
`boss->IsInCombat()` gate.

Constants go in `UldBossHelper.h/.cpp` next to the existing pool positions.

```
IGNIS_BOSS_ANCHOR         (587.5, 277.8, 360.8)   room centre, midway between the pools
IGNIS_TANK_BEARING        3*pi/2 rad (south, -y)   patch fan points away from both pools and the raid
IGNIS_TANK_RADIUS         9.5f                     inside melee range (10.8) with slack
IGNIS_TANK_ARC_SLOTS      3 slots at -60 deg, 0, +60 deg off the bearing
IGNIS_ANCHOR_TOLERANCE    25.0f                    beyond this the tank is still in transit
IGNIS_SCORCH_SPAWN_RANGE  20.0f                    patch offset along the frozen boss orientation
```

**Why south.** It separates the three things that must not collide onto three axes: patches south,
pools east/west, raid north (emergent — the raid enters from Ignis' spawn at y 378.8, north of the
anchor). Landing points and their clearances, with the boss settled at the anchor:

```
slot 0 (240 deg)  patch (577.5, 260.5)   -> west pool 53.6 yd   -> east pool 71.6 yd
slot 1 (270 deg)  patch (587.5, 257.8)   -> west pool 63.9 yd   -> east pool 62.6 yd
slot 2 (300 deg)  patch (597.5, 260.5)   -> east pool 52.2 yd   -> west pool 72.2 yd

every patch > 25 yd from both pools, so every patch lights
patch -> raid (north of the boss)  40+ yd
construct walk patch -> assigned pool  ~53 yd at 9 yd/s = ~6 s, against a 30 s Molten window
leash box y 130-410; patches sit at y 257.8-260.5, well inside
```

**Two phases, one rule.**

*Transit* — boss more than `IGNIS_ANCHOR_TOLERANCE` from the anchor. The main tank runs to his arc
slot with a plain forward `MoveTo` at `MOVEMENT_COMBAT`, leash-gated: skip the step while
`!bot->IsWithinMeleeRange(boss)` and let the boss catch up. Ignis at 10 yd/s outruns the tank at 7,
so this is a no-op in practice and the boss never drops threat. Spawn to anchor is 101 yd ≈ 15 s.
(The SWP incremental 2.25 yd/tick backwards drag from `SWPActions_Twins.cpp:104-132` is **not**
used — it was sized for short drags and would take ~45 s here.)

*Settled* — boss within tolerance. The tank holds `IGNIS_BOSS_ANCHOR + 9.5 · u(south + slot·60°)`,
2 yd arrival tolerance, and stops moving.

*Both phases, on the rising edge of aura 62546*: advance the arc slot by one and move to the new
spot at **full speed**, not throttled. Ignis is rooted and rotation-locked for those 3 s and the
patch spawns from his frozen orientation, so the tank can cross the 9.5 yd chord (~1.4 s) with no
effect on where the patch lands and no risk of dragging the boss. The new slot sits **17.3 yd** from
the patch just dropped and 26.8 yd from the one before — both clear of the 13 yd burn. During
transit the same edge fires the same move, which is what keeps the tank out of the patch that would
otherwise land ~9 yd ahead of him on his own path.

Latch the slot index per instance in one `.cpp`,
`unordered_map<uint32 /*instanceId*/, uint8>`, the same shape as `kiljaedenHandTankAssignments`
(`SWPEncounter_KJ.cpp:154`). Only the main tank reads it, so no cross-bot agreement is needed.

A reactive "step out when the patch lands" was considered and rejected: it is the documented
oscillation pair in `docs/engine/raid-mechanics-lessons.md`. The rotation is scheduled instead.

### D3 — No raid anchors

Melee, ranged and healers get **no positioning action**. `SetBehindTargetAction` and
`ReachTargetAction` keep working; with the boss facing south, "behind" is north, which is where the
raid already is. The three dodges (scorched ground, molten construct, Flame Jets) are the whole
positioning story for non-tanks.

`CombatFormationMoveAction` is left alone. Note for whoever implements this: it is personal-space
spacing that **always returns `false`** (`MovementActions.cpp`), not a formation mover — do not
plan around it doing more than it does.

### D4 — Two construct tanks, fixed pools

Replace `GetIgnisConstructTank` with `GetIgnisConstructTankIndex(botAI, bot)` → `0`, `1`, or `-1`,
backed by `GetGroupAssistTank(botAI, bot, 0)` and `…, 1)`. Index 1 engages only when a second
activated non-Brittle construct exists; with one assist tank the raid runs the single-tank loop
unchanged, with none it skips the loop rather than feeding the boss a dead main tank.

**Pool assignment is by index, not by distance: tank 0 → west, tank 1 → east.** With a south bearing
the anchor is an almost exact tie between the pools and `GetIgnisNearestWaterPool` resolves every
tie to west, which would stack two Molten pulses and two Shatters in one spot. Patch choice follows
the same split — each tank takes the lit patch nearest **his own** pool, so tank 0 works the slot-0
patch and tank 1 the slot-2 patch.

Key `ignisTankDrivenConstructGuid` by instance:
`unordered_map<uint32 /*instanceId*/, unordered_map<ObjectGuid, ObjectGuid>>`.

### D5 — Direct targeting

Two replacement `AttackAction`s, SWP shape throughout — resolve the unit, compare against
`AI_VALUE(Unit*, "current target")`, `Attack()` only on a mismatch:

- **`IgnisAttackBrittleConstructAction`** — the Brittle construct with the **lowest GUID** raid-wide
  (`SWPActions.cpp:179-203` precedent), so every bot converges on the same one with no shared state.
  Gated on the bot being ranged or a caster (`PlayerbotAI::IsRanged`). Melee are admitted only once
  the Brittle aura has under ~7 s left, as a fallback for a raid with no ranged in position.
- **`IgnisAttackBossAction`** — `GetIgnis(botAI)`; this is what the skull-back-on-the-boss behaviour
  becomes. Tanks excluded (they hold what they hold).

**Shatter hit.** Brittle needs one hit of 5000 (10-man) / 3000 (25-man) inside 15 s. Prefer a
designated per-class burst spell, falling back to the normal rotation if it is not available:
mage Pyroblast/Frostbolt, hunter Aimed Shot/Chimera Shot, warlock Chaos Bolt/Shadow Bolt,
shadow priest Mind Blast, boomkin Starfire, elemental shaman Lava Burst.

New **`IgnisDisableDefaultTargetingMultiplier`** zeroing `DpsAssistAction`, `TankAssistAction` and
`AttackRtiTargetAction` for the **whole encounter**, not just inside the Brittle window — the
failure mode documented in `docs/raids/README.md` ("Targeting-suppression multiplier") is exactly
this shape: a settled `Attack` returns `false`, the queue drains to `dps assist` at 50, and
`GeneralFindTargetSmartStrategy` re-picks the lowest-lifetime add every tick.

### D6 — Flame Jets

- **`IgnisFlameJetsTrigger`** — Ignis is casting and
  `GetCurrentSpell(CURRENT_GENERIC_SPELL)->m_spellInfo->Id == 62680`. This one *is* observable
  (non-triggered, 2.7 s cast), unlike the triggered instants warned about in
  `docs/engine/raid-mechanics-lessons.md`.
- **`IgnisFlameJetsHoldCastAction`** at `ACTION_EMERGENCY + 2` — `botAI->InterruptSpell()` when a
  cast-time spell is in progress; returns `true` to consume the tick so nothing new starts.
- **`IgnisFlameJetsHoldCastMultiplier`** — 0 for any `CastSpellAction` whose spell has a non-zero
  cast time while the window is open; instants keep firing. `GetValue` runs per queued action per
  bot per tick, so memoise the window answer with the `cachedAtMs` / `cachedValue` pair already used
  by `XT002BurstWindowMultiplier` (`UldMultipliers.h:28`).

Update the `ulduar.md` claim that Flame Jets is deliberately unhandled.

### D7 — Corrections to existing code

- `ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS` **8 → 15** (13 yd burn + park tolerance).
- `ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS` **12 → 15** (covers the 13 yd Shatter, not just the ~7 yd
  Molten pulse).
- Replace `IgnisScorchedGroundAction`'s `MoveAwayFromCreatureAction` base with a `MovementAction`
  that resolves the patch through `GetIgnisNearestScorchedGround` (grid) and `FleePosition`s off it,
  so trigger and action read the same source. `GetIgnisNearestScorchedGround` already skips patches
  inside the water exclusion, which is correct here too — an unlit patch does no damage.
- Add `boss->IsInCombat()` to the shared encounter gate in every Ignis trigger.
- Extend `IgnisMultiplier`: add a `bot->GetMapId() != ULDUAR_MAP_ID` early-out (it has none today),
  and exempt the **main tank** from the scorched-ground dodge the same way the construct tank
  already is — his arc rotation *is* his dodge, and the two would otherwise fight.
- New **`IgnisTankMovementMultiplier`** zeroing `ReachTargetAction`, `CastReachTargetSpellAction`,
  `FollowAction` and `FleeAction` **for the main tank and the two construct tanks only** — the three
  roles whose position the strategy owns. Non-tanks are untouched, and `AvoidAoeAction` is left alone
  raid-wide. Template: `MuruControlMovementMultiplier` (`SWPMultipliers.cpp:761-822`). Scoping it to
  three bots is deliberate: the Void Reaver case study in `docs/raids/README.md` is a whole raid
  frozen by a blanket movement suppression whose replacement mover silently failed.

### D8 — Node table after the change

`UldStrategy.cpp`, replacing lines 89-122:

| Trigger | Action | Relevance |
|---|---|---|
| `ignis flame jets trigger` | `ignis flame jets hold cast action` | `ACTION_EMERGENCY + 2` |
| `ignis slag pot heal trigger` | `ignis slag pot heal action` | `ACTION_EMERGENCY + 1` |
| `ignis molten construct avoid trigger` | `ignis molten construct avoid action` | `ACTION_EMERGENCY` |
| `ignis main tank position trigger` | `ignis main tank position action` | `ACTION_RAID + 4` |
| `ignis construct tank trigger` | `ignis construct tank action` | `ACTION_RAID + 4` |
| `ignis attack brittle construct trigger` | `ignis attack brittle construct action` | `ACTION_RAID + 3` |
| `ignis scorched ground trigger` | `ignis scorched ground action` | `ACTION_RAID + 2` |
| `ignis attack boss trigger` | `ignis attack boss action` | `ACTION_RAID + 0.5` |
| `ignis fire resistance trigger` | `ignis fire resistance action` | `ACTION_RAID` |

Main-tank and construct-tank positioning share `+4` because they are mutually exclusive by role.
Multipliers added to `InitMultipliers` beside the existing `IgnisMultiplier`.

---

## Files to touch

| File | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` | Anchor/arc constants; radius corrections; instance-keyed sticky map; arc-slot latch; `GetIgnisConstructTankIndex`, `GetIgnisAssignedWaterPool`, `GetIgnisMainTankPosition`, `GetIgnisScorchLandingPosition`, `IsIgnisScorchWindow`, `IsIgnisFlameJetsCasting`, lowest-GUID Brittle pick |
| `src/Ai/Raid/Uld/Action/UldActions_Ignis.h` / `.cpp` | Delete the mark action; add main-tank position, attack-brittle, attack-boss, flame-jets-hold; rewrite the scorched-ground dodge; two-tank construct kite |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_Ignis.h` / `.cpp` | Delete the mark trigger; re-key attack-brittle off the construct instead of the icon; add main-tank position, attack-boss, flame-jets triggers; add the combat gate |
| `src/Ai/Raid/Uld/UldActionContext.h` / `UldTriggerContext.h` | Registrations follow the new names |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | Node table above; register the three new multipliers |
| `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp` | `IgnisTankMovementMultiplier`, `IgnisDisableDefaultTargetingMultiplier`, `IgnisFlameJetsHoldCastMultiplier`; extend `IgnisMultiplier` |
| `docs/raids/ulduar.md` | Rewrite the Ignis section (`:439-478`) — geometry, corrected radii, Flame Jets no longer unhandled, RTI gone |

No `CMakeLists.txt` edit: the module has none and AzerothCore globs `modules/*/src`.

## Reuse — do not re-derive

- `GetIgnis`, `IsIgnisConstruct{Activated,Molten,Brittle}`, `GetIgnisBrittleConstruct`,
  `GetIgnisNearestMoltenConstruct`, `GetIgnisDrivenConstruct`, `GetIgnisNearestScorchedGround`,
  `GetIgnisSlagPotVictim` — all in `UldBossHelper.cpp:622-786`, all still correct.
- `GetGroupMainTank` / `GetGroupAssistTank` / `GetNearestPlayerInRadius` —
  `src/Ai/Raid/RaidBossHelpers.cpp`.
- Z/collision sanitiser for any computed position: `GetMapWaterOrGroundLevel` + `INVALID_HEIGHT`
  fallback + `CheckCollisionAndGetValidCoords` (`SWPActions_Muru.cpp:160-174`).
- `PlayerbotAI::IsRanged` / `IsMelee` / `IsTank` / `IsHeal`, `IsAssistTankOfIndex`.

## Verification

Static, before anything else — the module cannot be compiled headless here, so hand the build to the
user:

1. `grep -rniE "rti|targeticon|skull" src/Ai/Raid/Uld/{Action,Trigger}/*Ignis*` returns nothing.
2. Every string in `UldStrategy.cpp`'s Ignis block resolves to a creator in `UldActionContext.h` /
   `UldTriggerContext.h` — names fail **silently** at runtime, so diff the two lists by eye.
3. Arithmetic check on the anchors: for each of the three arc slots, assert patch→nearest-pool
   > 25 yd and patch→next-slot > 13 yd. Numbers are in D2; recompute if any constant moves.

In-game, 25-man Ulduar, Ignis, with a main tank and at least one assist tank:

4. Pull from anywhere. Nothing moves before the pull. The main tank walks Ignis to ≈ (587.5, 277.8)
   in roughly 15 s and holds; the boss never drops threat en route.
5. Watch three consecutive Scorches: patches land **south** of the boss in a fan, all of them lit
   (they tick damage), none within 25 yd of a pool, none on the raid. The tank has left the landing
   point before the patch appears, in transit as well as settled.
6. Construct cycle: assist tank 0 walks his construct onto the west-side patch and then to the
   **west** pool; assist tank 1 uses the east-side patch and the **east** pool. Brittle lands within
   a couple of seconds of arrival. The two never converge on one pool.
7. A ranged bot — not a melee one — shatters it, and every bot picks the same construct. The
   Strength of the Creator stack count on Ignis drops. Confirm no melee bot took Shatter damage.
8. Flame Jets: casters stop mid-cast during the 2.7 s and resume afterwards instead of eating the
   6 s lockout.
9. Slag Pot: the victim is never a tank (core guarantees this); healers pour direct heals; the
   victim does not try to walk.
10. **No raid target icon appears at any point in the fight.**
11. Kill it, then wipe and re-pull once: the arc slot and the sticky driven-construct map must both
    reset cleanly (the instance-keyed rewrite is what makes this pass).
