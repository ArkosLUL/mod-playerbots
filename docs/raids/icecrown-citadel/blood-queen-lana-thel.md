# Blood-Queen Lana'thel

Boss id 8, trace slug `blood-queen-lana-thel`, prefix `bql.` ([README](README.md)). Code:
`Action/ICCActions_BQL.cpp`, the `icc bql` nodes, `IccBqlMultiplier`. Mechanics:
`boss_blood_queen_lana_thel.cpp`.

## Mechanics

- Entry 37955 (difficulty entries 38434-38436). `IN_PROGRESS` at her engage, which evades unless the
  council is `DONE`. Berserk at 330 s. Immunity set -361 lets taunts land; `flags_extra` 0x80000 puts
  them on DR, and 0x200 (`NO_MOVE_FLAGS_UPDATE`) leaves her movement flags to her script alone.
- **Blood Mirror** every 2.5 s links her victim to the nearest other player within 39 yd; **Delirious
  Slash** (row 2091) at 10-12 s, then every 20-24 s, on that player within 10 yd, else her victim.
- **Vampiric Bite** (row 2093) at 15 s on the highest-threat player who is neither her victim, the
  Blood Mirror player, nor a vampire: it gains Essence of the Blood Queen (row 2087), whose 75 s tick
  sets off **Frenzied Bloodthirst** (row 2083: 70877 10-man, 71474 25-man, the same ids on heroic,
  15 s and 10 s). Biting a non-vampire clears it and renews the Essence; letting it expire brings
  Uncontrollable Frenzy 70923, a mind control.
- **Pact of the Darkfallen** 71340 at 20 s, then every 30 s, on 2 players (25-man 3), never her victim
  or the Blood Mirror player; it ends once every member stands within 5 yd of the others.
- **Swarming Shadows** at 30 s, then every 30 s, on one random player outside the victim, the Blood
  Mirror player, Pact and Frenzy: 71264 for 2 s, then 71265 dropping a Swarming Shadows 38163 every
  0.5 s for 6 s. Both carry the name, so a name test covers the warning and the trail.
- **Twilight Bloodbolt** at 15-25 s, then every 10-15 s, on 2 players (25-man 4).
- **Air phase** at 124 s (25-man 127 s): she turns passive and walks to the room's centre, casts
  Incite Terror 73070 there, and 2.5 s later takes off with gravity off to 22 yd above it, casts
  Bloodbolt Whirl 71772 at the top and lands 7 s later. The next comes 120 s (25-man 100 s) after she
  reaches the centre. Pact returns 5 s after landing, Swarming Shadows 20 s after.

## Phase

`IccBqlPhaseOf` publishes `bql.phase` from her own state: `center` while passive with gravity on (the
walk and Incite Terror), `air` from takeoff until she lands, `ground` otherwise. Every phase test of
the group action and the multiplier goes through it. It replaced a height test plus three latches,
which saw the walk only once she was within 20 yd of the centre, never before she had once stood at
the tank spot, and read her as walking whenever she was tanked off that spot near the centre.

Her triggers also bail while a prince is found. The princes die together and stay dead once the
council is `DONE`, so that never fires in her fight; the council room's intro Blood-Queen 38004
shares her name but never enters combat, so `"find target"` never resolves it.

## Tactics

- The main tank holds her at `ICC_BQL_TANK_POSITION`; the assist tank stands on the main tank, and
  holds still while it carries Blood Mirror with her above 90%.
- **Ground**: melee on her; ranged and healers take one of 19 slots on three arcs (20, 27, 35 yd)
  from the tank spot toward the centre (`bql.slot`), each slot 7 yd clear of any shadow, else follow
  the lowest-guid melee bot until one frees up.
- **Center**: everyone walks to the centre. **Air**: tanks stack on the lowest-guid tank at the
  centre; every other bot takes one of 73 slots on rings of 10, 18, 26 and 34 yd round it, hunters
  from the outside in (`bql.airslot`), and pops a defensive on Bloodbolt Whirl. Reach, flee and avoid
  are vetoed in the air.
- **Swarming Shadows**: the victim, tanks included, walks one of three lanes along the nearer wall,
  the outermost preferred, in steps of up to 7 yd that keep 4 yd from every shadow, and runs to the
  centre when all three are blocked and it stands in a shadow. The lane (`bql.curve`) is dropped
  when the trail ends, so the next trail picks its own.
- **Frenzied Bloodthirst**: biters in guid order each claim the nearest player who is no vampire,
  carries no Swarming Shadows and is a dps or healer, tanks only when none is left. A biter cancels
  Divine Shield, Ice Block, Cloak of Shadows and Deterrence, walks in, and bites within 4 yd.
- **Pact**: members meet on a tank carrying it, else at their centre, else on the one other member.
- A biter, a Pact member and a Swarming Shadows victim run only that action.

## Cheats

Removed: `bql.swarmingshadows`, a tank stripping Swarming Shadows. The core never puts it on her
victim or the Blood Mirror player, and a tank that gets it now walks the trail the way every other
bot does: the multiplier already held it to the group action, which falls through to the lane walk.

## Known gaps

- `bql.curve`, `bql.slot` and `bql.airslot` write `0` for an erase as well as for lane or slot 0.
- No ICC trace yet carries these probes, so no figure here is measured.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `bql.phase` | per bot | `ground`, `center`, `air` |
| `bql.bite` | per biter | `dps` (dps or healer) or `tank` and the pick, `nobody`, `none` outside Frenzied Bloodthirst; re-derived each tick, so a switch is the nearest target changing |
| `bql.pact` | per Pact member | `tank` and the tank walked to, `hold` for that tank, `center n`, `partner` and the other member; `none` outside it |
| `bql.curve` | per shadowed bot | The wall lane, 0 the outermost; erased when the trail ends |
| `bql.slot`, `bql.airslot` | per bot | Ground and air slot indices |

`tools/botobs/bosses/blood_queen_lana_thel.py`: `--phases`, `--bites` (per Frenzied Bloodthirst: the
picks and `bit`, `frenzy`, `died` or `open`), `--pact`, `--shadows` (per trail, the lanes walked),
`--slots` (first slot and moves per bot), `--cheats`.
