# Rotface

Boss id 5, trace slug `rotface`, prefix `rf.` ([README](README.md)). Code: `Action/ICCActions_RF.cpp`
and the Rotface regions of the shared ICC files. Mechanics: `boss_rotface.cpp`.

## Mechanics

- **Mutated Infection** row 1991 (69674/71224/73022/73023) on one player every 14 s; 25-man only, the
  interval drops 2 s every 90 s to a floor of 8 s. Its removal summons a Little Ooze 36897 that seeds
  500,000 threat on the player.
- **Oozes**: two Little Oozes merge into a Big Ooze 36899 (25-man 38123); a Big Ooze absorbing another
  ooze gains Unstable Ooze stacks and at 5 casts Unstable Ooze Explosion 69839, a 4 s cast.
- **A Big Ooze is tankable.** At its first update it resets its threat list and attacks the nearest
  player; after that only threat decides, and nothing seeds it again. Its immunity set -361 blocks
  snares, stuns and roots but neither the taunt aura (11) nor attack-me (114), so a class taunt lands,
  where the Little Ooze's -362 blocks both. `flags_extra` 0x80000 puts taunts on DR, 100/65/42/27%
  duration, then immune until 15 s after the last one fades
  ([raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
  Run speed 0.57, about 4 yd/s.
- **Slime Spray** 69508, a 1.5 s cast at a random non-tank every 20 s; **Ooze Flood** from a quarter
  of the room every 25 s, first at 8 s.
- **Vile Gas, heroic**: Putricide casts 69240 (73019/73020) from the balcony at a random ranged player
  every 15-20 s, triggered, so no line of sight; 6 s.
- His state reaches `IN_PROGRESS` only at Putricide's balcony point, so the trace opens on the engage.

No phase.

## Tactics

- The main tank drags him to `ICC_ROTFACE_CENTER_POSITION_BOSS`; melee stay on him. Every tank and
  melee keeps the skull on him.
- **Big Oozes**: the assist tank taunts each one not on it and walks to the nearest such, then kites
  the nearest on a 24-34 yd ring around the centre, 13 yd ahead of the ooze and 30 yd from any flood.
  The main tank takes this over while no assist tank lives and a Big Ooze is not on one.
- A bot with a Little Ooze, or 4 s or less of Mutated Infection, walks to the assist tank kiting a Big
  Ooze so the oozes merge there, else to the main tank. Mutated Infection is never dispelled.
- **Explosion**: non-tanks run to one of 25 slots on a 40 yd ring round the centre, at most two bots a
  slot (`rfExplosionSlotMemory`, kept between explosions while still safe), hold 2 s, and the trigger
  stays on 6 s after the cast.
- **Vile Gas**: the victim, and anyone carrying the aura, run 40 yd out from the centre on the first
  bearing clear of the Big Ooze (20 yd) and floods (30 yd), in 15 degree steps off its own, then hold
  4 s. A window idle for 2 s drops its spot, so the next cast picks one against the room as it is.
- Heroic ranged and healers hold a 22-spot grid; normal ranged stand 3.5 yd off him and on him during
  Slime Spray.

## Cheats

Removed:

- `rf.nitro` (5 sites): the bot's own Nitro Boosts through `UseNitroBoosts`, its 3 min cooldown
  included; without boots it walks.
- `rf.spitefulfury` (3): the Big Ooze pickup is the plain class taunt, which lands per the immunity
  set above, with nothing resetting the ooze's threat afterwards.

None kept.

## Known gaps

- **Nothing holds a Big Ooze but the taunt.** The assist tank never attacks one, so it keeps an ooze
  only by the threat the taunt matched. Heal threat reaches every Big Ooze, since zone combat puts the
  whole raid on its list, and can pull one onto a healer; repeated taunts run into DR. `--oozes` gives
  the share of Big Ooze time on a tank.
- Skull by raid icon (README).
- `IccRotfaceTankPositionAction::FindAssignedBigOoze` has no caller.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `rf.bigooze` | per tank | `pickup <ooze>` walking to one not on it, `kite <ooze>` once all are, `none` with none up or for the main tank while the assist tank lives |
| `rf.explosionslot` | per bot | Its escape slot, 0-24, kept between explosions; an erase writes `0`, a real slot too |
| `rf.vilegas` | event | Heroic: the victim of each cast, `txt` the spell id |
| `rf.vilegasspot` | per bot | Heroic: `offset N` steps of 15 degrees it turned, `blocked` with no clear bearing, `none` when its window closes |
| `rf.skull` | per bot | `boss <guid>`, only from the bot that moved the icon; a repeat of its own last placement writes nothing |

`tools/botobs/bosses/rotface.py`; its banner names declared keys the pull never wrote, sparing the
Vile Gas pair on normal:

- `--oozes`: the Big Oozes' time on target by role, and each tank's `rf.bigooze` runs;
- `--explosion`: the explosion casts and each bot's slots;
- `--vilegas`: per cast, the victim and the spot it took, and who ran from the splash;
- `--skull`: each move of the skull;
- `--cheats`: this encounter's keys plus the raid-wide `icc.` ones.
