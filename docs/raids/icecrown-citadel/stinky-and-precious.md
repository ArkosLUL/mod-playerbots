# Stinky and Precious

Festergut's and Rotface's dogs. Slug `stinky-and-precious`, prefix `dogs.` ([README](README.md)). Code:
stem `Dogs` (`src/Ai/Raid/ICC/*/ICC*_Dogs.*`); nodes `icc dogs` zombie taunt, tank defensive, decimate
cover, swap, yield, target; multiplier `icc dogs yield`. Core: `npc_stinky_icc` (`boss_festergut.cpp`),
`npc_precious_icc` (`boss_rotface.cpp`), `spell_stinky_precious_decimate` (`icecrown_citadel.cpp`).

## Mechanics

Stinky 37025 (25-man 38064) stands in Festergut's room, Precious 37217 (38103) in Rotface's; combat reach
9, no taunt diminishing returns. No boss state: the definition's tick opens the trace through
`IccMarkPull` while the gate is live, so one file is one dog.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Mortal Wound 71127 | On the victim every 1.5-2.5 s, −10% healing taken a stack, 15 s, ten at most; one id on every difficulty; `SPELL_ATTR0_NO_IMMUNITIES`, so no bubble removes it | Tank swap | Elected swap at 6 by a tank 4 stacks lighter (`dogs.swap`); the relieved tank yields until its stacks drop (`dogs.yield`); threat brake (`dogs.brake`) |
| Decimate 71123 | From 20-25 s, every 20-25 s: 3 s cast, everyone within 100 yd (SpellInfoCorrections) above 15% health set to 15%, then the dog stuns itself 3 s | Healers ready for it | The holder spends a tank button in the cast; the raid cover while the dog is stunned (`icc.cover`) |
| Plague Stench 71805 (Stinky) | Aura from the pull: 71160 (10) 3k, 71161 (25) 4.5k nature every 3 s within 100 yd, no heroic row | Heal through | Counted as two ticks in the Decimate cover |
| Awaken Plagued Zombies 71159 (Precious) | From 25-30 s, every 20-25 s: 11 Plagued Zombies 38104, each opening on a random player, living until killed (SpellInfoCorrections sets the summon infinite); she despawns them when she dies. 63k / 101k health (10 / 25), 1.2-1.9k / 2.1-3.2k a swing before armour, each swing Infected Wound 71157 (+100 physical damage taken a stack, 99, 60 s) | Kill her, AoE the zombies | Everyone on her; the free tank, or with one tank the zombie guard, takes zombies on non-tanks; area damage free |

Each dog has 3.15M (10) / 8.82M (25) health and swings for 4.9-7.3k / 10.3-15.5k (Stinky) or 9.7-14.6k /
20.7-31k (Precious) before armour. A zombie wave is 22% (10) or 13% (25) of Precious's health.

## Duties

- **Tanks**: generic taunts are blocked while a dog fights. Once the holder carries 6 Mortal Wound, or a
  non-tank holds the dog, the bot tank with 4 fewer stacks than the holder, the fewest, taunt ready in its
  form within 30 yd, taunts it. A tank not holding the dog yields while it carries Mortal Wound or sits on
  the threat brake (90% of the holder's threat, free under 80%, `dogs.brake`): off the dog, its pickers and
  its spells and area damage near the dog held. A human partner is called to taunt when no bot can.
- **Zombie guard** (`dogs.guard`): on Precious with fewer than two tanks alive, the damage-dealing death
  knight, else paladin, with the lowest guid takes the free tank's zombies, and taunts the nearest zombie
  hitting a healer within 30 yd whenever its taunt is ready (`icc dogs zombie taunt`).
- **One tank, no swap**: no bot holds a dog the 15 s its partner's stacks take to drop: Decimate leaves the
  taker at 15% under the swings above.
- **Tank defensives** (`dogs.defensive`, physical only): the holder through Decimate's cast, any tank
  under 40%; class nodes held until 25%. A human holder gets the same windows as calls (`dogs.humancall`).
- **Decimate cover**: while the dog stuns itself after Decimate, everyone at 15%, `GetRaidCover` deals one
  cover on a 6 s hit stamped on the landing (Anti-Magic Zone dropped on Precious, whose zombies hit
  physical).
- **Targets** (`dogs.target`): a player's skull, the holder its dog, a free tank or the zombie guard the
  zombie on a non-tank nearest it within 30 yd, else the dog; a yielding tank a zombie or nothing.

No stands: Decimate and Plague Stench reach 100 yd, so positioning buys nothing.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Swap at 6 by a tank 4 stacks lighter | script: 15 s against 1.5-2.5 s a stack, so a relieved tank is clean in 15 s while its partner gains 6-10 | Fixed 8 |
| Generic taunts blocked | the free tank's would take the dog straight back | |
| Burn the dog, zombies only to the free tank or guard | the script despawns them with Precious, and a wave costs as much damage as 13-22% of her; the blog; spreading buys nothing against melee, and at 8 yd/s they outrun a kiter | Zombies first |
| No zombie taunts by a free tank | an 8 s taunt spent on a zombie misses the next swap | |
| Guard taunts zombies off healers | the guard has no swap to save its taunt for; a healer at 15% dies to one swing | No guard taunt |
| Guard only with fewer than two tanks, death knight first | Dark Command and Hand of Reckoning work in any presence or seal; plate | Hunters or warlocks with pets |
| Brake 90%/80%, folded into the yield | Festergut's and Putricide's brakes | A separate node |
| Cover after Decimate, button in its cast | everyone at 15% under a 3-4.5k tick every 3 s | Divine Hymn and Tranquility kept for Festergut: the first Decimate takes every ready cover |

## Cheats

None before, none now.

## Known gaps

- One tank: the holder carries Mortal Wound to ten stacks, healing taken to nothing, until the dog dies.
- A guard holding zombies at 15% after Decimate can die to them; `--targets` and the deaths show it.
- Humans hold no duty. Built clean at `623638b99` (2026-10-29); no 631 trace from a build with this stem
  yet.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `dogs.target` | per bot | The resolver's rule: `dog`, `zombie`, `skull`, `none` |
| `dogs.swap` | per tank | `1` for the bot elected to taunt the dog |
| `dogs.yield` | per tank | `1` while a relieved or braked tank stays off the dog |
| `dogs.brake` | per tank | `1` while braked |
| `dogs.guard` | per bot | `1` for the zombie guard |
| `dogs.defensive` | per tank | The ladder's pick: a cast name, `covered`, `none`, `idle` outside a window |
| `dogs.humancall` | per sender | The button or taunt a human tank was told to press |
| `icc.cover` | per bot | The cover dealt after Decimate |

`tools/botobs/bosses/stinky_and_precious.py`: `--tanks` (per dog its target runs, swaps, peak Mortal
Wound), `--swaps` (elections, their taunts, yields, brake time), `--targets` (plus the guard and its zombie
taunts), `--defensives` (tank windows with the buttons cast, calls to a human tank, each Decimate's cover),
`--cheats`.

Next pull, 25N: every swap at 6-7 stacks, no rip by a braked tank, nobody dead to Plague Stench after a
Decimate, a cover after each one, no healer dead to a zombie. 25H: the same.
