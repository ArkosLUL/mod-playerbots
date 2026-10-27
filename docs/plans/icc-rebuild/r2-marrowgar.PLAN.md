# r2-marrowgar — Lord Marrowgar

Wave 3a, parallel with `r3-deathwhisper`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `lord-marrowgar`, prefix `lm.`, boss id 0, stem `Marrowgar`. Core: `boss_lord_marrowgar.cpp`.
Doc `docs/raids/icecrown-citadel/lord-marrowgar.md`, reader `tools/botobs/bosses/lord_marrowgar.py`.
Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function; line numbers moved with `r0a`.

- The action stem was 565 lines (`ICCActions_LM.cpp`).
- Targeting is by icon: 12 writes and 8 reads, the skull on the first spike. Spike duty comes from guid
  buckets (`IccBalancedGroupSizes`, `IccAssignedBucketIndex`).
- Bone Storm anchors are the two lowest-guid ranged (`PickBoneStormRangedTargets`).
- Three named positions plus one inline, none navprobed.
- No cheats and no multiplier. The tank trigger runs `IccStripExperienced`, which `r1` moved.
- Heroic: Bone Spike Graveyard during Bone Storm is not handled.

## Guide answers to build on

Warcraft Tavern 25:
- Bone Slice splits between up to 3 tanks within 8 yd: 2-3 tanks stay stacked and move together.
- Coldflame runs from his hitbox toward a raider outside it. At least 30% of the raid (8 in 25, 3 in
  10) stays outside the hitbox so the lines aim away from the melee.
- Melee cleave spikes near them, ranged swap to far spikes.
- During Bone Storm the raid spreads, with tanks and hunters far away.
- Heroic: spikes continue through Bone Storm.
- Setup: 4-5 healers.

## Focus

- Spike kill duty by direct targeting, no icons.
- A Coldflame hazard model and dodge.
- The outside-hitbox count, Bone Storm spread and movement, and the heroic spikes during the storm.

## Owns

Stem `Marrowgar` files and its definition, the doc, the reader and `tools/botobs/tests/test_lord_marrowgar.py`,
this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
