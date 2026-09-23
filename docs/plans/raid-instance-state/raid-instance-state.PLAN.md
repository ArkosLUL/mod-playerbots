# Raid instance state store

## Context

Raid strategies keep cross-bot state (latches, slot assignments, fight clocks) in hand-rolled
`static std::mutex` + `std::unordered_map<uint32 /*instanceId*/, State>` pairs, one per boss, each
with its own accessor, its own copy of the "not thread_local" rationale, and its own eviction rule.
Three defects follow from that:

- **Data race.** Algalon (`Uld/Util/UldEncounter_Algalon.cpp:29`, `extern` in `.h:151`) and Vezax
  (`UldEncounter_Vezax.cpp:32`, `extern` in `.h:196`) have no mutex, and their reset triggers read the
  map raw (`Uld/Trigger/UldTriggers_Algalon.cpp:32`, `UldTriggers_Vezax.cpp:35`). Razorscale's
  `RazorscaleBossHelper::_lastRoleSwapTime` / `_harpoonCooldowns` (`UldEncounter_Razorscale.h:128,133`,
  defined `.cpp:36,38`) are class statics keyed by guid, no instance id, no mutex. Different instances
  update concurrently, so two raids in Ulduar race on the outer map.
- **Carry-over into a re-entered lockout.** Most stores never erase the instance entry. Instance ids
  are not reused for new instances within a run, but a lockout-bound instance re-entered after its map
  unloaded is recreated with its saved id (core `Maps/MapInstanced.cpp:141-149, 159-163`), so the old
  latches come back.
- **Ulduar reset nodes cannot clean up after a kill.** Every Uld trigger is wrapped in
  `UldGatedTrigger` (`UldTriggerContext.h:207-216`), which closes once the boss is DONE
  (`UldEncounterGate.cpp:70-71`), and most reset actions never erase the instance entry anyway.

Goal: one module, `RaidInstanceState`, owns locking, lookup and instance-lifetime eviction; boss code
keeps only its state struct and its own mid-fight reset rules.

### Threading facts the design relies on (verified in core)

- One `InstanceMap` is updated by one worker at a time; the worker can differ tick to tick
  (`Maps/MapMgr.cpp:269-280`, `MapInstanced.cpp:44-71`, `MapUpdater.cpp:124-182`).
- Different instances update concurrently on different workers, so any map shared across instances
  needs a lock.
- `PlayerbotAI::UpdateAI` runs only from `OnPlayerAfterUpdate` (`src/Script/Playerbots.cpp:187-193`),
  inside the bot's `Map::Update`. The "do"/"d" chat commands run on the world thread, sequentially
  before `sMapMgr->Update`, never concurrently with it. `AllSpellScript` hooks
  (`Uld/Util/UldBotScripts.cpp`) run on the casting thread, which is the map worker.
- Consequence: locking only the outer lookup and handing back a reference to the instance's entry is
  safe; `unordered_map` references survive rehashing and erasure of other keys.
- Non-instanced maps all have instance id 0 (`Maps/MapMgr.cpp:88`).

## Design

### Module

`src/Ai/Raid/RaidInstanceState.h`: header-only template, **std includes only** (use
`std::uint32_t`; core's `uint32` is the same type), so a native test can include it alone.

```cpp
template <typename State>
class RaidInstanceState
{
public:
    State& For(std::uint32_t instanceId);   // creates the entry on first use
    State* Find(std::uint32_t instanceId);  // nullptr if none; never creates
    void Reset(std::uint32_t instanceId);   // erases the entry; next For() starts fresh
};

void RaidInstanceStateDrop(std::uint32_t instanceId);  // erases the id from every store
```

Behind the interface:

- One mutex per store, covering the lookup/insert/erase only. The one-thread-per-instance rule above
  is stated once, in the header, replacing the ~10 pasted "Not thread_local…" comments.
- `assert(instanceId != 0)` (`<cassert>`) in `For`/`Find`/`Reset`.
- A registry: each store adds itself on construction to a list reached through an `inline`
  function-local static (one definition across TUs, no static-init-order problem). The registry has
  its own mutex because a function-local store may be constructed lazily on a worker.
  `RaidInstanceStateDrop` walks it and erases the id from each store.
- `State` must be default-constructible.

Stores are declared at namespace scope in the boss `.cpp`, replacing the mutex, map and accessor:

```cpp
static RaidInstanceState<HodirBotLatches> hodirLatches;
// hodirLatches.For(bot->GetInstanceId()).shelter
```

### Map hook

`src/Ai/Raid/RaidInstanceState.cpp`: a small `AllMapScript` restricted to
`{ALLMAPHOOK_ON_DESTROY_MAP}` whose `OnDestroyMap(Map* map)` calls
`RaidInstanceStateDrop(map->GetInstanceId())` when the id is non-zero. Precedents:
`IccMapCleanupScript` (`ICC/ICCScripts.cpp:177-187`), `RaidObsMapScript`
(`src/Bot/Obs/RaidObsScripts.cpp:42-53`). Register via `AddSC_RaidInstanceStateScripts()`, declared
and called in `src/Script/Playerbots.cpp` next to `AddSC_IcecrownBotScripts` (`:560`, `:584`).
No `CMakeLists.txt` edit: sources under `src/` are globbed.

### Native test

`tools/nativetest/raid_instance_state_test.cpp` plus a runner script in the same directory that
compiles and runs it inside `acore/ac-wotlk-build:master` (the image `~/.claude/scripts/pb-syntax-check.sh`
uses; mount the module read-only, `-I src/Ai/Raid`, plain `assert`-based checks, non-zero exit on
failure). It must stay **outside `src/`**, or the module build globs it in. Cases:

1. `For` creates on first use; a second `For` returns the same object.
2. `Find` returns nullptr for an unknown id and does not create.
3. `Reset(a)` leaves instance `b` intact.
4. `RaidInstanceStateDrop(a)` erases `a` from two different store types, leaves `b`.
5. A reference from `For(a)` stays valid and unchanged while other threads insert many ids.

The id-0 assert is not tested (needs a death-test harness).

Add to step 1 of `CLAUDE.local.md` §"Closing an implementation cycle": when
`src/Ai/Raid/RaidInstanceState.h` changes, run the native test. Run `/compact-docs-writer` before
editing it (standing rule for governing docs).

## Scope: stores to migrate

Paths relative to `src/Ai/Raid/`. "Own reset" = keep the boss's current in-place reset logic
unchanged; it now runs on the store's entry.

| Store | Current eviction (keep) | Migration notes |
|---|---|---|
| `Uld/Util/UldEncounter_Algalon.cpp:29` | Reset node: clearing bot erases the id, others erase own guids via `find` (`:650-668`) | Drop `extern` from `.h:151`. `clearInstance` → `Reset`; per-bot path → `Find`, never `For` (a `For` would recreate the entry and re-arm the "entry exists" reset trigger every tick). Trigger `UldTriggers_Algalon.cpp:32` → a helper in the encounter module calling `Find` |
| `UldEncounter_Vezax.cpp:32` | Same pattern (`:567-582`) | Same as Algalon; trigger `UldTriggers_Vezax.cpp:35` |
| `UldEncounter_Razorscale.h:128,133` | Never | Move both maps into one struct in a per-instance store; drop the class statics |
| `UldEncounter_FlameLeviathan.cpp:113` | Fields reset in place when boss null/out of combat (`:247-262`) | Own reset |
| `UldEncounter_Hodir.cpp:163` | Per-bot entries erased in getters; `blockRank` cleared at `:1284` | Own reset |
| `UldEncounter_Ignis.cpp:47`, `:341` | Inner entry erased when construct Brittle/dead (`:202`); `:341` never | Two stores |
| `UldEncounter_IronAssembly.cpp:76` | Reset node erases only the caller's slot | Remove the whole-map lock in `HasState`/`Reset` (`:1404, :1415`) |
| `UldEncounter_Mimiron.cpp:883`, `:936`, `:941` | Never; fight state replaced by `ResetMimironFightState` (`:1276-1283`) | Three stores; `:941` currently shares `:935`'s mutex. Replacement → `Reset` or keep assignment, behaviour identical |
| `UldEncounter_Thorim.cpp:98` | Reset node clears latches in place (`:2749-2831`) | Own reset |
| `UldEncounter_YoggSaron.cpp:118`, `:1080`, `:1833` | Fields reset in place; `:1833` prunes by TTL on read | Three stores. Remove the extra re-locks of the global mutex around field use (`:559, 569, 690, 703, 836, 1365, 1419, 1456, 1491, 1952, 2037`) and the whole-lock in `:1094` and the `:1833` accessors |
| `EoE/Util/EoEEncounter_Drakes.cpp:60` | Never | |
| `EoE/Util/EoEEncounter_Malygos.cpp:77` | Never; caches time-refreshed, layout latch dropped on phase 0 or >5 min (`:291-301`) | Creature cache (`:67`) stays inside the state struct |
| `OS/Util/OSEncounter.cpp:68` | Whole value reset on boss guid change, out of combat, or unseen >15 s (`:81-87`) | Function-local static → namespace-scope store; reset → `Reset` or keep assignment |

Behaviour must not change except: state no longer survives the map being destroyed.

## Out of scope

- Naxx (`Naxx/NaxxBossHelper.h:1370, 1694, 1722, 2097, 2756`): upstream still edits that file.
- ICC and RS: have their own reset paths; ICC already clears on `OnDestroyMap`.
- Per-bot and per-pass caches (`RazorscaleScan`-style values, `thread_local` pass caches).
- Unlocked `extern` maps in BT, SSC, SWP, Hyjal, Mag, Kara, TK, ZA and RS (`RSActions_SAV.cpp:23,42`,
  `RSActions_BAL.cpp:52`): same race; recorded as a known gap, fixed later by moving them to this store.
- Merging a boss's several stores into one struct.

## Commits

Close each per `CLAUDE.local.md`: syntax check, pblint, docs, commit (stage only files this commit
touched; other sessions edit the tree).

**Status:** commit 1 landed, with its docs; continue at commit 2. The store's section in
`docs/raids/README.md` §"Per-instance state" is the rule later commits follow.

1. **Store, hook, test, race fixes.** `RaidInstanceState.{h,cpp}`, `Playerbots.cpp` registration,
   `tools/nativetest/`, Algalon, Vezax, Razorscale. Docs: see below. This plan file goes in this commit.
2. **Ulduar, one commit per large boss**: Yogg-Saron, Mimiron, Thorim, Hodir; then one commit for Flame
   Leviathan, Ignis, Iron Assembly.
3. **EoE** (Drakes, Malygos).
4. **OS.** Then fold what shipped into the permanent docs and delete this plan.

## Docs (commit 1 unless noted)

- `docs/raids/README.md`: add a `RaidInstanceState` section beside the `RaidAntiFear` one: what it
  owns, destroy-hook eviction, the one-thread-per-instance rule, "boss code keeps its own reset rules",
  never `For` in a has-state check. Add the unlocked TBC/RS maps as a known gap. Fix `:126` and `:148`,
  which still cite `RaidBossHelpers.h` (now `src/Util/EncounterHelpers.h`).
- `docs/engine/raid-mechanics-lessons.md` §"Coordinating a raid with no shared state", bullet "Latch
  per instance": keep the thread_local rationale, point to `RaidInstanceState` instead of describing the
  hand-rolled mutex + map.
- `docs/raids/naxxramas.md:85`: says `AnubrekhanBossHelper` keeps no clock; it does
  (`NaxxBossHelper.h:1625-1682`, read at `NaxxActions_Anubrekhan.cpp:303`).
- `docs/raids/ulduar/README.md` or per-boss files: nothing unless a boss's reset behaviour is
  described there and changed.
- Run `/compact-docs-writer` before these edits.
- Memory `eoe-upstream-pr-prep`: after commit 3, record that the EoE upstream PR must carry
  `RaidInstanceState.{h,cpp}` and its registration.

## Verification

- Every commit: `~/.claude/scripts/pb-syntax-check.sh` on changed files; the header is widely
  included, so raise `PB_MAX_FANOUT` to cover every includer. `tools/pblint/pblint.py` on changed
  paths. Native test run on commit 1 and whenever the header changes.
- In game (user): Algalon, Vezax, Yogg-Saron, Mimiron, one EoE pull, one OS pull; compare their
  RaidObs/botobs traces against a recent run. Also re-enter a saved Ulduar lockout after the map
  unloads and confirm a fresh pull starts from clean state.
