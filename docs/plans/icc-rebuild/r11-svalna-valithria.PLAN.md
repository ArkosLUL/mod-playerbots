# r11-svalna-valithria — Sister Svalna and her gauntlet, Valithria Dreamwalker

Wave 3e, parallel with `r10-blood-queen`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout". It may split.

## Encounters

- **Sister Svalna** and the gauntlet before her: slug `sister-svalna`, prefix `ss.`, boss id 9, stem
  `Svalna`. Core `icecrown_citadel.cpp`. Doc `sister-svalna.md`, reader `sister_svalna.py`. Guide:
  the mischiefmagicandrocketscience blog.
- **Valithria Dreamwalker:** slug `valithria-dreamwalker`, prefix `vd.`, boss id 10, stem `Valithria`.
  Core `boss_valithria_dreamwalker.cpp`. Doc `valithria-dreamwalker.md`, reader
  `valithria_dreamwalker.py`. Guides in the cache: Warcraft Tavern 25 and 10, instant-carry's raid
  overview, Wowhead if saved.

## Facts as of `824468439`

Sites are named by function.

- **Svalna:**
  - 66 lines: a spear click, and a spear throw while she is shielded (Aether Shield).
  - Every bot within 100 yd, tanks and healers included, runs for the spear at emergency priority.
- **Valithria:**
  - The action stem was 1272 lines (`ICCActions_VT.cpp`).
  - What is handled today:
    - add taunts and flees, and marks (25-man splits the room in halves);
    - the zombie kite;
    - a healer ranking, which counts humans and never ranks a selfbot;
    - portals, a heal rotation and dream clouds.
  - 7 icon writes. 5 `FleePosition` calls (Mana Void, the exploding zombie).
  - `IccValithriaDreamCloudMultiplier`:
    - always: Follow and Formation;
    - chased by a zombie: everything but the kite;
    - in Dream State above 50% health: everything but the cloud action;
    - icon and assist filters by role.
  - Cheats:
    - `vd.dreamteleport`: a same-map `TeleportTo` onto a human in Dream State;
    - `vd.cloudteleport`: the same onto the shared cloud.
    - Both drop combat. With the raid cheat off, a Dream State bot hovers.
  - The floor snaps (`NearTeleportTo`) skip fall damage; they're borderline.
  - Its 12 raw-id casts are all guarded by known-spell checks.
  - Heroic Nightmare portals and Twisted Nightmares are handled.
  - 4 positions and 4 Z constants.

## Guide answers to build on

- **Portals** spawn every 45 s, open 15 s later, and take up to 8 players in 25. 1-2 healers stay
  outside.
- **Dream State** is 20 s of free flight. Fly into the clouds together for Emerald Vigor, take a
  cloud at once as old stacks run out, and fly low before Dream State ends.
- **Setup:** 5-10 healers and 1 tank for the abominations. Ranged kite the zombies.
- **Kill order:** Skeletons and Archmages, then Worms and Suppressers, then Zombies and Abominations.
- **Heroic:** Twisted Nightmares replaces Emerald Vigor, a stacking DoT, so healers heal each other.
- **Svalna:** one raider throws a spear to break Aether Shield.

## Focus

- Real flight in Dream State: a flight mover through the portal and between clouds, from the
  script's Dream State and flight auras. The floor at the end, without a snap.
- The healer split inside and outside, ranking bots only.
- The add kill order by direct targeting.
- Mana Void, Column of Frost and zombie dodges on hazard models.
- Svalna: one elected spear thrower, and the gauntlet's captains.

## Owns

Stems `Svalna` and `Valithria` and their definitions, both docs, both readers and their tests, this
brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
