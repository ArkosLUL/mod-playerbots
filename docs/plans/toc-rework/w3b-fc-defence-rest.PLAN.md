# w3b-fc-defence-rest — Faction Champions: CC that pays, crowd-AoE spread, totems, treants

Wave 4d, a refine lane. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Mechanics, ids and the 10/25 roster:
`docs/raids/trial-of-the-crusader/faction-champions.md`, "What the defence plays against". Guide:
`faction-champions-master-strategy-guide-toc-25/`. Line numbers are as of `1846fc944`.

Trace (25N kill 4:05, 5 deaths, raid DPS 129k, human tank "Dragon", build carrying waves 3-4c):
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/649_3_thrakgar_1792176980.ndjson`.
Read it with `bosses/faction_champions.py <f>` (sections `--cc --fear --aoe --burst --kill`). For
the per-bot damage comparison, the same raid's Jaraxxus pull minutes before:
`.../botobs/649_3_lord-jaraxxus_1792176436.ndjson`; damage is the snapshots' cumulative `dealt`
column.

## Findings from the 2026-10-16 25N pull

4:05 is partly expected and about 75 s too long: the ten champions have 30.2M health, about 2:50 at
the raid's Jaraxxus rate. Health scaling is not the cause (`mod-dungeon-scale` gives every raid boss
the same 1.2; DB `HealthModifier` 192 per champion). Kill-target focus works: 97-99% of DPS samples.

1. **CC.** Three warlocks and the Balance druid spent 55-80% of the fight casting Fear or Cyclone:
   417 casts (Agony 133, about 200 of 245 s). A fear kept its champion feared (snapshot movement
   type 9) only 4-7% of the time:
   - champions get diminishing returns like players (`flags_extra` carries `ALL_DIMINISH`);
   - the CC targets stand in the melee pile, and damage breaks Fear.
   Those four dealt a third to half their Jaraxxus damage: about 22k raid DPS, half the gap.
2. **Other CC faults.** The feral druid (Ecoterrorist) left cat form to cast Cyclone. Both mages held
   a CC assignment and never cast Polymorph. The CC pick lives in the offence stem
   (`ToCHelpers_FactionChampions.cpp`, `SetRtiCcTarget` call at 710).
3. **Melee deaths.** Shadow died three times, Totemist twice: champion threat picks close,
   low-armour players. About 10k DPS lost.
4. **No lust before 2:09.** The Jaraxxus lust about 8.5 min earlier left the 10-minute Exhaustion;
   Totemist cast at 1:36 only because his death cleared his own. Pull pacing, not a bot fault.
5. **Stacking.** Intimidating Shout feared 22 raiders for 87 s in total because the melee stack.

## Scope

New item first; then the carried-over items, as far as the lane holds.

1. **CC that pays.**
   - A CC bot casts only when the cast can land and hold: track each champion's DR per category;
     skip while immune or when the next application would last 2.5 s or less; no damage-breaking CC
     on a champion damaged within about the last 2 s. Otherwise it attacks the kill target.
   - Melee druids leave the CC pool.
   - Find why the mages' assignment never cast; make it work or stop assigning it.
   - Measure first: if even well-timed CC can't beat the lost damage on normal, turn it off there
     and log the decision.
2. **Crowd-AoE spread.** Shout fears and Bladestorm on the melee stack (finding 5) are the numbers to
   beat.
   - Fan of Knives, Divine Storm, Arcane Explosion, Frost Nova, Psychic Scream and Intimidating Shout
     are instants that fire only with 3+ threat-list units (pets included) within 5-10 yd of the
     caster by `GetDistance2d`, which subtracts both combat reaches. Nothing can dodge them; only
     spacing denies them.
   - Guide: stay spread over the whole arena; keep away from melee champions. Champions run 8.0 yd/s
     against a player's 7, so kiting a melee champion fails.
   - Decide from traces first: `--aoe` gives victims per cast. Weigh that against what else kills
     (pitfalls: a spread tighter than the AoE buys nothing, spacing is a step function, and the
     spread can cost more than the AoE).
   - Melee on a melee kill target always make 3; any spread is ranged and healers only, and must
     yield to `faction champions avoid aoe` and keep healers in heal range.
   - The arena floor is GO 195527, invisible to navprobe: derive spots live or from where bots stood
     in traces.
3. **Totems.** The Enhancement Shaman drops Grounding (5925), Windfury (6112) or Tremor (5913) every
   30 s. Tremor strips the warlock bots' Fear-on-CC, Grounding eats one CC. The guide wants a player
   killing them. The offence target guard leaves no bot free to, so a duty needs its own node.
4. **Force of Nature** (Balance, every 3 min): three treants 36070 for 30 s. Decide from traces
   whether they matter.
5. **Mass Dispel** on AoE fears (3+ raiders under Psychic Scream) and on champion Heroism/Bloodlust,
   the guide's other two uses, if `--dispel` shows the fears cost more than the dispel duty clears.
6. **Observability.** `fc.` probes for whatever is built, and the matching reader sections and test:
   `--cc` gains fear uptime per champion (snapshot movement type 9) and casts landed against casts
   started; `--burst` names Exhaustion at the pull.

**Wave 4e** (`w8-prepull-staging`) reads this lane's opening spread for the pre-pull stands: keep it
reachable through a helper.

## Owns

Both stems, `FactionChampions` (for the CC pick) and `FactionChampionsDefence`;
`faction-champions.md`, `faction_champions.py` and its test.

## Verified facts

Mechanics, ids and the measured 25N numbers are in the boss doc ("Mechanics every choice rests on",
"The bots' CC pipelines", "Crowd AoE", "Totems and treants"). Measured from the trace above and its
combat log (gzipped in `chronicle-app-1:/root/.chronicle/storage/raidlogs/logs/4168c2a7-9b97-4e34-abe9-359663af1ccb`,
18:56:20-19:00:26 UTC). What the implementers need beyond those:

- **CC on the 25N pull.** 307 Fear casts gave 67 applications; 111 Cyclones 28, held 100 s. Druidica
  spent about 153 s of 245 casting Cyclone, much of it into DR (11 `IMMUNE` results), and kept
  casting after her release on the `moon` her restored `rti cc` named.
- **Class CC family.** Every class CC node the `cc` strategies wire is a `CastCrowdControlSpellAction`
  (`GenericSpellActions.h:385`): `CastPolymorphAction`, `CastFearOnCcAction`, `CastBanishOnCcAction`,
  `CastCycloneCcAction`, `CastEntanglingRootsCcAction`, `CastHibernateCcAction`. Warlock, mage,
  balance, cat and hunter bots all run `cc`.
- **Cyclone's prerequisite** (corrected in review). The generic `cyclone on cc` node carries
  `caster form`, but `BalanceDruidStrategy`'s factory re-registers it and `entangling roots on cc`
  with `moonkin form`, and `Engine::Init` keeps that one (strategies merge in name order; inside one,
  a factory added later, the subclass's after the base's, wins). So a Balance druid Cyclones in Moonkin Form; on 25N Druidica's
  Cyclone `PREREQ` rows pushed `moonkin form`, never `caster form`. Feral and Restoration keep
  `caster form`.
- **Diminishing returns API**: `Unit::GetDiminishing(group)` (non-const, resets a stale entry),
  `GetDiminishingReturnsGroupForSpell(info, false)`, `GetDiminishingReturnsGroupType`,
  `GetDiminishingReturnsLimitDuration` (`SpellMgr.h`); levels `DIMINISHING_LEVEL_1` 0 … `_IMMUNE` 3,
  durations ×1, ½, ¼, 0 per `Unit::ApplyDiminishingToDuration`. Cyclone's group is
  `DIMINISHING_CYCLONE`.
- **The `--aoe` attribution bug.** An instant AoE's `dmg` rows land 0-6 ms before its own `cast` row,
  so the reader gave each to the previous cast or none: Arcane Explosion (68000 on 25N), Divine Storm
  and Frost Nova read 0.1, 0.1 and 0 victims a cast against 6.7, 2.4 and 10 in the combat log. Fan of
  Knives and the two channels were right.
- **Trace facts for the reader.** Champion casts are `cast` rows (`tr` 1 for a triggered one:
  their Bloodlust at 26 s); totems and treants, hostile creatures, get `unit` rows with `own` and
  snapshot rows; `dmg` rows cover hits on roster players from any source. Exhaustion applied before
  the trace shows only as its removal: on 25N 23 of 25 carried it, 21 expiring at 2:09 (`dur` 0),
  Shadow (0:30) and Totemist (1:32) cleared by their deaths. Fear on a champion is snapshot column 9
  (`moveGen`) 9: Harkzog 6.9%, Ruj'kah 5.1%, Malithas 3.9%, Thrakgar 3.6% of their samples.
- **No coordinate is proposed**: no spread is built and the totem duty walks to a unit, so no
  navprobe run.

## Task list

Three groups code against these contracts; the integrate step compiles them together. No file is
new, so no CMake re-run and no root dispatch change.

- **Helper API** (group 1 writes, group 2 calls), `namespace TrialOfTheCrusaderHelpers`:
  - `Util/ToCHelpers_FactionChampions.h`: `constexpr uint32 SPELL_CYCLONE = 33786;` (the player
    spell, one rank) and `bool FactionChampionsCcCastPays(PlayerbotAI* botAI, Unit* target, uint32
    spellId);`: true only while live, when this bot holds a CC assignment and `target` is that
    champion, the spell's aura doesn't break on damage, the target isn't immune to it, and its next
    application lasts more than 2500 ms.
  - `Util/ToCHelpers_FactionChampionsDefence.h`: `Unit* FactionChampionsTotemTarget(PlayerbotAI*
    botAI);`: the champion totem this bot holds the kill duty for, null unless live.
- **Probes** (group 1 declares, group 3 reads), each declared on one line with its literal:
  `fc.ccaura` `ObsGuidMap<uint32>` keyed on the champion: the spell id of a player-cast fear, stun or
  confuse aura on it, erased (`0`) when none; `fc.totem` `ObsGuidMap<ObjectGuid>` keyed on the bot:
  the totem it kills, erased on release.
- **Names** (group 2 registers, group 3 reads): trigger and action `faction champions kill totem`
  (`ACTION_RAID + 2.7f`, no tie with +5, +4, +3.5, +3, +2.5, +2, +1, +0); multiplier `faction
  champions cc guard multiplier`; Definition rule `faction champions totem guard multiplier`.

### Group 1 — helpers and state

Files: `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h`,
`src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.cpp`,
`src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampionsDefence.h`,
`src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampionsDefence.cpp`.

1. **CC pool** (`IsCcBot`): a druid bot, alive, on the map, not `IsHeal`, not `IsMelee`, not a tank
   (`IsTank(member) || IsTank(member, true)`), knowing `SPELL_CYCLONE`, running `cc` in combat. Mages
   and warlocks leave the pool. Fix the header comments that name them.
2. **`FactionChampionsCcCastPays`**, cheapest first: live; `FactionChampionsCcTarget(botAI, icon)` is
   `target`; the `SpellInfo` exists; not `BreaksOnDamage`; `!target->IsImmunedToSpell(info)`;
   `NextCcDurationMs(target, info) > 2500`.
   - `BreaksOnDamage`: `AuraInterruptFlags & AURA_INTERRUPT_FLAG_TAKE_DAMAGE`, or `ProcFlags` set,
     no `SPELL_AURA_PROC_TRIGGER_SPELL`, and an effect applying `SPELL_AURA_MOD_CONFUSE`, `_MOD_FEAR`,
     `_MOD_STUN`, `_MOD_ROOT` or `_TRANSFORM` (the breakable branch of
     `AuraEffect::CalculateAmount`). Cyclone passes; Fear, Polymorph, Entangling Roots and
     Hibernate fail.
   - `NextCcDurationMs`, mirroring `Unit::ApplyDiminishingToDuration`: the max duration; for a group
     other than `DIMINISHING_NONE`, capped at a positive limit duration (the caster is a player, the
     champion `ALL_DIMINISH`), then scaled by the level `GetDiminishing` reads when the group type is
     `DRTYPE_ALL` or the creature `HasFlagsExtra(CREATURE_FLAG_EXTRA_ALL_DIMINISH)`. So Cyclone goes
     at 6 s and 3 s, never at 1.5 s or immune.
3. **`fc.ccaura`**, in `FactionChampionsState`, written by the live refresh after the scan: per
   alive champion, the spell id of the first `MOD_FEAR`, `MOD_STUN` or `MOD_CONFUSE` aura effect
   whose caster guid is a player (`Set` emits on change); erase a key whose champion now has none or
   left the scan, and every key in `EndPull`.
4. **Totem scan**: add Grounding 5925, Windfury 6112 and Tremor 5913 (`NPC_*` constants in the
   defence header) to the hazard scan's one `GetCreatureListWithEntryInGrid`. A totem counts while
   alive, `IsTotem()`, its owner (`GetOwnerGUID()`) a creature whose entry `IsFactionChampion`, and
   `bot->IsValidAttackTarget(totem)`: the raid's own totems share these entries.
5. **Totem duty**, in `FactionChampionsDefenceState` as `RaidObs::ObsGuidMap<ObjectGuid>
   totemKillers{"fc.totem"}`, refreshed with the rest:
   - erase a hold whose totem no longer counts, or whose bot is dead, off the map,
     `UNIT_STATE_LOST_CONTROL` or past 30 yd of it (`GetExactDist2d`);
   - give each unheld totem to the nearest group bot (`GetExactDist2d`, ties by guid) that is alive,
     on the map, not `IsHeal`, not lost control, within 30 yd and holding no totem;
   - `FactionChampionsTotemTarget` returns this bot's held totem while it is alive. Holds left by a
     wipe go at the next live refresh, their totems gone.

### Group 2 — nodes, multipliers, rule

Files: `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_FactionChampions.h`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_FactionChampions.cpp`,
`src/Ai/Raid/ToC/Trigger/ToCTriggers_FactionChampionsDefence.h`,
`src/Ai/Raid/ToC/Trigger/ToCTriggers_FactionChampionsDefence.cpp`,
`src/Ai/Raid/ToC/Action/ToCActions_FactionChampionsDefence.h`,
`src/Ai/Raid/ToC/Action/ToCActions_FactionChampionsDefence.cpp`,
`src/Ai/Raid/ToC/Definition/ToCDefinition_FactionChampions.cpp`.

6. **`FactionChampionsCcGuardMultiplier`** (`faction champions cc guard multiplier`), pushed last in
   `AddToCFactionChampionsMultipliers`; 1.0 unless live:
   - a `CastCrowdControlSpellAction`: 1.0 only when `FactionChampionsCcTarget` names a champion,
     `action->GetTarget()` is it (ask after the assignment: Polymorph's `cc target` is a scan), and
     `FactionChampionsCcCastPays(botAI, it, AI_VALUE2(uint32, "spell id", cc->getSpell()))`; else 0.
     The `CastCasterFormAction` branch first planned here was dropped in review (Decisions).
7. **Kill totem**: trigger `FactionChampionsKillTotemTrigger`, one-line `: Trigger(botAI, "faction
   champions kill totem")`, active while `FactionChampionsTotemTarget` is non-null and `current target`
   isn't it or `ToCPetOffPick` holds; action `FactionChampionsKillTotemAction : ToCDirectAttackAction`
   calling `AttackDirect(FactionChampionsTotemTarget(botAI))`. Register both in the defence
   contexts; push the node between `dispel cc` and `physical switch`.
8. **Totem guard**: in `DefineFactionChampions`, `e.Block("faction champions totem guard
   multiplier", Role::Any, <predicate>, 0, {"faction champions focus priority", "faction champions
   physical switch"})`, the predicate `FactionChampionsTotemTarget(botAI) != nullptr`. Without it the
   settled duty returns false and focus drags the bot back to the kill target.

### Group 3 — reader, test, boss doc

Files: `tools/botobs/bosses/faction_champions.py`, `tools/botobs/tests/test_faction_champions.py`,
`docs/raids/trial-of-the-crusader/faction-champions.md`.

9. **`--aoe`**: a victim row up to `AOE_LEAD_MS = 50` before a cast start belongs to that cast; the
   window still runs from the cast start.
10. **`--cc`** adds, per champion, held time by spell from `fc.ccaura` and fear uptime (share of its
    live snapshot samples with `moveGen` 9); per CC bot, casts started and landed. A Fear lands when
    its target's `moveGen` turns 9 within the cast time + 1 s; a Cyclone, Polymorph or Entangling
    Roots when `fc.ccaura` on its target turns to that spell within the same window, `n/a` in a trace
    without the key.
11. **`--burst`** names Exhaustion 57723 and Sated 57724 held at the pull: each roster member whose
    first row of either is a removal, when it came off, `expired` (`dur` 0) or `cleared` (time left,
    a death), and a line with how many carried it and when the last expired.
12. **`--adds`**, new: champion casts of 65989, 65990, 65992 and 65861 (time, caster); their units
    (totems 5925/6112/5913 and treants 36070 whose `own` is a champion) with first and last snapshot;
    treant damage to the raid from `dmg` rows, total and victims; each `fc.totem` hold per bot, its
    length and whether the totem left the snapshots inside it; OK rows of `faction champions kill
    totem`.
13. **`--vetoes`** covers ten multipliers (add `faction champions cc guard multiplier` and `faction
    champions totem guard multiplier`). Docstring: the new section, and three things the generic
    views get wrong: instant AoE damage rows precede their cast row; Exhaustion applied before the
    trace shows only as its removal; `fc.ccaura` is the only record of CC on a champion.
14. **Test**: a synthetic pull pinning a Cyclone span on `fc.ccaura` with one landed and one unlanded
    cast, a Fear landed by `moveGen` 9 and the uptime share, an expired and a death-cleared
    Exhaustion beside one applied mid-pull, an instant AoE whose `dmg` rows sit 3 ms before its cast,
    a treant's damage and lifetime, a totem summon with an `fc.totem` hold, and ten multipliers; every
    new section empty on `fixtures/full-v13.ndjson`.
15. **Boss doc** (fold with `compact-docs-writer`): rewrite the CC row of "What the code decides" and
    add Totems, Treants, Spread and the Mass Dispel non-uses, from the Decisions table as built; the
    new keys and sections in "What a trace answers"; "Known gaps" per the list below, dropping "Class
    CC nodes recast into diminishing-returns immunity" and rewording the instant crowd AoE one. The
    investigator's mechanics facts are already in.

### Integrate

- Check group 2's calls against group 1's API and the probe keys, node and rule names against group
  3's reader. `test_toc_naming.py` needs nothing: the new trigger leads with `faction champions`.
- Checks per the overview on `src/Ai/Raid/ToC` and `tools/botobs`, `--spell-difficulty` included.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| No damage-breaking CC (Fear, Polymorph, Entangling Roots, Hibernate) on any difficulty; the "damaged in the last 2 s" test isn't built | core: Fear and Roots break at 2,652, Polymorph and Hibernate on any hit; 25N trace: outside Cyclones the champions spent 0.3 s in all more than 2 s past a hit, 66 of 67 Fears broke within 30 ms of one; the core keeps no last-damaged time, and on that data the test always fails | keep Fear on heroic; build the 2 s test on a damage hook |
| CC pool: non-healer, non-melee, non-tank druids knowing Cyclone; mages and warlocks leave | brief (melee druids leave; mages fixed or dropped); the mage pipeline never aims at `rti cc` in a raid, and Polymorph breaks on any hit | a ToC Polymorph node aimed at the assignment |
| Cyclone only when the next application lasts over 2.5 s (6 s, 3 s), read off the core's DR table | brief; one Cyclone cycle is then about 3 s of casting for 9 s held per 27 s, where Druidica cast for about 153 s of 245 | track DR per champion in the state |
| Cyclone stays on normal | well timed it denies a third of the next healer's output (Thrakgar healed 10.4k/s alive, Erin 4.6k/s) for about 11% of one druid's time | turn CC off on normal too |
| The guard zeroes every class CC node (`CastCrowdControlSpellAction`) for a bot without an assignment, off its assignment, or whose cast doesn't pay | `rti cc` defaults to `moon`, the first CC icon, so the dropped warlocks and any cat would still Fear or Cyclone the druid's champion | reset `rti cc` on non-pool bots |
| No `caster form` handling: the assigned druid's Cyclone already keeps Moonkin Form | review: `BalanceDruidStrategy` re-registers `cyclone on cc` with `moonkin form`, and on 25N Druidica's Cyclone pushed only that; zeroing `caster form` while the Cyclone pays was dead for every pool bot and would have zeroed it for every other node needing it meanwhile | zero `caster form` in Moonkin Form while the Cyclone pays |
| Heroic: Cyclone goes even with the champion's PvP trinket ready | it burns the 2 min trinket and the next one holds; the trinket goes on cooldown only when used, so waiting never ends | skip a champion with the trinket ready |
| No crowd-AoE spread | 25N: 97% of crowd AoE hit melee and tanks, who stack on a melee kill target by design; ranged and healers took 11% of champion damage, their crowd AoE one opening Shout (13 feared) and 14k of Bladestorm; denying the 3+ check needs about 14 yd between every ranged and healer pair (11 yd centre to centre plus his offset from his victim), which 14 bots can't hold within spell range of the ball; guide: the fastest kills are the death ball | a partial ranged and healer spread; opening stands 8 yd apart |
| Kill every champion totem: one bot each, the nearest non-healer within 30 yd, latched, `ACTION_RAID + 2.7f`, focus and physical switch blocked for it | guide ("a melee player dedicated to killing his totems"); within 30 yd Grounding draws the next spell aimed at a champion (a Cyclone, a Counterspell), Windfury gives the champions 16% melee haste, Tremor undoes fears; health modifiers 0.05-1.1; no trace yet has an Enhancement Shaman | Grounding and Windfury only; ranged only |
| No treant node; `--adds` measures them | no trace has a Balance Druid; the guide doesn't mention them; three 30 s adds every 3 min | a kill duty or a tank pick-up |
| No Mass Dispel on AoE fears | 25N: Psychic Scream held 8 raiders 8.5 s in all, 4 of whom drew a dispel; the costly one, Intimidating Shout (22 raiders, 87 s), can't be dispelled | Mass Dispel on 3+ under Psychic Scream |
| No Mass Dispel on champion lust | its hostile half takes one random magic buff per champion within 10 yd, where Thorns, HoTs and shields crowd the draw; one lust a pull (26 s on 25N); the purge duty already counts lust on the kill target; the Unstable Affliction guard blocks it in the ball | Mass Dispel at the lust |
| Kill order unchanged although the mage's Arcane Explosion was 17% of champion damage | overview: the guide decides tactics, Mage C | move the mage up |
| Probes `fc.ccaura` and `fc.totem`; why a CC waited reads off the guard's `veto` rows | observability: probe what no stream says; a creature's auras reach no stream | an `fc.ccwait` reason per bot |
| `--aoe` takes victim rows up to 50 ms before the cast | measured lead 0-6 ms | match by position |
| `NextCcDurationMs` scales, and caps at the limit duration, exactly where the core does: `DRTYPE_ALL`, or `DRTYPE_PLAYER` on a player, a player's pet or an `ALL_DIMINISH` creature | core `Unit::ApplyDiminishingToDuration`; the brief's "`DRTYPE_ALL` or `ALL_DIMINISH`" would also scale `DRTYPE_NONE` groups. Cyclone on a champion reads 6, 3, 1.5 s, then 0 either way | the brief's wording |
| Duration is the static `GetMaxDuration()`, without caster spell mods; the taunt DR branch isn't mirrored | brief; no CC node casts a taunt | apply the caster's mods |
| A permanent (negative) duration pays | core: negative is permanent | never pays |
| A totem hold is released by the test that assigns it, so also when its bot turns healer or isn't a bot | one predicate; a superset of the brief's release list | release only on the brief's list |
| `fc.ccaura` records fear, stun and confuse only; `--cc` reads a Roots cast's landing `n/a` | the probe contract; the guard never lets Roots cast | add `MOD_ROOT` to the scan |
| `--burst`: a removal followed within 100 ms by the member's apply is a fresh lust, not a carry; a removal before the pull isn't one either | 25N: a lust writes the caster's copy as a removal with full time left, then the member's own apply about 10 ms later; without the rule Deathsong and Dragon read as carriers | match removals to the lust cast |
| `--cc`: a fleeing or `fc.ccaura` turn inside several casts' windows lands only the first | 25N: otherwise 70 Fear landings from 52 turns | credit the nearest cast |

## Carried over

- `w8-prepull-staging`: Faction Champions has no opening spread, so there is no stand helper to read;
  its stands need only the 23 yd rule. The 25N opening Shout feared all 13 stacked ranged and healers.
- Merge stage, engine lessons:
  - `raid-mechanics-lessons.md` "Crowd control and threat on adds" says roots and CC don't break on
    damage; player-cast Fear and Entangling Roots do, at a 2,652 threshold, through their proc flags
    (`AuraEffect::CalculateAmount`, `HandleBreakableCCAuraProc`), DoT ticks included.
  - `docs/classes/mage.md`: in a raid, Polymorph never aims at `rti cc` (`PolymorphTrigger` is a
    `HasCcTargetTrigger`; `CastPolymorphAction` targets `cc target`). `pitfalls.md`:
    `FindTargetForCcStrategy` lets a later attacker overwrite its `rti cc target` match.
  - `docs/classes/druid.md`: the generic `… on cc` nodes' `caster form` prerequisite drops cat and
    bear form (the 25N feral left cat form to Cyclone); `BalanceDruidStrategy` re-registers Cyclone
    and Roots with `moonkin form`. A node's prerequisites come from whichever factory `Engine::Init`
    merges last, so read the spec's own strategy before trusting the generic node.
  - `raid-mechanics-lessons.md`: Exhaustion and Sated outlive a pull, so a lust spent on one boss
    is missing on the next within 10 min.
  - `observability.md`: snapshots, about 240 ms apart, miss a CC broken between two samples (25N: 15
    of 67 Fears never showed as moveGen 9), so a landing count off snapshots or a refresh-written
    probe is a lower bound.

## Known gaps

- No crowd-AoE spread: the melee stack takes the instant crowd AoE, and ranged and healers stand
  stacked, so an opening Shout or Bladestorm on them catches all of them.
- The mage champion's Arcane Explosion (17% of champion damage on 25N) under a C-tier kill order.
- Melee deaths from the script's threat picks (all 5 on 25N) get no answer.
- Treants are unanswered.
- With no Balance Druid in the raid there is no CC at all; on heroic the trinket breaks the first
  Cyclone on each champion every 2 min.
- A Grounding Totem draws the next Cyclone or Counterspell aimed at a champion within 30 yd before
  the duty kills it.
- `fc.ccaura` and `fc.totem` keep their last values past the kill, like `fc.kill`: stage 6 closes
  the gate before a refresh clears them.

## Blocked
