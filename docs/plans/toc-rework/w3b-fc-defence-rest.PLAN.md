# w3b-fc-defence-rest — Faction Champions: crowd-AoE spread, totems, treants

Carried over from `w3b-fc-defence` under the oversized-lane rule; runs after it merges, best as a
`refine-faction-champions` lane once traces from that build exist. Rules, sources, naming contract
and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, ids and the 10/25 roster:
`docs/raids/trial-of-the-crusader/faction-champions.md`, "What the defence plays against". Guide:
`faction-champions-master-strategy-guide-toc-25/`.

## Scope

1. **Crowd-AoE spread.**
   - Fan of Knives, Divine Storm, Arcane Explosion, Frost Nova, Psychic Scream and Intimidating Shout
     are instants that fire only with 3+ threat-list units (pets included) within 5-10 yd of the
     caster by `GetDistance2d`, which subtracts both combat reaches. Nothing can dodge them; only
     spacing denies them.
   - Guide: stay spread over the whole arena; keep away from melee champions. Champions run 8.0 yd/s
     against a player's 7, so kiting a melee champion fails.
   - Decide from traces first: `bosses/faction_champions.py --aoe` gives victims per cast. Weigh that
     against what else kills (pitfalls: a spread tighter than the AoE buys nothing, spacing is a step
     function, and the spread can cost more than the AoE).
   - Melee on a melee kill target always make 3; any spread is ranged and healers only, and must
     yield to `faction champions avoid aoe` and keep healers in heal range.
   - The arena floor is GO 195527, invisible to navprobe: derive spots live or from where bots stood
     in traces.
2. **Totems.** The Enhancement Shaman drops Grounding (5925), Windfury (6112) or Tremor (5913) every
   30 s. Tremor strips the warlock bots' Fear-on-CC, Grounding eats one CC. The guide wants a player
   killing them. The offence target guard leaves no bot free to, so a duty needs its own node.
3. **Force of Nature** (Balance, every 3 min): three treants 36070 for 30 s. Decide from traces
   whether they matter.
4. **Mass Dispel** on AoE fears (3+ raiders under Psychic Scream) and on champion Heroism/Bloodlust,
   the guide's other two uses, if `--dispel` shows the fears cost more than the dispel duty clears.
5. **Observability.** `fc.` probes for whatever is built, and the matching reader sections and test.

## Owns

The `FactionChampionsDefence` stem, `faction-champions.md`, `faction_champions.py` and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
