# Priest — Discipline, Holy, Shadow

Audit against the wowtbc.gg WotLK guides. Numbered findings marked *Fixed* record a decision; the
rest are still live. Engine and healer semantics are in
[../engine/action-selection.md](../engine/action-selection.md).

## Settled decisions — do not re-litigate

- **Discipline gets no Greater Heal.** Too slow for the Disc rotation and the guide omits it.
- **Discipline gets Flash Heal in exactly one case**: the target is below `lowHealth`, carries
  Weakened Soul and Penance is on cooldown (see [Discipline](#discipline)). Everywhere else the spec
  runs on PW:S, Penance, Prayer of Mending and tank Renew, and an idle global is preferred over a Flash
  Heal.
- **Shadow keeps only `critical health` → self Power Word: Shield.** Desperate Prayer and Hymn of
  Hope are not on the Shadow ladder — neither is castable in Shadowform, and dropping form costs
  more DPS than the self-heal is worth. Shadow relies on the raid's healers.
- **No retuning of `HealerAutoSaveManaMultiplier` itself** for the priest: it would move every healer
  class.

## Strategy sets

Keys since PR #2836: Disc `disc` (old `heal`), Holy `holy heal` (alias `holy`), Shadow `shadow` (old
`dps`) + `shadow debuff` + `shadow aoe`. Every priest also gets `dps assist` + `cure` and the
always-on set (`boost`, `burst`, …); healers add `save mana` + `healer dps`. **`holy dps`
(`HolyDpsPriestStrategy`) plus `shadow debuff` / `shadow aoe` are added only to an ungrouped
non-Shadow selfbot or random bot**, so it is irrelevant to raids. Shadow does not get `save mana` or
`healer dps` — `PlayerbotAI::IsHeal` is false for it.

Inherited nodes the ladders must not collide with: `CombatStrategy` — `drop target` 99,
`check mount state` 54, `set facing` 37, `reach spell` 20, `reset` 1; `RangedCombatStrategy` — `flee`
34; `GenericPriestStrategy` — `set pet stance` 60, `fade` 55, `flee` 39, `fear ward on main tank`
20.7, `critical health` → self `power word: shield` 10, `apply oil` 1; `PriestCureStrategy` —
`dispel magic` 41, `dispel magic on party` 40, `abolish disease` 31, `abolish disease on party` 30.
`PriestBoostStrategy` is empty: `AiFactory` hands `boost` to every bot, so the class must exist.

## Cross-spec

- **Shadowfiend** is a `SpellNoCooldownTrigger` (known and off cooldown, no mana gate): Disc and Holy
  23, Shadow 21. `CastShadowfiendAction::GetTarget` falls back to the `grind target` when a healer has
  nothing selected. Its exemption from the per-boss phase holds is in
  [consumables](../systems/consumables-and-burst.md#the-burst-gate) and must survive any rebinding.
- **Power Infusion** is Discipline only, self-cast on `medium group heal setting` @36.4, and passes
  the burst gate for a healer. `power infusion on party` is registered and referenced by zero nodes.
- **Inner Focus**: Disc and Holy @33.7 on `medium group heal setting`, ahead of Prayer of Healing;
  Shadow @19 on `inner focus` (off cooldown and the aura missing).
- **Hymn of Hope**: Disc and Holy @22, below `mediumMana` (40) while no one is in the critical AoE-heal
  band — an 8 s channel must not start while somebody is about to die.
- **No offensive dispel node**, so a priest never purges an enemy buff, and never casts Mass Dispel
  on its own.
- **Dead registrations** (registered, zero nodes): `symbol of hope`, `lightwell`, `holy nova`,
  `mass dispel`, `levitate`, `mind soothe`, `consume magic`, `elune's grace`, `power infusion on party`.

## Discipline

Guide priority: PW:S → Renew on tank → Penance → Prayer of Mending → Flash Heal **when the target has
Weakened Soul** → Binding Heal when the priest is also hurt → Prayer of Healing with Borrowed Time.

`DiscPriestStrategy` (defaults: `shoot` 5.0):

| Rel | Trigger | Action |
|---|---|---|
| 91 / 90 | critical health / protect party member | pain suppression / on party |
| 38 | party member to heal out of spell range | reach party member to heal |
| 36.8 / 36.4 / 33.7 / 33.5 / 33 | medium group heal setting | divine hymn / power infusion / inner focus / prayer of healing on party / power word: shield on not full |
| 36 / 35.5 / 35 | party member critical health | power word: shield on party / penance on party / prayer of mending on party |
| 34 | binding heal | binding heal |
| 32.5 / 32 | group heal setting | prayer of mending on party / power word: shield on not full |
| 28 / 27.5 | critical health / low health | desperate prayer / power word: shield |
| 27.4 / 27.35 | weakened soul on party member / flash heal on party member | penance on party / flash heal on party |
| 26 / 25.5 / 25 | party member low health | power word: shield on party / penance on party / prayer of mending on party |
| 24.5 / 24 | power word: shield on main tank / renew on main tank | same |
| 23.5 / 23 / 22 | being attacked / shadowfiend / hymn of hope | power word: shield / shadowfiend / hymn of hope |
| 19 / 18.5 / 18 | party member medium health | power word: shield on party / penance on party / prayer of mending on party |
| 13 / 12 / 11 | party member almost full health | power word: shield on party / prayer of mending on party / renew on party |

- **Flash Heal only through `flash heal on party member`** @27.35: the heal target is below
  `lowHealth` (45), has Weakened Soul and Penance is on cooldown (no Penance id also counts, so an
  untalented priest still gets the heal). Just under `weakened soul on party member` → Penance @27.4
  (target below `almostFull` with Weakened Soul), so Penance wins whenever it is up. No band carries
  `flash heal on party` and the `penance_on_party` ActionNode has no alternative, so Flash Heal cannot
  creep back into the general ladder; with PW:S, Penance and Prayer of Mending all on cooldown the bot
  idles the global. **That is the intended trade, not a bug** — do not add a filler heal.
- **Known back door, accepted**: `flash heal on party`'s own ActionNode keeps `greater heal on party`
  as its `/*A*/` alternative, shared with Holy through `GenericPriestStrategyActionNodeFactory`, so a
  Disc bot whose Flash Heal is unavailable can fall through to Greater Heal. Emptying it would strip
  Holy's fallback too; in practice whatever blocks Flash Heal (mana, range) blocks Greater Heal as well.
- **The party shields substitute their target**: `CastPowerWordShieldOnPartyAction::GetTarget` skips a
  target with Weakened Soul or a shield and takes `FindShieldTarget` instead. The shielded raider stays
  the one `party member to heal` returns ([why](../engine/action-selection.md#healing)), so with a guard
  instead all four PW:S bands went out at once, roughly one shield per 15 s. One action name, so the
  retarget repaired every band.
- **The Weakened Soul veto on the *self* shield is correct and stays — do not re-audit.** A self-cast
  has no alternative target to substitute.
- **Main-tank shield** @24.5: below the low-band party shield (26), above `renew on main tank` (24), so
  a hurt raider still outranks topping up the tank's bubble. It leaves `checkIsOwner` at `false`,
  unlike Renew's `true`: **Weakened Soul is shared**, so any priest's shield stops this one, and
  `PowerWordShieldOnMainTankTrigger` tests Weakened Soul or it would rearm every tick for 15 s.
  **Trap:** it derives from `CastBuffSpellAction`, not `CastHealingSpellAction`, so
  `HealerAutoSaveManaMultiplier` never gates it on low mana. Intended for a tank bubble; worth watching
  in a long fight.
- **Penance sits 0.5 under PW:S in every band** (the guide's order), with `15.0f, HIGH` metadata like
  Flash Heal. Ruled out while chasing its suppression, do not re-investigate: Penance **is** in the
  Disc premade build (the first tree of `Playerbots.PremadeSpecLink.5.0.80` ends in `1`),
  `spell_pri_penance::CheckCast` passes on friendly targets, `SpellIdValue` resolves the name, and
  `CastTimeMultiplier` only touches actions aimed at `current target`, never party heals.
- **D5. Prayer of Healing has no Borrowed Time pairing** — it appears only on
  `medium group heal setting` @33.5.
- **D6. Pain Suppression is correctly wired — do not change it.** Self @91 on `critical health`,
  party @90 on `protect party member`. `PartyMemberToProtect` requires damage to actually be incoming
  and uses calibrated split thresholds (below 30% HP, or below 10% if the victim is a tank), where
  the critical-health trigger is a flat 25% with no incoming-damage requirement. `pVictim == bot` is
  excluded, which is why the self-cast lives on its own node. The priest deliberately stays on the
  generic `party member to protect`, not `party member to protect no tank`: Pain Suppression on a dying
  tank is the intended use. The value only sees a current melee victim
  ([pitfalls](../engine/pitfalls.md#role-and-index-traps)).

## Holy

`HolyHealPriestStrategy` (defaults: `shoot` 5.0):

| Rel | Trigger | Action |
|---|---|---|
| 38 | party member to heal out of spell range | reach party member to heal |
| 36.8 / 33.7 / 33.5 / 33 | medium group heal setting | divine hymn / inner focus / prayer of healing on party / circle of healing on party |
| 36.2 / 36 / 35.5 / 35 / 34.5 / 34.2 | party member critical health | guardian spirit / power word: shield / circle of healing / prayer of mending / flash heal / greater heal (all on party) |
| 34 | binding heal | binding heal |
| 32.5 / 32 | group heal setting | prayer of mending on party / circle of healing on party |
| 28 / 27.5 | critical health / low health | desperate prayer / power word: shield |
| 26 / 25.5 / 25 / 24.5 | party member low health | circle of healing / prayer of mending / flash heal / greater heal (on party) |
| 24 | renew on main tank | renew on main tank |
| 23.5 / 23 / 22 | being attacked / shadowfiend / hymn of hope | power word: shield / shadowfiend / hymn of hope |
| 19 / 18.5 / 18 / 17.5 | party member medium health | circle of healing / prayer of mending / greater heal / renew (on party) |
| 13 / 12 | party member almost full health | prayer of mending on party / renew on party |

Binding Heal (both healing specs) fires when a party member is below `lowHealth` **and** the bot is
below `mediumHealth`, the guide's condition; Holy needs it for Serendipity stacking.

- **H5. `reach party member to heal` @38 outranks Divine Hymn @36.8 and Guardian Spirit @36.2.** The
  paladin settled the equivalent node at 39.5.
- *Fixed* — H6: `holy dps` used to inherit the Discipline ladder; `HolyDpsPriestStrategy` now derives
  from `GenericPriestStrategy` (PR #2836).

## Shadow

Guide priority: Vampiric Embrace → maintain Vampiric Touch → Devouring Plague → Shadow Word: Pain
(Mind Flay refreshes it) → Mind Blast → SW:D **only if the target dies before a Mind Blast or Mind
Flay finishes** → Mind Flay, clipped after the second tick.

`ShadowPriestStrategy` + `shadow debuff` + `shadow aoe`: silence 42 / on enemy healer 41.5, dispersion
25.5 on `critical health` and 25 on `low mana`, cancel channel 24.5 on `mind sear channel check`,
shadowform 24.2, mind sear 24 on `medium aoe`, vampiric touch 23, devouring plague 22.5, shadow word:
pain 22, vampiric embrace 21.5, shadowfiend 21, inner focus 19, SW:P on attacker 15, VT on attacker
14, shadow word: death 12 on `shadow word: death execute`; defaults mind blast 5.3 / mind flay 5.2 /
shoot 5.0.

- *Fixed* — S1: **Shadow Word: Death is an execute, not a filler**: `shadow word: death execute` fires
  when the target dies within 2 s at `estimated group dps` and the bot is above `mediumHealth`, since
  SW:D backlashes whenever the target survives it.
- *Fixed* — S2: **Vampiric Embrace is maintained in combat** @21.5, not only out of combat.
- **S3. DoT refresh: only VT gets a window.** DP and SW:P keep `beforeDuration` 0 (the wowsims APL
  also lets DP expire; Mind Flay keeps SW:P up). VT recasts at `1500 ms × UNIT_MOD_CAST_SPEED` left
  (`VampiricTouchTrigger::RefreshWindow`, shared with the action), the sim's rule: a recast resets the
  tick timer ([why](../engine/action-selection.md#trigger-semantics)), so landing early drops the tick
  in progress. Waiting for expiry gave VT 88% uptime against the sim's 95%. Do not copy this window, or
  rogue's 2000, onto DP or SW:P.
- **SW:P waits for 5 Shadow Weaving stacks** (buff 15258; skipped without the talent). A DoT
  snapshots pct mods and crit chance at apply (`CalculatePeriodicData`), and Pain and Suffering's
  refresh recalculates only the base amount, so the first SW:P's snapshot lasts the fight. Cast before
  Shadowform it ran 97 ticks with no crit.
- **S6. `shadowform` @24.2**, above the DoTs and Mind Sear, for the same snapshot: at 20.5 the bot
  pulled with DP, VT and SW:P before Shadowform. **Open gap:** nothing casts it out of combat, and the
  out-of-combat heals and resurrection drop it (`remove shadowform`), so such a pull starts with a
  Shadowform GCD. An out-of-combat node would ping-pong with `party member dead` → `remove shadowform`
  @41 whenever the resurrection can't be cast.
- **S7. Mind Flay is not clipped — do not re-add a clip.** Cancel and recast take two AI updates (next
  cast ~0.45 s after the 2nd tick, ~0.2 s before the 3rd), so clipping traded a tick for ~0.4 s. Only a
  same-update cancel-and-cast could pay.
- **Mind Blast above the sim's count is the sim's choice**: its APL skips Mind Blast when the hasted
  cast is under 1 s; the bot casts it on cooldown.
- *Fixed* — S8: **Shadow inherits no Holy-school node.** Desperate Prayer, Hymn of Hope and the
  `low health` / `being attacked` self shields moved from `GenericPriestStrategy` into the two healing
  specs by [relocation](../engine/action-selection.md#healing): in Shadow they burned an action slot on
  a guaranteed failure, or cost a GCD leaving Shadowform, one shielding and one re-entering.

### Shadow's mana kit

Shadow carries **no mana potions**
([../systems/consumables-and-burst.md](../systems/consumables-and-burst.md)), so the kit is
Shadowfiend @21 on cooldown (5 min, ungated by mana), Replenishment off Vampiric Touch @23 plus Mind
Blast crits, and Dispersion @25 on `low mana` as the floor — 36% over a 6 s channel, 2 min cooldown.
Nothing else is needed and nothing else is wanted:

- **Do not add Hymn of Hope** — settled above, and it is not castable in Shadowform.
- Dispersion's 15% threshold is late on purpose. It is 6 s of zero damage, so it is a floor, not a
  mana plan.

## Confirmed correct — do not re-audit

- DP and SW:P expiry (S3) — `beforeDuration = 0` is deliberate.
- Circle of Healing absent from Disc, Pain Suppression absent from Holy and Shadow, Penance absent
  from Holy — talent placement.
- `mind sear channel check` cancelling when attackers drop below 2.
- The shared out-of-combat ladder in `PriestNonCombatStrategy`.

## Documented limitations, deliberately out of scope

- **Surge of Light and Serendipity are not modelled** — both are Holy procs the guide leans on, and
  there is no aura-tracking trigger for either. Adding them means a new `HasAuraTrigger` subclass per
  proc plus band-specific nodes to spend them.
- **Borrowed Time is not modelled** for Disc, for the same reason.

## Needs in-game confirmation

Spell school and Shadowform castability live in DBC, not this repo. Desperate Prayer and Hymn of Hope
were settled by the user (both blocked in Shadowform).
