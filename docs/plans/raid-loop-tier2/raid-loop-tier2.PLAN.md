# Cutting iterations out of the raid-strategy loop — Tier 2

## Context

Tier 1 shipped (commit `2816b7171`): traces now carry `hdr.bin` and `hdr.cfg` so `postmortem.py` opens
every report by saying whether the pull counts, and `tools/pblint/pblint.py` sweeps the string-wired
names statically. That closed two of the three costs the Tier 1 investigation identified —
**evidence disqualification** and the **static** half of **silent failure**.

Two costs remain, and this investigation measured both:

1. **Silent failure at runtime is still total.** pblint proves a name resolves to a registered
   creator. Nothing proves a node ever *did* anything. A trace cannot distinguish these, and four of
   the five are byte-identical silence:

   | State | Recorded today |
   |---|---|
   | Name resolved to nothing (`Engine.cpp:518`) | silence |
   | Never evaluated — `checkInterval` throttle or `minimal` mode (`:524`, `:526`) | silence |
   | Evaluated, trigger false every pass (`:535`) | silence |
   | Fired, handlers pushed, action never won a tick | silence |
   | Fired and ran | `act` with `vd:"OK"` |

   The fourth is reportedly the most common way a raid node fails to act, and it looks exactly like a
   node that was never wired in. Each instance costs a full build+pull cycle to find.

2. **Sample size.** 125 traces and 1.3 GB sit in `env/dist/logs/botobs/`, and there is **no
   cross-trace facility of any kind**: `Trace` takes one path, both CLIs take one positional, and
   `docs/systems/observability.md:38` explicitly leaves batch auditing to an ad-hoc shell loop. This
   is why twelve Lightning Charge shelters were solved against three pulls and two later pulls pushed
   four rows out of their own cone margin. Retention is 7 days / 5 GB, so baselines expire.

A third finding reframes an item the Tier 1 plan had queued. **`flame_leviathan.py` did not go stale
in its numbers** — every one of its ~20 constants still matches
`UldEncounter_FlameLeviathan.h`. What rotted is the *model*: a `tar-lead` branch that is dead code on
every modern trace, two of the module's five `fl.station` values known to the scorer, six published
probes (`fl.pursued`, `fl.corner`, `fl.frozen`, `fl.lifetower`, `fl.interrupter`, `fl.pyrite`) that
nothing reads, and a `victim()` heuristic guessing what `fl.pursued` now states outright. The queued
"share constants with the module" idea would have prevented none of it. The real defect is
**published-but-unread probe keys**, and that is a check, not an architecture — it folds into 2.1.

Tier 2 is one build. **2.1 is pure Python and lands first, before any rebuild.** 2.2, 2.3 and 2.4 all
need a compile, so they ship together and one pull proves all three.

---

## 2.1 Batch analysis across the trace corpus (no build)

**Why.** Decisions are taken on 1–5 pulls because reading a sixth costs another full run of the tool.
125 traces are already on disk. Making the corpus queryable converts sample size from a cost into a
free parameter.

**New `tools/botobs/batch.py`.** Selection plus roll-ups; deliberately not a generic metric engine.

- **Selection**: `--boss SLUG`, `--since REF` (reuses `validity.resolve_since`), `--valid` (only
  traces with zero disqualifiers), and one or more roots so a pinned baseline directory outside
  `botobs/` survives retention. Default root from the same config path `postmortem.py` uses.
- **Streaming, one trace at a time.** Individual files reach 21 MB; the corpus is 1.3 GB. `batch.py`
  loads a `Trace`, reduces it to a small per-trace row, and drops it. Never hold two.
- **Output is one row per trace plus aggregate lines**: outcome, duration, roster size, human count,
  disqualifier count, `--verify` pass count, death count. This is the shell loop
  `observability.md:38` describes, made a first-class thing.
- **Validity census** — the direct extension of Tier 1: `18 thorim traces, 11 valid, 5 stale binary,
  2 human in raid`. Turns "does this pull count" into "how many pulls of this boss count".
- **Unread probe keys** — collect every `note.k` across the selection, diff against the keys the
  scorers actually consume. This is the check that would have caught all six unread Flame Leviathan
  probes. The consumed set is a literal list in each scorer; have them export it rather than
  re-deriving it by grep.

**Shared loader.** Add `load_many(roots, **filters)` to `obstrace.py` as a generator, so a future
per-boss scorer gets corpus access without another bespoke walker.

**Do not** add a `--batch` flag to `postmortem.py`. Its dispatch is a first-match `if` ladder over a
single positional; batch selection does not fit it.

---

## 2.2 Node coverage — the runtime twin of pblint (schema v12)

**Why.** See cost 1. pblint proves the wiring exists; coverage proves the wiring carried current.
Neither is evidence for the other's finding, and there is no overlap in what they catch: pblint's
`orphan-creator` has no runtime counterpart (a creator nothing names never enters `triggers`), and a
perfectly wired node whose condition is never true is invisible to pblint by construction.

**Design constraint that decides everything: streaming is not an option.** ~200–250 nodes per bot per
pass, ~5 passes/sec, 25 bots ⇒ ~25,000 records/sec ≈ **450 MB for a 5-minute pull against a 15 MB
baseline**, blowing `maxFileBytes` about three minutes in. So: **accumulate in the engine, flush once
at pull end.** This is a third grain below the existing per-pass and per-verdict ones, and the
recorder has no such mechanism today.

### Counters

**`TriggerNode*` cannot be a key.** `Engine::Reset()` (`Engine.cpp:121-124`) deletes every node and
`Init()` calls `Reset()` first (`:140`); `PlayerbotAI::ChangeEngine` re-inits on every
combat/non-combat/dead transition. Recycled addresses would silently merge counters across
differently-named nodes. String keys put a `getName()` heap build in the hottest loop the engine has.

Instead: **a `std::vector<RaidObs::NodeCoverage>` on the `Engine`, parallel to `triggers` and indexed
by the same `i`**, allocated lazily on the first covered pass so idle open-world bots carry an empty
vector. Names are resolved once, at drain, before the vector is torn down.

```cpp
struct NodeCoverage
{
    uint32 checks = 0;     // needCheck said yes and Trigger::Check() ran
    uint32 fires = 0;      // Check() returned a truthy Event
    uint32 pushes = 0;     // that fire turned into at least one queue entry
    uint32 shared = 0;     // another node's Trigger* already fired this pass; loop 2 still pushes
    uint32 throttled = 0;  // checkInterval had not elapsed
    uint32 minimal = 0;    // minimal mode dropped it for sitting under relevance 100
    uint32 dead = 0;       // the name resolved to no creator this bot's context stack carries
};
```

Increment sites, all in `Engine::ProcessTriggers` (`Engine.cpp:501-562`), converting the iterator
loops to index loops: `dead` at `:518`, `shared` at `:521`, `minimal` at `:526`, `checks` before
`:529`, `fires` after `:535`, `throttled` as a new `else` on `:524`, `pushes` at `:554`.

Hoist the gate once per pass — `bool const obs = RaidObs::Active() && RaidObs::CoversBot(bot);` — so
`Active()` stays exactly one acquire-load and each site is a not-taken branch when inactive.

Note `checks + throttled + minimal + shared + dead` is the pass count. Do **not** record it; a
redundant column is a `--verify` liability the moment the two disagree.

### State 4 — fired but never ran

One extra counter, `won`, keyed by **trigger name**, incremented only at the `OK` verdict site
(`Engine.cpp:263`) from `event.GetSource()`. The loop `break`s on OK, so this is at most one call per
pass per bot. The reader then reads state 4 off one row: `pushes > 0 && won == 0`.

**Do not add `act.src`.** `Queue::Push` dedups by action *name* and `updateExistingBasket`
(`Queue.cpp:64-77`) keeps the **existing** basket's `Event`, deleting the newcomer's. So in the exact
case `src` would exist to resolve — two triggers pushing one action in a pass — it names the first
pusher confidently and is silently wrong about the second. It also collides with the `ShouldEmit`
latch: leave `src` out of the key and the emitted value is arbitrary; put it in and the `act` stream
multiplies, which `observability.md:170-178` says was 40% of a v4 file before that latch existed.

Document that `won <= pushes` is **not** an invariant — an action can win in a pass where nothing was
pushed, having survived in the queue. The ratio is the signal, not the difference.

### Strategy attribution

Capture at `Engine::Init` (`Engine.cpp:143-154`), the only place it exists: `InitTriggers` only
appends, so `triggers.size()` before and after each call is that strategy's index range. Store
`vector<pair<size_t, string>> strategySpans` — copy the name, not the `Strategy*`, which dangles by
the next drain. Binary-search at drain time.

Honest limitation: all 172 Ulduar nodes come from one strategy named `"ulduar"`, so this separates
Ulduar from `"dps"`/`"heal"`/`"melee"` but not Hodir from Thorim. The next section handles that.

### Ulduar's encounter gate

`UldGatedTrigger::Check` (`UldEncounterGate.cpp:199-204`) returns an empty `Event()` when the gate is
shut — indistinguishable from "condition false" for all 165 Ulduar triggers. **Handle it reader-side
at zero recorder cost**: every Ulduar trigger name leads with its encounter, and
`UldEncounterOfTrigger` already derives it from the prefix table at `UldEncounterGate.cpp:26-42`.
Mirror that table into `views.py` and fold the ~150 other-encounter nodes into one
`SKIPPED … (gate shut for this pull)` line. Cross-reference both sides in a comment; the duplication
will drift otherwise. Escalation, only if the gate proves to close mid-pull, is a counter on
`UldGatedTrigger` read once per node at drain — four lines, no hot-loop cost. Do not build it now.

### Flush

Two one-shot drains, no periodic timer:

1. **`Engine::Init()`, first statement, before `Reset()`** tears down the names — catches the mid-pull
   rebuild when a bot enters combat or dies. Deliberately not inside `Reset()`, which `~Engine` also
   calls.
2. **`CloseSession`** (`RaidObsLifecycle.cpp:199-211`), in the loop already flushing ticks, reaching
   back through the roster via `PlayerbotAI::ObsDrainCoverage()`. Excluded for `shutdown`/`mapgone`,
   which reach that point with the thread pool gone — comment the invariant, not the two strings.
   Accepted loss: those two outcomes write no coverage, and their traces are already degraded.

Two record types, dictionary plus rows — the move `unit` and `spell` already make:

```
{"e":"covdef","d":[[0,"hodir move to shelter","ulduar","c"], ...]}   // chunked at 200
{"e":"cov","g":12,"r":[[0,1847,412,412,289],[1,1847,203,203,0]]}     // trailing zeros trimmed
```

`cov.r` columns: `[id, chk, fire, push, won, dup, thr, min, dead]`. A fifth `covdef` element carries
the trigger's own name only when it differs from the node's — a runtime sighting of the mismatch
pblint hunts statically. Rows only for nodes touched at least once; absence is itself an answer.

**Per-bot, not raid-aggregated.** 25 rows per node is 24 redundant for the raid-level question, but
the reader can sum per-bot rows and cannot un-sum an aggregate — and the failures this framework
exists to catch are role-shaped. A raid-summed `fires: 1204` cannot separate "every bot fired it 48
times" from "one bot fired it 1204 times and 24 never did". The view defaults to summed with an
`n/25 bots` column; `--by-bot` splits it.

**Cost**: ~230 KB per pull (1.5% of a 15 MB baseline), ~45 µs/s of engine time while recording,
nothing measurable when inactive, ~3 ms for the final drain.

### Reader

`postmortem.py --coverage [PREFIX]`, with `--by-bot`. `obstrace.py` hoists `covdef` into a dict beside
`unit`/`spell`; `cov` stays in `records`. New `views.show_coverage(trace, prefix, by_bot) -> int`
matching the existing `show_*` contract. Buckets in severity order:

| bucket | test | means |
|---|---|---|
| `DEAD` | `dead > 0` | name resolved to nothing in this bot's own context stack |
| `NEVER` | `chk > 0, fire == 0` | asked every pass, never true |
| `LOST` | `push > 0, won == 0` | **state 4 — the invisible one** |
| `THIN` | fired on < 20% of bots carrying it | role-shaped; print the split from `trace.roles` |
| `THROT` | `thr > 0, chk == 0` | never got a look in |
| `ok` | `won > 0` | ran |

`DEAD` catches what pblint structurally cannot: a name registered in some context this bot does not
include. Add three `--verify` checks (every `cov` id defined in `covdef`; `fires <= checks`;
`pushes <= fires + shared`), taking it to 17.

### Sequencing within 2.2

Steps 1–3 cannot change a byte of any trace, which makes the engine-side diff safe to review ahead of
the recorder side.

1. `NodeCoverage` + probe declarations in `RaidObs.h`; sink fields in `RaidObsSession.h`. Inert.
2. `Engine.h` members, `Init()` attribution + drain, `Reset()` clears. Still inert.
3. The seven increments in `ProcessTriggers` and `won` at `:263`. Collecting, not writing.
4. `EmitCoverage`, the `CloseSession` reach-back, `SCHEMA_VERSION` 11 → 12. First trace with `cov`.
5. `obstrace.py` load branch + `SUPPORTED_SCHEMA` 12 (keep 4–11 readable), `show_coverage`,
   `postmortem.py` flag, the three `--verify` checks.
6. `docs/systems/observability.md`.

---

## 2.3 navprobe `los` — move the sight class off the pull loop

**Why.** `pitfalls.md:167` says "navprobe answers the floor, never sight", and Mimiron paid for it:
out-of-sight is an invalid target, so `drop target` puts the bot in the non-combat engine where
`follow` walks it away — **58 and 49 `follow ↔ arc spread` flip-flops in two night pulls, two bots
20–35 s without a target each**. The anchor bearings that fixed it were derived from **323 traced MK
II positions** across multiple pulls. An offline probe answers that in one command.

**It is far smaller than the docs assume.** `pitfalls.md:167-171` proposes a scratch program doing
`InitMap`, `LoadMapTile` and the call. navprobe **already does the first two** — it constructs
`VMAP::StaticMapTree`, calls `InitMap`, and `LoadMapTile`s every grid tile on the map
(`src/tools/navprobe/NavData.cpp:471-482`) — and already calls a sibling method on that same tree,
`_tree->getHeight` at `NavData.cpp:518`, with the coordinate conversion written one line above.
`VMAP::StaticMapTree::isInLineOfSight` is declared at `src/common/Collision/Maps/MapTree.h:77`, three
lines below `getHeight`, and is already linked: `src/tools/CMakeLists.txt:137-144` links every tool
`PUBLIC common`, and `src/common/CMakeLists.txt:19` folds `Collision/` in. **No new link dependency,
no new data path, no new file format.**

Work, in the **core fork** (`src/tools/navprobe/`, not the module):

- `HeightData::IsInLineOfSight(...)` mirroring `GetVmapHeight`'s three lines — convert both endpoints
  with `VMAP::VMapMgr2::convertPositionToInternalRep`, call through, guard `!_tree`.
- **`ring` gains a sight column**, which is the headline: the Mimiron question was never "can A see B"
  but "of these N candidate slots, which can see this focus". Add `--los-from X Y Z` to `ring` so
  every generated point reports pass/fail against that focus, and the trailer reads
  `N/M on mesh, K/M in sight`. This is the query that cost 323 traced positions.
- `los X1 Y1 Z1 X2 Y2 Z2` as the bare primitive, for completeness and for scripting.
- Cast the ray `WorldObject::IsWithinLOSInMap` casts (`src/server/game/Entities/Object/Object.cpp:1407-1431`):
  observer eye = position + collision height; target = `GetHitSpherePointFor`, pulled toward the
  observer by its hit sphere, not the centre. Reuse the existing `--collision` flag for the eye.
- Extend `coverage`'s existing vmap reporting to warn when the tree is missing, since LOS silently
  passes everything without it.
- **GameObject collision still needs a live server** — `DynamicMapTree` holds spawned state. navprobe
  already carries this caveat for height (`NavData.cpp:527`); carry it for sight too, in the output.

Then rewrite `pitfalls.md:167-171`: the recipe describes a scratch program that no longer needs to
exist. Also `docs/raids/ulduar/mimiron.md:77-78`, which points at that recipe.

---

## 2.4 pblint triage — 31 findings, three shapes

The tree is red by design (Tier 1 landed the lint before the fixes). The findings are **not one job**.
Current output: 31 errors, 648 warnings.

### Shape A — verified typos, 7 errors at 4 sites. Fix mechanically.

**Zul'Aman (6 errors).** The direction is unambiguous: the whole registration chain says `Tanks` —
creator key `"akil'zon tanks position boss"` (`ZAActionContext.h:26`), factory
`akilzon_tanks_position_boss` (`:120`), class `AkilzonTanksPositionBossAction`. Only
`ZAStrategy.cpp:20-21` asks for `main tank`. So **change the three strategy references to `tanks`**,
six lines in `ZAStrategy.cpp` (`:20-21`, `:46-47`, `:91-92`). Halazzi's chain says `MainTank`
throughout and is correctly wired; leave it. Nalorakk already uses `tanks` on both sides.

**M'uru (1 error).** `SWPActionContext.h:169` registers `"…shadowsword berseker"` while the factory
(`:170`) and class both spell `berserker`. Fix the registration string.

**These switch on behaviour that has never run** — three of Zul'Aman's five bosses gain main-tank
positioning and M'uru gains a Berserker stun. That is not a refactor. It is also exactly why they
ship in the same build as 2.2: the next ZA pull's `--coverage` output moves those nodes from absent
to `ok`, proving both the fix and the tool in one pull.

### Shape B — 4 errors that are not names at all. Real geometry bugs.

- `NaxxActions_Gluth.cpp:177,182,191` — `MoveInside(..., 0)` never returns false and stacks the raid
  on one point.
- `NaxxActions_Sapphiron.cpp:38` — `isInFront() || isInBack()` covers the whole circle, always true.

Each needs a real radius / a real arc, decided against the encounter. Treat as four small fixes, not
a rename pass.

### Shape C — 20 errors, registered nowhere. One call per finding.

Two sub-shapes:

- **Wrong layer** — the name exists, on the other side. `explosive shot` (×2, `SurvivalHunterStrategy.cpp:58,100`)
  referenced as an action; `rune strike` (`BloodDKStrategy.cpp:117`), `blessing of might`
  (`OffhealRetPaladinStrategy.cpp:155`) and `high threat` (`GenericMageStrategy.cpp:143`) referenced
  as triggers. Confirm which layer holds the creator, then move the reference.
- **Genuinely absent** — nothing registers the name anywhere: `reset` (×2), `stay line`,
  `grounding totem` (×2), `master loot roll`, chat `naxx` / `bwl`, `team flagcarrier near`,
  `location stuck`, `tank aoe`, `mind freeze on enemy healer`, `freezing trap on cc`, and the two
  Felmyst landing-timer nodes (`SWPStrategy.cpp:104-105`). Each is either a dead reference to delete
  or a creator to write, and that is a judgment call against what the strategy meant.

Only the Felmyst pair is raid code; the rest is base and class. **Do the raid ones (A, B, Felmyst) in
this cycle and leave the 17 base/class findings recorded** unless the cycle has room — they are real
but they are not what raid iteration count turns on.

---

## Files touched

| Purpose | Path |
|---|---|
| Batch selection + roll-ups | `tools/botobs/batch.py` (new), `tools/botobs/obstrace.py` (`load_many`) |
| Coverage counters + drain | `src/Bot/Engine/Engine.h`, `src/Bot/Engine/Engine.cpp` |
| Coverage probes + sink | `src/Bot/Obs/RaidObs.h`, `RaidObsEngine.cpp`, `RaidObsSession.h`, `RaidObsLifecycle.cpp` |
| Coverage reader | `tools/botobs/obstrace.py`, `views.py`, `postmortem.py` |
| LOS (core fork) | `src/tools/navprobe/NavData.h`, `NavData.cpp`, `main.cpp`, `README.md` |
| Name fixes | `src/Ai/Raid/ZA/ZAStrategy.cpp`, `src/Ai/Raid/SWP/SWPActionContext.h` |
| Geometry fixes | `src/Ai/Raid/Naxx/Action/NaxxActions_Gluth.cpp`, `NaxxActions_Sapphiron.cpp` |
| Docs | `docs/systems/observability.md`, `docs/engine/pitfalls.md`, `docs/raids/ulduar/mimiron.md` |

---

## Verification

Each item has a target it must **reproduce**, not merely run clean — a tool that only comes back green
proves nothing, which is the lesson from `flame_leviathan.py` scoring a gate the module had replaced.

- **2.1** — the validity census over all 125 traces must independently flag the two pulls already
  known bad: the Mimiron pull a human main-tanked, and the Thorim normal-mode kill. The unread-probe
  check must report all six Flame Leviathan keys (`fl.pursued`, `fl.corner`, `fl.frozen`,
  `fl.lifetower`, `fl.interrupter`, `fl.pyrite`). Peak RSS must stay flat across the corpus — if it
  climbs, a `Trace` is being retained.
- **2.2** — on a Ulduar pull, ~150 nodes must land in the gate-shut fold and the fought boss's nodes
  must not. `--verify` goes to 17/17. All 125 v11 traces still read unchanged. On a Zul'Aman pull
  after 2.4, `akil'zon main tank position boss` moves from absent to `ok` — that single line is the
  proof for both the lint and the coverage view. Confirm the `Init()` drain fires by checking a bot
  that died mid-pull has `d` engine rows.
- **2.3** — must reproduce Mimiron's documented figures from `docs/raids/ulduar/mimiron.md:68`:
  anchors at 30°/330° hide the worst edge slot **21% and 38%** of the time, 25°/345° **1.5% and
  1.2%**. Those came from 323 traced boss positions; an offline `ring --los-from` sweep matching them
  is the whole case for the tool. Also confirm `coverage` warns when the vmap tree is absent.
- **2.4** — `pblint.py` error count drops by exactly the number fixed, and no warning becomes an
  error. `pb-syntax-check.sh` over every changed `.cpp` and every `.cpp` including a changed `.h`.
- Whole-tree `pblint.py` and `codestyle-cpp.py` before the commit, per `CLAUDE.local.md` step 1.

---

## What follows Tier 2, not planned here

- **Per-boss scorers built on published probes**, replacing re-derivation. The Flame Leviathan lesson
  is that constants stay in sync and models do not, so a scorer should consume `fl.pursued` rather
  than infer a victim by facing ray. Fix `flame_leviathan.py` first as the worked example: the dead
  `tar-lead` branch, the three unknown station values, and `victim()`.
- **Slot solver**, taking on-mesh, sight (now available from 2.3), hazard clearance with park
  tolerance, spacing and range band at once, plus re-checking the shipped table parsed back out of
  the source — which `pitfalls.md:31` asks for.
- **Unit tests for the pure geometry.** The module has zero; the core has Google Test and mocks in
  `src/test/`.
- **Extract the two suppression idioms** — `dynamic_cast<CombatFormationMoveAction*>` across 17 raid
  files, `dynamic_cast<DpsAssistAction*>` across 18. `docs/raids/README.md:79` says "eight
  encounters" and undercounts by half.
- **Port `UldEncounterGate` to the other raids.** Ulduar keys trigger evaluation to the boss in the
  room via one 12-line decorator over the whole creator table; every other raid still evaluates every
  boss's triggers every tick. Cheapest per-raid cost win available, and already written.
- **The 17 base/class pblint findings** left from Shape C.
