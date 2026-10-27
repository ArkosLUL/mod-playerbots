# Festergut

Boss id 4, trace slug `festergut`, prefix `fg.` ([README](README.md)). Code: stem `Festergut`
(`src/Ai/Raid/ICC/*/ICC*_Festergut.*`). Mechanics: `boss_festergut.cpp`.

## Mechanics

- **Gas Spore**: 69278 (10) / 71221 (25) on 2 or 3 players every 40-45 s, first at 20-25 s and again
  20-25 s after each Pungent Blight. Both apply the same 12 s aura 69279 on every difficulty, whose
  burst (Blighted Spores, row 1990) inoculates the players near it. A spore pushes Vile Gas back to at
  least 20 s.
- **Gastric Bloat** row 2198 (72219/72551/72552/72553) on his victim every 15-17.5 s, first at
  12.5-15 s; 100 s, explodes at 10 stacks.
- **Vile Gas** 69240 (row 1987) on a random ranged player every 28-35 s.
- **Malleable Goo, heroic**: Putricide throws 72296 from the balcony at a random non-tank every
  15-20 s. The listener stamps each impact; its missile and impact ids carry Putricide as target
  (README, spell listeners).
- His state reaches `IN_PROGRESS` only at Putricide's balcony point, so the trace opens on the engage.

## Phase

`IccFestergutSporeCycle` is the one spore test, any group member carrying 69279; every phase test calls
it, and it publishes `fg.phase`: `spore` or `normal`.

## Tactics

- Tanks hold him at `ICC_FESTERGUT_TANK_POSITION`. At 6 Gastric Bloat a tank stops attacking and the
  other taunts.
- **Spores**: melee stack on the tank spot, ranged and healers 2 yd around a ranged spore spot. The
  spot is raid-wide (`fgSlotState`): slot 1, flipped to 2 for the rest of the cycle once goo lands
  within 12 yd of it; a 2 s gap in the spore node's calls ends the cycle and resets it to 1. The lowest
  guid non-tank spore carrier joins the melee unless the main tank carries one.
- **Goo**, outside a spore window only: a non-tank inside 12 yd of an impact flees to a ring 13, 16 or
  20 yd out, 10 yd from allies, then holds until the impact's 8 s are up (`festergutGooWaitUntil`),
  while the multiplier blocks every other mover. Inside one the spore spot flip answers goo.
- Out of spores the melee stack on the main tank and the ranged take a six-wide, 10 yd grid behind him.

## Cheats

None.

## Known gaps

- Vile Gas has no answer beyond the ranged grid's 10 yd spacing.
- A tank swap waits out the class taunt's cooldown; `IccCastClassTaunt` retries every 500 ms.
- No ICC trace yet carries these probes, so no figure here is measured.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `fg.phase` | per bot | `spore` while any group member carries Gas Spore, else `normal` |
| `fg.sporespot` | per ranged bot | The raid-wide ranged spore spot, `1` or `2`, on a change |
| `fg.goohold` | per bot | Heroic: `dodge` inside an impact's 12 yd, `hold` waiting it out, `none` after. A spore window stops the writes, so `hold` can outlast its impact there |

`tools/botobs/bosses/festergut.py`; its banner names declared keys the pull never wrote, sparing
`fg.goohold` on normal:

- `--phases`: the `fg.phase` steps;
- `--spores`: per window, the players who gained Gas Spore and the spots written inside it;
- `--goo`: impact count, and per bot its dodges and time held;
- `--tanks`: his target runs, swaps between tanks, peak Gastric Bloat;
- `--cheats`: this encounter's keys plus the raid-wide `icc.` ones.
