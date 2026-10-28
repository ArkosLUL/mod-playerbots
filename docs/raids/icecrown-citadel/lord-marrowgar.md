# Lord Marrowgar

Boss id 0, slug `lord-marrowgar`, prefix `lm.`. Code: stem `Marrowgar` (`src/Ai/Raid/ICC/*/ICC*_Marrowgar.*`),
`Definition/ICCDefinition_Marrowgar.cpp`. Raid conventions: [README.md](README.md). Guides: Warcraft
Tavern 25 and 10, Wowhead, instant-carry.

## Mechanics

Core: `boss_lord_marrowgar.cpp`. Ids are 10N / 25N / 10H / 25H; one id means no difficulty row (CSV
and world DB checked). His combat reach is 20.25. Timers run from the engage.

| Ability | Core | Guide | Bots |
|---|---|---|---|
| Bone Slice 69055 / 70814 / 69055 / 70814 | Every swing from 10 s in and 10 s after each storm. 5 yd round his victim (SpellInfoCorrections, not the guides' 8), at most 2 (10) or 3 (25) random targets, damage split. Hit players can't be spiked. | 2-3 tanks stacked, everyone else 8 yd off the main tank | Off-tanks 2 yd beside the main tank, everyone else 10+ yd off; every tank spends a defensive as it resumes ([Defensives](#defensives)) |
| Coldflame 69140, patch 69146 / 70823 / 70824 / 70825 | Every 5 s outside storms, at a random player over 22.75 yd (3D) from his centre who is not his last victim or Impaled, else at anyone. A head (36672) appears 10.1 yd out on that bearing, lays a patch where it stands every 450 ms and steps 5 yd, for 12 s or until a wall. Patch: 3 yd (SpellInfoCorrections) plus the player's 1.5, 3 s (heroic 8 s), 1 s ticks of 6k / 8k / 9k / 11k. | 8 (25) or 3 (10) out of melee, melee inside his hitbox | Baits on a ring 31 yd out, everyone else within 3 yd of his centre, where no line reaches; a dodge |
| Bone Spike Graveyard 69057 / 70826 / 72088 / 72089 | 3 s cast at 10-15 s, then every 15-20 s; normal skips storms and re-arms 15-20 s after. 1 (10) or 3 (25) spikes (36619, 38712, 38711) on random players within 150 yd but his victim and the last Bone Slice targets. Impaled 69065: 10% max HP a second, physical. | Melee cleave spikes near them, ranged take far ones; Hand of Protection stops Impaled's ticks | Direct targeting, below. No Hand of Protection: Impaled carries `SPELL_ATTR0_NO_IMMUNITIES`, so the physical immunity doesn't stop its ticks on this core |
| Bone Storm 69076, hit 69075 / 70834 / 70835 / 70836 | 3 s cast at 45-50 s, then every 90-95 s. 20 s (10) or 30 s (25). Every 2 s 6k / 6k / 12k / 14k to all, divided by √(d − 9) past 10 yd. Drops his target (so tanks count), then every 5 s charges at 25 yd/s at the farthest player (25: one of the two farthest) under x −337 and drops 72705: four heads 2.5 yd out on the world diagonals. No swings, Bone Slice or plain Coldflame meanwhile. After: back to top threat. | Raid stacked mid-room, tanks and a hunter or warlock in a triangle 30-40 yd out; whoever he reaches steps off the cross; rotate defensive cooldowns, Divine Guardian early | Storm stack and three anchors, below; raid cover per 6 s of ticks ([Defensives](#defensives)) |
| Heroic | Bone Spike Graveyard goes on through storms, as an instant | Spikes go on through the storm | Same targeting; in a storm a bot takes only a spike it reaches from its stand |
| Berserk 47008 | 10 min | Very lenient | No pacing; `--phases` prints the margin |

**Room.** He evades if he leaves a circle of r 95 round (−428, 2211) inside x −430..−330, checked every
2.5 s; the whole floor is inside it. He attacks and charges nobody past x −337, the gate.

## Layout

Bots hold one latched stand each (park 1 yd, back past 2.5): `lm.stand`.

- **Main tank** at `ICC_LM_TANK_SPOT`, 14 yd from his spawn `ICC_LM_BOSS_SPOT` away from the gate,
  which keeps him on his spawn. Settled more than 8 yd off it after a storm, it first walks to a pull
  point 18 yd past the spot, so he follows back onto it; at most three pulls per normal phase. Taken
  off it, it holds no stand and the class "lose aggro" taunt brings him back.
- **Off-tanks** 2 yd beside the main tank across his bearing; all tanks stack, the 10-man spare included.
- **Inside**: melee, healers and ranged that don't bait, 2 yd from his settled centre on a 160° arc
  round his back (the bearing from the tank spot to him), out of his parry arc and far from Bone
  Slice. A line starts 10.1 yd out with 4.5 yd reach, so nothing reaches 5.6 yd from him (Wowhead puts
  non-baits in melee for the spikes).
- **Baits**: 3 (10) or 8 (25), every hunter first (he is inside their dead zone otherwise), then
  warlock, priest, druid, shaman, mage; a healer only with no ranged dps. 31 yd from him, 20° apart
  from the bearing away from the tank spot, at most 8 slots (80° either side): a ninth falls behind
  the walls once he settles 6 yd to the side, so a ninth hunter stands inside. One bait outside is all
  the core needs; the count follows the guides.
- **Bone Storm**, from the cast: three anchors at `ICC_LM_STORM_ANCHOR_180/60/300`, 34 yd round
  `ICC_LM_STORM_STACK`, and everyone else on a 1.5 yd ring there. Every tank takes 180, 2.6 yd from
  the tank spot; 60 and 300 (and 180 without a bot tank) the bait with the nearest ring slot, 14 yd
  away in 25, 19 in 10. His first charge, 3.05 s into the cast, takes the farthest (25: one of two):
  from his spot, 60 or 300 (45 yd), so all three are held by then. Not 0/120/240 with tanks on
  120/240: their 32.6 yd walk (4.7 s) lets a 25-man first charge land on one mid-walk, or on the
  stack, and cross the stack. From any anchor the other two are farthest, so his paths run 17 yd off
  the stack, and his cross misses it by 8.8 yd from 60 and 300, 24 from 180. 34 keeps the anchors in
  a healer's 38.5 yd from the stack. Nobody chases him: the storm hold blocks reach melee and spell.

Stands round him use his settled position (still 500 ms) and hold while he stays within 3 yd of where
they latched; otherwise reach follows him. A bot on a spike holds no stand, an anchor excepted, nor
does anyone while he evades home after a wipe.

Navprobed `--nav 0x09` (settled Z 41.4-42.4): every named spot, the stack ring, the bait ring and inner
slots for him up to 6 yd off his spot (the bait ring again to 8 yd, below), 10 yd round each anchor, and
the pull points from each anchor.
The floor ends near x −427, inside his boundary. Derived spots also run `ValidateFloorPoint` when they
latch.

**Line of sight**, navprobe `los` with him on his spot and 3, 6 and 8 yd off it in 16 directions: clear
for tank spot to him, every inner slot (1 to 20 bots) to him and the tank spot, every bait slot to him,
the tank spot and the inner arc, the stack ring and stack centre to each anchor, and each anchor to the
tank spot and the other anchors. Blocked, the raw slot off the floor behind a wall (settled Z −140):
bait slots 8 and 9 with him 6 yd to the side (now removed), slots 6 and 7 with him 8 yd to the
north-west or south-west, which is where the re-centre starts. Gameobject collision isn't modelled.

## Targeting

`GetLmTarget`, DPS bots only, Impaled ones excluded: a player's skull on him or a spike first. With a
spike up, melee take the nearest within 10 yd unless it is within 8 yd of the main tank; ranged split
by guid over the spikes no melee reaches, else over all. Otherwise him. In a storm (cast included) only
spikes within 10 yd (melee) or 28 yd (ranged) of the bot count: a heroic spike on an anchor is 32+ yd
from the stack and nobody walks his lanes for it (it waits for healers or the storm's end). Spikes come
from Impaled members' vehicle, no sweep. `icc lm targeting` holds the generic pickers while it picks. Once per
instance a bot's saved rti of cross or star goes back to skull for the icon-led fights after him.

## Coldflame

`GetLmColdflame`, cached per instance for 100 ms and swept from him: every live patch (remapped id),
the next three patches of each live head (dead once still 700 ms past its first wait, or after 12.5
s), and while he charges in a storm the two nearest patches of each line of the cross at his spline's
end. The dodge fires within 5 yd of one, at `ACTION_EMERGENCY + 6` with `MOVEMENT_FORCED`, interrupts a
pinning cast and walks to the nearest spot 7 yd clear (5.5 if none), near its stand, below x −340,
on a walk that crosses no other patch within 5 yd (`cross` in `lm.dodge`: taken only when no such walk
exists, over standing in a line). A stand never walks into or across one. The anchor a charge aims at leaves before he lands.
`avoid aoe` is blocked while live.

## Defensives

His only predictable tank spike is Bone Slice resuming: plain swings for 10 s after the engage and after
each storm (`EVENT_ENABLE_BONE_SLICE`), then 300% (400% on 25) split over at most 2 (3) players round the
victim on every swing. Arrival after a storm is plain swings, not the spike.

- **Tanks**: the model is the core's timers, so no cast is read. A window opens 10 s into every
  `normal` phase. The main tank and each off-tank spend `NextTankDefensive` (physical only, so no
  Anti-Magic Shell) once per window, from 1 s before to 6 s after, and again under 40% while it runs.
  Each takes the weakest ready button, so the cooldowns rotate over the storms. The class buttons
  (`HeldTankDefensiveNames()`) stay held over these casts until the tank is under 25%, when they stack on
  the first. A human tank in the victim's place gets `CallHumanTankDefensive` at the window and under 40%.
- **Raid**: Bone Storm is physical and unreduced by armor, 6k / 6k / 12k / 14k a tick (10N / 25N / 10H /
  25H) divided by √(d − 9) past 9 yd. `GetRaidCover` gets each 6 s of ticks (three) as a hit: the tick
  at 28 yd, the raid's mean distance as he ping-pongs between anchors, from the aura's remaining time.
  It deals a cooldown once three members would end under 25% health, one holder per cooldown: Divine
  Sacrifice (Divine Guardian), Divine Hymn, Tranquility. Aura Mastery needs a resistance aura of the hit's
  school, so physical never gets it. The hit is offered 3 s before its first tick and the class cover
  nodes wait over that lead; anchors don't channel. Anti-Magic Zone is dealt out of the deal: it absorbs
  spells only, and sits in the shared list. Personal buttons aren't wired: the class nodes act on low
  health, and a button timed to one tick of a 30 s storm buys little.

## Cheats

None.

## Known gaps

- No trace yet: every number above is from the core and the guides.
- Humans hold no duty and count toward nothing: a human far out draws his charges.
- Bait slots 6 and 7 can sit off the floor with him 8 yd to the north-west or south-west.
- No bot tank has held a Bone Slice window live: `--defensives` says whether the button lands in time.
- The cover's 28 yd damage is a mean, not each member's own distance; the reader's `--covers` and the
  deaths in a storm say whether it deals too late.

## Targets for the next pull

25N: no Bone Slice on a non-tank; no Coldflame tick inside him; baits dodge (`--dodges` mostly `move`,
few `tight`, `cross` or `none`); every storm's anchors on their stands (`--storms`), charges ending on
them and the first on 60 or 300; spikes dying before the second tick; a tank button in every Bone Slice
window (`--defensives`). 25H: the same with spikes through storms, and a cover dealt and cast in a storm
whose next three ticks would take three raiders under 25% (`--covers`).

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `lm.phase` | per bot | `normal`, `stormcast`, `bonestorm` |
| `lm.duty` | roster | 1 main tank, 2 off-tank, 3 inside, 4 bait, 0 lost |
| `lm.anchor` | roster | 1 at 180 (every tank), 2 at 60, 3 at 300 |
| `lm.stand` | per bot | `tank`, `pull`, `offtank`, `inner`, `bait`, `anchor`, `stack`, `none` |
| `lm.spike` | per bot | `skull`, `melee`, `far`, `spread`, `boss` |
| `lm.dodge` | per bot | `move N`, `tight`, `cross`, `hold`, `none`, `locked`, `stunned`, `clear` |
| `lm.defensive` | per tank bot | the button offered (cast name), `covered` once one runs, `none` with nothing ready, `idle` outside a window |
| `lm.humancall` | per call | the button a human tank was told to press |
| `icc.cover` | roster | the raid cooldown dealt to each holder: 1 Aura Mastery, 2 Divine Sacrifice, 3 Anti-Magic Zone (never cast), 4 Divine Hymn, 5 Tranquility |
| haz 72705 | circle | the cross a storm charge is about to drop |

`tools/botobs/bosses/lord_marrowgar.py`: `--phases` (ends with the Berserk margin), `--duties`,
`--storms`, `--spikes`, `--dodges`, `--defensives` (a window opens 10 s into every `normal` phase; the
casts are the `cast` rows), `--covers`, `--cheats`.
