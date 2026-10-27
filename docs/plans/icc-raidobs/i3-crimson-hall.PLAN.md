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

Sized as one lane: two encounters, most of the work in the shared ICC files a split leaves to the
finisher. No `631_*` trace exists, so nothing was read as evidence. All done:

- [x] Phases: `IccBpcEmpoweredPrince` (`bpc.phase`, the three Invocation ids) behind the vortex and
  ball triggers and the skull rule; `IccBqlPhaseOf` (`bql.phase`) behind the group action and the
  multiplier, replacing the height test and the three latches.
- [x] Probes: `bpc.bomb` in `IccBpcAssignedKineticBomb`, now the one assignment the action and the
  multiplier read; `bpc.spread` in `BpcSpreadAngle`, shared by both spread rings; `bpc.skull`;
  `bql.bite`, `bql.pact`; `none` at each window's end.
- [x] Cheats: `bql.swarmingshadows` removed.
- [x] Bugs: council unstucks on `NearTeleportTo`; ball victims no longer freeze; `bql.curve` reset per
  trail; dead BQL latches out of `ICCScripts.h`; stale Nitro comment.
- [x] Multipliers: the council's bomb block tests the action first and its flame searches are cached
  per ms; the Blood-Queen's air block tests the action first.
- [x] Readers `blood_prince_council.py`, `blood_prince_trash.py`, `blood_queen_lana_thel.py` with
  tests; docs `blood-prince-council.md` (trash section), `blood-queen-lana-thel.md`.
- [x] Checks: syntax (lane `.cpp`, then the four headers' 20 TUs and both `BuildShared*` contexts),
  pblint 22/23 warnings (baseline), botobs suite (1066), codestyle.

## Decisions for review

- **Blood-Queen phase from her state**: `center` while `REACT_PASSIVE` with gravity on, `air` while
  passive and levitating, `ground` otherwise. Source: `EVENT_AIR_PHASE` sets passive,
  `EVENT_AIR_START_FLYING` and `POINT_GROUND` set and clear gravity, and `NO_MOVE_FLAGS_UPDATE` (0x200
  on every difficulty entry) keeps the core from touching the flag. Changes play: the centre stack starts when she turns
  to walk, not 20 yd short of the centre, and never fires on the ground. Alternative: keep the height
  test and latches behind the helper.
- **Dead latches deleted from `ICCScripts.h`** (outside the shared-file list): `bql.groundphase`,
  `bql.postairlanding` and `bqlBossWasAirborne` have no reader left, and a declared key that never
  emits breaks the probe rules. Alternative: leave them declared.
- **`bql.swarmingshadows` moved from gap to removed.** Source: the core never targets her victim or
  the Blood Mirror player, and with the strip gone a tank's tick falls through to the same lane walk
  every bot runs, which the multiplier already isolates. No new duty. Changes play on the default
  config: a shadowed assist tank walks the trail. Alternative: keep as the brief said.
- **One bomb assignment**: the multiplier now screens bombs by height above the floor, as the action
  did, instead of above the bot. Source: one helper, two readers. Alternative: the bot-relative
  height, which only differs for a bot off the floor.
- **Ball triggers fire for any ball victim.** Source: the multiplier leaves a chased bot only the ball
  action, and the trigger required Taldaram empowered, so a plain ball's victim stood still. Changes
  play for that victim only: it now keeps 15 yd from everyone.
- **`bpc.skull` names the unit it came off**, written by the mover only, so a tank re-marking the same
  prince a rotation later writes again unless the same move repeats exactly. Alternative: every tank
  notes the skull after the rule, which only restates `bpc.phase`.
- **"Vortex spread lock" probed as the slot it protects** (`bpc.spread`): the 250 ms lock is a
  throttle and stays bare. Both rings go through one angle helper, the old two copies being identical.
- **Bites and Pact**: the strategy derives both (claim order by guid, nearest first; tank, centroid
  or partner), so both are probed; their window end is written from the trigger.
- **Blood Prince trash**: reader and doc section only. No trace can open under its slug (state 14
  never goes `IN_PROGRESS`, the trash is not boss-flagged), and adding a `MarkPull` means trash code
  the scope excludes.
- **Prince bail in the Blood-Queen triggers** (brief item): it never fires in her fight,
  since the princes die together and stay dead once the council is `DONE`. Left as is.
- **Unstuck returns** kept `false` after `NearTeleportTo`, as the gunship's.

## Carried over

- **Merge stage, ICC README**: delete the `bql.swarmingshadows` cheat row; drop `bql.groundphase`,
  `bql.postairlanding` and `bqlBossWasAirborne` from "Per-instance state"; link
  `blood-prince-council.md` and `blood-queen-lana-thel.md` from the boss table.
- **i6-closeout**: the three readers repeat the banner, refusal and cheat code the earlier readers do.

## Known gaps

In the boss docs: skull by raid icon (council); the Keleseth tank action's non-assist-tank branch
never runs; `bql.curve`/`bql.slot`/`bql.airslot` write `0` on an erase; the trash slug names no trace;
nothing measured yet.

## Blocked
