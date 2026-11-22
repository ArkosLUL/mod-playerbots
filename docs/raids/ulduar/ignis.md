# Ignis

The fight is a construct-disposal loop, not a damage race. Each Iron Construct (33121) Ignis
activates puts a stack of Strength of the Creator (64473) on him, and a construct **cannot be killed
by damage** — it only dies to the chain:

1. it stacks Heat (65667) while standing in a Scorched Ground patch (33123), and turns **Molten**
   (62373) at 10 stacks, which also wipes its threat table;
2. a Molten construct within **18 yd of a water trigger** turns **Brittle** (62382 10-man / 67114
   25-man) on the construct's own once-a-second poll — which sits behind `UpdateVictim()`, so a
   construct with nobody on it never polls at all;
3. any single hit of 5000 (10-man) / 3000 (25-man) then shatters it, killing it and removing a
   Strength stack.

Ignis activates **one** construct at a time, the first 30 s in and one every 30 s after — 40 s on
10-man, the only difficulty difference in the fight — so that gap is most of a construct tank's
fight.

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

He works **three arc slots 60° apart**, because a patch lives 30 s (`TEMPSUMMON_TIMED_DESPAWN` in
`spell_ignis_scorch_aura`) against Scorch's 20 s repeat, so two are alive for 10 s of every 20. He
advances one on the rising edge of Scorch (62546). Ignis is
rooted and rotation-locked for those 3 s and the patch spawns from the orientation frozen at cast
start, 20 yd along it (`spell_ignis_scorch_aura`'s last tick). With Ignis on the anchor the step is
free and leaves him 17.3 yd from the patch — 26.1 on the 120° wrap from the third slot to the first —
outside its 13 yd burn, where standing still would have left him 10.5 yd from its centre. The slot is
latched per instance id, and only the main tank advances it: anyone else asking would eat the rising
edge. A reactive "step out once it lands" was rejected: it is the
[dodge-and-hold oscillation](../../engine/raid-mechanics-lessons.md#movement-that-oscillates).

Reaching the spot latches it: he moves again only past twice `ULDUAR_IGNIS_TANK_SPOT_TOLERANCE` (2 yd),
since re-issuing a move on every drift inside the tolerance left him sliding and never landing a swing.

**Nothing pre-positions**: the tank walks the boss to the anchor in combat, from wherever the pull
happened. Ignis runs at 10 yd/s against a player's 7, so he keeps up with a tank at full speed, but
an issued walk goes on whether he follows or not: one `MoveTo` to the spot carried a tank 99 yd off
a boss who had switched to a DPS, and left him there. So the tank walks
`ULDUAR_IGNIS_TANK_DRAG_LEG` (5 yd) at a time, checking before each leg that Ignis is on him and in
melee range or rooted for Scorch. A spot inside his melee reach (an arc step) goes in one move.

## Two construct tanks, pools fixed by index

Assist tank 0 takes the **west** pool, assist tank 1 the **east**, and each works the lit patch
nearest its own pool. Fixed by index rather than distance because the anchor sits almost exactly
between the pools: nearest-pool is a tie that always resolves the same way, stacking both Molten
pulses and both Shatters in one spot.

Kiting is **assist-tank only, on purpose**: a main tank pulled off Ignis drags the boss along behind
the construct, and a DPS holding a Molten one dies. A raid without an assist tank skips the loop and
lets the Strength stacks climb; with one, tank 1 never finds a construct anyway, since
`GetIgnisDrivenConstruct` excludes whatever the other tank holds.

**A tank's construct is sticky**: kept until Brittle or dead, never handed to a closer new one, since
Molten wiped its threat and letting go releases it into the raid. With no lit patch yet he holds it
where it stands; within 18 yd of the pool he holds still for the poll rather than walk it back out.

**The walk stops 12 yd short of the trigger** (`ULDUAR_IGNIS_WATER_STANDOFF`), never on it: the
trigger sits in a channel whose surface is ~1.8 yd below the room floor of 360.802, so a tank parked
on it logs Z 359.0 and needs a summon to get back out. navprobe `settledZ` room-side reads
356.3 / 359.3 / 359.9 at 4 / 8 / 12 yd. He has no reason to go in — the poll measures the
**construct**, which follows into melee contact ~5 yd back, and being 3D and radius-inclusive it
reaches ~3 yd past the bare 18.

**A construct that is not on the tank gets chased, not waited on.** Molten wipes threat and the
construct fixates on whoever is nearest, so the tank taunts and walks it down when the taunt cannot
land — every taunt in the game is 30 yd. Returning on the failed taunt instead froze a tank 15.7 s
from a construct 81 yd away, in a pull that converted **zero** constructs.

**Between constructs a construct tank is an ordinary tank again.** `IgnisPlacedTank` counts one only
while it holds a construct, so the movement block lifts and `ignis attack boss trigger` puts it on
the boss; keyed on the role instead, the activation gap left it standing still with every generic
mover stripped. The held test, `GetIgnisHeldConstruct`, is a hash lookup and never walks the grid —
the rules ask it per popped action.

## Targeting is direct — no raid icons

Nothing here sets, clears or reads a raid target icon. `ignis attack brittle construct action` and
`ignis attack boss action` resolve the unit and `Attack()` it, and
the `ignis disable default targeting` rule zeroes `DpsAssist`, `TankAssist` and `AttackRti` for
the whole encounter — not just the Brittle window, or a settled `Attack`
returning false drains the queue to `dps assist`, which re-picks every tick.

The Brittle pick is the **lowest GUID** raid-wide, so every bot converges with nothing shared to
agree through. Ranged and casters take it, with a designated per-class burst spell (Pyroblast,
Chimera Shot, Chaos Bolt, Mind Blast, Starfire, Lava Burst) because rotation filler often will not
reach the 5000/3000 alone. Melee are let in only for the last 7 s of the 15 s window: Shatter deals
18850 in 13 yd, inside melee range of the thing they would be swinging at. Tanks never join: pulling
one off Ignis or off a construct Molten wiped costs far more than the hit. The trigger stays hot once
the construct is targeted, so the action can fire the burst spell; with none castable it returns false
and the rotation runs rather than wait on a cooldown.

With the generic pickers off, `ignis attack boss trigger` is also what puts the **main tank** on
Ignis — it excludes a construct tank only while that tank holds a construct. It yields to the Brittle
trigger, testing "already on the boss?" first: that settles most ticks, and the Brittle test scans
every construct.

## Everything else

**Flame Jets** (62680) is an observable 2.7 s cast. Bots stop a cast that cannot land before the
knockback and start no new one that would not finish either; instants and short heals keep going.
The 6 s lockout afterwards is unavoidable — this only stops feeding casts into it.

**Slag Pot** (62717 / 63477) is a vehicle ride the victim cannot be moved or shielded out of for its
10 s of ticks: healers pour direct heals into it, and its movement actions are suppressed, since orders
only fight the ride and leave it facing the wrong way when it drops. The heal node outranks every other
heal (`ACTION_EMERGENCY + 1`), so it waits for the victim to drop under
`ULDUAR_IGNIS_SLAG_POT_HEAL_HP_PCT` (85), or the whole team tops off a full-health victim while the tank
takes Flame Jets unhealed. Neither tank can be potted — the core skips the boss's victim and every
construct's victim.

Hazard radii are sized against the spells, not the visuals: the Scorched Ground dodge is 15 yd
(62548 burns in 13), the Molten avoid 15 yd (sized against Shatter's 13, not the ~7 yd pulse). The
Molten avoid keys on the aura, not the entry, so a construct still being walked through the fire does
not scatter the raid, and skips two bots: the tank holding a construct (the pulse is his job; between
constructs he clears it like anyone) and Ignis' own tank, who would drag the boss and his Flame Jets
through the raid.

The main-tank spot and the kite share `ACTION_RAID + 4`, above the raid's damage, since no bot is ever
both. The placed tanks are blocked from **both** AoE dodges, this encounter's and the generic `avoid aoe`:
the construct tank is parked in a patch on purpose and the main tank's arc already steps him clear.
The generic one is what matters — at `ACTION_EMERGENCY` it sits above the kite's `ACTION_RAID + 4`,
so left on it outranks the walk on nearly every tick spent in the fire.

The `ignis tank movement` rule takes the generic movers off the main tank while Ignis is on him and
a construct tank while it holds a construct, and nobody else. A main tank that lost Ignis gets them
back, or nothing walks him into taunt range again. Non-tanks keep everything, which is what
spreads the ranged half without an anchor of their own, and a blanket block would freeze the whole raid
if a replacement mover ever failed quietly.

Ignis has no hard mode. Heroic is free: the paired spell ids above are both checked, Scorch's 25-man
remap of 62546 included (**`SPELL_IGNIS_SCORCH_25` = 63474**,
[difficulty ids](../../engine/pitfalls.md#detection-that-never-fires)).

Every node is gated on `IsIgnisEngaged` — alive **and** in combat, since the boss is visible from
the whole 200 yd room. Lookups go through `GetIgnis`, not `"find target"`: a bot parked on a
construct never has Ignis on its threat list, and a dormant construct carries
`UNIT_FLAG_NOT_SELECTABLE`, which drops it out of `"possible targets"` entirely, plus aura 38757
(`SPELL_IGNIS_CONSTRUCT_INACTIVE`, stripped by Activate Construct); it counts as activated once both are
gone. The room is wider
than SightDistance too, so the cached `"nearest npcs"` list goes blind at the pools: constructs spawn
at x 543..631 / y 217..338 and Ignis starts at (586.5, 378.8), so a tank at a pool is ~120 yd from him,
hence `ULDUAR_IGNIS_ROOM_SEARCH_RADIUS` 200. The Scorched Ground trigger and its action share
`GetIgnisNearestScorchedGround`: an action on `"nearest npcs"` leaves the trigger hot on a patch it
cannot see, and the bot burns.

Construct scans run `GetCreatureListWithEntryInGrid`'s own body into a vector: one allocation per scan,
same visit order, so ties break the same. The Flame Jets multiplier runs against every candidate action,
so it memoises the boss lookup for the tick.

**`GetIgnisIf` never searches the grid.** It reads `instance->GetCreature(ULD_BOSS_IGNIS)` through the
acceptance test `FindNearestCreature` would apply
([instance lookups](../../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid)), with
`InSamePhase` from both sides since it is not symmetric. That is sound because 33118 has exactly one DB
spawn, nothing summons it, and `instance_ulduar` registers it through the base `OnCreatureCreate` /
`OnCreatureRemove`. Where a trigger depends on threat, keep `"find target"`: the instance handle knows
nothing about threat.
