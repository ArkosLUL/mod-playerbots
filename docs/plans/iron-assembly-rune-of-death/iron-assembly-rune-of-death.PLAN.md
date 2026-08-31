# Iron Assembly — the raid stands in Rune of Death

## Context

Two pulls were traced back to back on 2026-08-31:

| | file | outcome | deaths |
|---|---|---|---|
| **A** | `603_1_ulduar_1788197293.ndjson` | wipe at 3:10, Steelbreaker down at 1:44 | 29 |
| **B** | `603_1_runemaster-molgeim_1788198415.ndjson` | **kill** at 9:00, all three down | 14 |

Rune of Death was the largest single killer in both. In B it took **7 of 14 killing blows** and
24.7% of all damage taken (1.38 M). In A it was >50% of incoming damage on 10 of the 29 deaths, and
it caused the wipe indirectly as well (below).

The RaidObs probes added last session did their job — the diagnosis below is read almost entirely
off the trace. `ironassembly.alive` latched correctly (7→6 in A, 7→6→5→4 in B), and the Tendrils and
Overload hazard models check out against measured damage. The bug is in the positioning.

## What the trace says

### Root cause: the raid-position action drags bots back into the rune

Molgeim casts Rune of Death with `DoCastRandomTarget`
(`boss_assembly_of_iron.cpp:519`), repeating every 30–40 s with a ~30 s duration, from his phase 2
only. It lands **on a random raid member's feet** — so with the raid stacked it lands on the stack.
This is the normal case, not bad luck:

```
pull B — 4 of 4 runes landed within 15 yd of the raid stack point (1577.18, 121.02)
   (1577.18, 121.02)   153.1s..183.1s    0.0 yd   <- exactly on it
   (1590.20, 126.17)   183.6s..213.5s   14.0 yd
   (1574.41, 108.65)   218.0s..247.9s   12.7 yd
   (1588.03, 115.18)   253.2s..282.9s   12.3 yd
```

`DeriveIronAssemblyRaidSpot`
([UldEncounter_IronAssembly.cpp:491](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L491))
returns the fixed stack point without ever looking at the runes — even though
`IsIronAssemblyPositionClearOfRunes` already exists two hundred lines below and the Rune of Power
soak path already calls it.

So the escape action (rel 95) and the raid-position action (rel 60) alternate every tick, both
accepted, and the bot never travels:

```
pull A — Druidica, 8 seconds, 20 consecutive pairs (abridged)
   142.47s  ESCAPE -> (1591.5, 117.0)  ok=1
   142.67s  stack  -> (1577.2, 121.0)  ok=1     <- 5.0 yd from the rune centre
   142.88s  ESCAPE -> (1591.5, 117.4)  ok=1
   143.08s  stack  -> (1577.2, 121.0)  ok=1
   ...
```

Move counts inside rune windows: **A 735 escape vs 524 stack; B 2195 vs 2032.**
`IronAssemblyRaidPositionAction` has a reach-then-hold deadband whose own comment says holding "is
what keeps a bot available to interrupt Lightning Whirl" — it never reaches, so it never holds.

### Contributing cause: the clearance has no margin over the real radius

`ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE = 15.0f` does two different jobs — "am I in danger"
and "where do I stand" — and `FindNearestPositionClearOfHazards` returns the *nearest* point that
satisfies it ([EncounterHelpers.cpp:348](src/Util/EncounterHelpers.cpp#L348)), so the bot parks
exactly on the boundary.

The DBC radius is 13 (`spell.reference.csv` 63490 → radius index 17 → 13/13), which the constant
matches. But the aura is applied out to a **measured 15.3 yd (A) / 15.4 yd (B)** — the target
searcher adds object size. The clearance sits inside the real grab radius, so a bot that has
"escaped" by the code's own test keeps re-acquiring the aura:

```
pull B — Rune of Death re-applications per bot: Bulwark 31, Druidica 27, Fel 20, Nightwarrior 19
```

Every other hazard in the same header carries 5–10 yd of margin (Overload 20/25, Tendrils 18/28,
Meltdown 15/20). Rune of Death has 2, and the effective radius eats all of it.

### Knock-on: the interrupt election starves

`IronAssemblyMemberMustMove` disqualifies anyone inside a rune, so while the tug-of-war runs nobody
can hold the kick. `ironassembly.interrupt = none:moving` in pull A: **2 rows before the first rune,
119 after.** Lightning Whirl then free-cast for 467 k and **15 of A's 29 killing blows**.

Per your call this is left alone for now — it is a symptom of the jitter, and worth re-measuring
once the positioning is fixed rather than changing the election on a hypothesis.

### Checked and correct — do not touch

- `ironassembly.alive` latch, `focus`, `soak`, `tank`, `slot` all read cleanly.
- Lightning Tendrils 63485: 151 hits, median 11.9 yd, **max 17.7** against an 18 yd model.
- Overload 61878: 18 hits, median 10.0, **max 19.8** against a 20 yd model.
- Rune of Death radius constant 13.0 matches the DBC.
- High Voltage was 30–43% of damage taken in both pulls but is unavoidable chip damage that stops
  when Steelbreaker dies; it caused no deaths after 1:44 in A.

## Approach

### 1. Split the rune radii — hysteresis

In [UldBossHelper.h](src/Ai/Raid/Uld/Util/UldBossHelper.h) (**additive only**, another session has
this file open):

```cpp
// The DBC radius is 13, but the searcher that applies the aura adds object size and two traces
// measured applications out to 15.4 yd. Run when inside DANGER, stand at CLEARANCE: the gap is what
// stops a bot parking on the boundary and re-acquiring the rune every time it drifts a yard.
constexpr float ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_DANGER_RADIUS = 16.0f;
```

and raise `ULDUAR_IRON_ASSEMBLY_RUNE_OF_DEATH_CLEARANCE` 15 → 21, giving the same 5 yd deadband
Overload and Meltdown already have. Keep `RADIUS = 13.0f` as the DBC fact.

`IsIronAssemblyPositionClearOfRunes` takes a radius parameter so its callers can ask different
questions:

| Caller | Radius |
|---|---|
| `IronAssemblyRuneOfDeathTrigger::IsActive` | DANGER |
| `IronAssemblyMemberMustMove` | DANGER |
| `DeriveIronAssemblyRuneOfPowerSoakSpot` (`none:rune`) | DANGER |
| `IronAssemblyRuneOfDeathAction` escape target | CLEARANCE |
| `IronAssemblyOverloadAction` (runes as extra hazards) | CLEARANCE |

### 2. Make the raid spot rune-aware

In `DeriveIronAssemblyRaidSpot`, after computing `stack` and before the spread branch: gather the
live runes, and if any covers the stack point at CLEARANCE, push the point directly away from that
rune's centre out to CLEARANCE. Iterate a bounded number of passes so overlapping runes converge;
if nothing clears inside a travel cap, fall back to the plain stack point.

Two constraints that decide whether this works:

- **Derive it only from the stack point and the rune set — never from the calling bot.** Each bot
  computes this independently; if the answer depends on where the caller is standing, 25 bots pick
  25 points and the raid scatters instead of relocating.
- **Filter the gathered runes by distance to the stack point, not to the bot.**
  `GatherIronAssemblyRunesOfDeath` sweeps 40 yd around the *caller*
  ([EncounterHelpers.cpp:306](src/Util/EncounterHelpers.cpp#L306)), so a straggler could otherwise
  see a different set than the stack and disagree about where to go.

The spread branch takes the displaced point as its ring centre, so slots move with it and stay
deterministic.

New `ironassembly.spot` values `stack-rune` / `stack-late-rune` / `spread-rune`, so the next trace
shows the branch firing.

## Files

| Path | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldBossHelper.h` | `DANGER_RADIUS` constant; `CLEARANCE` 15 → 21. **Additive only** |
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp` | Bulk: `IsIronAssemblyPositionClearOfRunes` signature, rune-aware `DeriveIronAssemblyRaidSpot`, `IronAssemblyMemberMustMove` |
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.h` | Note-key inventory comment — the new `spot` values |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.cpp` | Rune trigger tests DANGER |
| `src/Ai/Raid/Uld/Action/UldActions_IronAssembly.cpp` | Escape targets CLEARANCE |
| `docs/raids/ulduar.md` | Fold in the findings — invoke `/compact-docs-writer` first, it is reachable from the module `CLAUDE.md` |

## Verification

Static, here: `python apps/codestyle/codestyle-cpp.py`. The module cannot be compiled in this
environment — hand the build off rather than claiming one.

In-game, one 25-man pull to Molgeim's phase 2, then against the trace:

1. `--notes ironassembly.spot` shows `stack-rune` while a rune covers the stack point.
2. Escape vs raid-position move counts inside rune windows are no longer ~1:1 — raid-position should
   fall to near zero while a rune covers the stack. (B was 2195 vs 2032.)
3. Rune of Death re-applications per bot drop from 20–31 into low single digits.
4. Rune of Death's share of damage taken falls well below B's 24.7%, and it stops taking half the
   killing blows.
5. `ironassembly.interrupt = none:moving` returns near its pre-rune baseline. If it does not, the
   must-move gate is an independent bug after all — revisit it then, with evidence.

## Step 0

Copy this document to `docs/plans/iron-assembly-rune-of-death/iron-assembly-rune-of-death.PLAN.md`
before starting; delete that directory and fold the durable findings into `docs/raids/ulduar.md`
once the in-game pass is green.
