# Warlock — Affliction, Demonology, Destruction

Audit against the wowtbc.gg WotLK guides, implemented in commit `db4179d56` ("Warlock rotation
changes based on audit") — including `CorruptionSnapshotTrigger` and registrations for the
previously dead Destruction triggers. The findings below are the audit record; **items marked OPEN
were re-verified against `src/` and are still live**. Engine semantics are in
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
2. **The real gap is crit and % damage mods.** `AfflictionWarlockStrategy` casts Corruption at 18.0,
   the highest DoT relevance, making it the opener — applied cold, before any trinket proc, potion or
   raid cooldown. With Everlasting Affliction rolling it forward the debuff never drops,
   `CorruptionTrigger` (presence-only) never fires again, and **that cold crit snapshot is frozen for
   the whole encounter.**

The agreed design is a `CorruptionSnapshotTrigger` that reads the stored snapshot off the aura and
compares it against live crit — `AuraEffect::GetCritChance()` against
`Unit::SpellDoneCritChance` / `SpellTakenCritChance`, recasting once the delta exceeds ~5%. Properties
worth keeping in mind:

- **It self-gates on Pandemic.** `CalcPeriodicCritChance` returns `0.0f` unless a
  `SPELL_AURA_ABILITY_PERIODIC_CRIT` aura affects the spell, so an untalented bot never trips it.
- **It self-clears.** The recast runs `RefreshTimers()` → `CalculatePeriodicData()`, the snapshot
  becomes live, the trigger goes quiet. No cooldown or one-shot latch needed.
- **It needs a companion action.** `CastCorruptionAction::isUseful()` delegates to
  `CastAuraSpellAction::isUseful()`, which returns false while Corruption is up — it would veto every
  re-snapshot. A separate action that skips the aura-presence veto and relies on `isPossible()` is
  required. Suggested slot ~16.0, under the DoT maintenance block, so it never delays Haunt or an
  actually-missing DoT.
- `m_pctMods` staleness has the same cause and is fixed by the same recast — no separate trigger.

## Framework notes specific to warlock

- **Every warlock DoT action overrides `isUseful()` to call `CastAuraSpellAction::isUseful()`**, which
  returns false while the aura is present. A trigger that wants a cast on an already-buffed target is
  vetoed by the action unless the action changes too. Same trigger/action duplication as warrior
  Sunder.
- `AttackerWithoutAuraTargetValue` filters on **aura presence only**, so `DebuffOnAttackerTrigger`
  cannot see remaining duration even with `beforeDuration` set — the value returns `nullptr` for a
  target that still has the about-to-expire DoT. Multi-target duration-aware refresh needs a new
  value class.
- `ValueContext` exposes no aura-duration value at all; direct access is `PlayerbotAI::GetAura(...)`.
- `"metamorphosis"` is the **only** warlock entry in `burstCooldownNames`, so `BurstWindowStrategy`
  paces it and nothing else.

## Affliction

Code order: corruption on attacker 19.5, UA on attacker 19.0, corruption 18.0, UA 17.5, haunt 16.5,
shadow trance → shadow bolt 16.0, target critical health → drain soul 15.5; life tap glyph 29.5, life
tap 5.1, flee 39.0. Defaults: corruption 5.5, UA 5.4, haunt 5.3, shadow bolt 5.2, shoot 5.0. Curse of
Agony comes from the separate `curse of agony` strategy (18.5 on-attacker / 17.0 single).

| # | Finding |
|---|---|
| AF1 | Corruption applied as the opener and never recast — see the snapshot section. **Largest single Affliction DPS loss in the audit.** |
| AF2 | `HauntTrigger` is presence-only at 16.5, so Haunt is cast every ~12s (when the debuff drops) instead of every ~8s (its cooldown), and it sits below Corruption and UA. Also loses the free Corruption refresh Everlasting Affliction hangs off it. Wants a `SpellNoCooldownTrigger`. |
| AF3 | Drain Soul is gated on `"target critical health"` = **20%**, while the guide says 25% — 5% of the execute window spent on Shadow Bolt instead of a Death's-Embrace-boosted Drain Soul. |
| AF4 | All DoT triggers are presence-only (`beforeDuration` omitted), so the DoT must fully drop before a recast is queued — a guaranteed GCD (Corruption, CoA) or full cast (UA, 1.5s) of downtime every cycle. |
| AF5 | No Shadow Bolt trigger; the filler comes only from `getDefaultActions` at 5.2. Works, but cannot be reasoned about or reordered from the ladder. |
| AF6 | No Seed of Corruption or Shadowflame in the Affliction ladder — AoE comes solely from the shared `aoe` strategy on `"medium aoe"` (3+). Two-target cleave never gets the second DoT set. |

## Demonology

Code order: life tap glyph 29.5, metamorphosis 28.5, demonic empowerment 28.0, corruption on attacker
19.5, immolate on attacker 19.0, corruption 18.0, immolate 17.5, decimation → soul fire 17.0, molten
core → incinerate 16.5, life tap 5.1, meta melee flee check 39.0.

| # | Finding |
|---|---|
| DM1 | **Decimation → Soul Fire at 17.0 sits below corruption 18.0 and immolate 17.5.** Decimation procs (10s window) get spent on a DoT refresh, so the strongest execute button is the one that gets dropped. |
| DM2 | Immolation Aura is reachable only through the shared `aoe` strategy or `meta melee`, so **single-target Metamorphosis windows never use it**. `CastImmolationAuraAction::isUseful` already handles the aura-47241 + 5 yd check — a node gated on a "metamorphosis active" trigger is all that is missing. |
| DM3 | `AiFactory` gives Demonology `curse of agony`. **User decision: switch Demonology to Curse of Doom** (higher throughput on any fight over 60s) and leave Destruction on Curse of the Elements so the raid debuff still comes from somewhere. |
| DM4 | `CorruptionTrigger` / `ImmolateTrigger` carry `needLifeTime = 0.5f` — refresh whenever the target survives another half second — so DoTs land on targets about to die. |
| DM6 | Metamorphosis fires on a plain `BoostTrigger` at 28.5. It is burst-gated, so the tank-hold applies, but **nothing aligns it with Bloodlust.** |

## Destruction

Code order: flee 39.0, life tap glyph 29.5, immolate 20.0, conflagrate 19.5, chaos bolt 19.0, target
critical health → shadowburn 18.0, corruption on attacker 5.5, corruption 5.4, life tap 5.1.
Defaults: immolate 5.9, conflagrate 5.8, chaos bolt 5.7, incinerate 5.6, corruption 5.3, shadow bolt
5.2, shoot 5.0.

| # | Finding |
|---|---|
| DS1 | *Fixed.* `"conflagrate"` and `"chaos bolt"` had **no registered trigger creator**, so both nodes were dead and the spells only ever fired from `getDefaultActions` at 5.8/5.7 — unprioritisable, and **nothing checked that Immolate was on the target before Conflagrate**. Both are now registered in `WarlockAiObjectContext.cpp`. |
| DS2 | *Fixed.* Incinerate had no trigger at all (default 5.6 only); `"incinerate"` is now registered. |
| DS3 | Shadowburn at 18.0 sits above the (dead) Conflagrate/Chaos Bolt nodes and is not in the guide's priority at all. It costs a soul shard, and `"no soul shard"` sits at 60.0 in the shared block, so heavy execute usage can put the bot into a shard-recreation loop mid-fight. |
| DS4 | No Curse of Doom option; `AiFactory` hard-assigns `curse of elements`. **User decision: leave Destruction on CoE**, since it is the only warlock spec supplying the raid debuff. |

## Shared

| # | Finding |
|---|---|
| SH1 | `WarlockBoostStrategy` (`"boost"`) and `WarlockPetStrategy` (`"pet"`) are registered with **empty `InitTriggers`**. Dead strategies — fill or drop. |
| SH2 | `TankWarlockStrategy::InitTriggers` is empty and does not chain to `GenericWarlockStrategy::InitTriggers`, so a tank-spec warlock loses life tap, soulshatter and all soul-shard management. |
| SH3 | All five curses sit at 29.0, so enabling two curse strategies gives order-dependent, non-deterministic behaviour. |
| SH4 | The `backlash` trigger and the `hellfire`, `shadow cleave`, `drain mana`, `drain life` actions are registered but referenced by no strategy. |
| SH5 | **OPEN — Life Tap on `"low mana"` at 95.0 outranks `spell lock` (40.0) and everything else** (`GenericWarlockStrategy.cpp:27`): a low-mana warlock will Life Tap through an interrupt window. |
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
Incinerate at 16.5 is correct by accident** — Soul Fire at 17.0 outranks it, which is what the guide
wants. Destruction's Corruption-below-Incinerate ordering (the `// won't be used after level 64`
comments already say so). The shared AoE ladder — immolation aura 26.0, shadowfury 23.0, shadowflame
22.5, seed of corruption 22.0/21.5, rain of fire 21.0 — matches the guides' "large pulls: Shadowflame
then Seed", and `CastRainOfFireAction` correctly suppresses itself when the bot knows Seed of
Corruption. Corruption and Seed of Corruption are mutually exclusive on the same target, which is
correct for WotLK.
