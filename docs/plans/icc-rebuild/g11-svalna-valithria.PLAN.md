# g11-svalna-valithria — close Sister Svalna's and Valithria Dreamwalker's gaps

Gap wave `3efg-gaps`, after `3cd-gaps`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap
lanes" section included. Builds on the merged `r11-svalna-valithria` lane and its review: read
[r11-svalna-valithria.PLAN.md](r11-svalna-valithria.PLAN.md) first. Its decisions stand unless
closing a gap needs a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/sister-svalna.md`,
`docs/raids/icecrown-citadel/valithria-dreamwalker.md` and r11's brief.

- The Svalna gauntlet is in scope (the overview's Scope row), though r11 left its Ymirjar waves on the
  generic AI. Build it per the boss lane recipe. Her boss state stays `NOT_STARTED` until she engages,
  so it needs its own gate, as `UnitGate` gives Stinky and Precious (`ICCDefinition_Dogs.cpp`). It also
  needs a trace slug per the ICC README's "Boss states and trace names" (the Blood Prince trash's
  `RaidObs::MarkPull` in `ICCScripts.cpp` is the pattern for a pull with no boss state), a reader and
  its tests. Sources: the core script for the waves and their spells, and
  `build-icc-wt/_guides/text/sister-svalna.blog.md`.
- Valithria's healer cooldowns (Guardian Spirit, and Beacon of Light or Earth Shield on her): no shared
  dealer exists for a single-target cooldown, so the answer is ICC-local.
- A new definition or stem file is this lane's. Its registration (`ICCDefinitions.h`, the root
  strategy's `InitMultipliers`, the umbrella headers, `ICC_PREFIXES` in
  `tools/botobs/raidobs/encounter.py`, `tools/botobs/tests/test_icc_naming.py`) goes in Carried over for
  the merge stage, as `r12-sindragosa` did.

## Owns

Stems `Svalna` and `Valithria` and their definitions, any gauntlet stem and definition this lane adds,
both docs, the readers `sister_svalna.py` and `valithria_dreamwalker.py` (plus any gauntlet reader) and
their tests, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
