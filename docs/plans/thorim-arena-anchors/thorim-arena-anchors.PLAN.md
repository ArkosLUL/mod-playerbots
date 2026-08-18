# Thorim phase 1 — anchor the arena squad

> **Status: implemented.** Every section below is in the tree and statically verified. Not yet
> compiled or run in-game - the Verification section is the hand-off list.

## Context

The phase 1 arena/gauntlet split (`docs/plans/thorim-phase1-split/`) is implemented: the raid latches
into two squads at the pull, and a 30 yd leash fences the arena squad so the Lightning Orb wipe check
never finds the box empty.

Live run since: the arena squad no longer runs down the corridor, but it **spreads east across the
arena until it is standing in the gauntlet gateway**. The squad's job in phase 1 is to survive the
arena adds, and a squad smeared over 50 yd cannot — healers end up out of range of whoever the adds
picked.

This plan gives the arena squad real anchors so there is something holding it together, instead of
only a fence stopping it leaving.

## Root cause

The leash is the only force acting on an arena bot during phase 1, and a fence is not a formation.

1. `ThorimArenaPositioningTrigger::IsActive`
   ([UldTriggers_Thorim.cpp:201](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp#L201))
   returns false in `BOT_STATE_COMBAT`. Phase 1 is combat from end to end, so the node never fires
   and nothing pulls a bot back toward the middle.
2. `ThorimArenaLeashMultiplier` only zeroes movement **while the bot is outside** the leash. Inside
   it, every generic mover runs free. Bots therefore diffuse outward until they hit the fence and
   stay parked against it.
3. The fence is a 30 yd circle on `ULDUAR_THORIM_NEAR_ARENA_CENTER (2134.99, -263.12)`, so its east
   edge is x ≈ 2165. The lever gate is x = 2180.76. Parked against the east edge *is* standing in
   the gateway.

East is the worst direction to drift, from `boss_thorim.cpp`:

- `boss_thorim_arena_npcs::CanAIAttack` is `target->GetPositionX() < 2180 && target->GetPositionZ() < 425`,
  and `SelectT()` skips any player with `GetPositionX() > 2180`. A bot that drifts to the gate stops
  being attackable at all — the add drops it and re-rolls onto someone else, usually a healer.
- `SelectT()` picks a **random** arena-side player and `AddThreat(target, 500.0f)`. Threat is
  chaotic by design, so the only thing that keeps adds manageable is the raid being in one place.

## Verified mechanic facts

From `src/server/scripts/Northrend/Ulduar/Ulduar/boss_thorim.cpp`. Do not re-derive from guides.

- `Middle = (2134.68, -263.13, 419.44)`, within a yard of `ULDUAR_THORIM_NEAR_ARENA_CENTER`.
- `SpawnAnArenaNPC` (`:585`) spawns on the rim and `MoveJump`s to `urand(19, 24)` yd from `Middle`
  along the bearing of the spawn point. **Nothing an arena bot must reach lands further than 24 yd
  from centre.**
- Phase 1 arena adds and their cadence (`:707-720`): Dark Rune Warbringer 15s, Dark Rune Evoker 20s,
  6-7 Dark Rune Commoners 21s, Dark Rune Champion 25s. Champion casts `SPELL_CHARGE` on selection and
  `SPELL_WHIRLWIND` on a timer; Evoker heals the other adds with `SPELL_RUNIC_MENDING` at 40 yd.
- The arena floor is flat: `ULDUAR_THORIM_PHASE2_RANGE1/RANGE3_SPOT` sit 22 yd west and east of
  centre at z 419.53, `ULDUAR_THORIM_PHASE2_TANK_SPOT` 24 yd south at z 419.49.

## Decisions

Settled with the user; recorded so a fresh session does not relitigate them.

- **Anchors only.** No add kill priority, no tank pickup/taunt node. Targeting and threat stay with
  the generic combat strategies.
- **Melee are leashed to the tank spot** rather than left on the 30 yd fence.
- **The ring is centred on the arena centre**, not biased west. No new hardcoded `Position`, so no
  navprobe run: the ring points are computed and mesh-validated at runtime, the way
  `GetHodirRingSlot` already does it.

## Implementation

The pattern to copy is Hodir's, which is the same shape XT-002 uses: a single
`Get<Boss>Anchor(botAI, bot, out, tolerance)` that trigger and action both go through so they cannot
disagree ([UldBossHelper.cpp:843](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L843)),
with the ring slot derived from a fixed bearing and validated against ground and collision
([UldBossHelper.cpp:679](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L679)).

**No new node names.** The existing `thorim arena positioning` trigger/action are rewritten into the
anchor node, so `UldActionContext.h`, `UldTriggerContext.h` and `InitTriggers` need no edit — and the
name is already on `ThorimArenaLeashMultiplier`'s whitelist.

### 1. Constants — `UldBossHelper.h`

Beside the existing arena split tunables, each with its derivation:

```
ULDUAR_THORIM_ARENA_RING_INNER       = 10.0f  // clear of the melee clump at centre
ULDUAR_THORIM_ARENA_RING_OUTER       = 14.0f
ULDUAR_THORIM_ARENA_RING_INNER_SLOTS = 5
ULDUAR_THORIM_ARENA_MELEE_LEASH      = 24.0f  // the furthest an add ever lands from Middle
```

Arrival uses the existing `ULDUAR_THORIM_RING_ARRIVE_TOLERANCE` (3.0) and
`ULDUAR_THORIM_RING_REPOSITION_TOLERANCE` (5.0). The tank anchor is
`ULDUAR_THORIM_NEAR_ARENA_CENTER` itself, so there is no new tank constant either.

### 2. Anchors — `UldEncounter_Thorim.{h,cpp}`

Add to `ThorimEncounterState`:

```cpp
std::unordered_set<ObjectGuid> arenaAnchorArrived;
```

Its own set, **not** `ringArrived`: phase 1 and phase 2 are mutually exclusive on the z threshold
today, and sharing a latch between them is a trap waiting for the first time that stops being true.

New public API:

- `bool GetThorimArenaAnchor(PlayerbotAI*, Player*, Position& out, float& tolerance)`
  - False unless the bot's squad is `ThorimSquad::Arena` and Thorim is alive, hostile and still above
    `ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`. Deliberately **not** gated on `ThorimSplitActive`, which
    also requires combat: the squad has to walk to its side before the pull, not after it.
  - Tank → `ULDUAR_THORIM_NEAR_ARENA_CENTER`. The adds are dragged here and everything else is
    measured from here.
  - Ranged (`IsRanged && !IsTank`) → ring slot, below.
  - Melee → an anchor at the tank spot **only while out of combat**, so the squad still forms up
    before the pull. In combat they get false and keep their uptime; the tightened leash in §3 is
    what holds them.
- `bool ThorimArenaAnchorNeedsMove(PlayerbotAI*, Player*, Position const& spot)` — reach-then-hold
  latch against `arenaAnchorArrived`, a copy of `ThorimRingNeedsMove`
  ([UldEncounter_Thorim.cpp:782](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L782)).
  Idempotent, so trigger and action can both ask. A moving bot casts nothing, and a tight deadband
  has it sliding in place forever.
- `bool ThorimArenaAnchorSettled(PlayerbotAI*, Player*)` — for the guard multiplier.

Ring slot, file-local, modelled on `GetHodirRingSlot`:

- Members are the **latched arena squad**, walked in group roster order and filtered on
  `GetThorimSquad(...) == Arena && IsRanged && !IsTank`. Roster order, not the live alive-set: bots
  die in here, and ranking by the survivors means one death renumbers everyone behind the corpse and
  the whole formation shuffles mid-fight. That is the same reason `meleeSlots` is sticky.
- Ranged DPS ahead of healers, ties on guid, so every bot derives the same layout without sharing
  state.
- Slot 0 takes the inner ring, not the centre — the centre belongs to the tank and the melee clump.
  First `ULDUAR_THORIM_ARENA_RING_INNER_SLOTS` on the inner radius, the rest on the outer.
- Base bearing is `atan2` from `ULDUAR_THORIM_NEAR_ENTRANCE_POSITION` to the arena centre — two
  points that already exist, and it aims slot 0 away from the gate rather than at it.
- Validate every point: `GetMapWaterOrGroundLevel`, then `CheckCollisionAndGetValidCoords`. Raw ring
  geometry is exactly the shape that lands off the navmesh, and `MoveTo` fails silently on an
  off-mesh destination.

Extend `ThorimBotHasEncounterState` and `ResetThorimEncounterState` for `arenaAnchorArrived`.

### 3. Melee leash — `UldEncounter_Thorim.cpp`

`ThorimArenaLeashBreached` becomes role-aware:

- Melee (non-tank, not ranged) breach past `ULDUAR_THORIM_ARENA_MELEE_LEASH` (24 yd).
- Everyone else keeps `ULDUAR_THORIM_ARENA_LEASH_RADIUS` (30 yd), which stays the backstop; the
  anchor holds them at 10-14 yd long before it matters.
- The `!ThorimInArenaBox(bot)` term is unchanged for both — it is the box the boss script scans.

24 yd is the furthest an add ever lands, so it costs no uptime, and it puts the melee clump's east
limit at x ≈ 2159, 21 yd short of the gate and well inside the x < 2180 line the adds need to keep
attacking.

### 4. Rewrite the node — `UldTriggers_Thorim.cpp`, `UldActions_Thorim.cpp`

`ThorimArenaPositioningTrigger::IsActive`:

```
anchor = GetThorimArenaAnchor(...)          // false → no node (melee in combat)
stand down for ThorimSifBlizzardTrigger / ThorimSifFrostNovaTrigger   // surviving beats standing on a spot
return ThorimArenaAnchorNeedsMove(botAI, bot, anchor)
```

Drop the `BOT_STATE_COMBAT` bail and the `> 5 yd from centre` term; the latch replaces both.

`ThorimArenaPositioningAction::Execute` — `MoveTo` the anchor at `MOVEMENT_COMBAT` instead of the
hardcoded centre. Keep the `follow master` strip and its `ThorimNoteFollowMasterStripped` bookkeeping
exactly as they are.

`ThorimArenaLeashAction::Execute` — walk to the bot's own anchor when it has one, falling back to the
arena centre. Yanking the whole squad onto one point every time the fence trips is how the formation
gets destroyed by the thing meant to protect it.

### 5. Guard multiplier — `UldMultipliers.{h,cpp}`, `UldStrategy.cpp::InitMultipliers`

`ThorimArenaAnchorGuardMultiplier`, an exact copy of `ThorimMovementGuardMultiplier`'s shape
([UldMultipliers.cpp:369](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L369)) gated on
`ThorimArenaAnchorSettled` instead of `ThorimMeleeRingSettled`:

- `dynamic_cast<MovementAction*>` gate first.
- Exempt `AttackAction`, `ReachTargetAction` and `AvoidAoeAction`. A ranged bot whose target landed
  on the far rim is up to 38 yd away and has to step in; blocking that would trade the drift problem
  for a silence problem.
- Exempt the encounter's own movers by name: `"thorim arena positioning action"`,
  `"thorim arena leash action"`, `"thorim sif blizzard action"`, `"thorim sif frost nova action"`.
- Zero only while settled. The window opens and closes with the latch, so this cannot become the Void
  Reaver permanent freeze.

Wire it in `InitMultipliers` beside the other Thorim multipliers. This is the only new wiring site in
the change.

### 6. Docs

`docs/raids/ulduar.md`, the **Phase 1 split** subsection: add the anchor layout, the two ring radii,
the melee leash and its derivation, and the two add-side numbers that make east the bad direction —
`CanAIAttack`'s x < 2180 and `SelectT`'s random target with 500 threat. Move "arena spread" out of the
Known gaps list, since it is no longer a gap. The file is not reachable from CLAUDE.md or AGENTS.md,
so no compaction skill is required.

## Verification

Static, before the build:

1. The rewritten trigger and action both reach their position through `GetThorimArenaAnchor` and
   nowhere else.
2. `ThorimArenaAnchorGuardMultiplier` is declared, defined, and constructed in `InitMultipliers`.
3. `arenaAnchorArrived` is cleared in `ResetThorimEncounterState` and counted in
   `ThorimBotHasEncounterState`.
4. No new hardcoded `Position` anywhere in the diff — confirming the navprobe rule does not apply.

The module cannot be compiled headless here, so build and in-game run are a hand-off. Worth watching:

5. **The formation holds.** Ranged and healers sit on a ring around the middle for all of phase 1 and
   do not creep east. Nobody crosses x 2165, let alone reaches the gate.
6. **Melee still have uptime.** They reach adds on the far rim and are not stuck sliding on a spot.
7. **Deaths do not shuffle the ring.** Kill an arena ranged bot mid-phase-1 and confirm the survivors
   keep their slots.
8. **Hazards still win.** With `AiPlayerbot.UlduarThorimHardMode = 1`, Sif's Blizzard and Frost Nova
   still move people off their anchors, and they walk back afterwards.
9. **Pre-pull.** The arena squad walks to the middle instead of trailing the master to the gate, and
   `follow master` is back on them after the kill.
10. **Phase 2 is untouched.** The ring forms normally the moment Thorim lands.
11. Run **both 10-man and 25-man** — 25-man fills the outer ring, 10-man may not.

## Out of scope

Add kill priority, arena tank pickup and taunting, Stormhammer 62042, Rune Detonation 62526,
Stomp 62411, the in-combat `SPELL_SMASH` 62339 cone, and any recovery once the arena squad is dead.
