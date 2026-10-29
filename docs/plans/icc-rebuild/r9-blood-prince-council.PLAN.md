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

One lead, no split: the stem shares one state, one hazard scan and one tick.

1. Rewrite the stem on one per-instance state (`bpc.*`): princes by the instance guid slots 19-21,
   one scan per 200 ms (bombs, nuclei, vortices, balls), the tick doing every assignment.
2. Direct targeting `GetBpcTarget`: skull, a bomb shot, the tank's owned prince, else the empowered
   prince. No icon writes; the skull rule and both `bpc.skull` writes go.
3. Tanks by roster: 3+ tanks V / K / T, 2 tanks V+T / K, 1 tank V+T. Owned princes held at
   navprobed spots; recovery taunt only below DR level 3; generic taunts blocked.
4. Keleseth's tank collects nuclei to 6 Shadow Resonance (taunt within 30 yd, walk outside his
   phase); tank defensives through `NextTankDefensive` in his empowered phase, under 40% for all.
5. Spread: one navprobed 13.5 yd lattice. Healers keep nodes covering every tank spot, ranged take
   nodes in range of the empowered prince, everyone else scatters onto free nodes during Empowered
   Shock Vortex. Nodes near a Shock Vortex are blocked; bots without a node dodge it.
6. Kinetic Bomb duty per bomb (hunter, mage, other ranged), shooting only when it falls low.
7. Balls: the victim steps clear of everyone; up to 3 soakers intercept a Ball of Inferno Flame.
8. Heroic Shadow Prison: generic movers held all fight. Splash hold near a non-empowered prince or
   a nucleus. Lust in the Keleseth phase, opener on Valanar, shadow and fire auras (hooks for the
   merge stage).
9. Reader and tests for the new keys, the boss doc with the mechanics table.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Tank spots at the stair foot, Valanar (and Taldaram with 2 tanks) south, Keleseth north, 40.5 yd apart; Taldaram's own tank 13.5 yd west of Valanar's | WT diagram; the script's 12 yd vortex | the old spots 51 yd apart, out of ranged reach of both |
| One triangular lattice, 13.5 yd = 12 + two park tolerances, the tank spots on it | Empowered Shock Vortex 72038 radius 12, Saurfang's spacing rule | rings round Valanar (old), which ran out of room on 25-man |
| Ranged move to the empowered prince's side on a transfer, healers never | spell range 30 vs spots 40.5 apart | one fixed spread: only ~6 nodes reach both sides |
| Melee scatter only during the cast, onto free nodes reached before it ends (7 yd/s less 0.25 s, at most 28 yd) | guide (back wall); 72039 4.5 s, heroic 73038/73039 4 s | melee hold nodes all fight: no melee uptime; a flat 28 yd (review: late on heroic) |
| Bomb shot within 5 s of landing (10 yd normal, 15 heroic) and while it carries no Kinetic Bomb Knockback | script: falls at walk speed, 2 yd/s, 3 on heroic (38776/38777 `speed_walk` 1.2), from 20 yd; a hit lifts it 2 yd/s for 3 s; explodes within 2 yd | a flat 10 yd (review: 3.3 s on heroic, short of a mage's cast); old: shoot all the time |
| Area damage held near a nucleus though area searches skip it (`creature_immunities` -363 `ImmuneAoE`) | the splash test can't split cleaves and chains, which take it, from plain area damage | hold only cleaves and chains near a nucleus: needs a chain test in the shared `RaidSplash.cpp` |
| A soaker within 6 yd of the ball's path and 16.5 yd of its victim holds there, reach blocked (`icc bpc soak hold`) | lessons: a hold that lets go on arrival | release on arrival, so reach walks it back to its target (review) |
| Bomb tiers hunter, mage, other ranged DPS | guide (hunter); only direct hits proc Unstable | old: hunter, druid, any |
| Inferno soak by up to 3 interceptors, melee first | WT, Wowhead (soak the path) | old: step toward a ball 2-10 yd from Taldaram, which ticks nothing before the chase starts |
| No recovery taunt at DR level 3 or above; threat holds the princes | `flags_extra` 0x80000 | old taunt loop into immunity |
| Lust in the Keleseth phase | WT | lust at the pull |
| Heroic: generic movers held all fight instead of a stack threshold | Shadow Prison stacks on every moving second | old 12/18 stack thresholds |
| No opening positions: every stand opens once Valanar engages | Valanar's engage pulls all three in one update | pre-pull stands, which pull bots that only walk through |
| Nodes block heal reach; resurrect reach passes | Saurfang's slot hold | heal reach free, walking healers off the spread |
| A tank's own prince and a bomb shot outrank a player's skull | Rotface's kiter precedent | skull first for every bot |
| Floor snaps removed, not wrapped | knockbacks land on the floor | keep them behind `IccRaidCheat` |

All tasks done.

## Carried over

- Merge stage, `HookRows()`: `{&IccBloodPrincesDefinition, &IccBloodPrincesBurstWindow,
  &GetBpcOpenerBosses, &IccBloodPrincesResistSchools, nullptr}`, in definition order.
- README: BloodPrinces joins the direct-targeting users (`GetBpcTarget`) and the defensive users
  (`icc bpc tank defensive`: Keleseth's empowered phase, under 40%, class buttons held to 25%,
  `bpc.defensive`, `bpc.humancall`); its stem keeps `bpc.tank`, `bpc.slot`, `bpc.bomb` and `bpc.soak`
  in its own `RaidInstanceState`; `bpcSpreadLockTimers` joins the unused list; trigger names `icc bpc`
  dodge, tank defensive, ball, prince taunt, nucleus, soak, target, stand; drop the council from
  "Targeting is by raid icon outside".
- r14: unused `bpcSpreadLockTimers` and the council ids in `ICCData.h`; hoist one walk base;
  `test_icc_naming.py`'s synthetic `icc bpc kinetic bomb` names no registered trigger.

## Known gaps

In the boss doc's Known gaps, the review's included (no threat brake past taunt DR level 3). Never
built, no 631 trace, no `-Werror` pass beyond `pb-check`.

## Blocked
