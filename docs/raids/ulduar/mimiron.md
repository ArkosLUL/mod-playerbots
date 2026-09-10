# Mimiron
## Firefighter

A player presses the Big Red Button before the pull; `_hardmode` is set at activation and never
cleared. Bots do not detect it: `IsMimironHardModeActive` reads `AiPlayerbot.UlduarMimironHardMode`
and nothing else, so the config is a declaration and has to match what the raid actually does.
`SPELL_EMERGENCY_MODE (64582)` is the empower aura on whichever mech is active, if a live signal is
ever wanted — those mechs *are* valid attack targets, unlike Mimiron himself.

**The fire is the boss.** Across three 2026-09-07 pulls, Flames 64566 was the largest single damage
source every time — 45%, 26% and 33% of everything the raid took, ahead of the Frost Bomb and every
mech ability. Roughly half of all effective healing goes to it.

Once the dodge and the Frost Bomb were fixed it fell to **22% and 19%** on 2026-09-08, and phase 2
per living bot per second went 449/426 → 356/332. It is still second. First is now Heat Wave.

Mimiron seeds it for the whole encounter, handovers included: `EVENT_SPAWN_FLAMES_INITIAL` every
**30 s** drops three `NPC_FLAMES_INITIAL` (34363) 5 yd from three random raid members. Each chain
then adds one `NPC_FLAMES_SPREAD` (34121) every **5.75 s**, **7 yd toward the player nearest that
chain's newest node**, and **stops growing while any player is inside 4 yd of that node**. Nodes
carry aura 64561, which ticks 64566 for ~3.1k a second inside **3 yd**, and they never expire. Both
entries are non-selectable trigger creatures, found by scanning `"nearest npcs"` — the same idiom as
Freya's beams.

**So where the raid stands is where the fire goes, and that is the only lever over it.** Three things
put fire out and nothing else does: the Frost Bomb, VX-001's Flame Suppressant (65192, 10 yd around
itself every 10 s in phase 2 — which also lands a 51% cast slow, so it is not a place to stand), and
one full-room clear 60 s into phase 1 (64570).

**Which is why phase 1 is fought in the west.** The MK II is held at
`ULDUAR_MIMIRON_PHASE1_TANK_SPOT` **(2691.576, 2568.532)**, 53 yd off centre, and ranged and healers
clump 22 yd off it on one of `ULDUAR_MIMIRON_PHASE1_STACK_SPOTS` — so every seed lands out there
rather than on the ground VX-001 is summoned onto. It works: **70% of phase-1 fire damage was taken
inside 20 yd of the centre** before the move and **0.0%** after it across all three 2026-09-10
pulls, with no node left within 24 yd of centre at the handover.

The tank spot is **51.5 yd from Mimiron's own spawn** (2742.53, 2560.99), and he evades past **80 yd**
from it on every tick (`boss_mimiron.cpp:394-398`) — that check is the only leash in the encounter,
the MK II has none of its own. navprobe `--nav 0x09`: 0.223 to poly, flat at **Z 364.314**, 16/16 at
12 yd. The four stack anchors sit 22 yd out, 45° apart, on the only arc with floor:

| # | bearing | x | y | to poly | 6 yd | 10 yd | to centre |
|---|---|---|---|---|---|---|---|
| 0 | 75° | 2697.270 | 2589.782 | 0.22 | 12/12 | 11/12 | 51.6 |
| 1 | 30° | 2710.629 | 2579.531 | 0.22 | 12/12 | 12/12 | 35.5 |
| 2 | 345° | 2712.827 | 2562.837 | 0.22 | 12/12 | 12/12 | 32.5 |
| 3 | 300° | 2702.576 | 2549.479 | 0.22 | 12/12 | 12/12 | 46.6 |

All settle at **Z 364.314**, all stay ≥32.5 yd from centre, and index 0 is the default. 105-255° is
excluded: off mesh against the west wall, or up in the raised doorway alcove. The mesh also has a
hole from **y 2582 to 2591** — 3.6-4.8 yd off poly, Z never settling — that swallows anything placed
due north of the tank spot.

Each anchor is a **fixed point**, never a slot that tracks the boss: chains grow toward whoever is
nearest their head, so an anchor that drifts smears the field along behind it. A slot gives ground
only when the MK II is further off than `spellDistance - ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN`,
because a slot past casting range deadlocks instead of correcting — `reach spell` is `ACTION_HIGH`
against the formation at `ACTION_RAID`. That is also what a dead main tank looks like.

**The clump is what the field converges on, so its own anchor burns first.** One 2026-09-10 pull
took **538k phase-1 flame damage with the median victim 3.4 yd from the anchor** — 26% of everything
taken, 329k of it on ranged and 191k on healers against 16k on melee — with 5 nodes inside 10 yd of
the anchor and 11 inside 20. `GetMimironPhase1StackAnchor` walks the whole raid to a cleaner one: it
counts live nodes within `ULDUAR_MIMIRON_STACK_FIRE_RADIUS` (10) of each anchor, and switches when
the live one carries more than `_FIRE_LIMIT` (2) **and** another is cleaner by `_FIRE_MARGIN` (2),
then holds it `_HOLD_MS` (15 s). Hysteresis both ways: chains grow 1.22 yd/s, so a bare "stand on
the cleanest" paces the raid across the arc all phase. Neighbours are 16.8 yd apart, which clears a
5 yd node cluster and a 7 yd chain step. **Decided once per instance** in `MimironFightState`, never
per bot — twelve bots each picking their own cleanest anchor is twelve clumps.

Clumping is paid for in Napalm Shell, which splashes 5 yd: it took **1.38 and 2.20 victims a cast**
against the old spread, already 6 at once on 7 of 40 casts in the tighter pull, for 26% and 37% of
phase-1 damage taken. `ULDUAR_MIMIRON_PHASE1_STACK_DISPERSE` is **3.0** against 5.5 elsewhere, which
with `ULDUAR_MIMIRON_SPREAD_TOLERANCE` (5) leaves a blob about 10 yd across — a Napalm on its edge
clips part of the group, not all of it, and packing inside the splash radius buys nothing back. The
bill never came: clumped, it took **1.20, 1.79 and 1.33 victims a cast**, under the spread it
replaced, so 3.0 is not the knob it was flagged as. All of this is Firefighter-only; normal mode has
no fire and keeps the room centre and 5.5.

**Emergency Fire Bots (34147)** never enter zone combat — the Bot Summon Trigger's
`if (_option < 3) SetInCombatWithZone()` skips them — and only run to flame nodes and cast Water
Spray, never healing or repairing Mimiron. They are not friendly, though: `creature_template` gives
them faction 16, the same as a Junk Bot, so they are attackable, and
`MimironSetDpsPriorityAction` puts them on the kill list whenever hard mode is on. That
contradicts `playerbots.conf.dist`, which promises bots leave them alone.
Unresolved, because none have spawned in a traced pull — they come from the phase 3 ACU summon
trigger, and no Firefighter attempt has reached phase 3.

## The Frost Bomb is 30 yd, and it lands in your own fire

VX-001 casts 64623 at `SPELLVALUE_MAX_TARGETS 1` from 1 s into phases 2 and 4, repeating every 45 s.
`acore_world.conditions` restricts it to entry **34121 carrying aura 64561**, so it never targets a
player: it picks a burning flame node, which summons `NPC_FROST_BOMB` (34149). That creature's
SmartAI detonates **exactly 10 s later** — 65333 in 25-man, **30 yd, 47124 base**, plus a knockback,
plus a dummy effect that despawns every flame it catches. It is the raid's fire extinguisher as much
as its hazard, and gathering the fire into one part of the room is what aims it. Bots avoid the bomb
*creature*, so no spell id is needed and 10/25 are covered identically.

47k against a 22-24k bot health pool means health is irrelevant and healing cannot answer it. It is a
pure positional check, and an easy one: at spawn the worst-placed bot in a traced pull needed 23 yd
of travel, 3.3 s against a 10 s fuse. navprobe has the room flat and on mesh to 45 yd across all 16
headings and 15/16 at 55, so there is always floor to run to.

`ULDUAR_MIMIRON_FROST_BOMB_RADIUS` was 12 — a placeholder its own comment flagged as unconfirmed —
and served as both the trigger range and the flee distance, so nobody between 12 and 30 yd reacted
and anyone who did stopped 18 yd inside the blast. One bomb killed 13-15 of 25 inside 0.15 s in each
of three pulls, which is what ended all three. The two jobs are now separate constants: 30 for the
trigger and every spot test, `ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE` 34 for where to stand.

The node also has to win the tick. Rocket strike, the flames dodge and the frost bomb all sat on
`ACTION_RAID + 4`; `Queue::findHighestRelevanceBasket` breaks an exact tie by push order and
`Engine::DoNextAction` stops at the first action returning true, so the flames step ended the tick
143-221 times a pull against 6-13 reaching the bomb. The ladder is now rapid burst +8, barrage +7,
frost bomb +6, rocket strike +5, flames +4. `mimiron shock blast` at +3 still sits below the flames
step — a 99999 blast losing to a 3k tick — but nothing has died to it yet.

It worked. Across the two 2026-09-08 pulls the bomb did **83,180 (2.2%) and 0**, against 697,314 and
612,008, and killed **one bot and none** against 13-15 at once.

## Two dodges fought over the fire, and both were too short

Standing in fire totals only ~204-258 bot-seconds a pull. The damage is all in the tail, where 5-14%
of exposures ran 5 s or longer at 20-50k each. Bots were not walking into fire; they were stuck in
it, for three compounding reasons.

**Hops shorter than the chain step.** Chains grow in 7 yd steps and 50-60 nodes are live by the
middle of the fight. Measured accepted moves: `avoid aoe` median **4.0 yd**, the Mimiron dodge
**5.0 yd** — both land on the next node along. The Mimiron dodge meant to leave the whole field, but
only collected nodes within 5 yd, so its "clear the outermost node" spread was near zero.

**`avoid aoe` takes the movement lock and then reports failure.**
`AvoidAoeAction::AvoidUnitWithDamageAura` is built for exactly this hazard — a non-selectable trigger
within 15 yd whose periodic-trigger aura does school damage, which 64561 → 64566 is. It flees to the
damage radius itself (**3 yd**), then **ignores the result and returns false**: 642 evaluations
across two pulls, **zero** wins, **619** accepted moves. At relevance 90 it outranks every Mimiron
node (60-65), so it goes first every tick a node is in range. Its own rate limiter is dead code —
`lastMoveTimer` is assigned only inside the `tellWhenAvoidAoe` branch.

**A shared cooldown between the two.** `MovementAction::FleePosition` refuses outright while the
shared `"recently flee info"` list holds an entry younger than `minInterval` (1000 ms). **71%** of
the Mimiron flame dodge's FAILEDs had an accepted flee by that same bot in the previous second — its
own last hop, or `avoid aoe`'s.

The flames dodge is now on the shared Mimiron fan instead of `FleePosition`, and
`MimironAvoidAoeGuardMultiplier` vetoes `avoid aoe` outright while hard mode is live.
`AvoidAoeAction`'s own two bugs are left alone: it runs in every encounter in the game and deserves
its own change.

That much worked — exposures of 5 s or longer fell from 5% to **1-2%**, p90 episode length from 3.0 s
to 1.1-2.0, and `avoid aoe` recorded **zero accepted moves** behind 339 and 309 vetoes. Two things it
still got wrong, both fixed in the dodge itself.

**One fixed hop, and it was the long one.** Cluster edge plus a full 7 yd chain step meant
`mimiron.flee` read **`flames fallback` 2121 against `flames ok` 468**: four dodges in five found no
clean bearing at that distance and fell through to an unscreened `MoveAway`. It is now a ladder,
`ULDUAR_MIMIRON_FLAMES_STEP_LADDER` = 3, 5, 7, 10 yd, each rung screened against the whole field by
the same fan and the first clean one taken; only the last rung may fall back. Short rungs are safe
precisely because they are screened, and what a short one saves is the walk home.

**And it dodged whatever the cost.** A node ticks ~3.1k against a 22-24k pool, so a healthy bot has
seven ticks of margin while the round trip costs 24 yd of uptime.
`ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT` is **60**: above it the bot stands in the fire and keeps
working. `ULDUAR_MIMIRON_FLAMES_DODGE_NODE_OVERRIDE` (2) overrules the gate, because two nodes halve
the margin to about four seconds — not long enough to notice a health bar and then walk 12 yd. Watch
this one: phase 2 has no healing slack, so if fire per living bot per second climbs back above
356/332 the threshold is too low.

**And the ladder was dead anyway.** On 2026-09-09 `mimiron.flee` read `flames+3 none` 2,173 times,
`+5` 2,161, `+7` 2,159 and `flames+10 fallback` 2,156, against **320 `flames ok` — 13%**. But the
hazard filters had refused only **5.8 of 11 bearings** on those calls, and 60 of them refused none at
all: the bearings were clean and the *mover* said no. `mimiron dodge flames action` logged **1,249
moves refused `wait` against 387 issued**, median hold 917 ms, p90 3.8 s. It issued at
`MOVEMENT_COMBAT`, so does `mimiron arc spread action`, and `IsWaitingForLastMove` wants a *strictly*
higher priority — so a formation leg blocked the dodge for that leg's whole duration and the bot
burned through it.

It is `MOVEMENT_FORCED` now, like every other hazard node here.
`ULDUAR_MIMIRON_FLAMES_MAX_HOP` (**12 yd**, about 1.7 s of lock) caps the leg, because a FORCED leg
blocks the Rapid Burst and Frost Bomb dodges in turn and Rapid Burst has no telegraph to stand down
for; the barrage does, and the trigger stands down for it outright. The fan also counts what the
mover refused — `move` in the `mimiron.flee` note, and a `locked` outcome from testing the lock once
up front instead of 44 times — because reading a mover refusal as a hazard refusal is what hid this.

## Raising the fire dodge to FORCED handed it the Shock Blast escape to cancel

Fixing that dodge cost more than it bought. At `MOVEMENT_COMBAT` it was dead — a formation leg
blocked it for its whole duration — so it went to `MOVEMENT_FORCED`, where every other Mimiron
hazard dodge already was. Equal `MOVEMENT_FORCED` blocks, and fire fires ~2,500 times a pull against
Shock Blast's five, so fire won the lock on volume and the escape lost:

```
64.5-65.8  mimiron shock blast action   forced  ok=1   escape issued, seven bots running
66.2-67.5  mimiron dodge flames action  forced  ok=1   fire leg overwrites it, they turn round
67.5-68.1  mimiron shock blast action   forced  ok=1   re-issued
68.0       Shock Blast lands                           all seven dead, still 9-15 yd out
```

Shock Blast 63631 hits for **82,450-109,125** against 22-30k pools, measured reach **15.2 yd** — so
`ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST` (18) is the right number — and lands only 1-3 times a pull.
One cast at t=38.4 killed **11 of 25** and ended that pull; ranged logged **30 `shock locked`** flee
notes in another's fatal window. Every other FORCED dodge took the same hit: `rapidburst locked` 327
against 175 issued, `rocket locked` 58-74, `frostbomb locked` 85.

Both halves of the fix are needed. `mimiron shock blast trigger` moved from `ACTION_RAID + 3` to
**`+ 5.5`**, above the fire dodge at `+ 4` — but relevance only picks which action runs, not which
move the lock accepts, so `MimironDodgeFlamesTrigger` **also stands down** whenever the barrage,
Shock Blast, a Rocket Strike, the Frost Bomb or Rapid Burst is live. Rapid Burst is tested last of
the five: it walks the group to build its cone window where the others read a cast bar or a nearby
creature.

## Laser Barrage is a 104° cone, not a beam

The single most load-bearing number on this boss, and it contradicts every public guide.

**63297 and 64042 deal no damage.** They are `SPELL_EFFECT_DUMMY`, placing the two beam *visuals*
via `TARGET_DEST_CASTER_FRONT` (60 yd) plus `TARGET_DEST_DEST_LEFT` 4 yd / `TARGET_DEST_DEST_RIGHT`
6 yd. Reading those 4/6 yd radii as a beam width is what produced the old dodge.

The damage is **63293**: `SPELL_EFFECT_SCHOOL_DAMAGE`, `TARGET_UNIT_CONE_ENEMY_104`, radius index 28
= 50000 yd. So **distance from VX-001 buys nothing** — the cone outreaches the room, and only bearing
matters.

`Spell.cpp` maps that target type to a **104° cone** (±52° through `HasInArc`, which compares
`arc/2`). Guides describe retail's 30° visual.

**Check `acore_world.spell_cone` before trusting the target type anywhere else here.**
`Spell::SelectImplicitConeTargets` reads `sSpellMgr->GetSpellCone(id)` **first** and only falls back
to the target-type switch when there is no row. There are rows for this boss, and one of them is
real: Rapid Burst and Hand Pulse are **60**, confirmed by measurement below. The row for 63293 says
10, which the traces do **not** bear out — and they cannot settle it either, because the beam bearing
leads VX-001's visible facing by up to the 42.6° that 33576 travels during Spinning Up, so every
measured hit angle is against the wrong reference. `ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE` stays at 52°
until something measures the beam rather than the boss.

Aim comes from `FaceBarrageArc`: VX-001 is repointed at NPC 33576 every tick of the aura, and 33576
laps the room on a fixed spline every 34016 ms — 10.6°/s, clockwise, ~106° over the 10 s barrage.
Cadence is 60 s. **NPC 33576 is spawned by world DB update `2026_08_10_00.sql`; without it
`FaceBarrageArc` returns early and the cone never moves.**

### Spinning Up is a channel on VX-001, and no aura at all

63414 puts **nothing on VX-001**. Effect 0 lands on 33576 and effect 1 on the MK II, both by
`conditions` row; effect 2 has no effect. On VX-001 it exists only as a **4 s channel**
(`SPELL_ATTR1_IS_CHANNELED`, duration index 35), which is why the encounter script polls
`FindCurrentSpellBySpellId`. So `HasAura(SPELL_SPINNING_UP)` never answers true — and for a release both
the window and the trigger asked exactly that. The raid's first barrage decision landed **0.1 s after
the beams were already live**, and one kill lost 12 bots to a mechanic that telegraphs for 4.16 s. Test
the channel, and read `GetCastTimeRemaining` for the time left, clamped at zero: the timer goes negative
on the pass that ends the channel.

### The cone ignites where 33576 will be, not where the boss points now

Since core `8f68451bb` the boss re-faces every 400 ms for the whole channel, so its facing is live
rather than stale. Prediction is still the point: 33576 travels **42.6°** during the windup, so a bot
deciding now has to aim at the bearing the cone will ignite on, not the one it can see. At **20000
damage every 250 ms**, being 42.6° out is fatal.

`GetMimironBarrageWindow` reads the channel timer while spinning up and the barrage aura's own duration
once firing, then rotates 33576's live position clockwise about the room centre to get the bearing at
ignition and at the last tick. Everything is recomputed each tick, so a moving apex, a rotating chassis
and a bot joining mid-cast all fall out for free, and every bot still derives the same cone without
coordinating because every input is world state.

The danger band is the sweep plus a clearance at each end — 106° plus 2 × (52° + 15°) ≈ **240°** at the
room centre, wider off it — so barely a third of the room is safe. Measure it clockwise from the
ignition bearing and never fold it to a signed angle: folded, part of the band lands past π on the wrong
side, which is how the far side of the room used to report safe while the beams swept across it.

### The orbit, measured rather than assumed

Path 13395 is 19 waypoints at velocity **20.8988**, perimeter **707.0 yd** — a **33.8 s** lap, so
**10.64 °/s** and **106.4°** per barrage, bearings strictly decreasing. A least-squares circle fits it
at centre (2741.98, 2569.36), r = 113.2, which is **2.7 yd** from `ULDUAR_MIMIRON_ROOM_CENTER` — close
enough to orbit about the room centre and stay within a couple of degrees over a whole cast.

**That rate only holds for a VX-001 standing at the room centre.** The bearing rate seen from an
off-centre observer is not constant: at 15 yd it swings 9.2–12.5 °/s, at 30 yd **8.3–14.7 °/s**. Phase
4 rides the chassis that far out, so a fixed 10.6 °/s is off by up to 40° over one barrage. Predicting
33576's actual position removes the error instead of budgeting for it.

### The danger band cannot be folded to (−π, π]

Sweep plus two clearances is **240°** at the room centre and wider off it. A signed fold puts part of
that band on the far side of π, which is precisely how the opposite side of the room came back "safe"
while the beams crossed it. The dodge works in a clockwise-from-centreline measure in `[0, 2π)`
instead, where the band is one contiguous interval and there is nothing to wrap.

The rest of the dodge is unchanged in shape:

- it is **selective** — bots already outside the swept union never move and keep casting;
- it rotates at **constant radius**, since radius is irrelevant to safety and melee keep their uptime;
- it picks direction on **time spent inside the cone**, not distance travelled.

That last one is the part that is easy to get wrong, and distance is the wrong currency: the short way
round is frequently straight through 104° of beam. Three cases, on which side of the centreline the
bot sits:

| Where the bot is | Direction | Why |
|---|---|---|
| More than 52° clockwise of the centreline | **clockwise** | The beams have not reached it. Running clockwise keeps it that way at zero cost; turning back walks it into a cone it is currently in front of. |
| Inside the cone, leading side | whichever edge is **sooner** | Counter-clockwise rides the sweep out through the trailing edge at `turnRate + rate`; clockwise pushes out through the leading edge against it at `turnRate − rate`. Crossover lands near 30° off the centreline. |
| Counter-clockwise of the centreline | **counter-clockwise** | The sweep is already carrying it clear; it is out in a fraction of a second. |

**During Spinning Up neither direction gets help from the sweep.** The boss aims once and holds for
four seconds, so the band is fixed in world space and both edges close at plain `turnRate`. Getting
that wrong is not cosmetic: it made a bot 25° inside the ignition cone at 24 yd pick the far edge —
92° of travel against the 67° its four seconds actually bought — and it was still in the cone when the
beams lit. With the rate corrected it leaves by the near edge in 1.6 s and is ahead of the sweep for
the rest of the cast.

A simulation over the real waypoint path — every combination of DB Target phase, bot bearing, orbit
radius 14–24 yd and chassis offset out to 30 yd in eight directions — clears **62,208 of 62,208
positions**, with the committed legs and the movement lock modelled rather than assuming the bot can
correct continuously. The same harness scores the previous model at 9.3 % of bots told to stand still
while the cone crossed them, before counting the ones it sent the wrong way.

**The step size is not a safety dial.** Re-running that sweep at 40°, 30°, 20°, 15° and 10° gives zero
hits at every one of them, and *smaller steps are worse* under a moving apex, not better. Every failure
the harness can produce needs the apex to be dragged around underneath the raid — at 2 yd/s of drift
0.53 % of positions are caught, and widening the margin from 15° to 30° only takes that to 0.34 % and
then plateaus. It is not a clearance problem and cannot be tuned away.

What prevents it is the apex holding still, which is why the phase 4 main tank's tighter floor is
load-bearing for the whole raid rather than a tank convenience. `Unit::GetMeleeRange` is
`ownerReach + targetReach + 4/3` — 8 + 1.5 + 1.33 = **10.83 yd** — and `ChaseMovementGenerator`
leaves the chassis alone inside that. The tank orbits at reach + 1.5 = **9.5 yd**, so it never triggers
a chase and the cone apex stays where it is.

The margin is **15°**, up from 12. The cone turns 2.7° per damage tick, so 12° was about one bot
reaction interval with nothing spare. 15° still leaves a 120° safe wedge for a 25-man raid at 22 yd,
and the action drops to a 100 ms recheck while it is actually relocating.

The 24 yd cap on the arc spread is what makes this work: it keeps the worst-case 52° rotation inside
the 4 s window with 1.6× speed margin.

**Without NPC 33576 the cone is frozen, not sweeping.** `FaceBarrageArc` returns early when it cannot
find the target, so the core never re-aims at all. The fallback reports `sweep = 0` and a static
±clearance wedge around VX-001's current facing — running a sweep that is not happening would walk the
raid straight through the beams.

**Rotate in bounded steps, and never below a floor radius.** Creatures are absent from the navmesh,
so `MoveTo` will happily draw a chord straight through VX-001 — and a chord across the apex crosses
every bearing the cone covers, which is a guaranteed hit. The dodge therefore issues one leg of at
most 40° per tick (a 40° chord stays within 6% of the ring) and clamps the radius to
`[combat reach + 6, 24]`. VX-001's combat reach is **8**, so the floor is 14 yd: melee sit inside
that and would otherwise try to orbit through the model. `MOVEMENT_FORCED` sequences the legs for
free — `IsWaitingForLastMove` refuses anything not strictly above the move already in flight.

## Which dodges interrupt the cast, and which keep it

A bot mid-cast cannot be moved at all, and `MoveTo` still reports success — the mechanism is in
[../../engine/pitfalls.md](../../engine/pitfalls.md). A balance druid died to Rocket Strike this way.
It is **5,000,000 damage in 3 yd** (63041, radius idx 15), so one occurrence is one death.

The dodges that kill outright call `botAI->InterruptSpell()` before moving — Laser Barrage, Rocket
Strike, Shock Blast, and the Firefighter flames and Frost Bomb. The ones that do not kill keep their
cast: Proximity Mine is **9,000** and a Bomb Bot **12,000** (63009), both healable, and clipping a
cast every time a mine lands costs more than the mine does. Every other raid already did this;
Mimiron did not.

## Raid nodes must resolve bosses by entry, not by threat

`AI_VALUE2(Unit*, "find target", "<name>")` walks `bot->GetThreatMgr().GetThreatenedByMeList()`
(`TargetValue.cpp:159-184`) — it only ever resolves a boss that already has **this bot** on its threat
list. That is fine for a boss which calls `DoZoneInCombat`, and quietly fatal for one that does not.

VX-001's phase 4 `SetData` calls neither `DoZoneInCombat()` nor `AttackStart()`, and its `AttackStart`
is a no-op override. So a bot that never damaged VX-001 — a healer, a melee locked on the chassis —
got `nullptr` and **the Laser Barrage dodge never ran for it at all**. Every Mimiron node now uses
`GetFirstAliveUnitByEntry`, which reads the grid-swept `"possible targets no los"` and has no threat
dependency. The same defect is still live across a dozen other instance strategies.

## The two adds need opposite answers

**Proximity Mine (34362)** carries `unit_flags = 2` (`UNIT_FLAG_NON_ATTACKABLE`): there is no
legitimate way to remove one. It arms 2.5 s after landing, then polls every 500 ms for a player
inside **1.9 yd**, blasts 3 yd (66351), and self-detonates at 35 s. Ten land 8 s after every Shock
Blast. Avoidance is the whole answer — and because mines are non-selectable they never reach
`"possible targets"`, so pathing is blind to them. `IsMimironSpotMineSafe` gates the destination of
every Mimiron move **except the barrage dodge**, where the cone kills instantly and a mine does not.

**Avoid tightly, and rank it last.** Ten mines inside a 15 yd circle leave no clear ground if the
avoid radius is wide, so the node stays permanently active and the raid paces. Trigger at 3.0 yd
(the 1.9 yd poll plus slop), clear to 3.5 yd, cap the step at 5 yd, and take the *nearest* clear
spot — the generic `MoveAwayFromCreatureAction` maximises min-distance over a fan reaching 30 yd and
picks a different answer every tick, which is what produced both the long runs and the oscillation.
The node sits at `ACTION_RAID - 1`, below every other Mimiron node: Shock Blast is **100000 damage
in a 15 yd circle** (63631, radius idx 18) and a mine is a healable 3 yd, so a mine must never cost
the raid a dodge, a taunt or a core delivery. Some mine hits are the intended price.

**Melee do not dodge mines at all, and nobody dodges them during a barrage.** The ten mines scatter
inside 15 yd of the MK II (65347, radius idx 18) every 30 s, which is precisely where melee have to
stand, so the node fired more or less continuously. Work out what it was buying: 10 mines of 3 yd blast
inside a 707 yd² circle is about 40 % coverage, so a stationary melee expects ~0.4 hits a cycle —
**roughly 120 dps**, against the whole of its uptime and, in phase 4, the barrage deaths that come from
being nudged onto a cone-blind bearing. Ranged keep the sidestep; every healer spec is `IsRanged`
(`PlayerbotAI::IsRanged` reads `STRATEGY_TYPE_RANGED`), so "melee" here means melee DPS plus warrior,
DK, protection paladin and feral tanks. The barrage gate is a suppression rather than a filter,
because a mine is survivable and the cone is not.

**Bomb Bot (33836)** is the opposite: `speed_run` 1.14286 works out at **8.0 yd/s against a player's
7.0**, so it is not merely un-outrunnable, it gains on you, and it detonates on melee contact
(`SMART_EVENT_DAMAGED_TARGET` → 63801, base 23562, 5 yd; **66406 hp** in 25-man). `HealthModifier` is
1.5873, so ranged kill one in a few
globals — it sits at the top of the ranged priority list.

**The chase target decides who shoots it.** The bot it is chasing cannot leave, so that bot keeps
shooting at any range; everyone else inside the blast steps out, which is all a 5 yd blast is worth, and
healers and melee always sidestep. Both rules read `GetMimironBombBotChasing`. They used to split on
distance and each deferred to the other — the trigger stood a ranged DPS down for any Bomb Bot inside
`SpellDistance`, while `IsAllowedTarget` dropped one closer than 8 yd — so inside 8 yd a ranged bot
neither shot nor moved, and one kill lost two of them at full health standing on one. Out-of-range Bomb
Bots are still filtered out of `BuildPriorityList` entirely — targeting one the bot cannot reach
abandons the mech for an add somebody else can hit, and lands the bot in the `reach spell` versus
`ACTION_RAID` deadlock below.

**And it is the one Mimiron add that takes a snare.** Its immunity set is **−263**, which leaves
`SNARE`, `ROOT`, `STUN`, `FREEZE`, `GRIP` and `KNOCKOUT` off — every one of which the Assault Bot's
**−285** carries. At 20,000 HP and 8.0 yd/s, a second of extra approach is most of a cast.
`mimiron slow bomb bot` puts the class snare on whichever Bomb Bot the bot is already shooting:
hunter `concussive shot`, shaman `frost shock`, warlock `curse of exhaustion`. Roots are deliberately
absent — Entangling Roots and Frost Nova break on the first hit, and hitting it is the plan.

Two gates keep it from being a waste of a global. It reads `"current target"` rather than scanning, so
it can only fire on a Bomb Bot `mimiron set dps priority` already handed the bot — which also means
healers never snare. And the Bomb Bot has to be more than **15 yd from its own victim**, not from the
caster: measured from the caster, a hunter 25 yd away would happily snare one that is already two
yards from a healer. `ServerFacade::GetChaseTarget` answers that, falling back to the caster's own
distance before the add has picked anyone.

Both used to be handled by a main-tank `unit->Kill()` gated on `BotCheatMask::raid`. That is gone.

## Rapid Burst is a 60° cone, and it can be sidestepped

`EVENT_SPELL_RAPID_BURST` picks a random player within 80 yd every **3.2 s**, casts **63382 on that
player**, then `SetFacingToObject`. 63382 is a 3000 ms aura with a 500 ms periodic dummy — **six
ticks** — and each one fires 64531/64532 from VX-001 along the facing it was pointed in when the aura
landed. VX-001 does not move, so the centreline is exactly `VX-001 → the aura carrier`, fixed for the
whole 3 s. **Read the carrier, not the boss's orientation**, which is only the last thing the server
happened to write. Phase 2 only:
`_events.RescheduleEvent((_phase == 2 ? EVENT_SPELL_RAPID_BURST : EVENT_HAND_PULSE), 14.5s)`.

**60° is measured, not read off a table.** Bucketing every living bot past 14 yd — far enough that
`IsWithinBoundaryRadius` cannot short-circuit the cone test — by its bearing off VX-001 at the tick
before each hit, the hit rate holds at **81%, 83%, 74%** across 0-10°, 10-20° and 20-30° and then
falls off a cliff: **9%** at 30-40°, 3-7% to 60°, ~1% beyond. The residue is snapshot staleness.
Unlike the barrage there is no aim lead to confound it: the boss faces its target once and holds for
the whole 3 s.

The escape is therefore short. 93-97% of hits land inside ±30°, and the arc a victim had to cover to
clear it was **median 6.3-6.7 yd** — 46% under 6, 65% under 9, p90 17.
A ~1 s step saves four of the six ticks. `MimironRapidBurstAction` takes it whenever the arc is at or
under `ULDUAR_MIMIRON_RAPID_BURST_MAX_STEP` (9) and stands still above that, because past there the
boss has re-aimed at somebody else before the bot arrives.

Three things stop it costing more than it buys. It **keeps the bot's own radius**, moving purely
tangentially, so casting range and melee range both survive and neither `reach spell` nor
`reach melee` fires afterwards. The **arc spread yields on the slot**, not on the trigger — testing
the trigger would hand the bot back the instant the step worked, for the remaining ticks. And the
shared fan **screens for the cone**, so no other dodge can sweep a bearing into it.

It sits at `ACTION_RAID + 8`, top of the ladder, and that costs nothing: Rapid Burst exists only in
phase 2, so it never contends with the barrage, and the two lethal nodes it outranks there — Frost
Bomb on a 10 s fuse, Rocket Strike on 5 — both have seconds a 3 s cone does not. It is **not** behind
the hard-mode check, because Rapid Burst is scheduled unconditionally when phase 2 starts.

**Hand Pulse (64348/64352) is the same 60° cone**, every 1.75 s in phase 4, and is not covered — no
traced pull has reached phase 4.

The ring survives the correction. Six fixed phase-2 spots used to stack the raid into three clumps,
which is the worst shape against anything conical whatever its width; a ring of radius 22 with one
index-derived slot per ranged bot is not. The bearing is never keyed off VX-001's facing.

**The ring centres on the mech, not the room.** Bots cast out to `AiPlayerbot.SpellDistance` — 28.5
here, with no `AC_` override — so a ring pinned to the room centre puts the far half of the raid out
of range after about six yards of boss drift. It does not recover on its own either: `reach spell` is
`ACTION_HIGH` (20) and the ring is `ACTION_RAID` (60), so the ring wins every tick and walks the bot
straight back out. That deadlock is why ranged simply stop attacking rather than visibly struggling.

**Phase 1 drift has a cause worth fixing at the source.** The tank is melee, so the Shock Blast
trigger fires for it too: it runs, the MK II follows, and nothing brings either back. Over a five
minute phase that walks the fight around the room. The main tank now gets `ULDUAR_MIMIRON_ROOM_CENTER`
as its phase 1 slot — the same point `boss_mimiron.cpp` charges the MK II to — and drags the boss home
after every cast. Its slot is exempt from `IsMimironSpotSafe` (`IsMimironTankAnchorSlot`), because the
MK II parks on top of the mine field it just laid and a tank that refuses to stand in one never
returns.

**Recovery is a rigid translation, never a per-slot clamp.** When the outermost slot falls outside
casting range the whole anchor slides toward the mech by the excess; clamping slots one at a time
would deform the ring into a lopsided blob leaning at the boss, which is the clump shape the spread
exists to prevent. Every input (anchor, focus, radius, count) is group-global and the config distance
is read directly rather than through `PlayerbotAI::GetRange`, so each bot derives the same anchor
without coordinating. A second clamp keeps the anchor within 40 yd of the room centre — navprobe puts
the walkable floor at 16/16 headings out to 40, flat at Z 364.31.

## Phase 2 is a healing race, and Heat Wave is why

VX-001 casts **64533 on itself every 10 s** in phase 2 — implicit targets 22/15, radius index 28 =
**50000 yd**. Raid-wide, unavoidable, no bot-side answer, and the largest single source in both
2026-09-08 pulls at **936,259 (24.4%) and 750,071 (22.9%)**. Do not re-investigate it.

The arithmetic it forces: phase 2 takes **29,207/s and 20,366/s** against **21,389/s and 14,703/s** of
effective healing — a 617-679k deficit across the phase, about **1,600 damage per living bot per
second**. Overheal there is 22-27% where phase 1 runs 68-72%, so there is no slack to spend. Nothing
one-shots any more; the raid runs out of health from 3:20. Fire and Rapid Burst are the only
avoidable slices left in that budget, and every other decision on this boss is drawn against it.

**A clean arena buys the opening of phase 2, not the phase.** Mimiron seeds every 30 s throughout, so
the field rebuilds around wherever the raid is standing: attributing phase-2 flame damage to the most
recent batch, **85% and 53% of it followed one seeded inside phase 2 itself**. What the phase-1 west
anchor buys is a raid arriving on empty ground with a dodge ladder that has somewhere to go — which
is when it is at full strength, and when both 2026-09-09 pulls began dying, 15 s in.

## Phase 3 wants a wedge, not a ring

The add summon pads (GO 194740-194748) sit on **three arms** leaving the room centre at 180°, +59.4°
and -59.4°, each carrying pads at roughly 17, 29 and 40 yd. A ring therefore parks lone ranged bots
directly in an add's path. The raid stages in the **east wedge** instead — the one direction nothing
walks down — centred on the bearing to `ULDUAR_MIMIRON_PHASE3_STAGE` (2762.65, 2569.46, 364.31;
navprobe 16/16 on mesh with a 12 yd fan around it). Ranged and healers fill outward from 18 yd.

**Melee get no slot at all, and neither do the tanks.** They used to take inner rows at 8 and 14 yd,
which was the same deadlock as the phase 1 ranged ring, one relevance band down: every add walks in
from a pad at 17, 29 or 40 yd, `mimiron set dps priority` hands a melee bot one of them, `reach melee`
starts the chase at `ACTION_HIGH` (20), and the formation drags it back at `ACTION_RAID` (60). The
formation wins every tick, so the bot never lands a swing. Any fixed melee slot on this phase is a spot
the target is not in. Melee stand on whatever they are hitting.

That also removes the last reason to add a `disperse distance` back: melee are free-moving now, and a
disperse would be one more node fighting the chase for the same tick. Bomb Bots are the only phase 3
mechanic that punishes stacking, and melee already have a node for those.

The wedge is **built to fit casting range** rather than grown until it stops fitting: rows step by 6 yd
(one more than a Bomb Bot blast) and stop at `SpellDistance` minus the margin, and once the band is
full the remaining rows pack tighter. Spacing is the thing to give up, not range — a Bomb Bot catching
two bots is cheaper than half the raid unable to cast. At ±60° a 17-strong ranged group still holds
4.7 yd of separation. The phase 3 `disperse distance` of 5.0 is gone with it: the slots already do
that job, and `CombatFormationMoveAction` shoving bots off slots the formation pulls them back onto is
pure thrash.

**The wedge is anchored on the room centre and never slides.** It used to slide toward the Aerial
Command Unit until the outermost row was inside casting range, which sounds harmless and was not.
`extent` was measured in every direction while the wedge only occupies 120° of one, so with
`SpellDistance` 28.5 (margin 4 → 24.5) and a two-row wedge of extent 24 the excess came out at
`dist − 0.5`: the anchor landed **half a yard from the boss** every tick, whatever the room centre
said. The excess was never clamped to the distance either, so it could overshoot and place the anchor
on the far side of the unit entirely.

That closed a loop. The unit uses `AttackStartCaster(who, 30.0f)`, so it holds **30 yd** from its
threat target — and its threat target is a ranged bot standing in the wedge the anchor is dragging
after it. Raid and boss then circle the room together, which is what "ranged oscillate instead of
doing damage" looks like from the floor.

Holding still is also the whole Bomb Bot fix. They spawn on the unit (`SPELL_SUMMON_BOMB_BOT` is cast
on self), so once the wedge stops chasing, the unit's own 30 yd standoff is what a Bomb Bot has to
cross: **~3.7 s** of free fire on a 20,000 HP add at 8.0 yd/s, against approximately none while the
raid was closing on it. Pushing `ULDUAR_MIMIRON_PHASE3_MIN_RADIUS` past 18 buys nothing here — the
unit keeps 30 yd from its victim wherever that victim stands.

Nothing is lost by not sliding, because **the phase 3 Aerial Command Unit has no attack**. Its entire
event list is `EVENT_SUMMON_{BOMB,ASSAULT,JUNK}_BOT` plus the Firefighter fire bots; Plasma Ball is
scheduled only in phase 4. Range on it matters for exactly one thing, the Magnetic Core window, and
melee and pets cover that on foot.

**Under Firefighter every live phase wants the wedge**, for an unrelated reason: fire. A 360° ring
puts 25 bots on 25 bearings, and since each chain grows toward whoever is nearest its own head, that
drags 25 chains outward along 25 radii until the fire is everywhere. Grouped into one sector the
chains converge instead, the 4 yd freeze rule actually bites, and the Frost Bomb — which only ever
summons on a burning node — lands in that sector and clears it. So while hard mode is on, the `ring`
branch becomes `hmwedge` and reuses this machinery unchanged: `MimironWedgeRows` and
`MimironWedgeSlot` at the same 18 yd first row and 6 yd spacing, on the same east centreline, still
anchored on the mech so casting range holds. It cost less in Rapid Burst than expected: the cone
caught **20-30% of the living raid per tick under the wedge against 22-25% under the ring**, which is
the same number. The first reading of it — 1 victim per tick rising to 3 — was measuring a raid the
Frost Bomb had already cut to 7-10 alive, and is the reason to normalise anything per-tick by the
living count. `ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE` is the knob if the fire still fans out.

**What it did cost was spacing, and that collided with the generic unstacker.** `rangedDepth` is
`SpellDistance(28.5) − margin(4) − 18 = 6.5`, so `MimironWedgeRows` gets two rows: 14 ranged in rows
of seven, 6.0 yd apart radially and 6.3 along the inner one.
`MimironPhase1PositioningAction` set `disperse distance` to exactly **6.0**, so every bot that reached
its slot was immediately judged too close by `CombatFormationMoveAction` and shoved 5 yd off it,
and the arc spread walked it back. `CombatFormationMoveAction::Execute` **always returns false**, so
it never claims the tick — it moves the bot underneath whatever else runs, which is why this was
invisible in the verdict stream. Accepted unstack moves went **51 → 261** a pull, ranged went from
26% of phase 1 moving to **42%**, 1.8 to 2.9 yd/s, 56 direction reversals to 180, and lost **13-17% of
their output**; phase 1 ran 15 s longer.

Two halves fix it. `ULDUAR_MIMIRON_DISPERSE_DISTANCE` is **5.5**, in the gap between Napalm Shell's
5 yd splash and the wedge's 6.0: still unstacks anyone genuinely inside Napalm range, never fires on
two bots both on their slots. And `MimironFormationGuardMultiplier` zeroes `combat formation move`
while the bot is within `ULDUAR_MIMIRON_SPREAD_TOLERANCE` of its slot — the same window the arc spread
declines to act in, so inside it nothing moves the bot and outside it the formation owns the
correction. Match **by name**: `TankFaceAction` derives from `CombatFormationMoveAction` and does real
work. A bot with no slot keeps the unstacker untouched, which is every melee mid-phase — and melee
never had a disperse distance here anyway, since the phase 1 node is ranged-only and
`DisperseDistanceValue` defaults to -1.

**It shipped broken, and the failure is worth keeping.** `MimironPhase1PositioningTrigger` ends
`AI_VALUE(float, "disperse distance") != 6.0f`, and that literal was left behind, so the latch never
closed: the node returned `true` at `ACTION_RAID` every tick and the engine stops a pass there. For
the whole of phase 1 every ranged and healer bot — the trigger is `IsRanged`-only, which is why melee
were untouched — cast nothing whatever. Ranged output fell to **286 dps a bot from 3,487**, effective
healing to **1,422 HPS from 9,455**, mana never moved, and the only damage left was pets'. The
unstacker sits below `ACTION_RAID` too, so the raid packed to 4.8 yd and Napalm Shell took four bots
in three seconds. Both pulls wiped in phase 1.

**The invariant to keep:** `PHASE3_SPACING` > `DISPERSE_DISTANCE` > Napalm's 5 yd, and the trigger
compares `ULDUAR_MIMIRON_DISPERSE_DISTANCE` rather than a literal.

## A dodge that returns false hands the tick to Charge

Shock Blast (63631) is a **4 s cast**, `TARGET_SRC_CASTER`, 15 yd, 100000 damage, every 30 s. Four
seconds is a long time to leave the tick open, and `charge` and `intercept` sit at `ACTION_MOVE + 10`
= 40 with nothing above them once the dodge node yields. A gap-closer moves in a straight line and
consults nothing about the ground, so a warrior that finished its flee simply charged back under the
cast and died.

`MimironShockBlastAction` therefore returns **true** for the whole window, not just on the ticks where
it issued a move. It used to return false in two places: when the bot was already clear (so the flee
distance came out non-positive) and while a leg was in flight (the `MOVEMENT_FORCED` lock refuses a
second `MOVEMENT_FORCED` move, every bearing fails, and the `MoveAway` fallback issues at
`MOVEMENT_COMBAT` and is refused too). Once clear, only melee keep holding — they own the gap-closers
and have nothing to do at 18 yd anyway — while ranged and healers go back to work rather than lose
4 s in every 30. `MimironChargeGuardMultiplier` is the backstop: it zeroes every
`CastReachTargetSpellAction` (Charge, Intercept, both Feral Charges — the class has no other
subclasses) inside the five windows a hit actually kills through. Proximity Mines and Bomb Bots are
deliberately **not** among them; the mine node was demoted below the whole ladder because eating one
is healable, and letting it veto a charge would contradict its own ranking.

**`reach melee` is the same move without the spell, and it was missed.** `ReachMeleeAction` derives
from `ReachTargetAction : MovementAction`, not from `CastReachTargetSpellAction`, so the guard's
`dynamic_cast` walked straight past it. Melee get no formation slot mid-phase, so nothing else holds
their position either: the flames dodge threw a bot 12 yd clear at `ACTION_RAID + 4`, the movement
lock expired after that leg's travel time, the dodge trigger went quiet, and the tick fell to
`reach melee` at relevance **21**, which closed back onto ground the chains were crawling toward.
Melee spent **11% of phase 1 within 5 yd of the Mk II**, against 23-33% before the dodge worked at
all, and their median distance to it went 6.7-8.4 → 9.3-10.2. The guard now also matches the name
`reach melee` — **not** the `ReachTargetAction` base, which drags in `reach spell`,
`reach party member to heal` and `reach pull`; vetoing those strands ranged and healers out of range
in the window they most need to close.

**`GetDistance2d` versus `GetExactDist2d`, again.** `WorldObject::GetDistance2d(WorldObject*)`
subtracts *both* combat reaches, and the MK II's is 8. `20.0f - GetDistance2d(mk2)` therefore fled to
**29.5 yd centre to centre** against a 15 yd radius, and the trigger's `GetDistance2d(boss) < 15`
fired out to 24.5. Both now measure centre to centre against
`ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST` (18), which also puts the whole 22 yd ranged ring permanently
outside the mechanic. This is the second defect of this exact shape in this encounter — the barrage
radius was the first.

## The Magnetic Core needs a carrier that walks

The core mechanic never once fired in testing, for two independent reasons. The trigger required an
Assault Bot corpse **already within 5 yd** and the action had no travel branch, so unless the carrier
happened to be standing on a corpse it returned false forever. And the carrier came from
`IsMechanicTrackerBot`, which is the first alive bot in group order regardless of role — usually a
ranged bot pinned 22 yd out, which is never within loot range of anything.

`GetMimironCoreCarrier` picks the first alive non-tank melee bot in group order instead (still group
order, so every bot agrees without coordinating), the corpse search widened to 60 yd, and the action
walks to the corpse before looting it. Corpses last 25 s
(`TEMPSUMMON_CORPSE_TIMED_DESPAWN`), which is the entire window.

**What the core buys is the only killable window in the phase, and nothing was using it.** Aura 64436
runs **20 s**, carries `MOD_DAMAGE_PERCENT_TAKEN` at base 49 (**+50 % damage taken**), and its
`OnApply` runs `DO_DISABLE_AERIAL`: `CastStop`, `AttackStop`, `REACT_PASSIVE`, hover cleared,
`MoveFall`, and `_events.DelayEvents(25s)`. The unit's own `UpdateAI` returns early for the whole
aura, so **no adds spawn during it**. It carries neither `NOT_SELECTABLE` nor `NON_ATTACKABLE`, so it
is an ordinary target.

Melee and pets switch to it for the window — `IsAllowedTarget` used to refuse melee the Aerial Command
Unit outside phase 4 unconditionally, and the pet node only ever looked for adds, so both sat it out.
Ranged keep the add order and arrive on their own once the leftovers are dead, since nothing replaces
them. Tanks never reach this: `MimironSetDpsPriorityTrigger` stands down for them, so the Assault Bot
keeps its tank throughout, which is deliberate — it is the one add nobody can ignore, and a tank
contributes little of the burn.

**The core grounds the unit where the unit already is, and the main tank is what decides that.** 64444
summons by nearest entry conditioned on the Aerial Command Unit, so it lands under the unit and never
under the placer: in one trace both cores spawned **0.00 yd** from the unit's x/y while the carrier stood
12 yd away, and the unit then descended vertically. The tank decides it instead, because the unit hovers
directly over its threat target — and phase 3 gave the tank no slot, only a chase. After a Bomb Bot
sidestep pushed it 30 yd out, tank and unit converged **16.8 yd** off the room centre and stayed there
45 s, leaving 7 to 12 of 25 past casting range for both 20 s windows. `p3tank` pins the main tank to the
room centre, the same way `p1tank` does for the MK II and for the same reason.

## Pets need telling twice, in two different phases

`PetAttackAction`'s trigger node is commented out globally (`CombatStrategy.cpp:53-55`), so a pet keeps
whatever it last latched onto and no encounter redirects it unless it says so. Any fight with an
unreachable boss or a hard target switch has to do it by hand.

In phase 3 the Aerial Command Unit hovers 15 yd up and every ground pet parks underneath it
contributing nothing. `mimiron pet control` sends them to the nearest Assault Bot, then Junk Bot, then
Bomb Bot, and calls `StopPet` when no add is up; the Assault Bot comes first because it is the only
Magnetic Core source. Its action always returns **false** — it redirects the pet without consuming the
bot's own tick. Once a Magnetic Core lands, the unit itself outranks all of them: it is on the floor,
stationary and taking +50 % for 20 s, and nothing new spawns during it.

In phase 4 pets go where the melee go, and hold when the melee hold. A hunter's pet is about a fifth of
that hunter's damage, and leaking that into the rendezvous is exactly what the floor exists to prevent.
Note this is **not** a reachability workaround: the Aerial Command Unit is at ground level in phase 4
(see below), and pets are kept on the chassis parts to hold the ranged/melee split.

**`MOVEMENTFLAG_HOVER` cannot be used to mean "phase 3".** Nothing in the vehicle entry path clears it
and the phase 3 defeat branch does not either, so it survives into phase 4 whenever the ACU happened to
be airborne when it went down — a coin flip. Keying the node off it made it fire in phase 4, find no
adds, and call `StopPet`: every pet in the raid stopped for the whole phase. Both branches are gated on
the phase itself now.

## Phase 4 is a rendezvous, and percent is the only correct ordering

`EVENT_FINISH` fires only when **all three parts are casting Self Repair at once**, and Self Repair
(64383) has casting-time index 192 = **15000 ms**. That 15 s is the real mechanic: a part drops out at
`health < 15000` (damage zeroed, `UNIT_FLAG_NON_ATTACKABLE`, Self Repair cast), and if the other two
are not down before the cast completes, `SpellHit` brings it back aggressive and the phase resets.

So the three have to come down **level**, and the old ordering could not do it. It sorted on
`GetHealth()` descending, but `HealthModifier` is 300 / 300 / **200** — the Aerial Command Unit is the
smallest absolute pool in the room and so ranked last on every tick, whatever its actual percentage.
Ordering is on `GetHealthPct()` now, in **2 % bands**. Raw percent has no hysteresis and 25 bots burn two
parts down within a tenth of a percent of each other: one kill logged 1380 of 1564 target notes as a
VX-001/MK II flip, about five a second per bot, each resetting a swing or a cast. The band comes from
health alone, so the tank node, the DPS node and the pets still agree, and five fit inside the 10 %
floor.

**All three occupy the same point server-side.** VehicleSeat 3886 (VX-001 on the chassis) and 3806 (the
ACU on VX-001) both carry `AttachmentOffsetX/Y/Z = 0,0,0`, `Vehicle::AddPassenger` relocates by exactly
those offsets, and `RelocatePassengers` rewrites passengers to `vehiclePos + offset` on every chassis
move without re-applying hover height. The visual stack is a client-side model attachment. Two
consequences: melee cleave splashes every part, which is what the **10 % floor** is for; and the Aerial
Command Unit is genuinely reachable by melee in phase 4.

Melee are kept off it anyway, by choice. Ranged DPS own the ACU — `IsRangedDps`, not `IsRanged`, so a
healer is never steered onto it or into the hold — and with ranged at roughly half the raid against the
ACU's third of the health pool, that split lands close to even on its own. **Everyone holds at 10 %**
once nothing they are allowed to touch is above the floor: melee, pets, and both tanks. Tanks holding
is only safe because nothing else is generating threat by then, so threat is static and no mech changes
hands; it is the first thing to revisit if one ever does.

**Every restriction lifts the moment a part starts self-repairing.** `IsMimironPhase4` is keyed on
VX-001 riding the chassis, not on all three being attackable, precisely so it stays true through that
window — the phase is at its most time-critical there, not over. Melee join the ACU, the floor is gone,
and the raid pushes whatever is left.

`IsMimironPhase4` answers from cached lookups only: VX-001 or the ACU riding something covers all of
the phase bar the last seconds, and when only the chassis is left, seat 3 holds the cannon in phase 1
and VX-001 from phase 4 on. It is asked several times per bot per tick, so a grid sweep there would
cost the whole raid every phase.

## Plasma Blast is a cooldown check, not a tank swap

62997 is a 3 s cast, `TARGET_UNIT_TARGET_ENEMY`, aura 3 at 1000 ms period, base 16999 + 1 →
**17000/s for 6 s = 102000**, no stacking, every 22 s; the 25-man id is **64529**, base 24999 →
25000/s. The debuff expires 16 s before the next one, so it is a heal check, not a stacking debuff.
Measured windows open at t=27, 49, 72, 95 and 117 for **54k-119k** on a **43,407 HP** tank, troughing
him at **11-27%**; three of one pull's tank deaths were the tail of consecutive windows.

**The swap this used to answer with is gone.** It needed a second tank bot, and the taunt could never
be early enough anyway: the cannon casts at `me->GetVictim()` resolved **when the cast began**, so a
taunt during those 3 s owns nothing, and what makes a swap stick is taunt setting the taunter's
threat equal to the highest — which needs the boss well before the cast, not during it. The MK II is
fully tauntable, so that was never the obstacle: `flags_extra` 524289 is
`OBEYS_TAUNT_DIMINISHING_RETURNS | INSTANCE_BIND` with no `CREATURE_FLAG_EXTRA_NO_TAUNT`, and
`CreatureImmunitiesId` -361 masks 21 mechanics and effects 98/124/144/145 — knockback and pull, not
taunt. DR resets after 15 s against a 22 s cadence.

`MimironPlasmaBlastDefensiveTrigger` fires **while the cannon is casting** and one bot spends one big
defensive. A health threshold cannot do this: six ticks over five seconds means it notices around the
third. `NextTankDefensive` (`RaidTankDefensive.h`, shared with Obsidian Sanctum) picks the tank's
weakest ready cooldown, shortest first, and returns nothing while one is already running. A healer
steps in only when the tank has none — `pain suppression`, `guardian spirit`, `hand of sacrifice`,
never `hand of protection`, which sheds threat and hands the boss back. `ClaimMimironPlasmaWindow`
gives the window to the first claimant, so a Shield Wall and a Pain Suppression never land on the
same five seconds: either alone carries it and the spare is worth more 22 s later. The class nodes
are **not** held off their own reactive use — nothing measured yet says a button is wasted between
windows.

## The tank leaves with three seconds of threat and the boss does not follow

The 53 yd walk west only works if the MK II is actually his, and it was not. Across three 2026-09-10
pulls he engaged at 13-15 s, started walking at ~16 s, and the boss switched to a melee dps 2-3 s
later and **stopped** — one pull parked it at (2737.8, 2582.8), 15 yd off the room centre, while the
tank finished the walk alone 48 yd away. Time on the main tank: **65%, 26%, 20%**.

`IsMimironTankDragReady` holds the anchor back until he owns it: `GetVictim() == bot` **and** threat
at least `ULDUAR_MIMIRON_TANK_THREAT_LEAD` (1.3) times the highest **non-tank** threat, or
`_HOLD_MAX_MS` (10 s), whichever lands first. Until then he has no slot at all, so `reach melee` owns
him and he stands on the boss building it. It **latches**, raid-wide in `MimironFightState`: a
mid-phase dip must not restart the drag with the raid already spread out behind him. The timeout is
not optional — a human holding the boss, or a dead tank, would otherwise pin the fight at the pull
spot for the phase.

`MimironTankAnchorGuardMultiplier` then stops the yo-yo. `reach melee` is `ACTION_HIGH` against the
formation at `ACTION_RAID`, so the two traded him back and forth the whole way (8 `reach melee` moves
refused with `wait` in one phase 1). It zeroes `reach melee` only while the boss is his and the latch
is set — which is also when he does not need it, because the boss is following him — so losing aggro
lifts it and he can run back and taunt.

**Tricks of the Trade was making it worse.** The rogues cast it on the hardest-hitting melee at
13.5-15.5 s in all three pulls, and that exact bot pulled the boss 2-3 s later — Justice @16.2,
Obliteration @17.4, Justice @17.8. `TricksOfTheTradeTargetValue::TankNeedsRedirect` has an opener
branch keyed on `"combat start time"`, which `PlayerbotAI::ChangeEngineOnCombat` only sets under the
`wait for attack` strategy, so it is always 0 in a raid; it then falls through to
`myThreat > tankThreat * 0.5`, false for a rogue who has not swung, and the buff goes to the top dps.
**That generic bug is still open.** Mimiron only suppresses the smart-target node in phase 1, through
`MimironGenericRedirectGuardMultiplier`.

`MimironRedirectThreatAction` owns the redirect instead, subclassing `RaidRedirectThreatAction` the
way Hodir and Freya do: the main tank in phase 1, `nullptr` after, because phases 2-4 split two mechs
across two tanks. `UldThreatRedirectMultiplier` keeps holding the class-generic on-main-tank nodes
for the whole encounter, which is why Misdirection was never cast at all.

## The phase handovers are a minute of wasted time

Measured from the boss script: **47.75 s** from phase 1 to 2 (retreat 5 → elevator 6 → VX-001 summoned
6 → 18 → 4 → 5 → 2 → 1.75), **24 s** to phase 3, **31.8 s** to phase 4. Nearly two minutes a pull.

A defeated mech sets `UNIT_FLAG_NOT_SELECTABLE` and stays in the world — the MK II parks 58 yd off
centre for phases 2 and 3 — and the next mech carries the same flag until its phase starts.
`AttackersValue::IsPossibleTarget` rejects that flag, so `GetFirstAliveUnitByEntry` is blind for the
whole handover and every Mimiron node stands down. With no trigger-driven action succeeding, the
engine falls through to `follow` at relevance **1.0**.

**That fallthrough is the answer, not the problem.** Formations here were tried twice — an 8 yd melee
ring and a 22 yd caster ring on the room centre, then a 44 yd rim lap the whole raid walked — and
both are gone. `GetMimironSpreadSlot` returns false for everyone while staging. The measurement that
settled it: across two handovers the human masters stood **43-54 yd from the room centre** on their
own, against the 49-52 the lap produced. A raid leader already puts the raid where the fire wants
taking, and chains grow **1.22 yd/s** and cannot catch a pack that is walking.

Parking *on the centre* is the thing to avoid, and it is what a formation did by accident and a
master will not: one pull holding those two rings there put 37 nodes in the window, **86% of them
inside 25 yd of the centre**, and took **203,272 damage across it, all of it fire**, with nothing
attackable.

**One exception.** `ULDUAR_MIMIRON_PHASE4_TANK_SPOT` is still handed out while staging. It is not
somewhere to wait — it holds VX-001's chassis still, and every phase-4 bearing, radius and offset is
calculated against a stationary cone apex. It is 1.4 yd off the room centre because all three
handovers converge there: VX-001 is summoned at it, `ACUSummonPos` is (2744.650, 2569.460, 380.0), a
defeated ACU is walked back to (2744.65, 2569.46, 381.34), and the chassis ends there after charging
to (2755.77, 2574.95) at 10 s. `GetMimironStagingFocus` survives only to tell a phase-4 handover from
the other two: `Creature::FindNearestCreature` is a grid check on entry, alive state and range with
**no selectability filter**, so it sees what the target list cannot.

Three things fall out rather than needing code. The **elevator knockback** 11 s into the first
handover needs no guard, because VX-001 is not summoned until 17 s. **Eating and drinking happen** —
instance strategies are added to **both** `BOT_STATE_COMBAT` and `BOT_STATE_NON_COMBAT`
(`PlayerbotAI.cpp:1793-1794`), so nothing at `ACTION_RAID` is holding the tick against them. And
there is **nothing to do before a pull or after a wipe**: the MK II is `NOT_SELECTABLE` until pulled,
and evade despawns VX-001 and the ACU outright.

## The ring slot is what kills the Rocket Strike dodge

Rocket Strike target selection **removes every player within 15 yd first** and picks from what is
left, so it lands on the ranged ring by design. The marker (34047) burns a **5 s fuse**, blasts 3 yd
and despawns at 6 s.

That makes fleeing the easy half. The hard half is not walking back: once the bot has stepped clear,
the arc spread sees it off its slot and returns it — to the slot the marker is still sitting on.
`IsMimironSpotSafe` therefore tests the **slot**, not the bot, and holds while any live marker is
within 8 yd of it. Testing the bot's own surroundings is exactly the check that fails, because a bot
that dodged successfully is by definition clear.

Second, quieter trap: the flee and the arc spread both issued at `MOVEMENT_COMBAT`, and
`IsWaitingForLastMove` only lets a move through when its priority is **strictly** above the one in
flight. A dodge starting mid-walk was dropped with no trace. Shock Blast and Rocket Strike now issue
at `MOVEMENT_FORCED`, as do the Frost Bomb, Rapid Burst and — since it turned out to be losing every
contested tick to the formation — the flames dodge. Only the genuinely low-stakes avoids, mines and
bomb bots, stay at `MOVEMENT_COMBAT`, where they cannot stomp a real emergency.

**Two `MOVEMENT_FORCED` dodges in one encounter deadlock each other**, and there is no band above
`MOVEMENT_FORCED` to escape into. The barrage dodge returns `false` for a bot that is already clear, so
Rocket Strike and Shock Blast get the tick and flee on a bearing derived purely from the hazard they
are escaping. That leg then holds the lock — about 1.4 s for a 10 yd rocket step, 2.6 s for an 18 yd
Shock Blast flee, against `MaxWaitForMove` of 5000 — and *strictly above* means the barrage dodge's own
`MOVEMENT_FORCED` move is refused for its whole duration. The barrage action ignores `MoveTo`'s return
value and keeps holding the tick, so the bot stands in the beams at 20000 per 250 ms until the lock
expires.

The fix is to make the lower dodge cone-aware rather than to try to outrank it. `MoveAwayClearOfMines`
now rejects any bearing whose destination is inside the swept union, and it asks that question **for
the moment the leg lands**, not for the moment it is issued: the cone turns ~10.6 °/s, so a 2.6 s leg
outruns the 15° margin and a spot that clears by exactly the margin is inside the beams on arrival. The
fan also widened from ±90° to ±112.5° — two stacked filters can empty the first quadrant — with a
guard that the destination must be strictly further from the hazard than the bot already is, because
past roughly 120° off the escape bearing the geometry turns back inward.

`IsMimironSpotSafe` covers Firefighter's ground fire on the same argument, behind the hard-mode check:
without it the flames node at `ACTION_RAID + 4` pushes a bot out of a burning slot and the formation at
`ACTION_RAID` pulls it straight back, and it paces on the edge until it burns down. It covers the
Frost Bomb for the mirror reason — otherwise the formation walks the raid back inside the blast for
the whole ten second fuse. The fan screens both too, so a Shock Blast or barrage dodge no longer
lands in the fire it will have to leave again. Both come from one `GetMimironFirefighterHazards`
pass, because the fan tests eleven bearings and rescanning a 50-node field per bearing is not free.

## What a trace answers

Position, verdicts and movement commands come from the raid-agnostic streams. The Mimiron rows, under
`--notes mimiron.`:

| Key | Says |
|---|---|
| `phase` | 0 none, 1-4 the phase, 5 a handover. Per instance |
| `core` | The Magnetic Core window is open. Per instance |
| `carrier` | Who is fetching the core. Per instance |
| `corestep` | Where that carrier stopped: `no-acu`, `no-corpse`, `walk-corpse`, `loot`, `bags-full`, `walk-acu`, `blocked`, `use` |
| `slot` | Which formation shape answered — `p4tank`, `p3wedge`, `p3tank`, `p1tank`, `p1stack`, `hmwedge`, `ring`, `none` — with index/count and the point |
| `stack` | A phase 1 stack anchor switch, `<from>:<nodes> -> <to>:<nodes>`. Per instance |
| `plasma` | Which defensive answered the Plasma Blast window, or `covered`/`none` |
| `barrage` | Which dodge rule fired — `clear`, `hold`, or `ahead`/`inside`/`trailing` plus a direction — with the bearing clockwise of the centreline and the ring radius |
| `flee` | The bearing fan's outcome, `ok`/`fallback`/`none`/`locked`, and how many bearings each filter refused (`back`, `mine`, `cone`, `fire`, `bomb`, `burst`, `move`). `what` names the hazard: `shock`, `rocket`, `frostbomb`, `rapidburst`, and `flames+N` per ladder rung, so which rung won is readable. `locked` means the movement lock refused before a single bearing was tried |
| `dpsrule` | Which priority rule chose the target, `held:` when the hold kept it, `fallback`, or `p4hold` |

`flee` has no substitute: a refused bearing reaches no MotionMaster and so writes no `move` record,
which leaves a dodge that refuses all twelve completely silent.

The Laser Barrage cone is a `haz` `sweep` row every 250 ms — `lead`, `sweep`, `rate`, `live`,
originated on VX-001, which in phase 4 is the chassis, so a drifting apex shows. Written by hand
because the cone has no world object, and neither has the DB Target it aims at: that one is not
hostile, so the snapshot sweep skips it too.

Still invisible: **creature auras**, because `NoteAura` is roster-gated, so 64436 on the Aerial
Command Unit never appears and `mimiron.core` is the only record of the window; and **mines under
Firefighter**, because the sweep caps hostile creatures at 40 in grid order and fire nodes spread all
fight, so a Proximity Mine can drop out of the containment test a death is measured against.

