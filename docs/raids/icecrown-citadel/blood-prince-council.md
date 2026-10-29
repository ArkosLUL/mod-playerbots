# Blood Prince Council

Boss id 7, slug `blood-prince-council`, prefix `bpc.` ([README](README.md)). Code: stem `BloodPrinces`
(`src/Ai/Raid/ICC/*/ICC*_BloodPrinces.*`); nodes `icc bpc` dodge, flame defensive, lance defensive, dispel,
tank defensive, ball, prince taunt, nucleus, soak, target, stand, brake; multipliers `icc bpc` splash,
brake, direct hit. Core: `boss_blood_prince_council.cpp`.

## Mechanics

Ids and damage are 10N/25N/10H/25H where they differ; code remaps a difficulty row through
`GetSpellIdForDifficulty`. Every area is a creature's or a player's on players: centre to centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Three princes | Valanar 37970, Keleseth 37972, Taldaram 37973, guid slots 21/19/20. Any engage pulls all three in one update; Valanar's sets `IN_PROGRESS`, his death is `DONE`, one death kills the others. Taunt DR (`flags_extra` 0x80000); immunities -361 block neither taunt nor attack-me. Berserk at 10 min; leaving the ellipse round (4660.95, 2769.19), 85 by 60 yd, evades them | 2-3 tanks: one on Valanar and Taldaram, one on Keleseth, or one each; spots picked in advance; a warlock can tank Keleseth | Tank roster (`bpc.tank`), with one tank in the raid a stand-in on Keleseth; recovery taunt below DR level 3, generic taunts blocked; threat brake on the empowered prince (`bpc.brake`) |
| Invocation of Blood 70952/70981/70982 | Only its holder takes damage; on the others damage becomes threat. Valanar at the pull, then at 45 s and every 46 s on to a random one of the other two, the last, Valanar again, by a 25 yd/s missile; the next takes the current one's health | Kill the empowered one | Resolver on the empowered prince (`bpc.phase`, `bpc.target`) |
| Kinetic Bomb 72053 | 1.5 s cast at 18-24 s, then every 30.5 s (10) or 20.5 s (25). A bomb 38454 appears 20 yd above a random point within 50 yd of Valanar, falls 2 yd/s (heroic 3) and explodes within 2 yd of its landing: 72052 10.8-13.2k/12.6-15.4k/16.2-19.8k/18-22k and a knockback within 50 yd. A damaging hit, melee included, never periodic or a DoT's application (Unstable 72059, `spell_proc` `SpellTypeMask` 1), gives it Kinetic Bomb Knockback 72087 for 3 s and lifts it 2 yd/s until 3 s after the last hit. Combat reach 0, so melee hits it within 5 yd, height included. Despawns after 60 s | A hunter or three pets juggle | One duty bot per bomb (`bpc.bomb`), melee only when no ranged one is free, shooting it within 5 s of landing (10 yd, heroic 15; melee add their walk) while it carries no knockback (`bpc.juggle`); one pet per bomb (`bpc.petbomb`); spells that deal no damage held on it |
| Shock Vortex 72037 | 1.5 s cast at 15-20 s, then every 18-23 s while Valanar is not empowered: a vortex 38422 within 13 yd of a random player; from 4.5 s on, 71944 8k/10k/10k/12k and a knockback within 12 yd every 1.5 s, for its 30 s | Stay out; tanks move the princes off one | Nodes within 14.25 yd blocked and re-dealt; a bot without one dodges (`bpc.dodge`) |
| Empowered Shock Vortex 72039 (73037-73039) | Empowered, every 30 s instead: a 4.5 s cast (heroic 4), then 5k to every player within 30 yd of him and 72038 5k/6k/7k/8k plus a knockback within 12 yd of each of them | Everyone 12 yd apart; melee scatter toward the back, onto the dais | Healers and ranged already on nodes; everyone else runs to a free node it reaches before the cast ends, else to the reachable node that costs the fewest hits (`bpc.slot`, `bpc.hits`) |
| Shadow Lance 71405, Empowered 71815 | Keleseth casts from 10 yd off his victim, 1.5 s casts: 15.6-16.4k/17.1-17.9k/18-19k/19-20k; empowered 78-82k/85-90k/90-95k/95-100k | Tank him; save cooldowns for his phase | His tank chains buttons through his phase (`bpc.defensive`); a stand-in spends `icc.personal` on a lance that would leave it under 25% |
| Shadow Resonance 71943 | Every 10-15 s a Dark Nucleus 38369 at a random point within 60 yd of him, kept if past 35 yd and inside the room, else moved to the farthest such point in sight on that line. Area spells skip it (immunities -363); it takes the last unit to hit it directly (500M threat) and beams 71822 on it within 15 yd: -35% shadow taken, 1k a 3 s tick, one per nucleus. Taunts land, no DR. It damages itself and dies in about a minute | Keleseth's tank holds up to 6; nobody else touches them | His owner taunts them within 30 yd, a stand-in with no taunt hits them, and walks toward one outside his phase (`bpc.nucleus`); area damage held near them |
| Glittering Sparks 71806 → 71807 (72796-72798) | 12-15 s, then every 15-25 s, a cone at Taldaram's victim: fire, then a magic fire DoT and slow, 8 s and 20%, heroic 10 s and 40% | Dispel at once, Mass Dispel on several | The nearest priest or paladin bot within 30 yd dispels each carrier (`bpc.dispel`) |
| Conjure Flame 71718, Empowered 72040 | 3 s cast at 20 s, then every 20-25 s, or 15 s empowered. A ball 38332 (empowered 38451) appears on him 5 yd up at the cast's start, marks a random player more than 10 yd from him and chases it at 11 yd/s from the cast's end; on contact Flames 71393 7.5k/10k/10k/12k within 15 yd, through immunities (`SPELL_ATTR0_NO_IMMUNITIES`) | Victim moves away from others | The victim walks 16.5 yd clear of everyone, else to the least crowded node within 20 yd (`bpc.ball`) |
| Ball of Inferno Flame | Carries 71756 (row 2232), 20 stacks on 10-man and 25 on 25-man, each adding 3520/3200/5280/4800 fire to the explosion; every 0.2 s Empowered Flare 71708 1k/1k/1.5k/1.5k hits players within 10 yd, and each player hit takes a stack | Melee in its path, ranged shuffle in, victim kites through ranged; defensive cooldowns on the catch | Up to 3 soakers, melee first, stand in its path short of the victim and hold there (`bpc.soak`); the victim spends a magic button or Dispersion on a catch that would leave it under 25% (`bpc.flame`) |
| Shadow Prison, heroic | Valanar's 73001 ticks on every player each second, and on a moving one 72999 stacks: 350 plus 500 a stack, gone after 10 s still | Move only when needed | Generic movers held all fight; only the encounter's movers and reach move a bot |

## Layout

Every stand is a point of one triangular lattice with 13.5 yd sides (12 yd vortex plus two 0.75 yd park
tolerances; neighbours sit 13.499 yd apart), navprobed with `--nav 0x09` to within 0.1 yd; every node
sees all three tank spots and Keleseth's place 10 yd east of his (164 `los` rays), and paths to the dais
nodes are normal. The floor is flat at 361.16 out to 45 yd from (4638.71, 2769.37); the dais east of
x 4662 climbs to 364.1.

- **Tank spots**, at the stair foot: Valanar (4655.00, 2749.00), with Taldaram too under three tanks;
  Keleseth (4655.00, 2789.50), 40.5 yd north, outside Valanar's 30 yd; Taldaram's own tank (4643.31,
  2742.25), 13.5 yd west of Valanar's, a spread node otherwise.
- **Nodes** (`ICC_BPC_NODES`, 41): only 15 reach Valanar's spot within 28 yd, 14 with Taldaram's own
  spot. Healers stay out of that zone while a node outside it reaches a tank spot, each tank spot in use
  reached within 36 yd by half
  the healers rounded up, every healer node by one spot at least. The dais field, the five dais nodes
  within 28 yd of his spot, is never a healer's and in his phase no ranged bot's: the melee scatter
  there. Ranged take the nearest node within 28 yd (hunters 32) of the empowered prince, held to 31
  (35). During Empowered Shock Vortex every other bot takes the nearest free node it runs to at 7 yd/s
  in the cast left less 0.25 s, at most 28 yd; with none free, the reachable node whose vortex hits on
  it and on the players it lands next to are fewest, shared if need be and only if that beats staying
  put; it gives the node back 0.5 s after the cast. A bot without a node holds no stand.

## Duties

- **Tanks** (`bpc.tank`): bot tanks ranked main tank, assist tanks in order, guid. Three or more:
  Valanar, Keleseth, Taldaram one each; two: Valanar and Taldaram, then Keleseth; one: Valanar and
  Taldaram, and with no other tank in the raid, human or bot, Keleseth goes to a stand-in: a warlock,
  else a DK or paladin DPS (class taunt), else any ranged DPS, kept while it lives. A tank's resolver
  takes its own prince somebody else holds first, then its empowered one, then the one it is on. With
  all its princes on it, it walks to its spot; a melee owner of Keleseth alone stops once he parks
  within 12 yd of his spot and fights him there, dragging him back past 16; a ranged stand-in keeps
  the spot. A prince of its own on someone else is taunted only below DR level 3 (`icc bpc prince
  taunt`). A fourth tank fights the empowered prince under the brake.
- **Keleseth's owner**: under 6 Shadow Resonance it taunts the nearest nucleus not on it within 30 yd,
  or, with no taunt, takes it as its pick; outside his phase, taunt ready or none, it walks to 28 yd
  of one farther off.
- **Targets**: a stand-in's nucleus, owners their princes, a bomb shot, a player's skull, else the
  empowered prince. Area damage is held within 12 yd of a prince without the Invocation, whose damage
  becomes threat, or of a nucleus. On a bomb shot or a nucleus grab, spells that deal no damage on
  landing are held (`icc bpc direct hit`).
- **Threat brake** (`bpc.brake`): a damage dealer that doesn't own the empowered prince is held off
  him from 90% of his owner's threat, or his victim's with no owner on his list, until under 80%; a bot
  holding him away from his owner too. It still shoots bombs and follows a skull on anything else; its
  pickers, follow and random moves are blocked, its swing, Auto Shot or wand on him stops (`icc bpc brake`), and its
  attacks on him and area damage within 15 yd of him are held.
- **Kinetic Bombs**: each bomb goes to the nearest free DPS bot within 60 yd, hunters first, then
  mages, then other ranged, melee last, kept while both live. The bot shoots it within 5 s of landing,
  plus its walk for melee, without Kinetic Bomb Knockback, else fights; it leaves its node only while
  its node is out of range of the bomb, 30 yd with the height (hunters 35). Each bomb also gets the
  nearest pet within 60 yd, hunter, warlock, DK, others, which stays on it while both live.
- **Balls**: a chased bot walks to the nearest spot within 15 yd 16.5 yd from every other player and
  clear of vortices, else to the node within 20 yd with fewer players in 16.5 yd than where it stands.
  For a Ball of Inferno Flame with stacks, three non-tank bots without a bomb or a prince, melee first,
  nearest its path: one within 6 yd of the path and 16.5 yd of the victim holds there (`icc bpc soak
  hold`), any other walks to the first point it reaches before the ball, 16.5 yd short of the victim.
  Soakers keep soaking through an Empowered Shock Vortex cast.
- **Defensives**: Keleseth's tank spends `NextTankDefensive` through his empowered phase while he hits
  it; any tank under 40% spends a physical button; class buttons are held to 25%. A human tank is
  called for both (`bpc.humancall`). A stand-in asks `GetRaidPersonalDefensive` on each lance at it,
  as a victim keeper; the Inferno victim, 1.5 s out, spends `NextMagicDefensive` or Dispersion when
  Flames plus the ball's stack bonus would leave it under 25%, never an immunity or Cloak of Shadows.
- **Glittering Sparks**: per carrier the nearest priest (Dispel Magic) or paladin (Cleanse) bot that can
  land it now within 30 yd, kept while it can; nobody walks.
- **Hooks for the merge stage** (`HookRows()`): burst `IccBloodPrincesBurstWindow` (lust in Keleseth's
  phase, the guide's quietest), opener `GetBpcOpenerBosses` (Valanar), resist
  `IccBloodPrincesResistSchools` (shadow and fire all fight).

Rules: targeting owned for the whole fight; node holders keep only attacks and resurrect reach, owners
on their spot (a stand-in included) and soakers on the path only attacks; heroic prison hold; spell
movers, avoid aoe (it flees a vortex 5 yd and the ball the soak walks into), generic taunts, Death Grip
and Army of the Dead blocked; a stand-in's Soulshatter, Fade, Feign Death and Invisibility (Keleseth and one
nucleus are the two attackers that fire them) and Misdirection blocked; tank druids kept out of caster form.

A bomb and a loose nucleus are the picks that can be out of combat: nobody may have hit a bomb yet, and a
loose nucleus has no victim. Both exist only mid-fight, so they pull nothing, and drop target is blocked
while a bot holds either.

## Decisions

- Spots 40.5 yd apart at the stair foot, per the Warcraft Tavern diagram; the old ones were 51 yd
  apart, out of ranged reach of each other. Only about six nodes reach both sides, so ranged change
  sides with the empowered prince and healers never move.
- A lattice, not rings round Valanar: a 25-man ring at 13 yd spacing left the room.
- Melee hold nodes only for the cast; all fight would cost their uptime. They scatter onto the dais,
  the diagram's melee spread area, which the ranged leave free in his phase.
- Healers leave Valanar's zone and reach a tank spot by half of them, not every spot each: with every
  healer on every spot, five of six 25-man healers sat in his 14-node zone and left the ranged and
  melee short. A runner with no free node shares one rather than stay in the melee stack: a shared node
  costs one extra 6-8k vortex, the stack round him one per neighbour.
- Nodes block heal reach: a healer walking off breaks the spread.
- A tank's own prince, a stand-in's nucleus and a bomb shot outrank a player's skull, as Rotface's
  kiter's ooze does: the bomb would land, the prince go to a DPS.
- One tank: a warlock stands in first, as Warcraft Tavern and Wowhead tank Keleseth; a DK or paladin
  next for its taunt; never other melee, since he casts from 10 yd. No bot stands in beside a human
  tank: humans hold no duty.
- Bombs are shot only when low: the old shooter fired all fight, a lost DPS slot. The window is time to
  landing, since heroic bombs fall faster. Only damaging hits proc Unstable, so mages rank above other
  casters and spells that deal no damage are held on a bomb. Pets stay on bombs all fight as the guides
  set them; melee juggle only when no ranged bot can, as they reach a bomb only at 5 yd.
- A shooter whose node reaches the bomb shoots from it, so a bomb shot no longer walks it off its node
  in a cast window. Soakers stay on the path through one: an unsoaked 25H Inferno is 132k on the
  victim, a shared vortex 8k.
- The brake reads the empowered prince's owner, since a prince past taunt DR level 3 comes back on
  threat alone.
- The Inferno victim never takes an immunity: Flames ignores them. Nor Cloak of Shadows: Flames always
  hits (`SPELL_ATTR3_ALWAYS_HIT`). Wowhead's Divine Guardian and Pain Suppression are external, which no
  shared dealer hands out.
- Glittering Sparks: single dispels from where the dispeller stands; the cone mostly hits Taldaram's
  tank, and nodes block the walk. No Mass Dispel.
- Area damage is held near a nucleus though area searches skip it: the splash test can't tell plain
  area damage from a cleave or chain, which take it.
- No opening positions: every stand opens on Valanar's engage, and pre-pull stands would catch bots
  walking through.
- Heroic holds generic movers all fight rather than past a stack count (old: 12, tanks 18).
- No Berserk pacing: all three berserk at 10 min whatever the bots do, and lust already waits for
  Keleseth's phase.

## Cheats

None. The old floor snaps (an assist tank 6 yd off the floor, a bomb shooter above Z 371.2, through
`NearTeleportTo`) are gone: knockbacks land on the floor by themselves.

## Known gaps

- No 631 trace: every threshold is a model until a pull. Built clean at `623638b99` on 2026-10-29;
  the stand-in, brake, pets, dispels and scatter sharing came after it.
- A stand-in takes Keleseth with no Shadow Resonance at the pull, 15.6-20k a lance until its first
  nucleus 10-15 s in (`icc.personal`, its deaths). With no warlock, DK, paladin or ranged DPS bot
  nobody holds him.
- Pets and melee under a bomb: whether a pet stays under a bomb 20 yd up and a melee bot's reach gets
  under it before it lands needs a pull (`--bombs` pet and shots against 72052 hits). No DPS bot in
  60 yd and no pet: the bomb lands.
- 25-man scatter overflow shares a node, two vortices each (`--spread` hits); a shooter whose node
  is out of the bomb's reach still leaves it mid-cast.
- A healer node reaches only some tank spots, so a far tank relies on half the healers.
- No personal defensive on the Inferno catch for a class with no magic row (warlock, hunter, mage, rogue),
  and no healthstone: the victim mostly walks, and an item use stops it.
- The soak path is the straight line from the ball to its victim; a kiting victim bends it. The
  fallback node can hop as players move.
- Glittering Sparks on a carrier with no priest or paladin bot within 30 yd waits for the class
  dispel nodes; no Mass Dispel.
- The brake is blind at a threat wipe, a braked bot idles, and its pet keeps hitting the empowered
  prince: only the bot's own threat is braked (`--brake` against his target changes).
- Humans hold no duty.
- r14: unused `bpcSpreadLockTimers` (`ICCScripts.h`) and the council ids in `ICCData.h` (`NPC_PRINCE_*`,
  `NPC_DARK_NUCLEUS`, `NPC_KINETIC_BOMB1-4`, both balls, `NPC_SHOCK_VORTEX`, the Invocations,
  `SPELL_EMPOWERED_SHOCK_VORTEX1-4`); hoist one walk base, and `CanPayFor` and the dispel pick shared
  with Rotface.

Not defects: Kinetic Bomb 38775-38777 are 38454's difficulty entries, which `GetEntry` never returns
([pitfalls.md](../../engine/pitfalls.md#detection-that-never-fires)).

## Targets for the next pulls

25N first: `--tanks` holds three owners all fight; `--targets` is `empowered` for DPS after each
transfer, `recover` only at the pull, `brake` short; `--bombs` has no bomb landing (no 72052 hit),
shots per bomb under 6 and a pet on each; `--spread` shows every non-tank on a node at each Empowered
Shock Vortex, `hits` 1 for most and 2 at worst; `--nucleus` reaches `full` before Keleseth's first
phase; `--balls` shows soakers on every Inferno Flame and no explosion over 20k; `--dispels` covers
each Glittering Sparks. 25H: the same, plus Shadow Prison stacks under 10 outside the scatter and
transfers. With one bot tank: `--tanks` shows the stand-in on Keleseth and `--nucleus` its `hit`.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `bpc.phase` | per bot | `valanar`, `keleseth`, `taldaram`, `none` while the Invocation flies |
| `bpc.target` | per bot | `nucleus`, `owned`, `recover`, `bomb`, `skull`, `brake`, `empowered`, `none` |
| `bpc.tank` | per owner | prince bits 1 Valanar, 2 Keleseth, 4 Taldaram |
| `bpc.slot`, `bpc.hits` | per bot | node 1-41, `0` given up; vortex hits on it in the cast, `none` outside |
| `bpc.dodge` | per bot | `melee`, `clear`, `blocked`, `none` |
| `bpc.bomb`, `bpc.juggle`, `bpc.petbomb` | per duty bot | the bomb, `0` gone; `shoot` or `wait`; its pet's bomb |
| `bpc.ball`, `bpc.soak`, `bpc.flame` | per bot | `plain`/`inferno` then `clear`, `step`, `fewest`, `none`; soaker `1`/`0`; the Inferno victim's button, `safe`, `none`, `idle` |
| `bpc.nucleus` | per owner | `taunt N`, `hit N`, `walk`, `cooldown`, `full`, `none` |
| `bpc.brake` | per bot | `1` held, `0` freed |
| `bpc.dispel` | per carrier | the bot dealt to dispel it, `0` done |
| `bpc.defensive`, `bpc.humancall` | per tank | the button spent, `idle`; the call to a human tank |

`tools/botobs/bosses/blood_prince_council.py`: `--phases`, `--targets`, `--tanks`, `--spread`, `--bombs`,
`--balls`, `--nucleus`, `--brake`, `--dispels`, `--defensives`, `--cheats`. `blood_prince_trash.py`
reads a trash file: banner and cheats.

## Blood Prince trash

Boss id 14, slug `blood-prince-trash`, prefix `bpc.`. The state goes `NOT_STARTED` then `DONE` once
the four spawns guarding the Crimson Hall door (201479, 201482, 201646, 201659) die, never
`IN_PROGRESS`, and none of the trash is boss-flagged. So a `UnitScript` in `Util/ICCScripts.cpp`
opens the trace with `RaidObs::MarkPull(map, creature, "blood-prince-trash")` when any spawn of the
Darkfallen Advisor 37571, Archmage 37664, Blood Knight 37595 or Noble 37663 engages a player (18
spawns on 631). The pull that kills the last door guard closes on the `DONE`, as `kill`; any other
closes `idle`. A pet's pull is traced only once one of them engages a player: each fires the hook
once, at `Creature::AtEngage`, on its first target. The strategy has no trash code.
