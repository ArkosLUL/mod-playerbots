# r12-sindragosa — Sindragosa, her gauntlet, Rimefang and Spinestalker

Wave 3f, alone. Rules, conventions and the boss lane recipe: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md).
Layout and seams: `docs/raids/icecrown-citadel/README.md`, "Code layout". It may split.

## Encounters

- **Sindragosa:** slug `sindragosa`, prefix `sg.`, boss id 11, stem `Sindragosa`. Core
  `boss_sindragosa.cpp`, which also holds Rimefang and Spinestalker. Doc `sindragosa.md`, reader
  `sindragosa.py`. Guides in the cache: Warcraft Tavern 25 and 10, instant-carry's raid overview,
  Wowhead if saved.
- **Rimefang and Spinestalker:** no boss state. Use `r1`'s unit gate, with the slug and prefix of
  Sindragosa's encounter.
- **Sindragosa gauntlet:** slug `sindragosa-gauntlet`, prefix `sg.`, boss id 13. Core
  `icecrown_citadel.cpp`. Reader `sindragosa_gauntlet.py`. Guide: the mischiefmagicandrocketscience
  blog. It has no node today.

## Facts as of `824468439`

Sites are named by function.

- The action stem was 1635 lines (`ICCActions_SG.cpp`). Her trigger finds her with a 200 yd
  `FindNearestCreature`. The phase comes from her react state (`IccSindragosaPhase`).
- 24 icon writes (`SgSetIcon`).
- What is handled today:
  - the tank spot, drag and facing;
  - the Blistering Cold run;
  - Unchained Magic and Chilled to the Bone;
  - beacon tomb spots and air-phase hide groups (none probed);
  - the last phase's line-of-sight hide and tomb burn or hold.
- `IccSindragosaMultiplier`:
  - always: the tanks' AttackRti;
  - below 95%: Formation, Flee, Follow and Starfall;
  - heroic Unchained Magic: everything but movement and the Unchained Magic action;
  - Blistering Cold: everything but its action and heals;
  - a beacon: everything but the beacon action;
  - allowlists for tanks in the last phase, and a list of about 25 classes over the bomb station.
- `IccSindragosaGroupPositionTrigger` acts inside `IsActive`:
  - `UseNitroBoosts`;
  - the raid cheats:
    - `sg.frostbeacon`: the main tank's last-phase beacon strip;
    - `sg.mysticbuffet`: the tanks' Mystic Buffet strip;
    - `sg.magicbarrier`: Magic Barrier on tanks;
    - `sg.nothreat`: 1% Threat on assist tanks.
  - `r1` moved its buffs out; the cheats are this lane's.
- Other cheats:
  - `sg.flares`: smoke flares from `StoreNewItemInBestSlots` in `TryDropTombFlares`. Red flares burn
    out after 5 min.
  - `sg.tombstrip`: tomb aura strips in `IccSindragosaMysticBuffetAction` and
    `IccSindragosaFrostBombAction`.
- 18 positions, none navprobed.

## Guide answers to build on

- **Tanks:** 2 rotate, and a tank free of Mystic Buffet swaps in before every Frost Breath. Personal
  cooldowns, Frost Resistance, and Hand of Freedom against the Frost Breath slow.
- **Air phase:** Frost Beacons (heroic one more than normal; check the script) turn into Ice Tombs
  placed 10 ft apart in 3 rows of 2. They are cover for 4 Frost Bombs about 7 s apart, so don't break
  them early (Asphyxiate after 20 s).
- **Unchained Magic:** stay at 2-3 stacks or fewer. On heroic, Instability bursts within 20 yd, so the
  debuffed move out of the raid.
- **Blistering Cold:** run out.
- **Last phase:** a Frost Beacon every 15 s. Hide behind a tomb from Mystic Buffet and stay at 3
  stacks or fewer; auto attack must be off for the line of sight to count.
- **Setup:** attack from the sides, 1-2 tanks (2 to swap in the last phase), 4-8 healers.

## Focus

- The tank swap timed to Frost Breath.
- Tomb spots computed from the beacon count and navprobed, with line-of-sight checks for the bomb
  hide.
- Unchained Magic stack control and the heroic run out.
- Blistering Cold, and Chilled to the Bone stacks.
- The last phase's tomb hide with auto attack off, and tomb break timing.
- The Frost Resistance aura through `r1`'s election.
- Replacing all six raid cheats and the flares.
- The gauntlet and the two drakes. Direct targeting.

## Owns

Stem `Sindragosa` and its definition (and one for the drakes and the gauntlet if needed), the doc, both
readers and their tests, this brief.

## Task list

Split into three groups on disjoint files plus a finisher. A group edits only its files and reports
the rows, keys and seams it added; the mechanics table in `sindragosa.md` holds every id, timer and
radius. Reference patterns: `ICCHelpers_BloodQueen.cpp` (one `RaidInstanceState`, scan and duty
ticks, latched stands, brake, tank roster, `IccBqlBrakeStopAction`, `IccBqlAntiFear*`), the Blood-Queen
definition (rules), `EncounterHelpers` (`FindNearestPositionClearOfHazards`, `ValidateStaticFloorPoint`,
`UseNitroBoosts`).

**Positions**, navprobed `--nav 0x09` (settled Z within 0.03 of 203.38 unless given), static sight
clear from S and the unchained spots to T and her, from the hides to T. Ground stands are polar from
her live position B in the room frame (θ from +x toward +y, r in yd); she lands at L (4419.19,
2484.57, 203.385) facing her tank. Probed at B = L and 10, 20, 30 yd west of it: all on the floor
but the unchained healer spots θ35/θ25 at 20 yd and all three at 30 yd, so validate each at runtime
(`ValidateStaticFloorPoint`) and fall back to the nearest valid one.

| Stand | θ, r |
|---|---|
| T, the holder | 90, 20 |
| Tout, the holder's Blistering Cold run | 90, 30 |
| M, melee and the free tank | 170, 14 |
| S, ranged and healers | 180, 30 |
| escape for the rest | nearest of 150/180/210, 30 |
| heroic unchained, healers | 15, 29; 35, 30; 25, 38 |
| heroic unchained, others | 325, 34; 315, 40; 340, 40 |
| last-phase tomb spots A, B | 140, 31; 215, 31 (hides 4 yd behind: r 35) |

Air phase, absolute: beacon spots 25-man (4362, 2474.57), (4362, 2494.57), (4368, 2474.57), (4368,
2494.57), (4362, 2484.57), (4368, 2484.57) in that order; 10-man (4364, 2478.57), (4364, 2490.57);
stage on the stairs (4346.0, 2484.57, 206.2). The stairs run from x 4325 (z 211) to 4358 (z 203.4);
paths S → stage, T → stage and stage → spots are `PATHFIND_NORMAL`. Drakes: Rimefang tank (4413.31,
2446.42), melee (4405.31, 2456.42), ranged and healers (4391.31, 2456.42); Spinestalker tank (4418.90,
2524.23), melee behind him (4418.90, 2506.23), ranged and healers (4384.13, 2526.89), all z 203.38.

**Group 1, `sg-boss`**: `Util/ICCHelpers_Sindragosa.{h,cpp}`, `Trigger/ICCTriggers_Sindragosa.{h,cpp}`,
`Action/ICCActions_Sindragosa.{h,cpp}`, `Multiplier/ICCMultipliers_Sindragosa.{h,cpp}`,
`Definition/ICCDefinition_Sindragosa.cpp` (all under `src/Ai/Raid/ICC/`). Rewrite rather than patch.

1. One `RaidInstanceState`: her unit by instance guid slot 11, a 200 ms scan within 150 yd of L for
   tombs (entry 36980; check `GetEntry` on the difficulty entries) and Frost Bomb markers (37186 with
   70022), duties every 250 ms, a wipe reset. Keep `IccSindragosaPhase` (`sg.phase`). Every predicate is
   false unless she exists and the bot is within 120 yd of L: the gate is open between pulls and
   through the drakes.
2. Remove the six raid cheats, the flares, every side effect in `IsActive`, every icon write and `rti`
   use, and the old multiplier. Keep the `EnableICCBuffs` buffs in the tick, same behaviour and keys.
3. Tanks `sg.tank` (1 holder: her victim when a group tank, else the group's main tank; 2 free). The
   holder stands at T and taunts her back when she is on someone else (no taunt DR); the free tank
   fights from M, braked; generic taunts blocked. Last phase, with a bot free tank at Mystic Buffet ≤ 1:
   it walks to T from 12 s after the last breath and taunts once at 16 s (`sg.swap`), then roles flip.
   The breath clock is the first tick seeing her cast either breath row (`sg.breath`, a count).
4. Stands (`sg.stand`), latched: park 1 yd, walk back past 2.5 yd, re-latch once she drifts 3 yd. Before
   live (she flies in), the same stands round L.
5. Blistering Cold (`sg.cold`): while she casts row 1882 and the bot is within 29.5 yd 2D of her, a forced
   run (interrupt the cast, Nitro Boosts): holder to Tout, heroic debuffed to its unchained spot, the
   rest to the nearest escape spot.
6. Unchained Magic (`sg.unchained`): a 69762 carrier with Instability 69766 ≥ 3 holds every spell until
   Instability falls off; confirm the hold reaches the healer engine's casts. Heroic: each carrier walks
   to its own unchained spot (healers first set, others second) and also holds while any living
   non-carrier is within 23 yd; the stand holds while it carries Unchained Magic or Instability.
7. Chilled to the Bone (`sg.chilled`): a non-holder at 6 stacks of 70106 drops her and stops auto attack
   and auto shot until the aura is gone.
8. Brake (`sg.brake`, `ObsGuidSet`): non-holders at 90% of the holder's threat, freed at 80%.
9. Air phase. At takeoff everyone but beacons walks to the stage. Beacons (70126, `sg.beacon` spot 1-6)
   walk to their spot by guid order with Nitro Boosts. A non-beacon within 12 yd of a beaconed player
   dodges (`sg.splash`). Once tombs exist and a marker is live: hide 3.5 yd behind a tomb on the marker's
   far side, the hide nearest the bot among tombs 3 yd or more off the marker with a floor point, latched
   per marker (`sg.hide` `bomb <tomb>`). Count markers per air phase (`sg.bombs`). Tombs stay held
   (`sg.tombhold`) until four were counted and none is live, or she left both stations and none is live.
10. Tomb damage. A held tomb (air phase before release; the newest last-phase tomb while it is cover)
    takes damage only down to 20%: the resolver gives the bot's hide tomb, ranged also any other above
    20%; a live multiplier zeroes DoT casts on a held tomb (match narrowly) and area damage within 12 yd
    of one (`SpellSplashes`). Released air tombs die tanks first, then healers, then lowest health; ranged
    DPS break ground leftovers, last-phase tombs other than the newest, and any whose prisoner has
    Asphyxiation 71665 or is under 40% health.
11. Last phase. The beacon walks to the A/B spot without a live tomb (A when both are free; `sg.beacon`
    7 or 8). At Mystic Buffet (row 2193) ≥ 3 a non-holder hides 4 yd behind the newest tomb from her
    until the aura drops or the tomb dies (`sg.hide` `buffet <tomb>`), with no target and auto attack off.
    Healers hide only when the hide keeps the holder within 38 yd; the swapping tank never; a heroic
    carrier only without Instability.
12. `GetSgTarget` (`sg.target`, rule and unit): a player's skull; nothing for a held, braked, chilled or
    hiding bot; air tombs per 10; ranged on the ground tombs per 10; else her. `OwnTargeting`, and a
    `Block` of the pickers, follow and pet pickers while the resolver holds a bot.
13. Tank buttons (`sg.defensive`, `sg.humancall`): the holder on each breath cast in the last phase from
    3 Buffet stacks, or under 50%; any tank under 40%; class buttons held to 25%; a human tank called.
14. Hand of Freedom by name: on each tank carrying Frost Breath in the first 6 s of an air phase, and on
    the holder at 3 breath stacks on the ground. Healers put Renew, Rejuvenation, Riptide or Power Word:
    Shield on beaconed members before the freeze.
15. Rules: `OwnMovement` over every Mover row, spell movers blocked while one holds, `AvoidAoe` and
    `Taunt` (tanks) blocked while live, the tank defensive hold, tanks' caster form blocked.
16. Hooks for the merge stage, in the helpers: `IccSindragosaBurstWindow` (lust in the last phase),
    `GetSgOpenerBosses` ({36853}), `IccSindragosaResistSchools` (frost).
17. Priorities: `ACTION_EMERGENCY` + 6 cold, + 5 splash, + 4 bomb hide, + 3 beacon walk; `ACTION_RAID`
    + 10 tank defensive, + 9 swap, + 8 buffet hide, + 7 unchained walk, + 6 Hand of Freedom, + 5 beacon
    heals, + 4 hold, + 3 target, + 2 stand.

**Group 2, `sg-adds`**: `Util/ICCHelpers_SindragosaAdds.{h,cpp}`, `Trigger/ICCTriggers_SindragosaAdds.{h,cpp}`,
`Action/ICCActions_SindragosaAdds.{h,cpp}`, `Multiplier/ICCMultipliers_SindragosaAdds.{h,cpp}`,
`Definition/ICCDefinition_SindragosaDrakes.cpp`, `Definition/ICCDefinition_SindragosaGauntlet.cpp`.
Trigger names start `icc sindragosa drake` and `icc sindragosa gauntlet`.

1. `IccSindragosaDrakesDefinition()`: `ICC_BOSS_NONE`, `UnitGate<37533, 37534>`, trace `sindragosa`; its tick
   calls `IccMarkPull(bot, drake, "sindragosa")` while live. Drakes by guid slots 25 (Rimefang) and 24
   (Spinestalker).
2. `GetSgDrakeTarget` (`sg.drake.target`): a skull, then the in-combat drake whose victim is in the group,
   Rimefang first; tanks `sg.drake.tank`, the main tank on the first.
3. Stands (`sg.drake.stand`), latched, per the positions above while the drake is landed.
4. Rimefang up (`sg.drake.air`): non-tanks braked at 90%/80% of the top tank (`sg.drake.brake`); a forced
   dodge (`sg.drake.dodge` `blast`/`puddle`) out of 9.5 yd of each live 38223 and of the destination of a
   71376 he is casting.
5. Spinestalker: anti-fear on Bellowing Roar as the Blood-Queen does.
6. Tank buttons (`sg.drake.defensive`, `sg.drake.humancall`): any drake tank under 40%, Rimefang's tank on
   each Frost Breath cast under 60%; class buttons held to 25%.
7. `IccSindragosaGauntletDefinition()`: boss 13, `BossStateGate`, trace `sindragosa-gauntlet`.
   `GetSgGauntletTarget` (`sg.gauntlet.target`): a skull, then in-combat adds with a victim in the group,
   Sorceress 37229, Webweaver 37502, Champion 37501, Warrior 37228, Broodling 37232; tanks first on a
   Champion or Warrior hitting a non-tank, taunted (`sg.gauntlet.taunt`). `OwnTargeting` pickers
   `DpsAssist | TankAssist`, so area damage stays free.
8. Blade dodge (`sg.gauntlet.dodge`): a forced hazard-clear move out of 14.5 yd of each live 38199.

**Group 3, `sg-readers`**: `tools/botobs/bosses/sindragosa.py`, `tools/botobs/bosses/sindragosa_gauntlet.py`,
`tools/botobs/tests/test_sindragosa.py`, `tools/botobs/tests/test_sindragosa_gauntlet.py`.

1. Read the keys named in groups 1 and 2. Per-bot keys are change-only and note `none` when their duty
   ends; `sg.tank`, `sg.beacon`, `sg.drake.tank` are `ObsGuidMap<uint8>`, `sg.brake`, `sg.drake.brake`
   `ObsGuidSet`, `sg.breath`, `sg.bombs`, `sg.tombhold`, `sg.drake.air` `ObsValue`.
2. `sindragosa.py`: drop the icon, flare and `losmove` views; sections for phases, tanks (roles, breaths,
   swaps, buttons, brake), stands, Blistering Cold runs, Unchained Magic, Chilled to the Bone, the air
   phase (beacon spots, bombs per phase, hides per bomb, tomb hold), last-phase hides, targets, drakes,
   cheats. A `sindragosa` trace is a drakes pull (`sg.drake.*`) or a boss pull (`sg.phase`): the banner
   lists only the silent keys of the kind it is, and spares `sg.gauntlet.*` and the unused
   `IccInstanceState` keys `sg.group`, `sg.tomb`, `sg.freedfallback`, `sg.flaredblue`.
3. `sindragosa_gauntlet.py`: keep `--waves`, add target, taunt and dodge views; its banner names only
   silent `sg.gauntlet.*` keys (`silent_keys` matches `sg` to both slugs).
4. Synthetic-pull tests per section, and the `EveryView` check on the full fixture as the Blood-Queen
   tests do.

**Finisher**: integrates, runs the lane checks over every changed and new file, reviews through the wave
script's lenses, fixes, folds `sindragosa.md` (sections after the mechanics table describe the old code
until then), records Decisions, Carried over and Known gaps, commits. New `.cpp` files mean a CMake
re-run.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Raid on her south flank toward the stairs, holder 20 yd west, stands polar from her live position | WT and WH diagrams: raid on her flank toward the stairs; blog: parallel to the stairs | old ranged spot north of her |
| No recentring; she drifts about 7 yd toward the holder per last-phase Blistering Cold | each landing resets her to L; a recentre crosses her front or swings her tail over the raid | holder drags her round her north flank |
| Free tank fights from the melee stand under the brake; last-phase swap 16 s after each breath | WH: swap a few seconds before each breath | WH: every 3 breaths with cooldowns |
| Unchained Magic hold at 3 Instability | WT 2-3 | WH 3-5 |
| Heroic carriers on her north flank, healers north-west in reach of the holder, the rest north-east past the tail zone, 23 yd off the raid | WH: DPS tail side, healers head side; Backlash 20 yd plus size | WT: a ring of 20 yd circles round the raid |
| Chilled to the Bone stop at 6 | WT, WH 5-8 | 8 |
| Blistering Cold run past 29.5 yd | 25 yd plus 1.5 target size plus margin | old 33 yd |
| Air layout at the stair foot, raid staged on the stairs, a hide 3.5 yd behind a tomb per bomb | WH; WT 2 rows of 3; bombs land at x 4350 or more | old tomb spots on the north edge |
| Tombs held at 20% with DoTs and area damage barred, broken after the 4th bomb, tanks then healers then lowest | WH 20% | the tomb strip cheat |
| Last-phase tombs on two alternating spots 31 yd off her at 140° and 215°, hides 4 yd behind | WH: outer circle, alternating sides, away from melee; Blistering Cold 25 yd, her melee 21.6 yd | one fixed hide (old LOS2) |
| Mystic Buffet hide at 3 stacks; healers only where the holder stays in reach | WT, WH 3; WH: healers reset on the side that keeps the tank in sight | everyone hides |
| Older last-phase tombs broken by ranged once a newer one exists, or on Asphyxiation or a prisoner under 40% | WH | break each on spawn |
| Hand of Freedom at air-phase start and on the holder at 3 breath stacks | WH | whenever a tank has the debuff (old) |
| Smoke flares dropped with nothing in their place | Humans decision: humans hold no duty; the flares came only from the cheat | bots carrying flares |
| Tank buttons on last-phase breaths from 3 Buffet stacks or under 50%, any tank under 40%, class buttons to 25% | WH cooldown rotation per breath | class buttons free |
| Lust in the last phase | WH | at the pull |
| Rimefang first | blog | Spinestalker first |
| Spinestalker's melee behind him | script: `spell_cone` -82 on 71369 is a 278° front arc, 71370 a 24° front cone | the guide's flank, hit on 10-man |
| Spinestalker's ranged and healers 37 yd off him | Bellowing Roar 35 yd | anti-fear alone |
| Gauntlet kill order Sorceress, Webweaver, Champion, Warrior, Broodlings, area damage free | blog: AoE the spiders, then the Ymirjar | generic |
| The drakes and the gauntlet get definitions of their own | brief; her `BossStateGate` closes while the gauntlet's state is in progress | rows in her definition |
| Opening positions round her landing spot during her fly-in | Pre-pull decision | none |
| Gauntlet triggers named `icc sindragosa gauntlet` | the naming tests need an owner; the gauntlet's `ICC_PREFIXES` row is the merge stage's | `icc frostwing gauntlet` |
| Resolver: tomb picks before the brake and the chill; unchained-held and buffet hiders get nothing even under a skull; a skulled tomb is never held | Permeating Chill and threat only matter on her | brake first |
| Holder fallback: her victim if a group tank, else the last holder while alive and untombed, else the group main tank, else the first untombed bot tank | brief task 3 plus a tombed holder can't taunt | main tank only |
| Bots outside 29.5 yd hold still during Blistering Cold | nothing may walk them in | free movement |
| Splash dodge to the bot's next destination when clear by 13.5 yd, else a sweep retried after 1 s; a stand within 12 yd of a beacon waits | a sweep costs up to 240 navmesh queries | sweep every tick |
| Between bombs bots hold at their last hide and keep damaging its tomb above 20% | WH: damage tombs slowly while hiding | back to the stage |
| One Mystic Buffet hide try per newest tomb | reach and floor change only with a new tomb | retry each tick |
| The swapper needs its taunt ready when elected at 12 s; a missed taunt is not retried before the next breath | one swap per breath | retry |
| Hand of Freedom from the nearest paladin within 30 yd with it ready, one per tank | WH | any paladin |
| Beacon heals: priest Renew then Power Word: Shield (not on Weakened Soul), druid Rejuvenation, shaman Riptide | WH HoTs and shields | Renew only |
| Runtime floor checks for hides and unchained spots only; stands and beacon spots rely on the brief's navprobe | hides follow tombs and markers, unchained spots her drift | check every stand |
| In the last phase ranged don't pre-damage the newest tomb | it is the raid's only Buffet cover | damage to 20% |
| Air tombs broken early on Asphyxiation only, never for a prisoner under 40% | the tomb soaks 99% of its prisoner's frost damage, and its 16k hit alone leaves casters under 40% | the brief's 40% rule in the air too |
| Drakes: no pre-pull positions; stands once a drake fights and has landed | both land defensive; brief task 3 "while landed" | stands round each landing spot during the fly-in |
| Free drake tanks braked at 90%/80% all fight and their generic taunts blocked; the owner keeps its taunt | a free tank that took a drake would turn its breath, cleave or tail onto melee | brake only while Rimefang flies |
| Drake brake against the owner, else a tank victim, else the top-threat tank | "top tank" alone could land Rimefang on a free tank | top tank |
| Icy Blast impact a hazard up to 4 s after the cast or until its puddle shows; the cast's own target ignores the live cast | core: `Spell::_cast` takes the destination at cast end, 71376 flies 15 yd/s, `spell_rimefang_icy_blast` summons on hit | the cast destination only |
| Drake melee and ranged stands drop once the drake is 8 yd off its landing spot | absolute stands would strand melee out of reach | keep them |
| Gauntlet generic taunts free; only the Champion and Warrior rescue is elected | no taunt DR on these adds | block generic taunts |
| Gauntlet: nearest add within a tier; a tank's rescue before the kill order | keeps healers alive | kill order first |
| Gauntlet: no reach or charge for melee and tanks whose target stands within 14.5 yd of a blade | reach would walk them back into the whirl | none |

## Carried over

- Merge stage: `IccSindragosaDrakesDefinition` and `IccSindragosaGauntletDefinition` into
  `ICCDefinitions.h` (declared, listed before `IccSindragosaDefinition`) and into
  `RaidIccStrategy::InitMultipliers` after it; the new stem headers into the umbrellas; `HookRows()`
  `{&IccSindragosaDefinition, &IccSindragosaBurstWindow, &GetSgOpenerBosses, &IccSindragosaResistSchools,
  nullptr}`; `ICC_PREFIXES` `"icc sindragosa gauntlet": "sindragosa-gauntlet"` ahead of `"icc sindragosa"`.
- Merge stage, once `ICC_PREFIXES` has the gauntlet row: `tools/botobs/tests/test_icc_naming.py`
  `NODE_PER_PREFIX` gains an `icc sindragosa gauntlet ...` node.
- Merge stage, README "Code layout": the `SindragosaAdds` stem and its two definitions; direct targeting
  users `GetSgDrakeTarget`, `GetSgGauntletTarget`; tank defensive user `icc sindragosa drake tank
  defensive` (`sg.drake.defensive`, `sg.drake.humancall`); trigger prefixes `icc sindragosa drake`,
  `icc sindragosa gauntlet`.
- Optional, merge stage: a drakes `HookRows()` row resisting frost (Rimefang's Frost Aura 71387).
- CMake re-run: five new `.cpp` files.
- r14: `IccInstanceState`'s `sg*` members (`ICCScripts.h`) and `NPC_TOMB1-4`, `SPELL_FROST_BEACON`,
  `SPELL_ICE_TOMB`, `SPELL_FROST_BOMB_VISUAL`, `SPELL_BLISTERING_COLD1-4` in `ICCData.h` go unused
  (`SPELL_MAGIC_BARRIER`, `SPELL_NO_THREAT` too here: check other users); hoist one walk base
  (`IccSindragosaBossWalkAction`, `IccSgAddsWalkAction`) and the helpers shared with Svalna and
  Valithria (`IsTankPlayer`, `IsDutyBot`, `MembersOnMap`, `SlotCreature`).

## Known gaps

Also in `sindragosa.md`.

- Never built, no 631 trace, no `-Werror` pass.
- One bot tank: no last-phase swap. A swap taunt that misses leaves the swapper on T into the breath.
- Heroic: the other carriers' spots are about 61 yd from S, reached only by carrier healers; a carrier
  past its set's three spots gets none and holds while anyone is within 23 yd.
- Hiding relies on ice block GO collision, not checkable offline.
- The 150°/210° escapes sit within 6 yd of last-phase tomb spots A/B.
- A released air tomb the bot can't attack yields nothing rather than the next one.
- Tank druid caster form not blocked on the fly-in stands.
- Drakes: on a double pull DPS and healers follow Rimefang (Spinestalker's tank up to 73 yd off);
  Spinestalker's melee spot 40.4 yd from his ranged spot; a ranged Tremor Totem misses his melee; a
  puddle drops a stand up to 40 s with no other spot; melee idle while Rimefang flies; no frost resist.
- Gauntlet: no Frost Nova, Sorceress Ice Tomb or Web answer.
- Humans hold no duty.

## Blocked
