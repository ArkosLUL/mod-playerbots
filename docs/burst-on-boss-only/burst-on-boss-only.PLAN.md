# Plan: Suppress DPS burst cooldowns on trash (boss-only, like Bloodlust/Heroism)

## Context

Bots currently fire major DPS burst cooldowns (Recklessness, Arcane Power, Adrenaline
Rush, Avenging Wrath, Metamorphosis, etc.) on any target, including trash packs. The
user wants parity with the Shaman Bloodlust/Heroism behaviour: burst cooldowns should
be saved for bosses, not wasted on trash.

The infrastructure for this already exists and is loaded by default — it just doesn't
suppress on trash yet. The `burst` strategy
(`src/Ai/Base/Strategy/BurstWindowStrategy.cpp`) installs a per-action multiplier,
`HoldBurstUntilTankEngagedMultiplier`, that is registered for every bot via
`AiFactory.cpp:289`. Today it *holds* burst on bosses until the main tank has aggro,
but on a non-boss target it returns `1.0` — i.e. burst is allowed freely on trash.
That non-boss branch is the gap.

Reused, already-existing pieces:
- Burst-action registry + `IsBurstCooldownAction(name)` —
  `src/Ai/Base/Combat/BurstCooldowns.cpp:22-52` (single source of truth for which
  action names are burst cooldowns, across all classes).
- Boss predicate idiom `creature->IsDungeonBoss() || creature->isWorldBoss()` — the
  de-facto "is boss" check used everywhere, incl. the same multiplier
  (`BurstWindowStrategy.cpp:22`) and the Shaman lust gate
  (`ShamanTriggers.cpp:488`).

## Decisions (confirmed with user)

- **Gated behind a new config, default on.** New `AiPlayerbot.BurstOnBossOnly` setting
  (default `1`/true). When enabled, grouped bots hold burst cooldowns for bosses; when
  disabled, behaviour reverts exactly to today (burst allowed on trash). Follows the
  `offensivePotions` config pattern.
- **Scope: group/instance only.** Suppress burst on non-boss targets only when the bot
  is in a group (mirrors the existing group-gated burst-hold). Solo/questing bots keep
  popping cooldowns on tough elites and rares — unchanged.
- **Exempt sustain cooldowns.** `shadowfiend` (priest) doubles as a mana-return cast, so
  it stays available on trash to avoid mana starvation. Only pure throughput cooldowns
  are suppressed.

## The change

Single file: `src/Ai/Base/Strategy/BurstWindowStrategy.cpp`, function
`HoldBurstUntilTankEngagedMultiplier::GetValue`. Restructure so it handles three cases:
non-burst actions (ignore), non-boss target (new: suppress in group), boss target
(existing hold logic, unchanged).

Target shape:

```cpp
float HoldBurstUntilTankEngagedMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Only this multiplier's job is burst throughput cooldowns; leave everything else
    // (rotation spells, tank mitigation, healer mana) untouched.
    std::string const name = action->getName();
    if (!IsBurstCooldownAction(name))
        return 1.0f;

    // "use trinket" is the generic trinket action; for non-dps it also covers survival
    // and mana trinkets, which must stay available.
    if (name == "use trinket" && !PlayerbotAI::IsDps(bot))
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    Creature* creature = target ? target->ToCreature() : nullptr;
    bool const isBoss = creature && (creature->IsDungeonBoss() || creature->isWorldBoss());

    if (!isBoss)
    {
        // No boss engaged: the tank-dwell timer is meaningless, so clear it.
        holdState.Reset();

        // Config off, solo bots, or a sustain cast (shadowfiend doubles as a mana return):
        // keep bursting on whatever is being fought.
        if (!sPlayerbotAIConfig.burstOnBossOnly || !bot->GetGroup() || name == "shadowfiend")
            return 1.0f;

        // Grouped bot on trash: save the cooldown for the boss.
        return 0.0f;
    }

    // Boss target: existing behaviour — hold until the main tank has firmly engaged.
    if (!bot->GetGroup() || botAI->IsMainTank(bot))
    {
        holdState.Reset();
        return 1.0f;
    }

    uint32 dwellMs = (name == "bloodlust" || name == "heroism") ? LUST_DWELL_MS : BURST_DWELL_MS;
    return MainTankHasHeldBoss(bot, target, holdState, dwellMs) ? 1.0f : 0.0f;
}
```

Notes:
- The original ordering did the boss check *before* the `getName()` string copy, to skip
  that copy on the common non-boss case. That optimisation is no longer possible: to
  suppress burst on trash we must identify burst actions on trash, which needs the name.
  The `!IsBurstCooldownAction` early-out still bails before any target/boss work for the
  vast majority of actions, so the per-tick cost stays bounded to one short-string copy +
  one hash-set lookup per action. Acceptable; call out in review.
- `shadowfiend` is the only entry in the burst registry that is a sustain/mana cast; every
  other listed name is pure throughput, so a single name exemption covers the decision.
- No new files, no wiring changes — `burst` is already in the default strategy set
  (`AiFactory.cpp:289`). One new config bool, added following the `offensivePotions`
  pattern.

## Files

- `src/Ai/Base/Strategy/BurstWindowStrategy.cpp` — the behaviour change (function body of
  `HoldBurstUntilTankEngagedMultiplier::GetValue`), reading `burstOnBossOnly`.
- `src/PlayerbotAIConfig.h` — declare `bool burstOnBossOnly;` beside `offensivePotions`
  (~line 375).
- `src/PlayerbotAIConfig.cpp` — load it beside `offensivePotions` (~line 625):
  `burstOnBossOnly = sConfigMgr->GetOption<bool>("AiPlayerbot.BurstOnBossOnly", true);`
- `conf/playerbots.conf.dist` — document the new key after the `OffensivePotions` block
  (~line 1042), default `1`:
  ```
  # DPS bots hold their major offensive burst cooldowns (Recklessness, Arcane Power,
  # Adrenaline Rush, Avenging Wrath, Bloodlust/Heroism, etc.) for dungeon and world bosses
  # instead of spending them on trash, while grouped. Solo bots are unaffected; priest
  # Shadowfiend stays available for mana. Disable to let bots burst on any target.
  # Default: 1 (enabled)
  AiPlayerbot.BurstOnBossOnly = 1
  ```

## Verification

Static:
- Confirm `burst` remains in `AiFactory.cpp:289` default list (unchanged).
- Confirm `IsBurstCooldownAction` still the sole gate for which actions are affected, so
  future new cooldowns only need adding to `BurstCooldowns.cpp`.
- Confirm `AiPlayerbot.BurstOnBossOnly = 1` in the dist conf and default `true` in
  `PlayerbotAIConfig.cpp`, so out-of-the-box behaviour is boss-only.

Config toggle:
- Set `AiPlayerbot.BurstOnBossOnly = 0`, reload: bots burst on trash again (old
  behaviour). Set back to `1`: trash suppression returns.

In-game (build + run server, spawn a group of bots with a bot main tank):
1. Pull a **trash pack** in a dungeon/raid. Expect: DPS bots do NOT use burst cooldowns
   (Recklessness, Arcane Power, Adrenaline Rush, Avenging Wrath, Metamorphosis, lust,
   etc.). Priest shadowfiend still fires when low on mana.
2. Pull a **dungeon/world boss**. Expect: burst cooldowns fire as before, held briefly
   until the main tank has aggro (unchanged boss behaviour).
3. Take a **solo bot** (no group) and fight normal mobs. Expect: burst cooldowns still
   used — solo behaviour unchanged.
4. Non-dps trinket use on trash (e.g. tank survival trinket) still fires.
