# Itemization: scoring, gems, enchants, sockets, progression

How a bot decides what an item is worth and what to put in it. Loot rolling sits on top of this —
see [loot.md](loot.md).

## One function does all of it

**All bot enchanting and gemming happens in `PlayerbotFactory::ApplyEnchantAndGemsNew`**
(`src/Bot/Factory/PlayerbotFactory.cpp:5036`). It runs after `InitSkills` (`:715`) and
`InitEquipment` (`:779`), so professions and the full gear set are both known by then.

Two startup caches feed it, built in `PlayerbotFactory::Init()`:

- `enchantSpellIdCache` — scans `SPELL_EFFECT_ENCHANT_ITEM` (`:490`), with a list of `continue`
  guards for junk (test enchants, grandfathered TBC, the Naxx40 shoulder enchants at `:480`).
- `enchantGemIdCache` (`:548-588`) — walks `SpellItemEnchantment.GemID`, keeping items with a
  `GemProperties` entry, `ItemLevel >= 60`, not unique-equipped-without-a-limit-category.

**The invariant that keeps breaking:** `StatsWeightCalculator::BestGemScore` estimates what a socket
is worth and **must mirror `ApplyEnchantAndGemsNew`'s eligibility filters exactly** — level gates,
expansion gates, progression gates, item-side skill gates. Otherwise item *scoring* values sockets by
gems the bot cannot actually slot. There is one deliberate exception, documented at
`StatsWeightCalculator.cpp:818`: the tank cap-priority flag stays **off** in `BestGemScore` (see
below).

## The scoring pipeline

`StatsCollector::CollectItemStats` → `StatsWeightCalculator::CalculateItem` → `itemScore`.

- `weight_` is multiplied by `CalcMixedGearScore(ilvl, quality)` (`StatsWeightCalculator.cpp:158-167`),
  roughly `ilvl * 1.1^quality` — a few hundred. **Raw gem/enchant scores live in un-multiplied
  stat-sum space**, so adding the two directly is off by two orders of magnitude. Anything mixing
  them must use a ratio, not a sum.
- `CalculateEnchant` calls `Reset()`, which wipes `weight_` and the collector. Scoring an enchant
  mid-`CalculateItem` therefore needs a **separate calculator instance**.
- `StatsCollector::CollectEnchantStats` (`StatsCollector.cpp:236-264`) handles only
  `COMBAT_SPELL` (0.25×), `EQUIP_SPELL` (1.0×) and `STAT`; everything else hits `default: break;` and
  is worth 0. `EQUIP_SPELL` procs are followed through `SPELL_AURA_PROC_TRIGGER_SPELL` and averaged
  over the internal cooldown (`:737-742`), which is why embroidery and fur lining compete on real
  value. `USE_SPELL` scores only because bots now actually press those buttons — see
  [consumables-and-burst.md](consumables-and-burst.md).
- The enchant tie-break in `ApplyEnchantAndGemsNew` is `score > bestScore` (strict). It used to be
  `>=`, which let the *last* zero-scoring candidate win a slot by iteration order.

## Set bonuses

Set scoring lives only in `StatsWeightCalculator::CalculateItemSetMod` (`:664-706`). It was
effectively dead until the delta model landed, for three separate reasons worth remembering:

- `multiplier += 0.1f * itemCount` applied only while `itemCount < max_items`, so a **complete** set
  reset to `1.0f` — stickiness was zero exactly when the bonuses were live.
- It read `player->ItemSetEff`, i.e. currently-equipped state, so when comparing a candidate against
  the incumbent set piece **the incumbent's own contribution counted toward both scores and
  cancelled out**. Breaking a bonus was never penalised.
- Piece count was linear; the real 2/4/6 thresholds in `ItemSetEntry::items_to_triggerspell[]` were
  ignored.

The fix is a delta model keyed on `SetReplacedItemSet(setId)` — the set of the piece occupying the
contested slot, treated as removed so incumbent and challenger measure against the same baseline:

```
base   = EquippedSetPieces(player, proto->ItemSet) - (replaced_item_set_ == proto->ItemSet ? 1 : 0)
gained = ActiveSetBonuses(set, base + 1) - ActiveSetBonuses(set, base)
multiplier = 1 + ItemSet.BonusWeight * gained
if (gained == 0 && NextSetThreshold(set, base))
    multiplier += ItemSet.ProgressWeight * (base + 1)
```

Consequences: T7 chest → T7.5 chest is neutral (same set, same vacated baseline); a non-set
challenger must beat the set piece by the bonus weight **on top of** `EquipUpgradeThreshold`.

Every runtime path that scores a slot must supply the replaced set id — `QueryItemUsageForEquip`
(the primary path; `LootUsageValue`, `ItemUpgradeValue` and `ItemUsageValue` all funnel through it),
`AdjustUsageForCrossArmor`, and both `EquipAction` sites. `EquipAction` uses raw `>` comparisons with
no threshold, so leaving it out would silently undo what `QueryItemUsageForEquip` just protected.
`RandomItemMgr::CalculateItemWeight` (`RandomItemMgr.cpp:1055`) deliberately keeps
`SetItemSetBonus(false)` — it feeds a cached, player-independent table.

Known limitation: **tier tokens carry no `ItemSet`**, so `IsTokenLikelyUpgrade` /
`IsAnyTierSlotLikelyUpgrade` stay pure-ilvl and bots roll on tokens the old way.

## Sockets

`CalculateSocketBonus` used to be a flat `1.0 + socketNum * 0.03`, ignoring colour and meta sockets
entirely. It is now derived from what the bot would actually slot:

```
socketValue = Σ BestGemScore(cls, tab, lvl, pvp, socketColor) over sockets
multiplier  = 1 + Socket.ValueFactor * (socketValue / baseWeight)      // baseWeight = pre-quality-blend weight_
            = 1 + Socket.WeightPerSocket * socketNum                   // fallback when baseWeight ≈ 0
multiplier  = min(multiplier, Socket.MaxMultiplier)
```

Using a ratio keeps it unit-consistent and gives sockets a larger relative share on stat-light items.
Meta sockets need no special case — the cache returns the best *meta* gem for `SOCKET_COLOR_META`.

The `best_gem_score_` cache is keyed on class, tab, level, pvp spec and socket colour, guarded by a
shared mutex (bots tick on parallel map threads). **The progression tier must be part of that key**,
or a calculator reused across bots hands out a stale pool. Two deliberate differences from the
factory's `pickBestGem`: no `×1.2` colour-match nudge (that biases *which* gem to slot, not what the
socket is worth) and no jeweller cap (scoring a hypothetical socket, not allocating a budget).

Still out of scope: `StatsCollector.cpp:81-85` credits full `socketBonus` enchant stats regardless of
whether the bot can colour-match.

## Gems

**Only cut gems have `GemProperties`.** Raw prospected gems never enter the pool at all, so any
reasoning based on raw-ore ids is reasoning about the wrong id space. Raw-vs-cut ids also overlap
between families — `36766` in this DB is *Bright Dragon's Eye*, not raw Scarlet Ruby.

**Id ranges do not classify gems.** Families straggle badly (Living Ruby cuts span `24027`–`38292`,
Noble Topaz `24058`–`35316`). `(ItemLevel, Quality)` tracks the content tier reliably, because those
fields follow what the gem was designed for. See the progression section for the exact rule.

**Jewelcrafting Dragon's Eyes** carry `ITEM_FLAG_UNIQUE_EQUIPPABLE`, which the cache filter dropped
outright — making the old `ItemLimitCategory == 2` cap dead code. Two things matter when re-admitting
them: the profession lock lives on the gem **item** (`proto->RequiredSkill == SKILL_JEWELCRAFTING`),
not on the enchant (whose `requiredSkill` is 0), and the real cap is what
`Player::CanEquipUniqueItem` enforces — one copy per item id for `ITEM_FLAG_UNIQUE_EQUIPPABLE`, plus
the DBC `maxCount` for any `ItemLimitCategory` (a category with no DBC row is unequippable).
`ApplyEnchantAndGemsNew` tracks both in `gemsUsedById` / `gemsUsedByCategory`; the meta gem counts as
soon as it is chosen even though its apply is deferred.

**Prismatic sockets.** How the core stores this is easy to get wrong: `PRISMATIC_ENCHANTMENT_SLOT`
holds the socket-*adding* enchant, while the **gem** goes into the first template socket with no
colour, i.e. `SOCK_ENCHANTMENT_SLOT + firstPrismatic` (`ItemHandler.cpp:1244-1263`,
`PlayerStorage.cpp:4435-4442`). The core rejects meta gems there (`ItemHandler.cpp:1270`), so it is
always a coloured socket. The cached prismatic enchant must carry
`ITEM_ENCHANTMENT_TYPE_PRISMATIC_SOCKET` — `Spell::EffectEnchantItemPrismatic` refuses anything else
and setting it anyway leaves a phantom socket.

**Trap:** the prismatic level gate is 70, or 71 while `LimitEnchantExpansion` is on. **Do not also
filter by spell id inside the loop — the two checks cancel out and the feature goes dead at exactly
level 70.**

**Meta gem lifecycle.** `Player::ApplyEnchantment` gates on the meta condition for **both** apply and
remove (`PlayerStorage.cpp:4421`). A bot whose meta was socketed but *inactive*, on a run where
colour-steering now satisfies the condition, would have the leading `ApplyEnchantment(false)` remove
stats that were never applied — a phantom −stats wash that nets to zero and leaves the meta inactive
forever. The core avoids this with `wasactive` tracking (`Player.cpp:11256-11262`); the module does
not. The fix is a pre-deactivate pass before colour gems are rearranged, then a single
`ApplyEnchantment(true)` at the end. `CorrectMetaGemEnchants` is *not* a drop-in replacement — it
compares before/after a single changed socket, not a from-scratch recompute.

**Known filter hole:** `RandomItemMgr::IsInternalItem` (`RandomItemMgr.cpp:1412-1433`) matches
`"Unused "` **with a trailing space**, so `37430 "Solid Sky Sapphire (Unused)"` slips through as a
live gem candidate. The `TCHILTON TEST` and `zzOLD…` entries are caught correctly.

## Enchants

The enchant loop gates only on `enchant->requiredSkill`. **For profession-locked enhancements the
lock lives on the source item, not the enchant** — the same trap as Dragon's Eyes. That let bots
apply two leg armors no player can obtain:

| Enchant | Text | Spell | Source item | Craft |
|---|---|---|---|---|
| 3331 | +72 Stamina / +35 Agility | 50911 | 38377 Dragonscale Leg Armor | 50968 |
| 3332 | +100 Attack Power / +36 Crit | 50913 | 38378 Wyrmscale Leg Armor | 50969 |

Crafts 50968/50969 are taught by no trainer and no recipe item — leftover Blizzard data. A sweep of
every `class=0 subclass=6` enhancement at ilvl ≥ 74 confirmed these are the **only** two in that
state, so blacklisting the two spell ids in the cache build is sufficient. The `zzDEPRECATED`
tailoring spellthreads (41605/41606) look similar but apply the same enchants as the live
Brilliant/Sapphire Spellthread, so they are harmless.

Still out of scope: a general "source item requires a profession the bot lacks" gate. It needs a
spell→source-item map built from `item_template.spellid_1..5`, and it would not have caught the leg
armors for a leatherworking bot anyway.

## Profession enhancements

Per-profession status, and what blocked each one:

| Profession | Enhancement | Status |
|---|---|---|
| Enchanting | Ring enchants | Works — was the only one that ever reached bot gear |
| Engineering | Hyperspeed Accelerators | Applied and used (see the tinker section in consumables-and-burst.md) |
| Engineering | Hand-Mounted Pyro Rocket | Never applied — damage-only, scores 0 |
| Engineering | Nitro Boosts | Scored on its stat half (+24 crit) only |
| Engineering | Flexweave Underlay, Springy Arachnoweave | Never applied — the cloak slot is reserved for tailoring embroidery |
| Tailoring / Leatherworking | Embroidery, Fur Lining | Scored generically from DBC |
| Blacksmithing | Socket Bracer / Gloves | Needed prismatic support |
| Jewelcrafting | Dragon's Eye | Needed the unique-equippable filter narrowed |
| Inscription | Master's Inscription | Needed the skill cap raised — `maxValue = level * 5` capped at 400, and Inscription needs 430 |

Skill caps now derive from `sWorld->getIntConfig(CONFIG_EXPANSION)` (300/375/450) for primary trade
skills; `level * 5` stays for weapon and secondary skills. Accepted side effect: bots can also
learn 401-450 recipes, which touches `GuildTaskMgr` and item-usage logic.

The Eternal Belt Buckle has no skill requirement, so it goes to every bot clearing the level gate,
not only Blacksmiths.

`AutoMaintenanceOnLevelupAction` calls `InitEquipment(true)` but historically never
`ApplyEnchantAndGemsNew`, so gear swapped in on level-up stayed bare until a full maintenance run.

## Tank gemming and the defense cap

`pickBestGem` is a greedy per-socket argmax on `CalculateEnchant`. With prot warrior/paladin weights,
a +30 Sta gem scores 93 against 50 for +20 Defense rating and 60 for +20 Expertise — so stamina wins
every socket unconditionally and a bot below 540 defense never gems its way to crit immunity, the one
stat that is a hard requirement rather than a trade-off.

`SetCapPriority(true)` raises the defense weight to `DEFENSE_UNDERCAP_WEIGHT = 10.0f` while the bot
is a melee tank below the cap. Sizing: a +20 defense gem must beat a +30 stamina gem including the
×1.2 colour nudge on both sides, which needs > ~4.7. `ApplyOverflowPenalty` already truncates
`STATS_TYPE_DEFENSE` to the remaining-to-cap amount, so the escalation self-terminates — later
sockets revert to stamina naturally as each gem is applied before the next is scored.

**Not a bug:** `DEFENSE_OVERFLOW = 140` is correct — `GetRatingBonusValue(CR_DEFENSE_SKILL)` returns
defense skill *from rating*, and base skill at 80 is 400, so 140 is exactly the 540 cap.

**Druids are excluded.** Survival of the Fittest gives bears crit immunity without defense (which is
why the bear branch weights defense at 0.3); a bear never reaches `DEFENSE_OVERFLOW`, so the boost
would never switch off and every socket would go into a dead stat permanently.

**Deliberately not propagated to `BestGemScore`** — turning it on there would make an under-cap tank
re-rank whole gear pieces and churn equips as it crosses the cap. The cost is a slightly conservative
socket estimate for under-cap tanks.

## Progression gating (mod-individual-progression)

Level is the module's only native content gate, implemented as hardcoded "first item id of an
expansion" cutoffs: enchant spell `>= 27899` (TBC) / `>= 44483` (WotLK), gem `>= 39900`, item
`>= 23728` / `>= 35570`, prismatic floored at level 70/71. On an IP realm level is decoupled from
content, so every one of those gates passes for a level-80 bot on a pre-TBC realm.

### Reading a bot's tier

IP ships **no `CMakeLists.txt`**, so its headers cannot be included. Read the hidden quests directly:
loop `1..18`, `GetQuestStatus(66000 + tier) == QUEST_STATUS_REWARDED`, keep the highest. Detect
whether IP is installed at all with `sObjectMgr->GetQuestTemplate(66001) != nullptr`; when absent,
every gate must short-circuit to "allowed".

Do **not** use `IndividualProgression::hasPassedProgression` even if linking — it returns `false`
when the module is disabled and when `state > progressionLimit`, which reads as "not progressed"
rather than "unrestricted".

A bot's own tier is not trustworthy alone: **IP force-stomps every bot-account character to 0 / 8 / 13
by level on each login**. Resolution order: group leader's tier when grouped with a real player (IP's
own `SyncBotsProgressionToLeader` already pushes this, so it agrees rather than fights) → the bot's
own tier when non-zero → `ProgressionTierCap`.

### Tiers → content

| Tier | Name | Content unlocked | Patch |
|---|---|---|---|
| 0-7 | `PROGRESSION_START` … `NAXX40` | vanilla, through Naxx40 | 1.0–1.11 |
| 8 | `PRE_TBC` | Karazhan / Gruul / Magtheridon | 2.0 |
| 9 | `TBC_TIER_1` | SSC / Tempest Keep | 2.1 |
| 10 | `TBC_TIER_2` | Hyjal / Black Temple | 2.1 |
| 11 | *(unnamed, live value)* | Zul'Aman | 2.3 |
| 12 | `TBC_TIER_4` | Sunwell Plateau | 2.4 |
| 13 | `TBC_TIER_5` | WotLK Naxx / EoE / OS | 3.0 |
| 14 | `WOTLK_TIER_1` | Ulduar | 3.1 |
| 15 | `WOTLK_TIER_2` | Trial of the Crusader | 3.2 |
| 16 | `WOTLK_TIER_3` | ICC | 3.3 |
| 17 | `WOTLK_TIER_4` | Ruby Sanctum | 3.3.5 |
| 18 | `WOTLK_TIER_5` | — | — |

Tier 11's enumerator is commented out in IP but the value is live in data.

### Gem classification rule

Property fallback plus three overrides, checked first:

```
RequiredSkill == 755 (Dragon's Eye)     -> 13
30546..30607  (TBC raid epics, 2.1)     -> 9
45862..45987  (Stormjewels, 3.1)        -> 14
ItemLevel <= 70, Quality <  epic        -> 8
ItemLevel <= 70, Quality == epic        -> 12
ItemLevel >= 75, Quality <  epic        -> 13
ItemLevel >= 75, Quality == epic        -> 15
```

Eight lines of data that classify every straggler correctly. The highest-impact case it fixes: the
epic cut block `40111`–`40182` (Bold Cardinal Ruby … Shattered Eye of Zul) and Nightmare Tear `49110`
shipped with **patch 3.2**, not 3.0 — they are in the DBC from 3.0, so bots socket-filled with them
the moment a realm reached tier 13, roughly +40% gem stats over the rare cuts that should be
best-in-slot at tiers 13–14. The `39900` cutoff never helped: it separates TBC from WotLK and
nothing else.

### Enchant and item classification

Explicit ids **before** the `>=` ranges: the ZG/AQ vanilla cluster (`22749`, `22750`, `23802`,
`25072`, `25073`, `25074`, `25078`, `25079`, `25080`, `25084` — not contiguous) → tier 3;
`42974` Executioner (Sunwell) → tier 12; `>= 44483` → tier 13; `>= 27899` → tier 8. Items (ammo,
potions, consumables): `>= 35570` → 13, `>= 23728` → 8, mirroring
`RandomItemMgr::IsAllowedForLevelExpansion` one-for-one.

### Performance constraint

Resolve the tier **once per factory pass** and pass it into the loops — `enchantSpellIdCache` is
thousands of entries scanned per equipment slot, so the resolver must never be called inside them.
Cache the resolved tier on `PlayerbotAI` and invalidate on group change.

### Deliberately not gated

Glyphs (the existing `limitTalentsExpansion && level <= 70` bail approximates it) and **the equipped
gear itself**. Gating gems and enchants while a tier-8 bot still wears ICC gear is a known
half-measure: correct-for-era enchants on wrong-for-era items is expected, not a bug.

## Ranked BiS lists

`StatsWeightCalculator::BisRankMultiplier` is the last multiplier in `CalculateItem`, and the only
one that is off by default — it needs `SetBisBonus(true)`, which only `QueryItemUsageForEquip` and
the `calc` debug command set. `PlayerbotFactory::InitEquipment`, autogear and
`RandomItemMgr::CalculateItemWeight` all score without it.

    1 + Bis.ScoreBonus * kRankScale[rank - 1] * phaseScale

- `kRankScale = {1.0, 0.8, 0.6}` for ranks 1-3. Ranks 4-6 are filler alternates and get no score
  change (they still get the gate bypass). Flat-ish on purpose: ranks 1-3 of a slot are
  near-equivalent picks, and against `EquipUpgradeThreshold` (1.1) a 2.5% nudge is invisible.
- `phaseScale = 1 - Bis.PhaseDecay * (cap.phase - entryPhase)`, floored at 0. Without it a leftover
  pre-raid rank-1 piece is worth exactly as much as the current tier's rank-1, the two bonuses
  cancel, and the equipped lower-ilvl piece keeps the slot on `ItemSet.BonusWeight` (x1.15) times
  `EquipUpgradeThreshold` (x1.1) alone — a 26.5% raw-stat wall.

`PhaseDecay` defaults to 1.0, so only the bot's current phase carries any bonus. One global value
covers all three expansions even though Vanilla's `P1..P6` sit closer together in ilvl than WotLK's
tiers; split it per expansion only with evidence that Vanilla bots cling to old gear.

Lookups never cross expansions — see
[loot.md](loot.md#expansion-matching) for the `{expansion, phase}` ceiling and the tier table.

The lists carry gems and enchants per slot (the `enhs` sub-table), and the import **drops them**.
Bots still choose both by score in `ApplyEnchantAndGemsNew`.

## Config

Defaults below are the shipped values in `conf/playerbots.conf.dist`.

| Key | Default | Meaning |
|---|---|---|
| `AiPlayerbot.AutoEquipUpgradeLoot` | 1 | Auto-equip looted upgrades |
| `AiPlayerbot.EquipUpgradeThreshold` | 1.1 | Score ratio needed to swap |
| `AiPlayerbot.ItemSet.UseForUpgrades` | 1 | Master switch for set-bonus scoring on runtime paths |
| `AiPlayerbot.ItemSet.BonusWeight` | 0.15 | Boost per set bonus kept or gained |
| `AiPlayerbot.ItemSet.ProgressWeight` | 0.03 | Per-piece nudge toward the next threshold |
| `AiPlayerbot.Socket.ValueFactor` | 1.0 | Scales the gem-derived socket multiplier; 0 ⇒ fallback only |
| `AiPlayerbot.Socket.MaxMultiplier` | 1.3 | Clamp on the socket multiplier |
| `AiPlayerbot.Socket.WeightPerSocket` | 0.03 | Flat fallback when gem value is unusable |
| `AiPlayerbot.LimitEnchantExpansion` | 1 | Level-based expansion gate on enchants/gems |
| `AiPlayerbot.LimitGearExpansion` | 1 | Level-based expansion gate on items |
| `AiPlayerbot.ProfessionGearEnhancements` | 1 | Gates added sockets and profession-gated gems |
| `AiPlayerbot.FulfillMetaGemRequirements` | 1 | Steer colour gems to activate the meta |
| `AiPlayerbot.LimitProgressionTier` | 1 | IP tier gating; inert when IP is absent |
| `AiPlayerbot.ProgressionTierCap` | 18 | Fallback tier for ungrouped bots with no own tier |

Debug: `<bot> calc [item]` prints a raw score (`TellCalculateItemAction`, registered as `calc`).
`.ip set <n>` sets a test character's IP tier.
