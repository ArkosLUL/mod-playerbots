# Rotting Frost Giant

No boss state: slug `rotting-frost-giant`, prefix `rfg.` (`icc.` is raid-wide only). Code: stem
`FrostGiant` (`src/Ai/Raid/ICC/*/ICC*_FrostGiant.*`), node `icc rotting frost giant plague`; the
definition's tick opens the pull with `IccMarkPull` while the gate is live. Raid conventions:
[README.md](README.md). Core: `npc_rotting_frost_giant`, `spell_frost_giant_death_plague`
(`icecrown_citadel.cpp`). Entries 38490 (10) and 38494 (25), the same on heroic.

## Mechanics

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Death Plague 72879 → 72865 | Every 15 s on a random player other than his victim and without Recently Infected 72884, the giant as caster. One tick at 15 s: 72864 deals 8,000 to every player within 8 yd of the carrier (10 with reach, a player's ally spell), carrier included. If nobody else is in range, or all of them carry it, the carrier dies (72867); else it gets Recently Infected (35 s) and every hit player with neither aura catches 72865 | Pairs, each more than 10 yd from the next; it hits after 15 s and kills a host with nobody to spread to | Pair duty (`rfg.partner`): each carrier gets one partner, someone with Recently Infected first, so the plague stops there, else the nearest clean player, never a tank. The carrier walks to within 3 yd and stays until it ticks; a tank carrier stays and its partner walks to it. Every other clean non-tank keeps 12.5 yd from each carrier |
| Stomp 64652 | His victim, from 5-8 s, every 15-18 s: 12k and a knockback | Knocks back 20 yd | Generic; a pair broken by it walks back |
| Arctic Breath 72848 | His victim, from 10-15 s, every 26-33 s: a frontal cone, 4k each 0.5 s for 2 s | | Generic: the tank faces him away, melee stand behind |

A clean partner catches the plague and pairs back onto the cured carrier, whose Recently Infected
ends the chain at the next tick: two infections per giant cast while everyone else keeps clear.

**The code used to call the core bugged ("two plagues at once, or when cured"). Half holds.** The
giant's pick excludes Recently Infected but not a carrier, and periodic auras from different casters
stack (`Aura::CanStackWith`), so a carrier he picks again holds two plagues that tick apart: two
spreads, or a death at the second. A cured player is safe: Recently Infected blocks both the pick and
the spread. The pair duty survives a double, since the partner is still beside it. Issue text:

> Rotting Frost Giant: `EVENT_DEATH_PLAGUE` picks with `SelectTarget(Random, 0, 0.0f, true, false,
> -SPELL_RECENTLY_INFECTED)`, which skips Recently Infected (72884) but not a player already carrying
> Death Plague (72865). The giant's 72865 has him as caster and a spread one has the carrier, and
> periodic trigger auras from different casters stack, so that player carries two plagues that tick
> separately. The pick should also skip 72865.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Pairs form round each carrier as the plague lands, not as a fixed layout | The blog's pairs; a fixed layout would hold the melee off the giant all fight | standing pairs |
| No tank position | Nothing in the guide; the old one was used only by commented-out code | a navprobed spot |

## Cheats

Replaced: `rfg.deathplague`, the strip of every bot's plague, by the pair duty.

## Known gaps

- Melee dodging a carrier lose uptime for up to 15 s.
- A carrier finds no partner when every other player carries the plague or tanks, and dies at its tick.
- No trace from this build yet.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `rfg.partner` | carrier → partner | Who the carrier stands by when it ticks; `0` released: it ticked, died, or the partner caught one of its own |

`tools/botobs/bosses/rotting_frost_giant.py`: `--plague` (per player from the aura rows: infections,
doubles beside another caster's plague, cures, deaths carrying it), `--partners`, `--cheats` (older
builds' `rfg.deathplague`).

Next pull, 25N: no death carrying the plague, and no infection that `--partners` doesn't pair.
