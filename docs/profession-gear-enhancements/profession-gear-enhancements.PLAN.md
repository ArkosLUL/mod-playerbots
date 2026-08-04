# Profession-only gear enhancements for WotLK bots (71-80)

## Context

Question: do bots at level 71-80 apply the self-only gear upgrades their professions grant
(Engineering tinkers, Tailoring cloak embroidery, Blacksmithing extra sockets, Jewelcrafting
Dragon's Eye gems, etc.)?

**Answer: no, with one exception.** Enchanting ring enchants are the only profession-restricted
enhancement that reaches bot gear today. Every other profession perk is blocked by one or more of
five independent gaps.

All bot enchanting/gemming happens in exactly one function:
`PlayerbotFactory::ApplyEnchantAndGemsNew` — `src/Bot/Factory/PlayerbotFactory.cpp:5036`.
It runs after `InitSkills` (professions, line 715) and after `InitEquipment` (line 779), so at that
point both the bot's professions and its full gear set are already known — nothing consults the
profession list for gear enhancement.

## Decisions

- Implement items 1, 3, 4 and 5 below. Item 2 (scoring on-use tinkers) is **dropped** — see that
  section.
- Raise the profession skill cap to the expansion max (450 in WotLK). Accepted side effect: bots can
  also learn/craft 401-450 recipes, which affects `GuildTaskMgr` and item-usage logic, not just gear.
- Apply Eternal Belt Buckle to every bot that clears the level gate, not only Blacksmiths.

## Findings

### Per-profession status

| Profession | Enhancement | Status | Blocking gap |
|---|---|---|---|
| Enchanting | Ring enchants (Assault, Greater Spellpower, Stamina) | **Works** | — |
| Engineering | Hyperspeed Accelerators | Applied and used — see item 2 | — |
| Engineering | Hand-Mounted Pyro Rocket | Never applied (damage-only, so it scores 0) | — |
| Engineering | Nitro Boosts | Scored on its STAT half (+24 crit) only | — |
| Engineering | Flexweave Underlay, Springy Arachnoweave (equip enchants) | Never applied — the cloak slot is reserved for tailoring | — |
| Tailoring | Lightweave / Darkglow / Swordguard Embroidery | Scored generically | — |
| Leatherworking | Fur Lining (bracers) | Scored generically | — |
| Blacksmithing | Socket Bracer / Socket Gloves | Impossible | Gap 3 (prismatic) |
| Jewelcrafting | Dragon's Eye gems | Impossible | Gap 4 (unique-equipped) |
| Inscription | Master's Inscription of the Axe/Crag/Pinnacle/Storm | Impossible | Gap 1 (skill cap) |
| *(none — any bot)* | Eternal Belt Buckle | Impossible | Gap 3 (prismatic) |

"Scored generically" means the enchant reaches the scoring path and is valued from its own DBC data;
whether it beats the best plain stat enchant for a given slot and spec is up to that scoring, not to
any profession-specific code. Those rows are not blocked — they were simply never singled out.

Grep confirms zero source hits for any of these spell/item IDs anywhere in the module. (The only
`Nitro` hits — `SPELL_NITRO_BOOTS = 54861` in `src/Ai/Raid/ICC/ICCTriggers.h:158` and
`src/Ai/Raid/RS/RSTriggers.h:97` — are a free movement aura handed to raid bots via `AddAura`,
unrelated to the engineering boot tinker.)

### Gap 1 — profession skill caps at 400, not 450

`src/Bot/Factory/PlayerbotFactory.cpp:3229`:

```cpp
uint32 maxValue = level * 5;   // 400 at level 80; WotLK cap is 450
```

400 clears the 400-skill requirement on tinkers, embroidery, fur lining, and socket bracer/gloves.
It does **not** clear Inscription 430 for Master's Inscription shoulder enchants. Any future
enhancement requiring 401-450 is likewise unreachable.

### Gap 2 — on-use enchants score 0

`StatsCollector::CollectEnchantStats` handles only three display types and silently drops the rest —
`src/Mgr/Item/StatsCollector.cpp:236-264`:

```cpp
case ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL:  // melee, 0.25x
case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:   // 1.0x
case ITEM_ENCHANTMENT_TYPE_STAT:
default: break;                           // <- everything else worth 0
```

On-use tinkers are `ITEM_ENCHANTMENT_TYPE_USE_SPELL`, which lands on `default: break;` and is
therefore worth exactly 0. Consequence at `src/Bot/Factory/PlayerbotFactory.cpp:5223`: a tinker
always loses to a plain stat enchant on the same slot. The profession gate itself is correct and
DBC-driven (`:5214-5219`) — scoring is the only thing stopping these.

Embroidery and fur lining are **not** blocked here. They are `EQUIP_SPELL`, which is handled, and
`HandleApplyAura` follows `SPELL_AURA_PROC_TRIGGER_SPELL` into the triggered spell
(`src/Mgr/Item/StatsCollector.cpp:737-742`), averaging it over the proc's internal cooldown. They
compete on their real value.

Side note: `bestScore` starts at 0 and the comparison is `score >= bestScore`, so on a slot with no
positively-scored enchant the *last* zero-scoring candidate wins arbitrarily.

### Gap 3 — prismatic sockets are never created and never filled

Two separate blockers:

1. The startup enchant cache scans only `SPELL_EFFECT_ENCHANT_ITEM`
   (`src/Bot/Factory/PlayerbotFactory.cpp:490`). `SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC` appears
   nowhere in the module, so Socket Bracer (55628), Socket Gloves (55641) and Eternal Belt Buckle
   can never be applied.
2. Even if a prismatic socket existed, the gem filler only walks `SOCK_ENCHANTMENT_SLOT .. +3` and
   reads colors from the **item template** (`:5242-5244`), so a runtime-added socket is invisible
   to it.

`GetCurrentGemsCount` (`:3937`) already iterates through `PRISMATIC_ENCHANTMENT_SLOT` — the counter
is prismatic-aware, the filler is not.

### Gap 4 — Dragon's Eye gems dropped at cache build

`src/Bot/Factory/PlayerbotFactory.cpp:532-535`:

```cpp
if (proto->HasFlag(ITEM_FLAG_UNIQUE_EQUIPPABLE))
    continue;
```

Every Dragon's Eye carries that flag, so none ever reaches `availableGems`. This makes the existing
jeweler cap dead code — `:5124-5127` checks `ItemLimitCategory == 2` and caps at 3, but nothing with
that category survives the cache filter.

Second problem if the flag filter is lifted: the gem eligibility gate at `:5071` checks only the
*enchant's* `requiredSkill`, which is 0 for Dragon's Eyes — the restriction lives on the gem
**item** (`proto->RequiredSkill == SKILL_JEWELCRAFTING`), which the enchant/gem path never reads.
Without an item-level check, every bot would get JC gems.

### Gap 5 — level-up path skips re-enchanting

`AutoMaintenanceOnLevelupAction` calls `InitEquipment(true)` but never `ApplyEnchantAndGemsNew`
(`src/Ai/Base/Actions/AutoMaintenanceOnLevelupAction.cpp:163-180`), so gear swapped in on level-up
stays bare until a `maintenance` / `autogear` / full randomize runs. Independent of professions, but
it gates when any of the above becomes visible.

## Implementation

Each item is independently shippable.

### 1. Raise the profession skill cap to the expansion max

`SetRandomSkill` (`src/Bot/Factory/PlayerbotFactory.cpp:3227`): cap primary trade skills at the
expansion maximum rather than `level * 5`. Derive from `sWorld->getIntConfig(CONFIG_EXPANSION)`
(already used in `src/Bot/Factory/RandomPlayerbotFactory.cpp:47`) — 300 / 375 / 450. Keep
`level * 5` for weapon and secondary skills so nothing else shifts.

Unblocks Master's Inscription and leaves headroom for the rest.

### 2. Score on-use enchants — DROPPED, then reopened

Scoring `ITEM_ENCHANTMENT_TYPE_USE_SPELL` in `StatsCollector::CollectEnchantStats` was implemented
and then reverted. Reason: nothing in the module ever cast an enchant's on-use spell, so scoring the
tinker made an Engineering bot trade a real stat enchant for a button nobody presses. The stated way
back in was to give bots an action that fires those spells, and only then re-add the scoring.

That is what `docs/engineering-tinkers/engineering-tinkers.PLAN.md` did. `UseTinkerAction` walks the
equipped gear slots, reads `ITEM_ENCHANTMENT_TYPE_USE_SPELL` off each enchantment and fires it with
the same `CMSG_USE_ITEM` packet `UseTrinketAction` uses; `CollectEnchantStats` now scores those
spells, amortized by cooldown. Both sides share `ai::tinker::IsUsableTinkerSpell`, so only tinkers
granting a combat stat aura count — grenades, parachutes and sprints still score 0 and are never
pressed.

One claim in the original write-up was wrong and is worth flagging: `CastItemUseSpell` is **not**
unreachable for bots. `UseTrinketAction` already reaches it, and `WorldSession::HandleUseItemOpcode`
does not validate the packet's `spellId` against `ItemTemplate->Spells[]`.

Shipped from this item: the tie-break in `ApplyEnchantAndGemsNew` is now `score > bestScore`, so a
0-score enchant can no longer win a slot by iteration order.

### 3. Prismatic sockets (Blacksmithing + Eternal Belt Buckle)

How the core actually stores this matters and is easy to get wrong: `PRISMATIC_ENCHANTMENT_SLOT`
holds the socket-*adding* enchant, while the **gem** goes into the first template socket that has no
color, i.e. `SOCK_ENCHANTMENT_SLOT + firstPrismatic` (`ItemHandler.cpp:1244-1263`,
`PlayerStorage.cpp:4435-4442`). The core also rejects meta gems in that socket
(`ItemHandler.cpp:1270`), so it is always a colored socket.

- Build a `prismaticEnchantSpellIdCache` in `Init()` alongside the existing enchant cache, scanning
  `SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC`. Deriving the IDs from the DBC avoids hardcoding enchant IDs
  and picks up whatever the server's data actually has. The cached enchant must also carry an
  `ITEM_ENCHANTMENT_TYPE_PRISMATIC_SOCKET` type — `Spell::EffectEnchantItemPrismatic` refuses
  anything else and logs an error, and setting it anyway would leave a phantom socket.
- New `ApplyPrismaticSocket(Item*)`, called from the slot loop in `ApplyEnchantAndGemsNew`. It gates
  on level, on a free template socket slot, on `IsFitToSpellRequirements` (which restricts each
  enchant to its own slot), and on `enchant->requiredSkill` — the last of which is what makes socket
  bracer/gloves Blacksmith-only while the belt buckle, with no skill requirement, is open to all.
- Level gate: 70 is the hard floor because every socket-adding enchant is WotLK content, and 71 while
  `LimitEnchantExpansion` is on, matching how that config already holds level-70 bots to TBC-era
  enchants and gems. Do not also filter by spell ID inside the loop — the two checks cancel out and
  the feature goes dead at exactly level 70.
- Extend the socket-collection loop to emit a `SocketToGem` at `SOCK_ENCHANTMENT_SLOT +
  firstPrismatic` with `socketColor = 0`, so `pickBestGem` treats it as a colored socket that takes
  any gem and earns no socket-bonus nudge.
- `GetCurrentGemsCount` (`:3929`) already spans these slots, so meta-requirement solving picks the
  new gem up for free.

### 4. Jewelcrafting Dragon's Eye gems

- Narrow the cache filter at `:532-535`: keep excluding `ITEM_FLAG_UNIQUE_EQUIPPABLE` gems **unless**
  `proto->ItemLimitCategory` is set (i.e. it is a limit-category gem the existing cap already
  understands).
- Add an item-level profession gate in the eligibility loop next to the existing enchant gate at
  `:5071`: skip gems whose `proto->RequiredSkill` the bot lacks, or whose `proto->RequiredSkillRank`
  exceeds `bot->GetSkillValue(...)`.
- Replace the hardcoded cap with what `Player::CanEquipUniqueItem` actually enforces, because the old
  `ItemLimitCategory == 2` / max-3 pair covers neither the flag nor other categories: a gem carrying
  `ITEM_FLAG_UNIQUE_EQUIPPABLE` gets one copy per bot (by item ID), and any gem with an
  `ItemLimitCategory` is capped at that category's DBC `maxCount`. A category with no DBC row is
  unequippable, so skip it. `ApplyEnchantAndGemsNew` tracks both in `gemsUsedById` /
  `gemsUsedByCategory`; the meta gem counts as soon as it is chosen even though its apply is
  deferred, and the meta-requirement solver picks on a copy of the two maps minus the gem it is about
  to replace, so a swap can reuse the freed budget.
- `StatsWeightCalculator::BestGemScore` estimates what a socket is worth generically and must skip
  item-side skill gates (`gemTemplate->RequiredSkill`) as well as `enchant->requiredSkill`. Without
  the item-side check, Dragon's Eyes inflate socket value for every bot, jeweler or not.

### 5. Re-enchant on level-up

Add an `ApplyEnchantAndGemsNew()` call behind `minEnchantingBotLevel` after the `InitEquipment(true)`
in `AutoMaintenanceOnLevelupAction` (`src/Ai/Base/Actions/AutoMaintenanceOnLevelupAction.cpp:163-180`),
matching the guard used at `src/Bot/Factory/PlayerbotFactory.cpp:837-840`.

### Config

`AiPlayerbot.ProfessionGearEnhancements` (default 1) in `conf/playerbots.conf.dist` next to
`LimitEnchantExpansion`, parsed in `src/PlayerbotAIConfig.cpp` alongside `limitEnchantExpansion`.

It gates items 3 and 4 — the added sockets, and profession-gated gems (any gem with an item-side
`RequiredSkill`, which in practice is the Dragon's Eyes). Both are the new feature, so turning the
config off has to restore the prior behaviour for both. Items 1 and 5 are corrections to existing
generic behaviour and stay unconditional.

## Verification

The module cannot be compiled headless in the authoring environment, so verification is server-side:

1. Build the core with the module, start a world server.
2. `.playerbot bot add <name>` a level-80 bot, then `.playerbot randomize <name>`.
3. `.pinfo` / inspect the bot, or query
   `SELECT * FROM item_instance WHERE owner_guid = <guid>` and check the `enchantments` column for
   the expected enchant IDs on gloves/boots/cloak/bracers/waist.
4. Confirm profession pairing first: `SELECT skill, value FROM character_skills WHERE guid = <guid>`
   should show the primary professions at 450 after change 1.
5. Negative test: a bot without Jewelcrafting must have no Dragon's Eye gems, and a JC bot must have
   at most the limit category's `maxCount` (3) of them, with no two of the same unique-equipped gem
   ID. No bot should ever carry a tinker (item 2 is dropped).
   Also check the level gate: with `LimitEnchantExpansion = 1` a level-70 bot gets no added socket
   and a level-71 bot does; with it set to 0 the level-70 bot gets one.
   Then flip `ProfessionGearEnhancements = 0` and confirm a fresh randomize produces neither an added
   socket nor a Dragon's Eye.
6. Meta-gem regression: with `AiPlayerbot.FulfillMetaGemRequirements = 1`, confirm the meta is still
   active after a prismatic socket is added (the solver at `:5316-5398` now has one more socket in
   play).
7. Level-up test: `.playerbot bot add` a level-79 bot, level it, confirm new gear arrives enchanted.
