# Holy Paladin healing overhaul — findings

Investigation record for the Holy Paladin bot healing overhaul. Self-contained: a fresh session can
review or extend the change from this document alone. Paths are relative to `modules/mod-playerbots/`.

## How the engine resolves a heal

- `Engine::DoNextAction` picks strictly by **relevance across all active trigger nodes**, so every
  number below is global to the bot's combat engine, not local to one trigger. Ties break by
  insertion order, which reads as nondeterministic in-game.
- Relevance constants (`src/Bot/Engine/Strategy/Strategy.h:53-65`): `ACTION_DEFAULT 5`,
  `ACTION_LIGHT_HEAL 10`, `ACTION_NORMAL 10`, `ACTION_HIGH 20`, `ACTION_MEDIUM_HEAL 20`,
  `ACTION_CRITICAL_HEAL 30`, `ACTION_INTERRUPT 40`, `ACTION_DISPEL 50`, `ACTION_EMERGENCY 90`.
- The four party health bands are **nested, not exclusive** — all pass `minValue = 0`
  (`src/Ai/Base/Trigger/HealthTriggers.h:88-128`). A target at 20% HP fires critical, low, medium
  *and* almost-full at once; only relevance separates them.
- `Engine::PushDefaultActions` (`src/Bot/Engine/Engine.cpp:508-516`) pushes every active strategy's
  `getDefaultActions()` every tick, ungated by any trigger.
- Config defaults (`src/PlayerbotAIConfig.cpp:95-117`): critical 25, low 45, medium 65,
  almostFull 85, lowMana 15, mediumMana 40, highMana 65, saveManaThreshold 60, healDistance 38.5.
- Holy's strategy set (`src/Bot/Factory/AiFactory.cpp:348-349, 410-416`): `heal`, `dps assist`,
  `cure`, `bcast`, plus `save mana` and `healer dps`.

## Findings

### 1. Divine Plea halved the bot's healing for most of a raid fight

`GenericPaladinStrategy.cpp` bound `divine plea` @20 to the `high mana` trigger, which despite its
name is `mana < highMana` (65) — `src/Ai/Base/Trigger/GenericTriggers.cpp:75-79`. At relevance 20 it
outranked the medium band (19/18) and the almost-full band (13). In 3.3.5 Divine Plea reduces healing
done by 50% for 15s unless Glyph of Divine Plea is equipped. So the moment the paladin dipped under
65% mana with nobody critically hurt — roughly 30 seconds into any raid fight — it crippled its own
throughput, repeatedly. No glyph check, no raid-damage suppression.

### 2. Holy Shock, the spec's rotational spell, fired almost nowhere

Its only healing node was `party member critical health` @36, i.e. below 25% HP. The WotLK Holy
rotation is Holy Shock on cooldown to proc Infusion of Light, then a discounted Holy Light or Flash
of Light. There was no Infusion of Light awareness anywhere in the tree and no Holy Shock in the low
or medium bands, so the instant sat idle.

Worse, `PaladinHealerDpsStrategy` put **offensive** Holy Shock at 5.5. When nobody needed healing the
bot spent the cooldown on damage, so it was down when damage started.

### 3. `estAmount = 50` disabled Holy Light exactly when it was needed

The ordering itself is **correct for this class** — Holy Light leading the low and medium bands is
the WotLK Holy rotation (Light's Grace, Illumination, Beacon, Glyph of Holy Light), not a mistake to
be "fixed" into Flash spam. The bug was the mana-efficiency metadata.

`HealerAutoSaveManaMultiplier` (`src/Ai/Base/Strategy/ConserveManaStrategy.cpp:93-131`) vetoes a heal
when `lossAmount < estAmount || manaEfficiency <= <band>`. With
`CastHolyLightOnPartyAction(botAI, "holy light", 50.0f, MEDIUM)`, below 60% bot mana Holy Light was
vetoed at target HP ≥ 65 always, and at HP ≥ 45 unless the target had lost ≥50% of its health. A Holy
Light heals nowhere near 50% of a raider's health pool, so the number was simply wrong and it took
the spec's main heal offline under mana pressure. `holy shock on party` had the same problem at
`25.0f, LOW` — LOW is vetoed at any target HP ≥ 45.

### 4. `holy light on party` had no fallback, and the existing chain pointed the wrong way

`GenericPaladinStrategyActionNodeFactory.h` defined `flash of light on party` →
`holy light on party`. That is backwards: if the cheap fast heal fails, escalating to a slower, more
expensive one is worse. The direction that matters — Holy Light vetoed or failing, fall back to
Flash — did not exist, so a vetoed Holy Light dead-ended and recovery only happened by falling
through three lower trigger nodes.

### 5. Beacon of Light and Sacred Shield were main-tank-only and never re-evaluated

Both used `BuffOnMainTankTrigger` / `BuffOnMainTankAction`
(`src/Ai/Base/Trigger/GenericTriggers.h:961-969`, `src/Ai/Base/Actions/GenericSpellActions.h:488-497`)
resolving through `PartyMemberMainTankValue` → `FindMainTankPlayer`
(`src/Ai/Base/Value/PartyMemberValue.cpp:174-178`). In a two-tank raid the off-tank never got either,
and Beacon never followed a tank swap. Beacon is a throughput multiplier on everything the paladin
casts, so this is the single largest healing-output item in the list.

### 6. Relevance collisions

Resolved by insertion order:

- **93** — `reach party member to heal` (Holy) = `blessing of protection on party` =
  `flash of light` (divine shield low health). Three-way.
- **36** — `sacred shield on main tank` = `holy shock on party`.
- **35** — `divine sacrifice` on two different trigger nodes.
- **20** — `seal of wisdom` = `divine plea`.

Separately `reach party member to heal` @93 outranked **every emergency heal**: a Holy paladin whose
selected target was out of range ran instead of using Lay on Hands (92) on a different dying player.
Priest uses 40 for the same node (`HealPriestStrategy.cpp:97-104`); the shaman rebase settled on 40.

### 7. The default action was an ungated melee-range mana spend

`getDefaultActions()` returned `judgement of light` @5.0, pushed every tick by `PushDefaultActions`,
so it bypassed the `HealerShouldAttackTrigger` mana floor
(`src/Ai/Base/Trigger/GenericTriggers.cpp:443-471`) that gates the *identical* node inside
`healer dps` @5.3, and spent ~5% base mana on a Judgement that the gated node had just refused.

It did **not** drag the healer toward the boss, as an earlier draft of the plan claimed:
`CastSpellAction::getPrerequisites()` returns `{}`
(`src/Ai/Base/Actions/GenericSpellActions.h:31-34`), so an out-of-range Judgement simply fails that
tick and no movement action is queued.

Removing it did cost Judgement uptime — see the `paladin judgement of light` note below.

### 8. `healer dps` contained two entries wrong for Holy

`shield of righteousness` (5.4) requires a shield — Protection only. `consecration` (5.2) is a
~660-mana self-centred ground AoE at melee range on a healer: a mana leak and a positioning hazard.

### 9. Cooldown audit

| Cooldown | Implemented | Reachable | Trigger | Rel | Verdict |
|---|---|---|---|---|---|
| **Aura Mastery** | **No** | — | — | — | Not present at all: no action, no trigger, no spell ID 31821 anywhere. |
| **Divine Sacrifice** | Yes | Yes, twice | `medium group heal setting` **and** `party member critical health` | 35 / 35 | Identical relevance on two triggers → tie. Single-target critical health is the wrong trigger for a raid damage-redirect. `isUseful` had no self-health floor, so a low-HP paladin redirected raid damage onto itself. |
| **Divine Illumination** | Yes | Yes | `medium mana` (bot mana < 40) | 22 | Self-mana gated only. It is a throughput cooldown (-50% cost, 3 min) that should cover a heavy-healing window, not idle until 40% mana. |
| **Divine Plea** | Yes (inherited) | Yes | `high mana` = mana < 65 | 20 | See finding 1. Worst offender. |
| **Avenging Wrath** | Yes | Yes | `medium group heal setting` | 24 | Right intent (+20% healing during raid damage), wrong price — below every critical/low heal, so it only landed in a lull. Also on the burst gate list (`src/Ai/Base/Combat/BurstCooldowns.cpp:40`), so `HoldBurstUntilTankEngagedMultiplier` could suppress it for a healer. Its own `AvengingWrathTrigger` is used by `dps`/`tank`/`offheal` but not `heal`. |
| **Lay on Hands** | Yes | Yes | self `critical health` @91, party `party member critical health` @92 | 91 / 92 | Correctly priced. **No Forbearance guard on either variant** — after Divine Shield (90) landed, the next tick's LoH was a guaranteed failure that still burned the action slot. `CastLayOnHandsOnPartyAction` also omitted its `estAmount` / `manaEfficiency` args. |
| **Divine Favor** | Yes | Yes, wrong trigger | `low mana` (mana < 15) | 21 | A guaranteed-crit-next-heal cooldown gated on being nearly OOM. Its purpose-built `DivineFavorTrigger` was registered but dead — its only consumer, `PaladinBoostStrategy::InitTriggers`, was entirely commented out. |
| **Divine Protection** | Yes | Only as `/*A*/` fallback of `divine shield` | — | — | No direct trigger. `divine protection on party` was registered and referenced by zero trigger nodes — dead code. |
| **Divine Shield** | Yes | Yes | self `critical health` | 90 | OK, but applies Forbearance which then silently broke LoH @91. |
| **Hand of Protection** | As `blessing of protection on party` | Yes | `protect party member` | 93 | Has the Forbearance guard. Depends on `PartyMemberToProtect` being live — see `docs/classes/resto-shaman-healing-improvements-findings.md:198-237`. |
| **Hand of Sacrifice** | **No** | — | — | — | Spell ID 6940 existed only as an exclusion constant, `PaladinHelper.h:21`. |
| **Hand of Salvation** | **No** | — | — | — | Spell ID 1038, same. |
| **Hand of Freedom** | Yes | Yes | `hand of freedom on party` | 24 | Fine. Self-preference and "already has a Hand" guard implemented. |
| **Beacon of Light** | Yes | Yes | `beacon of light on main tank` | 37 | See finding 5. |
| **Sacred Shield** | Yes | Yes | `sacred shield on main tank` | 36 | See finding 5. Ties with `holy shock on party`. |

Confirmed correct, not gaps: Holy Radiance does not exist in 3.3.5; `bcast` gives Holy the
Concentration Aura (`PaladinBuffStrategies.cpp:66-70`); Avenging Wrath does not apply Forbearance in
3.3.5; the out-of-combat ladder in `GenericPaladinNonCombatStrategy.cpp` is ordered correctly.

## What shipped

Implementation plan: `docs/classes/holy-paladin-improvements-plan.md`. Deviations from it:

- The Divine Plea fallback value key is `"main tank"`, not `"main tank member"` — the latter is the
  value's own name, the former is its registration key (`src/Ai/Base/ValueContext.h:318`).
- Divine Plea is allowed through the raid-damage suppression below `lowMana` (15), on both the
  trigger and `CastDivinePleaAction::isUseful`, and a second `low mana` → `divine plea` node sits at
  23 — above the medium band, below the low band. An unglyphed healer with an empty bar heals
  nothing, so the 50% penalty is the cheaper price there.
- The `tank to beacon` hysteresis margin is 30 percentage points, not 20.
- Judgement of Light is maintained on a debuff trigger at 27, which the plan did not have at all.
  Dropping the ungated default action (finding 7) left `healer dps` @5.3 as the only path, and that
  node is gated by `HealerShouldAttackTrigger` — false whenever anyone is below 85% HP or mana is
  under the balance threshold, i.e. most of a fight. Judgements of the Pure (15% haste, 60s) and the
  Judgement of Light melee-swing raid heal are worth one global per 20s.
  `PaladinJudgementOfLightTrigger` is a `DebuffTrigger` with `checkIsOwner = true` — a second
  paladin's Judgement of Light must not stop this one refreshing its own Judgements of the Pure —
  and calls `CanCastSpell` so the node goes quiet at range instead of failing every tick. Judgement
  is 10 yards; the node never queues movement, it just waits until the paladin is close enough.
- Divine Illumination is pinned to Avenging Wrath instead of standing on its own mana node.
  `CastDivineIlluminationAction::isUseful` requires the `avenging wrath` aura for a healer, so the
  `medium group heal setting` pair (Avenging Wrath 31, Divine Illumination 30.5) always lands in
  that order on consecutive ticks and the -50% cost window covers the +20% healing window. Both are
  3 minute cooldowns, so they stay aligned. The old `medium mana` (40) node became a `low mana` (15)
  bail-out at 22, sharing a node with Divine Plea at 23 — an unglyphed healer that is nearly out of
  mana still gets the discount even with Avenging Wrath down.

Final Holy combat ladder (`src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp`, on top of
`GenericPaladinStrategy`):

| Trigger | Action | Rel |
|---|---|---|
| `party member to heal out of spell range` | reach party member to heal | 39.5 |
| `beacon of light on tank` | beacon of light on tank | 37 |
| `party member critical health` | holy shock on party | 36 |
| `party member critical health` | divine favor | 35.5 |
| `medium group heal setting` | divine sacrifice | 35 |
| `party member critical health` | holy light on party | 34 |
| `party member critical health` | flash of light on party | 33 |
| `sacred shield on tank` | sacred shield on tank | 32 |
| `medium group heal setting` | avenging wrath | 31 |
| `medium group heal setting` | divine illumination | 30.5 |
| `medium group heal setting` | aura mastery | 30 |
| `paladin judgement of light` | judgement of light | 27 |
| `infusion of light` | holy light on party | 26.5 |
| `party member low health` | holy shock on party | 26 |
| `party member low health` | holy light on party | 25 |
| `party member low health` | flash of light on party | 23.5 |
| `low mana` | divine plea | 23 |
| `low mana` | divine illumination | 22 |
| `seal` | seal of wisdom | 20 |
| `party member medium health` | holy shock on party | 19.5 |
| `party member medium health` | holy light on party | 19 |
| `party member medium health` | flash of light on party | 18 |
| `party member almost full health` | flash of light on party | 13 |
| `paladin divine plea` | divine plea | 12 |

Inherited from `GenericPaladinStrategy`: `hand of sacrifice on party` 93,
`blessing of protection on party` 92.8, `lay on hands on party` 92, `lay on hands` 91,
`divine shield` 90, `flash of light` 89 / `holy light` 88 (inside the bubble),
`hand of freedom on party` 24, the three Hammer of Justice interrupts at 40.

## Verification status

Static only — the module cannot be compiled headless in this environment. Every action, trigger and
value name used by the new ladder has a matching `creators[...]` entry in
`PaladinAiObjectContext.cpp`; no two relevances collide within the Holy engine except the
pre-existing `reach spell` / `seal of wisdom` pair at 20 (`CombatStrategy.cpp:17`) and the three
interrupts at 40; the action-node graph has no cycle (`flash of light on party` no longer lists
`holy light on party` as an alternative). The in-game checklist is in the plan document.
