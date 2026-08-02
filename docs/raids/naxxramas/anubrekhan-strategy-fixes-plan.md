# Anub'rekhan (Naxxramas) Playerbot Strategy Fixes

## Context

Bots wipe on Anub'rekhan. Reported symptom: ranged bots stack in the centre of the room and all of
them get hit by Impale.

That symptom is directly coded. `AnubrekhanPositionAction::Execute`
(`src/Ai/Raid/Naxx/Action/NaxxActions_Anubrekhan.cpp:125`) tells every non-main-tank to
`MoveInside(533, 3272.49f, -3476.27f, z, 3.0f)` — a deliberate 3-yard ball at room centre — for the
whole Locust Swarm. Outside the swarm the action returns `false` and does nothing at all, and the
generic de-clumper is inert by default: `DisperseDistanceValue` defaults to `-1.0f`
(`src/Bot/Engine/Value/Value.h:386`) and `CombatFormationMoveAction::Execute` bails on `dis <= 0`
(`src/Ai/Base/Actions/MovementActions.cpp:2338`). So the raid is clumped for the entire encounter.

Anub'rekhan is the thinnest strategy in the Naxx folder: 1 trigger, 2 actions, 1 multiplier, no boss
helper, ~130 lines. This plan brings it up to the standard set by the Heigan and Kel'Thuzad work.

**Decided with user (do NOT revisit):**
- **Full rework**, not a minimal spread patch.
- **Deterministic slot ring** for spread (Hyjal / Loatheb style), not reactive `FleePosition`.
- **During Locust Swarm, non-tanks follow the kite at safe range** — they do not park at the centre
  and they do not get a shrunken kite circle.

## Verified encounter facts (core side)

From `src/server/scripts/Northrend/Naxxramas/boss_anubrekhan.cpp` and
`ScriptedAI::ScheduleTimedEvent` (`src/server/game/AI/ScriptedAI/ScriptedCreature.cpp:349-370`),
both under `g:\DevStuff\GitHub\azerothcore-wotlk-pb`:

| Fact | Detail |
|---|---|
| Impale | `SPELL_IMPALE = 28783` (10) / `56090` (25, via `data/sql/base/db_world/spelldifficulty_dbc.sql:83`). `DoCastRandomTarget` → `SelectTarget(Random, 0, 0.0f, playerOnly=true, withMainTank=true)`. **Uniformly random living player, tank NOT excluded, no range filter.** Damage lands in an area around the victim — hence the whole stack dies. |
| Impale clock | `ScheduleTimedEvent(15s, fn, 20s)` resolves to `Schedule(15s, 15s)` then `Repeat(20s)` → **exactly 15s after engage, then exactly every 20s.** Fully deterministic. |
| Locust Swarm | `28785` (10) / `54021` (25, `spelldifficulty_dbc.sql:84`). Self-cast aura on the boss, ~15 yd, ~20s. First cast **random 70–120s**, then exactly every 90s. `EMOTE_LOCUST` fires on the same tick as the cast — **zero warning**. Boss is not slowed, rooted or threat-wiped: it keeps chasing its victim, so this is a kite. |
| Crypt Guards | `NPC_CRYPT_GUARD = 16573`. 25-man: 2 pre-spawned on `Reset()`. 10-man: 1 at engage + 17.5s. Both modes: 1 more at every Locust Swarm + 3s, at `(3331.217, -3476.607, 287.074)`. |
| Corpse Scarabs | 10 on Crypt Guard death (`28864`), **5 from every dead player's corpse** (`29105`). A wipe cascades. |
| Room | Floor Z is `287.077`. Boss spawns `(3308.59, -3476.29, 287.161)`. Crypt Guard spawn points sit at r=38 and r=58.7 from the strategy's centre point, so radius-45 is inside the room — geometry is not the bug. |

## Why nothing currently saves the bots

- Impale is not in `NaxxSpellIds.h` at all. No trigger, action or multiplier references it.
- The generic `avoid aoe` action **cannot see Impale or Locust Swarm**. All three of its cases
  (`MovementActions.cpp:1926` / `:1980` / `:2050`) need a dynobject aura, a damaging trap GameObject,
  or a `UNIT_FLAG_NOT_SELECTABLE` trigger NPC. Impale creates none of those, and the boss is
  selectable. → **the fix must be pre-emptive spread, not reactive avoidance.**
- Second wipe vector: during Locust Swarm the MT kites at **radius 45** from `(3272.49, -3476.27)`
  while everyone else parks at the centre. That is 45 yd boss-to-raid — outside caster range and
  outside heal range. Ranged contribute nothing and the MT goes unhealed for ~20s every 90s.
- `AnubrekhanGenericMultiplier` (`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:441`) only zeroes
  `FleeAction`. It does not suppress `CombatFormationMoveAction`, unlike Grobbulus (`:43`) and Heigan
  (`:56`), so the generic mover fights the positioning action.
- `AnubrekhanChooseTargetAction` never checks `IsAlive()` on units pulled from `"attackers"`, and its
  "lowest HP Crypt Guard" loop makes DPS re-target every tick as adds trade places.

## Current code map

All paths relative to `g:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots`.

| File | Content |
|---|---|
| `src/Ai/Raid/Naxx/Action/NaxxActions_Anubrekhan.cpp` | 2 actions: choose target `:15-101`, position `:103-129` |
| `src/Ai/Raid/Naxx/Action/NaxxActions.h:353-368` | Anub action declarations; `AnubrekhanPositionAction` inherits `RotateAroundTheCenterPointAction` centre `(3272.49, -3476.27)` r45, 16 waypoints |
| `src/Ai/Raid/Naxx/NaxxTriggers.h:138-143` / `NaxxTriggers.cpp:349-356` | `AnubrekhanTrigger` — whole-fight, no phase discrimination |
| `src/Ai/Raid/Naxx/NaxxTriggerContext.h:46`, `:102` | trigger registration |
| `src/Ai/Raid/Naxx/NaxxActionContext.h:49-50`, `:111-112` | action registration |
| `src/Ai/Raid/Naxx/NaxxMultipliers.h:91-98` / `.cpp:432-449` | `AnubrekhanGenericMultiplier` |
| `src/Ai/Raid/Naxx/NaxxSpellIds.h:126-129` | Locust ids only. `LocustSwarm10Alt = 28786` is dead — the core never casts it |
| `src/Ai/Raid/Naxx/NaxxStrategy.cpp:59-65` | trigger node; `:241` multiplier registration |
| `src/Ai/Raid/Naxx/NaxxBossHelper.h` | **no `AnubrekhanBossHelper`** — helpers exist only for Kel'Thuzad `:112`, Razuvious `:772`, Sapphiron `:799`, Gluth `:1038`, Heigan `:1127`, Loatheb `:1321`, Four Horsemen `:1447`, Thaddius `:1554` |

Reusable machinery this plan leans on:

| Helper | Location |
|---|---|
| `GetRangedGroups`, `GetBotCircleIndexAndCount` (stable healer/dps ring slots) | `src/Ai/Raid/Hyjal/Util/HyjalHelpers.cpp:43-73`, decl `HyjalHelpers.h:64-74` |
| arrival latch per GUID (`hasReachedWinterchillPosition`) | `src/Ai/Raid/Hyjal/Util/HyjalHelpers.h:78` |
| `GetNearestPlayerInRadius` | `src/Ai/Raid/RaidBossHelpers.h:31` |
| `FleePosition(pos, radius, minInterval)` | `src/Ai/Base/Actions/MovementActions.cpp:2255` |
| spread idiom to crib | `src/Ai/Raid/ToC/ToCActions.cpp:154-162`, `src/Ai/Raid/VoA/VoAActions.cpp:99-108` |
| role-branched position action template | `src/Ai/Raid/Naxx/Action/NaxxActions_Loatheb.cpp:335+` |
| per-instance shared phase clock | `src/Ai/Raid/Naxx/NaxxBossHelper.h:1279-1295` (`HeiganBossHelper::PhaseStateFor`) |
| threat redirect base | `src/Ai/Raid/Naxx/Action/NaxxActions.h:31-50` (`NaxxRedirectThreatAction`) |

---

## Approach

### 1. Spell ids — `src/Ai/Raid/Naxx/NaxxSpellIds.h:126-129`

Add under the existing Anub'Rekhan block:

```cpp
static constexpr uint32 Impale10 = 28783;
static constexpr uint32 Impale25 = 56090;
static constexpr uint32 SummonCorpseScarabs5 = 29105;   // from a dead player
static constexpr uint32 SummonCorpseScarabs10 = 28864;  // from a dead Crypt Guard
```

Drop `LocustSwarm10Alt = 28786` or comment it as dead — the core never casts it.

### 2. `AnubrekhanBossHelper` — new, in `src/Ai/Raid/Naxx/NaxxBossHelper.h`

Model it on `HeiganBossHelper` (`:1127-1319`), **not** on `GenericBossHelper<BossAiType>`:
`boss_anubrekhan` is declared inside its `.cpp` and drives everything off `scheduler`, not `events`,
so the template's `_ai->events` route gives nothing.

Reuse Heigan's per-instance shared-clock pattern verbatim (`PhaseStateFor(Unit*)` at `:1279-1295`:
static mutex + `unordered_map<instanceId, State>`, GUID re-arm, `StaleStateMs` re-anchor). State:

```cpp
struct EncounterState
{
    ObjectGuid bossGuid;
    bool   clockKnown = false;
    bool   synced = false;     // only a real pull (health > 99%) gives a trustworthy anchor
    uint32 engageMs = 0;
    uint32 lastSeenMs = 0;
};
```

Public surface:

- `bool UpdateBossAI()` — find `"anub'rekhan"`, anchor/refresh the clock.
- `Unit* GetBoss()`
- `bool IsLocustSwarmActive()` — the single place the Locust auras are checked. Today the same check
  is duplicated in `NaxxMultipliers.cpp:439` and `NaxxActions_Anubrekhan.cpp:111`.
- `uint32 MsUntilNextImpale()` — from the deterministic 15s / 20s clock; returns 0 when unsynced.
- `std::vector<Unit*> GetCryptGuards()` / `GetCorpseScarabs()` — **filtered by `IsAlive()`**, matched
  on entry id `16573` rather than lowercased name.
- Geometry constants, so no magic numbers survive in the actions:

```cpp
static constexpr float RoomCenterX = 3272.49f;
static constexpr float RoomCenterY = -3476.27f;
static constexpr float RoomFloorZ  = 287.08f;         // replaces bot->GetPositionZ()
static constexpr float KiteRadius  = 45.0f;
static constexpr float ImpaleSpreadDistance = 12.0f;  // > Impale splash, < room radius
static constexpr float LocustSafeDistance   = 20.0f;  // ~15 yd swarm + buffer
static constexpr float RangedBandMin = 20.0f;         // stay out of the swarm
static constexpr float RangedBandMax = 28.0f;         // stay in cast + heal range
```

Use `NAXX_MAP_ID` (`NaxxBossHelper.h:36`) instead of the literal `533`.

### 3. `AnubrekhanPositionAction` — rewritten, `NaxxActions_Anubrekhan.cpp:103`

Keep the `RotateAroundTheCenterPointAction` base for the MT kite, but **raise `intervals` 16 → 32**
(spacing drops 17.7 → 8.8 yd, so the kite tracks smoothly) in the ctor at `NaxxActions.h:360-368`,
and hold an `AnubrekhanBossHelper` member.

Structure it like `LoathebPositionAction::Execute` (`NaxxActions_Loatheb.cpp:335+`) — branch on role,
and run **every tick of the fight**, not only during the swarm:

- **Main tank** — unchanged during Locust Swarm (advance one waypoint round the circle). Outside the
  swarm, do nothing and let normal tanking run.
- **Assist tank** — hold Crypt Guards away from the ranged ring; do not follow the kite.
- **Melee DPS** — stay on their target, but claim a deterministic angular slot around it at ~5 yd so
  they are not stacked on each other either (Impale can pick a melee).
- **Ranged DPS + healers** — deterministic slot ring, below.

**Ranged slot ring.** Port the Hyjal helpers, which already solve exactly this
(`HyjalHelpers.cpp:43-73`): `GetRangedGroups(botAI, bot)` splits the group into healers/rangedDps in
stable group order, `GetBotCircleIndexAndCount(botAI, bot, groups)` returns `{index, count}`. Port
into `NaxxBossHelper.h` rather than including the Hyjal header, to avoid a cross-raid dependency.

Anchor angle: the bearing **from the boss to the room centre**. That keeps the ring on the inside of
the kite path, so the arc stays inside the room as the boss laps the circle.

```
r     = healer ? RangedBandMax : (RangedBandMin + 4.0f)
arc   = 4.0f * M_PI / 3.0f                       // 240 deg, keeps every slot within ~30 yd of boss
theta = anchor - arc/2 + arc * (index + 0.5f) / count
dest  = boss + (cos(theta), sin(theta)) * r
```

With `count <= 8` on a 240° arc at r >= 20 the minimum chord is ~10 yd, which clears Impale splash.
Guarantee it explicitly: once the bot has arrived, if `GetNearestPlayerInRadius(bot,
ImpaleSpreadDistance)` (`RaidBossHelpers.h:31`) still finds someone, nudge with
`FleePosition(nearestPlayer->GetPosition(), ImpaleSpreadDistance, 1000)` — the ToC/VoA idiom
(`ToCActions.cpp:154-162`) as a backstop, not the primary mechanism.

Anti-churn, or bots jitter and are mid-move when Impale lands:
- Move only when `bot->GetExactDist2d(dest) > SlotTolerance (3.0f)`.
- Latch arrival per-GUID, like Hyjal's `hasReachedWinterchillPosition` (`HyjalHelpers.h:78`); clear it
  when the boss moves more than ~8 yd or the phase flips.
- Rate-limit repositioning to ~1000 ms.
- Always pass `RoomFloorZ`, never `bot->GetPositionZ()`.

Net effect: outside the swarm the ring sits 20–28 yd behind the boss, spread ≥10 yd; during the swarm
the same ring tracks the kiting boss, staying outside the ~15 yd swarm and inside cast/heal range
instead of stranding itself 45 yd away at the centre.

### 4. `AnubrekhanChooseTargetAction` — fixes, `NaxxActions_Anubrekhan.cpp:15-101`

- Filter `unit->IsAlive()` in the `"attackers"` loop (cf. `NaxxActions_Loatheb.cpp:388`).
- Match Crypt Guards by entry `16573` via the helper instead of `EqualLowercaseName`.
- Replace "lowest HP Crypt Guard" with **lowest-GUID living Crypt Guard** so DPS stop swapping every
  tick; keep the Corpse Scarab fallback but prefer scarabs attacking a healer.
- Assist tank: prefer the newest untanked Crypt Guard (the Locust Swarm add), and pull it away from
  the ranged ring.

### 5. `AnubrekhanGenericMultiplier` — `NaxxMultipliers.cpp:432-449`

- Route the Locust check through `AnubrekhanBossHelper::IsLocustSwarmActive()`.
- Additionally zero `CombatFormationMoveAction` for the whole encounter (match Grobbulus `:43`,
  Heigan `:56`) so the generic mover stops fighting the ring.
- Keep the `FleeAction` suppression, but scope it to non-tanks so a fleeing bot cannot break the kite.

### 6. Threat redirect

Add `AnubrekhanRedirectThreatAction : NaxxRedirectThreatAction` (`NaxxActions.h:31-50`) mirroring
`PatchwerkRedirectThreatAction` (`:426`): Misdirection / Tricks onto the main tank, dumped into the
boss. Wire at `ACTION_RAID + 3`.

### 7. Wiring — the four standard sites

1. `NaxxTriggers.h` / `.cpp` — add `AnubrekhanLocustSwarmTrigger` (helper-backed) alongside the
   existing whole-fight `AnubrekhanTrigger` (`NaxxTriggers.cpp:349`).
2. `NaxxTriggerContext.h` — creator + static factory.
3. `Action/NaxxActions.h` + `NaxxActions_Anubrekhan.cpp` — declarations and bodies;
   `NaxxActionContext.h:111-112` — creators + factories.
4. `NaxxStrategy.cpp:59-65` — extend the trigger node:

```cpp
triggers.push_back(new TriggerNode("anub'rekhan",
    {
        NextAction("anub'rekhan redirect threat", ACTION_RAID + 3),
        NextAction("anub'rekhan position", ACTION_RAID + 2),
        NextAction("anub'rekhan choose target", ACTION_RAID + 1)
    }
));
```

Multiplier stays registered at `NaxxStrategy.cpp:241`.

### Heroic (25-man)

No separate strategy — 10 and 25 share one code path, as everywhere else in this module. The only
mode-dependent things are the spell ids (both variants already in the `HasAnyAura` lists) and the
ring `count`, which is derived from the live group. Verify on both sizes.

---

## Files touched

| File | Change |
|---|---|
| `src/Ai/Raid/Naxx/NaxxSpellIds.h` | Impale + Corpse Scarab ids |
| `src/Ai/Raid/Naxx/NaxxBossHelper.h` | new `AnubrekhanBossHelper` + ported ranged-ring helpers |
| `src/Ai/Raid/Naxx/Action/NaxxActions.h` | Anub decls: helper member, `intervals` 16→32, redirect action |
| `src/Ai/Raid/Naxx/Action/NaxxActions_Anubrekhan.cpp` | position rewrite, target-selection fixes, redirect impl |
| `src/Ai/Raid/Naxx/NaxxMultipliers.cpp` | helper-backed Locust check + suppress `CombatFormationMoveAction` |
| `src/Ai/Raid/Naxx/NaxxTriggers.h` / `.cpp`, `NaxxTriggerContext.h` | Locust Swarm trigger |
| `src/Ai/Raid/Naxx/NaxxActionContext.h` | register new actions |
| `src/Ai/Raid/Naxx/NaxxStrategy.cpp` | trigger node update |

## Verification

Static:
1. Every new spell id cross-checked against `boss_anubrekhan.cpp` and `spelldifficulty_dbc.sql`.
2. Every new name registered in **both** the creators map and the static factory of its context
   header — a missing factory is a silent no-op at runtime.
3. Grep the module for remaining literal `533`, `3272.49`, `3476.27` outside the helper.

In-game (the module cannot be compiled headless in this environment — see the build-verification
note; expect a hand-off for the actual build):
1. Pull Anub'rekhan 10-man with a full bot raid. Watch the first Impale at **T+15s**, then T+35s,
   T+55s. Expect a single victim taking damage, not the raid.
2. Confirm the ranged ring forms before the first Impale and holds ≥10 yd separation while bots cast.
3. At the first Locust Swarm (T+70–120s): MT kites, ranged/healers track at 20–28 yd from the boss.
   Confirm healers can reach the MT and that ranged keep casting for the full ~20s.
4. Confirm nobody is inside the swarm — no silence, no swarm ticks on ranged.
5. Kill a Crypt Guard, confirm the 10 Corpse Scarabs get picked up and DPS stop re-targeting every
   tick.
6. Repeat on 25-man: 2 pre-spawned Crypt Guards, `56090` / `54021` variants, larger ring `count`.
7. With `tellWhenAvoidAoe` on, confirm the spread comes from the ring and **not** from `avoid aoe`
   (which cannot see either mechanic).
