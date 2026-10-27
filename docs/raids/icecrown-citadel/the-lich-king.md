# The Lich King

Boss id 12, trace slug `the-lich-king`, prefix `lk.` ([README](README.md)). Code: `Action/ICCActions_LK.cpp`
and the Lich King regions of the shared ICC files. Mechanics: `boss_the_lich_king.cpp`.

## Mechanics

- `IN_PROGRESS` from his `JustEngagedWith`. Berserk at 15 min. The script kills any player under Z 700
  every 5 s, and counts as on the platform only a unit within 90 yd of the centre (503.6, -2124.7)
  between Z 840 and 875.
- **Phase one** to 70%: Shambling Horrors (15 s, then 60 s), Drudge Ghouls (10 s, then 30 s), Infest
  (5 s, then 22.5 s, a 2 s cast), Necrotic Plague (30-31 s, rows 2289 and 2280 for the jump), heroic
  Shadow Traps (every 15.5 s).
- **Transitions** at 70% and 40%, on the first hit at or under while he is not casting: passive, he
  walks to the centre and casts Remorseless Winter there, a 2.5 s cast and 60 s aura (row 2293 the
  first, 2294 the second). Pain and Suffering from 3.5 s (every 1.75-2.25 s, 1.25-1.75 s heroic),
  Raging Spirits from 4 s (every 20 s, 15 s under 40%), Ice Spheres from 8 s (every 7.5 s). Quake,
  62.5 s after he reaches the centre, a 1.5 s cast, breaks the platform's edge; he turns aggressive
  5.25 s after it lands. The first winter's opening hit despawns the Shadow Traps, the second's arrival
  the Val'kyrs.
- **Phase two** to 40%: Infest (14 s), Val'kyrs (20 s, then 45 s; one on 10-man, three on 25-man),
  Soul Reaper on his tank (40 s, then 30.5 s), Defile (38 s, then 32.5 s; 72762, a 2 s cast, no
  difficulty row). A heroic Val'kyr drops its victim at 50% health.
- **Phase three** from the second Quake to 10%: Soul Reaper, Defile, Vile Spirits (20 s, then 30 s).
  Normal: Harvest Soul (14 s, then 75 s; row 2298, instant) stuns one non-tank within 55 yd for 6 s
  and carries it into Frostmourne, where Terenas at half health fights the Spirit Warden; at 1 min the
  warden destroys the soul. Heroic: Harvest Souls (14 s, then 100-110 s; row 2295, instant) takes the
  whole raid in for up to 55 s while he stays passive: Spirit Bombs and Wicked Spirits. The
  Frostmourne room is the 40 yd round (495.7, -2523.8, 1050) the script tests.
- At 10% the outro stops combat.
- Raging Spirits are stun-immune, Shambling Horrors and Val'kyrs are not (`creature_immunities` -361,
  -339, -219); Wicked Spirits and Ice Spheres are taunt-immune (-362); he obeys taunt diminishing
  returns (`flags_extra` 0x80000).

## Phase

`IccLichKingPhase(bot, boss)` (`ICCTriggers`) publishes `lk.phase`; the winter trigger asks it for
every bot each tick:

| Value | When |
|---|---|
| `one` | above 70% |
| `winter1`, `winter2` | the winter aura or cast of that row; else passive above 40% (`winter1`) or at it and under (`winter2`): the walk and the Quake's wait |
| `two`, `three` | aggressive above 40%, at it and under |
| `frostmourne` | heroic, passive under 40%, for a bot inside Frostmourne or carrying Harvest Souls |

A unit other than entry 36597 reads `one` and writes nothing: Valithria's room has a "The Lich King"
voice NPC, so every Lich King trigger also bails while she is within 100 yd. Sites test the order:
"past the first threshold" is `>= winter1`, so the walk counts with the phase after it. Phase one
ends at the script's 70%, the centre stack starts at `two` and the Vile Spirit slots at `winter2`,
with no health bands of their own. The winter action, its multiplier block
and the adds action's winter guards key on the winter itself (`IccBossHasRemorselessWinter`), a
narrower window. The 60-40% Empowered Blood band is an operator buff and stays on health.

## Tactics

- **Phase one**: the main tank holds him at `ICC_LICH_KING_MELEE_POSITION`, the assist tank collects
  Shamblings at the adds spot, ranged at the ranged spot; heroic stacks everyone else on the main
  tank. Plagued bots walk to a Shambling and cures wait until every plague is delivered. Hunters
  Tranquilize enraged Shamblings.
- **Winter**: on the cast non-tanks walk to one staging anchor (Vile Spirit spot 1 or 3 nearest the
  raid's centroid, else 2) and hold 4 s, then take the frost spot nearest the centroid: tanks on it,
  melee between it and its ranged spot until a tank holds an add, ranged on spheres and the skull.
  Skull goes on a Shambling, else a Raging Spirit, on the main tank. Nitro Boosts within 35 yd of him.
- **Quake**: everyone steps to 35-45 yd from him.
- **Phase two**: the raid stacks at the centre; each non-main-tank bot takes a Val'kyr bucket by guid
  order, with its icon, and CCs it. The Defile target runs 10 yd out on Nitro Boosts; others step out
  of puddles.
- **Phase three**: the raid stacks on Vile Spirit spot 1 or 3; during spirits one shared slot, the
  assist tank on a 40 yd leash and Nitro Boosts, the lowest-guid bot hunter Frost Traps the centre,
  others flee spirits toward the main tank except while a raider carries Harvest Soul.
- **Heroic Frostmourne**: everyone stacks on the main tank, ranged kill the starred Wicked Spirit, the
  main tank steps clear of Spirit Bombs.
- The main and assist tanks taunt every loose add in a pass, one per 8 s cooldown (README, "The class
  taunt").

## Cheats

Removed: `lk.nitro` (the winter run, the Defile target's run, the assist tank's spirit chase; the
Defile site also reached humans), now the bot's own boots through `UseNitroBoosts`;
`lk.painsuppression` and `lk.aegis` on the assist tank, with no replacement.

Kept, behind the raid cheat:

| Key | Does | A legit answer needs |
|---|---|---|
| `lk.ragingspiritkill` | Heroic winter: the main tank kills the fullest Raging Spirit past two | Killing them |
| `lk.wickedspiritkill` | Heroic Frostmourne: ranged kill the starred Wicked Spirit outright | Killing it |
| `lk.hoj` | Phase one's last 2%: the assist tank, any class, stuns every Shambling | A stun rotation over the raid's own kits: Shamblings are not stun-immune, but each stun covers one per cooldown |
| `lk.frostmourneescape` | Normal: a Harvest Soul victim is teleported to the main tank on its first tick in Frostmourne, skipping the Spirit Warden | A Frostmourne duty: kill the warden, keep Terenas up |

With the cheat off a normal victim stays inside, the adds action leaves it to generic combat, and the
script brings it back.

Record-only, `EnableICCBuffs`: `lk.heroicbuff` (every member), `lk.empoweredblood` (60-40%),
`lk.valkyrstun` (Hammer of Justice on a bot's Val'kyr), heroic only.

Documented, not wrapped: the Val'kyr drop dive. A bot over the edge (52-70 yd out, above Z 844) puts
30440 on itself, jumps to Z 740 and kills itself.

## Unstucks

All `NearTeleportTo`, which keeps combat: the platform snap (winter and adds actions) for a bot more
than 1 yd off 840.857, in place, never inside Frostmourne; heroic Frostmourne bots more than 5 yd off
its floor, onto the main tank, or in place for the main tank.

The 1 yd band also fires on any jump or knockback and cancels it mid-flight: heroic Shadow Traps
(73529, which only the main tank dodges) and the Ice Spheres' Ice Burst (69108). The script counts a unit on the platform within 90
yd of the centre between Z 840 and 875, but a new value waits on a navprobe reading of the platform,
and a snap under the floor alone would let those knockbacks land, which changes play.

## Known gaps

- Icons by raid icon (README); `lk.skull`, `lk.cross`, `lk.star` and `lk.circle` record every move.
- The kept gap cheats; with the raid cheat off a normal Frostmourne victim has no duty there.
- The platform snap's 1 yd band (Unstucks); a bot fallen off the broken edge is snapped back over the
  gap and falls again.
- `lk.phase` reads `two` or `three` for the at most 2 s cast that delays a threshold, and `winter2`
  for a bot left outside heroic's Frostmourne.
- The Harvest Soul hold reads normal's 6 s victim aura; heroic's Harvest Souls aura has no timed end,
  so it stays outside the hold.
- The Val'kyr bucket on skull keeps its `rti` while a Raging Spirit holds the skull.
- Humans keep Experienced and 1% Threat after a heroic pull: nothing strips them.
- No ICC trace yet carries these probes, so nothing here is measured.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `lk.phase` | per bot | `one`, `winter1`, `two`, `winter2`, `three`, `frostmourne` |
| `lk.winterstage` | the bot that latched it | The winter and its staging anchor: `spirit1`-`spirit3`, `none` |
| `lk.winterinbound`, `lk.mtinbound`, `lk.rangedinbound` | traced map, bot → walking | The winter walks to the anchor, the frost spot, the ranged spot |
| `lk.valkyr` | per bot | Its bucket's icon and Val'kyr, `none` with no Val'kyr grabbing |
| `lk.stackslot` | the bot that refreshed it | The phase three stack: `spirit1`, `spirit3`, `none` |
| `lk.sharedslot` | traced value | The Vile Spirit slot, 0-2, -1 none |
| `lk.bombdodge` | main tank | `safe`, `dodge`, `escape` through the lowest bomb, `stuck` |
| `lk.defile` | the target | The Defile cast, with its 6 yd hazard circle |
| `lk.skull`, `lk.cross`, `lk.star`, `lk.circle` | the bot that moved the icon | Rule and unit, or `clear`: winter `shambling`, `spirit`; outside it `boss`, `spirit`; `valkyr`; Wicked Spirit `chasing`, `nearest`; `sphere` |

`lk.circle` is rare: the adds action that places it is vetoed while the winter is up, so only a sphere
outliving its winter is marked.

`tools/botobs/bosses/the_lich_king.py`; its banner names declared keys the pull never wrote, sparing
`lk.cross` on 10-man, `lk.star` on 10N, `lk.bombdodge` off heroic:

- `--phases`: the `lk.phase` steps;
- `--assignments`: winter anchors and walks, Val'kyr buckets, stack and Vile Spirit slots, Defile
  targets, bomb dodge rules;
- `--icons`: every icon move;
- `--cheats`: this encounter's keys.
