# Holy Paladin / Discipline Priest heal priority fixes

## Context

After commits `d06538b23` (Holy paladin rotation improvements) and `2ba4b0dba` (Priest strategy
improvements), two healer rotations pick the wrong primary heal in raids:

1. **Holy Paladin spams Flash of Light instead of Holy Light.** In WotLK, Holy Light (glyphed, with
   Beacon of Light and Illumination) is the throughput and mana-efficiency heal; Flash of Light is
   the cheap top-off.
2. **Discipline Priest spams Flash Heal and almost never casts Penance.** Penance should be the
   priority direct heal for Disc.

Both are relevance-ladder bugs, not casting bugs. Root causes are confirmed below.

---

## Root cause 1 — Holy Paladin

Two independent causes, both need fixing.

**a) The 65–85% HP band contains only Flash of Light.**
[HealPaladinStrategy.cpp:124-131](modules/mod-playerbots/src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L124-L131) —
the `party member almost full health` node has a single entry, `flash of light on party` @13. Holy
Light appears only in the critical (34), low (25), medium (19) and Infusion-of-Light (26.5) nodes,
i.e. only below 65% target HP. In a raid most heal targets sit between 65% and 85%, so Flash of
Light is what actually gets cast almost every global.

**b) The mana saver hard-vetoes Holy Light.**
[ConserveManaStrategy.cpp:92-130](modules/mod-playerbots/src/Ai/Base/Strategy/ConserveManaStrategy.cpp#L92-L130)
`HealerAutoSaveManaMultiplier::GetValue` returns `0.0f` when bot mana <= `saveManaThreshold` (60) and
target HP >= `mediumHealth` (65) and the action's `manaEfficiency <= MEDIUM`.
`CastHolyLightOnPartyAction` is tagged `MEDIUM`, `CastFlashOfLightOnPartyAction` is `HIGH`
([PaladinActions.h:167-185](modules/mod-playerbots/src/Ai/Class/Paladin/Actions/PaladinActions.h#L167-L185)).
So below 60% bot mana Holy Light is dead in exactly the band this fix targets — reordering the
ladder alone would not help.

The tag is backwards for WotLK: Illumination refunds 30% of mana on a heal crit and Beacon mirrors
the heal for free, which makes Holy Light the efficient spell per point healed.

## Root cause 2 — Discipline Priest

Penance already outranks Flash Heal inside every health band
([HealPriestStrategy.cpp](modules/mod-playerbots/src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp):
crit 35.5 vs 34.5, low 25.5 vs 24.5, medium 18.5 vs 17.5). The problem is a *separate* node that
outranks both:

[HealPriestStrategy.cpp:91-98](modules/mod-playerbots/src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L91-L98)

```cpp
triggers.push_back(new TriggerNode("weakened soul on party member",
    { NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 7) }));   // 27
```

`WeakenedSoulOnPartyMemberTrigger` ([PriestTriggers.cpp:105-113](modules/mod-playerbots/src/Ai/Class/Priest/PriestTriggers.cpp#L105-L113))
fires whenever the heal target is below 85% HP and carries Weakened Soul. Power Word: Shield sits at
the top of every band (36 / 26 / 19 / 13) plus `power word: shield on not full` at 32–33, so the bot
shields constantly and Weakened Soul (15s) is up on the heal target nearly all the time. Flash Heal
@27 then beats Penance's 25.5 and 18.5 on every non-critical heal. Only the critical band's
Penance @35.5 ever wins.

Verified not to be the cause: Penance is in the Disc premade talent build (`PremadeSpecLink.5.0.80`
ends in `1`), `spell_pri_penance::CheckCast` passes on friendly targets, `SpellIdValue` resolves it,
`HealerAutoSaveManaMultiplier` treats Penance and Flash Heal identically (both `HIGH`, `estAmount 15`),
and `CastTimeMultiplier` only applies to actions aimed at `current target`, never party heals.

## Glyphs — already correct, no change

`AiPlayerbot.PremadeSpecGlyph.2.0` (holy pve) in
[playerbots.conf.dist:2068](modules/mod-playerbots/conf/playerbots.conf.dist#L2068) is
`41106,43367,45741,43368,43365,41109`, which resolves against `acore_world.item_template` to:

| Slot | Item | Glyph |
|---|---|---|
| major 1 | 41106 | Glyph of Holy Light |
| minor 1 | 43367 | Glyph of Lay on Hands |
| major 2 | 45741 | Glyph of Beacon of Light |
| minor 2 | 43368 | Glyph of Sense Undead |
| minor 3 | 43365 | Glyph of Blessing of Kings |
| major 3 | 41109 | **Glyph of Seal of Wisdom** |

`PlayerbotFactory::InitGlyphs` indexes this by `AiFactory::GetPlayerSpecTab(bot)`, which is 0 for
Holy, so Holy Paladin bots already get exactly the requested set. Seal handling is also already
right: `seal of wisdom` is the *only* seal any Holy Paladin node ever casts
(`HealPaladinStrategy.cpp:110`); Seal of Light is never wired for Holy. Nothing to change — this
also means Glyph of Holy Light (10% splash to 5 nearby allies) is another reason Holy Light must be
the primary heal.

---

## Changes

### 1. Holy Paladin — put Holy Light in the 65–85% band

[src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp](modules/mod-playerbots/src/Ai/Class/Paladin/Strategy/HealPaladinStrategy.cpp#L124-L131),
`party member almost full health` node:

```cpp
NextAction("holy light on party",    ACTION_LIGHT_HEAL + 4),   // 14, new
NextAction("flash of light on party", ACTION_LIGHT_HEAL + 3)   // 13, unchanged
```

14 is free (medium band Flash of Light is 18, `paladin divine plea` is 12). Flash of Light stays as
the fallback for when Holy Light is vetoed or impossible; the existing
`holy_light_on_party` ActionNode already lists `flash of light on party` as its alternative
([GenericPaladinStrategyActionNodeFactory.h:163-169](modules/mod-playerbots/src/Ai/Class/Paladin/Strategy/GenericPaladinStrategyActionNodeFactory.h#L163-L169)),
so no factory change is needed. Do **not** add the reverse alternative — the comment there warns it
closes a cycle in node expansion.

### 2. Holy Paladin — fix the mana-efficiency tag

[src/Ai/Class/Paladin/Actions/PaladinActions.h:167-172](modules/mod-playerbots/src/Ai/Class/Paladin/Actions/PaladinActions.h#L167-L172):

```cpp
CastHolyLightOnPartyAction(PlayerbotAI* botAI)
    : HealPartyMemberAction(botAI, "holy light", 25.0f, HealingManaEfficiency::HIGH) {}
```

`MEDIUM` -> `HIGH`. Leave `estAmount` at 25 and leave Flash of Light at `HIGH`.

Resulting behaviour at bot mana <= 60%: the `lossAmount < estAmount` arm still vetoes Holy Light
above ~75% target HP (overheal guard), so Holy Light covers 65–75% and Flash of Light covers 75–85%.
Above 60% bot mana Holy Light wins the whole band. This is the intended split.

### 3. Discipline Priest — Penance above the Weakened Soul Flash Heal

[src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp:91-98](modules/mod-playerbots/src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L91-L98),
`weakened soul on party member` node:

```cpp
NextAction("penance on party",   ACTION_MEDIUM_HEAL + 7.4f),  // 27.4, new
NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 7)     // 27, unchanged
```

27.4 avoids the collision with `low health` -> `power word: shield` @27.5. With this, whenever the
target is shielded and Penance is off cooldown, Penance goes out; Flash Heal takes over during the
Penance cooldown. Penance stays `HIGH` / `estAmount 15`, so the mana saver does not veto it in this
band. Update the node's comment to say the shielded target gets Penance first, Flash Heal while
Penance is on cooldown.

### 4. Discipline Priest — give `penance on party` an ActionNode

[src/Ai/Class/Priest/Strategy/GenericPriestStrategyActionNodeFactory.h](modules/mod-playerbots/src/Ai/Class/Priest/Strategy/GenericPriestStrategyActionNodeFactory.h) —
`penance on party` is currently the only party heal with no node, so it gets neither the
`remove shadowform` prerequisite nor a fallback. Add, matching the shape of `flash_heal_on_party`:

```cpp
creators["penance on party"] = &penance_on_party;
...
static ActionNode* penance_on_party([[maybe_unused]] PlayerbotAI* botAI)
{
    return new ActionNode("penance on party",
                          /*P*/ { NextAction("remove shadowform") },
                          /*A*/ { NextAction("flash heal on party") },
                          /*C*/ {});
}
```

Engine behaviour this buys: when Penance is on cooldown the action is `IMPOSSIBLE`, and
[Engine.cpp:238](modules/mod-playerbots/src/Bot/Engine/Engine.cpp#L238) pushes the alternative at
`relevance + 0.003` in the same tick instead of leaving the heal to a later queue entry.

### 5. Docs

- [docs/classes/paladin.md:47-48](modules/mod-playerbots/docs/classes/paladin.md#L47-L48) — the
  `tank to beacon` hysteresis margin is now `75.0f` after `c4ead0074`, not 30. Correct the number.
  Worth also noting that at 75 the swap condition is true in nearly every realistic case, so Beacon
  effectively sticks to the first tank it picks until that tank leaves combat or range.
- `docs/classes/paladin.md` — record the 65–85% Holy Light node and the `MEDIUM -> HIGH` retag with
  the reason (Illumination + Beacon + Glyph of Holy Light), and note the glyph set is verified
  correct so it does not get re-litigated.
- `docs/classes/priest.md` — record that the `weakened soul` node leads with Penance, and why the
  earlier ordering silently suppressed Penance across the low and medium bands.

---

## Verification

Static (no compiler available in this environment — see the build-verification note):

1. Re-read both ladders and confirm no relevance collisions were introduced. New values are 14
   (paladin almost-full band) and 27.4 (priest weakened-soul node); grep each strategy file for
   those numbers to confirm they are unique within the file.
2. Confirm `holy light on party` now appears in four paladin nodes (critical 34, infusion 26.5,
   low 25, medium 19, almost-full 14) and that Flash of Light sits directly below it in each.

In-game, on a built server:

3. Spawn a Holy Paladin bot and a Disc Priest bot in a raid group, pull a boss (Naxx works — the
   raid strategies are already wired), and watch with `.bot debug spell` on each bot, or enable
   `AiPlayerbot.LogValuesPerTick` and read the `A:<action> - OK` lines in the playerbots log.
   - Holy Paladin: `holy light on party` should be the dominant `OK` line while the paladin is above
     60% mana; `flash of light on party` should appear mainly above 75% target HP at low mana.
   - Disc Priest: `penance on party` should show up roughly every 10s (its cooldown), with
     `flash heal on party` filling the gaps.
4. Sanity-check mana longevity on the paladin over a full fight — Seal of Wisdom + Glyph of Seal of
   Wisdom + Divine Plea should keep it going; if Holy Light now drains it faster than before, the
   `estAmount` overheal guard (25) is the dial to raise, not the efficiency tag.
5. Confirm glyphs with `.bot glyphs` (or inspect the bot) on a freshly rolled level-80 Holy Paladin:
   Holy Light, Beacon of Light, Seal of Wisdom. Existing bots keep their old glyphs until a
   re-roll/maintenance pass, so test on a new bot.

## Out of scope

- `holy shock on party` in the paladin 65–85% band (would keep the Infusion of Light proc engine
  rolling) — deliberately not included.
- `SealTrigger::IsActive` ([PaladinTriggers.cpp:14-21](modules/mod-playerbots/src/Ai/Class/Paladin/PaladinTriggers.cpp#L14-L21))
  returns true when Seal of Wisdom is already up and mana > 70, which queues a `seal of wisdom` node
  every tick that then resolves as `USELESS`. It costs one engine iteration and does not affect seal
  uptime. The clause exists for the DPS/tank ladders, so changing it means a Holy-specific trigger —
  noted, not fixed here.
