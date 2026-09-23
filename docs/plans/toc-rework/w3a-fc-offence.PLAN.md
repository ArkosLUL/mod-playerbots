# w3a-fc-offence — Faction Champions: targeting, CC, burst

Wave 3. `w3b-fc-defence` follows in wave 4. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Guide: `faction-champions-master-strategy-guide-toc-25/`.

## Scope

1. **Kill target.**
   - Today `FactionChampionsFocusPriorityAction` re-picks the lowest-health healer, then the
     lowest-health champion, every tick. The marks it places are never cleared inside a pull.
   - Latch the kill target with a sticky margin and switch only on a real reason. Clear the old mark
     on a switch.
   - Take the kill order from the guide.
2. **CC.** Assign it through `SetRtiCcTarget` and the `"rti cc"` value. Today it is one moon on a
   second healer; the guide may want more.
3. **Interrupts.** Class strategies already interrupt enemy healers (mage "counterspell on enemy
   healer", rogue kick). Check that focus plus those nodes cover the healers, and add duty only
   where they don't.
4. **AoE suppression.** Keep it, but through the encounter gate.
5. **Burst.**
   - Champions reset threat every 2 s (`boss_faction_champions.cpp`), so the base
     `HoldBurstUntilTankEngagedMultiplier` never opens.
   - Add a raid-agnostic exemption for bosses with no stable victim, modelled on `noVictimBosses`
     (`src/Ai/Base/Combat/BurstCooldowns.cpp`, used in `src/Ai/Base/Strategy/BurstWindowStrategy.cpp`).
     This is an allowed change outside ToC; list it in the report.
   - Then set the lust row.
6. **Fears.** Fear 65809, Psychic Scream 65543 and Intimidating Shout 65930 go through the shared
   `RaidAntiFear`.
7. **Threat redirect.** Veto the generic threat redirect here: the main tank is reliably the wrong
   target.
8. **Observability.** `fc.` probes (kill target, CC target), a
   `tools/botobs/bosses/faction_champions.py` reader and test.

## Owns

Stem `FactionChampions` (offence files; split so `w3b` gets its own); `faction-champions.md`; the
reader and its test; the base burst exemption above.

## Verified facts

- Both factions' rosters are in the `FactionChampions` stem, from the old `ToCHelpers.h`. NPC entries
  are the same on every difficulty.
- The core script picks the composition per instance: 6 champions in 10-man, 10 in 25-man.

## Task list

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
