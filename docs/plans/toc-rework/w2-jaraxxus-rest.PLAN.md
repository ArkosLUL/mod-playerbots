# w2-jaraxxus-rest — Lord Jaraxxus, Fel Lightning spread

Carried over from `w2-jaraxxus` under the Oversized-lane rule; runs after it merges, best as a
`refine-lord-jaraxxus` lane once traces from that build exist. Rules, sources, naming contract and
checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, tactics and the node ladder:
`docs/raids/trial-of-the-crusader/lord-jaraxxus.md`.

## Scope

1. **Fel Lightning spread.** Instant, every 10-15 s, on a random player (tanks included), chaining to
   3/5/3/5 targets (10N/25N/10H/25H, row 66528/67029/67030/67031). A magic chain jumps 10 yd
   (`Spell::SearchChainTargets`; no `spell_jump_distance` row), measured with `IsWithinDist`, which
   adds both combat reaches. The guide spreads ranged and healers evenly around the room; stacking
   them is its named alternative, harder on healers and on Legion Flame carriers.
2. **Decide from traces first.** `bosses/lord_jaraxxus.py --boss` prints hits per Fel Lightning cast;
   weigh the chain's damage against what else kills (pitfalls: "A gather node is a damage
   amplifier…", spacing is a step function) before building anything.
3. **Coordinates.** The arena floor is gameobject 195527, absent from the static navmesh and vmaps:
   navprobe verifies no floor point. Derive spots live (`ValidateFloorPoint`,
   `FindNearestPositionClearOfHazards`) or from positions bots stood on in traces; fix none from a
   model of the room.
4. **Coexist with the lane's nodes**: a spread slot must avoid Legion Flames (60 s each) and yield to
   `jaraxxus avoid legion flame` (`ACTION_EMERGENCY + 8`); healers stay within
   `sPlayerbotAIConfig.healDistance` of the tank and the Incinerate Flesh target; a positioning
   action returns false once parked, or it starves the interrupt and heal nodes below it.

## Owns

Stem `Jaraxxus`; `docs/raids/trial-of-the-crusader/lord-jaraxxus.md`; `tools/botobs/bosses/lord_jaraxxus.py`
and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
