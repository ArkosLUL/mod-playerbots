# Disc Priest bots barely cast Power Word: Shield

## Context

During Malygos Phase 1, Discipline Priest bots almost never cast Power Word: Shield, even though
PW:S sits at the top of every healing band in the Disc strategy table.

The cause is **not** in the Eye of Eternity strategy. `MalygosMultiplier::GetValue`
([EoEMultipliers.cpp:66-99](../../../src/Ai/Raid/EoE/EoEMultipliers.cpp#L66-L99))
returns `1.0f` for every `CastSpellAction` in P1 except `CastBlinkBackAction`,
`CastDisengageAction` and, for non-tanks, `CastReachTargetSpellAction`. No heal or buff cast is
suppressed. The bug is in the generic priest healing code and reproduces on every encounter — EoE P1
just makes it obvious because it is a long fight with steady chip damage.

### Root cause

`CastPowerWordShieldOnPartyAction` inherits its target from `HealPartyMemberAction`, i.e. the
`"party member to heal"` value, then hard-vetoes itself when that one target has Weakened Soul:

```cpp
// src/Ai/Class/Priest/PriestActions.cpp:24-27
bool CastPowerWordShieldOnPartyAction::isUseful()
{
    return HealPartyMemberAction::isUseful() && !botAI->HasAura("weakened soul", GetTarget());
}
```

`PartyMemberToHeal::Calculate`
([PartyMemberToHeal.cpp:71-126](../../../src/Ai/Base/Value/PartyMemberToHeal.cpp#L71-L126))
returns a **single** unit: `argmin(healthPct + distance/10)` over the whole raid. So:

1. Priest shields the lowest-HP raider, who now carries Weakened Soul for 15 s.
2. A shield absorbs damage but does not raise health %, so that raider stays lowest-HP and keeps
   owning the `"party member to heal"` slot.
3. For those 15 s **all four** PW:S bands go USELESS at once — critical (36), low (26), medium (19),
   almost-full (13) — because they resolve to the same blocked target. `Queue::Push` de-dupes by
   action name, so they are literally one basket
   ([Queue.cpp:11-28](../../../src/Script/WorldThr/Queue.cpp#L11-L28)).
4. The priest falls through to Penance / Prayer of Mending / Flash Heal, which heal the target up,
   and only once somebody *else* becomes the raid's lowest-HP member can a shield land again.

Net effect: roughly one shield per 15 s in a raid where two dozen other people are unshielded and
eligible.

Two aggravating factors:

- The one PW:S action that *does* rescan and retarget, `CastPowerWordShieldOnNotFullAction`
  ([PriestActions.cpp:95-124](../../../src/Ai/Class/Priest/PriestActions.cpp#L95-L124)),
  is wired only to the AoE bands `group heal setting` (32) and `medium group heal setting` (33)
  ([HealPriestStrategy.cpp:27-49](../../../src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp#L27-L49)).
  `AoeInGroupTrigger` needs 6+ damaged raiders in a 25-man, which Malygos P1 rarely produces.
- Disc never pre-shields. There is no main-tank shield node at all, so the shield is purely
  reactive.

The same veto on the self-shield `CastPowerWordShieldAction::isUseful`
([PriestActions.cpp:19-22](../../../src/Ai/Class/Priest/PriestActions.cpp#L19-L22)) is
correct and stays — there is no alternative target for a self-cast.

## Approach

### 1. Shared shield-target scan — `src/Ai/Class/Priest/PriestActions.cpp`

Add a file-local helper in an anonymous namespace. It replaces the three ad-hoc group scans that
exist today and fixes their shared defects: no `bot->GetGroup()` null check
(PriestActions.cpp:39, 68, 97), `sPlayerbotAIConfig.spellDistance` (30 yd) instead of heal range,
and no line-of-sight test.

```cpp
namespace
{
// Weakened Soul locks a target out for 15 s and a shield absorbs without raising health %, so the
// raider we just shielded stays the lowest-health one and keeps owning "party member to heal".
// Picking the next shieldable body instead is what makes the shield roll across a raid.
Unit* FindShieldTarget(PlayerbotAI* botAI, Player* bot, float maxHealthPct = 100.0f)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    float const range = botAI->GetRange("heal");
    MinValueCalculator calc(100);

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player || player->isDead() || player->IsGameMaster() || player->IsFullHealth())
            continue;
        if (player->GetMapId() != bot->GetMapId())
            continue;

        float const health = player->GetHealthPct();
        if (health > maxHealthPct)
            continue;
        if (player->GetDistance2d(bot) > range || !bot->IsWithinLOSInMap(player))
            continue;
        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
            continue;

        calc.probe(health, player);
    }

    return (Unit*)calc.param;
}
}
```

`MinValueCalculator` and `HasAnyAuraOf` are already in use in this file
(PriestActions.cpp:98, 112); `botAI->GetRange("heal")` is the same range basis
`CastHealingSpellAction` uses (GenericSpellActions.cpp:375).

### 2. Retarget the party shield — same file, plus `PriestActions.h`

Declare `Unit* GetTarget() override;` on `CastPowerWordShieldOnPartyAction`
([PriestActions.h:82-91](../../../src/Ai/Class/Priest/PriestActions.h#L82-L91)) and
implement:

```cpp
Unit* CastPowerWordShieldOnPartyAction::GetTarget()
{
    Unit* target = HealPartyMemberAction::GetTarget();
    if (target && !botAI->HasAnyAuraOf(target, "weakened soul", "power word: shield", nullptr))
        return target;

    return FindShieldTarget(botAI, bot);
}
```

`isUseful()` stays as it is — it now runs against the substituted target, so it only refuses when
nobody in the raid is shieldable. This one change fixes all four bands at once, because the queue
collapses them into a single basket.

Rescanning on every `GetTarget()` call costs ~5 raid walks per tick in the worst case (isUseful,
mana multiplier, isPossible, Execute), the same shape as the existing `on not full` scan. If that
shows up in a profile, memo the result behind a `getMSTime()` stamp the way
`MalygosMultiplier::RefreshSnapshot` does (EoEMultipliers.cpp:34-51) — not needed up front.

### 3. Main-tank pre-shield — new trigger + action + strategy node

Follows the existing `renew on main tank` wiring exactly (`RenewOnMainTankTrigger`
[PriestTriggers.h:165-169](../../../src/Ai/Class/Priest/PriestTriggers.h#L165-L169),
`CastRenewOnMainTankAction`
[PriestActions.h:96-100](../../../src/Ai/Class/Priest/PriestActions.h#L96-L100)).

`PriestTriggers.h` / `.cpp` — `BuffOnMainTankTrigger` only checks for the shield aura itself, so it
would rearm every tick during Weakened Soul:

```cpp
class PowerWordShieldOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    PowerWordShieldOnMainTankTrigger(PlayerbotAI* botAI)
        : BuffOnMainTankTrigger(botAI, "power word: shield") {}

    bool IsActive() override;   // BuffOnMainTankTrigger::IsActive() && no weakened soul
};
```

`PriestActions.h` / `.cpp` — note `checkIsOwner` is left at its `false` default, unlike renew: any
priest's shield should stop this one, since Weakened Soul is shared.

```cpp
class CastPowerWordShieldOnMainTankAction : public BuffOnMainTankAction
{
public:
    CastPowerWordShieldOnMainTankAction(PlayerbotAI* botAI)
        : BuffOnMainTankAction(botAI, "power word: shield") {}

    bool isUseful() override;   // BuffOnMainTankAction::isUseful() && no weakened soul
};
```

Registration in `PriestAiObjectContext.cpp` — trigger creator next to line 110, action creator next
to line 198, plus the two `static Trigger*` / `static Action*` factories (~:163, ~:310).

Action node in `GenericPriestStrategyActionNodeFactory.h`, matching the other two shield nodes
(prerequisite `remove shadowform`, no alternatives) — lines 95-113.

Strategy node in `HealPriestStrategy.cpp`, placed with the other main-tank node (~:123):

```cpp
triggers.push_back(
    new TriggerNode(
        "power word: shield on main tank",
        { NextAction("power word: shield on main tank", ACTION_MEDIUM_HEAL + 4.5f) }
    )
);
```

24.5 sits below the `party member low health` shield (26) and above `renew on main tank` (24), so a
hurt raider still outranks topping the tank's bubble. `HolyPriestStrategy` ("holy dps") calls
`HealPriestStrategy::InitTriggers` (HolyPriestStrategy.cpp:41-43), so it inherits the node too;
`HolyHealPriestStrategy` has its own table and is untouched.

Note this action derives from `CastBuffSpellAction`, not `CastHealingSpellAction`, so
`HealerAutoSaveManaMultiplier` (ConserveManaStrategy.cpp:92-130) does not gate it on low mana.
That is the intended behaviour for a tank bubble, but worth watching in a long fight.

### 4. Cleanup

- Point `CastPowerWordShieldOnNotFullAction::GetTarget()` at `FindShieldTarget(botAI, bot)` and drop
  its inline loop (PriestActions.cpp:95-119). `isUseful()` is unchanged.
- Delete `CastPowerWordShieldOnAlmostFullHealthBelowAction` — it is registered but no strategy
  references it. Three sites: class at PriestActions.h:242-252, impls at PriestActions.cpp:37-93,
  registration at PriestAiObjectContext.cpp:193 and factory at :304-306. Also remove the now-dangling
  `dynamic_cast` at
  [BTMultipliers.cpp:299](../../../src/Ai/Raid/BT/BTMultipliers.cpp#L299) — the other
  three PW:S casts in that whitelist stay.

## Critical files

| File | Change |
|---|---|
| `src/Ai/Class/Priest/PriestActions.cpp` | `FindShieldTarget` helper; party-shield `GetTarget()`; rewire `on not full`; delete dead action impls |
| `src/Ai/Class/Priest/PriestActions.h` | `GetTarget()` decl; new main-tank action; delete dead action class |
| `src/Ai/Class/Priest/PriestTriggers.h` / `.cpp` | new `PowerWordShieldOnMainTankTrigger` |
| `src/Ai/Class/Priest/PriestAiObjectContext.cpp` | register new trigger + action; unregister dead action |
| `src/Ai/Class/Priest/Strategy/GenericPriestStrategyActionNodeFactory.h` | action node for the main-tank shield |
| `src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp` | main-tank shield trigger node at 24.5 |
| `src/Ai/Raid/BT/BTMultipliers.cpp` | drop the dead `dynamic_cast` |

No EoE file changes.

## Verification

The module cannot be compiled headless in this environment, so this is a static-check plus a
hand-off:

1. **Static** — confirm every deleted symbol has zero remaining references
   (`grep -rn CastPowerWordShieldOnAlmostFullHealthBelow src/`), and that both new context names
   (`"power word: shield on main tank"` as trigger *and* action) resolve; an unregistered name shows
   up at runtime as `A:... - UNKNOWN` in the bot action log, not as a build error.
2. **Build** — you rebuild the worldserver (`mod-playerbots` only; no core files touched).
3. **In game** — enable the bot debug action log for a Disc priest bot
   (`.playerbot debug action` or the `LogInGroupOnly`/action-log path) and pull Malygos with a
   raid containing at least one Disc priest.
   - Expect `power word: shield on party` to appear repeatedly rather than once per ~15 s, on
     *different* targets, and no longer to log `USELESS` while other raiders are unshielded.
   - Expect `power word: shield on main tank` to re-fire roughly every 15 s once Weakened Soul
     falls off the tank.
   - Sanity check the same priest in a 5-man and solo to confirm the null-group guard and the
     narrower target pool behave (no crash, still shields).
4. **Regression** — the Black Temple Reliquary of Souls fight exercises the PW:S whitelist in
   `BTMultipliers.cpp`; confirm the priest still shields during Essence of Suffering.
