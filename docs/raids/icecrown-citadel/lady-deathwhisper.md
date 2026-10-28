# Lady Deathwhisper

Boss id 1, slug `lady-deathwhisper`, prefix `ldw.`. Code: stem `Deathwhisper`
(`src/Ai/Raid/ICC/*/ICC*_Deathwhisper.*`); nodes `icc lady deathwhisper` dodge, defensive, frostbolt,
dominate mind, taunt, redirect, target, and the trash's `icc dark reckoning`. Raid conventions:
[README.md](README.md). Core: `boss_lady_deathwhisper.cpp`; Dark Reckoning is the Deathspeaker High
Priest's SmartAI.

## Mechanics

Ids are 10N/25N/10H/25H where they differ (`spelldifficulty` rows 2121-2125, 2133, 2185-2188, 2250 and
802, CSV only; the world DB table holds none of them); the Spell constructor remaps a cast, so the code matches
the base id through `GetSpellIdForDifficulty`. `GetEntry` reports the 10N entry on every difficulty.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Mana Barrier 70842 | Phase 1: each second heals her missing health out of her mana. Phase 2 on the first hit at least her remaining mana: barrier off, every threat wiped, she moves and melees | Burn the mana, end phase 1 just after a wave dies | `ldw.phase` reads the aura. Adds come first in the kill order, so the barrier mostly breaks between waves |
| Shadow Bolt 71254 | Phase 1, every 2.1 s on a random player, 2 s cast, no interrupt | Heal | Generic |
| Death and Decay 71001/72108/72109/72110 | Dynobject, 8 yd plus the player's 1.5 yd reach, 10 s; from 10 s, then every 22-30 s under a random player | Step out; melee spread round her | Dodge to a spot 12.5 yd from every pool, nearest the bot's target; generic movers held inside that band, and a melee bot whose target stands in a pool waits |
| Dominate Mind 71289 | From 30 s, every 40-45 s: none/1/1/3 players within 150 yd, never her victim or an add's; 12 s, +200% damage, +500% healing | Cyclone at once, any CC will do; stop cleaving | CC duty, one bot per victim (`ldw.cc`), see [Dominate Mind](#dominate-mind) |
| Adds | Phase 1 waves from 5 s, then 60 s (normal) or 45 s (heroic): 10-man 3 from one door, alternating; 25-man both doors plus one at the entrance. Heroic phase 2 every 45 s: 25H 3 from one door, 10H 1 at the entrance. Each goes for a random player | A tank per side; kill by immunity, melee on adherents, casters on fanatics | Phase 1: each bot tank owns one side by her y, the main tank the lower (east) one, and taunts its side's adds a non-tank holds. Phase 2: the tanks she isn't on take any loose add. DPS kill order below |
| Dark Empowerment 70901, Dark Transformation 70900 | Phase 1 only (the event is phase-bound): a random cultist every 18-25 s from 20-30 s, or Dark Martyrdom instead. Empowered Adherent: no interrupt, Deathchill Blast 10 yd. Deformed Fanatic: damage x3 (entry 38135 modifier 76.2 against 25.4) plus a +99% aura, 4 yd/s against 8 | Bounce the Deformed one between tanks and hunters; no face-tank without a major defensive | Empowered first for physical DPS, Deformed second for casters. The tank it is on in melee presses a defensive (physical buttons only); no bounce or kite, a slowed add walks the tank back into the raid |
| Dark Martyrdom 70903 (adherent), 71236 (fanatic) | 4 s cast, then 15 yd, 12k (25k heroic); back 6 s later as Reanimated: adherent immune to magic (71234), fanatic to physical (71235) | Move away; right school on each | Hazard of 15 yd (17.5 clear) while it casts; the kill order skips what the bot's school can't hurt |
| Shroud of the Occult 70768 | Adherent: 50k absorb, deflects spells, no interrupt | Break it with physical damage | Casters skip a shrouded adherent |
| Necrotic Strike 70659 (72490-72492) | Fanatic on its victim, every 11-13 s from 10-12 s: 70% of a swing and a heal absorb of 9k/14k/14k/20k | | Generic |
| Shadow Cleave 70670 (72006, 72493, 72494) | Fanatic, first 14-16 s then every 9.5-11 s: frontal cone, 8 yd, 14-21k; 60 degrees on 10N (`spell_cone`), 104 on the rest | Tanks face fanatics away; melee behind | Generic tank facing and rear flank |
| Vampiric Might 70674 | Fanatic every 20-27 s on a timer (the Tavern guide says at 25% health): +25% damage, heals 300% of what it deals | Spellsteal | Generic |
| Frost Fever 67767 (67932-67934) | Adherent, 9-13 s from 10-12 s, its victim: disease DoT and -15% attack speed | | Generic |
| Curse of Torpor 71237 | Adherent, random player, 14-16 s then 9-13 s: +15 s cooldowns, 15 s curse | Decurse at once | Generic: a druid's Remove Curse (57) and a mage's (40) |
| Deathchill Bolt 70594 (72005, 72488, 72489) | Adherent, 2 s cast on its victim, then every 9-13 s | Interrupt | Generic: melee and hunters take adherents first, so the bot on one kicks its bolt; empowered and shrouded ones can't be kicked |
| Frostbolt 71420/72007/72501/72502 | Phase 2, on her victim from 10-12 s, every 12 s; 2 s cast, 41-62k | Interrupt every one | Duty for the lowest-guid bot with a kick in reach and castable in its form (`ldw.interrupter`); she is immune to silence. The tank it is aimed at presses a defensive when 1.1 s of the cast is left and no bot's kick is ready |
| Frostbolt Volley 72905 (72906-72908) | Phase 2, raid-wide every 13-15 s from 19-21 s, 30% slow for 4 s | Heal | Generic |
| Touch of Insignificance 71204 | Phase 2, on her victim every 6-9 s from 6-9 s: -20% threat a stack, 5 stacks, 30 s. Heroic: no taunt or attack-me all fight (`creature_immunities` -287) | Normal: the second tank taunts. Heroic, no taunt: two tanks keep level on threat, Misdirection and Tricks on the tank, Divine Shield drops the stacks | Normal: the lowest-guid tank free of stacks, taunt ready and within 30 yd, taunts when she holds a tank at 3 stacks or a non-tank. Stacks last 30 s, so each later swap waits for the taunter's stacks to fade and lands at 4-5. Heroic: every tank she isn't on keeps hitting her; hunters and rogues redirect onto her tank. Every difficulty: a damage dealer holds fire on her from 90% of the top tank's threat until it falls under 80% (`ldw.brake`) |
| Vengeful Shade 38222 | Phase 2 every 12 s from 12-15 s, near 1/2/1/3 non-tanks (10N/25N/10H/25H). Summoned for 7 s (spell 71426), rooted 2 s, then chases the player nearest its spawn at 5.25 yd/s for 5 s (26 yd); no immunity to snare, stun or root (fear and horror only). Its first swing casts Vengeful Blast: 71544/72010 on that player, 72011 15 yd, 72012 20 yd round it | Run, Nitro Boosts; never let one touch | The shade's victim clears it by 16 yd once it is within 12, its cast broken, Nitro Boosts used. Heroic: everyone else clears the victim by the blast plus 2.5 once the shade is within 10 yd of it |
| Berserk | 10 min | | |
| Dark Reckoning 69483 | Trash, the High Priest: from 5-10 s, every 10-15 s on a random player; 8 s later 15k to everyone within 15 yd of it | | A marked non-tank bot walks 17.5 yd clear of every member; the rest clear a marked tank or player |
| Darnavan 38472/38485 | The weekly quest's first add of wave 1 (Deprogramming): Bladestorm, Charge, Intimidating Shout, Mortal Strike, Shattering Throw, Sunder Armor. Killing him fails the quest | | Not in the kill order; whoever he hits fights him with the generic nodes |

### Dominate Mind

- **First CC:** Cyclone, Hammer of Justice, Hex, Polymorph, Repentance, Fear, Blind, each only if
  castable, then the nearest. A druid in cat form or Tree of Life (a shaman in Ghost Wolf) drops the form
  and casts, ranked just behind a caster who needn't shift. Tanks hold no duty.
- **Follow-up:** a victim that already had a control aura takes an instant CC first (Hammer of Justice,
  Repentance, Blind). Cyclone makes its target immune to every cast, so a second Cyclone starts only at
  the fade and leaves the victim free for its 1.5 s cast; an instant spell lands the tick after.
- **Early duty:** in the last second of a Cyclone the duty goes out, and the caster walks into range and
  sight but casts only once the Cyclone is gone. A duty landing nothing in 4 s passes to another caster.
- **Splash hold:** area damage is held while a victim not under Cyclone is within 15 yd of the bot or its
  target.
- With no instant CC in the raid the follow-up is a Cyclone recast and the gap stays 1.5-2 s. A holder
  mid-cast at the fade waits out that cast: a bot mid-cast runs no triggers.

Kill order of the DPS resolver, after a player's skull:

- physical (melee, hunters): Empowered Adherent, Cult Adherent, Reanimated Adherent, Deformed Fanatic,
  Cult Fanatic, then her;
- magic (casters): Reanimated Fanatic, Deformed Fanatic, Cult Fanatic, Empowered Adherent, Cult
  Adherent (neither shrouded), then her.

Within a tier the bot keeps its current target, else the lowest health, then guid. The tanks' pick
ends on her too, so the targeting rule covers the whole fight.

### Tank defensives

`NextTankDefensive` (`RaidTankDefensive.h`) presses the weakest ready button, and nothing while one runs.
A tank bot asks it on three windows:

- a Deformed Fanatic on it in melee: physical buttons only, since Anti-Magic Shell does nothing against it;
- a Frostbolt on it with 1.1 s of the cast left and no bot with a kick ready in reach;
- under 35% health, the valve the held class nodes used to be.

The class buttons stay held off the tank for the whole fight, so nothing spends a long cooldown on a bare
health trigger. A human tank is told the same through `CallHumanTankDefensive`.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| No tank stand at the torches: each tank meets its side's adds where they come | Warcraft Tavern stands the tanks at the torches to break casters' sight, which a bot can't plan | navprobed stands at the posts (see [Line of sight](#line-of-sight)) |
| No opening positions | The guides put the raid behind her, inside her 23 yd aggro radius (detection 20, three levels up) | a stand that pulls her |
| No phase 2 drag: she is tanked where she stands | Wowhead drags her to each heroic wave's side; the free tank takes the add instead | the drag |
| Kill order by school, melee on adherents and casters on fanatics | instant-carry; the immunities 71234, 71235 and Shroud | one kill order |
| Lust waits for phase 2 on every difficulty | Wowhead (heroic) | normal free |
| Only the encounter taunts her: tanks' class taunts are blocked | All four entries carry taunt diminishing returns (`flags_extra` 0x80000), and generic taunts would spend them | generic taunts on |
| Dark Reckoning walks to a hazard-clear spot, not a fixed one | The old spot at the entrance was 106 yd from both priests, against 56 yd in 8 s | the fixed spot |
| Follow-up CC is an instant spell, handed out in the Cyclone's last second | CC chain rule in [raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds): `CanCastSpell` refuses a Cycloned target | a Cyclone recast, 1.5-2 s free |
| A form-blocked druid or shaman drops the form to cast | The guides: Cyclone from every druid spec; `CheckShapeshift` | leave them out of the duty |
| Deformed Fanatic is face-tanked on a defensive, not bounced | The Tavern guides bounce it between tanks and hunters; Wowhead kites it or face-tanks it on a major defensive. A bounce needs a free tank or a hunter in reach, and the add walks 4 yd/s, so a kite drags the tank into the raid | taunt ping-pong between the tanks, hunter Distracting Shot |
| Defensive windows: Deformed in melee, a Frostbolt nobody kicks, under 35% | Convention 43; Wowhead's Frostbolt 45-60k to the tank | a button on every Frostbolt |
| No window on her own swing or the Touch stacks | A steady hit every 2 s, about 11k (10N) to 28k (25H) before armor, is healed; Touch costs threat, not health; the 35% valve covers a bad stretch | a window on 25H phase 2 |
| Damage dealers hold fire at 90% of the top tank's threat, free under 80%, on every difficulty | ToC Anub'arak's cap; her pull is 110% (melee), and the "threat" value wraps past 255% so the threat list is read | heroic only, the `ldw.nothreat` buff |
| No Divine Shield | A shielded victim is immune, so she drops it (`ShouldBeSuppressed`) and, untauntable on heroic, takes whoever has next-most threat; two tanks already trade her by stacks | shield the victim at 4-5 stacks with a second tank in reach |
| A non-lethal dodge spot must see the bot's target | A spot behind a torch post drops the target (`invalid target`); a shade or its blast keeps the nearest spot | no sight test |
| Heroic blast clearance stays at the shade within 10 yd of its victim | The shade lives 5 s of chase and its victim outruns it, so the others' move is insurance; earlier would send the raid walking every 12 s | clear whenever a shade is in the blast radius |

## Line of sight

No fixed stand in this stem, so none to move. Probed with navprobe `ring --los-from` and `los` on the
room (floor Z 50.9 at the doors, 51.6-52.2 on her platform):

- Torch posts stand about 13.8 yd from her at the 45 and 315 degree bearings, near (-625.0, 2221.1) and
  (-625.0, 2201.7): a ray from her is clear to 13.5 yd and blocked from 14, and the slots behind them at 20
  and 30 yd are blind. Every other bearing is in sight out to 30 yd.
- Door spawn points (-619.3, 2154.5), (-598.9, 2155.0), (-619.4, 2268.5) and the entrance (-524.2, 2211.9)
  see her; (-578.7, 2154.2), (-578.7, 2269.9) and (-598.97, 2269.3) don't.
- The dodge sweeps for a spot that sees the bot's target ([Decisions](#decisions)); a Dominate Mind caster
  walks until it sees its victim.
- Not checkable offline: gameobject collision (a door, the elevator).

## Cheats

Replaced: `ldw.cyclone` by the CC duty, `ldw.insignificance` by the normal-mode swap and the heroic
threat parity with redirects and the brake.

Record-only, `EnableICCBuffs` on heroic, from the definition's tick: `ldw.nothreat` (1% Threat on a
non-tank she holds) and `ldw.spitefulfury` (the main tank, while she is on someone else). The
raid-wide strip takes the 1% Threat off once her fight ends ([README.md](README.md#cheats)).

## Known gaps

- The brake holds spells and swings, not an auto attack already running, and waits for a tank to have
  threat: at phase 2's open, before the tanks reach her, only the redirects and the buffs hold a ranged
  bot back.
- No instant CC in the raid leaves the 1.5-2 s Cyclone recast gap, and one Hammer of Justice (1 min)
  covers a single victim per round.
- A defensive per unkicked Frostbolt, and the next one each time a button ends under a Deformed Fanatic,
  spends long cooldowns; tank health is unmeasured.
- Darnavan is outside the kill order.
- No trace from this build yet, never built.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `ldw.phase` | per bot | `1` while Mana Barrier holds, `2` after |
| `ldw.target` | per bot | The resolver's pick: an add kind, `boss`, `loose` or `held` (a tank's add), `skull`, `none` |
| `ldw.cc` | caster → victim | The CC duty; `0` released |
| `ldw.interrupter` | per bot | `1` for the bot elected to kick a Frostbolt |
| `ldw.dodge` | per bot | The hazard walked out of: `dnd`, `martyrdom`, `shade`, `blast`, `none` |
| `ldw.defensive` | per tank | The ladder's answer while a window is open: a cast name, `covered`, `none`; an unchanged answer is not written again |
| `ldw.brake` | per damage dealer | `1` while it holds fire on her |
| `ldw.humancall` | note | A call to a human tank, with the spell named |

Cheat keys: `ldw.nothreat`, `ldw.spitefulfury`. `tools/botobs/bosses/lady_deathwhisper.py`:
`--phases`, `--targets`, `--cc` (each Dominate Mind, its duty, every control aura with its offset and the
time free of the 12 s), `--kicks` (each Frostbolt and its interrupter), `--dodges`, `--defensives`,
`--brake`, `--cheats`.

Next pull, 25N: every Dominate Mind free under 2 s, every Frostbolt with an interrupter (`--kicks`; read
`--defensives` beside any without one), no death to a shade or a pool, reanimated adds only on the right
school. 25H: nobody but a tank holds her
in phase 2 and `--brake` shows the holds, no heroic blast hits a second player, and a Deformed Fanatic
on a tank shows a `ldw.defensive` button.
