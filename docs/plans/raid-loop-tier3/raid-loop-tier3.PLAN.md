# Cutting iterations out of the raid-strategy loop — Tier 3

## Context

Tier 1 (`2816b7171`) made a trace say whether the pull counts, and swept the string-wired names
statically. Tier 2 (`4fe60957e`) made the engine count what each node did, made the corpus queryable,
and moved the sight question offline (`064d52606` in the core fork).

Measuring the loop again, the bottleneck has moved. It is no longer *what the recorder captures* —
it is that almost nothing reads what is already captured, and that no tool answers the question the
loop exists to answer.

**1. The recorder publishes 96 probe keys. Python reads 2.**

`ObsValue` / `ObsGuidMap` / `ObsGuidSet` / `NoteDerived` declare 96 distinct keys across the raid
tree. 74 of them appear in the 129 traces on disk; `flame_leviathan.py` reads `fl.station` and
`fl.vent`, and nothing reads the other 72. Everything else that touches `note` is key-agnostic —
`views.show_notes` prints raw payloads filtered by prefix, `batch.py` counts keys without
interpreting one. These are the encounter's own answers about what it decided and why, already
written to disk, unread. Every improvement here is retroactive across 129 traces and costs no pulls.

Worse, one of the two that *is* declared consumed is not. `Frame.__init__` hard-returns at
`tools/botobs/flame_leviathan.py:153` (`if self.vehicles: return`) before the station-fallback loop,
and vehicles are in the snapshot on every trace on disk — so the `fl.station` path, the `tar-lead`
branch at `:229` and the per-station tables are dead code, and the "by station" tables are really by
vehicle entry. `victim()` (`:169-188`) still guesses the pursued vehicle by facing ray, while
`snap.u` column 7 names it outright and `fl.pursued` publishes the same guid 243 times.

**2. The recorder silently drops a whole class of probe.**

`NoteAssignment` (`src/Bot/Obs/RaidObsEngine.cpp:406-424`) resolves the key guid with
`ObjectAccessor::FindPlayer` and returns early when it is not a player. Every `ObsGuidMap` and
`ObsGuidSet` write goes through it. `fl.frozen` is keyed on a **vehicle** guid
(`src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp:172`), is written every pass, and has **zero
rows in all 129 traces** — while the other six `fl.*` keys have 1,556 between them. It gates three
separate decisions (`:503` the lead election, `:615` who drives to a corner, `:714` slot dedup), so a
wrong `frozen` read is undiagnosable by construction. Nothing reports the silence.

**3. A human's role is erased, which is what made `--valid` vacuous.**

`RoleOf` (`src/Bot/Obs/RaidObsSession.cpp:356-370`) returns the literal `"human"` before asking
anything. The `h` field already carries the human flag, so the two facts are conflated and the role
is destroyed. This is why `human-in-raid` had to ship as informational in Tier 2: it fires on 100% of
the corpus and the trace cannot say whether the human held a role the strategy was meant to play —
the exact fact that invalidated the Mimiron pull.

**4. Nothing compares two pulls.** No diff, no baseline, no regression mode anywhere in `tools/`.
`--since` compares a build timestamp against a git commit and never opens a second trace; `batch.py`
aggregates a selection but never pairs one. So "did my change help" is still answered by reading two
reports side by side, which is how twelve Lightning Charge shelters were solved against three pulls.

**5. Corpus selection is split-brained.** `find_traces` filters on `boss_from_path` (the filename
slug); `row_for` labels on `boss_of` (the rename record); `BOSS_ALIASES` is applied to neither. So
`xt002` (1 trace) and `xt-002-deconstructor` (2) are one boss under two labels, `--boss` cannot reach
a trace whose boss was learned after the file was named, and the census fragments. Sample size was
the whole point of `batch.py`.

Also standing: 13 of 129 traces fail invariants (11 "every guid referenced is named", 3 "every blow
has a damage row behind it"), and `tools/botobs/fixtures/coverage-v12.ndjson` has no runner — there
is no test harness over any Python in this repo.

Tier 3 is one build. **3.1–3.3 are pure Python and land first.** 3.4 needs a compile.

---

## 3.1 A generic probe reader (no build)

**Why.** 96 keys, 2 read. Writing a scorer per boss to make its probes legible does not scale and has
produced exactly one scorer in the project's life. One reader that interprets *any* key makes all 96
legible at once, retroactively.

**New `tools/botobs/probes.py`**, surfaced as `postmortem.py --probes [PREFIX]`.

The three emitters mean different things and the reader must not flatten them. Infer the shape from
the data rather than a hand-maintained table:

| shape | emitted by | looks like | render |
|---|---|---|---|
| latch | `ObsValue` | one guid, values change over time | state timeline: value → duration |
| assignment | `ObsGuidMap` / `ObsGuidSet` | many guids, each changing | per-holder timeline, plus who held what when |
| event | `RaidObs::Note` | repeats with the same value | a count and a rate, not a timeline |

Requirements:

- **Resolve guids inside payloads, not only whole-payload.** `records.note_text` (`records.py:68-80`)
  substitutes a name only when the entire `txt` is a known guid, so `slot 3 orb 4823...` stays raw.
  Reuse `trace.name()` on every guid-shaped token.
- **Churn is the headline metric.** For each key: distinct holders, changes per minute, mean hold
  duration, and the A-B-A count — a value that flips back to what it just was. This is the generic
  form of the defect that took Mimiron two night pulls to find by hand
  (`docs/raids/ulduar/mimiron.md:65,78`: 58 and 49 `follow ↔ arc spread` flips; `:642`: 100
  `arc spread ↔ reach spell` in a phase). A generic churn ranking finds the next one without knowing
  to look for it.
- **Mute-probe check.** Collect declared keys by parsing the `ObsValue<…> x{"key"}` /
  `ObsGuidMap<…>`/`ObsGuidSet` declarations and the `NoteDerived(..., "key")` / `Note(..., "key")`
  call sites out of `src/`, diff against what the selection emitted, and report keys that are
  declared for an encounter the selection actually pulled and never appeared. `fl.frozen` is the
  known casualty and the regression target. Scope it to pulled encounters — Algalon and Vezax keys
  are absent because nobody fought them, which is not a defect.

**Retire `CONSUMED_KEYS`.** `batch.py:35-37` hand-declares what scorers read, keyed by filename, and
is already wrong about `fl.station`. Once the generic reader covers every key, "unread" is no longer
the interesting property; "declared and never emitted" is. Replace the `--probes` roll-up with the
mute check plus a churn ranking.

## 3.2 Metrics, and comparing two selections (no build)

**Why.** See context 4. This is the question the loop exists to answer and no tool answers it.

**Extract values out of the printers.** `verify_checks` and `validity.inspect` were already split out
of their `show_*` wrappers in Tier 2; apply the same shape to the rest. `show_stalls`
(`views.py:123`) and `show_clump` (`views.py:178`) compute inline and print; `coverage.show_coverage`
does the same. Each gains a `*_metrics(trace) -> dict[str, float]` beside the renderer, and the
renderer consumes it.

**One metric set per trace**, assembled from what already exists plus the new readers:

| source | metrics |
|---|---|
| `batch.row_for` | duration, roster, deaths, humans, outcome, invariant failures |
| `views` (extracted) | stall seconds, stall windows, clump events |
| `coverage` (extracted) | node counts per bucket — `DEAD`/`NEVER`/`LOST`/`THIN`/`THROT`/`ok` |
| `probes` (3.1) | per-key churn rate and A-B-A count, distinct holders |

**`batch.py --split-at REF`** is the headline form. It splits the current selection by build time
around a git ref and compares the two halves — no new pulls, no pinned baseline, using the 129 traces
already on disk. `validity.resolve_since` (`validity.py:66-89`) already resolves a ref to a commit
time and `hdr.bin` already carries the build time, so the machinery exists. Also `--baseline ROOT`
for an explicitly pinned directory, since retention is 7 days.

**Be honest about noise.** Pulls differ in RNG, roster and duration. Report `n` on each side, median
and range per metric, and mark a metric as moved only when the two sides' ranges do not overlap.
Never a p-value on n=3. Normalise per-pull rates by duration where the metric is a count.

## 3.3 One boss key, and a test harness (no build)

**Boss identity.** Collapse the split-brain into one `boss_key(path_or_trace)` used by both
`find_traces` and `row_for`, applying `BOSS_ALIASES` in both. `--boss` must reach a trace whose file
name predates the rename, which means selection can no longer be filename-only for the general case
— keep the filename fast path, and fall back to opening the header (not the whole trace) when the
filename slug is the map name or an alias.

**pytest.** `tools/botobs/fixtures/coverage-v12.ndjson` exists and nothing runs it. Add
`tools/botobs/tests/` with pytest over the pure functions this cycle creates or touches: shape
inference, churn counting, guid substitution, `boss_key` aliasing, the metric extractors against the
fixture, and the corrupted-fixture cases proving the invariants bite. This is free — no C++, no
build. Do **not** add a C++ test target: the core's socket exists (`src/test/CMakeLists.txt:34-52`
reads `ACORE_MODULE_TEST_SOURCES`, set by nothing) but `BUILD_TESTING=ON` forces gcov flags on the
whole build and `unit_tests` links `game` plus `modules`, so a module unit test links the entire
server. That is a slower loop than the one it would shorten.

## 3.4 The two recorder holes (one build)

**`NoteAssignment` must not drop a non-player key.** Fall back to `t_currentBot` for the session the
way `NoteDerived` already does (`RaidObsEngine.cpp:426-449`), keeping the subject guid in `g`. The
session is what the lookup was for; the key guid is the subject and need not be a player. Unmutes
`fl.frozen` and every future container keyed on a vehicle, creature or object.

**`RoleOf` must give a human their real role.** Drop the `return "human"` at
`RaidObsSession.cpp:360` and call the **static** forms — `PlayerbotAI::IsTank(player)`,
`IsHeal`, `IsRanged` (`src/Bot/PlayerbotAI.h:461-466`). They already handle this: each opens with
`if (!bySpec && botAi)`, so a human with no `PlayerbotAI` falls through to the
`AiFactory::GetPlayerSpecTab` talent-tab path and answers correctly, bear form included
(`PlayerbotAI.cpp:2333-2339`). `h` stays the human flag. Consider adding `spec` from
`AiFactory::GetPlayerSpecName` while the field is being touched.

**Then promote `human-in-raid` to decidable.** With a real role, `validity.inspect` can say *a human
held tank* rather than *a human was present*, and `DECIDABLE` in `batch.py:48` can include it. This
changes what `--valid` returns on future traces — by design; it is what made the disqualifier
vacuous. Old traces keep `"human"` and must stay informational, so gate on schema version.

No schema bump: both are corrections to what v12 already claims to record.

---

## Files touched

| Purpose | Path |
|---|---|
| Generic probe reader | `tools/botobs/probes.py` (new), `postmortem.py`, `records.py` (guid substitution) |
| Metric extraction | `tools/botobs/views.py`, `coverage.py`, `batch.py` |
| Compare | `tools/botobs/batch.py`, `validity.py` (`resolve_since` reuse) |
| Boss identity | `tools/botobs/obstrace.py`, `validity.py`, `batch.py` |
| Tests | `tools/botobs/tests/` (new) |
| Recorder holes | `src/Bot/Obs/RaidObsEngine.cpp`, `src/Bot/Obs/RaidObsSession.cpp` |
| Docs | `docs/systems/observability.md` |

---

## Verification

Each item must **reproduce** a known result, not merely run clean.

- **3.1** — the churn ranking, run over the pre-fix Mimiron traces with no hint about what to look
  for, must surface `follow ↔ arc spread` at or near the top and put the count near the documented 58
  and 49 (`docs/raids/ulduar/mimiron.md:65,78`). The mute check must report `fl.frozen` against the
  7 Flame Leviathan traces and must **not** report Algalon or Vezax keys (never pulled). Guid
  substitution must resolve names inside `thorim.shelter` payloads, which are `slot N orb <guid>`.
- **3.2** — `--split-at` across the commit `mimiron.md:78` credits (`eb9db6855` is the candidate;
  confirm from the doc) must show the `follow ↔ arc spread` churn metric collapse to zero across the
  split. Peak RSS flat across the corpus, as in Tier 2 — two selections must not mean two corpora in
  memory.
- **3.3** — `--boss xt-002` returns all 3 XT traces where today `xt002` returns 1 and
  `xt-002-deconstructor` returns 2, and the census shows one row. All 129 traces still read
  17/17 where they did before. pytest green, and each new check demonstrated to fail against a
  deliberately corrupted fixture.
- **3.4** — on the next Flame Leviathan pull, `fl.frozen` rows appear and their count is in the same
  order as the other `fl.*` keys. On any pull with you in the raid, the roster shows a real role, and
  if you tank, `--valid` drops that trace with `human-in-raid` as a decidable disqualifier.
  `pb-syntax-check.sh` over both changed `.cpp` and every `.cpp` including a changed `.h`; scoped
  `pblint.py`; whole-tree `codestyle-cpp.py`.

---

## Found this pass, not this cycle

- **`PerfMonitor::start` allocates with the monitor off.** `Engine.cpp:627-628` calls
  `trigger->getName()`, which returns `std::string` **by value** (`AiObject.h:36`), into a parameter
  taken **by value** (`PerfMonitor.h:68`); the `perfMonEnabled` early-out is inside the callee
  (`PerfMonitor.cpp:21-22`), after both copies. Most Ulduar trigger names exceed SSO, so this is
  ~171 heap allocations per Ulduar bot per tick with the perf monitor switched off. A call-site guard
  or `std::string const&` parameters removes it. Cheapest measured win available.
- **The other 21 raids have no gate.** Their substitute is `AI_VALUE2(Unit*, "find target", "<boss>")`
  at the top of ~176 multiplier functions across 14 files, and `FindTargetValue` is constructed with
  a check interval of 1 (`TargetValue.h:133-136`), so it recomputes every tick, once per multiplier
  per queued action. Ulduar's gate is ~200 of ~270 lines generic; the per-raid part is an encounter
  enum and a prefix table. Prefix coverage if built today: RS 100%, BT 98%, ZA 96%, SWP/SSC/ICC 94%,
  Naxx 89%, TK 85%, Kara 84%, ToC 81%.
- **Ulduar's 54 multipliers are ungated** even inside Ulduar; only `RazorscaleMultiplier` checks
  `UldEncounterIsLive`. Already recorded in `docs/plans/ulduar-perf-followups/`.
- **Stale counts in shipped comments.** `UldEncounterGate.h:36,60` and `.cpp:90` say 165 gated
  triggers; it is 171, and all 171 match a prefix. `docs/raids/README.md:81` says "eight encounters"
  share the suppression idiom; strictly it is 14 functions, loosely 73 across ~66 encounters — and
  the Tier 2 plan's "undercounts by half" was wrong in the other direction.
- **Multiplier boilerplate.** 282 multiplier classes across 35 headers are byte-identical but for the
  class name and the string — ~1,700 lines a macro collapses. Per-tick memoisation is hand-rolled
  nine separate times.
- **The 17 base/class pblint findings**, and the 13 traces failing invariants.
- **The build and redeploy step is the only wholly undocumented step in the loop** — no script, no
  Makefile target, no doc in either repo.
