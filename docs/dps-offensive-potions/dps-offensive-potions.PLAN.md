# DPS Offensive Potions During Burst

## Context

Today playerbots only ever use **defensive** consumables in combat. The `maintenance`
command (`MaintenanceAction::Execute`, `src/Ai/Base/Actions/TrainerAction.cpp:176`) stocks
heal/mana potions via `PlayerbotFactory::InitPotions()`
(`src/Bot/Factory/PlayerbotFactory.cpp:3820`), and the default-enabled `UsePotionsStrategy`
("potions") fires them reactively (critical health -> `healing potion`, medium mana ->
`mana potion`). There is no offensive/DPS potion anywhere — not stocked, no action, no trigger.

Goal: DPS specs pop a **level-appropriate offensive potion during their burst window**, while
tanks/healers keep using health/mana potions exactly as now (no change to their behaviour).

Two decisions locked with the user:
- **Caster/melee split** potion ladders (each DPS bot uses the ladder matching its damage stat).
- **Fire only in the burst window on dungeon/world bosses** — reuse the existing burst gate so
  the potion is released together with the cooldown dump and never wasted on trash.

The burst infrastructure makes the timing nearly free: `BurstWindowStrategy`
(`src/Ai/Base/Strategy/BurstWindowStrategy.cpp`) installs a multiplier that returns `0.0f`
(suppresses) for any action whose **name** is in `burstCooldownNames`
(`src/Ai/Base/Combat/BurstCooldowns.cpp:22`) until the main tank has held the boss for a dwell
period; `NaxxBurstWindowMultiplier` further gates the same named actions per-boss. Adding
`"offensive potion"` to that name set means the potion is timed exactly like bloodlust/trinkets
with zero new gating logic.

## Potion ladders (hardcoded item ids, matches the WizardOilId/SharpeningStoneId idiom)

| Band  | Melee/physical DPS            | Caster (spellpower) DPS          |
|-------|-------------------------------|----------------------------------|
| 45-60 | Haste Potion (22838)          | Haste Potion (22838)             |
| 61-70 | Insane Strength Potion (22828)| Destruction Potion (22839)       |
| 71-80 | Potion of Speed (40211)       | Potion of Wild Magic (40212)     |

Nothing offensive exists below req-level 45, so DPS < 45 stock/use none (expected, not a bug).
Each pick is guarded by `proto->RequiredLevel <= bot level`.

## Implementation

### 1. Item-id enum — `src/Bot/PlayerbotAI.h`
Add next to `WizardOilId`/`ManaOilId` (~line 210):
```cpp
enum OffensivePotionId
{
    HASTE_POTION            = 22838,
    INSANE_STRENGTH_POTION  = 22828,
    DESTRUCTION_POTION      = 22839,
    POTION_OF_SPEED         = 40211,
    POTION_OF_WILD_MAGIC    = 40212
};
```

### 2. Stocking — `PlayerbotFactory::InitPotions()` (`src/Bot/Factory/PlayerbotFactory.cpp:3820`)
After the existing heal/energize loop, add a DPS-only block:
- Skip unless the bot's role is DPS (`botAI->IsDps(bot)` — get `botAI` via
  `GET_PLAYERBOT_AI(bot)`, the pattern already used elsewhere in the factory).
- Decide **caster vs physical** by reusing the exact class/spec branching `InitConsumables`
  already uses to choose wizard/mana oil (caster) vs sharpening/weightstone (melee) — grep
  `InitConsumables` in the same file (~`:917`+) and factor the predicate out (e.g. a small
  `IsSpellDamageSpec()` helper) so the two stay consistent.
- Pick the item id from the ladder by `level` band, skip if `proto->RequiredLevel > level`.
- Only stock if the bot isn't already carrying it (reuse the existing
  "already have -> continue" guard style; a bag item-count check on the id is enough), then
  `StoreNewItemInInventorySlot(bot, itemId, urand(maxCount/2, maxCount))`.

Because stocking lives inside `InitPotions`, it automatically inherits every existing call site
— `MaintenanceAction` (non-alt + alt under `altMaintenancePotions`) and
`AutoMaintenanceOnLevelupAction`. No new call sites, no new alt-config flag.

### 3. Combat action — `src/Ai/Base/Actions/UseItemAction.{h,cpp}`
Add alongside `UseHealingPotion`/`UseManaPotion`:
```cpp
class UseOffensivePotion : public UseItemAction
{
public:
    UseOffensivePotion(PlayerbotAI* botAI) : UseItemAction(botAI, "offensive potion") {}
    bool isUseful() override;
};
```
`isUseful` (mirror `UseHealingPotion::isUseful`, `UseItemAction.cpp:401`):
`return PlayerbotAI::IsDps(bot) && AI_VALUE2(bool, "combat", "self target");`
`isPossible` is inherited (`item count > 0`, `:397`).

### 4. Inventory resolution — `src/Ai/Base/Actions/InventoryAction.cpp:262`
Add an `if (text == "offensive potion")` branch mirroring the `"healing potion"` branch, using a
new `FindOffensivePotionVisitor`.

### 5. Item visitor — `src/Mgr/Item/ItemVisitors.h:291`
Add `FindOffensivePotionVisitor : public FindUsableItemVisitor` whose `Accept` matches the five
`OffensivePotionId` ids (a static id set is simplest and unambiguous, and reuses the base
usability check). `FindPotionVisitor` can't be reused because offensive potions aren't
HEAL/ENERGIZE effect potions.

### 6. Action registration — `src/Ai/Base/ActionContext.h`
Register `creators["offensive potion"]` -> `new UseOffensivePotion(botAI)` next to the existing
`healing potion` / `mana potion` entries (~line 119).

### 7. Burst gate hookup — `src/Ai/Base/Combat/BurstCooldowns.cpp:22`
Add `"offensive potion"` to `burstCooldownNames` (under a `// consumable` comment). This alone
gives it the tank-hold dwell **and** the Naxx per-boss windows (`NaxxBurstWindowMultiplier`
early-outs on `IsBurstCooldownAction`, so it picks up the new name automatically).

### 8. Trigger — new `OffensivePotionTrigger`
Add in `src/Ai/Base/Trigger/GenericTriggers.{h,cpp}` and register in
`src/Ai/Base/TriggerContext.h`. `IsActive` fires when:
`botAI->IsDps(bot)` && bot in combat && `current target` is a `IsDungeonBoss()`/`isWorldBoss()`
creature (mirror the boss check in `HoldBurstUntilTankEngagedMultiplier::GetValue`,
`BurstWindowStrategy.cpp:20-27`). The trigger only expresses "want to"; the burst multiplier
decides "not yet", so the boss/tank-hold timing isn't duplicated here beyond the boss gate that
keeps trash from arming the trigger.

### 9. Strategy wiring — `UsePotionsStrategy::InitTriggers` (`src/Ai/Base/Strategy/UsePotionsStrategy.cpp:29`)
```cpp
triggers.push_back(new TriggerNode(
    "offensive potion", { NextAction("offensive potion", ACTION_HIGH) }));
```
`ACTION_HIGH` puts it in the same tier as class burst cooldowns so it lands inside the dump.
`UsePotionsStrategy` is already in the default combat strategy list
(`src/Bot/Factory/AiFactory.cpp:289`), so no new strategy to register.

### 10. (Optional) master toggle — `src/PlayerbotAIConfig.{h,cpp}` + `playerbots.conf.dist`
Add `bool offensivePotions` (default `true`) read near the other consumable configs
(`~PlayerbotAIConfig.cpp:623`). Guard the stocking block (step 2) and the trigger (step 8) so a
server can disable the feature wholesale. Skip if a config toggle isn't wanted.

## Known interactions (call out, not blockers)
- WotLK shares one combat-potion use per fight, so a DPS that pops the offensive potion can't
  also use a healing potion that fight. That's acceptable for a DPS role; the healing-potion
  trigger stays `critical health`-gated and higher-priority if it does fire.
- Role is read at stock time and at use time via `IsDps`; a spec change is reconciled on the
  next `maintenance`/level-up restock, same as every other consumable.

## Verification
1. Build the module (hand off to user — headless build not available here; static review first).
2. In-game: create/possess a melee DPS bot (e.g. Fury warrior) lvl 75 and a caster DPS bot
   (e.g. Mage) lvl 75; run `maintenance`. Confirm bags now contain Potion of Speed (40211) /
   Potion of Wild Magic (40212) respectively; a tank/healer bot gets none.
3. Pull a dungeon boss with a bot main tank. Confirm the offensive potion is **held** until the
   tank has aggro (~4s dwell), then fires together with trinkets/cooldowns. Confirm it does NOT
   fire on trash pulls or when the bot is solo/non-boss.
4. Repeat a level-60 melee bot -> Haste Potion (22838); a level-40 bot -> no offensive potion.
5. On Naxx bosses (e.g. Thaddius), confirm the potion respects the per-boss `NaxxBurstWindow`
   the same way bloodlust/trinkets already do.
