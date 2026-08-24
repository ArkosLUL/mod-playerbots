# Obsidian Sanctum — Sartharion (map 615)

Strategy key `wotlk-os`, files in `src/Ai/Raid/OS/`. Cross-raid conventions are in
[README.md](README.md). Everything below is verified against
`src/server/scripts/Northrend/ChamberOfAspects/ObsidianSanctum/`, `acore_world` and the DBC
reference CSVs in `modules/mod-spell-tweaks/data/dbc-reference/`.

The strategy targets the 3-drake kill and degrades to 0/1/2-drake without special-casing.

`wotlk-os` is registered on **both** engines (`PlayerbotAI.cpp:1777-1778`), so the holds run out of
combat too — which is why the off-tank is already parked before the pull.

## Geometry

Sartharion home `(3246.57, 551.263, 58.62)`. The raid enters from the **south**, teleporting in at
`(3228.58, 385.86)` (`areatrigger_teleport` 5243) and walking north onto the platform. That X is
`RAID_ENTRY_ANCHOR_X`, the anchor every slot in the line is measured east of; the main tank drags
Sartharion south onto it. Safe to drag — `Creature::_IsTargetAcceptable` returns early on
`GetMap()->IsDungeon()` before any home-distance check and Sartharion sets no boundary, so there is
no leash to hit.

Landing spots: Tenebron `(3249.75, 566.95)`, Shadron `(3230.50, 533.00)`,
Vesperon `(3269.71, 532.79)`. **All three sit inside a lethal tsunami band under both wave
patterns**, so no one may camp one while a wave is airborne.

**The walkable floor is smaller than the room and its east edge moves with Y.** Read off the map 615
Detour navmesh (`dtPoly.areaAndtype & 0x3f` is 1 ground, 2 magma; recast `(x,y,z)` is wow `(y,z,x)`):
floor **X 3219.5–3274.5, Y 486–574**, z 58.0–59.6 over a magma surface at 57.08. East edge by Y:
3267.5 at 511.5, 3274.5 at 528.5, 3273 at 535.5, 3272 at 552.5.

Two boxes, different jobs. **Destinations** clamp to the floor first, then inside a Range Marker
(`ClampDestination`). The floor clamp is **banded**, because one rectangle either admits lava or gives
away the east end; each band was swept at 0.5yd and is 100% ground:

| Y | X |
|---|---|
| 487–492 | 3220–3231 |
| 492–496 | 3220–3242 |
| 496–504 | 3220–3250, the main tank's drag lane |
| 504–520, 566–569 | 3227–3266 |
| 520–566 | 3227–**3268**: corridors, drake spots, off-tank |

It doubles as the freeze guard: with movement suppression live, an off-navmesh `MoveTo` strands a bot
for the fight. **Targets** use the looser `InsideRoom` box `X 3208–3286, Y 474–591, Z ≤ 65`, so a Lava
Blaze on the rim still counts while the drake perches do not.

**The two boxes leave an 18yd band of lava that reads as "inside the room".** `ROOM_MAX_X` is 3286
and the platform ends at 3268, so a bot standing in between satisfies `InsideRoom` and no trigger
covers it. The strategy clamped every destination it *sent* a bot to and never checked where a bot
actually *was*. `OffThePlatform` (`OFF_PLATFORM_MARGIN` 1.0, opened by a yard so arrival tolerances do
not trip it) now sits beside the room test — with the main tank exempt until the drag latches, since
his drag corner is a hand-measured point 1.74yd south of `PLATFORM_MIN_Y`.

**The mmaps are no longer in this checkout**, so none of the above can be re-probed here. The offline
tool that answers these questions lives in the core fork, not this module — see
[../engine/pitfalls.md](../engine/pitfalls.md).

## Pyrobuffet — the raid is only ever safe inside a Range Marker

Sartharion self-casts 56916 at engage, pulsing 57557 map-wide every 8s: ~4000 fire plus a stacking
60s +1000 fire-taken debuff. The **only** exemption is `ExcludeTargetAuraSpell = 56911` ("Range
Marker"), a 40yd friendly area aura on two Safe Area triggers (NPC 30494) at `(3244.14, 512.60)` and
`(3242.84, 553.98)`. Anything outside both circles bleeds all fight.

The 15-minute despawn that would remove the triggers is scheduled into `extraEvents` but handled in
the `events` switch, so it is silently swallowed and the circles persist. Same swallow kills the
15-minute berserk — **there is no hard enrage to race**, which is what makes holding cooldowns safe.

`ClampToRangeMarker` pulls along **X only**, onto the chord the circle cuts at the current Y. Y is
the axis a tsunami kills on and must survive the clamp. Radial fallback only when no X on that line
is covered.

## Flame Tsunami — the two patterns are anti-phased

Creature 30616, side randomised per wave. Left spawns X 3211 travelling +X, right X 3286 travelling
−X. Lethal half-width **8.5yd** (spell 57491 radius 7.0 + the player's 1.5 combat reach; the
caster's reach is not added).

| Wave | Spawn Y offsets | Danger bands | Gaps |
|---|---|---|---|
| Left | 476,484,492 \| 524,532,540 \| 572,580,588 | 467.5–500.5, 515.5–548.5, 563.5–596.5 | **500.5–515.5**, **548.5–563.5** |
| Right | 500,508,516 \| 548,556,564 | 491.5–524.5, 539.5–572.5 | 480–491.5, **524.5–539.5**, 572.5–588 |

**Each wave's gap centres are the other wave's band centres — no position survives both.** Bots must
react per wave, and dodging is Y-only: a band sweeps the whole X range at 10.5 yd/s, outrunning a
bot's 7.

The raid holds **two** corridors, not one — see the section below. The tank pair is measured in-game,
southern, and carries its own X as well as a Y:

| `CorridorGroup` | Left wave | Right wave |
|---|---|---|
| **Tank** | **`(3221.3743, 511.089)`** (gap 500.5–515.5, on the southern Safe Area trigger) | **`(3220.0706, 490.0)`** (gap 480–491.5) |
| **Melee** | tank's, 511.089 | **raid's, 535.5** — they give this one up, see below |
| **Raid** | **551** (gap 548.5–563.5) | **535.5** (gap 524.5–539.5) |

The right tank hold reads 490.581 in-game but ships 0.58yd south of that: the southern right band is
only 5yd wide (floor 486.5, threshold 491.5), and 490.581 leaves 0.92yd of headroom for a 1yd
tolerance. `TANK_ARRIVAL_TOLERANCE` is **1.0** for the same reason, against 2.0 for the raid line —
anything looser lets a bot "arrive" inside a band.

Corridors are **home-and-away, not sticky**: each group idles on the hold that is safe under one
pattern and moves only for the other. Waves cover ~11s of every 25.

Timing: first summon at +20s, then every 25s. Damage aura up at +3.6s, down at +11s, despawn +13.5s.
Reaction window is 3.6s plus travel — worst case 4.69s at the east end of the line.

- **Detect on the creature spawning, never on the damage aura**, which appears at +3.6s and spends
  the whole window.
- Spell 57491 effect 3 is `MOD_DECREASE_SPEED −50%` for 10s, so a bot that grazes one wave **cannot
  clear the next**. Failure compounds; the first dodge has to be reliable.
- Wave side is read off the tsunami's Y. The two offset sets are disjoint whole numbers and Y never
  changes in flight, so the truncating integer compare is exact for the wave's whole life.
- Suppression lifts per bot once a wave has swept past its X, **or** once the wave is spent. A spent
  wave is scale 0.1 against 1.0 live (`FINISH_LAVA` shrinks it), so `GetObjectScale() < 0.5` is the
  second release test. It is needed because a right wave stops at X 3211 while the tank stands at
  3220.07, clearing "swept past" by only 0.57yd. Releasing on either test rather than on the 13.5s
  despawn saves 2.85s per wave for the tank and 5.5s for the raid.

## Molten Fury — why hunters matter here

The tsunami damage aura 57492 has a second periodic effect pulsing **60430 Molten Fury** every 800ms
in **6yd**: `DispelType 9 = DISPEL_ENRAGE`, +200% health, +100% damage, 30s. **No `conditions` rows**,
so it lands on anything in range — Lava Blaze, a drake, or Sartharion himself.

Tranquilizing Shot 19801: two `SPELL_EFFECT_DISPEL` effects (MiscValue 9 enrage, 1 magic), 8s
cooldown, 35yd. Targets are assigned by hunter index modulo the enraged list so several hunters
spread instead of stacking dispels.

## Drakes

| Drake | Entry N/H | Called at | Held at |
|---|---|---|---|
| Tenebron | 30452 / 31534 | 20s | `(3249.8467, 563.3801)` |
| Shadron | 30451 / 31520 | 60s | `(3228.9385, 534.65955)` |
| Vesperon | 30449 / 31535 | 120s | `(3266.0, 556.0)` |

**Each drake is held where it lands**, on its own measured spot — there is no shared pile. Assignment
walks the drakes **newest first**, because reverse call order is landing recency and the drake that
just touched down is the one with nobody on it.

Vesperon is held **north of his landing**, not on it, so his breath stops pointing down the raid line.

No spot is safe under **both** wave patterns, so a live wave takes the raid corridor Y and keeps X.
The east anchor is now only for blazes and whelps.

The drakes are **boss-flagged**: `instance_encounters` credit rows for 30452/30451/30449 make
`ObjectMgr` OR in `CREATURE_FLAG_EXTRA_DUNGEON_BOSS`, and the heroic entries carry `type_flags 108`
(`CREATURE_TYPE_FLAG_BOSS_MOB`) instead. So the shared burst gate applies to them, which is why it
had to learn about off-tanks — see
[../systems/consumables-and-burst.md](../systems/consumables-and-burst.md).

Kill order is landing order. Drakes are `UNIT_FLAG_NOT_SELECTABLE` while airborne, so
`"possible targets"` never sees them and the 100yd sight cap is too short anyway — use
`GetCreatureListWithEntryInGrid` at 200yd.

**A perched drake is selectable.** `Reset()` clears the flag; only `EVENT_DRAGON_START_PATROL` sets
it, 500ms after Sartharion's `JustEngagedWith` fires `ACTION_START_PATROL`. For that half second all
three are alive and targetable, sitting on spawns **off the arena** — Tenebron `(3239.07, 657.24,
86.88)`, Vesperon `(3145.68, 520.71, 89.70)`, Shadron `(3363.06, 525.28, 98.36)`. Unfiltered, that
window sends every DPS bot and the off-tank running at a perch, out of combat and into
`MoveRandomAction`. Hence `InsideRoom` on every target search.

**Inbound is read off the flight speed rate, not off distance.** The script sets `MOVE_FLIGHT` rate
3.0 in the same handler that issues `MovePoint(POINT_LANDING)` and leaves the pre-call patrol at 1.0
(`Reset()`), so `GetSpeedRate(MOVE_FLIGHT) > 2.0f` splits "coming down" from "circling" exactly. ETA
is then distance / 21 yd/s. Distance alone reports a 3s ETA minutes early, every time a patrol
waypoint passes near a landing spot.

## The encounter clock rebuilds on the combat edge

`GetSartharion` searches 200yd and the entrance sits 166yd from his spawn, so **he resolves before the
pull** — the raid has him in range from the moment it zones in. Stamping per-instance state on first
sight therefore started every clock at zone-in: `lastSeenMs` was never stale, the staleness branch
could not fire, the drag always timed out, the redirect pull window was already expired, and the
portal squad was picked over whoever happened to be on the map. `StateFor` rebuilds on the
**out-of-combat edge** instead. The general form of this trap is in
[../engine/pitfalls.md](../engine/pitfalls.md).

## The raid splits across two corridors

Effective config: **`HealDistance` 38.5, `SpellDistance` 28.5** (`playerbots.conf.dist`, no `AC_`
override).

`CorridorGroupFor` returns **three** groups, not two:

- healers and ranged → `Raid`, always. Walking to Sartharion is a 41yd trip that ends in his Flame
  Breath, cast every 6s — so they never do it, and the multiplier zeroes every generic mover whose
  target is the boss.
- main tank → `Tank`, all fight.
- melee whose victim is Sartharion → `Melee`, their own profile.
- everyone else, the off-tank included → `Raid`. He is locked off the boss all fight, so he never
  qualifies for `Melee`.

**`Melee` keeps the tank's lane except under a right wave, where it takes the raid's.** The tank's
right hold is the one place melee cannot follow him: below Y 492 the platform is 11yd wide, so the
clamp puts them 39.5° off his facing — inside a 98° Flame Breath — with the healers 46yd away and
nothing able to heal it off. The 524.5–539.5 band is no better, since everything in it inside his
20.83yd melee range is also inside a 30yd Tail Lash. So melee give the right wave up instead: it
costs them the ~11s the wave is airborne, and 31yd from the boss is well outside melee range anyway.

With no wave in the air the tank idles left and the raid idles right.

**There is no convergence latch.** The main tank keeps his own pair for the whole fight. Walking the
boss up to the raid line would turn him and put that line inside his 60yd frontal cone, which costs
more than the range it buys.

**The main tank is healable between waves** — 36.55yd against `HealDistance` 38.5 — and out of range
only while a wave has the groups split (48.3yd left, 53.2yd right). The earlier "unhealable tank" cost
and its "raid left corridor → 510" mitigation are **retired; do not re-audit.**

Ranged stay outside `SpellDistance` 28.5 of Sartharion all fight (36.8yd and 50.5yd from the two raid
holds), so **only melee and the tanks ever damage him**.

The raid pair brackets the drake touchdowns at Y 533 and 567. Separation does **not** protect it:
Flame Breath reaches 60yd, further than any distance on the platform. Only the angle does, which is
what the pull drag buys.

Accepted degradation: a Lava Blaze spawning on a player in the far corridor is outside the off-tank's
30yd taunt range until the next wave lines the groups up.

## Shadow Breath is what fixes the layout

57570 / H 59126 is `TARGET_UNIT_CONE_ENEMY_24` — a **frontal cone, 60° wide and 15yd deep**
(`spell_cone`; `EffectRadiusIndex 18` → `spellradius.reference.csv` id 18 = 15). `DoCastVictim` on a
**measured 17.5s** cadence per landed drake.

A corridor is 15yd wide with 4yd margins, so each group is effectively collinear along X and ordering
is the only tool. **West to east, offsets from `RAID_ENTRY_ANCHOR_X` 3228.58: Sartharion + main tank
(+0, the boss +7) → ranged/healers (+20) → off-tank (+33).** The boss faces the main tank, so his
cones point west into the empty strip. Two rules enforce "only the off-tank eats it":

1. **Nobody crosses west of a drake.** Everyone meets a landing 4yd **east** of its coord, so the
   drake faces east into empty platform on touchdown, and the ranged line clears Shadron's (X 3230.50)
   by 18yd. Vesperon is the exception: he touches down at X 3269.71, 2.3yd short of the floor's east
   edge, so the off-tank waits just west of him and lets him walk the rest. Costs nothing — the ranged
   line is 21yd from that touchdown, outside the cone for the whole transient.
2. **Melee take a flank, and which flank depends on the target.**

**Against Sartharion, melee take his north side**, southern only as a fallback. The trigger fires on
standing in a cone **or** simply on being south of him: a dodge can leave melee south, and standing
there is perfectly safe from both cones, so without that second test nothing would ever bring them
back north. He is parked, so his own Y does not move and there is 14yd between it and the northern
flank — no room for the test to chatter. The floor clamp can swing the
chosen point into a cone, so it is re-tested after clamping. Gated on the bot **actually standing in a
cone**, not on being off the flank — he sits 10–12yd from the tank, so a corridor swap turns him ~99°,
and tracking that every tick would walk melee a long arc around him every wave.

**Against a drake, melee take its rear outright** — gate 140°, draw ±30°. A drake has a frontal cone
and nothing off its tail, so the shared `RearFlankAction` 90°–120° band is the wrong tool here: it is
a *side* wedge, correct for a mob with a tail attack, wrong for one without. `RearFlankAction` is
therefore zeroed for the boss **and** the drakes rather than out-prioritised, because the actions
replacing it release the tick once the bot is in position.

The east anchor is clamped as the **east end of the line**, with the off-tank derived from it.

## Only the main tank ever touches Sartharion

**His combat reach is 18yd** (`creature_model_info`, display 27035), so `GetMeleeRange` against a
player is 20.83 and he stops chasing the instant the tank is inside it, so a tank walking to a fixed
anchor never gets him there.

| Spell | Direction | Arc | Radius | Cadence |
|---|---|---|---|---|
| Flame Breath 56908 | front | 82° | **60yd** | 6s |
| **Tail Lash 56910** | **rear** | 82° | 30yd | 11s |
| Cleave 56909 | victim only, no chain | — | 5yd | 7s |

Arcs come from `spell_cone` and are **full** angles, matching `HasInArc`. 56910 is rear-facing because
its `SpellVisual` is 3879, which is what `SpellMgr` turns into `SPELL_ATTR0_CU_CONE_BACK`. That leaves
two 98° side wedges; `InSartharionCone` tests a point against both through the engine's own
`HasInArc` / `isInBack`, so it cannot drift from the mechanic.

**One-time south drag.** At the pull the tank walks to `(3220.9905, 485.262)`, waits **5s**, then
resumes his normal flow; Sartharion follows and settles at ≈`(3228.5, 504.7)`. There are no waves in
the first 20s, so nothing can interrupt the dwell. Latched one-way, so it runs once.

The drag destination **deliberately bypasses `ClampDestination`**. It is a hand-measured point south
of the platform minimum and already inside a Range Marker, so both clamps could only damage it.

That corner is `NAV_MAGMA`, and **players path onto magma** — `PathGenerator::CreateFilter`
(`PathGenerator.cpp:764-767`) includes it — so it is reachable, not rejected. If the tank takes damage
there or never arrives, `Y 487.0` is the fallback; it costs under 1.5yd of boss placement.

The drag ends when the tank arrives with the boss in melee range, or the boss is south of Y 522 by any
route, or `MAIN_TANK_DRAG_TIMEOUT_MS` 45000 elapses — **measured from the drag, not from the pull**.
The give-up path logs one `LOG_WARN` per pull with the tank's position, which is the only way to tell
"the boss never followed" from "`MoveTo` silently refused the off-mesh corner".

Two tanks trading aggro spin the boss through the raid, so the off-tank is locked off him for the
**whole** encounter, not merely off the taunts: `SartharionMultiplier` zeroes `TankAssistAction`,
`AggressiveTargetAction`, `AttackAnythingAction` and every taunt whenever an assist tank's target is
the boss. Name those classes one by one — a cast to the shared `AttackAction` base also catches
`OsOffTankHoldAction`, whose `GetTarget()` reports the bot's *current* target, and would strand the
off-tank on the boss with nothing left able to switch it away.

Blocking re-acquisition cannot drop a target already picked up, so `os offtank hold` clears the target
outright when it has nothing to hold and is still swinging at the boss — `AttackStop()` alone is not
enough, see [../engine/pitfalls.md](../engine/pitfalls.md). That trigger is active for the **whole
encounter**, not only while adds are up.

**The off-tank idles on Tenebron's tank spot**, 60yd from Sartharion. Tenebron touches down 3.6yd
north of him, so he needs no repositioning to pick it up.

**No raid icons.** Bots resolve `PriorityTarget` per bot and attack it directly — no skull, no cross,
no marker bot. Accepted consequence: two bots can pick different units inside one priority tier, since
`FindUnitByEntries` is nearest-first per bot.

**Attack gates read `bot->GetVictim()`, never the `current target` AI value.** That value can never
hold a friendly — `AttackAction::Attack` bails on `IsFriendlyTo` long before it writes it — but every
friendly-target cast overwrites the client-side *selection*, and a tank buffing raid members does that
constantly. Gating on the AI value means the boss is never re-selected afterwards, so the hold actions
also restore `SetSelection` when the victim is right but the selection has drifted.

`os redirect threat` owns Tricks and Misdirection end to end: main tank inside the 10s pull window,
off-tank while any drake, Lava Blaze or whelp lives, main tank again after. The generic class nodes
only ever know the main tank, so `CastTricksOfTheTradeOnMainTankAction` and
`CastMisdirectionOnMainTankAction` are zeroed all fight — cast on those concrete classes, never the
shared `BuffOnMainTankAction` base, which also carries Beacon, Earth Shield, Thorns and Lifebloom.

## Twilight Realm

Portal `GO_TWILIGHT_PORTAL 193988`. During the Sartharion fight **all three drakes route to the same
shared portal** at `(3247.29, 529.80)` — `instance->DoAction(ACTION_ADD_PORTAL)`, hardcoded in
`instance_obsidian_sanctum.cpp`. Acolytes, disciples and eggs are `SetPhaseMask(16)`, so only shifted
bots see them and unshifted bots never accidentally target them. Symmetrically, a **shifted bot cannot
resolve Sartharion or the drakes** — in-realm logic must not depend on the boss.

Twilight Shift has **two ids, 57620 and 57874**, and the instance script removes only 57620. Both
count as shifted.

### Realm identity cannot be read off the portal

One refcounted GO serves all three drakes, and grid searches are phase-filtered —
`CreatureListSearcher` carries the searcher's phase mask even though its checker is purely geometric —
so a phase-1 bot cannot see which acolytes are inside. The only platform-visible signals are **Gift of
Twilight 58766 on the boss** (Shadron's realm) and **Twilight Torment 58835 / 57935 on the bot
itself** (Vesperon's). Both are cast at `EVENT_MINIBOSS_SPAWN_HELPERS`, 2s after the portal opens, so
the squad necessarily enters ~2s later than a bare "is the GO there" check would.

### The portal is one refcounted object

`ACTION_ADD_PORTAL` spawns the GO only if none exists and **resets `portalCount = 0`** when it does.
`ACTION_CLEAR_PORTAL` decrements and, at zero, deletes the GO **and force-removes Twilight Shift from
the entire raid**. Consequences:

- Bots get unshifted at a moment no bot chooses. An exit condition is advisory; the exit trigger
  exists only as a fast path.
- `portalCount` is `uint8` and **underflows**: Shadron's `SummonedCreatureDies` clears on every summon
  death while no Acolyte of Shadron is up, disciples included. The GO's presence proves nothing in
  either direction.
- Therefore: **enter whenever the GO exists, this bot is in the squad, and it is not already
  shifted.** Re-entry is normal and expected, not an error path.

**The squad skips Tenebron's realm** — it holds nothing worth the trip.

Squad is fixed at the pull by stable GUID sort and never reshuffled: **every DPS, melee included, plus
the first two healers and the second off-tank**. The main tank and the first off-tank always stay out.
`OnThePlatform` is false for a shifted bot, so realm melee are not dragged back at a landing drake.

### In-realm kill order is flat

Acolyte of Shadron (31218/31541) → Acolyte of Vesperon (31219/31543) → Disciples (30688/31544,
30858/31546).

**Twilight Eggs (30882/31539) are excluded outright** — from `TwilightAddsAlive` and from
`PriorityTarget`. They carry their own 25s fuse and hatch into **phase-1** whelps the off-tank already
picks up, mid-platform around `(3247, 528)`. Trading a raid-wide debuff for a few seconds of off-tank
threat is a bad deal every time.

**Gift of Twilight 58766 makes Sartharion immune, not merely tougher** — `SPELL_AURA_DAMAGE_IMMUNITY`
with school mask 127 (every school) plus +50% fire done. Granted by the Acolyte of Shadron, stripped
only when it dies. Every point of raid damage on the boss is discarded while it is up, which is why
that acolyte tops the list unconditionally and also hard-gates burst.

The two clocks overlap constantly in 3-drake: Tenebron's `EVENT_MINIBOSS_OPEN_PORTAL` **repeats every
60s** all fight, while Shadron's and Vesperon's acolytes stand until killed and re-arm 30s after they
die.

**Vesperon's portal is a blocker, not a nicety.** Its acolyte applies Twilight Torment (57935/58835):
map-wide, infinite, `AuraInterruptFlags = 0`, +75% shadow *and* fire taken, plus a 100%-chance proc
dealing 1712 to any player who deals damage (2s internal cooldown). **Positioning cannot avoid it** —
no movement logic should try. Never entering that portal leaves it on the raid for the rest of the
fight, stacked on Pyrobuffet's fire amplification. The pre-rework strategy gated both enter and exit
on the Acolyte of *Shadron*, so nobody ever went in.

## Other mechanics

- **Twilight Fissure** 30641 / H 31521, summoned by 57579 on a random target at 20s then every 22.5s
  by each drake. Void Blast 57581 (H 59128) is 4.0yd + 1.5 reach = **5.5yd lethal**, fired **5.1s
  after spawn**, once (`smart_scripts` `SMART_EVENT_UPDATE` at 5100ms). The fissure is
  **`UNIT_FLAG_NOT_SELECTABLE` for its whole life** (`creature_template.unit_flags` 33554432), so the
  default target search dropped it and this dodge had never once fired. Hence
  `FindUnitByEntries(..., requireSelectable = false)` here and nowhere else — that flag is also what
  tells a perched drake from a landed one.
- **The main tank dodges a fissure differently from everyone else**: along **Y** between waves, and
  **not at all** while a wave is up. A 10yd step on X swings his 60yd frontal cone from 138° to 66° —
  straight down the raid line at 57° — and his safe band (15yd left, 5yd right) has no room for it.
  One Void Blast on the tank beats a frontal breath on the whole ranged line.
- **Hold actions refuse a fissure-covered destination** (`FissureBlocks`). The dodge steps
  `FISSURE_CLEAR_RADIUS` 8 + `FISSURE_STEP_MARGIN` 2 = 10yd, further than `RAID_LINE_TOLERANCE_X` 8,
  so without the guard the hold walks the bot straight back onto the fissure and the two alternate
  every tick. Deliberately **unguarded**: the tsunami dodge (a wave beats a fissure), the fissure
  dodge itself, and the drake landing. The general form is in
  [../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md).
- **Lava Blaze** 30643 / H 31317, summoned by 57572 at a random player's position, 240s, no AI.
  Taunted at range, never chased.
- **Sartharion enrage**: 61632 at 30% HP (+500% damage), applied from the `DamageTaken` hook rather
  than a timer — check the aura with a health-percent fallback. At 10% every Fire Cyclone gains Lava
  Strike and the cycle drops to 1.4–2s, so blaze spawns accelerate hard.

## Burst gating

Default is suppressed. `SartharionBurstWindowMultiplier` reuses `IsBurstCooldownAction`
(`Base/Combat/BurstCooldowns.h`) and composes on top of `BurstWindowStrategy`'s own hold. The window
is a **one-way latch on Tenebron reaching `BURST_WINDOW_TENEBRON_PCT` 70%**, with the 30% enrage as a
backstop — and in both cases **only while Sartharion lacks 58766**, since a Bloodlust fired into a
full immunity is thrown away.

The earlier "two drakes landed and alive at once" rule is **dropped**: Tenebron lands at 30s and the
second at 75s, so it never opened first on any run that was going well.

## Nodes

| Trigger | Action | Priority |
|---|---|---|
| `os off platform` | `os return to platform` | `ACTION_EMERGENCY + 2` |
| `os twilight fissure` | `os avoid twilight fissure` | `ACTION_EMERGENCY + 1` |
| `os tsunami corridor` | `os tsunami corridor` | `ACTION_EMERGENCY` |
| `os drake landing` | `os drake landing position` | `ACTION_RAID + 5` |
| `os offtank hold` | `os offtank hold` | `ACTION_RAID + 4` |
| `os tranquilize` | `os tranquilize enrage` | `ACTION_RAID + 3` |
| `os redirect threat` | `os redirect threat` | `ACTION_RAID + 2` |
| `os main tank hold` / `os raid hold` / portal enter+exit | matching action | `ACTION_RAID + 1` |
| `sartharion dps` | `sartharion attack priority` | `ACTION_RAID` |
| `os sartharion flank` / `os drake rear` | matching action | `ACTION_MOVE + 5` |
| `sartharion melee positioning` | `rear flank` | `ACTION_MOVE + 4` |

`os off platform` outranks both dodges: off the arena nothing can hit the bot and the bot can do
nothing, so no mechanic is left worth reacting to. It moves to this bot's own hold at zero tolerance.
Bounded by the 200yd Sartharion search: enough, because it catches the first step off, not a bot
already halfway across the zone.

The corridor dodge outranks every hold, so the off-tank and melee **abandon their spots and dodge**;
it is Y-only and keeps X, so the west-to-east order re-forms on the new corridor. `"rear flank"` sits
below both deliberately — eating one Shadow Breath is survivable, standing in a tsunami is not.
Drakes lag behind while the off-tank crosses; that is expected, they are immune to the encounter's own
tsunami and re-path once the wave passes.

**A hold action that returns `false` on arrival hands the tick to the next action**, and
`ReachTargetAction` / `CombatFormationMoveAction` then nudge the bot out of its tolerance so the hold
drags it back — that is the tank oscillation. The multiplier therefore also zeroes:

- all generic movers for the main tank **while he is in melee range of the boss**. Gated on melee
  range, not on the encounter, so the pull still closes the gap.
- `CombatFormationMoveAction` and `FollowAction` for ranged and healers, all fight, plus every generic
  mover whose target is **Sartharion**. Reach actions stay live otherwise: `os raid hold` owns the
  line, but a drake parked at the east end still has to be closed on.
- **every generic mover for the off-tank, against every target.** He taunts drakes, blazes and whelps
  alike from wherever he stands, and his two hold actions produce every position he needs, clamped.
  Left open against a drake, `ReachTargetAction` walks him onto Vesperon's landing coord past the east
  edge — and `MoveOutOfCollisionAction` random-walks him off the anchor between drakes, because the
  holds return `false` inside their tolerance and `wotlk-os` runs out of combat on both engines.
- `MoveRandomAction`, `RunAwayAction` and `FleeAction` for everyone, all fight. `move random` is what
  carries a bot that has drifted off the arena across the zone; the other two move without corridor
  awareness, on the axis that kills.
- `RearFlankAction` whenever its target is Sartharion **or a drake** — see the layout section.
- **`CastBlinkBackAction` and `CastDisengageAction`, for everyone.** Both are unclamped 20yd
  displacements away from the current target with no idea the platform ends, and every other
  destination this strategy issues goes through `ClampDestination`. There is nowhere here that 20yd of
  blind travel is survivable.
- `AvoidAoeAction`, `MoveOutOfCollisionAction` and `MoveOutOfEnemyContactAction`, which clamp nothing.
  `avoid aoe` outranks every hold at `ACTION_EMERGENCY`, and what it flees — the fissure and the
  tsunami — is exactly what this strategy already dodges with clamped, corridor-aware destinations.
  The collision step is a random bearing taken whenever a parked bot is stacked with another, which
  here is the whole raid by design.
- `CastCasterFormAction` **for tanks only**, so a druid tank keeps bear form; a cat-spec druid still
  needs caster form for Rebirth. `CheckMountStateAction` goes for everyone — see
  [../engine/pitfalls.md](../engine/pitfalls.md).

**Pets follow `PriorityTarget`**, through `CommandPetAttack` / `StopPet` (`RaidBossHelpers`) issued
from `sartharion attack priority` **before** its already-on-target early return, or a pet whose owner
is already correct never gets the order. Skipped entirely while the owner is shifted: he resolves
phase-16 adds his phase-1 pet cannot touch, and leaving the last order standing keeps the pet on the
drake it is hitting.

Two tanks are a hard requirement. With no assist tank, `RequireOffTank` logs once per pull and every
off-tank behaviour stays inert — no pseudo-promotion, no MT-takes-everything fallback.

## Movement priority is a second ladder, and OS needs both

`ACTION_*` relevance decides which action *runs*; `MovementPriority` decides whether that action's
`MoveTo` is *accepted at all*, because `IsWaitingForLastMove` compares with a strict `>` (see
[../engine/pitfalls.md](../engine/pitfalls.md)). Every OS `MoveTo` once issued at `MOVEMENT_COMBAT`,
so a hold that had just stamped its lock silently refused the tsunami dodge for longer than the 3.6 s
the wave gives — the bot kept walking to the old destination and died on schedule.

The three emergency-band actions — `os return to platform`, `os avoid twilight fissure`,
`os tsunami corridor` — issue at **`MOVEMENT_FORCED`**. Holds, drake landing, flank and drake rear stay
at `MOVEMENT_COMBAT`. Both portal actions pass `MOVEMENT_COMBAT` explicitly, because the
`MoveTo(WorldObject*, distance, priority)` overload defaults to `MOVEMENT_NORMAL` — below every hold,
which left the walk to the portal perpetually preempted. All four `MoveToClamped` sites pass
`lessDelay = true`, subtracting the react delay from the stamped lock.

**Three `FORCED` dodges cannot preempt each other** (`FORCED > FORCED` is false), so precedence moves
to the multiplier layer. `OsMechanicPriorityMultiplier` ranks **off-platform > tsunami > fissure** —
off the platform is unrecoverable, a tsunami is lethal, a Void Blast is survivable. Each mechanic
whitelists its own action, passes non-movers through, and zeroes every other `MovementAction` while it
is live and nothing above it is. It is kept out of `SartharionMultiplier`, which is already ~140 lines
of generic-mover suppression and a different concern.

**`IsDuplicateMove` never fires here**, so do not lean on it. It needs the request within **0.01 yd**
of the last one, and every OS destination carries a Z that moves: passing `bot->GetPositionZ()` on a
platform floor running 58.6→59.6 makes the "same" destination a different point, and the pathfinding
branch stores the navmesh-resolved Z rather than the requested one. What actually throttles a
re-issuing action is the arrival tolerance plus the movement lock.

**Destination Z is derived at runtime, never surveyed.** `IssueMove` resolves the destination's own
ground level with an `INVALID_HEIGHT` fallback to the bot's Z, then validates with
`CheckCollisionAndGetValidCoords` — rejecting on failure for the dodges, where a blocked path means
try elsewhere, and accepting the clamped coordinates for routine holds, which just walk. OS
destinations are computed rather than measured, so a surveyed constant would not survive the slope.

## Known-open gaps

- **Trigger release and action arrival share one constant in three of the four pairs**, so a bot
  sitting at the edge of its tolerance can oscillate between "arrived" and "go again". The drake-rear
  pair is the only one built with a deliberate gap — a 140° gate against a 30° draw. Designed bands,
  never built: corridor Y 2.0/3.5 (stays well inside the 8.5 yd tsunami kill half-width), tank hold
  1.0/2.0 (the spots are tight and both are measured points), raid line X 8.0/12.0 (X is not a lethal
  axis, so a wide release is free).
- **Melee DPS have no post-pull hold.** A 5 s hold from `fightStartMs`, holding only the player's own
  attacks and casts while pets keep going, was specified and never built.
- **The raid's south-west corner clears Tail Lash by 1.24yd** while the tank is on his right hold —
  roughly 7s of every right wave, one cast at its 11s cooldown. Moving the raid south makes it worse.
  The mitigation, if it bites: `RAID_CORRIDOR_RIGHT_Y` 535.5 → 537.
- **Newest-drake-wins leaves the older drake trailing the off-tank.** He stays top of its threat
  table, so it follows him onto the newer drake's spot. Not unsafe — both stay off the raid line with
  their cones pointed away — but the measured spots only ever described a drake held alone. The fix,
  if it bites: a second assist tank.
- Whether phase-16 bots are truly immune to tsunamis is **assumed, not confirmed** — the tsunami
  creatures are phase 1, so they should not interact, but verify before relying on it.
