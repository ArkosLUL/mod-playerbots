# Shared anti-fear component (Fear Ward / Tremor Totem) for Onyxia 80 + Ulduar

## Context

Playerbots have no anti-fear behaviour in WotLK content. Fear Ward and Tremor Totem exist in the
codebase but are only reachable through five hand-copied, per-boss implementations in Classic/TBC
raids (BWL Nefarian, Kara Nightbane, TK Solarian, TK Kael'thas/Sanguinar, Hyjal Archimonde). Three
in-tree TODOs ask for a general system instead: `src/Ai/Raid/MC/MCStrategy.cpp:20`,
`src/Ai/Dungeon/DTK/DTKStrategy.cpp:26`, `src/Ai/Dungeon/Nex/NexStrategy.cpp:17`.

This change adds one reusable anti-fear component under `src/Ai/Raid/` and wires it to the WotLK
encounters that actually have counterable fears, so bots stop getting feared into Lunatic Gaze,
whelp packs, and the raid.

### What the investigation established

Content-tier Phase 1 (Naxxramas, Obsidian Sanctum, Eye of Eternity, Vault of Archavon) has **zero**
fear mechanics — all 15 Naxx boss spell enums plus the OS/EoE/VoA folders were checked. The only
mind effect is Chains of Kel'Thuzad (28410), a mind control, immune to both counters. Gluth's
vanilla Terrifying Roar (29685) does not exist in this build. So Phase 1 proper has no consumers;
Onyxia 80 (map 249, same tier by release) is included instead because Bellowing Roar is a real fear.

Counterable fears in scope:

| Encounter | Spell | ID | Notes |
|---|---|---|---|
| Onyxia (map 249) | Bellowing Roar | 18431 | `src/server/scripts/Kalimdor/OnyxiasLair/boss_onyxia.cpp:31`. Phase 3, below 40% |
| Auriaya | Terrifying Screech | 64386 | `.../Ulduar/Ulduar/boss_auriaya.cpp:31`, scheduled 35s. Raid-wide AoE fear |
| Yogg-Saron P2 | Malady of the Mind | 63830 / 63881 | `boss_yoggsaron.cpp:81-82`; aura registers on `SPELL_AURA_MOD_FEAR` at `:2245-2246`, and re-casts on removal to hop to a new player |
| Yogg-Saron P3 | Deafening Roar | 64189 | `boss_yoggsaron.cpp:143`, 50s into P3. Feared players run into Lunatic Gaze and bleed Sanity |

Explicitly **not** counterable, do not target: Insane 63120/64464 (AoE charm), Psychosis, Lunatic
Gaze, Induce Madness (Sanity drains), Chains of Kel'Thuzad (MC).

Auriaya's Terrifying Screech is already flagged as an unimplemented gap in
`docs/raids/ulduar/ulduar-boss-strategy-gap-analysis.md:24`.

### Two defects this must not reproduce

1. **`ChangeStrategy("+tremor")` does not work.** `ArchimondeCastFearImmunitySpellAction::UseTremorTotemStrategy()`
   (`src/Ai/Raid/Hyjal/HyjalActions.cpp:1062-1071`) only adds the `tremor` strategy. But
   `NoEarthTotemTrigger` picks the required earth totem by first match in
   `{"strength of earth", "stoneskin", "tremor", "earthbind"}`
   (`src/Ai/Class/Shaman/ShamanTriggers.cpp:306-311`), and every shaman spec ships with
   `strength of earth` or `stoneskin` (`src/Bot/Factory/AiFactory.cpp:335-344`). Tremor never wins,
   and both strategies register a node on the same `"no earth totem"` trigger at priority 55.0, so
   the slot thrashes. The strategy is also never removed afterwards, leaking past the instance.
2. The earth totem slot is exclusive. Anything that drops Tremor must also suppress the competing
   earth totem casts for as long as the fear window is open, or the shaman's own strategy re-drops
   Stoneskin on the next GCD.

## Approach

Cast directly and suppress the competition with a multiplier — no `ChangeStrategy`, no persistent
state, self-reverting when the fear window closes. The exact shape already exists in-tree:
`IllidanStormrageUseEarthbindTotemMultiplier` (`src/Ai/Raid/BT/BTMultipliers.cpp:704-722`) zeroes
`CastStrengthOfEarthTotemAction` / `CastStoneskinTotemAction` / `CastStoneclawTotemAction` /
`CastTremorTotemAction` the same way.

Per-class behaviour, following `ArchimondeCastFearImmunitySpellAction::Execute`
(`HyjalActions.cpp:1045-1051`):

- **Priest** → `botAI->CastSpell("fear ward", target)`. Target is the group main tank; if the tank
  already has the aura, the main healer. Reuse `GetGroupMainTank(botAI, bot)`
  (`src/Ai/Raid/RaidBossHelpers.h:27`) and the group walk from
  `NightbaneMainTankIsSusceptibleToFearTrigger::IsActive` (`src/Ai/Raid/Kara/KaraTriggers.cpp:315-340`).
- **Shaman** → `botAI->CastSpell("tremor totem", bot)`, gated on the totem not already being out.

## New files

### `src/Ai/Raid/RaidAntiFear.h` / `.cpp`

Sits next to `RaidBossHelpers.h`, cross-raid by design.

- `constexpr uint32 SPELL_FEAR_WARD = 6346;` — the single definition. It is currently duplicated in
  `KaraHelpers.h:58`, `TKHelpers.h:51`, `SSCHelpers.h:51`, `HyjalHelpers.h:45`; leave those alone in
  this change to keep the diff contained.
- `Player* GetAntiFearWardTarget(PlayerbotAI* botAI, Player* bot)` — main tank, else main healer;
  returns `nullptr` when every preferred target already has the aura or the bot cannot cast.
- `bool ShouldDropTremorTotem(PlayerbotAI* botAI, Player* bot)` — shaman, knows Tremor Totem
  (`SPELL_TREMOR_TOTEM_RANK_1 = 8143`, `src/Ai/Class/Shaman/ShamanTriggers.h:29`), and
  `!AI_VALUE2(bool, "has totem", "tremor totem")`.
- `class RaidAntiFearTrigger : public Trigger` — pure virtual `bool FearWindowActive()`. `IsActive()`
  returns false for non-priest/non-shaman, then `FearWindowActive()` plus the class-appropriate
  readiness check above.
- `class RaidAntiFearAction : public Action` — `Execute()` branches on class as described. Also
  overrides `isUseful()` by instantiating its trigger, matching the Ulduar convention
  (`src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp:101-105`).
- `class RaidAntiFearTotemGuardMultiplier : public Multiplier` — pure virtual `FearWindowActive()`;
  returns `0.0f` for `CastStrengthOfEarthTotemAction`, `CastStoneskinTotemAction`,
  `CastStoneclawTotemAction` while the window is open, `1.0f` otherwise. Non-shaman early-out first.

Concrete subclasses supply only `FearWindowActive()`.

### `src/Ai/Raid/Ony/OnyMultipliers.h` / `.cpp`

Onyxia has no multipliers file yet and `RaidOnyxiaStrategy::InitMultipliers` is empty
(`src/Ai/Raid/Ony/OnyStrategy.cpp:33-36`). Add the pair and populate it.

## Wiring

Three encounter consumers, each a trigger + action + multiplier subclass. Follow the four-site
registration pattern from the `raid-boss-strategy-recipe` skill.

**Onyxia** — window: `AI_VALUE2(Unit*, "find target", "onyxia")` alive and `GetHealthPct() <= 40.0f`.
- `src/Ai/Raid/Ony/OnyTriggers.h/.cpp`, `OnyActions.h/.cpp`, new `OnyMultipliers.h/.cpp`
- `OnyTriggerContext.h`, `OnyActionContext.h` — `"ony anti fear trigger"` / `"ony anti fear action"`
- `OnyStrategy.cpp` — new `// ----------- Phase 3 (40% - 0%) -----------` block, node at
  `ACTION_RAID + 2` (matches Kara/TK/Hyjal); register the multiplier in `InitMultipliers`

**Auriaya** — window: `AI_VALUE2(Unit*, "find target", "auriaya")` alive. Screech is on a fixed 35s
cycle, so keeping the totem up for the whole fight is correct.
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.h/.cpp`, `Action/UldActions_Auriaya.h/.cpp`
- `UldTriggerContext.h`, `UldActionContext.h` — `"auriaya anti fear trigger"` / `" action"`
- `UldStrategy.cpp` Auriaya block (currently `:199-216`), `ACTION_RAID + 2`

**Yogg-Saron** — window: `IsPhase2() || IsPhase3()` via the existing `YoggSaronTrigger` base
(`src/Ai/Raid/Uld/Trigger/UldTriggers_YoggSaron.h:12-32`). One trigger covers both Malady (P2) and
Deafening Roar (P3); no reason to split them. Complements, does not replace, the existing
`yogg-saron malady of the mind` spread node (`UldStrategy.cpp:443-445`).
- Same file set under `Uld/`, names `"yogg-saron anti fear trigger"` / `" action"`
- `UldStrategy.cpp` Yogg-Saron block (`:416-498`), `ACTION_RAID + 2`

**Multipliers** — add the two Ulduar guards to `src/Ai/Raid/Uld/UldMultipliers.h/.cpp` and push them
in `RaidUlduarStrategy::InitMultipliers` (`UldStrategy.cpp:532-547`). Follow the memoisation comment
convention already used there; a `find target` lookup per candidate action is the thing to avoid.

**Archimonde refactor** — repoint `ArchimondeCastFearImmunitySpellAction`
(`src/Ai/Raid/Hyjal/HyjalActions.h:242-251`, `.cpp:1045-1071`) at `RaidAntiFearAction`, deleting
`UseTremorTotemStrategy()` and its broken `ChangeStrategy` call. Add the matching guard multiplier to
Hyjal. This is the fix for defect 1 above.

No build-file edits: the module has no `CMakeLists.txt` and sources are globbed by
`src/cmake/macros/AutoCollect.cmake`. A CMake re-run is still needed to pick up new files.

## Open item for implementation

Confirm Fear Ward's WotLK cooldown and duration (spell 6346) before finalising the "tank, else
healer" fallback — if there is a cooldown, a single priest cannot cover both targets back to back
and the trigger will keep firing on a spell it cannot cast. `CanCastSpell` guards correctness either
way; this only affects how often the node churns.

## Verification

Static:
1. Build the core with the module (`AutoCollect` re-run) and confirm no unresolved symbols from the
   new shared header.
2. Grep that no `ChangeStrategy("+tremor"` remains anywhere.

In-game, per encounter, with a group containing at least one priest and one shaman:
3. **Onyxia (map 249)** — pull to below 40%. Expect the priest to Fear Ward the main tank on the
   phase transition, and the shaman to drop Tremor Totem and *keep* it down (this is the regression
   check for the earth-slot thrash — watch that Stoneskin does not replace it on the next GCD).
   Above 40%, expect normal Stoneskin/Strength of Earth behaviour to resume.
4. **Auriaya** — engage, wait through at least two Terrifying Screech casts (35s, then ~every 35s)
   and confirm bots are not scattered.
5. **Yogg-Saron** — reach P2 and confirm the totem is up during Malady; reach P3 and confirm it
   survives Deafening Roar at ~50s into the phase without the raid running through Lunatic Gaze.
6. **Archimonde (Hyjal)** — regression check that the refactored action still wards and still drops
   the totem, and that `tremor` is no longer stuck on the shaman after leaving the instance
   (`.bot strategy` / whatever the group's strategy dump command is).
