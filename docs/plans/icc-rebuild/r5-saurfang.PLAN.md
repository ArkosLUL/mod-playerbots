# r5-saurfang — Deathbringer Saurfang

Wave 3b, parallel with `r4-gunship`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `deathbringer-saurfang`, prefix `dbs.`, boss id 3, stem `Saurfang`. Core
`boss_deathbringer_saurfang.cpp`. Doc `deathbringer-saurfang.md`, reader `deathbringer_saurfang.py`.
Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 385 lines (`ICCActions_DBS.cpp`).
- What is handled today:
  - the Rune of Blood swap;
  - beast taunts;
  - a 3×5 ranged grid (`dbs.slot`, where 0 means both slot 0 and erase);
  - the melee skull on beasts (`IccEnsureIconOn`).
- `IccAddsDbsMultiplier` vetoes the AoE family, Army of the Dead, Formation, Follow, Flee and ranged
  ReachSpell. A tank with Rune of Blood gets everything but MovementAction vetoed, taunts included.
- The class taunt reaches one unit per 8 s, so a 25-man wave of five Blood Beasts sits on the raid
  for about 8 s a beast.
- Heroic Scent of Blood is not covered.
- No cheats. One position.

## Guide answers to build on

- 2 tanks swap on Rune of Blood.
- 3-6 healers, one healer per Mark of the Fallen Champion.
- A 12 yd spread for Blood Nova.
- Slows on the beasts, and ranged kill them. No Army of the Dead: it feeds Blood Power.
- Heroic: the beasts generate Blood Power, and a Marked raider's death heals him 20% (5% on normal).

## Focus

- Beast pickup without a taunt loop: a ranged kill duty plus slows, roots and stuns from the kits on
  hand, and Scent of Blood on heroic.
- The Mark heal assignment and the Boiling Blood heals.
- The Blood Nova spread.
- The Rune swap on the real taunt.
- Direct targeting.

## Owns

Stem `Saurfang` and its definition, the doc, the reader and its test, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
