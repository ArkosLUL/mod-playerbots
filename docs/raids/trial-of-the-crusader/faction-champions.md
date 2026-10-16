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
  ([raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md#crowd-control-and-threat-on-adds)),
  and a category resets 15 s after its last aura ends. `Unit::GetDiminishing` reads the level the
  next cast lands at.
- **Most CC breaks on damage.** Fear and Entangling Roots break once the damage since the apply
  reaches 2,652 (level-80 warrior base health 12,600 / 4.75, `AuraEffect::CalculateAmount`);
  Polymorph and Hibernate break on any hit (`AURA_INTERRUPT_FLAG_TAKE_DAMAGE`). Cyclone 33786 never
  breaks and makes the champion immune to everything. Raid DoTs, cleaves, Consecration and
  Retribution Aura reach every champion: on 25N (2026-10-16), outside Cyclones, the champions spent
  0.3 s in all more than 2 s past a hit, and 66 of 67 Fears ended within 30 ms of one, median 0.49 s.
- **Exhaustion and Sated** (57723, 57724) last 10 min, outlive a pull and clear on death, so a raid
  lust under 10 min before the pull leaves none here: on 25N the Jaraxxus one, 7.9 min earlier,
  held the raid's to 2:09.
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
Resto Druid, with no dispel, is least threatening. Mark every target (bots mark only CC targets and
attack directly), assign CC, and CC dangerous champions that are free. Lust and every DPS cooldown
at the pull: each early kill makes the rest easier.

**The bots' CC pipelines.** Every bot running `cc` casts its CC nodes on whatever its `rti cc`
names (`moon` by default), assigned or not. Warlock Fear and the druid's Cyclone, Entangling Roots
and Hibernate aim at `rti cc target`; the mage's Polymorph doesn't in a raid: `PolymorphTrigger` is a
`HasCcTargetTrigger`, which reads `rti cc target` only in a non-raid dungeon, and
`CastPolymorphAction` aims at `cc target`, whose scan lets any later attacker overwrite the marked
one. On 25N both assigned mages cast no Polymorph (49 `FAILED`, 11 `IMPOSSIBLE` rows). The druid's
`… on cc` nodes carry the `caster form` prerequisite, which drops cat and bear form (the 25N feral
left cat form for Cyclone); `BalanceDruidStrategy` swaps it for `moonkin form` on Cyclone and Roots,
so a Balance druid casts them in Moonkin Form.

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
- **Measured, 25N 2026-10-16**: crowd AoE was 32% of champion damage to raid players (0.90M of
  2.79M), 97% of it on melee and tanks; Arcane Explosion alone 0.47M (20 casts, 6.7 victims each).
  Ranged and healers took 11% of champion damage; stacked (median 0.5 yd to the nearest raider), all
  13 were feared by the opening Intimidating Shout at 14.7 s. Every death was a melee's.

**Roster by size.** 25-man always fields exactly one paladin, one shaman, one priest and one druid
(the healer or its DPS spec), so Hand of Protection and lust come every pull; 10-man may lack any of
them.

**Totems and treants.**

- The Enhancement Shaman drops Grounding Totem 65989 (NPC 5925, 45 s), Windfury Totem 65990 (6112,
  5 min) or Tremor Totem 65992 (5913, 5 min) 10-20 s in, then every 30 s; health modifiers 0.05, 1.1,
  0.05. The raid's own totems share these entries, so tell them apart by owner. Same-faction
  creatures count as one party (`Unit::IsInPartyWith`), so within 30 yd Grounding (8178) draws the
  next hostile spell aimed at a champion, Windfury (8515) gives the champions 16% melee haste, and
  Tremor (8145) pulses fear, charm and sleep off them.
- The Balance Druid's Force of Nature 65861, 20-40 s in then every 3 min, summons three Treants 36070
  (health modifier 1.5) for 30 s, each attacking the nearest target within 200 yd.
- On 25N either comes only when its class's healer was the one removed.

## What the code decides

| Topic | Decision | Why |
|---|---|---|
| Kill order | Holy Paladin, Disc Priest, Resto Shaman, Resto Druid, then the danger table in the order above. The CC order too. Pets are never picked or CC'd | The guide's healers-first school: champion heals go to the most-injured friendly, the kill target. Pets carry no Aegis and no kill credit |
| Kill latch | `FactionChampionsKillTarget`, one grid scan per instance so every bot agrees, keeps its target while attackable. On its death the best attackable champion takes over, else the best immune one. Turned immune to physical and magic damage both (Divine Shield, Ice Block, Cyclone), it is suspended for the best attackable other, if any (a newer suspension replaces it), and returns once attackable and no healthier than the current. A player's skull (`GetToCHumanSkull`: alive, in combat, selectable, attackable) on a champion that can be hurt outranks all of this (`skull`); CC skips the skulled champion, immune or not | A per-bot, per-tick lowest-health pick splits the raid. Divine Shield and Ice Block fire below 25%. Each half is asked apart: `IsImmunedToDamage` wants one aura covering the whole mask, and Ice Block 65802 is a physical aura plus a magic one. A player's skull wins; through the latch, Counterspell, purge, the physical switch and CC follow it too. `Group::SetTargetIcon` strips every other icon off its target, so a CC icon would erase the skull |
| CC icons | The only icons bots place, each CC bot its own. `faction champions clear cc icons`, on the mechanic tracker of any role, clears a CC icon from a champion not assigned it, and once not live only CC icons still where this encounter put them | A CC bot whose restored `rti cc` names a leftover icon would CC whatever wears it. `IsMechanicTrackerBot` elects any role, so a node gated to non-healers goes silent under a healer tracker. A wipe despawns the champions, so between pulls a stray can't be told from a human's pre-pull mark |
| Focus | Non-healers attack the kill target directly (`faction champions focus priority`, pet sent too). The rule `faction champions target guard multiplier` (the reader's name) zeroes `DpsAssist`/`TankAssist` exactly while that node has a kill target. Tanks join; no taunt peel | Assist makes its own pick and would take the bot off the kill target on any tick the settled focus node returns false. Taunt can't keep a champion (above), and the guide has no tank role |
| CC | CC bots: druid bots knowing Cyclone and running `cc`, not healers, melee or tanks. Each gets its own champion, in kill order skipping the kill target, a suspended and a skulled one, kept while both stay valid, and its own icon (moon, square, triangle, diamond, circle, star, cross) through `SetRtiCcTarget`. Its prior `rti cc` comes back on release, or once not live through `toc restore rti cc`. The next kill target stays CC'd until its turn; a Cyclone on it at the switch is an ordinary suspension | Only Cyclone holds in the melee pile (above); a melee druid would leave cat form for it, and the mage's Polymorph never aims at `rti cc` in a raid. Champions diminish like players, so CC bots piling on one moon burn its DR. CC on the suspended one breaks or blocks the latch's return. `rti cc` is saved to the bot's DB store and outlives the pull; the restore node has no encounter prefix, so it still runs after the kill closes the gate, for a bot dead then |
| CC casts | `faction champions cc guard multiplier` zeroes every class CC node (`CastCrowdControlSpellAction`) unless its target is the bot's assignment and the cast pays (`FactionChampionsCcCastPays`): its aura doesn't break on damage, the champion isn't immune, and by the core's DR table the next application lasts over 2.5 s. So Cyclone goes at 6 s and 3 s, never at 1.5 s or into immunity, on every difficulty and even with a heroic champion's trinket ready; Fear, Polymorph, Entangling Roots and Hibernate never go | 25N: four CC bots spent 55-80% of the pull on 417 Fears and Cyclones, for 4-7% fear uptime, costing about 22k raid DPS. Breakable CC breaks at once (above); a "not hit for 2 s" wait isn't built, since the core keeps no last-hit time and on 25N it would never have passed. A Cyclone cycle is about 3 s of casting for 9 s held per 27 s, against the 25N druid's 153 s of 245, and denies a third of the next healer's output (Thrakgar healed 10.4k/s alive) for about 11% of one druid's time, so it stays on normal. `rti cc` defaults to `moon`, so a bot outside the pool would still CC the druid's champion. The trinket goes on cooldown only when used, so the first Cyclone burns it and the next one holds |
| Interrupts | One mage, first by guid within 30 yd and neither CC'd nor silenced, counterspells the kill target's cast-time or channelled helpful spell unless it is immune (`faction champions counterspell kill target`, which re-checks the duty when it pops) | No mage node interrupts its own target: there is no current-target counterspell, and every `… on enemy healer` node skips the bot's target. Rogue, shadow priest, warrior, paladin, warlock pet, DK and shaman already cover the kill target, their healer nodes the others |
| AoE | Suppressed | Champion's Aegis. The guide exempts Death and Decay and Pestilence; the core doesn't confirm it |
| Burst | Lust and every cooldown at the pull, per the guide (`ToCFactionChampionsBurstWindow` allows both). The base tank-held gate and `OffensivePotionTrigger` exempt the 28 entries (`BossHasNoStableVictim`) | Re-seeded threat lets a tank hold a champion only by chance, so that gate would stay shut |
| Threat redirect | Misdirection and Tricks on the main tank vetoed; smart Tricks only when it resolves to him | Threat is assigned; Tricks' 15% damage on a melee DPS is all it still gives |
| Fears | `RaidAntiFear` (Fear Ward, Tremor Totem, the earth totem guard) while a warlock, priest or warrior champion lives | Only they fear (above) |
| CC dispels | `faction champions dispel cc` (`ACTION_RAID + 3.5f`) gives each CC'd raider one dispeller. Counted, with 2 s or more left: Polymorph, Fear, Psychic Scream, Repentance, both Hammers of Justice, Hex, Wyvern Sting on anyone; Silence and Strangulate on healers. Healers first, then guid; Magic before Curse before Poison. A dispeller: a bot not CC'd, silenced or casting, within 30 yd and in LOS, whose class spell of that type is off cooldown and affordable; non-healers first, one raider each | The class `cure` nodes sit below `ACTION_RAID`, and `party member to dispel` hands every dispeller the same first member, DoTs included. A CC'd healer costs the raid most. Roots, Psychic Horror (3 s) or a 1 s remainder aren't worth a GCD. CC'd means `UNIT_STATE_LOST_CONTROL`, which Hex lacks: it only silences, hence both tests. An idle dispeller is the fastest, and an instant dispel can't double up. A kicked healer is idle, but its dispels share the locked school |
| Unstable Affliction | Magic-removing dispels, duty and class `cure` nodes alike, skip its carrier (`faction champions dispel guard multiplier`); no Mass Dispel while a carrier stands within 15 yd of its point | The backlash above, and a dispel can't pick its aura. Mass Dispel's friendly half reaches the melee around the kill target, often the Warlock's victims |
| Purge | One purger purges the kill target while it holds Hand of Protection or a champion buff above but Thorns and Nature's Grasp (`faction champions purge kill target`, `ACTION_RAID`): first by guid of the non-healer shaman (Purge) and priest (Dispel Magic) bots with the spell off cooldown and affordable, and, while a kill target exists, within 30 yd of it and not CC'd or silenced. Class Purge and Spellsteal are vetoed on champions (`faction champions purge guard multiplier`); Tranquilizing Shot and Devour Magic stay | Guide: "a Shaman or Priest … spamming Purge or Dispel Magic on your kill target"; healers heal. Class purges fire on any dispellable buff on the bot's target, so Thorns alone kept every mage on 20%-of-base-mana Spellsteals. With those vetoed, a purger without a shot must hand the duty on; the first with one keeps it |
| Mass Dispel | A priest bot with it off cooldown, non-healers first, the first in range of the point (above) and in LOS, casts it on Hand of Protection or Divine Shield on the kill target, else Divine Shield on the suspended champion, 3 s or more left (`faction champions mass dispel`, `ACTION_RAID + 5`). Never on AoE fears or champion lust, the guide's other two uses | Guide: Divine Shield "can be Mass Dispelled". Ice Block lasts 5 s against a 1.5 s cast. Nothing else casts it here. Fears, 25N: Psychic Scream held 8 raiders 8.5 s in all, 4 of whom drew a dispel, and the costly Intimidating Shout (22 raiders, 87 s) can't be dispelled. Lust comes once a pull and the purge duty already takes it off the kill target; the hostile half draws one random magic buff per champion within 10 yd, where Thorns, HoTs and shields crowd the draw, and the Unstable Affliction guard blocks it in the ball |
| Hand of Protection | While the kill target is immune to physical and not to magic, physical attackers (non-healer melee, hunters) attack `fc.physical` instead (`faction champions physical switch`, `ACTION_RAID + 2.5f`, focus vetoed for them): kept while still eligible, else the first champion in kill order that isn't the kill target, suspended, CC-assigned or physically immune. None left: they stay | Casters keep the kill target, so `fc.kill` stays put |
| Champion AoE | Dodge Bladestorm and Hellfire only, to 12 and 13 yd clear (`faction champions avoid aoe`, `ACTION_EMERGENCY + 6`, `MOVEMENT_FORCED`). One kicker ignores a Hellfire under 2 s old: first by guid of the bots on the warlock, in melee range, with Kick, Pummel, Shield Bash or Mind Freeze off cooldown. Melee hold instead of reaching a target that is a source or within radius + 1 + the bot's melee range of one (`faction champions aoe guard multiplier`, on `reach melee` and `set behind target`) | The only two a bot can see, auras on the caster. Bladestorm slows him to 5.6 yd/s. Guide: Hellfire is "quickly interrupted", and class interrupts already fire on a channel of the bot's target. A ranged interrupt reaches from the clearance; power and the GCD stay out of the kicker's test, or it would flip every swing. A reach back in fights the dodge, and the bot stands up to its melee range on the target's near side |
| Spread | None against the crowd AoE | 97% of it hits melee and tanks (above), who stack on a melee kill target anyway; on 25N ranged and healers lost one opening Shout and 14k of Bladestorm to it. Denying the 3+ check takes about 14 yd between every ranged and healer pair (11 yd centre to centre plus the caster's offset from his victim), which 14 bots can't hold within spell range of the ball. Guide: the fastest kills are the death ball |
| Totems | Each champion totem (alive, `IsTotem`, owned by a champion, attackable) gets one killer, latched in `fc.totem`: the nearest eligible group bot holding no totem (`GetExactDist2d`, ties by guid), eligible meaning alive, on the map, in control, not a healer and within 30 yd. The hold ends once the totem stops counting or its bot turns ineligible. `faction champions kill totem` (`ACTION_RAID + 2.7f`) attacks it directly; the rule `faction champions totem guard multiplier` blocks focus and the physical switch for the holder | Guide: "a melee player dedicated to killing his totems". Grounding eats a Cyclone or Counterspell, Windfury hastes the champions' melee, Tremor undoes fears (above). The raid's own totems share the entries. The settled duty returns false, so without the rule focus drags the bot back to the kill target. No trace yet has an Enhancement Shaman |
| Treants | No node; `--adds` measures them | No trace has a Balance Druid yet, the guide doesn't mention them, and they're three 30 s adds every 3 min |

## Known gaps

- The guide's openers and positioning are beyond the bots: Death Grip pulls of the kill target, an
  AoE fear to burn trinkets, the tank-led "death ball", and kiting melee champions.
- With no eligible champion left, physical attackers keep swinging at the Hand of Protection target.
- Stage 6 closes the gate at the kill, before any cleanup runs: no latch is cleared (no `reset`
  switch, no `kill` or `physical` `0`), and `ccaura` and `totem` keep their last values.
- No crowd-AoE spread: the melee stack takes the instant crowd AoE (Fan of Knives, Divine Storm,
  Arcane Explosion, Frost Nova, Psychic Scream, Intimidating Shout), and ranged and healers stand
  stacked, so an opening Shout or Bladestorm on them catches all of them.
- The mage champion's Arcane Explosion, 17% of champion damage on 25N, sits under a C-tier kill
  order.
- Melee deaths from the script's threat picks (all 5 on 25N) get no answer.
- Treants are unanswered.
- With no balance druid bot in the raid there is no CC; on heroic the trinket breaks the first
  Cyclone on each champion every 2 min.
- Within 30 yd a Grounding Totem draws the next Cyclone or Counterspell aimed at a champion before
  the duty kills it.
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
| `switch` | Why the latch moved: `first` (from empty), `dead` (died, or left combat), `immune`, `back` (the suspended one returned), `skull` (a player's skull took it), `reset` (not live, a wipe). An event, one row per switch |
| `cc` | A CC bot's champion, keyed on the bot, `0` on release |
| `physical` | The physical switch target, `0` when none. Per instance, restated each pull |
| `ccaura` | Keyed on the champion: the spell of a player's fear, stun or confuse aura on it (Cyclone, Polymorph), `0` when none. The only record of CC on a champion; roots aren't in it |
| `totem` | A totem killer's totem, keyed on the bot, `0` on release |

Bladestorm and Hellfire write `haz` circles (aura id, source, radius, 1 s ttl), one per source a
second at most. The duties (counterspell, dispel, purge, Mass Dispel) have no probe: their nodes are
in `act`, their casts in `cast`. Why a CC bot waited reads off the CC guard's `veto` rows.

`tools/botobs/bosses/faction_champions.py` reads the rest:

- banner: the lineup and the `fc.` keys the pull never wrote;
- `--kill`: each kill hold with its reason, length, health at start and end, whether it died, and
  the share of non-healer bot samples on it;
- `--cc`: each bot's CC holds; per champion, `fc.ccaura` time by spell and fear uptime (the share of
  its live samples from the pull at moveGen 9, fleeing); per bot its Polymorph, Fear, Cyclone and
  Entangling Roots casts, started and landed, on the assigned target and on the kill target. A Fear
  lands when its target turns to fleeing within the cast time + 1 s, a Cyclone or Polymorph when
  `fc.ccaura` turns to that spell (`n/a` without the key, and for Roots), each turn landing one
  cast. A CC broken between two samples shows in neither, so landed is a lower bound: 52 of 67
  Fears on 25N;
- `--heals`: champion cast-time heals, how many a raid or pet interrupt on the caster followed
  inside the cast, kill target against the rest, by interrupter class;
- `--burst`: the first raid lust; each member who carried Exhaustion or Sated into the pull (its
  first row of either a removal), `expired` (`dur` 0) or `cleared` (time left, a death), how many and
  when the last expired; each bot's first burst cooldown; the tank-held and ToC burst vetoes;
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
- `--aoe`: per crowd-AoE spell, casts, raid victims per cast (its `dmg` rows from 50 ms before the
  cast row, since an instant's land 0-6 ms ahead of it; aura applies for the fears), hits per victim
  for Bladestorm and Hellfire; the avoid node's `OK` rows;
- `--adds`: champion totem and Force of Nature casts; each champion totem and treant with its first
  and last snapshot, and treant damage to the raid with its victims; each `fc.totem` hold, its length
  and whether the totem left the snapshots inside it; the duty's `OK` rows;
- `--vetoes`: the ten multipliers' vetoes by action.

Still invisible: creature deaths, heals and auras but `fc.ccaura`'s. A champion died when it left
the snapshots mid-fight, or was the kill target when the trace closed on a kill; heals landed and
heals stopped are upper bounds, both read off cast starts. Divine Shield, Ice Block and Cyclone on a
champion show as an `immune` switch, Cyclone also on `fc.ccaura`, the shields and Hand of Protection
also as the champion's cast; a purge or Mass Dispel shows as its own cast, never as the aura it
took. Exhaustion or Sated applied before the trace shows only as its removal; a fresh lust writes
the caster's copy as a removal with time left, then the member's apply. Mass Dispel is cast at a
point, so its row names no target. A CC removed with time left was dispelled, broken by damage, or
its holder died. A victim is a player: `dmg` covers no pet, while the champions' 3+ check counts
pets.
