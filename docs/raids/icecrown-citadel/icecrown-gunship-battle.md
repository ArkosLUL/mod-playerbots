# Icecrown Gunship Battle

Boss id 2, slug `icecrown-gunship-battle`, prefix `gsb.`. Code: stem `Gunship`
(`src/Ai/Raid/ICC/*/ICC*_Gunship.*`). Raid conventions: [README.md](README.md). Guides: Warcraft
Tavern 25 and 10, Wowhead, instant-carry.

## The battle (core)

The raid fights from its own ship, a transport, the instance's guid slot 2 (`GunshipGUID`): the
Skybreaker (Alliance, cannons 36838) against Orgrim's Hammer, or Orgrim's Hammer (Horde, cannons 36839)
against the Skybreaker; the enemy ship is slot 40. The faction is `GetData(27)`. `IN_PROGRESS` comes as
the enemy ship arrives; the battle ends when one ship's creature dies, the enemy's by cannon siege damage
(credit spell 72959). The ships hold still meanwhile, so the core keys its decks on world y: the Alliance
raid's deck lies past y 2042 and the enemy's short of 2025; the Horde raid's short of 2431 and the
enemy's past 2445. Anyone at z 478 or more is airborne. A ship also dies when every living player sits in
a vehicle (`WipeGunshipBlizzlike`). Bots put a unit on the ship on its side of the gap's middle (2033.5,
2438), so a bot on a deck's very edge never reads as in flight; a dodge spot must lie inside the bound.

| Ability (ids) | Script, 10N / 25N / 10H / 25H | Guide | Bots |
|---|---|---|---|
| Cannons: Cannon Blast 69399/70172, Incinerating Blast 69401/70174 | 2 cannons on 10, 4 on 25 (spawn masks). Blast adds 6-10 heat, 500 ms; Incinerating spends it, 1000 + (heat - 10)² × 8, 1500 ms; 100 heat is Overheat 69487 (2.5 s) | spam Blast, Incinerate near full, aim the mortars | ranged DPS first, then melee, one per live cannon; Incinerate from 90 heat; mortars, then gunners, then anything on the enemy deck |
| Below Zero 69705 | the freeze mage's 900 s channel stuns every cannon and ejects its gunner; first when the enemy ship falls under 90% and 45 s after his ship spawned, then 30-33.5 s after the last one dies | board, kill the mage, come back | see Boarding |
| Captain (36939 / 36948), Battle Fury 69638 | attacks only players on his deck, never dies; each hit adds a stack, +10% on normal (72306 on 25), +12% on heroic (72307, 72308), 16 s, refreshed per stack; Cleave 15284, Rending Throw 70309 off melee, Taste of Blood 69634 under 65%; taunts diminish on him (`flags_extra` 0x80000) | a tank picks him up first and drags him to the edge; leave with the party | the board tank holds him; retreat on stacks |
| Riflemen 36969 / Axethrowers 36968 | 4 on 10, 8 on 25, 30 s respawn; Shoot / Hurl Axe (2 s, rows 2195-2196) at players on the other ship | ranged clear them from the edge | home ranged, after boarders, in range only |
| Mortars 36978 / Rocketeers 36982 | 2 on 10, 4 on 25, every 9 s: 69679, a 5 s cast at a player's spot on the raid's ship, the 71371 marker for 5 s from launch, then Explosion 4 yd centre to centre (69680, 69687; heroic 69688/69689 knock back) | move out | dodge |
| Boarders: marines 36950 / reavers 36957, sergeants 36961 / 36960 | 12 s in, then every 60 s: 2 + 1 on 10, 4 + 2 on 25, through the portal to the raid's ship; Wounding Strike 69651; Bladestorm 69652, whose 69653 has no radius, so it hits nobody here; Desperate Resolve under 25%; Burning Pitch on the hull when no player is valid | the home tank collects them, the raid kills the oldest first | home tank at the portal exit, taunts; home melee and ranged by rank |
| Battle Experience | adds rank up 100 s after reaching their slot, then 70, 60 and on heroic 90 more (71188, 71193, 71195, 72525) | "every 30 s" | rank orders the kills |
| Check for Players 70332 | every 5 s, anyone 10 yd under the raid's ship creature is teleported back on deck | don't fall | nothing to do: the ground is 620 yd down, so a faller is back within ~6.5 s |
| Rocket Pack 68645 (item 49278, shirt slot) | 40 yd jump, 1 s; needs Rocket Pack Useable 70348 (200 yd round Zafod 37184); flight aura 68721 until landing | jump across | the only cross-deck mover |

## Roster

Bots only; humans hold no duty and a player's skull wins. Rebuilt once a second from the living group:

- **board tank**: the first bot tank, main tank first; **home tank**: the next.
- **cannon**: ranged DPS by guid, then melee DPS, one per live cannon.
- **home heal**: 2 healers on 25, 1 on 10; the other healers **board heal**.
- **boarder**: melee DPS; **home ranged**: ranged DPS. Ranged board only when no melee DPS bot exists.

Below Zero ejects the cannoneers: melee ones board, ranged ones take an edge slot.

Every bot takes its rocket pack from Zafod before the battle, a cannoneer before its seat. A bot
still without one holds no stand, since the stand would pull it back from Zafod.

## Boarding

`gsb.board` per bot: `home`, `wait`, `go`, `back`, `retreat`.

- A freeze sends the board tank across at once; the rest of the party walks to the launch point and
  waits until a tank holds the captain, or 15 s into the freeze.
- Jumps run launch point ↔ landing point (24 yd Alliance, 30 yd Horde), walking there first, but not
  while a hazard covers it.
- On the enemy deck the board tank taunts him off any non-tank and holds him 6 yd in from the landing
  point toward the mage (30 / 35 yd from the home edge), the board healers stand on the landing point and
  the melee kill the mage.
- When the mage dies everyone goes home, the tank once nobody else is on the enemy deck or 10 s on.
- **Retreat**: once his stacks reach +200% (20 on normal, 17 on heroic) the party goes home, tank last,
  and boards again once his stacks are gone. Nobody hits him meanwhile, so they fall off 16 s after.

## Home deck

- Home tank: holds the portal exit (the core's `SkybreakerTeleportExit` / `OrgrimsHammerTeleportExit`,
  transport-local) while no boarder is on deck, and taunts any boarder not on a tank, one per cooldown.
- Healers and ranged: edge slots 5 yd apart, 4 and 8.5 yd inboard of the cannon line (transport-local),
  healers nearest the landing.
- Melee home DPS and ranged: boarders on the home deck, highest rank first; ranged then riflemen in
  `reach spell` range, then mortars in range.

## Movement

- One mover crosses decks, `icc gunship rocket jump`; while a bot is in flight or in the gap, nothing
  else moves it.
- A hand-written multiplier zeroes reach, heal reach and charge at a target on the other deck. Follow,
  formation, flee, run away, random moves, collision steps, blink, disengage and avoid aoe are blocked
  while live: each can walk a bot off the edge.
- Stands latch (park within 1 yd, walk back past 2.5) and hold every generic mover but attacks.
- **Dodge**: anyone on deck and out of a cannon who stands within 5.5 yd of a mortar cast's spot or a
  71371 marker, or inside a Burning Pitch patch's reach + 1.5, walks to the nearest spot 6.5 yd clear
  (reach + 2.5), `MOVEMENT_FORCED`, interrupting a cast only on heroic. The sweep stays inside a box per
  hull from the core's crew slots (Skybreaker x -31..10, y -21..15; Orgrim's Hammer x -13..30, y -21..27,
  transport-local) and on the bot's deck: `FindNearestPositionClearOfHazards`' navmesh check refuses
  every point on a transport deck.

## Coordinates

The world points (launch, landing, mage area) are kept from the old code and match the core's y bounds;
the edge slots and the portal exit are transport-local from the core's spawns. Every deck is a
gameobject floor: navprobe reads each point off mesh with terrain 620 yd below, so none can be checked
offline.

## Cheats

None. `gsb.battlefury` (Battle Fury stripped off the captain) is replaced by the return home and the
retreat. The fall snap (`NearTeleportTo` 5 yd under the deck) is gone: the core's rescue covers every
fall.

## Known gaps

- No 631 trace yet.
- The edge slots assume the cannon side faces the enemy ship.
- No tank defensive or burst on the captain's stacks.
- Mortars sit beyond every player's reach; only the cannons kill them.
- A human holds no duty: a human tank on the captain is respected, nothing else.

## Next pull

25N, then 25H: `gsb.fury` at each thaw (the stacks when the mage died) and any `gsb.retreat`; time from
`frozen` to the first `go` and to `fight`; `gsb.dodge` entries against Explosion hits; deaths by falls
(the rescue's teleport 67335) and to the captain.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `gsb.phase` | per bot | `fight`, `frozen` |
| `gsb.duty` | per bot | the roster above |
| `gsb.deck` | per bot | `friendly`, `enemy`, `between` (airborne), `cannon`, `falling` |
| `gsb.board` | per bot | Boarding |
| `gsb.stand` | per bot | `tank`, `landing`, `launch`, `exit`, `edge`, `none` |
| `gsb.target` | per bot | the pick's rule: `skull`, `captain`, `mage`, `boarder`, `gunner`, `mortar`, `none` |
| `gsb.cannon` | per cannoneer | the aim: `mortar`, `gunner`, `other`, `none`, `overheat` |
| `gsb.dodge` | per bot | `rocket`, `pitch`, `none` |
| `gsb.retreat` | per instance | the stack count a retreat latched at, 0 when it clears |
| `gsb.fury` | per instance | the captain's stacks at each thaw |

Each mortar cast is a hazard circle (69679, 4 yd). No cheat key. `tools/botobs/bosses/icecrown_gunship_battle.py`:
`--phases`, `--duties`, `--decks`, `--boarding`, `--targets`, `--cannons`, `--dodges`, `--cheats`.
