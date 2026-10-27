# Lady Deathwhisper

Boss id 1, slug `lady-deathwhisper`, prefix `ldw.`. Code: stem `Deathwhisper`
(`src/Ai/Raid/ICC/*/ICC*_Deathwhisper.*`), nodes `icc dark reckoning`, `icc lady deathwhisper` →
ranged position, adds, shade; `IccLadyDeathwhisperMultiplier`. Raid conventions: [README.md](README.md).

## Mechanics (core)

- **Mana Barrier** 70842, cast at engage: every second it heals her missing health and takes the
  same from her mana, so her health stays full through phase 1. Phase 2 starts on the first hit at
  least as large as her remaining mana: the barrier goes, every threat entry is wiped, she starts to
  move, and on heroic she turns immune to taunt and attack-me and keeps summoning a wave every 45 s.
- **Dominate Mind** 71289, from 30 s then every 40-45 s, on 1/1/3 players in 25N/10H/25H, none in
  10N, never her victim or an add's.
- **Phase 2**: Touch of Insignificance 71204 on her victim every 6-9 s (-20% threat a stack), Vengeful
  Shades every 12 s on 1/2/1/3 non-tanks (10N/25N/10H/25H).

## Phase

`IccLdwBarrierUp` reads Mana Barrier, and every phase test of the adds action goes through it. They
used to read `HealthAbovePct(95)`, which the barrier keeps true until she has lost 5% in phase 2, so
the tanks kept stacking adds on her and the hunters kept up Viper Sting into phase 2.

## Targeting

The tanks keep skull on the add nearest her within 25 yd, else on her (`UpdateRaidTargetIcon`). The
rule moves skull only off a dead or missing unit, or from a non-add onto an add, so a live non-add
left marked from an earlier fight keeps it.

## Cheats

Kept, `raid` gate:

- `ldw.cyclone`: Cyclone on a Dominate Mind victim, bot or human; the Cyclone comes off ungated once
  the charm ends. A legit answer is a CC duty (a druid's Cyclone, Polymorph, Hex) with its own
  election and cast time.
- `ldw.insignificance`: a tank strips Touch of Insignificance. A legit answer is a tank swap on its
  stacks.

Record-only, `EnableICCBuffs` on heroic: `ldw.nothreat` (1% Threat on a non-tank holding her) and
`ldw.spitefulfury` (the main tank, while she is on someone else). The raid-wide strip takes the 1%
Threat off once her fight ends ([README.md](README.md#cheats)).

No class-taunt caller: the adds action casts its own taunts.

## Known gaps

- Raid-icon targeting (README Known gaps).
- Both kept cheats: a server with the raid cheat off loses its answer to Dominate Mind and to Touch
  of Insignificance.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `ldw.phase` | per bot | `1` while Mana Barrier holds, `2` after |
| `ldw.skull` | per tank | What skull is on after the rule: `add`, `boss`, `other` |

Cheat keys: `ldw.cyclone`, `ldw.insignificance`, `ldw.nothreat`, `ldw.spitefulfury`.
`tools/botobs/bosses/lady_deathwhisper.py`: `--phases`, `--skull`, `--dominate` (each Dominate Mind
landing, and a Cyclone on that victim within 1 s), `--cheats`.
