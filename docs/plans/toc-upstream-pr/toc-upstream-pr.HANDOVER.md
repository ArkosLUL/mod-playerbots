## Pull Request Description

**Disclaimer:** the heroic bosses are a real challenge, Anub'arak most of all. For 25 heroic I'd recommend an average raid item level of 245 or higher.

Stacked on #2876 (the raid encounter framework) and #2985 (shared raid helpers), next to the Ulduar rework in #2986. Until those are merged this diff shows them too. This PR's part is `src/Ai/Raid/ToC/`, a few lines registering it in `RaidStrategyContext.h`, `BuildSharedActionContexts.cpp`, `BuildSharedTriggerContexts.cpp` and `PlayerbotAI.cpp`, and one SQL update for its chat line.

This adds a Trial of the Crusader raid strategy, `trialofthecrusader`. test-staging has none yet, only the Trial of the Champion dungeon. It covers all five encounters in 10 and 25 man, normal and heroic. I cleared it on 25 normal and 25 heroic, and the 10 man fights play the same.

Each boss is one encounter definition from #2876, with its triggers, actions, rules and multipliers behind a gate that reads the instance's progress, so a boss's logic only runs at its own stage. The layout follows Ulduar and EoE: `Action/`, `Trigger/`, `Multiplier/`, `Definition/` and `Util/`, one file per boss in each, 123 files and about 42.5k lines.

Bots don't use raid icons here. Each boss picks targets in code and puts the bots on them, and a skull you place still wins. Once you talk to Barrett, bots walk to set spots around where the next boss turns hostile, and only tanks stand close enough to take his first swing.

Per-boss notes:

- **Northrend Beasts**: bot tanks split the beasts between them. A human tank keeps whatever beast it's tanking, since bots can't hand it a new one.
- **Lord Jaraxxus**: if you main tank him, stand on him through the intro. Otherwise whoever stands nearest takes his first swings.
- **Faction Champions**: the fight starts with you. Bot tanks wait about 30 yd out, so every champion's first pick is a tank. To get a healer out of the pack, target it and type `pull`, and the bot DKs grip it out in a chain. Healers you don't pull die where they stand. Bring 3 or more DKs, so each melee champion can be kept in Chains of Ice.
- **Twin Val'kyr**: the raid fights from the entrance tunnel, the heroic "door strategy" from Wowhead, so nobody dodges orbs. Pull a twin and run into the tunnel, or tell a bot tank to `attack` one, and each bot tank fetches its own twin.
- **Anub'arak**: bring three tanks. The two side tanks should be bots, and your best geared ones. The main tank matters a lot less, even a decently geared second spec can do it. A human only takes a side no bot can, and then its side is the tank spot it stands on. Bring at least 3 DKs, so there are enough Death Grips to move the Frost Spheres where they're needed. Put healers, mages and moonkins in groups with a shadow priest or a shaman, so they get at least one source of passive healing.

## How to Test

1. Take a raid of bots into Trial of the Crusader, 10 or 25 man, normal or heroic. `trialofthecrusader` is applied automatically on map 649 with `Playerbots.ApplyInstanceStrategies = 1`, which is the default.
2. Talk to Barrett to start each boss. Read the notes above first: Faction Champions and the twins wait for you to pull.
3. Bots should take their spots before each pull and play each boss's mechanics on their own.

## Impact Assessment

**1. Performance cost at scale**
- [ ] None
- [x] Minimal (explain below)
- [ ] Moderate or high (explain below)

Nothing runs outside map 649, and a boss's logic only runs while its gate is open.

- Each boss keeps its world reads in shared per-instance state (`RaidInstanceState` from #2876), so the bots of a raid share one answer instead of each scanning on its own.
- Every group walk a trigger or multiplier reaches runs once per group per ms through `ToCGroupMemo` (`Util/ToCHelpers_Shared.h`).
- A boss with many rules asks once per action whether any of them cares, through the cares table in #2876.

**2. Does this change what bots do by default?**
- [ ] No
- [x] Yes (explain why the new default is better)

Only in Trial of the Crusader, which had no raid strategy before.

## AI Use and Self-Review

**Did you use AI while writing this change?**
- [ ] No
- [x] Yes (say what you used it for and which parts it wrote)

Yes. I used Claude Code to read the core's Trial of the Crusader scripts and spell data, write and refactor the strategy, and port it from my fork and clean it up for this repo's guidelines. It wrote all of the code. I haven't reviewed it line by line, there's too much of it. I tested it in game on my fork instead, on 25 normal and 25 heroic.

- [x] I have read and understood the [AC guidelines for AI Agentic Engineering](https://www.azerothcore.org/wiki/agentic-engineering) (required if you ticked Yes)

**Review before submitting**
- [ ] I ran a self-review. Tool and model: ______

## Code Provenance

Was any code copied or adapted from another project (CMaNGOS playerbots, MaNGOS, another module)?
- [x] No, all code is original
- [ ] Yes (name the project and original authors below)

## Checklist

- [x] New bot chat lines are translated (see comment above), or there are none.
- [x] New source files use the GPLv2 header.

## Notes for Reviewers

- It's a draft so people can test it on their servers. It builds against test-staging with gcc and clang and the CI flags, no warnings, and cppcheck comes back clean.
- Commits go: the raid-wide base, one per encounter, then a few small follow-ups. Only the tip builds, since the shared headers name every boss.
- It needs these core changes, all on test-staging already: azerothcore/azerothcore-wotlk#28117 (boss states for the instance), azerothcore/azerothcore-wotlk#28181 (Jormungar timers), azerothcore/azerothcore-wotlk#28136 (a Barrett per stage) and the DB update `2026_11_06_01` (the beasts' combat reach). `Util/ToCEncounterGate.h` copies the instance script's data keys by hand, since a module can't include `trial_of_the_crusader.h`.
- The only new chat line is a bot DK telling you its Death Grip is on cooldown and it's skipping its turn in the Faction Champions grip chain. Its translations are in `2026_11_18_00_ai_playerbot_toc_texts.sql`.
- Not cheats, but worth knowing: bots read where some enemies are moving (Anub'arak's spike, the twins and their orbs) and threat values (Jaraxxus, Anub'arak, Faction Champions) to plan moves and taunts.
- A few generic changes from my fork that this leans on aren't in here, and could be their own PRs: a raid healer engine that keeps a healer bot in combat mode when a boss leaves it without a target, keeping a heal going on a target that reads full health under a heal absorb (Jaraxxus's Incinerate Flesh), no near teleport delay in combat (a warlock's Demonic Circle on Anub'arak), no self res in a slime pool or next to two or more champions, and paladin "aura mastery" and "hand of sacrifice on party" actions.
- `ResolveAlive` (`Util/ToCHelpers_Shared.h`) returns a `boost::container::small_vector`. Boost is already a core dependency, but happy to switch to std if you'd rather.
- The biggest planners (the Jormungars' movement plan, Anub'arak's duties and kite planner) aren't split up. Happy to split them if you want.
