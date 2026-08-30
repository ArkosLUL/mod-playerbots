# Iron Assembly — RaidObs probes

## Context

The Iron Assembly strategy was reworked into a fifteen-node encounter whose behaviour is almost
entirely **derived**: which member the raid focuses, which boss each tank owns, who owes the next
kick, whether the raid stacks or spreads, whether anyone soaks Rune of Power. None of that is
stored, so none of it reaches a trace.

Today the encounter's only probe is one line — `spreadSlots` is a
`RaidObs::ObsGuidMap<uint8>{"ironassembly.slot"}`
([UldEncounter_IronAssembly.cpp:38](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L38)) — which
records a hard-mode-only spread slot and nothing else. `docs/systems/observability.md` lists Iron
Assembly among the converted encounters on the strength of that single container.

What a pull file therefore cannot answer: *why* 25 bots were hitting Molgeim, why nobody tanked
Brundir, why Lightning Whirl went uninterrupted, why the ranged group ignored a Rune of Power. The
`act` stream reports that a node ran and returned OK; it never reports what the node picked, and a
node whose trigger came back false leaves no row at all — which is exactly the failure worth
diagnosing.

Outcome: a pull is readable end to end from `postmortem.py <file> --notes ironassembly.` — a fight
log of phase boundaries, per-bot assignments, and the three hazards nothing can sweep for.

Grounding: `docs/systems/observability.md` ("Adding a probe"), and the Hodir / Thorim / Algalon
probes as the in-repo precedent.

## Approach

Five per-bot derived decisions, one per-instance phase latch, three hazard circles. Nothing else —
see *Deliberately not probed* below, which is half the design.

### 1. Derived decisions — `NoteDerived`, one key each

Every one of these is derived fresh per call and stored nowhere, so no traced container can see it.
Probe **inside the helper**, never at the call sites: trigger, action and multiplier all route
through the same helper, and two probes at two sites can disagree about what was decided.

Follow the Hodir shape at
[UldBossHelper.cpp:1059-1070](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L1059-L1070) — a file-local
`Derive*` that returns the answer plus a `char const*& how`, and the existing public function
becomes a thin wrapper that probes `how` and returns. It keeps the probe off six separate `return`
statements.

| Key | Values | Answers |
|---|---|---|
| `ironassembly.focus` | `<guid> skull`, `<guid> order`, `none` | Which member, and whether a human's skull beat the configured kill order |
| `ironassembly.tank` | `<guid> brundir\|steelbreaker\|molgeim\|swap\|focus`, `none:onetank`, `none:surplus`, `none:nobosses` | Who owns which boss, and which collapse branch produced a `nullptr` |
| `ironassembly.interrupt` | `whirl`, `chain`, `standby:<rank>`, `none:moving`, `none:noready`, `none:nocast` | Who owed the kick, and why nobody did |
| `ironassembly.spot` | `stack`, `stack-late`, `spread`, `overflow`, `none` | Which formation branch, not the coordinate — the `move` record already carries that |
| `ironassembly.soak` | `<guid>`, `none:nocarrier`, `none:far`, `none:rune` | Why ranged did or did not walk into Rune of Power |

Wrapped functions, all in
[UldEncounter_IronAssembly.cpp](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp):
`IronAssemblyFocusTarget`, `IronAssemblyAssignedBoss`, `TryGetIronAssemblyRaidSpot`,
`TryGetIronAssemblyRuneOfPowerSoakSpot`, plus the interrupt election below.

**Probe only bots the key applies to.** `ironassembly.tank` emits only under `botAI->IsTank(bot)`,
and `spot` / `soak` only under `IronAssemblyTakesRaidSpot` — otherwise 25 bots each write one
meaningless `none:` row. `focus` is for everyone: tanks reach it too, through the two-tank fallback
at [UldEncounter_IronAssembly.cpp:279-284](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L279-L284).

`ironassembly.spot` deliberately does **not** carry the slot number — `ironassembly.slot` already
has it.

### 2. Interrupt duty — a small refactor, not just a probe

`IronAssemblyInterruptRank` returns a bare rank, and the trigger
([UldTriggers_IronAssembly.cpp:74-93](src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.cpp#L74-L93))
turns it into a duty by testing which cast is up. A probe in the helper can therefore only say
`rank1`, when the useful word is `chain`.

Move the duty decision into the helper:

```cpp
// The interrupt duty this bot holds for Brundir's current cast, or nullptr. Rank 0 owns Lightning
// Whirl, rank 1 owns Chain Lightning, and nobody else acts - so with one interrupt off cooldown
// Chain Lightning is deliberately allowed through.
char const* IronAssemblyInterruptDuty(PlayerbotAI* botAI, Player* bot, Unit* brundir);
```

The trigger collapses to `IronAssemblyInterruptDuty(...) != nullptr`. `IronAssemblyInterruptRank`
drops out of the header into the anonymous namespace — the trigger was its only caller. This is also
what the observability doc means by one place deciding: the action already re-derives
`IronAssemblyReadyInterrupt` on its own.

### 3. Phase latch — `ObsValue<uint8>` on the encounter state

Every member death restores the survivors to full and changes what the raid does, and it is the only
phase boundary the fight has. It is visible in `snap.u` only as a boss row that stops appearing while
the others jump back to 100% — true, but no reader surfaces it, and it is the line every other note
has to be read against.

```cpp
// Bit 0 Steelbreaker, bit 1 Molgeim, bit 2 Brundir - a scoped value written as its raw number, so
// the layout lives here.
RaidObs::ObsValue<uint8> membersAlive{"ironassembly.alive"};
```

into the anonymous-namespace `IronAssemblyEncounterState` at
[UldEncounter_IronAssembly.cpp:36](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L36). Emits on
change only, so one row per member death.

**Do not reset it on a re-pull.** The obvious idea — zero it in `ResetIronAssemblyEncounterState` so
each attempt opens with a baseline row — churns: that function runs per bot, so the first bot zeroes
the shared latch, the next tick writes 7 back, the second bot zeroes it again. The `pull` record
already delimits attempts and `snap.u` carries the boss rows from `t=0`. Consequence to accept: the
first pull after a restart writes a baseline row and later ones do not.

### 4. Hazard circles — `NoteHazard`, throttled per instance

Rune of Death and Rune of Power are DynamicObjects and are already swept into `snap.hz`; they must
**not** be noted. The three that nothing can sweep for:

| Spell | Origin | Params |
|---|---|---|
| Overload damage `61878` | Brundir | `"rad":20,"clear":25` |
| Lightning Tendrils damage `61886` (10m) / `63485` (25m) | Brundir | `"rad":18,"clear":28` |
| Meltdown `61889` | each Overwhelming Power carrier | `"rad":15,"clear":20` |

Emit the **damage** id rather than the aura id, so the row joins straight onto the `dmg` and
`death.rewind` entries it explains. Pick the difficulty's id from `map->Is25ManRaid()` — a timeline
row has to name one spell, unlike the OR-both-ids idiom the behaviour code uses.

Carry both radii: `rad` is the spell's, `clear` the line the strategy actually draws. `postmortem.py`
prints any shape field it does not recognise
([postmortem.py:559-569](tools/botobs/postmortem.py#L559-L569)), so neither needs a reader change.
Use `NoteHazard` with shape `"circle"` directly — `NoteHazardCircle` only writes `rad`.

`NoteHazard` has no change-latch and emits on every call, so this needs a throttle or 25 bots write
25 rows a tick. Copy Thorim's shape
([UldEncounter_Thorim.cpp:438-457](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L438-L457)): a bare
`uint32 hazardNoteMs` on the encounter state — bare, because a scan timestamp is bookkeeping, not an
assignment — against a new `ULDUAR_IRON_ASSEMBLY_HAZARD_NOTE_INTERVAL_MS = 1000`, passed as the row's
own `ttl` so the timeline reads as continuous cover. At 1 s the rows track Brundir as he drifts
through a 16 s Tendrils.

Gather it in one file-local `TickIronAssemblyObs(botAI, bot, targets)`, `RaidObs::Active()` at the
top, called from `IronAssemblyFormationActive`
([UldEncounter_IronAssembly.cpp:177-201](src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp#L177-L201))
once the combat test passes — the Algalon precedent, where `AlgalonEncounterActive` drives
`AlgalonTickEncounterState`. That predicate already runs every tick for every bot in the room and
returns early for everyone outside it, which is exactly the population wanted.

## Files

| Path | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.cpp` | Bulk of the work: `Derive*` splits, the five probes, the phase latch, `TickIronAssemblyObs` |
| `src/Ai/Raid/Uld/Util/UldEncounter_IronAssembly.h` | `IronAssemblyInterruptDuty` replaces `IronAssemblyInterruptRank`; a note-key inventory comment naming the five keys and the `alive` bit layout |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_IronAssembly.cpp` | `IronAssemblyInterruptTrigger::IsActive` collapses onto the duty helper |
| `src/Ai/Raid/Uld/Util/UldBossHelper.h` | Four damage spell ids + `ULDUAR_IRON_ASSEMBLY_HAZARD_NOTE_INTERVAL_MS`. **Additive only** — another effort has this file open in the working tree |
| `docs/raids/ulduar.md` | Iron Assembly section: a short "reading a pull" paragraph, the five keys, the bit layout, the three haz circles |

Radius constants already exist (`ULDUAR_IRON_ASSEMBLY_OVERLOAD_RADIUS` 20 /
`_CLEARANCE` 25, `TENDRILS` 18 / 28, `MELTDOWN` 15 / 20, `UldBossHelper.h:389-404`). The four spell
ids are new and must be re-checked against `modules/mod-spell-tweaks/data/dbc-reference/` before
shipping — 61886 and 63485 were removed from this header once already, as mis-labelled Overload
auras, and they are the Tendrils damage triggers.

`docs/raids/ulduar.md` is reachable from the module `CLAUDE.md`, so **invoke `/compact-docs-writer`
before writing that section**, not as cleanup afterwards.

## Deliberately not probed

- **Rune of Death, Rune of Power** — DynamicObjects, already in `snap.hz`, and only `snap.hz` feeds
  the death block's `STOOD IN` containment test. A `haz` row would take them *out* of the test.
- **Multiplier vetoes.** `IronAssemblyDisableAutomaticTargetingMultiplier` and
  `IronAssemblyMovementGuardMultiplier` already reach the `veto` stream through
  [Engine.cpp:228](src/Bot/Engine/Engine.cpp#L228). Nothing to add.
- **`IronAssemblyMemberMustMove`** — asked about every raid member by every bot in one pass, the same
  reason `IsHodirTrappedAllyBreaker` is unprobed. Its effect is already the veto row above, and the
  one bot it decides against appears in `ironassembly.interrupt` as `none:moving`.
- **Action outcomes** (taunt landed, dispel cast, interrupt cast) — an `OK` or `FAILED` in the `act`
  stream. A note per attempt re-adds what that stream latches.
- **`MarkPull`.** Verified unnecessary: `boss_assembly_of_iron.cpp:226` sets `IN_PROGRESS` on
  engage, and `DONE` only behind `IsEncounterComplete` (`:286`), so one session covers all three
  members rather than closing on the first death.
- **`SCHEMA_VERSION` / `SUPPORTED_SCHEMA`.** No bump. New note keys and extra `haz` shape fields are
  additive inside existing columns, and `postmortem.py` renders both generically. The bump rule is
  for a field or an encoding a reader would compute wrongly.

## Verification

Static, here:

- `python apps/codestyle/codestyle-cpp.py`
- Every probe wrapped in `if (RaidObs::Active())` — `DescribeAssignment` builds a `std::string`, so
  an ungated call pays for it on every bot every tick.
- Every new note key appears at exactly one call site.
- The four spell ids checked against the DBC reference CSVs.
- The module cannot be compiled in this environment; hand the build off rather than claiming one.

In-game, one 25-man pull with a trace open:

1. `postmortem.py <file> --notes ironassembly.` reads as a fight log — `alive` stepping 7 → 3 → 1,
   `focus` advancing with it, `tank` rows for two or three bots, `interrupt` alternating `whirl` and
   `chain` through Brundir's phase 2.
2. `grep -c '"k":"ironassembly' <file>` in the low hundreds for a full pull. Thousands means a latch
   is churning — a bug, not a tuning knob. Check `soak` first: it flips as the tank drags the boss
   off the rune.
3. `haz` rows appear at ~1 s cadence while Overload and Tendrils are up, and around each
   Overwhelming Power carrier; none appear for Rune of Death, which shows up in `snap.hz` instead.
4. `--death N` on a bot killed by Overload shows the circle on the timeline beside the killing blow.
5. Hard-mode flag set: `spot` flips to `spread` once Steelbreaker is alone, and `tank` shows `swap`
   for both partners.
6. Wipe and re-pull: no stale assignment rows before the second `pull` record.

## Step 0

Copy this document to `docs/plans/iron-assembly-obs-probes/iron-assembly-obs-probes.PLAN.md` before
starting, and fold the durable parts into `docs/raids/ulduar.md` when it ships.
