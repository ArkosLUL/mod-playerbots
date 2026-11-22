# The Lich King

Boss id 12, trace slug `the-lich-king`, prefix `lk.` ([README](README.md)). Code: stem `LichKing` for
phase one, the winters, tanks and targeting, stem `LichKingLate` for phases two and three and
Frostmourne (`src/Ai/Raid/ICC/*/ICC*_LichKing*.*`), one definition. Mechanics: `boss_the_lich_king.cpp`.

## Mechanics

The intro ends about 60 s after Tirion's gossip: he stops at (465.07, -2123.47) and pulls the zone,
which puts him `IN_PROGRESS`; Berserk at 15 min. Combat reach 5.5, so his melee reaches about 8.3 yd;
taunt diminishing returns (`flags_extra` 0x80000). His melee victims carry Boss Hittin' Ya 73879 for
60 s, which keeps tanks out of Necrotic Plague, the Val'kyrs and Harvest Soul. He and his adds attack
only units within 90 yd of the centre C (503.63, -2124.66, 840.857) between Z 840 and 875 and off
vehicles, and every 5 s the script kills any player under Z 700. Ids are 10N/25N/10H/25H, one id when
no spelldifficulty row exists (the world DB adds none). A creature's area spell adds no target size. A
`spell_cone` row names only the 10N id of Pain and Suffering, Soul Shriek and Shockwave, so the other
three difficulties fall back to 104°. Guides: Warcraft Tavern (WT), Wowhead (WH), instant-carry (IC).

Navprobe reads nothing here: the Frozen Throne is Arthas Platform GO 202161 and the Frostmourne room
belongs to Arthas Precipice GO 202078, with no navmesh, vmap or terrain under either, so the floors
exist only as live GO collision. The platform is intact to about 68 yd. Quake's shatter (`DAMAGED`,
model 9257) drops the ring past the crack triggers at 47-51 yd. The second winter's cast restores it
(`DESTROYED`, then `REBUILDING` 1.5 s later, both model 9258). Four ice shards (GO 202141-202144,
display 9227) stand 41 yd out at 43°, 137°, 223° and 317° (θ from +x, north, toward +y, west). The
Val'kyr drop triggers (22515, Z 845.9) stand 64-68 yd out.

| Ability | Script | Guide | Bot |
|---|---|---|---|
| Infest 70541/73779/73780/73781 | 2 s cast, 5 s then 22.5 s; after the first Quake 14 s then 22.5 s; none in phase three. Raid-wide hit, then 1k/1k/1.5k/2k shadow per s growing 15% a tick from the 2nd, removed above 90% health | heal everyone above 90%, shields | healer engine |
| Necrotic Plague 70337/73912/73913/73914, jump 70338/73785/73786/73787 | phase one, 30-31 s then 30-31 s (5 s retry); a random player but his victim. Disease, 15 s, 50k/100k/75k/150k shadow every 5 s. Dispel, expiry or death jumps it to the nearest unit within 10 yd, adds included, not him; expiry or death adds a stack, a dispel's stack is lost. Each jump gives him Plague Siphon 74074, +2% physical damage | the carrier runs to the add pile and is dispelled there before the first tick; in a winter the carrier stands 10 yd off everyone, then is dispelled | carrier walks to the pile's raid side; an elected curer dispels once its nearest unit is an add, or with under 1 s to the tick or under 35% health; class cures held off carriers, a Cleansing Totem replaced; with no add alive, and after phase one, it isolates first |
| Shambling Horror 37698 (70372) | phase one, 1 s cast, 15 s then 60 s; spawns by him, passive 5 s, then attacks threat or a random non-tank. Shockwave 72149/73794/73795/73796: 1.25 s cast, 20-25 s, 20 yd cone at its victim, 90° on 10N. Enrage 72143/72146/72147/72148: 1 s cast, 11-14 s then 20-25 s, +200% damage for 6 s, enrage dispel. Heroic Frenzy 28747 at 20%: +100% damage, +50% haste. Stun and grip land; snare, root and interrupt don't | off-tank faces Shockwave away; Tranquilizing Shot or Anesthetic Poison on Enrage; stunned through Frenzy; the plague kills it, DPS ignore it | add tank holds the pile with its cones off the raid; hunters Tranquilize; the add tank's button on an Enrage no hunter answers; heroic Frenzy: stun rotation, melee finish it |
| Drudge Ghoul 37695 (70358) | phase one, 0.5 s cast, 10 s then 30 s; three over 3 s by him, passive 5 s, negligible damage | off-tank; keep them alive as plague fodder, no cleave | add tank picks them up, else a DK or paladin damage dealer stands in; area damage held near adds in phase one |
| Shadow Trap, heroic, 73539 → 39137 | phase one, 15.5 s then 15.5 s; 0.5 s cast at a random player within 100 yd in sight. Arms after 5 s (73530); then any unit within 3 yd, a pet or totem too, fires 73529 at everyone within 10 yd: 23.6-26.4k and a 30/40 knockback off the platform. Lives 60 s; the first winter's opening hit despawns them | run out, never step on one; drag him off one | anyone within 10.5 yd of a trap walks, forced, interrupting, 12 yd off every trap, a bot with no stand to the spot nearest him; a stand within 11 yd of one re-latches, so the holder drags him off; walks keep 3.5 yd off; melee reach round a trap |
| Remorseless Winter 68981/74270/74271/74272, second 72259/74273/74274/74275 | at 70% and 40%, on the first hit at or under while he isn't casting: passive, walks to C, 2.5 s cast, 60 s aura; 68983/73791/73792/73793 deal 7.1-7.9k/9.4-10.6k/9.4-10.6k/14.1-15.9k frost per s within 45 yd | everyone on the ice past 45 yd | two clusters on the ring past 49 yd; ring stands only while the platform isn't `DAMAGED` |
| Pain and Suffering 72133/73788/73789/73790 | 0.5 s cast from 3.5 s into each winter, every 1.75-2.25 s (heroic 1.25-1.75 s), at a random target: a 100 yd cone from him, 25° on 10N; 2.8-3.2k/4.7-5.3k/4.7-5.3k/7.5-8.5k shadow, then 0.5k/0.8k/0.8k/1.2k per s for 3 s | spread; WH: ranged and healers apart from tanks and melee | the clusters sit the cone's half-width plus 8° apart |
| Ice Sphere 36633 (69104) | 8 s into each winter, then every 7.5 s; spawns within 10 yd of him and follows a random player at about 3 yd/s. Ice Pulse 69099/73776/73777/73778 every 3 s: 2.8-3.2k/4.7-5.3k/4.7-5.3k/7.5-8.5k frost within 5 yd of its target. A player within 4 yd sets off Ice Burst 69108/73773/73774/73775: 10 yd, 20/35 knockback. Immune to stun, snare and taunt; drops a target beyond 75 yd of C or outside Z 830-855 | ranged kill them at once, hunters first | ranged DPS kill the sphere within 30 yd closest to its target first; 25N/10H/25H: sphere guards in the tank cluster; winter slots 6 yd apart |
| Raging Spirit 36701 (69200) | 4 s into each winter, then every 20 s (15 s at 40% and under): three in the first winter, four in the second. Copies a random player within 100 yd, rooted 3 s, then attacks its source or threat. Stun, snare and interrupt don't land; taunt does. Soul Shriek 69242/73800/73801/73802: 0.5 s cast, 12-15 s, 15 yd cone at its victim, 60° on 10N; 18.8-21.2k/28.3-31.7k/28.3-31.7k/42.4-47.6k shadow and a 5 s magic silence | a tank takes each at once and faces it away; burn it before the phase ends; dispel the silence. Leftovers: tanked on him and cleaved in phase two, first priority in phase three | tanks taunt each as it unroots, never off a human tank, and hold it on the ring's rim facing out; DPS burn it; priests and paladins dispel silenced healers first, paladins never a plague carrier |
| Quake 72262 | 62.5 s after he reaches C: 1.5 s cast; the ring shatters 5 s after it lands, and he turns aggressive 5.25 s after | back onto the platform before the shatter | from the cast, everyone past 40 yd walks, forced, interrupting, to 38 yd on its bearing, Nitro Boosts, and holds until he turns aggressive |
| Soul Reaper 69409/73797/73798/73799 | after each Quake 40 s, then 30.5 s (1 s retry out of melee); instant on his victim: 50% weapon damage, 50k/60k/60k/70k shadow 5 s later, then +100% haste on him for 5 s (69410) | tank cooldowns on each; a swap once the tank has none | the victim's button; with none ready, the free tank taunts at once |
| Defile 72762, puddle 38757 | after each Quake 38 s, then 32.5 s (1 s retry), kept 4 s (10-man) or 5 s (25-man) off a Val'kyr summon; heroic: at once on the return from Frostmourne. 2 s cast at a random player within 100 yd in sight whom no Val'kyr has searched or carries. The puddle lands where the target stands at cast end and ticks every 1 s for 45 s on everyone within 10 × its scale (2D, not vehicle riders or Harvest Soul victims): 72754/73708/73709/73710 deal 3k/5k/6.5k/10k shadow × scale, and each hit adds 72756/74162/74163/74164, +10%/+5%/+10%/+5% scale | spread before it; the target runs out of the raid with Nitro Boosts; never stand in one | the target runs off the stack with Nitro Boosts; others within 13 yd step away; everyone keeps 10 × scale + 2 yd off a puddle; walks never cross one |
| Summon Val'kyr 69037 (25-man 74361) | after the first Quake 20 s, then 45 s; one (10-man) or three (25-man) 15 yd up by him. Each charges a random player 2.5 s later, not a tank, a searched or carried player or the Defile target, and carries it to the drop trigger nearest the grab at about 4.9 yd/s; Wings of the Damned 74352 floors slows at 50%. Stun lands, root doesn't, taunt not while carrying. Normal: drops it there and despawns. Heroic: at 50% drops it where it is, flies 5-15 yd off C at Z 855-859, turns tauntable and casts Life Siphon 73488/73782/73783/73784 (2 s, every 2.5 s) on its last taunter, else a random non-tank, healing 10× | him in the centre, the raid stacked tight behind him; kill it (heroic: to 50%) before the edge, stuns and slows; heroic: kill siphoners before the next grab | stack behind him at C; per Val'kyr DPS buckets, a stun rotation over the raid's kits, one slow; melee walk round a puddle; heroic: the free tank taunts siphoners, ranged kill them |
| Vile Spirits 70498 (37799) | after the second Quake 20 s, then 30 s (heroic: delayed 52.5 s by Harvest Souls); 0.5 s cast, 10 (25-man) or 8 (10-man) spirits within 13 yd of him, 13 yd up. They wander 15 s, then each chases a random player, descending; anyone within 5 yd sets off Spirit Burst 70503/73806/73807/73808: 18.8-21.2k/23.6-26.4k/28.3-31.7k/35.1-36.9k shadow within 5 yd. Stun and snare land | the raid runs to the far side; a tank with cooldowns, Divine Shield or Dispersion pops them; Frost Trap between | the holder drags him to the other side spot and the raid follows; one elected soaker, an immunity first, stands between; hunters trap |
| Harvest Soul, normal, 68980/74325 | after the second Quake 14 s, then 75 s (10 s retry); instant on a random non-tank within 55 yd in sight: 6 s stun, 7.5k/15k shadow per s. At 6.25 s the victim enters Frostmourne, the 40 yd round F (495.71, -2523.76, 1049.95). Terenas 36823 at half health (Light's Favor 69382: 5k per s, damage up by his health %) fights the Spirit Warden 36824 (taunt-immune, interruptible; Soul Rip 69397, an 8 s magic channel on its victim, 5k × tick number, 12-15 s then 23-27 s; Dark Hunger 69383 heals it for half its damage). The warden's death sends the victim back (Restore Soul 72595). At 60 s, or with Terenas dead, Destroy Soul 74086 kills the victim and gives him Harvested Soul 72679/74318 (+100%/+200% damage, 15 s) | heal the victim through the drain; inside, DPS kill the warden and kick Soul Rip, healers keep Terenas up and dispel it | healers heal the victim; inside, DPS kill the warden and kick, healers heal Terenas and dispel |
| Harvest Souls, heroic, 73654/74296/74297 | after the second Quake 14 s, then 100-110 s: the whole raid stunned 6 s for 2.5k/4k shadow per s (the aura has no duration and ends with his channel), then sent 2-20 yd off F at 6.25 s, taking 2k per s inside (73655). He stays passive until 1 s after the return. Terenas 39217 (immune) channels Restore Souls 73650 for 40 s from 8 s. Spirit Bombs 39189: one a second within 25 yd of F, one every 2 s 30 yd above a random player, falling about 3.5 yd/s and exploding 1 s after landing (73804/73805: 18.8-21.2k/23.6-26.4k within 10 yd). 23 Wicked Spirits 39190 hang at Z 1069.6; one wakes every 3 s from 9 s, 13 in all, and chases a random player to Spirit Burst within 5 yd; immune to taunt, stun and snare. Each death inside gives him Harvested Soul 74319/74320 (+100%/+500%, 15 s) | group up and move together off the bombs; ranged kill the Wicked Spirits early; nobody dies in the last 15 s; spread at once on the return for Defile | the raid stacks on one leader who steps off falling bombs, reach held; ranged kill the woken spirit nearest the group within 30 yd; Defile handling on the return |
| Fury of Frostmourne 72350 | at 10%: combat is disallowed and a 2.6 s cast kills the raid; Terenas raises it about 2:40 later; he takes damage only while levitating, once Frostmourne breaks | finish him | the resolver hands everyone the levitating Lich King |

## Phase

`IccLichKingPhase(bot, boss)` (`Util/ICCHelpers_LichKing`) reads his script's state and publishes
`lk.phase`; duties read the raid's phase, harvested if any member is:

| Value | When |
|---|---|
| `one` | above 70%, or aggressive under it until the first winter |
| `winter1`, `winter2` | the winter's aura or cast row; else passive above 40% (`winter1`) or at it and under (`winter2`): the walk to C and the wait after the Quake |
| `two`, `three` | aggressive after the first winter, above 40% or after the second winter at it and under |
| `frostmourne` | heroic, passive at 40% and under while harvested, or once phase three has run (the second after the return) |
| `outro` | combat disallowed at 10% |

He comes from guid slot 12; every predicate is false unless he is alive and the bot within 120 yd of
C or 50 yd of F. A unit other than entry 36597 reads `one`: the Lich King voice in Valithria's room
shares his name. The winters seen are latched per pull, since his transition waits for a hit he takes
while not casting, so a cast in flight keeps the phase before it for up to 2 s; bots that arrive
mid-pull take them from his health, and the lust window reads the same phase. A wipe, a kill or an
intro that evades resets both stems' state; opening stands run only while he walks the intro: out of
combat, immune, and no longer under the sit aura 73220, which he carries until Tirion stands him up
and again after a wipe.

## Layout

Polar from C (θ from +x, north, toward +y, west; r in yd). Navprobe reads neither floor, so nothing is
probed: spots stay within 44 yd of C, ring slots (49-62) only while the platform isn't `DAMAGED`,
Frostmourne spots within 20 yd of F. Every sweep refuses a point off those floors or, once the bot's
own spot reads the gameobject floor, more than 2 yd off its Z. Walks are straight lines at the floor's
Z (`exact_waypoint`), arrival judged in 2D, since the path search refuses off-mesh spots. A blocked
sweep (trap dodge, trap reach, plague walk) waits 1 s before the next. Frostmourne is judged in 2D:
its creatures hang up to 20 yd above the floor.

| Stand | θ, r | Who |
|---|---|---|
| L1 | 180, 12 | the holder, he faces it; (180, 14) during the intro, in taunt range of his stop |
| A | 90, 17 | the add pile |
| AT | 90, 22 | the free tank, so Shockwave points west off everyone |
| R1 lattice | 0, 8 | ranged and healers: 13 nodes 6 yd apart within 12 yd of R1, at least 12 from A, at most 28 from L1, by guid, shared least-used first |
| winter tank cluster | from 0 east, r 51 and 57 | non-spirit tanks, melee, half the healers rounded up (10N all), 6 yd apart along the arc; off 10N the sphere guards in its middle columns |
| winter ranged cluster | from 20.5 (10N) or 60 west | the rest |
| spirit spots | -7, 62; 7, 62 | the holder bot, then the free tank, spirit inward; melee DPS 3 yd inward of a held spirit or leftover |
| damaged ring | slot's bearing, 42 | every winter stand while the ring is down |
| Quake hold | own bearing, 38 | anyone past 40 yd |
| phase two | him at C | the holder (0, 3), the free tank (20, 4), the rest on 19 nodes 2 yd apart within 4 yd of K2 (180, 5), melee nearest him |
| phase three | east (270, 20), west (90, 20) | the phase two set rotated so 0° points out along the side |
| soak spot | 10 yd from the stack toward the old side | the Vile Spirit soaker; Frost Trap at the midpoint |

Ground stands latch (park 1 yd, walk back past 2.5), latching again once what they hang off moves 2
yd; one unparked for 15 s lets go for 3 s. A puddle
fouling him, the holder spot or K2 slides the phase two or three set up to 20 yd, within 40 yd of C,
and back once the spot clears.

## Duties

Two `RaidInstanceState`s, each a 200 ms scan and 250 ms duty passes, then per bot one walk, highest
first: quake, trap, plague, trap reach, stand (`LichKing`); Defile, Vile Spirits, the Val'kyr chase
round a puddle, stack (`LichKingLate`). The walk round a puddle and the stack yield to a plague walk;
trap reach goes on the stand row, since it dodges nothing.

- **Tanks** (`lk.tank`): the holder is his victim when a group tank, else the last holder alive, else
  the group's main tank, else the first bot tank; the free tank is the next bot tank by guid. Elected
  taunts (`lk.taunt`), reaper before back before spirit before add, his taunt DR read first: the holder
  takes him back off a non-tank; Soul Reaper on the holder with no button running or ready for its hit
  sends the free tank to taunt at once, and the roles flip. Generic taunts are blocked. A spirit on a
  human tank stays there.
- **Adds**: the free tank taunts each emerged add on someone else (Challenging Shout or Roar on two
  within 10 yd), leftovers through the winters too. With no tank player but the holder, phase one's
  stand-in (`lk.tank` 3), a DK, then a paladin damage dealer, kept while able, holds the ghouls alone
  at AT: its class taunt on each loose one, its pick a loose ghoul, else its own healthiest; it never
  walks the plague and its cure is `addtank`. Hunters Tranquilize each Enrage (`lk.enrage`),
  nearest first. Heroic Frenzy: one stun in flight per Shambling (`lk.stun`), the next by guid at
  0.5 s left; a stunner that hasn't landed in 1.5 s sits out 2 s. Melee DPS kill a Frenzied Shambling.
- **Control**: stuns (only below the immune DR) Hammer of Justice, Concussion Blow, Bash, Kidney Shot,
  Shockwave, Holy Wrath, Shadowfury, War Stomp, Intimidation and Gnaw with the pet in melee
  (Intimidation's DR read off its pet's stun 24394); slows
  Chains of Ice, Slow, Concussive Shot, Frost Shock, Curse of Exhaustion, Hamstring, Wing Clip,
  Frostbolt, Piercing Howl.
- **Necrotic Plague** (`lk.plague`, `lk.cure`): a non-tank carrier walks, forced, interrupting, Nitro
  Boosts past 15 yd, to 2 yd off the nearest pile add on its raid side, clear of traps; with no add
  alive, and after phase one, it walks 12 yd off every member (winters: outside 49 yd). One curer per
  carrier, healers then guid, kept while it can cure, dispels once the nearest unit within 10 yd is an
  add (`delivered`, `addtank`), at once with nobody in 10 yd (`isolated`), or with under 1 s to the
  tick or the carrier under 35% (`emergency`). Class disease cures and Cleansing Totem wait while any
  carrier lives, cures that also strip a disease (Cleanse, Purify, Cleanse Spirit, Cure Toxins,
  Stoneform) only off a carrier; a shaman bot's Cleansing Totem already down is replaced by Mana
  Spring, else Healing Stream (`lk.cure` `totem`).
- **Shadow Traps**, heroic (`lk.trap`): see the table. A melee DPS bot whose straight approach to its
  pick passes within 10.5 yd of a trap walks to the clear spot in its melee reach nearest it
  (`reach`), else holds (`hold`).
- **Splash**: in phase one, non-tank area damage within 12 yd of a ghoul or Shambling is held.
- **Winters**: slots laid out at his turn to passive. Winter slots and spirit spots are walked forced
  with Nitro Boosts past 15 yd, racing Remorseless Winter's first ticks; phase one stands walk at combat
  priority. Each
  Raging Spirit goes to the spirit tank holding fewer, taunted once unrooted and off its tank; a spirit
  tank more than 28 yd off its spirit walks to 22 yd of it. Later phases leave every spirit to the
  free tank. Off 10N, a third of the ranged DPS bots rounded up, hunters first, are sphere guards in
  the middle of the tank cluster (`winter guard <n>`). Priests and paladins dispel Soul Shriek,
  silenced healers first, paladins never a plague carrier, whose plague Cleanse takes too
  (`lk.silence`).
- **Brake** (`lk.brake`): in phases one to three, non-holders at 90% of the holder's threat until
  80%: nothing of theirs lands on him, nor area damage within 15 yd, the swap taunt excepted, and auto
  attack and shot stop.
- **Tank buttons** (`lk.defensive`, `lk.humancall`): the holder at 15 Plague Siphon stacks and each
  Soul Reaper, the free tank on an unanswered Enrage on it, any tank under 40%; Anti-Magic Shell only
  on Soul Reaper (Plague Siphon is physical); class buttons held to 25%; a human tank is called on the
  same windows.
- **Defile** (`lk.defilerun`), from the listener's cast start: the target runs, forced, interrupting,
  Nitro Boosts, to a spot 14 yd off every other member, 12 yd past live puddles, within 40 yd of C, on
  its side of the stack, else 16, 12 or 8 yd straight out. Others within 13 yd of it or its spot step
  13 yd off both. Anyone within 10 × scale + 2 yd of a puddle steps out; spots keep 10 × scale + 3,
  walks 10 × scale + 1.5.
- **Val'kyrs** (`lk.valkyr`): while one carries (heroic: above 50%), DPS bots and a free tank no
  Raging Spirit holds (it would drag the spirit into the raid) are bucketed, balanced, melee to the nearest, dealt again only when the carriers
  change. One stun in flight and one slow kept up per Val'kyr, the next caster at 0.5 s left, a 1 s
  grace for a cast in flight, one duty per bot, never the holder. Melee chase while the line clears
  every puddle, ranged past 30 yd too; healers stay. A melee chaser with a puddle in the line walks a
  waypoint beside the first one, latched until reached, reach held (`round`). Heroic siphoners
  (attackable, so not a fresh Val'kyr; no 68985, Z above 850, within 20 yd of C): the free tank
  taunts each once taunts land, at its siphon spot, never-taunted first, then longest ago
  (`lk.siphon`), and ranged kill the lowest once their bucket has no carrier.
- **Vile Spirits** (`lk.side`, `lk.vile`): east from the second Quake, flipped at each cast; the holder
  drags him over and the raid follows. One soaker, a bot with Divine Shield, Dispersion, Cloak of
  Shadows or Ice Block ready, non-healers first, else the free tank, walks into woken spirits within
  8 yd and 5 yd of the floor after its immunity or button; a non-tank with nothing left ready waits on
  its spot. The lowest-guid hunter with Frost Trap ready
  drops it once they wake. Ranged DPS shoot woken spirits within 20 yd of the stack, skipping a burst
  one, unselectable for its last 3 s.
- **Harvest Soul**, normal (`lk.harvest`): inside, non-healers attack the warden and kick Soul Rip
  from the second scan that sees it (Kick, Pummel, Shield Bash, Mind Freeze, Counterspell, Wind
  Shear); priest and paladin healers dispel it off Terenas or themselves, and every healer heals
  Terenas under 80%.
- **Heroic Frostmourne** (`lk.bombdodge`, `lk.stack` `fm`): the lowest-guid bot tank inside leads,
  else the lowest-guid bot. With a bomb's ground point within 13 yd it walks to the nearest spot 13 yd
  off every bomb, else off those landing within 4 s, else notes `stuck`. The rest stack within 3 yd
  of it, forced only while it dodges, reach held for everyone inside. Ranged DPS kill the woken
  Wicked Spirit nearest it within 30 yd of themselves.
- **Housekeeping** in the tick: the heroic buffs, a saved `rti` of cross or star reset to skull once
  per instance, the floor rescue.
- **Hooks**: lust in phase one, opener boss 36597, shadow and frost resistance.

`GetLkTarget` (`lk.target`), tanks and DPS, per bot per ms: a player's skull (not him for a braked
bot); in the outro him while levitating; then the late picks (Val'kyr bucket, siphoner, Vile Spirit,
warden, Wicked Spirit), and inside Frostmourne nothing else; nothing on a vehicle or under Harvest
Soul's stun. Winters: spirit tanks their spirit, the free tank then a leftover; ranged DPS the sphere
within 30 yd nearest its target, else a spirit or leftover in that reach; melee DPS the held spirit or
leftover, then any. Phase one: the holder him, the free tank a loose add, else its own; the stand-in a
loose ghoul, else its healthiest, which keeps threat and the plague's fodder alive longest; heroic melee
DPS a Frenzied Shambling; else him. Phases two and three: the free tank a loose or its own spirit,
ranged a leftover sphere (it still knocks its target off the platform), DPS the lowest spirit, else
him; a melee bot on a spirit within 12 yd of him leaves its stack node (`lk.stack` `p2 spirit`). A
braked bot gets nothing for him.

Rules: `OwnTargeting`; a `Block` of the pickers, follow, random moves and the pet picker while the
resolver holds a tank or DPS bot; `OwnMovement` over every Mover row, Attack kept under a `LichKing`
walk or stand, Attack and Reach under a `LichKingLate` one (a Harvest Soul victim walks to the Spirit
Warden, far stack nodes step into melee), Attack alone in heroic Frostmourne and on a walk round a
puddle; spell movers blocked; `AvoidAoe` (it flees a few yards at most, against the forced runs),
tanks' taunts and caster form blocked while live; the tank defensive hold. Multipliers: brake, splash,
cure hold.

## Decisions

Source, then the choice:

- WT, IC (him near the centre, adds faced off the raid; the plague must reach an add before its 5 s
  tick): phase one compact round C, not WT's adds on the rim. WT, WH: DPS ignore the adds, their area
  damage held near them; tanks' isn't, their area threat holds the pile.
- WH: the plague cured once delivered, by one curer. WT, WH: Enrage answered by hunters only
  (Anesthetic Poison needs weapon poisons bots don't manage), stuns only on heroic Frenzy.
- Script (arms after 5 s; any unit within 3 yd, a pet, totem or human too, fires its 10 yd throw): every
  bot keeps 11 yd off traps, not only the main tank.
- WT, WH, `spell_cone`: two winter clusters, Pain and Suffering's half-width plus 8° apart, growing
  away from each other (two centred clusters of 6 yd slots don't fit the margin); healers split but on
  10N. No pre-move at 71.5% or 41.5%, which keeps phase two's stack for the Val'kyrs.
- Script (a sphere spawns within 10 yd of him and follows a random player at 3 yd/s), WH (hunters
  suit spheres): off 10N the ranged cluster sits 44 yd or more off every path to the tank cluster, so
  the sphere guards stand in its middle, on a 25-man roster within 27 yd of every path to it and 7-29
  yd from the spirit spots; on 10N the ranged cluster at 20.5° covers them already.
- Gap lanes' one-bot-tank rule, WH (ghouls are plague fodder, abysmal damage): the stand-in holds
  ghouls only, its area damage still held near them, since a damage dealer's would burn the fodder.
  Taunt needs Defensive Stance and Growl bear form, so no warrior or druid. No stand-in
  takes a Shambling, a Raging Spirit, the Soul Reaper swap or a siphoner: `creature_template` swings
  of 50/131.9/90.5/214.8 × 177 per 2 s (Shambling, +200% enraged), 66.4/152.4/95.2/219.2 (spirit) and
  139/276.1/244.3/492.9 per 1.5 s (him, +100% haste after Soul Reaper) kill a damage dealer, and Life
  Siphon heals the Val'kyr 10× whoever it hits.
- Instance script: the ring is down from the first Quake's shatter until the second winter's cast, so
  bots wait at 42 yd. WT, WH: each Raging Spirit on the tank holding fewer, held facing out.
- Reach would walk a winter bot into Remorseless Winter: it is held under `LichKing` walks and stands,
  so winter ranged picks stay within 30 yd, every caster's base nuke range.
- WT, WH: back to 38 yd for the Quake, held for its 5 s. WH (swap once the tank has nothing left):
  Soul Reaper swaps only when no button runs or is ready for the hit; `NextTankDefensive` alone reads
  0 while one runs.
- His melee one-shots non-tanks: brake at 90%/80%. WH: lust in phase one, the DPS check while Plague
  Siphon ramps. Shadow and frost resistance: most damage is shadow, the winters frost.
- WT diagram, WH, IC: phase two stacked tight south of him at C, the holder north. WT, WH, script (the
  puddle lands at cast end): the target runs during the cast, no pre-spread; "12 yd off puddles" is
  past the edge, the side the half-plane through the stack centre.
- WH: Val'kyr buckets by guid with one stun and one slow each; WT (keep Val'kyr out of Defile, which
  grows on each player it hits): melee walk round a puddle, never across. Script (Life Siphon hits its
  last taunter): the free tank taunts siphoners.
- WT, WH: phase three side swapped at each Vile Spirits cast; the soaker's immunity first, Ice Block
  last since it roots the mage, then the free tank whether or not it has a button.
- WH: inside normal Frostmourne, DPS on the warden with a kick, healers on Terenas with a dispel. WH,
  IC: heroic Frostmourne moves as one group behind a leader, so reach is held there.
- Harvest Souls 73654 has no duration and ends with his channel: Harvest Soul's hold needs its stun.
- Script (combat disallowed, he can't be pulled): in the outro the resolver hands out him levitating,
  out of combat.

## Cheats

Removed: `lk.ragingspiritkill` (tanks taunt the spirits and DPS burn them), `lk.wickedspiritkill`
(ranged kill woken Wicked Spirits), `lk.hoj` (the stun rotation over the raid's kits),
`lk.frostmourneescape` (the Harvest Soul duty), the Val'kyr drop dive (buckets, stuns and slows), the
1 yd platform snap, which cancelled Shadow Trap and Ice Burst knockbacks mid-flight, and the heroic
Frostmourne snaps; earlier `lk.nitro`, `lk.painsuppression` and `lk.aegis`. No icon writes.

Kept, behind the raid cheat: `lk.floorsnap`, asked last: a bot under Z 838 within 44 yd of C, or under
1046 within 38 yd of F, is `NearTeleportTo`'d in place onto 840.857 or 1049.95. Both floors are
gameobject collision only, so only a collision fault puts a bot there, and without it the script kills
the bot at Z 700.

Record-only, `EnableICCBuffs`, heroic: `lk.heroicbuff` (every member), `lk.empoweredblood` (60-40%),
`lk.valkyrstun` (Hammer of Justice once per carrying Val'kyr).

## Known gaps

- No 631 trace: every threshold is a model until a pull. The user built `623638b99` clean, which
  carries this encounter. Nothing on either floor is probed or sight-checked: both floors and the
  ice shards at 41 yd are gameobjects with no vmap under them, so navprobe's `los` reads every ray
  clear.
- Sphere guards take Pain and Suffering with the tank cluster, and `--winters` shows whether they
  kill every sphere headed there before its Ice Burst.
- One tank player: Shamblings run loose on whomever they pick, every Raging Spirit lands on the lone
  bot tank, and nobody swaps on Soul Reaper (see Decisions).
- Gnaw through the ghoul and Intimidation's pet stun are untested (`--adds`).
- The farthest stack nodes can sit just past the 2.5 yd unpark from his melee reach, so a melee bot
  there may bounce (`--stands` moves). A chaser walks round only the first puddle in its line, and a
  ranged one with a puddle in between shoots only from in range.
- A bot whose trap dodge found no clear spot holds inside the throw rather than reaching.
- Trap reach and the walk round a puddle have no cap like the stands' 15 s: one that never lands holds
  its bot until the pick moves 2 yd, the trap goes or the Val'kyr drops (`lk.trap` `reach`, `lk.valkyr`
  `round`).
- Humans hold no duty.
- r14: the `IccInstanceState` Lich King members (`ICCScripts.h`) go unwritten; check
  `ICC_BUCKET_MARKERS`, `IccRtiNameForBucket` and `IccEnsureIconOn` for other users; hoist one walk
  base and the Cleansing Totem swap Rotface has too.

## Targets for the next pulls

25N first: `--phases` steps one, winter1, two, winter2, three, outro; `--tanks` one holder per phase,
a reaper swap only on an uncovered Soul Reaper, `lk.brake` on the free tank; `--plague` every carrier
`delivered` or `isolate` and few `emergency` cures; `--adds` every Enrage Tranquilized; `--winters`
a third of the ranged as sphere guards, spheres killed before Ice Burst and the Quake hold;
`--valkyrs` chained stuns, no drop, `round` only beside a puddle; `--defile` runs with no puddle hits;
`--vile` one soaker, no burst on the stack; `--frostmourne` the warden killed. 25H: the same plus
`--traps` with no hit and `reach` over `hold`, Frenzy stuns, siphon taunts, bomb dodges with no death
inside and no `winter2` after the return. With one tank player, `--tanks` names a stand-in.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `lk.phase` | per bot | `one`, `winter1`, `two`, `winter2`, `three`, `frostmourne`, `outro` |
| `lk.tank` | traced map, tank | `1` holder, `2` free, `3` stand-in, `0` off |
| `lk.brake` | traced set | `1` braked, `0` free |
| `lk.stun` | traced map, bot | heroic: the Frenzied Shambling its stun goes on |
| `lk.side` | traced value | `1` east, `2` west, `0` before phase three |
| `lk.stand` | per bot | `opening`, `holder`, `addtank`, `ranged <n>`, `winter tank <n>`, `winter guard <n>`, `winter ranged <n>`, `spirit`, `quake`, `none` |
| `lk.stack` | per bot | `p2 holder`, `p2 free`, `p2 <n>`, `p2 spirit`, `p3 east\|west holder\|free\|<n>\|spirit`, `fm`, `none` (n from 1, nearest him) |
| `lk.taunt` | per tank, DK, paladin | `reaper`, `back`, `spirit`, `add` with the unit, `none` |
| `lk.plague` | per bot | `walk <add>`, `delivered <add>`, `isolate`, `none` |
| `lk.cure` | per priest, paladin, shaman | `delivered`, `addtank`, `isolated`, `emergency` with the carrier, `totem`, `none` |
| `lk.enrage`, `lk.silence` | per hunter; per priest, paladin | the enraged Shambling; the silenced member; `none` |
| `lk.trap` | per bot, heroic | `dodge`, `reach`, `hold` with the trap, `none` |
| `lk.defile` | the target | the listener's cast, with its 10 yd hazard circle |
| `lk.defilerun` | per bot | `run`, `clear`, `puddle <defile>`, `none` |
| `lk.valkyr` | per bot | `dps`, `round`, `stun`, `slow` with the Val'kyr, `none` |
| `lk.siphon` | free tank, heroic | the siphoner after each taunt, `none` |
| `lk.vile` | per bot | `soak`, `trap`, `none` |
| `lk.harvest` | per victim, normal | `warden`, `kick`, `heal`, `dispel`, `none` |
| `lk.bombdodge` | per bot, heroic Frostmourne | the leader's `safe`, `dodge`, `stuck`; `none` |
| `lk.target` | per bot | rule and unit: `skull`, `outro`, `valkyr`, `siphon`, `vile`, `warden`, `wicked`, `frostmourne`, `held`, `brake`, `winter`, `sphere`, `spirit`, `leftover`, `boss`, `loose`, `add`, `frenzy`, `none` |
| `lk.defensive`, `lk.humancall` | per tank | the button, `idle`; the call to a human tank |

Change-only per bot; units are guid counters.

`tools/botobs/bosses/the_lich_king.py`: `--phases`, `--tanks`, `--stands`, `--plague`, `--adds`,
`--traps`, `--winters`, `--valkyrs`, `--defile`, `--vile`, `--frostmourne`, `--targets`, `--cheats`.
Besides the probes it counts plague ticks, Enrage, Tranquilizing Shot, Frenzy, trap, Defile and bomb
hits, and Spirit Burst hits by source. The banner names silent keys only for the phases the pull
reached, spares `lk.trap`, `lk.stun`, `lk.siphon` and `lk.bombdodge` off heroic and `lk.harvest` on
it, and never names the unused `IccInstanceState` keys `lk.winterinbound`, `lk.mtinbound`,
`lk.rangedinbound`, `lk.sharedslot`.
