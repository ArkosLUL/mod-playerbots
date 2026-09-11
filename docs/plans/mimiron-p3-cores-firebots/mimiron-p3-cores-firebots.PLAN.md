# Mimiron Firefighter 2026-09-11 evening: one core per corpse, fire bots kept and dodged, no walk-back into Frost Bomb, melee off the airborne ACU

## Context

Two Firefighter 25 pulls on an image built 13:57 UTC, 3.5 min after `6a06fe6bd` (per-bot phase 1
wedge, Shock Blast screen, fire-aware barrage orbit, arc spread walk hold, tank face veto, drink
guard, melee mine dodge after the MK II). Traces in `ac-worldserver:/azerothcore/env/dist/logs/botobs/`:
`603_1_mimiron_1789137577.ndjson` (**d1**), `..._1789138469` (**d2**); comparison c1/c2 =
`1789129048`/`1789129490`. Reader `tools/botobs/obstrace.py`; scratch scripts in the session scratchpad
(`mm.py` loader with tags, `verify.py`, `napalm2.py`, `osc.py`, `phdps.py`, `acu.py`, `cores.py`,
`firebot.py`, `fbdeath.py`, `firespread.py`, `botline.py`).

- **d1**: phase 1 13.1-112.3 s, phase 2 from 160.3 s, **wiped in phase 2** at 293 s, VX-001 at 21%.
- **d2**: phase 1 97.9 s, phase 2 124.5 s, phase 3 **231.4 s**, phase 4 from 571.3 s, **berserk**
  (25-man hard mode `EVENT_BERSERK` 10 min from engage, `boss_mimiron.cpp:357`; Self-Destruction from
  10:01) with all three parts at 43-46%. First pull to reach phases 3 and 4.

User decisions (AskUserQuestion): fire bots **kept alive through phase 3, dodged, killed before
phase 4**; handovers **keep following the master**; Frost Bomb veto covers **all reach moves**;
**strategy only**, core `boss_mimiron.cpp` untouched.

## Yesterday's list, scored (d1 / d2, c2 for contrast)

| check | result |
|---|---|
| Napalm victims per cast | **1.2 / 1.4** (c 3.67 / 7.25); 8 of 8 non-empty pools hit a pool member; only the 0:16 opener is still pool-empty (1 and 3 victims) |
| camp nearest neighbour | median **5.90 / 5.93 yd** (c2 0.59); share < 5 yd 25 / 30% (c2 88%) |
| Shock Blast | 0 bot deaths (d1's one is the human); flee fan refused 1 bearing each |
| barrage destinations in fire | 7 of 55 / 7 of 49 within 5 yd of a node (c2 8 of 24) |
| tank face, phase 1 tank | 6 / 8 moves (c2 20) |
| drink/food near fire | 0 of 4 / 0 of 23; drink guard vetoed 30 / 71 |
| mine deaths after MK II | d1 Assasin 0.2 s after it died (mine armed before); none later |
| alive at phase 2 start | **22 / 24** (c 14 / 11) |

## Why bots died

| phase | d1 | d2 |
|---|---|---|
| 1 | the human, Shock Blast | none |
| 2 | 22: Heat Wave/Rapid Burst 5, Frost Bomb 2 melee (3:42) + the human, **Flames 14** (3:51-4:38, healers 4:06-4:19) | 3: Frost Bomb 1 (Prayer), Heat Wave/Rapid Burst 2 |
| 3 | - | 3: **Water Spray 2** (Tree 24k, Druidica 22k), Bomb Bot 1 |
| 4 | - | 3 melee + the human to mines in fire at 9:41, 4 more to Hand Pulse/Plasma Ball/mines by 9:58, rest Self-Destruction |

### 1. Phase 3: the carrier minted 32 Magnetic Cores from 2 corpses (strategy bug)

`MimironMagneticCoreAction::Execute` (`UldActions_Mimiron.cpp:1234-1271`) creates a core with
`StoreNewItem` whenever the carrier stands near an Assault Bot corpse without one; nothing records that
the corpse gave its core. Justice looped `loot`/`use` every 0.4 s: **32 casts of 64444** from 5:36.1 to
6:00.3, 32 Magnetic Core creatures (34068) alive at once. It stopped only when the corpse despawned
(`TEMPSUMMON_CORPSE_TIMED_DESPAWN` 25 s). The trigger's only use gate is `MOVEMENTFLAG_HOVER`
(`UldTriggers_Mimiron.cpp:416`), which stays set for the 3 s before 34068 casts 64436, and is set again
by the script's 2 s lambda while an aura is still on.

Core mechanics (read from `boss_mimiron.cpp`, DBC): 34068 casts 64436 on itself 3 s after 64444; 64436
hits the ACU within 12 yd, lasts 20 s, +50% damage taken. Apply runs `DO_DISABLE_AERIAL` (`MoveFall`,
hover off, `DelayEvents(25s)`); remove runs `DO_ENABLE_AERIAL` (`MovePoint` z+16, 2 s lambda re-sets
hover). A second 64436 replaces the first, so remove and apply run in one tick: queued climb plus fall.
`UpdateAI` returns before `_events.Update` while the aura is on. Each landing therefore costs ~45 s of
add timers; each extra one another uncapped 25 s.

Consequences in d2: ACU Z bounced 364-388 from 5:41 to 6:07, hung at 387.7 idle until 6:23.9; **no
Bomb/Assault/Junk Bot after 5:48 and no second fire bot wave** for the remaining 3 min; the second
Assault Bot's corpse (6:27.5) went unused; with no fire bots after 6:20 the swept fire went from 9 to the
tracker's 40 cap by 6:55 and stayed there through phase 4.

**Strategy or core:** strategy. The script is fine with one core per landing, which is all a raid
gets. Its fragility on overlapping cores is documented, not patched (user decision).

### 2. Phase 3: melee and tanks chase the airborne ACU into fire

`mimiron dodge flames action <-> reach melee`: **1,034** A-B-A, melee/tanks 20-23 reversals/min,
340-410 yd/min, melee 0.9-1.8k dps. In `MimironSetDpsPriorityAction::ResolveTarget`
(`UldActions_Mimiron.cpp:1011-1067`) the hold keeps a **disallowed** current target when nothing is
allowed (both indexes equal `priority.size()`): melee entered phase 3 on `held:acu`. With no add up it
falls back to the generic `dps target`, the airborne ACU (`fallback` from 6:33 on).

The main tank's `p3tank` slot is the room centre (`UldEncounter_Mimiron.cpp:1165-1170`), but Bulwark's
median phase 3 position was (2740, 2582), 13.7 yd off, in the same loop. The ACU chases its victim
(`AttackStartCaster(who, 30)`) and drifted with him, while `p3wedge` slots are fixed around the centre
(`:1092-1105`, comment claims the ACU "has no attack" and range buys nothing: wrong, Plasma Ball P1 on
its victim via `DoSpellAttackIfReady`, **462k** to Bulwark in phase 3). South slots sat **35-45 yd 3D**
from it (hovers ~16 yd up): `arc spread <-> reach spell` **100** A-B-A; Smartface 91 reach-spell moves
vs 43 arc spread, 38% casting.

### 3. Phase 2: Frost Bomb flee undone by reach moves (3 deaths)

`frost bomb <-> reach melee` 114 / 150, `<-> set behind` 67, `<-> reach spell` 61, `<-> reach party
member to heal` 19. Trigger `FindNearestCreature(34149, 30)` (bounding radii added), flee to 34 yd
exact, then a reach move walks the bot back. `MimironChargeGuardMultiplier` vetoes `reach melee` only
while that trigger is active; heal and spell reach never. d2 Prayer ping-ponged 9 s between 32 and 17
yd, the last flees refused by the Rapid Burst cone, and took **45k at 24.8 yd**. Explosion 65333:
47,125 damage, knockback, radius 30 (creature caster, centre to centre), and clears flames within 30.

### 4. Phase 2 (d1 wipe): fire split during the handover

Both pulls hit the 40-node cap by 2:31. d2: the human stayed west, the raid followed (median radius
1.5-2.5 yd), fire compact (median radius 8 yd); the first Frost Bomb cleared **37 of 37** and phase 2
ran on 4-21 nodes. d1: the human walked ~45 yd north, the raid followed, fire split (median radius 24.6
yd); the bomb cleared 9, the rest stayed at the cap; healers burned 4:06-4:19. Same VX-001 schedule in
both. The raid follows the master between phases by design (`UldEncounter_Mimiron.h:413-418`); user
keeps it. Documented as a lever for the human: stand still in the handovers.

### 5. Emergency Fire Bots

AzerothCore (`npc_ulduar_emergency_fire_bot`, `boss_mimiron.cpp:2362-2407`): never attacks or chases
players; every 15 s walks to the nearest Flames (Spread) and sprays on arrival. Water Spray 64619 is
`SPELL_ATTR0_CU_CONE_LINE`: a ~2.3 yd half-width line 15 yd ahead, douses flames and does
18,850-21,150 frost (+25% Emergency Mode) plus knockback to players in it. 25-man (34148): 50.4k HP
(~63k with Emergency Mode) and the **Deafening Siren** aura 64616 (10 yd silence, 25-man only via
`creature_template_addon`). Waves of 3 from the ACU's `_events` every 45 s, first ~8 s into phase 3;
survivors carry into phase 4; phase-3 defeat stops new waves.

Real players (2009): most guides kill them (silence, knockback into fire), some keep one or two alive
for fire control, all agree none may be alive in phase 4.

Us today: on the kill list after the Assault Bot for melee and ranged (`UldActions_Mimiron.cpp:896-898`).
d2: all three dead by 5:38 / 5:45 / 6:20 but slowly; the siren hit **19 players 45 times** (3 healers),
Water Spray hit 3 and killed 2. `conf/playerbots.conf.dist:420-421` and `UldHardMode.h:66-67` claim
bots leave them alone.

## Oscillations

| pattern | where | count | cause |
|---|---|---|---|
| flame dodge <-> reach melee | d2 p3 | **1,034** | finding 2 |
| frost bomb <-> reach melee / set behind / reach spell / heal | p2 | 114-150 / 67 / 61 / 19 | finding 3 |
| arc spread <-> reach spell | d2 p3 | 100 | finding 2, wedge out of 3D range |
| arc spread <-> rapid burst | p2 | 24 / 32 | not examined |
| follow <-> arc spread | d1 p1 | 51 | 2 bots lost the target (`drop target`) at wedge edge slots near the 270 floor hole, 20 s each |

Gone since `6a06fe6bd`: tank arc spread <-> tank face (36 -> 0), arc spread <-> flame dodge (63/179 -> 7/1).

## Positioning and DPS

MK II median 5.4 / 5.4 yd off the tank anchor, 0% phase 1 fire within 20 yd of the centre, ranged median
24.3 / 25.7 yd from it. Phase 1 `drop target` is rare (2-4 bots, edge slots at bearing 282-299, 72-75).

| phase | d1 | d2 |
|---|---|---|
| 1 MK II 8.28M | 99.2 s = **83k** | 97.9 s = **85k** |
| 2 VX-001 8.28M | 79% in 133 s = 49k (wipe) | 124.5 s = **66k** |
| 3 ACU 5.52M | - | 231 s = **24k** |
| 4 3 x 50% = 11.0M | - | ~40k in the 29 s before berserk |

13 s pre-phase + 104 s of handovers leave 483 s for 33.1M: **~69k sustained**. Phase 3 is the binding
constraint (no landing, melee idle, ranged out of range); at 69k it is ~80 s and phase 4 gets ~180 s.

## Changes

All in `src/Ai/Raid/Uld/`. Match surrounding style; comments state mechanism, no history.

### 1. Magnetic Core: one core per corpse, one per landing

- `MimironFightState` (`Util/UldEncounter_Mimiron.cpp:432`): add `std::unordered_set<ObjectGuid>
  coreCorpses`, cleared by `ResetMimironFightState`.
- New util `Creature* GetMimironCoreCorpse(PlayerbotAI*, Player*)`: nearest dead `NPC_ASSAULT_BOT`
  within `ULDUAR_MIMIRON_CORE_SEARCH_RANGE` (`GetCreatureListWithEntryInGrid`, filter `!IsAlive()`),
  not in `coreCorpses`, whose `loot` holds an unlooted `ITEM_MIMIRON_MAGNETIC_CORE` or no entry for it
  (loot never filled). A looted entry means someone took it: skip.
- New util `bool TakeMimironCore(Player*, Creature* corpse)`: claim the guid (under the fight-state
  mutex), mark the loot entry `is_looted`, `--unlootedCount`, `NotifyItemRemoved(index)`, and when
  `loot.isLooted()` clear `UNIT_DYNFLAG_LOOTABLE` (mirror the creature branch of
  `WorldSession::DoLootRelease`; verify), then `StoreNewItem` as today.
- New util `bool IsMimironCoreUseReady(PlayerbotAI*)`: ACU alive, `!IsMimironAcuGrounded`, no live
  `NPC_MAGNETIC_CORE` (34068) in range (it lives 25 s from use, covering the 3 s arm, the 20 s aura and
  the 2 s climb), and the ACU has `MOVEMENTFLAG_HOVER`.
- `MimironMagneticCoreTrigger::IsActive`: carrier, ACU alive, and either (no core, `GetMimironCoreCorpse`)
  or (core, `IsMimironCoreUseReady`). Action takes the same two branches; new `corestep` values
  `pending` and `claimed`.

### 2. Phase 3 melee and tank off the airborne ACU

- `ResolveTarget` (`Action/UldActions_Mimiron.cpp:1041`): hold only when `IsAllowedTarget(currentTarget)`.
- `Execute` (`:1080`): a `p3hold` mirroring `p4hold`: non-healer melee, ACU alive and not grounded,
  not phase 4, nothing allowed -> `AttackStop()`, note, return true. `ResolveTarget` must report
  "nothing allowed" to this caller instead of returning the generic `dps target`; others keep the fallback.
- `MimironTankAnchorGuardMultiplier` (`Multiplier/UldMultipliers_Mimiron.cpp:144`): also zero `reach
  melee` for the main tank in phase 3 while the ACU is alive, airborne and not phase 4, so `p3tank`
  holds the centre.

### 3. Phase 3 wedge within 3D range of the ACU

`GetMimironPhase3Slot` (`Util/UldEncounter_Mimiron.cpp:1048-1107`): keep the centre anchor and
centreline, then rigidly shift the wedge toward the ACU's ground point (up to 3 passes) while the
outermost slot's **3D** distance to the ACU exceeds `spellDistance + ACU combat reach +
DEFAULT_COMBAT_REACH - ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN`. Extract the shift loop from
`MimironPhase1CampSlot` into a helper that takes the reach test. Rewrite the wrong "no attack" comment.

### 4. Emergency Fire Bots: keep, avoid, kill before phase 4

- Constants (`UldEncounter_Mimiron.h`): `ULDUAR_MIMIRON_FIREBOT_KEEP = 2` (living ones left alone;
  extras culled, bounding siren/spray exposure and the cleanup), `_CLEANUP_PCT = 15` (ACU health),
  `_SIREN_CLEARANCE = 13` (10 yd area aura plus both object sizes), `_SPRAY_LENGTH = 16`,
  `_SPRAY_HALF_WIDTH = 3.5`, `_AOE_CLEARANCE = 30`.
- `MimironFirefighterHazards` gains `fireBots` (position + orientation), gathered in
  `GetMimironFirefighterHazards`.
- `bool IsMimironFireBotProtected(PlayerbotAI*, Unit*)`: hard mode, phase 3 (ACU alive, not phase 4),
  ACU above `_CLEANUP_PCT`, and among the `_KEEP` lowest-guid living fire bots (same answer for every bot).
- Targeting: `IsAllowedTarget` for `NPC_EMERGENCY_FIRE_BOT` returns `!IsMimironFireBotProtected`;
  ranged list it above the Assault Bot (culls and cleanup die in seconds), melee below it.
- Cleanup in the 3->4 handover: `MimironSetDpsPriorityTrigger::IsActive` also fires for non-tanks when
  no construct is engaged, hard mode is on and a living fire bot is in the room (the list then holds
  only fire bots). Phase 4 already ranks fire bots above the phase 4 focus.
- New `MimironFireBotAoeGuardMultiplier`: zero `ActionThreatType::Aoe` (not `CastHealingSpellAction`)
  while a protected fire bot is within `_AOE_CLEARANCE` of the bot or its target. Register in
  `UldStrategy.cpp`.
- `bool IsMimironSpotFireBotSafe(Player*, MimironFirefighterHazards const&, Position const&)`: false
  inside any fire bot's spray strip (0.._SPRAY_LENGTH ahead on its orientation, |lateral| <
  `_SPRAY_HALF_WIDTH`) for everyone, and within `_SIREN_CLEARANCE` for ranged and healers in 25-man.
  Add it to `IsMimironSpotSafe`, and to the flee fan as a `spray%u` refusal counter (like `shock`).
- New trigger/action `mimiron fire bot trigger` / `mimiron fire bot action`: active when the bot's own
  position fails that screen; flee with `MoveAwayClearOfMines(fireBot, ..., "firebot")`. Wire through
  `UldTriggerContext.h`, `UldActionContext.h`, `UldStrategy.cpp` (place below the flame dodge; check the
  ladder).
- Fix the contradicting comments in `conf/playerbots.conf.dist:420-421` and `Util/UldHardMode.h:66-67`.

### 5. Frost Bomb: no reach moves back into the blast

New `MimironFrostBombGuardMultiplier` (`Multiplier/UldMultipliers_Mimiron.{h,cpp}`, registered in
`UldStrategy.cpp`): zero `reach melee`, `reach spell`, `reach party member to heal`, `set behind` and
`follow` while a live Frost Bomb from `GetMimironFirefighterHazards().bombs` is within
`ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE + ULDUAR_MIMIRON_FROST_BOMB_HOLD_MARGIN` (new, 4 -> 38 yd exact 2D).
Accepted cost: a healer may be out of range of a far tank for the <= 10 s fuse.

### Not changed

Handover following (user); core script (user); the d1 phase 1 edge-slot `drop target`; arc spread <->
rapid burst.

## Docs

Save this plan first to `docs/plans/mimiron-p3-cores-firebots/mimiron-p3-cores-firebots.PLAN.md`.
Run `/compact-docs-writer` (fresh invocation) before editing. `docs/raids/ulduar/mimiron.md`: Magnetic
Core section (32-core incident, fix, core mechanics above); correct "phase 3 ACU has no attack" and
"hovers directly over whoever holds it" (it chases at 30); new fire bot section (script, spray line,
siren 25-man only, HP, waves, 2009 practice, keep-2/cull/cleanup, d2 numbers) and fix the "only three
things put fire out" line and lines 80-87; Frost Bomb explosion numbers and walk-back veto; the handover
fire split and that following is by choice; phase 3 wedge 3D range; DPS budget with the berserk reached;
`corestep` values in the trace table. `docs/engine/pitfalls.md`: one line that a synthesized item handout
must consume its source.

## Verification

Per-TU `-fsyntax-only` on every changed TU (build image + `compile_commands.json`, scratchpad
`syntax.sh`), then the worldserver rebuild, then one Firefighter pull that reaches phase 3:

1. **Core**: `use` corestep count <= Assault Bots killed; no two 64444 within 25 s; ACU Z at ~364 for
   each window; each Assault Bot corpse used at most once.
2. **Adds**: after a landing, the next Bomb/Junk/Assault/fire bot summons resume within ~45 s.
3. **Melee/tank**: phase 3 `flame dodge <-> reach melee` < 50 A-B-A; melee on the ACU only while grounded;
   `p3hold` notes when nothing is allowed; main tank median <= 5 yd from the room centre.
4. **Ranged**: phase 3 `arc spread <-> reach spell` < 20; median 3D distance to the ACU <= 32 yd.
5. **Fire bots**: <= 1 Water Spray hit on players; siren applications on healers ~0; two alive until
   the ACU reaches 15%; none alive 2 s into phase 4.
6. **Frost Bomb**: zero explosion hits on bots; frost bomb <-> reach A-B-A < 10.
7. **Pace**: phase 3 <= 120 s, ACU >= 45k dps.
8. **No regression**: Napalm <= 1.5 victims/cast, camp nn >= 5 yd, 0 bot deaths in phase 1, >= 20 alive
   at phase 2 start.
