# Paladin — Holy

Engine and healer semantics (health bands, `HealerAutoSaveManaMultiplier`) are in
[../engine/action-selection.md](../engine/action-selection.md).

## What was wrong

1. **Divine Plea halved the paladin's healing for most of every fight.** It was bound to the
   `high mana` trigger, which despite its name is `mana < 65`. At relevance 20 it outranked the medium
   band (19/18) and almost-full band (13). In 3.3.5 Divine Plea reduces healing done by 50% for 15s
   unless glyphed — so roughly 30 seconds into any raid fight, the paladin repeatedly crippled its own
   throughput. No glyph check, no raid-damage suppression.
2. **Holy Shock, the spec's rotational spell, fired almost nowhere.** Its only healing node was
   `party member critical health` @36, i.e. below 25% HP. The WotLK rotation is Holy Shock on cooldown
   to proc Infusion of Light, then spend the proc on a discounted Flash of Light — and there was no
   Infusion of Light awareness anywhere. Worse, `PaladinHealerDpsStrategy` put **offensive** Holy Shock
   at 5.5, so the cooldown was spent on damage during lulls and down when damage started.
3. **`estAmount = 50` disabled Holy Light exactly when it was needed.** The *ordering* was correct —
   Holy Light leading the low and medium bands is the WotLK rotation, not a mistake to be "fixed" into
   Flash spam. The bug was purely the mana-efficiency metadata: at `50.0f, MEDIUM`, below 60% bot mana
   Holy Light was vetoed at target HP ≥ 65 always and at HP ≥ 45 unless the target had lost ≥50% of
   its health. A Holy Light heals nowhere near 50% of a raider's pool. `holy shock on party` had the
   same problem at `25.0f, LOW`.
4. **The fallback chain pointed the wrong way.** `flash of light on party` → `holy light on party`
   escalates from a cheap fast heal to a slower more expensive one. The direction that matters — Holy
   Light vetoed, fall back to Flash — did not exist, so a vetoed Holy Light dead-ended. The second
   pass added the reverse edge; **the third pass removed it again**, deliberately — see below.
5. **Beacon of Light and Sacred Shield were main-tank-only and never re-evaluated.** In a two-tank
   raid the off-tank got neither, and Beacon never followed a tank swap. Beacon multiplies everything
   the paladin casts, so this was the largest single output item.
6. **`reach party member to heal` @93 outranked every emergency heal** — an out-of-range selected
   target made the paladin run instead of using Lay on Hands (92) on a different dying player. Settled
   at 39.5. (Priest uses 40 for the same node; the shaman rebase settled on 40.)
7. **The default action was an ungated melee-range mana spend.** `getDefaultActions()` returned
   `judgement of light` @5.0, pushed every tick, bypassing the `HealerShouldAttackTrigger` mana floor
   that gates the *identical* node inside `healer dps` @5.3. It did **not** drag the healer toward the
   boss, as an earlier draft claimed — `CastSpellAction::getPrerequisites()` returns `{}`, so an
   out-of-range Judgement simply fails that tick.
8. **`healer dps` carried two entries wrong for Holy** — `shield of righteousness` requires a shield
   (Protection only) and `consecration` is a ~660-mana self-centred ground AoE at melee range.

## Deviations from the plan, as built

- The Divine Plea fallback value key is `"main tank"`, not `"main tank member"` — the latter is the
  value's own name, the former its registration key.
- Divine Plea is allowed through the raid-damage suppression **below** `lowMana` (15), and a second
  `low mana` → `divine plea` node sits at 23. An unglyphed healer with an empty bar heals nothing, so
  the 50% penalty is the cheaper price there.
- The `tank to beacon` hysteresis margin is 75 percentage points (`SWAP_MARGIN_PCT`, raised from 30 in
  `c4ead0074`), not 20. **Without hysteresis Beacon thrashes every GCD.** At 75 the swap condition is
  true in nearly every realistic case, so in practice Beacon sticks to the first tank it picks until
  that tank leaves combat, range or LOS.
- **Judgement of Light is maintained on a debuff trigger at 27, which the plan did not have at all.**
  Dropping the ungated default (item 7) left `healer dps` @5.3 as the only path, and that node is
  gated by `HealerShouldAttackTrigger` — false whenever anyone is below 85% HP, i.e. most of a fight.
  Judgements of the Pure (15% haste, 60s) plus the melee-swing raid heal are worth one global per 20s.
  `PaladinJudgementOfLightTrigger` is a `DebuffTrigger` with `checkIsOwner = true` (a second paladin's
  Judgement of Light must not stop this one refreshing its own Judgements of the Pure) and calls
  `CanCastSpell`, so the node goes quiet at range instead of failing every tick. Judgement is 10 yards
  and the node never queues movement — it waits.
- **Divine Illumination is pinned to Avenging Wrath** rather than standing on a mana node.
  `CastDivineIlluminationAction::isUseful` requires the `avenging wrath` aura for a healer, so the
  `medium group heal setting` pair (Avenging Wrath 31, Divine Illumination 30.5) always lands in that
  order on consecutive ticks and the −50% cost window covers the +20% healing window. Both are
  3-minute cooldowns, so they stay aligned.

## Current Holy ladder

`HealPaladinStrategy.cpp`, on top of `GenericPaladinStrategy`:

| Rel | Trigger | Action |
|---|---|---|
| 39.5 | party member to heal out of spell range | reach party member to heal |
| 37 / 36 | beacon of light on tank / party member critical health | beacon of light on tank / holy shock on party |
| 35.5 / 35 | party member critical health / medium group heal setting | divine favor / divine sacrifice |
| 34 | party member critical health | holy light on party |
| 32 | sacred shield on tank | sacred shield on tank |
| 31 / 30.5 / 30 | medium group heal setting | avenging wrath / divine illumination / aura mastery |
| 27 / 26.5 | paladin judgement of light / infusion of light | judgement of light / **flash of light on party** |
| 26 / 25 | party member low health | holy shock / holy light on party |
| 23 / 22 | low mana | divine plea / divine illumination |
| 20 | seal | seal of wisdom |
| 19.5 / 19 | party member medium health | holy shock / holy light on party |
| 14 | party member almost full health | holy light on party |
| 12 | paladin divine plea | divine plea |

`infusion of light` @26.5 is the **only** node in the Holy ladder that casts Flash of Light — see
below.

Inherited from `GenericPaladinStrategy`: `hand of sacrifice on party` 93,
`blessing of protection on party` 92.8, `lay on hands on party` 92, `lay on hands` 91,
`divine shield` 90, `flash of light` 89 / `holy light` 88 (inside the bubble),
`hand of freedom on party` 24, three Hammer of Justice interrupts at 40.

## Cooldown gaps still open

| Cooldown | State |
|---|---|
| **Aura Mastery** | Was absent entirely — no action, no trigger, no spell id 31821 anywhere |
| **Hand of Sacrifice** (6940), **Hand of Salvation** (1038) | Exist only as exclusion constants in `PaladinHelper.h` |
| **Divine Sacrifice** | Two nodes at identical relevance 35. Single-target critical health is the wrong trigger for a raid damage-redirect, and `isUseful` had no self-health floor, so a low-HP paladin redirected raid damage onto itself |
| **Lay on Hands** | Forbearance guards now exist on both variants (`PaladinActions.cpp:590,595`) — this row records the original finding, not the current state |
| **Divine Favor** | Was on `low mana` (15) — a guaranteed-crit-next-heal cooldown gated on being nearly OOM. Its purpose-built trigger was registered but dead, since its only consumer `PaladinBoostStrategy::InitTriggers` was entirely commented out |
| **Divine Protection** | No direct trigger; reachable only as the `/*A*/` fallback of `divine shield`. `divine protection on party` is registered and referenced by zero nodes |
| **Avenging Wrath** | Also on the burst registry, so `HoldBurstUntilTankEngagedMultiplier` can suppress it for a healer — a healer exemption is required |

Hand of Protection depends on `PartyMemberToProtect` being live — see the revival note in
[shaman.md](shaman.md).

## Holy Light over Flash of Light (second pass)

The first pass fixed `estAmount` but left two things that still made Flash of Light the spell the bot
actually cast in a raid:

- **The 65–85% band held only `flash of light on party` @13.** Raid targets sit in that band most of
  the time, so almost every global went to Flash regardless of what the lower bands said. Holy Light
  now leads it at 14. (Flash sat at 13 as the fallback until the third pass removed it.)
- **`CastHolyLightOnPartyAction` was still tagged `MEDIUM`** while Flash was `HIGH`, so
  `HealerAutoSaveManaMultiplier` returned `0.0f` for Holy Light whenever bot mana ≤ 60 and target
  HP ≥ 65 — the exact band above. Retagged `HIGH`. Illumination refunds 30% of the mana on a heal crit
  and Beacon mirrors the heal for free, which makes Holy Light the cheaper spell per point healed;
  `MEDIUM` had the WotLK economics backwards. `estAmount` stays 25, so the `lossAmount < estAmount`
  arm still keeps Holy Light off targets above ~75% HP when the paladin is low on mana.

## Infusion of Light was spent backwards (third pass)

The second pass wired `infusion of light` @26.5 to `holy light on party`. That is the wrong half of
the talent. Verified 3.3.5 behaviour (talent 53569/53576, proc aura 53672/54149): a Holy Shock crit
**reduces the cast time of the next Flash of Light by 0.75/1.5s** and increases the crit chance of
the next **Holy Light** by 10/20%. Holy Light gets the crit for free whatever the ladder does; the
cast-time cut is the half that needs a node, and it belongs to Flash.

At 2/2 the reduction is the whole 1.5s base cast, which has two consequences:

- **The instant Flash is castable while moving.** `PlayerbotAI::CanCastSpell` vetoes any spell with a
  cast time while `bot->isMoving()`, reading `SpellInfo::CalcCastTime(bot)` — which runs
  `ApplySpellMod(SPELLMOD_CASTING_TIME)`, so the proc is already accounted for. With the proc up the
  bot sees 0 and the veto does not fire. Besides Holy Shock this is the only heal a moving Holy
  paladin can land.
- **On a Sacred Shielded target it also drops a 12s HoT** (66922), via
  `spell_pal_infusion_of_light::HandleProc` in the core. The ladder already keeps Sacred Shield on the
  tank at 32, so this costs nothing extra.

The node keeps its 26.5 relevance on purpose. It sits under the critical band, so Holy Shock (36),
Divine Favor (35.5) and Holy Light (34) still lead on a dying target; the instant Flash picks up the
global only when those are vetoed — which is exactly the moving case.

Flash of Light is now removed from all four health bands (was 33 / 23.5 / 18 / 13), and the
`holy_light_on_party` ActionNode is deleted outright — its only content was the
`flash of light on party` alternative, which would have put Flash straight back into every band it
was just removed from (mana veto, out of range, moving). Holy Light now simply fails when it cannot
cast, and Holy Shock still leads every band, so the bot keeps an instant available whenever that
cooldown is up.

Two other strategies share `GenericPaladinStrategyActionNodeFactory` and lose the same fallback,
both harmlessly: `OffhealRetPaladinStrategy` already lists `flash of light on party` explicitly at 18
and 13, and `GenericPaladinNonCombatStrategy` has its own Flash nodes and no time pressure out of
combat. The self-target `flash of light` node and the in-bubble Flash @89 are untouched.

## Glyphs — verified, do not re-audit

`AiPlayerbot.PremadeSpecGlyph.2.0` (holy pve) is `41106,43367,45741,43368,43365,41109`, read as
major/minor/major/minor/minor/major: **Glyph of Holy Light**, Lay on Hands, **Glyph of Beacon of
Light**, Sense Undead, Blessing of Kings, **Glyph of Seal of Wisdom**. `InitGlyphs` indexes
`parsedSpecGlyph[cls][tab]` with `AiFactory::GetPlayerSpecTab`, which is 0 for Holy, so this is what
Holy bots get. Glyph of Holy Light (10% splash to five allies within 8 yards) is a third reason Holy
Light has to be the primary heal.

Seals are already right too: `seal of wisdom` @20 is the only seal any Holy node casts, and Seal of
Light is never wired for Holy. One wart — `SealTrigger::IsActive` returns true when Seal of Wisdom is
already up and mana > 70, which queues a node that then resolves `USELESS`. The clause is there for
the DPS and tank ladders, so removing it needs a Holy-specific trigger. Costs one engine iteration,
does not affect seal uptime.

## Confirmed correct — do not re-audit

Holy Radiance does not exist in 3.3.5. `bcast` gives Holy the Concentration Aura. Avenging Wrath does
**not** apply Forbearance in 3.3.5. The out-of-combat ladder in `GenericPaladinNonCombatStrategy` is
ordered correctly. Hand of Freedom's self-preference and "already has a Hand" guard are implemented.

## Reusable pattern established here

**A derived `InitTriggers` cannot remove a base-class node.** The Divine Plea fix is *relocation*:
strip the node from the base strategy and re-add it verbatim in each sibling spec that wants it. The
priest Holy-school relocation reuses this.
