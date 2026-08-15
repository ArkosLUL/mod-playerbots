# Priest — Discipline, Holy, Shadow

Audit against the wowtbc.gg WotLK guides, implemented in commit `2ba4b0dba` ("Priest strategy
improvements"). The findings below are the audit record — the relevance figures describe the state
the audit found, so re-check `src/` before acting on any single one. Engine and healer semantics are
in [../engine/action-selection.md](../engine/action-selection.md).

## Settled decisions — do not re-litigate

- **Discipline gets no Greater Heal.** Too slow for the Disc rotation and the guide omits it.
- **Discipline gets Flash Heal in exactly one case**: the target is below `lowHealth`, carries
  Weakened Soul and Penance is on cooldown. See "Fourth pass" below. Everywhere else the spec runs on PW:S,
  Penance, Prayer of Mending and tank Renew, and an idle global is preferred over a Flash Heal.
- **Shadow keeps only `critical health` → self Power Word: Shield.** Desperate Prayer and Hymn of
  Hope leave the Shadow ladder entirely — neither is castable in Shadowform, and dropping form costs
  more DPS than the self-heal is worth. Shadow relies on the raid's healers.
- **Blast radius**: the priest folder plus narrow additive shared changes. No retuning of
  `HealerAutoSaveManaMultiplier` itself, which would move every healer class.

## Strategy sets

Every priest gets `boost` + `burst` + `dps assist` + `cure`; Disc adds `heal`, Holy `holy heal`,
Shadow `dps` + `shadow debuff` + `shadow aoe`; healers add `save mana` + `healer dps`. **`holy dps`
is added only when ungrouped**, so `HolyPriestStrategy` is irrelevant to raids.

Inherited nodes the relevance tables must not collide with: `CombatStrategy` — `drop target` 99,
`check mount state` 54, `set facing` 37, `reach spell` 20, `reset` 1; `GenericPriestStrategy` —
`set pet stance` 60, `fade` 55, `flee` 39, `apply oil` 1; `PriestCureStrategy` — `dispel magic` 41,
`dispel magic on party` 40, `abolish disease` 31, `abolish disease on party` 30;
`PriestBoostStrategy` — `power infusion` 41.

## Cross-spec findings

- **`TriggerNode("boost", …)` is dead.** Shadowfiend is bound at 20 to a trigger named `"boost"`. No
  trigger by that name is registered anywhere — `creators["boost"]` is a **strategy** factory.
- **Power Infusion is unreachable in a raid.** `PowerInfusionTrigger` is a `BOOST_TRIGGER`, so in PvE
  it needs `balance <= 50`. Its only node ties with `dispel magic` @41, moot only because the node
  never activates. `power infusion on party` is registered and referenced by zero nodes.
- **The registered `shadowfiend` trigger is also boost-gated**, so there is currently **no working
  "Shadowfiend is off cooldown" trigger for any spec to use**.
- **Inner Focus is bound to `medium mana` @21** — a 3-minute free-cast spent on whatever the engine
  picks below 40% mana. Its purpose-built `InnerFocusTrigger` is dead and would not help as written:
  `BUFF_TRIGGER` resolves to `BuffTrigger`, whose `IsActive` is simply "the aura is missing", with no
  cooldown check.
- **Hymn of Hope fires at < 15% mana** — an 8-second channel on a 6-minute cooldown, started when the
  priest is already nearly out of mana, stopping all healing at the worst possible moment.
- **Shadowfiend fires only below 40% mana**, losing uses over a long fight. It is already burst-listed
  and **explicitly exempt from the boss-only hold** (`name == "shadowfiend"`), so rebinding it to a
  plain cooldown trigger is safe. Separately, `CastShadowfiendAction::GetTargetName()` returns
  `"current target"`, so a healer with no current target cannot cast it at all.
- **Dead registrations** (registered, zero trigger nodes): `symbol of hope`, `binding heal`,
  `lightwell`, `holy nova`, `mass dispel`, `levitate`, `mind soothe`, `consume magic`, `elune's
  grace`, `power infusion on party`.

## Discipline

Guide priority: PW:S → Renew on tank → Penance → Prayer of Mending → Flash Heal **when the target has
Weakened Soul** → Binding Heal when the priest is also hurt → Prayer of Healing with Borrowed Time.
The Flash Heal step is implemented narrowly — see "Fourth pass" below.

- **D1. Flash Heal is offline under mana pressure.** `15.0f, LOW` — LOW is vetoed for *any* target
  above 45% HP below 60% bot mana. Its `/*A*/` alternative is `greater heal on party` at
  `50.0f, MEDIUM`, itself vetoed above 65% HP always, and not a Disc spell anyway. Flash Heal is
  Disc's only direct heal in the ladder. **Superseded by the third pass — Flash Heal is gone from
  Disc entirely, so this finding no longer applies.**
- **D2. No Weakened Soul guard** on `power word: shield on party` or the self shield — only the two
  custom variants check it. PW:S on party leads Disc's critical band @35, so a Weakened-Soul target
  makes the bot re-attempt a guaranteed failure every tick. **Superseded by the fifth pass — the
  party shields now substitute a target rather than guarding, so this reads backwards today.**
- **D3. No Renew maintenance on the tank.** `renew on party` appears once, in the almost-full band
  @11. `BuffOnMainTankTrigger` / `BuffOnMainTankAction` already exist — reuse, do not write new
  plumbing. **Superseded — `renew on main tank` @24 and the main-tank shield @24.5 both ship.**
- **D4. Binding Heal is dead code.** Trigger and action are both registered and used by nothing, and
  the trigger already encodes the guide's condition exactly (a party member below `lowHealth` **and**
  the bot below `mediumHealth`).
- **D5. Prayer of Healing has no Borrowed Time pairing** — it appears only on
  `medium group heal setting` @34, tying with `penance on party`.
- **D6. Pain Suppression is correctly wired — do not change it.** Self @91 on `critical health`,
  party @90 on `protect party member`. `PartyMemberToProtect` requires damage to actually be incoming
  and uses calibrated split thresholds (below 30% HP, or below 10% if the victim is a tank), where
  the critical-health trigger is a flat 25% with no incoming-damage requirement. `pVictim == bot` is
  excluded, which is why the self-cast lives on its own node. The priest deliberately stays on the
  generic `party member to protect` — see [shaman.md](shaman.md).
- **D7. Penance is priced below PW:S in every band** (34 vs 35 critical, 22 vs 24 low, 16 vs 19
  medium), and its `25.0f, HIGH` metadata vetoes it above 75% target HP under mana pressure. It is
  the guide's #4 and the spec's signature cooldown-driven heal.

### Second pass — Penance was still being suppressed

D2's fix (the `weakened soul on party member` node) papered over D7. Penance leads Flash Heal inside
every band (35.5/34.5 critical, 25.5/24.5 low, 18.5/17.5 medium), but the Weakened Soul node sat
*outside* those bands at 27 and outranked both of the lower two. PW:S is on top of every band plus
`power word: shield on not full` @32–33, so the bot shields constantly and Weakened Soul (15s) is up
on the heal target nearly all the time — Flash Heal @27 therefore won every non-critical heal and
Penance only ever fired below 25% HP.

`penance on party` now leads that node at 27.4 (27.5 is taken by `low health` → self PW:S), so a
shielded target gets Penance when it is off cooldown. `penance on party` also gained the ActionNode
it was the only party heal to lack, carrying the `remove shadowform` prerequisite.

Ruled out while chasing this, do not re-investigate: Penance **is** in the Disc premade build
(`PremadeSpecLink.5.0.80` ends in `1`), `spell_pri_penance::CheckCast` passes on friendly targets,
`SpellIdValue` resolves the name, `HealerAutoSaveManaMultiplier` treats Penance and Flash Heal
identically (both `15.0f, HIGH`), and `CastTimeMultiplier` only touches actions aimed at
`current target`, never party heals.

### Third pass — Flash Heal removed from Discipline

The second pass left Flash Heal as the fallback under Penance: in every band, and as the
`penance on party` ActionNode's alternative so a cooldown-blocked Penance fell through to it in the
same tick. Flash Heal therefore still took most of the globals Penance and Prayer of Mending did not.

Flash Heal is now gone from Discipline outright — all four `flash heal on party` entries deleted
(critical, weakened soul, low, medium bands) and the `penance_on_party` alternative emptied. Nothing
replaces it: Greater Heal stays off Disc per the settled decision above, so when PW:S, Penance and
Prayer of Mending are all on cooldown the bot idles the global. **That is the intended trade, not a
bug** — do not "fix" it by adding a filler heal.

Holy is untouched: it runs `HolyHealPriestStrategy`, a sibling of `GenericPriestStrategy` rather
than a subclass of `HealPriestStrategy`, and still lists Flash Heal at 34.5 and 25. The
`flash heal` / `flash heal on party` actions and ActionNodes stay registered for it.

### Fourth pass — Flash Heal back, on one condition only

The third pass left Disc with no direct heal at all. Flash Heal returns through its own trigger node,
`flash heal on party member` (`FlashHealOnPartyMemberTrigger`, `src/Ai/Class/Priest/PriestTriggers.cpp`),
which fires only when the heal target is **below `lowHealth`** (45 by default), **has Weakened Soul** and **Penance is on
cooldown** (`bot->HasSpellCooldown` on the resolved Penance id; no id at all also counts as
unavailable, so an untalented priest still gets the heal). Priority `ACTION_MEDIUM_HEAL + 7.35f` = 27.35,
just under `penance on party` @27.4 on the Weakened Soul node so Penance always wins when it is up.

The three deleted in-band `flash heal on party` entries stay deleted, and the `penance_on_party`
ActionNode alternative stays empty — the point is that Flash Heal cannot creep back into the general
ladder.

Known back door, accepted: `flash heal on party`'s own ActionNode keeps `greater heal on party` as its
`/*A*/` alternative, and that node is shared with Holy through `GenericPriestStrategyActionNodeFactory`.
So a Disc bot whose Flash Heal is unavailable can fall through to Greater Heal, against the settled
decision above. Emptying it would strip Holy's fallback too, so it is left alone; in practice whatever
blocks Flash Heal (mana, range) blocks the slower, pricier Greater Heal as well.

Current collisions: `dispel magic` = `power infusion` @41; `dispel magic on party` =
`reach party member to heal` @40; `power word: shield on party` = `power word: shield on not full`
@35; `penance on party` = `prayer of healing on party` @34; `penance` = `shadowfiend` @22;
`power word: shield` = `inner focus` @21; a three-way at 20 (`hymn of hope`, `power word: shield`,
`reach spell`).

### Fifth pass — Power Word: Shield was effectively offline

**A shield absorbs damage but does not raise health %.** So the raider the priest just shielded stays
the lowest-health one and keeps owning `party member to heal` for the whole 15s of Weakened Soul,
while the shield action attached to that target can only fail. This took **all four** PW:S bands out
at once — a raid full of eligible unshielded raiders got roughly one shield per 15s.

The fix is target substitution, not a guard: the party shields resolve their own target, skipping
anyone with Weakened Soul. Because
[../engine/action-selection.md](../engine/action-selection.md) de-dupes baskets by action **name**,
retargeting the one action repaired every band it appears in.

- **The Weakened Soul veto on the *self* shield is correct and stays — do not re-audit.** A self-cast
  has no alternative target to substitute.
- `CastPowerWordShieldOnMainTankAction` leaves `checkIsOwner` at its `false` default, unlike
  `renew on main tank` which passes `true`: **Weakened Soul is shared**, so any priest's shield
  should stop this one.
- **Trap:** that main-tank shield derives from `CastBuffSpellAction`, not `CastHealingSpellAction`,
  so `HealerAutoSaveManaMultiplier` never gates it on low mana. Intended for a tank bubble; worth
  watching in a long fight.
- It sits at **24.5** — below the `party member low health` shield @26 and above `renew on main tank`
  @24, so a hurt raider still outranks topping up the tank's bubble.

## Holy

- **H1. Greater Heal is priced out of its own bands.** `ACTION_MEDIUM_HEAL + 2` = 22 inside the
  **critical** node (siblings 36/35/33/31) so it never fires there, and `+5` = 25 inside the
  **medium** node where it outranks the entire low band. Net raid effect: a 2.5s Greater Heal on a 60%
  target instead of Circle of Healing on a 40% one.
- **H2. Renew is neither maintained on the tank nor used as the filler** — one node, almost-full @12.
- **H3. Circle of Healing is absent from the critical band** despite being the spec's AoE answer.
- **H4. Binding Heal is dead** — same as D4, and Holy actually needs it for Serendipity stacking.
- **H5. `reach party member to heal` @40 outranks Divine Hymn @37 and Guardian Spirit @36.** The
  paladin settled the equivalent node at 39.5.
- **H6. Structural, deferred.** `HolyPriestStrategy : HealPriestStrategy` makes the off-spec
  `holy dps` layer inherit the **Discipline** ladder, including Penance nodes a Holy priest has no
  talent for — and now also the conditional Flash Heal node. Raid impact is nil (ungrouped only) — flag, do
  not fix.

Collisions include `set facing` @37 tying with `divine hymn`, and `greater heal on party` =
`desperate prayer` @25.

## Shadow

Guide priority: Vampiric Embrace → maintain Vampiric Touch → Devouring Plague → Shadow Word: Pain
(Mind Flay refreshes it) → Mind Blast → SW:D **only if the target dies before a Mind Blast or Mind
Flay finishes** → Mind Flay, clipped after the second tick.

- **S1. Shadow Word: Death is wired as a filler, not an execute** — in `getDefaultActions()` @5.1,
  ungated, with a comment claiming it is for movement. On a boss the guide's condition is never met,
  so the bot takes backlash every cooldown for nothing, with no self-health floor anywhere. The
  time-to-die idiom already exists in `DebuffTrigger::IsActive` and
  `TargetWithComboPointsLowerHealTrigger::IsActive`.
- **S2. Vampiric Embrace, the guide's #1, exists only out of combat** — if it falls off mid-fight it
  is never reapplied. Trigger and action are both already registered; pure wiring.
- **S3. Do NOT add DoT refresh windows.** VT, DP and SW:P are declared through
  `DEBUFF_CHECKISOWNER_TRIGGER`, which leaves `beforeDuration` at 0. The guide is explicit — "let it
  expire before you re-apply it". **This is correct as written.** Guardrail: rogue sets 2000 on Slice
  and Dice / Hunger for Blood / Rupture *on purpose*, which is right for rogue. Do not "make priest
  consistent" — the two specs want opposite behaviour from the same parameter, and copying the rogue
  value here introduces a Shadow DPS regression that does not exist today.
- **S5. Dispersion has two nodes at identical relevance 25**, tying with `desperate prayer` @25 and
  `cancel channel` @25 — a four-way resolved by insertion order.
- **S6. `shadowform` @20** collides with `hymn of hope`, self `power word: shield` and `reach spell`.
- **S7. No Mind Flay clipping.** The precedent exists for Mind Sear (`MindSearChannelCheckTrigger` +
  `cancel channel`); nothing equivalent exists for Mind Flay.
- **S8. Shadow drops Shadowform for inherited Holy-school spells.** `ShadowPriestStrategy` derives
  from `GenericPriestStrategy`, so the bot carries both factories. Four inherited nodes are
  Holy-school and unusable in Shadowform:

  | Trigger | Action | Rel | `remove shadowform` prereq? |
  |---|---|---|---|
  | `critical health` | desperate prayer | 25 | No — fails silently |
  | `low mana` | hymn of hope | 20 | No — fails silently |
  | `low health` | power word: shield (self) | 20 | Yes |
  | `being attacked` | power word: shield (self) | 21 | Yes |

  The two without a prereq burn an action slot on a guaranteed failure. **The two with one are
  worse**: `being attacked` fires on any attacker, so routine add damage costs a GCD leaving
  Shadowform, a GCD shielding and a GCD re-entering, losing the damage aura in between. Per the
  settled decision above, Shadow keeps only `critical health` → self PW:S.

Shadow does not get `save mana` or `healer dps` — `PlayerbotAI::IsHeal` is false for it.

## Confirmed correct — do not re-audit

- Shadow DoT expiry behaviour (S3) — `beforeDuration = 0` is deliberate.
- `power word: shield on party`'s action-node creator is defined but never added to `creators[]`, so
  it has no `remove shadowform` prerequisite. Harmless: no spec reaching the node is ever in
  Shadowform. Recorded so the dead-looking code is not re-investigated.
- Circle of Healing absent from Disc, Pain Suppression absent from Holy and Shadow, Penance absent
  from Holy — talent placement.
- Shadowfiend's exemption from the boss-only burst hold is intentional and must survive any rebinding.
- `mind sear channel check` cancelling when attackers drop below 2.
- The shared out-of-combat ladder in `PriestNonCombatStrategy`.
- `HolyPriestStrategy` inheriting the Disc ladder (H6) — reachable only when ungrouped.

## Documented limitations, deliberately out of scope

- **`PartyMemberToProtect` keys on `attacker->GetVictim()`**, so a raider dying to raid-wide AoE, a
  DoT or a ground effect is nobody's current victim and can never be selected. Pain Suppression
  cannot answer that damage pattern, which is a large share of raid PvE. Widening the value would
  move paladin `blessing of protection on party` and warrior `intervene` too.
- **Surge of Light and Serendipity are not modelled** — both are Holy procs the guide leans on, and
  there is no aura-tracking trigger for either. Adding them means a new `HasAuraTrigger` subclass per
  proc plus band-specific nodes to spend them.
- **Borrowed Time is not modelled** for Disc, for the same reason.

## Needs in-game confirmation

Spell school and Shadowform castability live in DBC, not this repo. Desperate Prayer and Hymn of Hope
were settled by the user (both blocked in Shadowform). Open: whether the Mind Flay channel exposes
remaining duration through an aura on the target, which the proposed clip trigger reads. If not, the
trigger must fall back to the hardcoded-spell-id shape `MindSearChannelCheckTrigger` uses.
