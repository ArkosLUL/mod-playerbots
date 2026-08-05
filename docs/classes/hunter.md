# Hunter — Beast Mastery, Marksmanship, Survival

Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Arcane Shot was cast zero times, by two independent regressions

Both lived in `CastArcaneShotAction::isUseful()` (`HunterActions.cpp:26`) and between them covered
every spec, which is why the meter showed no casts at all:

- **Inverted spec gate (killed BM and MM).** `if (!target || !botAI->HasSpell("explosive shot"))
  return false;` — Explosive Shot is the Survival 51-point talent, so every BM and MM bot bailed on
  every tick. The original commit's intent was the opposite: use Arcane Shot *until* Explosive Shot
  is learned. A `HasSpell`/`HasAura` refactor flipped the polarity while converting to the string
  overload; the two sibling conversions in the same hunk came out correct.
- **Armor-penetration read the wrong update field (killed Survival).**
  `GetUInt32Value(CR_ARMOR_PENETRATION)` — `CR_ARMOR_PENETRATION` is `24`, an index *into the rating
  block*, not an update-field index, and field 24 is `UNIT_FIELD_HEALTH`. So it read the bot's current
  hit points, orders of magnitude above the 435 threshold. Correct form is
  `GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_ARMOR_PENETRATION)`.

## Other fixes

- **Rapid Fire barely fired in raids.** `RapidFireTrigger : BoostTrigger` needs `balance <= 50`, so a
  3-minute 40%-haste cooldown went unspent for whole encounters. Re-keyed to
  `SpellNoCooldownTrigger`; `BurstWindowStrategy` now places the cast. Same treatment for
  **Bestial Wrath**, whose trigger was defined but never registered and only competed as a `19.0`
  default entry.
- **Pet death was unrecoverable mid-fight.** `HunterPetStrategy` (call/revive pet) is only added to
  the *non-combat* engine; the combat set carried only `mend pet`. BM lost ~35-40% of its throughput
  for the rest of the fight, and both Kill Command and Bestial Wrath went dead with the pet since
  both resolve `"pet target"`. `no pet` → `call pet` and `hunters pet dead` → `revive pet` now sit
  under every shot priority, so they only spend a GCD the rotation had nothing else for.
- **Kill Command checked an aura that never lands** — `IsActive` tested the KC aura on the *hunter*
  while the action resolves `"pet target"` and `CastBuffSpellAction::isUseful` then checks the aura on
  the *pet*, where Kill Command never puts one. It worked only because the cooldown paced it. Now a
  plain `SpellNoCooldownTrigger`.
- **Chimera Shot and Aimed Shot had no triggers**, only `5.5` / `5.4` defaults. With `no stings` at
  17.0 the bot hard-recast Serpent Sting instead of rolling it forward with Chimera Shot, which is the
  entire point of the spell.

## Current ladders

Shared (`GenericHunterStrategy`):

| Rel | Trigger | Action |
|---|---|---|
| 61 | tranquilizing shot enrage / magic | tranquilizing shot |
| 37 | enemy within melee | explosive trap *(see gap D2)* |
| 35 | low health / medium threat / enemy too close | deterrence / feign death / disengage *(D4)* |
| 34 | enemy too close for auto shot | flee |
| 30 / 29.5 | no ammo / hunter's mark | equip upgrades / hunter's mark |
| 29 | rapid fire off cooldown | rapid fire *(burst-gated)* |
| 28 | aspect of the viper | aspect of the viper |
| 27 | low tank threat / md on main tank and light aoe | misdirection on main tank |
| 22 / 21 | pet medium / low health, enemy within melee | mend pet, mongoose bite / wing clip |
| 20 | has aggro / concussive shot on snare target | concussive shot *(D1)* |
| 17 | trap launcher: explosive trap no cd | trap launcher: explosive trap |
| 14.5 / 14 | no pet / pet dead | call pet / revive pet |

Per spec, on top:

| Rel | Beast Mastery | Marksmanship | Survival |
|---|---|---|---|
| 40 | intimidation | silencing shot | — |
| 28.5 | bestial wrath *(burst-gated)* | — | — |
| 28 | — | — | lock and load → explosive shot rank 4 |
| 18.5 / 18 | kill command / kill shot | kill command / kill shot | kill command / kill shot |
| 17.5 | low mana → viper sting | low mana → viper sting | explosive shot |
| 17 | no stings → serpent sting | chimera shot | — |
| 16.5 | serpent sting on attacker | no stings → serpent sting | black arrow |
| 16 | — | serpent sting on attacker | low mana → viper sting |
| 15.5 / 15 | — | aimed shot | no stings → serpent sting, serpent sting on attacker |
| 5.7 → 5.1 | *defaults* | *defaults* | *defaults* |

Survival's Explosive Shot handling and the Trap Launcher node were correct and untouched; the only
Survival change was dropping the now-unreachable `arcane shot` default.

## Open gaps — documented, not implemented

| # | Gap |
|---|---|
| D1 | **Concussive Shot on aggro wastes a GCD on bosses.** `has aggro` → concussive shot @20; bosses are snare-immune, so the cast burns a GCD every tick the hunter holds aggro. Fix is gating the trigger to non-boss targets. |
| D2 | **Melee weave outranks the whole shot list.** `enemy within melee` pushes explosive trap @37, mongoose bite @22, wing clip @21. At 37 the trap beats everything except dispels, and melee weaving is a straight DPS loss for a ranged spec in WotLK. |
| D3 | **Aspect of the Viper hysteresis is far too wide** — enters at `mana < lowMana/2` = 7.5% and only leaves at `mana >= 60%`, a long stretch at half ranged damage. ~35% is the obvious exit. |
| D4 | **Disengage is raid-hostile.** Fixed-vector backwards leap at @35; on Thorim / Mimiron / Gunship-style encounters it lands bots in fire, off platforms, or out of healer range. Should be suppressed in raid instances in favour of the existing `flee` alternative. |
| D5 | `trap launcher: explosive trap` @17 sits mid-rotation, between Survival's explosive shot (17.5) and black arrow (16.5). Left alone because Survival's trap usage is what the user wanted preserved. |
| D6 | `ExplosiveShotTrigger` overrides `DebuffTrigger` to `BuffTrigger::IsActive()`, so it re-pushes on debuff-absence while the 6s cooldown still has the spell unavailable. Harmless — the action just fails — but it costs a queue iteration. |
| D7 | **OPEN — `"freezing trap on cc"` has no action creator.** `GenericHunterStrategy.cpp:118` pushes `NextAction("freezing trap on cc", …)`, but no creator registers that name. The node logs `A:freezing trap on cc - UNKNOWN` and hunter bots never trap a CC target. |

Against the wowtbc.gg guides, also open: BM and Survival default lists both carry `aimed shot`, a
Marksmanship talent neither spec has (dead entries); neither BM nor Survival has Multi-Shot in its
single-target priority (only `AoEHunterStrategy` on `light aoe`); Survival's Sniper Training (6s
stationary) is not modelled.

## Confirmed correct — do not re-audit

`medium threat` → feign death @35 and `low health` → deterrence @35 keep their current relevance and
triggers by explicit decision. Aspect of the Dragonhawk via `bdps`, Hunter's Mark, Kill Shot at 20%,
Chimera Shot ranked above the hard Serpent Sting recast, the Lock and Load rank 4→3→2 chain,
trap-launcher Explosive Trap, Misdirection on the main tank, and Volley/Multi-Shot on AoE are all
correct as written.
