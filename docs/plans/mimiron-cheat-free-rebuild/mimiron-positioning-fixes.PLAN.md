# Mimiron: fix the three field-observed positioning failures

## Context

The cheat-free Mimiron rebuild (commit `b6c9a41f7`) is in and compiles. A live attempt exposed three
positioning defects that the static rewrite could not have caught:

1. **Bots dodge Proximity Mines far too eagerly and oscillate**, and the mine node steals the tick
   from the Shock Blast dodge, so bots die to Shock Blast. Eating a mine is healable; eating a Shock
   Blast is not.
2. **The Laser Barrage dodge walks bots straight through VX-001**, which crosses the live cone.
3. **Bots step out of a Rocket Strike and their positioning anchor immediately drags them back in.**

Goal: fix exactly these three, without reopening the cheat-free design.

---

## Verified facts behind each defect

| Fact | Source |
|---|---|
| A Proximity Mine polls `SelectTargetFromPlayerList(1.9f)` every 500 ms, arms 2.5 s after spawn, auto-detonates at 35 s | `boss_mimiron.cpp:1784-1825` |
| Mine blast 66351 → radius idx 15 = **3 yd** | `spellradius.reference.csv` |
| Mines land randomly within **15 yd** of the MK II (65347, radius idx 18), 10 per Shock Blast | `boss_mimiron.cpp:1142-1145` |
| Shock Blast 63631 = **100000 damage**, `TARGET_SRC_CASTER`, radius idx 18 = **15 yd**, cast-time idx 15 | `spell.reference.csv` |
| VX-001 combat reach = **8** (display 28841, `creature_model_info`); MK II also 8 | acore_world |
| Rocket Strike marker: **5 s fuse**, despawns at 6 s, blast 63041 radius idx 15 = **3 yd** | `boss_mimiron.cpp:1908-1913`, `:2394-2415` |
| Rocket target selection **removes everyone within 15 yd** first — it prefers the ranged ring | `boss_mimiron.cpp:1895-1900` |
| `IsWaitingForLastMove` returns false only when `priority > lastMove.priority`; **equal priority is refused** while the previous leg's lock lasts | `MovementActions.cpp:951-963` |
| `MoveTo` with `exact_waypoint=true` skips `SearchForBestPath` and calls `DoMovePoint` directly; creatures are not in the navmesh either way | `MovementActions.cpp:214-238` |
| `MoveAwayFromCreatureAction` scans 8 bearings × 3…30 yd and keeps the candidate **maximising min-distance-to-any-creature** | `MovementActions.cpp:2857-2911` |

### Root causes

**#1 Mines.** `MimironProximityMineTrigger` fires at `MINE_CLEARANCE + 1 = 6.0 yd` against a mine
that only trips at 1.9 yd. Ten mines inside a 15 yd circle means overlapping 6 yd bubbles with no
clear ground, so the node is permanently active. Its base class then picks the *global* safest point
out to 30 yd and re-picks a different one every tick — that is the oscillation. And the node sits at
`ACTION_RAID + 3` against shock blast's `+2`, so the engine ends the tick on the mine dodge and the
Shock Blast flee never runs.

**#2 Barrage.** The action issues one `MoveTo` to the *final* heading on the ring. The resulting
chord passes through or beside VX-001 — and the clockwise branch targets `arc − 170°`, a chord
straight across the apex. Crossing the apex crosses every bearing, including the live beam. Second,
`radius = min(dist, 24)` has no lower bound, so a melee bot at 9 yd tries to orbit inside a model
with combat reach 8.

**#3 Rocket Strike.** `MimironArcSpreadTrigger` yields only while the **bot** is within 10 yd of a
marker. After fleeing, the bot is outside that, the trigger goes live again, and it walks back to a
ring slot that still has a marker burning on it. Compounding it, `MimironFleeAction` issues at
`MOVEMENT_COMBAT` — the same priority arc spread uses — so a flee starting mid-walk is silently
refused by `IsWaitingForLastMove`.

---

## Work items

### F1 — Constants (`src/Ai/Raid/Uld/Util/UldBossHelper.h`)

```cpp
constexpr float ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS = 3.0f;   // 1.9 yd poll + pathing slop
constexpr float ULDUAR_MIMIRON_MINE_CLEARANCE      = 3.5f;   // was 5.0f; covers the 3 yd blast
constexpr float ULDUAR_MIMIRON_MINE_MAX_STEP       = 5.0f;
constexpr float ULDUAR_MIMIRON_ROCKET_CLEARANCE    = 8.0f;   // 3 yd blast + slot tolerance
constexpr float ULDUAR_MIMIRON_BARRAGE_STEP        = 40.0f * float(M_PI) / 180.0f;
constexpr float ULDUAR_MIMIRON_BARRAGE_RING_MARGIN = 6.0f;   // added to VX-001's combat reach
```

`ULDUAR_MIMIRON_MINE_CLEARANCE` dropping 5.0 → 3.5 also unblocks every other flee: at 5.0 the
destination filter refused nearly every bearing in a fresh field, so `MoveAwayClearOfMines` fell
through to the unfiltered `MoveAway`.

### F2 — Mine dodge: tight, bounded, non-oscillating

`UldTriggers_Mimiron.cpp:189-194` — trigger radius `MINE_CLEARANCE + 1.0f` → `MINE_TRIGGER_RADIUS`.

`UldActions_Mimiron.h:139-145` — reparent `MimironProximityMineAction` from
`MoveAwayFromCreatureAction` to `MimironFleeAction` and give it its own `Execute` / `isUseful`. It
takes the **shortest** step that clears every mine, not the safest point in the room:

```cpp
Unit* mine = nearest live NPC_PROXIMITY_MINE within ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS;
if (!mine)
    return false;
float const step = std::min(ULDUAR_MIMIRON_MINE_CLEARANCE + 1.0f - bot->GetExactDist2d(mine),
                            ULDUAR_MIMIRON_MINE_MAX_STEP);
return MoveAwayClearOfMines(mine, std::max(step, 2.0f), MovementPriority::MOVEMENT_COMBAT, false);
```

Bounded at ~4.5 yd, and the trigger goes false as soon as the bot is clear, so there is nothing left
to oscillate between.

Extend `MimironFleeAction::MoveAwayClearOfMines` (`UldActions_Mimiron.cpp:32-72`) with two
parameters: `MovementPriority priority` and `bool fallbackUnfiltered`. The existing unfiltered
`MoveAway` fallback is right for Shock Blast and Rocket Strike (both beat a mine) and wrong for the
mine dodge itself, which must not escape one mine into another — it passes `false` and gives up.

### F3 — Barrage: step around the ring, never across it

`UldActions_Mimiron.cpp:116-167`. Keep the latch, the wedge test and the CCW/CW time comparison
exactly as they are; change only how the destination is reached.

```cpp
float const minRing = boss->GetCombatReach() + ULDUAR_MIMIRON_BARRAGE_RING_MARGIN;   // 14 for VX-001
float const radius  = std::clamp(bot->GetDistance2d(boss), minRing,
                                 ULDUAR_MIMIRON_SPREAD_RADIUS_MAX);
...
// target is the chosen rest bearing, relative to the arc: `clearance` (CCW) or `cwRest` (CW).
float const remaining = target - delta;
if (std::fabs(remaining) * radius < 1.0f)
    return false;                               // effectively there already

// One bounded step per tick. A single chord to the far side cuts through VX-001, and crossing the
// apex crosses every bearing the cone covers. At 40 degrees the chord stays within 6% of the ring.
float const stepped = std::copysign(std::min(std::fabs(remaining), ULDUAR_MIMIRON_BARRAGE_STEP),
                                    remaining);
float const heading = Position::NormalizeOrientation(arc.angle + delta + stepped);
```

`MOVEMENT_FORCED` already serialises the legs: `IsWaitingForLastMove` refuses the next step at equal
priority until the previous one's lock expires, so the ticks produce a polyline around the ring
rather than a stutter.

The time comparison keeps using the **full** remaining arc, not the step — the branch decision must
account for the whole trip.

### F4 — Rocket-aware anchoring

New helper in `UldBossHelper.h/.cpp`, next to `IsMimironSpotMineSafe`:

```cpp
// A destination is only worth walking to if nothing lethal is already sitting on it. Mines are
// non-attackable and Rocket Strike markers burn a 5s fuse, so both are pure destination filters.
bool IsMimironSpotSafe(Player* bot, Position const& dest);
```

Mine clearance plus any live `NPC_ROCKET_STRIKE_N` within `ULDUAR_MIMIRON_ROCKET_CLEARANCE`, both
off the `"nearest npcs"` list (markers and mines are non-selectable, so neither reaches
`"possible targets"`).

- `MimironArcSpreadTrigger` (`UldTriggers_Mimiron.cpp:90-113`) — drop the
  `FindNearestCreature(NPC_ROCKET_STRIKE_N, 10.0f)` check on the **bot** and test the **slot** with
  `IsMimironSpotSafe` instead. A marker on the bot is the rocket node's job at `+4`; a marker on the
  slot is what drags it back.
- `MimironArcSpreadAction` (`UldActions_Mimiron.cpp:175-188`) — swap `IsMimironSpotMineSafe` for
  `IsMimironSpotSafe`.
- `MimironRocketStrikeAction` and `MimironShockBlastAction` — pass `MOVEMENT_FORCED` so an in-flight
  arc-spread leg cannot swallow the dodge.

Mines, bomb bots, flames and frost bombs stay at `MOVEMENT_COMBAT`.

### F5 — Priority ladder (`UldStrategy.cpp:417-479`)

| Relevance | Nodes | Change |
|---|---|---|
| `+5` | laser barrage | — |
| `+4` | rocket strike, dodge flames (HM), frost bomb (HM) | — |
| `+3` | shock blast | **was `+2`** |
| `+2` | bomb bot | — |
| `+1` | plasma blast, magnetic core | — |
| `+0` | arc spread, ACU, set dps priority, phase 4 mark dps, fire resistance | — |
| `-1` | proximity mine | **was `+3`** |

The mine node goes to `ACTION_RAID - 1`, strictly below every other Mimiron node and tied with
nothing, so it only takes a tick no other mechanic wants. It stays above `ACTION_DISPEL` (50) and
below the rest of the ladder, so a bot with nothing else to do still steps off a mine, but a mine
can never cost the raid a dodge, a taunt, a core delivery or a target switch. Bomb Bot avoidance
keeps `+2`: its blast is 5 yd and it chases at player run speed.

### F6 — Docs

`docs/raids/ulduar.md`, Mimiron section: record the mine's real 1.9 yd poll against the 3 yd blast
(and why a wide avoid radius is actively harmful), Shock Blast's 100000/15 yd, the rocket's 5 s fuse
plus its >15 yd targeting preference, and the barrage ring-stepping rule with the reason (creatures
are absent from the navmesh, so any chord across the apex crosses the beam).

Also mirror this plan to `docs/plans/mimiron-cheat-free-rebuild/mimiron-cheat-free-rebuild.PLAN.md`,
which currently holds only the first round.

---

## Files touched

- `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` — F1 constants, `IsMimironSpotSafe`
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.h` / `.cpp` — F2, F3, F4
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp` — F2 trigger radius, F4 slot test
- `src/Ai/Raid/Uld/UldStrategy.cpp` — F5
- `docs/raids/ulduar.md`, `docs/plans/mimiron-cheat-free-rebuild/…PLAN.md` — F6

No new triggers or actions, so none of the four wiring sites change.

## Verification

The module cannot be compiled from this checkout; the worldserver build is the only compile path.

1. Build, then pull Mimiron with the SQL update applied
   (`SELECT COUNT(*) FROM creature WHERE id=33576` must return 1 — the barrage cone does not rotate
   without it).
2. **Mines.** After a Shock Blast, bots inside the field take at most a short sidestep and then hold
   and keep casting. No bot walks a long arc away from a mine, and none paces back and forth. A bot
   already clear at 3.5 yd must not move at all. Expect some mine hits — at `ACTION_RAID - 1` the
   dodge yields to every other Mimiron node, which is the intent. What must not happen is a bot
   losing a Shock Blast, Rocket Strike or barrage dodge to it.
3. **Shock Blast.** With ten mines on the ground, everyone still clears 15 yd of the MK II before
   the cast lands. This is the regression the priority swap exists for — verify it *while* a field
   is up, not on a clean pull.
4. **Barrage.** Watch the paths during Spinning Up: bots must trace the ring, never cut across
   VX-001. Nobody passes within ~14 yd of it. Melee visibly step outward as they rotate. No
   "P3Wx2 Laser Barrage" entries in the combat log.
5. **Rocket Strike.** A ranged bot on the ring gets a marker, steps out, and **stays** out for the
   full 5 s fuse — no walk-back. Confirm the flee actually starts even when the bot was mid-walk to
   its slot when the marker landed.
6. Re-run the cheat-free acceptance grep: `HasCheat`, `TeleportTo` and `->Kill(` must return nothing
   across `UldTriggers_Mimiron.*` and `UldActions_Mimiron.*`.
7. Full clear on normal, then Firefighter against its 10 min berserk.
