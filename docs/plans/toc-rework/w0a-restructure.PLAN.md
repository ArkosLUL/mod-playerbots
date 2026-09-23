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
  135, ToCStrategy.h 17, ToCTriggerContext.h 221, ToCTriggers.cpp 353, ToCTriggers.h 276. Helpers
  and id enums are in namespace `TrialOfTheCrusaderHelpers`; trigger, action and multiplier classes
  are global. Counts: 32 trigger nodes, 32 trigger creators, 32 action creators, 9 multipliers.
- pblint baseline, taken on the pristine branch before any edit (`pblint.py --warnings
  src/Ai/Raid/ToC src/Bot/PlayerbotAI.cpp src/Ai/Raid/RaidStrategyContext.h`): 0 errors, 22 warnings,
  all `[strategy-never-added]` at `RaidStrategyContext.h:39-60` (aq20 … trialofthecrusader), none
  under `src/Ai/Raid/ToC`. Compare by `[check] message`, since paths and lines move.
- pblint buckets a `creators["…"]` key under the last one-line
  `class X : public NamedObject{Context,Factory}<K>` seen earlier in the same file
  (`creators_by_kind`), so a stem context's constructor must be inline in the header declaring it.
- `TrialOfTheCrusaderHelpers::IsBotInFrontalCone` is identical to the `EncounterHelpers` one. ToC's
  `CastTankTaunt` differs from `EncounterHelpers::CastClassTaunt` only by a `!target->IsAlive()`
  check; its one caller passes a unit from `GetFirstAliveUnitByEntry`.
- The five drag blocks (Gormok, Icehowl, Jaraxxus and Fjola to `ARENA_CENTER`, Anub'arak to
  `ANUBARAK_PIT_CENTER`) are identical: while the boss's victim is the bot and it is more than 12 yd
  from the anchor, `MoveTo` a point up to 5 yd toward it with `MOVEMENT_COMBAT`, `lessDelay` and
  `backwards` true. `MoveTo` is protected in `MovementAction`, so the helper is a base-class member.
- Nothing links `trial-of-the-crusader.md`; the brief's "known hits" are plain mentions. The stale
  RaidBossHelpers note was `vault-of-archavon.md:121-122`.
- Registration sites: `RaidStrategyContext.h` (include, creator, factory),
  `BuildSharedActionContexts.cpp` and `BuildSharedTriggerContexts.cpp` (path include + `Add`),
  `PlayerbotAI.cpp` (`case 649`, and `trialofthecrusader` in the instance strategy list). The key
  must not change.
- CMake collects sources and include directories recursively (`src/cmake/macros/AutoCollect.cmake`),
  so new subdirectories build once the user re-runs CMake.
- The syntax check adds `-I` for new directories. This lane resolves about 45 TUs, so pass
  `PB_MAX_FANOUT=60`.
- Integrate results, re-run after the review fixes:
  - pblint after-set equals the baseline by `[check] message`: 0 errors, the same 22
    `[strategy-never-added]` warnings, none under `src/Ai/Raid/ToC`.
  - Parity against `HEAD` (task 31): 32 nodes equal in dispatch order, 32 + 32 creator keys with the
    same factories and no key in two stems, 9 multipliers in order, 32 trigger ctor strings and 32
    action name defaults unchanged.
  - Syntax check (task 32): all 33 TUs pass (every `.cpp` under `src/Ai/Raid/ToC` plus both
    `BuildShared*Contexts.cpp`).
  - Collisions (task 30): no include of the old flat headers; the only case-insensitive header
    collisions are the pre-existing root `ToCActionContext.h`, `ToCTriggerContext.h` and
    `ToCStrategy.h`, included by path.
  - Python suite: 221 tests pass.

## Task list

All tasks are done. The pre-lane sources are on `HEAD` (`git show HEAD:src/Ai/Raid/ToC/<file>`);
task 30 deleted the old flat files.

**Mover rules (groups 1-5).**

- Copy bodies, comments, names, strings and priorities verbatim. Allowed changes: the refactors a
  task names, includes, and header guards (`PLAYERBOTS_RAID_TOC<KIND>_<STEM>_H`). No new comments
  beyond what a refactor needs (no-nonsense rule). Leave `GetFirstAliveUnitByEntry` calls alone.
- Keep each trigger constructor's `: Trigger(botAI, "literal")` on one line, as it is now.
- `.cpp` files keep `using namespace TrialOfTheCrusaderHelpers;` and `using namespace EncounterHelpers;`
  as the old ones do, and include what they use: `ToCData.h`, `ToCHelpers_Shared.h`
  (`GetNearestCreatureByEntry`, `GetCreatureClusterCenter`), the stem's own headers, `Playerbots.h`,
  `EncounterHelpers.h`, plus the old file's system and core includes where still needed.
- Edit no file outside your group: not the old flat files, not the Shared stem, not the brief.
  Report anything a shared file needs to the integrate step.

**Stem template.** `<Stem>` as in the file names. Seam names are exact: the root already calls them.

- `Util/ToCHelpers_<Stem>.{h,cpp}`: `#include "PlayerbotAI.h"` and `"ToCData.h"`; the stem's
  helpers and constants from the old `ToCHelpers`, with their comments, inside
  `namespace TrialOfTheCrusaderHelpers`.
- `Action/ToCActions_<Stem>.{h,cpp}`: the action classes, then
  `class ToC<Stem>ActionContext : public NamedObjectContext<Action>` (that declaration on one line)
  with an inline constructor holding the stem's `creators[...]` lines and private static factories
  copied from the old `ToCActionContext.h`: same keys, same order, `RaidTrialOfTheCrusaderActionContext::`
  renamed to `ToC<Stem>ActionContext::`. Include `NamedObjectContext.h` and `ToCActions_Shared.h`.
- `Trigger/ToCTriggers_<Stem>.{h,cpp}`: the trigger classes, then
  `class ToC<Stem>TriggerContext : public NamedObjectContext<Trigger>` built the same way from the old
  `ToCTriggerContext.h`, and `void AddToC<Stem>TriggerNodes(std::vector<TriggerNode*>& triggers);`.
  The `.cpp` defines it with the stem's `triggers.push_back(new TriggerNode(...))` blocks and their
  comments copied from the old `ToCStrategy.cpp`, in their old order; include `Strategy.h` for the
  `ACTION_*` constants.
- `Multiplier/ToCMultipliers_<Stem>.{h,cpp}`: the multiplier classes, and
  `void AddToC<Stem>Multipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);` pushing
  the stem's `new X(botAI)` in the old `InitMultipliers` order. Encounter stems (`NorthrendBeasts`,
  `Jaraxxus`, `FactionChampions`, `TwinValkyr`, `Anubarak`) also declare
  `ToCBurstWindow ToC<Stem>BurstWindow(PlayerbotAI* botAI);` (include `ToCMultipliers_Shared.h`) and
  define it as `return {};` with the parameter name commented out. Nothing calls it until
  `w0c-foundation`.
- Main-tank drag refactor, wherever a task names it: derive the action from `ToCMainTankHoldAction`
  (`Action/ToCActions_Shared.h`, ctor `: ToCMainTankHoldAction(botAI, name)`) and replace the
  `if (<boss>->GetVictim() == bot) { ... } return false;` tail with
  `return DragBossToAnchor(<boss>, <anchor>);`, keeping the comment that sat above the `if`.

**Done by the investigator.**

1. `Util/ToCData.{h,cpp}`: map id, `ARENA_CENTER`, `ANUBARAK_PIT_CENTER`, `ToCNpcs`, `ToCSpells`,
   each with its core source; `Util/ToCHelpers_Shared.{h,cpp}`.
2. `Action/ToCActions_Shared.{h,cpp}` (`AvoidCreatureClusterAction`, `ToCMainTankHoldAction`),
   `Multiplier/ToCMultipliers_Shared.h` (`ToCBurstWindow`), and the empty seams
   `Action/ToCActions_NorthrendBeasts.h`, `Trigger/ToCTriggers_NorthrendBeasts.h`,
   `Multiplier/ToCMultipliers_Gormok.h`, `Multiplier/ToCMultipliers_Jormungars.h`.
3. Umbrellas `Action/ToCRaidActions.h`, `Trigger/ToCRaidTriggers.h`, `Multiplier/ToCRaidMultipliers.h`.
   Root `ToCActionContext.h` and `ToCTriggerContext.h` merge the eight stem contexts; `ToCStrategy.cpp`
   dispatches nodes and multipliers in the order Gormok, Jormungars, Icehowl, NorthrendBeasts,
   Jaraxxus, Anubarak, FactionChampions, TwinValkyr, which reproduces the old insertion order.
4. Docs: `docs/raids/trial-of-the-crusader.md` replaced by `docs/raids/trial-of-the-crusader/`
   (`README.md` with boss table and Code layout, five boss docs); `vault-of-archavon.md` note fixed.

### Group 1: Beasts

Files: `src/Ai/Raid/ToC/Action/ToCActions_{Gormok,Jormungars,Icehowl}.{h,cpp}`,
`src/Ai/Raid/ToC/Trigger/ToCTriggers_{Gormok,Jormungars,Icehowl}.{h,cpp}`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_{Icehowl,NorthrendBeasts}.{h,cpp}`,
`src/Ai/Raid/ToC/Util/ToCHelpers_{Gormok,Jormungars,Icehowl}.{h,cpp}`.

5. Gormok helpers: `GORMOK_IMPALE_SWAP_STACKS`, `GetGormokImpaleStacks`.
6. Gormok actions (3) and context. `GormokMainTankHoldBossAction` takes the drag refactor with
   `ARENA_CENTER`. `GormokTankSwapTauntAction` calls `CastClassTaunt(botAI, gormok)`; `CastTankTaunt`
   is not carried over.
7. Gormok triggers (3), context, nodes (3).
8. Jormungars helpers: `ToCDisplayIds` (comment naming `boss_northrend_beasts.cpp` `Model`),
   `IsWormMobile`, `GetWormCastingSweep`. `IsBotInFrontalCone` is not carried over.
9. Jormungars actions (6) and context. `FindWorm` goes to an anonymous namespace in
   `ToCActions_Jormungars.cpp`; `WormsAvoidSlimePoolAction` keeps `AvoidCreatureClusterAction` from
   the Shared stem.
10. Jormungars triggers (6), context, nodes (6). `WormsSweepFrontalTrigger` calls
    `EncounterHelpers::IsBotInFrontalCone`.
11. Icehowl helpers: `IsBotInChargeCorridor`, `HasMassiveCrashAura`.
12. Icehowl actions (2) and context; `IcehowlMainTankHoldBossAction` takes the drag refactor with
    `ARENA_CENTER`. Triggers (2), context, nodes (2).
13. `IcehowlSuppressMovementDuringChargeMultiplier` and `AddToCIcehowlMultipliers`;
    `NorthrendBeastsControlTankMovementMultiplier`, `AddToCNorthrendBeastsMultipliers`,
    `ToCNorthrendBeastsBurstWindow`.

### Group 2: Jaraxxus

Files: `src/Ai/Raid/ToC/{Action/ToCActions,Trigger/ToCTriggers,Multiplier/ToCMultipliers,Util/ToCHelpers}_Jaraxxus.{h,cpp}`.

14. Helpers: `JaraxxusHasNetherPower`, `GetPriorityJaraxxusAdd`, `GetSecondaryJaraxxusAdd`.
15. Actions (8) and context. `JaraxxusMainTankHoldBossAction` takes the drag refactor with
    `ARENA_CENTER`; `JaraxxusAvoidLegionFlameAction` keeps the Shared `AvoidCreatureClusterAction`.
16. Triggers (8), context, nodes (8, with the Incinerate Flesh priority comment).
17. `JaraxxusControlTankMovementMultiplier`, `AddToCJaraxxusMultipliers`, `ToCJaraxxusBurstWindow`.

### Group 3: Faction Champions

Files: `src/Ai/Raid/ToC/{Action/ToCActions,Trigger/ToCTriggers,Multiplier/ToCMultipliers,Util/ToCHelpers}_FactionChampions.{h,cpp}`.

18. Helpers: the `ToCFactionChampions` roster enum with its comment, `IsFactionChampion`,
    `IsFactionChampionHealer`, `FactionChampionsEncounterActive`, `GetPriorityFactionChampion`,
    `GetCcFactionChampionHealer`; `CollectAliveFactionChampions` stays in an anonymous namespace.
19. Action (1) and context; trigger (1), context, node (1, with the interrupts comment).
20. `FactionChampionsSuppressAoeMultiplier`, `AddToCFactionChampionsMultipliers`,
    `ToCFactionChampionsBurstWindow`.

### Group 4: Twin Val'kyr

Files: `src/Ai/Raid/ToC/{Action/ToCActions,Trigger/ToCTriggers,Multiplier/ToCMultipliers,Util/ToCHelpers}_TwinValkyr.{h,cpp}`.

21. Helpers: `TwinValkyrEncounterActive`, `HasLightEssence`, `HasDarkEssence`, `HasAnyEssence`,
    `TwinValkyrLightVortexActive`, `TwinValkyrDarkVortexActive`, `GetTwinCastingPact`.
22. Actions (6), `TwinValkyrEssenceActionBase`, context. `TwinValkyrMainTankHoldLightTwinAction` takes
    the drag refactor with `ARENA_CENTER`.
23. Triggers (6), context, nodes (6, with the overview comment on the first node).
24. `TwinValkyrControlTankMovementMultiplier`, `TwinValkyrPrioritizeEssenceSwapMultiplier` (its `.cpp`
    includes `ToCActions_TwinValkyr.h`), `AddToCTwinValkyrMultipliers`, `ToCTwinValkyrBurstWindow`.

### Group 5: Anub'arak

Files: `src/Ai/Raid/ToC/{Action/ToCActions,Trigger/ToCTriggers,Multiplier/ToCMultipliers,Util/ToCHelpers}_Anubarak.{h,cpp}`.

25. Helpers: `AnubarakSubmerged`, `AnubarakLeechingSwarmActive`, `GetNearestPermafrost`.
26. Actions (6) and context. `AnubarakMainTankHoldBossAction` takes the drag refactor with
    `ANUBARAK_PIT_CENTER`.
27. Triggers (6), context, nodes (6).
28. `AnubarakControlTankMovementMultiplier`, `AnubarakProtectSpikeKiteMultiplier`,
    `AnubarakDelayBloodlustUntilLeechingSwarmMultiplier` in that order, `AddToCAnubarakMultipliers`,
    `ToCAnubarakBurstWindow` (default; the lust gate stays its own multiplier until `w0c`).

### Integrate (done)

30. Delete `src/Ai/Raid/ToC/ToC{Actions,Triggers,Multipliers,Helpers}.{h,cpp}`. Grep `src/` for any
    include of those four headers. Check that no new header collides case-insensitively with another
    header under `src/`.
31. Parity against `HEAD`: the ordered `(trigger, action, priority)` list from every
    `AddToC*TriggerNodes`, in dispatch order, equals the old `InitTriggers`; the creator key sets
    equal the old contexts' (32 + 32); the multiplier class order equals the old `InitMultipliers`;
    every `: Trigger(botAI, "…")` string is unchanged.
32. Checks per the overview: syntax check on every `.cpp`/`.h` under `src/Ai/Raid/ToC` plus
    `src/Bot/Engine/BuildShared{Action,Trigger}Contexts.cpp` with `PB_MAX_FANOUT=60`; pblint with
    `--warnings` over the three scoped paths must match the baseline above; the Python suite.
33. Fix what the checks find inside the lane's files, then close per the overview.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Per-stem context classes, merged by the root contexts (`Absorb`); `BuildShared*` untouched | Scope 2 seam and pblint's one-line class rule; `SharedNamedObjectContextList::Add` already merges by copying creators | Free `Register<Stem>` functions plus a pblint extension; or eight `Add` lines per `BuildShared*` file |
| Node list in the Trigger stem (`AddToC<Stem>TriggerNodes`), multiplier list in the Multiplier stem; fixed root dispatch order | Scope 2; ties pop in insertion order (`action-selection.md`) | Per-stem strategy files |
| Seams exist for all eight boss stems, the empty ones header-only and inline | Wave 3 runs five lanes in parallel; a root edit per lane would collide | Lanes add their own dispatch line to the root |
| `ToCNpcs`/`ToCSpells` stay whole in `ToCData.h`; single-stem enums move (roster to `FactionChampions` per the w3a brief, display ids to `Jormungars`); new ids go in the stem's Util header | Scope 1 "core enum mirrors"; `UldData.h`'s raid-wide-only rule | Split both enums per stem now: about 100 renamed use sites that `w0c`'s constexpr conversion rewrites again |
| Util files named `ToCHelpers_<Stem>` | Continues `ToCHelpers` and its namespace | Ulduar's `UldEncounter_<Boss>` |
| Umbrellas `ToCRaidActions.h`, `ToCRaidTriggers.h`, `ToCRaidMultipliers.h`; no Util umbrella | Scope 3 collision with `Dungeon/TOC/TOC*.h` | `ToCActions.h` etc., which collide case-insensitively |
| Drag helper is the base class `ToCMainTankHoldAction::DragBossToAnchor` | `MoveTo` is protected | Free step helper, leaving five `MoveTo` calls |
| Burst hook `ToCBurstWindow{allowAll, allowLust}` with one `ToC<Stem>BurstWindow` per encounter stem, default allow | Ulduar's `UlduarBurstWindowMultiplier::BurstWindow`; w0c scope 5 | Leave the signature to `w0c`, which owns all of ToC then |
| Positions `extern` in `ToCData.h`, defined in `ToCData.cpp` | Ulduar's `extern const Position` pattern | `inline const` in the header |
| Investigator wrote all docs; boss docs hold the moved mechanics and a one-line "What a trace answers" | Pure move; boss lanes rewrite them | Each group writes its boss doc |
| pblint baseline recorded by the investigator | It must precede every edit, and it is read-only | Integrate rebuilds it from `HEAD` in a temp worktree |
| No guide, navprobe or spell-id work | Pure move: no tactic, coordinate or id changes | n/a |
| Movers dropped the bare section labels (`// Gormok the Impaler`, `// Northrend Beasts - Icehowl`, `// Lord Jaraxxus`, `// Faction Champions`, `// Twin Val'kyr`, `// Anub'arak`, …); every other comment is verbatim | No-nonsense-comments rule: a file holding one stem makes its label redundant | Copy the labels too |
| A moved `.cpp` keeps only the `using` directives and includes it uses: no `EncounterHelpers` in `ToCMultipliers_{Jaraxxus,FactionChampions,NorthrendBeasts}.cpp`, `ToCTriggers_FactionChampions.cpp` or the Util files that never call it | Include what you use, as in the `Shared` stem | The mover rule's "keep both `using namespace` lines" everywhere |
| `CastTankTaunt` and ToC's `IsBotInFrontalCone` leave with their comments; no forwarding stubs | Scope 4; each had one caller | Keep them as thin wrappers |
| Root `Absorb` logs a creator key two stems register (`LOG_ERROR`) and keeps the first | A duplicate spread over eight stem headers would otherwise fail silently; runtime check needs no pblint change | A pblint duplicate-key check across same-kind contexts |
| Boss docs corrected where the moved summary contradicted the script: Icehowl Trample, Staggering Stomp, Jaraxxus interrupt/dispel targets, Twins Touch and Vortex, Anub'arak phases | Decisions: the script wins | Move the summaries verbatim and leave them to the boss lanes |

## Carried over

- Stale `RaidBossHelpers` mentions outside this lane's files (the helpers are
  `src/Util/EncounterHelpers.{h,cpp}`): `docs/engine/pitfalls.md:268`, `eye-of-eternity.md:307`,
  `naxxramas.md:359`, `obsidian-sanctum.md:481`, `docs/raids/README.md:126,148`,
  `vault-of-archavon.md:106`. For the `w6-closeout` docs pass.
- `w0b-tooling.PLAN.md` scope 2 reads champion entries from `src/Ai/Raid/ToC/ToCHelpers.h`; after this
  lane they are in `Util/ToCHelpers_FactionChampions.h`. For the merge stage.
- Sweep 66794 → 67644/67645/67646: `GetWormCastingSweep` misses 25N/10H. For `w0c-foundation`'s
  spell-id pass.
- `AnubarakFocusBurrowerAction` keeps, verbatim, a comment ending "forcing it onto Permafrost is a
  future refinement": a future-work pointer under the no-nonsense-comments rule. For `w5-anubarak`.

## Known gaps

Re-review left no open issues and no deferred findings.

- Pure move: every behaviour bug in the overview's "Why" is still live, for `w0c-foundation` and the
  boss lanes.
- Never compiled: the syntax check is the only proof. The new subdirectories build only after the
  user re-runs CMake.
- Sweep dodge dead in 25N/10H (`GetWormCastingSweep`, carried over above); also in
  `northrend-beasts.md`.

## Blocked
