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

Sized as one lane: most of the work sits in the shared ICC files, which a split would leave to the
finisher. All done:

- [x] Phases: `IccFestergutSporeCycle` (`fg.phase`) and `IccPutricidePhase` (`pp.phase`) in
  `ICCTriggers`, every phase test routed through them.
- [x] Probes: `fg.sporespot`, `fg.goohold`, `rf.bigooze`, `rf.skull`, `rf.vilegasspot`, `pp.skull`,
  `pp.bloatflee`.
- [x] Cheats: `rf.nitro` (5) and `pp.nitro` (1) to `UseNitroBoosts`; `rf.spitefulfury` (3) removed,
  the class taunts beside it stay. Gap sites and the buff sites unchanged.
- [x] Bugs: Acid Burst remap, unused goo constants, dead Gastric Bloat trigger, Rotface explosion timer
  in ms, Putricide 35/36 split, stale Vile Gas spot, stale Gaseous Bloat direction.
- [x] Readers `festergut.py`, `rotface.py`, `professor_putricide.py`, `stinky_and_precious.py`, tests.
- [x] Docs `stinky-and-precious.md`, `festergut.md`, `rotface.md`, `professor-putricide.md`.
- [x] Checks: syntax (lane `.cpp`, then header includers and the `BuildShared*` contexts), pblint 22/23
  warnings (was 23/25), botobs suite, codestyle.

## Decisions for review

- **Putricide phase from health**, mirroring `DamageTaken`: 1 above 80%, 2 above 35%, 3 at 35% and
  under. Source: `boss_professor_putricide.cpp`. Alternative: `AI()->GetData(2)` (`DATA_PHASE`, a
  file-local enum to mirror), his own `_phase`, which flips at `ChangePhase` a tick or a cast later.
- **Phase and decision probes are per-bot `NoteDerived`.** Source: `IccInstanceState` is in
  `ICCScripts.h`, outside this lane, and derived state is `NoteDerived` per observability.md;
  `icecrown_citadel.py` merges bots. Alternative: `ObsValue` fields there, one row per change.
- **Spiteful Fury removal is simple.** Source: Big Ooze immunity set -361 carries neither aura 11 nor
  effect 114, and its threat resets only at its first update. Alternative: keep it as a gap, since the
  assist tank never attacks an ooze (Known gaps).
- **`pp.freeooze` stays a gap**: the legit answer is a duty cycle eating to 45 energy before each cast.
  Alternative: an energy gate at the site, which changes the abomination's play on the default config.
- **`pp.oozekill`, `pp.gascloudkill` stay gaps**: a second add of one entry lives only when the last
  one outlived the next experiment of its kind (70-80 s). `pp.unboundplague` stays: the answer is
  passing the plague, a new duty.
- **Icon probes write only when the bot moves the icon**, rule plus unit, change-latched per bot.
  Alternative: `RaidObs::Note` per move, no dedup but unbounded if a human and a bot fight over it.
- **`pp.bloatflee` names the search branch**, not the direction, and the gas cloud trigger forgets a
  non-bloated bot's direction. Source: each bloat is a new cast on a new victim. Alternative: a
  direction rounded to the octant, still churning every leg.
- **Vile Gas spot resets after 2 s idle**, as Festergut's spore slot does: the action stops running
  with its trigger, so its own close branch never ran. Alternative: key on the listener's cast time,
  which misses bots running from the splash.
- **Goo constants**: deleted the unused `SPELL_MALLEABLE_GOO_25N/_10H/_25H` and the duplicate enum
  `SPELL_MALLEABLE_GOO`; `_10N` keeps its name for `ICCScripts.cpp`. Vile Gas needed a comment fix
  only, the listener already remaps.
- **Acid Burst fixed at its only site, `ICCActions_VT.cpp`** (i4's file), as this brief assigns it.
- **Readers**: no phase section for Rotface and the dogs; banners spare heroic-only keys on normal.
- **Explosion trigger** keeps a `_castEnded` flag rather than a 0 sentinel, 0 being a real `getMSTime`.

## Carried over

- **Merge stage, ICC README**: delete the `rf.nitro`, `pp.nitro`, `rf.spitefulfury` cheat rows, and
  link the four boss docs.
- **Lanes with Nitro left** (i4, i5): `SPELL_NITRO_BOOSTS` stays in `ICCTriggers.h` for them;
  `SPELL_SPITEFULL_FURY` stays for the buffs.
- **i6-closeout**: the four readers repeat banner, refusal and cheat code a shared ICC reader helper
  could hold; `icecrown_citadel.py --phases` reads "rf.phase never written" on encounters with no
  phase.

## Known gaps

In the boss docs: the assist tank holds a Big Ooze by the taunt alone; skulls by raid icon (Rotface,
Putricide); `pp.freeooze` off still claims the tick; Decimate and Festergut's Vile Gas unanswered;
`FindAssignedBigOoze` has no caller; no ICC trace carries these probes yet.

## Blocked
