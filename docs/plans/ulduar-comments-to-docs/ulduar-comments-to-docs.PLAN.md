# Ulduar: move the data out of code comments into the docs

## Context

Ulduar's boss code is fork-only (139 files under `src/Ai/Raid/Uld/`, upstream has 11) and carries
~5,800 lines of comment blocks of 3+ lines, plus ~1,000 lines of long comments and docstrings in the
Ulduar botobs checkers. Most of it is measured evidence: pull and hit counts, medians, distances,
failed alternatives and what they cost. Per `docs/README.md` that belongs in
`docs/raids/ulduar/<boss>.md`; `.agents/docs/cpp-guidelines.md` wants code comments at one line,
more than two only in exceptional cases. Part of the data is already in the docs (duplicated), part
exists only in the comments.

Decided with the user (2026-10-10): **all of Ulduar, C++ and Python, one boss per commit.**
Precedent: `b8d15d76d` did the same pass for Eye of Eternity.

Each boss commit ticks its Status line below; the last commit deletes this plan.

## Scope and order

Largest first. Each row: `src/Ai/Raid/Uld/*/Uld*_<Boss>.{h,cpp}` → `docs/raids/ulduar/<doc>.md`.
The number is lines in 3+-line `//` runs.

| Boss | Doc | C++ | Python checker (`tools/botobs/bosses/`) |
|---|---|---|---|
| YoggSaron | yogg-saron.md | 1354 | yogg_saron.py (149 `#` + 294 docstring) |
| Mimiron | mimiron.md | 1121 | mimiron.py (9 + 45) |
| Thorim | thorim.md | 633 | — |
| Hodir | hodir.md | 588 | hodir.py (0 + 47) |
| Freya | freya.md | 452 | — |
| XT002 | xt002.md | 420 | — |
| FlameLeviathan | flame-leviathan.md | 393 | flame_leviathan.py (43 + 98) |
| IronAssembly | iron-assembly.md | 362 | — |
| Vezax | vezax.md | 227 | general_vezax.py (3 + 39) |
| Algalon | algalon.md | 157 | algalon.py (0 + 203) |
| Ignis | ignis.md | 128 | — |
| Auriaya | auriaya.md | 104 | — |
| Kologarn | kologarn.md | 36 | — |
| Razorscale | razorscale.md | 29 | — |
| Shared | README.md | ~150 | — |

Shared = `UldStrategy.cpp`, `UldHardMode.{h,cpp}`, `UldBotScripts.cpp`, `UldData.h`,
`UldActions_Shared.*`, `UldMultipliers_Shared.*`.

Other sessions edit this tree concurrently, Yogg-Saron above all. Before a boss, `git status` its
files; any dirty → skip it and come back later.

## Per-boss cycle

1. **Inventory** every comment run of 3+ lines in the boss's files, `#` runs and docstrings of 3+
   lines in its checker, and shorter comments carrying evidence. Run-finder:
   `awk '/^[ \t]*\/\//{r++;next}{if(r>=3)print FILENAME":"FNR-r;r=0}' <files>`.
2. **Sort each sentence** of each block:
   - **Keep** (≤2 lines): the invariant or non-obvious why the next editor needs: engine or spell
     quirks, ordering hazards, why this API and not the obvious one (e.g. "GetExactDist2d, not
     GetDistance2d: the latter subtracts both object sizes"). A short fact explaining a constant's
     value stays too ("DBC 15 yd plus both object sizes, 16.2 at the furthest").
   - **Move** to the doc: evidence (sample, pull and hit counts, medians, trace ids), rejected
     alternatives with their measured cost, failure records ("eight melee sat still for 13 s"),
     design explanations that need more than 2 lines.
   - **Drop**: restating the code, history with no durable lesson ("the old model", "used to"),
     anything the doc already says.
3. **Doc first, then code.** Grep the doc for each Move item. Already there → only cut the comment.
   Missing → add it to the section covering that mechanic, naming the constant or action so the two
   match up (the docs already do this, e.g. `…_NOVA_EXPECTED_DAMAGE` in yogg-saron.md). Follow
   `docs/README.md`: verified facts, decisions with rationale, traps, open gaps; no change lists.
   Checker facts about reading this boss's trace go to the doc's `## The trace` section (create it if
   missing, as in algalon.md); facts about RaidObs in general go to `docs/systems/observability.md`.
4. **A kept comment never points at the doc** ("see yogg-saron.md"); it states the fact itself.
5. **Python module docstring**: keep line 1 (it is argparse's description, `run_sections` in
   `tools/botobs/raidobs/cli.py:22`); drop the usage block, which duplicates the `SECTIONS` help text
   and has drifted from it; move the prose after it per step 2.
6. **Skills, invoked fresh each boss cycle**: `/compact-docs-writer` before editing the doc;
   `use-conversational-language` before rewriting comments and writing the commit message.

## Verification (per boss, gates the commit)

- **Only comments changed.** A scratchpad script compares `git show HEAD:<file>` with the working
  file after stripping comments. C++: drop `//` and `/* */` outside string and char literals, collapse
  whitespace. Python: `ast.dump` with docstrings removed, plus the module docstring's line 1 compared
  on its own. Any difference → fix before going on.
- `~/.claude/scripts/pb-syntax-check.sh` on each changed `.cpp` and every `.cpp` including a changed
  `.h`.
- `tools/pblint/pblint.py <touched paths>` and `python apps/codestyle/codestyle-cpp.py` from the
  module root.
- Python touched → `python -m unittest discover -s tools/botobs/tests`, and `--help` once per touched
  checker.
- No nativetest includes an Ulduar header, so `tools/nativetest/run.sh` is not needed.
- **Done check**: rerun the inventory; no 3+-line comment run is left in the boss's files.

## Commit (per boss)

Stage only that boss's `src` files, its checker, its doc (and `observability.md` if touched) and this
plan's Status line. Subject ≤72 chars, e.g. `Yogg-Saron: move the measurements from the comments
into the docs`, written to a scratchpad file and committed with `git commit -F`; verify with
`git log -1 --format=%B`. No AI trailer, no amend, no push.

## Status

- [x] Yogg-Saron
- [x] Mimiron
- [x] Thorim
- [x] Hodir
- [x] Freya
- [x] XT-002
- [x] Flame Leviathan
- [x] Iron Assembly
- [x] General Vezax
- [ ] Algalon
- [x] Ignis
- [x] Auriaya
- [x] Kologarn
- [x] Razorscale
- [ ] Shared files → README.md; then delete this plan
