# Mimiron Firefighter: fight phase 1 in the west, stack the ranged, and let the fire dodge move

## Context

Two Firefighter attempts on the 09th (`603_1_mimiron_1788982366` "n1", `603_1_mimiron_1788982770`
"n2"), the first two pulls after the phase-1 latch fix in `6fcee411c`. That fix worked: ranged cast
uptime is 63.5 % / 65.5 % across live phase 1, ranged damage is 3,643 / 3,662 dps per bot, and both
pulls reached phase 2 for the first time in three days.

Both still wiped, and both wiped **in phase 2**.

| | phase 1 (0-112 s) | handover (112-160 s) | phase 2 (160 s+) |
|---|---|---|---|
| damage taken /s | 9,910 / 12,071 | 6,687 / 3,470 | **18,022 / 18,463** |
| effective HPS | 10,904 / 12,108 | 5,555 / 3,433 | 14,034 / 13,438 |
| overheal | 63 % / 61 % | 69 % / 75 % | **20 % / 19 %** |
| bot deaths | 2 / 1 | 0 / 2 | 23 / 23 |

Phase 1 is comfortable. Phase 2 runs a 4,000-5,000 HPS deficit with healers already flat out, and the
raid dies in 89-128 s to Heat Wave (40 % / 31 %), Rapid Burst (38 % / 48 %) and ground Flames (20 % /
21 %). Heat Wave 64533 fires every 10 s and lands out to 46-53 yd, so it is not dodgeable and healing
simply has to cover it. Flames are the half that is a positioning problem, and Flames are the top
damage source in the last 8 s of roughly half the deaths.

### Two findings that shape everything below

**1. Fire seeds on players, not on the boss.** `boss_mimiron.cpp:427-443`: Mimiron picks three random
alive players and drops a flame node 5 yd from each, every 30 s, for the whole encounter.
`npc_ulduar_flames_initial::SpreadFlame` (`:2284`) grows each chain toward **whoever is nearest its
head**. The middle of the arena burns because the raid stands there. Measured: 70 % of phase-1 fire
damage is taken inside 20 yd of the room centre, and 53 % / 54 % of phase-2 fire damage likewise.

Moving the fight west is therefore the right fix, but it works by moving the *raid*. One expectation
to correct: attributing phase-2 flame damage to the most recent seed batch, **85 % (n1) / 53 % (n2)
of it follows a batch seeded during phase 2 itself**. A clean arena does not stay clean - what it
buys is the **opening** of phase 2, where the raid arrives on empty ground with a dodge ladder that
has somewhere to go instead of inheriting a saturated field. That is exactly when the raid is at full
strength and started dying (first phase-2 death 15 s in).

**2. The fire dodge is movement-locked, not out of options.** `MimironFleeAction` tries 11 bearings
on each of four rungs, screens each against mines, the barrage, fire, bombs and Rapid Burst, then
calls `MoveTo`, **throws the result away and counts nothing**. In n1 the ladder reported `none` on
every rung 2,159-2,173 times and fell through to an unscreened `MoveAway` 2,156 times, against 320
successful dodges (13 %). But on those failures the hazard filters had refused only **5.8 of 11
bearings on average**, and in 60 calls they refused **zero**. The move stream names the cause:
`mimiron dodge flames action` logged **1,249 moves refused with `r=wait` against 387 issued**, median
hold 917 ms, p90 3.8 s.

`IsWaitingForLastMove` returns false only when `priority > lastMove.priority` - **strictly greater**.
The fire dodge issues at `MOVEMENT_COMBAT` (`UldActions_Mimiron.cpp:706`) and so does the ranged
formation (`:468`), so a formation leg blocks the dodge for its whole duration and the bot stands and
burns. Every other Mimiron hazard dodge is already `MOVEMENT_FORCED` - Shock Blast `:247`, barrage
`:435`, Rocket Strike `:503`, Rapid Burst `:743`, Frost Bomb `:763`.

### The cleanse window, answered rather than chased

`EVENT_FLAME_SUPPRESSION_50000` is scheduled once at **60 s** into phase 1
(`boss_mimiron.cpp:1015`), never repeats, and the MK II then casts Flame Suppressant 64570 and Clear
Fires 65229. Seeds run every 30 s from 7 s, so the window between the cleanse and the next batch is
**81 s to 97 s**. The MK II is **8,363,521 HP**, takes 90 s of live combat in both pulls (~93,000
effective dps on it), and at 97 s is still at **11.2 % / 11.9 %** - the window is missed by about
12 s, or **+13 % phase-1 damage**.

That 13 % is not available from positioning. Ranged are already fine; the MK II already holds still
(109-120 yd of travel across the whole phase); the gap is melee, in melee range only **59.8 % /
50.9 %** of live phase 1, and most of the dead time is three Shock Blast holds at 18 yd which are
correct - that trigger clears the instant the cast ends. **Deferred by decision**: the reposition
retires the question anyway, since the 97 s batch seeds out west either way, so whether the MK II
dies at 97 s or 112 s stops deciding what the centre looks like. Re-measure melee uptime against a
west fight before opening it as its own change.

No special pull handling is needed: at the 7 s seed the raid is still at the door, **62.9 / 63.0 yd**
from the centre, so the first batch already lands in the west. It is batches two onward (22.2 /
16.6 yd median at 37 s) that burn the middle.

## Change 1 - fight phase 1 in the west

All of this is Firefighter-gated (`IsMimironHardModeActive`). Normal mode has no fire, so it keeps the
room centre and the existing ring - there is nothing to buy there and a 53 yd drag to pay for it.

**New constants** in `UldEncounter_Mimiron.h` / `.cpp`:

```cpp
const Position ULDUAR_MIMIRON_PHASE1_TANK_SPOT  = Position(2691.5762f, 2568.5315f, 364.3138f);
const Position ULDUAR_MIMIRON_PHASE1_STACK_SPOT = Position(2697.0f,    2588.0f,    364.3138f);
```

Both navprobe-verified on map 603 with `--nav 0x09`, reading the settled Z rather than the on-mesh
count:

- **Tank spot**: 0.223 yd to poly, settles flat at **Z 364.314**. Rings around it are 16/16 at 12 yd
  and 14/16 at 18 and 24 yd, the failures all west in the raised doorway alcove. 53.1 yd from the
  room centre, and **51.5 yd from Mimiron's own spawn** at (2742.53, 2560.99) (`acore_world.creature`),
  which matters because he evades above **80 yd** from his home position every tick
  (`boss_mimiron.cpp:394-398`) and is the only leash in the encounter - the MK II has none of its own.
  28 yd of margin.
- **Stack spot**: 0.223 to poly, settles at **Z 364.314**, **12/12 on mesh at 6 yd and 11/12 at
  10 yd**, with ~10 yd of floor behind it before the north wall at about y 2598. It sits 5 yd east of
  the tank spot's own x on purpose: there is a mesh hole against the west wall from y 2582 to 2591
  (3.6 to 4.8 yd to poly, Z never settles) and a stack placed due north of the tank spot lands in it.
- **Geometry**: stack to tank spot is **20.2 yd**, which is outside
  `ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST` (18), so ranged and healers never take the Shock Blast flee
  at all - today they do, three times a phase. Stack to room centre is **51.1 yd**, well clear of the
  24 yd ground phases 2-4 are fought on.

**`DeriveMimironSpreadSlot`** (`UldEncounter_Mimiron.cpp`):

- `p1tank` branch returns `ULDUAR_MIMIRON_PHASE1_TANK_SPOT` instead of `ULDUAR_MIMIRON_ROOM_CENTER`.
- New `p1stack` branch, placed straight after `p1tank` so the main tank keeps its anchor, for ranged
  and healers in Firefighter phase 1. Melee fall through to the existing ranged-only gate and keep
  chasing the boss, which is what they should do.
- Guard phase 1 off the `phase4` bool already computed in that function, not off the MK II being
  alive - it is alive in phase 4 too, where `GetMimironRingFocus` returns VX-001.
- `hmwedge` and `ring` are then reached only in phases 2-4, so `ClampMimironAnchorToRoom` needs no
  change: the 40 yd clamp only ever saw a boss near the centre again.

**Holding the stack, and when to give ground.** The stack is a fixed world point - that is what makes
the fire chains converge on one place instead of smearing behind a drifting boss. It only slides when
holding still would stop the raid casting:

```cpp
// Fixed on purpose: chains grow toward whoever is nearest their head, so a stack that tracks the
// boss drags the field around with it. Gives ground only to stay in range - "reach spell" is
// ACTION_HIGH against this formation at ACTION_RAID, so a slot past casting range deadlocks
// rather than self-correcting.
float const reach = sPlayerbotAIConfig.spellDistance - ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN;
```

If the MK II is further than `reach` from the stack point, move the stack along the line toward it to
exactly `reach`; otherwise return the constant unchanged. Covers boss drift and the main tank dying.

**Disperse distance.** The stack fights the generic unstacker, which currently holds ranged 5.5 yd
apart in phase 1. Drop it to **3.0** under Firefighter, keep **5.5** in normal mode. Both the trigger
and the action must read the same helper - this is the exact latch that broke in `ab43383d3`, where
the action wrote a constant and the trigger still tested a literal:

```cpp
float GetMimironPhase1DisperseDistance(PlayerbotAI* botAI);
```

The header's stated invariant (`PHASE3_SPACING > DISPERSE_DISTANCE > Napalm's 5 yd`) is deliberately
broken on its lower half now, and the comment has to say so and why.

**What the stack actually looks like, and the Napalm price.**
`ULDUAR_MIMIRON_SPREAD_TOLERANCE` is 5, so a bot within 5 yd of the slot is left alone; with a 3 yd
unstacker floor that gives a blob roughly 10 yd across, not a point. That is deliberate. Napalm Shell
splashes 5 yd, and today it lands 32 casts at 1.38 victims (n1) and 40 at 2.20 with **7 casts hitting
6 bots** (n2), for 26 % / 37 % of phase-1 damage taken. A 10 yd blob means a Napalm on the edge of
the group clips part of it rather than all of it; a literal point would hand it every caster and
phase 1 only has about 4,000 HPS of headroom. Watch this number in the next trace - it is the one
place this change can make phase 1 worse.

## Change 2 - drop the between-phase formations, follow master

Between phases nothing is attackable, and the three staging shapes go: `hmlap` (the Firefighter rim
lap), `stagemelee` and `stagering`. `DeriveMimironSpreadSlot` returns false for everyone while
staging, the engine falls through to `follow` at relevance 1.0, and the human leads.

The evidence says this costs nothing: during both handovers the masters were already at **43.1 /
54.1** and **45.3 / 53.1** yd median from the room centre, against the lap's 51.5 / 48.8. The fire
dodge sits at `ACTION_RAID + 4` and is unaffected, so bots still leave fire while following.

**One exception: `p4tank` stays.** It is not a staging shape, it is a boss-holding spot that happens
to be reachable while staging, and it holds VX-001's chassis still so the Laser Barrage cone apex
does not move - every phase-4 bearing, radius and offset is calculated against a stationary apex.

Dead code to remove with it: `GetMimironLapSlot`, `GetMimironStagingMeleeSlot`, `IsMimironLapSlot`
and its two call sites (the exemptions in `MimironArcSpreadAction::Execute` and
`MimironArcSpreadTrigger::IsActive`), `ULDUAR_MIMIRON_HM_LAP_RADIUS` / `_STEP` / `_STEP_MS` / `_ARC`,
`ULDUAR_MIMIRON_HM_LAP_SWEEP_STEPS` and `ULDUAR_MIMIRON_STAGING_MELEE_RADIUS`. Keep
`GetMimironStagingFocus` and `ULDUAR_MIMIRON_STAGING_SEARCH_RANGE`: the `phase4` test still needs
them while staging.

Note this reverts F4 from `ab43383d3`, which was never tested on its merits. The `hmlap` verification
items from that plan are void.

## Change 3 - make the fire dodge move

**a. `UldActions_Mimiron.cpp:706`** - `MOVEMENT_COMBAT` to `MOVEMENT_FORCED`, matching every other
hazard dodge in the file.

**b. Stand the trigger down during Spinning Up and the barrage**, so a fire leg can never hold the
lock against the one dodge that kills outright. Same shape `MimironArcSpreadTrigger` already uses:

```cpp
MimironP3Wx2LaserBarrageTrigger barrage(botAI);
if (barrage.IsActive())
    return false;
```

**c. Cap the hop.** Rapid Burst has no telegraph, so a stand-down cannot protect it - only a short
leg can. The ladder currently asks for `FLAMES_RADIUS + spread + step`, where `spread` is the burning
cluster's own radius and is unbounded. New constant, clamping the requested distance:

```cpp
// A FORCED leg holds the movement lock for its whole duration, and Rapid Burst lands with no
// warning at all. 12 yd is 1.7 s at run speed, so a fire dodge can cost at most that much of a
// burst dodge. A capped hop that still lands in fire is refused by the fan and the next bearing
// tried, so the cap trades reach for lock time rather than for safety.
constexpr float ULDUAR_MIMIRON_FLAMES_MAX_HOP = 12.0f;
```

**d. Report the real outcome.** `MoveTo` is a thin wrapper over `TryMoveTo`, which already returns
`RaidObs::MoveOutcome` (`Issued` / `NotAllowed` / `Duplicate` / `Waiting` / `NoPath`). Call
`TryMoveTo` in the fan, count the non-`Issued` outcomes and put them in the flee note - today
`flames+3 none (fire2)` reads as "the fire is everywhere" when it means "the bot was locked", and
that is what hid this. Bump `char line[144]` in `NoteFleeOutcome`. Also hoist the per-bot gates
(`IsMovingAllowed`, `IsWaitingForLastMove`) out of the fan and check once: when they refuse, all 44
candidates fail identically and the fan is 44 wasted hazard screens over a 50-60 node fire field.

## Docs

Governing-docs rule: invoke `/compact-docs-writer` **up front**, before editing either.

- `docs/raids/ulduar/mimiron.md` - the phase-1 west anchor and stack with the navprobe figures and
  the mesh hole; the disperse split and the Napalm price it accepts; the removal of the staging
  shapes; the corrected expectation that a clean arena only buys the opening of phase 2.
- `docs/engine/pitfalls.md` - the general lesson: `IsWaitingForLastMove` compares priorities with
  `>`, so **equal priority blocks**, and a hazard dodge sharing `MOVEMENT_COMBAT` with a formation
  move is silently dead for the formation leg's duration. Plus: `MoveTo` discards the `MoveOutcome`
  that `TryMoveTo` returns, so any caller logging its own refusals will misattribute them.

## Files

- `src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.h` / `.cpp` - the two spots, the disperse helper, the
  hop cap, the `p1tank` and `p1stack` branches, the staging removals
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp` - dodge priority, hop cap, `TryMoveTo` outcome
  counting, note buffer, the `IsMimironLapSlot` call site
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp` - the barrage stand-down, the disperse helper in
  the phase-1 latch, the `IsMimironLapSlot` call site
- `docs/raids/ulduar/mimiron.md`, `docs/engine/pitfalls.md`

Other sessions are editing Thorim and Iron Assembly in this tree - stage only Mimiron and doc work.

## Verification

Per-TU `-fsyntax-only` on the three changed TUs against the build image's `compile_commands.json`,
then the worldserver Docker rebuild (the only compile path), then one Firefighter pull.

Read in this order, all pass/fail:

1. **The reposition landed.** `mimiron.slot` shows `p1tank` at (2691.6, 2568.5) and `p1stack` at
   (2697.0, 2588.0). Phase-1 raid median distance from the room centre moves from 20.7-21.1 yd to
   roughly 50 yd. No evade - the MK II is still engaged past 60 s of phase 1.
2. **The centre is clean at the handover.** Phase-1 flame damage inside 20 yd of the room centre
   drops from 70 % to near zero.
3. **Ranged still cast.** Phase-1 ranged `%cast` stays at or above 60 % (was 63.5 % / 65.5 %), and
   `reach spell` does not start appearing in the move stream - that would mean the stack outran
   casting range and the `reach spell` / formation deadlock is live.
4. **The stack is a blob, not a pillar.** Ranged nearest-neighbour median lands around 3-5 yd, and
   **Napalm Shell victims per cast stays under about 4** (1.38 / 2.20 today). If it goes to 8+ and
   phase-1 deaths rise, the 3 yd disperse is too tight - that is the tuning knob.
5. **Ranged stop fleeing Shock Blast.** `shock` flee notes from ranged and healers should drop to
   near zero at 20.2 yd; melee keep theirs.
6. **The dodge moves.** `mimiron dodge flames action` `r=wait` refusals fall from 1,249 against 387
   issued to a minority, and `flames+3 ok` rises well above 13 % of ladder calls. The new lock
   counter should be near zero on `none` outcomes - if `none` persists with that counter empty, the
   fire really is everywhere and it is a different problem.
7. **No regression on the hazards that kill outright.** `rapidburst` and barrage dodge `wait`
   refusals do not rise (320 and low respectively in n1). This is the risk the FORCED bump carries.
8. **Handovers still work.** `mimiron.slot` shows nothing during staging, `follow` picks the raid up,
   and handover damage taken does not rise above the 6,687 / 3,470 per second seen with the lap.
9. **Phase 2 opens clean.** Flame damage in the first 30 s of phase 2 falls sharply and the phase-2
   healing deficit narrows from 4,000-5,000 HPS. Heat Wave and Rapid Burst are untouched and will
   still be 70-79 % of phase-2 damage taken.

Normal mode too: it should see only the staging removal, since the west anchor, the stack and the
3 yd disperse are all Firefighter-gated.
