# Mimiron: phase 1 and phase 3 positioning, charge suppression, Bomb Bots, Magnetic Core, pets

## Context

The cheat-free Mimiron rebuild (`27fce75f9`) and the positioning round after it are in. A 25-man test
surfaced six more defects, all in phases 1 and 3. Fix exactly these; the cheat-free design does not
reopen (no `HasCheat`, no `TeleportTo`, no `->Kill(`).

1. **Warriors charge back into a live Shock Blast and die.**
2. **Ranged are pinned to a ring around the room centre**, so once the MK II drifts they fall out of
   spell range and can never recover.
3. **The phase 3 ring is the wrong shape** — the raid should group on one side, off the add lanes.
4. **Ranged in range of a Bomb Bot should kill it**, not run from it.
5. **The Magnetic Core is never delivered**, so phase 3 only ends when ranged grind the ACU down.
6. **Pets flail under the hovering Aerial Command Unit** instead of killing adds.

---

## Verified facts

| Fact | Source |
|---|---|
| Shock Blast 63631: cast-time idx 15 = **4000 ms**, `TARGET_SRC_CASTER`, radius idx 18 = **15 yd**, 100000 damage, repeats 30 s | `spell.reference.csv`, `spellcasttimes.reference.csv`, `spellradius.reference.csv`, `boss_mimiron.cpp:1137-1141` |
| `WorldObject::GetDistance2d(WorldObject*)` subtracts **both** combat reaches | `Object.cpp:1317-1321` |
| Leviathan MK II and VX-001 combat reach = **8**; ACU = 5 | `acore_world` `creature_model_info` |
| Effective `AiPlayerbot.SpellDistance` = **28.5**, `HealDistance` = 38.5, **no `AC_` override** | `docker exec ac-worldserver`, live `playerbots.conf:600` |
| `charge`/`intercept` run at `ACTION_MOVE + 10` = **40**; `reach spell` at `ACTION_HIGH` = **20**; every Mimiron node is `ACTION_RAID` = **60** or above | `TankWarriorStrategy.cpp:139`, `CombatStrategy.cpp:16`, `UldStrategy.cpp:425-486` |
| All four gap-closers (`CastChargeAction`, `CastInterceptAction`, `CastFeralChargeBearAction`, `CastFeralChargeCatAction`) derive from `CastReachTargetSpellAction`, and **nothing else does** | `WarriorActions.h:70,105`, `DruidBearActions.h:15`, `DruidCatActions.h:18`, `AiObject.h:382` |
| `IsWaitingForLastMove` returns false only when `priority > lastMove.priority`; equal priority is refused for the lock's duration | `MovementActions.cpp:951-963` |
| Phase 3 ACU uses `AttackStartCaster(who, 30.0f)` — it chases its threat target and **stops at 30 yd**, never backing off if approached | `boss_mimiron.cpp:1541-1546` |
| Bomb Bot 33836: run speed **1.14286** (8.0 yd/s vs a player's 7.0), health modifier **1.59**, explodes via 63801 — radius idx 8 = **5 yd** — on `SMART_EVENT_DAMAGED_TARGET`, then dies | `acore_world` `creature_template`, `smart_scripts`, `spell.reference.csv` |
| Phase 3 summons: Bomb Bot every 15 s **from the ACU itself**, Assault Bot 30 s and Junk Bot 10 s from perimeter pads | `boss_mimiron.cpp:1688-1701` |
| Summon pads sit on **three arms**: west (180°), north-east (+59.4°), south-east (-59.4°) from the room centre | `acore_world` `gameobject` 194740-194748 |
| Adds despawn `TEMPSUMMON_CORPSE_TIMED_DESPAWN, 25000` — the corpse lingers 25 s | `boss_mimiron.cpp:2044` |
| The ACU really does set `MOVEMENTFLAG_HOVER` while airborne, cleared when grounded | `boss_mimiron.cpp:1560`, `:1598` |
| `IsMechanicTrackerBot` returns the **first alive playerbot in group order**, regardless of role | `RaidBossHelpers.cpp:132-151` |
| `PetAttackAction`'s trigger node is commented out globally, so pets keep their last target unless a fight redirects them; `CommandPetAttack` / `StopPet` are the intended hooks | `CombatStrategy.cpp:53-55`, `RaidBossHelpers.cpp:331-374` |
| The boss script itself sends the MK II to `(2744.65, 2569.46, 364.31)` — the same point as `ULDUAR_MIMIRON_ROOM_CENTER` | `boss_mimiron.cpp:657` |
| navprobe map 603: the floor is walkable to **40 yd** from the room centre, 16/16 headings, flat at Z **364.314** | `navprobe ring 2744.65 2569.46 364.32 40 16` |
| navprobe map 603: `(2762.65, 2569.46, 364.31)` and a 12 yd fan around it are 16/16 on mesh, flat at Z 364.314 | `navprobe point` + `ring … 12 16` |

### Root causes

**#1 Charge.** `MimironShockBlastAction::Execute` returns **false** in two situations, and both hand
the tick to `charge` at relevance 40: once the bot is clear (`20.0f - GetDistance2d(mk2) <= 0`, so
`MoveAwayClearOfMines` bails on a non-positive distance), and while a flee leg is in flight (the
`MOVEMENT_FORCED` lock refuses a second `MOVEMENT_FORCED` move, every bearing fails, and the
`MoveAway` fallback issues at `MOVEMENT_COMBAT` and is refused too). Charge ignores hazards, so the
warrior lands back inside a 15 yd, 100000-damage circle with seconds of cast left.

Compounding it, `20.0f - bot->GetDistance2d(mk2)` mixes units: `GetDistance2d` already took off the
MK II's reach of 8 and the bot's ~1.5, so the loop only stops at **29.5 yd centre to centre** against
a 15 yd radius. The trigger's ranged branch has the same bug the other way — `GetDistance2d(boss) < 15`
fires out to 24.5 yd.

**#2 Ring range, and what actually moves the boss.** The tank is melee, so the Shock Blast trigger
fires for it too: it runs 18 yd, the MK II chases, and the boss ends up somewhere new every 30 s with
nothing to bring it back. Meanwhile `GetMimironSpreadSlot` pins the ring to `ULDUAR_MIMIRON_ROOM_CENTER`
at radius 22, so a far-side bot is `22 + drift` from the boss and **6.5 yd of drift** breaks the 28.5 yd
spell range. It then deadlocks rather than self-correcting: `reach spell` is `ACTION_HIGH` (20) and the
ring is `ACTION_RAID` (60), so the ring wins and walks the bot back out of range.

**#3 Phase 3 shape.** Same generator, so phase 3 also gets a full ring around the room centre. Adds
funnel in from three arms and walk into isolated ranged bots, and a ring cannot be right for a boss
that flies.

**#4 Bomb Bots.** Ranged already rank Bomb Bot first in `BuildPriorityList`, but `MimironBombBotAction`
sits at `ACTION_RAID + 2` and makes them run instead — and running cannot work against something faster
than a player. Nothing checks range either, so a Bomb Bot spawning at the ACU 40 yd away becomes the
target and drops the bot into the same deadlock as #2.

**#5 Magnetic Core.** `MimironMagneticCoreTrigger` requires an Assault Bot corpse **already within 5 yd**
and `MimironMagneticCoreAction` has no travel branch — no corpse in reach means `return false`. Nothing
walks the carrier to one, and the carrier is the first bot in group order, usually a ranged bot pinned
22 yd out.

**#6 Pets.** Nothing redirects them, so they hold whatever they latched onto. A ground pet cannot reach
a unit hovering 15 yd up; it stands underneath contributing nothing while Junk and Assault Bots pile in.

### One decision reversed after checking the mechanic

The grilling round agreed a tank should hold ACU threat in phase 3 so it parks predictably. **That is
not buildable as specified**: no tank can melee a unit 15 yd up, so it reduces to taunt on an 8 s
cooldown with a 3 s duration, and the ACU re-picks top threat in the gaps. DK and paladin tanks could
hold it with ranged threat; warriors and druids cannot.

Dropped, because F4 already delivers what it was for: `AttackStartCaster` **stops** at 30 yd and never
retreats when approached, so an ACU whose victim is standing on a fixed slot parks and stays parked,
and the rigid translation in F3 slides the formation on the rare occasions it does move.

---

## Work items

### F1 — Shock Blast: correct distance, and hold the tick

`UldBossHelper.h`:

```cpp
// Shock Blast 63631 is TARGET_SRC_CASTER with a 15 yd radius and a 4 s cast, so 18 clears it with
// margin. Measured centre to centre: the old code built its flee distance out of GetDistance2d,
// which had already taken off the MK II's combat reach of 8 and the bot's own 1.5, and so ran
// everyone out to 29.5 yd.
constexpr float ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST = 18.0f;
```

`UldTriggers_Mimiron.cpp:34-41` — ranged branch becomes
`bot->GetExactDist2d(boss) < ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST`. Melee branch unchanged.

`UldActions_Mimiron.cpp:78-96`:

```cpp
float const gap = ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST - bot->GetExactDist2d(leviathanMkII);
if (gap > 0.0f)
{
    MoveAwayClearOfMines(leviathanMkII, gap, MovementPriority::MOVEMENT_FORCED);

    if (botAI->IsMelee(bot))
        botAI->SetNextCheckDelay(100);

    // Still inside the circle, so nothing else gets this tick - not even while a leg is in flight,
    // where the movement lock refuses a second move and every bearing comes back false.
    return true;
}

// Clear, and the only thing that can still undo that is a gap-closer: Charge sits at relevance 40
// with nothing above it once this node yields. Everyone else keeps casting through the rest of the
// cast rather than losing four seconds in thirty.
return bot->getClass() == CLASS_WARRIOR || bot->getClass() == CLASS_DRUID;
```

The ring at radius 22 now sits outside 18, so ranged stop fleeing Shock Blast entirely.

### F2 — Suppress gap-closers inside the lethal dodge windows

New `MimironChargeGuardMultiplier` in `UldMultipliers.h` / `.cpp`. One `dynamic_cast` covers all four
gap-closers, because `CastReachTargetSpellAction` has no other subclasses:

```cpp
float MimironChargeGuardMultiplier::GetValue(Action* action)
{
    // Cheap gate first: this only ever has an opinion about the gap-closers, and every one of them is
    // a CastReachTargetSpellAction - Charge, Intercept and both Feral Charges, nothing else.
    if (!dynamic_cast<CastReachTargetSpellAction*>(action))
        return 1.0f;

    return MimironLethalWindowActive(botAI) ? 0.0f : 1.0f;
}
```

File-local helper in `UldMultipliers.cpp` (include `UldTriggers.h`), gated on a mech being alive so it
costs nothing elsewhere in Ulduar, then the five windows a hit actually kills through: shock blast,
laser barrage, rocket strike, dodge flames, frost bomb. The two hard-mode triggers already early-out on
`IsMimironHardModeActive`.

**Mines and Bomb Bots are deliberately excluded.** The mine node was demoted to `ACTION_RAID - 1` last
round precisely because eating one is healable; letting a mine veto a charge would contradict its own
ranking. Bomb Bots are the same argument at 5 yd.

Register beside `MimironTargetGuardMultiplier` in `UldStrategy.cpp:654`.

### F3 — Stop the phase 1 drift, and anchor the ring to the ground mech

**Tank spot.** `GetMimironSpreadSlot` already has a main-tank branch for phase 4; add a phase 1 one
returning `ULDUAR_MIMIRON_ROOM_CENTER`. No new node — the arc spread node walks the tank back at
`ACTION_RAID`, the Shock Blast flee outranks it at `+3` during the cast, and the 5 yd tolerance stops
it fidgeting. The coordinate is the one the boss script itself charges the MK II to
(`boss_mimiron.cpp:657`), so it is not invented.

Comment to carry: nothing else brings the boss back, and a tank that flees 18 yd every 30 s and stays
there walks the MK II round the room over a five minute phase.

**Anchor.** New helper:

```cpp
// The mech the ranged formation is shaped around. Phase order is MK II, VX-001, Aerial Command Unit,
// then all three together, and VX-001 is the one that stays parked once they reassemble.
Unit* GetMimironRingFocus(PlayerbotAI* botAI);   // VX-001, else MK II, else ACU
```

In `GetMimironSpreadSlot` (`UldBossHelper.cpp:2017-2066`) the ring centres on the focus rather than the
room, except for the ACU, which hovers and wanders — anchoring fifteen bots to it makes the whole
formation chase.

**Range recovery is a rigid translation, not a per-slot clamp.** Every input is group-global (anchor,
focus, radius, count), so each bot computes the same offset independently and the shape is preserved
exactly:

```cpp
constexpr float ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN = 4.0f;
// Everything within 40 yd of the room centre is walkable and flat at Z 364.31 (navprobe, 16 headings).
// Past that a boss-anchored formation hangs over the edge once the MK II reaches the wall.
constexpr float ULDUAR_MIMIRON_ROOM_RADIUS = 40.0f;
```

- `excess = (dist(anchor, focus) + radius) - (botAI->GetRange("spell") - MARGIN)`; if positive, slide
  the whole anchor that far toward the focus.
- Then clamp the anchor to `ULDUAR_MIMIRON_ROOM_RADIUS` of the room centre — also rigid.
- Z is always `ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ()`; the floor is flat.

Rationale for the comment: clamping slots one at a time deforms the formation into a lopsided blob
leaning at the boss, which hands Rapid Burst and Bomb Bots exactly the clumps the spread exists to
prevent.

`ULDUAR_MIMIRON_SPREAD_RADIUS` stays 22 and `ULDUAR_MIMIRON_SPREAD_TOLERANCE` stays 5.

### F4 — Phase 3: group in the east wedge

The three add arms leave the room centre at 180°, +59.4° and -59.4°, so the east wedge is the one
stretch of floor no add walks down.

```cpp
// Phase 3 staging, 18 yd east of the room centre - the gap between the north-east and south-east
// summon arms. Grouping here funnels every Junk and Assault Bot into the melee instead of into a lone
// ranged bot on a ring. navprobe: this point and a 12 yd fan around it are 16/16 on mesh, Z 364.31.
extern const Position ULDUAR_MIMIRON_PHASE3_STAGE;   // (2762.65f, 2569.46f, 364.31f)

// Bomb Bots blast 5 yd, so no two bots may share one; 6 keeps a detonation to a single victim.
constexpr float ULDUAR_MIMIRON_PHASE3_SPACING = 6.0f;
constexpr float ULDUAR_MIMIRON_PHASE3_MIN_RADIUS = 8.0f;
constexpr float ULDUAR_MIMIRON_PHASE3_MELEE_RADIUS = 6.0f;
```

When the ACU is the only mech alive, `GetMimironSpreadSlot` stages everyone instead of ringing the room:

- **Ranged and healers** take a **half-fan opening away from the hub** (bearings within ±90° of east),
  radius `max(MIN_RADIUS, SPACING * count / M_PI)`. A full circle would put a third of them ~6 yd from
  the hub, in front of the melee and on top of where adds converge.
- **Melee and both tanks** take a short arc on the hub side of the stage at `MELEE_RADIUS`, so they
  meet the adds first. This is also the only thing spacing melee out once disperse comes off.

`MimironAerialCommandUnitAction` (`UldActions_Mimiron.cpp:228-233`) stops setting `disperse distance`
to 5.0, and its trigger check on line 141 goes with it: the slots guarantee 6 yd on their own, and
`CombatFormationMoveAction` shoving bots off slots the formation then pulls them back onto is exactly
the thrash to avoid.

The F3 rigid translation is what keeps the far side of the fan inside spell range of the ACU. Because
`AttackStartCaster` stops at 30 yd and never retreats when approached, the ACU parks once its victim is
standing on a slot, and the translation only fires when threat changes hands.

### F5 — Ranged DPS kill Bomb Bots instead of running

`MimironBombBotTrigger` (`UldTriggers_Mimiron.cpp:198-202`) returns false for a **ranged DPS** when the
Bomb Bot is inside its casting range — that bot's job is to shoot it. Melee keep the sidestep (5 yd is
cheap and they cannot kill it alone), and healers keep healing rather than being swept in by a bare
`!IsMelee` test.

`MimironSetDpsPriorityAction::BuildPriorityList` (`UldActions_Mimiron.cpp:400-402`) only offers
`NPC_BOMB_BOT` when the selected one is within `botAI->GetRange("spell")`, and only to ranged DPS.
Out-of-range Bomb Bots stay off the list entirely, so a bot never abandons the ACU to chase one.

Comment to carry: a Bomb Bot runs 8.0 yd/s against a player's 7.0 and dies to almost nothing, so range
is the whole question — in range, shoot it; out of range, it is someone else's.

### F6 — Actually deliver the Magnetic Core

```cpp
// The core carrier. Group order so every bot computes the same answer, but melee first: they are
// already standing on the Assault Bot when it dies, whereas the plain first-bot-in-group pick is
// usually a ranged bot 22 yd out that will never be within loot range of anything.
Player* GetMimironCoreCarrier(PlayerbotAI* botAI);
```

First alive non-tank melee playerbot in group order, falling back to `IsMechanicTrackerBot`'s pick when
the raid has no melee.

`MimironMagneticCoreTrigger` (`UldTriggers_Mimiron.cpp:259-279`) — swap `IsMechanicTrackerBot` for
`GetMimironCoreCarrier(botAI) == bot`, and widen the corpse search from `ULDUAR_MIMIRON_CORE_LOOT_RANGE`
to a new `ULDUAR_MIMIRON_CORE_SEARCH_RANGE = 60.0f`, so the node goes live while the corpse is still
somewhere in the room.

`MimironMagneticCoreAction::Execute` (`UldActions_Mimiron.cpp:578-604`) gains the missing leg: holding
no core and with the nearest corpse beyond `ULDUAR_MIMIRON_CORE_LOOT_RANGE`, walk to it at
`MOVEMENT_COMBAT` and return true. The pickup, the walk to the ACU and the `CMSG_USE_ITEM` path are
unchanged.

Comment to carry: corpses linger 25 s, which is the whole window — the node has to start the walk, not
wait for the bot to happen to be standing there.

### F7 — Keep pets off the airborne ACU

New trigger/action pair `mimiron pet control` — the only new node in this round, so **all eight wiring
sites apply** (class declaration, `creators[...]` entry, static factory, `TriggerNode`, for both the
trigger and the action).

- Trigger: the bot has a guardian pet, the ACU is alive, and it still has `MOVEMENTFLAG_HOVER`.
- Action at `ACTION_RAID`: `CommandPetAttack` onto the nearest live Assault Bot, else Junk Bot, else
  Bomb Bot; `StopPet` when no add is up. **Returns `false`** so the node never consumes the tick — it
  only redirects the pet and lets the bot get on with its own turn.

Comment to carry: a ground pet cannot reach something hovering 15 yd up, and `PetAttackAction` is
disabled globally, so without an explicit redirect the pet keeps the target it latched onto and stands
under the ACU doing nothing. Adds are the better target anyway — the Assault Bot is the only Magnetic
Core source.

### F8 — Firefighter: flames are a destination hazard too

`IsMimironSpotSafe` (`UldBossHelper.cpp:1966-1990`) also rejects a destination within
`ULDUAR_MIMIRON_FLAMES_RADIUS` of a live `NPC_FLAMES_SPREAD` / `NPC_FLAMES_INITIAL`, behind an
`IsMimironHardModeActive` check.

Without it the staging point can sit in fire: the flames node at `ACTION_RAID + 4` pushes the bot out,
the formation at `ACTION_RAID` pulls it back, and it paces there until it burns down. Same defect class
as the rocket-marker one fixed last round, and the same fix.

### F9 — Docs

`docs/raids/ulduar.md`, Mimiron section:
- Shock Blast's 4 s cast and 15 yd radius, and why the dodge node must hold the tick rather than
  return false — `charge` at relevance 40 is waiting underneath it.
- `GetDistance2d` versus `GetExactDist2d` around a boss with combat reach 8. Second time this has bitten
  the encounter.
- The tank flee that walks the boss round the room, and the phase 1 tank spot that stops it.
- The 28.5 yd spell range against a formation the boss can walk out of, the `ACTION_HIGH` versus
  `ACTION_RAID` deadlock that makes it permanent, and why recovery is a rigid translation.
- `AttackStartCaster(who, 30.0f)`: the ACU parks just outside bot casting range of its victim and never
  retreats when approached. Both halves matter.
- The three summon arms, the east wedge, and the navprobe-verified staging point and floor Z.
- Bomb Bot speed 8.0 versus player 7.0: running is not an option, range decides who shoots it.
- Why the Magnetic Core never fired — a trigger with no travel branch on a carrier chosen by group
  order rather than proximity.
- Pets and `PetAttackAction` being globally disabled: any fight with an unreachable boss has to redirect
  them by hand.

Mirror this plan to `docs/plans/mimiron-cheat-free-rebuild/mimiron-p1-p3-positioning.PLAN.md`.

---

## Files touched

- `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` — F1, F3, F4, F6, F8 constants and helpers
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.h` / `.cpp` — F1, F5, F6 triggers, F7 new trigger
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.h` / `.cpp` — F1, F4, F5, F6 actions, F7 new action
- `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp` — F2
- `src/Ai/Raid/Uld/UldStrategy.cpp` — F2 multiplier, F7 node and both `creators[...]` entries
- `docs/raids/ulduar.md`, `docs/plans/mimiron-cheat-free-rebuild/…PLAN.md` — F9

## Verification

The module cannot be compiled from this checkout; the worldserver build is the only compile path.

1. Build, then pull Mimiron with the SQL applied
   (`SELECT COUNT(*) FROM creature WHERE id=33576` must return 1).
2. **Charge.** Watch a warrior through a phase 1 Shock Blast: it runs to ~18 yd, stands there for the
   rest of the 4 s cast, and charges back only after the blast lands. No Shock Blast entries on melee in
   the log. Repeat during Laser Barrage. Then confirm the other half — a healer inside 18 yd keeps
   casting once it is clear rather than freezing for four seconds.
3. **Phase 1 drift.** Over a full phase 1, the MK II stays within ~10 yd of the room centre; the tank
   returns after every Shock Blast. Nobody stops casting for range.
4. **Rigid translation.** Drag the MK II 20 yd off centre and confirm the ranged formation slides as a
   whole — same relative spacing, no lopsided lean toward the boss — and that no bot ends up further
   than 40 yd from the room centre.
5. **Phase 3 shape.** The raid groups in the east wedge, melee and tanks hub-side, ranged and healers
   fanned out behind them roughly 6 yd apart, nobody standing on a summon arm. Junk and Assault Bots
   walk into the melee.
6. **Bomb Bots.** Ranged DPS inside casting range switch and kill it; ranged DPS out of range stay on
   the ACU and do not walk toward it; healers keep healing; no ranged bot fleeing one.
7. **Magnetic Core.** The carrier walks to an Assault Bot corpse, picks the core up, carries it under
   the ACU and grounds it — the ACU visibly lands and takes melee damage. Never once observed in
   testing, so watch for it explicitly rather than inferring it from a faster phase.
8. **Pets.** No pet parked under the hovering ACU. Hunter, warlock and shaman pets are on adds, and
   they switch back to the ACU once it is grounded.
9. Cheat-free grep: `HasCheat`, `TeleportTo`, `->Kill(` return nothing across `UldTriggers_Mimiron.*`
   and `UldActions_Mimiron.*`.
10. Full clear on normal, then Firefighter — check the staging point is abandoned when fire reaches it
    and that bots do not pace on its edge.

## Known and deliberately out of scope

- No tank holds ACU threat in phase 3; see the reversal note above.
- W6 Napalm Shell spread, carried over from the first round, still awaiting a decision.

---

## As built — where the implementation departed from the plan above

1. **Wedge geometry.** A half-fan sized by spacing alone either could not hold a 17-strong ranged
   group inside casting range or spilled onto the add arms. Built instead as a ±60° wedge with a
   fixed melee band (rows at 8 and 14 yd) and a ranged band from 18 yd whose rows stop at
   `SpellDistance` minus the margin; when the band is full the remaining rows pack tighter rather
   than growing outward. A 17-strong ranged group still keeps 4.7 yd of separation.
2. **Shock Blast hold-once-clear** is gated on `IsMelee`, not on warrior/druid class. A restoration
   druid is `CLASS_DRUID` and would have been frozen for the whole 4 s cast.
3. **`IsMimironTankAnchorSlot`** was added: the main tank's phase 1 and phase 4 spots are exempt from
   `IsMimironSpotSafe`. The MK II parks on top of the mine field it just laid, so a tank that applies
   the destination filter to its own anchor never brings the boss back. This was latent in phase 4
   before this round.
4. **Pet targeting** picks the nearest add of each entry rather than the first one in
   `possible targets no los`, which is in no useful order — the pet has to run there.
