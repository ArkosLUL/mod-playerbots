# g9-blood-prince-council — close the Blood Prince Council's gaps

Gap wave `3cd-gaps`, after `3g`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r9-blood-prince-council` lane and its review: read
[r9-blood-prince-council.PLAN.md](r9-blood-prince-council.PLAN.md) first. Its decisions stand unless
closing a gap needs a change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/blood-prince-council.md` and in r9's
brief.

- One bot tank leaves Keleseth unowned: the core script and the guides decide who else can hold him,
  under the Gap lanes' one-bot-tank rule.

## Owns

Stem `BloodPrinces` and its definition, the doc, the reader `blood_prince_council.py` and its test,
this brief.

## Task list

One lead, no split: every change sits in the stem's one state, scan and tick.

1. Keleseth stand-in: with one tank player in the raid, a warlock bot (guide), else a DK or paladin
   DPS (class taunt), else another ranged DPS owns Keleseth (`bpc.tank` bit 2): his spot, its
   damage as threat, nuclei by a direct hit (`bpc.target` `nucleus`, `bpc.nucleus` `hit N`) or its
   taunt, `icc bpc lance defensive` on a Shadow Lance that would leave it under 25%.
2. Threat brake `bpc.brake`: non-owners at 90% of the empowered prince's owner (else holder), free at
   80%; `icc bpc brake` stops the swing, `icc bpc brake` multiplier holds its attacks and area damage,
   a Block keeps the pickers off; a fourth tank fights under it.
3. Bombs: a pet per bomb (`bpc.petbomb`, hunter, warlock, DK, mage); a melee DPS bot takes a bomb no
   ranged one can, with its walk in the window; `icc bpc direct hit` holds spells that deal no damage
   while the target is a bomb shot or a nucleus grab.
4. 25-man scatter: healers off Valanar's 28 yd zone with each tank spot reached by half of them; the
   dais nodes within his reach stay free of ranged in his phase; a runner with no free node takes
   the reachable node with the fewest vortex hits (`bpc.hits`).
5. A bomb shooter whose node reaches the bomb shoots from it.
6. Ball victim: the least crowded nearby node when no spot is clear (`bpc.ball` `fewest`); the Inferno
   victim's magic button or Dispersion on a catch that would leave it under 25% (`icc bpc flame
   defensive`, `bpc.flame`).
7. Glittering Sparks: one priest or paladin bot within 30 yd dispels each carrier (`icc bpc dispel`,
   `bpc.dispel`).
8. Reader, tests, boss doc; Berserk pacing and the soakers' cast windows closed by decision.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| One tank player: Keleseth to a warlock, then a DK or paladin DPS, then other ranged DPS; none with a human tank | WT, Wowhead (warlock tank); Gap lanes' class-taunt rule; Humans decision | a DPS with a class taunt first; melee DPS (Keleseth casts from 10 yd) |
| A ranged stand-in holds Keleseth's spot all fight; a melee owner still lets go once he parks | his 10 yd `AttackStartCaster`; r9's spot | release on park for every owner |
| Stand-in grabs nuclei by its own direct spells with non-damaging ones held, taunters by taunt | `npc_dark_nucleus` `DamageTaken` (direct damage only, 500M threat) | a per-class spell table |
| Brake reference is the empowered prince's owner, else his victim; a puller is braked too | Putricide's brake; a taunt below DR 3 is the only other way back | the victim only |
| Pets on bombs all fight, shooters unchanged | Wowhead, WT (pets juggle passively) | pets only when no shooter |
| Melee bomb duty only when no ranged DPS bot is free within 60 yd | Unstable 72059 procs on melee; 3D melee range 5 yd (bomb reach 0) | no fallback |
| Non-damaging spells held on a bomb shot or nucleus grab | Unstable's `spell_proc` `SpellTypeMask` 1; the nucleus grabs on direct damage only | DoT-first casters ranked last |
| Healers leave Valanar's 28 yd zone and the dais field, each tank spot reached by ceil(healers / 2) | 25-man: 15 nodes reach him (14 with Taldaram's own spot) against ~22 non-tanks | every healer covers every spot (r9), five healers in his zone |
| Dais nodes within 28 yd of his spot held for melee in his phase | WT diagram (melee spread area on the dais), WT and Wowhead (melee to the back) | ranged anywhere |
| A runner with no free node takes the reachable node with the fewest hits, sharing one if it must | 72039 hits only players within 30 yd of him, 72038 12 yd | stay put (r9) |
| Soakers keep soaking through an Empowered Shock Vortex cast | 25H Inferno unsoaked 132k against 8k a shared vortex | release them |
| Inferno victim: magic row but Cloak of Shadows, Dispersion, never an immunity or a healthstone | Flames 71393 `SPELL_ATTR0_NO_IMMUNITIES` and `SPELL_ATTR3_ALWAYS_HIT`; Wowhead (Divine Guardian, Dispersion); the victim mostly walks, which an item use stops | `GetRaidPersonalDefensive`, whose lethal branch spends an immunity |
| Glittering Sparks dispelled by the nearest priest or paladin bot within 30 yd, no walk, no Mass Dispel | WT, Wowhead (dispel at once); nodes block reach; the cone mostly hits his tank | Mass Dispel on three carriers |
| No Berserk pacing | 10 min Berserk on all three; lust already in Keleseth's phase | a late lust |
| A braked bot idles rather than hitting another prince | damage on a prince without the Invocation is threat only | let it hit the others |
| Loose nuclei join bombs under the drop target guard | a loose nucleus has no victim, so it is out of combat | none |

All tasks done. Every kept coordinate re-probed: the 41 nodes and both tank spots settle within 0.1 yd
(`--nav 0x09`), and 164 `los` rays from every node to the three tank spots and Keleseth's place are
clear. No coordinate added.

## Carried over

- Merge stage, README "Trigger names": the council's list gains flame defensive, lance defensive,
  dispel and brake (`icc bpc brake`, action `icc bpc brake stop`).
- Merge stage, README defensives bullet: the council adds `icc bpc lance defensive` (a Keleseth
  stand-in's `GetRaidPersonalDefensive` on `icc.personal` per lance), `icc bpc flame defensive` (the
  Inferno victim's magic row but Cloak of Shadows, or Dispersion, `bpc.flame`) and `icc bpc brake`, a `LiveMultiplier`
  holding non-owners at 90% of the empowered prince's owner's threat (frees at 80%).
- Merge stage, README per-instance state: the council adds `bpc.petbomb`, `bpc.dispel` (`ObsGuidMap`)
  and `bpc.brake` (`ObsGuidSet`).
- Merge stage, status row: r9's "one bot tank leaves Keleseth unowned", "no ranged DPS means bombs
  land", "25-man nodes can run short", "no threat brake", "Glittering Sparks left to class dispels" and
  "no Berserk pacing" are closed or decided here; the leftovers are in the boss doc's Known gaps.
- r14: unused `bpcSpreadLockTimers` and the council ids in `ICCData.h`; hoist one walk base;
  `CanPayFor` and the dispel pick duplicate Rotface's; `test_icc_naming.py`'s synthetic `icc bpc
  kinetic bomb` names no registered trigger.

## Known gaps

- Those in the boss doc.
- No 631 trace; the user built `623638b99` clean, this change needs a rebuild (no new `.cpp`, no CMake
  re-run); no `-Werror` pass beyond `pb-check`.

## Blocked
