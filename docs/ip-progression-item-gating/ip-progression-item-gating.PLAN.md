# IP-Progression Item Gating — Implementation Plan

Companion to `ip-progression-item-gating.FINDINGS.md`, which holds the investigation: the IP tier →
patch mapping, the DB-verified gem/enchant blocks, and the per-improvement gate table. Read it first;
this document only covers how to build the thing.

## Goal

Gate bot enchants, gems, prismatic sockets, consumables, potions and ammo by
mod-individual-progression tier, keeping the existing level gates as an independent second floor.
No behaviour change on realms without IP.

**Out of scope by decision**: glyphs, and the equipped gear itself (`InitEquipment`,
`RandomItemMgr` item pools).

## Design decisions

- **Tier source**: hybrid — group leader's tier → bot's own quest tier → config cap.
- **Coupling**: decoupled quest read (`66000 + tier` via `Player::GetQuestStatus`). IP ships no
  CMake target, so its headers cannot be included.
- **Classification**: item-property fallback plus a short override list, *not* pure id ranges.
  Gem families straggle across the id space (Living Ruby cuts span `24027`–`38292`), but
  (`ItemLevel`, `Quality`) tracks content tier reliably. See FINDINGS § "Why id ranges alone are not
  sufficient".

---

## Part 1 — Progression resolver

New unit `src/Mgr/Progression/ProgressionMgr.{h,cpp}`.

```cpp
// Hidden IP progression quests, ids 66000 + tier. Read directly rather than linking against
// mod-individual-progression, which ships no CMake target.
static constexpr uint32 IP_QUEST_BASE  = 66000;
static constexpr uint8  IP_MAX_TIER    = 18;
static constexpr uint8  IP_TIER_TBC    = 8;   // PROGRESSION_PRE_TBC
static constexpr uint8  IP_TIER_SUNWELL = 12; // PROGRESSION_TBC_TIER_4
static constexpr uint8  IP_TIER_WOTLK  = 13;  // PROGRESSION_TBC_TIER_5
static constexpr uint8  IP_TIER_ULDUAR = 14;  // PROGRESSION_WOTLK_TIER_1
static constexpr uint8  IP_TIER_TOTC   = 15;  // PROGRESSION_WOTLK_TIER_2

enum ProgressionGateKind { GATE_ENCHANT_SPELL, GATE_GEM, GATE_ITEM };

class ProgressionMgr
{
public:
    static ProgressionMgr* instance();
    void Init();                                   // detect whether IP is installed
    bool IsEnabled() const { return _ipPresent && sPlayerbotAIConfig.limitProgressionTier; }

    uint8 GetPlayerProgressionTier(Player* player) const;
    uint8 GetBotProgressionTier(Player* bot) const;
    bool  IsAllowedForProgression(uint32 id, ProgressionGateKind kind, uint8 tier) const;

private:
    uint8 GetGemMinTier(ItemTemplate const* proto) const;
    bool _ipPresent = false;
};
#define sProgressionMgr ProgressionMgr::instance()
```

- `Init()` — `_ipPresent = sObjectMgr->GetQuestTemplate(IP_QUEST_BASE + 1) != nullptr`. Call from
  `PlayerbotFactory::Init()` (`PlayerbotFactory.cpp:425`), before the caches are built.
- `GetPlayerProgressionTier` — mirrors `IndividualProgression::GetPlayerProgressionFromQuests`
  (`IndividualProgression.cpp:21-26`): loop `1..IP_MAX_TIER`, keep the highest quest with
  `QUEST_STATUS_REWARDED`.
- `GetBotProgressionTier` — returns `IP_MAX_TIER` when `!IsEnabled()`. Otherwise: grouped-with-a-real-
  player → that leader's tier (skip bot leaders via `GET_PLAYERBOT_AI`); else the bot's own tier when
  non-zero; else `sPlayerbotAIConfig.progressionTierCap`.

### Gem classification

```cpp
uint8 ProgressionMgr::GetGemMinTier(ItemTemplate const* proto) const
{
    // Dragon's Eye cuts are ilvl80 epics but shipped with 3.0, so they'd fall through to the
    // epic-cut rule below without this.
    if (proto->RequiredSkill == SKILL_JEWELCRAFTING)
        return IP_TIER_WOTLK;

    if (proto->ItemId >= 30546 && proto->ItemId <= 30607)  // Tanzanite/Fire Opal/Chrysoprase, 2.1
        return IP_TIER_SSC;                                 // 9
    if (proto->ItemId >= 45862 && proto->ItemId <= 45987)  // Stormjewels, 3.1
        return IP_TIER_ULDUAR;

    if (proto->ItemLevel <= 70)
        return proto->Quality >= ITEM_QUALITY_EPIC ? IP_TIER_SUNWELL : IP_TIER_TBC;

    return proto->Quality >= ITEM_QUALITY_EPIC ? IP_TIER_TOTC : IP_TIER_WOTLK;
}
```

### Enchant and item classification

Small static `{ lo, hi, minTier }` tables plus a `{ id, minTier }` override list:

- **Enchant spells**: ZG/AQ cluster (`22749`, `22750`, `23802`, `25072`–`25084` — enumerate the ten
  ids from FINDINGS, they are not contiguous) → tier 3; `42974` Executioner → tier 12;
  `>= 44483` → tier 13; `>= 27899` → tier 8.
  Order matters — check the explicit ids before the `>=` ranges.
- **Items** (ammo, potions, consumables): `>= 35570` → tier 13; `>= 23728` → tier 8. This mirrors
  `RandomItemMgr::IsAllowedForLevelExpansion` (`RandomItemMgr.cpp:987-988`) one-for-one, so the two
  gates stay consistent.

---

## Part 2 — Wiring

Compute the tier **once per factory pass** and pass it into the loops. `enchantSpellIdCache` is
thousands of entries scanned per equipment slot, so the resolver must never be called inside them.

| Site | Change |
|---|---|
| `PlayerbotFactory.cpp:5150` `ApplyEnchantAndGemsNew` | resolve tier once at the top; use it in both the gem pre-filter and the enchant scan |
| `PlayerbotFactory.cpp:5165` | gem pre-filter: add the gem gate next to the existing `39900` check |
| `PlayerbotFactory.cpp:5363-5367` | enchant scan: add the enchant-spell gate next to the `27899` / `44483` checks |
| `PlayerbotFactory.cpp:4036` `ApplyPrismaticSocket` | add `tier >= IP_TIER_WOTLK` alongside the level 70/71 floor |
| `StatsWeightCalculator.cpp:804-833` `BestGemScore` | **must** apply the identical gem gate — otherwise item *scoring* keeps valuing sockets by gems the bot cannot socket |
| `PlayerbotFactory.cpp:991` `InitConsumables` | filter the stone / poison handout lists by tier |
| `PlayerbotFactory.cpp:3956-3965` `InitPotions` | add the tier check beside `IsAllowedForLevelExpansion` |
| `RandomItemMgr.cpp:959` `GetAmmo` | add the tier check beside `IsAllowedForLevelExpansion` |

`StatsWeightCalculator` already memoises in `best_gem_score_`; the tier must be part of what that
cache is keyed on, or a calculator reused across bots will hand out a stale pool.

Cache the resolved tier on `PlayerbotAI` and invalidate on group change, so the repeated
`ApplyEnchantAndGemsNew` calls from `AutoMaintenanceOnLevelupAction.cpp:186` and
`TrainerAction.cpp:191-268` stay cheap.

### Unrelated fix found while dumping the pool

Add `37430` ("Solid Sky Sapphire (Unused)") to the gem exclusions — `IsInternalItem`
(`RandomItemMgr.cpp:1427`) matches `"Unused "` with a trailing space, so `"(Unused)"` slips through
and the item is a live gem candidate today.

---

## Part 3 — Config

`conf/playerbots.conf.dist`, next to the existing `LimitEnchantExpansion` block (`:1078-1106`):

| Key | Default | Meaning |
|---|---|---|
| `AiPlayerbot.LimitProgressionTier` | 1 | master switch; inert when IP is not installed |
| `AiPlayerbot.ProgressionTierCap` | 18 | fallback tier for ungrouped bots with no own tier |

Document in the conf comment that gear itself is not gated, so correct-for-era enchants on
wrong-for-era items is expected behaviour and not a bug.

---

## Verification

The gem and enchant blocks in FINDINGS were already dumped and confirmed against a live
`acore_world`; they do not need re-verifying.

1. **Build** — the module cannot be compiled headless in this environment. Hand off to the normal
   AzerothCore build; everything here is static review only.
2. **Per-tier in game.** Set a test character's tier with `.ip set <n>`, group a bot to it, run
   `.playerbot randomize` (or `.bot maintenance`), inspect the bot's gear:
   - tier 7 → no gems in any socket, no TBC enchants, no belt buckle
   - tier 8 → TBC base cuts only; no `30546`+ raid epics, no `32193`+ Sunwell epics, no Executioner
   - tier 9 → Tanzanite / Fire Opal / Chrysoprase appear
   - tier 12 → Sunwell epic cuts and Executioner appear
   - tier 13/14 → WotLK rare cuts (`39996`+), metas, belt buckle, Potion of Speed; **no** `40111`+
     epic cuts, no Nightmare Tear
   - tier 14 → Stormjewels appear
   - tier 15 → Bold Cardinal Ruby (`40111`) family and Nightmare Tear (`49110`) appear
3. **Ungrouped bot** with `ProgressionTierCap = 8` → behaves as tier 8 despite being level 80.
4. **IP absent or disabled** → bots gear exactly as they do today. Pure regression check.
5. **Scoring consistency** — with a tier-13 bot, confirm `BestGemScore` and the applied gem come from
   the same pool; a socketed item's score must not assume epic gems the bot cannot use.
