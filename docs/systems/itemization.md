# Itemization: scoring, gems, enchants, sockets, progression

How a bot decides what an item is worth and what to put in it. Loot rolling sits on top of this —
see [loot.md](loot.md).

## One function does all of it

**All bot enchanting and gemming happens in `PlayerbotFactory::ApplyEnchantAndGemsNew`**
(`src/Bot/Factory/PlayerbotFactory.cpp:5365`). It runs after `InitSkills` (`:715`) and
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
`StatsWeightCalculator.cpp:985`: the tank cap-priority flag stays **off** in `BestGemScore` (see
below).

## The scoring pipeline

`StatsCollector::CollectItemStats` → `StatsWeightCalculator::CalculateItem` → `itemScore`.

- `weight_` is multiplied by `CalcMixedGearScore(ilvl, quality)` (`StatsWeightCalculator.cpp:264-274`),
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

### Talent-driven stat weights

`GenerateBasicWeights` is flat per spec; **stat-conversion talents belong in
`GenerateAdditionalWeights`** (`StatsWeightCalculator.cpp:787-837`), gated on `HasAura` — safe there
because `Randomize` runs `InitTalentsTree` (`PlayerbotFactory.cpp:959`) before `InitEquipment` and
`ApplyEnchantAndGemsNew`. Convention: **added weight = conversion ratio × the target stat's weight**.
Careful Aim and Mental Dexterity each convert 100% of Intellect to attack power and each add 1.1
against an attack power weight of 1.0.

**`ITEM_MOD_SPELL_POWER` fills both `STATS_TYPE_SPELL_POWER` and `STATS_TYPE_HEAL_POWER`**
(`StatsCollector.cpp:562-565`). Healer branches weight only `HEAL_POWER`, so spell power still
collects that full weight and outscores anything below it — which is why holy paladins gemmed Runed
(pure spell power) in every socket while Intellect sat at 0.9.

**Holy paladin Intellect is 1.3** (0.9 + 0.4, `:829`). Holy Guidance rank 5 (`31841`) converts 20% of
total Intellect to healing power, and Divine Intellect and Blessing of Kings each add 10% on top, so
a point on gear is worth 0.24 healing — plus roughly 0.2 for the spell crit it carries
(`1.21 / 166.6 %` × `45.9` rating per % × crit weight `0.6`), value a flat weight cannot scale. Gated
on `PALADIN_TAB_HOLY` as well as the aura, because protection and retribution sit on a
`SPELL_POWER -2.0` branch that does not want Intellect. Measured at ilvl 80: yellow and blue sockets
move to Brilliant King's Amber and red to Luminous Ametrine, and 4 of 12 gear slots reorder toward
Intellect.

**Still unmodelled**, same aura family — `SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT` (174) and
`SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT` (175), stat index in `EffectMiscValue`, percent in
`EffectBasePoints + 1`: restoration shaman Nature's Blessing (`30867-30869`, 15% Intellect, but only
Healing Wave / Lesser Healing Wave / Riptide), priest Spiritual Guidance (`15031`, 25% Spirit),
restoration druid Improved Tree of Life (`48537`, 15% Spirit, form-gated). Each shares a weight
branch with specs that lack the talent, so each needs its own branch, never a base bump.

### Sim stat weights

Level-80 DPS bots take their weights from wowsim, not `GenerateBasicWeights`: `SimWeightsMgr` loads
`playerbots_sim_weights` (playerbots DB), one row set per spec and WotLK content phase, measured at
that phase's sim BiS gear and relative to an anchor stat (Strength for warriors, DKs and
retribution; Agility for rogues, hunters, feral and enhancement; spell power for casters).

- **Scope** (`SimWeightsMgr::ResolveKey`): no rows for tanks and healers (`IsTank`/`IsHeal`, i.e.
  the bot's strategies), Beast Mastery, or the protection/holy/restoration tabs. PvP specs and
  bots below 80 (rating per point changes with level) stay hand-written too. A non-Shadow priest
  that isn't healing reads the Smite rows (tab 13).
- **By gear level, not content phase.** The calculator interpolates linearly between the two
  phase rows around the bot's `GetAverageItemLevelForDF`, clamped to P1 and P5. `ProgressForBot`
  doesn't fit: it follows the group leader or IP's login default (phase 1 for an ungrouped
  level-80 bot here), while hit, expertise and armor pen move 1.5-2x from P1 to P5 with the bot's
  own gear.
- **Merge** (`SimWeights::Merge`, after `GenerateBasicWeights` and `GenerateAdditionalWeights`):
  each measured stat becomes sim weight × the hand-written anchor weight, so scores keep their
  scale. Unmeasured stats (stamina, armor, a caster's wand DPS) keep the hand value, and so does
  a negative hand weight the sim puts below 0.05, since those repel off-role items. Talent bumps on
  measured stats (Careful Aim, Mental Dexterity) are overwritten; the sim builds carry the talents.
- `ApplyWeightFinetune`'s armor pen ×1.2 is skipped: the phase rows already carry that rise.
- Resolved lazily, once per calculator, which copies what it needs: `Initialize` re-runs
  `LoadAll` on `.playerbots rndbot reload` while map threads score, so the snapshot is swapped
  under a mutex. A missing table (checked via `information_schema`, see
  [pitfalls](../engine/pitfalls.md#a-missing-table-or-column-kills-the-worldserver-not-just-the-query))
  or `SimWeights.Enable = 0` leaves everyone hand-written.

**Branch routing** in `GenerateBasicWeights`, at every level: a Blood DK that isn't tanking takes
the Unholy branch, a Frost or Unholy DK that is tanking takes the Blood branch, and a non-Shadow
priest that isn't healing takes the caster branch. Blood DK bots always get "tank assist", so Blood
DPS exists only when a user removes it.

**Cap-aware slot comparisons.** Runtime slot comparisons score with `SetOverflowPenalty(false)`,
so without more, a hit-capped bot would take hit gear over better items now that hit weighs ~2x.
`SetReplacedItem(item)` clips hit, expertise, armor pen and defense for both incumbent and
challenger to what the bot would still need with that item removed (template, random property,
enchants, gems). An unslotted score (the first `CalculateItem` in `QueryItemUsageForEquip`, `calc`,
gear generation) has no item to take out, so clipping it would count the bot's own hit against
it; those stay unclipped. `PLAYER_EXPERTISE` already includes the rating part
(`Player::UpdateExpertise`), so the expertise cap reads it alone.

**Regenerating:** run wowsim's `tools/statweights` (its README), then
`python apps/simweights/generate_weights.py --weights <weights.json>`, which writes a dated
`data/sql/playerbots/updates/*_playerbots_sim_weights.sql` for the module's DB updater.

**Known gaps:**
- `MELEE_DPS` takes the sim's main-hand slope, and `StatsCollector` counts an off-hand weapon's DPS
  under the same stat, so dual wielders overvalue off-hand weapon DPS (the off-hand slope is ~40%
  of the main hand's).
- `StatsCollector` gives feral weapons no feral attack power, so bear weapons (hand-written) score
  far too low: the sim puts bear threat at ~51 per weapon DPS against the bots' 3. Cats read the
  sim's weapon DPS slope instead.
- `InitEquipment` clears the gear first and uses one calculator for the whole gear-up, so a
  generated bot is weighted at P1 whatever tier it is geared from.
- Haste still steps between some phases, from real haste breakpoints the ±300 rating step doesn't
  average out: Elemental reads 0.64 then 0.99 (anchor units) from P3 to P4.
- The sim builds use presetgen's talents, race and professions; a bot on another talent build
  has slightly different true values.

## Set bonuses

Set scoring lives only in `StatsWeightCalculator::CalculateItemSetMod` (`:897-920`). It was
effectively dead until the delta model landed, for three separate reasons worth remembering:

- `multiplier += 0.1f * itemCount` applied only while `itemCount < max_items`, so a **complete** set
  reset to `1.0f` — stickiness was zero exactly when the bonuses were live.
- It read `player->ItemSetEff`, i.e. currently-equipped state, so when comparing a candidate against
  the incumbent set piece **the incumbent's own contribution counted toward both scores and
  cancelled out**. Breaking a bonus was never penalised.
- Piece count was linear; the real 2/4/6 thresholds in `ItemSetEntry::items_to_triggerspell[]` were
  ignored.

The fix is a delta model keyed on `SetReplacedItem(item)` — the piece occupying the contested slot,
treated as removed so incumbent and challenger measure against the same baseline:

```
base   = EquippedSetPieces(player, proto->ItemSet) - (replaced_item_set_ == proto->ItemSet ? 1 : 0)
gained = ActiveSetBonuses(set, base + 1) - ActiveSetBonuses(set, base)
multiplier = 1 + ItemSet.BonusWeight * gained
if (gained == 0 && NextSetThreshold(set, base))
    multiplier += ItemSet.ProgressWeight * (base + 1)
```

Consequences: T7 chest → T7.5 chest is neutral (same set, same vacated baseline); a non-set
challenger must beat the set piece by the bonus weight **on top of** `EquipUpgradeThreshold`.

Every runtime path that scores a slot must supply the replaced item — `QueryItemUsageForEquip`
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

### Socket bonuses

`BONUS_ENCHANTMENT_SLOT` is written in exactly one place server-wide:
`WorldSession::HandleSocketOpcode` (`ItemHandler.cpp:1387-1393`), which calls `Item::GemsFitSockets()`
and stores `proto->socketBonus`. Bots never send that opcode, so **every socket bonus on every bot was
dead**, even on items whose gems already colour-matched. `ApplyEnchantAndGemsNew` now mirrors it in a
final pass — which must run after the meta apply, because `GemsFitSockets` counts the meta socket too.

`GemsFitSockets` is all-or-nothing over *coloured template* sockets: an empty one fails, a colourless
(prismatic) one is skipped. A partial colour match is worth exactly zero.

**Placement is colour-blind by construction.** `pickBestGem` sees only `socketColor` and applies its
×1.2 nudge per socket in isolation, which cannot express all-or-nothing. A pass before the meta apply
hill-climbs pairwise **exchanges of already-placed gems**. Permutation-only is what makes it safe to
bolt onto this function: the gem multiset never changes, so total stats, the unique/`ItemLimitCategory`
budget and the global colour counts `Player::EnchantmentFitsRequirements` reads are all invariant — it
can only add bonuses, never cost one. Meta sockets live in their own vector and never take part.

Measured on an Ulduar-geared prot paladin: 54 stamina of socket bonuses on the gear, none applied; +18
after both fixes. The other five need gems the bot does not own — five yellow sockets against one
yellow-capable gem.

**Still out of scope:** changing which gems are *chosen*. A per-item "colour-matched candidates plus
the bonus" alternative must mirror `BestGemScore`'s eligibility filters exactly and interacts with the
cap escalation. `StatsCollector.cpp:81-85` still credits full `socketBonus` stats without checking
colour-match feasibility — much closer to true now that matched bonuses apply.

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

**Confirmed correct — do not re-audit:**

- **`enchant->slot` compared with `!=`** against `PERM_`/`TEMP_ENCHANTMENT_SLOT`
  (`PlayerbotFactory.cpp:5607-5610`), though the field is a bitmask (`EnchantmentSlotMask`,
  `ENCHANTMENT_CAN_SOULBOUND = 0x01`, `Item.h:199`). It rejects 131 entries with flags 3/6/7/9 — all
  shaman imbues, rogue poisons and warlock firestones, which `ImbueAction` owns separately.
- **A capped tank skipping Arcanum of the Stalwart Protector is right.** `ApplyOverflowPenalty` zeroes
  `STATS_TYPE_DEFENSE` once `GetRatingBonusValue(CR_DEFENSE_SKILL) >= DEFENSE_OVERFLOW`, so Arcanum
  (3818, +37 Sta +20 defense) scores 114.7 against Mind Amplification Dish's 139.5 (+45 Sta). Guides
  name Arcanum as the way to *reach* 540; a bot already there correctly values the defense at 0.

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

**Block value is weighted 0.7** (`StatsWeightCalculator.cpp:743`), raised from 0.5 so `3849 'Titanium
Plating'` (+81 block value, 56.7) takes a tank shield over `1071 '+18 Stamina'` (55.8) — a 0.9 margin,
so the shield enchant is not stable across gear changes. Gemming cannot move: no gem in the DBC carries
`ITEM_MOD_BLOCK_VALUE` or `ITEM_MOD_BLOCK_RATING`. Shield *selection* does move — `StatsCollector.cpp:54`
feeds `proto->Block` into the stat sum, and raid shields carry 0–259 of it (mean 223).

## Enhancement shaman gemming

Target: haste in yellow and blue sockets, Stark Ametrine (+20 AP +10 haste) as the red-socket bonus
filler, Relentless Earthsiege meta with one Tear. Each weight in the branch
(`StatsWeightCalculator.cpp:657-674`) holds a boundary, so **re-check gem picks before moving any**.
At level 80 the [sim weights](#sim-stat-weights) replace them and cross two of these: crit beats
Agility in P1-P3 (2.1 vs 1.8, so Chaotic Skyflare can take the meta) and haste passes 2.7 from P4.

- **HASTE 2.5, above 2.0:** below 2 AP per point, Bright Cardinal Ruby (+40 AP) beats Quick King's
  Amber (+20 haste). Under the ×1.2 colour nudge 2.5 gives Quick in yellow/blue and Stark in red. At
  2.2 Stark ties Nightmare Tear in red and jewelcrafters keep +68 AP Dragon's Eyes.
- **HASTE below ~2.7:** Thundering Skyflare's proc (`55380` → `55379`, 480 haste, 6 s, 40 s cooldown)
  averages ~62.6 haste in the collector and takes the meta above that.
- **AGILITY 1.8, above CRIT 1.5:** Relentless (+21 Agi) and Chaotic Skyflare (+21 crit) carry the same
  3% crit damage spell (aura 163, scored as 90 crit), so Agility vs crit alone picks the meta. 1.8 =
  1 AP + 0.55 crit rating per point (83.3 Agi vs 45.9 rating per 1%). At 1.4 bots ran Chaotic and fed
  its blue ≥2 condition with Balanced Dreadstones.
- **HIT 2.8, EXPERTISE 2.7, above haste:** under-cap bots still gem Rigid / Accurate / Precise;
  `ApplyOverflowPenalty` hands later sockets to haste once capped.

Relentless condition `149` is red ≥1 AND yellow ≥1 AND blue ≥1: Stark covers red and yellow, a Tear
(colour 14, all three) covers blue. Nightmare Tear `49110` and Enchanted Tear `42702` are
unique-equipped, so greedy or the meta fixup places exactly one — no factory special case.

Measured at ilvl 80, hit-capped: Relentless 207.4 vs Chaotic 199.8; red Stark 54, yellow Quick 60,
blue Tear 52.8 then Quick 50, jewelcrafter Quick Dragon's Eye 85. Gear #1 changes in 3 of 13 slots
(T10 shoulders and chest, Band of the Bone Colossus).

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

The lists carry gems and enchants per slot (the `enhs` sub-table), and the import still **drops
them** — a bot resolving to the lists alone still chooses both by score in
`ApplyEnchantAndGemsNew`. A bot resolving to the sim dataset instead gets them from its own block;
see below.

## Sim BiS dataset

`BisDatasetMgr` loads the BisTooltipAC export (`bistooltip_dataset`/`_subject`/`_block`, world DB,
owned by mod-bis-tooltip). `BisListMgr::ResolveSource` picks each bot's `BisSource` for the score
multiplier, the spec gates, `calc` and `PlanBisGear`: its roster subject (by character guid), else
its spec subject, else the ranked lists above. **Never mixed per item.** A subject counts only for a
WotLK-progression bot whose `ResolveSpecKey` class and tab it matches (role included, via the
sentinel tabs), and only with a block at or below the bot's phase cap. So a respec or role change
drops a roster bot to its spec subject or the lists, never onto a stale-role block.

`AiPlayerbot.BisDataset.Enable` gates the whole path, and a placeholder export (one payload shared
across ≥2 classes' subjects) is refused at load, so `Enable = 1` ships safe with no real data. A
refused or failed load keeps the snapshot already loaded; the version poll retries only a new
version.

The subject is read at its own **effective phase** — its latest phase at or below the bot's
progression cap that actually has a block, not the bot's raw cap — and
`StatsWeightCalculator::BisRankMultiplier` decays from that same phase. Decaying from the bot's
phase instead would flatten every item on a subject lagging the bot's own progression to 0.

**Exact vs palette.** The equipped item being the block's rank-1 pick for its slot ("exact") is
what unlocks the sim's gems and reforge on that item. Any item in the slot gets the slot's sim
enchant when it fits (`IsFitToSpellRequirements` etc.). Every socket without a pin picks from the
**palette** first — the block's gem ids across all slots, filtered through the same eligibility
check as the gem cache — and from the full pool only when no palette gem fits. Weapons match
strictly per hand (Titan's Grip is two independent matches; a 2H with no sim off-hand slot leaves
it to the ordinary picker). Rings try the same finger first, then cross; finger fallback picks the
sim's ring whose slot no exact match has claimed. Trinkets take nothing from the block.

**Pin alignment**, after `ApplyPrismaticSocket`, comparing the sim's gem count `n` against the
item's template sockets `T` (meta included) and its prismatic socket `P` (0 or 1):

| Case | Mapping |
|---|---|
| `n == T+P` | positional; gem `T` goes to the prismatic socket |
| `n == T+1`, `P == 0` | the sim had a prismatic this item lacks: drop the trailing gem |
| `n == T`, `P == 1` | pin the template sockets; the prismatic goes to the picker |
| otherwise, or a meta/non-meta color mismatch | pin nothing — dropped empty sockets make the rest ambiguous |

Pins are budgeted **before** the picker runs, so they get first claim on the unique-equipped and
`ItemLimitCategory` budget; a pin failing `gemEnchantIfUsable` or the budget falls through to
palette → full pool exactly like an unpinned socket. Later passes skip pinned sockets outright;
only meta-activation steering may still touch one, and only after every unpinned socket has been
tried and failed.

**Enchant validity, not the cache path.** The dataset enchant skips the enchant cache's blacklist,
its enchant-flags check and the engineering-cloak rule on purpose — the sim already chose from what
the server can actually hand out. It still has to pass `spellGatesPass` (`LimitEnchantExpansion`,
`IsFitToSpellRequirements`, `BaseLevel`, `IsEnchantSpellAllowed`) and the enchant's own
`requiredSkill`/`requiredLevel`. Precedence per slot: the dataset enchant, then
`GetRuneforgeEnchantId` (frost DK runeforges), then the cache scan.

**Reforges** run only on an exact rank-1 item with `AiPlayerbot.BisDataset.Reforges` on, and an
exact item the sim left unreforged loses any reforge it has, caps permitting (below). Every map
thread reads mod-reforging's `reforgingDataMap` when it applies item mods, so a write from a bot's
map thread would race the others: the map thread only queues `{bot, item, entry, from, to}`
(`src/Bot/Factory/BisReforge.cpp`; `from == to == 0` requests removal), and the world thread
applies it after `sMapMgr->Update` has waited out every map thread. Switching to a different
reforge takes two `ApplyEnchantAndGemsNew` runs: the first only removes the old one. At most one
write per item per 10s, since `RemoveReforge`'s DELETE and `Reforge`'s INSERT share the
`character_reforging` row and can race once `CharacterDatabase.WorkerThreads > 1`.

**Caps overrule the sim's reforge.** The sim reforges for its full set, so on a partly geared bot
its `hit -> haste` can drop hit below the cap. The world thread takes whichever of the sim's
reforge, the item's current one and none leaves the bot least rating short of its caps, ties in
that order (`src/Bot/Factory/ReforgeCaps.h`): a bot capped either way follows the sim, one below
cap keeps an old reforge into hit. The caps are `StatsWeightCalculator::CapRoom`, the ones its
overflow penalty clips scores at: hit except for healers, expertise for melee only.

**This depends on mod-reforging's local, uncommitted lock patch** (`modules/mod-reforging`). Once
bots carry reforges, mod-reforging itself erases their entries on map threads whenever a bot
destroys, trades, mails or auctions a reforged item or auto-unequips its off-hand, while other map
threads read the map; the `shared_mutex` the patch adds around it makes that safe.

**Keep `Reforging.Enable` fixed at runtime.** mod-reforging's own reload
(`ItemReforge::HandleReload`) walks only `sWorldSessionMgr`'s sessions, which hold no bots —
toggling it leaves already-reforged bots' stats drifted from their DB row until they relog.

**Deliberate `BestGemScore` exception, the second one** (the tank cap-priority flag above is the
first): the palette narrows *placement* — which gem a socket gets — never *valuation*.
`BestGemScore` never sees it, so socket scoring stays independent of which subject a bot resolved
to.

**Known gaps:** acbis's roster exports carry no healer blocks, so healers stay on the lists. acbis
labels a raider "Blood tank" or "Feral tank" only when the raid's main-tank flag is set, so an
unflagged tanking DK or bear misses its roster subject and falls to a tank spec subject or the
lists. Pinned gems follow the sim whatever the bot's caps; only unpinned sockets are scored
against them.

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
| `AiPlayerbot.BisDataset.Enable` | 1 | Read `bistooltip_*` and prefer it per bot; no-op without the tables or with a placeholder export |
| `AiPlayerbot.BisDataset.Enhancements` | 1 | Dataset enchants, pinned gems, palette; 0 leaves gems and enchants to the normal picker |
| `AiPlayerbot.BisDataset.Reforges` | 1 | Sim reforge on exact rank-1 items; no-op without mod-reforging or `Reforging.Enable = 0` |
| `AiPlayerbot.BisDataset.PollSeconds` | 300 | World-thread version check; 0 = startup and the command only |
| `AiPlayerbot.SimWeights.Enable` | 1 | Level-80 DPS weights from `playerbots_sim_weights`; no-op without the table |

Debug: `<bot> calc [item]` prints a raw score (`TellCalculateItemAction`, registered as `calc`) plus
a BiS line naming the source (lists or dataset), subject, effective phase and rank, and a weights
line: `weights: sim <class>/<tab>, ilvl <x> (P<a> -> P<b> at <pct>%)` or `hand-written (<reason>)`. `.ip set <n>`
sets a test character's IP tier. `.playerbots bis reload` (admin) forces a dataset reload; `.playerbots
bis status` (gamemaster) reports what's loaded, plus dataset enchants without
`SPELL_EFFECT_ENCHANT_ITEM` and gems without `GemProperties`.
