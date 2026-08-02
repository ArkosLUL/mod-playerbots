# Warlock rotation audit — findings document

## Context

The warlock class AI in `mod-playerbots` has not been through the rotation-audit pass that Frost DK,
Warrior, Rogue, Hunter, Priest and Holy Paladin have already had (commits `342bba906`, `024e2d3f3`,
`d06538b23`, `2ba4b0dba`, `220598977`). The request is to audit all three warlock specs against the
wowtbc.gg WotLK guides, with a deep look at the Affliction Corruption snapshot mechanic.

**Deliverable: a findings document only. No changes under `src/`.** The comparison work is already
done and is reproduced in full below — the remaining work is writing it up.

Reference: `https://wowtbc.gg/wotlk/class-guides/{affliction,demonology,destruction}-warlock/`.
Pre-pull steps in those guides (pre-pot, pre-cast Shadow Bolt for Improved Shadow Bolt, Life Tap
before the pull) are out of scope — bots have no pre-pull phase.

Target file: `docs/classes/warlock-all-specs-findings.md`. This is a separate deliverable from the
pending `docs/classes/rotation-audit-findings.md` work order, which covers DK/Warrior/Rogue/Hunter
and must not be touched.

---

## Deliverable layout

Follow the shape of `docs/classes/warrior-arms-fury-dps-findings.md` and
`docs/classes/priest-all-specs-findings.md`:

1. `# Warlock PvE DPS — findings` + one-paragraph preamble saying why the doc exists.
2. `## How the engine picks an ability` — `Engine::DoNextAction`
   ([Engine.cpp:144](../../src/Bot/Engine/Engine.cpp#L144)), the `ACTION_*` table from
   [Strategy.h:53-65](../../src/Bot/Engine/Strategy/Strategy.h#L53), then the framework facts below.
3. `## Corruption snapshot — how it actually works on this core` (its own H2; this is the headline
   finding and needs the core-side walkthrough, not a table row).
4. `## Findings` with one `### <spec>` per spec, each an ID'd table `# | Code | Guide | Impact`.
5. `## Divergences to decide` — table `# | Spec | Code position | Guide position | Trade-off`.
6. `## Confirmed correct (do not re-audit)`.
7. `## Recommended fixes` — grouped by finding ID, naming the trigger class, the registered string
   name and the proposed relevance as `ACTION_* + offset`.
8. `## Verification` — how a later implementation pass would check itself.

All source references as markdown links with anchors, `[File.cpp:NN](../../src/...#LNN)`. Warlock
strategy files currently use bare floats, not `ACTION_*` constants — quote the bare float as it
exists and give the `ACTION_*` equivalent alongside.

---

## Framework facts the findings rely on

- `Engine::ProcessTriggers` ([Engine.cpp:458-466](../../src/Bot/Engine/Engine.cpp#L458)) silently
  `continue`s past a `TriggerNode` whose name has no registered creator. An unregistered trigger is
  a dead node, not an error.
- Triggers and actions are evaluated independently. Every warlock DoT action overrides `isUseful()`
  to call `CastAuraSpellAction::isUseful()`, which returns false while the aura is present — so a
  trigger that wants a cast on an already-buffed target is vetoed by the action unless the action is
  changed too. Same trigger/action duplication as warrior sunder
  ([WarriorTriggers.cpp:61](../../src/Ai/Class/Warrior/WarriorTriggers.cpp#L61) /
  [WarriorActions.cpp:94](../../src/Ai/Class/Warrior/WarriorActions.cpp#L94)).
- `BuffTrigger`/`DebuffTrigger`
  ([GenericTriggers.h:328](../../src/Ai/Base/Trigger/GenericTriggers.h#L328),
  [:401](../../src/Ai/Base/Trigger/GenericTriggers.h#L401)) already support a remaining-duration
  threshold via the `beforeDuration` ctor arg (ms). It defaults to `0` = presence-only. Precedent
  for non-zero values: `RuptureTrigger` (2000,
  [RogueTriggers.h:59](../../src/Ai/Class/Rogue/RogueTriggers.h#L59)),
  `PlagueStrike3sDebuffTrigger` / `IcyTouch3sDebuffTrigger` (3000,
  [DKTriggers.h:27](../../src/Ai/Class/Dk/DKTriggers.h#L27)).
- `DebuffTrigger`'s `needLifeTime` argument is **not** a DoT-remaining check — it is
  `target->GetHealth() / AI_VALUE(float, "estimated group dps") >= needLifeTime`, i.e. "will the
  target live this many more seconds"
  ([GenericTriggers.cpp:311-319](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L311)). The `0.5f` in
  `CorruptionTrigger` is that, not a 0.5 s refresh window.
- `AttackerWithoutAuraTargetValue` ([.cpp:32](../../src/Ai/Base/Value/AttackerWithoutAuraTargetValue.cpp#L32))
  filters on aura presence only, so `DebuffOnAttackerTrigger` cannot see remaining duration even if
  `beforeDuration` is set — the value returns `nullptr` for a target that still has the about-to-expire
  DoT. Multi-target duration-aware refresh needs a new value class.
- `ValueContext` exposes no aura-duration value at all. Direct access is via
  `PlayerbotAI::GetAura(name, unit, checkIsOwner, checkDuration, checkStack)`
  ([PlayerbotAI.cpp:3204](../../src/Bot/PlayerbotAI.cpp#L3204)), which returns the `Aura*`.
- `"target critical health"` is `TargetLowHealthTrigger(botAI, 20)` — 20 %, not 25 %
  ([HealthTriggers.h:141-145](../../src/Ai/Base/Trigger/HealthTriggers.h#L141)).
- `"metamorphosis"` is the only warlock entry in `burstCooldownNames`
  ([BurstCooldowns.cpp:31-32](../../src/Ai/Base/Combat/BurstCooldowns.cpp#L31)), so
  `BurstWindowStrategy` paces it and nothing else.
- Spec → strategy assignment lives in
  [AiFactory.cpp:384-393](../../src/Bot/Factory/AiFactory.cpp#L384) (combat) and
  [:566-575](../../src/Bot/Factory/AiFactory.cpp#L566) (non-combat).

---

## Section 3 material — Corruption snapshot on this core

This is the part that must be written carefully, because the wowtbc guide describes retail-Classic
behaviour and this core does something different. Walk the reader through the actual code path.

**What Everlasting Affliction does here.** `spell_warl_everlasting_affliction`
([spell_warlock.cpp:671-691](../../../src/server/scripts/Spells/spell_warlock.cpp#L671)) finds the
caster's own Corruption `AuraEffect` and calls:

```cpp
aur->GetBase()->RefreshTimersWithMods();
aur->ChangeAmount(aur->CalculateAmount(aur->GetCaster()), false);
```

`Aura::RefreshTimersWithMods()`
([SpellAuras.cpp:878-897](../../../src/server/game/Spells/Auras/SpellAuras.cpp#L878)) recomputes
`m_maxDuration` against **current** `UNIT_MOD_CAST_SPEED` and calls `CalculatePeriodic()` per effect
— but it does **not** call `CalculatePeriodicData()`.

`AuraEffect::CalculatePeriodicData()`
([SpellAuraEffects.cpp:584-597](../../../src/server/game/Spells/Auras/SpellAuraEffects.cpp#L584)) is
the only thing that sets `m_pctMods` (from `SpellPctDamageModsDone`) and `m_critChance` (from
`CalcPeriodicCritChance`). It runs on aura application and from `Aura::RefreshTimers()`
([SpellAuras.cpp:856-875](../../../src/server/game/Spells/Auras/SpellAuras.cpp#L856)) — which is the
path a manual recast takes, not the Everlasting Affliction path.

Periodic ticks roll crit off the stored value:
`if ((crit = roll_chance_f(GetCritChance())))`
([SpellAuraEffects.cpp:6461](../../../src/server/game/Spells/Auras/SpellAuraEffects.cpp#L6461)).

Net result, and this is the table the doc should carry:

| Component | Snapshot at cast? | Refreshed by Everlasting Affliction? |
|---|---|---|
| Haste (duration + tick amplitude) | yes | **yes** — `RefreshTimersWithMods` recalcs with live haste |
| Base amount / spell power | yes | **yes** — `ChangeAmount(CalculateAmount(...))` |
| Death's Embrace (< 35 % target HP) | evaluated inside `SpellDamageBonusDone` | **yes** — class-script block at [Unit.cpp:8522](../../../src/server/game/Entities/Unit/Unit.cpp#L8522), re-run by `CalculateAmount` |
| `m_pctMods` (`SpellPctDamageModsDone`) | yes | **no** |
| `m_critChance` (Pandemic) | yes | **no** |

Two consequences to state plainly:

1. The guide's "manually re-apply Corruption at 35 % health for Death's Embrace" rule buys nothing
   on this core — Death's Embrace is picked up by every Everlasting Affliction refresh. Do **not**
   recommend implementing it.
2. The real gap is crit and % damage mods. `AfflictionWarlockStrategy` casts Corruption at `18.0`,
   the highest DoT relevance, which makes it the opener — applied cold, before any trinket proc,
   potion or raid cooldown. With Everlasting Affliction rolling it forward the debuff never drops,
   `CorruptionTrigger` (presence-only) never fires again, and that cold crit snapshot is frozen for
   the whole encounter.

**Recommended fix (the user picked this option; document the design, do not implement).**

A new `CorruptionSnapshotTrigger` that reads the stored snapshot straight off the aura and compares
it against live crit. Both accessors needed are public:
`AuraEffect::GetCritChance()` ([SpellAuraEffects.h:109](../../../src/server/game/Spells/Auras/SpellAuraEffects.h#L109))
and `Unit::SpellDoneCritChance` / `Unit::SpellTakenCritChance`
([Unit.h:1662-1663](../../../src/server/game/Entities/Unit/Unit.h#L1662)).

```cpp
// Corruption stores its crit chance at cast time. Everlasting Affliction rolls the duration and the
// base amount forward but never re-runs CalculatePeriodicData, so the snapshot taken on the pull
// sticks for the whole fight - recast once live crit is clearly better than what is stored.
float snap = aurEff->GetCritChance();
if (snap <= 0.0f)
    return false;   // no Pandemic: Corruption cannot crit, nothing to snapshot
float live = bot->SpellDoneCritChance(nullptr, spellInfo, SPELL_SCHOOL_MASK_SHADOW, BASE_ATTACK, true);
live = target->SpellTakenCritChance(bot, spellInfo, SPELL_SCHOOL_MASK_SHADOW, live, BASE_ATTACK, true);
return live - snap >= CRIT_DELTA_PCT;   // 5.0f
```

Points the doc must make about this design:

- It self-gates on Pandemic. `CalcPeriodicCritChance`
  ([SpellAuraEffects.cpp:1073-1102](../../../src/server/game/Spells/Auras/SpellAuraEffects.cpp#L1073))
  returns `0.0f` unless a `SPELL_AURA_ABILITY_PERIODIC_CRIT` aura affects the spell, so an untalented
  bot never trips the trigger.
- It self-clears. The recast runs `Aura::RefreshTimers()` → `CalculatePeriodicData()`, `snap` becomes
  `live`, the trigger goes quiet. No cooldown or one-shot latch needed.
- It needs a companion action. `CastCorruptionAction::isUseful()`
  ([WarlockActions.h:262](../../src/Ai/Class/Warlock/WarlockActions.h#L262)) delegates to
  `CastAuraSpellAction::isUseful()`, which returns false while Corruption is up — it would veto every
  re-snapshot. A separate action (e.g. `"corruption resnapshot"`) that skips the aura-presence veto
  and relies on `isPossible()` for castability is required.
- Suggested slot: just under the DoT maintenance block, around `ACTION_NORMAL + 6` (16.0), so it
  never delays Haunt or an actually-missing DoT.
- `m_pctMods` staleness has the same cause and is fixed by the same recast; no separate trigger.

---

## Findings to write up

Relevance figures are the bare floats as they exist in the files today.

### Affliction — `src/Ai/Class/Warlock/Strategy/AfflictionWarlockStrategy.cpp`

Guide priority: Felhunter → Inferno → Improved Shadow Bolt → Curse of the Elements (if assigned) →
Corruption → Glyph of Life Tap buff → Corruption re-apply at 35 % → Haunt → Unstable Affliction →
Curse of Agony → Drain Soul below 25 % → Shadow Bolt.

Code order: corruption on attacker 19.5, UA on attacker 19.0, corruption 18.0, UA 17.5, haunt 16.5,
shadow trance → shadow bolt 16.0, target critical health → drain soul 15.5; life tap glyph 29.5,
life tap 5.1, flee 39.0. Defaults: corruption 5.5, UA 5.4, haunt 5.3, shadow bolt 5.2, shoot 5.0.
Curse of Agony comes from the separate `curse of agony` strategy (18.5 on-attacker / 17.0 single,
[GenericWarlockStrategy.cpp:142-160](../../src/Ai/Class/Warlock/Strategy/GenericWarlockStrategy.cpp#L142)).

| # | Code | Guide | Impact |
|---|---|---|---|
| AF1 | Corruption applied as the opener at 18.0 and, under Everlasting Affliction, never recast — crit/`m_pctMods` snapshot frozen cold for the whole fight | Corruption is a snapshot DoT; apply it where the snapshot is worth keeping | See section 3. Largest single Affliction DPS loss in the audit |
| AF2 | `HauntTrigger` is `DebuffTrigger(ai, "haunt", 1, true, 0)` at 16.5 — fires only once the 12 s debuff is gone, and sits below Corruption 18.0 and UA 17.5 | "Maintain Haunt" ranked above Unstable Affliction; Haunt is on an 8 s cooldown | Haunt cast every ~12 s instead of every ~8 s. Also loses the free Corruption refresh that Everlasting Affliction hangs off it. Wants a `SpellNoCooldownTrigger`, same shape as `ChimeraShotNoCdTrigger` from `342bba906` |
| AF3 | Drain Soul gated on `"target critical health"` = 20 % ([HealthTriggers.h:144](../../src/Ai/Base/Trigger/HealthTriggers.h#L144)) | "Cast Drain Soul on targets 25 % HP or below" | 5 % of the execute window spent on Shadow Bolt instead of a Death's-Embrace-boosted Drain Soul |
| AF4 | All DoT triggers presence-only (`beforeDuration` omitted) — the DoT must fully drop before a recast is queued | Maintain the DoTs | Guaranteed downtime of a GCD (Corruption, CoA) or a full cast (UA, 1.5 s) on every cycle, for any bot without Everlasting Affliction and for UA/CoA always |
| AF5 | No Shadow Bolt trigger; filler comes only from `getDefaultActions` at 5.2 | "Cast Shadow Bolt" as the last priority | Works, but the filler cannot be reasoned about or reordered from the ladder |
| AF6 | No Seed of Corruption or Shadowflame in the Affliction ladder — AoE comes solely from the shared `aoe` strategy on `"medium aoe"` (3+ attackers within 8 y) | 1-2 targets: DoT both, Shadow Bolt/Drain Soul filler. Large pulls: Shadowflame then Seed | Two-target cleave never gets the second DoT set; matches guide only at 3+ |

### Demonology — `src/Ai/Class/Warlock/Strategy/DemonologyWarlockStrategy.cpp`

Guide priority: Felguard → Improved Shadow Bolt → Glyph of Life Tap buff → Curse of Doom or Curse of
the Elements → Metamorphosis → Immolation Aura → Demonic Empowerment → Soul Fire below 35 %
(Decimation, eating Molten Core) → Corruption (only if it will run its full duration) → Incinerate
with Molten Core → Immolate (only if full duration) → Shadow Bolt.

Code order: life tap glyph 29.5, metamorphosis 28.5, demonic empowerment 28.0, corruption on attacker
19.5, immolate on attacker 19.0, corruption 18.0, immolate 17.5, decimation → soul fire 17.0, molten
core → incinerate 16.5, life tap 5.1, meta melee flee check 39.0.

| # | Code | Guide | Impact |
|---|---|---|---|
| DM1 | Decimation → Soul Fire at 17.0, below corruption 18.0 and immolate 17.5 | Soul Fire below 35 % ranked above Corruption and Immolate maintenance | Decimation procs (10 s window) get spent on a DoT refresh; the strongest execute button is the one that gets dropped |
| DM2 | Immolation Aura reachable only through the shared `aoe` strategy (26.0 on `"medium aoe"`) or the `meta melee` strategy | Immolation Aura is in the single-target priority, right after Metamorphosis | Single-target Metamorphosis windows never use Immolation Aura. `CastImmolationAuraAction::isUseful` ([WarlockActions.cpp:152-165](../../src/Ai/Class/Warlock/WarlockActions.cpp#L152)) already handles the aura-47241 + 5 y check, so a node gated on a "metamorphosis active" trigger is all that is missing |
| DM3 | `AiFactory` gives Demonology `curse of agony` ([AiFactory.cpp:388](../../src/Bot/Factory/AiFactory.cpp#L388)) | Curse of Doom, or Curse of the Elements if assigned | Curse of Doom is the higher-throughput curse on any fight over 60 s. **User decision: recommend switching Demonology to Curse of Doom, leave Destruction on Curse of the Elements** so the raid debuff still comes from somewhere |
| DM4 | `CorruptionTrigger`/`ImmolateTrigger` carry `needLifeTime = 0.5f` — refresh whenever the target survives another half second | "as long as it will run for its full duration" | DoTs re-applied on targets about to die; a small amount of wasted GCD time on trash and on execute |
| DM5 | Molten Core → Incinerate at 16.5, unconditional | Molten Core goes to Soul Fire during the execute window, Incinerate otherwise | Currently correct by accident: Soul Fire at 17.0 outranks it. Worth a confirmed-correct line rather than a fix |
| DM6 | No Immolate/Corruption re-ordering for Metamorphosis; Metamorphosis fires on a plain `BoostTrigger` (balance 50) at 28.5 | "Cast Metamorphosis. Try to align with Bloodlust" | `"metamorphosis"` is in `burstCooldownNames`, so `BurstWindowStrategy` already holds it until the tank has the boss — but nothing aligns it with Bloodlust |

### Destruction — `src/Ai/Class/Warlock/Strategy/DestructionWarlockStrategy.cpp`

Guide priority: Imp → Inferno → Glyph of Life Tap buff → Curse of the Elements (if assigned) → Curse
of Doom (if the target lives 60 s+ and CoE is not assigned) → Immolate (let it expire before
re-applying) → Conflagrate → Chaos Bolt → Incinerate → while moving, Conflagrate else Corruption or
Life Tap.

Code order: flee 39.0, life tap glyph 29.5, immolate 20.0, conflagrate 19.5, chaos bolt 19.0, target
critical health → shadowburn 18.0, corruption on attacker 5.5, corruption 5.4, life tap 5.1.
Defaults: immolate 5.9, conflagrate 5.8, chaos bolt 5.7, incinerate 5.6, corruption 5.3, shadow bolt
5.2, shoot 5.0.

| # | Code | Guide | Impact |
|---|---|---|---|
| DS1 | `"conflagrate"` ([DestructionWarlockStrategy.cpp:45](../../src/Ai/Class/Warlock/Strategy/DestructionWarlockStrategy.cpp#L45)) and `"chaos bolt"` ([:53](../../src/Ai/Class/Warlock/Strategy/DestructionWarlockStrategy.cpp#L53)) have **no registered trigger creator** — `WarlockAiObjectContext.cpp:146-192` registers neither, and no generic context does | Both are core rotation entries | Both nodes are dead. Conflagrate and Chaos Bolt only ever fire from `getDefaultActions` at 5.8/5.7. The rotation still roughly works because the default list is ordered, but neither ability can be prioritised against anything else, and nothing checks that Immolate is actually on the target before Conflagrate |
| DS2 | No Incinerate trigger at all — default 5.6 only | "Cast Incinerate" as the main filler | Same class of problem as DS1: the filler is unreachable from the ladder |
| DS3 | Shadowburn at 18.0 on `"target critical health"`, above the (dead) Conflagrate/Chaos Bolt nodes | Shadowburn is not in the guide's single-target priority | Shadowburn costs a soul shard; `"no soul shard"` sits at 60.0 in the shared block ([GenericWarlockStrategy.cpp:19-79](../../src/Ai/Class/Warlock/Strategy/GenericWarlockStrategy.cpp#L19)), so heavy execute usage can put the bot into a shard-recreation loop mid-fight |
| DS4 | No Curse of Doom option; `AiFactory` hard-assigns `curse of elements` ([AiFactory.cpp:390](../../src/Bot/Factory/AiFactory.cpp#L390)) | Curse of Doom when not assigned Curse of the Elements | Divergence. **User decision: leave Destruction on Curse of the Elements**, since it is the only warlock spec supplying the raid debuff |
| DS5 | Corruption nodes at 5.5/5.4 sit below default incinerate 5.6 | Corruption only as a movement filler | Correct outcome; worth a confirmed-correct line. The `// won't be used after level 64` comments at [:23](../../src/Ai/Class/Warlock/Strategy/DestructionWarlockStrategy.cpp#L23) and [:60](../../src/Ai/Class/Warlock/Strategy/DestructionWarlockStrategy.cpp#L60) already say so |

### Shared / cross-spec

| # | Code | Impact |
|---|---|---|
| SH1 | `WarlockBoostStrategy` (`"boost"`) and `WarlockPetStrategy` (`"pet"`) are registered but have empty `InitTriggers` ([GenericWarlockStrategy.cpp:108-116](../../src/Ai/Class/Warlock/Strategy/GenericWarlockStrategy.cpp#L108)) | Dead strategies. Either fill them or drop them |
| SH2 | `TankWarlockStrategy::InitTriggers` is empty and does not call `GenericWarlockStrategy::InitTriggers` | A tank-spec warlock loses life tap, soulshatter and all soul-shard management |
| SH3 | `curse of elements`, `curse of doom`, `curse of exhaustion`, `curse of tongues`, `curse of weakness` all sit at 29.0 ([GenericWarlockStrategy.cpp:166-240](../../src/Ai/Class/Warlock/Strategy/GenericWarlockStrategy.cpp#L166)) | Enabling two curse strategies gives order-dependent, non-deterministic behaviour |
| SH4 | `backlash` trigger ([WarlockAiObjectContext.cpp:166](../../src/Ai/Class/Warlock/WarlockAiObjectContext.cpp#L166)) and the `hellfire`, `shadow cleave`, `drain mana`, `drain life` actions are registered but referenced by no strategy | Dead wiring; the paladin commit `d06538b23` is the precedent for deleting it |
| SH5 | Life Tap on `"low mana"` at 95.0 in the shared block outranks `spell lock` (40.0) and everything else | A low-mana warlock will Life Tap through an interrupt window |
| SH6 | Warlock has no `CLASS_WARLOCK` case in `AiFactory::GetPlayerRoles` | Falls through to the default DPS role — fine today, worth a note |

---

## Divergences to decide (put in the doc, do not resolve)

| # | Spec | Code position | Guide position | Trade-off |
|---|---|---|---|---|
| V1 | Affliction | `curse of agony` only | Curse of the Elements if assigned | Bots have no raid assignment concept. CoA is the higher personal DPS choice; leaving it is defensible |
| V2 | Destruction | Curse of the Elements always | Curse of Doom unless assigned CoE | Personal DPS vs. supplying the raid's spell-damage debuff. Resolved in favour of keeping CoE (DS4) |
| V3 | All | No Inferno anywhere | "Cast Inferno if the rest of the fight will not last longer than 1 minute" | Needs a fight-length estimate the bot does not have |
| V4 | All | No Improved Shadow Bolt tracking | "Maintain Improved Shadow Bolt debuff if assigned" | Debuff comes free from any Shadow Bolt cast; only matters as an assignment |

---

## Confirmed correct (do not re-audit)

Pet assignment per spec (Felhunter / Felguard / Imp,
[AiFactory.cpp:566-575](../../src/Bot/Factory/AiFactory.cpp#L566)) matches all three guides.
Glyph of Life Tap upkeep at 29.5 in every spec matches the guide's "maintain Glyph of Life Tap buff"
slot. Demonic Empowerment on cooldown at 28.0 (Demonology). Molten Core routing (DM5). Destruction's
Corruption-below-Incinerate ordering (DS5). The shared AoE ladder — immolation aura 26.0, shadowfury
23.0, shadowflame 22.5, seed of corruption 22.0/21.5, rain of fire 21.0 — matches the guides' "large
pulls: Shadowflame then Seed of Corruption", and `CastRainOfFireAction` correctly suppresses itself
when the bot knows Seed of Corruption
([WarlockActions.cpp:135-139](../../src/Ai/Class/Warlock/WarlockActions.cpp#L135)). Corruption and
Seed of Corruption are mutually exclusive on the same target
([WarlockTriggers.h:183-200](../../src/Ai/Class/Warlock/WarlockTriggers.h#L183)), which is correct
for WotLK.

---

## Verification

The findings doc ships no code, so verification is a read-back:

1. Every `file:line` reference resolves and still says what the doc claims — including the core-side
   references under `../../../src/server/`, which are outside the module.
2. Every relevance float quoted matches the literal in the cited strategy file, and every
   `ACTION_* + offset` equivalent given alongside it resolves to the same number.
3. Each of DS1's two trigger names is genuinely absent from
   `WarlockAiObjectContext.cpp:146-192` and from `src/Ai/Base/TriggerContext.h`.
4. The doc reads standalone — a fresh session with no prior context can implement any single finding
   from what the doc says, without re-reading the guides.
