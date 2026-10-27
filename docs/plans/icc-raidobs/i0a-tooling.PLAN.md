# i0a-tooling — ICC naming tables, schema v14 reader, raid-wide ICC reader

Wave 1, parallel with `i0b-foundation`, which owns `src/`. Rules, sources, naming contract and
schema v14: [icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). Build only from the contract: the C++ that
emits v14 lands at the same time, so test on synthetic pulls.

## Scope

1. **`tools/botobs/raidobs/encounter.py`.**
   - `ICC_PREFIXES`: trigger-name prefix → slug, added to the tuple `node_encounter` walks. The 52
     creators are in `src/Ai/Raid/ICC/ICCTriggerContext.h`; all start with `icc `.
   - Names that don't put the boss next need their own row:
     - `icc spike` → `lord-marrowgar`
     - `icc dark reckoning` → `lady-deathwhisper`
     - `icc in cannon` and `icc gunship` → `icecrown-gunship-battle`
     - `icc rotting frost giant` → `rotting-frost-giant`
     - `icc dogs` → `stinky-and-precious`
     - `icc valkyre spear` and `icc sister svalna` → `sister-svalna`
   - `i0b-foundation` renames two triggers' own names: `IccGunshipCannonNearTrigger` to
     `icc gunship cannon near`, `IccBqlGroupPositionTrigger` to `icc bql group position`.
   - `NOTE_PREFIXES` rows for every contract prefix that doesn't derive from its slug: `ldw`, `gsb`,
     `dbs`, `fg`, `rf`, `bql`, `sg` (Sindragosa and the gauntlet), `bpc` (council and trash), `lk`,
     `dogs`, and `icc` matching every ICC slug.
   - Keep `lk` off ToC: `the-lich-king` is also a passive boss-flagged unit's slug on map 649, and
     aliases are not map-scoped.
   - `BOSS_ALIASES` for engage names that could still name a file: `blood-council`, `prince-keleseth`,
     `prince-taldaram`, `prince-valanar` → `blood-prince-council`; `queen-lana-thel` →
     `blood-queen-lana-thel`; the captains `muradin-bronzebeard` and `high-overlord-saurfang` →
     `icecrown-gunship-battle`.
   - Check every alias for a collision with another raid's slug and say what you found. Muradin
     appears elsewhere in ICC too, friendly.
2. **`raidobs/trace.py`.** `SUPPORTED_SCHEMA` 14, 14 added to `READABLE_SCHEMAS`. Load `cheat`
   records the way the other streams load, with `tgt` optional. Join `g` and `tgt` to names through
   the `unit` records.
3. **`raidobs/validity.py`.** The banner shows `cfg.iccbuffs` on ICC pulls, flagged when the pull is
   heroic, since the buffs only act there. It also shows a one-line cheat summary: kinds fired and
   total uses. Neither disqualifies a pull.
4. **`bosses/icecrown_citadel.py`.** The raid-wide reader for any 631 trace, shaped like
   `bosses/trial_of_the_crusader.py`:
   - banner;
   - `--cheats`: per key the bots, targets, first `t`, total `n`, with humans marked;
   - `--phases`: the `<prefix>.phase` timeline of the trace's encounter, read from notes.
5. **Tests.**
   - `tests/test_icc_naming.py`, modelled on `tests/test_toc_naming.py`:
     - every `ICCTriggerContext.h` creator resolves to an owner or sits in an ICC raid-wide set;
     - every `ICC_PREFIXES` key starts some creator;
     - every slug maps to itself;
     - `the-lich-king` stays unaliased;
     - each note prefix matches its slugs and no slug of another raid.
   - `tests/test_icecrown_citadel.py`: a synthetic v14 ICC pull with cheat records, through every view.
   - A v14 fixture from `fixtures/make_full.py` that carries a `cheat` row with and without `tgt`. Older
     fixtures stay.
6. **`docs/systems/observability.md`:**
   - the `cheat` row and `cfg.iccbuffs` in the schema table, plus the `cheat` record's write rule;
   - the ICC aliases under "Reading a trace";
   - replace the "`cfg.cheats` is the conf string" bullet with what the record now answers and what it
     still can't (a cheat site nobody wrapped);
   - leave ICC on the "Still bare" list: `i6-closeout` moves it.
   - Fold with `compact-docs-writer`.

## Owns

`tools/botobs/raidobs/encounter.py`, `trace.py`, `validity.py`, `bosses/icecrown_citadel.py`,
`tests/test_icc_naming.py`, `tests/test_icecrown_citadel.py`, `fixtures/make_full.py` and the fixture
it writes, `docs/systems/observability.md`, this brief. Anything else in `tools/botobs/` only to keep
the suite green, listed in the report. Nothing under `src/`.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
