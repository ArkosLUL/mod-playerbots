## Pull Request Description

A raid strategy registers every boss of the raid at once. So today each boss's triggers check by hand whether their fight is on, and each boss gets its own multiplier that `dynamic_cast`s through the generic movement and targeting actions to keep them off its mechanics. That's the same boilerplate per boss, and it's easy to miss an action class and have a bot walk off its spot.

This adds a small framework for that:

- `EncounterDefinition` declares one boss in one place: its trigger/action rows, the rules that keep generic actions out of the way (`OwnMovement`, `OwnTargeting`, `Block`, `Exclusive`) and any hand-written multipliers. The raid's trigger and action contexts and its strategy are built from it.
- Everything a definition registers sits behind a gate. The default one reads the instance's boss states: closed once the boss is done or while another boss is in progress, open otherwise, so pre-pull steps still run.
- `ClassifyAction` is the one place that maps an `Action` to the families the rules talk about.
- `RaidInstanceState<T>` holds state shared by the bots of one raid instance and frees it when the instance map is destroyed. It's locked rather than `thread_local` because different maps update in parallel and can move between worker threads.
- `Trigger::getCheckInterval()`, so the gated wrapper keeps the check interval of the trigger it wraps.
- A once per tick hook for housekeeping a trigger shouldn't do: `Strategy::OnTick`, which the engine calls on every strategy before any trigger is checked, and `EncounterBuilder::Tick` to give a boss one. A boss's tick only runs while its gate is open.
- `EncounterTriggerPassId`, an id unique to one trigger pass, so a boss can cache a heavy scan for that pass. Triggers don't change the world, so the cached read holds until the pass ends. Actions and multipliers always get 0.
- `BossStateGateLive`, a gate that's only open while that boss is in progress.
- `EncounterHelpers::CommandPetAttack` and `StopPet`, moved here from #2877.
- Mocking Blow counts as a taunt now, since the Arms snare chain falls back to it.

Nothing uses it yet, so on its own this changes no behaviour. #2877 moves the Malygos strategy onto it, and the Ulduar rework (#HELPERS, #ULDUAR) is built on it too.

## Feature Evaluation

- Minimum logic: a gate check in front of each wrapped trigger and each rule, plus one classification per action.
- Processing cost: only bots running a strategy built on a definition pay anything. For those, all boss states are read once per bot per trigger pass and every gate in that pass reuses the read. A bot's whole tick runs on one map thread, so that pass cache is `thread_local`, keyed by bot and timestamp. Classifying an action is a chain of `dynamic_cast`s, done once per action and cached for the AI's lifetime. A rule checks the cheap things first (does it care about this action, is the gate open, does the bot have the role) and only then calls the boss's own predicate. The map script only erases the destroyed instance from each store. The tick hook is one call per active strategy per tick, and it's empty unless a strategy overrides it.

## How to Test the Changes

Nothing to see in game on its own, since no strategy uses it yet. Bots should behave exactly as before. To see it working, test #2877, which is stacked on this branch.

## Impact Assessment

- Does this change increase per-bot/per-tick processing or risk scaling poorly with thousands of bots?
    - - [ ] No, not at all
    - - [x] Minimal impact (**explain below**)
    - - [ ] Moderate impact (**explain below**)

The engine now calls `OnTick` on each active strategy once per tick, which is an empty call unless a strategy overrides it. Everything else only runs once a strategy is built on it.

- Does this change modify default bot behavior?
    - - [x] No
    - - [ ] Yes (**explain why**)

- Does this change add new decision branches or increase maintenance complexity?
    - - [ ] No
    - - [x] Yes (**explain below**)

It's about 1100 lines of new shared code to maintain. In exchange a boss strategy doesn't need its own gating and multiplier boilerplate anymore.

## AI Assistance

Was AI assistance used while working on this change?
- - [ ] No
- - [x] Yes (**explain below**)

Yes. I used Claude Code to read the core's Malygos script and spell data, write and refactor the strategy, and clean it up for this repo's guidelines. I reviewed all of it and tested it in game.

## Code Provenance / Attribution

Was any code in this PR copied or adapted from a sister / upstream project (e.g. CMaNGOS playerbots,
 MaNGOS, another module)?
- - [x] No, all code in this PR is original
- - [ ] Yes (**name the project and the original author(s) below**)

## Final Checklist

- - [x] Changes are understood and tested for server stability and performance impact.
- - [x] Any new bot dialogue lines are translated.
- - [x] New source files use the GPLv2 header.
- - [x] Documentation updated if needed (Code comments, Conf comments, WiKi commands).
- - [x] New and modified files do not introduce new compiler warnings.

## Notes for Reviewers

This is the base of a stack: #2877 (Malygos) is built on it, and so are #HELPERS and #ULDUAR (Ulduar). #2877 is the smallest user, so that's the place to see how a definition reads. I also run it on my fork for Naxxramas, Obsidian Sanctum and Ulduar.

