# w3b-fc-defence — Faction Champions: dispels, purges, avoidance

Wave 4, after `w3a-fc-offence` merged. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Guide: `faction-champions-master-strategy-guide-toc-25/`.

## Scope

1. **Dispel friendly CC**: Polymorph, Hex, Cyclone, Repentance, Fear, and whatever else the script
   casts. Class dispel nodes sit below `ACTION_RAID`, and `PartyMemberToDispel` returns one target
   per tick (`docs/engine/pitfalls.md`); decide whether a raid-priority dispel node is needed.
2. **Offensive dispels.**
   - Purge or spellsteal champion buffs.
   - Divine Shield can't be purged, so switch off it.
   - Switch off physical attacks on a Hand of Protection target.
3. **Avoidance**: Bladestorm, Hellfire, Fan of Knives, and ground AoE (Blizzard, Consecration, Death
   and Decay, and whatever the script actually casts). A mechanic the bots can't see is answered by
   spreading in advance.
4. Check the 10- and 25-man rosters, and every champion ability the `w3a` investigator did not
   cover.
5. `fc.` probes; extend `faction_champions.py` and its test.

## Owns

The `FactionChampions` defence files (split from offence in `w3a`); `faction-champions.md`; the
reader and its test.

## Verified facts

- Script mechanics, ids, radii, timers and the 10/25 roster: `faction-champions.md`, "What the
  defence plays against" (written by the investigator). No ToC trace exists on disk, so nothing here
  is measured; the reader sections below exist to measure it.
- Scope item 3's ground AoE doesn't exist here: the script casts no Blizzard, Consecration or Death
  and Decay. Its only ground effect is the Hunter's Frost Trap 65880, a slow.
- **Class dispels today.** From the `cure` strategy (every priest, mage, shaman, paladin, balance and
  resto druid): priest `dispel magic on party` 40, paladin `cleanse magic on party` 51, mage
  `remove curse on party` 40, druid 57, resto shaman `cleanse spirit curse on party` 52.
  `party member to dispel` returns the first member holding any aura of the type (master, healers,
  tanks, others; own subgroup first), DoTs included, so every dispeller chases that one member and
  nothing prefers CC over a DoT or skips Unstable Affliction.
- **Class purges today.** Shaman `purge` 50, mage `spellsteal` 40, warlock `devour magic purge` 50,
  hunter `tranquilizing shot magic` 61 fire on any dispellable positive magic aura on the bot's
  current target (`TargetAuraDispelTrigger`, checked every 1 s). Thorns alone keeps that true on the
  kill target, so every mage spends every GCD on 20%-of-base-mana Spellsteals. Priests have no
  offensive dispel node and nothing casts Mass Dispel outside SWP and Iron Assembly.
- `PlayerbotAI::CanCastSpell` refuses a target immune to the spell (`IsImmunedToSpell`), so no dispel
  is wasted on Divine Shield or Ice Block. `CastSpell` on a dest spell (Mass Dispel) aims at the unit's
  position.
- Class interrupt nodes fire on any non-melee cast of the bot's current target, channels included, so
  melee on a Hellfire warlock already kick it.
- Hex sets `UNIT_FLAG_SILENCED` (aura 60) without `UNIT_STATE_LOST_CONTROL`; a readiness test needs
  both flags. `AttackAction::Attack` never moves the bot.
- No coordinate is proposed; the dodge derives its spot live (arena floor is GO 195527, see
  `README.md`), so no navprobe run.

## Task list

Three groups code against these contracts; the integrate step compiles them together.

- **Helper API** (group 1 writes, group 2 calls), `namespace TrialOfTheCrusaderHelpers`.
  - Additive, `Util/ToCHelpers_FactionChampions.h`, each checking `ToCEncounterIsLive` before
    `Refresh`, like `FactionChampionsKillTarget`:
    `Unit* FactionChampionsNextTarget(PlayerbotAI*, std::function<bool(Unit*)> const& accept);` the
    first alive, in-combat champion in kill order that is not the kill target, not the suspended one,
    not any `fc.cc` assignment, and that `accept` takes; `Unit* FactionChampionsSuspendedTarget(PlayerbotAI*);`
  - `Util/ToCHelpers_FactionChampionsDefence.h` (new): spell constants (below), and
    `Unit* FactionChampionsDispelCcTarget(PlayerbotAI*, char const*& spell);`
    `Unit* FactionChampionsMassDispelTarget(PlayerbotAI*);`
    `Unit* FactionChampionsPurgeTarget(PlayerbotAI*, char const*& spell);`
    `Unit* FactionChampionsPhysicalSwitchTarget(PlayerbotAI*);`
    `bool FactionChampionsInAoe(PlayerbotAI*);`
    `std::vector<EncounterHelpers::HazardCircle> FactionChampionsAoeClearances(PlayerbotAI*);`
    `bool FactionChampionsReachIntoAoe(PlayerbotAI*);`
    `bool FactionChampionsDispelBackfires(Unit* target);`
    Each returns null/false for any bot the rule does not name, and unless
    `ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions)`.
- **Probes** (group 1 declares, group 3 reads): `fc.physical` `ObsValue<ObjectGuid>` (the physical
  switch target, `0` when none; written on every live refresh so it restates each pull), declared on
  one line with its literal. Bladestorm and Hellfire as `RaidObs::NoteHazardCircle` rows.
- **Node and multiplier names** (group 2 registers, group 3 reads), trigger and action alike:
  `faction champions avoid aoe` (`ACTION_EMERGENCY + 6`), `faction champions mass dispel`
  (`ACTION_RAID + 5`), `faction champions dispel cc` (`ACTION_RAID + 3.5f`),
  `faction champions physical switch` (`ACTION_RAID + 2.5f`), `faction champions purge kill target`
  (`ACTION_RAID`). None ties an offence node (+4, +3, +2, +1, `ACTION_INTERRUPT + 3`). Multipliers
  `faction champions aoe guard multiplier`, `faction champions physical switch guard multiplier`,
  `faction champions dispel guard multiplier`, `faction champions purge guard multiplier`.

### Group 1 — helpers and state

Files: `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampionsDefence.h` (new),
`src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampionsDefence.cpp` (new),
`src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h`,
`src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.cpp` (the two accessors only).

1. **Offence accessors** above, reading the refreshed `FactionChampionsState` (`champions` is already
   in kill order; skip `killTarget`, `suspended` and every `ccTargets` value).
2. **State.** A `RaidInstanceState<FactionChampionsDefenceState>` in the new `.cpp`, refreshed at most
   once per instance per `getMSTime()` ms by the first asker, only while live (a refresh after the
   kill would write into the Twins trace). Holds the `fc.physical` latch, the hazard sources, the CC
   dispel assignment (dispeller → member and spell), the Mass Dispel and purge duties, and the last
   hazard note per source.
3. **Spell constants**, plain `constexpr uint32 SPELL_*`, remapped at the call with
   `sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)` where the row exists: Unstable Affliction
   65812, Hellfire 65816, Hand of Freedom 68757, Power Word: Shield 66099, Renew 66177, Riptide 66053,
   Rejuvenation 66065, Lifebloom 66093, Regrowth 66067 remap; Polymorph 65801, Fear 65809, Psychic
   Scream 65543, Repentance 66008, Hammer of Justice 66613 and 66007, Silence 65542, Strangulate
   66018, Hex 66054, Wyvern Sting 65877, Bladestorm 65947, Hand of Protection 66009, Divine Shield
   66010, Earth Shield 66063, Heroism 65983, Bloodlust 65980, Avenging Wrath 66011, Barkskin 65860,
   Mass Dispel 32375 do not. `pblint.py --spell-difficulty` stays clean.
4. **CC dispel duty** (`FactionChampionsDispelCcTarget`).
   - Counted CC, with at least 2000 ms left: Polymorph, Fear, Psychic Scream, Repentance, both Hammers
     of Justice (Magic), Hex (Curse), Wyvern Sting (Poison) on anyone; Silence and Strangulate (Magic)
     on healers only.
   - CC'd members: group players, humans included, alive, on the map, holding a counted aura. Order:
     healers (`PlayerbotAI::IsHeal`) first, then guid. One assignment per member per refresh, type
     Magic before Curse before Poison.
   - Dispellers: group bots, alive, on the map, neither `UNIT_STATE_LOST_CONTROL` nor
     `UNIT_FLAG_SILENCED`, not `IsNonMeleeSpellCast(false)`, within 30 yd and in LOS of the member,
     the spell off cooldown (a school lockout) and mana at least its `CalcPowerCost`. Spell per type
     by `HasSpell`: Magic → priest `dispel magic` (988, 527), paladin `cleanse` (4987); Curse → mage
     or druid `remove curse` (475, 2782), shaman `cleanse spirit` (51886); Poison → paladin
     `cleanse`, druid `abolish poison` (2893) else `cure poison` (8946), shaman `cleanse spirit`
     else `cure toxins` (526). Ordered non-healers first, then guid; each takes one member.
   - A spell that removes Magic (`dispel magic`, `cleanse`) skips a member for whom
     `FactionChampionsDispelBackfires` holds.
5. **Mass Dispel duty** (`FactionChampionsMassDispelTarget`). Target: the kill target carrying Hand
   of Protection or Divine Shield with at least 3000 ms left, else the suspended champion carrying
   Divine Shield with at least 3000 ms left, and no group member within 15 yd of it (`GetExactDist`)
   carrying Unstable Affliction. Duty: priest bots, alive, on the map, knowing 32375 off cooldown,
   neither lost control nor silenced, mana at least its cost; non-healers first, then guid; the
   first within 30 yd of the target by `IsWithinDist3d(target, 30)` (the core's point-cast range)
   and in LOS.
6. **Purge duty** (`FactionChampionsPurgeTarget`). Worth purging: the kill target carries Hand of
   Protection, Hand of Freedom, Power Word: Shield, Renew, Riptide, Rejuvenation, Lifebloom,
   Regrowth, Earth Shield, Heroism, Bloodlust, Avenging Wrath or Barkskin (Thorns and Nature's Grasp
   don't count). Purger: the first by guid of the group's non-healer bots that are alive, on the
   map, and either a shaman knowing `purge` (8012, 370) or a priest knowing `dispel magic`, off
   cooldown, with mana for it, and while a kill target exists within 30 yd of it and neither lost
   control nor silenced. The first with a shot keeps the duty; one without hands it on.
7. **Physical switch** (`FactionChampionsPhysicalSwitchTarget`). Physical attacker:
   `!PlayerbotAI::IsHeal(bot) && (PlayerbotAI::IsMelee(bot) || class hunter)`. While the kill target
   is immune to `SPELL_SCHOOL_MASK_NORMAL` and not to `SPELL_SCHOOL_MASK_MAGIC` (asked apart), keep the
   latched champion while `FactionChampionsNextTarget` still accepts it and it is not physically
   immune, else latch the first `FactionChampionsNextTarget` that is not physically immune (may be
   none); otherwise write `Empty`. Answers physical attackers only.
8. **AoE hazards.** One `GetCreatureListWithEntryInGrid` over warriors 34475/34453 and warlocks
   34474/34450, 200 yd. Sources: a warrior carrying Bladestorm with at least 300 ms left (radius 8,
   clearance 12); a warlock carrying Hellfire (radius 10, clearance 13). While the aura is under
   2000 ms old (max duration minus remaining), one kicker ignores that source: the first by guid of
   the group bots in control whose `current target` is the warlock, within melee range, knowing
   `kick`, `pummel`, `shield bash` or `mind freeze` off cooldown. `FactionChampionsInAoe`: within
   radius + 1 (`GetExactDist2d`) of a non-ignored source. `FactionChampionsAoeClearances`: those
   sources at their clearance. `FactionChampionsReachIntoAoe`: `IsMelee(bot)` and its `current
   target` is a source or within radius + 1 + `bot->GetMeleeRange(target)` of one.
   `NoteHazardCircle(map, aura id in play, source position, radius, 1000)` at most once per source
   per second per instance.
9. `FactionChampionsDispelBackfires(PlayerbotAI*, Unit*)`: live, and the target carries Unstable
   Affliction on the map's difficulty.

### Group 2 — nodes and multipliers

Files: `src/Ai/Raid/ToC/Trigger/ToCTriggers_FactionChampionsDefence.{h,cpp}`,
`src/Ai/Raid/ToC/Action/ToCActions_FactionChampionsDefence.{h,cpp}`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_FactionChampionsDefence.{h,cpp}` (all new).

10. **Triggers**, each testing exactly what its action acts on: avoid aoe `FactionChampionsInAoe`; mass
    dispel, dispel cc and purge kill target a non-null answer; physical switch a non-null target that
    `current target` differs from.
11. **Actions.**
    - `faction champions avoid aoe` (`MovementAction`), shaped like `JaraxxusAvoidLegionFlameAction`:
      `FindNearestPositionClearOfHazards(bot, FactionChampionsAoeClearances(botAI), 30.0f)`; latch the
      spot while it stays clear and the walk is in flight, 3 s ceiling; interrupt a pinning cast first
      (`IsMovementPreventedByCasting` → `InterruptNonMeleeSpells(true)`); `TryMoveTo` at
      `MOVEMENT_FORCED`, stepping its own previous forced leg down to replace it; release a walk
      stalled 500 ms; `true` while walking, `false` once outside every radius + 1 or with no spot.
    - `faction champions mass dispel`: stop a walk below `MOVEMENT_FORCED`, then
      `CastSpell("mass dispel", target)`.
    - `faction champions dispel cc`, `faction champions purge kill target`: `CastSpell(spell, target)`.
    - These three `isUseful` re-check their helper (a queued basket pops late).
    - `faction champions physical switch` (`AttackAction`): `Attack(target)`.
12. **Multipliers**, each testing the action family, then `ToCEncounterIsLive`, then its predicate:
    - aoe guard: `ReachMeleeAction` or `SetBehindTargetAction` → 0 while
      `FactionChampionsReachIntoAoe`.
    - physical switch guard: `FactionChampionsFocusPriorityAction` → 0 while
      `FactionChampionsPhysicalSwitchTarget` is non-null.
    - dispel guard: `CastCureSpellAction` or `CurePartyMemberAction` whose `getSpell()` is
      `dispel magic` or `cleanse` → 0 when `GetTarget()` backfires.
    - purge guard: `CastPurgeAction` or `CastSpellstealAction` → 0 when `GetTarget()` is a champion
      (`IsFactionChampion`). Tranquilizing Shot and Devour Magic stay.
13. **Registration** inside the stem, per the README constraints (inline one-line context class,
    one-line `: Trigger(botAI, "…")`): `ToCFactionChampionsDefenceTriggerContext`,
    `ToCFactionChampionsDefenceActionContext`, `AddToCFactionChampionsDefenceTriggerNodes` in the
    order avoid, mass dispel, dispel cc, physical switch, purge, and
    `AddToCFactionChampionsDefenceMultipliers(PlayerbotAI*, std::vector<Multiplier*>&)` in the order
    above. No burst row.

### Group 3 — reader, test, boss doc

Files: `tools/botobs/bosses/faction_champions.py`, `tools/botobs/tests/test_faction_champions.py`,
`docs/raids/trial-of-the-crusader/faction-champions.md`.

14. **Reader**, four new sections plus the docstring's "what the generic views get wrong":
    - `--dispel`: per counted CC spell (task 4), auras on the roster: count, time held, removed with
      more than 500 ms left; roster dispel casts (every rank of the task-4 spells, Mass Dispel 32375)
      on a member holding one, first-cast latency from the apply, by dispeller class; Unstable
      Affliction backlash auras (row 65813) on the roster; OK rows of `faction champions dispel cc`.
    - `--purge`: roster and pet offensive dispels on champions (Purge 370/8012, Dispel Magic 527/988,
      Spellsteal 30449, Tranquilizing Shot 19801, every Devour Magic rank, read from
      `spell.reference.csv`), kill target against the rest, by spell and caster class; OK rows of
      `faction champions purge kill target`.
    - `--hop`: champion casts of Hand of Protection 66009 (with target, kill target or not), Divine
      Shield 66010 and Ice Block 65802; each `fc.physical` hold with its length and the share of
      physical bots' samples (roles `tank`/`melee`, class hunter) on it; Mass Dispel casts after each
      shield and the delay; the `fc.switch back` that followed.
    - `--aoe`: per champion cast of Bladestorm 65947, Hellfire (row 65816), Fan of Knives (row 65955),
      Divine Storm 66006, Arcane Explosion (row 65800), Frost Nova 65792, Psychic Scream 65543 and
      Intimidating Shout 65930: roster victims per cast (damage rows of 65946, row 65817, the others'
      own ids; aura applies for the two fears), hits per victim for the two channels, and OK rows of
      `faction champions avoid aoe`. It is the data the carried-over spread decision needs.
    - `--vetoes` covers the eight FC multipliers.
15. **Test**: a synthetic pull pinning each new section's arithmetic (a dispelled Polymorph and one
    that expired, a backlash, a purge on and off the kill target, a HoP with a physical hold and a
    Mass Dispel, a Bladestorm hitting two bots twice), plus every section reading empty on
    `fixtures/full-v13.ndjson`.
16. **Boss doc** (fold with `compact-docs-writer`): add the defence rows to "What the code decides"
    (the Decisions table below, as built), the new keys and sections to "What a trace answers", and
    the gaps below to "Known gaps".

### Integrate

- Root dispatch, right after each `FactionChampions` line: `Absorb(ToCFactionChampionsDefence…Context())`
  in `ToCActionContext.h` and `ToCTriggerContext.h`, `AddToCFactionChampionsDefenceTriggerNodes` and
  `AddToCFactionChampionsDefenceMultipliers` in `ToCStrategy.cpp`, the three new headers in
  `Action/ToCRaidActions.h`, `Trigger/ToCRaidTriggers.h`, `Multiplier/ToCRaidMultipliers.h`.
- Four new `.cpp` files: CMake re-run (report it).
- Check the helper API against group 2's calls and the probe key, node and multiplier names against
  group 3's reader. `test_toc_naming.py` needs nothing: every new trigger leads with
  `faction champions`.
- Checks per the overview on `src/Ai/Raid/ToC` and `tools/botobs`, `--spell-difficulty` included.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Own stem `FactionChampionsDefence`; offence helper gains two read-only accessors | w3a (stem split); "one helper, two readers" keeps one kill order | copy the kill order and CC state into the defence stem |
| One dispeller per CC'd member at `ACTION_RAID + 3.5f`, healers dispelled first, non-healer dispellers used first | class dispels sit below `ACTION_RAID` and all chase one member; a CC'd healer costs the raid most | rely on the class `cure` nodes |
| Counted CC: Polymorph, Fear, Psychic Scream, Repentance, Hammer of Justice, Hex, Wyvern Sting on anyone, silences on healers, 2 s or more left; roots and Psychic Horror (3 s) not counted | script durations and targets; a GCD for a root or a 1 s remainder buys little | dispel roots on melee too |
| Magic-removing dispels, duty and class nodes alike, skip an Unstable Affliction carrier; no Mass Dispel while a carrier stands within 15 yd of its point | script: 65813 silences the dispeller 5 s and hits 9,250-10,750 (10N, 25N), 11,563-13,437 (10H), 13,875-16,125 (25H); `EffectDispel` picks at random; Mass Dispel's friendly half is `TARGET_UNIT_DEST_AREA_ALLY` 15 yd, centre to centre for players | dispel anyway; take the backlash for the shield |
| Readiness for the CC duty includes "not casting", so an idle dispeller takes over | fastest dispel; an instant dispel can't double up | stable duty that waits for the cast |
| Mass Dispel only on Hand of Protection or Divine Shield on the kill target, or Divine Shield on the suspended one, 3 s or more left; non-healer priest first | guide ("Divine Shield … can be Mass Dispelled"); Ice Block lasts 5 s against a 1.5 s cast | also AoE fears, champion lust, Ice Block |
| One non-healer shaman or priest purges the kill target while it holds a buff worth purging; class Purge and Spellsteal vetoed on champions; Tranquilizing Shot and Devour Magic kept | guide ("a Shaman or Priest … spamming Purge or Dispel Magic on your kill target"); Thorns keeps the class nodes firing every GCD; Spellsteal costs 20% base mana | leave the class nodes; pace Spellsteal only |
| Healers never purge | guide: healers heal | a healer shaman as fallback purger |
| Physical attackers (non-healer melee, hunters) leave a Hand of Protection kill target for the first champion in kill order that is not suspended, CC-assigned or physically immune, latched raid-wide; none left means stay | brief; latch untouched so casters keep the kill target | per-bot pick; allow a CC'd champion |
| Dodge only Bladestorm and Hellfire, the two AoEs a bot can see, at clearance 12/13 yd, `ACTION_EMERGENCY + 6` `MOVEMENT_FORCED`; one elected kicker waits 2 s | script radii and speeds (Bladestorm slows him to 5.6 yd/s); README priority convention; guide ("Hellfire … quickly interrupted") | pre-emptive spread (carried over) |
| Melee hold instead of reaching a target that is a source or within radius + 1 + the bot's melee range of one; the guard casts to `ReachMeleeAction` and `SetBehindTargetAction` only | raid-mechanics-lessons: a dodge fights the hold that walks back, and park tolerance belongs in the clearance maths (reach melee parks up to melee range on the near side); action-selection: narrow casts, heal and resurrect reaches exempt | radius + 2 on the target; `ReachTargetAction` base |
| Probes: `fc.physical` and hazard circles only; duties read from `act` and `cast` | w3a (no probe for the counterspell duty); observability ("don't probe what another stream says") | `fc.dispel`, `fc.purger` |
| Spread, totems and treants carried over | oversized-lane rule; no trace to size a spread; a spread tighter than the AoE buys nothing | build the spread blind |
| Hammer of Justice constants `SPELL_HAMMER_OF_JUSTICE_HOLY` 66613 and `…_RET` 66007 | `ICC/ICCTriggers.h` owns a global `SPELL_HAMMER_OF_JUSTICE` enumerator | one name per id as the brief listed |
| The 2 s Hellfire kick wait also exempts `FactionChampionsReachIntoAoe`, not only `InAoe` and the clearances | task 8 ("ignores that source"); else the aoe guard holds a melee kicker out of kick range | guard the reach regardless |
| One Hellfire kicker per fresh channel: first by guid of the bots in control targeting the warlock, in melee range, knowing Kick, Pummel, Shield Bash or Mind Freeze off cooldown; power and the GCD not tested | action-selection: a readiness election tests the bot's own cooldown and range; ranged interrupts reach from the 13 yd clearance, and mages have no current-target counterspell; the top ranks of Kick, Pummel and Shield Bash sit on the GCD and energy dips under Kick's cost, so either test would flip the kicker every swing | exempt every bot on the warlock; exempt only bots with a ready interrupt |
| A dispeller, Mass Dispel priest or purger whose chosen rank is on cooldown is skipped | `Player::ProhibitSpellSchool`: Counterspell 65790 (8 s), Spell Lock 67519 (6 s), Earth Shock (row 65973, 2 s) put the school's dispels on cooldown; the kicked bot is idle, so it would hold the duty through the lockout | keep the duty, wait out the lockout |
| Mass Dispel range: `IsWithinDist3d(target, 30)`, the priest's own size only | `Spell::CheckRange` dest branch; `IsWithinDistInMap` adds the champion's reach (up to 2.6) and elects a priest that can't cast | `GetExactDist` |
| Purger election tests control and 30 yd while a kill target exists | pitfalls: a claim needs a liveness test; class purges on champions are vetoed, so a claim without a shot means nobody purges | let class purges through while the duty is empty |
| `FactionChampionsDispelBackfires` takes the `PlayerbotAI*` and checks the live gate | the header's contract that every reader answers false unless live | narrow the header comment |
| A dispeller never takes itself; an ungrouped bot is its own one-member group | a CC'd bot can't cast; keeps the helpers total | none |
| An out-of-mana purger hands the duty to the next by guid | task 6 puts mana in the election | keep the purger, wait for mana |
| Hazard notes only while `RaidObs::Active()`, one per source per second | observability: no work when nothing records | note unconditionally |
| Mass dispel, dispel cc and purge are plain `Action`s | a blanket `MovementAction` veto would silence a dispel | `CastSpellAction` subclasses |
| Mass dispel stops a walk unless a `MOVEMENT_FORCED` leg still holds its lock (`lastdelayTime + msTime > now`, MoveTo's own gate) | a bare priority test lets an expired forced lock block the cast | yield to any forced priority |
| The dispel guard covers every `cleanse` cast, poison and disease nodes too | `spell.reference.csv`: Cleanse 4987 has three Dispel effects (poison, disease, magic) | `cleanse magic on party` only |
| `--dispel`: Silence and Strangulate count on role `heal` only; "removed early" is a removal row with `dur` over 500 ms, no `dur` counts as unknown | task 4's counted CC; a removal without `dur` can't be judged | count every silence; treat no `dur` as early |
| `--dispel` counts Mass Dispel apart: casts started while any counted CC was up, and how many of those started inside a champion shield's duration | a point cast (Targets 64) logs `tgt` 0, so no raider to tie it to; the shield duty is the only bot source of Mass Dispel | nearest CC'd raider; drop casts in shield windows |
| `--dispel` pairs a dispel with a CC only when it removes the CC's type (Dispel Magic magic; Cleanse magic, poison, disease; Remove Curse curse; Cleanse Spirit curse, poison, disease; Abolish and Cure Poison poison; Cure Toxins poison, disease) | a DoT dispel on a Hexed raider isn't an answer to the Hex | any friendly dispel |
| `--hop` windows each shield by its own duration (Hand of Protection 10 s, Divine Shield 12 s, Ice Block 5 s); `back` only for the two suspending shields cast on the kill target, only when `fc.kill` returns to that champion (within `SWITCH_SLACK_MS`), to 2 s past the end (`BACK_SLACK_MS`); `early` when it came before the shield's own end | Hand of Protection never suspends `fc.kill`; a shield that runs its full length also ends in a `back`, and another champion's `back` can land in the window | any `back` in the window |
| Reader's physical bots: non-human, role `tank` or `melee`, or class hunter | the same set as `IsPhysicalAttacker` | none |
| `--aoe` windows from the cast start: instants and fears 1 s, Bladestorm 9 s, Hellfire 16 s; a victim row goes to that caster's latest cast of the spell still in window; every remapped id counts | aura or channel length plus 1 s slack | attribute by position |
| Devour Magic ranks (19505 … 48011) hardcoded from `spell.reference.csv`; the champion felhunter's 67518 left out | the CSV lives in `mod-spell-tweaks`, outside this repo; 67518 is the enemy's, never a raid cast | read the CSV at runtime |

## Carried over

- `w3b-fc-defence-rest.PLAN.md`: pre-emptive spread against the instant crowd AoE, sized from `--aoe`
  traces; a totem kill duty; Force of Nature treants; Mass Dispel on AoE fears and champion lust.
- Merge stage, `docs/raids/trial-of-the-crusader/README.md`: `FactionChampionsDefence` in the Faction
  Champions "Code stems" cell.
- Merge stage, engine lessons: class purge and Spellsteal nodes fire on any dispellable positive
  magic aura on the current target, so a long self-buff (Thorns) keeps a mage stealing every GCD
  (`docs/classes/mage.md`, `shaman.md`); every dispel draws its auras at random and class dispels
  ignore dispel backlash like Unstable Affliction (`pitfalls.md`); priests have no offensive dispel
  node and never cast Mass Dispel on their own (`docs/classes/priest.md`).

## Known gaps

- Instant crowd AoE (Fan of Knives, Divine Storm, Arcane Explosion, Frost Nova, Psychic Scream,
  Intimidating Shout) is unanswered until the carried-over spread.
- Cyclone, Blind, Intimidating Shout and Death Grip plus Chains of Ice can't be dispelled, and no node
  answers them.
- Mass Dispel on Divine Shield rests on a trigger chain (32375 → 32592 → 39897) not verified on this
  core; `--hop` shows whether `back` came before the shield's own end after a Mass Dispel (`early`).
- A dispel may take a DoT instead of the CC; the duty fires again next tick.
- While Unstable Affliction holds, its carrier gets no magic dispel at all, and no Mass Dispel lands
  within 15 yd of it.
- With no eligible champion left, physical attackers keep swinging at the Hand of Protection target.
- The `fc.physical` latch outlives the kill, like the offence latch.

Mirrored in the boss doc's "Known gaps". Open issues after re-review: none; deferred findings: none.

## Blocked
