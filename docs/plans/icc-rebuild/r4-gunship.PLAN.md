# r4-gunship — Gunship Battle

Wave 3b, parallel with `r5-saurfang`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `icecrown-gunship-battle`, prefix `gsb.`, boss id 2, stem `Gunship`. Core
`boss_icecrown_gunship_battle.cpp`. Doc `icecrown-gunship-battle.md`, reader `icecrown_gunship_battle.py`.
Both factions' ships. Guides in the cache: Warcraft Tavern 25 and 10, instant-carry, Wowhead if
saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1067 lines (`ICCActions_GSB.cpp`), with 9 `FindNearestCreature` lookups.
- Icons: the skull on the captain or mage for the boarders, and the star on axethrowers (5 writes).
- What is handled today:
  - cannons and the rocket pack;
  - the main tank jumps to the captain;
  - the assist tank holds the deck and collects boarders.
- `IccGunshipMultiplier` vetoes Formation, Follow and the main tank's AttackRti. More than 30 yd from
  all three jump points, it vetoes everything but the rocket jump.
- The cheat `gsb.battlefury` (`IccGunshipRocketJumpAction`) strips Battle Fury off the enemy captain.
  The code comment's core-bug claim does not hold (doc).
- The fall snap (`NearTeleportTo` with `removeTransport`) is borderline. The core's own rescue already
  saves deeper falls.
- 8 positions, all on transport decks, which navprobe can't check: document them as such.
- The 100 yd and 200 yd radii have no core value behind them.

## Guide answers to build on

- The heroic changes no mechanic, only numbers.
- **Setup:** 2 tanks (one boards, one defends), up to 4 healers with 2 at home, 4 cannoneers of any
  role, ranged at home.
- **Boarding:** the boarders follow the tank across, kill the mage (Below Zero freezes the cannons),
  and return at once.
- **Battle Fury:** +12% damage per stack, 20 s, refreshed with each stack. Staying too long kills the
  tank; leaving the ship lets the stacks fall off.
- **Cannons:** manage heat (Overheat).

## Focus

- The cannon duty with heat.
- The boarding party's timing and a return home that replaces `gsb.battlefury`.
- The home deck's ranged kill of riflemen, axethrowers and mortar.
- The assist tank's boarder pickup.
- Direct targeting instead of skull and star. The fall snap under the cheat rule.

## Owns

Stem `Gunship` and its definition, the doc, the reader and its test, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
