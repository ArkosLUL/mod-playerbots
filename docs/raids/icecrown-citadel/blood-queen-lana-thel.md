# Blood-Queen Lana'thel

Boss id 8, slug `blood-queen-lana-thel`, prefix `bql.` ([README](README.md)). Code: stem `BloodQueen`
(`src/Ai/Raid/ICC/*/ICC*_BloodQueen.*`); nodes `icc bql` dodge, vampiric bite, swarming shadows, pact of
darkfallen, mass dispel, whirl defensive, whirl cover, tank defensive, anti fear, frenzy cc, taunt, brake,
target, stand; multipliers `icc bql brake`, `icc bql splash`, `icc bql anti fear totem guard`. Core:
`boss_blood_queen_lana_thel.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; code remaps a difficulty row through
`GetSpellIdForDifficulty`. The world DB remaps none of hers. Every area is a creature's or a player's
on players: centre to centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Blood-Queen 37955 | Instance guid slot 8. `IN_PROGRESS` at her engage, which evades unless the council is `DONE`; zone combat. Combat reach 7.2, so melee reach her from 10 yd. Taunt DR (`flags_extra` 0x80000), no parry haste (0x8), 0x200 leaves her gravity flag to her script. Evades past 64 yd from (4595.93, 2769.37). Berserk at 330 s | 2 tanks, 4-6 healers (10-man 1-3) | Tank roster (`bql.tank`); recovery taunt below DR level 3, generic taunts and Death Grip blocked |
| Shroud of Sorrow 70986 → 70985 (row 2092) | Every 2 s, shadow within 40 yd: 4k on 10N, 4.5k otherwise | Healed through | Shadow resistance aura (hook) |
| Presence of the Darkfallen, heroic | Each bite stacks 70995 on her: +5% shadow damage done a vampire | Raid cooldowns late | Counted in the Whirl's predicted hit |
| Blood Mirror 70821 | From 2.5 s, every 2.5 s: links her victim to the nearest other alive player within 39 yd, vampires included; the victim carries 70821, the linked player 70838 and takes every hit on the victim again in its own school, armour skipped, only absorbs and immunity counting (`SPELL_AURA_SHARE_DAMAGE_PCT`) | The off-tank stays next to the main tank, in stamina gear | Mirror tank stands on the main tank; with one tank player in the raid, a damage dealer does |
| Delirious Slash 71623 (row 2091) | 10-12 s, then every 20-24 s: 50% weapon damage (heroic 75%) plus a 15 s bleed on the linked player within 10 yd plus both reaches, else her victim | Off-tank in stamina gear | Tank buttons under 40% |
| Vampiric Bite 71726, hers | Once, at 15 s, on the highest-threat player who is no vampire and not her victim, the linked off-tank included: Essence of the Blood Queen 70867 (row 2082) | Bitten third on threat | Tanks but the main one, and a damage-dealer mirror, don't attack her until the first vampire exists |
| Essence of the Blood Queen 70867, 70879 after a bite (rows 2082, 2087) | 75 s (25-man 60 s): +100% damage, heals 10% of damage done; 70871 sets threat ×0. It ends in Frenzied Bloodthirst 70877 (row 2083: 71474 on 25-man), 15 s (25-man 10 s), with pacify and silence | Bite another player inside the window, in a set order, never a healer | Each frenzied bot is handed a target (`bql.bite`) and bites by id |
| Vampiric Bite 70946, the vampire's (row 2084) | Melee range, not a vampire: renews the biter's Essence, gives the target one; an override spell, never in the spellbook | As above | Cast by id: a name resolves to nothing |
| Uncontrollable Frenzy 70923 | A Frenzy that runs out: a permanent charm with +100% damage; 70924 (25-man 73015, linked) adds immunity to fear and grips, +300% health (25-man +900%) and +1000% healing taken; she kills them all if she evades | Mind controlled until killed (Wowhead) | One CC bot per victim (`bql.cc`), area damage held round it; one no bot can control for 2 s is killed (`bql.kill`) |
| Pact of the Darkfallen 71340 | 20 s, then every 30 s (5 s after landing), on 2 players (25-man 3), never her victim, the linked off-tank or a frenzy victim, for 28 s. Every second 71341 hits the carrier and every unlinked ally within 10 yd of it, +10% each 5 s, +20% on heroic; it ends once every member stands within 5 yd of every other | Gather at a spot picked in advance, in the middle | Members walk to the latched Pact spot (`bql.pact`), or to a human member who doesn't come; stands keep 12 yd off it |
| Swarming Shadows 71264 → 71265 | 30 s, then every 30 s (20 s after landing), on a random player not her victim, the off-tank, a Pact member or a frenzy victim: a 2 s warning, then a Swarming Shadows 38163 at its feet every 0.5 s for 6 s, each living 90 s and hitting 71268 (row 2213) within 4 yd every second; the carrier takes 71277 (row 2214) | Run to the edge of the room, never near the Pact spot | The carrier walks a wall lane (`bql.curve`); everyone else dodges, nodes near a shadow are re-dealt (`bql.dodge`), bite and Pact walks detour (`bql.detour`) |
| Twilight Bloodbolt 71818 (row 2109) | 15-25 s, then every 10-15 s, on 2 players (25-man 4) not her victim, the off-tank, Pact or frenzy; 15 yd/s missile, then Bloodbolt Splash 71447 (row 2086) within 6 yd | 6 yd spread, melee in 2-3 small stacks | 7.5 yd lattice, melee in up to 3 groups |
| Air phase | 124 s (25-man 127 s): passive, she runs to (4595.71, 2769.42), casts Incite Terror 73070 there (instant, a 4 s magic fear within 99 yd, no difficulty row), takes off 2.5 s later to 22 yd up at 4.5 yd/s, then Bloodbolt Whirl. The next comes 120 s (25-man 100 s) after she reaches the centre | Priests Fear Ward themselves, then Mass Dispel; Tremor Totem; tanks stay put | `RaidAntiFear` from 8 s before the walk until takeoff, priests warding themselves; free priests Mass Dispel the feared (`bql.massdispel`); the main tank holds the tank spot |
| Bloodbolt Whirl 71772 | A 6 s channel at the top: every 2 s 71899 (row 2113) picks a third of the raid not yet hit, so everyone once; it flies 30 yd/s, then 71446 (row 2085) 40 yd/s, each over the distance less her 7.2 reach, and splashes 71447 within 6 yd. She starts down 7 s after the cast starts, lands at the centre and runs to her top threat | Spread 6 yd, raid and personal cooldowns | Melee, spare and mirror tanks take air nodes until the Whirl ends; raid cover and personal buttons (`icc.cover`, `icc.personal`) |

## Phase

`IccBqlPhaseOf` publishes `bql.phase` from her own state: `center` while passive with gravity on (the
walk and Incite Terror), `air` from takeoff until she lands, `ground` otherwise. It replaced a height
test plus three latches that saw the walk late and misread her tanked off-spot near the centre.

## Layout

Every point is navprobed with `--nav 0x09` and settles within 0.1 yd of its Z; every node's 3x3 grid at
1 yd sees her ramp spot and her air spot 22 yd above the centre, and those in 38 yd of the tank spot see
it and the group spots. The bowl is flat at 400.14 out to 37.5-39 yd from the centre (4595.71, 2769.42)
on the sides and past 41 yd at the south end; the dais north of it is flat at 401.75 from 27 to 37.5 yd
north, 12-14 yd either side of the tank spot, over a ramp, and drops to the council room below at
38.5 yd.

- **Tank spot** (4630.71, 2769.42, 401.75), 3.5 yd short of the dais edge (the old one was 1 yd). She
  stops 9.2 yd short of a tank she chases, on the ramp at (4621.51, 2769.42).
- **Nodes** (`ICC_BQL_NODES`, 45): one triangular lattice with 7.5 yd sides (6 yd splash plus two
  0.75 yd park tolerances) within 25 yd of the centre, so a shadow on the inner lane misses it, plus
  three of its points on the dais edge 33-35 yd north, outside the lanes' ends and 8.3 yd or more off
  them: (4627.46, 2758.18) on the dais, (4627.46, 2784.16) on its slope, (4623.71, 2751.68) on the bowl,
  each with a straight bot path from the centre. The 30 ground nodes keep 12 yd off the first Pact spot
  and 9 yd off her default group spots: 20 are in caster reach of her ramp spot, 14 in heal reach of the
  tank spot too. The rest are air nodes.
- **Pact spots**: 6 yd south of the centre, then 8 yd either side when a shadow fouls it.
- **Lanes**: arcs of 35.5 and 30.5 yd round the centre, 40 degrees either side of north round the south
  end, a point every 5 yd.
- **Melee groups**: 7.5 yd from her latched spot, behind her and 60 degrees either side (two groups 45
  degrees), stepped 30 degrees at a time to at most 120 off her back when a shadow or a closer group
  fouls one; on floor within 2.5 yd of her height, 36 yd of the centre. Groups 60 degrees apart are
  7.5 yd apart, and 7.5 + 1.5 drift + 0.75 park stays in her 10 yd melee reach.

## Duties

- **Tanks** (`bql.tank`): bot tanks ranked main tank, assist tanks in order, guid: main, mirror, then
  spares. With a human main tank in the group, the first bot tank is the mirror. With one tank player in
  the raid, human or bot, the mirror is the sturdiest damage dealer: plate melee (DK, warrior, paladin),
  then other melee, then ranged, most health, latched; it skips groups, nodes, CC and kills. The main
  tank holds the tank spot all fight, air phase included, leaving it only to fetch her while she is on
  someone else more than 15 yd off it; the mirror stands on it (1.5 yd beside a bot parked on the
  spot, else on the main tank) and takes an air node until the Whirl ends; spares join the melee groups.
  The main tank taunts her back below DR level 3 when she is on somebody else on the ground.
- **Targets and brake** (`bql.target`, `bql.brake`): a player's skull on her or a mind-controlled
  vampire, then for a damage dealer no vampire a vampire marked for the kill, else her; nothing for a
  frenzied bot, nor for a braked one but a vampire. Non-holders other than healers and vampires are
  braked past 90% of her holder's threat until under 80%, the main tank's while she has none (her walk
  to the centre until she lands on her top threat); tanks but the main one, and a damage-dealer mirror,
  are braked until a vampire exists or 20 s in.
- **Bites** (`bql.bite`): each frenzied bot is handed the nearest damage dealer, then a tank she isn't
  on or a damage-dealer mirror, then a healer, never her victim or the main tank, nobody another biter
  holds, no vampire, mind control or shadows carrier; anyone under 15k health after those. It cancels
  Ice Block, walks within 3 yd and bites by id.
- **Mind control** (`bql.cc`, `bql.kill`): each Uncontrollable Frenzy victim gets the nearest bot that
  can land a CC now (Polymorph, Cyclone, Hex, Repentance or Hammer of Justice, Blind, Death Coil, Scatter
  Shot: known, off cooldown, in its form, short of DR immunity, not immune, in range); it recasts
  whenever the victim isn't stunned, confused, feared or pacified, and hands it on once nothing it can
  land is left and its last CC has worn off. Area damage waits while a controlled vampire stands within
  15 yd of the bot or its target (`icc bql splash`). One left uncontrolled 2 s, by bots and players,
  with no bot able to land a CC is marked for the kill until it dies or the charm ends: damage dealers
  other than vampires, CC holders and the mirror take the nearest and drop their stands.
- **Pact**: members walk to the latched spot (`bql.pact`) at forced priority, with Nitro Boosts past
  12 yd, and stop within 1 yd. A human member still over 4 yd off it 3 s after the Pact landed is walked
  to instead, the lowest guid of several, the bots stopping within 2 yd of him.
- **Detours** (`bql.detour`): a bite or Pact walk whose straight line crosses a shadow's 4.5 yd goes
  through the nearest point within 16 yd clear of shadows by 5.75 yd whose two legs miss them, at most
  8 yd longer, kept while its legs stay clear; with none it walks straight. Shadows the bot or its goal
  stands in don't count.
- **Swarming Shadows** (`bql.curve`): the carrier takes the lane and direction with the shortest walk to
  a point clear of shadows by 5.5 yd and the longest clear run (45 yd wanted), the outer lane first,
  walks it point to point at forced priority, and re-plans at a lane end or a foreign shadow ahead.
  Everyone else within 4.5 yd of a shadow steps 5.75 yd clear (`bql.dodge`); melee keep their target
  in reach.
- **Precedence**: the bite, then a shadows trail, then the Pact, when the script stacks them on one bot.
- **Spread** (`bql.slot`, `bql.airslot`, `bql.group`): healers keep a ground node in 36 yd (held to
  38) of the main tank and her; ranged one in 35.5 yd of her (hunters 40, held to 37.5 and 42), a 30 yd
  spell reaching 38.7 yd past her 7.2 combat reach. While she
  walks or flies held nodes stay put. From her walk to the centre until the Whirl ends, every bot but the
  main tank without a ground node takes the nearest free node. Melee and spares split into up to three
  balanced groups by guid. Nodes and spots within 5.75 yd of a shadow are given up.
- **Defensives**: raid cover and personal buttons over the Whirl (a third of a bolt per tick left plus
  Shroud's ticks, heroic Presence counted; frenzied bots skip it), class cover nodes held over its
  lead-in; any tank or damage-dealer mirror under 40%, and her holder and the mirror in the last minute
  before Berserk, spend `NextTankDefensive` (a damage dealer `NextPhysicalDefensive`, `bql.defensive`),
  tank class buttons held to 25% until that minute; a human tank is called for both (`bql.humancall`).
- **Fear**: `RaidAntiFear` from 8 s before her predicted walk (124/127 s in, then 120/100 s after she
  reaches the centre) until takeoff: Tremor Totem, earth totems held, Fear Ward on each priest knowing
  Mass Dispel, else the shared main tank or healer. Under Incite Terror each unfeared priest takes the
  feared player with the most feared within 15 yd (at most 10, at least 3, fear outlasting the 1.5 s
  cast), in 30 yd and sight, leaving out those within 15 yd of another free priest's pick
  (`bql.massdispel`), and casts Mass Dispel there, stopping a walk first.
- **Hooks** (`HookRows()`): burst `IccBloodQueenBurstWindow` (lust once vampires
  reach 8, 16 on 25-man, or 280 s in), opener `GetBqlOpenerBosses` (her), resist
  `IccBloodQueenResistSchools` (shadow all fight).

Rules: targeting owned for the whole fight; a braked bot's pickers held; a frenzied bot runs only the
bite and a shadows carrier only its lane; Pact members and stand holders keep attacks and resurrect
reach only, the main tank and mirror on their stands, any role, attacks only; spell movers, avoid aoe
(it flees a shadow 5 yd), generic taunts, Death Grip and tank caster form blocked.

## Decisions

- Pact spot 6 yd south of the centre rather than on the medallion: still in the middle, and the same
  lattice keeps three more caster and four more healer nodes.
- One lattice for ground and air: rings round the centre (old air slots) can't hold the ground rules.
- The mirror, a damage dealer too, stays off her until the first vampire and is bitten after the other
  damage dealers: her bite takes the linked player, not only DPS, and a frenzied mirror walks off the
  main tank.
- With one tank player the mirror goes to the most health, plate first (they carry a class taunt too):
  it takes the main tank's hits again plus Delirious Slash, and with nobody posted Blood Mirror lands on
  whoever stands nearest. No damage-dealer mirror beside a second tank player, human included: two
  racing for nearest move the link. A melee mirror in front loses only its own parried swings.
- Bites reach tanks and healers after the damage dealers: Warcraft Tavern's 10-man count of eight
  vampires by 3:30 needs them, and an Uncontrollable Frenzy costs more. Never the main tank: a vampire
  makes no threat.
- The mind-controlled vampire is controlled while any bot can, so area damage near it is held; a
  player's skull on it still wins. Warlocks use Death Coil: the vampire is immune to fear. One nobody
  can control is killed, after Wowhead, by the damage dealers that are no vampire: its health makes the
  kill dear (+900% on 25-man), and a vampire's doubled damage is worth more on her.
- Priests ward themselves rather than the main tank, after Warcraft Tavern and Wowhead: she is passive
  through the fear, so a feared tank costs nothing, and a warded priest can Mass Dispel the rest.
- Bite and Pact walks detour by at most 8 yd, about the second a crossing costs in shadow ticks.
- Bots walk to a human Pact member who doesn't come: he holds no duty, and the Pact only ends on a
  meeting.
- Three dais-edge nodes for the 25-man shortage rather than overflow onto the air nodes by the Pact spot,
  which every Pact walk-in reaches.
- The main tank fetches her once she chases someone else 15 yd off the spot: its taunt reaches 30 yd,
  and her landing spot is 35 yd away.
- Lust at the vampire peak, after Wowhead.
- Tank buttons wait for a low tank or the last minute: no hit of hers is predictable, and Wowhead saves
  them for the end.
- Lanes as a horseshoe through the south end, so three live trails fit.
- No opening positions: her aggro radius is 23 yd, and the gate is open everywhere between pulls.
- Nodes block heal reach: every healer node covers the tank spot and her.

## Cheats

None, and none before: `bql.swarmingshadows`, a tank stripping Swarming Shadows, was removed earlier (the
script never puts it on her victim or the off-tank). Own-aura cancels (Ice Block before a bite) are
not cheats.

## Known gaps

- No 631 trace: every threshold is a model until a pull. The user built `623638b99` clean on
  2026-10-29; the damage-dealer mirror, kill, Mass Dispel, detours, human Pact meeting and dais nodes
  came after it, unbuilt.
- A damage-dealer mirror carries the shared hits and Delirious Slash on DPS health: `--tanks`,
  `--defensives` and its deaths say whether it lasts.
- Humans hold no duty; two human Pact members apart still keep the Pact up, the bots meeting one.
- A 25-man kill takes non-vampire damage off her for seconds: `--cc` kill marks against the victim's
  death time say how long.
- Mass Dispel needs a warded or unfeared priest within 30 yd of a cluster, and Fear Ward's 3 min cooldown
  (2 min glyphed) wards only the first Incite Terror: at the second, 120 s (25-man 100 s) later, only a
  glyphed 10-man priest is warded, the rest waiting for Tremor Totem or the fear's end. Priests could
  split, half keeping the ward for the second. `--fear` freed early.
- Percent cuts (Shield Wall, Divine Protection, Icebound Fortitude) miss the mirrored share, which only
  absorbs, immunity and health count against (`Unit::DealDamage`): the mirror's buttons cut only
  Delirious Slash and direct hits, and `--defensives` against its deaths says whether to pick health
  buttons and absorbs.
- Pact members walking in hurt bots within 10 yd of their path: a 7.5 yd spread has no 10 yd corridor,
  and the guides' walk to a picked spot pays the same.
- Three live trails can fill both lanes; the carrier then takes the best partial run (`--shadows`).
- A walk with no clear point inside 16 yd and 8 yd extra crosses straight (`bql.detour` `blocked`).
- A melee group spot only follows her once she has stood still 0.5 s; while she walks, generic reach
  follows her.
- On 25-man the ground nodes run short with more than 20 casters and healers; the rest hold no stand.
- r14: unused `bqlBotCurrentCurve`, `bqlAirSlotMemory`, `bqlBotSlotMemory` (`ICCScripts.h`, whose
  keys this stem reuses) and `NPC_SWARMING_SHADOWS`, `SPELL_PACT_OF_THE_DARKFALLEN`,
  `SPELL_BLOODBOLT_WHIRL` in `ICCData.h`; hoist one walk base.

## Targets for the next pulls

25N first: `--bites` ends every window `bit`, the first vampire a damage dealer; `--tanks` holds one
main and one mirror all fight, and no Blood Mirror lands on anyone but the mirror (aura 70838);
`--targets` shows `brake` on the mirror until 15 s and rarely after; `--pact` has every bot member on
`spot 1` and Pacts under 6 s; `--shadows` keeps every trail on a lane and no `--spread` node lost to a
shadow; `--spread` has every caster and healer on a ground node and nobody hit by two 71447 in one
Whirl; `--fear` frees most of each Incite Terror early; `--cc` marks a kill only once CC runs out. With
one tank, `--tanks` names a plate damage dealer mirror. 25H: the same, plus deaths in the second Whirl.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `bql.phase` | per bot | `ground`, `center`, `air` |
| `bql.target` | per bot | `boss`, `skull`, `brake`, `bite`, `kill` |
| `bql.brake` | per bot | `1` braked, `0` free |
| `bql.tank` | per tank, or a damage-dealer mirror | `1` main, `2` mirror, `3` spare, `0` off the roster |
| `bql.bite` | per biter | the player handed, `0` once its Frenzy ends |
| `bql.cc` | per CC bot | the mind-controlled vampire, `0` released |
| `bql.kill` | per vampire | `1` marked for the kill, `0` dead or released |
| `bql.massdispel` | per priest | the feared player its Mass Dispel centres on, `0` none |
| `bql.pact` | per bot | `spot N` while in a Pact, `human` walking to a human member, `none` |
| `bql.curve` | per carrier | lane `1` outer rising, `2` outer falling, `3` inner rising, `4` inner falling, `0` trail over |
| `bql.slot`, `bql.airslot`, `bql.group` | per bot | ground node 1-30, node 1-45, melee group 1-3; `0` given up |
| `bql.dodge` | per bot | `melee`, `clear`, `blocked`, `none` |
| `bql.detour` | per bite or Pact walker | `via`, `blocked`, `none` |
| `bql.defensive`, `bql.humancall` | per tank or mirror | the button spent, `idle`; the call to a human tank |

`tools/botobs/bosses/blood_queen_lana_thel.py`: `--phases`, `--targets`, `--tanks`, `--bites`
(per Frenzied Bloodthirst: the players handed, `bit`, `frenzy`, `died` or `open`), `--cc` (with the kill
mark), `--fear` (per Incite Terror: feared, freed early, each priest's centres and casts), `--pact`,
`--shadows`, `--spread` (with detours), `--defensives`, `--cheats`.
