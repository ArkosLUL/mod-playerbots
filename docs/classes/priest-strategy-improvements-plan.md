# Priest PVE raid audit — all three specs

## Context

The Priest bot's three combat ladders (`heal` for Discipline, `holy heal` for Holy, `dps` +
`shadow debuff` + `shadow aoe` for Shadow) were written by copying relevance literals rather than
modelling the WotLK 3.3.5 rotations. The result: several signature spells never fire, several
cooldowns are bound to triggers that cannot become true in a raid, and one node in the shared base
strategy is wired to a trigger name that is not registered at all.

The user asked for an analysis of all three specs in a PVE raid context, with explicit attention to
Hymn of Hope, Divine Hymn, Power Infusion, Pain Suppression, Shadowfiend and Inner Focus.

**Deliverable is documentation only** — two markdown files under `docs/classes/`. No code changes in
this pass. Rotations are grounded against the wowtbc.gg WotLK class guides
(`/wotlk/class-guides/{discipline,holy,shadow}-priest/`), which the user named as the reference.

Decisions already taken by the user, to be recorded as settled in both documents:

- **Discipline gets no Greater Heal.** Too slow for the Disc rotation; the guide omits it entirely.
  Fix Flash Heal's mana-veto metadata instead.
- **Shadow keeps only `critical health` → self Power Word: Shield.** Desperate Prayer and Hymn of
  Hope leave the Shadow ladder — neither is castable in Shadowform, and dropping form costs more DPS
  than the self-heal is worth. Shadow relies on the raid's healers.
- **Blast radius**: priest folder plus narrow additive shared changes only; no retuning of
  `HealerAutoSaveManaMultiplier` itself, which would move every healer class.

Paths are relative to `modules/mod-playerbots/`.

---

## Deliverable

Two documents, matching the existing `docs/classes/` convention
(`holy-paladin-improvements-plan.md`, `resto-shaman-healing-improvements-findings.md`):

1. `docs/classes/priest-all-specs-findings.md` — the audit. Framework facts, per-spec findings, the
   cooldown audit table, the relevance-collision table, and an explicit "confirmed correct, not
   gaps" section.
2. `docs/classes/priest-improvements-plan.md` — the executable spec. Lettered subsections organised
   by file, the full proposed ladder per spec as a `Trigger | Action | Rel | Note` table, a
   registration checklist, and a verification section.

Both self-contained: a fresh session must be able to execute the plan without this conversation.

**Before writing either document, read the existing `docs/classes/*-findings.md` files.** Several
questions this audit raises were already investigated and decided there — `PartyMemberToProtect` and
the `HealerAutoSaveManaMultiplier` health guard both are. Re-flagging a settled decision as a finding
is a defect in the document.

---

## Framework facts the documents rest on

- `Engine::DoNextAction` picks by **relevance global across all active trigger nodes**; ties break by
  insertion order (`src/Bot/Engine/Engine.cpp:160-240`).
- A multiplier that zeroes relevance sends the node down the **IMPOSSIBLE** branch, which *does*
  still push the action node's `/*A*/` alternatives (`Engine.cpp:235-240`). Fallback chains survive
  a mana veto — worth stating, because it is the opposite of what the paladin doc assumed.
- Party health bands are nested, all `minValue = 0` (`src/Ai/Base/Trigger/HealthTriggers.h:88-128`).
- `Engine::PushDefaultActions` pushes `getDefaultActions()` every tick, ungated.
- Config defaults: critical 25, low 45, medium 65, almostFull 85, lowMana 15, mediumMana 40,
  highMana 65, saveManaThreshold 60.
- `HealerAutoSaveManaMultiplier` (`src/Ai/Base/Strategy/ConserveManaStrategy.cpp:93-131`): under 60%
  bot mana, veto when target HP ≥ 65 and (`loss < estAmount` or efficiency ≤ MEDIUM); veto when
  target HP ≥ 45 and (`loss < estAmount` or efficiency ≤ LOW). Tanks get `estAmount / 1.5`.
- `AoeInGroupTrigger` (`HealthTriggers.cpp:47-62`) needs ≥ 3 healable members; threshold 3 (≤5),
  `min(half,4)` (≤10), `min(half,6)` (≤25).
- `BoostTrigger::IsActive` (`src/Ai/Base/Trigger/GenericTriggers.cpp:422-432`) returns true only when
  the current target is a **Player**, or `balance ≤ 50`. In PvE the target is a Creature.
- Strategy sets (`src/Bot/Factory/AiFactory.cpp:292, 302-308, 411-416`): Disc `heal`; Holy
  `holy heal`; Shadow `dps` + `shadow debuff` + `shadow aoe`; all get `boost` + `burst`; healers add
  `save mana` + `healer dps`. `holy dps` is added **only when ungrouped** (`AiFactory.cpp:421-433`),
  so it is irrelevant to raids.

---

## Findings to be written up

### Cross-spec

1. **`TriggerNode("boost", …)` is dead** — `GenericPriestStrategy.cpp:69` binds Shadowfiend @20 to a
   trigger named `"boost"`. No trigger by that name is registered anywhere; `creators["boost"]` in
   `PriestAiObjectContext.cpp:32` is a *strategy* factory. Silent runtime no-op.
2. **Power Infusion is unreachable in a raid** — `PowerInfusionTrigger` is a `BoostTrigger`
   (`PriestTriggers.h:29`), so against a Creature it needs `balance ≤ 50`, which a raid group rarely
   reaches. `power infusion on party` is registered (`PriestAiObjectContext.cpp:207, 325`) and
   referenced by zero trigger nodes.
3. **Inner Focus is bound to `medium mana` @21** (`GenericPriestStrategy.cpp:35`). Both healing
   guides say "Prioritize using with Divine Hymn and Prayer of Healing"; the Shadow guide says use it
   on cooldown with Mind Flay. Current binding spends a 3-minute free-cast on whatever the engine
   happens to pick below 40% mana. Its purpose-built `InnerFocusTrigger` (`PriestTriggers.h:30`,
   registered `:92`/`:134`) is dead.
4. **Hymn of Hope on `low mana` (< 15%) @20** (`GenericPriestStrategy.cpp:38`) — an 8-second channel
   on a 6-minute cooldown, fired when the priest is already nearly out of mana and therefore stops
   healing for 8 seconds. Also part of a three-way relevance tie at 20.
5. **Shadowfiend on `medium mana` (< 40%) @22** (`GenericPriestStrategy.cpp:34`) — a 5-minute
   cooldown gated on being two-fifths empty loses uses over a long fight. It is already burst-listed
   (`src/Ai/Base/Combat/BurstCooldowns.cpp:36`) and exempt from the boss-only hold
   (`src/Ai/Base/Strategy/BurstWindowStrategy.cpp:42`), so rebinding to `SpellNoCooldownTrigger`
   (`GenericTriggers.h:197`) is safe and matches what commit 342bba906 did for Rapid Fire and
   Bestial Wrath. Separately `CastShadowfiendAction::GetTargetName()` returns `"current target"`
   (`PriestActions.h:213`), so a healer with no target cannot cast it.
6. **Dead registrations** — `symbol of hope`, `binding heal`, `lightwell`, `holy nova`,
   `mass dispel`, `levitate`, `mind soothe`, `consume magic`, `elune's grace`,
   `power word: shield on almost full health below` and `power infusion on party` are all registered
   actions with zero trigger nodes.
7. **`GenericPriestStrategy` hands Shadow four Holy-school nodes it should not have** — see S8.
   Because a derived `InitTriggers` cannot remove a base node, the fix is relocation: move the
   offending nodes down into `HealPriestStrategy` and `HolyHealPriestStrategy` verbatim and leave
   only what all three specs want in the base. This is the pattern the paladin plan used for Divine
   Plea (`docs/classes/holy-paladin-improvements-plan.md:206-212`).

### Discipline (`heal`, `HealPriestStrategy.cpp`)

Guide priority: PW:S (tank + spread before raid damage) → Renew on tank → Penance → Prayer of
Mending → Flash Heal **when the target has Weakened Soul** → Binding Heal when the priest is also
hurt → Prayer of Healing with Borrowed Time. **Greater Heal is not in the Disc rotation** — confirmed
with the user; it is too slow. Cooldown order: Divine Hymn > Hymn of Hope > Pain Suppression >
Power Infusion > Inner Focus > Shadowfiend.

- **D1. Flash Heal is offline under mana pressure.** `CastFlashHealOnPartyAction` is
  `15.0f, LOW` (`PriestActions.h:74`); below 60% bot mana LOW is vetoed for any target above 45% HP.
  Its `/*A*/` alternative is `greater heal on party` (`GenericPriestStrategyActionNodeFactory.h:193-201`)
  at `50.0f, MEDIUM` — also vetoed above 65%, and not a Disc spell. Flash Heal is Disc's only direct
  heal in the ladder, so the spec loses single-target healing exactly when mana is tight.
- **D2. No Weakened Soul guard** on `power word: shield on party` or the self `power word: shield`.
  Only the two custom variants check it (`PriestActions.cpp:40, 69, 95`). PW:S on party leads Disc's
  critical band @35, so a Weakened-Soul target makes the bot re-attempt a guaranteed failure. The
  guide's own rule — Flash Heal *because* the target has Weakened Soul — is the missing branch.
- **D3. No Renew maintenance on the tank** (guide #2). `renew on party` appears once, in the
  almost-full band @11 (`HealPriestStrategy.cpp:92`). `BuffOnMainTankTrigger` /
  `BuffOnMainTankAction` already exist (`GenericTriggers.h:961-969`,
  `GenericSpellActions.h:489-498`) — reuse, do not write new plumbing.
- **D4. Binding Heal is dead code.** `BindingHealTrigger` (`PriestTriggers.h:94-100`, body
  `PriestTriggers.cpp:39-48`: party member low **and** self below mediumHealth) and
  `CastBindingHealAction` (`PriestActions.h:86`) are both registered and used by nothing. The trigger
  already encodes the guide's condition exactly.
- **D5. Prayer of Healing has no Borrowed Time pairing** — only on `medium group heal setting` @34
  (`HealPriestStrategy.cpp:45`).
- **D6. Pain Suppression is correctly wired — do not change it.** Self @91 on `critical health`,
  party @90 on `protect party member` (`HealPriestStrategy.cpp:106-120`).

  `PartyMemberToProtect::Calculate` (`src/Ai/Base/Value/PartyMemberToHeal.cpp:165-212`) walks the
  **attackers** list, takes each attacker's current victim, and keeps only players that are alive,
  within 30 yards of that attacker, pass the inherited map / LOS / `spellDistance * 2` / GM check
  (`PartyMemberValue.cpp:107-115`), and sit **below 30% HP — or below 10% if the victim is a tank**.
  It then sorts by health and returns the lowest. `pVictim == bot` is excluded, which is why the
  self-cast lives on its own `critical health` node.

  For a 3-minute −40%-damage-taken cooldown that is strictly better than
  `party member critical health`: it requires damage to actually be incoming, and the split
  tank/non-tank thresholds are calibrated, where the critical-health trigger is a flat 25% with no
  incoming-damage requirement at all.

  `docs/classes/resto-shaman-healing-improvements-findings.md` already settled this deliberately —
  point 6 of the `PartyMemberToProtect` revival section: "Priest stays on the generic
  `party member to protect` — Pain Suppression on a dying tank is the intended use." That doc also
  records that the value had an unreachable body until recently, so `pain suppression on party` has
  only just started firing at all. The findings doc must cite this so the decision is not revisited
  a third time.
- **D7. Penance is priced below PW:S in every band** (34 vs 35, 22 vs 24, 16 vs 19) and its
  `25.0f, HIGH` metadata means the `loss < estAmount` clause vetoes it above 75% target HP under mana
  pressure. It is the guide's #4 and the spec's signature cooldown-driven heal.
- **D8. Collisions**: `flash heal on party` @20 = `hymn of hope` @20 = `power word: shield`
  (low health) @20; `penance on party` @22 = `shadowfiend` @22; `power word: shield`
  (being attacked) @21 = `inner focus` @21.

### Holy (`holy heal`, `HolyPriestStrategy.cpp:80-171`)

Guide priority: Renew on tank → Prayer of Mending → Circle of Healing for AoE → Flash Heal on Surge
of Light → Flash Heal / Binding Heal to build Serendipity → Prayer of Healing → Greater Heal →
Renew as filler. Cooldowns: Divine Hymn > Hymn of Hope > Guardian Spirit > Inner Focus > Shadowfiend.

- **H1. Greater Heal is priced out of its own bands.** It uses `ACTION_MEDIUM_HEAL + 2` = 22 inside
  the **critical** node (`:123`, siblings 36/35/33/31) so it never fires there, and
  `ACTION_MEDIUM_HEAL + 5` = 25 inside the **medium** node (`:147`, siblings 17/16/14) where it
  outranks the entire low band (24/23/22/21). Net effect in a raid: the priest casts a 2.5-second
  Greater Heal on a 60% target instead of Circle of Healing on a 40% one.
- **H2. Renew is neither maintained on the tank (guide #1) nor used as the filler (guide #9)** — one
  node, almost-full band @12 (`:157`).
- **H3. Circle of Healing is absent from the critical band** despite being the spec's AoE answer.
- **H4. Binding Heal is dead** — same as D4, and Holy actually needs it for Serendipity stacking.
- **H5. Collisions**: `guardian spirit on party` @36 = `prayer of mending on party` @36;
  `circle of healing on party` @35 = `power word: shield on party` @35; `greater heal on party` @25 =
  `desperate prayer` @25; `greater heal on party` @22 = `shadowfiend` @22; `flash heal on party` @21
  = `inner focus` @21 = `power word: shield` (being attacked) @21.
- **H6. `reach party member to heal` @40** (`:167`) outranks Divine Hymn @37 and Guardian Spirit @36
  — an out-of-range selected target makes the priest run instead of spending a raid cooldown on the
  people who are in range. The paladin doc settled the equivalent node at 39.5.
- **H7. Structural**: `HolyPriestStrategy : HealPriestStrategy` (`:28`) makes the off-spec `holy dps`
  layer inherit the **Discipline** ladder, including Penance nodes a Holy priest has no talent for.
  Only reachable when ungrouped, so raid impact is nil — flag as deferred, do not fix.

### Shadow (`dps` + `shadow debuff` + `shadow aoe`, `ShadowPriestStrategy.cpp`)

Guide priority: Vampiric Embrace → maintain Vampiric Touch → maintain Devouring Plague → maintain
Shadow Word: Pain (Mind Flay refreshes it) → Mind Blast → Shadow Word: Death **only if the target
dies before a Mind Blast or Mind Flay finishes** → Mind Flay, clipped after the second tick when
something higher is ready. Cooldowns: Shadowfiend before Bloodlust, Dispersion for mana or big
damage, Inner Focus on cooldown with Mind Flay.

- **S1. Shadow Word: Death is wired as a filler, not an execute.** `ShadowPriestStrategy.cpp:22`
  puts it in `getDefaultActions()` @5.1, ungated, with a comment claiming it is for movement. On a
  boss the guide's condition is never met, so the bot takes backlash damage every cooldown for
  nothing, with no self-health floor anywhere. The time-to-die idiom already exists and is reusable:
  `target->GetHealth() / AI_VALUE(float, "estimated group dps")` in `DebuffTrigger::IsActive`
  (`GenericTriggers.cpp:311-319`) and `TargetWithComboPointsLowerHealTrigger` (`:70-78`).
- **S2. Vampiric Embrace, the guide's #1, exists only out of combat** (`PriestNonCombatStrategy.cpp:24`
  @16). If it falls off mid-fight it is never reapplied. Trigger and action are both already
  registered — pure wiring.
- **S3. Do NOT add DoT refresh windows.** VT, DP and SW:P run `beforeDuration = 0` through
  `DEBUFF_CHECKISOWNER_TRIGGER` (`src/Bot/Engine/AiObject.h:86-91`), and the guide is explicit:
  "Let it expire before you re-apply it" for both VT and DP. This is **correct as written** and has
  never been modified — no priest rotation commit exists in the branch history.

  Guardrail for whoever executes the plan: rogue sets the opposite value on purpose
  (`RogueTriggers.h:23, 32, 58` — `beforeDuration = 2000` on Slice and Dice, Hunger for Blood and
  Rupture), which is right for rogue, where those effects are refreshed before they drop. Do not
  "make priest consistent" with it. The two specs want opposite behaviour from the same parameter,
  and copying the rogue value here would introduce a Shadow DPS regression that does not exist today.
- **S4. Shadowfiend only fires below 40% mana** (inherited generic node) and the `boost` node is
  dead — see cross-spec 1 and 5.
- **S5. Dispersion has two nodes at the identical relevance 25** — `low mana` and `critical health`
  (`ShadowPriestStrategy.cpp:39-54`), resolved by insertion order.
- **S6. `shadowform` @20** collides with `hymn of hope` @20 and `power word: shield` (low health) @20.
- **S7. No Mind Flay clipping.** The guide wants the channel interrupted after the second tick when a
  higher-priority spell is up. The precedent already exists for Mind Sear —
  `MindSearChannelCheckTrigger` (`PriestTriggers.cpp:55-72`) plus the `cancel channel` action — and
  nothing equivalent exists for Mind Flay.
- **S8. Shadow drops Shadowform for inherited Holy-school spells.** `ShadowPriestStrategy` derives
  from `GenericPriestStrategy`, so its ctor runs the base ctor first and the bot carries **both**
  `GenericPriestStrategyActionNodeFactory` and `ShadowPriestStrategyActionNodeFactory`. Four
  inherited nodes (`GenericPriestStrategy.cpp:23-44`) are Holy-school and unusable in Shadowform:

  | Trigger | Action | Rel | Has `remove shadowform` prereq? |
  |---|---|---|---|
  | `critical health` | `desperate prayer` | 25 | No — fails silently |
  | `low mana` | `hymn of hope` | 20 | No — fails silently |
  | `low health` | `power word: shield` (self) | 20 | Yes (`…ActionNodeFactory.h:93-102`) |
  | `being attacked` | `power word: shield` (self) | 21 | Yes |

  The two without a prereq burn an action slot on a guaranteed failure. The two with one are worse:
  `being attacked` fires on any attacker, so on routine add damage the bot spends a GCD leaving
  Shadowform, a GCD shielding and a GCD re-entering, losing the damage aura in between.

  **User decision:** Shadow keeps only `critical health` → self PW:S as an emergency button. Desperate
  Prayer and Hymn of Hope come out of the Shadow ladder entirely — the priest relies on the raid's
  healers rather than spending DPS time on self-healing. Shadow's mana tools stay Dispersion
  (`low mana` @25, castable in Shadowform) and Shadowfiend.

  Confirmed by the user: neither Desperate Prayer nor Hymn of Hope is castable in Shadowform. Divine
  Hymn is not affected — it appears only in the Disc and Holy ladders, which Shadow never loads.
- **S9. Not a finding, checked and cleared.** `power word: shield on party` is healer-only — it
  appears in `HealPriestStrategy.cpp` and `HolyPriestStrategy.cpp:121`, never in
  `ShadowPriestStrategy`. Its action-node creator (`GenericPriestStrategyActionNodeFactory.h:103-111`)
  is defined but never added to `creators[]`, so it has no `remove shadowform` prerequisite — which
  is harmless, because no spec that reaches the node is ever in Shadowform. Recorded so the same
  dead-looking code is not re-investigated later.

### Cooldown audit table (goes in the findings doc)

`Cooldown | Implemented | Reachable | Trigger | Rel | Verdict`, covering Hymn of Hope, Divine Hymn,
Power Infusion (+ on party), Pain Suppression (+ on party), Shadowfiend, Inner Focus, Guardian
Spirit, Penance, Dispersion, Desperate Prayer, Fade, Symbol of Hope, Lightwell — each with the
file:line where it is defined, registered and referenced, and whether a raid PvE bot can actually
reach it.

### Confirmed correct, not gaps

Shadow DoT expiry behaviour (S3); the missing `remove shadowform` prereq on
`power word: shield on party` (S9); Circle of Healing absent from Disc; Pain Suppression absent from
Holy and Shadow; Penance absent from Holy; Shadowfiend's exemption from the boss-only burst hold;
`mind sear channel check` cancelling when attackers drop below 2; the shared out-of-combat ladder in
`PriestNonCombatStrategy.cpp`.

### Documented limitations, deliberately out of scope

`PartyMemberToProtect` keys on `attacker->GetVictim()`, so a raider dying to raid-wide AoE, a DoT, or
a ground effect is nobody's current victim and can never be selected. Pain Suppression therefore
cannot answer that damage pattern, which is a large share of raid PvE. Widening the value would move
`blessing of protection on party` (Paladin) and `intervene` (Warrior) too, which is outside the
chosen blast radius. Record it; do not act on it.

### Open items requiring in-game confirmation

Spell school and Shadowform castability are not derivable from this repo — that data lives in DBC.
Desperate Prayer and Hymn of Hope were settled by the user (both blocked in Shadowform). Anything
else of this kind found while writing the documents gets listed here with the exact in-game test,
not asserted.

---

## Fix directions the plan doc will specify

Not executed this pass — the plan doc records them for a later session. Constrained to the priest
folder plus narrow additive shared changes (the user's chosen blast radius).

- Reprice healing metadata in `PriestActions.h` (`Action | Before | After | Why` table) so the mana
  multiplier stops disabling each spec's actual filler: Flash Heal off `LOW`, Penance's `estAmount`
  down from 25, Guardian Spirit and Pain Suppression explicit at `SUPERIOR`.
- Weakened Soul guard on the plain PW:S actions, plus a Weakened-Soul-driven Flash Heal branch for
  Disc.
- New triggers, all in `PriestTriggers.h/.cpp`: a Weakened-Soul check, a Shadow Word: Death execute
  gate with a bot-health floor, a Mind Flay clip check modelled on `MindSearChannelCheckTrigger`, and
  a raid-safe Power Infusion trigger replacing the unusable `BoostTrigger`.
- Rebind Inner Focus to fire alongside Divine Hymn / Prayer of Healing (healers) and Mind Flay
  (Shadow); rebind Shadowfiend to `SpellNoCooldownTrigger`; raise Hymn of Hope's mana gate off 15%.
- **Relocate the Holy-school nodes out of `GenericPriestStrategy::InitTriggers`** (S8). Move
  `critical health` → `desperate prayer` @25, `low mana` → `hymn of hope` @20, `low health` → self
  `power word: shield` @20 and `being attacked` → self `power word: shield` @21 into
  `HealPriestStrategy::InitTriggers` and `HolyHealPriestStrategy::InitTriggers` verbatim. Leave
  `critical health` → self `power word: shield` @10 in the base, which is the emergency button Shadow
  keeps. A derived `InitTriggers` cannot remove a base node, so relocation is the only mechanism.
- Wire the existing-but-dead `binding heal` trigger and action into both healing ladders.
- Tank Renew (and tank PW:S for Disc) via the existing `BuffOnMainTankTrigger` /
  `BuffOnMainTankAction`.
- Full relevance rewrite per spec, no two reachable actions sharing a value, with
  `reach party member to heal` dropped off 40.
- Delete the dead `TriggerNode("boost", …)` and register `power_word_shield_on_party` in the action
  node factory.
- Shared, additive only: consider adding Power Infusion to `BurstCooldowns.cpp`'s priest block, with
  the healer exemption shape in `BurstWindowStrategy.cpp`.

---

## Files to be created

- `docs/classes/priest-all-specs-findings.md` — the audit
- `docs/classes/priest-improvements-plan.md` — the executable spec

No source files change in this pass.

---

## Verification

Documentation-only, so verification is review rather than execution:

1. Every `file:line` citation in both documents resolves to the claimed code on the current `Custom`
   branch.
2. Every action and trigger name string quoted from a strategy ladder has a matching `creators[...]`
   entry in `PriestAiObjectContext.cpp`, or is explicitly labelled dead in the findings.
3. The proposed relevance tables contain no duplicate values within a spec, checked against the
   inherited `GenericPriestStrategy` nodes (fade 55, interrupts 40/41, dispel 40/41) and
   `PriestCureStrategy` (30/31/40/41).
4. Every rotation claim is attributable either to the wowtbc.gg guide for that spec or to a cited
   line of code — no unsourced assertions about WotLK play.
5. Both documents read standalone: hand `priest-improvements-plan.md` to a fresh session and it can
   execute without this conversation.
