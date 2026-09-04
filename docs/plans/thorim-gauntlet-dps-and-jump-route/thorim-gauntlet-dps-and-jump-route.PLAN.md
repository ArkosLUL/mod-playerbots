# Thorim: more gauntlet DPS, and stop the jump-down beeline crossing the traps

Traces (both 2026-09-04, schema v10, 25 man, 2 humans):

- **A** `env/dist/logs/botobs/603_1_thorim_1788551616.ndjson` — 22:59, reached phase 2, wipe at 5:15.6
- **B** `env/dist/logs/botobs/603_1_thorim_1788552114.ndjson` — 23:05, wipe at 3:45.8, never reached the
  Ancient Rune Giant

Read with `python modules/mod-playerbots/tools/botobs/postmortem.py <file> [--notes|--stalls|--track]`.
Timestamps in the NDJSON are **milliseconds**.

The build under test already contains `8c211ec2d` — confirmed from trace A, which carries 94
`thorim balcony guard` veto records and 396 `thorim balcony advance action` move records against 50
`follow`. Both of the previous round's fixes held (see "What already works").

On commit, copy this file to
`docs/plans/thorim-gauntlet-dps-and-jump-route/thorim-gauntlet-dps-and-jump-route.PLAN.md`.

---

## Context

Two complaints: the gauntlet squad cannot beat the hard-mode timer, and the bots were caught in the
Paralytic Field traps again. Both reproduce in trace A. They are unrelated to each other, and the
trap failure has a completely different cause from the one fixed last round.

### The deadline is ~170 s from the pull

From `src/server/scripts/Northrend/Ulduar/Ulduar/boss_thorim.cpp`:

- `EVENT_THORIM_START_PHASE1` fires **20 s** after the encounter starts (`:479`).
- That event sends `ACTION_SIF_START_DOMINION`, and Sif schedules `EVENT_SIF_FINISH_DOMINION` at
  **150 s** (`:817`). On that event she despawns.
- Hard mode is granted in `DamageTaken` (`:511`) when a **player above z 430** damages Thorim while
  `_isHitAllowed`.

So the raid has ~170 s from the pull to put damage on Thorim from the upper hallway. Sif appears once
in each trace as a unit record (A at 0:44.8, B at 0:41.3) and never again — hard mode was missed in
both.

### Where trace A's 231 s went

| window | what | gauntlet damage |
|---|---|---|
| 0:00 – 0:27.7 | squad idle in the arena, DPSing adds, waiting for the human to lead | 1.53 M |
| 0:27.7 – 0:50 | walk to the corridor | ~0 |
| 0:50 – 3:35.4 | corridor: Iron Ring Guards, Runic Colossus (1:27–2:17), Iron Honor Guards, Ancient Rune Giant (2:53.7–3:35.4) | 3.87 M |
| 3:35.4 – 3:49.3 | walk up the ramp and the hallway | ~0 |
| 3:49.3 | **Deathsong (human) tags Thorim** from (2151.7, −331.1, z 438.2) | — |
| ~3:51.0 | Thorim jumps to the arena floor. Phase 2 starts at **231 s** | — |

Corridor phase, 165 s: the 10 gauntlet bots dealt 3,865,880 at **23,430 dps**; the 7 DPS-role bots
averaged **3,075 dps** each. The 12 arena DPS bots averaged 4,214 dps each over the same window — the
gap is uptime lost to walking between packs, not bot quality, so a bot moved into the corridor should
be costed at the gauntlet rate.

**Trace B** never got past the Iron Honor Guards. Runic Colossus down ~2:07.7, then the squad sat at
y −426…−440 from 2:30 until the raid wiped at 3:35. Deathsong died there at 2:22. No Ancient Rune
Giant unit record exists in the trace at all, and no Paralytic Field — they never reached the hallway.
B's arena wipe is the downstream cost of a slow corridor: the add waves keep spawning until phase 2.

### What +2 DPS buys, and what it does not

Required corridor damage is ~3.87 M (essentially all of it lands on kills the squad is gated on).
Two more bots at the observed gauntlet rate of ~3,075 dps take the squad from 23,430 to ~29,580 dps,
which is **~30–34 s off the corridor phase**. That moves phase 2 from 231 s to roughly **198 s**.

Still ~28 s past the deadline. The remaining 28 s is the opening wait: `ThorimGauntletPositioningTrigger`
(`UldTriggers_Thorim.cpp:65`) is gated entirely on the master's position, so the squad does not move
until a human walks to `ULDUAR_THORIM_NEAR_ENTRANCE_POSITION`. First accepted corridor move was
0:27.683 in A and 0:24.794 in B. **Decided: not fixing that this round** — the human keeps leading.
Recorded here so the shortfall is not a surprise.

### The traps: the jump-down branch throws the route away

The waypoint chain is correct and it worked. Assasin walked B1 → B2 → B3 hugging the east wall and its
closest approach to the south bunny was **15.9 yd** — clear. Deathsong walked the same ground and was
never trapped.

What broke it is `ThorimBalconyAdvanceAction::Execute` (`UldActions_Thorim.cpp:241`). Its first branch
fires the moment Thorim drops below the floor threshold and **replaces the chain with a straight
`MoveTo` at `ULDUAR_THORIM_JUMP_START_POINT`**:

```cpp
if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
{
    if (bot->GetExactDist2d(&ULDUAR_THORIM_JUMP_START_POINT) > ULDUAR_THORIM_JUMP_START_TOLERANCE)
        return MoveTo(... ULDUAR_THORIM_JUMP_START_POINT ...);   // the beeline
    return JumpTo(... ULDUAR_THORIM_JUMP_END_POINT ...);
}
```

JUMP_START is at x 2137.1, on the centre line — and so are both bunnies, at (2134.9, −390.776) and
(2134.93, −339.696). The route exists precisely because the centre line is not walkable. So the
beeline runs from wherever the bot is straight back across it.

Trace A, all ten bots switched to JUMP_START within 0.7 s of each other (3:51.024 – 3:51.724), and all
ten were stunned:

| bot | caught at | nearest bunny |
|---|---|---|
| Holylight | 3:52.16 (2140.8, −400.4) | 11.3 yd (south) |
| Assasin | 3:56.67 (2147.3, −341.6) | 12.5 yd (north) |
| Bulwark, Trueshot, Prayer, Malediction, Agony, Hellflame, Nightwarrior, Obliteration | 3:56.7 – 3:59.3 | 11.7 – 13.6 yd (north) |

Holylight was at (2141.0, −407.7) when the branch flipped — one step short of BALCONY_1. Its beeline to
JUMP_START passes the south bunny at **5.5 yd**. Assasin was at BALCONY_3 (2152, −365); its beeline
passes the north bunny at **10.2 yd**. Its observed track after 3:53 matches a straight line to
JUMP_START to within 0.01 in the x/y slope, so this is measured, not inferred.

Cost: stunned 9–15 s each, `r=blocked` on 123 balcony moves, and they did not land on the arena floor
until 4:20.3 – 4:22.1. Walking the remaining chain at the observed ~6.9 yd/s would have put them at
JUMP_START around 4:05, so the traps cost roughly **15 s of ten-bot phase 2 uptime**.

### What already works — do not touch

Both `8c211ec2d` fixes held, and the plan must not regress them:

- The balcony route owns the walk. 396 balcony move records against 50 `follow` and 7 `reach melee`
  in the 3:20–4:20 window, at combat priority.
- Nothing walked back down the corridor. Last round the squad covered 321–380 yd the wrong way; this
  time every bot went forward and jumped down.
- The cached boss guid holds above the ramp — the balcony node stayed live through Thorim's jump,
  which is what let the "he jumped" branch fire at all.

---

## Fix 1 — walk the chain to the edge instead of beelining

**File:** `src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp`, `ThorimBalconyAdvanceAction::Execute` (`:241`)

`ULDUAR_THORIM_JUMP_START_POINT` is already the last entry in `balconyWaypoints`
(`UldEncounter_Thorim.cpp:108`), so the chain ends where the jump begins. The branch does not need a
route of its own — it only needs to add the jump.

Restructure so the step latch always advances and the walk always comes from the chain:

```cpp
bool const bossDown = boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;
uint8 const step = ThorimAdvanceBalconyStep(bot);

if (bossDown && (step >= ULDUAR_THORIM_BALCONY_WAYPOINTS ||
                 bot->GetExactDist2d(&ULDUAR_THORIM_JUMP_START_POINT) <= ULDUAR_THORIM_JUMP_START_TOLERANCE))
{
    return JumpTo(bot->GetMapId(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionX(),
                  ULDUAR_THORIM_JUMP_END_POINT.GetPositionY(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionZ(),
                  MovementPriority::MOVEMENT_COMBAT);
}

if (step >= ULDUAR_THORIM_BALCONY_WAYPOINTS)
    return false;

Position const& waypoint = GetThorimBalconyWaypoint(step);
return MoveTo(bot->GetMapId(), waypoint.GetPositionX(), waypoint.GetPositionY(), waypoint.GetPositionZ(),
              false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
```

Two details that matter:

- `ThorimAdvanceBalconyStep` must run before the jump test, or the latch never reaches
  `ULDUAR_THORIM_BALCONY_WAYPOINTS` and a bot standing on the edge never jumps.
- The `step >= WAYPOINTS` arm of the jump test is what closes the gap between
  `ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE` (6.0) and `ULDUAR_THORIM_JUMP_START_TOLERANCE` (3.0). Without
  it a bot 4 yd from the edge latches "arrived", falls through to `return false`, and never jumps.

Rewrite the branch comment to say why the chain has to survive the jump: the direct line to the edge
runs down the centre, which is where the bunnies are.

**Checked, no change needed.** Every remaining leg clears both bunnies: B3→B4 16.3 yd, B4→B5 16.9 yd,
B5→JUMP_START ≥21.7 yd. Against an observed field reach of ~13.6 yd that is thin but it is what the
route already delivers, and the one bot that stayed on it (Deathsong) walked through clean.

## Fix 2 — two more DPS in the gauntlet

**File:** `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp`, `AssignThorimSquads` (`:249`)

```cpp
uint32 dpsQuota = twentyFive ? 7 : 3;
```
becomes
```cpp
uint32 dpsQuota = twentyFive ? 9 : 4;
```

25-man gauntlet goes from 10 to 12 (1 tank + 2 healers + 9 DPS); 10-man from 5 to 6. The arena keeps
the rest and is still floored by `ULDUAR_THORIM_ARENA_MIN_MEMBERS` (3) through `cap`, which is
`roster.size() - 3` — 22 on a 25-man roster, so the new quota is nowhere near it.

The `else ++dpsQuota` on the one-tank path (`:295`) still applies on top, so a raid with no spare tank
sends 10 DPS instead of 9 — which is what trace B's comp would have got.

Replace the quota comment with why the number is what it is: the corridor is a 170 s race and 10 bots
measured 23.4 k dps against ~3.9 M of gated health, which does not finish in time.

**Nothing else needs touching.** Swept every squad consumer in `src/Ai/Raid/Uld/`:

- `ULDUAR_THORIM_ARENA_MIN_MEMBERS` has exactly one use site, the `cap` above.
- The corridor lane index is a *progress station* shared by the whole squad, not a per-bot slot —
  `GetThorimGauntletWaypoint` clamps and every bot is sent to the same `Position`, so 12 bots behave
  as 10 do.
- The phase 2 ring is not squad-filtered at all (`GetThorimPhase2Role`, `TryGetThorimPhase2Spot`), so
  it sees the same bodies either way; `EnsureMeleeSlot` is least-loaded across 3 slots and already
  designed for more melee than slots.
- The phase 1 arena ring is rebuilt live from membership and `ULDUAR_THORIM_ARENA_RING_INNER_SLOTS` is
  a cap under `min()`, not an allocation.

The one behavioural side effect to watch: the DPS loop picks in raw roster order with no melee/ranged
split, so the two bots taken may both be ranged. The arena's ranged ring then re-spaces two bodies
thinner. It degrades cleanly — ranged DPS sort ahead of healers in `ringMembers` — but it is the thing
to eyeball on the test pull.

### Deliberately not doing

- **No change to the opening wait.** Decided. `ThorimGauntletPositioningTrigger`'s master gate stays,
  so the squad still leaves when the human does. Worth ~28 s if it is ever revisited.
- **No new Paralytic Field dodge node.** The route already clears both bunnies; the bug was a branch
  that stopped using it.
- **No new jump-down point.** A shorter drop off the east wall would need a `navprobe` check against
  the `ac-client-data` volume, and the existing edge works.

### Still open from last round, not fixed here

`ThorimGauntletPositioningTrigger` (`UldTriggers_Thorim.cpp:67`) and `ThorimFallFromFloorTrigger`
(`:139`) both carry a hard-coded `> 110.0f` bail from the arena centre, and the left lane's last three
corridor waypoints are 117.3 / 120.8 / 126.1 yd out. A left-lane squad loses its corridor node for the
final stretch and finishes on `follow`. Same class of bug as the one `8c211ec2d` fixed; own pass.

---

## Verification

Static, before any pull:

- `grep -n "JUMP_START_POINT" src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp` shows it only in the
  distance test, never as a `MoveTo` destination.
- `grep -n "dpsQuota" src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp` shows `9 : 4`.
- `python apps/codestyle/codestyle-cpp.py` passes; no line over 120 columns; files stay LF.
- `python tools/botobs/postmortem.py env/dist/logs/botobs/603_1_thorim_1788551616.ndjson` still runs.
- The module cannot be compiled headless here. Hand the branch off for a build rather than claiming one.

In-game, one 25-man pull, then re-read the fresh trace:

1. **`thorim.squad` shows 12 bots on squad 2**, one of them the tank, two healers.
2. **No Paralytic Field aura (62241 / 63540) on anyone.** Was all ten at 3:52–3:59. This is the headline.
3. **After Thorim jumps, balcony move destinations still step through B4 (2151, −332) and B5
   (2137.5, −318)** before JUMP_START. Was a single hop to JUMP_START from wherever each bot stood.
4. **The squad stays east of x 2144 until it is north of y −330.**
5. **They still land.** Expect `JumpTo` toward (2137.88, −278.19, 419.67) and a floor arrival well
   inside the 4:20 – 4:22 this trace produced.
6. **The corridor clock moves.** Runic Colossus and Ancient Rune Giant should each die meaningfully
   sooner than 2:17 and 3:35; phase 2 should start near 198 s rather than 231 s. It will most likely
   still miss the 170 s hard-mode window — that is the opening wait, not this change.
7. **Regression on the arena.** Two fewer DPS there: watch for arena deaths during phase 1, which
   neither of today's traces had before 3:35 (B) or 4:56 (A).
8. **Regression on last round's fixes.** `thorim balcony advance action` still owns the walk (`follow`
   move records for a gauntlet bot above z 429.6 should stay at zero) and nobody walks back down the
   corridor.
