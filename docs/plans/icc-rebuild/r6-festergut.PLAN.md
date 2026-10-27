# r6-festergut — Festergut, Stinky and Precious

Wave 3c, parallel with `r7-rotface`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounters

- **Festergut:** slug `festergut`, prefix `fg.`, boss id 4, stem `Festergut`. Core
  `boss_festergut.cpp`. Doc `festergut.md`, reader `festergut.py`.
- **Stinky and Precious:** slug `stinky-and-precious`, prefix `dogs.`, no boss id (the `r1` unit
  gate), stem `Dogs`. Core `icecrown_citadel.cpp`. Doc `stinky-and-precious.md`, reader
  `stinky_and_precious.py`.
- Guides in the cache:
  - Festergut: Warcraft Tavern 25 and 10, Wowhead if saved;
  - the dogs: the battlechicken blog.

## Facts as of `824468439`

Sites are named by function.

- **Festergut:**
  - The action stem was 530 lines (`ICCActions_FG.cpp`).
  - What is handled today:
    - a tank swap at 6 Gastric Bloat;
    - spore stacking with a raid-wide spot flip;
    - the heroic Malleable Goo dodge;
    - a ranged grid.
  - Vile Gas is answered only by 10 yd spacing. A hand-rolled flee vector sits in the goo dodge.
  - `IccFestergutMultiplier` vetoes Formation, Follow, Flee, Disengage and BlinkBack, plus:
    - a tank at 6 Gastric Bloat: everything but movement;
    - a Gas Spore carrier: all MovementAction, Flee and ReachSpell;
    - the goo-wait window: all MovementAction except the avoid action.
  - The goo-avoid trigger writes `festergutGooWaitUntil` inside `IsActive`.
  - The Malleable Goo listener in `ICCScripts.cpp` notes a 12 yd, 8 s hazard circle per impact.
  - 4 positions.
- **Dogs:**
  - 70 lines: a tank swap at 8 Mortal Wound, and a taunt on adds within 20 yd.
  - `IccDogsMultiplier` vetoes everything but MovementAction for a tank at 8+ stacks, plus Formation
    and Follow.
  - Decimate has no answer.

## Guide answers to build on

- 2 tanks swap at 9 Gastric Bloat.
- 3 spore groups (tanks, melee, ranged) for Inoculated.
- Ranged spread 8 yd for Vile Gas.
- Heroic: Malleable Goo every 10 s.
- Setup: 3-5 healers.
- Dogs, from the blog: a Mortal Wound swap, and heals ready for Decimate.

## Focus

- The spore groups and Inoculated stacks.
- The Gastric Bloat swap threshold (6 now, 9 in the guide: settle it from the script).
- The Vile Gas spread, the Malleable Goo dodge on the hazard model, and defensives for Pungent Blight.
- The dogs' swap and Decimate recovery.
- Direct targeting.

## Owns

Stems `Festergut` and `Dogs` and their definitions, both docs, both readers and their tests, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
