# Priest rotation overhaul — implementation plan (all three specs)

## Scope and premises

This is the executable spec for the audit in `docs/classes/priest-all-specs-findings.md`. It is
self-contained: a fresh session can execute it without that document or the originating conversation,
though the findings doc explains *why* each item is here.

Paths are relative to `modules/mod-playerbots/`. All `file:line` references were verified against the
`Custom` branch.

Settled decisions, not open for re-litigation:

- **Discipline gets no Greater Heal.** Fix Flash Heal's mana-veto metadata instead.
- **Shadow keeps only `critical health` → self Power Word: Shield.** Desperate Prayer and Hymn of Hope
  leave the Shadow ladder — neither is castable in Shadowform.
- **Blast radius**: the priest folder plus one additive shared change. `HealerAutoSaveManaMultiplier`
  itself is not retuned; that would move every healer class.
- **Surge of Light, Serendipity and Borrowed Time are not modelled.** No proc-aura triggers exist and
  adding them is a separate change.

Naming note that matters: a missing `creators[...]` entry in `PriestAiObjectContext.cpp` is a **silent
runtime no-op, not a compile error**. Every new action and trigger name below must be registered — see
section G.

### Engine facts the numbers depend on

- Relevance is **global across all active trigger nodes**; ties break by insertion order
  (`src/Bot/Engine/Engine.cpp:166-245`). Every number in this document competes with every other
  number the same bot has loaded.
- Party health bands are **nested** — all pass `minValue = 0`
  (`src/Ai/Base/Trigger/HealthTriggers.h:88-128`). A target at 20% fires all four bands at once.
- `HealerAutoSaveManaMultiplier` (`src/Ai/Base/Strategy/ConserveManaStrategy.cpp:93-131`): below 60%
  bot mana, a `CastHealingSpellAction` is zeroed when target HP ≥ 65 and
  (`100 - targetHP < estAmount` or efficiency ≤ MEDIUM), and when target HP ≥ 45 and
  (`100 - targetHP < estAmount` or efficiency ≤ LOW). Tanks get `estAmount / 1.5`. Efficiency ranks
  (`src/PlayerbotAIConfig.h:34-42`): VERY_LOW 1, LOW 2, MEDIUM 4, HIGH 8, VERY_HIGH 16, SUPERIOR 32.
- Config defaults (`src/PlayerbotAIConfig.cpp:95-117`): critical 25, low 45, medium 65,
  almostFull 85, lowMana 15, mediumMana 40, highMana 65, saveManaThreshold 60, healDistance 38.5.

### Relevance values already taken

The ladders below must not reuse these. Inherited by **every** priest:

| Rel | Node | Source |
|---|---|---|
| 99 | `drop target` | `src/Ai/Base/Strategy/CombatStrategy.cpp:22-29` |
| 60 | `set pet stance` | `GenericPriestStrategy.cpp:45` |
| 55 | `fade` | `GenericPriestStrategy.cpp:22` |
| 54 | `check mount state` | `CombatStrategy.cpp:30-36` |
| 41 / 40 | `dispel magic` / `dispel magic on party` | `PriestCureStrategy`, `GenericPriestStrategy.cpp:55-58` |
| 39 | `flee` | `GenericPriestStrategy.cpp:40-41` |
| 37 | `set facing` | `CombatStrategy.cpp:45-51` |
| 31 / 30 | `abolish disease` / `abolish disease on party` | `GenericPriestStrategy.cpp:59-62` |
| 20 | `reach spell` | `CombatStrategy.cpp:13-20` |
| 1 | `apply oil`, `reset` | `GenericPriestStrategy.cpp:42`, `CombatStrategy.cpp:37-44` |

Healers additionally run `healer dps` (`GenericPriestStrategy.cpp:78-93`) at 5.0–5.5.

---

## A. `src/Ai/Class/Priest/PriestActions.h` / `.cpp`

### A1. Mana-efficiency metadata

Read only by `HealerAutoSaveManaMultiplier`, compared against `lossAmount = 100 - targetHealthPct`.

| Action | Line | Before | After | Why |
|---|---|---|---|---|
| `CastFlashHealOnPartyAction` | `:74` | `15.0f, LOW` | `15.0f, HIGH` | LOW is vetoed for any target above 45% HP under mana pressure, which takes Disc's only direct heal offline exactly when it is needed. HIGH leaves only the `lossAmount < 15` clause, i.e. it still stops topping off targets above 85%. |
| `CastPenanceOnPartyAction` | `:184-190` | `25.0f, HIGH` | `15.0f, HIGH` | `estAmount = 25` vetoed the spec's signature heal above 75% target HP. Penance heals nowhere near 25% of a raider's pool. |
| `CastGuardianSpiritOnPartyAction` | `:248-255` | `40.0f, MEDIUM` | `15.0f, SUPERIOR` | It is a death-prevention cooldown, not a heal. SUPERIOR means the efficiency clause never vetoes it. |
| `CastPainSuppressionProtectAction` | `:62` | macro defaults | leave as is | `PROTECT_ACTION` builds a `CastProtectSpellAction`, not a `CastHealingSpellAction`, so the multiplier never sees it. Confirmed, not a gap. |
| `CastGreaterHealOnPartyAction` | `:72` | `50.0f, MEDIUM` | `25.0f, MEDIUM` | Holy's big heal. 50 meant it needed the target to have lost half its health before it was allowed to cast. |
| `CastBindingHealAction` | `:86` | `15.0f, MEDIUM` | `15.0f, HIGH` | It heals the priest *and* the target; MEDIUM vetoed it above 65% target HP, which is most of when a priest wants it. |
| `CastCircleOfHealingAction` | `:89-96` | `15.0f, HIGH` | unchanged | Already correct. |
| `CastPowerWordShieldOnPartyAction` | `:73` | `15.0f, VERY_HIGH` | unchanged | Already correct. |
| `CastRenewOnPartyAction` | `:75` | `15.0f, VERY_HIGH` | unchanged | Already correct. |
| `CastPrayerOfHealingAction` | `:87` | `15.0f, MEDIUM` | `15.0f, HIGH` | MEDIUM vetoes it above 65% target HP, but the whole point of the spell is healing a group that is at 60–70%. |

### A2. Weakened Soul guards

`power word: shield` cannot land on a target that still has Weakened Soul. The two custom variants
already check it (`PriestActions.cpp:40, 69, 95`); the two plain ones do not, so they re-attempt a
guaranteed failure every tick.

- Replace the `BUFF_ACTION(CastPowerWordShieldAction, "power word: shield")` macro use
  (`PriestActions.h:30`) with an explicit `CastBuffSpellAction` subclass overriding `isUseful()`:
  base check plus `!botAI->HasAura("weakened soul", GetTarget())`.
- Replace the `HEAL_PARTY_ACTION(CastPowerWordShieldOnPartyAction, …)` macro use
  (`PriestActions.h:73`) with an explicit `HealPartyMemberAction` subclass, same
  `15.0f, VERY_HIGH` metadata, overriding `isUseful()` the same way.

Both need out-of-line bodies in `PriestActions.cpp`. No context change — the factory constructs by
class name (`PriestAiObjectContext.cpp:279-283`).

### A3. Shadowfiend targeting

`CastShadowfiendAction::GetTargetName()` returns `"current target"` (`PriestActions.h:208-214`), so a
healer with no current target cannot cast it. Override `GetTarget()` instead:

```cpp
Unit* CastShadowfiendAction::GetTarget()
{
    if (Unit* target = AI_VALUE(Unit*, "current target"))
        return target;

    return AI_VALUE(Unit*, "grind target");
}
```

`"grind target"` is registered in `src/Ai/Base/ValueContext.h:151`.

### A4. New actions

- `CastRenewOnMainTankAction : BuffOnMainTankAction` — `BuffOnMainTankAction(botAI, "renew", true)`
  (`src/Ai/Base/Actions/GenericSpellActions.h:489-498`). `checkIsOwner = true` so another priest's
  Renew does not satisfy it. Name resolves through `MainTankActionNameSupport` to
  `"renew on main tank"`.

No other new actions. Everything else the ladders use is already registered.

---

## B. `src/Ai/Class/Priest/PriestTriggers.h` / `.cpp`

### B1. Rewire two existing triggers off `BoostTrigger` / `BuffTrigger`

Both are currently unusable: `BoostTrigger::IsActive` needs a Player target or `balance ≤ 50`
(`src/Ai/Base/Trigger/GenericTriggers.cpp:422-432`), and `BuffTrigger::IsActive` is only "the aura is
missing" with no cooldown check (`GenericTriggers.cpp:193-204`).

```cpp
class InnerFocusTrigger : public SpellNoCooldownTrigger
{
public:
    InnerFocusTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "inner focus") {}

    bool IsActive() override;
};

class ShadowfiendTrigger : public SpellNoCooldownTrigger
{
public:
    ShadowfiendTrigger(PlayerbotAI* botAI) : SpellNoCooldownTrigger(botAI, "shadowfiend") {}
};
```

`InnerFocusTrigger::IsActive()` = `SpellNoCooldownTrigger::IsActive() && !botAI->HasAura("inner focus", bot)`
— without the aura check the bot would re-cast an already-active Inner Focus. `ShadowfiendTrigger`
needs no override; `SpellNoCooldownTrigger` (`GenericTriggers.h:197-203`) already resolves the spell
id through the `"spell id"` value and checks `HasSpellCooldown`, which subsumes the current hardcoded
`34433`.

Replace the `BOOST_TRIGGER_A(ShadowfiendTrigger, "shadowfiend")` declaration (`PriestTriggers.h:44`)
and delete `bool ShadowfiendTrigger::IsActive()` (`PriestTriggers.cpp:37`).

### B2. Delete `PowerInfusionTrigger`

`BOOST_TRIGGER(PowerInfusionTrigger, "power infusion")` (`PriestTriggers.h:29`) cannot activate in
raid PvE. Power Infusion moves onto the raid-damage trigger in the Disc ladder (section C2), so no
replacement trigger class is needed. Delete the class and its registration
(`PriestAiObjectContext.cpp:91, 133`).

### B3. New triggers

```cpp
class WeakenedSoulOnPartyMemberTrigger : public Trigger
{
public:
    WeakenedSoulOnPartyMemberTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "weakened soul on party member") {}

    std::string const GetTargetName() override { return "party member to heal"; }
    bool IsActive() override;
};
```

`IsActive()`: target from `GetTarget()`, non-null, `target->GetHealthPct() < sPlayerbotAIConfig.almostFullHealth`,
and `botAI->HasAura("weakened soul", target)`. Same target value the Flash Heal action uses
(`HealPartyMemberAction::GetTargetName()` → `"party member to heal"`), so trigger and action agree.

```cpp
class PriestHymnOfHopeTrigger : public Trigger
{
public:
    PriestHymnOfHopeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "hymn of hope") {}

    bool IsActive() override;
};
```

`IsActive()`: `AI_VALUE2(bool, "has mana", "self target")`, `AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.mediumMana`,
and `AI_VALUE2(uint8, "aoe heal", "critical") == 0`. The last clause is what makes it safe: an
8-second channel must not start while somebody is below 25% HP. `"aoe heal"` counts alive group
members below the qualifier's health band, within `healDistance` and in LOS
(`src/Ai/Base/Value/AoeHealValues.cpp:12-52`).

```cpp
class RenewOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    RenewOnMainTankTrigger(PlayerbotAI* botAI)
        : BuffOnMainTankTrigger(botAI, "renew", true) {}
};
```

`BuffOnMainTankTrigger` (`GenericTriggers.h:961-969`) resolves through
`GetTargetValue() → context->GetValue<Unit*>("main tank", spell)` (`GenericTriggers.cpp:770`).
`checkIsOwner = true` matches the action.

```cpp
class PriestShadowWordDeathExecuteTrigger : public Trigger
{
public:
    PriestShadowWordDeathExecuteTrigger(PlayerbotAI* botAI, float lifeTime = 2.0f)
        : Trigger(botAI, "shadow word: death execute"), lifeTime(lifeTime) {}

    std::string const GetTargetName() override { return "current target"; }
    bool IsActive() override;

protected:
    float lifeTime;
};
```

`IsActive()`: target alive and in world, bot health above `sPlayerbotAIConfig.mediumHealth`, and
`(target->GetHealth() / AI_VALUE(float, "estimated group dps")) <= lifeTime`. The time-to-die idiom is
copied from `TargetWithComboPointsLowerHealTrigger::IsActive` (`GenericTriggers.cpp:100-108`);
`"estimated group dps"` is registered at `src/Ai/Base/ValueContext.h:324`. `lifeTime = 2.0f` is the
guide's condition — the target dies before a Mind Blast or Mind Flay would finish. The bot-health
floor exists because Shadow Word: Death deals its damage back to the caster when the target survives.

```cpp
class MindFlayChannelCheckTrigger : public Trigger
{
public:
    MindFlayChannelCheckTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "mind flay channel check") {}

    std::string const GetTargetName() override { return "current target"; }
    bool IsActive() override;
};
```

`IsActive()`:

1. `Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL)`; return false if null.
2. Return false unless `spell->m_spellInfo->Id == AI_VALUE2(uint32, "spell id", "mind flay")`.
   Resolving through the `"spell id"` value avoids hardcoding the nine WotLK Mind Flay ranks; this is
   the same lookup `SpellNoCooldownTrigger::IsActive` uses (`GenericTriggers.cpp:357-364`).
3. Only clip once the channel is mostly spent: `Aura* aura = botAI->GetAura("mind flay", GetTarget(), true)`;
   return false unless `aura && aura->GetDuration() <= aura->GetMaxDuration() / 3` (two of the three
   ticks landed).
4. Return true only if something better is actually ready — any of: `mind blast` off cooldown
   (`!bot->HasSpellCooldown(AI_VALUE2(uint32, "spell id", "mind blast"))`), or `vampiric touch`,
   `devouring plague` or `shadow word: pain` missing from the target
   (`!botAI->HasAura(<spell>, target, true)`).

Step 3 is the one implementation detail that could not be verified statically — see "Open items"
below. If the channel does not expose remaining duration through a target aura, fall back to the
hardcoded-spell-id shape `MindSearChannelCheckTrigger` uses (`PriestTriggers.cpp:50-72`) and drop the
tick gate, accepting that the clip may happen a tick early.

---

## C. The ladders — `src/Ai/Class/Priest/Strategy/`

### C1. `GenericPriestStrategy.cpp` — relocate the Holy-school nodes out of the base

A derived `InitTriggers` cannot remove a base node, so relocation is the only mechanism. Same pattern
as `docs/classes/holy-paladin-improvements-plan.md:206-212`.

`GenericPriestStrategy::InitTriggers` keeps only:

```cpp
CombatStrategy::InitTriggers(triggers);

triggers.push_back(new TriggerNode("medium threat", { NextAction("fade", 55.0f) }));
triggers.push_back(new TriggerNode("critical health",
    { NextAction("power word: shield", ACTION_NORMAL) }));
triggers.push_back(new TriggerNode("enemy too close for spell",
    { NextAction("flee", ACTION_MOVE + 9) }));
triggers.push_back(new TriggerNode("often", { NextAction("apply oil", 1.0f) }));
triggers.push_back(new TriggerNode("new pet", { NextAction("set pet stance", 60.0f) }));
```

Deleted from the base — `critical health` → `desperate prayer` @25, `low health` → self
`power word: shield` @20, `being attacked` → self `power word: shield` @21, `medium mana` →
`shadowfiend` @22 + `inner focus` @21, `low mana` → `hymn of hope` @20. All six reappear, repriced,
in the two healing ladders. Shadow gets its own Shadowfiend and Inner Focus nodes; it does not get
Desperate Prayer or Hymn of Hope at all.

`PriestBoostStrategy::InitTriggers` (`GenericPriestStrategy.cpp:65-70`) — delete both nodes. The
`"boost"` trigger does not exist (silent no-op) and `power infusion` moves into the Disc ladder. Leave
the method with an empty body plus a one-line comment saying why: `AiFactory.cpp:292` adds the
`"boost"` strategy to every bot, so the class has to stay.

`PriestHealerDpsStrategy::InitTriggers` (`:78-93`) — change `mind sear` from `ACTION_DEFAULT + 0.5f`
to `ACTION_DEFAULT + 0.6f`; it currently ties with `shadow word: pain` at 5.5.

### C2. `HealPriestStrategy.cpp` — full Discipline ladder

`getDefaultActions()` unchanged (`shoot` @5.0).

| Trigger | Action | Rel | Note |
|---|---|---|---|
| `critical health` | `pain suppression` | 91 | unchanged |
| `protect party member` | `pain suppression on party` | 90 | unchanged |
| `party member to heal out of spell range` | `reach party member to heal` | **38** | 40 → 38; clears `dispel magic on party` @40, `flee` @39 and `set facing` @37 |
| `medium group heal setting` | `divine hymn` | **36.8** | 37 → 36.8; clears `set facing` @37 |
| `medium group heal setting` | `power infusion` | **36.4** | **new binding** — replaces the unreachable `BoostTrigger` node |
| `party member critical health` | `power word: shield on party` | 36 | |
| `party member critical health` | `penance on party` | **35.5** | raised above Prayer of Mending — guide #3 |
| `party member critical health` | `prayer of mending on party` | 35 | |
| `party member critical health` | `flash heal on party` | 34.5 | |
| `binding heal` | `binding heal` | **34** | **new node** — trigger and action already exist and were dead |
| `medium group heal setting` | `inner focus` | **33.7** | **new binding** — sits directly above Prayer of Healing so the free cast is spent on it |
| `medium group heal setting` | `prayer of healing on party` | 33.5 | |
| `medium group heal setting` | `power word: shield on not full` | 33 | |
| `group heal setting` | `prayer of mending on party` | 32.5 | |
| `group heal setting` | `power word: shield on not full` | 32 | |
| `critical health` | `desperate prayer` | **28** | relocated from the base @25 |
| `low health` | `power word: shield` (self) | **27.5** | relocated from the base @20 |
| `weakened soul on party member` | `flash heal on party` | **27** | **new node** — the guide's "Flash Heal because the target has Weakened Soul" branch |
| `party member low health` | `power word: shield on party` | 26 | |
| `party member low health` | `penance on party` | **25.5** | |
| `party member low health` | `prayer of mending on party` | 25 | |
| `party member low health` | `flash heal on party` | 24.5 | |
| `renew on main tank` | `renew on main tank` | **24** | **new node** — guide #2 |
| `being attacked` | `power word: shield` (self) | **23.5** | relocated from the base @21, now below the low band |
| `shadowfiend` | `shadowfiend` | **23** | **rebound** off `medium mana` |
| `hymn of hope` | `hymn of hope` | **22** | **rebound** off `low mana` (15%) onto the new gated trigger |
| `party member medium health` | `power word: shield on party` | 19 | |
| `party member medium health` | `penance on party` | **18.5** | |
| `party member medium health` | `prayer of mending on party` | 18 | |
| `party member medium health` | `flash heal on party` | 17.5 | |
| `party member almost full health` | `power word: shield on party` | 13 | |
| `party member almost full health` | `prayer of mending on party` | 12 | |
| `party member almost full health` | `renew on party` | 11 | |

Plus the base `critical health` → self `power word: shield` @10.

### C3. `HolyPriestStrategy.cpp` — full Holy ladder (`HolyHealPriestStrategy`)

`getDefaultActions()` unchanged (`shoot` @5.0). Holy has no Penance, no Pain Suppression and no Power
Infusion — that is correct, not a gap.

| Trigger | Action | Rel | Note |
|---|---|---|---|
| `party member to heal out of spell range` | `reach party member to heal` | **38** | 40 → 38 |
| `medium group heal setting` | `divine hymn` | **36.8** | 37 → 36.8 |
| `party member critical health` | `guardian spirit on party` | **36.2** | 36 → 36.2; clears Prayer of Mending |
| `party member critical health` | `power word: shield on party` | 36 | |
| `party member critical health` | `circle of healing on party` | **35.5** | **new** — the spec's AoE answer was absent from the critical band |
| `party member critical health` | `prayer of mending on party` | **35** | 33 → 35 |
| `party member critical health` | `flash heal on party` | **34.5** | 31 → 34.5 |
| `party member critical health` | `greater heal on party` | **34.2** | 22 → 34.2; it could never fire in its own band |
| `binding heal` | `binding heal` | **34** | **new node** — Holy needs it for Serendipity |
| `medium group heal setting` | `inner focus` | **33.7** | **new binding** |
| `medium group heal setting` | `prayer of healing on party` | 33.5 | 34 → 33.5 |
| `medium group heal setting` | `circle of healing on party` | **33** | 35 → 33 |
| `group heal setting` | `prayer of mending on party` | **32.5** | 29 → 32.5 |
| `group heal setting` | `circle of healing on party` | **32** | 28 → 32 |
| `critical health` | `desperate prayer` | **28** | relocated from the base @25 |
| `low health` | `power word: shield` (self) | **27.5** | relocated from the base @20 |
| `party member low health` | `circle of healing on party` | **26** | 24 → 26 |
| `party member low health` | `prayer of mending on party` | **25.5** | 23 → 25.5 |
| `party member low health` | `flash heal on party` | **25** | 21 → 25 |
| `party member low health` | `greater heal on party` | **24.5** | 22 → 24.5 |
| `renew on main tank` | `renew on main tank` | **24** | **new node** — guide #1 |
| `being attacked` | `power word: shield` (self) | **23.5** | relocated from the base @21 |
| `shadowfiend` | `shadowfiend` | **23** | **rebound** off `medium mana` |
| `hymn of hope` | `hymn of hope` | **22** | **rebound** off `low mana` |
| `party member medium health` | `circle of healing on party` | **19** | 17 → 19 |
| `party member medium health` | `prayer of mending on party` | **18.5** | 16 → 18.5 |
| `party member medium health` | `greater heal on party` | **18** | 25 → 18; it outranked the entire low band |
| `party member medium health` | `renew on party` | **17.5** | **new** — Renew as the filler, guide #9 |
| `party member almost full health` | `prayer of mending on party` | **13** | 11 → 13 |
| `party member almost full health` | `renew on party` | **12** | |

Plus the base `critical health` → self `power word: shield` @10.

`HolyPriestStrategy` (`:28-78`) is a different class — the ungrouped `holy dps` layer — and stays
exactly as it is, including its `holy fire` / `shadowfiend` / `medium mana` / `low mana` nodes. H6 in
the findings (`HolyPriestStrategy : HealPriestStrategy` handing `holy dps` the Discipline ladder) is
deferred, not fixed here: it is only reachable when the bot is ungrouped.

One side effect to be aware of: `HolyPriestStrategy`'s `shadowfiend` node (`:54-61`, @20) is dead
today because `ShadowfiendTrigger` is boost-gated, and section B1 makes it live. It then ties with the
inherited `reach spell` @20 and with its own `medium mana` → `shadowfiend` @20 node (`:62-69`). Both
only affect an ungrouped Holy priest. If you want it clean while you are in the file, delete the
`medium mana` node and move the `shadowfiend` one to 20.5.

### C4. `ShadowPriestStrategy.cpp` — full Shadow ladder

`getDefaultActions()` — **drop `shadow word: death`**. It becomes a gated execute (see below), so the
ungated 5.1 entry and its "cast during movement" comment go away:

```cpp
return {
    NextAction("mind blast", ACTION_DEFAULT + 0.3f),
    NextAction("mind flay", ACTION_DEFAULT + 0.2f),
    NextAction("shoot", ACTION_DEFAULT)
};
```

`ShadowPriestStrategy::InitTriggers`:

| Trigger | Action | Rel | Note |
|---|---|---|---|
| `silence` | `silence` | **42** | `ACTION_INTERRUPT + 2`; 41 → 42 clears `dispel magic` @41 |
| `silence on enemy healer` | `silence on enemy healer` | **41.5** | `ACTION_INTERRUPT + 1.5`; clears `dispel magic on party` @40 |
| `critical health` | `dispersion` | **25.5** | split off the shared 25 |
| `low mana` | `dispersion` | 25 | |
| `vampiric embrace` | `vampiric embrace` | **21.5** | **new node** — the guide's #1, previously out-of-combat only |
| `shadowfiend` | `shadowfiend` | **21** | **new node**, on the rebound cooldown trigger |
| `shadowform` | `shadowform` | **20.5** | 20 → 20.5 clears `reach spell` @20 |
| `inner focus` | `inner focus` | **19** | **new node**, on the rebound cooldown trigger |
| `shadow word: death execute` | `shadow word: death` | **12** | **new node** — replaces the ungated default action |

`ShadowPriestAoeStrategy::InitTriggers`:

| Trigger | Action | Rel | Note |
|---|---|---|---|
| `mind sear channel check` | `cancel channel` | **24.5** | 25 → 24.5 |
| `mind flay channel check` | `cancel channel` | **24.4** | **new node** |
| `medium aoe` | `mind sear` | 24 | |
| `shadow word: pain on attacker` | `shadow word: pain on attacker` | 15 | |
| `vampiric touch on attacker` | `vampiric touch on attacker` | 14 | |

`ShadowPriestDebuffStrategy::InitTriggers`:

| Trigger | Action | Rel | Note |
|---|---|---|---|
| `vampiric touch` | `vampiric touch` | 23 | |
| `devouring plague` | `devouring plague` | **22.5** | 22 → 22.5; the old value tied with the inherited Shadowfiend node |
| `shadow word: pain` | `shadow word: pain` | 22 | 21 → 22 |

Plus the base `critical health` → self `power word: shield` @10 — the one emergency button Shadow
keeps.

**Do not touch the DoT triggers' `beforeDuration`.** `VampiricTouchTrigger`, `DevouringPlagueTrigger`
and `ShadowWordPainTrigger` are declared with `DEBUFF_CHECKISOWNER_TRIGGER`
(`src/Bot/Engine/AiObject.h:86-92`), leaving `beforeDuration` at 0, which means "re-apply only after
it expires". That matches the Shadow guide exactly. Rogue deliberately sets 2000 on its equivalents
(`src/Ai/Class/Rogue/RogueTriggers.h:23, 32, 58`) because rogue refreshes before the drop; copying
that here is a DPS regression.

---

## D. `PriestNonCombatStrategy.cpp`

No changes. `vampiric embrace` @16 stays as the out-of-combat pre-buff; the new in-combat node at
21.5 covers the mid-fight reapplication. The two never compete — different engines.

---

## E. Shared change (additive, one file)

`src/Ai/Base/Combat/BurstCooldowns.cpp:35-36` — add `"power infusion"` to the priest block next to
`"shadowfiend"`. This makes `HoldBurstUntilTankEngagedMultiplier` hold Power Infusion for the boss
instead of spending it on trash.

`src/Ai/Base/Strategy/BurstWindowStrategy.cpp:41-52` already exempts Shadowfiend by name. Power
Infusion is a genuine burst cooldown and wants the hold, so **no exemption is added** — but verify the
Shadowfiend exemption still reads `name == "shadowfiend"` after the change, because the Disc node now
fires far more often than it used to.

Blast radius: priest only. No other class has either name in the burst list.

---

## F. Files to modify

- `src/Ai/Class/Priest/PriestActions.h` / `.cpp` — metadata, Weakened Soul guards, Shadowfiend target,
  `CastRenewOnMainTankAction`
- `src/Ai/Class/Priest/PriestTriggers.h` / `.cpp` — `InnerFocusTrigger` and `ShadowfiendTrigger`
  rewrites, `PowerInfusionTrigger` deletion, five new triggers
- `src/Ai/Class/Priest/Strategy/GenericPriestStrategy.cpp` — base-node relocation,
  `PriestBoostStrategy` emptying, `mind sear` reprice
- `src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp` — full Disc ladder
- `src/Ai/Class/Priest/Strategy/HolyPriestStrategy.cpp` — full Holy ladder
  (`HolyHealPriestStrategy` only)
- `src/Ai/Class/Priest/Strategy/ShadowPriestStrategy.cpp` — all three Shadow strategies, default
  actions
- `src/Ai/Class/Priest/PriestAiObjectContext.cpp` — registration (section G)
- `src/Ai/Base/Combat/BurstCooldowns.cpp` — Power Infusion

No new files, so no CMake change.

---

## G. Registration checklist — `src/Ai/Class/Priest/PriestAiObjectContext.cpp`

Add to `PriestTriggerFactoryInternal` (`creators[…]` around `:75-107`, static factories `:111-149`):

- `"weakened soul on party member"` → `WeakenedSoulOnPartyMemberTrigger`
- `"hymn of hope"` → `PriestHymnOfHopeTrigger`
- `"renew on main tank"` → `RenewOnMainTankTrigger`
- `"shadow word: death execute"` → `PriestShadowWordDeathExecuteTrigger`
- `"mind flay channel check"` → `MindFlayChannelCheckTrigger`

Remove: `"power infusion"` trigger (`:91` and `:133`).

Keep unchanged (class bodies changed, names did not): `"inner focus"` (`:92`, `:134`),
`"shadowfiend"` (`:106`, `:144`), `"binding heal"` (`:102`, `:148`), `"vampiric embrace"` (`:90`,
`:111`).

Add to `PriestAiObjectContextInternal`:

- `"renew on main tank"` → `CastRenewOnMainTankAction`

Optionally remove the now-provably-dead action creators (`symbol of hope`, `lightwell`, `holy nova`,
`mass dispel`, `levitate`, `mind soothe`, `consume magic`, `elune's grace`,
`power word: shield on almost full health below`, `power infusion on party`). This is cleanup, not
required by any behaviour change — do it in a separate commit or not at all.

Also register `"power word: shield on party"` in `GenericPriestStrategyActionNodeFactory`
(`GenericPriestStrategyActionNodeFactory.h:16-36`): the creator exists at `:103-111` but was never
added to `creators[]`. Harmless today, because no spec that reaches that node is ever in Shadowform —
but the node is now used by both healing ladders in more bands, and leaving a defined-but-unwired
creator invites the same investigation again.

---

## H. Verification

The module cannot be compiled headless in this environment, so verification is static review plus an
in-game pass by the user.

### Static

1. Every action and trigger name string used in the three ladders has a matching `creators[...]` entry
   in `PriestAiObjectContext.cpp`, or is a shared name registered in `src/Ai/Base/TriggerContext.h` /
   `ActionContext.h`. Specifically check the shared ones the ladders rely on: `binding heal` (priest),
   `protect party member`, `group heal setting`, `medium group heal setting`, `party member to heal out of spell range`,
   the four party health bands, `critical health`, `low health`, `being attacked`, `medium aoe`,
   `cancel channel`.
2. No two `NextAction` entries reachable in the same tick share a relevance value. Check each ladder
   against the inherited table at the top of this document (99, 60, 55, 54, 41, 40, 39, 37, 31, 30, 20,
   1) and, for healers, against `healer dps` (5.0–5.6 after the `mind sear` reprice).
3. `PowerInfusionTrigger` has no remaining references after deletion — grep for
   `"power infusion"` as a *trigger* name.
4. `GenericPriestStrategy::InitTriggers` no longer contains `desperate prayer`, `hymn of hope`,
   `inner focus`, `shadowfiend` or the `low health` / `being attacked` shield nodes.
5. No cycle in the action-node graph. `flash heal on party` → `/*A*/ greater heal on party`
   (`GenericPriestStrategyActionNodeFactory.h:193-201`) stays as is; do not add the reverse.

### In-game

6. Build the module, start a server with one bot of each priest spec in a raid group.
7. `.playerbot debug` on the Disc priest, damage a party member through each band, and confirm the
   cast order: PW:S → Penance → Prayer of Mending → Flash Heal in the critical band; Penance and Flash
   Heal now appear in the low and medium bands too.
8. Shield a target, then damage it again while Weakened Soul is up — expect Flash Heal (the new @27
   node), **not** a repeated PW:S attempt.
9. Drain the Disc priest to 55% mana with a target at 60% HP — Flash Heal must still cast (previously
   vetoed by `LOW`), and Penance must still cast at 80% target HP (previously vetoed by
   `estAmount = 25`).
10. Take raid damage with the Disc priest above 40% mana — expect Divine Hymn, then Power Infusion,
    then Inner Focus immediately followed by Prayer of Healing.
11. Drop the Disc priest below 40% mana with nobody in the critical band — Hymn of Hope should fire.
    Repeat with somebody below 25% — it must **not** fire.
12. Holy priest: confirm Circle of Healing now leads the critical band, Greater Heal fires inside the
    critical and low bands rather than the medium one, and Renew appears as the medium-band filler.
13. Both healers: pull a boss and confirm Renew stays up on the main tank, and that Shadowfiend fires
    early in the fight rather than waiting for 40% mana.
14. Shadow priest on a boss: confirm Shadow Word: Death is **never** cast (the boss never enters the
    2-second execute window), Vampiric Embrace is reapplied if dispelled or expired, and the bot never
    leaves Shadowform. On a trash mob at ~5% HP, confirm SW:D does fire.
15. Shadow priest with three or more adds: Mind Sear channels, and the clip fires when the channel is
    interrupted by an expiring DoT. Watch for Mind Flay being clipped after roughly two ticks when
    Mind Blast comes off cooldown.
16. `.playerbot perf` before and after — the new triggers run every tick, and
    `MindFlayChannelCheckTrigger` does four cooldown/aura lookups when it passes the channel check.

## Open items requiring in-game confirmation

- Whether the Mind Flay channel exposes remaining duration through an aura on the target, which is
  what step 3 of `MindFlayChannelCheckTrigger` reads. Fallback described in section B3.
- Spell school and Shadowform castability are DBC data, not derivable from this repo. Desperate Prayer
  and Hymn of Hope were settled by the user as blocked in Shadowform, which is why Shadow keeps
  neither.
