# Deathbringer Saurfang

Boss id 3, slug `deathbringer-saurfang`, prefix `dbs.`. Code: stem `Saurfang`
(`src/Ai/Raid/ICC/*/ICC*_Saurfang.*`); nodes `icc dbs` rune swap, rune yield, beast taunt, beast
control, mark heal, target, stand; multipliers `icc dbs area damage`, `icc dbs beacon duty`. Raid
conventions: [README.md](README.md). Core: `boss_deathbringer_saurfang.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; the Spell constructor remaps a cast, so code matches the
base id through `GetSpellIdForDifficulty`. None has a world DB difficulty row. He and his beasts carry
the default 1.5 yd reach.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Blood Power 72371, Blood Link 72178 | Energy 0-100, +1% damage a point, fed by his spell damage (Blood Nova double) and 3 a Blood Beast hit, every difficulty. At 100: a Mark | Mitigate his damage | `dbs.phase` |
| Rune of Blood 72410 | On his victim from 20 s, then every 20-25 s, 20 s aura. Each swing on it leeches 5.1-6.9k (10) or 5.95-8.05k (25) (72409/72447/72448/72449) and heals him 10× that | The other tank taunts at once | A bot tank free of the rune swaps (`dbs.swap`), the rune tank stops hitting him (`dbs.yield`) |
| Boiling Blood 72385/72441/72442/72443 | From 15.5 s, every 15-20 s: 5k every 3 s for 15 s on 1 (10) or 3 (25) players, never his victim | Heal; Hand of Protection on a marked one | Generic heals |
| Blood Nova 72378 (10), 73058 (25) | 1 s cast from 17 s, every 20-25 s. Victim drawn at random from players more than 12 yd off his edge (15 yd from his centre, 16.5 at full Blood Power), or the farthest 4 (10) / 10 (25) when fewer. 72380/72438/72439/72440: 7.6-8.4k (10) or 9.5-10.5k (25) to everyone within 12 yd of the victim, centre to centre | Ranged and healers 12 yd apart, part of the raid out of melee | 11 slots, 13.5 yd apart; overflow stacks with the melee |
| Mark of the Fallen Champion 72293 | At 100 Blood Power, 1.5 s cast, on a random player other than his victim or the marked; permanent, through death. Each of his swings deals 72255/72444/72445/72446, 4.3-4.7k (10) or 6.2-6.8k (25), to every marked player. A marked death heals him 5% (72260) on every difficulty | A healer per Mark, holy paladins with Beacon first; 20% heal on heroic | Mark heal duty (`dbs.markheal`) |
| Blood Beast 38508 | From 30 s, every 40 s: 2 (10) at his left and right, 5 (25) also back-left, back-right and front, 7 yd out. Rooted 1.5 s, opens on a random player but his victim, then threat; 8 yd/s; Resistant Skin 72723 -95% area damage; open to stun, root, snare, knockback (`creature_immunities` -205) | Ranged focus, the furthest first; melee only those in reach; slows, stuns, knockbacks, no roots; taunt as a last resort; no area damage | Kill split (`dbs.kill`), control duty (`dbs.cc`), the free tank's taunt, melee in reach |
| Scent of Blood 72769 (heroic) | 10 s after each wave: the raid slowed 80% for 10 s, the beasts +300% damage (72771) | Kill them inside 10 s | A beast 9 s old on heroic tops the control and taunt order |
| Frenzy 72737 | Once, at 30%: +30% attack speed | Lust and burn | Lust waits for it; `dbs.phase` 2 |
| Berserk | 8 min (6 min heroic) | | |

He never aggroes by sight (`REACT_DEFENSIVE`, empty `MoveInLineOfSight`), obeys taunt diminishing
returns (`flags_extra` 0x80000) and is immune to nearly all control and knockback
(`creature_immunities` -286). His boundary is x -565..-465, y 2160..2260.

## Duties

- **Tanks**: the main tank's stand is 2.1 yd east of his spot (-496.35, 2211.33), the next tank's 1.5
  yd north of it, a third's 1.5 yd south: all on the platform inside his melee reach, so he stays on
  his spot facing east. Generic taunts are blocked while he is live. When the rune lands on the
  holder or a non-tank holds him, the lowest-guid bot tank free of it, taunt ready and within 30 yd,
  taunts him (a rune tank only for a non-tank holder). The rune tank not holding him
  drops its attack on him, its pickers blocked, and takes a beast on it or in reach. The tank not
  holding him taunts the loose beast nearest its victim, only while its own rune has the taunt
  cooldown plus 0.5 s left or another bot tank free of the rune has its taunt ready, so the next
  swap is covered; a scented beast lifts that.
- **Ranged and healers**: one latched slot each (`dbs.slot`). Healers take 1, 3, 2, 10, 11 first;
  ranged, hunters leading, take 7, 6, 5, 4, 9, 8, then 1, 3, 2; spare healers the inner rest; the
  rest stack 3.5 yd behind him (12). Slots 1-9 sit within 31 yd of his spot, inside `reach spell`'s
  32; 10 and 11 at 33 yd, healers only. Reach, heal reach and spell movers are held while a slot holds.
- **DPS targets** (`dbs.target`): a player's skull, then ranged on their kill duty, split evenly over
  the beasts, each the one furthest from it within 32 yd, kept while in reach; melee on a beast in
  their melee reach; else him.
- **Control** (`dbs.cc`): each beast on a non-tank and not held by a stun, root or snare with time
  left gets one bot, any but the tank holding him, casting from where it stands: Hammer of Justice,
  Concussion Blow, Bash, Chains of Ice, Slow, Concussive Shot, Frost Shock, Curse of Exhaustion,
  Hamstring, Wing Clip. Released once it lands, after 4 s, or when the beast dies or reaches a tank.
- **Mark heal** (`dbs.markheal`): one bot healer per marked player, in reach of it first, then the
  least loaded: holy paladins, holy priest, shaman, druid, discipline priest. A paladin keeps Beacon
  of Light on its first mark, its class beacon nodes held (they carry the spell's name, which no rule
  can list); every duty healer heals its marks under 75% unless a tank is under 35%.
- A damage dealer's area damage and Army of the Dead are held; the opener holds damage until a tank
  has him 2 s.

Slots, navprobe-clean (`--nav 0x09`, settled z 539.28-539.29; tanks 541.11, stack 540.64), paths
from the stack normal. Sight from them is not checkable offline.

| Slot | x, y | From his spot |
|---|---|---|
| 1 | -515.85, 2211.33 | 19.5 yd, 180° |
| 2 | -511.07, 2224.12 | 19.5, 139° |
| 3 | -511.07, 2198.54 | 19.5, 221° |
| 4 | -499.07, 2230.64 | 19.5, 98° |
| 5 | -499.07, 2192.02 | 19.5, 262° |
| 6 | -525.19, 2221.26 | 30.5, 161° |
| 7 | -525.19, 2201.40 | 30.5, 199° |
| 8 | -511.60, 2237.74 | 30.5, 120° |
| 9 | -511.60, 2184.92 | 30.5, 240° |
| 10 | -499.23, 2244.20 | 33, 95° |
| 11 | -499.23, 2178.46 | 33, 265° |

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Tanks east, the raid west | Warcraft Tavern diagram; the old spot | Wowhead: him facing the ship |
| 13.5 yd slot spacing, 11 slots, overflow with the melee | 72380's 12 yd plus park tolerance; the floor holds no more inside reach | 12.5 yd for 13 |
| No opening positions | 17 s to the first Blood Nova; a pre-pull hold strands bots when the leader leaves | pre-pull stands |
| Rune tank yields | a taunt only matches threat, so the rune tank could take him back | keep hitting |
| Beast taunt gated on the rune clock | an 8 s taunt spent on a beast late in the rune misses the next swap | taunt loop |
| Ranged on the furthest beast | Wowhead: the beast you hit comes for you, so it crosses the slows | nearest |
| No roots | a rooted creature here keeps its threat victim, unlike the guides' retail note, but roots break on their damage cap under focus fire | roots |
| Healer order for Marks | Wowhead: the least effective tank healers carry the marks | |
| Lust on Frenzy | both guides | |
| 5% Mark heal and beast Blood Power on every difficulty | the script | the guides' heroic-only notes |
| Holder ignores a player's skull | it must keep him | |
| Scent of Blood by a beast's age | the script's 10 s event; 72771's target is unproven | the aura |

## Cheats

None before, none now. The old beast taunt loop (one beast per 8 s cooldown, both tanks) is gone, and
so are the melee skull icon and the root CC (`IccTryClassCC`'s Frost Nova and Entangling Roots).

## Known gaps

- No Hand of Protection on a marked Boiling Blood victim, no Divine Intervention or hearthstone tricks.
- No defensive rotation at high Blood Power or Frenzy, no Frost Trap or Earthbind Totem at his feet,
  no knockbacks on cue.
- A human tank partner is not told to swap.
- With fewer than 4 (10) or 10 (25) players on slots, Blood Nova can pick from the melee stack.
- Overflow ranged in the stack sit beside the beasts' spawn points.
- A beast's victim does not kite it.
- No 631 trace from this build yet.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `dbs.phase` | per bot | `1`, `2` once he is in Frenzy |
| `dbs.slot` | per bot | 1-11 a slot, 12 the stack, 13-15 a tank spot; `0` released |
| `dbs.target` | per bot | The resolver's rule: `skull`, `kill`, `reach`, `beast` (a tank's), `boss`, `none` |
| `dbs.kill` | ranged → beast | The kill duty; `0` released |
| `dbs.cc` | bot → beast | The control duty; `0` released |
| `dbs.markheal` | marked → healer | The Mark heal duty; `0` released |
| `dbs.swap` | per tank | `1` for the bot elected to taunt him |
| `dbs.yield` | per tank | `1` while the rune tank stands off him |

`tools/botobs/bosses/deathbringer_saurfang.py`: `--phases`, `--slots`, `--targets`, `--swaps` (each
Rune of Blood, its tank and the swap taunt after it), `--novas` (players each Blood Nova hit),
`--kills`, `--control`, `--marks` (each Mark, its healer and whether it died), `--cheats`.

Next pull, 25N: every Rune of Blood swapped within 2 s, no second player hit by a Blood Nova, every
beast dead inside 10 s without reaching a healer, every Mark with a healer and alive. 25H: no beast
alive at its Scent of Blood, lust inside Frenzy.
