# Thorim phase 2: end the taunt war, stop towing the boss, and hold the melee ring still

Traces read with `python tools/botobs/postmortem.py <file>` from `modules/mod-playerbots`; files live in
`env/dist/logs/botobs/`. **Timestamps are milliseconds.** Scratchpad scripts that produced every number
below are reusable: `p2.py`, `win.py`, `osc.py`, `rot.py`, `cl.py`, `chain2.py`, `dodge.py`, `lc.py`,
`dps2.py`, `why.py`, `dth.py`, `us.py`, `bul2.py`, `p1.py`.

| file | outcome | phase 2 began | Sif damage |
|---|---|---|---|
| `603_1_thorim_1788898994.ndjson` (A) | wipe 5:27.4, 29 dead | 3:11.1 | 1,605,882 |
| `603_1_thorim_1788899499.ndjson` (B) | wipe 5:05.4, 27 dead | 3:09.1 | 1,216,075 |

Both hard mode, both won the race. Build carries `8cd13085a` (the phase 2 tank pickup) and `d4ff61347`.
Raid is 25-man with **one bot tank (Bulwark, protection paladin)** and **one human tank (Felesta, blood
DK)**; `PlayerbotAI::IsTank` correctly recognises Felesta through the spec branch, so every tank
predicate treats the two symmetrically.

---

## Context

### `8cd13085a` worked. Everything it aimed at moved.

| | before | now (A / B) |
|---|---|---|
| vacuum before a tank held Thorim | 15.3 s / 9.6 s | **0.4 s / 1.2 s** |
| Thorim median distance from the anchor | 21.3 / 29.9 yd | **6.6 / 7.7 yd** |
| phase 1 deaths | 1 / 0 | **0 / 0** |

No regressions: squad split still 13/12, `combat formation move` still owns 0 moves, hard mode still won.
All 29 and 27 deaths are in phase 2.

### Why they die now

Phase 2 damage taken, by source: **Sif is the largest single contributor** at 1.61M of 3.84M (42%) in A
and 1.22M of 3.29M (37%) in B, split across Frostbolt Volley, Frostbolt and Frost Nova. Thorim's melee is
714k / 768k. Both wipes are cascades rather than a steady bleed - A lost 9 bots over 100 s then 19 in the
last 16 s; B lost 6 then 10 in 9.5 s. Thorim was still at **55% and 71%** when the raid died, at 0.33 and
0.25 %/s, so the raid is also far short of the DPS the phase needs. Everything below either stops a
death outright or buys back damage.

### 1. The paladin class node taunts on cooldown and fights the human tank

`TankPaladinStrategy.cpp:114-121` wires `"lose aggro"` to `hand of reckoning` at `ACTION_HIGH + 7`, which
falls back to `righteous defense`. The trace shows exactly that: `rel 27.0`, firing every ~2 s at Thorim.
This is **not** the new pickup node - `thorim tank pickup action` ran 4 times all pull and stood down
correctly every time.

Felesta taunts, Bulwark rips it straight back. Trace A: Felesta takes him at 3:12.8, Bulwark at 3:17.6;
Felesta 3:18.8, Bulwark 3:19.8; Felesta 3:21.3, Bulwark 3:35.8. Felesta landed 16 Dark Commands against a
continuous stream from Bulwark, and Thorim's victim changes 34 and 45 times across the phase.

Unbalancing Strike is a red herring in the code - `ThorimUnbalancingStrikeAction` strips the aura outright
under `BotCheatMask::raid`, within 0.1-1.6 s each time - but the observation stands: it landed on Bulwark
at 4:18.4 and he taunted Thorim off Felesta at 4:20.0 and again at 4:22.1.

**Hodir already solved this exact problem**: `IsHodirTauntAction` + `HodirGuardMultiplier`
(`UldMultipliers_Hodir.cpp:44-65`) name-match every class taunt and zero them when taunting would be
wrong. Thorim has no such guard.

### 2. Blizzard: the dodge does not step out of the AoE, it runs 30 yd

`MoveAwayFromCreatureAction::Execute` (`MovementActions.cpp:2925-3000`) scans 8 compass rays out to 30 yd
and keeps the candidate that **maximises** distance from the bunny. It always takes the full 30. Measured
leg length: median **29.8 / 29.9 yd**, p90 30.0, and the compass is overwhelmingly E/NE/SE - away from the
west anchor, into the middle of the arena. It has no term for the anchor, the boss, or the rest of the raid.

The guard added in `8cd13085a` is `boss->GetVictim() == bot`, which the taunt war switches off. Trace B,
the drag the user saw:

```
4:21.971  Felesta taunts; Bulwark is standing on the anchor, 0.0 yd off
4:22.084  guard lifts -> thorim sif blizzard action -> (2132.0, -231.4), 30.0 yd out
4:23.444  -> (2139.6, -223.8), 40.9 yd out
4:26.262  the lose-aggro node taunts Thorim back, from out there
4:28.438  boss at (2124.3, -240.5), 18.2 yd off the anchor
```

Cost of standing in it instead: Blizzard did 109k to Felesta and 86k to Bulwark across pull A, against
11-38k from a single Thorim swing on whoever he reaches when the anchor breaks.

### 3. The melee ring turns 90 degrees twice per charge, and the turn is what gets them hit

Melee are **moving 71% and 59% of phase 2** against ranged at 8-10%; 282 and 194 yd/min against 34 and 44.
Per head they deal 2.6k dps against ranged 5.9k.

Two independent defects in `RingRotation` (`UldEncounter_Thorim.cpp:~455-506`):

- **It resets when the orb goes dark.** `ThorimChargedThunderOrb` returns null with no orb lit, so
  `RingRotation` hands back 0 and the whole ring snaps back to its latched bearing. Every charge cycle is
  therefore two whole-ring turns, out and back. Measured: **307 of 352 melee destination changes turned
  more than 60 degrees, median 90, p90 178 - with the boss having moved 0.0 yd in between.** At radius 8
  that is an 11.3 yd run each way, matching the observed median leg of 11.3 yd exactly.
- **It solves against a ring the bots are not standing on.** `clears()` tests
  `SlotBearing(RingAnchorBearing(boss) + candidate, slot)` - the *live* anchor bearing - while each bot's
  destination uses `LatchedRingBearing`, struck when it first asked. Once the boss has moved off where he
  was at the latch, the rotation being solved for is not the ring that exists.

And it does not work. Melee took 139k / 46k of Lightning Charge, the same per head as the ranged who never
dodge at all - and **every single Lightning Charge hit on a melee bot landed while that bot was running**
(7 of 9 in A, 4 of 4 in B, against 0 of 8 stationary ranged hits). The dodge is causing the damage.

### 4. How the dodges aim, and Chain Lightning

`Chain Lightning 64390` from the DBC (`mod-spell-tweaks/data/dbc-reference/spell.reference.csv`):
`EffectChainTargets_1 = 8`, **`EffectChainAmplitude_1 = 1.5`** - each jump hits 50% harder, so the eighth
target takes roughly 17x the first. Core jump radius is 10 yd for magic chains
(`Spell.cpp:2163`), with `searchRadius = jumpRadius * chainTargets`, so it finds all 8 whenever a 10-yd
chain of that length exists. It picks a **random** primary, not Thorim's victim.

Trace B 3:50.8, boss 18-28 yd off the anchor: 8 targets, 174,932 damage, running 4,695 -> 7,378 -> 8,716
-> 14,071 -> 19,664 -> 27,242 -> 46,142 -> 47,024, straight out of the melee ring and into the ranged
camp. **Four bots dead in 0.1 s.** Trace B 4:05.9, boss on the anchor: 8 targets, 141,198, all of them
melee and tanks, **zero deaths**.

The raid is one connected 10-yd component for **100% of phase 2** (median cluster 20 of 25 in A, 16 in B),
so the chain always maxes out. Neither dodge helps or hurts that much: the Blizzard dodge actually
isolates the dodger (nearest raid member 2.6 -> 16.2 yd) but parks him 30 yd out of position, and the
Lightning Charge dodge relocates the melee cluster without breaking it (2.3 -> 2.4 yd). What decides
survival is **where the chain ends**, and that is the anchor's job - which is defects 1 and 2.

Stacking is real and deliberately out of scope this round: 8 melee share 3 destination points and 14
ranged share 5, with no per-bot offset, so Fel + Nightwarrior + Malediction stand within 2 yd of each
other for 428-529 snapshots. Revisit after this lands.

---

## The change

Four edits, all under `src/Ai/Raid/Uld/`. No coordinates change, so no navprobe run is needed.

### 1. A taunt guard, modelled on Hodir's

New `ThorimTauntGuardMultiplier` in `Multiplier/UldMultipliers_Thorim.{h,cpp}`, registered in
`UldStrategy.cpp` beside the other seven (`:901-907`).

- File-local `ThorimIsTauntAction(std::string const&)` copying `IsHodirTauntAction`
  (`UldMultipliers_Hodir.cpp:44-51`): `taunt`, `hand of reckoning`, `righteous defense`, `dark command`,
  `growl`, `challenging shout`, `challenging roar`. `righteous defense` must be in the list - it is what
  `hand of reckoning` falls back to on cooldown, and it is half the taunts in the trace.
- `GetValue` returns 1.0 unless **all** of: `ThorimPhase2Active`, the name matches, the bot's
  `"current target"` is Thorim, and Thorim's victim is a different player for whom
  `PlayerbotAI::IsTank` is true. Then 0.0.
- Name test first, before the boss lookup - this runs for every action in the queue.

Gating on the current target keeps taunting an add off a healer alive, and the pickup node is unaffected
because `ThorimTankPickupTrigger` already stands down whenever another tank holds the boss. The encounter's
own taunts are safe for a second reason worth knowing: both of them go out through
`PlayerbotAI::DoSpecificAction`, and `Engine::ExecuteAction` (`Engine.cpp:361`) runs `isUseful` and
`isPossible` and then executes - multipliers only apply on the queue path. So this can only ever silence
the class nodes, and the Unbalancing Strike swap, which fires precisely when another tank holds the boss,
still works.

### 2. Tanks never dodge Blizzard in phase 2

`ThorimSifBlizzardTrigger::IsActive` (`Trigger/UldTriggers_Thorim.cpp:~295`): widen the existing
`boss->GetVictim() == bot` exemption to cover any bot whose `GetThorimPhase2Role` is `MainTank` or
`OffTank` while `ThorimPhase2Active`. Keep the victim test as-is for everything outside phase 2. Replace
the comment block, which currently explains the narrower rule.

### 3. Only the slot the cone covers steps aside, and it holds

Replace `RingRotation` with a per-bot `ThorimLightningChargeOffset(PlayerbotAI*, Player*, float bearing,
float& offset)` in `Util/UldEncounter_Thorim.cpp`, and delete `RingRotation` along with the
`ringRotationOrb` / `ringRotation` / `ringRotationHeld` state fields (`UldEncounter_Thorim.h:280-282`,
reset at `.cpp:2001-2003`).

- New state on `ThorimEncounterState`: `std::unordered_map<ObjectGuid, RingOffset> ringOffsets`, where
  `RingOffset { ObjectGuid orb; float offset = 0.0f; }`. Erase per bot next to `ringBearings` in
  `ResetThorimEncounterState`, clear the map in the instance-wide reset, and add it to
  `ThorimBotHasEncounterState`. Deliberately **not** pruned in `EnsureMeleeSlot`: `ringBearings` is not
  either, and its comment says why - a slot handed back on a rez swings the bearing a quarter turn.
- With no orb lit, hand back the held offset unchanged. Not resetting is the point of the change.
- On a **new** orb guid, re-solve from scratch against the **latched** bearing. If the latched bearing is
  not in that orb's cone the offset goes to zero; if it is, step out by the nearer edge:

  ```
  halfWidth = CONE_ANGLE/2 + MARGIN + CONE_CLEARANCE          // 37.5 + 15 + 5 degrees
  low, high = coneBearing -/+ halfWidth
  exit      = the one nearer the latched bearing
  offset    = normalize(exit - latchedBearing)
  ```

  Closed form, no 5-degree search. Solving from the latched bearing rather than from where the last cone
  left the bot matters: a simulation over 20000 random cones shows chained offsets drift the three slots
  out of their 90 degree spacing until **two of them share a point**, which is what the old rigid
  whole-ring turn was really buying. Add `ULDUAR_THORIM_RING_CONE_CLEARANCE = 0.0873f` (5 degrees) to the
  header beside the other cone constants.
- Simulated against the old behaviour over 20000 random cones: **1.49 slot-moves per cone against 3.00**,
  at a **median 4.3 yd and p90 7.3 yd against 11.31 yd every time** - and the old design paid that twice
  per charge, out and back. Nobody is ever left inside the cone. The cost is that a displaced slot can sit
  **4.5 yd** off a neighbour instead of 11.3, inside Chain Lightning's 10 yd jump, 16.6% of the time. The
  melee ring already chains through the 2-3 bots stacked on each slot, and the anchored melee-only chain
  in trace B at 4:05.9 killed nobody, so that is the right side of the trade.
- `TryGetThorimPhase2Spot`'s MeleeRing branch (`~:1785-1792`) calls the new helper instead of
  `RingRotation`; the `bearing + offset` composition and the `StaticMeleeSpot` fallback stay as they are.
- `ThorimLightningChargeActive` (`~:1880-1892`) currently returns `RingRotation`'s "a turn was needed"
  bool. Simplify it to "phase 2 and an orb is lit" - the per-slot decision now lives entirely in the spot,
  and `ThorimLightningChargeTrigger`'s own `> ULDUAR_THORIM_RING_ARRIVE_TOLERANCE` check already keeps a
  bot that is already clear from firing.

### 4. Save the plan

Copy this document to `docs/plans/thorim-phase2-taunt-war-and-ring-cone/thorim-phase2-taunt-war-and-ring-cone.PLAN.md`
once implementation starts.

## Deliberately not doing

- **Unstacking the shared anchors.** 8 melee on 3 points and 14 ranged on 5 is real, but keeping the boss
  anchored already kept one full 8-target chain entirely on melee and tanks with zero deaths. Re-read the
  next trace first.
- **Nothing about Sif.** She is the largest damage source now, but Frostbolt Volley is DBC radius 200 and
  has no positional answer, and Frost Nova already has a dodge node.
- **No coordinate changes**, so no navprobe run.
- **Nothing about the arena adds.** Unchanged from last round and still not what kills anyone.

## Verification

Static, from `modules/mod-playerbots`:

- `python apps/codestyle/codestyle-cpp.py` clean for the touched files. Its three standing failures are
  pre-existing (`DBCStructure.h` tabs, `mmaps_generator`).
- No line over 120 columns; files stay LF and ASCII. Check the encoding in Python reading bytes, not with
  `grep -P` - the locale here rejects it and the pass is then meaningless.
- Per-TU syntax check in `acore/ac-wotlk-build:master` against `/azerothcore/build/compile_commands.json`
  for each changed `.cpp`: mount `modules/mod-playerbots/src` read-only, take the entry for the file, drop
  `-c` and `-o`, add `-fsyntax-only`, run from its `directory`. Needs `MSYS_NO_PATHCONV=1` and `$(pwd -W)`
  for the mount source or Git Bash mangles the paths. Expect only the two standing `-Wunused-parameter`
  warnings in `UldEncounter_Thorim.cpp`. Takes ~2.5 min - run it backgrounded.

In game, one 25-man hard-mode pull with the human tank present, then re-read the trace:

1. **Thorim's victim flips fewer than ~6 times in the phase**, against 34 and 45, and Bulwark casts no
   `hand of reckoning` or `righteous defense` at Thorim while Felesta holds him. (Both counts include the
   free-for-all after the tanks die, so read them alongside the time the last tank was alive.)
2. **No `thorim sif blizzard action` move by either tank in phase 2**, against 29 and 25 accepted dodge
   moves at a median 29.8 yd.
3. **Thorim walks under ~40 yd in phase 2**, against 136 and 323, and stays a median under ~5 yd from
   (2110.7, -252.7), against 6.6 and 7.7.
4. **Melee move under ~25% of the phase**, against 71% and 59%, and under ~80 yd/min against 282 and 194.
   Melee destination turns should be mostly small: the >60 degree share should fall well under the
   307-of-352 and 167-of-219 seen now.
5. **Melee per-head damage up** from 2.6k dps, and Lightning Charge damage on melee no higher than the
   139k / 46k it is now.
6. **Chain Lightning bursts stay inside melee and tanks**, as the anchored burst at B 4:05.9 did, rather
   than ending on cloth for 46k like B 3:50.8.
7. **No regression** on: the pickup vacuum still under ~2 s; phase 1 deaths still 0; `combat formation
   move` still owning 0 moves; squad split still 13/12; hard mode still won with Sif dealing damage
   through phase 2.
