# Rogue — Combat and Assassination

Subtlety aliases the Assassination strategy (`AiFactory.cpp:379-380`) and inherits whatever improves
there. Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

**`GenericRogueStrategy` must stay parented to `MeleeCombatStrategy`.** Upstream parents it to
`CombatStrategy`, and `MeleeCombatStrategy` is the only source of the `"enemy out of melee"` trigger
that produces `reach melee`; `AiFactory.cpp` never hands rogues the `"close"` strategy either. Take
upstream's parent and **every rogue stands still whenever Sprint is on cooldown**, in every raid, with
nothing failing to compile. It is a local divergence to re-apply on each merge — see
[../engine/pitfalls.md](../engine/pitfalls.md). `tricks of the trade` likewise sits at
`ACTION_HIGH + 6.5f` rather than upstream's 26.0, to break an exact relevance tie with
`use instant poison on main hand`.

## Design decisions

- **Armor-debuff ownership.** A rogue applies Expose Armor only when **nobody else in the group can**,
  not merely when the debuff currently happens to be missing. The original trigger used
  `HasAura("sunder armor", target, false, false, -1, true)`, and with `maxStack = false,
  maxAuraAmount = -1` that is a **pure presence check** — one stack blocks Expose Armor exactly as
  hard as five. Presence-only is conservative-correct mid-fight (Sunder and Expose share the same 20%
  armor slot), but wrong at the pull: for the second before the warrior lands stack 1 the rogue sees a
  clean target, burns a finisher, and the warrior overwrites it — and the warrior's trigger has no
  reciprocal check, so both keep fighting for the slot.

  `GroupSuppliesMajorArmorDebuff` (`src/Ai/Base/Combat/ArmorDebuff.h`, sitting beside
  `BurstCooldowns.cpp`) walks the group with a **class check only** — alive warrior on the same map,
  or a hunter with a worm pet — no spellbook scan, amortised by a `checkInterval`. Stated trade-off:
  a warrior present but not actually sundering (levelling, wrong stance) suppresses the rogue. That
  costs 20% armor on the boss rather than a wasted finisher every pull, and the direct aura probe
  covers the case where someone else did land it.
- **Refresh windows, not expiry.** Every maintained aura (Slice and Dice, Rupture, Hunger for Blood)
  is refreshed before it drops via `beforeDuration = 2000`. Rogue passing 0 everywhere guaranteed
  downtime each cycle. **This is the opposite of the correct priest setting** — see
  [priest.md](priest.md).
- **Reuse over new plumbing.** `MediumEnergyAvailableTrigger`, `ComboPointsNotFullTrigger`,
  `CastComboAction`, `RuptureTrigger`, `BoostTrigger` and the `burstCooldownNames` registry all
  already existed and were either unused or unreachable from rogue.

## Bugs the rebuild fixed

- **`blade fury` was a doubly dead name.** The strategy asked for `"blade flurry"` while the registry
  had `"blade fury"` — and no spell by that name exists either. Renamed throughout.
- **Envenom was gated on a talent rank.** `isPossible` checked `HasAura(58410)` (Master Poisoner rank
  3), so any Assassination rogue without 3/3 silently never envenomed and fell through the action node
  to Eviscerate. Now `HasSpell("envenom")`, with the Deadly Poison requirement moved to the trigger.
- **Builders had no combo-point guard.** Sinister Strike, Mutilate and Backstab derived from
  `CastSpellAction`; they now derive from `CastComboAction`, which implements `combo < 5` plus the
  melee range check and was previously dead code with nothing inheriting from it.
- **`CastDebuffSpellAction` swallowed `beforeDuration`** — it forwarded only `isOwner` to
  `CastAuraSpellAction`. An optional trailing parameter was added (non-breaking for the existing call
  sites) so Rupture can pass 2000.
- **Sinister Strike idled through the 40-59 energy band** every cycle, because the only gate was
  `high energy available` (60) while SS costs 40.
- **Assassination never cast Rupture at all** — the action node existed but nothing emitted it. That
  also blocked Hunger for Blood, which needs a bleed on the target and could otherwise only get one
  from a stealth-opener Garrote.

## Current ladders

Combat (`DpsRogueStrategy`):

| Rel | Trigger | Action |
|---|---|---|
| 42 / 41 | kick, kick on enemy healer | kick |
| 29 / 28 | low health | evasion, feint |
| 27 | critical health | cloak of shadows |
| 26 | low tank threat | tricks of the trade |
| 25 / 24 | slice and dice / rupture | slice and dice / rupture |
| 23 / 22 | killing spree / blade flurry | killing spree / blade flurry |
| 21 | expose armor | expose armor |
| 20.5 / 20 | target with combo points almost dead, combo points 5 available | eviscerate |
| 19 / 17 | enemy out of melee / medium threat | sprint / vanish |
| 13 | combo points not full and medium energy | sinister strike |

Assassination:

| Rel | Trigger | Action |
|---|---|---|
| 42 / 41 | kick, kick on enemy healer | kick |
| 29 / 28 | low health | evasion, feint |
| 27 | critical health | cloak of shadows |
| 26 | low tank threat | tricks of the trade |
| 25 / 24 / 23 | slice and dice / rupture / hunger for blood | same |
| 22 | expose armor | expose armor |
| 21.5 / 21 | envenom | cold blood, envenom |
| 20 | target with combo points almost dead | envenom |
| 19 / 15 | enemy out of melee / medium aoe | sprint / fan of knives |
| 13 | combo points not full and high energy | mutilate |

Rupture sits below Slice and Dice because SnD comes first in the opener; Hunger for Blood sits below
Rupture because it depends on it. Feint stays on `low health` beside Evasion — with Glyph of Feint it
is a 50% AoE damage reduction, so it belongs in the survival band, not with the threat tools.
`killing spree` moved off the default list, where at 5.1 it only ever fired when nothing else could;
`getDefaultActions` keeps only `melee` at 5.0.

`TricksOfTheTradeTargetValue` returns the main tank while `low tank threat` holds or in the first
~10s of combat, otherwise the highest-attack-power melee DPS group member within 20 yd excluding self.

## Deferred, with reasons

- **Vanish → Ambush for Overkill (Assassination).** Real DPS, but Vanish drops combat *and* the bot's
  target, and the stealth re-entry path (`CheckStealthAction` → `stealthed` strategy) is only
  reachable via chat command — `"stealth"` and `"stealthed"` are never auto-added by `AiFactory`. A
  bot that vanishes mid-boss can plausibly just stop attacking.
- Hemorrhage, Shadow Dance, Premeditation, Shiv — Subtlety abilities or PvE-irrelevant.
- Subtlety getting its own strategy — out of the agreed scope.

## Open against the guide

- Eviscerate fires only on `combo points 5 available` while Rupture fires from 4; the guide wants
  Eviscerate at 4-5 CP and Rupture at 5.
- Killing Spree (23) outranks Blade Flurry (22) and Adrenaline Rush (22); the guide orders them
  Adrenaline Rush → Blade Flurry → Killing Spree.
- **Rupture always maintained above Hunger for Blood** is a deliberate divergence: it guarantees the
  bleed HfB needs, but spends 5 CP that would otherwise be Envenom. The guide-accurate version would
  be a group-bleed check shaped like `GroupSuppliesMajorArmorDebuff`.

## Confirmed correct — do not re-audit

Cold Blood before Envenom, Envenom's Deadly Poison aura-state check, Expose Armor suppressed when the
group already supplies a major armor debuff, Slice and Dice, Tricks of the Trade.
