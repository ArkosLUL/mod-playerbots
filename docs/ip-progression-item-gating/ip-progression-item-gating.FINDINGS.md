# IP-Progression Item Gating — Findings

## Problem

Bots always apply the **best** enchant / gem / consumable their *level* allows. Level is the only
content gate in the module, and it is implemented as a handful of hardcoded "first item id of an
expansion" cutoffs:

| Site | Gate | Meaning |
|---|---|---|
| `src/Bot/Factory/PlayerbotFactory.cpp:5363` | enchant spell `>= 27899` blocked at level <= 60 | first TBC enchant spell |
| `src/Bot/Factory/PlayerbotFactory.cpp:5366` | enchant spell `>= 44483` blocked at level <= 70 | first WotLK enchant spell |
| `src/Bot/Factory/PlayerbotFactory.cpp:5165` | gem item `>= 39900` blocked at level <= 70 | first WotLK gem cut |
| `src/Mgr/Item/RandomItemMgr.cpp:987-988` | item `>= 23728` / `>= 35570` | TBC / WotLK item boundary (ammo, potions) |
| `src/Bot/Factory/PlayerbotFactory.cpp:4036` | prismatic socket floored at level 70/71 | Eternal Belt Buckle etc. |

On a realm running **mod-individual-progression** (IP), level is decoupled from content. A level-80
bot on a realm still locked at `PROGRESSION_PRE_TBC` receives epic Wrath gems, Wrath meta gems, an
Eternal Belt Buckle and a Potion of Speed. Every gate above passes, because they all only ask "is
the bot level 80?".

There is currently no integration between the two modules in either direction, except IP's
account-name regex, which force-stomps every `^RNDBOT.*` character's tier to 0 / 8 / 13 purely by
level on login (`mod-individual-progression/src/IndividualProgressionPlayer.cpp:33-38`).

## IP progression tiers → real content patches

Source enum: `mod-individual-progression/src/IndividualProgression.h:227-248`. State is stored as
hidden rewarded quests with ids `66000 + tier`; there is no `character_settings` involvement.

| Tier | Name | Content unlocked | Patch |
|---|---|---|---|
| 0 | `PROGRESSION_START` | vanilla leveling | 1.0–1.5 |
| 1 | `MOLTEN_CORE` | BWL | 1.6 |
| 2 | `ONYXIA` | — | 1.6 |
| 3 | `BLACKWING_LAIR` | ZG, AQ war effort | 1.7–1.9 |
| 4 | `PRE_AQ` | AQ gates | 1.9 |
| 5 | `AQ_WAR` | — | 1.9 |
| 6 | `AQ` | Naxx40, Scourge Invasion | 1.11 |
| 7 | `NAXX40` | — | 1.11 |
| 8 | `PRE_TBC` | Karazhan / Gruul / Magtheridon | 2.0 |
| 9 | `TBC_TIER_1` | SSC / Tempest Keep | 2.1 |
| 10 | `TBC_TIER_2` | Hyjal / Black Temple | 2.1 |
| 11 | *(unnamed, value is live)* | Zul'Aman | 2.3 |
| 12 | `TBC_TIER_4` | Sunwell Plateau | 2.4 |
| 13 | `TBC_TIER_5` | WotLK Naxx / EoE / OS | 3.0 |
| 14 | `WOTLK_TIER_1` | Ulduar | 3.1 |
| 15 | `WOTLK_TIER_2` | Trial of the Crusader | 3.2 |
| 16 | `WOTLK_TIER_3` | ICC | 3.3 |
| 17 | `WOTLK_TIER_4` | Ruby Sanctum | 3.3.5 |
| 18 | `WOTLK_TIER_5` | — | — |

Tier 11 has no symbolic name (the enumerator is commented out) but is a live value in data —
`RequiredZulAmanProgression` defaults to 12 and the read loops iterate `1..18` inclusive.

---

## Gems

### How the pool is actually built

`enchantGemIdCache` (`PlayerbotFactory.cpp:548-588`) walks `SpellItemEnchantment.GemID` and keeps
items that have a `GemProperties` entry, `ItemLevel >= 60`, and are not unique-equipped-without-a-
limit-category. **Only cut gems have `GemProperties`.** Raw prospected gems — Cardinal Ruby `36919`,
Scarlet Ruby `36766`*, Crimson Spinel `32249`, Living Ruby `23436` — never enter the pool, so any
reasoning based on raw ore ids is reasoning about the wrong id space entirely.

<sub>\* `36766` in this DB is *Bright Dragon's Eye*, not raw Scarlet Ruby. Raw-vs-cut id spaces overlap between families.</sub>

All figures below were dumped from a live `acore_world` (`item_template`, 46 100 rows), not inferred
from id ordering.

### Verified blocks

| Block | Ids | ilvl | Quality | Patch | Gate at tier |
|---|---|---|---|---|---|
| TBC base cuts (Living Ruby, Star of Elune, Noble Topaz, Talasite, Nightseye, Dawnstone) | `24027`–`24071` + stragglers to `38292` | 70 | 3 | 2.0 | **8** |
| TBC JC "Ornate" epics | `28117`–`28363`, `38545`–`38550` | 60 | 4 | 2.0 | **8** |
| TBC metas (Skyfire / Earthstorm) | `25890`–`25901`, `28556`–`28557`, `32409`–`32410`, `32640`–`32641`, `34220`, `35501`, `35503` | 70 | 3 | 2.0–2.3 | **8** |
| TBC raid epics (Tanzanite, Fire Opal, Chrysoprase) | `30546`–`30607` | 70 | 4 | **2.1** | **9** |
| TBC Sunwell epics (Crimson Spinel, Empyrean Sapphire, Shadowsong Amethyst, Seaspray Emerald, Lionseye, Pyrestone) | `32193`–`32250` + stragglers `35489`, `35759`, `37503` | 70 | 4 | **2.4** | **12** |
| WotLK uncommon cuts (Bloodstone, Sun Crystal, Shadow Crystal, Huge Citrine, Dark Jade, Chalcedony) | `39900`–`39995`, Perfect cuts `41432`–`41502` | 70 | 2 | 3.0 | **13** |
| WotLK rare cuts (Scarlet Ruby, Autumn's Glow, Sky Sapphire, Forest Emerald, Monarch Topaz, Twilight Opal) | `39996`–`40110` | 80 | 3 | 3.0 | **13** |
| WotLK metas (Skyflare / Earthsiege) | `41285`–`41401` | 80 | 3 | 3.0 | **13** |
| WotLK lesser metas (Starflare / Earthshatter) | `44076`–`44089` | 80 | 3 | 3.0 | **13** |
| JC-only Dragon's Eye | `36766`–`36767`, `42142`–`42158` (`RequiredSkill = 755`) | 80 | 4 | 3.0 | **13** |
| Stormjewels | `45862`–`45987` | 80 | 4 | **3.1** | **14** |
| **WotLK epic cuts** (Cardinal Ruby, Majestic Zircon, King's Amber, Dreadstone, Ametrine, Eye of Zul) | **`40111`–`40182`** | 80 | 4 | **3.2** | **15** |
| Nightmare Tear | `49110` | 80 | 4 | **3.2** | **15** |

### The single highest-impact gap

The epic cut block `40111`–`40182` (Bold Cardinal Ruby `40111` … Shattered Eye of Zul `40182`) and
Nightmare Tear `49110` shipped with **patch 3.2, Call of the Crusade** — not 3.0. They are present in
the DBC from 3.0, so today a bot socket-fills with them the moment the realm reaches tier 13, roughly
+40% gem stats over the 3.0 rare cuts that should be best-in-slot at tiers 13–14.

The existing `39900` cutoff does not help: `39900` is *Bold Bloodstone*, the first WotLK **uncommon**
cut. The constant separates TBC from WotLK and nothing else. Nothing anywhere separates 3.0 WotLK
content from 3.2 WotLK content.

### Why id ranges alone are not sufficient

Families straggle badly — Living Ruby cuts run `24027`–`38292`, Noble Topaz `24058`–`35316`, Crimson
Spinel `32193`–`35489`. But **(`ItemLevel`, `Quality`) classifies them correctly**, because those
fields track the content tier the gem was designed for. The recommended rule is a small property
based fallback plus a short override list for the three blocks that break it:

```
fallback:
  ItemLevel <= 70, Quality <= 3   -> tier 8
  ItemLevel <= 70, Quality == 4   -> tier 12
  ItemLevel >= 75, Quality <= 3   -> tier 13
  ItemLevel >= 75, Quality == 4   -> tier 15
overrides (checked first):
  RequiredSkill == 755 (Jewelcrafting, Dragon's Eye)  -> tier 13
  entry in 30546..30607 (TBC raid epics)              -> tier 9
  entry in 45862..45987 (Stormjewels)                 -> tier 14
```

This is eight lines of data and it classifies every straggler correctly.

### Junk found in the pool while dumping

`RandomItemMgr::IsInternalItem` (`RandomItemMgr.cpp:1412-1433`) matches `"Unused "` with a trailing
space, so **`37430 "Solid Sky Sapphire (Unused)"`** (ilvl 80, rare, no flags) slips through and is a
live gem candidate today. Unrelated to progression — worth blacklisting on its own.

`28388`/`28389` (`TCHILTON TEST …`) and the `zzOLD…` entries are correctly caught by the existing
filter.

---

## Enchants

Enchant spell ids were cross-checked against `item_template` recipe rows (`class = 9`,
`subclass = 8`, `spellid_2` = the taught enchant spell).

| Cluster | Enchant spells | Patch | Gate at tier |
|---|---|---|---|
| ZG / AQ-era vanilla formulas | `22749` Weapon – Spellpower, `22750` Weapon – Healing Power, `23802` Bracer – Healing Power, `25072` Gloves – Threat, `25073` Shadow Power, `25074` Frost Power, `25078` Fire Power, `25079` Gloves – Healing Power, `25080` Superior Agility, `25084` Cloak – Subtlety | 1.7–1.9 | **3** |
| All TBC enchants (`>= 27899`) — Mongoose, Soulfrost, Sunfire, Major Spellpower `27975`, Gloves – Major Spellpower `33997`, Nethercleft/Nethercobra leg armor, Golden Spellthread, Sun Scope, Adamantite Weapon Chain | `27899`+ | 2.0 | **8** |
| Sunwell formula — **Enchant Weapon – Executioner `42974`** (recipe `33307`, 375 skill) | — | 2.4 | **12** |
| All WotLK enchants (`>= 44483`) — Icescale/Frosthide leg armor, Titanium Weapon Chain, Heartseeker Scope, ring enchants (`60714`, `60767`, `62948`), Sons of Hodir shoulder inscriptions, Lightweave/Darkglow/Swordguard Embroidery, Cloak – Superior Agility `44500` | `44483`+ | 3.0 | **13** |

Within-expansion enchant splits are otherwise thin: 3.1 and 3.2 added no meaningful gear enchants,
and the 2.1–2.3 additions are marginal. The two worth encoding are the vanilla ZG cluster and
Executioner.

**Already handled, no work needed.** The Naxx40 Sapphiron shoulder enchants (`29467`, `29475`,
`29480`, `29483`) are blacklisted outright at `PlayerbotFactory.cpp:480`.

---

## Other item improvements

| Improvement | Site | Content | Gate at tier |
|---|---|---|---|
| Prismatic sockets — Eternal Belt Buckle (`41611` / spell `55016`), Socket Bracer `55628`, Socket Gloves `55641` | `ApplyPrismaticSocket`, `PlayerbotFactory.cpp:4025` | 3.0 | **13** |
| DK runeforging (`53343`, `53344`) | `GetRuneforgeEnchantId`, `PlayerbotFactory.cpp:118-149` | 3.0 | moot — DK is 3.0 content |
| Fel / Adamantite Sharpening Stone (`23528`, `23529`), Fel / Adamantite Weightstone (`28420`, `28421`) | `InitConsumables`, `PlayerbotFactory.cpp:991` | 2.0 | **8** |
| Wizard / Mana oils (`20744`–`22522`) | `InitConsumables` | ≤1.11 | none needed |
| Instant Poison VI–VII, Deadly Poison V–VI | `InitConsumables` | 2.0 | **8** |
| Instant Poison VIII–IX, Deadly Poison VII–VIII, Anesthetic Poison | `InitConsumables` | 3.0 | **13** |
| Potion of Speed `40211`, Potion of Wild Magic `40212` | `InitPotions`, `PlayerbotFactory.cpp:3946-3952` | 3.0 | **13** |
| Haste / Destruction / Insane Strength potions | `InitPotions` | 1.11 | none needed |
| TBC ammo (`>= 23728`), WotLK ammo (`>= 35570`) | `RandomItemMgr::GetAmmo` / `IsAllowedForLevelExpansion` | 2.0 / 3.0 | **8** / **13** |

### Out of scope (decided)

- **Glyphs** — all Inscription/3.0 content, so strictly a bot below tier 13 should have none. Left
  alone by decision; the existing `limitTalentsExpansion && level <= 70` bail approximates it.
- **The equipped gear itself** (`InitEquipment`, `RandomItemMgr` item pools). Gating gems and
  enchants while a tier-8 bot still wears ICC gear is deliberately a half-measure: the result is
  correct-for-its-era enchants on wrong-for-its-era items.

---

## How to read a bot's progression tier

IP ships **no `CMakeLists.txt`** (only `include.sh`), so its headers are not an exported interface
target and cannot be included cleanly from mod-playerbots. The decoupled read is to query the hidden
quests directly, exactly as `mod-levelsync` already does
(`mod-levelsync/src/LevelSync.cpp:582-591`, `LevelSync.h:13-15`): loop `1..18`,
`player->GetQuestStatus(66000 + i) == QUEST_STATUS_REWARDED`, keep the highest.

Do **not** use `IndividualProgression::hasPassedProgression` even if linking: it returns `false` when
the module is disabled and when `state > progressionLimit`
(`IndividualProgression.cpp:32-36`), which reads as "not progressed" rather than "unrestricted".

A bot's *own* tier is not a trustworthy signal on its own: IP force-stomps every bot-account
character to 0 / 8 / 13 by level on each login. Resolution order that works:

1. Group/master leader's tier when the bot is grouped with a real player — IP's own
   `SyncBotsProgressionToLeader` (`IndividualProgression.cpp:411-437`) already pushes leader tier
   onto grouped bots, so this agrees with IP rather than fighting it.
2. Else the bot's own quest tier, but only when it is non-zero.
3. Else a server-wide config cap.

Detect whether IP is installed at all with `sObjectMgr->GetQuestTemplate(66001) != nullptr`; when it
is absent every gate must short-circuit to "allowed" so behaviour on non-IP realms is unchanged.
