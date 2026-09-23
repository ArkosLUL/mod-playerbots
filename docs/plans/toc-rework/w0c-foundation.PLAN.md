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

_Filled by the investigator: numbered tasks in 1-3 groups with disjoint file ownership._

## Decisions for review

## Carried over

## Known gaps

## Blocked
