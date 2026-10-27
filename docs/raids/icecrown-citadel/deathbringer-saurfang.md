# Deathbringer Saurfang

Boss id 3, slug `deathbringer-saurfang`, prefix `dbs.`. Code: stem `Saurfang`
(`src/Ai/Raid/ICC/*/ICC*_Saurfang.*`), node `icc dbs` → `icc dbs tank position` (`ACTION_RAID + 3`),
`icc adds dbs` (`+ 5`); `IccAddsDbsMultiplier`. Raid conventions: [README.md](README.md).

## Mechanics (core)

- **Rune of Blood** 72410 on his victim, first at 20 s, then every 20-25 s.
- **Blood Beasts** 38508, first at 30 s, then every 40 s: 2 in 10-man, 5 in 25-man; on heroic they gain
  Scent of Blood 10 s after each wave.
- **Mark of the Fallen Champion** 72293 on players as his Blood Power fills.

No phase: `dbs.phase` is not published, and the runes and marks are player auras the trace carries.

## Duties

- **Tanks**: the one with Rune of Blood stops attacking him, and the multiplier zeroes all but its
  movement, the class taunt actions included; the other taunts him off it. Both taunt any Blood Beast
  not on a tank through the shared helper, which no multiplier sees.
- **Ranged and healers** hold a 3 × 5 grid behind the tank spot, one slot each (`dbs.slot`), and step
  away from a beast that targets them; a 16th gives its slot up and stacks on the nearest melee.
- **Melee** keep skull on the first live Blood Beast, else on him (`IccAddsDbsAction`).

The multiplier zeroes AoE, Army of the Dead, formation, follow and flee, and a ranged bot's reach. It
tests the action before resolving him, since it is asked about every action.

## Cheats

None. Two taunt callers ([README.md](README.md#cheats)):

- The beast taunt reaches one beast per cooldown, so a 25-man wave of five sits on the raid while the
  tanks work through them, about 8 s a beast each, unless CC or the tanks' threat takes them first.
- The boss swap waits out the off tank's cooldown when a beast took the last taunt; the rune tank has
  stopped attacking, so he goes to the next on threat meanwhile.

## Known gaps

- Raid-icon targeting (README Known gaps).
- A `dbs.slot` of `0` is both slot 0 and an erase (the 16th ranged), so a reader takes each bot's
  first slot.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `dbs.slot` | per bot | The grid slot, 0-14 |
| `dbs.skull` | per melee | `beast`, `boss`, `none` (he is dead) |

`tools/botobs/bosses/deathbringer_saurfang.py`: `--slots`, `--skull`, `--runes` (each Rune of Blood's
tank, every Mark), `--cheats`.
