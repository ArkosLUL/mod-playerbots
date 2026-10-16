# w3b-fc-defence-rest — Faction Champions: CC that pays, crowd-AoE spread, totems, treants

Wave 4d, a refine lane. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, ids and the 10/25 roster:
`docs/raids/trial-of-the-crusader/faction-champions.md`, "What the defence plays against". Guide:
`faction-champions-master-strategy-guide-toc-25/`. Line numbers are as of `1846fc944`.

Trace (25N kill 4:05, 5 deaths, raid DPS 129k, human tank "Dragon", build carrying waves 3-4c):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_thrakgar_1792176980.ndjson`.
Read it with `bosses/faction_champions.py <f>` (sections `--cc --fear --aoe --burst --kill`). For
the per-bot damage comparison, the same raid's Jaraxxus pull minutes before:
`.../botobs/649_3_lord-jaraxxus_1792176436.ndjson`; damage is the snapshots' cumulative `dealt`
column.

## Findings from the 2026-10-16 25N pull

4:05 is partly expected and about 75 s too long: the ten champions have 30.2M health, about 2:50 at
the raid's Jaraxxus rate. Health scaling is not the cause (`mod-dungeon-scale` gives every raid boss
the same 1.2; DB `HealthModifier` 192 per champion). Kill-target focus works: 97-99% of DPS samples.

1. **CC.** Three warlocks and the Balance druid spent 55-80% of the fight casting Fear or Cyclone:
   417 casts (Agony 133, about 200 of 245 s). A fear kept its champion feared (snapshot movement
   type 9) only 4-7% of the time:
   - champions get diminishing returns like players (`flags_extra` carries `ALL_DIMINISH`);
   - the CC targets stand in the melee pile, and damage breaks Fear.
   Those four dealt a third to half their Jaraxxus damage: about 22k raid DPS, half the gap.
2. **Other CC faults.** The feral druid (Ecoterrorist) left cat form to cast Cyclone. Both mages held
   a CC assignment and never cast Polymorph. The CC pick lives in the offence stem
   (`ToCHelpers_FactionChampions.cpp`, `SetRtiCcTarget` call at 710).
3. **Melee deaths.** Shadow died three times, Totemist twice: champion threat picks close,
   low-armour players. About 10k DPS lost.
4. **No lust before 2:09.** The Jaraxxus lust about 8.5 min earlier left the 10-minute Exhaustion;
   Totemist cast at 1:36 only because his death cleared his own. Pull pacing, not a bot fault.
5. **Stacking.** Intimidating Shout feared 22 raiders for 87 s in total because the melee stack.

## Scope

New item first; then the carried-over items, as far as the lane holds.

1. **CC that pays.**
   - A CC bot casts only when the cast can land and hold: track each champion's DR per category;
     skip while immune or when the next application would last 2.5 s or less; no damage-breaking CC
     on a champion damaged within about the last 2 s. Otherwise it attacks the kill target.
   - Melee druids leave the CC pool.
   - Find why the mages' assignment never cast; make it work or stop assigning it.
   - Measure first: if even well-timed CC can't beat the lost damage on normal, turn it off there
     and log the decision.
2. **Crowd-AoE spread.** Shout fears and Bladestorm on the melee stack (finding 5) are the numbers to
   beat.
   - Fan of Knives, Divine Storm, Arcane Explosion, Frost Nova, Psychic Scream and Intimidating Shout
     are instants that fire only with 3+ threat-list units (pets included) within 5-10 yd of the
     caster by `GetDistance2d`, which subtracts both combat reaches. Nothing can dodge them; only
     spacing denies them.
   - Guide: stay spread over the whole arena; keep away from melee champions. Champions run 8.0 yd/s
     against a player's 7, so kiting a melee champion fails.
   - Decide from traces first: `--aoe` gives victims per cast. Weigh that against what else kills
     (pitfalls: a spread tighter than the AoE buys nothing, spacing is a step function, and the
     spread can cost more than the AoE).
   - Melee on a melee kill target always make 3; any spread is ranged and healers only, and must
     yield to `faction champions avoid aoe` and keep healers in heal range.
   - The arena floor is GO 195527, invisible to navprobe: derive spots live or from where bots stood
     in traces.
3. **Totems.** The Enhancement Shaman drops Grounding (5925), Windfury (6112) or Tremor (5913) every
   30 s. Tremor strips the warlock bots' Fear-on-CC, Grounding eats one CC. The guide wants a player
   killing them. The offence target guard leaves no bot free to, so a duty needs its own node.
4. **Force of Nature** (Balance, every 3 min): three treants 36070 for 30 s. Decide from traces
   whether they matter.
5. **Mass Dispel** on AoE fears (3+ raiders under Psychic Scream) and on champion Heroism/Bloodlust,
   the guide's other two uses, if `--dispel` shows the fears cost more than the dispel duty clears.
6. **Observability.** `fc.` probes for whatever is built, and the matching reader sections and test:
   `--cc` gains fear uptime per champion (snapshot movement type 9) and casts landed against casts
   started; `--burst` names Exhaustion at the pull.

**Wave 4e** (`w8-prepull-staging`) reads this lane's opening spread for the pre-pull stands: keep it
reachable through a helper.

## Owns

Both stems, `FactionChampions` (for the CC pick) and `FactionChampionsDefence`;
`faction-champions.md`, `faction_champions.py` and its test.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
