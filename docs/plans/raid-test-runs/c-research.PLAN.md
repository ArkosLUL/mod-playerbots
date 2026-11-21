Lane C of `docs/plans/raid-test-runs/raid-test-runs.PLAN.md` in mod-playerbots
(`G:\DevStuff\GitHub\azerothcore-wotlk-pb\modules\mod-playerbots`): read its Decisions and "Facts the work
rests on" sections first. Read-only research: write exactly one file,
`docs/plans/raid-test-runs/c-research.FINDINGS.md`, and nothing else; no git writes, no Docker, no
SOAP, no database connection (read SQL from files instead).

Automated test pulls will field a dedicated, all-Alliance pool of bots per raid; each boss scenario
declares the comp its fight needs, and the user reviews this file before any character is created.

1. Comp table. For every Ulduar boss (Flame Leviathan, Ignis, Razorscale, XT-002, Iron Assembly,
   Kologarn, Auriaya, Hodir, Thorim, Freya, Mimiron, General Vezax, Yogg-Saron, Algalon) and every Trial
   of the Crusader encounter (Northrend Beasts, Lord Jaraxxus, Faction Champions, Twin Val'kyr,
   Anub'arak), at 10 and 25: tank count, healer count, and the classes or specs the fork's strategy
   assigns duties to (extra tanks, kiters, interrupt rotations, dispellers, Misdirect, vehicle or portal
   squads, anything role-shaped). Take each from the strategy code (`src/Ai/Raid/Uld/`,
   `src/Ai/Raid/ToC/`) and the boss docs (`docs/raids/ulduar/`, `docs/raids/trial-of-the-crusader/`),
   cite file and line per row, and mark anything inferred rather than read as a guess.
2. Pool templates. Per raid (`uld`, `toc`), the union of what the bosses need: about 30-35 members, each
   with class, spec (the premade spec names in `conf/playerbots.conf.dist`), Alliance race and gender,
   ordered so the first 25 form a sound default 25-man and the first 10 a sound 10-man, and so every
   boss's comp at both sizes can be picked from the pool. Say which members exist only for specific
   bosses.
3. Pilot start paths for XT-002 (Ulduar, map 603) and Northrend Beasts (Trial of the Crusader, map 649),
   from the core scripts under `G:\DevStuff\GitHub\azerothcore-wotlk-pb\src\server\scripts\Northrend\Ulduar\Ulduar`
   and `...\Northrend\CrusadersColiseum\TrialOfTheCrusader` and the world SQL under
   `G:\DevStuff\GitHub\azerothcore-wotlk-pb\data\sql`: what starts the encounter (aggro, or Barrett
   Ramsey's gossip: NPC entry, gossip menu id, option index per difficulty); which earlier boss states or
   instance data it requires; what a wipe does (reset, despawn, respawn delay, whether the gossip option
   comes back, ToC's attempt counter); and two or three drop-point candidates per boss where a raid can
   stand before the pull, taken from the fork's own position constants or the boss's spawn row, with
   their source. The orchestrator checks them against the navmesh; never invent a coordinate.

Lead the file with a short summary of open questions for the user. Report: the file's path and those
open questions.
