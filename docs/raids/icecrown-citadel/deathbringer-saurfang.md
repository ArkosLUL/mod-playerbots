# Deathbringer Saurfang

Boss id 3, slug `deathbringer-saurfang`, prefix `dbs.`. Code: stem `Saurfang`
(`src/Ai/Raid/ICC/*/ICC*_Saurfang.*`); nodes `icc dbs` rune swap, rune yield, beast taunt, beast
control, mark heal, protect, tank defensive, mark defensive, target, stand; multipliers `icc dbs area
damage`, `icc dbs beacon duty`. Raid conventions: [README.md](README.md). Core:
`boss_deathbringer_saurfang.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; the Spell constructor remaps a cast, so code matches the
base id through `GetSpellIdForDifficulty`. The leech, Boiling Blood, Blood Nova, the Mark splash and the
Mark's death heal have a row (none in the world DB); the Mark, the Rune and Frenzy auras do not. He and
his beasts carry the default 1.5 yd reach. Everything he does is physical (swings, the Rune leech, Boiling
Blood, Blood Nova, the Mark splash), so Anti-Magic Shell and Cloak of Shadows are worth nothing here. He
swings once a second, 1.3 a second in Frenzy.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Blood Power 72371, Blood Link 72178 | Energy 0-100, +1% of all his damage a point. Fed by his spell damage (+1 a hit, +2 per Blood Nova target), +3 per Blood Beast hit, every difficulty; his swings feed none. At 100: a Mark, energy to 0 | Mitigate his damage, defensives from 70, majors kept for Frenzy | `dbs.phase`; tank ladder, marked bots' buttons |
| Rune of Blood 72410 | On his victim from 20 s, then every 20-25 s, 20 s aura. Each swing on it leeches 5.1-6.9k (10) or 5.95-8.05k (25) (72409/72447/72448/72449) and heals him 10× that, 20× on heroic | The other tank taunts at once | A bot tank free of the rune swaps (`dbs.swap`), the rune tank stops hitting him (`dbs.yield`); a human partner is called |
| Boiling Blood 72385/72441/72442/72443 | From 15.5 s, every 15-20 s: 5k every 3 s for 15 s on 1 (10) or 3 (25) players, never his current victim, so the off-tank can carry it | Heal; Hand of Protection on a marked one | Generic heals; Hand of Protection duty (`dbs.protect`) |
| Blood Nova 72378 (10), 73058 (25) | 1 s cast from 17 s, every 20-25 s. Victim drawn at random from players more than 12 yd off his edge (15 yd from his centre, 16.5 at full Blood Power), or the farthest 4 (10) / 10 (25) when fewer. 72380/72438/72439/72440: 7.6-8.4k (10) or 9.5-10.5k (25) to everyone within 12 yd of the victim, centre to centre | Ranged and healers 12 yd apart, part of the raid out of melee | 11 slots, 13.5 yd apart; overflow stacks with the melee; melee fill slots when too few are out |
| Mark of the Fallen Champion 72293 | At 100 Blood Power, 1.5 s cast, on a random player other than his victim or the marked, so the off-tank can carry it; permanent, through death. Each of his swings deals 72255/72444/72445/72446, 4.3-4.7k (10) or 6.2-6.8k (25), to every marked player, armor ignored. A marked death heals him 5% (72260/72278), 20% on heroic (72279/72280) | A healer per Mark, holy paladins with Beacon first; personal cooldowns from Blood Power 70 and in Frenzy | Mark heal duty (`dbs.markheal`); marked bots' own buttons (`icc.personal`); Hand of Protection |
| Blood Beast 38508 | From 30 s, every 40 s: 2 (10) at his left and right, 5 (25) also back-left, back-right and front, 7 yd out. Rooted 1.5 s, opens on a random player but his victim, then threat; 8 yd/s; Resistant Skin 72723 -95% area damage; open to stun, root, snare, knockback (`creature_immunities` -205) | Ranged focus, the furthest first; melee only those in reach; slows, stuns, knockbacks, no roots; taunt as a last resort; no area damage | Kill split (`dbs.kill`), control duty (`dbs.cc`), the free tank's taunt, melee in reach |
| Scent of Blood 72769 (heroic) | 10 s after each wave, on every live beast, older waves too: the raid slowed 80% for 10 s, the beasts +300% damage (72771) | Kill them inside 10 s | 9-21.5 s after the latest wave the beast taunt skips its swap reserve |
| Frenzy 72737 | Once, at 30%: +30% attack speed | Lust and burn; majors for it | Lust waits for it; `dbs.phase` 2; the holder spends tank buttons throughout |
| Berserk | 8 min (6 min heroic) | | |

He never aggroes by sight (`REACT_DEFENSIVE`, empty `MoveInLineOfSight`), obeys taunt diminishing
returns (`flags_extra` 0x80000) and is immune to nearly all control and knockback
(`creature_immunities` -286). His boundary is x -565..-465, y 2160..2260.

## Duties

- **Tanks**: the main tank's stand is 2.1 yd east of his spot (-496.35, 2211.33), the next tank's 1.5
  yd north of it, a third's 1.5 yd south: all on the platform inside his melee reach, so he stays on
  his spot facing east. Generic taunts are blocked while he is live. When the rune lands on the
  holder or a non-tank holds him, the lowest-guid bot tank free of it, taunt ready in its form (bear,
  Defensive Stance) and within 30 yd, taunts him (a rune tank only for a non-tank holder). The rune
  tank not holding him drops its attack on him, its pickers blocked, and takes a beast on it or in
  reach. The tank not holding him taunts the loose beast nearest its victim, only while its own rune
  has the taunt cooldown plus 0.5 s left or another bot tank free of the rune has its taunt ready, so
  the next swap is covered; Scent of Blood lifts that.
- **Tank defensives** (`dbs.defensive`, physical only): the holder spends one button from Blood Power
  70 (40 with a Mark, whose splash hits it every swing too) once per cycle, a cycle ending at each Mark,
  and whatever is ready all through Frenzy; any tank under 40% spends one. Class nodes stay held until
  25%. A human holder gets the same windows as calls (`dbs.humancall`): per cycle, at Frenzy, when low.
  A human partner is called to taunt when the rune sits on a bot holder and no bot tank is free. The
  rune is no window: the swap ends it in a swing or two.
- **Ranged and healers**: one latched slot each (`dbs.slot`). Healers take 1, 3, 2, 10, 11 first;
  ranged, hunters leading, take 7, 6, 5, 4, 9, 8, 1, 3, 2, 10, 11; spare healers the inner rest; the
  rest stack 3.5 yd behind him (12). Every slot sits within 31 yd of his spot, inside `reach spell`'s
  32. Bots park within 0.75 yd, so neighbours stay 12 yd apart. Reach, heal reach and spell movers are
  held while a slot holds. While fewer bots than Blood Nova needs (4 on 10, 10 on 25) hold a slot,
  melee bots idle on the free inner ones, those already out first.
- **DPS targets** (`dbs.target`): a player's skull, then ranged on their kill duty, split evenly over
  the beasts, each the one furthest from it within 32 yd, kept while in reach; melee on a beast in
  their melee reach; else him.
- **Control** (`dbs.cc`): each beast on a non-tank and not held by a stun, root or snare with time
  left gets one bot, any but the tank holding him, casting from where it stands in a form or stance
  the spell allows: Hammer of Justice, Concussion Blow, Bash, Chains of Ice, Slow, Concussive Shot,
  Frost Shock, Curse of Exhaustion, Hamstring, Wing Clip. Released once it lands, after 4 s, or when
  the beast dies or reaches a tank.
- **Mark heal** (`dbs.markheal`): one bot healer per marked player, in reach of it first, then the
  least loaded: holy paladins, holy priest, shaman, druid, discipline priest. A paladin keeps Beacon
  of Light on its first mark, its class beacon nodes held (they carry the spell's name, which no rule
  can list); every duty healer heals its marks under 75% unless a tank is under 35%.
- **Marked bots** (`icc.personal`): a marked non-tank, from Blood Power 70 or in Frenzy, asks
  `GetRaidPersonalDefensive` about 4 s of splash at the current Blood Power: a class button, and on a
  lethal reading an immunity or a healthstone, since a Mark's death heals him 5%, 20% on heroic.
- **Hand of Protection** (`dbs.protect`, paladin → player): a bot with a Mark, 7 s or more of Boiling
  Blood and no Forbearance that fights with spells (no melee, hunter or tank) gets the nearest bot
  paladin with the hand ready within 30 yd and in sight. Ticks and splash are physical: the hand takes
  both and the Blood Power they feed.
- A damage dealer's area damage and Army of the Dead are held; the opener holds damage until a tank
  has him 2 s.

Slots, navprobe-clean (`--nav 0x09`; settled z 539.28-539.29 west, 541.11 east and on the tank
spots, stack 540.64), paths from the stack normal. `los` from every slot, the stack and each tank spot
to him and to the three tank spots is clear with him anywhere in 2.5 yd of his spot (144 rays).
Gameobject collision isn't in navprobe's sight.

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
| 10 | -478.85, 2211.33 | 17.5, 0° (east causeway) |
| 11 | -465.35, 2211.33 | 31, 0° (east causeway) |

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Tanks east, the raid west | Warcraft Tavern diagram; the old spot | Wowhead: him facing the ship |
| 13.5 yd slot spacing, 11 slots, overflow with the melee | 72380's 12 yd plus 0.75 yd park tolerance each; the floor holds no more inside reach | 12.5 yd for 13 |
| Slots 10 and 11 on the east causeway, 17.5 and 31 yd out | navprobe `los`: 33 yd north or south of him the platform edge hides the tank spots (blind from 80° to 95° and from 265° to 285°, clear from 100° and 260° out) while his own spot stays visible, so healers 4 and 5 never saw a tank. The first robust west spots lie 37-39 yd from the tank spots, past the 38.5 yd heal reach of the far one | Keep them at 33 yd |
| Heal reach held on slots | slot 1 reaches every slot and tank spot; a walk breaks the spread | heal reach kept, as `docs/engine/pitfalls.md` advises |
| No opening positions | 17 s to the first Blood Nova; a pre-pull hold strands bots when the leader leaves | pre-pull stands |
| Rune tank yields | a taunt only matches threat, so the rune tank could take him back | keep hitting |
| Beast taunt gated on the rune clock | an 8 s taunt spent on a beast late in the rune misses the next swap | taunt loop |
| Ranged on the furthest beast | Wowhead: the beast you hit comes for you, so it crosses the slows | nearest |
| No roots | a rooted creature here keeps its threat victim, unlike the guides' retail note, but roots break on their damage cap under focus fire | roots |
| Healer order for Marks | Wowhead: the least effective tank healers carry the marks | |
| Lust on Frenzy | both guides | |
| Beast Blood Power on every difficulty | the script | Warcraft Tavern: none on normal |
| Holder ignores a player's skull | it must keep him | |
| Scent of Blood by the latest wave's clock | the script's 10 s event on every live beast; 72771's target is unproven | the aura |
| Tank ladder: one button at Blood Power 70, all in Frenzy, class nodes held to 25% | Wowhead tank tips (defensives from 70, majors for Frenzy); convention 43 | A button per Rune; a timer |
| A Mark on the holder opens its window at 40 | the splash hits it every swing and Blood Power scales that too | 70 |
| Marked bots ask the shared personal pick about 4 s of splash | Wowhead (personal cooldowns at 70+ and in Frenzy); the pick adds an immunity or a healthstone only when the model reads lethal | Class buttons only |
| Hand of Protection only on bots that fight with spells | Wowhead (on Boiling Blood, preferably marked); 1022/10278 is physical immunity plus pacify, and nothing on 72255 or 72385 pierces it | Any marked victim; Divine Intervention and Hearthstone lines (cheese) |
| Melee fill free slots below the Nova minimum | `spell_deathbringer_blood_nova_targeting`: victim among players off his edge, else the farthest 4 / 10, stack included | The stack takes the Nova |
| Calls to a human tank, no duty | convention 43; Humans decision | None |

## Cheats

None before, none now. The old beast taunt loop (one beast per 8 s cooldown, both tanks) is gone, and
so are the melee skull icon and the root CC (`IccTryClassCC`'s Frost Nova and Entangling Roots).

## Known gaps

- No Frost Trap or Earthbind Totem at his feet, no knockbacks on cue: both drop at the caster's feet,
  and anyone at his feet breaks the Blood Nova spread. The control duty slows and stuns from the slots
  instead. No Righteous Fury paladin bait at max range either: each beast opens on a random player
  (`JustSummoned`), so threat has nothing to hold before they run.
- The tank ladder spends the weakest ready button at each peak and keeps no major for Frenzy.
  `--defensives` shows what each window spent.
- No Divine Intervention or hearthstone lines (Wowhead strategies 2 and 3): they take the Mark carrier
  out of the fight; the hand and Beacon lines carry the Marks.
- No healer external cooldowns (Pain Suppression, Guardian Spirit, Hand of Sacrifice) at Blood Power 70
  or in Frenzy: no shared dealer exists for a single-target cooldown.
- A rune holder with no free tank, and a tank on a scented beast, get only the low-health rule. A human
  holder gets one call at Frenzy, then low-health calls.
- Overflow ranged and healers in the stack sit beside the beasts' back spawn points: no room on the
  floor or the causeway holds them 13.5 yd from the rest. `--control` and `--marks` show whether stack
  bots die to first strikes.
- A beast's victim does not kite it: Scent slows the raid 80%, and the slots hold the Nova spread.
  `--kills` and `--control` show whether any beast reaches a victim.
- Melee fill counts bots only. Humans hold no duty. Sight is static geometry only.
- No 631 trace from this build yet, and never built.

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
| `dbs.defensive` | per tank | The ladder's pick: a cast name, `covered`, `none`, `idle` outside a window |
| `dbs.humancall` | per sender | The button or taunt a human tank was told to press |
| `dbs.protect` | paladin → player | The Hand of Protection duty; `0` released |
| `icc.personal` | per bot | A marked bot's pick: a cast name, `healthstone`, `none`, `covered` |

`tools/botobs/bosses/deathbringer_saurfang.py`: `--phases`, `--slots`, `--targets`, `--swaps` (each
Rune of Blood, the swap taunt after it and the rune tank's yield), `--novas` (players each Blood Nova hit),
`--kills`, `--control`, `--marks` (each Mark, its healer and whether it died), `--defensives` (tank
windows with the buttons cast, calls to a human tank, the marked bots' picks), `--protect` (each hand
duty and its cast), `--cheats`.

Next pull, 25N: every Rune of Blood swapped within 2 s, no second player hit by a Blood Nova, every
beast dead inside 10 s without reaching a healer, every Mark with a healer and alive, a tank button in
every `--defensives` window, each Mark with Boiling Blood under a hand or healed. 25H: no beast alive at
its Scent of Blood, lust inside Frenzy, no Mark carrier dead.
