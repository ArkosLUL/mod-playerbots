# How a bot picks an ability

Everything a bot does each tick comes out of one relevance-ordered queue. Read this before touching
any strategy, trigger, action or multiplier.

## Primitives

Under `src/Bot/Engine/`:

| Primitive | File | Role |
|---|---|---|
| `Strategy` | `Strategy/Strategy.h` | Overrides `getName()`, `InitTriggers()`, `InitMultipliers()`, `getDefaultActions()`. |
| `Trigger` | `Trigger/Trigger.h` | Condition, `IsActive()`. Wrapped by `TriggerNode`: trigger name → list of `NextAction(name, relevance)`. |
| `Action` | `Action/Action.h` | `Execute(Event)`, `isUseful()`, `isPossible()`. Raid actions subclass `AttackAction` or `MovementAction`. |
| `Multiplier` | `Multiplier.h` | `GetValue(Action*)` scales relevance; `0.0f` suppresses. |
| `Value<T>` | `Value/Value.h` | Cached state calculators, read via `AI_VALUE` / `AI_VALUE2`. |

Hierarchy, verified — **`AttackAction : MovementAction`**, not siblings
(`Ai/Base/Actions/AttackAction.h:16`):

- `MovementAction : Action` → `AttackAction` (→ `DpsAssistAction`, `TankAssistAction`),
  `FollowAction`, `FleeAction`, `RunAwayAction`, `ReachTargetAction`, `EnterVehicleAction`,
  `LeaveVehicleAction`, `CombatFormationMoveAction` (→ `SetBehindTargetAction`), `AvoidAoeAction`
- `CastSpellAction : Action` → `CastReachTargetSpellAction`, `CastDisengageAction`,
  `CastBlinkBackAction`
- `SetFacingTargetAction` and `DropTargetAction` are plain `Action`s, so facing survives a movement
  lockout

Only `AttackAction` exposes `Attack`, only `MovementAction` exposes `MoveTo`/`FleePosition`; picking
the wrong base silently limits the action.

**A blanket `MovementAction` veto therefore also kills targeting and vehicle boarding** — EoE's disk
riders boarded and then sat still for a phase. Name the boss's own actions as exemptions. The two
families are **disjoint**, which is what lets a multiplier suppressing only those two split on one
`dynamic_cast` each way instead of a chain — see
[raid-mechanics-lessons.md](raid-mechanics-lessons.md).

## The selection loop

`Engine::DoNextAction` (`src/Bot/Engine/Engine.cpp:144`, detail at `:166-245`):

1. Push every fired trigger's `NextAction`s.
2. `PushDefaultActions` (`Engine.cpp:508-516`) pushes every active strategy's `getDefaultActions()`
   **ungated by any trigger**, with `forceRelevance = 0.0f` so the listed value is kept verbatim.
3. Pop in descending relevance; **break on the first action returning `true`**.
4. Ties break by insertion order, which reads as nondeterministic in-game.

Two rules follow from 3; breaking either silently disables every node below:

- **An action that only writes state returns `false`.** Bookkeeping that returns `true` at
  `ACTION_RAID` costs the bot every cast, heal and formation move it owns
  (`MimironResetEncounterStateAction`, `MimironPhase1PositioningAction`).
- **A latch trigger tests the constant its action writes, never a literal.** An `IsActive` shaped
  `AI_VALUE(...) != X` re-arms until the write matches, so drift between the two leaves it
  permanently active. Mimiron's phase 1 disperse latch shipped the action on `5.5` against the
  trigger on `6.0` and ran a whole Firefighter phase 1 with no ranged damage and no healing.

A zeroed multiplier breaks the multiplier loop, fails `isPossible() && relevance > 0`, and lands on
the **IMPOSSIBLE** branch — which still pushes the node's `/*A*/` alternatives. Fallback chains
survive a veto. Never build an A→B→A cycle in `getAlternatives`.

**`Queue::Push` de-dupes baskets by action *name*** (`src/Script/WorldThr/Queue.cpp:11-27` walks
`actions` and calls `updateExistingBasket` on a name match). So one action name listed at four
different relevances is **one basket**, not four independent entries — which means retargeting or
fixing that action once repairs every band it appears in.

Relevance constants (`src/Bot/Engine/Strategy/Strategy.h:53-65`):

`ACTION_DEFAULT 5` · `ACTION_NORMAL` / `ACTION_LIGHT_HEAL 10` · `ACTION_HIGH` / `ACTION_MEDIUM_HEAL 20`
· `ACTION_MOVE` / `ACTION_CRITICAL_HEAL 30` · `ACTION_INTERRUPT 40` · `ACTION_DISPEL 50` ·
`ACTION_RAID 60` · `ACTION_EMERGENCY 90`.

Half-step values (`19.5`, `39.5`) are collision breakers, not design statements.

## Registration

Objects are created by name through `NamedObjectContext<T>` (`src/Bot/Engine/NamedObjectContext.h`).
Class contexts resolve **before** shared ones, so registering the same name in both gives a
per-class override for free (Ritual of Souls uses this).

**Names are resolved at runtime and fail silently** — see [pitfalls.md](pitfalls.md).

Per-raid wiring is four edits: `src/Ai/Raid/RaidStrategyContext.h` (key → strategy),
`src/Bot/Engine/BuildSharedTriggerContexts.cpp`, `BuildSharedActionContexts.cpp`, and
`PlayerbotAI::ApplyInstanceStrategies` (`src/Bot/PlayerbotAI.cpp:1623`) for the `allInstanceStrategies`
list plus the `case <mapId>:` arm. `ApplyInstanceStrategies` strips every instance strategy, then
adds the one matching the current map; it runs on login and every map change
(`PlayerbotAI.cpp:156, 797, 1609, 1873`).

Naming: strategy keys are bare lowercase (`"blacktemple"`); triggers and actions are lowercase,
space-separated, boss-prefixed; multiplier classes are `{BossName}{Purpose}Multiplier`.

The module has **no `CMakeLists.txt`** — AzerothCore globs `modules/*/src` recursively and adds every
subdirectory to the include path, so new files need only a cmake re-configure. Splitting a large
file is free if you keep an umbrella header (`UldActions.h`) that includes the parts: contexts and
registration maps then need no edits.

## Trigger semantics

- `BuffTrigger` fires when the named aura is **missing** (`GenericTriggers.cpp:193-204`). With no such
  self-aura (`"bloodthirst"`, `"whirlwind"`) it is permanently active — read it as "always try this".
- `DebuffTrigger` fires when the debuff is missing from the current target (`GenericTriggers.cpp:311`).
  It cannot drive a stack ramp or pace a cooldown, because it goes quiet the moment the aura lands.
- `beforeDuration` (ms, default 0) is the refresh window on both. **0 is a real choice, not an
  oversight**: priest DoTs must expire before re-applying, rogue sets 2000 on Slice and Dice / Hunger
  for Blood / Rupture on purpose. Do not "make them consistent".
- `DebuffTrigger`'s `needLifeTime` is a **time-to-die** test —
  `target->GetHealth() / AI_VALUE(float, "estimated group dps")` (`GenericTriggers.cpp:311-319`) — not
  a DoT-remaining test. Overriding `IsActive()` to call `BuffTrigger::IsActive()` throws that guard
  away.
- `BoostTrigger::IsActive` (`GenericTriggers.cpp:422-432`) needs the buff missing **and** either a
  Player target or `balance <= 50`. In raid PvE the target is a Creature and the raid is normally
  winning, so **every `BoostTrigger` subclass is effectively dead**. Re-key to
  `SpellNoCooldownTrigger` and let `BurstWindowStrategy` place the cast.
- `TwoTriggers`' creator key must equal `getName()` = `"<name1> and <name2>"`.

## Multiplier semantics

Multipliers multiply, so independent gates AND together for free — the shared burst gate and a
per-boss lust gate compose with no coordination. Returning `0.0f` is the standard veto idiom, and it
is **final**: no later multiplier can hand the action back, so a rule phrased as "allow X here" only
works if nothing else already vetoed X.

`dynamic_cast` is the identification mechanism, and **must be narrow**: cast to the concrete action
(`CastMisdirectionOnMainTankAction`), never a shared base like `BuffOnMainTankAction` — that base
also carries paladin Beacon, shaman Earth Shield and druid Thorns, a bug the Naxx work had to undo.

At scale, match on `getName()` instead: `CastSpellAction` passes the spell name to
`Action(botAI, spell)` (`GenericSpellActions.cpp:138`), so the name is a reliable identifier and does
not require including 25 class headers. Racials and trinkets follow the same convention
(`"berserking"`, `"blood fury"`, `"use trinket"`).

Unlike creator-name strings, a `dynamic_cast` mistake fails at **compile** time.

## Casting

- `PlayerbotAI::CanCastSpell` builds its probe with `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST` and
  whitelists `SPELL_FAILED_OUT_OF_RANGE`, so `isPossible()` never checks mana, rage or runes.
  **Resource and pacing gating must live in a trigger, not in the action.**
- There is no cast-while-moving model and no spell queue: any cast-time spell is refused outright
  while moving (see [pitfalls.md](pitfalls.md)), and casts are never clipped — the AI yields while
  `SPELL_STATE_PREPARING`. Deliberate clipping needs an explicit `cancel channel` node.
- `CastTimeStrategy` multiplies relevance by 0.1 when the cast would outlast the target. That is what
  makes the execute-swap to an instant work with no explicit node.
- `HasAura(..., maxStack = false, maxAuraAmount = -1)` is a **presence** check, not a stack check.
- `CastBuffSpellAction` resolves the highest known rank by parsing the `Rank N` subtext, so spell ids
  never need hardcoding.
- `YieldThread` (`PlayerbotAIBase.cpp:53-61`) *raises* `nextAICheckDelay` to the react delay whenever
  it is lower, so `SetNextCheckDelay(0)` written inside an action is overwritten before the tick
  ends. To let an off-GCD ability share a tick, return `false` from `Execute` — `DoNextAction` only
  breaks on `true`. Two consequences: the debug log reads `A:<name> - FAILED` for a cast that went
  out, and the node's alternatives get pushed.

## Healing

- The four party health bands are **nested, not exclusive** — all pass `minValue = 0`
  (`src/Ai/Base/Trigger/HealthTriggers.h:88-128`). A target at 20% fires critical, low, medium *and*
  almost-full at once; only relevance separates them.
- Config defaults (`src/PlayerbotAIConfig.cpp:95-117`): critical 25, low 45, medium 65, almostFull 85,
  lowMana 15, mediumMana 40, highMana 65, saveManaThreshold 60, healDistance 38.5. Note `"high mana"`
  tests mana **< 65%** despite the name; `"target critical health"` is 20%, not 25%.
- `HealerAutoSaveManaMultiplier` (`src/Ai/Base/Strategy/ConserveManaStrategy.cpp:93-131`): above 60%
  bot mana always returns 1. Below, it vetoes a `CastHealingSpellAction` when target HP ≥ 65 and
  (`lossAmount < estAmount` or efficiency ≤ MEDIUM), and when target HP ≥ 45 and (same, or efficiency
  ≤ LOW), where `lossAmount = 100 - targetHealthPct`. Tanks get `estAmount / 1.5`. Efficiency ranks
  (`PlayerbotAIConfig.h:34-42`): VERY_LOW 1, LOW 2, MEDIUM 4, HIGH 8, VERY_HIGH 16, SUPERIOR 32.

  **This metadata is a functional gate, not documentation.** A wrong `estAmount` or efficiency takes
  a spec's main heal offline exactly under mana pressure — it did so for paladin Holy Light,
  priest Flash Heal and shaman Healing Wave.
- **`PartyMemberToHeal::Calculate` returns a single unit** — `argmin(healthPct + distance/10)` across
  the whole raid. Every action bound to it competes for that one target, so anything that lowers
  damage taken without raising health % (a shield, an absorb) leaves the same unit winning the scan
  until the effect expires.
- A group scan measures with `botAI->GetRange("heal")` — **30 yd** — not
  `sPlayerbotAIConfig.spellDistance`, and not `healDistance` 38.5. Null-check `bot->GetGroup()` and
  LOS-test while you are in there.
- `AoeInGroupTrigger` (`HealthTriggers.cpp:47-63`) needs ≥ 3 healable members: threshold 3 (≤5),
  `min(half,4)` (≤10), `min(half,6)` (≤25), `min(half,8)` above. The threshold and the counting value
  must measure the **same population** — `CountHealableGroupMembers` counts alive members inside
  `healDistance`, `GetNearGroupMemberCount` uses `sightDistance` and counts the dead. Mixing them
  makes the threshold unreachable in a spread 25-man.
- `PartyMemberValue` derives from `UnitCalculatedValue`, not `Qualified`, so per-spell target
  filtering must be a **second registered value name** (`"party member to protect no tank"`), not a
  qualifier.
- A derived `InitTriggers` **cannot remove a base-class node**. The only mechanism is relocation:
  strip it from the base and re-add it verbatim in each sibling that wants it.

## Strategy assignment

`src/Bot/Factory/AiFactory.cpp` builds each bot's strategy set. The always-on combat list is at
`:289`: `racials`, `chat`, `default`, `cast time`, `potions`, `duel`, `boost`, `burst` — skipped
for battlegrounds. Spec sets follow at `:302-309, 411-416, 420-433`; healers add `save mana` and
`healer dps`.

Every strategy is switchable per bot with `strategy -<name>`.

**Instance strategies are location-derived and must never be restored from persisted per-bot data.**
`PlayerbotRepository::Load` runs after `ResetStrategies()` and `ClearStrategies` wipes the
map-derived strategy, so a saved string re-applied verbatim resurrects the previous instance's
strategy — bots last saved in Serpentshrine arrived in The Eye still carrying `ssc` and fired no Void
Reaver triggers.
