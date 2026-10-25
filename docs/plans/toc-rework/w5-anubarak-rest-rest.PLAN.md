# w5-anubarak-rest-rest — Anub'arak: phase 3 economy, Freezing Slash, burrowers once he is low, scarabs

Carried over from `w5-anubarak-rest`; it runs after that lane merges and branches from the merge.
Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics and
the shipped design, `w5-anubarak-rest` included: [anubarak.md](../../raids/trial-of-the-crusader/anubarak.md);
read it and the merged `Anubarak` stem, not line numbers from older briefs. Guide:
`anubarak-master-strategy-guide-toc-25/`.

Trace (25N kill 4:11, 5 deaths, human tank "Dragon", a build before `w5-anubarak-rest`):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_anub-arak_1792178435.ndjson`.
Read it with `tools/botobs/bosses/anub_arak.py <f>` (`--swarm --burrowers --spike --threat`); use any
newer 649 trace first, checking `hdr.bin` against HEAD. Boss max health there: 22.68M (unit row).

## Scope

In this order, as far as the lane holds; apply the overview's oversized-lane rule past that.

1. **Phase 3 raid-health economy.** Leeching Swarm takes 10/10/20/30% of *current* health a second
   (minimum 250) and heals him by what lands, so the guide keeps the raid low: no healing for melee,
   hunters and affliction warlocks, fast heals only for Penetrating Cold carriers, a cancelaura on
   stamina buffs at phase 3. Needs a heal hold that cannot starve a real emergency. 2026-10-16 25N:
   phase 3 55.4 s, 66240 1.96M over 1259 ticks (about 9% of his health), no deaths, carriers down to
   46-67%.
2. **Freezing Slash defensives.** Guide: one per slash. Instant, untelegraphed, no row (66012), on
   his victim: first 7-15 s after the engage or emerge, then every 15-20 s, so only a timer model on
   those edges can place one. 2026-10-16 25N: three slashes on the tanks for 0.8-7.2k.
3. **Burrowers once he is low.** Guide: ignore them for him (heroic keeps them coming in phase 3).
   Wave pacing, side tanks and Spider Frenzy spacing shipped in `refine-anubarak` (anubarak.md).
4. **Scarabs.** 20000 threat on a random player each; Acid-Drenched Mandibles (65775/67861/67862/67863)
   stacks a nature DoT (guide: limit stacks going into phase 3). Consider a DPS threat brake (none
   exists, see `docs/engine/pitfalls.md`) and stack management. 2026-10-16 25N: 123k Mandible damage
   in all, five stacks at most.
5. **10H Spider Frenzy:** one side tank holds both burrowers of a wave on one patch. Heroic phase 2
   positioning shipped in `refine-anubarak`; normal's (guide: a corner for the first spike, then
   stacked where he should emerge) is still open.

## Owns

As `w5-anubarak-rest`: stem `Anubarak`, `docs/raids/trial-of-the-crusader/anubarak.md`,
`tools/botobs/bosses/anub_arak.py`, `tools/botobs/tests/test_anub_arak.py`.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
