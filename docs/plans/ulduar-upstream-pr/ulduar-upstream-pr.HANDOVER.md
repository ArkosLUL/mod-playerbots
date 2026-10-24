## Pull Request Description

[[ DISCLAIMER: Arkos ]]

Stacked on #2876 (the raid encounter framework) and #HELPERS (shared raid helpers). Until those are merged this diff shows them too. This PR's part is `src/Ai/Raid/Uld/`, the four hard mode options in `PlayerbotAIConfig` and `playerbots.conf.dist`, and its registration in `Playerbots.cpp` and `BuildSharedValueContexts.cpp`.

This replaces the Ulduar strategy. The old one is 11 files and about 7.5k lines and uses `BotCheatMask::raid` in 21 places. The new one covers all 14 bosses in 10 and 25 man, hard modes included, and doesn't use `BotCheatMask` at all.

It's built on #2876's encounter definitions: each boss has one definition with its triggers, actions, rules and multipliers, behind a gate that's closed once that boss is dead or while another boss is in progress. The layout follows EoE in #2877, so `Action/`, `Trigger/`, `Multiplier/`, `Definition/` and `Util/` with one file per boss in each. That's where the 140 files come from.

Bots never start a hard mode, they play the one the raid starts. Four bosses have an option, because their hard mode changes the plan before it shows up:

- `Playerbots.UlduarVezaxHardMode`: leave the Saronite Vapors alive and stop damaging Vezax at 15% (tanks at 2%) until the Animus spawns.
- `Playerbots.UlduarIronAssemblyHardMode`: kill Steelbreaker last.
- `Playerbots.UlduarXT002HardMode`: burn the Heart instead of stopping at 15%.
- `Playerbots.UlduarMimironHardMode`: the Firefighter positions, fire and Frost Bomb handling.

Flame Leviathan's towers, Freya's Elders, Thorim's Sif and Yogg-Saron's Keepers need no option. Bots react to those mechanics whenever they show up.

[[ PER-BOSS NOTES: Arkos ]]

## Feature Evaluation

- Minimum logic: per boss, one trigger per mechanic, each behind that boss's gate.
- Processing cost: nothing runs outside Ulduar.
    - Most of the heavy scans live in per instance state (`RaidInstanceState`) and refresh on a throttle, so the bots of a raid share them instead of searching on their own. The state is freed with the instance.
    - Mimiron and Yogg-Saron cache their heaviest scans for one trigger pass (`EncounterTriggerPassId`).
    - Flame Leviathan, Thorim, Mimiron, Yogg-Saron and Algalon use the once per tick hook from #2876 for their planners and housekeeping, and only while their gate is open.
    - A few reads are cached per bot per tick in `thread_local`s, since a bot's whole tick runs on one map thread.
    - Not everything is cached yet. About 20 triggers still scan nearby units or walk the group in `IsActive`, mostly on Yogg-Saron, XT-002, Mimiron, Freya and Hodir, plus group walks on Iron Assembly, Kologarn and Razorscale. They use short radii and only run while their boss's gate is open. Happy to move them behind cached values if you want that before merge.

## How to Test the Changes

1. Take a raid of bots into Ulduar, 10 or 25 man. `ulduar` is applied automatically on map 603 with `Playerbots.ApplyInstanceStrategies = 1`, which is the default.
2. Pull the bosses. The per boss notes above say what each fight should look like.
3. For a hard mode, start it the usual way (leave the towers up, keep the vapors alive, press Mimiron's button, ...). For Vezax, Iron Assembly, XT-002 and Mimiron, turn that boss's option on first.

## Impact Assessment

- Does this change increase per-bot/per-tick processing or risk scaling poorly with thousands of bots?
    - - [ ] No, not at all
    - - [x] Minimal impact (**explain below**)
    - - [ ] Moderate impact (**explain below**)

Only bots inside Ulduar run any of it, and only for bosses whose gate is open. See Feature Evaluation for the scans that aren't cached yet.

- Does this change modify default bot behavior?
    - - [ ] No
    - - [x] Yes (**explain why**)

Only in Ulduar, which is the point of the PR. The hard mode handling for Vezax, Iron Assembly, XT-002 and Mimiron stays off unless its option is on.

- Does this change add new decision branches or increase maintenance complexity?
    - - [ ] No
    - - [x] Yes (**explain below**)

Ulduar goes from 11 files and about 7.5k lines to 140 files and about 53k lines.

## AI Assistance

Was AI assistance used while working on this change?
- - [ ] No
- - [x] Yes (**explain below**)

Yes. I used Claude Code to read the core's Ulduar boss scripts and spell data, write and refactor the strategy, and port it from my fork and clean it up for this repo's guidelines. I haven't reviewed the code line by line, there's too much of it. I tested it in game on my fork instead, over more than 10 Ulduar lockouts.

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

- It's a draft so people can test it on their servers. I tested it over more than 10 Ulduar lockouts on my fork, not on test-staging's core. The build matrix on this PR will be the first full build against test-staging.
- Commits go: remove the old strategy, add the shared files, one commit per boss, then the config options. Only the tip builds, since the shared headers name every boss.
- My fork's core makes a few Ulduar spells resistible that test-staging's core doesn't: Algalon's Cosmic Smash (fire), Mimiron's Water Spray and Sif's spells (frost), and Yogg-Saron's shadow damage. The resistance aura picks for those don't help against them on test-staging's core.
- Not cheats, but worth knowing: Mimiron hands the Magnetic Core to a bot with `StoreNewItem`, and bots that fall through the floor on Auriaya, Kologarn, Thorim and Yogg-Saron get put back with `NearTeleportTo`.
- The only new chat line is Razorscale's "set as main tank" yell, which comes over from the old strategy.
- This also replaces the XT-002 strategy, so it overlaps #2653.
- Like #2877, a few actions still do more than one job. Happy to split them if you'd rather have one job per action.
