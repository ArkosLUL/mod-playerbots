# Mimiron Firefighter: space the phase 1 camp for real, and screen the dodges that walk bots into death

## Context

Two Firefighter 25 attempts on 2026-09-11, both on an image built 3 min after `60486d139` (Plasma
Blast 25-man id, camp disperse 6 + camp tolerance 10). Traces in `ac-worldserver` at
`/azerothcore/env/dist/logs/botobs/603_1_mimiron_1789129048.ndjson` (**c1**) and `..._1789129490`
(**c2**); comparison pulls `1789072883`/`1789073123` (**b1/b2**, 09-10 evening) and
`1789063575`/`1789063939`/`1789064624` (**a1-a3**). Reader: `tools/botobs/postmortem.py`
(`obstrace.Trace`). Phase notes: `mimiron.phase` 1-4, 0 none, 5 handover.

**Both reached phase 2 again** (b1/b2 did not) and wiped there: c1 phase 1 13.3-121.9 s, phase 2 from
170.0 s, wipe 258 s; c2 phase 1 13.2-146.0 s, phase 2 from 194.2 s, wipe 287.7 s.

Yesterday's verification list, scored: Plasma defensive fires (9 notes, windows 1-2 survive with a
defensive) **pass**; phase 2 reached **pass**; MK II off tank anchor median 7.3/6.4 yd, 0% phase-1
fire near centre **pass**; ranged casting 63.3%/51.2% (b: 49.6/44.8, a: 64.6/67.4) **pass**; Napalm
**fail** (3.67 / 7.25 victims a cast); camp spacing **fail** (median nearest-neighbour 0.74 / 0.48 yd).

## Why bots died

Phase 1 is still where the pull is lost; phase 2 only finishes it. Entering phase 2 alive: c1 17
(3 healers, 1 tank), c2 14 (3 healers, **0 tanks**); best older pull a2 had 21. Phase 2 took 18.8k/s
against 14.2k/s healing (c1), 15.7k/s vs 12.1k/s (c2); VX-001 (8,276,398 HP) ended at 61% / 65%.

| cause | c1 p1 | c2 p1 | c1 p2+handover | c2 p2+handover |
|---|---|---|---|---|
| Napalm Shell 65026 | 4 | **8** | - | - |
| Plasma Blast 64529 (tank, after healers died) | 2 | 3 | - | - |
| Proximity Mine 63009 | 3 (+2 at 2:02 after MK II died) | 2 | - | - |
| Shock Blast 63631 | 1 | 1 | - | - |
| Flames 64566 | - | - | 6 early p2 (4 moved there by barrage dodge) | 2 handover (drinking) + 4 p2 (2 by barrage dodge) |
| Heat Wave / Rapid Burst late p2 | - | - | 11 | 9 |

### 1. Napalm: the camp is still one point, and sits on Napalm's minimum range

Napalm (every 14 s, 2 s cast by the cannon 34071, ~57k over 8 s, 5 yd) picks uniformly among
players at raw distance **> 24.5 yd** from the MK II (`GetDistance2d > 15` minus CombatReach 8 + 1.5);
an empty pool falls back to a random threat-list pick. Validated on all 36 casts of seven pulls: a
non-empty pool was hit in 23 of 26 casts (3 explained by movement during cast + flight).

Victims per cast: a 1.20/1.83/1.33, b 7.25/4.00, **c 3.67/7.25**. Two mass casts per pull, same times:

- **~0:16 opener (9 and 13 victims)**: the whole raid within 16.4 yd of the MK II while the tank
  drags it west, pool empty, fallback lands in the camp. Cause: `MimironPhase1StackSlot`
  (`UldEncounter_Mimiron.cpp:934`) clamps the camp to **raw** `spellDistance - margin` = 24.5 yd from
  the MK II, so with the MK II 31 yd off anchor 0 the camp was pulled to it. Bots' own range test is
  `IsWithinCombatRange` (`RangeTriggers.cpp:155`), which adds both reaches: they cast to ~38.5 yd raw.
- **~0:54 (6 and 12 victims)**: c2 had **10 bots on the exact point (2708.1, 2578.3), nn 0.0 yd, for
  20 s**: the clamped slot at 24.7 yd from the MK II, exactly on the cutoff, so all ten were eligible
  and one shell hit twelve.

Why the spacing never happened: `MimironFormationGuardMultiplier` (`UldMultipliers_Mimiron.cpp:92`)
returns **0 for `combat formation move` whenever the bot is within tolerance of its slot**. Raising
the camp tolerance to 10 yesterday switched the unstacker off for the whole camp; the looser pairs
(48-55% under 5 yd vs 86-97%) only came from arc spread no longer pulling bots within 10 yd back.

Knock-on: healers die (c2 down to one by 1:23), then Plasma windows 3-5 kill the tank with 5-11k
healing behind him (windows 1-2 with healers alive got 60-90k and a defensive and survived).

### 2. Shock Blast: dodges walk ranged back into the circle

The escape reaches 18 yd, then returns false for ranged, and `mimiron arc spread action` walks the
bot back toward a slot inside the circle (c2 Trueshot: slot 12.5 yd from a tankless MK II, pulled to
14.3 yd as it landed). The flee fan (`UldActions_Mimiron.cpp` ~100-210) has no Shock Blast filter, and
the flame dodge only stands down for bots already inside 18 yd, so a bot outside can be moved in
(c1 Nightwarrior, flame dodge to 14 yd). `IsMimironSpotSafe` screens mines, rockets, fire, Frost
Bomb, not Shock Blast.

### 3. Phase 2 fire: the barrage dodge orbits through it

`MimironP3Wx2LaserBarrageAction::Execute` walks a fixed-radius orbit in `MOVEMENT_FORCED` steps with
no fire screen, and `MimironDodgeFlamesTrigger` stands down for the whole barrage. c1 3:29-3:35: four
bots walked to (2752-2755, 2553-2558) with 7-10 nodes within 5 yd and burned; c2 3:57: two healers
the same way. The comment says radius does not affect cone safety, so radius is free to dodge fire.

### 4. Handover: blind drinkers and mine fields

- `DrinkAction`/`EatAction` call `SetNextCheckDelay(12-18 s x missing%)` (`NonCombatActions.cpp`), so a
  drinking bot stops thinking; stationary, it is the nearest player to a chain head. c2 Justice and
  Stormweaver started drinking at 3:01.8/3:02.2 and burned 3:08.4-3:13.1 with no dodge at all.
  Drinking is worth keeping: c1 healers went 35-65% → 71-100% mana across the handover.
- Melee never dodge mines (`MimironProximityMineTrigger::IsActive`, by design while the MK II lives).
  c1 Assasin and Totemist died 0.6 s after the MK II did, `follow`ing the master over live mines.

## Positioning

MK II held well (median 7.3 / 6.4 yd off the tank anchor, p90 12-14). Ranged sit a median
**19.8 / 23.3 yd** from the MK II, inside Napalm's 24.5 yd floor; p10 14-15 yd, inside Shock Blast
and the mine field. Phase-1 fire 426k / 207k, 0% within 20 yd of centre.

`drop target` clusters at **anchor 0** (x 2695-2700, y 2590-2595) in every pull (c1 10, c2 9, b1 5,
b2 7, a3 3): ranged there hold a target 85-90% of the time vs 93-100% at anchors 1/2, but still
out-damage them, so it reads as intermittent line of sight. navprobe has no LOS query; unverified.

navprobe `--nav 0x09 ring` around the tank spot at 19/25/31 yd: bearings 285°→75° (through 0°) all on
mesh, settled Z 364.314, 0.22 yd to poly; 90° (the known hole, 3.3-4.6 yd off poly) and 105-255°
(except a raised band 135-225° at 19-25 yd) fail.

## Oscillations

| pattern | c1 | c2 | mechanism |
|---|---|---|---|
| tank: `mimiron arc spread action` ↔ `tank face` | 36 A-B-A, 9.6 rev/min, 182 yd/min | 36, 17.4 rev/min, 250 yd/min | `tank face` steps 5.6 yd off the slot, just past the 5 yd tolerance; arc spread walks him back. The MK II has no frontal, so facing buys nothing |
| arc spread ↔ flame dodge | 63 | **179** | slot is fire-free but the walk back crosses a fire band; the FORCED dodge fires mid-walk and throws the bot out the far side (up to 23 yd) |
| `follow` ↔ flame dodge | - | 62 | 9 ranged/healers lost their target at 0:38.2 (`drop target` at anchor 0), idled on `follow` (rel 1.0) toward the master 30 yd south while the dodge sent them north |
| arc spread ↔ Shock Blast escape | 3 | 33 | finding 2 |

## Phase 1 DPS and target

- **Target**: the MK II is the only attackable unit in phase 1 (mines are `NON_ATTACKABLE`).
  `mimiron.dpsrule` = `mkii` for every bot; share of samples on the MK II: c1 94-99% by role. c2
  melee targetless **48-77% after 100 s**: c2 pulled with one tank (Ecoterrorist carried role
  `melee`, `tank` in c1), Bulwark died at 1:39/1:58 and the MK II chased Agony through the camp.
- **Rate**: MK II 8,276,398 HP. c1 108.5 s = **76.3k** raid DPS on it; c2 132.8 s = **62.3k**; a3 79k.
  Per bot while alive: ranged 3.6-7.0k, melee 4.3-6.4k. Burst cooldowns all go at 0:19-0:36.
- **Budget**: 25-man berserk 10 min from the pull; 13.3 s pre-phase and 103.5 s of fixed handovers
  (47.75 + 24 + 31.8) leave ~483 s for MK II 8.28M + VX-001 8.28M + ACU ~5.5M + phase 4 at 50%
  (~11M) ≈ 33M → **~69k sustained**, so phase 1 at ≤ ~120 s is on pace (c1 yes, c2 no). The binding
  constraint is phase 2: at the 39-55k/s the raid manages there, VX-001 needs 150-210 s against a
  healing race that lasted 88-137 s, so phase 2 needs ~20 alive at its start, or Heroism.
- **Heroism/Bloodlust is never cast**: `UlduarBurstWindowMultiplier` (`UldMultipliers_Shared.cpp:36`)
  holds it for phase 4 (all three mechs up), which no pull has reached.

## Changes

### 1. Indexed phase-1 camp: a wedge of per-bot slots on the anchor bearing

`src/Ai/Raid/Uld/Util/UldEncounter_Mimiron.{h,cpp}`.

- In the `p1stack` branch of `DeriveMimironSpreadSlot` (~line 1103), deal each ranged/healer an index
  with the same group loop the ring branch uses (alive, `IsRanged`, not main tank), and place it with
  `MimironWedgeRows`/`MimironWedgeSlot` (`UldEncounter_Mimiron.cpp:887/904`) around
  `ULDUAR_MIMIRON_PHASE1_TANK_SPOT`, centreline = bearing from the tank spot to the live
  `GetMimironPhase1StackAnchor` spot. Both helpers read the phase-3 half-angle and spacing directly:
  give them `halfAngle`/`spacing` parameters and pass the phase-3 constants at the two existing callers.
- New constants: first row **19 yd** (Shock Blast lethal edge 16.5 raw plus slop, clear of the 15 yd
  mine scatter), spacing **6** (Napalm 5), **3 rows** (19/25/31), half-angle **38°** (capacity 4+5+6 =
  15 ≥ 14). The far rows sit past Napalm's 24.5 yd floor whenever the MK II is on its anchor, so the
  pool is never empty and each shell finds one isolated bot.
  **As built: first row 21 (rows 21/27/33, capacity 16), two anchors on 30° and 330°.** The MK II
  stands ~5 yd off the tank spot toward the camp; replaying c1/c2/b1/b2 MK II positions, any slot
  inside 15 yd fell from 40-49% of the time at 19 to 17-21% at 21, pool empty 0.3-1.3%. navprobe
  1° rings at 21/27/33: clean floor 288°-74°, every slot for 1-16 bots on mesh, nearest 20.7 yd from
  the room centre (the ≥ 24 yd target below was traded for this).
- Re-place `ULDUAR_MIMIRON_PHASE1_STACK_SPOTS` as wedge centroids (radius 25) on centrelines where
  every slot is on mesh and the whole wedge is ≥ 24 yd from `ULDUAR_MIMIRON_ROOM_CENTER`. From the
  ring probe that is roughly 323°-37° plus shifted versions of 75°/300° that clear the 90°/270° holes;
  confirm every slot with navprobe `point` (settled Z 364.314, ≤ 0.5 yd to poly) and record the table
  in `mimiron.md`. The fire-rotation logic (`GetMimironPhase1StackAnchor`, `_FIRE_*`) is unchanged.
- `MimironPhase1StackSlot` becomes a rigid translation of the whole wedge, triggered when the
  outermost slot's raw distance to the MK II exceeds `spellDistance + focus reach + bot reach - margin`
  (≈ 34), never the raw 24.5.
- Drop `ULDUAR_MIMIRON_PHASE1_STACK_TOLERANCE` and the `p1stack` special case in
  `GetMimironSpreadSlot`'s `outTolerance`: with per-bot slots the formation guard's "on slot means no
  unstacker" is correct again. Keep the `outTolerance` parameter only if another caller needs it.
- Trace: the `mimiron.slot` note already carries branch and index; keep `p1stack` as the branch name.

### 2. Screen Shock Blast in the slot check and the flee fan

- `IsMimironSpotShockSafe(PlayerbotAI*, Position const&)` in the util: false while the MK II has a
  current `SPELL_SHOCK_BLAST` cast (63631, no difficulty remap) and `dest` is within
  `ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST` of it. Call it from `IsMimironSpotSafe`, so arc spread and
  its trigger hold instead of walking back in (the tank anchor stays exempt, as for mines).
- Add it to the shared flee fan as a `refusedShock` counter for every flee except `"shock"` itself;
  extend `NoteFleeOutcome`'s line with `shock%u`.

### 3. Fire-aware barrage orbit

In `MimironP3Wx2LaserBarrageAction::Execute`, after `heading`/`radius`: under Firefighter, if the
step point fails `IsMimironSpotFireSafe`, try the same heading at radius ±2, ±4, ±6, ±8 clamped to
`[minRing, ULDUAR_MIMIRON_SPREAD_RADIUS_MAX]`, first clean one wins; keep the original when none is
(the cone kills, fire does not).

### 4. Arc spread does not walk back across fire

`MimironArcSpreadAction::Execute` and `MimironArcSpreadTrigger::IsActive` (keep them agreeing, via one
helper): under Firefighter, hold while the straight segment from the bot to its slot passes within
`ULDUAR_MIMIRON_FLAMES_RADIUS` of a live node (sample every ~1.5 yd against
`GetMimironFirefighterHazards`). The bot stays where the dodge left it until the band clears or the
camp rotates.

### 5. Veto `tank face` on the phase-1 main tank

`MimironTankAnchorGuardMultiplier::GetValue` (`UldMultipliers_Mimiron.cpp:143`): zero `"tank face"`
under exactly the conditions it zeroes `"reach melee"` (hard mode, phase 1, main tank, MK II's
victim, drag latched). The MK II has no frontal ability.

### 6. No blind drinking near fire

New Mimiron multiplier (or extend `MimironAvoidAoeGuardMultiplier`'s file): under Firefighter while
`IsMimironEngaged`, zero `"eat"`/`"drink"` when a live flame node is within 15 yd (chain growth 1.22
yd/s over a 12 s blind window). Register in `UldStrategy.cpp` beside the other Mimiron multipliers.

### 7. Melee dodge mines once the MK II is dead

`MimironProximityMineTrigger::IsActive`: let melee through when no Leviathan MK II is alive and
attackable (nothing to hit, mines last 35 s).

### Not changed, reported

- c2 had one tank: Ecoterrorist pulled as `melee`. A raid setup issue, not code.
- Heroism/Bloodlust stays held for phase 4 (user decision); `UlduarBurstWindowMultiplier` untouched.
- Anchor-switch paths still cross the MK II area (3→0 chord ~8 yd off the tank spot at 22 yd, ~11 yd
  at 29); path screening is out of scope.

## Docs

Run `/compact-docs-writer` before editing (fresh invocation). `docs/raids/ulduar/mimiron.md`:
rewrite the stale Firefighter-intro paragraph (lines 71-78, still says disperse 3.0 and "the bill never
came"); correct the Napalm section's claim that the camp tolerance lets the unstacker work (the
guard zeroes it inside tolerance); add the wedge table, the Shock Blast re-entry, barrage-through-fire
and blind-drink findings, and the phase-1 DPS budget. `docs/engine/pitfalls.md`: one line under the
combat-reach entry that our own slot clamp (`MimironPhase1StackSlot`) made the same raw-vs-edge
mistake the boss script's range literal invites. Save this plan to
`docs/plans/mimiron-p1-wedge-and-dodge-screens/mimiron-p1-wedge-and-dodge-screens.PLAN.md` first.

## Verification

Per-TU `-fsyntax-only` on every changed TU (build image + `compile_commands.json`), then the
worldserver rebuild, then one Firefighter pull. Pass/fail:

1. **Napalm**: victims per cast ≤ 1.5 mean, no cast over 3, the ~0:16 and ~0:54 mass casts gone;
   pool empty on ≤ 1 cast (re-run the cast-start eligibility check: cannon 34071 casting 65026).
2. **Camp**: phase-1 ranged median nearest-neighbour ≥ 5 yd; no two bots on one point for > 2 s.
3. **Shock Blast**: zero ranged deaths; no arc-spread or flee move into 18 yd of a casting MK II.
4. **Phase 2 fire**: no barrage-dodge destination within 5 yd of a node; zero fire deaths within
   3 s of a barrage move.
5. **Tank**: `tank face` absent from the phase-1 main tank's move stream; tank rev/min under 3.
6. **Handover**: no `drink`/`eat` OK within 15 yd of a node; no mine deaths after the MK II dies.
7. **No regression**: MK II ≤ ~7 yd median off anchor, 0% centre fire in phase 1, ranged casting
   ≥ 60%, Plasma notes one per window.
8. **Outcome**: ≥ 20 alive at phase 2 start.
