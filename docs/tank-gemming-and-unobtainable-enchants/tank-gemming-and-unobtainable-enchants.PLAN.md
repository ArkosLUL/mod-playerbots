# Tank gearing fixes: unobtainable leg armor + cap-aware gemming

## Context

Two problems found while tracing why tank bots show `+72 Stamina and +35 Agility` on their legs.

**1. Bots apply two enchants no player can ever get.** Verified against `SpellItemEnchantment.dbc`
and `Spell.dbc` in the `ac-worldserver` container plus `acore_world`:

| Enchant | Text | Applied by | Source item | Craft spell |
|---|---|---|---|---|
| 3331 | `+72 Stamina and +35 Agility` | spell 50911 | 38377 Dragonscale Leg Armor | 50968 |
| 3332 | `+100 Attack Power and +36 Critical Strike Rating` | spell 50913 | 38378 Wyrmscale Leg Armor | 50969 |

Both source items are `RequiredSkill=165` (Leatherworking) `RequiredSkillRank=410`, BoP. Their
craft spells **50968 and 50969 are taught by no trainer and no recipe item** — they are the only
two WotLK leg-armor crafts in that state; 50964 (Jormungar), 50965 (Frosthide), 50966 (Nerubian),
50967 (Icescale) and 62448 (Earthen) all have `npc_trainer` rows. The items also have no
`npc_vendor` or loot-template source. Leftover Blizzard data that never shipped a pattern.

The enchant loop in `PlayerbotFactory::ApplyEnchantAndGemsNew` only gates on
`enchant->requiredSkill`, which is 0 for these — the profession lock lives on the source *item*, not
the enchant (the same trap already documented for Jeweler's gems in the gem loop). So every bot
beats the best obtainable leg armor (Frosthide, `+55 Sta / +22 Agi`) for free.

**2. Tanks gem pure stamina, even when not crit-immune.** `pickBestGem` is a greedy per-socket
argmax on `StatsWeightCalculator::CalculateEnchant`. For prot warrior/paladin weights (Sta 3.0+0.1
base, Defense 2.5, Expertise 3.0): a +30 Sta gem scores 93, a +20 Defense-rating gem scores 50, a
+20 Expertise-rating gem 60. Stamina wins every socket unconditionally, so a bot below 540 defense
never gems its way to crit immunity — the one stat that is a hard requirement rather than a
trade-off.

**Not a bug:** `DEFENSE_OVERFLOW = 140` in `src/Mgr/Item/StatsWeightCalculator.h` is correct.
`GetRatingBonusValue(CR_DEFENSE_SKILL)` returns defense *skill from rating*, and base skill at 80
is 400, so 140 is exactly the 540 crit-immunity cap. No change there.

## Change 1 — blacklist the two unobtainable leg armors

`src/Bot/Factory/PlayerbotFactory.cpp`, the enchant-spell cache build (~line 471-484) already
carries a list of `continue` guards for junk spell ids (test enchants, grandfathered TBC, Naxx40
shoulder enchants). Add one more guard in the same style:

```cpp
// Dragonscale / Wyrmscale Leg Armor: their crafts (50968/50969) have no trainer and no
// pattern, so no player can ever wear these.
if (id == 50911 || id == 50913)
    continue;
```

Blacklisting the two spells is enough — a sweep of every `class=0 subclass=6` enhancement item at
ilvl >= 74 turned up no other unobtainable entry. The `zzDEPRECATED` tailoring spellthreads
(41605/41606) look similar but apply the same enchant spells as the live Brilliant/Sapphire
Spellthread, so they are harmless.

This keeps them out of `enchantSpellIdCache` entirely, so both the factory enchant pass and
`ItemUsageValue`-side scoring stop seeing them. Bots fall back to Frosthide / Icescale / Earthen
Leg Armor.

Explicitly **out of scope**: a general "source item requires a profession the bot lacks" gate for
the enchant loop. Worth doing eventually (it would cover obtainable profession-only enhancements),
but it needs a spell→source-item map built from `item_template.spellid_1..5`, and it would not have
caught this case for a leatherworking bot anyway.

## Change 2 — cap-aware defense weighting for gems and enchants

Goal: while a tank is below the defense cap, defense gems/enchants outscore stamina; once capped,
behaviour returns to today's stamina spam. The existing `ApplyOverflowPenalty` already truncates
`STATS_TYPE_DEFENSE` to the remaining-to-cap amount, so the escalation self-terminates — no
oscillation, no overshoot beyond a partial gem.

**`src/Mgr/Item/StatsWeightCalculator.h`**
- Add `bool enable_cap_priority_ = false;` and a `SetCapPriority(bool)` setter.
- Add a constant for the boosted weight next to the `*_OVERFLOW` enum, e.g.
  `DEFENSE_UNDERCAP_WEIGHT = 10.0f`.

**`src/Mgr/Item/StatsWeightCalculator.cpp`, `ApplyWeightFinetune` (line 1071)**
- When `enable_cap_priority_` is set, the calculator is a tank type (`type_ & MELEE_TANK`), the class
  is not druid, and `player->GetRatingBonusValue(CR_DEFENSE_SKILL) < DEFENSE_OVERFLOW`, raise
  `stats_weights_[STATS_TYPE_DEFENSE]` to `DEFENSE_UNDERCAP_WEIGHT`.
- Druids are excluded: Survival of the Fittest gives bears crit immunity without defense, which is
  why the bear branch weights defense at 0.3. Boosting it would send every socket and enchant into a
  dead stat permanently, since a bear never reaches `DEFENSE_OVERFLOW` for the boost to switch off.
- Weight sizing: a +20 defense gem must beat a +30 stamina gem including the ×1.2 socket-colour
  nudge on both sides — needs > ~4.7; 10.0 stays clear of that threshold without being
  so large that the truncated last gem misleads the comparison.
- Leave expertise on its normal weights. Expertise is a soft DPS/threat gain, not a survival
  requirement, and WotLK tanks legitimately eat/gear for it rather than gemming it.

**`src/Bot/Factory/PlayerbotFactory.cpp`, `ApplyEnchantAndGemsNew` (~line 5250)**
- Call `calculator.SetCapPriority(true)` on the single calculator used by both the per-slot enchant
  scan and `pickBestGem`. Both re-read live player stats through `ApplyOverflowPenalty` on every
  `CalculateEnchant` call, and each gem is applied to the bot before the next socket is scored, so
  the remaining-to-cap shrinks as defense gems go in and later sockets naturally revert to stamina.

**Deliberately not propagated to item scoring.** `StatsWeightCalculator::BestGemScore` (line 786)
builds a throwaway `gemCalculator` for socket-value estimation during `CalculateItem`; leave its
flag off. Turning it on there would make an under-cap tank re-rank whole gear pieces and risk
equip/unequip churn as it crosses the cap. Consequence: for an under-cap tank, the socket-bonus
estimate on candidate items is slightly conservative — acceptable, and worth a short comment next
to the existing "Has to match ApplyEnchantAndGemsNew exactly" note (line 818) so the mismatch is
intentional and documented.

## Verification

No headless build is possible in this environment — compilation and in-game checks are a hand-off
(see the build-verification note). Static checks that can be done here:

1. Re-run the DBC/DB queries to confirm 50911/50913 are the only unobtainable leg-armor enchant
   spells reaching the cache:
   `docker exec ac-worldserver cat /azerothcore/env/dist/data/dbc/SpellItemEnchantment.dbc` parsed
   for stat enchants, cross-checked against `item_template` for a teaching recipe.
2. Confirm no other call site constructs `StatsWeightCalculator` expecting the old gem behaviour
   (grep for `SetPvpSpec` / `CalculateEnchant` users).

In-game, after a rebuild:

3. `.playerbot` a fresh prot warrior / blood DK and a fresh melee DPS; inspect legs — should show
   Frosthide (`+55 Sta / +22 Agi`), Icescale (`+75 AP / +22 crit`) or Earthen, never
   `+72 Sta / +35 Agi` or `+100 AP / +36 crit`.
4. Inspect a tank bot geared below 540 defense: sockets should hold defense-rating gems up to the
   cap, then stamina gems for the rest.
5. Inspect a fully-geared, already-capped tank bot: all-stamina gemming, unchanged from today.
