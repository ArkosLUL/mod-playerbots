# w0b-tooling — spell-difficulty lint, trace naming, close outcomes

Wave 2, parallel with `w0c-foundation`, which owns everything under `src/Ai/Raid/ToC/`. Rules,
sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).

## Scope

1. **pblint `--spell-difficulty`** (`tools/pblint/pblint.py`).
   - Build its table from `acore_world.spelldifficulty_dbc` rows plus the client DBC CSV
     (`modules/mod-spell-tweaks/data/dbc-reference/spelldifficulty.reference.csv`). The core merges
     both sources (`DBCStores.cpp`); they complement each other.
   - Fix the "DB is a superset" claim in `docs/engine/pitfalls.md` (spell-difficulty entry) and in
     the pblint docstring.
   - Known recogniser quirks:
     - The "properly remapped" regex needs a bare `GetSpellIdForDifficulty(SPELL_X`.
     - `difficulty_is_discussed` silences a finding when a nearby comment says "difficult" or
       "25-man".
   - Re-run on `src/Ai/Raid/Uld` and `src/Ai/Raid/ICC`. Report the new findings; don't fix other
     raids.
2. **`tools/botobs/raidobs/encounter.py`**, map 649 per the naming contract.
   - `BOSS_ALIASES`:
     - `gormok-the-impaler`, `acidmaw`, `dreadscale`, `icehowl` → `northrend-beasts`;
     - all 24 champion slugs → `faction-champions`;
     - `fjola-lightbane`, `eydis-darkbane` → `val-kyr-twins`;
     - any engage-name variants of Jaraxxus and Anub'arak. The Pursuing Spike (34660) engages under
       the name "Anub'arak".
     - Derive champion slugs from `creature_template.name` for the entries in
       `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h` (both factions), exactly as
       `ResolveBossName` slugifies.
   - Coverage gating: a `TOC_PREFIXES` mirror of the trigger-name prefixes, and `node_encounter`
     reading it next to `ULD_PREFIXES`.
   - Note-key prefixes: `nb.`, `jaraxxus.`, `fc.`, `tv.`, `anub.`, plus `toc.` as raid-wide on every
     649 encounter. Extend `prefix_matches_boss` or add an explicit table.
   - Tests in `tools/botobs/tests/`.
3. **RaidObs close outcomes for instance scripts without boss state** (`src/Bot/Obs/`,
   raid-agnostic).
   - Today a ToC trace only closes on idle, then the roster rule may call it a wipe.
   - Prefer a poll in `OnMapUpdate`, for scripts with `GetEncounterCount() == 0`, on the fall of
     `InstanceScript::IsEncounterInProgress()`. It closes the trace as `kill` or `wipe`, so no ToC
     code needs a new API this wave.
   - `IsEncounterInProgress()` goes true before the pull on most ToC encounters (see the overview).
     Only a fall after combat counts.
   - If a new API turns out unavoidable, add it and carry the ToC call over to wave 3.
   - Bump `SCHEMA_VERSION` (`RaidObs.h`) and `SUPPORTED_SCHEMA`/`READABLE_SCHEMAS`
     (`raidobs/trace.py`) only if a field or value changes.
   - Update `docs/systems/observability.md`: lifecycle, and ToC in the converted list.

## Owns

`tools/pblint/`, `tools/botobs/raidobs/`, `tools/botobs/tests/`, `src/Bot/Obs/`,
`docs/systems/observability.md`, the spell-difficulty entry of `docs/engine/pitfalls.md`. Nothing
under `src/Ai/Raid/`.

## Verified facts

- Trace naming today:
  - `OnCreatureEngage` (`RaidObsLifecycle.cpp`) opens every ToC trace, because all openers are
    boss-flagged.
  - `ResolveBossName` matches the engaging creature against the DBC encounter credit entry,
    otherwise it takes the creature's own name.
  - Credit rows: Beasts = Icehowl 34797, Jaraxxus = 34780, Faction Champions = spell 68184 (credit
    type 1), Twins = Eydis 34496, Anub'arak = 34564.
  - `NamePull` only renames a trace still carrying the map name (`UpgradeBossName`).
- `MAP_SLUGS` already has 649 (`trial-of-the-crusader`). `BOSS_ALIASES` has no ToC entries.
- DBC encounter names (client `DungeonEncounter.dbc`, all four difficulties): `Northrend Beasts`,
  `Lord Jaraxxus`, `Faction Champions`, `Val'kyr Twins`, `Anub'arak`. Creature names always come from
  the normal entry (`Creature::InitEntry` `SetName(normalInfo->Name)`), so difficulty entries such as
  "Gormok the Impaler (1)" never reach a trace.
- Engage slugs per encounter:
  - Beasts: `gormok-the-impaler`, `acidmaw`, `dreadscale`, `icehowl` (unit name; Icehowl engaging
    resolves to `northrend-beasts`), `fire-bomb` (34854, boss-flagged, damages the raid).
  - Jaraxxus: `lord-jaraxxus` only (Mistress of Pain, Felflame Infernal, Fizzlebang are not
    boss-flagged).
  - Anub'arak: `anub-arak` for both 34564 and the Pursuing Spike 34660. Burrowers, scarabs, spheres
    are not boss-flagged.
  - Twins: `fjola-lightbane`, `eydis-darkbane`, or `val-kyr-twins` when Eydis (the credit) engages.
  - Champions: **28** entries in `ToCHelpers_FactionChampions.h`, 14 per faction, not 24. Slugs:
    `vivienne-blackwhisper` 34441, `thrakgar` 34444, `liandra-suncaller` 34445, `caiphus-the-stern`
    34447, `ruj-kah` 34448, `ginselle-blightslinger` 34449, `harkzog` 34450, `birana-stormhoof` 34451,
    `narrhok-steelbreaker` 34453, `maz-dinah` 34454, `broln-stouthorn` 34455, `malithas-brightblade`
    34456, `gorgrim-shadowcleave` 34458, `erin-misthoof` 34459, `kavina-grovesong` 34460,
    `tyrius-duskblade` 34461, `shaabad` 34463, `velanaa` 34465, `anthar-forgemender` 34466,
    `alyssia-moonstalker` 34467, `noozle-whizzlestick` 34468, `melador-valestrider` 34469, `saamul`
    34470, `baelnor-lightbearer` 34471, `irieth-shadowstep` 34472, `brienna-nightfell` 34473,
    `serissa-grimdabbler` 34474, `shocuul` 34475.
  - Also boss-flagged, never aliased: Dark/Light Essence (friendly, no damage), The Lich King
    (passive; `the-lich-king` is ICC's slug), Tirion, Varian, Garrosh, Thrall, Jaina.
- Current ToC trigger names already lead with the contract prefixes. Outside ToC, Azjol-Nerub's
  `anub'arak impale`/`anub'arak pound` and Naxx's `anub'rekhan*` must not match `anubarak`.
- Note-key prefixes against `prefix_matches_boss` today: `nb`, `fc` (initials) and `anub` (first
  word) derive; `jaraxxus`, `tv`, `toc` do not. `anub` also derives for Naxx's `anub-rekhan`.
- Spell difficulty:
  - Two sources that share **no row**: world DB `spelldifficulty_dbc` 604 rows (ids 8374-72397, row
    id = base spell id), client `SpellDifficulty.dbc` 581 rows (mirrored in
    `modules/mod-spell-tweaks/data/dbc-reference/spelldifficulty.reference.csv`, real DBC ids). The core
    loads the DBC, then overlays DB rows by row id, DB winning (`DBCDatabaseLoader.cpp:40-110`,
    `LoadDBC` in `DBCStores.cpp`). DB-only: Plasma Blast 62997, Valithria's Emerald Vigor 70873
    (70873/70873/71941/71941, heroic Twisted Nightmares) and Dream Portal 72224
    (72224/72224/72480/72480); the rest of ICC is DBC-only. Every ToC combat remap is DBC-only (Light
    Essence row 405: 65686/67222/67223/67224); ToC's one DB row is the Anub'arak scarab achievement
    spell 68186/68515.
  - `DBCStores.cpp:498-523` drops a row unless its first two ids are set, and maps **every** id of a
    row to it, so `GetSpellIdForDifficulty` resolves from any member. ToC's Touch constants hold the
    10H ids 67297/67282 (rows 464 `65950/67296/67297/67298` and 441 `66001/67281/67282/67283`),
    invisible to today's base-keyed sweep.
  - Today's sweep (DB only, base-keyed) reports 6 findings tree-wide (Naxx 5, OS 1), none in Uld,
    ICC or ToC. `difficulty_is_discussed` silences five, all real: ICC `SPELL_VILE_GAS_H` 69240 (its
    comment says "no difficulty variants", but DBC row 1987 is 69240/71218/73019/73020),
    `SPELL_MALLEABLE_GOO_25N`/`_10H`, ToC `SPELL_SWEEP_0`/`_1`.
  - A scratch prototype of the tasks below (merged table, member index, one finding per row, marker
    opt-out, brace-list names) reports ToC 16, ICC 5, Naxx 5, VoA 2, OS 1, Ony 1, RS 1, Uld 0. ICC:
    Vile Gas 69240, Acid Burst 70744, Harvest Soul 68980, Malleable Goo 72297 (row
    72297/72548/72549/72550) and 74280 (row 72295/72615/74280/74281). VoA `SPELL_BURNING_BREATH` is a
    false positive: `GetSpellForDifficultyFromSpell(GetSpellInfo(SPELL_X))` is not recognised.
- Close outcomes (core facts behind Scope 3):
  - `InstanceScript::GetEncounterCount()` is `bosses.size()`; ToC and Vault of Archavon (624) are the
    only raid scripts without `SetBossNumber`, and both override `IsEncounterInProgress`.
  - ToC's override (`instance_trial_of_the_crusader.cpp:183-194`) returns `EncounterStatus ==
    IN_PROGRESS` if any alive non-GM player is on the map, **otherwise resets `EncounterStatus` to
    `NOT_STARTED`**. `InstanceCleanup` (run every 5 s once nobody is alive) charges a heroic attempt
    only while `EncounterStatus == IN_PROGRESS` (line 1553), so a call with nobody alive makes that
    wipe free.
  - Falls: every boss's `EnterEvadeMode` sets `TYPE_FAILED` → `InstanceCleanup(true)` → `NOT_STARTED`;
    kills set it through `TYPE_NORTHREND_BEASTS_ALL`/`TYPE_JARAXXUS`/`TYPE_FACTION_CHAMPIONS`/
    `TYPE_VALKYR`/`TYPE_ANUBARAK` `DONE`. Gormok's and the worms' `DONE` keep it `IN_PROGRESS`.
  - Kill credit: `Map::UpdateEncounterState` (`Map.cpp:2925-2972`) fires
    `GlobalScript::OnAfterUpdateEncounterState` (upstream hook,
    `GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE`) on every call, matched row or not. Callers:
    `KillRewarder.cpp:309-313` (a dungeon-boss kill with a reward) and `Spell.cpp:4532` (a spell with
    `SPELL_ATTR0_CU_ENCOUNTER_REWARD`, i.e. FC's 68184 cast by Tirion). Both run synchronously in the
    same call stack as the matching `DONE`.
  - Twins: when Fjola takes the last blow her `JustDied` kills Eydis with `Unit::Kill(twin, twin)`.
    The shared `JustEngagedWith` calls `me->LowerPlayerDamageReq(me->GetMaxHealth())`
    (`boss_twin_valkyr.cpp:217`), and only evade or respawn resets it (`CreatureAI.cpp:409`,
    `Creature.cpp:1997`), so `IsDamageEnoughForLootingAndReward` holds for both twins. `Unit::Kill`
    (`Unit.cpp:14048-14066`) then rewards Eydis's loot recipient and the 34496 credit fires whichever
    twin took the last blow. No credit only if no player ever hit Eydis (no loot recipient).
  - Heroic Beasts: `EVENT_SCENE_004` schedules `EVENT_SCENE_006` (Icehowl) at 340 s whether or not the
    worms are dead (`instance_trial_of_the_crusader.cpp:626-627`). The credit fires on Icehowl's
    death, but `TYPE_ICEHOWL` `DONE` ends the encounter only once `northrendBeastsMask` reaches 7
    (lines 377-383), so a credit can land with Gormok or a worm still fighting. Normal mode summons
    Icehowl only after both worms are `DONE`.
  - VoA's override also returns true in Wintergrasp war time, the 10 min before it, or with no WG
    battlefield (`Wintergrasp.KickVoAPlayers`, default 1).
  - `acore_characters.log_encounter` holds no 649 row: ToC has never been killed on this server.

## Task list

The integrate step runs every check. Groups do not run the lane checks; group 1 may run pblint while
building it.

### Group 1 — pblint spell difficulty

Files: `tools/pblint/pblint.py`, `tools/botobs/tests/test_pblint_spell_difficulty.py` (new),
`docs/engine/pitfalls.md` (the spell-difficulty bullet under "Detection that never fires" only).

1. **Table.** Load both sources as rows of ids: the DB query as today (`PB_MYSQL`), and the CSV at
   `REPO.parents[1] / "modules/mod-spell-tweaks/data/dbc-reference/spelldifficulty.reference.csv"`
   (resolves from the main tree and from a `build-toc-wt/<lane>` worktree), overridable with
   `PB_SPELL_DIFFICULTY_CSV`. A missing CSV or DB is a `RuntimeError` (exit 2), never a silent
   one-source sweep. Merge by row id with the DB winning, as the core does. Drop a row unless its
   first two ids are positive; keep only positive ids; drop rows with fewer than two distinct ids.
   Index every id of a row to that row. Keep the merge a pure function so tests need no DB.
2. **Row-level check.** Per raid directory, group the referenced 4-6 digit literals by row. Report a
   row once, at its first referenced member (file, line), when some id of the row is never referenced
   and no referenced member is handled. Message names the constant, the row and the missing ids.
   Findings stay `error=False`.
3. **Handled.** A member is handled when:
   - its constant, or the literal itself, appears anywhere in the **whole first argument** of a
     `GetSpellIdForDifficulty(` call. Take the argument up to the top-level comma and collect every
     identifier and 4-6 digit literal in it, so `ToCSpells::SPELL_X`, `static_cast<uint32>(SPELL_X)`
     and `65686` all count;
   - its constant is declared but never used (today's rule);
   - its name, or for a literal inside a brace initializer the initializer's declared name, matches
     `NOT_A_SPELL`. Widen `entry` to `entr(y|ies)` (ICC `addEntriesLady` holds creature 38135, which is
     also DB row 33534/38135);
   - a comment on its line or the line directly above carries `pblint: spell-difficulty-ok`.
     Delete `difficulty_is_discussed`: free text no longer silences anything.
4. **Docstrings.** Module usage line: the sweep reads the world DB and the client CSV. Table
   docstring: replace the superset claim with the split (facts above).
5. **pitfalls.md** spell-difficulty bullet, through `compact-docs-writer`: the two sources share no
   row and the core overlays DB on DBC by row id, so either alone reads a remap as "none" (Plasma
   Blast DB-only, ToC DBC-only); any member id resolves the row (Touch 67297 is a 10H id); what the
   sweep reads, how it reports, the marker opt-out. Recount "28 of Ulduar's 144" against the merged
   table and correct or drop the figure.
6. **Tests** (stdlib `unittest`, no DB, runs under `python -m unittest discover -s tools/botobs/tests`;
   import pblint by path): the merge (both sources contribute, DB wins on a shared id, degenerate rows
   dropped, member index); each handled rule of task 3 over a temp `src/Ai/Raid/<X>/` tree with
   `pblint.REPO` patched; a free-text "difficulty" comment no longer silences; one finding per row.

### Group 2 — trace readers

Files: `tools/botobs/raidobs/encounter.py`, `tools/botobs/raidobs/coverage.py`,
`tools/botobs/tests/test_toc_naming.py` (new).

7. **`BOSS_ALIASES`**: `gormok-the-impaler`, `acidmaw`, `dreadscale`, `icehowl`, `fire-bomb` →
   `northrend-beasts`; `fjola-lightbane`, `eydis-darkbane` → `val-kyr-twins`; the 28 champion slugs
   above → `faction-champions`. Jaraxxus and Anub'arak need none. Never alias `the-lich-king`.
8. **`TOC_PREFIXES`** beside `ULD_PREFIXES`: `gormok`, `northrend worms`, `icehowl` →
   `northrend-beasts`; `jaraxxus` → `lord-jaraxxus`; `faction champions` → `faction-champions`;
   `twin valkyr` → `val-kyr-twins`; `anubarak` → `anub-arak`. `node_encounter` reads both. Its comment
   states that every ToC trigger name leads with one of these and a new encounter prefix needs a row.
9. **Note-key prefixes.** An explicit table consulted first in `prefix_matches_boss`, where a hit
   decides alone: `nb` → `northrend-beasts`, `jaraxxus` → `lord-jaraxxus`, `fc` → `faction-champions`,
   `tv` → `val-kyr-twins`, `anub` → `anub-arak`, `toc` → all five. Other prefixes keep the derived
   rule. Update the docstring.
10. **`coverage.py`**: the gated-fold line and docstring say "another encounter of this raid", not
    Ulduar.
11. **Tests** in `test_toc_naming.py`:
    - every alias above resolves;
    - the count of `faction-champions` aliases equals the enum entries parsed from
      `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h`;
    - `slugify("Anub'arak") == "anub-arak"`, `slugify("Lord Jaraxxus") == "lord-jaraxxus"`,
      `slugify("Val'kyr Twins") == "val-kyr-twins"`;
    - `node_encounter` maps a name per prefix, returns None for `anub'arak impale` and
      `anub'rekhan locust swarm`, and each ToC prefix leads at least one `creators["..."]` key under
      `src/Ai/Raid/ToC/Trigger/`;
    - `prefix_matches_boss`: `toc` matches all five and not `anub-rekhan`, `anub` does not match
      `anub-rekhan`, `jaraxxus`/`tv` match their slugs, the existing Ulduar cases still hold;
    - `silent_keys` and `coverage.walked` on a small in-memory ToC trace fold another encounter's node.

### Group 3 — RaidObs close outcomes

Files: `src/Bot/Obs/RaidObs.h`, `src/Bot/Obs/RaidObsSession.h`, `src/Bot/Obs/RaidObsSession.cpp`,
`src/Bot/Obs/RaidObsLifecycle.cpp`, `src/Bot/Obs/RaidObsScripts.cpp`, `docs/systems/observability.md`.

12. **Credit hook.** Add `GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE` to `RaidObsGlobalScript`. On a
    call whose `encounters` list holds a row with the same credit type and entry, call a new
    `RaidObs::OnEncounterCredit(Map*)` (declared in `RaidObs.h` next to `OnBossState`). It only
    queues the instance id in a registry set (`g_pendingCredit`, beside `g_pendingBossState`, under
    `g_registryMutex`). Never close from the hook: it runs inside `Unit::Kill` or `Spell::cast`, often
    inside a bot's own action, and `CloseSession` drains every roster engine's coverage.
13. **Kill.** In `OnMapUpdate`, after `ProcessPendingBossState`, take the queued credit. If the
    instance script exists and `GetEncounterCount() == 0`, `CloseSession(instanceId, "kill")`. Boss-state
    scripts ignore it: their `DONE` path stays authoritative. `OnMapDestroyed` erases the entry.
14. **Fall.** Two `ObsSession` fields: the last `IsEncounterInProgress()` answer, and whether the
    roster was in combat on a poll that answered true. Each `OnMapUpdate` of an open session on a
    script with `GetEncounterCount() == 0`, before the idle close:
    - no alive non-GM player on the map: do **not** call it; that state is the answer false;
    - otherwise call it;
    - true → false with combat seen while true: `CloseSession(instanceId, "reset")` and return. The
      roster rule in `CloseSession` files it `wipe` when most of the raid died.
    Compute `AnyRaidMemberInCombat()` once per update and reuse it. No schema bump.
15. **`observability.md`**, through `compact-docs-writer`:
    - Lifecycle: the no-boss-state close (credit → `kill`, fall after combat → `reset`/`wipe`), why
      it never calls `IsEncounterInProgress` with nobody alive, and the two gaps (Twins,
      Wintergrasp).
    - Reading a trace: ToC's aliases (Beasts, 28 champions, twins), `TOC_PREFIXES` gating beside
      `ULD_PREFIXES`, the explicit note-key table (`toc.` raid-wide).
    - Converted list: add Trial of the Crusader (`toc.progress`; boss keys arrive with each boss
      lane).

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Kill = a DBC encounter credit queued by the upstream `OnAfterUpdateEncounterState` hook and read on the next map update, only for scripts with no boss state | `Map.cpp:2925-2972`, `KillRewarder.cpp:309-313`, `Spell.cpp:4532`, `instance_encounters` 629-648 | Roster alive-share at the fall: files a messy kill as a wipe and an evade with the raid up as a kill. ToC's `GetData(1)` advancing: exact, but not raid-agnostic |
| Fall after combat closes `reset`, left to the existing roster rule to call `wipe` | Same semantics as boss-state `NOT_STARTED` (`ProcessPendingBossState`) | Close `wipe` outright: loses the evade-with-raid-alive case |
| Never call `IsEncounterInProgress` with no alive non-GM player; treat that as false | `instance_trial_of_the_crusader.cpp:183-194`, 1553 | Call it: every observed wipe stops costing a heroic attempt |
| Queue the credit, close on the map update | `OnBossState` pattern; `CloseSession` drains engine coverage | Close in the hook: re-enters a bot mid-action |
| Accept the Twins no-hit edge (a kill where no player ever hit Eydis files `reset`/`wipe`) | `boss_twin_valkyr.cpp:217` zeroes Eydis's damage requirement, so any other kill credits through her loot recipient | Treat any boss-flagged death as a kill: Gormok dies mid-Beasts, so a wipe on Icehowl would read as a kill |
| A credit that lands while `IsEncounterInProgress` still answers true (checked only with someone alive) latches; the fall then closes `kill` unless the roster is mostly dead, else `reset`; an idle close of a latched trace is `kill` | Heroic Beasts credits on Icehowl, who can die before the worms; VoA's override stays true through Wintergrasp | Close `kill` on every credit: a heroic wipe to the worms after Icehowl files as a kill |
| Trial of the Crusader stays out of `observability.md`'s converted list | Nothing emits `toc.progress` yet; no permanent doc describes unbuilt behaviour | List it now with `toc.progress`: a silent probe if `w0c` lands late or names the key differently |
| `test_toc_naming.py` also requires every ToC `creators[...]` key to have an owner in `TOC_PREFIXES`, except an explicit `RAID_WIDE_TRIGGERS` allowlist (empty today) | An off-contract name reads NEVER on every other encounter's pull | Check only that each prefix leads some trigger: misses drift |
| Raid-agnostic, so VoA changes too (kills close on credit); Wintergrasp edge documented | Scope 3 asks raid-agnostic | Restrict to map 649 |
| No schema bump | No field or value changes; `end.out` keeps its set | Tag the close source in `end`: nothing reads it |
| 28 champion aliases; `fire-bomb` aliased though it cannot open a trace; essences, Lich King and NPC leaders not aliased | Enum header, `creature_template` type flags | 24 as the brief said: 4 slugs split the sample |
| Explicit note-key table consulted first | `anub` derives for Naxx `anub-rekhan`; `tv`, `toc`, `jaraxxus` do not derive | Add a last-word derivation: still no `tv`/`toc`, and more false matches |
| pblint: merge by row id with DB winning; a missing source is exit 2 | `DBCDatabaseLoader.cpp`, `DBCStores.cpp:498-523` | Fall back to DB-only with a warning: the bug this fixes |
| pblint: index every member id, one finding per row | Touch 67297/67282 are 10H ids | Base-keyed: Touch stays invisible |
| pblint: explicit `pblint: spell-difficulty-ok` marker replaces free-text silencing | All five silenced hits are real; ICC's comment is factually wrong | Narrow the wording to "spelldifficulty"/"remap": still silences Vile Gas |
| Spell-difficulty findings stay warnings; the merge stage runs with `--warnings` | Error level would fail scoped runs on ICC/Naxx/VoA/OS/Ony/RS today | Error level |
| pblint tests live in `tools/botobs/tests/` | The standing check only discovers that directory | `tools/pblint/tests/`: nothing runs it |
| pblint: float literals are not ids; the scan is `(?<![\w.])\d{4,6}(?![\w.])` | Kara/Naxx/Hyjal positions: `3462.99f` | `\b\d{4,6}\b`: reads 3462 as an id |
| pblint: a source returning zero rows is exit 2, like a missing one | Task 1, never a one-source sweep | Accept an empty table |
| pblint: an id in two rows maps to the higher row id | `DBCStores.cpp:498-523` fills `SetSpellDifficultyId` in ascending row order, last write wins | First row wins: not what the core resolves |
| pblint: a brace list is named by `name = {`, `name[..] = {`, `name[..] {`, `Type name{`; class, struct, enum, namespace and base-class bodies excluded; the innermost list wins; the literal's own `NAME = id` and its list's name both go through `NOT_A_SPELL` | ICC `addEntriesLady` (creature 38135, also DB row 33534/38135) | Only `name = {`: misses brace-init declarations |
| pblint: an id with several constant names in one raid is remapped if any name is in a remap argument; "declared and never used" needs every name unused | Conservative: a finding means no name handles the id | First name only (the old `setdefault`): depends on file order |
| pblint: `pblint: spell-difficulty-ok` counts only inside a comment (in the raw line, not the comment-stripped one) | Task 3 asks for a comment | Anywhere on the line: a string literal would silence |
| `OnEncounterCredit` queues only while a session is open, not on every tracked map | A credit with no trace has nothing to close, and a stale queued id would close a session opened later in the same tick as `kill` | Queue on every tracked map, as task 12 wrote it |
| Durable facts stay in this brief until the owning group folds them into `pitfalls.md`/`observability.md` with the code | Docs rule: no permanent doc describes unbuilt behaviour | Edit the docs now and again later |
| Known gaps go to `observability.md` (the raid-agnostic owner) and this brief, not the ToC boss docs; the per-boss lines are carried over | `w0c-foundation` owns the ToC boss docs and has them dirty in its worktree: a same-wave edit here would conflict at merge | Edit `twin-valkyr.md`/`northrend-beasts.md`/`anubarak.md` now |
| No Warcraft Tavern read, no navprobe | Tooling lane: no tactics, no coordinates | — |

## Sweep findings

`pblint.py --spell-difficulty --warnings src/Ai/Raid/Uld src/Ai/Raid/ICC src/Ai/Raid/ToC` on the
merged table (1182 rows, 2941 member ids): Uld 0, ICC 5, ToC 16, all warnings. Tree-wide also Naxx 5,
VoA 2, OS 1, Ony 1, RS 1 (RS: `SPELL_FLAME_BREATH_ALT1` 68970, row 1264 18435/68970). Ulduar has 53
of its 155 `SPELL_` constants in a row (51 DB, 2 DBC: Ignis Brittle 62382/67114), all handled.

- ICC (`ICCTriggers.h`): `SPELL_VILE_GAS_H` 69240 (row 1987), `SPELL_ACID_BURST` 70744 (2096),
  `SPELL_HARVEST_SOUL_LK` 68980 (2298), `SPELL_MALLEABLE_GOO_25N` 72297 (2197),
  `SPELL_MALLEABLE_GOO_10H` 74280 (2211). Not fixed here.
- ToC (`Util/ToCData.h`, for `w0c-foundation`): `SPELL_IMPALE` 66331, `SPELL_BURNING_BITE` 66879,
  `SPELL_BURNING_SPRAY` 66902, `SPELL_SWEEP_0` 66794 (row 614 lacks 67644/67645),
  `SPELL_FEL_FIREBALL` 66532, `SPELL_INCINERATE_FLESH` 66237, `SPELL_PERMAFROST` 66193,
  `SPELL_LEECHING_SWARM` 66118, `SPELL_LIGHT/DARK_ESSENCE` 65686/65684, `SPELL_LIGHT/DARK_VORTEX`
  66046/66058, `SPELL_LIGHT/DARK_TOUCH` 67297/67282 (10H ids), `SPELL_LIGHT/DARK_TWIN_PACT`
  65876/65875.
- VoA: `SPELL_BURNING_BREATH` 66665 is the known false positive (Known gaps); `SPELL_FREEZING_GROUND`
  72090 (DBC row 2132, 72090/72104) is a new real finding. Not fixed here.

## Carried over

- Stale docs outside this lane (merge stage or `w6-closeout`):
  - `docs/raids/README.md:137` says to check `spelldifficulty_dbc` per spell (DB only; ToC remaps are
    DBC-only, ICC spans both).
  - The overview's "Spell difficulty" constraint says the DB has none of ToC's remaps; it holds one,
    the Anub'arak scarab achievement spell 68186/68515, which no bot references.
  - `docs/raids/vault-of-archavon.md:43` says the DB maps "nothing else"; the merged table adds DBC
    rows 481 (Burning Breath 66665/67328) and 2132 (Freezing Ground 72090/72104). VoA traces now also
    close `kill` on credit.
  - `docs/raids/ulduar/mimiron.md:1474` says "all 139 Ulduar constants"; merged count is 155, 53 in a
    row.
- Merge stage, cross-lane with `w0c-foundation`:
  - `test_toc_naming.py` parses `enum class ToCFactionChampions` (28 `NAME = <digits>` lines) in
    `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h` and needs each `TOC_PREFIXES` key to lead a
    `creators["..."]` key under `src/Ai/Raid/ToC/Trigger/`. If `w0c` renames the enum or stops
    registering triggers that way, update the test.
  - `observability.md`'s converted list gets Trial of the Crusader (`toc.progress`) in the same
    change that emits the key (`w0c`); this lane left it out.
  - Once `w0c` lands, the `TOC_PREFIXES` comment in `raidobs/encounter.py` should read "Mirrors
    ENCOUNTER_PREFIXES in src/Ai/Raid/ToC/Util/ToCEncounterGate.cpp; change both together", with a
    test in `test_toc_naming.py` that parses that array and compares it with `TOC_PREFIXES`. A
    raid-wide ToC trigger goes into that test file's `RAID_WIDE_TRIGGERS`.
  - Once `w0c`'s ToC trigger wrapper exists, observability.md's `NamePull` paragraph (only
    `UldGatedTrigger` today) needs a ToC line.
- `w4-twins`: nothing needed; the no-hit edge below is not worth a snapshot inference.
- Merge stage or the owning boss lane: one "Known gaps" line each, pointing at observability.md's
  lifecycle section, in `twin-valkyr.md` (no-hit Twins kill files `reset`/`wipe`),
  `northrend-beasts.md` (heroic latched credit + evade with most alive files `kill`) and
  `anubarak.md` (`anub-arak` slug shared with Azjol-Nerub).

## Review outcome

Open issues after re-review: none. Deferred findings: none.

## Engine lessons (for the merge stage)

- ToC's `InstanceScript::IsEncounterInProgress` mutates state: with no alive non-GM player it resets
  `EncounterStatus` to `NOT_STARTED` and the wipe costs no heroic attempt. `w0c`'s `ToCEncounterIsLive`
  runs per bot per tick and must not call it when nobody is alive (a dead bot's engine included).
- The spell-difficulty sources split cleanly: world DB 604 rows, client DBC CSV 581, no shared row. The
  core overlays DB on DBC by row id; any member id resolves its row.
- `OnAfterUpdateEncounterState` fires on every `Map::UpdateEncounterState` call, matched row or not,
  so a handler must match the row itself.

## Known gaps

- A Twins kill where no player ever hit Eydis carries no credit and files as `reset`/`wipe` (facts
  above).
- VoA's `IsEncounterInProgress` stays true through Wintergrasp war time: a wipe that leaves anyone
  alive closes on idle (a full wipe still reads false), and a war ending mid-pull closes the trace
  early as `reset`.
- Heroic Beasts: after Icehowl's credit latched, an evade with most of the raid alive files `kill`.
- `anub-arak` is also Azjol-Nerub's boss slug (map 601); slug-keyed selection cannot tell them apart
  (5-mans are untracked unless `Obs.Maps` names them).
- pblint does not follow `GetSpellForDifficultyFromSpell(GetSpellInfo(SPELL_X))` (VoA Burning
  Breath false positive).

## Blocked
