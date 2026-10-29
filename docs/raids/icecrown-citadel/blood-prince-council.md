# Blood Prince Council

Boss id 7, slug `blood-prince-council`, prefix `bpc.` ([README](README.md)). Code: stem `BloodPrinces`
(`src/Ai/Raid/ICC/*/ICC*_BloodPrinces.*`); nodes `icc bpc` dodge, tank defensive, ball, prince taunt,
nucleus, soak, target, stand; multiplier `icc bpc splash`. Core: `boss_blood_prince_council.cpp`.

## Mechanics

Ids and damage are 10N/25N/10H/25H where they differ; code remaps a difficulty row through
`GetSpellIdForDifficulty`. Every area is a creature's or a player's on players: centre to centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Three princes | Valanar 37970, Keleseth 37972, Taldaram 37973, guid slots 21/19/20. Any engage pulls all three in one update; Valanar's sets `IN_PROGRESS`, his death is `DONE`, one death kills the others. Taunt DR (`flags_extra` 0x80000); immunities -361 block neither taunt nor attack-me. Berserk at 10 min; leaving the ellipse round (4660.95, 2769.19), 85 by 60 yd, evades them | 2-3 tanks: one on Valanar and Taldaram, one on Keleseth, or one each; spots picked in advance | Tank roster (`bpc.tank`); recovery taunt below DR level 3, generic taunts blocked |
| Invocation of Blood 70952/70981/70982 | Only its holder takes damage; on the others damage becomes threat. Valanar at the pull, at 45 s and every 46 s after to a random one of the other two, then the last; the next takes the current one's health | Kill the empowered one | Resolver on the empowered prince (`bpc.phase`, `bpc.target`) |
| Kinetic Bomb 72053 | 1.5 s cast at 18-24 s, then every 30.5 s (10) or 20.5 s (25). A bomb 38454 appears 20 yd above a random point within 50 yd of Valanar, falls 2 yd/s and explodes within 2 yd of its landing: 72052 10.8-13.2k/12.6-15.4k/16.2-19.8k/18-22k and a knockback within 50 yd. A direct hit (never periodic) gives it Kinetic Bomb Knockback 72087 for 3 s and lifts it 2 yd/s until 3 s after the last hit. Despawns after 60 s | A hunter or three pets juggle | One duty bot per bomb (`bpc.bomb`), shooting it below 10 yd while it carries no knockback (`bpc.juggle`) |
| Shock Vortex 72037 | 1.5 s cast at 15-20 s, then every 18-23 s while Valanar is not empowered: a vortex 38422 within 13 yd of a random player; from 4.5 s on, 71944 8k/10k/10k/12k and a knockback within 12 yd every 1.5 s, for its 30 s | Stay out; tanks move the princes off one | Nodes within 14.25 yd blocked and re-dealt; a bot without one dodges (`bpc.dodge`) |
| Empowered Shock Vortex 72039 (73037-73039) | Empowered, every 30 s instead: a 4.5 s cast, then 5k (10N) to every player within 30 yd of him and 72038 5k/6k/7k/8k plus a knockback within 12 yd of each of them | Everyone 12 yd apart; melee scatter toward the back | Healers and ranged already on nodes; everyone else runs to a free node for the cast (`bpc.slot`) |
| Shadow Lance 71405, Empowered 71815 | Keleseth casts from 10 yd off his victim, 1.5 s casts: 15.6-16.4k/17.1-17.9k/18-19k/19-20k; empowered 78-82k/85-90k/90-95k/95-100k | Tank him; save cooldowns for his phase | His tank chains buttons through his phase (`bpc.defensive`) |
| Shadow Resonance 71943 | Every 10-15 s a Dark Nucleus 38369 at a random point within 60 yd of him, pulled inside the room. It takes the last unit to hit it directly (500M threat) and beams 71822 on it within 15 yd: -35% shadow taken, 1k a 3 s tick, one per nucleus. Taunts land, no DR. It damages itself and dies in about a minute | Keleseth's tank holds up to 6; nobody else touches them | His tank taunts them within 30 yd and walks toward one outside his phase (`bpc.nucleus`); area damage held near them |
| Glittering Sparks 71806 → 71807 | 12-15 s, then every 15-25 s, a cone at Taldaram's victim: fire, an 8 s fire DoT and a slow, magic | Dispel at once | Class dispel nodes (below `ACTION_RAID`) |
| Conjure Flame 71718, Empowered 72040 | 3 s cast at 20 s, then every 20-25 s, or 15 s empowered. A ball 38332 (empowered 38451) appears on him 5 yd up at the cast's start and chases, at 11 yd/s, a random player more than 10 yd from him at its end; on contact Flames 71393 7.5k/10k/10k/12k within 15 yd, through immunities | Victim moves away from others | The victim walks 16.5 yd clear of everyone (`bpc.ball`) |
| Ball of Inferno Flame | Carries 20 stacks of 71756 (row 2232), +3520 fire (10N) to the explosion each; every 0.2 s Empowered Flare 71708 1k/1k/1.5k/1.5k hits players within 10 yd, and each player hit takes a stack | Melee in its path, ranged shuffle in, victim kites through ranged | Up to 3 soakers, melee first, stand in its path short of the victim (`bpc.soak`) |
| Shadow Prison, heroic | Every prince carries 72998; each second a player moves, 72999 stacks on it: 350 plus 500 a stack, gone after 10 s still | Move only when needed | Generic movers held all fight; only the encounter's movers and reach move a bot |

## Layout

Every stand is a point of one triangular lattice with 13.5 yd sides (12 yd vortex plus two 0.75 yd park
tolerances), navprobed with `--nav 0x09`; every node sees all three tank spots and Keleseth's place 10 yd
east of his, and paths to the dais nodes are normal. The floor is flat at 361.16 out to 45 yd from
(4638.71, 2769.37); the dais east of x 4662 climbs to 364.1.

- **Tank spots**, at the stair foot: Valanar (4655.00, 2749.00), with Taldaram too under three tanks;
  Keleseth (4655.00, 2789.50), 40.5 yd north, outside Valanar's 30 yd; Taldaram's own tank (4643.31,
  2742.25), 13.5 yd west of Valanar's, a spread node otherwise.
- **Nodes** (`ICC_BPC_NODES`, 41): healers keep one in 36 yd of every tank spot in use, leaving the
  ranged zone round Valanar's spot when they can; ranged take the nearest within 28 yd (hunters 32) of
  the empowered prince, held to 31 (35); during Empowered Shock Vortex every other bot takes the
  nearest free one within 28 yd and gives it back 0.5 s after the cast. A bot without a node holds no
  stand.

## Duties

- **Tanks** (`bpc.tank`): bot tanks ranked main tank, assist tanks in order, guid. Three or more:
  Valanar, Keleseth, Taldaram one each; two: Valanar and Taldaram, then Keleseth; one: Valanar and
  Taldaram. A tank's resolver takes its own prince somebody else holds first, then its empowered one,
  then the one it is on. With all its princes on it, it walks to its spot; Keleseth's stops once he
  parks within 12 yd of his spot, so his tank fights him there, and drags him back past 16. A prince
  of its own on someone else is taunted only below DR level 3 (`icc bpc prince taunt`).
- **Keleseth's tank**: under 6 Shadow Resonance it taunts the nearest nucleus not on it within 30 yd,
  and outside his phase, taunt ready, walks to 28 yd of one farther off.
- **Targets**: tanks their princes, a bomb shot, a player's skull, else the empowered prince. Area
  damage is held within 12 yd of a prince without the Invocation, whose damage becomes threat, or of a
  nucleus.
- **Kinetic Bombs**: each bomb goes to the nearest free ranged DPS bot within 60 yd, hunters first,
  then mages, then the rest, kept while both live. The bot shoots it below 10 yd without Kinetic Bomb
  Knockback, else fights; it leaves its node while shooting or while its node is out of range of the
  bomb.
- **Balls**: a chased bot walks to the nearest spot within 15 yd 16.5 yd from every other player and
  clear of vortices. For a Ball of Inferno Flame with stacks, three non-tank bots without a bomb, melee
  first, nearest its path: one off the path walks to the first point it reaches before the ball,
  16.5 yd short of the victim.
- **Defensives**: Keleseth's tank spends `NextTankDefensive` through his empowered phase while he hits
  it; any tank under 40% spends a physical button; class buttons are held to 25%. A human tank is
  called for both (`bpc.humancall`).
- **Hooks for the merge stage** (`HookRows()`): burst `IccBloodPrincesBurstWindow` (lust in Keleseth's
  phase, the guide's quietest), opener `GetBpcOpenerBosses` (Valanar), resist
  `IccBloodPrincesResistSchools` (shadow and fire all fight).

Rules: targeting owned for the whole fight; node holders keep only attacks and resurrect reach, tanks
on their spot only attacks; heroic prison hold; spell movers, avoid aoe (it flees a vortex 5 yd and the
ball the soak walks into), generic taunts, Death Grip and Army of the Dead blocked. A bomb is the one
pick that can be out of combat: nobody may have hit it yet. It exists only mid-fight, so it pulls
nothing, and drop target is blocked while the bot shoots it.

## Decisions

- Spots 40.5 yd apart at the stair foot, per the Warcraft Tavern diagram; the old ones were 51 yd
  apart, out of ranged reach of each other. Only about six nodes reach both sides, so ranged change
  sides with the empowered prince and healers never move.
- A lattice, not rings round Valanar: a 25-man ring at 13 yd spacing left the room.
- Melee hold nodes only for the 4.5 s cast; all fight would cost their uptime.
- Nodes block heal reach: a healer walking off breaks the spread, and every healer node covers every
  tank spot.
- A tank's own prince and a bomb shot outrank a player's skull, as Rotface's kiter's ooze does: the
  bomb would land, the prince go to a DPS.
- Bombs are shot only when low: the old shooter fired all fight, a lost DPS slot. Periodic damage never
  procs Unstable, so mages rank above other casters.
- No opening positions: every stand opens on Valanar's engage, and pre-pull stands would catch bots
  walking through.
- Heroic holds generic movers all fight rather than past a stack count (old: 12, tanks 18).

## Cheats

None. The old floor snaps (an assist tank 6 yd off the floor, a bomb shooter above Z 371.2, through
`NearTeleportTo`) are gone: knockbacks land on the floor by themselves.

## Known gaps

- No 631 trace and never built: every threshold is a model until a pull.
- One bot tank: Keleseth goes to whoever has threat. A fourth tank takes the DPS branch.
- No ranged DPS bot: bombs land. A DoT-first caster on bomb duty wastes its first shot; pets are not
  set on bombs.
- On 25-man the nodes can run short for the cast; a bot without one stays where it is.
- A ball victim finds no clear spot in a dense spread and stays; no personal defensive on an Inferno
  Flame catch.
- The soak path is the straight line from the ball to its victim; a kiting victim bends it.
- A bomb shooter's or soaker's walk leaves its node in the middle of a cast window.
- Humans hold no duty. Glittering Sparks waits for the class dispel nodes.
- r14: unused `bpcSpreadLockTimers` (`ICCScripts.h`) and the council ids in `ICCData.h` (`NPC_PRINCE_*`,
  `NPC_DARK_NUCLEUS`, `NPC_KINETIC_BOMB1-4`, both balls, `NPC_SHOCK_VORTEX`, the Invocations,
  `SPELL_EMPOWERED_SHOCK_VORTEX1-4`); hoist one walk base.

Not defects: Kinetic Bomb 38775-38777 are 38454's difficulty entries, which `GetEntry` never returns
([pitfalls.md](../../engine/pitfalls.md#detection-that-never-fires)).

## Targets for the next pulls

25N first: `--tanks` holds three owners all fight; `--targets` is `empowered` for DPS after each
transfer, `recover` only at the pull; `--bombs` has no bomb landing (no 72052 hit) and shots per bomb
under 6; `--spread` shows every non-tank on a node at each Empowered Shock Vortex, and nobody hit by
two 72038; `--nucleus` reaches `full` before Keleseth's first phase; `--balls` shows soakers on every
Inferno Flame and no explosion over 20k. 25H: the same, plus Shadow Prison stacks under 10 outside
the scatter and transfers.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `bpc.phase` | per bot | `valanar`, `keleseth`, `taldaram`, `none` between pulls |
| `bpc.target` | per bot | `owned`, `recover`, `bomb`, `skull`, `empowered`, `none` |
| `bpc.tank` | per tank | prince bits 1 Valanar, 2 Keleseth, 4 Taldaram |
| `bpc.slot` | per bot | node 1-41, `0` given up |
| `bpc.dodge` | per bot | `melee`, `clear`, `blocked`, `none` |
| `bpc.bomb`, `bpc.juggle` | per duty bot | the bomb, `0` gone; `shoot` or `wait` |
| `bpc.ball`, `bpc.soak` | per bot | `plain`/`inferno` then `clear`, `step`, `none`; soaker `1`/`0` |
| `bpc.nucleus` | per tank | `taunt N`, `walk`, `cooldown`, `full`, `none` |
| `bpc.defensive`, `bpc.humancall` | per tank | the button spent, `idle`; the call to a human tank |

`tools/botobs/bosses/blood_prince_council.py`: `--phases`, `--targets`, `--tanks`, `--spread`, `--bombs`,
`--balls`, `--nucleus`, `--cheats`. `blood_prince_trash.py` reads a trash file: banner and cheats.

## Blood Prince trash

Boss id 14, slug `blood-prince-trash`, prefix `bpc.`. The state goes `NOT_STARTED` then `DONE` once
the four spawns guarding the Crimson Hall door (201479, 201482, 201646, 201659) die, never
`IN_PROGRESS`, and none of the trash is boss-flagged. So a `UnitScript` in `Util/ICCScripts.cpp`
opens the trace with `RaidObs::MarkPull(map, creature, "blood-prince-trash")` when any spawn of the
Darkfallen Advisor 37571, Archmage 37664, Blood Knight 37595 or Noble 37663 engages a player (18
spawns on 631). The pull that kills the last door guard closes on the `DONE`, as `kill`; any other
closes `idle`. A pet's pull is traced only once one of them engages a player: each fires the hook
once, at `Creature::AtEngage`, on its first target. The strategy has no trash code.
