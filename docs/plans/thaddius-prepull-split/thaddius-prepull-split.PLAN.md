# Thaddius — pre-pull raid split on room entry

## Context

Thaddius phase 1 requires the raid to split in two, one half per add (Stalagg / Feugen), because
each add must stay pinned at its own tesla coil and both must die within ~5s of each other.

That split logic already exists and works — `ThaddiusBossHelper::ComputeAssignedToPrimarySide`
(`src/Ai/Raid/Naxx/NaxxBossHelper.h:2576`) does an even per-role parity split, honours RTI marks,
and `ThaddiusAttackNearestPetAction` drives everyone to their side's positions.

The problem: none of it runs until combat starts. Two deliberate guards in
`src/Ai/Raid/Naxx/Action/NaxxActions_Thaddius.cpp:26-29` and `:43-44` block the action unless the
bot is already in combat or the main tank has engaged a pet. So the raid walks into the room as one
blob and only scrambles into position after the pull, which costs the opening seconds of a fight
that is on a 5-minute enrage.

Goal: when bots enter the Thaddius room out of combat, they walk to their assigned side and wait
there, so the pull starts from a correct formation. The existing combat-phase behaviour must be
left byte-for-byte unchanged.

## Key facts established during investigation

These are non-obvious and drive the design — do not re-derive them.

1. **Raid strategies already run out of combat.** `PlayerbotAI::ApplyInstanceStrategies`
   (`src/Bot/PlayerbotAI.cpp:1639`) maps map id 533 → `"naxx"` and adds it to *both*
   `BOT_STATE_COMBAT` and `BOT_STATE_NON_COMBAT` engines (`:1777-1778`). `RaidNaxxStrategy` does not
   override `GetType()`, so it is `STRATEGY_TYPE_GENERIC`. Nothing filters raid triggers by bot
   state — an out-of-combat trigger just works. No new registration hook is needed.

2. **`AI_VALUE2(Unit*, "find target", "...")` is unusable pre-pull.** `FindTargetValue::Calculate`
   (`src/Ai/Base/Value/TargetValue.cpp:158-183`) only walks the bot's own threat list, which is
   empty out of combat. The out-of-combat-safe lookup is
   `GetFirstAliveUnitByEntry(botAI, entry)` (`src/Ai/Raid/RaidBossHelpers.cpp:224`), backed by
   `"possible targets no los"` — a real grid search, hostile-only, range `sightDistance`
   (config default **100 yd**, `src/PlayerbotAIConfig.cpp:112`).

3. **`ThaddiusBossHelper::UpdateBossAI()` must not be touched.** It calls `Reset()` on every
   out-of-combat tick (`NaxxBossHelper.h:2353`). More importantly, `ThaddiusGenericMultiplier`
   (`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:143`) keys off `helper.IsPhasePet()` to zero
   `FollowAction`. If entry-based resolution were added inside `UpdateBossAI`, `IsPhasePet()` would
   become true for any bot within 100 yd of the live adds, and follow-master would break across a
   large chunk of the Construct Quarter. **Add a separate resolution path instead.**

4. **Helper instances are per-object.** Every trigger, action and multiplier owns its own
   `ThaddiusBossHelper helper;` member (see `NaxxTriggers.h:233`, `NaxxActions.h:153`,
   `NaxxMultipliers.h:46`). State written by the new pre-pull path cannot leak into the
   multiplier's instance.

5. **Precedent for out-of-combat raid positioning is Ulduar Thorim.**
   `ThorimGauntletPositioningTrigger::IsActive()` (`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp:134`)
   has no combat check at all and gates on `bot->GetDistance(anchor) > 110.0f`. Its action uses
   `MoveTo(..., MOVEMENT_COMBAT)`. Follow the same shape. `ThorimFallFromFloorTrigger` (`:334`) is
   the precedent for the Z-based "bot fell off the platform" bailout used below.

6. **Geometry.** Coil spots `tankPosStalagg (3436.14, -2919.98)` and
   `tankPosFeugen (3522.94, -3002.60)`, `tankPosZ = 312.61`. Ranged standoff spots
   `rangedPosStalagg (3441.01, -2942.04)` and `rangedPosFeugen (3500.45, -2997.92)` sit
   **~22.6 yd and ~23.0 yd** from their respective coils. A level-83 elite's aggro radius against a
   level-80 player is ~23 yd, so parking bots on the existing ranged spots pre-pull would very
   likely pull the encounter. Staging must be backed off.

7. **Movement is navmesh-pathed, but a bad destination fails silently.**
   `MoveTo(..., exact_waypoint = false)` goes through `SearchForBestPath`
   (`MovementActions.cpp:243`, impl at `:1758`) → real `PathGenerator`. Bots do **not** walk a
   straight line into the slime. But `docs/engine/pitfalls.md:49-56` documents the trap: an off-mesh
   destination, or a Z outside the height band, yields `INVALID_HEIGHT` and `MoveTo` returns
   **false with no movement at all**; and with `normal_only = false` `SearchForBestPath` returns the
   last unvalidated `result` rather than failing. Raw extrapolated geometry is named in that doc as
   exactly the shape that produces off-mesh points — which is what a naive
   "ranged spot + 10 yd outward" would be over the Thaddius slime pit. Handled by the backoff ladder
   below.

8. **Known limitation, no fix required.** `Engine::ProcessTriggers` (`src/Bot/Engine/Engine.cpp:472`)
   skips any trigger whose first handler relevance is `< 100` when the bot is `minimal`
   (`!AllowActivity()`). `ACTION_RAID` is 60, so throttled/inactive bots run no raid triggers —
   this already applies to every raid trigger in the module and is out of scope here.

## How the split balances (answers a question raised during review)

`ComputeAssignedToPrimarySide` splits by **head count per role, not by throughput**: a DPS-only walk
and a healer-only walk each assign `index % 2 == 0` to the primary side, so 16 DPS → 8/8 and 6 DPS →
3/3, with tanks going MT → primary, first off-tank → secondary, then alternating. Gear and spec are
not weighed, so the two sides will not do equal damage.

That is deliberate and already solved downstream by a different mechanism: `PetSyncSuppress`
(`NaxxBossHelper.h:2702`), applied by `ThaddiusGenericMultiplier` (`NaxxMultipliers.cpp:180-190`).
Once either pet drops below `SYNC_WINDOW_PCT` (30%), non-tank `MeleeAction` and non-healing
`CastSpellAction` are zeroed on whichever pet is more than `SYNC_BALANCE_MARGIN` (5%) ahead, with a
hard floor at `SYNC_HARD_FLOOR_PCT` (5%) and free-burn for both once under `SYNC_FLOOR_RELEASE_PCT`
(8%). Tanks and healing are never suppressed. So a lopsided split kills more slowly but both deaths
still land inside the ~5s revive window. **This change does not affect kill-time balance either way**
— it only removes the opening scramble.

### Per-side role coverage

Parity over a role-only walk is even to ±1, so **both sides already get at least one healer whenever
the raid has two or more healers** (2 → 1/1, 3 → 2/1, 4 → 2/2), and likewise at least one tank
whenever there are two or more tanks (MT → primary, first off-tank → secondary, then alternating).
A raid with exactly one healer, or exactly one tank, cannot satisfy the encounter at all — the two
staging spots are ~82 yd apart, well outside heal range, so no midpoint helps. In that case the lone
healer/tank goes to the primary (MT) side, which is the current behaviour and is kept.

There is one way a side can still end up unhealed with enough healers in the raid: **role
misclassification**. The healer walk skips anyone `botAI->IsTank(member)` returns true for
(`NaxxBossHelper.h:2632`), and `IsTank`/`IsHeal` resolve from the bot's *strategy set* for bots but
from spec tab for real players (`PlayerbotAI.cpp:2265, 2303`). A healer bot that also carries the
tank strategy drops out of the healer walk, and the remaining healers can then land lopsided.
The implementation below adds a cheap safety net for exactly that case.

Pre-existing wart, noted not fixed: the parity walks skip dead members, so a mid-fight death flips
the side of everyone after that player in group order. Harmless pre-pull (nobody is dead), but it is
why the split can look unstable later in phase 1.

## Decisions (confirmed with the user)

- Staging points are the existing ranged spots **pushed further from their coil**, computed at
  runtime from the two existing coordinate pairs, via a **validated backoff ladder** (below). No new
  hardcoded coordinates.
- **Tanks stage with their side**, not at the coil. The existing "pin at the coil the moment we
  engage" logic (`NaxxActions_Thaddius.cpp:74-80`) stays the only thing that closes that gap.
- **No strategy mutation.** Do not remove `follow master`. Instead the trigger requires both the bot
  *and* the master/leader to be inside the room, so it self-releases when the master walks out.
- Staging runs only while **both adds are alive, not feigning, and not in combat**. Once either is
  engaged or down, the existing `thaddius phase pet` / transition / polarity nodes take over
  exactly as they do today.

## Implementation

### 1. `src/Ai/Raid/Naxx/NaxxBossHelper.h` — extend `ThaddiusBossHelper`

Add public constants and three methods. Keep `UpdateBossAI()` and everything it touches untouched.

```cpp
// Pre-pull staging. Anchor is the midpoint of the two coil spots; it sits over the gap
// between the platforms, it is only ever used as a distance reference.
static constexpr float ROOM_ANCHOR_X = 3479.54f;
static constexpr float ROOM_ANCHOR_Y = -2961.29f;
static constexpr float ROOM_RADIUS   = 80.0f;   // tune in-game
// Thaddius' platform is at 304, the coils at 312.6; below this we are in the slime pit.
static constexpr float ROOM_FLOOR_Z  = 295.0f;
static constexpr float PREPULL_ARRIVED = 3.0f;

// Tried longest-first. 0 falls back to the plain ranged spot, which the live phase-1 code
// already paths to successfully every pull, so the ladder always has a reachable rung.
static constexpr float PREPULL_BACKOFF_LADDER[] = {10.0f, 7.0f, 4.0f, 0.0f};

// Out-of-combat pet resolution. "find target" needs threat, so it is null before the pull -
// go through the grid search instead. Deliberately NOT folded into UpdateBossAI(): that would
// make IsPhasePet() true for the multiplier and kill follow-master across the whole wing.
bool ResolvePetsPrepull();

bool IsInThaddiusRoom(WorldObject const* who) const;

// Ranged spot pushed `backoff` yd directly away from the coil.
std::pair<float, float> PrepullGetStagingPos(Unit* pet, float backoff) const;
```

- `ResolvePetsPrepull()` — set `stalagg` / `feugen` via
  `GetFirstAliveUnitByEntry(botAI, NPC_STALAGG)` / `NPC_FEUGEN`, falling back to the existing
  `ResolveFromIcon` path if marks are set. Return true only when **both** resolved and neither
  `IsDownOrFeigning`. Does not clear `_unit`.
- `IsInThaddiusRoom(who)` — `who && who->GetMapId() == NAXX_MAP_ID &&
  who->GetPositionZ() > ROOM_FLOOR_Z && who->GetExactDist2d(ROOM_ANCHOR_X, ROOM_ANCHOR_Y) <= ROOM_RADIUS`.
  The Z term doubles as the slime-pit bailout: a bot that falls in stops matching, the trigger
  drops, and normal follow-master pulls it back out.
- `PrepullGetStagingPos(pet, backoff)` — take `PetPhaseGetPosForRanged(pet)` and
  `PetPhaseGetPosForTank(pet)`, normalise `ranged - coil`, extend by `backoff`. Guard a near-zero
  length by returning the ranged spot unchanged.

Add the `RaidBossHelpers.h` include to `NaxxBossHelper.h` if it is not already reachable there.

### 1b. `ComputeAssignedToPrimarySide` — guarantee a healer on each side

Applies to the combat path too, not just pre-pull, since both go through the same method. It only
ever fires when a side would otherwise be left with zero healers, so it is strictly a repair.

Add a private helper and call it from the healer branch instead of returning the parity result
directly:

```cpp
// Parity already splits healers evenly to +-1, but it only sees members the healer walk accepts -
// a healer carrying the tank strategy drops out of it and can leave one add unhealed. Repair that
// by moving the last healer of the over-full side across. Every bot walks the group in the same
// order, so they all reach the same answer without communicating.
bool BalanceHealerSides(Player* player, bool parityResult);
```

Implementation:

1. Collect healers in group order into a `std::vector<Player*>` using the same accept test as the
   healer walk (alive, `!IsTank`, `IsHeal`).
2. If fewer than 2, return `parityResult` unchanged — nothing to balance.
3. Compute each healer's parity assignment, count primary and secondary.
4. If both counts are non-zero, return `parityResult` unchanged.
5. Otherwise flip the **last** healer in group order to the empty side, and return the adjusted value
   for `player`.

Deterministic and group-order stable, so all bots agree within a tick. Cost is one ≤40-entry walk,
and the self result is already memoised per tick by `IsAssignedToPrimarySide` (`:2561`).

Do **not** add the equivalent for tanks: the tank branch already yields ≥1 per side for ≥2 tanks,
and a one-tank raid cannot pin both coils regardless.

### 2. `src/Ai/Raid/Naxx/NaxxTriggers.h` + `.cpp` — new trigger

```cpp
class ThaddiusPrepullSplitTrigger : public Trigger
{
public:
    // 2-tick interval: the grid-search resolution is not worth running every tick out of combat.
    ThaddiusPrepullSplitTrigger(PlayerbotAI* ai) : Trigger(ai, "thaddius prepull split", 2), helper(ai) {}
    bool IsActive() override;

private:
    ThaddiusBossHelper helper;
};
```

`IsActive()`, cheap checks first:

1. `bot->GetMapId() == NAXX_MAP_ID` and `!bot->IsInCombat()`.
2. `helper.IsInThaddiusRoom(bot)` — also rejects a bot that has fallen into the slime.
3. Master (or, if `botAI->GetMaster()` is null, the group leader) resolved **and**
   `helper.IsInThaddiusRoom(master)`.
4. `helper.ResolvePetsPrepull()`.
5. Neither `GetStalagg()` nor `GetFeugen()` is `IsInCombat()` — the release rule.

### 3. `src/Ai/Raid/Naxx/Action/NaxxActions.h` + `NaxxActions_Thaddius.cpp` — new action

```cpp
class ThaddiusPrepullSplitAction : public MovementAction
{
public:
    ThaddiusPrepullSplitAction(PlayerbotAI* ai) : MovementAction(ai, "thaddius prepull split"), helper(ai) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    ThaddiusBossHelper helper;
};
```

- `isUseful()` — mirror the trigger's gate (own helper instance, so it must resolve for itself).
- `Execute()`:
  - `Unit* pet = helper.GetPetForSide(helper.IsAssignedToPrimarySide(bot));` — this is the *whole*
    point of the change: it reuses the existing marks-aware, per-role-parity split verbatim.
  - Walk `PREPULL_BACKOFF_LADDER` longest-first. For each rung, compute
    `helper.PrepullGetStagingPos(pet, backoff)`; if the bot is already within `PREPULL_ARRIVED` of it,
    **return false** so lower-priority actions still run (staged bots can buff, eat and drink).
    Otherwise call

    ```cpp
    MoveTo(533, pos.first, pos.second, helper.tankPosZ,
           false, false, /*normal_only=*/true, /*exact_waypoint=*/false,
           MovementPriority::MOVEMENT_COMBAT);
    ```

    and return true on the first rung that succeeds.
  - `normal_only = true` is the important bit: per fact 7 it makes an off-mesh rung fail cleanly
    (`INVALID_HEIGHT`) instead of handing back an unvalidated path that could walk a bot off the
    ledge. The ladder then tries the next, shorter rung, and the 0-yd rung is the plain ranged spot
    that the live combat code already reaches every pull — so the ladder cannot end with the bot
    frozen at an unreachable point.
  - Return false if every rung fails, so nothing else is starved.

### 4. Registration — four one-line sites

- `src/Ai/Raid/Naxx/NaxxTriggerContext.h` — `creators["thaddius prepull split"]` + static factory
  (match the block at `:27-31` / `:85-89`).
- `src/Ai/Raid/Naxx/NaxxActionContext.h` — same, matching `:28-31` / `:85-88`.
- `src/Ai/Raid/Naxx/NaxxStrategy.cpp` — in the Thaddius block near `:125`:

```cpp
triggers.push_back(new TriggerNode("thaddius prepull split",
    { NextAction("thaddius prepull split", ACTION_RAID) }
));
```

`ACTION_RAID` (60) beats `follow` (1.0) and `return to stay position` (`ACTION_MOVE`, 30), and sits
below every combat-phase Thaddius node, so it can never outrank real fight behaviour.

- No CMake change — `NaxxActions_Thaddius.cpp` and `NaxxTriggers.cpp` already build.

### Explicitly out of scope

- Do not relax the guards at `NaxxActions_Thaddius.cpp:26-29` and `:43-44`. They stop RTI marks alone
  from starting the fight and remain correct; the new node sits alongside them.
- Do not touch the dead-member parity instability noted above; it is pre-existing and only affects
  mid-fight.

## Verification

The module cannot be compiled headless from this environment, so steps 2 onward are an in-game
hand-off.

1. Build the module.
2. Form a raid, teleport into Naxxramas, confirm `ApplyInstanceStrategies` picked up `"naxx"` (or
   force it with the `.naxx` chat shortcut, `src/Ai/Base/Actions/ChatShortcutActions.cpp:250`).
3. Walk the raid up to Thaddius **without marking anything**. Expected: on crossing the room
   threshold the bots break into two roughly even clumps — ≥1 tank, ~half the healers and ~half the
   DPS at each staging point — and hold there.
3b. Re-run step 3 with 2, 3 and 4 healers in the raid. Expected **≥1 healer on each side every
   time** (1/1, 2/1, 2/2). Then give one healer bot the tank strategy as well and repeat — this is
   the misclassification case `BalanceHealerSides` exists for, and the empty side must still get a
   healer.
4. **Watch the routing, this is the main in-game unknown.** Bots assigned to the far side have to
   cross the room. Check that (a) nobody ends up in the slime, (b) nobody freezes mid-room having
   silently failed every ladder rung, (c) crossing bots do not brush an add's ~23 yd aggro radius.
   If (c) fails, the fallback is to stage only bots already on their assigned side and let the rest
   join at the pull.
5. Confirm **no add pulls** while staged. If either add aggros on arrival, lengthen the first ladder
   rung.
6. Deliberately jump a bot into the slime pit. Expected: `IsInThaddiusRoom` fails on Z, the trigger
   drops, and follow-master recovers it — no re-issued staging move while it is down there.
7. Walk the master back out of the room. Expected: the trigger drops and bots resume following
   normally, with no leftover held position.
8. Re-enter, mark the adds skull + cross. Expected: the same split with the marked pets respected
   (`GetPetForSide` prefers marks).
9. Pull. Expected: phase 1, the transition platform jump and the polarity split all behave exactly
   as before this change — the fight simply starts from formation.
10. Wipe and let the adds reset. Expected: bots re-stage once both adds are alive and out of combat.
11. Record the outcome in the Thaddius section of `docs/raids/naxxramas.md` (repo convention for raid
    findings).

## Implementation notes — where the build deviated from the plan

Three things changed once the code was written. All are in the shipped diff.

1. **Section 1b (`BalanceHealerSides`) was dropped as dead code.** Parity over the healer-only walk
   assigns 0,1,2,3 → primary,secondary,primary,secondary, so `primaryCount` can never be 0 or equal
   to the healer count once there are two healers — the repair branch was unreachable by
   construction. The misclassification story does not produce a lopsided split either: a healer that
   also carries the tank strategy is claimed by the *tank* branch, which sends the first off-tank to
   the secondary side, so that side still has someone who can heal. Replaced with a comment
   documenting the invariant at `NaxxBossHelper.h:2682`.

2. **The backoff ladder does not retry on `MoveTo` failure.** `MoveTo` also returns false for
   duplicate and pending moves, so retrying the next rung would have marched bots down the ladder
   toward the coil one rung per tick and pulled the add. The rung is now chosen up front: take the
   longest backoff whose point has ground within 2 yd of `tankPosZ`
   (`bot->GetMapHeight(x, y, tankPosZ)`), which rejects points pushed off the platform edge over the
   slime, then issue exactly one `MoveTo`. `normal_only = true` is still passed.

3. **Pet resolution caches guids.** `sightDistance` is 100 yd but the two coils are ~120 yd apart, so
   a bot standing on its own staging spot loses the grid search on the far add, which would have
   dropped the trigger and handed it back to follow-master — an oscillation. `ResolvePrepullPet`
   now remembers each add's `ObjectGuid` once found and revalidates through
   `botAI->GetUnit(guid)` + `IsAlive()`, falling back to the grid search and then to RTI marks.
   Guids rather than raw pointers, so a despawn cannot leave a dangling deref. The pre-pull helper
   instances never call `UpdateBossAI()`, so `Reset()` never wipes this cache.

## Live test round 1 — geometry was wrong, corrected

Bots ended up in the slime under Feugen's platform, and aggroed the adds on arrival at the staging
spots. Both came from the same mistake: the backoff was measured against `tankPos*`/`rangedPos*`
instead of against where the adds actually stand.

**Real add spawns**, from `src/server/scripts/Northrend/Naxxramas/boss_thaddius.cpp:165-166`:

| | spawn | module's `rangedPos` | real gap |
|---|---|---|---|
| Stalagg | (3450.45, -2931.42, 312.091) | (3441.01, -2942.04) | **14.2 yd** |
| Feugen | (3508.14, -2988.65, 312.092) | (3500.45, -2997.92) | **12.0 yd** |

`tankPos*` are the *outer end* of each platform, 18-20 yd from the add — hence the bogus 22.6 yd
figure in fact 6 above. Every proven-walkable point on a platform is 12-20 yd from its add, inside
the ~23 yd aggro radius, so **there is no on-platform staging solution**. Frozen Rune gameobject
spawns in `acore_world` (`3440.38,-2901.05,313.89`, `3416.93,-2924.5,313.65`,
`3516.1,-3022.03,313.75`) confirm the platforms are only ~20 yd across.

The Feugen backoff direction (`rangedPos - tankPos`) also points toward the room centre, i.e. off
the platform edge into the pit — the slime landing.

### What replaced it

- The extrapolation and `PREPULL_BACKOFF_LADDER` are gone. Staging is now explicit measured
  coordinates per side (`ThaddiusBossHelper::PrepullStaging`), captured in game with `.gps`.
- Staging is on the **catwalk**, not the platforms. The catwalk runs ~5 yd below platform level, so
  each spot carries its **own Z** — the old `tankPosZ` ground check would have rejected it.
- `stalaggSpawn` / `feugenSpawn` constants added, and `IsPrepullStagingSafe` rejects any spot within
  `PREPULL_AGGRO_SAFE_DIST` (28 yd) of either add, or whose ground height does not match its own Z
  within 2 yd. An unmeasured or failing spot means that side simply does not stage — it keeps
  following the master. Fail-safe by construction.

### Measured staging spots (both in use)

| side | coordinate | to own add | to other add | to room anchor |
|---|---|---|---|---|
| Stalagg | `3422.97, -2959.07, 307.40` | 39.0 yd | 90.2 yd | 56.6 yd |
| Feugen | `3480.12, -3017.49, 306.88` | 40.2 yd | 91.0 yd | 56.2 yd |

Both clear `PREPULL_AGGRO_SAFE_DIST` comfortably. They sit 81.7 yd apart while the adds are 81.3 yd
apart, i.e. a clean parallel offset ~39-40 yd perpendicular off the add axis, on the opposite side
from the tesla coils (which are +40 yd on the same axis). The geometry is self-consistent, so if
further points are ever needed the room can be treated as mirror-symmetric about the add axis.

`ROOM_RADIUS` was raised 80 -> 100 because of these measurements: the Thaddius Door is 80.55 yd from
the room anchor, so a leader standing in the doorway failed the room check by half a yard and
nothing staged. The Construct Quarter trash before the room is 141 yd out, so 100 does not reach
back into it.

## Live test round 2 — three behaviour bugs, all fixed

Pathing to both staging spots was clean. Three separate problems remained.

### 1. Bots oscillated on the ramp after arriving

The staging action returns false once parked (deliberately, so bots can still buff and drink), which
hands the tick to `follow` at relevance 1.0. That drags the bot off the spot, the staging node fires
again, and the two fight over it.

The original plan explicitly chose *not* to suppress `follow master`, unlike the Thorim precedent.
That was wrong. Fixed with a new `ThaddiusPrepullMultiplier` (`NaxxMultipliers.cpp:143`) that zeroes
`FollowAction` while the split is held. A multiplier rather than the Thorim strategy-removal because
it is re-decided every tick from the same conditions the trigger uses and therefore cannot leak.

### 2. Bots stayed parked after the pull and never joined

`UpdateBossAI()` resolves the adds through the threat list (`find target`) or RTI icons. A bot parked
40 yd out has no threat, and in an unmarked raid no icon either, so `thaddius phase pet` never fired
for it. The staging feature created this by moving bots out of aggro range — before it, they were
close enough to be dragged into combat.

Fixed by adding an entry lookup to `UpdateBossAI()`, **accepted only when the resolved add is already
in combat**. That keeps the original constraint intact: idle adds still do not resolve, so
`IsPhasePet()` cannot trip `ThaddiusGenericMultiplier` and kill follow-master for bots walking past
the room.

### 3. Non-tanks changed sides mid-fight

This is the pre-existing wart called out in the original plan, now observed: the parity walks in
`ComputeAssignedToPrimarySide` skip dead members, so one death re-parities everyone behind that
player and they walk to the other add.

Two fixes, both scoped to non-tanks (**tanks keep the live answer on purpose** — Magnetic Pull
teleports them across and they must follow their add):

- `LatchedPrimarySide` freezes the first answer per player in a file-static GUID-keyed map with a
  mutex, matching the `HeiganBossHelper::PhaseStateFor` convention. Entries go stale after
  `SideLatchStaleMs` (30s) without a read, so the next attempt after a wipe re-splits, and the map
  self-prunes past 200 entries.
- `GetAssignedPetForBot` no longer falls back to the sibling or to `GetNearestPet` for non-tanks —
  both would send half the raid across the room the moment their own add drops into its feign.
  It returns null instead, so the bot simply holds.

Trade-off accepted on the second one: if the death sync fails badly and one add stays down while the
other is still high, that side's non-tanks now idle rather than crossing over. `PetSyncSuppress`
exists to stop that state arising.

### Refactor forced by these fixes

The gate was duplicated across the trigger, the action's `isUseful` and the new multiplier, and the
multiplier's copy was missing the leader-in-room check — which would have frozen bots in the room
with follow suppressed and nothing left to move them. All three now call one
`ThaddiusBossHelper::IsPrepullStagingActive()`.

### Known residual risk

A non-tank now depends on resolving **its own** add only, at ~40 yd from its staging spot — well
inside any sane `AiPlayerbot.SightDistance` (default 100). A heavily lowered sight distance would
leave that bot idle rather than falling back to the nearer add.
