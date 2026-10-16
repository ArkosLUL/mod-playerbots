# refine-beasts — Northrend Beasts: worm-stage oscillation and hazards, Arctic Breath spread

Wave 4c. It runs after `w7-direct-targeting` merges and branches from that merge. Read what w7
changed in the Beasts stems first: DPS now pick their beast through a resolver, and no raid icons
remain. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). This
is a refine lane, so its findings come from a trace; line numbers below are as of `8f0f5975d`.

Trace (25N, one kill at 6:33, a human tank "Dragon", a build carrying waves 3 and 4):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_gormok-the-impaler_1792096133.ndjson`.
Read it with:

- `postmortem.py <f> --probes move.by --during nb.stage=2` (worm stage 1:48-4:29);
- `postmortem.py <f> --death 0`;
- `bosses/northrend_beasts.py <f> --worms --cure --breath`;
- `postmortem.py <f> --probes nb.arc`.

## Findings to fix

**Worm-stage oscillation.** The action owning each move flipped A-B-A 713 times in 2.7 min, against
about 130 on Gormok and 40 on Icehowl. Pairs: `northrend worms reposition` against `reach melee`
(251), `reach spell` (244) and `follow` (155).

- *No held spot.* The reposition latches only its walk. On arrival it notes `clear`, erases the latch
  and returns false (`ToCActions_Jormungars.cpp:263-288,343-345`). `northrend worms move guard`
  needs the bot to be moving (`ToCMultipliers_Jormungars.cpp:35-58`,
  `ToCHelpers_Jormungars.cpp:1106-1120`). So generic movers win the next tick, and the next trigger
  re-plans from where the bot stands.
- *The spread rule.*
  - Casters space from all 24 members, the melee stack and tanks included, on a 9/11 yd band.
  - The ring search runs outward from the bot (`EncounterHelpers.cpp:489-559`) and ignores others'
    destinations.
  - It stays on with both worms under ground (`ToCHelpers_Jormungars.cpp:988-1002`).
- *Reach dropped.* Fallback tiers drop the role range limit (`ToCActions_Jormungars.cpp:305-320`).
  The ranged limit is a raw 28.5 yd against `reach spell`'s 37 yd trigger (`RangeTriggers.cpp:155-169`).
- *Melee flanks.* Melee land on the worm's front flanks, so `set behind` (`MovementActions.cpp:2801-2858`)
  moves them.
- *Submerge.* Bots lose their target and fall to the non-combat engine. Its `follow` fights the
  spread every 0.3-1 s.

**Hazards.**

- *No path check.* No move checks its path. `EncounterHelpers.cpp:487` says so, and
  `WormSpotStillSafe` (`ToCHelpers_Jormungars.cpp:1061-1072`) checks only the endpoint.
- *The one death.*
  - Stormweaver, a Bile carrier, walked 28 yd at 3:23.7 on a line 14 yd from the stationary worm.
  - The Sweep knockback threw it into a growing Slime Pool.
  - Its next spot ran the line through the pool's centre, and it died "holding" that walk at 3:30.
- *Bile spread.*
  - At 3:21.8 a Burning Spray (10 yd splash) gave Bile to 10 stacked melee.
  - The urgent escape drops the "keep off clean bots" circles, and 5 carriers chose one spot within
    90 ms.
  - 46 pulses (~110k) hit bots carrying neither debuff.
- *Toxin.*
  - 3 melee kept Toxin 60 s.
  - `DirectCureSpot` (`ToCActions_Jormungars.cpp:87-95`) tries one bearing, refused because it sat
    in a pool, with no fallback.
  - Stuck carriers wait for a runner, and only bots run.
  - Bile pulse 66870/67621/67622/67623 hits every ally within 10 yd of the carrier and removes
    Toxin. Toxin slows to a full stop and can't be dispelled.
- *Pools.* Generic `reach spell` walks casters into pools. A cast in progress counts as "locked" while
  the pool grows onto the bot.

**Arctic Breath.** The breath at 5:45.6 froze 8 melee, a healer and a ranged.

- Every melee bot held bearing slot −3.
- `nb.arc` flickered between 0 and N every ~100 ms beforehand. `IsBreathSpreadUp`
  (`ToCHelpers_Icehowl.cpp:499-515`) drops the spread whenever Icehowl's victim steps out of his
  melee range.
- Only 4-11 bearings survived the floor probe.

**Readers.**

- `--breath` finds no breath on 25N: 67650 reaches the trace only as aura and damage records, never
  as `cast`.
- `--cure` (`northrend_beasts.py:889`) attributes Bile pulses to their source, which is always
  Dreadscale.

## Scope

**Jormungars:**

1. *Held spot per bot* in `JormungarsState`.
   - It holds while the spot passes every rule at trigger distance and stays inside generic reach of
     the current target.
   - It releases when a rule breaks at the spot itself, the target or phase changes, or the anchor
     worm drifts past a deadband.
   - The action returns true while walking and false once parked.
   - The latch clears where the trigger turns off.
   - Template: Icehowl's dealt, clamped stand (`ToCHelpers_Icehowl.cpp:594-632`, thresholds
     `ToCHelpers_Icehowl.h:39-76`).
2. *Guard while the spot holds.*
   - Zero: reach, `set behind`, combat formation, follow, charge/blink/disengage.
   - Keep: attack, reach-to-heal, `avoid aoe`. Zeroing reach strands a bot whose spot is out of
     reach (`pitfalls.md`).
   - Template: the Yogg-Saron portal-hold `Block` rule (`UldDefinition_YoggSaron.cpp:213-217`). If
     w7 added a Beasts `EncounterDefinition`, declare it there.
3. *Path test.* `accept`, the escape search and `WormSpotStillSafe` reject a straight walk that
   crosses either hazard:
   - a pool, within its radius plus its growth over the walk time (radius 2 + 0.3 × tick);
   - an up stationary worm's Sweep, within 15 yd plus a margin.
   From inside a pool, run the escape search first, and allow only an exit moving away from the
   centre.
4. *Destinations count.* Other bots' latched destinations (`state.walks`) count as obstacles. A start
   angle by guid breaks ties.
5. *Bile carriers.* Each carrier gets a deterministic slot by guid, at least 10.5 yd from other
   carriers and clean bots. The escape keeps the clean-bot circles.
6. *Caster spread dealt per instance.*
   - Slots are sticky, re-dealt on emerge or when the anchor drifts.
   - The melee stack counts as one circle.
   - The band is trigger ≥ 10 yd, clear ~13.
   - Off while the stationary worm is under ground.
   - Role limits are in combat-reach terms. Relax spread before reach, and never drop reach for a
     non-urgent move.
   - Note the tier that picked the spot in `nb.wormmove`.
7. *Melee.*
   - Dealt spots on the mobile worm's rear arc, at least 108° off its facing.
   - Spread as widely as melee reach allows. Measure whether two groups 10 yd apart fit.
   - Clamped, and re-clamped only past a deadband.
8. *Submerge.* Zero follow for every bot while a worm is engaged (Malygos and Yogg-Saron precedent).
9. *Toxin cure.*
   - `DirectCureSpot` tries 16 bearings round the Bile carrier, or walks part of the way past 30 yd.
   - A stuck carrier with no runner keeps walking toward the Bile carrier, a human one included.
10. *Pools against generic movers.*
    - A guard zeroes `reach spell` and `reach melee` while their line crosses a pool.
    - "The pool reaches me before this cast ends" counts as urgent.

**Icehowl:**

11. `IsBreathSpreadUp` gets hysteresis: it stays up until the victim has been out of reach for a short
    window.
12. Melee are dealt across all rear bearings.
13. Find out why so few bearings survive the floor probe on 25N.

**Readers and tests:**

14. `--breath` falls back to grouping Arctic Breath aura applications by Icehowl.
15. `--cure` attributes each pulse to the carrier within 10 yd of its victim.

**Priority under the oversized-lane rule:** 1-5 first, then 11-12, then 14-15, then 6-10 and 13.
Write what doesn't fit into `refine-beasts-rest.PLAN.md` and list it in the report.

## Owns

- the `Jormungars`, `Icehowl` and `NorthrendBeasts` stems;
- `northrend-beasts.md`;
- `northrend_beasts.py` and its test.

`src/Util/EncounterHelpers.{h,cpp}` is open for additive helpers only, such as a segment-against-
circle test, listed in the report. Algalon's `DropForeignBooking` lives in Ulduar, which stays
untouched: write a ToC or shared helper instead.

## Task list

Read `docs/raids/trial-of-the-crusader/northrend-beasts.md`, "Acidmaw & Dreadscale" and "Icehowl",
first: mechanics, ids and what the bots do today. Paths are under `src/Ai/Raid/ToC/` unless they start
with `src/Util/`, `docs/` or `tools/`. No new files, so no CMake re-run, and no registration seam: the
rules go in the existing Beasts definition, which `ToCStrategy::InitMultipliers` already wires after
the stem multipliers. Probe keys stay string literals on the line of the call. No coordinates: every
spot derives from a unit and is clipped live, since the arena floor is a GO navprobe can't see.

### Contracts

Group A writes these in `Util/ToCHelpers_Jormungars.h` (namespace `TrialOfTheCrusaderHelpers`), group
B codes against them, the integrate step compiles them together. `SetWormWalk`, `ClearWormWalk` and
`GetWormWalk` go; `IsWormWalkInFlight`, `GetMobileWormDrag`, `GetWormRedirectTarget` and
`IsWormBileCarrierHeldOut` keep their signatures.

```cpp
constexpr float WORM_SPREAD_TRIGGER = 10.0f;    // was 9
constexpr float WORM_SPREAD_CLEARANCE = 13.0f;  // was 11
// Casters keep off the melee round the up mobile worm as one circle on it
constexpr float WORM_MELEE_STACK_TRIGGER = WORM_MELEE_RANGE + WORM_SPREAD_TRIGGER;
constexpr float WORM_MELEE_STACK_CLEARANCE = WORM_MELEE_RANGE + WORM_SPREAD_CLEARANCE;
// Dealt melee stands on the mobile worm
constexpr float WORM_MELEE_STAND = WORM_MELEE_RANGE - 1.0f;  // from its centre
constexpr float WORM_MELEE_GROUP_DEG = 54.0f;                // either side of its back: groups 13.4 yd apart
constexpr float WORM_MELEE_REAR_ARC_DEG = 108.0f;            // least angle off its facing for a melee spot
// Held spots
constexpr float WORM_HOLD_ARRIVE = 1.0f;
constexpr float WORM_HOLD_TRIGGER = 2.5f;           // a parked bot pushed past this walks back
constexpr float WORM_HOLD_MELEE_DRIFT = 2.5f;       // anchor worm moved, melee hold
constexpr float WORM_HOLD_MELEE_TURN_DEG = 20.0f;   // anchor worm turned, melee hold
constexpr float WORM_HOLD_DRIFT = 6.0f;             // anchor worm moved, any other hold
constexpr uint32 WORM_HOLD_FRESH_MS = 1000;         // a hold the plan hasn't confirmed this long guards nothing
constexpr uint32 WORM_SEARCH_BACKOFF_MS = 1000;     // after a non-urgent search found no spot
// Path test
constexpr float WORM_POOL_PATH_PAD = 1.0f;
constexpr float WORM_SWEEP_PATH_RADIUS = WORM_SWEEP_TRIGGER;
// Search
constexpr float WORM_SEARCH_RADIUS = 30.0f;
constexpr float WORM_PREFER_JITTER = 0.5f;
constexpr uint32 WORM_CURE_DIRECT_BEARINGS = 16;
constexpr float WORM_CURE_PARTIAL_STEP = 25.0f;

enum class WormMoveReason : uint8 { None, Cure, Run, Pool, Bile, Sweep, Spew, Spread, Melee, Reach };
// The search that found a spot. Escape, loose, partial and cross spots are walked to, never parked on.
enum class WormSpotTier : uint8 { Stand, Full, NoSpread, Partner, Direct, Partial, Escape, Loose, Cross };
char const* WormSpotTierName(WormSpotTier tier);  // "stand", "full", "nospread", "partner", ...
bool WormTierParks(WormSpotTier tier);            // not escape, loose, partial or cross
bool IsWormCureReason(WormMoveReason reason);     // cure or run

struct WormHoldView
{
    Position spot;
    WormMoveReason reason = WormMoveReason::None;
    WormSpotTier tier = WormSpotTier::Full;
    uint32 issuedMs = 0;
    bool parked = false;
};

struct WormMovePlan
{
    WormMoveReason reason = WormMoveReason::None;
    bool urgent = false;
    bool held = false;          // a valid hold: walk back to hold.spot, never search
    WormHoldView hold;
    bool escapeFirst = false;   // inside a pool: the escape tier goes first
    bool hasStand = false;      // Melee: the dealt stand
    Position stand;
    Position preferNear;
    ObjectGuid partner;         // Cure: the Bile carrier; Run: the Toxin carrier
    Position partnerPos;
    std::vector<EncounterHelpers::HazardCircle> circles;        // hard: pools, Bile, Sweep, stomp, own feet
    std::vector<EncounterHelpers::HazardCircle> spreadCircles;  // casters' spread and the melee stack
    std::vector<EncounterHelpers::HazardCircle> urgentCircles;  // pools, Bile, Sweep while cast
    // urgentCircles, a run's other Bile carriers, a held-out carrier's clean bots
    std::vector<EncounterHelpers::HazardCircle> escapeCircles;
    std::vector<EncounterHelpers::HazardCircle> looseCircles;   // escapeCircles less those clean bots
    std::function<bool(float, float)> accept;        // floor, Spew cones, path, partner reach, melee rear arc
    std::function<bool(float, float)> escapeAccept;  // floor, Spew cones, path
    std::function<bool(float, float)> crossAccept;   // floor, Spew cones
    std::function<bool(float, float)> roleAccept;    // the role's reach, in combat-reach terms
};
// Trigger and action both read it. Notes nb.cure and nb.wormhold; parks and releases holds.
bool GetWormMovePlan(PlayerbotAI* botAI, WormMovePlan& plan);
// urgentCircles, then escapeAccept while urgent, else accept
bool WormSpotStillSafe(WormMovePlan const& plan, Position const& spot);

// The spot the action just walked the bot to, parked when it already stands there. Notes `set`.
void SetWormHold(PlayerbotAI* botAI, Position const& spot, WormMoveReason reason, WormSpotTier tier, bool parked);
bool IsWormSpotHeld(PlayerbotAI* botAI);      // a hold the plan confirmed within WORM_HOLD_FRESH_MS
bool IsWormWalkInFlight(PlayerbotAI* botAI);  // unparked hold, under WORM_WALK_LATCH_MS, moving, booked on it
void BackOffWormSearch(PlayerbotAI* botAI);   // the plan stays quiet on non-urgent reasons for the backoff
bool WormReachCrossesPool(PlayerbotAI* botAI);  // reach melee or reach spell would walk this bot through a pool
```

`src/Util/EncounterHelpers.h` gains, additively, `bool SegmentCrossesCircle(Position const& from,
Position const& to, HazardCircle const& circle)`: the 2D segment passes strictly inside the circle. The
name clashes with none of the file-local `DistanceToSegment2d` copies in files that `using namespace
EncounterHelpers`.

| Key | Writer | Values |
|---|---|---|
| `nb.wormmove` | B | as now, but a walk is `move <yd> <reason> <tier>`; reasons gain `melee` and `reach` |
| `nb.wormhold` | A | per bot, change-only: `set <reason> <tier>` (not on a walk back), `park`, `unpark`, `release <cause>`; cause a reason name, or `path`, `target`, `phase`, `drift`, `heal`, `arrived`, `gone` |
| `nb.arc` | C | bearings kept, melee-only ones included |

### Group A — worm state, plan, held spots

Files: `Util/ToCHelpers_Jormungars.h`, `Util/ToCHelpers_Jormungars.cpp`, `src/Util/EncounterHelpers.h`,
`src/Util/EncounterHelpers.cpp`.

1. **Contracts** above and `SegmentCrossesCircle`. `static_assert` every trigger under its clearance
   and `WORM_HOLD_ARRIVE < WORM_HOLD_TRIGGER`.
2. **Holds** replace `walks` in `JormungarsState`: per bot a `WormHoldView` plus, at set time, the
   bot's current-target guid, the `state.worm` value, and the anchor worm's guid, position and facing
   (anchor = the up mobile worm, else the stationary one), and `checkedMs`. `Disengage` clears them;
   `RefreshRoster` drops those of bots no longer in the roster.
3. **Rules as one table**, built per bot once per plan and testable at any point, so the bot's own
   spot (which reason fires) and a held spot (validity) run the same tests. A rule centred on another
   member sits on that member's held spot when it has one, else on its position. In reason order, each
   a trigger and a clearance:
   - Pool, Bile, Sweep, Spew and the casters' stomp circle in a heroic overlap: unchanged, Bile
     also gaining task 7.
   - Spread (`spreadCircles`): ranged and healers, while both worms live and the stationary worm is up
     (`IsUp`, not its duty). Circles on other ranged and healers only, never melee or tanks, cure
     partners still exempt, at `WORM_SPREAD_TRIGGER` / `WORM_SPREAD_CLEARANCE`; plus one on the up
     mobile worm at `WORM_MELEE_STACK_TRIGGER` / `WORM_MELEE_STACK_CLEARANCE`.
   - Melee (8), then Reach (10).
4. **Validity**, in `GetWormMovePlan` after the cure pairing and the Icehowl-charge early return
   (which leaves holds untouched):
   - A bot owing a cure walk or a run releases a hold of another reason (cause `cure`).
   - Walking (unparked): `WormSpotStillSafe(plan, spot)`, the only test for an escape, loose or
     partial hold.
   - Parked: no rule's trigger fails at the spot, but a nospread hold skips spread and the stack,
     and partner and direct holds skip spread, melee and reach and keep `accept`'s partner reach;
     `roleAccept` holds at the spot, except for partner and direct.
   - Always: current target unchanged (not for healers); `state.worm` unchanged (emerge, submerge,
     first death); anchor moved at most `WORM_HOLD_DRIFT`, for a melee hold at most
     `WORM_HOLD_MELEE_DRIFT` and turned at most `WORM_HOLD_MELEE_TURN_DEG`; a healer's `party member
     to heal` within heal reach of the spot (task 9's reach).
   - Valid: stamp `checkedMs`, fill `plan.held` and `plan.hold`. Parked within `WORM_HOLD_TRIGGER`:
     reason None. Unparked within `WORM_HOLD_ARRIVE`: park (`park`), reason None; an escape, loose or
     partial hold is released instead (`arrived`). Otherwise unpark, reason = the hold's.
   - Invalid: erase, note `release <cause>`, carry on as with no hold.
   - No hold: reason = the first rule whose trigger the bot's spot fails, as now; None for a
     non-urgent reason while the bot's backoff runs.

   Parking and releasing inside the plan is safe: a second read the same tick reaches the same
   answer.
5. **Path test**, in `accept` and `escapeAccept` (so also in `WormSpotStillSafe` and validity): the
   straight walk from the bot to (x, y) must not cross
   - a pool, `SegmentCrossesCircle` at `PoolRadius(tick + ceil(walk s)) + WORM_POOL_PATH_PAD`, walk s
     = 2D length / max(bot run speed, 1); `WormPool` keeps its tick;
   - an up stationary worm at `WORM_SWEEP_PATH_RADIUS`, for everyone but its holder.

   A circle the bot already stands in isn't tested for crossing; the walk must head out of it (dot
   of the walk and centre-to-bot > 0) and end outside. `plan.escapeFirst` while the bot is inside a
   pool's radius now.
6. **Ties.** Wherever `preferNear` is the bot's own spot today, and for a held-out Bile carrier
   whatever its role, it moves `WORM_PREFER_JITTER` toward angle `fmod(guid counter × 2.39996, 2π)`:
   inside a ring of equal walks each bot then takes a different bearing.
7. **Bile carriers.** A held-out carrier (`IsWormBileCarrierHeldOut`'s test) also puts its circles on
   clean bots (held spots, `WORM_BILE_CLEARANCE`) into `escapeCircles`; `looseCircles` drops only
   those.
8. **Melee stands.** For a melee bot (not a tank) whose current target is the up mobile worm: rank =
   its index among the roster's living melee bots by guid, side +1 for even and −1 for odd; stand =
   the worm plus `WORM_MELEE_STAND` along its facing + 180° + side × `WORM_MELEE_GROUP_DEG`. Set
   `stand`, `hasStand`, `preferNear` = stand; `accept` adds at least `WORM_MELEE_REAR_ARC_DEG` off the
   worm's facing. The Melee rule fires with no hold more than `WORM_HOLD_TRIGGER` from the stand.
9. **Role reach** (`roleAccept`), 1 yd inside the generic trigger:
   - ranged: the target within `GetRange("spell")` + both combat reaches − 1; a hunter also at least
     `WORM_MELEE_RANGE + 6` from a worm;
   - melee and tanks: within `WORM_MELEE_RANGE - 1` of a worm, else `GetMeleeRange(target) - 1`;
   - healers: both holders and its `party member to heal` within `GetRange("heal")` + both reaches − 1.
10. **Pools against casts and reach.**
    - `UrgentAt`, also for `BreakPinnedCasts`: a pool whose radius when the member's current cast or
      channel ends (`Spell::GetCastTimeRemaining`) reaches its spot.
    - `WormReachCrossesPool`: a living bot holding no beast duty, with a living target, that generic
      reach would walk (melee: not `IsWithinMeleeRange`; others: not `IsWithinCombatRange(target,
      GetRange("spell") + CONTACT_DISTANCE)`), whose walk to the stop point on the line to its
      target, at that range less 1, fails the path test for pools. The Reach rule is this predicate.
11. **Toxin.** A stuck carrier holding no beast and with no runner seeks the nearest Bile carrier, a
    human included, as a cure walk (`nb.cure` `seek`), staying stuck for `PairRunners`.
12. **Backoff**: `BackOffWormSearch` stamps the bot in the state.

### Group B — nodes and rules

Files: `Action/ToCActions_Jormungars.h`, `Action/ToCActions_Jormungars.cpp`,
`Definition/ToCDefinition_NorthrendBeasts.cpp`; `Multiplier/ToCMultipliers_Jormungars.{h,cpp}` and
`Trigger/ToCTriggers_Jormungars.{h,cpp}` only if needed.

13. **`NorthrendWormsRepositionAction::Execute`**:
    - no plan or reason None: false, leaving the hold alone; `!CanMove`: `stunned`, false.
    - Held: own walk in flight to the spot (booked on it, moving or under 500 ms since issue, under
      `WORM_WALK_LATCH_MS`): `hold <reason>`, true. Else, pinned and not urgent: `locked`, false.
      Else re-issue to the spot.
    - Not held: pinned and not urgent: `locked`, false. Else the first tier with a spot:
      1. `stand` (`hasStand`): the stand itself, clear of `circles` and `spreadCircles` and passing
         `accept` and `roleAccept`;
      2. `escape`, here when `escapeFirst`;
      3. `full`: `circles` + `spreadCircles`, `accept` and `roleAccept`;
      4. `nospread`: `circles`, `accept` and `roleAccept`;
      5. cure and run only, `partner`: `circles`, `accept`;
      6. urgent only: `escape` (`escapeCircles`, `escapeAccept`), then `loose` (`looseCircles`,
         `escapeAccept`);
      7. cure and run only: `direct`, then `partial` (14).

      Sweeps are `FindNearestPositionClearOfHazards` over `WORM_SEARCH_RADIUS`, 2 yd and 22.5° steps,
      `preferNear`, one `HazardSweepCache` per Execute; an empty list takes the bot's own spot at
      radius 0.
    - No spot: `none <reason>`; not urgent: `BackOffWormSearch`, false; urgent: true.
    - Issuing: interrupt (`InterruptNonMeleeSpells(true)`) only when urgent and pinned; clear our own
      stale booking; drop a booking at `MOVEMENT_COMBAT` or below that isn't on the spot (a ToC copy of
      Algalon's `DropForeignBooking` in this file); `TryMoveTo`, forced when urgent or a cure walk,
      else combat. Issued: `SetWormHold(…, false)`, `move <yd> <reason> <tier>`, true. AlreadyThere:
      `SetWormHold(…, true)`, `clear`, false. Otherwise `locked`, return urgent.
14. **`DirectCureSpot`**: `WORM_CURE_DIRECT_BEARINGS` bearings round the partner at `WORM_CURE_REACH −
    1`, the one passing `WormSpotStillSafe` nearest the bot (`direct`). None and the partner past
    `WORM_SEARCH_RADIUS`: `partial`, the point `WORM_CURE_PARTIAL_STEP` toward it, then that heading
    turned ±22.5° and ±45°, the first clear of `circles` passing `escapeAccept`.
15. **Rules**, in `DefineNorthrendBeasts` after the two `OwnTargeting`:
    - `"northrend worms spot hold"`, `Role::Any`, `IsWormSpotHeld`: `Reach | SetBehind | RearFlank |
      CombatFormationMove | Follow | Flee | RunAway | MoveRandom | Charge | Blink | Disengage`.
      Attacks, `ReachHeal` and `avoid aoe` stay.
    - `"northrend worms pool reach guard"`, `Role::Any`, `WormReachCrossesPool`: `Reach`.
    - `"northrend worms follow guard"`, `Role::Any`, a worm engaged (`GetBeastsStageMask &
      BEASTS_STAGE_WORMS`): `Follow`.

    `northrend worms move guard` and the Bile reach guard stay as they are.

### Group C — Icehowl spread, readers, test

Files: `Util/ToCHelpers_Icehowl.h`, `Util/ToCHelpers_Icehowl.cpp`, `tools/botobs/bosses/northrend_beasts.py`,
`tools/botobs/tests/test_northrend_beasts.py`; `Action/ToCActions_Icehowl.{h,cpp}`,
`Trigger/ToCTriggers_Icehowl.{h,cpp}`, `Multiplier/ToCMultipliers_Icehowl.{h,cpp}` only if needed.

16. **Hysteresis**: `ICEHOWL_SPREAD_VICTIM_GRACE_MS = 1000`. With a latch already held, the spread
    stays up while his victim, a player, has been out of his melee range under that long, and doesn't
    re-latch meanwhile. Passive, another beast engaged, a latched charge still turn it off at once.
17. **Room per role**: `ICEHOWL_SPREAD_MELEE_ROOM = 14` (`static_assert` ≥ `ICEHOWL_SPREAD_MELEE_MAX +
    1`). A candidate within `ICEHOWL_SPREAD_MELEE_ARC_DEG` of his back with room in [14, 22.5) is kept
    as melee-only and doesn't count toward the wanted total; only melee seat on one. `RoomRanShort`
    tests each bearing against its own floor.
18. **Deal**: healers, then melee, then ranged, each by guid, same keys. Before dealing, a sticky bot
    whose bearing holds at least 2 more occupants than the emptiest bearing its role may take is
    unseated, last in deal order first.
19. **`--breath`**: with no `cast` row of the four ids from an Icehowl guid, group the `aura` applies
    (not removed, caster an Icehowl guid, a roster victim) into breaths, a new one past
    `BREATH_VICTIM_MS` after a group's first apply; target 0, printed `?`, `predicted` None.
20. **`--cure`**: give each Bile pulse hit to the Bile carrier nearest its victim within
    `radius("WORM_SPLASH_RADIUS")`, in the snapshot nearest the hit, the carrier's span covering it and
    the victim not the carrier; `clean` counts hits on a victim carrying neither Bile nor Toxin, with
    their damage, and the section prints the raid's total.
21. **`--worms`**: `nb.wormhold` counts per bot, first word plus reason or cause.
22. **Test**: Bile pulse `dmg` rows sourced from a Dreadscale guid with positions putting one carrier
    within 10 yd of the victim; an `nb.wormhold` series; an aura-only breath read as one breath.
    `EveryView` still reads every section empty on another boss.

### Integrate

Syntax-check every changed `.cpp` plus `Util/ToCHelpers_Jormungars.h`, `Util/ToCHelpers_Icehowl.h` and
`src/Util/EncounterHelpers.h` (fan-out 60), pblint with `--spell-difficulty`, the Python suite. Then
fold `northrend-beasts.md` with `compact-docs-writer`: the worm and Icehowl "What the bots do" for
every task, these decisions with their rationale, the Known gaps below, and the trace table
(`wormmove` tiers and reasons, `wormhold`, the `--breath` fallback and `--cure` attribution).

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Every reposition walk latches its spot as the bot's hold, parked within 1 yd and walked back past 2.5 | Scope 1; raid-mechanics-lessons (latch split by edge, loose tolerance) | arrive and let go, as before |
| No per-instance caster deal: holds plus member circles on held spots give sticky slots, re-found on emerge (`state.worm`) and anchor drift | Scope 6 asks for stickiness and re-deal on emerge and drift, which the hold carries | a second layout dealt per instance |
| Walking holds are tested like the old walk latch; parked ones against every rule their tier honoured | a nospread or escape spot tested against every rule loops park, release, search | every rule always |
| Escape, loose and partial spots are released on arrival, never parked | they ignore reach, and the spot guard would strand the bot there (pitfalls, `ReachTargetAction`) | park them |
| Release on target change (not healers), worm form change, anchor drift 2.5 yd or 20° for melee and 6 yd for the rest, a healer's heal target out of reach | Scope 1; the Icehowl healer yield | no healer clause, which fights `reach party member to heal` |
| The spot guard also zeroes flee, run away and move random | class escapes outrank `ACTION_MOVE` (raid-mechanics-lessons, range and reach); the hunter floor in task 9 keeps them past minimum range | the brief's list alone |
| Path test: a pool at its radius when the walk ends + 1, Sweep at 16, a circle the bot stands in left only outward | Scope 3; Stormweaver's 3:27.4 walk ran 2 yd from a pool's centre and killed it | growth along each point of the walk |
| Member circles sit on held spots, not also on current positions | a carrier walking past a held spot would release it in turn | both |
| Ties broken by a guid-angled 0.5 yd shift of `preferNear` | same effect as a guid start angle without touching the shared sweep or its cache key | an additive start-angle overload |
| Bile carriers pick in tick order against held spots, the escape keeps their clean-bot circles, `loose` drops only those last | Scope 5; five carriers took one spot within 90 ms at 3:21.9 | a guid-ordered deal per instance |
| Spread 10 / 13 yd, casters off casters only, the melee as one 19.3 / 22.3 yd circle on the mobile worm, off while the stationary worm is under | Spray's 10 yd splash, centre to centre; guide `/range 10` | spacing off all 24 |
| Reach in combat-reach terms, spread relaxed before reach, reach never dropped on a non-urgent move | `reach spell` fires past `IsWithinCombatRange` 28.5 + 0.5 + both reaches, ~37 yd (`RangeTriggers.cpp:155`), against the old raw 28.5 | raw ranges |
| Melee stands two groups at ±54° off the worm's back, 8.3 yd out (13.4 yd apart), rear arc ≥ 108° | worms parry and parry-haste (`flags_extra` 0x80001); `set behind`'s 3π/5; ±39° is the 10.5 yd minimum | one stack behind; ±72°, no margin for his turn |
| Melee groups by guid parity among the roster's melee bots | every bot derives the same split | balance by target |
| Follow zeroed for everyone while a worm is engaged | Scope 8; Malygos and Yogg-Saron rules | targetless bots only |
| The pool reach guard pairs with a Reach reason that moves the bot | zeroing reach alone strands it out of range (pitfalls) | guard only |
| A pool that reaches the bot before its cast ends counts as urgent | Scope 10; a pinned caster runs no triggers, so the per-instance read interrupts it | wait the cast out |
| A stuck carrier with no runner walks to any Bile carrier, a human too; holders still wait | Angry, Assasin and Justice kept Toxin 4:04-5:04 with only the human tank carrying Bile; a walking holder drags its worm and turns its Spew | holders walk too |
| A non-urgent search with no spot backs off 1 s | each fruitless search is up to 240 collision checks a tick | retry every tick |
| Icehowl victim grace 1 s | `nb.arc` flipped every ~100 ms at 4:56-4:57 and 5:44-5:45; he runs 16 yd/s and closes a Whirl throw in ~2 s | a longer grace, which chases him |
| Melee-only bearings at 14 yd of room | the melee band ends at 13; walls cut the ranged room first | one floor for all |
| Deal healers, melee, ranged, rebalancing at +2 | every melee held −3 at 5:45, the only rear bearing kept, and stayed there after re-latches added bearings | the old order, sticky for good |
| Task 13: few bearings survive because he is fought at the wall with his back to it; no tank re-orientation in this lane | counts match a 48-50 yd wall circle (northrend-beasts.md, Icehowl bearings); turning him needs room on the wall side that a charge end doesn't leave | the holder turns him |
| `--breath` without a target when only auras exist | Icehowl's casts never reach the trace | infer the target |
| `--cure` gives a pulse to the nearest carrier within 10 yd | every pulse row names Dreadscale | leave it by source |
| No coordinates | the arena floor is GO 195527 | — |
| Cure and run holds release (`target`) when the partner changes or goes and skip the target, drift and heal tests; a parked one holds while the partner is within 8 yd of the spot, and walks back past 1 yd when it owes a cure walk | the cure trigger, so a partner shifting a yard doesn't restart the walk | accept's 6 yd reach |
| A Cure hold keeps its partner while that one carries Bile and is within 8 yd or no carrier is | stops flipping between two carriers at similar distances | nearest carrier every read |
| Escape, loose and partial holds stay in the escape test after urgency ends; a partial one also releases on a partner change | accept's partner reach and rear arc would release a partial walk at once | accept once not urgent |
| A parked hold walks back past 1 yd when the bot's own spot is urgent | an urgent hazard can't wait out the 2.5 yd deadband | 2.5 always |
| Park and unpark are tested in the read that makes them | a trigger read and an action read in one tick agree | test on the next read |
| A held spot answers to the generic reach trigger (role reach + 1 yd); searches use role reach + 0.05 | the stand sits exactly on the 8.3 yd role reach: it would release on the worm's first step or fail on float error | one reach for both |
| Held-out Bile carriers in melee and tank roles: no role reach, no stand, out of `WormReachCrossesPool` | the Bile reach guard already zeroes their `reach melee`; with reach required a non-urgent Bile move finds no spot | require reach |
| No Melee or Reach rule at a held spot: drift deadband and role reach instead | a 20° turn still leaves a stand ≥ 106° off the facing; the Reach rule is about the bot's own walk | test both |
| Stands only for melee bots carrying neither debuff; cure pairs and carriers keep their old `preferNear`; melee and tank holds use the 2.5 yd / 20° drift, others 6 | they have a partner to reach or a stack to keep off | stands for every melee |
| The hunter floor holds against every up worm | a hunter's spot can sit near the stationary worm too | its target only |
| `SetWormHold` on the same spot, reason and tier keeps the first snapshot | drift counts from the first set, not from where the worm has got to | a fresh snapshot per issue |
| A holder whose run ends releases its hold (`target`) | only a run gives a holder a spot | keep it |
| AlreadyThere on an escape, loose or partial spot sets no hold | those spots are never parked; parked, the spot guard would strand the bot out of reach | park it, or set it unparked and release on `arrived` |
| The action tracks its own last issued spot (`issuedSpot`) | it clears its own stale forced booking after the plan released that hold, else the next combat walk waits behind it | clear only a booking on the current hold spot |
| Duplicate searches skipped: `nospread` with no spread circles, `loose` equal to `escape`, `escape` not repeated after `escapeFirst` | same answer, less collision work | run every tier |
| Stand, direct and partial points unclipped | the floor test keeps them inside the arena wall, like the old direct spot | clip each like the sweep |
| Follow guard on every bot, human-master followers included | Scope 8 | targetless bots only |
| Icehowl grace starts at the first out-of-reach read and resets only once the victim is back; a null or non-player victim turns the spread off at once | the grace is for a tank flickering at the edge | grace for every victim |
| The rebalance counts the emptiest bearing by bots for healers too; a moved healer can land on its old bearing again | no movement results, only wasted work | the healer key in the rebalance |
| `--cure` skips hits on anyone carrying Bile or Toxin; a span covers a hit up to 250 ms after it ends | only clean hits count; the existing `CURE_WINDOW_MS` slack | count hits on carriers |
| `--breath` fallback reads victims from every apply in the window, only the grouping needs Icehowl as caster | same as the cast path | Icehowl-cast victims only |
| Review: an urgent bot with no escape or loose spot takes `cross`, the escape's then loose's circles with only the floor and Spew cones, after `direct`/`partial`; its walk skips the path test and releases on arrival | a bot near the 35 yd floor edge, beside the up stationary worm or between two pools froze in Slime Pool (`none pool` every tick); `FindNearestPositionClearOfHazards`: crossing a hazard beats standing in it | freeze, as with the path test alone |
| Review: no one but the `WormStationary` holder pairs with a cure partner within 11 yd (16 − 6 + 1) of the up stationary worm, in `PairingOf` and `PairRunners` | every cure spot round the stationary holder lies inside the 16 yd Sweep path circle; guide: free them while the worms are submerged | keep the nearest carrier and loop `none cure` |
| Review: the pool reach guard and the Reach rule are off past the search radius (walk to the stop point over 30 yd); the guard also during the bot's backoff | with no spot found, zeroing reach strands the bot (pitfalls) | guard while any pool lies on the line |
| Review: the spot hold and pool reach guard block `reach melee` and `reach spell` by name, not `Family::Reach` | that family holds `reach party member to resurrect`, every battle res's prerequisite (pitfalls) | block the family |
| Review: `nb.wormhold` writes `set` only for a new latch, `unpark` when pushed off, `release phase` on Disengage and `release gone` for a bot leaving the roster | change-only keys need an explicit closing value (pitfalls); a walk back read as a new latch | let the reader cut spans |

## Carried over

- Outside ToC, for the report: `EncounterHelpers::SegmentCrossesCircle` (additive).
- `w6-closeout`: move the ToC segment copies (`DistanceToSegment2d` in `ToCHelpers_Jaraxxus.cpp`,
  `SegmentDistance2d` in `ToCHelpers_Anubarak.cpp`, `DistanceToIcehowlLane`) onto it.
- Merge stage, engine lesson candidate for `raid-mechanics-lessons.md`: a spot checked only at its
  endpoint lets the walk there cross the hazard it dodges, and a growing one has to be tested at its
  size when the walk ends.
- No `refine-beasts-rest.PLAN.md`: every Scope item fit. The Icehowl tank re-orientation at the
  wall is a known gap, not carried scope.

Open issues after re-review: none. Deferred findings: none.

## Known gaps

- Fought at the wall with his back to it, Icehowl keeps 3-4 bearings and none near his back: no tank
  turns him (guide: the tank stands with its back to a wall).
- Bile carriers find their spots in tick order, not guid order.
- The path test covers pools and Sweep, not Spew cones or Bile carriers on the way; a `cross` walk
  takes none, and generic reach skips it past the search radius or during a backoff.
- A Toxin'd `WormStationary` holder gets no runner until its worm submerges.
- `reach party member to heal` still walks healers through pools.
- Melee groups split by guid parity, so they're uneven when some melee hit another target.

## Blocked

None.
