## Pull Request Description

Shaman bots cast Heroism or Bloodlust whenever the `balance` value drops to 50 or below, and a big enough trash pull gets it there. The spell has a 10 minute cooldown and leaves everyone Sated for 10 minutes, so after a lust on trash the next boss gets none. They also cast it while they're Sated or Exhausted from someone else's lust, which spends the cooldown on a group that can't take it.

With this change a shaman only lusts on a dungeon or raid boss, or on a player target like before, and never while Sated or Exhausted. Heroism and Bloodlust both use a new `ShamanBoostTrigger`; the generic `BoostTrigger` is unchanged.

## Feature Evaluation

- Minimum logic: two aura checks and a boss check on the current target, inside the existing lust trigger.
- Processing cost: about the same as before. The trigger reads two auras and the target's creature flags instead of the `balance` value, and returns early while Sated or Exhausted.

## How to Test the Changes

1. Take a group with a level 80 shaman bot into a dungeon or raid and pull a big trash pack. It shouldn't cast Heroism or Bloodlust.
2. Pull a boss. The shaman lusts once the boss is its target.
3. Have someone else lust first, then pull a boss. The shaman doesn't cast it while it's Sated or Exhausted.

## Impact Assessment

- Does this change increase per-bot/per-tick processing or risk scaling poorly with thousands of bots?
    - - [x] No, not at all
    - - [ ] Minimal impact (**explain below**)
    - - [ ] Moderate impact (**explain below**)

- Does this change modify default bot behavior?
    - - [ ] No
    - - [x] Yes (**explain why**)

Shamans no longer lust on trash or while Sated. That's the point of the change.

- Does this change add new decision branches or increase maintenance complexity?
    - - [ ] No
    - - [x] Yes (**explain below**)

One small trigger subclass with the Sated and boss checks.

## AI Assistance

Was AI assistance used while working on this change?
- - [ ] No
- - [x] Yes (**explain below**)

Yes. I used Claude Code to write this on my fork, where my raids have used it, and to port it to test-staging.

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

- The boss check is the same one `DebuffOnBossTrigger` uses, `IsDungeonBoss() || isWorldBoss()`.
- The trade-off: bots won't lust on a trash pack even when it's about to wipe the group.
