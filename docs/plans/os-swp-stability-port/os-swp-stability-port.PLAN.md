# Port SWP movement/targeting discipline into Obsidian Sanctum

## Context

The Obsidian Sanctum strategy (`src/Ai/Raid/OS/`, key `"wotlk-os"`, map 615) has one real in-game
failure and a set of missing stability patterns that the Sunwell Plateau strategy
(`src/Ai/Raid/SWP/`) already solved. Step 7 additionally carries three raid-strategy changes the raid
asked for — portal dives held to the last drake, Sartharion above Lava Blaze for melee, and a 5s melee
hold after the pull — which are not stability ports. OS is 12 flat files with a 1158-line `OSHelpers.cpp` carrying all
geometry, state and target selection; SWP is ~13k lines split per boss with a documented
movement-priority discipline.

### The bug

**All 17 OS `MoveTo` sites issue at `MovementPriority::MOVEMENT_COMBAT`.**
`MovementAction::IsWaitingForLastMove` (`src/Ai/Base/Actions/MovementActions.cpp:951-963`) compares
with a **strict** `>`:

```cpp
if (priority > lastMove.priority)
    return false;
if (lastMove.lastdelayTime + lastMove.msTime > getMSTime())
    return true;
```

Failure sequence: on tick N, no wave is up, `os raid hold` issues a ~20yd X re-form and stamps a
~2.9s `MOVEMENT_COMBAT` lock (`MovementActions.cpp:228-235`, capped at
`AiPlayerbot.MaxWaitForMove = 5000`). On tick N+1 a wave spawns; `os tsunami corridor` runs first (it
sits at `ACTION_EMERGENCY`), but its `MoveTo` hits `COMBAT > COMBAT == false`, the lock is still live,
and the move is refused. The bot keeps walking to its old hold destination. The Flame Tsunami budget
from summon to damage is ~3.6s, so the ~2.9s lock covers most of it.

Strategy-level `ACTION_*` priority decides which action *runs*. It has no bearing on whether that
action's `MoveTo` is *accepted*. OS conflates the two.

### Second finding

`OSActions.h:17-20` states the anti-oscillation argument rests on "the duplicate-move guard already
inside `MoveTo`". That premise is false in OS. `IsDuplicateMove` (`MovementActions.cpp:939-949`)
requires the request within **0.01yd** of `lastMoveShort`, but every OS `MoveTo` passes
`bot->GetPositionZ()` — and the platform floor runs 58.6→59.6 (noted at `OSHelpers.h:271`), so a
re-issued "same" destination carries a different Z once the bot has moved. The pathfinding branch also
stores the **navmesh-resolved** Z (`MovementActions.cpp:265-266`), not the requested one. The guard
effectively never fires, and what actually throttles OS is the movement lock above.

### What OS already does well — do not undo it

`OSMultipliers.cpp` is careful, well-reasoned work. The author already thought hard about priority
collisions — including explicitly zeroing `AvoidAoeAction` because it "outranks every hold at
`ACTION_EMERGENCY`" — just at the multiplier layer rather than the movement-priority layer.
`OSHelpers.h` annotates nearly every constant with its measured in-game basis. Preserve both
properties: new constants get a stated basis, and multiplier rules keep their explanatory comments.

---

## Settled decisions

Agreed during planning. Do not re-litigate these.

| Decision | Outcome |
|---|---|
| Dodge-vs-dodge arbitration | Port SWP's mechanic-priority multiplier pattern **and** set `MOVEMENT_FORCED` — they fix different layers and both are required |
| Destination Z | Runtime `GetMapWaterOrGroundLevel` + `INVALID_HEIGHT` fallback + `CheckCollisionAndGetValidCoords`, not a surveyed constant — OS destinations are derived, not measured |
| Sequencing | File split lands **first**, as a pure no-behaviour-change move |
| Arc-slot helper | OS gets its **own local copy**. `RaidBossHelpers` unchanged, SWP left byte-identical |
| `MoveToClamped` contract | Stays `bool`. Candidate validation moves to the caller that builds the list |
| Multiplier shape | One `OsMechanicPriorityMultiplier` class, precedence **off-platform > tsunami > fissure** |
| New latch storage | All in `EncounterState`, which already re-arms on the right edges. No action-member latches |
| Reset node + tracker election | **Dropped.** Q13 removed their justification; an age sweep in `StateFor` covers what's left |
| Hysteresis bands | Hand-picked per pair with a stated basis, no shared ratio |
| Spread slots | Drake landings only. The raid line already self-spreads |
| Memoisation | Gated on timing measurement first |
| Heroic ids | Settle by DB query, not an in-game raid |
| Raid marking | **Not added.** OS stays marking-free |
| Portal entry gate | Dive **only once two drakes are dead** — Tenebron's and Shadron's realm cycles are skipped, Vesperon's is taken |
| Off-tank portal role | **Unchanged.** Assist-0 keeps drakes, then Lava Blazes. He never joins the portal squad |
| DPS priority order | Split by role: **melee** get Sartharion above Lava Blaze, **ranged keep today's order**. Tiers 1-7 identical |
| Melee pull hold | 5s from `fightStartMs`, melee DPS only. **Pets keep attacking** — only the player's own attacks and casts are held |
| `PlatformEyes` cross-instance filter (`OSHelpers.cpp:189` uses `GetMapId()` where `GetInstanceId()` is wanted) | **Out of scope.** Leave exactly as is; file separately |


---

## Step 1 — Settle the heroic ids by DB query

No code yet. `OSHelpers.h:25-55` pairs every entry with its `difficulty_entry_1` on the stated premise
that "the grid searches below match on entry, not on name". The code says otherwise: AzerothCore keeps
the **normal** entry on the spawned creature (`src/server/game/Entities/Creature/Creature.cpp:509` —
`SetEntry(Entry)`, with only `m_creatureInfo` mode-dependent), and `AllCreaturesOfEntryInRange` matches
`unit->GetEntry()` (`src/server/game/Grids/Notifiers/GridNotifiers.h:1510`). OS is the only place in
mod-playerbots that does this — `grep -r difficulty_entry src/` hits just that comment.

**Resolved: the `*H` constants are dead.** Three independent confirmations:

- `Creature::UpdateEntry` sets the normal entry with the comment saying so verbatim —
  `SetEntry(Entry); // normal entry always` (`Creature.cpp:509`), while only `m_creatureInfo` picks up
  `DifficultyEntry[diff - 1]` (`:492-494`).
- `AllCreaturesOfEntryInRange` matches `unit->GetEntry()` (`GridNotifiers.h:1510`), i.e. the normal
  entry, never the difficulty template.
- `acore_world` has no `creature` row for any `*H` entry. Map 615 spawns only 28860, 30449, 30451,
  30452 (plus Onyx Sanctum trash) at `spawnMask = 3`, so one row serves both difficulties. The
  templates exist — all 13 pairs resolve, named "... (1)" — but nothing ever spawns them, and the
  script summons adds by the normal ids (`NPC_ACOLYTE_OF_SHADRON = 31218`, `NPC_LAVA_BLAZE = 30643`).

Step 11 removes them.

## Step 2 — File split (pure move, no behaviour change)

`OSHelpers.cpp` at 1158 lines is the main obstacle to working on this strategy. OS has one boss, so
split by concern rather than SWP's per-boss layout:

```
OS/
├── OSStrategy.{h,cpp}          + one multiplier registration (Step 3)
├── OSTriggers.{h,cpp}          unchanged here
├── OSMultipliers.{h,cpp}       + OsMechanicPriorityMultiplier (Step 3),
│                               + OsHoldMeleeDpsAtPullMultiplier (Step 7)
├── OSActionContext.h           unchanged
├── OSTriggerContext.h          unchanged
├── OSHelpers.h                 umbrella include, so the 4 wiring sites do not churn
├── Util/
│   ├── OSData.h                NpcId / SpellId / GoId, OS_MAP_ID, coordinate constants
│   ├── OSGeometry.{h,cpp}      platform + range-marker clamp, wave classification, corridor
│   │                           selection, cone tests, fissure tests, arc-slot helper (Step 8)
│   └── OSEncounter.{h,cpp}     EncounterState, StateFor, ResolveAssignments, PriorityTarget,
│                               tank-defensive table, DrakesRemaining (Step 7)
└── Action/
    ├── OSActions.h             all action class decls (SWP keeps one header — follow that)
    ├── OSActions.cpp           OsPositioningAction::MoveToClamped
    ├── OSActions_Tank.cpp      main tank hold/drag/cooldown/shapeshift, off-tank hold, drake landing
    ├── OSActions_Raid.cpp      raid hold, tsunami corridor, fissure dodge, flank, drake rear,
    │                           return to platform, dps priority, tranquilize, redirect threat
    └── OSActions_Portal.cpp    portal enter/exit
```

Move the measured-value annotations with the code they document. Do not drop or summarise them.

Land this as its own commit with no behaviour change, so the diff for everything after it is readable.

## Step 3 — Movement layer (the actual fix)

Three changes plus one new multiplier.

**3a. Priority parameter.** `OsPositioningAction::MoveToClamped` (`OSActions.cpp:21-30`) hardcodes
`MOVEMENT_COMBAT`. Add `MovementPriority priority = MovementPriority::MOVEMENT_COMBAT` and pass
`MOVEMENT_FORCED` from the three emergency-band actions:

- `OsReturnToPlatformAction` (`ACTION_EMERGENCY + 2`)
- `OsAvoidTwilightFissureAction` (`ACTION_EMERGENCY + 1`)
- `OsTsunamiCorridorAction` (`ACTION_EMERGENCY`)

Holds, drake landing, flank and drake rear stay `MOVEMENT_COMBAT`.

Both portal actions use the `MoveTo(WorldObject*, distance, priority)` overload (`OSActions.cpp:542`,
`:563`) whose default is `MOVEMENT_NORMAL` (`MovementActions.h:40-41`) — below every hold, so the walk
to the portal is perpetually preempted. Pass `MOVEMENT_COMBAT` explicitly.

**3b. `OsMechanicPriorityMultiplier`.** Setting all three dodges to `FORCED` means they can no longer
preempt each other (`FORCED > FORCED` is false). Resolve that at the multiplier layer, following
`FelmystPrioritizeDemonicVaporKiteMultiplier` (`SWPMultipliers.cpp:433-456`) — which encodes precedence
inside its own predicate by deferring to fog when fog is live.

One new class in `OSMultipliers.{h,cpp}`, registered in `RaidOsStrategy::InitMultipliers`
(`OSStrategy.cpp:64-68`). Precedence **off-platform > tsunami > fissure**: off the platform is
unrecoverable, a tsunami is lethal, a Void Blast is survivable. Shape per mechanic: whitelist its own
action, return `1.0f` for non-movers, return `0.0f` for every other `MovementAction` while that
mechanic is live and no higher one is.

Keep it separate from `SartharionMultiplier` — that class is already ~140 lines of generic-mover
suppression and this is a distinct concern.

**3c. `lessDelay=true`** on all four `MoveTo` sites (`OSActions.cpp:28`, `:232`, `:244`, `:329`).
Every SWP `MoveTo` passes it; it subtracts `botAI->GetReactDelay()` (100ms default) from the stamped
lock.

**3d. Destination Z from the destination**, per `SWPEncounter_Felmyst.cpp:641-652`:

```cpp
float z = bot->GetMapWaterOrGroundLevel(x, y, bot->GetPositionZ());
if (z <= INVALID_HEIGHT)
    z = bot->GetPositionZ();
```

Follow SWP's split on the collision check's last argument: `true` (reject on failure) for the dodges,
`false` (accept clamped coords) for routine holds.

Rewrite the `OSActions.h:17-20` comment — it must state the actual mechanism, not the false one.

**Verify in-game before proceeding past this step.** This is the change that fixes an observable
failure and it is independently shippable.

## Step 4 — Hysteresis bands

Trigger release and action arrival use the same constant in three of four pairs
(`OSTriggers.cpp:71`/`OSActions.cpp:39`, `:155`/`:460`, `:158`/`:475`). The drake-rear pair is the only
one built with an intentional gap (140° gate vs 30° draw, `OSHelpers.h:234-236`) — generalise that.

Hand-picked per pair, each with its basis recorded in the header comment:

| Pair | Arrival | Release | Basis |
|---|---|---|---|
| Corridor Y | 2.0 (unchanged) | 3.5 | Stays well inside the 8.5yd tsunami kill half-width, leaving ~5yd margin |
| Tank hold | 1.0 (unchanged) | 2.0 | Tank spots are tight — drag corner and boss home are both measured points |
| Raid line X | 8.0 (unchanged) | 12.0 | X is not a lethal axis; the band is anti-pile, so a wide release is free |

## Step 5 — Ordered-fallback dodge destinations

**`OsAvoidTwilightFissureAction`** (`OSActions.cpp:42-149`) currently commits to the best-clearance
candidate even when every one is still inside `FISSURE_CLEAR_RADIUS` — i.e. it can move into the blast.
Restructure to SWP's tiered relaxation (`SWPEncounter_Felmyst.cpp:1036`):

```
tryCandidates(waveClear && fissureClear)
    || tryCandidates(fissureClear only)
    || tryCandidates(any that strictly increases clearance)
```

Within each tier, iterate candidates until one `MoveTo` returns `true`
(`SWPActions_Felmyst.cpp:317-327`) rather than committing to the first computed one.

`MoveToClamped` stays `bool`. Its `false` is now three-way ambiguous (already there / collision
rejected / lock blocked), so **pre-validate each candidate with `CheckCollisionAndGetValidCoords` while
building the list**, exactly as `TryGetFelmystFogSafeDestinations`
(`SWPEncounter_Felmyst.cpp:1132-1209`) does. A `false` from `MoveToClamped` then only ever means "not
this one, try the next".

**Both tank holds** (`OSActions.cpp:238-239`, `:323-324`) do `if (FissureBlocks(...)) return false;`
with no alternative, so the tank stands in the blast until it expires. Compute a displaced hold using
SWP's capped incremental backwards step (`SWPActions_Felmyst.cpp:56-71`), so the boss's frontal cone
does not swing through the raid:

```cpp
float const moveDist = std::min(2.25f, distToPosition);
// normalized step toward the goal, backwards = true
```

## Step 6 — Targeting hysteresis and corridor debounce

Both store state in `EncounterState` (`OSHelpers.cpp:100-121`), keyed by bot GUID where per-bot.
`StateFor` already re-inits on boss change, out-of-combat and staleness, so every latch re-arms on a
wipe for free — this is why no reset node is needed.

**6a. `PriorityTarget`** (`OSHelpers.cpp:1083-1110`) walks a flat tier table fresh every tick. Add:

- **Priority-index hysteresis** (`SWPActions_Muru.cpp:328-350`): keep the current target unless the
  candidate's tier index is *strictly* better.
- **Distance-margin switch within a tier** (`SWPActions_Muru.cpp:358-393`): only switch between two
  same-entry adds when the candidate is ≥10yd closer.

Both wrap the tier walk, so they apply to whichever of the two tables Step 7d selects. Tier indices
are only ever compared within one role's table — a melee bot and a ranged bot rank Sartharion
differently and neither should read the other's index.

Note: `SartharionDpsTrigger` (`OSTriggers.cpp:27-34`) does **not** call `PriorityTarget` — it only
checks `IsDps` + encounter-active — so there is no trigger/action divergence risk on the DPS path. The
off-tank path (`OffTankChargeFor`, `OSHelpers.cpp:703-711`) does derive a target in both trigger and
action; keep both going through one helper (`UldBossHelper.h:582` documents the every-tick retarget
that results when they disagree).

Also add `bot->SetSelection(target->GetGUID())` to `SartharionAttackPriorityAction`
(`OSActions.cpp:519-522`), for parity with both tank holds which re-assert selection for the reason
documented at `OSActions.cpp:158-161`.

**6b. Corridor-group debounce.** `CorridorGroupFor` (`OSHelpers.cpp:374`) is
`return bot->GetVictim() == boss ? Melee : Raid;`. A melee bot's home corridor jumps from Y 513 to
Y 535.5 — a 22.5yd walk — the instant its victim changes, e.g. when a drake dies and `PriorityTarget`
hands back Sartharion. Hold the group for **2000ms** after a flip: long enough to ride out the drake
death, short enough that a genuine role change lands within one wave cycle (25s).

## Step 7 — Requested behaviour changes

Three raid-strategy changes, none of them stability ports: hold portal dives to the last drake, put
Sartharion above Lava Blaze for melee, and keep melee off the boss for the first 5s. Independent of
Steps 3-6 and shippable on their own.

### Portal entry gate: last drake only

Today the portal squad dives the moment an acolyte is up
(`TwilightRealmWorthEntering`, `OSHelpers.cpp:276-281`, i.e. Sartharion immune or a torment on the
bot). The raid wants the drakes killed first and only the *last* realm cycle taken.

"All three drakes dead" cannot be the gate. In Twilight Zone (`isCalledBySartharion`) there is one
refcounted portal GameObject (`instance_obsidian_sanctum.cpp:119-152`): `ACTION_ADD_PORTAL` fires only
from a live drake's `EVENT_MINIBOSS_OPEN_PORTAL`, and at `portalCount == 0` the GO is `Delete()`d and
`SPELL_TWILIGHT_SHIFT` stripped raid-wide. Vesperon's `JustDied` calls `ACTION_CLEAR_PORTAL` outright
(`boss_sartharion.cpp:844`). With every drake dead there is no portal and no acolyte will ever spawn
again. So the gate is **two drakes dead, one still standing** — in call order that is Vesperon's cycle.

**7a. `DrakesRemaining(Player* bot)`** — new helper in `Util/OSEncounter.{h,cpp}`. Count alive drakes
by `GetCreatureListWithEntryInGrid(found, DRAKE_ENTRIES, ROOM_SEARCH_RADIUS)` filtered on `IsAlive()`
**only**. Do *not* reuse `FindUnitByEntries`' defaults: it requires `UNIT_FLAG_NOT_SELECTABLE` clear and
`InsideRoom`, and an uncalled drake is still circling with the flag set, so it would read as dead and
open the gate on the pull. `LandedDrakes` (`OSHelpers.cpp:662-677`) has the same two filters and is
likewise unsuitable here.

**7b. Latch it.** Store `portalGateOpen` in `EncounterState`, set once `DrakesRemaining(bot) <= 1` and
never cleared within a pull (`StateFor` re-arms it on the next pull for free, per the latch decision
above). Without the latch a single missed grid search yanks a bot mid-walk to the portal.

**7c. Gate the trigger.** `TwilightPortalEnterTrigger::IsActive` (`OSTriggers.cpp:240-251`) gets the
latch test alongside the existing conditions. Nothing else changes — the exit trigger, the squad
composition and `TwilightRealmWorthEntering` all stay as they are.

Consequence to record in the header comment: while Shadron and its acolyte live, Sartharion is immune
to every school and no one is dispatched to fix it. That is intended under this gate — the raid is on
drakes, and Shadron's own death strips `GIFT_OF_TWILIGHT_FIRE` from the boss
(`boss_sartharion.cpp:831-835`), just as Vesperon's strips both torments (`:845-846`). The cost is
torment healing while Vesperon lives, nothing more.

The off-tank is **not** added to the portal squad. Assist-0 keeps drakes and then Lava Blazes, which
spawn from Sartharion for the whole fight and which `OffTankChargeFor` already falls back to
(`OSHelpers.cpp:703-711`). Sending him in would hand the last drake to the raid.

### 7d. Sartharion above Lava Blaze, melee only

`PriorityTarget` (`OSHelpers.cpp:1083-1110`) is one flat table shared by every role. Melee want
Sartharion at tier 8 and Lava Blaze at 9; ranged keep today's order. Split it into two statics rather
than swapping indices at runtime — the heroic-id removal (Step 11) has to edit the entries anyway, and
two explicit tables are easier to get right than an index remap:

```cpp
static std::vector<std::vector<uint32>> const meleePriority  = { ..., Sartharion, LavaBlaze };
static std::vector<std::vector<uint32>> const rangedPriority = { ..., LavaBlaze, Sartharion };
```

Select on `PlayerbotAI::IsMelee(bot) && !PlayerbotAI::IsMainTank(bot) && !IsOffTank(bot)`, mirroring
the role test the melee triggers already use (`OSTriggers.cpp:37`, `:162`, `:185`). Tiers 1-7 are
identical in both — the acolytes still top the list unconditionally.

`OsRedirectThreatAction::GetThreatDumpTarget` (`OSActions.cpp:489-492`) also reads this, so a rogue
now redirects off the melee order and a hunter off the ranged one. That is the behaviour you want:
each redirects to the tank of the thing it is actually hitting.

Record the consequence: melee stop killing Lava Blazes. They still spawn on the Lava Strike timer and
the off-tank still picks them up, and ranged still have them at tier 8 — so in a ranged-light raid they
accumulate on the off-tank. That is a live thing to watch, not a reason to change the order.

### 7e. Melee DPS hold for 5s after the pull

Port `EredarTwinsHoldDpsAtStartMultiplier` (`SWPMultipliers.cpp:547-579`, declared
`SWPMultipliers.h:188-194`) as `OsHoldMeleeDpsAtPullMultiplier`, registered in
`RaidOsStrategy::InitMultipliers` (`OSStrategy.cpp:64-68`).

Drive the window off `EncounterElapsedMs(bot) < MELEE_DPS_PULL_HOLD_MS` (new, `5000`, in `OSData.h`).
Do **not** copy SWP's `eredarTwinsDpsHoldTimer` static map: `EncounterState.fightStartMs` already
stamps within a tick of the pull (`OSHelpers.cpp:135-144`), re-arms on a wipe, and needs no second
never-erased map — the exact thing Step 9 exists to clean up. Keep the new constant distinct from
`PULL_WINDOW_MS = 10000` (`OSHelpers.h:304`), which is a different window used by `RedirectTarget` and
`ShadronGone`; do not fold them together.

Shape, following SWP's whitelist-first idiom:

- `1.0f` for anything that is neither `AttackAction` nor `CastSpellAction`
- `1.0f` for `CastHealingSpellAction`
- `1.0f` for anyone who is not melee DPS — tanks, ranged and healers are untouched
- `1.0f` for `SartharionAttackPriorityAction` itself (it is an `AttackAction`,
  `OSActions.h:147-153`), so the pet branch keeps running
- `0.0f` otherwise, while the window is live

Movement is deliberately left alone. Melee walk to their flank position during the hold and are in
place when it releases.

**Pets are not held** — `CommandPetAttack` keeps firing every tick. So inside
`SartharionAttackPriorityAction::Execute` (`OSActions.cpp:504-523`) the hold has to skip only the
`Attack(target)` call, not the pet branch above it. Without that the bot's own auto-attack swings
through the whole window and the multiplier buys nothing, since `Attack()` is what sets the victim.

## Step 8 — Drake-landing spread

Every melee DPS targets `landing->X + OFFTANK_EAST_OFFSET` (`OSActions.cpp:265`) and nothing de-stacks
them, because `MoveOutOfCollisionAction` is zeroed for everyone (`OSMultipliers.cpp:213-217`).

Port SWP's centre-out arc slotting as an **OS-local** function in `Util/OSGeometry.{h,cpp}` —
`GetCenteredArcSlotAngleOffset` (`SWPEncounter_Brut.cpp:128-154`) handles odd counts (centre slot
first) and even counts (half-step pairs). Combine with GUID-sorted slot assignment
(`SWPEncounter_KJ.cpp:247-334`): sort eligible melee by `GetGUID()`, take the index, derive the angle.
Every bot computes the same layout independently, no leader election.

Do not touch `RaidBossHelpers` and do not repoint SWP's two copies.

The raid line is deliberately **out of scope**: `RAID_LINE_TOLERANCE_X = 8.0f` means a bot already
within 8yd does not move, and `OsRaidHoldAction` preserves X during wave dodges (`OSActions.cpp:460`
is Y-only), so existing spread survives every corridor swap. Convergence only affects bots >8yd out
during form-up, and lasts seconds.

## Step 9 — `StateFor` age sweep

The `static std::unordered_map<uint32, EncounterState>` in `StateFor` (`OSHelpers.cpp:124-148`) is
never erased, so it accumulates one entry per instance id ever visited for the process lifetime.

Drop entries older than `STALE_STATE_MS` on the existing locked pass — the function already holds the
mutex, and the map is small enough that a full scan is free. No new trigger, action, context
registration or strategy node.

## Step 10 — Measure, then memoise if warranted

`SartharionEncounterActive` appears in **15 of 17 triggers**, each running `GetSartharion` →
`GetCreatureListWithEntryInGrid(..., ROOM_SEARCH_RADIUS = 200.0f)` → `Cell::VisitObjects`. A single
`os main tank hold` tick costs roughly five 200yd sweeps (boss ×2, tsunami ×2 via `TankHoldX` and
`SafeCorridorY`, fissure ×1) before the other 16 nodes run.

The existence of `SartharionMultiplier::Snapshot` (`OSMultipliers.cpp:67-85`) is evidence the author
hit this, but not proof the *remaining* sweeps matter.

**Measure first:** accumulate elapsed time per tick in `SartharionEncounterActive` and
`ClassifyTsunamiWave`, `LOG_INFO` above a threshold, run a live 25H pull. Call volume alone won't
settle it — the question is whether ~125 sweeps/tick costs enough against everything else in a 25-bot
update.

**If warranted:** extend `EncounterState` with a `getMSTime()`-granularity memo for the boss GUID and
the tsunami classification (same granularity `Snapshot` already uses), and route
`SartharionEncounterActive`, `GetSartharion` and `ClassifyTsunamiWave` through it.

If not warranted, drop this step and say so in the docs update.

## Step 11 — Folded-in side bugs

**Heroic ids.** Given Step 1 confirms — and note Step 7 leaves two more places to edit,
`DrakesRemaining` reading `DRAKE_ENTRIES` and the second priority table — remove the `*H` constants, halve the tier vectors, and fix the
`OSHelpers.h:25-26` header comment that states the wrong premise. If Step 1 *disproves* it, leave
everything and correct the plan instead.

**Molten Fury is Lava-Blaze-only.** `TranquilizeTargetFor` scanned three tiers - blazes, then drakes,
then Sartharion - on the stated premise that "Molten Fury has no target conditions". It has one. Spell
60430 picks targets through `TARGET_UNIT_SRC_AREA_ENTRY` (`ImplicitTargetB = 7`, radius index 29 =
6yd), and `TARGET_CHECK_ENTRY` resolves against the `conditions` table, where 60430 carries exactly
one row: `SourceTypeOrReferenceId 13`, `SourceGroup 7` (all three effects),
`ConditionTypeOrReference 31` (`CONDITION_OBJECT_ENTRY_GUID`), `ConditionValue1 3` (`TYPEID_UNIT`),
`ConditionValue2 30643` - Lava Blaze - plus a `CONDITION_ALIVE` row. Nothing else in the room can be
enraged. Entry 30643 covers heroic too, per the Step 1 finding.

So two of the three tiers were dead and cost a 35yd grid sweep each, every hunter tick. Collapsed to
the blaze scan, with the real basis in the comment. The GUID sort stays: it is what hands two hunters
different blazes.

**Ungated triggers.**
- `OsTranquilizeTrigger` (`OSTriggers.cpp:139-145`) has no `SartharionEncounterActive` gate, so every
  hunter runs a 35yd sweep every tick anywhere on map 615, including trash and pre-pull. Add the
  gate.
- `PortalSquadMember` (`OSHelpers.cpp:916-921`) calls `StateFor` with no encounter gate; `StateFor`'s
  own comment (`OSHelpers.cpp:136-139`) acknowledges this is why the staleness check cannot stand
  alone. Gate the call so `ResolveAssignments` does not re-run out of combat.

## Step 12 — Update `docs/raids/obsidian-sanctum.md`

Already stale; this change widens the gap. Fix alongside the code:

- Doc `:414-421` describes the burst window as a latch on **Tenebron at 70%**
  (`BURST_WINDOW_TENEBRON_PCT`); `OSMultipliers.h:37` says Tenebron at half health; the code
  (`OSHelpers.cpp:1000-1014`) latches on `LandedShadron(bot) || ShadronGone(bot)` — no Tenebron, no
  health percentage. The named constant does not exist in the source.
- Doc `:436` puts `os sartharion flank` and `os drake rear` both at `ACTION_MOVE + 5`;
  `OSStrategy.cpp:57-60` has drake rear at `+6`, flank at `+5`.
- Doc `:430-434` omits `os main tank cooldown` (`ACTION_RAID + 7`) and `os tank shapeshift` (`+6`).

Add the new movement-priority contract — which nodes are `FORCED` vs `COMBAT`, why the multiplier
exists alongside them, and the fact that `ACTION_*` priority does not govern whether a `MoveTo` is
accepted. That last point is the thing a future reader is most likely to undo by accident.

Document all three Step 7 changes as raid-strategy decisions rather than bug fixes:

- The portal gate, with the refcount reasoning — the squad now skips Tenebron's and Shadron's realm
  cycles, Sartharion stays immune while Shadron lives, and "all three drakes dead" is not an available
  gate because the portal is gone by then.
- The split target order, both tables side by side, so nobody "fixes" the divergence back to one list.
- The 5s melee hold, including that pets are deliberately exempt.

---

## Verification

**The module cannot be compiled headless in this environment.** Build verification has to be handed
off. Do not report a successful build.

**Static checks (runnable here):**

```sh
cd modules/mod-playerbots/src/Ai/Raid/OS
grep -rn "MoveTo(" Action/                     # every call passes lessDelay
grep -rn "MOVEMENT_FORCED\|MOVEMENT_COMBAT\|MOVEMENT_NORMAL" .   # only the 3 dodges are FORCED
grep -rn "TOLERANCE\|RELEASE" Util/ OSTriggers.cpp Action/       # no release == its arrival
grep -rn "H = 3" Util/OSData.h                 # heroic ids gone
grep -rn "MarkTargetWith\|SetRtiTarget" .      # still marking-free
grep -rn "DrakesRemaining" Util/ OSTriggers.cpp # no FindUnitByEntries / LandedDrakes inside it
grep -rn "PULL_WINDOW_MS\|MELEE_DPS_PULL_HOLD_MS" . # the two windows stayed separate
```

**In-game, 25H Obsidian Sanctum with all three drakes up ("Twilight Zone")** — the only configuration
that exercises every path:

| What to watch | Pass condition |
|---|---|
| Tsunami during a raid re-form | No bot eats a wave while `os raid hold` is mid-move. **This is the Step 3 fix and the single most important observation.** |
| Two mechanics at once | A bot off the platform under a wave returns to the platform; a bot dodging a fissure under a wave takes the corridor. Confirms the multiplier precedence. |
| Drake landing | Melee fan around the rear arc instead of stacking on `landing->X + 4`. |
| Drake death | No retarget flicker as `PriorityTarget` hands back Sartharion; melee do not immediately walk 22.5yd on the corridor flip. |
| Pull drag | Completes before the first wave (~20s). If not, watch for the drag↔dodge loop and the existing `LOG_WARN` at `MAIN_TANK_DRAG_TIMEOUT_MS`. |
| Twilight Fissure on a tank hold | Tank steps off the blast to a displaced hold rather than standing in it. |
| Portal gate | Nobody dives on Tenebron's or Shadron's cycle. First entry happens only once two drakes are down. **Watch specifically that no bot dives on the pull** — that is the failure mode if `DrakesRemaining` picks up the selectable/room filters. |
| Portal squad | Once the gate opens, bots reach and click the portal without being preempted by holds. |
| Off-tank after drakes | Assist-0 stays on the platform taking Lava Blazes. He must never walk to the portal. |
| First 5s of the pull | Melee DPS do not swing and cast nothing. Their pets **do** attack. Ranged and healers are unaffected. Melee should still be walking to their flank position during the hold, not standing at the raid line. |
| Melee target order | Melee stay on Sartharion with a Lava Blaze up; ranged peel off to kill it. Watch whether blazes accumulate on the off-tank across a full fight. |
| Wipe / soft reset | Every latch re-arms — a re-pull behaves identically to a fresh pull. Directly tests the Q13 decision to put latches in `EncounterState`. |

**Config note:** never trust `playerbots.conf` values directly — gitignored
`configurationOverrides/*.env` at the server root override them via `AC_*` env vars. Read the
effective `MaxWaitForMove` / `ReactDelay` with `docker exec ac-worldserver env | grep ^AC_`.
