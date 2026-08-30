# Obsidian Sanctum — wire the strategy into RaidObs

## Context

The Sartharion 3-drake strategy (`src/Ai/Raid/OS/`) has been reworked over rounds A–M, each round
driven by the user pulling in game and reporting what went wrong. That loop is slow because a wipe
leaves nothing to read: the diagnosis is eyeballed off the raid frame.

RaidObs already writes a trace for this fight. `615_1_sartharion_1787592359.ndjson` exists on the
worldserver, so the pull opens a session on `SetBossState(DATA_SARTHARION, IN_PROGRESS)`, the file is
slugged `sartharion`, and positions, damage, auras, action verdicts, vetoes and moves are all being
recorded. Config is default (`Obs.Enabled = 1`, `Obs.Maps = ""` → every raid map; no `AC_*OBS*`
override in the container), so nothing has to be turned on.

What is missing is the whole `note` stream — the OS strategy stores and derives every decision it
makes in bare types, so a trace shows where a bot went and never why. Two of the three things this
fight is lost to are invisible today:

- **Which corridor each bot thought it should hold.** `SafeCorridorY` and `ClassifyTsunamiWave` are
  derived fresh per bot per tick and stored nowhere. Half the raid dodging to one gap and half to the
  other is the single most likely wipe cause and leaves no trace of the disagreement.
- **Where the lethal lane actually was.** Proven from the existing trace: it names 52
  `Twilight Egg (Cosmetic)` (entry 31103, faction 14) plus ~26 Onyx Sanctum trash (faction 103) inside
  the 150 yd snapshot sweep, against `OBS_MAX_WATCHED = 40`. The cap binds before a wave is reached,
  the wave never enters combat so `WatchCreature` never claims it, and Flame Tsunami's lethality is an
  aura on the creature (57492) rather than a dynamic object — so no `snap.hz` row either. Nothing puts
  the wave in the trace.

Outcome: after a pull, `postmortem.py <file> --notes sartharion.` reads back what every bot decided
and where the waves were, so the next round starts from evidence.

Rules this follows, from [docs/systems/observability.md](docs/systems/observability.md): store
assignments in the traced containers so instrumentation follows the data; probe derived state inside
the helper that derives it, never at the call sites; probe the rule, not the coordinate; do not probe
what another stream already says; prefix every key with the encounter that owns it.

## Change

Note key prefix is **`sartharion.`** — it matches the trace file's own slug and the doc's "prefix
every note key with the encounter that owns it".

### 1. Traced containers — `src/Ai/Raid/OS/Util/OSEncounter.cpp`

`#include "RaidObs.h"`, then in `EncounterState` (line ~59) swap four members:

```cpp
RaidObs::ObsValue<bool> mainTankDragged{"sartharion.dragdone"};
RaidObs::ObsValue<bool> burstWindowOpen{"sartharion.burstwindow"};
RaidObs::ObsValue<bool> tankCooldownWindowOpen{"sartharion.tankcdwindow"};
RaidObs::ObsGuidSet portalSquad{"sartharion.portalsquad"};
```

The three bools read and write through `ObsValue`'s conversion and `operator=(T)` unchanged. The squad
needs two edits: `ResolveAssignments` uses `insert` instead of `push_back`, and `PortalSquadMember`
uses `count(bot->GetGUID()) != 0` instead of `std::find`. Order was never read.

`fightStartMs`, `lastSeenMs`, `mainTankDragArrivedMs`, `mainTankDragStartedMs`, `assignmentsResolved`
and `offTankWarned` stay bare — timestamps and bookkeeping, which the doc says not to trace.

**Watch at build time.** `StateFor` resets with `state = EncounterState();`. Neither container declares
a copy-assignment operator of its own (`ObsValue::operator=(T)` is not one), so the implicit member-wise
copy applies and the reset is silent — correct, because a wipe reset is not an assignment, and the
per-tick out-of-combat rebuild must not emit note churn. If a compiler disagrees, replace the
whole-struct assignment with an explicit `Reset()` that assigns each member.

### 2. Derived probes

Each is one `RaidObs::NoteDerived` call wrapped in `if (RaidObs::Active())`, because `NoteDerived`
takes `std::string const&` and the value is built before any gate inside it can run. Emission is
change-only, so a bot holding one state all fight costs one record.

| key | site | value |
|---|---|---|
| `sartharion.wave` | `ClassifyTsunamiWave` — OSGeometry.cpp:122 | `none` / `left` / `right` |
| `sartharion.corridor` | `SafeCorridorY` — OSGeometry.cpp:211 | `tank 513.0`, `melee 535.5`, `raid 551.0` |
| `sartharion.offtank` | `OffTankAnchor` — OSGeometry.cpp:557 | `shadron spot` / `shadron corridor` / `none spot` |
| `sartharion.target` | `PriorityTarget` — OSEncounter.cpp:555 | `DescribeAssignment(guid)` or `none` |
| `sartharion.defensive` | `NextTankDefensive` — OSEncounter.cpp:506 | cast name / `covered` / `none` |
| `sartharion.drag` | `OsMainTankHoldAction::Execute` — OSActions_Tank.cpp:45 | `walking` / `onpoint` / `settled` / `timeout` |
| `sartharion.realm` | `TwilightPortalEnterTrigger::IsActive` — OSTriggers.cpp:181 | `enter` / `notworth` / `noportal` |

Why each earns its place:

- **wave / corridor** are the fight. Both are derived per bot against the bot's own X, so bots
  legitimately disagree, and the disagreement is what a corridor bug looks like. `corridor` carries the
  group name and the hold, not the raw float — the group is the rule, the Y says which of the two holds
  it picked.
- **offtank** names the drake and whether the wave overrode the spot's Y. `move` already carries the
  coordinate; what it cannot say is which drake produced it.
- **target** is Thorim's `dpstarget` argument verbatim: the trace otherwise shows `GetVictim()`, which
  is where a bot ended up rather than where it was sent.
- **defensive** — the act stream says `os main tank cooldown` succeeded or failed but never which
  button, and the whole point of round M was the order. `covered` is the one-at-a-time bail (an aura
  from the table still running), `none` is nothing off cooldown.
- **drag** is a four-branch state machine with a 45 s timeout whose only current output is a
  `LOG_WARN` that never reaches the trace.
- **realm** — the aura stream already carries Gift of Twilight and Twilight Torment, and
  `sartharion.portalsquad` carries membership, but whether a portal was in range is in no stream at
  all, and a trigger that returns false writes nothing.

`ClassifyTsunamiWave`, `SafeCorridorY`, `NextTankDefensive` and `PriorityTarget` all return from inside
a loop or switch; each becomes compute-into-a-local, probe once, return.

### 3. Flame Tsunami lanes — `src/Ai/Raid/OS/Util/OSGeometry.{h,cpp}`

New `void NoteTsunamiHazards(Player* bot)`, deduped on a bare
`std::unordered_set<ObjectGuid> tsunamiTraced` in `EncounterState`, so each wave creature is written
once:

```cpp
RaidObs::NoteHazard(bot->GetMap(), SpellId::FlameTsunamiDamageAura, tsunami->GetPosition(), "wave",
                    "\"y\":524,\"side\":\"left\",\"half\":8.5", TSUNAMI_HAZARD_TTL_MS);
```

`y` is the line, `side` the pattern, `half` the `TSUNAMI_LETHAL_HALF_WIDTH` the geometry is built on.
TTL 11000 ms — `SendLavaWaves(false)` strips the damage aura 11 s after the summon.

Called from `SartharionEncounterActive` (OSEncounter.cpp:140), which already holds the boss and the
state and is the one predicate every trigger runs every tick, gated on
`RaidObs::Active() && IsMechanicTrackerBot(bot, OS_MAP_ID)` — `EncounterHelpers` is already included
there. One bot per instance runs it, so the cost is one extra `GetCreatureListWithEntryInGrid` per
tick while a trace is open, against the ten per bot per tick `ClassifyTsunamiWave` already spends.

This is the `haz` channel used for exactly what the doc reserves it for ("a cone, **a rolling wave**"),
and the note stream does not make it redundant: `haz` is where the lane was, `sartharion.wave` is what
each bot believed about it, and a corridor bug is those two disagreeing.

### 4. Docs

- [docs/systems/observability.md](docs/systems/observability.md) — add Obsidian Sanctum to the
  "Converted:" list near the end of *Adding a probe*.
- [docs/raids/obsidian-sanctum.md](docs/raids/obsidian-sanctum.md) — a short section naming the keys
  and the one command that reads them back.

Per the user's `compact-governing-docs` rule, invoke `/compact-docs-writer` **before** the first doc
edit, not as cleanup.

### Deliberately not probed

- `OsMechanicPriorityMultiplier::Live()` — every suppression is already a `veto` record naming the
  multiplier and the action it zeroed.
- `SartharionBurstWindowMultiplier`'s Gift of Twilight interlock — same `veto` stream, and the boss
  aura is in the aura stream.
- Every dodge and hold destination — already a `move` record whose `by` names the issuing action.
- The fissure dodge branch — `move.by` plus the destination says whether X or Y moved.

## Files

| file | change |
|---|---|
| `src/Ai/Raid/OS/Util/OSEncounter.cpp` | `RaidObs.h`; four traced members + `tsunamiTraced` in `EncounterState`; squad insert/count; probes in `PriorityTarget`, `NextTankDefensive`; the hazard call in `SartharionEncounterActive` |
| `src/Ai/Raid/OS/Util/OSGeometry.cpp` | `RaidObs.h`; probes in `ClassifyTsunamiWave`, `SafeCorridorY`, `OffTankAnchor`; `NoteTsunamiHazards` |
| `src/Ai/Raid/OS/Util/OSGeometry.h` | declare `NoteTsunamiHazards` |
| `src/Ai/Raid/OS/Util/OSData.h` | `TSUNAMI_HAZARD_TTL_MS` |
| `src/Ai/Raid/OS/Action/OSActions_Tank.cpp` | `sartharion.drag` probe |
| `src/Ai/Raid/OS/OSTriggers.cpp` | `sartharion.realm` probe |
| `docs/systems/observability.md`, `docs/raids/obsidian-sanctum.md` | as above |

No schema change, so `SCHEMA_VERSION` and `SUPPORTED_SCHEMA` stay put, and `postmortem.py` is
raid-agnostic and needs no edit.

## Verification

**Static**

1. `python apps/codestyle/codestyle-cpp.py` — no new findings under `src/Ai/Raid/OS/`, no line over 110.
2. Every `NoteDerived` and `NoteHazard` call sits inside an `if (RaidObs::Active())`; grep for a bare
   one. Cost is paid at the call site because the value is a `std::string`.
3. Every key in the code matches the table above and starts `sartharion.`; grep both directions.
4. `state = EncounterState();` still compiles — the one real build risk, see §1.
5. `portalSquad` has no remaining `push_back` or `std::find` caller.

**In game** — one 3-drake pull, then `postmortem.py <newest 615 file>`:

- `--notes sartharion.` is non-empty. Before this change it is empty; the existing trace has zero
  `note` records.
- `--notes sartharion.corridor` shows the tank on `tank 513.0` and the raid on `raid 535.5` between
  waves, both flipping together when a wave goes out. Two bots of the same group on different holds at
  the same `t` is the bug this exists to catch.
- `--notes sartharion.wave` flips `none`→`left`/`right`→`none` around each volley, and the `haz` rows
  in the same window name the three lanes that volley used.
- `sartharion.dragdone` goes true once, early, with `sartharion.drag` reading `settled` and not
  `timeout`.
- `sartharion.burstwindow` flips when Shadron lands, `sartharion.tankcdwindow` when he passes 50%, and
  `sartharion.defensive` then names buttons in weakest-first order with `covered` between them.
- `sartharion.portalsquad` lists the squad once, at the pull, and the members that go in match it.
- A wipe closes the file `out: wipe` and the next pull opens a fresh one with the latches re-armed.

**Not in scope:** no build (the module cannot be compiled headless here), and no git operation —
nothing is committed without an explicit instruction naming the command.

## Adjacent, not included

`docs/raids/obsidian-sanctum.md` is stale against rounds L and M — line 93 still quotes the tank hold
as `(3221.3743, 511.089)` and line 94 `tank's, 511.089`, and there is nothing on the held tank
cooldowns or the burst window moving to Shadron's landing. Out of scope here; worth its own pass.
