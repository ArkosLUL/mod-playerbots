# Iron Assembly performance: bottlenecks and behaviour-neutral fixes

## Context

The strategy plays correctly as of `431b9ee11`. This is a CPU pass over it: a ranked list of
bottlenecks and, for each, whether it can be fixed **without changing what any bot decides**.

Same exercise the sibling sessions ran on Razorscale, XT-002, Ignis and Auriaya
(`docs/plans/{razorscale-performance,xt002-perf,ignis-perf-audit,auriaya-performance}/`). Same root
cause, same vocabulary, and where they already settled a question this follows them rather than
inventing a second answer.

No profile exists (`AC_AI_PLAYERBOT_PERF_MON_ENABLED=0`, and traces carry no timing), so the counts
below are read from the code. The one runtime input is from the three long 2026-09-09 pulls
(`env/dist/logs/botobs/603_1_runemaster-molgeim_17889{78332,78840,79301}.ndjson`): chain passes per
tick, from unfiltered verdicts in the death windows — **~10 ranged, ~8 melee and healers, 2-4 tanks
(range 1.6-13.8)**, at a median ~4.5 AI ticks a second.

Effective config: SightDistance 100, ReactDelay 100, MapUpdate.Interval 100, MapUpdate.Threads 6,
Iron Assembly hard mode **on** (env override; the conf default is 0), Obs on.

## Cost model

- **S** — one `GetFirstAliveUnitByEntry` (`src/Util/EncounterHelpers.cpp:294`): `"possible targets no
  los"` rebuilt from scratch, because `CalculatedValue::Get` recalculates whenever `checkInterval < 2`
  (`src/Bot/Engine/Value/Value.h:71-85`) and `NearestUnitsValue` takes the default interval 1
  (`src/Ai/Base/ValueContext.h:445`, `NearestUnitsValue.h:20`). Each one is a `Cell::VisitObjects` at
  100 yd over every unit in range (in the hall: 25 players, pets, totems, 3 bosses),
  `AnyUnfriendlyUnitInObjectRangeCheck` on each, `IsPossibleTarget` on each hostile, a vector copy per
  read, and two heap-allocated string copies for the value name. No LOS ray — this list is the no-LOS
  one, so an S is cheaper than the **N** the XT and Auriaya docs cost.
- **D** — one dynobject grid visit at 40 yd (`GetDynamicObjectPositions`, `EncounterHelpers.cpp:354`).
- **Chain pass** — for every popped action whose `isUseful()` passes, `Engine::DoNextAction` runs every
  multiplier (`src/Bot/Engine/Engine.cpp:216-232`).
- **Gate** — all 12 Iron Assembly triggers are wrapped in `UldGatedTrigger`
  (`src/Ai/Raid/Uld/UldTriggerContext.h:196-205`), closed while another encounter is IN_PROGRESS or
  this one is DONE. So the counts below apply **during the Iron Assembly pull and between pulls**, not
  during other bosses' fights. Multipliers are registered bare and are **not** gated, but all five of
  Iron Assembly's reach `IronAssemblyFormationActive`'s room test (two float compares,
  `UldEncounter_IronAssembly.cpp:328-333`) or a `dynamic_cast` first, which is why the Auriaya
  cross-cutting list does not carry any of them.
- **World state is frozen for one bot's tick**: `Map::Update` runs every player before
  `UpdateNonPlayerObjects`, on one thread, and the engine loop breaks on the first `Execute` that
  returns true.

Where the S count goes, in the hall, hard mode:

| helper | S per call |
|---|---|
| `GetIronAssemblyMember` | 1 |
| `GatherIronAssemblyTargets`, `IronAssemblyEncounterActive`, `IronAssemblyFormationActive` (in the room), `IronAssemblyBrundirIsLast`, `IronAssemblyRuneOfPowerCarrier`, `IronAssemblyEncounterStateIsStale`, `IronAssemblyFocusTarget`, and `IsSteelbreakerEmpowered(botAI)` / `GetIronAssemblyNextKillTarget` in `UldHardMode.cpp` | 3 |
| `IronAssemblyAssignedBoss` (tanks) | 6, 9 with two tanks and three members |
| `TryGetIronAssemblyRaidSpot` (ranged, healers) | 7, 8 once Steelbreaker is alone |

| role | triggers | actions | multipliers (~6 per pass) | total per tick |
|---|---|---|---|---|
| ranged | ~34 | ~11 | ~60 at 10 passes | **~105 S** |
| healer | ~34 | ~11 | ~50 at 8 passes | **~95 S** |
| melee | ~20 | ~3 | ~50 at 8 passes | **~70 S** |
| tank | 25-31 | 15-21 | 10-25 at 2-4 passes | **50-140 S** |

Hunters and rogues add ~6 (redirect threat). Raid-wide, roughly 10,000 S a second.

## Verdict legend

Borrowed from the sibling docs so the bar is the same one they were held to.

- **Exact** — same decision in every case by construction: pure predicates reordered, or a value
  resolved once and passed down inside one call chain.
- **Tick-exact** — a result read more than once in one bot's tick is computed once and shared. Differs
  from a fresh read only if an action earlier in the same tick changed the answer **and still returned
  false**. Same guarantee as the `cachedAtMs` multipliers already in the tree
  (`UlduarBurstWindowMultiplier`, `RazorscaleMultiplier`) and as `RazorscaleScan`, which the Razorscale
  session has already built on this pattern.
- **Not neutral** — do not do it.

## Bottlenecks, ranked

| # | where | now | fix | verdict |
|---|---|---|---|---|
| B1 | Every council lookup is a fresh S, and the three-member gather does three of them: `GatherIronAssemblyTargets` (`UldEncounter_IronAssembly.cpp:303`), `IsSteelbreakerEmpowered` / `GetIronAssemblyNextKillTarget` (`UldHardMode.cpp:26,36`) | 50-140 S per bot-tick | one pass over the list for all three entries; resolve once per call chain and pass the units down | **Exact** |
| B2 | The same members are re-resolved by every node in a tick: raid spot in trigger and action, focus in trigger and action (twice more for hunters and rogues), tank spot after the assignment | what is left of B1 | per-bot scan keyed on `getMSTime()`, guids not pointers, in the `RaidUlduarValueContext` the Razorscale session added | **Tick-exact** |
| B3 | `IronAssemblyMovementGuardMultiplier` (`UldMultipliers_IronAssembly.cpp:54`) calls `IronAssemblyFormationActive` before its `MovementAction` cast; `IronAssemblyHoldDpsCooldownsMultiplier` (`:119`) before its burst test | 6 S per pass in the room | type test and burst test first | **Exact** |
| B4 | The interrupt election walks the whole group per bot while Brundir casts, each lower-guid member costing `IronAssemblyReadyInterrupt` (10 name-based `CanCastSpell`, each a string build plus a `Spell` + `CheckCast` for a known spell) and `IronAssemblyMemberMustMove` (1 S + 2 D) (`UldEncounter_IronAssembly.cpp:1016-1033`) | up to ~12 members × 10 checks, per bot, per tick | stop counting at rank 1 during Whirl, rank 2 during Chain Lightning — beyond that every answer is `"standby"` | **Exact** |
| B5 | `GatherIronAssemblyRunesOfDeath` (`:849`) visits the grid twice, once per difficulty id, and runs 3-6 times a tick (trigger, both raid-spot calls, soak spot, `MemberMustMove` per movement pass, the escape actions) | 6-12 D per bot-tick | one visit matching either id; every consumer is order-independent | **Exact** |
| B6 | Tank assignment derives the boss twice — trigger (`UldTriggers_IronAssembly.cpp:84`, then `:92`) and action (`UldActions_IronAssembly.cpp:164`, then `:192`) — each derivation being a group walk, a sort, S, and a trace string | 6-9 S per tank-tick | `TryGetIronAssemblyBossTankSpot(bot, boss, spot)` with the boss already in hand | **Exact** |
| B7 | Eight triggers have no room test of their own (Tendrils, Overload, Rune of Death, Interrupt, Shield of Runes, Fusion Punch, Redirect Threat, Rune of Power soak), so **between pulls**, with the gate open, every bot in the instance pays for them while the raid walks to the next boss | ~8-11 S per bot-tick | B1 takes it to ~3, B2 to 1. A room gate would take it to 0 but is **not** exact: a bot outside the 78 yd bubble can still have a member inside its 100 yd sight | **Exact** as far as B1/B2 go |

## Expected effect

Per ranged bot-tick in the fight: **~105 S → ~17** with the Exact fixes alone (B1, B3-B6), and
**→ ~2** with B2 as well. Melee ~70 → ~12 → ~2, tanks 50-140 → ~10-20 → ~2. Between pulls every bot
in the instance drops from ~8-11 S to ~3, or to 1 with B2.

## Implementation

Exact tier:

- `UldEncounter_IronAssembly.cpp`: `GatherIronAssemblyTargets` walks `"possible targets no los"` once
  and takes the first alive unit per council entry — the same test `GetFirstAliveUnitByEntry` applies.
  `GetIronAssemblyMember` keeps its signature. `DeriveIronAssemblyAssignedBoss` and
  `DeriveIronAssemblyRaidSpot` feed the gathered units to the existing
  `IsSteelbreakerEmpowered(botAI, sb, mg, br)` overload instead of calling the `botAI` form.
  `DeriveIronAssemblyInterruptDuty` gets the B4 early exit. `GatherIronAssemblyRunesOfDeath` gets B5.
- `UldHardMode.{h,cpp}`: overloads of `IsSteelbreakerEmpowered` and `GetIronAssemblyNextKillTarget`
  that take the three units, the shape the XT-002 session is already using there for
  `IsXT002HeartbreakActive(Player*, Unit*)`.
- `EncounterHelpers.{h,cpp}`: a `GetDynamicObjectPositions` overload taking several spell ids
  (additive, next to the existing one).
- `UldMultipliers_IronAssembly.cpp`: B3 reorders.
- `UldTriggers_IronAssembly.cpp`, `UldActions_IronAssembly.cpp`: B6, and pass the gathered units into
  the helpers each node already calls twice.

Tick-exact tier (B2) — in scope, the user picked this bar over Exact-only:

- An `IronAssemblyScan` beside `RazorscaleScan` (`UldEncounter_Razorscale.h:137-167`): the three
  council guids and the bot's own Rune of Death list, each stamped with `getMSTime()` through the same
  `FreshThisTick` test, guids re-resolved and re-checked alive on every read.
- Held in a per-bot value registered in `src/Ai/Raid/Uld/UldValueContext.h`, exactly as
  `"razorscale scan"` is. Per-bot values are only touched on that bot's map thread, so no lock.

## Negligible, checked

Role checks (`IsTank`/`IsRanged`/`IsDps` are one `GET_PLAYERBOT_AI` hash lookup plus a strategy-type
bitmask since 2024-08-05), group walks of 25, `HasAura` tests, the per-instance state mutex (~20
uncontended locks per bot-tick), `RaidObs::NoteDerived` string building (tracing only, change-only),
and the Iron Assembly multipliers outside the room (room test or `dynamic_cast` first).

## Not neutral, rejected

- **A shared instance-wide member cache.** The sweep is relative to each bot's position and
  `IsPossibleTarget` has per-bot terms, so one answer for the raid is not the same answer.
- **Resolving members through the instance script's stored guids.** O(1), but it drops the 100 yd
  condition, so the strategy would switch on for bots that cannot see the council today.
- **A room gate on the eight ungated triggers** (B7) — see the reason in the table.
- **Throttling triggers** (`checkInterval > 1`) — changes reaction latency.
- **Pruning the escape-spot search** (`FindNearestPositionClearOfHazards`, `EncounterHelpers.cpp:379`,
  up to 16 collision raycasts per call while dodging): the collision check moves the candidate, so its
  score is unknown until it has run.

## Adjacent

- **Behaviour bug, not performance:** `MimironAvoidAoeGuardMultiplier`
  (`UldMultipliers_Mimiron.cpp:110`) vetoes `avoid aoe` in every Ulduar fight whenever Mimiron hard
  mode is configured on — P5 shows 19 such vetoes on Holylight inside Iron Assembly. Report it, do not
  fix it here.
- **Doc correction:** `docs/engine/raid-mechanics-lessons.md`, "What a strategy costs per raid", still
  says `IsTank()` → `ContainsStrategy` "scans that member's strategy list". It has been a bitmask test
  (`Engine::HasStrategyType`, `Engine.h:88`) since 2024-08-05; the cost is the `GET_PLAYERBOT_AI`
  lookup. Worth adding there too: `"possible targets no los"` recalculates on every `Get`. That doc is
  reached from `CLAUDE.md` via `pitfalls.md`, so `/compact-docs-writer` up front, diff first, apply on
  approval.

## Coordination

Other sessions hold uncommitted work in `UldValueContext.h` (new), `UldHardMode.{h,cpp}`,
`BuildSharedValueContexts.cpp`, `AiFactory.{h,cpp}`, `Playerbots.cpp`, `UldEncounterGate.cpp` and the
Razorscale, XT-002, Ignis and Auriaya files. The Iron Assembly files are clean. Re-read
`UldValueContext.h` and `UldHardMode.h` immediately before editing them, touch nothing else outside
the list above, and stage only Iron Assembly files.

## Verification

- Static: `python apps/codestyle/codestyle-cpp.py` from the core root. The module cannot be compiled
  here (no `compile_commands.json`) — hand the build off, never claim one.
- Behaviour: one hard-mode pull, then `tools/botobs/postmortem.py <trace> --notes ironassembly.` —
  one `alive` row per transition, the same focus / tank / spot labels, one `whirl` or `chain` holder
  per cast, `iron assembly hold dps cooldowns` vetoes on burst names before phase 3, and Steelbreaker
  still reaching ~3% or better. Anything that moves means a fix was not identical: revert it first.
- Cost, before and after the rebuild, same roster: `.playerbots pmon toggle`, `.playerbots pmon reset`
  at the pull, `.playerbots pmon` after (output lands in `env/dist/logs/Playerbots.log`), then toggle
  off. Compare the `iron assembly *` trigger and action rows and
  `Total : PlayerbotAI::UpdateAIInternal I`. Multipliers have no PerfMon row, so B3 shows only in that
  total, and with random bots feeding the same row only the ratio means anything.

## Step 0

Copy this document to
`docs/plans/iron-assembly-performance/iron-assembly-performance.INVESTIGATION.md` before starting.
