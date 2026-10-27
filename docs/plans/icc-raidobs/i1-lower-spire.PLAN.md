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
1b. **Frost Giant prefix `icc.` → `rfg.`** (user's call after wave 1). Under the shared raid-wide
   `icc.` prefix, `--probes` reports every giant key as silent on every ICC pull. Move all giant keys
   to `rfg.`: the C++ keys (`icc.deathplague` → `rfg.deathplague`, `ICCActions_LDW.cpp` ~781, plus any
   giant probe), `ICC_KEY_PREFIXES["rotting-frost-giant"]` and its comment in
   `tools/botobs/raidobs/encounter.py`, the tests that pin it, the README's naming and cheat tables,
   and any `icc.` giant mention in `docs/systems/observability.md`. `icc.` stays raid-wide only.
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
   - The dead `IccDbsMainTankRuneOfBloodTrigger` class is already gone (wave 1).

## Owns

`Action/ICCActions_LM.cpp`, `_LDW.cpp`, `_GSB.cpp`, `_DBS.cpp`, and their regions of the shared ICC
files. `ICCShared.{h,cpp}` for scope item 1. Readers `bosses/lord_marrowgar.py`,
`lady_deathwhisper.py`, `rotting_frost_giant.py`, `icecrown_gunship_battle.py`,
`deathbringer_saurfang.py` with their tests. Docs `docs/raids/icecrown-citadel/lord-marrowgar.md`,
`lady-deathwhisper.md`, `rotting-frost-giant.md`, `icecrown-gunship-battle.md`,
`deathbringer-saurfang.md`. This brief. For item 1b only: the giant row and comment in
`raidobs/encounter.py`, the tests that pin it, and the giant lines of the ICC README and
`observability.md`.

## Task list

Sized as one lane: each encounter's change is small, and a split would make every implementer re-read
the same shared files. No ICC trace exists on disk (`631_*`), so nothing was read as evidence. All done:

- [x] `ICCShared`: taunt helper without fixate or reset, no ranged pull while the taunt cools down;
  `IccStripExperienced` takes Experienced and 1% Threat off the bot and every human in the instance;
  `IccSameInstance` guard on the heroic buff and on Deathwhisper's member loop.
- [x] Giant prefix `rfg.`: `rfg.deathplague`, `ICC_KEY_PREFIXES`, its comment, a pinning test, README.
- [x] Phases: `lm.phase` (Bone Storm aura), `ldw.phase` (Mana Barrier aura, replacing seven
  `HealthAbovePct(95)` tests), `gsb.phase` (Below Zero on the raid's cannons).
- [x] Probes: `lm.stormanchor`, `lm.mark`, `lm.spike`, `ldw.skull`, `gsb.deck`, `gsb.skull`, `gsb.star`,
  `dbs.skull` (`dbs.slot` was traced in wave 1).
- [x] Gunship unstuck on `NearTeleportTo`; shared `IccGunshipEnemyCaptain`, `IccGunshipCannonsFrozen`.
- [x] Multipliers: per-ms cache in Deathwhisper's and the gunship's, Saurfang's tests the action first.
- [x] Readers `lord_marrowgar.py`, `lady_deathwhisper.py`, `icecrown_gunship_battle.py`,
  `deathbringer_saurfang.py`, `rotting_frost_giant.py` with tests; five boss docs; README rows.
- [x] Checks: syntax (lane `.cpp` alone, then the four headers' 20 TUs), pblint 23/25 warnings (wave 1's
  baseline), botobs suite, codestyle.

## Decisions for review

- **Taunt on cooldown**: returns false without the `shoot`/`throw` fallback. Source: the fallback faces
  its target (`PlayerbotAI::CastSpell`), and with the reset gone every 500 ms retry inside the cooldown
  would turn a tank holding the boss toward the add. Alternative: keep the fallback on every failure.
- **Strip scope**: humans and the bot itself, never other bots (each strips itself); Aegis and Pain
  Suppression are left, both expire (10 s, 8 s). Alternative: strip every member from every bot.
- **Deathwhisper phase**: Mana Barrier aura, the core's own phase-2 edge (`DamageTaken` removes it).
  Changes play: phase-2 positioning, the end of Viper Sting and the add consolidation now start at the
  break, not at 95% health. Alternative: keep 95%.
- **Gunship radii**: trigger 100 yd, multiplier 200 yd, unchanged. Source: no core value decides, and
  200 for the trigger would keep bots steering at world-coordinate deck points longer while a ship
  leaves. Alternative: unify on either.
- **Gunship unstuck**: `NearTeleportTo(..., removeTransport = true)` onto the same friendly deck
  point. Source: kept on the transport, the bot keeps the offset of where it fell; PlayerbotAI's 1 s
  check re-boards it. The core's Check for Players already rescues deeper falls, so the snap is
  documented as borderline, not wrapped. Alternative: the bot's own x, y at deck height, which is
  mid-air for a bot off the edge.
- **Phase keys**: Saurfang and the giant publish none (no phase in the script); `gsb.phase` is the
  freeze, since "fight active" is the trigger gate itself and writes nothing a trace lacks.
- **Probe values**: rule names, one guid only in `gsb.star` (the unit the star went on); per-bot
  latches throughout, merged by the readers from the first bot to note each step.
- **Gap rechecks**: all four kept. Cyclone needs a CC election, Insignificance a tank-swap duty, Death
  Plague a spread duty, Battle Fury a stack threshold. Death Plague's core-bug claim half holds (issue
  text in `rotting-frost-giant.md`), Battle Fury's does not.

## Carried over

- **i2-plagueworks**: Festergut's bloat swap, Stinky and Precious's swap and add taunts, Putricide's
  plague swap, Rotface's Big Ooze loop now get one taunt per 8 s cooldown and no fixate; note each in
  its boss doc (README "The class taunt").
- **i3-crimson-hall**: the council assist tank's taunts on Keleseth, Valanar, Taldaram and stray Dark
  Nuclei, one per cooldown.
- **i4-frostwing**: Sindragosa's main-tank taunt-back and Valithria's stray-add taunt.
- **i5-lich-king**: the add taunt passes (`ICCActions_LK.cpp` ~1439, ~1674) and the Raging Spirit loop
  (~2792) reach one unit per cooldown.
- **Merge stage**: observability.md needs no giant edit (it names no giant prefix). Generic lessons are
  in the lane report.

## Known gaps

- Raid-icon targeting on every encounter here (boss docs).
- Deathwhisper's skull rule never moves the icon off a live non-add left marked earlier (`other`).
- The four kept gap cheats (boss docs).
- `HandleAddTargeting` in `ICCActions_LDW.cpp` has no caller; left for the upstream merge's sake.

## Blocked
