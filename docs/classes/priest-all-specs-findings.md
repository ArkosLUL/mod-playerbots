# Priest PVE raid audit — Discipline, Holy, Shadow

## Context

The Priest bot's three combat ladders were written by copying relevance literals rather than
modelling the WotLK 3.3.5 rotations. Several signature spells never fire, several cooldowns are bound
to triggers that cannot become true in a raid, and one node in the shared base strategy is wired to a
trigger name that is not registered at all.

This document is the audit. The executable fix lives in
`docs/classes/priest-improvements-plan.md`. Both are self-contained — a fresh session can read either
without the originating conversation.

Rotations are grounded against the wowtbc.gg WotLK class guides
(`/wotlk/class-guides/{discipline,holy,shadow}-priest/`). Paths are relative to
`modules/mod-playerbots/`. All `file:line` references were checked against the `Custom` branch.

Decisions already settled with the user, recorded here so they are not re-litigated:

- **Discipline gets no Greater Heal.** Too slow for the Disc rotation; the guide omits it. Fix Flash
  Heal's mana-veto metadata instead.
- **Shadow keeps only `critical health` → self Power Word: Shield.** Desperate Prayer and Hymn of
  Hope leave the Shadow ladder — neither is castable in Shadowform, and dropping form costs more DPS
  than the self-heal is worth. Shadow relies on the raid's healers.
- **Blast radius**: the priest folder plus narrow additive shared changes. No retuning of
  `HealerAutoSaveManaMultiplier` itself, which would move every healer class.

---

## Framework facts the findings rest on

- `Engine::DoNextAction` picks by **relevance global across all active trigger nodes**; ties break by
  insertion order, which reads as nondeterministic in-game
  (`src/Bot/Engine/Engine.cpp:166-245`).
- Relevance constants (`src/Bot/Engine/Strategy/Strategy.h:53-65`): `ACTION_DEFAULT 5`,
  `ACTION_NORMAL 10`, `ACTION_LIGHT_HEAL 10`, `ACTION_HIGH 20`, `ACTION_MEDIUM_HEAL 20`,
  `ACTION_MOVE 30`, `ACTION_CRITICAL_HEAL 30`, `ACTION_INTERRUPT 40`, `ACTION_DISPEL 50`,
  `ACTION_RAID 60`, `ACTION_EMERGENCY 90`.
- A multiplier that zeroes relevance breaks out of the multiplier loop, fails the
  `isPossible() && relevance > 0` gate and lands on the **IMPOSSIBLE** branch — which *does* still
  push the action node's `/*A*/` alternatives (`Engine.cpp:190-240`). Fallback chains therefore
  survive a mana veto.
- The four party health bands are **nested, not exclusive** — all pass `minValue = 0`
  (`src/Ai/Base/Trigger/HealthTriggers.h:88-128`). A target at 20% fires critical, low, medium *and*
  almost-full simultaneously; only relevance separates them.
- `Engine::PushDefaultActions` (`Engine.cpp:508-516`) pushes every active strategy's
  `getDefaultActions()` every tick, ungated by any trigger.
- Config defaults (`src/PlayerbotAIConfig.cpp:95-117`): critical 25, low 45, medium 65,
  almostFull 85, lowMana 15, mediumMana 40, highMana 65, saveManaThreshold 60, healDistance 38.5.
- `HealerAutoSaveManaMultiplier` (`src/Ai/Base/Strategy/ConserveManaStrategy.cpp:93-131`): above 60%
  bot mana it always returns 1. Below that, for any `CastHealingSpellAction` it vetoes when target
  HP ≥ 65 and (`lossAmount < estAmount` or efficiency ≤ MEDIUM), and when target HP ≥ 45 and
  (`lossAmount < estAmount` or efficiency ≤ LOW). `lossAmount = 100 - targetHealthPct`. Tanks get
  `estAmount / 1.5`. Efficiency ranks (`src/PlayerbotAIConfig.h:34-42`): VERY_LOW 1, LOW 2, MEDIUM 4,
  HIGH 8, VERY_HIGH 16, SUPERIOR 32.
- `AoeInGroupTrigger` (`HealthTriggers.cpp:47-63`) needs ≥ 3 healable members; threshold 3 (≤5),
  `min(half,4)` (≤10), `min(half,6)` (≤25), `min(half,8)` above. `group heal setting` counts members
  below 85% HP, `medium group heal setting` below 65% (`src/Ai/Base/TriggerContext.h:289, 293`).
- `BoostTrigger::IsActive` (`src/Ai/Base/Trigger/GenericTriggers.cpp:422-432`,
  decl `GenericTriggers.h:445-455`) requires the buff to be missing **and** either the current target
  to be a Player or `balance ≤ 50`. In PvE the target is a Creature and a raid group rarely drops to
  balance 50, so every `BoostTrigger` subclass is effectively dead in raid PvE.
- Strategy sets (`src/Bot/Factory/AiFactory.cpp:292, 302-309, 411-416, 420-433`): every priest gets
  `boost` + `burst` + `dps assist` + `cure`; Disc `heal`, Holy `holy heal`, Shadow `dps` +
  `shadow debuff` + `shadow aoe`; healers add `save mana` + `healer dps`. `holy dps` is added **only
  when ungrouped**, so `HolyPriestStrategy` is irrelevant to raids.
- Inherited nodes every priest carries, which the relevance tables must not collide with:
  `CombatStrategy::InitTriggers` (`src/Ai/Base/Strategy/CombatStrategy.cpp:11-56`) — `drop target` 99,
  `check mount state` 54, `set facing` 37, `reach spell` 20, `reset` 1; `GenericPriestStrategy` —
  `set pet stance` 60, `fade` 55, `flee` 39, `apply oil` 1; `PriestCureStrategy`
  (`GenericPriestStrategy.cpp:53-63`) — `dispel magic` 41, `dispel magic on party` 40,
  `abolish disease` 31, `abolish disease on party` 30; `PriestBoostStrategy` (`:65-70`) —
  `power infusion` 41.

---

## Cross-spec findings

### X1. `TriggerNode("boost", …)` is dead

`GenericPriestStrategy.cpp:69` binds Shadowfiend @20 to a trigger named `"boost"`. No trigger by that
name is registered anywhere. `creators["boost"]` in `PriestAiObjectContext.cpp:32` is a **strategy**
factory, not a trigger. A missing trigger name is a silent runtime no-op, not a compile error.

### X2. Power Infusion is unreachable in a raid, and its node collides with dispel

`PowerInfusionTrigger` is a `BOOST_TRIGGER` (`PriestTriggers.h:29`), so in PvE it needs
`balance ≤ 50` — see the `BoostTrigger` fact above. Its only node,
`PriestBoostStrategy::InitTriggers` (`GenericPriestStrategy.cpp:68`), sits at 41, tying with
`dispel magic` @41 from `PriestCureStrategy`, which every priest also runs. The tie is moot only
because the node never activates. `power infusion on party` is registered
(`PriestAiObjectContext.cpp:207, 325`) and referenced by zero trigger nodes.

### X3. The registered `shadowfiend` trigger is also boost-gated

`ShadowfiendTrigger` is `BOOST_TRIGGER_A` (`PriestTriggers.h:44`) with
`IsActive() = BoostTrigger::IsActive() && !bot->HasSpellCooldown(34433)`
(`PriestTriggers.cpp:37`). So the purpose-built trigger has the same raid-PvE blindness as Power
Infusion. Its only consumer is `HolyPriestStrategy.cpp:54-61` (`holy dps`, ungrouped only), so raid
impact today is nil — but it means there is currently **no** working "Shadowfiend is off cooldown"
trigger for any spec to use.

### X4. Inner Focus is bound to `medium mana` @21

`GenericPriestStrategy.cpp:35`. Both healing guides say "Prioritize using with Divine Hymn and Prayer
of Healing"; the Shadow guide says use it on cooldown with Mind Flay. The current binding spends a
3-minute free-cast on whatever the engine happens to pick below 40% mana. Its purpose-built
`InnerFocusTrigger` (`PriestTriggers.h:30`, registered `PriestAiObjectContext.cpp:92, 134`) is dead —
and would not help as written: `BUFF_TRIGGER` resolves to `BuffTrigger`, whose `IsActive` is simply
"the aura is missing" (`GenericTriggers.cpp:193-204`), with no cooldown check.

### X5. Hymn of Hope fires at < 15% mana

`GenericPriestStrategy.cpp:38` binds it to `low mana` @20. It is an 8-second channel on a 6-minute
cooldown, fired when the priest is already nearly out of mana and therefore stops healing for 8
seconds at the worst possible moment. It is also part of a four-way relevance tie at 20 (see the
collision tables).

### X6. Shadowfiend fires only below 40% mana

`GenericPriestStrategy.cpp:34`, `medium mana` @22. A 5-minute cooldown gated on being two-fifths
empty loses uses over a long fight. It is already burst-listed (`src/Ai/Base/Combat/BurstCooldowns.cpp:35-36`)
and explicitly exempt from the boss-only hold (`src/Ai/Base/Strategy/BurstWindowStrategy.cpp:41-52`,
the exemption reads `name == "shadowfiend"`), so rebinding it to a plain cooldown trigger is safe and
matches what commit 342bba906 did for Rapid Fire and Bestial Wrath.

Separately, `CastShadowfiendAction::GetTargetName()` returns `"current target"`
(`PriestActions.h:208-214`), so a healer with no current target cannot cast it at all.

### X7. Dead registrations

Registered actions with zero trigger nodes anywhere: `symbol of hope`, `binding heal`, `lightwell`,
`holy nova`, `mass dispel`, `levitate`, `mind soothe`, `consume magic`, `elune's grace`,
`power word: shield on almost full health below`, `power infusion on party`.

### X8. `GenericPriestStrategy` hands Shadow four Holy-school nodes it should not have

See S8 below. A derived `InitTriggers` cannot remove a base node, so the fix is relocation: move the
offending nodes down into the two healing strategies verbatim and leave only what all three specs
want in the base. Same pattern the paladin plan used for Divine Plea
(`docs/classes/holy-paladin-improvements-plan.md:206-212`).

---

## Discipline — `heal`, `HealPriestStrategy.cpp`

Guide priority: PW:S (tank plus spread before raid damage) → Renew on tank → Penance → Prayer of
Mending → Flash Heal **when the target has Weakened Soul** → Binding Heal when the priest is also
hurt → Prayer of Healing with Borrowed Time. **Greater Heal is not in the Disc rotation** — settled
with the user. Cooldown order: Divine Hymn > Hymn of Hope > Pain Suppression > Power Infusion >
Inner Focus > Shadowfiend.

- **D1. Flash Heal is offline under mana pressure.** `CastFlashHealOnPartyAction` is
  `15.0f, LOW` (`PriestActions.h:74`). LOW ≤ MEDIUM and LOW ≤ LOW, so below 60% bot mana it is
  vetoed for **any** target above 45% HP regardless of `estAmount`. Its `/*A*/` alternative is
  `greater heal on party` (`GenericPriestStrategyActionNodeFactory.h:193-201`) at `50.0f, MEDIUM`
  (`PriestActions.h:72`), which is itself vetoed above 65% HP always and above 50% HP under the
  `lossAmount < estAmount` clause — and is not a Disc spell anyway. Flash Heal is Disc's only direct
  heal in the ladder, so the spec loses single-target healing exactly when mana is tight.
- **D2. No Weakened Soul guard** on `power word: shield on party` or the self `power word: shield`.
  Only the two custom variants check it (`PriestActions.cpp:40, 69, 95`). PW:S on party leads Disc's
  critical band @35, so a Weakened-Soul target makes the bot re-attempt a guaranteed failure every
  tick. The guide's own rule — Flash Heal *because* the target has Weakened Soul — is the missing
  branch.
- **D3. No Renew maintenance on the tank** (guide #2). `renew on party` appears once, in the
  almost-full band @11 (`HealPriestStrategy.cpp:92`). `BuffOnMainTankTrigger` /
  `BuffOnMainTankAction` already exist (`src/Ai/Base/Trigger/GenericTriggers.h:961-969` with
  `GetTargetValue()` at `GenericTriggers.cpp:770`, `src/Ai/Base/Actions/GenericSpellActions.h:489-498`) —
  reuse, do not write new plumbing.
- **D4. Binding Heal is dead code.** `BindingHealTrigger` (`PriestTriggers.h:94-100`, body
  `PriestTriggers.cpp:39-48`: a party member below `lowHealth` **and** the bot itself below
  `mediumHealth`) and `CastBindingHealAction` (`PriestActions.h:86`) are both registered
  (`PriestAiObjectContext.cpp:102/148` and `:233/326`) and used by nothing. The trigger already
  encodes the guide's condition exactly.
- **D5. Prayer of Healing has no Borrowed Time pairing** — it appears only on
  `medium group heal setting` @34 (`HealPriestStrategy.cpp:45`), where it ties with
  `penance on party` (see D8).
- **D6. Pain Suppression is correctly wired — do not change it.** Self @91 on `critical health`,
  party @90 on `protect party member` (`HealPriestStrategy.cpp:106-120`).

  `PartyMemberToProtect::Calculate` (`src/Ai/Base/Value/PartyMemberToHeal.cpp:165-211`) walks the
  **attackers** list, takes each attacker's current victim, and keeps only players that are alive,
  within 30 yards of that attacker, pass the inherited map / LOS / `spellDistance * 2` / GM check
  (`src/Ai/Base/Value/PartyMemberValue.cpp:107-115`), and sit **below 30% HP — or below 10% if the
  victim is a tank**. It sorts by health and returns the lowest. `pVictim == bot` is excluded, which
  is why the self-cast lives on its own `critical health` node.

  For a 3-minute −40%-damage-taken cooldown that is strictly better than
  `party member critical health`: it requires damage to actually be incoming, and the split
  tank/non-tank thresholds are calibrated, where the critical-health trigger is a flat 25% with no
  incoming-damage requirement at all.

  `docs/classes/resto-shaman-healing-improvements-findings.md:198-237` already settled this
  deliberately — point 6: "Priest stays on the generic `party member to protect` — Pain Suppression
  on a dying tank is the intended use." That doc also records that the value had an unreachable body
  until recently, so `pain suppression on party` has only just started firing at all.
- **D7. Penance is priced below PW:S in every band** (34 vs 35 critical, 22 vs 24 low, 16 vs 19
  medium) and its `25.0f, HIGH` metadata (`PriestActions.h:184-190`) means the `lossAmount < estAmount`
  clause vetoes it above 75% target HP under mana pressure. It is the guide's #4 and the spec's
  signature cooldown-driven heal.

### D8. Discipline relevance map (current)

Every reachable node, including inherited ones. Collisions in bold.

| Rel | Nodes |
|---|---|
| 99 | `drop target` |
| 91 / 90 | `pain suppression` (self critical) / `pain suppression on party` (protect) |
| 60 / 55 / 54 | `set pet stance` / `fade` / `check mount state` |
| **41** | **`dispel magic`** = **`power infusion`** (both `PriestCureStrategy` and `PriestBoostStrategy`) |
| **40** | **`dispel magic on party`** = **`reach party member to heal`** |
| 39 / 37 | `flee` / `set facing` |
| 36 | `prayer of mending on party` (medium group heal) |
| **35** | **`power word: shield on party`** (critical band) = **`power word: shield on not full`** (medium group heal) |
| **34** | **`penance on party`** (critical band) = **`prayer of healing on party`** (medium group heal) |
| 33 | `prayer of mending on party` (critical band) |
| 32 | `flash heal on party` (critical band) |
| 31 / 30 | `abolish disease` / `abolish disease on party` |
| 28 / 27 | `prayer of mending on party` / `power word: shield on not full` (group heal setting) |
| 25 | `desperate prayer` (self critical) |
| 24 / 23 | `power word: shield on party` / `prayer of mending on party` (low band) |
| **22** | **`penance on party`** (low band) = **`shadowfiend`** (medium mana) |
| **21** | **`power word: shield`** (being attacked) = **`inner focus`** (medium mana) |
| **20** | **`flash heal on party`** (low band) = **`hymn of hope`** (low mana) = **`power word: shield`** (low health) = **`reach spell`** (enemy out of spell) |
| 19 / 17 / 16 / 15 | `power word: shield on party` / `prayer of mending on party` / `penance on party` / `flash heal on party` (medium band) |
| 13 / 12 / 11 | `power word: shield on party` / `prayer of mending on party` / `renew on party` (almost full band) |
| 10 | `power word: shield` (self critical health) |
| 5.5 – 5.0 | `healer dps` — **`shadow word: pain`** = **`mind sear`** at 5.5, then holy fire 5.4, smite 5.3, mind blast 5.2, shoot 5.0 |

The two collisions the earlier audit missed are `reach party member to heal` = `dispel magic on party`
at 40, and `penance on party` = `prayer of healing on party` at 34.

---

## Holy — `holy heal`, `HolyPriestStrategy.cpp:80-171` (`HolyHealPriestStrategy`)

Guide priority: Renew on tank → Prayer of Mending → Circle of Healing for AoE → Flash Heal on Surge
of Light → Flash Heal / Binding Heal to build Serendipity → Prayer of Healing → Greater Heal → Renew
as filler. Cooldowns: Divine Hymn > Hymn of Hope > Guardian Spirit > Inner Focus > Shadowfiend.

- **H1. Greater Heal is priced out of its own bands.** It uses `ACTION_MEDIUM_HEAL + 2` = 22 inside
  the **critical** node (`:123`, siblings 36/35/33/31) so it never fires there, and
  `ACTION_MEDIUM_HEAL + 5` = 25 inside the **medium** node (`:147`) where it outranks the entire low
  band (24/23/22/21). Net effect in a raid: the priest casts a 2.5-second Greater Heal on a 60%
  target instead of Circle of Healing on a 40% one.
- **H2. Renew is neither maintained on the tank (guide #1) nor used as the filler (guide #9)** — one
  node, almost-full band @12 (`:157`).
- **H3. Circle of Healing is absent from the critical band** despite being the spec's AoE answer.
- **H4. Binding Heal is dead** — same as D4, and Holy actually needs it for Serendipity stacking.
- **H5. `reach party member to heal` @40** (`:167`) outranks Divine Hymn @37 and Guardian Spirit @36:
  an out-of-range selected target makes the priest run instead of spending a raid cooldown on the
  people who are in range. It also ties with `dispel magic on party` @40. The paladin doc settled the
  equivalent node at 39.5.
- **H6. Structural, deferred.** `HolyPriestStrategy : HealPriestStrategy` (`:28`) makes the off-spec
  `holy dps` layer inherit the **Discipline** ladder, including Penance nodes a Holy priest has no
  talent for. `holy dps` is only added when ungrouped (`AiFactory.cpp:420-433`), so raid impact is
  nil — flag, do not fix.

### H7. Holy relevance map (current)

| Rel | Nodes |
|---|---|
| 99 / 60 / 55 / 54 | `drop target` / `set pet stance` / `fade` / `check mount state` |
| 41 | `dispel magic` = `power infusion` (dead) |
| **40** | **`dispel magic on party`** = **`reach party member to heal`** |
| 39 / **37** | `flee` / **`set facing`** = **`divine hymn`** (medium group heal) |
| **36** | **`guardian spirit on party`** (critical band) = **`prayer of mending on party`** (medium group heal) |
| **35** | **`power word: shield on party`** (critical band) = **`circle of healing on party`** (medium group heal) |
| 34 / 33 | `prayer of healing on party` (medium group heal) / `prayer of mending on party` (critical band) |
| 31 | `flash heal on party` (critical band) |
| 30 | `abolish disease on party` — and 31 `abolish disease` |
| 29 / 28 | `prayer of mending on party` / `circle of healing on party` (group heal setting) |
| **25** | **`greater heal on party`** (medium band) = **`desperate prayer`** (self critical) |
| 24 / 23 | `circle of healing on party` / `prayer of mending on party` (low band) |
| **22** | **`greater heal on party`** (critical band) = **`shadowfiend`** (medium mana) |
| **21** | **`flash heal on party`** (low band) = **`inner focus`** = **`power word: shield`** (being attacked) |
| **20** | **`hymn of hope`** = **`power word: shield`** (low health) = **`reach spell`** |
| 17 / 16 / 14 | `circle of healing on party` / `prayer of mending on party` / `flash heal on party` (medium band) |
| 12 / 11 | `renew on party` / `prayer of mending on party` (almost full band) |
| 10 | `power word: shield` (self critical health) |
| 5.5 – 5.0 | `healer dps`, same as Disc |

`set facing` @37 tying with `divine hymn` is the collision the earlier audit missed.

---

## Shadow — `dps` + `shadow debuff` + `shadow aoe`, `ShadowPriestStrategy.cpp`

Guide priority: Vampiric Embrace → maintain Vampiric Touch → maintain Devouring Plague → maintain
Shadow Word: Pain (Mind Flay refreshes it) → Mind Blast → Shadow Word: Death **only if the target
dies before a Mind Blast or Mind Flay finishes** → Mind Flay, clipped after the second tick when
something higher is ready. Cooldowns: Shadowfiend before Bloodlust, Dispersion for mana or big
damage, Inner Focus on cooldown with Mind Flay.

- **S1. Shadow Word: Death is wired as a filler, not an execute.** `ShadowPriestStrategy.cpp:22` puts
  it in `getDefaultActions()` @5.1, ungated, with a comment claiming it is for movement. On a boss
  the guide's condition is never met, so the bot takes backlash damage every cooldown for nothing,
  with no self-health floor anywhere. The time-to-die idiom already exists and is reusable:
  `target->GetHealth() / AI_VALUE(float, "estimated group dps")` in `DebuffTrigger::IsActive`
  (`src/Ai/Base/Trigger/GenericTriggers.cpp:311-319`) and `TargetWithComboPointsLowerHealTrigger::IsActive`
  (`GenericTriggers.cpp:100-108`, decl `GenericTriggers.h:118-129`).
- **S2. Vampiric Embrace, the guide's #1, exists only out of combat**
  (`PriestNonCombatStrategy.cpp:23-24` @16). If it falls off mid-fight it is never reapplied. Trigger
  and action are both already registered (`PriestAiObjectContext.cpp:90/111` and `:202/250`) — pure
  wiring.
- **S3. Do NOT add DoT refresh windows.** VT, DP and SW:P are declared through
  `DEBUFF_CHECKISOWNER_TRIGGER` (`src/Bot/Engine/AiObject.h:86-92`), which constructs
  `DebuffTrigger(botAI, spell, 1, true)` and therefore leaves `beforeDuration` at its default 0
  (`GenericTriggers.h:401-413`, `BuffTrigger` at `:327-347`). The guide is explicit — "Let it expire
  before you re-apply it" for both VT and DP. This is **correct as written**.

  Guardrail for whoever executes the plan: rogue sets the opposite value on purpose
  (`src/Ai/Class/Rogue/RogueTriggers.h:23, 32, 58` — `beforeDuration = 2000` on Slice and Dice,
  Hunger for Blood and Rupture), which is right for rogue, where those effects are refreshed before
  they drop. Do not "make priest consistent" with it. The two specs want opposite behaviour from the
  same parameter, and copying the rogue value here would introduce a Shadow DPS regression that does
  not exist today.
- **S4. Shadowfiend only fires below 40% mana** via the inherited generic node, and the `boost` node
  is dead — see X1, X3, X6.
- **S5. Dispersion has two nodes at identical relevance 25** — `low mana` and `critical health`
  (`ShadowPriestStrategy.cpp:39-54`) — and both tie with the inherited `desperate prayer` @25 and
  with `cancel channel` @25 from `shadow aoe` (`:91-98`). Four-way, resolved by insertion order.
- **S6. `shadowform` @20** collides with `hymn of hope` @20, `power word: shield` (low health) @20 and
  `reach spell` @20.
- **S7. No Mind Flay clipping.** The guide wants the channel interrupted after the second tick when a
  higher-priority spell is up. The precedent exists for Mind Sear — `MindSearChannelCheckTrigger`
  (`PriestTriggers.h:102-113`, body `PriestTriggers.cpp:50-72`) plus the `cancel channel` action — and
  nothing equivalent exists for Mind Flay.
- **S8. Shadow drops Shadowform for inherited Holy-school spells.** `ShadowPriestStrategy` derives
  from `GenericPriestStrategy`, so its ctor runs the base ctor first and the bot carries **both**
  `GenericPriestStrategyActionNodeFactory` and `ShadowPriestStrategyActionNodeFactory`. Four
  inherited nodes (`GenericPriestStrategy.cpp:22-44`) are Holy-school and unusable in Shadowform:

  | Trigger | Action | Rel | Has `remove shadowform` prereq? |
  |---|---|---|---|
  | `critical health` | `desperate prayer` | 25 | No — fails silently |
  | `low mana` | `hymn of hope` | 20 | No — fails silently |
  | `low health` | `power word: shield` (self) | 20 | Yes (`GenericPriestStrategyActionNodeFactory.h:93-102`) |
  | `being attacked` | `power word: shield` (self) | 21 | Yes |

  The two without a prereq burn an action slot on a guaranteed failure. The two with one are worse:
  `being attacked` fires on any attacker, so on routine add damage the bot spends a GCD leaving
  Shadowform, a GCD shielding and a GCD re-entering, losing the damage aura in between.

  **User decision:** Shadow keeps only `critical health` → self PW:S as an emergency button. Desperate
  Prayer and Hymn of Hope come out of the Shadow ladder entirely. Shadow's mana tools stay Dispersion
  (`low mana`, castable in Shadowform) and Shadowfiend. Divine Hymn is unaffected — it appears only in
  the Disc and Holy ladders, which Shadow never loads.
- **S9. Not a finding, checked and cleared.** `power word: shield on party` is healer-only — it
  appears in `HealPriestStrategy.cpp` and `HolyPriestStrategy.cpp:121`, never in
  `ShadowPriestStrategy`. Its action-node creator (`GenericPriestStrategyActionNodeFactory.h:103-111`)
  is defined but never added to `creators[]` (the list ends at `:35`), so it has no
  `remove shadowform` prerequisite — harmless, because no spec that reaches the node is ever in
  Shadowform. Recorded so the same dead-looking code is not re-investigated later.

### S10. Shadow relevance map (current)

| Rel | Nodes |
|---|---|
| 99 / 60 / 55 / 54 | `drop target` / `set pet stance` / `fade` / `check mount state` |
| **41** | **`silence`** = **`dispel magic`** = **`power infusion`** (last two inherited, PI dead) |
| **40** | **`silence on enemy healer`** = **`dispel magic on party`** |
| 39 / 37 | `flee` / `set facing` |
| 31 / 30 | `abolish disease` / `abolish disease on party` |
| **25** | **`dispersion`** (low mana) = **`dispersion`** (critical health) = **`desperate prayer`** = **`cancel channel`** (mind sear channel check) |
| 24 / 23 | `mind sear` (medium aoe) / `vampiric touch` |
| **22** | **`devouring plague`** = **`shadowfiend`** (medium mana) |
| **21** | **`shadow word: pain`** = **`power word: shield`** (being attacked) = **`inner focus`** |
| **20** | **`shadowform`** = **`hymn of hope`** = **`power word: shield`** (low health) = **`reach spell`** |
| 15 / 14 | `shadow word: pain on attacker` / `vampiric touch on attacker` |
| 10 | `power word: shield` (self critical health) |
| 5.3 / 5.2 / 5.1 / 5.0 | `mind blast` / `mind flay` / `shadow word: death` / `shoot` (defaults) |

Shadow does not get `save mana` or `healer dps` — `PlayerbotAI::IsHeal` is false for it
(`AiFactory.cpp:411-416`).

---

## Cooldown audit

| Cooldown | Implemented | Reachable in raid | Trigger | Rel | Verdict |
|---|---|---|---|---|---|
| **Hymn of Hope** | Yes — action `PriestActions.h:192-198`, registered `PriestAiObjectContext.cpp:235, 337` | Yes, but far too late | `low mana` (< 15%) | 20 | X5. An 8s channel started at 15% mana. Four-way tie at 20. Also inherited by Shadow, where it cannot be cast at all (S8). |
| **Divine Hymn** | Yes — `PriestActions.h:200-206`, registered `:236, 338` | Yes | `medium group heal setting` | 37 | Correct trigger. Ties with `set facing` @37 in Holy. Not present for Shadow, which is correct. |
| **Power Infusion** | Yes — `PriestActions.h:57`, registered `:157, 247` | **No** | `power infusion` (`BoostTrigger`) | 41 | X2. Unreachable in PvE. Node also ties with `dispel magic` @41. |
| **Power Infusion (party)** | Yes — `PriestActions.h:58`, registered `:207, 325` | **No** | — | — | X7. Zero trigger nodes. |
| **Pain Suppression** | Yes — `PriestActions.h:61`, registered `:230, 329` | Yes | `critical health` | 91 | Correct. D6. |
| **Pain Suppression (party)** | Yes — `PriestActions.h:62`, registered `:231, 328` | Yes | `protect party member` | 90 | Correct, and only recently reachable at all. D6. |
| **Shadowfiend** | Yes — `PriestActions.h:208-214`, registered `:228, 331` | Yes, but late | `medium mana` (< 40%) | 22 | X6. Ties with `penance`/`greater heal` @22. `GetTargetName()` is `"current target"`, so a target-less healer cannot cast it. |
| **Inner Focus** | Yes — `PriestActions.h:59`, registered `:158, 248` | Yes, wrong trigger | `medium mana` | 21 | X4. Purpose-built `InnerFocusTrigger` is registered and dead, and has no cooldown check. |
| **Guardian Spirit** | Yes — `PriestActions.h:248-255`, registered `:238, 340` | Yes (Holy only) | `party member critical health` | 36 | Right trigger. Ties with `prayer of mending on party` @36. `40.0f, MEDIUM` metadata makes the mana multiplier veto it between 60% and 65% target HP — irrelevant in the critical band, but should be explicit. |
| **Penance** | Yes — `PriestActions.h:184-190`, registered `:234, 336` | Yes (Disc only) | all four party bands | 34/22/16 | D7. Priced below PW:S everywhere; `estAmount = 25` vetoes it above 75% target HP under mana pressure. |
| **Dispersion** | Yes — `PriestActions.h:176-182`, registered `:203, 249` | Yes (Shadow only) | `low mana` and `critical health` | 25 / 25 | S5. Identical relevance on two nodes, plus two more nodes at 25. |
| **Desperate Prayer** | Yes — `PriestActions.h:155`, registered `:218, 314` | Yes | `critical health` | 25 | Fine for the healing specs. Silently impossible for Shadow (S8). |
| **Fade** | Yes — `PriestActions.h:126`, registered `:195, 306` | Yes | `medium threat` | 55 | Correct, no collision. |
| **Symbol of Hope** | Yes — `PriestActions.h:163`, registered `:224, 335` | **No** | — | — | X7. Zero trigger nodes. Draenei racial. |
| **Lightwell** | Yes — `PriestActions.h:97`, registered `:213, 319` | **No** | — | — | X7. Zero trigger nodes. |

---

## Confirmed correct, not gaps

- Shadow DoT expiry behaviour (S3) — `beforeDuration = 0` is deliberate for priest.
- The missing `remove shadowform` prereq on `power word: shield on party` (S9).
- Circle of Healing absent from Disc — it is a Holy talent.
- Pain Suppression absent from Holy and Shadow, Penance absent from Holy — talent placement.
- Shadowfiend's exemption from the boss-only burst hold (`BurstWindowStrategy.cpp:48`) is intentional
  and must survive any rebinding.
- `mind sear channel check` cancelling when attackers drop below 2 (`PriestTriggers.cpp:55-72`).
- The shared out-of-combat ladder in `PriestNonCombatStrategy.cpp`.
- `HolyPriestStrategy` inheriting the Disc ladder (H6) — reachable only when ungrouped.

---

## Documented limitations, deliberately out of scope

- `PartyMemberToProtect` keys on `attacker->GetVictim()`, so a raider dying to raid-wide AoE, a DoT or
  a ground effect is nobody's current victim and can never be selected. Pain Suppression therefore
  cannot answer that damage pattern, which is a large share of raid PvE. Widening the value would move
  `blessing of protection on party` (Paladin) and `intervene` (Warrior) too, which is outside the
  chosen blast radius.
- **Surge of Light and Serendipity are not modelled.** Both are Holy procs the guide's rotation leans
  on, and there is no aura-tracking trigger for either. Adding them means a new `HasAuraTrigger`
  subclass per proc plus band-specific nodes to spend them; recorded as a follow-up rather than folded
  into this pass.
- **Borrowed Time is not modelled** for Disc, for the same reason — the Prayer of Healing pairing in
  D5 is approximated by the raid-damage trigger, not by the buff.

## Open items requiring in-game confirmation

Spell school and Shadowform castability are not derivable from this repo — that data lives in DBC.
Desperate Prayer and Hymn of Hope were settled by the user (both blocked in Shadowform). The one new
item this audit adds: whether the Mind Flay channel exposes remaining duration through an aura on the
target, which is what the proposed clip trigger reads (see the plan, section B). If it does not, the
trigger has to fall back to the hardcoded-spell-id shape `MindSearChannelCheckTrigger` uses.
