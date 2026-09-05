# RaidObs audit, 2026-09-04/05: name the pull, sweep the snapshot

## Context

`RaidObs` is a per-instance NDJSON session recorder for playerbot raid pulls. Its premise: hand a
fresh agent session a trace file path and it can diagnose why a specific bot died, without video.
Recorder in `src/Bot/Obs/` (split into six files by concern on 2026-09-01, commit `2b846b142`),
analyzer in `tools/botobs/` (split the same day, `f12d9309b`), schema in
`docs/systems/observability.md`.

23 traces were recorded on 2026-09-04 (4) and 2026-09-05 (19), all Ulduar: Thorim ×9, Freya's Elder
Stonebark ×12, General Vezax ×1, and one filed as `ulduar`. All are schema v10.

**The framework collects correctly.** Across all 23:

| Check | Result |
|---|---|
| roster members at 0 hp in the final snapshot with no `death` record | **0** of 515 deaths |
| `death.hplast` present | 515/515 |
| `move.pr` present | 71 900/71 900 (100%) |
| `move` `wait` rows carrying both `hpr` and `hms` | 28 521/28 521 (100%) |
| unparsable lines / `truncated` traces | 0 / 0 |
| snapshot cadence (p50 / p95) vs 250 ms interval | 224–303 ms / ~325 ms |
| analyzer views (default, `--notes`, `--stalls`, `--clump`, `--death`) | all exit 0 |

The 46 deaths missing `death.acts` / `death.lastmove` are **44 human players** (no engine pass, no
bot-issued move) plus 2 bots — not a gap.

**All three 2026-08-31 fixes are confirmed live.**

- **`cause`**: 204 of 515 deaths are blow-less self-kills, and 204/204 carry `cause:"reset"`. The
  invariant behind it (`killer == victim` ⟺ no `blow`) holds with zero violations.
- **Ulduar trigger gate**: foreign `note` keys dropped from 7 prefixes / 621 rows on 2026-08-31 to
  **one** (`vezax.*`, 477 rows), and 472 of those fire before t=2 s — the deliberate
  nothing-IN_PROGRESS window the gate leaves open between pulls, one row per bot, written once
  because `NoteDerived` is write-on-change. **Zero** foreign boss-named actions executed anywhere;
  the 20 `flame leviathan enter vehicle` acts that ran mid-Kologarn are gone.
- **Rename**: never fired, in any trace on disk. See Defect 2.

**v10 pets work as documented.** 151 252 pet snapshot rows, `own` on every one, `dealt` 0 on all of
them so a window difference cannot double-count. Sampled: Army of the Dead Ghoul, Mirror Image,
Spirit Wolf, Shadowfiend, Treant, Ebon Gargoyle, Greater Fire Elemental, hunter pets by name. Skipped:
every totem, as designed.

**Cost is flat.** Same boss, v7 vs v10: `elder-stonebark_1788201407` 43.9 KB/s and
`elder-stonebark_1788200810` 44.7 → `elder-stonebark_1788610461` 47.3 and `_1788613108` 47.0. Pets
are 6.2–7.2% of a file. The 31.6–39.3 KB/s range quoted in the 2026-08-31 audit was encounter mix
(Kologarn, Auriaya, Mimiron), not a cheaper framework. Largest trace 21 MB against `MaxFileMB` 256;
directory 570 MB against `MaxDirMB` 5120.

Two defects, and doc drift.

---

## Defect 1 — the snapshot's casting spell is never named

`UnitRow` ([RaidObsSnapshot.cpp:33](src/Bot/Obs/RaidObsSnapshot.cpp#L33)) walks
`GetCurrentSpell` and writes the id into column 10 of every `snap.u` row. Nothing calls
`EnsureSpell` on it — `UnitRow` is a free function with no session handle, which is exactly how it
was missed.

99 bare ids across 14 of the 23 traces. Cross-referencing every trace on disk names some of them and
leaves three that no trace has ever named:

| id | name, if any trace ever wrote one |
|---|---|
| 62444, 62417, 16496, 57807 | Heroic Strike, Sweep, Shoot, Sunder Armor — Thorim arena trash |
| 47884, 2366 | Create Soulstone, Herb Gathering — bot casts during the pre-roll |
| 61964, 61965, 62942 | **never named in any trace on disk** |

Volume is low because most casters also produce a `cast` record, which does sweep. What is left is
precisely the case the column exists for: a swept creature that `NoteCast` filtered out as
irrelevant, and pre-roll rows written before any session exists. This is the doc's own load-bearing
rule (*"Call `EnsureUnit`/`EnsureSpell` from every field that emits an id"*, and *"a new emitter
needs the sweep as much as a new field"*) failing again.

### Fix

Collect the ids in `UnitRow` through an out-param and sweep them at the two call sites:

- `BuildSnapshotPayload` ([RaidObsSnapshot.cpp:187](src/Bot/Obs/RaidObsSnapshot.cpp#L187)) already
  takes `ObsSession* session`. Non-null → `session->EnsureSpell(id)` inline, the same shape as the
  `EnsureUnit(creature)` calls already in that sweep at lines 173 and 320.
- The pre-roll path passes `nullptr` ([RaidObsLifecycle.cpp:301](src/Bot/Obs/RaidObsLifecycle.cpp#L301)).
  Add `std::vector<uint32> spells` to `PreRollEntry`
  ([RaidObsSession.h:172](src/Bot/Obs/RaidObsSession.h#L172)) and sweep them in the drain loop
  ([RaidObsLifecycle.cpp:135](src/Bot/Obs/RaidObsLifecycle.cpp#L135)) before each payload is emitted.
  `spell` records are a lookup table the analyzer builds on load, so emitting them at drain time
  rather than at sample time changes nothing for a reader.

Update the `UnitRow` declaration at [RaidObsSession.h:312](src/Bot/Obs/RaidObsSession.h#L312).

**No schema bump.** No field or value changes; the trace just gains `spell` records it should always
have had.

---

## Defect 2 — a `bossstate` trace still cannot name itself, and the v9 rename cannot reach it

`603_1_ulduar_1788553442` (2026-09-04, 77.8 s, 25 deaths, `wipe`) is a **Yogg-Saron** pull. The
evidence is unambiguous inside the trace: `Sanity 63050 x100 from Voice of Yogg-Saron` on the one
non-reset death, and `Guardian of Yogg-Saron` at 101 yd. It is filed as `ulduar`, with **General
Vezax** swept into `snap.u` 42.2 s in and flagged `b:1` — the only boss the file names, and the wrong
one.

Why the v9 fix could not help:

- `OpenSession` came from `bossstate`. `ProcessPendingBossState`
  ([RaidObsLifecycle.cpp:233](src/Bot/Obs/RaidObsLifecycle.cpp#L233)) reads `GetBossState(bossId)`
  and then calls `OpenSession(map, FindEngagedBoss(map), "bossstate")` — **it has the encounter id
  in its hand and drops it**. `FindEngagedBoss` scans `getAttackers()` for a boss, finds none, and
  `ResolveBossName(map, nullptr)` returns the map name.
- `UpgradeBossName` is only ever called from `OnCreatureEngage`
  ([RaidObsLifecycle.cpp:461](src/Bot/Obs/RaidObsLifecycle.cpp#L461)), which needs a boss to enter
  combat **with a player**. In Yogg-Saron phase one nothing that swings at the raid is a boss —
  Sara, the Voice and the Guardians do the work and none is `isWorldBoss`/`IsDungeonBoss`. So the
  rename path never fires. Across every trace on disk, `src:"rename"` appears **zero** times.
- `MarkPull` would have fixed it, and cannot: `OpenSession` returns early when a session already
  exists ([RaidObsLifecycle.cpp:77](src/Bot/Obs/RaidObsLifecycle.cpp#L77)), so `MarkPull` on an open
  session is a silent no-op. It also has only two callers in the whole module — Flame Leviathan
  ([UldEncounter_FlameLeviathan.cpp:107](src/Ai/Raid/Uld/Util/UldEncounter_FlameLeviathan.cpp#L107))
  and Thorim ([UldEncounter_Thorim.cpp:604](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L604)).

The cost is the same as before: the filename and `hdr.boss` are all anyone has to pick a trace by,
and a Yogg-Saron wipe filed as `ulduar` is invisible to anyone looking for one.

### Fix, part 1 — make the rename reachable

Split `UpgradeBossName` ([RaidObsLifecycle.cpp:385](src/Bot/Obs/RaidObsLifecycle.cpp#L385)) so the
name can arrive as a string, keeping the `Creature*` overload the engage hook uses:

```cpp
void ObsSession::UpgradeBossName(Creature* boss);          // resolves, then calls the below
void ObsSession::UpgradeBossName(std::string const& slug);  // the existing body, guards unchanged
```

The guards stay exactly as they are: only a session still carrying `SlugOf(map->GetMapName())` is
ever renamed, so a correctly named trace is never touched and a repeat call costs one string compare.

Then:

- `MarkPull` ([RaidObsLifecycle.cpp:467](src/Bot/Obs/RaidObsLifecycle.cpp#L467)) renames when a
  session is already open instead of returning nothing.
- New entry point beside it, declared in [RaidObs.h:62](src/Bot/Obs/RaidObs.h#L62) next to
  `MarkPull`, for the case where the strategy knows the encounter but has no boss unit to point at:

```cpp
// Name a pull the engage hook cannot: an encounter whose boss never swings at a player in the phase
// the raid dies in. Yogg-Saron's phase one is all Sara, the Voice and Guardians, none of them a boss,
// so the trace stays filed under the map name. Only ever upgrades that fallback.
void NamePull(Map* map, char const* bossName);
```

`char const*`, not `std::string` — the doc's own constraint about `Active()` gating and names built
before the gate can skip them applies to any caller on a bot path.

### Fix, part 2 — give it a caller that covers all 14 encounters

The per-encounter route the two existing `MarkPull` calls use does not generalise cheaply: the
Yogg-Saron helper is three phase functions with no state struct and no scan
([UldEncounter_YoggSaron.cpp](src/Ai/Raid/Uld/Util/UldEncounter_YoggSaron.cpp)), and the Ulduar
strategies have no shared per-tick hook to hang one on. Twelve new scans is the wrong shape.

`UldGatedTrigger` already wraps all 165 triggers and already carries each one's `bossId`
([UldEncounterGate.cpp:98](src/Ai/Raid/Uld/UldEncounterGate.cpp#L98)). A trigger that **fires** is
proof of which encounter this is. Name the pull there:

```cpp
Event UldGatedTrigger::Check()
{
    if (!inner || !UldEncounterGateOpen(botAI, bossId))
        return Event();

    Event event = inner->Check();

    // A trigger that fires while its own encounter is live is the one thing that can name a pull the
    // engage hook missed. IN_PROGRESS rather than the gate's weaker test on purpose: with nothing
    // engaged every trigger is open, and a Vezax trigger firing in that window must not name a
    // Yogg-Saron pull after Vezax.
    if (event && RaidObs::Active() && UldEncounterIsLive(botAI, bossId))
        RaidObs::NamePull(botAI->GetBot()->GetMap(), UldEncounterName(bossId));

    return event;
}
```

Two small additions to [UldEncounterGate.cpp](src/Ai/Raid/Uld/UldEncounterGate.cpp):

- `bool UldEncounterIsLive(PlayerbotAI*, uint32 bossId)` — `GetBossState(bossId) == IN_PROGRESS`,
  the same `InstanceScript` lookup `UldEncounterGateOpen` already does.
- `char const* UldEncounterName(uint32 bossId)` — reads `ENCOUNTER_PREFIXES`
  ([UldEncounterGate.cpp:24](src/Ai/Raid/Uld/UldEncounterGate.cpp#L24)) backwards, first prefix per
  id. That table already covers all 14 encounters and its entries are usable slugs
  (`flame leviathan`, `iron assembly`, `yogg-saron`, `thorim`, `freya`, `vezax`). `sara` also maps to
  `ULD_BOSS_YOGGSARON`, so taking the first match yields `yogg-saron`.

Note the naming direction this introduces: a Freya pull renamed by this path becomes `freya`, not
`elder-stonebark`. That only ever replaces the map name, so nothing regresses, and `freya` beats
`ulduar`.

Cost when nothing is recording: one `Active()` bool. With a trace open: one `FindSession` (per-map-
thread memo) and one string compare per firing trigger, and only on the ticks a boss trigger actually
fires — not on every check.

**No schema bump.** `src:"rename"` and the `was` field are already v9; this only makes them reachable.

---

## Doc drift

- Heading reads `## Schema (v: 9)` ([docs/systems/observability.md:77](docs/systems/observability.md#L77))
  while `RaidObs.h:35` is `SCHEMA_VERSION = 10`. The v10 body text (pets, `own`, `dealt`) is already
  there — the heading was missed when the bump rode in on `caa8cf692`, a Thorim strategy commit.
- The schema-bump paragraph still says *"`SUPPORTED_SCHEMA` in `postmortem.py`"*. Both
  `SUPPORTED_SCHEMA` and `READABLE_SCHEMAS` moved to `tools/botobs/obstrace.py` in the reader split.
- Add one line to the naming rule: `bossstate` is handed an encounter id it cannot turn into a name,
  so the strategy side names the pull instead — and `hdr.boss` stays stale, so prefer the rename
  record.

Run `/compact-docs-writer` **before** editing this file, as the first step of the doc task.

---

## Files

- `src/Bot/Obs/RaidObsSnapshot.cpp` — collect casting spell ids in `UnitRow`, sweep them in
  `BuildSnapshotPayload`.
- `src/Bot/Obs/RaidObsSession.h` — `UnitRow`/`BuildSnapshotPayload` signatures, `PreRollEntry::spells`,
  the second `UpgradeBossName` overload.
- `src/Bot/Obs/RaidObsLifecycle.cpp` — sweep the drained pre-roll, split `UpgradeBossName`, make
  `MarkPull` rename an open session, add `NamePull`.
- `src/Bot/Obs/RaidObs.h` — declare `NamePull`.
- `src/Ai/Raid/Uld/UldEncounterGate.{h,cpp}` — `UldEncounterIsLive`, `UldEncounterName`, the
  `UldGatedTrigger::Check` call.
- `docs/systems/observability.md` — the three items above.

Save this plan to `docs/plans/raidobs-name-the-pull-sweep-the-snapshot/raidobs-name-the-pull-sweep-the-snapshot.PLAN.md`
once plan mode exits.

---

## Verification

Static checks are mine; the build and the pull are the user's — there is no headless build path here.

1. Build the module. `src/Ai/Raid/Uld/` is a file set other sessions commit to daily; re-check
   `git status` before staging.
2. Re-run the analyzer over all 23 traces from 2026-09-04/05 plus the 2026-08-31 ten. Every view must
   still exit 0, unnamed guids and unresolved spell ids must not rise, and no `held` may exceed its
   fight span.

| Check | measured 2026-09-04/05 | expected after |
|---|---|---|
| bare `snap.u` casting spell ids | 99 across 14 traces | 0 |
| traces filed under the map name | 1 of 23 (a Yogg-Saron pull) | 0 |
| `pull` records with `src:"rename"` | 0 ever recorded | ≥1 on any pull the engage hook misses |
| a session opened by `engage` | correctly named | **unchanged**, never renamed |
| foreign boss-named actions executed | 0 | 0 |
| deaths with `cause:"reset"` | 204/204 of the blow-less self-kills | unchanged |
| roster at 0 hp with no `death` record | 0 | 0 |
| KB/s | 39.4–50.2 | ≤ 52 |

3. Pull Yogg-Saron and wipe in phase one. The trace must be named `yogg-saron` and carry a `pull`
   record with `src:"rename"` and `was:"ulduar"`.
4. Pull an encounter whose boss engages normally (Thorim, Freya) and confirm it is **not** renamed.
5. Confirm a clean kill still closes `kill` and an empty instance still closes `idle`.

---

## Out of scope

- **The `.die` GM command reads as an ordinary combat death.**
  [cs_misc.cpp:1194](../../src/server/scripts/Commands/cs_misc.cpp#L1194) calls
  `Unit::DealDamage(gm, target, target->GetHealth(), ...)`, which fires `OnDamage` — so `blow` is
  filled and `killer` is the GM — but sends no combat-log packet, so there is no `dmg` row behind it.
  The result reads *"killed by Dragon, final blow 41987, health 100.0%, took no damage in the rewind
  window"*. `cause` cannot catch it: the discriminator is `killer == victim`. 4 of 515 deaths, but 2
  of 3 in `thorim_1788555714`. The cheap answer if it ever matters is an analyzer line flagging a
  blow with no combat-log record behind it — no schema change.
- **82 bare aura-caster guids** (65 `aura.s`, 17 `death.auras.caster`), all in the two short Thorim
  traces with 10- and 8-member rosters, all raid members who had left the map. `NoteAura` calls
  `EnsureUnit(aura->GetCaster())` and gets null, which the code already documents as the honest
  answer; for a **player** guid the name is still recoverable via `ObjectAccessor::FindPlayer`, which
  is global rather than map-scoped. Separately, `NoteAuraApplied`
  ([RaidObsCombat.cpp:183](src/Bot/Obs/RaidObsCombat.cpp#L183)) sets `state.caster` and never calls
  `EnsureUnit` at all, so a same-tick aura reaches `death.auras` unnamed by a second route.
- **`World Invisible Trigger` gets `unit` rows with an `own` field** — 17 per Thorim trace — and is
  never sampled. Harmless, but it inflates the owned-unit count against the pets that matter.
- **`thorim_1788555562` closes `wipe` with 1 death and nobody dead.** The instance script reported
  `FAIL` and `end.out` follows the script; `RosterMostlyDead` only ever upgrades `reset`/`idle`, it
  never downgrades. Working as designed — the death count is the reader's check on it.
- **The Freya elders strategy.** 12 attempts on 2026-09-05, 2 kills. An encounter fix, not a
  recorder one.
