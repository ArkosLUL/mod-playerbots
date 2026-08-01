# Holy Paladin healing overhaul — findings + implementation plan

## Context

The Holy Paladin bot heals in raid PvE, but the ladder was written by copying relevance literals
rather than modelling the WotLK Holy rotation, and most of the spec's cooldowns are either bound to
the wrong trigger, tied with another action at the same relevance, or absent entirely. One of them —
Divine Plea — actively halves the bot's own healing for most of a raid fight.

This document records the audit and specifies the fix. It is self-contained: a fresh session can
execute it without the original conversation. Paths are relative to `modules/mod-playerbots/`.

The findings section should also be saved as `docs/classes/holy-paladin-healing-findings.md`,
matching `docs/classes/resto-shaman-healing-improvements-findings.md`.

---

## How the engine resolves a heal

- `Engine::DoNextAction` picks strictly by **relevance across all active trigger nodes**, so every
  number below is global to the bot's combat engine, not local to one trigger. Ties break by
  insertion order — which reads as nondeterministic in-game.
- Relevance constants (`src/Bot/Engine/Strategy/Strategy.h:53-65`): `ACTION_DEFAULT 5`,
  `ACTION_LIGHT_HEAL 10`, `ACTION_NORMAL 10`, `ACTION_HIGH 20`, `ACTION_MEDIUM_HEAL 20`,
  `ACTION_CRITICAL_HEAL 30`, `ACTION_INTERRUPT 40`, `ACTION_DISPEL 50`, `ACTION_EMERGENCY 90`.
- The four party health bands are **nested, not exclusive** — all pass `minValue = 0`
  (`src/Ai/Base/Trigger/HealthTriggers.h:88-128`). A target at 20% HP fires critical, low, medium
  *and* almost-full simultaneously; only relevance separates them.
- `Engine::PushDefaultActions` (`src/Bot/Engine/Engine.cpp:508-516`) pushes every active strategy's
  `getDefaultActions()` **every tick, ungated by any trigger**.
- Config defaults (`src/PlayerbotAIConfig.cpp:95-117`): critical 25, low 45, medium 65,
  almostFull 85, lowMana 15, mediumMana 40, highMana 65, saveManaThreshold 60, healDistance 38.5.
- Holy's strategy set (`src/Bot/Factory/AiFactory.cpp:348-349, 410-416`): `heal`, `dps assist`,
  `cure`, `bcast`, plus `save mana` and `healer dps`.

---

## Findings

### 1. Divine Plea halves the bot's healing for most of a raid fight

`GenericPaladinStrategy.cpp:34` binds `divine plea` @20 to the `high mana` trigger, which despite its
name is `mana < highMana` (65) — `src/Ai/Base/Trigger/GenericTriggers.cpp:75-79`. At relevance 20 it
outranks the medium band (19/18) and the almost-full band (13). In 3.3.5 Divine Plea reduces healing
done by 50% for 15s unless Glyph of Divine Plea is equipped. So the moment the paladin dips under
65% mana with nobody critically hurt — roughly 30 seconds into any raid fight — it cripples its own
throughput, repeatedly. No glyph check, no raid-damage suppression.

### 2. Holy Shock, the spec's rotational spell, fires almost nowhere

Its only healing node is `party member critical health` @36, i.e. below 25% HP. The WotLK Holy
rotation is Holy Shock on cooldown to proc Infusion of Light, then a discounted Holy Light or Flash
of Light. There is no Infusion of Light awareness anywhere in the tree and no Holy Shock in the low
or medium bands, so the instant sits idle.

Worse, `PaladinHealerDpsStrategy` (`GenericPaladinStrategy.cpp:70-82`) puts **offensive** Holy Shock
at 5.5. When nobody needs healing the bot spends the cooldown on damage, so it is down when damage
starts.

### 3. `estAmount = 50` disables Holy Light exactly when it is needed

Note the ordering itself is **correct for this class** — Holy Light leading the low and medium bands
is the WotLK Holy rotation (Light's Grace, Illumination, Beacon, Glyph of Holy Light), not a mistake
to be "fixed" into Flash spam. The bug is the mana-efficiency metadata.

`HealerAutoSaveManaMultiplier` (`src/Ai/Base/Strategy/ConserveManaStrategy.cpp:93-131`; the non-tank
health guard from the shaman work is already applied) vetoes a heal when
`lossAmount < estAmount || manaEfficiency <= <band>`. With
`CastHolyLightOnPartyAction(botAI, "holy light", 50.0f, MEDIUM)` (`PaladinActions.h:165-170`),
below 60% bot mana Holy Light is vetoed at target HP ≥ 65 always, and at HP ≥ 45 unless the target
has lost ≥50% of its health. A Holy Light heals nowhere near 50% of a raider's health pool, so the
number is simply wrong and it takes the spec's main heal offline under mana pressure.

`holy shock on party` has the same problem at `25.0f, LOW` — LOW is vetoed at any target HP ≥ 45.

### 4. `holy light on party` has no fallback, and the existing chain points the wrong way

`GenericPaladinStrategyActionNodeFactory.h:154-167` defines `flash of light on party` →
`holy light on party`. That is backwards: if the cheap fast heal fails, escalating to a slower more
expensive one is worse. The direction that matters — Holy Light vetoed or failing, fall back to
Flash — does not exist, so a vetoed Holy Light dead-ends and recovery only happens by falling
through three lower trigger nodes.

### 5. Beacon of Light and Sacred Shield are main-tank-only and never re-evaluated

Both use `BuffOnMainTankTrigger` / `BuffOnMainTankAction`
(`src/Ai/Base/Trigger/GenericTriggers.h:961-969`, `src/Ai/Base/Actions/GenericSpellActions.h:488-497`)
resolving through `PartyMemberMainTankValue` → `FindMainTankPlayer`
(`src/Ai/Base/Value/PartyMemberValue.cpp:174-178`). In a two-tank raid the off-tank never gets
either, and Beacon never follows a tank swap. Beacon is a throughput multiplier on everything the
paladin casts, so this is the single largest healing-output item in the list.

### 6. Relevance collisions

Resolved by insertion order:

- **93** — `reach party member to heal` (Holy) = `blessing of protection on party` =
  `flash of light` (divine shield low health). Three-way.
- **36** — `sacred shield on main tank` = `holy shock on party`.
- **35** — `divine sacrifice` on two different trigger nodes.
- **20** — `seal of wisdom` = `divine plea`.

Separately `reach party member to heal` @93 outranks **every emergency heal**: a Holy paladin whose
selected target is out of range runs instead of using Lay on Hands (92) on a different dying player.
Priest uses 40 for the same node (`HealPriestStrategy.cpp:97-104`); the shaman rebase settled on 40.

### 7. The default action is an ungated melee-range mana spend

`getDefaultActions()` returns `judgement of light` @5.0, pushed every tick by `PushDefaultActions`,
so it bypasses the `HealerShouldAttackTrigger` mana floor
(`src/Ai/Base/Trigger/GenericTriggers.cpp:443-471`) that gates the *identical* node inside
`healer dps` @5.3. Judgement is a 10-yard spell, so it also drags the healer toward the boss.

### 8. `healer dps` contains two entries wrong for Holy

`shield of righteousness` (5.4) requires a shield — Protection only. `consecration` (5.2) is a
~660-mana self-centred ground AoE at melee range on a healer: a mana leak and a positioning hazard.

### 9. Cooldown audit

| Cooldown | Implemented | Reachable | Trigger | Rel | Verdict |
|---|---|---|---|---|---|
| **Aura Mastery** | **No** | — | — | — | Not present at all: no action, no trigger, no spell ID 31821 anywhere. |
| **Divine Sacrifice** | Yes | Yes, twice | `medium group heal setting` **and** `party member critical health` | 35 / 35 | Identical relevance on two triggers → tie. Single-target critical health is the wrong trigger for a raid damage-redirect. `isUseful` (`PaladinActions.cpp:591`) has no self-health floor, so a low-HP paladin redirects raid damage onto itself. |
| **Divine Illumination** | Yes | Yes | `medium mana` (bot mana < 40) | 22 | Self-mana gated only. It is a throughput cooldown (-50% cost, 3 min) that should cover a heavy-healing window, not idle until 40% mana. |
| **Divine Plea** | Yes (inherited) | Yes | `high mana` = mana < 65 | 20 | See finding 1. Worst offender. |
| **Avenging Wrath** | Yes | Yes | `medium group heal setting` | 24 | Right intent (+20% healing during raid damage), wrong price — below every critical/low heal, so it only lands in a lull. Also on the burst gate list (`src/Ai/Base/Combat/BurstCooldowns.cpp:40`), so `HoldBurstUntilTankEngagedMultiplier` can suppress it for a healer. Its own `AvengingWrathTrigger` is used by `dps`/`tank`/`offheal` but not `heal`. |
| **Lay on Hands** | Yes | Yes | self `critical health` @91, party `party member critical health` @92 | 91 / 92 | Correctly priced. **No Forbearance guard on either variant** — after Divine Shield (90) lands, the next tick's LoH is a guaranteed failure that still burns the action slot. `CastLayOnHandsOnPartyAction` (`PaladinActions.h:191-195`) also omits its `estAmount` / `manaEfficiency` args. |
| **Divine Favor** | Yes | Yes, wrong trigger | `low mana` (mana < 15) | 21 | A guaranteed-crit-next-heal cooldown gated on being nearly OOM. Its purpose-built `DivineFavorTrigger` is registered (`PaladinAiObjectContext.cpp:141, 165`) but dead — its only consumer, `PaladinBoostStrategy::InitTriggers`, is entirely commented out (`GenericPaladinStrategy.cpp:58-62`). |
| **Divine Protection** | Yes | Only as `/*A*/` fallback of `divine shield` | — | — | No direct trigger. `divine protection on party` is registered (`PaladinAiObjectContext.cpp:286, 393`) and referenced by zero trigger nodes — dead code. |
| **Divine Shield** | Yes | Yes | self `critical health` | 90 | OK, but applies Forbearance which then silently breaks LoH @91. |
| **Hand of Protection** | As `blessing of protection on party` | Yes | `protect party member` | 93 | Has the Forbearance guard. Depends on `PartyMemberToProtect` being live — see `docs/classes/resto-shaman-healing-improvements-findings.md:198-237`. |
| **Hand of Sacrifice** | **No** | — | — | — | Spell ID 6940 exists only as an exclusion constant, `PaladinHelper.h:21`. |
| **Hand of Salvation** | **No** | — | — | — | Spell ID 1038, same, `PaladinHelper.h:19`. |
| **Hand of Freedom** | Yes | Yes | `hand of freedom on party` | 24 | Fine. Self-preference and "already has a Hand" guard implemented (`PaladinActions.cpp:547-580`). |
| **Beacon of Light** | Yes | Yes | `beacon of light on main tank` | 37 | See finding 5. |
| **Sacred Shield** | Yes | Yes | `sacred shield on main tank` | 36 | See finding 5. Ties with `holy shock on party`. |

Confirmed correct, not gaps: Holy Radiance does not exist in 3.3.5; `bcast` gives Holy the
Concentration Aura (`PaladinBuffStrategies.cpp:66-70`); Avenging Wrath does not apply Forbearance in
3.3.5; the out-of-combat ladder in `GenericPaladinNonCombatStrategy.cpp` is ordered correctly.

---

## Implementation

Naming note that matters: a missing `creators[...]` entry in `PaladinAiObjectContext.cpp` is a
**silent runtime no-op, not a compile error**. Every new action/trigger/value name below must be
registered.

### A. `src/Ai/Class/Paladin/Actions/PaladinActions.h` / `.cpp`

Mana-efficiency metadata — read only by `HealerAutoSaveManaMultiplier`, compared against
`lossAmount = 100 - targetHealthPct`:

| Action | Before | After | Why |
|---|---|---|---|
| `CastHolyLightOnPartyAction` | `50.0f, MEDIUM` | `25.0f, MEDIUM` | 50 vetoed the spec's main heal unless the target had lost half its health. |
| `CastHolyShockOnPartyAction` | `25.0f, LOW` | `15.0f, HIGH` | It is an instant that also procs Infusion of Light; it must survive the veto so the cooldown keeps rolling. |
| `CastFlashOfLightOnPartyAction` | `15.0f, HIGH` | unchanged | Correct — the low-mana backstop. |
| `CastLayOnHandsOnPartyAction` | defaults (15 / MEDIUM) | `15.0f, SUPERIOR` | Explicit; SUPERIOR means the efficiency clause never vetoes the emergency button. |

Guards:

- `CastLayOnHandsAction` and `CastLayOnHandsOnPartyAction` — add `isUseful()` returning the base
  check plus `!botAI->HasAura("forbearance", GetTarget())`. Both need out-of-line bodies in `.cpp`.
- `CastDivineSacrificeAction::isUseful()` (`PaladinActions.cpp:591`) — add
  `AI_VALUE2(uint8, "health", "self target") >= 50`. The spell redirects 30% of raid damage onto the
  paladin; casting it while already hurt kills the healer.
- `CastDivinePleaAction` — add `isUseful()`: for a healer (`botAI->IsHeal(bot)`) without Glyph of
  Divine Plea, require `AI_VALUE2(uint8, "aoe heal", "medium") == 0`, i.e. nobody in range below 65%
  HP. Non-healers and glyphed paladins keep current behaviour. Glyphs are applied as passive auras,
  so the check is `bot->HasAura(SPELL_GLYPH_DIVINE_PLEA)`; add that ID (63223) to
  `PaladinHelper.h` alongside the existing Hand constants.

New actions:

- `CastAuraMasteryAction : CastBuffSpellAction(botAI, "aura mastery")`.
- `CastHandOfSacrificeOnPartyAction : CastProtectSpellAction` — target `"party member to protect"`
  (the generic value, tanks included, unlike BoP's `"party member to protect no tank"`), name
  `"hand of sacrifice on party"`. `isUseful()`: base check, plus a self-health floor of 50% for the
  same reason as Divine Sacrifice, plus `!HasAnyPaladinHandFromCaster(...)` (`PaladinHelper.h:39-43`).
  Hand of Sacrifice does **not** apply Forbearance, so no Forbearance guard.
- `CastBeaconOfLightOnTankAction` / `CastSacredShieldOnTankAction : BuffOnMainTankAction` overriding
  `GetTargetValue()` to return the new `"tank to beacon"` value (section D).

### B. `src/Ai/Class/Paladin/PaladinTriggers.h` / `.cpp`

- `PaladinInfusionOfLightTrigger : HasAuraTrigger` — name `"infusion of light"`.
  `IsActive()` = base has-aura check **and**
  `AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig.almostFullHealth`, so a
  proc is only spent when there is actually something to heal.
- `PaladinDivinePleaTrigger : Trigger` — name `"paladin divine plea"`.
  `IsActive()` = `has mana` && `mana < mediumMana` (40) && `AI_VALUE2(uint8, "aoe heal", "medium") == 0`.
  This is the healer-safe replacement for the inherited `high mana` node.
- `BeaconOfLightOnTankTrigger` / `SacredShieldOnTankTrigger : BuffOnMainTankTrigger` overriding
  `GetTargetValue()` to `"tank to beacon"`, keeping the existing `checkIsOwner` flags
  (`true` for Beacon, `false` for Sacred Shield).

Delete the dead `DivineFavorTrigger` class and its registration, and delete the commented-out body of
`PaladinBoostStrategy::InitTriggers` (`GenericPaladinStrategy.cpp:58-62`) — Divine Favor moves onto
a health trigger, so neither is reachable or wanted.

### C. `src/Ai/Class/Paladin/Strategy/` — the ladder

`GenericPaladinStrategy.cpp`:

- **Move** the `high mana` → `divine plea` node out of `GenericPaladinStrategy::InitTriggers` into
  `TankPaladinStrategy::InitTriggers` and `DpsPaladinStrategy::InitTriggers` verbatim (@20).
  `OffhealRetPaladinStrategy` already has its own on `low mana` @24. Heal gets the gated node below.
  This is the only way to keep it for the other three specs while replacing it for Holy — a derived
  `InitTriggers` cannot remove a base node.
- `divine shield low health` → flash of light 93→**89**, holy light 92→**88**. Topping up inside the
  bubble must not outrank a dying ally.
- `protect party member` → add `hand of sacrifice on party` @**93**; lower
  `blessing of protection on party` 93→**92.8**. BoP wipes tank threat and applies Forbearance, so it
  is the last resort.
- `PaladinHealerDpsStrategy` — drop `shield of righteousness` (Prot-only) and `consecration`
  (melee-range mana sink). Drop offensive `holy shock` so the cooldown stays banked for Infusion.
  Remaining: hammer of wrath 5.6, judgement of light 5.3, exorcism 5.1.

`HealPaladinStrategy.cpp` — `getDefaultActions()` returns `{}`. The gated `healer dps` nodes are the
correct idle behaviour for a healer; an ungated 10-yard mana spend is not.

Full new `HealPaladinStrategy::InitTriggers` ladder:

| Trigger | Action | Rel | Note |
|---|---|---|---|
| `beacon of light on tank` | beacon of light on tank | 37 | retargeting value |
| `party member critical health` | holy shock on party | 36 | |
| `party member critical health` | divine favor | 35.5 | **moved off `low mana`** |
| `medium group heal setting` | divine sacrifice | 35 | **now the only Divine Sacrifice node** |
| `party member critical health` | holy light on party | 34 | |
| `party member critical health` | flash of light on party | 33 | **new** — cheap fallback in the critical band |
| `sacred shield on tank` | sacred shield on tank | 32 | retargeting value, ties broken |
| `medium group heal setting` | avenging wrath | 31 | raised from 24 |
| `medium group heal setting` | divine illumination | 30.5 | **new** — the throughput window |
| `medium group heal setting` | aura mastery | 30 | **new** |
| `infusion of light` | holy light on party | 26.5 | **new** — spend the proc on the big heal |
| `party member low health` | holy shock on party | 26 | **new** — filler, keeps Infusion rolling |
| `party member low health` | holy light on party | 25 | |
| `party member low health` | flash of light on party | 23.5 | **new** — clears `hand of freedom on party` @24 |
| `medium mana` | divine illumination | 22 | kept as the mana-driven fallback node |
| `seal` | seal of wisdom | 20 | |
| `party member medium health` | holy shock on party | 19.5 | **new** |
| `party member medium health` | holy light on party | 19 | |
| `party member medium health` | flash of light on party | 18 | |
| `party member almost full health` | flash of light on party | 13 | |
| `paladin divine plea` | divine plea | 12 | **new gated node**, below every heal band |
| `party member to heal out of spell range` | reach party member to heal | 39.5 | 93→39.5; 39.5 not 40 to clear `hammer of justice` @40 |

`GenericPaladinStrategyActionNodeFactory.h` — **reverse** the Flash/Holy Light chain:
delete `flash of light on party` → `/*A*/ holy light on party`, add
`holy light on party` → `/*A*/ flash of light on party`. Do not add both: A→B→A is a cycle in
action-node expansion.

### D. New value — `"tank to beacon"`

`src/Ai/Class/Paladin/` (paladin-scoped; register in `PaladinValueContextInternal`, which currently
holds only the two greater-blessing values, `PaladinAiObjectContext.cpp:441-461`).

`PaladinTankToBeaconValue : PartyMemberValue`, 2000 ms check interval to match
`PartyMemberMainTankValue`:

1. Collect group members where `botAI->IsTank(player)`, alive, within `sPlayerbotAIConfig.healDistance`,
   `bot->IsWithinLOSInMap(player)`, and in combat.
2. Pick the lowest health% among them.
3. **Hysteresis** — cache the last returned GUID; keep it unless it is dead, out of range, or the new
   candidate is at least 20 percentage points lower. Without this, Beacon thrashes between two tanks
   every GCD and heals nothing.
4. Fall back to `AI_VALUE(Unit*, "main tank member")` when the list is empty (pre-pull, solo, no tank
   in range).

### E. Burst gate exemption

`src/Ai/Base/Strategy/BurstWindowStrategy.cpp:12-27` — `HoldBurstUntilTankEngagedMultiplier` holds
burst cooldowns until the main tank has held the boss. For a healer, Avenging Wrath is a *healing*
cooldown, not a damage one. Add an exemption next to the existing `"use trinket"` one, same shape:

```cpp
if (name == "avenging wrath" && PlayerbotAI::IsHeal(bot))
    return 1.0f;
```

### F. Registration — `src/Ai/Class/Paladin/PaladinAiObjectContext.cpp`

- Triggers: `"infusion of light"`, `"paladin divine plea"`, `"beacon of light on tank"`,
  `"sacred shield on tank"`. Remove `"divine favor"`.
- Actions: `"aura mastery"`, `"hand of sacrifice on party"`, `"beacon of light on tank"`,
  `"sacred shield on tank"`.
- Values: `"tank to beacon"` in `PaladinValueContextInternal`.
- Dead code to delete: `"divine protection on party"` action creator and
  `CastDivineProtectionOnPartyAction` (referenced by zero trigger nodes).

---

## Files to modify

- `src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp` — the ladder, default actions
- `src/Ai/Class/Paladin/Strategy/GenericPaladinStrategy.cpp` — Divine Plea relocation, emergency
  reprice, `PaladinHealerDpsStrategy`, `PaladinBoostStrategy` deletion
- `src/Ai/Class/Paladin/Strategy/TankPaladinStrategy.cpp`, `DpsPaladinStrategy.cpp` — receive the
  relocated Divine Plea node
- `src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h` — fallback chain reversal
- `src/Ai/Class/Paladin/Actions/PaladinActions.h` / `.cpp` — metadata, guards, new actions
- `src/Ai/Class/Paladin/PaladinTriggers.h` / `.cpp` — new triggers
- `src/Ai/Class/Paladin/PaladinHelper.h` — Glyph of Divine Plea ID
- `src/Ai/Class/Paladin/PaladinAiObjectContext.cpp` — registration
- New: `src/Ai/Class/Paladin/Value/PaladinTankToBeaconValue.h` / `.cpp` (add to the module CMake
  source glob if the build does not glob automatically)
- `src/Ai/Base/Strategy/BurstWindowStrategy.cpp` — healer Avenging Wrath exemption

Blast radius outside Holy: the Divine Plea relocation touches Prot and Ret paladins (behaviour
unchanged — same trigger, same relevance, just declared per-spec). The `BurstWindowStrategy` change
affects any healer with a burst-listed cooldown, which today is only Avenging Wrath and Shadowfiend.

---

## Verification

The module cannot be compiled headless in this environment, so verification is static review plus an
in-game pass by the user.

Static:

1. Every action/trigger/value name string used in `HealPaladinStrategy.cpp` and the new paladin
   triggers has a matching `creators[...]` entry in `PaladinAiObjectContext.cpp`.
2. No two `NextAction` entries reachable in the same tick share a relevance value — check the new
   ladder against `GenericPaladinStrategy` (interrupts 40, hand of freedom 24, seal 20),
   `PaladinCureStrategy` (51/52) and `PaladinBuffCastStrategy` (concentration aura 10).
3. No cycle in the action-node graph: `flash of light on party` must no longer list
   `holy light on party` as an alternative.

In-game:

4. Build the module, start a server with a Holy Paladin bot in a raid group.
5. `.playerbot debug` on the paladin, damage a party member through each band, and confirm the cast
   order: Holy Shock → Holy Light in the low band, Holy Shock → Holy Light → Flash in critical.
6. Drain the paladin to 55% mana with a target at 60% HP — it must still cast Holy Light (previously
   vetoed by `estAmount = 50`).
7. Drain to 55% mana with the raid at full health — Divine Plea should fire. Repeat with raid damage
   landing — it must **not** fire.
8. Two-tank Naxx 25 pull, then a tank swap — Beacon and Sacred Shield must follow the tank taking
   damage, and must not flip back and forth every GCD.
9. Trigger a raid-wide AoE hit — expect Divine Sacrifice, Avenging Wrath, Divine Illumination and
   Aura Mastery in the `medium group heal setting` window.
10. Bubble the paladin, then drop an ally to 10% — expect Lay on Hands to be skipped (Forbearance)
    rather than attempted, and the paladin to heal instead.
