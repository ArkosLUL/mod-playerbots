# Professor Putricide

Boss id 6, trace slug `professor-putricide`, prefix `pp.` ([README](README.md)). Code: stem
`Putricide` (`src/Ai/Raid/ICC/*/ICC*_Putricide.*`). Mechanics: `boss_professor_putricide.cpp`.

## Mechanics

- **Phases** come from `DamageTaken`: phase 2 once his health is no longer above 80%, phase 3 at 35%
  and under. Each transition delays his abilities 24 s (49 s heroic). Normal: Tear Gas stuns the raid
  while he drinks at the table. Heroic: two Unstable Experiments at once, Ooze and Gas Variable split
  across the 25-man raid, and the drink 25 s after he reaches the table.
- **Unstable Experiment** every 35-40 s, first at 30-35 s, cancelled in phase 3: it alternates a
  Volatile Ooze 37697 and a Gas Cloud 37562 from the two sides of the room. Both live until killed, so
  two of one entry are alive only when the last of that kind outlived the next experiment of its kind,
  70-80 s on.
- **Volatile Ooze** puts Adhesive (row 2241) on a random player and erupts on reaching them. **Gas
  Cloud** puts 10 stacks of Gaseous Bloat (row 2176) on a random
  player and chases them; Expunged Gas on contact. Both are taunt-immune (set -362).
- **Slime Puddle**: a Growing Ooze Puddle 37690 under 2 random players every 35 s, first at 10 s,
  growing by Grow 70347 stacks.
- **Phase 2 on**: Malleable Goo 70852 every 25-30 s on 1 player (25-man 2, 25H 3), never his last
  victim nor anyone within 7 yd of his reach; Choking Gas Bomb every 35-40 s. **Phase 3**: Mutated Plague (row 2183) on
  his victim every 10 s.
- **Unbound Plague**, heroic (row 2244): from 20 s, every 90 s, on a random player within 45 yd who is
  not his victim; damage ×1.25 a tick. Once its searcher has ticked twice it jumps to a random player
  in range, leaving Plague Sickness on the old carrier.
- **Mutated Abomination** 37672 (25-man 38285), from the drink GO 201584: refused in phase 3 and while
  one exists, despawned when phase 3 resumes. Energy starts at 0 with no regen; Eat Ooze on a puddle
  refills it, Regurgitated Ooze 70539 (row 2178) costs 45.

## Phase

`IccPutricidePhase(bot, boss)` derives 1-3 from the same thresholds and publishes `pp.phase`, rather
than read his `_phase` (`GetData(DATA_PHASE)`), which flips only at `ChangePhase`, a tick or a cast
later. Every phase test calls it, the abomination, goo, Unbound Plague, puddle and tank-spot ones, so
all take phase 3 at the core's 35% and under.

## Tactics

- The main tank holds him at `ICC_PUTRICIDE_TANK_POSITION`, and turns him off any Growing Ooze Puddle
  within 8 yd of him, out of phase 3. Tanks swap on Mutated Plague: the one with more stacks stops,
  the one with fewer taunts.
- **Volatile Ooze**: skull on it; melee attack it, ranged and healers stack on the Adhesive target
  within 20 yd.
- **Gas Cloud**: the bloated bot flees, never doubling back on the direction it committed this bloat;
  everyone else keeps 15 yd from the cloud. Skull on the cloud while no ooze lives.
- **Goo**, before phase 3: a bot within 8 yd of an impact younger than 6 s flees to the nearest clear
  spot on rings 8-17 yd out.
- **Phase 3**: everyone stacks on the main tank; hunters may stay within 12 yd in his front cone.
- **Unbound Plague**: the carrier runs 20 yd clear of everyone.
- The assist tank drives the abomination: Regurgitated Ooze on the ooze, then the cloud, then eats
  puddles, then Mutated Slash on ooze, cloud or boss.

## Cheats

Removed: `pp.nitro`, now the bot's own Nitro Boosts through `UseNitroBoosts` on a bloat flee.

Kept, behind the raid cheat:

| Key | Does | A legit answer needs |
|---|---|---|
| `pp.oozekill`, `pp.gascloudkill` | Kills every Volatile Ooze, or Gas Cloud, past one of its entry | A kill order across two adds of one kind, which exist only when the raid failed to kill one in time |
| `pp.unboundplague` | The carrier kills itself once nobody is within 20 yd | Passing it: walking it into a chosen player after two ticks. The bots do the opposite and run from everyone |
| `pp.freeooze` | Regurgitated Ooze at no energy cost; off, it casts at the full 45 | A duty cycle that eats puddles up to 45 energy before each cast, not a site swap |

Record-only, `EnableICCBuffs`: `pp.heroicbuff` and `pp.spitefulfury` on every member on heroic
(README).

## Known gaps

- Skull by raid icon (README); `pp.skull` records every move.
- With the raid cheat off, a Regurgitated Ooze without the energy still stamps its 1 s cooldown and
  claims the tick.
- No ICC trace yet carries these probes, so no figure here is measured.

## What a trace answers

| Key | Shape | Says |
|---|---|---|
| `pp.phase` | per bot | `1`, `2`, `3` from his health |
| `pp.bloatflee` | per bloated bot | The branch that produced the flee leg: `corner`, `wedge` (most open spot, nothing passing), `open` (scored search), `loose` (its relaxed pass), `straight`; `none` once the bloat is gone. Change-only, so counts are switches |
| `pp.skull` | per bot | `ooze`, `cloud` or `boss` with the unit, or `clear`, only from the bot that moved the icon; a repeat of its own last placement writes nothing |

`tools/botobs/bosses/professor_putricide.py`; its banner names declared keys the pull never wrote:

- `--phases`: the `pp.phase` steps;
- `--bloat`: per Gaseous Bloat, victim, length, death inside it, and the flee branches noted;
- `--skull`: the skull's placements merged across the bots that made them;
- `--cheats`: this encounter's keys plus the raid-wide `icc.` ones.
