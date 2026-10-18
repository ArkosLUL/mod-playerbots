## Pull Request Description

Stacked on #2876 (the raid encounter framework). Until that one's merged this diff shows its changes too. This PR is the commits from "Tell encounter code why a move was not issued" on.

These are the shared pieces the Ulduar rework (#ULDUAR) calls from several bosses:

- `MovementAction::TryMoveTo` returns a `MoveOutcome`, so encounter code with several spots to pick from can tell "no path there" apart from "still walking to the last one". `MoveTo` keeps its bool and now wraps it.
- `EncounterHelpers`: the nearest spot clear of several hazards, derived points snapped onto the floor, class taunts, frontal cone dodges and Nitro Boosts.
- `RaidTankDefensive`, `RaidHumanTankCall` and `BurstCooldowns` pick the defensive cooldown a bot should press ahead of a big hit, tell a human tank when to press theirs, and let an encounter hold burst cooldowns until a tank has held the boss for a while.
- `RaidAntiFear`, `RaidRedirectThreat` and `RaidSplash` are base classes an encounter subclasses for Fear Ward and Tremor Totem, or Misdirection and Tricks of the Trade, plus a check for actions that hit an area.
- `BossNatureAspectHoldMultiplier` keeps the nature resistance hunter in Aspect of the Wild. Without it the hunter's own aspect nodes recast Dragonhawk on the next GCD. `FindBossByName` scans all possible targets, since a bot on an add never has the boss on its threat list.
- `CastTrapLauncherExplosiveAction`, so a multiplier that holds Explosive Trap matches the trap launcher cast too.
- `RaidDifficulty.h` reads 10 or 25 man from the map instead of the group. A group left on 25 Heroic in Ulduar, Naxxramas or Eye of Eternity plays 25 Normal but still reports 25 Heroic, so a check on the group's setting treats it as 10 man.

Nothing calls the new pieces yet, so on its own this changes no behaviour.

## Feature Evaluation

- Minimum logic: helpers and base classes only. The logic runs in the encounters that use them.
- Processing cost: nothing until an encounter uses them. `MoveTo` does the same work as before, it just goes through `TryMoveTo` now. The hazard helpers only run when an encounter asks for a spot, and the nature resistance hold checks the action name before it looks up the hunter, since it runs for every queued action.

## How to Test the Changes

Nothing to see in game on its own. Bots should behave exactly as before, movement included, since `MoveTo` now goes through `TryMoveTo`. To see it working, test #ULDUAR, which is stacked on this branch.

## Impact Assessment

- Does this change increase per-bot/per-tick processing or risk scaling poorly with thousands of bots?
    - - [x] No, not at all
    - - [ ] Minimal impact (**explain below**)
    - - [ ] Moderate impact (**explain below**)

Nothing calls into it until an encounter uses it.

- Does this change modify default bot behavior?
    - - [x] No
    - - [ ] Yes (**explain why**)

- Does this change add new decision branches or increase maintenance complexity?
    - - [ ] No
    - - [x] Yes (**explain below**)

It's about 1400 lines of new shared code. In exchange the Ulduar bosses don't each carry their own copy.

## AI Assistance

Was AI assistance used while working on this change?
- - [ ] No
- - [x] Yes (**explain below**)

Yes. I used Claude Code to move these helpers out of my fork's raid strategies and clean them up for this repo's guidelines. I reviewed all of it.

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

- The nature resistance hunter only counts as eligible above the mana where Aspect of the Viper takes over (`LowMana / 2`), so the role only moves on a real mana drop.
- test-staging's `BossNatureResistanceTrigger` picks the first living hunter, while the hold uses that mana based pick, so the two can name different hunters.
- `RaidAntiFear`'s trigger walks the group in `IsActive`. Happy to put a cached value behind it.
- Nothing on test-staging creates `CastTrapLauncherExplosiveAction` yet, so the Explosive Trap hold only matches it once something does.
