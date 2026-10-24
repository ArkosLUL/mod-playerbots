# Mimiron

## Firefighter

A player presses the Big Red Button before the pull; `_hardmode` is set at activation and never
cleared. Bots do not detect it: `IsMimironHardModeActive` reads `Playerbots.UlduarMimironHardMode`
and nothing else, so the config is a declaration that has to match what the raid does, and it holds
all over Ulduar, not only in this room
([a config read is not a gate](../../engine/pitfalls.md#movement-that-silently-no-ops)).
`SPELL_EMERGENCY_MODE (64582)` is the empower aura on whichever mech is active, if a live signal is
ever wanted; the mechs *are* valid attack targets, unlike Mimiron himself.

**The fire is the boss.** Flames 64566 was the largest single damage source on the first pulls, 26-45%
of everything the raid took and roughly half of all effective healing. With the dodges below it fell to
about 20%, behind Heat Wave, though a long phase 3 still puts it back on top (27.7% of one pull).

Mimiron seeds it for the whole encounter, handovers included: `EVENT_SPAWN_FLAMES_INITIAL` every
**30 s** drops three `NPC_FLAMES_INITIAL` (34363) 5 yd from three random raid members. Each chain then
adds one `NPC_FLAMES_SPREAD` (34121) every **5.75 s**, **7 yd toward the player nearest that chain's
newest node**, and **stops growing while any player is inside 4 yd of that node**. Nodes carry aura
64561, which ticks 64566 for ~3.1k a second inside **3 yd**, and they never expire. Both entries are
non-selectable triggers, found by scanning `"nearest npcs"`
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)).

**Only a node carrying 64561 is fire.** The seed is a NullCreatureAI that parks its first spread node
on its own spot, never burns, and outlives its fire: `EVENT_FLAMES_SPREAD` breaks without
rescheduling once `_createTime < GetData(10)`, so it never reaches its despawn. A put-out node drops
the aura 2.5 s before it despawns. Counting seeds made every seed spot two nodes, which overrides the
dodge's health gate: only ~200 of one pull's 636 fire dodges were real. `ReadMimironFirefighterHazards`
reads the aura.

**Where the raid stands is where the fire goes, and that is the only lever over it.** Four things put
fire out and nothing else does: the Frost Bomb; VX-001's Flame Suppressant (65192, 10 yd around itself
every 10 s in phase 2, with a 51% cast slow, so not a place to stand); one full-room clear 60 s into
phase 1 (**64570**, cast alongside **Clear Fires 65229**); and the phase 3 Emergency Fire Bots.

**The room clear happens once.** `EVENT_FLAME_SUPPRESSION_50000` is scheduled a single time, 60 s into
phase 1, so everything after it burns until phase 2. The cleanse is a deadline: its usable window is
**81-97 s**, so the MK II has to die by about **100 s** to spend it, while it dies at **112-133 s**.
Closing that gap is worth roughly **13-20% more phase-1 damage**, the largest known-open item on this
fight.

### Phase 1 is fought in the west

The MK II is held at `ULDUAR_MIMIRON_PHASE1_TANK_SPOT` **(2691.576, 2568.532)**, 53 yd off centre, and
ranged and healers camp 21-33 yd off it toward one of `ULDUAR_MIMIRON_PHASE1_STACK_SPOTS`, so every seed
lands out there rather than on the ground VX-001 is summoned onto. Phase-1 fire damage taken inside
20 yd of the centre went from **70% to 0.0%**, with no node within 24 yd of centre at the handover.

The tank spot is **51.5 yd from Mimiron's own spawn** (2742.53, 2560.99), and he evades past **80 yd**
from his home on every tick (`boss_mimiron` `UpdateAI`): the encounter's only leash, the MK II has none
of its own. navprobe `--nav 0x09`: 0.223 to poly, flat at **Z 364.314**, 16/16 at 12 yd and 14/16 at
18 and 24, every miss in the raised west doorway alcove.

The camp is a wedge of **per-bot slots** round the tank spot (`MimironPhase1CampSlot`): rows at **21, 27
and 33 yd**, ±38°, 16 slots, never closer than 5.5 yd even full. Its centreline runs through one of two
anchors, each a middle-row centre, index 0 by default:

| # | bearing | x | y |
|---|---|---|---|
| 0 | 25° | 2716.047 | 2579.942 |
| 1 | 345° | 2717.656 | 2561.543 |

navprobe 1° rings at 21, 27 and 33 yd: clean floor from **288° to 74°**, so every slot for 1-16 bots is
on mesh at Z 364.314, the nearest ~20 yd from centre. 105-255° is off mesh against the west wall or in
the alcove, and 90° and 270° sit on holes; the northern one runs **y 2582 to 2591**, 3.6-4.8 yd off poly
with Z never settling.

**Every slot has to see the MK II**, or the bot drops its target and `follow` walks it off the slot
([out of sight is an invalid target](../../engine/pitfalls.md#things-the-bot-cannot-see)). The tank spot
sits ~3.4 yd inside the doorway alcove, whose side walls end at x ≈ 2695, y ≈ 2553 and 2585, and the MK
II spends 45% of phase 1 west of the spot, deeper in. Anchors at 30°/330° hid the worst edge slot **21%
and 38%** of the time, 25°/345° **1.5% and 1.2%**; the south shadow is wider because the MK II drifts
south-west. Sliding the camp east buys nothing, since the range shift drags it back, and ±32° fixes
only the north wedge.

That margin is thin: 2 yd more westward drift puts the south edge back at 34%. `MimironPhase1CampTurn`
turns the whole camp toward the room in 4° steps up to 16° until every slot sees the MK II
(`Map::isInLineOfSight` along the `IsWithinLOSInMap` ray from a 2 yd eye, fixed only so every bot builds
the same camp: 1.2 or 2.5 yd answers the same), raid-wide on the 250 ms scan; it rises at once and
falls back only after 15 s (`mimiron.campturn`). In that stress case ≤ 8° clears 98% of positions; the
static anchors alone took `follow <-> arc spread` A-B-A from 58 a pull to 0.

Sight is checkable offline: `navprobe --map 603 --collision 2.0 ring 2691.5762 2568.5315 364.3138 27 24
--los-from <boss>` reports it per slot. Against the deepest traced MK II position it clears only **300°
to 45°**, inside the 288-74° floor arc, and goes blind at 285° south against 60° north, the same
south-wider asymmetry.

**The wedge is fixed, never tracking the boss**: chains grow toward whoever is nearest their head, so a
drifting camp smears the field along behind it. It gives ground only as one rigid piece, when its
farthest slot is out of casting range, because a slot past range deadlocks instead of correcting
(`reach spell` at `ACTION_HIGH` against the formation at `ACTION_RAID`), which is also what a dead main
tank looks like. Range is the bots' own test, `IsWithinCombatRange` with both reaches:
`spellDistance + 8 + 1.5 - margin`, about **34 raw**; a raw `spellDistance - margin` is 24.5, Napalm's
floor ([range and reach](../../engine/raid-mechanics-lessons.md#range-and-reach)).

**The field converges on the camp, so its own anchor burns first**: one pull took 538k phase-1 flame
damage, 26% of everything taken and nearly all of it on ranged and healers, with the median victim
3.4 yd from the anchor. `GetMimironPhase1StackAnchor` walks the whole raid to a cleaner one: it counts
live nodes within `ULDUAR_MIMIRON_STACK_FIRE_RADIUS` (10) of each anchor, switches when the live one
carries more than `_FIRE_LIMIT` (2) **and** another is cleaner by `_FIRE_MARGIN` (2), then holds it
`_HOLD_MS` (15 s). Hysteresis both ways: chains grow 1.22 yd/s, so a bare "stand on the cleanest" paces
the raid across the arc all phase. The two anchors' middle rows are 18.5 yd apart, clear of a 5 yd node
cluster plus a 7 yd chain step; 30° neighbours would move them only 14. **Decided once per instance** in
`MimironFightState`, never per bot: twelve bots each picking their own anchor is twelve camps.

Napalm Shell is why it is slots and not a point
([Napalm Shell wants one bot per shell](#napalm-shell-wants-one-bot-per-shell)). All of this is
Firefighter-only; normal mode has no fire and keeps the room centre, the ring and 5.5 yd.

## The Frost Bomb is 30 yd, and it lands in your own fire

VX-001 casts 64623 at `SPELLVALUE_MAX_TARGETS 1` from 1 s into phases 2 and 4, repeating every 45 s.
`acore_world.conditions` restricts it to entry **34121 carrying aura 64561**, so it never targets a
player: it picks a burning node, which summons `NPC_FROST_BOMB` (34149) at the end of 64623's 2 s cast
and flight, 2.5-4 s after VX-001 starts it, and a cast can summon nothing. The bomb's SmartAI detonates
**exactly 10 s later**: 65333 in 25-man, **30 yd, 47124 base**, plus a knockback, plus a dummy effect
that despawns every flame it catches. It is the raid's fire extinguisher as much as its hazard, and
gathering the fire into one part of the room is what aims it. Bots avoid the bomb *creature*, so no
spell id is needed and 10/25 are covered identically.

47k against a 22-24k pool is a pure positional check that healing cannot answer, and an easy one: the
worst-placed bot at spawn needed 23 yd, 3.3 s against the 10 s fuse, and navprobe has the room flat and
on mesh to 45 yd on all 16 headings and 15/16 at 55.

**Run 30, stand 32, flee 34.** One radius for trigger and flee (a placeholder 12) left everyone between
12 and 30 yd unmoved and stopped the rest inside the blast: one bomb killed 13-15 of 25 in each of three
pulls. Now `ULDUAR_MIMIRON_FROST_BOMB_RADIUS` (30) is the trigger, which lets go at ~31 since
`FindNearestCreature` adds both bounding radii; every destination screen (`IsMimironSpotBombSafe`) tests
`ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS` (32); and the escape takes the per-bearing hop onto
`ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE` (34), since one radial
gap swept over the fan reaches the clearance only straight away (136 of 211 escapes landed short of it).

**The generic movers walk bots back onto the edge**, and the flee ping-pongs there for the whole fuse:
114-150 `frost bomb <-> reach melee` a pull before the guard. `mimiron frost bomb guard` zeroes `reach
melee`, `reach spell`, `reach party member to heal`, `set behind`, `follow` and `mimiron approach target
action` within `ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE` + `_HOLD_MARGIN` (38 yd) of a live bomb; a healer may be out of
range of a far tank for the 10 s.

**A bomb near VX-001 packs the raid because of the melee, not the room.** Ground at least 32 yd from a
bomb 10.6-15.1 yd off VX-001 spans 97-178° of bearing at the ranged radii, so the far side has room; the
pack is the melee clump carried by the straight-away flee through the boss onto the ranged line, where
Rapid Burst hits 13-17 bots a cast for 125-164k (seven dead in 3.6 s once). In hard-mode phase 2 the
flee screens `IsMimironSpotInSector`: melee refuse the wedge ±90° (60 plus the 30° cone), ranged the
melee sector ±75°, counted as `burst`. A fan that empties sweeps again unscreened before the fallback:
the other group's line costs a burst, the blast kills.

## The dodge ladder and the movement lock

| node | `ACTION_RAID` + | move |
|---|---|---|
| Laser Barrage | 7 | `FORCED` |
| Frost Bomb, Plasma Blast defensive | 6 | `FORCED` / a cast |
| Shock Blast | 5.5 | `FORCED` |
| Rocket Strike | 5 | `FORCED` |
| flames | 4 | `FORCED` |
| fire bot (spray, siren) | 3 | `FORCED` |
| Bomb Bot step-out, slow Bomb Bot | 2 | `COMBAT` / a cast |
| arc spread, phase 1 positioning | 0 | `COMBAT` |
| Proximity Mine | −1 | `COMBAT` |
| approach target | −2 | `COMBAT` |

Ties break by push order and the first action returning true ends the tick
([the selection loop](../../engine/action-selection.md#the-selection-loop)), so with rocket, flames and
bomb all at `+ 4` the flames step ended the tick 143-221 times a pull against 6-13 reaching the bomb.
`ACTION_RAID` (60) already outranks `ACTION_HIGH` (20), so every Mimiron node beats the generic reach
nodes without a multiplier.

**Relevance picks the action, not the move the lock accepts**
([movement lock](../../engine/pitfalls.md#movement-that-silently-no-ops)). Every lethal dodge issues at
`MOVEMENT_FORCED`; a `COMBAT` dodge starting mid-walk was dropped with no trace, and the flames dodge at
`COMBAT` was dead, 1,249 moves refused against 387 issued behind formation legs. Only the healable
avoids, mines and Bomb Bots, stay at `MOVEMENT_COMBAT`, where they cannot stomp a real emergency, and
the formation stays there so a `FORCED` dodge can take a bot off it mid-walk. Equal `FORCED` blocks, and
fire fires ~2,500 times a pull against Shock Blast's five, so the frequent one has to stand down
(pitfalls has the seven Shock Blast deaths it cost). `MimironDodgeFlamesTrigger` therefore stands down
while Shock Blast, a Rocket Strike, the Frost Bomb or a live Water Spray line has the bot, and while the
bot's own spot is inside the barrage's swept band. `ULDUAR_MIMIRON_FLAMES_MAX_HOP` (**12 yd**, about
1.7 s of lock) caps a fire leg for the same reason.

**Two `FORCED` dodges deadlock, so the lower one is made aware of the higher rather than outranking it.**
The barrage dodge returns `false` for a bot already clear, Rocket Strike or Shock Blast takes the tick,
and its leg (~1.4 s for a 10 yd step, 2.6 s for 18 yd, against `MaxWaitForMove` 5000) refuses the barrage
dodge's own move while the bot stands in the beams. `MoveAwayClearOfMines` rejects any bearing whose
destination is inside the swept band **at the moment the leg lands**: the cone turns ~10.6°/s, so a 2.6 s
leg outruns the 15° margin. The mirror: the barrage step refuses a spot within
`ULDUAR_MIMIRON_ROCKET_CLEARANCE` of a marker (`IsMimironSpotRocketSafe`) or inside a Frost Bomb's stand
radius, even as its fire fallback, and so does `stepin`; with no clean radius it takes the plain step.
Fire stays the gap there: a step can land a bot in a node, its flames flee `locked` behind the leg.

**Interrupt only for what kills.** A bot mid-cast cannot be moved and `MoveTo` still reports success
([pitfalls](../../engine/pitfalls.md#a-bot-mid-cast-cannot-be-moved-at-all)); a balance druid died to
Rocket Strike that way. The dodges that kill outright call `botAI->InterruptSpell()` before moving:
Laser Barrage, Rocket Strike, Shock Blast, the flames and the Frost Bomb. Proximity Mine (**9,000**) and
Bomb Bot (**12,000**, 63009) keep the cast: both are healable, and clipping a cast every time a mine
lands costs more than the mine.

### The bearing fan

The hazard flees share one fan, `MoveAwayClearOfMines` / `FleeFan`: eleven bearings
over ±112.5° (two stacked filters can empty the first quadrant), each screened against mines, the
cone, fire, the bomb, the other group's sector, Shock Blast and spray lanes, noted per filter in
`mimiron.flee`. Its traps:

- **A destination must be strictly further from the hazard** than the bot is: past roughly 120° off the
  escape bearing the geometry turns back inward.
- **Solve the hop per bearing.** At bearing θ off straight-away the destination sits
  `sqrt(d² + r² + 2dr·cosθ)` from the hazard, so a radial gap `SAFE - d` reaches the circle at θ=0 and
  nowhere else. `r(θ) = -d·cosθ + sqrt(SAFE² - d²·sin²θ)` (`clearRadius`) lands every bearing on it.
- **"Further than I started" is not an escape.** A bearing leaving the mesh comes back as the bot's
  own feet ([pitfalls](../../engine/pitfalls.md#movement-that-silently-no-ops)): three melee logged `shock ok`, moved
  0.3 yd, and died together to one Shock Blast. A candidate must cover
  `ULDUAR_MIMIRON_FLEE_MIN_PROGRESS_PCT` (0.5) of the hop it asked for, counted as `move`.
- **Count the mover's refusals apart from the hazards'.** The lock is tested once before the fan
  (`locked`), and a mover refusal inside it is `move`; reading either as a hazard refusal hid the dead
  flames dodge.
- **The unscreened fallback was the last way in.** Straight away from a fire node once ran a bot at a
  casting MK II to 9.9 yd and its leg locked the escape out. A fallback landing in a live Shock Blast
  (every flee but the escape), a Frost Bomb, the barrage cone or a spray lane returns false as
  `unsafe`, leaving the bot in fire at ~3.1k a second and the lock free.

## Two dodges fought over the fire

Standing in fire totals only ~204-258 bot-seconds a pull; the damage is all in the tail, 5-14% of
exposures running 5 s or longer at 20-50k each. Bots were not walking into fire, they were stuck in it.

**`avoid aoe` is vetoed.** `AvoidAoeAction` sees the flame nodes, moves for them with 3 yd hops at
relevance 90, reports failure and shares `FleePosition`'s 1 s cooldown with every other flee
([its traps](../../engine/pitfalls.md#things-the-bot-cannot-see)). `mimiron avoid aoe guard` vetoes it
outright while hard mode is on and a construct is in combat, and the flames dodge runs on the Mimiron
fan instead of `FleePosition`. Exposures of 5 s or longer fell from 5% to 1-2%.

**The hop is a ladder, not a fixed step.** Chains grow in 7 yd steps and 50-60 nodes are live mid-fight,
so a 4-5 yd hop lands on the next node, and a fixed long one found no clean bearing four times in five
and cost melee two thirds of their time in range on the walk home.
`ULDUAR_MIMIRON_FLAMES_STEP_LADDER` = 3, 5, 7, 10 yd, each rung screened against the whole field and the
first clean one taken; only the last may fall back. Short rungs are safe because they are screened.

**It dodges only when it pays.** A node ticks ~3.1k against a 22-24k pool, so a healthy bot has seven
ticks of margin while the round trip costs 24 yd of uptime. The gate is `nodes >=
ULDUAR_MIMIRON_FLAMES_DODGE_NODE_OVERRIDE (2) || GetHealthPct() <
ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT (60)`, an **or**: two nodes halve the margin to about four
seconds, too short to notice a health bar and walk 12 yd, and they are the usual reason (481 of 820
dodges in one pull were at 100%). The count is not checkable from a trace, whose fire set caps at 40.
Watch it: if fire per living bot per second in phase 2 climbs back above ~356 the threshold is too low.

**No walk may cross fire.** Arc spread screened its slot, not the walk, so a clean slot behind a burning
band sent bots through it and the dodge threw them out the far side, up to 23 yd. No leg may pass within
5 yd of a node the bot is not already standing in (`IsMimironWalkFireSafe`). Once the fire fills the
room that hold empties the formation, so `GetMimironSlotApproaches` plans for trigger and action alike:

- A slot that is not clear gives way, outside phase 1, to the nearest clear point within
  `ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS_WIDE` (12) that keeps 5.5 yd off every raid member. A bot
  already on clear ground within `_RADIUS` (6) stays put, since a stand-in moves whenever the fire grows.
  The wide ring is for phase 3, where 40+ nodes stay alive and a 6 yd ring round a burning slot burns
  too; the phase 3 wedge orbits 17-27 yd out against a 35 yd cast range, so it can afford 12.
- A walk through fire goes via one waypoint over its midpoint, 20°, 35°, 50° or 65° off the direct
  bearing, whose two legs both miss the fire. Only a candidate with no path hands over to the next.
- Mines likewise: a leg passing within `ULDUAR_MIMIRON_MINE_LEG_CLEARANCE` (2.5 yd: the 1.9 yd poll plus
  slop) of one detours, and the straight walk follows as the last candidate whenever fire leaves it open,
  since a mine ranks last. About half the phase 4 mine hits came on arc spread legs.
- Phase 1 keeps holding: its camp leaves its fire by switching anchors, and a stand-in between 6 yd rows
  is a Napalm pair.

`mimiron.approach` notes each non-direct leg, and `none` when no ring is clear, without which an empty
answer reads exactly like the trigger never firing.

**The dodge works; the generic movers undo it.** They aim at a unit and screen nothing about the floor:
3-4% of arc spread and dodge legs ended inside a node against 30-45% of `reach melee`, `reach spell`,
`follow`, `set behind` and heal reach, and half the legs after a dodge were `follow`, most straight back
in. Two answers, because the hazard has two shapes:

- `mimiron fire hold guard` zeroes the five names the Frost Bomb guard lists (`reach melee`, `reach
  spell`, `reach party member to heal`, `set behind`, `follow`) for `ULDUAR_MIMIRON_FLAMES_HOLD_MS`
  (2500) after this bot's last accepted dodge, and while it stands in a spray line. A **hold**, not a
  veto: fire covers a third of the floor in phase 3, so gating on fire being near would strand the raid.
  It suppresses `follow` for most of a handover, free while nothing is attackable, but bots lag a master
  who walks the raid then. The stamps live outside `MimironFightState`, which `TickMimironObs` resets on
  every scan while nothing is attackable; inside it the hold never reached a handover.
- `mimiron approach target action` (`ACTION_RAID - 2`), because melee stood in fire as much as their own
  target did (29% against 31% of phase 3): the exposure is the add, not the walk.
  `GetMimironTargetApproach` sweeps 12 bearings round the target at the bot's stop distance less
  `ULDUAR_MIMIRON_APPROACH_INSET` (2), starting from the bearing toward the bot; if that one is clear it
  returns false and the generic node does its job. Destinations are screened by `IsMimironSpotStandable`
  and the Rapid Burst cone, noted `mimiron.close`. Bots the formation places are left out: their slot is
  in range of its boss, and closing on another target only had the formation walk them back.

**It has to be a node, not a screen inside `reach melee`**: re-registering `reach melee` for Ulduar
would replace it for every bot in every fight ([registration](../../engine/action-selection.md#registration)).

## Shock Blast

63631 is a **4 s cast**, `TARGET_SRC_CASTER`, 15 yd, 100000 base, every 30 s; measured **82,450-109,125**
against 22-30k pools, reach **15.2 yd**, so `ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST` (18) is right. It lands
1-3 times a pull and one cast killed 11 of 25. The tank is melee, so the trigger fires for him too
(phase 1 drift, below). Both trigger and flee measure centre to centre against the 18; `GetDistance2d`
against the MK II's reach of 8 once fled to 29.5 yd and fired out to 24.5
([range and reach](../../engine/raid-mechanics-lessons.md#range-and-reach)). The 22 yd ranged ring is
outside it permanently.

**The escape holds the tick for the whole window.** `charge` and `intercept` sit at `ACTION_MOVE + 10`
with nothing above them once the dodge yields, and a warrior that finished its flee charged back under
the cast. `MimironShockBlastAction` returns **true** throughout, including when already clear and while
a leg is in flight. Once clear only melee keep holding; ranged and healers go back to work rather than
lose 4 s in every 30.

`mimiron charge guard` is the backstop: inside the five windows a hit actually kills through, it zeroes
every `CastReachTargetSpellAction` (Charge, Intercept, both Feral Charges, the class's only subclasses),
Killing Spree by name (it teleports onto the target, onto VX-001 inside the barrage boundary, and killed
the same rogue in every barrage it fired in, 4 of 4 pulls), and `reach melee` by name
([why by name](../../engine/pitfalls.md#movement-that-silently-no-ops)): melee have no mid-phase slot
to hold them, and the tick fell to `reach melee` at 21, which closed back onto ground the chains were
crawling toward. Mines
and Bomb Bots are deliberately not windows: the mine node ranks below the whole ladder because eating
one is healable, and a veto would contradict that.

**Nothing may walk into the circle.** `IsMimironSpotShockSafe` refuses anything within 18 yd of an MK II
casting 63631, inside `IsMimironSpotSafe` so arc spread holds, and in the fan for every flee, the escape
included: it once excepted the escape, reasoning about where the bot starts while the test is on where
it ends.

**The escape aims past the line.** It hops `ULDUAR_MIMIRON_SHOCK_FLEE_MARGIN` (2) beyond the screen,
since a hop landing exactly on 18 failed the `>= 18` test to float noise or a wall-shortened leg; if
every bearing is refused it sweeps again as `shock fire` with fire allowed (a node is ~3.1k/s against
100k), then `FleeShockToAnchor` aims at the bot's own spread slot, else `ULDUAR_MIMIRON_ROOM_CENTER`,
through `MoveTowardClearOfMines` (`shock anchor`); an anchor inside the circle gives way to the room
centre (a `p4melee` slot is inside it by construction). Before this, 4 of 23 casts in one evening left a
bot standing in the blast.

**A mesh trap at (2688.45, 2580.02).** navprobe rings at 8.5 and 12 yd read 14/16 with holes at **112°
and 135°**; with the MK II at -49°, straight-away is 131°, dead centre. Per-bearing, +45° gives
(2678.51, 2580.69) (poly 0.23, `PATHFIND_NORMAL`, 13.48 yd, ~2 s of the 4 s cast), and ±67.5° also clear.

## Laser Barrage is a 104° cone, not a beam

The single most load-bearing number on this boss, and it contradicts every public guide.

**63297 and 64042 deal no damage.** They are `SPELL_EFFECT_DUMMY`, placing the two beam *visuals* via
`TARGET_DEST_CASTER_FRONT` (60 yd) plus `TARGET_DEST_DEST_LEFT` 4 yd / `TARGET_DEST_DEST_RIGHT` 6 yd;
reading those as a beam width produced the old dodge. The damage is **63293**:
`SPELL_EFFECT_SCHOOL_DAMAGE`, `TARGET_UNIT_CONE_ENEMY_104`, radius index 28 = 50000 yd, so **distance
from VX-001 buys nothing**, only bearing. `Spell.cpp` maps that target type to a **104° cone** (±52°
through `HasInArc`, which compares `arc/2`); guides describe retail's 30° visual. **20000 damage every
250 ms.**

**Check `acore_world.spell_cone` before trusting a target type here.** `Spell::SelectImplicitConeTargets`
reads `sSpellMgr->GetSpellCone(id)` **first** and falls back to the target type only without a row.
Rapid Burst and Hand Pulse rows are **60**, confirmed by measurement. The 63293 row says 10, which traces
do **not** bear out, and cannot settle either: the beam bearing leads VX-001's visible facing by up to
the 42.6° that 33576 travels during Spinning Up, so every measured hit angle is against the wrong
reference. `ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE` stays 52° until something measures the beam.

Aim comes from `FaceBarrageArc`: VX-001 is repointed at NPC 33576 every tick of the aura, and 33576 laps
the room on a fixed spline every 34016 ms (orbit below). Cadence is 60 s. **NPC 33576 is spawned by world DB update
`2026_08_10_00.sql`; without it `FaceBarrageArc` returns early and the cone never moves.** The dodge's
fallback then reports `sweep = 0`, a static ±clearance wedge round VX-001's facing: running a sweep that
is not happening would walk the raid through the beams.

**Spinning Up is a channel on VX-001, and no aura at all.** 63414's effect 0 lands on 33576 and effect 1
on the MK II, both by `conditions` row; effect 2 has no effect. On VX-001 it exists only as a **4 s
channel** (`SPELL_ATTR1_IS_CHANNELED`, duration index 35), which is why the script polls
`FindCurrentSpellBySpellId`. `HasAura(SPELL_SPINNING_UP)` never answers true; keyed on it, the raid's
decision landed 0.1 s after the beams went live and one kill lost 12 bots. Test the channel, and read
`GetCastTimeRemaining` clamped at zero: it goes negative on the pass that ends the channel.

**Aim where the cone will ignite, not where the boss points.** Since core `8f68451bb` the boss re-faces
every 400 ms through the channel, but 33576 travels **42.6°** during the windup, which is fatal at 20000
a tick. `GetMimironBarrageWindow` reads the channel timer while spinning up and the barrage aura's
duration once firing, then rotates 33576's live position clockwise about the room centre to the bearing
at ignition and at the last tick. Recomputed each tick from world state, so a moving apex, a rotating
chassis and a bot joining mid-cast fall out for free and every bot derives the same cone.

**The orbit.** Path 13395 is 19 waypoints at velocity **20.8988**, perimeter **707.0 yd**: a **33.8 s**
lap, **10.64°/s**, **106.4°** per 10 s barrage, clockwise (bearings strictly decreasing). A least-squares
circle fits it at (2741.98, 2569.36), r = 113.2, **2.7 yd** from `ULDUAR_MIMIRON_ROOM_CENTER`, close
enough to orbit about the room centre within a couple of degrees a cast. The radius is not constant
(110-116 yd), so `MimironOrbitAhead` reads it live off 33576. That rate holds only for a VX-001 at the
centre: from 30 yd off it swings **8.3-14.7°/s**, off by up to 40° a barrage, so predict 33576's position
rather than budget for the error.

**Never fold the band to (−π, π].** Sweep plus a clearance at each end is 106° + 2 × (52° + 15°) ≈
**240°** at the room centre, wider off it, so barely a third of the room is safe. A signed fold puts part
of it past π, which is how the far side of the room reported safe while the beams crossed it. The dodge
measures clockwise from the ignition centreline in `[0, 2π)`, where the band is one interval.

### The dodge

It is **selective** (bots outside the swept union never move and keep casting); ranged rotate at
**constant radius** unless the step is on fire, since radius is irrelevant to safety; melee orbit a fixed
9.0 yd ring; and direction is chosen on **time spent inside the cone**, not distance, since the short way
round is often straight through 104° of beam:

| Where the bot is | Direction | Why |
|---|---|---|
| More than 52° clockwise of the centreline | **clockwise** | The beams have not reached it; running clockwise keeps it that way, turning back walks into the cone. |
| Inside the cone, leading side | whichever edge is **sooner**, except the Spinning Up exit below | Counter-clockwise rides the sweep out at `turnRate + rate`; clockwise pushes against it at `turnRate − rate`. Crossover near 30° off the centreline. |
| Counter-clockwise of the centreline | **counter-clockwise** | The sweep is already carrying it clear. |

**During Spinning Up the sweep helps neither way**: the band is fixed in world space and both edges close
at plain `turnRate`. And the near edge is not where the walk ends: clockwise out of the ignition cone
commits a bot to outrunning the sweep round the whole band (150-200°), counter-clockwise is `cw +
clearance` (67-97°) onto ground the beams are leaving. In one barrage every bot that left
counter-clockwise lived and 10 of 11 fire deaths had gone clockwise. So an `inside` bot exits
counter-clockwise whenever `(cw + clearance) / turnRate` fits in `untilLive` less
`ULDUAR_MIMIRON_BARRAGE_IGNITION_BUFFER` (0.5 s, two ticks), else by the near edge. `ahead` bots keep
clockwise: they would cross the whole ignition cone, and a delayed leg is inside it when it lights.

A simulation over the real waypoint path (every DB Target phase, bot bearing, ranged radius 14-24 yd and
chassis offset to 30 yd in eight directions, committed legs and the movement lock modelled) clears
**62,208 of 62,208** positions. **The step size is not a safety dial**: 40° down to 10° all give zero
hits, and smaller is worse under a moving apex. Every failure the harness can produce needs the apex
dragged around under the raid (2 yd/s of drift catches 0.53%, and a 30° margin only takes that to
0.34%), so it cannot be tuned away.

**The melee ring holds the apex still, which makes it load-bearing for the whole raid.**
`Unit::GetMeleeRange` is 8 + 1.5 + 1.33 = **10.83 yd** and `ChaseMovementGenerator` leaves the chassis
alone inside it. Every melee bot, the phase 4 main tank included, orbits at reach +
`_MELEE_RING_MARGIN` (1.0) = **9.0 yd**, inside `reach melee`'s **10.25 yd**
(`IsWithinCombatRange(target, MeleeDistance 0.75)`), so it keeps swinging; nothing chases, and VX-001
drifted **0.0-0.4 yd** over eleven barrage windows. The old 14 yd floor left melee outside reach with
`mimiron charge guard` vetoing the chase, idle for the window: 0.2-0.5M a phase 2, the same in phase 4.
A tighter ring is faster too (44°/s at 9 yd against 28 at 14). A melee bot the band never reached but a
hop left wide steps radially back in **on the bearing it holds** (same bearing, same safety) and holds
the tick for that step only.

**The margin is 15°**: the cone turns 2.7° per tick, so 12° was one reaction interval. 15° still leaves a
120° safe wedge for 25 at 22 yd, and the action rechecks every 100 ms while relocating. The **24 yd cap**
on the arc spread keeps the worst-case 52° rotation inside the 4 s window with 1.6× speed margin.

**Rotate in bounded steps above a floor radius.** Creatures are absent from the navmesh, so `MoveTo`
draws a chord through VX-001, and a chord across the apex crosses every bearing the cone covers. One leg
is at most 40° (within 6% of the ring), and a ranged radius stays in `[reach + 6, 24]`, VX-001's reach
being **8**, so 14-24. `MOVEMENT_FORCED` sequences the legs.

**The boundary is a place a bot can stop.** A cone hits anything inside it at any bearing
([pitfalls](../../engine/pitfalls.md#arc-and-geometry-defaults)), so a bot parked **1.72 yd** from
VX-001's centre at 180° off the cone died in two ticks. `IsMimironSpotBarrageSafe` refuses `ULDUAR_MIMIRON_BARRAGE_BOUNDARY` (3.0) at any
bearing, and a clear bot inside it steps straight out on its own bearing to its ring (`boundary`),
unscreened: the beams are certain, a marker on the ring is not.

**Other movers stand down for the window.** `mimiron approach target action` (its destinations are never
cone-screened, and the two traded one bot 78 times in a phase 4); `combat formation move` for everyone;
`set behind` and `tank face` for melee (`mimiron vx001 facing guard`): the dodge hands the tick back
while a bot holds at the band edge, and `mimiron formation guard` stops the unstacker only on a slot,
which normal-mode phase 4 melee lack. The unstacker once walked five holding melee into one band, all
dead in 6 s.

**Radius is the free parameter, so it dodges the fire, walk and endpoint.** A step takes the nearest
radius within ±8 whose destination **and** straight leg are clear, falling back to a clean endpoint and
then to the plain step, because the cone kills in a tick. The search stays in `[14, 24]` for ranged and
`[5, 9]` for melee, who give ground inward only.

**The fire dodge keeps running during a barrage for every bot the cone does not own.** The window is
22-26% of phase 2 and carried 36-76% of its fire damage; one window killed 13 bots to Flames while bots
told `clear` or `hold` stood in nodes for 8-14 s. The fire trigger defers only for a bot inside the band,
where the barrage dodge screens every hop.

**A burning band edge is not a place to stop.** When the re-aimed wedge held 22-27 of 38 nodes, 8-11
bots died to fire in the next 15 s, against none at 11 of 34 or fewer: `ahead` bots `hold` within a yard
of the band edge, the fire dodge's fan refuses every bearing (band behind, fire ahead, hops capped at
12 yd), and bots near 3-5 nodes take 12-15k/s, dead from full in under 2 s. Two rules probe the ring in
5° steps (`_REFUGE_PROBE`) over the same radius shifts, clean meaning clear of fire, the bomb and
rockets. A walk ending on a burning edge carries on into the safe sector to the first clean bearing (the
whole sector stays clear for the rest of the cast: the far edge is fixed and the trailing side only
opens), and a clear bot standing in fire walks the ring to the nearest clean bearing (`refuge`), which
outranks the fire dodge; that gets the tick only when nothing is clean. Still open: phase 2 melee pinned
between beams and fire die with every flee bearing refused (`cone9 fire2`); in those windows fire took
74-292k against 18-20k in clean pulls.

## Raid nodes resolve bosses by entry, not by threat

`"find target"` walks only the bot's own threat list
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)), and VX-001's phase 4 `SetData` calls
neither `DoZoneInCombat()` nor `AttackStart()` (its `AttackStart` is a no-op override), so a bot that
never damaged it, a healer or a melee on the chassis, got `nullptr` and **the Laser Barrage dodge never
ran for it**. Every Mimiron node resolves through `GetUldUnitByEntry`.

**Targets are set directly, never through a raid icon.** `mimiron set dps priority` owns every non-tank's
`"current target"` while a mech is up; the old icon round-trip failed whenever a bot's `rti` string
differed from the marker's, since icons are group-global. `MimironAerialCommandUnitAction` still puts the
skull on the Assault Bot, else the ACU, for a human raid leader only: no bot reads it.

## The two adds need opposite answers

**Proximity Mine (34362)** carries `unit_flags = 2` (`UNIT_FLAG_NON_ATTACKABLE`), so nothing removes one.
It arms 2.5 s after landing, polls every 500 ms for a player inside **1.9 yd**, blasts 3 yd (66351), and
self-detonates at 35 s. Ten land inside 15 yd of the MK II (65347, radius idx 18) 8 s after every Shock
Blast. Non-selectable, so never in `"possible targets"` and invisible to pathing; `IsMimironSpotMineSafe`
gates the destination of every Mimiron move **except the barrage dodge**, where the cone kills and a mine
does not.

**Avoid tightly, and rank it last.** Ten mines in 15 yd leave no clear ground at a wide avoid radius, so
the node stays active and the raid paces. Trigger at 3.0 yd, clear to 3.5, step at most 5, and take the
*nearest* clear spot: `MoveAwayFromCreatureAction` maximises min-distance over a 30 yd fan and picks a
different answer every tick. `ACTION_RAID - 1`, below every other Mimiron node: Shock Blast is 100000 in
15 yd and a mine a healable 3 yd, so a mine must never cost a dodge, a taunt or a core delivery. Some
mine hits are the intended price.

**Melee do not dodge mines, and nobody does during a barrage.** Mines land where melee have to stand, so
the node fired continuously; 10 mines of 3 yd in a 707 yd² circle is ~40% coverage, ~0.4 hits a cycle,
**roughly 120 dps**, against all of melee uptime and, in phase 4, barrage deaths from being nudged onto a
cone-blind bearing. Ranged keep the sidestep; every healer spec is `IsRanged` (`PlayerbotAI::IsRanged`
reads `STRATEGY_TYPE_RANGED`), so "melee" means melee DPS plus warrior, DK, protection paladin and feral
tanks. **Between phases melee dodge too**: with nothing attackable (`GetMimironRingFocus` null) there is
no uptime to lose, and mines outlive the MK II (two melee died `follow`ing the master across them). Open:
mines still did 251-418k of phase-1 damage, ~250-390 dps a melee against the ~120 priced above, with
`set behind` and `reach melee` putting melee on them.

**Bomb Bot (33836)**: `speed_run` 1.14286 is **8.0 yd/s against a player's 7.0**, so it gains on you, and
it detonates on melee contact (`SMART_EVENT_DAMAGED_TARGET` → 63801, base 23562, 5 yd; **66406 hp** in
25-man, `HealthModifier` 1.5873). Ranged kill one in a few globals; it tops the ranged priority list.

**The chase target decides who shoots it.** The bot it chases cannot leave, so keeps shooting at any
range; everyone else inside the blast steps out, and healers and melee always sidestep. Both rules read
`GetMimironBombBotChasing`; split on distance instead, each deferred to the other and a ranged bot inside
8 yd neither shot nor moved. Bomb Bots out of `SpellDistance` stay out of `BuildPriorityList`: chasing one
abandons the mech and lands in the `reach spell` against `ACTION_RAID` deadlock.

**It is the one Mimiron add that takes a snare.** Its immunity set **−263** leaves `SNARE`, `ROOT`, `STUN`,
`FREEZE`, `GRIP` and `KNOCKOUT` off, all of which the Assault Bot's **−285** carries; at 8.0 yd/s a
second of extra approach is most of a cast. `mimiron slow bomb
bot action` puts the class snare on the Bomb Bot the bot is already shooting: hunter `concussive shot`,
shaman `frost shock`, warlock `curse of exhaustion`. Roots are deliberately absent: Entangling Roots and
Frost Nova break on the first hit ([CC breaks two ways](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
It reads `"current target"`, so it fires only on a Bomb Bot `mimiron set dps priority` handed the bot
(healers never snare), and only while the Bomb Bot is more than **15 yd from its own victim**
(`ServerFacade::GetChaseTarget`, else the caster's distance before it picks anyone): from the caster, a
hunter 25 yd off would snare one two yards from a healer.

A main-tank `unit->Kill()` on both adds, gated on `BotCheatMask::raid`, was removed.

## Rapid Burst and Hand Pulse are 60° cones

`EVENT_SPELL_RAPID_BURST` picks a random player within 80 yd every **3.2 s**, casts **63382 on that
player**, then `SetFacingToObject`. 63382 is a 3000 ms aura with a 500 ms periodic dummy, **six ticks**,
each firing 64531/64532 from VX-001 along the facing set when the aura landed. VX-001 does not move, so
the centreline is `VX-001 → the aura carrier`, fixed for 3 s: **read the carrier, not the boss's
orientation**. Phase 2 only: `_events.RescheduleEvent((_phase == 2 ? EVENT_SPELL_RAPID_BURST :
EVENT_HAND_PULSE), 14.5s)`.

**60° is measured.** Bucketing living bots past 14 yd (beyond `IsWithinBoundaryRadius`) by bearing off
VX-001 the tick before each hit, the hit rate holds at 74-83% to 30° and falls to **9%** at 30-40°, ~1%
beyond; no aim lead confounds it, since the boss faces its target once and holds.

**So there is no step.** A step dodge, measured and removed, saved ~700 damage a cast (2.32 ticks against
2.4-2.75 standing) while its `rapidburst locked` legs held the lock against the lethal dodges and its walk
back was the largest piece of phase 2 formation walking. What stays is the screen: the fan, slot
approaches, target approach and the arc spread's slot test refuse a destination inside the live cone
(`IsMimironSpotRapidBurstSafe`), and a bot in it waits the 3 s out.

**Hand Pulse (64348/64352, 64536/64537 on 25-man) is the same 60° cone**, an 850 ms cast every 1.75 s in
phase 4 at a uniformly random player within 80 yd (`SelectTargetFromPlayerList`), so only the layout
helps; it took 2.64M and seven of fourteen phase 4 deaths in one pull. Scored on traced positions with a
random target, the ranged/healer wedge plus melee wherever `set behind` put them gave 5.56 bots a cast;
melee opposite the wedge 4.45 (−19%); ranged over 240° as well 3.31. Widening the wedge would put more
of the raid in every barrage's ~240° band, so only the melee move (`p4melee`, below); it then measured
28-33% of the living raid a cast, the floor this layout gives.

**The ring survives the correction.** Six fixed phase-2 spots stacked the raid into three clumps, the
worst shape against any cone; a radius-22 ring with one index-derived slot per ranged bot is not. The
bearing is never keyed off VX-001's facing.

**Under Firefighter the melee stand opposite the wedge.** A bot in the other group's line takes every
burst aimed at that group: 21-39% of phase 2 Rapid Burst damage crossed between melee and ranged, once
643k against a ~400k healing deficit. VX-001 never swings (`AttackStart` empty, no
`DoMeleeAttackIfReady`) and turns to each burst carrier and the barrage arc, so "behind" moved every
3.2 s. `p2melee` gives melee and tanks slots on the barrage melee ring (9.0) over
`ULDUAR_MIMIRON_PHASE2_MELEE_HALF_ANGLE` (±45°) opposite the wedge centreline, following its barrage
re-aim. The 5 yd slot tolerance is ~32° there, and 77° off stays past the 90° where wedge plus cone ends.
Mid-barrage the slot is in the swept band, so the arc spread waits and the barrage dodge owns the melee.
A melee substitute stays 5.0-10.0 yd from VX-001 and off the wedge's line, and skips the Napalm spacing
test. `mimiron vx001 facing guard` zeroes `set behind` and `tank face` there. Hard-mode phase 4 hands out
the same sector as `p4melee`, round VX-001 on the chassis, without the main tank, who holds the MK II;
melee sent to the ACU (reach 5, so 7.83 yd) close the last yards inside the slot tolerance.

**The ring centres on the mech, not the room.** Bots cast out to `Playerbots.SpellDistance`, 28.5 here
(no `AC_` override), so a ring pinned to the room centre puts the far half out of range after about six
yards of boss drift, and the `reach spell` against `ACTION_RAID` deadlock keeps them there: ranged simply
stop attacking. **Recovery is a rigid translation, never a per-slot clamp**: when the outermost slot
falls out of casting range the anchor slides toward the mech by the excess, since per-slot clamping
deforms the ring into the clump the spread exists to prevent. Every input (anchor, focus, radius,
count) is group-global and the config distance is read directly rather than through
`PlayerbotAI::GetRange`, so each bot derives the same anchor. A second clamp keeps the anchor within
40 yd of the room centre: navprobe has the floor at 16/16 headings to 40, flat at Z 364.31.

**Phase 1 drift is fixed at the source.** The Shock Blast trigger fires for the tank too, the MK II
follows, and over five minutes that walks the fight round the room. The main tank's phase 1 slot (normal
mode) is `ULDUAR_MIMIRON_ROOM_CENTER`, the point `boss_mimiron.cpp` charges the MK II to, so he drags it
home after every cast. His slot is exempt from `IsMimironSpotSafe` (`IsMimironTankAnchorSlot`): the MK II
parks on the mine field it just laid, and a tank refusing to stand in one never returns.

## Phase 2 is a healing race, and Heat Wave is why

VX-001 casts **64533 on itself every 10 s** in phase 2: implicit targets 22/15, radius index 28 =
**50000 yd**. Raid-wide, unavoidable, no bot-side answer, and the largest single source of phase 2
(23-24% of intake, up to half once fire was contained). Do not re-investigate it.

The arithmetic it forces: phase 2 takes 20-36k/s, about **1,600 damage per living bot per second**,
against 15-33k/s of effective healing, a 617-679k deficit across a long phase, with overheal 14-34%
against phase 1's 68-72%, so there is no slack. Nothing one-shots; the raid runs out of health. Four healers top out at 26-31k/s standing
against 28-34k/s of intake, a ceiling the roster sets, and storm deaths carry 0-2.3k overkill where 20% off their last 6 s
was 3.1-5.9k. Fire and
Rapid Burst are the only avoidable slices, and every decision on this boss is drawn against that budget.

**A walking healer heals at a third of its rate**: 7.4-7.8k effective HPS standing against 2.6-3.5k
walking, so a window with half of them walking is a ~15k/s hole. Healers spent 38-39% of phase 2 moving:
formation walk-back after dodges, then the bomb, rockets and fire.

**The 15 s after each barrage is a schedule, not luck.** VX-001 banks its events while casting
([reading a boss script](../../engine/raid-mechanics-lessons.md#reading-a-boss-script)), so during
the 14.5 s of Spinning Up plus barrage only the fire lands, then every overdue event fires one per tick. Heat Wave every 10 s, Rocket Strike 16 then 20, Frost Bomb 1 then 45,
Spinning Up 30 then 60: both phase 2 barrages are followed **within 0.5-4 s** by all three. Those 15 s
take ~600-690k against 430-490k healed (raid health ~30 points down) with 30-59% of healers moving against
22-36% the rest of the phase, mostly the formation walking back to the slots the dodge took them off.
So **the formation walks during the barrage**: `MimironArcSpreadTrigger` acts while the beams are up
whenever the bot's spot **and** its slot are outside the swept band (two endpoints in one sub-180°
sector put the leg inside it too), and `GetMimironSlotApproaches` screens every candidate against the
cone with its own travel time. The leg stays at `MOVEMENT_COMBAT` so a `FORCED` dodge can take the bot
off it mid-walk: that ordering is what makes it safe, and why it must not be raised.

**So the raid cooldowns wait for the storm.** Divine Sacrifice (Divine Guardian, 70940: 20% off the raid
for 6 s; both paladins grant it), Divine Hymn and Power Infusion share "medium group heal setting" (6 at
65% or less within 38.5 yd), which the walk-in Rapid Burst meets at once: they went out 2.7-20 s into
phase 2 at 67-85% raid health and sat out the storm on 2-8 min cooldowns. `mimiron storm cooldown hold`
zeroes the three in phase 2 (`IsMimironPhase2`, both modes) until `"aoe heal" "low"` reaches
`ULDUAR_MIMIRON_STORM_COOLDOWN_LOW_COUNT` (6 at 45% or less), a count that peaks at 3-4 before barrage 1
and 11-17 after; the storm after barrage 1 then killed nobody. Holding the cast holds its `cancel divine
sacrifice` continuer too. Aura Mastery stays free, since the holy paladin runs Concentration.
Tranquility never fires: its strategy left the default resto set in `278be80c9`, and in 3.3 it heals
only the druid's party.

**Heroism goes in phase 4, 15 s in.** In the first kill the human held it into phase 4: 160-166k/s for
the next 20 s against 75-128k before, while phase 2 without it held. Berserk at 10:00 against a
10-minute cooldown allows one window. It opens with all three parts selectable and VX-001 riding, after
`ULDUAR_MIMIRON_PHASE4_LUST_DELAY_MS` (15 s): the opening Frost Bomb has the raid running, and haste does
nothing for a moving bot. Each bot's multiplier stamps when it first saw that, reset when it fails.

**Two gates had to move for any Mimiron lust window.** `HoldBurstUntilTankEngagedMultiplier` waits for a
tank to hold the boss 3 s, and VX-001 never takes a victim (`AttackStart` overridden to nothing, no
`UpdateVictim`; snapshot victim empty 100% of phase 2), so `BossTakesNoVictim` exempts it
([burst gate](../../systems/consumables-and-burst.md)); multipliers only suppress, so no raid node could
undo it. And
`UlduarBurstWindowMultiplier` keys on selectability: `possible targets no los` lags and drops
unselectable units, so with no mech resolved the permissive fall-through leaked lust in phase 1, and a
plain `FindNearestCreature` finds the MK II, which the core never kills (phase 1 ends with its damage
zeroed, `UNIT_FLAG_NOT_SELECTABLE` set and the chassis parked at (2795, 2598) at 0.1% through phases 2
and 3). All three templates start unselectable, VX-001 clears it in `SetData(1, 2)`, and Self Repair sets
it again, so the handovers stay shut.

**A clean arena buys the opening of phase 2, not the phase.** Mimiron seeds every 30 s throughout, so
53-85% of phase-2 flame damage came from seeds landed in phase 2 itself. The west camp buys a raid
arriving on empty ground with somewhere to dodge to.

**Mana is a tail problem, not the wipe**, and not worth re-investigating: healers died at 40-60% mana,
entering phase 2 at 45-65% because the drink guard vetoed handover drinks. The guard stays (handovers,
below).

**Open: the raid meets phase 2 as one clump.** All 25 sit inside a 60° arc for the 30 s handover, melee
and ranged 0-5° apart, and `EVENT_SPELL_RAPID_BURST` is scheduled at **0 ms**, so the first cast lands at
P2 −0.2 s and the first two to four cones hit everybody: 365-568k in the first 10 s against a phase rate
of 13k/s. The shape forms only once `IsMimironPhase2` turns true, which needs VX-001 off the chassis, the
instant Rapid Burst starts.

## Phase 3 wants a wedge, not a ring

The add summon pads (GO 194740-194748) sit on **three arms** leaving the room centre at 180°, +59.4° and
-59.4°, with pads at roughly 17, 29 and 40 yd, so a ring parks lone ranged in an add's path. The raid
stages in the **east wedge**, the one direction nothing walks down, centred on the bearing to
`ULDUAR_MIMIRON_PHASE3_STAGE` (2762.65, 2569.46, 364.31; navprobe 16/16 on mesh in a 12 yd fan). Ranged
and healers fill outward from 18 yd.

**Melee and tanks get no slot.** Inner rows at 8 and 14 yd deadlocked like the ranged ring one band down:
`mimiron set dps priority` hands melee an add walking in from a pad, `reach melee` chases at
`ACTION_HIGH` and the formation drags it back at `ACTION_RAID`, so no swing lands. Melee stand on
whatever they are hitting. No phase 3 `disperse distance` either: the slots do the spacing, the unstacker
shoving bots off slots the formation pulls them back onto is pure thrash, it would be one more node
fighting the melee chase, and Bomb Bots, the only phase 3 mechanic punishing stacking, have their own node.

**The wedge is built to fit casting range**, not grown until it stops fitting: rows step 6 yd (one more
than a Bomb Bot blast) and stop at `SpellDistance` minus the margin, and once full the remaining rows pack
tighter. Spacing is what to give up, not range: a Bomb Bot catching two bots is cheaper than half the
raid unable to cast. At ±60° a 17-strong ranged group still holds 4.7 yd.

**It is anchored on the room centre and never slides onto the unit.** A slide measuring `extent` in every
direction while the wedge occupies 120° of one landed the anchor half a yard from the boss, unclamped;
the unit uses `AttackStartCaster(who, 30.0f)`, so it chases its threat target, a wedge ranged bot, to
30 yd, and raid and boss circled the room together.

**It does shift, rigidly and only for range.** The unit hovers ~16 yd up and follows its tank, so a fixed
wedge left south slots 35-45 yd 3D from it (100 `arc spread <-> reach spell` a phase).
`MimironShiftIntoRange`, shared with the phase 1 camp, moves the whole wedge toward the unit's ground
point until its outermost slot is inside `spellDistance + unit reach + 1.5 − margin` in 3D, ~26.7 yd of
floor under a hover. Recomputed every scan it slid every slot a yard per yard of drift (961 yd of
formation walking in one phase), so `MimironStickyShiftIntoRange` keeps it raid-wide while every slot is
in reach and the fresh one is within `ULDUAR_MIMIRON_SHIFT_SLACK` (4, under the 5 yd tolerance), and
recomputes it `SLACK` inside the limit. The phase 1 camp keeps the plain shift: that inset would pull
its middle row inside Napalm's 24.5 yd floor.

**Holding still is also the Bomb Bot fix.** They spawn on the unit (`SPELL_SUMMON_BOMB_BOT` is cast on
self), so the unit's own 30 yd standoff is what a Bomb Bot crosses: **~3.7 s** of free fire on a 20,000 HP
add at 8.0 yd/s. Pushing `ULDUAR_MIMIRON_PHASE3_MIN_RADIUS` past 18 buys nothing; the unit keeps 30 yd
from its victim wherever it stands.

**The unit does attack.** Its events are only the four summons, but `UpdateAI` ends in
`DoSpellAttackIfReady(SPELL_PLASMA_BALL_P1)` on its victim every swing: 462k into the tank in one phase 3.

**Under Firefighter every live phase wants the wedge**, because of fire. A 360° ring puts 25 bots on 25
bearings and drags 25 chains outward until the fire is everywhere; grouped into one sector the chains
converge, the 4 yd freeze bites, and the Frost Bomb, which only summons on a burning node, lands there and
clears it. So in hard mode the `ring` branch becomes `hmwedge`, reusing `MimironWedgeRows` and
`MimironWedgeSlot` at the same 18 yd first row, 6 yd spacing and east centreline, still anchored on the
mech. It costs nothing in Rapid Burst: 20-30% of the living raid per tick against 22-25% under the ring.
Normalise anything per-tick by the living count: a raw "1 victim rising to 3" was a raid the bomb had cut
to 7-10. `ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE` is the knob if the fire still fans out. Phase 2 and 4
melee get `p2melee`/`p4melee` opposite it.

**`hmwedge` gives way to the Laser Barrage**, the one thing that outranks fire convergence: a 120° wedge
overlapping the swept band cannot be walked out of once the beams ignite. The safe sector's bisector is
`lead + π − sweep/2`, and it measured 117-126° wide, the wedge's own width, so the fit is exact and the
aim has to be. `GetMimironWedgeCentreline` re-aims there at most once per barrage
(`ULDUAR_MIMIRON_WEDGE_REAIM_MS`, 20 s), latched raid-wide, and only while VX-001 is **still spinning
up**: once lit, `lead` and `sweep` describe what is left, so the bisector moves every tick and would walk
25 bots round the room under the beams. It moves nothing when the centreline and both edges are clear.
`p3wedge` keeps the fixed east bearing: phase 3 has add pads on the other arms and no barrage. Everything
reading a slot follows the wedge: `mimiron approach target action` skips bots the formation places,
`combat formation move` is muted within tolerance, and `FleeShockToAnchor` runs to it.

### Disperse distance

`rangedDepth` is `SpellDistance(28.5) − margin(4) − 18 = 6.5`, so `MimironWedgeRows` gets two rows: 14
ranged in rows of seven, 6.0 yd apart radially and 6.3 along the inner one. `MimironPhase1PositioningAction`
once set `disperse distance` to exactly 6.0, which judged every bot on its slot too close: `CombatFormationMoveAction` shoved it 5 yd off and the
arc spread walked it back, unseen in the verdict stream
([pitfalls](../../engine/pitfalls.md#things-the-bot-cannot-see)). Ranged lost 13-17% of output.

- `ULDUAR_MIMIRON_DISPERSE_DISTANCE` is **5.5**, between Napalm Shell's 5 yd splash and the wedge's 6.0.
- `mimiron formation guard` zeroes `combat formation move` while the bot is within
  `ULDUAR_MIMIRON_SPREAD_TOLERANCE` of its slot, the window the arc spread declines to act in. Match
  **by name**: `TankFaceAction` derives from `CombatFormationMoveAction` and does real work. A bot with no
  slot keeps the unstacker, which is every mid-phase melee, and melee never had a disperse distance here
  anyway: the phase 1 node is ranged-only and `DisperseDistanceValue` defaults to -1. Phase 4 sets its
  own: `MimironPhase4FocusAction` puts `disperse distance` at **4.0** on its non-tank tail.
- **The invariant:** `PHASE3_SPACING` > `DISPERSE_DISTANCE` > Napalm's 5 yd, and
  `MimironPhase1PositioningTrigger` compares `ULDUAR_MIMIRON_DISPERSE_DISTANCE`, never a literal: a stale
  `!= 6.0f` left the latch open and ranged cast nothing for a whole phase 1
  ([latch trigger](../../engine/action-selection.md#the-selection-loop)).

**Mimiron is the module's only writer of `disperse distance`, and owes every later encounter a reset**
(`MimironResetEncounterStateTrigger` / `Action`;
[state does not cross objects](../../engine/pitfalls.md#state-does-not-cross-objects)). The same leak
carried `mimiron.slot` and `ironassembly.*` notes into a Thorim trace, never followed up.

## Phase 3 is the DPS check

The 25-man hard-mode berserk is 10 min from the pull (`EVENT_BERSERK`, then Self-Destruction). Less
13.3 s before phase 1 and 103.5 s of fixed handovers, ~483 s is left for the MK II and VX-001 (8,276,398
each), the ACU (5,517,599) and phase 4 at half health (~11M): about **33M, 69k sustained**. At 69k phase 3
is ~80 s and phase 4 gets ~180 s. Phase 3 is where pulls are lost: it ran 129-348 s on pulls that reached
it and lost, against 105 and 145 s on the two kills (9:18, 9:52.5).

The time goes to the adds, not the handovers (the raid is back on the new mech 3-5 s after it becomes
attackable): an Assault Bot is up ~71% of the phase and outranks the Junk Bot for every role, so only 54%
of one pull's phase 3 output reached the ACU. The airborne unit loses ~0.11%/s against 0.60%/s grounded.
The core section is the fix for the unit; the pile is the fix for the adds.

**Melee clear the Junk Bot pile; the rest keep the core source.** Seventeen Junk Bots arrived in one phase
(one per 10 s, 251k each), lived 63 s median, and did 627k into the raid, more than the fire or the unit.
Above `ULDUAR_MIMIRON_JUNK_BOT_PILE` (4) alive, melee take a Junk Bot ahead of the Assault Bot, and finish
the one they are on when the count falls back. It costs them nothing: the nearest is ~3.4 yd away with 4
inside a cleave, while the Assault Bot stands ~7.7 yd off the pile, out of melee AoE. Ranged and healers
keep the Assault Bot, the only Magnetic Core source; a grounded unit outranks both for melee and ranged
DPS.

Open: **the MK II's opening swing** goes to a random raid member with the tank 24-34 yd off, and once
one-shot a healer; **the ACU has no tank in phase 4**, so it sat on ranged 82% of one phase 4 for 407k
and three killing blows.

## The Magnetic Core needs a carrier that walks

`GetMimironCoreCarrier` is the first alive non-tank melee bot in group order (still group order, so every
bot agrees); `IsMechanicTrackerBot`, the first alive bot regardless of role, was usually a ranged bot 22 yd
out, never in loot range. The corpse search covers the room (`ULDUAR_MIMIRON_CORE_SEARCH_RANGE` 90: 80 yd
across, ~13 s), and the action walks to the corpse before looting. Corpses last 25 s
(`TEMPSUMMON_CORPSE_TIMED_DESPAWN`), the entire window.

**The core buys the only killable window in the phase.** The placed core (34068) casts 64436 on itself
3 s after the use, reaching the unit within 12 yd, and despawns at 25 s. The aura runs **20 s**, carries
`MOD_DAMAGE_PERCENT_TAKEN` at base 49 (**+50% damage taken**), and its `OnApply` runs `DO_DISABLE_AERIAL`:
`CastStop`, `AttackStop`, `REACT_PASSIVE`, hover cleared, `MoveFall`, and `_events.DelayEvents(25s)`.
`UpdateAI` returns before the event map for the whole aura, so add timers freeze and then slip 25 s more:
**the next add comes ~45 s after the core**. Removal runs `DO_ENABLE_AERIAL`, a climb of +16 and a 2 s
lambda setting hover again. The unit carries neither `NOT_SELECTABLE` nor `NON_ATTACKABLE`. One core holds
it on the floor 18.4-19.0 s of the 20.

**`TakeMimironCore` hands the core over because bots cannot loot it in combat.** Looting is wired only
into `LootNonCombatStrategy`, and the loot strategies discard 46029, a white consumable, as junk. The
Assault Bot drops it at 100%, so the handover asks only what a player would: kill the bot, stand on the
corpse, have bag room. It takes the core off the corpse's own loot, marked looted as
`Player::StoreLootItem` does, with a per-instance claimed set for a corpse whose loot was never filled:
**one per corpse**. Handing one over on every visit once gave 32 cores off two corpses in 24 s.

**One core per landing, and neither gate covers the arming.** A second 64436 replaces the first, so
removal and apply run in one tick, a climb against a fall: the unit bounced for 26 s, each apply added
another uncapped 25 s of event delay, and no add came for the rest of the phase. `IsMimironCoreUseReady`
requires airborne, no 64436, and no live 34068 within 100 yd. `MOVEMENTFLAG_HOVER` alone holds through
the arm and comes back with the lambda while an aura is on; and between the use and the aura (64444 is
instant, but its 34068 is not findable next tick and its 64436 reaches the unit 3.8-4.4 s later) nothing
is visible, so two cores spent 0.2 s apart left the unit on the floor 0.3-0.5 s.
`ULDUAR_MIMIRON_CORE_ARM_MS` (6 s) stamps the use in `MimironFightState`, per instance so a new carrier
inherits it; the 34068 check stays for a core a player placed.

**Bank two and chain them, for one 40 s window.** A landing's 45 s hole is a hole in the Assault Bot
cadence, the only core source (30-31 s apart with the unit up, 65-75 s across a landing). Chaining does not
merge holes, since every landing runs `DelayEvents(25s)` again; it puts them end to end and keeps the
unit down 40 s in one piece, the second going down the moment the first's aura ends and 34068 despawns.
The carrier loots up to `ULDUAR_MIMIRON_CORE_BANK` (2) and holds the first until one of four escapes:
the second is in the bags; the held one has under `ULDUAR_MIMIRON_CORE_HOLD_EXPIRY_MS` (10 s) of life
left (`item_template` 46029 has **`duration` 60**, against observed waits of 32-52 s); the unit is at or
below `_CORE_HOLD_RELEASE_PCT` (25%), where the phase ends before another core matters; or the next
Assault Bot is up (`hold-bot-up`), since a landing only delays one still to spawn and holding past one
made first landings 33-45 s late.

**The chain needs a latch, because the count cannot tell**: one core left after spending the first of a
pair reads as a bank of one, and went back to `hold` until its own expiry. Every spend sets `chaining` on
the action, cleared once the bags are empty with no 34068 live; while set, a core in hand goes down as
soon as the unit is ready, ahead of any corpse, and one looted during the landing follows it.

**Wait under the unit, not where the last corpse fell.** A carrier holding or waiting out a live core
walks to within `_CORE_USE_RANGE` and stands there, handing the tick back once it arrives; walking from
the corpse at release cost 25 s of one window. `chain` and `hold-expiring` are noted at the use itself,
since a note before the walk alternated with `walk-acu` every tick.

**The fetch walks through fire.** The fire dodge (above the core node, `FORCED`) threw a carrier back from
corpses 28 yd off every 2 s at full health until three cores rotted, delaying the first landing ~140 s.
While `IsMimironCoreFetchDue` (carrier, room in the bank, a corpse waiting) the dodge waits for the 60%
health gate alone. Still open: the spray-lane escape steps the carrier 0.4-1 yd out each tick and its
walk steps back in, and a Bomb Bot step-out, spray and siren escapes and fire dodges taking the movement
lock can still keep it off a corpse until it rots.

**Who hits it.** Melee, pets and ranged DPS switch to it for the window; healers never do, and a Bomb Bot
in casting range still comes first. Ranged on the adds through a landing took 17-23 points against 35-44
with nothing else up. Tanks never reach this (`MimironSetDpsPriorityTrigger` stands down for them), and
nothing sends the tank to the Assault Bot either.

**Outside the window melee wait**: the generic picker handed them the airborne unit and `reach melee`
walked them under it into the fire (1,034 `flame dodge <-> reach melee` in one phase 3). The hold requires
an allowed target, and melee with nothing allowed while the unit is airborne hold the tick (`p3hold`,
like `p4hold`).

**The core grounds the unit where the unit already is, and the main tank decides that.** 64444 summons by
nearest entry conditioned on the ACU, so it lands under the unit (0.00 yd in one trace) and never under
the placer. The unit chases its threat target to within 30 yd, so a tank pushed out by a Bomb Bot sidestep
dragged it 16.8 yd off centre and left 7-12 of 25 out of range for both windows. `p3tank` pins the main
tank to the room centre, as `p1tank` does for the MK II, and `MimironTankAnchorGuardMultiplier` vetoes his
`reach melee` while the unit is airborne, in both modes (chasing it put him 13.7 yd off). That pins the
tank, not the unit: a melee tank builds little threat on it in the air, and it sat on ranged 22-45% of
phase 3.

## Emergency Fire Bots are kept, dodged, and killed before phase 4

Three at a time every 45 s from the ACU's `_events`, the first ~8 s into phase 3, so a core landing
postpones them too; the phase 3 defeat stops new waves and survivors carry into phase 4. They never enter
zone combat (the Bot Summon Trigger's `if (_option < 3) SetInCombatWithZone()` skips them) and never
attack, heal or repair: every 15 s each walks to the nearest Flames (Spread) and fires **Water Spray
64619** on arrival, which douses flames and hits players too. `SPELL_ATTR0_CU_CONE_LINE` turns its 15 yd
cone into a line about 2.3 yd each side, 18,850-21,150 frost plus Emergency Mode's 25% and a knockback.
The 25-man bot (34148, 50.4k HP, ~63k empowered) also carries **Deafening Siren 64616**, a 10 yd area
silence, from `creature_template_addon`. Faction 16, so attackable.

**Every one is kept until a 15% sweep.** `GetMimironKeptFireBots`, raid-wide, folded every 250 ms. At 15%
ACU health (`_FIREBOT_CLEANUP_PCT`) they all join the kill list at once, above everything but the Bomb Bot
for ranged; a loose one with no unit up (a handover stray) ranks behind the mech; melee keep them behind
the Assault Bot; and **nobody culls in phase 4**, since a stray spray line is cheaper than pulling anyone
off the rendezvous. In the phase 4 handover `MimironSetDpsPriorityTrigger` fires with nothing engaged
while a fire bot is in the room. Both alternatives were measured: an early cull spent 7% of phase 3
output and 21% of ranged bot-seconds on them at ACU 97.9%; demoting them let six live together, spray
248k and kill six bots; keeping two and culling the rest left the raid in a permanent cull with 1.7-2.4
alive while fire ran to 41-46% of phase 3 intake. A fire bot with no flame within 150 yd stops moving and
never sprays, so a raid that clears the field ends up with quiet ones. Watch `arc spread <-> fire bot`
A-B-A and the phase 3 casting share: nine to fifteen alive is nine to fifteen silences walking into the
raid, and those numbers would say a cull was right after all.

**Keeping them out of Mimiron's picker is not enough.** The bot tank killed both kept bots on its own,
since `MimironSetDpsPriorityTrigger` stands down for tanks and `mimiron target guard` only zeroes
`DpsAssistAction` for non-tanks. `RaidUlduarStrategy` excludes the kept set in `AppendTargetExclusions`,
covering the tank picker, `dps target`, `dps aoe target` and the attacker values at once; the only aims
left are in the first second after a spawn, while the 250 ms scan catches up. What still kills them is
splash, for the guard below.

**`mimiron fire bot aoe guard` reads the spell, not the threat type.** `MimironFireBotAoeGuardMultiplier`
holds splash while a kept bot is within `ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE` (**12**) of the bot or its
target: the widest raid AoE in play is about 10 yd, and 30 switched the raid's AoE off for all of phase 3
around a dozen bots, the phase a Junk Bot arrives every 10 s. It tests `SpellSplashes`
([the Aoe threat type is no splash marker](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).

**Water Spray one-shots, so it outranks the fire.** Cast time index 1, 0 ms: no cast to see, only the hit,
**23-26k against a 22-24k pool**, a single tick on a standing victim. Three fixes keep bots out of the
line: `MimironDodgeFlamesTrigger` stands down while the bot is inside a live spray line (the flames node at
`+ 4` outranks the fire bot node at `+ 3`, so the ladder cannot); the spray escape passes `FleeFan` a flag
dropping the ground-fire screen, since a node is 3.1k a second against the whole pool and a fan refusing
all eleven bearings for fire fell back unscreened; and `mimiron fire hold guard` zeroes its movers while
the bot stands in a line (a `reach melee` 3.5 s earlier had walked the victims in).

**The spray has to be predicted.** The script faces the nearest `NPC_FLAMES_SPREAD` every 15 s, or 5 s
after a spray, and sprays at once if it is within 5 yd, else walks to 5 yd short and sprays on arrival
facing the way it walked. The live line, the bot's current facing, appears only as it fires (1 of 14
victims had been in it 0.5 s before). The lane toward the nearest spread flame from
`ULDUAR_MIMIRON_FIREBOT_SPRAY_REACH` (5) short of it, read 2 s ahead, pointed within 20° of the real spray
in 90% of 126 and held 13 of the 14 victims. `ReadMimironFirefighterHazards` puts both in `sprayLanes`
and every spray test walks them, cheaply: 0.76 of ~18 living raid members stood in some lane at a time.
The bot's own spot is judged at 3.5 each side, a destination at `_SPRAY_STAND_HALF_WIDTH` (4.5), and the
sidestep lands at 5.0, since a lane jumps whenever a spray puts out its flame. `IsMimironSpotFireBotSafe`
refuses every spray lane (16 ahead) for everyone and the siren's 13 yd for casters and healers in 25-man,
in `IsMimironSpotSafe` and the fan; `mimiron fire bot action` (`ACTION_RAID + 3`) steps sideways out of the
lane, or away from the siren.

**The siren needs a stand radius past its run radius.** A kept bot walks to the next flame, which is in
the wedge, so spots picked just past 13 yd were walked into soon after: 212 `arc spread <-> fire bot`
A-B-A in one phase 3. Destinations clear `ULDUAR_MIMIRON_FIREBOT_SIREN_STAND` (17) and the siren dodge
takes the per-bearing hop to `_SIREN_FLEE` (19), while the bot's own spot is judged at 13. A bot already
walking somewhere clear of the siren does not dodge it: the sidestep cancelled the walk and nobody casts
on the move anyway. The spray line still dodges.

**Frost resistance works on it now, so a second paladin runs Frost Resistance Aura in phase 3.** Until
core `c1c9ff79b` it did nothing: 64619 has no damage class, so `Unit::SpellHitResult` never rolls it, and
its knockback made `SpellMgr`'s binary heuristic flag it, so `CalcAbsorbResist` skipped partial resists
too (0 of 26 hits resisted). It now takes partial resists in 10% bands, never a full one. The average is
R / (R + 506.5) against the level 83 bots, and Gift of the Wild's 54 shares exclusive aura type 143 with
the paladin auras, so only the higher counts: 9.6% on the druid buff alone (28% of sprays unresisted),
20.4% on the aura's 130 (none unresisted, three in four losing 20% or more). Replayed over one pull's 12
spray kills, the fix alone leaves ~8.9 and the aura ~6.9: the five killed from full needed 3-11%
resisted, and the rest were already hurt, which is the lanes' job. `GetMimironFrostResistancePaladin` picks it in hard-mode phase 3: never the fire carrier (the bot
holding `rfire` from the shared `BossFireResistanceTrigger`, dead or alive, else the first alive paladin
that node would pick); then a tank, a non-healer, a healer. A lone paladin keeps fire, which resists
22-31% of the Flames. The tank is the right carrier: 24 of 26 hits landed within its 40 yd, and 91% of the
living raid stood inside it. `MimironFrostResistanceAction` casts it directly, and `mimiron paladin aura`
holds the slot against every other aura and the shared fire node, lifting with phase 3 so Devotion comes
back.

## Pets need telling twice, in two different phases

`PetAttackAction`'s trigger node is commented out globally (`CombatStrategy::InitTriggers`), so a pet keeps
whatever it last latched onto and no encounter redirects it unless it says so.

In phase 3 the ACU hovers 15 yd up and ground pets park underneath it. `mimiron pet control action` sends
them to the nearest Assault Bot, then Junk Bot, then Bomb Bot, and calls `StopPet` when no add is up; the
Assault Bot first as the only core source. It always returns **false**, redirecting the pet without the
bot's tick. Once a core lands, the unit outranks all of them: on the floor, stationary, +50% for 20 s.

In phase 4 pets go where the melee go, and hold when the melee hold: a hunter's pet is about a fifth of
its damage, and leaking that into the rendezvous is what the floor exists to prevent. That includes the
ACU while the pair waits on it, reachable at ground level in phase 4.

**`MOVEMENTFLAG_HOVER` cannot mean "phase 3".** Nothing in the vehicle entry path or the phase 3 defeat
branch clears it, so it survives into phase 4 whenever the ACU was airborne when it went down; keyed on
it, the node fired in phase 4, found no adds, and stopped every pet in the raid for the whole phase. Both
branches are gated on the phase itself.

## Phase 4 is a rendezvous, and percent is the only correct ordering

`EVENT_FINISH` fires only when **all three parts are casting Self Repair at once**, and Self Repair (64383)
has casting-time index 192 = **15000 ms**. A part drops out at `health < 15000` (damage zeroed,
`UNIT_FLAG_NON_ATTACKABLE`, Self Repair cast), and if the other two are not down before the cast
completes, `SpellHit` brings it back aggressive and the phase resets.

So the three come down **level**. `HealthModifier` is 300 / 300 / **200**, so sorting on `GetHealth()`
ranked the ACU last every tick whatever its percentage. Ordering is on `GetHealthPct()` in **2% bands**:
raw percent has no hysteresis, and 25 bots burn two parts within a tenth of a percent of each other (1380
of 1564 target notes in one kill were a VX-001/MK II flip, each resetting a swing or a cast). The band
comes from health alone, so the tank node, the DPS node and the pets agree, and five fit inside the 10%
floor.

**All three occupy one point server-side.** VehicleSeat 3886 (VX-001 on the chassis) and 3806 (the ACU on
VX-001) carry `AttachmentOffsetX/Y/Z = 0,0,0`, `Vehicle::AddPassenger` relocates by exactly those offsets,
and `RelocatePassengers` rewrites passengers to `vehiclePos + offset` on every chassis move without
re-applying hover height; the visual stack is client-side. So everything splashes every part, and the ACU
is reachable by melee.

Ranged DPS own the ACU (`IsRangedDps`, not `IsRanged`, so a healer is never steered onto it or into the
hold); melee get it only while the pair waits on it.

**The ACU still arrives last, so the floor follows the leader.** Ranged aimed 94% of their damage at it and
it still lost 25.6k hp/s against the pair's 44-46k: one point, so pets, DoTs, totems and AoE spread across
the stack. Against a flat 10% floor the pair waited with the ACU at 20.2%, Self Repair started over a
16.7 s spread against the 15 s cast, all three returned at 50%, and phase 4 took 235 s against 112. The
floor is **`max(10%, leader − ULDUAR_MIMIRON_PHASE4_CONVERGE_PCT)`**, 4% or two bands, `leader` being the
part furthest from death: the pair parks just under the ACU and the three walk down together. Humans put
71% of their damage into the pair, which no bot change reaches: in phase 4 a player belongs on the ACU.
This holds in both modes.

**Melee at the floor hit the ACU instead of holding.** It is the leader then, so every hit closes the gap;
held, melee were targetless 24-81% of a phase 4. Melee and pets take it while the lower ground part is
within twice the converge (8 points) of it, and hold past that, since cleave splashes the pair. The main
tank never holds.

**Every restriction lifts the moment a part starts self-repairing.** `IsMimironPhase4` is keyed on VX-001
riding the chassis, not on all three being attackable, so it stays true through that window, the most
time-critical of the phase. Melee join the ACU, the floor is gone, and the raid pushes what is left.

`IsMimironPhase4` answers from cached lookups only: VX-001 or the ACU riding something covers the phase
bar the last seconds, and with only the chassis left, seat 3 holds the cannon in phase 1 and VX-001 from
phase 4 on. It is asked several times per bot per tick, so a grid sweep there would cost the raid every
phase.

## Plasma Blast is a cooldown check, not a tank swap

62997 is a 3 s cast, `TARGET_UNIT_TARGET_ENEMY`, aura 3 at 1000 ms period, base 16999 + 1 → **17000/s for
6 s = 102000**, no stacking, every 22 s; the 25-man id is **64529**, base 24999 → 25000/s. The debuff
expires 16 s before the next, so it is a heal check. Windows open at t=27, 49, 72, 95 and 117 for 54-119k
on a **43,407 HP** tank, troughing him at 11-27%.

**No swap.** It needed a second tank bot, and a taunt is never early enough: the cannon casts at
`me->GetVictim()` resolved **when the cast began**, so a taunt during those 3 s owns nothing, and a swap
sticks only by taunt setting the taunter's threat equal to the highest well before the cast. The MK II
is fully tauntable: `flags_extra` 524289 is `OBEYS_TAUNT_DIMINISHING_RETURNS | INSTANCE_BIND` with no
`CREATURE_FLAG_EXTRA_NO_TAUNT`, and `CreatureImmunitiesId` -361 masks 21 mechanics and effects
98/124/144/145 (knockback and pull, not taunt). DR resets after 15 s against a 22 s cadence.

`MimironPlasmaBlastDefensiveTrigger` fires **while the cannon is casting** and one bot spends one big
defensive; a health threshold notices around the third of six ticks. `NextTankDefensive`
(`RaidTankDefensive.h`, shared with Obsidian Sanctum) picks the tank's weakest ready cooldown, shortest
first, and nothing while one is running. A healer steps in only when the tank has none: `pain
suppression`, `guardian spirit`, `hand of sacrifice`, never `hand of protection`, which sheds threat and
hands the boss back. `ClaimMimironPlasmaWindow` gives the window to the first claimant, so a Shield Wall
and a Pain Suppression never share one: either alone carries it and the spare is worth more 22 s later.
The claim lasts 12 s (`ULDUAR_MIMIRON_PLASMA_WINDOW_MS`; cast start to the last tick is ~9 s, and an 8 s
claim lapsed into a second button), and in phase 1 the main tank's class nodes are held off its
`NextTankDefensive` buttons (`MimironPlasmaDefensiveHoldMultiplier`, as Obsidian Sanctum does), since they
fire on health mid-window. A paladin tank opening with Avenging Wrath locks Divine Protection for 30 s, so
window 1 goes to a healer external. On a replay, windows 1-3 get one button each and 4-5 stay bare (min HP
74-82%). Every window with a defensive and 60-90k of healing left the tank at 38% or better; the ones
that killed him had 5-11k of healing behind them, once Napalm had taken the healers.

`GetMimironPlasmaBlastCast` tries both ids: `spelldifficulty_dbc` remaps 62997 to 64529 on 25-man
([spell ids by difficulty](../../engine/pitfalls.md#detection-that-never-fires)). Every Ulduar constant
was swept against the table; Ignis' Scorch and Flame Jets had the same hole, no others.

**Still untested in a pull**: a human has main-tanked every Firefighter attempt since, so no bot tank has
taken a Plasma Blast with the claim and hold in place.

## The tank leaves with three seconds of threat and the boss does not follow

The 53 yd walk west works only if the MK II is his. He engaged at 13-15 s, walked at ~16, and the boss
switched to a melee DPS 2-3 s later and stopped, leaving him on the MK II 20-65% of phase 1.

`IsMimironTankDragReady` holds the anchor back until he owns it: `GetVictim() == bot` **and** threat at
least `ULDUAR_MIMIRON_TANK_THREAT_LEAD` (1.3) times the highest **non-tank** threat, or `_HOLD_MAX_MS`
(10 s), whichever first. Until then he has no slot, so `reach melee` owns him and he builds threat on the
boss. It **latches** raid-wide in `MimironFightState`: a mid-phase dip must not restart the drag with the
raid already spread out. The timeout is not optional: a human holding the boss, or a dead tank, would pin
the fight at the pull spot.

`MimironTankAnchorGuardMultiplier` stops the yo-yo between `reach melee` (`ACTION_HIGH`) and the
formation: it zeroes `reach melee` only while the boss is his and the latch is set, when the boss is
following him anyway, so losing aggro lifts it and he can run back and taunt. It also zeroes `tank face`
under the same conditions: the MK II has no frontal, and the ~5.6 yd step off the slot, just past the
5 yd tolerance, gave 36 round trips a pull.

**Tricks of the Trade landed on the melee who then pulled the boss**
([its target value](../../engine/pitfalls.md#detection-that-never-fires)). `mimiron generic redirect
guard` suppresses the smart-target node in phase 1, and `MimironRedirectThreatAction` owns the redirect,
subclassing `RaidRedirectThreatAction` as Hodir and Freya do: the main tank in phase 1, `nullptr` after,
since phases 2-4 split two mechs across two tanks. `UldThreatRedirectMultiplier` holds the class-generic
on-main-tank nodes for the whole encounter, which is why Misdirection was never cast before. With both,
the MK II sat a median 5.4 yd off the anchor (p90 13-15, from 10.5 and 108) on the main tank unbroken.
With a single tank, after his death the MK II chased a warlock through the camp.

**Phase 4 lost the MK II off the tank three ways**, and Shock Blast killed ranged it walked into. `p4tank`
was handed out whatever the MK II's victim; his focus flipped to VX-001 whenever it led, which never takes
a victim, so `lose aggro` could not see the MK II; and at the floor he `AttackStop`ped with the node
claiming every tick, blocking the taunt. `GetMimironPhase4TankFocus` is the MK II whenever it is
attackable and never a hold (his 1-2k/s barely moves the floor), and `p4tank` goes out only while the MK
II is his or nobody's, so without it `reach melee` and his taunt take him back. `tank assist` read VX-001
as a loose target and flipped him 308 times a phase 4, so `mimiron phase 4 tank target guard` leaves his
phase 4 target to `MimironPhase4FocusAction` alone.

**Under Firefighter the phase 4 tank spot moves off the fire.** The centre is where `p3tank` stood for
three minutes. `GetMimironPhase4TankSpot` keeps the fixed spot until more than `_PHASE4_TANK_FIRE_LIMIT`
(3) nodes lie within `_CLEAR_RADIUS` (14 yd: the 9 yd melee ring plus a node's 5), then drags to the
least-burning of the centre and 8/16/24 yd rings × 12 bearings, only when it is `_FIRE_MARGIN` (2) cleaner,
ties to the shorter drag. It holds `_HOLD_MS` (30 s), and never moves while a barrage spins up or fires,
since the chassis carries the apex. Staging and normal mode keep the fixed spot. A drag also walks the
ranged wedge, anchored on VX-001, which is why the limit and hold are high: at 1 node and 20 s it
re-picked six times in one phase, three drags of 25-36 yd between the same two spots.

**Open: the tank's fire dodge drags the chassis to the edge.** Fire spreads toward the raid, so away from
it is outward: 15-16 of the tank's 17-20 flames dodges a phase 4 ended over 30 yd off centre, the chassis
sat past 30 yd 24-42% of phase 4 and ranged and healers past `ULDUAR_MIMIRON_ROOM_RADIUS` 29-34%; Shock
Blast escapes stalled at 40 yd, and a 65 yd wedge slot pathed a loop back past a Frost Bomb. Untried fix:
leash the main tank's phase 4 dodges to ~28 yd of the room centre, falling back to the unleashed fan.

**Self Repair must not hand out the phase 1 spot.** A part going `NON_ATTACKABLE` fails a
three-attackable test, which once gave the tank `p1tank`, 53 yd west, mid-rendezvous, and turned the phase
1 guards back on through `MimironPhase1Active`. Both key on `IsMimironPhase4`, the vehicle seat.

## Napalm Shell wants one bot per shell

65026 is a **5 yd** blast, 9,424 on impact plus 5,999 a tick for 8 ticks, about **57k** against 22-30k
pools, from the cannon (34071) every 14 s on a 2 s cast. The radius is centre to centre
([range and reach](../../engine/raid-mechanics-lessons.md#range-and-reach)), so slots a little over 5 yd
apart isolate every bot.

It picks uniformly among players at `GetDistance2d(mkII) > 15.0f`, raw **> ~24.5 yd** with both reaches,
at cast start, and lands where the target stands at cast end. An empty pool falls through to
`SelectTarget(Random, 0, 100.0f, true)`, a threat-list pick with no distance floor. A non-empty pool was
hit in 23 of 26 casts; the rest were a target walking in or out during cast and flight.

Victims a cast rose to 3-8 once the tank held the boss beside a point camp (worst casts 13 and 14,
phase-1 wipes), for two reasons:

- **One point disables the unstacker.** `mimiron formation guard` returns 0 for `combat formation move`
  inside a slot's tolerance, right for per-bot slots and fatal for a shared one; a 10 yd camp tolerance
  switched it off for the whole camp.
- **A camp on the floor empties the pool.** The raw 24.5 clamp dragged the camp onto the MK II: mid-drag
  the whole raid was within 16.4 yd, the pool was empty and the fallback landed in the camp; just past the
  floor, all ten were eligible on one point.

The wedge answers both. The MK II stands ~5 yd off the tank spot toward the camp, so the first row is 21:
replaying traced positions, a slot inside 15 yd drops from 40-49% of the time with a first row of 19 to
17-21%, and the pool is empty 0.3-1.3%. `GetMimironPhase1DisperseDistance` is 5.5 in both modes, under the
6 yd rows, and the camp uses the ordinary 5 yd tolerance. It cost no range (ranged casting 51-63% against
45-50%) and holds **1.2-1.4** victims a cast, median nearest neighbour 5.9 yd; only the ~0:16 opener,
mid-drag, still falls back (1-3 victims). The knock-on of a bad camp is the wipe: healers die, then Plasma
windows kill the tank.

## The phase handovers are a minute of wasted time

Measured from the boss script: **47.75 s** from phase 1 to 2 (retreat 5 → elevator 6 → VX-001 summoned 6 →
18 → 4 → 5 → 2 → 1.75), **24 s** to phase 3, **31.8 s** to phase 4. VX-001 boards the chassis **18.8 s**
into the last one, and `GetMimironStagingFocus` returns it only then, so that is when `p4tank` goes out,
still over twice the walk in from a phase 3 wedge slot.

A defeated mech sets `UNIT_FLAG_NOT_SELECTABLE` and stays in the world (the MK II parks 58 yd off centre
through phases 2 and 3), and the next mech carries the flag until its phase starts.
`AttackersValue::IsPossibleTarget` rejects it, so `GetUldUnitByEntry` is blind for the whole handover,
every Mimiron node stands down, and the engine falls through to `follow` at relevance **1.0**.

**That fallthrough is the answer.** Two handover formations were built and removed: an 8 yd melee ring
plus 22 yd caster ring on the room centre, then a 44 yd rim lap. `GetMimironSpreadSlot` returns false for
everyone while staging. Human masters stood 43-54 yd from the centre on their own, against the lap's
49-52: a raid leader already puts the raid where the fire wants taking, and chains grow 1.22 yd/s and
cannot catch a walking pack. Parking *on the centre* is what to avoid: the rings there put 86% of 37
nodes inside 25 yd of the centre and took 203k, all fire, with nothing attackable.

**The master decides whether phase 2 starts clean.** The first Frost Bomb clears flames within 30 yd of
one node: a master standing still in the west kept the fire in one cluster and the bomb cleared 37 of 37;
one walking ~45 yd north split it, the bomb cleared 9, and phase 2 burned at the 40-node cap. Masters who
stood 49-54 yd out and walked in over the last ~10 s opened with 324-399k of Rapid Burst in 30 s;
staying ~40 yd west stacked the raid on one line, 653-679k, 10-24 victims a tick. Following stays, by
choice: stand still in the west, then walk to the centre over the last ~10 s.

**One exception.** `ULDUAR_MIMIRON_PHASE4_TANK_SPOT` is handed out while staging: it holds VX-001's chassis
still, and every phase-4 bearing, radius and offset is computed against a stationary apex (under
Firefighter it moves off fire once phase 4 starts). It is 1.4 yd off the room centre because all three
handovers converge there: VX-001 is summoned at it, `ACUSummonPos` is (2744.650, 2569.460, 380.0), a
defeated ACU is walked back to (2744.65, 2569.46, 381.34), and the chassis ends there after charging to
(2755.77, 2574.95) at 10 s. `GetMimironStagingFocus` survives only to tell a phase-4 handover from the
others: `Creature::FindNearestCreature` is a grid check on entry, alive state and range with **no
selectability filter**.

Two things need no code. The **elevator knockback** 11 s into the first handover needs no guard, since
VX-001 is not summoned until 17 s. And there is **nothing to do before a pull or after a wipe**: the MK II
is `NOT_SELECTABLE` until pulled, and evade despawns VX-001 and the ACU.

**Eating and drinking happen**, and are worth it: instance strategies are added to both
`BOT_STATE_COMBAT` and `BOT_STATE_NON_COMBAT` (`PlayerbotAI::ApplyInstanceStrategies`), and healers went
from 35-65% to 71-100% mana across one handover. But `DrinkAction`/`EatAction` push the next AI check
back 12-18 s, so a sitting bot dodges nothing and, standing still, is the nearest player to a chain head:
two burned to death drinking. `mimiron drink guard` vetoes `drink`/`food` within 15 yd of a flame node.

## The ring slot is what kills the Rocket Strike dodge

Rocket Strike target selection **removes every player within 15 yd first** and picks from the rest, so it
lands on the ranged ring by design. The marker (34047) burns a **5 s fuse**, blasts 3 yd and despawns at
6 s. 63041 is **5,000,000 damage** in `EffectRadiusIndex` 15, a flat **3.0 yd**, and a creature caster adds
no reach, so 3.0 is real: one occurrence is one death.

Fleeing is the easy half; the hard half is not walking back to the slot the marker still sits on.
`IsMimironSpotSafe` therefore tests the **slot**, not the bot, and holds while any live marker is within
8 yd of it: a bot that dodged is by definition clear.

**Run 5, stand 8.** The trigger once compared `GetDistance2d(...) <= 10` (11.5 yd centre to centre) and ran
a flat 10 yd, so one marker moved five to eleven bots of a 6 yd ring: 40-149 rocket legs a pull against
one blast hit in four pulls, and the walk back was phase 2's largest formation cost. It runs at
`ULDUAR_MIMIRON_ROCKET_RUN_RADIUS` (5) by `GetExactDist2d` and hops per bearing onto the
`_ROCKET_CLEARANCE` (8) circle, the same run/stand hysteresis as the bomb and the siren.
`mimiron rocket guard` holds the unit-chasing movers (reach, heal reach, `set behind`, `follow`, the
approach) within the clearance plus `ULDUAR_MIMIRON_ROCKET_HOLD_MARGIN` (4) of a live marker, as the Frost
Bomb guard does: `reach spell` walked three ranged back onto one in a kill.

`IsMimironSpotSafe` covers Firefighter's ground fire on the same argument, behind the hard-mode check
(else the flames node pushes a bot out of a burning slot and the formation pulls it straight back), and
the Frost Bomb for the mirror reason (else the formation walks the raid back inside the blast for the
fuse). The fan screens both too. Both come from one `GetMimironFirefighterHazards` pass, since the fan
tests eleven bearings and rescanning a 50-node field per bearing is not free; that read,
`GetMimironMarkers` and the Rapid Burst carrier are answered once per trigger pass under
`EncounterTriggerPassId`, since the flames, formation, approach and fire bot triggers all ask; actions and
multipliers read live. `MimironLethalWindowActive` still constructs triggers per call
([cost](../../engine/raid-mechanics-lessons.md#what-a-strategy-costs-per-raid)).

## What a trace answers

Position, verdicts and movement commands come from the raid-agnostic streams; whether a trace counts is
[observability](../../systems/observability.md#is-this-trace-evidence). The Mimiron rows sit under
`--notes mimiron.`:

| Key | Says |
|---|---|
| `phase` | 0 none, 1-4 the phase, 5 a handover. 0 also covers the 1→2 handover until VX-001 is summoned ~17 s in (`GetMimironStagingFocus` sees only VX-001 and the Aerial Command Unit), so `mimiron.py` prints it as a `Z` span, then `H`. Per instance |
| `core` | The Magnetic Core window is open, which is not the same as the unit being on the floor. Per instance |
| `carrier` | Who is fetching the core. Per instance |
| `corestep` | Where that carrier stopped: `no-acu`, `no-corpse` (none with a core left), `walk-corpse`, `loot`, `bags-full`, `pending` (a core is still live), `hold` (waiting for the second), `walk-corpse-second`, `loot-second`, `walk-acu`, `blocked`, `use`, preceded at the use by `chain` (the rest of a bank) `hold-expiring` (the held one was about to) or `hold-bot-up` (the next Assault Bot is up) |
| `slot` | Which formation shape answered — `p4tank`, `p3wedge`, `p3tank`, `p1tank`, `p1stack`, `hmwedge`, `p2melee`, `p4melee`, `ring`, `none` — with index/count and the point |
| `stack` | A phase 1 stack anchor switch, `<from>:<nodes> -> <to>:<nodes>`. Per instance |
| `tankspot` | A Firefighter phase 4 tank spot move, `x,y <old>><new>` nodes within 14 yd. Per instance |
| `plasma` | Which defensive answered the Plasma Blast window, or `covered`/`none` |
| `barrage` | Which dodge rule fired — `clear`, `hold`, `stepin` (melee back onto the ring), `boundary` (out from under VX-001), or `ahead`/`inside`/`trailing`/`refuge` (off fire inside the safe sector) plus a direction — with the bearing clockwise of the centreline, bucketed to 15°, since at whole degrees it changes every tick and one kill wrote 2,300 rows. Not the radius: `snap.u` samples the position four times a second beside the hazard row's apex |
| `wedge` | `reaim N`, the Firefighter wedge's new centreline in degrees, when a barrage moved it. Per instance |
| `flee` | The bearing fan's outcome, `ok`/`fallback`/`unsafe`/`none`/`locked`, and how many bearings each filter refused (`back`, `mine`, `cone`, `fire`, `bomb`, `burst`, `shock`, `spray`, `move`). `what` names the hazard: `shock`, `shock fire` (the second sweep, fire allowed), `shock anchor` (the escape aiming at a slot once every bearing failed), `rocket`, `frostbomb`, `spray`, `siren`, and `flames+N` per ladder rung, so which rung won is readable. `locked` means the movement lock refused before a single bearing was tried; `unsafe`, that the fallback would have landed in a Shock Blast, a Frost Bomb, the barrage cone or a spray lane |
| `campturn` | The phase 1 camp's sight turn toward the room, degrees, on change. Per instance |
| `approach` | A formation leg that is not the plain walk to the slot: `substitute rN` (yd off the slot) or `detour ±N` (degrees off the direct bearing) |
| `close` | A melee approach steered round the fire instead of straight at the target |
| `dpsrule` | Which priority rule chose the target, `held:` when the hold kept it, `fallback`, `p3hold` or `p4hold` |

`flee` has no substitute: a refused bearing reaches no MotionMaster and so writes no `move` record, which
leaves a dodge that refuses every bearing completely silent.

`tools/botobs/bosses/mimiron.py --help` lists its reports. Beyond it: `--walk` charges a moving sample to
the bot's last accepted `move` within 6 s (`ATTRIBUTE_MS`), `?` being a walk with none, a human or a
chase; `--p3` measures each landing off the unit's own Z rather than the core latch; and a phase that
comes round twice (P4, H4, P4) gets a letter, `P4b`, or its deaths and yards count under both spans.

The Laser Barrage cone is a `haz` `sweep` row every 250 ms (`lead`, `sweep`, `rate`, `live`), originated
on VX-001, which in phase 4 is the chassis, so a drifting apex shows. Written by hand because the cone has
no world object, and neither has the DB Target it aims at: that one is not hostile, so the snapshot sweep
skips it too.

Still invisible: **creature auras**, because `NoteAura` is roster-gated, so 64436 on the ACU never appears
and `mimiron.core` says only that a core is on the unit, not that the unit is down (its Z in `snap.u` says
that); and **anything past the nearest 40 hazard creatures under Firefighter**, since fire spreads all
fight: a Proximity Mine or Frost Bomb can drop out of the snapshot, and so out of the containment test a
death is measured against, and a fire count reads 40 at most.
