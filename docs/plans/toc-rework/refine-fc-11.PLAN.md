# refine-fc-11: stand still, hold in place, peel with every CC

Wave 4r, run in session on the user's call. Status: [toc-rework.PLAN.md](toc-rework.PLAN.md). Boss doc:
[faction-champions.md](../../raids/trial-of-the-crusader/faction-champions.md). Base: 4q `4d562ce01`; the
pulls below ran the 4o + 4p + 4q build.

## Why

2026-10-23, two 25H wipes: `env/dist/logs/botobs/649_5_caiphus-the-stern_1792747264.ndjson` (2:39, two
kills) and `649_5_caiphus-the-stern_1792747579.ndjson` (2:25, one kill).

- **Healers walked:** moving 56-84% of live samples (yesterday's 25H 22-39%, the 25N kill 8-16%), 61-65%
  from `faction champions ball stand` (ranged 27-29%): a healer stand re-picks whenever a held champion
  comes within 26 yd or a holder or the kill target passes 42 yd. Raid healing 5.1-5.8k/s against
  10.6-11.2k/s taken. The user: rigid positioning and constant movement ruin the fight.
- **Holders held ~21%** (median share with the holder as victim); most died. The pick weighs health and
  armour, so cloth within ~30 yd or leather within ~20 yd outranks plate in melee range. The three melee
  champions dealt 57-61% of the damage, under our CC 0-7 s each.
- **Externals gone by 1:00:** all three Hands of Protection in the first 30 s on one physical champion
  against a healer or ranged at 63-100%; nothing for 15-20 later focus episodes.
- **Holylight's class self Lay on Hands** (23.5%, no champion on him) cost his Divine Shield 15 s later.
- **Summons leak the AoE ban:** they read positive, so Trap Launcher Explosive Trap (6-7 a pull) and Fire
  Nova (8) went out.
- **Unused CC:** Frost Trap (feet only; shares its 30 s cooldown with Freezing Arrow), Freezing Arrow,
  Repentance, Blind, Death Coil and more.

## Scope (the user's calls)

1. **Stay put:** no healer stands or ranged slots; a rule zeroes the wander families for healers and
   ranged, reaches stay. The Cyclone stand stays, kept while both its healer and the kill target remain in
   reach.
2. **Step-off:** a healer walks 10 yd off melee champions within 8 yd, latched to arrival or 2.5 s, then
   rests 3 s.
3. **Holders in place:** no spot or walk; a holder stops swinging (and its pet) while our breakable CC is
   on its champion or a breakable peel is aimed at it.
4. **Peel adds:** Freezing Arrow (hunter), Repentance (paladin, after Hammer of Justice), Blind (rogue,
   after Dismantle), Death Coil (warlock). A breakable one only on a champion nobody but its holder hits,
   no group DoT; none DR-immune.
5. **Hand of Protection** only at 4+ champions, actual or predicted.
6. **Self Lay on Hands** held while the paladin's Divine Shield is ready.
7. **Summons** (Magma Totem, Fire Nova, Explosive Trap, Trap Launcher) count as splash in the FC AoE ban.
8. **Reader:** `--stands` moving share by mover, healing per healer, raid healing and damage taken per
   second; `--peel` counts the new spells. Tests, docs.

## Targets (next 25H)

Healers moving ≤ 25%, ranged ≤ 20%; raid healing ≥ 8k/s; the new peels cast and landing; no hold walk; no
Hand of Protection under 4 champions; no self Lay on Hands with Divine Shield ready; no summon splash while
live; 25N still kills.
