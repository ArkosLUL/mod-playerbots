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

## Decisions for review

## Carried over

## Known gaps

## Blocked
