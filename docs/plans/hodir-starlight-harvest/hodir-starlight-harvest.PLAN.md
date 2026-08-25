# Hodir: harvest Starlight, stop chasing fires

## Context

Trace `env/dist/logs/botobs/603_3_hodir_1787601745.ndjson` (schema v6, 25-man, map 603 `diff:1`,
captured against `df8b2a339`). Read it with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`.

Raid: 1 bot tank (`Bulwark`), 4 bot healers, 8 bot melee, 10 bot ranged, plus two real players
carrying `h:1` and no bot AI - `Dragon` (prot paladin) and `Deathsong` (DK).

**Outcome: kill at 6:00.44 with 2 deaths** (`Angry` at 0:04, `Power` at 3:55). The previous pull
wiped at 8:26 with Hodir at 3.2% and 33 deaths, so `df8b2a339` worked: melee are on Hodir for 98.1%
of their ticks against 60.3% before, and exactly one bot was caught by Flash Freeze all fight.

**The hard-mode deadline is 3:00, not 2:00.** `SPELL_SHATTER_CHEST_TIMER` is 65272, an
`EffectAuraPeriod_1` of 180000 ms on a 180000 ms duration, cast on engage at
`boss_hodir.cpp:263`; its single tick fires `EVENT_HARD_MODE_MISSED` and shatters the Rare Cache.
So the target is halving this fight, not thirding it. Everything below is measured; the total on
offer is roughly a third to a half more raid dps, which lands near 4:00-4:30, not 3:00. Getting
under 3:00 needs this **plus** the three gaps listed as out of scope.

Damage taken, 6,163,616 total: Frozen Blows 4,297,530 (69.7%), Biting Cold 957,286 (15.5%), Freeze
447,935 (7.3%), boss melee 249,303 (4.0%), Ice Shards 157,693 (2.6%).

---

## The measured problem: 23.9% of dps time is productive

Across 18 bot dps over 360s of combat, per 250 ms snapshot tick, alive only:

| state | share |
|---|---|
| in range, on Hodir, standing still | **23.9%** |
| in range, on Hodir, but moving | 23.6% |
| on Hodir but out of range | 29.6% |
| not on Hodir at all (helper ice) | 23.0% |

Moving matters because casts land at **0.60 per tick while moving against 1.16 while still** - a
moving bot is at 52% throughput. Bots move 62.6% (melee) / 54.6% (ranged) of their alive ticks.

"In range" is 7.83 yd for melee - Hodir's `CombatReach` is 5.0 at `DisplayScale` 1
(`creature_template_model`), plus ~1.5 player reach plus 4/3, per `Unit::GetMeleeRange`
(`Unit.cpp:799`). Melee sit at p50 7.6, p75 15.7, p90 24.8.

---

# Finding 1 - Starlight is up 95% of the fight and nobody stands in it

Starlight (62807) is the single biggest throughput lever in the encounter: `EffectAura_1` 193 runs
through `HandleModCombatSpeedPct`, so +50% applies to cast time and all three attack timers. The
druid helper re-casts it every 10s (`boss_hodir.cpp:833`).

From the `hz` hazard channel (`[spellId,x,y,z,radius,hostile]`, `RaidObs.cpp:719-727`):

- Zones present in **95.1%** of snapshots, **p50 3 at once**, max 5.
- **85.8%** of snapshots have a zone 15-30 yd from Hodir - reachable by a caster who is still on
  the boss - and a median of 3 such zones.
- Average dps standing in one: **1.40 of 18**. 906 of 1371 snapshots have **zero**.
- Aura uptime on dps: **5.7%**, 20.6s each across six minutes.

**Cause.** `FindHodirStarlightStand` (`UldBossHelper.cpp:858`) rejects any stand point more than
`fireLeash` = `ULDUAR_HODIR_TOASTY_FIRE_RADIUS - ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE` = 10 yd
from the ring centre. The nearest zone to the ring centre is a median **16.3 yd** away, and only
**10.5%** of snapshots have one inside 10.

The leash itself is sound reasoning - inside a Toasty Fire, Biting Cold neither applies nor stacks
(`boss_hodir.cpp:1225` and `:1259` both test `SPELL_MAGE_TOASTY_FIRE_AURA`) - but it is applied
**unconditionally**, and the ring is actually riding a fire only 20.1% of the time. The other 79.9%
it is a 10 yd box drawn around `ULDUAR_HODIR_RAID_ANCHOR` that protects nothing.

The anchor note confirms the step is losing: ranged hold a Starlight anchor (`~1.0` tolerance) for
27.6% of ticks and the plain ring slot (`~2.0`) for 72.4%.

# Finding 2 - the ring chases fires out of spell range

`GetHodirRaidFire` (`UldBossHelper.cpp:687`) adopts a fire on its distance from the **fixed**
`ULDUAR_HODIR_RAID_ANCHOR` (<= `ULDUAR_HODIR_FIRE_ADOPT_RADIUS` 25). Hodir moves; the anchor does
not.

- Toasty Fires sit **p25 33.5 / p50 36.1 / p75 41.4 yd from Hodir**, and **0%** of fire rows are
  within 11 yd of him - so melee can never be in one. Melee Toasty Fire uptime is 2.0-4.3%.
- Ring centre distance to Hodir: p50 18.3, **p75 29.1, p90 33.9, max 52.3**.
- `hodir raid position action` is 14.2% of ranged ticks and 25.6% of their off-boss time; `reach
  spell` is another 4.6% walking back into range.
- Ranged targeting Hodir already stand at p50 25.4, **p75 33.4, p90 38.1** yd from him.

A ring with a 9 yd outer radius centred on a fire 36 yd out puts its far side at 45 yd. The fire
buys 13.6-21.8% Toasty Fire uptime for ranged and costs the whole caster group its spell range.

**Decision taken: gate the fire on Hodir.** Stated plainly so it is not a surprise on the next
trace - with this mage's drop pattern, **0 of 959 fire rows** pass that gate, so the ring will form
on the fixed anchor for the whole fight. The rule still adopts a fire if one ever lands close; in
practice this is the same outcome as dropping the fire ride, reached by a criterion that stays
correct if fire placement ever changes.

# Finding 3 - the Biting Cold shed will fight the Starlight stand

With the ring off fires, a bot parked in a 4 yd zone accumulates Biting Cold, and
`HodirBitingColdShedAction` (`UldActions_Hodir.cpp:174`) arms at
`ULDUAR_HODIR_BITING_COLD_SHED_STACKS` = 2 and walks a 6 yd declump leg - straight out of the zone.
Today that is 14.0% of ranged and 13.7% of melee ticks.

Shedding needs **two consecutive 1-second ticks** with `isMoving()` true: `boss_hodir.cpp:1259-1267`
toggles `_prev` and only calls `ModStackAmount(-1)` on the second. Gaining takes four stationary
ticks (`_counter >= 4`). Stacks reached in this trace: 2821 applications at 1, 426 at 2, 36 at 3,
15 at 4. Damage is `200 * 2^stacks` per tick.

Jumping does count - `isMoving()` tests `MOVEMENTFLAG_MASK_MOVING`, which includes
`MOVEMENTFLAG_FALLING` (`UnitDefines.h:405-408`) - but it is not a way out: the same flag blocks and
cancels cast-time spells (`Spell.cpp:3565`, `Spell.cpp:4412`), so a jumping caster is not casting.

**Decision taken: shed inside the zone, and hold to 3 stacks while in one.**

# Finding 4 - the helper-ice assignment is distance-blind

Ranged spend **35.1% of their alive ticks** targeting a helper block (32938), and are **moving
61.6%** of that time. 59 blocks were targeted, a median 20.2s each (p90 38.5, max 46.6), median 2
distinct attackers.

The cap of 8 is delivering - every block gets freed and Starlight zones are up 95% of the fight -
so the cap stays. But `GetHodirAssignedHelperBlock` (`UldBossHelper.cpp`) walks blocks in guid order
and starts each block's search at `block->GetGUID().GetCounter() % total`, deliberately ignoring
distance. A bot gets sent across the room, and the round trip is most of the 20.2s.

---

## Approach

Four edits, no new files, no new nodes, no strategy or context wiring.

### A. Let the Starlight step see the zones that exist

`UldBossHelper.h` - one new constant:

```cpp
// How far from Hodir a Starlight zone may be and still be somewhere a caster can stand: the boss has
// to stay inside the shortest caster range in the raid, and a Shadow Bolt is 30. Measured, this keeps
// 85.8% of snapshots holding a usable zone, a median of 3 of them; ranged already stand at a p75 of
// 33.4 from him, so the band pulls them in rather than out.
constexpr float ULDUAR_HODIR_STARLIGHT_MAX_BOSS_GAP = 30.0f;
```

`GetHodirRingCentre` (`UldBossHelper.cpp:709`) grows an optional `bool* onFire = nullptr` out-param
so the caller learns whether a fire was adopted **without a second grid sweep** - it already calls
`GetHodirRaidFire` and that sweep is the expensive part.

`FindHodirStarlightStand` (`:858`) takes `bool onFire` and replaces the unconditional leash:

```cpp
// The leash is only real when the ring is on a fire: inside one, Biting Cold neither applies nor
// stacks, so a zone outside it would be shed straight back out of. Off a fire the bot is shedding
// wherever it stands, and the only thing that matters is whether Hodir is still in range.
if (onFire && centre.GetExactDist2d(&stand) > fireLeash)
    continue;

if (hodir)
{
    float const gap = stand.GetExactDist2d(hodir);
    if (gap < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP || gap > ULDUAR_HODIR_STARLIGHT_MAX_BOSS_GAP)
        continue;
}
```

The existing `hodir` lookup and its `RANGED_MIN_BOSS_GAP` test fold into the same block. Keep the
shortest-walk tie-break and everything else unchanged.

### B. Gate the fire on Hodir instead of on a fixed point

`GetHodirRaidFire` (`:687`) - replace the anchor-relative gate. Hodir moves and the anchor does not,
which is the whole bug:

```cpp
// Measured from Hodir, not from ULDUAR_HODIR_RAID_ANCHOR: he drifts, and a fire picked off a fixed
// point put the far side of the ring 45 yd from him. A fire is only worth forming on if the whole
// ring still reaches him from it.
float const gap = fire->GetExactDist2d(hodir);
if (gap < ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP ||
    gap + ULDUAR_HODIR_RAID_RING_OUTER + ULDUAR_HODIR_RING_SPOT_TOLERANCE >
        ULDUAR_HODIR_STARLIGHT_MAX_BOSS_GAP)
    continue;
```

That leaves a 15-19 yd window. `ULDUAR_HODIR_FIRE_ADOPT_RADIUS` becomes unused - delete it and its
comment block. `best`/`bestDist` now rank on `gap`, nearest to Hodir winning.

### C. Shed inside the zone, and hold to 3 stacks while in one

`UldBossHelper.h`:

```cpp
// Where the shed legs sit when the bot is standing in Starlight. Both ends and the straight path
// between them stay inside the zone, so the aura survives the shuttle; 5 yd is as long a leg as a
// 4 yd zone allows and still covers two aura ticks when the legs chain.
constexpr float ULDUAR_HODIR_STARLIGHT_SHED_RADIUS = 2.5f;
static_assert(ULDUAR_HODIR_STARLIGHT_SHED_RADIUS < ULDUAR_HODIR_STARLIGHT_RADIUS,
              "both ends of the shed shuttle have to stay inside Starlight");

// Standing in Starlight is worth more than a stack: +50% to every cast and swing against 1600 a
// tick. Only 36 of 3298 stack applications in a six minute kill ever reached 3.
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT = 3;
```

New helper beside `FindHodirStarlightStand`, exported through the header:

```cpp
// The zone the bot is currently standing in, if any. Nearest centre within ULDUAR_HODIR_STARLIGHT_RADIUS.
bool GetHodirStarlightZoneAt(PlayerbotAI* botAI, Player* bot, Position& out);
```

It reuses `GetDynamicObjectPositions(bot, ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS, SPELL_HODIR_STARLIGHT)`,
the same call `FindHodirStarlightStand` already makes.

`DeriveHodirShuttleLeg` (`:921`) gains a branch between the tank branch and the crowd branch, built
exactly like the tank shuttle - two opposite points through a centre, take whichever is further so
`IsDuplicateMove` can never refuse it and the legs chain without a stationary tick:

```cpp
Position zone;
if (GetHodirStarlightZoneAt(botAI, bot, zone))
{
    float const bearing = std::atan2(bot->GetPositionY() - zone.GetPositionY(),
                                     bot->GetPositionX() - zone.GetPositionX());
    float const dx = std::cos(bearing) * ULDUAR_HODIR_STARLIGHT_SHED_RADIUS;
    float const dy = std::sin(bearing) * ULDUAR_HODIR_STARLIGHT_SHED_RADIUS;

    Position const legA(zone.GetPositionX() + dx, zone.GetPositionY() + dy, zone.GetPositionZ());
    Position const legB(zone.GetPositionX() - dx, zone.GetPositionY() - dy, zone.GetPositionZ());

    out = bot->GetExactDist2d(&legA) > bot->GetExactDist2d(&legB) ? legA : legB;
    how = "starlight";
    return true;
}
```

Run both ends through `ValidateHodirFloorPoint`, the way `FindHodirStarlightStand` does its stand.

`HodirBitingColdShedAction::Execute` (`UldActions_Hodir.cpp:174`) picks the threshold from whether
the bot is in a zone:

```cpp
uint32 const arm = bot->HasAura(SPELL_HODIR_STARLIGHT) ? ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT
                                                       : ULDUAR_HODIR_BITING_COLD_SHED_STACKS;
if (!_shedding && cold->GetStackAmount() < arm)
    return false;
```

Aura, not position: the aura is what the bot is actually paid for, it costs no grid sweep, and it
is the thing that is about to be lost.

### D. Give the block assignment a stable sense of distance

`GetHodirAssignedHelperBlock` (`UldBossHelper.cpp`) keeps the cap of 8, one breaker per block, the
ranged-only candidate list, the `MIN_FREE` floor and the guid sorts. Only the pairing changes:
instead of starting each block's search at `block->GetGUID().GetCounter() % total`, walk blocks in
guid order and give each the **unassigned candidate whose ring slot is nearest that block**.

The slot is the stable proxy the existing comment demands - it is derived from the guid-sorted ring
roster and the ring centre, so it does not move between ticks the way a live position does, and it
is where the bot is trying to stand anyway. Derive the centre once with `GetHodirRingCentre` and the
slots with `GetHodirRingSlot(botAI, member, centre, slot)`, both already used by `DeriveHodirAnchor`.
A candidate whose slot cannot be derived falls back to the end of the order rather than being
dropped.

## Files

- `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}` - `ULDUAR_HODIR_STARLIGHT_MAX_BOSS_GAP`,
  `ULDUAR_HODIR_STARLIGHT_SHED_RADIUS`, `ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT`,
  delete `ULDUAR_HODIR_FIRE_ADOPT_RADIUS`; `GetHodirStarlightZoneAt`; edits to
  `FindHodirStarlightStand`, `GetHodirRingCentre`, `GetHodirRaidFire`, `DeriveHodirShuttleLeg`,
  `DeriveHodirAnchor`, `GetHodirAssignedHelperBlock`
- `src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp` - the shed threshold in
  `HodirBitingColdShedAction::Execute`

No `CMakeLists.txt` - AzerothCore globs module sources. Run `python apps/codestyle/codestyle-cpp.py`
from `modules/mod-playerbots` before calling it done.

**Another session is working on Thorim and RaidObs v7 in this repo.** `UldBossHelper.h` is already
modified in the working tree by that session. Do not stage whole files; stage hunks, and check
`git diff --cached | grep -i thorim` is empty before committing.

## Deliberately out of scope

Named because they are what stands between the result this buys and the 3:00 chest, not because
they are small.

- **Melee out of range 48.6% of their ticks.** Attributed: `hodir move snowpacked icicle` 28.4%,
  `hodir biting cold shed` 25.2%, `reach melee` 23.0% walking back, `hodir icicle dodge action`
  18.3%. `reach melee` is refused **1076 times as "wait"** behind the FORCED dodge.
- **The dodge still re-issues at 2.5 Hz.** Median 401 ms between accepted dodge moves in 2.0 yd
  hops - unchanged from the 420 ms / 2.0 yd before the latch shipped in `df8b2a339`. The latch
  releases because `ULDUAR_HODIR_DODGE_ARRIVE` is 1.5 yd and `FindNearestPositionClearOfHazards`
  steps 2.0, so the bot "arrives" almost immediately and picks again; only 20% of destinations are
  covered by a fresh icicle 1.2s later, so re-deriving is not what drives it. Direction did improve:
  43% of melee dodge destinations are further from Hodir, against 89% before.
- **Storm Cloud lands on people who cannot spread it.** 23 carrier episodes, **12 on a human player,
  the tank or a healer**. `HodirStormCloudTrigger` (`UldTriggers_Hodir.cpp:268`) only fires for the
  carrier, so when the carrier is one of the two humans nothing moves at all. Storm Power (65134,
  30s) uptime 28.4%, 63 applications spread 1-6 per bot.
- **`docs/raids/ulduar.md:435-525`** still describes the 8 yd Starlight premise and still names the
  removed `GetHodirDruidHelper`. Needs its own `/compact-docs-writer` pass; this change makes it
  wronger.

## Verification

The module cannot be compiled headless here, so hand the build off and verify from a fresh trace.

1. Rebuild and restart `ac-worldserver`; confirm the effective config with
   `docker exec ac-worldserver env | grep ^AC_`, since `configurationOverrides/*.env` overrides
   `playerbots.conf`.
2. Pull Hodir with the same raid, by boss so the trace stays boss-scoped.
3. On the new trace:
   - **Starlight aura uptime on dps** - was 5.7%. This is the number the whole change exists to
     move; anything under 40% means the band is still rejecting zones.
   - **Average dps standing in a zone while one exists** - was 1.40 of 18, with 906 of 1371
     snapshots at zero. Expect the zero share to collapse.
   - **Anchor kind for ranged** - `hodir.anchor` at `~1.0` was 27.6% of ticks against `~2.0` at
     72.4%. Expect the split to invert.
   - **Ring centre distance to Hodir** - was p50 18.3 / p75 29.1 / p90 33.9 / max 52.3. With the
     fire gate it should sit flat at the fixed anchor's distance, and `hodir.fire` should read
     `none` for the whole fight. If it ever adopts one, check the ring still reaches the boss.
   - **`reach spell` share of ranged ticks** - was 4.6%; should fall toward zero.
   - **Ranged distance to Hodir while on him** - was p50 25.4 / p75 33.4 / p90 38.1. p90 should come
     inside 30.
   - **Biting Cold** - 957,286 taken, 15.5% of all damage, and the stack histogram 2821/426/36/15 at
     1/2/3/4. The 3-stack hold should move mass from 2 into 3; a 4-stack tail that grows means the
     in-zone shuttle is not shedding and the threshold has to come back down.
   - **`hodir.shuttle` note** - a new `starlight` value should appear. If it never does,
     `GetHodirStarlightZoneAt` is not matching and the shed is still walking bots out.
   - **`hodir biting cold shed` share of ticks** - was 14.0% ranged / 13.7% melee. Melee should be
     unchanged (they are never in a zone); ranged should fall.
   - **Ranged time on helper blocks and the moving share of it** - was 35.1% of ticks, 61.6% of it
     moving, blocks targeted a median 20.2s. The cap is unchanged, so the target is the moving share
     and the 20.2s, not the 35.1%.
   - **The time budget** - the four-way split was 23.9% / 23.6% / 29.6% / 23.0%. "In range, on
     Hodir, standing still" is the headline.
   - **Kill time and deaths** - 6:00.44 and 2 deaths. Deaths rising past ~6 means the 3-stack hold
     is being paid for by the healers rather than by the boss.
4. Keep `603_3_hodir_1787601745.ndjson` for the before/after. Retention is 7 days by default.

On approval, copy this document to
`modules/mod-playerbots/docs/plans/hodir-starlight-harvest/hodir-starlight-harvest.PLAN.md`
before starting, and fold the durable findings into `docs/raids/ulduar.md` once it ships.


---

## As implemented

Four deviations from the approach above, all deliberate.

**The band constant is `ULDUAR_HODIR_CASTER_MAX_BOSS_GAP`, not `ULDUAR_HODIR_STARLIGHT_MAX_BOSS_GAP`.**
Both the fire gate and the Starlight gate test against it, and both are asking the same question -
how far from Hodir a caster may stand and still reach him - so naming it after one of its two users
would have read as a coincidence. It pairs with the existing `ULDUAR_HODIR_RANGED_MIN_BOSS_GAP` as
the far end of one band.

**Edit D ranks on slot points derived from `ULDUAR_HODIR_RAID_ANCHOR`, not from
`GetHodirRingCentre`.** Calling the real centre inside `GetHodirAssignedHelperBlock` would sweep the
grid for a fire a second time per bot per tick. The slot point here is only a ranking key - nobody
walks to it - and it only has to be stable and identical across the raid, which the fixed anchor
already is. After edit B the two are the same point on almost every pull anyway.

**`ULDUAR_HODIR_STARLIGHT_SHED_RADIUS` is 2.5, giving a 5 yd leg, one yard shorter than the declump
leg the shuttle comment calls the shortest move that spans two aura ticks.** It is sized off the p90
of 3.3 that bots actually holding the aura measure at rather than the 4 the radius nominally reaches,
so both ends keep a yard of margin. The leg being short is safe because the shuttle chains -
`_shedding` stays latched until the aura is gone and each tick issues the opposite end - so the bot
never stops and it is the chain, not the single leg, that covers the ticks. Same mechanism as the
tank shuttle, which runs a 6 yd leg off a 3.0 half-leg.

**The measured ceiling for this change is 73.1%, not 85.8%.** The 85.8% figure in Finding 1 is
Hodir-relative: it counts snapshots where a zone sits 15-30 yd from the boss.
`FindHodirStarlightStand` searches `ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS` = 30 yd **from the bot**,
and the nearest zone to a ranged bot is p90 33.7 yd away, so some in-band zones are out of the
search. Replaying the shipped gate over this trace - band 15-30, search 30 - gives **73.1% of ranged
bot-ticks with an acceptable zone in reach**, against 5.7% actual aura uptime under the old gate.

That is the number to judge the next trace against. If uptime plateaus well under it, the gate is
still rejecting; if it lands near it and more is wanted, the known next lever is the search radius,
which is a grid sweep per bot per tick and was left alone deliberately:

| search radius | band 15-30 | band 15-33 |
|---|---|---|
| 30 (shipped) | 73.1% | 78.4% |
| 35 | 80.3% | 85.7% |
| 40 | 83.7% | 89.1% |

## Verification status

Static only - the module cannot be compiled headless here.

- Braces and parentheses balance on all three edited files.
- `python apps/codestyle/codestyle-cpp.py` reports the same three pre-existing double-blank findings
  in `UldBossHelper.h` as `HEAD` does, shifted by the inserted lines, and nothing new. The
  `RaidObs.cpp` `GetTypeId()` findings and the `Engine.cpp` / `PlayerbotCommandScript.cpp` findings
  are pre-existing and belong to other work.
- No line added exceeds 120 characters; the three over-length lines in `UldBossHelper.cpp` are
  byte-identical to `HEAD`.
- `ULDUAR_HODIR_FIRE_ADOPT_RADIUS` has no remaining references anywhere under `src/`.
- Definition order holds: `GetHodirStarlightZoneAt` (926) precedes `DeriveHodirShuttleLeg` (967);
  `ValidateHodirFloorPoint` (818) precedes both; `BuildHodirRingMembers` (739) and
  `HodirRingSlotPoint` (778) precede `GetHodirAssignedHelperBlock` (1255).
- `GetHodirRingCentre`'s new parameter is defaulted, so the two callers that do not want the flag
  (`UldActions_Hodir.cpp:333`, `UldBossHelper.cpp:647`) are unchanged.
- `GetHodirRaidFire` still returns early on a null Hodir, so the new `fire->GetExactDist2d(hodir)`
  cannot dereference null.
- `ULDUAR_HODIR_RETURN_LEASH` is measured against the anchor, which is now the Starlight stand when
  one is found, so the leash pulls the bot toward the zone rather than away from it.

**Not compiled, and not run.** The build and a fresh trace are what actually confirm any of this.
