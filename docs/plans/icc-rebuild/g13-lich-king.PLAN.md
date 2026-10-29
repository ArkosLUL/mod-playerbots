# g13-lich-king — close the Lich King's gaps

Gap wave `3efg-gaps`, after `3cd-gaps`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap
lanes" section included. Builds on the merged `r13-lich-king` lane and its review: read
[r13-lich-king.PLAN.md](r13-lich-king.PLAN.md) first. Its decisions stand unless closing a gap needs
a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/the-lich-king.md` and in r13's brief.

- Neither floor is on the navmesh, so navprobe checks nothing there. A stand the lane adds or moves
  stays inside r13's always-solid envelope, and sight stays unchecked.
- The 25-man and heroic ranged winter cluster against the spheres' paths: with no pull, the script's
  sphere speed and target choice decide, and the choice goes under Decisions for review.
- One bot tank: Raging Spirits, the adds and the Soul Reaper swap. The Gap lanes' one-bot-tank rule
  applies.

## Owns

Stems `LichKing` and `LichKingLate` and the definition, the doc, the reader `the_lich_king.py` and its
test, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
