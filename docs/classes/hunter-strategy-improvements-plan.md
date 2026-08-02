# Hunter PVE raid rotation — findings + fixes (BM / MM / Survival)

## Context

Hunter bots underperform in raid PVE and the damage meter shows **zero Arcane Shot casts** across
all three specs. The hunter class AI (`src/Ai/Class/Hunter/`) never received the rotation rework that
Frost DK (`25040abd9`) and Arms/Fury (`1f5e5ebf1`) got: it still uses bare float relevances instead of
the `ACTION_*` constants, keys cooldown-paced abilities off debuff-presence triggers, and carries two
upstream regressions that silently disabled Arcane Shot entirely.

Goal: explain the Arcane Shot zero, then land the rotation/raid-behaviour fixes that follow from the
same audit. Deliverable follows the existing class-doc convention:
`docs/classes/warrior-arms-fury-dps-findings.md` → `docs/classes/fury-and-arms-rotation-improvements-plan.md`.

## Framework facts the fixes rely on

- `Engine::DoNextAction` ([Engine.cpp:144](modules/mod-playerbots/src/Bot/Engine/Engine.cpp#L144)):
  triggers push their `NextAction` lists, then `getDefaultActions()` is pushed verbatim, then the queue
  is popped highest-relevance-first through `isUseful()` → multipliers → `isPossible()` → `Execute()`,
  breaking on the first success.
- `PlayerbotAI::CanCastSpell` builds the probe spell with `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST`, so
  **`isPossible()` never checks mana**, and `SPELL_FAILED_OUT_OF_RANGE` is whitelisted. Resource/pacing
  gating must live in a trigger, not in the action.
- `BurstWindowStrategy` + `IsBurstCooldownAction`
  ([BurstCooldowns.cpp:34](modules/mod-playerbots/src/Ai/Base/Combat/BurstCooldowns.cpp#L34)) already
  hold `rapid fire`, `bestial wrath`, `readiness` until the encounter's real burst window — a trigger
  only needs to say "off cooldown", the multiplier decides *when*.

## Part 1 — Why Arcane Shot is never cast

Both blockers are in `CastArcaneShotAction::isUseful()`
([HunterActions.cpp:26-39](modules/mod-playerbots/src/Ai/Class/Hunter/HunterActions.cpp#L26-L39)).

### (a) Inverted spec gate — kills BM and MM

```cpp
if (!target || !botAI->HasSpell("explosive shot"))
    return false;
```

Explosive Shot is the Survival 51-point talent, so every BM and MM bot fails here on every tick.

Original code (`8ca4ab134` "Hunter Overhaul") was the **opposite**, and the commit message states the
intent: *"never use it after explosive shot is learned (so the hunter can use it as it levels survival,
but drops it after that)"*:

```cpp
if (bot->HasSpell(53301) || bot->HasSpell(60051) || bot->HasSpell(60052) || bot->HasSpell(60053))
    return false;
```

Commit `a76f2ca26` (#2435, HasSpell/HasAura refactor) flipped the polarity while converting to the
string overload. The two sibling conversions in the same hunk (aspect of the hawk, immolation trap)
were converted correctly — only Arcane Shot was inverted.

### (b) Armor-penetration check reads the wrong update field — kills Survival

```cpp
int32 armorPenRating =
    bot->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1) + bot->GetUInt32Value(CR_ARMOR_PENETRATION);
if (armorPenRating > 435)
    return false;
```

`CR_ARMOR_PENETRATION` is `24` ([Unit.h:248](src/server/game/Entities/Unit/Unit.h#L248)) — an index
*into the rating block*, not an update-field index. Field 24 is `UNIT_FIELD_HEALTH`
(`OBJECT_END 6 + 0x0012`, [UpdateFields.h:96](src/server/game/Entities/Object/Updates/UpdateFields.h#L96)),
so this reads the bot's **current hit points**. Any level-80 hunter is orders of magnitude above 435.
The first term is `CR_WEAPON_SKILL` (always 0).

Original was correct: `GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_ARMOR_PENETRATION)`. Broken by
`b661264c5` (#1576, "Removes compiler warning").

**Net:** (a) blocks BM/MM, (b) blocks SV. Zero casts everywhere — matches the meter.

### Fix

`src/Ai/Class/Hunter/HunterActions.cpp`:

```cpp
bool CastArcaneShotAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    // Explosive Shot takes the slot in the Survival rotation.
    if (botAI->HasSpell("explosive shot"))
        return false;

    // Arcane Shot deals magic damage, so armor penetration does nothing for it - past ~435 rating the
    // physical fillers out-damage it.
    int32 armorPenRating = bot->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_ARMOR_PENETRATION);
    if (armorPenRating > 435)
        return false;

    return true;
}
```

Then drop the now-dead `NextAction("arcane shot", 5.3f)` from
[SurvivalHunterStrategy.cpp:60](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/SurvivalHunterStrategy.cpp#L60)
so it stops burning a queue iteration per tick.

## Part 2 — Rotation fixes (all specs)

Items 1-2 are in scope. Items 3-6 are **deferred** (behavioural / raid-safety, recorded in the findings
doc as known gaps but not implemented in this pass).

`src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp`, plus trigger classes in `HunterTriggers.*`
and registration in `HunterAiObjectContext.cpp`.

1. **Rapid Fire barely fires in raids.** `RapidFireTrigger : BoostTrigger`
   ([HunterTriggers.h:84](modules/mod-playerbots/src/Ai/Class/Hunter/HunterTriggers.h#L84)) and
   `BoostTrigger::IsActive` ([GenericTriggers.cpp:422](modules/mod-playerbots/src/Ai/Base/Trigger/GenericTriggers.cpp#L422))
   requires `balance <= 50` unless the target is a player. On a boss the raid is normally "winning", so
   a 3-minute 40%-haste cooldown goes unspent for the whole encounter. Re-key it on off-cooldown
   (`SpellNoCooldownTrigger`) and let `BurstWindowStrategy` place it — same shape as the DK's
   `army of the dead`.

2. **Pet death is unrecoverable mid-fight.** `HunterPetStrategy` (call/revive pet) is only added to the
   non-combat engine ([AiFactory.cpp:535-537](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L535-L537));
   the combat set is `bm|mm|surv`, `cc`, `dps assist`, `aoe`, `bdps`
   ([AiFactory.cpp:368-376](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L368-L376)) and only
   carries `mend pet`. When the pet dies to raid damage, BM loses ~35-40% of its throughput for the rest
   of the fight and both Kill Command and Bestial Wrath go dead (both resolve `"pet target"`). Add
   in-combat `call pet` (instant) and `revive pet` nodes to `GenericHunterStrategy::InitTriggers`,
   below the shot priorities.

### Explicitly out of scope — leave alone

The hunter's survivability cooldowns keep their current wiring
([GenericHunterStrategy.cpp:73-75](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L73-L75)):

- `medium threat` → **feign death** @ 35.0 — stays as-is. It is the hunter's threat-drop / damage-avoid
  cooldown and no relevance or trigger change should touch it.
- `low health` → **deterrence** @ 35.0 — same, unchanged.

### Deferred (document only, do not implement in this pass)

3. **Concussive Shot on aggro is a wasted GCD on bosses.** `has aggro` → concussive shot 20.0
   ([GenericHunterStrategy.cpp:67](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L67)).
   Bosses are snare-immune; the cast still consumes the GCD every tick the hunter holds aggro. Gate the
   trigger to non-boss targets.

4. **Melee weave outranks the whole shot list.** `enemy within melee` pushes explosive trap **37.0**,
   mongoose bite 22.0, wing clip 21.0
   ([GenericHunterStrategy.cpp:86-88](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L86-L88)).
   At 37.0 the trap beats everything except dispels, and melee weaving is a straight DPS loss for a
   ranged spec in WotLK. Drop these below the shot priorities.

5. **Aspect of the Viper hysteresis is far too wide.** Enter at `mana < lowMana/2` = 7.5%
   (`AiPlayerbot.LowMana = 15`, [HunterTriggers.cpp:92-93](modules/mod-playerbots/src/Ai/Class/Hunter/HunterTriggers.cpp#L92-L93)),
   leave only at `mana >= 60%` ([HunterTriggers.cpp:44-45](modules/mod-playerbots/src/Ai/Class/Hunter/HunterTriggers.cpp#L44-L45)).
   That is a long stretch at half ranged damage. Tighten the exit to ~35%.

6. **Disengage is raid-hostile.** `enemy too close for auto shot` → disengage 35.0
   ([GenericHunterStrategy.cpp:90](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp#L90)).
   Fixed-vector backwards leap — in Thorim/Mimiron/Gunship-style encounters it lands bots in fire, off
   platforms, or out of healer range. Suppress in raid instances and prefer the existing `flee`
   alternative.

## Part 3 — Per-spec fixes

**Beast Mastery** — `BeastMasteryHunterStrategy.cpp`

- `BestialWrathTrigger` ([HunterTriggers.h:90-94](modules/mod-playerbots/src/Ai/Class/Hunter/HunterTriggers.h#L90-L94))
  is defined but never registered in `HunterTriggerFactoryInternal` and never referenced. BW works only
  as a `19.0` default push. Wire it as a trigger next to Rapid Fire so the two land in the same
  "The Beast Within" window, or delete the dead class.
- `KillCommandTrigger::IsActive` checks the KC aura on the *hunter* while `CastKillCommandAction`
  resolves `"pet target"` and `CastBuffSpellAction::isUseful` checks the aura on the *pet*, where it
  never lands. It works only because the cooldown paces it. Simplify to an off-cooldown trigger.

**Marksmanship** — `MarksmanshipHunterStrategy.cpp`

- **Chimera Shot has no trigger**, only a `5.5` default
  ([MarksmanshipHunterStrategy.cpp:22](modules/mod-playerbots/src/Ai/Class/Hunter/Strategy/MarksmanshipHunterStrategy.cpp#L22)).
  Because `no stings` sits at 17.0, the bot hard-recasts Serpent Sting instead of refreshing it with
  Chimera Shot. Add a `chimera shot` off-cooldown trigger *above* the sting refresh.
- Aimed Shot likewise has no trigger (10s cd, `5.4` default). Lower impact — the default ladder does
  reach it — but the same treatment applies.

**Survival** — `SurvivalHunterStrategy.cpp`

Survival's Explosive Shot handling and the Trap Launcher node stay exactly as they are — no priority
reshuffle, no trigger re-key. The only Survival change is removing the dead `arcane shot` default entry
from Part 1.

Recorded in the findings doc as observations, deliberately **not** acted on:

- `trap launcher: explosive trap` @ 17.0 (generic, all specs) sits between explosive shot 17.5 and
  black arrow 16.5.
- `ExplosiveShotTrigger : DebuffTrigger` overridden to `BuffTrigger::IsActive()`
  ([HunterTriggers.h:155-160](modules/mod-playerbots/src/Ai/Class/Hunter/HunterTriggers.h#L155-L160))
  fires on debuff-absence against a 6s cooldown, so it re-pushes while the spell is unavailable.
- Lock and Load → `explosive shot rank 4` @ 28.0 is correct.

## Files to touch

| File | Change |
|---|---|
| `src/Ai/Class/Hunter/HunterActions.cpp` | Arcane Shot `isUseful` — both regressions |
| `src/Ai/Class/Hunter/HunterTriggers.h/.cpp` | Rapid Fire / Kill Command / Chimera Shot re-keyed to off-cooldown (Survival's Explosive Shot trigger untouched) |
| `src/Ai/Class/Hunter/HunterAiObjectContext.cpp` | Register the new/renamed triggers |
| `src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp` | Rapid Fire node, in-combat pet recovery |
| `src/Ai/Class/Hunter/Strategy/{BeastMastery,Marksmanship}HunterStrategy.cpp` | Per-spec trigger additions, default-ladder cleanup |
| `src/Ai/Class/Hunter/Strategy/SurvivalHunterStrategy.cpp` | Remove the dead `arcane shot` default entry only |
| `docs/classes/hunter-dps-rotation-findings.md` | New findings doc — full analysis including the deferred items |

Convert the bare float relevances to `ACTION_*` constants + offsets while touching these files, matching
`FrostDKStrategy.cpp` and `ArmsWarriorStrategy.cpp`.

## Verification

The module cannot be compiled headless in this environment (see prior sessions), so:

1. Static check: every `NextAction`/`TriggerNode` name string added must have a matching `creators[...]`
   entry — an unregistered trigger name silently no-ops (`Engine::ProcessTriggers` skips it with
   `continue`, no warning), and an unregistered action logs `A:<name> - UNKNOWN`.
2. Hand off for a real build, then in-game: `.playerbot ai debug` on a hunter bot and read the
   `T:`/`PUSH:`/`A:... OK|FAILED|IMPOSSIBLE|USELESS` lines during a boss pull.
3. Confirm on a damage meter: Arcane Shot present for BM/MM and absent for Survival; Rapid Fire spent
   once per burst window; Survival's Explosive Shot / Black Arrow / Trap Launcher usage unchanged from
   before the patch.
