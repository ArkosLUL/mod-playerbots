# navprobe — offline navmesh + height probe

## Context

Every destination the raid strategies compute — ring points, hold spots, dodge headings, formation
slots — is validated by reasoning alone. `MoveTo` silently returning false on an off-navmesh point
is a known killer (Void Reaver), and `UpdateAllowedPositionZ` silently rewriting Z is the other
half. There is no way to ask either question outside a live pull.

The Eye of Eternity work made the gap concrete. Answering "does map 616 have a navmesh?" took
hand-hexdumping files in a Docker volume. The answers, all verified during planning and all
reusable as the tool's first test cases:

- `616.mmap` exists (28 B, `dtNavMeshParams`, `maxTiles = 25`) with **zero `.mmtile` files**. It is
  the only such map id of 98.
- Map 616 has **no vmaps at all** — no `616.vmtree`, no `616*.vmtile`. It is the only mmap-owning
  map id with no vmtree.
- All 25 of its `.map` tiles are 616 bytes with `MHGT` flags `0x09`
  (`MAP_HEIGHT_NO_HEIGHT | MAP_HEIGHT_HAS_FLIGHT_BOUNDS`) and **`gridHeight = 0.0f`**.

Chained through the core: `GridTerrainData::getHeightFromFlat` returns `0.0`
(`GridTerrainData.cpp:119, 229-234`); `Map::GetHeight` takes it because
`fuzzyGe(z, gridHeight - GROUND_HEIGHT_TOLERANCE)` holds and vmaps give nothing
(`Map.cpp:1164-1194`); `UpdateAllowedPositionZ` sees `max_z = 0.0 > INVALID_HEIGHT` and clamps
(`Object.cpp:1637-1657`). The EoE platform is near Z 266, so **every non-flying Z on map 616 is
slammed 266 yards down to 0.0**. That is the disk dive, exactly.

A tool that prints those two lines in one command pays for itself.

**Decisions already taken (do not relitigate):** C++ against real Detour, not a Python
re-implementation; lives in `src/tools/navprobe/`; run via a service in
`docker-compose.override.yml`; full port of PathGenerator's fresh-path decision tree; includes the
vmap/ADT height half. Four queries: coverage, point, path, ring.

## Constraints

- **No compiler on the Windows host.** No cmake anywhere; VS 18's MSVC exists but is not wired up.
  The build is Docker (`apps/docker/Dockerfile`, ninja + clang, `TOOLS_BUILD="all"`). Every build
  and run goes through a container. This is fine — `ac-client-data` is a named volume a container
  mounts directly, and `ac-worldserver` is already running with it at
  `/azerothcore/env/dist/data`.
- **`PathGenerator` cannot be linked.** It lives in `src/server/game/Movement/MovementGenerators/`
  and pulls in `Unit`, `Map`, `DisableMgr`, `Metric`. Detour itself is shared; the *logic* is a
  port and will drift if `PathGenerator.cpp` changes. Say so in the tool's README.
- The core repo is a personal fork (`ArkosLUL/azerothcore-wotlk`, branch `Custom`, clean at plan
  time). Adding `src/tools/navprobe/` is one new directory and **no edits to any existing core
  file**.
- Read-only against client data. The volume is writable; nothing here writes to it.

## Step 0 — mirror the plan

Copy this file to
`modules/mod-playerbots/docs/plans/navprobe-offline-navmesh-tool/navprobe-offline-navmesh-tool.PLAN.md`
and delete it when the work lands. That is the planning directory already in use, and the tool
exists to serve the module even though the code lands in the core fork.

---

## Part 1 — the target (zero CMake edits)

`src/tools/CMakeLists.txt` globs subdirectories of `src/tools/` and generates one executable per
directory (`GetToolsList`, `src/cmake/macros/ConfigureTools.cmake:34-50`). Target name is
`string(TOLOWER dirname)`, so `src/tools/navprobe/` yields target **`navprobe`**, installed to
`${CMAKE_INSTALL_PREFIX}/bin` on UNIX (`src/tools/CMakeLists.txt:180-184`).

It inherits the standard tool link set (`src/tools/CMakeLists.txt:133-144`): `common`, `mpq`,
`zlib`, `Recast`, `g3dlib`, `fkYAML`, plus `acore-dependency-interface`. **Detour arrives
transitively through `common`** (`src/common/CMakeLists.txt:79-84`, gated on `BUILD_TOOLS_MAPS`,
which `ConfigureTools.cmake` sets for any enabled non-dbimport tool). Include path already covers
`${CMAKE_SOURCE_DIR}/src`, so `src/common/**` headers are directly usable.

Files, all new, all inside `src/tools/navprobe/`:

| File | Holds |
|---|---|
| `main.cpp` | arg parsing, subcommand dispatch, output formatting |
| `NavData.h/.cpp` | navmesh loading, vmap loading, `.map` height reader |
| `NavQuery.h/.cpp` | the `PathGenerator` port |
| `README.md` | what it mirrors, what it deliberately does not, and the drift warning |

### 1.1 Navmesh loading

Do **not** call `MMapMgr::LoadNavMesh` — it resolves `DataDir` from `sConfigMgr`, which a tool
never initialises. Reuse the format-string constants from `src/common/Collision/Management/MMapMgr.h:48-49`
(note this fork uses **3-digit** map ids, `"{}/mmaps/{:03}.mmap"` and
`"{}/mmaps/{:03}{:02}{:02}.mmtile"`, not the upstream `%04i`) and own the file I/O:

- Read the 28-byte `dtNavMeshParams` raw, `dtAllocNavMesh`, `mesh->init(&params)`.
- Scan the mmaps directory (`stdfs` is linked) for `{:03}*.mmtile`; validate the 56-byte
  `MmapTileHeader` (`src/common/Collision/Maps/MapDefines.h:64-79`): `mmapMagic == MMAP_MAGIC`
  (0x4d4d4150), `dtVersion == DT_NAVMESH_VERSION` (7), `mmapVersion == MMAP_VERSION` (20). Then
  `dtAlloc(size, DT_ALLOC_PERM)`, read, `addTile(..., DT_TILE_FREE_DATA, 0, &ref)`.
- **Cross-check the filename ordering.** The generator formats the tile name with
  `(mapID, tileY, tileX)` (`MapBuilder.cpp:866`) while the loader reads it as `(mapId, x, y)`
  (`MMapMgr.cpp:71`). Mirror the *loader*, then compare the x/y parsed from the filename against
  `dtMeshHeader.x/.y` in the tile just added and warn on mismatch. That converts a latent asymmetry
  into a printed fact.
- Preload every tile for the map. `maxTiles` in the params is sized for it, and the biggest map is
  778 tiles. **This diverges from the server**, which only holds tiles for loaded grids
  (`GridTerrainLoader.cpp:74-85`). navprobe answers "reachable in principle", which is the design
  question; note the difference in `--help` and README.

### 1.2 Query core — the `PathGenerator` port

Mirror `src/server/game/Movement/MovementGenerators/PathGenerator.cpp` for the cold, no-prior-
corridor case. Reuse `dtQueryFilterExt` from `src/common/Navigation/DetourExtended.h` — it is in
`common`, so the slope-cost override comes for free rather than being re-derived.

- Swizzle `{y, z, x}` on every coordinate handed to Detour.
- `HaveTile`: `calcTileLoc` → guard `tx < 0 || ty < 0` → `getTileAt(tx, ty, 0)` (`.cpp:817-831`).
- `GetPolyByLocation`: `findNearestPoly` with extents `{3, 5, 3}`, then the `{3, 50, 3}` fallback
  (`.cpp:222-255`). Skip the cached-corridor branch — there is no corridor offline.
- Filter include flags default to the player profile `NAV_GROUND | NAV_WATER | NAV_MAGMA`,
  exclude 0 (`.cpp:749-774`). `--creature` switches to the creature branch; `--nav LIST` overrides.
- `CalculatePath`: `IsValidMapCoord` on both ends; the `!HaveTile(start) || !HaveTile(dest)` guard
  → `BuildShortcut()` and `PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH` (`.cpp:173-179`).
- `BuildPolyPath`: the 7.0f far-from-poly threshold and its `FARFROMPOLY_START/END` flags; the
  fly / swim / falling short-circuits driven by `--can-fly`, `--can-swim`, `--falling`; the
  same-poly case; `findPath` with `MAX_PATH_LENGTH = 74`; `NORMAL` vs `INCOMPLETE` decided by
  whether the last poly equals the end poly (`.cpp:600-607`).
- `BuildPointPath`: `--straight` → `findStraightPath`; default → ported `FindSmoothPath`
  (`SMOOTH_PATH_STEP_SIZE 4.0f`, `SMOOTH_PATH_SLOP 0.3f`, `MAX_VISIT_POLY 16`, `FixupCorridor`,
  the `result[1] += 0.5f` nudge). `pointCount >= pointPathLimit` → `BuildShortcut` + `PATHFIND_SHORT`
  (`.cpp:685-690`).
- Report the resulting `PathType` as both the raw hex mask and decoded flag names. **Always print a
  mask, never compare with `==`** — the tool exists partly because `MoveToLOS` gets that wrong.

**Deliberately not ported**, and stated in `--help`: corridor reuse (needs a prior path), raycast
mode (`_useRaycast`), and the `DT_SLOPE_TOO_STEEP` gate, which needs liquid data — put the last
behind `--slope-check`, default off, and print that it was skipped.

### 1.3 Height half

- **vmaps**: `VMapMgr2` from `src/common/Collision/Management/` — linkable. `loadMap(vmapsDir,
  mapId, tx, ty)` then `getHeight(mapId, x, y, z, 50.0f)`; returns `VMAP_INVALID_HEIGHT_VALUE`
  when the map has none, which is what map 616 will report.
- **ADT**: a local reader for `{:03}{:02}{:02}.map`. Redeclare `map_fileheader` (44 B) and
  `map_heightHeader` mirroring `src/server/game/Grids/GridTerrainData.h:59-72, 83-86` — that header
  is in `server/game` and must not be included. Implement all four forms: flat
  (`MAP_HEIGHT_NO_HEIGHT` → `gridHeight`), int8, int16, float, copying the interpolation from
  `GridTerrainData::getHeightFromUint8/Uint16/Float`. Note in a comment that it mirrors
  `GridTerrainData` and has to be kept in step.
- **Combine** per `Map::GetHeight` (`Map.cpp:1164-1194`): take the grid height when
  `fuzzyGe(z, gridHeight - GROUND_HEIGHT_TOLERANCE)`, then the vmap-vs-map selection.
- **Then report what `UpdateAllowedPositionZ` would do** (`Object.cpp:1614-1681`) for the selected
  profile, including the `max_z > INVALID_HEIGHT` gate and the hover offset.
- The dynamic tree (GameObject collision) is not available offline. Say so once in the output.

### 1.4 CLI

```
navprobe --map ID [--data DIR] <command> [flags]

  coverage                          params, tile count, .map and vmap presence
  point   X Y Z                     on-mesh test, snapped Z, height breakdown
  path    X1 Y1 Z1  X2 Y2 Z2        PathType mask, poly count, waypoints, length
  ring    --centre X Y Z --radius R --headings N      pass/fail table

  profile: --can-fly --can-swim --falling --creature --nav LIST
  path:    --straight --slope-check
  output:  --format table|json
```

`--data` defaults to `/azerothcore/env/dist/data`, the container mount point.

---

## Part 2 — run wiring

One addition to `docker-compose.override.yml` — the file `docker-compose.yml` explicitly nominates
for local changes. No Dockerfile edit; the existing `build` stage already installs every tool to
`/azerothcore/env/dist/bin`.

```yaml
  ac-navprobe:
    build:
      context: .
      target: build
      dockerfile: apps/docker/Dockerfile
    volumes:
      - ${DOCKER_VOL_DATA:-ac-client-data}:/azerothcore/env/dist/data:ro
    entrypoint: ["/azerothcore/env/dist/bin/navprobe"]
    profiles: [tools]
```

Run: `docker compose --profile tools run --rm ac-navprobe --map 616 coverage`.

The `build` stage carries the toolchain, so the image is large — acceptable for a dev tool, and it
means no second build path to keep working. ccache is already mounted for that stage.

---

## Verification

Ground truth below was established during planning by reading the volume directly, so these are
exact expected values, not sanity checks.

1. `--map 616 coverage` → params `orig (0.0, 1.175494e-38, -533.3333)`, tileWidth/Height
   `533.3333`, `maxTiles 25`, **tiles loaded 0**, 25 `.map` tiles, **no vmaps**.
2. `--map 603 coverage` → **66** tiles, 82 `.map` tiles, `603.vmtree` present, `maxTiles 82`.
3. `--map 616 path` between any two platform points → `0x11 PATHFIND_NORMAL|PATHFIND_NOT_USING_PATH`.
4. `--map 616 point <any x y 266>` → ADT flat **0.0**, vmap none, and
   `UpdateAllowedPositionZ -> 0.0`. This is the disk dive; if the tool does not print it, the
   height chain is wrong.
5. **Fidelity test, the only real one.** Pick an Ulduar coordinate, run `--map 603 point` and
   `--map 603 path`, then run `.mmap loc` and `.mmap path` in-game at the same coordinates
   (`src/server/scripts/Commands/cs_mmaps.cpp`) and compare tile `[x,y]`, poly ref, path Type and
   Length. Divergence here means the port is wrong, and nothing else in this list would catch it.
6. `--map 603 ring` around a known-good boss position → mostly on-mesh, with any misses landing
   where geometry says they should.

Note for step 5: the in-game commands only see grids the server has loaded, while navprobe preloads
everything. Compare somewhere a player is standing.

---

## Part 3 — doc follow-ups (after the tool runs)

Per the global rule, invoke `/compact-docs-writer` once before touching the mod-playerbots docs; it
covers the whole cycle.

- `modules/mod-playerbots/docs/raids/eye-of-eternity.md`, **"No navmesh"** section — extend to the
  full picture: no vmaps either, `.map` tiles flat at `gridHeight = 0.0`, therefore `GetMapHeight`
  returns 0.0 map-wide and `UpdateAllowedPositionZ` drops Z from ~266 to 0. Replace the current
  claim that the height half survives "because `GetMapHeight` reads vmaps" — on this map it reads
  the flat `.map` header instead, and the consequence is far larger than that sentence implies.
- `modules/mod-playerbots/docs/engine/pitfalls.md` — same correction to the qualifier on the
  off-mesh entry.
- `src/tools/navprobe/README.md` — new: what it mirrors, what it omits, the preload-vs-loaded-grids
  divergence, and the warning that it tracks `PathGenerator.cpp` by hand.

## Out of scope

- Regenerating map 616's vmaps or mmaps. It needs client MPQs in `var/client`, which is empty, and
  it is a data-extraction problem, not a bot problem.
- GameObject / dynamic-tree collision.
- Corridor reuse and raycast pathing.
- Any write to the client-data volume.
