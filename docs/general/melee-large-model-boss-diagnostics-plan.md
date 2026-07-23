# Melee bots idle / underperform on large-model bosses — findings + diagnostics plan

## Context

Reports: on some bosses (large models) melee DPS bots do far less damage than they should — seen
standing at a distance doing nothing until manually summoned closer. Confirmed cases named by the
user: **Sapphiron** (Naxxramas), and **Archimonde** (Hyjal) where melee DPS was far below casters.

Investigation confirms the behaviour is reproducible from the code. There are **two independent
problems**: a generic defect in the melee approach, and boss-specific raid actions that starve the
melee rotation. This document records both; the agreed next step is **diagnostics only** — land the
logging, reproduce in-game, then fix against confirmed data.

Everything below is verified against this tree (module + `azerothcore-wotlk` core in the same repo).

## Established facts (core side)

- `WorldObject::GetObjectSize()` returns `UNIT_FIELD_COMBATREACH` (`Object.cpp:2888`), so all core
  distance helpers (`GetDistance`, `GetDistance2d`, `IsWithinDist*`) are surface-to-surface on
  combat reach.
- `Unit::IsWithinCombatRange(o, d)`: `dist3d < d + reachSum` (`Unit.cpp:765`).
- `Unit::GetMeleeRange(t) = reachSum + 4/3`; `IsWithinMeleeRange(t, d)`: `dist3d < d + GetMeleeRange
  + leeway` (`Unit.cpp:781-802`).
- Auto-attack gate: `IsWithinMeleeRange(victim)` (`PlayerUpdates.cpp:169`). Melee-range spells:
  `IsWithinMeleeRange(target, max(max_range - 2*MIN_MELEE_REACH, 0))` (`Spell.cpp:7100-7110`).
- Large bosses carry large reach — the repo already documents Magtheridon: `combat reach 12,
  bounding radius 4` (`src/Ai/Raid/Mag/MagActions.cpp:335`).
- Action priorities (`src/Bot/Engine/Strategy/Strategy.h:53-65`): `ACTION_HIGH = 20`,
  `ACTION_RAID = 60`, `ACTION_EMERGENCY = 90`.

## Ruled out (checked, not the cause)

- `ServerFacade::GetDistance2d(unit, wo)` → `unit->GetDistance2d(wo)` = surface distance, so the
  `"distance"` AI value is reach-aware and consistent with the triggers.
- Trigger/action thresholds are conservative, not a dead band: `EnemyOutOfMeleeTrigger` fires at
  `dist ≥ reachSum + 1.25` (`RangeTriggers.cpp:156-165`), `ReachTargetAction::isUseful` at
  `dist ≥ reachSum + 0.75` (`ReachTargetActions.cpp:16-34`), core allows swings to `reachSum + 1.33`.
- Self-centred AoE (Whirlwind etc.) is fine — AoE target search adds the target's combat reach.
- `CastMeleeSpellAction::isUseful` uses `bot->IsWithinMeleeRange` — matches core.
- `EnemyTooCloseForMeleeTrigger` / `MoveOutOfEnemyContactAction` are dead code (registered in
  `TriggerContext.h` / `ActionContext.h`, referenced by no strategy).

## Problem A — generic melee approach (`MovementAction::ReachCombatTo`, `src/Ai/Base/Actions/MovementActions.cpp:802-857`)

`ReachMeleeAction` is the *only* NextAction behind the `enemy out of melee` trigger
(`MeleeCombatStrategy.cpp:15-17`). If `ReachCombatTo` returns false, nothing else closes the gap —
the bot stands still and, if out of range, does nothing.

### A1 — the approach paths to the boss's **origin**; any path failure aborts the move

```cpp
PathGenerator path(bot);
path.CalculatePath(tx, ty, tz, false);          // tx,ty,tz = boss centre
int typeOk = PATHFIND_NORMAL | PATHFIND_INCOMPLETE | PATHFIND_SHORTCUT;   // 0x07
if (!(type & typeOk))
    return false;                                // bot never moves
```

The destination is the boss's centre, not the melee stand point. On a large boss the centre is 10+
yd inside the model and can sit on a spot the navmesh does not cover (mesh hole, off-mesh spawn,
raised/sloped platform). `BuildPolyPath` then yields `PATHFIND_NOPATH` (0x08) — or
`PATHFIND_NOT_USING_PATH` / `PATHFIND_SHORT` alone — none in `typeOk`, so `ReachCombatTo` bails and
the bot is parked wherever it stood. `MoveTo`'s `SearchForBestPath` is stricter still
(`NORMAL|INCOMPLETE` only, `MovementActions.cpp:1733`) and returns `INVALID_HEIGHT` → `MoveTo`
returns false too. Manually summoning inside melee range is exactly the workaround this predicts.

### A2 — prediction offset vs. trigger use different target positions

```cpp
if (target->HasUnitMovementFlag(MOVEMENTFLAG_FORWARD) && behind) {
    float predictDis = std::min(3.0f, target->GetObjectSize() * 2);   // = 3.0 for any big boss
    tx += cos(target->GetOrientation()) * predictDis; ...
}
if (bot->GetExactDist(tx, ty, tz) <= distance)   // measured against the PREDICTED point
    return false;
```

Trigger and `isUseful()` measure against the boss's **actual** position; `ReachCombatTo` measures
against a point up to 3 yd ahead of it. While the boss moves and the bot is behind it, `isUseful()`
can be true while `ReachCombatTo` immediately returns false → trigger fires forever, action declines
forever, bot stands still.

### A3 — stand point sits at the extreme edge of legal melee range

`distance = meleeDistance(0.75) + reachSum`, so the bot parks at `reachSum + 0.75` while core's
swing limit is `reachSum + 1.33` — a **0.58 yd** margin. The endpoint comes from
`PathGenerator::ShortenPathUntilDist`, which explicitly "settles for a guesstimate"
(`PathGenerator.cpp:1094`) and truncates early on any LoS failure along the path
(`PathGenerator.cpp:1071-1080`), so the real stop distance is ≥ the intended one, never below.

## Problem B — boss-specific raid actions outrank and starve the melee rotation

### B1 — Sapphiron: melee pinned to one point for the whole ground phase

`SapphironGroundPositionAction::Execute` (`src/Ai/Raid/Naxx/Action/NaxxActions_Sapphiron.cpp:11-81`),
wired at `ACTION_RAID + 1` = **61**, far above `reach melee` (21) and above the whole DPS rotation:

```cpp
bool needsSideStack = boss->isInFront(bot) || boss->isInBack(bot);
```

`WorldObject::isInFront/isInBack` default to `arc = M_PI` (`Object.h:558-559`) — front half plus
back half is the **entire circle**, so `needsSideStack` is *always* true. Melee are therefore
permanently commanded to a single point at `boss->GetOrientation() + π/2`, hard-coded
`distance = 5.0f` from the boss centre (line 48-52), recomputed every tick as the boss turns. The
`JustLanded()` branch (line 60-81) does the same from `helper.center` (room centre), not the boss.
Neither distance accounts for `GetCombatReach()`.

Consequence: every tick where the move is issued, the priority-61 action returns true and the
rotation never runs; and the melee stack point is a fixed 5 yd from the origin regardless of how
big the boss's reach is. This is the most likely explanation for the Sapphiron report.

### B2 — Archimonde: two high-priority actions pull melee off the boss

- `ArchimondeSpreadToAvoidAirBurstAction` (`src/Ai/Raid/Hyjal/HyjalActions.cpp:1067-1122`,
  `ACTION_RAID + 1`): `MoveAway(mainTank, AIR_BURST_SAFE_DISTANCE - distanceToMainTank)` with
  `AIR_BURST_SAFE_DISTANCE = 15.0f` (`src/Ai/Raid/Hyjal/Util/HyjalHelpers.h:126`). Melee get pushed
  15 yd from the main tank — i.e. off the boss — every Air Burst.
- `ArchimondeAvoidDoomfireAction` (`HyjalActions.cpp:1124+`, `ACTION_EMERGENCY + 6` = 96) with
  `dangerDist = 10.0f` and an 18 s trail lifetime. Doomfires wander continuously, so melee can be
  in near-permanent flee state.

Both are legitimate mechanics, but the combination plausibly keeps melee off the boss most of the
fight while casters keep DPSing at range. Needs measurement before changing anything.

## Agreed next step: diagnostics only

Add temporary, log-only instrumentation (no behaviour change), following the existing
`LOG_DEBUG("playerbots", …)` + `sPlayerbotAIConfig.logInGroupOnly` conventions already used
throughout `PlayerbotAI.cpp` / `MovementActions.cpp`.

1. **`ReachCombatTo` bail-out log** — `src/Ai/Base/Actions/MovementActions.cpp:802-857`.
   On every `return false`, and once on success, log: bot name, target entry + name,
   `target->GetCombatReach()`, `bot->GetCombatReach()`, `bot->GetExactDist(target)`,
   `bot->GetMeleeRange(target)`, the computed `distance`, whether the prediction branch fired,
   `path.GetPathType()` (raw int), and the chosen endpoint. Tag each with the reason
   (`!IsMovingAllowed` / `already within distance` / `bad path type` / `MoveTo failed` / `moved`).

2. **`ReachMeleeAction` suppression log** — log when `EnemyOutOfMeleeTrigger` is active but the
   action is skipped, and the winning action's name + priority for that tick. This is what
   distinguishes Problem A (movement fails) from Problem B (higher-priority raid action wins).
   The engine's action-selection point is the place to hook this; keep it behind the same debug
   gate so it is off by default.

3. **One-shot boss telemetry** — when a bot enters combat with a creature whose
   `GetCombatReach() > 5.0f`, log entry id, name, `GetCombatReach()`, `GetObjectSize()`,
   `GetCollisionHeight()`, spawn position. Gives the concrete reach numbers for Sapphiron,
   Archimonde and anything else that shows up, which A1/A3 and B1 both need.

Gate all three behind the existing `debug move` strategy check
(`botAI->HasStrategy("debug move", BOT_STATE_NON_COMBAT)`, already used in this file) so nothing
logs in normal play.

## Verification of the diagnostics step

1. Build the module — note this cannot be compiled headless in this environment; hand off to the
   normal build.
2. In-game: enable `debug move` on 2-3 melee bots, run Sapphiron and Archimonde.
3. Expected discriminator:
   - Log dominated by `bad path type` / `MoveTo failed` on `reach melee` → **Problem A** confirmed;
     fix = path to the melee stand point instead of the boss origin, accept the degraded path types
     with a straight-line fallback, measure the early-out against the actual (not predicted)
     position, and aim at `GetMeleeRange(target) - meleeDistance` instead of `reachSum + 0.75`.
   - Log dominated by `sapphiron ground position` / `archimonde …` winning the tick → **Problem B**
     confirmed; fix = give `isInFront`/`isInBack` real arcs on Sapphiron, make the melee stand
     distance reach-aware, and let the positioning action return false once the bot is already at
     its slot so the rotation runs.
4. Capture a DPS-meter baseline for one melee bot on each boss so the later fix can be measured.
