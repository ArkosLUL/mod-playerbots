# Rotation audit vs wowtbc.gg WotLK guides — findings doc

## Context

Recent commits reworked DPS rotations for Rogue (Assassination/Combat), Hunter (all three),
Frost DK, and Fury/Arms Warrior:

- `024e2d3f3` Assassination and Combat rogue strategy improvements
- `342bba906` Hunter rotation improvements
- `45a0bc2c9` Frost DK rotation improvements
- `58d327c1d` Fury and Arms strategy improvements
- `8966c4ee1` Shared major armor debuff helper

The audit compares them against the wowtbc.gg WotLK class guides
(`https://wowtbc.gg/wotlk/class-guides/<spec>/`). Pre-pull steps (pre-pot, pre-cast shouts,
Bloodrage pre-pull, opener sequences) are out of scope — bots have no pre-pull phase.

**Deliverable: a findings document only. No changes under `src/`.** Deviations the code defends in
a comment get written up with the trade-off and left for a per-item decision.

The comparison work is already done (below). The remaining work is writing it up.

## Deliverable

`docs/classes/rotation-guide-audit-findings.md`, matching the layout of the existing
`docs/classes/*-findings.md` files. Self-contained: a fresh session must be able to act on it
without this conversation.

Structure:

1. **Scope** — the five commits, the guide URLs per spec, pre-pull exclusion.
2. **How to read priorities** — `ACTION_DEFAULT=5, ACTION_NORMAL=10, ACTION_HIGH=20, ACTION_MOVE=30,
   ACTION_INTERRUPT=40, ACTION_RAID=60, ACTION_EMERGENCY=90`
   ([Strategy.h:53-65](src/Bot/Engine/Strategy/Strategy.h#L53-L65)), so every finding can quote the
   resolved float.
3. **Per spec**: guide priority list, the code's resolved priority list, then a findings table
   (id / what the code does / what the guide says / impact).
4. **Divergences to decide** — the deliberate ones, each with the trade-off, no recommendation baked
   in as fact.
5. **Confirmed-correct list** — so the next reader does not re-audit them.

## Findings to write up

### Frost DK — [FrostDKStrategy.cpp](src/Ai/Class/Dk/Strategy/FrostDKStrategy.cpp)

| # | Code | Guide |
|---|------|-------|
| F1 | Killing Machine → Frost Strike at `ACTION_HIGH` (20); Rime → Howling Blast at `ACTION_HIGH+1` (21), so Rime always wins | Filler order is KM Frost Strike **then** Rime Howling Blast |
| F2 | Unbreakable Armor at `ACTION_DEFAULT+0.6` (5.6), below the default Obliterate (5.7) — fires only when Obliterate is unusable | UA is a cooldown, used on cd |
| F3 | `HighRunicPowerTrigger` needs >= 80 RP before Frost Strike ([DKTriggers.cpp:96](src/Ai/Class/Dk/DKTriggers.cpp#L96)) | "Use Frost Strike if you have the Runic Power" — spend to avoid capping |
| F5 | AoE: Howling Blast at `ACTION_HIGH+4` correctly outranks Obliterate, but Killing Machine still routes to Frost Strike | On AoE, KM procs go into Howling Blast |

### Fury Warrior — [FuryWarriorStrategy.cpp](src/Ai/Class/Warrior/Strategy/FuryWarriorStrategy.cpp)

| # | Code | Guide |
|---|------|-------|
| U1 | Bloodsurge Slam (`instant slam`, 25) below Bloodthirst (27) and Whirlwind (26); 5s proc window vs 6s BT cd and 8s WW cd means procs get eaten | Slam with Bloodsurge ranks **above** Bloodthirst and Whirlwind |
| U2 | AoE does not reprioritise Whirlwind over Bloodthirst; `WarrirorAoeStrategy` adds Sweeping Strikes / Bladestorm (both Arms-only) plus Cleave at `ACTION_HIGH` | On AoE, Whirlwind gains priority over Bloodthirst, Cleave replaces Heroic Strike |

### Arms Warrior — [ArmsWarriorStrategy.cpp](src/Ai/Class/Warrior/Strategy/ArmsWarriorStrategy.cpp)

| # | Code | Guide |
|---|------|-------|
| A1 | Bladestorm only wired to `bladestorm on aoe` — never on a single target | Bladestorm is in the single-target priority (used when low on rage) |
| A3 | Overpower and Taste for Blood at HIGH+4 (24), Mortal Strike at HIGH+6 (26) | Taste for Blood Overpower ranks above Mortal Strike |
| A4 | Rend at HIGH+3 (23), under MS / Execute / Overpower — a refresh loses a GCD | Rend maintained at the top of the priority |
| A5 | Arms registers a `death wish` trigger, but Death Wish is Fury tier 5 (20 points), unreachable in a PvE Arms build — verify against the Arms premade spec in `conf/playerbots.conf.dist` | Not in the Arms priority |

### Assassination Rogue — [AssassinationRogueStrategy.cpp](src/Ai/Class/Rogue/Strategy/AssassinationRogueStrategy.cpp)

| # | Code | Guide |
|---|------|-------|
| R1 | No Vanish anywhere in the Assassination strategy | Vanish on cd to proc Overkill (let it expire before reapplying) |

### Combat Rogue — [DpsRogueStrategy.cpp](src/Ai/Class/Rogue/Strategy/DpsRogueStrategy.cpp)

| # | Code | Guide |
|---|------|-------|
| C1 | Eviscerate only on `combo points 5 available`; Rupture fires from 4 CP (`RuptureTrigger`) | Eviscerate at 4-5 CP, Rupture at 5 CP |
| C2 | Killing Spree (23) outranks Blade Flurry (22) and Adrenaline Rush (22, from `RogueBoostStrategy`) | Adrenaline Rush, then Blade Flurry, then Killing Spree |

### Hunter — [BeastMasteryHunterStrategy.cpp](src/Ai/Class/Hunter/Strategy/BeastMasteryHunterStrategy.cpp), [SurvivalHunterStrategy.cpp](src/Ai/Class/Hunter/Strategy/SurvivalHunterStrategy.cpp)

| # | Code | Guide |
|---|------|-------|
| H1 | BM and Survival default action lists both contain `aimed shot`, a Marksmanship talent neither spec has — dead entries | Not in either priority |
| H2 | BM has no Multi-Shot in its single-target priority (only `AoEHunterStrategy` on `light aoe`) | BM single target: Explosive Trap launcher, **Multi-Shot**, Arcane Shot, Steady Shot |
| H3 | Survival has no Multi-Shot in its single-target priority either | SV single target: Serpent Sting, **Multi-Shot**, Steady Shot |
| H4 | Sniper Training not modelled | SV maintains Sniper Training (6s stationary) |

### Divergences to decide (code has a stated reason)

| # | Spec | Code position | Guide position | Trade-off |
|---|------|---------------|----------------|-----------|
| F4 | Frost DK | Pestilence only via `PestilenceGlyphTrigger`, which hard-requires Glyph of Disease (aura 63334); without it, Icy Touch / Plague Strike (`duration 0.0f`) only refire once the disease has fully dropped | Core cycle is Obliterate, Obliterate, Pestilence, Blood Strike | Guide assumes Glyph of Disease. Either confirm the frost premade spec carries it, or accept a one-GCD disease gap per cycle |
| A2 | Arms | `ShatteringThrowTrigger` fires only against Divine Shield / Ice Block / Blessing of Protection ([WarriorTriggers.cpp:113-137](src/Ai/Class/Warrior/WarriorTriggers.cpp#L113-L137)) | In the standard single-target priority as a raid armor debuff | Using it on cd costs a GCD plus a stance-independent 5s self-slow, and duplicates other armor debuffs |
| A6 | Arms | Rend on the current target only; Thunder Clap deliberately left to `TankWarriorStrategy` (comment in `WarrirorAoeStrategy`) | Rend on all targets, Thunder Clap on large pulls | The comment argues a DPS spec donates a GCD to a debuff the tank already carries |
| R2 | Assassination | Rupture always maintained at HIGH+4, above Hunger for Blood; `HungerForBloodTrigger` requires a Rupture/Garrote bleed | Rupture only in the "bleed rotation" variant, when nobody else applies bleeds | Internally consistent — it guarantees the HfB bleed — but spends 5 CP that would otherwise be Envenom. A group-bleed check (like `GroupSuppliesMajorArmorDebuff`) would be the guide-accurate version |

### Confirmed correct (do not re-audit)

Hunter: Aspect of the Dragonhawk via the `bdps` strategy, Hunter's Mark, Kill Shot at 20%
(`TargetCriticalHealthTrigger` = 20), Chimera Shot ranked above the hard Serpent Sting recast, the
Lock and Load rank-4→3→2 chain, trap-launcher Explosive Trap, Misdirection on the main tank,
Volley/Multi-Shot on AoE. Frost DK: Blood Presence, Empower Rune Weapon on `no rune`, Army of the
Dead, Obliterate rune accounting (`ObliterateRunesTrigger` handles death runes). Rogue: Cold Blood
before Envenom, Envenom's Deadly Poison aura-state check, Expose Armor suppressed when the group
already supplies a major armor debuff, Slice and Dice, Tricks of the Trade. Warrior: Sunder Armor
stack ramp with the Expose Armor / prot-warrior guards, Battle Shout vs Blessing of Might AP
comparison.

## Verification

- Every `file:line` reference in the doc resolves and still says what the doc claims.
- Every priority float quoted matches `ACTION_* + offset` in the cited strategy file.
- Doc reads standalone: someone with a clean context can pick any finding and act on it without
  this conversation or the guide pages.
