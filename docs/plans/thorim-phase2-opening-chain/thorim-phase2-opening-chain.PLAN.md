# Thorim phase 2: keep ranged out of the opening Chain Lightning

Traces `env/dist/logs/botobs/603_1_thorim_1789135678.ndjson` (pull M, 11 Sep 17:13) and
`603_1_thorim_1789136547.ndjson` (pull N, 17:28), both on `6a06fe6bd` (includes `b97c40f7f`; worldserver
started 16:57). Compared against L `1789129966`, K `1789071729`, J `1789071070`. Read with
`python tools/botobs/postmortem.py <file>` and scratch scripts over `tools/botobs/obstrace.py`.
**Timestamps are ms; "+N s" is time since Thorim is first seen below z 425 (trace phase 2 start).**

## Context

25-man hard mode, 24 in raid (4 heal, 10 ranged, 7 melee, 2 tanks, 1 human DK). MT is Bulwark
(`thorim.p2role = maintank`).

| | L | **M** | **N** |
|---|---|---|---|
| MT dies | +155 s | **+137 s** | **+121 s** |
| boss HP at MT death | 32.4% | **50.6%** | **46.8%** |
| boss HP at +120 s | 46.9% | 55.2% | 47.2% |
| bot deaths before MT death (OT included) | 8 | **14** | **7** |

### What `b97c40f7f` fixed (verified)

| | L | M | N |
|---|---|---|---|
| `thorim.ringarrived` notes | 0 | 70 | 86 |
| movement-guard vetoes on melee | 0 | 104 | 180 |
| melee `set behind` moves/min/bot | 4.7 | 0 | 0 |
| melee moving share | 30% | 16% | 10% |
| A→B→A move reversals (all roles) | 107 | 5 | 10 |
| melee Blizzard damage | 63k | 12k | 0 |
| camp Blizzard dodge, median order length | 30 yd | 2.2 yd | 6.2 yd |

- **Unbalancing Strike:** the debuff now runs its full 15 s and the partner swaps 70-304 ms after the
  strike. The only misses were strikes landing while the partner was dead.
- **Oscillation:** effectively gone. The worst remaining case is one healer, Prayer, bouncing
  between positioning and the generic `flee` (5 reversals).

### Why bots died before the MT (M + N)

| cause | deaths | detail |
|---|---|---|
| First Chain Lightning, +11.7 s | 5 | M: 8-hop chain through the gauntlet ranged, stacked within 1 yd on the balcony drop (Holylight, Totemist). N: tank → melee trailing east of the boss → Prayer on RANGE4 → gauntlet ranged walking across RANGE3 (Agony, Malediction, Nightwarrior 77k). K lost 3 the same way as M |
| Melee ring chain | 3 + human | M +42 s: 8 melee hops (Assasin, Shadow, human Deathsong); N +117 s Angry. Obliteration and Mighty stood between slots on cone offsets and bridged them |
| Walk home through Blizzard | 2 | Power in both pulls. The positioning walk home only tests the destination, and the Blizzard escape got `wait` refusals: equal `MOVEMENT_COMBAT` priority cannot cut off the in-flight walk (`MovementActions.cpp:1037-1049`) |
| Off-tank at the tank spot | 4 | 28-49k Blizzard each, then melee or Unbalancing Strike. Blizzard is 16-21% of tank intake |
| Frostbolt Volley finishing blow | 3 | M Totemist, Elemena, Trueshot, each already low from a Frostbolt or a Chain Lightning hop |
| Lightning Charge | 2 | M Hellflame (walking at 27%), Elemena (rezzed, walking) |
| Chain Lightning, other | 2 | M Smartface (6 ranged converging while walking), Tree (2-hop at 41% after Frost Nova) |

**The wipe itself is the damage race.** Tank intake reaches 190-260k per 20 s by +100 s (Thorim's melee
is 110-150k of that) against 180-210k healed. Deaths only pull the MT's death forward.

### Chain Lightning mechanics (verified, not modelled)

- **Damage:** 64390 is 4625-5375 over 8 targets, with `EffectChainAmplitude` 1.5. Each hop multiplies
  the next (`Spell.cpp:8456-8459`), so the multipliers are 1, 1.5, 2.25, 3.4, 5.1, 7.6, 11.4 and 17.1.
  Hops 6-8 kill; hop 5 kills cloth.
- **Jump:** `spell_jump_distance` sets 64390/62131 to 5 yd, and `IsWithinDist` adds both combat
  reaches, so 8 yd centre to centre. Each hop goes to the nearest target not yet hit
  (`Spell.cpp:2151-2245`).
- **Pets:** they are chain targets too. The trace has no damage rows for pets.
- **Targeting:** a random unit off the threat list (`boss_thorim.cpp:740`). The first cast is 13 s
  after the phase trigger, then every 15 s (529, 742).
- **Timing in the traces:** lands at +11.6-11.8 s in every pull that cast it (J's first cast was
  interrupted by the drag), then about +26.7, +41.8, +56.8 s.
- **Rest of the opening timeline:**
  - Thorim lands at the Middle (2134.68, -263.13), 6.3-6.5 yd from RANGE3/RANGE4. The tanks have him
    on his final spot, about (2114.7, -255.7), by +7-16 s.
  - The first Lightning Charge orb lights at +10.7-11.0 s and the cone fires at +15.5-15.9 s.
  - Sif teleports at +8.3 s and casts Frost Nova at +10.8 s.
  - The first Blizzard bunny appears at +27.7 s. Zones reach x > 2140 at +39.7 s and the north rim at
    about +33 s.

**Chain model.** Replay every snapshot: the nearest-neighbour chain from every possible primary target
(players and pets), 8 hops, 8 yd. Count player hops at index ≥ 6.

| expected lethal hops per cast | M | N | L | K |
|---|---|---|---|---|
| +10..13 s, all | 1.11 | 1.06 | 0.58 | 0.70 |
| +10..13 s, ranged (arena / gauntlet) | 0.02 / 0.40 | 0.26 / 0.13 | 0.08 / 0.23 | 0.13 / 0.20 |
| +10..13 s, with all ranged removed | 0.55 | 0.81 | 0.23 | 0.34 |
| +60..150 s, all (ranged) | 0.45 (0.03) | 0.58 (0.02) | 0.43 (0.02) | 0.46 (0.05) |

Two things follow:
- **The opening is where ranged die, and the camp is not.** Once settled, the camp is chain-safe.
- **The gauntlet stack is half of the opening risk.** Arena ranged alone would miss it.

Taking all ranged out of the opening cuts the first cast's lethal hops by 25-60%. Melee risk stays
flat, because the pets soak the extra hops.

**Where the opening melee pack goes.** Over +0..13.5 s in all five pulls, melee, tanks and melee pets
came within 8 yd of **every** camp spot. Only the north rim and the far east stayed 12+ yd clear.

## Timing the hold rests on

All times below are measured from the bot-side phase 2 start: Thorim below z 429.61, which is when
`ThorimPhase2Active` first returns true. Bots issue their first phase 2 move 0.1-0.2 s after it.

| event, pulls K L M N | time |
|---|---|
| first Chain Lightning cast | +11.4-11.5 s |
| first Chain Lightning lands | +12.0-12.1 s |
| first orb lights | +11.1-11.2 s |
| first Lightning Charge | +15.9-16.1 s |

J's first cast was interrupted by the drag.

Effective `AiPlayerbot.SpellDistance` is 28.5 and `HealDistance` 38.5: there is no `AC_` override and
the live `playerbots.conf` matches. "reach spell" therefore fires past about 36.75 yd from Thorim.

## Changes

### 1. Phase 2 clock

- **State:** add `RaidObs::ObsValue<uint32> phase2StartMs{"thorim.p2start"}` to `ThorimEncounterState`
  (`UldEncounter_Thorim.h`, near `runicSmashSide` at :353).
- **Helper:** `bool ThorimPhase2ElapsedMs(PlayerbotAI*, uint32& out)`, placed after `ThorimPhase2Active`
  (`UldEncounter_Thorim.cpp:2026`).
  - It returns false unless phase 2 is active **and** Thorim `IsInCombat()`. His evade walk home stays
    below the floor line and would otherwise latch a stale start.
  - On the first true it latches `std::max<uint32>(getMSTime(), 1)`.
  - `out` is `GetMSTimeDiffToNow(start)`.
  - `ThorimPhase2Active` stays a pure read. The clock is write-once, which is the kind of latch the
    ring already writes from its trigger.
- **Clearing:**
  - Set it to 0 in the raid-wide half of `ResetThorimEncounterState`, after `state->marksCleared = false;`
    (:2494).
  - Also set it to 0 in the engaged branch of `ThorimEncounterStateIsStale` (:2374-2381) while Thorim is
    still at or above the floor line.

### 2. The opening hold, one-way

- **Helper:** `bool ThorimPhase2OpeningHold(PlayerbotAI*, Player*, Position const& holdSpot)`.
- **New per-bot state:** `RaidObs::ObsGuidSet openingReleased{"thorim.openingreleased"}`.
- **Rules, first match wins:**
  1. No phase 2 clock, or the bot is in `openingReleased` → false.
  2. elapsed < 12.5 s → true. That covers the first Chain Lightning with 0.4 s to spare.
  3. Otherwise release the bot and return false, when any of these holds:
     - elapsed ≥ 25 s;
     - `ThorimCampSpotInLitCone(botAI, holdSpot)`;
     - Thorim is within 8 yd of `ULDUAR_THORIM_PHASE2_TANK_SPOT`. He settles 4.1-6.1 yd off it.
  4. Else true.
- **Why the release latches:** it is monotonic, so a dark orb or a tank swap nudging Thorim can never
  send a released bot back. A second read in the same tick cannot flip either.
- **The cone budget:** a bot whose spot is in the first cone leaves at 12.5 s, which gives it 3.4 s
  before the charge. The worst case is spot 3 under orb 0, about 22 yd to the cone edge.
- **The cap:** 25 s is also the most this can safely hold. Sif's bunny path passes 2.6-7.7 yd from the
  opening spots and its first bunny spawns at about +24 s, so the cap must not be raised and these
  spots must not be reused later in the fight.
- **Constants:** `ULDUAR_THORIM_OPENING_HOLD_MIN_MS = 12500`, `..._MAX_MS = 25000`,
  `ULDUAR_THORIM_OPENING_SETTLED_RADIUS = 8.0f`.

### 3. Arena ranged: opening spots on the north-east rim

- **New constants:** `ULDUAR_THORIM_PHASE2_OPENING1..4_SPOT`, with z as navprobe settles it.
- **Selection rule:** each spot is 12.8-15.6 yd from every opening melee, tank or pet sample in the
  five pulls (+0..13.5 s), 12+ yd from the others, and in cast and heal range. DPS and heals therefore
  continue during the hold.

| spot | x, y, z | clear | boss | both tanks | cones (orb index) |
|---|---|---|---|---|---|
| 1 | 2114, -232, 420.146 | 13.7 | 23.7 | 20.9 | 5, 6 |
| 2 | 2128, -226, 420.146 | 15.6 | 32.5 | 31.7 | 0, 6 |
| 3 | 2140, -232, 419.337 | 14.9 | 34.7 | 35.8 | 0, 1, 6 |
| 4 | 2146, -244, 419.531 | 12.8 | 33.4 | 36.3 | 0, 1 |

- **Slots:** `RaidObs::ObsGuidMap<uint8> openingSlots{"thorim.openingslot"}`, cloned from
  `EnsureRangedSlot`/`RangedSlotOf` (`UldEncounter_Thorim.cpp:241-293`).
  - The present set is `HoldsFormationSlot`, plus `TakesRangedSpot`, plus a squad other than Gauntlet,
    read from `state.squads` as `ThorimArenaRingOrder` does (:1737-1739). No entry counts as Arena.
  - Pick the least-loaded spot, breaking ties by the bot's distance to it.
  - 8 arena ranged means 2 per spot, which is how the camp doubles up anyway. Humans never get an entry.
- **Wiring:** in `TryGetThorimPhase2Spot`, between :2119 and :2121, after `if (!boss) return true;` and
  before `ThorimRangedSpot`: if the bot has an opening slot and `ThorimPhase2OpeningHold` holds, set
  `position` to the opening spot and return true.
  - Callers are the positioning trigger, the positioning action and the `ThorimCampBlizzardEscape`
    preferNear. All three are fine with it.
  - The shelter latch re-derives on its first call after the release, because the orb is new to it.

### 4. Gauntlet ranged: spread hold on the balcony

- **New constants:** `ULDUAR_THORIM_BALCONY_HOLD1..6_SPOT`, all navprobe-settled and 12+ yd apart. The
  platform is floor over x 2121-2153, y -289..-320, and the path from `BALCONY_4` is `PATHFIND_NORMAL`.
  - (2126, -294, 438.247)
  - (2138, -296, 438.247)
  - (2150, -297, 438.247)
  - (2132, -307, 438.243)
  - (2144, -307, 438.243)
  - (2138, -318, 438.222)
- **Slots:** `RaidObs::ObsGuidMap<uint8> balconyHoldSlots{"thorim.balconyhold"}`, same pattern, over
  gauntlet bots that pass `TakesRangedSpot`.
- **Header helper:** `bool ThorimBalconyHoldSpot(PlayerbotAI*, Player*, Position&)`, true while the
  hold applies. It is needed because `TakesRangedSpot` is private to the .cpp.
- **Wiring:** in `ThorimBalconyAdvanceAction::Execute`, between :240 (the step update) and :242.
  - It has to sit before the `atEdge` jump, because hold 2 is 4.9 yd from `JUMP_START`, inside that
    6 yd test.
  - Condition: `bossDown && step >= ULDUAR_THORIM_BALCONY_WAYPOINTS - 1 && ThorimBalconyHoldSpot(...)`.
  - Within 1.5 yd (2D) of the spot, return false. Otherwise issue the waypoint's own
    `MoveTo(..., false, false, false, true, MOVEMENT_COMBAT, true)`.
  - The step gate (`BALCONY_5` reached) matters. Lines straight from `BALCONY_3` pass 7.4-9.2 yd from the
    Paralytic Field bunny at (2134.9, -339.7).
  - When the hold ends the existing flow resumes. The step latch copes: it drops from 6 to 5 on the
    west spots, and the bot walks to `JUMP_START` and jumps.
- **Guard fix:** while held, the action returns false and lower nodes run. At 37-69 yd from Thorim,
  "reach spell" and "reach party member to heal" would then walk the bot down the hallway.
  - In `ThorimBalconyGuardMultiplier` (`UldMultipliers_Thorim.cpp`, between :210 and :212), return 0
    for a `ReachTargetAction` while `ThorimPhase2Active`.
  - Casts are unaffected, because the guard only scores `MovementAction`s.
- **Scope:** melee and tanks keep descending at once, because the off-tank is needed for the swap at
  about +6.5 s.

### 5. Camp: never walk home through a zone, and let the escape pre-empt

- **Hold the walk:** `bool ThorimWalkUnderBlizzard(Player*, Position const& from, Position const& to)`.
  - True when the 2D segment passes within `ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE` of any
    `ThorimBlizzardSpots` point.
  - It sits next to `ThorimSpotUnderBlizzard`, with a local `SegmentDistance2d`. Copy the body of
    `DistToSegment2d` from `UldTriggers_Hodir.cpp:29-48`, but keep a unique name in the anonymous
    namespace: that `static` would clash with a shared declaration.
  - `UldTriggers_Thorim.cpp:173` becomes
    `return !ThorimWalkUnderBlizzard(bot, *bot, spot) || ThorimCampSpotInLitCone(botAI, *bot);`.
  - Starting inside a zone is already the Blizzard trigger's case. The worst wait is one trail
    crossing the path, 10-15 s.
- **Pre-empt:** `ThorimSifBlizzardAction::Execute` (`UldActions_Thorim.cpp:406`) moves at
  `MOVEMENT_FORCED`, as Vezax's puddle clear does (`UldActions_Vezax.cpp:64-70`).
  - `IsWaitingForLastMove` (`MovementActions.cpp:1041`) lets only a strictly higher priority through.
  - The lock lasts at most about 2 s for a 15 yd step, and no other Thorim mover uses FORCED.

### 6. Reset and bookkeeping

- **Per-bot reset:** after :2453-2454, erase the bot from `openingSlots`, `balconyHoldSlots` and
  `openingReleased`.
- **Raid-wide reset:** clear `openingReleased` as well.
- **`ThorimBotHasEncounterState`** (:2409-2415): add the three per-bot containers.
- **`ObsGuidMap`:** read with `find` or `count`, never `operator[]`, which inserts.

### 7. Docs (one `/compact-docs-writer` invocation, up front, covering both)

- **`docs/raids/ulduar/thorim.md`:**
  - The Chain Lightning facts: 8 yd jump, ×1.5 per hop, pets as targets, and the timings above.
  - The opening and balcony holds and why they exist.
  - The fact that no trace yet shows whether Lightning Charge reaches players above z 430.
- **Plan file:** first implementation step is to save this plan as
  `docs/plans/thorim-phase2-opening-chain/thorim-phase2-opening-chain.PLAN.md`.
- **Code comments:** follow `use-conversational-language`, with a fresh invocation.

## Deliberately not doing

- **The damage race.** Fewer deaths buy DPS; nothing here raises it.
- **Tank spot under the Blizzard track.** Blizzard is 16-21% of tank intake. Moving the anchor means
  re-solving the camp and shelter tables. Separate plan.
- **Melee ring slot bridging.** The slots are 90° apart (11.3 yd at r 8), cone offsets reach 57.5° and
  Blizzard slides ±180°, so the ring chains as one. Steady risk is 0.35-0.56 lethal hops per cast.
  Separate plan.
- **Sif Frost Nova flee.**
  - It is the generic 30 yd ray flee. Sif teleports at about +8 s into x 2108-2150, y -238..-284:
    spot 4 is inside that box and spots 1 and 3 are 6 yd out.
  - A Nova there can flee a pair toward the pack just before the first Chain Lightning. The dodge
    rightly wins; a clear-spot search in place of the flee is a follow-up.
- **Balcony vs Lightning Charge.** Orb 2's cone covers all six hold spots and both jump points, and
  orb 3 covers holds 1, 4 and 6. No trace has a player above z 430 inside a cone, so whether it reaches
  up there is unknown. The release rule is the same as the arena's.
- **Frostbolt Volley top-ups.** Healing AI, not this strategy.

## Verification

**Static**, from `modules/mod-playerbots`:
- `python apps/codestyle/codestyle-cpp.py`.
- LF, ASCII, ≤ 120 columns.
- Per-TU `-fsyntax-only` in `acore/ac-wotlk-build:master` with the live tree mounted and all module
  `src` dirs on `-I`. Cover `UldEncounter_Thorim.cpp`, `UldTriggers_Thorim.cpp`,
  `UldActions_Thorim.cpp`, `UldMultipliers_Thorim.cpp` and `UldStrategy.cpp`.

**Navprobe:** use the bot filter, `--nav 0x09` (`pitfalls.md:143-151`).
- `point` on all ten new spots must settle to the z in the tables.
- `path` must return `PATHFIND_NORMAL` for each leg:
  - each phase 1 arena ring slot → each opening spot;
  - each opening spot → each camp spot and shelter;
  - `BALCONY_5` → each balcony hold;
  - each balcony hold → `JUMP_START`.

**In game**, one 25-man hard-mode pull, then re-read the trace:
1. **Notes:** exactly one `thorim.p2start`, and it is not carried into a later pull. Every arena Ranged
   bot has a `thorim.openingslot` (2 per spot), every gauntlet Ranged bot has a `thorim.balconyhold`,
   and every one of them gets a `thorim.openingreleased`.
2. **Opening positions:** at the first Chain Lightning, about 12 s in, no Ranged bot is within 8 yd of
   any melee, tank or pet (M+N lost 5 there). Gauntlet ranged are on the platform, 12 yd apart. Balcony
   bots show "reach spell" vetoes during the hold.
3. **Chain length:** the first cast has at most 3 ranged hops and kills no ranged.
4. **Release:** every hold releases by 25 s, and no bot returns to an opening spot afterwards.
   Lightning Charge hits no bot still walking from an opening or balcony spot. Record any 62466 hit on
   a player above z 430.
5. **Blizzard:** no camp bot takes a Blizzard hit while its last accepted mover is the positioning
   action, and `thorim sif blizzard action` gets no `wait` refusals.
6. **No regression:** the ring latch notes and melee vetoes stay present, swaps stay under ~1 s, and
   there are no melee `set behind` moves.
