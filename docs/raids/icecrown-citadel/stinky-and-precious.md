# Stinky and Precious

Festergut's and Rotface's dogs. Trace slug `stinky-and-precious`, prefix `dogs.`
([README](README.md)). Code: stem `Dogs` (`src/Ai/Raid/ICC/*/ICC*_Dogs.*`), the `icc dogs` trigger
and multiplier.
Mechanics: `npc_stinky_icc` (`boss_festergut.cpp`), `npc_precious_icc` (`boss_rotface.cpp`).

## Mechanics

- Stinky 37025 (25-man 38064) in Festergut's room, Precious 37217 (38103) in Rotface's.
- Both: Mortal Wound 71127 on the victim every 1.5-2.5 s, 15 s, stacking, one id on every difficulty;
  Decimate 71123 every 20-25 s.
- Precious: Awaken Plagued Zombies every 20-25 s, first at 25-30 s.
- No boss state. The definition's tick opens the trace through `IccMarkPull` while the gate is live,
  so one file is one dog.

## Tactics

A tank at 8 Mortal Wound stops attacking; the other tank taunts the dog off a victim carrying 8,
and any add within 20 yd not on a tank. Taunts are class taunts through `IccCastClassTaunt`, one 500 ms
throttle per bot across all targets.

## Cheats

None.

## Known gaps

- Decimate has no answer: nothing pre-heals or spreads for it.

## What a trace answers

No probe: who holds a dog is the snapshot's target column, a swap is a taunt in the act and cast
streams. `tools/botobs/bosses/stinky_and_precious.py`:

- `--tanks`: per dog, its target runs, the swaps between tanks, each tank's peak Mortal Wound;
- `--cheats`: this encounter's keys plus the raid-wide `icc.` ones.
