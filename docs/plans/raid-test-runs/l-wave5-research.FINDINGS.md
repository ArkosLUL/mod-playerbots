# Lane L findings: Wave 5 research (Flame Leviathan, Thorim, Algalon, Yogg-Saron)

Read-only research for [raid-test-runs.PLAN.md](raid-test-runs.PLAN.md), brief
[l-wave5-research.PLAN.md](l-wave5-research.PLAN.md). Read at core `e98ba65bc`, fork `1a95e2554`, DC
`9d5e1b2` (`local/wrath-raid-tests`, lane J merged). Path prefixes: core `src/server/...` and `data/sql/...`;
Ulduar scripts shortened to `ulduar/<file>` (`src/server/scripts/Northrend/Ulduar/Ulduar/`); fork
`src/Ai/...`, `docs/...`; DC `DC:src/...`. Nothing here was run live: every coordinate needs navprobe, and
every "live only" item needs the first run.

## Summary

| | Flame Leviathan | Thorim | Algalon | Yogg-Saron |
|---|---|---|---|---|
| Start | First pull: base-camp vehicles unlocked by an intro gossip, a ~830 yd gauntlet, both Colossi dead, a 63.5 s RP. Every later pull: fresh vehicles at the arena edge, he is attackable, no RP | Damage to the arena start pack zone-combats him; 6 pack deaths unlock the lever into the gauntlet and start phase 1 | Console + key item (once per instance) summons Brann, then Algalon after a ~36 s intro; each pull is an attack on him | Sara starts by herself 5 s after a living player comes within 90 yd (3D) |
| DC needs | preState SPECIAL + one focus evade, preData (towers), preDespawn, "seated and inside the arena" readiness | Roster anchor, aggro start on a separate engage entry | Roster anchor, once-per-run GameObject use with lock items, "focus attackable" readiness, preDespawn | Roster anchor, preData (keeper mask), teleport start |
| Strategy blockers | Boarding needs `master->GetVehicle()`; nothing drives the fleet into the arena before the pull | Gauntlet squad follows the raid leader, who is the arena main tank under DC; no bot pulls the lever | None | None |
| Size | Large | Large | Medium | Medium |
| Top risk | Fleet spawns west of the Shield Wall that closes on engage | A bot-led gauntlet must still beat the 150 s hard-mode clock | Server-side console use with a granted key | Start point z must keep 3D range under 90 yd |

## Facts that apply to all four

- **Every static creature has already reset before DC's pre-steps run.** Instances load all grids at creation
  (`src/server/game/Maps/Map.cpp:88-93`), and DC's preDone / preDespawn run later in `TickStarting`
  (`DC:src/TestRun/DcTestRunJob.cpp:1573`, `:1618`). A pre-step that changes state a script read in its
  `Reset()` needs that creature reset afterwards. `CreatureAI::EnterEvadeMode` calls `Reset()` even out of
  combat (`src/server/game/AI/CreatureAI.cpp:235-263`).
- **DC's Ulduar roster holds only kill-credit bosses with a static spawn** (`DC:src/Ai/Dungeon/DungeonClear/Data/BossSpawnIndex.cpp:66-80`).
  From `data/sql/base/db_world/instance_encounters.sql:551-578`: Flame Leviathan (kill 33113, spawned) is on
  it; Thorim (spell 64985), Yogg-Saron (kill 33288, summoned) and Algalon (spell 65184, summoned) are not, so
  `ValidateScenario` refuses rows focused on them (`DC:src/TestRun/DcTestDungeonRegistry.cpp:497`, `:502`).
  Fix: an Ulduar roster patch like the ToC one (`DC:src/Ai/Dungeon/DungeonClear/Data/Events/TrialOfTheCrusaderEvents.cpp:16-34`)
  anchoring Thorim 32865 on boss 8, Sara 33134 on boss 12 (she is always present, Yogg-Saron is not) and
  Algalon 32871 on boss 13.
- **Who "leads" under the DC driver.** `botAI->GetMaster()` is the driver: non-null, off the map, never on a
  vehicle, not in the group. `GetRaidLeaderOnInstance` (`src/Ai/Raid/RaidCombatHelpers.cpp:38-51`) skips it
  and returns the group leader, slot 0, which DC fills with the first tank, which is also
  `GetMainTankGuid`'s fallback main tank.
- **Four hard-mode env lines do nothing.** `Playerbot.env` sets `AC_PLAYERBOTS_ULDUAR_{THORIM,FREYA,FLAME_LEVIATHAN,YOGG_SARON}_HARD_MODE`,
  but `src/PlayerbotAIConfig.cpp:782-785` reads only Vezax, Iron Assembly, Mimiron and XT-002. For these four
  bosses hard mode is instance state, set as described per boss.
- **Two corrections to the plan's Facts.** Thorim's lever unlocks after the 6 deaths of the arena start pack,
  not gauntlet deaths (`ulduar/boss_thorim.cpp:454-491`). Algalon's despawn countdown is **300 minutes** in
  this fork (`ulduar/ulduar.h:422`), not one hour, started by his first engage.

## Flame Leviathan (boss 0, entry 33113)

### Start and reset in the core

- **First pull.** `Reset()` on `NOT_STARTED` makes him `NON_ATTACKABLE` and arms `_startTimer`
  (`ulduar/boss_flame_leviathan.cpp:306-339`). `UpdateAI` waits until no live Ulduar Colossus (33237) is within
  250 yd and a player is within 250 yd, then plays 63.5 s of Brann radio lines, drops the flag, drives to
  `homePos` (322.4, -14.5) and zone-combats (`:351-406`). The static Colossi at (367.0, 12.8) and
  (368.8, -46.8) gate it; he spawns at (420.7, -15.4, 409.8).
- **First-pull vehicles** spawn at the Expedition Base Camp (`VEHICLE_POS_START`, `ulduar/instance_ulduar.cpp:512-520`,
  positions `:1347-1364`), 2 / 5 of each, with the spell click withheld (`:1418-1428`) until Brann's intro
  sets data 107 (`:1003-1009`, `:331-339`). The intro starts from Lore Keeper of Norgannon's gossip (menu
  10366 option 0, then 10477 option 0: towers kept) or Brann's (menu 10355 option 0, which also sets data 802
  and destroys all four towers, `:1089-1094`) (`data/sql/base/db_world/smart_scripts.sql:36716-36788`,
  `:51588-51636`; `data/sql/updates/db_world/2026_08_27_09.sql`). The camp is ~830 yd from the arena,
  through the gauntlet.
- **Every later pull.** `JustEngagedWith` sets `SPECIAL` (`:275-292`), which nothing resets; he never uses
  `IN_PROGRESS`, and `IsEncounterInProgress` reads his combat instead (`instance_ulduar.cpp:548-563`). A wipe:
  `EVENT_CHECK_PLAYERS` evades him once no living player is in his combat list (`:469-475`); `Reset()` on
  `SPECIAL` removes `NON_ATTACKABLE`, skips the RP and lays out a fresh, usable set at x 119.8, y 38 to -112
  (`:329-335`; `instance_ulduar.cpp:1386-1416`, positions `:1366-1383`), retiring the old set and its wrecks.
- **The Shield Wall closes on engage.** `TurnGates(false)` on reset opens GO 194416 at (148.2, -36.5);
  `TurnGates(true)` on engage closes it (`:531-565`). In play the raid boards at x 119.8, drives east through
  the open wall and pulls; the wall shuts behind.
- **Hard mode** is towers left standing: all four default to standing (`instance_ulduar.cpp:264-265`);
  `ActivateTowers` adds a +40% health aura and a hazard per standing tower at every engage (`:497-529`). A
  tower goes down with `SetData(<event>, 0)`, events 21030 Life, 21031 Storm, 21032 Frost, 21033 Flames
  (`instance_ulduar.cpp:984-998`, `ulduar.h:401-404`).
- **Kill:** `JustDied` sets `DONE` (`:636-655`). No prerequisite boss.

### The strategy under the DC driver

- **Nobody boards.** `FlameLeviathanVehicleNearTrigger` returns true only while `master->GetVehicle()`
  (`src/Ai/Raid/Uld/Trigger/UldTriggers_FlameLeviathan.cpp:37-50`): the driver never has one. The boarding
  action itself is master-free (`src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp:1230-1372`: vehicles
  within 200 yd, melee drive siege engines, ranged drive demolishers, then turrets and choppers; zero spare
  seats, `docs/raids/ulduar/flame-leviathan.md:33-34`).
- **Nobody drives in.** The vehicle movement rule is inert until he is engaged
  (`src/Ai/Raid/Uld/Definition/UldDefinition_FlameLeviathan.cpp:61-64`, `flame-leviathan.md:244-247`); in play
  the bot drivers follow the human into the arena. Under DC nothing follows anyone, so the fleet stays at x
  119.8. Engaging him from there closes the Shield Wall between him and the fleet.
- After the engage nothing reads the master.

### What DC needs

- **preState + one focus evade** (generic): set boss 0 to `SPECIAL` on the fresh instance (primed off
  `TO_BE_DECIDED` the way lane J's preDone is), then `EnterEvadeMode` the focus once so its `Reset()` reads it:
  vehicles at the arena edge and usable, him attackable, no RP. Every pull is then a re-pull. This skips the
  real first-pull path (camp, intro gossip, gauntlet, Colossi, RP), which no strategy node covers.
- **preData** (generic, below): `SetData(2103x, 0)` for each tower the test wants down.
- **preDespawn** (existing): Steelforged Defender 33236, Ulduar Colossus 33237, Runeforged Sentry 34234, Ulduar
  Gauntlet Generator 33571 (spawns a Defender on any target within 80 yd every 4 s, `:1241-1257`).
- **Readiness: every member seated and inside the arena** (generic: on a vehicle and within R yd of a start
  point east of the wall), then an aggro start: `EngageWithTarget` on the tank, which his `AttackStart`
  turns onto the tank's vehicle (`:267-273`).
- **Fork, two changes:** boarding when no real player masters the bot on this instance; a pre-pull drive that
  takes the seated fleet through the Shield Wall to a staging line and holds there.

### Drop, comps, risks

- **Drop (108.0, -37.0, 409.8, o 0.0):** 11.8 yd west of the vehicle line, 210 yd from `homePos`, 313 yd
  from his spawn. Within 60 yd, all removed by the preDespawn above: Runeforged Sentry 136808 (patrol x 103.3,
  y 43.5 to -24.7, path 1368080), Steelforged Defenders 137146, 137411, 137299 standing on the vehicle line
  plus ten more, Colossus 137476 (patrol (81.4, -52.4) to (5.9, -54.3), 27 yd), generators 136077 (52 yd) and
  136091 (56 yd). navprobe: the drop and the staging line (x ~175).
- **Comps:** `Sized(25, 2, 4)`, `Sized(10, 2, 2)`; tanks and healers have no vehicle duty. The raid must be
  exactly 25 / 10 with at least 5 / 2 melee and 5 / 2 ranged for the driver seats; every roster draft has that.
- **Risks:** which side of the Shield Wall the x 119.8 line is on cannot be read from SQL (top); DC's refresh
  runs `maintenance` and its revive teleports members who may be seated (live only); one lost boarding race
  leaves a bot on foot and the "seated" readiness waiting until `no_progress`.
- **Size: large** (two fork changes, three DC features).

## Thorim (boss 8, entry 32865)

### Start and reset in the core

- **Reset** summons the arena start pack in a mock battle (Jormungar Behemoth 32882, three Captured Mercenary
  Soldiers 32885, Captain 32908, Dark Rune Acolyte 32886; Alliance versions because the first player is
  Alliance) and the whole gauntlet (`ulduar/boss_thorim.cpp:351-387`, `:421-442`), then locks the lever, shuts
  both colossus doors and opens the arena fence GO 194559 at (2135.0, -216.9) (`:389-405`).
- **Start:** a player or pet damaging any pack member zone-combats Thorim and turns the pack (`:1073-1097`);
  `JustEngagedWith` sets `IN_PROGRESS` and roots him on the balcony at (2131.0, -297.6, 438.3) (`:505-513`). No
  prerequisite boss.
- **Six pack deaths** (`:483-491`, `:454-478`) unlock the lever and the corridor trash, close the fence
  (`:654-661`), summon Sif 15.5 s later (`:662-668`) and start phase 1 at 20 s: arena adds, an arena check every
  5 s that wipes the raid with an orb once no living player is in x 2085-2185, y -305..-214, z < 425
  (`:633-641`, `:691-707`), a 5 min cap (`:708-713`), and Sif's 150 s Touch of Dominion clock (`:669-682`,
  `:815-824`).
- **Lever** GO 194264 at (2173.3, -252.9, 420.1): using it opens the Dark Iron Portcullis GO 194560 at
  (2180.8, -263.0), the gauntlet's entrance (`:1659-1672`). The Runic Colossus' death opens the first door
  (`:1363-1377`), the Rune Giant's allows the hit (`:1474-1484`); phase 2 starts when a player above z 430
  damages him, and hard mode is Sif joining because Touch of Dominion (62565) is not on him yet (`:515-547`).
- **Kill:** lethal damage sets `DONE` and he turns friendly (`:551-590`). **Wipe:** `JustExitedCombat` evades,
  `NOT_STARTED`, and `Reset()` respawns everything and relocks (`:407-442`).

### The strategy under the DC driver

- **The gauntlet never moves.** `ThorimGauntletPositioningTrigger` and its action
  (`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.cpp:47-80`, `src/Ai/Raid/Uld/Action/UldActions_Thorim.cpp:216-253`)
  wait for the leader at the corridor mouth (2172.4, -258.3), then walk the squad to the leader's lane
  waypoint. Under DC the leader is slot 0, the main tank, who stays in the arena by design
  (`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp:389-491`). Only the gauntlet healers move, tethering back to
  him. This is the known gap at `docs/raids/ulduar/thorim.md:777-802`.
- **No bot uses the lever** (no lever code under `src/Ai/Raid/Uld`), so the portcullis stays shut anyway.
- Works under DC: `ThorimFallFromFloorAction` teleports a fallen bot onto slot 0 (`UldActions_Thorim.cpp:373-382`);
  `ThorimResolveGauntletIndex` falls back to the bot's own lane (`UldEncounter_Thorim.cpp:1384-1393`); the
  balcony walk has no master check.
- Net effect today: the arena fights adds until the 5 min orb. Thorim cannot be tested unattended without the
  two fork changes below.

### What DC needs

- **Roster anchor** for boss 8.
- **Aggro start on a separate engage entry** (generic): engage the Jormungar Behemoth (the same entry for both
  factions) on the tank, so the tank's hits start the encounter as a player's would. The focus must stay
  Thorim: the pack fights itself all the time, so a pack focus would never read "ready".
- **Fork, two changes:** a bot leads the gauntlet (the gauntlet tank, already picked by `AssignThorimSquads`,
  walks the lane chain, and the squad keys on him while no real player stands in the gauntlet squad); a
  gauntlet bot uses the lever once it is usable.

### Drop, comps, risks

- **Drop (2135.0, -235.0, 419.85, o 4.71):** inside the arena check box, so the fence closes behind the raid;
  16-20 yd north of the pack (soldiers at (2120-2127, -251..-259), captain (2131.3, -259.2), Behemoth
  (2146.6, -266.7)), 62 yd from Thorim, 42 yd from the lever. Thorim's own tank anchor (2135.0, -263.1) sits
  inside the mock battle, where a stray hit starts the pull before DC's refresh. The stands' Dark Runes carry
  `unit_flags` 768 (immune). Dark Rune Thunderer 137482 and Ravager 137484 patrol x 2128.6 / 2138.0, y -150.6
  to -199.5, z 412.3 outside the fence: 35.6 yd at closest, 7.5 yd lower; preDespawn 33754 / 33755 if they react.
- **Comps:** `Sized(25, 2, 4, {NeedClass(CLASS_PALADIN, 1), NeedClass(CLASS_HUNTER, 1)})`, same at 10 with
  2 / 2 (Frost Resistance Aura once Sif joins, Aspect of the Wild). The gauntlet takes 1 tank, 2 / 1 healers and
  9 / 4 DPS.
- **Risks:** a bot-led gauntlet has to make Sif's 150 s clock, which today relies on a human walk (top);
  Thorim walking back up to his balcony after a phase 2 wipe (live only).
- **Size: large** (two fork changes; DC side small).

## Algalon (boss 13, entry 32871)

### Start and reset in the core

- **Summon, once per instance.** The console (GO 194628 at 10, lock 579; GO 194752 at 25, lock 1831; both at
  (1646.2, -174.7, 427.3)) runs `go_celestial_planetarium_access` (`ulduar/boss_algalon_the_observer.cpp:1157-1243`):
  the user must hold an item the GO's lock names (Lock.dbc; `item_template` has 45796 "Celestial Planetarium
  Key" and 45798 "Heroic Celestial Planetarium Key"). It summons Brann, opens sigil doors 01 / 02,
  perm-binds every player and deletes itself 5 s later. Brann walks in and summons Algalon 7.5 s after his
  talk point (`:988-999`); the first intro keeps him `ImmuneToPC` for 36 s (`:429-459`, `:696-701`).
- **Start:** he then stands passive until attacked. `JustEngagedWith` zone-combats, keeps him unselectable for
  26 s (8.5 s on later pulls), sets `IN_PROGRESS` at 14 s (1.5 s), and on the first engage starts the 300 min
  despawn countdown (`:513-556`; `instance_ulduar.cpp:1010-1017`).
- **Wipe:** with no unphased player alive within 120 yd he Ascends (`:369-382`), the encounter goes `FAIL`,
  he despawns (hard reset) and the map resummons him about 20 s later with a 5 s intro (`:384-396`,
  `:794-803`; `instance_ulduar.cpp:686-710`, `:747-752`). Leaving the 47 yd disc also Ascends (`:651-660`).
- **Kill:** `DONE` at the outro start, at 2% health (`:633-648`, `:804-810`). No prerequisite boss.

### The strategy under the DC driver

No master reads in the Algalon files. It needs two bot tanks plus a handler at 25
(`docs/raids/ulduar/algalon.md:403-412`, `:760-763`).

### What DC needs

- **Roster anchor** for boss 13, and preDespawn Chamber Overseer 34197 (elite, 19.7 yd from the console).
- **Once-per-run GameObject use** (generic): give the user the items the GO's lock names (read from `sLockStore`,
  as the GO script does, so the row carries no key ids), bring it into reach, use the GO, send it back to the
  drop. Rows list both console entries; only the difficulty's one exists.
- **Readiness: the focus is attackable** (generic: no `IMMUNE_TO_PC`, no `NOT_SELECTABLE`). Engaging him mid
  intro runs `JustEngagedWith` early; its `events.Reset()` cancels `EVENT_INTRO_FINISH`, the only thing that
  clears `ImmuneToPC`, and the pull is dead. It also covers the first pull's Brann walk and intro without a
  timer. Aggro start as XT-002.

### Drop, comps, risks

- **Drop (1632.0, -282.0, 417.3, o 4.71):** inside the disc around (1632.7, -302.8), 20.8 yd north of where he
  lands, 15.9 yd inside the room door GO 194910 (`DOOR_TYPE_ROOM`, shut while `IN_PROGRESS`). The only spawns
  within 60 yd are Dark Matter 33089 with phase mask 16, which exist only inside a black hole. The room's floor
  GOs have collision off (`instance_ulduar.cpp:886-892`), so navprobe must find the static floor.
- **Comps:** `Sized(25, 3, 4, {NeedSpec(CLASS_PRIEST, "shadow pve", 1)})`, `Sized(10, 2, 2, {same})`. The
  strategy's backup Big Bang soaker is a second shadow priest; every roster draft has one, so the backup falls
  to a swap tank (`algalon.md:61-66`).
- **Risks:** the console used from a server-side opcode with a granted key (live only, top); the perm-bind
  (DC's teardown unbind clears it, as for Karazhan chess).
- **Size: medium** (two small DC features plus the anchor).

## Yogg-Saron (boss 12, Sara 33134, Yogg-Saron 33288)

### Start and reset in the core

- **Start:** every 5 s Sara looks for a living non-GM player within 90 yd, measured in 3D
  (`ulduar/boss_yoggsaron.cpp:829-843`; `src/server/game/AI/ScriptedAI/ScriptedCreature.cpp:591-609`), and
  `InitFight` sets `IN_PROGRESS`, zone-combats, despawns the Observation Ring keepers and wakes the room's;
  the chamber doors GO 194773 at (1877.9, -24.1, 335.3) close 15 s later (`:532-558`). No prerequisite:
  `CheckRequiredBosses` (Vezax `DONE`, `instance_ulduar.cpp:1324-1340`) is only asked by `BossAI`
  (`ScriptedCreature.cpp:694-701`), and Sara is not one.
- **Keepers.** The Observation Ring keepers (Freya 33241, Hodir 33213, Mimiron 33244, Thorim 33242) appear when
  their boss is `DONE` (`instance_ulduar.cpp:611-630`, `:341-353`), in Auriaya's ring at z 411. Each runs a C++
  scripted gossip (option 0, then confirm option 0, `ulduar/ulduar.cpp:163-212`); 1 s later the keeper teleports,
  ORs its bit into persistent data 0 (`PERSISTENT_DATA_WATCHERS_MASK`) and sets data 62941, and Sara summons
  the room keepers from the mask (`ulduar.cpp:135-148`; `instance_ulduar.cpp:980-983`;
  `boss_yoggsaron.cpp:572-584`, `:683-689`). Bits: Freya 0, Hodir 1, Mimiron 2, Thorim 3 (`ulduar.h:441-444`).
  The mask persists for the instance, so the choice is made once per run. Hard mode is fewer keepers; at 25 any
  missing keeper adds Deafening Roar (`:700-702`). The fork reads the same mask
  (`src/Ai/Raid/Uld/Util/UldHardMode.cpp:73-110`).
- **Wipe:** Sara evades once nobody sane is within 90 yd and nobody is alive in the illusions
  (`:781-792`, `:845-850`). The state stays `IN_PROGRESS` through a 30 s recovery: doors open at 20 s,
  `NOT_STARTED` with clouds and room keepers at 30 s (`:464-474`, `:794-827`), and the 90 yd scan resumes.
- **Kill:** Yogg-Saron's `JustDied` sets `DONE` (`:1194-1206`).

### The strategy under the DC driver

No blockers. `IsMasterIsInIllusionGroup` and `IsMasterIsInBrainRoom`
(`src/Ai/Raid/Uld/Trigger/UldTriggers_YoggSaron.cpp:99-108`) have no callers. `GetYoggSaronBrainTeam` gives the
master a portal only if the master is in the group (`src/Ai/Raid/Uld/Util/UldEncounter_YoggSaron.cpp:1025-1050`);
the driver is not, so every portal goes to a bot. Bots walk in from 72-96 yd themselves
(`docs/raids/ulduar/yogg-saron.md:35-50`).

### What DC needs

- **Roster anchor** for boss 12 on Sara.
- **preData** (generic): `StorePersistentData(0, mask)`, then `SetData(62941, 0)` so Sara summons the room
  keepers. The faithful alternative, the keepers' own gossip, costs more: DC refuses menu-0 picks, which is
  what a scripted menu reads as (`DcTestDungeonRegistry.cpp:552`); its gossip start runs every pull while a
  chosen keeper despawns; and it needs preDone 7-10 plus clearing Auriaya's ring.
- **Teleport start** (generic): after the refresh and settle at a drop outside 90 yd, move the raid to a start
  point inside it and let Sara start. A drop inside 90 yd fails: she pulls 5 s after her recovery, and DC's
  Prepare takes a boss that engages first as the pull and drops the unfinished refresh
  (`DC:src/TestRun/DcTestRepull.cpp:74-80`).

### Drop, comps, risks

- **Drop (1866.0, -24.0, 335.0, o 0.0):** 11.9 yd west of the chamber doors, 114 yd from Sara in 3D. Nearest
  hostile at that level over 100 yd off: Faceless Horror 137566 at (1880.2, 78.2) and Vezax at (1852.8, 81.4),
  behind the Runed Stone Door GO 194750 at (1854.9, 31.5). **Start point (1896.0, -25.0, z from navprobe,
  ~330):** inside the doors, so their close 15 s in locks nobody out, 84 yd from Sara in 2D, outside the cloud
  orbits (43 yd); navprobe must give a z that keeps it under 90 yd in 3D.
- **Comps:** `Sized(25, 2, 4)`, `Sized(10, 2, 2)`. The anti-fear need (priest or shaman) is met by every roster
  draft and DC cannot express "either" in one need.
- **Risks:** a start point at the wrong z never starts the fight and sits until `no_progress` (top); a member
  left outside the doors when they close.
- **Size: medium** (two small DC features plus the anchor).

## The smallest set

| Piece | Kind | Bosses |
|---|---|---|
| Ulduar roster patch: Thorim on 8, Sara on 12, Algalon on 13 | DC data, ToC pattern | Thorim, Yogg-Saron, Algalon |
| preData: one-time `SetData` / `StorePersistentData` after preDone | DC generic | Yogg-Saron (keepers), Flame Leviathan (towers) |
| Teleport start | DC generic | Yogg-Saron |
| Once-per-run GameObject use with the lock's items | DC generic | Algalon |
| Readiness: focus attackable | DC generic | Algalon (helps any summoned boss) |
| Aggro start on a separate engage entry | DC generic | Thorim |
| preState (any boss state) plus one focus evade | DC generic | Flame Leviathan |
| Readiness: every member seated and within R of a start point | DC generic | Flame Leviathan |
| Boarding without a real-player master; pre-pull drive past the Shield Wall | fork | Flame Leviathan |
| Bot-led gauntlet; a bot pulls the lever | fork | Thorim |

Nothing is impossible unattended. Left untested by design: Flame Leviathan's first-pull path, and the keeper
gossip and console clicks themselves (replaced by data and a harness step).

## Questions for the user

1. **Should Yogg-Saron and Algalon come first?** These two need only harness work in DC: no change to how the
   bots fight. Thorim and Flame Leviathan also need strategy changes (questions 2 and 3). Starting with the two
   cheap ones gets more bosses into the polish loop sooner. **Recommendation: yes, build Yogg-Saron and Algalon
   as the next DC lane.**

2. **May a bot lead Thorim's gauntlet and pull the lever?** Today the gauntlet group walks only behind the raid
   leader, and in your raid that is you. Under DC the leader is the main tank, who stays in the arena, so the
   gauntlet never starts and every pull wipes at 5 minutes. The change: while no real player is in the gauntlet
   group, the gauntlet tank walks the corridor route himself, the group follows him, and a gauntlet bot pulls the
   lever. When you lead, nothing changes. It is new strategy behaviour, so it needs your yes. **Recommendation:
   yes.**

3. **Should Flame Leviathan be automated at all?** It is the biggest job: two strategy changes (bots board
   without you, and drive the vehicles into the arena before the pull) and three new harness features. It would
   also test only the repeat pull, never the first one (intro gossip, vehicles at the base camp, the gauntlet
   drive). **Recommendation: not now. Decide again once the other three run.**

4. **If Flame Leviathan is automated, how many towers?** Each standing tower gives him +40% health and adds a
   hazard, so 4 towers is the full hard mode the strategy was tuned against, and 0 towers is normal mode. The
   harness can knock down any towers before the first pull. **Recommendation: all 4 standing.**

5. **Which Keepers should help against Yogg-Saron?** Options: all four (normal mode, so a kill does not count
   toward "polished"), Freya only, or Thorim only. The strategy handles all three; the latest phase 3 work
   (Guardian floor, melee on Yogg) was decided for the Freya-only race. **Recommendation: Freya only.**

6. **Set the Keeper choice as instance data, or click the Keepers' gossip?** The fight only reads which Keepers
   were chosen, a value stored in the instance. The harness can write that value directly before the first
   pull. Clicking the real gossip needs more harness work and clearing Auriaya's room first, for the same fight.
   **Recommendation: write the value directly.**

7. **Add a second shadow priest to the Ulduar pools for Algalon?** At 25 the strategy wants a backup Big Bang
   soaker, and every pool has one shadow priest. Without a second one a swap tank takes the backup role, which
   the strategy already supports. Adding one means replacing a DPS in each pool. **Recommendation: no change
   for now; revisit only if Big Bang deaths show up.**
