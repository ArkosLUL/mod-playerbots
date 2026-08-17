# Mimiron: Laser Barrage dodge model, phase 4 targeting, phase 3 melee, handovers, phase 1 tank swap

## Context

Two rounds of Mimiron fixes are in the working tree (uncommitted). A further 25-man test surfaced
five defects:

1. **The Laser Barrage dodge aims at the wrong arc.** Bots latch the cone bearing at Spinning Up
   *start*, but the cone does not ignite until 4 s later and re-aims at that moment. The model is
   therefore ~43° counter-clockwise of the real danger band in every phase. Bots standing in 43° of
   genuinely lethal floor are told they are safe, and bots fleeing clockwise stop 43° short of the
   trailing edge. The cone does 20000 damage every 250 ms, so anyone caught dies.
2. **Phase 4 orders targets by raw health**, but the Aerial Command Unit has two thirds the health
   pool of the other two mechs, so it is always ranked last and never keeps pace. Phase 4 also
   inherits two flag leaks from phase 3.
3. **Phase 4 is worse than phase 2 for the barrage**, for three further reasons on top of (1).
4. **Phase 3 pins melee to formation slots**, which outrank the chase and keep them off the adds.
5. **The phase 1 tank swap never happens.** The Leviathan MK II is tauntable, but the taunt is issued
   mid-cast where it cannot move anything, a 3 s taunt cannot hold a 22 s rotation, and every hunter
   and rogue in the raid is redirecting threat back onto the main tank the whole time.
6. **Pets do nothing in phase 4.** Either stopped outright by a leaked hover flag, or left holding a
   dead phase 3 add that nothing retargets.
7. **The phase handovers are spent following the master.** They run 47.75 s, 24 s and 31.8 s, and no
   Mimiron node can resolve a boss for any of it, so the engine falls through to the `follow` default
   action and the raid trails whoever it is following instead of forming up for the next phase.

Fix exactly these. The cheat-free design does not reopen: no `HasCheat`, no `TeleportTo`, no
`->Kill(`.

---

## Verified facts

| Fact | Source |
|---|---|
| Spinning Up 63414: aura 23, period 4000 ms, duration 4000 ms, triggers 63274 — so the barrage lands exactly **4 s** after Spinning Up starts | `spell.reference.csv`, `spellduration.reference.csv` |
| 63274: duration **10000 ms**, effect 2 is aura 23 at **250 ms**, triggering the damage 63293; effect 3 links 63300 (beam visuals only) | `spell.reference.csv` |
| 63293: `TARGET_UNIT_CONE_ENEMY_104` → `coneAngle = 104°`, checked via `isInFront` → **±52°**; radius index 28 = 50000 yd; base points **19999**, die sides 1 → ~20000 **per 250 ms tick** | `Spell.cpp:1256`, `Spell.cpp:9218`, `spell.reference.csv`, `spellradius.reference.csv` |
| `spell_mimiron_p3wx2_laser_barrage_aura` calls `FaceBarrageArc` on **AfterEffectApply** and on every 250 ms periodic tick. Spinning Up has no such script — the boss re-aims once at t=0 and then not again until ignition | `boss_mimiron.cpp:2153-2181`, `:1435-1441` |
| Mimiron DB Target (33576) guid 13395, `MovementType=2`, waypoint path 13395: 19 points, velocity **20.8988** yd/s, perimeter **707.0 yd** → 33.8 s lap → **10.64 °/s**, **106.4°** per 10 s barrage, bearings strictly decreasing (**clockwise**) | `acore_world` `creature`, `creature_addon`, `waypoint_data`; least-squares circle fit |
| Orbit fit: centre (2741.98, 2569.36), r = 113.2 — **2.7 yd** from `ULDUAR_MIMIRON_ROOM_CENTER` (2744.65, 2569.46), so the room centre is a usable orbit centre | least-squares fit over the 19 waypoints |
| Bearing rate seen from an observer **off centre** is not constant: at 15 yd it swings 9.2–12.5 °/s, at 30 yd **8.3–14.7 °/s** (−22 % / +38 %) | same fit, evaluated per waypoint |
| `FindTargetValue` scans `bot->GetThreatMgr().GetThreatenedByMeList()` — it only resolves a boss that already has *this bot* on its threat list | `TargetValue.cpp:159-184` |
| VX-001 `SetData(1, 4)` calls neither `DoZoneInCombat()` nor `AttackStart()`, and its `AttackStart` is a no-op override. LMK2 and the ACU both call `DoResetThreatList()` + `AttackStart()` + `DoZoneInCombat()` | `boss_mimiron.cpp:1296-1310`, `:1017`, `:1572` |
| `PossibleTargetsValue` is a grid sweep at `sPlayerbotAIConfig.sightDistance` with no threat dependency; it does reject `UNIT_FLAG_NON_ATTACKABLE` | `PossibleTargetsValue.cpp:34`, `AttackersValue.cpp:131-156`, `ValueContext.h:445` |
| Phase 4 win condition: `EVENT_FINISH` fires only when **all three** are simultaneously casting Self Repair. Self Repair 64383 has casting-time index 192 = **15000 ms** | `boss_mimiron.cpp:864-895`, `spellcasttimes.reference.csv` |
| A part drops out at `health < 15000` (damage is zeroed, `UNIT_FLAG_NON_ATTACKABLE` set, Self Repair cast). `SpellHit(SELF_REPAIR)` brings it back aggressive | `boss_mimiron.cpp:1035-1079`, `:1208-1212` |
| `HealthModifier`: LMK2 **300**, VX-001 **300**, ACU **200** — the ACU's pool is two thirds the others' | `acore_world` `creature_template` |
| Phase 4 seating: VX-001 → LMK2 seat index 3 (VehicleSeat **3886**), ACU → VX-001 seat index 3 (VehicleSeat **3806**). Both have `AttachmentOffsetX/Y/Z = 0,0,0`, and `Vehicle::AddPassenger` relocates the passenger by exactly those offsets — so **all three occupy the same point server-side** | `vehicle.reference.csv`, `vehicleseat.reference.csv`, `Vehicle.cpp:397-405` |
| Nothing in the vehicle entry path clears `MOVEMENTFLAG_HOVER`, and the ACU's phase 3 defeat branch does not remove it either — so the flag survives into phase 4 **iff** the ACU happened to be airborne when it was defeated | `boss_mimiron.cpp:1621-1650`, `Vehicle.cpp:390-410`, grep for hover in the vehicle path |
| LMK2 in phase 4 runs `AttackStart(target)` under plain `ScriptedAI`, so the chassis **chases its victim** and drags VX-001 (the cone apex) with it | `boss_mimiron.cpp:1017-1029` |
| Phase 4 ACU uses `DoSpellAttackIfReady(SPELL_PLASMA_BALL_P2)` — 65647, ~9424–10575 single target. Survivable by a ranged bot; no tank required on it | `boss_mimiron.cpp:1716`, `spell.reference.csv` |
| Phase 1→2 handover: 5 + 6 + 6 + 18 + 4 + 5 + 2 + 1.75 = **47.75 s**. VX-001 is summoned at the room centre 17 s in, `NOT_SELECTABLE` until the end | `boss_mimiron.cpp:869`, `:491-568` |
| Phase 2→3 handover: 2.5 + 2.5 + 2 + 7 + 6 + 4 = **24 s**. The ACU is summoned at `ACUSummonPos` (2744.650, 2569.460, 380.0) 5 s in | `boss_mimiron.cpp:568-616`, `:280` |
| Phase 3→4 handover: 5 + 5 + 4 + 4.8 + 3 + 10 = **31.8 s**. The chassis charges to (2755.77, 2574.95) at 10 s and to the room centre at 18.8 s, where VX-001 boards it; the ACU boards at 21.8 s | `boss_mimiron.cpp:618-704` |
| A defeated mech sets `UNIT_FLAG_NOT_SELECTABLE` and stays in the world — the MK II parks at (2795.076, 2598.616), 58 yd off centre, for phases 2 and 3. All three `DespawnOrUnsummon(7s)` at `EVENT_FINISH` | `boss_mimiron.cpp:1044-1057`, `:707-732` |
| `Creature::FindNearestCreature` is a grid check on entry, alive state and range — it does **not** filter on selectability, unlike `"possible targets no los"` | `AttackersValue.cpp:155`, existing use at `UldActions_Mimiron.cpp` for Assault Bot corpses |
| Plasma Blast 62997: **3 s cast**, `TARGET_UNIT_TARGET_ENEMY`, aura 3 at 1000 ms period, base 16999 die-sides 1 → **17000/s for 6 s = 102000**, no stacking, repeats every **22 s**. The debuff expires 16 s before the next cast | `spell.reference.csv`, `spellcasttimes.reference.csv`, `spellduration.reference.csv`, `boss_mimiron.cpp:1129-1135` |
| The cannon casts at `me->GetVictim()` resolved **at cast start**, so nothing during the cast can move that cast's target | `boss_mimiron.cpp:1129-1134` |
| MK II `flags_extra` 524289 = `OBEYS_TAUNT_DIMINISHING_RETURNS (0x80000) | INSTANCE_BIND (0x1)` — **no** `CREATURE_FLAG_EXTRA_NO_TAUNT (0x100)`. `CreatureImmunitiesId` -361 masks 21 mechanics and effects 98/124/144/145 (knockback and pull), none of them taunt. It is fully tauntable | `acore_world` `creature_template`, `creature_immunities`, `CreatureData.h:47-75` |
| Taunt DR only applies to creatures carrying `OBEYS_TAUNT_DIMINISHING_RETURNS`, and resets after 15 s. At a 22 s cadence every taunt lands at full duration | `Unit.cpp:11864-11882`, `SpellMgr.cpp:292-320` |
| `Vehicle::RelocatePassengers` writes each passenger to `vehiclePos + seatOffset` via `UpdatePosition`, and neither that nor the relocation path re-applies hover height. With zero seat offsets and a chassis that chases its victim, the ACU sits at **ground level** in phase 4 | `Vehicle.cpp:535-556`, `Unit.cpp:16076-16101`, `:16714-16741` |
| `BuffOnMainTankAction::getName()` returns `"<spell> on main tank"`, so `"misdirection on main tank"` and `"tricks of the trade on main tank"` are exact action names a multiplier can match | `GenericSpellActions.h:491-511`, `HunterActions.h:175-180`, `RogueTriggers.h:152-156` |
| `"taunt spell"` is registered for all four tank specs: warrior `taunt`, DK `dark_command`, paladin `hand_of_reckoning`, druid `growl` | `TankWarriorStrategy.cpp:22`, `BloodDKStrategy.cpp:20`, `TankPaladinStrategy.cpp:19`, `BearDruidStrategy.cpp:21` |
| On evade, VX-001 and the ACU are `DespawnOrUnsummon()`'d outright; only the MK II is a permanent spawn that merely evades. So no staging focus exists before a pull or after a wipe | `boss_mimiron.cpp:802-822` |
| `Unit::SetFacingTo` on a transport passenger calls `DisableTransportPathTransformations`, so the spline facing is seat-local while `GetOrientation()` stays world | `Unit.cpp:16526-16534`, `:16545` |
| Instance strategies are added to **both** `BOT_STATE_COMBAT` and `BOT_STATE_NON_COMBAT`, so `ACTION_RAID` nodes run out of combat | `PlayerbotAI.cpp:1793-1794` |
| `follow` is a **default action at relevance 1.0**, not a trigger node, so it is what the engine reaches when nothing else succeeds. `drink` and `food` sit at 3.0 to 4.2 | `FollowMasterStrategy.cpp:9-14`, `UseFoodStrategy.cpp:16-22`, `GrindingStrategy.cpp:13-14` |
| `SPELL_ELEVATOR_KNOCKBACK` (65096) is cast 11 s into the phase 1 handover from a trigger on GO 194749 at (2744.56, 2569.35) — the room centre | `boss_mimiron.cpp:502-511`, `acore_world` `gameobject` |
| Phase 3 summon pads (GO 194740-194748) sit at roughly 17, 29 and 40 yd on three arms, so every add walks in from outside any melee formation slot | `acore_world` `gameobject` (previous round) |

### Root causes

**Barrage, all phases.** `MimironP3Wx2LaserBarrageAction` measures every bot against
`GetMimironLatchedBarrageArc`, which latches `vx001->GetAngle(dbTarget)` the first tick the trigger
goes live — i.e. at Spinning Up start. The cone does not exist yet; it ignites 4 s later, re-aimed at
the DB Target's position *then*, which is **42.6° clockwise**. Relative to the latched angle the true
danger band is `(−213.7°, +21.2°)`, but the code treats `(−170.9°, +64°)` as unsafe. It therefore

- marks 43° of lethal floor safe, so bots standing there never move, and
- stops clockwise runners at −170.9°, inside the real band, right where the tail finishes.

**Barrage, phase 4 specifically**, three more compounding faults:

1. Both the trigger and the action resolve the boss with `AI_VALUE2(Unit*, "find target", "vx-001")`.
   That value only sees bosses that already threaten this bot, and VX-001's phase 4 entry calls no
   `DoZoneInCombat`. Any bot that never damaged VX-001 gets `nullptr` and **the dodge never runs at
   all**. Every other Mimiron node uses `GetFirstAliveUnitByEntry`.
2. The fixed 10.6 °/s rate is only correct for a VX-001 standing at the room centre. On the chassis
   it sits up to 30 yd out, where the true rate is 8.3–14.7 °/s — up to 40° of extra error per cast.
3. `ULDUAR_MIMIRON_BARRAGE_RELATCH_DIST` (5 yd) fires constantly while the chassis chases the tank,
   resetting the reference mid-cast to "the sweep is only just starting".

**Phase 4 targeting.** `BuildPriorityList` sorts the three mechs by `GetHealth()` descending. With the
ACU at `HealthModifier` 200 against 300, it is the smallest absolute pool and so is always last. It
also cannot be reached by melee (see below), so it falls further behind, and the raid cannot get all
three into the 15 s Self Repair window.

**Phase 1 tank swap.** Four faults stacked:

1. `MimironPlasmaBlastTrigger` requires `cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST)`,
   so it only fires **while the cannon is already casting**. The victim was resolved at cast start, so
   a mid-cast taunt cannot change that cast — only the next, 22 s away.
2. Taunt lasts 3 s. It expires, the MK II re-picks top threat, and 19 s later that is still the main
   tank.
3. `CastMisdirectionOnMainTankAction` and `TricksOfTheTradeOnMainTankTrigger` both target the main
   tank by name, so every hunter and rogue in the raid transfers threat onto exactly the tank the swap
   is trying to move off.
4. `MimironPlasmaBlastAction` burns a tick on `Attack(leviathanMkII)`, which returns true and ends the
   tick, before it can reach `DoSpecificAction("taunt spell", ...)` on the next one — two ticks against
   a 3 s window.

**Two phase 3 leaks into phase 4.** Both key off `MOVEMENTFLAG_HOVER`, which the ACU keeps into phase
4 on a coin flip:

- `MimironSetDpsPriorityAction::IsAllowedTarget` rejects the ACU for melee whenever it hovers. This is
  the actual reason melee never touch it — not geometry.
- `MimironPetControlTrigger` fires in phase 4, finds no Assault/Junk/Bomb Bot, and calls `StopPet`.
  **Every hunter, warlock and shaman pet stops attacking for the whole of phase 4.** This is a
  regression from the previous round.

---

## Work items

### B1 — Resolve Mimiron bosses by entry, not by threat

Replace `AI_VALUE2(Unit*, "find target", …)` with `GetFirstAliveUnitByEntry(botAI, …)` at all four
Mimiron call sites:

- `MimironP3Wx2LaserBarrageTrigger::IsActive` and `MimironP3Wx2LaserBarrageAction::Execute`
  (`"vx-001"` → `NPC_VX001`)
- `MimironShockBlastTrigger::IsActive` (`"leviathan mk ii"` → `NPC_LEVIATHAN_MKII`)
- `MimironRocketStrikeTrigger::IsActive` (`"vx-001"` → `NPC_VX001`)

Comment to carry: the value behind `"find target"` walks the bot's own threat list, so it only ever
resolves a boss that already threatens this bot — VX-001 never calls `DoZoneInCombat` in phase 4, so
a bot that has not damaged it is blind to a mechanic that kills in one tick.

### B2 — Rebuild the barrage window as a live model

Delete the latch entirely. Every input is read live each tick, so apex motion, chassis rotation and
the off-centre rate all fall out for free, and no two bots can disagree.

**Remove** from `UldBossHelper.h` / `.cpp`: `struct MimironBarrageArc`,
`GetMimironLatchedBarrageArc`, `ULDUAR_MIMIRON_BARRAGE_LATCH_TTL`,
`ULDUAR_MIMIRON_BARRAGE_RELATCH_DIST`, `ULDUAR_MIMIRON_BARRAGE_SWEEP_TOTAL`,
`ULDUAR_MIMIRON_BARRAGE_SWEEP_RATE`. The rate is now measured per cast from the DB Target's own orbit
rather than assumed, and the no-DB-Target fallback needs no rate at all.

**Add:**

```cpp
// Waypoint velocity on path 13395. NPC 33576 laps a 707 yd polygon fitted by a circle of radius
// 113.2 centred 2.7 yd from ULDUAR_MIMIRON_ROOM_CENTER, which is close enough to orbit about the
// room centre: 10 s of prediction lands within a couple of degrees of bearing.
constexpr float ULDUAR_MIMIRON_DB_TARGET_SPEED = 20.8988f;

// 63414 is a 4 s aura whose only tick triggers the barrage, and 63274 then runs 10 s. Fallbacks for
// when the aura cannot be read; the durations themselves are always preferred.
constexpr float ULDUAR_MIMIRON_BARRAGE_SPIN_SECONDS = 4.0f;
constexpr float ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS = 10.0f;

// Widened from 12. That was about one bot reaction tick of cone travel, which is nothing to spare
// against 20000 damage every 250 ms. 15 leaves a 120 degree safe wedge, still wide enough to hold a
// 25 man raid at 22 yd.
constexpr float ULDUAR_MIMIRON_BARRAGE_MARGIN = 15.0f * static_cast<float>(M_PI) / 180.0f;
```

New helper, replacing `GetMimironLatchedBarrageArc`:

```cpp
// The cone as it will actually be, computed live rather than latched. `lead` is the centreline at the
// moment the beams start (which is 4 s after Spinning Up begins, not when it begins - the aura
// re-aims at the DB Target on apply, and by then it has moved 42.6 degrees clockwise). `sweep` is how
// much clockwise travel is still to come, and `rate` is what it works out at per second.
struct MimironBarrageWindow
{
    bool valid = false;
    float lead = 0.0f;    // world bearing of the cone centreline at ignition, or now if already firing
    float sweep = 0.0f;   // clockwise radians still to be swept, >= 0
    float rate = 0.0f;    // radians per second, 0 while still spinning up
    float untilLive = 0.0f;  // seconds until the first damage tick, 0 once firing
};

MimironBarrageWindow GetMimironBarrageWindow(Player* bot, Unit* vx001);
```

Implementation:

1. Read the auras off VX-001. `GetAura(SPELL_SPINNING_UP)` → `untilLive = GetDuration() / 1000`,
   `fire = ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS`. Else `GetAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1)` →
   `untilLive = 0`, `fire = GetDuration() / 1000`. Neither present → `valid = false`.
2. Find the DB Target (`bot->FindNearestCreature(NPC_MIMIRON_DB_TARGET, 250.0f)`). If it is missing,
   fall back to `lead = vx001->GetOrientation()`, `sweep = 0`, `rate = 0`. Not the nominal sweep:
   `FaceBarrageArc` returns early without 33576, so the core never re-aims and the cone stays frozen
   on the facing it has. A static ±clearance wedge is the correct read of that, and the raid should
   not be running a sweep that is not happening.
3. Otherwise predict the orbit. `OrbitAhead(p, t)` rotates `p` **clockwise** about
   `ULDUAR_MIMIRON_ROOM_CENTER` by `ULDUAR_MIMIRON_DB_TARGET_SPEED * t / radius`, where `radius` is
   measured live from the DB Target's current position.
   - `lead  = vx001->GetAngle(OrbitAhead(dbNow, untilLive))`
   - `tail  = vx001->GetAngle(OrbitAhead(dbNow, untilLive + fire))`
   - `sweep = NormalizeOrientation(lead - tail)` — clockwise magnitude in `[0, 2π)`
   - `rate  = fire > 0 ? sweep / fire : 0`

`GetMimironBarrageAngle` is now only used by the fallback path; fold it into the helper and drop the
separate declaration.

### B3 — Rewrite the dodge against that window

`MimironP3Wx2LaserBarrageAction::Execute`. Keep the existing shape — bearing-only movement, bounded
40° legs, `MOVEMENT_FORCED`, hold the tick while relocating, return false when clear — and replace
only the geometry.

**Angles must not be folded to (−π, π].** `sweep + 2 × clearance` reaches 240° at the room centre and
more off it, so a signed fold puts part of the danger band on the wrong side of ±180 — which is
exactly the far-side blind spot in the current code. Work in a clockwise-from-lead measure instead:

```cpp
float const clearance = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE + ULDUAR_MIMIRON_BARRAGE_MARGIN;

// How far clockwise of the cone centreline the bot stands, in [0, 2pi). The cone sweeps clockwise, so
// a small value means the beams are about to reach it and a large one means they already passed.
float const cw = Position::NormalizeOrientation(window.lead - vx001->GetAngle(bot));

// Two fringes: ahead of the leading edge, and behind where the trailing edge finishes.
bool const safe = cw > window.sweep + clearance && cw < 2.0f * M_PI - clearance;
if (safe)
    return false;
```

Escape:

- **Counter-clockwise** exit at `cw == 2π - clearance`; travel `= NormalizeOrientation(cw + clearance)`.
- **Clockwise** exit at `cw == sweep + clearance`; travel `= NormalizeOrientation(sweep + clearance - cw)`.
- Closing rates: `turnRate = bot->GetSpeed(MOVE_RUN) / radius`, as today. While still spinning up the
  band is fixed in world space, so both directions close at `turnRate`. Once firing, counter-clockwise
  closes at `turnRate + window.rate` and clockwise at `turnRate - window.rate` — infeasible when that
  is not positive.
- Take whichever clears sooner, but require clockwise to beat counter-clockwise by **1.5x** before
  taking it. Counter-clockwise runs away from the sweep, so it is the safer error, and the apex jumps
  around in phase 4 as the chassis chases its tank - the monotonicity that would otherwise rule out
  a flip mid-cast assumes a fixed apex. If neither exit is feasible, take the shorter travel rather
  than standing still.

Radius handling is unchanged (`clamp(GetExactDist2d(vx001), vx001->GetCombatReach() + RING_MARGIN,
ULDUAR_MIMIRON_SPREAD_RADIUS_MAX)`) **except** for the phase 4 main tank: give it a floor of
`vx001->GetCombatReach() + 1.5f` instead of `+ 6.0f`, so it orbits inside the chassis's chase range
and the MK II does not follow it. Comment to carry: the tank cannot simply hold
`ULDUAR_MIMIRON_PHASE4_TANK_SPOT` through the cast — 20000 per tick kills it — but every yard it runs
drags the apex, so it runs on the tightest ring it can.

Add `botAI->SetNextCheckDelay(100)` **only on the tick that issues a move** - not for the whole
window. The cone lands damage every 250 ms and turns 2.7° in that time, so a relocating bot has to
re-evaluate faster than a normal AI interval; a bot already clear only gets clearer as the cone
sweeps away from it, and the 15° margin is sized to cover one interval for a bot standing still.

Callers of the deleted helper: `MimironChargeGuardMultiplier`'s `MimironLethalWindowActive` only
constructs the trigger, so it is unaffected.

### F1 — Phase 4: direct targeting, health percent, ranged own the ACU

New shared helper in `UldBossHelper`, so the tank node and the DPS node cannot disagree:

```cpp
// Phase 4 only ends when all three parts are channelling Self Repair at once, and that cast is 15 s -
// so they have to come down level, not one at a time. Percent, not raw health: the ACU's
// HealthModifier is 200 against 300, so ordering on raw health ranked it last every tick and it never
// kept pace.
constexpr float ULDUAR_MIMIRON_PHASE4_HOLD_PCT = 10.0f;

// Phase 4, start to finish. Keyed on VX-001 riding the chassis rather than on all three being
// attackable: a part pushed under 15000 sets UNIT_FLAG_NON_ATTACKABLE and vanishes from "possible
// targets no los", and the phase is at its most time-critical after that, not over. Grid scan, so it
// sees the mechs whatever their flags say.
bool IsMimironPhase4(Player* bot);

// What this bot should be hitting in phase 4. nullptr means hold: everything it is allowed to touch is
// already at the floor, and pushing a part under early costs the raid the whole rendezvous. `melee` is
// passed rather than derived so the pet node can ask for a melee answer on behalf of a hunter.
Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee);
```

`GetMimironPhase4Focus`:

1. Not phase 4 → `nullptr`.
2. Collect the **attackable** parts. Fewer than three means one is already channelling Self Repair, so
   the rendezvous is over and the 15 s clock is running: return the highest-percent attackable part
   with no role restriction and no floor. Melee included on the ACU — there is no longer a rendezvous
   left to wreck, and this is the stretch that decides whether the kill lands or resets.
3. All three attackable, so the rendezvous is on. Allowed set: callers passing `melee == false` and
   `PlayerbotAI::IsRangedDps` get all three; **melee, tanks and healers get MK II and VX-001 only**. Melee never attack the Aerial Command Unit
   here — deliberate, and not dependent on `MOVEMENTFLAG_HOVER`, which leaks in from phase 3 at random.
   `PlayerbotAI::IsRangedDps`, not `IsRanged`, so a healer is never steered onto the ACU or into the
   hold path below and stops healing; same predicate the Bomb Bot rule already uses.
4. If every part is at or below `ULDUAR_MIMIRON_PHASE4_HOLD_PCT`, everyone pushes: return the
   highest-percent allowed part.
5. Otherwise return the highest-percent allowed part still **above** the floor.
6. Allowed nothing above the floor → `nullptr`, hold.

Comment to carry for step 5/6: all three sit on the same point server-side, so melee cleave splashes
every part; stopping at 10 % is the margin that keeps incidental damage from pushing one under while
the others are still high.

**`MimironPhase4MarkDpsAction` / `…Trigger` → rename to `mimiron phase 4 focus`.** The node no longer
marks anything, and a node called "mark dps" that does not mark is dead context. All four wiring sites
move: class declarations in the trigger/action headers, `creators[…]` in `UldTriggerContext.h` and
`UldActionContext.h`, the two static factories, and the `TriggerNode` in `UldStrategy.cpp`.

- Action, tank branch: drop `MarkTargetWithSkull`; `Attack(GetMimironPhase4Focus(botAI, bot, true))` when it
  differs from `"current target"`, else `false`. **Tanks hold at the floor like everyone else**: a
  `nullptr` focus means `bot->AttackStop()` and return `true`. With every melee DPS held there is no
  competing threat, so threat is static and the mech does not change hands, whereas a tank left
  swinging is a slow steady push toward 15000 on the one part nobody wants pushed. Comment to carry:
  this holds only because nothing else is generating threat — if a mech ever does change hands here,
  the branch goes back to swinging.
- Action, non-tank branch: unchanged (`disperse distance` 4.0 for Hand Pulse, then `true`).
- Trigger, tank branch: drop the `GetTargetIcon(RtiTargetValue::skullIndex)` comparison; fire on
  `AI_VALUE(Unit*, "current target") != focus`, plus the hold case — a `nullptr` focus while the bot is
  still swinging has to fire once so the action can stop it. `RtiTargetValue.h` stays included;
  `MimironAerialCommandUnitTrigger` still uses it.
- Trigger, non-tank branch: unchanged.

`MimironSetDpsPriorityAction`:

- `BuildPriorityList`: replace the raw-health mech sort with a single entry from
  `GetMimironPhase4Focus(botAI, bot, botAI->IsMelee(bot))` when `IsMimironPhase4`. Outside phase 4
  exactly one mech is up, so keep the
  existing behaviour — push whichever is alive. The add entries (Bomb, Assault, fire, Junk) are
  unchanged and still outrank the mechs; no adds spawn in phase 4 anyway.
- `IsAllowedTarget`: replace the `MOVEMENTFLAG_HOVER` test for `NPC_AERIAL_COMMAND_UNIT` with
  "melee never, **unless** this is phase 4 and fewer than three parts are still attackable". In phase 3
  that is what the hover test already achieved, without depending on a flag that leaks; in phase 4 it
  is the deliberate rule, and it lifts for the Self Repair window so melee are not stranded there.
- `Execute`: when `IsMimironPhase4` and the focus is `nullptr`, `bot->AttackStop()` and return `true`
  — hold the tick so no class strategy re-engages. Precedent: `StopAttackAction`
  (`VHActions.cpp:104-107`). Everything above `ACTION_RAID` still runs, so dodges are unaffected;
  only DPS stops. Healers never reach this path, because step 3 does not put them on the allowed set
  in the first place.

### F2 — `mimiron pet control` owns pets in phase 3 **and** phase 4

Pets are unmanaged for the whole of phase 4 today, in two different ways.
`MimironPetControlTrigger::IsActive` fires on "ACU alive and hovering", and the flag survives the
phase 3 defeat and the vehicle boarding, so in phase 4 it often fires, finds no adds, and calls
`StopPet` — every pet in the raid stops for the phase. Gate that flag correctly and the opposite
happens: `PetAttackAction`'s node is commented out globally (`CombatStrategy.cpp:53-55`), so a pet
enters phase 4 still holding a dead Junk Bot and never retargets. Either way it contributes nothing.

Widen the node rather than narrowing it. One node owning "where pets go in this encounter", still
returning `false` so it never eats the owner's tick:

- **Phase 3 branch** — the ACU is up and neither the MK II nor VX-001 is, matching
  `MimironAerialCommandUnitTrigger`. Nearest Assault Bot, then Junk Bot, then Bomb Bot, `StopPet` when
  none is up. Unchanged behaviour, correct gate.
- **Phase 4 branch** — `IsMimironPhase4`. `CommandPetAttack` on whatever `GetMimironPhase4Focus` hands
  a **melee** bot, so a hunter's pet goes where the warriors go, and `StopPet` when that comes back
  `nullptr`. Pets hold at the floor with the melee they are standing next to: a hunter's pet is about a
  fifth of that hunter's damage, and leaking that into the rendezvous is exactly what the floor exists
  to stop. The endgame lift comes along for free, so pets pile back on as soon as the first part is
  self-repairing.
- Otherwise inactive.

Comments to carry: the hover flag cannot be used on its own to mean "phase 3", because it survives
into phase 4 at random. And the phase 4 branch treats every pet as melee even though the Aerial
Command Unit is reachable there — seat offsets are zero, the chassis relocation writes passengers to
its own position, and nothing re-applies hover height, so it sits at ground level. Keeping pets on the
ground mechs is the split the raid wants, not a reachability workaround.

### F4 — Phase 3: melee chase their target, they do not hold a slot

`GetMimironPhase3Slot` hands melee and both tanks wedge rows at
`ULDUAR_MIMIRON_PHASE3_MELEE_RADIUS` (8) and 14 yd, and `MimironArcSpreadAction` walks them there at
`ACTION_RAID`. The Assault, Junk and Bomb Bot pads sit at roughly 17, 29 and 40 yd on the three arms,
so `mimiron set dps priority` hands a melee bot an add well outside the wedge, `reach melee` starts the
chase at `ACTION_HIGH` (20), and the formation drags it back at 60. Same deadlock as the phase 1 ranged
ring, aimed at melee this time — they never reach anything.

Melee get no phase 3 slot at all, matching what phases 1, 2 and 4 already do at
`UldBossHelper.cpp:2276`:

```cpp
// Melee stand on whatever they are hitting. Adds walk in from pads at 17, 29 and 40 yd on three
// separate arms, so any fixed melee slot is a spot the target is not in - and the formation runs at
// ACTION_RAID, above the chase, so it wins and the bot never lands a swing.
if (!PlayerbotAI::IsRanged(bot))
    return false;
```

Restructure the member loop to count only ranged, since `rangedCount` was its only other output, and
drop the now-unused `ULDUAR_MIMIRON_PHASE3_MELEE_RADIUS` and `ULDUAR_MIMIRON_PHASE3_MELEE_ROWS`.
`MimironWedgeRows` and `MimironWedgeSlot` stay — ranged still use them.

The ranged band stays at `ULDUAR_MIMIRON_PHASE3_MIN_RADIUS` (18). Do **not** pull it inward now that
the inner rows are empty: 18 yd is what keeps ranged off the add paths and outside contact.

Leave `disperse distance` off for phase 3. It was removed last round because the slots already held
everyone `ULDUAR_MIMIRON_PHASE3_SPACING` apart; that reasoning no longer covers melee, but melee are
now free-moving, and a disperse would be one more node fighting the chase for the same tick. Melee
already sidestep Bomb Bots through `MimironBombBotTrigger`, which is the only phase 3 mechanic that
punishes stacking.

### F5 — Form up during the phase handovers

A defeated mech sets `UNIT_FLAG_NOT_SELECTABLE`, which `AttackersValue::IsPossibleTarget` rejects, so
`GetFirstAliveUnitByEntry` stops seeing it — and the next mech carries the same flag until its own
phase starts. `GetMimironRingFocus` therefore returns `nullptr` across **47.75 s**, **24 s** and
**31.8 s** of handover and `MimironArcSpreadTrigger`'s "one of the three alive" gate fails. With
no trigger-driven action succeeding the engine falls through to its default action, and with follow
enabled that is `follow` at relevance **1.0**, so the raid spends every handover trailing its master
and then walks into the next phase from wherever that left it.

Nothing structural is in the way. `Creature::FindNearestCreature` is a grid scan that does not filter
on selectability; the Ulduar strategy is added to **both** engines (`PlayerbotAI.cpp:1793-1794`), so
`ACTION_RAID` nodes run out of combat; and 60 clears both `follow` at 1.0 and `drink`/`food` at
3.0 to 4.2 without any multiplier or ordering work.

New helper:

```cpp
// The mech to form up on when none of them is attackable yet. Handovers are long - 47.75 s from
// phase 1 to 2, 24 s to phase 3, 31.8 s to phase 4 - and the mechs sit NOT_SELECTABLE throughout, so
// "possible targets no los" cannot see them. Nothing else fires either, so the engine falls through
// to follow and the raid trails its master for the whole handover. A grid scan does see them, which
// is enough to walk everyone to where the next phase happens instead.
Unit* GetMimironStagingFocus(Player* bot);
```

Resolution order, all via `bot->FindNearestCreature(entry, 200.0f)` — 200 because the MK II parks
58 yd off centre between phases and a ranged bot can be another 40 out:

1. VX-001 present **and** `GetVehicleBase()` non-null → VX-001, phase 4 shape. It boards the chassis
   18.8 s into the 31.8 s handover, leaving 13 s to cross from a phase 3 wedge slot to a ring slot —
   about 6 s of walking at worst.
2. Aerial Command Unit present → the ACU, phase 3 shape.
3. VX-001 present → VX-001, phase 2 shape.
4. Otherwise `nullptr`.

`GetMimironSpreadSlot` falls through to this when `GetMimironRingFocus` returns nothing, and reuses
the existing shapes against it — the wedge for an ACU focus, the ring otherwise, both anchored on the
room centre where all three mechs end up. Two additions on the staging path only:

- Main tank staging for phase 4 → `ULDUAR_MIMIRON_PHASE4_TANK_SPOT`, since that branch currently
  requires all three attackable.
- Melee and tanks get a slot **while staging**, unlike during a live phase. There is no chase to
  fight yet, and arriving in melee range is the whole point:

```cpp
// Melee only get a staging spot, never a fighting one - see the phase 3 note. Eight yards is inside
// melee range of a combat-reach 8 mech the moment it goes live.
constexpr float ULDUAR_MIMIRON_STAGING_MELEE_RADIUS = 8.0f;
```

Index melee over living melee in group order, same pattern as the ring.

`MimironArcSpreadTrigger` drops its own "one of the three alive" gate entirely and lets
`GetMimironSpreadSlot` answer — it already calls it, and it returns false when neither focus resolves.
That is also what keeps the node quiet before the pull: the MK II is `NOT_SELECTABLE` until
`SetData(1, 1)`, and neither VX-001 nor the ACU exists yet.

Two behaviours worth recording rather than coding around:

- **The elevator knockback needs no guard.** `SPELL_ELEVATOR_KNOCKBACK` fires 11 s into the phase 1
  handover from a trigger on the elevator at (2744.56, 2569.35) — the room centre. VX-001 is not
  summoned until 17 s, and it is the staging focus, so nobody is standing there yet.
- **Staging does not cost the drink break.** The trigger only fires beyond
  `ULDUAR_MIMIRON_SPREAD_TOLERANCE`, so a bot walks to its slot and then yields the tick; `drink` and
  `food` at 3.0 to 4.2 pick it up once it arrives. Walking is 5–6 s out of a 24–48 s window.
- **This deliberately overrides follow for the handover.** A master who wants the raid moved between
  phases will find it walking back to formation. That is the same trade already made for every live
  phase, and the alternative is the raid arriving late to all three of them.
- All three mechs `DespawnOrUnsummon(7s)` at `EVENT_FINISH`, so the raid holds a ring for seven
  seconds after the kill and then stops.

Keep `IsMimironSpotSafe` on the staging move: phase 1 Proximity Mines outlive the phase.

### F6 — Phase 1: make the Plasma Blast tank swap actually swap

**Trigger, inverted.** `MimironPlasmaBlastTrigger` fires when the MK II is the only mech up, the bot is
the main tank or first assist tank, the bot is **not** the MK II's current victim, and the cannon is
**not** casting Plasma Blast. That gives the taunt roughly 19 s of lead so it owns the *next* cast,
instead of landing mid-cast where the victim is already resolved and nothing can move it.

Comment to carry: the cannon resolves its target when the cast begins, so a taunt during the cast is
always one cast too late; taunting between casts is the only timing that changes who gets hit.

**Action, one tick.** Fold the `Attack` into the same `Execute` rather than spending a tick on it:
set the target if it is wrong and go straight to `DoSpecificAction("taunt spell", event, true)`. Taunt
sets the taunter's threat equal to the current highest, so the swap holds as long as the new tank keeps
swinging; the 19 s of lead is what makes that possible. `"taunt spell"` is registered for all four tank
specs, so no class gap.

Taunt DR needs no handling: the MK II carries `OBEYS_TAUNT_DIMINISHING_RETURNS`, but DR resets after
15 s and the cadence is 22 s, so each tank taunts at full duration and each individual tank only
taunts every 44 s.

**New `MimironThreatRedirectGuardMultiplier`** in `UldMultipliers.h` / `.cpp`, registered beside the
other two:

```cpp
float MimironThreatRedirectGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Misdirection and Tricks of the Trade both hand their threat to the main tank by name, which is
    // the one tank the Plasma Blast swap is trying to move off. Twenty-odd seconds of a hunter's and a
    // rogue's threat is more than a 3 s taunt buys back, so the swap never sticks while these run.
    std::string const& name = action->getName();
    if (name != "misdirection on main tank" && name != "tricks of the trade on main tank")
        return 1.0f;

    return GetFirstAliveUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
                   !GetFirstAliveUnitByEntry(botAI, NPC_VX001) &&
                   !GetFirstAliveUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT)
               ? 0.0f
               : 1.0f;
```

Match on the name rather than the type: `dynamic_cast<MainTankActionNameSupport*>` would also catch
every blessing and buff that happens to target the main tank.

Phase 1 only. Phase 4 has its own tank rules and no swap, and phases 2 and 3 have no melee tanking at
all, so the redirects stay useful everywhere else in the encounter.

### F7 — Docs

`docs/raids/ulduar.md`, Mimiron section:

- The 4 s Spinning Up lead. Why latching at Spinning Up start is wrong, what 42.6° of drift does to
  the band, and why the far side of the room was the blind spot rather than the near side.
- The DB Target orbit as measured: 19 waypoints, 707 yd, 20.8988 yd/s, 33.8 s lap, 10.64 °/s
  clockwise, circle fit 2.7 yd off the room centre. That the fixed rate only holds for a VX-001 at the
  centre, and swings 8.3–14.7 °/s at 30 yd off it.
- Why the danger band cannot be folded to (−π, π]: it is 240° wide.
- `"find target"` walking the threat list, and VX-001 phase 4 never calling `DoZoneInCombat` — the
  general rule being that raid nodes resolve bosses by entry.
- Phase 4: Self Repair is a 15 s cast and all three must be inside it together; the ACU's 200 against
  300 `HealthModifier` and why percent is the only correct ordering.
- That vehicle seats 3886/3806 have zero attachment offsets, so all three parts share one point
  server-side — melee cleave splashes all of them, which is what the 10 % floor is for — and that
  melee are nonetheless kept off the ACU by rule.
- `MOVEMENTFLAG_HOVER` surviving into phase 4, and the two nodes it silently broke.
- Pets: that `PetAttackAction` is disabled globally so every encounter with a target switch has to
  redirect them by hand, and that the phase 4 Aerial Command Unit is reachable at ground level — pets
  are kept on the ground mechs by choice, to hold the split, not because they cannot reach it.
- The phase 1 tank swap: that the MK II is fully tauntable and the immunity set proves it, that the
  cannon resolves its victim at cast start so a mid-cast taunt is always one cast late, that 3 s of
  taunt cannot hold a 22 s rotation, and that `misdirection on main tank` / `tricks of the trade on
  main tank` were actively undoing the swap. Plasma Blast's real numbers: 17000/s for 6 s, no
  stacking, every 22 s.
- Phase 3 melee: why a formation slot for melee is always wrong when the targets are adds walking in
  from 17–40 yd, and that the deadlock is the same one the phase 1 ranged ring had, one relevance
  band down.
- The handover lengths (47.75 / 24 / 31.8 s), that a defeated mech goes `NOT_SELECTABLE` and so
  vanishes from `"possible targets no los"` while a grid scan still sees it, and that instance
  strategies are attached to the non-combat engine as well — which is what makes staging possible at
  all.

Mirror this plan to `docs/plans/mimiron-cheat-free-rebuild/mimiron-p4-barrage-and-focus.PLAN.md`.

---

## Files touched

- `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` — B2 constants and window helper, F1 phase 4 helpers,
  F4 phase 3 melee, F5 staging focus and slots, removal of the latch
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.h` / `.cpp` — B1, F1 rename and trigger body, F2 pet
  gate, F5 spread gate, F6 inverted Plasma Blast gate
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.h` / `.cpp` — B1, B3, F1, F2 phase 4 pet branch, F6 taunt
- `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp` — B2 charge guard unchanged, F6 threat redirect guard
- `src/Ai/Raid/Uld/UldTriggerContext.h`, `UldActionContext.h`, `UldStrategy.cpp` — F1 rename wiring,
  F6 multiplier registration
- `docs/raids/ulduar.md`, `docs/plans/mimiron-cheat-free-rebuild/…PLAN.md` — F7

## Verification

The module cannot be compiled from this checkout; the worldserver build is the only compile path.

1. Build, then pull Mimiron with the DB Target present
   (`SELECT COUNT(*) FROM creature WHERE id=33576` returns 1 — the fallback path is only for when it
   is not).
2. **Barrage, phase 2.** Through a full Spinning Up, bots move *before* the beams appear and are
   still clear when they ignite. Nobody stops in the 43° slice clockwise of where the boss was
   pointing when it started spinning — that is the band the old model marked safe. No Laser Barrage
   entries in the combat log.
3. **Barrage, phase 4.** Same, with the chassis deliberately dragged 20–30 yd off centre first.
   Confirm bots that have never damaged VX-001 (a healer, a melee locked on the MK II) still dodge —
   that is the `"find target"` fix.
4. **Phase 4 tank.** The main tank orbits on a tight ring during the barrage and the MK II does not
   walk across the room following it; the tank returns to `ULDUAR_MIMIRON_PHASE4_TANK_SPOT` after.
5. **Phase 4 evening-out.** Watch the three health bars: they track within roughly 10 % of each other
   all the way down rather than the ACU trailing. Ranged are on the ACU, melee alternate between MK II
   and VX-001.
6. **The floor.** When MK II and VX-001 reach ~10 % and the ACU is still above it, melee **and both
   tanks** stop swinging and stand — they should still dodge Shock Blast, Rocket Strike and the
   barrage while holding, and healers should keep healing throughout. Confirm no mech changes hands
   while the tanks are idle, and that no part drops under 15000 and self-repairs alone.
7. **The finish.** As soon as the first part goes into Self Repair, melee pile onto whatever is left,
   the ACU included. All three cross into Self Repair inside 15 s of each other and the encounter
   ends.
8. **Pets.** In phase 3 they are on the adds, Assault Bot first. In phase 4 they are on whichever
   ground mech the melee are on, they stop when the melee hold at the floor, and they come back as
   soon as the first part is self-repairing. Nothing is parked under the hovering ACU in phase 3.
9. **Phase 3 melee.** Melee and both tanks walk out to whatever `mimiron set dps priority` gave them
   and stay on it — no snapping back toward the staging point mid-swing. Ranged and healers still hold
   the wedge from 18 yd out and nobody stands on a summon arm.
10. **Phase 1 tank swap.** Over three Plasma Blast cycles the two tanks alternate: whoever is not the
    victim taunts between casts, and the next 102000-over-6 s lands on the other one. Confirm the
    taunt lands (no immune message), that it sticks for the full 19 s rather than snapping back after
    3 s, and that hunters and rogues stop casting Misdirection and Tricks on the main tank while the
    MK II is up but resume in phases 2 to 4.
11. **Handovers.** Time each one. The raid should be standing in the next phase's formation *before*
    the boss goes active, not walking into it afterwards: the phase 2 ring formed by the time VX-001
    rises, the phase 3 wedge by the time the ACU drops, melee inside 8 yd of the room centre for the
    phase 4 assembly, main tank on `ULDUAR_MIMIRON_PHASE4_TANK_SPOT`. With follow enabled, bots must
    break off from the master rather than trailing him. Confirm nobody is knocked off
    the elevator 11 s into the phase 1 handover, that bots still eat and drink once they arrive, and
    that they stop holding formation about seven seconds after the kill.
12. Cheat-free grep: `HasCheat`, `TeleportTo`, `->Kill(` return nothing across `UldTriggers_Mimiron.*`,
   `UldActions_Mimiron.*` and the Mimiron parts of `UldBossHelper.*`.
13. Full clear on normal, then Firefighter.

## Known and deliberately out of scope

- Melee never attack the Aerial Command Unit while the phase 4 rendezvous is on, by decision — even
  though seat offsets of zero mean they could reach it. The bar lifts once the first part is
  self-repairing, so the cost is idle melee DPS only while the raid is levelling the three out.
- Tanks hold at the floor with everyone else. This is only safe because nothing else is generating
  threat at that point; it is the first thing to revisit if a mech changes hands in testing.
- No tank holds ACU threat, in phase 3 or phase 4. Plasma Ball is ~10000 single target and a ranged
  bot survives it.
- Redirecting Misdirection and Tricks to the MK II's *current* victim rather than suppressing them
  outright. Strictly better, but it means touching the hunter and rogue target values, and it belongs
  in its own change.
- Sweeping `"find target"` out of the other raid strategies. Same threat-list defect, a dozen
  instances, not this round.
- W6 Napalm Shell spread, carried from the first round, still awaiting a decision.


---

## As built — where the implementation departed from the plan above

Five changes, all found while building or simulating the work, none of them reopening a settled
decision.

**B3, the escape direction, was rebuilt twice.** The plan chose direction on travel time with a 1.5x
bias toward counter-clockwise. Simulating the shipped logic against the real waypoint path showed that
losing 12 % of bot positions in phase 2 and 16 % in phase 4, because distance is the wrong currency:
for a bot the cone has not reached yet, counter-clockwise is the shorter path *through* 104 degrees of
beam. The rule is now chosen on **time spent inside the cone**, in three cases — ahead of the sweep is
always clockwise, inside the cone takes whichever edge is sooner, and counter-clockwise of the
centreline rides the sweep out. Applying the sweep rate only once the beams are live (it is zero
through Spinning Up, where the boss aims once and holds) was the last correction. Final sweep:
**0 hits of 497,664 positions**, across every DB Target phase, bot bearing, orbit radius 14-24 yd and
chassis offset out to 30 yd in eight directions.

**`IsMimironPhase4` does no grid scan.** As planned it called `FindNearestCreature` at 200 yd, and it
is asked several times per bot per tick from the target list, the tank node and the pet node. It now
answers from the cached target list — VX-001 or the Aerial Command Unit riding something covers all of
the phase bar the last seconds, and when only the chassis is left, seat 3 holds the cannon in phase 1
and VX-001 from phase 4 on.

**Healers are excluded from the phase 4 hold explicitly.** The plan asserted they could never reach it
because the focus helper would not put them on the allowed set. That was wrong: their allowed set is
the two ground mechs like any melee, so they do reach the floor, and holding the tick there would have
stopped them healing. The hold is now gated on `!botAI->IsHeal(bot)`.

**Staging melee slots count main tanks.** The slot generator initially skipped them the way the ranged
ring does, which left a staging main tank in phases 2 and 3 taking index 0 and colliding with whoever
actually held it. Only a phase 4 main tank has a spot of its own, and that is handed out earlier.

**`ULDUAR_MIMIRON_BARRAGE_SPIN_SECONDS` was dropped.** The spin duration is always read off the aura,
so the constant had no caller.
