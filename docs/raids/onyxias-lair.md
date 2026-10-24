# Onyxia's Lair — Onyxia (map 249)

Strategy key `onyxia`, files in `src/Ai/Raid/Ony/`, all of it in `Definition/OnyDefinition_Onyxia.cpp`
behind her boss-state gate (index 0). Cross-raid conventions are in [README.md](README.md). Verified
against `src/server/scripts/Kalimdor/OnyxiasLair/`, `acore_world` and navprobe. 10 and 25 share one
path; there is no heroic. 25-man creatures keep the 10-man entries (10184, 11262, 36561), so match
those only.

Coordinates: +X north, +Y west, entrance south. Ids, spots, radii, the 8 breath lanes and the 52
fissures live in `Util/OnyData.h`.

## Encounter

- **Phases** (`OnyxiaPhase`, derived each tick, traced as `onyxia.phase`): Ground (P1) → below 65%
  threat wiped, `REACT_PASSIVE`, walks to `(-64.5, -214.9)` (Liftoff) → takes off to z -60 (Air,
  flying or z > -75) → below 40% flies back and lands at `ONYXIA_LAND_SPOT`, threat wiped again on the
  way (Landing, passive) → aggressive again (Final, latched once Landing was seen; health ≤ 40% is the
  fallback). Final opens with Bellowing Roar 18431 and Eruption, Roar again at 15 s then every 22 s.
- **Frontal/rear**: Flame Breath 18435/68970 (frontal, 45 yd), Wing Buffet 18500/69293 (frontal,
  knockback), Cleave 68868 (front half, chains), Tail Sweep 68867/69286 (rear cone). Combat reach 18:
  melee range ≈ 20.8 yd, spell range to her ≈ 18 yd longer than nominal.
- **Phase 2**: per waypoint two Fireballs 18392/68926 (random player, 8 yd splash), then a step to a
  neighbour or a Deep Breath along that waypoint's fixed lane (`BREATH_LANES`) and a flight to the
  opposite one. Whelps: 40 from the two caves at lift-off and every 90 s. Lair Guard 36561 every 30 s
  at `ONYXIA_GUARD_SPAWN`: Blast Nova 68958, 15 yd, 5 s cast, interruptible. The guard summoned into
  the tunnel on the pull (x < -130) idles unless attacked: never target it.
- **Eggs**: GO 176511, 119 in the two side caves (z -96..-90). A player within ~6.9 yd 3D hatches one;
  they respawn. Read live from the grid, never hard-coded.
- **Taunts**: `flags_extra` 0x80000 `OBEYS_TAUNT_DIMINISHING_RETURNS`
  ([../engine/raid-mechanics-lessons.md](../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
  In one kill none of 22 taunts on her landed, not even the first of the pull: no 67485 on her, no
  victim turn. Cause not found; nothing here relies on taunting her.
- **Phase 3 whelps**: two at +10 s, then two every 30 s. **Eruption** 17731/69294 bursts 5 yd round
  every fissure on each Roar.

## Strategy

| Priority | Row | Who |
|---|---|---|
| EMERGENCY+6 | `onyxia dodge` (Mover, `MOVEMENT_FORCED`) | everyone: egg (8 yd 3D), live breath lane (20 yd, clear to 22), Blast Nova in its last 2 s within 17 yd (non-tanks), Fireball on the bot with a neighbour within 9.5 yd. One sweep clear of every live hazard; destination latched while it stays clear |
| RAID+7 | `onyxia main tank cooldown` | MT on Flame Breath or below 35%: `NextTankDefensive` (`onyxia.defensive`) |
| RAID+6 | `onyxia main tank` (Mover) | MT branch, traced `onyxia.tank`: pickup / taunt / recover / secure / drag / return / hold |
| RAID+5 | `onyxia anti fear` | Landing and Final |
| RAID+4 | `onyxia redirect threat` | Misdirection/Tricks on the MT in the first 10 s after touch-down, not on the pull; the generic redirect nodes, plain Tricks included, are blocked all fight (a self-targeted Tricks on a DPS warrior is what lost the MT aggro) |
| RAID+3 | `onyxia choose target` | the ladder below |
| RAID+2 | `onyxia ground slot` / `onyxia air spot` (Movers) | ranged and healer bots |
| MOVE+5 | `onyxia rear flank` (Mover) | melee DPS on grounded Onyxia |

**Main tank.** He tanks at `ONYXIA_TANK_SPOT (34.0, -212.5)` with his back to the north wall: bot
knockback runs `MoveKnockbackFrom` → `MovePositionToFirstCollision`, so the wall stops Wing Buffet.
The spot is on the floor, pathable from her home and the landing, and 7.1/7.5 yd clear of the nearest
fissures. She settles in melee range at `ONYXIA_BOSS_HOLD_SPOT (18.9, -213.5)` (refit from six holds,
navprobe z). Branches: Landing → wait at `ONYXIA_P3_PICKUP (-32, -205)`, in taunt range of the
landing; she is on someone else → taunt, or attack and let generic reach close if it is down; on him
→ **secure**: tank her where she is, generic reach and facing allowed, until he has held her 5 s with
1.25× the next threat on her list (or 10 s, so a human out-threating him can't pin him); latched for
the hold, cleared when she leaves him or goes up → walk to the spot (drag, or return after a
knockback) and hold facing her. The walk returns true only on the tick that issues it (re-issued
after 500 ms stalled or 2 s), so his threat buttons run between.
While he drags, waits or holds, generic movers, wander and avoid-aoe are blocked for him.

**Threat** (`onyxia threat hold`, Spell | Melee; latch `onyxia.threat`, written in the tick): MT and
healers free. Landing holds everyone (3). Ground/Final: hold (1) until her victim is the MT and has
been for 3 s (a bot MT: until secured), then cap (2) at 90% of his threat and release at 80%. Read
off `GetThreatMgr()`: the `threat` AI value is a uint8 and wraps. A dead MT is replaced by the living
tank highest on her list; no tank, no hold. Misdirection/Tricks auras free the bot. Only damage aimed
at her is zeroed. `onyxia taunt guard`: nobody but the MT taunts her.

**Burst** (`onyxia burst window`): Bloodlust/Heroism only in Final; all burst waits for the MT to
have held her 5 s on the ground, and with a human MT 15 s after touch-down. Free in the air, held while
she lands. `onyxia.lust` traces the lust window.

**Layout.** Ranged and healers own their movement (`OwnMovement`; Reach kept on the ground, dropped in
the air). Ground slots (`onyxia.slot`, once per pull by GUID): bearings ±102/114/126° off her facing,
radius 24 healers (spill to 30), 30 casters, 36 hunters, overflow from 42. Anchor
(`onyxia.anchor`): the hold spot facing north under a bot MT, so the raid is set before she arrives;
her live position under a human one, re-latched after 6 yd or 20°. Every slot goes through
`ValidateStaticFloorPoint` and is nudged off eggs and, from Landing on, fissures; a slot with no clean
nudge traces `onyxia.slotfail`. Arrive within 0.5 yd, re-trigger past 3.5. Melee hold a stable
per-bot angle of 100-120° on the side they stand on (kept within 20° past her tail, so
her turning never sends them across her nose), at 0.6 of melee range, nudged off fissures in Final;
the generic `rear flank` and formation movers are blocked for them. A flank walk whose point moved more
than 3 yd steps its booking down to `MOVEMENT_NORMAL` to re-aim: an equal-priority walk otherwise holds
the stale point up to 5 s, which cost two Cleave and Wing Buffet deaths
([movement lock](../engine/pitfalls.md#movement-that-silently-no-ops)).

**Phase 2 cells** (`onyxia.p2spot`): built once at lift-off per waypoint — every 9 yd along its lane,
±13.5/22.5/31.5 yd across — kept if on the floor (z -90.5..-82.5), 9.5 yd from every egg, 20 yd from
the guard spawn, within 49.5 yd 3D of the hover point and in static LOS. Lane-clear cells first, none
closer than 9.5 yd (Fireball). On each waypoint change bots take the nearest free cell in GUID order.
Her waypoint is her spline's destination once she has reached her first hover point (WP 5).

**Targeting** (`OwnTargeting` for DPS and tanks): ranged DPS always Onyxia. MT: Onyxia on the ground,
in the air guard → egg-clear whelp within 30 yd → Onyxia. Off-tanks: egg-clear whelp (sticky, loose
ones first) → Onyxia; in the air also the guard if no MT lives. Melee: Onyxia, in the air guard →
egg-clear whelp → Onyxia. Landing: Onyxia for all. Egg-clear means 12 yd from any live egg.

**Eggs.** Besides the dodge and the slot/cell filters, `onyxia egg guard` zeroes closers and
reach-heal onto anything within 12 yd of a live egg, and closers are blocked onto airborne Onyxia and
while a breath lane is live. Blink and Disengage are blocked all fight.

**Trace hazards** (tracker bot): a `lane` per Deep Breath, an 8 yd circle per live egg once per pull,
a 15 yd circle per Blast Nova. Reader: `tools/botobs/bosses/onyxia.py <trace>`; its tank
section counts secure stretches, aggro lost on a walk, and taunts on her that took.

## Open risks

- Taunts may not land on her (above): the MT wins her back by threat only.
- Lane-clear cells are scarce at some waypoints; the dodge is the guarantee.
- Pit exits are 65-73 yd paths that may pass eggs; prevention is primary.
- Tremor may not reach the MT from the slots in Final.
- The Final drag from the landing to the tank spot is ~98 yd; ranged at the hold-spot slots are out
  of range until she arrives.
