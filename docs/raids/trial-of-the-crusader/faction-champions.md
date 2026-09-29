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

The three fears, Divine Shield 66010, Ice Block 65802 and every CC the champions cast but Death Grip
have one id on every difficulty. 53 of the script's 121 ids remap, heals included, all in the client
DBC only.

**Tactics** (Warcraft Tavern, 25-man): kill healers first or the most dangerous DPS first, the
second by the guide's danger table — Rogue and Warrior S, Hunter A, Enhancement, Death Knight and
Retribution B, Warlock, Shadow Priest and Mage C, Balance D. Among healers the Holy Paladin heals
most and his Cleanse strips the raid's CC; the Disc Priest's instants and dispels make her the next
to kill or chain-CC; the Resto Shaman's cast heals are interruptible but killing him is faster; the
Resto Druid, with no dispel, is least threatening. Mark every target, assign CC, and CC dangerous
champions that are free. Lust and every DPS cooldown at the pull: each early kill makes the rest
easier.

## What the defence plays against

**CC on the raid**, one id on every difficulty unless a row is named:

| Spell | Cast by, on | Lasts | Removed by |
|---|---|---|---|
| Polymorph 65801 | Mage, random within 30 yd, every 15 s | 10 s, breaks on damage | Magic |
| Fear 65809 | Warlock, random within 20 yd, every 10-15 s | 10 s | Magic |
| Psychic Scream 65543 | Shadow Priest, and Disc Priest below 50%: 3+ within 8 yd | 8 s | Magic |
| Intimidating Shout 65930 | Warrior, 3+ within 8 yd; 65931 stuns his victim | 8 s | nothing |
| Hex 66054 | Resto Shaman, random within 20 yd, every 45 s | 8 s, pacify and silence, damage doesn't break it | Curse |
| Repentance 66008 | Retribution, lowest threat within 20 yd, every 60 s | 10 s, breaks on damage | Magic |
| Hammer of Justice 66613, 66007 | Holy (farthest within 15 yd), Retribution (top threat within 15 yd), every 40 s | 6 s stun | Magic |
| Wyvern Sting 65877 | Hunter, random within 35 yd, every 60 s | 8 s sleep, breaks on damage | Poison |
| Silence 65542, Strangulate 66018 | Shadow Priest on a mana user within 30 yd, every 45 s; DK on his victim, every 2 min | 5 s | Magic |
| Cyclone 65859 | Balance, farthest within 20 yd, every 25-40 s | 6 s, immune to everything | nothing |
| Blind 65960 | Rogue, lowest threat within 20 yd, every 2 min | 10 s, breaks on damage | nothing |
| Death Grip (row 66017), Chains of Ice 66020 | DK, his victim 12-30 yd away: pulled in, then rooted | 10 s | nothing |

Also Magic: Entangling Roots 65857 (Balance, random within 30 yd), Frost Nova 65792 (Mage below 50%,
3+ within 10 yd), both roots; Psychic Horror 65545 (3 s). Counterspell 65790 (8 s), Spell Lock 67519
(6 s) and Earth Shock (row 65973, 2 s) lock the school of the cast they interrupt, dispels included
(`Player::ProhibitSpellSchool`).

**Unstable Affliction backlash.** The Warlock keeps Unstable Affliction on his victim (row
65812/68154/68155/68156). Dispelling it hits the dispeller with 65813 (row 65813/68157/68158/68159):
5 s silence plus 9,250-10,750 (10N, 25N), 11,563-13,437 (10H) or 13,875-16,125 (25H) shadow.

**Every dispel picks at random.** `Spell::EffectDispel` draws each removal from the target's eligible
auras, weighted by stacks: Purge and Dispel Magic take two, Cleanse one per dispel type, Spellsteal
one. So a magic dispel on a raider carrying Polymorph, DoTs and Unstable Affliction may take any of
them. Mana: Purge 8% of base, Dispel Magic 14%, Spellsteal 20%, Mass Dispel 33%; Tranquilizing Shot
8% on an 8 s cooldown.

**Champion buffs**, positive and Magic, mostly on the kill target: Renew, Power Word: Shield,
Riptide, Rejuvenation, Lifebloom, Regrowth (all remap), Earth Shield 66063 (2 min), Hand of Freedom
(row 66115/68756/68757/68758; the script names the 10H id), Barkskin 65860, Avenging Wrath 66011,
Heroism 65983 or Bloodlust 65980 (40 s, +30% haste on every champion within 100 yd; the
Enhancement Shaman's entry check compares against the Resto entry, so he always casts Bloodlust),
Thorns 66068 (10 min) and Nature's Grasp 66071 (45 s, roots whoever hits the druid). Thorns goes
to each champion in turn, so over a pull nearly every one carries a magic buff. Not dispellable:
Seal of Command, Shadowform, Dispersion, Cloak of Shadows, Deterrence, Icebound Fortitude,
Retaliation, Blade Flurry, Bladestorm.

**Immunity shields**, all mechanic 29 and Magic:

- **Divine Shield** 66010, Holy and Retribution below 25%, 12 s; **Ice Block** 65802, Mage below
  25%, 5 s. Immune to every school, so no Purge, Dispel Magic or Spellsteal lands.
- **Hand of Protection** 66009, 10 s: physical immunity only (aura 39 misc 1) and, unlike a
  player's, no pacify, so the champion keeps fighting. Holy 20-35 s into the pull, Retribution
  25-40 s, then every 5 min each; it goes to the most-injured friendly missing it within 40 or
  30 yd. Magic still lands, so a purge can take it.
- **Mass Dispel** 32375 (1.5 s cast at a point, in range within 30 yd plus the caster's own size):
  up to 10 friendlies within 15 yd, centre to centre, lose a magic debuff; enemies within 10 yd lose
  a magic buff (32592) and one mechanic-29 aura (39897, which ignores immunity). Whether that chain
  reaches a champion immune to Holy is unverified on this core.

**Crowd AoE.** `EnemiesInRange` measures `GetDistance2d`, which subtracts both combat reaches
(champions 0-2.6, players 1.5), and counts every unit on the threat list, pets included. A creature's
AoE hits centre to centre. Champions run at 8.0 yd/s (`speed_run` 1.14286) against a player's 7, so
a melee champion can't be outrun.

- **Bladestorm** 65947, Warrior, 3+ within 8 yd, every 90 s: 8 s aura ticking 65946 each second for
  50% weapon damage to all within 8 yd. Slows him 30%, to 5.6 yd/s.
- **Hellfire** (row 65816/68145/68146/68147), Warlock, 3+ within 9 yd, every 30 s: an interruptible
  15 s channel ticking 65817 (row 65817/68142/68143/68144) each second for 2,500/4,000/7,000/9,000
  fire (10N/25N/10H/25H) to all within 10 yd. The aura sits on the warlock.
- **Instants**: Fan of Knives (row 65955, 10 yd), Divine Storm 66006 (8 yd, 5 targets), Arcane
  Explosion (row 65800, 10 yd, every 6 s), Frost Nova, Psychic Scream and Intimidating Shout (above).
- **No ground AoE** but the Hunter's Frost Trap 65880, a slow: no Blizzard, Consecration or Death and
  Decay.

**Roster by size.** 25-man always fields exactly one paladin, one shaman, one priest and one druid
(the healer or its DPS spec), so Hand of Protection and lust come every pull; 10-man may lack any of
them.

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
| CC dispels | `faction champions dispel cc` (`ACTION_RAID + 3.5f`) gives each CC'd raider one dispeller. Counted, with 2 s or more left: Polymorph, Fear, Psychic Scream, Repentance, both Hammers of Justice, Hex, Wyvern Sting on anyone; Silence and Strangulate on healers. Healers first, then guid; Magic before Curse before Poison. A dispeller: a bot not CC'd, silenced or casting, within 30 yd and in LOS, whose class spell of that type is off cooldown and affordable; non-healers first, one raider each | The class `cure` nodes sit below `ACTION_RAID`, and `party member to dispel` hands every dispeller the same first member, DoTs included. A CC'd healer costs the raid most. Roots, Psychic Horror (3 s) or a 1 s remainder aren't worth a GCD. CC'd means `UNIT_STATE_LOST_CONTROL`, which Hex lacks: it only silences, hence both tests. An idle dispeller is the fastest, and an instant dispel can't double up. A kicked healer is idle, but its dispels share the locked school |
| Unstable Affliction | Magic-removing dispels, duty and class `cure` nodes alike, skip its carrier (`faction champions dispel guard multiplier`); no Mass Dispel while a carrier stands within 15 yd of its point | The backlash above, and a dispel can't pick its aura. Mass Dispel's friendly half reaches the melee around the kill target, often the Warlock's victims |
| Purge | One purger purges the kill target while it holds Hand of Protection or a champion buff above but Thorns and Nature's Grasp (`faction champions purge kill target`, `ACTION_RAID`): first by guid of the non-healer shaman (Purge) and priest (Dispel Magic) bots with the spell off cooldown and affordable, and, while a kill target exists, within 30 yd of it and not CC'd or silenced. Class Purge and Spellsteal are vetoed on champions (`faction champions purge guard multiplier`); Tranquilizing Shot and Devour Magic stay | Guide: "a Shaman or Priest … spamming Purge or Dispel Magic on your kill target"; healers heal. Class purges fire on any dispellable buff on the bot's target, so Thorns alone kept every mage on 20%-of-base-mana Spellsteals. With those vetoed, a purger without a shot must hand the duty on; the first with one keeps it |
| Mass Dispel | A priest bot with it off cooldown, non-healers first, the first in range of the point (above) and in LOS, casts it on Hand of Protection or Divine Shield on the kill target, else Divine Shield on the suspended champion, 3 s or more left (`faction champions mass dispel`, `ACTION_RAID + 5`) | Guide: Divine Shield "can be Mass Dispelled". Ice Block lasts 5 s against a 1.5 s cast. Nothing else casts it here |
| Hand of Protection | While the kill target is immune to physical and not to magic, physical attackers (non-healer melee, hunters) attack `fc.physical` instead (`faction champions physical switch`, `ACTION_RAID + 2.5f`, focus vetoed for them): kept while still eligible, else the first champion in kill order that isn't the kill target, suspended, CC-assigned or physically immune. None left: they stay | Casters keep the kill target, so `fc.kill` stays put |
| Champion AoE | Dodge Bladestorm and Hellfire only, to 12 and 13 yd clear (`faction champions avoid aoe`, `ACTION_EMERGENCY + 6`, `MOVEMENT_FORCED`). One kicker ignores a Hellfire under 2 s old: first by guid of the bots on the warlock, in melee range, with Kick, Pummel, Shield Bash or Mind Freeze off cooldown. Melee hold instead of reaching a target that is a source or within radius + 1 + the bot's melee range of one (`faction champions aoe guard multiplier`, on `reach melee` and `set behind target`) | The only two a bot can see, auras on the caster. Bladestorm slows him to 5.6 yd/s. Guide: Hellfire is "quickly interrupted", and class interrupts already fire on a channel of the bot's target. A ranged interrupt reaches from the clearance; power and the GCD stay out of the kicker's test, or it would flip every swing. A reach back in fights the dodge, and the bot stands up to its melee range on the target's near side |

## Known gaps

- The guide's openers and positioning are beyond the bots: Death Grip pulls of the kill target, an
  AoE fear to burn trinkets, the tank-led "death ball", and kiting melee champions.
- With no eligible champion left, physical attackers keep swinging at the Hand of Protection target.
- Class CC nodes recast into diminishing-returns immunity.
- Stage 6 closes the gate at the kill, before any cleanup runs: the skull stays on the last
  champion and neither latch is cleared (no `reset` switch, no `kill` or `physical` `0`).
- The instant crowd AoE (Fan of Knives, Divine Storm, Arcane Explosion, Frost Nova, Psychic Scream,
  Intimidating Shout) is unanswered: only spacing denies it, and the bots don't spread.
- Cyclone, Blind, Intimidating Shout and Death Grip plus Chains of Ice can't be dispelled, and no
  node answers them.
- Mass Dispel on Divine Shield rests on the unverified chain above; `--hop` shows whether `back`
  came before the shield's own end after a Mass Dispel (`early`).
- A dispel may take a DoT instead of the CC; the duty fires again next tick.
- While Unstable Affliction holds, its carrier gets no magic dispel at all, and no Mass Dispel
  lands within 15 yd of it.

## What a trace answers

The Faction Champions rows, under `--notes fc.`:

| Key | Says |
|---|---|
| `kill` | The kill target, `0` when cleared. Per instance. At a kill the last one runs to the trace end |
| `switch` | Why the latch moved: `first` (from empty), `dead` (died, or left combat), `immune`, `back` (the suspended one returned), `reset` (not live, a wipe). An event, one row per switch |
| `cc` | A CC bot's champion, keyed on the bot, `0` on release |
| `physical` | The physical switch target, `0` when none. Per instance, restated each pull |

Bladestorm and Hellfire write `haz` circles (aura id, source, radius, 1 s ttl), one per source a
second at most. The duties (counterspell, dispel, purge, Mass Dispel) have no probe: their nodes are
in `act`, their casts in `cast`.

`tools/botobs/bosses/faction_champions.py` reads the rest:

- banner: the lineup and the `fc.` keys the pull never wrote;
- `--kill`: each kill hold with its reason, length, health at start and end, whether it died, and
  the share of non-healer bot samples on it;
- `--cc`: each bot's CC holds and its Polymorph, Fear, Cyclone and Entangling Roots casts, on the
  assigned target and on the kill target;
- `--heals`: champion cast-time heals, how many a raid or pet interrupt on the caster followed
  inside the cast, kill target against the rest, by interrupter class;
- `--burst`: the first raid lust, each bot's first burst cooldown, the tank-held and ToC burst
  vetoes;
- `--fear`: Fear, Psychic Scream and Intimidating Shout on the raid, count and time held, and Fear
  Ward and Tremor Totem casts;
- `--dispel`: per counted CC, auras on the raid, time held, removed with over 500 ms left and drawing
  a dispel; the first dispel cast inside each that can remove its type and its delay from the apply,
  by class; Mass Dispels while CC held, counted apart, and how many of them fell inside a champion
  shield's duration; Unstable Affliction backlashes; the duty's `OK` rows;
- `--purge`: raid and pet offensive dispels on champions, kill target against the rest, by spell and
  caster class; the duty's `OK` rows;
- `--hop`: champion Hand of Protection, Divine Shield and Ice Block casts, on the kill target or
  not, the first Mass Dispel inside each and how many, and for the two suspending shields on the
  kill target the first `back` to that champion up to 2 s past its end, `early` when before it; each
  `fc.physical` hold, its length and the share of physical bot samples (tank and melee roles,
  hunters) on it;
- `--aoe`: per crowd-AoE spell, casts, raid victims per cast (its `dmg` rows, aura applies for the
  fears), hits per victim for Bladestorm and Hellfire; the avoid node's `OK` rows;
- `--vetoes`: the eight multipliers' vetoes by action.

Still invisible: creature deaths, heals and auras. A champion died when it left the snapshots
mid-fight, or was the kill target when the trace closed on a kill; heals landed and heals stopped
are upper bounds, both read off cast starts. Divine Shield, Ice Block and Cyclone on a champion show
only as an `immune` switch, the shields and Hand of Protection also as the champion's cast; a purge
or Mass Dispel shows as its own cast, never as the aura it took. Mass Dispel is cast at a point, so
its row names no target. A CC removed with time left was dispelled, broken by damage, or its holder
died. A victim is a player: `dmg` covers no pet, while the champions' 3+ check counts pets.
