# Freya: unpin the Sun Beam dodge, bail melee off dying lashers, anchor the camp on the pile

## Context

`e865b00bf` reverted the 16 yd lattice and put the raid back in one camp. It worked: the newest pull
`env/dist/logs/botobs/603_1_elder-stonebark_1788559467.ndjson` survived to **4:52**, against
1:53.9 for the lattice and 2:06-4:01 for the six earlier camp pulls. Army of the Dead, the frost
trap and the mage nova are all still firing and are out of scope here.

Three separate problems remain, measured across the three post-revert pulls (`1788558567`,
`1788559124`, `1788559467`).

### 1. Ranged bots stand in Sun Beam because their own channel pins them

`FreyaDodgeUnstableSunBeamAction` is not failing to fire — it fired **192 times, 158 accepted** in
`1788559467` alone. The moves are accepted and then do nothing.

`MoveTo` → `DoMovePoint` (`src/Ai/Base/Actions/MovementActions.cpp:1915`) does `mm->Clear()` then
`mm->MovePoint(...)`. AzerothCore's `PointMovementGenerator<T>::DoInitialize`
(`src/server/game/Movement/MovementGenerators/PointMovementGenerator.cpp:36`) returns **without
launching the spline** when `unit->IsMovementPreventedByCasting()`, and `DoUpdate` (`:119`) calls
`StopMoving()` and returns every tick while that holds. `Unit::IsMovementPreventedByCasting()`
ends in "prohibit movement for all other spell casts" — every cast qualifies except a channel
flagged `IsActionAllowedChannel()`. `MoveTo` still returns `Issued`, so the failure is invisible to
the AI *and* to the trace.

Measured, displacement within 1500 ms of an **accepted** move order:

| | orders | went nowhere (<1 yd) |
|---|---|---|
| not casting | 959 | 45 (5%) |
| casting | 223 | 20 (9%) |
| ...Mind Sear | 6 | 4 (**67%**) |
| ...Volley | 7 | 4 (**57%**) |
| ...Blizzard | 5 | 3 (**60%**) |
| ...Hurricane | 1 | 1 |

So it is not a raid-wide stall — it is concentrated in **channeled AoE**, which is exactly what
ranged bots cast into a lasher pile. Both Unstable Energy deaths where the dodge was actively
firing were channelers: `Malediction` (Mind Sear) and `Nightwarrior` (Volley). Nightwarrior sat at
exactly `(2357.77, -54.83)` for 3.0 s, 2.4 yd inside a 5 yd beam, through 4 move orders (3
accepted), and died.

A `cancel channel` action already exists (`src/Ai/Base/Actions/CancelChannelAction.cpp:15`) but its
triggers only fire on "too few enemies" (`VolleyChannelCheckTrigger`,
`src/Ai/Class/Hunter/HunterTriggers.cpp:190`) and sit at relevance 23, far under the dodge at 64.
It will never break a channel to save a bot.

Secondary: generic `avoid aoe` is a default action of the always-on `AvoidAoeStrategy` at
`ACTION_EMERGENCY` (90), outranking the dodge at `ACTION_RAID + 4` (64). It flees at
`MOVEMENT_COMBAT` with `fleeDis = min(radius + 1, fleeDistance)` — **≤5 yd**
(`MovementActions.cpp:2240`, `:2303`). In Nightwarrior's death it re-took the tick at 109.89 s and
replaced the dodge's 14 yd escape with its own 5 yd hop. No Freya multiplier suppresses it, while
`UldMultipliers_XT002.cpp:102-107` and `UldMultipliers_Razorscale.cpp:44-52` both do exactly that,
with comments describing this failure.

### 2. Melee die to Detonate because nothing moves them off a lasher that is about to blow

34 melee Detonate deaths across all camp pulls. They die stacked: **5 or more lashers sit inside the
15.5 yd blast radius of a melee bot 42% of the wave**, all ten 4.6% of it. Ten chained blasts at
~8k each is 80k, and the raid evaporates in under a second — in `1788559467`, Totemist, Justice and
Assasin died within 0.05 s of each other at 1:28.

A lasher's health is the usable warning, and it is a good one:

| lasher below | dwell before it detonates (n=120-137) |
|---|---|
| **15%** | median **2.6 s**, p25 1.5 s, p75 4.3 s |
| 25% | median 5.0 s |

2.6 s at 7 yd/s is 18 yd, enough to clear the 15.5 yd blast. Cost, measured as the share of wave
time a melee bot would spend outside: **26%** at a 15% gate (19% at 10%). Bounded, and unlike the
lattice it is temporary and per-bot.

The deleted `freya lasher pack step out` (last good copy `1283532ea`) is the right *shape* but the
wrong gate: it required 3+ lashers all under 20% inside an 8 yd pack radius, a conjunction that
fired once in six pulls. Per-lasher is what makes it fire.

The cast pin applies here too: at the 1:28 triple death, Totemist was casting Lightning Bolt and
Justice Hammer of Wrath, with **zero** move orders issued.

### 3. The ranged camp is anchored on a bot, not on anything about the fight

`GetFreyaRangedCampAnchor` (`src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp:404`) returns the
**lowest-GUID living ranged DPS bot**. Nothing in it reads Freya or the lashers. In all three pulls
that was `Agony`, a warlock; the whole camp stands wherever that one bot wandered.

Measured over the wave in `1788559467`: **23.8-35.7 yd from Freya** (median 27.1), **16.3-25.5 yd
from the lasher pile**, and the anchor drifted **27.5 yd between wave 1 and wave 2**. Gathering
itself works (10/10 ranged inside 10 yd in wave 1, 9/10 in wave 2). The accidental 16-25 yd standoff
is roughly right — outside the 15.5 yd blast, inside `AiPlayerbot.SpellDistance` (28.5) — which is
what to make deterministic. Latent risk: the anchor is re-picked from *living* bots each tick, so
when it dies the camp jumps to the next GUID (observed at the 1788558567 wipe, twice in 0.3 s).

## Approach

Three independent changes. None of them zeroes `ReachTargetAction` — that is what broke
`45d9a3e36`, and it stays untouched.

### A. Unpin the escapes

Add a file-local helper in `src/Ai/Raid/Uld/Action/UldActions_Freya.cpp`, used by both Freya
escapes, that clears a cast **only when it is actually blocking movement**:

```cpp
// A point move never launches its spline while the bot is casting - PointMovementGenerator bails on
// IsMovementPreventedByCasting and MoveTo still reports success - so a channeled Blizzard or Volley
// pins a bot inside the blast it was told to leave.
void FreyaClearCastBlockingMove(Player* bot)
{
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);
}
```

Call it immediately before the `MoveTo` in `FreyaDodgeUnstableSunBeamAction::Execute`
(`UldActions_Freya.cpp:341-415`) and in the new step-out. Gating on
`IsMovementPreventedByCasting()` is what keeps it from shredding the rotation on the 192 ticks a
pull the dodge fires — it only bites when the bot is genuinely stuck.

Also break the dodge's arrival latch (`UldActions_Freya.cpp:384-391`) when the bot is pinned: the
latch returns true without touching the motion master, so a pinned bot would otherwise sit inside
it for the full `ULDUAR_FREYA_SUN_BEAM_LATCH_MS` (3000). Add `!bot->IsMovementPreventedByCasting()`
to the latch condition.

### B. Suppress the generic mover during Freya

New `FreyaAvoidAoeHoldMultiplier` in `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Freya.{h,cpp}`,
registered in `UldStrategy.cpp` beside the other five (`:889-895`). Copy the shape from
`UldMultipliers_XT002.cpp:102-107`:

```cpp
if (dynamic_cast<AvoidAoeAction*>(action))
    return 0.0f;
```

Gated on the Freya encounter being active, like the other Freya multipliers. **Movement-suppression
only, and only this one class** — Nature Bomb, Sun Beam and the new step-out all have dedicated
nodes that place the bot with knowledge of every hazard, which the generic 5 yd hop does not have.

### C. Step melee off a lasher that is about to detonate

Revive the step-out with a per-lasher health gate.

- **Constant**, `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h`, in the Detonating Lasher block:
  `constexpr float ULDUAR_FREYA_LASHER_BAIL_PCT = 15.0f;` — a lasher dwells below it a median 2.6 s,
  which is the walk out of a 15.5 yd blast.
- **Helper**, `UldEncounter_Freya.{h,cpp}`:
  `std::vector<Position> GetFreyaDetonatingLasherPositions(PlayerbotAI*, FreyaWaveState const&, float maxPct, float radius);`
  living detonating lashers under `maxPct` within `radius` of the bot.
- **Trigger** `FreyaLasherAboutToBlowTrigger` ("freya lasher about to blow"),
  `UldTriggers_Freya.{h,cpp}`: melee only — bail for tanks (they would drop Freya or the
  Conservator) and for ranged/healers (already outside at the camp). Active when
  `GetFreyaDetonatingLasherPositions(..., ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_DETONATE_RADIUS)`
  is non-empty.
- **Action** `FreyaLasherAboutToBlowAction`, `UldActions_Freya.{h,cpp}`: `FreyaClearCastBlockingMove`,
  then `FindNearestPositionClearOfHazards(bot, lowLashers, ULDUAR_FREYA_LASHER_PACK_CLEAR,
  ULDUAR_FREYA_HAZARD_SEARCH_RADIUS)` and `MoveTo(..., exact_waypoint=true,
  MovementPriority::MOVEMENT_FORCED, lessDelay=true)`. `ULDUAR_FREYA_LASHER_PACK_CLEAR` (16.0) is
  already in the header, one yard past Detonate, and is the right clearance. Carry the same
  latch idiom as the beam dodge so re-deriving each tick does not re-clear the motion master.
  Clear only the **low** lashers, not every lasher: clearing all ten would push melee out of the
  fight entirely, which is the lattice failure.
- **Wiring**, `UldStrategy.cpp`: `ACTION_RAID + 3` — above nova/trap/army (`+2`) and above
  `reach melee`, below the Sun Beam dodge (`+4`). Register the pair in `UldTriggerContext.h` /
  `UldActionContext.h`.

### D. Anchor the camp on the pile

Replace the bot anchor with a pile-derived spot, keeping the existing tolerance as the latch.

- **Constant**: `constexpr float ULDUAR_FREYA_LASHER_CAMP_STANDOFF = 20.0f;` — outside the 15.5 yd
  blast, inside `AiPlayerbot.SpellDistance` (28.5), and it is where the camp accidentally sat
  (16-25 yd) in the 4:52 pull.
- **Helper** `Position GetFreyaLasherCampSpot(PlayerbotAI*, FreyaWaveState const&)`: centroid of
  living detonating lashers, then a point `STANDOFF` out along the bearing from the centroid toward
  the raid's current centre, so nobody crosses the pile. Validate with
  `bot->GetMap()->CheckCollisionAndGetValidCoords(...)` as `FindNearestPositionClearOfHazards` does
  (`src/Util/EncounterHelpers.cpp:331-390`); sweep bearings if the first is invalid.
- **Fallback to `GetFreyaRangedCampAnchor`** when `state.detonatingLashers` is empty or no bearing
  validates. Keep the existing function — a live bot is guaranteed to be on the mesh, which is why
  it exists, and it is still the right answer outside the wave.
- `FreyaRangedCampTrigger` / `FreyaRangedCampAction` switch to the new spot. Tolerances stay
  (`_RANGED_CAMP_TOLERANCE` 10, `_HEALER_CAMP_TOLERANCE` 15): every bot derives the same spot
  deterministically, and standing down inside the tolerance is what stops the churn.
- Priority stays `ACTION_RAID`, movement stays `MOVEMENT_COMBAT`. Gathering is still the
  lowest-value thing a bot can be doing here.

### Out of scope

- Army of the Dead, frost trap and frost nova — all confirmed working, untouched.
- `ReachTargetAction`, `FreyaSetDpsPriorityAction`'s reach clamp, `ULDUAR_FREYA_MELEE_LASHER_RANGE`.
- `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` (12.0) vs the 5.0 yd the recorder reports. **Flagged, not
  changed:** Unstable Energy is a lingering debuff (~7 s after leaving), so damage ticks out to 6-7
  yd are bots that already left, and the recorded radius is not proof of the trigger radius. Confirm
  against the DBC before touching it.
- `FreyaLasherFinishAoeMultiplier`, `GetFreyaFinishingPackNear`, `IsFreyaLasherPackFinishing`,
  `ULDUAR_FREYA_LASHER_PACK_CLEAR` — all still live (`UldMultipliers_Freya.cpp:103`), leave them.

## Docs

`docs/engine/pitfalls.md` is referenced from `CLAUDE.md`, so **run `/compact-docs-writer` before
editing either doc**, one invocation for the whole cycle.

- **`docs/engine/pitfalls.md`** — new bullet: an accepted `MoveTo` is not a move. The point
  generator silently refuses to launch while the bot is casting and `MoveTo` still returns success,
  so a channeled AoE pins a bot inside the hazard its dodge just told it to leave; judge a
  positioning node by measured displacement, never by its verdict.
- **`docs/raids/ulduar/freya.md`** — record the three findings with the numbers above: the cast pin
  and the channel list, the lasher-health dwell table that makes 15% a usable gate, and the camp
  anchor's real geometry.
- **Plan doc** to `docs/plans/freya-beam-pin-and-lasher-bail/freya-beam-pin-and-lasher-bail.PLAN.md`
  per the plans-directory rule.

## Verification

The module cannot be compiled headless here, so step 2 onward is a hand-off.

1. **Static**: `freya lasher about to blow` resolves in both context maps; no dangling refs;
   `python apps/codestyle/codestyle-cpp.py`.
2. **Build** the worldserver in Docker. The last two cycles both surfaced errors only at build time.
3. **Re-pull** Freya 25 hard mode and capture a trace.
4. **Measure against `1788559467`** (4:52, the best pull so far). Scripts used to produce every
   figure above are in the session scratchpad; the checks that matter:
   - Deaths to Unstable Energy 62865: **2** in `1788559467` → expect 0-1.
   - Accepted move orders that produce <1 yd while channeling: **57-67%** for Mind
     Sear/Volley/Blizzard → expect under 15%.
   - Melee deaths to Detonate 62937: **6** in `1788559467`, 34 across all camp pulls → expect a
     clear drop.
   - **Guard against the lattice failure**: melee DPS on the wave must stay in the 40k+ band, and
     median bot-to-nearest-lasher must stay under 10 yd. If melee output falls, the step-out
     clearance or the 15% gate is too aggressive — tighten the gate, do not widen the camp.
   - Lashers killed per wave must not regress; the wave cleared in `1788559467`.
   - Army of the Dead casts and ghoul taunts must not regress from **9 and 213**.
   - Ranged camp: standoff from the pile centroid should hold near 20 yd instead of drifting
     16-25, and should not jump when a bot dies.
5. `python tools/botobs/postmortem.py <trace>` and `--stalls` for anything still holding station.
