# RaidObs v8: measure what Hodir actually does

## Context

`3e1ebb2e3 Hodir tweaks for improved DPS uptime` shipped four edits (Starlight stand gate, a
Hodir-relative fire gate, an in-zone Biting Cold shuttle, distance-aware ice-block pairing) against
the 6:00.44 kill in `env/dist/logs/botobs/603_3_hodir_1787601745.ndjson`. It has not been rebuilt or
re-pulled.

Auditing that trace before re-pulling found that **RaidObs cannot verify two of the four edits**, and
that the original analysis leaned on a channel that does not mean what it looked like. Fix the
recorder first, then pull. Nothing here changes bot behaviour.

Read a trace with `python modules/mod-playerbots/tools/botobs/postmortem.py <file>`. Schema and probe
rules: `docs/systems/observability.md`. Recorder: `src/Bot/Obs/`.

---

## What the trace already answers

Two channels were under-used rather than missing. No work needed; noted so the next analysis uses them.

**Boss HP is a raid-DPS curve.** Hodir appears in 1441 of 1536 snapshot `u[]` rows (he is in
`s.watched`, so the sweep cap below never touches him). `Num()` writes `%.2f` on a 38,973,488 pool, so
one LSB is 3,897 HP against a median 250 ms drop of 23,384 — ample resolution.

| 0:30 | 0:45 | 1:00 | 2:30 | 3:00-6:00 |
|---|---|---|---|---|
| 242k | 205k | 112k | 57k | ~100k plateau |

At the opening rate Hodir dies at 2:41. The fight does not underperform evenly; it loses ~60% of its
rate after the opening cooldown window. The prior analysis missed this.

**`cast.ct` carries haste**, so Starlight landing is directly observable: Fireball spans 2296→1079 ms,
Chain Heal sits in four clean buckets (2015 / 1848 / 1550 / 1343). Only 748 bot casts carry `ct > 0`,
so it is a thin sample, but it is a real one.

**Correction to carry forward.** The prior "casts land 0.60/tick moving vs 1.16 still" ran over the
whole `cast` channel. `NoteCast` is hooked on `ALLSPELLHOOK_ON_PREPARE`, so 64% of bot records carry no
target and the top entries are passive procs — Biting Cold 62188/62039 is 13% of them, then Judgement
of Wisdom/Light, Fel Synergy, Necrosis, Blood Presence. The number measured procs as much as casts.
Redo it filtered to `tgt == Hodir` (90-276 casts per ranged bot over 360 s); T3 below makes that filter
exact rather than a proxy.

---

## Blind spot 1 - no outgoing damage exists

`NoteDamage` ([RaidObs.cpp:1531](src/Bot/Obs/RaidObs.cpp#L1531)) returns unless
`TracksPlayer(s, victim)`. All 5019 `dmg` records in the trace have a raid-member victim. So the raid's
DPS curve can be read off the boss but cannot be attributed to a bot, a position, or an aura — which is
exactly what "did Starlight convert into damage" asks.

The three feeds (`OnSendSpellNonMeleeDamageLog`, `OnSendAttackStateUpdate`, `OnSendPeriodicAuraLog` in
`RaidObsScripts.cpp`) already see both directions. Only the gate inside `NoteDamage` discards it.

## Blind spot 2 - Hodir's encounter objects have no positions

Ice blocks (32938), Toasty Fires (33342), Icicles (33169), Snowpacked Icicles (33173/33174) and the
frozen helper NPCs get **zero** snapshot rows:

| trace | span | creature rows/snap p50 | at cap | Hodir-object rows |
|---|---|---|---|---|
| `..._1787601745` | 390 s | 41 | 93.8% | **0** |
| `..._1787595615` | 536 s | 41 | 95.5% | **0** |
| `..._1787582677` | 445 s | 41 | 94.6% | 366 |

Ranged bots spend 32.6% of their alive ticks targeting an ice block, and the block's position is known
in **0.0%** of them.

**Cause.** `SweepArea` ([RaidObs.cpp:694](src/Bot/Obs/RaidObs.cpp#L694)) walks
`Cell::VisitObjects` output in raw grid order and stops taking creatures at `swept >= OBS_MAX_WATCHED`
(40). Thorim's arena sits inside the 150 yd sweep with 40-odd permanently parked trash, so the budget
is spent before Hodir's own platform is reached:

| swept creature | rows | p50 distance from Hodir |
|---|---|---|
| Dark Rune Commoner | 25438 | 112.6 |
| Dark Rune Warbringer | 10258 | 112.4 |
| Thorim Event Bunny | 5799 | 128.5 |
| Dark Rune Champion | 5092 | 121.7 |
| Thunder Orb | 4837 | 130.5 |

Nearest Thorim trash is 74.1 yd out. Raid members sit p50 17.3 / p99 46.9 from Hodir, and blocks and
fires spawn among them, so a distance-ordered cap separates the two cleanly with room to spare.

`observability.md` states the sweep exists so "hazard units carrying no dynamic object that never enter
combat are swept into `snap.u`". At Hodir it achieves the exact opposite of that. Starlight zones are
unaffected — they are dynamic objects in `hz[]`, which is uncapped, and are present in 95.1% of
snapshots.

This is what blocks verifying the shipped **distance-aware block pairing**, and the icicle dodge named
as the next lever after it.

---

## Approach

Three probes, one schema bump to **v8**. Size is not a constraint: `Obs.MaxFileMB` is 256 and these
traces run 20-27 MB; the two additive fields cost about +2%.

### T1 - order the sweep by distance before capping it

`SweepArea` ([RaidObs.cpp:694](src/Bot/Obs/RaidObs.cpp#L694)). Keep the single grid visit and the
inline dynamic-object branch (hazards are uncapped and must stay that way). Collect the creatures that
pass the existing filters — alive, hostile to `anchor`, not already in `s.watched` — into a local
vector instead of emitting them inline, then `std::nth_element` / `partial_sort` on
`anchor->GetExactDist2dSq(creature)` and emit the nearest `OBS_MAX_WATCHED`.

No new field, no byte change, no reader change. The cap keeps its original purpose; it just stops being
first-come.

The anchor is the first roster player, not the boss. That is correct and unchanged — it is where the
raid is standing.

### T2 - per-bot damage dealt, cumulative, in the snapshot row

- `BotTrace` ([RaidObs.cpp:273](src/Bot/Obs/RaidObs.cpp#L273)) gains `uint64 damageDealt = 0;`.
- `NoteDamage` splits into two independent blocks. The existing incoming-damage record is untouched.
  Ahead of it, resolve the session from the **attacker** and, when the attacker is a player that
  session tracks (or a pet/totem whose owner is — mirror the owner resolution `NoteCast` already does
  at [RaidObs.cpp:1730](src/Bot/Obs/RaidObs.cpp#L1730)) and the victim is *not* a tracked player, add
  `amount` to that bot's `damageDealt`. Emit nothing.
- `UnitRow` ([RaidObs.cpp:620](src/Bot/Obs/RaidObs.cpp#L620)) takes `uint64 dealt = 0` and appends it
  as element `[11]`. `BuildSnapshotPayload` passes `session->bots[GuidKey(guid)].damageDealt` for
  roster players and leaves the default for vehicles and swept creatures — it already resolves that
  same `BotTrace` to stamp `lastHpPct`, so the lookup costs nothing extra.

**Cumulative, not a delta**, so a coalesced or dropped snapshot loses nothing and any window
differences cleanly. `amount` is post-mitigation, matching what the boss's health actually loses — so
the per-bot sum reconciles against the HP curve, which is the verification below.

Cost: ~7 chars x 25 rows x 4/s = ~0.25 MB per six-minute pull.

### T3 - mark triggered casts

`NoteCast` ([RaidObs.cpp:1715](src/Bot/Obs/RaidObs.cpp#L1715)) takes `bool triggered` and emits
`,"tr":1` only when set — omitted otherwise, so a real cast costs nothing, the way `move` already omits
`hpr`/`hms` off a `wait`.

`RaidObsSpellScript::OnSpellPrepare` (`RaidObsScripts.cpp`) has the `Spell*` and passes
`spell->IsTriggered() || spell->GetTriggeredByAuraSpellInfo()`. `Spell::IsTriggered()` is
`_triggeredCastFlags & TRIGGERED_FULL_MASK` (`Spell.h:568`), which catches a triggered cast; the aura
term catches a proc that carries no cast flag. `NoteCast` has exactly one caller, so no other site
changes.

Do not filter these out at the recorder. Blood Presence 50475 fires 814 times in 360 s and Judgement of
Wisdom 1316 — a rate worth looking at, and dropping the records would hide it. Flag, filter on read.

### Schema and docs

- `SCHEMA_VERSION` → 8 (`RaidObs.h:34`); `SUPPORTED_SCHEMA = 8` and `READABLE_SCHEMAS = (4, 5, 6, 7, 8)`
  (`postmortem.py:24,29`). Both additions are additive, so older traces stay readable.
- `postmortem.py` needs no read-path change to keep working — it indexes `u[]` rows at 0-7 only
  ([:248](tools/botobs/postmortem.py#L248), [:530](tools/botobs/postmortem.py#L530),
  [:616](tools/botobs/postmortem.py#L616)) and element 11 is past all of them. Surfacing damage in the
  summary view is optional and can follow later.
- `docs/systems/observability.md`: the `snap` row in the schema table gains element 11; the `cast` row
  gains `tr`; the paragraph asserting hazard units get swept into `snap.u` needs the nearest-first rule
  and the Hodir measurement that forced it. **Invoke `/compact-docs-writer` as the first step of that
  doc edit**, per the standing rule — no invocation from an earlier cycle carries over.

## Files

- `src/Bot/Obs/RaidObs.cpp` — `SweepArea`, `NoteDamage`, `UnitRow`, `BuildSnapshotPayload`, `BotTrace`
- `src/Bot/Obs/RaidObs.h` — `SCHEMA_VERSION`, the `NoteCast` signature
- `src/Bot/Obs/RaidObsScripts.cpp` — the `triggered` argument at `OnSpellPrepare`
- `tools/botobs/postmortem.py` — schema constants
- `docs/systems/observability.md` — schema table and the sweep paragraph

No `CMakeLists.txt`; AzerothCore globs module sources. Run `python apps/codestyle/codestyle-cpp.py`
from `modules/mod-playerbots` before calling it done. There is no headless build path here, so the
build and the pull are the user's.

## Verification

Rebuild, restart `ac-worldserver`, confirm the effective config with
`docker exec ac-worldserver env | grep ^AC_` (gitignored `configurationOverrides/*.env` overrides
`playerbots.conf`), then pull Hodir with the same raid.

**The recorder, on the new trace:**

| check | v7 measured | expected v8 |
|---|---|---|
| Ice block / fire / icicle rows in `snap.u` | 0 | non-zero every snapshot one is alive |
| Dark Rune / Thorim Event Bunny rows | ~54k of ~63k | 0 — all are 74+ yd out |
| creature rows per snapshot | p50 41, cap-bound 93.8% | unchanged; the cap may still bind |
| `u[]` row length | 11 | 12, every row |
| ranged block-target ticks with a known block position | 0.0% | ~100% |
| `cast` records carrying `tr` | absent | present on the procs, absent on Fireball/Shadow Bolt |
| bytes per second | 51.0 KB/s | ≤ 53 KB/s |
| `postmortem.py` default / `--death` / `--bot` / `--track` / `--notes` | exit 0 | exit 0, and still exit 0 on the three v6 Hodir traces |

**Reconciliation, which is the real test of T2:** sum element 11 across the roster at the end of the
pull and compare against `(1 - finalHp%) * 38,973,488` from Hodir's own rows, plus what the raid put
into ice blocks and icicles. Expect the raid total to exceed the boss total by roughly the block damage
and to track it closely in shape. A large unexplained gap means pets, totems or a periodic feed are
being dropped or double-counted.

**Then re-run the Hodir analysis**, which is what the probes exist for:

- Raid DPS curve from boss HP — does the 242k → 57k collapse after 1:00 survive the shipped change, and
  does per-bot damage say who stops?
- Starlight aura uptime on dps (was 5.7%, replayed ceiling for the shipped gate 73.1%) cross-checked
  against `cast.ct` — a bot holding the aura must show shorter cast times, or the aura is being counted
  but not paid.
- Distance from each ranged bot to its assigned block, now measurable. This is the verdict on the
  distance-aware pairing; blocks were previously targeted a median 20.2 s each with 61.6% of that time
  moving.
- Biting Cold stack histogram (was 2821/426/36/15 at 1/2/3/4). The 3-stack hold should move mass from
  2 into 3; a growing 4-stack tail means the in-zone shuttle is not shedding.
- A `starlight` value on the `hodir.shuttle` note. Absent means `GetHodirStarlightZoneAt` never
  matched and that edit is inert.
- Kill time and deaths (6:00.44, 2). Deaths past ~6 means the healers are paying for the haste.

Keep `603_3_hodir_1787601745.ndjson` for the before/after — retention is 7 days.

## Out of scope

- **Any bot-behaviour change.** This is recorder work only. The three known Hodir gaps — melee out of
  range 48.6% of ticks, the dodge re-issuing at 2.5 Hz in 2.0 yd hops, and Storm Cloud landing on the
  two humans and the tank who cannot spread it — stay untouched until the new trace can price them.
- **`docs/raids/ulduar.md:435-525`**, still written against the wrong 8 yd Starlight premise and still
  naming the removed `GetHodirDruidHelper`. Its own `/compact-docs-writer` pass.
- **`haz` geometry.** Still timeline-only and still never tested against a death, per the v7 decision.
- **The Ulduar trigger leak.** `RaidUlduarStrategy::InitTriggers` registers all 90 triggers for every
  boss, and this trace carries `thorim.squadsassigned`, `thorim.markscleared`, `thorim.dpstarget`,
  `thorim.arenaarrived` and `thorim.followstripped` notes fired during a Hodir pull. Real, already
  flagged in the v7 plan, and strategy work rather than recorder work.
