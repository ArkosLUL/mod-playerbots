# Warrior — Arms and Fury

Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Current ladders

Fury (`fury` + `aoe`):

| Rel | Trigger | Action |
|---|---|---|
| 90 | critical health | enraged regeneration |
| 40 | pummel / pummel on enemy healer / victory rush | interrupts |
| 39 | enemy out of melee | charge |
| 29 | berserker stance | berserker stance |
| 28.5 / 28.4 | in battle stance | rend / overpower |
| 28 | battle shout | battle shout |
| 27 | instant slam (Bloodsurge) / medium aoe | slam / sweeping strikes |
| 26.5 / 26 | medium aoe | whirlwind / bladestorm |
| 26 / 25 | bloodthirst / whirlwind *(both always active)* | bloodthirst / whirlwind |
| 24 | target critical health | execute |
| 23 | battle stance dance | battle stance |
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
| 28 | rend refresh | rend |
| 27 | medium aoe / taste for blood | sweeping strikes / overpower |
| 26 | mortal strike available, medium aoe | mortal strike, bladestorm |
| 25 | target critical health / sudden death | execute |
| 24 | overpower | overpower |
| 22 | bloodrage / death wish | *(off-GCD)* |
| 21 | bladestorm available | bladestorm |
| 20 | hamstring, medium aoe | piercing howl, cleave *(off-GCD)* |
| 19 | sunder armor stack | sunder armor |
| 18 | light rage available | slam |
| 5.1 → 5.0 | *defaults* + high rage available | mortal strike, sunder armor, melee, heroic strike *(off-GCD)* |

Both specs also run `GenericWarriorStrategy` (`fear sleep sap` → berserker rage 91, `enemy out of
melee` → reach melee 21) and `WarriorAoeStrategy` (the `medium aoe` rows above).

## Decisions

- **Execute has a `target critical health` node** (Fury 24, Arms 25); from `getDefaultActions()` alone
  it sat behind every trigger-driven action.
- **Mortal Strike is keyed off a `CAN_CAST_TRIGGER`** (`mortal strike available`): on its own
  debuff, Mortal Wound (10 s) against a 6 s cooldown fired it about once per 10 s. **`DebuffTrigger`
  cannot pace a cooldown-driven ability** ([why](../engine/action-selection.md#trigger-semantics)).
- **Heroic Strike is on `high rage available`**: `medium rage available` is rage ≥ 40, and HS costs 15,
  leaving 25, below Bloodthirst's 30.
- **No `intimidating shout` or `retaliation` on `critical health`.** Intimidating Shout fears
  everything in 8 yd; bosses are immune, so `CastDebuffSpellAction` never saw the aura land and retried
  every tick, and on trash it broke CC. Retaliation needs ≥2 melee attackers, so it is inert on a boss
  and wastes a GCD when adds latch on. `enraged regeneration` is on `critical health`, not `medium
  health`, where it donated a GCD plus 15 rage to a healer's job.
- **`WarriorAoeStrategy` fires on `medium aoe` (3+), not `light aoe`.** `"aoe"` is added
  unconditionally, and 2+ attackers within 8 yd is true on most boss fights with adds. Thunder Clap
  and Demoralizing Shout live in `TankWarriorStrategy`: a DPS spec either cannot cast them or donates a
  GCD to a debuff the tank already carries.
- **DPS warriors are the raid's Sunder providers** when no warrior tank is in the group:
  `CastSunderArmorAction::isUseful` yields only to a *warrior* tank. `SunderArmorStackTrigger`
  mirrors that (in a group, no warrior tank, debuff below 5 stacks or under 6s left), so it goes quiet
  at 5 stacks and costs one refresh per ~24s; a non-tank also stands down while Expose Armor or a worm's
  Acid Spit holds the slot. The plain `"sunder armor"` `DEBUFF_TRIGGER` stays for `TankWarriorStrategy`.
- **Arms Rend runs on `"rend refresh"` at 28, top of the GCD band.** mod-spell-tweaks'
  `SpellTweaks.RendTrauma.Enable` (default 1) gives Trauma (46854 / 46855) holders Rend ticks that crit
  and come faster with melee haste at fixed duration, and Taste for Blood procs off them; the plain
  `"rend"` trigger at 23 let it lapse for several GCDs. No `beforeDuration` window, it costs tick time
  ([why](../engine/action-selection.md#trigger-semantics)): `ShouldCastRend` fires only when the bot's
  own Rend is missing or all its ticks have landed, i.e. in the dead tail hasted Rend leaves (unhasted
  15s / 21s divides evenly by 3s). `CastRendAction::isUseful` mirrors it, since the base rejects a
  present aura. Tanks keep the plain `"rend"` trigger.
- **Arms Slams from 20 rage** (`light rage available` @18), as the sim's filler does from 15; Heroic
  Strike keeps the 60 overflow. Improved Slam is a passive talent, so a `HasSpell` check
  ([misses it](../engine/action-selection.md#casting)) left Slam to Bloodsurge procs only.
- **Fury stance dance.** Rend and Overpower need Battle Stance, so Improved Rend, Improved Overpower
  and Taste for Blood were dead talents. `battle stance dance` swaps from Berserker Stance only with a
  free GCD, Bloodthirst ≥1s and Whirlwind ≥1.5s from ready, no Bloodsurge proc, ≥10 rage, in melee,
  and either the bot's own Rend due (`ShouldCastRend`) or a Taste for Blood proc with Overpower off
  cooldown. In Battle Stance, Rend and Overpower outrank Bloodsurge Slam; the 1s stance cooldown holds
  `berserker stance` off for that one strike. No rage ceiling: the swap keeps 10 rage with Stance Mastery
  (Rend's cost; Tactical Mastery adds 5 per rank), and the excess lost is worth less than the strike.
  Bleed-immune targets get no Rend (`ShouldCastRend`), or the dance would repeat all fight. No AoE gate either. Cost:
  Pummel and Intercept are unavailable for that ~1s.

### Off-GCD abilities

Heroic Strike, Cleave, Bloodrage, Death Wish, Recklessness and Berserker Rage do not trigger the
in-game GCD, but each cost a whole AI tick — 100-300 ms with a real master, 500-700 ms for a
masterless bot. `OffGlobalCooldownAction<Base>` casts and returns `false`, so the tick goes on
([why not `SetNextCheckDelay(0)`](../engine/action-selection.md#casting)). For these six, read ability
order from the engine, not the OK/FAILED tag; the alternatives it pushes are harmless (`heroic strike`
→ `melee`, `death wish` → `bloodrage`, itself off-GCD).

### Protect party member

`intervene` (Protection, 90) aims at `party member to protect no tank` and also requires `my attacker
count == 0`: without it a tank warrior charges off the boss to any raider below 30%.
`ProtectPartyMemberTrigger` stays generic, so the paladin and warrior nodes fire on the shared trigger
and report `isUseful() == false` when the candidate is not for them
([value semantics](../engine/action-selection.md#healing)).

### Gear

`ApplyPreferredSpecWeapons` returned `1.0f` unless the slot was MAINHAND/OFFHAND/RANGED, and every
runtime caller passed the default slot of −1 — so the spec-speed preference shaped initial factory
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

## Open

- **Bladestorm on cooldown** is now in the Arms single-target priority at 21. For 6s it blocks every
  cast but Heroic Strike / Cleave (aura 263 sets `PLAYER_ALLOW_ONLY_ABILITY`), so it will occasionally
  push back one Mortal Strike or Execute, and Rend can lapse mid-spin (accepted over refreshing Rend
  early, user decision). This matches the WotLK Arms priority, but it is the first thing to try
  lowering if Arms still trails.
- **Talents (G2) and glyphs (G4) need the DB checked first.** `Playerbots.PremadeSpecLink.1.0.80`
  decodes to 55 Arms / 8 Fury / 8 Prot, and the Prot points land in Improved Bloodrage / Improved
  Thunder Clap / Incite — no raid-DPS value. Decode against a live character before moving them.
  Warrior glyph sets are raw item ids in conf and unfilled slots get a **random** eligible glyph; the
  PvP remap in `PlayerbotFactory::InitGlyphs` also reclassifies an Arms bot purely on the presence of
  Second Wind (29838) and needs a guard.
- `shockwave on snare target` (25) still sits in `WarriorAoeStrategy`, which every warrior gets,
  though a DPS spec cannot cast Shockwave.
- **Rend haste re-snapshot** (Bloodlust, Berserking, haste procs) is deferred: ~1 extra tick per lust,
  and it would copy mod-spell-tweaks' Trauma + attack-speed formula into playerbots.
- Arms registers a `death wish` trigger, but Death Wish is Fury tier 5 — unreachable in a PvE Arms
  build. Verify against the Arms premade spec before removing.

### Deliberate divergences from the guide

- **Shattering Throw** fires only against Divine Shield / Ice Block / Blessing of Protection. Using it
  on cooldown as a raid armor debuff costs a GCD plus a 5s self-slow and duplicates other armor
  debuffs.
- **Rend on the current target only, Thunder Clap left to the tank.** A DPS spec donating a GCD to a
  debuff the tank already carries is the trade-off being refused.
