# RaidObs review fixes 1–10 and 12

## Context

A code review of the observability framework (branch `Custom`, commits `d337b7e19`/`878fb640c`) produced
15 findings. The user asked to fix 1–10 and 12. Skipped deliberately: #11 (pre-roll gating), #13
(`postmortem.py` streaming), #14 (dead `NoteHazardCircle`), #15 (unused `Map*` param).

The theme across the accepted findings: the trace either records the **wrong thing** (guid collisions,
misattributed actions, "refused" that means "walking"), the **missing thing** (bosses already in combat,
chase/follow movement), or **too much** (`act` spam that truncates the file). Three of them are latent
correctness bugs with no symptom until a clock wraps, a memory model reorders, or a stat fails.

Several fixes change record fields, so `SCHEMA_VERSION` goes 2 → 3 and `postmortem.py` +
`docs/systems/observability.md` move with it — the doc mandates this ("Bump `SCHEMA_VERSION` in
`RaidObs.h` and `SUPPORTED_SCHEMA` in `postmortem.py` on any field change").

## Critical files

| File | Findings |
|---|---|
| [src/Bot/Obs/RaidObs.h](src/Bot/Obs/RaidObs.h) | 4, 6, 8 + schema bump |
| [src/Bot/Obs/RaidObs.cpp](src/Bot/Obs/RaidObs.cpp) | 2, 3, 5, 7, 9, 10, 12 |
| [src/Bot/Engine/Engine.cpp](src/Bot/Engine/Engine.cpp) | 1, 6 |
| [src/Ai/Base/Actions/MovementActions.h](src/Ai/Base/Actions/MovementActions.h) / [.cpp](src/Ai/Base/Actions/MovementActions.cpp) | 7, 10 |
| [tools/botobs/postmortem.py](tools/botobs/postmortem.py) | reader side of 2, 7, 10 |
| [docs/systems/observability.md](docs/systems/observability.md) | schema table + semantics |

---

## 1. `act` spam — PREREQ probe fires unconditionally

[Engine.cpp:233](src/Bot/Engine/Engine.cpp#L233). The probe sits above the prerequisite check, so a
default-pushed action (`skipPrerequisites == false`) logs `PREREQ` every tick even when
`MultiplyAndPush` returns false and execution falls through to `OK`. The alternating pair defeats
`NoteAction`'s `(lastAction, lastVerdict)` dedup at [RaidObs.cpp:1204](src/Bot/Obs/RaidObs.cpp#L1204).

Move both the `LogAction` and `ObsVerdict` calls **inside** the `if (MultiplyAndPush(...))` branch,
before `PushAgain`, so the verdict is recorded only when the tick actually yields to a prerequisite:

```cpp
if (!skipPrerequisites)
{
    if (MultiplyAndPush(actionNode->getPrerequisites(), relevance + 0.002f, false, event, "prereq"))
    {
        LogAction("A:%s - PREREQ", action->getName().c_str());
        ObsVerdict(botAI, action, relevance, "PREREQ");
        PushAgain(actionNode, relevance + 0.001f, event);
        continue;
    }
}
```

## 2. Guid collision between creatures and players

`GuidLow` ([RaidObs.cpp:128](src/Bot/Obs/RaidObs.cpp#L128)) returns `ObjectGuid::GetCounter()`, which is
a separate numbering space per guid type — a creature and a player both start at 1. Every record key
(`unit.g`, `dmg.s/d`, `cast.s`, `death.killer`, `death.dist`, `snap` row 0 and the target column) is
therefore ambiguous, and `s.seenUnits` at [:377](src/Bot/Obs/RaidObs.cpp#L377) suppresses the `unit`
record for the colliding creature — so it never lands in `trace.bosses` and `--track` shows no boss
column.

Replace with a type-tagged 64-bit key. Players keep their bare counter (they are the bulk of every
snapshot, so the file does not grow), everything else gets a small tag in the high half, and the whole
range stays inside JSON's exact-integer window:

```cpp
// The counter is a separate numbering space per guid type - a creature and a player both start at 1 -
// so it cannot key a record on its own. Players keep the bare counter because they dominate every
// snapshot; the tag stays small enough that the result is still an exact JSON integer.
uint64 GuidKey(ObjectGuid guid)
{
    uint64 tag;
    switch (guid.GetHigh())
    {
        case HighGuid::Player:        tag = 0; break;
        case HighGuid::Unit:          tag = 1; break;
        case HighGuid::Pet:           tag = 2; break;
        case HighGuid::Vehicle:       tag = 3; break;
        case HighGuid::GameObject:    tag = 4; break;
        case HighGuid::DynamicObject: tag = 5; break;
        default:                      tag = 7; break;
    }

    return (tag << 32) | guid.GetCounter();
}
```

`ObjectGuid::Empty` yields 0, so `0` still reads as "none" everywhere it already does.

Mechanical follow-through in the same file: rename `GuidLow` → `GuidKey` at all 25 call sites, widen
`ObsSession::seenUnits` to `std::unordered_set<uint64>`, `ObsSession::bots` to
`std::unordered_map<uint64, BotTrace>`, `DamageEntry::source` to `uint64`, and the `low` locals in
`EnsureUnit`/`NoteDeath`. `DescribeAssignment(ObjectGuid const&)`
([RaidObs.cpp:1461](src/Bot/Obs/RaidObs.cpp#L1461)) must use `GuidKey` too — it is in the same TU, so
the anonymous-namespace helper is visible.

## 3. Bosses already in combat are never watched

`s.watched` is only ever inserted from `OnCreatureEngage`
([RaidObs.cpp:1060](src/Bot/Obs/RaidObs.cpp#L1060)). A session opened by `MarkPull` (Thorim's gauntlet,
[UldEncounter_Thorim.cpp:429](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L429)) or by the `bossstate`
path arrives *after* those creatures entered combat, so `snap.u` carries players only and `death.dist`
is `{}`.

Two changes:

**Seed on open.** Add a `SeedWatched(ObsSession& s, Unit* source)` helper next to `PruneWatched`
([RaidObs.cpp:576](src/Bot/Obs/RaidObs.cpp#L576)) that walks `map->GetPlayers()` → `player->getAttackers()`,
inserting every creature found, plus `source` itself. Call it from `OpenSession` after the `pull`
record is emitted (so the `unit` records it writes land after `hdr`/`pull`) and before the session is
published to the registry.

**Bosses outrank trash for the cap.** `OBS_MAX_WATCHED` (40) is filled first-come, so a boss engaging
after 40 adds is silently dropped. In both `SeedWatched` and `OnCreatureEngage`, when the set is full
and the incoming creature is boss-flagged, evict one non-boss entry instead of skipping. Keep the
existing boss test (`isWorldBoss() || IsDungeonBoss()`); re-resolving a stored guid to a `Creature*`
for the eviction scan uses `s.map->GetCreature`, the same call `PruneWatched` already makes.

## 4. `g_active` is an unsynchronized cross-thread bool

`RefreshActiveFlag` ([RaidObs.cpp:297](src/Bot/Obs/RaidObs.cpp#L297)) writes it under `g_registryMutex`
on one map thread; `Active()` ([RaidObs.h:40](src/Bot/Obs/RaidObs.h#L40)) reads it lock-free from every
other map thread's hot path. Nothing pairs release with acquire — the `fetch_add` on
`g_registryGeneration` is a different object no reader touches first — so a thread can hold the stale
`false` and record nothing but snapshots for a whole pull.

```cpp
extern std::atomic<bool> g_active;
inline bool Active() { return g_active.load(std::memory_order_acquire); }
```

Definition becomes `std::atomic<bool> g_active{false};`, and `RefreshActiveFlag` stores with
`std::memory_order_release`. `RaidObs.h` needs `<atomic>`. Replace the ~12 bare `if (!g_active ...)`
guards inside `RaidObs.cpp` (`NoteDamage`, `NoteHeal`, `NoteAbsorb`, `NoteAura`, `NoteCast`,
`NoteAction`, `NoteVeto`, `NoteMove`, `Note`, `NoteAssignment`, `NoteHazard`, `NoteDeath`) with
`Active()` so they all take the acquire path.

## 5. `Stamp` breaks on the 32-bit millisecond wrap

`ObsSession::Stamp` ([RaidObs.cpp:224](src/Bot/Obs/RaidObs.cpp#L224)) subtracts raw `getMSTime()`
values widened to `int64`. `getMSTime()` wraps every ~49.7 days, so a session straddling the wrap
returns ≈ -4.29e9 instead of a small positive number, inverting the negative-`t`-means-pre-roll
convention for the rest of the file.

`getMSTimeDiff` cannot be used directly — it is unsigned and the pre-roll case is legitimately
negative. Let the unsigned subtraction wrap and reinterpret it as signed, which is exact for any gap
under ~24.8 days:

```cpp
// getMSTime() wraps every ~49.7 days. Unsigned subtraction wraps with it, so reading the result back
// as int32 gives the true signed gap in both directions - pre-roll samples are older than the start
// and must stay negative, which is what marks them as leading up to the pull.
int64 Stamp(uint32 ms) const { return static_cast<int32>(ms - startMs); }
```

## 6. `ExecuteAction` misattributes movement, and nesting clobbers the outer action

`DoNextAction` sets the current action around `ListenAndExecute` and then resets it to `nullptr`
([Engine.cpp:248](src/Bot/Engine/Engine.cpp#L248)/[:252](src/Bot/Engine/Engine.cpp#L252));
`ExecuteAction` ([Engine.cpp:365](src/Bot/Engine/Engine.cpp#L365)) never sets it at all. So a
`DoSpecificAction` outside a tick emits `move` with `by: ""`, and the same call from inside another
action's `Execute()` is blamed on the **outer** action. The `nullptr` reset is the second half of the
same bug: a nested execute clears the outer action's attribution for the rest of its own tick.

Add an RAII scope beside `BotContext` in `RaidObs.h`, which also removes the fragile "held across the
call because SetCurrentAction stores the pointer" dance in `Engine.cpp`:

```cpp
// Publishes the action a movement command should be attributed to, restoring the previous one on the
// way out - a nested DoSpecificAction executes a second action inside the first, and clearing to null
// would strip the outer action's attribution for the rest of its own tick. Holds its own copy because
// Action::getName() returns by value.
class ActionScope
{
public:
    explicit ActionScope(std::string action);
    ~ActionScope();

    ActionScope(ActionScope const&) = delete;
    ActionScope& operator=(ActionScope const&) = delete;

private:
    std::string _action;
    char const* _previous;
};
```

Use it at both sites, constructed only when a trace is open so an idle server never copies the name
(the doc's "pass the object, never the name" constraint):

```cpp
std::optional<RaidObs::ActionScope> obsAction;
if (RaidObs::Active())
    obsAction.emplace(action->getName());

actionExecuted = ListenAndExecute(action, event);
```

`SetCurrentAction` stays as the primitive `ActionScope` is built on; drop the manual
`SetCurrentAction(nullptr)` at [Engine.cpp:252](src/Bot/Engine/Engine.cpp#L252).

## 7 & 10. Movement: refusal reasons, and the generators that were never instrumented

These two share one record, so they land together.

**7 — `ok: 0` conflates "still walking" with "refused."** `MoveToImpl` returns false from
`IsDuplicateMove` ([MovementActions.cpp:193](src/Ai/Base/Actions/MovementActions.cpp#L193)) and
`IsWaitingForLastMove` ([:197](src/Ai/Base/Actions/MovementActions.cpp#L197)) — the ordinary state
while a bot is mid-spline — and also from `IsMovingAllowed` and a failed path search. `postmortem.py`
prints all four as `[REFUSED]`.

Give `MoveToImpl` a reason. It has exactly one caller (`MoveTo`), so the return type is free to change:

```cpp
enum class MoveOutcome : uint8
{
    Issued,      // handed to the MotionMaster
    Duplicate,   // same destination as last tick
    Waiting,     // still walking to the previous destination
    NotAllowed,  // IsMovingAllowed said no
    NoPath,      // path search found nothing
};
```

`MoveTo` maps `Issued` → `true` for its callers and passes the outcome to `NoteMove`. The trailing
`return false` at [MovementActions.cpp:286](src/Ai/Base/Actions/MovementActions.cpp#L286) covers both
"already there" (`distance <= 0.01f`) and `modifiedZ == INVALID_HEIGHT`; report the former as `Issued`
(the destination was accepted, the bot is standing on it) and the latter as `NoPath`.

**10 — `Follow`, `ChaseTo` and `JumpTo` drive the MotionMaster directly.**
[MovementActions.cpp:1324](src/Ai/Base/Actions/MovementActions.cpp#L1324) (`MoveFollow`),
[:1358](src/Ai/Base/Actions/MovementActions.cpp#L1358) (`MoveChase`) and
[:78](src/Ai/Base/Actions/MovementActions.cpp#L78) (`MoveJump`) bypass `MoveTo` entirely, so the two
generators bots spend most of a fight in produce no records at all.

Extend `move` rather than adding a record type — the reader, the death-record latch and the `by`
attribution all already work:

| field | meaning |
|---|---|
| `k` | `"point"` (existing behaviour), `"jump"`, `"follow"`, `"chase"` |
| `tgt` | followed/chased unit guid; `0` for point and jump |
| `r` | `""` when issued, else `"dup"`, `"wait"`, `"blocked"`, `"nopath"` |

`ok` stays as-is (`1` iff `MoveOutcome::Issued`) so nothing that reads it breaks.

`NoteMove` grows a `kind` and a target guid parameter. Its dedup latch needs to split by kind:
`point`/`jump` keep the existing coordinate comparison, while `follow`/`chase` have **no fixed
destination** — the target moves every tick, so comparing coordinates would emit constantly. Latch
those on `(by, kind, targetGuid)` and emit only when the bot starts following/chasing something
different. Record the target's position at issue time in `x/y/z` so `--track` still has a point to
draw.

`death.lastmove` keeps latching only `point`/`jump` — a chase has no destination for `arrived` to be
measured against.

Probe placement: `JumpTo` already funnels its three early-outs the same way `MoveToImpl` does, so give
it the same `MoveOutcome` treatment. `Follow` and `ChaseTo` return `bool` and are called from ~20
sites; leave their signatures alone and call `NoteMove` at each success point, plus at the
`FOLLOW_MOTION_TYPE`-already-on-target early-out in `Follow`
([:1318](src/Ai/Base/Actions/MovementActions.cpp#L1318)) as `r: "dup"`.

## 8. `ObsGuidSet::erase` and `ObsGuidMap::erase` disagree

`ObsGuidSet::erase(guid)` ([RaidObs.h:266](src/Bot/Obs/RaidObs.h#L266)) emits; `ObsGuidMap::erase(guid)`
([RaidObs.h:238](src/Bot/Obs/RaidObs.h#L238)) is silent. The header comment at
[:215](src/Bot/Obs/RaidObs.h#L215) only exempts **iterator**-erase, which is what the prune loops use —
the silent `erase(ObjectGuid)` on the map is unintended, not the loud one on the set.

Every `ObsGuidMap::erase(guid)` call site is a genuine state clear, not a prune — `ResetThorimEncounterState`
([UldEncounter_Thorim.cpp:1102](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L1102)), the Algalon and
Vezax per-bot resets, and the "boss no longer present, drop the latch" checks in `BTActions.cpp`,
`HyjalActions.cpp` and `SSCActions.cpp`. The only real prune loop is
[UldEncounter_Thorim.cpp:123](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L123), which already uses
iterator-erase.

So converge **upward**: make `ObsGuidMap::erase(ObjectGuid)` emit `NoteAssignment(guid, kind, "0")` when
it removed something, matching `ObsGuidSet`. Iterator-erase stays silent on both. Update the header
comment and the matching paragraph in `docs/systems/observability.md` to say exactly that — that the
exemption is iterator-erase, not erase-by-guid.

## 9. Retention deletes files it could not stat

`ApplyRetention` ([RaidObs.cpp:825](src/Bot/Obs/RaidObs.cpp#L825)) ignores the `error_code` from
`file_size` and `last_write_time`, so a trace another process holds open becomes `{path, epoch, 0}` —
which sorts first, reads as decades old, and is removed. The single shared `ec` is also reused across
the loop, so one earlier failure leaves it set for every later call.

Use a fresh `std::error_code` per call, and skip the entry entirely when either fails:

```cpp
for (auto const& item : std::filesystem::directory_iterator(dir, ec))
{
    std::error_code itemEc;
    if (!item.is_regular_file(itemEc) || itemEc || item.path().extension() != ".ndjson")
        continue;

    uint64 const size = static_cast<uint64>(item.file_size(itemEc));
    if (itemEc)
        continue;

    auto const written = item.last_write_time(itemEc);
    if (itemEc)
        continue;

    // A file another process still holds open reports nothing; treating that as zero-byte and
    // epoch-old would delete a trace mid-copy.
    entries.push_back({item.path(), written, size});
    total += size;
}
```

## 12. `NoteCast` filters in the wrong order

[RaidObs.cpp:1164](src/Bot/Obs/RaidObs.cpp#L1164). `OnSpellPrepare` fires for every cast on the server
and player casts dominate, yet `SessionFor(caster)` (map tests plus a thread-local memo lookup) runs
before the `TYPEID_PLAYER` test that discards them at
[:1170](src/Bot/Obs/RaidObs.cpp#L1170). Hoist the type test above `SessionFor`, keeping its comment.

---

## Reader and docs

**[tools/botobs/postmortem.py](tools/botobs/postmortem.py)**
- `SUPPORTED_SCHEMA` 2 → 3.
- `Trace.name` renders an unknown guid as `#<tag>:<counter>` (`tag = guid >> 32`, `counter = guid & 0xFFFFFFFF`) instead of `#<guid>`, so a creature and a player are visibly different units.
- `show_bot` move line: `[REFUSED]` only for `r` in `blocked`/`nopath`; `dup`/`wait` render as `[waiting]`; print `k` and resolve `tgt` through `trace.name` for follow/chase.

**[docs/systems/observability.md](docs/systems/observability.md)**
- Schema heading `v: 2` → `v: 3`; `move` row gains `k`, `tgt`, `r`.
- Replace "Guids are `ObjectGuid::GetCounter()`" with the tagged-key description, noting players are untagged.
- Rewrite the `ok: 0` paragraph — a refusal is now `r`, and `dup`/`wait` are the normal mid-spline state, not a refusal.
- Fix the "Adding a probe" paragraph that currently says erase emits nothing: the exemption is iterator-erase only.
- Note that `move` now covers `Follow`, `ChaseTo` and `JumpTo`, not just the `MoveTo` funnel.

## Verification

The module cannot be compiled headless in this environment (it needs the local core additions from
commit `208764946` and a full AzerothCore build tree), so this is static verification plus a hand-off:

1. **Static sweep** — `grep -rn "GuidLow" src/` returns nothing; `grep -rn "g_active" src/` shows only the definition, the release store and the acquire load; every `MoveToImpl`/`JumpTo` return path yields a `MoveOutcome`; `SCHEMA_VERSION` and `SUPPORTED_SCHEMA` both read 3.
2. **Reader against a synthetic trace** — hand-write a small NDJSON fixture into the scratchpad exercising a creature/player counter collision, a negative pre-roll `t`, and each `move` `k`/`r` combination, then run all five `postmortem.py` modes over it. This is the one piece that runs end-to-end here.
3. **Hand-off for the server run** — user builds, then on a live pull: `.playerbots debug obs` shows the trace open; `grep '"e":"unit"' <trace>` shows the boss with a tagged guid; `grep -c '"e":"act"' <trace>` is dramatically lower than a pre-fix trace of comparable length (finding 1); `postmortem.py <trace> --track <bot>` prints a boss distance column on a `MarkPull`-opened gauntlet trace (finding 3); `grep '"k":"chase"' <trace>` is non-empty (finding 10).
