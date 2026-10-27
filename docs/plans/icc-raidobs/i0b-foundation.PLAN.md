# i0b-foundation — ICC trace names, cheat record, traced instance state

Wave 1, parallel with `i0a-tooling`, which owns `tools/` and `docs/systems/observability.md`. Rules,
sources, naming contract, schema v14 and cheat classification:
[icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). On the default config (`food,taxi,raid`, any
`EnableICCBuffs`), nothing here may change play. The three exceptions are in scope item 6.

## Scope

1. **Trace names.**
   - Give `src/Bot/Obs/` a per-map table of boss id → trace name. A session that the boss-state hook
     opens on a map with a table takes the name outright, rather than the engaged creature's or the
     map's name. Today's open is in `RaidObsLifecycle.cpp`: `OpenSession(map, FindEngagedBoss(map),
     "bossstate")`.
   - A later engage can't rename such a session (only the map-name fallback upgrades). Check that
     this holds and that the `pull` record carries the table's name.
   - ICC registers its 15 contract rows once from `AddSC_IcecrownBotScripts` (`ICCScripts.cpp`).
   - Stinky/Precious and the Rotting Frost Giants have no boss state. Open their pulls with
     `RaidObs::MarkPull` when their trigger fires in combat. Name them with `NamePull` to the contract
     slug, since `MarkPull` names from the creature ("Stinky").
   - Keep `Bot/Obs` raid-agnostic: the table is data a raid hands in.
2. **Cheat record.**
   - `RaidObs::NoteCheat` and the `cheat` record, exactly per schema v14 in the overview.
   - Bump `SCHEMA_VERSION` to 14 in `RaidObs.h`.
   - Add `iccbuffs` to `EnvFieldsJson` (`RaidObsConfig.cpp`), read through the same config the ICC
     code reads.
   - Per-(bot, key) pacing state lives in the session and resets with it.
   - Gate on `Active()` inside the probe and take `char const*`, never a built `std::string`.
3. **ICC cheat helpers and every site.**
   - In `ICCShared`: `IccRaidCheat(botAI, key, target)`, plus a record-only call for the
     `EnableICCBuffs` paths (`IccApplyHeroicBuffToMember` and its callers).
   - Wrap every cheat site in the overview's classification, the ones later lanes remove included.
     Use the fixed keys for Nitro (`<prefix>.nitro`), Spiteful Fury on Rotface (`rf.spitefulfury`),
     the taunt helper (`icc.tauntfixate`, `icc.tauntreset`) and Valithria's speed (`vd.speed`). Name
     the rest `<prefix>.<kind>`.
   - Sites that act on another unit pass it as `target`: Cyclone on a human, Battle Fury on the
     captain, tomb aura strips, Spiteful Fury on a tank, the buffs on members.
   - The 39 ungated sites now sit behind `raid`. The 2 that already check it keep that check through
     the helper.
   - List every key and its sites in the README cheat table.
4. **Traced instance state.**
   - Move the assignments and latches of `IccInstanceState` (`ICCScripts.h`) into `ObsGuidMap`,
     `ObsGuidSet` and `ObsValue`, each keyed with its encounter's contract prefix.
   - Timestamps, throttles, caches and struct values (`WinterStageState`, `StackChoice`,
     `LastLosMove`, `BloatDir`, the goo list) stay bare; boss lanes probe the struct values with
     `NoteDerived`.
   - Rewrite every site that binds a `T&` into a converted field, since `ObsGuidMap::operator[]`
     returns a proxy: `ICCShared.cpp:55,130,155`, `ICCTriggers.cpp:232`,
     `ICCActions_SG.cpp:392,1233`, `ICCActions_BQL.cpp:485-487`, `ICCActions_LK.cpp:2161-2163,3558,3934`
     (the timestamps among them stay bare).
   - Where a stale value would read as a live assignment when a window opens, write an explicit
     `none` (observability.md, "Reading traps").
   - The containers are unordered. Check every iteration over a converted field: today they only
     count or prune.
5. **Listener probes** in `ICCScripts.cpp`:
   - a `NoteHazardCircle` per Malleable Goo impact (12 yd, 8 s, as the triggers model it);
   - the Vile Gas victim under `rf.`;
   - the Defile target under `lk.`, plus a hazard circle at its start radius.
   - Pace them per instance.
   - Check each listener's spell id list against the spelldifficulty CSV and complete it if a
     difficulty is missing. That is a small-bug fix, so list it.
6. **The three allowed behaviour fixes:**
   - `IccGunshipCannonNearTrigger` (`ICCTriggers.h`) names itself `icc in cannon`. That collides with
     the real `icc in cannon` and merges their coverage. Rename it `icc gunship cannon near`, its node
     name.
   - `IccBqlGroupPositionTrigger` names itself `icc bql tank position`. Rename it
     `icc bql group position`.
   - `IccBossStateResetScript` resets state only after an `IsDungeonBoss()` creature leaves combat,
     so Gunship, Valithria and Svalna latches outlive a wipe. Key the reset on the instance's boss
     states instead, keeping the 5 s grace and the wipe-only behaviour.
   - Also drop the orphan creator `icc dbs main tank rune of blood` (`ICCTriggerContext.h`), which
     has no node (`ICCStrategy.cpp` comment).
7. **`docs/raids/icecrown-citadel/README.md`**, new:
   - code layout and registration sites;
   - the boss-state ids and who sets `IN_PROGRESS` per encounter, from the core;
   - the naming contract;
   - the cheat table (key, sites, class, gate);
   - per-instance state and its reset;
   - the trigger-prefix quirks.
   - Add the raid row to `docs/raids/README.md`'s raid table.
   - Fold with `compact-docs-writer`.

## Owns

`src/Bot/Obs/*`; `src/Ai/Raid/ICC/ICCScripts.{h,cpp}`, `ICCShared.{h,cpp}`; the cheat sites and
converted-field sites in every ICC file; the two trigger names and the orphan creator;
`docs/raids/icecrown-citadel/README.md`; the raid row in `docs/raids/README.md`; this brief. Nothing
under `tools/`. No other behaviour change in boss code: the boss lanes own phases, derived probes,
removals and bug fixes.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
