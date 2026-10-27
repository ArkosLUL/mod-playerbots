# Sindragosa

Boss id 11, trace slug `sindragosa`, prefix `sg.` ([README](README.md)). Code: `Action/ICCActions_SG.cpp`
and the Sindragosa regions of the shared ICC files. Mechanics: `boss_sindragosa.cpp`. Her gauntlet
is the last section.

## Mechanics

- She flies in once Rimefang and Spinestalker die, lands and pulls the zone; `IN_PROGRESS` from her
  `JustEngagedWith`. Berserk at 10 min.
- **Ground**: Cleave, Tail Smash, Frost Breath (row 1885), Unchained Magic every 30-35 s (Instability
  stacks), Icy Grip (first at 33.5 s) pulling everyone in for Blistering Cold 1 s later (row 1882:
  70123/71047/71048/71049, a 5 s cast). Frost Aura is row 1883.
- **Air phase** at 50 s, then every 110 s. She goes passive and takes off 20 yd, flies to the near
  air station (4476, 2484) and puts Frost Beacon 70126 on 2/5/2/6 players (10N/25N/10H/25H); after its
  7 s each becomes Ice Tomb 70157, a stun and an Ice Tomb 36980 to kill. She then holds the far
  station, `ICC_SINDRAGOSA_FLYING_POSITION` (4526, 2485), 49.6 yd on, for four Frost Bombs (row 1884),
  7 s then every 6 s, each target marked 5.5 s by Frost Bomb Targeting Visual 70022 on a 37186. She
  flies back 5.5 s after the fourth and turns aggressive again at touchdown.
- **Last phase** once her health is no longer above 35%, latched; reached in the air, it starts at
  touchdown. No more air phases. Mystic Buffet 70128 stacks 70127 (row 2193) on everyone, 8 s
  refreshed; every 18-22 s, first 7-10 s, Frost Beacon and Ice Tomb on one random player within 80 yd
  who is not her tank.

## Phase

`IccSindragosaPhase(bot, boss)` reads her script's own state and publishes `sg.phase`: `air` while
in combat and passive (takeoff to touchdown), `last` at 35% and under on the ground, else `ground`.
Every phase test calls it. `IccSindragosaOverAirStation(bot, boss, radius)` is the air phase within
30 yd of the far station (the bombs) or 50 yd (both stations, from the beacons on). It replaced
coordinate tests that also carried health bands (not under 25%, not over 99%) the script never
uses.

## Tactics

- **Tanks** hold `ICC_SINDRAGOSA_TANK_POSITION`. The main tank taunts her back from anyone else, one
  taunt per 8 s cooldown (README, "The class taunt"), drags her toward the centre once she is 16 yd
  off it, and steps round her until she faces east. Tanks use Nitro Boosts over the air stations.
- **Ground**: ranged at `ICC_SINDRAGOSA_RANGED_POSITION`, melee and healers 15 yd from her toward the
  middle of the melee and ranged spots, clear of the tail. Icons: skull on an entombed tank's tomb
  first; with no tomb, skull on her; else skull, cross and star on up to three tombs, kept across
  the air phase, each DPS keeping its air group's mark.
- **Blistering Cold**: everyone but the main tank within 33 yd steps 15 yd toward
  `ICC_SINDRAGOSA_BLISTERING_COLD_POSITION` with Nitro Boosts; ranged and healers already out hold
  still. Unchained Magic stops casting at 6 Instability on normal and holds all but movement on
  heroic; Chilled to the Bone at 6 stops attacks.
- **Frost Beacon**: beaconed bots take the tomb spots `THOMB1-3` by beacon count (2: first and last;
  5: two, one, two; 6: two each; else index mod 3), in the last phase `THOMBMB2`; healers keep a HoT
  on them; paladins Hand of Freedom a tank under Frost Breath. Others hold `FBOMB` while she is over
  the far station, else their ground spot. Smoke flares mark the spots for humans: red on `THOMB1-3`
  once before the last phase, blue on `THOMBMB2` once in it; a flare burns 5 min.
- **Air phase hide**: 3 groups on 25-man, 2 on 10-man (`sg.group`, kept for the pull; an entombed bot
  joins its tomb's group). Each owns a tomb zone and hides 6.5 yd behind its sticky tomb (`sg.tomb`),
  away from the bomb marker. Its icon (skull, cross, star) goes on an extra tomb, else the sticky one
  down to 40% (60% on 10-man), then the group stops attacking. Without a bomb marker or a tomb, bots
  stop attacking and wait.
- **Last phase**: with a tomb up and no beacon out, everyone but tanks and her victim hides at
  `ICC_SINDRAGOSA_LOS2_POSITION`; a lone tomb is burned to 50%, then held until 60% of the raid has
  shed Mystic Buffet, then killed. Between tombs non-tanks hold their spread spots. Healers stay
  pinned at LOS2; with a beacon out only ranged DPS attack.

## Cheats

Removed: `sg.nitro`, now the bot's own Nitro Boosts through `UseNitroBoosts`: tanks over the air
stations, the last phase beacon walk, and the Blistering Cold run.

Kept, behind the raid cheat:

| Key | Does | A legit answer needs |
|---|---|---|
| `sg.frostbeacon` | Strips a last phase Frost Beacon off the main tank | A tank swap: the assist tank taunts and the main tank takes the beacon |
| `sg.mysticbuffet` | Strips Mystic Buffet off tanks | A tank swap on stacks |
| `sg.magicbarrier` | Magic Barrier 38112 on tanks, immune to every school, while a tomb is up, Blistering Cold casts or a last phase beacon is out | The main tank running Blistering Cold too, and tank cooldowns on the tomb hits |
| `sg.nothreat` | 1% Threat on assist tanks while a main tank lives | A threat brake on the assist tank |
| `sg.flares` | Smoke flares from `StoreNewItemInBestSlots`, as with Mimiron's core | Bots carrying flares |
| `sg.tombstrip` | Strips a held tomb's auras: under 30% in the air, at the last phase hold | Stopping DoTs before the hold threshold |

Record-only, `EnableICCBuffs`: `sg.heroicbuff` and `sg.spitefulfury` on every member on heroic
(README).

## Known gaps

- Icons by raid icon (README); `sg.skull`, `sg.cross` and `sg.star` record every move.
- The red flares latch until the last phase, so an air phase more than 5 min after the first finds
  them burnt out. Only humans read them.
- No Sindragosa node strips the heroic buffs, so humans keep Experienced and 1% Threat until a later
  boss strips them.
- Beacon spots are not probed: they follow from the beacon count and guid order, and the moves show
  them.
- No ICC trace yet carries these probes, so nothing here is measured.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `sg.phase` | per bot | `ground`, `air`, `last` |
| `sg.group` | traced map, bot → group | 0-2, kept for the pull |
| `sg.tomb` | traced map, bot → tomb | The sticky tomb it hides behind |
| `sg.freedfallback` | traced set | A bot freed from a tomb, until its zone has a tomb again |
| `sg.losmove` | per bot | `hide`, `parked`, `replay` (finishing a walk whose tomb went), `zone` (no tomb in its zone), `fallback`, `tombed`, `wait` (no bomb or tomb), `lost`; `none` once she leaves the stations |
| `sg.skull`, `sg.cross`, `sg.star` | the bot that moved the icon | Rule and unit, or `clear`: ground `tanktomb`, `boss`, `tomb`; air `extra`, `sticky`; last phase `tomb`, `preburn`, `hold`, `kill` |
| `sg.flaredred` | the bot that flared | Spots flared red (0-2, 1 is 25-man only), `none` once cleared |
| `sg.flaredblue` | traced value | The last phase flare is down |

Change-only per bot, so a bot repeating its own last value writes nothing.

`tools/botobs/bosses/sindragosa.py`; its banner names declared keys the pull never wrote, sparing
`sg.star` on 10-man:

- `--phases`: the `sg.phase` steps;
- `--tombs`: each bot's last group, tomb picks, freed bots, and `sg.losmove` counts per air phase;
- `--icons`: every skull, cross and star move;
- `--flares`: the red and blue flare notes;
- `--cheats`: this encounter's keys.

## Sindragosa gauntlet

Boss id 13, trace slug `sindragosa-gauntlet`, prefix `sg.`, shared with Sindragosa.
`IN_PROGRESS` from the controller's `DoAction(ACTION_START_GAUNTLET)`, `DONE` when it dies after the
third wave, `NOT_STARTED` on its reset; it evades once no living player is within 100 yd. Three waves,
each once only Nerub'ar Broodlings 37232 are left: Nerub'ar Champions 37501 and Webweavers 37502 drop
from the webs, Frostwarden Warriors 37228 and Sorceresses 37229 walk in, then fresh spiders drop.
Broodlings come with every wave.

No ICC node runs it, so it has no probe and no cheat: every `sg.*` key is Sindragosa's.
`tools/botobs/bosses/sindragosa_gauntlet.py`: `--waves` counts each kind of add with its first and
last sample, spiders split at the first Frostwarden; `--cheats` lists any use, none of them the
gauntlet's.
