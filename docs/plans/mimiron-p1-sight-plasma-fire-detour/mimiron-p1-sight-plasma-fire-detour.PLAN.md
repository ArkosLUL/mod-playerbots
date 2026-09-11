# Mimiron Firefighter 2026-09-11 night: phase 1 camp line of sight, Plasma Blast cooldown order, fallback into Shock Blast, fire detour for the formation

## Context

Three Firefighter 25 pulls on an image built 16:22:57 UTC, 5.6 min after `e165ce049` (one core per
corpse, fire bots kept, Frost Bomb reach veto, p3 melee hold; confirmed live by `p3hold` and
`corestep` notes). Traces in `ac-worldserver:/azerothcore/env/dist/logs/botobs/`:
`603_1_mimiron_1789144166.ndjson` (**e1**), `..._1789144656` (**e2**), `..._1789145170` (**e3**);
comparison d1/d2 = `1789137577`/`1789138469`. Reader `tools/botobs/obstrace.py`; scratch scripts in
the session scratchpad (`mm.py` loader, `los1.py`, `losmap.py`, `losreplay.py`, `notgt.py`,
`plasma.py`, `p2open.py`, `burstdeath.py`, `p3verify.py`, `slotdist.py`, `detour.py`).

| pull | result | p1 (MK II 8.28M) | p2 (VX 8.28M) | p3 (ACU 5.52M) | p4 |
|---|---|---|---|---|---|
| e1 | wipe p2 4:57, VX 24.6% | 106.2 s, 78k | 48k | - | - |
| e2 | wipe p2 3:54, VX 56.5% | 93.8 s, 88k | 46k | - | - |
| e3 | **berserk** 10:02 | 84.2 s, **98k** | 104.3 s, **79k** | **129.2 s**, 42.7k | 179 s, 52k; MK 6%, VX 6%, ACU 12% left (~1.66M, ~32 s) |

User decisions: the raid **keeps following the master** in the 1→2 handover (document where the master
should stand); the **fire detour ships in this round**; the **phase 1 tank spot stays**, the fix for
line of sight goes into the ranged camp.

Offline LoS: `navprobe` has no line-of-sight mode, so a scratch probe (`scratchpad/los/losprobe.cpp`,
built by `los/build.sh` in `acore/ac-wotlk-build:master` against `build/src/common/libcommon.a`)
calls `VMAP::StaticMapTree::isInLineOfSight`, the static half of `Map::isInLineOfSight`, on map 603 with
the `ac-client-data` volume. Rays mirror `WorldObject::IsWithinLOSInMap` (player eye = pos + collision
height from DBC, draenei 2.46-2.54; creature end = hit-sphere point, MK II collision height 2.031,
combat reach 8).

## Findings

### 1. Phase 1: the camp's edge slots lose sight of the MK II (user's observation, confirmed)

- `AttackersValue::IsValidTarget` = `IsPossibleTarget && bot->IsWithinLOSInMap(target)`
  (`src/Ai/Base/Value/AttackersValue.cpp:247`), so out of LoS is an **invalid target**:
  `invalid target` → `drop target` (99) → `DropTargetAction` calls
  `ChangeEngine(BOT_STATE_NON_COMBAT)` (`ChooseTargetActions.cpp:63`) → `follow` (1.0) walks toward the
  master while `mimiron arc spread action` walks back: **follow <-> arc spread 58 / 49 A-B-A** (e1/e2),
  Elemena (slot 5/14) and Trueshot (10/14) 24-31 A-B-A each, 20-35 s without a target. Trueshot
  4.7k / 5.2k dps against a ranged median of 5.8k / 7.1k.
- Geometry (losmap): the tank spot (2691.576, 2568.532) sits ~3.4 yd **inside the doorway alcove**,
  whose side walls end at x ≈ 2695, y ≈ 2553 and 2585 (ray hits at z 366-367). The camp's edge
  bearings (292° and 68° off the tank spot, 27-33 yd) sit on the shadow line, and the MK II is west
  of the tank spot 45% of the time (median 5.4 yd off it), which widens the shadow.
- Replay of 323 traced MK II positions (d1, d2, e1, e2, e3) against all slots of both stack anchors
  (`losreplay*.py`, with `MimironShiftIntoRange`), tank spot unchanged (user decision):

  | camp | north (anchor 0) worst slot blind | south (anchor 1) worst | spacing | anchors apart |
  |---|---|---|---|---|
  | shipped, 30° / 330°, ±38° | 21% | 38% | 6.0 | 27 yd |
  | camp hub slid 4-8 yd east | 20% | 38% (the range clamp drags it back) | 6.0 | 27 |
  | half-angle ±32° | 1.5% | 35% | 5.8 | 27 |
  | **25° / 345°, ±38°** | **1.5%** | **1.2%** | 6.0 | 18.5 |
  | 25° / 345° with the MK II 2 yd further west (stress) | 21% | 34% | 6.0 | 18.5 |
  | 25° / 345° + sight turn (below), stress | clear after ≤ 4° in 98% | clear after ≤ 8° in 98% | 6.0 | - |

  The south shadow is the wider one because the MK II drifts south-west. Slots within 15 yd of the MK II
  (Shock Blast) stay at 12-14% of the time and the nearest slot stays ~20 yd off the room centre in
  every variant. (Moving the tank spot itself 4 yd east also worked, 1.5% / 0%, but the tank spot
  stays.)
- Melee also drop target after Shock Blast flees behind the south wall (e1 1:05-1:21, 8 drops); a few
  seconds each. Watch item.

### 2. Phase 1: the third Plasma Blast window gets no defensive (e1 tank death 1:17.7)

`plasma.py`: windows at ~0:23, 0:45, 1:08, 1:30, 1:52. Window 3 (1:08) had **no button in all three
pulls** (101.5k / 92.4k / 105.6k taken, min HP 46% / 78% / 48%); e1's tank died. Why:
- The paladin tank casts Avenging Wrath at the pull (0:14-0:22), which locks Divine Protection for
  30 s, so window 1 always goes to a healer external (PS or HoSac).
- Window 2 got **two** buttons in every pull: e1 HoSac 0:45.2 + DP 0:53.2; e2 DP 0:45.2 + PS 0:54.0
  (and Lay on Hands 0:44.9); e3 PS 0:45.6 + DP 0:53.8.
  - The DP at 0:53 is the **class node** firing on health (`MimironPlasmaBlastDefensiveAction` only
    casts through `ClaimMimironPlasmaWindow`, but nothing holds the class nodes; OS does, via
    `IsHeldTankDefensive`, `src/Ai/Raid/OS/OSMultipliers.cpp:146`).
  - e2's second claim at 0:54 got through because `ULDUAR_MIMIRON_PLASMA_WINDOW_MS` is 8000 while cast
    start to last tick is ~9 s (e3: cast 0:45.24, ticks 0:49.2-0:54.2).
- With both fixed, replaying the three pulls: w1 healer, w2 DP or the other healer, w3 whichever is
  left. Windows 4-5 stay bare, as they were (min HP 74-82%).

### 3. Phase 1: the fire dodge's unfiltered fallback walked Smartface into Shock Blast (e1 1:08.4)

At 1:06.77 `flames+10` refused every bearing (mine 3, fire 5, shock 3) and took `fallback`, the
unscreened `MoveTo` along the away bearing, to 9.9 yd from the MK II mid-cast. The leg is
`MOVEMENT_FORCED`, so the Shock Blast escape logged `shock locked` at 1:07.4
(`UldActions_Mimiron.cpp:235-250`). 78k, dead.

### 4. Phase 2 opener: the stacked raid 40+ yd from VX-001 eats every Rapid Burst tick (e2 wipe)

`p2open.py`, first 30 s of phase 2:

| pull | raid at start | spread | Rapid Burst dmg | victims per tick | deaths |
|---|---|---|---|---|---|
| c1, c2, e1, e3 (human near centre) | 3.6-15 yd from VX | 2.6-4.9 | 324-399k | mostly 1-3 | e1: 1 at 13 s |
| d2 (human west) | 40 yd | 2.8 | 653k | 10-22 early | 0 |
| e2 (human west) | 42.8 yd | **1.6** | 679k | **24, 21, 16, 14** | **6 (4 healers)** |

The Rapid Burst dodge (`MimironRapidBurstTrigger`) walks only while the arc out of the 30° half-cone
is under `ULDUAR_MIMIRON_RAPID_BURST_MAX_STEP` (9 yd), i.e. within ~13.6 yd of VX on the centreline;
at 40 yd every bot stands and eats all six ticks while walking east to the wedge along the line. **User
decision: keep following.** Document that the master should stand near the room centre for the 1→2
handover.

### 5. Phases 2-4: the formation is abandoned because the fire is at the tracker cap

- Fire nodes (snapshot, capped at 40): 34-40 by the end of the 1→2 handover in every pull; phases 2-4
  median 30-39.
- Ranged/heal distance from their slot: e3 p2 **14.6 yd** median (59% > 10 yd), p3 **23.5** (71%),
  p4 **22.0** (78%); d2 p2 7.7 when the first Frost Bomb cleared 37 of 37.
- `MimironArcSpreadTrigger`/`Action` refuse the slot when the slot fails `IsMimironSpotSafe` or the
  straight walk fails `IsMimironWalkFireSafe` (`UldTriggers_Mimiron.cpp:149-152`,
  `UldActions_Mimiron.cpp:532-535`).
- `detour.py` over off-slot bot-seconds (e3 p2 / p3 / p4):

  | why the bot is off its slot | e3 p2 | e3 p3 | e3 p4 |
  |---|---|---|---|
  | walk blocked, a two-leg detour exists | 17% | 20% | 20% |
  | walk blocked, no detour | 21% | 19% | 18% |
  | slot burning, clear point ≤ 6 yd reachable | 28% | 24% | 14% |
  | slot burning, no reachable clear point | 10% | 31% | 11% |
  | walk clear (off slot for other reasons) | 24% | 6% | 37% |

- **Downstream costs:**
  - Phase 3: `flame dodge <-> reach spell` **89** A-B-A.
  - Phase 3: the main tank stood 16.5 yd off the room centre, because the p3tank slot (centre) is burning or its walk is blocked. Bulwark's arc spread first fired at 6:07.
  - e1 4:00: four ranged were sent by `reach spell` onto one point, (2725.5, 2536.7), at max range 38 yd, on the Rapid Burst centreline, and died together.

### 6. `e165ce049` scorecard (e3 phase 3 unless noted)

| check | result |
|---|---|
| cores | **2 uses, 2 corpses**, 34.3 s apart; landings 5:10-5:26 and 5:46-5:57 |
| adds | fire bots 3, Assault 2, Junk 3, Bomb 1; none after 5:14 (landings freeze events ~45 s each, as modelled) |
| fire bots | one culled at 4:47.8, the kept two died 6:03.6 / 6:24.5, **none in phase 4** |
| Water Spray / siren | **1** hit (Trueshot 24k) / 18 applications, 6 on casters and healers, most in the 3 s after spawn (d2: 45, 19) |
| Frost Bomb on bots | 0 in phase 2 (the human died to one at 3:28; Fel at 9:58 in phase 4) |
| melee chase | flame dodge <-> reach melee **1,034 → 28**; `p3hold` 18; fallback 0 |
| ranged wedge | arc spread <-> reach spell **100 → 9**; median 3D distance to ACU 26.4 (p90 33.9) |
| tank at centre | **missed**: median 16.5 yd (finding 5) |
| pace | **129 s / 42.7k** (target ≤ 120 s / ≥ 45k); ACU lost 21% and 33% in the two landings, 32k dps airborne |

### 7. Deaths

| pull | phase 1 | phase 2 | phases 3-4 |
|---|---|---|---|
| e1 | Smartface (finding 3), Bulwark (finding 2), human (mine) | 25: Heat Wave/Rapid Burst 4:01-4:02 (5 ranged on one point, finding 5), Rocket Strike 1, **Flames 14** 4:27-4:47 | - |
| e2 | - | **6 in the opener** (finding 4), 16 at 3:33-3:51 (Heat Wave/Rapid Burst) | - |
| e3 | - | human (Frost Bomb), Flames 5 at 3:54-4:06 | p4: 2 melee to mines 7:49, Hand Pulse/Plasma Ball 3 bots (4 deaths) at 8:36-8:44, Frost Bomb 1, then Self-Destruction |

Phase 4 (e3): the ACU sat on Agony (warlock) 70% of the time (Plasma Ball 233k, survived); Hand Pulse
56-82k on most of the raid.

### 8. Oscillations

| pair | e1 | e2 | e3 |
|---|---|---|---|
| p1 follow <-> arc spread | 58 | 49 | 0 |
| p1 flame dodge <-> reach melee | 116 | 52 | - |
| p2 arc spread <-> rapid burst | 24 | 24 | 31 |
| p2 flame dodge <-> reach melee | 71 | 31 | 8 |
| p3 flame dodge <-> reach spell | - | - | 89 |
| p4 combat formation <-> barrage / arc spread <-> frost bomb | - | - | 44 / 38 |

## Changes

All in `src/Ai/Raid/Uld/`. Match surrounding style (CRLF in `UldEncounter_Mimiron.cpp` and
`UldMultipliers_Mimiron.*`); comments state mechanism, no history.

### 1. Turn the phase 1 camp out of the alcove's shadow; tank spot unchanged

**Static turn.** `Util/UldEncounter_Mimiron.cpp:44-47`, the two `ULDUAR_MIMIRON_PHASE1_STACK_SPOTS` become the middle-row centres (27 yd) at the new bearings off the unchanged tank spot:
- 25°: `(2716.0464f, 2579.9421f, 364.3138f)`
- 345°: `(2717.6562f, 2561.5434f, 364.3138f)`

Keep the bearing comments. The wedges then span 347°-63° and 307°-23°, both inside the navprobe-clean 288°-74° band.

**Sight turn**, the safety net for drift the traces have not shown:
1. **State.** `MimironFightState` gains `float campTurn`, `uint32 campTurnScanMs` and `uint32 campTurnRaisedMs`, reset with the rest.
2. **`MimironPhase1CampTurn(Player* bot, Unit* focus, Position const& anchor, uint32 count)`** (anonymous namespace). At most once per `ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS`:
   - Find the smallest turn in `ULDUAR_MIMIRON_CAMP_SIGHT_STEP` (4°) steps up to `_SIGHT_MAX` (16°), toward the bearing from the tank spot to the room centre, at which every slot sees `focus`.
   - A slot is built exactly as in `MimironPhase1CampSlot`, including `MimironShiftIntoRange`.
   - "Sees" is `Map::isInLineOfSight(slot + _SIGHT_EYE (2.0), focus->GetHitSpherePointFor(slot + eye), focus->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS, ModelIgnoreFlags::Nothing)`, the ray of `IsWithinLOSInMap` from a player. The fixed eye height keeps every bot on the same answer; the replay showed no difference between 1.2 and 2.5.
   - Raise the turn at once. Lower it only after `_SIGHT_HOLD_MS` (15000) since the last raise, so the camp does not swing with every MK II step.
   - Keep the current turn when no step clears.
   - Note `mimiron.campturn <deg>` on change.
3. **Use.** `MimironPhase1CampSlot` gains `bot` and applies the turn to its centreline before building slots.

**Comments:**
- Rewrite the camp comment at `UldEncounter_Mimiron.h:151-165` and the stack spot comment. The camp keeps its wall-side edge off the alcove: the tank spot sits ~3 yd inside the doorway alcove, the MK II drifts further in, and a bot that loses sight of its target drops it and walks to its master.
- Note the 18.5 yd anchor separation (still clears a 5 yd cluster plus a 7 yd chain step).

### 2. Plasma Blast: one button per window, window 3 covered

- `Util/UldEncounter_Mimiron.h:189-191`: `ULDUAR_MIMIRON_PLASMA_WINDOW_MS` 8000 → **12000**; fix the
  comment (cast ~4 s then six 1 s ticks, ~9 s start to last tick; next cast 22 s later).
- New `MimironPlasmaDefensiveHoldMultiplier` (`Multiplier/UldMultipliers_Mimiron.{h,cpp}`, registered in
  `UldStrategy.cpp` beside the other Mimiron multipliers): returns 0 for the **main tank** on any action
  where `IsHeldTankDefensive(action->getName())` while phase 1 is live (MK II alive, VX-001 and ACU
  not). Pattern: `OSMultipliers.cpp:143-147`. `MimironPlasmaBlastDefensiveAction` casts through
  `botAI->CastSpell` directly, so it is unaffected. Include `RaidTankDefensive.h`.

### 3. The unfiltered flee fallback never walks into a Shock Blast or Frost Bomb

`Action/UldActions_Mimiron.cpp:233-250` (`FleeFan` fallback):
1. Before either fallback move, build the straight-away point `bot + (cos away, sin away) * distance`.
2. If it fails `IsMimironSpotShockSafe(botAI, point)` (only when `screenShock`) or `IsMimironSpotBombSafe(hazards, point)`, note outcome `"unsafe"` with the refusal counts and return false.
3. Standing in fire (~3.1k/s) beats a 78k Shock Blast or a 47k Frost Bomb, and a false return leaves the movement lock free for the Shock Blast escape.

### 4. Formation approach: substitute a burning slot, detour a blocked walk

New util in `Util/UldEncounter_Mimiron.{h,cpp}`:

```cpp
// Where the formation should send this bot next on its way to `slot`, preference order.
std::vector<MimironApproach> GetMimironSlotApproaches(PlayerbotAI*, Player*, Position const& slot,
                                                     MimironFirefighterHazards const&);
struct MimironApproach { Position dest; char const* how; float angle; };  // how: direct/substitute/detour
```

**Spot screen** for candidates, from pre-gathered hazards (one hazard gather per call; do not call `IsMimironSpotSafe`, which re-gathers):
- `IsMimironSpotMineSafe`
- the Rocket Strike marker clearance
- `IsMimironSpotShockSafe`
- `IsMimironSpotFireSafe`, `IsMimironSpotBombSafe`, `IsMimironSpotFireBotSafe`
- `IsMimironSpotRapidBurstSafe` against the live window

Factor the marker and shock part out of `IsMimironSpotSafe` so both share it.

**Steps:**
1. **Goal.**
   - If the slot passes the screen, the goal is the slot.
   - Otherwise, outside phase 1 (Napalm's 5 yd pairs make phase 1 substitutes unsafe), the goal is the first screened point on rings of 2, 4 and 6 yd (`ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS` = 6) × 12 bearings round the slot that is ≥ `ULDUAR_MIMIRON_DISPERSE_DISTANCE` (5.5) from every other living group member.
   - With no goal, return empty.
2. **Direct.** If `IsMimironWalkFireSafe(bot, hazards, goal)`, emit `{goal, "direct" or "substitute"}`.
3. **Detour.**
   - For δ in 20°, 35°, 50°, 65°, on both sides: waypoint W = bot + dir(bearing ± δ) · (D/2)/cos δ, where D is the bot-to-goal distance.
   - Keep W when it passes the spot screen and both legs (bot→W, W→goal) are fire-safe. This needs a from-position overload of `IsMimironWalkFireSafe`.
   - Emit every kept W in that order.

**Trigger** (`UldTriggers_Mimiron.cpp:139-165`), replacing the refusal at `:149-152`. Tank anchor slots keep their exemption.
- Inactive when:
  - the bot is within `ULDUAR_MIMIRON_SPREAD_TOLERANCE` of the slot;
  - or the slot fails the screen but the bot's own spot passes it within 6 yd of the slot (parked on a substitute: no chasing a substitute that moves as fire grows);
  - or there are no approaches.
- The Rapid Burst slot check is kept.

**Action** (`UldActions_Mimiron.cpp:522-539`):
- Walk the approaches in order with `TryMoveTo(..., MOVEMENT_COMBAT)` until one is `Issued`, mirroring `FleeFan`. The next tick recomputes from the new position, and the equal-priority lock commits the bot to a leg.
- Note `mimiron.approach` (`substitute r4`, `detour +35`) on each non-direct issue.

Expected from `detour.py`: roughly a third to a half of e3's phase 2-4 off-slot bot-seconds recovered.

## Docs

Save this plan first to
`docs/plans/mimiron-p1-sight-plasma-fire-detour/mimiron-p1-sight-plasma-fire-detour.PLAN.md`. Run
`/compact-docs-writer` (fresh invocation) before editing.

`docs/raids/ulduar/mimiron.md`:
- Phase 1: the anchor table (25°, 345°, 18.5 yd apart), plus a paragraph on the alcove LoS (mechanism, the replay table, the sight turn, the offline probe).
- The Plasma Blast cooldown order: the AW lockout, the class-node hold, the 12 s claim.
- The fallback screen.
- Phase 2 opener: the table and "master stands near the centre for the 1→2 handover".
- Formation approach (substitute/detour) and the fire-at-cap numbers.
- The `e165ce049` scorecard.
- The DPS table and the berserk miss (~1.66M).
- Trace keys: flee outcome `unsafe` and note `mimiron.approach`.

`docs/engine/pitfalls.md`, two paragraphs:
- An out-of-LoS target is an invalid target: `drop target` switches the bot to the non-combat engine, where `follow` competes with any formation.
- navprobe answers the floor, not sight: `StaticMapTree::isInLineOfSight` in the build image's `libcommon.a` is what to probe, with the ray shape of `IsWithinLOSInMap`.

## Verification

Per-TU `-fsyntax-only` on every changed TU (scratchpad `syntax2.sh`), then the worldserver rebuild,
then a Firefighter pull (the master stands near the room centre in the 1→2 handover):

1. **Phase 1 sight**:
   - no ranged/heal `drop target` in phase 1 before the MK II dies;
   - `follow <-> arc spread` < 5 A-B-A;
   - `los1.py` on the new trace shows no bot out of LoS more than 3 s;
   - `mimiron.campturn` notes rare (turn 0 most of phase 1), and no camp swing visible as `arc spread` A-B-A.
2. **Plasma**:
   - windows 1-3 each show exactly one of DP / PS / HoSac on the tank;
   - no DP outside a claimed window;
   - the tank survives window 3.
3. **Fallback**: no `flee ... fallback` issued into a Shock Blast; zero bot Shock Blast deaths.
4. **Formation**:
   - ranged/heal median off-slot in phases 2 and 3 ≤ 10 yd (e3: 14.6 / 23.5);
   - p3 `flame dodge <-> reach spell` < 40 (e3: 89);
   - main tank median ≤ 8 yd from the room centre in phase 3;
   - `mimiron.approach` notes present;
   - no new A-B-A pair above 30 involving `mimiron arc spread action`.
5. **No regression**:
   - Napalm ≤ 1.5 victims/cast and camp nn ≥ 5 yd;
   - phase-1 fire damage inside 20 yd of the room centre stays near 0;
   - 0 bot deaths in phase 1, ≥ 22 alive at phase 2 start;
   - core uses = Assault Bot corpses;
   - no fire bot in phase 4.
