# Paladin

Engine and healer semantics (health bands, `HealerAutoSaveManaMultiplier`) are in
[../engine/action-selection.md](../engine/action-selection.md).

## Current Holy ladder

`HealPaladinStrategy` (key `holy`), on top of `GenericPaladinStrategy`:

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

Inherited from `GenericPaladinStrategy`: `hand of sacrifice on party` 93,
`blessing of protection on party` 92.8 (never casts, see [Traps](#traps)), `lay on hands on party` 92,
`lay on hands` 91, `divine shield` 90, `flash of light` 89 / `holy light` 88 (inside the bubble),
`hand of freedom on party` 24, three Hammer of Justice interrupts at 40.

## Holy decisions

- **Holy Light is the primary heal and leads every band**, including the 65-85% band at 14 where raid
  targets sit most of the time — the WotLK rotation, not a mistake to be "fixed" into Flash spam.
  `CastHolyLightOnPartyAction` is `25.0f, HIGH`: Illumination refunds 30% of the mana on a heal crit
  and Beacon mirrors the heal for free, so it is the cheaper spell per point healed. At `50.0f,
  MEDIUM`, `HealerAutoSaveManaMultiplier` vetoed it below 60% bot mana on most of the raid. The
  `lossAmount < estAmount` arm still keeps it off targets above ~75% HP when the paladin is low on
  mana.
- **Holy Shock leads every band** (36 / 26 / 19.5) to proc Infusion of Light, and `healer dps` carries
  no offensive Holy Shock: spent on damage during lulls, the cooldown was down when damage started.
- **`infusion of light` @26.5 is the only Holy node that casts Flash of Light.** Verified 3.3.5
  behaviour (talent 53569/53576, proc aura 53672/54149): a Holy Shock crit **cuts the next Flash of
  Light's cast time by 0.75/1.5 s** and raises the next **Holy Light**'s crit chance by 10/20%. Holy
  Light gets the crit whatever the ladder does; the cast-time cut is the half that needs a node. At
  2/2 the Flash is instant, so:
  - it is castable while moving: `CanCastSpell` reads `SpellInfo::CalcCastTime(bot)`, which applies
    `SPELLMOD_CASTING_TIME`, so the proc already zeroes the cast it tests. Besides Holy Shock it is the
    only heal a moving Holy paladin lands;
  - on a Sacred Shielded target it also drops a 12 s HoT (66922, `spell_pal_infusion_of_light::HandleProc`).

  It fires only while the heal target is below `almostFull` (85%). 26.5 sits under the critical band on
  purpose: Holy Shock, Divine Favor and Holy Light lead on a dying target, and the instant Flash takes
  the global only when they are vetoed — exactly the moving case.
- **No Flash of Light in the health bands, and no `holy_light_on_party` ActionNode.** Its only content
  was a `flash of light on party` alternative, which would put Flash back into every band (mana veto,
  out of range, moving); Holy Light simply fails when it cannot cast, and Holy Shock keeps an instant up.
  The two other users of `GenericPaladinStrategyActionNodeFactory` lose that fallback harmlessly:
  `OffhealRetPaladinStrategy` lists `flash of light on party` at 18 and 13 itself, and
  `GenericPaladinNonCombatStrategy` has its own Flash nodes. The self-target `flash of light` node and
  the in-bubble Flash @89 keep theirs.
- **Divine Plea is not on `high mana`** (`mana < 65`): unglyphed it halves healing done for 15 s, and
  at 20 it outranked the medium and almost-full bands. Holy casts it on `paladin divine plea` @12 below
  `mediumMana` (40) while no one is in the medium AoE-heal band, or on `low mana` @23; below `lowMana`
  (15) the raid-damage suppression lets it through, since an unglyphed healer with an empty bar heals
  nothing. `CastDivinePleaAction::isUseful` passes a glyphed or non-healer paladin unconditionally. The
  node moved out of the base strategy by [relocation](../engine/action-selection.md#healing).
- **Beacon of Light and Sacred Shield follow `tank to beacon`**, the lowest-health tank in combat,
  range and LOS, not only the main tank: in a two-tank raid the off-tank got neither, and Beacon
  multiplies everything the paladin casts. **Without hysteresis Beacon thrashes every GCD**:
  `SWAP_MARGIN_PCT` is 75 percentage points, so in practice Beacon sticks to the first tank it picks
  until that tank leaves combat, range or LOS.
- **`reach party member to heal` sits at 39.5**: at 93 an out-of-range target made the paladin run
  instead of using Lay on Hands (92) on a different dying player. Holy priest and Resto shaman use 40.
- **Holy has no default actions**: they are pushed every tick ungated, bypassing the
  `HealerShouldAttackTrigger` mana floor that gates `healer dps`. `healer dps` is hammer of wrath 5.6 /
  judgement of light 5.3 / exorcism 5.1: Shield of Righteousness needs a shield (Protection only) and
  Consecration is a ~660-mana self-centred AoE at melee range.
- **Judgement of Light is maintained at 27** (`paladin judgement of light`), because `healer dps` is
  gated off whenever anyone is below 85% HP, i.e. most of a fight. Judgements of the Pure (15% haste,
  60 s) plus the melee-swing raid heal are worth one global per 20 s. The trigger is a `DebuffTrigger`
  with `checkIsOwner = true` (a second paladin's Judgement of Light must not stop this one refreshing
  its own Judgements of the Pure) and calls `CanCastSpell`, so at range the node goes quiet instead of
  failing every tick. Judgement is 10 yards and the node never queues movement — it waits.
- **Divine Illumination is pinned to Avenging Wrath.** `CastDivineIlluminationAction::isUseful`
  requires the `avenging wrath` aura for a healer, so the `medium group heal setting` pair (31, 30.5)
  lands in that order on consecutive ticks and the −50% cost window covers the +20% healing window.
  Both are 3-minute cooldowns, so they stay aligned.

## Cooldowns

| Cooldown | State |
|---|---|
| **Aura Mastery** | `medium group heal setting` @30 |
| **Hand of Sacrifice** | `protect party member` @93, on the tank list since it redirects damage onto the paladin; ahead of BoP, which wipes threat and applies Forbearance. Refused below 50% own health |
| **Hand of Salvation** (1038) | **Open**: exists only as an exclusion constant in `PaladinHelper.h` |
| **Divine Sacrifice** | `medium group heal setting` (Holy 35, Protection 25); refused below 50% own health, since it redirects raid damage onto the paladin |
| **Lay on Hands** | Forbearance guards on both variants |
| **Divine Favor** | `party member critical health` @35.5. `PaladinBoostStrategy::InitTriggers` is empty |
| **Divine Protection** | **Open**: no direct trigger; reachable only as the `/*A*/` fallback of `divine shield` |
| **Avenging Wrath** | On the burst registry, with a healer exemption ([consumables](../systems/consumables-and-burst.md#the-burst-gate)) |

## Traps

- **`blessing of protection` never casts.** The spell was renamed Hand of Protection in 3.0 (player
  ranks 1022 / 5599 / 10278; the only "Blessing of Protection" row is 41450, not in a player's
  spellbook), and `SpellIdValue` matches the bot's spellbook by full name, so
  `CastBlessingOfProtectionProtectAction` (`blessing of protection on party` @92.8) resolves spell id 0
  ([why](../engine/pitfalls.md#names-fail-silently-at-runtime)). It aims at `party member to protect
  no tank`, since BoP wipes a tank's melee threat and applies Forbearance. Left alone: reviving it changes
  paladin behaviour in every instance. Raid code that needs the spell casts `hand of protection`
  directly (Yogg-Saron, Anub'arak).
- Hand of Protection's target, `PartyMemberToProtect`, only sees a current melee victim
  ([pitfalls](../engine/pitfalls.md#role-and-index-traps)).
- **Read the main tank by its registration key `"main tank"`**: `"main tank member"` is the value's
  own name (`PartyMemberMainTankValue`) and resolves nothing.
- **`OffhealRetPaladinStrategy` still pushes `reach party member to heal` at 93**, above Lay on Hands
  on party (92) — the shape Holy fixed at 39.5.

## Glyphs — verified, do not re-audit

`Playerbots.PremadeSpecGlyph.2.0` (holy pve) is `41106,43367,45741,43368,43365,41109`, read as
major/minor/major/minor/minor/major: **Glyph of Holy Light**, Lay on Hands, **Glyph of Beacon of
Light**, Sense Undead, Blessing of Kings, **Glyph of Seal of Wisdom**. `InitGlyphs` indexes
`ParsedSpecGlyph[cls][tab]` with `AiFactory::GetPlayerSpecTab`, which is 0 for Holy, so this is what
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

**Beacon of Light is single-rank 53563.** The ids **53652 / 53653 / 53654** that show up in Skada are
the server-side beacon *copy heals* for Holy Light, Flash of Light and Holy Shock respectively
(`spell_pal_light_s_beacon::HandleProc` in the core's `spell_paladin.cpp`). Three entries in the meter
means the beacon is working, not that something is casting three spells.

## Protection — Holy Shield

`holy shield` sits on its own `HolyShieldTrigger` at `ACTION_HIGH + 10` (30): above the threat/AoE/mana
cluster (seal of wisdom 29 … avenger's shield 25), below interrupts (40). The warrior's `shield block`
sits at 41, but that is a 60 s cooldown — Holy Shield returns every 8 s and would cost interrupts at that
priority. On `medium health` (65%) a healthy tank never cast it: one Freya kill had it up on 8% of
swings (18.7% block against an 18.5% no-Holy-Shield baseline) against a player paladin's 75%.

**`beforeDuration` 0 means "only once the buff has already dropped".** `BuffTrigger::IsActive` and
`CastBuffSpellAction::isUseful` both route through `ai::buff::BuffBelowRefreshTarget`, whose in-combat
path is `baseBeforeDuration && remaining < baseBeforeDuration`. **Set it on both** — the action vetoes
any early refresh the trigger proposes. Holy Shield uses 2000 on trigger and action.

**Holy Shield 48952**: +30% block, 10 s, 8 charges, 8 s cooldown (category 931), 274 holy damage per
block. The damage is one event per block, so `SPELL_DAMAGE` 48952 in a Chronicle log counts blocks
directly.

**Skada cannot measure block.** Only a *full* block (`amount=0`) is a distinct event; a partial block
is a normal hit carrying a `blocked` amount — `MELEE_HIT_BLOCK` keeps `VICTIMSTATE_HIT` in
`Unit::CalculateMeleeDamage`. Count Chronicle rows where `blocked > 0`.

## Retribution: Consecration leads the default ladder

`consecration` sits at `ACTION_DEFAULT + 0.7`, above Hammer of Wrath. The server's Swift Retribution tweak
(mod-spell-tweaks) gives Consecration ticks base damage, and the sim measures it best first. Not a
mana or priority mistake, do not re-audit.
