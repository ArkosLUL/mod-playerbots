# Warrior — Arms and Fury

Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Current ladders

Fury (`fury` + `aoe`):

| Rel | Trigger | Action |
|---|---|---|
| 90 | critical health | enraged regeneration |
| 40 | pummel / pummel on enemy healer / victory rush | interrupts |
| 39 | enemy out of melee | charge |
| 29 / 28 | berserker stance / battle shout | berserker stance / battle shout |
| 27 / 26 | bloodthirst / whirlwind *(both always active)* | bloodthirst / whirlwind |
| 26 | medium aoe | sweeping strikes, bladestorm |
| 25 | instant slam (Bloodsurge) | slam |
| 24 | target critical health | execute |
| 22 / 21 | bloodrage / death wish, recklessness | *(off-GCD)* |
| 20 | medium aoe | cleave *(off-GCD)* |
| 19 | sunder armor stack | sunder armor |
| 5.5 → 5.0 | *defaults* | bloodthirst, whirlwind, sunder armor, execute, melee |
| 5.1 | high rage available | heroic strike *(off-GCD)* |

Arms (`arms` + `aoe`):

| Rel | Trigger | Action |
|---|---|---|
| 90 | critical health | enraged regeneration |
| 41 / 40 | shattering throw / victory rush, enemy out of melee | shattering throw / victory rush, charge |
| 30 / 29 | battle stance / battle shout | battle stance / battle shout |
| 27 | medium aoe | sweeping strikes |
| 26 | mortal strike available, medium aoe | mortal strike, bladestorm |
| 25 | target critical health / sudden death | execute |
| 24 | overpower / taste for blood | overpower |
| 23 | rend | rend |
| 22 | bloodrage / death wish | *(off-GCD)* |
| 21 | bladestorm available | bladestorm |
| 20 | hamstring, medium aoe | piercing howl, cleave *(off-GCD)* |
| 19 | sunder armor stack | sunder armor |
| 18 | medium rage available | slam |
| 5.1 → 5.0 | *defaults* + high rage available | mortal strike, sunder armor, melee, heroic strike *(off-GCD)* |

## What was wrong and why the fixes look like they do

- **No Execute phase** — no `target critical health` trigger existed, so Execute only competed from
  `getDefaultActions()` at 5.2, behind every trigger-driven action.
- **Mortal Strike was gated on its own debuff.** `MortalStrikeDebuffTrigger` was a `DEBUFF_TRIGGER`;
  Mortal Wound lasts 10s against a 6s cooldown, so it fired roughly once per 10s and MS otherwise
  competed from the default list. Now keyed off a `CAN_CAST_TRIGGER`. This is the general lesson:
  **`DebuffTrigger` cannot pace a cooldown-driven ability.**
- **Heroic Strike dumped rage too early** — `medium rage available` is rage ≥ 40, and HS costs 15,
  leaving 25, below Bloodthirst's 30. Moved to `high rage available`.
- **`intimidating shout` @90 on `critical health`** fears everything in 8 yd. Bosses are immune, so
  `CastDebuffSpellAction` never saw the aura land and retried every tick; on trash it broke CC.
  Removed. `retaliation` @91 needs ≥2 melee attackers, so it is inert on a boss and wastes a GCD when
  adds latch on — also removed. `enraged regeneration` moved from `medium health` to `critical
  health` (it was donating a GCD plus 15 rage to a healer's job).
- **The AoE strategy hijacked both DPS specs.** `"aoe"` is added unconditionally, and
  `WarrirorAoeStrategy` fired on `light aoe` (2+ attackers within 8 yd) — true on most boss fights
  with adds — pushing Thunder Clap and Shockwave, which DPS specs cannot cast. Now fires on
  `medium aoe` (3+) and carries only sweeping strikes / bladestorm / cleave; the tank-only entries
  moved into `TankWarriorStrategy`.
- **Sunder Armor survived only by accident.** `CastSunderArmorAction::isUseful` yields only to a
  *warrior* tank, so with no prot warrior in the roster the DPS warriors are the raid's Sunder
  providers — and Sunder had no trigger at all, sitting in `getDefaultActions()`. The new
  `SunderArmorStackTrigger` mirrors that `isUseful` (in a group, no warrior tank, debuff below 5
  stacks or under 6s left), so it goes quiet at 5 stacks and costs one refresh per ~24s. The old
  `"sunder armor"` `DEBUFF_TRIGGER` still exists so `TankWarriorStrategy` is untouched.

### Off-GCD abilities and `SetNextCheckDelay(0)`

Heroic Strike, Cleave, Bloodrage, Death Wish, Recklessness and Berserker Rage do not trigger the
in-game GCD, but each cost a whole AI tick — 100-300 ms with a real master, 500-700 ms for a
masterless bot.

**The plan's `SetNextCheckDelay(0)` cannot work**: `YieldThread` raises `nextAICheckDelay` to the
react delay whenever it is lower, so a 0 written inside an action is overwritten before the tick
ends. `OffGlobalCooldownAction<Base>` instead casts and returns `false`, so `DoNextAction` keeps
walking the queue. Two consequences: the debug log reads `A:heroic strike - FAILED` for a cast that
actually went out (read ability order from the engine, not the OK/FAILED tag, for these six), and the
node's alternatives get pushed — harmless here (`heroic strike` → `melee`, `death wish` →
`bloodrage`, itself off-GCD).

### Gear

`ApplyPreferredSpecWeapons` returned `1.0f` unless the slot was MAINHAND/OFFHAND/RANGED, and every
runtime caller passed the default slot of −1 — so the 3× spec-speed bonus shaped initial factory
gearing only. The slot is now threaded through `ItemUsageValue`, `EquipAction` and `BuyAction`. The
vendor *sort* still scores without a slot, deliberately: candidates are ranked against each other
there, not against an equipped piece. Server config was never involved.

## Confirmed correct — do not re-audit

- **Melee hit cap is not a defect.** `ApplyOverflowPenalty` computes `hit_current` as
  `GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE) + GetRatingBonusValue(CR_HIT_MELEE)`, byte-for-byte
  what the core uses in `Player::UpdateMeleeHitChances`. Precision and every other talent granting
  melee hit through that aura is therefore **already inside `hit_current`**, and the 8% cap is a
  *total* hit cap, not a gear-only one. Subtracting talented hit on top would double-count and make
  bots under-value hit by 3%.
- Sunder Armor stays in both DPS default lists (user decision).
- `InitConsumables` skipping the weapon-stone ladder above level 75 is intentional.
- Flasks, elixirs and stat food are handled server-side as world buffs.
- Battle Shout vs Blessing of Might AP comparison.
- `WarriorStanceRequirementActionNodeFactory` was ~200 lines never instantiated anywhere — deleted.

## Open

- **Bladestorm on cooldown** is now in the Arms single-target priority at 21. It is a 6s channel, so
  it will occasionally push back one Mortal Strike or Execute. This matches the WotLK Arms priority,
  but it is the first thing to try lowering if Arms still trails.
- **Talents (G2) and glyphs (G4) need the DB checked first.** `PremadeSpecLink.1.0.80` decodes to
  55 Arms / 8 Fury / 8 Prot, and the Prot points land in Improved Bloodrage / Improved Thunder Clap /
  Incite — no raid-DPS value. Decode against a live character before moving them. Warrior glyph sets
  are raw item ids in conf and unfilled slots get a **random** eligible glyph; the PvP remap at
  `PlayerbotFactory.cpp:4404-4416` also reclassifies an Arms bot purely on the presence of Second
  Wind (29838) and needs a guard.
- Against the wowtbc.gg guides, still open: Bloodsurge Slam should rank *above* Bloodthirst and
  Whirlwind (a 5s proc window against a 6s BT and 8s WW cooldown means procs get eaten); AoE should
  reprioritise Whirlwind over Bloodthirst; Taste for Blood Overpower should rank above Mortal Strike;
  Rend should sit at the top of the Arms priority rather than under MS/Execute/Overpower.
- Arms registers a `death wish` trigger, but Death Wish is Fury tier 5 — unreachable in a PvE Arms
  build. Verify against the Arms premade spec before removing.

### Deliberate divergences from the guide

- **Shattering Throw** fires only against Divine Shield / Ice Block / Blessing of Protection. Using it
  on cooldown as a raid armor debuff costs a GCD plus a 5s self-slow and duplicates other armor
  debuffs.
- **Rend on the current target only, Thunder Clap left to the tank.** A DPS spec donating a GCD to a
  debuff the tank already carries is the trade-off being refused.
