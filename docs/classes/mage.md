# Mage — Fire, Frostfire, Arcane

Audit against the wowtbc.gg WotLK guides. Numbered findings without *Removed* or *Fixed* are still
live. Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Current ladders

- **Fire** (`FireMageStrategy`): scorch on improved scorch 19.0, living bomb 18.5, pyroblast on hot
  streak 18.25 — maintenance first, as the guide ranks it: Hot Streak lasts 10 s and survives 2-3 GCDs,
  a dropped Living Bomb tick does not come back. Close range: dragon's breath 41.0 on
  `enemy too close for spell`, blast wave 40.0 on `enemy is close`. Defaults fireball 5.3 / frostbolt
  5.2 / fire blast 5.1 / shoot 5.0.
- **Frostfire** (`FrostFireMageStrategy`) inherits Fire's triggers and changes only the defaults:
  frostfire bolt 5.2 / fire blast 5.1 / shoot 5.0. Every Fire finding applies to it.
- **Arcane** (`ArcaneMageStrategy`): evocation 29.8 on `arcane evocation`; arcane missiles 15.5
  on `arcane blast conserve stack and medium mana`, 15.0 on
  `arcane blast 4 stacks and missile barrage`; defaults arcane blast 5.6 / arcane missiles 5.5 /
  arcane barrage 5.4 / fire blast 5.3 / frostbolt 5.2 / shoot 5.1.
- **Shared** (`GenericMageStrategy`): ice block / wards / mana gem / evocation 90.0,
  mana shield 85.0, frost nova 50.0, remove curse 41.0 / on party 40.0, spellsteal and
  counterspell-on-healer 40.0, blink back 35.0, invisibility and polymorph 30.0.
- **Boost**: arcane — presence of mind 29.5 / arcane power 29.0 / icy veins 28.5 / mirror image 28.0;
  fire — combustion 18.0 / mirror image 17.5; frostfire — combustion 18.0 / icy veins 17.5 / mirror
  image 17.0; frost — cold snap 28.0 / icy veins 27.5 / mirror image 26.0.
- **AoE**: fire — firestarter → flamestrike 40.0, living bomb on attackers 39.5, dragon's breath 39.0 /
  blast wave 38.0 (F6); every spec — flamestrike active → blizzard 24.0, flamestrike 23.0 / blizzard
  22.0; frost — cone of cold 21.0 on `light aoe`. The Living Bomb spread stands down above 5 attackers
  so big pulls fall to Flamestrike/Blizzard. Both Living Bomb triggers are `DebuffTrigger`s with a
  12 s `needLifeTime` (the fuse), so it skips targets that die before the explosion.

Mage strategies mostly use bare floats; evocation, presence of mind, dragon's breath and blast wave
use `ACTION_*` constants, so grep both forms.

## Fire

| # | Finding |
|---|---|
| F1 | *Removed.* `"high threat"` → mirror image 60.0 named a trigger registered nowhere, so it never fired and failed pblint. Mirror Image stays reachable via `boost`. Registering a real `high threat` trigger would be a behaviour change and is undecided. |
| F3 | **Combustion has no window logic.** `CombustionTrigger : BoostTrigger` fires the moment the aura is absent and `balance <= 50`; the only gate is the burst tank-hold. The guide wants it aligned with Bloodlust and kept up for the sub-35% Molten Fury window. Result: burned at the pull, unavailable during execute. A fix needs a mage-local trigger also requiring lust on self **or** target below 35% **or** a staleness fallback so it is never held forever. |
| F6 | **Melee AoE nodes sit in the ranged block.** `dragon's breath` 39.0 and `blast wave` 38.0 on `medium aoe` sit in the fire AoE list, but their `isUseful()` needs ≤10 yd, so at raid range they always fail through. Harmless but misleading — and `FirestarterStrategy`, which exists to close that gap, is opt-in and off by default. |

## Arcane

| # | Finding |
|---|---|
| A1 | Presence of mind 29.5 heads the arcane boost list; it cannot be cast while Arcane Power runs. `PresenceOfMindTrigger` adds two guards. **Arcane Power off cooldown:** the two share category 1151, the Presence of Mind aura is permanent until a cast-time spell spends it and locks Arcane Power meanwhile, and spending it writes a 1.5 s category cooldown over Arcane Power's entry — by code reading (unverified in game) an ungated bot would alternate the two about every 17 s. **Arcane Blast known:** below 64 the rotation has no cast-time spell (Missiles is a channel), so the aura would never be spent and Arcane Power would stay locked. |
| A2 | **Conserve stack dump** (the guide's variant): below `mediumMana` (40%) the 15.5 node spends 3 Arcane Blast stacks through Missiles. With only the 4-stack Missile Barrage node (a 40% proc), the bot held 4 stacks — +175% mana per Arcane Blast — for unbounded casts, and `"low mana"` fires Evocation only below 15%, so arcane bots ran dry in long fights. |
| A3 | `arcane evocation` → evocation 29.8, Arcane only; the shared 15% node stays as the floor. It fires below `mediumMana` (40%, the conserve threshold) when Evocation is off cooldown, no burst aura is up (Presence of Mind, Arcane Power, Icy Veins, Bloodlust, Heroism), the Arcane Blast stack is below 3 (the conserve node spends a held stack through Missiles first) and the target's time-to-die is at least 20 s. 29.8 sits above the boost band so the bot refills before it burns. |
| A4 | **AoE has no Presence of Mind + Blizzard.** The guide leads with an instant Blizzard under PoM for the Arcane Potency crit. |
| A6 | **Not a defect — do not hold Arcane Barrage to keep the stack.** A Barrage cast while moving still gets the full stack bonus (about 1.3–1.4 Arcane Blasts) and rebuilding the stack loses about 1.5–1.8, so holding gains well under one cast per move while the rebuild casts are far cheaper in mana. Fire Blast (20 yd) cannot substitute at caster range. |

## Shared

| # | Finding |
|---|---|
| S1 | The gem node is chosen by the highest *Conjure* rank the bot knows, but the bags may hold an older gem, so `GenericMageStrategy` chains sapphire → emerald → ruby → citrine → jade → agate through `ActionNode` alternatives. The "not in bags" test sits in `isPossible`, **not** `isUseful` ([why](../engine/action-selection.md#the-selection-loop)). The gem fires on `"high mana"`, which tests mana **< 65%** ([why](../engine/action-selection.md#healing)): do not "fix" it backwards. |
| S2 | Mages carry **no mana potions** ([../systems/consumables-and-burst.md](../systems/consumables-and-burst.md)) from level 20. The gem already fires at 65% and Evocation at 15%, both at 90.0, so the potion was pure redundancy that cost the mage its Potion of Wild Magic. |
| S3 | **Registered but unreferenced.** Triggers `fireball`, `pyroblast`, `frostfire bolt`, `ice barrier`, `counterspell`; actions `counterspell`, `conjure food`, `conjure water`. Wire `counterspell` (a raid-PvE interrupt node — only `counterspell on enemy healer` is used today, at 40.0); delete the rest via the two-site `creators[...]` + factory pattern. |
| S4 | **Focus Magic is out-of-combat only.** If the focus target dies mid-fight the buff is gone for the rest of the encounter. Low impact. |
| S5 | **Spellsteal fires on any dispellable positive magic aura** on the current target (`TargetAuraDispelTrigger`), so a long self-buff like Thorns keeps the mage stealing every GCD. Shaman Purge behaves the same. |
| S6 | **No execute swap.** `CastTimeStrategy`'s ×0.1 does not reorder the ladder ([../engine/action-selection.md](../engine/action-selection.md#casting)), so the cast-time filler still goes out on a dying target. The guide's "Fire Blast if the target dies before your next cast finishes" is unimplemented for every spec. |

## Divergences to decide

| Topic | Code | Guide |
|---|---|---|
| Fire close-range block | `FireMageStrategy` pushes `dragon's breath` at 41.0 on `enemy too close for spell` and `blast wave` at 40.0 on `enemy is close` | Guide treats both as AoE-rotation cooldowns. In raid PvE a self-defence disorient can break CC, move mobs out of the tank's pile, and spend a cooldown the AoE rotation wants |
| Firestarter | Opt-in, off by default; suppresses frost nova and blink-back kiting when on | The guide's AoE ladder assumes Blast Wave and Dragon's Breath are usable, i.e. melee range |
| Frostbolt in the Fire/Arcane default lists | Present at 5.2 for immune targets | Not in the guide; keeping it is a bot-specific robustness call |

## Confirmed correct — do not re-audit

- Movement handling: cast-time spells are refused while moving, so the ladder falls through
  to the instants already sitting below them.
- **`ImprovedScorchTrigger` is the model for other debuff triggers** — it correctly stands down when
  the target already carries Shadow Mastery (17794-17800), Winter's Chill (12579) or another mage's
  Improved Scorch (22959).
- Firestarter proc → free instant Flamestrike at 40.0, above the whole fire AoE block.
- `blizzard channel check` → `cancel channel` 26.0 correctly stops a Blizzard when attackers drop
  below two.
- `HasAuraStackTrigger("arcane blast", 4)` is an **at least 4** test on self, matching the guide.
- Molten Armor for fire and arcane, Mage Armor for frost, via `bdps` / `bmana` at 19.0, in the combat
  and non-combat engines alike, so a lost armor is re-buffed mid-fight.
- Potions and trinkets need no mage node — the generic `"potions"` strategy plus the burst window
  already hold them to the tank-engaged window.
