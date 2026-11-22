# Obsidian Sanctum — Sartharion (map 615)

Strategy key `wotlk-os`, files in `src/Ai/Raid/OS/`. Cross-raid conventions are in
[README.md](README.md). Verified against `src/server/scripts/Northrend/ChamberOfAspects/ObsidianSanctum/`,
`acore_world` and the DBC reference CSVs in `modules/mod-spell-tweaks/data/dbc-reference/`.

The strategy targets the 3-drake kill and degrades to 0/1/2-drake without special-casing. Like every
instance strategy it runs on both engines, so the holds run out of combat too, which is why the
off-tank is parked before the pull.

Everything sits in `Definition/OSDefinition_Sartharion.cpp`, behind his encounter gate, except the
realm kill order (`sartharion dps`) and `twilight portal exit`: a drake pulled alone starts its own
encounter, which closes his gate, and still opens a Twilight portal. `OS_BOSS_SARTHARION` copies
`DATA_SARTHARION` from the core's `obsidian_sanctum.h`, which sits under `scripts/` and can't be
included.

## Geometry

Sartharion home `(3246.57, 551.263, 58.62)`. The raid enters from the **south**, teleporting in at
`(3228.58, 385.86)` (`areatrigger_teleport` 5243) and walking north onto the platform. That X is
`RAID_ENTRY_ANCHOR_X`, the anchor every slot in the line is measured east of; the main tank drags
Sartharion south onto it. Safe to drag: `Creature::_IsTargetAcceptable` returns early on
`GetMap()->IsDungeon()` before any home-distance check and Sartharion sets no boundary, so there is
no leash.

Landing spots: Tenebron `(3249.75, 566.95)`, Shadron `(3230.50, 533.00)`, Vesperon
`(3269.71, 532.79)`. **All three sit inside a lethal tsunami band under both wave patterns**, so no
one may camp one while a wave is airborne.

**The walkable floor is smaller than the room and its east edge moves with Y.** Read off the map 615
Detour navmesh (`dtPoly.areaAndtype & 0x3f` is 1 ground, 2 magma; recast `(x,y,z)` is wow `(y,z,x)`):
floor **X 3219.5–3274.5, Y 486–574**, z 58.0–59.6 over a magma surface at 57.08. East edge by Y:
3267.5 at 511.5, 3274.5 at 528.5, 3273 at 535.5, 3272 at 552.5. Re-probe with navprobe
([../engine/pitfalls.md](../engine/pitfalls.md#movement-that-silently-no-ops)).

Two boxes, different jobs. **Destinations** clamp to the floor first, then inside a Range Marker
(`ClampDestination`). The floor clamp is **banded**, because one rectangle either admits lava or
gives away the east end; each band was swept at 0.5 yd and is 100% ground:

| Y | X |
|---|---|
| 487–492 | 3220–3231 |
| 492–496 | 3220–3242 |
| 496–504 | 3220–3250, the main tank's drag lane |
| 504–520, 566–569 | 3227–3266 |
| 520–566 | 3227–**3268**: corridors, drake spots, off-tank |

The west edge runs out to 3219.75 from Y 490 north, so the southern bands start at 3220: the tank's
right hold needs that yard. The floor clamp doubles as the freeze guard: with movement suppression
live, an off-navmesh `MoveTo` strands a bot for the fight. **Targets** use the looser `InsideRoom` box
`X 3208–3286, Y 474–591, Z ≤ 65`, so a Lava Blaze on the rim still counts while the drake perches do
not.

**The two boxes leave an 18 yd band of lava that reads as "inside the room"** (`ROOM_MAX_X` 3286
against a platform ending at 3268), so clamping every destination never caught a bot standing out
there. `OffThePlatform` (`OFF_PLATFORM_MARGIN` 1.0, opened by a yard so arrival tolerances do not
trip it) sits beside the room test, with the main tank exempt until the drag latches: his drag corner
is a hand-measured point 1.74 yd south of `PLATFORM_MIN_Y`.

## Pyrobuffet — the raid is only ever safe inside a Range Marker

Sartharion self-casts 56916 at engage, pulsing 57557 map-wide every 8 s: ~4000 fire plus a stacking
60 s +1000 fire-taken debuff. The **only** exemption is `ExcludeTargetAuraSpell = 56911` ("Range
Marker"), a 40 yd friendly area aura on two Safe Area triggers (NPC 30494) at `(3244.14, 512.60)` and
`(3242.84, 553.98)`. Anything outside both circles bleeds all fight.

The 15-minute despawn that would remove the triggers is scheduled into `extraEvents` but handled in
the `events` switch, so it is silently swallowed and the circles persist. The same swallow kills the
15-minute berserk — **there is no hard enrage to race**, which is what makes holding cooldowns safe.

`ClampToRangeMarker` pulls along **X only**, onto the chord the circle cuts at the current Y, to
`RANGE_MARKER_SAFE_RADIUS` (36 yd) of the marker whose chord is wider there (on the corridor holds
always the northern one). Y is the axis a tsunami kills on and must survive the clamp. Radial
fallback only when no X on that line is covered.

## Flame Tsunami — the two patterns are anti-phased

Creature 30616, side randomised per wave. Left spawns X 3211 travelling +X, right X 3286 travelling
−X. Lethal half-width **8.5 yd** (spell 57491 radius 7.0 + the player's 1.5 combat reach; the caster's
reach is not added).

| Wave | Spawn Y offsets | Danger bands | Gaps |
|---|---|---|---|
| Left | 476,484,492 \| 524,532,540 \| 572,580,588 | 467.5–500.5, 515.5–548.5, 563.5–596.5 | **500.5–515.5**, **548.5–563.5** |
| Right | 500,508,516 \| 548,556,564 | 491.5–524.5, 539.5–572.5 | 480–491.5, **524.5–539.5**, 572.5–588 |

**Each wave's gap centres are the other wave's band centres — no position survives both.** Bots react
per wave, and dodging is Y-only: a band sweeps the whole X range at 10.5 yd/s, outrunning a bot's 7.

The raid holds **two** corridors (see below). The tank pair carries its own X as well as a Y:

| `CorridorGroup` | Left wave | Right wave |
|---|---|---|
| **Tank** | **`(3221.3743, 513.0)`** (gap 500.5–515.5, on the southern Safe Area trigger) | **`(3220.0706, 490.0)`** (gap 480–491.5) |
| **Melee** | tank's, 513.0 | **raid's, 535.5** — they give this one up, see below |
| **Raid** | **551** (gap 548.5–563.5) | **535.5** (gap 524.5–539.5) |

The whole arrival disc must clear 8.5 yd, not just the hold, so neither tank hold is its in-game
reading. The right one read 490.581, 0.92 yd of headroom in a 5 yd band (floor 486.5, threshold
491.5); 490.0 buys the yard back. The left one read 511.089 and moved north into heal range between
waves, capped at 513.5 by melee sharing it on the raid's 2.0 yd tolerance. `TANK_ARRIVAL_TOLERANCE`
is **1.0** against 2.0 for the raid line — anything looser lets a bot "arrive" inside a band.

Corridors are **home-and-away, not sticky**: each group idles on the hold that is safe under one
pattern and moves only for the other. Waves cover ~11 s of every 25. A bot already clear of the live
pattern stays put, so the holds are the fallback, not the only safe ground: the off-tank's drake
spots and melee behind a drake clear some lines outright, and a 28 yd walk to a lane no safer costs
the trip twice.

Timing: first summon at +20 s, then every 25 s. Damage aura up at +3.6 s, down at +11 s, despawn
+13.5 s. Reaction window is 3.6 s plus travel — worst case 4.69 s at the east end of the line.

- **Detect on the creature spawning, never on the damage aura**, which appears at +3.6 s and spends
  the whole window.
- Spell 57491 effect 3 is `MOD_DECREASE_SPEED −50%` for 10 s, so a bot that grazes one wave **cannot
  clear the next**. Failure compounds; the first dodge has to be reliable.
- Wave side is read off the tsunami's Y. The two offset sets are disjoint whole numbers and Y never
  changes in flight, so the truncating integer compare is exact for the wave's whole life.
- Suppression lifts per bot once a wave has swept past its X, **or** once the wave is spent. A spent
  wave is scale 0.1 against 1.0 live (`EVENT_SARTHARION_FINISH_LAVA` shrinks it), so
  `GetObjectScale() < 0.5` is the second release test. It is needed because a right wave stops at
  X 3211 while the tank stands at 3220.07, clearing "swept past" by only 0.57 yd. Releasing on either
  test rather than on the 13.5 s despawn saves 2.85 s per wave for the tank and 5.5 s for the raid.
  Across the 16 yd raid band bots release up to ~1.5 s apart, which is safe: the release means the
  wave has passed that bot, and the walk home only changes Y.

## Molten Fury — why hunters matter here

The tsunami damage aura 57492 has a second periodic effect pulsing **60430 Molten Fury** every 800 ms
in **6 yd**: `DispelType 9 = DISPEL_ENRAGE`, +200% health, +100% damage, 30 s. **No `conditions`
rows**, so it lands on anything in range — Lava Blaze, a drake, or Sartharion himself.

Tranquilizing Shot 19801: two `SPELL_EFFECT_DISPEL` effects (MiscValue 9 enrage, 1 magic), 8 s
cooldown, 35 yd. Targets are assigned by hunter index modulo the enraged list so several hunters
spread instead of stacking dispels.

## Drakes

| Drake | Entry N/H | Called at | Held at |
|---|---|---|---|
| Tenebron | 30452 / 31534 | 20 s | `(3249.8467, 563.3801)` |
| Shadron | 30451 / 31520 | 60 s | `(3228.9385, 534.65955)` |
| Vesperon | 30449 / 31535 | 120 s | `(3266.0, 556.0)` |

**Each drake is held where it lands**, on its own measured spot — no shared pile. Assignment walks
the drakes **newest first**, because reverse call order is landing recency and the drake that just
touched down is the one with nobody on it. Vesperon is held **north of his landing**, so his breath
stops pointing down the raid line: level with his touchdown is only west of it, straight down the
line, whose east end is 9.5 yd away. From the north he settles around (3267, 549) facing up the empty
rim, 17 yd from the raid's home hold (outside the 15 yd breath), its left-wave hold 71° off his
facing. No spot is safe under **both** wave patterns, so a live wave takes the raid corridor Y and
keeps X; the east anchor is only for blazes and whelps. Shadron's spot clears every right line and
Vesperon's every left one. Under a right wave Tenebron's is the long walk, 27.9 yd to the raid
corridor, and the wave needs 6.2 s from its summon to reach that X.

The drakes are **boss-flagged**: `instance_encounters` credit rows for 30452/30451/30449 make
`ObjectMgr` OR in `CREATURE_FLAG_EXTRA_DUNGEON_BOSS`, and the heroic entries carry `type_flags 108`
(`CREATURE_TYPE_FLAG_BOSS_MOB`) instead. So the shared burst gate applies to them, which is why it
had to learn about off-tanks — see
[../systems/consumables-and-burst.md](../systems/consumables-and-burst.md).

Kill order is landing order. Drakes are `UNIT_FLAG_NOT_SELECTABLE` while airborne, so
`"possible targets"` never sees them and the 100 yd sight cap is too short anyway — use
`GetCreatureListWithEntryInGrid` at 200 yd.

**A perched drake is selectable.** `Reset()` clears the flag; only `EVENT_DRAGON_START_PATROL` sets
it, 500 ms after Sartharion's `JustEngagedWith` fires `ACTION_START_PATROL`. For that half second all
three are alive and targetable on spawns **off the arena** — Tenebron `(3239.07, 657.24, 86.88)`,
Vesperon `(3145.68, 520.71, 89.70)`, Shadron `(3363.06, 525.28, 98.36)` — which sent every DPS bot
and the off-tank running at a perch, out of combat and into `MoveRandomAction`. Hence `InsideRoom` on
every target search.

**Inbound is read off the flight speed rate, not off distance.** The script sets `MOVE_FLIGHT` rate
3.0 in the same handler that issues `MovePoint(POINT_LANDING)` and leaves the pre-call patrol at 1.0
(`Reset()`), so `GetSpeedRate(MOVE_FLIGHT) > 2.0f` splits "coming down" from "circling" exactly. ETA
is then distance / 21 yd/s. Distance alone reports a 3 s ETA minutes early, every time a patrol
waypoint passes near a landing spot.

## The encounter clock rebuilds on the combat edge

`GetSartharion` searches 200 yd and the entrance sits 166 yd from his spawn, so he resolves from
zone-in ([../engine/pitfalls.md](../engine/pitfalls.md#before-the-pull-and-out-of-combat)). Stamped
on first sight, every clock started then: the staleness branch could not fire, the drag always timed
out, the redirect pull window was already expired, and the portal squad was picked over whoever was
on the map. `StateFor` rebuilds on the **out-of-combat edge** instead.

## The raid splits across two corridors

Effective config: **`HealDistance` 38.5, `SpellDistance` 28.5** (`playerbots.conf.dist`, no `AC_`
override).

`CorridorGroupFor` returns **three** groups:

- healers and ranged → `Raid`, always. Walking to Sartharion is a 41 yd trip that ends in his Flame
  Breath, cast every 6 s — so they never do it, and `SartharionBossReachMultiplier` zeroes every
  generic mover whose target is the boss for them and the off-tank (the pull leaves the boss as his
  current target before the strategy is live).
- everyone shifted → `Raid`: the other splits are about his cones, and none exist in phase 16.
- main tank → `Tank`, all fight.
- melee whose victim is Sartharion → `Melee`, their own profile.
- everyone else, the off-tank included → `Raid`. He is locked off the boss all fight, so he never
  qualifies for `Melee`.

**`Melee` keeps the tank's lane except under a right wave, where it takes the raid's.** Below Y 492
the platform is 11 yd wide, so the clamp would put them 39.5° off his facing — inside a 98° Flame
Breath — with the healers 46 yd away. The 524.5–539.5 band is no better, since everything in it
inside his 20.83 yd melee range is also inside a 30 yd Tail Lash. So melee give the right wave up: it
costs them the ~11 s the wave is airborne, and 31 yd from the boss is well outside melee range anyway.

With no wave in the air the tank idles left and the raid idles right.

**There is no convergence latch.** The main tank keeps his own pair all fight. Walking the boss up to
the raid line would turn him and put that line inside his 60 yd frontal cone, which costs more than
the range it buys.

**The main tank is healable between waves** — 35.3 yd against `HealDistance` 38.5 — and out of range
only while a wave has the groups split (46.7 yd left, 53.7 yd right). The earlier "unhealable tank"
cost and its "raid left corridor → 510" mitigation are **retired; do not re-audit.**

Ranged stay outside `SpellDistance` 28.5 of Sartharion all fight (36.8 yd and 50.5 yd from the two
raid holds), so **only melee and the tanks ever damage him**.

The raid pair brackets the drake touchdowns at Y 533 and 567. Separation does **not** protect it:
Flame Breath reaches 60 yd, further than any distance on the platform. Only the angle does, which is
what the pull drag buys.

Accepted degradation: a Lava Blaze spawning on a player in the far corridor is outside the off-tank's
30 yd taunt range until the next wave lines the groups up.

## Shadow Breath is what fixes the layout

57570 / H 59126 is `TARGET_UNIT_CONE_ENEMY_24` — a **frontal cone, 60° wide and 15 yd deep**
(`spell_cone`; `EffectRadiusIndex 18` → `spellradius.reference.csv` id 18 = 15). `DoCastVictim` on a
**measured 17.5 s** cadence per landed drake.

A corridor is 15 yd wide with 4 yd margins, so each group is effectively collinear along X and
ordering is the only tool. **West to east, offsets from `RAID_ENTRY_ANCHOR_X` 3228.58: Sartharion +
main tank (+0, the boss +7) → ranged/healers (+20) → off-tank (+33).** The boss faces the main tank,
so his cones point west into the empty strip. Ranged and healers hold an 8 yd band along X
(`RAID_LINE_TOLERANCE_X`), not a point, so a dozen bots don't pile onto one coordinate and fight the
collision mover; Y keeps the tight corridor tolerance. Two rules enforce "only the off-tank eats
it":

1. **Nobody crosses west of a drake.** Everyone meets a landing 4 yd **east** of its coord, so the
   drake faces east into empty platform on touchdown, and the ranged line clears Shadron's
   (X 3230.50) by 18 yd. Vesperon is the exception: he touches down at X 3269.71, 2.3 yd short of the
   floor's east edge, so the off-tank waits just west of him and lets him walk the rest. Costs
   nothing — the ranged line is 21 yd from that touchdown, outside the cone for the whole transient.
2. **Melee take a flank, and which flank depends on the target.**

**Against Sartharion, melee take his north side**, southern only as a fallback. Both flanks are
equally safe from the cones; picking the one that survived the platform clamp followed his facing and
handed melee the south flank whenever a right wave swapped the tank over. The trigger fires on
standing in a cone **or** on being south of him: a dodge can leave melee south, safe from both
cones, and without the second test nothing would bring them back north. He is parked, so his own Y
holds and there is 14 yd between it and the northern flank — no room for the test to chatter. The
floor clamp can swing the chosen point into a cone, so it is re-tested after clamping. Gated on the
bot **actually standing in a cone**, not on being off the flank: he sits 10–12 yd from the tank, so a
corridor swap turns him ~99°, and tracking that every tick would walk melee a long arc every wave.

**Against a drake, melee take its rear outright** — gate 140°, draw ±30°. A drake has a frontal cone
and nothing off its tail, so the shared `RearFlankAction` 90°–120° *side* wedge is the wrong tool:
right for a mob with a tail attack, wrong for one without. `RearFlankAction` is therefore zeroed for
the boss **and** the drakes (`SartharionRearFlankMultiplier`) rather than out-prioritised, because
the actions replacing it release the tick once the bot is in position. The draw's mirror is the
fallback: Vesperon is held 2 yd from the east rim, so one side of his tail can clamp back out of the
arc.

Under a live wave the flank and drake-rear points keep X and take the wave-clear Y. His northern flank
lands around Y 518.7, 5.3 yd off the left line at 524; without this the corridor dodge and these
actions alternated for the whole ~11 s a wave was up.

The east anchor is clamped as the **east end of the line**, with the off-tank derived from it.

## Only the main tank ever touches Sartharion

**His combat reach is 18 yd** (`creature_model_info`, display 27035), so `GetMeleeRange` against a
player is 20.83, and he stops chasing the instant the tank is inside it: a tank walking to a fixed
anchor never gets him there.

| Spell | Direction | Arc | Radius | Cadence |
|---|---|---|---|---|
| Flame Breath 56908 | front | 82° | **60 yd** | 6 s |
| **Tail Lash 56910** | **rear** | 82° | 30 yd | 11 s |
| Cleave 56909 | victim only, no chain | — | 5 yd | 7 s |

Arcs come from `spell_cone` and are **full** angles, matching `HasInArc`. 56910 is rear-facing
because its `SpellVisual` is 3879, which `SpellMgr` turns into `SPELL_ATTR0_CU_CONE_BACK`. That
leaves two 98° side wedges; `InSartharionCone` tests a point against both through the engine's own
`HasInArc` / `isInBack`, so it cannot drift from the mechanic, with each cone widened by 8° a side
(`SARTHARION_CONE_MARGIN_DEGREES` 16) before anyone is called unsafe.

**One-time south drag.** At the pull the tank walks to `(3220.9905, 485.262)`, waits **5 s**, then
resumes his normal flow; Sartharion follows and settles at ≈`(3228.5, 504.7)`, inside his reach of
both tank holds, so he never walks again. No waves come in the first 20 s, so nothing interrupts the
dwell. Latched one-way.

The drag destination **deliberately bypasses `ClampDestination`**: it is a hand-measured point south
of the platform minimum and already inside a Range Marker, so both clamps could only damage it. The
corner is `NAV_MAGMA`, which bots path onto
([../engine/pitfalls.md](../engine/pitfalls.md#movement-that-silently-no-ops)). If the tank takes
damage there or never arrives, `Y 487.0` is the fallback; it costs under 1.5 yd of boss placement.

The drag ends when the tank arrives with the boss in melee range, or the boss is south of Y 522 by any
route, or `MAIN_TANK_DRAG_TIMEOUT_MS` 45000 elapses — **measured from the drag, not from the pull**.
The tank reaches the corner in ~3 s and the boss needs 7-9 s to cross the room, so arrival alone
proves nothing; the dwell lets him settle facing the tank, not mid-turn. Arrival is cleared if the
tank is pushed off, so a tsunami mid-wait restarts the dwell. The drag takes ~10 s and a wave can add
~11, so past the timeout the boss isn't following (a player holds threat), and the corner is lethal
under a left wave. The give-up path logs one `LOG_WARN` per pull with the tank's position, the only
way to tell "the boss never followed" from "`MoveTo` silently refused the off-mesh corner".

**`os main tank cooldown` owns the main tank's defensives.** Each drake's aura covers the raid from
the pull until that drake dies: Power of Shadron 58105 +100% fire taken, Vesperon's 61251 −25% health,
Tenebron's 61248 double shadow. The first two are what kill the main tank. The class nodes' bare
health trigger spends a 5-minute Shield Wall on the first Flame Breath, so a rule blocks them for him
and the node casts `NextTankDefensive` once a landed Shadron is at 50%
(`MAIN_TANK_COOLDOWN_SHADRON_PCT`), Shadron is gone, or the tank is below 25%
(`MAIN_TANK_COOLDOWN_PANIC_PCT`). The 25% release isn't latched: one dip buys one cooldown. The node
tops the RAID band, so a hold that keeps returning true can't starve it, and sits under every dodge.

Two tanks trading aggro spin the boss through the raid, so the off-tank is locked off him for the
**whole** encounter, not merely off the taunts: `SartharionOffTankBossMultiplier` zeroes the
TankAssist, AggressiveTarget, AttackAnything and Taunt families whenever an assist tank's target is
the boss. Never the `Attack` family — the shared `AttackAction` base also catches
`OsOffTankHoldAction`, whose `GetTarget()` reports the bot's *current* target, and would strand the
off-tank on the boss with nothing left able to switch it away. `SartharionMainTankTargetMultiplier`
keeps the main tank's taunts and tank assist on Sartharion alone.

Blocking re-acquisition cannot drop a target already picked up, so `os offtank hold` clears the target
outright when it has nothing to hold and is still swinging at the boss (`AttackStop()` alone is not
enough: [../engine/pitfalls.md](../engine/pitfalls.md#before-the-pull-and-out-of-combat)). That
trigger is active for the **whole encounter**, not only while adds are up.

**The off-tank idles on Tenebron's tank spot**, 60 yd from Sartharion. Tenebron touches down 3.6 yd
north of him, so he needs no repositioning to pick it up.

**`OffTankTauntTarget` runs first in his hold**: a landed drake not on him, inside taunt range. He
attacks only the newest drake, so the tick one is in range is the tick he sets off: a handover walks
him 35.5 yd, and by the time the old drake peels it is outside the 30 yd taunt.

**No raid icons.** Bots resolve `PriorityTarget` per bot and attack it directly — no skull, no cross,
no marker bot. Accepted consequence: two bots can pick different units inside one priority tier,
since `FindUnitByEntries` is nearest-first per bot.

**Attack gates read `bot->GetVictim()`, never the `current target` AI value.** That value can never
hold a friendly — `AttackAction::Attack` bails on `IsFriendlyTo` long before it writes it — but every
friendly-target cast overwrites the client-side *selection*, and a tank buffing raid members does that
constantly. Gating on the AI value means the boss is never re-selected afterwards, so the hold actions
also restore `SetSelection` when the victim is right but the selection has drifted. Acquiring a
target never costs the tick, the hold falls through to its move: when the off-tank's hold returned
there, he chased a drake to wherever it stood, its perch included, instead of taunting it from his
spot.

`os redirect threat` owns Tricks and Misdirection end to end: main tank inside the 10 s pull window,
off-tank while any drake, Lava Blaze or whelp lives, main tank again after. The generic class nodes
only ever know the main tank, so `tricks of the trade on main tank` and `misdirection on main tank`
are zeroed all fight, by name. A rogue redirects off his own target instead: Tricks moves everything
for 6 s, so it goes to the tank of what he hits (the off-tank on a drake, the main tank on the boss,
nobody otherwise), never on a Lava Blaze, which dies to one Fan of Knives and takes the 30 s cooldown
with it. Hunters keep the raid-wide target: Misdirection is three shots, and the off-tank pulling a
whelp off a healer is worth one.

## Twilight Realm

Portal `GO_TWILIGHT_PORTAL 193988`. During the Sartharion fight **all three drakes route to the same
shared portal** at `(3247.29, 529.80)` — `instance->DoAction(ACTION_ADD_PORTAL)`, hardcoded in
`instance_obsidian_sanctum.cpp`. Acolytes, disciples and eggs are `SetPhaseMask(16)`, so only shifted
bots see them and unshifted bots never accidentally target them. Symmetrically, a **shifted bot cannot
resolve Sartharion or the drakes** — in-realm logic must not depend on the boss.

The walk to the portal is not clamped: it sits 17 yd from the southern Safe Area trigger, well inside
its Range Marker. It also sits 2.2 yd off the left wave line at 532, so entering and leaving both wait
while a wave can still reach its Y. A shifted bot can't see the tsunamis (phase 1), so
`ClassifyTsunamiWave` reads them through `PlatformEyes`: the main tank or the first off-tank, the two
bots never in the realm, else anyone still outside. The reach test still runs against the shifted
bot's own X, since coordinates are shared across phases.

Twilight Shift has **two ids, 57620 and 57874**, and the instance script removes only 57620. Both
count as shifted.

### Realm identity cannot be read off the portal

One refcounted GO serves all three drakes, and grid searches are phase-filtered —
`CreatureListSearcher` carries the searcher's phase mask even though its checker is purely geometric —
so a phase-1 bot cannot see which acolytes are inside. The only platform-visible signals are **Gift of
Twilight 58766 on the boss** (Shadron's realm) and **Twilight Torment 58835 / 57935 on the bot
itself** (Vesperon's). Both are cast at `EVENT_MINIBOSS_SPAWN_HELPERS`, 2 s after the portal opens, so
the squad necessarily enters ~2 s later than a bare "is the GO there" check would.

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
the first two healers and the second off-tank**, since the acolyte dies to whatever the raid brings
and damage left on the platform only lengthens the immunity. The main tank and the first off-tank
always stay out. `OnThePlatform` is false for a shifted bot, so realm melee are not dragged back at a
landing drake.

### In-realm kill order is flat

Acolyte of Shadron (31218/31541) → Acolyte of Vesperon (31219/31543) → Disciples (30688/31544,
30858/31546).

**Twilight Eggs (30882/31539) are excluded outright** — from `TwilightAddsAlive` and from
`PriorityTarget`. They carry their own 25 s fuse and hatch into **phase-1** whelps the off-tank
already picks up, mid-platform around `(3247, 528)`. Trading a raid-wide debuff for a few seconds of
off-tank threat is a bad deal every time.

**Gift of Twilight 58766 makes Sartharion immune, not merely tougher** — `SPELL_AURA_DAMAGE_IMMUNITY`
with school mask 127 (every school) plus +50% fire done. Granted by the Acolyte of Shadron, stripped
only when it dies. Every point of raid damage on the boss is discarded while it is up, which is why
that acolyte tops the list unconditionally and also hard-gates burst.

The two clocks overlap constantly in 3-drake: Tenebron's `EVENT_MINIBOSS_OPEN_PORTAL` **repeats every
60 s** all fight, while Shadron's and Vesperon's acolytes stand until killed and re-arm 30 s after they
die.

**Vesperon's portal is a blocker, not a nicety.** Its acolyte applies Twilight Torment (57935/58835):
map-wide, infinite, `AuraInterruptFlags = 0`, +75% shadow *and* fire taken, plus a 100%-chance proc
dealing 1712 to any player who deals damage (2 s internal cooldown). **Positioning cannot avoid it** —
no movement logic should try. Never entering that portal leaves it on the raid for the rest of the
fight, stacked on Pyrobuffet's fire amplification; a strategy gating both enter and exit on the
Acolyte of *Shadron* never went in.

## Other mechanics

- **Twilight Fissure** 30641 / H 31521, summoned by 57579 on a random target at 20 s then every
  22.5 s by each drake. Void Blast 57581 (H 59128) is 4.0 yd + 1.5 reach = **5.5 yd lethal**, fired
  **5.1 s after spawn**, once (`smart_scripts` `SMART_EVENT_UPDATE` at 5100 ms). The fissure is
  **`UNIT_FLAG_NOT_SELECTABLE` for its whole life** (`creature_template.unit_flags` 33554432), so the
  default target search drops it and a dodge built on it never fires. Hence
  `FindUnitByEntries(..., requireSelectable = false)` here and nowhere else — that flag is also what
  tells a perched drake from a landed one.
- **The main tank dodges a fissure differently from everyone else**: along **Y** between waves, and
  **not at all** while a wave is up. A 10 yd step on X swings his 60 yd frontal cone from 138° to 66° —
  straight down the raid line at 57° — and his safe band (15 yd left, 5 yd right) has no room for it.
  One Void Blast on the tank beats a frontal breath on the whole ranged line. South comes first: it
  keeps both cones off the line and puts him back on his hold X. North is no better than east, since
  `PlatformMinX` steps from 3220 to 3227 at Y 520 and turns a north step into a 5.6 yd move east. X
  steps stay as the tail, for a fissure pinning him against the southern rim.
- **The off-tank steps along Y first too**: he sets the drake's facing. Held from the west on
  Shadron's spot, a 10 yd step east turns its breath onto the raid line, whose west end is 10.2 yd off
  on the same Y. X steps are his tail.
- **Everyone else steps along X first**, keeping the Y it stands on rather than the corridor's (melee
  on a drake stand up to 28 yd off it). The corridor is 15 yd wide against a 5.5 yd blast, and a bare
  `MoveAway` picks Y and fights the corridor action. Y steps are the rim fallback, and a live wave
  still sets Y.
- **Hold actions refuse a fissure-covered destination** (`FissureBlocks`): the dodge steps
  `FISSURE_CLEAR_RADIUS` 8 + `FISSURE_STEP_MARGIN` 2 = 10 yd (the margin keeps it off the trigger
  boundary, where the dodge would fire again), past `RAID_LINE_TOLERANCE_X` 8
  ([../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md#movement-that-oscillates)).
  Deliberately **unguarded**: the tsunami dodge (a wave beats a fissure), the fissure dodge itself,
  and the drake landing.
- **Lava Blaze** 30643 / H 31317, summoned by 57572 at a random player's position, 240 s, no AI.
  Taunted at range, never chased.
- **Sartharion enrage**: 61632 at 30% HP (+500% damage), applied from the `DamageTaken` hook rather
  than a timer — check the aura with a health-percent fallback. At 10% every Fire Cyclone gains Lava
  Strike and the cycle drops to 1.4–2 s, so blaze spawns accelerate hard.

## Burst gating

Default is suppressed. `SartharionBurstWindowMultiplier` reuses `IsBurstCooldownAction`
(`Base/Combat/BurstCooldowns.h`, mana-return cooldowns exempt) on top of `BurstWindowStrategy`'s own
hold. The window is a **one-way latch** opening once Shadron is on the ground or gone (past the 10 s
pull window with no Power of Shadron 58105), with his enrage (61632, or ≤ 30%) as a backstop — and
**only while Sartharion lacks 58766**, since a Bloodlust fired into a full immunity is thrown away.
The interlock holds it while Shadron's acolyte lives, so it lands the first moment the raid can hit
the boss with a second drake up. Superseded: a latch on Tenebron at 70%, and "two drakes landed at
once", which never opened first on a good run (Tenebron lands at 30 s, the second at 75 s).

## Nodes

| Trigger | Action | Priority |
|---|---|---|
| `os off platform` | `os return to platform` | `ACTION_EMERGENCY + 2` |
| `os twilight fissure` | `os avoid twilight fissure` | `ACTION_EMERGENCY + 1` |
| `os tsunami corridor` | `os tsunami corridor` | `ACTION_EMERGENCY` |
| `os main tank cooldown` | `os main tank cooldown` | `ACTION_RAID + 7` |
| `os tank shapeshift` | `os tank shapeshift` | `ACTION_RAID + 6` |
| `os drake landing` | `os drake landing position` | `ACTION_RAID + 5` |
| `os offtank hold` | `os offtank hold` | `ACTION_RAID + 4` |
| `os tranquilize` | `os tranquilize enrage` | `ACTION_RAID + 3` |
| `os redirect threat` | `os redirect threat` | `ACTION_RAID + 2` |
| `os main tank hold` / `os raid hold` / portal enter+exit | matching action | `ACTION_RAID + 1` |
| `sartharion dps` | `sartharion attack priority` | `ACTION_RAID` |
| `os drake rear` | `os drake rear` | `ACTION_MOVE + 6` |
| `os sartharion flank` | `os sartharion flank` | `ACTION_MOVE + 5` |
| `sartharion melee positioning` | `rear flank` | `ACTION_MOVE + 4` |

`os off platform` outranks both dodges: off the arena nothing can hit the bot and the bot can do
nothing, so no mechanic is left worth reacting to. It moves to this bot's own hold at zero tolerance;
the off-tank takes both of his anchor's axes, since his hold is a drake spot and the corridor Y only
bought a second walk. Bounded by the 200 yd Sartharion search: enough, because it catches the first
step off, not a bot already halfway across the zone.

`os tank shapeshift` puts a druid tank back in bear form: the class strategy does that only from the
combat engine, and the off-tank is out of combat between drakes. Only a druid who knows a bear form
counts: one with neither spell is a healer or moonkin standing in.

The corridor dodge outranks every hold, so the off-tank and melee **abandon their spots and dodge**;
it is Y-only and keeps X, so the west-to-east order re-forms on the new corridor. `"rear flank"` sits
below both deliberately — eating one Shadow Breath is survivable, standing in a tsunami is not.
Drakes lag behind while the off-tank crosses; that is expected, they are immune to the encounter's own
tsunami and re-path once the wave passes.

**A hold action that returns `false` on arrival hands the tick to the next action**, and
`ReachTargetAction` / `CombatFormationMoveAction` then nudge the bot out of its tolerance so the hold
drags it back — the tank oscillation. So the `sartharion` rules also zero:

- every generic mover, for everyone, **only while a dodge is live**: held permanently it is the freeze
  bug, a silently failing `MoveTo` with nothing left to move the bot.
- every generic mover for the main tank **while he is on the boss**, gated on melee range and the
  drag rather than the encounter, so the pull still closes the gap.
- the formation movers (`CombatFormationMove`, `TankFace`, `SetBehind`, `Follow`) for ranged and
  healers, plus every generic mover whose target is **Sartharion**. Reach stays live otherwise:
  `os raid hold` owns the line, but a drake parked at the east end still has to be closed on.
- **every generic mover for the off-tank, against every target.** He taunts drakes, blazes and whelps
  from wherever he stands, and his two clamped holds give every position he needs; left open against
  a drake, `ReachTargetAction` walks him onto Vesperon's landing coord past the east edge.
- `MoveRandomAction`, `RunAwayAction` and `FleeAction` for everyone: `move random` carries a bot that
  drifted off the arena across the zone, and the other two move on the axis that kills.
- `RearFlankAction` whenever its target is Sartharion **or a drake** — see the layout section.
- **`CastBlinkBackAction` and `CastDisengageAction`, for everyone**: unclamped 20 yd displacements
  with no idea the platform ends.
- `AvoidAoeAction`, `MoveOutOfCollisionAction` and `MoveOutOfEnemyContactAction`, which clamp nothing.
  `avoid aoe` outranks every hold and flees what this strategy already dodges with clamped,
  corridor-aware destinations; the collision step takes a random bearing whenever a parked bot is
  stacked, which here is the whole raid by design, and walked the off-tank off his anchor between
  drakes.
- `CastCasterFormAction` **for tanks only** and `CheckMountStateAction` for everyone
  ([../engine/pitfalls.md](../engine/pitfalls.md#before-the-pull-and-out-of-combat)).
- In the Twilight Realm, where the boss cannot be resolved and every other rule is off, a bot waiting
  out a wave keeps the generic, wander, collision and enemy-contact movers off, or they walk it back
  into a lane before the shift is stripped.

**Pets follow `PriorityTarget`**, through `CommandPetAttack` / `StopPet` (`EncounterHelpers`) issued
from `sartharion attack priority` **before** its already-on-target early return, or a pet whose owner
is already correct never gets the order. Skipped entirely while the owner is shifted: he resolves
phase-16 adds his phase-1 pet cannot touch, and leaving the last order standing keeps the pet on the
drake it is hitting.

Two tanks are a hard requirement. With no assist tank, `RequireOffTank` logs once per pull and every
off-tank behaviour stays inert — no pseudo-promotion, no MT-takes-everything fallback.

## Movement priority

Why `MovementPriority` is a second ladder beside `ACTION_*` relevance, and why a same-priority hold
lock silently refused the tsunami dodge here, is in
[../engine/pitfalls.md](../engine/pitfalls.md#movement-that-silently-no-ops). OS's assignment:

- The three emergency-band actions — `os return to platform`, `os avoid twilight fissure`,
  `os tsunami corridor` — issue at **`MOVEMENT_FORCED`**. Holds, drake landing, flank and drake rear
  stay at `MOVEMENT_COMBAT`.
- Both portal actions pass `MOVEMENT_COMBAT` explicitly: the `MoveTo(WorldObject*, distance,
  priority)` overload defaults to `MOVEMENT_NORMAL`, below every hold, which left the walk to the
  portal perpetually preempted.
- All four `MoveToClamped` sites pass `lessDelay = true`, subtracting the react delay from the
  stamped lock.
- The three `FORCED` dodges cannot preempt each other, so the `os mechanic priority` rules rank them
  **off-platform > tsunami > fissure** — off the platform is unrecoverable, a tsunami is lethal, a
  Void Blast is survivable. Each is an `Exclusive` passing its own action while its mechanic is live
  and nothing above it is. The rules read the same three predicates as the triggers (`LiveMechanic`,
  kept for the bot's tick): a rule claiming a mechanic its trigger won't fire on leaves the bot with
  every mover off and nothing to replace them.

**Destination Z is derived at runtime, never surveyed.** `IssueMove` resolves the destination's own
ground level with an `INVALID_HEIGHT` fallback to the bot's Z, then validates with
`CheckCollisionAndGetValidCoords` — rejecting on failure for the dodges, where a blocked path means
try elsewhere, and accepting the clamped coordinates for routine holds, which just walk. The floor
runs 58.6→59.6, so a surveyed constant would not survive the slope.

## Diagnosing a pull

Each pull writes `<LogsDir>/botobs/615_<inst>_sartharion_<epoch>.ndjson`; `postmortem.py <file>
--notes sartharion.` replays what the raid decided. Schema and probe rules:
[../systems/observability.md](../systems/observability.md).

| key | answers |
|---|---|
| `sartharion.wave` | the pattern that can still reach this bot - `none`/`left`/`right` |
| `sartharion.corridor` | the profile it was sorted into and the hold that follows - `tank 513.0` |
| `sartharion.offtank` | which drake set his anchor, and whether a wave took the spot's own Y |
| `sartharion.target` | where a bot was *sent*; `GetVictim()` only says where it ended up |
| `sartharion.defensive` | which of the nine buttons the cooldown node picked, or `covered`/`none` |
| `sartharion.drag` | `walking` / `onpoint` / `settled` / `timeout` |
| `sartharion.realm` | `enter` / `notworth` / `noportal` |
| `.dragdone`, `.burstwindow`, `.tankcdwindow` | the three one-way latches, written on the flip |
| `sartharion.portalsquad` | the squad, struck once at the pull |

`wave` and `corridor` are derived per bot against that bot's own X, so bots legitimately disagree -
two of one group on different holds at the same `t` is the half-and-half split that wipes the raid.

**Waves reach the trace only as `haz` rows**, written once each by `NoteTsunamiHazards` from the
definition's tick, on one mechanic-tracker bot per instance, since `ClassifyTsunamiWave` already
costs ten grid searches per bot per tick. Nothing else records them: a wave never enters combat so
the watch list skips it, its damage is an aura on the creature rather than a dynamic object so there
is no swept-hazard row, and the 40-slot snapshot sweep is spent on Onyx Sanctum trash and 52 cosmetic
Twilight Eggs before it reaches one.

## Known-open gaps

- **Trigger release and action arrival share one constant in three of the four pairs**, so a bot
  sitting at the edge of its tolerance can oscillate between "arrived" and "go again". The drake-rear
  pair is the only one built with a deliberate gap — a 140° gate against a 30° draw. Designed bands,
  never built: corridor Y 2.0/3.5 (stays well inside the 8.5 yd tsunami kill half-width), tank hold
  1.0/2.0 (the spots are tight and both are measured points), raid line X 8.0/12.0 (X is not a lethal
  axis, so a wide release is free).
- **Melee DPS have no post-pull hold.** A 5 s hold from `fightStartMs`, holding only the player's own
  attacks and casts while pets keep going, was specified and never built.
- **The raid's south-west corner clears Tail Lash by 1.24 yd** while the tank is on his right hold —
  roughly 7 s of every right wave, one cast at its 11 s cooldown. Moving the raid south makes it
  worse. The mitigation, if it bites: `RAID_CORRIDOR_RIGHT_Y` 535.5 → 537.
- **Newest-drake-wins leaves the older drake trailing the off-tank.** He stays top of its threat
  table, so it follows him onto the newer drake's spot. Not unsafe — both stay off the raid line with
  their cones pointed away — but the measured spots only ever described a drake held alone. The fix,
  if it bites: a second assist tank.
- Whether phase-16 bots are truly immune to tsunamis is **assumed, not confirmed** — the tsunami
  creatures are phase 1, so they should not interact, but verify before relying on it.
