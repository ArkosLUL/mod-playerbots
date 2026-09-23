# w0a-restructure — Ulduar layout, no behaviour change

Wave 1. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).

## Scope

1. Split the 12 flat files of `src/Ai/Raid/ToC/` into
   `ToC/{Action,Trigger,Multiplier,Util}/ToC<Kind>_<Stem>.{h,cpp}`. Stems: `Gormok`, `Jormungars`,
   `Icehowl`, `NorthrendBeasts` (helpers shared by the three beasts), `Jaraxxus`,
   `FactionChampions`, `TwinValkyr`, `Anubarak`, `Shared`. `Util/ToCData.h` holds the map id (649),
   `ARENA_CENTER`, `ANUBARAK_PIT_CENTER` and the core enum mirrors, each with a comment naming its
   source header.
2. **Seams.** After this lane, a boss lane must be able to add triggers, actions and multipliers
   touching only its own stem files. So each boss stem owns its strategy node list (the strategy's
   `InitTriggers`/`InitMultipliers` only dispatch) and its context registration. Reserve a per-boss
   burst row hook for `w0c-foundation`. pblint's name checks need `creators["literal"]` inside a
   one-line `class X : public NamedObjectContext<…>`; if per-boss context classes break that, extend
   pblint rather than drop the seam.
3. **Include names.** `src/Ai/Dungeon/TOC/TOC*.h` (5-man Trial of the Champion) collide
   case-insensitively with raid `ToC*.h`. That is why `BuildShared{Action,Trigger}Contexts.cpp`
   include `"Ai/Raid/ToC/ToC…Context.h"` by path. Give new umbrella headers raid-unique names. Keep
   the contexts and `ToCStrategy.{h,cpp}` at the ToC root, so those includes and
   `RaidStrategyContext.h`'s bare `"ToCStrategy.h"` stay valid, or update them in the same commit.
4. **Duplicates.** Drop `TrialOfTheCrusaderHelpers::IsBotInFrontalCone` for the identical
   `EncounterHelpers::IsBotInFrontalCone` (`src/Util/EncounterHelpers.cpp`). Drop `CastTankTaunt` for
   `EncounterHelpers::CastClassTaunt`, which only loses a redundant `IsAlive` check. Factor the five
   "main tank drags the boss toward a centre point" blocks (the Gormok, Icehowl, Jaraxxus, Anub'arak
   and Fjola main-tank actions) into one helper.
5. **Pure move.**
   - Every trigger, action and multiplier name, priority and string stays identical: 32 trigger
     nodes, 9 multipliers.
   - Record the pblint finding set over the three scoped paths before and after; it must match.
   - Keep every `: Trigger(botAI, "literal")` initialiser on one line, since pblint's ctor-name check
     reads single lines only.
   - Leave `GetFirstAliveUnitByEntry` calls alone: replacing them is a behaviour change, and
     `w6-closeout` audits them.
6. **Docs.**
   - Move `docs/raids/trial-of-the-crusader.md` to `docs/raids/trial-of-the-crusader/README.md` with:
     - the boss table (boss, doc, code stems, trace slug, key prefix, from the naming contract);
     - a "Code layout" section naming the seams, which boss lanes read.
   - Split per-boss content into `northrend-beasts.md`, `lord-jaraxxus.md`, `faction-champions.md`,
     `twin-valkyr.md` and `anubarak.md`.
   - Fix every inbound link: grep `trial-of-the-crusader.md`, with known hits in
     `docs/raids/README.md`, `vault-of-archavon.md`, `sunwell.md` and `docs/systems/itemization.md`.
   - Fix the stale "promoted into RaidBossHelpers" note: the shared file is
     `src/Util/EncounterHelpers.{h,cpp}`.
   - This lane may write the README.

## Pipeline for this lane

The investigator is also the seam agent:
- It creates the directory skeleton, the seams, `ToCData.h` and the umbrella headers itself.
- It then hands out up to five mover groups, one per boss group: Beasts (Gormok, Jormungars,
  Icehowl, NorthrendBeasts), Jaraxxus, FactionChampions, TwinValkyr, Anubarak.
- The integrate agent removes the old flat files and runs the checks.
- Reviewers read `git diff -M --color-moved Custom...toc/w0a-restructure`.

## Verified facts

- Current files and line counts: ToCActionContext.h 221, ToCActions.cpp 823, ToCActions.h 302,
  ToCHelpers.cpp 401, ToCHelpers.h 244, ToCMultipliers.cpp 217, ToCMultipliers.h 89, ToCStrategy.cpp
  135, ToCStrategy.h 17, ToCTriggerContext.h 221, ToCTriggers.cpp 353, ToCTriggers.h 276. Everything
  is in namespace `TrialOfTheCrusaderHelpers`.
- Registration sites: `RaidStrategyContext.h` (include, creator, factory),
  `BuildSharedActionContexts.cpp` and `BuildSharedTriggerContexts.cpp` (path include + `Add`),
  `PlayerbotAI.cpp` (`case 649`, and `trialofthecrusader` in the instance strategy list). The key
  must not change.
- CMake collects sources and include directories recursively (`src/cmake/macros/AutoCollect.cmake`),
  so new subdirectories build once the user re-runs CMake.
- The syntax check adds `-I` for new directories. This lane resolves about 45 TUs, so pass
  `PB_MAX_FANOUT=60`.

## Task list

_Filled by the investigator: numbered tasks in groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
