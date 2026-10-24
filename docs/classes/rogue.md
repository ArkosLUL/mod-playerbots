# Rogue — Combat and Assassination

Strategy keys: `combat` (`CombatRogueStrategy`), `assassin` (`AssassinationRogueStrategy`). Engine
semantics are in [../engine/action-selection.md](../engine/action-selection.md).

**Subtlety has no strategy of its own.** `SubtletyRogueStrategy.cpp` is a commented-out stub and its
`subtlety` creator is commented out, so `AiFactory::AddDefaultCombatStrategies` gives Subtlety
`assassin`, and it inherits whatever improves there.

`GenericRogueStrategy`'s `MeleeCombatStrategy` parent and Tricks of the Trade's 26.5 are local
divergences a merge must re-apply
([pitfalls](../engine/pitfalls.md#local-divergences-a-merge-must-re-apply)). Every trigger the specs
share (poisons, kicks, survival, Tricks, Sprint) lives in `GenericRogueStrategy` in this tuned form;
the spec strategies carry only their rotation, stealth openers and Combat's Vanish.

## Design decisions

- **Armor-debuff ownership.** A rogue applies Expose Armor only when **nobody else in the group can**,
  not merely when the debuff currently happens to be missing. A `HasAura("sunder armor", target, false,
  false, -1, true)` probe is a pure presence check
  ([why](../engine/action-selection.md#casting)): conservative-correct mid-fight (Sunder and Expose
  share the same 20% armor slot), but wrong at the pull, where for the second before the warrior lands
  stack 1 the rogue sees a clean target, burns a finisher, and the warrior overwrites it — and the
  warrior's trigger has no reciprocal check, so both keep fighting for the slot.

  `GroupSuppliesMajorArmorDebuff` (`src/Ai/Base/Combat/ArmorDebuff.h`) walks the group with a **class
  check only** — alive warrior on the same map, or a hunter with a worm pet — no spellbook scan,
  amortised by a `checkInterval`. Stated trade-off: a warrior present but not actually sundering
  (levelling, wrong stance) suppresses the rogue. That costs 20% armor on the boss rather than a wasted
  finisher every pull, and the direct aura probe covers the case where someone else did land it.
- **Refresh windows, not expiry.** Every maintained aura (Slice and Dice, Rupture, Hunger for Blood)
  is refreshed before it drops via `beforeDuration = 2000`; 0 guaranteed downtime each cycle. **This is
  the opposite of the correct priest setting** — see [priest.md](priest.md).
- **Poisons per spec.** Assassination: Deadly main hand (falls back to Instant below level 30),
  Instant off hand. Combat and Subtlety: Instant main hand, Deadly off hand. Deadly procs on a flat
  per-hit chance (30%, up to 50% with Improved Poisons), Instant at 8.53 PPM. Core
  `spell_rog_deadly_poison` casts the **other** weapon's poison for every Deadly proc on a 5-stack
  target, skipped when that poison is Deadly too, so Deadly on both hands wastes every proc past 5.
  Triggers only fire on a weapon with no temp enchant: a bot keeps its old poison until it expires.
  Branching is by `GetPlayerSpecTab` in both generic strategies' `InitTriggers`; keep them in step.
- **Weapon speed** (`ApplyPreferredSpecWeapons`, daggers only for Assassination/Subtlety) follows the
  poisons: Assassination fast main hand (strong), slow off hand (weak); Subtlety slow main, fast off
  (weak); Combat slow main (weak), fast off hand for Combat Potency (strong).
- **Envenom needs only `HasSpell("envenom")`**, with the Deadly Poison requirement in the trigger: a
  `HasAura(58410)` (Master Poisoner rank 3) gate made any rogue without 3/3 fall through to Eviscerate.
- **Builders derive from `CastComboAction`** (`combo < 5` plus the melee range check), and
  `CastDebuffSpellAction` takes a trailing `beforeDuration` so Rupture can pass 2000.
- **Assassination's Rupture stands down when a group mate's bleed is on the target**
  (`GroupSuppliesBleedOn`): it is kept up mostly to feed Hunger for Blood, so with another bleed those
  combo points are worth more as an Envenom.

## Current ladders

Shared (`GenericRogueStrategy`): kick 42, kick on enemy healer 41, `low health` → evasion 29 / feint
28, `critical health` → cloak of shadows 27, tricks of the trade 26.5, main-hand poison 26 / off-hand
poison 25.5, `enemy out of melee` → sprint 22. Defaults: `melee` 5.0 only.

Combat (`CombatRogueStrategy`):

| Rel | Trigger | Action |
|---|---|---|
| 29.5 / 29.4 | in stealth | garrote / ambush |
| 25 | slice and dice | slice and dice |
| 24.5 | riposte | riposte |
| 24 | combat rupture | rupture |
| 23.5 | slice and dice before rupture | slice and dice |
| 23 / 22 | killing spree / blade flurry | killing spree / blade flurry |
| 21 | expose armor | expose armor |
| 20.5 / 20 | target with combo points almost dead, combat eviscerate | eviscerate |
| 17 | medium threat | vanish |
| 13 / 12 | combo points not full and medium energy / almost full energy available | sinister strike |

Assassination:

| Rel | Trigger | Action |
|---|---|---|
| 29.5 / 29.4 | in stealth | garrote / ambush |
| 25 / 24 / 23 | slice and dice / assassination rupture / hunger for blood | slice and dice / rupture / hunger for blood |
| 22 | expose armor | expose armor |
| 21.5 / 21 | envenom | cold blood, envenom |
| 20 | target with combo points almost dead | envenom |
| 15 | medium aoe | fan of knives |
| 13 | combo points not full and high energy | mutilate |

`boost` adds adrenaline rush 22 and blade flurry 24; `aoe` adds blade flurry 24 on `light aoe` and
fan of knives 15 on `medium aoe`.

Rupture sits below Slice and Dice because SnD comes first in the opener; Hunger for Blood sits below
Rupture because it depends on it. Feint stays on `low health` beside Evasion — with Glyph of Feint it
is a 50% AoE damage reduction, so it belongs in the survival band, not with the threat tools.
`killing spree` is off the default list, where at 5.1 it only ever fired when nothing else could.

**Combat's finishers follow the sim's APL** (`combat.apl.json`). Before, Rupture was up 77% against
the sim's 87%, every Rupture at 4 CP, and 4 of 5 drops came right after an Eviscerate left it 3-5.5 s:

- `combat rupture`: Rupture at 5 CP only (20 s with the glyph, stronger ticks).
- `combat eviscerate`: 5 CP, held while our Rupture has under 10 s (6 s under Adrenaline Rush), unless
  the target dies before Rupture's 12 s life check. Held at 5 CP, the bot pools to 90 energy, then
  Sinister Strikes (`almost full energy available` @12) and wastes the point, as the sim does.
- `slice and dice before rupture` (23.5, under Rupture): at 3+ CP, once SnD would expire before
  Rupture + 2 s and the refreshed SnD would outlast that, so SnD never takes Rupture's 5 CP. The
  second check stops an untalented short SnD from eating every 3 CP.
- Killing Spree and Adrenaline Rush wait for energy ≤ 50: regen past the cap is lost.

**Tricks of the Trade's target is still open** (`TricksOfTheTradeTargetValue`,
[pitfalls](../engine/pitfalls.md#detection-that-never-fires)): its opener branch runs only under
`wait for attack`, so in a raid it falls through to the highest-attack-power melee DPS, which then
pulls the boss.

## Deferred, with reasons

- **Vanish → Ambush for Overkill (Assassination).** Real DPS, but Vanish drops combat *and* the bot's
  target, and the stealth re-entry path (`CheckStealthAction` → `stealthed` strategy) is only
  reachable via chat command — `"stealth"` and `"stealthed"` are never auto-added by `AiFactory`. A
  bot that vanishes mid-boss can plausibly just stop attacking.
- Hemorrhage, Shadow Dance, Premeditation, Shiv — Subtlety abilities or PvE-irrelevant.
- Subtlety getting its own strategy — out of the agreed scope.

## Open against the guide

- Combat's Eviscerate fires only at 5 CP, and Assassination's Rupture from 4; the guide wants
  Eviscerate at 4-5 CP and Rupture at 5.
- Blade Flurry (24 via `boost`) outranks Killing Spree (23) and Adrenaline Rush (22); the guide orders
  them Adrenaline Rush → Blade Flurry → Killing Spree.

## Confirmed correct — do not re-audit

Cold Blood before Envenom, Envenom's Deadly Poison aura-state check, Expose Armor suppressed when the
group already supplies a major armor debuff, Slice and Dice.
