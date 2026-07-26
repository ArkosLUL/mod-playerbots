# Koralon the Flame Watcher — Playerbot Raid Strategy

## Context

The shared `voa` playerbot strategy (`src/Ai/Raid/VoA/`) already fully handles Emalon
(mark, lightning nova spread, overcharge, fall-recover, nature resist) and Archavon
(mark, rock-shards spread, nature resist). **Koralon is only stubbed** — the sole wired
behavior is `koralon fire resistance` (reuses base `BossFireResistanceAction/Trigger`).

This task fleshes Koralon out into a full, slightly richer encounter strategy so bots
focus-fire the boss, dodge the frontal **Burning Breath** cone, and spread for
**Flaming Cinder**. Fire resistance stays as-is. Must work normal (10) and heroic (25) —
all Koralon spells are difficulty-scaled server-side (single spell id per ability), so
one predicate covers both modes.

### Encounter facts (from `src/server/scripts/Northrend/VaultOfArchavon/boss_koralon.cpp`)
- Boss entry `CREATURE_KORALON = 35013`; find via name lookup `"koralon the flame watcher"`.
- `SPELL_BURNING_FURY = 68168` — passive self-buff at pull. No bot action.
- `SPELL_BURNING_BREATH = 66665` — **real cast** (`CastSpell(..., false)`), frontal cone,
  boss slow-rotates during it (`rotateTimer`). Detectable via `UNIT_STATE_CASTING` +
  `FindCurrentSpellBySpellId(66665)`. Non-tanks in front should leave the cone.
- `SPELL_FLAMING_CINDER = 66681` → missile `66682` at a random player's location
  (splash, `MaxAffectedTargets=1`). Spread reduces overlap; the generic `avoid aoe`
  strategy (CombatStrategy → `AreaDebuffValue`) already pulls a bot out of any lingering
  ground fire, so no custom "stand out of fire" action is needed.
- `SPELL_METEOR_FISTS = 66725` — melee proc, damage lands only on the boss's melee
  victim. **Not implemented** (user-confirmed out of scope).

## Scope (user-confirmed)
1. Mark boss with skull (tank) — focus fire.
2. Burning Breath frontal-cone dodge (non-tanks reposition behind boss).
3. Flaming Cinder spread (ranged, when clustered).
4. Keep existing fire resistance.

## Design & files

All 4 high-level wiring sites are **already done for `voa`** (strategy registered in
`RaidStrategyContext.h`; contexts added in `BuildSharedTriggerContexts.cpp` /
`BuildSharedActionContexts.cpp`; auto-activation in `PlayerbotAI.cpp`). Only intra-VoA
files + one shared helper change.

### 1. `VoATriggers.h` — enum + 3 trigger classes
Add to `enum VoAIDs`:
```cpp
// Koralon the Flame Watcher
CREATURE_KORALON      = 35013,
SPELL_BURNING_BREATH  = 66665,
SPELL_FLAMING_CINDER  = 66681,
```
Add classes (mirroring existing style):
- `KoralonMarkBossTrigger` ("koralon mark boss trigger")
- `KoralonBurningBreathTrigger` ("koralon burning breath trigger")
- `KoralonFlamingCinderSpreadTrigger` ("koralon flaming cinder spread trigger")

### 2. `VoATriggers.cpp` — `IsActive()` bodies
- **MarkBoss**: copy `ArchavonMarkBossTrigger` verbatim, swap name to
  `"koralon the flame watcher"`.
- **BurningBreath**: `!IsTank`; boss `"koralon the flame watcher"` alive; boss casting —
  `boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_BURNING_BREATH)`;
  bot inside cone — `IsBotInFrontalCone(bot, boss, float(M_PI) / 2.0f /*90°*/, 40.0f)`.
- **FlamingCinderSpread**: copy `ArchavonRockShardsSpreadTrigger` verbatim, swap name to
  `"koralon the flame watcher"` (ranged-only, alive+in-combat, clustered within 8.0f).

### 3. `VoAActions.h` — 3 action classes
- `KoralonMarkBossAction : public MovementAction` ("koralon mark boss action")
- `KoralonBurningBreathAction : public MovementAction` ("koralon burning breath action")
- `KoralonFlamingCinderSpreadAction : public MovementAction` ("koralon flaming cinder spread action")

### 4. `VoAActions.cpp` — `Execute()` + `isUseful()`
- **MarkBoss**: copy `ArchavonMarkBossAction`, swap name to `"koralon the flame watcher"`.
- **BurningBreath**: get boss; compute a point ~10yd **behind** the boss
  (`o = boss->GetOrientation() + M_PI`; `x = boss x + cos(o)*dist`, `y = boss y + sin(o)*dist`,
  keep boss z, then `bot->UpdateAllowedPositionZ`), and `MoveTo(mapId, x, y, z)`. `isUseful`
  delegates to the trigger.
- **FlamingCinderSpread**: copy `ArchavonRockShardsSpreadAction`
  (`GetNearestPlayerInRadius` → `FleePosition`).

### 5. `VoATriggerContext.h` / `VoAActionContext.h` — register 3 pairs
Add `creators["koralon ..."] = &...` lines + static factory methods, matching the
existing koralon-fire-resistance entries.

### 6. NEW `VoAMultipliers.{h,cpp}` — Burning-Breath movement suppressor
`class KoralonBurningBreathMultiplier : public Multiplier` overriding
`float GetValue(Action* action)`: while `KoralonBurningBreathTrigger` is active for this
bot, return `0.0f` for normal reposition/combat-movement actions (e.g. `"reach spell"`,
`"reach melee"`, `"follow"`, `"flee"`) so they don't drag the bot back into the cone;
return `1.0f` otherwise. (Model exact `Multiplier` signature on an existing file such as
`src/Ai/Raid/ZA/ZAMultipliers.*` or `src/Ai/Raid/Mag/MagMultipliers.*`.)

### 7. `VoAStrategy.h` / `VoAStrategy.cpp` — wire triggers + multipliers
- Header: add `virtual void InitMultipliers(std::vector<Multiplier*>& multipliers) override;`.
- `.cpp` `InitTriggers`: add the three `TriggerNode`s under the Koralon section:
  - `"koralon mark boss trigger"` → `"koralon mark boss action"`, `ACTION_RAID`
  - `"koralon flaming cinder spread trigger"` → action, `ACTION_RAID`
  - `"koralon burning breath trigger"` → action, `ACTION_EMERGENCY` (avoid-mechanic priority)
- `.cpp` add `InitMultipliers` pushing `new KoralonBurningBreathMultiplier(botAI)`;
  `#include "VoAMultipliers.h"`.

### 8. `RaidBossHelpers.{h,cpp}` — add reusable cone helper
```cpp
bool IsBotInFrontalCone(Player* bot, Unit* source, float coneAngle, float range);
// return bot && source && source->GetExactDist2d(bot) <= range && source->HasInArc(coneAngle, bot);
```
(Generic version of the existing `TrialOfTheCrusaderHelpers::IsBotInFrontalCone`; declaring
it in shared helpers lets VoA — and future strategies — reuse it.)

## Reused existing code
- `ArchavonMarkBossAction/Trigger`, `ArchavonRockShardsSpreadAction/Trigger` — templates to copy.
- `GetNearestPlayerInRadius`, `FleePosition`, `MoveAway`, `MoveTo`, `AI_VALUE(Unit*, "main tank")`,
  `IsAssistTankOfIndex`, group skull-icon logic — all already in the file.
- `BossFireResistanceAction/Trigger` — Koralon fire resist, untouched.
- Generic `avoid aoe` (CombatStrategy) — handles any lingering cinder ground fire; no custom.
- `Unit::HasInArc`, `Unit::FindCurrentSpellBySpellId`, `Unit::UpdateAllowedPositionZ`.

## Notes / gotchas
- CMake: VoA sources compile via the module's glob; new `VoAMultipliers.cpp` should be
  picked up automatically — confirm no explicit source list in `modules/mod-playerbots/CMakeLists.txt`.
- Codestyle: `auto const&`, `Type const*`, Allman braces, 4-space indent, `{}` fmt — run
  `python apps/codestyle/codestyle-cpp.py` before done. Do **not** build unless asked.

## Verification
- Grep-check the 3 trigger/action names appear consistently across
  `VoATriggers/Actions/{h,cpp}`, `VoATriggerContext.h`, `VoAActionContext.h`, `VoAStrategy.cpp`.
- Run `python apps/codestyle/codestyle-cpp.py` — must pass.
- (If a build is later requested) in-game on Koralon: tank bot marks skull; ranged bots
  spread on Flaming Cinder; non-tank bots step behind the boss during Burning Breath and
  resume DPS after; fire-resistance buff still applied.
