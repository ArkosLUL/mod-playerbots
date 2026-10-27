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

Sized as one lane: nearly every change sits in `ICCActions_LK.cpp` and the LK regions of the shared
files, and the reader hangs on the probe design. No `631_*` trace exists, so nothing was read as
evidence. All done:

- [x] Phase: `IccLichKingPhase` (`lk.phase`) and `IccLichKingInFrostmourne` in `ICCTriggers`, every
  LK phase test on them; `IccRemorselessWinterOf` (row 1 or 2) in `ICCShared.cpp`.
- [x] Probes: `lk.skull`, `lk.cross`, `lk.star`, `lk.circle` (`LkSetIcon`), `lk.winterstage`,
  `lk.stackslot`, `lk.valkyr`, `lk.bombdodge`.
- [x] Cheats: `lk.nitro` (3 sites) to `UseNitroBoosts`; `lk.painsuppression`, `lk.aegis` removed;
  `lk.frostmourneescape` wrapped.
- [x] Bugs: Harvest Soul ids and guard; six `TeleportTo` to `NearTeleportTo`; no platform logic or
  platform snap inside Frostmourne; multiplier per-ms cache, defile scan behind its action test;
  `SPELL_NITRO_BOOSTS` deleted (last user). Platform Z tolerance deferred (Carried over).
- [x] Reader `the_lich_king.py` with tests; doc `the-lich-king.md`.
- [x] Checks: syntax (lane `.cpp`, then the 20 header includers and the `BuildShared*` contexts),
  pblint 22/22 warnings (the Harvest Soul one fixed), botobs suite, codestyle.

## Decisions for review

- **Phase from his react state and the winter's row**, stateless: `one` above 70%; `winter1` and
  `winter2` from the aura or cast of rows 2293/2294, else passive above or at 40% (the walk and the
  Quake's 5 s wait); `two`/`three` aggressive; `frostmourne` heroic, passive under 40%, for a bot
  inside the room or carrying Harvest Souls. Source: `DamageTaken` thresholds, `MovementInform`,
  `EVENT_QUAKE(_2)` + `ACTION_START_ATTACK`, `EVENT_HARVEST_SOULS`. Alternative: a per-instance latch
  in `IccInstanceState` (`ICCScripts.h`, outside the lane).
- **Sites test the phase order** (`>= winter1` past 70%, `>= winter2` past 40%), so the walk counts
  with the following phase as the health tests did. Thresholds moved: eight 71% tests (necrotic
  plague approach, P1 skull, P1 melee, ranged and heroic stacks, main tank boss hold, the multiplier's
  two P1 rules) to 70%; centre stack from 67% to `two`; Vile Spirit slots from 43%/37% to `winter2`
  (P2's last 3% now stacks at the centre; exactly 37% no longer fell to it). Neutral: 70% sites, the 72-70%
  stun band, the 40% Val'kyr gates, the edge dive band (`two`; no Val'kyr exists in the windows that
  left it), the Shadow Trap trigger's 65% (`<= winter1`: traps despawn at the winter's first hit).
  Kept: the Empowered Blood 60-40% band (operator buff); winter-up tests stay on
  `IccBossHasRemorselessWinter`, the hazard rather than the phase.
- **`lk.frostmourneescape`, a new gap cheat**: `HandleTeleportationFixes`' normal snap-back (Y 200 yd
  off) fires on a Harvest Soul victim's first tick in Frostmourne, skipping the Spirit Warden.
  Wrapped, with `NearTeleportTo` per the brief (keeps combat; otherwise identical). With the cheat
  off, a new guard keeps platform logic away from a bot inside Frostmourne, whose `exact_waypoint`
  walks would be straight lines out. Alternative: leave it unwrapped as an unstuck.
- **Harvest Soul guard made live (changes play)**: both Harvest spells are instant (CastingTimeIndex
  1), so the cast test never fired. It now reads normal's 6 s victim aura (row 2298, all four ids) on
  any group member, so non-tanks hold off the Vile Spirit flee while a raider is harvested. Heroic's
  Harvest Souls stays out: its aura has no timed end (DurationIndex 21), so it could hold the guard for
  the rest of the pull. Alternative: fix the ids only and leave the guard dead.
- **Platform snap tolerance kept at 1 yd**: the brief wants a navprobe reading for a new value and the
  lane rules allow no container beyond the prescribed checks. The band also cancels knockbacks
  mid-flight (heroic Shadow Traps on non-tanks, Ice Burst), so the script's own floor (Z 840,
  `IsValidPlatformPosition`) would let them land: a play change to take with the reading. Both snaps
  now skip Frostmourne, where an expired Harvest Soul aura would send a normal victim to Z 840 over
  the void. Alternative: the script's 840 now.
- **Nitro on real boots**: the winter run within 35 yd, the Defile target's own run after `CastStop`
  (humans run on their own), the assist tank's spirit chase only when it walks.
- **Probes**: icons by the mover with rule and unit (`LkSetIcon`); `lk.winterstage` carries its winter
  so a second winter's same anchor still writes; `lk.stackslot` by the bot that refreshed the 2 s
  choice; `lk.valkyr` per bot with `none` once no Val'kyr grabs. The brief's "Defile unsafe spot"
  (`defileLastUnsafe*`) is the Spirit Bomb dodge's memory: probed by rule as `lk.bombdodge`, the
  coordinates left bare. No hazard notes: every LK hazard is a sampled creature or a cast row.
- **Gap rechecks: all kept.** `lk.hoj`: Shamblings are not stun-immune, but every Shambling at once
  needs a stun rotation; the spirit kills need the kill.

## Carried over

- **Merge stage, ICC README**: delete the `lk.nitro`, `lk.painsuppression`/`lk.aegis` rows; add
  `lk.frostmourneescape` (gap, raid, `HandleTeleportationFixes`, normal); link `the-lich-king.md`;
  `SPELL_NITRO_BOOSTS` is gone.
- **i6-closeout**: the reader repeats the refusal and cheat code too; humans keep Experienced and 1%
  Threat after heroic Sindragosa and Lich King pulls; the platform snap's tolerance needs a navprobe
  reading of the Frozen Throne (boss doc, Unstucks) and a call on letting knockbacks land.
- **Engine lessons** (lane report): an instant boss cast is never seen casting, so a guard on it reads
  the victim's aura; a same-map unstuck that also runs where the bot is meant to be is a hidden
  escape.

## Known gaps

In the boss doc: icon targeting; the kept gap cheats; a cheat-off Frostmourne victim has no duty; the
1 yd platform snap and its edge loop; `lk.phase` at a threshold's cast and outside heroic
Frostmourne; heroic Harvest Souls outside the hold; the Val'kyr skull bucket's `rti`; humans keep
heroic buffs; nothing measured yet.

## Blocked
