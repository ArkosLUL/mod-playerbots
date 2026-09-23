# w3b-fc-defence — Faction Champions: dispels, purges, avoidance

Wave 4, after `w3a-fc-offence` merged. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Guide: `faction-champions-master-strategy-guide-toc-25/`.

## Scope

1. **Dispel friendly CC**: Polymorph, Hex, Cyclone, Repentance, Fear, and whatever else the script
   casts. Class dispel nodes sit below `ACTION_RAID`, and `PartyMemberToDispel` returns one target
   per tick (`docs/engine/pitfalls.md`); decide whether a raid-priority dispel node is needed.
2. **Offensive dispels.**
   - Purge or spellsteal champion buffs.
   - Divine Shield can't be purged, so switch off it.
   - Switch off physical attacks on a Hand of Protection target.
3. **Avoidance**: Bladestorm, Hellfire, Fan of Knives, and ground AoE (Blizzard, Consecration, Death
   and Decay, and whatever the script actually casts). A mechanic the bots can't see is answered by
   spreading in advance.
4. Check the 10- and 25-man rosters, and every champion ability the `w3a` investigator did not
   cover.
5. `fc.` probes; extend `faction_champions.py` and its test.

## Owns

The `FactionChampions` defence files (split from offence in `w3a`); `faction-champions.md`; the
reader and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
