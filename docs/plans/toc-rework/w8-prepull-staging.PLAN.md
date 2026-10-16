# w8-prepull-staging — ToC: bots wait on their opening spots before each boss engages

Wave 4e. It runs after wave 4d (`w5-anubarak-rest`, `w3b-fc-defence-rest`, `w2-jaraxxus-rest`,
`refine-beasts-2`) merges and branches from that merge: those lanes change the spots this one reads
(worm spread, breath bearings, the Anub'arak anchor, the champions' opening spread), so read their
merged code, not the line numbers here. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Boss docs: `docs/raids/trial-of-the-crusader/`. Line
numbers are as of `1846fc944`; core script paths are under
`src/server/scripts/Northrend/CrusadersColiseum/TrialOfTheCrusader/` at the server root.

Goal: each boss arrives at the same coordinates every pull, so bots wait on their spots before he
engages, and the tanks catch him at once.

## Facts

**Where each boss arrives and how it engages** (`trial_of_the_crusader.h:274-320` `Locs[]`,
`instance_trial_of_the_crusader.cpp`):

| Boss | Arrives at | Engages | Window |
|---|---|---|---|
| Gormok | gate front (563.9, 178.4) | forced attack on the nearest player within 200 yd | 11 s after `SCENE_004`; 46 s from the gossip on a first start, 11 s on a retry |
| Acidmaw (stationary) / Dreadscale (mobile) | (549.9, 171.5) / (575.7, 171.5) | forced, nearest | 17.5 s after Gormok's death; heroic on its timer |
| Icehowl | gate front | forced, nearest | 15.5 s after the worms die |
| Jaraxxus | (563.7, 129.6) | forced, nearest | 70.8 s from the gossip; after a wipe at the centre, attack only |
| Faction Champions | ten landing slots, west for an Alliance raid (x 520-537, y 111-137), mirrored for Horde | no forced attack: 23 yd proximity, and one engaged pulls all | released 4 s after the jump |
| Twins | Fjola (585.5, 170), Eydis (545.5, 170) | 23 yd proximity | attackable 25.5 s from the gossip |
| Anub'arak | (785.9, 133.4) | attackable once a player is inside 80 yd, then 23 yd proximity | no gossip, stage 9 |

- Barrett Ramsey (`NPC_BARRENT` 34816) loses his gossip flag when "We are ready!" is chosen and gets
  it back only after the encounter, the Beasts' walk-ins and Jaraxxus' retry (Barrett re-summons
  him) included.
- In the 2026-10-16 traces (`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_*`
  from `1792176436` on) each boss turns hostile within 1-3 yd of the same spot every pull; the raid
  waits 37-70 yd away; Gormok runs 32-36 yd before a tank reaches him (in 25N his first victim was a
  ranged bot); Dreadscale hit a melee DPS before any tank in both Beasts pulls. Jaraxxus, whose main
  tank already walks to him in the intro, had zero tank movement.
- The ToC strategy runs on both engines and its triggers are gated by stage only, so they fire out
  of combat. No helper says a scene is running, and stage 8 closes every ToC gate.
- Precedent, Naxx Thaddius staging: node `NaxxDefinition_Thaddius.cpp:20-33`; spot and safety
  `NaxxBossHelper.h:2491-2532`; `follow` zeroed by `ThaddiusPrepullMultiplier`
  (`NaxxMultipliers.cpp:124`); doc `docs/raids/naxxramas.md`, "Pre-pull staging". ToC's own:
  `jaraxxus intro main tank` (`ToCTriggers_Jaraxxus.cpp:28-35`, `ToCActions_Jaraxxus.cpp:158-169`,
  `ToCMultipliers_Jaraxxus.cpp:70-80`).
- What works against it: `follow` (a default action at 1.0) walks a parked bot back to its master;
  `FindNearestPositionClearOfHazards` searches only around the bot. Three ways to start a pull early
  (`docs/engine/pitfalls.md`, "Before the pull, and out of combat" and "A targeting mark never
  clears inside a pull"): a direct `Attack()` reaches 100 yd, `tank assist` runs out of combat, and
  Anub'arak is attackable inside 80 yd.

## Scope

1. **Window.** `ToCStagingEncounter(bot)` names the encounter about to start; it is non-None while
   none of its units is engaged, the bot is not in combat, and either Barrett is present without his
   gossip flag, or the stage is 9 and the bot is in Anub'arak's room (past the Web Door, x > 661.6).
   The next beast is the first one not yet dead. Decision (log it): staging starts at the gossip,
   not before, so bots stay with the player while they buff, eat or talk. Tightest case, a Beasts
   retry: 11 s against a walk of about 60 yd from Barrett, so about 9 s.
2. **Stands**, per encounter and role, built from the script's own points. The arena floor is a GO,
   so per the module `CLAUDE.md` they anchor on those points, are validated live with
   `ValidateFloorPoint`, and are checked against the traces' positions.
   - *Forced-attack bosses* (Gormok, the worms, Icehowl, Jaraxxus): each boss's duty tank stands
     strictly nearest its release point, on the spot the encounter holds it; every other bot a
     margin further out. So he walks straight onto his tank: Gormok's tank, the two worm holders,
     Icehowl's tank, Jaraxxus' main tank and add tanks. This takes over `jaraxxus intro main tank`.
   - *Proximity bosses* (Faction Champions, Twins, Anub'arak): nobody inside 23 yd plus a margin of a
     boss or a landing slot; the pull stays with the player, as today. Stands are the opening
     layout: champions, the crowd-AoE spread; Twins, the existing essence step, then each tank on
     its twin's side; Anub'arak, the main tank on the anchor, side tanks on their side points, the
     raid west of the centre, so once a player pulls he runs onto the anchor and there is no drag.
   - *Non-tanks* take each encounter's opening layout: Gormok, the ranged band out of Stomp; worms,
     the caster spread slots and melee rear spots on Dreadscale's arrival; Icehowl, breath bearings
     around his arrival point.
   - *Humans* can't be moved. A human nearer the release point than the bot tank shows in the probe
     and is not fought.
3. **Hold.** Park within about 1 yd; the action returns true while walking (latched), false once
   parked. A Block rule in `ToCDefinition_Staging.cpp` zeroes `follow`, the `stay` return and
   formation moves for a bot holding a stand; its predicate is the window plus a latched stand.
   Never `Attack()` or `tank assist` from a stand. The hold ends when the encounter goes live or the
   bot enters combat, so leftover snobolds win.
4. **Observability.** A `toc.stage` probe gives each bot's stand label, or `none`. A `--staging`
   view, in `postmortem.py` or per boss reader as the investigator decides: each bot's distance to
   its stand at the release, the boss's first victim, time to a tank, any non-tank hit in the first
   5 s.

## Owns

- a new `Staging` stem: `ToC{Actions,Triggers,Helpers}_Staging.*` and `ToCDefinition_Staging.cpp`;
- its registration in `ToCStrategy.cpp`;
- the `jaraxxus intro main tank` node and its multiplier, to retire (`Jaraxxus` stem, those lines
  only);
- a "Pre-pull staging" section in `docs/raids/trial-of-the-crusader/README.md`;
- the `--staging` reader section and its test.

Encounter spots come from each encounter's helpers, read-only: where a spot is not reachable from
outside its stem, add an accessor there and list it in the report. New `.cpp` files need a CMake
re-run: say so in the report.

## Task list

_Filled by the investigator: numbered tasks in 1-4 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
