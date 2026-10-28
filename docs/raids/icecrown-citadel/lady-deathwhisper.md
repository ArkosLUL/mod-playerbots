# Lady Deathwhisper

Boss id 1, slug `lady-deathwhisper`, prefix `ldw.`. Code: stem `Deathwhisper`
(`src/Ai/Raid/ICC/*/ICC*_Deathwhisper.*`); nodes `icc lady deathwhisper` dodge, frostbolt, dominate
mind, taunt, redirect, target, and the trash's `icc dark reckoning`. Raid conventions:
[README.md](README.md). Core: `boss_lady_deathwhisper.cpp`; Dark Reckoning is the Deathspeaker High
Priest's SmartAI.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; the Spell constructor remaps a cast, so the code matches
the base id through `GetSpellIdForDifficulty`. `GetEntry` reports the 10N entry on every difficulty.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Mana Barrier 70842 | Phase 1: each second heals her missing health out of her mana. Phase 2 on the first hit at least her remaining mana: barrier off, threat wiped, she moves and melees | Burn the mana, end phase 1 just after a wave dies | `ldw.phase` reads the aura. Adds come first in the kill order, so the barrier mostly breaks between waves |
| Shadow Bolt 71254 | Phase 1, every 2.1 s on a random player, 2 s cast, no interrupt | Heal | Generic |
| Death and Decay 71001/72108/72109/72110 | Dynobject, 8 yd (10 centre to centre with reach), 10 s; from 10 s, then every 22-30 s under a random player | Step out; melee spread round her | Dodge to a spot 12.5 yd from every pool, nearest the bot's target; generic movers held inside that band, and a melee bot whose target stands in a pool waits |
| Dominate Mind 71289 | From 30 s, every 40-45 s: none/1/1/3 players within 150 yd, never her victim or an add's; 12 s, +200% damage, +500% healing | Cyclone at once, any CC will do; stop cleaving | CC duty, one bot per victim (`ldw.cc`): Cyclone, Hammer of Justice, Hex, Polymorph, Repentance, Fear, Blind, then nearest. Recast once under 1 s is left, since Cyclone leaves the victim immune. Area damage held while a victim not under Cyclone is within 15 yd of the bot or its target |
| Adds | Phase 1 waves from 5 s, then 60 s (normal) or 45 s (heroic): 10-man 3 from one door, alternating; 25-man both doors plus one at the entrance. Heroic phase 2 every 45 s: 25H 3 from one door, 10H 1 at the entrance | A tank per side; kill by immunity, melee on adherents, casters on fanatics | Phase 1: each bot tank owns one side by her y, the main tank the lower one, and taunts its side's adds a non-tank holds. Phase 2: the tanks she isn't on take any loose add. DPS kill order below |
| Dark Empowerment 70901, Dark Transformation 70900 | A random cultist every 18-25 s from 20-30 s, or Dark Martyrdom instead. Empowered Adherent: no interrupt, Deathchill Blast 10 yd. Deformed Fanatic: +100% damage, half speed | Bounce the Deformed one between tanks and hunters | Empowered first for physical DPS, Deformed second for casters; the Deformed one is face-tanked |
| Dark Martyrdom 70903 (adherent), 71236 (fanatic) | 4 s cast, then 15 yd, 12k (25k heroic); back 6 s later as Reanimated: adherent immune to magic (71234), fanatic to physical (71235) | Move away; right school on each | Hazard of 15 yd (17.5 clear) while it casts; the kill order skips what the bot's school can't hurt |
| Shroud of the Occult 70768 | Adherent: 50k absorb, deflects spells, no interrupt | Break it with physical damage | Casters skip a shrouded adherent |
| Curse of Torpor 71237 | Adherent, random player, +15 s cooldowns, curse | Decurse at once | Generic dispels |
| Deathchill Bolt 70594 | Adherent, 2 s cast | Interrupt | Generic interrupts of the bots on it |
| Frostbolt 71420/72007/72501/72502 | Phase 2, on her victim from 10-12 s, every 12 s; 2 s cast, 45-60k | Interrupt every one | Duty for the lowest-guid bot with a kick in reach (`ldw.interrupter`); she is immune to silence |
| Frostbolt Volley 72905 | Phase 2, raid-wide every 13-15 s from 19-21 s, 30% slow | Heal | Generic |
| Touch of Insignificance 71204 | Phase 2, on her victim every 6-9 s: -20% threat a stack, 5 stacks, 30 s. Heroic: no taunt or attack-me all fight (`creature_immunities` -287) | Normal: the second tank taunts. Heroic, no taunt: two tanks keep level on threat, Misdirection and Tricks on the tank | Normal: the lowest-guid tank free of stacks, taunt ready and within 30 yd, taunts when she holds a tank at 3 stacks or a non-tank. Heroic: every tank she isn't on keeps hitting her; hunters and rogues redirect onto her tank |
| Vengeful Shade 38222 | Phase 2 every 12 s from 12-15 s, near 1/2/1/3 non-tanks. Rooted 2 s, then chases the player nearest its spawn at 5.25 yd/s for about 5 s; its first swing casts Vengeful Blast: 71544/72010 on that player, 72011 15 yd, 72012 20 yd round it | Run, Nitro Boosts; never let one touch | The shade's victim clears it by 16 yd once it is within 12, its cast broken, Nitro Boosts used. Heroic: everyone else clears the victim by the blast plus 2.5 once the shade is within 10 yd of it |
| Berserk | 10 min | | |
| Dark Reckoning 69483 | Trash, the High Priest: from 5-10 s, every 10-15 s on a random player; 8 s later 15k to everyone within 15 yd of it | | A marked non-tank bot walks 17.5 yd clear of every member; the rest clear a marked tank or player |

Kill order of the DPS resolver, after a player's skull:

- physical (melee, hunters): Empowered Adherent, Cult Adherent, Reanimated Adherent, Deformed Fanatic,
  Cult Fanatic, then her;
- magic (casters): Reanimated Fanatic, Deformed Fanatic, Cult Fanatic, Empowered Adherent, Cult
  Adherent (neither shrouded), then her.

Within a tier the bot keeps its current target, else the lowest health, then guid. The tanks' pick
ends on her too, so the targeting rule covers the whole fight.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| No tank stand at the torches: each tank meets its side's adds where they come | Warcraft Tavern stands the tanks at the torches to break casters' sight, which a bot can't plan | navprobed stands |
| No opening positions | The guides put the raid behind her, inside her 23 yd aggro radius (detection 20, three levels up) | a stand that pulls her |
| No phase 2 drag: she is tanked where she stands | Wowhead drags her to each heroic wave's side; the free tank takes the add instead | the drag |
| Kill order by school, melee on adherents and casters on fanatics | instant-carry; the immunities 71234, 71235 and Shroud | one kill order |
| Lust waits for phase 2 on every difficulty | Wowhead (heroic) | normal free |
| Only the encounter taunts her: tanks' class taunts are blocked | All four entries carry taunt diminishing returns (`flags_extra` 0x80000), and generic taunts would spend them | generic taunts on |
| Dark Reckoning walks to a hazard-clear spot, not a fixed one | The old spot at the entrance was 106 yd from both priests, against 56 yd in 8 s | the fixed spot |

## Cheats

Replaced: `ldw.cyclone` by the CC duty, `ldw.insignificance` by the normal-mode swap and the heroic
threat parity with redirects.

Record-only, `EnableICCBuffs` on heroic, from the definition's tick: `ldw.nothreat` (1% Threat on a
non-tank she holds) and `ldw.spitefulfury` (the main tank, while she is on someone else). The
raid-wide strip takes the 1% Threat off once her fight ends ([README.md](README.md#cheats)).

## Known gaps

- The Deformed Fanatic is face-tanked, not bounced.
- No threat brake: on heroic, once both tanks carry stacks, a damage dealer can take her.
- No Divine Shield to drop Touch stacks.
- Deathchill Bolt and Curse of Torpor are left to the generic interrupt and dispel nodes.
- A shade's victim under Frostbolt Volley's slow runs 4.9 yd/s, slower than the shade.
- Heroic blast clearance waits for the shade within 10 yd of its victim.
- The lust hook (`IccDeathwhisperBurstWindow`) needs its `HookRows()` row.
- No trace from this build yet.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `ldw.phase` | per bot | `1` while Mana Barrier holds, `2` after |
| `ldw.target` | per bot | The resolver's pick: an add kind, `boss`, `loose` or `held` (a tank's add), `skull`, `none` |
| `ldw.cc` | caster → victim | The CC duty; `0` released |
| `ldw.interrupter` | per bot | `1` for the bot elected to kick a Frostbolt |
| `ldw.dodge` | per bot | The hazard walked out of: `dnd`, `martyrdom`, `shade`, `blast`, `none` |

Cheat keys: `ldw.nothreat`, `ldw.spitefulfury`. `tools/botobs/bosses/lady_deathwhisper.py`:
`--phases`, `--targets`, `--cc` (each Dominate Mind, its duty and the first CC on it), `--kicks`
(each Frostbolt and its interrupter), `--dodges`, `--cheats`.

Next pull, 25N: every Dominate Mind held within 2 s, every Frostbolt with an interrupter, no death to a
shade or a pool, reanimated adds only on the right school. 25H: nobody but a tank holds her in phase
2, and no heroic blast hits a second player.
