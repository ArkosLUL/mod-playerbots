# r7-rotface — Rotface

Wave 3c, parallel with `r6-festergut`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `rotface`, prefix `rf.`, boss id 5, stem `Rotface`. Core `boss_rotface.cpp`. Doc `rotface.md`,
reader `rotface.py`. Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1032 lines (`ICCActions_RF.cpp`).
- The most coordinates in ICC: 35 named positions and 3 Z constants, none navprobed. They include
  25 explosion slots and a 22-spot heroic grid.
- What is handled today:
  - the main tank to the centre;
  - the Big Ooze taunt and kite ring;
  - ooze merge routing;
  - the heroic Vile Gas run (the spot resets after 2 s idle).
- One icon write. Nothing holds a Big Ooze but the taunt; heal threat can pull one, and taunts meet
  DR.
- `FindAssignedBigOoze` has no caller.
- `IccRotfaceMultiplier`:
  - with Vile Gas, it vetoes everything but the avoid action;
  - always: TankAssist, Formation, AvoidAoe, Flee (non-hunters), BlinkBack, Army of the Dead, the
    whole cure and cleanse family, the assist tank's AttackRti, TankAssist and all taunts;
  - while a Big Ooze casts its explosion: MovementAction and group position;
  - it calls `AttackStop` and `SetTarget(Empty)` inside `GetValue`.
- The Vile Gas listener in `ICCScripts.cpp` notes `rf.vilegas` on the victim (heroic, through
  `GetSpellIdForDifficulty`).
- No cheats left: `rf.nitro` and `rf.spitefulfury` were removed.

## Guide answers to build on

- 2 tanks: one on the boss, one kiting the Big Ooze.
- An 8 yd spread.
- A Mutated Infection victim's small ooze goes to the kiter.
- The Big Ooze is kited, and everyone shifts 6 yd on the Unstable Ooze Explosion. Move out of Slime
  Spray.
- Heroic: Vile Gas from Putricide, a stronger infection, Ooze Flood slows.
- Setup: 3-5 healers.

## Focus

- The infected raider's walk to the kiter, and dispel timing.
- The kiter's path, computed and navprobed instead of 35 fixed points where possible.
- The Slime Spray, explosion and Ooze Flood dodges on hazard models.
- The Vile Gas spread.
- Replacing the multiplier's family vetoes and the side effect in `GetValue`.
- Direct targeting.

## Owns

Stem `Rotface` and its definition, the doc, the reader and its test, this brief.

## Task list

## Decisions for review

## Carried over

- From `r0a-restructure`: `spellEntriesFlood` (`Util/ICCHelpers_Rotface.h`) has no user, and
  `Multiplier/ICCMultipliers_Rotface.h` keeps the commented-out `IccRotfaceGroupPositionMultiplier`.

## Known gaps

## Blocked
