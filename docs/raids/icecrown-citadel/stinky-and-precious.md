# Stinky and Precious

Festergut's and Rotface's dogs. Slug `stinky-and-precious`, prefix `dogs.` ([README](README.md)). Code:
stem `Dogs` (`src/Ai/Raid/ICC/*/ICC*_Dogs.*`); nodes `icc dogs` tank defensive, decimate cover, swap,
yield, target; multiplier `icc dogs yield`. Core: `npc_stinky_icc` (`boss_festergut.cpp`),
`npc_precious_icc` (`boss_rotface.cpp`), `spell_stinky_precious_decimate` (`icecrown_citadel.cpp`).

## Mechanics

Stinky 37025 (25-man 38064) stands in Festergut's room, Precious 37217 (38103) in Rotface's; combat reach
9, no taunt diminishing returns. No boss state: the definition's tick opens the trace through
`IccMarkPull` while the gate is live, so one file is one dog.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Mortal Wound 71127 | On the victim every 1.5-2.5 s, −10% healing taken a stack, 15 s, ten at most; one id on every difficulty | Tank swap | Elected swap at 6 by a tank 4 stacks lighter (`dogs.swap`); the relieved tank yields until its stacks drop (`dogs.yield`) |
| Decimate 71123 | From 20-25 s, every 20-25 s: 3 s cast, everyone within 100 yd (SpellInfoCorrections) above 15% health set to 15%, then the dog stuns itself 3 s | Healers ready for it | The holder spends a tank button in the cast; the raid cover while the dog is stunned (`icc.cover`) |
| Plague Stench 71805 (Stinky) | Aura from the pull: 71160 (10) 3k, 71161 (25) 4.5k nature every 3 s within 100 yd, no heroic row | Heal through | Counted as two ticks in the Decimate cover |
| Awaken Plagued Zombies 71159 (Precious) | From 25-30 s, every 20-25 s: 11 Plagued Zombies 38104, each opening on a random player; she despawns them when she dies | Kill her, AoE the zombies | Everyone on her; the free tank takes zombies on non-tanks; area damage free |

## Duties

- **Tanks**: generic taunts are blocked while a dog fights. Once the holder carries 6 Mortal Wound, or a
  non-tank holds the dog, the bot tank with 4 fewer stacks than the holder, the fewest, taunt ready in its
  form within 30 yd, taunts it. A tank not holding the dog that still carries Mortal Wound yields: off the
  dog, its pickers and its spells and area damage near the dog held. A human partner is called to taunt
  when no bot can.
- **Tank defensives** (`dogs.defensive`, physical only): the holder through Decimate's cast, any tank
  under 40%; class nodes held until 25%. A human holder gets the same windows as calls (`dogs.humancall`).
- **Decimate cover**: while the dog stuns itself after Decimate, everyone at 15%, `GetRaidCover` deals one
  cover on a 6 s hit stamped on the landing (Anti-Magic Zone dropped on Precious, whose zombies hit
  physical).
- **Targets** (`dogs.target`): a player's skull, the holder its dog, a free tank the zombie on a non-tank
  nearest it within 30 yd, else the dog; a yielding tank a zombie or nothing.

No stands: Decimate and Plague Stench reach 100 yd, so positioning buys nothing.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Swap at 6 by a tank 4 stacks lighter | script: 15 s against 1.5-2.5 s a stack, so a relieved tank is clean in 15 s while its partner gains 6-10 | Fixed 8 |
| Generic taunts blocked | the free tank's would take the dog straight back | |
| Burn the dog, zombies only to the free tank | the script despawns them with Precious; the blog | Zombies first |
| No zombie taunts | an 8 s taunt spent on a zombie misses the next swap | |
| Cover after Decimate, button in its cast | everyone at 15% under a 3-4.5k tick every 3 s | Divine Hymn and Tranquility kept for Festergut: the first Decimate takes every ready cover |

## Cheats

None before, none now.

## Known gaps

- Eleven zombies a wave on Precious: nothing spreads or kites them, and the raid's area damage near her is
  incidental. `--targets` and the deaths show whether a pull outlasts the waves.
- A free tank on the dog can still rip it by threat: no brake here.
- Humans hold no duty. No 631 trace from this build yet, and never built.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `dogs.target` | per bot | The resolver's rule: `dog`, `zombie`, `skull`, `none` |
| `dogs.swap` | per tank | `1` for the bot elected to taunt the dog |
| `dogs.yield` | per tank | `1` while a relieved tank stays off the dog |
| `dogs.defensive` | per tank | The ladder's pick: a cast name, `covered`, `none`, `idle` outside a window |
| `dogs.humancall` | per sender | The button or taunt a human tank was told to press |
| `icc.cover` | per bot | The cover dealt after Decimate |

`tools/botobs/bosses/stinky_and_precious.py`: `--tanks` (per dog its target runs, swaps, peak Mortal
Wound), `--swaps` (elections, their taunts, yields), `--targets`, `--defensives` (tank windows with the
buttons cast, calls to a human tank, each Decimate's cover), `--cheats`.

Next pull, 25N: every swap at 6-7 stacks, nobody dead to Plague Stench after a Decimate, a cover after
each one. 25H: the same.
