# Split RaidObs.cpp and postmortem.py

## Context

The observability subsystem was created on 2026-08-22 and has grown hard for nine days. Its two
implementation files are now the largest things in their trees:

- `src/Bot/Obs/RaidObs.cpp` — 2325 lines, of which lines 54-1368 are **one anonymous namespace**
  holding every shared type, all 8 file-scope globals and ~40 helpers.
- `tools/botobs/postmortem.py` — 773 lines, one module holding the loader, the schema interpreters,
  the analysis passes, seven output views and the CLI.

Both are well written and heavily commented; neither is confusing at the line level. The problem is
navigation and blast radius: every probe added for a new boss lands in the same anonymous namespace,
and every reader tweak lands in the same module. The section banners already in `RaidObs.cpp`
(`// --- session ---`, `// --- snapshots ---`, …) show the author has been maintaining the seams by
hand for weeks — this promotes them to files.

**Outcome:** same behaviour, same trace bytes, same reader output. No `SCHEMA_VERSION` bump. Every
public entry point in `RaidObs.h` and every `postmortem.py` CLI flag keeps working unchanged.

## Constraints that shape the work

- **No CMake edits needed.** The module has no `CMakeLists.txt`. `modules/CMakeLists.txt:205` calls
  `CollectSourceFiles`, defined at `src/cmake/macros/AutoCollect.cmake:26-47`, which recursively
  `file(GLOB)`s `*.cpp`/`*.h` under the module's `src/`. New files are picked up automatically — but
  the glob is **configure-time**, so a re-run of `cmake` is required, not just a rebuild.
- **Filenames are globally unique across all modules** — `CollectIncludeDirectories` puts every
  subdirectory of every module's `src/` on the include path. The `RaidObs*` prefix keeps this safe
  and is why `#include "RaidObsSession.h"` will resolve bare.
- **`src/Bot/Obs/` must not include anything from `src/Ai/`** (`docs/systems/observability.md:217`).
  This is why `RaidObs::MovePriority` duplicates `MovementPriority`. Preserve it.
- **Cannot compile here.** No `g++`/`clang++`/`cl`/`cmake` on this machine and no build directory.
  The C++ half is statically verified and handed to the user to build. Python 3.14 is available, so
  the Python half is verified exactly.
- Style: `.clang-format` is google-based, 4-space, **120 columns**, Allman braces, `Type const*`.
  (`.editorconfig`'s `max_line_length = 80` is not honoured by this code — follow clang-format.)
  Every file opens with the 5-line GPL-v2 block, then its own header, then core headers, then
  `<system>`. Namespaces close with `}  // namespace Name` (two spaces).

## Part 1 — `tools/botobs/postmortem.py` → flat sibling modules

Do this first: it is the half that can be proven correct on this machine.

`tools/botobs` is deliberately **not** a package and stays that way. `flame_leviathan.py:27` already
does `sys.path.insert(0, ...parent)` before `from postmortem import Trace, clock, roster_guids`, so
sibling imports are the established pattern and `python .../postmortem.py <trace>` keeps working.

| New file | ~lines | From `postmortem.py` |
|---|---|---|
| `obstrace.py` | 130 | `SUPPORTED_SCHEMA`, `READABLE_SCHEMAS` (L24-30), `clock` (L33), `class Trace` (L39-144) |
| `records.py` | 120 | `STRIP_AT_DEATH_MS`, `is_debuff`, `aura_line`, `held_to_death`, `act_row` (L150-196); `REFUSALS`, `note_text`, `move_line` (L264-311) |
| `analysis.py` | 130 | `MARKER_PROXIMITY_YD`, `snapshot_before`, `hazards_at` (L199-261); `damage_summary`, `combat_deaths` (L358-377); `ANSWERED_MOVE_YD`, `roster_guids`, `position_runs` (L597-617) |
| `deathreport.py` | 210 | `summarise` (L314), `death_block` (L380), `show_death` (L478) |
| `views.py` | 190 | `show_bot` (L500), `show_track` (L534), `show_notes` (L567), `show_stalls` (L620), `show_clump` (L675) |
| `postmortem.py` | 80 | module docstring (the usage text), `main` (L718), `sys.exit(main())` — re-exports `Trace`, `clock`, `roster_guids` so `flame_leviathan.py` keeps importing from it |

**Do not name the loader `trace.py`** — it shadows the stdlib `trace` module, and the script's own
directory is first on `sys.path`.

Each new file keeps the botobs conventions the two existing scripts already share: module docstring,
`from __future__ import annotations`, stdlib-only alphabetised imports, modern type hints
(`dict | None`, `list[dict]`), plain classes (the repo uses no dataclasses anywhere), double quotes.
Only `postmortem.py` and `flame_leviathan.py` keep the shebang and the executable bit.

`flame_leviathan.py` needs no change if `postmortem.py` re-exports the three names it imports. Prefer
that over editing it — it keeps the module's one existing consumer untouched.

## Part 2 — `src/Bot/Obs/RaidObs.cpp` → 6 translation units + a shared header

`RaidObs.h` (the public contract, 404 lines) is **not touched**. Its 24 consumers — `Engine.cpp`,
`MovementActions.{h,cpp}`, `WipeAction.cpp`, `PlayerbotCommandScript.cpp` and 20 raid encounter files
— see nothing. `RaidObsScripts.cpp` (the ScriptMgr adapter, 225 lines) is not touched either.

`RaidObs.cpp` is deleted and replaced by:

| File | ~lines | Contents (source lines in today's `RaidObs.cpp`) |
|---|---|---|
| `RaidObsSession.h` | 300 | **NEW.** `OBS_*` constants (L58-76), `ObsConfig` (L78), record structs `DamageEntry`/`TickEntry`/`TickRecord`/`AuraState`/`ActionLatch`/`BotTrace`/`PreRollEntry`/`PreRollRing` (L222-327), `class ObsSession` (L329), `extern` globals, free-helper declarations, `ProbeTarget`, `JsonFields` |
| `RaidObsSession.cpp` | 330 | `g_active` (L51), globals (L97-98, L364-371), format helpers (L102-217), registry `FindSession`/`SessionFor`/`MapIsTracked`/`RefreshActiveFlag` (L381-450), `ObsSession` write path and roster (L454-623), `ProbeTarget`, `JsonFields` |
| `RaidObsSnapshot.cpp` | 340 | `UnitRow`, `HazardRadius`, `HazardIsFriendly`, `SweepArea`, `BuildSnapshotPayload`, `ObsSession::WatchCreature`/`SeedWatched`/`PruneWatched`/`PruneAuras` (L630-947) |
| `RaidObsLifecycle.cpp` | 430 | `ResolveBossName`, `FindEngagedBoss`, `OpenSession`, `CloseSession`, `ProcessPendingBossState`, `ObsSession::RosterMostlyDead`/`AnyRaidMemberInCombat` (L1031-1262), `OnMapUpdate`, `OnMapDestroyed`, `OnBossState`, `UpgradeBossName`, `OnCreatureEngage`, `MarkPull` (L1436-1641) |
| `RaidObsConfig.cpp` | 180 | `ApplyRetention` (L1266), `LoadConfig` (L1372), `Shutdown` (L1423), `Status` (L2303) |
| `RaidObsCombat.cpp` | 470 | `AccrueDamageDealt` (L1342), `NoteDamage`…`NoteCast` (L1643-1889), `NoteDeath` (L2129-2285) |
| `RaidObsEngine.cpp` | 400 | tick buffer `SameTick`/`EmitTickEntry`/`ShouldEmit`/`ObsSession::FlushTick` (L953-1027), `t_currentAction`/`t_currentBot` (L1366-1367), `BeginTick`…`NoteMove` (L1891-2018), `ActionScope`, `BotContext` (L2020-2028), `Note`, `NoteAssignment`, `NoteDerived`, `NoteHazard`, `NoteHazardCircle` (L2030-2127), `DescribeAssignment`/`DescribeDerived` (L2287-2301) |

Naming follows the house pattern for a non-per-boss split — `UldEncounterGate.cpp`,
`UldHardMode.cpp`, and the existing `RaidObsScripts.cpp` — not the `_<Boss>` suffix form.

### `RaidObsSession.h` — the one genuinely new thing

This is the module's first header shared between sibling `.cpp` files (there are no `*Internal*`,
`*Detail*`, `*Impl*` or `.inl` files anywhere in `src/`). Keep everything in plain `namespace RaidObs`
rather than inventing a `detail` namespace — the module has no such precedent, and the closest
analogue is ICC, where one `ICCActions.h` backs fourteen `ICCActions_*.cpp`. Open the file with a
comment saying it is private to `src/Bot/Obs/` and that the public contract is `RaidObs.h`.

Keep `GuidKey` (L146) `inline` in the header. It is called 36 times on per-event paths and the
anonymous namespace is what let the compiler inline it today; the rest are cold enough or expensive
enough that a cross-TU call does not matter.

`t_currentAction` and `t_currentBot` stay file-local in `RaidObsEngine.cpp` — every one of their five
use sites lands there.

### `class ObsSession`

`struct ObsSession` (L329-362) becomes a class. **Keep the data members public** — it is an internal
aggregate and touching field access would triple the diff for nothing. What changes is that the free
functions taking `ObsSession&` become members:

| Today | Becomes | Defined in |
|---|---|---|
| `RawWrite(s, line, force)` | `s.Write(line, force)` | `RaidObsSession.cpp` |
| `Flush(s)` | `s.Flush()` | `RaidObsSession.cpp` |
| `Emit(s, ms, event, fields, force)` | `s.Emit(ms, event, fields, force)` | `RaidObsSession.cpp` |
| `EnsureUnit(s, unit)` / `EnsureSpell(s, id)` | `s.EnsureUnit(unit)` / `s.EnsureSpell(id)` | `RaidObsSession.cpp` |
| `TracksPlayer(s, unit)` | `s.Tracks(unit)` | `RaidObsSession.cpp` |
| `RebuildRoster(s)` / `RosterJson(s)` | `s.RebuildRoster()` / `s.RosterJson()` | `RaidObsSession.cpp` |
| `WatchCreature`/`SeedWatched`/`PruneWatched`/`PruneAuras` | members | `RaidObsSnapshot.cpp` |
| `FlushTick(s, key, trace)` | `s.FlushTick(key, trace)` | `RaidObsEngine.cpp` |
| `RosterMostlyDead(s)` / `AnyRaidMemberInCombat(s)` | members | `RaidObsLifecycle.cpp` |

Roughly 90 call sites, all mechanical. `Stamp` (L361) is already a member.

Stay free functions — they are registry- or map-scoped, not session-scoped: `FindSession`,
`SessionFor`, `MapIsTracked`, `RefreshActiveFlag`, `OpenSession`, `CloseSession`,
`ProcessPendingBossState`, `ApplyRetention`, `ResolveBossName`, `FindEngagedBoss`, and the pure
helpers (`JsonEscape`, `Quoted`, `Num`, `SlugOf`, `UnitRow`, `SweepArea`, `BuildSnapshotPayload`,
`SameTick`).

`BuildSnapshotPayload` keeps its `ObsSession*` parameter: the pre-roll path at L1475 calls it with
`nullptr`, before any session exists.

`Status()` and `Shutdown()` are the two documented world-thread callers
(`docs/systems/observability.md:289-307`). `bytes` and `rosterSize` must stay `std::atomic` and the
`g_registryMutex` discipline must survive the move unchanged.

### Collapsing the duplication

**`ProbeTarget`** — this six-line prologue is repeated in 14 probes (L1891, L1907, L1923, L1939,
L2030, L2071, L2129, and the combat probes):

```cpp
if (!Active() || !bot) return;
ObsSession* session = SessionFor(bot);
if (!session || !TracksPlayer(*session, bot)) return;
BotTrace& trace = session->bots[GuidKey(bot->GetGUID())];
```

becomes a small value type in `RaidObsSession.h`:

```cpp
// What a bot probe needs before it can write, or nothing. Falsy covers the three ways a probe has
// no work: nothing recording, no session on this bot's map, and a unit the open trace does not
// follow. Trace() inserts on first use, so a probe that only emits never grows the bot map.
class ProbeTarget
{
public:
    explicit ProbeTarget(Player* bot);

    explicit operator bool() const { return _session != nullptr; }
    ObsSession& Session() const { return *_session; }
    BotTrace& Trace() const { return _session->bots[_key]; }
    uint64 Key() const { return _key; }

private:
    ObsSession* _session = nullptr;
    uint64 _key = 0;
};
```

`Trace()` must stay lazy. Today `Note` (L2030) and `NoteAssignment` (L2051) resolve a session without
ever touching `bots`; an eager insert would silently change which units the map holds.

**`JsonFields`** — every record is assembled by hand-concatenating `"key":value` (see `NoteDeath`
L2146-2228 and `OpenSession` L1113-1120). A small append-only builder in `RaidObsSession.h` with
`Raw`/`Text`/`Int`/`Real` removes the stray-comma class of bug. **It must emit byte-identical output**
— `Real` delegates to the existing `Num` (L135) and `Text` to `Quoted` (L133), and key order is
whatever the call site adds. Convert the record builders to it; leave the nested array loops
(`auras`, `rewind`, `dist`) alone, they are not key-value shaped.

If `JsonFields` turns out to shift a single byte of output, drop it and keep the concatenation — the
`ProbeTarget` change carries most of the value and none of the risk.

## Order of work

0. Save this plan to `docs/plans/raidobs-split/raidobs-split.PLAN.md` (per `docs/README.md`, plans
   live under `docs/plans/<slug>/`), then start.
1. **Python** — capture the baseline (below), split, re-run, diff. Commit on its own.
2. **C++** — `RaidObsSession.h` first, then move sections file by file, `ObsSession` methods, then
   `ProbeTarget`, then `JsonFields`. Commit separately from the Python change.
3. **Docs** — `docs/systems/observability.md`. Line 10 currently reads "Code: `src/Bot/Obs/`. Reader:
   `tools/botobs/postmortem.py`."; it needs a short layout note so a fresh agent knows which file a
   new probe or a new view belongs in, and the "Adding a probe" section (`:225-287`) should name the
   target file. **This doc is a convention doc referenced from `RaidObs.h:28`, so invoke
   `/compact-docs-writer` before editing it**, per the global governing-docs rule.

## Verification

### Python — exact, and runnable here

`env/dist/logs/botobs/` holds **33 real traces**, 333 MB, spanning ten Ulduar bosses. A full trace
loads in 0.17-0.81 s, so a whole sweep is about two minutes. This is the regression suite:

```bash
# from the AzerothCore root
snap() {                       # $1 = output dir
  P=modules/mod-playerbots/tools/botobs
  mkdir -p "$1"
  for t in env/dist/logs/botobs/*.ndjson; do
    b=$(basename "$t" .ndjson)
    who=$(python -c "import json,sys
for l in open(sys.argv[1],encoding='utf-8'):
    r=json.loads(l)
    if r.get('e')=='hdr': print(r['roster'][0]['n']); break" "$t")
    python $P/postmortem.py "$t"                > "$1/$b.default" 2>&1
    python $P/postmortem.py "$t" --death 0      > "$1/$b.death"   2>&1
    python $P/postmortem.py "$t" --bot   "$who" > "$1/$b.bot"     2>&1
    python $P/postmortem.py "$t" --track "$who" > "$1/$b.track"   2>&1
    python $P/postmortem.py "$t" --notes        > "$1/$b.notes"   2>&1
    python $P/postmortem.py "$t" --stalls       > "$1/$b.stalls"  2>&1
    python $P/postmortem.py "$t" --clump        > "$1/$b.clump"   2>&1
    python $P/flame_leviathan.py "$t" --ram     > "$1/$b.fl-ram"  2>&1
    python $P/flame_leviathan.py "$t" --fury    > "$1/$b.fl-fury" 2>&1
  done
}
```

Run `snap before` on the current tree, do the split, run `snap after`, then `diff -r before after`.
**It must be empty** — stderr is captured too, so a new import warning fails the check. Write the
outputs under the scratchpad, not the repo. Also confirm `postmortem.py --help` and
`postmortem.py /nonexistent` (exit 1) still behave, and that `python -c "import flame_leviathan"`
from `tools/botobs` still resolves.

### C++ — static here, compiled by the user

Cannot be built on this machine. What can be checked without a compiler:

- Every symbol declared in `RaidObsSession.h` has exactly one definition across the six `.cpp`, and
  no symbol is defined twice — compare the function list from
  `grep -n "^[A-Za-z_].*(" src/Bot/Obs/RaidObs*.cpp` against the pre-split list from
  `git show HEAD:src/Bot/Obs/RaidObs.cpp`. The two sets must match name for name.
- `grep -rn '#include "' src/Bot/Obs/` names nothing under `src/Ai/`.
- `python apps/codestyle/codestyle-cpp.py` passes (no trailing whitespace, no double blank lines,
  `Type const*` ordering).
- `RaidObs.h` is byte-identical to `git show HEAD:src/Bot/Obs/RaidObs.h`, and so is
  `RaidObsScripts.cpp`.

Then hand off: **a fresh `cmake` configure is required**, not just a rebuild, because
`CollectSourceFiles` globs at configure time. After it builds, the end-to-end check is a recorded
pull — set `RaidObs` enabled, pull any Ulduar boss, and run the new trace through `postmortem.py`.
The trace must carry `"v":9` and produce a summary with a boss name, a roster, an outcome and death
blocks with populated `auras`/`rewind`/`acts`. A trace that opens but shows empty death blocks means
a probe lost its session lookup in the move.

## Files at a glance

Deleted: `src/Bot/Obs/RaidObs.cpp`.
Created: `src/Bot/Obs/RaidObsSession.{h,cpp}`, `RaidObsSnapshot.cpp`, `RaidObsLifecycle.cpp`,
`RaidObsConfig.cpp`, `RaidObsCombat.cpp`, `RaidObsEngine.cpp`; `tools/botobs/obstrace.py`,
`records.py`, `analysis.py`, `deathreport.py`, `views.py`.
Modified: `tools/botobs/postmortem.py`, `docs/systems/observability.md`.
Untouched: `src/Bot/Obs/RaidObs.h`, `src/Bot/Obs/RaidObsScripts.cpp`,
`tools/botobs/flame_leviathan.py`, and all 24 consumers of `RaidObs.h`.
