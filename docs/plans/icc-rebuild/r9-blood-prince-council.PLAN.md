# r9-blood-prince-council — Blood Prince Council

Wave 3d, parallel with `r8-putricide`. Rules, conventions and the boss lane recipe:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). Layout and seams: `docs/raids/icecrown-citadel/README.md`,
"Code layout".

## Encounter

- Slug `blood-prince-council`, prefix `bpc.`, boss id 7, stem `BloodPrinces`. Core
  `boss_blood_prince_council.cpp`. Doc `blood-prince-council.md`, readers `blood_prince_council.py` and
  `blood_prince_trash.py`. Guides in the cache: Warcraft Tavern 25 and 10, Wowhead if saved.
- The Blood Prince trash (id 14) is traced only, through `r1`'s `MarkPull` hook. It gets no strategy
  code. This lane keeps its reader and doc section current, nothing more.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 733 lines (`ICCActions_BPC.cpp`).
- What is handled today:
  - the main tank holds Valanar and Taldaram; the assist tank holds Keleseth and the Dark Nuclei;
  - the skull goes on the empowered prince (2 icon writes);
  - a kinetic bomb assignment, vortex rings and an inferno soak;
  - a heroic Shadow Prison movement block.
- The Keleseth tank branch for a non-assist tank never runs. A prince goes taunt-immune after the
  fourth taunt (DR).
- `IccBpcAssistMultiplier`:
  - for the whole fight: the AoE family;
  - while an assist tank lives: the tanks' AttackRti and TankAssist;
  - the bomb-assigned bot: its targeting and movement;
  - over 12 Shadow Prison stacks (tanks 18): MovementAction except attack and reach;
  - while empowered vortex is up: everything but AvoidAoe and the vortex action;
  - chased by a flame: everything but the ball action.
- Floor snaps through `NearTeleportTo` are borderline: apply the cheat rule.
- No cheats. 3 positions and 3 Z constants.

## Guide answers to build on

- 2-3 tanks; Keleseth's tank holds up to 6 Dark Nucleus stacks.
- 4-6 healers.
- A hunter or 3 pets juggle the Kinetic Bombs.
- A 12 yd spread with spots picked in advance, for Shock Vortex and Empowered Shock Vortex.
- The empowered prince takes the damage after each Invocation of Blood.
- Bloodlust in the Keleseth phase.
- Heroic: Shadow Prison.

## Focus

- The empowered prince by direct targeting.
- Keleseth's tank and the nuclei pickup.
- A kinetic bomb duty.
- The vortex spread on latched stands, and the flame ball soak and kite.
- The Shadow Prison hold.
- Prince handling that doesn't depend on a taunt loop across DR.

## Owns

Stem `BloodPrinces` and its definition, the doc, both readers and their tests, this brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
