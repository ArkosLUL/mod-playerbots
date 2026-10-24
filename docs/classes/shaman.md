# Shaman: Restoration and [Elemental](#elemental)

Engine and healer semantics are in [../engine/action-selection.md](../engine/action-selection.md).

Greater Healing Wave does **not** exist for Shaman in WotLK 3.3.5.

## Restoration ladder

| Trigger | Actions |
|---|---|
| `low health` (self) | stoneclaw totem 41 |
| `party member to heal out of spell range` | reach party member to heal 40 |
| `enemy too close for spell` | flee 39 |
| `party member critical health` | nature's swiftness 58, riptide 36, lesser healing wave 34, healing wave 33, chain heal 32 |
| `nature's swiftness active` | healing wave on party 56 |
| `medium group heal setting` | tidal force 36.5, chain heal 35, riptide 34.5 |
| `group heal setting` | chain heal 28, riptide 27 |
| `medium mana` | mana tide totem 26.5 |
| `earth shield on main tank` | earth shield 26 |
| `party member low health` | riptide 25, chain heal 24.5, healing wave 23.5, lesser healing wave 22.5 |
| `call of the elements` | 21 |
| `party member medium health` | riptide 19, chain heal 18, lesser healing wave 17, healing wave 16 |
| `party member almost full health` | riptide 13, chain heal 12 |
| `water shield` | 10.5 |

Ordering rationale:

- **The heal chain opens at 34-36 like every other healer.** At 23-25, `earth shield on main tank`,
  the group heals, `stoneclaw totem` and `call of the elements` all outranked an emergency heal on a
  dying player.
- **Riptide leads every band** — instant, and it grants Tidal Waves, which speeds the Healing Wave or
  crits the Lesser Healing Wave that follows.
- **Chain Heal is in all four bands and `medium group heal setting`**; on `group heal setting` alone it
  waited for ten injured raiders in a 25-man. It is also the only heal that survives the save-mana veto
  in every situation (HIGH efficiency, low `estAmount`), so it doubles as the low-mana backstop — which
  is why the **critical band ends with chain heal, not lesser healing wave**.
- **Medium band puts lesser healing wave above healing wave** — a 2.5s Healing Wave is the wrong tool
  for topping off a 60% target.
- **`call of the elements` sits at 21, not 60.** Its trigger fires whenever ≥2 totem slots are empty
  or out of range, so at 60 it re-dropped the whole totem set mid-fight while a raider sat at 10% HP.
- **Nature's Swiftness (16188) and Tidal Force (55198)** resolve by spell **name** against the bot's
  own known spells, so no id constants are needed and untalented bots simply fail `CanCastSpell`.
  Registration must be in the **Shaman** context — the Druid registration is not visible to Shaman
  bots, and a missing registration is a silent runtime no-op.
- The out-of-combat strategy carries `chain heal on party` in all four bands too, one step below
  riptide.

**Half-step values are collision breakers**: a resto shaman's engine also runs
`GenericShamanStrategy` (wind shear 23), `ShamanCureStrategy` (cleanse spirit 24, cleanse on party
23) and the four totem strategies (`no X totem` re-drops at 55, `set X totem` at 60).

## `estAmount` / `manaEfficiency`

Read **only** by `HealerAutoSaveManaMultiplier`
([semantics](../engine/action-selection.md#healing)):

| Action | Value | Why |
|---|---|---|
| Lesser Healing Wave (party) | `25.0f, MEDIUM` | LOW was vetoed at any health below 60% bot mana on non-tanks |
| Healing Wave (party) | `40.0f, MEDIUM` | 50 vetoed the cast unless the target was under 50% HP; 40 lets it fire from 60% down |
| Chain Heal | `20.0f, HIGH` | HIGH keeps it the low-mana backstop; a bounce is worth more than a Riptide tick |
| Riptide (party) | `15.0f, VERY_HIGH` | |

**Chain Heal rides the shared group-heal count** ([`AoeHealValue`](../engine/action-selection.md#healing)),
which matches `PartyMemberToHeal::Check` and raycasts last, so only members actually hurt and in range
cost a VMap query.

## Mana kit per spec

Only restoration carries mana potions
([../systems/consumables-and-burst.md](../systems/consumables-and-burst.md), which also covers
Elemental's thin kit); the other two spend their one potion on damage.

| Spec | In combat | Shield |
|---|---|---|
| Elemental | `high mana` (< 65%) → thunderstorm @19.0 | water shield @19.5 |
| Enhancement | `medium mana` (< 40%) → shamanistic rage @23.0, also on `low health`, and on cooldown with the T10 2-piece (aura 70830, +12% damage) | lightning shield @18.5 — **no** water shield |
| Restoration | `medium mana` → mana tide totem @26.5, plus potions | water shield @10.5 |

Removing the potion is what lets enhancement's node work at all: it sat at the same 40% threshold
but was outranked 90 vs 23, so the potion always won the tick.

Watch out: `WaterShieldTrigger` derives from `BuffTrigger`, which tests aura presence only. Water
Shield is **charge**-based, so a shield down to its last orb still reads as up and is not refreshed
until the charges are gone.

## Elemental

Single target follows the wowsims APL: earth shock execute 5.5, flame shock when missing 5.3 > lava
burst 5.2 > lightning bolt 5.0. Chain lightning (5.6) needs `light aoe`, 2+ enemies within 8 yd of the
target.

- **No single-target Chain Lightning.** It deals no more per second than Lightning Bolt for 2.6x the
  mana (26% against 10% base). Ungated, it outranked Lava Burst and Flame Shock every cycle (16 Lava
  Bursts against the sim's 19, Flame Shock up 90% against 95%) and ran the bot dry by 155 s of a
  180 s fight.
- **Elemental Mastery (29) fires whenever its aura is missing.** Waiting for Chain Lightning's
  cooldown to save the instant for Lava Burst would never fire without single-target Chain Lightning.
  Lava Burst and Lightning Bolt both cast 2.0 s, so either can take the instant.
- **Lava Burst needs our Flame Shock with more than 2 s left** (the sim's rule): its sure crit is
  rolled when the cast finishes, so a shock dropping mid-cast leaves a plain hit.
- **Thunderstorm's knockback node is `enemy is close and being attacked`.** Plain `enemy is close`
  fired on every cooldown beside a tanked boss, which is immune, and would knock trash off the tank.
  The `high mana` node is separate.
- **Heroism and Bloodlust skip while the bot is Sated (57724) or Exhausted (57723)**: another lust
  already landed, so the cast only burned a GCD. Shared `ShamanBoostTrigger`, so every spec.
- **Trap: Glyph of Totem of Wrath's 5 min self-buff (63283) is also named "Totem of Wrath".** A
  name check held the totem back after Fire Elemental's 2 min until the glyph buff ran out, so
  `CastTotemOfWrathAction` reads the totem's spell power auras by id (57658, 57660, 57662, 57663).
- **Call of the Elements sits at 59.5, below the `set <totem>` nodes (60).** Tied, it could win on a
  fresh character's empty totem bar, summon nothing and cost a GCD.

Not the bot's: the sim swaps in Searing Totem outside Fire Elemental while still granting Totem of
Wrath and its glyph's spell power (`Shaman.Reset`).

## Known follow-ups, deliberately out of scope

- `PartyMemberToHeal` is uncached — a full group scan with LOS raycasts on every `Get()`. Real perf
  issue, separate change.
- The `AOE_HEAL_ACTION` macro drops its `estAmount` / `manaEfficiency` arguments, so AoE heal actions
  built through it get defaults.
- Duplicate `cleanse spirit` trigger nodes registered by both `RestoShamanStrategy` (52) and
  `ShamanCureStrategy` (23) — harmless, wasted trigger evaluation only.
- Resto's default water totem is Mana Spring rather than Healing Stream.
- Purge (`TargetAuraDispelTrigger`) fires on any dispellable positive magic aura on the current
  target, so a long self-buff like Thorns draws a Purge every GCD.
