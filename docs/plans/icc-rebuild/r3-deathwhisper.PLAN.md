# r3-deathwhisper — Lady Deathwhisper, Rotting Frost Giants

Wave 3a, parallel with `r2-marrowgar`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounters

- **Lady Deathwhisper:** slug `lady-deathwhisper`, prefix `ldw.`, boss id 1, stem `Deathwhisper`
  (with `icc dark reckoning`). Core `boss_lady_deathwhisper.cpp`. Doc `lady-deathwhisper.md`, reader
  `lady_deathwhisper.py`.
- **Rotting Frost Giant:** slug `rotting-frost-giant`, prefix `rfg.`, no boss id (the `r1` unit
  gate), stem `FrostGiant`. Core `icecrown_citadel.cpp`. Doc `rotting-frost-giant.md`, reader
  `rotting_frost_giant.py`.
- Guides in the cache:
  - Deathwhisper: Warcraft Tavern 25 and 10, instant-carry's raid overview, Wowhead if saved;
  - giant: the dontstandinthefire blog.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1011 lines (`ICCActions_LDW.cpp`). The Mana Barrier aura sets the phase
  (`IccLdwBarrierUp`).
- The skull goes on the add nearest her within 25 yd, and never leaves a stale non-add.
  `HandleAddTargeting` has no caller.
- Hand-rolled flee vectors (`fleeX`, 12 hits).
- `IccLadyDeathwhisperMultiplier` always vetoes Flee, BlinkBack, Follow and Formation. With a Shade on
  the bot it vetoes everything but the shade action.
- Cheats (`IccAddsLadyDeathwhisperAction`):
  - `ldw.cyclone`: a Cyclone aura on a Dominate Mind victim, bot or human;
  - `ldw.insignificance`: the tank strips Touch of Insignificance.
  - Buffs: `ldw.nothreat`, `ldw.spitefulfury` (`EnableICCBuffs`, keep).
- Giant (`IccRottingFrostGiantTankPositionAction`): cheat `rfg.deathplague` strips Death Plague. The
  spread duty is commented out. The doc has core-bug issue text: the plague pick should also skip
  72865.
- Positions: 2 for Deathwhisper, 1 for Dark Reckoning, 1 for the giant, none navprobed.

## Guide answers to build on

- **Dominate Mind** takes 3 non-tanks for 12 s (the script: 1/1/3 on 25N/10H/25H, none on 10N). The
  raid stops cleaving, and a druid Cyclones each victim (it doesn't break on damage); any CC works.
  Victims can be humans.
- **Touch of Insignificance:** each stack cuts threat 20%. The guide says she can't be taunted, so tanks
  keep threat neck and neck with Misdirection and Tricks of the Trade, and Divine Shield drops the
  stacks. Check against the script whether taunts work on normal.
- **Adds:** a tank at each side's torches on the upper platform, physical and magic DPS split for the
  immune adds.
- **Heroic:** adds keep spawning in phase 2, Shades explode in an area, she can't be taunted.
- **Setup:** 2-3 tanks, 4-6 healers.
- **Death Plague:** pairs more than 10 yd apart. The plague hits after 15 s for 8k and spreads to
  everyone within 8 yd; with nobody to spread to, it kills its host. The giant also knocks back 20 yd.

## Focus

- A CC duty on each Dominate Mind victim, with ranked casters and Cyclone first.
- Threat parity and redirects on Touch.
- Add kill order by immunity, an interrupt duty for Frostbolt, the Death and Decay and Shade dodges,
  and heroic phase 2 adds.
- The giant's pair spread duty.

## Owns

Stems `Deathwhisper` and `FrostGiant` and their definitions, both docs, both readers and their tests,
this brief.

## Task list

## Decisions for review

## Carried over

- From `r0a-restructure`: `Action/ICCActions_FrostGiant.cpp` keeps the comment "Rotting Giant WiP
  (waiting for core fix)" and a commented-out `TODO` block, the only user of
  `ICC_ROTTING_FROST_GIANT_TANK_POSITION`. Both break the no-nonsense-comments rule.

## Known gaps

## Blocked
