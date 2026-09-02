# Freya: survive the Detonating Lasher wave

## Context

Six Freya 25 hard-mode pulls on 2026-09-02
(`env/dist/logs/botobs/603_1_elder-stonebark_17883744{01,861}`, `_1788375{211,957}`,
`_1788376{461,879}.ndjson`) all wiped between 2:06 and 4:01 with Freya never below **96.1%**.
**120 of 161 combat deaths across the six pulls were killed by a Detonating Lasher.**

The reported symptom — "bots didn't drag the lashers to the off-tank" — is the right encounter and
the right wave. The proposed remedy is blocked by the core script, and the fix has to come from
somewhere else. Everything below is measured, not assumed.

### What kills the raid

`Detonate` **62937** (25-man; 62598 is the 10-man twin) is `EffectRadiusIndex 18` = **15.0 yd**,
Nature, `EffectBasePoints_1 6824` + `DieSides 351` → 6826-7176, crits to ~10.6k. Measured in-trace:
mean **6366**, max **10582**, victims out to **15.5 yd** (object size on both ends, the documented
DBC-understates-radius effect). **No falloff** — the 12-15 yd bucket averages as much as the 0-3 yd
bucket. `ULDUAR_FREYA_DETONATE_RADIUS = 15.0f` already exists in `UldEncounter_Freya.h` and is
**never referenced in code**.

The raid stands on top of itself. While ≥3 lashers are alive: median nearest-neighbour **0.7-0.8 yd**,
mean **9.6-11.0 raid mates within 15 yd**. So every lasher death is a raid-wide ~6.4k nuke:

| trace | wipe | deaths | by lasher | Freya HP | blasts | hits | victims/blast | mates ≤15 yd |
|---|---|---|---|---|---|---|---|---|
| 1788374401 | 3:49 | 24 | 24 | 98.5% | 14 | 122 | 8.7 | 10.1 |
| 1788374861 | 2:16 | 23 | 20 | 96.1% | 7 | 53 | 7.6 | 9.9 |
| 1788375211 | 2:06 | 27 | 23 | 98.1% | 3 | 20 | 6.7 | 8.7 |
| 1788375957 | 2:24 | 29 | 11 | 98.6% | 10 | 67 | 6.7 | 9.9 |
| 1788376461 | 3:16 | 28 | 21 | 98.5% | 10 | 89 | 8.9 | 11.0 |
| 1788376879 | 4:01 | 30 | 21 | 98.2% | 13 | 135 | **10.4** | 9.6 |

Attempt 1 is the clearest: ten blasts in 30 s at (2343-2350, -33 to -37), 9-10 victims each, 776k
Detonate damage — three melee died 8 ms apart at 0:45.5. Lashers also free-hit whatever they charge:
melee swings 8-11k and `Flame Lash` **62608** 13-17k, which is the single largest damage source in
four of the six pulls (463k-920k of melee alone).

### Why an off-tank cannot hold them (`boss_freya.cpp:1273-1287`)

```cpp
case EVENT_DETONATING_LASHER_FLAME_LASH:
    me->CastSpell(me->GetVictim(), SPELL_FLAME_LASH, false);
    DoResetThreatList();
    if (Unit* target = SelectTargetFromPlayerList(80))
        AttackStart(target);
    ...
    events.Repeat(10s);
```

Every **10 s** each lasher wipes its own threat list and charges a **random player within 80 yd**;
`Reset()` does the same on spawn with a 70 yd list. `creature_template.speed_run` is **1.14286** →
**8.0 yd/s** against a player's 7.0. So a taunt holds one for at most 10 s, there is no fixate to
"drag", and nothing can be walked anywhere. `UldEncounter_Freya.h:101-106` already records this, and
`FreyaDisableAutomaticTargetingMultiplier` (`UldMultipliers_Freya.cpp:57-59`) records that an earlier
off-tank-collects-the-wave attempt walked the pack into the raid stack. **Do not re-litigate this.**

The pile in the middle of the raid is an *effect* of the 10 yd camp, not a cause: "a random player"
is always inside the same 10 yd ball.

### Why the existing counters never fire

`freya lasher pack step out` fired **once in six pulls** (0 times in most). Its gate is
`GetFreyaFinishingPackNear` → `IsFreyaLasherPackFinishing`: ≥3 lashers within 8 yd of the pack focus
**and not one of them above 20% HP**. Lashers scatter themselves across the raid by design and die
one at a time, so that conjunction essentially never holds. `freya frost nova lashers` and
`freya trap lashers` hang off the same dead gate.

Meanwhile `freya ranged camp` actively packs ranged and healers into a **10 yd** ball (15 yd for
healers) around the lowest-GUID ranged DPS *specifically while `state.detonatingLashers` is
non-empty*. A 15 yd bomb over a 10 yd camp is the delivery mechanism for the wipe.

Verdict counts, attempt 1: `freya ranged camp` 120, `freya lasher pack step out` **1**,
`freya tank adds` 22 (all FAILED).

### Two things the raid already had and never used

**Army of the Dead ghouls do taunt the lashers.** `spell_dk_aotd_taunt` (43263 "Ghoul Taunt",
`spell_dk.cpp:311`) filters only `isWorldBoss()` targets, and Detonating Lashers are not world
bosses. In-trace, **12.5-18.9% of all lasher target samples pointed at an Army ghoul** rather than a
player. Every one of those casts came from **Deathsong, a human** (`h:1`). The bot DK
(`Obliteration`) never cast it in any pull, because `"army of the dead"` is in
`BurstCooldowns.cpp:42` and `UldMultipliers_Shared.cpp:171-180` holds every burst cooldown until
Freya's `SPELL_ATTUNED_TO_NATURE` is gone — i.e. for the entire six-wave add phase, which is exactly
when the ghouls are worth having. The comment there reasons about burst as *damage*; on this wave it
is a **threat sink**.

**Hunters drop the wrong trap.** Both hunters cast `Explosive Trap` on repeat (every ~30 s, all six
pulls). `Frost Trap` **13809** places `Frost Trap Aura` **13810**: `EffectRadiusIndex 13` = **10 yd**,
duration **30 s**, `EffectBasePoints_1 -51` on `SPELL_AURA_MOD_DECREASE_SPEED` = **-50% movement
speed**, dropping a lasher from 8.0 to **4.0 yd/s** — below player run speed. Detonating Lasher has
`CreatureImmunitiesId = 0`, so it is **not** snare-immune. Traps share a **30 s category cooldown**
and a hunter can only have one trap down, so the generic `explosive trap` node is what blocks it.
`freya trap lashers` exists and would cast `frost trap`, but hangs off the dead finishing-pack gate;
it landed exactly once, in the last pull.

### Wave structure (`boss_freya.cpp:366-404, 508, 615-627`)

`EVENT_FREYA_ADDS_SPAM` is scheduled 10 s after engage, repeats on a **60 s** clock, and is pulled
forward to 5 s once the current wave is cleared. **Six waves total**, a permutation of
{trio, conservator, lashers} per set of three, so **two lasher waves per pull**. A lasher wave is
`SPELL_SUMMON_WAVE_10` **62687** cast ten times — `EffectRadiusIndex 18` = **10 lashers within 15 yd
of Freya**, each `REACT_PASSIVE` and stationary for **5 s** before it picks its first target.

Freya's `SPELL_ATTUNED_TO_NATURE` is set to **150 stacks** (`:508`) of +8% healing received, and each
add's death strips a fixed dose (lasher 2, trio member 10, conservator 25) — 2 × (10×2 + 3×10 + 25) =
150 exactly. Killing the wave is the *only* thing that progresses the fight; DPS on Freya during a
wave buys nothing, which is why "everyone spreads" costs nothing real.

### The room

navprobe grid over map 603 (`--map 603 point`, read from `UpdateAllowedPositionZ`): solid floor over
roughly **x 2300..2400, y -30..-80** at z 423-426, sloping to z 419-422 at x ≥ 2400 and y ≥ -25. No
holes. Freya spawns at **(2338.46, -52.33, 425.55)**; across the six pulls the tank dragged her
anywhere from (2359.7, -40.3) to (2397.4, -52.3).

Liquid is not modelled offline and the fork's bot-only nav filter (`NAV_GROUND | NAV_WATER` minus
`NAV_GROUND_STEEP`, `NAV_WATER` at 20x cost) is not reproduced by navprobe's human branch — see
`docs/engine/pitfalls.md`. There is a stream in this room. Slots that land in it will be slow to
reach; that is a tuning problem, not a blocker, and it is not worth chasing up front.

## Approach

Three changes, in descending order of expected effect. All of them are scoped to "a Detonating
Lasher is alive"; nothing outside that window changes.

### 1. Spread the raid on the lasher wave (the load-bearing change)

Because the blast radius is a hard 15.5 yd with no falloff, victims-per-blast is a **step function**
of slot spacing: at 12 yd a hex lattice puts six neighbours inside the radius (7 victims), at
**16 yd** it puts none (1 victim). There is no useful middle. Target **16 yd**.

Per-bot arithmetic over a ten-blast wave: at 10 victims/blast each bot eats ~4 blasts ≈ 25.6k of its
~40k pool; at 2 victims/blast it eats ~0.8 blasts ≈ 5k. That is the whole fix.

- New constants in `Util/UldEncounter_Freya.h`:
  - `ULDUAR_FREYA_LASHER_SPREAD_SPACING = 16.0f` — one yard past the measured 15.5 yd blast reach.
  - `ULDUAR_FREYA_LASHER_SPREAD_TOLERANCE` — how close to its slot a bot has to be before the
    trigger stands down. Keep it small (2-3 yd); the whole point is that neighbours stay >15.5 yd
    apart, and a loose tolerance eats the one yard of margin.
  - The room clamp box (`x 2300..2400`, `y -30..-78`), in the shape of Kologarn's
    `ULDUAR_KOLOGARN_WALKWAY_{X,Y}_{MIN,MAX}`.
- **Latched anchor.** On the first tick a bot sees a live `NPC_DETONATING_LASHER`, freeze Freya's
  current position as the formation anchor; release it when no lasher is alive. **Snap the latched
  x/y to a 5 yd grid** so two bots that latch a tick apart still agree on the same anchor — this is
  cheap and removes the whole class of "half the raid used a different formation" bug. Clamp the
  anchor into the room box so a formation never hangs off the floor.
  - The fallback anchor if Freya cannot be resolved is **(2357.83, -52.33)** — the spot from the
    field report, verified on mesh (`distance to poly` 0.66, settled z 425.61 at y -52).
- **Slot table.** A hex lattice: 4 rows at a 13.9 yd pitch (`16 * √3/2`), 7 columns at 16 yd, odd
  rows offset 8 yd — 28 slots, footprint ~96 x 42 yd, which is what the room actually holds. Assign
  by a **stable index** (group index, GUID tiebreak) so the assignment cannot flip mid-wave. Put the
  four healers on interior columns: 40 yd heal range does not span the footprint's 104 yd diagonal
  from one corner, but does from the middle.
  - Declare the table the way Thorim does — `const Position` in the `.cpp`, `extern const Position`
    in the `.h`, reached through an accessor (`UldEncounter_Thorim.cpp:509` is the slot-table
    precedent) — or generate it from anchor + row/col, which is less text and easier to clamp.
- **Everyone spreads**, main tank excepted (it holds Freya). Melee included: with Attuned to Nature
  up, boss uptime is worth nothing, the lashers come to the melee on their own, and
  `GetFreyaLocalLasherTarget` already tells a melee bot to hit only what is standing next to it.
- **`freya ranged camp` must not run during a lasher wave.** Its trigger currently *requires*
  `!state.detonatingLashers.empty()`, so it is only ever active in the window the spread owns.
  Simplest correct change: retire the node, or invert it to "no lashers alive". Do not leave both
  live — they would fight, and every `MoveTo` calls `mm->Clear()`.
- Priority: above `freya ranged camp`'s old slot and above generic combat movement (`set behind`,
  `combat formation move`, `reach melee` are what stack the melee today), below the Sun Beam dodge
  and Nature Bomb at `ACTION_RAID + 4`. `ACTION_RAID + 3` — the slot `freya lasher pack step out`
  occupies — is the right band. `MOVEMENT_COMBAT`, not `FORCED`: the hazard nodes must still win.
- Reuse `ValidateFloorPoint` (`EncounterHelpers.cpp:629`) on each computed slot before moving to it.
- **Follow the Hodir precedent for the stand-down** (`UldActions_Hodir.cpp:199-218`): the trigger
  standing down inside the tolerance is the arrival latch; do not add a second one, and use the exact
  slot rather than `MoveInside`.

### 2. Let the bot DK open Army of the Dead on the lasher wave

Eight ghouls that AoE-taunt for 40 s, on a 10 min cooldown, against two lasher waves per pull. The
trace already shows the human's ghouls holding 12.5-18.9% of lasher attention.

- New Freya node `freya summon army` at `ACTION_RAID + 2`, active for a death knight when a live
  Detonating Lasher exists and the spell is off cooldown.
- It has to **not** be the generic `CastArmyOfTheDeadAction`, or the burst gates will zero it
  anyway. Check how `IsDpsCooldownAction` (`EncounterHelpers.cpp`) matches — by `dynamic_cast` or by
  action name — and make the new action fall outside it. Then confirm against
  `HoldBurstUntilTankEngagedMultiplier` (referenced at `UldMultipliers_Shared.cpp:174`), which zeroes
  burst on non-boss-flagged targets and would otherwise catch it a second time.
- Leave `UldMultipliers_Shared.cpp:171-180` alone for everything else — holding lust and real DPS
  cooldowns until the final phase is still correct.

### 3. Give the hunters the frost trap instead of the explosive one

- **Re-gate `freya trap lashers`.** Drop `GetFreyaFinishingPackNear` and fire on the live wave
  instead: a Detonating Lasher within the trap's 10 yd effect radius, or the bot standing on its
  spread slot with lashers inbound. Every hunter, not just the elected one
  (`IsFreyaLasherTrapHunter`) — two traps on a 30 s cooldown across a 30-40 s wave is the whole
  budget.
- **The spawn window is the free hit.** All ten lashers spawn within 15 yd of Freya and sit
  `REACT_PASSIVE` and stationary for 5 s. A trap dropped in that cluster inside those 5 s catches
  several at once. That is what "use them so the lashers actually activate them" cashes out to.
- **Suppress the generic explosive trap for hunters while a lasher is alive** — traps share a 30 s
  category cooldown and only one can be down, so the `GenericHunterStrategy.cpp:99-101` node is what
  is spending it today. A multiplier alongside `FreyaLasherFinishAoeMultiplier` is the existing shape.
- A trapped lasher moves at 4.0 yd/s against a bot's 7.0, which also buys the spread time to form.

## Critical files

| File | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h` | spread spacing/tolerance/room-box constants, fallback anchor, new helper declarations |
| `src/Ai/Raid/Uld/Util/UldEncounter_Freya.cpp` | anchor latch + grid snap + clamp, slot table / slot derivation, stable slot index |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.{h,cpp}` | `freya lasher spread` trigger; re-gate `freya trap lashers`; retire or invert `freya ranged camp` |
| `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}` | `freya lasher spread` action (latch members), `freya summon army` action, trap action for all hunters |
| `src/Ai/Raid/Uld/Multiplier/UldMultipliers_Freya.{h,cpp}` | suppress generic explosive trap during a lasher wave |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | TriggerNodes for the two new nodes, drop/replace `freya ranged camp` |
| `src/Ai/Raid/Uld/Trigger/UldTriggerContext.h`, `Action/UldActionContext.h` | creators + factories for the new trigger/action pair |
| `docs/raids/ulduar/freya.md` | the findings: blast geometry, why tanking is impossible, the spread, ghouls, traps |
| `docs/engine/pitfalls.md` | the general lesson: a raid-gather node inside a larger AoE radius is a damage amplifier, and a "finishing pack" style conjunctive gate can be dead code for six pulls without anyone noticing |

Both docs are reachable from `CLAUDE.md`, so run `/compact-docs-writer` before editing them.

Reuse, do not reinvent: `ValidateFloorPoint` and `FindNearestPositionClearOfHazards`
(`src/Util/EncounterHelpers.cpp:629, 331`), the Hodir anchor/tolerance pattern
(`UldActions_Hodir.cpp:199-218`, `DeriveHodirAnchor` at `UldEncounter_Hodir.cpp:555-572`), the Thorim
slot table (`UldEncounter_Thorim.cpp:509`), and the Kologarn walkway clamp
(`UldEncounter_Kologarn.cpp:211-226`).

## Verification

The module cannot be compiled headless here, so steps 2 onward are a hand-off.

1. **Static**: every new constant is referenced; the anchor latch releases when the last lasher
   dies; no two live nodes issue a `MoveTo` in the same window (`mm->Clear()` inside `MoveTo`
   destroys the other one's spline); `freya ranged camp` cannot fire while the spread can.
2. **Build** the worldserver.
3. **Re-pull** Freya 25 hard mode and capture a fresh trace.
4. **Re-measure against this pull's baseline** with the scripts in the session scratchpad
   (`f2_summary.py`, `f2_spread.py`, `f2_ghoul.py`):
   - victims per Detonate blast **6.7-10.4** → expect **under 3**.
   - mean raid mates within 15 yd while ≥3 lashers up **9.6-11.0** → expect **under 3**.
   - median nearest-neighbour **0.7-0.8 yd** → expect **over 14**.
   - `freya lasher pack step out` accepted 1 time in six pulls → the spread node should be the
     dominant mover for the whole wave.
   - Army ghouls present in a bot-only pull at all (they never were), and lasher target samples on
     ghouls above the human-only 12.5-18.9%.
   - `Frost Trap` casts > 0 per lasher wave, `Explosive Trap` casts during a wave = 0.
5. **Outcome checks**: deaths by Detonating Lasher well under the 120-of-161 baseline, first death
   later than 45 s, and the raid clearing more than two of the six waves (all six pulls died on wave
   1-3, and Freya never dropped below 96.1%).
6. `python tools/botobs/postmortem.py <trace> --clump 15` should no longer show the whole raid inside
   one 15 yd circle during a lasher wave.
