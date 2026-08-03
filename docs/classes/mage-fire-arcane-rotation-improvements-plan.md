# Fire / Arcane Mage rotation audit — findings document

## Context

The mage AI in `modules/mod-playerbots` has never had the rotation audit that warrior, priest, hunter,
rogue, paladin, DK and warlock all received (see `docs/classes/`). Exploration turned up real gaps
against the wowtbc.gg Wrath rotation guides in a PvE raid setting — most notably a dead trigger node,
an unreachable `presence of mind`, no mana-aware Arcane Blast stack management, and Living Bomb losing
its "will the target live long enough" guard.

**Deliverable: one findings document. No code changes in this pass.** The doc must be complete enough
that a later implementation session can execute from it alone. Fire findings apply to Frostfire too,
which currently carries a byte-identical copy of Fire's `InitTriggers`.

Recommended fixes must stay within mage-local triggers/actions built on existing base classes
(`BoostTrigger`, `DebuffTrigger`, `HasAuraStackTrigger`, `TargetLowHealthTrigger`, `TwoTriggers`, …).
No reaching into AzerothCore headers.

## Deliverable

`modules/mod-playerbots/docs/classes/mage-fire-arcane-rotation-findings.md`

Follow the house shape set by `docs/classes/hunter-dps-rotation-findings.md` and
`docs/classes/warlock-rotation-audit-findings.md`:

1. `# Mage Fire & Arcane PvE DPS — findings` + one-paragraph preamble on why the doc exists.
2. `## How the engine picks an ability` — `Engine::DoNextAction` ([Engine.cpp:144](../../src/Bot/Engine/Engine.cpp#L144)),
   the `ACTION_*` table from [Strategy.h:53-65](../../src/Bot/Engine/Strategy/Strategy.h#L53), then the
   framework facts below.
3. `## Findings` — `### Fire`, `### Arcane`, `### Shared / cross-spec`, each an ID'd table
   `# | Code | Guide | Impact`.
4. `## Divergences to decide` — table `# | Spec | Code position | Guide position | Trade-off`.
5. `## Confirmed correct (do not re-audit)`.
6. `## Recommended fixes` — grouped by finding ID, naming the trigger/action class, the registered
   string name, and the proposed relevance as a bare float with its `ACTION_* + offset` equivalent.
7. `## Verification` — how a later implementation pass checks itself.

All source references as markdown links with anchors: `[File.cpp:NN](../../src/...#LNN)`. Mage strategy
files use bare floats, not `ACTION_*` constants — quote the float as it exists and give the constant
equivalent alongside.

## Reference rotations (wowtbc.gg, Wrath)

Quote these in the doc as the "Guide" column source.

**Fire** — https://wowtbc.gg/wotlk/class-guides/fire-mage/
Molten Armor → Combustion + Icy Veins (align with Bloodlust; keep available for Molten Fury <35%) →
maintain Improved Scorch if assigned → maintain Living Bomb, let it expire before re-applying →
Pyroblast on Hot Streak → Fireball/Frostfire Bolt filler → Mirror Image while moving → Fire Blast if
the target dies before the next filler lands or while moving → Evocation when out of mana.
AoE: Firestarter-proc Flamestrike → spread Living Bomb (only on targets that survive to the explosion)
→ Blast Wave → Flamestrike R9 → Dragon's Breath → Flamestrike R8 → Blizzard → Arcane Explosion /
Cone of Cold while moving.

**Arcane** — https://wowtbc.gg/wotlk/class-guides/arcane-mage/
Molten Armor → Presence of Mind + Arcane Blast → Icy Veins + Arcane Power (+ on-use trinkets) →
4× Arcane Blast before consuming a Missile Barrage proc with Arcane Missiles (3× when conserving mana) →
Mirror Image while moving → Fire Blast on execute → Evocation when out of mana.
AoE: Blizzard under Presence of Mind (Arcane Potency) → Flamestrike R9 → Flamestrike R8 → Blizzard →
Arcane Explosion / Cone of Cold while moving.

## Framework facts the findings rest on

Section 2 of the doc. All verified during exploration.

- Relevance is used verbatim from `NextAction`; triggers push with `forceRelevance = 0.0f`
  ([Engine.cpp:277](../../src/Bot/Engine/Engine.cpp#L277), [:498](../../src/Bot/Engine/Engine.cpp#L498)).
- `BuffTrigger::IsActive()` = "the named aura is absent (or under `beforeDuration`)"
  ([GenericTriggers.cpp:193](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L193)).
- `DebuffTrigger::IsActive()` = `BuffTrigger::IsActive()` **and**
  `target->GetHealth() / "estimated group dps" >= needLifeTime`
  ([GenericTriggers.cpp:311](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L311)). Overriding
  `IsActive()` to call `BuffTrigger::IsActive()` directly throws that guard away.
- `BoostTrigger::IsActive()` = `BuffTrigger::IsActive()` and (target is a player **or**
  `"balance" <= 50`) ([GenericTriggers.cpp:422](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L422)).
- `HasAuraStackTrigger` is an **at least N stacks** test — `GetAura` compares with `<` and `continue`s
  ([GenericTriggers.cpp:559](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L559),
  [PlayerbotAI.cpp:3242](../../src/Bot/PlayerbotAI.cpp#L3242)).
- No aura `AI_VALUE`. Stacks and remaining duration are read via
  `botAI->GetAura(name, unit, checkIsOwner, checkDuration, checkStack)` then `GetStackAmount()` /
  `GetDuration()`. `checkDuration` means "has a finite duration", not "duration remaining". Prior art:
  [DruidTriggers.h:333](../../src/Ai/Class/Druid/DruidTriggers.h#L333),
  [PriestTriggers.cpp:93](../../src/Ai/Class/Priest/PriestTriggers.cpp#L93).
- No spell queue and no cast-while-moving model: any spell with a cast time is refused outright while
  moving ([PlayerbotAI.cpp:3366](../../src/Bot/PlayerbotAI.cpp#L3366),
  [:3745](../../src/Bot/PlayerbotAI.cpp#L3745)). Movement handling is expressed purely as lower-priority
  instants in the default action list. Casts are never clipped — the AI yields while
  `SPELL_STATE_PREPARING` ([PlayerbotAI.cpp:271](../../src/Bot/PlayerbotAI.cpp#L271)); deliberate
  clipping needs an explicit `cancel channel` node.
- `CastTimeStrategy` multiplies relevance by `0.1f` when the cast would outlast the target
  ([CastTimeStrategy.cpp:12](../../src/Ai/Base/Strategy/CastTimeStrategy.cpp#L12)). This is what makes
  the execute swap to Fire Blast work without an explicit node.
- `BurstWindowStrategy` zeroes `arcane power`, `icy veins`, `combustion`, `mirror image`,
  `presence of mind` until the main tank has held the boss
  ([BurstCooldowns.cpp:22](../../src/Ai/Base/Combat/BurstCooldowns.cpp#L22),
  [BurstWindowStrategy.cpp:12](../../src/Ai/Base/Strategy/BurstWindowStrategy.cpp#L12)).
- Mana thresholds: `LowMana = 15`, `MediumMana = 40`, `HighMana = 65`
  ([PlayerbotAIConfig.cpp:115](../../src/PlayerbotAIConfig.cpp#L115)). **`"high mana"` is a
  `mana < 65%` test despite the name** ([GenericTriggers.cpp:75](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L75)).
- Current relevance ladders, to quote in the doc:
  - Fire ([FireMageStrategy.cpp:17-58](../../src/Ai/Class/Mage/Strategy/FireMageStrategy.cpp#L17)):
    pyroblast on hot streak 25.0, scorch on improved scorch 19.0, living bomb 18.5; defaults
    fireball 5.3 / frostbolt 5.2 / fire blast 5.1 / shoot 5.0.
  - Arcane ([ArcaneMageStrategy.cpp:38-63](../../src/Ai/Class/Mage/Strategy/ArcaneMageStrategy.cpp#L38)):
    arcane missiles 15.0 on `arcane blast 4 stacks and missile barrage`; defaults arcane blast 5.6 /
    arcane missiles 5.5 / arcane barrage 5.4 / fire blast 5.3 / frostbolt 5.2 / shoot 5.1.
  - Shared ([GenericMageStrategy.cpp:91-127](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L91)):
    ice block / wards / mana gem / evocation 90.0, mana shield 85.0, mirror image on `high threat` 60.0,
    frost nova 50.0, spellsteal & counterspell-on-healer 40.0, blink back 35.0, invisibility 30.0.
  - Boost ([GenericMageStrategy.cpp:135-166](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L135)):
    arcane — arcane power 29.0 / icy veins 28.5 / mirror image 28.0; fire — combustion 18.0 /
    mirror image 17.5; frostfire — combustion 18.0 / icy veins 17.5 / mirror image 17.0.
  - AoE ([GenericMageStrategy.cpp:181-216](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L181)).

## Findings to write up

Each gets a table row with `Code` (what the tree does, with file:line), `Guide` (what wowtbc says) and
`Impact` (what actually goes wrong in a raid), plus a `## Recommended fixes` entry.

### Fire — `src/Ai/Class/Mage/Strategy/FireMageStrategy.cpp`, `GenericMageStrategy.cpp`

| ID | Substance |
|---|---|
| F1 | **Dead node.** `"high threat"` → `mirror image` 60.0 at [GenericMageStrategy.cpp:96](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L96). `"high threat"` is registered nowhere — [TriggerContext.h:127](../../src/Ai/Base/TriggerContext.h#L127) only has `"medium threat"`, and `MageTriggerFactoryInternal` doesn't add it. Affects all three specs. Fix: use `"medium threat"` or add a real high-threat trigger; note Mirror Image is still reachable via `boost`. |
| F2 | **Hot Streak outranks maintenance.** Pyroblast 25.0 sits above improved scorch 19.0 and living bomb 18.5; the guide ranks Scorch and Living Bomb above the Hot Streak dump. Hot Streak lasts 10 s, so it survives 2–3 GCDs of maintenance; a dropped Living Bomb tick or Improved Scorch does not come back. Fix: pyroblast to 18.25 (`ACTION_HIGH - 1.75`), keeping the guide's order scorch 19.0 > living bomb 18.5 > pyroblast 18.25 > combustion 18.0. |
| F3 | **Combustion has no window logic.** `CombustionTrigger : BoostTrigger` fires the moment the aura is absent and `balance <= 50` ([MageTriggers.h:118](../../src/Ai/Class/Mage/MageTriggers.h#L118), pushed at 18.0). The only gate is `BurstWindowStrategy`'s tank-engaged hold. Guide: align with Bloodlust, and keep it up for the sub-35% Molten Fury window. Impact: Combustion burned at pull, unavailable during execute. Fix: a mage-local `CombustionWindowTrigger : BoostTrigger` also requiring bloodlust/heroism on self **or** target below 35% **or** a staleness fallback so it is never held forever. Prior art for the health band: `DrainSoulExecuteTrigger : TargetLowHealthTrigger` from the warlock pass. |
| F4 | **Living Bomb lost its lifetime guard.** `LivingBombTrigger` / `LivingBombOnAttackersTrigger` override `IsActive()` to call `BuffTrigger::IsActive()` ([MageTriggers.h:208-220](../../src/Ai/Class/Mage/MageTriggers.h#L208)). That correctly implements "let it expire before you re-apply it", but drops `DebuffTrigger`'s `needLifeTime` check, so Living Bomb goes on mobs that die before the explosion — the guide's explicit caveat. `CastTimeStrategy` does not cover this: Living Bomb is instant. Fix: keep the no-clip behaviour, restore the time-to-live test (`BuffTrigger::IsActive() && health / "estimated group dps" >= needLifeTime`). |
| F5 | **AoE ranks Living Bomb spread last.** `living bomb on attackers` 21.0 sits below flamestrike 23.0 and blizzard 22.0 ([GenericMageStrategy.cpp:206](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L206)); the guide puts the spread second, above Blast Wave / Flamestrike / Blizzard. Fix: raise above the flamestrike/blizzard pair. |
| F6 | **Melee AoE nodes in the ranged block.** `dragon's breath` 39.0 and `blast wave` 38.0 head the fire AoE list but their `isUseful()` needs ≤10 yd ([MageActions.cpp:90-109](../../src/Ai/Class/Mage/MageActions.cpp#L90)), so at raid range they always fail through. Harmless but misleading — and `FirestarterStrategy`, the strategy that exists to close that gap, is opt-in and off by default ([FireMageStrategy.cpp:61-77](../../src/Ai/Class/Mage/Strategy/FireMageStrategy.cpp#L61)). Document the interaction. |
| F7 | **Frostfire is a copy-paste fork.** `FrostFireMageStrategy::InitTriggers` is byte-identical to Fire's and differs only in `getDefaultActions()`. Every Fire finding must be applied twice or the two drift. Fix: derive Frostfire from `FireMageStrategy`, or factor the shared trigger list. |
| F8 | **No in-combat Molten Armor.** Molten Armor is only pushed out of combat by `bdps` at 19.0 ([GenericMageNonCombatStrategy.cpp:44](../../src/Ai/Class/Mage/Strategy/GenericMageNonCombatStrategy.cpp#L44)); the guide has it as priority #1. A mage that loses it mid-fight never re-buffs. Low impact — document, propose a low-relevance combat node. |

### Arcane — `src/Ai/Class/Mage/Strategy/ArcaneMageStrategy.cpp`

| ID | Substance |
|---|---|
| A1 | **Presence of Mind is unreachable.** `PresenceOfMindTrigger : BoostTrigger` ([MageTriggers.h:161](../../src/Ai/Class/Mage/MageTriggers.h#L161)) and `CastPresenceOfMindAction` are both registered ([MageAiObjectContext.cpp](../../src/Ai/Class/Mage/MageAiObjectContext.cpp)), and `"presence of mind"` is already in `burstCooldownNames`, but **no strategy references it**. It is the guide's #2 arcane priority. Fix: node in the arcane branch of `MageBoostStrategy` at 29.5 (`ACTION_MOVE - 0.5`), above arcane power 29.0; the default `arcane blast` 5.6 consumes it on the next GCD. |
| A2 | **No mana-aware stack dump.** The only arcane rotation node is `arcane blast 4 stacks and missile barrage` → arcane missiles 15.0. Missile Barrage is a 40% proc, so the bot holds 4 stacks — at +175% mana cost per Arcane Blast — for an unbounded number of casts, and `"low mana"` only fires Evocation below 15%. Guide has an explicit conserve variant: dump after 3 Arcane Blasts. Impact: arcane bots run dry in long raid fights. Fix: a `TwoTriggers("arcane blast stack", "medium mana")`-style node (or a 3-stack variant trigger) → `arcane missiles` at 15.5. |
| A3 | **Evocation fires too late for Arcane.** `"low mana"` = 15% ([GenericTriggers.cpp:28](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L28)). At 15% an arcane mage cannot pay for a stacked Arcane Blast and burns GCDs. Fix: a mage-local threshold around 25% for the arcane evocation node, leaving the shared 90.0 relevance alone. Also list in *Divergences to decide* — this is a tuning call, not a defect. |
| A4 | **AoE has no Presence of Mind + Blizzard.** The arcane AoE branch is flamestrike 23.0 / blizzard 22.0, with blizzard promoted to 24.0 once a Flamestrike is down ([GenericMageStrategy.cpp:188-193](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L188)). The guide leads with an instant Blizzard under Presence of Mind for the Arcane Potency crit. Fix: an AoE node pairing the two, gated on `medium aoe`. |
| A5 | **`CastArcaneBlastAction` is the wrong base class.** It is a `CastBuffSpellAction` with `GetTargetName()` overridden to `"current target"` ([MageActions.h:306](../../src/Ai/Class/Mage/MageActions.h#L306)), so its inherited `isUseful()` hunts for an "arcane blast" aura on the *enemy*. That aura never exists, so it always returns true and the action works by accident. Fix: plain `CastSpellAction`. Cleanup, no behaviour change. |

### Shared / cross-spec

| ID | Substance |
|---|---|
| S1 | **Mana gem has no fallback chain.** The gem node is chosen by the highest *Conjure* rank the bot knows ([GenericMageStrategy.cpp:107-120](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L107)), but each `UseMana*Action::isUseful()` requires that exact item in bags ([MageActions.cpp:31](../../src/Ai/Class/Mage/MageActions.cpp#L31)). A level-80 mage carrying a Ruby but no Sapphire uses nothing. Fix: an `ActionNode` alternatives chain down the ranks, same shape as `CastMoltenArmorAction::getAlternatives()`. |
| S2 | **`"high mana"` is a misnomer.** It tests `mana < 65%`. Worth one line in the framework section so nobody "fixes" the gem node backwards. |
| S3 | **Registered but unreferenced.** Triggers `fireball`, `pyroblast`, `frostfire bolt`, `arcane blast`, `presence of mind`, `ice barrier`, `counterspell`; actions `presence of mind`, `counterspell`, `conjure food`, `conjure water`. Recommend: wire `presence of mind` (A1) and `counterspell` (a raid-PvE interrupt node — only `counterspell on enemy healer` is used today, at 40.0); delete the rest, following the dead-wiring deletions in the warlock pass. Deletion is the two-site `creators[...]` + factory-function pattern in `MageAiObjectContext.cpp`. |
| S4 | **Focus Magic is out-of-combat only** ([GenericMageNonCombatStrategy.cpp:34-57](../../src/Ai/Class/Mage/Strategy/GenericMageNonCombatStrategy.cpp#L34)). If the focus target dies mid-fight the buff is gone for the rest of the encounter. Low impact — document. |

## Divergences to decide (state, do not resolve)

Put each of these in the `## Divergences to decide` table with the trade-off spelled out.

| Topic | Code position | Guide position |
|---|---|---|
| Fire CC block | `MageCcStrategy` pushes `dragon's breath` at `ACTION_INTERRUPT + 1` (41.0) and `blast wave` at 40.0 on "enemy close" for fire only ([GenericMageStrategy.cpp:174-178](../../src/Ai/Class/Mage/Strategy/GenericMageStrategy.cpp#L174)) | Guide treats both as AoE-rotation cooldowns. In raid PvE a self-defence disorient can break CC, move mobs out of a tank's pile, and spends a cooldown the AoE rotation wants |
| Arcane evocation threshold (A3) | Shared 15% | Guide says only "when you run out of mana"; ~25% is the practical arcane number |
| Firestarter | Opt-in, off by default; suppresses frost nova and blink-back kiting when on | Guide's AoE ladder assumes Blast Wave and Dragon's Breath are usable, i.e. assumes melee range |
| Frostbolt in the Fire/Arcane default lists | Present at 5.2 for immune targets | Not in the guide; keeping it is a bot-specific robustness call |

## Confirmed correct (do not re-audit)

- Execute swap to Fire Blast works without an explicit node: `CastTimeStrategy` demotes the cast-time
  filler by ×0.1 (fireball 5.3 → 0.53) so instant `fire blast` 5.1 / 5.3 wins. Covers guide item "Fire
  Blast if the target will die before your next cast finishes" for both specs.
- Movement handling likewise: cast-time spells are refused while moving, so the ladder falls through to
  the instants already sitting below them.
- `ImprovedScorchTrigger` correctly implements "maintain if assigned" — it stands down when the target
  already carries Shadow Mastery (17794-17800), Winter's Chill (12579) or another mage's Improved
  Scorch (22959) ([MageTriggers.cpp:126](../../src/Ai/Class/Mage/MageTriggers.cpp#L126)). Use it as the
  model for other debuff triggers.
- Firestarter proc → free instant Flamestrike at 40.0, above the whole fire AoE block.
- `blizzard channel check` → `cancel channel` 26.0 correctly stops a Blizzard when attackers drop below
  two ([MageTriggers.cpp:160](../../src/Ai/Class/Mage/MageTriggers.cpp#L160)).
- `HasAuraStackTrigger("arcane blast", 4)` is an at-least-4 test on self, matching the guide's
  "at least 4 Arcane Blasts".
- Molten Armor for fire and arcane, Mage Armor for frost, via `bdps` / `bmana`
  ([AiFactory.cpp:312-325](../../src/Bot/Factory/AiFactory.cpp#L312)) — matches the guide.
- Potion and trinket pairing needs no mage node: generic `"potions"` + `OffensivePotionTrigger` +
  the `burst` window already hold them to the tank-engaged window.

## Verification

Doc-only pass, so verification is fact-checking rather than building.

1. Re-open every file:line cited in the doc and confirm the quoted code matches. No links to lines that
   moved.
2. Confirm the "registered but unreferenced" list (S3) by grepping each name across
   `src/Ai/Class/Mage/Strategy/` — a name must appear in `MageAiObjectContext.cpp` and nowhere else.
3. Confirm `"high threat"` (F1) is absent from both `TriggerContext.h` and `MageTriggerFactoryInternal`.
4. Spot-check the game-mechanic claims (Hot Streak duration, Living Bomb duration and explosion,
   Arcane Blast stack cap and mana scaling, Molten Fury 35%, Improved Scorch 22959) against
   `acore_world` rather than from memory — `mysqlsh` creds are already on file for this project.
5. No build, no linter run — nothing under `src/` is touched.
