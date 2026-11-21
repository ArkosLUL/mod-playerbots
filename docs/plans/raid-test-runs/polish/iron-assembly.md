# Polish log: Assembly of Iron (25N hard mode)

Trace slug `iron-assembly` (files land under `stormcaller-brundir`), raid `uld`, scenario
`uld-assembly` (drop 40 yd east of Steelbreaker, aggro start on him pulls all three). Hard mode is the
kill order: a kill counts only when Steelbreaker died last, i.e. the last `ironassembly.alive` value is
`1`. Reader: `tools/botobs/bosses/iron_assembly.py <trace>` (`--phases`, `--empowered`, `--deaths`);
its banner prints the verdict.

## Round 1 (2026-11-22, build 0c08a0922 + DC 7495350)

**Data.** Window 5, one pull per pool, all valid, all wipes in hard mode order:
uld1 `tr-20261121-212648-23` (`603_23_stormcaller-brundir_1795296460`, 5:31),
uld2 `tr-20261121-213312-25` (`603_25_stormcaller-brundir_1795296846`, 5:30),
uld3 `tr-20261121-212809-24` (`603_24_stormcaller-brundir_1795296542`, 4:07).
Harness clean: no DC veto inside a pull, healers start at full mana, tanks pick up their assignments
inside 10 s.

**Stop rules.** Not polished (31 deaths each). First round.

**Deaths by mechanic.**

| Pull | Phase timeline | Deaths |
|---|---|---|
| uld1 | Brundir 1:43, Molgeim 3:19, Steelbreaker alone 127 s, lowest 7.0% | 24 High Voltage, 2 Fusion Punch, 1 Meltdown, 3 Steelbreaker melee (1 tank), 1 Static Disruption |
| uld2 | Brundir 1:51, Molgeim 3:57, alone 87 s, lowest 43.6% | 26 High Voltage, 1 Fusion Punch, 1 Meltdown, 2 Steelbreaker melee (1 tank), 1 Static Disruption |
| uld3 | Brundir 1:37, Molgeim 3:43, alone 19 s | Molgeim phase: 2 Static Disruption, 1 Rune of Death, 1 High Voltage; then 25 High Voltage, 2 Steelbreaker melee on tanks |

**Picked: the High Voltage cascade, started by healers out of mana.** HV is 73–76% of all damage
taken and 76 of 93 deaths. Every pull had healers dry within 25 s of Steelbreaker's
phase (`--phases`): uld1 priest and paladin under 10% at 3:37/3:44 (18–25 s in), uld2 two of four
under 10% and two at 12–14% at Molgeim's death, uld3 two healers dead and the third priest at 1%. The first deaths in his phase
follow from it: uld2's partner tank took Fusion Punch at 61% health with all four healers at 1–3%
mana, raid mean health 33–35% when the first ranged died.

Contributing bug: **every healer priest cast its one Shadowfiend 8–23 s in at 80–95% mana** (uld1
Uldxac, uld2 Uldyaf/Uldyad, uld3 Uldzae/Uldzac), the full bar capping the return at 5–19%. Cast low it returns about half the
bar: uld2 Uldyaf's second at 5:08, 1.8% → 54.7% 16 s later. Priests are the first healers dry in every
pool (2:49–3:37). Cause: disc and holy run the class `shadowfiend` trigger, `SpellNoCooldownTrigger`
(`DiscPriestStrategy.cpp:153`, `HolyPriestStrategy.cpp:203`); the encounter exempts it from the
burst hold as a mana return (`UldMultipliers_IronAssembly.cpp:70`) but never gates it on mana.

Fix: `iron assembly shadowfiend hold` blocks `shadowfiend` for healers while their mana is above 50%
during the fight (`UldDefinition_IronAssembly.cpp`, `IronAssemblyShadowfiendHeld`). Mana timing only,
so it holds for 10/25 and both modes.

**Questions (plan changes).**
1. *Five healers in the `uld-assembly` 25-man comp?* Recommended: yes. With 4 at ilvl 226 the fight
   runs 5:20+ and the healing runs out before the last phase in all three pools; the Shadowfiend fix
   buys each priest roughly a minute, not the whole phase.
2. *Spread the ranged from Molgeim's phase (Steelbreaker's phase 2) too, closing the known-open Static
   Disruption gap?* Recommended: yes, after question 1. Phase-2 casts hit 10–17 stacked members and the
   +75% nature vulnerability adds 0.24–0.58M of High Voltage per pull, 5–9% of the healing.

**Also seen, not picked.** Fusion Punch kills the swap partner at one charge (uld1 Uldxam 4:21, 43.6k
on a 41k-HP DK whose IBF went at 0:34 and 2:52, AMS on cooldown; uld2 Uldyab 4:56). Holding tank
defensives for it was considered and left: tanks dropped to 8–38% in Molgeim's phase, where those
buttons were what kept them up. Uld1's rebirthed tank took the Meltdown taunt at 65% health and died
3 s later.

**Next window should show.** `--phases`: each healer priest's Shadowfiend at ≤ 50% mana, and the
first "under 10% mana" times later than 2:49–3:37 for priests. Then fewer HV deaths before the first
Meltdown and a later first non-Meltdown death in his phase.
