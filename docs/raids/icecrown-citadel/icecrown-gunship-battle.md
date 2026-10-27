# Icecrown Gunship Battle

Boss id 2, slug `icecrown-gunship-battle`, prefix `gsb.`. Code: stem `Gunship`
(`src/Ai/Raid/ICC/*/ICC*_Gunship.*`), nodes `icc gunship cannon near`, `icc in cannon`, `icc gunship
rocket jump`, `icc gunship rocket pack setup`; `IccGunshipMultiplier`. Raid conventions:
[README.md](README.md).

## Mechanics (core)

- The raid fights from its own ship, a transport: the Skybreaker (Alliance, cannon 36838) against
  Orgrim's Hammer's High Overlord Saurfang 36939, or Orgrim's Hammer (Horde, cannon 36839) against
  the Skybreaker's Muradin 36948. The captain never dies (damage stops at 1 hp); the battle ends on
  the enemy ship's death, credited by spell 72959.
- **The captain** attacks only players on his own deck, on his side of it (`CanAIAttack`), and keeps
  every player in combat. **Battle Fury** 69637, cast on engage, procs a stacking 69638 per hit
  (+10%; 72306 10H, 72307/72308 25-man +12%), refreshing 16 s: his stacks fall off 16 s after the last
  player leaves his deck. Nothing else resets them, and that is the intended answer, not a bug.
- **Below Zero** 69705: the enemy mage's 900 s stun on all the raid's cannons, which sends the raid
  across to kill him.
- **Falls**: Check for Players 70332, on the raid's ship creature, ticks 70331 every 5 s and
  teleports any player more than 10 yd below that creature back onto the ship (67335).
- Rocket pack: item 49278 from Zafod Boombox 37184; jumping needs Rocket Pack Useable 70348.

## Phase

`IccGunshipEnemyCaptain(bot, range)` is the fight test: the rocket-jump trigger asks within 100 yd,
the multiplier within 200 (cached per ms, since it is asked for every action). No core value decides
between the two radii, and 200 would keep the trigger steering bots at the deck points, which are
world coordinates, a little longer while a ship flies off, so both stay as they were.
`IccGunshipCannonsFrozen` publishes `gsb.phase`: `frozen` while Below Zero holds the raid's cannons,
else `fight`; the cannon-near trigger and the rocket-jump action both call it.

## Duties

- DPS enter a free friendly cannon once they carry the equipped rocket pack, and leave it when frozen.
- The main tank jumps to the captain only while the cannons are frozen or a non-tank is on the enemy
  deck, taunts him when it isn't his victim, drags him 40 yd from its own cannon, and jumps back last.
- The assist tank holds the friendly deck and collects boarders within 20 yd of the wait point.
- Non-tanks board to kill the freezing mage under skull, then return; at home the ranged keep star on
  the enemy axethrower or rifleman nearest the last star.

## Falling below the deck

A bot more than 5 yd under its own deck point is put back there with `NearTeleportTo`, which keeps
combat (`TeleportTo` dropped it), and off the transport: its stored offset is where it fell, and
PlayerbotAI's 1 s transport check boards it again at the new spot. The core's own rescue
(Check for Players) covers deeper falls a few seconds later, so the bot snap is a borderline cheat,
documented and not wrapped, like Valithria's floor snaps.

## Cheats

Kept, `raid` gate: `gsb.battlefury`, the main tank strips Battle Fury, the 69637 proc aura included,
off the captain. The captain recasts 69637 only on a new engage, so with the raid cheat on Battle Fury
is gone after the first strip. The code calls the core bugged; the script shows no aggro defect
(above), so no issue is filed. A legit answer is a stack threshold that sends the main tank home
until the stacks fall off.

## Taunt callers

The class taunt no longer fixates or resets its cooldown ([README.md](README.md#cheats)):

- The assist tank collects boarders one per taunt cooldown; the rest go to whoever they hit until the
  tank's threat takes them.
- The main tank's captain taunt holds him for 3 s, then real threat decides. Every player starts at
  zero threat on him, so the tank keeps him by attacking.

## Known gaps

- Raid-icon targeting (README Known gaps).
- `gsb.battlefury`: a server with the raid cheat off fights Battle Fury with no stack threshold.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `gsb.phase` | per bot | `fight`, `frozen` |
| `gsb.deck` | per bot | Where the rocket-jump rule puts the bot: `friendly`, `enemy`, `between`, `cannon` (only on ticks its cannon was cooling down) |
| `gsb.skull` | per non-tank | `mage` while it keeps skull on the freezing mage, `none` |
| `gsb.star` | per ranged | On each star move, `handoff <guid>` (nearest the last star), `fresh <guid>` (no star yet), `none` |

Cheat key: `gsb.battlefury`. `tools/botobs/bosses/icecrown_gunship_battle.py`: `--phases`, `--decks`
(time per deck value and trips to the enemy deck), `--icons`, `--cheats`.
