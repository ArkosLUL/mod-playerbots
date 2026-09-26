# w1a-beasts — stage model, Gormok, Icehowl

Wave 3. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). The
code layout and gate are in `docs/raids/trial-of-the-crusader/README.md`. Guide:
`beasts-of-northrend-master-strategy-guide-toc-25/`.

## Scope

1. **Stage model.**
   - Which beast is active, and tank assignment across all three stages.
   - Heroic runs on timers, not kills (worms at 150 s, Icehowl at 340 s, enrage at 520 s; verify in
     the instance script), so beasts overlap. One tank model must cover that, for 10- and 25-man
     tank counts.
   - `w1b-jormungars` builds on this model, so expose it from the `NorthrendBeasts` stem.
2. **Gormok.**
   - Impale tank swap (today at 3 stacks, `GORMOK_IMPALE_SWAP_STACKS`; check the Impale id per
     difficulty).
   - Snobolds: Snobolled players, Snobold Vassals. The guide has a Snobold-to-melee tactic.
   - Fire Bomb ground fire, and Staggering Stomp (interrupting cast).
   - `RaidTankDefensive` if the swap fails at high Impale stacks.
3. **Icehowl.**
   - Rebuild the charge: today it uses `FleePosition` plus a multiplier vetoing every
     `MovementAction`, which also vetoes `AttackAction`. Use hazard-clear stands from
     `FindNearestPositionClearOfHazards` and forced dodges, and settle two forced dodges in a
     multiplier.
   - Probe the charge corridor with `NoteHazard`, since it has no world object.
   - Arctic Breath and Whirl.
   - The wall-crash stun as a burn window.
   - A `RaidTankDefensive` for Frothing Rage after a missed charge.
4. **Lust row.** Today lust fires on Gormok's pull and is wasted. Pick the window per the guide and
   the script.
5. **Observability.** `nb.` probes (stage, tank assignment, charge decisions), a
   `tools/botobs/bosses/northrend_beasts.py` reader, and a synthetic-pull test.
6. Verify every Beasts node that `w0c-foundation` made live in 25N/10H/25H.

## Owns

Stems `Gormok`, `Icehowl`, `NorthrendBeasts`; `docs/raids/trial-of-the-crusader/northrend-beasts.md`;
the reader and its test. Not the `Jormungars` stem, except the stage-model interface it consumes.

## Task list

Verified mechanics (ids per difficulty, timers, charge geometry, the arena floor) are in
`docs/raids/trial-of-the-crusader/northrend-beasts.md`; read it first. All paths below are under
`src/Ai/Raid/ToC/` unless they start with `docs/` or `tools/`. No registration seam changes: every
stem already has its header in the umbrellas, its `AddToC<Stem>…` call in `ToCStrategy.cpp` and its
context in the root contexts. Trigger names keep a Beasts prefix (`gormok`, `icehowl`, `northrend
worms`) so `ToCGatedTrigger` gates them. Spell ids with a difficulty row are remapped at the call
(`sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)`); new ids go in the stem's `Util/ToCHelpers_<Stem>.h`,
never `Util/ToCData.h` (other wave-3 lanes edit it). Relevances in a task are the node's; keep the
existing ones where a task names none.

The groups share these contracts; code against them, the integrate step compiles them together.

- **Stage model** (group 1 writes, groups 2 and 3 read), `Util/ToCHelpers_NorthrendBeasts.h`,
  namespace `TrialOfTheCrusaderHelpers`:
  ```cpp
  enum class NorthrendBeast : uint8 { Gormok, Acidmaw, Dreadscale, Icehowl };
  // Deal order after None is the priority order of task 2.
  enum class BeastsTankDuty : uint8 { None, Gormok, Icehowl, WormMobile, GormokSwap, WormStationary };
  Unit* GetEngagedBeast(PlayerbotAI* botAI, NorthrendBeast beast);   // alive and in combat, else nullptr
  uint32 GetBeastsStageMask(PlayerbotAI* botAI);                      // 1 Gormok, 2 a worm, 4 Icehowl
  bool IsBeastsTank(Player* player);                                  // IsTank(p) || IsTank(p, true)
  BeastsTankDuty GetBeastsTankDuty(PlayerbotAI* botAI);              // this bot's duty
  Player* GetBeastsDutyHolder(PlayerbotAI* botAI, BeastsTankDuty duty);
  Unit* GetBeastOfDuty(PlayerbotAI* botAI, BeastsTankDuty duty);     // GormokSwap answers Gormok
  BeastsTankDuty GetDutyOfBeast(PlayerbotAI* botAI, Unit* beast);    // None for a non-beast
  ```
- **Icehowl daze** (group 3 writes, group 1 reads), `Util/ToCHelpers_Icehowl.h`:
  `constexpr uint32 SPELL_STAGGERED_DAZE = 66758;` and `bool IsIcehowlStaggered(Unit* icehowl);`.
- **Probe keys** (each writer as named; group 1 documents all of them and reads them in the reader).
  All emit only on change.

  | Key | Kind | Writer | Value |
  |---|---|---|---|
  | `nb.stage` | `ObsValue<uint32>`, per instance | 1 | the stage mask |
  | `nb.tank` | `NoteDerived`, per tank | 1 | `gormok`, `swap`, `wormmobile`, `wormstationary`, `icehowl` or `none`, then the beast's guid (`DescribeAssignment`) |
  | `nb.swap` | `NoteDerived`, per `GormokSwap` tank | 2 | `go v=<holder stacks>`, `wait mine=<n>`, `wait v=<n>` |
  | `nb.snobold` | `NoteDerived`, per DPS | 2 | `<rider guid> heal\|ranged\|melee\|tank`, or `none` |
  | `nb.defensive` | `NextTankDefensive` note kind | 2, 3 | the cooldown, `covered` or `none` |
  | `nb.charge` | `ObsValue<uint8>`, per instance | 3 | 0 none, 1 crash, 2 gaze, 3 charge, 4 daze, 5 rage |
  | `nb.gaze` | `ObsValue<ObjectGuid>`, per instance | 3 | the gaze target |
  | `nb.dodge` | `NoteDerived`, per bot | 3 | `move <yd>`, `hold`, `tight`, `clear`, `stunned`, `locked`, `none` |
  | haz `lane` | `NoteHazard(map, 66734, P_jb, "lane", "\"ex\":…,\"ey\":…,\"half\":12", 4000)`, once per latch | 3 | the charge path |

### Group 1 — stage model, shared multipliers, reader, doc

Files: `Util/ToCHelpers_NorthrendBeasts.h` (new), `Util/ToCHelpers_NorthrendBeasts.cpp` (new),
`Multiplier/ToCMultipliers_NorthrendBeasts.h`, `Multiplier/ToCMultipliers_NorthrendBeasts.cpp`,
`Trigger/ToCTriggers_Jormungars.cpp`, `docs/raids/trial-of-the-crusader/northrend-beasts.md`,
`tools/botobs/bosses/northrend_beasts.py` (new), `tools/botobs/tests/test_northrend_beasts.py` (new).

1. **Beast resolution.** Per-instance state in a `RaidInstanceState` (`src/Ai/Raid/RaidInstanceState.h`),
   refreshed at most once per instance per `getMSTime()` ms, as `Util/ToCEncounterGate.cpp` does.
   Gormok, Dreadscale and Acidmaw through `map->GetCreature(instance->GetGuidData(type))`, types 4,
   6, 7 (mirror them with a comment naming `trial_of_the_crusader.h`), Icehowl 34797 by entry with a
   bot-anchored 200 yd `GetCreatureListWithEntryInGrid`. Cache guids, resolve live. Engaged = alive and
   `IsInCombat()`. The refresh writes `nb.stage`. Nothing off map 649.
2. **Tank duties**, derived from world state so every bot agrees. Roster: living bot tanks
   (`IsBeastsTank`, `GET_PLAYERBOT_AI`, same instance), the group's flagged main tank first when it
   is one, the rest by guid. Duties in priority order `Gormok`, `Icehowl`, `WormMobile`, `GormokSwap`,
   `WormStationary`, each only while its beast is engaged (`GormokSwap` with Gormok). Worm form by
   `IsWormMobile` (`Util/ToCHelpers_Jormungars.h`); neither mobile → Acidmaw is `WormMobile`.
   - Pass 1: each duty takes its beast's current victim when the victim is a tank (bot or human, via
     `IsBeastsTank`) not yet holding a duty. A human holds a duty only this way.
   - Pass 2: each unfilled duty, in order, takes the first unassigned roster tank; with none left, it
     takes the tank of the lowest-priority filled duty below it.
   - Memoise the deal with the stage. `GetBeastsTankDuty` writes `nb.tank` for tank bots.
3. **`NorthrendBeastsTauntGuardMultiplier`** ("northrend beasts taunt guard"): zero an action when
   `IsTauntAction(bot, action)` (`src/Util/EncounterHelpers.h`), the encounter is live, and the bot's
   `"current target"` is a beast whose duty holder (`GetDutyOfBeast`, `GetBeastsDutyHolder`) is not
   this bot. The encounter's own taunts run inside `Execute` and are unaffected. Register it in
   `AddToCNorthrendBeastsMultipliers` after the existing multiplier.
4. **`NorthrendBeastsControlTankMovementMultiplier`**: keep its verdict; replace its entry list with
   `GetDutyOfBeast(botAI, bot->GetVictim()) != None`.
5. **Burst row** `ToCNorthrendBeastsBurstWindow`: while Icehowl is engaged, `allowLust` =
   `IsIcehowlStaggered` or his health ≤ 50%, `allowAll` = staggered or ≤ 30%; otherwise `{}`. The
   base tank-held gate still ANDs (his victim is cleared at the jump, so it re-arms 3-5 s into a daze).
6. **Jormungar consumers**, only `IsActive` of `WormsMobileEngagedByMainTankTrigger` and
   `WormsStationaryNeedsAssistTankTrigger`: fire on `GetBeastsTankDuty` = `WormMobile` /
   `WormStationary` instead of `IsMainTank` / assist index 0. Nothing else in the stem.
7. **Reader** `tools/botobs/bosses/northrend_beasts.py`, modelled on `bosses/general_vezax.py`
   (`run_sections`, `SECTIONS` table, banner that says when the trace is not a Beasts pull):
   - `--stage`: `nb.stage` spans with deaths per span, and when each beast was first sampled in combat
     against the pull.
   - `--tanks`: `nb.tank` per tank; each change of Gormok's victim (snap target column) with the
     Impale stacks (`aura` rows, `st`, all four ids) of old and new holder; peak stacks per tank;
     `nb.swap` branches; `gormok tank swap taunt` act rows; `nb.defensive` picks.
   - `--charge`: per `nb.charge` cycle the gaze target, the `lane` haz row, the outcome (4 daze, 5
     rage), bots within 12 yd of the lane's end at the outcome from the nearest snapshot, `nb.dodge`
     branches per bot, Trample 66734 `dmg` victims.
   - `--snobold`: Snobolled! 66406 `aura` spans per rider with role and seconds carried; `nb.snobold`
     picks per DPS.
8. **Test** `tools/botobs/tests/test_northrend_beasts.py`, modelled on `tests/test_general_vezax.py`:
   a synthetic pull pinning each section's arithmetic (one swap with stacks, one daze and one rage
   charge with a bot inside 12 yd, one healer rider), and every section reading empty on
   `fixtures/full-v12.ndjson`.
9. **Doc** `northrend-beasts.md`, folded with `compact-docs-writer` after the other groups land: the
   stage model and tank duties, the Gormok and Icehowl decisions below with their rationale, known
   gaps, and "What a trace answers" (every key above, reader usage), as in
   `docs/raids/ulduar/mimiron.md`. Keep the mechanics sections as they are unless the code proves one
   wrong.

### Group 2 — Gormok

Files: `Util/ToCHelpers_Gormok.h`, `Util/ToCHelpers_Gormok.cpp`, `Trigger/ToCTriggers_Gormok.h`,
`Trigger/ToCTriggers_Gormok.cpp`, `Action/ToCActions_Gormok.h`, `Action/ToCActions_Gormok.cpp`,
`Multiplier/ToCMultipliers_Gormok.h`, `Multiplier/ToCMultipliers_Gormok.cpp` (new; the header's
inline no-op becomes a declaration).

10. **Constants** in `ToCHelpers_Gormok.h`: keep `GORMOK_IMPALE_SWAP_STACKS = 3`; add
    `GORMOK_IMPALE_DEFENSIVE_STACKS = 5`, `SPELL_SNOBOLLED = 66406`, `NPC_FIRE_BOMB = 34854`,
    `GORMOK_STOMP_INTERRUPT_RADIUS = 20.0f`, `GORMOK_STOMP_TRIGGER = 22.0f`,
    `GORMOK_STOMP_CLEARANCE = 24.0f`, `GORMOK_CARRIER_MELEE_RANGE = 10.0f` (centre to centre).
11. **Hold** (`gormok tank duty` → `gormok tank hold boss`, renamed from `gormok engaged by main tank`
    / `gormok main tank hold boss`): duty `Gormok`. Skull and RTI as today; taunt (`CastClassTaunt`)
    only while his victim is not a tank; then the existing `DragBossToAnchor(gormok, ARENA_CENTER)`.
12. **Swap** (`gormok tank swap needed` → `gormok tank swap taunt`, `ACTION_RAID + 5`): duty
    `GormokSwap`, his victim is the `Gormok` holder, the holder's Impale stacks ≥ 3 and the bot's own
    are 0. The action taunts; on a failed taunt it attacks him. Writes `nb.swap` from the trigger's
    derivation (one helper for both).
13. **Defensive** (`gormok tank defensive` → `gormok tank defensive`, `ACTION_RAID + 6`): duty
    `Gormok`, the bot is his victim, own stacks ≥ 5, or ≥ 3 with no `GormokSwap` holder. Casts
    `NextTankDefensive(botAI, bot, "nb.defensive")` (`src/Ai/Raid/RaidTankDefensive.h`) on itself.
14. **Snobold pick**, one helper for trigger and action: a per-instance guid cache of 34800 refreshed
    at most once per ms; a candidate is alive and its `GetVehicleBase()` is a player. Rank by rider:
    healer > ranged DPS > melee DPS > tank, then snobold guid. Healer and ranged riders' snobolds are
    for every DPS; melee and tank riders' for melee DPS only; once Gormok is not engaged, every
    rider's for every DPS. Writes `nb.snobold`.
15. **Focus** (`gormok snobold on raid` → `gormok focus snobold`, `ACTION_RAID + 2`): DPS with a pick
    attack it; no raid icon. **`GormokSnoboldTargetGuardMultiplier`** zeroes `DpsAssistAction` for a
    bot with a pick, so the flip back to the skull cannot happen.
16. **Carrier** (`gormok snobolled` → `gormok bring snobold to melee`, `ACTION_RAID + 3`): a ranged DPS
    or healer carrying a snobold (`SPELL_SNOBOLLED` or a snobold whose vehicle base is the bot) while
    Gormok is engaged walks to `GORMOK_CARRIER_MELEE_RANGE` of him, latched destination, returns false
    once inside. **`GormokSnoboldCarrierMultiplier`** zeroes that bot's `MovementAction`s except its own,
    `AttackAction` and `avoid aoe` until the snobold dies. Register both multipliers in
    `AddToCGormokMultipliers`.
17. **Stomp range** (`gormok stomp range` → `gormok leave stomp range`, `ACTION_RAID + 3`): a ranged DPS
    or healer, not carrying a snobold, closer than `GORMOK_STOMP_TRIGGER` to Gormok's centre moves to
    `FindNearestPositionClearOfHazards` with Gormok at `GORMOK_STOMP_CLEARANCE`, `preferNear` his
    victim's position.
18. Drop the old `GetFirstAliveUnitByEntry` lookups for Gormok in favour of `GetEngagedBeast`.

### Group 3 — Icehowl

Files: `Util/ToCHelpers_Icehowl.h`, `Util/ToCHelpers_Icehowl.cpp`, `Trigger/ToCTriggers_Icehowl.h`,
`Trigger/ToCTriggers_Icehowl.cpp`, `Action/ToCActions_Icehowl.h`, `Action/ToCActions_Icehowl.cpp`,
`Multiplier/ToCMultipliers_Icehowl.h`, `Multiplier/ToCMultipliers_Icehowl.cpp`.

19. **Constants** in `ToCHelpers_Icehowl.h`: `SPELL_STAGGERED_DAZE = 66758`, `SPELL_FROTHING_RAGE =
    66759` (remapped at the call), `SPELL_TRAMPLE = 66734`, `ICEHOWL_CHARGE_BACK = 35.0f`,
    `ICEHOWL_CHARGE_REACH = 50.0f`, `ICEHOWL_CHARGE_REACH_GATE = 46.0f` (angle in (1, 2) rad),
    `ICEHOWL_TRAMPLE_RADIUS = 12.0f`, `ICEHOWL_CHARGE_TRIGGER = 14.0f`, `ICEHOWL_CHARGE_CLEARANCE =
    16.0f`, `ICEHOWL_CHARGE_TIGHT_CLEARANCE = 13.0f`, `ICEHOWL_CENTRE_TOLERANCE = 3.0f`. Delete
    `IsBotInChargeCorridor` and `HasMassiveCrashAura`.
20. **Charge state**, per instance in a `RaidInstanceState`, refreshed at most once per ms, pure
    function of world state:
    - active while Icehowl is engaged and `REACT_PASSIVE`; phase 1 without a target, 2 while
      `GetTarget()` names a living player and he is within `ICEHOWL_CENTRE_TOLERANCE` of
      `ARENA_CENTER` (angle = centre → that player), 3 once he is further out (angle = his position →
      centre, then frozen until the cycle ends);
    - `P_jb` = centre − 35 along the angle, `P_end` = centre + 50 (46) along it;
    - on leaving passive: 4 with `SPELL_STAGGERED_DAZE` on him, 5 with Frothing Rage, else 0;
    - writes `nb.charge`, `nb.gaze` and the `lane` haz row; drops a latch older than 12 s.
    Expose `bool IcehowlChargeLatched(PlayerbotAI*, Position& start, Position& end)`,
    `float DistanceToIcehowlCharge(PlayerbotAI*, float x, float y)` (2D, to the segment),
    `IsIcehowlStaggered`.
21. **Dodge** (`icehowl charge incoming` → `icehowl clear charge path`, `ACTION_EMERGENCY + 8`): any bot
    within `ICEHOWL_CHARGE_TRIGGER` of the latched segment. The action, skipping while stunned:
    `FindNearestPositionClearOfHazards` (circles at both ends, `accept` = segment distance ≥
    `ICEHOWL_CHARGE_CLEARANCE`, 30 yd), falling back to the tight clearance; interrupt a pinning cast
    (`IsMovementPreventedByCasting` → `InterruptNonMeleeSpells(true)`) before `TryMoveTo` at
    `MOVEMENT_FORCED`; latch the destination per bot and return true without re-issuing while the walk
    is in flight and the spot still clears; return true while inside the corridor. Writes `nb.dodge`.
22. **`IcehowlChargeGuardMultiplier`** ("icehowl charge guard"), replacing
    `IcehowlSuppressMovementDuringChargeMultiplier`: while the charge is latched, for every bot zero
    each `MovementAction` except `AttackAction` and `IcehowlClearChargePathAction`, each
    `CastReachTargetSpellAction`, and the actions named `blink` and `disengage` (by name, no class
    headers). Test the action before the latch.
23. **Hold** (`icehowl tank duty` → `icehowl tank hold boss`, renamed from `icehowl engaged by main
    tank` / `icehowl main tank hold boss`): duty `Icehowl`; skull and RTI; taunt only while his
    victim is not a tank and he is not passive; no drag.
24. **Defensive** (`icehowl frothing rage` → `icehowl tank defensive`, `ACTION_RAID + 6`): the bot is
    his victim and he carries Frothing Rage; `NextTankDefensive(botAI, bot, "nb.defensive")`.

### Integrate

Compile the contracts together, re-run CMake (new `Util/ToCHelpers_NorthrendBeasts.cpp`,
`Multiplier/ToCMultipliers_Gormok.cpp`), run the checks per the overview. Check that every renamed
node resolves (pblint) and that `IcehowlSuppressMovementDuringChargeMultiplier` has no reference left.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| One duty deal for every stage: priority `Gormok`, `Icehowl`, `WormMobile`, `GormokSwap`, `WormStationary`; sticky to the beast's tank victim; an unfilled duty takes the lowest-priority tank | brief (one model for 10/25 and the heroic overlap); `raid-mechanics-lessons.md` (derive, don't communicate); guide (the stationary worm "can be tanked by any ranged DPS") | `IsMainTank`/assist index per stage, which gives the MT Gormok and the mobile worm at once in the overlap |
| Roster = living bot tanks, flagged MT first, then guid; a human tank only as the sticky holder of what he tanks | `UldEncounter_IronAssembly.cpp` (group flags count humans) | group role flags |
| Swap at 3 Impale stacks on every difficulty, and only once the taunter's own Impale expired | guide (3; heroic 2-3); DBC durations 30/40/30/45 s refreshed per hit, so a taunter with stacks resumes them | a fixed 3 (stacks escalate on 25-man), 2 on heroic |
| Holder's tank defensive at 5 stacks, or 3 with no swap partner | brief; guide (rotate defensives at high stacks) | none |
| Class taunts on a beast vetoed for a tank the deal does not give it to | `docs/raids/ulduar/thorim.md` (class taunts fight the encounter); beasts obey taunt diminishing returns | leave class taunts |
| Snobold rank healer > ranged > melee > tank; healer/ranged riders for all DPS, melee/tank riders for melee DPS; all DPS once Gormok is down | guide (high/low priority; both DPS roles attack snobolds) | melee on the first snobold found (today, which also finds the ones riding Gormok) |
| No raid icon on snobolds; `dps assist` vetoed while a bot has a pick | pitfalls (marks never clear in a pull); README "Targeting: the whole fight" | cross mark per snobold |
| A snobolled ranged DPS or healer walks into Gormok's melee; generic movers held off meanwhile | guide | stay and wait to be freed |
| Casters keep 22 yd off Gormok, stepping to 24 | script (20 yd, 8 s school lockout, centre to centre); guide (ranged past 15) | rely on `spellDistance` 28.5 |
| Fire Bomb: no new node; generic `avoid aoe` flees the 2 yd pulse | `SpellInfoCorrections.cpp` cuts 66320's radius to 2; the NPC passes `PossibleTriggersValue` | a dedicated dodge now (rest brief) |
| Charge hazard = the whole path from `P_jb` to `P_end`, 12 yd half-width, trigger 14, clear to 16 | guide ("strafe out of the way"); script tests contact ~100-200 ms in and on arrival, and a server stall moves the first test along the path | the two contact points only |
| Charge line from the gaze target while he is at the centre, then from his own position; latched per instance to the end of the cycle | script (line fixed at the jump back; everyone is stunned through the gaze) | his facing (today; after the jump back he faces away from the line) |
| From the gaze to the end of the charge every bot's movers are zeroed except attacks and the dodge, gap closers and blink/disengage too | brief (settle forced dodges in a multiplier); `docs/raids/ulduar/mimiron.md` (charge back into the hazard); melee chasing his start point meet the first contact test | veto only bots inside the corridor (today; others walk in) |
| Dodge at `MOVEMENT_FORCED`, interrupting a pinning cast, destination latched | pitfalls (movement lock, cast pin, `mm->Clear` flips) | `MOVEMENT_COMBAT` / `FleePosition` (today) |
| Icehowl held where he stands, no drag to `ARENA_CENTER` | script (every charge starts at the centre and ends at the wall); guide (tank him at a wall) | keep the drag |
| Icehowl holder's tank defensive under Frothing Rage; dispel left to the hunters' generic node | brief; script (heroic Icehowl is immune to dispel effects) | a raid dispel node |
| Lust at Staggered Daze or Icehowl ≤ 50%; other burst cooldowns during Icehowl only in the daze or ≤ 30%; Gormok and worms unrestricted | guide (all cooldowns in the stun); script (+100% damage taken, 15 s); thresholds are the conservative fallback for a raid that never lands a clean charge | lust at the pull (today) |
| Whirl gets no node | melee range 14.8 < Whirl's 15 yd (reach 12), ranged already outside | the guide's max-melee stand |
| Jormungar tank triggers re-keyed on the worm duties, nothing else there | brief (the interface it consumes) | leave them (MT double duty in the overlap) |
| New ids in stem helpers; `Util/ToCData.h` untouched | lane rules (other wave-3 lanes edit it) | move the Massive Crash constants now |
| No coordinates proposed | the arena floor is GO 195527 with no static mesh, so navprobe cannot verify one (`northrend-beasts.md`); `ARENA_CENTER` and the charge points come from the script | navprobe-validated stands |
| Arctic Breath spread and the Fire Bomb impact dodge move to `w1a-beasts-rest` | Oversized-lane rule: a 5 s freeze and a 5-6k hit are healable, a missed charge or a failed swap is not | build them now |
| Burst row before Icehowl engages: `allowLust` false, `allowAll` true, not task 5's literal `{}` | scope item 4 and the lust row above: `{}` keeps lust on Gormok's pull | `{}` (one line in `ToCNorthrendBeastsBurstWindow`) |
| A lone surviving worm is `WormMobile` whatever its form | script: a lone worm goes under at once and comes up mobile | by form, leaving the holder idle while it is stationary |
| Icehowl's grid search runs at most once a second while he is missing | he walks in for 10 s before he attacks | every ms, a full grid walk per miss |
| Pass 2 never takes a duty a human holds | a human cannot be reassigned | take it, leaving his beast with no bot holder |
| Beast guid slots reuse `TOC_DATA_*` in `Util/ToCEncounterGate.h` | already mirrored there with the `trial_of_the_crusader.h` comment | a second mirror |
| Snobold carrier multiplier spares every Beasts-prefixed mover (`ToCEncounterOfTrigger`), not only the carrier walk | pitfalls (a blanket mover veto kills dodges); a heroic carrier still has pools, Sweep and the charge | only the carrier's own walk. Cost: `northrend worms spread` can pull a carrier out, the walk brings it back |
| Snobold carrier multiplier also zeroes `CastSpellAction`s named `blink`/`disengage` | review: `EnemyTooCloseForSpellTrigger` fires under 10 yd and throws the carrier out; `raid-mechanics-lessons.md` "Range and reach" | leave them (the walk re-fires every cooldown) |
| A snobold candidate needs a living rider | `npc_snobold_vassal` `UpdateAI`: a dead rider's snobold returns to Gormok | any rider |
| Carrier and stomp walks share `GormokWalkAction`: spot latched while the bot moves and `last movement` still names it | pitfalls ("Every MoveTo calls mm->Clear()") | re-derive each tick |
| Carrier aims 7 yd from Gormok's centre, keeps a latched spot while ≥ 1 yd inside 10, stops within 10 | `raid-mechanics-lessons.md` (park tolerance belongs in the clearance maths) | aim at 10 |
| Stomp sweep radius 30 yd | a caster on his centre has 24 to walk | the dodge default |
| Both Gormok walks at `MOVEMENT_COMBAT`, never interrupting a cast: nothing issued while a cast pins the feet, and a walk left standing 500 ms past its issue with `last movement` still on its spot has the booking cleared before the re-issue | neither mechanic is lethal; pitfalls (interrupt per mechanic, a walk stopped short is never re-issued) | `MOVEMENT_FORCED` with an interrupt |
| Gormok hold skips `DragBossToAnchor` while an Icehowl charge is latched | review: every charge line crosses `ARENA_CENTER`, and the drag is an `AttackAction` the charge guard spares | reject only drag steps near the lane |
| Charge dodge's `hold` needs `last movement` on its spot (0.5 yd) | review: another mover's walk counted as the dodge in flight | any `isMoving` |
| Gormok multipliers gated on live: in a normal walk-in gap `dps assist` isn't vetoed | w0c (all multipliers gated); harmless, the focus node outranks it | gate on open |
| Gormok hold taunts only off a non-tank victim (a pet counts), and its trigger needs him engaged, so no attack during the walk-in | brief task 11 | taunt whenever he isn't on the holder |
| Roles: rider tank, then healer, then ranged, else melee; DPS = neither tank nor healer; carrier and stomp = not a tank, healer or ranged; tank always `IsBeastsTank`, here and in the control-tank multiplier | brief tasks 14, 16, 17; pitfalls (`IsTank` flickers) | group role flags |
| Icehowl's skull only while he is the only engaged beast; RTI always | pitfalls (raid marks): two holders re-marking every tick flip the DPS | always mark (task 23) |
| Dodge sweep circles every ≤ 4 yd along the whole lane, `accept` still the segment distance | same result on a convex corridor, but spots inside it fail before the collision ray | circles at both ends only |
| Tight 13 yd fallback only for a bot under 13 yd; past 13 with no 16 yd spot it parks (`clear`) | pitfalls (a dodge must clear more than its own trigger radius): a 13 yd spot sits inside the 14 yd trigger | fall back from anywhere, re-firing 2 yd hops |
| "Stunned" = `!CanMove()`; "inside the corridor" for `none`/`locked` returning true = within 12 yd | root, freeze and knockback pin a bot the same way; 12 is Trample's reach | stun aura only; 14 yd |
| Dodge latch: 5 s ceiling, 500 ms before a walk must show `isMoving`, 1 yd arrival, spot kept while ≥ 13 yd off the line; a stalled or replaced walk clears `last movement` first | a knockback or stun leaves a booking that blocks the re-issue for up to 5 s | no clear |
| Charge cycle: latch dropped after 12 s until he leaves passive; a refresh gap over 20 s starts a new cycle; a gaze target dying at the centre keeps phase 2 and the line; phase 3 once he is off the centre with a player target or a seen gaze | a cycle runs ~9 s and 30 s separate cycles; a stuck latch would freeze every mover | no expiry |
| Icehowl hold taunts only while he isn't passive and his victim isn't a beasts tank, else attacks when he isn't the current target | brief task 23 | taunt on every loss |
| Charge guard exempts `AttackAction` and the dodge; zeroes other `MovementAction`s, `CastReachTargetSpellAction`s, and `CastSpellAction`s named `blink`/`disengage` | brief task 22; name read only for non-reach casts | class headers |
| The reader takes a cycle's last `lane` row | the gaze writes one, the jump back writes the frozen line | the first |

## Carried over

- `w1a-beasts-rest` ([w1a-beasts-rest.PLAN.md](w1a-beasts-rest.PLAN.md)): Arctic Breath spread, the
  Fire Bomb impact dodge.
- Merge stage: the arena-floor fact (`northrend-beasts.md`, "The arena floor is a gameobject") is
  raid-wide; move it to `docs/raids/trial-of-the-crusader/README.md`.
- Merge stage, engine lessons for `docs/engine/pitfalls.md`: `SpellInfoCorrections.cpp` can rewrite
  a DBC radius (Fire Bomb 5 → 2 yd), so read it before sizing a hazard; `EventMap::ExecuteEvent`
  always erases, so an event the script does not `Repeat` runs once whatever its comment says.
- Merge stage, `docs/raids/trial-of-the-crusader/README.md:93`: the node row still names
  `icehowl charge incoming`, `IcehowlSuppressMovementDuringChargeMultiplier` | Massive Crash. Now
  `icehowl charge incoming` and `icehowl charge guard` read `REACT_PASSIVE` and Icehowl's
  `UNIT_FIELD_TARGET` (no spell id), and `icehowl frothing rage` keys on Frothing Rage
  66759 / 67657 / 67658 / 67659.
- Merge stage or `w6-closeout`: `tools/botobs/tests/test_toc_naming.py:58,199` use the removed
  `gormok engaged by main tank` as synthetic node data; passes, only the name is stale.
- `w1b-jormungars`:
  - `FindWorm` in `Action/ToCActions_Jormungars.cpp` picks by form; use `GetBeastOfDuty`, or the
    `WormMobile` holder has nothing to hold with neither worm mobile or a lone survivor still
    stationary (doc Known gaps).
  - The mobile-worm hold marks skull every tick, so a heroic Gormok + worms overlap trades it with
    Gormok's hold; mark only while Gormok isn't engaged, as Icehowl's hold does.
- `w6-closeout`: `SPELL_MASSIVE_CRASH_*` in `Util/ToCData.h` lose their last reader in group 3.
- User: re-run CMake before building (two new `.cpp`).

## Known gaps

- No Hand of Protection on a snobolled healer or caster (guide) and no external on Ferocious Butt.
- Heroic Frothing Rage cannot be dispelled; only the holder's defensive answers it.
- Melee eat every Whirl (reach math above).

## Blocked
