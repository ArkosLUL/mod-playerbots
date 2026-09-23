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

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
