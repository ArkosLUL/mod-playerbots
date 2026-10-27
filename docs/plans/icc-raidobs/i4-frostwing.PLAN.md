# i4-frostwing — Sister Svalna, Valithria Dreamwalker, Sindragosa gauntlet, Sindragosa

Wave 2b, parallel with `i3-crimson-hall`. Rules, contract, classification and the boss lane steps:
[icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). Waves 1 and 2a are on `Custom`. Read
`docs/raids/icecrown-citadel/README.md` for the cheat keys and for the shared taunt-helper change 2a
made.

## Scope

Run the boss lane steps for `sister-svalna`, `valithria-dreamwalker`, `sindragosa-gauntlet` and
`sindragosa`. The gauntlet trace needs only a reader and a doc section.

1. **Phases.**
   - Svalna: none (Aether Shield state only if a reader needs it).
   - Valithria: no boss phase. Publish the portal waves and each bot's Dream State window.
   - Sindragosa: air phase (`IsBossFlying` within 30 yd of `ICC_SINDRAGOSA_FLYING_POSITION`; the
     triggers use 50 yd, often with health bands) and phase 3 under 35% (latched in `sgLastPhase3`).
     One helper, on the core's thresholds.
2. **Decisions to probe.**
   - Valithria: the raid-heal choice (`IccValithriaShouldHealRaid`, `ICCShared.cpp`; you may edit
     that function), the portal claim and the cloud target (traced by wave 1), and her icon targeting
     (`ICCActions_VT.cpp`).
   - Sindragosa: the tomb groups and tomb choice (traced by wave 1), the LOS move (`sgLastLosMove`,
     left bare: `NoteDerived` in its helper), the flare latches, and the tomb icon buckets.
3. **Cheats.**
   - Remove:
     - Valithria's Dream State speed ×2 (`ICCActions_VT.cpp` ~928; ~865 resets it).
     - Nitro, `AddAura 54861`, at VT ~1089, the Sindragosa trigger (`ICCTriggers.cpp` ~799),
       `ICCActions_SG.cpp` ~614 and ~796. Each becomes `UseNitroBoosts(bot)`.
   - Keep, with each boss doc's gap entry:
     - Valithria's teleports to the cloud and onto a Dream State player (~962, ~1028). These are
       same-map `TeleportTo`, which also drop combat, worth stating.
     - Sindragosa's Frost Beacon strip (trig ~809), Mystic Buffet strip (trig ~815), Magic Barrier
       full immunity on tanks (spell 38112 is immune to all schools, trig ~821), 1% Threat on the
       assist tank (trig ~845), and the tomb aura strips (`ICCActions_SG.cpp` ~981, ~1064).
   - Document only:
     - the smoke flares from `StoreNewItemInBestSlots` (~492);
     - the borderline floor snaps after Dream State that skip fall damage (VT ~716, ~856).
   - Record-only: Sindragosa's `EnableICCBuffs` sites (trig ~775-792).
4. **Bugs.**
   - Hard-coded heal casts may cast a spell the bot never learned, because
     `PlayerbotAI::CastSpell(uint32)` doesn't check (VT ~885-915: Wild Growth, Beacon, Riptide and
     others; SG ~562). Add a known-spell check at those sites only.
   - Wave 1 moved the instance-state reset onto boss state. Check that Valithria's portal claims and
     cloud target now clear on a wipe.

## Owns

`Action/ICCActions_SS.cpp`, `_VT.cpp`, `_SG.cpp`, and their regions of the shared ICC files.
`IccValithriaShouldHealRaid` in `ICCShared.cpp`. Readers `bosses/sister_svalna.py`,
`valithria_dreamwalker.py`, `sindragosa_gauntlet.py`, `sindragosa.py` with their tests. Docs
`docs/raids/icecrown-citadel/sister-svalna.md`, `valithria-dreamwalker.md`, `sindragosa.md` (gauntlet
as a section). This brief.

## Task list

Sized as one lane: most of the work sits in the shared ICC files, and the readers hang on the probe
design. No `631_*` trace exists, so nothing was read as evidence. All done:

- [x] Phases: `IccSindragosaPhase` (`sg.phase`) and `IccSindragosaOverAirStation` in `ICCTriggers`,
  every Sindragosa phase and air test routed through them; `IccValithriaPortals` (`vd.wave`);
  `IccSvalnaShielded` (`ss.shield`).
- [x] Probes: `vd.raidheal`, `vd.cloud`, `vd.group`, `vd.skull`, `vd.cross`, `sg.losmove`,
  `sg.flaredred`, `sg.skull`, `sg.cross`, `sg.star`.
- [x] Cheats: `vd.nitro` (1) and `sg.nitro` (3 sites) to `UseNitroBoosts`; `vd.speed` and its speed
  reset removed. Gap and buff sites unchanged.
- [x] Bugs: known-spell checks on Valithria's heals and Hand of Freedom; Valithria floor snaps on
  `NearTeleportTo`; Sindragosa's 25%/99% bands and 35% tests on her script's phase.
- [x] Multipliers: per-ms cache in Valithria's and Sindragosa's.
- [x] Wipe check: portal claims and the cloud target clear 5 s after Valithria's `NOT_STARTED`.
- [x] Readers `sister_svalna.py`, `valithria_dreamwalker.py`, `sindragosa.py`,
  `sindragosa_gauntlet.py` with tests; docs `sister-svalna.md`, `valithria-dreamwalker.md`,
  `sindragosa.md`.
- [x] Checks: syntax (lane `.cpp`, then the 20 header includers and the `BuildShared*` contexts),
  pblint 22/23 warnings (unchanged), botobs suite, codestyle.

## Decisions for review

- **Sindragosa's phase from her react state**: `air` while in combat and passive, takeoff to
  touchdown; `last` at 35% and under on the ground. Source: `EVENT_AIR_PHASE`, `POINT_LAND_GROUND`,
  `DamageTaken` and `EVENT_THIRD_PHASE_CHECK`. Changes play only for a last phase reached mid-air,
  whose behaviour now waits for touchdown as the script's does. Alternative: the coordinate tests.
- **Station tests kept** (30 yd far station, 50 yd both) inside the air phase. Source: the script
  casts beacons and drops bombs only there; the whole air phase would also run the Frost Bomb action's
  no-marker branch, which stops attacks, through takeoff and the flight back. The script-free 25%/99%
  bands went. Alternative: every air test on the whole air phase.
- **`vd.wave` reads the pre-effect's visual**. Source: addon auras 71304/71986, the 6 s linger
  (`DelayedCastEvent`). Alternative: any live pre-effect, which reads `pre` again after the portals.
  Healers in Dream State write none: they are phased away from the portals.
- **No Dream State probe**: the `aura` stream carries 70766 (`LogAuras` defaults to 1). Alternative: a
  per-bot `NoteDerived`.
- **Floor snaps on `NearTeleportTo`** as a small-bug fix (same-map `TeleportTo` drops combat). The two
  kept teleports stay `TeleportTo` so the raid cheat behaves identically; their combat drop is in the
  boss doc. Alternative: `NearTeleportTo` there too.
- **Speed reset removed with `vd.speed`**: `SetSpeed(..., 1.0)` each tick set the rate outright and
  cancelled Sprint and other speed auras. Alternative: keep it as cleanup.
- **Nitro after `CastStop`** on the zombie kite: `UseNitroBoosts` refuses a casting bot.
- **Unknown heals are skipped** to the next spell in the chain. Alternative: return false.
- **Svalna's trigger gates on the shield** so `ss.shield` is written each tick a bot is in range;
  neutral, the action returned false without it.
- **Icon and cloud probes noted only by the bot that moves them**, as `pp.skull`.
- **No `sg.beaconspot`**: beacon spots follow from the count and guid order, and the moves show them.
- **No Blistering Cold hazard circle**: her 5 s `cast` row already times it, and pacing a circle per
  instance needs `IccInstanceState` (`ICCScripts.h`, outside the lane); unpaced, every bot writes one
  per cast. Alternative: a `sgBlisteringColdNotedMs` field there.
- **Healer ranking unchanged**: humans counted, a selfbot asking ranks nobody (upstream's
  `IsRealPlayer` port). Both are tactic choices, listed as gaps.
- **Gap rechecks: all kept.** The teleports need a flight mover; the beacon and buffet strips a tank
  swap; Magic Barrier a Blistering Cold run plus cooldowns; 1% Threat a threat brake; tomb strips a
  DoT-aware stop; flares bots carrying them.

## Carried over

- **Merge stage, ICC README**: delete the `vd.nitro`, `vd.speed` and `sg.nitro` rows; link
  `sister-svalna.md`, `valithria-dreamwalker.md`, `sindragosa.md`; the floor snaps now use
  `NearTeleportTo`.
- **i5-lich-king**: `SPELL_NITRO_BOOSTS` stays for the Lich King's sites. Humans keep Sindragosa's
  heroic buffs, since no later node strips them off heroic.
- **i6-closeout**: the four readers repeat the refusal and cheat code too.
- **Engine lessons** (lane report): a probe that reads the world must skip a bot phased away from
  it; `SetSpeed` sets the rate outright, so a reset to 1.0 cancels speed auras.

## Known gaps

In the boss docs: icon targeting; the kept gap cheats; Dream State bots hover with the raid cheat
off; humans hold healer ranks; a selfbot gets no rank; every bot rushes a Svalna spear; red flares
burn out after 5 min; humans keep Sindragosa's heroic buffs; no ICC trace carries these probes yet.

## Blocked
