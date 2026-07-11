# Ulduar Boss Strategy File-Structure Refactor

## Context

Ulduar (`Uld`) raid AI is crammed into two monolithic files:

- `Uld/UldActions.cpp` — **115 KB**, all 13 bosses' action implementations.
- `Uld/UldTriggers.cpp` — **76 KB**, all 13 bosses' trigger implementations.
- `Uld/UldActions.h` (19 KB) and `Uld/UldTriggers.h` (16 KB) — all class declarations inline.

Every other raid that has been maintained (ICC, Naxx) already splits **Actions** into a `Action/` subdirectory with per-boss `.cpp` files. Uld predates that cleanup. The monolithic files are hard to navigate, produce huge diffs, and slow incremental compiles. Goal: split both Actions and Triggers into per-boss files (headers **and** implementations), matching — and extending — the established ICC/Naxx layout, with **zero behavior change**.

Bosses (class-name prefixes, 13 total; XT-002 not implemented):
`FlameLeviathan`, `Ignis`, `Razorscale`, `IronAssembly`, `Kologarn`, `Auriaya`, `Hodir`, `Freya`, `Thorim`, `Mimiron`, `Vezax`, `YoggSaron`, `Algalon`.

## Reference layout (existing convention)

`ICC/Action/ICCActions.h` + `ICC/Action/ICCActions_<Boss>.cpp`; context includes the flat header name (`#include "ICCActions.h"`). Module has **no CMakeLists** — the AzerothCore core globs the module `src/` tree recursively and adds every subdir to the include path, so new `.cpp` files compile automatically and nested headers resolve via flat `#include "..."`. **No build wiring needed.**

## Target structure

```
Uld/
  UldActionContext.h      (unchanged — still #include "UldActions.h")
  UldTriggerContext.h     (unchanged — still #include "UldTriggers.h")
  UldStrategy.{h,cpp}     (unchanged — references names as strings only)
  Util/                   (unchanged)
  Action/
    UldActions.h              umbrella: #includes every UldActions_<Boss>.h
    UldActions_Shared.h       extern decls for cross-boss globals + shared bases
    UldActions_Shared.cpp     definitions of cross-boss globals
    UldActions_<Boss>.h       per-boss class declarations (13 files)
    UldActions_<Boss>.cpp     per-boss Execute()/method bodies (13 files)
  Trigger/
    UldTriggers.h             umbrella: #includes every UldTriggers_<Boss>.h
    UldTriggers_Shared.h      shared base/util decls if any
    UldTriggers_<Boss>.h      per-boss trigger declarations (13 files)
    UldTriggers_<Boss>.cpp    per-boss trigger bodies (13 files)
```

### Why umbrella headers
`UldActionContext.h` includes `"UldActions.h"`; `UldTriggerContext.h` includes `"UldTriggers.h"`; and `UldActions.h` includes `"UldTriggers.h"`. Keeping `UldActions.h` / `UldTriggers.h` as thin umbrella headers that just `#include` the per-boss headers means **the two context files and all existing cross-includes need no edits**, and all `creators[...]`/`triggers[...]` registration maps keep working untouched.

## Work plan

### 1. Triggers first (Actions depend on `UldTriggers.h`)
1. Create `Uld/Trigger/`.
2. For each boss, create `UldTriggers_<Boss>.h` — move that boss's trigger class declarations out of the old `UldTriggers.h`, preserving include-guard uniqueness (`PLAYERBOTS_ULDTRIGGERS_<BOSS>_H`). Add the includes each header actually needs (base trigger headers currently at top of `UldTriggers.h`).
3. Create `UldTriggers_Shared.h` for any shared base classes / free helpers used by multiple bosses (carry over the common includes).
4. Create `UldTriggers_<Boss>.cpp` — move the matching `bool <Boss>...Trigger::` method bodies from `UldTriggers.cpp`. Each `.cpp` starts with `#include "UldTriggers_<Boss>.h"` plus whatever it references (mirror the include block from the old `UldTriggers.cpp`).
5. Replace `Uld/UldTriggers.h` with an umbrella at `Uld/Trigger/UldTriggers.h` that includes all per-boss trigger headers + `UldTriggers_Shared.h`; **delete** old `Uld/UldTriggers.h` and `Uld/UldTriggers.cpp`.

### 2. Actions
1. Create `Uld/Action/`.
2. Per boss: `UldActions_<Boss>.h` with that boss's action class declarations (moved from old `UldActions.h`). Keep the top-of-file includes (`Action.h`, `MovementActions.h`, `UldBossHelper.h`, `UldTriggers.h`, etc.) distributed to the headers that need them; `UldTriggers.h` include now resolves to the umbrella.
3. Per boss: `UldActions_<Boss>.cpp` with the matching `bool <Boss>...Action::` bodies moved from `UldActions.cpp`, each including its own `UldActions_<Boss>.h` + the reference includes from the old `.cpp` top block (`UldBossHelper.h`, `UldScripts.h`, `RaidBossHelpers.h`, `Vehicle.h`, etc.).
4. **File-scope globals** currently atop `UldActions.cpp` (lines ~30–46: `ADD_STRATEGY_CHAR`, `REMOVE_STRATEGY_CHAR`, `availableVehicles`, `corners`, `ULDUAR_KOLOGARN_*`, `ULDUAR_THORIM_*`, `ULDUAR_YOGG_SARON_*`, `yoggPortalLoc[]`):
   - Boss-specific constants (Kologarn/Thorim/Yogg positions) → move into that boss's `.cpp` as file-local (`static`/anonymous namespace).
   - Generic constants used by more than one boss → define in `UldActions_Shared.cpp` with `extern` declarations in `UldActions_Shared.h`; include that header where referenced. (Grep each symbol before placing it to confirm single- vs multi-boss usage.)
5. Umbrella `Uld/Action/UldActions.h` includes all per-boss action headers + `UldActions_Shared.h`; **delete** old `Uld/UldActions.h` and `Uld/UldActions.cpp`.

### 3. Context files
Leave `UldActionContext.h` and `UldTriggerContext.h` as-is (they include the flat umbrella names). Only touch them if a registration references a symbol that ended up needing an extra include — none expected.

## Critical files
- Split source: `Uld/UldActions.{h,cpp}`, `Uld/UldTriggers.{h,cpp}`.
- Unchanged consumers: `Uld/UldActionContext.h`, `Uld/UldTriggerContext.h`, `Uld/UldStrategy.cpp`, `Uld/Util/UldBossHelper.*`.
- Pattern reference: `ICC/Action/ICCActions_BQL.cpp`, `ICC/Action/ICCActions.h`, `Naxx/Action/NaxxActions_Shared.cpp`.

## Verification
- **Build** the module (`worldserver` target) — the only real proof the split compiles/links. Success = no missing-symbol/duplicate-definition errors and all `creators[...]`/`triggers[...]` still resolve. (Per repo rules, only build when the user asks.)
- **Sanity greps before/after**: class-declaration count and `::Execute`/`Trigger::` body count must match pre-split totals (Actions: 13 boss groups; Triggers: 13 boss groups) — nothing dropped or duplicated.
- Run `python apps/codestyle/codestyle-cpp.py` for lint (indent, `auto const&`, includes).
- Behavior is unchanged by construction (pure move); no runtime test needed beyond a clean build and one bot-driven Ulduar pull if the user wants extra assurance.
