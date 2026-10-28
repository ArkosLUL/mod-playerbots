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

One lead, no split: helpers, nodes and reader share one state.

1. [x] `Util/ICCHelpers_Marrowgar.{h,cpp}` (new `.cpp`): ids, navprobed spots, boss by entry, phase from
   the cast, duty roster, stand latches, re-centre, resolver, Coldflame model, tick, opener hook.
2. [x] Triggers and actions: `icc lm target` / `attack target`, `icc lm stand` / `move to stand`,
   `icc lm coldflame` / `dodge coldflame`. Old icon nodes and positions removed.
3. [x] Definition: rows, `OwnTargeting`, two `OwnMovement`s, spell-mover and `avoid aoe` blocks, tick.
4. [x] Probes `lm.phase`, `lm.duty`, `lm.anchor`, `lm.stand`, `lm.spike`, `lm.dodge`, haz 72705; reader
   and tests rewritten.
5. [x] Boss doc rewritten.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Non-baits stand within 3 yd of his centre, on a 160° arc of his back (reviewer: the full ring put half the melee in his parry arc) | Core: a line starts 10.1 yd out, 4.5 yd reach; Wowhead: non-baits in melee | Default reach at 22.5 yd, 0.25 yd inside Coldflame's exclusion |
| Baits 3 / 8 bots, hunters always, then warlock, priest, druid, shaman, mage; healer only with no ranged dps; ring 31 yd, 20° apart | WT and Wowhead counts, Wowhead class picks; core needs one | Count humans; healers before casters |
| Bone Slice: off-tanks 2 yd beside the main tank, all tanks stack | Core 5 yd split (guides say 8) | Only 1 off-tank in 10-man (max 2 targets) |
| Review: storm anchors 34 yd at 180/60/300 round a fixed stack; every tank on 180, 2.6 yd from the tank spot, the bait with the nearest ring slot on 60 and 300 | Core: his first charge, 3.05 s into the cast, takes the farthest (25: one of the two), 60 or 300 from his spot; WT tanks and hunters out; Wowhead triangle 30-40 yd; 34 for heal range | The lane's 0/120/240 with the main tank on 120 and an off-tank on 240: 32.6 yd walks, so a 25-man first charge can land on one mid-walk, or on the stack, and cross the stack; 40 yd: out of heal range |
| Storm stands from the 3 s cast | Core cast time | From the aura: 3 s less to move |
| Melee hold the stack in storms | Conservative; Wowhead allows chasing | Melee chase with the dodge |
| Main tank re-centres him on his spawn after storms (pull point 18 yd past it, 3 tries) | None; keeps the layout fixed | Layout round wherever he settles |
| Taunt-back left to the class "lose aggro" nodes | Generic tank logic already does it | Own taunt node |
| Everyone dodges, tanks too | Only fallback lines reach tanks, and a 7 yd step keeps him in place | Tanks eat it |
| No pre-pull staging | Conservative: walking near him pre-pull risks the pull | Staging spots |
| Opener hook in the stem, its `HookRows()` row added by the merge stage (`30ecb644c`) | Lane rules: `HookRows()` is the merge stage's | No opener hold |
| Review: the tick puts a bot's saved rti of cross or star back to skull, once per instance | Convention 10; the old spike duty set them and RtiValue saves them | `RaidRestoreLegacyRti` in the root once no ICC code writes rti (merge stage), which misses star |

## Carried over

- `ICCData.h` keeps `NPC_SPIKE*_10H/_25N/_25H`, which nothing reads now; r14 can drop them.
- `IccLmWalkAction` copies ToC's `NorthrendBeastsWalkAction`; r14 can hoist one raid-wide walk latch.

## Known gaps

- No 631 trace on disk: nothing here has been seen in a pull.
- No Berserk pacing, Bone Storm raid cover or tank defensive on his arrival.
- No Hand of Protection on an Impaled player (Wowhead: it stops the physical ticks).
- Heroic: a caster reaching for a spiked anchor during a storm walks into his lanes.
- Humans hold no duty; one standing far out draws storm charges.
- Line of sight unchecked: this navprobe build has no `los`.
- The dodge's walk is not tested against other lines (reviewer): every line radiates from him, so a
  sideways step crosses none.
- Never built: CMake re-run needed (new `Util/ICCHelpers_Marrowgar.cpp`); no `-Werror` pass.

## Blocked
