# RaidObs: recover lost aura apply times, stop filing wipes as idle

## Context

RaidObs is a per-instance NDJSON session recorder for playerbot raid pulls. Its premise: hand a
fresh agent session a trace file path and it can diagnose why a specific bot died, without video.
Recorder in `src/Bot/Obs/`, analyzer in `tools/botobs/postmortem.py`, schema in
`docs/systems/observability.md`. Schema is at v7.

Five traces were recorded on 2026-08-30, the first v7 data and the first coverage of Ulduar's
opening four bosses:

| trace | boss | span | out | deaths | KB/s |
|---|---|---|---|---|---|
| `603_1_flame-leviathan_1788099810` | flame-leviathan | 409.2 s | idle | 31 | 18.4 |
| `603_1_flame-leviathan_1788100330` | flame-leviathan | 333.1 s | kill | 13 | 17.6 |
| `603_1_razorscale_1788100789` | razorscale | 552.9 s | kill | 4 | 34.2 |
| `603_1_ignis-the-furnace-master_1788101696` | ignis | 217.6 s | kill | 0 | 33.0 |
| `603_1_xt-002-deconstructor_1788102538` | xt-002 | 340.1 s | wipe | 31 | 28.2 |

**The framework collects correctly.** Audited across all five: 0 unnamed guids in any field, 0
unresolved spell ids (including `snap.hz`, `death.auras`, `death.rewind`), `move.pr` present on
100% of 29433 move rows, `hpr`+`hms` present on 100% of 15606 `wait` rows, 79/79 deaths carrying
`blow` + `hplast` + a named killer, no `act`/`veto`/`move`/`note` record naming a foreign Ulduar
boss, and every analyzer view exiting 0. Cost 17.6–34.2 KB/s against a 48 KB/s budget.

**v7 is confirmed live.** 1991 `death.auras` rows, 700 carrying the new `-1` sentinel, **zero**
`held` values exceeding their fight span. 15 debuff rows render `held      ?` with column alignment
intact. A probe over every sentinel row confirmed the aura stream genuinely carries no apply record
for that `(bot, spell)` pair, so the sentinel masks no lost data at the analyzer level.

Two defects remain, both in the recorder.

---

## Defect 1 — the apply time is lost whenever an aura kills inside one unit-update tick

Root cause is in the core, not the module.

`AuraApplication::_Apply` ([SpellAuras.cpp:83](../../src/server/game/Spells/Auras/SpellAuras.cpp#L83))
only calls `SetNeedClientUpdate()`. The flush happens later, in `Unit::_UpdateSpells`
([Unit.cpp:4053](../../src/server/game/Entities/Unit/Unit.cpp#L4053)). `AuraApplication::_Remove`
([SpellAuras.cpp:120](../../src/server/game/Spells/Auras/SpellAuras.cpp#L120)) calls
`ClientUpdate(true)` **synchronously**.

`RaidObsGlobalScript` listens on `GLOBALHOOK_ON_AURA_APPLICATION_CLIENT_UPDATE`
([RaidObsScripts.cpp:161](src/Bot/Obs/RaidObsScripts.cpp#L161)), so it sees the removal but never
the apply when the aura's whole life is shorter than a tick. `NoteAura`
([RaidObs.cpp:1557](src/Bot/Obs/RaidObs.cpp#L1557)) then creates the `AuraState` from the removal
with `appliedMs` still 0, and `NoteDeath` writes the v7 `-1`.

Measured on `603_1_flame-leviathan_1788099810`:

| spell | apply records | removal records |
|---|---|---|
| Hodir's Fury 62297 | 14 | 21 |
| Battering Ram 62376 | 4 | 12 |

Every Flame Leviathan death to Hodir's Fury reads `held ?` for the debuff that killed it — a
145500-damage one-shot, applied and fatal in the same tick:

```
[0] Deathsong (human) died at 1:23.960
     killed by Hodir's Fury
     debuffs (1):
       Hodir's Fury 62297 x1   60.0s left  held      ?  from Hodir's Fury
```

A second, narrower blind spot shares the cause: when a unit's 56 visible-aura slots are full,
`_Apply` never calls `SetNeedClientUpdate` and `_Remove` returns early on the slotless application,
so such an aura fires **neither** hook and never enters the tracked set at all.

### Fix

Add `UNITHOOK_ON_AURA_APPLY` to the `UnitScript` hook list at
[RaidObsScripts.cpp:60-63](src/Bot/Obs/RaidObsScripts.cpp#L60) and route it to a new **state-only**
entry point. The core fires it from `Unit::_ApplyAura`
([Unit.cpp:4852](../../src/server/game/Entities/Unit/Unit.cpp#L4852)) and again on a stack refresh
via `ModStackAmount` ([Unit.cpp:4711](../../src/server/game/Entities/Unit/Unit.cpp#L4711)) — the
double-fire that made the module prefer the client-update hook in the first place. That does not
matter here because the new path emits no record and its stamp is idempotent.

New function in `RaidObs.cpp`, declared in `RaidObs.h` beside `NoteAura`:

```cpp
// The client-update hook only sees an apply once Unit::_UpdateSpells flushes the pending flag, but a
// removal goes out synchronously. Anything that lands and kills inside one tick - Hodir's Fury, a
// Battering Ram ram - therefore arrives as a removal with no apply behind it, and the death record
// has to fall back to the -1 sentinel. This runs inside _ApplyAura, so it gets the real stamp.
void NoteAuraApplied(Unit* target, Aura* aura)
{
    if (!Active() || !target || !aura)
        return;

    ObsSession* session = SessionFor(target);
    if (!session || !TracksPlayer(*session, target))
        return;

    AuraState& state = session->bots[GuidKey(target->GetGUID())].auras[aura->GetId()];
    uint32 const now = getMSTime();
    if (!state.appliedMs || state.removedMs)
        state.appliedMs = now;

    state.removedMs = 0;
    state.caster = GuidKey(aura->GetCasterGUID());
    state.stacks = aura->GetStackAmount();
    state.duration = aura->GetDuration();

    SpellInfo const* info = aura->GetSpellInfo();
    state.positive = info && info->IsPositive();
}
```

The guard must stay byte-identical to the one in `NoteAura`
([RaidObs.cpp:1605-1608](src/Bot/Obs/RaidObs.cpp#L1605)) so the two feeds cannot disagree about
what a stack refresh means: `appliedMs` is the first application in the current uninterrupted run,
not the last refresh. With both hooks live, `NoteAura`'s apply branch becomes a no-op — it finds
`appliedMs` already set and `removedMs` already clear — which is the intended outcome, not a
redundancy to remove.

Filling `caster` / `stacks` / `duration` / `positive` here as well as in `NoteAura` is what closes
the slot-exhaustion case: an aura that fires neither client update still lands in the tracked set
with a complete row.

**No schema bump.** No field, record, or value domain changes. `-1` remains valid for anything
applied before the session opened, which is most raid buffs and the bulk of the 700 sentinels.

**Cost.** The hook fires for every aura application server-wide, gated by the `Active()` atomic and
then a map check before the registry lock. `NoteAura` already carries the same global exposure on a
comparable event rate, so this roughly doubles a load that is currently unmeasurable in the traces.

---

## Defect 2 — a failed attempt is filed as `idle`

`603_1_flame-leviathan_1788099810` closed `out:"idle"` after 31 deaths with the boss still alive.

`CloseSession` upgrades `idle`/`reset` to `wipe` only when `RosterMostlyDead` holds
([RaidObs.cpp:1142](src/Bot/Obs/RaidObs.cpp#L1142)), and that predicate is evaluated **at close
time** — `idleCloseMs` is 30 s ([RaidObs.cpp:91](src/Bot/Obs/RaidObs.cpp#L91)), by which point the
raid has released and run back, so `dead / present` is near zero and the upgrade never fires. The
comment above that line names the case it was written for (Hodir reporting `NOT_STARTED` on
release) but the sample is taken too late to catch it.

Anyone scanning outcomes to pick a trace worth reading skips this one.

### Fix

Latch the verdict while it is still observable instead of reconstructing it 30 s late.

Add to `ObsSession` beside `lastCombatMs` ([RaidObs.cpp:342](src/Bot/Obs/RaidObs.cpp#L342)):

```cpp
bool sawMostlyDead = false;
```

In the map-update block at [RaidObs.cpp:1429](src/Bot/Obs/RaidObs.cpp#L1429), sample it while
combat is live:

```cpp
if (AnyRaidMemberInCombat(s))
{
    s.lastCombatMs = now;
    // Sampled during the fight, not at the close: idleCloseMs is 30s and the raid has released and
    // run back by then, so the close-time check below never sees a wipe it should have caught.
    s.sawMostlyDead = s.sawMostlyDead || RosterMostlyDead(s);
}
else if (g_cfg.idleCloseMs && getMSTimeDiff(s.lastCombatMs, now) > g_cfg.idleCloseMs)
    CloseSession(instanceId, "idle");
```

And widen the upgrade at [RaidObs.cpp:1142](src/Bot/Obs/RaidObs.cpp#L1142):

```cpp
if ((!strcmp(outcome, "reset") || !strcmp(outcome, "idle")) && (session->sawMostlyDead || RosterMostlyDead(*session)))
    result = "wipe";
```

Sticky-OR is safe: the upgrade only applies to `reset` and `idle`, so a raid that dips past the 50%
threshold and then wins still closes as `kill`, which `CloseSession` never rewrites. The extra
roster scan runs only while the raid is in combat, alongside the `AnyRaidMemberInCombat` scan
already there.

`RosterMostlyDead` is defined at [RaidObs.cpp:1103](src/Bot/Obs/RaidObs.cpp#L1103), above
`CloseSession`, and `AnyRaidMemberInCombat` at
[RaidObs.cpp:1197](src/Bot/Obs/RaidObs.cpp#L1197), below it — check the declaration order compiles
before assuming a forward declaration is unnecessary.

---

## Files

- `src/Bot/Obs/RaidObs.h` — declare `NoteAuraApplied`.
- `src/Bot/Obs/RaidObs.cpp` — `NoteAuraApplied`, `ObsSession::sawMostlyDead`, the two edits above.
- `src/Bot/Obs/RaidObsScripts.cpp` — add `UNITHOOK_ON_AURA_APPLY` to the hook list and override
  `OnAuraApply`.
- `docs/systems/observability.md` — the `-1` sentinel now means "applied before the session opened"
  rather than "the recorder missed it", and the `end.out` note that `idle` no longer hides a wipe.
  Invoke `/compact-docs-writer` **before** editing this file, as the first step of the doc task.

Save this plan to `docs/plans/raidobs-apply-time-and-wipe-outcome/raidobs-apply-time-and-wipe-outcome.PLAN.md`
once plan mode exits.

---

## Verification

Static checks are mine; the build and the pull are the user's — there is no headless build path here.

1. Build the module.
2. Re-run the analyzer over the five 2026-08-30 traces and the two v6 Thorim traces. All five views
   must still exit 0 and no `held` may exceed its fight span.
3. Pull Flame Leviathan, which is the cheapest reproduction of both defects.

| Check | 2026-08-30 measured | Expected after |
|---|---|---|
| `aura` apply records for 62297 | 14 (vs 21 removals) | **21** |
| `aura` apply records for 62376 | 4 (vs 12 removals) | **12** |
| `death.auras` rows with `applied == -1` | 700 of 1991 | fewer; the remainder are pre-session buffs |
| debuff rows rendering `held      ?` | 15 | **0** for anything applied after the session opened |
| a Hodir's Fury death block | `held      ?` | a real sub-second hold |
| unnamed guids / unresolved spell ids | 0 | 0 |
| `move.pr` / `wait.hpr` coverage | 100% | 100% |
| KB/s | 17.6–34.2 | ≤ 40 |
| a 31-death attempt with the boss alive | `out:"idle"` | `out:"wipe"` |

4. Confirm a clean kill still closes `kill` and an empty instance still closes `idle`.

---

## Out of scope

- **`note` coverage for Razorscale, Ignis and XT-002 — the largest remaining blind spot.** All
  three emit **zero** `note` records because their strategies declare no `ObsValue` /
  `ObsGuidMap` / `ObsGuidSet`; within Ulduar only `thorim.*`, `fl.*`, `algalon.*`, `vezax.*` and
  `ironassembly.*` exist. XT-002 was the day's only wipe and its shared decisions —
  `xt002 set dps priority action` (1277 `act` rows), `xt002 debuff carrier action`,
  `xt002 raid position action` — are invisible: the trace shows the action ran, never what it
  chose. Deferred because it edits `src/Ai/Raid/Uld/` files another session is working in.
- **`haz` remains unexercised.** Zero `haz` records in all five traces; Thorim is still the only
  caller of `NoteHazard`, so the v7 `EnsureSpell` fix has still never been observed working. Needs
  a Thorim pull, not code.
- **`fl.pursued` writes a bare `0` for "nobody"** (210 of 448 rows), which the analyzer's
  guid→name join renders literally. Cosmetic.
- **Trigger and strategy telemetry.** `RaidUlduarStrategy::InitTriggers`
  ([UldStrategy.cpp:11](src/Ai/Raid/Uld/UldStrategy.cpp#L11)) still registers all 90 Ulduar triggers
  for every boss and nothing in a trace would show it. The five clean cross-boss checks above are
  the same weak evidence that let Thorim's strategy run through three Hodir fights unnoticed:
  `act` records only exist once an action is selected.
- **The Flame Leviathan strategy itself.** Bots died on foot to a 145500 one-shot at 68% health,
  which is a vehicle-uptime failure the recorder surfaced correctly and another session owns.
