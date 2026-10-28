# Lord Marrowgar

Boss id 0, slug `lord-marrowgar`, prefix `lm.`. Code: stem `Marrowgar` (`src/Ai/Raid/ICC/*/ICC*_Marrowgar.*`),
`Definition/ICCDefinition_Marrowgar.cpp`. Raid conventions: [README.md](README.md). Guides: Warcraft
Tavern 25 and 10, Wowhead, instant-carry.

## Mechanics

Core: `boss_lord_marrowgar.cpp`. Ids are 10N / 25N / 10H / 25H; one id means no difficulty row (CSV
and world DB checked). His combat reach is 20.25. Timers run from the engage.

| Ability | Core | Guide | Bots |
|---|---|---|---|
| Bone Slice 69055 / 70814 / 69055 / 70814 | Every swing from 10 s in and 10 s after each storm. 5 yd round his victim (SpellInfoCorrections, not the guides' 8), at most 2 (10) or 3 (25) random targets, damage split. Hit players can't be spiked. | 2-3 tanks stacked, everyone else 8 yd off the main tank | Off-tanks 2 yd beside the main tank, everyone else 10+ yd off |
| Coldflame 69140, patch 69146 / 70823 / 70824 / 70825 | Every 5 s outside storms, at a random player over 22.75 yd (3D) from his centre who is not his last victim or Impaled, else at anyone. A head (36672) appears 10.1 yd out on that bearing, lays a patch where it stands every 450 ms and steps 5 yd, for 12 s or until a wall. Patch: 3 yd (SpellInfoCorrections) plus the player's 1.5, 3 s, 1 s ticks of 6k / 8k / 9k / 11k. | 8 (25) or 3 (10) out of melee, melee inside his hitbox | Baits on a ring 31 yd out, everyone else within 3 yd of his centre, where no line reaches; a dodge |
| Bone Spike Graveyard 69057 / 70826 / 72088 / 72089 | 3 s cast at 10-15 s, then every 15-20 s; normal skips storms and re-arms 15-20 s after. 1 (10) or 3 (25) spikes (36619, 38712, 38711) on random players within 150 yd but his victim and the last Bone Slice targets. Impaled 69065: 10% max HP a second. | Melee cleave spikes near them, ranged take far ones | Direct targeting, below |
| Bone Storm 69076, hit 69075 / 70834 / 70835 / 70836 | 3 s cast at 45-50 s, then every 90-95 s. 20 s (10) or 30 s (25). Every 2 s 6k / 6k / 12k / 14k to all, divided by √(d − 9) past 10 yd. Drops his target (so tanks count), then every 5 s charges at 25 yd/s at the farthest player (25: one of the two farthest) under x −337 and drops 72705: four heads 2.5 yd out on the world diagonals. No swings, Bone Slice or plain Coldflame meanwhile. After: back to top threat. | Raid stacked mid-room, tanks and a hunter or warlock in a triangle 30-40 yd out; whoever he reaches steps off the cross | Storm stack and three anchors, below |
| Heroic | Bone Spike Graveyard goes on through storms, as an instant | Spikes go on through the storm | Same targeting |
| Berserk 47008 | 10 min | | Gap: no pacing |

**Room.** He evades if he leaves a circle of r 95 round (−428, 2211) inside x −430..−330, checked every
2.5 s; the whole floor is inside it. He attacks and charges nobody past x −337, the gate.

## Layout

Bots hold one latched stand each (park 1 yd, back past 2.5): `lm.stand`.

- **Main tank** at `ICC_LM_TANK_SPOT`, 14 yd from his spawn `ICC_LM_BOSS_SPOT` away from the gate,
  which keeps him on his spawn. Settled more than 8 yd off it after a storm, it first walks to a pull
  point 18 yd past the spot, so he follows back onto it; at most three pulls per normal phase. Taken
  off it, it holds no stand and the class "lose aggro" taunt brings him back.
- **Off-tanks** 2 yd beside the main tank across his bearing; all tanks stack, the 10-man spare included.
- **Inside**: melee, healers and ranged that don't bait, on a 2 yd ring round his settled centre. A line
  starts 10.1 yd out with 4.5 yd reach, so nothing reaches 5.6 yd from him (Wowhead puts non-baits in
  melee for the spikes).
- **Baits**: 3 (10) or 8 (25), every hunter first (he is inside their dead zone otherwise), then
  warlock, priest, druid, shaman, mage; a healer only with no ranged dps. 31 yd from him, 20° apart
  from the bearing away from the tank spot. One bait outside is all the core needs; the count follows
  the guides.
- **Bone Storm**, from the cast: three anchors at `ICC_LM_STORM_ANCHOR_0/120/240`, 34 yd round
  `ICC_LM_STORM_STACK`, and everyone else on a 1.5 yd ring there. Anchors are the main tank (120), the
  first off-tank (240) and the first bait (0), baits filling a missing tank. From any anchor the other
  two are farthest, so his paths run 17 yd off the stack, and his cross misses it by 8.8 yd from the
  side anchors. 34 keeps the anchors in a healer's 38.5 yd from the stack. Nobody chases him: the storm
  hold blocks reach melee and spell.

Stands round him use his settled position (still 500 ms) and hold while he stays within 3 yd of where
they latched; otherwise reach follows him. A bot on a spike holds no stand, an anchor excepted.

Navprobed `--nav 0x09` (settled Z 41.4-42.4): every named spot, the stack ring, the bait ring and inner
slots for him up to 6 yd off his spot, and the pull points from each anchor. Derived spots also run
`ValidateFloorPoint` when they latch. Line of sight is unchecked: this navprobe build has no `los`.

## Targeting

`GetLmTarget`, DPS bots only, Impaled ones excluded: a player's skull on him or a spike first. With a
spike up, melee take the nearest within 10 yd unless it is within 8 yd of the main tank; ranged split
by guid over the spikes no melee reaches, else over all. Otherwise him. Spikes come from Impaled
members' vehicle, no sweep. `icc lm targeting` holds the generic pickers while it picks.

## Coldflame

`GetLmColdflame`, cached per instance for 100 ms and swept from him: every live patch (remapped id),
the next three patches of each live head (dead once still 700 ms past its first wait, or after 12.5
s), and while he charges in a storm the two nearest patches of each line of the cross at his spline's
end. The dodge fires within 5 yd of one, at `ACTION_EMERGENCY + 6` with `MOVEMENT_FORCED`, interrupts a
pinning cast and walks to the nearest spot 7 yd clear (5.5 if none), near its stand, below x −340. A
stand never walks into or across one. The anchor a charge aims at leaves before he lands. `avoid aoe`
is blocked while live.

## Cheats

None.

## Known gaps

- No trace yet: every number above is from the core and the guides.
- No Berserk pacing, raid cover for Bone Storm or tank defensive on his arrival.
- The opener hold needs Marrowgar's `HookRows()` row: `GetLmOpenerBosses` is ready.
- A caster reaching for a spiked anchor in a heroic storm walks into his lanes.
- Humans hold no duty and count toward nothing: a human far out draws his charges.

## Targets for the next pull

25N: no Bone Slice on a non-tank; no Coldflame tick inside him; baits dodge (`--dodges` mostly `move`,
few `tight` or `none`); every storm's anchors on their stands (`--storms`) and charges ending on them;
spikes dying before the second tick. 25H: the same with spikes through storms.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `lm.phase` | per bot | `normal`, `stormcast`, `bonestorm` |
| `lm.duty` | roster | 1 main tank, 2 off-tank, 3 inside, 4 bait, 0 lost |
| `lm.anchor` | roster | 1-3 for the anchor at 0, 120, 240 |
| `lm.stand` | per bot | `tank`, `pull`, `offtank`, `inner`, `bait`, `anchor`, `stack`, `none` |
| `lm.spike` | per bot | `skull`, `melee`, `far`, `spread`, `boss` |
| `lm.dodge` | per bot | `move N`, `tight`, `hold`, `none`, `locked`, `stunned`, `clear` |
| haz 72705 | circle | the cross a storm charge is about to drop |

`tools/botobs/bosses/lord_marrowgar.py`: `--phases`, `--duties`, `--storms`, `--spikes`, `--dodges`,
`--cheats`.
