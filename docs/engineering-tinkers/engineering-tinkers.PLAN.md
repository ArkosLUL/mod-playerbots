# Engineering tinkers for playerbots — firing action + enchant scoring

## Context

Today bots never fire an engineering tinker, and the gear factory never applies the on-use ones.

Two separate gaps:

1. **No firing action.** Nothing in `mod-playerbots` casts an enchantment's on-use spell.
   `UseTrinketAction` (`src/Ai/Base/Actions/GenericSpellActions.cpp:513-665`) is the only on-use item
   path, and it reads only `ItemTemplate->Spells[]` on trinket slots — never `GetEnchantmentId`.
2. **On-use enchants score 0.** `StatsCollector::CollectEnchantStats`
   (`src/Mgr/Item/StatsCollector.cpp:225-269`) handles `COMBAT_SPELL`, `EQUIP_SPELL` and `STAT`;
   `ITEM_ENCHANTMENT_TYPE_USE_SPELL` deliberately falls through to `default: break`, with a comment
   at `:249-251` explaining why: nothing fires them, so picking one would waste a slot. The strict
   `>` compare in `PlayerbotFactory.cpp:5439` then guarantees a 0-scoring enchant never wins.

`docs/profession-gear-enhancements/profession-gear-enhancements.PLAN.md:143-158` records that
USE_SPELL scoring was implemented and then reverted for exactly this reason, and names the reopen
condition: add the firing action first, then re-add scoring. That is what this plan does.

Result today: Hyperspeed Accelerators and Hand-Mounted Pyro Rocket are never applied; Nitro Boosts
is applied only on the strength of its +24 crit half; Flexweave Underlay / Springy Arachnoweave are
applied because they are `EQUIP_SPELL`/`STAT`, and can beat a tailor's embroidery.

Desired outcome:
- Bots with Engineering apply and actively use Hyperspeed Accelerators.
- Hyperspeed always beats Hand-Mounted Pyro Rocket on gloves.
- Tailoring's exclusive cloak embroideries always beat engineering cloak tinkers.

## Verified facts (checked against this tree, not assumed)

- `WorldSession::HandleUseItemOpcode` (`src/server/game/Handlers/SpellHandler.cpp:58-202`) does
  **not** validate the packet's `spellId` against `proto->Spells[]`. It only requires the spell to
  exist, the item to be equipped and usable, then calls `Player::CastItemUseSpell`.
- `Player::CastItemUseSpell` (`src/server/game/Entities/Player/Player.cpp:7671-7719`) iterates
  `MAX_ENCHANTMENT_SLOT`, finds `ITEM_ENCHANTMENT_TYPE_USE_SPELL`, checks `HasSpellCooldown`
  itself, runs `CheckCast`, and casts with `m_CastItem = item`. So a `CMSG_USE_ITEM` aimed at
  equipped gloves fires the tinker. The prior plan doc's claim that this path is unreachable for
  bots is wrong — `UseTrinketAction` already reaches it.
- Enchant spell ids confirmed from `acore_world.item_template` (the tinkers are crafted consumables
  whose `spellid_1` is the apply-enchant spell): Hyperspeed Accelerators `54758` (item 41093),
  Hand-Mounted Pyro Rocket `54998` (41091), Flexweave Underlay `55002` (41111), Nitro Boosts
  `55016` (41118), Personal EMP Generator `54736` (40776).
  **These ids are for reference only — the design below needs none of them hardcoded.**
- `CollectSpellStats` (`src/Mgr/Item/StatsCollector.cpp:94-223`) already takes a `spellCooldown` and
  amortises auras by uptime coverage (`:165-172`). Reuse it; do not write new amortisation math.

## Design

Three changes, in this order. No spell ids are hardcoded — both of the user's ranking constraints
fall out of structural rules.

### 1. `UseTinkerAction` — fire on-use enchants

New action, modelled closely on `UseTrinketAction` (`GenericSpellActions.cpp:527-665`). Put it
beside that class in `src/Ai/Base/Actions/GenericSpellActions.{h,cpp}`.

- `Execute`: walk `EQUIPMENT_SLOT_HANDS`, `WAIST`, `FEET`, `HEAD`, `BACK` via
  `bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)`; first successful use returns true.
- Per item: `item->GetEnchantmentId(EnchantmentSlot(e))` for each `e < MAX_ENCHANTMENT_SLOT` →
  `sSpellItemEnchantmentStore.LookupEntry` → for each `s`, take `spellid[s]` where
  `type[s] == ITEM_ENCHANTMENT_TYPE_USE_SPELL`.
- **Usefulness gate (this is what keeps Pyro Rocket and Nitro Boosts from firing):** require
  `spellInfo->IsPositive()` and at least one `SPELL_EFFECT_APPLY_AURA` whose `ApplyAuraName` is a
  combat-stat aura — `SPELL_AURA_MOD_RATING`, `MOD_STAT`, `MOD_ATTACK_POWER`,
  `MOD_RANGED_ATTACK_POWER`, `MOD_DAMAGE_DONE`, `MOD_HEALING_DONE`, `MOD_INCREASE_HEALTH`,
  `MOD_RESISTANCE`. Movement-speed, feather-fall and pure-damage effects fail this and are skipped.
  Factor the predicate into a free function so step 2 can share it.
- Cooldown: `HasSpellOrCategoryCooldown(bot, spellId)` (already used at
  `GenericSpellActions.cpp:510`) plus `bot->IsNonMeleeSpellCast(true)` and
  `botAI->CanCastSpell(spellId, bot, false, nullptr, item)`, as `UseTrinket` does.
  Keep a local `spellId -> expiry` map mirroring `trinketItemCooldownExpiries`, seeded after the
  cast from `bot->GetSpellCooldownDelay(spellId)`. **Watch this in testing:** if the enchant spell
  carries no `RecoveryTime`/`CategoryRecoveryTime`, `GetSpellCooldownDelay` returns 0 and the bot
  will retry every tick — if that happens, fall back to a fixed cooldown constant in the action.
- Fire it exactly like `GenericSpellActions.cpp:640-646`:
  ```cpp
  WorldPacket packet(CMSG_USE_ITEM);
  packet << bagIndex << slot << cast_count << spellId << item_guid << glyphIndex << castFlags;
  packet << uint32(TARGET_FLAG_NONE) << bot->GetPackGUID();
  bot->GetSession()->HandleUseItemOpcode(packet);
  ```
  `bagIndex = item->GetBagSlot()`, `slot = item->GetSlot()`.

Wiring:
- `src/Ai/Base/ActionContext.h` — `creators["use tinker"]`, next to the `"use trinket"` entry
  (`:183`, `:395`).
- `src/Ai/Base/Strategy/RacialsStrategy.cpp:106-107` — add
  `NextAction("use tinker", ACTION_NORMAL + 4)` on the `"generic boost"` trigger, alongside
  `use trinket`.
- `src/Ai/Base/Combat/BurstCooldowns.cpp` — add `"use tinker"` to the burst-cooldown name list so
  `HoldBurstUntilTankEngagedMultiplier` (`BurstWindowStrategy.cpp:12-62`) holds it for the boss
  pull. It is a pure dps cooldown, so it needs **no** exemption like the `"use trinket"` one at
  `BurstWindowStrategy.cpp:26`.

### 2. Score `ITEM_ENCHANTMENT_TYPE_USE_SPELL`

`src/Mgr/Item/StatsCollector.cpp:249` — replace the "stays unscored on purpose" comment with a real
case:

```cpp
case ITEM_ENCHANTMENT_TYPE_USE_SPELL:
{
    // Only tinkers the bot will actually press (see UseTinkerAction's gate); anything else
    // would trade a stat enchant for a button nobody uses.
    if (!IsUsableTinkerSpell(enchant_spell_id))
        break;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(enchant_spell_id);
    uint32 cd = std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime);
    CollectSpellStats(enchant_spell_id, 1.0f, Milliseconds(cd));
    break;
}
```

The cooldown amortisation at `:165-172` then values Hyperspeed at roughly its uptime share of 340
haste. Hand-Mounted Pyro Rocket is damage-only, fails the shared predicate, and stays at 0 — so
**Hyperspeed always outscores it on gloves**, with no id list. Nitro Boosts keeps scoring on its
+24 crit half only, unchanged.

Nothing in `PlayerbotFactory::ApplyEnchantAndGemsNew` needs to change: tinkers are already in
`enchantSpellIdCache` (`PlayerbotFactory.cpp:527-546`) and already clear the `enchant->requiredSkill`
Engineering gate at `:5427`. They only ever lost on score.

### 3. Engineering never wins the cloak slot

In the per-slot enchant loop (`src/Bot/Factory/PlayerbotFactory.cpp:5390-5445`), after the
`sSpellItemEnchantmentStore` lookup at `:5422` and next to the existing `requiredSkill` check at
`:5427`, skip engineering enchants on cloaks:

```cpp
// Tailoring's exclusive embroideries are the intended cloak enchant; engineering's cloak
// tinkers are a parachute with a stat rider and would otherwise beat them on raw stats.
if (enchant->requiredSkill == SKILL_ENGINEERING &&
    item->GetTemplate()->InventoryType == INVTYPE_CLOAK)
    continue;
```

Structural, so no ids to keep in sync. A tailor then gets an embroidery; a non-tailor engineer
falls back to a normal cloak enchant (Major Agility +22 agi vs Flexweave's +23 agi — the loss is
noise, and it removes an unusable parachute).

## Files to touch

| File | Change |
| --- | --- |
| `src/Ai/Base/Actions/GenericSpellActions.h` | `UseTinkerAction` class + shared `IsUsableTinkerSpell` decl |
| `src/Ai/Base/Actions/GenericSpellActions.cpp` | action implementation + predicate |
| `src/Ai/Base/ActionContext.h` | register `"use tinker"` |
| `src/Ai/Base/Strategy/RacialsStrategy.cpp` | hang it off `"generic boost"` |
| `src/Ai/Base/Combat/BurstCooldowns.cpp` | add `"use tinker"` to the burst list |
| `src/Mgr/Item/StatsCollector.cpp` | `USE_SPELL` scoring case |
| `src/Bot/Factory/PlayerbotFactory.cpp` | cloak-slot engineering skip |
| `docs/profession-gear-enhancements/profession-gear-enhancements.PLAN.md` | update the "DROPPED" section — the reopen condition is now met |

## Verification

Static (this environment cannot compile the module — see prior sessions):

1. `IsUsableTinkerSpell` is the single predicate used by both the action and the scorer; grep that
   there is no second copy.
2. Confirm `SKILL_ENGINEERING` and `INVTYPE_CLOAK` are reachable from `PlayerbotFactory.cpp`'s
   existing includes (`SharedDefines.h` / `ItemTemplate.h`).
3. Re-read the prior-art doc's "DROPPED" section and confirm the stated reopen condition is
   satisfied by step 1.

In-game (needs a build on the user's side):

1. `.playerbot bot add <name>` an 80 with Engineering ≥ 400; confirm gloves show Hyperspeed
   Accelerators after `AutoMaintenanceOnLevelupAction` / a factory randomise.
2. Confirm the same bot's cloak carries a tailoring embroidery (if it is also a tailor) or a normal
   cloak enchant — never Flexweave/Springy.
3. Confirm no bot ever gets Hand-Mounted Pyro Rocket.
4. Pull a dungeon boss and watch the combat log for the Hyperspeed Accelerators haste buff, once per
   ~60s, held until the tank engages (burst-window behaviour).
5. Watch `Player::GetSpellCooldownDelay` behaviour — if the log shows the buff reapplying every
   tick, the enchant spell has no cooldown and the fallback constant is needed.

## Out of scope

Frag Belt, Personal EMP Generator, Mind Amplification Dish and the parachute tinkers stay unused and
unscored; they are targeted/utility effects that would need their own targeting logic.
