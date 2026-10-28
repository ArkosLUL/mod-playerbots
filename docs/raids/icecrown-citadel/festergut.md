# Festergut

Boss id 4, slug `festergut`, prefix `fg.`. Code: stem `Festergut` (`src/Ai/Raid/ICC/*/ICC*_Festergut.*`);
nodes `icc festergut` goo, pungent defensive, pungent cover, tank defensive, bloat swap, brake, target,
stand; multiplier `icc festergut brake`. Raid conventions: [README.md](README.md). Core:
`boss_festergut.cpp`.

## Mechanics

Ids are 10N/25N/10H/25H where they differ; the Spell constructor remaps a cast, so code matches the base
id through `GetSpellIdForDifficulty`. Inhaled Blight's heroic columns are empty and fall back to the
normal id of the same size. Gas Spore's aura 69279 has no row. His combat reach is 8, so a player's melee
range is 10.83 yd from his centre. Every area spell here comes from a creature or reaches allies, so its
radius is exact, centre to centre.

| Ability | Script | Guide | Bots |
|---|---|---|---|
| Gaseous Blight 69157/69162/69164 | Putricide fills the room from his balcony at the pull: shadow every 2 s, three levels, each Inhale Blight a level less | Heal through | Generic heals; shadow resist hook for the merge stage |
| Inhale Blight 69165, Inhaled Blight 69166 (10), 71912 (25) | 3.5 s cast every 34 s from 25-30 s; each stack +25% (10) or +30% (25) damage and attack speed, three at most | Tank cooldowns: minor at 2 stacks, major at 3 | Holder spends a button at 2 once a cycle, keeps one up from 3 (`fg.defensive`) |
| Pungent Blight 69195/71219/73031/73032 | The fourth inhale event: 3 s cast, 42.9-45.1k / 48.8-51.3k / 63.4-66.6k / 73.1-76.9k shadow to everyone; takes his stacks and everyone's Inoculated; next Gas Spore 20-25 s later | Three Inoculated; Aura Mastery with Shadow Resistance Aura, shields; immunities | `GetRaidCover` and `GetRaidPersonalDefensive` on the cast, the hit cut 25% per Inoculated stack |
| Gas Spore 69278 (10), 71221 (25) → 69279 | 2 (10) or 3 (25) random players, first at 20-25 s, every 40-45 s; bursts at 12 s: Blighted Spores 69290/71222/73033/73034 on everyone within 8 yd of the carrier (1.95-2.05k / 2.9-3.1k / 2.9-3.1k / 3.9-4.1k shadow a second for 6 s); Inoculated 69291/72101/72102/72103 as it ends, 120 s, −25% shadow area damage a stack, three at most, one per 10 s | Groups stack on a carrier: WT10 melee with tanks and ranged; WT25 tanks, melee, ranged; a spare carrier moves to an empty group | Two groups, a deal per wave (`fg.carrier`, `fg.stand`) |
| Vile Gas 69240/71218/73019/73020 | From 30-40 s, every 28-35 s, held to 20 s after every spore. On a random player past his melee by 5 yd (14.5 yd from his centre) once 3 (10) or 8 (25) stand there, else anyone: everyone within 8 yd of it, then each victim within 8 yd of its allies, 3.9-4.1k / 4.9-5.1k / 4.9-5.1k / 6.3-6.7k every 2 s for 6 s and disoriented | Ranged and healers 8 yd apart | 18 slots, 10.4 yd apart, out past 14.5 yd; melee fill when too few are out |
| Gastric Bloat 72219/72551/72552/72553 | On his victim from 12.5-15 s, every 15-17.5 s: 9.8-10.3k / 12.2-12.8k / 12.2-12.8k / 14.6-15.4k nature, +10% damage done a stack, 100 s; at 10 Gastric Explosion 72227/72228 (10 yd, 29.3-30.8k) or 72229/72230 (everyone, 48.8-51.3k) | Swap at 9 once the other tank's has dropped; the relieved tank stops attacking a while | Elected swap at 9 (`fg.swap`); threat brake (`fg.brake`) |
| Malleable Goo (heroic) 72296 → 72297/72548/72549/72550 | Putricide, from his balcony (4324.8, 3166.0, 389.4), at a random player but his victim, from 15-20 s, every 15-20 s; lands where it stood at the throw after the 12 yd/s flight (46-97 yd to a stand, 3.9-8.1 s): 5 yd, 14.6-15.4k nature (72549/72550), 20 s swing and cast slow | Ranged dodge the circle; melee switch legs | Forced dodge, leg and far-anchor flips (`fg.goo`, `fg.leg`, `fg.sporespot`) |
| Berserk | 5 min | | |

His boss state moves to `IN_PROGRESS` only once Putricide reaches his balcony point, about 7 s in, so
the trace opens on the engage and every bot rule reads his combat, not the state. He obeys taunt
diminishing returns (`flags_extra` 0x80000 on all four entries). His boundary is x 4205-4325, y
3082-3195.

## Duties

- **Tanks**: one tank spot each, 6.5 yd from his spawn at 40° either side of west and 8.4 yd apart, so
  he turns to the holder without moving and goo on the free tank misses the holder; a third tank takes the
  legs. Generic taunts are blocked while he fights. Once the holder carries 9 Gastric Bloat, or a non-tank
  holds him, the bot tank with the fewest stacks, at most 7, taunt ready in its form within 30 yd, taunts
  him, one on a tank spot before a third tank. A tank not holding him stops attacking him at 90% of the
  holder's threat, its pickers and its area damage near him held, until under 80%. A human partner is
  called to taunt when no bot can.
- **Tank defensives** (`fg.defensive`, physical only): the holder spends one button at 2 Inhaled Blight
  once a cycle (a cycle ends at each Pungent Blight), and one whenever none runs from 3; any tank under
  40%. Class nodes stay held until 25%. A human holder gets the same windows as calls (`fg.humancall`).
- **Melee**: both stand on one leg, 7 yd out south-east or north-east, behind him whichever tank he faces;
  `fg.leg` flips to the other once goo lands within 8.25 yd of the one in use and the other is clear. The
  legs release once he stands 3.5 yd off his spawn.
- **Ranged and healers**: one latched slot each (`fg.slot`), released once he is 10 yd off his spawn.
  Healers take 1, 2, 3, 8, 9, 10 first; from there both tank spots and all but two or three slots are in
  40 yd. While fewer than 3 (10) or 8 (25) bots hold a slot, melee fill the free ones. Reach and spell
  movers are held on a stand, heal reach is not.
- **Spores**: per wave, the tanks and melee are the close group, stacking on the anchor between the tank
  spots, the ranged and healers the far group, on slot 1 east of him or 5 north (`fg.sporespot`, flipped
  like the legs once goo lands within 10.75 yd). A group with no spore takes the one nearest it off a group
  with two (a melee for the far group, never the holder); a group still without one stacks with the other.
  A bot carrier stands on its anchor, everyone else on a 2.5 yd ring round it; a group carried only by the
  holder rings round him, one carried only by players round the first of them. A swap inside the wave
  deals again. The holder never moves.
- **Malleable Goo**: a bot other than the holder within 6.5 yd of a live impact interrupts its cast and
  walks, forced, to its own stand if that is 7.5 yd clear, else to the nearest clear spot, a ranged one
  8.5 yd from everyone outside a spore window. A stand under goo waits until the impact ages out.
- **Pungent Blight**: during the cast the raid cover is dealt on the lowest Inoculated stack on the map,
  its class nodes held over the lead-in; each bot asks for its own button on its own stacks, so a bot
  short of Inoculated takes an immunity or a healthstone.
- **Targets** (`fg.target`): a player's skull, else him; a braked tank nothing.

Stands are in `ICCHelpers_Festergut.h` round his spawn (4267.92, 3137.26): slots 1-7 at 18 yd and 8-18 at
28.5 yd, behind and beside him. Every one is navprobe-clean (`--nav 0x09`, settled z 360.385), the floor
flat out to 45 yd; `los` from each and its 3x3 neighbourhood at 1 yd to him and both tank spots is clear
(657 rays); the walks between them are straight normal paths. Gameobject collision isn't in navprobe's
sight.

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
| Heal reach kept on slots | the spread is 55 yd wide; slots 1-3 and 8-10 reach most of it | Held, as Saurfang does |
| Lust free | neither guide times it; a 5 min DPS race | Lust at 3 Inhaled Blight |

## Cheats

None before, none now.

## Known gaps

- If he settles more than 3.5 yd off his spawn, melee fall to the generic movers and the legs stop; nobody
  walks him back.
- A goo on the close stack during a spore window can cost the tanks a stack: the anchor sits between them,
  and its dodgers wait outside until the impact ages out. WT25's separate tank group would avoid it.
- The healers walk to heal the far slots and back.
- A third tank takes him only when neither spot tank can; it then holds him from the legs, turning him onto
  the melee and standing 11 yd from the close spore stack.
- Melee fill counts bots only. Humans hold no duty.
- The shadow resist hook (`IccFestergutResistSchools`) waits for its `HookRows()` row, so Aura Mastery is
  never dealt for Pungent Blight until then. No opener: hooks answer only once he is live.
- No 631 trace from this build yet, and never built.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `fg.phase` | per bot | `spore` while any group member carries Gas Spore, else `normal` |
| `fg.slot` | per bot | 1-18 a slot, 19 the legs, 20-21 a tank spot; `0` released |
| `fg.stand` | per bot | The stand held now: `slot`, `leg`, `tank`, `close`, `far`, `none` |
| `fg.carrier` | carrier → group | 1 close, 2 far; `0` once the wave ends |
| `fg.leg`, `fg.sporespot` | raid-wide | The melee leg, the far spore anchor |
| `fg.goo` | per bot | A dodge's branch: `stand`, `spread`, `tight`, `none`; `out` between dodges while goo is live |
| `fg.target` | per bot | `boss`, `skull`, `brake` |
| `fg.swap` | per tank | `1` for the bot elected to taunt him |
| `fg.brake` | per tank | `1` while braked |
| `fg.defensive` | per tank | The ladder's pick: a cast name, `covered`, `none`, `idle` outside a window |
| `fg.humancall` | per sender | The button or taunt a human tank was told to press |
| `icc.cover`, `icc.personal` | per bot | Pungent Blight's raid cover and personal picks |

`tools/botobs/bosses/festergut.py`: `--phases`, `--stands`, `--spores` (each window's carriers, where each
was sent, how many stacked close and far), `--goo` (impacts, dodge branches, leg flips), `--targets`,
`--tanks` (his target runs, elections and their taunts, brake time, peak Gastric Bloat), `--defensives`
(tank windows with the buttons cast, calls to a human tank, each Pungent Blight's picks), `--cheats`. Its
banner names declared keys the pull never wrote, sparing `fg.goo` on normal.

Next pull, 25N: three Inoculated on everyone at each Pungent Blight, every swap at 9 with no explosion, no
tank braked into a rip, no second player hit by a Vile Gas, a tank button in each Inhaled Blight window.
25H: no player hit twice by one goo, no goo on a stand held, the far stack flipped before a goo lands.
