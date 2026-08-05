# Mage — Fire, Frostfire, Arcane

Audit against the wowtbc.gg WotLK guides, most of which shipped in commit `81bca7c10` ("Mage
rotation improvements based on audit"). The findings below are the audit record; **items marked
OPEN were re-verified against `src/` and are still live**. Engine semantics are in
[../engine/action-selection.md](../engine/action-selection.md).

Fire findings apply to Frostfire too — see F7.

## Current ladders

- **Fire** (`FireMageStrategy.cpp:17-58`): pyroblast on hot streak 25.0, scorch on improved scorch
  19.0, living bomb 18.5; defaults fireball 5.3 / frostbolt 5.2 / fire blast 5.1 / shoot 5.0.
- **Arcane** (`ArcaneMageStrategy.cpp:38-63`): arcane missiles 15.0 on
  `arcane blast 4 stacks and missile barrage`; defaults arcane blast 5.6 / arcane missiles 5.5 /
  arcane barrage 5.4 / fire blast 5.3 / frostbolt 5.2 / shoot 5.1.
- **Shared** (`GenericMageStrategy.cpp:91-127`): ice block / wards / mana gem / evocation 90.0,
  mana shield 85.0, mirror image on `high threat` 60.0, frost nova 50.0, spellsteal and
  counterspell-on-healer 40.0, blink back 35.0, invisibility 30.0.
- **Boost** (`:135-166`): arcane — arcane power 29.0 / icy veins 28.5 / mirror image 28.0; fire —
  combustion 18.0 / mirror image 17.5; frostfire — combustion 18.0 / icy veins 17.5 / mirror image
  17.0.

Mage strategy files use **bare floats, not `ACTION_*` constants**, unlike the other classes.

## Fire

| # | Finding |
|---|---|
| F1 | **OPEN — dead node.** `"high threat"` → mirror image 60.0 (`GenericMageStrategy.cpp:143`). `"high threat"` is registered nowhere — `TriggerContext.h` has only `"medium threat"`, and `MageTriggerFactoryInternal` does not add it. Affects all three specs. Mirror Image is still reachable via `boost`. |
| F2 | **Hot Streak outranks maintenance.** Pyroblast 25.0 sits above improved scorch 19.0 and living bomb 18.5; the guide ranks both maintenance spells higher. Hot Streak lasts 10s and survives 2-3 GCDs; a dropped Living Bomb tick does not come back. |
| F3 | **Combustion has no window logic.** `CombustionTrigger : BoostTrigger` fires the moment the aura is absent and `balance <= 50`; the only gate is the burst tank-hold. The guide wants it aligned with Bloodlust and kept up for the sub-35% Molten Fury window. Result: burned at the pull, unavailable during execute. A fix needs a mage-local trigger also requiring lust on self **or** target below 35% **or** a staleness fallback so it is never held forever. |
| F4 | **Living Bomb lost its lifetime guard.** `LivingBombTrigger` / `LivingBombOnAttackersTrigger` override `IsActive()` to call `BuffTrigger::IsActive()`. That correctly implements "let it expire before re-applying", but **drops `DebuffTrigger`'s `needLifeTime` check**, so Living Bomb goes on mobs that die before the explosion — the guide's explicit caveat. `CastTimeStrategy` does not cover this: Living Bomb is instant. |
| F5 | **AoE ranks the Living Bomb spread last** — 21.0, below flamestrike 23.0 and blizzard 22.0; the guide puts the spread second. |
| F6 | **Melee AoE nodes sit in the ranged block.** `dragon's breath` 39.0 and `blast wave` 38.0 head the fire AoE list, but their `isUseful()` needs ≤10 yd, so at raid range they always fail through. Harmless but misleading — and `FirestarterStrategy`, which exists to close that gap, is opt-in and off by default. |
| F7 | **Frostfire is a copy-paste fork.** `FrostFireMageStrategy::InitTriggers` is byte-identical to Fire's, differing only in `getDefaultActions()`. **Every Fire finding must be applied twice or the two drift.** |
| F8 | **No in-combat Molten Armor** — pushed only out of combat by `bdps` at 19.0, while the guide has it as priority #1. A mage that loses it mid-fight never re-buffs. Low impact. |

## Arcane

| # | Finding |
|---|---|
| A1 | **Presence of Mind is unreachable.** Trigger and action are both registered, and `"presence of mind"` is already in `burstCooldownNames`, but **no strategy references it**. It is the guide's #2 arcane priority. A node at ~29.5 (above arcane power 29.0) would let the default `arcane blast` 5.6 consume it on the next GCD. |
| A2 | *Shipped* — a second `arcane missiles` node at 15.5 now exists. **No mana-aware stack dump.** The only arcane rotation node is `arcane blast 4 stacks and missile barrage` → arcane missiles 15.0. Missile Barrage is a 40% proc, so the bot holds 4 stacks — at +175% mana cost per Arcane Blast — for an unbounded number of casts, and `"low mana"` only fires Evocation below 15%. Arcane bots run dry in long fights. The guide has an explicit conserve variant: dump after 3. |
| A3 | **Evocation fires too late for Arcane.** `"low mana"` is 15%; at 15% an arcane mage cannot pay for a stacked Arcane Blast and burns GCDs. ~25% is the practical number — but this is a tuning call, not a defect. |
| A4 | **AoE has no Presence of Mind + Blizzard.** The guide leads with an instant Blizzard under PoM for the Arcane Potency crit. |
| A5 | **`CastArcaneBlastAction` is the wrong base class.** It is a `CastBuffSpellAction` with `GetTargetName()` overridden to `"current target"`, so its inherited `isUseful()` hunts for an "arcane blast" aura on the *enemy*. That aura never exists, so it always returns true and **the action works by accident**. Cleanup only, no behaviour change. |

## Shared

| # | Finding |
|---|---|
| S1 | **Mana gem has no fallback chain.** The gem node is chosen by the highest *Conjure* rank the bot knows, but each `UseMana*Action::isUseful()` requires that exact item in bags. A level-80 mage carrying a Ruby but no Sapphire uses nothing. Wants an `ActionNode` alternatives chain down the ranks, same shape as `CastMoltenArmorAction::getAlternatives()`. |
| S2 | `"high mana"` is a misnomer — it tests `mana < 65%`. Noted so nobody "fixes" the gem node backwards. |
| S3 | **Registered but unreferenced.** Triggers `fireball`, `pyroblast`, `frostfire bolt`, `arcane blast`, `presence of mind`, `ice barrier`, `counterspell`; actions `presence of mind`, `counterspell`, `conjure food`, `conjure water`. Wire `presence of mind` (A1) and `counterspell` (a raid-PvE interrupt node — only `counterspell on enemy healer` is used today, at 40.0); delete the rest via the two-site `creators[...]` + factory pattern. |
| S4 | **Focus Magic is out-of-combat only.** If the focus target dies mid-fight the buff is gone for the rest of the encounter. Low impact. |

## Divergences to decide

| Topic | Code | Guide |
|---|---|---|
| Fire CC block | `MageCcStrategy` pushes `dragon's breath` at 41.0 and `blast wave` at 40.0 on "enemy close", fire only | Guide treats both as AoE-rotation cooldowns. In raid PvE a self-defence disorient can break CC, move mobs out of the tank's pile, and spend a cooldown the AoE rotation wants |
| Arcane evocation threshold | Shared 15% | Guide says only "when you run out of mana"; ~25% is the practical arcane number |
| Firestarter | Opt-in, off by default; suppresses frost nova and blink-back kiting when on | The guide's AoE ladder assumes Blast Wave and Dragon's Breath are usable, i.e. melee range |
| Frostbolt in the Fire/Arcane default lists | Present at 5.2 for immune targets | Not in the guide; keeping it is a bot-specific robustness call |

## Confirmed correct — do not re-audit

- **The execute swap to Fire Blast works without an explicit node**: `CastTimeStrategy` demotes the
  cast-time filler by ×0.1 (fireball 5.3 → 0.53) so instant `fire blast` wins. Covers the guide's
  "Fire Blast if the target dies before your next cast finishes" for both specs.
- Movement handling likewise: cast-time spells are refused while moving, so the ladder falls through
  to the instants already sitting below them.
- **`ImprovedScorchTrigger` is the model for other debuff triggers** — it correctly stands down when
  the target already carries Shadow Mastery (17794-17800), Winter's Chill (12579) or another mage's
  Improved Scorch (22959).
- Firestarter proc → free instant Flamestrike at 40.0, above the whole fire AoE block.
- `blizzard channel check` → `cancel channel` 26.0 correctly stops a Blizzard when attackers drop
  below two.
- `HasAuraStackTrigger("arcane blast", 4)` is an **at least 4** test on self, matching the guide.
- Molten Armor for fire and arcane, Mage Armor for frost, via `bdps` / `bmana`.
- Potions and trinkets need no mage node — the generic `"potions"` strategy plus the burst window
  already hold them to the tank-engaged window.
