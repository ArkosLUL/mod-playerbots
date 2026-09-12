# RaidObs audit, 2026-09-10/11: fix the `hp` contract, name the pre-roll pets, make the audit repeatable

## Context

`RaidObs` is a per-instance NDJSON session recorder for playerbot raid pulls. Its premise: hand a
fresh agent session a trace file path and it can diagnose why a specific bot died, without video.
Recorder in `src/Bot/Obs/` (six files by concern), analyzer in `tools/botobs/`, schema contract in
`docs/systems/observability.md`. Schema is v10 and unchanged since `685ab2661` (2026-09-05).

The question this round is two questions: does the framework *collect* correctly, and is the
collected data *true*? Prior audits only asked the first.

28 traces were recorded on 2026-09-10 (11) and 2026-09-11 (17): Thorim ×11 and Mimiron ×12 on map
603, plus **Malygos** (616) and **Sartharion** ×2 (615) — the first non-Ulduar maps any audit has
seen. All schema v10, 356 MB.

### Collection: correct

| Check | Result |
|---|---|
| unparsable lines / truncated traces | 0 / 0 |
| `snap.u` rows, all exactly 12 wide | 2 462 485 / 2 462 485 |
| `move.pr` present | 116 960 / 116 960 (100%) |
| `move` `wait` rows carrying both `hpr` and `hms` | 49 923 / 49 923 (100%) |
| `cast` rows with no spell id | 0 |
| **unresolved spell ids** | **0** (was 99 on 2026-09-04/05) |
| unnamed guid refs | 245 — all pre-roll pet rows, see Defect 2 |
| deaths with `hplast` | 752 / 752 |
| blow-less deaths carrying `cause` | 108 / 108, all `reset`; 0 carry both `cause` and `blow` |
| deaths missing `acts`/`lastmove` | 41, **all 41 human players**, 0 bots |
| snapshot cadence p50 / p95 vs 250 ms | 221–303 ms / 319–333 ms |
| pet rows with a non-zero `dealt` | 0 / 299 817 (by design — a pet credits its owner) |
| traces named by `engage`, none falling back to the map name | 28 / 28 |
| analyzer views (default, `--notes`, `--stalls`, `--clump`, `--death`) | all exit 0, both new maps included |
| KB/s | 27.1–51.2 |

**The 2026-09-05 cast-column fix is live**: unresolved spell ids went 99 → 0. The naming work from
the same commit was never exercised — every pull engaged a real boss, so nothing needed renaming.

**No pull was caught late.** Every trace carries a full 30 s pre-roll and a healthy raid at `t=0`
(zero traces with more than two raiders below 90%). `end.out` matches the roster in all 28: every
`wipe` has 21–24 of 24 dead, the one `reset` has 11, both `kill`s have 0.

### Data: true, on every external check available

| Cross-check | Result |
|---|---|
| creature entries against `creature_template` | 91 / 91 resolve; 86 exact name match, 5 are player pets carrying their own names (Felguard "Flaaghun") |
| spell names against `Spell.dbc` | **1092 / 1092 exact, 0 disagreements** (21 more absent from the reference CSV are genuine internal spells) |
| aura `dur` against the DBC base duration | 332 851 checked; 3 125 longer, from exactly 14 spells, every one talent/glyph-extended (Glyph of Thorns ×5, Mixology ×2, Glyph of Horn of Winter ×1.5) — the recorder reports the real modified duration |
| `death.dist` against the snapshots either side of the death | 1 848 entries, **0** outside the bracketed range |
| `death.hplast` against the last snapshot | 0 disagreements |
| `death.blow` against the damage ledger | 644 deaths with a blow, **0** with no damage-log row behind them, 5 marginal gaps explained by healing inside the final 250 ms |
| `death.rewind` time ordering | 0 out of order |
| `dealt` throughput against the boss health pool | Malygos kill 29.6M vs 23.2M (1.28×), Sartharion kill 30.3M vs 18.0M (1.68×), Sartharion reset 12.7M vs 21.3M (0.60×) — physically consistent |
| coordinates, hp%, mana%, `dealt` monotonicity | 0 non-finite, 0 out of range, 0 backwards |
| hazard rows | 114 776, 0 malformed, one stable and correct radius per spell |

The 1 705 implied-speed outliers are all real game events, and the trace explains its own: a mage's
two position jumps are each preceded ~100 ms earlier by a `Blink` cast at exactly Blink's distance;
Thorim's trigger bunnies are repositioned exactly 128.0 yd; Sif blinks; the 8 300 yd/s samples are
the raid zoning in during pre-roll. The `moving` flag agrees with actual displacement on 96.8% of
472 428 samples — 6 961 disagreements sit on a flag transition (a 250 ms sampling boundary) and
3 374 are a bot asking to move and not moving, which is the signal `--stalls` exists to surface.

Two defects, both in how the data is *labelled* or *named* rather than in what is measured.

---

## Defect 1 — `dmg.hp` is the health *before* the hit, and everything says "after"

The doc's schema table reads ``| `dmg` | …,`hp` after |`` ([observability.md:100](docs/systems/observability.md#L100)).
It is the health **before** the hit lands.

`NoteDamage` ([RaidObsCombat.cpp:88](src/Bot/Obs/RaidObsCombat.cpp#L88)) writes
`victim->GetHealthPct()` at hook time, and all three damage hooks fire *before* the core applies the
damage:

| Path | logs at | applies at |
|---|---|---|
| spell | `Spell.cpp:2887` | `Spell.cpp:2894` |
| melee | `Unit.cpp:2835` | `Unit.cpp:2839` |
| periodic | `SpellAuraEffects.cpp:6427` | `SpellAuraEffects.cpp:6429` |

The measurement confirms it twice over. **`hp` never once reads 0 in 36 885 damage rows** — if it
were post-hit health every killing blow would. And reconciling consecutive hits on one victim against
`mhp` fits "before" 18× better: median error 0.073 percentage points (3 780 of 4 257 pairs within 1
pp) against 1.322 pp for "after" (1 749).

**`heal.hp` really is after** — `HealBySpell` deals at `Unit.cpp:8456` and logs at `8459` — measured
at 0.007 pp median error over 32 744 pairs. So the two `hp` fields in adjacent rows of the same doc
table mean opposite things, and nothing says so.

This is not cosmetic. `views.py:34` renders the damage row as `-> {hp}%`, an arrow that reads as
"this hit took the bot to here". The `--bot` timeline for a real death prints:

```
1:52.514  dmg   9215 from Proximity Mine Explosion 63009 -> 100.0%
1:52.515  dmg  10367 from Proximity Mine Explosion 63009 -> 61.34%
1:52.515  dmg  11518 from Proximity Mine Explosion 63009 -> 17.84%
1:52.520  DIED   killed by Proximity Mine
```

Read as written, a 9 215 hit left the bot at full health and the bot then died at 17.84% from
nothing. Read correctly the trajectory is shifted one row down and the last line is the killing blow:
11 518 landing on 17.84% of a 25 562 pool. `ok` (overkill) is the only in-row signal that a hit was
lethal, and the doc never says so.

### Fix

Keep the field and its value — it is true, and the pre-hit reading is the more useful one in a death
rewind. Correct the label in the two places that carry it:

- `docs/systems/observability.md:100` — ``` `hp` health before the hit ```, leaving `heal`'s "after"
  alone, plus a short bold rule under the table naming the asymmetry, why it exists (the core logs
  before applying damage and after applying a heal), and that `ok` is how a reader tells a hit was
  lethal because `hp` never reads 0.
- `tools/botobs/views.py:34` — render the damage row so it cannot be read as an outcome. Change the
  arrow to a "from" reading, e.g. `… 9215 from Proximity Mine Explosion 63009 (at 100.0%)`, leaving
  line 36's heal arrow as it is.

Run `/compact-docs-writer` **before** editing the doc, as the first step of that task.

**No schema bump.** No emitted value changes.

---

## Defect 2 — pre-roll snapshots name neither pets nor ridden vehicles

`BuildSnapshotPayload` gates both naming calls on having a session:
`session->EnsureUnit(vehicle)` ([RaidObsSnapshot.cpp:230](src/Bot/Obs/RaidObsSnapshot.cpp#L230)) and
`session->EnsureUnit(pet)` ([RaidObsSnapshot.cpp:271](src/Bot/Obs/RaidObsSnapshot.cpp#L271)). The
pre-roll path passes `nullptr`, so a pre-roll row can carry a guid nothing in the file names — the
same shape as the cast-column defect fixed in `685ab2661`, which swept the pre-roll's *spell* ids and
left its *unit* guids behind.

245 rows across 10 guids in 2 of 28 traces (0.08% of 299 817 pet rows), all at negative `t`, all
pets. They are guardians that expired before the pull — Army of the Dead ghouls and Mirror Images,
which live about 30 s. That is exactly why a lookup at drain time cannot fix it: the unit is gone by
then. **The name has to be captured at sample time.** The owner goes with it, so today a reader
cannot even say whose pet it was.

### Fix

Split the record building out of `EnsureUnit` ([RaidObsSession.cpp:269](src/Bot/Obs/RaidObsSession.cpp#L269)),
so the same fields can be produced without a session:

```cpp
std::string UnitRecordFields(Unit* unit);              // the existing body, minus the dedupe and Emit
void ObsSession::EnsureUnit(Unit* unit);               // dedupe on seenUnits, then Emit the above
void ObsSession::EnsureUnitRecord(uint64 key, std::string const& fields);  // same dedupe, pre-built
```

Then carry the records through the ring the way `castSpells` already travels:

- `PreRollRing` ([RaidObsSession.h:183](src/Bot/Obs/RaidObsSession.h#L183)) gains
  `std::unordered_map<uint64, std::string> unitRecords`. Dedupe at *sample* time so each pet is
  serialised once per pre-roll window rather than 120 times — this is the reason it lives on the ring
  and not on `PreRollEntry`.
- `BuildSnapshotPayload` takes an optional out-param alongside `castSpells` and fills it for the pet
  and vehicle branches when `session` is null.
- The pre-roll push ([RaidObsLifecycle.cpp:310](src/Bot/Obs/RaidObsLifecycle.cpp#L310)) merges what
  the sample collected into `ring.unitRecords`.
- The drain ([RaidObsLifecycle.cpp:135](src/Bot/Obs/RaidObsLifecycle.cpp#L135)) emits each record via
  `EnsureUnitRecord` before the first payload, mirroring the `EnsureSpell` loop already there.

A pet whose rows have since been shed from the ring leaves one orphan `unit` record. That is what a
`unit` record is — a lookup table entry — so it is harmless and not worth pruning.

**No schema bump.** The trace gains `unit` records it should always have had.

---

## Fix 3 — `--verify`, so the next audit is one command

Every audit so far (2026-08-30, 08-31, 09-04/05, and this one) has been hand-rolled Python in a
scratchpad, and both defects above were found that way. The invariants are stable enough to live in
the tool.

Add `show_verify(trace)` to [views.py](tools/botobs/views.py) and a `--verify` flag to
[postmortem.py](tools/botobs/postmortem.py), following the existing one-function-per-mode shape
(returns an exit code; non-zero when a check fails, so it can gate a batch run). Checks, all already
written and measured in this audit:

| Check | Passing value on these 28 |
|---|---|
| every guid and spell id referenced anywhere is named | 245 unnamed today, 0 after Defect 2 |
| `snap.u` row width, hp%/mana% range, finite coordinates | 12 wide, 0..100, finite |
| `dealt` never decreases; 0 on pet rows | holds |
| `dmg`/`heal` reconcile against `mhp` between consecutive hits | median 0.073 pp |
| `death.hplast` matches the last snapshot; `death.dist` within the bracketing snapshots | 0 / 1 848 wrong |
| `death.blow` has a damage-log row behind it | 644 / 644 |
| `death.rewind` in time order; `death.auras` `removedT` ≥ `appliedT` | holds |
| blow-less deaths carry `cause`; no death carries both | 108 / 108 |
| `end.out` against the roster's final state | holds |
| snapshot cadence against `SnapshotIntervalMs` | p50 221–303 ms |

`Trace` does not currently keep `mhp`; add `self.maxhp: dict[int, int]` in
[obstrace.py](tools/botobs/obstrace.py) `_load` alongside `owners`. Backwards compatible — an older
trace simply has fewer entries and the ledger check skips those guids.

---

## Files

- `docs/systems/observability.md` — the `hp` label and the asymmetry rule (`/compact-docs-writer` first).
- `tools/botobs/views.py` — damage-row rendering; new `show_verify`.
- `tools/botobs/postmortem.py` — `--verify` flag and docstring line.
- `tools/botobs/obstrace.py` — `Trace.maxhp`.
- `src/Bot/Obs/RaidObsSession.cpp` / `.h` — `UnitRecordFields`, `EnsureUnitRecord`, `PreRollRing::unitRecords`.
- `src/Bot/Obs/RaidObsSnapshot.cpp` — collect pet and vehicle records when there is no session.
- `src/Bot/Obs/RaidObsLifecycle.cpp` — merge at the pre-roll push, emit at the drain.

`src/Ai/Raid/Uld/` and `src/Util/EncounterHelpers.*` are being edited by another session right now —
re-check `git status` before staging, and stage paths explicitly.

---

## Verification

Static checks are mine; the build and the pull are the user's.

1. Per-TU `-fsyntax-only` on each changed `.cpp` against the compile database inside
   `acore/ac-wotlk-build:master` (`/azerothcore/build/compile_commands.json`), mounting the working
   tree's `src` read-only over `/azerothcore/modules/mod-playerbots/src`. Needs `MSYS_NO_PATHCONV=1`
   in Git Bash or the container paths get mangled. No link is possible.
2. `--verify` over all 111 traces on disk. It must exit 0 on every v10 trace except the 245 unnamed
   pre-roll pet rows in the two named traces, which only a rebuilt server can clear.
3. Re-run every view over a sample spanning all four bosses plus both new maps; all must still exit 0.
4. Confirm the damage row now reads as a pre-hit health and the `--bot` timeline for
   `603_1_mimiron_1789137577.ndjson --bot Assasin` no longer shows `9215 … -> 100.0%`.

After a build and a pull:

5. A pull with a death knight or mage who summons before the pull: no unnamed guid in the pre-roll,
   and the ghouls/images carry `own`.
6. A clean kill still closes `kill`, an empty instance still closes `idle`, and a wipe still `wipe`.

---

## Out of scope

- **The `--bot` heal arrow** (`views.py:36`) is correct as written and stays.
- **`NoteAuraApplied`** ([RaidObsCombat.cpp:183](src/Bot/Obs/RaidObsCombat.cpp#L183)) sets
  `state.caster` without calling `EnsureUnit`, so a same-tick aura can still reach `death.auras`
  unnamed. It did not fire in these 28 traces (0 unnamed aura casters) because no raid member left
  the map mid-pull; it remains a live route.
- **The `.die` GM command** reading as an ordinary combat death — carried from the last audit, and it
  did **not** recur: 0 of 644 blow-carrying deaths lack a damage-log row behind them.
- **The 3 374 steady-state `moving` disagreements** are bots asking to move and not moving. That is a
  strategy or pathing signal, not a recorder defect, and `--stalls` already surfaces it.
- **Mimiron and Thorim themselves.** 25 of 28 traces are wipes on those two. An encounter problem,
  not a recorder one.
