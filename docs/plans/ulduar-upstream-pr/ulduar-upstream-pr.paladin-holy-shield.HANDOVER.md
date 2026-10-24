## Pull Request Description

Protection paladin bots almost never have Holy Shield up. The tank strategy only casts it from the `medium health` trigger, so a tank above 65% health never presses it, and the `holy shield` trigger is registered but no strategy uses it. On a Freya kill on my fork a bot tank had it up on 8% of the boss's swings, while a player paladin in the same pull had 75%.

The tank strategy now uses the `holy shield` trigger at `ACTION_HIGH + 10`, and both the trigger and the action recast it in its last 2 seconds. Holy Shield lasts 10 seconds on an 8 second cooldown, so that keeps it up. Both need the margin, or the action turns down the early recast the trigger asks for.

## Feature Evaluation

- Minimum logic: the existing `holy shield` trigger wired into the tank strategy, plus a refresh margin on the trigger and the action.
- Processing cost: one buff check per protection paladin instead of a health check. No scans.

## How to Test the Changes

1. Put a protection paladin bot on a boss and keep it above 65% health.
2. Holy Shield should be up nearly all the time, recast every 8 seconds or so. Before this change it only went up once the tank dropped below 65%.

## Impact Assessment

- Does this change increase per-bot/per-tick processing or risk scaling poorly with thousands of bots?
    - - [x] No, not at all
    - - [ ] Minimal impact (**explain below**)
    - - [ ] Moderate impact (**explain below**)

- Does this change modify default bot behavior?
    - - [ ] No
    - - [x] Yes (**explain why**)

Protection paladins keep Holy Shield up, which costs them a global cooldown and some mana every 8 seconds.

- Does this change add new decision branches or increase maintenance complexity?
    - - [x] No
    - - [ ] Yes (**explain below**)

## AI Assistance

Was AI assistance used while working on this change?
- - [ ] No
- - [x] Yes (**explain below**)

Yes. I used Claude Code to find why the uptime was low from my combat logs, write the fix on my fork and port it to test-staging.

## Code Provenance / Attribution

Was any code in this PR copied or adapted from a sister / upstream project (e.g. CMaNGOS playerbots,
 MaNGOS, another module)?
- - [x] No, all code in this PR is original
- - [ ] Yes (**name the project and the original author(s) below**)

## Final Checklist

- - [ ] Changes are understood and tested for server stability and performance impact.
- - [x] Any new bot dialogue lines are translated.
- - [x] New source files use the GPLv2 header.
- - [x] Documentation updated if needed (Code comments, Conf comments, WiKi commands).
- - [ ] New and modified files do not introduce new compiler warnings.

## Notes for Reviewers

- Priority 30 puts it above Seal of Wisdom (29) and the rest of the tank strategy's threat and mana actions, and below interrupts (40).
- The 2 second margin is `HOLY_SHIELD_REFRESH_MS` in `PaladinHelper.h`, used by both constructors.
