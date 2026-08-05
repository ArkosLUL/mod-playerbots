# Shaman — Restoration

Engine and healer semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Root causes

1. **The whole heal chain was priced below emergency.** Hand-written literals put the critical band at
   23–25, where every other healer opens at 34–36 (`ACTION_CRITICAL_HEAL` is 30). So
   `earth shield on main tank` (27), group-heal riptide/chain heal (27/26), `stoneclaw totem` (40) and
   `call of the elements` (60) all outranked an emergency heal on a dying player, and `water shield`
   (19.5) even outranked the low-health riptide (19.0).
2. **Chain Heal was effectively disabled.** It appeared on exactly one node, `group heal setting`,
   whose `AoeInGroupTrigger` bailed below 5 nearby members and then wanted 3/5/10/15 injured by group
   size — ten players below 85% HP in a 25-man before a single Chain Heal. Resto was the only healer
   spec with no `medium group heal setting` node at all.
3. **No spell differentiation between bands** — all four used the same riptide → healing wave →
   lesser healing wave chain, differing only by relevance.
4. **A mana dead zone where the bot cast nothing.** The non-tank branch of
   `HealerAutoSaveManaMultiplier` had no health check on its second veto, so **any MEDIUM-or-lower
   efficiency heal was vetoed at any target health, including 5%**. Below 60% bot mana on a non-tank
   between 50% and 65% HP with riptide already up, lesser healing wave, healing wave and riptide were
   all vetoed and the bot fell through to Lightning Bolt.
5. **No healing cooldowns existed.** Nature's Swiftness (16188) and Tidal Force (55198) are both
   Restoration talents and appeared nowhere in the Shaman tree. Everything else in the WotLK resto
   toolkit was already present at the action layer.

Greater Healing Wave does **not** exist for Shaman in WotLK 3.3.5.

## Current ladder

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

- **Riptide leads every band** — instant, and it grants Tidal Waves, which speeds the Healing Wave or
  crits the Lesser Healing Wave that follows. The pre-existing riptide-first ordering was
  accidentally correct.
- **Chain Heal is now in all four bands**, the change that matters most. It is also the only heal that
  survives the save-mana veto in every situation (HIGH efficiency, low `estAmount`), so it doubles as
  the low-mana backstop — which is why the **critical band ends with chain heal, not lesser healing
  wave**.
- **Medium band puts lesser healing wave above healing wave** — a 2.5s Healing Wave is the wrong tool
  for topping off a 60% target.
- **`call of the elements` drops 60 → 21.** Its trigger fires whenever ≥2 totem slots are empty or out
  of range, so at 60 it re-dropped the whole totem set mid-fight while a raider sat at 10% HP.

**Half-step values are collision breakers, not design choices.** A resto shaman's engine also runs
`GenericShamanStrategy` (wind shear 23), `ShamanCureStrategy` (cleanse spirit 24, cleanse on party
23) and the four totem strategies (`no X totem` re-drops at 55, `set X totem` at 60), so the values
that would have collided were nudged. Band ordering is unchanged.

## `estAmount` / `manaEfficiency` tuning

These two fields are read **only** by `HealerAutoSaveManaMultiplier`; `estAmount` is compared against
`lossAmount = 100 - targetHealthPct`.

| Action | Before | After | Why |
|---|---|---|---|
| Lesser Healing Wave (party) | `25.0f, LOW` | `25.0f, MEDIUM` | LOW is vetoed at any health below 60% bot mana on non-tanks |
| Healing Wave (party) | `50.0f, MEDIUM` | `40.0f, MEDIUM` | 50 vetoed the cast unless the target was under 50% HP; 40 lets it fire from 60% down |
| Chain Heal | `15.0f, HIGH` | `20.0f, HIGH` | Keeps HIGH (the low-mana backstop) while reflecting that a bounce is worth more than a Riptide tick |
| Riptide (party) | `15.0f, VERY_HIGH` | unchanged | Correct as-is |

Nature's Swiftness and Tidal Force resolve by spell **name** against the bot's own known spells, so no
id constants are needed and untalented bots simply fail `CanCastSpell`. Registration must be in the
**Shaman** context — the Druid registration is not visible to Shaman bots, and a missing registration
is a silent runtime no-op.

The out-of-combat strategy had the same Chain-Heal problem in miniature and got `chain heal on party`
added to all four bands, one step below riptide.

## Shared infrastructure changed here — read before touching healers

These three moved **all four healer specs**.

- **`AoeInGroupTrigger` thresholds.** Old: hard bail at `member < 5`, thresholds 3/5/10/15. New: bail
  at `< 3`, thresholds `≤5 → 3`, `≤10 → 4`, `≤25 → 6`, else 8. Critically, the size the thresholds
  scale from comes from `CountHealableGroupMembers` (alive, inside `healDistance`), not
  `GetNearGroupMemberCount` (`sightDistance`, counts the dead) — **both sides of the comparison must
  measure the same population** or the threshold is unreachable in a spread 25-man. Blast radius:
  Chain Heal, Circle of Healing, Prayer of Healing, Wild Growth and Tranquility now fire at realistic
  raid damage levels instead of only during a near-wipe.
- **`AoeHealValue` counting radius.** The counter swept `sightDistance` (100 yd) with no LOS check,
  counting players the bot cannot heal. Now `healDistance` plus `IsWithinLOSInMap`, matching
  `PartyMemberToHeal::Check`. The raycast runs last, after the alive/range/health-band filters, so
  only members actually hurt and in range cost a VMap query. **This must land together with the
  threshold change** — the two offset each other, and together make the trigger mean "N people I can
  actually heal are hurt".
- **Save-mana health guard** — added `health >= lowHealth &&` to the non-tank branch's second
  condition, mirroring the tank branch. Below 45% target HP, mana conservation stops vetoing heals
  entirely. Equally benefits priest Flash Heal, paladin Holy Shock and druid Nourish.

## `PartyMemberToProtect` revival

A pre-existing bug unrelated to Resto: `PartyMemberToProtect::Calculate` had `return nullptr;` as its
**first statement**, making the rest of the body unreachable and `ProtectPartyMemberTrigger`
permanently false. Three emergency actions had therefore never fired: Disc Priest
`pain suppression on party` @90, Paladin `blessing of protection on party` @93, Tank Warrior
`intervene` @90.

Reviving it as written would hand a tank below 10% HP to Blessing of Protection — BoP wipes the
tank's melee threat and applies Forbearance, which is **worse than the bug**. So the revival needed a
per-spell target filter.

**`PartyMemberValue` derives from `UnitCalculatedValue`, not `Qualified`**, so the filter is exposed
as a **second registered value name**, `"party member to protect no tank"`, rather than a qualifier.
Paladin's BoP and warrior's Intervene point at that name; Intervene additionally requires
`AI_VALUE(uint8, "my attacker count") == 0`, without which a tank warrior charges off the boss to any
raider below 30%. **Priest deliberately stays on the generic value** — Pain Suppression on a dying
tank is the intended use — and `ProtectPartyMemberTrigger` also stays generic, so the paladin and
warrior nodes still fire on the shared trigger and simply report `isUseful() == false` when the
candidate is not for them.

## Known follow-ups, deliberately out of scope

- `PartyMemberToHeal` is uncached — a full group scan with LOS raycasts on every `Get()`. Real perf
  issue, separate change.
- The `AOE_HEAL_ACTION` macro drops its `estAmount` / `manaEfficiency` arguments, so AoE heal actions
  built through it get defaults.
- Duplicate `cleanse spirit` trigger nodes registered by both `RestoShamanStrategy` (52) and
  `ShamanCureStrategy` (23) — harmless, wasted trigger evaluation only.
- Resto's default water totem is Mana Spring rather than Healing Stream.
