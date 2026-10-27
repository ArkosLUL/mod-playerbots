# Rotting Frost Giant

No boss state: slug `rotting-frost-giant`, prefix `rfg.` (`icc.` is raid-wide only). Code:
`IccRottingFrostGiantTankPositionTrigger` (opens the pull with `IccMarkPull` once bot and giant are
both in combat) and `IccRottingFrostGiantTankPositionAction`, in `Action/ICCActions_LDW.cpp`. Raid
conventions: [README.md](README.md).

## Death Plague (core)

`npc_rotting_frost_giant`, `spell_frost_giant_death_plague` (`icecrown_citadel.cpp`):

- Every 15 s the giant casts 72879 on a random player other than his victim and without Recently
  Infected 72884, which puts Death Plague 72865 on it, the giant as caster.
- 72865 ticks once, at 15 s: 72864 hits every player within 8 yd, the carrier included, for 8,000.
  If no other player is in range, or all of them carry it, the carrier dies (72867). Otherwise it gets
  Recently Infected (35 s) and every hit player without either aura catches 72865, cast by itself.
- So a legit carrier stands within 8 yd of a clean player at its tick, and the chain moves on.

**The code calls the core bugged ("two plagues at once, or when cured"). Half holds.** The giant's
target pick excludes Recently Infected but not a carrier, and periodic auras from different casters
stack (`Aura::CanStackWith`), so a carrier he picks again holds two plagues that tick apart: two
spreads, or a death at the second. A cured player is safe: Recently Infected blocks both the pick
and the spread. The plague is still playable, so the comment's "impossible the legit way" does not
hold. Issue text:

> Rotting Frost Giant: `EVENT_DEATH_PLAGUE` picks with `SelectTarget(Random, 0, 0.0f, true, false,
> -SPELL_RECENTLY_INFECTED)`, which skips Recently Infected (72884) but not a player already carrying
> Death Plague (72865). The giant's 72865 has him as caster and a spread one has the carrier, and
> periodic trigger auras from different casters stack, so that player carries two plagues that tick
> separately. The pick should also skip 72865.

## Cheats

Kept, `raid` gate: `rfg.deathplague`, every bot strips its own Death Plague the tick it lands, so with
the raid cheat on nobody spreads, ticks or dies to it. A legit answer is the spread duty the action
carries commented out: a carrier walks to a clean player, ranged spread from clean players only.

## Known gaps

- `rfg.deathplague`: a server with the raid cheat off has no plague handling.

## What a trace answers

No probe: the giant has no phase or assignment. Cheat key: `rfg.deathplague`.
`tools/botobs/bosses/rotting_frost_giant.py`: `--plague` (per player from the aura rows: infections,
doubles beside another caster's plague, cures, deaths carrying it), `--cheats`.
