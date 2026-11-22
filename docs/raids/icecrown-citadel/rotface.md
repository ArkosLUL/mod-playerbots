# Rotface

Boss id 5, slug `rotface`, prefix `rf.`. Code: stem `Rotface` (`src/Ai/Raid/ICC/*/ICC*_Rotface.*`); nodes
`icc rotface` dodge, freedom, totem, tank defensive, dispel, ooze taunt, kite, carrier, cure walk, target, stand;
multipliers `icc rotface ooze splash`, `icc rotface self cure`. Raid conventions: [README.md](README.md). Core:
`boss_rotface.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; the Spell constructor remaps a cast, so code matches the base
id through `GetSpellIdForDifficulty`. Every area below is a creature's or a player's on players: centre to
centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Mutated Infection 69674/71224/73022/73023 | From 14 s on one random player but his victim, every 14 s; 25-man only, 2 s shorter every 90 s down to 6 s at 6:00. Disease, 12 s, 3.4-3.6k (10N), 3.9-4.1k (25N, 10H), 4.9-5.1k (25H) a second, healing taken -50% (-75% heroic). Its removal, by expiry, cure or death, summons a Little Ooze on the spot | Bring the ooze to the Big Ooze; hold the first cure, cure the second at once beside the first ooze, later ones beside the Big Ooze | Carrier walk (`rf.carrier`), a cure duty (`rf.dispel`), a walk to the carrier with no curer in reach (`rf.curewalk`) |
| Little Ooze 36897 | 500k threat on its player, taunt-immune, 8 yd/s. Weak Radiating Ooze 10 yd every 2 s; Sticky Ooze on its victim 5 s in, then every 15 s. Merges with another Little Ooze within 8 yd (every 0.5 s) into a Big Ooze | Move it out of the raid | Carrier walk; area damage held near it; all but carriers and the kiter keep 10 yd off |
| Big Ooze 36899 (25: 38123) | 4 yd/s. Opens on the nearest player, then threat only. Swings average 31k (10) and 54k (25) before armor (DamageModifier 88.2 and 152.5, level 82). Radiating Ooze 10 yd every 2 s; absorbs a Little Ooze (+1) or a Big Ooze (stacks added) within 8 yd: Unstable Ooze 69558, +20% a stack. Taunt diminishing returns (0x80000); immune to snares, stuns, roots (-361) | Off-tank grabs every ooze, builds threat while it is weak, kites it later; brings a second Big Ooze to the first | The kiter (`rf.kiter`, `rf.bigooze`) |
| Unstable Ooze Explosion 69839 | At 5 stacks, 4 s cast. At its end a stalker 38107 marks where every player and pet stands, and a missile flies there from the ooze at 5 yd/s: 9.8-10.3k (10N), 11.7-12.3k (25N, 10H), 14.6-15.4k (25H) within 6 yd. The ooze turns invisible 2 s later and lingers so for a minute; the bots' scan skips it | Everyone steps 6+ yd at the cast's end, never onto another's spot; melee to the other leg | Everyone, kiter and carriers included, keeps 12.5 yd off it through the cast; then a dodge clear of every mark until it lands |
| Sticky Ooze 37006 | A 30 s puddle: 2.9-3.1k (10), 3.9-4.1k (25) a second within 6 yd, -50% speed with no mechanic, so no freedom clears it | Avoid | Dodge; slots and kite legs keep clear |
| Slime Spray 69508 | From 20 s, every 20 s, at a random non-tank: a stalker 37986 at the target, 1.5 s cast, 5 s channel. He casts on himself and faces the stalker, so it is the cone's axis. Each second 69507/71213/73189/73190 hits a 25° cone (10N, `spell_cone`) or 24° (others) at any range, and anyone within 2 yd of him | Melee run through him; ranged step aside | Dodge out of the cone |
| Ooze Flood | From 8 s, every 25 s, one quadrant: random first, then counter-clockwise. Its two ground stalkers 37013 carry 69783/69797/69799/69802 for 5 s, then 69788 for 20 s, each casting 69789/71215/71587/71588 every second within 25 yd: 4.4-4.6k (10N), 5.4-5.6k (25N, 10H), 6.3-6.7k (25H), a snare of -25% (normal) or -10% a stack to 10 (heroic). Never within 13.9 yd of his spawn | Avoid the wet quadrant; the kiter crosses it, freed by Hand of Freedom or Anti-Magic Shell. Warcraft Tavern's "no slow on normal" is wrong: 69789 and 71215 slow | Slots there swap at the warning; dodge; kite legs stay dry and wade only with no dry leg left; the freedom duty (`rf.freedom`); `rf.flood` |
| Vile Gas, heroic | Putricide casts 69240 (73019/73020) from 15-20 s, every 15-20 s, triggered, at a random player more than 14.5 yd from him when 3 (10) or 8 (25) stand there, else anyone. 8 yd round the victim at cast; 6 s: damage, -50% speed and confusion (69248), and every carrier hits allies in 8 yd every 2 s | Ranged and healers 8 yd apart | Spread slots; others step out of 8 yd round a carrier; a gassed kiter with its ooze within 20 yd spends a defensive |

Sticky Ooze's and the flood's slows are shadow-nature, so Anti-Magic Shell keeps both off. He has 8 yd
combat reach, so his melee reaches 10.8 yd and a hunter's minimum range against him 15.8 yd. He follows only a
tank more than 10.8 yd away. No phase, no Berserk in this script. His boundary is x 4385..4505, y 3082..3195.
His state reaches `IN_PROGRESS` only at Putricide's balcony point, seconds in: every node, rule and the tick key
on him being in combat, the splash multiplier waits for live.

## Duties

- **Main tank**: the bot tank he hits holds a stand 5 yd east of his spawn (4445.87, 3137.31). Once he is
  4.5 yd off it, the stand moves 9 yd past the spawn on the far side from him until he is back within 2.5.
- **Kiter** (`rf.kiter`): one bot tank, not the one holding him, an assist tank first, kept while it can;
  with none, a Death Knight, then a paladin damage dealer, kept until a tank other than the main tank is
  free (the main tank is free whenever he drops his victim for a moment, and would take the duty for one
  refresh). Its duty (`rf.bigooze`): with carriers out and no Big Ooze it waits at the ooze spot; it taunts a
  Big Ooze not on it from 30 yd (`icc rotface ooze taunt`, never at the immune level of taunt diminishing
  returns) and walks to it; it holds one under 3 stacks at the ooze spot while within 15 yd of it, else
  kites it: once the ooze is within 15 yd, a leg of 20, 30, 10 or 40 degrees in that order (20 degrees is
  12.5 yd) along the kite ring (36 yd), counter-clockwise first, to a point 16 yd from the ooze, dry by 28 yd, clear of puddles, its walk not
  through the ooze's aura or a flood. With no dry leg either way it wades (`wade`), the longest leg first;
  floods leave its dodge, and one under it moves it on whatever the ooze does. It drops a cast that pins it
  once the ooze is within 10 yd or it stands wet. It holds while it carries an ooze itself. Towing its ooze
  or off its bursting one, it never dodges within 32.5 yd of his spawn. Generic taunts are blocked for it.
- **Damage dealer kiter**: never stands for a Big Ooze (no `tank`, no `hold`). It waits 30 degrees
  counter-clockwise of the ooze spot, walks to 20 yd of an ooze it has to taunt from past 25 yd, is cured at
  once while it tows, and with a Little Ooze on it lets the Big Ooze to 9 yd so the two merge behind it.
- **Freedom** (`rf.freedom`): a towing kiter with a snare or root and its ooze within 20 yd casts its own
  Hand of Freedom, Every Man for Himself or Escape Artist, else the nearest paladin bot within 30 yd and in
  sight casts Hand of Freedom on it. A Death Knight kiter in a ticking flood, not its warning, casts
  Anti-Magic Shell.
- **Ooze spot** (`rf.oozespot`): the kite ring point at the middle of the quadrant opposite the latest
  flood, so it moves on a quadrant with each flood and never floods while it is the spot; the south-west
  one before the first flood. At a handover the old flood's last tick can overlap the next warning, so
  the warned quadrant counts (`rf.flood` too).
- **Carriers** (`rf.carrier`): a bot with Mutated Infection or a Little Ooze on it walks to the Big Ooze
  (the kiter's first), else another Little Ooze, else the ooze spot, and stops within 4 yd.
- **Cure duty** (`rf.dispel`): an infected player within 6 yd of the Big Ooze, or of another Little Ooze,
  or of another infected player with neither up, or under 35%, gets the nearest bot priest, paladin or
  shaman with a disease cure ready within 30 yd and in sight. With none, the nearest one with a cure ready
  that holds neither him nor the kite and carries nothing walks to 25 yd of the carrier on its own side,
  swung 20 degrees at a time until clear of every hazard, its stand released (`rf.curewalk`). The class
  cure nodes, Stoneform and Cleansing Totem are blocked; a paladin's own Cleanse and Purify through `icc
  rotface self cure`, since they report `cleanse` and `purify`, not the names they register under. A
  shaman bot with a Cleansing Totem down replaces it with Mana Spring, else Healing Stream (`icc rotface
  totem`).
- **Spread** (`rf.slot`): healers on the inner ring (12 yd, 6 slots), ranged on the outer (21.5 yd, 13),
  each on the other ring when theirs is full, hunters outer only, the rest the stack 8 yd behind him. A
  healer's first slot leaves the widest gap to the other healers, so with two or more one always reaches
  the kiter (inner slots 90 degrees off it are 38 yd away). A lone bot healer takes the inner slot nearest
  the kiter, again once its own is 30 yd from it.
  Every slot is 9.5 yd or more from every other. A slot within 1.75 yd of a flood or puddle or 11.75 of a
  Big Ooze changes for the nearest usable one. Reach, heal reach and spell movers are held while a stand
  holds.
- **Dodge** (`rf.dodge`, emergency, forced): within half a yard of a hazard (Slime Spray cone, a mark not
  yet landed, a puddle, a flood or its warning but for a towing kiter, a Vile Gas carrier for others, a Big
  Ooze casting its explosion for all by 12.5 yd, any other Big or Little Ooze for all but the kiter and
  carriers), to the nearest spot clear of all of them by 2.5 yd, 0.75 when nothing is, its walk across none,
  the cone cleared by 2 yd; melee and the main tank within 9 yd of him first. A cast is cut for the spray, a
  bursting ooze, a mark landing within 2.5 s, a puddle, a gas carrier, a flood or its last 1.5 s of warning.
  A stand walks back only once no hazard covers its spot; generic movers are held while a mark flies or he
  sprays.
- **Targets** (`rf.target`): the kiter's Big Ooze, then a player's skull on him or an ooze for all but the
  tank holding him, else him. Area damage from a damage dealer but the kiter is held while an ooze is within
  15 yd of it or its target. Army of the Dead (ghouls taunt the oozes) and Death Grip (all three are grip
  immune) are held. The kiter's pet attacks him, never the kited ooze: there it would pull the ooze off a
  kiter that can't reach it, and die to its swings.
- **Tank defensives** (`rf.defensive`): a tank kiter under a Big Ooze of 4 stacks within 6 yd, a damage
  dealer kiter under any within 6 yd (its physical buttons), the kiter gassed with its ooze within 20 yd,
  any tank under 40%; class buttons held to 25%. A human tank gets the same as calls (`rf.humancall`).

Every stand is computed round his spawn, bearings counter-clockwise from east: slot n of 1-6 at
30 + 60(n-1) degrees on the inner ring, of 7-19 at 13.85 + 27.69(n-7) on the outer, the stack at 180, the
tank spot at 0, the ooze spots at 45 + 90q on the kite ring, the damage dealer kiter's wait at 75 + 90q,
kite points every 10 degrees. The floor is navprobe-clean (`--nav 0x09`, settled z 360.385, in sight of his
spawn) on rings 3 yd or less apart from 2 to 41 yd at 72 headings, as are paths from the outer ring to the
kite ring and across the room, and 2211 chords between ring points are clear: every computed spot (cure
walks, pick-up stand-offs, dodges, all inside 41 yd) is covered.
Gameobject collision isn't in navprobe's sight. A flood takes 4-5 outer slots; inner slot 3 sits 1.96 yd
outside the north-west flood.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Spread on two rings, 9.5 yd apart, on every difficulty | Warcraft Tavern: everyone 8 yd apart; Vile Gas 8 yd plus park tolerance | Wowhead normal: stack behind him |
| Healers inside 14.5 yd, ranged outside | Vile Gas picks outside while 3 / 8 stand there, so healers aren't its targets | One ring |
| Kite ring 36 yd, counter-clockwise first | 14.5 yd past the outer ring; a flood moves counter-clockwise, so a kiter behind it is never flooded onto | Kite ring 24-34 yd; fixed points |
| Wade a flood only with no dry leg either way | Both guides: cross it freed; a held 3-stack ooze hits a tank ~25k a second, a flood 4.4-6.7k | Hold and tank it at the edge; reverse past it (drags the aura onto the outer ring) |
| Floods leave a towing kiter's dodge | The dodge pulled it out of the leg the kite walked again | Dodge and kite both own floods |
| Freedom: the kiter's own first, then the nearest paladin bot; a wading Death Knight's Anti-Magic Shell | Wowhead; 1044 clears snares and roots in 30 yd; 48707 makes it immune to magic auras | Generic paladin self-freedom only |
| Hold the Big Ooze at the ooze spot under 3 stacks, from within 15 yd | Wowhead: threat lead while it is weak, kite once stacked; a straight walk to a far spot drags its aura through the raid | Kite from the pickup |
| Ooze spot opposite the latest flood | It never floods while it is the spot; one held into its own warning leaves the way out through the raid | Hold it until it floods, then jump across the room |
| Healers' first slots spread round the inner ring; a lone one follows the kiter | Kiter in heal reach at any bearing; an inner slot within 30 degrees of a kite ring point is 26.3 yd from it | Nearest slot; fixed slot |
| Taunt only off the kiter, never at the immune level | 0x80000 on the ooze | Taunt every ooze each cooldown |
| No threat redirect onto the kiter | The pickup taunt recovers it; Misdirection's shots on him would hand the kiter his threat | Misdirection on each pickup |
| Cure at the merge, the first not at all, any under 35% | Wowhead's three-step order; -50/75% healing on a 12 s DoT | Never cure; cure at once |
| A curer walks to a carrier no curer reaches; never the holder, the kiter or a carrier | Cures reach 30 yd; a far merge sits beyond every 10-man curer | The infection runs its 12 s |
| Cleansing Totem replaced, not only blocked | It strips a disease every 3 s; one already down keeps pulsing | Block Call of the Elements |
| Explosion: everyone 12.5 yd off the casting ooze, then dodge every mark until its missile lands | The script: marks at cast end, 5 yd/s flight, so 12.5 yd leaves 2.5 s for the step | Dodge the marks only (the kiter's lands in ~1 s); 25 slots on a 40 yd ring |
| Slime Spray: step out of the cone | The script's cone and 2 yd boundary; the old stack onto his centre was inside it | Run through him |
| Vile Gas: spread, others step away; a gassed kiter spends a defensive | Its carrier is confused for 6 s, in which the ooze closes 24 yd | Carrier runs 40 yd out |
| Main tank drags him back past 4.5 yd | His 10.8 yd reach keeps him where the pull left him | No drag |
| One bot tank: a Death Knight, then a paladin damage dealer kites, never standing for the ooze | Gap lanes' rule; Dark Command and Hand of Reckoning need no stance or form; a 25-man swing averages 54k | Warrior or cat (Taunt and Growl need Defensive Stance or Bear Form), a hunter (no taunt), the main tank; nobody (old) |
| The holder and the kiter ignore a player's skull | Each must keep its unit; Saurfang's holder does the same | The skull wins |
| A towing kiter keeps 32.5 yd in its last dodge | A puddle or mark on the kiter costs less than its ooze's aura on the outer ring | Drop the keep-out last |
| The kiter's rotation stays free mid-leg | An instant's facing lasts until the spline's next update (`Unit::UpdateSplinePosition`), and `PlayerbotAI::CastSpell` refuses a cast-time spell while moving | Block its casts while walking |
| No opening positions | 8 s to the first flood, 14 s to the first infection | Pre-pull stands |

## Cheats

None before, none now: `rf.nitro` and `rf.spitefulfury` were removed earlier. The skull icon write is gone.

## Known gaps

- The kiter keeps a kited Big Ooze by its threat and instants alone; heal threat can still pull it, a damage
  dealer kiter's sooner (no threat multiplier). The taunt hits the immune level after four taunts inside
  15 s of each other. `--oozes` gives the share on the kiter.
- Sticky Ooze's slow has no mechanic, so only a Death Knight's Anti-Magic Shell keeps it off a kiter. A
  paladin beyond 30 yd never walks to free one. The shell a wading Death Knight spends isn't there for a
  4-stack ooze. A wade can find no leg at all (every point a puddle or past the ooze); the kiter then stands
  in the flood, which its dodge no longer sees. A damage dealer kiter's pick-up walk ignores floods its
  dodge still sees, so the two can trade steps at a flood's edge until the ooze is within 25 yd
  (reviewer): `rf.dodge` `flood` rows between `rf.bigooze` `pickup` rows show it.
- A carrier's walk still crosses the raid; those within 10 yd step off its Little Ooze after the first tick.
- Call of the Elements can drop a Cleansing Totem again; it pulses until the next tick replaces it (only
  with the opt-in `cleansing` water totem strategy).
- Overflow ranged and healers stand in the stack; overflow hunters get no stand: a third ring would sit in
  the kite ring's aura, and 19 slots hold 6 healers and 13 ranged.
- On heroic the kiter, 36 yd out, is a Vile Gas target, and its ooze still catches it in the 6 s of
  confusion; only the defensive answers it.
- With no second bot tank and no Death Knight or paladin damage dealer, no one kites. Humans hold no duty,
  but get cured and their gas avoided.
- Built clean at `623638b99` (2026-10-29), before these duties; no 631 trace yet.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `rf.target` | per bot | The resolver's rule: `skull`, `ooze`, `boss` |
| `rf.kiter` | raid | The elected kiter's guid, `0` none |
| `rf.bigooze` | kiter | `none`, `spot`, `pickup <ooze>`, `tank <ooze>`, `kite <ooze>`, `wade <ooze>`, `hold <ooze>` |
| `rf.freedom` | raid | The bot due to free the kiter, the kiter itself included, `0` none |
| `rf.carrier` | per bot | Merge walk to `ooze`, `little`, `spot`; `none` |
| `rf.dispel` | infected → bot | The cure duty; `0` released |
| `rf.curewalk` | per bot | The carrier a curer walks to, `none` |
| `rf.slot` | per bot | 1-6 inner, 7-19 outer, 20 the stack; `0` released |
| `rf.flood` | raid | The flooding quadrant counter-clockwise from east, 1-4; `0` none |
| `rf.oozespot` | raid | The ooze spot's quadrant, 1-4; `0` no merge pending |
| `rf.dodge` | per bot | `<hazard> <branch> <yards>`, hazard `spray`, `mark`, `sticky`, `flood`, `gas`, `burst`, `ooze`, `little`, branch `move`, `tight` or `cross`; `hold`, `<hazard> none`, `<hazard> locked`, `pinned` |
| `rf.defensive` | per tank or kiter | The ladder's pick, `idle` outside a window |
| `rf.humancall` | per sender | The button a human tank was told to press |
| `rf.vilegas` | event | Heroic: the victim of each cast, `txt` the spell id |

`tools/botobs/bosses/rotface.py`; its banner names declared keys the pull never wrote, sparing Vile Gas
on normal: `--oozes` (Big Ooze time on target by role, the kiter, its duty, its freedom), `--carriers`
(merge walks, cure duties, cure walks), `--dodges` (dodges by hazard and branch, per bot), `--spread`
(floods, ooze spot, slots), `--vilegas`, `--cheats`.

Next pull, 25N: the Big Oozes over 90% of their time on the kiter, every cure duty landing within 6 yd of
the merge and every `rf.curewalk` ending in one, no `<hazard> none` dodge, no explosion mark hitting a
second player, no slot holder hit by a flood, a `wade` only with `rf.freedom` handed out. 25H: no Vile Gas
victim within 8 yd of another, the infected alive to the merge.
