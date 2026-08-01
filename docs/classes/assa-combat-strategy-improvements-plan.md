# Rogue Combat + Assassination: raid-grade rotation rebuild

## Context

The Combat (`DpsRogueStrategy`) and Assassination (`AssassinationRogueStrategy`) rotations were
written as generic melee priority lists and have drifted from what a WotLK 3.3.5a raid rogue
actually does. An audit found a dead trigger, a talent gate that disables a core finisher, a whole
missing ability, and several priority inversions — all of which cost measurable single-target DPS on
boss fights.

The audit was prompted by one specific question: **does Combat check Sunder Armor stacks before
casting Expose Armor?**

**No.** [RogueTriggers.cpp:113-118](modules/mod-playerbots/src/Ai/Class/Rogue/RogueTriggers.cpp#L113-L118):

```cpp
return DebuffTrigger::IsActive() && !botAI->HasAura("sunder armor", target, false, false, -1, true) &&
       AI_VALUE2(uint8, "combo", "current target") <= 3;
```

With `maxStack = false` and `maxAuraAmount = -1`, `PlayerbotAI::HasAura`
([PlayerbotAI.cpp:3183](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L3183)) returns `true` on the
first matching aura effect. It is a pure presence check — one stack blocks Expose Armor exactly as
hard as five. Compare the warrior's own
[SunderArmorStackTrigger](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.cpp#L52-L53),
which does read `GetStackAmount()`.

Presence-only is conservative-correct mid-fight (Sunder and Expose Armor share the same 20% armor
slot, so a rogue re-applying it gains the raid nothing and costs a finisher). It is wrong at the
pull: for the second or two before the warrior lands stack 1, the rogue sees a clean target, burns a
finisher on Expose Armor, and the warrior then overwrites it — and the warrior's trigger has no
reciprocal check, so both keep fighting for the slot.

**Chosen fix: group-composition awareness.** The rogue skips Expose Armor entirely whenever anyone
in the group can supply the debuff, not just when the debuff currently happens to be up.

## Design decisions

- **Armor debuff ownership** — a rogue applies Expose Armor only when nobody else can. Detection is
  cheap and class-based (alive warrior in the group on the same map, or a hunter with a worm pet)
  plus a direct aura probe of the target. No per-tick spellbook scan; the trigger gets a
  `checkInterval` so the group walk is amortised.
- **Refresh windows, not expiry** — every maintained aura (Slice and Dice, Rupture, Hunger for
  Blood) is refreshed before it drops. `BuffTrigger`/`DebuffTrigger` already take a `beforeDuration`
  ([GenericTriggers.h:330-337, 404-406](modules/mod-playerbots/src/Ai/Base/Trigger/GenericTriggers.h#L330-L337));
  today rogue passes 0 everywhere, guaranteeing downtime each cycle.
- **Reuse over new plumbing** — `MediumEnergyAvailableTrigger` (40 energy),
  `ComboPointsNotFullTrigger`, `CastComboAction`, `RuptureTrigger`, `BoostTrigger`, the
  `burstCooldownNames` registry and `HoldBurstUntilTankEngagedMultiplier` all already exist and are
  either unused or unreachable from rogue. Wire them up rather than writing new ones.
- **Subtlety is untouched** — it keeps aliasing to the Assassination strategy at
  [AiFactory.cpp:379-380](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L379-L380) and
  inherits whatever improves there.

## Changes

### 1. Shared armor-debuff helper — new `src/Ai/Base/Combat/ArmorDebuff.h/.cpp`

Sits next to [BurstCooldowns.cpp](modules/mod-playerbots/src/Ai/Base/Combat/BurstCooldowns.cpp),
which is the existing precedent for cross-class combat helpers.

```cpp
bool TargetHasMajorArmorDebuff(PlayerbotAI* botAI, Unit* target);   // sunder armor | expose armor | acid spit
bool GroupSuppliesMajorArmorDebuff(Player* bot, Unit* target);      // alive warrior, or hunter with worm pet
```

`GroupSuppliesMajorArmorDebuff` walks `bot->GetGroup()` the same way
[SunderArmorStackTrigger](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.cpp#L38-L50)
does: skip self, dead, out-of-world, wrong map. Class check only — no spellbook scan. Worm-pet case
reads `member->GetPet()`'s creature family.

Trade-off to note in the code: a warrior who is present but not actually sundering (levelling, wrong
stance) will suppress the rogue's Expose Armor. Acceptable — the aura probe covers the case where
someone else did land it, and the failure mode costs 20% armor on the boss rather than a wasted
finisher every pull.

### 2. Expose Armor trigger — `src/Ai/Class/Rogue/RogueTriggers.cpp`

`ExposeArmorTrigger::IsActive` becomes:

```
DebuffTrigger::IsActive()
  && !TargetHasMajorArmorDebuff(botAI, target)
  && !GroupSuppliesMajorArmorDebuff(bot, target)
  && combo <= 3
```

Constructor gets `checkInterval = 5`. `CastExposeArmorAction`'s 25 s lifetime gate
([RogueFinishingActions.h:29](modules/mod-playerbots/src/Ai/Class/Rogue/Action/RogueFinishingActions.h#L29))
stays as-is — it already keeps the debuff off trash.

Optional reciprocal (small, directly serves this fix): in
[WarriorTriggers.cpp:52](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.cpp#L52), return
false when Expose Armor is already on the target, so a warrior at 0 stacks doesn't clobber a full
20% debuff a rogue just applied.

### 3. Triggers — `src/Ai/Class/Rogue/RogueTriggers.h/.cpp`, registered in `RogueAiObjectContext.cpp:60-111`

| Trigger | Change |
|---|---|
| `slice and dice` | `BuffTrigger(..., beforeDuration = 2000)`; `IsActive` also requires `combo >= 1` |
| `rupture` | Replace the unused `RuptureTrigger` with a refresh version: `DebuffTrigger("rupture", 1, /*isOwner*/ true, /*needLifeTime*/ 12.0f, /*beforeDuration*/ 2000)`, plus `combo >= 4` |
| `hunger for blood` | Add `IsActive` override: self aura missing/expiring **and** a bleed on the target (`GetAura("rupture", target, true) \|\| GetAura("garrote", target, true)`) |
| `envenom` | **New.** `combo >= 4 && GetAura("deadly poison", target)` |
| `blade flurry` | **Fix the dead name.** Rename `BladeFuryTrigger` → `BladeFlurryTrigger`, spell string `"blade flurry"`, register under `"blade flurry"`. Today the strategy asks for `"blade flurry"` while the registry has `"blade fury"` — and no spell by that name exists either, so it is doubly dead |
| `killing spree` | **New.** `BoostTrigger(botAI, "killing spree")`, matching how `adrenaline rush` is done |
| `tricks of the trade` | **New.** Replaces the two duplicate nodes currently firing at identical relevance |

### 4. Values — new `src/Ai/Class/Rogue/RogueValues.h/.cpp` + a `RogueValueContext`

`RogueAiObjectContext::BuildSharedValueContexts`
([RogueAiObjectContext.cpp:236-239](modules/mod-playerbots/src/Ai/Class/Rogue/RogueAiObjectContext.cpp#L236-L239))
is currently a pass-through; add a rogue value context to it.

`TricksOfTheTradeTargetValue` (`"tricks of the trade target"`) — returns the main tank while
`low tank threat` holds or within the first ~10 s of combat (the `"combat start time"` value exists),
otherwise the highest-attack-power melee DPS group member within 20 yd excluding self; `nullptr` if
none. Reuses `PlayerbotAI::GetMainTankGuid`, `IsMelee`, `IsDps`.

### 5. Actions — `src/Ai/Class/Rogue/Action/*`

- **Envenom talent gate removed.** [RogueActions.cpp:72-76](modules/mod-playerbots/src/Ai/Class/Rogue/Action/RogueActions.cpp#L72-L76)
  gates `isPossible` on `HasAura(58410)` — Master Poisoner rank 3. Any Assassination rogue without
  3/3 in that talent silently never envenoms and falls through the action node to Eviscerate.
  Replace with `botAI->HasSpell("envenom")`; the Deadly Poison requirement moves to the new trigger.
  Keep the `energy >= 35` `isUseful`. Delete the now-unused `SPELL_MASTER_POISONER_RANK_3` constant.
- **Builders get the combo-point guard.** `CastSinisterStrikeAction`, `CastMutilateAction`,
  `CastBackstabAction` derive from `CastComboAction`
  ([RogueComboActions.h:14-20](modules/mod-playerbots/src/Ai/Class/Rogue/Action/RogueComboActions.h#L14-L20))
  instead of `CastSpellAction`. That class already implements `combo < 5` and is currently dead code
  — nothing derives from it. It also adds the melee range check, which all three want.
- **Rupture refresh window.** `CastDebuffSpellAction`
  ([GenericSpellActions.h:69-78](modules/mod-playerbots/src/Ai/Base/Actions/GenericSpellActions.h#L69-L78))
  swallows `beforeDuration` — it forwards only `isOwner` to `CastAuraSpellAction`. Add an optional
  trailing `beforeDuration = 0` parameter (non-breaking for the ~dozen existing call sites) and pass
  `2000` from `CastRuptureAction`.
- **`CastTricksOfTheTradeAction`** — new, `CastBuffSpellAction` with
  `GetTargetName() = "tricks of the trade target"` and the existing `distance < 20` `isUseful` check
  copied from `CastTricksOfTheTradeOnMainTankAction`. Keep the old main-tank action registered; the
  strategies switch to the new one.

### 6. Combat priority ladder — `DpsRogueStrategy::InitTriggers`

Replace [DpsRogueStrategy.cpp:78-232](modules/mod-playerbots/src/Ai/Class/Rogue/Strategy/DpsRogueStrategy.cpp#L78-L232)
wholesale. Drop both duplicate `high energy available` nodes, the duplicate Tricks nodes, and the
in-combat `garrote`/`ambush` node (both need stealth; move them behind the already-registered but
unused `"in stealth"` trigger). Remove `stealth` from the `enemy out of melee` node — the inherited
`CombatStrategy` already supplies `reach melee` at 21.

| Rel. | Trigger | Action |
|---|---|---|
| 42 / 41 | `kick`, `kick on enemy healer` | kick |
| 29 / 28 | `low health` | evasion, feint |
| 27 | `critical health` | cloak of shadows |
| 26 | `low tank threat` | tricks of the trade |
| 25 | `slice and dice` | slice and dice |
| 24 | `rupture` | rupture |
| 23 | `killing spree` | killing spree |
| 22 | `blade flurry` | blade flurry |
| 21 | `expose armor` | expose armor |
| 20.5 / 20 | `target with combo points almost dead`, `combo points 5 available` | eviscerate |
| 19 | `enemy out of melee` | sprint |
| 17 | `medium threat` | vanish |
| 13 | `combo points not full and medium energy` | sinister strike |

`killing spree` moves off the default-action list
([DpsRogueStrategy.cpp:70-76](modules/mod-playerbots/src/Ai/Class/Rogue/Strategy/DpsRogueStrategy.cpp#L70-L76)),
where at relevance 5.1 it only ever fired when nothing else could. `getDefaultActions` keeps `melee`
at 5.0 only.

Feint stays on `low health` alongside Evasion — with Glyph of Feint it is a 50% AoE damage
reduction, so it belongs in the survival band rather than with the threat tools.

Two new trigger names are needed in `TriggerContext`: `"combo points not full and medium energy"`
(mirror the existing `ComboPointsNotFullAndHighEnergy` at
[TriggerContext.h:376](modules/mod-playerbots/src/Ai/Base/TriggerContext.h#L376) with
`"medium energy available"`). Sinister Strike costs 40 energy; the current `high energy available`
gate at 60 makes the bot idle through the 40-59 band every cycle.

### 7. Assassination priority ladder — `AssassinationRogueStrategy::InitTriggers`

Same treatment for
[AssassinationRogueStrategy.cpp:73-227](modules/mod-playerbots/src/Ai/Class/Rogue/Strategy/AssassinationRogueStrategy.cpp#L73-L227).

| Rel. | Trigger | Action |
|---|---|---|
| 42 / 41 | `kick`, `kick on enemy healer` | kick |
| 29 / 28 | `low health` | evasion, feint |
| 27 | `critical health` | cloak of shadows |
| 26 | `low tank threat` | tricks of the trade |
| 25 | `slice and dice` | slice and dice |
| 24 | `rupture` | rupture |
| 23 | `hunger for blood` | hunger for blood |
| 22 | `expose armor` | expose armor |
| 21.5 / 21 | `envenom` | cold blood, envenom |
| 20 | `target with combo points almost dead` | envenom |
| 19 | `enemy out of melee` | sprint |
| 15 | `medium aoe` | fan of knives |
| 13 | `combo points not full and high energy` | mutilate |

Rupture is the headline addition — the spec currently never casts it at all (the action node exists
at [AssassinationRogueStrategy.cpp:50-58](modules/mod-playerbots/src/Ai/Class/Rogue/Strategy/AssassinationRogueStrategy.cpp#L50-L58)
but nothing emits it). That also unblocks Hunger for Blood, which needs a bleed on the target and
today can only get one from a stealth-opener Garrote.

Rupture sits below Slice and Dice because SnD comes first in the opener; Hunger for Blood sits below
Rupture because it depends on it.

### 8. Burst registry — `src/Ai/Base/Combat/BurstCooldowns.cpp:28`

Add `"killing spree"` to `burstCooldownNames` so it gets the same
`HoldBurstUntilTankEngagedMultiplier` treatment as Adrenaline Rush and Blade Flurry.

### 9. Planning doc

Write the findings + this change list to
`modules/mod-playerbots/docs/classes/rogue-combat-assassination-raid-improvements.PLAN.md`, matching
the format of the existing
[frost-dk-rotation-improvements-plan.md](modules/mod-playerbots/docs/classes/frost-dk-rotation-improvements-plan.md).

## Deferred, with reasons

- **Vanish → Ambush for Overkill (Assassination).** Real DPS, but Vanish drops combat and the bot's
  target, and the stealth re-entry path (`CheckStealthAction` → `stealthed` strategy) is only
  reachable via chat command — `"stealth"` and `"stealthed"` are never auto-added by `AiFactory`. A
  bot that vanishes mid-boss can plausibly just stop attacking. Not worth the risk in this pass.
- **Hemorrhage / Shadow Dance / Premeditation / Shiv.** Subtlety abilities or PVE-irrelevant.
- **Subtlety getting its own strategy.** Out of the agreed scope.

## Verification

The module cannot be compiled in this environment, so verification is static plus in-game.

1. **Name-resolution audit** (this is the exact bug class that produced the dead `blade flurry`
   trigger). Cross-check every `NextAction("x")` and `TriggerNode("y")` string in
   `src/Ai/Class/Rogue/Strategy/*.cpp` against the creator keys in
   [RogueAiObjectContext.cpp:60-196](modules/mod-playerbots/src/Ai/Class/Rogue/RogueAiObjectContext.cpp#L60-L196),
   [TriggerContext.h](modules/mod-playerbots/src/Ai/Base/TriggerContext.h) and
   [ActionContext.h](modules/mod-playerbots/src/Ai/Base/ActionContext.h). Every name must resolve.
2. **Spell-name audit.** Every `CastSpellAction` string must match a real `SpellInfo->SpellName[0]` —
   resolution is name-based via the `"spell id"` value, so a typo fails silently at runtime, not at
   compile time.
3. **Build.** No CMake changes needed — the module has no `CMakeLists.txt`; the core globs
   `modules/*/src`. New files require a cmake re-configure, then a full build.
4. **In-game, Combat rogue on a raid boss dummy or Patchwerk-style fight:**
   - `.playerbot co ?` shows `dps`, `boost`, `burst`, `aoe`.
   - Slice and Dice and Rupture uptime near 100%, refreshed before expiry rather than after.
   - Blade Flurry and Killing Spree fire on cooldown once the main tank has aggro, and are held on
     trash by the burst multiplier.
   - Sinister Strike fires at 40+ energy, never at 5 combo points.
5. **In-game, Assassination rogue:**
   - Rupture is applied and maintained; Hunger for Blood goes up shortly after and stays up.
   - Envenom is cast on a rogue **without** 3/3 Master Poisoner (the current bug) — verify it fires
     rather than falling back to Eviscerate.
6. **Expose Armor matrix** — pull the same boss four times and watch the rogue's finishers:
   - warrior in group → rogue never casts Expose Armor, including during the opener;
   - no warrior, no Sunder on target → rogue applies it once and maintains it;
   - hunter with worm pet, no warrior → rogue skips it;
   - solo/no group → rogue applies it.
