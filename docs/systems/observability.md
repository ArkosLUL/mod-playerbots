# Raid observability (RaidObs)

Per-pull NDJSON traces of where every raid member stood, what its engine decided, and what killed it.
Answers what video cannot: position over time, and the action that produced it.

`mod-chronicle` already writes a full WotLK combat log per instance to `logs/chronicle_logs/`. RaidObs
does not replace it — it records what a combat log has no field for: **position and bot intent**. It
duplicates enough damage/heal/aura data that a pull file stands alone.

Code: [src/Bot/Obs/](../../src/Bot/Obs/). `RaidObs.h` is the public contract; `RaidObsSession.h` is
private to the directory, holding the session type and shared helpers. One
`RaidObs*.cpp` per concern: `Session` (registry, write path, roster), `Snapshot` (rows, hazard sweep,
watched set), `Lifecycle` (open/close, map and script events), `Config` (config, retention,
`Status`), `Combat` (damage, heal, aura, cast, death), `Engine` (verdict ticks, moves, notes),
`Scripts` (ScriptMgr hooks).

Readers in [tools/botobs/](../../tools/botobs/): `postmortem.py` for one pull, `batch.py` for the
corpus, `bosses/<boss>.py` per encounter. All import the `raidobs/` package, a module per question,
which imports none of them. Their `--help` lists every view and flag; what it does not say is below.
A boss reader consumes the probes its encounter publishes rather than re-deriving them: a re-derived
model rots while its constants still match the source.
The suite is `python -m unittest discover -s tools/botobs/tests`. There is deliberately no C++ unit
test target: the core's socket exists (`src/test/CMakeLists.txt` reads `ACORE_MODULE_TEST_SOURCES`,
which nothing sets), but `BUILD_TESTING=ON` forces gcov flags on the whole build and `unit_tests`
links `game` plus `modules`, so a module unit test links the entire server — a slower loop than the
one it would shorten.

## Reading a trace

Traces land in `<LogsDir>/botobs/<map>_<instance>_<boss>_<epoch>.ndjson`, named from the creature that
engaged. `.playerbots debug obs` lists what is open. NDJSON is one record per line with no enclosing
array, so `grep '"e":"death"'` beats parsing the file.

A fight with several bosses lands under whichever one engaged, so `BOSS_ALIASES` in
`raidobs/encounter.py` maps every such slug to one encounter: Iron Assembly pulls file as Brundir or
Molgeim, Freya's three-elder pulls as Stonebark. Selection and grouping both go through it, and so
does `cfg.hardmode`, whose keys are these names — an unmapped slug means the hard-mode check silently
never applies.

Trial of the Crusader: `gormok-the-impaler`, `acidmaw`, `dreadscale`, `icehowl` and `fire-bomb` →
`northrend-beasts`; the 28 champions of `ToCHelpers_FactionChampions.h`, both factions →
`faction-champions`; `fjola-lightbane`, `eydis-darkbane` → `val-kyr-twins`. Jaraxxus and Anub'arak
engage under their encounter's slug. Never alias `the-lich-king`: ICC's boss, and a passive
boss-flagged unit here. `anub-arak` is also Azjol-Nerub's boss, so a slug cannot tell the two apart.

Icecrown Citadel hands RaidObs a boss-id → slug table, so a boss-state open files under its contract
slug (`encounter.ICC_KEY_PREFIXES`). The aliases cover a pull no table named: `blood-council` (the
DBC name Valanar's credit takes on normal), `prince-keleseth`, `prince-taldaram`, `prince-valanar`
(heroic has no encounter list, so the creature names it) → `blood-prince-council`; `queen-lana-thel`
→ `blood-queen-lana-thel`; the enemy captain, `muradin-bronzebeard` or `high-overlord-saurfang`, →
`icecrown-gunship-battle`, whose credit is a spell; `stinky`, `precious` → `stinky-and-precious`,
since `MarkPull` names a session after its creature and only a map-named one is ever renamed. None
collides with another raid's slug, but Utgarde Keep's Keleseth and Ahn'kahet's Taldaram share the
princes' slugs once `Obs.Maps` traces a 5-man. Deathbringer's Rise's friendly Muradin and Saurfang
are boss-flagged too but never fight the raid, so they never name a file.

`--coverage` and `--probes` join on that name too, and the map-name fallback (`ulduar`) matches no
encounter, which would fold every node into the "gate shut this pull" line and read as a closed gate
when only the join failed. So a slug still equal to its map's `Map.dbc` name (`encounter.MAP_SLUGS`)
resolves to the boss-flagged unit that traded damage; the flag alone is not enough, as Yogg's room
holds the four Keepers. **Any other slug stands**: Mimiron files under his encounter name, yet the
first boss-flagged unit to trade damage is Leviathan Mk II. `--boss` opens only map-named files beyond
those its name selects.

A shut encounter gate reads like a false condition, so `--coverage` folds a node owned by another
encounter of the raid into that gated line rather than list it NEVER. The owner is the trigger-name
prefix the node leads with: `ULD_PREFIXES` holds the Ulduar ones, and `TOC_PREFIXES` holds
`gormok`, `northrend worms`, `icehowl`, `jaraxxus`, `faction champions`, `twin valkyr`, `anubarak`.
Every per-encounter ToC trigger name leads with one, so a new ToC encounter prefix needs a row.
`ICC_PREFIXES` holds ICC's: `icc ` then the boss, spelled out or short (`lm`, `dbs`, `bpc`, `bql`,
`dogs`), plus `icc dark reckoning`, `icc in cannon` and `icc valkyre spear`. The raid-wide `icc opener
swing`, `icc opener redirect` and `icc resistance aura` lead with none and never fold. Each other ICC
node sits behind its encounter's `BossStateGate` or `UnitGate`; the gauntlet and the Blood Prince
trash own no trigger, so their pulls fold every one.

**The fight supplies the point, `geometry.py` the rest.** `--from` names a `const Position`, in any
initialiser form, declared anywhere under `src/Ai/Raid/`, and `--band` a `constexpr float` radius
there, parsed out of the source the way probe keys are, so nothing is kept by hand and every trace on
disk can be measured against any of them. A unique suffix is enough (`YOGG_SARON_MIDDLE`), an
ambiguous one lists the candidates, and a name two files declare with different values
(`LEASH_RADIUS`) is refused with both listed. `--band` also takes a bare number. `--from entry:<N>`
measures from a creature's spot at the pull, refusing an entry that more than one sampled creature
shares. That spot stays put while the creature walks, so a boss-relative formation needs its own
reader: Vezax's camp reads a median 8-16 yd off against one. This turns "`move.by` churned 2,396
times" into "957 `flee` moves, median 21.5 yd out to 24.9, 96% ending further from the band than
they started".

A `move` carries its destination only, so `--moves` joins the snapshot before it for the origin.
`--where` also takes `cast:<spell>`, because a creature's death reaches no record at all and what it
cast on the way out stands in (below). `--threat` with no entry reads every sampled hostile except
the raid's own pets. These three and `--clump` take `--during KEY=VALUE`; any other view exits 2
rather than read the whole pull under the scope.

`batch.py` streams one trace at a time, because the corpus runs to gigabytes; it is its own tool
because `postmortem.py` dispatches a first-match ladder over one positional. Extra positionals are
more roots, which is how a baseline survives retention.

**`--split-at REF` answers "did the change help".** It compares pulls before a commit against pulls
after it on every metric either side carries — deaths less the wipe command's, stalls, clumping,
coverage buckets, and churn per probe key and per action pair — reporting median and range per side,
and calling a metric *moved* only when the two ranges are disjoint **and** the gap is at least a tenth
of the larger side and visible at the printed precision — without those, 0.14 against 0.13 ranks as a
finding. A stream has to appear in most of each side's pulls, and a one-sided one be non-zero there.
The sides are rarely the same size, so with twelve pulls before and three after, anything occasional
shows up on the bigger side and nowhere else (`apply oil <-> clean quest log` beside the raid
streams). Two-sided is no safer: a phase only one of two pulls reached leaves that pull's value alone,
which no range overlaps. Splits on `hdr.bin` where there is one, else on the pull's own timestamp,
which assumes you rebuilt before pulling.

**`--probes` names keys the source declares that never reached a trace of their own boss.** One silent
pull usually means the thing did not happen; silence across every pull of a boss means the recorder is
dropping it. Only encounters the selection pulled count, so an unfought boss's keys are not reported.
In one trace, `--probes` reads each key by the shape of its emitter — a latch (`ObsValue`) as a state
timeline, an assignment (`ObsGuidMap`/`ObsGuidSet`) per holder, an event (`Note`) as a count — and
ranks keys by churn: changes per minute and A-B-A flips back to the value just left.
A key's boss comes from its prefix. `NOTE_PREFIXES` decides alone where it has a row: `nb`,
`jaraxxus`, `fc`, `tv`, `anub` name ToC's five encounters, and `toc` is raid-wide, matching all five;
the table is also what keeps `anub` off Naxx's `anub-rekhan`. Every ICC contract prefix has a row,
built from `ICC_KEY_PREFIXES` so none claims another raid's slug by initials: `sg` covers Sindragosa
and her gauntlet, `bpc` the council and its trash, and `icc` every ICC slug. Any other prefix
matches as the slug spelled out (`thorim`), its initials (`fl`) or its first word (`yogg`). The
declared set is parsed from `src/`, call sites wrapped across lines included; a hand-kept key list
goes wrong in both directions within a week, so never keep one beside the parse.

### Reading traps

- **Churn is only legible inside the phase you care about.** Mimiron's phase-1 flip-flop counts 57
  A-B-A within phase 1 and disappears into ~700 across the whole pull, so `--probes --during
  mimiron.phase=1` reproduces it and a whole-pull number hides it.
- **A change-only key keeps a stale value across windows**, a dead bot's duty for one. Write an
  explicit `none` when the window opens, or have the reader cut spans at death. Nor does a per-bot
  change-only latch that coverage reads NEVER prove its node dead: a generic node may have taken the
  action first (generic interrupts kicked every Fel Fireball before Jaraxxus's duty node could).
- **A note written before the session opens is lost.** The pre-roll ring keeps only snapshots, so a
  per-bot `NoteDerived` written before the pull never reaches a trace that opens at the engage. Write
  it again after the open, from any bot's tick: a bot mid-cast or drinking runs no triggers. Opening
  early with `MarkPull` fails on a long pre-pull wait: `IdleCloseSeconds` (30 s) closes it first.
- **`snap` rows carry no combat state.** A drink or eat `act` OK row stands in: both actions'
  `isPossible` require the bot out of combat.

## Is this trace evidence?

Every report opens with a validity banner, and getting one of these wrong by hand has cost whole
sessions. A pull recorded before a fix landed looks exactly like a pull where the fix did nothing.
Six things can disqualify a pull; the header carries four.

**A disqualifier that fires on the whole sample rejects the whole sample and says nothing**, so two of
them only decide once you say what is under test. The loop commits after every pull, so every trace
predates HEAD; every normal-mode pull of a hard-mode-capable boss has hard mode off. `--since REF`
names the change being tested and makes the build decide, so a REF that does not resolve disqualifies
too (`batch.py` exits 1); `--hardmode` says the pull was meant to be one. A human holding tank or heal
always decides — the strategy was not asked to do the job, whatever else was being tested.

- **`bin`** — the worldserver binary's mtime, epoch ms (UTC; convert before comparing against a local
  commit time). It stands in for a build hash because nothing better exists — this module has no
  `CMakeLists.txt` to stamp one from, and `AzerothCore rev.` in `Server.log` names the core. Read once
  at load from `/proc/self/exe`, so 0 off Linux. Compared against HEAD's commit time; `--since` takes a
  commit-ish or ISO time instead (local without an offset, as for `--split-at`).
- **`cfg.hardmode`** — the four `Ulduar*HardMode` toggles (Vezax, Iron Assembly, XT-002, Mimiron),
  keyed by the conf's boss names so the banner joins them against the boss the trace names. Flame
  Leviathan, Freya, Thorim and Yogg-Saron have no toggle, so their pulls never get `hardmode-off`. A
  kill with the toggle off is not a hard-mode kill.
- **humans** — read through `trace.humans`, not `roster[].h` alone: anyone who zoned in after the
  header appears only in a `unit` record. `roster[].r` carries a human's real role (the static
  `PlayerbotAI::IsTank`/`IsHeal`/`IsRanged` forms fall through to the talent tab when there is no bot
  AI); older traces carry the literal `"human"` there and can only say a human was present. Tank or
  heal disqualifies the pull; damage among 24 bots is recorded and not counted.
- **`diff`** — rendered 10/25 normal or heroic, not a bare id.
- **raid dead at the open** — over half the roster at 0 hp in the first in-pull snapshot: nobody
  pulled, so it always decides (`batch.py` flag `D`). The recorder now drops these.
- **no combat engine** — no `covdef` row tagged `c`: no bot ever entered combat, so nothing in the
  trace measures a fight. `Attack()` is what enters that engine, so an encounter zeroing the target
  pickers without assigning one of its own reads exactly like this. Always decides; pre-v12 traces
  have no `covdef` and cannot answer.

The build's timestamp is not proof the code ran. Where a change's effect must be *shown*, confirm it
from the records: a node the change added appears at least once, and an action a new multiplier
zeroes carries that multiplier's label in a `veto` row instead of its own relevance.

`cfg` also carries `cheats`, `mapthreads` and, from v14, `iccbuffs` (`Playerbots.EnableICCBuffs`, 0
or 1), and is a fixed short list on purpose: a full dump would grow with every option and bury the
few that decide whether a pull counts. Two banner lines never disqualify: `iccbuffs` on an ICC pull,
flagged when on in heroic, since every `EnableICCBuffs` site tests 10H/25H; and `cheated`, the cheat
kinds fired and their total uses, `none recorded` only on a v14 ICC pull, where every site records.

## Config

Knobs and their defaults are documented in `conf/playerbots.conf.dist` under RAID OBSERVABILITY; env
overrides are `AC_PLAYERBOTS_OBS_*`. Read effective values with
`docker exec ac-worldserver env | grep ^AC_` — never trust the `.conf`.

Three that decide file size: `SnapshotIntervalMs` (250 — two samples per oscillation cycle, the
minimum that distinguishes a bouncing bot from a stationary one), `DeathVerdictMs`, and
`MinDamageToLog`.

Only **raid** maps are tracked unless `Obs.Maps` names a map id — a tracked map is sampled for as
long as anyone stands on it, which no 5-man is worth.

## Lifecycle

A session opens on the first of: boss state → `IN_PROGRESS`, `RaidObs::MarkPull` from encounter code,
or a boss-flagged creature engaging a raid member. It closes on kill, wipe, reset, map destruction, or
`IdleCloseSeconds` out of combat, and deletes its file if nobody fought: no roster member ever in
combat, or combat only while over half the raid was dead.

**Boss state is observed, not requested.** `SetBossState` fires its hook *before* deciding: it drops
every change while a boss loads from the DB, and refuses `DONE` while a world-boss minion lives. The
hook only queues the boss id; the next map update reads the state that stuck. That delay also lets the
engage hook name the trace after the creature rather than the map — the core runs `SetBossState`
before `OnUnitEnterCombat`.

**A script without boss state closes on a credit or a fall.** ToC and Vault of Archavon keep none
(`GetEncounterCount()` 0), so two other signals close their traces:

- **`kill`**: a DBC encounter credit (`OnAfterUpdateEncounterState`). The hook fires inside
  `Unit::Kill` or `Spell::cast` (Tirion's 68184 for Faction Champions), often mid bot action, and a
  close drains every roster engine's coverage, so it only queues. Boss-state scripts ignore it: their
  `DONE` already closed. The next map update closes `kill` if `IsEncounterInProgress` has fallen,
  which a kill's `DONE` does in the credit's own call stack. If it is still true the credit only
  latches: heroic Beasts credits on Icehowl, summoned at 340 s whether or not the worms are dead, so a
  worm or Gormok can outlive him. A latched trace closes `kill` at the fall unless most of the raid is
  dead, and always at the idle close, since VoA's override stays true through Wintergrasp.
- **`reset`**: `IsEncounterInProgress()`, polled each map update, falling after the roster was in
  combat while it was true. ToC raises it before the pull (Beasts walk-in, champions released, twins
  3.25 s early, Anub'arak from the Lich King scene), so a fall without that combat ends no pull. The
  roster rule below files it `wipe` when most of the raid died.

**Never call `IsEncounterInProgress` with nobody alive.** ToC's override, finding no alive non-GM
player, resets its encounter to `NOT_STARTED`, and `InstanceCleanup` then charges the wipe no heroic
attempt. The poll runs that same player test first and reads a failure as false.

Two gaps. A Twins kill carries no credit, filing `reset` (or `wipe`), only if no player ever hit
Eydis: her engage zeroes her damage requirement (`LowerPlayerDamageReq` in `JustEngagedWith`), so
whichever twin takes the last blow, the kill credits through her loot recipient. VoA's override stays
true through Wintergrasp war time, the 10 min before it, and with no WG battlefield, so a wipe that
leaves anyone alive closes on idle (a full wipe still reads false), and a war ending mid-pull closes
as `reset`.

**A trace can rename itself.** Opened by `bossstate` or `MarkPull` with nothing engaged yet, it has
only the map name to use. Whatever names it first renames the file and emits a second `pull`, `src`
`rename`, carrying `was`. `hdr.boss` stays stale, line one being long written, so **prefer the rename
record**. Only the map-name fallback is ever upgraded, except by a boss-state table.

Three callers reach it, because the boss cannot always say: `OnCreatureEngage`, the first boss to
swing at a player; `MarkPull` on an already-open session; and `NamePull(map, name)`, which takes the
name outright. Yogg-Saron is why the last two exist — phase one is fought against Sara, the Voice and
the Guardians, none flagged a boss, so nothing engages and the only boss the file named was General
Vezax, swept in from the next room. `EncounterGatedTrigger` calls `NamePull` when a trigger fires
while its own encounter is `IN_PROGRESS` (the gate's own test leaves every trigger open between pulls,
far too loose to name by), and ToC's gate names its pull from the encounter slug (`ToCEncounterSlug`).

**A boss-state table names a trace outright.** `RaidObs::NameBossStates(mapId, {{id, slug}, …})`,
called while scripts load (ICC's `AddSC_IcecrownBotScripts`), gives boss ids a slug. A trace opened on
that map while one is `IN_PROGRESS` takes it, whatever opened it: an engage open reads the boss id
still queued, since the core sets `IN_PROGRESS` inside `JustEngagedWith`, before the engage hook. A
listed boss going `IN_PROGRESS` renames an open trace of another name, or, if `MarkPull` opened it,
closes it `idle` and opens its own, so a trash trace in its idle window never absorbs the boss pull.

While a raid sits on a tracked map with no session, snapshots go to a `PreRollSeconds` ring that is
flushed into the file when a pull starts — so the trace opens *before* the engage, and pre-pull
records carry a **negative** `t`. That is what makes a bad squad latch visible. **`t=0` is the pull
and first damage is not**: `bossstate` fires inside the boss's own init, before anything is hostile, so
"before the first damage record" measures 20-30 s late. A Yogg-Saron plan called four of twelve
Guardians pre-pull that way; against `t=0` none of them was.

`end.out` follows the roster, not the instance script: more than half the raid dead files a `reset` or
`idle` as `wipe`. Hodir's script reports `NOT_STARTED` on release, which filed a 23-of-24 wipe as
`reset`. **Latched during combat**, never at the close — `IdleCloseSeconds` (30 s) has by then let
everyone release and run back alive, which filed a 31-death Flame Leviathan attempt as `idle`.

## Schema (`v: 14`)

`t` is milliseconds from the `hdr`. A guid is a type tag in the high 32 bits over
`ObjectGuid::GetCounter()` in the low 32 — the counter alone is a separate numbering space per type, so
a creature and a player both start at 1 and would collide. Players are tag `0`, so they read as the
bare counter; creatures are `1`, pets `2`, vehicles `3`, gameobjects `4`, dynamic objects `5`, anything
else `7`. `0` still means no unit.

| `e` | Fields |
|---|---|
| `hdr` | `v`, `ts` epoch ms, `map`, `inst`, `diff`, `boss`, `bin`, `cfg` (`iccbuffs` from v14), `roster[]` of `{g,n,c,r,h}` — `r` role, `h` human |
| `pull` / `end` | `boss`,`src`: bossstate, mark, engage, rename (then `was`) / `out`: kill, wipe, reset, idle, mapgone, shutdown |
| `unit` | `g`,`en` entry,`n`,`lvl`,`mhp`,`b` is-boss,`own` owner guid when it has one, plus `c`,`r`,`h` for a player — once per guid |
| `spell` | `sp`,`n` — once per spell id |
| `snap` | `u[]` rows `[guid,x,y,z,o,hp%,mana%,target,moving,moveGen,castingSpell,dealt,powerType,power%]`, `dealt` cumulative damage to non-raid targets, 0 off the roster and on pet rows; `powerType` the unit's own, so a vehicle's bar reads there and not in `mana%`; roster players, their pets and guardians, ridden vehicles and the swept creatures; `hz[]` swept dynamic objects `[spellId,x,y,z,radius,foe]` |
| `dmg` | `s`,`d`,`sp`,`a`,`ok` overkill,`sc` school,`ab`,`rs`,`hp` before the hit — roster players and the non-hostile vehicles they ride |
| `heal` | `s`,`d`,`sp`,`a` gross,`oh` overheal,`hp` after; Beacon of Light's copy is its own row from the paladin |
| `abs` | `d`,`s` shield caster,`sp`,`a` |
| `aura` | `d`,`s` caster,`sp`,`r` 1=removed,`st` stacks,`dur` ms left,`p` 1=positive |
| `cast` | `s`,`sp`,`tgt`,`ct` cast time,`tr` 1 when triggered, else absent — cast **start**, the reaction window, for a spell with a cast time ([not an instant](#what-a-trace-cannot-tell-you)); roster, its pets and watched creatures only |
| `act` | `g`,`a`,`rel`,`vd`: OK, FAILED, IMPOSSIBLE, USELESS, PREREQ, UNKNOWN |
| `veto` | `g`,`m` multiplier,`a` action it zeroed. Only the first multiplier to zero an action writes, again only when the verdict changes, so counts are rows, not ticks, and miss a guard behind another zero |
| `covdef` | `d[]` rows `[id,node,strategy,engine,trigger?]`; `engine` c/n/d, `trigger` only when it differs from the node's name; chunked, so a `cov` may cite a chunk several records back |
| `cov` | `g`,`r[]` rows `[id,checks,fires,pushes,won,shared,throttled,minimal,dead]`, trailing zeros trimmed - pad short rows back out ([node coverage](#node-coverage)) |
| `move` | `g`,`k` generator,`x`,`y`,`z`,`tgt`,`ok`,`r` reason,`by` owning action,`pr` priority; on `wait` also `hpr`,`hms` — the walk that beat it |
| `note` | `g`,`k` kind,`txt` — assignments, latches, phases, derived state |
| `cheat` | `g` bot whose code cheated,`tgt` unit it acted on (absent: the bot itself),`k` `<prefix>.<kind>`,`n` uses since this bot's last row for `k` |
| `haz` | `sp`,`shape`,`x`,`y`,`z`,`ttl`, plus shape fields — hazards with no world object; timeline only, never tested against a death |
| `death` | `g`,`killer`,`x`,`y`,`z`,`dist{}`,`auras[]`,`rewind[]`,`blow[]`,`hplast[]`,`acts[]`,`lastmove{}`,`cause` |
| `truncated` | file hit `MaxFileMB`; the `end` record is still written past it |

`death.auras` rows are `[sp,stacks,dur,caster,appliedT,removedT,positive]`, `removedT` −1 while held
and `appliedT` −1 when the aura was applied before the session opened;
`death.rewind` rows are `[t,source,sp,amount]`; `death.blow` is `[source,amount]` and `death.hplast`
`[hp%,t]`; `death.acts` rows are `[firstT,lastT,action,rel,verdict,repeats]`, a veto's verdict reading
`VETO:<multiplier>`.

**A `cheat` row is paced, so sum `n`.** A bot writes one on its first use of a key in the session,
then at most one per 10 s, and the close flushes the rest. `tgt` names only the use that wrote the
row: who was acted on, never how often. `RaidObs::NoteCheat(bot, key, target)` is raid-agnostic and
gates on `Active()`; ICC calls it through `IccRaidCheat`, which also tests `BotCheatMask::raid`.

**`dmg.hp` is the health a hit landed *on*; `heal.hp` is what the heal left behind.** The core logs
damage before applying it and a heal after. So `dmg.hp` never reads 0, and `ok` is the row's only sign
a hit was lethal; read as an outcome it shifts a death's trajectory down a row and hides the blow.

**`cause` explains a death with no `blow`.** `Unit::Kill` called directly never reaches `DealDamage`,
so the hook feeding `blow` never fires and `killer` is the victim itself: `reset` is the master's
`wipe` command (`WipeAction`), `self` any other script's kill - Yogg's Insane running out, the Brain
killing whoever hits it before its tentacles die. Falling, lava and `.die` on yourself do pass
`DealDamage`, reading as a `blow` from the victim with no `dmg` row, since none reaches the combat log;
`--verify` allows that. Absent whenever there is a `blow`. `postmortem.py` numbers `--death N` over the
others and counts resets separately.

**An aura that kills its owner when it comes off writes one death twice.** Yogg's Insane calls
`Unit::Kill` from `OnRemove` and dying strips it, so the kill re-enters and `OnUnitDeath` fires nested
first: 14 pairs in 3 of 17 Yogg traces, all Insane, and a summary of 12 deaths where 6 were real. The
recorder drops a second death in one world tick (`GameTime::GetGameTimeMS`) and takes the killer from
that tick's lethal blow, since the nested call names the victim. Readers fold older traces within
50 ms (`trace.death_records`); `--verify` still flags the file.

**Name every id in the file.** `unit` and `spell` are written once each, so later records carry a bare
number and the trace still reads standalone: `spell 63511` needs a DBC open beside it, `Frozen Blows
63511` does not. Call `EnsureUnit`/`EnsureSpell` from *every* field and every *emitter* that writes an
id — `aura.s`, `cast.tgt` and `NoteHazard` each shipped unswept once. Where no session exists yet
(`UnitRow` is a free function, the pre-roll has none) the ids travel with the sample — `castSpells`
and `unitRecords` on the pre-roll ring, emitted at the drain through `EnsureSpell` /
`EnsureUnitRecord` — and a unit record is built at sample time, because an expiring guardian (Army of
the Dead ghouls, Mirror Images) is gone by the drain. A guid inside `note.txt` is the exception: free
text the recorder cannot inspect, so `postmortem.py` joins it on read. **Open:** `NoteAuraApplied`
sets the caster without `EnsureUnit`, so a same-tick aura from a caster that left the map can reach
`death.auras` unnamed.

**Emit per engine pass, not per verdict.** A pass walks several action nodes and reports a verdict for
each, so no single verdict is news on its own. `act` and `veto` buffer until `BeginTick` closes the
pass, and a pass whose ordered set matches the one before is not written at all — it becomes a
`repeats` count, replayed into `death.acts` with the interleaving intact. A bot losing the same tick
forty times *is* the oscillation signal; `repeats` is where it lives.

**Then latch each verdict per action.** The pass compare is far too coarse alone: relevance drifts
reorder the priority queue every tick, so barely 1 in 200 passes matches its predecessor. A verdict
surviving it is written only when that action's own verdict changed. Both grains re-emit after
`OBS_ACTION_REPEAT_MS` (10 s), so a bot stuck in one state leaves a trail, not a gap.

A verdict is written when its pass ends, so line order can trail `t` by a tick; sort by `t`
(`postmortem.py` does, on load).

**`death.auras` comes from a tracked set, never from the unit.** `Unit::Kill` calls
`RemoveAllAurasOnDeath` well before `OnUnitDeath` fires, so at hook time only passives and
death-persistent auras are still applied — v3 death records listed 57 talents and no boss debuff.
`NoteAura` maintains the set itself, *ahead of* the `LogAuras` gate so switching the aura stream off
cannot also empty every death record, and stamps `removedT` rather than erasing, because the strip runs
through that same hook. Anything dropped within 2 s of the death is still reported, flagged. The strip
stamps the death's own timestamp, so a `removedT` that close means held to death, not worn off.

**Two hooks, because one cannot see a same-tick apply.** `_Apply` only flags the client update and
`Unit::_UpdateSpells` flushes it a tick later, while `_Remove` fires it synchronously — so Hodir's
Fury, which lands and kills in one tick, reached `NoteAura` as a removal with no apply behind it.
`NoteAuraApplied` runs inside `Unit::_ApplyAura` on `UNITHOOK_ON_AURA_APPLY`, stamping `appliedMs` and
emitting nothing: that hook double-fires on a stack refresh, and is the only one to see an aura that
found no free visible slot. Keep its guard identical to `NoteAura`'s, or the feeds disagree on what a
refresh means.

**An unknown timestamp gets a sentinel, never arithmetic.** `appliedMs` stays 0 for anything applied
before the session opened, and stamping 0 yields `−startMs`, one constant across every bot.
`postmortem.py` renders the sentinel `?`: a fabricated `held 1740.7s` in a 155 s fight is worse than no
answer.

**Helps or hurts comes from the spell, not the caster.** `p` is `SpellInfo::IsPositive`. The caster is
wrong both ways — totems and pets buff from a creature guid, and Biting Cold is applied to the player
*by the player* through the zone aura's trigger — so a caster-tag heuristic listed Windfury Totem as a
debuff and hid the six stacks that killed the bot.

**`death.rewind` is trimmed on read as well as on push, and ordered by time.** The ring sheds its front
only when something new arrives, so a bot untouched for a minute would report that minute-old damage as
what killed it — and what a rewind answers is what landed last, not hardest.

Every damage hook is a `Send*Log` hook, so a fall or a script kill reaches none of them and leaves the
rewind empty. `NoteKillingBlow` fills `death.blow` from `Unit::DealDamage`, which all of them pass
through, taking only the blow that lands with the bot's health or more — emitting the rest from there
would double every hit the log hooks already see. `death.hplast` carries the last sampled health and
when, so the record states the drop even when nothing caught the blow. `blow` names no spell and cannot:
`OnDamage` carries no `SpellInfo`, and the hooks that do are modifiers rather than the funnel, missing
the environmental and script damage `blow` exists for. The debuff list answers that instead.

**Damage out is a running total, not a record each.** `dmg` is one row per hit because a death is
rewound blow by blow. Outgoing damage is only ever read as a rate, so a row per swing and tick would be
tens of thousands of lines for what `snap.u`'s `dealt` already carries four times a second. A pet,
totem or guardian credits its owner, and its own row carries 0 so differencing a window cannot count
the same damage twice.

**Pets are sampled, totems are not.** Nothing else in the file can place one: the sweep keeps only
units hostile to the anchor and the watched set is seeded from attackers, so before v10 a trace could
say a pet cast something but never where it went — and half of all pet casts carry no target at all,
which is exactly the window a pet spends running somewhere. Guardians count, because a death knight's
ghoul and a shaman's wolves are guardians rather than pets. Totems are skipped: a shaman drops four,
none of them move, and none of them can pull anything. `Playerbots.Obs.LogPets` turns it off.

Same instance is not the same pull: gating `cast` on the session alone picked up 253 casts from a mob
two rooms away and none at all from the raid.

`snap.hz` radius falls back to the spell's own when the dynamic object reports zero, and rows still
come out at zero: Hodir's icicle dynobjects are `SPELL_ICICLE_VISUAL_*`, force-cast on the target with
no radius in the DBC to fall back to. A zero-radius row is a marker — where something landed, not an
area to test a position against — so `postmortem.py` reads it by proximity and keeps containment for
the rest. `foe` separates raid AoE from boss AoE: two thirds of a Hodir trace's rows were the
raid's own Death and Decay. What is lethal there is a creature, so hazard units carrying no dynamic
object that never enter combat are swept into `snap.u` instead, capped at the **nearest**
`OBS_MAX_WATCHED` (40) so a trash-heavy pull cannot blow the row count up. Nearest, because the cap
decides who makes the row set and grid order is not a ranking: at Hodir it binds in 94-96% of
snapshots, and in grid order every slot went to Thorim arena trash parked 74+ yd out, so not one ice
block, Toasty Fire or icicle was ever sampled — the units the sweep exists for.

The sweep sees only its anchor's phase, so the anchor is the first roster player in the normal phase:
one bot inside an Algalon hole would otherwise drop every hole, star and constellation from the rows.

Nearest is still a cap. A 25-man Algalon room holds 11 constellations, 4 stars, holes, void Dark Matter
and six meteor markers, and markers far from the anchor never made a row. A short-lived hazard the
encounter already scans for is written as a `haz` circle by that scan, once per unit.

`haz` is the other channel and the two never meet — `snap.hz` is what a sweep found, `haz` what nothing
can sweep for. Only `snap.hz` feeds the death block's containment test, so a `haz` mechanic reaches the
timeline and never a `STOOD IN` line. A `lane` carries no geometry to test anyway: an origin, no heading
or width, so only `side` separates Thorim's two.

**Derive a side from a guid, never from a sampled unit.** `foe` comes from the dynamic object's caster
guid, which outlives the caster. Asking a live roster member `IsHostileTo` flipped the same puddle
mid-fight, twice over: the member sampled was whichever the map iterated first, and a caster that had
gone fell into the hostile branch.

`move` covers every generator a bot drives: `k` is `point` (`MoveTo`), `jump`, `follow` or `chase`.
Follow and chase steer at a unit rather than a position, so they carry it in `tgt` and record where it
stood at the time; they emit only when the bot starts tracking somebody new, since the coordinates
change every tick by design.

`ok: 0` is not automatically a refusal — read `r`. Only `blocked` (`IsMovingAllowed` said no) and
`nopath` are; `dup` and `wait` are the ordinary state while a bot walks to a destination its action
re-offers every tick, and `there` means it already stands on it. A non-issued destination is a
*candidate*, not a command — `MoveNear` sweeps eight angles a tick — so these are throttled to one a
second and never latched into `death.lastmove`, whose `arrived` would otherwise be measured against a
position no MotionMaster saw. `follow` and `chase` are left out of that latch for the same reason:
neither has a destination to arrive at.

**A `wait` is a contest, so record both sides.** `IsWaitingForLastMove` yields only to a *strictly*
higher priority ([pitfalls.md](../engine/pitfalls.md#movement-that-silently-no-ops)). `pr` is what
this command was issued at, `hpr`/`hms` what beat it and for how long. Without them a trace reports two
thirds of moves refused and cannot say what won: Hodir's shelter move lands 12.7% of the time purely
because it sits at `combat` alongside `reach melee`. `follow` and `chase` never reach the gate and carry
`pr` empty — worth seeing: they steer without outranking anything. Names, not ordinals:
`RaidObs::MovePriority` mirrors `MovementPriority` so `Bot/Obs` stays off `Ai`, and
`MovementActions.cpp` static_asserts the two in step.

Bump `SCHEMA_VERSION` in `RaidObs.h` and `SUPPORTED_SCHEMA` in `raidobs/trace.py` on any field *or value*
change; an additive one keeps the old version in `READABLE_SCHEMAS` so traces already on disk still
read. A new sentinel in an existing column is not additive — a reader that does not know it computes a
wrong number.

**Audited and correct — do not re-audit** (cross-checked against `creature_template`, `Spell.dbc` and
the snapshots): an aura `dur` longer than the DBC base is a talent or glyph extension, the real
modified duration; implied-speed outliers are real events (a Blink cast ~100 ms before a jump of
Blink's distance, Thorim's bunnies repositioned exactly 128 yd, Sif's blinks, the raid zoning in
during pre-roll); `moving` disagreeing with displacement sits on a 250 ms sampling boundary, or is a
bot asking to move and not moving, which `--stalls` exists to surface; deaths without
`acts`/`lastmove` are human players, who have no engine; `--bot`'s heal arrow (`-> hp`) is right as
written, since `heal.hp` is the health after.

## Node coverage

`cov` is the runtime twin of pblint ([pitfalls.md](../engine/pitfalls.md#names-fail-silently-at-runtime)):
what each strategy node did, per bot. `--coverage` buckets each node, worst first:

| Bucket | Test | Means |
|---|---|---|
| `DEAD` | `dead > 0` | the name resolved to no creator this bot's own context stack carries |
| `NEVER` | checked, nothing pushed or won | asked every pass, never true |
| `LOST` | `pushes > 0`, `won == 0` | fired and was queued, never ran — otherwise indistinguishable from a node never wired in |
| `THIN` | won, but fired on under 20% of the bots carrying it | role-shaped |
| `THROT` / `MIN` | only `throttled` / only `minimal` | its `checkInterval`, or minimal mode under relevance 100, never let it be asked |
| `ok` | `won > 0` | ran |

The columns: `checks` (`Trigger::Check()` ran), `fires` (it returned an event), `pushes` (that turned
into a queue entry), `shared` (pushed on a `Trigger*` a sibling node fired this pass), `throttled`,
`minimal`, `dead`, and `won`. Decisions behind the shape:

- **`won` is keyed by trigger name**, from `event.GetSource()` at the OK verdict, so **`won <= pushes`
  is not an invariant**: an action can win in a pass where nothing was pushed, having survived in the
  queue. The ratio is the signal, not the difference. `checks + throttled + minimal + shared + dead` is
  the pass count and is deliberately not recorded: a redundant column is a `--verify` failure the
  moment the two disagree.
- **No `act.src`.** `Queue::Push` de-dupes by action name and `updateExistingBasket` keeps the
  existing basket's `Event`, so in the case `src` would exist for — two triggers pushing one action in
  a pass — it names the first pusher and is silently wrong about the second. It would also have to
  join the `act` latch key, multiplying the stream.
- **Counters live in a vector parallel to `Engine::triggers`**, never keyed by `TriggerNode*`:
  `Engine::Reset` deletes every node and `PlayerbotAI::ChangeEngine` re-inits on every engine change,
  so recycled addresses would merge counters across differently-named nodes. Names are resolved once,
  at drain. Strategy attribution is the index span each strategy's `InitTriggers` appended at
  `Engine::Init`, so all of a raid's nodes read as that raid's one strategy; per-encounter separation
  is the trigger-name prefix (above).
- **Accumulated in the engine, flushed once**: streaming would be tens of thousands of records a
  second for a 25-man. One drain runs at the top of `Engine::Init`, before `Reset`, catching the
  rebuild when a bot enters combat or dies (so a bot that died mid-pull has `d` engine rows); the other
  in `CloseSession` through `PlayerbotAI::ObsDrainCoverage`, about 230 KB a pull. `shutdown` and
  `mapgone` write no coverage: they close with the thread pool gone.
- **Rows are per bot, not raid-summed.** A reader can sum per-bot rows and cannot un-sum an
  aggregate, and the failures this exists to catch are role-shaped. The view sums with an `n/25 bots`
  column; `--by-bot` splits it.

## Adding a probe

Declare a probe in `RaidObs.h`, define it in the `RaidObs*.cpp` that owns its concern.
`RaidObsSession.h` is shared state, never public API.

Store assignment state in the traced containers — `RaidObs::ObsGuidMap<T>` (per-bot assignment),
`ObsGuidSet` (membership latch), `ObsValue<T>` (per-instance scalar), each constructed with its key
(`{"thorim.slot"}`) — and it records itself: instrumentation follows the data, not the call sites, so a
new boss is covered without further work.

They emit only on real change since the trace opened: they outlive the pull, so their first write in
a trace restates the whole container. Non-const iteration and iterator-`erase` exist for prune loops
and deliberately emit nothing — dropping a departed member is not an assignment. `erase(guid)` does
emit, on both containers: clearing one named bot is a wipe reset or a boss that is gone, which is real
news. Keep scan timestamps and cached guid lookups in bare types; tracing bookkeeping is noise. Reset
an `ObsValue` by assigning it a value: copying over a struct that holds one (`state = Struct{}`)
changes it without emitting.

The key guid decides which session the record lands in; where it is not a player, the bot whose tick
set it does (`NoteAssignment` falls back as `NoteDerived` does). Before that fallback, a container
keyed on a vehicle emitted nothing at all — `fl.frozen` was absent from 129 traces while its six
siblings carried 1,556 rows.

State **derived** fresh on every call rather than stored — Hodir derives every position that way on
purpose — has nothing for a container to watch. Probe inside the helper that derives it, with
`RaidObs::NoteDerived(bot, key, RaidObs::DescribeDerived(value))`, which emits only on a change — never
at the call sites: trigger and action both route through the helper, so one probe covers both and two
cannot disagree about what was decided. Being change-latched per bot, it hides the same decision
made again in a later window: write an explicit `none` at the window's end. A raid-icon pick is
noted by the bot that moves the icon only, or 25 rows describe one placement; put the unit the icon
came off into the value, or the same move repeated after another bot moved the icon stays hidden. A
probe that reads world state skips a bot phased away from it: grid searches filter by the
searcher's phase, so a bot in a phase aura (Valithria's Dream State) reads "nothing there" and flips
a shared latch mid-window.

**Probe the rule, not the coordinate.** A derived destination is already a `move` record naming the
action that issued it, and one derived from the crowd or the bot's own position moves every tick, so
latching it emits every tick — `hodir.shuttle` records which branch produced the leg (`tank`, `solo`,
`crowd`, `none`). Where the value is the news, `DescribeDerived` rounds it to the yard;
`DescribeAssignment`'s full precision is for stored assignments, which do not drift.

**Do not probe what another stream already says.** An action that succeeds is an `OK` in the act stream
and one that fails a `FAILED` — a note per attempt re-adds what that stream latches. Hodir's tank swap
retries at tick rate whenever the 8 s taunt cooldown is not up, so it is unprobed; so is
`IsHodirTrappedAllyBreaker`, which is asked about every block in range and comes back true for several
in one pass, while the one the bot goes for is already in `hodir.dpstarget`.

Prefix every note key with the encounter that owns it — that is what `--notes KEY` filters on, and Iron
Assembly legitimately spans three bosses under one slug. A prefix the slug does not derive needs a row
in `NOTE_PREFIXES`, or `--probes` never ties the key to its boss.

`probes.py`'s `declared_keys()` finds only literal keys at `ObsGuidMap`, `ObsGuidSet`, `ObsValue` and
`NoteDerived` sites, so a raid-wide helper taking its key from a caller's config
(`RaidDefensiveScope`, `RaidOpenerConfig`, `RaidResistAuraConfig`) hides that key from `--probes`. Such
a helper's trigger checks first that the current encounter takes part; otherwise it notes a constant
(`free`, `none`) every pass in raids that never use it.

A pull no instance script reports — a gauntlet, trash, a mid-phase re-engage — needs `MarkPull(map,
source, name)`, where `name`, else `source`, names the file. Thorim's corridor and ICC's dogs and
giants use it.

For a hazard with no world object — a cone, a rolling wave, a rotating sweep — nothing can sweep for
it, so the helper that derives the geometry must say so (Thorim's Runic Smash lanes do), with
`RaidObs::NoteHazardCircle(map, spellId, pos, radius, ttlMs)` or `NoteHazard(map, spellId, origin,
shape, fields, ttlMs)` for a `sweep` or `wedge`. It lands on the timeline only, so give the shape
enough fields to reconstruct it by hand: Yogg-Saron's Crush wedge carries its facing for exactly that
reason, while its body's permanent knockback ring — an aura on the boss, not an object — needs only a
circle. `NoteHazard` has no change-latch, so pace these per instance; 25 bots reaching one once a tick
writes 25 rows a tick.

Anything else: `RaidObs::Note(bot, kind, text)`, which emits every call rather than on change. Reserve
it for something that happens once and is in no other stream; nothing uses it today.

Everything above this section is raid-agnostic and already covers every instance map — only the `note`
stream needs per-raid wiring, because only the encounter code knows what an assignment is. Converted:
Ulduar, Trial of the Crusader, Black Temple, Obsidian Sanctum, Onyxia. Still bare: Hyjal, Tempest
Keep and SSC (upstream's rewrites replaced the converted code), ICC's `IccInstanceState`
(`std::map`, needs an ordered container variant), SWP's instance-keyed nested maps (the inner map
must be default-constructible, which a kind-carrying container is not), and Naxx's function-local
statics. Timestamps, thresholds and caches are left bare on purpose.

## What a trace cannot tell you

- **A `cast` row is the cast start only for a spell with a cast time.** A Speed 0 instant's `dmg`
  rows land 0-6 ms *before* it, since `Spell::prepare` runs `cast(true)` before `OnSpellPrepare`
  fires; one with a Speed (`Spell::_cast`) lands after it by the flight. Match a Speed 0 instant's
  hits to the nearest cast on either side.
- **Snapshots ~240 ms apart miss a CC broken between two samples** (15 of 67 Fears on Faction
  Champions never showed as moveGen 9), so a landing count from snapshots or a refresh-written probe
  is a lower bound.
- **A creature's death reaches no record at all.** The `death` stream is roster-only, and so is `dmg`,
  so nothing says an add died or who killed it — a count over `death` for a creature is not low, it is
  structurally zero. What a trace does carry is the last health the snapshot sampled before the unit
  stopped appearing, and whatever it cast on the way out: Yogg's Guardians are counted through their
  Shadow Nova for exactly this reason.
- **Pet damage is not recorded.** `dmg` rows are written only for raid members and their vehicles, so
  nothing involving a pet as a damage source or a chain-lightning link can be confirmed from a trace.
  Thorim's phase 2 carries a median of 6 and a p90 of 25 live pets and guardians, which is the
  likeliest reason observed chains reach 7-8 hops where an idealised formation caps at 3 — and it
  stays a hypothesis until `Bot/Obs` logs pet damage.
- **A traced container restates itself only on its first write in a trace**, so one the pull never
  writes is absent even while it holds a value. Older builds restate nothing, and there absence can
  still be state leaking in from a previous pull rather than nothing happening.
- **A friendly boss is never sampled, so its health is invisible.** The snapshot sweep keeps only
  units hostile to the anchor, and Yogg-Saron's Sara is `FACTION_FRIENDLY` for all of phase 1 — the
  phase whose entire progress is her health bar. No trace can say how close a phase-1 attempt came;
  Guardian deaths inside 15 yd of her are the only proxy. `yogg.phase` supplies the boundaries, which
  is why an encounter with an unsampled boss needs one.
- **An aura on a creature is never recorded.** `NoteAura` goes through `ObsSession::Tracks`, which
  wants a player, so a boss debuff has no `aura` row — Hodir's doc once read that silence as Singed
  never landing, when 115-173 procs a pull had. A proc debuff still shows as a triggered `cast`
  (`tr:1`) with `tgt` the creature: count those and model the stacks, as `bosses/hodir.py --singed`
  does. The mod-chronicle log does carry creature auras. Its `SPELL_AURA_APPLIED` fires on every aura
  client update, so a refresh with no cast behind it (Everlasting Affliction rolling Corruption) reads
  as a repeat `APPLIED` with no `REMOVED` between.
- **A `unit` row's `mhp` is the pool at first sight**, written once per guid, so `trace.maxhp` keeps
  a bot's pre-death pool: after a resurrection its percentages are wrong, and the killing blow is the
  only witness to its real bar.
- **A friendly creature is not swept either**, so Yogg-Saron's one-use portals (34072) leave no row
  anywhere but `yogg.wave`. Where a mechanic's unit is not hostile, the probe is the only record.
- **A `cheat` row covers only a wrapped site.** `cfg.cheats` is `Playerbots.BotCheats` verbatim
  (`EnvFieldsJson`): what was allowed, not what ran. From v14 a `cheat` row names each use of a site
  that calls `NoteCheat`, which is every ICC site and nothing elsewhere yet. Off ICC an empty stream
  is no clean pull, and an encounter that stopped cheating reads identically before and after; only
  the body can show it.

## Server stalls

**Snapshot gaps time the world.** `snap` is written from `OnMapUpdate` every `SnapshotIntervalMs`, so
gaps run ~220-320 ms; a stall is a second or more with no record of any kind. `t` counts
`getMSTime()` from a wall-clock `hdr.ts`, so it keeps wall time only while the steady clock does; where
that is in doubt, match RaidObs against the mod-chronicle combat log or `docker logs --timestamps` on a
shared event, never the clock.

Bot logins run their DB callbacks under a 10 ms budget per world tick, at least one a tick
(`PlayerbotHolder::UpdatePendingLogins`), so `RandomPlayerbotMgr`'s login batches spread over a few
seconds instead of stalling one tick; an alt whose master left while it was queued is dropped, not
logged in as a random bot. Each login's synchronous module queries stay cheap only through local
patches in mod-individual-progression (account kind cached per account) and mod-weapon-visual (no
per-item query while loading).

**Attributing a stall**, read-only on a live server:
- sample `/proc/<pid>/task/<tid>/{stat,wchan,io}` every ~20-30 ms. The main thread (tid = pid) is
  the world thread, `wchan` `wait_woken` is a MySQL socket read, and `io` `syscw` counts round trips;
- diff `performance_schema.events_statements_summary_by_digest` and `prepared_statements_instances`
  counts across one stall and a quiet window of the same length to name the queries;
  `OWNER_THREAD_ID` tells a sync connection from an async worker.

## Timing a bot: `.playerbots pmon`

A `SEC_GAMEMASTER` command (`PlayerbotCommandScript`): `pmon toggle` to arm, `pmon reset` at the pull,
`pmon` afterwards. `Playerbots.PerfMonEnabled` is **0** by default, so it is off unless asked for.

Two traps make its absolute numbers meaningless. **It does not time `isUseful` or `isPossible`** —
both show up only inside the single `PlayerbotAI::UpdateAIInternal I` bucket. Multipliers are timed
per name (`Mult`, with block counts). And its global mutex plus a per-call string build inflate every
figure once a thousand random bots are logged in. **Read ratios between buckets, never times.**

## Constraints

**Map thread only.** Per-session state is unlocked, safe solely because every probe runs on the map
thread owning that instance — which the damage, aura, death and `OnMapUpdate` hooks all do. A probe
driven from the world thread (`WorldScript::OnUpdate`) breaks it.

Two callers are not, and each earns it: `Status()` reads only `path` — fixed before the session is
published — and the two atomics that exist for it; `Shutdown()` runs from `Main.cpp` after
`WorldUpdateLoop` returns and the map thread pool is gone. Any new field either reads must be atomic.

**Gate hot paths on `RaidObs::Active()`** — a single bool, false whenever no trace is open. Put the
gate *inside* the probe and pass the object, never the name: `Action::getName()` returns
`std::string` **by value**, so a name passed as an argument is built before any gate can skip it, on
every bot every tick. A thousand idle open-world bots pay that bill.

**Filter on the map before touching the registry.** The damage, heal and aura hooks fire for the whole
server, not just instances. `SessionFor` tests `IsDungeon()` — a DBC flag read, no lock — and
`FindSession` answers from a per-map-thread memo a generation counter clears on every open and close.
Neither open-world combat nor an idle instance touches the registry mutex.

**Publish the bot for the whole engine pass** with `RaidObs::BotContext`, as `Engine::DoNextAction`
and `ExecuteAction` do. An `ObsValue` latch carries no guid and resolves its trace through it, so
covering only action execution silently drops every latch a trigger or value flips.

**Open the pass with `RaidObs::BeginTick`** at the top of `DoNextAction`, beside `BotContext`. It closes
the previous pass; without it the verdict buffer flushes only on a death or at the trace's close.

**Publish the action around every `ListenAndExecute`** with `RaidObs::ActionScope`, which is what
`move.by` reads. It restores the previous action instead of clearing, because a nested
`DoSpecificAction` runs a second action inside the first — clearing would blame the rest of the outer
action's movement on nobody. Construct it only under `Active()`: it copies the name.

**This module no longer builds against upstream AzerothCore.** The `Send*Log` and
`OnAuraApplicationClientUpdate` hooks are local core additions (commit `208764946`), absent from
`mod-playerbots/azerothcore-wotlk`, so module CI cannot compile it. Deliberate.

Probes in `Engine.cpp` (`BeginTick`, the verdict sites and the coverage counters in `ProcessTriggers`)
and in `MovementActions.cpp` (the `MoveTo` wrapper plus `JumpTo`, `Follow` and `ChaseTo`) will
conflict on upstream merges. They sit beside the existing `LogAction` calls and the `MotionMaster`
calls respectively, so re-applying is mechanical. `MoveToImpl` returning `RaidObs::MoveOutcome` rather
than `bool` is the one part that is not.
