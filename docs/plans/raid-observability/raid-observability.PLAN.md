# Raid Observability Framework (`RaidObs`)

## Context

Ulduar boss strategies are tuned by live raiding. The only forensic tools today are screen recordings
and `LOG_DEBUG("playerbots", …)`, and neither answers *why did that specific bot die?* Video shows the
outcome, not the decision. The whole of `src/Ai/Raid/Uld/` emits one log line — a `Yell` at
`Util/UldBossHelper.cpp:410` — so a wipe leaves no record of what any bot was told to do.

Two things already exist and must not be rebuilt:

- **`mod-chronicle`** (`modules/mod-chronicle/`) is installed and enabled
  (`configurationOverrides/Chronicle.env`), writing a full WotLK-format combat log per instance to
  `env/dist/logs/chronicle_logs/` and uploading to a local Chronicle server. It captures damage,
  heals, auras, casts, deaths, interrupts and `CHRONICLE_ENCOUNTER_START/END`. Current Ulduar file:
  152 MB / 1.02 M lines.
- **The core is a private fork** carrying 18 non-mainline observation hooks (commit `208764946`,
  "Port custom hooks for future Chronicle use"). Verified present in
  `src/server/game/Scripting/ScriptMgr.h:568-612` and `ScriptDefines/UnitScript.h:144-274`.
  mod-playerbots consumes none of them — it registers no `UnitScript` and no `GlobalScript`.

What chronicle fundamentally cannot record is **position** and **bot intent**. Nearly every avoidable
Ulduar death is positional (stood in Runic Smash, wrong lane, inside a Barrage cone, never reached a
ring slot) or a decision failure (an escape outranked the boss positioning action, two actions fought
over the `MotionMaster`, a multiplier vetoed the dodge). `docs/engine/raid-mechanics-lessons.md` is a
catalogue of exactly these.

**Outcome:** a per-pull NDJSON trace of every raid member's position, action verdicts, movement
commands, damage, heals, auras, hazards and deaths, plus a Python analyzer that turns a pull file into
a short per-death postmortem — so a fresh session can be handed a path and diagnose a wipe without
video.

## Decisions

Settled with the user; do not relitigate.

| # | Decision | Choice |
|---|---|---|
| 1 | What the trace must answer | Individual avoidable death, decision failure, assignment failure. **Raid-level (enrage, DPS check, healer OOM, threat) is out of scope** — chronicle covers it |
| 2 | Portability | **Fork hooks, unguarded.** No mainline fallback, no compile guard. Module CI (`core_build.yml` etc., which builds `mod-playerbots/azerothcore-wotlk`) will not compile this. Accepted — this is not going upstream |
| 3 | Breadth | All raids. Config map allowlist, default `""` = any map with an instance strategy applied |
| 4 | Format | **NDJSON** — one object per line, no enclosing array. `grep`-sliceable without parsing whole files |
| 5 | Live surface | Status command only. No `mark`, no death whisper — the user is playing and cannot type mid-pull |
| 6 | Engine instrumentation | Direct probes at all 7 verdict sites in `DoNextAction` plus the `MoveTo` funnel. Upstream merge conflicts accepted |
| 7 | Heals | Heals on raid members + healer mana in snapshots. No boss/add self-heals |
| 8 | Snapshot rate | **250 ms** — two samples per oscillation cycle, the minimum to see a bounce |
| 9 | Assignment tracking | `ObsLatch<T>` wrapper on encounter-state assignment fields. Instrumentation follows the data, not the call site |
| 10 | Pull naming | From the engaging creature, matched against the DBC encounter list for the canonical name, falling back to the raw creature name |
| 11 | Verdict filtering | Change-only steady state, plus a full unfiltered 10 s window before each death |
| 12 | Hazards | Dynamic objects (generic sweep) + hostile creatures in snapshots + helper-declared invisible hazards via `NoteHazard` |
| 13 | Humans | Traced like bots for everything not engine-derived. Flagged `h:1` |
| 14 | Retention | Age + total-size cap, defaults 7 days / 5 GB. No upload. `env/dist/*` is already gitignored |
| 15 | Schema | Short keys + `v` version integer in `hdr`, documented in `docs/systems/observability.md` |
| 16 | Pre-pull roll | 30 s rolling snapshot buffer per instance, flushed into the file when a session opens |
| 17 | Analyzer | Text only for v1. HTML pull view is a follow-up, not a gate |
| 18 | Build order | **Everything, then one verification pass.** Not staged |

## Architecture

One subsystem, `RaidObs`, raid-agnostic. It owns a per-instance **session**; a session owns one
buffered NDJSON file. Events arrive from core script hooks, the bot engine, and explicit calls from
encounter helpers.

### Session lifecycle

Registry: `std::unordered_map<uint32 /*instanceId*/, std::unique_ptr<ObsSession>>` behind a mutex.
Only the registry is locked — session bodies are touched solely by their own map thread, the same
reasoning `InstanceTracker` uses at `mod-chronicle/src/Chronicle.h:308`.

Open, on whichever comes first:

- `GlobalScript::OnBeforeSetBossState(id, newState, oldState, instance)` → `IN_PROGRESS` on a tracked map;
- `RaidObs::MarkPull(map, creature)` from encounter helper code — gauntlets, trash, mid-phase pulls;
- `UnitScript::OnUnitEnterCombat` where a boss-flagged creature engages a raid member.

Close on boss state → `DONE`/`FAIL`/`NOT_STARTED`, on `AllMapScript::OnDestroyMap`, or after
`Obs.IdleCloseSeconds` with no raid member in combat. Writes `end` with the outcome.

Path: `<LogsDir>/botobs/<map>_<instance>_<bossSlug>_<epoch>.ndjson`.

**Do not** hang session ownership on `IsMechanicTrackerBot` (`src/Ai/Raid/RaidBossHelpers.cpp:132`) —
it returns "first alive bot in the group on this map", so it reassigns the moment that bot dies, which
is exactly when a session must stay put.

### Gating cost

Static `bool RaidObs::s_active`, true only while ≥1 session is open. Every probe on a shared hot path
opens with that single load, so with nothing recording the framework costs one predictable branch.
Only past the gate does anything hash-lookup the instance.

### Sampler and pre-roll

Driven by `AllMapScript::OnMapUpdate(Map*, uint32 diff)` (`ScriptDefines/AllMapScript.h:97`, map
thread, `Map.cpp:529`). One call per map per tick regardless of bot count — far cheaper than 25 bots
self-sampling, and it avoids the per-bot group walk that `docs/engine/raid-mechanics-lessons.md` warns
about.

Every 250 ms, write one `snap` line holding an array of unit rows plus a hazard array. The session
caches the group roster and refreshes it every few seconds rather than walking `GetFirstMember()` per
sample.

**Pre-roll:** whenever a raid group is present on a tracked map with no session open, the same sampler
writes into a 30 s circular buffer instead of a file. On session open the buffer is flushed first, so
the trace begins 30 s before the pull. Snapshots only — no damage, no verdicts.

**Hazard sweep:** one `DynamicObject` grid sweep per snapshot per instance, modelled on
`GetDynamicObjectPositions` (`src/Ai/Raid/RaidBossHelpers.cpp:283`) but sweeping all dynamic objects
rather than filtering to one spellId. One sweep per instance per 250 ms is ~250× cheaper than what the
bots already do per-tick, so the grid-sweep warning does not bite here.

## Schema (`v: 1`)

`t` is milliseconds since the `hdr` record. Guids are `ObjectGuid::GetCounter()` (low part).

```jsonc
{"v":1,"e":"hdr","ts":<epochMs>,"map":603,"inst":3,"diff":1,"boss":"thorim",
 "chron":"instance_603_3_1787337683.log",
 "roster":[{"g":123,"n":"Name","c":<classId>,"r":"tank|heal|melee|ranged","h":0}]}

{"t":0,"e":"pull","boss":"thorim","src":"bossstate|mark|engage"}
{"t":..,"e":"end","out":"kill|wipe|reset"}

{"t":..,"e":"unit","g":..,"en":<entry>,"n":"Name","lvl":80,"mhp":..,"b":1}

// u rows: [guid, x, y, z, o, hp%, power%, target, moving, moveGenType, castingSpellId]
// hz rows: [spellId, x, y, z, radius]   — swept dynamic objects
{"t":..,"e":"snap","u":[[..]],"hz":[[..]]}

{"t":..,"e":"dmg","s":..,"d":..,"sp":..,"a":..,"ok":..,"sc":..,"ab":..,"rs":..,"hp":..}
{"t":..,"e":"heal","s":..,"d":..,"sp":..,"a":..,"oh":..,"hp":..}
{"t":..,"e":"aura","d":..,"s":..,"sp":..,"r":0}          // r=1 removed
{"t":..,"e":"cast","s":..,"sp":..,"tgt":..,"ct":<castTimeMs>}

{"t":..,"e":"act","g":..,"a":"action name","rel":60.0,"vd":"OK|FAILED|IMPOSSIBLE|USELESS|PREREQ|UNKNOWN"}
{"t":..,"e":"veto","g":..,"m":"multiplier name","a":"action name"}
{"t":..,"e":"move","g":..,"x":..,"y":..,"z":..,"ok":1,"by":"owning action name"}

{"t":..,"e":"note","g":..,"k":"slot|squad|role|phase|focus","txt":".."}
{"t":..,"e":"haz","sp":..,"x":..,"y":..,"z":..,"rad":..,"ttl":..}   // helper-declared, no world object

{"t":..,"e":"death","g":..,"killer":..,"x":..,"y":..,"z":..,
 "dist":{"<bossGuid>":24.3},
 "auras":[[sp,stacks,remainingMs]],
 "rewind":[[t,src,sp,amt]],          // last 15 s of damage, largest-first
 "acts":[[t,"action",rel,"vd"]],     // full unfiltered 10 s of verdicts
 "lastmove":{"x":..,"y":..,"z":..,"by":"..","arrived":0}}
```

**Emit on change, not on repeat.** A bot holding `thorim ring hold` for 60 s produces one `act`
record, not 120. Same for target, phase, assignment and move destination. Only `snap` is periodic.

Volume for a 6-minute 25-man, compact JSON: `snap` @250 ms ≈ 2.4 MB, `dmg` ≈ 11 MB, `heal` ≈ 2 MB,
`act` change-only ≈ 0.1 MB. Roughly 15–18 MB per pull.

## Hooks

All verified present. Register the **narrow** enabled-hook set — this core uses opt-in registration
(`ScriptMgrMacros.h:72`, `CALL_ENABLED_HOOKS`), and an empty vector enables everything for that script
type. Model class layout on `mod-chronicle/src/ChronicleLogs_SC.cpp:32-736`.

| Hook | Class | Use |
|---|---|---|
| `OnMapUpdate`, `OnDestroyMap`, `OnPlayerEnterAll` | `AllMapScript` | sampler, lifecycle |
| `OnSendSpellNonMeleeDamageLog(SpellNonMeleeDamage const*, int32 overkill)` | `UnitScript` | `dmg`, final post-mitigation |
| `OnSendAttackStateUpdate(CalcDamageInfo const*, int32 overkill)` | `UnitScript` | `dmg`, melee |
| `OnSendPeriodicAuraLog(Unit*, SpellPeriodicAuraLogInfo*)` | `UnitScript` | `dmg`/`heal`, ticks |
| `OnSchoolAbsorbApplied(DamageInfo&, SpellInfo const*, Unit*, uint32)` | `UnitScript` | absorb attribution |
| `OnSendHealSpellLog(HealInfo const&, bool critical)` | `UnitScript` | `heal` |
| `OnUnitDeath(Unit*, Unit* killer)` | `UnitScript` | `death` |
| `OnUnitEnterCombat(Unit*, Unit* victim)` | `UnitScript` | session open, boss identity |
| `OnAuraApplicationClientUpdate(Unit*, Aura*, bool remove)` | `GlobalScript` | `aura`, apply+remove in one |
| `OnBeforeSetBossState(uint32, EncounterState, EncounterState, Map*)` | `GlobalScript` | pull boundary |
| `OnSpellPrepare(Spell*, Unit*, SpellInfo const*)` | `AllSpellScript` | `cast`, boss telegraph |

Notes that cost time if missed:

- Prefer `GlobalScript::OnAuraApplicationClientUpdate` over `UnitScript::OnAuraApply` — the latter
  double-fires on the stack-refresh path (`Unit.cpp:4711` and `:4852`).
- `OnUnitDeath` fires at `Unit.cpp:14358` inside `Unit::Kill`, while `PlayerScript::OnPlayerJustDied`
  fires later from `Player::KillPlayer()` on a subsequent tick. Use `OnUnitDeath` — world state is
  still intact. It gives the killer but **not** the killing spell (`Unit::Kill`'s `spellProto` is
  unused, `Unit.cpp:14029`), which is why the rewind buffer exists.
- Leave `DealDamage` unoverridden: `ScriptMgr::DealDamage` (`ScriptDefines/UnitScript.cpp:52`) ignores
  the enabled-hook filter and fires for every registered `UnitScript`.
- The DBC encounter list (`ObjectMgr.h:953`, `GetDungeonEncounterList(mapId, difficulty)`) is keyed by
  creature entry, not by the `OnBeforeSetBossState` boss index — so pull naming must come from the
  engaging creature's entry, matched against `creditEntry`.

## Probes in shared files

### `src/Bot/Engine/Engine.cpp`

`Engine::DoNextAction` (`:143-262`) already emits a verdict string at each outcome via `LogAction`.
Mirror those exact seven sites with `RaidObs::NoteAction(...)`:

| Line | Verdict |
|---|---|
| `:183` | UNKNOWN |
| `:195` | multiplier veto → `veto` record, carries multiplier and action name |
| `:204` | PREREQ |
| `:220` | OK |
| `:229` | FAILED |
| `:236` | IMPOSSIBLE |
| `:243` | USELESS |

Also set a `thread_local char const* g_obsCurrentAction` around `ListenAndExecute` (`:214`), cleared
after. This is what lets a `move` record name the action that issued it — the direct answer to "two
actions steering the same `MotionMaster`", the first failure mode in
`docs/engine/raid-mechanics-lessons.md`.

Do **not** use the `ActionExecutionListener` seam (`Engine.h:33-42`, `AddActionExecutionListener` at
`:85`, zero callers today). It only sees actions that reach execution, so it misses every USELESS,
IMPOSSIBLE and vetoed verdict — the interesting failures.

### `src/Ai/Base/Actions/MovementActions.cpp`

**One probe**, in `MovementAction::MoveTo(uint32 mapId, float x, float y, float z, …)` at `:169`.
Verified funnel: `MoveTo(WorldObject*, …)` `:763`, `MoveNear` `:82`/`:88`, `MoveAway` `:1580` and
`MoveInside` `:1687` all route into it. One insertion covers every bot movement in the module.

### `ObsLatch<T>`

A thin wrapper for encounter-state assignment fields. Assigning a changed value emits a `note`
automatically; reads are transparent. Apply to the existing assignment state so instrumentation
follows the data:

- `ThorimEncounterState` (`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.h:33-65`): `meleeSlots`, `squads`,
  `runicSmashSide`, `ringArrived`, `barrierBailing`
- `VezaxEncounterState` (`UldEncounter_Vezax.h:47`), `AlgalonEncounterState` (`UldEncounter_Algalon.h:26`),
  `IronAssemblyEncounterState` (`UldEncounter_IronAssembly.cpp:33`): their slot and duty maps

A new boss that stores assignments in an `ObsLatch` is instrumented for free.

### `NoteHazard`

For hazards with no world object — Barrage cone, Runic Smash wave, Lightning Charge cone. The helpers
already derive these geometries (`GetMimironBarrageWindow`, `runicSmashSide`, `ThorimChargedThunderOrb`),
so the call is one line beside an existing derivation. It doubles as a check that the helper's idea of
the hazard matches where bots actually died.

## Config

New block in `conf/playerbots.conf.dist` near the DEBUG SWITCHES at `:2799`; loaded in
`PlayerbotAIConfig::Initialize()` (patterns: `.cpp:407` bool, `:183` int, `:466` string). Env
overrides follow automatically as `AC_AI_PLAYERBOT_OBS_*`.

```
AiPlayerbot.Obs.Enabled            = 1
AiPlayerbot.Obs.Dir                = "botobs"
AiPlayerbot.Obs.Maps               = ""     # empty = every map with an instance strategy
AiPlayerbot.Obs.SnapshotIntervalMs = 250
AiPlayerbot.Obs.PreRollSeconds     = 30
AiPlayerbot.Obs.LogHeals           = 1
AiPlayerbot.Obs.LogAuras           = 1
AiPlayerbot.Obs.MinDamageToLog     = 0
AiPlayerbot.Obs.DeathRewindMs      = 15000
AiPlayerbot.Obs.DeathVerdictMs     = 10000
AiPlayerbot.Obs.IdleCloseSeconds   = 30
AiPlayerbot.Obs.RetentionDays      = 7
AiPlayerbot.Obs.MaxDirMB           = 5120
AiPlayerbot.Obs.MaxFileMB          = 256
```

Per `CLAUDE.md`, read effective values with `docker exec ac-worldserver env | grep ^AC_` — never trust
the `.conf`.

Retention runs once at startup: delete pull files older than `RetentionDays`, then trim oldest-first
above `MaxDirMB`.

## Command

Extend the existing `debug` subtable at `src/Script/PlayerbotCommandScript.cpp:24-26`:

- `.playerbots debug obs` — open sessions, file paths, line counts, bytes written.

No `mark`, no per-death whisper.

## Files

**Create**

```
src/Bot/Obs/RaidObs.h          — facade: s_active gate, Note*() emitters, ObsLatch<T>, session lookup
src/Bot/Obs/RaidObs.cpp        — registry, NDJSON writer, sampler, pre-roll, rewind buffers, retention
src/Bot/Obs/RaidObsScripts.cpp — hook classes + AddSC_playerbots_raid_obs()
tools/botobs/postmortem.py     — analyzer (host Python 3.14.4)
docs/systems/observability.md  — schema + how to add a probe
```

No CMake edit: the module has no `CMakeLists.txt`; AC auto-globs `.cpp` under `src/`.

**Modify**

| File | Change |
|---|---|
| `src/Script/Playerbots.cpp:522-543` | `AddSC_playerbots_raid_obs();` |
| `src/PlayerbotAIConfig.h` / `.cpp` | config members + `Initialize()` loads |
| `conf/playerbots.conf.dist` | `AiPlayerbot.Obs.*` block |
| `src/Bot/Engine/Engine.cpp` | 7 verdict probes + `g_obsCurrentAction` marker |
| `src/Ai/Base/Actions/MovementActions.cpp:169` | 1 move probe |
| `src/Script/PlayerbotCommandScript.cpp:24-26` | `.playerbots debug obs` |
| `src/Ai/Raid/Uld/Util/UldEncounter_*.h/.cpp` | `ObsLatch` on assignment fields, `NoteHazard` calls |

## Analyzer

`tools/botobs/postmortem.py`, plain text sized for an agent to read.

```
postmortem.py <file>                 session summary + one block per death
postmortem.py <file> --death N       full rewind for one death
postmortem.py <file> --bot <name>    that bot's timeline: acts, vetoes, moves, damage
postmortem.py <file> --track <name>  position track + distance to boss and hazards over time
postmortem.py <file> --notes         pull/phase/note/end lines only
```

It interval-joins the `aura` and `haz`/`snap` hazard streams onto position tracks, so "was the bot
standing in it" is answered without the caller cross-referencing by hand.

## Verification

**The module cannot be compiled in this environment.** Static verification first, then the user builds
and runs a real pull.

Static, before handoff:

- every hook signature re-checked against `src/server/game/Scripting/ScriptDefines/*.h`;
- every `enabledHooks` enum id checked against the enum it comes from;
- each probe insertion point re-read in surrounding context;
- `postmortem.py` exercised against a hand-written fixture NDJSON covering every record type.

Then, in order:

1. `docker exec ac-worldserver env | grep ^AC_AI_PLAYERBOT_OBS` — flags took effect.
2. Pull any Ulduar boss. `ls -la env/dist/logs/botobs/` — a file appears at pull, grows, stops at
   kill/wipe.
3. `head -1` — `hdr` lists the full roster, schema version and the chronicle filename.
4. First `snap` timestamp is ≈30 s before the `pull` record (pre-roll worked).
5. `grep '"e":"death"' <file> | head -3` — every dead bot has a postmortem with a non-empty rewind.
6. `grep -c '"e":"veto"' <file>` — non-zero on any pull with suppression multipliers active.
7. `python tools/botobs/postmortem.py <file>` — readable report.
8. File size against pull length; worldserver CPU compared at `Obs.Enabled` 0 vs 1.

Then hand over a path and a wipe to diagnose. That is the real acceptance test.

## Implementation notes

Where the built thing differs from the plan above, and why.

- **`abs` record added.** `OnSchoolAbsorbApplied` names *which* shield ate a hit, so "the bubble was up
  but too small" reads differently from "no bubble". Not in the original record list.
- **`SetCurrentAction` became `SetCurrentContext(bot, action)`.** A scalar latch (`ObsValue`) holds no
  guid, so it could not resolve its own session — it would have silently written nothing. Encounter
  helpers only run inside a bot's action execution, so the engine publishes the ticking bot too.
- **`MoveTo` is wrapped, not probed in place.** The real mover has 27 return paths; it was renamed
  `MoveToImpl` and a thin `MoveTo` wraps it. One probe still covers every movement.
- **`NoteHazard` carries a shape.** Ulduar's invisible hazards are not circles — Barrage is a rotating
  sweep, Runic Smash a lane — so the general form takes `shape` plus the shape's own fields, with
  `NoteHazardCircle` as the convenience case.
- **`DescribeAssignment` is one template, not per-width overloads.** `uint8` is `unsigned char` and
  converts to `uint32`, `uint64` and `bool` at identical rank, so overloads made every
  `ObsGuidMap<uint8>` call ambiguous.
- **The traced containers grew a wider surface** than Ulduar needed — `erase` returning a count,
  `try_emplace`, iterator-erase, non-const iteration — because the other raids use all of it.
- **Assignment latches reach past Ulduar.** Also converted: Black Temple, Hyjal, SSC, Tempest Keep
  (16 fields). Left bare: ICC's `IccInstanceState` (`std::map`, needs an ordered variant), SWP's
  instance-keyed nested maps (inner map must be default-constructible), Naxx's function-local statics,
  and every timestamp/threshold/cache.

Nothing here has been compiled. See Verification.

## Known constraints

- **Module CI will not build this.** Accepted per decision 2. Not a bug to fix.
- **Upstream merge conflicts** in `Engine.cpp` and `MovementActions.cpp`. Accepted per decision 6; the
  probes sit beside existing `LogAction` calls so re-application is mechanical.
- **Thread safety** holds only because every probe fires on the owning instance's map thread. The
  damage, aura, death hooks and `OnMapUpdate` all do. A probe added later from the world thread
  (`WorldScript::OnUpdate`) breaks it. `docs/systems/observability.md` must say so.
- **Worldserver restart mid-pull** leaves a truncated file. NDJSON parses up to the truncation; no
  recovery logic.
- Write `docs/systems/observability.md` through `/compact-docs-writer`.
