# Faction Champions

Raid-wide facts and the code layout: [README.md](README.md). Mechanics: `boss_faction_champions.cpp`;
lineup, release and kill credit: `instance_trial_of_the_crusader.cpp`.

## Lineup

The raid fights the other faction's champions: `ToCFactionChampions` in
`Util/ToCHelpers_FactionChampions.h`, 14 per faction, same entries on every difficulty.
`EVENT_SUMMON_CHAMPIONS` rolls a new lineup each pull:

- **Healers**: Resto Druid, Holy Paladin, Disc Priest, Resto Shaman, minus 1 at random (25-man) or 2
  (10-man). Each removed healer adds its class's DPS spec (Balance, Retribution, Shadow,
  Enhancement), so no class brings both.
- **DPS**: Death Knight, Hunter, Mage, Rogue, Warlock, Warrior plus those hybrids; 10-man drops 4 of
  them at random.
- 25-man: 3 healers + 7 DPS; 10-man: 2 + 4. The warlock and hunter each summon a pet (Felhunter
  35465, Cat 35610): no Aegis, no kill credit.

They jump in `UNIT_FLAG_NON_ATTACKABLE` and `REACT_PASSIVE`; `EVENT_CHAMPIONS_ATTACK` releases them
4 s later and sets `IsEncounterInProgress`. The first to engage calls `SetInCombatWithZone` and starts
the others on their nearest player. The last champion's death gives kill credit and stage 6. A wipe
(`TYPE_FAILED` → `InstanceCleanup`) despawns them at stage 4; the next pull brings new guids and a new
lineup.

## Mechanics every choice rests on

- **Threat is assigned, never earned.** On engage, 2 s later, then every 8.75-9.25 s,
  `RecalculateThreat` resets each player to 10,000,000 × a modifier: distance (melee past 5 yd, ranged
  past 35 yd), health (2.0 above 40,000, rising to 6.0 near 0) and, for melee and pets, armour. So
  melee champions go for close, low-health, low-armour players and casters for low-health ones; no
  tank holds a champion, a taunt lasts until the next reset, and a threat redirect does nothing.
- **Taunt**: normal templates obey it with diminishing returns; heroic ones (`CreatureImmunitiesId`
  -13) are immune to taunt and Attack Me. Both are immune to charm.
- **Champion's Aegis** 68595, on every champion via `creature_template_addon`: −75% AoE damage, all
  schools (aura 229), flat rather than stacking. The guide's advice follows: single-target only.
- **Diminishing returns as on players** (`CREATURE_FLAG_EXTRA_ALL_DIMINISH`): every CC category
  diminishes 10 s → 5 → 2.5 → immune, under the PvP duration cap, unlike most raid adds
  ([raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)).
- **Heroic PvP trinket** 65547, 2 min cooldown: breaks fear, stun, confuse or freeze (Polymorph and
  Cyclone included) the moment the champion's AI next updates. Roots are not in `IsCCed`, so a rooted
  champion keeps casting.
- **Their AoE needs a crowd**: Psychic Scream, Intimidating Shout, Hellfire, Arcane Explosion, Frost
  Nova, Bladestorm, Fan of Knives and Divine Storm fire only with 3+ of the raid within 5-10 yd of the
  caster (`EnemiesInRange`), so spreading denies them.
- **Never out of resources**: mana users regain a third of their pool every 4 s, energy users every
  1 s.
- **Boss-flagged** (`type_flags` 12 carries `CREATURE_TYPE_FLAG_BOSS_MOB`), so the burst gate treats
  every champion as a boss.
- **Helpful casts go to the most-injured friendly**: heals, Cleanse, Dispel Magic, Power Word: Shield
  and Hand of Protection all take `SelectTarget_MostHPLostFriendlyMissingBuff` (most health lost, in
  combat, within 40 yd, self included). In practice they land on the raid's kill target.

## What the offence plays against

| Champion | Abilities |
|---|---|
| Holy Paladin | Flash of Light 1.3 s, Holy Light 2 s, Holy Shock, Cleanse; Hand of Protection (10 s physical immunity), 5 min; Divine Shield below 25% (12 s, 5 min) |
| Disc Priest | Flash Heal 1.5 s, Renew, Shield; Mana Burn 2 s; Psychic Scream below 50% with 3+ of the raid within 8 yd |
| Resto Shaman | Lesser Healing Wave 1.5 s, Riptide, Earth Shield; Hex; Heroism or Bloodlust 25-40 s in, as does the Enhancement Shaman |
| Resto Druid | Nourish 1.5 s, Regrowth 2 s, Rejuvenation, Lifebloom; Tranquility channel every 2-3 min |
| Mage | Ice Block below 25% (5 s, 5 min) |
| Warlock | Fear 65809 on a random player within 20 yd every 10-15 s |
| Shadow Priest | Psychic Scream 65543 with 3+ of the raid within 8 yd, every 30 s |
| Warrior | Intimidating Shout 65930 with 3+ of the raid within 8 yd, every 2 min |

The three fears, Divine Shield 66010, Ice Block 65802 and every CC the champions cast have one id on
every difficulty. 53 of the script's 121 ids remap, heals included, all in the client DBC only.

**Tactics** (Warcraft Tavern, 25-man): kill healers first or the most dangerous DPS first, the
second by the guide's danger table — Rogue and Warrior S, Hunter A, Enhancement, Death Knight and
Retribution B, Warlock, Shadow Priest and Mage C, Balance D. Among healers the Holy Paladin heals
most and his Cleanse strips the raid's CC; the Disc Priest's instants and dispels make her the next
to kill or chain-CC; the Resto Shaman's cast heals are interruptible but killing him is faster; the
Resto Druid, with no dispel, is least threatening. Mark every target, assign CC, and CC dangerous
champions that are free. Lust and every DPS cooldown at the pull: each early kill makes the rest
easier.

## What the code decides

| Topic | Decision | Why |
|---|---|---|
| Kill order | Holy Paladin, Disc Priest, Resto Shaman, Resto Druid, then the danger table in the order above. The CC order too. Pets are never picked or CC'd | The guide's healers-first school: champion heals go to the most-injured friendly, the kill target. Pets carry no Aegis and no kill credit |
| Kill latch | `FactionChampionsKillTarget`, one grid scan per instance so every bot agrees, keeps its target while attackable. On its death the best attackable champion takes over, else the best immune one. Turned immune to physical and magic damage both (Divine Shield, Ice Block, Cyclone), it is suspended for the best attackable other, if any (a newer suspension replaces it), and returns once attackable and no healthier than the current | A per-bot, per-tick lowest-health pick splits the raid. Divine Shield and Ice Block fire below 25%. Each half is asked apart: `IsImmunedToDamage` wants one aura covering the whole mask, and Ice Block 65802 is a physical aura plus a magic one |
| Marks | `faction champions mark targets`, on the mechanic tracker of any role: skull on the kill target (a switch clears the old one); clears a CC icon from a champion not assigned it, and once not live only icons still where this encounter put them. CC bots place their own icons | `IsMechanicTrackerBot` elects any role, so a marker gated to non-healers goes silent under a healer tracker. A wipe despawns the champions, so between pulls a stray can't be told from a human's pre-pull mark |
| Focus | Non-healers attack the kill target (`faction champions focus priority`), `DpsAssist`/`TankAssist` vetoed meanwhile. Tanks join; no taunt peel | `TankTargetValue` leaves the skull while a tank holds it. Taunt can't keep a champion (above), and the guide has no tank role |
| CC | Each CC bot (non-healer mage, warlock or druid running `cc`) gets its own champion, in kill order skipping the kill target and a suspended one, kept while both stay valid, and its own icon (moon, square, triangle, diamond, circle, star, cross) through `SetRtiCcTarget`. Its prior `rti cc` comes back on release, or once not live through `toc restore rti cc`. The next kill target stays CC'd until its turn; a Cyclone on it at the switch is an ordinary suspension | Champions diminish like players, so CC bots piling on one moon burn its DR. CC on the suspended one breaks or blocks the latch's return. `rti cc` is saved to the bot's DB store and outlives the pull; the restore node has no encounter prefix, so it still runs after the kill closes the gate, for a bot dead then |
| Interrupts | One mage, first by guid within 30 yd and neither CC'd nor silenced, counterspells the kill target's cast-time or channelled helpful spell unless it is immune (`faction champions counterspell kill target`, which re-checks the duty when it pops) | No mage node interrupts its own target: there is no current-target counterspell, and every `… on enemy healer` node skips the bot's target. Rogue, shadow priest, warrior, paladin, warlock pet, DK and shaman already cover the kill target, their healer nodes the others |
| AoE | Suppressed | Champion's Aegis. The guide exempts Death and Decay and Pestilence; the core doesn't confirm it |
| Burst | Lust and every cooldown at the pull, per the guide (`ToCFactionChampionsBurstWindow` allows both). The base tank-held gate and `OffensivePotionTrigger` exempt the 28 entries (`BossHasNoStableVictim`) | Re-seeded threat lets a tank hold a champion only by chance, so that gate would stay shut |
| Threat redirect | Misdirection and Tricks on the main tank vetoed; smart Tricks only when it resolves to him | Threat is assigned; Tricks' 15% damage on a melee DPS is all it still gives |
| Fears | `RaidAntiFear` (Fear Ward, Tremor Totem, the earth totem guard) while a warlock, priest or warrior champion lives | Only they fear (above) |

## Known gaps

- The guide's openers and positioning are beyond the bots: Death Grip pulls of the kill target, an
  AoE fear to burn trinkets, the tank-led "death ball", and kiting melee champions.
- Hand of Protection (physical only) keeps its champion latched, so melee swing at an immune target.
- Class CC nodes recast into diminishing-returns immunity.
- Stage 6 closes the gate at the kill, before any cleanup runs: the skull stays on the last
  champion and the latch is never cleared (no `reset` switch, no `kill` `0`).

## What a trace answers

The Faction Champions rows, under `--notes fc.`:

| Key | Says |
|---|---|
| `kill` | The kill target, `0` when cleared. Per instance. At a kill the last one runs to the trace end |
| `switch` | Why the latch moved: `first` (from empty), `dead` (died, or left combat), `immune`, `back` (the suspended one returned), `reset` (not live, a wipe). An event, one row per switch |
| `cc` | A CC bot's champion, keyed on the bot, `0` on release |

The counterspell duty has no probe: its node is in `act`, its casts in `cast`.

`tools/botobs/bosses/faction_champions.py` reads the rest: the lineup and the `fc.` keys the pull
never wrote (banner); each kill hold with its reason, length, health at start and end, whether it
died, and the share of non-healer bot samples on it (`--kill`); each bot's CC holds and its
Polymorph, Fear, Cyclone and Entangling Roots casts, on the assigned target and on the kill target
(`--cc`); champion cast-time heals, how many a raid or pet interrupt on the caster followed inside
the cast, kill target against the rest, by interrupter class (`--heals`); the first raid lust, each
bot's first burst cooldown and the tank-held and ToC burst vetoes (`--burst`); Fear, Psychic Scream
and Intimidating Shout on the raid, count and time held, and Fear Ward and Tremor Totem casts
(`--fear`); the four multipliers' vetoes by action (`--vetoes`).

Still invisible: creature deaths, heals and auras. A champion died when it left the snapshots
mid-fight, or was the kill target when the trace closed on a kill; heals landed and heals stopped
are upper bounds, both read off cast starts; Divine Shield, Ice Block and Cyclone on a champion show
only as an `immune` switch.
