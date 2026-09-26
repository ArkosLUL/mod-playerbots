# w3a-fc-offence — Faction Champions: targeting, CC, burst

Wave 3. `w3b-fc-defence` follows in wave 4. Rules, sources, naming contract and checks:
[toc-rework.PLAN.md](toc-rework.PLAN.md). Guide: `faction-champions-master-strategy-guide-toc-25/`.

## Scope

1. **Kill target.**
   - Today `FactionChampionsFocusPriorityAction` re-picks the lowest-health healer, then the
     lowest-health champion, every tick. The marks it places are never cleared inside a pull.
   - Latch the kill target with a sticky margin and switch only on a real reason. Clear the old mark
     on a switch.
   - Take the kill order from the guide.
2. **CC.** Assign it through `SetRtiCcTarget` and the `"rti cc"` value. Today it is one moon on a
   second healer; the guide may want more.
3. **Interrupts.** Class strategies already interrupt enemy healers (mage "counterspell on enemy
   healer", rogue kick). Check that focus plus those nodes cover the healers, and add duty only
   where they don't.
4. **AoE suppression.** Keep it, but through the encounter gate.
5. **Burst.**
   - Champions reset threat every 2 s (`boss_faction_champions.cpp`), so the base
     `HoldBurstUntilTankEngagedMultiplier` never opens.
   - Add a raid-agnostic exemption for bosses with no stable victim, modelled on `noVictimBosses`
     (`src/Ai/Base/Combat/BurstCooldowns.cpp`, used in `src/Ai/Base/Strategy/BurstWindowStrategy.cpp`).
     This is an allowed change outside ToC; list it in the report.
   - Then set the lust row.
6. **Fears.** Fear 65809, Psychic Scream 65543 and Intimidating Shout 65930 go through the shared
   `RaidAntiFear`.
7. **Threat redirect.** Veto the generic threat redirect here: the main tank is reliably the wrong
   target.
8. **Observability.** `fc.` probes (kill target, CC target), a
   `tools/botobs/bosses/faction_champions.py` reader and test.

## Owns

Stem `FactionChampions` (offence files; split so `w3b` gets its own); `faction-champions.md`; the
reader and its test; the base burst exemption above.

## Verified facts

- Both factions' rosters are in the `FactionChampions` stem, from the old `ToCHelpers.h`. NPC entries
  are the same on every difficulty.
- The core script picks the composition per pull, not per instance: 6 champions in 10-man, 10 in
  25-man, a new lineup and new guids after every wipe.
- Mechanics, lineup, threat, Aegis, DR, trinket, fears and the guide's tactics:
  `docs/raids/trial-of-the-crusader/faction-champions.md` (written by the investigator). Scope item 5's
  "every 2 s" is the first threat reset only; then every 8.75-9.25 s.
- **Old code bugs.** `FactionChampionsFocusPriorityAction` marks only when its bot is the tracker,
  but its trigger refuses healers and `IsMechanicTrackerBot` is the first alive bot of any role, so
  a healer tracker means no marks for the whole pull. The focus trigger stays true while the bot is
  already on target. `GetPriorityFactionChampion` reads each bot's own `possible targets no los`,
  so two bots can disagree about the pick.
- **Interrupt coverage** (class strategies). The kill target's casts get the current-target nodes:
  rogue `kick` (42), shadow priest `silence` (42), warrior `pummel` (40, fury), paladin
  `hammer of justice` (40), warlock pet `spell lock` (40), DK `mind freeze` (21), shaman
  `wind shear` (23). Other healers get the `… on enemy healer` nodes, which skip the bot's own
  current target (`EnemyHealerTargetValue`): mage `counterspell on enemy healer` (40), priest
  `silence`, paladin `hammer of justice`, rogue `kick` and warrior `pummel` in melee range only; DK
  `mind freeze on enemy healer` has no action registered. **Mages have no current-target counterspell
  node**, so no mage ever interrupts the kill target.
- **CC reach.** In a raid `HasCcTargetTrigger` does not need the `rti cc` target:
  `FindTargetForCcStrategy` takes it when castable, else any attacker at 65%+ health that is not the
  bot's own target. Classes with an in-combat CC that holds a humanoid: mage `polymorph`, warlock
  `fear on cc`, balance and cat druid `cyclone on cc`/`entangling roots on cc`. Hunter
  `freezing trap on cc` has no creator (`pitfalls.md`); rogue `sap` is out of combat only; resto
  druids carry no `cc` strategy. With one moon every CC bot piles onto one champion and burns its DR.
- **`rti cc` persists.** `RtiCcValue` implements `Save`/`Load`, and `AiObjectContext::Save` writes
  every created value to the bot's DB store, so a changed value outlives the pull and the session.
- **Burst.** The 28 champions are boss-flagged (`type_flags & CREATURE_TYPE_FLAG_BOSS_MOB`), so
  `HoldBurstUntilTankEngagedMultiplier` waits for `TankHasHeldBoss`, which the script's threat
  resets make rare. `OffensivePotionTrigger` (`src/Ai/Base/Trigger/GenericTriggers.cpp`) runs the
  same dwell with no vehicle or no-victim exemption.
- **Redirects.** Hunter `misdirection on main tank` is `CastMisdirectionOnMainTankAction`; rogue
  `tricks of the trade` is `CastTricksOfTheTradeAction`, whose `tricks of the trade target` is the
  main tank whenever `TankNeedsRedirect`, else the strongest melee DPS within 20 yd.
  `CastTricksOfTheTradeOnMainTankAction` is registered but no class node uses it.
- **Targeting.** `DpsTargetValue` returns the skull first, but `TankTargetValue` takes the skull only
  while its victim is not a tank, else another champion, so tanks and the focus node trade targets.
- No coordinate is proposed, so no navprobe run.

## Task list

Three groups code against these contracts; the integrate step compiles them together.

- **Helper API** (group 1 writes, group 2 calls), `namespace TrialOfTheCrusaderHelpers`
  in `Util/ToCHelpers_FactionChampions.h`. The `ToCFactionChampions` enum keeps its name and its 28
  entries (`ToCEncounterGate.cpp` and `test_toc_naming.py` read it); `IsFactionChampion` and
  `IsFactionChampionHealer` stay.
  - `Unit* FactionChampionsKillTarget(PlayerbotAI* botAI);` latched per instance, `nullptr` unless
    `ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions)`.
  - `bool FactionChampionsFocusBot(PlayerbotAI* botAI);` every non-healer (`!PlayerbotAI::IsHeal(bot)`).
  - `bool FactionChampionsFearWindowActive(PlayerbotAI* botAI);` live, and a warlock, disc or shadow
    priest, or warrior champion alive.
- **Kill order**, also the CC order: Holy Paladin (34465, 34445), Disc Priest (34466, 34447), Resto
  Shaman (34470, 34444), Resto Druid (34469, 34459), Rogue (34472, 34454), Warrior (34475, 34453),
  Hunter (34467, 34448), Enhancement (34463, 34455), Death Knight (34461, 34458), Retribution
  (34471, 34456), Warlock (34474, 34450), Shadow Priest (34473, 34441), Mage (34468, 34449), Balance
  (34460, 34451). Alliance entry first. Pets (35465, 35610) are never picked.
- **Probe keys** (group 1 declares, group 3 reads): `fc.kill` `ObsValue<ObjectGuid>` (the kill
  target, `0` when cleared); `fc.switch` `RaidObs::Note`, text `first`/`dead`/`immune`/`back`/`reset`;
  `fc.cc` `ObsGuidMap<ObjectGuid>` bot → its CC target, erased on release. Each on one line with its
  literal, so `probes.py` finds it.
- **Node and multiplier names** (group 3 reads): triggers and actions `faction champions mark
  targets`, `faction champions anti fear`, `faction champions should focus` → `faction champions
  focus priority`, `faction champions cc icon` → `faction champions set cc icon`, `faction champions
  kill target healing` → `faction champions counterspell kill target`; multipliers `faction champions
  suppress aoe multiplier`, `faction champions target guard multiplier`, `faction champions threat
  redirect veto multiplier`, `faction champions anti fear totem guard multiplier`.

### Group 1 — targeting, CC, interrupt and fear nodes

Files: `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.h`,
`src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.cpp`,
`src/Ai/Raid/ToC/Trigger/ToCTriggers_FactionChampions.h`,
`src/Ai/Raid/ToC/Trigger/ToCTriggers_FactionChampions.cpp`,
`src/Ai/Raid/ToC/Action/ToCActions_FactionChampions.h`,
`src/Ai/Raid/ToC/Action/ToCActions_FactionChampions.cpp`.

1. **Per-instance state**, a `RaidInstanceState<…>` in the helper `.cpp`, refreshed at most once per
   instance per `getMSTime()` ms by whichever bot asks first: the three probes, the alive champion
   guids from one `GetCreatureListWithEntryInGrid(list, <28 entries>, 200.0f)` off the asking bot
   (the gate's radius), the suspended guid, per-bot CC icon index and the `rti cc` value each bot had
   before its first assignment. While not live: clear the kill latch (`fc.switch reset` once), erase
   every `fc.cc` entry, keep the saved `rti cc` values for task 5.
2. **Kill latch.** Candidates are alive, in-combat champions; touchable means
   `!unit->IsImmunedToDamage(SPELL_SCHOOL_MASK_ALL)` (Divine Shield, Ice Block, Cyclone; Hand of
   Protection is physical only and stays). Rank by the kill order, then guid. Keep the latched target
   while it is a touchable candidate. When it turns untouchable, suspend it and latch the best
   touchable other (`immune`); a newer suspension replaces an older one. When the suspended one is
   touchable again and its health % is at or below the current target's, return to it (`back`).
   When the target dies or leaves, latch the best
   touchable (`dead`, or `first` from empty); with none touchable, the best untouchable. Replace
   `GetPriorityFactionChampion` and `GetCcFactionChampionHealer`.
3. **CC assignment.** CC bots: group members with a bot AI, alive, on the map, not healers, class
   mage, warlock or druid, and `HasStrategy("cc", BOT_STATE_COMBAT)`; sorted by guid. Candidates:
   alive champions except the kill target, in kill order. Keep each bot's target while both stay
   eligible; give unassigned bots the best unassigned candidates in guid order; one target per bot.
   A new assignment takes the first icon of moon, square, triangle, diamond, circle, star, cross not
   held by another assignment; a kept one keeps its icon. `Unit* FactionChampionsCcTarget(PlayerbotAI*,
   uint8& iconIndex)` answers for the asking bot.
4. **Marks** — trigger and action `faction champions mark targets`, `ACTION_RAID + 4`, any role, only
   on `IsMechanicTrackerBot(bot, TRIAL_OF_THE_CRUSADER_MAP_ID)`. Fires while the skull is not on the
   kill target, or a pool icon sits on a champion no assignment gives it to, or (not live) an icon
   this node placed is still up. Moves the skull (`MarkTargetWithSkull`; `Group::SetTargetIcon`
   clears any icon the new target wore), clears stray pool icons, clears its own icons once not live.
   Returns `false`.
5. **CC icon** — trigger `faction champions cc icon` → action `faction champions set cc icon`,
   `ACTION_RAID + 1`. Assigned: `SetRtiCcTarget(botAI, <icon name>, target)` whenever the bot's
   `rti cc` or the group icon differs, saving the bot's prior `rti cc` first. Released or not live:
   restore the saved value and forget it. Returns `false`. The trigger tests exactly what the action
   writes.
6. **Focus** — `faction champions should focus` (name kept, `test_toc_naming.py` lists it) →
   `faction champions focus priority`, `ACTION_RAID + 2`: `FactionChampionsFocusBot`, a kill target,
   and `current target` differs from it; the action `Attack`s it. No marking here any more.
7. **Kill-target counterspell** — `faction champions kill target healing` → `faction champions
   counterspell kill target`, `ACTION_INTERRUPT + 3`. Fires for the one mage the group ranks first by
   guid among alive mage bots on the map, within 30 yd of the kill target, not casting
   (`IsNonMeleeSpellCast(false)`) and without
   Counterspell 2139 on cooldown (`Player::HasSpellCooldown`), while the kill target's current generic
   or channelled spell `IsPositive()` and has a cast time or is a channel (Tranquility). Casts
   `counterspell` on the kill target.
8. **Anti-fear** — `FactionChampionsAntiFearTrigger : RaidAntiFearTrigger` and
   `FactionChampionsAntiFearAction : RaidAntiFearAction`, both `faction champions anti fear`,
   `ACTION_RAID + 3`, `FearWindowActive()` = `FactionChampionsFearWindowActive`.
9. **Registration** inside the stem: creators in `ToCFactionChampionsTriggerContext` and
   `ToCFactionChampionsActionContext` (constructor inline, one-line `class X : public
   NamedObjectContext<…>`, each trigger's `: Trigger(botAI, "…")` on one line, per the README), nodes
   in `AddToCFactionChampionsTriggerNodes` in the order of task numbers 4, 8, 6, 5, 7. Drop the old
   interrupt comment; state the kill order's source in one line at the table.

### Group 2 — multipliers and the burst exemption

Files: `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_FactionChampions.h`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_FactionChampions.cpp`,
`src/Ai/Base/Combat/BurstCooldowns.h`, `src/Ai/Base/Combat/BurstCooldowns.cpp`,
`src/Ai/Base/Strategy/BurstWindowStrategy.cpp`, `src/Ai/Base/Trigger/GenericTriggers.cpp`,
`docs/systems/consumables-and-burst.md`.

Every multiplier tests the action family first, then `ToCEncounterIsLive(botAI,
ToCEncounter::FactionChampions)`, then anything else.

10. **AoE suppression** stays as it is; its comment states Champion's Aegis 68595: a flat −75% to
    AoE damage on every champion.
11. **`FactionChampionsTargetGuardMultiplier`** (`faction champions target guard multiplier`):
    `DpsAssistAction` or `TankAssistAction` → 0 while `FactionChampionsFocusBot` and
    `FactionChampionsKillTarget` is non-null, the focus node being their only target source.
12. **`FactionChampionsThreatRedirectVetoMultiplier`** (`faction champions threat redirect veto
    multiplier`): `CastMisdirectionOnMainTankAction` and `CastTricksOfTheTradeOnMainTankAction` → 0;
    `CastTricksOfTheTradeAction` → 0 only when `AI_VALUE(Unit*, "tricks of the trade target")` is
    `AI_VALUE(Unit*, "main tank")`.
13. **`FactionChampionsAntiFearTotemGuardMultiplier`** (`faction champions anti fear totem guard
    multiplier`) : `RaidAntiFearTotemGuardMultiplier`, `FearWindowActive()` =
    `FactionChampionsFearWindowActive`.
14. Register 11-13 in `AddToCFactionChampionsMultipliers` after the AoE one.
15. **Lust row**: `ToCFactionChampionsBurstWindow` returns `{true, true}` with one line of why (guide:
    lust and every cooldown at the pull).
16. **Base exemption.** `BurstCooldowns.cpp` gains `unstableVictimBosses` (the 28 champion entries,
    listed as numbers with a comment naming them) and `bool BossHasNoStableVictim(Unit const* boss)`,
    declared beside `BossTakesNoVictim` with a comment: the script re-seeds threat on a timer, so a
    tank holds the boss only by chance. `HoldBurstUntilTankEngagedMultiplier` adds it to the vehicle
    and no-victim exemption; `OffensivePotionTrigger` returns true past its boss check for a boss on
    a vehicle, with no victim or with no stable one, resetting `holdState`.
17. **`consumables-and-burst.md`** (fold with `compact-docs-writer`): the "Behaviour by target" table
    gains the exemption (vehicle, `BossTakesNoVictim`, `BossHasNoStableVictim`, and the potion
    trigger now matching); lines 20 and 79 drop Anub'arak's deleted `dynamic_cast` lust gate.

### Group 3 — reader, test, boss doc

Files: `tools/botobs/bosses/faction_champions.py` (new), `tools/botobs/tests/test_faction_champions.py`
(new), `docs/raids/trial-of-the-crusader/faction-champions.md`.

18. **Reader**, shaped like `bosses/general_vezax.py` (`run_sections`, module docstring naming what
    the generic views get wrong here). Banner: difficulty, the lineup from champion `unit` rows
    (name, spec, healer or DPS), `fc.` keys the source declares and this pull never wrote. Sections:
    - `--kill`: each `fc.kill` hold with its `fc.switch` reason, start, length, health % at start
      and end from `snap`, whether it died, and the share of non-healer samples whose target column
      is the kill target.
    - `--cc`: per bot, each `fc.cc` hold; roster Polymorph, Fear, Cyclone and Entangling Roots casts
      on champions, the share on the bot's assigned target and any on the kill target.
    - `--heals`: champion cast-time heals started (Lesser Healing Wave 66055/68115-68117, Nourish
      66066/67965-67967, Regrowth 66067/67968-67970, Flash Heal 66104/68023-68025, Holy Light
      66112/68011-68013, Flash of Light 66113/68008-68010, Tranquility 66086/67974-67976), and how
      many a roster or pet interrupt cast on that champion followed within the cast time, split kill
      target versus other, by interrupter class. `cast` is cast start only, so this is an upper
      bound on landed heals.
    - `--burst`: first Heroism 32182/Bloodlust 2825 from the pull, each bot's first burst cooldown,
      `veto` rows from `hold burst until tank engaged` and `toc burst window`.
    - `--fear`: roster auras of Fear 65809, Psychic Scream 65543, Intimidating Shout 65930 (count,
      time held), and Fear Ward 6346 and Tremor Totem 8143 casts.
    - `--vetoes`: veto rows of the four FC multipliers, by action.
19. **Test**: a synthetic pull pinning each section's arithmetic (a switch for each reason, a CC
    hold, one heal interrupted and one not, a lust cast, a fear aura pair, a veto), plus every section
    reading empty on `fixtures/full-v13.ndjson` without raising.
20. **Boss doc** (fold with `compact-docs-writer`): add what the code decides and why (the Decisions
    table below, as built) and replace "What a trace answers" with the probe keys and the reader, as
    in `docs/raids/ulduar/mimiron.md`; move the gaps below into "Known gaps".

### Integrate

Nothing changes at the root or the four registration sites, and no C++ file is new, so no CMake
re-run. Compile the helper API against group 2's calls; check the probe keys and every node and
multiplier name against group 3's reader. Run the checks per the overview on the eight ToC files,
the four base files and `tools/botobs`.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Healers first (paladin, disc, shaman, druid), then the DPS danger table | guide's first school ("shorten the fight"), its healer notes, the old code's healers-first; script: every champion helpful cast goes to the most-injured friendly, so healers keep the kill target up | the guide's second school, dangerous DPS first |
| Kill target latched per instance from one grid scan; switches only on death, all-school immunity, or a return when the suspended target is attackable and no healthier | brief (sticky latch, real reasons); script: Divine Shield and Ice Block below 25%; one scan so all bots agree | re-pick lowest health each tick (old); never return from a suspension |
| One CC target per CC-capable bot, in kill order skipping the kill target, kept while valid, own icon via `SetRtiCcTarget`; prior `rti cc` restored on release and wipe | guide (mark every target, assign CC); brief (`SetRtiCcTarget`); ALL_DIMINISH makes piling burn DR | one moon for every CC bot (old) |
| The next kill target is CC'd until its turn; a Cyclone on it at the switch is a normal suspension | guide: CC or kill each healer | exclude the next target from CC |
| Interrupt duty only for mages on the kill target's heals, one mage by guid | class audit above: every other interrupter already covers the kill target or other healers | a raid-wide interrupt rotation (`vezax.md`) |
| AoE suppression kept as is | Champion's Aegis −75% on every AoE, DB; guide | allow Death and Decay and Pestilence (guide says unaffected; not verified in the core) |
| Burst exemption keyed on the 28 entries, in the multiplier and the offensive potion trigger | brief; script's timed threat re-seed; the potion trigger runs the same dwell | multiplier only, leaving potions dead here (and on VX-001) |
| Lust row `{true, true}`: lust and cooldowns at the pull | guide ("Use Bloodlust / Heroism & all DPS cooldowns immediately") | hold lust for a kill target below a health threshold |
| Misdirection and Tricks on the main tank vetoed; smart Tricks only when it resolves to him | brief; threat is script-assigned, so the 15% damage to a melee DPS is the only value left | veto all Tricks (`UldDefinition_Vezax.cpp`) |
| `DpsAssist`/`TankAssist` vetoed for non-healers while a kill target exists | `raids/README.md` "Targeting: the whole fight"; `TankTargetValue` leaves the skull when a tank holds it | veto nothing, relying on the skull |
| Tanks join the kill target, no taunt peel | guide: no tank role in the standard strategy; heroic immune to taunt; a normal taunt lasts until the next threat reset | a normal-mode peel node |
| Marks get their own tracker node for any role | old focus action never marked under a healer tracker | keep marks in the focus action |
| Fear window: live and a fear-capable champion alive | script: Fear every 10-15 s, Screams and Shout on 3+ within 8 yd | open for the whole encounter |
| Pets never targeted or CC'd | guide silent; no kill credit, no Aegis | rank them last |
| No probe for the counterspell duty | its node is in `act`, its casts in `cast` | `fc.interrupter` like `vezax.interrupter` |
| Offence keeps the `FactionChampions` stem; `w3b` adds a `FactionChampionsDefence` stem and its four root dispatch lines | brief ("split so `w3b` gets its own"); no empty skeleton classes | register an empty defence stem now |
| `consumables-and-burst.md` edited, including Anub'arak's stale lines | the base exemption is this lane's; `w0c` carried the stale lines over | leave the stale lines to `w6-closeout` |
| Stray CC icons cleared only while live; between pulls the marker clears only icons still where this encounter put them (the skull, each CC icon recorded when a CC bot placed it) | conservative: a wipe despawns the champions, so their icons stop resolving; human pre-pull marks stay | clear only icons the mark node placed (brief) |
| Counterspell duty also needs the mage to know 2139; the heal test adds `Spell::EffectInterruptCast`'s guard (casting, or preparing with a cast time; silence prevention type; interrupt flag on the generic spell or the channel) | core script; CLAUDE.md "a call site isn't proof of its effect" | `IsPositive` plus cast time or channel only (brief) |
| An immune kill target with no touchable alternative stays latched, unsuspended | conservative reading of task 2 | suspend it and leave the latch empty |
| `FactionChampionsKillTarget` and `FactionChampionsFearWindowActive` check live before refreshing | after the kill only multipliers ask; their refresh would write `reset` into the Twins trace | refresh on every ask |
| No `rti cc` save when the bot's value and the group icon already match its assignment | the prior value is the same one | save before every set |
| `consumables-and-burst.md` drops all three `dynamic_cast` lust gates (Maulgar, Gruul, Anub'arak), and cites the registry as `BurstCooldowns.cpp:22-46` | upstream `3a8a6e4af` (#2751) deleted Maulgar's and Gruul's; source line numbers | drop Anub'arak's only (brief) |
| Reader "died": the champion left the snapshots 2 s+ before the last one and within 2 s of the hold's end, or the hold is still open when the trace closes on a kill | creature deaths are never recorded and the sweep drops a dead creature (`RaidObsSnapshot.cpp`); a kill writes no reset. Risk: a despawn at a wipe before the trace closes reads as a death, so `--kill` prints the switch reason beside it | none: no death record exists |
| Reader `--kill` "on it" counts non-healer bots only, humans out | the latch steers bots only | every roster member |
| Reader interrupts: the brief's class nodes plus Shield Bash, Strangulate, Silencing Shot, every Hammer of Justice rank and both Spell Lock ranks; a pet's counts under its owner's class | trace schema (pet owners) | the brief's list only |
| Reader heal window: the cast row's `ct` (haste included); Tranquility, a channel whose row reads 0, falls back to its 10 s DBC channel | trace schema | fixed DBC cast times |
| Reader burst names parsed from `burstCooldownNames`; CC and heal tables carry their own spell names; `--heals`/`--fear` also count the duty actions' OK rows | one registry; traces without spell names lump every spell under "spell"; recorder `act` rows | a copied name list; `trace.spell` |
| Touchable = not immune to both `SPELL_SCHOOL_MASK_NORMAL` and `SPELL_SCHOOL_MASK_MAGIC`, asked apart | review: `IsImmunedToDamage(mask)` needs one aura covering the mask; Ice Block 65802 is aura 39 misc 1 plus misc 126 (spell CSV) | `SPELL_SCHOOL_MASK_ALL` (missed Ice Block) |
| CC candidates skip the suspended champion too | review: CC on it breaks or blocks the `back` return | skip every untouchable one (would drop a CC bot's own Cyclone target) |
| Counterspell duty skips a mage with `UNIT_STATE_LOST_CONTROL` or `UNIT_FLAG_SILENCED`, and a target immune to 2139; no pacify check | core `Spell::CheckCasterAuras`: pacify blocks only `SPELL_PREVENTION_TYPE_PACIFY`, Counterspell is silence type; Hex's aura 60 sets silenced too. A school lockout is a per-spell cooldown (`Player::ProhibitSpellSchool`) | `CanCastSpell` per mage (a `Spell` object per mage per tick); also check pacify (review) |
| The counterspell action's `isUseful` is the duty predicate | review: a queued basket pops up to `ExpireActionTime` later | none |
| The not-live `rti cc` restore is its own node, `toc restore rti cc` (trigger and action, `ACTION_RAID + 1`), ungated, reading the state with `Find`, no `Refresh`; `faction champions cc icon` keeps the assigned case and release while live. `tools/botobs/tests/test_toc_naming.py` lists it in `RAID_WIDE_TRIGGERS` | review: the gate closes at stage 6, so a gated restore never reaches a bot dead at the kill; a `Refresh` would write `fc.switch reset` into the Twins trace | leave it a known gap |
| Reader classes: ids 1-11 mapped to names, strings still accepted, late joiners read from `unit` rows by re-reading the file | recorder writes `c` as `getClass()`; `Trace` drops `unit` rows and keeps no class | edit `raidobs/trace.py` (shared, outside the lane) |
| Banner lists `fc.` keys only | brief ("`fc.` keys the source declares") | relabel as FC/ToC probes |

The guide's openers (Death Grip pulls, an AoE fear to burn trinkets, the death ball) and kiting are
beyond the bots; they are in the boss doc's gaps.

## Carried over

- `w3b-fc-defence`: Hand of Protection needs a per-bot physical switch (the latch keeps a
  physically immune target); purge and spellsteal, including the champions' Heroism/Bloodlust;
  friendly CC dispels; spreading against the 3+-within-8-10 yd AoE. Divine Shield and Ice Block are
  handled by the latch. Its stem is `FactionChampionsDefence` (Decisions).
- Merge stage, engine lesson for `docs/engine/pitfalls.md` or `docs/classes/mage.md`: mages have no
  current-target counterspell node, and every `… on enemy healer` value skips the bot's own target,
  so no mage interrupts what it is attacking.
- Merge stage, engine lesson: `IsMechanicTrackerBot` elects any role, so a marker gated behind a role
  predicate goes silent whenever a healer is first in the group.
- Merge stage: `rti cc` (and `rti`) values are persisted by `AiObjectContext::Save`; an encounter
  that writes one owes a restore (`pitfalls.md`, "An AI value set by one encounter is still set in
  the next one").
- Merge stage: `docs/raids/trial-of-the-crusader/README.md` has no Faction Champions spell rows; none
  are needed (every id this lane keys on has one id everywhere).
- `w6-closeout`, stale in `docs/systems/consumables-and-burst.md`: the name table lacks
  `killing spree` and `power infusion`; "Two names carry explicit exemptions" omits the healer
  exemption for `avenging wrath` and `power infusion`.

## Known gaps

Mirrored in the boss doc's "Known gaps". Open issues after re-review: none; deferred findings: none.

- The guide's openers and positioning (Death Grip pulls, an AoE fear to burn trinkets, the death
  ball, kiting melee champions) are beyond the bots.
- Hand of Protection (physical only) keeps its champion latched, so melee swing at an immune target
  (`w3b-fc-defence`).
- The gate closes the stem at stage 6, at the kill, before any cleanup runs: the skull stays on the
  last champion and the latch is never cleared (no `fc.switch reset`, no `fc.kill 0`).
- Class CC nodes recast into diminishing-returns immunity; champions DR like players.

## Blocked
