# Ignis

The fight is a construct-disposal loop, not a damage race. Each Iron Construct (33121) Ignis
activates puts a stack of Strength of the Creator (64473) on him, and a construct **cannot be killed
by damage** — it only dies to the chain:

1. it stacks Heat (65667) while standing in a Scorched Ground patch (33123), and turns **Molten**
   (62373) at 10 stacks, which also wipes its threat table;
2. a Molten construct within **18 yd of a water trigger** turns **Brittle** (62382 10-man / 67114
   25-man) on the construct's own once-a-second poll;
3. any single hit of 5000 (10-man) / 3000 (25-man) then shatters it, killing it and removing a
   Strength stack.

Scorch only lights a patch when it lands more than 25 yd from water, so the fire and the pools are
always separate places and the walk between them is the mechanic. The two pools are at
`(526.771, 277.796, 360.802)` and `(646.771, 277.796, 360.802)` — hardcoded as
`ULDUAR_IGNIS_WATER_POOL_WEST` / `_EAST` rather than found by entry, because the water trigger is
22515, the generic Ulduar world trigger.

## The main tank picks where every patch lands

Scorch summons its patch at `boss + 20 yd` along the boss's facing, and the boss faces the main
tank. His spot therefore has to put the patch more than 25 yd from both pools — closer and it never
lights, stalling the loop — and off the raid.

`ULDUAR_IGNIS_BOSS_ANCHOR` (587.5, 277.8) is the room centre and the only spot where either bearing
clears both pools. The tank stands 9.5 yd off it — melee range is ~10.8, Ignis' combat reach being
8.0 — on a bearing of **south**, putting patches south, pools east and west and the raid north:
three axes that cannot collide. Patch to either pool then lands at 44–78 yd; the construct walk is
~45 yd, ~5 s against a 30 s Molten window.

He works **three arc slots 60° apart**, advancing one on the rising edge of Scorch (62546). Ignis is
rooted and rotation-locked for those 3 s and the patch spawns from the orientation frozen at cast
start, so the step is free and leaves him 16 yd from the patch — outside its 13 yd burn — where
standing still would have left him 9 yd inside it. The slot is latched per instance id. A reactive
"step out once it lands" was rejected as the documented oscillation pair.

**Nothing pre-positions**: the tank walks the boss to the anchor in combat, from wherever the pull
happened. Ignis runs at 10 yd/s against a player's 7, so he stays glued to a tank at full speed: a
melee-range leash check is the whole guard, no throttled drag.

## Two construct tanks, pools fixed by index

Assist tank 0 takes the **west** pool, assist tank 1 the **east**, and each works the lit patch
nearest its own pool. Fixed by index rather than distance because the anchor sits almost exactly
between the pools: nearest-pool is a tie that always resolves the same way, stacking both Molten
pulses and both Shatters in one spot.

Kiting is **assist-tank only, on purpose**: a main tank pulled off Ignis drags the boss along behind
the construct, and a DPS holding a Molten one dies. A raid without an assist tank skips the loop and
lets the Strength stacks climb; with one, tank 1 never finds a construct anyway, since
`GetIgnisDrivenConstruct` excludes whatever the other tank holds.

## Targeting is direct — no raid icons

Nothing here sets, clears or reads a raid target icon. `ignis attack brittle construct action` and
`ignis attack boss action` resolve the unit and `Attack()` it, and
`IgnisDisableDefaultTargetingMultiplier` zeroes `DpsAssistAction`, `TankAssistAction` and
`AttackRtiTargetAction` for the whole encounter — not just the Brittle window, or a settled `Attack`
returning false drains the queue to `dps assist`, which re-picks every tick.

The Brittle pick is the **lowest GUID** raid-wide, so every bot converges with nothing shared to
agree through. Ranged and casters take it, with a designated per-class burst spell (Pyroblast,
Chimera Shot, Chaos Bolt, Mind Blast, Starfire, Lava Burst) because rotation filler often will not
reach the 5000/3000 alone. Melee are let in only for the last 7 s of the 15 s window: Shatter deals
18850 in 13 yd, inside melee range of the thing they would be swinging at.

With the generic pickers off, `ignis attack boss trigger` is also what puts the **main tank** on
Ignis — it excludes construct tanks only.

## Everything else

**Flame Jets** (62680) is an observable 2.7 s cast. Bots stop a cast that cannot land before the
knockback and start no new one that would not finish either; instants and short heals keep going.
The 6 s lockout afterwards is unavoidable — this only stops feeding casts into it.

**Slag Pot** (62717 / 63477) is a vehicle ride: healers pour direct heals into the victim, and the
victim's movement actions are suppressed, since orders only fight the ride and leave it facing the
wrong way when it drops. Neither tank can be potted — the core skips the boss's victim and every
construct's victim.

Hazard radii are sized against the spells, not the visuals: the Scorched Ground dodge is 15 yd
(62548 burns in 13), the Molten avoid 15 yd (sized against Shatter's 13, not the ~7 yd pulse).

`IgnisTankMovementMultiplier` takes the generic movers off the main tank and the two construct tanks
and nobody else — the three roles whose spot the encounter owns. Non-tanks keep everything, which is
what spreads the ranged half without an anchor of their own.

Scorch's 25-man id is **`SPELL_IGNIS_SCORCH_25` = 63474** (`UldEncounter_Ignis.h:42`), the
difficulty remap of 62546 — a whole-module sweep for missing remaps of this shape found it here and a
matching hole on Mimiron's Plasma Blast.

Ignis has no hard mode. Heroic is free: the paired spell ids above are both checked, and the only
other 25-man difference is the construct cadence (30s instead of 40s).

Every node is gated on `IsIgnisEngaged` — alive **and** in combat, since the boss is visible from
the whole 200 yd room. Lookups go through `GetIgnis` (a grid search), not `"find target"`: a bot
parked on a construct never has Ignis on its threat list, and a dormant construct carries
`UNIT_FLAG_NOT_SELECTABLE`, which drops it out of `"possible targets"` entirely. The room is wider
than SightDistance too, so the cached `"nearest npcs"` list goes blind at the pools.

**A grid search that far is expensive, so ask the instance script first.** `GetIgnisIf` reads
`instance->GetCreature(ULD_BOSS_IGNIS)` as an O(1) pre-filter and rejects there — **then still runs
the real search**, because only the search knows which cells its 200 yd octagon actually covers. Most
lookups now never reach it. The preconditions are what make the pre-filter sound: there is exactly
one DB spawn of 33118, the script summons none, and `instance_ulduar` registers it through the base
`OnCreatureCreate` / `OnCreatureRemove`. Where a trigger depends on threat, keep `"find target"` —
the instance handle knows nothing about threat.

