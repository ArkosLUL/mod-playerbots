# w2-jaraxxus-rest — Lord Jaraxxus, Fel Lightning hit matching and spread

Wave 4d, a refine lane. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, tactics and the node ladder:
`docs/raids/trial-of-the-crusader/lord-jaraxxus.md`.

Trace (25N kill 2:22, 0 deaths, raid DPS 177k, human tank "Dragon", build carrying waves 3-4c):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_lord-jaraxxus_1792176436.ndjson`.

## Findings from the 2026-10-16 25N pull

Clean: all 11 Fel Fireballs kicked; every Nether Power purged or stolen within about 1 s; all 5
Incinerate Flesh windows healed through, no Burning Inferno; fire damage only on the human tank.
Fel Lightning hit the full chain of 5 on all 7 casts: 35 hits, about 5k each, 175k in total, 37% of
the raid's damage taken; nobody died of it. `bosses/lord_jaraxxus.py <f> --boss` matched only 5 of
those hits to their casts ("0.7 a cast", "30 hits matched no cast").

## Scope

1. **Fix the Fel Lightning cast-to-hit matching** in `lord_jaraxxus.py`, with a test built from this
   trace's shape.
2. **Decide the spread from the corrected numbers.** On 25N it cost about 25k a cast, no deaths.
   Recommended: no spread on normal; heroic decided by its own numbers. Log either way.
   - Fel Lightning: instant, every 10-15 s, on a random player (tanks included), chaining to
     3/5/3/5 targets (10N/25N/10H/25H, row 66528/67029/67030/67031). A magic chain jumps 10 yd
     (`Spell::SearchChainTargets`; no `spell_jump_distance` row), measured with `IsWithinDist`,
     which adds both combat reaches. The guide spreads ranged and healers evenly around the room;
     stacking them is its named alternative, harder on healers and on Legion Flame carriers.
   - Weigh the chain's damage against what else kills (pitfalls: "A gather node is a damage
     amplifier…", spacing is a step function) before building anything.
3. **If a spread is built:**
   - *Coordinates.* The arena floor is gameobject 195527, absent from the static navmesh and vmaps:
     navprobe verifies no floor point. Derive spots live (`ValidateFloorPoint`,
     `FindNearestPositionClearOfHazards`) or from positions bots stood on in traces; fix none from a
     model of the room.
   - *Coexist with the lane's nodes*: a spread slot must avoid Legion Flames (60 s each) and yield to
     `jaraxxus avoid legion flame` (`ACTION_EMERGENCY + 8`); healers stay within
     `sPlayerbotAIConfig.healDistance` of the tank and the Incinerate Flesh target; a positioning
     action returns false once parked, or it starves the interrupt and heal nodes below it.

**Wave 4e** (`w8-prepull-staging`) takes over `jaraxxus intro main tank`
(`ToCTriggers_Jaraxxus.cpp:28`, `ToCActions_Jaraxxus.cpp:158`): leave that node alone.

## Owns

Stem `Jaraxxus`; `docs/raids/trial-of-the-crusader/lord-jaraxxus.md`; `tools/botobs/bosses/lord_jaraxxus.py`
and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
