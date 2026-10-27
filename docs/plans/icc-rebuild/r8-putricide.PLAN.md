# r8-putricide — Professor Putricide

Wave 3d, parallel with `r9-blood-prince-council`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout". It may split.

## Encounter

Slug `professor-putricide`, prefix `pp.`, boss id 6, stem `Putricide`. Core
`boss_professor_putricide.cpp`. Doc `professor-putricide.md`, reader `professor_putricide.py`. Guides
in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1698 lines (`ICCActions_PP.cpp`). The phase comes from health thresholds
  (`IccPutricidePhase`).
- 6 icon writes (`PutricideSetSkull`, 5 callers).
- A hand-rolled flee vector for Gaseous Bloat.
- `IccAddsPutricideMultiplier` vetoes:
  - in the abomination vehicle, everything but the abomination action;
  - for a tank in follow motion, everything but Follow and the goo dodge;
  - with Gaseous Bloat, everything but the cloud and puddle actions (healers exempt);
  - with Unbound Plague, everything but the goo dodge;
  - for the tank with more stacks, everything but movement and the plague action;
  - always: Flee, Formation, Disengage and BlinkBack.
  - One branch can never be true: Volatile Ooze can't also be the goo dodge or the puddle action.
- The gas cloud trigger erases `ppBloatLastDir` inside `IsActive`.
- Cheats:
  - `pp.oozekill` and `pp.gascloudkill` (`IccPutricideVolatileOozeAction`,
    `IccPutricideGasCloudAction`) kill every extra add of the entry;
  - `pp.unboundplague` (`HandleUnboundPlague`) is a suicide with nobody near; the doc notes bots do
    the opposite of passing it;
  - `pp.freeooze` (`TryRegurgitate`) casts Regurgitated Ooze with no energy cost. With the cheat off,
    a cast with no energy still stamps its cooldown and claims the tick.
  - Buffs: `pp.heroicbuff` and `pp.spitefulfury` (keep; `r1` moved them out of the trigger).
- 3 positions.

## Guide answers to build on

- **Volatile Ooze:** everyone stacks on the Adhesive target, and it is the top DPS priority.
- **Gas Cloud:** kited for its whole duration. Gaseous Bloat starts at 10 stacks and loses one per
  tick; at 0 the cloud picks a new target. Also top DPS priority.
- **The abomination:** the off-tank drinks the potion. Eat Ooze gives 4 energy each time a puddle
  shrinks. Regurgitated Ooze costs 45 energy and slows its target 50% for 20 s, good on Gas Clouds.
  Mutated Slash stacks armor reduction on the boss.
- **Mutated Plague:** tank swaps. In phase 3, DPS who can taunt share it so nobody reaches 4 stacks.
- **Heroic Unbound Plague:** a line of ranged DPS passes it along, about 10 s each, 6 players. It
  passes within 5 yd, and Plague Sickness stops passing it back. The guide's alternative is to let
  the carriers die and battle-res them.
- **Heroic intermissions:** an ooze and a gas cloud, with Ooze and Gas Variable splitting the DPS.
  Normal intermissions are 15 s of roleplay.
- Dodge Malleable Goo and the Choking Gas bombs.
- **Setup:** 2-3 tanks, 4-6 healers.

## Focus

All of the above on every difficulty:
- the ooze stack and the cloud kite, each with a priority kill;
- the abomination's puddle-eating duty cycle for its energy;
- the plague swap and sharing;
- the heroic plague pass line and the Variables split.

Replace the four cheats and the family vetoes, and use direct targeting.

## Owns

Stem `Putricide` and its definition, the doc, the reader and its test, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
