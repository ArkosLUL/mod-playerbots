# Gothik the Harvester — living-side-only bot strategy

## Context

**The current Gothik strategy does not exist.** Every piece of it is commented out —
`GothikMoveToAssignedSideAction` / `GothikChooseTargetAction`
(`src/Ai/Raid/Naxx/Action/NaxxActions_Gothik.cpp`, whole file), the two triggers
(`NaxxTriggers.cpp:471-487`), both context registrations
(`NaxxActionContext.h:57-58,116-117`, `NaxxTriggerContext.h:55-56,108-109`), the NPC entry
constants (`NaxxSpellIds.h:26-34`), `GothikGenericMultiplier`
(`NaxxMultipliers.cpp:475-498`) and the trigger nodes (`NaxxStrategy.cpp:96-103`). Commit
`48651fab7` ("Commented out Gothik The Harvester strategy") disabled the lot. The old design
split the raid across both sides and computed a per-bot side assignment from group
composition — that is what we are replacing.

Two Gothik-related things *are* live and stay live:
- `NaxxBurstWindowMultiplier` gates burst cooldowns on Gothik
  (`NaxxMultipliers.cpp:655-658`) — the burst-suppression requirement is already implemented.
- `NaxxThreatRedirectMultiplier` already lists `"gothik the harvester"` in
  `noRedirectBosses` (`NaxxMultipliers.cpp:556`) — nothing to do.

Goal: the whole raid fights on the living side, never touches Gothik while he is on the
balcony or behind the gate, kills adds in a fixed priority, and saves burst cooldowns for
phase 2.

## Encounter facts (from `src/server/scripts/Northrend/Naxxramas/boss_gothik.cpp`)

| Fact | Source |
| --- | --- |
| Living side is `Y < -3360.78`; dead side is `Y > -3360.78` | `IN_LIVE_SIDE`, line 175 |
| Room bounds: `X` in `[2633.84, 2750.49]`, `Y` in `[-3434, -3285]` | lines 170-174 |
| Balcony perch: `2642.1, -3387.0, 285.5` (arena floor is `Z ≈ 267.7`) | line 221 |
| Boss takes **zero damage** until phase 2 | `DamageTaken`, lines 370-376 |
| `UNIT_FLAG_DISABLE_MOVE` set on pull, removed at phase 2 | lines 232, 481 |
| Phase 2 starts after the 24-wave table (~30 s + 254 s ≈ 4:45) and teleports him to the **living** side first | lines 468-488 |
| In phase 2 he teleports side-to-side every 20 s until he drops below 30 % | `EVENT_TELEPORT`, lines 438-453 |
| Gate opens at 30 % HP **or** at the 2-minute check if the raid is *not* split | lines 459-465, 491-508 |
| Gothik entry `16060` | `naxxramas.h:172` |

**Consequence of the living-side-only tactic:** `CheckGroupSplitted()` returns false when
everyone stands on one side, so the gate opens ~2 minutes into the fight and every dead-side add
walks over at once, long before phase 2. The strategy has to handle mixed living/dead adds, and
the kill-priority table below covers all seven entries for that reason.

## Design

Everything is rebuilt fresh — do not un-comment the old side-assignment code; delete it.

### 1. Constants — `src/Ai/Raid/Naxx/NaxxSpellIds.h`

Replace the commented block at lines 26-34 with live constants:

```cpp
// Gothik the Harvester
static constexpr uint32 GothikEntry              = 16060;
static constexpr float  GothikGateY              = -3360.78f;
// Balcony perch sits ~18y above both arena floors (boss_gothik.cpp:221).
static constexpr float  GothikBalconyZ           = 280.0f;
static constexpr uint32 GothikLivingTraineeEntry = 16124;
static constexpr uint32 GothikLivingKnightEntry  = 16125;
static constexpr uint32 GothikLivingRiderEntry   = 16126;
static constexpr uint32 GothikDeadTraineeEntry   = 16127;
static constexpr uint32 GothikDeadKnightEntry    = 16148;
static constexpr uint32 GothikDeadHorseEntry     = 16149;
static constexpr uint32 GothikDeadRiderEntry     = 16150;
```

### 2. `GothikBossHelper` — `src/Ai/Raid/Naxx/NaxxBossHelper.h`

New helper next to `LoathebBossHelper` (`NaxxBossHelper.h:1321-1349`), same
`UpdateBossAI()/Reset()` shape as the other Naxx helpers. The `GenericBossHelper<BossAiType>`
template cannot be used — `boss_gothikAI` is file-local to `boss_gothik.cpp`.

Resolve with `AI_VALUE2(Unit*, "find target", "gothik the harvester")` like the other Naxx
helpers. That value only walks `GetThreatenedByMeList()`
(`src/Ai/Base/Value/TargetValue.cpp:160-185`), but `JustEngagedWith` calls
`SetInCombatWithZone()` (`boss_gothik.cpp:227`) which threats every player in the room, so it
resolves from the pull onward even though he is `REACT_PASSIVE` on the balcony. Fall back to
`GetFirstAliveUnitByEntry(botAI, NaxxSpellIds::GothikEntry)`
(`src/Ai/Raid/RaidBossHelpers.h:29`) for a bot that battle-rezzed or arrived after the pull and
so never made the threat list.

Public surface:

```cpp
static bool IsLiveSide(WorldObject const* who);   // who->GetPositionY() < GothikGateY
bool IsPhaseTwo() const;                          // !_unit->HasUnitFlag(UNIT_FLAG_DISABLE_MOVE)
bool IsOnBalcony() const;                         // _unit->GetPositionZ() > GothikBalconyZ
bool IsBossAttackable() const;                    // IsPhaseTwo() && !IsOnBalcony()
                                                  //   && IsLiveSide(_unit) == IsLiveSide(bot)
Unit* GetBoss() const;
Unit* GetBestAdd() const;                         // priority table below
static uint32 GetAddPriority(uint32 entry);
```

Living-side hold point: `{2691.2f, -3387.0f}` at `Z = 267.68f` (the arena floor; these are the
coordinates the old code used and match `PosGroundLivingSide` in `boss_gothik.cpp:166`).

`GetBestAdd()` walks `context->GetValue<GuidVector>("possible targets no los")` — the **no-los**
variant, because the gate wall would otherwise hide adds the bot is about to have to deal with,
and the side bookkeeping is done on coordinates anyway (idiom: `ZAActions.cpp:543-588`). It picks
the highest-priority alive add **on the bot's own side**, tie-broken by distance. The side test is
on the *add's* live position, so an add that crosses after the gate opens becomes a valid target
automatically — no gate-state lookup needed.

Priority table:

| Prio | Entry | Add | Why |
| --- | --- | --- | --- |
| 70 | 16150 | Dead Rider | Drain Life self-heal + Unholy Frenzy snowball |
| 60 | 16126 | Living Rider | Shadow Bolt Volley, biggest raid damage in P1 |
| 50 | 16148 | Dead Knight | Whirlwind |
| 40 | 16149 | Dead Horse | Stomp |
| 30 | 16125 | Living Knight | Shadow Mark |
| 20 | 16127 | Dead Trainee | Arcane Explosion |
| 10 | 16124 | Living Trainee | Death Plague |

### 3. Triggers — `NaxxTriggers.h` / `NaxxTriggers.cpp`

Replace the two commented triggers with:

- `GothikTrigger` (`"gothik"`) — `helper.UpdateBossAI()` and (`bot->IsInCombat()` or the boss is
  in combat). Drives targeting.
- `GothikWrongSideTrigger` (`"gothik wrong side"`) — helper live **and** `!IsLiveSide(bot)`.
  Deliberately narrow: while the gate is shut nobody can cross, so this is a safety net for a
  bot that started on, or got knocked to, the dead side.

Both hold a `GothikBossHelper` member, like `HeiganDecrepitFeverTrigger` and friends.

### 4. Actions — `Action/NaxxActions.h` + `Action/NaxxActions_Gothik.cpp`

Delete the entire commented file body and write two actions.

`GothikChooseTargetAction : public AttackAction` — `"gothik choose target"`:

```
isUseful(): helper.UpdateBossAI()
Execute():
    if (helper.IsBossAttackable())            -> Attack(boss)     // P2, he is standing on us
    else if (Unit* add = helper.GetBestAdd()) -> Attack(add)
    else                                      -> false
    (skip the Attack call when it is already the current target, as the old code did)
```

Boss-first in phase 2 is deliberate: the wave table is finished by then, leftovers get cleaved,
and it lines up with the burst window.

`GothikStayOnLivingSideAction : public MovementAction` — `"gothik stay on living side"`:

```
isUseful(): helper.UpdateBossAI() && !GothikBossHelper::IsLiveSide(bot)
Execute():  MoveTo(NAXX_MAP_ID, 2691.2f, -3387.0f, 267.68f, ...,
                   MovementPriority::MOVEMENT_COMBAT)
            fall back to MoveInside(...) like the old code did
```

No side assignment, no role math — every bot wants the living side.

### 5. Multiplier — `NaxxMultipliers.h` / `NaxxMultipliers.cpp`

Replace the commented `GothikGenericMultiplier` (which referenced a non-existent
`boss_botAI` and would not compile) with:

```cpp
float GothikGenericMultiplier::GetValue(Action* action)
{
    if (!helper.UpdateBossAI())
        return 1.0f;

    // Targeting belongs to "gothik choose target"; the generic assist actions would copy
    // whatever the tank happens to be on.
    context->GetValue<bool>("neglect threat")->Set(true);
    if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action))
        return 0.0f;

    // On the balcony he is immune (boss_gothik.cpp:370) and out of reach; behind the gate he
    // is unreachable. Either way nothing may spend a global or a step on him.
    if (!helper.IsBossAttackable() && AI_VALUE(Unit*, "current target") == helper.GetBoss())
    {
        if (dynamic_cast<CastHealingSpellAction*>(action))
            return 1.0f;
        if (action->getName() == "gothik choose target" ||
            action->getName() == "gothik stay on living side")
            return 1.0f;
        return 0.0f;
    }

    return 1.0f;
}
```

The assist-suppression plus `neglect threat` mirrors `GluthGenericMultiplier`
(`NaxxMultipliers.cpp:500-510`), `FourhorsemanGenericMultiplier` (`:460-473`) and
`LoathebGenericMultiplier` (`:112-142`) — every Naxx boss with a `choose target` action pairs it
with exactly this. The heal exemption mirrors `ThaddiusGenericMultiplier:179-186`.

Note the old `GothikGenericMultiplier` body did not compile (`boss_botAI->GetEvents()` and
`Action::GetTarget()` do not exist) — that is dead code, not a reference.

### 6. Wiring

- `NaxxActionContext.h` — uncomment/rewrite lines 57-58 and 116-117 for the two action names.
- `NaxxTriggerContext.h` — same for lines 55-56 and 108-109 (`"gothik"`,
  `"gothik wrong side"`).
- `NaxxStrategy.cpp:96-103` — replace the commented block with:
  ```cpp
  triggers.push_back(new TriggerNode("gothik wrong side",
      { NextAction("gothik stay on living side", ACTION_RAID + 4) }));
  triggers.push_back(new TriggerNode("gothik",
      { NextAction("gothik choose target", ACTION_RAID + 1) }));
  ```
- `NaxxStrategy.cpp:232-247` — `multipliers.push_back(new GothikGenericMultiplier(botAI));`

No changes needed at the four cross-cutting registration sites (`RaidStrategyContext.h`,
`BuildShared*Contexts.cpp`, `PlayerbotAI.cpp` map switch) — Naxx (map 533) is already wired.

### 7. Burst cooldowns — already done

`NaxxBurstWindowMultiplier::EvaluateWindow` (`NaxxMultipliers.cpp:655-658`) returns `0.0f`
while Gothik has `UNIT_FLAG_DISABLE_MOVE`, i.e. for the whole wave phase, and `1.0f` from
phase 2 on. That covers Bloodlust/Heroism and the rest of `IsBurstCooldownAction`
(`src/Ai/Base/Combat/BurstCooldowns.cpp:22-52`). Decision: it stays phase-2-only, with no
same-side condition — **no code change here**.

### Deliberately out of scope

- **Skull-marking the priority add** (`SetRtiTarget` / `MarkTargetWithSkull`, the ZA idiom). Adds
  die every few seconds here, so the mark would thrash; the multiplier already forces every bot
  through `gothik choose target`, which is enough to focus-fire. Easy to add later if human raid
  members should see the call.
- **Any side-splitting.** No tank/healer side assignment, no dead-side hold point.

## Files

- `src/Ai/Raid/Naxx/NaxxSpellIds.h` — Gothik entry/geometry constants
- `src/Ai/Raid/Naxx/NaxxBossHelper.h` — new `GothikBossHelper`
- `src/Ai/Raid/Naxx/NaxxTriggers.{h,cpp}` — two triggers
- `src/Ai/Raid/Naxx/Action/NaxxActions.h` + `Action/NaxxActions_Gothik.cpp` — two actions
- `src/Ai/Raid/Naxx/NaxxActionContext.h`, `NaxxTriggerContext.h` — registration
- `src/Ai/Raid/Naxx/NaxxMultipliers.{h,cpp}` — `GothikGenericMultiplier` (burst window untouched)
- `src/Ai/Raid/Naxx/NaxxStrategy.cpp` — trigger nodes + multiplier

## Verification

Static:
- No `gothik` string remains commented out in the Naxx directory.
- Every NPC entry matches `boss_gothik.cpp:77-84`; `GothikGateY` matches `POS_Y_GATE`.
- `GothikGenericMultiplier` is in `InitMultipliers`, both trigger nodes in `InitTriggers`, both
  names in both contexts.

In-game, 10-man and 25-man, with a bot raid:
1. **Pull.** Bots stay on the living side (`Y < -3360.78`) for the whole fight; nobody walks
   through the gate. Drop a bot on the dead side before the pull — it should walk back.
2. **Phase 1.** No bot targets or moves toward Gothik while he is on the balcony; melee never
   collect under it. Shaman does not cast Bloodlust/Heroism; no Recklessness / Arcane Power /
   Avenging Wrath / offensive potions either.
3. **Kill order.** With a Rider and Trainees up at once, everything focuses the Rider first,
   then Knights, then Trainees.
4. **~2 min in,** the gate opens because the raid is not split; dead-side adds cross. Bots pick
   them up as they enter the living side, Dead Rider first.
5. **Phase 2.** He teleports onto the living side: bots switch to the boss, burst cooldowns and
   lust fire. When he teleports to the dead side, bots stop attacking him and go back to adds
   without running at the gate; they re-engage on his next teleport back.
6. **Below 30 %,** the gate is open and he stops teleporting — bots finish him.
