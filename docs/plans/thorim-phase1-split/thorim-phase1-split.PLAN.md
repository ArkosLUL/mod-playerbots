# Thorim phase 1 — hold the arena/gauntlet split until phase 2

> **Status: implemented.** Every section below is in the tree and statically verified. Not yet
> compiled or run in-game — the Verification section is the hand-off list.

## Context

A live Ulduar Thorim run wiped: the group that was supposed to hold the arena during phase 1 ran into
the gauntlet behind the master. With nobody left in the arena, Thorim summoned the Lightning Orb and
killed the raid.

The raid must split in two at the pull — an arena group and a gauntlet group — and that split has to
hold for all of phase 1, ending only when Thorim drops to the arena floor for phase 2.

A split already exists in the code but is unreliable and unenforced. This plan makes it a latched,
raid-wide assignment and leashes the arena group inside the wipe box.

The previous Thorim work (`docs/plans/thorim-guide-parity/`) is implemented and untouched by this
plan except where noted.

## Verified mechanic facts

From `src/server/scripts/Northrend/Ulduar/Ulduar/boss_thorim.cpp` and the `acore_world` gameobject
table. Do not re-derive from guides.

### The wipe

`ThorimAI::GetArenaPlayer()` (`:625-633`) scans the whole map player list for one **alive** player in

```
x  > 2085  &&  x  < 2185
y  > -305  &&  y  < -214
z  < 425
```

`EVENT_THORIM_LIGHTNING_ORB` runs that scan **every 5 s** from `EVENT_THORIM_START_PHASE1` onward
(`:669`, `:683-699`). One failed scan is terminal: `Talk(SAY_WIPE)` and `SummonCreature(33138)`.
There is no grace period and no recovery — the orb is the wipe.

`EVENT_THORIM_NOT_REACH_IN_TIME` (`:700-705`) fires the same orb plus `SPELL_BERSERK_FRIENDS` at
5 min regardless, which is the outer bound on the whole of phase 1.

### Encounter start and geometry

- Six pre-boss trash deaths → `ACTION_START_TRASH_DIED` count 6 (`:460-481`) → the lever is unlocked,
  the Iron Ring Guards and Runic Colossus lose `UNIT_FLAG_IMMUNE_TO_PC`, `EVENT_THORIM_AGGRO` fires
  and `EVENT_THORIM_START_PHASE1` is scheduled 20 s later.
- `EVENT_THORIM_AGGRO` closes the arena fence, GO 194559 at `(2134.98, -216.91, 419.09)` — the north
  entrance. It reopens only on death (`:563-564`).
- The corridor is reached through the arena lever gate, GO 194560 at `(2180.76, -263.02, 414.68)`,
  opened by GO 194264 at `(2173.27, -252.87, 420.15)`. Both are at the **east** edge of the wipe box.
- Arena adds (`SpawnAnArenaNPC`, `:585-594`) spawn on the rim and `MoveJump` to **19–24 yd** from
  `Middle = (2134.68, -263.13, 419.44)`, which is `ULDUAR_THORIM_NEAR_ARENA_CENTER` to within a yard.
  Nothing an arena bot needs to fight is further than 24 yd from centre.
- Distance from arena centre to the wipe-box edges: 50 yd east/west, 49 yd north, 42 yd south. To the
  lever gate: 45.8 yd. So a **30 yd leash radius from arena centre covers every add landing and
  cannot reach the corridor mouth**.

### Phase boundary

Thorim jumps down on `DamageTaken` from a player above z 430 (`:509-541`). The bot side already
separates the halves with `ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD` = 429.6094. Phase 1 is
`boss->GetPositionZ() >= threshold`.

## Root causes

Four, all in [UldTriggers_Thorim.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp).

**1. The quota walk cannot assign the arena group when the roster does not fit.**
`ThorimGauntletPositioningTrigger::IsActive` (`:135-226`) and `ThorimArenaPositioningTrigger::IsActive`
(`:228-316`) each re-derive membership every tick by walking the group and decrementing
`requiredDpsQuantity` / `requiredHealerQuantity` / `requiredAssistTankQuantity` (10 m: 3/1/1,
25 m: 7/2/1). The arena verdict is the `return false` that only fires once **all three** counters
reach zero.

`requiredAssistTankQuantity` decrements only for `IsAssistTankOfIndex(member, 0)` — exactly one player
in the entire raid. With a single tank, or a tank the predicate does not recognise, that counter never
reaches zero, the `return false` never runs, and **every bot falls through to the gauntlet branch**.
The same happens on a short DPS or healer roster. That is the reported symptom exactly.

**2. A human off-tank breaks tank identity.**
`GetMainTankGuid` ([PlayerbotAI.cpp:2414](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L2414))
prefers `MEMBER_FLAG_MAINTANK` from the raid slots and otherwise falls back to the first alive tank in
roster order. With no explicit Main Tank flag and a human tank earlier in the roster than the bot MT,
the human is treated as main tank, `IsAssistTankOfIndex(botMT, 0)` becomes true, and **the bot main
tank is assigned to the gauntlet** leaving the arena tankless. Independently, a human always consumes
the gauntlet's single assist-tank quota slot even though no bot code can move them, and
`IsTank(human)` uses the spec-tab path (`:2281-2317`), which reports false for a feral druid out of
bear form.

**3. The arena node is dead for the whole of phase 1.**
`ThorimArenaPositioningTrigger` returns false while **any** arena add is alive (`:294-310`), and adds
respawn every 15/20/21/25 s. Its `Execute`
([UldActions_Thorim.cpp:155-169](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp#L155))
is the only place that strips `follow master`, so arena bots keep it and trail the master east. The
strip is also never undone, so any bot that did run it loses `follow master` permanently.

**4. Nothing leashes the arena group.** `PossibleTargetsValue` is every unfriendly unit inside
`AiPlayerbot.SightDistance` = 100 yd; the Runic Colossus is ~92 yd from arena centre, so corridor mobs
are legal targets for arena bots and `ReachTargetAction` will walk them out of the box.

## Decisions

Settled with the user; recorded so a fresh session does not relitigate them.

- **Hard leash plus a target guard.** A leash node walks strays back, a multiplier zeroes the generic
  movers while a bot is outside, and a second guard refuses out-of-box targets so a latched corridor
  mob is dropped instead of being chased in a loop.
- **Roster shortfalls shrink the gauntlet group; the main tank never leaves the arena.** No new config
  option.
- **No failsafe if the arena group dies.** The 5 s scan makes recovery impossible in practice, so the
  reassignment path is not worth the state it costs.
- **Leash only — no arena positioning.** Arena spread and add kill order stay out of scope.

## Implementation

### 1. Latched squads — `UldEncounter_Thorim.{h,cpp}`

Add to `ThorimEncounterState`:

```cpp
std::unordered_map<ObjectGuid, uint8> squads;   // ThorimSquad, stored as uint8
bool squadsAssigned = false;
std::unordered_set<ObjectGuid> followMasterStripped;
```

and beside `ThorimPhase2Role`:

```cpp
enum class ThorimSquad : uint8 { None, Arena, Gauntlet };
```

New public functions:

- `bool ThorimSplitActive(PlayerbotAI*)` — bot within `ULDUAR_THORIM_ENCOUNTER_PROXIMITY` of arena
  centre, Thorim alive and hostile, `IsInCombat()`, and `GetPositionZ() >= FLOOR_THRESHOLD`. The
  combat term is what keeps the leash off the raid while it walks in and clears the pre-boss trash;
  the z term is what ends the split the instant he jumps down.
- `ThorimSquad GetThorimSquad(PlayerbotAI*, Player*)` — reads the latched map, running the assignment
  pass on first call while Thorim is alive and the bot is in proximity. A guid with no entry (someone
  who joined late) is **Arena**, which is the side that cannot cause a wipe.
- `bool ThorimInArenaBox(WorldObject const*)` — the core's box verbatim, so the bot side and the wipe
  check cannot disagree.
- `bool ThorimArenaLeashBreached(PlayerbotAI*, Player*)` — split active, squad is Arena, and 2D
  distance from `ULDUAR_THORIM_NEAR_ARENA_CENTER` exceeds `ULDUAR_THORIM_ARENA_LEASH_RADIUS`.
- `void ThorimNoteFollowMasterStripped(Player*)` / `bool ThorimFollowMasterStripped(Player const*)` —
  so the reset node can hand `follow master` back.

Assignment pass, run once per instance and stored:

1. Walk the group in roster order, keeping members alive on `ULDUAR_MAP_ID` in this instance.
2. Quota: 25-man → 1 tank + 2 healers + 7 DPS; anything else → 1 tank + 1 healer + 3 DPS.
3. **Gauntlet tank must be a bot**: the first member satisfying `IsAssistTankOfIndex(member, 0)`
   *and* `member->GetSession()->IsBot()`; failing that, the first bot tank in roster order that is not
   the main tank. If neither exists the slot goes unfilled and one extra DPS is taken instead — a
   human off-tank cannot consume it, because nothing in this module can move them.
4. Healers, then DPS, in roster order, skipping the main tank and anyone already assigned. Stop early
   when the roster runs out — a short roster shrinks the gauntlet rather than emptying the arena.
5. Hard cap: stop assigning once the arena would drop below 3 members.
6. `PlayerbotAI::IsMainTank(member)` is **never** assigned to the gauntlet, whatever the quota says.
7. Everyone else is Arena. Set `squadsAssigned = true`.

Cleared by the existing `ResetThorimEncounterState`, alongside the melee slots and scan caches. Extend
`ThorimBotHasEncounterState` to count a squad entry, so the reset node still fires for a bot whose
only state is its side.

Note in the doc block that `MEMBER_FLAG_MAINTANK` should be set in the raid frame — without it
`GetMainTankGuid` falls back to roster order and a human tank can silently displace the bot MT.

### 2. Constants — `UldBossHelper.h`

Beside the existing Thorim tunables, each with its derivation:

```
ULDUAR_THORIM_ARENA_LEASH_RADIUS = 30.0f   // adds land 19-24 yd out; box edge is >= 42 yd
ULDUAR_THORIM_ARENA_BOX_MIN_X    = 2085.0f
ULDUAR_THORIM_ARENA_BOX_MAX_X    = 2185.0f
ULDUAR_THORIM_ARENA_BOX_MIN_Y    = -305.0f
ULDUAR_THORIM_ARENA_BOX_MAX_Y    = -214.0f
ULDUAR_THORIM_ARENA_BOX_MAX_Z    = 425.0f
```

The box values are the boss script's `GetArenaPlayer` bounds; comment that they are copied from it and
must not be tightened, because the wipe check uses them literally. No new positions, so no navprobe
run is needed.

### 3. Leash node

`thorim arena leash trigger` / `thorim arena leash action`.

- Trigger: `ThorimArenaLeashBreached(botAI, bot)`.
- Action: `MoveTo` `ULDUAR_THORIM_NEAR_ARENA_CENTER` at `MovementPriority::MOVEMENT_COMBAT`, return
  true. Centre is the destination rather than the nearest box edge, because the edge is exactly where
  a rounding error becomes a wipe.
- Relevance `ACTION_RAID + 5`, the top of the Thorim ladder. Phase 1 and phase 2 nodes are mutually
  exclusive on the z threshold, so this does not compete with the Lightning Charge dodge at `+4`.

### 4. Two guard multipliers

`ThorimArenaLeashMultiplier` — while `ThorimArenaLeashBreached` holds:

- `dynamic_cast<MovementAction*>` gate first.
- Exempt `AvoidAoeAction` and the encounter's own movers by name, including
  `"thorim arena leash action"`.
- **Do not** exempt `AttackAction` / `ReachTargetAction` here. Those are what drag the bot east, and
  they are the reason the leash exists. This is the deliberate exception to the rule the other Uld
  guards follow.
- Return 0 only while outside the leash. The window closes on re-entry, so this cannot become the Void
  Reaver permanent freeze.

`ThorimArenaTargetGuardMultiplier` — split active, squad is Arena, and `AI_VALUE(Unit*, "current
target")` is outside the arena box: zero `AttackAction*` and `ReachTargetAction*` so the picker drops
the corridor mob instead of the leash and the chase fighting each other tick by tick. Gated on the
current target, so a switch releases it for free, the same shape as `ThorimRunicBarrierMultiplier`.

### 5. Rewire the two existing positioning nodes

- `ThorimGauntletPositioningTrigger::IsActive` — delete the quota walk, replace with
  `GetThorimSquad(botAI, bot) == ThorimSquad::Gauntlet`. Keep the rest (master proximity, lane index,
  balcony jump) unchanged.
- `ThorimArenaPositioningTrigger::IsActive` — replace the quota walk with
  `== ThorimSquad::Arena`, and **drop the "no arena adds alive" suppression**: the node's job is now to
  keep the bot in the arena, and adds are alive for essentially all of phase 1. Keep the `> 5 yd from
  centre` term so it stays an idle nudge rather than a permanent order, and let the leash node handle
  the actual breaches at higher relevance.
- `ThorimArenaPositioningAction::Execute` — record the `follow master` strip via
  `ThorimNoteFollowMasterStripped` so it can be undone.
- `ThorimResetEncounterStateAction::Execute` — re-add `follow master` in `BOT_STATE_NON_COMBAT` for any
  bot the state says it was stripped from, before clearing. Fixes the existing leak where an arena bot
  never follows again after the encounter.

### 6. Wiring

Four string-keyed sites; every one is silent at runtime if misspelled.

- `UldActionContext.h` — creator + factory for `thorim arena leash action`.
- `UldTriggerContext.h` — same for `thorim arena leash trigger`.
- `UldStrategy.cpp::InitTriggers` — the node at `ACTION_RAID + 5`, in the Thorim block.
- `UldStrategy.cpp::InitMultipliers` — `ThorimArenaLeashMultiplier` and
  `ThorimArenaTargetGuardMultiplier`, beside the two existing Thorim multipliers.

No CMake edit; `CollectSourceFiles` globs the module. No new files either — everything lands in the
existing Thorim units.

### 7. Docs

`docs/raids/ulduar.md`, Thorim section: add a **Phase 1 split** subsection with the wipe box, the 5 s
scan, the 5 min hard fail, the add landing radius, the leash radius and its derivation, the squad
quotas, and the `MEMBER_FLAG_MAINTANK` caveat for human tanks. Move arena spread and add kill order
into the existing Known gaps list. The file is not reachable from CLAUDE.md or AGENTS.md, so no
compaction skill is required.

## Verification

Static, before the build:

1. Grep each new `TriggerNode` / `NextAction` string against its `creators[...]` entry in both context
   files.
2. Confirm `ThorimArenaLeashMultiplier` is window-gated on `ThorimArenaLeashBreached` last, and that
   its deliberate non-exemption of `AttackAction` / `ReachTargetAction` carries the comment explaining
   why it differs from the other Uld guards.
3. Confirm the box constants match `GetArenaPlayer` in `boss_thorim.cpp` exactly.

The module cannot be compiled headless here, so build and in-game run are a hand-off. Worth watching:

4. **At the pull**: the raid splits once, and the assignment does not change again — no bot swaps sides
   mid-phase-1, including after a death.
5. **Single-tank raid**: the arena group is still populated. This is the case that wiped.
6. **Human off-tank**: with `MEMBER_FLAG_MAINTANK` set on the bot MT, the bot MT stays in the arena and
   a *bot* takes the gauntlet tank slot. Then repeat with the flag cleared and confirm the fallback
   still leaves the arena tanked.
7. **The leash**: an arena bot that targets a corridor mob drops it rather than ping-ponging at the
   gate. Nobody crosses x 2185 during phase 1.
8. **No wipe orb**: no `SAY_WIPE` in the combat log, and the gauntlet still clears inside 2:45 with
   `AiPlayerbot.UlduarThorimHardMode = 1`.
9. **Phase 2**: the moment Thorim lands, the leash releases and the gauntlet group rejoins — the
   phase-2 ring must form normally, and `follow master` must be back on the arena bots after the kill.
10. Run **both 10-man and 25-man** — the quotas differ.

## Out of scope

Arena group spread and positioning, arena add kill priority, Stormhammer 62042, Rune Detonation 62526,
Stomp 62411, the in-combat `SPELL_SMASH` 62339 cone, and any recovery once the arena group is dead.
