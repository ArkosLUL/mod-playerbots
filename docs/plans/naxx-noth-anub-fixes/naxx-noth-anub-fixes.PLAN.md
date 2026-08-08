# Naxxramas fixes: Noth target oscillation + Anub'rekhan Locust Swarm formation

## Context

Two bugs reported from live raid testing of the playerbot Naxxramas strategies.

**1. Noth the Plaguebringer, ground phase.** Bots freeze in place and visibly flip between the boss
and the Plagued adds every tick, never landing a cast. Root cause found:

- `NothChooseTargetAction::Execute` (`src/Ai/Raid/Naxx/Action/NaxxActions_Noth.cpp:155-160`) returns
  `false` when the bot is already on the target it wants.
- `Engine::DoNextAction` (`src/Bot/Engine/Engine.cpp:218-232`) treats a `false` return as FAILED and
  **keeps draining the queue**, so the tick falls through to the generic `dps assist` at relevance
  `50.0f` (`src/Ai/Base/Strategy/DpsAssistStrategy.cpp:13`).
- `GeneralFindTargetSmartStrategy::IsBetter` (`src/Ai/Base/Value/DpsTargetValue.cpp:171-195`) ranks
  in-melee-range and low-remaining-lifetime first and has **no current-target preference**, so a
  freshly spawned Plagued Warrior always beats Noth. The bot attacks the warrior, next tick
  `noth choose target` yanks it back to the boss, and it loops on a 2-tick cycle.
- `NothGenericMultiplier` (`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:405-448`) only zeroes
  `DpsAssistAction`/`TankAssistAction` **inside the Blink window**. Every other Naxx boss that owns
  its targeting kills them for the whole fight — Loatheb (`:118-125`), Razuvious (`:264-271`),
  Four Horsemen (`:480-484`), Gothik (`:497-501`), Gluth (`:527-531`). Noth is the outlier.

Kill order also has to change: today ground-phase DPS get `{guardians.tanked, guardians.any, boss}`
(`NaxxActions_Noth.cpp:149-153`), so Plagued Warriors (2 in 10-man / 3 in 25-man every 30 s for the
whole 110 s ground phase) are never killed by anyone but the assist tank. **Decision: ranged DPS burn
the adds, melee DPS stay on the boss.**

**2. Anub'rekhan, Locust Swarm.** Non-tanks keep their Impale spread ring during the swarm and eat
the ~15 yd aura. Answer to "did the MT kite logic change?": **no, not behaviourally.** `git log` on
`NaxxActions_Anubrekhan.cpp` shows the last two touches were `929650bb8` (2026-07-28, extracted the
kite into `KiteBoss()`, swapped `bot->GetMapId()`→`NAXX_MAP_ID` and `bot->GetPositionZ()`→
`RoomFloorZ`, waypoint count → 32) and `5cae83867` (2026-07-29, `MaxHoldRadius` 40→52 — assist-tank
`HoldAdds` only). The kite itself has always been "nearest waypoint + 1 on a fixed circle around the
room centre, gated on the live swarm aura".

The formation is the actual bug. `AnubrekhanPositionAction::Execute`
(`NaxxActions_Anubrekhan.cpp:114-141`) routes non-tanks to `TakeRangedSlot`/`TakeMeleeSlot` in every
phase; the swarm only changes the action's *relevance* (`ACTION_RAID + 2` → `ACTION_EMERGENCY + 5`,
`NaxxStrategy.cpp:67-71`). Melee park 5 yd from their target — inside the swarm — with `FleeAction`
multiplied to 0 (`NaxxMultipliers.cpp:464-469`). There is also **no pre-warning**: the only detection
is the reactive aura check `IsLocustSwarmActive()` (`NaxxBossHelper.h:1624-1633`).

### Decisions taken (agreed with user)

| Topic | Decision |
|---|---|
| Swarm formation | Non-tanks **stack**, MT kite radius **45 → 35 yd** |
| Pre-warning | Model the swarm clock: swarms 2+ get a ~3 s lead, swarm 1 is reactive |
| Noth kill order | **Ranged DPS burn adds, melee DPS stay on boss** |

This reverses the "non-tanks follow the kite at safe range" decision recorded in
`docs/raids/naxxramas.md:68-76`, so that doc has to be updated too.

---

## Part 1 — Noth: kill the oscillation, split targeting by role

### 1.1 `src/Ai/Raid/Naxx/NaxxMultipliers.cpp` — `NothGenericMultiplier::GetValue` (line 405)

Move the `DpsAssistAction` / `TankAssistAction` kill **out** of the Blink-window branch so it applies
for the whole encounter, matching `GothikGenericMultiplier` (`:488-519`). Insert it right after the
existing `CombatFormationMoveAction` check at `:412-415`, before the
`if (!helper.IsBlinkWindow() || botAI->IsTank(bot)) return 1.0f;` early-out:

```cpp
    // Targeting belongs to "noth choose target". The generic assists rank by range and remaining
    // lifetime, so a fresh Plagued Warrior always outranks the boss and the bot flips between them
    // every tick instead of committing.
    if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action))
    {
        return 0.0f;
    }
```

Drop `DpsAssistAction`/`TankAssistAction` from the Blink-window list at `:437-441`, since they are
now handled above.

**Do not** set `neglect threat` here, unlike Gothik/Loatheb. Noth 10-man never resets threat, so the
`ThreatMultiplier` throttle (`src/Ai/Base/Strategy/ThreatStrategy.cpp:12-36`) is still wanted.

### 1.1b Same function — scope the Blink mute to bots on the boss

`IsBlinkWindow()` (`NaxxBossHelper.h:1857-1883`) is true for `BlinkLeadMs 1500 + BlinkWindowMs 4000`
= **5.5 s**, three times per ground phase, 25-man only. Today the window zeroes `MeleeAction` and
every `CastSpellAction` for all non-tanks — a total DPS stop so the MT can re-taunt after
`DoResetThreatList()`.

That is right for anyone on Noth, but wrong once 1.2 puts ranged on adds: `DoResetThreatList()` only
empties **Noth's** threat table, so a bot hitting a Plagued Warrior cannot pull the boss and is being
silenced for nothing — 5.5 s × 3 per ground phase is ~18% of ranged uptime. Gate the early-out on the
bot's actual target instead of on role alone:

```cpp
    // The threat wipe only empties Noth's own table, so only the bots hitting him have to hold.
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (!helper.IsBlinkWindow() || botAI->IsTank(bot) || (boss && currentTarget != boss))
    {
        return 1.0f;
    }
```

`helper.GetBoss()` is already available in this function via the `helper.UpdateBossAI()` call at the
top; capture it into a local `Unit* boss` alongside it. Keep the `boss &&` guard so a null boss falls
back to the old conservative behaviour.

### 1.2 `src/Ai/Raid/Naxx/Action/NaxxActions_Noth.cpp` — ground-phase target lists (line 149)

Replace the `else` branch of the priority build (currently
`targets = {guardians.tanked, guardians.any, boss};`) with a caster/melee split:

```cpp
    else if (botAI->IsRanged(bot))
    {
        // Ranged clear the adds: they spawn from alcoves 25-50 yd out every 30s and nobody but the
        // assist tank ever touched them. Type order, not distance, so the whole back line focuses
        // one add; lowest GUID inside a type keeps that stable as adds trade health.
        targets = {guardians.tanked, guardians.any, champions.tanked, champions.any,
                   warriors.tanked,  warriors.any,  boss};
    }
    else if (helper.IsBlinkWindow())
    {
        // The threat wipe means nobody may touch Noth for ~5.5s. Rather than stand idle, melee spend
        // the window on whatever the ranged are already burning.
        targets = {guardians.tanked, guardians.any, champions.tanked, champions.any,
                   warriors.tanked,  warriors.any};
    }
    else
    {
        // Melee stay on Noth - chasing adds across the room costs more uptime than the adds are
        // worth, and the ranged are already on them.
        targets = {boss};
    }
```

The Blink branch deliberately has **no boss fallback**: with no adds up there is nothing to switch
to, `FirstAvailable` returns null, the action bails, and 1.1b's `MeleeAction`/`CastSpellAction` mute
holds them still — which is the correct behaviour during a threat reset. When the window closes they
fall back to `{boss}` on the next tick.

Ordering works out: `NothChooseTargetAction` derives from `AttackAction`
(`NaxxActions.h:482-490`), not `CastSpellAction` or `MeleeAction`, so the Blink mute never blocks the
switch itself. The one transition tick where a melee bot still has the boss as `current target` is
muted by 1.1b and then retargets — a single tick, not a loop.

`botAI->IsHeal(bot)` bots fall into the ranged branch here, which is fine — they only reach this
action if they have a target to pick at all, and healing outranks it.

Leave the `ownsAdds` branch (`:127-141`) and the `balcony` branch (`:142-148`) as they are — during
the balcony phase the boss is `UNIT_FLAG_NOT_SELECTABLE` and everyone is already on adds.

No sticky/hysteresis layer is needed. Once 1.1 mutes the generic assists, `noth choose target` is the
only authority and its output is a pure function of the GUID-sorted add list, so it cannot flip.

### 1.3 Known non-blocker, no change

`NothPositionAction` sits at `ACTION_RAID + 2` above `noth choose target` at `+1`
(`NaxxStrategy.cpp:221-227`), so a ranged bot inside 25 yd of a Champion spends its tick on
`KiteChampions()` and defers the retarget by a tick. That is a delay, not a loop — leave it.

---

## Part 2 — Anub'rekhan: swarm clock + centre stack

### 2.1 `src/Ai/Raid/Naxx/NaxxBossHelper.h` — `AnubrekhanBossHelper` (line 1547)

**Constants** (around `:1553-1573`):

- `KiteRadius` `45.0f` → `35.0f`, and rewrite the comment: the raid stacks inside the circle now, so
  the radius is what sets boss-to-raid distance.
- Add:
  ```cpp
  // Where the raid piles up during the swarm: measured from the boss towards the room centre, so it
  // is always in heal/cast range and always outside the ~15 yd aura, wherever the kite has got to.
  static constexpr float SwarmStackDistance = 25.0f;
  // Bodies block each other at a single point; this is a de-clump ring, not a spread.
  static constexpr float SwarmStackRingRadius = 3.0f;
  static constexpr uint32 SwarmPeriodMs = 90000;
  static constexpr uint32 SwarmWarningMs = 3000;
  ```

**Clock state.** Add to `EncounterState` (`:1673-1680`): `uint32 lastSwarmStartMs = 0;`,
`bool swarmSeen = false;`, `bool swarmActive = false;`. These are per-instance and already reset when
the boss GUID changes or the state goes stale (`:1607-1616, 1683-1699`).

**Rising-edge detection** in `UpdateBossAI()`, immediately after `_state->lastSeenMs = now;` (`:1617`):

```cpp
        // The core schedules the first swarm at a random 70-120s (boss_anubrekhan.cpp, JustEngagedWith)
        // so it cannot be predicted - but every later one is exactly 90s after the last, and the aura
        // gives a clean edge to anchor on.
        bool swarmActive = IsLocustSwarmActive();
        if (swarmActive && !_state->swarmActive)
        {
            _state->lastSwarmStartMs = now;
            _state->swarmSeen = true;
        }
        _state->swarmActive = swarmActive;
```

**Accessors**, next to `MsUntilNextImpale()` / `IsImpaleImminent()` (`:1636-1650`):

```cpp
    // 0 until the first swarm has been seen - the opening cast is not predictable.
    uint32 MsUntilNextSwarm() const
    {
        if (!_state || !_state->swarmSeen)
            return 0;
        return SwarmPeriodMs - ((getMSTime() - _state->lastSwarmStartMs) % SwarmPeriodMs);
    }

    bool IsSwarmImminent() const
    {
        return _state && _state->swarmSeen && !IsLocustSwarmActive() && MsUntilNextSwarm() <= SwarmWarningMs;
    }

    // The one gate the formation, the kite and the multiplier all read, so they cannot disagree
    // about which shape the raid is in.
    bool IsSwarmFormation() const { return IsLocustSwarmActive() || IsSwarmImminent(); }
```

### 2.2 `src/Ai/Raid/Naxx/NaxxTriggers.cpp` — `AnubrekhanLocustSwarmTrigger::IsActive` (line 347)

Swap `helper.IsLocustSwarmActive()` for `helper.IsSwarmFormation()` so the `ACTION_EMERGENCY + 5`
node (`NaxxStrategy.cpp:67-71`) also owns the ~3 s run-up.

### 2.3 `src/Ai/Raid/Naxx/Action/NaxxActions_Anubrekhan.cpp`

**Dispatch** in `Execute` (`:114-141`) — gate on the formation, not just the live aura:

```cpp
    bool swarm = helper.IsSwarmFormation();
    if (botAI->IsMainTank(bot))
    {
        return swarm ? KiteBoss() : false;
    }
    if (botAI->IsAssistTank(bot))
    {
        return HoldAdds(boss);
    }
    if (swarm)
    {
        return TakeSwarmStack(boss);
    }
    if (botAI->IsHeal(bot) || botAI->IsRanged(bot))
    {
        return TakeRangedSlot(boss);
    }
    return TakeMeleeSlot(boss);
```

`HoldAdds` needs no change: its hold point is already derived from the live boss radius
(`:161-171`) and `MaxHoldRadius = 52` still clears the new `KiteRadius = 35`.

**New `TakeSwarmStack(Unit* boss)`**, declared in `NaxxActions.h` next to the other slot helpers
(`:363-385`):

```cpp
bool AnubrekhanPositionAction::TakeSwarmStack(Unit* boss)
{
    // Anchored on the boss, not on the room centre: 25 yd out along the bearing to the centre keeps
    // everyone in heal and cast range whatever the kite is doing, and at KiteRadius 35 the pile only
    // orbits a 10 yd circle instead of chasing the boss around the room.
    float toCenter = std::atan2(AnubrekhanBossHelper::RoomCenterY - boss->GetPositionY(),
                                AnubrekhanBossHelper::RoomCenterX - boss->GetPositionX());
    float stackX = boss->GetPositionX() + std::cos(toCenter) * AnubrekhanBossHelper::SwarmStackDistance;
    float stackY = boss->GetPositionY() + std::sin(toCenter) * AnubrekhanBossHelper::SwarmStackDistance;

    std::pair<size_t, size_t> slot = AnubrekhanSwarmSlot(botAI, bot);
    float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(slot.first) / static_cast<float>(slot.second);
    float x = stackX + std::cos(theta) * AnubrekhanBossHelper::SwarmStackRingRadius;
    float y = stackY + std::sin(theta) * AnubrekhanBossHelper::SwarmStackRingRadius;
    return MoveToSlot(x, y);
}
```

`NaxxGetSlotIndexAndCount` (`NaxxBossHelper.h:94-106`) indexes **within** a role group, so healers,
ranged DPS and melee DPS would each build their own ring on the same point and overlap. Add a small
file-local helper in the anonymous namespace at the top of `NaxxActions_Anubrekhan.cpp` (`:15-25`)
that reuses `NaxxGetRoleGroups` and concatenates the three vectors into one index space:

```cpp
// One ring for every non-tank, so the three role groups do not each build their own on the same spot.
std::pair<size_t, size_t> AnubrekhanSwarmSlot(PlayerbotAI* botAI, Player* bot)
{
    NaxxRoleGroups groups = NaxxGetRoleGroups(botAI, bot);
    std::vector<Player*> all;
    all.insert(all.end(), groups.healers.begin(), groups.healers.end());
    all.insert(all.end(), groups.rangedDps.begin(), groups.rangedDps.end());
    all.insert(all.end(), groups.meleeDps.begin(), groups.meleeDps.end());

    auto it = std::find(all.begin(), all.end(), bot);
    if (it == all.end())
        return {0, 1};
    return {static_cast<size_t>(std::distance(all.begin(), it)), all.size()};
}
```

**`MoveToSlot`** (`:231-259`) — also bypass the 1 s throttle when the swarm formation is being
entered, so the collapse is not delayed by up to a second:

```cpp
    if (sameDestination && getMSTimeDiff(state.lastMoveMs, now) < AnubrekhanBossHelper::RepositionIntervalMs &&
        !helper.IsImpaleImminent() && !helper.IsSwarmImminent())
```

### 2.4 `src/Ai/Raid/Naxx/NaxxMultipliers.cpp` — `AnubrekhanGenericMultiplier` (line 450)

Two changes:

- Widen the `FleeAction` kill from `IsLocustSwarmActive()` to `IsSwarmFormation()` (`:466`).
- Add a `MeleeAction` kill for non-tanks during the formation. Without it `TakeSwarmStack` moves a
  rogue out and the generic melee chase drags it straight back onto the boss:

```cpp
    // Melee have no business on the boss during the swarm, and the generic chase would undo the
    // stack move as fast as the position action issues it.
    if (helper.IsSwarmFormation() && !botAI->IsTank(bot) && dynamic_cast<MeleeAction*>(action))
    {
        return 0.0f;
    }
```

### 2.5 Optional, same bug class as Part 1 — say if you want it left out

`AnubrekhanGenericMultiplier` has the identical gap Noth had: `AnubrekhanChooseTargetAction` ends on
`return false` when already on target (`NaxxActions_Anubrekhan.cpp:107-111`) and nothing mutes
`DpsAssistAction`/`TankAssistAction`, so the Crypt Guard focus can be overridden by `dps target`.
Not reported as a live symptom. One-line fix, same shape as 1.1.

### 2.6 Accepted trade-offs — worth knowing, not bugs

- **One Impale lands on the stack per swarm.** Swarm is ~20 s, Impale fires every 20 s. Unavoidable
  once the raid stacks; this is the cost of the chosen behaviour.
- **Boss-to-raid is 35 yd during the swarm** (kite radius), but the stack sits 25 yd from the boss,
  so casters and healers both reach it. If it still feels long in play, `KiteRadius` is the single
  knob — the stack distance is independent of it.

---

## Part 3 — Docs

`docs/raids/naxxramas.md`:

- **Anub'rekhan section (`:66-76`)** — replace the "non-tanks follow the kite at safe range" decision
  paragraph. Record: kite radius 35, stack 25 yd from the boss toward the room centre during the
  swarm window, spread suppressed ~3 s early from swarm 2 onward, one Impale eaten per swarm.
- **Anub'rekhan fact table (`:62`)** — the "zero warning" line now needs the caveat that the 90 s
  repeat is modelled after the first observed cast.
- **Noth section (`:183-208`)** — add the target-oscillation gap (engine fall-through into
  `dps assist`) and the ranged-burn-adds / melee-stay-on-boss split to the "gaps the rebuild had to
  solve" list.

---

## Files touched

| File | Change |
|---|---|
| `src/Ai/Raid/Naxx/NaxxMultipliers.cpp` | Noth: whole-fight assist kill. Anub: swarm-formation gate, melee kill |
| `src/Ai/Raid/Naxx/Action/NaxxActions_Noth.cpp` | Ranged/melee ground-phase target split |
| `src/Ai/Raid/Naxx/NaxxBossHelper.h` | `KiteRadius` 45→35, swarm constants, swarm clock + accessors |
| `src/Ai/Raid/Naxx/Action/NaxxActions_Anubrekhan.cpp` | `TakeSwarmStack`, swarm dispatch, slot helper, throttle bypass |
| `src/Ai/Raid/Naxx/Action/NaxxActions.h` | Declare `TakeSwarmStack` |
| `src/Ai/Raid/Naxx/NaxxTriggers.cpp` | Trigger reads `IsSwarmFormation()` |
| `docs/raids/naxxramas.md` | Anub + Noth sections |

No new files, no registration sites — every action, trigger and multiplier already exists and is
wired.

## Verification

Static (what can be done here):

1. Re-read each edited hunk and confirm the includes are already present — `MeleeAction` comes in via
   `NaxxMultipliers.cpp`'s existing includes (it already `dynamic_cast`s it for Noth), `<algorithm>`
   for `std::find` is already reachable through `NaxxBossHelper.h`.
2. Grep that `IsLocustSwarmActive()` has no callers left outside `AnubrekhanBossHelper` itself:
   `rg "IsLocustSwarmActive" src/`.
3. Confirm `KiteRadius` has no other consumer than `AnubrekhanPositionAction`'s ctor:
   `rg "KiteRadius" src/`.

**This module cannot be compiled headless in this environment** — the build has to happen on your
side. Expect to build the full core with the module.

In-game, 10-man Naxx:

4. **Noth ground phase** — pull, watch the first Plagued Warrior wave at t≈14 s. Ranged bots should
   all land on the same warrior (lowest GUID) and stay on it until it dies; melee should never leave
   Noth. Nothing should be visibly re-targeting between ticks. Confirm with `.bot debug` /
   `LogAction` output that `dps assist` is logged as `USELESS`, not `OK`.
5. **Noth blink window (25-man)** — at ground-phase t≈26/56/86 s: melee must leave Noth for the adds
   for ~5.5 s and return to Noth the moment it closes; ranged keep burning adds throughout. The Blink
   taunt must still fire, so the MT has to be on the boss when Cripple goes off — `noth choose
   target`'s MT pin (`NaxxActions_Noth.cpp:74-77`) still wins. Nobody should rip Noth off the MT
   after the reset, and with no adds up the melee should stand still rather than hit the boss.
6. **Anub'rekhan swarm 1** — reactive path. At the first `EMOTE_LOCUST`, non-tanks should collapse
   within a tick or two to a tight pile ~25 yd from the boss, MT should start the 35 yd circle, no
   swarm damage on anyone but the MT.
7. **Anub'rekhan swarm 2** — predictive path. The collapse must start ~3 s *before* the emote.
8. **Between swarms** — the Impale slot ring must come straight back (ranged at 24/28 yd, melee
   fanned 5 yd around their target) and stay there for the whole gap.
9. **Healer range** — watch that the MT is actually being healed through the swarm; that is the one
   thing the old 45 yd radius broke and the reason for 35.
