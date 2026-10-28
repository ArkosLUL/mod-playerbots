# Rotface

Boss id 5, slug `rotface`, prefix `rf.`. Code: stem `Rotface` (`src/Ai/Raid/ICC/*/ICC*_Rotface.*`); nodes
`icc rotface` dodge, tank defensive, dispel, ooze taunt, kite, carrier, target, stand; multipliers `icc
rotface ooze splash`, `icc rotface self cure`. Raid conventions: [README.md](README.md). Core: `boss_rotface.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; the Spell constructor remaps a cast, so code matches the base
id through `GetSpellIdForDifficulty`. Every area below is a creature's or a player's on players: centre to
centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Mutated Infection 69674/71224/73022/73023 | From 14 s on one random player but his victim, every 14 s; 25-man only, 2 s shorter every 90 s down to 6 s at 6:00. Disease, 12 s, 3.4-3.6k (10N), 3.9-4.1k (25N, 10H), 4.9-5.1k (25H) a second, healing taken -50% (-75% heroic). Its removal, by expiry, cure or death, summons a Little Ooze on the spot | Bring the ooze to the Big Ooze; hold the first cure, cure the second at once beside the first ooze, later ones beside the Big Ooze | Carrier walk (`rf.carrier`) and a cure duty (`rf.dispel`) |
| Little Ooze 36897 | 500k threat on its player, taunt-immune, 8 yd/s. Weak Radiating Ooze 10 yd every 2 s; Sticky Ooze on its victim 5 s in, then every 15 s. Merges with another Little Ooze within 8 yd (every 0.5 s) into a Big Ooze | Move it out of the raid | Carrier walk; area damage held near it |
| Big Ooze 36899 (25: 38123) | 4 yd/s. Opens on the nearest player, then threat only. Radiating Ooze 10 yd every 2 s; absorbs a Little Ooze (+1) or a Big Ooze (stacks added) within 8 yd: Unstable Ooze 69558, +20% a stack. Taunt diminishing returns (0x80000); immune to snares, stuns, roots (-361) | Off-tank grabs every ooze, builds threat while it is weak, kites it later; brings a second Big Ooze to the first | The kiter (`rf.kiter`, `rf.bigooze`) |
| Unstable Ooze Explosion 69839 | At 5 stacks, 4 s cast. At its end a stalker 38107 marks where every player and pet stands, and a missile flies there from the ooze at 5 yd/s: 9.8-10.3k (10N), 11.7-12.3k (25N, 10H), 14.6-15.4k (25H) within 6 yd. The ooze turns invisible 2 s later | Everyone steps 6+ yd at the cast's end, never onto another's spot; melee to the other leg | Dodge clear of every mark until it lands |
| Sticky Ooze 37006 | A 30 s puddle: 2.9-3.1k (10), 3.9-4.1k (25) a second within 6 yd, -50% speed | Avoid | Dodge; slots and kite legs keep clear |
| Slime Spray 69508 | From 20 s, every 20 s, at a random non-tank: a stalker 37986 at the target, 1.5 s cast, 5 s channel. Each second 69507/71213/73189/73190 hits a 25° cone (10N, `spell_cone`) or 24° (others) at any range, and anyone within 2 yd of him | Melee run through him; ranged step aside | Dodge out of the cone |
| Ooze Flood | From 8 s, every 25 s, one quadrant: random first, then counter-clockwise. Its two ground stalkers 37013 carry 69783/69797/69799/69802 for 5 s, then 69788 for 20 s, each casting 69789/71215/71587/71588 every second within 25 yd: 4.4-4.6k (10N), 5.4-5.6k (25N, 10H), 6.3-6.7k (25H), slowed. Never within 13.9 yd of his spawn | Avoid the wet quadrant | Slots there swap at the warning; dodge; kite legs stay dry (`rf.flood`) |
| Vile Gas, heroic | Putricide casts 69240 (73019/73020) from 15-20 s, every 15-20 s, triggered, at a random player more than 14.5 yd from him when 3 (10) or 8 (25) stand there, else anyone. 8 yd round the victim at cast; 6 s: damage, -50% speed and confusion (69248), and every carrier hits allies in 8 yd every 2 s | Ranged and healers 8 yd apart | Spread slots; others step out of 8 yd round a carrier |

He has 8 yd combat reach, so his melee reaches 10.8 yd and a hunter's minimum range against him 15.8 yd. He
follows only a tank more than 10.8 yd away. No phase, no Berserk in this script. His boundary is x 4385..4505, y
3082..3195. His state reaches `IN_PROGRESS` only at Putricide's balcony point, seconds in: every node, rule
and the tick key on him being in combat, the splash multiplier waits for live.

## Duties

- **Main tank**: the bot tank he hits holds a stand 5 yd east of his spawn (4445.87, 3137.31). Once he is
  4.5 yd off it, the stand moves 9 yd past the spawn on the far side from him until he is back within 2.5.
- **Kiter** (`rf.kiter`): one bot tank, not the one holding him, an assist tank first, kept while it can.
  Its duty (`rf.bigooze`): with carriers out and no Big Ooze it waits at the ooze spot; it taunts a Big
  Ooze not on it from 30 yd (`icc rotface ooze taunt`, never at the immune level of taunt diminishing
  returns) and walks to it; it holds one under 3 stacks at the ooze spot while within 15 yd of it, else
  kites it: once the ooze is within 15 yd, a leg 10-40 degrees along the kite ring (36 yd),
  counter-clockwise first, to a point 16 yd from the ooze, dry by 28 yd, clear of puddles, its walk not
  through the ooze's aura or a flood. It holds while it carries an ooze itself. Towing its ooze, it never
  dodges within 32.5 yd of his spawn. Generic taunts are blocked for it.
- **Ooze spot** (`rf.oozespot`): the kite ring point at the middle of the quadrant opposite the latest
  flood, so it moves on a quadrant with each flood and never floods while it is the spot.
- **Carriers** (`rf.carrier`): a bot with Mutated Infection or a Little Ooze on it walks to the Big Ooze
  (the kiter's first), else another Little Ooze, else the ooze spot, and stops within 4 yd.
- **Cure duty** (`rf.dispel`): an infected player within 6 yd of the Big Ooze, or of another Little Ooze,
  or of another infected player with neither up, or under 35%, gets the nearest bot priest, paladin or
  shaman with a disease cure ready within 30 yd and in sight. The class cure nodes, Stoneform and Cleansing
  Totem are blocked; a paladin's own Cleanse and Purify through `icc rotface self cure`, since they report
  `cleanse` and `purify`, not the names they register under.
- **Spread** (`rf.slot`): healers on the inner ring (12 yd, 6 slots), ranged on the outer (21.5 yd, 13),
  each on the other ring when theirs is full, hunters outer only, the rest the stack 8 yd behind him. A
  healer's first slot leaves the widest gap to the other healers, so with two or more one always reaches
  the kiter (inner slots 90 degrees off it are 38 yd away).
  Every slot is 9.5 yd or more from every other. A slot within 1.75 yd of a flood or puddle or 11.75 of a
  Big Ooze changes for the nearest usable one. Reach, heal reach and spell movers are held while a stand
  holds.
- **Dodge** (`rf.dodge`, emergency, forced): within half a yard of a hazard (Slime Spray cone, a mark not
  yet landed, a puddle, a flood or its warning, a Vile Gas carrier for others, a Big Ooze for all but the
  kiter and carriers), to the nearest spot clear of all of them by 2.5 yd, 0.75 when nothing is, its walk
  across none, the cone cleared by 2 yd; melee and the main tank within 9 yd of him first. A cast is cut
  for the spray, a mark landing within 2.5 s, a puddle, a gas carrier, a flood or its last 1.5 s of
  warning. A stand
  walks back only once no hazard covers its spot; generic movers are held while a mark flies or he sprays.
- **Targets** (`rf.target`): the kiter's Big Ooze, then a player's skull on him or an ooze for all but the
  tank holding him, else him. Area damage from a damage dealer is held while an ooze is within 15 yd of it
  or its target. Army of the Dead (ghouls taunt the oozes) and Death Grip (all three are grip immune) are
  held.
- **Tank defensives** (`rf.defensive`): the kiter under a Big Ooze of 4 stacks within 6 yd, any tank under
  40%; class buttons held to 25%. A human tank gets the same as calls (`rf.humancall`).

Every stand is computed round his spawn, bearings counter-clockwise from east: slot n of 1-6 at
30 + 60(n-1) degrees on the inner ring, of 7-19 at 13.85 + 27.69(n-7) on the outer, the stack at 180, the
tank spot at 0, the ooze spots at 45 + 90q on the kite ring, kite points every 10 degrees. All are
navprobe-clean (`--nav 0x09`, settled z 360.385, in sight of his spawn): the rings at 5, 8, 9 (drag), 12,
21.5 and 36 yd, and paths from the outer ring to the kite ring and across the room. Gameobject collision
isn't in navprobe's sight. A flood takes 4-5 outer slots; inner slot 3 sits 1.96 yd outside the north-west
flood.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Spread on two rings, 9.5 yd apart, on every difficulty | Warcraft Tavern: everyone 8 yd apart; Vile Gas 8 yd plus park tolerance | Wowhead normal: stack behind him |
| Healers inside 14.5 yd, ranged outside | Vile Gas picks outside while 3 / 8 stand there, so healers aren't its targets | One ring |
| Kite ring 36 yd, counter-clockwise first | 14.5 yd past the outer ring; a flood moves counter-clockwise, so a kiter behind it is never flooded onto | Kite ring 24-34 yd; fixed points |
| Hold the Big Ooze at the ooze spot under 3 stacks, from within 15 yd | Wowhead: threat lead while it is weak, kite once stacked; a straight walk to a far spot drags its aura through the raid | Kite from the pickup |
| Ooze spot opposite the latest flood | It never floods while it is the spot; one held into its own warning leaves the way out through the raid | Hold it until it floods, then jump across the room |
| Healers' first slots spread round the inner ring | Kiter in heal reach at any bearing with two healers | Nearest slot |
| Taunt only off the kiter, never at the immune level | 0x80000 on the ooze | Taunt every ooze each cooldown |
| Cure at the merge, the first not at all, any under 35% | Wowhead's three-step order; -50/75% healing on a 12 s DoT | Never cure; cure at once |
| Explosion: dodge every mark until its missile lands | The script: marks at cast end, 5 yd/s flight | 25 slots on a 40 yd ring |
| Slime Spray: step out of the cone | The script's cone and 2 yd boundary; the old stack onto his centre was inside it | Run through him |
| Vile Gas: spread, others step away | Its carrier is confused for 6 s | Carrier runs 40 yd out |
| Main tank drags him back past 4.5 yd | His 10.8 yd reach keeps him where the pull left him | No drag |
| Only a second bot tank kites | Humans decision; the main tank holds him | Main tank takes over |
| The holder and the kiter ignore a player's skull | Each must keep its unit; Saurfang's holder does the same | The skull wins |
| No opening positions | 8 s to the first flood, 14 s to the first infection | Pre-pull stands |

## Cheats

None before, none now: `rf.nitro` and `rf.spitefulfury` were removed earlier. The skull icon write is gone.

## Known gaps

- The kiter keeps a kited Big Ooze by its threat and instants alone; heal threat can still pull it. The
  taunt hits the immune level after four taunts inside 15 s of each other. `--oozes` gives the share on a
  tank.
- No kite plan through a flood: with no dry leg, nor a dodge spot 32.5 yd out, the kiter holds and tanks
  the ooze where it stands. No Hand of Freedom or Anti-Magic Shell for a slowed kiter.
- Curers never walk: in 10-man a merge on the kite ring can sit beyond 30 yd of every curer, and the
  carrier then runs its 12 s out. With one healer, or the healers dead, the far side is out of heal reach
  too.
- A Cleansing Totem already down or dropped by Call of the Elements still cures (only with the opt-in
  `cleansing` water totem strategy).
- A class rotation casting at the ooze can turn the kiter mid-leg.
- Overflow ranged and healers stand in the stack; overflow hunters get no stand.
- A carrier crossing the raid splashes it (Weak Radiating Ooze); nobody steps away from a Little Ooze.
- A mark beside the kiter lands too soon to clear. On heroic the kiter, 36 yd out, is a Vile Gas target,
  and 6 s of confusion lets the ooze catch it.
- With one bot tank no one kites. Humans hold no duty, but get cured and their gas avoided.
- No 631 trace from this build yet, and never built.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `rf.target` | per bot | The resolver's rule: `skull`, `ooze`, `boss` |
| `rf.kiter` | raid | The elected kiter's guid, `0` none |
| `rf.bigooze` | kiter | `none`, `spot`, `pickup <ooze>`, `tank <ooze>`, `kite <ooze>`, `hold <ooze>` |
| `rf.carrier` | per bot | Merge walk to `ooze`, `little`, `spot`; `none` |
| `rf.dispel` | infected → bot | The cure duty; `0` released |
| `rf.slot` | per bot | 1-6 inner, 7-19 outer, 20 the stack; `0` released |
| `rf.flood` | raid | The flooding quadrant counter-clockwise from east, 1-4; `0` none |
| `rf.oozespot` | raid | The ooze spot's quadrant, 1-4; `0` no merge pending |
| `rf.dodge` | per bot | `<hazard> <branch> <yards>`, branch `move`, `tight` or `cross`; `hold`, `<hazard> none`, `<hazard> locked`, `pinned` |
| `rf.defensive` | per tank | The ladder's pick, `idle` outside a window |
| `rf.humancall` | per sender | The button a human tank was told to press |
| `rf.vilegas` | event | Heroic: the victim of each cast, `txt` the spell id |

`tools/botobs/bosses/rotface.py`; its banner names declared keys the pull never wrote, sparing Vile Gas
on normal: `--oozes` (Big Ooze time on target by role, the kiter, its duty), `--carriers` (merge walks, cure
duties), `--dodges` (dodges by hazard and branch, per bot), `--spread` (floods, ooze spot, slots),
`--vilegas`, `--cheats`.

Next pull, 25N: the Big Oozes over 90% of their time on the kiter, every cure duty landing within 6 yd of
the merge, no `<hazard> none` dodge, no explosion mark hitting a second player, no slot holder hit by a
flood. 25H: no Vile Gas victim within 8 yd of another, the infected alive to the merge.
