# Valithria Dreamwalker

Boss id 10, trace slug `valithria-dreamwalker`, prefix `vd.` ([README](README.md)). Code: stem
`Valithria` (`src/Ai/Raid/ICC/*/ICC*_Valithria.*`). Mechanics: `boss_valithria_dreamwalker.cpp`.

## Mechanics

- **Win and loss.** She is friendly and starts at 50% health. Healed to 100% she wins (credit spell
  72706); at 0 the combat trigger sets `NOT_STARTED` and despawns the room 5 s later. `IN_PROGRESS`
  comes from that trigger's `JustEnteredCombat`, as the adds reach her. Heroic's 7 min berserk is a
  yell only.
- **Adds**, summoned by the Lich King controller on decaying periodic timers (floor 5 s), first at:
  Gluttonous Abomination 37886 5 s, Suppresser 37863 10 s, Blistering Zombie 37934 15 s, Risen
  Archmage 37868 20 s, Blazing Skeleton 36791 30 s. Acid Burst is row 2096 (70744/71733/72017/72018);
  the archmage drops Mana Void 38068 and Column of Frost 37918.
- **Portals** every 45-48 s, first at 45-48 s: 3 pre-effects on 10-man, 8 on 25-man, 20-30 yd from
  her, 38186 (heroic Nightmare 38429, through the DB-only row 72224 → 72480). Each shows its visual
  71304 (71986) for 15 s, then summons a real portal 37945 (38430) for 10 s and lingers 6 s itself
  with no visual. Portals are friendly, so no snapshot samples them.
- **Dream State** 70766, the portal's spell click: 20 s, phase 16, flight, out of reach of the adds and
  of everyone outside it. Dream Clouds 37985 (heroic Nightmare 38421) there burst on a player within
  5 yd: Emerald Vigor 70873, DB-only row to heroic Twisted Nightmares 71941, 40 s and stacking.

## Tactics

- `icc valithria group` (`ACTION_RAID + 1`), everyone: tanks taunt the nearest add no tank holds
  (abominations, rot worms) within 30 yd, else step toward it; others flee Mana Void (10 yd), Column
  of Frost (7 yd), a rot worm on them, any exploding zombie (20 yd), and non-tank melee keep 15 yd from
  zombies; non-healers but the main tank CC the nearest zombie. Anyone not in Dream State, claiming a
  portal, chased by a zombie or (as DPS) fighting an add near her keeps within 35 yd of
  `ICC_VDW_HEAL_POSITION`. It also places the marks.
- **Marks**: priority skeleton, suppresser, archmage, zombie, abomination, worm, within 45 yd of the
  anchor. 10-man: one skull. 25-man: at most one priority add up keeps one skull; otherwise skull
  and cross groups, balanced by role, each owning one half of the room split on the anchor's Y.
- `icc valithria zombie kite` (`ACTION_EMERGENCY + 9`), a non-tank a zombie targets: stop casting,
  Nitro Boosts, 18 yd steps on a 36-point arc kept within 25 yd of the anchor.
- **Healers** split by `IccValithriaShouldHealRaid`, a guid ranking of alive healers, humans
  included. With druids: the first druid by guid heals the raid, the first two past three healers;
  past three healers a lone druid is joined by the last-ranked healer. No druid: the last two past
  three healers, else the last one. The rest take portals (`icc valithria portal`, `ACTION_RAID + 5`:
  claim the nearest unclaimed pre-effect, click the real portal within 3 yd; never past 25 Twisted
  Nightmares) and heal her (`icc valithria heal`, `ACTION_RAID + 2`): druid Rejuvenation, Regrowth,
  Lifebloom to 3, Wild Growth; shaman Riptide, Healing Wave; priest Renew, Greater Heal; paladin
  Beacon of Light, Holy Light; each skipped when the bot does not know it.
- **Dream State** (`icc valithria dream cloud`, `ACTION_RAID + 4`; above 50% health the multiplier
  allows nothing else): with a human in it every bot stacks on the lowest-guid one, otherwise all
  share one cloud target, the nearest to their leader, held 1 s.
- Out of Dream State, a healer above z 367.961 is snapped to z 365 with `NearTeleportTo`, which keeps
  combat but skips fall damage (borderline, documented in the README).
- The tanks' stray-add taunt reaches one add per 8 s cooldown, with no fixate (README, "The class
  taunt").

## Waves

Valithria has no phase. `IccValithriaPortals` lists the portals for the portal trigger and the
healers' anchor move, and publishes `vd.wave`, keyed on the pre-effect's visual, since the pre-effect
lingers 6 s after its portal opens. A healer in Dream State is phased away from them, so
it writes none. Portal claims and the cloud target clear 5 s after the wipe's `NOT_STARTED`, with the
rest of the instance state; claims also drop with their portal, the target once no cloud is left.

## Cheats

Removed:

- `vd.nitro`, now the bot's own Nitro Boosts through `UseNitroBoosts`, after the kite's `CastStop`,
  since the boots refuse a casting bot.
- `vd.speed`, run, walk and flight at 2× in Dream State, with no replacement. Its every-tick reset to
  1.0 went too: it set the rate outright, cancelling Sprint and any other speed aura.

Kept, behind the raid cheat:

| Key | Does | A legit answer needs |
|---|---|---|
| `vd.dreamteleport` | Teleports onto the lowest-guid human in Dream State when over 1 yd off | A flight mover holding 1 yd on the leader |
| `vd.cloudteleport` | Teleports onto the shared cloud every tick | A flight mover reaching each cloud within 5 yd |

Both are same-map `TeleportTo`, so each also drops combat. With the raid cheat off, a bot in Dream
State hovers.

## Known gaps

- Marks by raid icon (README); `vd.skull` and `vd.cross` record every move.
- Humans count in the healer ranking, so a human can hold a raid-heal slot no bot then covers.
- A selfbot gets no rank, so its portal and heal nodes stay off: the ranking skips every member when
  the asking bot is one.
- No ICC trace yet carries these probes, so nothing here is measured.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `vd.wave` | per healer | `pre` while pre-effects show their visual, `open` while a real portal stands, `none` |
| `vd.portalclaim` | traced map, healer → portal | The claimed pre-effect or portal; `0` once it lapses |
| `vd.raidheal` | per healer | `raid`, `boss` (portals and her), `none` when unranked |
| `vd.cloud` | the bot that moves the shared target | `cloud` and the unit, `none` with no cloud left; `human` and the unit is each bot's own pick |
| `vd.skull`, `vd.cross` | the bot that moved the icon | Tier, ` other` for an add off the other group's half, and the unit; or `clear` |
| `vd.group` | per bot, 25-man | `skull`, `cross`, `single` (one priority add or none), `none` |

Dream State itself is the `aura` stream (70766), so no probe repeats it. Change-only per bot, so a bot
repeating its own last value writes nothing.

`tools/botobs/bosses/valithria_dreamwalker.py`; its banner names declared keys the pull never wrote,
sparing the 25-man ones on 10-man:

- `--waves`: per wave, pre and open times, the claiming healers and who entered Dream State;
- `--dream`: each Dream State window and the `vd.cloud` counts;
- `--heal`: `vd.raidheal` per healer;
- `--marks`: every skull and cross move, and `vd.group` per bot;
- `--cheats`: this encounter's keys.
