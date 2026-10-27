# Lord Marrowgar

Boss id 0, slug `lord-marrowgar`, prefix `lm.`. Code: `Action/ICCActions_LM.cpp`, nodes `icc lm` →
`icc lm tank position` (`ACTION_RAID + 5`), `icc spike` (`+ 3`). Raid conventions:
[README.md](README.md).

## Mechanics (core)

- **Bone Storm** 69076, no difficulty row: a 3 s cast, first at 45-50 s, then every 90-95 s. The aura
  lasts 20 s (10-man) or 30 s (25-man); he charges every 5 s at the farthest player (25-man: one of
  the two farthest) and drops Coldflame at each stop. Heroic keeps casting Bone Spike Graveyard
  inside it.
- **Bone Spike Graveyard** every 15-20 s: one spike in 10-man, three in 25-man (base entries 36619,
  38711, 38712; `GetEntry` reports the base entry on every difficulty, so the listed difficulty
  entries never match). The victim rides the spike with Impaled 69065.
- He attacks nothing past x = -337, the main gate (`CanAIAttack`).

## Phase

`IccLmInBoneStorm` reads the 69076 aura on him, so `lm.phase` turns `bonestorm` when he starts to
spin, not at the cast. Both actions call it.

## Assignments

- **Storm anchors** (`IsBoneStormAnchor`): the two lowest-guid ranged non-tanks, hunters first, hold
  `ICC_LM_BONE_STORM_AT_POSITION` to draw his charges; everyone else keeps to the tank spot or mid.
- **Spikes**: every bot marks them, skull, cross and star on the spikes in guid order; a lone spike
  holding a tank takes skull with cross on him. The assist tank takes the tank's spike, melee the
  nearest spike within 20 yd that is out of his front and Coldflame, ranged and healers split evenly
  across all spikes by guid (`IccBalancedGroupSizes`), each on its bucket's icon.

## Cheats

None, and no class-taunt caller.

## Known gaps

- Raid-icon targeting (README Known gaps).

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `lm.phase` | per bot | `normal`, `bonestorm` |
| `lm.stormanchor` | per bot | `1` while picked as a storm anchor; written only inside a storm |
| `lm.mark` | per bot | The icon rule: `spikesN` (one icon per spike), `tankspike`, `boss` |
| `lm.spike` | per bot | The spike duty: `skull`/`cross`/`star` (ranged bucket), `nearest`, `nosafe`, `tank`, `idle` (assist tank, no tank spike), `boss`, `none` |

`tools/botobs/bosses/lord_marrowgar.py`: `--phases`, `--storms` (each storm's anchors, counting a
bot that kept `1` from the last storm), `--spikes` (icon steps, who held each duty), `--cheats`.
