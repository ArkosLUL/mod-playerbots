# Lord Jaraxxus

Raid-wide facts and the code layout: [README.md](README.md). Mechanics source:
`boss_lord_jaraxxus.cpp`; ids are 10N/25N/10H/25H, all DBC-only rows.

## Pull and wipe

- **Intro** (stage 2): he is summoned at `LOC_CENTER` (`ARENA_CENTER`), walks 10 yd south and stays
  `UNIT_FLAG_NON_ATTACKABLE` and passive for the ~50 s scene. The release (stage 3) clears the flag
  and attacks `SelectNearestTarget(200)`, so whoever stands nearest takes the first swings.
- `JustEngagedWith` sets `TYPE_JARAXXUS` `IN_PROGRESS`: the only ToC boss that raises
  `IsEncounterInProgress` on engage. `MoveInLineOfSight` is empty, so he never aggros by proximity.
- **Wipe**: evade despawns his summons, sets `NON_ATTACKABLE` and `TYPE_FAILED`; the cleanup respawns
  him at the centre, attackable and aggressive, wearing the cosmetic chains 67924 until pulled. The
  stage stays 3, so the gate stays open: the main tank's hold waits for combat, and the leader pulls
  the retry.
- **Kill**: `summons.DespawnAll` takes the portals and volcanoes, not the Mistresses and Infernals
  they summoned, which live on into stage 4 (heroic Mistresses keep kissing). Multipliers that
  concern the adds gate on "open", not "live".
- No enrage: `SPELL_BERSERK` is never scheduled. Touch of Jaraxxus (25H) is commented out.

## Boss abilities

Timers from engage; events run only while he is not casting, so they slip.

| Ability | Ids | Timer | Effect |
|---|---|---|---|
| Fel Fireball | 66532/66963/66964/66965 | 5 s, then 10-15 s | on his victim; cast 2.5/2.0/2.5/2.0 s; 15.1k/24.4k/18.0k/34.1k fire plus a Magic DoT 5.8k/9.3k/7.8k/10.7k a second for 5 s |
| Fel Lightning | 66528/67029/67030/67031 | 10-15 s, repeat | instant, random player, chains to 3/5/3/5 targets, 10 yd jumps; 7.9k/9.7k/9.7k/11.7k |
| Incinerate Flesh | 66237/67049/67050/67051 | 24-26 s, then 20-25 s | random player; undispellable heal absorb 30k/60k/40k/85k for 15/15/12/12 s |
| Burning Inferno | 66242/67059/67060/67061 | Incinerate expiring unhealed | every player, 2.4k/2.4k/3.9k/7.8k a second for 5 s |
| Nether Power | 66228/67106/67107/67108 | 25-45 s, repeat; delays every event 5 s | full 5/10/5/10 stacks on himself, 30 s, +20% spell damage per stack, Magic |
| Legion Flame | 66197/68123/68124/68125 | 30 s, repeat | random player; 2 s later 66199/68126/68127/68128 for 6 s |
| Nether Portal | 66269/67898/67899/67900 | 20 s | 15 yd to his left; schedules the Volcano 60 s later |
| Infernal Eruption | 66258/67901/67902/67903 | Portal + 60 s | 15 yd to his left; schedules the Portal 60 s later |

Random picks are `SelectTarget(Random, 0, 0, playerOnly, withTank = true)`: **tanks are eligible**
for Legion Flame, Incinerate Flesh and Fel Lightning. The guide's Portal/Volcano times (60 s, 120 s,
…) are not this core's.

**Interrupts.** His immunity set (`creature_immunities` −286) holds SILENCE and STUN but not
INTERRUPT: Kick, Pummel, Shield Bash, Counterspell, Wind Shear, Mind Freeze and a Felhunter's Spell
Lock land, Silencing Shot and Silence do not. The Fel Fireball DoT is Magic, so the generic party
dispels strip it.

**Nether Power.** His `SpellHit` resets the stacks to full on each cast. A Spellsteal (30449)
strips one: this core marks Nether Power unstealable (SpellInfoCorrections), so only the script's
`SpellHit` removes a stack, casting Nether Power on the mage. Purge and Dispel Magic remove a stack
per effect dispelled.

**Legion Flame.** 66199 burns its carrier 2.9k/4.4k/3.9k/6.3k a second and summons a Legion Flame
(34784) at its feet every second. Each flame lasts 60 s, is `NOT_SELECTABLE`, and ticks 66877/67070/
67071/67072 every second on everyone within 3 yd of it, centre to centre (creature caster):
2.9k/4.4k/4.9k/7.3k. A flame's first tick lands a second after it spawns, so a carrier that never
stops takes none. `AvoidAoeAction` sees the flames (a non-selectable unit with a periodic damage
trigger) out to 5.5 yd centre to centre (`GetDistance` takes off both combat reaches, 1.0 and 1.5),
and flees by `FleePosition`; `jaraxxus avoid aoe guard` vetoes it while any flame is up.

## Adds

| Unit | Entry | Normal | Heroic |
|---|---|---|---|
| Nether Portal | 34825 | not selectable, lasts 15 s, one Mistress at 8 s | selectable (health ×15/×64), permanent, a Mistress every **6 s** |
| Infernal Volcano | 34813 | not selectable, lasts 18 s (SpellInfoCorrections), an Infernal every 5 s (three) | selectable, permanent, an Infernal every 5 s |

Test `UNIT_FLAG_NOT_SELECTABLE`, not the difficulty. A Mistress spawning hits everyone within 10 yd
of the portal; an Infernal lands at a random point within 15 yd of the volcano and hits 10 yd around
it.

**Mistress of Pain** (34826, health ×25/×70/×25/×70; immune to STUN and INTERRUPT): Shivan Slash on
her victim; Spinning Pain Spike, a jump to a random player within 140 yd. Heroic adds **Mistress'
Kiss**: a 1.5 s cast every 25-35 s after 10-15 s onto one random mana user, landing 66334/67905/
67906/67907 for 15 s. Every 0.5 s, if the target has `UNIT_STATE_CASTING` — a cast-time spell or a
channel, never an instant — it takes 66359/67073/67074/67075 (8.3k, 13.6k on 25H) plus an 8 s school
lockout, and the kiss is spent. A bot mid-cast runs no triggers (`UpdateAIInternal` yields while a
spell prepares), so only `RequestSpellInterrupt` from outside reaches it.

**Felflame Infernal** (34815, health ×6/×16/×7/×23; stunnable on normal only, immune to
INTERRUPT): melee. Every 30 s after 7-20 s, Fel Streak: `DoResetThreatList`, 50,000 threat on a
random player within 44 yd, tanks included, a charge there with a 15 yd impact, then Fel Inferno
pulsing 13-15 yd around it for 6 s. A taunt answers the reset by copying the top threat.

## Tactics

The Warcraft Tavern guide, reconciled with the script:

- Boss held at `ARENA_CENTER`; the main tank stands on him through the intro, since the release
  hits the nearest player.
- **Kill order**, everyone but tanks and healers, oldest (lowest guid) first: heroic Portal, Volcano,
  Mistress, Infernal; normal Mistress, then Infernal. Portal and Volcano stop the spawns.
- **Tanks**: main tank on the boss; assist tank 0 on Mistresses, assist tank 1 on Infernals, taunting
  its add back off anyone else, tanks included; a lone assist tank takes Mistresses first. The boss
  is not walked to the portal for cleave: it spawns 15 yd from him anyway.
- **Fel Fireball**: one interrupter per cast, the lowest guid among bots with an interrupt ready,
  affordable, in range and not mid-cast.
- **Nether Power**: mages Spellsteal, non-healer shamans and priests Purge and Dispel Magic; healers
  only when no other remover lives and no Incinerate Flesh is up.
- **Incinerate Flesh**: every healer in range heals the target, instants, HoTs and channels first,
  and nothing with a cast time while its health reads full: `PlayerbotAI::UpdateAI` cancels a heal
  still casting on a full-health target.
- **Legion Flame**: the carrier keeps moving for its 8 s, away from the boss and the raid; everyone
  else steps clear of the flames. A tank carrier takes the boss with it.
- **Mistress' Kiss**: a kissed bot casts instants only; a cast already running is broken from outside.
- **Lust** at the pull (default burst row): no enrage, and the heroic Portal at 20 s falls inside it.

## Node ladder

No ties; names drop their `jaraxxus` prefix. Two contests decide the order: the EMERGENCY band
beats every node, and Nether Power beats focus. Tank rows never share a bot (the main tank is never
an assist tank), so their order is arbitrary.

| Trigger → action | Relevance | Why here |
|---|---|---|
| `fel fireball interruptible` → `interrupt fel fireball` | `ACTION_EMERGENCY + 9` | A 2.0-2.5 s window on one bot; the kick moves nobody, so the dodge follows next tick |
| `legion flame nearby` → `avoid legion flame` | `+8` | Beats every hold, so a carrying tank walks and he follows; breaks the bot's own cast first. The only flame mover: `AvoidAoeAction` is vetoed ([Legion Flame](#boss-abilities)) |
| `pinned cast` → `break pinned cast` | `+7` | Breaks a kissed or carrying bot's cast from another bot's tick ([Mistress of Pain](#adds)). Returns `false`: costs nothing, and no RAID node returning `true` can skip it |
| `intro main tank` → `intro main tank stand` | `ACTION_RAID + 6` | Fires only while he is `NON_ATTACKABLE`, never beside the hold; `jaraxxus intro hold` zeroes every other movement on the main tank, `follow` included |
| `engaged by main tank` → `main tank hold boss` | `+5` | Only while he is in combat ([Pull and wipe](#pull-and-wipe)). The drag to `ARENA_CENTER` tests only its next 5 yd step, since a tank-carried trail lies along the whole way back; a flame there sends the tank around the trail |
| `add needs assist tank` → `assist tank hold add` | `+4` | Assist tank 0 |
| `second add needs assist tank` → `assist tank hold second add` | `+3` | Assist tank 1 |
| `nether power active` → `remove nether power` | `+2` | A remover is usually a focusing DPS too, and each stack is +20% spell damage for 30 s. Its own node, since class dispels sit under `ACTION_RAID` |
| `add should be focused` → `focus add` | `+1` | Needs an add alive and the reset none, so the two never meet |
| `focus stale` → `reset focus` | `ACTION_RAID` | Only writes `rti`, so returns `false` |
| `incinerate flesh on raid` → `heal incinerate target` | `ACTION_MEDIUM_HEAL + 5` | Above routine heals, below critical ones (30): the target's health reads full under the absorb, so no generic node picks it. Returns `false` with nothing castable, so routine heals still run |

## Traps

- **The arena floor is a gameobject** (195527, display 9059 `Coliseum_Intact_Floor.wmo`): no static
  navmesh or vmap height anywhere on it, so navprobe reads every floor point off-mesh and paths as a
  0x11 straight line. Live, its dynamic collision gives height, so `FindNearestPositionClearOfHazards`
  and `MoveTo` work there, but no floor point can be verified offline: nothing here fixes a
  coordinate other than `ARENA_CENTER`.

## Known gaps

- No Fel Lightning spread: a 3-5 target chain with 10 yd jumps.
- No raid cooldown calls: Aura Mastery with Concentration Aura against the Kiss, Divine Sacrifice or
  Fire Resistance Aura through a Volcano.
- Melee take Fel Inferno and heroic Portal and Volcano pulses; the guide answers with defensive
  cooldowns only.
- `--flame` judges a tank carrier's repeat window: `flame` is change-only, so a second `tank`
  window with no switch since the first holds no note. The fix seeds each window with the value
  latched at its start.

## What a trace answers

Position, verdicts and moves come from the raid-agnostic streams; `postmortem.py --vetoes` covers
the five multipliers. The Jaraxxus rows, under `--notes jaraxxus.`:

| Key | Shape | Says |
|---|---|---|
| `netherpower` | latch | His Nether Power stacks, the only record: his auras are never traced |
| `focus` | latch | The DPS kill-order add's guid, `0` for none |
| `addtank` | per assist tank | The add it holds, `0` for none |
| `interrupter` | per bot | `1` holding Fel Fireball duty, only inside that cast: rewritten every tick he lives, so it drops to `0` as the cast ends |
| `flame` | per bot | `carrier`, `relaxed` (nothing clear of raid and boss, so flames only), `tank` (a carrier holding him, flames only), `dodge`, `none`. Change-only: counts are switches, not legs |

`tools/botobs/bosses/lord_jaraxxus.py` reads the rest, counting every difficulty id of a row; its
banner names declared probes the pull never wrote.

- `--boss`: each Fel Fireball landed, kicked or neither, by whom, and who held `interrupter` inside
  it; Fel Lightning hits per cast against the chain cap, which decides whether a spread is worth
  building.
- `--nether`: the stack timeline, seconds at 1+ and at full; Spellsteal, Purge and Dispel Magic at
  him, those cast while stacked and those the count fell after; Nether Power landing on a mage.
- `--incinerate`: per window, the target, how it ended (healed, expired, died), heals landed on the
  target, Burning Inferno in the 6 s after.
- `--flame`: per carrier window (debuff and trail joined), yards walked, legs issued, nearest other
  member and the boss against the carrier's 8 and 15 yd (not judged when its switches are all
  `tank`), `flame` switches; flame-tick damage per bot, with the nearest sampled flame.
- `--adds`: per add, first and last sample and the share of samples on a non-tank; `focus` and
  `addtank` changes; each Mistress' Kiss, how it ended, its punish and `kiss cast hold` vetoes.
