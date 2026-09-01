# Split UldBossHelper and UldMultipliers per boss

## Context

`src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}` (2381 + 4166 lines) holds data and behaviour for all
14 Ulduar bosses in one pair. `src/Ai/Raid/Uld/UldMultipliers.{h,cpp}` (456 + 1250, 36 classes) does
the same for multipliers. Both keep growing with every encounter tuning pass.

The concrete cost: 64 files include `UldBossHelper.h`, so touching one Hodir tolerance recompiles all
64 TUs. `UldActions_Mimiron.cpp` pulls 2381 header lines to use 40 symbols, all of them Mimiron's.
And the boss grouping inside the header is not even contiguous — constants for Mimiron appear in 4
separate runs, Hodir/XT-002/Ignis/Flame Leviathan in 3 each, and `// General Vezax` appears twice in
the id enum. Only Kologarn is in one piece.

The intended outcome: each boss owns one `UldEncounter_<Boss>.{h,cpp}` pair carrying its ids, tuning
constants, positions and behaviour, and one `UldMultipliers_<Boss>.{h,cpp}` pair. `UldBossHelper.{h,cpp}`
is deleted. `UldActions_Mimiron.cpp` ends up including ~450 lines instead of 2381.

This is a **pure move**. No behaviour changes, no renames beyond the one noted in Step 1, no
retrospective-comment trimming, no `constexpr`-ification. Anything noticed en route goes on a
follow-up list, not into the diff.

## Why this shape

- **The consumer graph is already per-boss.** `UldActions_<Boss>.cpp` uses only `<Boss>` symbols.
  The only genuinely cross-boss consumers are `UldMultipliers.cpp` (being split here) and
  `UldHardMode.cpp` (9 symbols, FL + XT-002).
- **Cutting the id enum costs nothing.** `grep -rn "UlduarIDs" src/` returns exactly one hit — its
  own definition. It is never named as a type, so it can become 14 unscoped per-boss enums with zero
  call-site churn. Unscoped keeps every enumerator at namespace scope.
- **Per-boss data has precedent.** `SWPEncounter_Brut.h` carries 22 `constexpr` tuning constants,
  `SWPEncounter_KJ.h` 8. SWP's actual rule is ids-and-tuning in the per-encounter header, not the
  one-data-header model `EoEData.h` suggests.
- **The umbrella pattern is already in the tree.** `Action/UldActions.h` and `Trigger/UldTriggers.h`
  are 20-line guard + 14 includes + `#endif`. `Multiplier/UldMultipliers.h` copies that exactly.
- **No build files to edit.** There is no `CMakeLists.txt` in mod-playerbots; the core collects
  sources by recursive `file(GLOB)` (`src/cmake/macros/AutoCollect.cmake`) and puts every subdir on
  the public include path, so `#include "Foo.h"` resolves by bare filename from anywhere. New files
  need zero build-file changes — but GLOB runs at **configure** time, so adding files needs a CMake
  re-configure, not just an incremental build.

## Target layout

```
src/Ai/Raid/Uld/
  Util/
    UldData.h                              NEW ~40L  ULDUAR_MAP_ID + the 2 class spells only
    UldEncounter_FlameLeviathan.{h,cpp}    NEW
    UldEncounter_Razorscale.{h,cpp}        NEW  (carries class RazorscaleBossHelper)
    UldEncounter_IronAssembly.{h,cpp}      exists - gains its data
    UldEncounter_Ignis.{h,cpp}             NEW
    UldEncounter_XT002.{h,cpp}             NEW
    UldEncounter_Kologarn.{h,cpp}          NEW
    UldEncounter_Auriaya.{h,cpp}           NEW
    UldEncounter_Hodir.{h,cpp}             NEW
    UldEncounter_Freya.{h,cpp}             NEW
    UldEncounter_Thorim.{h,cpp}            exists - gains its data
    UldEncounter_Mimiron.{h,cpp}           NEW
    UldEncounter_Vezax.{h,cpp}             exists - gains its data
    UldEncounter_YoggSaron.{h,cpp}         NEW
    UldEncounter_Algalon.{h,cpp}           exists - gains its data
    UldBossHelper.{h,cpp}                  DELETED (step 18)
  Multiplier/                              NEW dir
    UldMultipliers.h                       20L umbrella (moves here from Uld/)
    UldMultipliers_Shared.{h,cpp}          the 2 raid-wide classes
    UldMultipliers_<Boss>.{h,cpp}          x14
```

New headers follow the existing [UldEncounter_Vezax.h](src/Ai/Raid/Uld/Util/UldEncounter_Vezax.h)
template verbatim: GPL 4-line block → `#ifndef PLAYERBOTS_ULDENCOUNTER<BOSSUPPER>_H` → project
includes alphabetical (`UldData.h` where the old files had `UldBossHelper.h`) → blank → `<std>`
includes → forward decls (`class Player; class PlayerbotAI; class Unit;`) → prose comment on the
encounter mechanics → **ids enum → tuning constants → `extern const Position` →** structs/state →
`extern` state map → function decls grouped by phase with `//`-banner dividers, each with a
why-comment → bare `#endif`. Ids become `enum UlduarMimironIds { ... }` etc.

Follow Thorim/Algalon for include ordering, not Vezax — Vezax's is unsorted.

## Invariants (check at every step)

1. **Dependencies point one way.** Once a boss's data lives in its encounter header, that header must
   not include `UldBossHelper.h`. It includes `UldData.h`. Consumers → per-boss headers → `UldData.h`.
   This is why the 4 existing encounter headers drop `UldBossHelper.h` as the first action of their step.
2. **`UldBossHelper.h` is hollowed, not edited in place.** Each boss step *adds one*
   `#include "UldEncounter_<Boss>.h"` line to it. It temporarily becomes the umbrella, so every
   intermediate state compiles and nothing else in the tree changes. It dies in step 18.
3. **File-local mutable state moves atomically with every one of its readers.** Highest-severity
   risk here, because the failure is silent: leave a copy of `flStates` behind and two maps exist,
   the raid-wide answer quietly stops being shared, and you get the `fl.pursued` flapping the FL
   comment already records. No compiler error. Never partially move one of these:
   - anon ns (FL): `flStatesMutex` + `flStates` + `FlameLeviathanStateFor` + `TickFlameLeviathan` (cpp 2761-2864)
   - anon ns (Mimiron): `mimironObsStatesMutex` + `mimironObsStates` + `MimironObsStateFor` + `TickMimironObs` (cpp 3577-3688)
   - Ignis: `ignisTankDrivenConstructGuid` (cpp 2196, unguarded), `_ignisTankArcStates` (cpp 2433, thread_local)
   - Razorscale class statics `_harpoonCooldowns`, `_lastRoleSwapTime`
4. **No namespace-scope object's initialiser may name another namespace-scope object.** Holds today
   (every `const Position` initialiser is a literal; `ULDUAR_FL_ARENA_CORNERS` is four literals) and
   must survive the split into 14 TUs, where relative init order becomes unspecified.
   `ULDUAR_MIMIRON_ROOM_CENTER` read inside `MimironOrbitAhead` is *not* a violation — that is a
   runtime call, not an initialiser. Note this in the commit message so nobody "fixes" it.
5. **Enumerator names stay globally unique.** One enum guarantees it today; 14 enums do not.

## Sequencing

Every step is one commit. Steps 7-16 touch strictly disjoint file sets, so concurrent sessions on
different bosses cannot conflict. Rebase rather than merge; other sessions are active in this tree.

**Step 0.** Save this plan to `docs/plans/uld-helper-split/uld-helper-split.PLAN.md`. Capture the
step-0 baselines: symbol inventory (7a), per-file usage map (7b), mutable-state counts (7f).

**Step 1 — shared promotions + dead code.** Append `ValidateFloorPoint` (currently the static at
[UldBossHelper.cpp:811](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L811)) and `UldCastClassTaunt`
(cpp 1371-1389) to [src/Util/EncounterHelpers.h](src/Util/EncounterHelpers.h) + `.cpp`, beside the
existing `FindNearestPositionClearOfHazards` / `IsTauntAction`. Append-only edits — other sessions
share this file. `ValidateFloorPoint` **keeps its name**, so its 6 call sites (5 Hodir, 1 XT-002 at
cpp 2159) need zero edits: all callers already have `using namespace EncounterHelpers;`.
`UldCastClassTaunt` → `CastClassTaunt`, 7 call sites across 5 files, mechanical. Delete the
commented-out `GenericBossHelper` template (h 2318-2379) and the now-unused `#include "EventMap.h"`.

This is first because it removes the one cross-boss coupling — after it, no boss step ever has to
reason about `ValidateFloorPoint`.

**Step 2 — `UldData.h`.** `ULDUAR_MAP_ID`, `SPELL_MISDIRECTION`, `SPELL_FROST_TRAP` (both hunter
spells, under a `// Class spells the encounters name by id` comment, not under a boss). Header
comment states the constraint the way `OSData.h` does: no behaviour here, and nothing boss-specific
may be added — without that line this file regrows into UldBossHelper.h.

**Steps 3-6 — the 4 already-split bosses absorb their data.** Order: **Algalon → IronAssembly →
Vezax → Thorim** (ascending constant count: 36, 31, 47, 59; Algalon first, its cpp block is already
fully separated). No new files — these prove the data-movement mechanics at lowest risk.

**Steps 7-16 — the 10 remaining bosses, ascending by difficulty.** If interrupted after step 7 the
tree is valid, compiling and strictly better.

| # | Boss | cpp block | What is new vs the previous step |
|---|---|---|---|
| 7 | Kologarn | 2501-2745 | nothing — contiguous, 14 constants, zero file-local state. Pilot for a new pair. |
| 8 | Auriaya | 445-573 | two `const Position[]` arrays sized by `ULDUAR_AURIAYA_STATION_COUNT`; `AuriayaFearWindowActive` is stranded at cpp 1391 |
| 9 | Yogg-Saron | 429-443 + 1391 | tiny code, but two non-contiguous fragments and 17 constants in two runs |
| 10 | Ignis | 2191-2499 | the two file-local statics (invariant 3) |
| 11 | Razorscale | 133-427 + 1890-1929 | `class RazorscaleBossHelper` + 2 class statics + file-local `GetLowestHealthUnitByEntry`; two fragments |
| 12 | Freya | 1411-1888 | 478 lines, `struct FreyaWaveState`, one file-local static |
| 13 | XT-002 | 1931-2189 | file-local `GetFirstAliveNpcByEntry` (XT-only, moves with it) |
| 14 | Hodir | 575-1369 | 795 lines, 62 constants, 5 file-local statics |
| 15 | Mimiron | 3321-4166 | 846 lines, 3 anon namespaces, `struct MimironBarrageWindow`, obs state + mutex |
| 16 | Flame Leviathan | 2747-3319 | `enum FlameLeviathanTowerFlags`, dynamic-init `ULDUAR_FL_ARENA_CORNERS`, anon ns with mutex + map |

**Step 17 — retarget the 64 includers.** Generated from the step-0 usage map, not chosen by hand:
25 files drop the line outright (they reference zero symbols and already include `AiObject.h` /
`PlayerbotAI.h` / `Playerbots.h` directly); 39 swap it for one or two per-boss includes.

**Step 18 — `git rm UldBossHelper.{h,cpp}`.** By now the `.h` is only include lines and the `.cpp`
only its own include and a `using namespace`.

**Steps 19-20 — the multiplier split.** Last, because `UldMultipliers.cpp` is the one genuinely
cross-boss consumer and per-boss files only become possible once the encounter headers exist.

- Create `Uld/Multiplier/` with the 14 per-boss pairs + `UldMultipliers_Shared.{h,cpp}`. The umbrella
  `UldMultipliers.h` **moves into `Multiplier/`**; because includes resolve by bare filename,
  [UldStrategy.cpp:9](src/Ai/Raid/Uld/UldStrategy.cpp#L9) and its 36-line `InitMultipliers`
  (825-901) are untouched. Umbrella lists bosses in the same encounter order the other two umbrellas
  use. All new files get the GPL block — `UldMultipliers.h` is currently the one Uld file missing it.
- The 4 file-local helpers each move with their single boss: `ThorimIsTargetSelectionAction` (static),
  `IsRazorscaleGenericMover`, `IsHodirTauntAction`, `MimironLethalWindowActive` (anon-ns).
- `UldThreatRedirectMultiplier` and `UlduarBurstWindowMultiplier` → `UldMultipliers_Shared`, matching
  the existing `UldActions_Shared.{h,cpp}`.
- **`UlduarBurstWindowMultiplier::EvaluateWindow` stays whole.** Its 105-line per-boss switch exists
  precisely so the `IsBurstCooldownAction` early-out runs once per action rather than once per boss,
  and it resolves nine boss entries out of a single `"possible targets no los"` sweep. Splitting it
  into nine predicates means either nine sweeps per tick or a redesign. This refactor moves code; it
  does not redesign hot paths. `UldMultipliers_Shared.cpp` will therefore include ~6 encounter
  headers — correct and bounded; say so in its header comment so nobody "fixes" it later.

## Verification

A full AzerothCore build is expensive and there is no compiler on PATH here. Rely on static checks
between steps; **configure + build at 4 checkpoints only: after steps 1, 6, 16, 20.** Remember the
re-configure — GLOB runs at configure time.

Run from the module root (Git Bash). Baselines captured at step 0, compared after every step.

```bash
# (7a) symbol inventory - the oracle for everything else
inv() { { grep -rhoE "^[[:space:]]+[A-Z][A-Z0-9_]+[[:space:]]*=" "$1" | tr -d ' ='
          grep -rhoE "^constexpr [A-Za-z0-9_:]+ [A-Za-z_][A-Za-z0-9_]*" "$1" | awk '{print $3}'
          grep -rhoE "^extern [A-Za-z0-9_:<> ]*[ *&][A-Za-z_][A-Za-z0-9_]*" "$1" | awk '{print $NF}' | tr -d '*&'
          grep -rhoE "\b[A-Za-z_][A-Za-z0-9_]*\(" "$1" | tr -d '(' ; } | sort -u ; }
inv src/Ai/Raid/Uld/Util/UldBossHelper.h > /tmp/baseline.txt     # step 0
diff /tmp/baseline.txt <(inv src/Ai/Raid/Uld/Util/)              # after any step: empty

# (7b) per-file usage map - drives step 17 and proves it
for f in $(grep -rl "UldBossHelper.h" src/Ai/Raid/Uld/); do
  printf "%4d %s\n" "$(grep -owFf /tmp/baseline.txt "$f" | sort -u | wc -l)" "$f"; done | sort -n

# (7c) no duplicate enumerators across the split headers - must be empty
grep -h -E "^[[:space:]]+[A-Z][A-Z0-9_]+[[:space:]]*=" src/Ai/Raid/Uld/Util/UldEncounter_*.h \
  src/Ai/Raid/Uld/Util/UldData.h | tr -d ' =' | sed 's/[0-9].*$//' | sort | uniq -d

# (7e) dependency direction - both must be empty from step 3 on
grep -l 'UldBossHelper.h' src/Ai/Raid/Uld/Util/UldEncounter_*.h
grep -rn 'UldEncounter_' src/Ai/Raid/Uld/Util/UldData.h

# (7f) mutable-state singletons - files must be 1, total must match baseline
for s in flStates flStatesMutex mimironObsStates mimironObsStatesMutex \
         ignisTankDrivenConstructGuid _ignisTankArcStates _harpoonCooldowns _lastRoleSwapTime; do
  printf "%-32s files=%s total=%s\n" "$s" \
    "$(grep -rl "\b$s\b" src/ --include=*.cpp | wc -l)" "$(grep -ro "\b$s\b" src/ | wc -l)"; done

# (7h) declaration/definition parity per new pair - must be empty
diff <(grep -oE "extern const Position [A-Za-z_0-9]+" H.h  | awk '{print $4}' | sort) \
     <(grep -oE "^const Position [A-Za-z_0-9]+"      C.cpp | awk '{print $3}' | sort)

# (7i) content conservation - THE review command; put it in every boss commit message
diff <(git show HEAD~1:src/Ai/Raid/Uld/Util/UldBossHelper.cpp | sed -n '2501,2745p') \
     <(sed -n '<start>,<end>p' src/Ai/Raid/Uld/Util/UldEncounter_Kologarn.cpp)

# (7k) filename collisions - BEFORE creating anything (flat include namespace)
cd ../.. && for n in UldData.h UldEncounter_Hodir.h ...; do
  [ "$(find src modules -name "$n" | wc -l)" -gt 0 ] && echo "COLLISION $n"; done

# (7j) terminal invariants, after step 18
grep -rn "UldBossHelper" src/ docs/          # 0 hits
wc -l src/Ai/Raid/Uld/Util/UldEncounter_*.h  # none over ~550
```

`git` cannot detect a move *within* a 4166-line file, so a boss step's `git diff` shows a big
deletion and a big addition without saying they are the same lines. **(7i) is the only real review
this diff will get** — which is why the "pure move, nothing else" rule is load-bearing, not fastidious.

If the Docker build image can be run directly, a one-off ~20 minute investment in a header
self-containment probe (a 2-line TU per new header, `-fsyntax-only` with the module include dirs)
turns each of the 14 risky steps into a 5-second check. Worth resolving before step 7.

## Out of scope (recorded so it is not lost)

- Folding other raid-local floor-snap wrappers onto the promoted `ValidateFloorPoint`. They may not
  be behaviourally identical (e.g. `OSGeometry::ResolveMoveDestination` takes a `rejectOnCollision`
  flag and returns `bool`). Behaviour review, not a move.
- Deduplicating `SPELL_MISDIRECTION = 35079`, redefined across several raid headers plus
  `RaidRedirectThreat.h` as `SPELL_MISDIRECTION_PROC` — already flagged in `docs/raids/ulduar.md`.
- Compressing the retrospective prose in the moved constants. Roughly half is load-bearing
  anti-regression record; trimming it is a judgement-heavy pass that would destroy the (7i) check.
