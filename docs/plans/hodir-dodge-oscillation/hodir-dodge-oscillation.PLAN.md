# Hodir: stop the dodge oscillation, make the buff zones reachable

## Context

Pull `env/dist/logs/botobs/603_1_hodir_1788289957.ndjson` (schema v9, 24 members, 2026-09-01) was
wiped deliberately at **5:56.9** with Hodir at **14.93%**, because the kill was running far past the
3-minute target and the bots were visibly oscillating.

Both complaints are now measured, and they are separate problems:

- The oscillation is one bug in `HodirIcicleDodgeAction`. It costs roughly 20-25% raid throughput.
- The 3-minute target needs ~214k dps on the boss. The raid did **92k**. The missing factor is the
  encounter's own damage buffs, which the raid barely touches.

Read a trace with `python tools/botobs/postmortem.py <file>`. Schema: `docs/systems/observability.md`.
Hodir code: `src/Ai/Raid/Uld/{Action,Trigger,Multiplier,Util}/*_Hodir.*`.

**Scope decided with the user:** fix the oscillation and the buff-uptime bugs. Leave melee excluded
from Starlight, leave `ULDUAR_HODIR_HELPER_BLOCK_BREAKERS` at 8, leave tank survival alone. No
bot-behaviour change beyond the five edits below.

---

## What the trace says

### The damage budget

Boss pool 38,567,500. Raid dps against Hodir's own health curve:

| 0:30 | 0:45 | 1:00 | 1:30 | 2:15 | 3:00 | 4:30 | 5:45 |
|---|---|---|---|---|---|---|---|
| 105k | 162k | 160k | 105k | 68k | 65k | 110k | 66k |

Overall **92,026 dps**. The opening 60 s runs at 117,461, which alone projects a kill at **5:28** —
so this is not a fight that collapses after the cooldown window, it is a fight running at roughly
half the rate it needs from the first pull.

A 3:00 kill needs **214,264 dps**. Nothing in the trace's best window comes close.

The new v8/v9 probes reconcile cleanly: roster `snap.u[11]` totals **37,828,505** against
**32,805,516** taken off Hodir's health, ratio **1.15**. The 5.0M difference is ice blocks, which
matches the target data below. T1/T2/T3 are all confirmed working — icicles (131 guids), Flash Freeze
(67), Snowpacked Icicle and its target are all sampled now, where three earlier traces had zero.

### Problem 1 - the dodge re-derives its destination every 410 ms

`HodirIcicleDodgeAction` issued **4,197** `MOVEMENT_FORCED` moves in 357 s, 11.8/s raid-wide across
21 bots. Per bot:

| measure | value |
|---|---|
| gap between consecutive issues | p50 **410 ms**, 71% under 500 ms |
| distance between consecutive destinations | p50 **2.00 yd** |
| leg the bot was asked to walk | p50 **1.33 yd** |

Agony, 1:34.5-1:39.9 — 14 forced moves in 5.4 s, destination crawling
(1993.9,-247.6) → (1994.2,-249.2) → (1993.3,-250.6) → (1992.8,-252.2) → … → (1985.0,-257.4).

**Cause.** `UldActions_Hodir.cpp:119-130`. The `_dest` latch clears on arrival and then **falls
through to a fresh sweep in the same tick**:

```cpp
if (remaining <= ULDUAR_HODIR_DODGE_ARRIVE)   // 1.5
    _dest = Position();                        // …and control drops to the sweep below
```

`FindNearestPositionClearOfHazards` rings outward from the bot in **2.0 yd** steps and returns the
first clear point, so the sweep's own leg is ~2 yd, the bot covers it in ~280 ms, `remaining` is then
under `ULDUAR_HODIR_DODGE_ARRIVE` (1.5), `_dest` clears, and the next 2 yd hop is derived
immediately. A 2 yd hop at 2.4 Hz, forever. The comment above that latch describes this exact
failure ("a fresh destination every 420ms, each 2 yd past the last") — it is the bug the latch was
added to fix, and it is back through the arrive branch.

The `dup` counts confirm it: of 5,061 dodge attempts that were accepted or deduplicated, only
**864 (17%)** were the latch holding.

**Blast radius.** Every bot moves 41-64% of the fight. Gross travel 1,000-1,700 yd for a net
displacement of 15-55 yd — a gross/net ratio of 23-79x — with 15-35 direction reversals per minute.
`hodir biting cold shed` runs at `combat` and loses to the dodge's `forced` **1,002 times**
(p50 held 108 ms) and to other `combat` movers 2,079 more, so it never completes a leg either.

Throughput cost, measured directly from `dealt` over 5 s windows:

| fraction of window moving | windows | dps |
|---|---|---|
| 0-20% | 141 | 4,198 |
| 20-40% | 193 | **6,207** |
| 40-60% | 332 | 5,790 |
| 60-80% | 373 | 5,149 |
| 80-100% | 381 | 4,032 |

Most of the fight sits in the 40-100% bands. Moving everything to the 20-40% band is worth about
**+23% raid dps**.

### Problem 2 - Toasty Fire can never be accepted

`GetHodirRaidFire` (`UldEncounter_Hodir.cpp:131`) accepts a fire only when its distance to Hodir is

- `>= ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP` (15), and
- `+ ULDUAR_HODIR_RAID_RING_OUTER (9) + ULDUAR_HODIR_RING_SPOT_TOLERANCE (2)
  <= ULDUAR_HODIR_CASTER_MAX_BOSS_GAP (30)`, i.e. `<= 19`.

A 4-yard-wide annulus. Measured across 556 sampled Toasty Fire dynamic objects: **0 fall inside it**,
p50 gap **26.4 yd**, p10 23.2. So `hodir.fire` reads `none` for every bot for the entire fight —
23 note rows, all at t≈0, never changing.

Consequences: `GetHodirRingCentre` never leaves the fixed `ULDUAR_HODIR_RAID_ANCHOR`, the raid never
stands in fire, and Biting Cold is never shed by fire — uptime **31-54%** raid-wide, which is what
keeps `hodir biting cold shed` shuffling 23 bots for 1,834 issued moves.

### Problem 3 - the Starlight radius constant is wrong

Starlight (62807) is `SPELL_AURA_MELEE_SLOW` at base 49, i.e. **+50% to cast time and all three
attack timers** — the fight's single biggest throughput lever. A zone is on the floor in **93%** of
snapshots, so the druid helpers are being freed.

Measured aura-held rate against distance to the nearest zone centre:

| 0-1 yd | 1-2 | 2-3 | **3-4** | 4-5 | 5-6 |
|---|---|---|---|---|---|
| 94.5% | 89.9% | 77.5% | **20.6%** | 7.6% | 2.0% |

The real boundary is **3.0 yd**. The code uses `ULDUAR_HODIR_STARLIGHT_RADIUS = 4.0`, so
`GetHodirStarlightZoneAt` reports a bot as standing in a zone when it holds nothing — and that
function gates the shuttle's `starlight` branch and the shed's in-zone leg. Worse,
`ULDUAR_HODIR_STARLIGHT_STAND_RADIUS` (2.0) plus `ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE` (1.0)
lets a bot park **3.0 yd** from the centre — exactly on the boundary — and call it arrived.

Result: Starlight uptime is **0.0% on all 7 melee and both tanks** (melee are excluded from the
anchor by design at `UldEncounter_Hodir.cpp:528`) and **14.8-29.5%** on ranged. While a zone is up,
ranged are inside one only 26.8% of the time.

### Problem 4 - Storm Cloud lands on tanks and healers and nothing happens

Storm Power (65134) is `SPELL_AURA_MOD_CRIT_DAMAGE_BONUS` base 134 — **+135% crit damage**, spread
at 3 yd by the Storm Cloud carrier. Uptime is 16.8-50.4%, and 0.0% on Ecoterrorist.

Of 53 Storm Cloud applications, **14 went to a tank** (Bulwark 9, Ecoterrorist 5) and 6 to healers.
`HodirSpreadStormCloudAction` skips tanks when *choosing a lap direction* but never checks whether
the **carrier** is a tank — so a tank carrier either laps the ring and drags Hodir off the corner, or
does nothing. Both waste the whole 4-6 tick window.

### Where the raid's attention goes (measured, not changing this pass)

65.4% of alive ticks target Hodir, **32.8% target a Flash Freeze block**, concentrated on the ranged:
Fel 68.0%, Nightwarrior 67.0%, Trueshot 58.8%, Stormweaver 49.0%, Smartface 47.1%, Malediction 42.9%,
Druidica 42.1%, Hellflame 39.2%. Melee and tanks are 93-100% on the boss. 4-6 blocks are up at any
time, p50 lifetime 25.2 s. **Left alone this pass by decision** — changing it at the same time as the
buff fixes would make the next trace unreadable, and the buff fixes may make freeing helpers pay off.

### Ecoterrorist - he never left Dire Bear Form

Asked directly, answered directly. Dire Bear Form (9634) applied at **0:07.058** and removed at
**2:57.559** — the same millisecond as his death at 2:57.568, i.e. `RemoveAllAurasOnDeath`.
Re-applied 2:58.619 after the battle-res, removed again at 4:34.264 against his second death at
4:34.270. Both removals are consequences of dying, not causes. The `Improved Moonkin Form` (50172)
that comes and goes at 2:28/2:32 is someone else's raid buff, not his shapeshift.

He died to **Frozen Blows 63511** — 13 hits for 269,428, avg **20,725** each (DBC base 39,999, so
about 48% mitigated in bear). Death [2]: 105,752 over 17 hits, 70.3% of it three 63511 hits.
Death [3]: 50,747, one 63511 for 23,652 plus an Icicle for 11,825. Both deaths land from ~46-55%
health in a single blow, and `ULDUAR_HODIR_TAUNT_HEALTH_FLOOR` is 50.0 — the swap arms exactly where
he is dying. **Out of scope for this pass** (tank survival was not selected); recorded here so it is
not re-derived next time.

---

## Approach

Five edits, no new files, no schema change.

### E1 - the dodge holds its destination (`UldActions_Hodir.cpp:104-165`)

On arrival, **keep** `_dest` and return false, instead of clearing it and falling through to a fresh
sweep in the same tick. `FindNearestPositionClearOfHazards` rings outward in 2 yd steps from wherever
the bot is standing, so finishing a leg immediately bought another one.

The held spot is re-validated against the live hazard list every tick and only re-swept once it stops
being clear. That is strictly safer than the timed hold-off this plan first proposed: it re-derives
the instant an icicle lands on the spot, instead of standing the bot there for the rest of a timer.
No new constant.

Keep the `_destDist + ULDUAR_HODIR_DODGE_SLIP` regression check — that catches a knockback pushing
the bot off its walk, which is a real case and not what is firing here.

Lower `ULDUAR_HODIR_DODGE_ARRIVE` from 1.5 to **0.8**. At 1.5 it swallows a fifth of a sweep leg (the
ring step is 2.0 yd), leaving the bot inside the radius that re-arms the trigger.

### E2 - the anchor stops standing down for a shed that will not happen

**Not the Toasty Fire band, which was this plan's first draft and is wrong.** Relaxing it walks half
the ring out of casting range: a fire must leave the whole outer ring inside
`ULDUAR_HODIR_CASTER_MAX_BOSS_GAP` (30), and a 12-bot ring cannot sit on a fire 26 yd out and keep its
far side within 30 of the boss. That is geometry, not a bad constant, and the ceiling was 19% fire
uptime. Dropped.

What the Starlight measurement actually points at: a zone passes every gate on **85%** of ranged ticks
while they stand in one for **22%**, so the gate is not what rejects them — the anchor that would walk
them there is suppressed. `HodirRaidPositionTrigger` stands down whenever `HodirBitingColdTrigger` is
active, and that trigger fires on *any* stack, because the action owns the arm threshold and the
shed-to-zero latch. But **87%** of the time a bot holds Biting Cold it holds exactly one stack —
**32.8% of the fight each** — and there the shuttle does nothing at all.

Add `IsHodirBitingColdShedArmed(Player*)` to `UldEncounter_Hodir.{h,cpp}`: true when the bot holds at
least the stacks the shuttle arms at (3 in Starlight, else 2) and is not in a fire that sheds them for
free. `HodirBitingColdShedAction` and `HodirRaidPositionTrigger` both call it.
`HodirBitingColdTrigger` keeps firing on any stack — a shed already running still has to be carried
down to zero, and the trigger is a stack-allocated copy that cannot see the latch.

### E3 - correct the Starlight geometry (`UldEncounter_Hodir.h`)

- `ULDUAR_HODIR_STARLIGHT_RADIUS` 4.0 → **3.0**. Binned by distance the hold rate is 94/90/78% across
  the first three yards and 21% in the fourth, so the edge is 3 and the medians that read as 4 were
  sampling blur — bots cover 1.75 yd between snapshots.
- `ULDUAR_HODIR_STARLIGHT_STAND_RADIUS` 2.0 → **1.5**, tolerance left at **1.0**. The sum was landing
  exactly on the edge. Take the margin out of the stand radius and never the tolerance: tolerance is
  also what stands the position trigger down, so trimming it trades dodge churn for anchor churn.
- `ULDUAR_HODIR_STARLIGHT_SHED_RADIUS` 2.5 → **2.0**, so the in-zone shed leg keeps a yard inside 3.0
  at both ends.

### E4 - nothing to do

`HodirSpreadStormCloudTrigger` already refuses for a tank, with the same reasoning. This plan inferred
the gap from a comment in the action without reading the trigger.

14 of 53 carries in the pull went to a tank and 6 more to healers, and all of them are dead windows —
but the tank correctly stays in the corner. Collecting one needs the *receivers* to walk to the
carrier, which is new behaviour and out of scope.

### E5 - probe what E2 and E3 are supposed to change

`hodir.fire` already emits `none` vs a guid, so E2 is directly observable with no new probe.

Add one derived probe so Starlight uptime is attributable rather than inferred: in
`FindHodirStarlightStand`, record which rule decided — `stand`, or the rejection that won: `none` (no
zone in the search radius), `noreach` (outside the caster band), `fire` (outside the fire leash). The
rule, not the coordinate: the point itself is already `hodir.anchor`, which the stand becomes.

```cpp
RaidObs::NoteDerived(bot, "hodir.starlight", …);   // emits only on change
```

`hodir.shuttle` already reports `starlight`/`crowd`/`solo`/`tank` (191 crowd, 119 starlight, 58 solo,
2 tank this pull), so the shuttle side needs nothing.

## Files

- `src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp` — E1 (dodge latch), E2 (shed arms off the helper)
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp` — E2 (the position trigger's suppression)
- `src/Ai/Raid/Uld/Util/UldEncounter_Hodir.cpp` — E2 (the helper), E5 (probe)
- `src/Ai/Raid/Uld/Util/UldEncounter_Hodir.h` — E1/E2/E3 constants and the helper
- `docs/plans/hodir-dodge-oscillation/hodir-dodge-oscillation.PLAN.md` — copy of this plan, written
  first per the planning-directory rule

No `CMakeLists.txt` change; AzerothCore globs module sources. Run
`python apps/codestyle/codestyle-cpp.py` from `modules/mod-playerbots` before calling it done. There
is no headless build path here, so the build and the pull are the user's.

`docs/raids/ulduar/hodir.md` carried the 4.0 yd Starlight radius, a `_FIRE_ADOPT_RADIUS` that no
longer exists in the code, and a Storm Cloud line claiming tanks are never buff targets. Corrected
under `/compact-docs-writer`, along with the new findings.

It is reachable from `CLAUDE.md` through `docs/engine/pitfalls.md`, so that pass is required, not
optional.

## Verification

Rebuild, restart `ac-worldserver`, confirm the effective config with
`docker exec ac-worldserver env | grep ^AC_`, then pull Hodir with the same raid.

| check | this pull | expected |
|---|---|---|
| `hodir icicle dodge action` issued moves | 4,197 | under ~1,500 |
| gap between consecutive dodge issues | p50 410 ms | p50 over 900 ms |
| dodge issues under 500 ms apart | 71% | under 15% |
| per-bot gross/net travel ratio | 23-79x | under 15x |
| fraction of ticks moving | 41-64% | 25-45% |
| `hodir.starlight` values | probe absent | mostly `stand` for ranged |
| `hodir raid position action` issued moves | 583 | higher — it was suppressed a third of the fight |
| Starlight uptime, ranged | 14.8-29.5% | over 45% |
| Starlight uptime, melee and tanks | 0.0% | still 0.0%, they have no anchor |
| ranged ticks inside 3 yd of a zone | 22% | over 45%, against 85% with one in range |
| raid dps on the boss | 92,026 | over 115,000 |
| kill time | wipe at 5:56.9, 14.93% left | kill under 5:00 |

Keep `603_1_hodir_1788289957.ndjson` for the before/after — retention is 7 days.

Reconciliation to re-run, since it is the check that validates every `dealt` number above: sum
`snap.u[11]` across the roster at the end of the pull against `(1 - finalHp%) * 38,567,500` from
Hodir's own rows. It came out at ratio 1.15 here; a large move in that ratio means the block split
changed, not that throughput did.

## Out of scope

- **Melee and tank Starlight.** They are excluded from the anchor by design and sit p50 21.6 yd from
  the nearest zone; while one is up, melee are inside it 1.8% of the time. Fixing it means re-siting
  the fight around the zones, which the user deferred.
- **The Toasty Fire band.** 0 of 556 sampled fires sit in the 15-19 yd window `GetHodirRaidFire`
  accepts, so the ring never rides one and Biting Cold is never shed by fire. It is the ring's shape
  that blocks it, not the band: a 12-bot ring on a fire 26 yd out puts its far side 35 yd from the
  boss. A half-ring or an arc would fit, but neither keeps the 4.5 yd spacing Ice Shards forces.
- **The ice-block split.** 32.8% of raid attention for 13% of the damage, 8 breakers assigned.
  Deliberately unchanged so the next trace isolates the buff fixes.
- **Tank survival.** `ULDUAR_HODIR_TAUNT_HEALTH_FLOOR` at 50.0 against a 23.6k Frozen Blows is why
  Ecoterrorist died twice at 46-55%. Measured above, not touched.
- **Making a tank's Storm Cloud window pay.** E4 only stops the tank walking; collecting the buff
  from a stationary tank carrier is new behaviour.
- **The sweep cap.** Still binds at 41 rows in 93.8% of snapshots, but only far trash is being
  dropped — Winter Jormungar at 78+ yd and Dark Rune trash at 71+ yd. Every Hodir object is sampled.
  Working as intended.
