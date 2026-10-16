# w5-anubarak-rest — Anub'arak: drag oscillation, pull threat, phase-2 patches, then w5's carry-over

Wave 4d, a refine lane. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics and the shipped design:
[anubarak.md](../../raids/trial-of-the-crusader/anubarak.md). Guide:
`anubarak-master-strategy-guide-toc-25/`. Read `tools/botobs/bosses/anub_arak.py` output before
designing each item. Line numbers are as of `1846fc944`.

Trace (25N kill 4:11, 5 deaths, raid DPS 125k, human tank "Dragon", build carrying waves 3-4c):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_anub-arak_1792178435.ndjson`.
Read it with `bosses/anub_arak.py <f>` (sections `--pull --threat --spike --spheres`),
`postmortem.py <f> --probes move.by` (who owned each move) and `--probes anub.pickup`.

## Findings from the 2026-10-16 25N pull

**Drag oscillation.** The bot main tank Bulwark walks the boss about 25 yd every 8-9 s in phase 1
(x 711 ↔ 736) and about 14 yd every 10 s after the emerge; the boss's median distance from the
anchor is 7.8 yd.

- `DragBossOnto` (`ToCActions_Anubarak.cpp:261-284`) stops only with the boss inside
  `ANUBARAK_DRAG_ARRIVE` = 2 yd of the anchor and aims the tank "his current gap" past it.
- He steps only once the tank is about 12 yd away (his melee reach), then closes to about 5 yd, so
  one drag moves him at least about 7 yd: the 2 yd target can't be hit. He overshoots, the next
  tick sees him past the anchor, and the direction flips.
- Knock-on: `reach melee` ↔ `set behind` flipped 216 times; the tank loses swing time.

**Pull death.** Bulwark dragged from the first tick and landed no threat ability for 7 s (first Holy
Shield 7.8 s). Angry (Intercept 1.2 s, Bloodthirst 4.9 s) pulled the boss at 5.4 s and died to one
32.6k hit. Until the patches land (9-12 s) the anchor is the room centre (`GetAnubarakBossAnchor`,
`ToCHelpers_Anubarak.cpp:1170`), so he is dragged twice.

**Phase-2 deaths.**

- The four patches made in phase 1 went to the first four spikes (none left at 1:43).
- Agony held the only `kite` sphere duty from 1:46 to 2:09 and never shot: he spent it all in
  `anubarak avoid spike` (duties: `ComputeDuties`, `ToCHelpers_Anubarak.cpp:856`).
- With no Permafrost, Mighty kited 16.8 s and died; the spike then ran through the ranged:
  Stormweaver, Trueshot and Druidica died at 2:08, two Impales each.
- `anubarak avoid spike` flipped with `reach spell` 130 times and `reach melee` 107 times; Trueshot
  died while `reach spell` walked him back. Same release-on-arrival pattern `refine-beasts` fixed on
  the worms.

Phase 3 and the stretch after the emerge were clean: no deaths, lust at phase 3 as designed.

## Scope

New items first; then w5's carry-over, in its order, as far as the lane holds.

1. **Drag with a deadband.**
   - Start a drag only past a start radius; end it inside a smaller done radius, both measured on
     the boss. Done radius at least half the minimum step (about 4 yd).
   - Plan the tank's spot from his real chase threshold and stop gap, read live (his melee range),
     not constants. Never re-plan while he moves.
   - `ANUBARAK_SUBMERGE_PATCH_CLEARANCE` (`ToCHelpers_Anubarak.h:62`) adds the arrive radius: keep
     it satisfiable, checked against the room with `navprobe` and the trace's positions.
2. **Pull threat.** No drag until both tank patches are latched, which also removes the room-centre
   detour; the tank holds him where it picks him up. The investigator picks and logs the fallback if
   patches never land. Optional: a DPS opener hold on him (no threat brake exists; check
   `docs/engine/pitfalls.md` first).
3. **Phase-2 patch supply.** Shoot spheres before patches run out (e.g. always one patch per
   remaining kite); more than one shooter; never give the duty to the spike's target or a bot
   dodging it, unless it can shoot from where it stands. Item 5 is the backstop.
4. **Spike dodge hold.** Hold the dodge spot until the spike has passed, with a Block rule zeroing
   `reach melee` and `reach spell` by name while the hold is up: the worm pattern,
   `e.Block("northrend worms spot hold", ...)` in `ToCDefinition_NorthrendBeasts.cpp:35-38`.
5. **Kiter immunity.** A kiter with no patch dies once the spike reaches 11.4 yd/s. Impale is
   physical (school 1), so Hand of Protection voids it; the guide's heroic phase 2 has a paladin
   HoP the kiter just outside a patch and, for the last kite, stand still under HoP where he should
   emerge. Also self-immunities (Divine Shield, Ice Block). `anub.kite` `ring` rows and Impale hits
   on kiters (`spike` section) say how often it is needed.
6. **Phase 3 raid-health economy.** Leeching Swarm takes 10/10/20/30% of *current* health a second
   and heals him by it, so the guide keeps the raid low: no healing for melee, hunters and
   affliction warlocks, fast healing only for Penetrating Cold carriers, a cancelaura on stamina
   buffs at phase 3. Needs a heal hold that cannot starve a real emergency; `swarm` section first.
7. **Freezing Slash defensives.** The guide wants one per slash. It is a non-triggered instant with
   no warning: first 7-15 s after the engage or emerge, then every 15-20 s, so only a timer model
   anchored on those edges can place one.
8. **Burrower pacing.** Guide: throttle AoE so each wave dies just before the next (45 s cadence),
   and once he is low, ignore burrowers for him (heroic keeps them coming in phase 3).
9. **Landing and regroup.** The floor break drops the raid into what is likely water under the
   arena; navprobe cannot path out of it. The Web Door (661.6, 144.7) shuts at the engage. Act only
   if the `pull` section shows bots stuck or locked out.
10. **Scarabs.** 20000 threat on a random player each; Acid-Drenched Mandibles stacks (guide: limit
    stacks going into phase 3). Consider a DPS threat brake (none exists, see pitfalls) and stack
    management.
11. **Spider Frenzy.** Burrowers within 12 yd haste each other; 25H brings four a wave to two
    patches, 10H two to one tank. A third patch, or spacing on the patch.
12. **Phase 2 raid positioning.** Guide: raid to one corner for the first spike, back to the centre,
    then stacked where he should emerge; w5 ships only the spike dodge and the boss drag off the
    patches 15 s before each submerge.

**Reader.** `--threat` reports drag reversals per phase and the boss's distance from the anchor;
`--spheres` shows each phase-2 kite's patch supply.

**Wave 4e** (`w8-prepull-staging`) reads this lane's anchor and side points for the pre-pull
stands: keep them reachable through a helper.

## Owns

As `w5-anubarak`: stem `Anubarak`, `docs/raids/trial-of-the-crusader/anubarak.md`,
`tools/botobs/bosses/anub_arak.py`, `tools/botobs/tests/test_anub_arak.py`.

## Task list

Status: groups A-C implemented, integrated, reviewed and committed; "Decisions for review" rows 2-5
come from the review.

Scope taken: items 1-5 and 9 (settled, no code); 6-8 and 10-12 are carried over (below). Read
[anubarak.md](../../raids/trial-of-the-crusader/anubarak.md) first: "How he follows a tank", the
spike's "Standing still breaks it" and "Immunities", and "An untouched sphere is no valid target"
are the facts the tasks build on. Paths are under `src/Ai/Raid/ToC/` unless they start with `docs/`
or `tools/`; line numbers as of `83a157cc7`. No new files, so no CMake re-run, and no seam outside
the stem: `ToCStrategy.cpp` already wires the stem lists and `ToCAnubarakDefinition()`. Probe keys
stay string literals on the line of the call.

Root causes the tasks fix, from the 2026-10-16 trace:

- **Drag.** `DragBossOnto` (`Action/ToCActions_Anubarak.cpp:261-284`) re-plans every tick from his
  live position against a 2 yd arrive, but he starts only past 12 yd and stops wherever his 400 ms
  recheck finds the tank still: 17 legs and 9 reversals (>120°) in phase 1, 11 and 8 after the
  emerge (the shipped `--threat` count). While dragging the tank swings at nothing, so Angry took him at 5.4 s.
- **Taunts.** The bot main tank and the human tank traded 11 taunts in phase 1 (Bulwark 7, two of
  them Righteous Defense), through the paladin's generic `lose aggro` taunt; he obeys taunt DR.
- **Spheres.** Sphere duty never landed a shot: an untouched sphere is a critter out of combat, so
  `drop target` follows every `Attack()`. The duty also went to one shooter (Agony, lowest guid) who
  spent 1:46-2:09 dodging, 55-72 yd from its sphere, and from 2:28 to the kill held side 1 without a
  shot. Starfall made every patch; mages sheeped spheres six times.
- **Dodge.** `AnubarakAvoidSpikeAction` (`:138-162`) returns `MoveTo`'s false on a `dup`, so `reach
  spell`/`reach melee` cancel the walk, and it lets go on arrival: 130 and 107 flips.
- **Kites.** 5 of 6 `ring` kites took Impales; nobody had an immunity answer.

### Contracts

Group A writes these in `Util/ToCHelpers_Anubarak.h` (namespace `TrialOfTheCrusaderHelpers`); group B
codes against them; change them only through the integrate step. `ANUBARAK_DRAG_ARRIVE` goes;
`GetAnubarakBossAnchor` keeps its meaning (both patches' midpoint, else the room centre) for
`w8-prepull-staging`, but no node drags to it any more.

```cpp
// Drag deadband, measured on him. In the submerge window DONE is also the start radius.
constexpr float ANUBARAK_DRAG_START = 8.0f;
constexpr float ANUBARAK_DRAG_DONE = 4.0f;
// He has stood still past his 400 ms chase recheck
constexpr uint32 ANUBARAK_DRAG_SETTLE_MS = 500;
// A leg he has not settled from by then is dropped
constexpr uint32 ANUBARAK_DRAG_LEG_MS = 6000;
// Spike reach round a patch, plus its 3.5 yd/s * 1.5 s first walk, plus the drag's done radius
constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE = ANUBARAK_SPIKE_PATCH_REACH + 5.25f + ANUBARAK_DRAG_DONE;
// Second search when no spot clears the first
constexpr float ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN = ANUBARAK_SPIKE_PATCH_REACH + 5.25f;

// DPS bots stop on him at this share of his tank victim's threat, resume under the release share
constexpr float ANUBARAK_THREAT_CAP = 0.9f;
constexpr float ANUBARAK_THREAT_RELEASE = 0.8f;

constexpr uint8 ANUBARAK_SHOOTERS_PER_NEED = 2;
// Patches that aren't tank patches kept ready in phase 2, falling spheres counted
constexpr uint8 ANUBARAK_P2_PATCH_STOCK = 2;
constexpr uint8 ANUBARAK_P2_PATCH_STOCK_HEROIC = 1;
// A shooter reaches a sphere within its "spell" range less this, 3D, in line of sight
constexpr float ANUBARAK_SHOOT_MARGIN = 2.0f;

constexpr float ANUBARAK_DODGE_ARRIVE = 2.0f;        // moved here from the action file
constexpr float ANUBARAK_DODGE_HOLD_TRIGGER = 2.5f;  // a held bot further off its spot lets go
constexpr float ANUBARAK_DODGE_HOLD_RELEASE = 20.0f; // spike this far from the spot: it has passed
constexpr uint32 ANUBARAK_DODGE_WALK_MS = 3000;
constexpr uint32 ANUBARAK_DODGE_HOLD_MS = 8000;

constexpr float ANUBARAK_KITE_PROTECT_RANGE = 12.0f;  // spike this close: a paladin HoPs the kiter
constexpr float ANUBARAK_KITE_IMMUNITY_RANGE = 9.0f;  // Impale's 6 + 1.5 plus a margin: own immunity
constexpr float ANUBARAK_HOP_RANGE = 30.0f;
constexpr uint32 SPELL_FORBEARANCE = 25771;
// Hand of Protection ranks, Divine Shield, Ice Block
constexpr uint32 ANUBARAK_KITE_IMMUNITIES[] = {1022, 5599, 10278, 642, 45438};

enum class AnubarakDragStep : uint8 { None, Walk, Settle };
struct AnubarakDragLeg
{
    AnubarakDragStep step = AnubarakDragStep::None;
    Position tankSpot;
};

// Midpoint of both latched tank patches. False until both have landed: he is held where picked up.
bool GetAnubarakDragAnchor(Player* bot, Position& out);
// The pickup tank's leg toward point, latched per instance. Called by the hold action only.
AnubarakDragLeg GetAnubarakDragLeg(Player* bot, Creature* boss, Position const& point, bool submerge);
void ClearAnubarakDrag(Player* bot);
// A latched leg is walking or waiting for him to settle: the Block rule's predicate, a pure read
bool IsAnubarakDragInFlight(PlayerbotAI* botAI);

// Current target is him and his victim is a tank other than this bot
bool AnubarakTauntWouldSteal(PlayerbotAI* botAI);
// Latched per bot: capped at ANUBARAK_THREAT_CAP of his tank victim's threat, freed under RELEASE
bool AnubarakThreatCapped(Player* bot);

// The bot's dodge spot while the hold is valid: walking (set within ANUBARAK_DODGE_WALK_MS) or
// parked within ANUBARAK_DODGE_HOLD_TRIGGER. Pure read; an invalid hold reads false.
bool GetAnubarakDodgeHold(Player* bot, Position& spot);
void SetAnubarakDodgeHold(Player* bot, Position const& spot);
bool IsAnubarakDodgeHeld(PlayerbotAI* botAI);

// 3D within the bot's spell range less ANUBARAK_SHOOT_MARGIN, in line of sight
bool AnubarakSphereInReach(Player* bot, Unit* sphere);
// Floor point toward the sphere where it is in reach, off the spike and its lane. False when none.
bool GetAnubarakSphereStand(Player* bot, Unit* sphere, Position& out);

// The kiter this paladin bot covers with Hand of Protection now, null when it has no such duty
Player* GetAnubarakKiterToProtect(Player* bot);
// The marked bot itself must cast an immunity now: no protector, spike within the immunity range
bool AnubarakKiterNeedsImmunity(Player* bot);
bool AnubarakKiterImmune(Unit* kiter);
```

| Key | Writer | Values |
|---|---|---|
| `anub.pickup` | B | `taunt`, `attack`, `hold` (no leg), `drag` (leg to the anchor), `submerge` (leg to the submerge spot) |
| `anub.sphere` | A | gains `stock` |
| `anub.protect` | A | per bot, change-only: `hop` while this paladin holds the duty, `self` while the marked bot needs its own immunity, `none` |

### Group A — state and geometry

Files: `Util/ToCHelpers_Anubarak.h`, `Util/ToCHelpers_Anubarak.cpp`.

1. **Contracts** above. New per-instance fields go in `AnubarakState` (`:80-122`); every latch
   below is cleared by `ClearPullLatches` (`:309`).
2. **Drag.** `GetAnubarakDragAnchor`: both `LatchedTankPatch` sides resolve, else false.
   `GetAnubarakDragLeg` keeps one leg per instance (tank guid, point, tank spot, planned ms) and his
   stillness (`!boss->isMoving()` since when):
   - his victim isn't the bot → clear, `None`;
   - a latched leg: he is within `ANUBARAK_DRAG_DONE` of the point → clear, `None`; he has not
     stood still `ANUBARAK_DRAG_SETTLE_MS` and the leg is under `ANUBARAK_DRAG_LEG_MS` → `Walk`
     while the bot is over 1 yd from the spot, else `Settle`; settled (or over the ceiling) → judge:
     past the start radius re-plan, else clear and `None`. A point that moved more than
     `ANUBARAK_DRAG_DONE` from the leg's (the submerge window opening, a patch re-latched) is judged
     the same way, never mid-walk;
   - no leg: plan only once he has stood still `ANUBARAK_DRAG_SETTLE_MS` and stands past the start
     radius (`ANUBARAK_DRAG_START`, or `ANUBARAK_DRAG_DONE` when `submerge`). Read live: C =
     `boss->GetCombatReach() + bot->GetCombatReach() + CONTACT_DISTANCE`, k = his run speed over
     the bot's, E = his run speed × 0.2 s, D = his 2D distance to the point. Tank spot = his position
     + unit(point − him) × (C + max(D − E, 0) / k), through `ValidateFloorPoint` → `Walk`.
   - `IsAnubarakDragInFlight`: a latched leg for this bot, under the ceiling, his victim the bot.
3. **Submerge spot** (`GetAnubarakSubmergeSpot`, `:1182-1222`): clearance
   `ANUBARAK_SUBMERGE_PATCH_CLEARANCE`, then `_MIN` when nothing clears it; same latch.
4. **Threat.** `AnubarakTauntWouldSteal`: engaged, current target him, his victim a player for whom
   `IsAnubarakTankPlayer` holds and not the bot (Thorim's `ThorimTauntWouldSteal` shape).
   `AnubarakThreatCapped`: phase `Surface`/`Swarm`, `IsToCDpsBot`, his victim a tank player other than
   the bot; own = `boss->GetThreatMgr().GetThreat(bot)`, tank = the same for the victim; set when own
   > 0 and own ≥ CAP × tank, cleared when own < RELEASE × tank, per-bot latch in the state, memoised
   per bot per ms. Never through the `threat` AI value (a wrapping uint8).
5. **Dodge hold**, per bot in the state: spot, the kiter's guid, set ms. Valid while engaged, the bot
   unmarked, the spike alive, the mark on the same kiter, the spot at least
   `ANUBARAK_SPIKE_DANGER_RADIUS` from the spike and `ANUBARAK_SPIKE_LANE_HALF_WIDTH` off the
   spike→kiter segment, the spike within `ANUBARAK_DODGE_HOLD_RELEASE` of the spot, under
   `ANUBARAK_DODGE_HOLD_MS` since set, and either under `ANUBARAK_DODGE_WALK_MS` since set or the
   bot within `ANUBARAK_DODGE_HOLD_TRIGGER` of the spot. `GetAnubarakSpikeDodgeSpot` (`:1506`) adds,
   in heroic, every patch as a hazard circle of `ANUBARAK_PERMAFROST_SLOW_REACH`, and searches again
   without them when that finds nothing.
6. **Sphere duty** (`ComputeDuties`, `:856-944`):
   - Shooters: alive ranged DPS bots in the pit, unmarked, not walking a dodge hold (valid and over
     `ANUBARAK_DODGE_HOLD_TRIGGER` from its spot); one parked on a hold only for a sphere
     `AnubarakSphereInReach` from where it stands.
   - Needs, in order: `kite` (unchanged); `stock` while `Submerged`, one at a time, while patches that
     aren't latched tank patches plus falling spheres number under the stock
     (`ANUBARAK_P2_PATCH_STOCK`, heroic `_HEROIC`): the flying sphere nearest
     `ANUBARAK_ROOM_CENTER` that lies over `ANUBARAK_SPIKE_PATCH_REACH + 5.25` from the spike (from
     him while no spike lives) and over `ANUBARAK_SPIKE_LANE_CLEARANCE` off the spike→kiter segment;
     then sides `0`, `1` (unchanged, heroic reserve unchanged).
   - Up to `ANUBARAK_SHOOTERS_PER_NEED` shooters per need, latched per need while its sphere is
     unchanged and the shooter eligible; a vacancy takes the eligible unassigned shooter nearest the
     sphere (2D), guid breaking a tie. Notes as now, plus `stock`.
   - `AnubarakSphereInReach`: `bot->IsWithinDist3d(sphere, GetRange("spell") − ANUBARAK_SHOOT_MARGIN)`
     (the bot's own AI) and `IsWithinLOSInMap`. `GetAnubarakSphereStand`: the point on the line from
     the bot to the sphere's ground point whose 3D distance to the sphere is that reach (the sphere
     flies about 13.5 yd up), floor-checked, rejected inside `ANUBARAK_SPIKE_DANGER_RADIUS` of the
     spike or where `AnubarakSpikeLaneCrosses`.
7. **Kiter protection**, derived once per instance per ms. Need: a marked group member, alive, not
   `AnubarakKiterImmune` (any `ANUBARAK_KITE_IMMUNITIES` aura), no Forbearance, the spike within
   `ANUBARAK_KITE_PROTECT_RANGE` (2D). Protector: alive unmarked paladin bots by guid, first with
   `CanCastSpell("hand of protection", kiter)`, within `ANUBARAK_HOP_RANGE` and in sight
   (`CanCastSpell` waves out-of-range through). One claim per mark window, kept until the mark moves;
   a claim 1.5 s old with no immunity on the kiter passes to the next paladin.
   `AnubarakKiterNeedsImmunity`: the bot is marked and not immune, the spike is within
   `ANUBARAK_KITE_IMMUNITY_RANGE`, and no protector claim stands. Notes `anub.protect`.

### Group B — nodes and rules

Files: `Trigger/ToCTriggers_Anubarak.h`, `Trigger/ToCTriggers_Anubarak.cpp`,
`Action/ToCActions_Anubarak.h`, `Action/ToCActions_Anubarak.cpp`, `Multiplier/ToCMultipliers_Anubarak.h`,
`Multiplier/ToCMultipliers_Anubarak.cpp`, `Definition/ToCDefinition_Anubarak.cpp`.

8. **Ladder.** New rows; the rest unchanged; no ties. Each trigger starts with `AnubarakEngaged`.

   | Relevance | Trigger → action | Who, when | Does |
   |---|---|---|---|
   | EMERGENCY+9 | `anubarak kiter needs immunity` → `anubarak kiter immunity` | `AnubarakKiterNeedsImmunity(bot)` | first of `divine shield`, `hand of protection`, `ice block` that `CanCastSpell` allows on self |
   | EMERGENCY+7 | `anubarak kiter needs protection` → `anubarak protect kiter` | `GetAnubarakKiterToProtect(bot)` | `hand of protection` on that kiter |

   Both actions are plain `Action`s (no movement), registered in the stem contexts.
9. **Main tank hold** (`:233-284`). Taunt and attack as now. Then the point: `GetAnubarakSubmergeSpot`
   (`submerge`), else `GetAnubarakDragAnchor`, else `ClearAnubarakDrag`, note `hold`, return false.
   `GetAnubarakDragLeg`: `Walk` → `MoveTo` the tank spot at `MOVEMENT_COMBAT` when not already booked
   on it (`ReleaseStalledWalk` first), note `drag`/`submerge`, return true only on the tick that
   issues; `Settle`/`None` → false. Drop `DragBossOnto` and `ANUBARAK_DRAG_STEP`.
10. **Avoid spike** (`:138-162`). A valid `GetAnubarakDodgeHold` whose spot is still clear: within
    `ANUBARAK_DODGE_ARRIVE` → false; walking (bot moving, booked on the spot) → true without
    `MoveTo`; else `MoveTo` it at `MOVEMENT_FORCED`, return `issued || bot->isMoving()`. Otherwise a
    new `GetAnubarakSpikeDodgeSpot`, `SetAnubarakDodgeHold`, walk. The latch members go; channel
    break and stalled-walk release stay.
11. **Destroy frost sphere** (`:187-191`): `Attack(sphere)` returns true → true. On it already:
    `AnubarakSphereInReach` → false (the rotation shoots); else `GetAnubarakSphereStand` and `MoveTo`
    at `MOVEMENT_COMBAT`, false when none.
12. **Multipliers** in `ToCMultipliers_Anubarak.{h,cpp}`, wired from the Definition:
    - `AnubarakThreatCapMultiplier` ("anubarak threat cap"): `AnubarakThreatCapped(bot)`, not a
      `CastHealingSpellAction`, and the action's target is him (or none with him as current target),
      or `EncounterHelpers::IsAoeThreatAction` within 30 yd of him → 0 (Onyxia's
      `OnyxiaThreatHoldMultiplier`).
    - `AnubarakSphereGuardMultiplier` ("anubarak sphere guard"): while engaged, an action whose target
      is a Frost Sphere → 0 unless it is the bot's `GetAnubarakSphereToShoot` sphere and the name is
      none of `polymorph`, `hex`, `repentance`, `freezing trap`, `shackle undead`, `fear`, `banish`,
      `hibernate`, `entangling roots`, `cyclone`, `sap`, `blind`, `seduction`.
13. **Definition** (`Definition/ToCDefinition_Anubarak.cpp`), after the four `OwnTargeting` rules:
    - `e.Block("anubarak spike dodge hold", Role::Any, IsAnubarakDodgeHeld, SetBehind | RearFlank |
      CombatFormationMove | Follow | Flee | RunAway | MoveRandom | Charge | Blink | Disengage,
      {"reach melee", "reach spell"})`, the worm hold's shape (`ToCDefinition_NorthrendBeasts.cpp:35-39`);
    - `e.Block("anubarak boss drag hold", Role::Any, IsAnubarakDragInFlight, SetBehind | TankFace |
      CombatFormationMove | Follow | Charge, {"reach melee"})`;
    - `e.Block("anubarak taunt guard", Role::Any, AnubarakTauntWouldSteal, Taunt)`;
    - `e.Block("anubarak sphere shooter drop guard", Role::Any, <shooter whose current target is its
      flying duty sphere>, DropTarget)`;
    - `e.Multiplier<AnubarakThreatCapMultiplier>(Spell | Melee)`,
      `e.Multiplier<AnubarakSphereGuardMultiplier>(Spell | Attack)`.

### Group C — reader, test, doc

Files: `tools/botobs/bosses/anub_arak.py`, `tools/botobs/tests/test_anub_arak.py`,
`docs/raids/trial-of-the-crusader/anubarak.md`. Read the radii from the source with `radius()`.

14. **`--threat`**, per surfaced window: his legs of 2 yd or more, yards walked, reversals (a leg
    turning more than 120° from the last), median distance off the point he is dragged to, now
    `GetAnubarakDragAnchor`'s midpoint (none: no figure). Taunts gain Righteous Defense (31789,
    counted when the friend it targets was his victim) and a human/bot split; `anub.pickup`
    counts; `anubarak threat cap` veto rows per bot. Baseline on the 2026-10-16 trace: P1 17 legs, 163
    yd, 9 reversals; P1b 11, 113, 8.
15. **`--spheres`**, per spike window: patches that are and aren't tank patches, flying and falling at
    the mark, the `anub.sphere` holders at the mark; per duty span (holder, label): casts by the
    holder at that sphere and its `drop target` acts; `stock` needs.
16. **`--spike`**: per window, Hand of Protection (1022/5599/10278) on the kiter, Divine Shield or
    Ice Block by it, and `anub.protect` rows.
17. **Test**: synthetic rows pinning the leg and reversal counts, per-window patch supply, duty-span
    shots and the protection columns; the shared fixture still reads empty without raising.
18. **Doc**, via `compact-docs-writer`: "How the bots play it" gains the drag, threat cap, taunt guard,
    sphere duty, sphere guard, drop guard, dodge hold and kiter protection, each with its why from
    "Decisions for review"; the ladder and "What a trace answers" tables follow the contracts; Known
    gaps per this brief.

### Integrate

Compile groups A and B together, run the checks, fix the seams. Report for the merge stage: no code
outside ToC; no new hand-rolled per-instance state (all in `RaidInstanceState<AnubarakState>`);
engine lessons for `docs/engine/pitfalls.md` ("Things the bot cannot see": an out-of-combat critter
is an invalid target, so `Attack()` on one is dropped 0.2 s later; "Movement": a creature stops on
its next 400 ms recheck once a still victim is inside its reach, so a drag plans one leg and never
re-plans while he moves).

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| One latched leg, planned only after he has stood still 500 ms; tank spot on the ray from him to the point at C + max(D − E, 0)/k, read live; start 8 yd, done 4 yd, done also the start in the submerge window | Core chase: starts past both reaches + 0.5, heads 2 yd past a moving victim, stops at the 400 ms recheck; trace 17 legs, 9 reversals in phase 1 | Re-plan each tick on his gap (current); fixed step constants |
| The leg is walked backwards and planned on the tank's backward speed | Review: the rewrite had dropped w5's backward walk, which `ToCMainTankHoldAction::DragBossToAnchor` and the BT/Gruul drags share; his first swings land on the tank's front, and the chase timings in the doc were measured on backward drags | Forward walk at run speed (faster legs, back to him on arrival) |
| A bot parked on its dodge hold never walks to a sphere stand | Review: the in-reach test is made at duty time, so a drifting sphere could walk a parked shooter off its spot | Let the duty reassign first |
| `anubarak kiter needs protection` fires for paladins only | Review: only a paladin can hold the claim; the others paid the derivation and wrote a `none` row each | All bots (as integrated) |
| No design change from the second 2026-10-16 25N kill (`649_6_anub-arak_1792187557`, build 20:39 UTC, before this lane) | Its phase 1 repeats both findings: Angry pulled him and died at 13.7 s, the bot main tank taunted 15 times against the human's 4; 11 legs, 3 reversals | - |
| The leg returns true only on its issuing tick; `anubarak boss drag hold` blocks reach melee, set behind, tank face, formation, follow and charges while it is latched | The tank's threat buttons need the ticks between; every `MoveTo` clears the spline (pitfalls); Onyxia's walk | True every tick, no block |
| No drag until both tank patches are latched; with one or none he is held where the tank picks him up; the pre-submerge drag always runs | Brief item 2; until 9-12 s the anchor was the room centre, two drags | Room centre or a single patch as fallback |
| Submerge clearance 11.5 + 5.25 + 4 = 20.75 yd, a second search at 16.75 | Trace layout at 65 s: 273 floor points within 30 yd of the tank clear 20.75 (42.6% of the 35 yd disc); he submerged 16.2 yd from a patch | Start radius 8 in the sum, 24.75 (121 points) |
| DPS bots hold spells and melee on him, and area threat within 30 yd, from 90% of his tank victim's threat until under 80%, while he is up | Onyxia's threat hold; pitfalls "No bot has a threat brake"; Angry took him at 5.4 s and died | Opener timer only |
| Taunt guard on `Taunt` (Righteous Defense included) while the bot's current target is him and his victim is another tank, a human included; the hold node's own taunt off non-tanks is direct and unaffected | Thorim's guard; 11 taunts traded with the human tank in phase 1; `flags_extra` 0x80000 taunt DR | None |
| Two shooters per need, the nearest eligible to the sphere, latched while eligible; eligible: alive ranged DPS bot in the pit, unmarked, not walking a dodge; parked on a dodge only for a sphere in its reach and sight | Brief item 3; the lone shooter dodged 23 s and stood 55-72 yd off its sphere | Guid order, one shooter (current) |
| Drop guard: `drop target` zeroed while a shooter's current target is its flying duty sphere; the action walks into reach and sight | An untouched sphere is a critter out of combat, an invalid target; no duty shot all pull | A class-specific direct cast |
| Sphere guard: anything aimed at a sphere other than the bot's own duty sphere is zeroed, CC even on that one; AoE stays unguarded | Guide "Do not attack Frost Spheres unless explicitly told to"; six polymorphs; heroic's six | Stray pops allowed in normal (free patches, unplaced) |
| Phase 2 stock: 2 non-tank patches (heroic 1), falling spheres counted, one sphere at a time, nearest the room centre, over 16.75 yd from the spike (him before it spawns) and 9 yd off its lane; never in phase 1 | Brief "always one patch per remaining kite"; kites every 5.5 s, patches gone at 1:43; normal refills a slot within 4 s; the ring kite circles the centre; a phase 1 stock would crowd the submerge spot | Count remaining kites; stock in phase 1 |
| Dodge hold: one latched spot, walked up to 3 s then parked within 2.5 yd, held while the spike lives, the mark stays, the spot keeps 10 yd off the spike and 7 off the lane, and the spike is within 20 yd, at most 8 s; reach melee and reach spell blocked by name with the walk families; heroic spots keep out of Permafrost's 7.5 yd when one can | Brief item 4; the worm hold; 130/107 flips | Release on arrival (current) |
| Hand of Protection from the first able paladin bot by guid, unmarked, within 30 yd and sight, once the spike is within 12 yd of a kiter with no immunity or Forbearance, one claim per mark; without one, the marked bot casts Divine Shield, Hand of Protection or Ice Block on itself at 9 yd | Brief item 5; Impale is physical; 5 of 6 ring kites took Impales | The guide's stall under HoP outside a patch |
| The immune kiter keeps kiting; Ice Block is the last choice | A still kiter is dropped and the spike moves on at its speed | Stand still (guide) |
| Item 9 settled with no node | `--pull`: everyone in the pit at the pull | A regroup node |
| Items 6-8 and 10-12 carried over | Oversized-lane rule; phase 3 was clean (no deaths), Freezing Slash hit for 0.8-7.2k | - |
| The carried-over brief is `w5-anubarak-rest-rest.PLAN.md` | The overview's `<lane>-rest` naming | `-2`, as `refine-beasts-2` |

## Carried over

- To [w5-anubarak-rest-rest.PLAN.md](w5-anubarak-rest-rest.PLAN.md), for the merge stage to add a
  status row: phase 3 raid-health economy, Freezing Slash defensives, burrower pacing and ignoring
  burrowers when he is low, scarab threat and Mandible stacks, Spider Frenzy spacing, phase 2 raid
  positioning.
- To `w6-closeout`, unchanged from `w5-anubarak`: raid-wide `rti` reset after a kill, drop
  `ANUBARAK_PIT_CENTER`, hoist the direct-heal list into Shared.

## Known gaps

- Raid AoE still pops spheres (Starfall made every patch in the 2026-10-16 kill), which spends
  heroic's six; the sphere guard covers targeted actions only.
- A human kiter or tank reads none of this: no protection claim waits on a human paladin, and the
  drag waits while a human holds him.
- Ice Block holds the mage still, so the spike drops it and moves on at its current speed.
- A melee bot under the threat cap keeps white-swinging: `Attack()` starts auto-attack itself and
  the cap zeroes only actions.

## Blocked

None.
