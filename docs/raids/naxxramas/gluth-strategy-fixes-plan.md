# Gluth (Naxxramas) Playerbot Strategy Fixes

## Context

The Gluth encounter in mod-playerbots has gaps that can wipe the raid:

1. **Frenzy is never dispelled.** Gluth's Frenzy (28371 / 54427 in 25-man) massively boosts his damage. Only the generic hunter tranq trigger exists (relevance 61.0, `GenericHunterStrategy.cpp:76-77`), and it targets `"current target"` — during Gluth, hunters #0/#1 usually target zombies, and raid actions at `ACTION_RAID+1` starve the generic trigger anyway. Frenzied Gluth kills the tank.
2. **Extra tanks (assist index >= 2) act as DPS.** `GluthChooseTargetAction` sends only assist tank index 1 to zombies; any further tanks fall into the generic DPS branch and stand on the boss. Zombies overwhelm the single kiter; if any zombie reaches Gluth it is eaten and heals him 5%.
3. **Non-aggro boss tank never positions.** `GluthPositionAction` moves the boss tank to the door anchor only `if has aggro on boss target`. After a Mortal Wound taunt swap (or any displacement) the tank without aggro stands wherever it happens to be, so the boss drifts away from the door anchor over time.
4. **Dead-tank role gaps.** Gluth's `IsAssistTankOfIndex` calls omit `ignoreDeadPlayers=true` (A'lar pattern, `TKActions.cpp:63`), so when a tank dies its role vanishes instead of promoting the next living tank.

**Decided with user (do NOT revisit):**
- **Fear Ward: skipped.** AzerothCore's `boss_gluth.cpp` casts no fear (Terrifying Roar is classic-only; WotLK Gluth has only Mortal Wound, Decimate, Frenzy, Berserk — verified by grep of `src/server/scripts/Northrend/Naxxramas`). Fear Ward would never be consumed. Positioning drift is addressed via anchor-keeping instead.
- **All extra tanks share the existing 12-waypoint kite circle** (no duty split).

Verified spell ids: Frenzy 10-man = 28371, 25-man = 54427 (`data/sql/base/db_world/spelldifficulty_dbc.sql:73`).

## Current code map

All paths relative to `g:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots`.

| File | Content |
|---|---|
| `src/Ai/Raid/Naxx/Action/NaxxActions_Gluth.cpp` | 3 Gluth actions (choose target :7-98, position :100-180, slowdown :182-206) |
| `src/Ai/Raid/Naxx/Action/NaxxActions.h:330-361` | Gluth action declarations; `GluthPositionAction` inherits `RotateAroundTheCenterPointAction` center (3293.61, -3149.01) r12, 12 waypoints |
| `src/Ai/Raid/Naxx/NaxxTriggers.cpp:210-248` / `NaxxTriggers.h:278-309` | Gluth triggers incl. `GluthMainTankMortalWoundTrigger` |
| `src/Ai/Raid/Naxx/NaxxMultipliers.cpp:520-558` | `GluthGenericMultiplier` |
| `src/Ai/Raid/Naxx/NaxxBossHelper.h:1030-1114` | `GluthBossHelper`: `mainTankPos25 {3331.48, -3109.06}` (door anchor), `beforeDecimatePos`, `decimatedZombiePct=10`, `BeforeDecimate()`, `UpdateBossAI()` |
| `src/Ai/Raid/Naxx/NaxxSpellIds.h:104-118` | Gluth ids; `SPELL_ENRAGE = 28371` exists only as a comment. Helpers `HasAnyAura` (:133), `MatchesAnySpellId` (:167) |
| `src/Ai/Raid/Naxx/NaxxStrategy.cpp:150-171` | Gluth TriggerNodes; `:209` multiplier registration |
| `src/Ai/Raid/Naxx/NaxxActionContext.h:55-57,100-102` / `NaxxTriggerContext.h:59-61,101-103` | name → factory registrations |

Reference patterns: Faerlina frenzy tranq (`NaxxStrategy.cpp:60-71`, trigger `NaxxTriggers.cpp:282-303`); A'lar two-boss-tank gate (`src/Ai/Raid/TK/TKActions.cpp:61-84`).

## Changes

### 1. Spell id constants — `NaxxSpellIds.h` (Gluth section ~line 109)

```cpp
static constexpr uint32 GluthFrenzy10 = 28371;
static constexpr uint32 GluthFrenzy25 = 54427;
```

### 2. Frenzy → Tranquilizing Shot (hunter, high priority)

Generic `"tranquilizing shot"` action hits `"current target"` (often a zombie), so a boss-explicit action is required.

**`NaxxTriggers.h`** (after `GluthMainTankMortalWoundTrigger` ~:309): `GluthFrenzyTrigger : public Trigger`, name `"gluth frenzy"`, member `GluthBossHelper helper`.

**`NaxxTriggers.cpp`** (after :248):

```cpp
bool GluthFrenzyTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER)
        return false;
    if (!helper.UpdateBossAI())
        return false;
    Unit* boss = AI_VALUE2(Unit*, "find target", "gluth");
    if (!boss || !boss->IsInCombat())
        return false;
    if (!NaxxSpellIds::HasAnyAura(boss, {NaxxSpellIds::GluthFrenzy10, NaxxSpellIds::GluthFrenzy25}) &&
        !botAI->GetAura("frenzy", boss))
        return false;
    return botAI->CanCastSpell("tranquilizing shot", boss);
}
```

(id check + name fallback = Naxx convention, cf. `GluthMainTankMortalWoundTrigger` :238-242)

**`NaxxActions.h`** (after `GluthSlowdownAction` ~:361) + **`NaxxActions_Gluth.cpp`** (append): `GluthTranquilizingShotAction : public Action`, name `"gluth tranquilizing shot"`, member helper:

```cpp
bool GluthTranquilizingShotAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
        return false;
    Unit* boss = AI_VALUE2(Unit*, "find target", "gluth");
    if (!boss)
        return false;
    return botAI->CastSpell("tranquilizing shot", boss);
}
```

### 3. Tank assignment — `NaxxActions_Gluth.cpp`, `GluthChooseTargetAction::Execute`

- `:35` → `if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0, true))` (add `true` = ignoreDeadPlayers, A'lar pattern — dead assist-0 promotes next living tank to boss duty).
- `:39` → replace `else if (botAI->IsAssistTankOfIndex(bot, 1))` with `else if (botAI->IsTank(bot))` — every other tank (index 1, 2, 3, …) picks up zombies. Keep selection body (nearest zombie >10% hp, not on them, within 10yd) unchanged.

### 4. Mortal Wound swap robustness — `NaxxTriggers.cpp:229`

`GluthMainTankMortalWoundTrigger::IsActive`: `IsAssistTankOfIndex(bot, 0)` → `IsAssistTankOfIndex(bot, 0, true)`.

### 5. Anchor keeping + zombie tanks — `NaxxActions_Gluth.cpp`, `GluthPositionAction::Execute`

- `:107` → add `, true` to `IsAssistTankOfIndex(bot, 0)`.
- Keep aggro-holder branch (:109-131, MoveTo `mainTankPos25` / MoveInside 2.0f) as is; add `else` for boss tank WITHOUT aggro:

```cpp
// Non-aggro boss tank stages at the door anchor so taunt swaps don't strand it.
return MoveInside(NAXX_MAP_ID, helper.mainTankPos25.first, helper.mainTankPos25.second,
                  bot->GetPositionZ(), 5.0f, MovementPriority::MOVEMENT_COMBAT);
```

Radius 5.0f vs holder's 2.0f keeps the two tanks off the exact same spot but within taunt range.

- `:133` → replace `else if (botAI->IsAssistTankOfIndex(bot, 1))` with `else if (botAI->IsTank(bot))` — all zombie tanks run the beforeDecimate stand-off (:135-144) and the shared waypoint kite (:147-153). Multiple kiters on one circle are fine: each advances from its own nearest waypoint.

### 6. Wiring

**`NaxxTriggerContext.h`**: creator `"gluth frenzy"` (after :61) + static factory returning `new GluthFrenzyTrigger(ai)` (after :103).

**`NaxxActionContext.h`**: creator `"gluth tranquilizing shot"` (after :57) + static factory (after :102).

**`NaxxStrategy.cpp`** (after the `gluth main tank mortal wound` node :168):

```cpp
triggers.push_back(new TriggerNode("gluth frenzy",
    { NextAction("gluth tranquilizing shot", ACTION_RAID + 4) }
));
```

`ACTION_RAID + 4` mirrors the Faerlina frenzy precedent (`NaxxStrategy.cpp:68`) and outranks all Gluth nodes (`RAID+1`), fixing generic-tranq starvation. Momentary preemption of hunter zombie duty is one GCD — acceptable.

### 7. No multiplier change

`GluthGenericMultiplier` zeroes only `DpsAssistAction` / `TankAssistAction` / `FleeAction` / `CastDebuffSpellOnAttackerAction` / MT taunts at 5 Mortal Wound stacks / `PetAttackAction` on zombies — via `dynamic_cast`, none match the new plain `Action` subclass. Leave untouched.

## Remaining known gaps (documented, not fixed)

- If ALL extra tanks die, zombies fixate Gluth (`boss_gluth.cpp:126` `AttackStart(me)`) and get eaten; hunters #0/#1 shooting boss-bound zombies remain the only mitigation.
- 10-man uses 25-man positions (pre-existing, commented-out 10-man branch).

## Verification

1. `python apps/codestyle/codestyle-cpp.py` (repo hard rule).
2. Build only if the user asks (cannot compile module headless here — offer hand-off).
3. In-game 25-man: bot raid with 3-4 tanks (MT + assist flags), 2 hunters. Engage Gluth; confirm:
   - On Frenzy (boss emote "goes into a killing frenzy"), a hunter tranqs Gluth within a GCD.
   - All tanks beyond MT/assist-0 kite zombies on the circle; run to `beforeDecimatePos` during Decimate.
   - Both boss tanks hold near {3331.48, -3109.06}; the non-aggro tank walks back after displacement.
   - Mortal Wound 5-stack taunt swap still works; kill assist-0 mid-fight → next living tank takes boss duty.
