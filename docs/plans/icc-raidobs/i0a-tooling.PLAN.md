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

Not split: one coherent Python change of a few hundred lines.

1. [x] `encounter.py`: `ICC_KEY_PREFIXES` (the contract, slug → key prefix), `ICC_MAP`,
   `ICC_PREFIXES` in `node_encounter`, `NOTE_PREFIXES` rows generated from the contract, ICC
   `BOSS_ALIASES`.
2. [x] `trace.py`: schema 14 readable; `cheat_uses(trace)` aggregates `cheat` rows per key.
3. [x] `validity.py`: `iccbuffs` line on ICC pulls, flagged on heroic with the buffs on; `cheated` summary
   line; both informational.
4. [x] `bosses/icecrown_citadel.py`: banner, `--cheats`, `--phases`.
5. [x] `fixtures/make_full.py` writes `full-v14.ndjson` (`cfg.iccbuffs`, cheat rows with and without
   `tgt`); v12 and v13 regenerate byte-identical.
6. [x] `tests/test_icc_naming.py`, `tests/test_icecrown_citadel.py`.
7. [x] `docs/systems/observability.md` per scope item 6, folded with `compact-docs-writer`.

Alias collisions (scope item 1): none with another raid's slug. `prince-keleseth` is also Utgarde
Keep's boss and `prince-taldaram` Ahn'kahet's, both 5-man and untraced unless `Obs.Maps` names them.
Deathbringer's Rise's friendly Muradin (37200) and Saurfang (37187) carry `CREATURE_TYPE_FLAG_BOSS_MOB`
like the captains (36948, 36939), but never fight the raid, so they never name a file. Names
confirmed from `DungeonEncounter.dbc` (client and server copies match) and `instance_encounters`:
Valanar's credit 37970 is "Blood Council", 37955 "Queen Lana'thel", and only difficulties 0/1 have
an encounter list. No `631_` trace exists, so tests run on synthetic pulls.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| No `icc spike` row in `ICC_PREFIXES` | `icc spike` is an action (`ICCActionContext.h:20`); coverage only owns trigger nodes, and Marrowgar's trigger is `icc lm`. A row would break "every prefix starts a creator" | Keep the row as dead weight |
| Every contract prefix gets a `NOTE_PREFIXES` row, generated from one slug → prefix table, the derivable `lm`, `pp`, `ss`, `vd` included | A row decides alone, so no ICC prefix can also claim another raid's slug by initials, and the contract lives in one table | Rows only for the 11 non-derivable prefixes |
| `icc` matches the 17 contract slugs, not the map slug `icecrown-citadel` | Contract: "matches every ICC slug". A map-named trace names no encounter, so no key is silent there | Include the map slug |
| `lk` matches `the-lich-king` only; `toc` never matches it | Brief: keep `lk` off ToC. Aliases are not map-scoped, so the guard is the prefix sets, pinned by tests | Map-scoped prefixes, a larger change |
| Extra aliases `stinky`, `precious` → `stinky-and-precious` | `MarkPull` opens a session under the creature's name (`ResolveBossName`), and `UpgradeBossName` only renames a map-named session, so a later `NamePull` cannot fix a `stinky` file | Rely on `i0b` opening map-named first |
| Banner flags `iccbuffs` only when on and the pull is heroic | Every `EnableICCBuffs` site tests 10H/25H (`ICCActions_LDW.cpp:130`, `ICCTriggers.cpp:394,775`, `ICCActions_LK.cpp:74`), so off, or on in normal, changes nothing | Flag every heroic pull |
| Banner prints `cheated none recorded` only on a v14 ICC pull | ICC wraps every cheat site; elsewhere an empty stream means unwrapped, not clean | Print it on every v14 pull |
| `--phases` treats `<prefix>.phase` as one raid-wide stream; a step starts at the first bot to note it | The key may be an `ObsValue` or a per-bot `NoteDerived`; one merged change-only stream reads both | Read the declared shape from source |
| `--cheats` lists targets as a set, uses per bot only | A row's `tgt` is the use that wrote it while `n` spans every use since, so a per-target count would be made up | Count `n` per target |
| ICC pulls now get `strategy-off` (always decides) when no own node fired; its text now reads "its gate never opened or no trigger found its boss" | Follows from owning ICC nodes; ICC has no gate, and a trigger keyed on the wrong boss is what the check exists to catch | Exempt ICC from the check |
| Python lines at 120 columns | Every botobs file and the lane rules use 120; the module `.editorconfig` says 80, which no tools file follows | Rewrap the new files to 80 |

## Carried over

- `raidobs/verify.py` (outside this lane's ownership): add `"cheat": ("g", "tgt")` to
  `VERIFY_UNIT_FIELDS`, so `--verify` catches a `NoteCheat` that skips `EnsureUnit`.
- `observability.md` states the boss-id → slug table as the contract has it; the merge stage checks
  it against `i0b-foundation`'s Lifecycle code and adds the mechanics there if they differ.

## Known gaps

- `--bot` and `--notes` (`raidobs/timeline.py`) don't render `cheat` rows; the ICC reader's
  `--cheats` is the only view of them.
- The Rotting Frost Giant shares `icc` with the raid-wide keys, so `--probes` reports a declared
  giant key silent on every ICC pull. A contract change (own prefix) would fix it; noted for the ICC
  README's naming section.
- Aliases are not map-scoped: a traced Utgarde Keep or Ahn'kahet prince pull would read as
  `blood-prince-council` (in `observability.md`).

## Blocked
