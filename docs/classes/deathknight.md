# Death Knight — Frost

Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Why a Frost DK ran the tank rotation

Two defects reinforced each other, and the symptom was a fully frost-talented bot casting Death
Strike, sitting in Frost Presence, and pulling aggro while `co ?` showed `frost` alongside
`tank assist`, `tank face`, `pull`, `pull back`.

- **DK spec-tab detection defaulted to blood.** `AiFactory::GetPlayerSpecTab` returns the fallback tab
  whenever `level < 10` or the talent-point sum is 0 — which also happens when `GetActiveSpecMask()`
  does not match the stored `PlayerTalent::specMask`, e.g. after a talent reset or a dual-spec switch.
  The fallback `switch` handled mage/paladin/priest/warlock only, so DK fell through to `tab = 0` =
  blood, and `AddDefaultCombatStrategies` then applied `"blood", "tank assist", "pull", "pull back"`.
  Death Strike has only two emitters, both in `BloodDKStrategy` at relevance 23 and 21 — an order of
  magnitude above `FrostDKStrategy`'s defaults (5.0-5.7) — so blood dominated any tick it was active.

  **Manually adding `frost` is not a workaround**: it evicts `blood` (sibling context) but leaves
  `tank assist` / `pull` / `pull back` / `tank face`, which are not siblings. And `ResetStrategies`
  re-derives the whole wrong set from the same bad tab.
- **Frost Presence made any DK a tank.** `IsTank(player, bySpec = true)` returned true for
  `tab == BLOOD || HasAura(SPELL_DK_FROST_PRESENCE)`, and `BloodDKStrategy`'s nodes carry a
  `frost presence` prerequisite — so blood force-cast Frost Presence, which classified the bot as a
  tank regardless of talents. Self-reinforcing. The aura clause is gone; blood is unambiguously the
  tank tree, and unlike druid feral there is no shared-tree case needing an aura probe.

Blast radius of that change is small: only `bySpec = true` callers reach the branch. The
`bySpec = false` default used by gear weighting resolves through `ContainsStrategy(STRATEGY_TYPE_TANK)`
and is unaffected.

## Frost rotation

Everything added sits **below** the existing disease triggers at `ACTION_HIGH + 2` (22): Howling Blast
does not apply Frost Fever without its glyph, so Icy Touch and Plague Strike must always get first
refusal. The ordering is glyph-independent — if the bot does carry the glyph, diseases are already up
from Icy Touch and nothing is lost.

| Rel | Trigger | Action | Why |
|---|---|---|---|
| 21 | `freezing fog` | howling blast | Rime proc makes HB rune-free; consume before it expires. Was 5.5, losing to Obliterate at 5.7. |
| 20 | `killing machine` | frost strike | Guaranteed-crit proc spent on a runic-power dump, so no rune is wasted. |
| 19 | `high runic power` | frost strike | Stops RP capping. |
| 18 | `frost and unholy runes` | obliterate | Main rune spender, fired when the runes are actually up. |

Defaults stay as the fallback ladder: Obliterate 5.7 / Frost Strike 5.4 / Horn of Winter 5.1 /
melee 5.0.

**Rune gating has to live in the triggers** because `CanCastSpell` builds its probe with
`TRIGGERED_IGNORE_POWER_AND_REAGENT_COST` — so Obliterate reports castable with no runes available,
wins the tick at 5.7, fails, and burns a queue iteration before Frost Strike gets a turn. Runic power
is stored ×10, so 80 RP is `800`.

`HighRunicPowerTrigger` and `"frost and unholy runes"` (a `TwoTriggers` of `high frost rune` +
`high unholy rune` — Obliterate costs one of each) are new registrations. `KillingMachineTrigger`
already existed but was never registered. `HowlingBlastTrigger` was deleted: unregistered dead code,
and wrong as written — a `DebuffTrigger` keyed on a debuff named "howling blast", which does not
exist.

## Open

- **Killing Machine routes to Frost Strike, including on AoE.** The guide's filler order is KM Frost
  Strike *then* Rime Howling Blast, but Rime is priced above KM here so Rime always wins; and on AoE
  the guide sends KM procs into Howling Blast.
- **Unbreakable Armor sits at 5.6**, below the default Obliterate at 5.7, so it fires only when
  Obliterate is unusable. It is a cooldown and should be used on cooldown. Note it is deliberately
  **excluded** from the burst registry as tank mitigation.
- `HighRunicPowerTrigger` requires ≥80 RP before Frost Strike; the guide spends earlier to avoid
  capping.
- **Pestilence** is reachable only via `PestilenceGlyphTrigger`, which hard-requires Glyph of Disease
  (aura 63334). Without it, Icy Touch / Plague Strike only refire once the disease has fully dropped.
  The guide's core cycle assumes the glyph — either confirm the frost premade spec carries it or
  accept a one-GCD disease gap per cycle. Related: `PremadeSpecGlyph.6.1` contains item `43544`,
  which falls in the DK glyph id range and may be Glyph of Howling Blast — resolve against
  `item_template`. That is a config decision; the priorities above work either way.

## Out of scope, flagged

- **Blood as a DPS tree is not representable.** `"blood"` maps unconditionally to `BloodDKStrategy`
  (`STRATEGY_TYPE_TANK`), and `"tank"` is an alias for it.
- `CastKillingMachineAction` and the `"killing machine"` / `"improved icy talons"` **action** nodes
  cast passive proc auras and can never succeed. Left alone — nothing emits them. (Note the trigger
  namespace is separate from the action namespace for the same name.)
- `FrostDKAoeStrategy::InitTriggers` does not chain to `CombatStrategy::InitTriggers`; matches
  `UnholyDKAoeStrategy`, so left consistent.

## Confirmed correct — do not re-audit

Blood Presence, Empower Rune Weapon on `no rune`, Army of the Dead, and Obliterate rune accounting
(`ObliterateRunesTrigger` handles death runes).
