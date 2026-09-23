# Anub'arak

Raid-wide facts and the code layout: [README.md](README.md).

- **P1**: Burrowers. Penetrating Cold hits 2/5 random players (10/25) with no splash, so no spread
  is needed.
- **P2** (submerged): Swarm Scarabs, and the Pursuing Spike's target kites it through Permafrost,
  the grounded patch a killed Frost Sphere leaves.
- **P3**: burst through the permanent Leeching Swarm. Lust waits until the bot carries it or he
  drops below 30% (Anub'arak row of `ToCBurstWindowMultiplier`).

**Frost Spheres.** A flying sphere carries Frost Sphere 67539. Damage that would kill it instead
sets `UNIT_FLAG_NOT_SELECTABLE` and drops it to the floor; 1.5 s later it loses 67539 and casts
Permafrost, a 6 yd, 15 min persistent area aura. A dynobject aura skips its own caster
(`Unit::_IsValidAttackTarget` refuses self), so **no sphere ever carries Permafrost**: a patch is an
alive sphere without 67539 (`IsPermafrostPatch`), a flying one has 67539 and is selectable
(`IsFrostSphereFlying`). Normal mode respawns a missing sphere every 4 s, heroic never.

**Leeching Swarm** is an enemy area aura (effect 129), and `UnitAura::FillTargetMap` never applies
one to its owner: the players carry it, Anub'arak never does. He casts it once below 30% health
while surfaced.

## What a trace answers

No `anub.` notes or boss reader yet, only the raid-agnostic streams.
