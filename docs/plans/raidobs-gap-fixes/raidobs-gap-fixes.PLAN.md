# RaidObs gap fixes (schema v3 → v4)

## Context

`RaidObs` shipped and is recording. First real trace analysed:
`env/dist/logs/botobs/603_3_hodir_1787435701.ndjson` — 19.7 MB, 218,241 lines, 0 truncated, 0
unparsable, 23 deaths, boss taken 100% → 67.8%. The pipe works end to end.

But the file cannot answer the question the framework exists for. Six gaps, verified against the
trace and the source:

| # | Gap | Evidence |
|---|---|---|
| 1 | Death records contain **no boss debuffs at all** | Biting Cold (62039) applied 731× / removed 174× → 557 outstanding. Present in 0 of 23 death records. Same for Flash Freeze, Freeze, Icicle. Death aura lists are ~57 permanent talents/racials. |
| 2 | Aura records carry **no stacks, no duration** | Fields are `d,s,sp,r` only. On Hodir stacks *are* the kill mechanic. |
| 3 | Change-only emission broken | `act` 156,701 emitted / 15,283 needed (90.2% waste). `veto` 25,412 / 186 (99.3%). 14.6 MB of 19.7 MB is duplicate. Death `acts` ring averages 472 entries = 95% of each death record. |
| 4 | `cast` unscoped, and drops all bot casts | 6,329 casts, **0** from roster. Top casters `Flaaghun` (253), `Guardian Lasher` (211) — Freya-area trash elsewhere in instance 3. Hodir himself only 245. |
| 5 | Hodir has zero strategy instrumentation | Only 2 `note` records in the file, both `thorim.squadsassigned` leaking in. You can see where bots stood, not what they were told to do. |
| 6 | Hazards unusable | Radius 0 on **exactly the three hostile spells** — Icicle 62234/62236/62462, 2,484 of 6,720 rows. Every friendly AoE carries a correct radius. So the other 4,236 rows are bot Death and Decay / Blizzard / Consecration / Hurricane / Volley / Explosive Trap, unflagged, and the one hazard that matters is the one you cannot test against. Lethal icicles are also *Creatures*, never enter the watched set, and so are absent from snapshots entirely. |

**Outcome:** a Hodir trace where a death record alone says which debuff stack killed the bot, what it
was told to do, and whether it was standing in something — at roughly a quarter the file size.

### Decisions taken

| Question | Answer |
|---|---|
| Instrumentation scope | **Hodir only.** Other bosses stay bare until raided. |
| Bot casts | **Restore, roster only** (players + their pets + watched creatures). |
| Hazard creatures | **Generic hostile sweep into snapshots.** No per-boss call sites. |

## Fixes

### Fix 1 — death records must carry the debuffs (severe)

Root cause is core ordering, not module code: `Unit.cpp:14190` calls
`victim->RemoveAllAurasOnDeath()`, `Unit.cpp:14358` calls `sScriptMgr->OnUnitDeath()`. 168 lines
apart. By the time the hook fires only `IsPassive() || IsDeathPersistent()` auras survive, which is
exactly what the trace shows.

Stop walking `unit->GetAppliedAuras()` in the death builder
([RaidObs.cpp:1544](src/Bot/Obs/RaidObs.cpp#L1544)). Track the set instead, in `BotTrace`:

```cpp
struct AuraState
{
    uint64 caster;
    uint32 stacks;
    int32  duration;
    uint32 appliedMs;
    uint32 removedMs;   // 0 while held
};
std::unordered_map<uint32 /*spellId*/, AuraState> auras;
```

Fed from `NoteAura`. Two things make it robust:

- **Update the set before the `g_cfg.logAuras` gate**, so it stays complete when aura record
  emission is switched off.
- **On removal, stamp `removedMs`; never erase.** `AuraApplication::ClientUpdate`
  (`SpellAuras.cpp:229`) is the hook's only call site and whether the death-strip reaches it is
  timing-dependent. Marking instead of erasing makes the fix correct either way. Prune entries whose
  `removedMs` is older than `deathRewindMs` (15 s) on the same pass that trims the damage ring.

Death record emits everything held plus anything dropped in the last 2 s, flagged. This also removes
the per-death iteration over every applied aura, so the death path gets cheaper, not dearer.

### Fix 2 — aura records need stacks and duration

[RaidObs.cpp:1262](src/Bot/Obs/RaidObs.cpp#L1262). The hook already hands over `Aura*`
([RaidObsScripts.cpp:149](src/Bot/Obs/RaidObsScripts.cpp#L149)), so `GetStackAmount()` and
`GetDuration()` are free. Add `st` and `dur`. Falls out of Fix 1 — the same two values feed the
tracked set.

### Fix 3 — emit per tick, not per action

The existing check at [RaidObs.cpp:1325](src/Bot/Obs/RaidObs.cpp#L1325) is a single last-value slot
per bot:

```cpp
if (trace.lastAction == action && trace.lastVerdict == verdict)
```

`Engine::DoNextAction` emits ~4 verdicts per pass in a fixed repeating cycle
(`pull action` / `avoid aoe` / `hodir raid position action` / `set facing`), so consecutive records
never match and nothing is ever suppressed. `NoteVeto` has no dedup at all.

Latching per action name would fix the stream but not the ordering loss in death records. Move to a
**tick model** instead — it is the semantics "change-only steady state" was always meant to have:

- Add `RaidObs::BeginTick(Player*)` at the top of `Engine::DoNextAction`, gated on `Active()` like
  the existing `ObsVerdict` helpers in [Engine.cpp](src/Bot/Engine/Engine.cpp).
- `NoteAction` / `NoteVeto` append into the bot's current tick buffer rather than emitting.
- `BeginTick` flushes the previous tick: compare its signature against the one before. Different →
  emit its `act` / `veto` records. Identical → bump a repeat counter, emit nothing.
- The death ring becomes a deque of ticks with repeat counts. Since the cycle repeats verbatim,
  1304 entries collapse to roughly 15 rows **with interleaving order intact** — strictly more useful
  than the raw list, not just smaller.
- `NoteDeath` flushes the in-flight tick first, so the last tick before dying is never lost.

Bounded by design: one buffer per bot, a handful of actions per tick.

### Fix 4 — scope `cast` to the pull, restore roster casts

[RaidObs.cpp:1280](src/Bot/Obs/RaidObs.cpp#L1280) drops every `TYPEID_PLAYER` caster and gates
creatures on `SessionFor` alone — which only proves same-instance, hence the Freya trash. Replace
with: emit when the caster is a roster player, a roster player's pet/totem, **or** in `s.watched`.
Delete the "bot casts are already covered by the action stream" comment — the action stream records
the decision, not that the cast actually started.

### Fix 5 — instrument Hodir

Hodir holds no latched state; every value is derived fresh per call, deliberately
([UldBossHelper.h:1118](src/Ai/Raid/Uld/Util/UldBossHelper.h#L1118) explains why). So the traced-
container pattern does not apply. Add a change-latched derived-note probe instead, reusing the tick
machinery from Fix 3:

```cpp
void RaidObs::NoteDerived(Player* bot, char const* key, std::string const& value);  // emits only on change
```

Probe inside the helper getters in
[UldBossHelper.cpp](src/Ai/Raid/Uld/Util/UldBossHelper.cpp), not at call sites — trigger and action
both route through them, so one probe covers both and they cannot disagree:

| Helper | Note key |
|---|---|
| `GetHodirAnchor` | `hodir.anchor` (position + tolerance, or `none` for melee) |
| `GetHodirRingCentre` | `hodir.centre` |
| `GetHodirRingSlot` | `hodir.slot` |
| `GetHodirShuttleLeg` | `hodir.shuttle` |
| `GetHodirSharedShelter` | `hodir.shelter` |
| `IsHodirTrappedAllyBreaker` | `hodir.breaker` |

Plus the two `AttackAction` targets in
[UldActions_Hodir.cpp](src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp):
`HodirSetDpsPriorityAction` → `hodir.dpstarget`, `HodirFrozenBlowsSwapAction` → `hodir.tankswap`.

### Fix 6 — make hazards mean something

Three changes in [RaidObs.cpp:556](src/Bot/Obs/RaidObs.cpp#L556) (`HazardRows`) and
`BuildSnapshotPayload`:

- **Radius fallback.** `dyn->GetRadius()` returns 0 for the boss dynobjects specifically — all three
  Icicle spells, while every bot AoE reports correctly. The real value is on the spell: fall back to
  `SpellInfo::Effects[i].CalcRadius()` when `GetRadius()` is 0.
- **Friend/foe flag.** Sixth field on the hazard row, from the dynobject caster's reaction to the
  roster. Today 63% of hazard rows are friendly bot AoE reading as threats.
- **Hostile creature sweep.** Extend the snapshot sweep to pick up non-friendly creatures near the
  roster and emit them as ordinary unit rows, so hazard units that never enter combat — Hodir's
  icicles are `Creature`s, see `IsHodirIcicleLethal` — appear positionally. Cap with the existing
  `OBS_MAX_WATCHED` (40) so a trash-heavy pull cannot blow the row count up.

### Minors

- **`end` outcome.** [RaidObs.cpp:892](src/Bot/Obs/RaidObs.cpp#L892) maps `NOT_STARTED` → `reset`
  literally, so a 23/24 wipe with the boss at 67.8% was filed as `reset`. At close, if most of the
  roster is dead, write `wipe` regardless of boss state.
- **Snapshot interval.** [RaidObs.cpp:1117](src/Bot/Obs/RaidObs.cpp#L1117) sets
  `s.sinceSnapshotMs = 0` instead of subtracting the interval, so 250 ms configured lands at 314 ms
  actual. Subtract, and clamp so a long tick cannot bank credit.
- **Cross-encounter note leak.** `ThorimEncounterState::squadsAssigned` reset
  ([UldEncounter_Thorim.cpp:1121](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L1121)) fired during a
  Hodir pull. Two records; harmless. Add the note's key prefix to the record so the analyzer can
  filter rather than suppressing it — Iron Assembly legitimately spans three bosses under one slug.

## Schema v4

Bump `SCHEMA_VERSION` at [RaidObs.h:35](src/Bot/Obs/RaidObs.h#L35) and update the table in
[docs/systems/observability.md](docs/systems/observability.md).

| Record | Change |
|---|---|
| `aura` | `+st` (stacks), `+dur` (ms remaining) |
| `death.auras` | rows become `[sp, stacks, dur, caster, appliedT, removedT]`; `removedT` −1 while held |
| `death.acts` | rows become `[firstT, lastT, action, rel, verdict, repeats]` |
| `cast` | unchanged shape; scoped to roster + pets + watched |
| `snap.hz` | `+foe` flag; radius now falls back to spell radius |
| `snap.u` | may include swept hostile creatures |
| `note` | `+k` prefix retained for filtering |
| `end` | `out` may now be `wipe` where v3 wrote `reset` |

## Analyzer

`tools/botobs/postmortem.py` already refuses mismatched schema versions, so it must move to v4 in
the same change. Death block gains a **debuffs held at death** section (spell, stacks, duration,
how long held) — the payoff for Fixes 1 and 2, and the first thing to read on any death. `--bot`
renders the RLE'd tick rows as `action verdict ×N`. `--notes` picks up the `hodir.*` keys.

## Verification

No headless build path here, so: static checks are mine, the pull is yours. Baseline to beat is the
current file — every number below is measured from it.

1. Build, then `docker exec ac-worldserver env | grep ^AC_AI_PLAYERBOT_OBS` — confirm no override is
   shadowing the conf (none today).
2. Pull Hodir. New trace appears in `env/dist/logs/botobs/`.
3. Compare against `603_3_hodir_1787435701.ndjson`:

| Check | v3 baseline | Expected v4 |
|---|---|---|
| `grep -c '"e":"act"'` | 156,701 | ~15,000 |
| `grep -c '"e":"veto"'` | 25,412 | ~200 |
| file size | 19.7 MB | ~5 MB |
| Hodir debuffs in death records | **0 of 23** | most deaths carry Biting Cold with a stack count |
| `cast` records from non-roster, non-watched casters | ~2,000 | 0 |
| `cast` records from roster | 0 | non-zero |
| `hz` rows with radius 0 | 2,484 of 6,720 (all 3 Icicle spells) | 0 |
| `hz` rows flagged friendly | n/a — no flag | ~63% |
| `note` records | 2 (both Thorim) | `hodir.*` keys across the roster |
| snapshot interval | 314 ms | ~250 ms |
| `end.out` on a wipe | `reset` | `wipe` |

4. `python tools/botobs/postmortem.py <file> --death 1` — the block should name the killing debuff
   and its stack count without cross-referencing anything else.
5. Confirm worldserver CPU is unchanged against `Obs.Enabled = 0`. The ~1000 open-world bots gain one
   `BeginTick` call per `DoNextAction`, gated on `Active()`, so idle cost must not move.

Real acceptance test: hand me the new file and a wipe, and see whether the death blocks alone explain
it.

## Out of scope

- Instrumenting the other Ulduar encounters (Leviathan, XT, Ignis, Razorscale, Kologarn, Auriaya,
  Freya, Mimiron, Yogg). Same derived-probe pattern when each gets raided.
- ICC / SWP / Naxx containers still deliberately bare (ordered-map and default-constructible
  variants needed).
- HTML pull view for the analyzer.

## Housekeeping

Plan mode blocks writes outside this file. On approval, first copy this into
`docs/plans/raid-observability/` alongside the existing `raid-observability.PLAN.md` — as a second
document, not a rewrite; that one records what shipped.

## Implementation status

All six fixes and the three minors are in the tree, schema bumped to v4, `postmortem.py` and
`docs/systems/observability.md` moved with it. Nothing is compiled — there is no headless build path
here, so every check below the C++ line is static.

**Fix 3 landed as a merge, not as written.** While this plan was being written, a per-action latch with
a 10 s repeat heartbeat (`OBS_ACTION_REPEAT_MS`) was added to `NoteAction` in the working tree. The tick
model replaces its dedup — the buffer compares whole passes, which the latch could not — but keeps its
heartbeat: a pass whose signature has been repeating for longer than `OBS_ACTION_REPEAT_MS` is written
out again, so a bot stuck in one state for minutes still leaves a trail instead of a gap. `NoteVeto`
gained dedup for the first time, through the same buffer.

`postmortem.py` reads both `death.acts` shapes, so a v4 run can be compared against
`603_3_hodir_1787435701.ndjson` without converting it.

### Fix 5 correction — two probes walked into the trap the change was fixing

`hodir.tankswap` was written as an unlatched `Note`, one record per `Execute`, on the reasoning that a
swap is an event worth a timestamp. Wrong on two counts:

- The act stream already carries it. A landed taunt is an `OK` verdict on
  `hodir frozen blows swap action`, a failed one a `FAILED`, and v4 run-length encodes both. The note
  re-added, uncompressed, exactly what the same change had just compressed.
- All four class taunts share an 8 s cooldown (`RecoveryTime`/`CategoryRecoveryTime` 8000, verified in
  `spell.reference.csv` for 355 / 6795 / 56222 / 62124), and `HodirFrozenBlowsSwapTrigger` stays active
  until `boss->GetVictim() == bot`. A swap that has to wait for the cooldown therefore retries at tick
  rate — measured at ~105 ms per pass — until it lands.

The probe is removed, with a comment saying why so it is not re-added.

The same defect was in the position probes, and was the worse of the two: `hodir.centre`, `hodir.slot`,
`hodir.shuttle` and `hodir.anchor` latched positions formatted to two decimals. `GetHodirRingSlot` ends
in `CheckCollisionAndGetValidCoords`, which takes the bot's *current* position as the ray origin, and
`GetHodirShuttleLeg` derives a non-tank's leg from the surrounding crowd — both drift every tick, so the
latch never held and every one of ~20 bots emitted at tick rate. `RaidObs::DescribeDerived` now rounds a
derived position to the yard; `DescribeAssignment` keeps full precision for stored assignments, which do
not drift.

## v4 measured — `603_3_ulduar_1787492421.ndjson`

402 s whole-instance `mark` session, 24 bots, 29.2 MB, 256,197 lines, 0 unparsable, 26 deaths. Not
line-for-line comparable to the Hodir-only v3 baseline, which covered less.

| Check | v3 | v4 | |
|---|---|---|---|
| Hodir debuffs in death records | 0 of 23 | 26 of 26 carry Biting Cold `62038` from Hodir | pass |
| `aura` with `st`/`dur` | none | 20,190 of 20,190 | pass |
| `cast` from roster | 0 | 13,078 (+4,018 creature, all watched; +2,025 pet; 0 leaks) | pass |
| units per snapshot | 24 roster | 69, 152 distinct creature guids swept | pass |
| `hz` foe flag | none | 5,233 foe / 7,058 friendly | pass |
| snapshot interval | 314 ms | mean 254 ms against 250 configured | pass |
| `end.out` on a wipe | `reset` | `wipe` | pass |
| `act` | 156,701 | 146,779 | **fail** |
| `veto` | 25,412 | 26,848 | **fail** |
| `hz` rows with radius 0 | 2,484 | 5,796 | **moot, see below** |

### Fix 3 correction — the tick model is the wrong grain on its own

The plan assumed a pass "repeats verbatim". It does not: **0.5%** of consecutive passes match their
predecessor (2.2% ignoring verdict, 0.9% unordered). Relevance drifts — `mind blast` at 0.53 then 5.30
between two passes 211 ms apart — reorder the priority queue, so the ordered set changes almost every
tick even though the individual verdicts hold for seconds.

A per-action latch would have cut `act` by 86.3% and `veto` by 100% on the same data. Both grains now
run: `ShouldEmit` latches per action for the stream, the pass compare stays for `death.acts`' `repeats`.
Replaying the trace through the shipped gate gives `act` 146,779 → 27,934 (−81.0%), `veto` 26,848 →
2,515 (−90.6%), file 29.2 → 17.6 MB. Short of the naive figures because the 10 s heartbeat still
re-emits, which is the trail-not-gap property.

`death.acts` is unchanged at ~342 rows per record (1.6% of the file) — the pass compare rarely fires, so
nothing collapses it.

### Fix 6 correction — the radius fallback has nothing to fall back to

`62234`, `62236` and `62462` are `SPELL_ICICLE_VISUAL_UNPACKED`, `ICICLE_FALL_EFFECT_UNPACKED` and
`SPELL_ICICLE_VISUAL_PACKED` — visuals force-cast on the selected player, which is also why `62462`
reads `foe=0` (correctly: the player really is its caster). All three carry `EffectRadiusIndex` 36,
which is `0,0,0` in `spellradius.reference.csv`, so `CalcRadius` returns 0 and always will.

The lethal thing is the creature: the death records name `Snowpacked Icicle` and `Ice Shards` `65370`
for 13,438 a hit. The creature sweep is what delivered Fix 6; the radius fallback is correct but inert
here. 5,796 of 12,291 hazard rows are unusable zero-radius markers, and `postmortem.py`'s `STOOD IN`
line consequently never fires.

### Fix 5 correction, second pass

`hodir.breaker` and `hodir.shuttle` flapped anyway — 384 and 121 notes per bot. `IsHodirTrappedAllyBreaker`
is asked about every block in range and answers true for several in one pass, so no latch can hold; the
block the bot goes for is already in `hodir.dpstarget`, and the probe is removed. `GetHodirShuttleLeg`
now records which branch produced the leg (`tank`/`solo`/`crowd`/`none`) rather than the coordinate,
which the `move` stream already carries under `by`. The other five probes are healthy: `hodir.centre`
2.6 notes per bot, `anchor` 9.5, `shelter` 9.7, `slot` 15, `dpstarget` 26.

### Still open

- Not compiled. No headless build path here.
- Re-pull Hodir and confirm `act` lands near 28,000.
- `postmortem.py` renders an aura stripped by `Unit::Kill` as "fell off 0.0s before"; it was held to
  death. Its `is_debuff` test (caster tag ≠ player) is wrong both ways — it shows totem and pet buffs as
  debuffs, and hides Biting Cold `62039`, which the player self-casts through the trigger.
- `docs/plans/hodir-anchor-dodge-thrash/` tells its reader to check `hodir.breaker`, which no longer
  exists.
