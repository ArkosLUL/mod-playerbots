# Festergut

Boss id 4, slug `festergut`, prefix `fg.`. Code: stem `Festergut` (`src/Ai/Raid/ICC/*/ICC*_Festergut.*`);
nodes `icc festergut` goo, bloat clear, pungent defensive, pungent cover, tank defensive, bloat swap,
brake, target, stand; multiplier `icc festergut brake`. Raid conventions: [README.md](README.md). Core:
`boss_festergut.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; the Spell constructor remaps a cast, so code matches the base
id through `GetSpellIdForDifficulty`. Inhaled Blight's heroic columns are empty and fall back to the
normal id of the same size. Gas Spore's aura 69279 has no row. His combat reach is 8, 20% more a stack of
Inhaled Blight (its scale aura), so a player's melee range from his centre is 10.83 yd, 15.63 at three
stacks. Every area spell here comes from a creature or reaches allies, so its radius is exact, centre to
centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Gaseous Blight 69157/69162/69164 | Putricide fills the room from his balcony at the pull: shadow every 2 s, three levels, each Inhale Blight a level less | Heal through | Generic heals; a paladin's Shadow Resistance Aura (`HookRows()`) |
| Inhale Blight 69165, Inhaled Blight 69166 (10), 71912 (25) | 3.5 s cast every 34 s from 25-30 s; each stack +25% (10) or +30% (25) damage and attack speed, three at most | Tank cooldowns: minor at 2 stacks, major at 3 | Holder spends a button at 2 once a cycle, keeps one up from 3 (`fg.defensive`) |
| Pungent Blight 69195/71219/73031/73032 | The fourth inhale event: 3 s cast, 42.9-45.1k / 48.8-51.3k / 63.4-66.6k / 73.1-76.9k shadow to everyone; takes his stacks and everyone's Inoculated; next Gas Spore 20-25 s later | Three Inoculated; Aura Mastery with Shadow Resistance Aura, shields; immunities | `GetRaidCover` and `GetRaidPersonalDefensive` on the cast, the hit cut 25% per Inoculated stack |
| Gas Spore 69278 (10), 71221 (25) → 69279 | 2 (10) or 3 (25) random players, first at 20-25 s, every 40-45 s; bursts at 12 s: Blighted Spores 69290/71222/73033/73034 on everyone within 8 yd of the carrier (1.95-2.05k / 2.9-3.1k / 2.9-3.1k / 3.9-4.1k shadow a second for 6 s); Inoculated 69291/72101/72102/72103 as it ends, 120 s, −25% shadow area damage a stack, three at most, one per 10 s | Groups stack on a carrier: WT10 melee with tanks and ranged; WT25 tanks, melee, ranged; a spare carrier moves to an empty group | Two groups, a deal per wave (`fg.carrier`, `fg.stand`) |
| Vile Gas 69240/71218/73019/73020 | From 30-40 s, every 28-35 s, held to 20 s after every spore. On a random player past his melee by 5 yd (14.5 yd from his centre) once 3 (10) or 8 (25) stand there, else anyone: everyone within 8 yd of it, then each victim within 8 yd of its allies, 3.9-4.1k / 4.9-5.1k / 4.9-5.1k / 6.3-6.7k every 2 s for 6 s and disoriented | Ranged and healers 8 yd apart | 18 slots, 10.4 yd apart, out past 14.5 yd; melee fill when too few stand out, players counted by spec |
| Gastric Bloat 72219/72551/72552/72553 | On his victim from 12.5-15 s, every 15-17.5 s: 9.8-10.3k / 12.2-12.8k / 12.2-12.8k / 14.6-15.4k nature, +10% damage done a stack, 100 s; at 10 Gastric Explosion 72227/72228 (10 yd, 29.3-30.8k) or 72229/72230 (everyone, 48.8-51.3k). School 40, no `SPELL_ATTR0_NO_IMMUNITIES`: Divine Shield 642 purges it on apply | Swap at 9 once the other tank's has dropped; the relieved tank stops attacking a while; a protection paladin can bubble it off (Wowhead) | Elected swap at 9 (`fg.swap`); threat brake (`fg.brake`); a paladin holder nobody can relieve bubbles (`fg.bloatclear`) |
| Malleable Goo (heroic) 72296 → 72297/72548/72549/72550 | Putricide, from his balcony (4324.8, 3166.0, 389.4), at a random player but his victim, from 15-20 s, every 15-20 s; lands where it stood at the throw after the 12 yd/s flight (46-97 yd to a stand, 3.9-8.1 s): 5 yd, 14.6-15.4k nature (72549/72550), 20 s swing and cast slow | Ranged dodge the circle; melee switch legs | Forced dodge, leg and far-anchor flips, spore stack shifts (`fg.goo`, `fg.leg`, `fg.sporespot`, `fg.closeshift`, `fg.farshift`) |
| Berserk | 5 min | | |

His 2 s swing before armour and Inhaled Blight: 35.2-48.5k / 57.5-79.2k / 54.4-75k / 81.9-112.8k
(level 83's 177 base plus 805 attack power / 14, times damage modifier 75 / 122.6 / 116 / 174.5 and the
2 s attack time).

His boss state moves to `IN_PROGRESS` only once Putricide reaches his balcony point, about 7 s in, so
the trace opens on the engage and every bot rule reads his combat, not the state, with the bot within 60
yd of his spawn (his boundary lies 55-63 yd off it). He obeys taunt
diminishing returns (`flags_extra` 0x80000 on all four entries). His boundary is x 4205-4325, y
3082-3195.

## Duties

- **Tanks**: one tank spot each, 6.5 yd from his spawn at 40° either side of west and 8.4 yd apart, so
  he turns to the holder without moving and goo on the free tank misses the holder; a third tank takes the
  legs, and while it holds him trades stands with the tank-spot tank carrying the most Gastric Bloat.
  Generic taunts are blocked while he fights. Once the holder carries 9 Gastric Bloat, or a non-tank
  holds him, the bot tank with the fewest stacks, at most 7, taunt ready in its form within 30 yd, taunts
  him, one on a tank spot before a third tank; nobody does while a taunt aura is on him, since one just
  landed and his victim catches up next update. A tank not holding him stops attacking him at 90% of the
  holder's threat, its pickers and its area damage within 14 yd of him held, until under 80%; a braked
  druid stays in bear form, which its non-combat nodes would drop with no target. A human partner is
  called to taunt when no bot can.
- **Nobody to take him** at 9 (one tank, or none able): a bot paladin holder free of Forbearance and
  61987 (either refuses the bubble, `spell_pal_immunities`) casts Divine Shield and cancels it as it
  lands (`fg.bloatclear`), at once when no other tank lives, else 6 s after the 9th stack; a human
  paladin holder is called to. No other class can: his stacks need 100 s to drop, and no damage
  dealer outlives his swing that long.
- **Drag**: once he stands 3.5 yd off his spawn outside a spore wave, a bot holder walks past his spawn,
  on the far side from him, by the distance his chase stops at: both combat reaches plus 0.5, 10 yd and
  14.8 at three Inhaled Blight (`fg.drag`, stand `drag`). He follows a tank past his melee range, so he
  stops on his spawn. It ends within 2.5 yd, or when a wave starts, where the holder keeps its tank spot.
- **Tank defensives** (`fg.defensive`, physical only): the holder spends one button at 2 Inhaled Blight
  once a cycle (a cycle ends at each Pungent Blight), and one whenever none runs from 3; any tank under
  40%. Class nodes stay held until 25%. A human holder gets the same windows as calls (`fg.humancall`).
- **Melee**: both stand on one leg, 7 yd out south-east or north-east, behind him whichever tank he faces;
  `fg.leg` flips to the other once goo lands within 8.25 yd of the one in use and the other is clear. The
  legs release once he stands 3.5 yd off his spawn, until the drag brings him back.
- **Ranged and healers**: one latched slot each (`fg.slot`), released once he is 10 yd off his spawn
  (reach stays blocked on a slot, so a holder would watch him walk out of range).
  Healers take 1, 2, 3, 8, 9, 10 first; from there both tank spots and all but two or three slots are in
  40 yd. While fewer than 3 (10) or 8 (25) stand out, bots on slots plus players in ranged or healer specs,
  melee fill the free ones, nearest the legs first. Reach and spell movers are held on a stand, heal reach
  is not.
- **Spores**: per wave, the tanks and melee are the close group, stacking on the anchor between the tank
  spots, the ranged and healers the far group, on slot 1 east of him or 5 north (`fg.sporespot`, flipped
  like the legs once goo lands within 10.75 yd). A group with no spore takes the one nearest it off a group
  with two (a melee for the far group, never the holder); a group still without one stacks with the other.
  A bot carrier stands on its anchor, everyone else on a 2.5 yd ring round it; a group carried only by the
  holder rings round him, one carried only by players round the first of them. A swap inside the wave
  deals again. The holder never moves.
- **Stack shift**: goo within 10 yd of the close stack, or of a far stack the flips can't move (both anchors
  fouled, or ringing a player), sends the whole group onto one spot 7.5 yd clear of every impact
  (`fg.closeshift`, `fg.farshift`): the nearest to its stack within 6.75 yd of its fixed player (the player
  it rings, else for the close group the holder), so his burst still lands; failing that the nearest clear
  spot within 12 yd, the fixed player left out (value 2). It holds for the wave unless goo fouls it or the
  fixed player walks 7.25 yd off it. With no clear spot at all the group goes back to its stack and dodges
  one by one.
- **Malleable Goo**: a bot other than the holder within 6.5 yd of a live impact interrupts its cast and
  walks, forced, to its own stand if that is 7.5 yd clear, else to the nearest clear spot, a ranged one
  8.5 yd from everyone (so one Vile Gas still takes one player) outside a spore window. A stand under goo waits until the impact ages out.
- **Pungent Blight**: during the cast the raid cover is dealt on the lowest Inoculated stack on the map,
  its class nodes held over the lead-in; each bot asks for its own button on its own stacks, so a bot
  short of Inoculated takes an immunity or a healthstone.
- **Targets** (`fg.target`): a player's skull, else him; a braked tank nothing.

Stands are in `ICCHelpers_Festergut.h` round his spawn (4267.92, 3137.26): slots 1-7 at 18 yd and 8-18 at
28.5 yd, behind and beside him. Every one is navprobe-clean (`--nav 0x09`, settled z 360.385); `los` from
each and its 3x3 neighbourhood at 1 yd to him and both tank spots is clear (657 rays); the walks between
them are straight normal paths. Rings at 2-44 yd (25 radii, 72 headings) all settle at 360.385 and see his
spawn out to 42 yd (one heading blind at 44), so the derived spots, the drag's 10-14.8 yd ring and shifted
stacks kept within 40 yd, need no probe of their own. Gameobject collision isn't in navprobe's sight.

## Decisions

| Choice | Source | Alternative |
|---|---|---|
| Swap at 9, taunter at most 7 | WT25, Wowhead; script: 100 s at 15-17.5 s a stack, so a tank relieved at 7-9 is clean before its partner gets there, while at 6 both tanks can sit at 6 together | 8 for a second taunt try |
| One elected taunt, generic taunts blocked | taunt diminishing returns | |
| Threat brake on every tank not holding him, 90% hold, 80% free | Wowhead "stop attacking for a while", WT25 off-tank over-aggro; Deathwhisper's brake | Full yield, ~100 s idle |
| Two spore groups on every difficulty | WT10; Wowhead (melee and tanks collapse together) | WT25's third tank group |
| A group with no carrier collapses on the other, ranged into melee included | script: Vile Gas held 20 s past every spore | Wowhead: ranged never collapse into melee |
| Goo 5 yd, run at 6.5, stand at 7.5, live 8.5 s from the throw | DBC 72297 row 2197; the farthest stand's flight | Each impact's own flight |
| Pungent Blight hit cut by Inoculated | `ExpectedDamage` ignores aura 229 | Raw damage, a cover on every cast |
| No opening stands | stands inside his aggro radius pull him; every stand is seconds from the pull | Stands at the door |
| Legs at 135°, 7 yd, released 3.5 yd off his spawn | his 10.83 yd melee range; behind him for either tank | Legs derived from where he settles |
| Heal reach kept on slots | the spread is 55 yd wide; the heal pick adds 30 to a member past 38.5 yd, so a healer walks only for one 30 points lower than everyone in reach; slot 1 reaches every stand, slots 2 and 3 all but two each, each covering the other's | Held, as Saurfang does |
| Lust free | neither guide times it; a 5 min DPS race | Lust at 3 Inhaled Blight |
| Drag past 3.5 yd to his chase stop beyond his spawn, done at 2.5, never in a spore wave | core chase: past his melee range he walks to both reaches plus 0.5 off a tank, a fixed 9 yd leaves him short from one Inhaled Blight and unmoved from two; a holder leaving the close stack costs the tanks Inoculated | Legs derived from where he stands |
| A shifted stack is one spot, kept for the wave | no 2.5 yd ring fits between the 7.5 yd goo clearance and a fixed player 8 yd off; Vile Gas is held for the window; a walk back can land on the burst | Members dodge one by one and wait the impact out |
| A shift that can't keep its fixed player in still moves the rest | the others keep their stack | Leave the group to dodge |
| Third tank holding him trades stands with the relieved tank | his facing and the close stack follow the tank spots | Third tank on the close anchor, inside goo reach of both spot tanks |
| Players count toward Vile Gas's quota by spec | a position count flaps melee in and out as players move | Players past 14.5 yd |
| Paladin bubble at 9, cancelled as it lands, 6 s late while another tank lives | Wowhead; kept up, the immunity suppresses the holder and he turns to the next on his threat list; the next stack lands 15-17.5 s after the 9th | Bubble kept up |

## Cheats

None ever.

## Known gaps

- Goo landing within 5 yd of the holder still hits him: the holder never moves. A goo near the holder
  that the shift can't clear within 6.75 yd of him leaves him out of the burst (`fg.closeshift` 2).
- One bot tank that is no paladin, with no human able to taunt, explodes at the 10th Gastric Bloat unless
  he dies first (about 150 s in).
- The drag rests on the core's chase stop distance; `fg.drag` spans and `--stands` show whether each one
  ends.
- A cancelled bubble leaves the holder suppressed on his threat list until its next hit or Consecration
  tick, and his once-a-second threat check in between can hand him to the next player for a swing.
  `--tanks` shows his target runs round each bubble.
- Humans hold no duty. Hooks, the resist aura included, answer only once he is live (about 7 s in).
- Built clean at `623638b99` (2026-10-29); no 631 trace from a build with this stem yet.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `fg.phase` | per bot | `spore` while any group member carries Gas Spore, else `normal` |
| `fg.slot` | per bot | 1-18 a slot, 19 the legs, 20-21 a tank spot; `0` released |
| `fg.stand` | per bot | The stand held now: `slot`, `leg`, `tank`, `close`, `far`, `drag`, `none` |
| `fg.carrier` | carrier → group | 1 close, 2 far; `0` once the wave ends |
| `fg.leg`, `fg.sporespot` | raid-wide | The melee leg, the far spore anchor |
| `fg.closeshift`, `fg.farshift` | raid-wide | `1` shifted with the fixed player in the burst, `2` without him, `0` none |
| `fg.drag` | raid-wide | `1` while the holder walks him back |
| `fg.goo` | per bot | A dodge's branch: `stand`, `spread`, `tight`, `none`; `out` between dodges while goo is live |
| `fg.target` | per bot | `boss`, `skull`, `brake` |
| `fg.swap` | per tank | `1` for the bot elected to taunt him |
| `fg.bloatclear` | per paladin tank | `1` while it is due to bubble Gastric Bloat off |
| `fg.brake` | per tank | `1` while braked |
| `fg.defensive` | per tank | The ladder's pick: a cast name, `covered`, `none`, `idle` outside a window |
| `fg.humancall` | per sender | The button or taunt a human tank was told to press |
| `icc.cover`, `icc.personal` | per bot | Pungent Blight's raid cover and personal picks |

`tools/botobs/bosses/festergut.py`: `--phases`, `--stands`, `--spores` (each window's carriers, where each
was sent, how many stacked close and far, the shifts), `--goo` (impacts, dodge branches, leg flips),
`--targets`, `--tanks` (his target runs, elections and their taunts, bubbles, brake time, drags, peak
Gastric Bloat), `--defensives` (tank windows with the buttons cast, calls to a human tank, each Pungent
Blight's picks), `--cheats`. Its banner names declared keys the pull never wrote, sparing `fg.goo` on
normal and `fg.bloatclear` without a paladin tank.

Next pull, 25N: three Inoculated on everyone at each Pungent Blight, every swap at 9 with no explosion, no
tank braked into a rip, no second player hit by a Vile Gas, a tank button in each Inhaled Blight window,
every drag ending inside 2.5 yd. 25H: no player hit twice by one goo, no goo on a stand held, the far stack
flipped before a goo lands, every shifted group Inoculated at its burst.
