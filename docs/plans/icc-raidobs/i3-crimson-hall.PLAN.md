# i3-crimson-hall — Blood Prince Council, Blood Prince trash, Blood-Queen Lana'thel

Wave 2b, parallel with `i4-frostwing`. Rules, contract, classification and the boss lane steps:
[icc-raidobs.PLAN.md](icc-raidobs.PLAN.md). Waves 1 and 2a are on `Custom`. Read
`docs/raids/icecrown-citadel/README.md` for the cheat keys and for the shared taunt-helper change 2a
made.

## Scope

Run the boss lane steps for `blood-prince-council`, `blood-prince-trash` and
`blood-queen-lana-thel`. The trash trace needs only a reader and a doc section; the strategy has no
trash code.

1. **Phases.**
   - Council: the empowered prince (Invocation of Blood on Valanar or Taldaram today; check Keleseth
     in the core).
   - Blood-Queen: ground and air. Today this is boss Z more than 5 above `ICC_BQL_CENTER_POSITION`,
     plus the latches `bqlGroundPhaseEstablished`, `bqlBossWasAirborne` and `bqlPostAirLanding`
     (traced by wave 1). Put them behind one helper.
2. **Decisions to probe.**
   - Council: the kinetic bomb assignment derived inside `IccBpcAssistMultiplier`
     (`ICCMultipliers.cpp`). Move it into a helper and probe it there, not in the multiplier's call
     path. Also the empowered-prince icon (`MarkEmpoweredPrince`) and the vortex spread lock.
   - Blood-Queen: the shadow-kite lane (`bqlBotCurrentCurve`) and the air and ground slots (traced by
     wave 1). Bite order and Pact pairs if the strategy derives them.
3. **Cheats.** Keep the Swarming Shadows strip on the tank (`ICCActions_BQL.cpp` ~26) as a gap. The
   legit answer is the trail walked away from the raid; say what that needs. The own-shield cancels
   (~1144) are not cheats.
4. **Bugs.**
   - The Council unstucks (`ICCActions_BPC.cpp` ~29, ~559) use a same-map `TeleportTo`, which drops
     combat. Make them `NearTeleportTo`.
   - The Blood-Queen triggers bail while any prince is present (`ICCTriggers.cpp`). Check that this
     never shuts them during her own fight.

## Owns

`Action/ICCActions_BPC.cpp`, `_BQL.cpp`, and their regions of the shared ICC files, including
`IccBpcAssistMultiplier`. Readers `bosses/blood_prince_council.py`, `blood_prince_trash.py`,
`blood_queen_lana_thel.py` with their tests. Docs `docs/raids/icecrown-citadel/blood-prince-council.md`
(trash as a section) and `blood-queen-lana-thel.md`. This brief.

## Task list

## Decisions for review

## Carried over

## Known gaps

## Blocked
