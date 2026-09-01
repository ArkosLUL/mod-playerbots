# Thorim: walk the balcony past the Paralytic Fields, trigger phase 2, drop into the arena

## Context

Trace: `env/dist/logs/botobs/603_1_thorim_1788288995.ndjson`, schema v9, 25-man, pull to wipe in
4:14. Read it with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`, or with the
scratchpad scripts under
`C:\Users\boss2\AppData\Local\Temp\claude\...\f68e6a38-.../scratchpad\` (`lib.py`, `dmg.py`,
`churn.py`, `trap.py`, `track.py`, `adds.py`, `taken.py`, `auras.py`, `dbc.py`).

**The human, Deathsong, ran the gauntlet.** The trace labels them Arena because
`AssignThorimSquads` never picks a human for the corridor, but their track and their casts say
otherwise: Runic Colossus 104, Ancient Rune Giant 85, Iron Honor Guard 49, Thorim 26. Every number
below counts them as gauntlet. That mislabel is Fix 4.

Real split: **arena 13 bots, gauntlet 10 bots + the human.**

### The previous round's fix worked — the arena is no longer the problem

`1283532ea` (targeting multiplier + melee-aware tiers) did what it was meant to:

| metric | before | now |
|---|---|---|
| arena melee target switches/min | 108-125 | **5.8-7.5** |
| arena melee in range of own target | 30-46% | **70-86%** |
| melee picks | Evoker 55-59%, Warbringer never | **Champion 31.8%, Warbringer 21.8%** |

`snap.u[11]`, the new cumulative damage-dealt column, gives the Skada view directly. Arena melee run
3070-4232 dps. The arena held 14/14 above 90% hp until 2:10 and *won* the add war — adds within 50 yd
of the centre went 27 at 2:00 down to 11 at 3:00, dangerous tiers flat at 6-8 throughout. Attrition
starts at 2:20, the collapse is 3:20 → 3:40 (9 alive → 1).

**What killed them is when phase 2 arrived and who was missing for it.** Thorim dropped into the
arena at ~3:15 with ~20 adds still up, and the eleven people who should have been fighting beside
them were 150 yd away in the upper hallway. Damage taken splits 1 484 846 (arena, 13 bodies,
114k each) against 687 143 (gauntlet, 11 bodies, 62k each) — the arena carries 2.4x per body on the
same two healers, survivable while the fight is short and not while it runs 3:15.

### The gauntlet squad's low damage is downtime, not targeting

| squad | target uptime | moving | melee dps |
|---|---|---|---|
| arena | 88-98% | 5-27% | 3070-4232 |
| gauntlet | 62-65% | 43-52% | 1939-2236 |
| the human | 60% | 72% | 3472 |

Gauntlet bots stand with no target 38% of the time. Normalised for uptime the gap mostly closes
(2236/0.63 ≈ 3550 against 4232/0.88 ≈ 4810), so this is travel, not a picker bug. Per 30 s bucket the
dips land exactly on the travel legs, and the two worst are avoidable:

```
0:00 91%   1:30 90% (Colossus)   3:00 48%  <- Paralytic Field stun
0:30 43%   2:00 63%              3:30 13-26% <- the walk backwards
1:00 61%   2:30 74% (Rune Giant)
```

Both of those are what Fix 1 removes. The rest is the corridor being a corridor.

### The traps: confirmed, and the cause is a missing route

**Spell 62241 / 63540 "Paralytic Field"** — `SPELL_AURA_MOD_STUN`, **15 000 ms**, **12 yd**
persistent ground area, cast instantly with no bar and no telegraph by `Thorim Trap Bunny`
(entries 33054 / 33725, `boss_thorim_trap` at `boss_thorim.cpp:921-946`). The bunnies are
`UNIT_FLAG_NOT_SELECTABLE`, so **nothing in the world tells a bot they exist** — only hard-coded
positions can avoid them. Each polls `SelectNearbyTarget(nullptr, 12.0f)` about once a second and
re-arms after ~50 s.

Static spawns, `data/sql/base/db_world/creature.sql:128459` and `:128987`:

```
33054  (2134.900, -390.776, 437.311)
33725  (2134.930, -339.696, 437.311)
```

Both sit **dead centre** of the upper hallway. In the trace all nine surviving gauntlet bots were
stunned at 3:05.4-3:06.9 for 13.5-15.0 s standing at x 2129.7-2142.4, y ≈ -400; Bulwark caught the
second field at 3:20.2 at (2147.0, -341.5) for 7.6 s.

navprobe shows this is unavoidable without waypoints. The direct path from the Ancient Rune Giant to
the jump point runs straight up x ≈ 2135-2137 and passes **0.9 yd** from trap 1's centre:

```
navprobe --map 603 path 2135 -430 438.247 2137.137 -291.19 438.248
    ...  2135.615  -390.005   <- 0.9 yd from trap 1
    ...  2136.354  -342.011   <- 2.6 yd from trap 2
```

**Then it gets worse.** The `by` field on `move` records says the gauntlet squad had no encounter
node driving it on the balcony at all — every move from 0:45 onward reads `by=follow`.
`ThorimGauntletPositioningAction` is inert up there: branch (b) needs
`ThorimGauntletLaneIndex(master, ...)` and all six lane waypoints are at z 412 in the lower corridor;
branch (c) needs `boss->GetPositionZ() < 429.6` and in phase 1 Thorim stands at 438.33.

So when the stun broke at 3:20 the squad was following a master who had jumped into the arena at
z 419.8. There is no walkable link from the balcony, so the navmesh gave them the only legal ground
route — back through the hallway, down the ramp, through the lower corridor and around.
`follow(fail)` jumps to 106 and 152 in the 3:15 and 3:30 buckets. Bulwark's track:
(2146.9, -356.2, 438.4) at 3:30 → (2190.1, -431.4, 424.9) at 3:45 → (2219.5, -344.1, 412.1) at 4:00,
dead at 4:14. All ten died strung along the corridor between y -266 and -337.

**And nothing but a human ever starts phase 2.** `boss_thorim.cpp:511` —

```cpp
if (who && _isHitAllowed && who->GetPositionZ() > 430 && who->IsPlayer())
```

Phase 2 begins when a player **above z 430** damages Thorim, once the Rune Giant's death has set
`_isHitAllowed`. The bots could not have done it: `GetThorimDpsTarget`'s gauntlet branch ends
`return NoteThorimDpsTarget(bot, targets.runeGiant);`, null the moment the Giant dies, so the squad
has no target left at all. Thorim sat at **100% health for the entire fight**. `DisableThorim(true)`
only applies `UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_PACIFIED` and a root — he is hostile and attackable
throughout, so this is a missing instruction, not a protected boss.

### Pets: no arena pet went into the gauntlet this pull

Every cast from the four arena-squad pets went at an arena target:

| pet | owner | squad | targeted casts |
|---|---|---|---|
| Worm | Nightwarrior | arena | Warbringer 68, Evoker 48, Champion 39 |
| Wolf | Trueshot | arena | Warbringer 29, Champion 8, Evoker 6 |
| Flaaghun (felguard) | Hellflame | arena | Warbringer 25, Champion 15, Commoner 11 |
| Ruirin (imp) | Fel | arena | Champion 12, Evoker 11, Warbringer 10 |
| Khiigrom (felhunter) | Agony | **gauntlet** | Colossus 18, Rune Giant 10, Honor Guard 7 |
| Rootmuncher (ghoul) | **the human** | **gauntlet** | Colossus 11, Honor Guard 7, Rune Giant 7 |

Rootmuncher is the human's, and the position correlation is not close: at every one of its
gauntlet-mob casts it was 6-25 yd from Deathsong and 136-203 yd from Obliteration, the bot death
knight. Its owner ran the corridor, so a ghoul in the corridor is correct.

Last round's leash fired: `thorim.petrecall` 8 times between 0:29 and 0:47 (Wolf ×6, Ruirin ×2).

**The blind spot is still real.** Pets have no `snap.u` rows in v9 — the sweep drops them
(`RaidObsSnapshot.cpp:152` keeps only units hostile to the anchor) and nothing enumerates
`m_Controlled`. About half of all pet casts carry no target (Worm 217/409, Flaaghun 210/277,
Wolf 132/185), and those windows are invisible. Fix 3 closes it.

---

## Fix 1 — a balcony route that hugs the east wall, reaches Thorim, and drops

**Files:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/Action/UldActions_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.{h,cpp}`, `src/Ai/Raid/Uld/UldStrategy.cpp`,
`src/Ai/Raid/Uld/Uld{Trigger,Action}Context.h`

### The hallway, as measured

navprobe grid over map 603 at z 438.247 (`##` = settles on the floor, `..` = falls to the void):

```
        x=2116 2120 2126 2135 2144 2150 2154
y=-430    ##   ##   ##   ##   ##   ##   ##      Rune Giant end
y=-418    ..   ..   ##   ##   ##   ..   ..      south neck (second doors)
y=-406    ##   ##   ##   ##   ##   ##   ##   ┐
y=-390    ##   ##   ##   ##   ##   ##   ##   │  wide chamber
y=-340    ##   ##   ##   ##   ##   ##   ##   │  both traps sit at x 2134.9
y=-328    ##   ##   ##   ##   ##   ##   ##   ┘
y=-316    ..   ..   ##   ##   ##   ..   ..      north neck
y=-298    ##   ##   ##   ##   ##   ##   ##      Thorim's platform
```

A ~40 yd chamber with the traps on its centreline, and both necks forcing x 2126-2144. So the route
is: centre through the south neck, swing east for the chamber, back to centre for the north neck.

### New waypoints — all navprobe-verified, settled Z

Add beside the existing lane positions in `UldEncounter_Thorim.cpp`, declared `extern const Position`
in the header:

```cpp
ULDUAR_THORIM_BALCONY_1 = Position(2141.0f, -408.0f, 438.247f)
ULDUAR_THORIM_BALCONY_2 = Position(2151.0f, -396.0f, 438.247f)
ULDUAR_THORIM_BALCONY_3 = Position(2152.0f, -365.0f, 438.744f)
ULDUAR_THORIM_BALCONY_4 = Position(2151.0f, -332.0f, 438.247f)
ULDUAR_THORIM_BALCONY_5 = Position(2137.5f, -318.0f, 438.222f)
```

with the existing `ULDUAR_THORIM_JUMP_START_POINT` (2137.137, -291.190, 438.248) as the sixth and
last. Move that constant out of `UldActions_Thorim.cpp:28` into the helper header beside
`ULDUAR_THORIM_JUMP_END_POINT` — the per-boss split left the pair separated and the chain needs it.

Clearance, worst case per leg, walked off navprobe's own path output:

| leg | closest approach to the nearer trap |
|---|---|
| Rune Giant → B1 | 19.0 yd |
| B1 → B2 | **15.8 yd** |
| B2 → B3 | 16.3 yd |
| B3 → B4 | 16.4 yd |
| B4 → B5 | 17.0 yd |
| B5 → jump start | 21.8 yd |

Worst case 15.8 yd against a 12 yd radius — 3.8 yd of margin, and it holds even when somebody else
has already tripped a field, because the field is centred on the bunny.

East rather than west because the navmesh's own smoothing bulges that way (it routes B2→B4 out to
x ≈ 2155, giving 19 yd for free), and because it is where the bots already drift — Bulwark was at
x 2151 unprompted.

### New node `thorim balcony advance`

Trigger + action wired at `ACTION_RAID + 2` (between `thorim fall from floor` at +1 and
`thorim runic smash action` at +3). Registered in `UldTriggerContext.h`, `UldActionContext.h` **and**
`UldStrategy.cpp` — an unregistered name fails silently.

Active when all of:
- `NearThorimEncounter(bot)` and Thorim alive
- `GetThorimSquad(botAI, bot) == ThorimSquad::Gauntlet`
- `bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD` (429.6) — on the balcony
- the Ancient Rune Giant is dead or gone (new `GetThorimAncientRuneGiant` helper alongside the
  existing `GetThorimRunicColossus`, `NPC_ANCIENT_RUNE_GIANT = 32873`)

Deliberately **no master dependency** — the point is that the squad finishes the gauntlet on its own
clock. The corridor node keeps its master-driven behaviour untouched.

Execute:
1. **Thorim still up top** (`boss->GetPositionZ() > 429.6`): walk the chain. Keep a per-bot index in
   `ThorimEncounterState` as `RaidObs::ObsGuidMap<uint8> balconyStep{"thorim.balcony"}` so it shows
   in the trace; advance it when the bot is within ~6 yd of the current waypoint **or** already past
   its y, and never decrease it. Monotonic on purpose: a positional nearest-waypoint pick is what let
   them walk backwards. `MoveTo(..., exact_waypoint = true, MOVEMENT_NORMAL)`, same call shape as
   `MoveToGauntletWaypoint`.
2. **Thorim has jumped** (`boss->GetPositionZ() < 429.6`) and the bot is still above it: move to
   `ULDUAR_THORIM_JUMP_START_POINT`, and once within **3 yd** `JumpTo(ULDUAR_THORIM_JUMP_END_POINT)`
   and `return true`.

Then delete branch (c) from `ThorimGauntletPositioningAction::Execute`
(`UldActions_Thorim.cpp:232-246`) and its matching clause in
`ThorimGauntletPositioningTrigger::IsActive`, so exactly one place owns the drop. That also retires
two defects in the old branch: a **0.5 yd** arrival precondition against `exact_waypoint` movement
that a bot can loop on forever, and a `return false` after a successful `JumpTo` that never claims
the tick.

`ULDUAR_THORIM_JUMP_END_POINT` (2137.88, -278.189, 419.667) is navprobe-clean — settles to 419.666 on
the arena floor.

## Fix 2 — give the gauntlet squad Thorim to hit

**File:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`

Only the gauntlet branch of `GetThorimDpsTarget` changes. After the Colossus and Rune Giant
fallbacks, before returning the null Rune Giant:

```
if the bot is above ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD and the Rune Giant is dead,
    return NoteThorimDpsTarget(bot, boss);
```

Gate on the bot's own Z, not the squad alone: a corridor bot at z 412 picking Thorim would be dragged
150 yd out of its lane. This is what lands the hit that starts phase 2, and it is the difference
between an encounter that progresses without a human and one that does not.

`ThorimHasDpsTarget` reads the same cached answer, so the targeting multiplier follows with no
further change.

## Fix 3 — pet rows in the snapshot

**Files:** `src/Bot/Obs/RaidObsSnapshot.cpp`, `src/Bot/Obs/RaidObsSession.cpp`,
`src/Bot/Obs/RaidObs.h`, `src/PlayerbotAIConfig.{h,cpp}`, `conf/playerbots.conf.dist`,
`tools/botobs/obstrace.py`, `tools/botobs/views.py`, `docs/systems/observability.md`

Bump `SCHEMA_VERSION` (`RaidObs.h:35`) to **10**, and `SUPPORTED_SCHEMA` / `READABLE_SCHEMAS`
(`obstrace.py:12-18`) to match. Additive only, so v4-v9 files stay readable.

- `BuildSnapshotPayload` (`RaidObsSnapshot.cpp:187-262`): inside the roster loop, after the player's
  own row, emit a `UnitRow` for each of `player->m_Controlled` that is alive, in world, on the same
  map and `IsPet() || IsGuardian()`. Skip `IsTotem()` — a shaman drops four and none of them move.
  `EnsureUnit` each. Follow the shape of the vehicle block just above, `ridden` set included, so a pet
  cannot be emitted twice. `dealt` stays 0 on the pet row: pet damage is already folded into the
  owner's column by `AccrueDamageDealt` and counting it twice would break the differencing.
- `EnsureUnit` (`RaidObsSession.cpp:279-295`): add `own`, the owner's guid key, wherever the unit has
  one. Turns "which bot owns Flaaghun" from a spell-list deduction into a field.
- New `AiPlayerbot.Obs.LogPets` (default 1), wired like `obsLogHeals`: declared in
  `PlayerbotAIConfig.h`, loaded in `PlayerbotAIConfig.cpp`, copied into `ObsConfig` and `LoadConfig`,
  documented in the conf.dist RAID OBSERVABILITY block. ~6 extra rows a snapshot here, against the
  40-row creature sweep.
- `postmortem.py --track` shows pets, and every place a pet is named gains its owner in parentheses.
  A pet row is a `snap.u` row like any other, so no reader needs structural change.

## Fix 4 — stop the trace claiming a human is in the arena when they are not

**File:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`

`AssignThorimSquads`'s final loop writes a squad for every roster member and defaults humans to
`Arena`, because the picks above it are all `IsBotPlayer`-gated. This pull, that put the human who
personally killed the Runic Colossus, the Rune Giant and started phase 2 on the arena side of every
number in the trace.

Label only — **no bot moves squad.** Every consumer of `GetThorimSquad`
(`ThorimStrayArenaPets`, `GetThorimDpsTarget`, the corridor and balcony triggers) runs inside a
trigger or action, which only ever evaluates for a bot, so a human's label is inert behaviour-wise
today.

- In that final loop, write a squad only for `IsBotPlayer(member)`.
- Add a small tick, rate-limited by `ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS` (500 ms) the way
  `TickRunicSmash` is, that sets each human's entry from position: `ThorimInArenaBox(member)` →
  `Arena`, else `Gauntlet`. `state.squads` is already a `RaidObs::ObsGuidMap<uint8>`, which emits a
  note only on change, so this costs one `thorim.squad` note per human per crossing and nothing else.
- Reuse `ThorimInArenaBox` rather than a new boundary — it is the same line the arena leash and the
  pet leash are held to.

Sizing the bot gauntlet down when a human walks the corridor is **not** in this fix: the squads latch
at pull time while the human is still standing in the arena, so it cannot be known up front, and
moving a bot mid-pull is a bigger change than the label. Judge the headcount from the next pull.

## Verification

Static, before any pull:

- `grep -rn "thorim balcony advance" src/Ai/Raid/Uld/` — six hits: trigger header + body, action
  header + body, both context files, and `UldStrategy.cpp`. A name missing from any wiring site fails
  silently.
- `grep -n "SCHEMA_VERSION" src/Bot/Obs/RaidObs.h` and the two constants in `obstrace.py` all say 10,
  with 9 still in `READABLE_SCHEMAS`.
- `python tools/botobs/postmortem.py env/dist/logs/botobs/603_1_thorim_1788288995.ndjson` still runs
  clean against the v9 file.
- The module cannot be compiled headless here. Hand the branch off for a build rather than claiming
  one.

In-game, one 25-man pull, then re-read the fresh trace:

1. **Nobody is stunned.** No `62241` or `63540` in the `aura` stream on any bot. Binary, and the whole
   point.
2. **The route is walked.** `thorim.balcony` steps 0 → 5 monotonically per gauntlet bot, and `--track`
   on any of them shows x ≈ 2151 through y -396..-332 rather than x ≈ 2135.
3. **Phase 2 starts on the bots' clock.** Thorim's health leaves 100% while he is still at z 438, and
   his z drops below 429.6 without a human having to do it.
4. **Nobody walks backwards.** No gauntlet bot's y goes back below -400 after passing -330, and
   `follow(fail)` after the drop stays in single digits instead of 106 and 152.
5. **They arrive.** Every living gauntlet bot is inside the arena box within ~10 s of Thorim landing.
6. **Gauntlet target uptime rises** from 62-65% — the 3:00 and 3:30 buckets (48%, 13-26%) are the two
   this fix owns; the travel dips at 0:30 and 2:00 are expected to stay.
7. **Time to phase 2 falls** from 3:15. Under ~2:30 means the arena is still near full strength when
   the boss lands, which is what all of this is for.
8. **Pets read directly.** Every pet has `snap.u` rows and an `own` field; arena-squad pets stay inside
   the arena box, and `thorim.petrecall` says whether the leash fired and lost or never fired.
9. **The human's label follows them.** `thorim.squad` flips to 2 when they enter the corridor and back
   to 1 if they drop into the arena, and no bot's label changes with it.

## Found while implementing

Three things the plan did not know, all now in the code.

**`NearThorimEncounter` could not see the balcony at all.** It gates on
`bot->GetPositionZ() < ULDUAR_THORIM_WING_MAX_Z` (425) to keep Hodir's floor at 433 out of the
encounter, and the hallway is at 438. So `GetThorimSquad` returned `None` for anyone up there, and
every Thorim node keyed off it went quiet the moment the squad climbed the ramp - including the old
balcony-jump branch, which was unreachable code rather than merely mis-gated. Neither height nor
distance separates the hallway from Hodir (its far end is 177 yd out, inside Hodir's own 136-176 yd
band), so the second half of the gate is a box, `ULDUAR_THORIM_BALCONY_BOX_*`, x 2100-2205,
y -455..-280, z 425-450. X does the work: Hodir sits at x 1980. Checked against every `ULDUAR_*`
anchor in the module - nothing but Thorim's own geometry is inside it.

**Phase 2 nodes had to be kept off anyone still up top.** With the gate fixed, `ThorimPhase2Active`
became true for a bot on the balcony, and a phase 2 spot handed out from up there is the same 300 yd
walk this whole change exists to stop. One guard in `TryGetThorimPhase2Spot`, which the positioning
trigger, the Lightning Charge trigger and both their actions all pass through.

**"Arrived" had to be a latch, not a distance test.** Standing on the jump start with Thorim 8.9 yd
away, the walk would re-anchor the bot every tick while `reach melee` pulled it onto the boss - the
same two-node tug-of-war `1283532ea` fixed in the arena. `ThorimAdvanceBalconyStep` therefore counts
one past the last waypoint, and `ULDUAR_THORIM_BALCONY_WAYPOINTS` means done: the node returns false
from then on and the combat nodes own the bot.

## Explicitly not in this plan

- **The healer split.** Arena 13 bodies / 1.48M damage / 2 healers against gauntlet 11 / 0.69M / 2.
  Left alone on purpose: the arena held 14/14 above 90% for two minutes and won the add war, so the
  gauntlet's clock is the lever. Changing both at once would make the next trace unreadable.
- **Resizing the bot gauntlet when a human runs it** — see Fix 4.
- **The lower corridor's master dependency.** `ThorimGauntletPositioningTrigger` opens with
  `if (!master) return false;` and branch (b) derives progress from the master's waypoint, so a
  bot-only raid never walks the corridor at all. Real, and the balcony node is deliberately written
  without it, but rewriting the corridor leg is a separate job.
- **The unwaypointed last 67 yd** of the lower corridor (waypoints stop at y -329, the Colossus is at
  y -396), which also means `ThorimRunicSmashAction` cannot resolve an index and does not fire on the
  final approach, where the smash is most dangerous.
- **Pets dodging anything.** Still no. They have the resistances to eat this fight.
- Writing the gauntlet into `docs/raids/ulduar/thorim.md`: the Paralytic Fields and their
  coordinates, the z > 430 phase 2 trigger, and the balcony route. Owed, and worth doing once the
  route is proven in a pull.
