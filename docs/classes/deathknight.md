# Death Knight — Frost

Engine semantics are in [../engine/action-selection.md](../engine/action-selection.md).

## Spec detection

- **The spec-tab fallback is Frost.** `AiFactory::GetPlayerSpecTab` falls back whenever `level < 10`
  or the talent-point sum is 0, which also happens when `GetActiveSpecMask()` does not match the
  stored `PlayerTalent::specMask` (after a talent reset or a dual-spec switch). A Blood fallback gave
  frost-talented bots `blood`, `tank assist`, `pull`, `pull back`, and `ResetStrategies` re-derived it.
- **`IsTank(player, bySpec = true)` reads only the Blood tab, never Frost Presence**: `BloodDKStrategy`'s
  nodes carry a `frost presence` prerequisite, so an aura clause made a DK a tank once Blood ran,
  whatever its talents.
  Unlike druid feral there is no shared-tree case needing an aura probe. The `bySpec = false` default
  (gear weighting) reads `ContainsStrategy(STRATEGY_TYPE_TANK)`.

## Frost rotation

Everything sits **below** the disease triggers at `ACTION_HIGH + 2` (22): Howling Blast does not
apply Frost Fever without its glyph, so Icy Touch and Plague Strike get first refusal. With the glyph
the diseases are already up, so nothing is lost. Unbreakable Armor shares the frost rune with Icy
Touch, which is why it sits under them too.

| Rel | Trigger | Action | Why |
|---|---|---|---|
| 26 | `army of the dead` | army of the dead | |
| 22 | `icy touch` / `plague strike` | icy touch / plague strike | Diseases first. |
| 21.5 | `unbreakable armor` | unbreakable armor | `BuffTrigger`: on cooldown. Kept **out** of the burst registry as tank mitigation. |
| 21 | `killing machine` | frost strike | Guaranteed crit on a runic-power dump wastes no rune; KM before Rime. |
| 20 | `freezing fog` | howling blast | Rime makes HB rune-free; consume before it expires. |
| 19 | `high runic power` | frost strike | ≥ 60 RP (stored ×10, `600`): FS costs 40 and an Obliterate returns 20-25, so waiting longer caps. |
| 18 | `frost and unholy runes` | obliterate | Main rune spender, fired when the runes are up. |
| 5.2 | `high blood rune` | blood strike | |

AoE (`frost aoe`): `killing machine on aoe` → Howling Blast 25, `medium aoe` → Howling Blast 24. On
several targets a KM crit is worth more on HB; the single-target node picks the proc up while HB is on
cooldown.

Defaults are the fallback ladder: Obliterate 5.7 / Frost Strike 5.4 / Horn of Winter 5.1 / melee 5.0.

**Rune gating lives in the triggers** ([why](../engine/action-selection.md#casting)): Obliterate
reports castable with no runes, wins the tick at 5.7, fails, and burns a queue iteration.
`frost and unholy runes` is `ObliterateRunesTrigger`: runes ready within 2 s, a death rune covering
either half but not both. `HowlingBlastTrigger` was deleted: unregistered, and a `DebuffTrigger` on a
"howling blast" debuff that does not exist.

## Open

- **Pestilence** is reachable only via `PestilenceGlyphTrigger`, which hard-requires Glyph of Disease
  (aura 63334). Without it, Icy Touch / Plague Strike only refire once the disease has fully dropped.
  The guide's core cycle assumes the glyph — either confirm the frost premade spec carries it or
  accept a one-GCD disease gap per cycle. Related: `Playerbots.PremadeSpecGlyph.6.1` contains item
  `43544`, which falls in the DK glyph id range and may be Glyph of Howling Blast — resolve against
  `item_template`. That is a config decision; the priorities above work either way.

## Out of scope, flagged

- **Blood as a DPS tree is not representable.** `"blood"` maps unconditionally to `BloodDKStrategy`
  (`STRATEGY_TYPE_TANK`), and `"tank"` is an alias for it.
- `CastKillingMachineAction` and the `"killing machine"` / `"improved icy talons"` **action** nodes
  cast passive proc auras and can never succeed. Left alone — nothing emits them. The trigger
  namespace is separate from the action namespace for the same name.
- `FrostDKAoeStrategy::InitTriggers` does not chain to `CombatStrategy::InitTriggers`; matches
  `UnholyDKAoeStrategy`, so left consistent.

## Confirmed correct — do not re-audit

Blood Presence, Empower Rune Weapon on `no rune`, Army of the Dead, and Obliterate rune accounting
(`ObliterateRunesTrigger` handles death runes).
