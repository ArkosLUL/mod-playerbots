# Ulduar performance follow-ups

## Context

Six per-boss CPU reviews ran between 2026-09-11 and 2026-09-12 (Auriaya, Ignis, Razorscale, XT-002,
Freya, Iron Assembly). Each shipped its own fixes and each left items open, and because they ran as
six independent sessions several of them found **the same defect on a different boss** and filed it
separately. The reviews themselves are retired; their durable findings are now in
[../../engine/raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md) §"What a strategy
costs per raid", [../../engine/action-selection.md](../../engine/action-selection.md) and
[../../systems/observability.md](../../systems/observability.md).

This plan carries only what is **verified still open in source**, de-duplicated. It is the one live
entry in `docs/plans/`.

Shipped already, for orientation: the per-bot `getMSTime()`-stamped scan (`RazorscaleScan`,
`IronAssemblyScan`), the entry-first `CollectPossibleTargetsByEntry` lookup, `specTabCache`, the
`GatePass` thread-local trigger gate, and the Ignis instance-script pre-filter.

## Open items

### 1. Multipliers resolve the boss before they look at the action

The single most repeated finding — raised independently against XT-002, Algalon, Mimiron, Hodir,
Auriaya, Ignis and Razorscale. Verified still unfixed:

- `AuriayaMovementGuardMultiplier::GetValue` calls `IsAuriayaEngaged` first
  (`UldMultipliers_Auriaya.cpp`)
- `AlgalonTargetGuardMultiplier` calls `AlgalonEncounterActive` first

One Auriaya chain pass costs about **5 sight-range sweeps + 3 LOS sweeps ≈ 150 raycasts** before
anything returns a verdict, and every verdict is 1.0. Fix: test the action family first, then resolve.
Mimiron's guards landed this way in `256854ebf` and are the worked example.

**Do not** move `IsMimironEngaged` behind the action test — it starves `TickMimironObs` housekeeping
that rides on the same call.

### 2. 51 Ulduar multipliers have no encounter gate

Only triggers are wrapped in `UldGatedTrigger`; `UldStrategy.cpp` registers multipliers bare, so they
run for the whole instance — on trash, between pulls, and after the boss is dead. Between pulls
nothing is `IN_PROGRESS`, so all **165** triggers are open too and each runs its own boss lookup for
every bot.

**Do not** simply gate multipliers on `UldEncounterGateOpen`: a boss in sight during another
encounter then stops being guarded, which is a behaviour change, not a saving.

### 3. `BossFireResistanceTrigger` looks up the boss before the cheap tests

`src/Ai/Base/Trigger/BossAuraTriggers.cpp:41` runs `FindBossByName` — a sight-range sweep plus a
UTF-16 lowercase of every unit name — **before** `HasAura`, the spell-known check and the raid-group
check. Shared by 7 paladin and 3 hunter triggers across every raid, not just Ulduar.
`BossFrostResistanceTrigger` is the identical shape. Reorder; no behaviour changes.

### 4. `UldThreatRedirectMultiplier` sweeps six or seven times per candidate

Six to seven separate `GetFirstAliveUnitByEntry` calls per Misdirection/Tricks candidate, anywhere in
Ulduar. One scan serves all of them.

### 5. `UlduarBurstWindowMultiplier::EvaluateWindow` falls back to Razorscale everywhere

It ends on `"find target" razorscale` in **every** non-Razorscale Ulduar fight. The
`UldEncounterIsLive(ULD_BOSS_RAZORSCALE)` gate was proposed and never added.

**Do not** split `EvaluateWindow` itself. Its 105-line per-boss switch exists so the
`IsBurstCooldownAction` early-out runs once per action rather than once per boss, and it resolves nine
boss entries out of **one** `"possible targets no los"` sweep. Splitting it means nine sweeps or a
redesign.

### 6. Two unlocked process-wide statics — a correctness bug, not a cost

`RazorscaleBossHelper::_harpoonCooldowns` and `_lastRoleSwapTime`
(`UldEncounter_Razorscale.cpp:36,38`) are keyed on `ObjectGuid` with **no mutex and no instance key**.
With `MapUpdate.Threads` at 6, two concurrent Razorscale raids both race on them *and* share state.
The Ignis sibling got a mutex in the same wave; these did not. The rule and its signature are in
[../../engine/raid-mechanics-lessons.md](../../engine/raid-mechanics-lessons.md).

This one should go first — it is the only item here that changes what bots do.

### 7. `Engine::LogAction` formats every engine event even with logging off

`Engine.cpp:672-704` builds `lastAction` for every bot with a real player master
(`LogInGroupOnly` defaults to 1), regardless of whether debug logging is on.

### 8. XT-002 verification still outstanding

The code and the equivalence harness landed; the rebuild, the PerfMon A/B and a confirming pull did
not.

## Method, so it is not re-derived a seventh time

**Cost symbols** used across all six reviews: **S** = one `"possible targets no los"` read (a
`Cell::VisitObjects` at `SightDistance` 100 plus `IsPossibleTarget` per hostile, no LOS ray);
**N** = one `"nearest npcs"` read (the same visit **plus a VMAP and dynamic-tree LOS raycast per
non-player unit** — 49-67 units per Auriaya snapshot, so ~50 raycasts a read); **G100** / **G7** =
grid sweeps at those radii.

**Proving a refactor is behaviour-neutral.** The equivalence harness is the only validated technique
here: pull the old function bodies verbatim out of `git show HEAD:<file>`, take the new ones from the
working tree, compile both together with counting stubs, and enumerate the input space —
**18.5 M combinations, 0 mismatches** on XT-002. Include a red-capability check so a pass means
something: dropping `purgingImmunity` produced 1,392 mismatches and dropping the main-tank exemption
11,520.

**Measurement.** `.playerbots pmon`, protocol and traps in
[../../systems/observability.md](../../systems/observability.md). Read ratios, never absolute times.

### Settled negatives — do not re-audit

- No `checkInterval` or framework memo on `"possible targets no los"` / `"nearest npcs"`: both are
  global values, so a memo goes stale across ticks for every other reader.
- Do not run `isPossible` before the multipliers — Mimiron's and Algalon's multipliers have side
  effects, and vetoes are logged.
- `GetCreatureListWithEntryInGrid` is **not** a substitute for a LOS sweep. It is a grid container
  only, and `AllCreaturesOfEntryInRange` measures **2D** (`GridNotifiers.h:1510`) where
  `AnyUnitInObjectRangeCheck` measures **3D** (`:1067`).
- Leave the shared `GetFirstAliveUnitByEntry` alone. It is marked `DO NOT USE, WILL BE REMOVED`
  upstream and every raid calls it; the replacement plan is a Custom-owned copy under a different
  name, not an edit in place.
- Replicating `Cell::CalculateCellArea` + `VisitCircle` to skip the octagon walk is provably identical
  but copies ~40 lines of core internals that go silently wrong the day AC changes cell visiting.
  Rejected.
