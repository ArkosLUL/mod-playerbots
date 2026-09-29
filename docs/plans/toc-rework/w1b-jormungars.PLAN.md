# w1b-jormungars — Acidmaw and Dreadscale

Wave 4, after `w1a-beasts` merged its stage model. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Guide: `beasts-of-northrend-master-strategy-guide-toc-25/`.

## Scope

1. **Tanks.** Mobile/stationary tank split across the submerge form swap.
   - The worms wipe threat on every submerge (`boss_northrend_beasts.cpp`, the submerge handler), so
     use `RaidRedirectThreat` and re-establish the tanks on emerge.
   - Taunt ownership is not target ownership: gate on a second living tank (`docs/raids/ulduar/xt002.md`).
2. **Mechanics.**
   - Paralytic Toxin / Burning Bile pairing, where a burning player cures a toxin-slowed one per the
     guide; verify the actual mechanic in the script.
   - Slime pools: today `FleePosition`; move to hazard-clear stands.
   - The Sweep cone: take the shape from the DBC implicit target type. `HasInArc` and
     `IsBotInFrontalCone` take the full arc.
   - Spit/spray spread: a spread only helps once spacing exceeds the AoE radius.
3. **Kill timing.** Kill both worms within 10 s of each other, else enrage; add a DPS balance rule.
4. Verify the heroic differences and every Jormungar node `w0c-foundation` made live.
5. **Observability.** `nb.` probes (form, tank split, pairing); extend `northrend_beasts.py` and its
   test.

## Owns

Stem `Jormungars`; the Jormungars section of `northrend-beasts.md`; `northrend_beasts.py` and its
test.

## Task list

Verified mechanics (ids and damage per difficulty, timers, radii, the submerge, Toxin and Bile, the
first death) are in `docs/raids/trial-of-the-crusader/northrend-beasts.md`, "Acidmaw & Dreadscale";
read it first. Paths are under `src/Ai/Raid/ToC/` unless they start with `docs/` or `tools/`.

- **No registration seam changes.** The stem's headers are in the umbrellas, `ToCStrategy.cpp`
  already calls `AddToCJormungarsTriggerNodes` and `AddToCJormungarsMultipliers`, and the root
  contexts absorb the stem contexts. Trigger names keep the `northrend worms` prefix so
  `ToCGatedTrigger` gates them.
- Spell ids with a difficulty row are remapped at the call
  (`sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)`). New ids and constants go in
  `Util/ToCHelpers_Jormungars.h`.
- Worms come from the stage model (`Util/ToCHelpers_NorthrendBeasts.h`): `GetEngagedBeast`,
  `GetBeastsTankDuty`, `GetBeastsDutyHolder`, `GetBeastOfDuty`, `GetDutyOfBeast`. Never
  `GetFirstAliveUnitByEntry`: it loses a submerged worm (`NOT_SELECTABLE`).
- Per-instance state in a `RaidInstanceState` (`src/Ai/Raid/RaidInstanceState.h`), refreshed at most
  once per instance per `getMSTime()` ms, as `Util/ToCHelpers_NorthrendBeasts.cpp` does. Nothing
  resolves off map 649 or while `ToCEncounterIsLive(botAI, ToCEncounter::NorthrendBeasts)` is false.
- While `IcehowlChargeLatched` holds (heroic overlap), every mover here stands down: the charge guard
  owns movement.
- Probe keys are string literals on the line of the call, so `batch.py --probes` finds them.

### Contracts

Group 1 writes these in `Util/ToCHelpers_Jormungars.h` (namespace `TrialOfTheCrusaderHelpers`),
group 2 codes against them, the integrate step compiles them together.

```cpp
// 10N ids; remap at the call where a row exists
constexpr uint32 SPELL_PARALYTIC_TOXIN = 66823;   // row 605
constexpr uint32 SPELL_BURNING_BILE = 66869;      // no row
constexpr uint32 SPELL_ACIDIC_SPEW = 66818;       // no row: the cast and the worm's 2.5 s aura
constexpr uint32 SPELL_MOLTEN_SPEW = 66821;       // no row
constexpr uint32 SPELL_ACIDIC_SPEW_TICK = 66819;  // row 602
constexpr uint32 SPELL_MOLTEN_SPEW_TICK = 66820;  // row 611
constexpr uint32 SPELL_SLIME_POOL_AURA = 66882;   // no row, one tick a second grows the pool

constexpr float WORM_MELEE_RANGE = 9.3f;          // reach 6.5 + 1.5 + 4/3
constexpr float WORM_SPEW_RANGE = 55.0f;
constexpr float WORM_FLOOR_RADIUS = 35.0f;        // the worms surface up to 35 yd from ARENA_CENTER
constexpr float WORM_TAUNT_STAND = 20.0f;         // the mobile holder's reach under ground

bool IsWormSubmerged(Unit* worm);                 // UNIT_FLAG_NOT_SELECTABLE, submerge start to emerge
bool IsWormMobile(Unit* worm);                    // display form; under ground see Decisions
float WormSpewHalfArc(PlayerbotAI* botAI);        // radians, half the cone the remapped tick uses

struct WormDragPlan
{
    std::vector<EncounterHelpers::HazardCircle> hazards;    // pools and the stationary worm
    std::vector<EncounterHelpers::HazardCircle> poolsOnly;  // fallback sweep
    Position preferNear;                                     // 10 yd on from the tank, away from the worm
};
// The mobile worm this bot holds must be walked out: true fills the plan for the tank's own spot
bool GetMobileWormDrag(PlayerbotAI* botAI, Unit* worm, WormDragPlan& plan);

// The worm this hunter or rogue redirects onto its duty holder right now, else nullptr
Unit* GetWormRedirectTarget(PlayerbotAI* botAI, Player*& holder);
bool IsWormBileCarrierHeldOut(PlayerbotAI* botAI);  // Bile, no Beasts duty, not a runner

enum class WormMoveReason : uint8 { None, Cure, Run, Pool, Bile, Sweep, Spew, Spread };
char const* WormMoveReasonName(WormMoveReason reason);  // "cure", "run", "pool", ...
struct WormMovePlan
{
    WormMoveReason reason = WormMoveReason::None;
    bool urgent = false;          // damage landing on the bot's spot now: forced move, break a pinning cast
    Position preferNear;
    ObjectGuid partner;           // Cure: the Bile carrier; Run: the Toxin carrier
    std::vector<EncounterHelpers::HazardCircle> circles;        // every clearance that applies
    std::vector<EncounterHelpers::HazardCircle> urgentCircles;  // pools, Bile, Sweep while cast
    std::vector<EncounterHelpers::HazardCircle> escapeCircles;  // urgentCircles + a run's other Bile carriers
    std::function<bool(float, float)> accept;       // floor, Spew cones, partner reach
    std::function<bool(float, float)> escapeAccept; // accept without the partner reach
    std::function<bool(float, float)> roleAccept;   // reach of the bot's target, or of both holders
};
// Reason None when the bot's spot is fine. Trigger and action both read this.
bool GetWormMovePlan(PlayerbotAI* botAI, WormMovePlan& plan);
// urgentCircles, and accept (escapeAccept while urgent)
bool WormSpotStillSafe(WormMovePlan const& plan, Position const& spot);

// The reposition walk, shared by the action that issues it and the guard that protects it
void SetWormWalk(Player* bot, Position const& spot);
void ClearWormWalk(Player* bot);
bool GetWormWalk(Player* bot, Position& spot, uint32& issuedMs);
bool IsWormWalkInFlight(PlayerbotAI* botAI);  // latched, under 5 s old, moving, `last movement` on it
```

Probe keys, all emitting only on change:

| Key | Kind | Writer | Value |
|---|---|---|---|
| `nb.worm` | `ObsValue<uint8>`, per instance | 1 | 0 no worm engaged, 1 Dreadscale mobile, 2 Acidmaw mobile, 3 a worm under ground, 4 lone Dreadscale, 5 lone Acidmaw |
| `nb.cure` | `NoteDerived`, per bot | 1 | `seek <bile guid>`, `run <toxin guid>`, `wait <runner guid>`, `wait`, `none` |
| `nb.wormmove` | `NoteDerived`, per bot | 2 | `move <yd> <reason>`, `hold <reason>`, `clear`, `none <reason>`, `locked`, `stunned`; holders `approach <yd>`, `drag <yd>` |
| haz `wedge` | `NoteHazard(map, remapped Spew tick, worm pos, "wedge", "\"facing\":…,\"arc\":…,\"range\":55", 3500)`, once per Spew cast | 1 | the cone, `arc` in degrees |

### Group 1 — worm state, hazards, cure pairing

Files: `Util/ToCHelpers_Jormungars.h`, `Util/ToCHelpers_Jormungars.cpp`, `Util/ToCData.h` (the
Acidmaw & Dreadscale block only).

1. **Constants** as in the contracts, plus: `WORM_SWEEP_TRIGGER = 16`, `WORM_SWEEP_CLEARANCE = 18`
   (15 yd circle); `WORM_BILE_TRIGGER = 10.5`, `WORM_BILE_CLEARANCE = 12` (10 yd exact);
   `WORM_SPREAD_TRIGGER = 9`, `WORM_SPREAD_CLEARANCE = 11` (10 yd splash); `WORM_CURE_REACH = 6`,
   `WORM_CURE_TRIGGER = 8`; Spew cone pads 5° (trigger) and 12° (clear) past the half-arc; pool radius
   `2 + 0.3 × tick` capped at 11, trigger `radius + 1`, clearance `radiusSoon + 2`;
   `WORM_TANK_OFFSET = 8.5` (worm centre to its victim: the chase stops at contact 0.5 + reaches 6.5
   and 1.5); `WORM_TOXIN_STUCK_SLOW = -70`. In `ToCData.h`
   delete `SPELL_BURNING_BITE`, `SPELL_BURNING_SPRAY` and the comment above them (dead keys, pblint
   `--spell-difficulty` warns on them); keep `SPELL_SWEEP`.
2. **Form.** `IsWormSubmerged` reads `UNIT_FLAG_NOT_SELECTABLE`. `IsWormMobile` returns the display
   form; under ground the opposite of the other worm's display while that one is up, else flipped:
   every emerge flips it, and the stage model's `SplitWorms` then deals by the form the worm comes
   up in, with no edit there. `WormSpewHalfArc`: half of
   `sSpellMgr->GetSpellCone(remapped SPELL_ACIDIC_SPEW_TICK)->cone_degrees`, else 12°.
3. **State**, per instance, refreshed once per ms while a worm is engaged:
   - both worms by `GetEngagedBeast`: submerged, mobile, spewing (casting or carrying
     `SPELL_ACIDIC_SPEW`/`SPELL_MOLTEN_SPEW`), casting Sweep;
   - pools: a bot-anchored 200 yd `GetCreatureListWithEntryInGrid(NPC_SLIME_POOL)` at most every
     500 ms, guids cached, resolved live; radius from
     `GetAura(SPELL_SLIME_POOL_AURA)->GetEffect(EFFECT_0)->GetTickNumber()`;
   - roster (living group members in the instance): Toxin carriers with their snare
     (`GetEffect(EFFECT_0)->GetAmount()` of the remapped aura), Bile carriers;
   - stuck Toxin carriers: a worm duty holder, or snare ≤ `WORM_TOXIN_STUCK_SLOW`. Runners: Bile
     carriers not holding a worm duty, or holding one whose worm is submerged. Stuck carriers by guid
     each take the nearest unassigned runner (3D, then guid); a runner already within
     `WORM_CURE_TRIGGER` of a stuck carrier serves that one;
   - writes `nb.worm`, and the `wedge` haz row once per Spew cast (latched per worm while it casts).
4. (Dropped: `GetSlimePools` had no caller; see Decisions.)
5. **`GetMobileWormDrag`**, only for the `WormMobile` holder while it is its worm's victim and the
   worm is up: true when the worm stands within `radiusSoon + WORM_MELEE_RANGE` of a pool, or within
   `WORM_SWEEP_CLEARANCE + WORM_MELEE_RANGE` of the stationary worm. Hazards for the tank's spot: pools
   at `11 + WORM_MELEE_RANGE + WORM_TANK_OFFSET + 2`, the stationary worm at
   `WORM_SWEEP_CLEARANCE + WORM_MELEE_RANGE + WORM_TANK_OFFSET + 2`; `poolsOnly` the first set.
6. **`GetWormRedirectTarget`**: a living hunter or rogue. Rogues serve `WormMobile`; hunters by
   index among the group's living hunters in group order, even `WormMobile`, odd `WormStationary`,
   falling back to `WormMobile` without one. Fires while that duty's holder is alive, not this bot,
   and either the worm is submerged, its victim is not the holder, or a hunter holds
   `SPELL_MISDIRECTION_PROC`.
7. **`GetWormMovePlan`**, a pure read. None for a dead bot, a worm duty holder (unless it is a
   runner), no engaged worm, or a latched Icehowl charge. Otherwise, first match:
   - **Run**: an assigned runner farther than `WORM_CURE_TRIGGER` from its carrier.
   - **Cure**: a Toxin carrier, not stuck and not carrying Bile, farther than `WORM_CURE_TRIGGER` from
     the nearest Bile carrier (3D).
   - For Run and Cure, `accept` adds "within `WORM_CURE_REACH` of the partner" and `preferNear` is the
     partner. Circles: pools, and Sweep for anyone but the `WormStationary` holder; no Bile circles.
   - **Hazards** otherwise, reason the first trigger the bot's spot fails, in this order:
     - Pool: every pool, for everyone.
     - Bile: every other Bile carrier, for a bot not carrying Toxin; a Bile carrier (not a runner)
       also keeps off every other living member except Toxin carriers.
     - Sweep: a stationary worm that is up, for all but its holder.
     - Spew: a mobile worm that is up, for everyone (holders are already out): trigger inside the
       cone plus 5°, `accept` outside it plus 12°.
     - Spread: ranged DPS and healers (`IsRangedDps`, `IsHeal`) against every other living member,
       while a stationary worm is engaged and both worms live.
   - Casters also clear Gormok's centre by `GORMOK_STOMP_CLEARANCE` while he is engaged (circle only,
     no trigger).
   - `urgent`: inside a pool's `radius`, within `WORM_SPLASH_RADIUS` (10) of a Bile carrier, inside a
     Sweep being cast, or inside the cone of a worm spewing.
   - `accept`: within `WORM_FLOOR_RADIUS` of `ARENA_CENTER` and outside every up mobile worm's cone
     plus 12°. `roleAccept`: melee DPS within `WORM_MELEE_RANGE - 1` of their current target when it
     is a worm; ranged DPS within `GetRange("spell")` of theirs; healers within `GetRange("heal")` of
     each living worm duty holder; tanks none. `preferNear`: melee, 6 yd behind their worm (against
     its orientation); otherwise the bot.
   - Writes `nb.cure` for this bot: `seek`, `run`, `wait <runner>` or `wait` for a stuck carrier,
     else `none`.
8. **Walk latch** per bot in the per-instance state, keyed by bot guid; `IsWormWalkInFlight` as in
   the contracts (`last movement` within 0.5 yd of the spot).

### Group 2 — nodes and multipliers

Files: `Trigger/ToCTriggers_Jormungars.h`, `Trigger/ToCTriggers_Jormungars.cpp`,
`Action/ToCActions_Jormungars.h`, `Action/ToCActions_Jormungars.cpp`,
`Multiplier/ToCMultipliers_Jormungars.h`, `Multiplier/ToCMultipliers_Jormungars.cpp` (new; the
header's inline no-op becomes a declaration).

9. **Hold triggers**, renamed, `IsActive` unchanged (the duty): `northrend worms mobile tank duty`
   (was `… mobile engaged by main tank`) → `northrend worms tank hold mobile worm` (was `… main tank
   hold mobile worm`); `northrend worms stationary tank duty` (was `… stationary needs assist tank`)
   → `northrend worms tank hold stationary worm` (was `… assist tank hold stationary worm`). Both
   `ACTION_RAID + 1`.
10. **Hold actions**, one base for both, the worm from `GetBeastOfDuty` (drops `FindWorm`):
    - mobile: skull (`MarkTargetWithSkull`) only while Gormok isn't engaged;
      `SetRtiTarget(botAI, "skull", worm)` always. Stationary: no mark.
    - submerged: walk toward the worm to `WORM_TAUNT_STAND` (mobile) or `WORM_MELEE_RANGE - 2`
      (stationary), `MOVEMENT_COMBAT`, re-issued only once the worm stands 3 yd off the last goal (it
      moves once, 2.5 s in); true while walking; `nb.wormmove` `approach <yd>`.
    - up: its victim isn't this bot → `CastClassTaunt`, true if cast; then `Attack` when it isn't the
      current target.
    - mobile, with no Icehowl charge latched: `GetMobileWormDrag` →
      `FindNearestPositionClearOfHazards` over `hazards`, then `poolsOnly`, 30 yd, `preferNear`,
      `accept` within `WORM_FLOOR_RADIUS` of `ARENA_CENTER`; latched walk at `MOVEMENT_COMBAT`, held
      while in flight (the `GormokWalkAction` pattern); `nb.wormmove` `drag <yd>`.
11. **Redirect**: trigger `northrend worms redirect threat` (`GetWormRedirectTarget` non-null) →
    action `northrend worms redirect threat`, a `RaidRedirectThreatAction` whose tank is the holder
    and dump target the worm, `ACTION_RAID + 1`. Misdirection and Tricks go out under ground; the
    shots wait for the worm to come up.
12. **Reposition**: trigger `northrend worms misplaced` (`GetWormMovePlan` reason not None) → action
    `northrend worms reposition`, `ACTION_EMERGENCY + 6`, shaped like `IcehowlClearChargePathAction`:
    - plan None → `ClearWormWalk`, false. `!botAI->CanMove()` → `stunned`, false.
    - a latched walk under 5 s old whose spot `WormSpotStillSafe`: arrived within 1 yd → `clear`,
      `ClearWormWalk`, false; booked on it and moving, or under 500 ms old → `hold <reason>`, true;
      stopped past 500 ms → clear `last movement` before re-issuing.
    - sweeps with one `HazardSweepCache`: `circles` with `accept && roleAccept`, then `circles` with
      `accept`, then, if `urgent`, `urgentCircles` with `accept`. None → `none <reason>`, return
      `urgent`.
    - urgent and `IsMovementPreventedByCasting` → `InterruptNonMeleeSpells(true)` first.
    - `TryMoveTo` at `MOVEMENT_FORCED` when urgent or Cure/Run, else `MOVEMENT_COMBAT`: issued →
      `SetWormWalk`, `move <yd> <reason>`, true; already there → `clear`, false; else `locked`,
      return `urgent`.
13. **Delete** `northrend worms ranged should spread`/`spread`, `… afflicted by burning`/`keep
    moving`, `… slime pool nearby`/`avoid slime pool`, `… sweep frontal`/`avoid sweep`, their eight
    classes and `FindWorm`. Node order: the two holds, redirect, reposition.
14. **Multipliers**, registered in `AddToCJormungarsMultipliers`:
    - `NorthrendWormsMoveGuardMultiplier` ("northrend worms move guard"): test the action first (a
      `MovementAction` other than `AttackAction`, a `CastReachTargetSpellAction`, or a
      `CastSpellAction` named `blink`/`disengage`), then `IsWormWalkInFlight`; zero the spell movers,
      spare `avoid aoe` and every Beasts-prefixed action (`ToCEncounterOfTrigger`), zero the rest.
    - `NorthrendWormsBileReachGuardMultiplier` ("northrend worms bile reach guard"): zero
      `ReachMeleeAction` and `CastReachTargetSpellAction` while `IsWormBileCarrierHeldOut`.
    - `NorthrendWormsRedirectGuardMultiplier` ("northrend worms redirect guard"): hunters and rogues;
      zero `CastMisdirectionOnMainTankAction`, `CastTricksOfTheTradeOnMainTankAction` and
      `CastTricksOfTheTradeAction` while the encounter is live and a worm is engaged
      (`TwinValkyrRedirectGuardMultiplier`'s shape).

### Group 3 — reader, test, doc

Files: `tools/botobs/bosses/northrend_beasts.py`, `tools/botobs/tests/test_northrend_beasts.py`,
`docs/raids/trial-of-the-crusader/northrend-beasts.md`.

`w1a-beasts-rest` edits these three files in the same wave: add new functions, `SECTIONS` rows and
test classes after the existing ones, and keep doc edits inside the worm sections and the worm rows
of "What a trace answers".

15. **Reader**, two sections in the `general_vezax.py` style:
    - `--worms`: `nb.worm` spans with deaths per span; per emerge (`nb.worm` leaving 3, and the first
      engage) seconds until each worm's snapshot target is a tank, naming the non-tank victims
      meanwhile; per `wedge` haz row the non-tank roster inside it at the nearest snapshot, and Spew
      tick `dmg` victims by role in the next 3.5 s; per pool (entry 35176, from its first snapshot,
      radius `2 + 0.3 × whole seconds`) the bot-seconds inside and pool `dmg` by victim; Sweep `dmg`
      victims by role; `nb.wormmove` branches per bot, numbers dropped.
    - `--cure`: Paralytic Toxin `aura` spans per carrier (all four ids), each ended `cured` (a Bile
      pulse `dmg` row on the carrier within 250 ms of the removal), `died`, `expired` (59 s or more)
      or `other`, with seconds carried and `stuck` past 18 s (13.5 s on `hdr.diff` 2 and 3); Burning
      Bile 66869 spans per carrier with its pulse `dmg` victims that carried no Toxin; `nb.cure`
      branches per bot.
    - Ids per difficulty from the doc's table. Extend the module docstring.
16. **Test**, a second synthetic pull in its own test class: an emerge with a healer victim 2 s then
    the tank; a `wedge` row with a ranged bot inside and one Spew tick on it; a pool with a bot inside
    past 10 s; a Toxin cured by a pulse row; a Toxin kept 60 s (`expired`, `stuck`); a pulse on a
    melee carrying nothing. Every new section reads empty on `fixtures/full-v12.ndjson`.
17. **Doc**, folded with `compact-docs-writer` after the other groups land: in "Stage model and tank
    duties" the worm form flipped while submerged; under "Acidmaw & Dreadscale" a "What the bots do"
    covering tasks 9-14 and the decisions below with their sources; Known gaps (drop the two worm
    gaps this closes, add this brief's); "What a trace answers" rows for `worm`, `cure`, `wormmove`
    and the `wedge` row, plus `--worms` and `--cure` usage. Keep the mechanics as written unless the
    code proves one wrong.

### Integrate

Compile the contracts together, re-run CMake (new `Multiplier/ToCMultipliers_Jormungars.cpp`), run
the checks per the overview. pblint must resolve every renamed node; nothing may still name
`SPELL_BURNING_BITE`, `SPELL_BURNING_SPRAY`, `FindWorm` or the deleted classes.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Kill target = skull on the mobile worm: Dreadscale while he is mobile, Acidmaw once they swap, back again on the next swap | guide ("kill Dreadscale first … consider killing Acidmaw first if your raid team is unable to kill Dreadscale before they submerge"); script: Sweep is a 15 yd circle every melee on a stationary worm eats. Health 1.26M / 5.02M / 1.67M / 6.69M before scaling, so a 25-man bot raid rarely kills Dreadscale in the first 45-50 s | Dreadscale whatever his form (melee in Sweep after a swap) |
| No DPS balance rule (brief task 3) | script: the survivor enrages the moment the first worm dies (`DoAction(-2)`); the 10 s (`AchievementTimer`) only gates the achievement. Following the mobile worm spreads damage over both anyway | balance health to die within 10 s, which puts melee on a stationary worm half the time |
| A submerged worm counts as the form it comes up in once both are under; while the other is up, as the opposite of that one's display. In `IsWormMobile`, so `SplitWorms` stays unedited | script: every emerge flips the form, the display changes only at emerge, the two submerge and emerge 1.5 s apart and always hold opposite forms; review: flipping each worm at its own submerge tied `SplitWorms`, which relabelled the duties at the first submerge in one phase and the second in the other | display form, which sends each holder to the wrong worm for 8.5 s; the same rule in `SplitWorms` (NorthrendBeasts stem, outside this lane) |
| Duties across a submerge stay with the stage model's deal (victim, then roster) | script: `ResetAllThreat` keeps the old victim until someone gains threat, and a stunned worm takes no heal assist threat, so pass 1 keeps a tank on its worm unless a DoT took it | a per-worm tank latch in the `NorthrendBeasts` stem |
| A holder taunts its worm whenever it isn't the victim, not only off a non-tank | no swap duty on the worms; pass 1 gives a tank the worm it already holds, so a holder never taunts another holder's worm | Gormok's rule, which lets one tank keep both worms |
| A lone living tank holds only the mobile worm; the stationary one is left to its threat | deal order has nothing below `WormStationary`; guide ("can be tanked by any ranged DPS class"); brief (gate taunts on a second living tank) | the lone tank taunting both |
| Under ground the mobile holder closes to 20 yd and taunts; the stationary holder walks into melee of the spot | `xt002.md` (taunt ownership: the add walks to the tank); the stationary worm is rooted | both walk into melee |
| Hunters by index among hunters alternate between the two holders, rogues always serve `WormMobile`, casting from the submerge on; a hunter holding Misdirection charges keeps the node until they're spent on its worm; generic MT redirects vetoed while a worm is engaged | README threat-redirect rubric (target not the MT, moment not the pull); `TwinValkyrRedirectGuardMultiplier`; review: `ThreatManager` redirects Tricks off whatever the rogue hits, and melee hit the skull | rogues alternating too (odd rogues handed the stationary holder threat on the mobile worm) |
| Skull on the mobile worm only while Gormok isn't engaged, RTI always; no cross on the stationary worm | `w1a-beasts` carry-over; pitfalls (a mark never clears); nothing reads the cross | mark every tick (trades skull with Gormok's hold on heroic) |
| One reposition node per bot for pools, Spew, Sweep, Bile, spray spread and the cure walk | `xt002.md` (one mover per bot); pitfalls (two movers clear each other's MotionMaster) | a node per hazard (four `FleePosition` nodes today) |
| Non-tanks stay out of the mobile worm's cone all the time: trigger 5° past the half-arc, clear 12° past | guide ("avoid standing in front of the mobile worm"); script: a 1 s cast then 2.5 s of ticks is too short to leave a 55 yd cone on the cast | dodge only while he spews |
| The cone is read from `GetSpellCone` on the remapped tick, else 24° | `Spell::SelectImplicitConeTargets`: `spell_cone` 60° on the 10N ids only, `TARGET_UNIT_CONE_ENEMY_24` elsewhere | 60° everywhere |
| Sweep: non-holders keep 18 yd off a stationary worm, trigger inside 16 | DBC: 15 yd `TARGET_UNIT_SRC_AREA_ENEMY` circle, centre to centre | the old 90° frontal cone |
| Pool clearance = radius 5 s on + 2, trigger at radius now + 1 | `SpellAuraEffects.cpp`: radius `2 + 0.3 × tick`; pitfalls (clear past the trigger; park tolerance) | the full 11 yd from spawn, which clears melee off the worm for 30 s |
| The mobile holder walks the worm about 31 yd off a pool, or 38 off the stationary worm, to the clear spot nearest 10 yd ahead of itself; both clearances padded 2 yd | guide ("slowly drag the boss away from the Slime Pool so melee can safely attack"); walking on keeps his front pointing the same way; review: without the pad the worm settled 26.8-28.8 yd off the stationary worm, inside the 27.3 trigger | step-by-step drag; no drag |
| Bile: bots without Toxin keep 12 yd off every Bile carrier; a Bile carrier keeps 12 yd off everyone but Toxin carriers | script: 10 yd exact pulse on every ally, carrier included; guide ("immediately spread out from all nearby players, even if you are melee") | eat the pulse |
| A Toxin carrier walks to within 6 yd of the nearest Bile carrier, trigger past 8 | guide; script: the pulse strips Toxin at 10 yd exact | 10 yd with no margin |
| A stuck Toxin carrier (a holder, or snared 70% or more) gets a runner: the nearest free Bile carrier, a holder only while its worm is under ground | guide ("be ready to free them while the worms are submerged"); script: each Bite restarts the snare, so a mobile Acidmaw's tank is never cured otherwise | let it wait out 60 s |
| Spray spread: ranged DPS and healers keep 11 yd off every player while both worms live, trigger inside 9 | script: 10 yd splash, centre to centre; pitfalls (a spread only helps past the radius); guide (`/range 10`) | the old 8 yd `FleePosition` spread; the guide's "one neighbour is fine" |
| Derived spots stay within 35 yd of `ARENA_CENTER`; no coordinates proposed | script: the worms surface up to 35 yd out; navprobe can't see the gameobject floor (README) | no bound |
| `MOVEMENT_FORCED` only while damage lands on the spot or on a cure walk, else `MOVEMENT_COMBAT`; a pinning cast broken only when urgent | pitfalls (frequency decides forced ties; interrupt per mechanic) | always forced |
| While a reposition walk is in flight, other movers are zeroed except attacks, `avoid aoe` and Beasts-prefixed nodes; blink/disengage too | `GormokSnoboldCarrierMultiplier` pattern; pitfalls (every `MoveTo` clears the MotionMaster; spell movers) | no guard |
| Casters' spots also clear Gormok's stomp while he is engaged | heroic overlap; one mover per bot | leave it to Gormok's node, and let the two alternate |
| `SPELL_BURNING_BITE`/`SPELL_BURNING_SPRAY` deleted from `ToCData.h` | `w0c-foundation` carry-over (dead keys, pblint warning) | reword the comment |
| No `-rest` brief | every item hurts on every difficulty: the cone and the threat wipe every cycle, Toxin, Bile and pools every 20-30 s | carry the spray spread or the runners over |
| Worm form from the native display id; a worm under ground carrying Enrage 68335 reads mobile | script: Submerge 53421's transform shows creature 24417's model 22452, neither form; `DoAction(-2)` sends the survivor up mobile | `GetDisplayId`, blind under ground |
| Runner pairing keeps last ms's pair while both still qualify, between "runner within 8 yd" and "nearest free runner" | conservative: two runners closing on two carriers would trade carriers every ms | re-pick by distance every ms |
| Only bots are runners | conservative: a human never answers the assignment and leaves the stuck carrier waiting | any Bile carrier |
| "A Bile carrier (not a runner)" reads as not an assigned runner | every non-holder Bile carrier is eligible, so "not eligible" would switch the rule off | not eligible |
| Spread skips a Toxin carrier next to a Bile carrier, either way round | `xt002.md` (one mover per bot): the spread and the cure walk would take turns moving the bot | spread from every player |
| Gormok's stomp circle only in casters' hazard plans, never on a cure walk or for a snobold carrier | brief (the cure walk's circles are pools and Sweep); `GetGormokForSnoboldCarrier` keeps carriers in his melee | every plan |
| Bile urgency only for a bot without Toxin within 10 yd of another carrier; a carrier's circles round others are not urgent | script (Toxin carriers want the pulse); passers-by would invalidate the carrier's latched spot | urgent for everyone within 10 yd |
| Every plan's `circles` carries a 1 yd circle at the bot's feet, `urgentCircles` none; an urgent sweep with no urgent circle uses a 0 yd one | `FindNearestPositionClearOfHazards` returns nothing for an empty list, and its collision check can pull a spot back onto the bot | a minimum travel in `accept` |
| Sweep urgency on the real 15 yd; Spew urgency on the unpadded cone, only while that worm spews | DBC radius, `spell_cone`; the pads are for choosing spots, not for forcing moves | urgency on the clearances |
| The drag's "stationary worm" is the other worm only while up in stationary form; no drag while an Icehowl charge is latched | brief (every mover stands down under the charge) | also a submerged stationary worm |
| The submerged approach also stands down while an Icehowl charge is latched | brief; the charge guard spares `AttackAction` subclasses, so the hold has to check itself | rely on the guard |
| The drag walk is held in flight only while its spot stays clear of `poolsOnly` | conservative: a pool dropped on the spot mid walk re-plans at once | hold regardless |
| Hold walks clear their own `last movement` booking before re-issuing | pitfalls (a single mover is refused its own re-target) | wait out the booking |
| A non-urgent reposition waits out a pinning cast (`locked`, before the sweeps) | pitfalls (a bot mid-cast can't be moved; interrupt per mechanic) | interrupt every cast |
| A cure or run walk whose sweeps all miss walks straight to 5 yd of the partner, if `WormSpotStillSafe` | the sweep's rays fan 22.5° apart, so past about 30 yd they can straddle the partner's 6 yd disc | report `none` and wait |
| `nb.worm` writes 0 while the encounter is live with no worm engaged, off it only the drop to 0; `nb.cure` writes `none` only while live; `nb.wormmove` yards are the rounded 2D travel | observability (emit on change, no rows off the encounter); the `dodge` convention | write every tick |
| Cure and run spots, and a pair within 8 yd's Spew trigger, use the unpadded cone | review: on 10N the 60° cone + 12° covers the whole 6 yd disc round a mobile worm's tank (8.5 yd ahead of it), so nobody was cured while a worm was up; urgency still covers a Spew being cast | the padded cone everywhere |
| An urgent escape takes `escapeCircles` and `escapeAccept` (no partner reach; a run also clears the other Bile carriers); a latched spot is judged by `escapeAccept` while urgent | review: a cure walker in a pool or spewing cone could only escape into the partner's disc, else stood still; judging the latched escape spot by `accept` would re-issue it every tick | escape only near the partner |
| `northrend worms bile reach guard` zeroes `ReachMeleeAction` and `CastReachTargetSpellAction` for a Bile carrier with no Beasts tank duty that isn't a runner; the carrier-side Bile rule also skips any Beasts duty holder (a Gormok tank in a heroic overlap stays on Gormok) | guide ("spread out from all nearby players, even if you are melee"); pitfalls (a "too close" rule including melee needs the reach held, the Hodir case); script: the pulse is up to 9.7k per 2 s on everyone within 10 yd | leave melee out of the carrier rule and let the stack eat the pulses |
| A melee whose current target is live and not a worm triggers Sweep only while it's cast; melee `roleAccept` takes any live target's melee range | review: in a heroic overlap a melee on Gormok near a surfaced stationary worm was walked out and `reach melee` walked it back | keep melee on Gormok out of the 16 yd always |
| A bot mid hard cast in an urgent zone gets `RequestSpellInterrupt` from the per-instance read; no per-cast latch | pitfalls (a bot mid-cast runs no triggers); `RequestSpellInterrupt` is idempotent and `UpdateAI` clears a stale request | channels only, as before |
| The hold's taunt is gated on `CanCastSpell` plus spell range and LOS, a local copy of Anub'arak's `TryClassTaunt` | action-selection.md: `CastSpell` faces the target before failing | hoist into `ToCActions_Shared` now (touches the Anub'arak stem; carried over) |
| The move guard also zeroes `CastReachTargetSpellAction` (charge, Intercept, Feral Charge) | README encounter rules; `ToCMultipliers_Icehowl.cpp` `CompetesWithChargeDodge` | blink/disengage only |
| `--worms` times each worm from its own Emerge cast 66947 in the snapshot casting column, falling back to `nb.worm` leaving 3 | review: `nb.worm` leaves 3 at the later worm's emerge, up to 1.5 s after the first | document the blind spot |
| `GetWormCastingSweep`, `GetSlimePools` and `IsInWormSpewCone` deleted, `WormPool` moved into the .cpp | review: no caller after the old triggers went | keep them in the contract |
| `--cure` ends a Toxin span `open` when the file ends first; a death within 250 ms of the removal beats a cure; deaths from `combat_deaths`, so a wipe-command kill isn't `died` | the four labels can't describe an unfinished span; a death also removes the aura | `other` |
| Bile "clean" victims leave out the carrier's own pulse hits; pool `dmg` ties to a pool by `dmg.s`, hits from an unsampled pool listed apart | the carrier can't avoid its own pulse; a pool first seen after its hits has no snapshot radius | count them |

Nodes `w0c-foundation` made live: `northrend worms sweep frontal` reads the remapped Sweep correctly
on all four (the script casts 66794, the `Spell` constructor remaps it), but its cone shape is wrong;
task 13 replaces it.

## Carried over

- Merge stage, `docs/raids/trial-of-the-crusader/README.md`: the node table's `northrend worms sweep
  frontal` and `… afflicted by burning` rows become `northrend worms misplaced` | Paralytic Toxin
  (four ids), Burning Bile 66869, the Sweep cast, the Spew ticks' cone; the Code layout line keeping
  `AvoidCreatureClusterAction` "for its one caller (slime pools)" loses that caller.
- `w6-closeout`: delete `AvoidCreatureClusterAction`, `GetCreatureClusterCenter` and
  `GetNearestCreatureByEntry` (no callers left); hoist the spell-mover test (`IsBlinkOrDisengage`
  in Gormok, `IsSpellMover` in Jormungars, `CompetesWithChargeDodge` in Icehowl) into `Shared`,
  with `CastReachTargetSpellAction` in it; hoist `TryClassTaunt` (Anub'arak and Jormungars copies)
  into `ToCActions_Shared`.
- Merge stage or `w6-closeout`: `tools/botobs/tests/test_toc_naming.py:59,200` sample the removed
  `northrend worms sweep frontal`; passes, only the name is stale.
- Merge stage, engine lessons for `docs/engine/pitfalls.md`:
  - a hardcoded aura handler can scale a trigger's radius every tick (`SpellAuraEffects.cpp` casts
    Slime Pool 66881 and Grobbulus' Poison with a growing `SPELLVALUE_RADIUS_MOD`), so neither the DBC
    nor `SpellInfoCorrections.cpp` has the live radius;
  - a re-applied non-stacking aura recalculates its amounts (`Aura::SetStackAmount`), so a ramp built
    in a periodic dummy restarts on every refresh;
  - a stunned creature takes no heal assist threat (`ThreatManager::ForwardThreatForAssistingMe`
    skips `UNIT_STATE_CONTROLLED`).
- Merge stage: `w1a-beasts-rest` edits the same doc, reader and test in this wave.
- User: re-run CMake before building (new `Multiplier/ToCMultipliers_Jormungars.cpp`).

## Known gaps

- No Hand of Freedom or defensive for a snared tank, and no raid cooldowns when a Spray lands on
  several players.
- Melee still stack, so a Spray on one of them splashes the rest.
- DoTs are not refreshed on both worms before a submerge (guide).
- The achievement (both worms within 10 s) is not pursued.
- Healer reach to both holders is a preference; with the worms on opposite sides the spot falls back
  to safety alone.
- A hunter that can't Steady Shot (moving, or too close) leaves held Misdirection charges to its
  rotation, which can spend them on the skull and hand the `WormStationary` holder threat on the
  mobile worm; the mobile holder re-taunts, at the cost of taunt diminishing returns.

## Review

Open issues after re-review: none. Deferred findings:

- conformance-7, the optional part: hoisting `TryClassTaunt` into `ToCActions_Shared` touches the
  Anub'arak stem, so it is carried over to `w6-closeout` (above); the gated taunt is applied locally.
- mechanics-6, `SplitWorms`: left unedited on purpose; the fix sits in `IsWormMobile` in this stem
  (Decisions, submerged form). Nothing carried over.

## Blocked
