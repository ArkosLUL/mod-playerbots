# g4-gunship — close the Gunship Battle's gaps

Gap wave `3ab-gaps`, after `3b`. Rules: [icc-rebuild.PLAN.md](icc-rebuild.PLAN.md), its "Gap lanes"
section included. Builds on the merged `r4-gunship` lane and its review: read
[r4-gunship.PLAN.md](r4-gunship.PLAN.md) first. Its decisions stand unless closing a gap needs a
change, which then goes under Decisions for review here.

## Scope

Every bullet of "Known gaps" in `docs/raids/icecrown-citadel/icecrown-gunship-battle.md` and in r4's
brief, plus the two every gap lane does (tank defensives, line of sight).

- Tank defensives: the board tank against the captain's Battle Fury stacks, and the home tank
  against boarders; the doc also names a missing burst on the captain.
- Line of sight: both decks are transport floors, which navprobe can't see (convention 28). Say so in
  the doc and spend no probes there.
- The edge slots assume the cannon side faces the enemy ship. Settle it from the core's transport and
  ship data offline where it can be settled.

## Owns

Stem `Gunship` and its definition, the doc, the reader and its test, this brief.

## Task list

One lead, no split: the defensive node reads the stem's roster, deck test and unit cache.

- [x] Tank defensives (convention 43): `GsbTankDefensive` / `GsbSpentTankDefensive` / `GsbTankDefensiveHeld`,
  trigger and action `icc gunship tank defensive` at `ACTION_RAID + 8`, a `Block` row holding the class
  nodes (`HeldTankDefensiveNames()`), per-tank latch, `gsb.defensive` and `gsb.furywindow`, a
  `CallHumanTankDefensive` for a human on the captain (`gsb.humancall`).
- [x] Line of sight and coordinates: both decks are gameobject floors, so no probe was spent; the doc
  says sight and floor are unchecked.
- [x] Cannon side: settled offline from the `TaxiPathNode` DBC (stop-frame headings) and the cannon
  spawns in the world DB; the edge slots face the enemy ship on all four ships. Re-derived launch,
  landing, mage area, healer reach and mortar distance too (doc, Coordinates).
- [x] Burst: nothing releasable from this stem; recorded (doc, Defensives).
- [x] Mortars out of reach: by design, the cannons are the guide's answer (84 to 93 yd, 72 to 81 yd).
- [x] Reader `--defensives`, tests, boss doc.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Board tank: one physical-only button at +50% and one at +100% of the captain's hit (5 and 10 stacks on normal, 5 and 9 on heroic), the retreat staying at +200% | IC ("use defensive abilities to buy time"), WT/Wowhead (stacks kill the tank); core: 1.5 s swing, +10%/+12% per stack (row 2155) | only the retreat; HP-only |
| Home tank: window 1 when 3 boarders sit on it with a party member across, or one is a Veteran; window 2 at Elite | WT/Wowhead (the home tank and healers contain the adds while the party is away); core `ExperiencedTimes` 100/70/60 s | HP-only; a button per wave |
| Under 40% health a tank also spends the next ready button outside any window, one at a time (no latch); class defensive nodes held above 25% | ToC Anub'arak side tank (stacks or low health), convention 43 | one button per low-health stretch, latched; leave the class nodes on |
| A human on the captain gets `CallHumanTankDefensive` at the same windows and under 40%; boarder waves on a human get none | convention 43, `RaidHumanTankCall` | no call |
| No burst pacing: lust is left to the base gate (any tank holds the captain 3 s, which is the boarding); personal cooldowns stay held because the freeze mage is not boss-flagged | `BurstWindowStrategy.cpp`, `creature_template` type_flags 72 (mage) vs 76 (captain) | an entry-keyed exemption for the mage in the shared gate, outside this lane |
| The edge slots stay as they are: the cannon side faces the enemy ship | `TaxiPathNode` DBC, `MotionTransport::Update`, cannon spawns | slots read from the enemy ship's bearing at runtime |
| Home tank latch ends with its window (fewer than 3 held, no Veteran), so a fresh wave gets a button | `UpdateDefensive`; guide: the home tank contains each wave | latch until no boarder is on the tank |
| Mortars stay with the cannons | WT/Wowhead ("they stand out of range of your ranged DPS") | the boarding party kills them after the mage (IC); they respawn 30 s after dying |

## Carried over

- r14: `IccInstanceState::gsbLastStarPos` and `gsbLastRocketPackUse` (`ICCScripts.h`) are unused now.
- r14: `SPELL_FROZEN_CANNON` 69704 (`ICCData.h`) is in neither the DBC nor `spell_dbc`.
- r14: `RaidRestoreLegacyRti` doesn't cover `star`; the gunship tick resets it for its own window.
- r14: the stem's walk base copies Marrowgar's `IccLmWalkAction`; hoist one.
- merge stage: README "Before live" row for the Gunship also lists `icc gunship tank defensive` (live-only
  in effect); trigger names list.
- engine: a kill-race add that isn't boss-flagged never gets personal burst from the base gate, and a
  per-boss `RaidBurstWindow` can only hold; releasing needs an entry in `BurstCooldowns.cpp`.

## Known gaps

- No 631 trace on disk; nothing here was replayed. Never built, CMake needs no re-run (no new `.cpp`).
- The decks are gameobject floors: no coordinate and no line of sight is checked offline.
- The defensive thresholds are guesses until a pull shows the stacks against the tank's health
  (`--defensives`).
- Personal burst cooldowns stay held on the freeze mage.
- A home tank's window latch ends once fewer than 3 boarders sit on it (and none is a Veteran), so a
  wave that flaps around 3 can cost a second button every 12 s or so. Kept so each fresh wave gets
  one; `--defensives` shows a repeat as casts on the friendly deck.
- A human holds no duty.
- No `-Werror` pass; the syntax check hides warnings.

## Blocked
