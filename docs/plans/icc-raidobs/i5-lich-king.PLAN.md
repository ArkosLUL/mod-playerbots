# i5-lich-king — The Lich King

Wave 2c, alone. Rules, contract, classification and the boss lane steps:
[icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). Waves 1, 2a and 2b are on `Custom`. Read
`docs/raids/icecrown-citadel/README.md` for the cheat keys and for the shared taunt-helper change 2a
made. `ICCActions_LK.cpp` is about 4.4k lines. Split it only if the work falls into large independent
parts (for example phases and probes vs. cheats and unstucks).

## Scope

Run the boss lane steps for `the-lich-king`.

1. **Phases.** Today's health bands disagree: phase 1 above 70 or 71%, Val'kyr until below 40%, the
   spirit phase 43-37%, the last phase below 37%, phase 3 below 40% with no Winter. Remorseless Winter
   is `IccBossHasRemorselessWinter` (`ICCShared.cpp`), and Frostmourne is Terenas within 55 yd. Derive
   one phase helper from the core script's transitions, move every site onto it, and log each
   threshold that moved.
2. **Decisions to probe.** All of these are traced by wave 1 or left bare for `NoteDerived` in their
   helper:
   - the Val'kyr buckets (`IccBalancedGroupSizes` / `IccAssignedBucketIndex`);
   - the Winter staging spot (`winterStage`);
   - the inbound latches;
   - the main-tank stack slot (`mtStackChoice`);
   - the shared Vile Spirit slot (`lkSharedSlot`);
   - the Defile unsafe spot;
   - the skull, star and Val'kyr icons.
   The Defile target and its hazard come from the wave-1 listener.
3. **Cheats.**
   - Remove:
     - Nitro, `AddAura 54861`, at `ICCActions_LK.cpp` ~377, ~3565 and ~4054. Each becomes
       `UseNitroBoosts(bot)`. ~3565 also reached human Defile targets.
     - ~4054's Pain Suppression and Aegis on the assist tank, with no replacement.
   - Keep, with gap entries:
     - the Raging Spirit kill by the main tank (~382);
     - the Wicked Spirit kill by ranged in the Frostmourne room (~2420);
     - Hammer of Justice from any class on Shambling Horrors (~2715).
   - Document only: the Val'kyr drop suicide at the platform edge (~1962-1991).
   - Record-only: the `EnableICCBuffs` sites (~176, ~373, ~1810-1825, ~3833).
4. **Bugs.**
   - `SPELL_HARVEST_SOUL_LK` 68980 (`ICCTriggers.h`) matches one difficulty. Cover all four.
   - The unstucks at ~661, ~2043-2071, ~2075-2082, ~2087-2106 and ~2146-2151 use a same-map
     `TeleportTo`, which drops combat. Make them `NearTeleportTo`.
   - The Winter platform Z check at ~661 has a 1 yd tolerance, so it fires on ordinary bobbing. Set it
     from the platform geometry; a navprobe reading is needed for a new value.
   - Every Lich King trigger bails while Valithria is within 100 yd, because her room has a "The Lich
     King" voice NPC (entry 16980). Keep the guard and make sure the phase helper doesn't lose it.

## Owns

`Action/ICCActions_LK.cpp` and its regions of the shared ICC files, plus `IccBossHasRemorselessWinter`
in `ICCShared.cpp`. Reader `bosses/the_lich_king.py` with its tests. Doc
`docs/raids/icecrown-citadel/the-lich-king.md`. This brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
