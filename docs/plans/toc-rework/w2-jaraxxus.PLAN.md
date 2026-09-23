# w2-jaraxxus — Lord Jaraxxus

Wave 3. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).
Guide: `lord-jaraxxus-master-strategy-guide-toc-25/`.

## Scope

1. **Intro.** Keep the Fizzlebang intro (stage 2) closed. Jaraxxus sets `IsEncounterInProgress` on
   engage (`boss_lord_jaraxxus.cpp`, `JustEngagedWith`).
2. **Fel Fireball interrupt duty**, ranked by GUID among able and ready interrupters and recomputed
   per cast (`docs/raids/ulduar/vezax.md`). A positioning action returning true while moving starves
   class interrupts.
3. **Incinerate Flesh** absorb healing, which exists today; re-check its priority against the
   "dispel below `ACTION_RAID`" trap.
4. **Legion Flame.** Today `AvoidCreatureClusterAction`. Make it a trail-aware hazard sweep with
   `FindNearestPositionClearOfHazards`; the target walks out of the raid.
5. **Nether Power.** Dispel or spellsteal it. Its stacks aren't in any trace, so probe them.
6. **Adds.**
   - Mistress of Pain (Mistress' Kiss locks a school: casters stop casting while kissed).
   - Felflame Infernal (Fel Streak, threat reset).
   - Nether Portal and Infernal Volcano by difficulty. Their ids are declared but unused today:
     `NPC_NETHER_PORTAL`, `NPC_INFERNAL_VOLCANO`, `SPELL_LEGION_FLAME`.
   - Tank assignment for the adds.
7. **Touch of Jaraxxus** is commented out in this core (`boss_lord_jaraxxus.cpp`): record it as not
   implemented; don't build it.
8. **Lust row**, `jaraxxus.` probes, a `tools/botobs/bosses/lord_jaraxxus.py` reader and test.
9. Verify every Jaraxxus node `w0c-foundation` made live in 25N/10H/25H.

## Owns

Stem `Jaraxxus`; `docs/raids/trial-of-the-crusader/lord-jaraxxus.md`; the reader and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
