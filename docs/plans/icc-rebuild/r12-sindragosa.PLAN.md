# r12-sindragosa — Sindragosa, her gauntlet, Rimefang and Spinestalker

Wave 3f, alone. Rules, conventions and the boss lane recipe: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md).
Layout and seams: `docs/raids/icecrown-citadel/README.md`, "Code layout". It may split.

## Encounters

- **Sindragosa:** slug `sindragosa`, prefix `sg.`, boss id 11, stem `Sindragosa`. Core
  `boss_sindragosa.cpp`, which also holds Rimefang and Spinestalker. Doc `sindragosa.md`, reader
  `sindragosa.py`. Guides in the cache: Warcraft Tavern 25 and 10, instant-carry's raid overview,
  Wowhead if saved.
- **Rimefang and Spinestalker:** no boss state. Use `r1`'s unit gate, with the slug and prefix of
  Sindragosa's encounter.
- **Sindragosa gauntlet:** slug `sindragosa-gauntlet`, prefix `sg.`, boss id 13. Core
  `icecrown_citadel.cpp`. Reader `sindragosa_gauntlet.py`. Guide: the mischiefmagicandrocketscience
  blog. It has no node today.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1635 lines (`ICCActions_SG.cpp`). Her trigger finds her with a 200 yd
  `FindNearestCreature`. The phase comes from her react state (`IccSindragosaPhase`).
- 24 icon writes (`SgSetIcon`).
- What is handled today:
  - the tank spot, drag and facing;
  - the Blistering Cold run;
  - Unchained Magic and Chilled to the Bone;
  - beacon tomb spots and air-phase hide groups (none probed);
  - the last phase's line-of-sight hide and tomb burn or hold.
- `IccSindragosaMultiplier`:
  - always: the tanks' AttackRti;
  - below 95%: Formation, Flee, Follow and Starfall;
  - heroic Unchained Magic: everything but movement and the Unchained Magic action;
  - Blistering Cold: everything but its action and heals;
  - a beacon: everything but the beacon action;
  - allowlists for tanks in the last phase, and a list of about 25 classes over the bomb station.
- `IccSindragosaGroupPositionTrigger` acts inside `IsActive`:
  - `UseNitroBoosts`;
  - the raid cheats:
    - `sg.frostbeacon`: the main tank's last-phase beacon strip;
    - `sg.mysticbuffet`: the tanks' Mystic Buffet strip;
    - `sg.magicbarrier`: Magic Barrier on tanks;
    - `sg.nothreat`: 1% Threat on assist tanks.
  - `r1` moved its buffs out; the cheats are this lane's.
- Other cheats:
  - `sg.flares`: smoke flares from `StoreNewItemInBestSlots` in `TryDropTombFlares`. Red flares burn
    out after 5 min.
  - `sg.tombstrip`: tomb aura strips in `IccSindragosaMysticBuffetAction` and
    `IccSindragosaFrostBombAction`.
- 18 positions, none navprobed.

## Guide answers to build on

- **Tanks:** 2 rotate, and a tank free of Mystic Buffet swaps in before every Frost Breath. Personal
  cooldowns, Frost Resistance, and Hand of Freedom against the Frost Breath slow.
- **Air phase:** Frost Beacons (heroic one more than normal; check the script) turn into Ice Tombs
  placed 10 ft apart in 3 rows of 2. They are cover for 4 Frost Bombs about 7 s apart, so don't break
  them early (Asphyxiate after 20 s).
- **Unchained Magic:** stay at 2-3 stacks or fewer. On heroic, Instability bursts within 20 yd, so the
  debuffed move out of the raid.
- **Blistering Cold:** run out.
- **Last phase:** a Frost Beacon every 15 s. Hide behind a tomb from Mystic Buffet and stay at 3
  stacks or fewer; auto attack must be off for the line of sight to count.
- **Setup:** attack from the sides, 1-2 tanks (2 to swap in the last phase), 4-8 healers.

## Focus

- The tank swap timed to Frost Breath.
- Tomb spots computed from the beacon count and navprobed, with line-of-sight checks for the bomb
  hide.
- Unchained Magic stack control and the heroic run out.
- Blistering Cold, and Chilled to the Bone stacks.
- The last phase's tomb hide with auto attack off, and tomb break timing.
- The Frost Resistance aura through `r1`'s election.
- Replacing all six raid cheats and the flares.
- The gauntlet and the two drakes. Direct targeting.

## Owns

Stem `Sindragosa` and its definition (and one for the drakes and the gauntlet if needed), the doc, both
readers and their tests, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
