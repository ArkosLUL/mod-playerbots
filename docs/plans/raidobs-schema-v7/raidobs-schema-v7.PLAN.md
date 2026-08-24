# RaidObs defect fixes found at Thorim (schema v6 → v7)

## Context

RaidObs is a per-instance NDJSON session recorder for playerbot raid pulls. Its premise: hand a
fresh agent session a trace file path and it can diagnose why a specific bot died, without video.
Recorder in `src/Bot/Obs/`, analyzer in `tools/botobs/postmortem.py`, schema documented in
`docs/systems/observability.md`.

Schema v6 shipped and was audited against the first two Thorim pulls:

| trace | span | deaths | size | cost |
|---|---|---|---|---|
| `603_3_thorim_1787596597.ndjson` | 237.3 s | 29 | 9.58 MB | 36.7 KB/s |
| `603_3_thorim_1787596946.ndjson` | 155.6 s | 25 | 7.16 MB | 39.5 KB/s |

Both `out:"wipe"`, 25 members, `diff 1`. **Everything v6 added works.** All five analyzer views
(`--death`, `--bot`, `--track`, `--notes`, default) exit 0 on both:

| v6 fix | evidence on these traces |
|---|---|
| `EnsureUnit` sweep | 0 unnamed guids anywhere: `aura.d`/`aura.s`, `cast.s`/`cast.tgt`, `dmg`/`heal`/`abs`, `death.auras` casters, `death.rewind` sources, killers |
| move priority | 6538 / 5922 move rows, **100%** carry `pr` |
| holder on a refusal | 2976 / 2809 `wait` rows, **100%** carry `hpr` + `hms` |
| note guid join | `thorim.chargedorb` renders `Thunder Orb`; `thorim.slot` / `arenaarrived` / `barrierbail` correctly left as bare counters |
| killing blow | 29/29 and 25/25 deaths carry `blow` + `hplast` + a named killer |
| cost | 36.7 and 39.5 KB/s, under the 48 KB/s budget |

The v6 move fields paid off immediately. **Neither trace contains a single `forced` move** — every
Thorim mover runs at `combat` or `normal`, so the waits are equal-priority standoffs the gate can
never break: `normal` held by `normal` and `combat` held by `combat`, plus `follow` losing to Thorim
positioning. That is a strategy diagnosis read straight off the trace, which is the whole point.

No cross-boss strategy leak: zero `act` / `veto` / `move` / `note` records naming any Ulduar boss
other than Thorim.

Auditing the two traces then found three defects. Two are `NoteHazard` surfacing on its **first
real use** — Hodir traces emit zero `haz` records, so the function was effectively dead code until
Thorim. The third is older and neither the v5 nor the v6 audit caught it, because both only looked
for unresolved ids.

**Decided with the user:** fix the hazard *naming* and document the death-block limitation. Do not
add lane geometry — that needs a navprobe-verified lane and is strategy work owned by another
session.

---

## Defect 1 — `NoteHazard` never calls `EnsureSpell`

The v5 sweep added `EnsureSpell` at 8 emission points and missed this one, the only guid/spell
bearing emitter that resolves its session from a `Map*` (`FindSession`) rather than a `Unit*`
(`SessionFor`).

Measured: 121 and 89 `haz` records carrying `62057` / `62058` (Runic Smash left/right). Those two
ids appear **nowhere else in either file** — not in `cast`, `aura`, `dmg`, `heal`, nor `snap.hz` —
so nothing else can name them incidentally. The analyzer falls back to its placeholder:

```
0:05.207  haz    spell 62058 lane at (2227.5, -396.18, 412.18) ttl 500 side=right
```

This stayed hidden through every prior audit because Hodir emits **0** `haz` records.

### Fix

[RaidObs.cpp:1892](src/Bot/Obs/RaidObs.cpp#L1892) `NoteHazard` — add `EnsureSpell(*session, spellId);`
after the `FindSession` null check and before `Emit`. The session is already in hand, so no
restructuring. `NoteHazardCircle` delegates here and is covered by the same line.

---

## Defect 2 — `haz` records are invisible to the death block

`hazards_at` ([postmortem.py:201](tools/botobs/postmortem.py#L201)) reads only
`snapshot["hz"]`. It never consults `haz` records. The two channels are fully disjoint on these
traces:

- `snap.hz` — Consecration, Blizzard, Death and Decay, Volley, Flamestrike, Hurricane, Paralytic
  Field. All player-cast, all named, and **never** the subject of a `haz` record.
- `haz` — only `62057` / `62058`, and **never** present in any `snap.hz` row.

So a boss mechanic recorded through `NoteHazard` can never produce a `STOOD IN` or `NEAR` line.
That is precisely the question the recorder call exists to answer — see the comment at
[UldEncounter_Thorim.cpp:423](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L423): *"no way to tell
afterwards which lane was hot when somebody died in it."*

Latent, not yet realised: `62057` / `62058` appear in no `dmg`, `aura` or `cast` record in either
trace, so nobody took Runic Smash damage and no diagnosis was actually lost.

A `lane` also carries no geometry. The origin is `colossus->GetPosition()` — byte-identical for
both lanes across both traces — with no heading, width or length. `NoteHazardCircle` emits `rad`,
so a circle is testable; a lane is not. Only the `side` field separates them.

### Fix — documentation only

Per the decision above: record in `docs/systems/observability.md` that `haz` is a **timeline-only**
channel, that the death block's hazard test covers `snap.hz` alone, and that a `lane` shape carries
no geometry so containment cannot be computed from it. Closing it properly needs a navprobe-verified
lane and belongs with the strategy session.

---

## Defect 3 — `death.auras` reports a false `held` when the apply was never seen

Not a v6 regression. Present in the v5 Hodir trace too, and missed by both prior audits because
they only checked for unresolved ids.

`NoteAura` ([RaidObs.cpp:1612](src/Bot/Obs/RaidObs.cpp#L1612)) leaves `state.appliedMs` at 0 when
the first event it sees for a `(bot, spellId)` pair is a **removal** — the normal case for anything
buffed before the pull. `NoteDeath` ([RaidObs.cpp:1977](src/Bot/Obs/RaidObs.cpp#L1977)) then emits
`s.Stamp(state.appliedMs)` unguarded. `Stamp(0)` is `static_cast<int32>(0 - startMs)`, so the
column gets `−startMs` — one constant shared by every bot in the session.

The very next line, [1978](src/Bot/Obs/RaidObs.cpp#L1978), already guards the identical case for
`removedMs` with a `-1` sentinel. The convention exists; it was just not applied to `applied`.

The analyzer computes `held = (death_t - applied) / 1000`
([postmortem.py:160](tools/botobs/postmortem.py#L160)), so the bogus stamp prints directly:

```
Mortal Strike 35054 x1    5.0s left  held 1740.7s  from Dark Rune Champion
```

1740.7 s in a 155.6 s fight. That row's caster is a Dark Rune Champion that spawned mid-fight, so
it demonstrably was not applied before the pull.

| trace | death.auras rows | negative `applied` | of which debuffs |
|---|---|---|---|
| thorim 1787596597 (v6) | 733 | 282 (38%) | 6 |
| thorim 1787596946 (v6) | 706 | 267 (38%) | 1 |
| hodir 1787595615 (v6) | 1333 | 302 (23%) | 2 |
| hodir 1787590072 (v5) | 996 | 288 (29%) | 0 |

Most are buffs, which render as a bare name list where the number never prints. The debuffs do
print, in the block the doc names as the first thing to read.

The debuff sort is unaffected: it keys on `-a[4]`
([postmortem.py:365](tools/botobs/postmortem.py#L365)), so a large negative `applied` sorts last
within the held-to-death group rather than crowding the top. Only the printed number lies.

### Fix

**`RaidObs.cpp`** — mirror line 1978 exactly:

```cpp
auras += "," + std::to_string(state.appliedMs ? s.Stamp(state.appliedMs) : int64(-1));
```

**`postmortem.py`** — `aura_line` ([:158](tools/botobs/postmortem.py#L158)) renders the sentinel
instead of arithmetic on it. An unknown apply time is honest; a fabricated duration is not:

```python
held = "held     ?" if applied < 0 else f"held {(death_t - applied) / 1000:5.1f}s"
```

Keep the column width so the block stays aligned. `held_to_death` reads `row[5]`, not `row[4]`, so
it needs no change.

Older traces keep the old meaning — a negative `applied` there is the bogus `−startMs`, and
rendering it as `?` is the right answer for those too, so no version gate is needed on the read
path.

---

## Schema and docs

**Bump `SCHEMA_VERSION` to 7** ([RaidObs.h](src/Bot/Obs/RaidObs.h)). Defect 3 changes the value
domain of an existing column rather than adding a field, so a reader that does not know about the
`-1` sentinel would compute a wrong number from a v7 file.

**`postmortem.py`** — `SUPPORTED_SCHEMA = 7`, `READABLE_SCHEMAS = (4, 5, 6, 7)`.

**`docs/systems/observability.md`**
- Schema table `v: 7`; `death.auras` applied column gains the `-1` sentinel and its meaning.
- The `haz` limitation from defect 2.
- Extend the existing **Name every id in the file** rule: it already covers `EnsureUnit` on a new
  guid field; add that a *new emitter* needs the sweep too, not just a new field. `NoteHazard` is
  the worked example — it sat unnoticed because no boss exercised it until Thorim.
- New rule worth stating plainly: **an unknown timestamp gets a sentinel, never arithmetic.** Line
  1978 had it right and line 1977 did not, in adjacent lines of the same loop.

Per the standing rule, invoke `/compact-docs-writer` **before** editing this file, as the first step
of the doc task. The v6 invocation belonged to that cycle and does not carry over.

---

## Verification

Static checks are mine; the build and the pull are the user's — there is no headless build path here.

1. Build the module.
2. Re-run the analyzer against the two v6 Thorim traces and the v5 Hodir trace. All must still
   exit 0 on all five views, and no `held` over the fight span may print.
3. Pull Thorim. Against the new trace:

| Check | v6 measured | Expected v7 |
|---|---|---|
| `haz` rows whose `sp` has no `spell` record | 121 / 89 | **0** |
| `death.auras` rows with a negative `applied` | 282 / 267 | unchanged (still emitted, now as `-1`) |
| `held` values exceeding the fight span | 282 / 267 | **0** printed |
| `move` rows carrying `pr` | 100% | 100% |
| `wait` rows carrying `hpr` | 100% | 100% |
| unnamed guids, any field | 0 | 0 |
| bytes per second | 36.7 / 39.5 KB | ≤ 40 KB |

4. `postmortem.py <file> --notes thorim.` — `haz` lines must read `Runic Smash 62057` / `62058`,
   and the `side=` field must still be present.
5. Spot-check one death block: no `held` larger than the fight span, and any unknown apply time
   renders as `?`.

## Out of scope

- Lane geometry and any `hazards_at` change (decided against above).
- Fixing the Thorim strategy. Another session owns that, including the two corrections already
  handed over: the three `110.0f` trigger gates do fire at Hodir (min 96.08 yd), and
  `ULDUAR_THORIM_WING_MAX_Z = 425.0` leaks at z 423.93.
- **Trigger and strategy telemetry — still unimplemented, still the biggest blind spot.** Both
  traces show zero non-Thorim activity, but that is the same weak evidence that let Thorim's
  strategy run through three Hodir fights unnoticed: `act` records only exist once an action is
  selected. `RaidUlduarStrategy::InitTriggers`
  ([UldStrategy.cpp:11](src/Ai/Raid/Uld/UldStrategy.cpp#L11)) still registers all 90 Ulduar triggers
  for every boss, and nothing in the trace would show it.
