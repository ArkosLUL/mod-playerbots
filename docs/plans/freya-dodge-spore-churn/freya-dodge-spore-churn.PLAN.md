# Freya: stop the Sun Beam dodge and the spore node fighting each other

## Context

The 2026-09-01 Freya 25 hard-mode pull
(`env/dist/logs/botobs/603_1_elder-stonebark_1788288220.ndjson`, wipe at 5:25, 31 deaths) was
reported as "bots cannot cross the water stream when reaching a mushroom, so DPS was lost and add
waves overlapped".

The symptom is real. The cause is not the water. Two movement nodes are fighting at ~3 Hz, and each
`MoveTo` calls `mm->Clear()`, so the loser's in-flight walk is destroyed before the bot travels. The
bot ends up jittering inside a 5 yd strip while accumulating 60+ yd of walking, which reads exactly
like "it will not cross".

### What the trace shows

`freya dodge unstable sun beam` is the primary sink:

- 694 accepted move orders, **median step 1.9 yd**, 95% under 12 yd — against a 12 yd beam radius.
  It never leaves the beam, so it re-fires forever.
- It is `ACTION_RAID + 4` (relevance 64) and `MOVEMENT_FORCED`, so it outranks everything it fights.
- 1055 direction flips raid-wide (two actions <1s apart aiming >6 yd apart): **584 are
  dodge ↔ spore**, 215 dodge ↔ `reach melee`.
- Order bursts line up with the collapse: 113 orders in the 180-195s window, **217 in 225-240s**.

`freya move to healing spore action` is the node it starves, and it is independently noisy:

- 1865 move requests → only **601 issued**; **840 duplicates**, 373 wait, 51 blocked
  (`reach melee` by comparison has 18 duplicates in 1247 requests).
- It aims at the spore's exact centre with no arrival tolerance, so `IsDuplicateMove` (0.01 yd, 5 s
  window) rejects it from the second tick, `Execute` returns false, and the engine falls through to
  the combat-movement nodes that then re-point the bot.
- **62%** of accepted spore moves are overridden within 2s by a different action aiming >6 yd away.
- The trigger stands down only on `HasAura(SPELL_POTENT_PHEROMONES)`; between entering the 6 yd
  aura and the aura landing it keeps ordering moves onto the centre.

Outcome: Freya never dropped below **99.2%** in 5:25 — the whole pull was spent on adds. **Zero
deaths in the first 3 minutes, then all 31 in the last 85s**, the signature of waves stacking past
the 60s `EVENT_FREYA_ADDS_SPAM` clock. Raid damage decayed from ~140k to 22k as it unravelled.
Killers: Detonating Lasher 10, Snaplasher 5, Ancient Water Spirit 5, Storm Lasher 5, Freya 4.

Worth noting: Conservator's Grip held only **8.2%** raid uptime while Potent Pheromones held 30.7%.
The spore chase is already expensive relative to the debuff it counters, which is why the fix is to
make it cheap and quiet rather than to make it win more ticks.

### Why not the water

- 133 detour segments (≥8 yd straight, ≥2.0 walked/straight ratio) cluster on two short corridors;
  117 of them belong to the spore node. Worst case: 14.8 yd straight, **62.9 yd walked**.
- `navprobe --map 603 path` across that exact segment returns **14.92 yd, `PATHFIND_NORMAL`,
  3 polys** — identical under the default human filter and under `--nav 0x09`, which reproduces the
  fork's bot-only filter by making `NAV_GROUND_STEEP` polys fail. Three other corridors probe clean
  too (10.90 / 12.74 / 32.67 yd).
- Only **9** failed spore approaches involved the bot barely moving. 188 walked >8 yd and still
  failed — churn, not a wall.
- Per-bot position dumps show the mechanism directly: at t=189-197s `Shadow` alternates between a
  spore order to (2345.4, -37.2) and a dodge order to (2361-2363, -37.2) every ~200-400 ms.

Record this in the findings and stop there — do not chase the terrain angle. Two caveats belong in
the write-up so a later session does not re-litigate it: `exact_waypoint = true` skips
`SearchForBestPath`, so the spore and dodge nodes **structurally cannot** log `nopath`; and navprobe
models neither liquid nor the fork's 20x `NAV_WATER` area cost, so it proves the mesh is connected,
not that water is free.

## Changes

### 1. Sun Beam dodge: real hysteresis plus a destination latch

`src/Ai/Raid/Uld/Action/UldActions_Freya.cpp` / `.h`, `Trigger/UldTriggers_Freya.cpp`,
`Util/UldEncounter_Freya.h`.

The action already uses `FindNearestPositionClearOfHazards` (the correct helper — the Nature Bomb
node was fixed the same way and shows **zero** spurious accepted moves). The bug is the gap between
the trigger and the clearance:

- Trigger fires at `< ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` (12.0).
- Action clears to `RADIUS + 1.0f` (13.0).

`FindNearestPositionClearOfHazards` rings outward from `distanceStep` and returns the *first* clear
spot, so a bot 11.5 yd from a beam centre gets a 1.9 yd answer that sits barely past the boundary.
One yard of hysteresis then loses to the next beam spawn, and the node re-fires.

Do both of:

- Add `constexpr float ULDUAR_FREYA_SUN_BEAM_CLEARANCE = 15.0f;` to `UldEncounter_Freya.h` beside
  `ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS` and use it as the clear radius, giving 3 yd of hysteresis
  over the trigger. Follow the Vezax precedent, which separates
  `ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS` (10) from `..._DODGE_CLEARANCE` (12).
- Latch the destination on the action, in the shape of `FreyaTankAddsAction::parkedSpore`: store the
  chosen `Position` plus the `getMSTime()` it was issued. While the bot is still walking to a latched
  spot that is *still* clear of every beam, return false instead of re-issuing. Release the latch
  when the spot stops being clear, when the bot arrives, or after a short ceiling so a cancelled
  spline cannot strand it.

The latch is the load-bearing half: it is what stops `mm->Clear()` from wiping the spore walk every
tick.

### 2. Spore node: arrival tolerance so it stops duplicate-spamming

`FreyaMoveToHealingSporeTrigger::IsActive` / `FreyaMoveToHealingSporeAction::Execute`.

- Stand the trigger down once the bot is inside `ULDUAR_FREYA_SPORE_RADIUS - 1.0f` of its target
  spore, in addition to the existing `HasAura(SPELL_POTENT_PHEROMONES)` test. The aura is exact and
  lands late; the distance test closes the window where the node keeps ordering moves onto a spore
  it is already standing on.
- Have the action aim at a point just inside the aura rather than the spore's exact centre, so
  `IsDuplicateMove` is not comparing against an unreachable coordinate the bot can never occupy.
- Make the trigger and the action agree on *which* spore. Today the trigger requires any live spore
  in `"nearest npcs"` (LOS-filtered, 100 yd) while melee follow `GetFreyaConservatorSpore` (grid
  search around the Conservator, no LOS, no bot-distance bound). Resolve the target once and use the
  same answer in both, so a bot cannot be triggered by one spore and walked at another.

Do not raise the node's priority. It sits at `ACTION_RAID + 2` below the hazard nodes on purpose,
and with 8.2% Grip uptime it does not deserve more.

### 3. Docs

- `docs/raids/ulduar/freya.md` — the findings: the dodge/spore interaction, the measured numbers,
  and the "not the water" conclusion with both caveats.
- `docs/engine/pitfalls.md` — two general lessons this pull earned: a dodge whose clearance only just
  exceeds its trigger radius will re-fire forever and starve every node under it; and `mm->Clear()`
  inside `MoveTo` means a high-priority node that re-aims each tick cancels lower-priority walks
  even when it is barely moving the bot.
- Also record the fork's undocumented **bot-only nav filter** (`PathGenerator::CreateFilter`
  excludes `NAV_GROUND_STEEP` and sets `NAV_WATER` area cost to 20x for bots) and that `navprobe`
  reproduces the *human* filter — so every "navprobe-verified" coordinate in `docs/raids/**` was
  verified against the wrong filter, and `--nav 0x09` is the closer approximation.

Both docs are reachable from `CLAUDE.md`, so run `/compact-docs-writer` before editing them, per the
compaction rule.

## Critical files

| File | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_Freya.h` | new `ULDUAR_FREYA_SUN_BEAM_CLEARANCE` |
| `src/Ai/Raid/Uld/Action/UldActions_Freya.h` | latch members on `FreyaDodgeUnstableSunBeamAction` |
| `src/Ai/Raid/Uld/Action/UldActions_Freya.cpp` | dodge latch + clearance; spore aim point and target agreement |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.cpp` | spore arrival tolerance |
| `docs/raids/ulduar/freya.md`, `docs/engine/pitfalls.md` | findings and general lessons |

No new triggers, actions or multipliers, so none of the four wiring sites
(`UldTriggerContext.h`, `UldActionContext.h`, `UldStrategy.cpp`, multiplier registration) change.

Reuse, do not reinvent: `FindNearestPositionClearOfHazards` (`src/Util/EncounterHelpers.cpp:331`),
the `parkedSpore` latch shape (`UldActions_Freya.cpp:209`), and the Nature Bomb dodge
(`UldActions_Freya.cpp:39`) as the reference for a correctly-sized escape.

## Verification

The module cannot be compiled headless in this environment, so steps 2 onward are a hand-off.

1. **Static**: confirm the new constant is referenced, the latch releases on all three conditions,
   and no `MoveTo` in the dodge path runs without consulting the latch.
2. **Build** the worldserver.
3. **Re-pull** Freya 25 hard mode and capture a fresh trace.
4. **Re-measure against this pull's baseline**, using the scripts in the session scratchpad:
   - dodge accepted orders **694** and median step **1.9 yd** — expect far fewer orders and a median
     comfortably past 12 yd.
   - dodge ↔ spore flips **584** — expect near zero.
   - spore duplicate move requests **840 of 1865** — expect a small fraction.
   - spore moves overridden within 2s **62%** — expect well under 20%.
5. **Outcome checks**: first death later than 240s, fewer than 31 deaths, and Freya's health actually
   moving off 99%+ once waves are being cleared inside the 60s clock.
6. `python tools/botobs/postmortem.py <trace> --stalls 4000` should no longer list
   `freya move to healing spore action` as the dominant owner of stationary windows.
