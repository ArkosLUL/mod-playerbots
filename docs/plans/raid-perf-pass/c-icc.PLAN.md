# c-icc: ICC

Lane of [raid-perf-pass.PLAN.md](raid-perf-pass.PLAN.md) (wave 2, branched from Custom after wave 1
merged, beside `b-anub-twins-jarax`). The overview's neutrality rule, file ownership, git, Docker and
verification rules bind this lane.

Implement the overview's lane `c-icc` items: the splash cache at the `IsRaidAreaDamage` sites and
Sindragosa's, `char const*` overloads for the note wrappers with `Active()` guards on pure argument
builds, and N3 `IccGetCreaturesByEntries` in one grid visit. Counts and names there (15 sites, 13
wrappers) predate later ICC work; re-count on Custom.

- Wave 1 merged the splash cache API (lane `a-shared-fc`); read its commits on Custom first and use
  it as merged.
- Leave `IccLichKingSplashMultiplier`'s own cache and `IccDbsAreaDamage`'s order as they are.
- An `Active()` guard is allowed only where every skipped argument is pure; a skipped call that
  touches a memo, a Value or any state is a behavior change.
- N3 keeps the identical per-entry predicate, range and the result order callers see; prove it with
  a harness against the per-entry calls.
- ICC has no baseline measurement; the proofs carry the lane.

One commit per fix, each compiling (`pb-check` over its paths), harness files in
`build-toc-wt/perf/_harness/c-icc/`, never committed.

## Task list

Sites re-verified against Custom `3175a814b` (wave 1 merged). Lead implements all of it (no split: mechanical
edits). Harnesses in `build-toc-wt/perf/_harness/c-icc/`, old bodies from `git show 3175a814b:<file>`.

1. **S1** splash cache at the 15 `IsRaidAreaDamage(botAI, action)` sites (`ICC/Multiplier`): BPC splash + brake,
   BQL brake + splash, LDW + LDW brake, Dogs yield, Festergut, LK brake, Putricide, Rotface ooze splash, DBS area
   damage, Sindragosa brake, drake brake, Valithria splash. Each class gets a `RaidSplashCache _splash` member and
   calls the cache overload at the same point; nothing else moves (DBS keeps its order). Proof: `s1_textual.py`
   (each hunk is exactly the one-call swap, member and include) + the A0 splash harness rerun from the worktree.
2. **N3** `IccGetCreaturesByEntries` (`ICCHelpers_Shared.cpp:33`): one grid visit (the single-entry core call for
   one entry, the multi-entry overload otherwise), then hits taken entry by entry in list order, visit order inside
   an entry, so the pre-sort vector equals the old concatenation; sort and unique unchanged. Proof: harness `n3`
   (old vs new bodies over enumerated worlds and entry lists incl. repeats and colliding guids; compares output and
   the `IsAlive` call sequence; reds).
3. **NO** `char const*` overloads of the note wrappers taking `std::string const&` (BPC, BQL, LK, Putricide,
   Rotface, Sindragosa, drakes `Note`, Svalna, Valithria helpers; Putricide action `NoteDerived`, Marrowgar and
   Rotface action `NoteDodge`): a literal or `char const*` value builds its `std::string` only once `Active()` holds.
   Proof: review (overload resolution per caller; every non-literal `char const*` value proven non-null, since
   `std::string(nullptr)` throws).
4. **NG-*** `Active()` guards on pure value builds outside an existing guard: LK `ResolveTarget` `Tag()` (4),
   BPC (6), Sindragosa (`UpdateWalks`, `ResolveTarget`: 5), drakes (2), Putricide helper (1) + action (3), Rotface
   action (3), Marrowgar action (1). One commit per file group. Proof: review (every skipped call pure).
5. **SG-tomb** Sindragosa `KindOf`: check whether a splash cache gains anything (it doesn't, see dropped).

## Candidates done

Harnesses: `build-toc-wt/perf/_harness/c-icc/` (`python -I run.py [splash|n3]`, `python -I s1_textual.py [red]`,
`python -I no_callers.py <scratch dir>`); old bodies cut verbatim from `3175a814b` by `extract.py`, new from the
worktree, every red variant must fail.

| id | site | why neutral | proof | commit |
|---|---|---|---|---|
| S1 | the 15 `IsRaidAreaDamage` calls in `ICC/Multiplier` (BPC splash + brake, BQL brake + splash, LDW + brake, Dogs yield, Festergut, LK brake, Putricide, Rotface ooze splash, DBS area damage, Sindragosa brake, drake brake, Valithria splash) | one-call swap to the cache overload at the same point, a `RaidSplashCache` member per multiplier instance (one bot's engine), nothing else moved; the overload gives the same answer and the same `"spell id"` create/`Get()` sequence as the old form | `s1_textual.py`: 15 swaps, 15 members, 0 other changes, 2 reds fail; harness `splash` rerun from the worktree: 600,668 calls, 0 mismatches, 4 reds fail; pb-check | `16ebe62fe` |
| N3 | `IccGetCreaturesByEntries` (`ICCHelpers_Shared.cpp`) | both core overloads run the same `CreatureListSearcher` over the same cell visit with the same `IsWithinDist` and phase tests; hits taken entry by entry in list order, visit order inside an entry, rebuild the old pre-sort vector exactly, so sort and unique return the same vector even with tied guids; `IsAlive` runs in the same order; `Map::Visit` loads no grid | harness `n3`: 4,557,800 calls (every world of up to 3 creatures × every entry list of up to 3 incl. repeats and the empty list, then 40k random worlds of 4–9 creatures with lists of up to 4), output and `IsAlive` sequence compared, 0 mismatches; 4 reds fail | `2faf77224` |
| NO | `char const*` forms of the note wrappers in BPC, BQL, LK, Putricide, Rotface, Sindragosa, drakes, Valithria, Putricide/Marrowgar/Rotface actions; Svalna's wrapper now takes `char const*` only | the same `std::string` reaches `RaidObs::NoteDerived` when a trace is open, built after the one `Active()` check instead of before it; `std::string(nullptr)` throws, so every non-literal `char const*` value was checked non-null: BPC phase and `SpellName[0]` (DBC strings point into the string table), PP swap kind when elected, PP kite branch when found, Svalna `GauntletName`/`cast.note`/danger, drake name tables (full), `electedSpell`, VD `TierName`/freedom label/snare/danger, SG `StandName`/cover name | `no_callers.py` (each new overload marked deprecated in a scratch copy, compiled by pb-syntax-check): 150 call sites, every non-literal one reviewed as above; pb-check | `7537d24ac` |
| NG-LK | LK `ResolveTarget`: 4 `Tag()` notes | `Active()` guard; skipped `Tag`, `GetGUID`, `to_string` and string concat are pure; `rule` is always a literal (every Pick assigns literals), `lateRule` falls back to `"late"` | review; pb-check | `836a5f733` |
| NG-BPC | BPC `ResolveTarget` nucleus hit, `GetBpcNucleusToTaunt`, `GetBpcBallWalk` (4) | skipped `GetAuraCount` (const read), `to_string`, concat with the literal `kind` are pure | review; pb-check | `fcfead91f` |
| NG-SG | SG `UpdateWalks` hide note (early return as its last statement), `ResolveTarget` skull/air/break `Tag()` notes | skipped guid tests, `Tag`, `rule.c_str()` are pure; `AirTombPick` and `PickTomb` still run before the guard | review; pb-check | `d9dd67dae` |
| NG-SGA | drakes stand note, owned target note | skipped `RoleName`, name tables, `DrakeIndex` (entry read) are pure; the index is valid on both paths | review; pb-check | `057b91e1c` |
| NG-PP | PP `ShareHealthLeftPct` step note; dodge action none/locked/moved notes | skipped arithmetic, `lround`, `to_string`, concat pure; `kind` non-null past the early return; `travel` still measured before `WalkTo` | review; pb-check | `f6a695336` |
| NG-RF | Rotface dodge action none/locked/moved notes | as NG-PP | review; pb-check | `b1d917862` |
| NG-LM | Marrowgar dodge action moved note | `branch` is a literal from the attempt table; the note build is pure | review; pb-check | `ab35bf3fd` |

## Candidates dropped

- SG-tomb (`IccSindragosaTombMultiplier::KindOf`): it already caches the whole kind per action, so `SpellSplashes`
  and its `"spell id"` reads run once per action for the multiplier's life; a splash cache gains nothing.
- LK late wrapper overload (`ICCHelpers_LichKingLate.cpp`): every `char const*` caller sits inside `WriteNotes`'
  `Active()` guard, so an overload saves nothing.
- Guards on Valithria `vd.cloud` (runs only when the cloud changes) and LK late `lk.siphon` (once per taunt): no
  measurable gain.

## Decisions for review

- A guard in front of a wrapper that checks `Active()` again keeps the existing race: a trace opening on another
  thread between the two loads loses that one note, as with every `if (Active())` guard (wave 1's OBS decision).
- N3 keeps the per-entry grouping although the result is sorted by guid: distinct creatures never share a guid, but
  the grouping makes the equivalence exact without relying on that.
- Svalna's wrapper had no `std::string` caller left, and an unused overload in an anonymous namespace fails CI's
  `-Wunused-function`, so its parameter changed to `char const*` instead of gaining a sibling.
- The 15-site count matched Custom; of the 13 wrappers, 11 gained an overload, Svalna's was swapped and LK late's
  dropped. The guards also cover the drakes and Marrowgar, beyond the overview's five named files: same pattern,
  same proof.

## Known gaps

- NO and NG-* are proven by review only, as the overview allows for trace gates; S1's per-site half is textual.
- Harness `n3` models the grid visit as a visit-ordered list filtered by each overload's entry test; the real
  `Cell::VisitObjects` is shared by both overloads and not exercised.
- ICC has no baseline pmon, so the gain is unmeasured.
- Not attempted (outside the overview's list): the splash multipliers' `CastHealingSpellAction` cast ahead of the
  cached test (`_splash.Of(action).heal` would share its lookup), and BPC `DirectHit`'s per-action `"spell id"`
  lookup (`RaidSpellIdRef`).
- Subject of `057b91e1c` is 73 chars, one over the limit; it stays, since the lane may not amend.

## Blocked
