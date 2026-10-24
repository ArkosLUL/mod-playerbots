# Warlock — Affliction, Demonology, Destruction

Audit against the wowtbc.gg WotLK guides. Numbered findings marked *Fixed* or *Removed* record a
decision; the rest are still live. Engine semantics are in
[../engine/action-selection.md](../engine/action-selection.md).

Warlock strategy files use bare floats, not `ACTION_*` constants.

## Corruption snapshot — how it actually works on this core

The guide describes retail-Classic behaviour. **This core does something different, and the
difference changes the recommendation.**

`spell_warl_everlasting_affliction` finds the caster's own Corruption `AuraEffect` and calls
`RefreshTimersWithMods()` + `ChangeAmount(CalculateAmount(...))`. `Aura::RefreshTimersWithMods()`
recomputes `m_maxDuration` against **current** `UNIT_MOD_CAST_SPEED` and calls `CalculatePeriodic()`
per effect — but it does **not** call `CalculatePeriodicData()`.

`AuraEffect::CalculatePeriodicData()` is the only thing that sets `m_pctMods` (from
`SpellPctDamageModsDone`) and `m_critChance` (from `CalcPeriodicCritChance`). It runs on aura
application and from `Aura::RefreshTimers()` — the path a **manual recast** takes, not the Everlasting
Affliction path. Periodic ticks then roll crit off the stored value.

| Component | Snapshot at cast? | Refreshed by Everlasting Affliction? |
|---|---|---|
| Haste (duration + tick amplitude) | yes | **yes** |
| Base amount / spell power | yes | **yes** |
| Death's Embrace (< 35% target HP) | evaluated inside `SpellDamageBonusDone` | **yes** — re-run by `CalculateAmount` |
| `m_pctMods` (`SpellPctDamageModsDone`) | yes | **no** |
| `m_critChance` (Pandemic) | yes | **no** |

Two consequences:

1. **The guide's "manually re-apply Corruption at 35% for Death's Embrace" rule buys nothing here** —
   Death's Embrace is picked up by every Everlasting Affliction refresh. Do not implement it.
2. **The real gap was crit and % damage mods.** Corruption at 18.0 is Affliction's opener — applied
   cold, before any trinket proc, potion or raid cooldown — and with Everlasting Affliction rolling it
   forward the debuff never drops, so `CorruptionTrigger` (presence-only) never fires again and the
   cold snapshot would last the whole encounter.

`corruption snapshot` → `corruption resnapshot` @16.5, under the DoT maintenance block so it never
delays Haunt or an actually-missing DoT. `CorruptionSnapshotTrigger` (checked every 2 s) compares the
stored `AuraEffect::GetCritChance()` against live `Unit::SpellDoneCritChance` /
`SpellTakenCritChance` and fires once live leads by `CRIT_DELTA_PCT` (5). Properties:

- **It self-gates on Pandemic.** `CalcPeriodicCritChance` returns `0.0f` unless a
  `SPELL_AURA_ABILITY_PERIODIC_CRIT` aura affects the spell, so an untalented bot never trips it.
- **It self-clears.** The recast runs `RefreshTimers()` → `CalculatePeriodicData()`, the snapshot
  becomes live, the trigger goes quiet. No cooldown or one-shot latch needed.
- **It needs its own action.** `CastCorruptionAction::isUseful()` refuses while Corruption is up, so
  `CastCorruptionResnapshotAction` is a plain `CastSpellAction` that skips the aura-presence veto (and
  waits out a pending Seed).
- `m_pctMods` staleness has the same cause and is fixed by the same recast — no separate trigger.

On Ulduar traces the trigger fires once per pull, 0-6 s after the crit-taken debuffs the opener
missed reach the boss (Shadow Mastery, Improved Scorch, Heart of the Crusader, Totem of Wrath, Master
Poisoner), then stays quiet; Everlasting Affliction carries that snapshot for the rest of the uptime,
114 s on one Thorim pull. Tick crit rate cannot show the gain: 5 points needs far more ticks than a
pull yields.

## Seed of Corruption stays off bosses

The bot's own Seed landing removes its own Corruption on that target; another warlock's Seed does
not. With the `aoe` ladder's Seeds aimed at the boss (`attacker without aura` and the current target),
one Ulduar run lost 38 Corruptions, 219 s of boss uptime, 70-86 s per Freya pull, and on XT-002 looped
Corruption casts into Seeds still in flight.

Both Seed actions go through `seed of corruption target`: never a boss, preferring an add
without own Corruption, falling back to Corrupted trash, else no Seed. Never a boss even when it
lacks Corruption, or after an expiry Seed (22.0) outranks the Corruption recast (19.5) and blocks it
until the Seed pops. Consequence: no Seed on Iron Assembly, all three being bosses. A Seed cast at a
target in the last 3 s (`last spell cast`, whole seconds) counts as already there for every
Corruption action and both Seed actions.

## Framework notes specific to warlock

- **Every warlock DoT action overrides `isUseful()` to call `CastAuraSpellAction::isUseful()`**, which
  returns false while the aura is present. A trigger that wants a cast on an already-buffed target is
  vetoed by the action unless the action changes too. Same trigger/action duplication as warrior
  Sunder.
- `AttackerWithoutAuraTargetValue` filters on **aura presence only**, so `DebuffOnAttackerTrigger`
  cannot see remaining duration even with `beforeDuration` set — the value returns `nullptr` for a
  target that still has the about-to-expire DoT. Multi-target duration-aware refresh needs a new
  value class. It also returns the **highest-health** match, so on a boss fight, the boss.
- `ValueContext` exposes no aura-duration value at all; direct access is `PlayerbotAI::GetAura(...)`.
- `"metamorphosis"` is the **only** warlock entry in `burstCooldownNames`, so `BurstWindowStrategy`
  paces it and nothing else.

## Life Tap is the whole mana bar

Warlocks carry no mana potions ([../systems/consumables-and-burst.md](../systems/consumables-and-burst.md)),
so Life Tap covers the bar rather than just its floor. Four nodes, shared across all three specs
except `tank` (SH2):

| Trigger | Rel | Why there |
|---|---|---|
| `medium mana` (< 40%) | 30.0 | above every spec rotation (Demonology tops at 28.5) and the glyph node |
| `low mana` (< 15%) | 39.5 | under `spell lock` 40.0 and `flee` 39.0 — an interrupt beats one tap |
| `life tap glyph buff` | 29.5 | spellpower upkeep; mana is the side effect |
| `life tap` filler | 5.1 | idle globals only; loses every contested tick |

**Tapping from 40% rather than 15% is the point, not a tuning preference.** `LifeTapTrigger` and
`CastLifeTapAction::isUseful` both refuse below `lowHealth` (45), so on a fight with heavy raid-wide
damage — Iron Assembly's High Voltage pulses every 3 s — a warlock that waits for 15% mana is likely
too hurt to tap at all. The emergency node exists for when that happens anyway.

**Fel Armor pays for the taps**: 2% health per 5 s (2.6% with Demonic Aegis) covers about one tap
per 15 s. Demon Armor is only for warlocks without Fel Armor (`CastDemonArmorAction::isUseful`), or
the seated-bot alternative put it on every pull
([pitfalls](../engine/pitfalls.md#a-seated-bots-first-cast-hands-the-tick-to-the-alternative)).

## Affliction

Code order: corruption on attacker 19.5, UA on attacker 19.0, corruption 18.0, haunt 17.75, UA 17.5,
corruption snapshot → corruption resnapshot 16.5, shadow trance → shadow bolt 16.0, drain soul
execute → drain soul 15.5; life tap glyph 29.5, life tap 5.1, flee 39.0. Defaults: corruption 5.5,
UA 5.4, haunt 5.3, shadow bolt 5.2, shoot 5.0. Curse of Agony comes from the separate
`curse of agony` strategy (18.5 on-attacker / 17.0 single).

| # | Finding |
|---|---|
| AF2 | *Fixed.* `HauntTrigger` fires on cooldown (~8 s), not when the debuff drops (~12 s), keeping the free Corruption refresh Everlasting Affliction hangs off it; it keeps `DebuffTrigger`'s target guards and an 8.0 s time-to-die test. |
| AF3 | *Fixed.* `drain soul execute` fires at 25% target health, where Drain Soul's execute bonus starts, not the generic 20% `target critical health`. |
| AF4 | Corruption, UA and CoA triggers are presence-only (`beforeDuration` omitted), so the DoT must fully drop before a recast is queued — a guaranteed GCD (Corruption, CoA) or full cast (UA, 1.5s) of downtime every cycle. |
| AF5 | No Shadow Bolt filler trigger (only the `shadow trance` proc node); the filler comes from `getDefaultActions` at 5.2. Works, but cannot be reasoned about or reordered from the ladder. |
| AF6 | No Seed of Corruption or Shadowflame in the Affliction ladder — AoE comes solely from the shared `aoe` strategy on `"medium aoe"` (3+). Two-target cleave never gets the second DoT set. |

## Demonology

Code order: life tap glyph 29.5, metamorphosis 28.5, demonic empowerment 28.0, corruption on attacker
19.5, immolate on attacker 19.0, decimation → soul fire 18.5, corruption 18.0, immolate 17.5, molten
core → incinerate 16.5, life tap 5.1, flee 39.0. Defaults: corruption 5.5, immolate 5.4, shadow bolt
5.3, incinerate 5.2, shoot 5.0.

| # | Finding |
|---|---|
| DM1 | *Fixed.* Decimation → Soul Fire sits at 18.5, above the DoT refreshes: its 10 s window spent on Corruption or Immolate dropped the strongest execute button. The trigger needs more than 3 s of Decimation left. |
| DM2 | *Wont fix.* Immolation Aura is reachable only through the shared `aoe` strategy, and only when something else already put the bot inside 5 yd. Running Demonology into melee for it fights the ranged spots raid strategies assign, so the `meta melee` strategy and its Demon Charge opener were dropped along with the single-target node. |
| DM3 | *Fixed* (user decision): `AiFactory` gives Demonology `curse of doom` (higher throughput on any fight over 60s); Destruction keeps Curse of the Elements so the raid debuff still comes from somewhere. Curse of Doom waits for a target expected to live 60 s (it hits on expiry); the strategy carries its own Curse of Agony node for shorter-lived targets, and Agony never replaces our own Doom. Curse strategies are siblings: a second one listed after `curse of doom` evicts it. |
| DM4 | DoTs land on targets about to die: Corruption, UA and CoA triggers call `BuffTrigger::IsActive()`, which throws away `DebuffTrigger`'s `needLifeTime` ([why](../engine/action-selection.md#trigger-semantics)), and `ImmolateNeedsRefresh` checks no life time at all. |
| DM6 | Metamorphosis fires on a plain `BoostTrigger` at 28.5. It is burst-gated, so the tank-hold applies, but **nothing aligns it with Bloodlust.** |
| DM7 | *Fixed* (Destruction too): Immolate recasts once it would run out before a new cast lands (`ImmolateNeedsRefresh`). A spell recast resets the tick timer on this core, so landing earlier loses the tick in progress; waiting for the drop cost 1-4 s per cycle. |

## Destruction

Code order: flee 39.0, life tap glyph 29.5, immolate 20.0, conflagrate 19.5, chaos bolt 19.0, target
critical health → shadowburn 18.0, incinerate 17.5, corruption on attacker 5.5, corruption 5.4, life
tap 5.1. Defaults: immolate 5.9, conflagrate 5.8, chaos bolt 5.7, incinerate 5.6, corruption 5.3,
shadow bolt 5.2, shoot 5.0.

| # | Finding |
|---|---|
| DS1 | *Fixed.* `"conflagrate"` and `"chaos bolt"` have registered trigger creators; unregistered, both spells only fired from `getDefaultActions` at 5.8/5.7, unprioritisable. `ConflagrateTrigger` requires our own Immolate on the target (WotLK Conflagrate no longer consumes it). |
| DS2 | *Fixed.* `"incinerate"` has a registered trigger (17.5), not only the 5.6 default. |
| DS3 | Shadowburn (18.0, `target critical health`) is not in the guide's priority at all. It costs a soul shard, and `"no soul shard"` sits at 60.0 in the shared block, so heavy execute usage can put the bot into a shard-recreation loop mid-fight. |
| DS4 | No Curse of Doom option; `AiFactory` hard-assigns `curse of elements`. **User decision: leave Destruction on CoE**, since it is the only warlock spec supplying the raid debuff. |

## Shared

| # | Finding |
|---|---|
| SH1 | `WarlockBoostStrategy` (`"boost"`) and `WarlockPetStrategy` (`"pet"`) are registered with **empty `InitTriggers`**. Dead strategies — fill or drop. |
| SH2 | `TankWarlockStrategy::InitTriggers` is empty and does not chain to `GenericWarlockStrategy::InitTriggers`, so a tank-spec warlock loses life tap, soulshatter and all soul-shard management. |
| SH3 | Five curses (Doom, Elements, Exhaustion, Tongues, Weakness) sit at 29.0, so enabling two curse strategies gives order-dependent, non-deterministic behaviour. |
| SH4 | *Removed* — the unreferenced `backlash` trigger and `hellfire`, `shadow cleave`, `drain mana`, `drain life` actions. |
| SH5 | *Fixed.* Life Tap on `"low mana"` at 95.0 outranked `spell lock` (40.0) and everything else, so a low-mana warlock tapped through an interrupt window. Now 39.5 — see the table above. |
| SH6 | No `CLASS_WARLOCK` case in `AiFactory::GetPlayerRoles`; falls through to the default DPS role. Fine today. |

## Divergences to decide

| # | Spec | Code | Guide | Trade-off |
|---|---|---|---|---|
| V1 | Affliction | `curse of agony` only | Curse of the Elements if assigned | Bots have no raid-assignment concept; CoA is the higher personal DPS choice, so leaving it is defensible |
| V2 | Destruction | CoE always | Curse of Doom unless assigned CoE | Personal DPS vs supplying the raid's spell-damage debuff — resolved in favour of CoE |
| V3 | All | No Inferno anywhere | "Cast Inferno if the rest of the fight will not last longer than 1 minute" | Needs a fight-length estimate the bot does not have |
| V4 | All | No Improved Shadow Bolt tracking | "Maintain if assigned" | The debuff comes free from any Shadow Bolt cast; only matters as an assignment |

Pre-pull steps in the guides (pre-pot, pre-cast Shadow Bolt, Life Tap before the pull) are out of
scope — bots have no pre-pull phase.

## Confirmed correct — do not re-audit

Pet assignment per spec (Felhunter / Felguard / Imp) matches all three guides. Glyph of Life Tap
upkeep at 29.5 in every spec. Demonic Empowerment on cooldown at 28.0 (Demonology). **Molten Core →
Incinerate at 16.5 is correct by accident** — Soul Fire at 18.5 outranks it, which is what the guide
wants. Destruction's Corruption-below-Incinerate ordering (the `// won't be used after level 64`
comments already say so). The shared AoE ladder — immolation aura 26.0, shadowfury 23.0, shadowflame
22.5, seed of corruption 22.0/21.5, rain of fire 21.0 — matches the guides' "large pulls: Shadowflame
then Seed", and `CastRainOfFireAction` correctly suppresses itself when the bot knows Seed of
Corruption. One caster's Corruption and Seed of Corruption are mutually exclusive on the same
target, which is correct for WotLK; see the Seed section for how the bot avoids trading one for the
other.
