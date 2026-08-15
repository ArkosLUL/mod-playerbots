# Obsidian Sanctum — surviving the Twilight Realm exit

## Context

Round L of in-game defects on the Sartharion 3-drake strategy
(`modules/mod-playerbots/src/Ai/Raid/OS/`). The user builds and raids, then reports. Rounds A-K are
committed (HEAD `c3aeb5837`). The doc rewrite covering E-K is drafted and still unapplied.

Reported: bots leaving the Twilight Realm are caught by a passing Lava Tsunami.

### What actually happens

A shifted bot is in phase 16. Every grid search and every area-spell target search filters on
`InSamePhase`, so while shifted it can neither see a tsunami nor be damaged by one — `ClassifyTsunamiWave`
returns `None` in the realm, and `os tsunami corridor` is gated on `OnThePlatform`, which excludes shifted
bots anyway. Nothing in the realm reacts to a wave, and nothing needs to.

The danger is the **materialisation**. Entering and leaving are phase toggles in place, not teleports: GO
193988 is a goober whose `Data10` casts 57620 on the user, and the exit is
`DoRemoveAurasDueToSpellOnPlayers(SPELL_TWILIGHT_SHIFT)` at
`src/server/scripts/Northrend/ChamberOfAspects/ObsidianSanctum/instance_obsidian_sanctum.cpp:148`. The bot
reappears in phase 1 on exactly the Y it was fighting on, with no warning and no cast. A wave runs 12.0
yd/s (`speed_run` 1.71429 x 7.0) against the bot's 7.0, and the walk to clear ground is 8.5-16.5yd, so
reacting after the fact loses: a bot that appears 8yd ahead of a wave has about 0.7s.

**When the strip fires.** All three drakes share one refcount in the instance script:

```
portalCount = [Shadron's acolyte alive] + [Vesperon's acolyte alive] + [Tenebron mid-egg-cycle]
```

Each drake's `+1` is outstanding exactly while its acolyte lives, so the count cannot reach zero while one
is alive — **the shift is never stripped mid-kill**. `TwilightAddsAlive(bot) == false` is a necessary
condition for it. Tenebron's egg cycle (open every 60s, hatch 27s later, `ACTION_CLEAR_PORTAL` on hatch)
only decides *when* after the last add dies the strip lands: immediately, or up to ~27s later.

That leaves two exposures:

1. **The wait for the portal.** With the adds dead the bot is standing on the add's Y — Shadron's spot at
   534.66 is 2.66yd off the left line at 532, Vesperon's at 556.0 is 8.00yd off the right line at 564 —
   and the strip can land on any tick from here on.
2. **The walk to the portal.** Both portals sit at (3247.29, 529.80). That Y is 2.2yd off the left line at
   532, so under a left wave the portal is a death trap, in either direction. Neither
   `enter twilight portal` nor `exit twilight portal` checks the wave today, so a bot standing on clear
   ground will walk into a lane to click.

**Out of scope by decision.** A bot stripped in the same tick the last add dies is still on the add's Y.
Covering that means holding clear ground *during* the kill, which the user declined: nothing moves in the
realm while there is anything left to fight.

### Why a shifted bot still has to know about the wave

Both fixes need the wave side, and phase 16 hides it. The side is read off the two bots that are never in
the realm — the same thing a raid does over voice. There is no wave-blind alternative: the two patterns'
lines interleave 8yd apart against an 8.5yd lethal half-width, so no Y on the platform is safe under both.

---

## Change

All under `modules/mod-playerbots/src/Ai/Raid/OS/`. No new trigger or action classes, so
`OSStrategy.cpp`, `OSTriggerContext.h` and `OSActionContext.h` are untouched.

### 1. Borrowed eyes — `OSHelpers.cpp`

Internal to the anonymous namespace, placed directly above `CollectTsunamis`:

```cpp
// Everything a shifted bot knows about the platform comes through here. Phase 16 filters the tsunamis
// out of its own searches, so it asks someone who is still outside - which is what a raid does over
// voice. The two tanks are who it asks: ResolveAssignments skips every tank bar the second assist, so
// the main tank and the first off-tank are the only two bots that can never be in the realm.
// Coordinates are shared across phases, so their list still answers "can it reach me" against the
// shifted bot's own X.
Player* PlatformEyes(Player* bot)
{
    if (!bot)
        return nullptr;

    if (!HasTwilightShift(bot))
        return bot;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* fallback = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != OS_MAP_ID ||
            HasTwilightShift(member) || !InsideRoom(member))
        {
            continue;
        }

        if (PlayerbotAI::IsMainTank(member) || IsOffTank(member))
            return member;

        // Both tanks down. Anyone left outside answers the same question - the side is a property of
        // the wave, not of who is looking at it.
        if (!fallback)
            fallback = member;
    }
    return fallback;
}
```

`CollectTsunamis` searches from it instead of from the bot:

```cpp
void CollectTsunamis(Player* bot, std::list<Creature*>& out)
{
    if (Player* eyes = PlatformEyes(bot))
        eyes->GetCreatureListWithEntryInGrid(out, NpcId::FlameTsunami, ROOM_SEARCH_RADIUS);
}
```

For an unshifted bot `PlatformEyes` hands back the bot itself, so **platform behaviour is unchanged, bit
for bit**. `HasTwilightShift`, `IsOffTank` and `InsideRoom` are all declared in `OSHelpers.h`, so the
anonymous-namespace placement needs no reordering. Precedent for a non-bot searcher:
`UldBossHelper.cpp:553`, `RSActions.h:652`.

### 2. The wait window — `OSHelpers.{h,cpp}`

```cpp
// The window a shifted bot has to spend on ground it is safe to materialise on. Not a dodge - nothing in
// phase 16 can be touched by a wave. The raid's three drakes share one portal refcount and each drake's
// entry is outstanding while its acolyte lives, so the shift is never stripped mid-kill; once they are
// dead it lands on a tick nobody chooses, and the bot reappears on whatever Y it was standing on.
bool TwilightRealmWaveWait(Player* bot);
```

```cpp
bool TwilightRealmWaveWait(Player* bot)
{
    if (!bot || bot->GetMapId() != OS_MAP_ID || !HasTwilightShift(bot))
        return false;

    return !TwilightAddsAlive(bot) && ClassifyTsunamiWave(bot) != TsunamiWave::None;
}
```

Deliberately without the Y test: the window has to stay up after the bot arrives, or the suppression in
step 5 lifts and something nudges it back off the ground it just took.

`CorridorGroupFor` gains an explicit first branch. It is what the null-boss fallback already produced, but
it is load-bearing now and should not be implicit:

```cpp
    // In the realm the boss is unresolvable, and the melee and tank splits below are all about his cones,
    // none of which exist in phase 16. Everyone shifted takes the raid pair.
    if (HasTwilightShift(bot))
        return CorridorGroup::Raid;
```

So the realm hold is 551.0 under a left wave and 535.5 under a right one — both already proven clear, and
both a short walk from the portal at 529.80 (21.2yd and 5.7yd on Y; the action keeps the bot's X).

### 3. The hold — `OSTriggers.cpp`

`OsTsunamiCorridorTrigger::IsActive` grows a shifted branch ahead of the existing platform one, which is
otherwise untouched:

```cpp
    if (HasTwilightShift(bot))
    {
        if (!TwilightRealmWaveWait(bot) || WaveClearsY(bot->GetPositionY(), ClassifyTsunamiWave(bot)))
            return false;

        return std::abs(bot->GetPositionY() - SafeCorridorY(bot)) > CorridorToleranceFor(bot);
    }
```

`OsTsunamiCorridorAction` is reused unmodified — it keeps the bot's X and moves to `SafeCorridorY`, which
is exactly right here. It sits at `ACTION_EMERGENCY` against `exit twilight portal` at `ACTION_RAID + 1`,
so while the hold is up the bot does not walk to the portal at all; when the wave clears, the hold releases
and the exit action takes the tick.

### 4. Portal gates — `OSActions.cpp`

`EnterTwilightPortalAction::Execute`, after the portal lookup:

```cpp
    // The portal is a fixed coordinate 2.2yd off the left line at 532, so a bot standing on clear ground
    // will still walk into a lane to click it. Classifying against the bot's own X is enough: a wave that
    // has passed the bot is running at 12 yd/s against its 7, so it sweeps the ground between them and is
    // gone before the bot arrives.
    if (!WaveClearsY(portal->GetPositionY(), ClassifyTsunamiWave(bot)))
        return false;
```

`ExitTwilightPortalAction::Execute` takes the same two lines against `GoId::NormalPortal`, which is
statically spawned in phase 16 on the same coordinate (world DB `gameobject` guid 268052). The
classification reaches through the phase wall via step 1.

### 5. Suppression — `OSMultipliers.cpp`

The realm is outside `Snapshot()` — the boss is unresolvable in phase 16, so `encounterActive` is false and
every existing rule is skipped. Without a rule of its own the hold oscillates: `MoveTo` returns false on
the tick after it issues its move (`IsDuplicateMove`), and with the adds dead the bot is idle and stacked,
so `MoveRandomAction` and `MoveOutOfCollisionAction` are both live to take the tick.

Add to the anonymous namespace:

```cpp
// Everything that can move a bot on its own. The realm rule below has no snapshot to lean on, so it names
// the lot rather than the in-combat subset.
bool IsAnyMover(Action* action)
{
    return IsGenericMover(action) || IsWanderMover(action) ||
           dynamic_cast<MoveOutOfCollisionAction*>(action) ||
           dynamic_cast<MoveOutOfEnemyContactAction*>(action);
}
```

and at the top of `GetValue`, ahead of the `encounterActive` bail:

```cpp
    if (TwilightRealmWaveWait(bot) && IsAnyMover(action))
        return 0.0f;
```

Scoped to the window rather than to being shifted, so a bot in the realm with an acolyte still up keeps
every generic mover it has today.

---

## Files

| file | change |
|---|---|
| `OSHelpers.h` | declare `TwilightRealmWaveWait`; note on `ClassifyTsunamiWave` that it now answers in the realm |
| `OSHelpers.cpp` | `PlatformEyes` reading off the two tanks; `CollectTsunamis` searches from it; `TwilightRealmWaveWait`; explicit shift branch in `CorridorGroupFor` |
| `OSTriggers.cpp` | shifted branch in `OsTsunamiCorridorTrigger::IsActive` |
| `OSActions.cpp` | wave gate in both portal actions |
| `OSMultipliers.cpp` | `IsAnyMover`; realm suppression rule |

## Docs

The E-K rewrite is still unapplied, so round L folds into the same pass: the realm exit being a phase strip
on a shared refcount rather than a choice, the borrowed-eyes relay, the wait on clear ground once the adds
are dead, the portal's own Y, and the residual same-tick case that is deliberately not covered. Presented
as a unified diff with the word delta measured from the files, applied only on approval.

## Verification

**Static**

1. `python apps/codestyle/codestyle-cpp.py` — no new findings under `src/Ai/Raid/OS/`, and no line over 110.
2. Grep `PlatformEyes` — `CollectTsunamis` is its only caller, so no other search silently changes phase.
   Re-read `ResolveAssignments` and confirm the main tank and first off-tank are still skipped, since the
   relay leans on them being outside the realm.
3. Grep `CollectTsunamis` — still only reached through `ClassifyTsunamiWave`.
4. Trigger and action name parity unchanged at 16/16/16 against 15 action creators plus the documented
   `rear flank` borrow.
5. Confirm no platform-gated caller of `ClassifyTsunamiWave` can now fire for a shifted bot:
   `OsAvoidTwilightFissureAction`, `OsDrakeLandingPositionAction`, `OsSartharionFlankAction`,
   `OsDrakeRearAction` and `OffTankAnchor` all sit behind triggers that require `OnThePlatform` or an
   off-tank, and `SartharionMultiplier::Snapshot` still bails on `encounterActive`.

**In-game**

- With an acolyte still alive, realm bots behave exactly as today — they fight it on its spot and nothing
  moves them, wave or no wave.
- With the adds dead and a wave up, they step to the raid corridor keeping their X — 551 under a left wave,
  535.5 under a right one — and **wait there** rather than walking onto the portal at Y 529.80. A bot
  stripped during the wait appears on the corridor and takes nothing.
- When the wave clears they walk to the portal and click, and the realm empties as it does today.
- On the platform: nothing changes except that a bot will no longer walk to the portal, in either
  direction, while a left wave can still reach X 3247 — a delay of at most ~6s.
- Known residual, by decision: a bot stripped in the same tick the last add dies is still on the add's Y.
- Round E-K behaviour still holds: the pull drag and settle, the off-tank on his drake's spot and never
  swinging at Sartharion, melee tracking a drake on X while the corridor owns Y, no mage Blinks or hunter
  Disengages, nobody east of X 3268, burst at Tenebron 70%.

**Not in scope:** no build (the module cannot be compiled headless here) and no git operation — nothing is
committed without an explicit instruction naming the command.
