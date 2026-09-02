# Thorim: stop the gauntlet squad going blind on the balcony

Trace: `env/dist/logs/botobs/603_1_thorim_1788382188.ndjson`, schema **v10**, 24 members, 6:57 to a
called wipe with Thorim at **2.47%**. Read with
`python modules/mod-playerbots/tools/botobs/postmortem.py <file> [--notes|--stalls|--track]`.
Build under test already contains `3e28b52b0` (phase-1 latch + pet leash) and `caa8cf692` (balcony
route) — confirmed from the trace: `thorim.dpstarget = Captured Mercenary Soldier` at 0:00.19 only
exists with the trash tiers, and `thorim.balcony` notes only exist with the balcony node.

On commit, copy this file to
`docs/plans/thorim-gauntlet-balcony-handoff/thorim-gauntlet-balcony-handoff.PLAN.md`.

---

## Context

Two complaints: the gauntlet squad ate the stun traps again, and they never dropped off the platform
into the arena. Both reproduce, and both come from the same thing.

**The corridor itself is fixed.** Squad 2 (Elemena, Bulwark, Power, Justice, Agony, Shadow, Druidica,
Totemist, Mighty, Holylight) walked the whole gauntlet cleanly: Runic Colossus 1:55–2:22, Iron Honor
Guards 2:29–2:48, up the ramp 3:00–3:20 (z 412 → 438), Ancient Rune Giant 3:10–3:33.5. The long
stationary windows the `--stalls` report flags in that stretch are all fights, not stalls. Everything
after 3:33 is the problem.

### The failure, in order

| time | what happened |
|---|---|
| 3:33.5 | Rune Giant dies. All ten drop to no target. |
| 3:39.2–3:44.5 | All ten acquire **Thorim**, from **111–138 yd**, while he is untouchable at z 438. |
| 3:39–3:44 | `reach melee` / `reach spell` walk them up the hallway **centre line**, x 2134–2140. |
| 3:41.3–3:47.3 | **Paralytic Field** lands on all ten, 8.5–14.5 s each. Pinned at y −393 to −403. |
| 3:48.5–3:50.5 | Thorim jumps to the arena floor. Phase 2 starts. |
| 3:48.6 | `thorim balcony advance action` flips to **USELESS** on every bot and never recovers. |
| 3:49.9 | All ten `drop target`, out of combat. |
| 3:53.9 | The human drops off the platform. Nobody follows. |
| 3:55.87 | Paralytic Field expires. |
| 3:56.3 | `follow` — now the only mover — walks them **back down the ramp and the whole corridor**. |
| 4:36.2–4:42.7 | They re-enter the arena, having covered **321–380 yd**. |

Thorim lost ~0.28%/s while they were gone and ~0.8%/s once they were back. The round trip cost
roughly 45 s of ten-player DPS on a boss that ended at 2.47%.

### Break 1 — the encounter goes blind above the ramp

`GetThorim` (`UldEncounter_Thorim.cpp:584`) is `GetFirstAliveUnitByEntry`, which walks
`"possible targets no los"` — capped at `AiPlayerbot.SightDistance`, **100.0** (conf default, no
`AC_*` override; checked `docker exec ac-worldserver env`). Effective reach is ~110 yd once the
boss's bounding radius is added. The upper hallway is 100–145 yd from Thorim's platform and 130–176 yd
from the arena floor.

Every Thorim entry point resolves the boss through it — `ThorimSplitActive`, `GetThorimSquad`,
`ThorimBalconyOpen`, `ThorimHasDpsTarget`, `GetThorimDpsTarget`, 20 call sites in all. So the moment
the squad climbs the ramp, the whole encounter stops existing for them.

Measured on the balcony trigger, the boundary is razor sharp:

```
active  (FAILED/OK): 102.2 .. 106.8 yd     11 evaluations, 220.5s .. 226.8s
inactive (USELESS):  110.9 .. 113.1 yd      5 evaluations, 228.6s .. 228.7s
```

Thorim's own jump carried him ~7 yd further north and switched the node off for good.

Meanwhile the generic picker is **threat-based, not range-based**: `AttackersValue::Calculate` builds
from threat lists (`AddAttackersOf`), so an untouchable boss 138 yd away is still in every bot's
attacker list. `dps assist` fires (`Mighty, 219.77s, rel=50.0, vd=OK`) and takes him. So the
suppression added in `3e28b52b0` is off exactly where the picker is most dangerous.

### Break 2 — the balcony route never drives the walk

The route is correct. Both trap bunnies are static spawns, from `acore_world.creature`:

```
33054  (2134.9,  -390.776, 437.311)
33725  (2134.93, -339.696, 437.311)
```

Every leg of the waypoint chain clears both by **≥16.3 yd** at closest approach (BALCONY_1 18.3,
BALCONY_2 16.9, the four segments 16.3–17.0). Against a 12 yd field that is enough.

It never got to drive. `ThorimBalconyAdvanceAction::Execute` issues the waypoint walk at
`MovementPriority::MOVEMENT_NORMAL`, and `IsWaitingForLastMove` refuses a move whose priority is not
strictly greater than the one in flight. `reach melee` / `reach spell` run at `MOVEMENT_COMBAT` and
hold the mover for up to ~5 s, so the trace is a wall of

```
r=wait  by=thorim balcony advance action  pr=normal/combat  hms=4367 -> 4144 -> 3938 -> 3731
```

**Exactly one balcony move was accepted in the entire pull** (Bulwark, 3:44.82), and `reach melee`
overrode it 0.3 s later. Everything else that moved the squad came from `follow` (normal) or the two
combat chasers, whose goals sat at x 2133.9–2140.2 — straight through bunny 33054 at x 2134.9.

Where the field actually landed, relative to that bunny: 5.1 yd (Power), 10.8 (Holylight), 11.2
(Elemena), 11.4 (Shadow), 12.2 (Justice), 13.0–13.7 (the rest, within snapshot lag of the 12 yd edge).

### Break 3 — nothing takes the boss back off them

`ThorimDisableAutomaticTargetingMultiplier` suppresses the *pickers*. It does not cover
`ReachTargetAction`, and it exempts healers and tanks by design — three of these ten. Once a bot holds
Thorim, the chase runs regardless. `ThorimArenaTargetGuardMultiplier`, the node that does drop a bad
target, is scoped to `GetThorimSquad(...) == ThorimSquad::Arena` and so never applies here.

---

## Fix 1 — give the encounter a boss handle that does not depend on range

**File:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`

Add `ObjectGuid bossGuid;` to `ThorimEncounterState` (next to `colossusGuid` / `runeGiantGuid`,
header ~`:264`) and rewrite `GetThorim` (`:584`) in the shape of `GetThorimRunicColossus`:

- if `bossGuid` is set, return `botAI->GetUnit(bossGuid)` when it is alive — `ObjectAccessor::GetUnit`
  is a map-wide lookup with no range cap, so it answers from anywhere in the wing;
- otherwise fall back to `GetFirstAliveUnitByEntry(botAI, NPC_THORIM)` and store the guid.

No scan-interval throttle: the fallback only runs before the cache seeds, and the raid pulls from
0–7 yd of the arena centre, so it seeds on the pull.

Clear `bossGuid` in `ResetThorimEncounterState` (`:1854` block) alongside `colossusGuid` and
`runeGiantGuid`.

**Required companion changes.** Two entry points lean on `GetThorim` being sight-limited instead of
carrying a `NearThorimEncounter` gate, and both need one added:

- `ThorimEncounterStateIsStale` (`:1776`) — the comment there records 178 spurious resets during a
  single Hodir pull, which is exactly what a range-free handle re-opens.
- `ThorimUnbalancingStrikeSwapTrigger` (`UldTriggers_Thorim.cpp:247`) — the only tank node that reads
  the boss without an aura on the bot to prove it is standing at him.

`NearThorimEncounter` is a pure distance test against the arena centre plus the balcony box, so it is
unaffected and still bounds every other caller to the wing.

## Fix 2 — walk the balcony route at combat priority

**File:** `src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp`, `ThorimBalconyAdvanceAction::Execute` (`:265`)

The waypoint branch's `MovementPriority::MOVEMENT_NORMAL` becomes `MOVEMENT_COMBAT`, matching the
jump-start branch six lines above it and the corridor node's `MoveToGauntletWaypoint`. One word.
Leave `lessDelay` at `true` so the whole node moves on one setting.

Rewrite the comment above the node to say why: the hallway is walked in combat with a boss the squad
cannot reach, so a normal-priority walk loses every tick to `reach melee`, and the route that avoids
the Paralytic Field bunnies never runs.

## Fix 3 — take the untouchable boss off a gauntlet bot

**Files:** `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/UldStrategy.cpp:868`, `src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp:40`,
`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`

New `ThorimBalconyGuardMultiplier` ("thorim balcony guard"), registered next to the arena guard. A
separate class rather than widening the arena one: the test is different, and a distinct name keeps
the `veto` records in the trace readable. Two clauses, both scoped to
`GetThorimSquad(...) == ThorimSquad::Gauntlet` and `bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`:

1. **Target.** For `AttackAction` / `ReachTargetAction`, return 0 when the current target is Thorim or
   Sif while `ThorimSplitActive` — he cannot be touched from up there. Keep the existing escape hatch:
   `ThorimIsTargetSelectionAction(action)` returns 1, or the guard deadlocks against the one node that
   would drop the target.

   **This does not work on its own.** `ThorimDpsPriorityTrigger` opens with a hard-coded
   `bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f` bail, and the hallway is 105-176 yd
   from the arena centre, so the only node that takes a forbidden target back off a bot never runs up
   there. Swap that bail for `NearThorimEncounter(bot)` — same cheap shape, and it is the gate every
   other Thorim entry point already uses. That means exporting `NearThorimEncounter`: declare it in
   `UldEncounter_Thorim.h` and move its definition out of the anonymous namespace in the .cpp, which
   is a pure move (it lands just above `GetThorim`). Leave the two other 110 yd gates alone —
   `ThorimFallFromFloorTrigger` is about the arena floor, and `ThorimGauntletPositioningTrigger` is
   the separate problem noted below.

2. **Movement.** While `ThorimBalconyOpen(botAI, bot)` is true, return 0 for `MovementAction` other
   than a whitelist of `thorim balcony advance action` plus `AvoidAoeAction` (Rune Detonation does
   land up there — it hit Holylight at 3:14 and Justice at 3:26). Same shape as
   `ThorimArenaAnchorGuardMultiplier`. This is what stops `follow` walking the squad 300 yd back down
   the corridor, and it is self-limiting: the moment the balcony node is not live, `follow` is free
   again, so nothing can strand a bot up there.

### Found while implementing, not fixed

`ThorimGauntletPositioningTrigger` (`UldTriggers_Thorim.cpp:65`) carries the same hard-coded 110 yd
bail, and the **left lane's last three waypoints are 117.3, 120.8 and 126.1 yd** from the arena centre
(the right lane's are all inside, 84-101). So a squad walking the left lane loses its corridor node
for the final stretch and finishes the walk on `follow`. It did get there this pull, so this is a
latent problem rather than the one being fixed — but it is the same class of bug and worth its own
pass.

### Deliberately not doing

- **No Paralytic Field dodge node.** Decided. The waypoint chain already clears both bunnies by 16 yd
  or more; the route was never the problem.
- **No change to `AttackersValue`.** It is threat-based on purpose and shared by every encounter.
- **No change to `SightDistance`.** It is a global.

---

## Verification

Static, before any pull:

- `grep -n "GetFirstAliveUnitByEntry" src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` appears only
  inside the new cached `GetThorim`.
- `grep -n "MOVEMENT_NORMAL" src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp` no longer matches inside
  `ThorimBalconyAdvanceAction`.
- `python apps/codestyle/codestyle-cpp.py` passes; no line over 120 columns; the CRLF file is still
  CRLF (`file` or a byte check on `UldMultipliers_Thorim.cpp`).
- `python tools/botobs/postmortem.py env/dist/logs/botobs/603_1_thorim_1788382188.ndjson` still runs.
- The module cannot be compiled headless here. Hand the branch off for a build rather than claiming
  one.

In-game, one 25-man pull, then re-read the fresh trace:

1. **No gauntlet bot targets Thorim above the ramp.** Was all ten, at 111–138 yd, from 3:39.2. Check
   `snap.u[7]` for squad-2 members at z > 429.6.
2. **No Paralytic Field on anyone.** Was all ten, 3:41.3–3:47.3. This is the headline.
3. **`thorim balcony advance action` moves are accepted, not `r=wait`.** Was one acceptance in the
   whole pull; expect it to own the walk from the Rune Giant's death onward.
4. **The squad hugs x 2141–2152 up the hallway**, not x 2134–2140.
5. **The balcony trigger survives Thorim's jump.** Was USELESS from 3:48.6 at 111 yd; with the cached
   handle it should stay active and take the jump branch.
6. **They drop off the platform.** Expect `JumpTo` toward (2137.88, −278.19, 419.67) within a few
   seconds of Thorim landing, against 45 s and 321–380 yd of walking back through the corridor.
7. **`follow` never moves a gauntlet bot while it is above z 429.6.**
8. **Regression check on the corridor.** The Colossus / Iron Honor Guard / Rune Giant clock should not
   move — Fix 3's movement clause is gated above the floor threshold, but it is the one change that
   could bite the corridor walk if that gate is wrong.
