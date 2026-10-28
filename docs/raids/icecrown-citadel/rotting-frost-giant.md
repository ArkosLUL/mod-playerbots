# Rotting Frost Giant

No boss state: slug `rotting-frost-giant`, prefix `rfg.` (`icc.` is raid-wide only). Code: stem
`FrostGiant` (`src/Ai/Raid/ICC/*/ICC*_FrostGiant.*`), node `icc rotting frost giant plague`; the
definition's tick opens the pull with `IccMarkPull` while the gate is live. Raid conventions:
[README.md](README.md). Core: `npc_rotting_frost_giant`, `spell_frost_giant_death_plague`
(`icecrown_citadel.cpp`). Entries 38490 (10) and 38494 (25), the same on heroic.

## Mechanics

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Death Plague 72879 → 72865 | Every 15 s on a random player other than his victim and without Recently Infected 72884, the giant as caster. One tick at 15 s: 72864 deals 8,000 to every player within 8 yd of the carrier (a player's area spell adds no reach to another player), carrier included. If nobody else is in range, or all of them carry it, the carrier dies (72867); else it gets Recently Infected (35 s) and every hit player with neither aura catches 72865 | Pairs, each more than 10 yd from the next; it hits after 15 s and kills a host with nobody to spread to | Pair duty (`rfg.partner`): each carrier gets one partner, someone with Recently Infected first, so the plague stops there, else the nearest clean non-tank, a tank only when no one else is left, and a bot for a tank carrier. A pair of bot non-tanks walks out together to a spot (`rfg.apart`) 12.5 yd clear of everyone else, in sight of the giant and within 30 yd of it, and stays until it ticks, so nobody else moves. A pair with a tank or a player stays: a tank or player carrier keeps its place and its partner walks to it, a carrier walks to its tank or player partner, and every other clean non-tank steps out to 12.5 yd once within the guide's 10 of a carrier. Generic movers hold while a bot is inside 12.5 |
| Stomp 64652 | Cast at his victim, from 5-8 s, every 15-18 s: 12k and a knockback to everyone within 50 yd of his front | Knocks back 20 yd | Generic; a pair broken by it walks back |
| Arctic Breath 72848 | His victim, from 10-15 s, every 26-33 s: a frontal cone, 2 s, 4k a tick, and a stun | | Generic: the tank faces him away, melee stand behind |

A clean partner catches the plague and pairs back onto the cured carrier, whose Recently Infected
ends the chain at the next tick: two infections per giant cast while everyone else keeps clear. The
cured partner stands where the first pair did, so the second pair's spot is that place.

**The code used to call the core bugged ("two plagues at once, or when cured"). Half holds.** The
giant's pick excludes Recently Infected but not a carrier, and periodic auras from different casters
stack (`Aura::CanStackWith`), so a carrier he picks again holds two plagues that tick apart: two
spreads, or a death at the second. A cured player is safe: Recently Infected blocks both the pick and
the spread. The pair duty survives a double, since the partner is still beside it. Issue text:

> Rotting Frost Giant: `EVENT_DEATH_PLAGUE` picks with `SelectTarget(Random, 0, 0.0f, true, false,
> -SPELL_RECENTLY_INFECTED)`, which skips Recently Infected (72884) but not a player already carrying
> Death Plague (72865). The giant's 72865 has him as caster and a spread one its new carrier, and
> periodic trigger auras from different casters stack, so that player carries two plagues that tick
> separately. The pick should also skip 72865.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Pairs form round each carrier as the plague lands, not as a fixed layout | The blog's pairs; a fixed layout would hold the melee off the giant all fight | standing pairs |
| A pair of bot non-tanks walks out to a spot clear of the raid; otherwise the others dodge | The blog's "pairs more than 10 yards apart" done by the two who must be paired, so a carrier among the melee costs two players' uptime, not every melee within 10 yd of it | everyone near the carrier steps out (what a pair with a tank or a player still does) |
| The spot is found by a sweep from the carrier, not fixed | The giant patrols, so no spot is the same twice; the sweep tests every other player and every other spot, sight to the giant and 30 yd to it | navprobed spots (nothing to anchor them on) |
| A pair not in place after 7 s forms where it stands | The plague ticks 15 s after it lands | keep walking |
| A tank is a partner only when no clean non-tank is left | A carrier with no one in reach dies at its tick; the tank holds the giant where it is, so the carrier walks to it | let the carrier die |
| No tank defensive | Largest predictable hits: Stomp 12k and Arctic Breath 4 x 4k, neither a spike to spend a long cooldown on | a button on the breath |
| No tank position | Nothing in the guide; the old one was used only by commented-out code | a navprobed spot |

## Line of sight

No fixed stand, so none to probe: a pair's spot is found at the time and must see the giant
(`IsWithinLOS` in the sweep). Not checkable offline: the spot's floor on the day, the giant's path.

## Cheats

Replaced: `rfg.deathplague`, the strip of every bot's plague, by the pair duty.

## Known gaps

- A pair with a tank or a player makes every non-tank within 10 yd of its carrier step out, for up to 30
  s, and a pair that walked out loses two players' damage for as long.
- A carrier finds no partner when every other player carries the plague, and dies at its tick.
- A spot may lie across the giant's cone or a hazard; only the spot, not the walk, is tested.
- No trace from this build yet, never built.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `rfg.partner` | carrier → partner | Who the carrier stands by when it ticks; `0` released: it ticked, died, or the partner caught one of its own |
| `rfg.apart` | carrier | `1` while its pair walks out to a spot, `0` released or given up |

`tools/botobs/bosses/rotting_frost_giant.py`: `--plague` (per player from the aura rows: infections,
doubles beside another caster's plague, cures, deaths carrying it), `--partners` (each pair, whether it
walked out, and how many did), `--cheats` (older builds' `rfg.deathplague`).

Next pull, 25N: no death carrying the plague, no infection that `--partners` doesn't pair, and most
pairs out, with no melee displaced besides them.
