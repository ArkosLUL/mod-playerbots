# w0c-foundation — encounter gate, spell ids, multiplier gating, burst rows

Wave 2, parallel with `w0b-tooling`, which owns `tools/` and `src/Bot/Obs/`. Rules, sources,
naming contract and checks: [toc-rework.PLAN.md](toc-rework.PLAN.md). The code layout and seams are
in `docs/raids/trial-of-the-crusader/README.md`.

## Scope

1. **`ToCEncounterGate`.**
   - Stage-open gate: `GetData(TYPE_INSTANCE_PROGRESS)` matches the encounter's stage per the naming
     contract. Mirror the core values with a comment naming `trial_of_the_crusader.h`.
   - `ToCEncounterIsLive`: stage matches, `IsEncounterInProgress()` is true, and the encounter's
     unit is in combat.
     - Resolve units via `GetGuidData` + `GetCreature` where the script exposes them: Gormok 4,
       Dreadscale 6, Acidmaw 7, the twins by NPC entry, Anub'arak 13 (this works while he is
       submerged).
     - Otherwise resolve by entry among nearby units.
   - "Live" keeps the walk-ins, the Fizzlebang intro (stage 2) and the Lich King fall (stage 8)
     closed. "Open" keeps pre-pull prep working, such as taking an essence before the Twins.
2. **Trigger wrapper**, modelled on `UldGatedTrigger`.
   - Wrap each trigger where the contexts build it, copying the inner trigger's name and check
     interval. A default interval of 1 would promote throttled triggers; a value of 2-99 means
     seconds.
   - When it fires while live, call `RaidObs::NamePull(map, <slug>)`. That is cheap, and correct in
     the one case it can act on.
   - Add a `toc.progress` `ObsValue`, per instance behind a mutex like Mimiron's `MimironObsState`.
     Never `thread_local`.
3. **Multiplier gating.** An encounter-live helper, memoised per ms, gates all nine multipliers.
   Test the action family before anything costly, because a multiplier runs for every popped action.
   `FactionChampionsEncounterActive` walks the whole target list per action today.
4. **Spell ids.**
   - Use plain `constexpr uint32`, with either all four difficulty variants or
     `sSpellMgr->GetSpellIdForDifficulty`, at every `HasAura` and `FindCurrentSpellBySpellId` site.
   - Known ids:
     - Light Essence 65686/67222/67223/67224, Dark Essence 65684/67176/67177/67178.
     - Touch: 67297/67282 are 10H only; 25H uses 67298/67283.
     - Leeching Swarm 66118 → 67630/68646/68647.
   - Check the rest against the CSV: Impale, Burning Bite/Spray, Incinerate Flesh, Fel Fireball,
     Light/Dark Vortex, Twin's Pact, Permafrost, Penetrating Cold.
   - The pblint fix lands in parallel in `w0b`, so verify against the CSV directly.
   - List every node this makes live in 25N/10H/25H, in the README's per-boss notes and the report.
     Boss lanes verify them.
5. **`ToCBurstWindowMultiplier`.**
   - One multiplier, keyed off the live encounter, dispatching to a per-boss row function in each
     boss stem (the seam `w0a` reserved).
   - Match by action name (`IsBurstCooldownAction`, see `docs/systems/consumables-and-burst.md`),
     exempt `IsManaReturnCooldown`, and memoise per ms.
   - Port `AnubarakDelayBloodlustUntilLeechingSwarmMultiplier` unchanged as the Anub'arak row, then
     delete the old class.
   - Other rows default to allow; the base gate still applies.

## Owns

Everything under `src/Ai/Raid/ToC/`, the ToC boss docs and `docs/raids/trial-of-the-crusader/README.md`
(handed to this lane: gate, stages, the spell-id table). Not `tools/` or `src/Bot/Obs/`.

## Task list

Verified facts behind these tasks are in `docs/raids/trial-of-the-crusader/README.md` (stages,
`IsEncounterInProgress`, `GetGuidData`, spell-id table) and the Beasts/Anub'arak docs (detection
traps). The groups share three contracts; code against them, the integrate step compiles them
together:

- **Gate API** (group 1 writes, group 2 calls), global scope, `Util/ToCEncounterGate.h`:
  `enum class ToCEncounter : uint8 { None, NorthrendBeasts, Jaraxxus, FactionChampions, TwinValkyr, Anubarak };`
  `bool ToCEncounterIsLive(PlayerbotAI*, ToCEncounter);` `ToCEncounter ToCLiveEncounter(PlayerbotAI*);`
- **Spell constants** (group 3 writes, group 2 reads): `enum class ToCSpells` becomes plain
  `constexpr uint32 SPELL_*` in `TrialOfTheCrusaderHelpers`, same names, so `SPELL_MARK` stays in
  `ToCData.h`; `bool HasLightTouch(Unit*)` and `bool HasDarkTouch(Unit*)` join
  `ToCHelpers_TwinValkyr.h`.
- **Champion roster** (group 2 owns the file, group 1 reads): the `ToCFactionChampions` enum and
  `IsFactionChampion`/`IsFactionChampionHealer` stay unchanged.

### Group 1 — gate

Files: `src/Ai/Raid/ToC/Util/ToCEncounterGate.h` (new), `src/Ai/Raid/ToC/Util/ToCEncounterGate.cpp`
(new), `src/Ai/Raid/ToC/ToCTriggerContext.h`, `docs/raids/trial-of-the-crusader/README.md`.

1. **Header.** The API above plus `bool ToCEncounterOfTrigger(std::string const&, ToCEncounter&)`,
   `char const* ToCEncounterSlug(ToCEncounter)`, `bool ToCEncounterGateOpen(PlayerbotAI*, ToCEncounter)`
   and `class ToCGatedTrigger`. Mirror the core values with a comment naming `trial_of_the_crusader.h`
   (`DataTypes`, `Progress`, `NPCs`): progress type 1; guid types Gormok 4, Dreadscale 6, Acidmaw 7,
   Anub'arak 13; twins keyed by entry 34497/34496. Stage → encounter: 0-1 Beasts, 2-3 Jaraxxus,
   4 Faction Champions, 6 Twins, 9 Anub'arak, anything else none.
2. **Per-instance state** in a `RaidInstanceState<ToCGateState>`: `RaidObs::ObsValue<uint32>
   progress{"toc.progress"}` plus a memo (ms stamp, valid flag, stage, live encounter), refreshed at
   most once per instance per `getMSTime()` ms. A refresh: bot on map 649 with an instance script,
   else no ToC instance; `stage = GetData(1)`, `progress = stage`; live = the stage's encounter when
   `IsEncounterInProgress()` and one of its units is alive and `IsInCombat()`:
   - Beasts: `map->GetCreature(GetGuidData(4|6|7))`, plus Icehowl 34797 by entry;
   - Jaraxxus 34780 by entry; Twins `GetGuidData(34497|34496)`; Anub'arak `GetGuidData(13)`;
   - Faction Champions: one `GetCreatureListWithEntryInGrid(list, entries, radius)` over the 24
     `ToCFactionChampions` entries.

   Entry-only lookups are bot-anchored with a 200 yd radius: the arena reaches about 80 yd from
   `ARENA_CENTER` (gate 77 yd, stands 64 yd), so any floor point sees all of it.
3. **Verdicts.** `ToCEncounterGateOpen`: the stage's encounter is this one; true with no ToC instance,
   as `UldEncounterGateOpen` leaves an unknown map ungated. `ToCEncounterIsLive`/`ToCLiveEncounter`
   read the memo; none/false with no ToC instance.
4. **`ToCGatedTrigger`**, modelled on `UldGatedTrigger` without the pass-id machinery: name and
   check interval copied from the inner trigger, destructor deletes it, every other virtual forwards.
   `Check` returns an empty event while closed; on a fired event while `RaidObs::Active()` and live,
   `RaidObs::NamePull(bot->GetMap(), ToCEncounterSlug(enc))`. Prefixes: `gormok`, `northrend worms`,
   `icehowl` → Beasts; `jaraxxus`; `faction champions`; `twin valkyr`; `anubarak`. Slugs per the
   naming contract.
5. **`ToCTriggerContext.h`**: `Absorb` wraps each creator whose name has an encounter, as
   `UldTriggerContext.h` does, keeping the duplicate-key log.
6. **README** (fold with `compact-docs-writer`): code layout gains the gate file, open versus live,
   the wrapper and `NamePull`; the burst-row seam gains `ToCBurstWindowMultiplier` dispatching on
   the live encounter; the `ToCData.h` constraint says spell constants are plain `constexpr uint32`
   (pblint's remap check reads a bare `GetSpellIdForDifficulty(SPELL_X`) and new boss ids go in the
   stem helper; a "What a trace answers" line for `toc.progress` (stage per the table, on change,
   attributed to the bot whose tick read it).

### Group 2 — multipliers and burst

Files: `src/Ai/Raid/ToC/Multiplier/ToCMultipliers_Shared.h`,
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_Shared.cpp` (new),
`src/Ai/Raid/ToC/Multiplier/ToCMultipliers_{Anubarak,FactionChampions,Icehowl,Jaraxxus,NorthrendBeasts,TwinValkyr}.{h,cpp}`,
`src/Ai/Raid/ToC/ToCStrategy.cpp`, `src/Ai/Raid/ToC/Util/ToCHelpers_FactionChampions.{h,cpp}`.

7. **Gate the eight remaining multipliers.** Order: action family, then cheap role or aura tests,
   then `ToCEncounterIsLive(botAI, <encounter>)`, then the existing lookups. Families: the four
   control-tank multipliers `dynamic_cast<CombatFormationMoveAction*>`; spike kite, Icehowl charge and
   essence swap `CastReachTargetSpellAction` or a `MovementAction` other than their own action (the
   old chain's `CombatFormationMoveAction`/`AvoidAoeAction` are `MovementAction`s, so verdicts do
   not change); Faction Champions AoE `getThreatType() == Aoe` and not `CastHealingSpellAction`.
   Verdicts are otherwise unchanged; the blanket vetoes belong to the boss lanes.
8. **Spell reads** in these files use the group 3 contract: `bot->HasAura(SPELL_MARK)`,
   `HasLightTouch(bot)`, `HasDarkTouch(bot)`.
9. **Delete `FactionChampionsEncounterActive`** (its only caller was the AoE multiplier).
10. **`ToCBurstWindowMultiplier`** ("toc burst window") in `ToCMultipliers_Shared.{h,cpp}`, modelled
    on `UlduarBurstWindowMultiplier`: return 1 unless `IsBurstCooldownAction(name)` and not
    `IsManaReturnCooldown(bot, name)` and in combat; memoise per bot per ms the row of
    `ToCLiveEncounter` (`ToC<Stem>BurstWindow` for NorthrendBeasts, Jaraxxus, FactionChampions,
    TwinValkyr, Anubarak; none gives `{}`); `bloodlust`/`heroism` read `allowLust`, the rest
    `allowAll`.
11. **Anub'arak row** `{true, AnubarakLeechingSwarmActive(botAI)}`, the old gate's rule; delete
    `AnubarakDelayBloodlustUntilLeechingSwarmMultiplier` with its registration and now-unused
    includes. The other rows stay `{}`.
12. **`ToCStrategy.cpp`** pushes `new ToCBurstWindowMultiplier(botAI)` after the stem lists.

### Group 3 — spell ids

Files: `src/Ai/Raid/ToC/Util/ToCData.h`, `src/Ai/Raid/ToC/Util/ToCHelpers_Anubarak.{h,cpp}`,
`src/Ai/Raid/ToC/Util/ToCHelpers_Gormok.cpp`, `src/Ai/Raid/ToC/Util/ToCHelpers_Icehowl.cpp`,
`src/Ai/Raid/ToC/Util/ToCHelpers_Jaraxxus.{h,cpp}`, `src/Ai/Raid/ToC/Util/ToCHelpers_Jormungars.cpp`,
`src/Ai/Raid/ToC/Util/ToCHelpers_TwinValkyr.{h,cpp}`,
`src/Ai/Raid/ToC/Trigger/ToCTriggers_{Anubarak,Jaraxxus,Jormungars,TwinValkyr}.cpp`,
`src/Ai/Raid/ToC/Action/ToCActions_{Anubarak,Jaraxxus,TwinValkyr}.cpp`,
`docs/raids/trial-of-the-crusader/{northrend-beasts,anubarak}.md`.

13. **Constants.** `enum class ToCSpells` → plain `constexpr uint32 SPELL_*` in
    `TrialOfTheCrusaderHelpers`, names kept; rewrite every `static_cast<uint32>(ToCSpells::X)` to `X`.
    `SPELL_SWEEP_0/1` become one `SPELL_SWEEP = 66794`. Touch constants become the 10N row ids 65950
    and 66001, commented heroic-only. Replace the false "bite/spray carry the debuff aura" comment.
    New boss ids go in the stem helper (`SPELL_FROST_SPHERE = 67539` in `ToCHelpers_Anubarak.h`);
    drop `SPELL_PERMAFROST` once unused.
14. **Remap each site** as `sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)` written at the call,
    never through a wrapper (pblint's check reads the bare identifier):
    - Impale: `GetGormokImpaleStacks`.
    - Sweep: `GetWormCastingSweep`, one lookup.
    - Incinerate Flesh: new `HasIncinerateFlesh(Unit*)` in `ToCHelpers_Jaraxxus`, used by the trigger
      and the heal action.
    - Fel Fireball: its trigger.
    - Essences, Vortex, Twin's Pact: the `ToCHelpers_TwinValkyr` readers.
    - Touch: new `HasLightTouch`/`HasDarkTouch`, used by the trigger, the swap action and group 2.
    - Nether Power, Massive Crash: keep the four-id lists; Mark, Submerge: no row, rename only.
    - Burning Bite/Spray: rename only, `WormsAfflictedByBurningTrigger` stays as keyed (Decisions).
15. **Anub'arak detection**, per `anubarak.md`: in `ToCHelpers_Anubarak`, `IsFrostSphereFlying`
    (alive, `SPELL_FROST_SPHERE` aura, not `UNIT_FLAG_NOT_SELECTABLE`) and `IsPermafrostPatch` (alive,
    no `SPELL_FROST_SPHERE`). `GetNearestPermafrost` uses the patch test; the seed trigger and
    `AnubarakDestroyFrostSphereAction` the flying test. `AnubarakLeechingSwarmActive` reads the
    remapped Leeching Swarm on the bot, or boss health below 30%, and drops the boss aura test.
16. **Boss docs** (fold with `compact-docs-writer`): drop the Sweep and Permafrost known gaps; in
    `anubarak.md` replace the `AnubarakDelayBloodlustUntilLeechingSwarmMultiplier` sentence with the
    lust rule's new home (the Anub'arak row of `ToCBurstWindowMultiplier`: lust waits for Leeching
    Swarm or his health below 30%).

### Integrate

Compile the three contracts together; re-run CMake (two new `.cpp`); run the checks per the
overview. Nothing changes at the four registration sites.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Triggers gate on "open" (stage); multipliers, burst rows and `NamePull` on "live"; no ToC instance leaves triggers open | brief; `UldEncounterGateOpen` | close everything off map 649 |
| Beasts stay open at stage 2 and Jaraxxus at stage 4 until the next encounter is live | review: the kill moves the stage while mounted snobolds (`npc_snobold_vassal::DoAction(1)` spares them) and the Mistress/Infernals (summoned by portal and volcano, outside `summons`) live on; the next pull follows a gossip and a 30 s+ scene | a known gap in both boss docs |
| Memo and `toc.progress` share one `RaidInstanceState` entry, refreshed once per instance per ms | `raid-mechanics-lessons.md` (ms-stamped scans, per-instance state, never `thread_local`) | a per-bot `thread_local` pass like Uld's |
| Icehowl, Jaraxxus and champions resolved by a bot-anchored 200 yd grid search | `Locs`/`FactionChampionLoc` in `trial_of_the_crusader.h`: the arena reaches ~80 yd from centre | `possible targets` (lags the pull, needs bot combat) or `GetFirstAliveUnitByEntry` (deprecated, per-bot sweep) |
| Plain `constexpr` constants with the lookup at each call; four-id lists kept where already right | brief; pblint's regex needs a bare identifier | four-id lists everywhere |
| `northrend worms afflicted by burning` left dead | Burning Bite/Spray carry no aura; Burning Bile 66869 is the debuff, and its 10 yd pulse strips Paralytic Toxin (`spell_linked_spell`) | re-key on 66869, which would start a 24 s keep-moving at `ACTION_EMERGENCY + 5` the mechanic does not call for |
| Permafrost patch = sphere without Frost Sphere 67539; flying = 67539 and selectable. Brings the spike kite's Permafrost branch to life on every difficulty | `npc_frost_sphere` in `boss_anubarak_trial.cpp`; `DynObjAura::FillTargetMap` and `_IsValidAttackTarget` skip the caster | remap the Permafrost id only, which still never matches |
| Leeching Swarm read on the bot, health fallback kept | `UnitAura::FillTargetMap` never applies effect 129 to its owner | remap the boss aura test, which still never matches |
| Burst rows: Anub'arak holds lust until Leeching Swarm (ported), the others allow | brief. The guides put lust at the Faction Champions opener and in a same-colour Twins shield special, and all offensive cooldowns in Icehowl's stun: boss lanes' rows | tune now |
| Touch constants on the 10N row ids 65950/66001 | CSV rows 464 and 441; any id in a row maps | keep 67297/67282, which map too |
| Only `toc.progress`, no live-encounter probe | brief | add `toc.live` for readers |
| Champion lookup covers all 28 roster entries (14 per faction) | `NPCs` in `trial_of_the_crusader.h`; the 24 in task 2 and the overview is a miscount | the 24 named |
| Lust matched by action name `bloodlust`/`heroism`; the multiplier returns 1 out of combat | `consumables-and-burst.md` (names, not `dynamic_cast`); same actions as `CastBloodlustAction`/`CastHeroismAction` (`ShamanActions.h:112-124`); `UlduarBurstWindowMultiplier` | keep the `dynamic_cast` gate |
| Essence swap gates on `ToCEncounterIsLive(TwinValkyr)`, not `TwinValkyrEncounterActive` (either twin alive); the helper stays for `ToCTriggers_TwinValkyr.cpp` | task 7 | keep either-twin-alive |
| Cheapest test first, verdicts unchanged: control-tank multipliers test live before the victim entry, `needSwap` Touch before the Vortex lookups, `AnubarakLeechingSwarmActive` the bot aura before the boss; Sweep and Twin's Pact remapped once per call against the bot | task 7; any unit on map 649 remaps alike | the old order; a remap per worm or twin |
| Anub'arak stays live while submerged on `IsInCombat` alone; the live test ignores selectability | `EVENT_SUBMERGE` in `boss_anubarak_trial.cpp` sets `UNIT_FLAG_NOT_SELECTABLE` and casts 65981, never `CombatStop` | also accept `AnubarakSubmerged` |
| `ToCData.h`'s Burning Bite/Spray comment ("on any difficulty") trips pblint's `difficulty_is_discussed` skip, so the dead keys raise no `--spell-difficulty` warning | the comment is accurate; the keys are dead on purpose (row above); CSV rows 607/608 exist | reword it and take the warning |

The overview guide carries no tactics; the boss guides were read only for burst timing, which stays
with the boss lanes. No coordinates are proposed, so no navprobe run.

## Nodes made live

For the boss lanes to verify; each misread the listed difficulties before.

- `gormok tank swap needed` (Impale): 25N/10H/25H.
- `northrend worms sweep frontal`: 25N/10H.
- `jaraxxus incinerate flesh on raid` and `JaraxxusHealIncinerateTargetAction`: 25N/10H/25H.
- `jaraxxus fel fireball interruptible`: 25N/10H/25H.
- `twin valkyr needs initial essence` (kept firing after the essence), `… vortex requires essence`,
  `… pact interruptible`, the essence-swap multiplier's essence reads: 25N/10H/25H.
- `twin valkyr touched requires essence` and the swap multiplier's Touch reads: 25H.
- `anubarak pursued by spike` now finds a Permafrost patch; `anubarak ranged should seed permafrost`
  and `anubarak destroy frost sphere` skip a grounded or falling sphere: every difficulty.
- Anub'arak lust row opens on Leeching Swarm on the bot, not only the 30% fallback: every difficulty.

## Carried over

- `w1b-jormungars`: re-key `northrend worms afflicted by burning` on Burning Bile 66869 and replace
  keep-moving with the Bile-to-Toxin cleanse (`northrend-beasts.md`).
- Merge stage or `w6-closeout`: `docs/systems/consumables-and-burst.md` (lines 20, 79) still names
  Anub'arak's `dynamic_cast` lust gate, which group 2 deletes.
- Engine lesson for `docs/engine/pitfalls.md` (merge stage): an enemy area aura (effect 129) is
  never applied to its owner and a persistent area aura never to its caster, so `HasAura` on that
  unit never fires.
- Merge stage: the overview's "Trace naming" says "whichever of 24"; the champion roster is 28.
- Merge stage: per-instance state is `RaidInstanceState`, so nothing for `w6-closeout` to migrate.
- User: re-run CMake before building; `Util/ToCEncounterGate.cpp` and
  `Multiplier/ToCMultipliers_Shared.cpp` are new.
- Merge stage or `w6-closeout`: `pb-syntax-check` of `src/Bot/Engine/BuildSharedStrategyContexts.cpp`
  fails on `Custom` too. `RaidStrategyContext.h`'s `#include "ToCStrategy.h"` resolves to
  `src/Ai/Dungeon/TOC/TOCStrategy.h` on the case-insensitive mount (the README's collision
  constraint), leaving `RaidTrialOfTheCrusaderStrategy` undeclared. Fix: include it by path, as
  `BuildShared{Action,Trigger}Contexts.cpp` do. Not a lane file.

## Known gaps

- `northrend worms afflicted by burning` has never fired (Carried over), also in
  `northrend-beasts.md`.

Open issues after re-review: none. Deferred findings: none.

## Blocked
