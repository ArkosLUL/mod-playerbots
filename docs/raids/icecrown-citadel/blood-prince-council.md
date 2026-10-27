# Blood Prince Council

Boss id 7, trace slug `blood-prince-council`, prefix `bpc.` ([README](README.md)). Code: stem
`BloodPrinces` (`src/Ai/Raid/ICC/*/ICC*_BloodPrinces.*`), the `icc bpc` nodes,
`IccBpcAssistMultiplier`. Mechanics: `boss_blood_prince_council.cpp`.

## Mechanics

- **Three princes, one pool.** Valanar 37970, Keleseth 37972, Taldaram 37973; any one engaging pulls
  the others, and Valanar's engage sets `IN_PROGRESS`. Only the prince holding **Invocation of
  Blood** (70952 / 70981 / 70982, no difficulty rows) takes damage; the others turn damage into
  threat. It starts on Valanar, moves at 45 s and every 46 s after to a random one of the other two,
  then the last, and the next prince takes over the current one's health. One prince dying kills the
  other two; Valanar's death is `DONE`. Berserk at 10 min; a prince leaving the room's ellipse
  evades all three.
- **Taunts**: immunity set -361 blocks neither taunt nor attack-me, but `flags_extra` 0x80000 puts
  taunts on DR, 100/65/42/27% duration, then immune until 15 s after the last one fades
  ([raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
- **Keleseth**: Shadow Lance (rows 2237, empowered 2238); Shadow Resonance 71943 every 10-15 s
  summons a Dark Nucleus 38369, immune to AoE, that puts Shadow Resonance 71822 on its victim within
  15 yd. Any direct hit makes the hitter its only threat (500M), so a nucleus leaves the tank the
  moment a dps hits it; taunts land and are not on DR.
- **Taldaram**: Glittering Sparks on his victim; Conjure Flame at 20 s, then every 20-25 s
  (empowered: every 15 s, an Inferno Flame). The Ball of Flame 38332 or Ball of Inferno Flame 38451
  chases a random player more than 10 yd away and explodes on contact (Flames 71393, 15 yd); the
  Inferno Flame carries 20 stacks of Ball of Flames Proc 71756, dropping one per proc.
- **Valanar**: Kinetic Bomb 72053 at 18-24 s, then every 20.5 s (25-man) or 30.5 s: a Kinetic Bomb
  38454 appears 20 yd above a random spot and falls slowly, a hit knocks it back up, and it explodes
  on landing (72052, damage and knockback within 50 yd). Shock Vortex 72037 at 15-20 s, then every
  18-23 s: a vortex 38422 at a random player that knocks back within 12 yd from 4.5 s on. Empowered,
  every 30 s instead: **Empowered Shock Vortex** 72039 (row 2261), a 4.5 s cast after which every
  player within 30 yd sets off 72038, 12 yd round themselves.
- **Heroic**: Shadow Prison 72999 stacks on a player each tick it moves, and each stack adds damage.

## Phase

`IccBpcEmpoweredPrince` reads the three Invocation ids and publishes `bpc.phase`. Every empowerment
test goes through it: the vortex and ball triggers and the skull rule.

## Tactics

- The main tank holds Valanar and Taldaram at `ICC_BPC_MT_POSITION`, taunting either one not on it;
  the assist tank holds Keleseth and, once holding him, taunts the first Dark Nucleus not on it within
  30 yd, walking toward one further out. Each taunt waits out the 8 s cooldown, so stray nuclei come
  back one per cooldown, and a prince taunted back every cooldown goes taunt-immune after the fourth.
- Every tank keeps the skull on the empowered prince.
- **Kinetic Bombs**: `IccBpcAssignedKineticBomb` gives each bomb no more than 35 yd above the floor,
  lowest first, to the nearest unassigned ranged dps bot within 80 yd: hunters, then druids, then
  anyone. Trigger, action and multiplier all read it; an assigned bot shoots its bomb and switches to
  nothing else.
- **Valanar empowered**: ranged and healers hold one slot each on a 25 yd ring round him; during
  Empowered Shock Vortex every non-tank takes a slot on a ring spaced 13 yd (radius 15-38 yd), holding
  it 250 ms on arrival. Either slot turns in 10° steps off any Shock Vortex within 13 yd.
- **Taldaram empowered**: non-tank non-hunters step into an Inferno Flame within 2-10 yd of him to
  soak it. A ball's victim keeps 15 yd from everyone, whoever is empowered: the multiplier leaves it
  nothing else, so a trigger tied to Taldaram froze it under a plain ball.
- **Heroic**: above 12 Shadow Prison stacks (tanks 18) only attacks and reaches still move a bot; an
  assigned bomb shooter keeps its bomb up to 18.
- AoE abilities are vetoed for the whole fight.

## Cheats

None. Borderline, not wrapped: the assist tank more than 6 yd off the floor and a bomb shooter above
Z 371.2 snap back to the floor through `NearTeleportTo`, which keeps combat where the same-map
`TeleportTo` before it dropped it ([pitfalls.md](../../engine/pitfalls.md#before-the-pull-and-out-of-combat)).

## Known gaps

- Skull by raid icon (README).
- The Keleseth tank action's branch pulling a non-assist-tank toward the centre never runs: its
  trigger fires for the assist tank only.
- No ICC trace yet carries these probes, so no figure here is measured.

Not defects: the bomb list's 38775-38777 are 38454's difficulty entries, which `GetEntry` never
returns ([pitfalls.md](../../engine/pitfalls.md#detection-that-never-fires)).

## Blood Prince trash

Boss id 14, slug `blood-prince-trash`, prefix `bpc.`. The state goes `NOT_STARTED` then `DONE` once
the four spawns guarding the Crimson Hall door (201479, 201482, 201646, 201659) die, never
`IN_PROGRESS`, and none of the trash is boss-flagged. So a `UnitScript` in `Util/ICCScripts.cpp`
opens the trace with `RaidObs::MarkPull(map, creature, "blood-prince-trash")` when any spawn of the
Darkfallen Advisor 37571, Archmage 37664, Blood Knight 37595 or Noble 37663 engages a player (18
spawns on 631). The pull that kills the last door guard closes on the `DONE`, as `kill`; any other
closes `idle`. A pet's pull is traced only once one of them engages a player: each fires the hook
once, at `Creature::AtEngage`, on its first target. The strategy has no trash code.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `bpc.phase` | per bot | `valanar`, `keleseth`, `taldaram`, `none` in the gap of a transfer |
| `bpc.bomb` | per ranged dps bot | `hunter`, `druid` or `any` and the bomb; `spare` with bombs down and none for it; `none` with no bomb low enough |
| `bpc.spread` | per bot | `vortex i/n` or `ranged i/n`, plus ` nudged` (turned off a Shock Vortex) or ` blocked` (no clear angle); `none` for melee outside the cast and once Valanar loses the Invocation |
| `bpc.skull` | per tank | `empowered` and the prince, plus `from` and the unit it came off; only the tank that moved it writes, and the very same move again later writes nothing |

`tools/botobs/bosses/blood_prince_council.py`: `--phases`, `--bombs` (per bot: assignments by tier,
bombs given, bomb-to-bomb switches, spare spans), `--spread` (per bot and ring: slots, nudged,
blocked), `--skull`, `--cheats`. `blood_prince_trash.py` reads a trash file: banner and cheats.
