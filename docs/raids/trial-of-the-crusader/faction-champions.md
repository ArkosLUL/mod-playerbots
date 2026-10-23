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
4 s later and sets `IsEncounterInProgress`. Released, they idle (no AI out of combat) until a player
comes within 23 yd (`detection_range` 20 + the level gap, 3D centre to centre) or attacks one. The
first to engage calls `SetInCombatWithZone` and starts the others on their nearest player
(`TYPE_FACTION_CHAMPIONS_START`, `SelectNearestTarget(200)`). The last champion's death gives kill credit and stage 6. A wipe
(`TYPE_FAILED` → `InstanceCleanup`) despawns them at stage 4; the next pull brings new guids and a new
lineup.

## Mechanics every choice rests on

- **Threat is assigned, never earned.** Zone combat adds every player at 0 threat. On engage, 2 s
  later, then every 8.75-9.25 s, `RecalculateThreat` resets each player holding **nonzero** threat on
  that champion (damage on it, or any heal, self-heals included, split over every champion) to
  10,000,000 × `mod_dist × mod_health × mod_armor`: health 2.0 above 40,000 current, rising to 6.0 near
  0; melee and pets `15/(10+d)` past 5 yd and the damage share armour leaves (`1 − A/(A+16635)`, level
  83); casters `25/(d−10)` past 35 yd, no armour. So at the release each champion takes the nearest
  player, and a tank (~0.72 for melee, 2.0 for casters) keeps a melee champion only while plate DPS
  stand ~22+ yd off it, leather and mail ~37, a plate healer ~31, cloth ~57; casters take almost any
  raider within 50-57 yd. A plate DPS in melee range (~1.4-1.6) keeps one against cloth ~23-27 yd off
  and leather or mail ~13. A taunt lasts until the next reset, a threat redirect does nothing, and pets
  keep earned threat, which never nears a player's 10M x modifier.
- **The champions focus one victim together**: the same modifiers give every one the same pick, casters
  too (the lowest current health within 35 yd), and they stay on it until it dies or turns suppressed
  (`ThreatReference::ShouldBeSuppressed`: immune to their melee school, physical for all ten, so Hand
  of Protection, Divine Shield and Ice Block drop it from every champion; confused; or under a
  breakable stun), then all take the next. A recount moves one off its victim only past 110% of its
  score, or 130% for a new victim out of its melee range (`ThreatManager::ReselectVictim`). Retail
  also reset every 9 s and "frequently" ganged up on one raider (Warcraft Wiki), damage counted.
  25H (2026-10-17): each switch 1-7 yd off, a hurt bot taken from 19-21 yd; melee took 1.05M in
  the first 60 s, ~0.31M of it crowd AoE; the kill target had 2+ other champions within 8 yd in
  every sample. 25H 2026-10-22: 5+ champions on one raider in 77% of samples, each until it died;
  Smartface's Ice Block moved them 0.5 s later.
- **Taunt**: normal templates obey it with diminishing returns; heroic ones (`CreatureImmunitiesId`
  -13) are immune to taunt and Attack Me. Both are immune to charm.
- **Death Grip** 49576 lands on both: `spell_dk_death_grip` exempts the 28 entries from its boss
  check, the taunt and Attack Me effects of its 49560 bounce off a heroic champion while the dummy
  still makes it jump in front of the DK, leaving no aura and so no diminishing return. On normal
  the taunt aura shares `DIMINISHING_TAUNT` with every class taunt, so the fifth inside the window
  misses outright. The hit interrupts a cast. No rune cost, 35 s cooldown, 30 yd.
- **What cuts a grip.** The pull is the champion's own 57604 (`SPELL_EFFECT_JUMP_DEST`), cast with the
  grip: `Spell::CheckCast` refuses it on a rooted caster, and a root or stun landing mid-flight runs
  `SendMoveRoot` → `StopMoving`, ending the jump where it is. A stun already on doesn't block it (no
  `UNIT_STATE_ROOT`; `SetControlled` returns early). 25H 2026-10-19: a hunter pet's Charge (61685, root
  53148) refused or cut the bot DK's link in both pulls (3-4 yd carried), and a bot's Frost Nova cut the
  player's 22 yd short.
- **25H health**: the template's 3.57M (non-cloth) and 2.86M (cloth) times `mod-dungeon-scale`'s 1.3
  (`AC_DUNGEON_SCALE_STAT_MODIFIER_RAID_HEROIC_BOSS_HEALTH`, every champion being boss-flagged):
  4.64M and 3.71M.
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
- **Banish and Disarm.** The Resto Druid champion (34459/34469, heroic 35688/35716) is an elemental,
  so Banish (710/18647: stun plus school immunity, never breaks on damage) lands and Polymorph and
  Hex don't; the other three healers are humanoid. A disarmed Warrior champion skips Bladestorm and Mortal Strike, rechecking
  every 5 s (`boss_faction_champions.cpp:1559-1592`); the other melee gate abilities on
  `UNIT_FLAG_DISARMED` too, and a disarmed creature's weapon damage halves (`StatSystem.cpp:1156`;
  Dismantle takes both hands). Heroic immunities (-13) cover neither disarm, roots nor fear. DR applies
  on apply, so a CC the trinket breaks still costs its step.
- **One target per caster.** Cyclone, Polymorph, Fear, Banish, Hex and Entangling Roots carry
  `SPELL_ATTR5_LIMIT_N`: a caster's next one of that spell (same rank chain, `Aura::IsSingleTargetWith`)
  on another unit ends the first. Fear and Banish are separate spells, so a warlock holds one of each.
- **Who kills**, 25H 2026-10-20, damage to the raid: DK 20% and rogue 17%, then rogue 19% (8 kills) and
  Retribution 17%; warrior, hunter and mage 13-15%. The four melee dealt 52-65%, the rogue steadiest at
  the top.
- **Exhaustion and Sated** (57723, 57724) last 10 min, outlive a pull and clear on death, so a raid
  lust under 10 min before the pull leaves none here: on 25N the Jaraxxus one, 7.9 min earlier,
  held the raid's to 2:09.
- **Heroic PvP trinket** 65547, 2 min cooldown, no DR check: breaks fear, stun, confuse or freeze (a
  frost root or stun; Polymorph, Cyclone, Intimidating Shout, Hungering Cold and Frost Nova included)
  the moment the champion's AI next updates, and clears every snare and root with it. Never on Chains
  of Ice and other snares, nature roots, silences, Disarm, Dismantle, Hex or Death Grip, and a silenced
  champion can't use it. Roots are not in `IsCCed`, so a rooted champion keeps casting. Its cooldown is
  the creature's own (`HasSpellCooldown`), so bots can read it.
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
- **Snares**: none diminishes. In reach of a champion: Hamstring and Wing Clip (melee), Piercing
  Howl (10 yd round the warrior), Chains of Ice (20 yd, a frost rune), Frost Shock (25 yd),
  Concussive Shot (35 yd). Frost Trap and Earthbind Totem sit at the caster's feet (no Trap
  Launcher in 3.3.5). Hand of Freedom makes a champion immune.

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
Retribution B, Warlock, Shadow Priest and Mage C, Balance D. Its death ball: DPS start across the room,
a tank runs in for the champions' attention (with everyone 40+ yd off they take the nearest), DKs grip
a healer as far off the pack as they can, chaining grips, and a second one once the first is near 20%,
an AoE CC burns the trinkets, ranged stay as far back as they can still hit, escapees are CC'd until
gripped back, and after two healers the raid finishes the clump. Among healers the Holy Paladin heals
most and his Cleanse strips the raid's CC; the Disc Priest's instants and dispels make her the next
to kill or chain-CC; the Resto Shaman's cast heals are interruptible but killing him is faster; the
Resto Druid, with no dispel, is least threatening. Mark every target (bots mark nothing and attack
directly), assign CC, and CC dangerous champions that are free: a Mage and a Warlock alternate
Polymorph and Fear on the healers not being killed, swapping at DR, and Banish or Fear the Resto
Druid. Keep melee champions off the raid: Chains of Ice and Death Grip, Hamstring and Disarm (no
Bladestorm while disarmed), Frost Trap, Entangling Roots, Frost Nova, Thunderstorm. Lust and every
DPS cooldown at the pull: each early kill makes the rest easier.

**Heroic raids** (warcraft.wiki.gg, Engadget 2009, mmo-champion threads, Sept-Dec 2009): one controller
per melee champion, away from the raid and the other champions, often a DK with Death Grip and Chains
of Ice ("if a kiter dies before his target is dead your raid will wipe"); trinket bait at the pull
(Hungering Cold, a Charge into Intimidating Shout, Psychic Scream), then real CC; a 50% healing debuff
on the kill target; a fear bomb when several champions converge, trinkets down. Kill order had no
consensus: healers first was most common.

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
| Kill order | Holy Paladin, Disc Priest, Resto Shaman, Resto Druid; then the casters: Hunter, Mage, Warlock, Shadow Priest, Balance; then the melee, held apart till last: Rogue, DK, Warrior, Retribution, Enhancement. Pets are never picked or CC'd | The guide's healers-first school: champion heals go to the most-injured friendly, the kill target. 25H 2026-10-22: casters dealt 41-50% of the damage and 15 of 22 healer deaths, the Hunter killed most. Pets carry no Aegis and no kill credit |
| Kill latch | `FactionChampionsKillTarget`, one grid scan per instance so every bot agrees, keeps its target while attackable. A healer pulled out of the pack (gripped: it moved 10+ yd inside 1 s, until back within 20 yd of the pack centre; and 20+ yd off that centre, no champion within 12 yd) takes the latch off one above 30% never pulled out (`pulled`), and `first`, `dead` and `immune` prefer one, farthest off the pack first; a kill target once pulled is kept. Otherwise on its death the best attackable champion takes over, else the best immune one. Turned immune to physical and magic damage both (Divine Shield, Ice Block, Cyclone), it is suspended for the best attackable other, if any (a newer suspension replaces it), and returns once attackable and no healthier than the current. A player's skull (`GetToCHumanSkull`: alive, in combat, selectable, attackable) on a champion that can be hurt outranks all of this (`skull`); CC skips the skulled champion, immune or not. The pack centre starts at the landing centroid (`GetFactionChampionsLandingCentre`) and each refresh moves to the mean of the champions but the kill target within 20 yd of the last one; with none, round the champion with the most others within 15 yd | A per-bot, per-tick lowest-health pick splits the raid. A pulled healer is out of its friends' 40 yd heal reach, and a human DK's grip picks it too: 25H 2026-10-18, one gripped the Resto Druid out at 0.6 s while the bots stayed on the Holy Paladin in the pack. Divine Shield and Ice Block fire below 25%. Each half is asked apart: `IsImmunedToDamage` wants one aura covering the whole mask, and Ice Block 65802 is a physical aura plus a magic one. A player's skull wins; through the latch, Counterspell, purge, the physical switch and CC follow it too. The pack goes wherever its picks walk it, 32-40 yd off the landing spot on 25H 2026-10-19, so its centre follows. Champion healers carry no blink or charge, and a run covers ~7 yd/s, so only a grip moves one 10 yd in 1 s. 25H 2026-10-20, on distance alone: Thrakgar at 40% stopped counting as pulled when the DK champion walked up, and the latch went to a Resto Druid that had wandered off ungripped (99 → 72% in 59 s, no kill); likewise off Caiphus at 90% |
| Focus | Non-healers holding no champion, tanks included, attack the kill target directly (`faction champions focus priority`, pet sent too). The rule `faction champions target guard multiplier` (the reader's name) zeroes `DpsAssist`/`TankAssist` exactly while that node has a kill target, or the bot holds a champion | Assist makes its own pick and would take the bot off the kill target on any tick the settled focus node returns false. A holder hits only its own champion (Holders) |
| CC | One duty per cast (`faction champions crowd control`, `ACTION_RAID + 3.4`, probe `fc.cc`), none for 1.5 s after the pull. CC bots: dps mages (Polymorph), warlocks (Fear; Banish on an elemental or demon), ranged druids (Cyclone), dps shamans, Enhancement included (Hex). Candidates: healers off the tanks and holders nearest the kill target, other healers, the danger champion (Peel), champions off the tanks and holders casters first, the rest in kill order; not the kill target, a suspended, skulled, pulled or held one, nor one the grip guard holds. None while CC'd 2+ s more, a CC cast at it is under way or it is in Bladestorm. A caster whose spell holds a champion longer than its own cast time + 0.4 s casts it nowhere else (Mechanics) unless moving it off a non-healer onto a healer; recasting on that champion stays open. The caster: the spell landing (creature type, immunity) and holding over 2.5 s by DR, in range and LOS or up to 15 yd past the range, which it walks to 3 yd inside (stand released meanwhile; never from a Cyclone stand); in range first, then never-breaking, then the longest hold, which by DR prefers another group's spell on a champion coming out of one (Banish 6 → 3 s, then Cyclone), then the nearest. Polymorph, Fear and Hex only on a champion with no group member or pet on it (victim or selection, CC bots aside) and no group DoT. Released when a fresh one lands (over its cast time + 0.4 s left, so the caster's own fading one doesn't count) or 3 s pass with none going (6 s for a walk); a cast at a champion that stops being a candidate is interrupted. Every difficulty | Guide: a Mage and a Warlock alternate Polymorph and Fear on the healers not being killed, Banish or Fear the Resto Druid. Ranking by hold gives that pair without fixed pairs: Polymorph 10 → 5 s, then Fear 10 → 5 s while Polymorph's 15 s DR resets. Breakable CC breaks at once in the pile (above); 25N: four CC bots spent 55-80% of the pull on 417 Fears and Cyclones for 4-7% fear uptime. 25H 2026-10-18: Cyclone druids alone, 2 Cyclones in 80 s, two mages and three warlocks idle. One caster per champion: champions diminish like players, so piling burns its DR. A Cyclone or Banish would make the opening grip's healer or the next grip's immune to Death Grip and to the kill; CC on the suspended one breaks or blocks the latch's return; one on the next kill target at its switch is an ordinary suspension. A Cyclone cycle, ~3 s of casting for 9 s held per 27 s, denies a third of the next healer's output (Thrakgar healed 10.4k/s alive) for ~11% of one druid's time, so it runs on normal too. The trinket breaks the first CC on a champion, so the next holds. 25H 2026-10-19: Pestilence and Death and Decay diseased nearly every pack champion (pull 1: 0 Fears, 1 Polymorph), and the stands kept the CC bots 40-58 yd from the champion healers (no duty 15-36 s), so they were held 9-31%. 25H 2026-10-20: a landed duty freed its bot for the next champion, so 29/39 and 39/43 of Druidica's Cyclones ended as its next landed (~1.3 s held) and it never hit the kill target in 80 s; Power's recast duty flapped every refresh on its own fading Polymorph (62 duties, 10 casts). Later, with a flat 2 s release: the next cast elsewhere landed 1+ s early and the healer exception sent Druidica straight back, so 12/18 and 10/15 Cyclones ended on its own next one; 3 went into Bladestorm |
| CC casts | `faction champions cc guard multiplier` zeroes every class CC node (`CastCrowdControlSpellAction`) while live, and a DoT spread (`… on attacker`) at any champion but the kill target | The class nodes cast at their own `cc target` or `rti cc` picks (above); a spread DoT breaks the CC on its first tick and keeps Polymorph, Fear and Hex off for its whole run. 25H 2026-10-21: raid members and pets targeted the four casters 10-52% of their live time, 13 and 10 Polymorphs a pull |
| Interrupts | One mage, first by guid within 30 yd and neither CC'd nor silenced, counterspells the kill target's helpful spell unless it is immune: a channel at once, a cast in its last 600 ms, starting no cast-time spell of its own while it waits (`faction champions counterspell kill target`, which re-checks the duty when it pops). `faction champions late interrupt multiplier` holds every class interrupt, silence and `… on enemy healer` node (the stuns too) on a champion's cast-time spell until its last 600 ms. Hellfire: one bot, the nearest with an interrupt that reaches the warlock, ranged first (Counterspell, Wind Shear, Silence, Strangulate, Silencing Shot, a felhunter's Spell Lock), else a melee kick, interrupts it at once (`faction champions channel interrupt`, `ACTION_INTERRUPT + 4`, probe `fc.kick`) | No mage node interrupts its own target: there is no current-target counterspell, and every `… on enemy healer` node skips the bot's target. Rogue, shadow priest, warrior, paladin, warlock pet, DK and shaman already cover the kill target, their healer nodes the others. The user's call, as in PvP: a kick at the end wastes the cast; `ReactDelay` 100 ms leaves several ticks in 600. 25H 2026-10-19: heals were kicked 6-10% into the cast, and 3 of 4 Hellfires ran ~9 s: a negative channel, kicked only by bots targeting the warlock |
| AoE | Suppressed, the raid's own splash too: area spells (`SpellSplashes`), the totem and trap summons whose spell reads positive (`SummonSplashes`: Magma Totem, Fire Nova, Explosive Trap and its Trap Launcher) and Death and Decay, Pestilence, Blood Boil, Consecration, Hammer of the Righteous, Avenger's Shield and Swipe (bear) (`faction champions suppress aoe multiplier`); area heals and buffs stay | Champion's Aegis. The guide exempts Death and Decay and Pestilence; the core doesn't confirm it. A Death and Decay or Consecration breaks every Polymorph and Fear in the pack, and Pestilence's diseases rule each champion out for both. 25H 2026-10-23: 6-7 Trap Launcher Explosive Traps a pull and 8 Fire Novas got past the positive-spell exemption |
| Burst | Lust and every cooldown at the pull, per the guide (`ToCFactionChampionsBurstWindow` allows both). The base tank-held gate and `OffensivePotionTrigger` exempt the 28 entries (`BossHasNoStableVictim`) | Re-seeded threat lets a tank hold a champion only by chance, so that gate would stay shut |
| Threat redirect | Misdirection and Tricks on the main tank vetoed; smart Tricks only when it resolves to him | Threat is assigned; Tricks' 15% damage on a melee DPS is all it still gives |
| Fears | `RaidAntiFear` (Fear Ward, Tremor Totem, the earth totem guard) while a warlock, priest or warrior champion lives | Only they fear (above) |
| CC dispels | `faction champions dispel cc` (`ACTION_RAID + 3.5f`) gives each CC'd raider one dispeller. Counted, with 2 s or more left: Polymorph, Fear, Psychic Scream, Repentance, both Hammers of Justice, Hex, Wyvern Sting on anyone; Silence and Strangulate on healers. Healers first, then guid; Magic before Curse before Poison. A dispeller: a bot not CC'd, silenced or casting, within 30 yd and in LOS, whose class spell of that type is off cooldown and affordable; non-healers first, one raider each | The class `cure` nodes sit below `ACTION_RAID`, and `party member to dispel` hands every dispeller the same first member, DoTs included. A CC'd healer costs the raid most. Roots, Psychic Horror (3 s) or a 1 s remainder aren't worth a GCD. CC'd means `UNIT_STATE_LOST_CONTROL`, which Hex lacks: it only silences, hence both tests. An idle dispeller is the fastest, and an instant dispel can't double up. A kicked healer is idle, but its dispels share the locked school |
| Unstable Affliction | Magic-removing dispels, duty and class `cure` nodes alike, skip its carrier (`faction champions dispel guard multiplier`); no Mass Dispel while a carrier stands within 15 yd of its point | The backlash above, and a dispel can't pick its aura. Mass Dispel's friendly half reaches the melee around the kill target, often the Warlock's victims |
| Purge | One purger purges the kill target while it holds Hand of Protection or a champion buff above but Thorns and Nature's Grasp (`faction champions purge kill target`, `ACTION_RAID`): first by guid of the non-healer shaman (Purge) and priest (Dispel Magic) bots with the spell off cooldown and affordable, and, while a kill target exists, within 30 yd of it and not CC'd or silenced. The next purger by the same election takes Bloodlust, Heroism, Avenging Wrath and Hand of Freedom off a pack champion within 30 yd, melee on their holders first. Class Purge and Spellsteal are vetoed on champions (`faction champions purge guard multiplier`); Tranquilizing Shot and Devour Magic stay | Guide: "a Shaman or Priest … spamming Purge or Dispel Magic on your kill target"; healers heal. Class purges fire on any dispellable buff on the bot's target, so Thorns alone kept every mage on 20%-of-base-mana Spellsteals. With those vetoed, a purger without a shot must hand the duty on; the first with one keeps it. 25H 2026-10-19: the champions' Bloodlust and Avenging Wrath (Malithas, top damage on Justice) went unpurged |
| Mass Dispel | A priest bot with it off cooldown, non-healers first, the first in range of the point (above) and in LOS, casts it on Hand of Protection or Divine Shield on the kill target, else Divine Shield on the suspended champion, 3 s or more left (`faction champions mass dispel`, `ACTION_RAID + 5`). Never on AoE fears or champion lust, the guide's other two uses | Guide: Divine Shield "can be Mass Dispelled". Ice Block lasts 5 s against a 1.5 s cast. Nothing else casts it here. Fears, 25N: Psychic Scream held 8 raiders 8.5 s in all, 4 of whom drew a dispel, and the costly Intimidating Shout (22 raiders, 87 s) can't be dispelled. Lust comes once a pull and the purge duty already takes it off the kill target; the hostile half draws one random magic buff per champion within 10 yd, where Thorns, HoTs and shields crowd the draw, and the Unstable Affliction guard blocks it in the ball |
| Hand of Protection | While the kill target is immune to physical and not to magic, physical attackers (non-healer melee, hunters) attack `fc.physical` instead (`faction champions physical switch`, `ACTION_RAID + 2.5f`, focus vetoed for them): kept while still eligible, else the first champion in kill order that is pulled and isn't the kill target, suspended, under breakable CC (`FactionChampionsCcProtected`) or physically immune. None: they stay | Casters keep the kill target, so `fc.kill` stays put. One in the pack would take the melee into it |
| Champion AoE | Dodge Bladestorm and Hellfire only, to 12 and 13 yd clear (`faction champions avoid aoe`, `ACTION_EMERGENCY + 6`, `MOVEMENT_FORCED`), to the first ring out with a clear spot within 30 yd of another living healer whose walk crosses no other source, the one nearest the healers' centre; none, or a holder, the nearest clear spot. A Hellfire's interrupter (Interrupts) ignores it for its first 2 s when its interrupt is a melee kick. Melee hold instead of reaching a target that is a source or within radius + 1 + the bot's melee range of one (`faction champions aoe guard multiplier`, on `reach melee` and `set behind target`) | The only two a bot can see, auras on the caster. Bladestorm slows him to 5.6 yd/s. Guide: Hellfire is "quickly interrupted", and class interrupts already fire on a channel of the bot's target. A ranged interrupt reaches from the clearance; power and the GCD stay out of the kicker's test, or it would flip every swing. A reach back in fights the dodge, and the bot stands up to its melee range on the target's near side. 25H: 4 of the first 7 deaths of a pull came mid-dodge 40-47 yd from the nearest healer, and the melee champions follow anyway, so the walk goes toward heals; standing in a Bladestorm costs more than leaving heal range |
| Grip chain | The player targets a champion and types `pull`. Bot tanks take it as the generic `pull my target`; `RaidTrialOfTheCrusaderStrategy::OnTick`, before triggers, turns a pull at a champion at the Faction Champions stage into the chain's request (`FactionChampionsTakePullRequest`, probe `fc.chain`, per instance, before the pull too) and ends the generic pull as `pull end` would, on every tank. No chain opens on its own: the other healers die in place (Kill order). The opener, the lowest-guid bot tank DK knowing Death Grip and in control, grips first (`faction champions chain grip`, `ACTION_RAID + 4.6`): from its own spot when in reach and LOS and, in combat, the grip lands the champion 12+ yd farther from the pack centre, else from 27 yd off it toward the grip post. Staging releases it meanwhile; its grip goes to nothing else (`faction champions chain grip reserve` zeroes the class `death grip`, and no peel). Death Grip over 3 s off its cooldown: it tells the master and drops the request. Then the bot DK (Death Grip), then the player from ~24 yd past the post. The request drops on the bot DK's grip, the champion's death, a new pull, 15 s with no tank grip, or 10 s after it with no bot DK grip | The user's chain, for the first healer: the bots' grips go out with no walk and the player's, last, carries it farthest. The generic DK pull walks to 29.5 yd, opens with Icy Touch when Death Grip isn't castable, refuses a target already in combat, and its multiplier zeroes everything else while armed; every bot tank answers it. 25H 2026-10-19: no DK tank opener existed, and Felesta spent her grips on peels. 25H 2026-10-21: no `pull` came after the first kill; the human's lone grip carried the next healer 22 yd, inside her friend's heal reach, and Liandra lived 120 s in the pack (98 → 11%). 25H 2026-10-22, with the after-kill chain: it carried her 12-28 yd, she walked back, and 1-2 champions died a pull |
| Death Grip | One DK bot holding no champion (lowest guid, in control, grip ready within 1 s), the chain's second link (`faction champions death grip`, `ACTION_RAID + 4.5`, latched in `fc.grip`). On an open pull it walks to the grip post until the DK tank's grip has landed the champion, then grips it; with no DK tank it grips the pull first. Without a pull only a kill target already pulled out (a grip someone made) while under 45 yd off the pack centre or within 40 yd of another champion healer. Never on a champion in the air (`EFFECT_MOTION_TYPE` in its controlled motion slot). Elected afresh each tick until one holds, a DK gripping from its own spot first, else the lowest within 50 yd of the target. The stand is the DK's own spot when the target is in reach (30 yd plus both combat reaches) and LOS and 12+ yd nearer the pack centre, else 27 yd off it on the floor bearing farthest from the pack centre (16 bearings, within 44 yd of `ARENA_CENTER`). A walk to a new stand clears the DK's older booking unless a dodge holds it. Released on the grip's cooldown, a new target, the target out of reach of its own spot or past 29 yd of a far stand, or 8 s. The rule `faction champions grip hold` zeroes reach, set behind, flank, formation, follow, flee and the spell movers for the holder; `faction champions kill squad wait` holds the melee meanwhile (Kill squad). All difficulties | Guide: grip a healer "as far from the other Champions as possible", chaining grips, and the next healer at ~20%; here the player calls each pull. Each grip carries it ~27 yd, a human's included; 45 yd puts it past its friends' heal reach plus the pack's spread. The grip also interrupts its cast. Melee joining before the grip stand in the pack. 25H 2026-10-18, the old 20 yd stand by the healers: it moved 17 yd, every champion stayed within 40 yd of it and another champion healer within 5.5-16 yd. The arena floor is a gameobject navprobe can't see; the Beasts spots are known to reach 44 yd. 25H 2026-10-18: the human gripped at 1.7 s and the bot 7.8 s later, `MoveTo` refusing its new walk 2.5 s on its own walk's COMBAT booking (`IsWaitingForLastMove`) before 31 yd of walking. 25H 2026-10-19: elected 0.2 s after the human's grip with the healer mid-jump 53 yd off, the bot walked 3.4-4.5 s to a stand derived from her mid-air spot; landed, she was 25 yd from its post |
| Grip guard | Guarded: the player's pull until 6 s after the bot DK's grip, a grip duty's target, and any champion in the air. A spell that roots, stuns, fears, confuses, charges or knocks back, itself or one trigger or linked spell down (`FactionChampionsSpellHoldsTarget`), is zeroed at a guarded champion, and an area or self-cast one (Frost Nova, Intimidation, Shockwave) while any guard is up (`faction champions grip guard multiplier`, which also zeroes the class `toggle pet spell` then). CC skips a guarded champion, peels skip it and take no area spell meanwhile. A pet's holding autocasts (Charge, Gnaw) go off while a guard is up and back on after it, its taunts off for the fight on heroic (`faction champions pet autocast`, `ACTION_RAID + 6`) | What cuts a grip (Mechanics). The player's link came 3-4 s after the bot DK's. Pet stuns are autocast only: a bot's cast of a pet spell flips its autocast, Spell Lock and Devour Magic aside, so off for the fight would lose them; unholy DKs and frost mages run `toggle pet spell` in combat |
| Snares | A melee champion (DK, Warrior, Rogue, Enhancement, Retribution) whose victim is a group member that is a healer or under 50%, or that is an escapee (20+ yd off the pack centre, victim not a tank or holder), not slowed, CC'd or immune to snares and not on its holder, gets one snarer (`faction champions snare`, `ACTION_RAID + 3.2`): healer victims first, then lowest victim health; the nearest non-healer bot, not on grip duty, not casting, with a ready snare in range (Hamstring or Piercing Howl, Chains of Ice, Wing Clip or Concussive Shot, Frost Shock) and LOS | Guide: keep melee champions "slowed or CC'd" and kite. A slowed champion takes longer to its next victim. Frost Trap and Earthbind Totem need it at the caster's feet (above), and Earthbind would take Tremor's earth slot from the anti-fear |
| Peel | A melee champion not CC'd, rooted or immune, within 10 yd of a victim that is neither a tank nor its holder, the Warrior champion with 2+ of ours (pets too) within 8 yd, or the danger champion anywhere while not disarmed, gets one peeler (`faction champions peel`, `ACTION_RAID + 3.3`, probe `fc.peel`): danger first, then Warrior, then healer victims, then lowest victim health. The danger champion (probe `fc.danger`): the first alive Rogue, DK, Retribution or Warrior champion with no holder that isn't the kill target, pulled or suspended, one not held (a player's fear, stun or confuse, or disarmed) first. The nearest bot not on grip or CC duty, not casting, with a ready spell and LOS: Disarm (Battle or Defensive Stance) or Dismantle in melee on one not disarmed, Hammer of Justice on the danger champion short of its third stun step, else Repentance, Blind, Freezing Arrow (judged by its trap's freeze, 60210), Death Coil, Frost Nova within 9 yd, Entangling Roots (not a healer or cat), Psychic Horror, or Death Grip from 12+ yd nearer the pack centre by a pack tank other than the chain's opener or a DK other than the chain's (lowest guid non-tank DK bot knowing it), never on the kill target. A CC a hit breaks, roots aside, only on a champion no group member or pet has as victim or selection but its holder, who stops meanwhile (Holders), and with no group DoT; none its DR group would make immune. Nothing that pops the trinket (`FactionChampionsSpellPopsTrinket`: fear, stun, confuse, a frost root or stun, one trigger down) on a held champion with its trinket up. No Nova or root within 12 yd of a pack tank; none on a guarded champion and no Nova while a guard is up (Grip guard) | Guide: Chains of Ice and Death Grip, Hamstring and Disarm, roots, Frost Nova. Disarm keeps Bladestorm off; Consecration and Death and Decay break a root at once; a grip back lands the champion on the pack, and the chain's DK keeps its cooldown. 25H 2026-10-18: Malithas (Retribution) killed five casters 54-78 yd off the tanks, Narrhok (Warrior) likewise, against 30 snares and no root, Nova or Disarm. The danger order is the measured damage (Mechanics); 25H 2026-10-20 the rogue was held 18.5 s and 9 s of ~2 min, disarmed once, most of it on the frontline where no peel looked. Kill-squad rogues stay on the kill target, so only bots already in reach peel. 25H 2026-10-21: danger stayed on the rogue all pull while the DK dealt the most (21%), held 17 s and 0.1 s; in four pulls, whichever melee champion was held, the next one topped the damage. 25H 2026-10-23: the three melee champions dealt 57-61% of the damage under 0-7 s of our CC each; Repentance, Blind, Freezing Arrow and Death Coil were added on the user's call. Frost Trap stays out (Known gaps) |
| External defensives | A group member holding none of the four or a physical immunity, with champions on it (not CC'd; melee within 8 yd of it, the rest at any range) or about to pick it at their next recount (Focus clock), gets one caster (`faction champions protect`, `ACTION_EMERGENCY + 4`): healers first, then holders, then lowest health; casters non-healers first, best spell first. Hand of Protection (no Forbearance, 30 yd) only for a healer or ranged bot that is no tank or holder, with four on it; Pain Suppression, Guardian Spirit (40 yd) and Hand of Sacrifice (30 yd, not self, caster 50%+) from four on a non-tank at any health, two at 60% or under, or a holder's one at 50% or under; one member per caster, in LOS. A non-tank with four on it that got none gets Divine Sacrifice from a free paladin bot with Divine Guardian, at 50%+, within Divine Guardian's radius of it, while no member carries Divine Guardian; cast on itself and cancelled once Divine Guardian is up (`FinishToCRaidCover`). The rule `faction champions divine sacrifice hold` zeroes the class `divine sacrifice` while live. Lay on Hands too: a member at 20% or under with any champion on it and none of the four, one per member in 15 s, from a free paladin bot other than it within 40 yd and in LOS, healers first, held until the cast goes out or 2 s; the rule `faction champions lay on hands guard` zeroes the class `lay on hands on party` (self stays) | The melee champions focus together (above) and kill a bot in 1-2 s on 25H. Hand of Protection leaves its holder suppressed, so they move on at once; on a caster it costs nothing, on a melee bot its swings and on a holder its champion. 25H 2026-10-19: every Hand went on melee DPS at 7-36 s and Pain Suppression and Hand of Sacrifice at 5-12 s, before anyone was low; Prayer (kill squad healer) took 66k from two melee champions with none left. The class `blessing of protection on party` names a spell the 3.3.5 client calls Hand of Protection and never casts, and `party member to protect` waits for 30% health. 25H 2026-10-20: each paladin's class node fired at one dying bot, three Lay on Hands on Totemist in 8 s, who died anyway. 25H 2026-10-21: the Hunter, Mage, Warlock and Shadow Priest together killed healers and ranged from full in 1-2 s, and no external went to any. The class raid cooldowns wait for `medium group heal setting`, but at most 2 members had 3+ champions on them at once: one Divine Sacrifice in two pulls, at 2:03. Divine Guardian (70940) is 20% less damage taken for the raid within 30 yd for 6 s; the 30% redirect would drain the paladin. 25H 2026-10-22: six of nine externals went to the frontline melee in the first 22 s, 1.2-1.8 s after their focus began, and none was left when the healers were focused from 0:41. 25H 2026-10-23, Hand of Protection still from one physical champion: all three went in the first 30 s, often against the Hunter on a healer at 63-100%, and 15-20 focus episodes after 1:00 had no cover |
| Personal defensives | A bot with 2+ champions as victim on it (a holder: 1+) whose health loss over the last 2 s (sampled every 250 ms), run on for 4 s, would leave it under 25%, or a non-tank with 4+ on it (a hit just past that floor), casts `GetToCPersonalDefensive`'s pick for that measured hit, physical when a physical champion is among them (`faction champions personal defensive`, `ACTION_EMERGENCY + 8.4`, interrupting its cast; `toc.personal`): a non-tank paladin's Divine Shield first, a mage's Ice Block with 4+ on it, neither for a holder (either drops it from every champion; ready on the GCD or mid-cast, only a cooldown or its excluded caster aura stops it), the champions about to pick it counted (Focus clock), then the weakest ready row (Divine Protection, Shamanistic Rage, Barkskin, Icebound Fortitude…), for a physical hit then Evasion, Deterrence or Pain Suppression on itself; none while one runs; on a lethal one Dispersion, the class immunity or a healthstone at 70% or under. The rule `faction champions avenging wrath hold` zeroes `avenging wrath` for tank, healer and holder paladin bots; `faction champions self lay on hands hold` zeroes the class `lay on hands` for a paladin whose own Divine Shield is ready (known, off cooldown, no Forbearance 25771 or 61988) | The class nodes wait for critical health, but focus takes 40-60% in 1-2 s, from full before the log shows a loss. Divine Protection's Forbearance and Avenging Wrath's 30 s marker both lock out Divine Shield. 25H 2026-10-20: Divine Protection at 20-21%, Shamanistic Rage at 13-27%, Enraged Regeneration at 1%, no Divine Shield from Justice or Holylight. Later: the trigger fired at 52-89%, but 15/24 and 13/21 picks were a healthstone (the tank rows hold nothing physical for rogues, hunters or Disc priests); Justice's Avenging Wrath and Holylight's early Divine Protection left both without a bubble when focused. 25H 2026-10-22: `Castable` failed Divine Shield on the GCD (Divine Protection is off it), so Justice (0:03, just after a Hand of Sacrifice) and Holylight (1:30, at 100%) took Divine Protection and lost the bubble; three healthstones went at 79-90%. 25H 2026-10-23: Holylight's class self Lay on Hands at 23%, no champion on him, left Forbearance, and the focus 15 s later found no bubble |
| Focus clock | Each tick, per champion in combat, its threat list against the last tick's: 2+ player entries moved over 1% (and 100,000) is a recount. From 1 s before its earliest next one (8.75 s) to 250 ms past the latest (9.25 s), a champion not under lost control is predicted to pick the top score (the recount's formula, over players with threat on it, online and not immune to physical), keeping its victim unless past 110% (new victim in its melee range) or 130%, counted only 10% clear of the runner-up. External defensives and Personal defensives count those as on the member. `fc.repick` notes 3+ recounts within 500 ms; `fc.nextfocus` holds the member 4+ are about to pick | A focus kills a bot from full in 3-4 s, and the externals and personals came 1.2-1.8 s after it began (25H 2026-10-22). The recount reads only the threat list, health, distance and armour, all visible to the bots; an immunity up before it leaves the bot suppressed, so they pick another |
| Battle res | The rule `faction champions battle res guard` zeroes the class `rebirth` while live; `faction champions battle res` (`ACTION_RAID + 4.2`) takes corpses (Corpse state, no res pending, no Soulstone) with at most one champion within 8 yd, healers, then tanks, never a DPS; each gets the nearest non-tank druid bot, not casting, Rebirth ready and affordable, within 30 yd and in LOS, skipping a corpse a res is already being cast on. Caster form first | The class target lists the master first, then healers, and takes a corpse inside the pack. 25H: every Rebirth fired within 1 s of a melee death, mostly 4-8 champions round the corpse, and each died again in 3-25 s. 25H 2026-10-20: 4 of 6 went to frontline DPS who died again in 7-28 s, and Holylight, Prayer and Tree then stayed dead; later, with one Rebirth kept back, all three still went to melee DPS, dead again in 13-32 s |
| Heroic taunts | The rule `faction champions heroic taunt guard` zeroes the `Taunt` family on heroic; `faction champions pet autocast` turns a pet's taunt or threat autocast off (Grip guard), which the non-combat `toggle pet spell` turns back on | Immune to taunt (above). The family holds the generic Death Grip, which would spend the duty's cooldown. Hunter pet Growl is threat only, which a pet never turns into a pick, and costs 15 focus a cast. 25H: 13 Righteous Defense and 44-46 pet Growls a pull |
| Start | Staging (`LayoutChampions`): bot tanks as forced duties on the line from the landing centroid to `ARENA_CENTER`, at the first 0.5 yd step 25.5+ yd from every landing slot (~30 yd out), extras 3 yd to alternate sides; non-tank DK bots knowing Death Grip 54 yd out on it (`grip`, the grip post, extras alike); everyone else 28+ yd from every slot and 3+ yd farther from each than the nearest tank stand. So a player starts the fight, and every champion's first pick is a tank | 23 yd aggro (above), plus the 2.5 yd a parked bot drifts before it walks back. 25H 2026-10-18: tanks staged on the centroid started the fight at the release, before the human could open. The grip post sits ~24 yd past the tank line, where the DK tank's grip drops a healer, so the bot DK grips with no walk; the player stands ~24 yd further, ~78 yd out, still floor: raid samples reach 85 yd on that line |
| Fire resistance | One bot paladin holds Fire Resistance Aura (README, Resistance) | The user's call. 25H 2026-10-19: fire was 7-8% of champion damage on the raid (Hunter 67985, Warlock 68150 and 68144, Enhancement 65974), against arcane 9-11%, frost 8-9% and shadow 5-10% |
| Holders | Each melee champion (DK, Warrior, Rogue, Retribution, Enhancement, dealt in that order) gets one holder, latched per instance (`FactionChampionsHoldOf`, probe `fc.hold`, holder → champion): plate DPS first, then cats, then bot tanks, the nearest within a tier; the chain DK takes the DK once the chain closes (the DK waits for it while one is open, and neither chain DK is dealt meanwhile), and on the Warrior a warrior without Mortal Strike goes first. Kept until the holder dies or turns ineligible, or the champion dies, leaves combat or turns kill target. The holder attacks it wherever it is (`faction champions hold`, `ACTION_RAID + 2`); while our breakable CC holds it or a breakable peel is aimed at it (`FactionChampionsCcProtected`) it stops its own and its pet's attack and clears its target, assist still vetoed. A warrior holder Disarms it in Battle or Defensive Stance and Hamstrings it while it's on someone else; a DK holder, while it's on someone else, Death Grips it in from 8-30 yd, else Chains of Ice (`faction champions hold control`, `ACTION_RAID + 2.2`): none pops the trinket. The rule `faction champions holder hold` zeroes formation, follow, flee, run away and random moves | The user's calls (2026-10-23): one controller per melee champion, replacing the frontline, then no spot or walk. The pick: plate DPS in melee range scores ~1.3 against a tank's ~0.64, and nobody else with threat on it comes near, since only the holder hits it and healers stand 30+ yd off (a hurt cloth healer outscores the holder from ~25 yd). Disarm leaves the trinket alone; the Arms warrior keeps Mortal Strike on the kill target. 25H 2026-10-22, the frontline before it: 5+ champions on one plate DPS in 72-74% of samples, every frontline DPS dead by 0:47-1:12, then the melee champions went to the healers; the Warrior Charged the backline 19 of 20 times. 25H 2026-10-23, walked to spots 30+ yd off the healers: the champion had its holder as victim a median 21-23% of the hold (0-61%), most holders died, and the healers' stands fled the held champions all pull (Stay put) |
| Release | Bot tanks staged nearest take every champion's first pick | The release pick is the nearest player, and his hits seed the champions before the 2 s recount. 25H 2026-10-18, staged on the centroid: all 10 first picks on tanks, then at the 2 s recount 8 took melee bots 20-28 yd off holding healing threat (Healing Stream, Divine Storm) |
| Stay put | Healers and ranged bots, no tank or holder, keep where they stand: the rule `faction champions stay put` zeroes set behind, flank, formation, follow, flee, run away, random moves, charges, blinks and disengage. Only a dodge (Champion AoE), a Nitro run, a CC walk, the Cyclone stand, `reach spell` or `reach party member to heal` with the target out of range, or a step-off moves one. Step-off: a healer not casting, rooted or stunned with a melee champion not under lost control within 8 yd walks 10 yd away from them (±45°, ±90° to stay on the floor and clear of Bladestorm and Hellfire; `faction champions step off`, `ACTION_RAID + 2.4`), latched to arrival within 1.5 yd or 2.5 s, then none for 3 s | The user's calls (2026-10-23): a moving bot neither heals nor casts, and a healer steps off melee. 25H 2026-10-23, healers stood 26-30+ yd off the held champions and ranged on slots off the kill target: healers moved 56-84% of their live samples (61-65% on the stand alone), ranged 27-29% on theirs, and the raid healed 5.1-5.8k/s against 10.6-11.2k/s taken; the 25N kill's healers moved 8-16%. The 3 s rest keeps a champion that follows from turning the step into a kite |
| Cyclone stand | A ranged DPS druid bot knowing Cyclone, once the kill target has been out and a champion healer is left in the pack, stands on the 20, 26 or 30 yd ring round the kill target (every 15°, floor and clearances) within Cyclone reach (its range with talents plus both combat reaches, less 2) of the pack healer nearest the kill target (probe `fc.cyclone`): one still in reach after the healer drifts 6 yd first, then one in reach, each farthest from the pack centre then nearest the druid; with none in reach, the spot nearest that healer. Kept while the kill target and healer stay the same, the healer within that reach of it, the kill target within 36 yd and the spot clear (`faction champions cyclone stand`, `ACTION_RAID + 1.9`). It takes no CC duty it would walk to meanwhile | The user's call: hit the pulled healer and Cyclone the healer in the pack from one spot. 25H 2026-10-20: its slot past the kill target sat 35-60 yd from the pack healers, so every Cyclone was a walk in and out. Later, farthest-from-the-pack first put the stand at the edge of reach: both in reach 28% and 50%, the healer at a median 26 and 22 yd against 23. 30 yd is inside Wrath's 33 with Nature's Reach. 25H 2026-10-23, re-picked on every 6 yd drift: the druid walked 24-29% of the pull |
| Kill squad | Every focus bot: ranged, melee holding no champion, tanks included. The rule `faction champions kill squad wait` zeroes `reach melee` and charges for those melee while the player's pull is open or a grip duty is on a kill target still in the pack; ranged then hit it from their slots | The pulled healer takes the latch when it lands, so melee going in earlier stand in the pack it leaves. With no later chains the other healers die where they stand (Kill order). 25H 2026-10-22, the old wait for each healer to be pulled out: the squad held 94 s at 13 yd while the next chain failed; 99 → 87% |
| Nitro Boosts | Only to escape: a bot with them (enchant 3606) that is no tank or holder, not rooted, stunned or on a CC duty, at 30% health or more, with 3+ champions as victim on it (not CC'd) or named by `fc.nextfocus`, boosts 25 yd away from them (from those within 30 yd when none is on it yet; ±45°, ±90° to stay on the floor), latched for the boost (`faction champions chased`, `ACTION_RAID + 2.5`, `MOVEMENT_FORCED`). Each run notes `fc.nitro` | The user's call (2026-10-23): no boost to pre-position, only when several champions start on a bot. 25H 2026-10-22: 9 of 30 uses were the frontline's walk in at the pull; chased runs at full health shed one melee chaser (the bot lived 20-80 s), late ones at low health died within 8 s (10 of 21) |
| Opener | Heroic: the bot, not a healer or the grip DK and in control, whose ready instant AoE fear or Frost Nova (Intimidating Shout, Psychic Scream, Hungering Cold, Howl of Terror once talented instant, Frost Nova) reaches the most champions with their PvP trinket up, within its radius − 1, three or more, casts it where it stands, at the nearest such champion for Intimidating Shout, else on itself (`faction champions opener`, `ACTION_RAID + 4.4`, probe `fc.opener`). The holder keeps a tie. Never with a grip-guarded champion within its radius + 1. It comes back whenever trinkets are up again | Guide: trinket bait at the pull. Feared and frozen both pass `IsCCed`, so each champion caught trinkets at once and the next CC holds; trinkets read off each champion's own cooldown (`HasSpellCooldown`), so it re-arms on the 2 min trinket. A fear or freeze ends a grip where it catches the champion. 25H 2026-10-22: no AoE fear went out, the old opener's walk to the pack stalled 2.8 s and its 2.5-8 s window closed, and every trinket went on single-target CC: Polymorph 12, Cyclone 7, Hammer of Justice 7, pet Intimidation 5, Fear 5 |
| Totems | Each champion totem (alive, `IsTotem`, owned by a champion, attackable) gets one killer, latched in `fc.totem`: the nearest eligible group bot holding no totem (`GetExactDist2d`, ties by guid), eligible meaning alive, on the map, in control, not a healer and within 30 yd. The hold ends once the totem stops counting or its bot turns ineligible. `faction champions kill totem` (`ACTION_RAID + 2.7f`) attacks it directly; the rule `faction champions totem guard multiplier` blocks focus and the physical switch for the holder | Guide: "a melee player dedicated to killing his totems". Grounding eats a Cyclone or Counterspell, Windfury hastes the champions' melee, Tremor undoes fears (above). The raid's own totems share the entries. The settled duty returns false, so without the rule focus drags the bot back to the kill target. No trace yet has an Enhancement Shaman |
| Treants | No node; `--adds` measures them | No trace has a Balance Druid yet, the guide doesn't mention them, and they're three 30 s adds every 3 min |

## Known gaps

- A tank holds only with no plate DPS or cat free, and loses its champion to any healer within ~45 yd
  (Mechanics).
- Holders hold ~21%: the pick ranks cloth within ~30 yd and leather within ~20 yd over a plate holder
  in melee range (Mechanics); 25H 2026-10-23, most holders died.
- Healing follows the generic lowest-health pick: holders come first for externals and personals
  only.
- `fc.hold`, `fc.nitro` and the trinket opener are unscored until a trace carries them.
- The grip guard keeps every stun and root, Gnaw and Intimidation included, off the player's pull for
  6 s after the bot DK's grip, heals or not.
- A kick held for the last 600 ms that a lag spike misses lets the heal through.
- The reader's escapee seconds and `--ball` pack still read off the landing centroid, not the moving
  pack centre.
- Kill squad melee hold wherever they stand between pulls, the pack included if they were there.
- Rebirth brings a healer back on low health, the top score at the next recount: all three on 25H
  2026-10-22 died again in 0-14 s.
- The humans and their pets, a Fire Elemental and healers with a caster champion selected still keep
  Polymorph, Fear and Hex off the casters.
- Ranged reach to 28.5 yd of their target, the pack included; 25H 2026-10-18 that put them 17-24 yd
  off the pack and 44-79% of champion targets on them.
- The other champion healers walk back into heal reach of a pulled one in 6-10 s, behind the raid
  members they chase (25H 2026-10-23); only CC keeps them off it.
- Frost Trap is never cast: it drops at the hunter's feet (only Explosive Trap has a Trap Launcher
  here, custom 425777-425782) and shares its 30 s cooldown with Freezing Arrow. Wyvern Sting, Gouge,
  Maim and Slow are never cast either.
- `fc.nextfocus` is unscored until a trace carries it.
- No bot grips a champion out but on the player's `pull`. Without a DK tank the bot DK opens and
  nobody grips second, so without a human DK the champion stops ~27 yd off, inside its friends' heal
  reach. Only a second DK bot grips an escapee back, and without a DK bot nothing grips.
- With holders on the melee champions only, nothing but CC holds the casters.
- A Fury warrior bot never Disarms: Berserker Stance can't cast it. The Warrior goes to a warrior without
  Mortal Strike first, so with one Fury and one Arms warrior its holder never disarms it.
- Casters take almost anyone within 50-57 yd, the healers and the kill target's melee included.
- A healer whose heal target is past its reach stalls until someone else is lower (`PartyMemberToHeal`
  adds 30 past HealDistance).
- Kiting is one boosted 25 yd run per Nitro Boosts cooldown; without the boots nothing walks off but
  the generic 5 yd flee.
- With no eligible champion left, physical attackers keep swinging at the Hand of Protection target.
- Stage 6 closes the gate at the kill, before any cleanup runs: no latch is cleared (no `reset`
  switch, no `kill` or `physical` `0`), and `ccaura`, `totem`, `grip` and `peel` keep their last
  values.
- The melee on the kill target take any instant crowd AoE a champion brings to them (Fan of Knives,
  Divine Storm, Arcane Explosion, Frost Nova, Psychic Scream, Intimidating Shout); the stands are only
  5-7 yd apart.
- The mage champion's Arcane Explosion, 17% of champion damage on 25N, sits under a C-tier kill
  order.
- The externals run out against the focus: three paladins' Hands every 5 min and a Pain
  Suppression every 3 min, against a new victim every 1-2 s once one dies.
- Under 4 champions the personal defensive reads 2 s of health loss, so a burst from full inside one
  sample goes up late.
- Holder Retribution paladins never cast Avenging Wrath.
- A champion healer knocked 10+ yd in 1 s (Typhoon, Thunderstorm) counts as gripped.
- The danger order is fixed; it only moves past one held at the time.
- No bot walks to peel the danger champion, so a rogue loose among the ranged gets only their stuns and
  Psychic Horror.
- The kill squad gets no healer of its own.
- `--pack` reads stalls only with 10+ bots on the kill target; on 25H the long flat spells came as
  the attackers fell below that.
- Treants are unanswered.
- Polymorph, Fear and Hex wait for a champion clear of our attackers and DoTs; on heroic the trinket
  breaks the first CC on each champion every 2 min.
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
| `switch` | Why the latch moved: `first` (from empty), `dead` (died, or left combat), `immune`, `back` (the suspended one returned), `skull` (a player's skull took it), `pulled` (a healer gripped out of the pack), `reset` (not live, a wipe). An event, one row per switch |
| `cc` | A CC duty's champion, keyed on the bot: one cast, `0` once it lands, 3 s pass or the champion stops being a candidate |
| `physical` | The physical switch target, `0` when none. Per instance, restated each pull |
| `ccaura` | Keyed on the champion: the spell of a player's fear, stun or confuse aura on it (Cyclone, Polymorph, Banish), `0` when none. The only record of CC on a champion; roots aren't in it |
| `totem` | A totem killer's totem, keyed on the bot, `0` on release |
| `grip` | The grip duty's champion, keyed on the DK, `0` on release |
| `peel` | A peel duty's melee champion, keyed on the bot, `0` on release |
| `hold` | A holder's melee champion, keyed on the bot, `0` on release |
| `opener` | The opener's spell, keyed on the bot while it reaches 3+ champions with their trinket up, `0` on release |
| `nitro` | An escape run on Nitro Boosts, on the bot: `focus N` (N champions on it) or `next` (`fc.nextfocus`). An event |
| `kick` | A Hellfire interrupter's warlock, keyed on the bot, `0` on release |
| `chain` | The player's pull in flight, `0` once it closes. Per instance; one made before the engage shows from the trace's opening |
| `cyclone` | The pack healer a Cyclone stand covers, keyed on the druid, `0` on release |
| `danger` | The danger champion, `0` when none. Per instance |

Bladestorm and Hellfire write `haz` circles (aura id, source, radius, 1 s ttl), one per source a
second at most. The duties (counterspell, dispel, purge, Mass Dispel, snare, externals, battle res)
have no probe: their nodes are in `act`, their casts in `cast`.

`tools/botobs/bosses/faction_champions.py` reads the rest:

- banner: the lineup and the `fc.` keys the pull never wrote;
- `--kill`: each kill hold with its reason, length, health at start and end, whether it died, the
  share of non-healer bot samples on it, and for a `pulled` one the raid Death Grip on it in the 3 s
  before, or `no grip`;
- `--cc`: per bot its `fc.cc` duties, how many its own cast went out in, their median length and
  spells; per champion, `fc.ccaura` time by spell and fear uptime (the share of its live samples from
  the pull at moveGen 9, fleeing) and the share a raid member or pet targeted it; per bot its Polymorph, Fear, Cyclone, Banish and Entangling Roots
  casts, started and landed, on the assigned target and on the kill target; landed per 10 s. A Fear
  lands when its target turns to fleeing within the cast time + 1 s, a Cyclone or Polymorph when
  `fc.ccaura` turns to that spell (`n/a` without the key, and for Roots), each turn landing one
  cast. A CC broken between two samples shows in neither, so landed is a lower bound: 52 of 67
  Fears on 25N. Per caster, its `fc.ccaura` spans that ended within 400 ms of its next cast of the
  spell landing on another champion, over 1 s short of its longest hold of that spell (29 of 39 and 39
  of 43 Cyclones by the 400 ms rule alone, 25H 2026-10-20; 4 of 13 with both, 2026-10-22);
- `--heals`: champion cast-time heals, how many a raid or pet interrupt on the caster followed
  inside the cast, kill target against the rest, by interrupter class; how far into the cast the
  followed ones were kicked (median) and how many in the last 600 ms; each Hellfire and Tranquility
  with its first interrupt within 10 s; `fc.kick` holds;
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
  caster class; each champion Bloodlust, Heroism, Avenging Wrath and Hand of Freedom and the raid
  offensive dispel within 8 s on it (on any champion for a lust); the duty's `OK` rows;
- `--hop`: champion Hand of Protection, Divine Shield and Ice Block casts, on the kill target or
  not, the first Mass Dispel inside each and how many, and for the two suspending shields on the
  kill target the first `back` to that champion up to 2 s past its end, `early` when before it; each
  `fc.physical` hold, its length and the share of physical bot samples (tank and melee roles,
  hunters) on it;
- `--aoe`: per crowd-AoE spell, casts, raid victims per cast (its `dmg` rows from 50 ms before the
  cast row, since an instant's land 0-6 ms ahead of it; aura applies for the fears), hits per victim
  for Bladestorm and Hellfire; the bots' own area and bounce casts from the pull, by spell; the avoid
  node's `OK` rows;
- `--adds`: champion totem and Force of Nature casts; each champion totem and treant with its first
  and last snapshot, and treant damage to the raid with its victims; each `fc.totem` hold, its length
  and whether the totem left the snapshots inside it; the duty's `OK` rows;
- `--pack`: per role, mean champions within 8 yd (centre to centre); melee deaths in the first
  60 s; median melee to nearest healer; time to the first kill; the longest span with 10+ bots on
  the kill target and its health moving under 3 points, a stall at 20 s; each raid Death Grip on a
  champion with the other champions within 8 yd before and a second after, how far it moved and
  the time since the raid's grip before it, a human's (`*`) included; `fc.grip` holds and how many
  ended in the bot's grip;
- `--chain`: per `fc.chain` pull, `chain N s after a kill` when it opened within 20 s of a `dead` switch, its Death Grips on that champion until the next pull or 20 s past
  its close, with the gap from the pull or the link before, the carry and the distance off the landing
  centroid a second after, and whether they came DK tank, bot DK, player; per link `no pull` when the
  champion cast no 57604 with it, and `cut by` each raid or pet cast that holds it within 1.2 s (a
  Frost Nova when its caster stood within 12 yd); bot grips outside any pull;
- `--peel`: snares on champions by spell and class, split by whether the champion's victim was a
  healer or under 50%; peels by spell and class, on an `fc.peel` duty or not (Death Grip, Frost
  Nova and Freezing Arrow only on one); per Warrior champion its Bladestorms against the raid's Disarm, Dismantle and
  Psychic Horror casts; each external on the raid, Lay on Hands and Divine Sacrifice included, the champions on its
  target at the cast (physical + caster), whether it died within 10 s, and how many Divine Guardian
  reached; Aura Mastery, Divine Hymn, Tranquility and Anti-Magic Zone casts; each Rebirth with the champions within 8 yd of where its target died and when it died
  again; taunts on champions by spell, bot or human;
- `--damage`: damage to the raid per champion, its pets counted in, with share, kills, top spells and
  victims by role; per melee champion its `fc.ccaura` time and the raid's disarm, stun and CC casts on
  it, the `fc.danger` ones marked, and how long each was danger;
- `--personal`: each bot's personal defensives with its health at the cast, the `toc.personal` picks,
  per death the first one in the 15 s before it, and healthstones over 70%;
- `--focus`: the share of samples by the most champions on one raider; each focus (4+ on one raider)
  with its peak, health, death and the externals and own immunities from 1 s before it; per
  `fc.repick` the `fc.nextfocus` member against the one with the most champions 1.5 s later;
- `--ball`: the raid's first cast on a champion, human or bot, and each champion's first pick by
  role; per 10 s from the pull, champion targets by role and the median ranged and healer distance
  to the pack centre (the champions within 25 yd of their first-snapshot centroid, the kill target left
  out); per kill hold, its median distance to the pack centre and the share of samples with no other
  champion healer within 40 yd; escapee seconds per champion (past 20 yd of the landing centroid, victim
  not a tank or a bot holding an `fc.hold` at that sample);
- `--hold`: the holders; per `fc.hold` span the holder, champion, start and length, the share of the
  champion's live samples on its holder, its median distance to the nearest living raid healer, whether
  the holder died inside the span and the champion by its end, and its other victims with seconds each;
  then the melee champions never held and the median on-holder share;
- `--trinket`: `fc.opener` holds, each with whether its holder cast an opener spell inside it; per
  champion PvP trinket cast its trigger, read off timing (no creature aura is recorded): the latest
  opener AoE in the 1.5 s before whose caster stood within 12 yd of the champion (`aoe`), else the latest
  raid or pet fear, stun or confuse cast on it, how long before, and whether its caster held
  `fc.opener`; a tally by trigger and the share burned by opener AoE;
- `--stands`: per bot with a stand move (`faction champions ball stand` in older traces, `cyclone
  stand`, `step off`), those moves, jumps (targets 5+ yd off the one before), and snapshot path against
  net distance from its first one; per healer its moving and casting share of its live samples; per
  healer and ranged bot its moving share by the mover of its last accepted move row within 4 s; the
  raid's effective healing (heal rows less overheal) and damage taken per second, healing per healer; per Cyclone caster its Cyclones against its
  casts on the kill target, and over its `fc.cyclone` holds the median distance to the kill target and
  the covered healer and the share with both in reach (33 and 23 yd); each Nitro Boosts use with its
  `fc.nitro` reason (`walk` when the bot noted none within 1 s, a boost no rule asks for), health, and
  the move row nearest it with its length, and a tally by reason;
- `--vetoes`: the multipliers' and rules' vetoes by action.

Still invisible: creature deaths, heals and auras but `fc.ccaura`'s. A champion died when it left
the snapshots mid-fight, or was the kill target when the trace closed on a kill; heals landed and
heals stopped are upper bounds, both read off cast starts. Divine Shield, Ice Block, Cyclone and
Banish on a champion show as an `immune` switch, Cyclone and Banish also on `fc.ccaura`, a disarm
not at all, the shields and Hand of Protection
also as the champion's cast; a purge or Mass Dispel shows as its own cast, never as the aura it
took. Exhaustion or Sated applied before the trace shows only as its removal; a fresh lust writes
the caster's copy as a removal with time left, then the member's apply. Mass Dispel is cast at a
point, so its row names no target. A CC removed with time left was dispelled, broken by damage, or
its holder died. A victim is a player: `dmg` covers no pet, while the champions' 3+ check counts
pets.
