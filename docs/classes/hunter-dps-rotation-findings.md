# Hunter PvE raid DPS (BM / MM / Survival) — findings

Why hunter bots underperformed in raid PvE, why the damage meter showed **zero Arcane Shot casts**
across all three specs, what was changed, and what was deliberately left alone. Companion to
`hunter-strategy-improvements-plan.md`.

## How the engine picks an ability

`Engine::DoNextAction` ([Engine.cpp:144](../../src/Bot/Engine/Engine.cpp#L144)) pushes every fired
trigger's `NextAction`s plus each strategy's `getDefaultActions()` into a relevance-ordered queue,
pops in descending relevance, and **breaks on the first action that returns `true`**. Constants
([Strategy.h:53-65](../../src/Bot/Engine/Strategy/Strategy.h#L53-L65)): `ACTION_DEFAULT 5`,
`ACTION_NORMAL 10`, `ACTION_HIGH 20`, `ACTION_MOVE 30`, `ACTION_INTERRUPT 40`, `ACTION_RAID 60`.

Three facts the fixes rely on:

- `PlayerbotAI::CanCastSpell` builds its probe spell with `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST`,
  so `isPossible()` never checks mana and `SPELL_FAILED_OUT_OF_RANGE` is whitelisted. Resource and
  pacing gating has to live in a trigger, not in the action.
- `BoostTrigger::IsActive` ([GenericTriggers.cpp:422](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L422))
  needs `balance <= 50` unless the target is a player. On a boss the raid is normally "winning", so a
  `BoostTrigger`-keyed cooldown can go unspent for a whole encounter.
- `BurstWindowStrategy` + `IsBurstCooldownAction`
  ([BurstCooldowns.cpp:22-52](../../src/Ai/Base/Combat/BurstCooldowns.cpp#L22-L52)) already hold
  `rapid fire`, `bestial wrath` and `readiness` back until the encounter's real burst window. A
  trigger only has to say "off cooldown"; the multiplier decides *when*.

## Findings

### Arcane Shot was disabled by two independent regressions

Both were in `CastArcaneShotAction::isUseful()`
([HunterActions.cpp:26](../../src/Ai/Class/Hunter/HunterActions.cpp#L26)).

| # | Problem | Detail |
|---|---------|--------|
| H1 | Inverted spec gate — killed BM and MM | `if (!target \|\| !botAI->HasSpell("explosive shot")) return false;`. Explosive Shot is the Survival 51-point talent, so every BM and MM bot bailed out on every tick. The original (`8ca4ab134`) was the opposite check, and its commit message states the intent: use Arcane Shot *until* Explosive Shot is learned. `a76f2ca26` (#2435, HasSpell/HasAura refactor) flipped the polarity while converting to the string overload — the two sibling conversions in the same hunk (aspect of the hawk, immolation trap) came out correct. |
| H2 | Armor-penetration check read the wrong update field — killed Survival | `GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1) + GetUInt32Value(CR_ARMOR_PENETRATION)`. `CR_ARMOR_PENETRATION` is `24`, an index *into the rating block*, not an update-field index; field 24 is `UNIT_FIELD_HEALTH`, so this read the bot's current hit points — orders of magnitude above the 435 threshold for any level-80 hunter. The first term is `CR_WEAPON_SKILL`, always 0. Correct form is `GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_ARMOR_PENETRATION)`; broken by `b661264c5` (#1576, "Removes compiler warning"). |

H1 blocked BM/MM, H2 blocked Survival — zero casts everywhere, which is what the meter showed.

### Rotation — all specs

| # | Problem | Detail |
|---|---------|--------|
| R1 | Rapid Fire barely fired in raids | `RapidFireTrigger : BoostTrigger` — see the balance rule above. A 3-minute, 40 %-haste cooldown went unspent for the whole encounter. |
| R2 | Pet death was unrecoverable mid-fight | `HunterPetStrategy` (call/revive pet) is only added to the non-combat engine ([AiFactory.cpp:535-537](../../src/Bot/Factory/AiFactory.cpp#L535-L537)); the combat set (`bm\|mm\|surv`, `cc`, `dps assist`, `aoe`, `bdps`) carried only `mend pet`. When the pet died to raid damage, BM lost ~35-40 % of its throughput for the rest of the fight and both Kill Command and Bestial Wrath went dead with it — both actions resolve `"pet target"`. |

### Beast Mastery

| # | Problem | Detail |
|---|---------|--------|
| B1 | `BestialWrathTrigger` was dead code | Defined in `HunterTriggers.h` but never registered in `HunterTriggerFactoryInternal` and never referenced. BW only competed as a `19.0` entry in `getDefaultActions()`, so it landed whenever nothing else won the tick — not in a burst window. |
| B2 | `KillCommandTrigger` checked an aura that never lands | `IsActive` tested the KC aura on the *hunter* while `CastKillCommandAction` resolves `"pet target"`, and `CastBuffSpellAction::isUseful` then checks the aura on the *pet*, where Kill Command never puts one. It worked only because the cooldown paced it. |

### Marksmanship

| # | Problem | Detail |
|---|---------|--------|
| M1 | Chimera Shot had no trigger | Only a `5.5` default. With `no stings` at 17.0, the bot hard-recast Serpent Sting instead of rolling it forward with Chimera Shot — which is the whole point of the spell. |
| M2 | Aimed Shot had no trigger | 10 s cooldown, `5.4` default. Lower impact, since the default ladder does reach it, but the same treatment applies. |

### Survival

Survival's Explosive Shot handling and the Trap Launcher node are correct as they stand and were not
touched. The only Survival change is dropping the `arcane shot` entry from `getDefaultActions()`,
which is unreachable now that H1 is fixed the right way round (Survival always has Explosive Shot).

## What changed

- **Arcane Shot (H1, H2)**: `CastArcaneShotAction::isUseful` now refuses when the bot *has* Explosive
  Shot, and reads armor penetration as `PLAYER_FIELD_COMBAT_RATING_1 + CR_ARMOR_PENETRATION`.
- **Rapid Fire (R1)**: `RapidFireTrigger` re-keyed from `BoostTrigger` to `SpellNoCooldownTrigger`.
  Node stays at `ACTION_HIGH + 9`; `BurstWindowStrategy` places the cast.
- **Pet recovery (R2)**: `GenericHunterStrategy::InitTriggers` gained `no pet` → `call pet`
  (`ACTION_NORMAL + 4.5`) and `hunters pet dead` → `revive pet` (`ACTION_NORMAL + 4`). Both sit under
  every shot priority, so they only spend a GCD the rotation had nothing else for.
- **Bestial Wrath (B1)**: `BestialWrathTrigger` re-keyed to `SpellNoCooldownTrigger`, registered as
  `"bestial wrath"`, and given a node at `ACTION_HIGH + 8.5` — next to Rapid Fire, so The Beast Within
  stacks both in the same window. The `19.0` default entry is gone.
- **Kill Command (B2)**: `KillCommandTrigger` is now a plain `SpellNoCooldownTrigger`; its
  `IsActive` override in `HunterTriggers.cpp` is deleted. Behaviour is the same, minus the misleading
  aura check.
- **Chimera Shot (M1)**: new `ChimeraShotNoCdTrigger`, registered as `"chimera shot no cd"`, wired in
  `MarksmanshipHunterStrategy` at `ACTION_NORMAL + 7` — above the sting refresh, which moved down to
  `ACTION_NORMAL + 6.5`.
- **Aimed Shot (M2)**: new `AimedShotNoCdTrigger`, registered as `"aimed shot no cd"`, MM only, at
  `ACTION_NORMAL + 5.5`.
- **Relevances**: every bare float in the four strategy files converted to `ACTION_*` constants plus
  an offset, matching `FrostDKStrategy.cpp` and `ArmsWarriorStrategy.cpp`. Numeric values are
  unchanged except where listed above.

## Final relevance tables

Shared by all three specs (`GenericHunterStrategy`):

| Relevance | Trigger | Action |
|-----------|---------|--------|
| 61 | tranquilizing shot enrage / magic | tranquilizing shot |
| 37 | enemy within melee | explosive trap *(see D2)* |
| 35 | low health / medium threat / enemy too close | deterrence / feign death / disengage *(see D4)* |
| 34 | enemy too close for auto shot | flee |
| 30 | no ammo | equip upgrades |
| 29.5 | hunter's mark | hunter's mark |
| 29 | rapid fire *(off cooldown)* | rapid fire *(burst-gated)* |
| 28 | aspect of the viper | aspect of the viper |
| 27 | low tank threat / md on main tank and light aoe | misdirection on main tank |
| 22 / 21 | hunters pet medium / low health | mend pet |
| 22 / 21 | enemy within melee | mongoose bite / wing clip *(see D2)* |
| 20 | has aggro / concussive shot on snare target | concussive shot *(see D1)* |
| 17 | trap launcher: explosive trap no cd | trap launcher: explosive trap |
| 14.5 / 14 | no pet / hunters pet dead | call pet / revive pet |

Per spec, on top of the shared table:

| Relevance | Beast Mastery | Marksmanship | Survival |
|-----------|---------------|--------------|----------|
| 40 | intimidation | silencing shot | — |
| 28.5 | bestial wrath *(burst-gated)* | — | — |
| 28 | — | — | lock and load → explosive shot rank 4 |
| 18.5 | kill command | kill command | kill command |
| 18 | kill shot | kill shot | kill shot |
| 17.5 | low mana → viper sting | low mana → viper sting | explosive shot |
| 17 | no stings → serpent sting | chimera shot | — |
| 16.5 | serpent sting on attacker | no stings → serpent sting | black arrow |
| 16 | — | serpent sting on attacker | low mana → viper sting |
| 15.5 | — | aimed shot | no stings → serpent sting |
| 15 | — | — | serpent sting on attacker |
| 5.7 → 5.1 | *defaults* | *defaults* | *defaults* |

## Still open — documented, not implemented

These came out of the same audit and were deliberately left for a later pass.

| # | Gap | Detail |
|---|-----|--------|
| D1 | Concussive Shot on aggro wastes a GCD on bosses | `has aggro` → concussive shot @ 20 ([GenericHunterStrategy.cpp:68](../../src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L68)). Bosses are snare-immune; the cast still burns the GCD every tick the hunter holds aggro. Fix is to gate the trigger to non-boss targets. |
| D2 | Melee weave outranks the whole shot list | `enemy within melee` pushes explosive trap @ 37, mongoose bite @ 22, wing clip @ 21. At 37 the trap beats everything except dispels, and melee weaving is a straight DPS loss for a ranged spec in WotLK. |
| D3 | Aspect of the Viper hysteresis is far too wide | Enters at `mana < lowMana/2` = 7.5 % (`AiPlayerbot.LowMana = 15`, [HunterTriggers.cpp:79-89](../../src/Ai/Class/Hunter/HunterTriggers.cpp#L79-L89)) and only leaves at `mana >= 60 %` ([HunterTriggers.cpp:26-43](../../src/Ai/Class/Hunter/HunterTriggers.cpp#L26-L43)) — a long stretch at half ranged damage. Tightening the exit to ~35 % is the obvious move. |
| D4 | Disengage is raid-hostile | `enemy too close for auto shot` → disengage @ 35. Fixed-vector backwards leap; on Thorim / Mimiron / Gunship-style encounters it lands bots in fire, off platforms, or out of healer range. Should be suppressed in raid instances in favour of the existing `flee` alternative. |
| D5 | `trap launcher: explosive trap` @ 17 sits mid-rotation | Generic to all specs, between Survival's explosive shot (17.5) and black arrow (16.5). Left alone because Survival's trap usage is what the user wanted preserved. |
| D6 | `ExplosiveShotTrigger` fires against its own cooldown | `DebuffTrigger` overridden to `BuffTrigger::IsActive()` ([HunterTriggers.h:171-176](../../src/Ai/Class/Hunter/HunterTriggers.h#L171-L176)), so it re-pushes on debuff-absence while the 6 s cooldown still has the spell unavailable. Harmless (the action just fails) but it costs a queue iteration. |
| D7 | `"freezing trap on cc"` has no action creator | `HunterCcStrategy` pushes `NextAction("freezing trap on cc", …)` ([GenericHunterStrategy.cpp:117](../../src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L117)) but only `"freezing trap"` and `"scare beast on cc"` are registered. The node logs `A:freezing trap on cc - UNKNOWN` and hunter bots never trap a CC target. Found during this audit, outside the plan's scope. |

Also left as-is by explicit decision: `medium threat` → **feign death** @ 35 and `low health` →
**deterrence** @ 35 keep their current relevance and triggers.

## Verification

The module cannot be compiled headless in this environment, so:

1. **Static check** (done): every `TriggerNode` and `NextAction` name added has a matching
   `creators[...]` entry. An unregistered trigger name silently no-ops — `Engine::ProcessTriggers`
   skips it with `continue` and no warning — and an unregistered action logs `A:<name> - UNKNOWN`.
   The audit that found D7 was this same sweep.
2. **Build** the module, then in-game run `.playerbot ai debug` on one bot per spec and read the
   `T:` / `PUSH:` / `A:… OK|FAILED|IMPOSSIBLE|USELESS` lines through a boss pull.
3. **Damage meter**, same boss and gear level, before/after: Arcane Shot present for BM and MM and
   absent for Survival; Rapid Fire and Bestial Wrath spent once per burst window rather than never;
   Survival's Explosive Shot / Black Arrow / Trap Launcher usage unchanged from before the patch.
4. **Pet recovery**: kill a BM bot's pet mid-fight and confirm it comes back without the bot dropping
   its rotation for the whole encounter — and that a live pet never triggers `call pet`.
