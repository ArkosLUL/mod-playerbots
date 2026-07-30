# Fix: expansion-gate offensive potions + stop ammo spam on no-ammo bows

## Context

Two unrelated live bugs reported against the playerbot module:

1. **Level-70 bots get WotLK offensive potions.** The DPS offensive-potion feature
   (commit `3c2db4a34`) picks from a hardcoded descending ladder and gates only on
   `proto->RequiredLevel <= bot level`. Potion of Speed (40211, req 70) and Potion of Wild
   Magic (40212, req 68) therefore win for a level 68-70 bot, so bots stuck at the TBC level
   cap drink WotLK consumables. Every other item category in the module (gear, ammo,
   enchants) already refuses next-expansion items via the `AiPlayerbot.LimitGearExpansion` /
   `LimitEnchantExpansion` item-id thresholds; the potion ladder was never wired into that.

2. **Bots with Thori'dal, the Stars' Fury spam "My bags are full".** Thori'dal is
   `ITEM_SUBCLASS_WEAPON_BOW`, so every ammo code path in the module treats it as an
   arrow-consuming bow. Its equip aura 46699 ("Requires No Ammo") makes the core reject all
   ammo: `Player::CanUseAmmo` returns `EQUIP_ERR_BAG_FULL6`
   ([PlayerStorage.cpp:2590](src/server/game/Entities/Player/PlayerStorage.cpp#L2590)), which
   maps to the same string as `EQUIP_ERR_BAG_FULL`
   ([InventoryChangeFailureAction.cpp:81](modules/mod-playerbots/src/Ai/Base/Actions/InventoryChangeFailureAction.cpp#L81)).
   The aura also permanently clears `PLAYER_AMMO_ID`, so the bot never converges: it restocks
   arrows, tries `SetAmmo`, fails, and repeats forever.

Outcome wanted: level-capped bots stock era-appropriate offensive potions, and a bot holding a
no-ammo ranged weapon stops buying, stocking, equipping and complaining about ammo.

## Decisions locked with the user

- Gate potions on **bot level + `AiPlayerbot.LimitGearExpansion`**, reusing the existing
  item-id threshold idiom. Not on the server's `CONFIG_EXPANSION`.
- Scope the gating fix to the **offensive-potion ladder only**. The heal/mana path
  (`RandomItemMgr::GetRandomPotion`) and food/oils are out of scope this round.
- **No use-time gate.** `CleanupConsumables()` wipes all subclass-POTION items right before
  `InitPotions()` runs on maintenance/level-up
  ([AutoMaintenanceOnLevelupAction.cpp:170-176](modules/mod-playerbots/src/Ai/Base/Actions/AutoMaintenanceOnLevelupAction.cpp#L170-L176)),
  so stale WotLK potions clear themselves on the next restock.

---

## Part 1 — Expansion-gate the offensive potion ladder

### 1.1 Extract the existing gate into a reusable helper

The rule already exists, inlined inside `RandomItemMgr::GetAmmo`
([RandomItemMgr.cpp:971-987](modules/mod-playerbots/src/Mgr/Item/RandomItemMgr.cpp#L971-L987)),
with the same two magic ids duplicated again for gear at
[PlayerbotFactory.cpp:2254-2259](modules/mod-playerbots/src/Bot/Factory/PlayerbotFactory.cpp#L2254-L2259).
Pull it out rather than adding a third copy.

Add to [RandomItemMgr.h](modules/mod-playerbots/src/Mgr/Item/RandomItemMgr.h) in the public
static block next to `IsValidItem` / `IsInternalItem` (~line 214):

```cpp
[[nodiscard]] static bool IsAllowedForLevelExpansion(uint32 itemId, uint32 level);
```

Implement in [RandomItemMgr.cpp](modules/mod-playerbots/src/Mgr/Item/RandomItemMgr.cpp),
moving the two constants out of `GetAmmo` to file scope:

```cpp
// Item ids are roughly chronological, so the first id of each expansion's content doubles as a
// cutoff for "this item didn't exist yet at that level cap".
static constexpr uint32 EXPANSION_ITEM_ID_TBC   = 23728; // approx. first item in TBC content (patch 2.0)
static constexpr uint32 EXPANSION_ITEM_ID_WOTLK = 35570; // approx. first item in WotLK content (patch 3.0)

bool RandomItemMgr::IsAllowedForLevelExpansion(uint32 itemId, uint32 level)
{
    if (!sPlayerbotAIConfig.limitGearExpansion)
        return true;

    uint32 const maxEntryId = level <= 60 ? EXPANSION_ITEM_ID_TBC :
                              level <= 70 ? EXPANSION_ITEM_ID_WOTLK :
                              std::numeric_limits<uint32>::max();
    return itemId < maxEntryId;
}
```

Then rewrite `GetAmmo`'s tail to use it (behaviour identical):

```cpp
std::vector<uint32> const& ammo = subItr->second;
for (uint32 entry : ammo)
    if (IsAllowedForLevelExpansion(entry, level))
        return entry;
return 0;
```

Note `GetAmmo` normalizes `level` through `NormalizeLevel` before this point — keep that
ordering.

### 1.2 Apply the gate in the potion ladder

In `PlayerbotFactory::InitPotions()`
([PlayerbotFactory.cpp:3868-3881](modules/mod-playerbots/src/Bot/Factory/PlayerbotFactory.cpp#L3868-L3881)),
add one condition to the existing skip:

```cpp
for (uint32 itemId : ladder)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto || proto->RequiredLevel > level)
        continue;

    if (!RandomItemMgr::IsAllowedForLevelExpansion(itemId, level))
        continue;
    ...
```

`level` here is the factory's member (bot level), which is what the ladder already compares
against. The `continue` (not `break`) is what makes the ladder fall through to the previous
expansion's entry.

### 1.3 Resulting behaviour

| Bot | Before | After (LimitGearExpansion=1) |
|---|---|---|
| lvl 70 caster DPS | Potion of Wild Magic (40212) | Destruction Potion (22839) |
| lvl 70 str melee  | Potion of Speed (40211)      | Insane Strength Potion (22828) |
| lvl 70 agi melee  | Potion of Speed (40211)      | Haste Potion (22838) |
| lvl 80 any        | unchanged                     | unchanged |
| LimitGearExpansion=0 | unchanged                  | unchanged |

**Known limitation to state, not fix:** ids 22828/22838/22839 sit below the 23728 TBC cutoff,
so a level-60 bot still gets these TBC-recipe potions. Making that exact would need a
per-item expansion tag rather than an id threshold, and it matches how gear/ammo already
behave. Out of scope.

---

## Part 2 — Suppress ammo handling for "requires no ammo" ranged weapons

### 2.1 Shared predicate

Add to [PlayerbotAI.h](modules/mod-playerbots/src/Bot/PlayerbotAI.h) next to the consumable id
enums / `IsOffensivePotionId` (~line 240):

```cpp
// Thori'dal and friends conjure their own ammo via this equip aura. Player::CanUseAmmo then
// rejects every SetAmmo with EQUIP_ERR_BAG_FULL6, which the bot reports as "My bags are full".
constexpr uint32 SPELL_REQUIRES_NO_AMMO = 46699;

inline bool RangedWeaponNeedsAmmo(Player* bot) { return !bot->HasAura(SPELL_REQUIRES_NO_AMMO); }
```

Use the aura rather than `Player::CanUseAmmo`, which also fails while the bot is dead and
would silently disable ammo restocking on corpses.

### 2.2 Guard the five ammo entry points

Each is a one-line early-out; together they cover stocking, buying, equipping, trigger arming
and the chat spam.

| File:line | Change |
|---|---|
| [PlayerbotAI.cpp:5495](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L5495) `FindAmmo()` | `if (!RangedWeaponNeedsAmmo(bot)) return nullptr;` before the ranged-weapon lookup |
| [PlayerbotFactory.cpp:3631](modules/mod-playerbots/src/Bot/Factory/PlayerbotFactory.cpp#L3631) `InitAmmo()` | `if (!RangedWeaponNeedsAmmo(bot)) return;` after the class check — stops the 6000-arrow stock and the direct `SetAmmo` at :3672 |
| [ItemUsageValue.cpp:1348](modules/mod-playerbots/src/Ai/Base/Value/ItemUsageValue.cpp#L1348) `QueryItemUsageForAmmo()` | `if (!RangedWeaponNeedsAmmo(bot)) return ITEM_USAGE_NONE;` — kills both the `ITEM_USAGE_EQUIP` at :1379 and the `ITEM_USAGE_AMMO` vendor purchase at :1399 |
| [EquipAction.cpp:75](modules/mod-playerbots/src/Ai/Base/Actions/EquipAction.cpp#L75) `EquipItem()` INVTYPE_AMMO branch | skip the whole branch (`return;`) when `!RangedWeaponNeedsAmmo(bot)` — this is where the "equipping \<arrow\>" whisper fires unconditionally alongside the failing `SetAmmo` |
| [GenericTriggers.cpp:772](modules/mod-playerbots/src/Ai/Base/Trigger/GenericTriggers.cpp#L772) `AmmoCountTrigger::IsActive()` | `if (!RangedWeaponNeedsAmmo(bot)) return false;` first — otherwise `PLAYER_AMMO_ID == 0` keeps the hunter "no ammo" trigger firing every 10s to do nothing |

`PlayerbotAI.cpp` / `ItemUsageValue.cpp` / `EquipAction.cpp` / `GenericTriggers.cpp` already
include `PlayerbotAI.h`; `PlayerbotFactory.cpp` does too. No new includes expected — confirm
while editing.

### 2.3 Deliberately not changed

- `FindAmmoVisitor` ([ItemVisitors.h:348](modules/mod-playerbots/src/Mgr/Item/ItemVisitors.h#L348))
  stays as-is. It answers "how much ammo is in the bags", which is still a truthful answer;
  the trigger guard above is what stops it mattering.
- The `errorDelay` throttle bug in `PlayerbotMgr::CheckTellErrors`
  ([PlayerbotAIConfig.cpp:83](modules/mod-playerbots/src/PlayerbotAIConfig.cpp#L83) — default
  100ms, integer-divided to 0 seconds, so errors flush every tick) is real and amplifies any
  repeated error, but it is a separate bug. Flag it to the user; don't change it here.
- Leftover arrows already in a Thori'dal bot's bags are not purged. `CleanupConsumables()`
  ([PlayerbotFactory.cpp:4139](modules/mod-playerbots/src/Bot/Factory/PlayerbotFactory.cpp#L4139))
  destroys all `ITEM_CLASS_PROJECTILE` on the next maintenance/level-up anyway.

---

## Verification

Headless build is not available in this environment — static review here, then hand the build
to the user (`./acore.sh compiler build` or the user's usual toolchain).

In-game:

1. **Potions.** Set `AiPlayerbot.LimitGearExpansion = 1`. Take a level-70 caster DPS bot and a
   level-70 fury warrior, run `maintenance`, inspect bags: expect Destruction Potion (22839)
   and Insane Strength Potion (22828), **not** 40212/40211. Level one to 71+ and re-run
   `maintenance`: the WotLK potions should come back. Set the config to 0 and confirm level 70
   goes back to 40211/40212.
2. **Ammo regression check.** A level-70 hunter with an ordinary bow must still stock arrows,
   get `PLAYER_AMMO_ID` set, and buy ammo from vendors — the guard must not fire for it.
3. **Thori'dal.** Give a hunter bot Thori'dal (34334), equip it, run `maintenance`, then pull
   a mob and let it shoot for a few minutes with a master watching. Expect zero "My bags are
   full" and zero "equipping ...Arrow" whispers, no arrow purchases, and ranged attacks still
   working.
4. Repeat step 3 with a thrown weapon and a wand equipped to confirm nothing regressed on the
   already-excluded subclasses.
