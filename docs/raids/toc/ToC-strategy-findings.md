# Trial of the Crusader — Bot Strategy Findings

Investigation of how raid boss strategies are implemented in `mod-playerbots`, written to prepare
the implementation of **Trial of the Crusader** (the raid, map **649**).

> **Key discovery:** ToC has **no current support**. The existing strategy key `wotlk-toc` is
> **Trial of the *Champion*** (the 5-man dungeon, map 650) — *not* the raid. There is **no handler
> for map 649** anywhere in the codebase. The new raid strategy will use the key
> **`trialofthecrusader`**.

---

## 1. How raid boss strategies work today

### 1.1 Object model (engine primitives)

Located under `src/Bot/Engine/`:

| Primitive | File | Role |
| --- | --- | --- |
| `Strategy` | `Strategy/Strategy.h` | Base class. Overrides `getName()`, `InitTriggers()`, `InitMultipliers()`. |
| `Trigger` | `Trigger/Trigger.h` | Condition; implements `IsActive()`. Wrapped by `TriggerNode` (trigger name → list of `NextAction(name, relevance)`). |
| `Action` | `Action/Action.h` | Executable response; implements `Execute(Event)`, `isUseful()`, `isPossible()`. Raid actions usually subclass `AttackAction` / `MovementAction`. |
| `Multiplier` | `Multiplier.h` | `GetValue(Action*)` returns a float scaling an action's relevance; `0.0f` suppresses it. Gates cooldowns/movement by phase. |
| `Value<T>` | `Value/Value.h` | Cached game-state calculators, read via the `AI_VALUE` / `AI_VALUE2` macros. |

### 1.2 Factory / registration pattern

Every object is created by name through `NamedObjectContext<T>` factories
(`src/Bot/Engine/NamedObjectContext.h`), aggregated into shared lists on `AiObjectContext`.

- **Strategies** — `src/Ai/Raid/RaidStrategyContext.h` maps a key → strategy class, e.g.
  `creators["gruulslair"] = &RaidStrategyContext::gruulslair;` with a matching static creator.
- **Per-raid Triggers/Actions contexts** are registered in the shared builders:
  - `src/Bot/Engine/BuildSharedTriggerContexts.cpp` — `triggerContexts.Add(new Raid...TriggerContext());`
  - `src/Bot/Engine/BuildSharedActionContexts.cpp` — `actionContexts.Add(new Raid...ActionContext());`
- `RaidStrategyContext` itself is added once in `BuildSharedStrategyContexts.cpp` (already covers
  all raids — no change needed there).

### 1.3 Automatic activation by map

`PlayerbotAI::ApplyInstanceStrategies(mapId)` — `src/Bot/PlayerbotAI.cpp:1623`:

1. Removes every instance strategy in the `allInstanceStrategies` vector (line ~1625).
2. `switch (mapId)` maps a numeric map ID → strategy key (e.g. `case 565 → "gruulslair"`).
3. Adds the matched key to both the `BOT_STATE_COMBAT` and `BOT_STATE_NON_COMBAT` engines.

Called on login and on every map change (`PlayerbotAI.cpp:156, 797, 1609, 1873`). When
`tellMaster` is set it announces `"Added <key> instance strategy"`.

### 1.4 Naming conventions

- **Strategy keys:** bare lowercase, no spaces — `"blacktemple"`, `"karazhan"`, `"gruulslair"`.
- **Triggers / actions:** lowercase, space-separated, boss-prefixed —
  `"high king maulgar boss channeling whirlwind"` → `"high king maulgar run away from whirlwind"`.
- **Multiplier classes:** `{BossName}{Purpose}Multiplier`.
- **Relevance bands:** `ACTION_RAID` (60) for assignments; `ACTION_EMERGENCY` (90) for avoidance,
  offset by `+N` to break ties (e.g. `ACTION_EMERGENCY + 7` for a whirlwind dodge).

### 1.5 Canonical example — Gruul's Lair (recently rewritten, PR #2473)

`src/Ai/Raid/Gruul/` is the reference pattern. File set per raid:

- `GruulStrategy.h/.cpp` — `getName()`, `InitTriggers()`, `InitMultipliers()`.
- `GruulTriggers.h/.cpp` + `GruulTriggerContext.h` — trigger classes + name factory.
- `GruulActions.h/.cpp` + `GruulActionContext.h` — action classes + name factory.
- `GruulMultipliers.h/.cpp` — phase / cooldown gating.
- `GruulHelpers.h/.cpp` — shared positions and utilities.

`GruulStrategy.cpp` composes the fight: 13 `TriggerNode`s (tank assignments, kill order,
whirlwind / blast-wave / shatter avoidance) and 8 multipliers (delay Bloodlust until the kill
target dies, lock tank movement, suppress ranged attacks during shatter spread). Triggers read
state via helpers like `AI_VALUE2(Unit*, "find target", "high king maulgar")`; actions mark
targets, set RTI, and `MoveTo` hardcoded boss positions.

### 1.6 Build wiring

The module has **no `CMakeLists.txt`** — AzerothCore's parent module loader globs sources
recursively, so **new `.cpp` files are compiled automatically**. Only the registration files in
§1.2 / §1.3 must be edited.

---

## 2. Trial of the Crusader — encounter mechanics

Core scripts (source of truth for GUIDs / spell IDs):
`src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/`
— `boss_northrend_beasts.cpp`, `boss_lord_jaraxxus.cpp`, `boss_faction_champions.cpp`,
`boss_twin_valkyr.cpp`, `boss_anubarak_trial.cpp`, `instance_trial_of_the_crusader.cpp`,
`trial_of_the_crusader.h`.

1. **Northrend Beasts** (one continuous fight, three stages):
   - *Gormok* — avoid Staggering Stomp zones; Fire Bombs leave ground hazards; spread.
   - *Acidmaw & Dreadscale* — submerge/emerge form swap; avoid slime pools / churning ground;
     focus the mobile worm; spread for sprays.
   - *Icehowl* — recognize the jump-to-center charge phase; intercept/dodge the charge (a wall
     crash stuns the boss → free DPS window); move out of Whirl / Arctic Breath.
2. **Lord Jaraxxus** — interrupt/dispel Incinerate Flesh & Touch of Jaraxxus; spread for Legion
   Flame; kill Mistress of Pain adds; avoid Infernal Volcano AoE.
3. **Faction Champions** (PvP-style NPCs) — focus-fire kill order (healers → casters → melee);
   interrupt heals; avoid clumping (Anti-AoE).
4. **Twin Val'kyr** (shared health pool) — essence-color assignment; collect matching-color orbs;
   keep opposite-color immunity vs Light/Dark Touch; spread for Vortex.
5. **Anub'arak** — P1 manage Scarabs/Burrowers and spread for Penetrating Cold; submerge phase
   kite Pursuing Spikes into Frost Spheres; P3 burst through the permanent Leeching Swarm.

**Bot-tractable now** (avoidance / positioning / kill-order / interrupts): Beasts, Jaraxxus,
Faction Champions, Anub'arak P1/P3. **Harder** (need new state tracking): Val'kyr essence system,
Anub'arak spike-kiting.

---

## 3. Integration checklist (for the implementation task)

### 3.1 Create the strategy package — mirror `src/Ai/Raid/Gruul/`

New folder `src/Ai/Raid/ToC/`:

- `ToCStrategy.h/.cpp` — `class RaidTrialOfTheCrusaderStrategy : public Strategy`; `getName()`
  returns `"trialofthecrusader"`.
- `ToCTriggers.h/.cpp` + `ToCTriggerContext.h`
- `ToCActions.h/.cpp` + `ToCActionContext.h`
- `ToCMultipliers.h/.cpp`
- `ToCHelpers.h/.cpp` — boss GUIDs / spell IDs from `trial_of_the_crusader.h`, arena positions.

### 3.2 Register it (four edits)

| File | Change |
| --- | --- |
| `src/Ai/Raid/RaidStrategyContext.h` | `#include "ToCStrategy.h"`; add `creators["trialofthecrusader"] = ...;` + static creator returning `new RaidTrialOfTheCrusaderStrategy(botAI)`. |
| `src/Bot/Engine/BuildSharedTriggerContexts.cpp` | `triggerContexts.Add(new RaidToCTriggerContext());` (+ include). |
| `src/Bot/Engine/BuildSharedActionContexts.cpp` | `actionContexts.Add(new RaidToCActionContext());` (+ include). |
| `src/Bot/PlayerbotAI.cpp` | Add `"trialofthecrusader"` to `allInstanceStrategies` (line ~1625) **and** `case 649: strategyName = "trialofthecrusader"; break;` in the switch. |

### 3.3 Suggested first encounters

Start with the most bot-tractable: **Northrend Beasts** and **Lord Jaraxxus**. Add **Anub'arak**
and **Faction Champions** next; do the **Val'kyr** essence system last.

### 3.4 Verification

- Build the module (new files are globbed automatically).
- Zone a bot group into map 649; confirm the `tellMaster` path prints
  `"Added trialofthecrusader instance strategy"`.
- Pull each implemented boss; confirm triggers fire and bots execute the assigned avoidance / kill
  actions.
- Run `python apps/codestyle/codestyle-cpp.py` before finishing.

---

## Critical files reference

- `src/Bot/PlayerbotAI.cpp:1623` — `ApplyInstanceStrategies` (map → strategy switch).
- `src/Ai/Raid/RaidStrategyContext.h` — strategy key registry.
- `src/Bot/Engine/BuildSharedTriggerContexts.cpp`, `BuildSharedActionContexts.cpp` — per-raid
  context registration.
- `src/Ai/Raid/Gruul/*` — reference implementation to mirror.
- `src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/*` — encounter source of truth.
