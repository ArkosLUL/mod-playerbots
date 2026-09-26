# w5-anubarak — Anub'arak

Wave 3. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).
Guide: `anubarak-master-strategy-guide-toc-25/`.

## Scope

1. **Transition.** The Lich King scene (stage 8) and the floor collapse.
   - `IsEncounterInProgress()` is true from that scene until Anub'arak dies or the raid wipes, so the
     gate must stay closed until the pull.
   - Handle landing in the pit and regrouping.
2. **Phase 1.**
   - Nerubian Burrowers: tank them on Permafrost in heroic; Shadow Strike interrupts.
   - Scarabs.
   - Frost Sphere → Permafrost economy.
   - Penetrating Cold targets.
3. **Phase 2 (submerged).**
   - Kite the Pursuing Spike into Permafrost with hazard-clear stands, replacing the blanket
     movement veto.
   - Scarabs.
   - The spike (34660) is boss-flagged and engages under the name "Anub'arak".
4. **Phase 3.** Leeching Swarm with `RaidTankDefensive`; burrowers keep coming in heroic.
5. **Threat.** On emerge he resets threat and picks a random target (`boss_anubarak_trial.cpp`), so
   add a redirect or taunt.
6. **Lust row.** `w0c-foundation` ported the Leeching Swarm delay unchanged; re-tune it now.
7. **Observability.** `anub.` probes (phase, spike target, sphere use), a
   `tools/botobs/bosses/anub_arak.py` reader and test.
8. Verify every Anub'arak node `w0c-foundation` made live in 25N/10H/25H.

## Owns

Stem `Anubarak`; `docs/raids/trial-of-the-crusader/anubarak.md`; the reader and its test.

## Task list

Mechanics, ids, timers and geometry are verified in
[anubarak.md](../../raids/trial-of-the-crusader/anubarak.md); read it first. Paths below are under
`src/Ai/Raid/ToC/`. Each group edits only its own files; the integrate step compiles them together,
runs the checks and fixes the seams.

### Group 1 — state and geometry

Files: `Util/ToCHelpers_Anubarak.h`, `Util/ToCHelpers_Anubarak.cpp`,
`Multiplier/ToCMultipliers_Anubarak.h`, `Multiplier/ToCMultipliers_Anubarak.cpp`.

1. **Header contract.** Group 2 codes against exactly this, so change it only through the integrate
   step. Drop `GetNearestPermafrost` (its only caller, the old kite, is rewritten). `ToCData.h`
   keeps `SPELL_MARK`, `SPELL_LEECHING_SWARM`, `SPELL_SUBMERGE_ANUB` and the NPC entries.

   ```cpp
   constexpr uint32 SPELL_FROST_SPHERE = 67539;      // kept
   constexpr uint32 SPELL_PERMAFROST = 66193;        // row; remap at the call
   constexpr uint32 SPELL_PENETRATING_COLD = 66013;  // row; remap at the call
   constexpr uint32 SPELL_SHADOW_STRIKE = 66134;     // no row
   extern const Position ANUBARAK_ROOM_CENTER;       // {745.0f, 135.0f, 142.6f}
   constexpr float ANUBARAK_ROOM_RADIUS = 120.0f;         // sphere/spike sweep from any bot
   constexpr float ANUBARAK_TANK_PATCH_OFFSET = 12.0f;    // side points ROOM_CENTER.y +/- this
   constexpr float ANUBARAK_TANK_PATCH_LATCH = 30.0f;     // max patch distance from its side point
   constexpr float ANUBARAK_TANK_PATCH_ARRIVE = 2.0f;
   constexpr float ANUBARAK_PERMAFROST_SLOW_REACH = 7.5f; // 6 yd aura + 1.5 player reach
   constexpr float ANUBARAK_KITE_STAND_OFFSET = 8.5f;
   constexpr float ANUBARAK_KITE_ARRIVE = 2.0f;
   constexpr float ANUBARAK_KITE_REAIM_DEGREES = 30.0f;
   constexpr float ANUBARAK_KITE_RING_RADIUS = 35.0f;
   constexpr float ANUBARAK_KITE_RING_STEP_DEGREES = 40.0f;
   constexpr float ANUBARAK_SPIKE_DANGER_RADIUS = 10.0f;
   constexpr float ANUBARAK_SPIKE_CLEARANCE = 13.0f;
   constexpr float ANUBARAK_SPIKE_LANE_HALF_WIDTH = 7.0f;
   constexpr float ANUBARAK_SPIKE_LANE_CLEARANCE = 9.0f;
   constexpr float ANUBARAK_SPIKE_DODGE_SEARCH = 30.0f;
   constexpr float ANUBARAK_INTERRUPT_RANGE = 30.0f;
   constexpr uint8 ANUBARAK_HEROIC_KITE_RESERVE = 2;

   enum class AnubarakPhase : uint8 { None = 0, Surface = 1, Submerged = 2, Swarm = 3 };
   enum class AnubarakKiteBranch : uint8 { None, Patch, Detour, Ring, Hold };
   struct AnubarakKiteStand { Position stand; ObjectGuid patch; AnubarakKiteBranch branch = AnubarakKiteBranch::None; };

   Creature* GetAnubarak(Player* bot);
   bool AnubarakEngaged(PlayerbotAI* botAI);
   AnubarakPhase GetAnubarakPhase(PlayerbotAI* botAI);
   bool AnubarakSubmerged(PlayerbotAI* botAI);
   bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI);
   Creature* GetPursuingSpike(Player* bot);
   Player* GetSpikeTarget(Player* bot);
   bool IsFrostSphereFlying(Unit* sphere);
   bool IsFrostSphereFalling(Unit* sphere);
   bool IsPermafrostPatch(Unit* sphere);
   std::vector<Creature*> GetFrostSpheres(Player* bot);
   bool UnitOnPermafrost(Unit* unit);
   Creature* GetAnubarakTankPatch(Player* bot, uint8 side);
   Position GetAnubarakBossAnchor(Player* bot);
   int8 GetAnubarakBurrowerTankSide(Player* bot);
   bool IsAnubarakPickupTank(Player* bot);
   Creature* GetAnubarakSphereToShoot(Player* bot);
   bool GetAnubarakKiteStand(Player* bot, AnubarakKiteStand const* previous, AnubarakKiteStand& out);
   bool AnubarakInSpikeDanger(Player* bot);
   bool GetAnubarakSpikeDodgeSpot(Player* bot, Position& out);
   Creature* GetShadowStrikeCaster(Player* bot);
   char const* AnubarakReadyInterrupt(Player* bot, Unit* target);
   bool IsAnubarakShadowStrikeInterrupter(Player* bot, Unit* burrower);
   ```

2. **Boss, engage, phase.** `GetAnubarak`: `map->GetCreature(instance->GetGuidData(TOC_DATA_ANUBARAK))`
   (`ToCEncounterGate.h`), alive, else nullptr; never `GetFirstAliveUnitByEntry`, which loses him
   while unselectable. `AnubarakEngaged` = `ToCEncounterIsLive(botAI, ToCEncounter::Anubarak)`.
   Phase, per instance and memoised per ms in a `RaidInstanceState<AnubarakState>`: `None` while not
   engaged, which also resets every latch below; `Submerged` while he has `UNIT_FLAG_NOT_SELECTABLE`
   or `SPELL_SUBMERGE_ANUB`; `Swarm` latched once surfaced with a group member carrying the remapped
   Leeching Swarm, his current spell being it, or his health below 30%; else `Surface`.
   `AnubarakSubmerged`/`AnubarakLeechingSwarmActive` read the phase. This fixes the dead
   `AnubarakSubmerged` (Known gaps).
3. **Spike and spheres.** `GetPursuingSpike`: grid search for entry 34660 within
   `ANUBARAK_ROOM_RADIUS` (it is unselectable, so never through `possible targets`).
   `GetSpikeTarget`: the alive group member carrying `SPELL_MARK`. `IsFrostSphereFalling`: alive,
   67539, unselectable. `GetFrostSpheres`: every alive sphere, one grid sweep per instance per ms
   cached as guids, resolved on read. `UnitOnPermafrost`: `HasAura` of the remapped Permafrost.
4. **Probes**, in the per-instance state: `ObsValue<uint32>` `anub.phase`, `anub.patches` (patches
   alive), `anub.flying` (flying spheres); `ObsValue<ObjectGuid>` `anub.spike` (marked player, empty
   when none), `anub.patch0`, `anub.patch1` (latched tank patches). Written by the phase read.
5. **Tank patches and anchor.** Side 0 point is `ANUBARAK_ROOM_CENTER` + (0, +offset), side 1
   (0, -offset). `GetAnubarakTankPatch` latches per instance the patch nearest each side point within
   `ANUBARAK_TANK_PATCH_LATCH`, never the same patch for both, re-latching once the held one is gone.
   `GetAnubarakBossAnchor`: midpoint of both latched patches, else `ANUBARAK_ROOM_CENTER`.
   `GetAnubarakBurrowerTankSide`: 0 or 1 for `IsAssistTankOfIndex(bot, 0|1, true)`, else -1.
   `IsAnubarakPickupTank`: the main tank while alive, else assist tank 0 (living index).
6. **Sphere duty** (`GetAnubarakSphereToShoot`, probe `NoteDerived` `anub.sphere`: `kite`, `0`, `1`,
   `none`). Shooters: alive `IsRangedDps` group members not carrying the mark, sorted by guid; shooter
   i takes need i. Needs in order: `kite` while a spike target exists and `GetAnubarakKiteStand` finds
   no patch for it, taking the flying sphere nearest that target; then each side, in phase
   `Surface`/`Swarm`, whose tank patch is missing and no sphere is falling within the latch distance,
   taking the flying sphere nearest its side point. Heroic (`GetRaidDifficulty()` heroic): a side need
   only while more than `ANUBARAK_HEROIC_KITE_RESERVE` spheres fly; `kite` always.
7. **Kite stand** (`GetAnubarakKiteStand`, probe `NoteDerived` `anub.kite` by branch). Per patch P:
   stand S = P + unit(P - spike) × `ANUBARAK_KITE_STAND_OFFSET`, floor-checked with
   `ValidateFloorPoint`, retried at ±20° and ±40° round P. Keep S only if the spike is farther from
   S than the bot is; in heroic prefer patches not latched as tank patches. Take the S nearest the bot.
   Keep `previous` while its patch lives and its bearing from P is within
   `ANUBARAK_KITE_REAIM_DEGREES` of the new one. Bot within `ANUBARAK_KITE_ARRIVE` of S: `Hold`.
   Straight line to S passing within `ANUBARAK_PERMAFROST_SLOW_REACH` + 1 of P: `Detour` via the point
   `ANUBARAK_KITE_STAND_OFFSET` from P perpendicular to the spike line, on the bot's side. No patch
   qualifies: `Ring`, a point `ANUBARAK_KITE_RING_RADIUS` from `ANUBARAK_ROOM_CENTER`,
   `ANUBARAK_KITE_RING_STEP_DEGREES` round from the bot's bearing in the direction that opens the
   angle to the spike, floor-checked. No spike yet: false.
8. **Spike dodge.** `AnubarakInSpikeDanger`: engaged, spike alive, bot not marked, and within
   `ANUBARAK_SPIKE_DANGER_RADIUS` of the spike or within `ANUBARAK_SPIKE_LANE_HALF_WIDTH` of the
   segment spike → spike target. `GetAnubarakSpikeDodgeSpot`: `FindNearestPositionClearOfHazards`
   (HazardCircle overload) with the spike at `ANUBARAK_SPIKE_CLEARANCE`, search
   `ANUBARAK_SPIKE_DODGE_SEARCH`, `accept` rejecting spots within `ANUBARAK_SPIKE_LANE_CLEARANCE` of
   the segment; probe `NoteDerived` `anub.dodge` `spike`/`lane`/`none`.
9. **Shadow Strike duty.** `GetShadowStrikeCaster`: nearest alive burrower with
   `FindCurrentSpellBySpellId(SPELL_SHADOW_STRIKE)`. `AnubarakReadyInterrupt`: first of `kick`,
   `pummel`, `shield bash`, `counterspell`, `wind shear`, `mind freeze`, `hammer of justice`,
   `shockwave`, `concussion blow`, `bash`, `war stomp`, `shadowfury` that `CanCastSpell` allows on the
   target (burrowers are silence-immune, so no Silencing Shot or Spell Lock).
   `IsAnubarakShadowStrikeInterrupter`: the burrower's victim if it has one ready, else the
   lowest-guid alive unmarked group member with one ready within `ANUBARAK_INTERRUPT_RANGE`; probe
   `NoteDerived` `anub.interrupter` `1`/`0`.
10. **Multipliers.** Keep `AnubarakControlTankMovementMultiplier`. Narrow
    `AnubarakProtectSpikeKiteMultiplier`: for the marked bot while engaged, zero
    `CastReachTargetSpellAction` and every `MovementAction` except `AttackAction` subclasses and
    `AnubarakKiteSpikeToPermafrostAction`, but do zero `AnubarakMainTankHoldBossAction` and
    `AnubarakAssistTankHoldBurrowerAction`, which move. Add `AnubarakTankTargetGuardMultiplier`
    ("anubarak tank target guard"): zero `TankAssistAction` and `DpsAssistAction` for the pickup tank
    in `Surface`/`Swarm`, and for a bot with a burrower side while a burrower lives; test the action
    family first. Burst row: `{allowAll = true, allowLust = phase == Swarm}`.

### Group 2 — nodes

Files: `Trigger/ToCTriggers_Anubarak.h`, `Trigger/ToCTriggers_Anubarak.cpp`,
`Action/ToCActions_Anubarak.h`, `Action/ToCActions_Anubarak.cpp`.

11. **Every Anub'arak trigger starts with `AnubarakEngaged(botAI)`**: the stage gate is open from
    the floor break, and he turns attackable pre-pull. Keep class names
    `AnubarakKiteSpikeToPermafrostAction`, `AnubarakMainTankHoldBossAction`,
    `AnubarakAssistTankHoldBurrowerAction` (group 1's multipliers name them). Keep the trigger name
    `anubarak scarab on raid` (`tools/botobs/tests/test_toc_naming.py` lists it). Relevance is the
    `ACTION_*` constant plus the offset, no ties:

    | Relevance | Trigger → action | Who, when | Does |
    |---|---|---|---|
    | EMERGENCY+8 | `anubarak pursued by spike` → `anubarak kite spike to permafrost` | carries `SPELL_MARK` | 12 |
    | EMERGENCY+6 | `anubarak spike nearby` → `anubarak avoid spike` | `AnubarakInSpikeDanger` | 13 |
    | EMERGENCY+3 | `anubarak burrower casting shadow strike` → `anubarak interrupt shadow strike` | caster exists, bot is its interrupter | 14 |
    | RAID+6 | `anubarak leeching swarm on tank` → `anubarak tank defensive` | phase `Swarm`, bot is his victim, `NextTankDefensive(botAI, bot, nullptr)` non-null | casts `NextTankDefensive(botAI, bot, "anub.defensive")` on self |
    | RAID+5 | `anubarak ranged should seed permafrost` → `anubarak destroy frost sphere` | `GetAnubarakSphereToShoot` non-null | `Attack` that sphere |
    | RAID+4 | `anubarak burrower should be focused` → `anubarak focus burrower` | DPS, a burrower with `UnitOnPermafrost`, or rti `cross` | 15 |
    | RAID+3 | `anubarak burrower needs assist tank` → `anubarak assist tank hold burrower` | burrower side ≥ 0, not the pickup tank while he is surfaced, a burrower alive | 16 |
    | RAID+2 | `anubarak scarab on raid` → `anubarak tank pick up scarab` | tank; pickup tank only in `Submerged`, side tanks only with no burrower alive; a scarab within 30 yd on a non-tank | `CastClassTaunt` then `Attack`, else `Attack` |
    | RAID+1 | `anubarak engaged by main tank` → `anubarak main tank hold boss` | `IsAnubarakPickupTank`, phase `Surface`/`Swarm` | 17 |
    | MEDIUM_HEAL+5 | `anubarak penetrating cold on raid` → `anubarak heal penetrating cold` | healer, a group member with the remapped Penetrating Cold below 90% | 18 |

12. **Kite.** Latch the stand in the action (`previous` for group 1's helper). `InterruptNonMeleeSpells`
    when `IsMovementPreventedByCasting`. `MoveTo` the stand (the detour point first) at
    `MOVEMENT_FORCED`; `Hold` returns false so a healer or caster keeps working while group 1's
    multiplier keeps generic movers off. Drop the raw flee fallback.
13. **Avoid spike.** Latch the dodge spot until reached or back out of danger; `MoveTo` at
    `MOVEMENT_FORCED`; false when no spot.
14. **Interrupt.** Cast `AnubarakReadyInterrupt` on the caster; out of range, `MoveTo` toward it at
    `MOVEMENT_COMBAT` first.
15. **Focus burrower.** With a burrower on Permafrost: `MarkTargetWithCross` the lowest health % one
    (re-mark if the cross sits on anything else), `SetRtiTarget(botAI, "cross", it)`, `Attack`. With
    none: rti back to `skull`, `ClearTargetIcon` cross if it marks a burrower off Permafrost or a dead
    unit, return false. Delete the future-work comment w0a flagged.
16. **Hold burrower.** Target: a burrower on this bot, else one whose victim is not a tank, nearest
    this bot's tank patch. Not on this bot: `CastClassTaunt`. `SetRtiTarget` to it under a side name
    (`square` side 0, `triangle` side 1) and no group icon, so DPS never follow the tank's pick.
    `Attack`, then walk to `GetAnubarakTankPatch(bot, side)` within `ANUBARAK_TANK_PATCH_ARRIVE` at
    `MOVEMENT_COMBAT` (no patch: hold at the side point), return false once there.
17. **Main tank.** `GetAnubarak`; unselectable → false. Victim not a tank → `CastClassTaunt` (probe
    `NoteDerived` `anub.pickup` `taunt`/`attack`/`hold`). `MarkTargetWithSkull`, rti `skull`, `Attack`,
    `DragBossToAnchor(boss, GetAnubarakBossAnchor(bot))`.
18. **Heal Penetrating Cold.** The lowest-health carrier below 90%; first direct heal `CanCastSpell`
    allows from the `JaraxxusHealIncinerateTargetAction` list.

### Group 3 — trace and doc

Files: `tools/botobs/bosses/anub_arak.py`, `tools/botobs/tests/test_anub_arak.py`,
`docs/raids/trial-of-the-crusader/anubarak.md`.

19. **Reader**, shaped like `bosses/general_vezax.py` (`run_sections`, a banner with missing probes,
    constants read with `radius()`/`anchor()` from the source). Boss is entry 34564, never the name:
    the spike (34660) is also "Anub'arak" and boss-flagged. Sections:
    - `phases`: `anub.phase` timeline, his health (`snap.u`) at each edge, P2 count.
    - `pull`: who stood west of the Web Door (x < 661.6) or above z 200 at t=0, i.e. locked out or
      never landed; time from `toc.progress` 9, when in the trace.
    - `spike`: each window of aura 67574 on the roster: kiter, `anub.kite` branches, seconds, outcome
      (mark gone with no Impale on the kiter = patch; Impale hit; death); Impale hits on everyone
      else (65919/67858/67859/67860) and `anub.dodge` rows.
    - `spheres`: flying (z > 150) and patch (z < 146) counts over time from 34606 rows, Permafrost
      `snap.hz` (66193/67855/67856/67857), patches that vanished within 3 s of a mark ending
      (consumed), `anub.sphere` shooters, heroic spheres left.
    - `burrowers`: per 34607 guid first/last seen, health, submerges (gone ≥ 5 s then back near full,
      or a jump up), share of samples within 6 yd + 1.5 of a Permafrost `hz` centre, Shadow Strike
      casts (`cast` 66134) and whether a 66134 `dmg` row followed, `anub.interrupter`.
    - `threat`: his target column by role per surfaced window, seconds from each emerge to a tank,
      taunts onto him, `anub.pickup`.
    - `swarm`: P3 length, Leeching Swarm damage (66240) per bot, lust casts (2825/32182) against P3
      start, Penetrating Cold carriers (66013/67700/68509/68510) and heals onto them,
      `anub.defensive`, deaths.
20. **Test**, as `tests/test_general_vezax.py`: a synthetic pull pinning each section's arithmetic,
    and the shared fixture reading empty without raising.
21. **Doc.** Run `compact-docs-writer` (standing approval). Add a section on how the bots play it:
    the ladder above and every entry of "Decisions for review" with its rationale. Replace the
    `AnubarakSubmerged` known gap once fixed, keep the lane's other known gaps, and replace "What a
    trace answers" with the `anub.*` key table and reader usage, as `docs/raids/ulduar/mimiron.md`.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Every node waits for the pull (`AnubarakEngaged`) | Script: `IsEncounterInProgress` from the Lich King scene; attackable pre-pull within 80 yd; Web Door shuts at the engage | Stage-open triggers, the main tank auto-pulling on sight |
| No landing or regroup node; the reader reports lock-outs and bots never landed | navprobe cannot model the water under the arena; no 649 traces | A regroup node now (carried over) |
| Emerge: pickup tank taunts only when a non-tank other than itself holds him, never with no victim; no redirect | Script never resets his threat and teleports him to the spike; Thorim (a taunt hands back to top threat after 3 s) | The brief's taunt or redirect on every emerge |
| Burrowers held on Permafrost in every difficulty | Script's submerge rule is not heroic-gated; guide "bring burrowers to your designated frost patch" | Heroic only, per the brief |
| DPS hit a burrower only while it carries Permafrost | Script: under 80% off Permafrost it submerges and returns at full health | Focus on sight (current); the guide's "die just before the next set" pacing (carried over) |
| Tank patches: the flying spheres nearest points 12 yd either side of (745, 135) at the pull; boss held at their midpoint | Guide: two spheres shot "one on Anub'arak's left and one on his right", boss "perfectly in-between two frost patches"; the numbers are conservative, navprobe-clean | Fixed drag to `ANUBARAK_PIT_CENTER` (current) |
| Room centre (745, 135) replaces `ANUBARAK_PIT_CENTER` (722.65, 135.41) as the fallback anchor | navprobe: the floor is centred there; 41 yd drag from his spawn against 63 | Keep the scarab spawn point |
| Heroic: refill a tank patch only while more than 2 spheres fly; a kite may always take one | Script: six spheres, no respawn; guide "use them wisely" | No reserve |
| Kiter stands 8.5 yd past the patch, away from the spike, not in it | Script: the fail test runs before Impale, 8 yd against 6; guide "sit outside the frost patch" | Run onto the patch (current), paying the 80% heroic slow |
| No patch: tangential 35 yd ring kite, and a shooter drops the sphere nearest the kiter | Script speeds; navprobe ring clean at 35 | 15 yd raw flee point (current), off-mesh risk |
| Non-kiters dodge the spike (13 yd) and its lane to the kiter (9 yd); the only hold guard is the side tank skipping its patch walk while the patch lies within 9 yd of the lane | Impale is a 6 yd cone; pitfalls: clearance past the trigger radius, a dodge past the hold's arrival tolerance | The guide's P2 raid positioning (carried over) |
| Kite veto keeps attack actions | action-selection: a blanket `MovementAction` veto kills targeting | Blanket veto (current) |
| Scarabs: generic targeting; tanks taunt them off non-tanks | Script: 20000 threat on a random player, a scarab per 4 s; guide "mindful of your aggro on Swarm Scarabs" | Melee focus the first scarab (current) |
| Shadow Strike: one interrupter per casting burrower, memoised per instance per ms: victim first, else lowest guid with a ready interrupt or stun within 30 yd, nobody picked twice; Holy Wrath in the list | Guide: AoE stuns (Shockwave, Holy Wrath), warriors and paladins; `-207` immunities cover silence, not interrupt or stun; burrowers are undead on every entry; 25H overlaps 8 s casts | Generic class interrupts; each bot checked against its nearest caster only |
| Phase 3: main tank chains `NextTankDefensive`; healers lead with Penetrating Cold carriers under 90% | Brief; guide "1-3 healers assigned solely to" Penetrating Cold | Generic |
| Lust only in phase 3, latched per instance; personal burst unrestricted | Guide: phase 3 damage taken; w0c's timing, now latched | Health < 30% or own aura, unlatched (current) |
| Scope 8 verified: entries resolve by base entry on every difficulty; 67574, 67539, 65981 have no row; Leeching Swarm remaps; `AnubarakSubmerged` dead (Known gaps) | DBC CSV, world DB, `GetFirstAliveUnitByEntry` source | - |
| Detour via the next corner of a square round the patch, sides 9 yd out (slow reach + 1 + 0.5), bot's side first; no clean corner: straight out. A leg clips only if it cuts deeper than either end | Geometry: from the brief's side point the leg on to the stand still passes 6 yd from the patch, inside the 8.5 test, so every tick re-issues the same detour | The brief's single perpendicular point |
| Stand floor check may move it 1.5 yd across, 3 yd in height, never into the 7.5 yd slow reach, else the next rotation; after a detour only the patch is kept; a kept stand must still have the spike behind it | A detour's stand is a waypoint with no bearing to compare | The 30° rule on every branch |
| Sphere shooters are bots only; a side takes only spheres within 30 yd of its point; the heroic reserve counts unassigned spheres; no kite need while a sphere falls within 30 yd of the kiter; kite prefers a sphere whose stand has the kiter ahead of the spike | A player never reads the assignment; a sphere further out lands where the latch never takes it; a falling sphere near the kiter is already its patch | Every ranged DPS, the nearest sphere, the raw flying count |
| Heroic from `Map::IsHeroic()` | The script reads the map's difficulty | `GetRaidDifficulty()` |
| A new pull is a phase read 5 s after the last; `anub.phase` 0 is never written | Gated triggers never read the phase while he is out of combat, so a not-engaged read never happens in practice | Reset on a not-engaged read (dropped: dead code) |
| Side tank taunts a loose burrower even while holding one; loose = its victim holds no burrower side, boss tank included | 10H brings two a wave to one side tank, 25-man four to two | One on this bot, else one on a non-tank |
| Side tank walks to its patch only once its pick is on it; an untaunted pick is attacked in place and the node returns false; with no pick (all submerged or on the other side) the hold node, guard and scarab exclusion step aside and rti goes back to `skull` | Walking off leaves it on its victim; the patch walk and reach melee trade the tank back and forth; action-selection: a targeting veto needs a node that assigns one; a leftover `square`/`triangle` steers `TankTargetValue` next fight | Attack, then walk; guard on any burrower alive |
| Every taunt (boss, burrower, scarab) checks `CanCastSpell` and the DBC range before `CastClassTaunt` | `CastSpell` selects and faces the target before failing, every tick for a taunt on cooldown | Bare `CastClassTaunt` |
| Kite and dodge break a channel pinning the feet and clear a `last movement` stalled 500 ms | A pinned `MoveTo` is a no-op, Impale does 14-20k; `MoveTo` refuses a repeat for up to 5 s after a potion or knockback (Hodir's `ReleaseStalledWalk`) | Kite only, per the brief |
| Dodge spot latched until within 2 yd or 1 s without the action running | Out of danger in between makes the spot stale | Until reached or out of danger |
| Interrupt reach from the spell's DBC range: melee or self range walks to contact, ranged to range - 2 with line of sight | War Stomp and Shockwave only reach adjacent units | One fixed range |
| Shadow Strike: the victim counts only if unmarked | The kite multiplier zeroes the interrupt (a `MovementAction`) for the kiter, so nobody would interrupt | Victim regardless |
| Focus burrower skips a ranged DPS with sphere duty; the scarab node yields to the hold node while a burrower lives; the tank target guard also zeroes `DpsAssist` for a sphere shooter | Otherwise two nodes, or a node and `dps assist`, swap the target every tick; a flying sphere never enters `attackers` | - |
| Penetrating Cold target within heal range and line of sight, one helper for trigger and action; `lesser healing wave` replaces `greater healing wave` | Trigger and action must agree; `greater healing wave` is not a 3.3.5 spell, and the Jaraxxus list still has it (w6 hoist) | Copy the list as is |
| One sweep per instance per ms for spheres, spike, burrowers and scarabs, from the boss when he resolves; the add helpers live in `Util/ToCHelpers_Anubarak` | A bot off the floor can't blank the shared cache; guard, triggers and Shadow Strike duty read one burrower set | Two sweeps (bot-origin and boss-origin), helpers in the trigger stem |
| From 65 s after the pull or an emerge the pickup tank leads him onto a spot 18.75 yd from every patch (spike reach 11.5 + 5.25 first-tick walk + 2 yd arrive), within 30 yd of the tank and 35 of the room centre; every drag (this and the anchor) arrives within 2 yd measured on him, not the shared 12 yd tank stop | Script: submerge at 80 s, spike summoned on him, first test 1.5 s later at 3.5 yd/s; `IsWithinDist` adds 1.5 + 2.0; guide: dragged away 15 s before submerge | Shared `DragBossToAnchor` 12 yd stop, which parks him on a tank patch |
| Kite prefers non-tank patches in every difficulty while a burrower lives; heroic first wants a stand whose spike route (spike to kiter, kiter to stand) passes no tank patch within 11.5 yd | A held burrower off Permafrost submerges back to full; a tank-patch stand puts the side tank in the spike's lane; the spike takes any patch it passes | Heroic-only preference, destination only |
| Side tanks latched per pull: first two living tanks (strategy or spec) after the stable main tank (explicit, else first living tank so defined), assistants first; re-latched only when dead, gone or now the main tank. Pickup tank reads the same main tank | pitfalls: `IsTank` reads false for a tick; an unlatched index swaps sides and trades burrowers | `IsAssistTankOfIndex` every tick |
| Sphere shooters must stand in the pit: within 60 yd of the room centre and below z 200 | The Web Door shuts at the engage (83 yd out); a locked-out or never-landed shooter would hold its need forever | Line of sight to the assigned sphere |
| The kite drops a `FORCED` booking to `COMBAT` for a new point, restored when nothing is issued | `IsWaitingForLastMove` refuses an equal priority, so a re-aim or the first leg after a dodge waits out the stale leg (Flame Leviathan drive) | Wait it out |
| Kite veto also zeroes Blink and Disengage | Spell movers, not `MovementAction`s; `EnemyTooCloseForSpell` fires them off a scarab | - |
| `anub.spike` dropped; the phase read writes `anub.kite none` for the last kiter when the mark moves | The 67574 `aura` stream already carries the mark (observability: don't probe what another stream says); without the close, a repeat kiter's window opened on its last branch | Brief item 4's `anub.spike` |
| `anub.interrupter` written on every trigger check, `0` once the cast ends; `anub.pickup` gains `submerge` | A reader counts duties per cast; hold and the pre-submerge drag are different points | - |

## Carried over

To [w5-anubarak-rest.PLAN.md](w5-anubarak-rest.PLAN.md): Hand of Protection or immunity for a kiter
with no patch, phase 3 raid-health economy and cancelaura, Freezing Slash defensives, burrower pacing
and "ignore burrowers when he is low", a landing/regroup node, scarab threat and Mandible stacks,
Spider Frenzy spacing on 25H, the guide's phase 2 raid positioning.

To the merge stage or `w6-closeout`, outside this lane's files: `ANUBARAK_PIT_CENTER`
(`Util/ToCData.{h,cpp}`) has no users left, so drop it and its mention in the README's code layout;
the README's "Nodes keyed on these" Anub'arak rows are stale (new Penetrating Cold node, the sphere
node keyed on sphere duty, the lust hold on the latched phase 3), as is its bot-tractable line on
spike kiting. A raid-wide reset of the `rti` marks ToC nodes leave behind (`cross`, `square`,
`triangle`) that runs after a kill, when the stage gate has closed every boss node.

## Known gaps

- Raid AoE can pop spheres; nothing guards them, which costs heroic's budget.
- 10H with one burrower tank puts both burrowers on one patch, inside Spider Frenzy's 12 yd.
- The direct-heal list is copied from `JaraxxusHealIncinerateTargetAction`; `w6-closeout` can hoist
  it into `Shared`.
- The spike dodge ignores Permafrost, so a dodge can land in heroic's 80% slow.
- Sphere and Shadow Strike duty are memoised for one group per instance per ms; two groups in one
  instance recompute them on every switch. Side tanks are latched per instance.
- A side tank's `square`/`triangle` and a DPS's `cross` outlive a kill: stage 10 closes every
  Anub'arak node before the leftover clause runs. Needs a reset outside the stage gate.

## Review

Open issues after re-review: none. Deferred: the rti leftover above, outside conformance-4's asked
fix; recorded in `anubarak.md` and carried over as the raid-wide rti reset.

## Blocked

None.
