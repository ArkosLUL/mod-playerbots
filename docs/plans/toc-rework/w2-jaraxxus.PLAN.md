# w2-jaraxxus — Lord Jaraxxus

Wave 3. Rules, sources, naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md).
Guide: `lord-jaraxxus-master-strategy-guide-toc-25/`.

## Scope

1. **Intro.** Keep the Fizzlebang intro (stage 2) closed. Jaraxxus sets `IsEncounterInProgress` on
   engage (`boss_lord_jaraxxus.cpp`, `JustEngagedWith`).
2. **Fel Fireball interrupt duty**, ranked by GUID among able and ready interrupters and recomputed
   per cast (`docs/raids/ulduar/vezax.md`). A positioning action returning true while moving starves
   class interrupts.
3. **Incinerate Flesh** absorb healing, which exists today; re-check its priority against the
   "dispel below `ACTION_RAID`" trap.
4. **Legion Flame.** Today `AvoidCreatureClusterAction`. Make it a trail-aware hazard sweep with
   `FindNearestPositionClearOfHazards`; the target walks out of the raid.
5. **Nether Power.** Dispel or spellsteal it. Its stacks aren't in any trace, so probe them.
6. **Adds.**
   - Mistress of Pain (Mistress' Kiss locks a school: casters stop casting while kissed).
   - Felflame Infernal (Fel Streak, threat reset).
   - Nether Portal and Infernal Volcano by difficulty. Their ids are declared but unused today:
     `NPC_NETHER_PORTAL`, `NPC_INFERNAL_VOLCANO`, `SPELL_LEGION_FLAME`.
   - Tank assignment for the adds.
7. **Touch of Jaraxxus** is commented out in this core (`boss_lord_jaraxxus.cpp`): record it as not
   implemented; don't build it.
8. **Lust row**, `jaraxxus.` probes, a `tools/botobs/bosses/lord_jaraxxus.py` reader and test.
9. Verify every Jaraxxus node `w0c-foundation` made live in 25N/10H/25H.

## Owns

Stem `Jaraxxus`; `docs/raids/trial-of-the-crusader/lord-jaraxxus.md`; the reader and its test.

## Task list

Verified mechanics, ids, timers and the tactics these tasks implement are in
`docs/raids/trial-of-the-crusader/lord-jaraxxus.md`; read it first. No new `.cpp`, so no CMake
re-run, and nothing outside the stem is registered: every creator and node lives in group 2's
headers. Groups 1 and 2 code against the contract below in parallel; the integrate step compiles
them together.

**Helper contract** (group 1 writes, group 2 calls), `Util/ToCHelpers_Jaraxxus.h`, namespace
`TrialOfTheCrusaderHelpers`. Spell constants stay plain `constexpr uint32` remapped at each call as
`sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)` (pblint reads the bare identifier);
`SPELL_LEGION_FLAME`, `SPELL_FEL_FIREBALL`, `SPELL_INCINERATE_FLESH` and the four
`SPELL_NETHER_POWER_*` stay in `ToCData.h` (not a lane file, do not edit it).

```cpp
constexpr uint32 SPELL_LEGION_FLAME_TRAIL = 66199;   // row 66199/68126/68127/68128
constexpr uint32 SPELL_MISTRESS_KISS = 66334;        // row 66334/67905/67906/67907
constexpr float JARAXXUS_LEGION_FLAME_RADIUS = 3.0f;       // 66877 row, centre to centre
constexpr float JARAXXUS_LEGION_FLAME_TRIGGER = 4.0f;      // dodge starts inside this
constexpr float JARAXXUS_LEGION_FLAME_CLEAR = 6.0f;        // a spot counts clear of a flame past this
constexpr float JARAXXUS_LEGION_FLAME_SEARCH = 15.0f;      // sweep maxRadius
constexpr float JARAXXUS_FLAME_CARRIER_MIN_LEG = 6.0f;     // carrier travel floor per leg
constexpr float JARAXXUS_FLAME_CARRIER_RAID_CLEAR = 8.0f;  // carrier spot from other members
constexpr float JARAXXUS_FLAME_CARRIER_BOSS_CLEAR = 15.0f; // carrier spot from the boss
constexpr float JARAXXUS_FLAME_ARRIVE = 1.5f;
constexpr float JARAXXUS_INTRO_STAND = 3.0f;               // main tank from him in the intro
constexpr float JARAXXUS_SCAN_RADIUS = 200.0f;
constexpr uint32 JARAXXUS_SCAN_MS = 200;

enum class JaraxxusFlameRole : uint8 { None, Carrier, Dodge };

Unit* GetJaraxxus(PlayerbotAI*);          // alive and not UNIT_FLAG_NON_ATTACKABLE
Unit* GetJaraxxusInIntro(PlayerbotAI*);   // alive, NON_ATTACKABLE, not in combat
bool JaraxxusHasNetherPower(Unit*);       // unchanged
uint32 JaraxxusNetherPowerStacks(PlayerbotAI*);  // 0 without a boss; writes jaraxxus.netherpower
bool JaraxxusIsNetherPowerRemover(Player*);
bool HasIncinerateFlesh(Unit*);           // unchanged
Unit* GetIncinerateFleshTarget(PlayerbotAI*);    // lowest-guid alive group member carrying it
bool IsLegionFlameCarrier(Unit*);         // SPELL_LEGION_FLAME or SPELL_LEGION_FLAME_TRAIL, remapped
bool HasMistressKiss(Unit*);
bool IsJaraxxusAdd(Unit*);                // portal, volcano, Mistress or Infernal entry
bool JaraxxusAnyAddAlive(PlayerbotAI*);   // Mistress, Infernal, or a selectable portal/volcano
Unit* GetJaraxxusFocusAdd(PlayerbotAI*);  // kill order; writes jaraxxus.focus
Unit* GetJaraxxusAssistTankAdd(PlayerbotAI*, uint8 index);  // writes jaraxxus.addtank
bool JaraxxusIsFelFireballCasting(Unit* boss);
char const* JaraxxusReadyInterrupt(Player* bot, Unit* boss);  // null if none ready and in range
bool JaraxxusIsFelFireballInterrupter(Player* bot, Unit* boss);  // writes jaraxxus.interrupter
std::vector<Player*> GetJaraxxusPinnedCasters(Player* bot);
JaraxxusFlameRole GetJaraxxusFlameRole(Player* bot);
float GetLegionFlameCarrierHeading(Player* bot);
bool DeriveLegionFlameSpot(Player* bot, JaraxxusFlameRole role, float heading, Position& spot);  // writes jaraxxus.flame
bool IsLegionFlameSpotClear(PlayerbotAI*, Position const& spot);
bool LegionFlameCrossesPath(Player* bot, Position const& to);
void JaraxxusClaimRti(Player* bot);
bool JaraxxusOwnsRti(Player* bot);
void JaraxxusReleaseRti(Player* bot);
```

`GetPriorityJaraxxusAdd` and `GetSecondaryJaraxxusAdd` go; group 2 stops calling them.

### Group 1 — helpers

Files: `src/Ai/Raid/ToC/Util/ToCHelpers_Jaraxxus.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_Jaraxxus.cpp`.

1. **Per-instance state**, one `static RaidInstanceState<JaraxxusState>` in the `.cpp`
   (`src/Ai/Raid/RaidInstanceState.h`; never `thread_local`): a scan (stamp, valid flag, boss guid,
   guids per kind sorted ascending, flame positions); `RaidObs::ObsValue<uint32>
   netherPower{"jaraxxus.netherpower"}`; `RaidObs::ObsValue<ObjectGuid> focus{"jaraxxus.focus"}`;
   `RaidObs::ObsGuidMap<ObjectGuid> addTank{"jaraxxus.addtank"}`; a plain guid set of bots whose
   `rti` Jaraxxus code owns. Off map 649 every reader returns its empty answer.
2. **Scan**, refreshed when `JARAXXUS_SCAN_MS` old: one `bot->GetCreatureListWithEntryInGrid(list,
   entries, JARAXXUS_SCAN_RADIUS)` over Jaraxxus 34780, Nether Portal 34825, Infernal Volcano 34813,
   Mistress 34826, Infernal 34815, Legion Flame 34784 (the `ToCNpcs` entries), alive only. Store guids
   and flame positions, never pointers; readers resolve with `map->GetCreature(guid)` and re-test
   alive. Replaces every `GetFirstAliveUnitByEntry` in the stem.
3. **Boss**: `GetJaraxxus`, `GetJaraxxusInIntro` per the contract; `JaraxxusNetherPowerStacks` reads
   the stack count of whichever of the four ids he carries and assigns it to `netherPower` on every
   call (the container emits on change only).
4. **Removers**: mage always; shaman or priest when not a healer; a healing shaman or priest only
   when no alive group member on the map is a mage or a non-healing shaman or priest, and
   `GetIncinerateFleshTarget` is null.
5. **Kill order**: `GetJaraxxusFocusAdd` returns, lowest guid first within each kind, the first of:
   portal without `UNIT_FLAG_NOT_SELECTABLE`, volcano without it, Mistress, Infernal; assigns `focus`
   (empty guid when none). `GetJaraxxusAssistTankAdd(0)`: lowest-guid Mistress, else, when no alive
   group member answers `PlayerbotAI::IsAssistTankOfIndex(member, 1, true)`, the Infernal pick.
   Index 1: the Infernal pick, else the second-lowest-guid Mistress. Infernal pick: lowest guid whose
   victim is a player that is not a tank (`IsTank(p) || IsTank(p, true)`), else the one whose victim
   is this bot, else lowest guid. Assign `addTank[bot]`, erase when null.
6. **Interrupt duty** (`docs/raids/ulduar/vezax.md`, "Node ladder and guards"):
   `JaraxxusIsFelFireballCasting` compares `GetCurrentSpell(CURRENT_GENERIC_SPELL)`'s id with the
   remapped Fel Fireball. `JaraxxusReadyInterrupt`: null for a bot without a bot AI or with
   `UNIT_STATE_CASTING`; else the first of `kick`, `pummel`, `shield bash`, `mind freeze`,
   `counterspell`, `wind shear` that `CanCastSpell(name, boss)` and is in range — `IsWithinMeleeRange`
   for a spell whose max range (`GetSpellMaxRangeForTarget`) is at most 5 yd, else
   `IsWithinCombatRange(boss, maxRange)`. Silencing
   Shot and Silence are silence effects he is immune to; Spell Lock is left out because
   `CanCastSpell` answers a pet spell true without a cooldown check. Interrupter = ready and no alive
   group member on the map with a lower guid is ready; `NoteDerived(bot, "jaraxxus.interrupter",
   "1"/"0")`.
7. **Pinned casters**: alive group members on the map with a bot AI, `HasUnitState(UNIT_STATE_CASTING)`
   tested first, then `HasMistressKiss || IsLegionFlameCarrier`.
8. **Legion Flame geometry**, all through `FindNearestPositionClearOfHazards` with `HazardCircle`s
   (never `FleePosition`; the arena floor is a gameobject, so let the live collision check judge
   every spot and fix no coordinate):
   - Role: Carrier while `IsLegionFlameCarrier(bot)`, Dodge within `_TRIGGER` of a flame, else None.
   - Carrier heading: bearing from the boss (or `ARENA_CENTER` without one) to the bot.
   - Carrier spot: flames at `_CLEAR`; unless the bot is the boss's victim, other alive members
     within 40 yd at `_RAID_CLEAR` and the boss at `_BOSS_CLEAR`; `preferNear` = bot + heading ×
     2·`_MIN_LEG`; `accept` rejects travel under `_MIN_LEG`; `maxRadius` `_SEARCH`. Nothing clear:
     retry with flames only. Note `carrier`, `relaxed` or `none`.
   - Dodge spot: flames at `_CLEAR`, `preferNear` the boss for a bot that is not `IsRanged`, note
     `dodge` or `none`.
   - `IsLegionFlameSpotClear`: every flame at least `_CLEAR` away. `LegionFlameCrossesPath`: a flame
     within `_CLEAR` of the segment bot → `to`.
9. **Rti claim**: the set in task 1; claim inserts, release erases.

### Group 2 — nodes

Files: `src/Ai/Raid/ToC/Trigger/ToCTriggers_Jaraxxus.{h,cpp}`,
`src/Ai/Raid/ToC/Action/ToCActions_Jaraxxus.{h,cpp}`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_Jaraxxus.{h,cpp}`.

Keep pblint's constraints (README "Constraints"): contexts inline in their headers, each
`: Trigger(botAI, "name")` on one line. Every trigger name leads with `jaraxxus` (gate prefix).

10. **Node ladder**, no ties, pushed in this order:

    | Trigger | Action | Relevance |
    |---|---|---|
    | `jaraxxus fel fireball interruptible` | `jaraxxus interrupt fel fireball` | `ACTION_EMERGENCY + 9` |
    | `jaraxxus legion flame nearby` | `jaraxxus avoid legion flame` | `ACTION_EMERGENCY + 8` |
    | `jaraxxus pinned cast` (new) | `jaraxxus break pinned cast` (new) | `ACTION_EMERGENCY + 7` |
    | `jaraxxus intro main tank` (new) | `jaraxxus intro main tank stand` (new) | `ACTION_RAID + 6` |
    | `jaraxxus engaged by main tank` | `jaraxxus main tank hold boss` | `ACTION_RAID + 5` |
    | `jaraxxus add needs assist tank` | `jaraxxus assist tank hold add` | `ACTION_RAID + 4` |
    | `jaraxxus second add needs assist tank` | `jaraxxus assist tank hold second add` | `ACTION_RAID + 3` |
    | `jaraxxus nether power active` | `jaraxxus remove nether power` | `ACTION_RAID + 2` |
    | `jaraxxus add should be focused` | `jaraxxus focus add` | `ACTION_RAID + 1` |
    | `jaraxxus focus stale` (new) | `jaraxxus reset focus` (new) | `ACTION_RAID` |
    | `jaraxxus incinerate flesh on raid` | `jaraxxus heal incinerate target` | `ACTION_MEDIUM_HEAL + 5` |

11. **Triggers** (all per tick):
    - interruptible: `GetJaraxxus`, `JaraxxusIsFelFireballCasting`, `JaraxxusIsFelFireballInterrupter`.
    - legion flame nearby: role not None.
    - pinned cast: `GetJaraxxusPinnedCasters` not empty.
    - intro main tank: `IsMainTank`, `GetJaraxxusInIntro`, 2D distance above `_INTRO_STAND + 1`.
    - engaged by main tank: `IsMainTank` and `GetJaraxxus`.
    - assist tanks: `IsAssistTankOfIndex(bot, 0|1, true)` and `GetJaraxxusAssistTankAdd(0|1)`.
    - nether power: `JaraxxusNetherPowerStacks > 0` first, so every bot keeps the probe current, then
      `JaraxxusIsNetherPowerRemover`.
    - focused: neither tank nor healer, `GetJaraxxusFocusAdd`.
    - focus stale: `JaraxxusOwnsRti` and not `JaraxxusAnyAddAlive`.
    - incinerate: `IsHeal`, `GetIncinerateFleshTarget` within `sPlayerbotAIConfig.healDistance` and in
      line of sight.
12. **Actions**:
    - interrupt: a plain `Action` casting `JaraxxusReadyInterrupt` on the boss.
    - avoid legion flame: a `MovementAction` (drop the `AvoidCreatureClusterAction` base, which stays
      for the slime pools). Break a pinning cast first (`IsMovementPreventedByCasting` →
      `InterruptNonMeleeSpells(true)`, pitfalls "A bot mid-cast cannot be moved"). Latch the spot in a
      member: while the walk is in flight, the spot `IsLegionFlameSpotClear` and younger than 3 s,
      return true without calling `MoveTo` (pitfalls "Every MoveTo calls mm->Clear()"). Re-derive when
      it arrives within `_ARRIVE`, turns unclear, or the bot stood still 500 ms past the issue (clear
      `last movement` first, as Hodir's `ReleaseStalledWalk`). A carrier latches its heading on its
      first tick and after each leg takes the leg's own bearing, so it sweeps forward and never back
      over its trail, and on arrival derives the next leg at once; a dodger returns false on arrival.
      `MOVEMENT_FORCED`. Role None clears both latches and returns false.
    - break pinned cast: `GET_PLAYERBOT_AI(member)->RequestSpellInterrupt()` for each pinned caster;
      returns false. A bot mid-cast runs no triggers, so another bot's tick has to do this.
    - intro stand: a `MovementAction` moving to `_INTRO_STAND` from him on the bot's side,
      `MOVEMENT_NORMAL`; false once within `_INTRO_STAND + 1`.
    - main tank hold: as now through `GetJaraxxus`, but skip `DragBossToAnchor` (return false) while
      `LegionFlameCrossesPath(bot, ARENA_CENTER)`.
    - assist tanks: `GetJaraxxusAssistTankAdd(0|1)`; mark square (0) or diamond (1) and
      `SetRtiTarget` to that icon, `JaraxxusClaimRti`; `Attack` it unless current; then, if its victim
      is a player that is not a tank, `CastClassTaunt(botAI, add)`.
    - focus: `GetJaraxxusFocusAdd`; mark cross, `SetRtiTarget("cross", add)`, `JaraxxusClaimRti`,
      `CommandPetAttack(botAI, add)` every tick above any early return, `Attack` unless current.
    - reset focus: `SetRtiTarget(botAI, "skull")`, `"rti target"` set to null,
      `JaraxxusReleaseRti`; returns false.
    - remove nether power: as now, on `GetJaraxxus`.
    - heal incinerate: as now on `GetIncinerateFleshTarget`; while `HasMistressKiss(bot)` skip heals
      with a cast time or channel.
13. **Multipliers**, each testing the action family first, then cheap bot state, then
    `ToCEncounterIsLive(botAI, ToCEncounter::Jaraxxus)` (except the intro hold), then lookups:
    - `JaraxxusControlTankMovementMultiplier`: victim test becomes the boss entry or `IsJaraxxusAdd`.
    - `JaraxxusTauntGuardMultiplier` ("jaraxxus taunt guard"): `IsTauntAction(bot, action)`, current
      target is Jaraxxus, his victim a player other than the bot with `IsTank(p) || IsTank(p, true)`
      → 0 (`docs/raids/ulduar/iron-assembly.md`, taunt guard).
    - `JaraxxusKissCastHoldMultiplier` ("jaraxxus kiss cast hold"): a `CastSpellAction` whose
      `getSpell()` resolves (`"spell id"` value) to a spell with `CalcCastTime(bot) > 0` or
      `IsChanneled()`, on a bot with `HasMistressKiss` → 0.
    - `JaraxxusIntroHoldMultiplier` ("jaraxxus intro hold"): any `MovementAction` except the intro
      stand, on the main tank, while `GetJaraxxusInIntro` → 0, so `follow` cannot walk him back.
    - `ToCJaraxxusBurstWindow` stays `{}`; its comment says why (boss doc, Tactics, lust).

### Group 3 — reader, test, doc

Files: `tools/botobs/bosses/lord_jaraxxus.py` (new), `tools/botobs/tests/test_lord_jaraxxus.py`
(new), `docs/raids/trial-of-the-crusader/lord-jaraxxus.md`.

14. **Reader**, modelled on `bosses/general_vezax.py` (`run_sections`, banner with `silent_keys`,
    radii through `geometry.radius("JARAXXUS_…")`). Entries and spell rows from the boss doc; every
    difficulty id of a row counts. Probes: `jaraxxus.netherpower` (stacks), `jaraxxus.focus` and
    `jaraxxus.addtank` (guid keys as in `snap`, `0`/empty for none; `addtank` keyed by the tank),
    `jaraxxus.interrupter` (`1`/`0` per bot), `jaraxxus.flame` (`carrier`/`relaxed`/`dodge`/`none`
    per bot). Sections:
    - `boss`: each Fel Fireball cast start, whether its damage landed or an interrupt cast by a bot
      at him fell inside the cast, who, and the `interrupter` latch then; hits per Fel Lightning
      cast (the input for `w2-jaraxxus-rest`).
    - `nether`: stack timeline, seconds at 1+ and at max, Spellsteal/Purge/Dispel Magic casts at him.
    - `incinerate`: per window from the aura rows: target, how it ended, heals landed on the target
      inside it, Burning Inferno damage after it.
    - `flame`: per carrier window: distance walked, nearest other member and the boss during the
      trail, `flame` branch counts; flame-tick damage per bot (who stood in fire).
    - `adds`: per add unit: first and last seen, share of samples on a non-tank victim, `focus` and
      `addtank` changes; Mistress' Kiss auras and kiss-punish hits.
15. **Test**: a synthetic pull pinning each section's arithmetic, and `fixtures/full-v12.ndjson`
    reading empty without raising (`tests/test_general_vezax.py` is the model).
16. **Boss doc** (fold with `compact-docs-writer`): the node ladder of task 10 with one line each
    on why the order holds; "What a trace answers" names the five probe keys and the reader's
    sections.

### Integrate

Compile the contract; run the checks per the overview, with `--spell-difficulty`.

## Nodes made live

Verified by reading (no map 649 trace exists): `jaraxxus incinerate flesh on raid` and its heal
action remap row 304, `jaraxxus fel fireball interruptible` remaps row 241 against his current
spell, which carries the remapped id; `jaraxxus nether power active` lists all four ids. All three
are live on 25N/10H/25H.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Intro closed to every node but the main tank's stand beside him, held by the intro-hold multiplier | script: release attacks `SelectNearestTarget(200)`; guide: tank on top of the boss at the pull | fully closed, as brief item 1 reads |
| Kill order heroic portal, volcano, Mistress, Infernal; normal Mistress, Infernal; lowest guid first | guide: swap to the portal before the Mistress, DPS the volcano down; script: heroic spawns every 6 s / 5 s | Mistress first (old code) |
| Assist tank 0 on Mistresses, 1 on Infernals; each taunts its add back off any other player, tanks included; a lone one takes Mistresses first | guide: pick up the Mistress, rotate taunts on Infernals; script: Fel Streak resets threat, adds 50,000 and picks with `withTank = true`, so it can land on the main tank | one assist tank on the priority add (old code); taunt off non-tanks only |
| Boss stays on `ARENA_CENTER`, not walked to the portal | guide positions him centrally; script summons the portal 15 yd from him | walk him over for cleave |
| One interrupter per cast: lowest guid ready, in range, not mid-cast, `ACTION_EMERGENCY + 9` | brief; `vezax.md`; guide: one assigned interrupter | pull all DPS onto him (old code) |
| No Silencing Shot, Silence or Spell Lock in the list | `creature_immunities` −286 has SILENCE; pet spells skip the cooldown check in `CanCastSpell` | Vezax's list |
| Healers remove Nether Power only with no other remover and no Incinerate up | brief item 3 trap; guide: mages Spellsteal | any mage, priest or shaman (old code) |
| Every healer in range heals Incinerate, relevance kept at `ACTION_MEDIUM_HEAL + 5` | its health reads full so no generic node takes it; critical heals (30) still win | a ranked subset, or above critical |
| Incinerate heals: instants, HoTs and channels first; none with a cast time while the target reads full health | `PlayerbotAI::UpdateAI` cancels a preparing single-target heal on a full-health target, refunding the GCD, so the node recast it every tick and nothing landed | fix the cancel (Carried over) |
| Carrier keeps moving its 8 s: legs of 6+ yd, 6 yd from flames, 8 yd from members, 15 yd from the boss, heading latched forward, flames-only fallback | script: a flame a second, 3 yd, 60 s, first tick after 1 s; guide: run to the edge, keep moving | `FleePosition` off the cluster centre (old code, 5 yd cap) |
| Dodge starts at 4 yd and clears to 6, melee preferring spots by the boss | pitfalls: clear past the trigger radius | — |
| A tank carrier walks the boss with it, flames only (`jaraxxus.flame` `tank`) | script: `withTank = true`; the guide's "non-tank" is wrong here | tank stands and takes every stacked tick |
| The hold's drag tests only its next 5 yd step; a flame there sends the tank around the trail: flames at `_CLEAR`, rings to 5 yd, `preferNear` `ARENA_CENTER`, no step that crosses one | `raid-mechanics-lessons.md`: reject the leg the hazard covers; a tank-carried trail lies along the whole way back for 60 s | wait while the whole route crosses a flame |
| `LegionFlameCrossesPath` lets a bot already inside `_CLEAR` of a flame walk on while it gets no nearer | a tank a few yards off its own last flame could otherwise never step | `_CLEAR` for every flame |
| Main tank hold only while he is in combat | a wipe respawns him attackable at stage 3 with no proximity aggro, so the hold's `Attack` pulled him mid-rez (pitfalls "It outlives the pull as well") | fire on attackable |
| `jaraxxus avoid aoe guard` vetoes `AvoidAoeAction` while any flame is up | its unit-trigger branch fires at `GetDistance` ≤ 3, 5.5 yd centre to centre, past our 4 yd trigger, and moves by `FleePosition` (pitfalls, Freya) | keep it as the fallback with trigger 5.5 and clear 8 |
| Kissed bot casts instants only; a running cast of a kissed or flame-carrying bot is broken by any bot's tick | script: 0.5 s `UNIT_STATE_CASTING` check; the engine yields mid-cast | Aura Mastery with Concentration Aura (Known gaps) |
| Lust at the pull (row stays `{}`) | script: no enrage; the heroic portal at 20 s falls inside it; guide silent | hold it for a volcano |
| Icons: focus cross, assist tank 0 square, 1 diamond; `rti` back to skull once no add lives, per claimed bot | pitfalls: marks never clear, a value one encounter sets leaks into the next | shared cross (old code), which flips when the tank's add is not the focus |
| Taunt guard on Jaraxxus | Thorim and Iron Assembly guards | none |
| One per-instance scan every 200 ms, guids and flame positions | `raid-mechanics-lessons.md`, per-raid cost | `GetFirstAliveUnitByEntry` per call |
| Melee stay in Fel Inferno and the heroic portal/volcano pulses | guide: defensive cooldowns only | step out |
| Fel Lightning spread carried over to `w2-jaraxxus-rest` (Oversized lane) | chain of 3/5 at 10 yd; no arena floor point can be verified offline | build a ring now |
| Carrier sweep adds a `_CLEAR` circle on the carrier's own feet, besides the `_MIN_LEG` floor | script: the next flame lands there; the sweep returns `Position()` for an empty hazard list, so a tank carrier (flames only) would stand still before its first flame | flames only |
| Only a bot within 120 yd (3D) of `ARENA_CENTER` refreshes the shared scan; one further out reads the last scan | a stray bot's 200 yd grid misses part of the floor and would blank adds and flames raid-wide for 200 ms | any bot refreshes |
| Flame mover steps its own `MOVEMENT_FORCED` lock down to COMBAT before replacing a leg in flight, restoring it when nothing was issued | pitfalls "A single mover is refused its own re-target": the next leg would come back `Waiting` | — |
| `TryMoveTo` answering `Duplicate` counts as already walking there: latch refreshed, returns true | `MoveTo` refuses the same point for 5 s | treat as refused |
| Flame mover clears its latches after a 2 s gap between its ticks | `Execute` runs only while the trigger fires, so the "role None" clear rarely runs and a heading would leak into the next Legion Flame | clear on role None only |
| Carrier takes the finished leg's bearing at every leg end, not only on arrival | its own trail (a flame a second) turns the latched spot unclear about 1 s in, so it rarely reaches `_ARRIVE` | arrival only |
| A bot turning carrier drops a latched dodge spot and derives a carrier leg at once | a dodge spot is not a leg away from the raid | finish the dodge |
| Tank tests are `IsTank \|\| IsTank(p, true)` (focus trigger, control-tank movement hold); the assist tank attacks and taunt-checks in one tick, never taunting off itself | pitfalls "IsTank flickers" | `IsTank` only |
| Intro hold multiplier asks no encounter gate, and the intro before `IsMainTank` | `GetJaraxxusInIntro` reads the 200 ms scan, cheaper than the gate or a group walk, and is null outside the intro | gate first |
| Kiss cast hold and control-tank movement hold gate on `ToCEncounterGateOpen`, not live | the kill leaves the Mistresses and Infernals up into stage 4; heroic Mistresses keep kissing, and the pinned-cast breaker stays open for them | live gate |
| Fel Fireball election counts only bots that can pay for the interrupt | `CanCastSpell` probes with `TRIGGERED_IGNORE_POWER_AND_REAGENT_COST`, so a rogue short of energy won the duty and nobody kicked | — |

## Carried over

- `w2-jaraxxus-rest.PLAN.md`: Fel Lightning spread.
- Merge stage: `docs/raids/trial-of-the-crusader/README.md` "Constraints" names Legion Flame as an
  `AvoidCreatureClusterAction` caller; after group 2 only the slime pools remain.
- Merge stage: the arena floor is gameobject 195527 (display 9059), absent from the static navmesh
  and vmaps, so navprobe cannot verify any ToC arena floor point; a raid-wide fact for the README
  (the boss doc's Traps has it).
- Merge stage or `w6-closeout`: the ToC README ("What a trace answers", boss table) and
  `docs/systems/observability.md`'s "Converted" list gain the Jaraxxus reader and `jaraxxus.*`
  probes.
- Merge stage, engine lesson for `docs/engine/pitfalls.md`: a bot mid-cast runs no triggers
  (`UpdateAIInternal` yields while its spell prepares), so a mechanic that punishes an ongoing cast
  is answered only by `RequestSpellInterrupt` from another bot's tick.
- Merge stage, outside ToC: `PlayerbotAI::UpdateAI`'s full-health cancel (`src/Bot/PlayerbotAI.cpp`,
  "Interrupt if target ally has full health") should skip a target carrying an
  `SPELL_AURA_SCHOOL_HEAL_ABSORB` aura. Health reads full under Incinerate Flesh, so every cast-time
  heal on it is cancelled; the lane only works around it (Decisions).

## Known gaps

- No Fel Lightning spread (Carried over).
- No raid cooldown calls: Aura Mastery with Concentration Aura against the Kiss, Divine Sacrifice or
  Fire Resistance Aura through a volcano.
- Melee take Fel Inferno and the heroic portal and volcano pulses.
- `lord_jaraxxus.py` `carrier_windows` judges a tank carrier's repeat window: `jaraxxus.flame` is
  change-only, so a second `tank` window with no switch since the first has no note inside it and
  `tank` reads False. Fix: seed `taken` with the value latched at `start`; test two consecutive `tank`
  windows on one bot with one note (re-review finding, left after the one round).

## Blocked
