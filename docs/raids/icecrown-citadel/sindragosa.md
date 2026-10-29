# Sindragosa

Boss id 11, trace slug `sindragosa`, prefix `sg.` ([README](README.md)). Code: stem `Sindragosa`
(`src/Ai/Raid/ICC/*/ICC*_Sindragosa.*`); the drakes and the gauntlet in stem `SindragosaAdds`, with
`ICCDefinition_SindragosaDrakes.cpp` and `ICCDefinition_SindragosaGauntlet.cpp`. Mechanics:
`boss_sindragosa.cpp`. Her gauntlet and her two drakes close the page.

## Mechanics

She is summoned once Rimefang and Spinestalker die, flies in, lands at (4419.19, 2484.57) and pulls
the zone; `IN_PROGRESS` from her `JustEngagedWith`, Berserk at 10 min. Combat reach 18.75, so her
melee reaches 21.6 yd from her centre; no taunt diminishing returns. Ids are 10N/25N/10H/25H, one id
for all four when no spelldifficulty row exists (none in the world DB either). "Landing" timers
restart at each touchdown. Radii are exact: a creature's or a player's area spell on players adds no
target size. Guides: Warcraft Tavern (WT), Wowhead (WH), the gauntlet blog.

| Ability | Script | Guide | Bot |
|---|---|---|---|
| Frost Aura 70084/71050/71051/71052 | 3000/4500/4500/6000 Frost per 3 s within 80 yd, all fight | Frost Resistance Aura | resist hook, frost |
| Cleave 19983 | 10 s, then 10-15 s (13-15 s after landing); her victim and its neighbours | stand at her side | only her holder in front |
| Tail Smash 71077 | 20 s, then 22-27 s (19-23 s after landing), 1 s cast; 11.25-18.75k and knockback within 20 yd of a point 20 yd behind her | stand at her side | no stand within 23 yd of that point |
| Frost Breath 69649/71056/71057/71058 | 8-12 s, then 20-25 s (7-10 s after landing), 1.5 s cast; 27.75-32.25k/37-43k/37-43k/55.5-64.5k Frost in a 60 yd cone, 75° on 10N (`spell_cone` row on 69649 only) and 24° otherwise; -50% attack speed and -15% run speed for 90 s, stacking | tank cooldowns, Frost Resistance; Hand of Freedom on the tank as the air phase starts | holder alone in front; Hand of Freedom; tank buttons |
| Frost Breath 73061/73062/73063/73064 | last phase, same timer; 10 s debuff, no slow | swap to a Mystic Buffet-free tank a few seconds before each breath, or every 3 with cooldowns | swap 16 s after each breath; buttons from 3 Buffet stacks |
| Permeating Chill 70107 | on every player, refreshed by 70109 every 5 s: 20% on a melee or ranged auto attack or melee-class hit on her for Chilled to the Bone 70106, 1000 Frost per 2 s per stack, 8 s refreshed | pause attacks at 5-8 stacks | non-holders stop at 6 until it drops |
| Unchained Magic 69762 | 9-14 s, then 30-35 s (12-17 s after landing); 30 s on one healer and one caster DPS (10-man), three each (25-man); every spell cast stacks Instability 69766 (5 s, refreshed), whose expiry casts Backlash 69770/71044/71045/71046, 2000 Arcane per stack, on heroic to allies within 20 yd as well | WT 2-3 stacks, WH 3-5; heroic: leave the raid, DPS to the tail side, healers to the head side | spells held at 3 stacks; heroic: a spot 23 yd off the raid, spells held while anyone else is within 23 yd |
| Icy Grip 70117, Blistering Cold 70123/71047/71048/71049 | grip at 33.5 s, 35-40 s after landing; last phase 35-40 s after it starts, then 65-70 s after each Cold. Pulls everyone within 100 yd and in sight onto her, except holders of Tank Marker 71038 (her melee hit in the last 30 s); 1 s later a 5 s cast, 30k/35k/45k/60k Frost within 25 yd | everyone runs past 25 yd | forced run past 29.5 yd, the holder along her front, the rest to her flank |
| Frost Beacon 70126 (air) | 50 s, then 110 s after each takeoff while above 35%: passive, up 20 yd, to (4476, 2484, 248); 69712 then 69675 beacon 2/5/2/6 random players not Frost-Imbued or immune | WH: beacons 2 by 2 at the stair foot, each pair over 10 yd from the next, raid up the stairs; WT: 3 rows of 2 (10-man a triangle) | pairs at the stair foot, everyone else staged on the stairs |
| Ice Tomb 70157 | 7 s after the beacon: 15.6-16.4k Frost and a stun on the target and everyone within 10 yd, other beacons included, then tomb 36980 (38320/38321/38322, 113k/453k/113k/453k) with ice block GO 201722 blocking sight; inside, -99% Frost damage taken; Asphyxiation 71665, 8% max health per s, after 22.5 s, about as the 4th bomb lands | stay 10 yd off a beacon; a pair takes 2x16k; HoTs on beacons; damage tombs to 20%, break them after the last bomb | 12 yd beacon dodge; HoTs and shields; tombs held at 20% with DoTs and area damage off them, broken after the 4th bomb or on Asphyxiation once no bot hides behind it from a live bomb |
| Frost Bomb 69846, 69845/71053/71054/71055 | from (4525.6, 2485.15, 245.1), 7 s after the beacons then every 6 s, four per air phase: a 1 s cast drops marker 37186 (aura 70022) at a random point x 4350-4425, y 2450-2525, 3 yd off ice blocks; 5.5 s later 23.6-26.4k/23.6-26.4k/188.5-211.5k/188.5-211.5k Frost to everyone in its sight. She flies back 5.5 s after the 4th cast, 1 s before it lands | hide behind a tomb | 3.5 yd behind the nearest tomb on the marker's far side |
| Mystic Buffet 70128, 70127/72528/72529/72530 | last phase, from 35% (latched; reached in the air, from touchdown), no more air phases; every 6 s +10/15/20/20% magic damage taken per stack, 8 s, to everyone in sight by GO models only; a player melee-attacking her within melee range is always hit | hide behind a tomb at 3 stacks, auto attack off | hide at 3 stacks, the free tank at 1, until it drops |
| Frost Beacon 70126 (last phase) | 7-10 s in, then 18-22 s: one random player within 80 yd who is not her victim and has no Tank Marker | WH: the tomb on the outer circle 25 yd off her, alternating sides, away from the melee; break the previous tomb once the next exists, within 20 s | two alternating spots 31 yd off her; ranged break older tombs |

## Phase

`IccSindragosaPhase(bot, boss)` reads her script's state and publishes `sg.phase`: `air` while in
combat and passive (takeoff to touchdown), `last` at 35% or under on the ground, else `ground`. She
comes from guid slot 11, and every predicate is false unless she is alive and the bot within 120 yd
of L: the gate stays open between pulls and through the drakes. A wipe or a kill resets the state.

## Layout

Ground stands are polar from her live spot, re-latched once she drifts 3 yd (θ from +x, north, toward
+y, west; r in yd), round L through her fly-in and nowhere while she idles there after a wipe.
Navprobed `--nav 0x09` at L and 10, 20 and 30 yd west (her drift toward the holder), settled Z within
0.03 of 203.38; static sight clear from S and the unchained spots to T and her, and from the hides to T.

| Stand | θ, r | Who |
|---|---|---|
| T | 90, 20 | the holder; the swapper from 12 s after a last-phase breath |
| M | 170, 14 | melee and the free tank |
| S | 180, 30 | ranged and healers |
| Tout | 90, 30 | the holder's Blistering Cold run |
| escape | nearest of 150/180/210, 30 | everyone else's run |
| heroic unchained, healers | 15, 29; 35, 30; 25, 38 | in heal reach of the holder |
| heroic unchained, others | 325, 34; 315, 40; 340, 40 | past her tail |
| last-phase tomb spots A, B | 140, 31; 215, 31 | hides 4 yd behind them from her |

Unchained spots and hides are floor-checked where used (`ValidateStaticFloorPoint`, 1.5 yd), else the
nearest valid spot of the set: the healer spots θ35/θ25 leave the floor at 20 yd of drift, all three
at 30.

Air phase, absolute: everyone not beaconed stages on the stairs at (4346.0, 2484.57, 206.2), south of
the bombs' x 4350; beacons take the stair-foot spots in guid order, 25-man (4362, 2471.57), (4362,
2497.57), (4368, 2471.57), (4368, 2497.57), (4362, 2484.57), (4368, 2484.57), 10-man (4364, 2478.07),
(4364, 2491.07). A pair 6 yd apart shares its Ice Tombs; pairs sit 13 yd apart, out of each other's
10 yd with both parked a yard short. Paths S → stage, T → stage and stage → spots are
`PATHFIND_NORMAL`.

## Duties

One `RaidInstanceState`: a 200 ms scan within 150 yd of L for tombs 36980 and Frost Bomb markers
37186 with 70022, duties every 250 ms, then per bot the holds, the brake and one walk, highest first:
Blistering Cold, splash, bomb hide, beacon, buffet hide, unchained, stand.

- **Tanks** (`sg.tank`): the holder is her victim when a group tank, else the last holder while alive
  and untombed, else the group's main tank, else the first untombed bot tank; other bot tanks are free.
  The holder taunts her back off a non-tank (no taunt DR); generic taunts are blocked.
- **Swap**: in the last phase, from 12 s after a breath, the first free bot tank untombed, unbeaconed,
  at Mystic Buffet ≤ 1 and with its taunt ready walks to T, taunts at 16 s (`sg.swap`), and the roles
  flip on the next duty pass. A taunt that misses isn't retried before the next breath. `sg.breath`
  counts her casts of either breath.
- **Brake** (`sg.brake`): on the ground, non-holders but healers and a swapper past its taunt, at 90%
  of the holder's threat until 80%. Nothing of theirs lands on her, nor area damage within 30 yd of her.
- **Blistering Cold** (`sg.cold`): while she casts it, bots within 29.5 yd run, forced, the cast
  interrupted, Nitro Boosts: the holder to Tout, a heroic carrier to its unchained spot, the rest to
  the nearest escape. Bots already out hold still.
- **Unchained Magic** (`sg.unchained`): a carrier at 3 Instability holds every spell, heals included,
  until Instability drops. Heroic: carriers walk to their own spot, healers to the first set and the
  rest to the second by guid, while they carry Unchained Magic or Instability, and hold while any living
  non-carrier is within 23 yd.
- **Chilled to the Bone** (`sg.chilled`): a non-holder at 6 stacks drops her, auto attack and auto
  shot until the aura is gone.
- **Air phase**: at takeoff all but beacons walk to the stage; beacons (70126, `sg.beacon` 1-6) walk
  to their spot, forced, Nitro Boosts. A non-beacon within 12 yd of a beaconed player or its spot moves
  to 13.5 yd (`sg.splash`): to its next destination when that is clear, else a hazard sweep retried
  after 1 s. Healers put Renew, Power Word: Shield, Rejuvenation or Riptide on beaconed members. Once
  tombs exist, each live marker sends every bot 3.5 yd behind a tomb on its far side, the hide nearest
  the bot among tombs 3 yd or more off the marker with a floor point (`sg.hide` `bomb`); between bombs
  bots hold at their last hide. Tombs stay held (`sg.tombhold`) until `sg.bombs` counted four and none
  is live, or she left both stations with none live.
- **Last phase**: a beacon walks to A, or to B while a tomb sits within 6 yd of A (`sg.beacon` 7, 8).
  At 3 Mystic Buffet stacks (the free tank at 1, so it meets the swap at 1 or less) a non-holder hides
  4 yd behind the newest tomb from her, auto attack off
  and no target, until the aura drops or the tomb dies, one try per tomb (`sg.hide` `buffet`). Healers
  hide only where the holder stays within 38 yd (behind A, not B), the holder and swapper never, a
  heroic carrier only without Instability.
- **Hand of Freedom**: the nearest paladin within 30 yd with it ready, one per tank, on each tank
  carrying Frost Breath in the first 6 s of an air phase and on the holder at 3 breath stacks.
- **Tank buttons** (`sg.defensive`, `sg.humancall`): the holder on each breath cast at 3 Buffet stacks
  in the last phase or under 50%, any tank under 40%; class buttons held to 25%; a human tank is called
  on the same windows.
- **Hooks**: lust in the last phase, opener boss 36853, frost resistance all fight.

`GetSgTarget` (`sg.target`), tanks and DPS: a player's skull on her or a tomb, except her for a braked
or chilled bot; nothing for a tombed, unchained-held or buffet-hiding bot. In the air, ranged break an
asphyxiating tomb no bot hides behind from a live bomb, then each bot takes its last hide's tomb above
20% and ranged also the highest
held one above 20%; once released, everyone breaks them. On the ground, ranged break any tomb but the
last phase's newest (air leftovers, older tombs, a prisoner asphyxiating or under 40%); else her, but
nothing for a braked or chilled bot. Tombs go danger first, then prisoner tank, healer, other, lowest
first.

Held tombs (air tombs before release, the last phase's newest): a live multiplier zeroes all damage on
one at 20% or under, mostly periodic casts at any health, and `SpellSplashes`/`SummonSplashes` area
damage within 12 yd; the hold node stops auto attack on one at 20%.

Rules: `OwnTargeting`; while the resolver holds a tank or DPS bot, a `Block` of the pickers, follow,
random moves and the pet picker; spells blocked while unchained-held; `OwnMovement` over every Mover
row, Attack kept; spell movers blocked; `AvoidAoe`, tanks' taunts and caster form blocked while live;
the tank defensive hold.

## Decisions

Source, then the choice:

- WT and WH diagrams: the raid on her south flank toward the stairs, the holder 20 yd west.
- Each landing resets her to L: no recentring. She drifts about 7 yd toward the holder per last-phase
  Blistering Cold, and the stands follow.
- WT and WH (a Buffet-free tank before each breath): the swap 16 s after each breath, her next coming
  20-25 s after it. WH (the tank comes fresh from behind a tomb): the free tank hides from 1 stack.
- WT 2-3, WH 3-5: Unchained Magic held at 3 Instability.
- WH (healers to the head side, DPS to the tail side) and Backlash's 20 yd plus sizes: heroic carriers
  on her north flank, 23 yd off the raid.
- WT, WH 5-8: Chilled to the Bone stop at 6.
- Blistering Cold's 25 yd plus margin: the run past 29.5 yd.
- WH (beacons 2 by 2, each pair over 10 yd from the next), against WT's three rows of two: pairs at
  the stair foot, the raid staged on the stairs.
- WH 20%: air tombs held at 20%, DoTs and area damage barred, broken after the 4th bomb, tanks then
  healers then lowest. Only Asphyxiation breaks one sooner, once no bot hides behind it from a live
  bomb: the tomb soaks 99% of its prisoner's frost damage, and its 16k hit alone leaves casters under
  40%.
- WH (the outer circle, alternating sides, away from melee): last-phase tombs on two spots 31 yd off
  her, outside her 21.6 yd melee and Blistering Cold.
- WH: Mystic Buffet hide at 3 stacks, healers only where the holder stays in sight; older last-phase
  tombs broken once a newer one exists. The newest is not pre-damaged.
- WH: Hand of Freedom at air-phase start and on the holder at 3 breath stacks; tank buttons per breath;
  lust in the last phase.
- Opening stands only through her fly-in: after a wipe she idles at L, out of combat.
- Humans hold no duty, and the smoke flares came only from the cheat: dropped.

## Cheats

None. Removed: `sg.nitro` (now the bot's own Nitro Boosts), `sg.frostbeacon` (a last-phase beacon
strip off the main tank, dead code: her selector skips her victim and Tank Marker holders),
`sg.mysticbuffet` (tanks' Buffet strip; now the swap), `sg.magicbarrier` (immunity on tanks; now the
Blistering Cold run and tank buttons), `sg.nothreat` (1% threat on assist tanks; now the brake),
`sg.flares` (smoke flares from `StoreNewItemInBestSlots`; dropped) and `sg.tombstrip` (held tombs'
aura strips; now the DoT and splash multiplier). No icon writes; a saved `rti` of cross or star
resets to skull once per instance.

Record-only, `EnableICCBuffs`: `sg.heroicbuff` and `sg.spitefulfury` on every member on heroic
(README).

## Known gaps

- No 631 trace and never built: every threshold is a model until a pull.
- One bot tank: no swap, so the holder takes every last-phase breath on its Buffet stacks.
- A swap taunt that misses leaves the swapper on T into the next breath.
- Heroic: raid healers on S are about 61 yd from the other carriers' spots, which only carrier healers
  reach. A carrier past its set's three spots gets none and holds while anyone is within 23 yd.
- 25H: a set's three unchained spots sit 9-17 yd apart, inside each other's 20 yd Backlash, so
  carriers hit each other. Spacing three per set 21 yd apart within heal reach of the holder and clear
  of her breath, tail and the raid across her drift needs a new navprobed layout; `--unchained` shows
  the hits.
- 10-man tombs keep 22.6k above their 20% floor, and ranged all on the highest held tomb can land more
  than that in flight past it. A tomb dying before `sg.tombhold` drops shows it.
- The splash dodge checks its destination, not the walk: the holder's line from T to the stage passes
  1.5 yd from 10-man spot 2, which a holder slowed by Frost Breath and missed by Hand of Freedom can
  cross as the tomb forms. A path check would turn the shortcut into a navmesh sweep per tick while
  beacons walk beside the raid.
- Hiding works through ice block GO 201722 collision, which navprobe can't model; her sight ray starts
  70% of her combat reach toward the bot. A healer behind A sees the holder past 2.3 yd of that tomb's
  centre.
- The 150° and 210° escapes sit within 6 yd of A and B, so a run can path round a tomb.
- A released air tomb the bot can't attack yields nothing rather than the next one.
- Tank druids' caster form isn't blocked on the fly-in stands, out of combat.
- Humans hold no duty; bots still dodge a beaconed human.
- r14: unused `IccInstanceState` `sg*` members (`ICCScripts.h`) and `NPC_TOMB1-4`, `SPELL_FROST_BEACON`,
  `SPELL_ICE_TOMB`, `SPELL_FROST_BOMB_VISUAL`, `SPELL_BLISTERING_COLD1-4` in `ICCData.h` (check
  `SPELL_MAGIC_BARRIER` and `SPELL_NO_THREAT` for other users); hoist one walk base
  (`IccSindragosaBossWalkAction`, `IccSgAddsWalkAction`) and the helpers shared with Svalna and
  Valithria (`IsTankPlayer`, `IsDutyBot`, `MembersOnMap`, `SlotCreature`).

## Targets for the next pulls

25N first: `--tanks` keeps one holder per ground phase with `sg.brake` on the free tank, and a swap
per last-phase breath; `--cold` shows every bot running (`tout`, `escape N`) and nobody hit; `--air`
has five beacons on spots, every bomb with hides, no Frost Bomb death, tombs held to the 4th bomb;
`--last` alternates A and B with hides at 3 stacks; `--chilled` peaks at 7 or under. 25H: the same with
six beacons, plus `--unchained` with no Backlash on non-carriers.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `sg.phase` | per bot | `ground`, `air`, `last` |
| `sg.tank` | traced map, tank bot | `1` holder, `2` free, `0` off |
| `sg.breath`, `sg.bombs` | traced value | her breaths; an air phase's markers, `0` at takeoff |
| `sg.tombhold` | traced value | `1` while air tombs are held |
| `sg.brake` | traced set | `1` braked, `0` free |
| `sg.beacon` | traced map, beaconed bot | air spot 1-6, `7` A, `8` B, `0` once it ends |
| `sg.swap` | per tank bot | `walk`, `taunt`, `none` |
| `sg.stand` | per bot | `T`, `M`, `S`, `stage`, `none` |
| `sg.cold` | per bot | `tout`, `unchained N`, `escape N`, `none` |
| `sg.unchained` | per bot | `carrier` or `spot N`, either with ` hold`; `none` |
| `sg.chilled` | per bot | `stop`, `none` |
| `sg.splash` | per bot | `dodge`, `blocked`, `none` |
| `sg.hide` | per bot | `bomb <tomb>`, `buffet <tomb>` (guid counter), `none` |
| `sg.target` | per bot | `skull`, `boss`, `break`, `hide`, `held`, `tombed`, `air`, `brake`, `chilled`, `none`; air picks `hide`, `tomb`, `release`, `break` with the tomb |
| `sg.defensive`, `sg.humancall` | per tank | the button, `idle`; the call to a human tank |

Change-only per bot. Blistering Cold has no hazard circle: her 5 s `cast` row times it.

`tools/botobs/bosses/sindragosa.py`: `--phases`, `--tanks`, `--stands`, `--cold` (per cast, the runs
and whom it hit), `--unchained` (per window, the rules, peak Instability, Backlash on non-carriers),
`--chilled`, `--air` (per air phase, beacon spots, dodges, hides per bomb, tomb hold), `--last`,
`--targets`, `--drakes`, `--cheats`. A `sindragosa` trace is a drakes pull (`sg.drake.*`) or a boss
pull (`sg.phase`); the banner names only the silent keys of its kind, never `sg.gauntlet.*` or the
unused `IccInstanceState` keys `sg.group`, `sg.tomb`, `sg.freedfallback`, `sg.flaredblue`.

## Sindragosa gauntlet

Boss id 13, trace slug `sindragosa-gauntlet`, prefix `sg.`, shared with Sindragosa.
`IN_PROGRESS` from the controller's `DoAction(ACTION_START_GAUNTLET)`, `DONE` when it dies after the
third wave, `NOT_STARTED` on its reset; it evades once no living player is within 100 yd. Three waves,
each once only Nerub'ar Broodlings 37232 are left: Nerub'ar Champions 37501 and Webweavers 37502 drop
from the webs, Frostwarden Warriors 37228 and Sorceresses 37229 walk in, then fresh spiders drop.
Broodlings come with every wave.

| Add | Script | Guide | Bot |
|---|---|---|---|
| Nerub'ar Champion 37501 | Rush 71801 on a target 8-25 yd off, every 10 s | AoE the spiders | kill order: Sorceress, Webweaver, Champion, Warrior, Broodlings; taunted off a non-tank |
| Nerub'ar Webweaver 37502 | Crypt Scarabs 71326 (2 s cast), Web 71327 (5 s root) | AoE the spiders | as above; no Web answer |
| Nerub'ar Broodling 37232 | 30 per wave, 0.35 s apart from 10 s in, landing 18-39 yd off the controller | AoE | area damage free |
| Frostwarden Warrior 37228 | Glacial Strike 71316/71317; Frostblade 71325 summons 38199 at a target, which casts 71323 (2 s) for 70305 every 1 s for 6 s within 12 yd, again every 9 s for its 14 s | avoid the sword's whirlwind | 14.5 yd blade dodge; taunted off a non-tank |
| Frostwarden Sorceress 37229 | Frostbolt 71318 (2 s), Frost Nova 71320 (10 yd), Ice Tomb 71330/71331 (1.5 s cast, 5 s stun) | kill the Ymirjar | first in the kill order; no Frost Nova or Ice Tomb answer |

`BossStateGate` on boss 13, and every predicate needs the bot within 100 yd of controller 37503 (the
entry is its guid slot). A 200 ms scan from the controller lists live adds and Frostblades; the state
resets once the boss state leaves `IN_PROGRESS`.

- `GetSgGauntletTarget` (`sg.gauntlet.target`): a player's skull; a duty tank's `rescue` of a Champion
  or Warrior hitting a non-tank, elected to the nearest; then the nearest engaged add of the first
  tier that has one. Engaged: in combat, its victim in the group, attackable, which skips spiders on
  their webs. `OwnTargeting` takes `DpsAssist | TankAssist` only, so area damage stays free.
- The rescue taunt (`sg.gauntlet.taunt` `champion`, `warrior`) goes to the nearest tank with its taunt
  ready, skipping an add a taunt just landed on, within 30 yd and in sight; the tank then attacks it.
  Generic taunts stay free: these adds have no taunt DR.
- The blade dodge (`sg.gauntlet.dodge` `melee`, `clear`, `blocked`): forced, inside 12.5 yd of a live
  Frostblade 38199 to 14.5 yd, a melee bot keeping its target in reach where it can. A melee bot or tank
  whose target stands within 14.5 yd of a blade gets no reach or charge, else reach walks it back in;
  avoid aoe is blocked inside a blade.

Known gaps: never built and no trace; no Frost Nova, Sorceress Ice Tomb or Web answer; humans hold no
duty.

`tools/botobs/bosses/sindragosa_gauntlet.py`: `--waves` counts each kind of add with its first and
last sample, spiders split at the first Frostwarden; `--targets`, `--taunts`, `--dodges` (with
Frostblade 70305 hits), `--cheats` (none is the gauntlet's). Its banner names only silent `sg.gauntlet.*` keys.

## Rimefang and Spinestalker

No boss state. Each flies in once its Frostwarden Handler and whelp pack dies and lands defensive:
Rimefang 37533 (25-man 38220) at (4413.309, 2456.421), Spinestalker 37534 (38219) at (4418.895,
2514.233). Combat reach 9. Sindragosa is summoned when the second dies.

| Ability | Script | Guide | Bot |
|---|---|---|---|
| Frost Breath 71386 (Rimefang) | 12-15 s, then 20-25 s, 1 s cast; 17-23k Frost and -75% attack speed for 4 s in a 60° front cone | tank him facing away | tank alone in front; a button under 60% |
| Frost Aura 71387 (Rimefang) | 1200 Frost per 2 s within 100 yd | | resist hook, frost, while a drake is live |
| Icy Blast 71376 (Rimefang) | 30-35 s, then 60-70 s: up 25 yd and passive, 4/6/5/7 casts 3 s apart (1.5 s) at a random player's spot: 71377 4.8-6.2k within 7 yd and puddle 38223 for 40 s with 71380/71381, -75% run speed and 5.5-6.5k/7.4-8.6k Frost per 2 s within 7 yd; lands on the top threat | avoid the marked spot and the puddles; don't top threat while he is up | dodge spot and puddles at 9.5 yd; non-tanks braked at 90% while he is up |
| Bellowing Roar 36922 (Spinestalker) | 20-25 s, then 25-30 s, 2.5 s cast; 4 s fear within 35 yd | Tremor Totem, Fear Ward | anti-fear; ranged and healers 37 yd off him |
| Cleave 40505 (Spinestalker) | 10-15 s, front | stand at his side | tank alone in front |
| Tail Sweep 71369/71370 (Spinestalker) | 8-12 s, then 22-25 s; 3.75-6.25k/5.25-8.75k and knockback within 30 yd. Visual 3879 makes both back cones: `spell_cone` -82 on 71369 leaves 10-man only his front 82° safe; 71370 has no row, a 24° wedge behind him | avoid the tail | melee on his flank; on 10-man the sweep still reaches it |

`UnitGate<37533, 37534>` with `ICC_BOSS_NONE`, trace `sindragosa` (`IccMarkPull` while live), the
drakes from guid slots 25 (Rimefang) and 24 (Spinestalker). A drake fights while alive, in combat,
and its victim is in the group (a pet or totem counts as its owner) or the bot fights it, which covers
Rimefang's flights with no victim.

- **Tanks** (`sg.drake.tank`, `1` Rimefang, `2` Spinestalker): kept for the fight. An unowned drake goes
  to the best free bot tank (main tank, assist order, guid); one a human tank holds stays his.
- **Targets** (`sg.drake.target`): a player's skull (a braked bot ignores one on its focus drake), a
  tank's own drake, nothing if braked, then the focus drake: a skulled fighting one, else the first in
  kill order, Rimefang first.
- **Stands** (`sg.drake.stand`), latched, absolute, z 203.38, navprobed `--nav 0x09`: the owner on the
  tank spot, free tanks and melee on the melee spot, ranged and healers on the ranged spot. Each holds
  only while its drake is landed and the spot 9.5 yd or more off every puddle and impact; melee and
  ranged also need the drake within 8 yd of its landing spot. Sight clear from each ranged spot to its
  landing and tank spots, Rimefang 25 yd up, and Spinestalker's melee spot.

  | Stand | Rimefang | Spinestalker |
  |---|---|---|
  | tank | (4413.31, 2446.42) | (4418.90, 2524.23) |
  | melee | (4405.31, 2456.42) | (4410.90, 2514.23), his south flank |
  | ranged, healers | (4391.31, 2456.42) | (4384.13, 2526.89) |

- **Brake** (`sg.drake.brake`, 90%/80%): free bot tanks all fight, everyone else but healers while
  Rimefang is up (`sg.drake.air` `1`), against the owner, else a tank victim, else the top tank. Free
  tanks' generic taunts are blocked; the owner keeps its own.
- **Icy Blast** (`sg.drake.dodge` `blast`, `puddle`, `blocked`): a forced dodge from 7.5 yd to 9.5 yd
  off puddles 38223 within 100 yd, the spot Rimefang is casting 71376 at (its target's when the cast
  has no destination), and after the cast that spot for up to 4 s or until a puddle shows within 3 yd.
  The cast's own target ignores the live cast, since the missile follows it until the cast ends. Melee
  keep their target in reach where they can.
- **Bellowing Roar**: `RaidAntiFear` from 8 s before the next roar can come (20 s after the first tick
  he fights, then 25 s after each cast seen) until he casts.
- **Tank buttons** (`sg.drake.defensive`, `sg.drake.humancall`): a tank holding a drake under 40%,
  physical only; Rimefang's tank on each Frost Breath at it under 60%, magic too; class buttons held to
  25%; a human tank called on the same every 500 ms.

Decisions, source then choice: the blog, Rimefang first. The blog (out of his head and tail) and the
25-man back cone: Spinestalker's melee on his flank. Bellowing Roar's 35 yd: his ranged 37 yd off. Both land defensive: stands only once a drake
fights and has landed. A free tank on the melee stand that took a drake would turn its breath, cleave
or tail onto melee: free tanks braked all fight, not only while Rimefang flies. The core's
`Spell::_cast` takes the destination at cast end, and 71376 flies 15 yd/s and summons on hit: the
impact stays a hazard after the cast. The stands are absolute: melee and ranged drop theirs once the
drake is dragged 8 yd off its landing spot.

Known gaps: never built and no trace. On a double pull DPS and healers follow Rimefang, and
Spinestalker's tank can be 73 yd from Rimefang's ranged spot. On 10-man his Tail Sweep reaches the
melee flank: only his front 82° is safe, where his Cleave jumps from the tank. His melee spot is 29.6 yd
from his ranged spot, at the edge of a ranged shaman's 30 yd Tremor Totem. A puddle on a stand drops it for up to 40 s, with no other spot. Melee have nothing
to hit while Rimefang flies. Humans hold no duty.

`sindragosa.py --drakes`: tank roles, Rimefang's flights, the brake, dodges with Icy Blast 71377/71378
and puddle 71380/71381 hits, stands, targets, buttons and human calls.
