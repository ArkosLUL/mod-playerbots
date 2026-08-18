# Thorim guide parity — Runic Smash dodge, Runic Barrier bail, Phase 2 melee spread, Lightning Charge

> **Status: implemented.** The durable mechanic facts now live in `docs/raids/ulduar.md`; this file
> stays as the record of why the design landed where it did. Not yet compiled or run in-game.

## Context

A test run of the Ulduar Thorim playerbot strategy surfaced three failures:

1. Bots don't dodge the Runic Colossus's corridor smash — the left/right hand telegraph.
2. Bots keep meleeing the Runic Colossus through Runic Barrier and die to its damage shield.
3. Phase 2 melee and tanks stand on top of each other, so Chain Lightning arcs through the whole
   melee ball. There is **no melee positioning at all** in phase 2 — only the main tank and
   ranged/healers get a spot.

Scope also includes the Lightning Charge cone, which is unhandled and is the largest remaining
phase-2 hit. Everything else from the encounter (Stormhammer, Rune Detonation, Stomp, arena add kill
order, and the in-combat `SPELL_SMASH` 62339 frontal cone) stays out and is recorded as a known gap.

Existing code is four files with no encounter helper and no dedicated multiplier:
[UldActions_Thorim.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp),
[UldActions_Thorim.h](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Thorim.h),
[UldTriggers_Thorim.cpp](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp),
[UldTriggers_Thorim.h](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.h).

## Verified mechanic facts

Read out of `src/server/scripts/Northrend/Ulduar/Ulduar/boss_thorim.cpp`, the DBC reference CSVs in
`modules/mod-spell-tweaks/data/dbc-reference/`, `acore_world`, and `SpellInfoCorrections.cpp`.
Several differ from published guides — do not re-derive them from the web.

### Runic Colossus (32872), spawned at (2227.5, −396.179, 412.176)

| Spell | Id | Detail |
|---|---|---|
| Runic Smash, left hand | 62057 | **5 s cast**. Lights `Thorim Golem Left Hand Bunny` (33141) at x≈2235 / 2246. |
| Runic Smash, right hand | 62058 | **5 s cast**. Lights `Thorim Golem Right Hand Bunny` (33140) at x≈2210 / 2221. |
| Runic Smash damage | 62465 | 10 yd radius per bunny. Wave starts 1 s after the cast lands, marches y −385 → −257 at 16 yd / 500 ms (~3.5 s). |
| Runic Barrier | 62338 | `MOD_DAMAGE_PERCENT_TAKEN` −51% + `SPELL_AURA_DAMAGE_SHIELD` **2000 arcane per melee swing** (arcane, `misc 68`). Cast t+10 s, 20 s duration, repeats 20 s → **permanently up**. |

`EVENT_RC_RUNIC_SMASH` is scheduled in `Reset()` and **cancelled in `JustEngagedWith`** — the
corridor smash only happens on the approach and stops the moment the Colossus is tanked.

Lane safety is already implicit in the existing waypoints: left lane x≈2237–2242, right lane
x≈2212–2219. Each lane is 2–9 yd from its own hand's bunnies (inside the 10 yd blast) and
15.5–22.9 yd from the other hand's (clear). The two lanes' waypoints are **pairwise index-matched**
by y (left −265.1/−275.8/−294.6/−310.2/−318.7/−329.1 vs right −264.8/−275.9/−295.0/−307.5/−318.2/−328.0,
within 2.7 yd), so "same progress, other lane" is a straight index map.

**The Ancient Rune Giant has no damage shield** — its `Runic Fortification` (62942) is a friendly
buff on its adds. The bail is Colossus-only.

### Thorim phase 2

| Spell | Id | Detail |
|---|---|---|
| Chain Lightning | 62131 | `spell_jump_distance` row sets jump radius to **5.0 yd** (not the 10 yd default). 8 bounces, chain source advances to each new victim (`Spell.cpp:2239-2240`). Random target every 15 s. |
| Lightning Charge | 62466 | `spell_cone` = **75°**, radius **150 yd**, 17343 base nature, **instant, no cast bar**. |
| Lightning Orb Charged | 62186 | Applied to a `Thunder Orb` (33378). `SpellInfoCorrections.cpp:2011` patches amplitude to 5000 ms so it ticks **once, exactly 5 s after the orb lights**, firing 62278 at Thorim, who re-orients to that orb and fires 62466. |
| Lightning Charge buff | 62279 | Permanent, one stack per cast (~10 s): +15% damage and melee haste, **+10% nature damage per stack**. |

So Chain Lightning needs **>5 yd of clear gap between stacks**, and Lightning Charge gives a clean
**5 second warning**: a Thunder Orb carrying aura 62186.

Seven Thunder Orb / pillar pairs ring the arena at r≈42 from `(2134.99, −263.12)`; none on the south
(entrance) side. Farthest orb is ~70 yd from a bot near Thorim, inside the 100 yd sight cap.

### Geometry (navprobe, map 603)

Arena floor is solid to r≈35 from `ULDUAR_THORIM_NEAR_ARENA_CENTER` **except the south wedge**:
`(2134.99, −293.12)` and anything past y≈−288 near x≈2135 settles to −27.7, off the floor. The
existing `ULDUAR_THORIM_PHASE2_TANK_SPOT` `(2134.857, −287.029)` sits ~1 yd from that hole — it
works, but nothing may be placed south of it.

With the MT on the tank spot, Thorim parks at ≈ `(2134.9, −278)`. His combat reach is 6.25 and a
player's 1.5, so melee range ≈ **9.1 yd** centre-to-centre. A ring at **radius 8** is inside that.

An r=9 ring probed at centre `(2134.9, −278, 419.6)` is fully on mesh at all 8 headings, so r=8 is
comfortably inside. The static fallback trio and off-tank spot were each point-probed and settle on
the arena floor (419.48–419.85), not the −27.7 terrain.

### Bot-side constraints found

- `GetFirstAliveUnitByEntry` reads `"possible targets no los"`, capped at
  `AiPlayerbot.SightDistance` = **100.0** ([playerbots.conf.dist:642](modules/mod-playerbots/conf/playerbots.conf.dist#L642)).
  The Colossus is **131 / 121 / 102 yd** from the first three waypoint pairs — out of range — so the
  corridor telegraph needs a wider, targeted lookup.
- `AiFactory.cpp:406-407` gives **every melee DPS the `behind` strategy**, so `SetBehindTargetAction`
  at `ACTION_MOVE + 7` is live for exactly the bots that will hold melee ring slots.
- `ThorimGauntletPositioningTrigger::IsActive` ([UldTriggers_Thorim.cpp:191](modules/mod-playerbots/src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp#L191))
  and `ThorimGauntletPositioningAction::Execute` ([UldActions_Thorim.cpp:180](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp#L180))
  both call `botAI->GetMaster()` and dereference it with **no null check**.
- Module sources are globbed by `CollectSourceFiles` ([AutoCollect.cmake:25](src/cmake/macros/AutoCollect.cmake#L25))
  — new files need no CMake edit, only a configure re-run.

## Design decisions

Settled with the user; recorded so a fresh session doesn't relitigate them.

- **Ranged and both tanks hold position through Lightning Charge and eat it.** Only the melee ring
  rotates. The scaling risk (~27k by stack 6) was raised and accepted; no health-gated escape hatch.
- **Master-led raids are the supported path.** The null master is fixed by early-return, not by
  electing a bot gauntlet leader.
- **Bots cross lanes regardless of where the master stands.**
- **The bail keeps the bot's target** — no `AttackStop()`. Suppression is the multiplier's job, so
  the bot keeps firing ranged and instant abilities from outside melee range.
- **No new config options.** All tunables are `constexpr` in `UldBossHelper.h`.

## Implementation

### 1. New encounter helper — `UldEncounter_Thorim.{h,cpp}`

Under `src/Ai/Raid/Uld/Util/`, following
[UldEncounter_IronAssembly.h](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.h)
exactly: doc block, `struct ThorimEncounterState` +
`thread_local std::unordered_map<uint32 /*instanceId*/, ThorimEncounterState>`, by-entry lookups,
`TryGet…` out-param functions, reset function.

State:

- `std::unordered_map<ObjectGuid, uint8> meleeSlots` — sticky ring assignment. Evicted the way
  `EnsureIronAssemblySpreadSlot` does (`UldEncounter_IronAssembly.cpp:92-133`), including **eviction
  of the dead**. Sticky because re-deriving by guid each tick reshuffles the ring on every death.
- `uint8 runicSmashSide` + `uint32 runicSmashSeenMs` — latched hand.
- `ObjectGuid chargedOrbGuid` + `uint32 orbScanMs` — cached Lightning Charge orb.
- `std::unordered_set<ObjectGuid> barrierBailing` — hysteresis latch for the bail.
- `std::unordered_map<ObjectGuid, bool> ringArrived` — reach-then-hold latch (see §4).

Both grid scans are **raid-wide answers, computed once per instance per ~500 ms** and stamped, the
way `XT002BurstWindowMultiplier` memoises. Twenty-five bots must not each sweep the grid for the same
answer, and sharing it guarantees they cannot disagree about which lane is hot.

Functions:

- `Unit* GetThorim(PlayerbotAI*)` — `GetFirstAliveUnitByEntry` (`RaidBossHelpers.h:29`), **not**
  `AI_VALUE2(Unit*, "find target", …)`: that value walks only the bot's own threat list and matches
  on localized name, so a bot on an add goes blind to the boss (`UldEncounter_Vezax.h:57-59`).
- `Unit* GetThorimRunicColossus(PlayerbotAI*)` — **`FindNearestCreature(32872, 150.0f)`**, because
  the sight-capped value lookup cannot see it from the top of the corridor.
- `bool ThorimRunicSmashSafeLane(PlayerbotAI*, bool& useLeftLane)` — while the Colossus is alive and
  **not in combat**, reads `FindCurrentSpellBySpellId(62057/62058)`, latches the hand with
  `getMSTime()`, and reports the **opposite** lane. Latch valid
  `ULDUAR_THORIM_RUNIC_SMASH_LATCH_MS`; a new cast overwrites. False when nothing is latched.
- `bool ThorimShouldBailFromBarrier(PlayerbotAI*, Player*)` — Colossus alive, `HasAura(62338)`, bot
  is melee and not a tank, health hysteresis latched in `barrierBailing`.
- `bool TryGetThorimMeleeSpot(PlayerbotAI*, Player*, Position&)` — the ring (§4).
- `Unit* ThorimChargedThunderOrb(PlayerbotAI*)` — cached scan for 33378 with aura 62186.
- `bool ThorimLightningChargeRingOffset(PlayerbotAI*, float& rotationRadians)` — the rigid rotation
  (§5). Returns false when no rotation is needed.
- `bool ThorimBotHasEncounterState(Player*)`, `void ResetThorimEncounterState(Player*, bool clearInstance)`.

### 2. Constants — `UldBossHelper.h` / `UldBossHelper.cpp`

Ids into the `UlduarIDs` enum beside the existing `SPELL_UNBALANCING_STRIKE`:

```
SPELL_THORIM_RUNIC_SMASH_LEFT     = 62057
SPELL_THORIM_RUNIC_SMASH_RIGHT    = 62058
SPELL_THORIM_RUNIC_BARRIER        = 62338
SPELL_THORIM_LIGHTNING_ORB_VISUAL = 62186
NPC_THORIM_THUNDER_ORB            = 33378
```

Tunables near `UldBossHelper.h:1326-1332`, each with its derivation in a one-line comment:

```
ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE      = 150.0f  // sight cap is 100; colossus is 131 yd from the first waypoint
ULDUAR_THORIM_RUNIC_SMASH_LATCH_MS       = 10000   // 5s cast + 1s delay + 3.5s wave
ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS = 500
ULDUAR_THORIM_BARRIER_BAIL_HEALTH_PCT    = 55.0f
ULDUAR_THORIM_BARRIER_RESUME_HEALTH_PCT  = 80.0f
ULDUAR_THORIM_BARRIER_BAIL_DISTANCE      = 14.0f   // clear of melee range (9.1 yd)
ULDUAR_THORIM_MELEE_RING_RADIUS          = 8.0f    // inside 9.1 yd melee range; slots 11.3 yd apart
ULDUAR_THORIM_MELEE_SLOTS                = 3
ULDUAR_THORIM_OFFTANK_BEARING_OFFSET     = 20° in radians
ULDUAR_THORIM_RING_ARRIVE_TOLERANCE      = 3.0f
ULDUAR_THORIM_RING_REPOSITION_TOLERANCE  = 5.0f
ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE = 75° in radians  // spell_cone row for 62466
ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN     = 15° in radians
ULDUAR_THORIM_LIGHTNING_CHARGE_RANGE      = 150.0f
```

Static fallback positions, **all navprobe-verified on mesh**:

```
ULDUAR_THORIM_PHASE2_MELEE1_SPOT  = (2142.9, -278.0, 419.64)
ULDUAR_THORIM_PHASE2_MELEE2_SPOT  = (2134.9, -270.0, 419.85)
ULDUAR_THORIM_PHASE2_MELEE3_SPOT  = (2126.9, -278.0, 419.64)
ULDUAR_THORIM_PHASE2_OFFTANK_SPOT = (2137.9, -287.0, 419.48)
```

Pairwise separation 11.3–18.0 yd, and 12.1–17.0 yd from the tank spot — every gap clears the 5 yd
Chain Lightning jump. Nothing sits south of the tank spot.

### 3. Runic Smash lane dodge (issue 1)

**Refactor first**: `ThorimGauntletPositioningAction::Execute` has the same six-waypoint
"pick nearest, `MoveTo` it" block pasted twice (~130 lines,
[UldActions_Thorim.cpp:203-329](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp#L203)).
Factor it into one helper taking a `Position const*` array **plus a per-slot movement priority**, and
reuse it from both lanes and from the dodge. Preserve the existing quirk exactly — five slots use
`MOVEMENT_NORMAL, lessDelay = true`, and `6_YARDS_2` alone uses `MOVEMENT_COMBAT` without it, in both
lanes. Behaviour-neutral refactor, so any gauntlet regression is attributable to the dodge.

New `thorim runic smash trigger` / `thorim runic smash action`:

- Active when `ThorimRunicSmashSafeLane` reports a lane **and** the bot is currently matched to the
  other one.
- Lane matching: resolve the **master** to a lane the same way the gauntlet action does; when the
  master matches none (walking the centre line, fighting an add), fall back to the **bot's own**
  nearest lane. Do not fire while the squad is still in follow-master approach mode near the
  entrance.
- Target waypoint: the safe lane's entry at the **same index** as the matched waypoint, so the squad
  keeps its progress down the corridor.
- Hold the safe lane until a new telegraph says otherwise — do not snap back to the master's lane
  when the latch expires.
- `MOVEMENT_COMBAT`.

### 4. Runic Barrier bail (issue 2)

`thorim runic barrier bail trigger` / `… action`, plus `ThorimRunicBarrierMultiplier`.

- Applies to **non-tank melee only**. Bail below `BAIL_HEALTH_PCT`, resume above
  `RESUME_HEALTH_PCT`, latched in `barrierBailing`.
- Action: **no `AttackStop()`** — keep the target so ranged and instant abilities keep firing.
  `MoveAway(colossus, ULDUAR_THORIM_BARRIER_BAIL_DISTANCE - currentDistance)`. Do **not** use
  `FleePosition`: it clamps travel to `AiPlayerbot.FleeDistance` (default 5.0) and would leave the
  bot inside the shield's reach.
- Multiplier, gated on `AI_VALUE(Unit*, "current target") == colossus` so a target switch releases it
  for free: zero `MeleeAction*` and `ReachTargetAction*`; leave `CastSpellAction*`, healing and
  `AvoidAoeAction` alone. This is the whole point — a blanket damage stop throws away DPS the
  damage shield was never going to punish, and the gauntlet is on a 2:45 hard-mode timer.
- Stays on in hard mode.

### 5. Phase 2 melee ring (issue 3)

Extend `ThorimPhase2PositioningTrigger` / `…Action`, and add `ThorimMovementGuardMultiplier`.

Slot assignment:

- Main tank → `PHASE2_TANK_SPOT` (unchanged).
- Assist tank index 0 → the ring at the **MT bearing + 20°**, so it stays in taunt range for the
  Unbalancing Strike swap.
- Ranged and healers → the existing three spots, unchanged, and their existing `> 1.0f` arrival check
  is left alone.
- Everyone else, **including a third tank** → a sticky melee ring slot.

Ring geometry: bearings at `MT_bearing + 90° / 180° / 270°`, radius 8, centred on Thorim's **live**
position. Anchoring on the MT bearing stops the ring rotating as the boss moves — the same trick
`GetHodirRingSlot` uses (`UldBossHelper.cpp:669-725`). **When there is no living MT, use the bearing
from Thorim toward the static `PHASE2_TANK_SPOT`**, so the ring doesn't spin the instant a tank dies.

Three-level fallback: dynamic ring point → static trio, but only if that spot is actually within
melee range of Thorim → otherwise return false and let the generic chase run. A melee bot parked
25 yd from the boss at zero DPS is worse than an unspread one.

**Reach-then-hold with hysteresis**, not the existing 1.0 yd check: arrive at
`RING_ARRIVE_TOLERANCE` (3 yd), latch `ringArrived`, `StopMoving()`, and issue no further move until
the bot drifts past `RING_REPOSITION_TOLERANCE` (5 yd). Return true while actually moving so the
action owns the tick. A 1 yd deadband against a ring recomputed from a moving boss is the Sapphiron
air-phase failure recorded in [pitfalls.md](modules/mod-playerbots/docs/engine/pitfalls.md) — the bot
slides in place and never casts, because a moving bot cannot cast at all.

`ThorimMovementGuardMultiplier`, shaped like `IronAssemblyMovementGuardMultiplier`
([UldMultipliers.cpp:240-269](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L240)):
role gate → `dynamic_cast<MovementAction*>` → **exempt `AttackAction*` and `ReachTargetAction*`** →
whitelist the encounter's own movers by name → gate on "phase 2 and this bot holds a ring slot"
**last**. Without it, `SetBehindTargetAction` walks every melee round behind Thorim the moment the
ring action yields, collapsing the three stacks into one arc. The trailing window gate is what stops
the guard becoming the Void Reaver permanent-freeze failure (`UldMultipliers.h:173-181`).

Also fix the trigger/action predicate mismatch: the trigger counts ranged with `IsRanged` only while
the action counts `IsRanged || IsHeal`, so they can disagree about a healer's slot. One predicate in
the helper. Note `PlayerbotAI::IsRanged()` returns true for healers (`UldMultipliers.h:160-162`).

### 6. Lightning Charge dodge

`thorim lightning charge trigger` / `… action`, **melee ring slot-holders only**.

- Detect: a Thunder Orb (33378) with aura 62186 (cached scan). Bearing Thorim→orb defines the wedge;
  half-width is `CONE_ANGLE / 2 + MARGIN` = 52.5°.
- **Rigid ring rotation**: rotate the whole set of melee slots by the smallest angle that clears
  every occupied slot, rather than letting each bot take its own shortest path. Independent rotation
  swings slots on opposite edges toward each other and can put them inside 5 yd — which trades a
  Lightning Charge death for a Chain Lightning one. The three slots span 180°, leaving a 180° gap on
  the tank side that always accommodates the 105° blocked arc, so a valid rotation always exists.
- MT, off-tank, ranged and healers **hold and eat it**. Moving the tank drags the boss, breaks the
  taunt swap, and re-anchors the whole ring every tick.
- Same three-level fallback if the rotated destination fails validation.

### 7. Reset node

`thorim encounter reset trigger` / `… action`, Iron Assembly shape. Detector: **Thorim alive, at full
health, and not in combat**. Full health alone is a trap — he sits at full health on the balcony for
the entire gauntlet; `JustEngagedWith` fires at the pull, so combat state is what separates the two.
Guard the trigger with `ThorimBotHasEncounterState` so it doesn't swallow ticks pre-pull. Evict
per-guid entries for members who left the instance.

### 8. Small fixes in files already being touched

- Null-guard `botAI->GetMaster()` in both the gauntlet trigger and action (early-return).
- Swap the initialisers at [UldActions_Thorim.cpp:109-110](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp#L109):
  `ironHonorGuard` is assigned `"iron ring guard"` and `ironRingGuard` is assigned
  `"iron honor guard"`. Fix the names, leave the branch order alone.

### 9. Wiring

Four string-keyed sites, all silent at runtime if misspelled — `pitfalls.md` opens on exactly this
failure, with `"thorim fall from floor action"` as a past Thorim casualty. Check every new name
against its `creators[...]` entry before building.

- `UldActionContext.h` — creators map + factory fn, in the later Thorim block (`:159-161`, `:316-318`).
- `UldTriggerContext.h` — same (`:159-161`, `:317-319`).
- `UldStrategy.cpp::InitTriggers`, after the existing Thorim block (`:424-469`):

  | Node | Relevance | Reason |
  |---|---|---|
  | `thorim lightning charge trigger` | `ACTION_RAID + 4` | ~17k instant and growing; must outrank the two Sif nodes already at `+3` |
  | `thorim runic smash trigger` | `ACTION_RAID + 3` | corridor-only, cannot coexist with Sif |
  | `thorim runic barrier bail trigger` | `ACTION_RAID + 2` | below the dodges, above positioning |
  | `thorim encounter reset trigger` | `ACTION_RAID` | |

- `UldStrategy.cpp::InitMultipliers` (`:758-817`) — `ThorimRunicBarrierMultiplier` and
  `ThorimMovementGuardMultiplier`.
- No CMake edit; sources are globbed. A configure re-run is still needed to pick up new files.

### 10. Docs

- `docs/raids/ulduar.md` — rewrite the Thorim section (`:291-300`) with the mechanic tables above and
  record the remaining known gaps (Stormhammer, Rune Detonation, Stomp, arena add kill order,
  `SPELL_SMASH` 62339). Correct the stale rows at `:1604`, `:1610` and `:1615` that still call the
  Unbalancing Strike swap cheat-only — that stopped being true in `dee4b0c3e`.
- `docs/engine/pitfalls.md` is **not** touched. If the Chain Lightning 5 yd `spell_jump_distance`
  override is later judged worth recording there, that edit needs `/compact-docs-writer` first.

## Verification

Static, before the build:

1. Grep every new `TriggerNode` / `NextAction` string against `creators[...]` in both context files.
2. Confirm the two new multipliers exempt `AttackAction*` / `ReachTargetAction*` and are window-gated
   last.
3. Re-probe any position that changes:
   `MSYS_NO_PATHCONV=1 docker run --rm -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master --map 603 point X Y Z`
   Read `UpdateAllowedPositionZ`, not the trailing "N/N on mesh" line.

The module cannot be compiled headless here, so build and in-game run are a hand-off. Worth watching:

4. **Corridor**: on a Runic Smash telegraph the squad crosses to the opposite lane at the same
   progress index, and holds it until an opposite-hand cast. Check bots at the *top* of the corridor
   dodge too — that's what the 150 yd lookup is for.
5. **Colossus in combat**: melee health oscillates around the 55/80 band, bots step out to ~14 yd and
   keep casting instants, nobody dies to the damage shield. Confirm the gauntlet still clears inside
   2:45 with `AiPlayerbot.UlduarThorimHardMode = 1` ([playerbots.conf.dist:398-405](modules/mod-playerbots/conf/playerbots.conf.dist#L398)).
6. **Phase 2**: three visible melee clumps around Thorim with both tanks south. Chain Lightning
   should hit one clump and not carry into another — watch the bounce count in the combat log.
   Confirm melee do **not** drift behind the boss once settled (that's the movement guard working).
7. **Lightning Charge**: when a Thunder Orb lights, the melee ring rotates as a unit within the 5 s
   window and re-forms after; ranged and tanks stay put by design.
8. Run **both 10-man and 25-man** — the phase-2 slot paths differ by roster size.

## Out of scope

`SPELL_SMASH` 62339 (in-combat 60° cone), Stormhammer 62042, Rune Detonation 62526, Stomp 62411,
Runic Fortification 62942, and arena add kill priority. Documented, not implemented.
