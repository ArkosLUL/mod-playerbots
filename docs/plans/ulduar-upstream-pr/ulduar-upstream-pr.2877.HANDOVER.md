## Pull Request Description

Stacked on #2876 (the raid encounter framework). Until that one's merged this diff shows its changes too. Everything under `src/Ai/Raid/EoE/` is the Malygos part.

Strategy showcase - https://youtu.be/n-m21V0K5HE

What bots did on Malygos before:

- Power Sparks weren't handled. The `power spark` trigger was registered but never bound, and the grip and kill actions were commented out, so every spark reached Malygos and buffed him.
- Phase 2 had nothing for the Arcane Overload bubbles, the hover disks or Surge of Power. Everyone stood in the open, melee couldn't reach the Scions, and pets chased the airborne Scions.
- Phase 3 never used Flame Shield and ignored Static Field.

And now:

- P1: the tank parks Malygos facing away from the raid, on whichever side he landed. A death knight grips each Power Spark next to the melee stack and chains it, and dps kill the spark they can reach without moving, so it dies before it gets to him and its buff pool lands on the melee.
- P2: everyone on foot shelters in an Arcane Overload bubble, spread over the bubbles and skipping ones that are about to shrink away. Melee ride the freed hover disks up to the Scions, pets go on the Nexus Lords, and mages steal the Lords' Haste. A bot that can't reach a bubble steps out of the Surge of Power line.
- P3: the drakes park together at a fixed spot facing Malygos, drift off Static Fields, and put up Flame Shield right before Surge of Power lands on them. The healer drake count is capped by raid size (2 in 10 man, 5 in 25). After the kill the drakes keep following the leader's drake.
- Between phases the raid holds at the centre and the EoE rules stay on.

The strategy is now built on the framework from #2876 and split into `Action/`, `Trigger/`, `Util/` and `Definition/`. The old `EoEActions.{h,cpp}` and `EoETriggers.{h,cpp}` are removed: the new headers have the same names, and since every source folder is on the include path the old ones would shadow them. The pet redirect uses `EncounterHelpers::CommandPetAttack` and `StopPet`, which moved into #2876.

## Feature Evaluation

- Minimum logic: one trigger per mechanic, each only checked while its phase is on.
- Processing cost: nothing runs outside map 616.
    - The EoE creatures come from one grid sweep around the room's centre, at most every 300 ms per instance, and every bot reads that instead of searching on its own.
    - The phase is cached per instance for 500 ms and the P3 healer roster for 2 s. Who's holding Malygos is read once per bot per tick (a `thread_local` cache, since a bot's whole tick runs on one map thread).
    - Triggers that start something lasting several ticks (power spark, bubble, surge, free disk) check every 200 to 300 ms instead of every tick.
    - All of that state lives in `RaidInstanceState` and is freed with the instance.

## How to Test the Changes

1. Take a raid of bots into Eye of Eternity, 10 or 25 man. `wotlk-eoe` is applied automatically on map 616.
2. Pull Malygos and watch each phase:
    - P1: the tank turns him away from the raid, the DK grips sparks next to the melee, and sparks die before they reach him.
    - P2: bots on foot stand in the bubbles, melee board the disks and fight the Scions, mages spellsteal the Nexus Lords.
    - P3: the drakes stack facing him, move off Static Fields, use Flame Shield when Surge of Power hits, and healer drakes heal.
3. After the kill the drakes follow the leader's drake and don't fall.

## Impact Assessment

- Does this change increase per-bot/per-tick processing or risk scaling poorly with thousands of bots?
    - - [ ] No, not at all
    - - [x] Minimal impact (**explain below**)
    - - [ ] Moderate impact (**explain below**)

Only bots inside Eye of Eternity run any of it, and the expensive parts are shared per instance (see Feature Evaluation).

- Does this change modify default bot behavior?
    - - [ ] No
    - - [x] Yes (**explain why**)

Only on Malygos, which is the point of the PR. Nothing changes outside Eye of Eternity.

- Does this change add new decision branches or increase maintenance complexity?
    - - [ ] No
    - - [x] Yes (**explain below**)

EoE goes from 10 files to 24, about 2700 lines added and 700 removed. Most of it is the P2 and P3 mechanics, which had no handling before.

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

- I tested on my fork, whose `boss_malygos.cpp` matches test-staging's core. One difference worth knowing: test-staging's `IsTank` also counts any death knight in Frost Presence as a tank and my fork doesn't, so I haven't tested a non-blood DK in Frost Presence here.
- A few actions still do more than one job: `MalygosTargetAction` picks the target and redirects the pet, `MalygosRideDiskAction` flies the disk up, back down and dismounts, and `EoEDrakeAttackAction` runs both the dps and the healer rotation. Happy to split them if you'd rather have one job per action.

