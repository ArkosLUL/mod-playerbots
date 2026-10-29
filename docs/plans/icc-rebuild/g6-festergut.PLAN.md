# g6-festergut — close Festergut's and Stinky and Precious's gaps

Gap wave `3cd-gaps`, after `3g`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r6-festergut` lane and its review: read
[r6-festergut.PLAN.md](r6-festergut.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/festergut.md`,
`docs/raids/icecrown-citadel/stinky-and-precious.md` and r6's brief.

- The resist-hook bullet is stale: his `HookRows()` row is in `src/Ai/Raid/ICC/Util/ICCSeams.cpp`
  (`be0531146`). Confirm and drop it.
- Precious's zombies: `build-icc-wt/_guides/text/stinky-and-precious.blog.md` and the core script decide
  who holds, spreads or kills them.

## Owns

Stems `Festergut` and `Dogs` and their definitions, both docs, the readers `festergut.py` and
`stinky_and_precious.py` and their tests, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
