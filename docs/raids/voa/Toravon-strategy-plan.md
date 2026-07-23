# Toravon the Ice Watcher — VoA playerbot strategy

Fourth boss of Vault of Archavon (map 624, creature entry 38433). Added on top of the shared `"voa"`
`RaidVoAStrategy`, mirroring the Archavon (mark + resist) and Koralon (avoid + multiplier) patterns.
All code lives in `src/Ai/Raid/VoA/` — no registration changes outside that directory, no CMake edit
(sources are picked up by the module glob).

## Encounter mechanics (from core `boss_toravon.cpp`)

| Spell / NPC | Id | Bot handling |
|---|---|---|
| Whiteout — raid-wide frost AoE, ~40s | 72034 | Unavoidable → **frost resistance** aura |
| Frozen Mallet — tank frost melee debuff | 71993 | Auto (tank), no action |
| Freezing Ground — void-zone patch under random player, ~20s | 72090 | **Move out** of the patch |
| Frozen Orb — summoned orb, pulses frost dmg, switches target ~10s (1 @10-man, 3 @25-man) | NPC **38456**, dmg 72081 | **Flee** the orbs |

**Heroic is free**: NPC entries and spell ids are identical across 10N/25N/10H/25H (difficulty is an
instance property). Orb count differs but the nearest-creature scan handles any count. No per-difficulty
spell variants to enumerate.

## Components (all in `src/Ai/Raid/VoA/`)

1. **IDs** — `VoATriggers.h` enum `VoAIDs`: `BOSS_TORAVON = 38433`, `NPC_FROZEN_ORB = 38456`,
   `SPELL_FREEZING_GROUND = 72090`.

2. **Mark boss (skull)** — `ToravonMarkBossTrigger` / `ToravonMarkBossAction`, copied from the Archavon
   pair with the boss name `"toravon the ice watcher"`. Tank assigns skull (icon 7); assist-tank when the
   main tank is a real player, else the main-tank bot. Priority `ACTION_RAID`.

3. **Frost resistance (Whiteout)** — reuses base `BossFrostResistanceTrigger` / `BossFrostResistanceAction`
   via the context factories only (no new class), like `archavon nature resistance`. Priority `ACTION_RAID`.

4. **Freezing Ground avoid** — `ToravonFreezingGroundTrigger` is active when `bot->HasAura(SPELL_FREEZING_GROUND)`.
   `ToravonFreezingGroundAction` reads the bot's `AI_VALUE(Aura*, "area debuff")`, confirms the spell id, and
   flees the aura's dynamic-object owner (`GetDynobjOwner()->GetPosition()`) via `FleePosition`, mirroring the
   engine's own AoE dodge (`MovementActions.cpp` `AvoidAuraWithDynamicObj`). Priority `ACTION_EMERGENCY`.

5. **Frozen Orb avoid** — `ToravonFrozenOrbAvoidTrigger` is active when a live Frozen Orb (38456) is within
   ~10 yd. `ToravonFrozenOrbAvoidAction` flees the nearest orb (same shape as the Archavon rock-shards spread,
   keyed on the orb creature instead of a player). Priority `ACTION_EMERGENCY`.

6. **Multiplier** — `ToravonAvoidMultiplier` returns `0.0f` for `CastReachTargetSpellAction`,
   `ReachTargetAction`, `CombatFormationMoveAction`, `FollowAction` while either avoid trigger is active, so
   move-to-boss actions don't cancel the dodge. Registered in `RaidVoAStrategy::InitMultipliers` next to the
   Koralon one.

7. **Wiring** — creators + static factories in `VoATriggerContext.h` / `VoAActionContext.h`; `TriggerNode`s in
   `RaidVoAStrategy::InitTriggers`. Files touched: `VoATriggers.{h,cpp}`, `VoAActions.{h,cpp}`,
   `VoATriggerContext.h`, `VoAActionContext.h`, `VoAMultipliers.{h,cpp}`, `VoAStrategy.cpp`.

## Verification

- Build only if asked (slow). `python apps/codestyle/codestyle-cpp.py` before done.
- In-game on map 624 (one normal + one heroic VoA), pull Toravon with a bot raid and confirm: a tank marks
  him with skull; eligible bots apply frost resistance; bots run out of Freezing Ground patches; bots flee
  Frozen Orbs (especially 25-man where 3 spawn). Sanity-check no regression to Emalon/Archavon/Koralon — their
  trigger names are untouched and Toravon's nodes are gated on his own boss lookup.
