# i2-plagueworks — Stinky and Precious, Festergut, Rotface, Putricide

Wave 2a, parallel with `i1-lower-spire`, which owns `ICCShared.{h,cpp}`. Rules, contract,
classification and the boss lane steps: [icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). Wave 1 is on
`Custom`: the trace names, `IccRaidCheat` at every cheat site, the traced `IccInstanceState`, the
Malleable Goo and Vile Gas listener probes, and `docs/raids/icecrown-citadel/README.md` (read its cheat
table for the keys).

## Scope

Run the boss lane steps for `stinky-and-precious`, `festergut`, `rotface` and `professor-putricide`.

1. **Phases.**
   - Festergut: the spore cycle (a group member carries Gas Spore). The raid-wide spore spot is
     `fgSlotState`.
   - Rotface: no phase.
   - Putricide: phase 3 is `HealthBelowPct(35)` at some sites and 36 at others, and phases 1 and 2 are
     never told apart. Derive all three from the core's thresholds in one helper.
   - Stinky and Precious: no phase.
2. **Decisions to probe.**
   - Rotface: the Big Ooze picker and kiter, the explosion escape slot (`rfExplosionSlotMemory`,
     traced by wave 1), the Vile Gas safe spot, and the Rotface skull (`IccEnsureIconOn`).
   - Putricide: the committed Gaseous Bloat flee direction (`ppBloatLastDir`, a struct left bare by
     wave 1: `NoteDerived` in its helper), and the ooze icons.
   - Festergut: who holds for goo (`festergutGooWaitUntil` is a timestamp; probe the derived hold
     instead).
3. **Cheats.**
   - Remove:
     - Nitro, `AddAura 54861`, at `ICCActions_RF.cpp` ~162, 692, 750, 784, 989 and
       `ICCActions_PP.cpp` ~733. Each becomes `UseNitroBoosts(bot)` (`src/Util/EncounterHelpers.h`).
     - Spiteful Fury for Big Ooze pickup at `ICCActions_RF.cpp` ~148, 235, 248. It becomes
       `IccCastClassTaunt`. Check in the core that a Big Ooze can be taunted and keeps threat. If it
       can't, the site is not simple: keep it and log why.
   - Keep, with each boss doc's gap entry:
     - Volatile Ooze and Gas Cloud kills (`ICCActions_PP.cpp` ~521, 680). First check in the core why
       more than one exists.
     - Unbound Plague suicide (~1327).
     - Regurgitated Ooze without its energy cost (~1555).
   - Record-only: Putricide's `EnableICCBuffs` sites (`ICCTriggers.cpp` ~394-409).
4. **Bugs.**
   - `SPELL_VILE_GAS_H` 69240, `SPELL_ACID_BURST` 70744, `SPELL_MALLEABLE_GOO_25N` 72297 and
     `SPELL_MALLEABLE_GOO_10H` 74280 (`ICCTriggers.h`) each match one difficulty. Cover all four per
     the spelldifficulty CSV.
   - `IccFestergutMainTankGastricBloatTrigger` (`ICCTriggers.h`) is declared with no creator and no
     body. Delete it.
   - Rotface's member-state timers use `time(nullptr)` seconds (`ICCTriggers.h`,
     `IccRotfaceMoveAwayFromExplosionTrigger`). Move them to ms game time if that is local.

## Owns

`Action/ICCActions_Dogs.cpp`, `_FG.cpp`, `_RF.cpp`, `_PP.cpp`, and their regions of the shared ICC
files. Readers `bosses/stinky_and_precious.py`, `festergut.py`, `rotface.py`,
`professor_putricide.py` with their tests. Docs `docs/raids/icecrown-citadel/stinky-and-precious.md`,
`festergut.md`, `rotface.md`, `professor-putricide.md`. This brief. Not `ICCShared.{h,cpp}`.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
