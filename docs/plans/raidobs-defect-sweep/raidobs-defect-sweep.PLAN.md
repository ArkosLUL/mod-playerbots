# RaidObs defect sweep (schema v4 → v5)

## Context

Schema v4 shipped and was measured against a real Hodir wipe:
`env/dist/logs/botobs/603_3_hodir_1787582677.ndjson` — 20.4 MB, 147,274 lines, 24 bots, 415 s,
26 deaths, `out:"wipe"`. All six v4 gap fixes landed. Against the v3 baseline
(`603_3_hodir_1787435701.ndjson`), normalised per second because the pulls differ in length:

| | v3 | v4 |
|---|---|---|
| `act` /s | 663.9 | 88.0 (−87%) |
| `veto` /s | 107.7 | 7.6 (−93%) |
| bytes /s | 83.6 KB | 45.8 KB (−45%) |
| deaths carrying Biting Cold | 0/23 | 26/26 |
| roster casts | 0 | 21,896, zero non-roster leaks |
| units per snapshot | 24 | 62.8 |
| `end.out` on a wipe | `reset` | `wipe` |

The trace now diagnoses the wipe: 23 of 26 deaths are Hodir melee plus **Frozen Blows `63511`**
landing in the same millisecond for 19k–35k, and Tree died to its own **Biting Cold `62188`**
ticking 8,939 → 7,946 → 9,933 at 6 stacks.

Nine defects survive that audit, split across the recorder, one unrelated strategy bug it exposed,
and the analyzer. This plan fixes all nine.

**Decided with the user:**

- Thorim: fix the underlying loop, not just the note leak.
- Snapshots: **leave whole.** 71.8% of unit rows repeat their predecessor and a delta-plus-keyframe
  scheme would cut ~19% off the file, but a snapshot stays a complete standalone world state. The
  36.2% snapshot share is accepted, not a defect. No work in this plan.

---

## A. Recorder — `src/Bot/Obs/RaidObs.cpp`, `RaidObs.h`, `RaidObsScripts.cpp`

### A1. Spell-name dictionary (defect 3)

Every death block reads `spell 63511`. Nothing in the trace says that is Frozen Blows, so a fresh
session cannot diagnose a death without a DBC cross-reference — which breaks the premise that the
file is self-contained.

Mirror the existing `unit` name-record pattern: a `spell` record written once per distinct id.

```
{"e":"spell","sp":63511,"n":"Frozen Blows"}
```

Add `std::unordered_set<uint32> spellsSeen` to `ObsSession` beside `watched`
([RaidObs.cpp:321-323](src/Bot/Obs/RaidObs.cpp#L321)) and an `EnsureSpell(ObsSession&, uint32)`
shaped like `EnsureUnit` ([RaidObs.cpp:~470](src/Bot/Obs/RaidObs.cpp#L470)). Name comes from
`SpellInfo::SpellName[LOCALE_enUS]` ([SpellInfo.h:407](src/server/game/Spells/SpellInfo.h#L407)).

Call it from `NoteAura`, `NoteDamage`, `NoteHeal`, `NoteAbsorb`, `NoteCast`, the hazard loop in
`SweepArea`, and the aura/rewind builders in `NoteDeath`.

Measured cost on this trace: **764 distinct spells ≈ 34 KB**, 0.17% of the file.

### A2. Aura positivity from the server, not a guid guess (defect 4)

`is_debuff` in the analyzer tests the caster's guid tag, which is wrong in both directions: totem and
pet buffs read as debuffs, and Biting Cold `62039` is invisible because the *player* self-casts it
through the trigger. Tree's 6 stacks — the thing that killed it — never appear in its debuff list.

The guid can never answer this. Emit the truth instead: add `p` (1 = positive) to the `aura` record
from `SpellInfo::IsPositive()` ([SpellInfo.h:486](src/server/game/Spells/SpellInfo.h#L486)), carry
it in `AuraState`, and append it as a **7th column** on `death.auras`.

### A3. Rewind ring: trim on read, order by time (defect 1)

`NoteDamage` trims the ring only on push ([RaidObs.cpp:1446](src/Bot/Obs/RaidObs.cpp#L1446)), so a
bot that takes no damage keeps arbitrarily old rows. Nightwarrior's record reports
*"took 17485 over the last 10 hits"* from damage **71 s** before it died. `NoteDeath` then sorts by
amount ([RaidObs.cpp:1873](src/Bot/Obs/RaidObs.cpp#L1873)), so the killing blow is not first either.

- In `NoteDeath`, prune entries older than `g_cfg.deathRewindMs` before building `rewind`.
- Sort chronologically, oldest first. The analyzer does its own amount ranking for the share table.

An empty ring then means "nothing hit this bot in 15 s", which is a fact worth reading, not a gap.

### A4. Killing blow via the one damage funnel every path uses (defect 2)

Nightwarrior sat frozen at (1990.1, −164.2, 432.8) — outside the room — at 100% HP for 16 s, then
went 100% → 0% in a single snapshot step with no damage recorded at all.

Cause: every damage hook in
[RaidObsScripts.cpp:55-62](src/Bot/Obs/RaidObsScripts.cpp#L55) is an `OnSend*Log` hook, so the
recorder only sees damage that produced a combat-log packet. Environmental damage, script kills, and
direct `DealDamage` calls are invisible.

`sScriptMgr->OnDamage(attacker, victim, damage)` sits inside `Unit::DealDamage`
([Unit.cpp:999](src/server/game/Entities/Unit/Unit.cpp#L999)) — the single funnel all of them pass
through.

Add `UNITHOOK_ON_DAMAGE` and an `OnDamage` override, but **do not emit `dmg` records from it** —
that would double-count everything the log hooks already cover, and `OnDamage` fires first so it
cannot be de-duplicated forward. Record only the killing blow:

- when `damage >= victim->GetHealth()`, store `{source, amount}` in a single `BotTrace::killBlow`
  slot;
- `NoteDeath` emits it as `"blow":[src,amount]` and clears it.

One row per death, no de-dup problem, and it names the attacker in exactly the case the log hooks
miss.

Also stash the bot's last snapshot HP and its timestamp in `BotTrace` from the roster loop in
`BuildSnapshotPayload` ([RaidObs.cpp:~700](src/Bot/Obs/RaidObs.cpp#L700)), emitted as
`"hplast":[pct,t]`. If `blow` still comes back empty, the record then states the useful fact
directly: 100% at t−0.05 s, dead at t, nothing recorded.

### A5. Hazard side from the caster guid, not the anchor (defect 7)

`anchor->IsHostileTo(caster)` ([RaidObs.cpp:~669](src/Bot/Obs/RaidObs.cpp#L669)) has two problems:
`anchor` is whichever roster player map iteration reached first, and a null caster falls into the
hostile branch. The same dynobject therefore flips side between snapshots — Starlight 8 rows,
Toasty Fire 17, Death and Decay 9, Blizzard 11.

Replace with a rule keyed on `dyn->GetCasterGUID()`, which is always valid:

- caster guid is a player → friendly;
- caster resolves live and its owner is a player on the map → friendly (pets, totems);
- otherwise → hostile.

No null branch, no anchor dependency, and the answer cannot change while the dynobject lives. Icicle
`62462` keeps reading friendly — correct, it is force-cast *on* the selected player.

### A6. Schema bump

`SCHEMA_VERSION` 4 → 5 at [RaidObs.h:35](src/Bot/Obs/RaidObs.h#L35).

| Record | v5 change |
|---|---|
| `spell` | **new** — `{sp, n}`, once per distinct spell id |
| `aura` | `+p` (1 = positive) |
| `death.auras` | 7th column `p` |
| `death.rewind` | time-trimmed, chronological |
| `death` | `+blow` `[src,amount]`, `+hplast` `[pct,t]` |
| `snap.hz` | `foe` now derived from the caster guid |

---

## B. Thorim thrash — `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`, `UldBossHelper.h`

Defect 9 is a strategy bug the trace exposed, not log noise. `thorim.squad` fired 4,264 times across
all 24 bots during a Hodir pull, plus `squadsassigned` 774 and `arenaarrived` 392 — 66% of every
note in the file.

Two independent causes, both need fixing:

**B1. Staleness has no engagement latch.** `ThorimEncounterStateIsStale`
([UldEncounter_Thorim.cpp:1064](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L1064)) returns true
whenever Thorim is at full health and out of combat — which is also true *before* the pull.
`AssignThorimSquads` deliberately runs pre-pull ("the corridor squad forms up at the gate before the
pull", [UldEncounter_Thorim.cpp:147](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L147)), so the
reset trigger clears the squads and the next tick reassigns all 24. The loop runs at Thorim too, not
only at Hodir.

Add `bool engagedSeen = false` to `ThorimEncounterState`, set it when Thorim is in combat or below
max health, clear it in `ResetThorimEncounterState`, and require it in `ThorimEncounterStateIsStale`.
Staleness then means "the encounter reset after we had engaged", which is what the reset action is
for.

**B2. Hodir's room is inside Thorim's proximity radius.** `ULDUAR_THORIM_ENCOUNTER_PROXIMITY` is
200 yd ([UldBossHelper.h:1554](src/Ai/Raid/Uld/Util/UldBossHelper.h#L1554)) and Hodir's fight sits
136–176 yd from `ULDUAR_THORIM_NEAR_ARENA_CENTER`, so `GetThorimSquad` runs during a Hodir pull.

A tighter radius does not separate them: the farthest gauntlet waypoint is 126.1 yd and Hodir's
melee spot is 136.2 yd, a 10 yd margin. **Use height instead** — all values below are already
committed constants or measured from this trace, none invented:

| | z |
|---|---|
| Thorim gauntlet | 412.1 |
| Thorim arena floor | 419.8 |
| Hodir room floor | 432.7 |

Add `ULDUAR_THORIM_WING_MAX_Z = 425.0f` beside the proximity constant and fold both tests into one
file-local `NearThorimEncounter(bot)` in
[UldEncounter_Thorim.cpp](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp), replacing all five open-coded
proximity gates there. The three `110.0f` gates in
[UldTriggers_Thorim.cpp](src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp) already sit inside Hodir's
nearest approach (136 yd) and need no change.

Note the gate is tested against the **bot's** position, never Thorim's, so his balcony height is
irrelevant.

---

## C. Analyzer — `tools/botobs/postmortem.py`

Accept schema **4 or 5** rather than one version, so the v4 traces already on disk still read.
`act_row` already handles two shapes; follow it.

| # | Defect | Fix |
|---|---|---|
| 3 | every spell is a bare number | build `self.spells` from the new `spell` records alongside `self.names`; render `Frozen Blows (63511)` everywhere a spell id is printed |
| 4 | `is_debuff` guesses from the caster guid | partition on the new `p` column; keep the guid heuristic only as the fallback for a v4 trace |
| 5 | `"fell off 0.0s before"` on auras held to death | `Unit::Kill` strips auras before the death hook, so `removedT` lands on the death timestamp. In `aura_line`, render `held to death` when `death_t - removed <= 250`, and keep the real elapsed figure otherwise |
| 6 | header says `debuffs (9)`, list prints 3 oldest | sort held-at-death first then most-recently-applied, raise the brief cap to 5, and print `debuffs (5 of 9 shown)` so the count matches what is on screen |
| 1 | `took X over the last N hits` from a stale ring | print the last three hits chronologically first, then the amount-ranked share table; say `no damage recorded in the last 15s` when the ring is empty |
| 2 | a death with no damage looks like a gap | render `blow` and `hplast` — `100% at t-0.05s, killed by <src> for <amount>`, or the same line ending `nothing recorded` |
| 8 | `STOOD IN` can never fire | see below |

**Defect 8 in detail.** `hazards_at` skips any row that is friendly *or* zero-radius
([postmortem.py:148](tools/botobs/postmortem.py#L148)). On Hodir that discards everything: all 5,281
hostile-flagged rows are the Icicle visuals `62234`/`62236`/`62462`, whose `EffectRadiusIndex_2` row
in `spellradius.reference.csv` is `36,0,0,0`, so `CalcRadius` returns 0 and always will. Every row
that *has* a radius is friendly. The lethal thing is the `Snowpacked Icicle` **creature** casting
Ice Shards, which the v4 sweep already puts in `snap.u`.

Report three things from the last snapshot before the death:

- **STOOD IN** — hostile rows with `radius > 0` whose radius covers the spot (unchanged rule, now
  reachable on other bosses).
- **NEAR** — hostile rows with `radius == 0` within 5 yd. A zero-radius row is an impact marker, not
  an area; label it as one rather than dropping it.
- **hostile units within 5 yd**, from the same snapshot's `u` rows, filtered to swept creatures. This
  is what actually answers "did an icicle land on me".

Also print the friendly zones covering the spot, labelled as such. Toasty Fire sits in the stream at
radius 11 for the whole fight and `hodir biting cold shed` failed 907 times — whether a bot was
standing in one is the diagnosis.

---

## D. Docs — `docs/systems/observability.md`

Update the schema table and record the two new rules: **name every id in the trace itself** (A1) and
**derive side from the caster guid, never from a sampled anchor** (A5). Also correct the hazard
paragraph — the analyzer now reads zero-radius rows as markers instead of discarding them.

Per the standing rule, run `/compact-docs-writer` **before** editing this file, as the first step of
the doc task rather than a cleanup afterwards.

---

## Verification

Static checks are mine; the build and the pull are yours — there is no headless build path here.

1. Build the module.
2. `docker exec ac-worldserver env | grep ^AC_` — confirm nothing shadows the obs config.
3. Pull Hodir. Compare the new trace against
   `603_3_hodir_1787582677.ndjson` with these commands:

| Check | v4 measured | Expected v5 |
|---|---|---|
| `grep -c '"e":"spell"'` | 0 | ~764 |
| `grep -c '"e":"aura"'` rows carrying `"p":` | 0 | all |
| `hz` rows flagged hostile that are not Icicle visuals | 74 | ~0 flapping |
| `note` records with a `thorim.` prefix | 5,430 | **0** |
| `note` records total | 8,194 | ~2,764 |
| deaths whose newest rewind row is >15 s old | 1 | 0 |
| `snap` share of file | 36.2% | unchanged, by decision |

4. `python tools/botobs/postmortem.py <file>` — every death block must name spells in words, list
   Biting Cold among the debuffs on the bots that held it, say `held to death` rather than
   `fell off 0.0s before`, and print a debuff count that matches the rows shown.
5. `python tools/botobs/postmortem.py <file> --death N` on a Frozen Blows death — the `STOOD IN` /
   `NEAR` / nearby-hostile lines must render without crashing on a trace that has no qualifying rows.
6. Re-run the analyzer against the **existing v4 trace** to confirm the version fallback still reads
   it.
7. Pull Thorim once. Confirm the squad split still forms at the gate before the pull and that
   `thorim.squad` settles rather than flapping — one assignment per bot, not 178 rounds of 24.

## Housekeeping

On approval, copy this document to
`docs/plans/raidobs-defect-sweep/raidobs-defect-sweep.PLAN.md` before any implementation work.

## Out of scope

- Delta-encoded snapshots (decided against above).
- The gameplay findings the trace surfaced: Frozen Blows killing ranged and healers, `hodir icicle
  dodge action` failing 48% of the time, `hodir biting cold shed` failing 54%, and Nightwarrior
  parked outside the room issuing `follow` moves that never moved it. Those are strategy work, and
  this plan only makes them legible.
