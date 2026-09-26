# w5-anubarak-rest — Anub'arak, what w5 carried over

After `w5-anubarak` merges; best as a `refine-anubarak` lane with 649 traces from a build carrying
it (check `hdr.bin` against HEAD). Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics and the shipped design:
[anubarak.md](../../raids/trial-of-the-crusader/anubarak.md). Guide:
`anubarak-master-strategy-guide-toc-25/`. Read `tools/botobs/bosses/anub_arak.py` output before
designing each item.

## Scope

1. **Kiter immunity.** A kiter with no patch dies once the spike reaches 11.4 yd/s. Impale is
   physical (school 1), so Hand of Protection voids it; the guide's heroic phase 2 has a paladin
   HoP the kiter just outside a patch and, for the last kite, stand still under HoP where he should
   emerge. Also self-immunities (Divine Shield, Ice Block). `anub.kite` `ring` rows and Impale hits
   on kiters (`spike` section) say how often it is needed.
2. **Phase 3 raid-health economy.** Leeching Swarm takes 10/10/20/30% of *current* health a second
   and heals him by it, so the guide keeps the raid low: no healing for melee, hunters and
   affliction warlocks, fast healing only for Penetrating Cold carriers, a cancelaura on stamina
   buffs at phase 3. Needs a heal hold that cannot starve a real emergency; `swarm` section first.
3. **Freezing Slash defensives.** The guide wants one per slash. It is a non-triggered instant with
   no warning: first 7-15 s after the engage or emerge, then every 15-20 s, so only a timer model
   anchored on those edges can place one.
4. **Burrower pacing.** Guide: throttle AoE so each wave dies just before the next (45 s cadence),
   and once he is low, ignore burrowers for him (heroic keeps them coming in phase 3).
5. **Landing and regroup.** The floor break drops the raid into what is likely water under the
   arena; navprobe cannot path out of it. The Web Door (661.6, 144.7) shuts at the engage. Act only
   if the `pull` section shows bots stuck or locked out.
6. **Scarabs.** 20000 threat on a random player each; Acid-Drenched Mandibles stacks (guide: limit
   stacks going into phase 3). Consider a DPS threat brake (none exists, see pitfalls) and stack
   management.
7. **Spider Frenzy.** Burrowers within 12 yd haste each other; 25H brings four a wave to two patches,
   10H two to one tank. A third patch, or spacing on the patch.
8. **Phase 2 raid positioning.** Guide: raid to one corner for the first spike, back to the centre,
   then stacked where he should emerge; w5 ships only the spike dodge and the boss drag off the
   patches 15 s before each submerge.

## Owns

As `w5-anubarak`: stem `Anubarak`, `docs/raids/trial-of-the-crusader/anubarak.md`,
`tools/botobs/bosses/anub_arak.py`, `tools/botobs/tests/test_anub_arak.py`.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
