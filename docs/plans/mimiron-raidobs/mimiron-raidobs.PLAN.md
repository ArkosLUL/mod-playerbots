# Mimiron: wire the strategy into RaidObs

## Context

Round 3 of Mimiron fixes is implemented and awaiting a rebuild. Every one of the eight defects behind
it was found by watching a 25-man pull and guessing at the cause afterwards — position, intent and
cone geometry all had to be reconstructed from video. That is the loop this change closes.

RaidObs (`src/Bot/Obs/`, guide in `docs/systems/observability.md`) already writes a per-pull NDJSON
trace of every raid member's position, engine verdicts, movement commands, damage, auras and deaths,
read back with `tools/botobs/postmortem.py`. It is raid-agnostic and covers Mimiron today for
everything except the `note` stream — only encounter code knows what an assignment or a derived
decision is. Thorim, Vezax, Algalon, Iron Assembly, Hodir, XT-002 and Flame Leviathan are wired;
**Mimiron is not**, so a trace of a Mimiron wipe carries no phase, no formation slot, no dodge
decision, and no cone.

Outcome: after this change, one Mimiron pull produces a trace that answers each of the eight round-3
defects on its own, without a rerun and without video.

### What the trace already gives, and must not be duplicated

`snap` samples every bot's position, target, movement generator and current cast every 250 ms.
`move` records carry the owning action, the priority, the outcome and — on a `wait` — what beat it
and for how long, which is exactly the `MOVEMENT_FORCED` deadlock between the barrage dodge and the
rocket-strike flee. `act`/`veto` carry per-pass verdicts with a `repeats` count, which is where
oscillation lives. `cast` covers watched creatures, so VX-001's Spinning Up already timestamps every
barrage ignition. Proximity Mines, Rocket Strike markers and Bomb Bots are hostile creatures with no
dynamic object, so the snapshot sweep pulls them into `snap.u` and the death block tests containment
against them.

### The four gaps this closes

| Gap | Why nothing else covers it |
|---|---|
| The Laser Barrage cone | No world object, so nothing can sweep for it. `NPC_MIMIRON_DB_TARGET` is not hostile, so the sweep skips it and the aim point is invisible. `GetMimironBarrageWindow` is the only thing that knows the geometry. |
| The Magnetic Core window | `NoteAura` is gated on `TracksPlayer`, which is roster players only — aura 64436 on the Aerial Command Unit is **not** in the trace at all. |
| Derived positions and decisions | Formation slot, dodge direction, flee-fan outcome and target rule are all derived fresh per call and stored nowhere, so no traced container can see them. |
| A flee that finds no bearing | `MoveAwayClearOfMines` can refuse all twelve bearings, and a refused candidate emits no `move` record. Today a total refusal is a silent gap. |

The cheat-free constraint stands: no `HasCheat`, no `TeleportTo`, no `->Kill(`. This change adds
observation only — no behaviour changes.

---

## Design

Follow the two rules the observability guide is built on: **probe the rule, not the coordinate**, and
**probe inside the helper that derives it**, never at the call sites, so trigger and action cannot
disagree about what was decided.

### O1 — Per-instance state and tick

New in the anonymous namespace beside the other Mimiron helpers in
`src/Ai/Raid/Uld/Util/UldBossHelper.cpp`, modelled on `FlameLeviathanState` (same file, ~line 2745):

```cpp
enum MimironTracedPhase : uint32
{
    MIMIRON_TRACE_NONE = 0, MIMIRON_TRACE_MKII = 1, MIMIRON_TRACE_VX001 = 2,
    MIMIRON_TRACE_ACU = 3,  MIMIRON_TRACE_ALL = 4, MIMIRON_TRACE_HANDOVER = 5,
};

struct MimironState
{
    RaidObs::ObsValue<uint32> phase{"mimiron.phase"};
    RaidObs::ObsValue<bool> acuGrounded{"mimiron.core"};
    RaidObs::ObsValue<ObjectGuid> coreCarrier{"mimiron.carrier"};
    uint32 scanMs = 0;
};

std::mutex mimironStatesMutex;
std::unordered_map<uint32 /*instanceId*/, MimironState> mimironStates;
```

**Not `thread_local`.** `MapUpdate.Threads` is 6 and a map is not pinned to one thread, so per-thread
copies hand the same instance a fresh state whenever the pool reassigns it — the bug that made
`fl.pursued` flap 199 times in a pull with twelve real switches. The mutex covers the lookup only;
references into an `unordered_map` survive rehashing.

`TickMimiron(PlayerbotAI*, Player*, Unit* mkII, Unit* vx001, Unit* acu)` runs from the top of
`IsMimironEngaged`, which already resolves all three constructs and is called for every non-tank bot
every tick through `MimironTargetGuardMultiplier::GetValue`. Restructure that loop to keep the three
pointers and hand them over rather than looking them up twice.

Throttle on a new `ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS = 250` (header, beside the other Mimiron
constants): matches the default snapshot interval, so every snapshot has a co-located hazard row, and
matches the barrage's own 250 ms damage tick.

Order inside the tick:

1. Return unless `bot->GetMapId() == ULDUAR_MAP_ID`; then the throttle.
2. **No construct alive** → reset all three latches and return. Off the constructs, never off the
   calling bot's combat state: one bot dropping combat is not a wipe, and a latch left set would make
   the re-pull emit nothing.
3. `phase` from which constructs are alive, plus `IsMimironPhase4(bot)` for `ALL`; `HANDOVER` when
   `GetMimironStagingFocus` answers but nothing is attackable.
4. `acuGrounded = IsMimironAcuGrounded(botAI)` — one `HasAura` on a cached unit, so ungated;
   `ObsValue` gates its own emit.
5. Under `RaidObs::Active()`: `coreCarrier` (walks the group) and the barrage hazard below.

### O2 — The Laser Barrage cone as a hazard

```cpp
MimironBarrageWindow const window = GetMimironBarrageWindow(bot, vx001);
if (window.valid)
{
    char params[96];
    snprintf(params, sizeof(params), "\"lead\":%.2f,\"sweep\":%.2f,\"rate\":%.2f,\"live\":%.1f",
             window.lead, window.sweep, window.rate, window.untilLive);
    RaidObs::NoteHazard(bot->GetMap(),
                        window.untilLive > 0.0f ? SPELL_SPINNING_UP : SPELL_P3WX2_LASER_BARRAGE_AURA_1,
                        vx001->GetPosition(), "sweep", params, ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS);
}
```

The `NoteHazard` docstring in `RaidObs.h` already names a Barrage sweep as its worked example, and
`postmortem.py` prints whatever fields a shape attaches, so **no schema bump and no reader change**.

Cost is bounded: `GetMimironBarrageWindow` returns invalid on its aura check *before* the 250 yd grid
scan for the DB Target, so the scan only runs while a window is actually live.

Origin is VX-001's own position, resampled every 250 ms. In phase 4 VX-001 rides the chassis, so that
is the apex drifting under the raid — the thing the tank ring margin exists to prevent, and the thing
no static record would show.

Emitting the two spell ids separately makes `--notes` read the 4 s Spinning Up warning and the 10 s
fire as distinct stages without decoding `live`.

### O3 — Derived probes

Five `NoteDerived` keys. Each emits only on change, and each is gated at the call site with
`if (RaidObs::Active())` around the string building — `NoteDerived` gates its own emit, but the
strings must not be built for a thousand idle open-world bots.

| Key | Where | Values | Defect it answers |
|---|---|---|---|
| `mimiron.slot` | `GetMimironSpreadSlot` | `p4tank <pos>`, `stagemelee i/n <pos>`, `p3wedge i/n <pos>`, `p1tank <pos>`, `ring i/n <pos>`, `none` | 3, 5, 6 — wedge drift, staging pacing |
| `mimiron.barrage` | `MimironP3Wx2LaserBarrageAction::Execute` | `clear`, `hold`, `ahead cw <deg> r<yd>`, `inside cw|ccw <deg> r<yd>`, `trailing ccw <deg> r<yd>` | 2, 7 — melee dying in the cone |
| `mimiron.flee` | `MimironFleeAction::MoveAwayClearOfMines` | `<what> ok <±deg> (back<n> mine<n> cone<n>)`, `<what> fallback (…)`, `<what> none (…)` | 2, 7, 8 — a dodge that goes nowhere |
| `mimiron.dpsrule` | `MimironSetDpsPriorityAction::ResolveTarget` / `Execute` | `bombbot`, `acu-grounded`, `assaultbot`, `firebot`, `junkbot`, `mech`, `p4:<entry>`, `held:<entry>`, `p4hold`, `fallback`, `none` | 1, 4 — grounded-ACU switch, snare targeting |
| `mimiron.corestep` | `MimironMagneticCoreAction::Execute` | `no-acu`, `no-corpse`, `walk-corpse`, `loot`, `bags-full`, `walk-acu`, `blocked`, `use` | 1 — why a core never landed |

**`mimiron.slot`** needs the `GetHodirAnchor` / `DeriveHodirAnchor` split already used in this file
(~line 1122): move the current body of `GetMimironSpreadSlot` into a file-local
`DeriveMimironSpreadSlot(botAI, bot, out, branch, index, count)` that sets `branch` at each of its six
returns, and make `GetMimironSpreadSlot` the probing wrapper. `GetMimironStagingMeleeSlot` and
`GetMimironPhase3Slot` gain `uint32& index, uint32& count` out-params; both are file-local with one
caller each. One probe then covers `MimironArcSpreadTrigger` and `MimironArcSpreadAction`, which both
route through here.

**`mimiron.flee`** needs a `char const* what` parameter on `MoveAwayClearOfMines` — `"shock"`,
`"rocket"`, `"mine"` from its three callers. This is the highest-value probe in the set: it is the
only one reporting something no other stream carries at all, because a refused bearing is a candidate
that never reaches the MotionMaster and so writes no `move` record. The three counters say *why* the
fan emptied — turned back toward the hazard, a mine, or the cone.

**`mimiron.dpsrule`** deliberately records the rule and not the guid: `snap.u` already samples every
bot's target four times a second. What it cannot say is whether the target was chosen, held over from
last tick, or withheld by the phase-4 floor.

Not probed, on purpose: the Bomb Bot snare (its firing is already an `act` verdict and a `cast`
record), `IsMimironSpotSafe` (asked about many candidates per pass, and the one that wins is already
in `mimiron.slot`), and `MarkPull` (Mimiron's instance script sets `IN_PROGRESS`, so a session opens
by itself).

### O4 — Docs

**Run `/compact-docs-writer` before editing either doc**, per the compaction rule — up front, not as
cleanup.

- `docs/systems/observability.md` — add Mimiron to the "Converted:" list. That list is stale: XT-002
  and Flame Leviathan carry probes and are not on it either. Fix all three.
- `docs/raids/ulduar.md` — one new section in the Mimiron cluster (after
  "### Mimiron — the ring slot is what kills the Rocket Strike dodge"): the eight keys and the sweep
  channel, and the two things a Mimiron trace still cannot show — creature auras, because `NoteAura`
  is roster-only, and the DB Target's position, because it is not hostile and the sweep skips it.
- Mirror this plan to `docs/plans/mimiron-raidobs/mimiron-raidobs.PLAN.md`.

## Files touched

- `src/Ai/Raid/Uld/Util/UldBossHelper.cpp` — O1 state and tick, O2 hazard, `mimiron.slot` wrapper and
  the two slot helpers' out-params
- `src/Ai/Raid/Uld/Util/UldBossHelper.h` — `ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS`
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp` / `.h` — `#include "RaidObs.h"`, the `what`
  parameter on `MoveAwayClearOfMines`, and the four action-side probes
- `docs/systems/observability.md`, `docs/raids/ulduar.md`, `docs/plans/mimiron-raidobs/`

No trigger, context or strategy wiring: no new node is added.

## Verification

The module cannot be compiled from this checkout; the worldserver build is the only compile path.
Rebuild first — the round-3 header-ordering fix is still unverified and lands in the same files.

Then confirm the trace is off before the pull (`Obs.Enabled` defaults on; read the effective value
with `docker exec ac-worldserver env | grep ^AC_`, never the `.conf`), pull Mimiron once on normal and
once on Firefighter, and check each of these against the trace with `postmortem.py`:

1. **No behaviour changed.** Compare a `--track` of two or three bots against the round-3 pull. This
   change adds observation only; a formation or dodge that moved is a bug in the `DeriveMimironSpreadSlot`
   split, not a finding.
2. **`--notes mimiron.phase`** reads a five-line phase timeline and nothing more — a key that emits
   per tick has a broken change test.
3. **`--notes mimiron.` on a barrage**: the `haz` rows walk `lead` smoothly across the sweep, and in
   phase 4 the origin drifts with the chassis. Cross-check one row against a `--track` of a bot that
   died there: `cw` in its `mimiron.barrage` note should put it inside the band.
4. **`mimiron.flee` shows at least one `none` or `fallback`** across a Shock Blast with mines down.
   If every flee is `ok`, the counters are not being reached and the probe is misplaced.
5. **`mimiron.core` brackets each Magnetic Core**, ~20 s apart, and `mimiron.dpsrule` flips to
   `acu-grounded` for melee inside it. `mimiron.carrier` names one bot, and `mimiron.corestep` walks
   `no-corpse → walk-corpse → loot → walk-acu → use`.
6. **`mimiron.slot`** holds steady per bot through phase 3 rather than re-emitting every tick, and the
   branch tag changes exactly at the handovers.
7. **Hard-mode sweep cap.** Count distinct entries in `snap.hz`/`snap.u` late in a Firefighter pull.
   The sweep caps hostile creatures at `OBS_MAX_WATCHED` (40, `RaidObs.cpp:63`) in grid order, and
   fire nodes spread continuously, so Proximity Mines can be silently dropped from the containment
   test. If mines are missing from a death that stood in one, the follow-up is a
   `AiPlayerbot.Obs.MaxWatched` config knob — deliberately out of scope here.
8. **Cost.** No visible tick-rate change with 25 bots and a trace open, and the file stays well under
   `MaxFileMB`.
9. Cheat-free grep: `HasCheat`, `TeleportTo`, `->Kill(` still return nothing across the Mimiron files.

## Deliberately out of scope

- Raising or configuring `OBS_MAX_WATCHED` — verify first (step 7), then decide.
- Probing the Bomb Bot snare, `IsMimironSpotSafe` or per-attempt action outcomes: the `act`, `cast`
  and `move` streams already latch those, and a note per attempt re-adds what they suppress.
- Any schema bump. Every record this adds fits `v: 7` as it stands.
- W6 Napalm Shell spread, still carried from round 1 and still awaiting a decision.
