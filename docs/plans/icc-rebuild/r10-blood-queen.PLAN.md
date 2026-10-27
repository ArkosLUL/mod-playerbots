# r10-blood-queen — Blood-Queen Lana'thel

Wave 3e, parallel with `r11-svalna-valithria`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

Slug `blood-queen-lana-thel`, prefix `bql.`, boss id 8, stem `BloodQueen`. Core
`boss_blood_queen_lana_thel.cpp`. Doc `blood-queen-lana-thel.md`, reader `blood_queen_lana_thel.py`.
Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1286 lines (`ICCActions_BQL.cpp`).
- The phase comes from her passive state and gravity flag (`IccBqlPhaseOf`).
- What is handled today:
  - 19 ground slots and 73 air slots;
  - defensives on Bloodbolt Whirl;
  - Swarming Shadows lanes;
  - bite picks and the Pact meeting.
- 9 positions and 9 Z constants, none navprobed. `bql.curve`, `bql.slot` and `bql.airslot` write 0
  for both slot 0 and an erase.
- A hand-rolled flee vector.
- `IccBqlMultiplier`:
  - ranged: AvoidAoe, Flee, Formation and Disengage;
  - Pact: everything but the pact action;
  - Frenzy: everything but the bite;
  - Swarming Shadows: everything but group position;
  - the air phase: reach and Flee.
- Own-aura cancels are not cheats. No cheats left (`bql.swarmingshadows` was removed).

## Guide answers to build on

- **Bites:** a set order that never bites healers.
- **Pact of the Darkfallen:** a central gathering spot.
- **Swarming Shadows:** go to the room's edge, never near the Pact spot.
- **Twilight Bloodbolt:** a 6 yd spread, melee in 2-3 small groups.
- **Incite Terror:** Fear Ward and Tremor Totem.
- **Blood Mirror:** the off-tank in stamina gear.
- **Heroic:** Presence of the Darkfallen, +5% Shroud of Sorrow per vampire.
- **Setup:** 2 tanks, 4-6 healers.

## Focus

- A bite order duty: DPS first, never healers, plus the bitten raider's next bite before Frenzied
  Bloodthirst ends, and CC for an Uncontrollable Frenzy victim, human or bot.
- The Pact gather, and Shadows lanes to the edge on hazard models.
- The spread and melee groups.
- Fear protection through `RaidAntiFear`, and the air phase.
- Slots derived from geometry where possible, navprobed otherwise.

## Owns

Stem `BloodQueen` and its definition, the doc, the reader and its test, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
