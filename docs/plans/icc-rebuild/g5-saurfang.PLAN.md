# g5-saurfang — close Deathbringer Saurfang's gaps

Gap wave `3ab-gaps`, after `3b`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r5-saurfang` lane and its review: read
[r5-saurfang.PLAN.md](r5-saurfang.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/deathbringer-saurfang.md` and in r5's
brief, plus the two every gap lane does (tank defensives, line of sight).

- Tank defensives: the doc names high Blood Power and Frenzy; check the script for every other
  predictable tank spike, Rune of Blood included.
- Line of sight: all 11 slots, both tank spots and the stack against him, and the healer slots
  against the tank spots.
- "A bot counted a tank by spec alone (a Thick Hide cat)" is not a gap: the user ruled it out. The spec
  test is the shared `PlayerbotAI::IsTank` (`src/Bot/PlayerbotAI.cpp`), not ICC code, and both cat
  presets in `conf/playerbots.conf.dist` (`PremadeSpecLink.11.3.*` and `.11.5.*`) put no point in
  Thick Hide. Remove the bullet from the doc and r5's brief, put the README's matching line in
  `readmeNotes`, and leave the ICC tank test as it is.

## Owns

Stem `Saurfang` and its definition, the doc, the reader and its test, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
