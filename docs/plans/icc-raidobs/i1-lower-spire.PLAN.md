# i1-lower-spire — Marrowgar, Deathwhisper, Rotting Frost Giant, Gunship, Saurfang

Wave 2a, parallel with `i2-plagueworks`. Rules, contract, classification and the boss lane steps:
[icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). Wave 1 is on `Custom`: the trace names, `IccRaidCheat` at
every cheat site, the traced `IccInstanceState`, and `docs/raids/icecrown-citadel/README.md` (read its
cheat table for the keys).

## Scope

Run the boss lane steps for `lord-marrowgar`, `lady-deathwhisper`, `rotting-frost-giant`,
`icecrown-gunship-battle` and `deathbringer-saurfang`, plus the shared work below.

1. **Shared taunt helper and heroic-buff cleanup (`ICCShared.cpp`).** Ten bosses call these, so the
   change lands here once and the README records it for the later lanes.
   - `IccCastClassTaunt` loses the `raid` branch's 1M threat plus fixate (`icc.tauntfixate`) and the
     taunt-cooldown reset (`icc.tauntreset`). It keeps only the real taunt. A taunt on cooldown
     fails, and the caller's 500 ms throttle retries.
   - Read all 24 callers for one that assumed an instant re-taunt or a permanent fixate. Note what
     each such caller now risks in its boss doc; don't redesign it.
   - `IccStripExperienced` strips human group members too, so nobody keeps Experienced (71188) or 1%
     Threat (70115) once the heroic fight ends.
2. **Phases.**
   - Marrowgar: Bone Storm, from the boss's aura.
   - Deathwhisper: the Mana Barrier phase is read as `HealthAbovePct(95)` today. Check the core
     (phase 2 starts when the barrier's mana runs out) and derive it the core's way.
   - Gunship: fight active while an enemy captain lives; also which ship the bot stands on.
   - Saurfang: no phase. Publish Blood Power or Rune of Blood state only if a reader needs it.
3. **Decisions to probe.**
   - Marrowgar: the spike buckets (`IccBalancedGroupSizes` / `IccAssignedBucketIndex`,
     `ICCActions_LM.cpp`) and the Bone Storm ranged picks (`PickBoneStormRangedTargets`).
   - Saurfang: the ranged slot (`dbsBotSlotMemory`, traced by wave 1) and the Blood Beast icon
     (`IccEnsureIconOn`).
   - Deathwhisper: the add icons.
   - Gunship: the captain skull and the ranged star.
4. **Cheats.**
   - Remove the shared taunt pair (above).
   - Keep, with each boss doc's gap entry:
     - Dominate Mind Cyclone on self and humans (`ICCActions_LDW.cpp` ~140-160);
     - Touch of Insignificance strip (~168);
     - Death Plague strip on the Rotting Frost Giant (`ICCActions_LDW.cpp` ~768);
     - Battle Fury strip off the captain (`ICCActions_GSB.cpp` ~341).
   - For Death Plague and Battle Fury, check the code comment's "core is bugged" claim against the
     core script. Write the verdict in the boss doc, with issue text if it holds. Never edit the core.
   - Record-only: Deathwhisper's `EnableICCBuffs` sites (~133-137).
5. **Bugs.**
   - The Gunship unstuck (`ICCActions_GSB.cpp` ~200) uses a same-map `TeleportTo`, which drops
     combat. Make it `NearTeleportTo`, and put the bot back over its own deck spot if the core allows.
   - pblint warns that `IccDbsMainTankRuneOfBloodTrigger` (`ICCTriggers.cpp`) calls
     `IsAssistTankOfIndex` without `ignoreDeadPlayers`. Wave 1 dropped its creator, so delete the dead
     class if nothing else uses it.

## Owns

`Action/ICCActions_LM.cpp`, `_LDW.cpp`, `_GSB.cpp`, `_DBS.cpp`, and their regions of the shared ICC
files. `ICCShared.{h,cpp}` for scope item 1. Readers `bosses/lord_marrowgar.py`,
`lady_deathwhisper.py`, `rotting_frost_giant.py`, `icecrown_gunship_battle.py`,
`deathbringer_saurfang.py` with their tests. Docs `docs/raids/icecrown-citadel/lord-marrowgar.md`,
`lady-deathwhisper.md`, `rotting-frost-giant.md`, `icecrown-gunship-battle.md`,
`deathbringer-saurfang.md`. This brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
