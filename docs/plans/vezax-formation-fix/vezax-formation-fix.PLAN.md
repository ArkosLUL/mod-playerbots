# General Vezax — formation fix, Shadow Crash dodge, mana economy, RaidObs

Follow-up to the Vezax rework that shipped in `35d8e26bb`. The `vezax-guide-parity` plan doc was
folded into `docs/raids/ulduar.md` when that work landed; this is the correction pass on top of it.

## Context

Four reports against the shipped build:

1. Ranged and healers position themselves **behind the boss**.
2. The Wowhead guide has a better layout than the three concentric arcs.
3. The fight's loop is: ranged split into **two groups**, dodge the Shadow Crash projectile, then
   stand in the field it leaves to cut mana cost; healers stay **near melee**, because the same field
   cuts healing done by 75%.
4. Whoever runs out of mana (tanks excepted) handles Saronite Vapors.

Plus: fold in RaidObs so future pulls can be diagnosed from a trace.

Hard mode (Stage 3) stays out of scope. It leaves a known gap: with hard mode on nothing kills vapors,
so no puddles drop and the mana economy has no source.

## Root cause of #1

`ULDUAR_VEZAX_ARC_ORIENTATION = -1.5291f` points **south**, and its comment claims the raid enters by
the southern door. That door is the exit:

| Evidence | Source |
|---|---|
| `{ GO_VEZAX_DOOR, BOSS_VEZAX, DOOR_TYPE_PASSAGE }` — 194750 opens when he **dies**, on the way to Yogg | `instance_ulduar.cpp:48` |
| Vezax spawns `(1852.78, 81.39, 342.46)` orientation **1.658** — facing north | `creature`, map 603 |
| Room trash (33818/33819/33820/33822/33823) spans x 1780–1898 at **y 109–137** | `creature`, map 603 |

Every ranged and healer slot sits between the boss and the exit.

## Verified facts

| Fact | Source |
|---|---|
| 62660: `CastingTimeIndex 1` (instant), `Speed 10` yd/s, effects `32/32` TRIGGER_MISSILE → 62659 + 63277 | `spell.reference.csv` |
| 62660 `ImplicitTargetA = 53` `TARGET_DEST_TARGET_ENEMY` — destination frozen at cast time | `spell.reference.csv` |
| A delayed spell stays in `CURRENT_GENERIC_SPELL` for the whole flight; `UNIT_STATE_CASTING` is cleared | `Spell.cpp:4016-4020` |
| 62659: `Effect_1 = 2` 11310 damage **+ `Effect_2 = 98` KNOCK_BACK_DEST**, radius index 13 = **10 yd** | `spell.reference.csv` |
| 63277 field: radius index 14 = **8 yd**; +100% magic dmg, **−75% healing**, +75% shadow | `spell.reference.csv` |
| `spell_linked_spell 63277 → 65269 type 2` (`SPELL_LINK_AURA`) — a bot in the field carries **both**, so `HasAura(63277)` is valid | `acore_world` |
| 65269: +100% cast speed, `MOD_POWER_COST_SCHOOL_PCT` misc **127 — all schools, physical included** | `spell.reference.csv` |
| 62662 Surge of Darkness `ImplicitTargetA = 1` (self) — the −55% speed is on **Vezax** | `spell.reference.csv` |

**Target-exclusion radius is 12.5 yd.** `SelectTarget(Random, 0, -3.0f, …)`
(`boss_general_vezax.cpp:210`) rejects on `IsWithinCombatRange(target, 3.0)` (`UnitAI.h:85`), which
adds **both** reaches (`Unit.cpp:766-780`): `3 + 8 (Vezax CombatReach) + 1.5 (player)`. Tank and melee
are structurally untargetable.

**Ranges** (effective, `docker exec ac-worldserver env`): `HealDistance 38.5`, `SpellDistance 28.5`,
`MeleeDistance 0.75`, `LowMana 15`, `SightDistance 100`. Melee park at ≤10.25 yd.
`ReachTargetAction`'s leash is `28.5 + 9.5` = **38 yd**.

**navprobe, map 603** — all 24 slots, tank spot and mark spots on mesh, `distance to poly`
0.002–0.047, `settledZ` 342.37–342.39. Flat WMO floor; raw terrain reads −27.707, so terrain height is
meaningless here. The hall reaches 70 yd north and west; southern diagonals go off-mesh past ~50.

**Cost.** `PossibleTargetsValue` inherits `checkInterval = 1` and `CalculatedValue::Get()`
recalculates on **every** call at that interval (`Value.h:73-74`) — a 100 yd `ignoreLos` sweep. And
`Engine.cpp:219-221` runs every multiplier against every action in every pass, so
`VezaxControlMovementMultiplier` → `VezaxFormationActive` → `GetVezax` fires that sweep tens of times
per bot per tick.

## Design

### 1. Face the entrance

`ULDUAR_VEZAX_ARC_ORIENTATION = 1.5708f`. `TryGetVezaxMarkSpot`'s fallback spots derive from it and
flip with it. `ULDUAR_VEZAX_ARENA_RADIUS` stays 45 — outside the bubble the multiplier is inert, so
generic movement carries bots in and the gate opens on arrival; raising it risks reintroducing the
wall-running it was added to stop.

### 2. Blocks, a tank slot, a boss-relative healer ring

One repeated block: centre bearing, rows at fixed radii, 3 slots per row at fixed chord spacing. Row
arc width is derived — `width = (slots − 1) * spacing / radius` — and fed to `VezaxArcSlotAngleOffset`.

| Block | Centre | Rows | Slots | Spacing | Tolerance | Centre point |
|---|---|---|---|---|---|---|
| Tank | — | — | 1 | — | 3.0 | anchor |
| Healers | `B` | 11.25 | 6 | 4.5 | 0.8 | **live boss** |
| Ranged L | `B + 1.0` | 24.5, 27.5 | 2×3 | 3.7 | 1.2 | anchor |
| Ranged R | `B − 1.0` | 24.5, 27.5 | 2×3 | 3.7 | 1.2 | anchor |
| Overflow L/R | `B ± 1.0` | 33.5 | 3 each | 3.7 | 1.2 | anchor |

- **Healers are boss-relative** because the 12.5 yd exclusion is measured from the boss. Anchor-derived,
  a ranged pull can leave Vezax ~13 yd north and the southern healers silently become eligible again.
  Bearings stay anchor-relative so the arc still faces the entrance. `11.25 ± 0.8` gives a live band of
  `[10.45, 12.05]` — clear of the 10.25 yd melee ring, 0.45 yd inside the line.
- **Groups at ±1.0 rad, not ±0.873**: at 0.873 the nearest healer slot was 8.49 yd from a ranged slot,
  inside the field. At 1.0 it is 13.25, and group centres are 43.8 yd apart.
- **Group max-internal 7.97 yd**, just under the 8 yd field, so a field on any slot covers all six at
  rest. Under the 1.2 tolerance it degrades to ~5 of 6; the guarantee is not reachable under drift and
  chasing it with a ~0.3 tolerance would starve casts. Shipped behaviour covers 2 of 12.
- **Overflow row is deliberately outside the guarantee**, 9 yd off the near row. A 13th ranged bot gets
  a real slot instead of falling through to the melee de-clump and walking into the boss.

**Fill order** alternates groups slot-by-slot (`L near 0, R near 0, L near 1, …`), then far rows, then
overflow. `EnsureVezaxSlotAssignments` takes the lowest free *suitable* index, so this needs an
explicit order table, not the raw index.

**Slot index space**, documented in the header because the trace writes the raw number: `[0,6)`
healers, `[6,12)` L, `[12,18)` R, `[18,21)` L overflow, `[21,24)` R overflow. The tank is not in the
assignment map — one holder, handled directly in the position action.

Melee keep the unanchored 4 yd de-clump: no crash can reach them, and spreading them wider pushes them
through the healer ring on the same ~10 yd circle.

### 3. Shadow Crash — dodge, then soak

New node `vezax shadow crash dodge`, ranged and healers only. Melee stay stacked and the main tank
never moves, so the boss is never dragged toward the berserk bounds (`x ∈ [1720,1940]`, `y ∈ [20,210]`).

Detection reads the delayed spell — the boss is never in `UNIT_STATE_CASTING`, so do not gate on it:

```cpp
Unit* boss = GetVezax(botAI);
Spell* spell = boss ? boss->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
if (!spell || spell->m_spellInfo->Id != SPELL_VEZAX_SHADOW_CRASH_CAST)
    return false;
```

Impact = `m_targets.GetDstPos()` when `HasDst()`, else the unit target's position.

**Per-bot clear, not a group shift.** A rigid translation clearing an 8 yd block from a 10 yd impact
needs ~18 yd in the worst orientation — 2.6 s at 7 yd/s, the whole flight, with nothing left for the
return. Each bot calls `FindNearestPositionClearOfHazards(bot, {dst}, 12.0f, 25.0f)` (~1.7 s) at
`MOVEMENT_FORCED`. Not `FleePosition` — it clamps to `FleeDistance` (5.0). Not "stand and eat it" —
the impact knocks back, so the block scatters either way.

**Radial band on the destination**, because the helper returns the nearest clear point and would
otherwise push a caster into the inner ball: ranged 16–36 yd from the **anchor**; healers 0–12.0 yd
from the **boss**, since a floor of 16 would throw a dodging healer out of the exclusion.

**Position and soak decline while an impact covers their destination**, mirroring
`AlgalonRaidPositionAction` (`UldActions_Algalon.cpp:245-249`). Without it the bot clears, the same
tick walks it back with the missile still in flight, and it ping-pongs — and a moving bot never casts.

**Delete `VezaxShadowCrashClearTrigger` / `Action`.** It strafes a constant arc step around the boss,
dropping a healer at an arbitrary bearing the position action then has to undo. Healers already treat
fields as hazards in `VezaxBuildAvoidPositions`, so a covered slot is "buried" and `TryGetVezaxSlot`
reassigns, with "step off the hazard" as the fallback.

**Hunters join the soak** — aura 72 carries misc 127, all schools, so their shots get the same −70%.
They get no damage boost.

`InterruptVezaxCastersNear` takes a radius parameter; for 62660 it covers everyone the dodge will move,
at the 10 yd impact radius rather than the 8 yd hazard radius.

### 4. Vapors handled by whoever is out of mana

`VezaxIsVaporHandler` replaces `VezaxIsVaporKiller`: eligible is `!IsTank`, `GetMaxPower(POWER_MANA) > 0`,
mana% `< 10`; ranked healers first, then ascending mana%, guid tiebreak; capped at 2. Nobody eligible
means nobody kills and the vapor despawns — the puddle costs health on a `100 * 2^stacks` curve and is
only worth paying for when the mana is needed.

`VezaxKillVaporAction` closes to 5 yd before attacking so the puddle drops at the handler's feet.

**The puddle is a hazard for everyone else.** Vapors spawn on the boss and `MoveRandom(4.0f)`, so an
8 yd puddle covers the tank, melee and most of the healer ring, and `100 * 2^stacks` would land on up
to 13 bots. Only the handlers and anyone under `lowMana` may stand in one. The HP-predictive exit is
unchanged. `VezaxVaporSoakTrigger` stays on `lowMana`: the handler at <10% goes and *makes* a puddle,
anyone under 15% already near one should use it.

### 5. Mark of the Faceless

Ranged step out radially +18 yd along their own bearing, **capped at 44 yd** — past
`ULDUAR_VEZAX_ARENA_RADIUS` the formation gate goes false for that bot, the multiplier hands its
generic movers back and it wanders. Healers, melee and the tank keep the three fixed rear spots; the
core prefers a target >15 yd out whenever ≥9 (25m) / ≥4 (10m) players are there, and 12 ranged always
clear that bar, so that path is a corner case.

### 6. Node ladder — renumbered, no ties

| Node | Priority |
|---|---|
| `vezax reset encounter state action` | `ACTION_EMERGENCY + 10` |
| `vezax shadow crash dodge action` | `ACTION_EMERGENCY + 9` |
| `vezax searing flames interrupt action` | `ACTION_EMERGENCY + 8` |
| `vezax vapor puddle clear action` | `ACTION_EMERGENCY + 7` |
| `vezax mark of the faceless action` | `ACTION_EMERGENCY + 6` |
| `vezax surge of darkness action` | `ACTION_EMERGENCY + 5` |
| `vezax saronite animus action` | `ACTION_RAID + 5` |
| `vezax vapor soak action` | `ACTION_RAID + 4` |
| `vezax kill vapor action` | `ACTION_RAID + 3` |
| `vezax shadow crash soak action` | `ACTION_RAID + 2` |
| `vezax shadow resistance action` | `ACTION_RAID + 1` |
| `vezax raid position action` | `ACTION_RAID` |

The interrupt moves above the puddle clear: interrupters are mostly melee, who stand where puddles
drop, and trading a 2 s interrupt for a puddle step lands 13875–16125 fire on the whole raid every 8 s.
**Guarded** — the interrupt yields when `VezaxShouldLeaveVaporPuddle` says the next tick is dangerous,
so the bot only eats a tick it was going to survive. Without the guard a stack-8 tick (25600) kills it.

Kill-vapor sits above shadow-crash-soak: the handler is by definition out of mana, and a cost reduction
is worth nothing with no mana to spend.

### 7. Multiplier

`VezaxControlMovementMultiplier` no longer exempts the main tank — it holds a slot now, and the
exemption would let `FollowAction` fight the tank position. `AttackAction` and `ReachTargetAction` stay
exempt, which is what keeps the tank in melee; melee remain exempt via `!IsRanged`. `encounterMovers`
gains the dodge action and loses the clear action.

### 8. Boss lookup off the grid sweep

`GetVezax` reads the instance object map instead of `possible targets no los`.
`instance_ulduar.cpp:115` registers `{ NPC_VEZAX, BOSS_VEZAX }`; `EoEEncounter_Malygos.cpp:200-211` is
the in-repo precedent, including its liveness check — `GetCreature` has no liveness filter of its own,
unlike `GetFirstAliveUnitByEntry`. `BOSS_VEZAX = 11` is mirrored into `UldScripts.h` as
`ULD_DATA_VEZAX`, because core script headers are not on a module's include path. Vapor and Animus
lookups keep the sweep; both sit behind cheap gates.

### 9. RaidObs

- **The in-flight missile.** During the 1.8–2.6 s flight the impact has no world object, which is what
  `NoteHazard` exists for. Emit `NoteHazardCircle(map, 62659, dst, 10.0f, ttl)` from
  `VezaxHazardListenerScript::OnSpellCast` — it already fires exactly once, server-side, for 62660 and
  already resolves the target position. In the per-bot helper it would write ~18 duplicate rows per
  missile. TTL from `Spell::GetDelayMoment()`, falling back to `dist(boss, dst) / 10`.
- **Four designations**: formation gate, vapor handler, Searing Flames interrupter, group.
  `ObsValue`/`ObsGuidSet` where the state is stored, `NoteDerived` inside the deriving helper where it
  is not — two probes at two call sites can disagree about what was decided. `NoteDerived` takes
  `std::string const&`, so the value is built before `Active()` is tested: guard with
  `if (!RaidObs::Active())` where formatting is non-trivial.
- Per-tick dodge/soak state is not probed — already visible as `act` verdicts against `snap` positions.
- `MarkPull` is not needed: `BossAI::_JustEngagedWith` sets `IN_PROGRESS` and opens the session.

## Files

| File | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldScripts.h` | `ULD_DATA_VEZAX` |
| `src/Ai/Raid/Uld/Util/UldBossHelper.h` | Flip `ARC_ORIENTATION`, fix both stale door comments; block/tank/dodge/band/vapor constants in; dead arc, strafe and vapor-killer constants out |
| `src/Ai/Raid/Uld/Util/UldEncounter_Vezax.{h,cpp}` | Block geometry; boss-relative healer ring; fill order; tank slot; overflow row; `GetVezax` via instance script; `VezaxIsVaporHandler`; `TryGetVezaxShadowCrashImpact`; mark cap; obs probes |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_Vezax.{h,cpp}` | Add the dodge trigger; delete the clear trigger; interrupt yields on a dangerous puddle tick; kill-vapor onto the handler predicate |
| `src/Ai/Raid/Uld/Action/UldActions_Vezax.{h,cpp}` | Add the dodge action; delete the clear action; kill-vapor closes to 5 yd; position action gains the tank slot and the decline-while-impact rule |
| `src/Ai/Raid/Uld/UldTriggerContext.h`, `UldActionContext.h` | Register the dodge, drop the clear |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | Renumbered ladder |
| `src/Ai/Raid/Uld/UldMultipliers.cpp` | Tank no longer exempt; `encounterMovers` updated |
| `src/Ai/Raid/Uld/Util/UldBotScripts.cpp` | Radius parameter; 62660 covers all dodgers at 10 yd; `NoteHazardCircle` |
| `docs/raids/ulduar.md` | Vezax chapter rewrite; correct the Profound Darkness radius and the Shadow Crash dodge description; clear the two open-gap rows |

Every added or removed node name must change in all three places — strategy node, trigger context,
action context — in one edit. Name lookups fail silently (`docs/engine/pitfalls.md`).

## Verification

1. `python apps/codestyle/codestyle-cpp.py` on every touched file.
2. Build is a hand-off — the module cannot be compiled headless here.
3. navprobe is done for all 24 slots, the tank spot and the mark spots. Re-probe only if a radius or
   bearing changes; the mark cap at 44 yd on the overflow bearings is the one unprobed point.
4. Confirm hard-mode and obs knobs with `docker exec ac-worldserver env | grep ^AC_`.
5. **25-man:** two ranged groups north of the boss ~44 yd apart, healers ringing him inside 12.5 yd,
   nobody walking past him. Vezax holds within a few yards of `(1852.78, 81.39)`.
6. **Shadow Crash:** confirm from the trace that no crash ever targets a healer or a melee — that is
   the basis of the inner ring. A crash on one group scatters that group only; the field lands; its
   casters walk back and hold. Watch for oscillation between dodge and soak once the field is down.
7. **Mana:** healers dip under 10%, walk to a vapor, kill it at 5 yd, soak, leave on the predicted
   lethal tick. With nobody low, no vapor is killed, and nobody else rides a puddle.
8. **Interrupt:** Searing Flames kicked on every cast with one kick spent, including while the
   interrupter stands in a puddle at a survivable stack.
9. **10-man:** 15 s Searing Flames cadence; blocks under-fill from each row's centre outward.
10. **Trace:** `postmortem.py <file> --notes vezax` shows slot, displaced, handler, interrupter, group
    and formation-gate transitions; `grep '"e":"haz"'` shows one circle per crash, named `Shadow
    Crash`, not a bare `62659`.
11. Wipe and re-pull twice; no assignment carries over.
